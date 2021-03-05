#include "naru_encode_processor.h"
#include "naru_internal.h"
#include "naru_utility.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* 固定小数点数の乗算 */
#define NARU_FIXEDPOINT_MUL(a, b, shift) NARUUTILITY_SHIFT_RIGHT_ARITHMETIC((a) * (b) + (1 << ((shift) - 1)), (shift))
/* ステップサイズに乗じる係数のビット幅 */
#define NARUNGSA_STEPSIZE_SCALE_BITWIDTH 10
/* ステップサイズに乗じる係数の右シフト量 */
#define NARUNGSA_STEPSIZE_SCALE_SHIFT 6
/* SAの右シフト量 */
#define NARUSA_STEPSIZE_SHIFT 2

/* ブロックヘッダに記録するデータのビット幅 */
#define NARU_BLOCKHEADER_DATA_BITWIDTH 8
/* ブロックヘッダに記録するデータのシフト数のビット幅 */
#define NARU_BLOCKHEADER_SHIFT_BITWIDTH 4

/* フィルタ次数/AR次数に関するチェック */
NARU_STATIC_ASSERT(NARUUTILITY_IS_POWERED_OF_2(NARU_MAX_FILTER_ORDER));
NARU_STATIC_ASSERT(NARU_MAX_FILTER_ORDER > (2 * NARU_MAX_AR_ORDER));

static int32_t NGSAFilter_Predict(struct NGSAFilter *filter, int32_t input);
static void NGSAFilter_PutFilterState(struct NGSAFilter *filter, struct NARUBitStream *stream);
static int32_t SAFilter_Predict(struct SAFilter *filter, int32_t input);
static void SAFilter_PutFilterState(struct SAFilter *filter, struct NARUBitStream *stream);

static uint32_t NARUEncodeProcessor_CalculateBitShift(const int32_t *data, int32_t num_data, int32_t maxbit);
static void NARUEncodeProcessor_RoundAndPutSint(struct NARUBitStream *stream, uint32_t bitwidth, int32_t *pval, uint32_t rshift);

/* dataをmaxbit内に収めるために必要な右シフト数計算 */
static uint32_t NARUEncodeProcessor_CalculateBitShift(const int32_t *data, int32_t num_data, int32_t maxbit)
{
  int32_t bitwidth;

  NARU_ASSERT(data != NULL);
  NARU_ASSERT(num_data > 0);
  NARU_ASSERT(maxbit > 0);

  bitwidth = (int32_t)NARUUtility_GetDataBitWidth(data, (uint32_t)num_data);

  return (bitwidth > maxbit) ? (uint32_t)(bitwidth - maxbit) : 0;
}

/* 符号付き整数pvalをrshiftした値を出力 その後シフトしたビット数だけpvalの下位bitをクリア */
static void NARUEncodeProcessor_RoundAndPutSint(
    struct NARUBitStream *stream, uint32_t bitwidth, int32_t *pval, uint32_t rshift)
{
  uint32_t putval;

  NARU_ASSERT(stream != NULL);
  NARU_ASSERT(pval != NULL);

  /* 右シフトしつつ符号なし整数に変換 */
  putval = NARUUTILITY_SINT32_TO_UINT32((*pval) >> rshift);

  /* 出力 */
  NARU_ASSERT(putval < (1 << bitwidth));
  NARUBitWriter_PutBits(stream, putval, bitwidth);

  /* シフトしたビットをクリア */
  (*pval) &= ~((1 << rshift) - 1);
}

void NARUEncodeProcessor_Reset(struct NARUEncodeProcessor *processor)
{
  NARU_ASSERT(processor != NULL);

  /* 各メンバを0クリア */
  memset(&processor->ngsa, 0, sizeof(struct NGSAFilter));
  memset(&processor->sa, 0, sizeof(struct SAFilter));
}

void NARUEncodeProcessor_CalculateARCoef(
    struct NARUEncodeProcessor *processor, struct LPCCalculator *lpcc,
    const double *input, uint32_t num_samples, int32_t ar_order)
{
  int32_t ord;
  double coef_double[NARU_MAX_FILTER_ORDER + 1];
  LPCCalculatorApiResult ret;

  NARU_ASSERT(processor != NULL);
  NARU_ASSERT(lpcc != NULL);
  NARU_ASSERT(input != NULL);
  NARU_ASSERT(num_samples > 0);
  NARU_ASSERT(ar_order <= NARU_MAX_AR_ORDER);

  /* AR係数の計算 */
  ret = LPCCalculator_CalculateLPCCoef(lpcc, input, num_samples, coef_double, (uint32_t)ar_order);
  NARU_ASSERT(ret == LPCCALCULATOR_APIRESULT_OK);
  /* 整数量子化: ar_coef_doubleのインデックスは1.0確定なのでスルー */
  for (ord = 0; ord < ar_order; ord++) {
    double tmp_coef = NARUUtility_Round(coef_double[ord + 1] * pow(2.0f, NARU_FIXEDPOINT_DIGITS));
    processor->ngsa.ar_coef[ord] = (int32_t)tmp_coef;
  }

  /* ステップサイズに乗じる係数の計算 */
  if (ar_order == 1) {
    int32_t scale_int;
    /* 1.0f / (1.0f - ar_coef[0] ** 2) */
    scale_int = (1 << 15) - ((processor->ngsa.ar_coef[0] * processor->ngsa.ar_coef[0] + NARU_FIXEDPOINT_0_5) >> NARU_FIXEDPOINT_DIGITS);
    scale_int = (1 << 30) / scale_int;
    processor->ngsa.stepsize_scale = scale_int >> (NARU_FIXEDPOINT_DIGITS - NARUNGSA_STEPSIZE_SCALE_BITWIDTH);
  } else {
    processor->ngsa.stepsize_scale = 1 << NARUNGSA_STEPSIZE_SCALE_BITWIDTH;
  }
}

static void NGSAFilter_PutFilterState(
    struct NGSAFilter *filter, struct NARUBitStream *stream)
{
  int32_t ord;
  uint32_t shift, putval;

  NARU_ASSERT(filter != NULL);
  NARU_ASSERT(stream != NULL);

  /* AR係数: 16bitで記録（TODO: 次数が1のときは負になることはないので1bit削れる） */
  for (ord = 0; ord < filter->ar_order; ord++) {
    putval = NARUUTILITY_SINT32_TO_UINT32(filter->ar_coef[ord]);
    NARU_ASSERT(putval < (1 << 16));
    NARUBitWriter_PutBits(stream, putval, 16);
  }

  /* フィルタ係数シフト量 */
  shift = NARUEncodeProcessor_CalculateBitShift(filter->weight, filter->filter_order, NARU_BLOCKHEADER_DATA_BITWIDTH);
  NARU_ASSERT(shift < (1 << NARU_BLOCKHEADER_SHIFT_BITWIDTH));
  NARUBitWriter_PutBits(stream, shift, NARU_BLOCKHEADER_SHIFT_BITWIDTH);

  /* フィルタ係数 */
  for (ord = 0; ord < filter->filter_order; ord++) {
    NARUEncodeProcessor_RoundAndPutSint(stream, NARU_BLOCKHEADER_DATA_BITWIDTH, &filter->weight[ord], shift);
  }

  /* フィルタ履歴シフト量 */
  shift = NARUEncodeProcessor_CalculateBitShift(filter->history, filter->filter_order, NARU_BLOCKHEADER_DATA_BITWIDTH);
  NARU_ASSERT(shift < (1 << NARU_BLOCKHEADER_SHIFT_BITWIDTH));
  NARUBitWriter_PutBits(stream, shift, NARU_BLOCKHEADER_SHIFT_BITWIDTH);

  /* フィルタ係数履歴 */
  for (ord = 0; ord < filter->filter_order; ord++) {
    NARUEncodeProcessor_RoundAndPutSint(stream, NARU_BLOCKHEADER_DATA_BITWIDTH, &filter->history[ord], shift);
  }
}

static void SAFilter_PutFilterState(
    struct SAFilter *filter, struct NARUBitStream *stream)
{
  int32_t ord;
  uint32_t shift;

  NARU_ASSERT(filter != NULL);
  NARU_ASSERT(stream != NULL);

  /* フィルタ履歴シフト量 */
  shift = NARUEncodeProcessor_CalculateBitShift(filter->history, filter->filter_order, NARU_BLOCKHEADER_DATA_BITWIDTH);
  NARU_ASSERT(shift < (1 << NARU_BLOCKHEADER_SHIFT_BITWIDTH));
  NARUBitWriter_PutBits(stream, shift, NARU_BLOCKHEADER_SHIFT_BITWIDTH);

  /* フィルタ係数履歴 */
  for (ord = 0; ord < filter->filter_order; ord++) {
    NARUEncodeProcessor_RoundAndPutSint(stream, NARU_BLOCKHEADER_DATA_BITWIDTH, &filter->history[ord], shift);
  }
}

void NARUEncodeProcessor_PutFilterState(
    struct NARUEncodeProcessor *processor, struct NARUBitStream *stream)
{
  NARU_ASSERT(processor != NULL);
  NARU_ASSERT(stream != NULL);

  /* プリエンファシスのバッファ MS処理によりbit幅は+1される */
  {
    const uint32_t putval = NARUUTILITY_SINT32_TO_UINT32(processor->preemphasis_prev);
    NARU_ASSERT(putval < (1 << 17));
    NARUBitWriter_PutBits(stream, putval, 17);
  }

  /* NGSAフィルタの状態 */
  NGSAFilter_PutFilterState(&processor->ngsa, stream);

  /* SAフィルタの状態 */
  SAFilter_PutFilterState(&processor->sa, stream);
}

void NARUEncodeProcessor_PreEmphasis(
    struct NARUEncodeProcessor *processor, int32_t *buffer, uint32_t num_samples)
{
  uint32_t smpl;
  int32_t tmp;
  const int32_t coef_numer = ((1 << NARU_EMPHASIS_FILTER_SHIFT) - 1);

  NARU_STATIC_ASSERT(NARU_FIXEDPOINT_DIGITS >= NARU_EMPHASIS_FILTER_SHIFT);

  NARU_ASSERT(processor != NULL);
  NARU_ASSERT(buffer != NULL);

  /* フィルタ適用 */
  for (smpl = 0; smpl < num_samples; smpl++) {
    tmp = buffer[smpl];
    buffer[smpl] -= (int32_t)NARUUTILITY_SHIFT_RIGHT_ARITHMETIC(processor->preemphasis_prev * coef_numer, NARU_EMPHASIS_FILTER_SHIFT);
    processor->preemphasis_prev = tmp;
  }
}

static int32_t NGSAFilter_Predict(struct NGSAFilter *filter, int32_t input)
{
  int32_t ord, residual, predict, direction;
  int32_t *ngrad, *history, *ar_coef, *weight;
  const int32_t scalar_shift = NARUNGSA_STEPSIZE_SCALE_BITWIDTH + NARUNGSA_STEPSIZE_SCALE_SHIFT;
  const int32_t filter_order = filter->filter_order;
  const int32_t ar_order = filter->ar_order;

  NARU_ASSERT(filter != NULL);

  /* ローカル変数に受けとく */
  ngrad = &filter->ngrad[0];
  history = &filter->history[0];
  ar_coef = &filter->ar_coef[0];
  weight = &filter->weight[0];

  /* フィルタ予測 */
  predict = NARU_FIXEDPOINT_0_5;
  for (ord = 0; ord < filter_order; ord++) {
    predict += weight[ord] * history[ord];
  }
  predict >>= NARU_FIXEDPOINT_DIGITS;

  /* 差分 */
  residual = input - predict;

  /* 自然勾配更新 */
  for (ord = 0; ord < ar_order; ord++) {
    ngrad[filter_order - 2 - ord] += NARU_FIXEDPOINT_MUL(ar_coef[ord], ngrad[filter_order - 1], NARU_FIXEDPOINT_DIGITS);
  }
  for (ord = filter_order - 1; ord > 0; ord--) {
    ngrad[ord] = ngrad[ord - 1];
  }
  ngrad[0] = NARU_FIXEDPOINT_0_5;
  for (ord = 0; ord < ar_order; ord++) {
    ngrad[0] += ar_coef[ord] * history[ord + 1];
  }
  ngrad[0] >>= NARU_FIXEDPOINT_DIGITS;
  ngrad[0] = history[0] - ngrad[0];
  for (ord = 0; ord < ar_order; ord++) {
    ngrad[ord + 1] -= NARU_FIXEDPOINT_MUL(ar_coef[ord], ngrad[0], NARU_FIXEDPOINT_DIGITS);
  }

  /* フィルタ係数更新 TODO: directionの値は3種類しかないのでキャッシュできる */
  direction = filter->stepsize_scale * NARUUTILITY_SIGN(residual);
  for (ord = 0; ord < filter_order; ord++) {
    weight[ord] += NARU_FIXEDPOINT_MUL(direction, ngrad[ord], scalar_shift);
  }

  /* 入力データ履歴更新 */
  for (ord = filter_order - 1; ord > 0; ord--) {
    history[ord] = history[ord - 1];
  }
  history[0] = input;

  return residual;
}

static int32_t SAFilter_Predict(struct SAFilter *filter, int32_t input)
{
  int32_t ord, residual, predict, sign;
  int32_t *history, *weight;
  const int32_t filter_order = filter->filter_order;

  NARU_ASSERT(filter != NULL);

  /* ローカル変数に受けとく */
  history = &filter->history[0];
  weight = &filter->weight[0];

  /* フィルタ予測 */
  predict = NARU_FIXEDPOINT_0_5;
  for (ord = 0; ord < filter_order; ord++) {
    predict += weight[ord] * history[ord];
  }
  predict >>= NARU_FIXEDPOINT_DIGITS;

  /* 差分 */
  residual = input - predict;

  /* 係数更新 */
  sign = NARUUTILITY_SIGN(residual);
  for (ord = 0; ord < filter_order; ord++) {
    weight[ord] += NARU_FIXEDPOINT_MUL(sign, filter->history[ord], NARUSA_STEPSIZE_SHIFT);
  }

  /* 入力データ履歴更新 */
  for (ord = filter_order - 1; ord > 0; ord--) {
    history[ord] = history[ord - 1];
  }
  history[0] = input;

  return residual;
}

void NARUEncodeProcessor_Predict(
  struct NARUEncodeProcessor *processor, int32_t *buffer, uint32_t num_samples)
{
  uint32_t smpl;

  /* 引数チェック */
  NARU_ASSERT(processor != NULL);
  NARU_ASSERT(buffer != NULL);
  NARU_ASSERT(num_samples > 0);

  /* フィルタ次数チェック */
  NARU_ASSERT(processor->ngsa.filter_order <= NARU_MAX_FILTER_ORDER);
  NARU_ASSERT(processor->ngsa.ar_order <= NARU_MAX_AR_ORDER);
  NARU_ASSERT(processor->ngsa.filter_order > (2 * processor->ngsa.ar_order));
  NARU_ASSERT(processor->sa.filter_order <= NARU_MAX_FILTER_ORDER);

  /* 自然勾配の初期化（初期の勾配: 入力データ履歴） */
  memcpy(processor->ngsa.ngrad,
      processor->ngsa.history, sizeof(int32_t) * (uint32_t)processor->ngsa.filter_order);

  /* 1サンプル毎に予測 
   * 補足）static関数なので、最適化時に展開されることを期待 */
  for (smpl = 0; smpl < num_samples; smpl++) {
    /* NGSA */
    buffer[smpl] = NGSAFilter_Predict(&processor->ngsa, buffer[smpl]);
    /* SA */
    buffer[smpl] = SAFilter_Predict(&processor->sa, buffer[smpl]);
  }
}

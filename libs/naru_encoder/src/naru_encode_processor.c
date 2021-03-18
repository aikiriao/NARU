#include "naru_encode_processor.h"
#include "naru_internal.h"
#include "naru_utility.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* フィルタ次数/AR次数に関するチェック */
NARU_STATIC_ASSERT(NARUUTILITY_IS_POWERED_OF_2(NARU_MAX_FILTER_ORDER));
NARU_STATIC_ASSERT(NARU_MAX_FILTER_ORDER > (2 * NARU_MAX_AR_ORDER));

/* NGSAフィルタの1サンプル予測処理 */
static int32_t NARUNGSAFilter_Predict(struct NARUNGSAFilter *filter, int32_t input);
/* NGSAフィルタの状態出力 */
static void NARUNGSAFilter_PutFilterState(struct NARUNGSAFilter *filter, struct NARUBitStream *stream);
/* SAフィルタの1サンプル予測処理 */
static int32_t NARUSAFilter_Predict(struct NARUSAFilter *filter, int32_t input);
/* SAフィルタの状態出力 */
static void NARUSAFilter_PutFilterState(struct NARUSAFilter *filter, struct NARUBitStream *stream);
/* dataをmaxbit内に収めるために必要な右シフト数計算 */
static uint32_t NARUEncodeProcessor_CalculateBitShift(const int32_t *data, int32_t num_data, int32_t maxbit);
/* フィルタ係数を引数のビット幅に丸め込む */
static void NARUEncodeProcessor_ClippingFilterWeight(int32_t *weight, int32_t filter_order, int32_t bitwidth);
/* 符号付き整数pvalをrshiftした値を出力 その後シフトしたビット数だけpvalの下位bitをクリア */
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

/* フィルタ係数を引数のビット幅に丸め込む */
static void NARUEncodeProcessor_ClippingFilterWeight(
    int32_t *weight, int32_t filter_order, int32_t bitwidth)
{
  int32_t ord;
  double maxabs, inv_scale;

  NARU_ASSERT(weight != NULL);
  NARU_ASSERT(filter_order > 0);
  NARU_ASSERT((bitwidth > 0) && (bitwidth < 32));
  
  /* [-1,1]に丸めた中で、最大絶対値を計測 */
  maxabs = 0.0f;
  for (ord = 0; ord < filter_order; ord++) {
    double abs = NARUUTILITY_ABS(weight[ord]) * pow(2.0f, -(bitwidth - 1));
    if (maxabs < abs) {
      maxabs = abs;
    }
  }

  /* ビット幅に収まっている: 丸め不要 */
  if (maxabs <= 1.0f) {
    return;
  }

  /* 最大絶対値がビット幅いっぱいに収まるようにクリッピング */
  inv_scale = 1.0f / maxabs;
  for (ord = 0; ord < filter_order; ord++) {
    weight[ord] = (int32_t)(inv_scale * weight[ord]);
  }
}

/* 符号付き整数pvalをrshiftした値を出力 その後シフトしたビット数だけpvalの下位bitをクリア */
static void NARUEncodeProcessor_RoundAndPutSint(
    struct NARUBitStream *stream, uint32_t bitwidth, int32_t *pval, uint32_t rshift)
{
  uint32_t putval;
  int32_t roundval;

  NARU_ASSERT(stream != NULL);
  NARU_ASSERT(pval != NULL);

  /* 右シフト値を取得 */
  roundval = NARUUTILITY_SHIFT_RIGHT_ARITHMETIC((*pval), rshift);

  /* 符号なし整数に変換 */
  putval = NARUUTILITY_SINT32_TO_UINT32(roundval);

  /* 出力 */
  NARU_ASSERT(putval < (1 << bitwidth));
  NARUBitWriter_PutBits(stream, putval, bitwidth);

  /* 左シフトして戻す（シフトしたビットをクリア） */
  (*pval) = roundval << rshift;
}

/* プロセッサのリセット */
void NARUEncodeProcessor_Reset(struct NARUEncodeProcessor *processor)
{
  NARU_ASSERT(processor != NULL);

  /* フィルタをリセット */
  NARUSAFilter_Reset(&processor->sa);
  NARUNGSAFilter_Reset(&processor->ngsa);

  processor->preemphasis_prev = 0;
}

/* フィルタ次数の設定 */
void NARUEncodeProcessor_SetFilterOrder(
  struct NARUEncodeProcessor *processor, int32_t filter_order,
  int32_t ar_order, int32_t second_filter_order)
{
  NARU_ASSERT(processor != NULL);

  NARUNGSAFilter_SetFilterOrder(&processor->ngsa, filter_order, ar_order);
  NARUSAFilter_SetFilterOrder(&processor->sa, second_filter_order);
}

/* AR係数の計算 */
void NARUEncodeProcessor_CalculateARCoef(
    struct NARUEncodeProcessor *processor, struct LPCCalculator *lpcc,
    const double *input, uint32_t num_samples)
{
  int32_t ord;
  double coef_double[NARU_MAX_FILTER_ORDER + 1];
  LPCCalculatorApiResult ret;
  int32_t ar_order;

  NARU_ASSERT(processor != NULL);
  NARU_ASSERT(lpcc != NULL);
  NARU_ASSERT(input != NULL);
  NARU_ASSERT(num_samples > 0);

  ar_order = processor->ngsa.ar_order;
  NARU_ASSERT(ar_order <= NARU_MAX_AR_ORDER);

  /* AR係数の計算 */
  ret = LPCCalculator_CalculateLPCCoef(lpcc, input, num_samples, coef_double, (uint32_t)ar_order);
  NARU_ASSERT(ret == LPCCALCULATOR_APIRESULT_OK);
  /* 整数量子化: ar_coef_doubleのインデックスは1.0確定なのでスルー */
  for (ord = 0; ord < ar_order; ord++) {
    double tmp_coef = NARUUtility_Round(coef_double[ord + 1] * pow(2.0f, NARU_FIXEDPOINT_DIGITS));
    processor->ngsa.ar_coef[ord] = (int32_t)tmp_coef;
  }
}

/* NGSAフィルタの状態出力 */
static void NARUNGSAFilter_PutFilterState(
    struct NARUNGSAFilter *filter, struct NARUBitStream *stream)
{
  int32_t ord;
  uint32_t shift;

  NARU_ASSERT(filter != NULL);
  NARU_ASSERT(stream != NULL);

  /* AR係数 */
  shift = NARUEncodeProcessor_CalculateBitShift(filter->ar_coef, filter->ar_order, NARU_BLOCKHEADER_ARCOEF_BITWIDTH);
  NARU_ASSERT(shift < (1 << NARU_BLOCKHEADER_ARCOEFSHIFT_BITWIDTH));
  NARUBitWriter_PutBits(stream, shift, NARU_BLOCKHEADER_ARCOEFSHIFT_BITWIDTH);
  for (ord = 0; ord < filter->ar_order; ord++) {
    NARUEncodeProcessor_RoundAndPutSint(stream, NARU_BLOCKHEADER_ARCOEF_BITWIDTH, &filter->ar_coef[ord], shift);
  }

  /* フィルタ係数値を固定bit幅に制限 */
  NARUEncodeProcessor_ClippingFilterWeight(filter->weight, filter->filter_order, NARU_FILTER_WEIGHT_RANGE_BITWIDTH);

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

  /* フィルタ履歴 */
  for (ord = 0; ord < filter->filter_order; ord++) {
    /* フィルタ次数の倍数サンプル入力ではないとき、バッファ参照位置がずれるため、
       バッファ参照位置だけずらして記録 */
    int32_t pos = (filter->buffer_pos + ord) & filter->buffer_pos_mask;
    NARUEncodeProcessor_RoundAndPutSint(stream, NARU_BLOCKHEADER_DATA_BITWIDTH, &filter->history[pos], shift);
    /* 履歴アクセス高速化のために、次数だけ離れた位置にも記録 */
    filter->history[pos + filter->filter_order] = filter->history[pos];
  }
}

/* SAフィルタの状態出力 */
static void NARUSAFilter_PutFilterState(
    struct NARUSAFilter *filter, struct NARUBitStream *stream)
{
  int32_t ord;
  uint32_t shift;

  NARU_ASSERT(filter != NULL);
  NARU_ASSERT(stream != NULL);

  /* フィルタ係数値を固定bit幅に制限 */
  NARUEncodeProcessor_ClippingFilterWeight(filter->weight, filter->filter_order, NARU_FILTER_WEIGHT_RANGE_BITWIDTH);

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

  /* フィルタ履歴 */
  for (ord = 0; ord < filter->filter_order; ord++) {
    /* フィルタ次数の倍数サンプル入力ではないとき、バッファ参照位置がずれるため、
       バッファ参照位置だけずらして記録 */
    int32_t pos = (filter->buffer_pos + ord) & filter->buffer_pos_mask;
    NARUEncodeProcessor_RoundAndPutSint(stream, NARU_BLOCKHEADER_DATA_BITWIDTH, &filter->history[pos], shift);
    /* 履歴アクセス高速化のために、次数だけ離れた位置にも記録 */
    filter->history[pos + filter->filter_order] = filter->history[pos];
  }
}

/* プロセッサの状態出力 */
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
  NARUNGSAFilter_PutFilterState(&processor->ngsa, stream);

  /* SAフィルタの状態 */
  NARUSAFilter_PutFilterState(&processor->sa, stream);
}

/* プリエンファシス */
static int32_t NARUEncodeProcessor_PreEmphasis(struct NARUEncodeProcessor *processor, int32_t input)
{
  int32_t tmp;
  const int32_t coef_numer = ((1 << NARU_EMPHASIS_FILTER_SHIFT) - 1);

  NARU_STATIC_ASSERT(NARU_FIXEDPOINT_DIGITS >= NARU_EMPHASIS_FILTER_SHIFT);

  NARU_ASSERT(processor != NULL);

  /* フィルタ適用 */
  tmp = input;
  input -= (int32_t)NARUUTILITY_SHIFT_RIGHT_ARITHMETIC(processor->preemphasis_prev * coef_numer, NARU_EMPHASIS_FILTER_SHIFT);
  processor->preemphasis_prev = tmp;

  return input;
}

/* NGSAフィルタの1サンプル予測処理 */
static int32_t NARUNGSAFilter_Predict(struct NARUNGSAFilter *filter, int32_t input)
{
  int32_t ord, residual, predict, delta;
  int32_t *ngrad, *history, *ar_coef, *weight;
  const int32_t scalar_shift = NARUNGSA_STEPSIZE_SCALE_BITWIDTH + NARUNGSA_STEPSIZE_SCALE_SHIFT;
  const int32_t filter_order = filter->filter_order;
  const int32_t ar_order = filter->ar_order;

  NARU_ASSERT(filter != NULL);

  /* ローカル変数に受けとく */
  history = &filter->history[filter->buffer_pos];
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

  /* バッファ参照位置更新 */
  filter->buffer_pos = (filter->buffer_pos - 1) & filter->buffer_pos_mask;

  /* 自然勾配更新 */
  ngrad = &filter->ngrad[filter->buffer_pos];
  for (ord = 0; ord < ar_order; ord++) {
    const int32_t pos = (filter->buffer_pos + filter_order - 1 - ord) & filter->buffer_pos_mask;
    filter->ngrad[pos] += NARU_FIXEDPOINT_MUL(ar_coef[ord], ngrad[filter_order], NARU_FIXEDPOINT_DIGITS);
    filter->ngrad[pos + filter_order] = filter->ngrad[pos];
  }
  ngrad[0] = 0;
  for (ord = 0; ord < ar_order; ord++) {
    ngrad[0] -= ar_coef[ord] * history[ord + 1];
  }
  ngrad[0] >>= NARU_FIXEDPOINT_DIGITS;
  ngrad[0] += history[0];
  for (ord = 0; ord < ar_order; ord++) {
    const int32_t pos = (filter->buffer_pos + ord + 1) & filter->buffer_pos_mask;
    filter->ngrad[pos] -= NARU_FIXEDPOINT_MUL(ar_coef[ord], ngrad[0], NARU_FIXEDPOINT_DIGITS);
    filter->ngrad[pos + filter_order] = filter->ngrad[pos];
  }

  /* フィルタ係数更新 */
  NARU_ASSERT(filter->pdelta_table == &filter->delta_table[1]);
  delta = filter->pdelta_table[NARUUTILITY_SIGN(residual)];
  for (ord = 0; ord < filter_order; ord++) {
    weight[ord] += NARU_FIXEDPOINT_MUL(delta, ngrad[ord], scalar_shift);
  }

  /* 入力データ履歴更新 */
  filter->history[filter->buffer_pos]
    = filter->history[filter->buffer_pos + filter_order] = input;

  return residual;
}

/* SAフィルタの1サンプル予測処理 */
static int32_t NARUSAFilter_Predict(struct NARUSAFilter *filter, int32_t input)
{
  int32_t ord, residual, predict, sign;
  int32_t *history, *weight;
  const int32_t filter_order = filter->filter_order;

  NARU_ASSERT(filter != NULL);
  
  /* ローカル変数に受けとく */
  history = &filter->history[filter->buffer_pos];
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
    weight[ord] += NARU_FIXEDPOINT_MUL(sign, history[ord], NARUSA_STEPSIZE_SHIFT);
  }

  /* 入力データ履歴更新 */
  filter->buffer_pos = (filter->buffer_pos - 1) & filter->buffer_pos_mask;
  filter->history[filter->buffer_pos]
    = filter->history[filter->buffer_pos + filter_order] = input;

  return residual;
}

/* 予測 */
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

  /* 自然勾配の初期化 */
  NARUNGSAFilter_InitializeNaturalGradient(&processor->ngsa);

  /* 1サンプル毎に予測 
   * 補足）static関数なので、最適化時に展開されることを期待 */
  for (smpl = 0; smpl < num_samples; smpl++) {
    /* プリエンファシス */
    buffer[smpl] = NARUEncodeProcessor_PreEmphasis(processor, buffer[smpl]);
    /* NGSA */
    buffer[smpl] = NARUNGSAFilter_Predict(&processor->ngsa, buffer[smpl]);
    /* SA */
    buffer[smpl] = NARUSAFilter_Predict(&processor->sa, buffer[smpl]);
  }
}

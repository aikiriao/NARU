#include "naru_decode_processor.h"
#include "naru_internal.h"
#include "naru_utility.h"

#include <stdlib.h>
#include <string.h>

/* 符号付き整数の取得 */
#define NARU_GETSINT32(stream, pval, bitwidth)\
  do {\
    uint32_t tmpbuf;\
    NARU_ASSERT((stream) != NULL);\
    NARU_ASSERT((pval) != NULL);\
    NARU_ASSERT((bitwidth) > 0 && (bitwidth) <= 32);\
    NARUBitReader_GetBits((stream), &tmpbuf, (bitwidth));\
    (*pval) = NARUUTILITY_UINT32_TO_SINT32(tmpbuf);\
  } while (0);

/* 符号なし整数の取得 */
#define NARU_GETUINT32(stream, pval, bitwidth)\
  do {\
    uint32_t tmpbuf;\
    NARU_ASSERT((stream) != NULL);\
    NARU_ASSERT((pval) != NULL);\
    NARU_ASSERT((bitwidth) > 0 && (bitwidth) <= 32);\
    NARUBitReader_GetBits((stream), &tmpbuf, (bitwidth));\
    (*pval) = tmpbuf;\
  } while (0);

/* NGSAフィルタの自然勾配初期化 */
static void NGSAFilter_InitializeNaturalGradient(struct NGSAFilter *filter);
/* NGSAフィルタの1サンプル合成処理 */
static int32_t NGSAFilter_Synthesize(struct NGSAFilter *filter, int32_t residual);
/* NGSAフィルタの状態取得 */
static void NGSAFilter_GetFilterState(struct NGSAFilter *filter, struct NARUBitStream *stream);
/* SAフィルタの1サンプル合成処理 */
static int32_t SAFilter_Synthesize(struct SAFilter *filter, int32_t residual);
/* SAフィルタの状態取得 */
static void SAFilter_GetFilterState(struct SAFilter *filter, struct NARUBitStream *stream);
/* 1サンプルをデエンファシス */
static int32_t NARUEncodeProcessor_DeEmphasis(struct NARUDecodeProcessor *processor, int32_t residual); 

/* プロセッサのリセット */
void NARUDecodeProcessor_Reset(struct NARUDecodeProcessor *processor)
{
  /* 引数チェック */
  NARU_ASSERT(processor != NULL);

  /* 各メンバを0クリア */
  memset(&processor->ngsa, 0, sizeof(struct NGSAFilter));
  memset(&processor->sa, 0, sizeof(struct SAFilter));

  processor->deemphasis_prev = 0;
}

/* NGSAフィルタの状態取得 */
static void NGSAFilter_GetFilterState(
    struct NGSAFilter *filter, struct NARUBitStream *stream)
{
  int32_t ord;
  uint32_t shift;

  /* 引数チェック */
  NARU_ASSERT(filter != NULL);
  NARU_ASSERT(stream != NULL);

  /* AR係数 */ 
  for (ord = 0; ord < filter->ar_order; ord++) {
    NARU_GETSINT32(stream, &filter->ar_coef[ord], 16);
  }

  /* フィルタ係数シフト量 */
  NARU_GETUINT32(stream, &shift, NARU_BLOCKHEADER_SHIFT_BITWIDTH);
  /* フィルタ係数 */
  for (ord = 0; ord < filter->filter_order; ord++) {
    NARU_GETSINT32(stream, &filter->weight[ord], NARU_BLOCKHEADER_DATA_BITWIDTH);
    filter->weight[ord] <<= shift;
  }

  /* フィルタ履歴シフト量 */
  NARU_GETUINT32(stream, &shift, NARU_BLOCKHEADER_SHIFT_BITWIDTH);
  /* フィルタ履歴 */
  for (ord = 0; ord < filter->filter_order; ord++) {
    NARU_GETSINT32(stream, &filter->history[ord], NARU_BLOCKHEADER_DATA_BITWIDTH);
    filter->history[ord] <<= shift;
  }

  /* ステップサイズに乗じる係数の計算 */
  if (filter->ar_order == 1) {
    int32_t scale_int;
    /* 1.0f / (1.0f - ar_coef[0] ** 2) */
    scale_int = (1 << 15) - NARU_FIXEDPOINT_MUL(processor->ngsa.ar_coef[0], processor->ngsa.ar_coef[0], NARU_FIXEDPOINT_DIGITS);
    scale_int = (1 << 30) / scale_int;
    filter->stepsize_scale = scale_int >> (NARU_FIXEDPOINT_DIGITS - NARUNGSA_STEPSIZE_SCALE_BITWIDTH);
    /* 係数が大きくなりすぎないようにクリップ */
    filter->stepsize_scale = NARUUTILITY_MIN(NARUNGSA_MAX_STEPSIZE_SCALE, filter->stepsize_scale);
  } else {
    filter->stepsize_scale = 1 << NARUNGSA_STEPSIZE_SCALE_BITWIDTH;
  }
}

/* SAフィルタの状態取得 */
static void SAFilter_GetFilterState(
    struct SAFilter *filter, struct NARUBitStream *stream)
{
  int32_t ord;
  uint32_t shift;

  /* 引数チェック */
  NARU_ASSERT(filter != NULL);
  NARU_ASSERT(stream != NULL);

  /* フィルタ係数シフト量 */
  NARU_GETUINT32(stream, &shift, NARU_BLOCKHEADER_SHIFT_BITWIDTH);
  /* フィルタ係数 */
  for (ord = 0; ord < filter->filter_order; ord++) {
    NARU_GETSINT32(stream, &filter->weight[ord], NARU_BLOCKHEADER_DATA_BITWIDTH);
    filter->weight[ord] <<= shift;
  }

  /* フィルタ履歴シフト量 */
  NARU_GETUINT32(stream, &shift, NARU_BLOCKHEADER_SHIFT_BITWIDTH);
  /* フィルタ係数履歴 */
  for (ord = 0; ord < filter->filter_order; ord++) {
    NARU_GETSINT32(stream, &filter->history[ord], NARU_BLOCKHEADER_DATA_BITWIDTH);
    filter->history[ord] <<= shift;
  }
}

/* プロセッサの状態を取得 */
void NARUDecodeProcessor_GetFilterState(
    struct NARUDecodeProcessor *processor, struct NARUBitStream *stream)
{
  /* 引数チェック */
  NARU_ASSERT(processor != NULL);
  NARU_ASSERT(stream != NULL);

  /* デエンファシスのバッファ */
  NARU_GETSINT32(stream, &processor->deemphasis_prev, 17);

  /* NGSAフィルタの状態 */
  NGSAFilter_GetFilterState(&processor->ngsa, stream);

  /* SAフィルタの状態 */
  SAFilter_GetFilterState(&processor->sa, stream);
}

/* 1サンプルをデエンファシス */
static int32_t NARUEncodeProcessor_DeEmphasis(struct NARUDecodeProcessor *processor, int32_t residual)
{
  const int32_t coef_numer = ((1 << NARU_EMPHASIS_FILTER_SHIFT) - 1);

  NARU_ASSERT(processor != NULL);

  /* 直前の値に固定フィルタ出力を加算 */
  residual += (int32_t)NARUUTILITY_SHIFT_RIGHT_ARITHMETIC(processor->deemphasis_prev * coef_numer, NARU_EMPHASIS_FILTER_SHIFT);

  /* 直前の値を更新 */
  processor->deemphasis_prev = residual;

  return residual;
}

/* NGSAフィルタの自然勾配初期化 */
static void NGSAFilter_InitializeNaturalGradient(struct NGSAFilter *filter)
{
  const int32_t filter_order = filter->filter_order;
  int32_t *ngrad, *history;

  NARU_ASSERT(filter != NULL);

  ngrad = &filter->ngrad[0];
  history = &filter->history[0];

  if (filter->ar_order == 1) {
    /* 1の場合は履歴とAR係数から直接計算 */
    int32_t ord;
    int32_t *ar_coef;

    ar_coef = &filter->ar_coef[0];

    for (ord = 0; ord < filter_order - 1; ord++) {
      ngrad[ord]
        = history[ord] - NARU_FIXEDPOINT_MUL(ar_coef[0], history[ord + 1], NARU_FIXEDPOINT_DIGITS);
    }
    for (ord = 1; ord < filter_order - 1; ord++) {
      ngrad[ord] 
        -= NARU_FIXEDPOINT_MUL(ar_coef[0], ngrad[ord - 1], NARU_FIXEDPOINT_DIGITS);
    }
    ngrad[filter_order - 1]
      = history[filter_order - 1] - NARU_FIXEDPOINT_MUL(ar_coef[0], history[filter_order - 2], NARU_FIXEDPOINT_DIGITS);
  } else {
    /* 1より大きい場合は履歴を初期値とする */
    memcpy(ngrad, history, sizeof(int32_t) * (uint32_t)filter_order);
  }
}

/* NGSAフィルタの1サンプル合成処理 */
static int32_t NGSAFilter_Synthesize(struct NGSAFilter *filter, int32_t residual)
{
  int32_t ord, synth, predict, direction;
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

  /* 合成復元 */
  synth = residual + predict;

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
  history[0] = synth;

  return synth;
}

/* SAフィルタの1サンプル合成処理 */
static int32_t SAFilter_Synthesize(struct SAFilter *filter, int32_t residual)
{
  int32_t ord, synth, predict, sign;
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

  /* 合成復元 */
  synth = residual + predict;

  /* 係数更新 */
  sign = NARUUTILITY_SIGN(residual);
  for (ord = 0; ord < filter_order; ord++) {
    weight[ord] += NARU_FIXEDPOINT_MUL(sign, filter->history[ord], NARUSA_STEPSIZE_SHIFT);
  }

  /* 入力データ履歴更新 */
  for (ord = filter_order - 1; ord > 0; ord--) {
    history[ord] = history[ord - 1];
  }
  history[0] = synth;

  return synth;
}

/* 合成 */
void NARUDecodeProcessor_Synthesize(
    struct NARUDecodeProcessor *processor, int32_t *buffer, uint32_t num_samples)
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
  NGSAFilter_InitializeNaturalGradient(&processor->ngsa);

  /* 1サンプル毎に合成 
   * 補足）static関数なので、最適化時に展開されることを期待 */
  for (smpl = 0; smpl < num_samples; smpl++) {
    /* SA */
    buffer[smpl] = SAFilter_Synthesize(&processor->sa, buffer[smpl]);
    /* NGSA */
    buffer[smpl] = NGSAFilter_Synthesize(&processor->ngsa, buffer[smpl]);
    /* デエンファシス */
    buffer[smpl] = NARUEncodeProcessor_DeEmphasis(processor, buffer[smpl]);
  }
}

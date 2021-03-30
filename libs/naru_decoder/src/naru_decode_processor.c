#include "naru_decode_processor.h"
#include "naru_internal.h"
#include "naru_filter.h"
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

/* プロセッサハンドル */
struct NARUDecodeProcessor {
  /* SA Filter */
  struct NARUSAFilter *sa;
  /* NGSA Filter */
  struct NARUNGSAFilter *ngsa;
  /* De Emphasis */
  int32_t deemphasis_prev;
};

/* NGSAフィルタの1サンプル合成処理 */
static int32_t NARUNGSAFilter_Synthesize(struct NARUNGSAFilter *filter, int32_t residual);
/* NGSAフィルタの状態取得 */
static void NARUNGSAFilter_GetFilterState(struct NARUNGSAFilter *filter, struct NARUBitStream *stream);
/* SAフィルタの1サンプル合成処理 */
static int32_t NARUSAFilter_Synthesize(struct NARUSAFilter *filter, int32_t residual);
/* SAフィルタの状態取得 */
static void NARUSAFilter_GetFilterState(struct NARUSAFilter *filter, struct NARUBitStream *stream);
/* 1サンプルをデエンファシス */
static int32_t NARUDecodeProcessor_DeEmphasis(struct NARUDecodeProcessor *processor, int32_t residual); 

/* プロセッサ作成に必要なワークサイズ計算 */
int32_t NARUDecodeProcessor_CalculateWorkSize(uint8_t max_filter_order)
{
  int32_t work_size, tmp;

  /* 構造体本体のサイズ */
  work_size = sizeof(struct NARUDecodeProcessor) + NARU_MEMORY_ALIGNMENT;

  /* SAフィルタのサイズ */
  if ((tmp = NARUSAFilter_CalculateWorkSize(max_filter_order)) < 0) {
    return -1;
  }
  work_size += tmp;

  /* NGSAフィルタのサイズ */
  if ((tmp = NARUNGSAFilter_CalculateWorkSize(max_filter_order)) < 0) {
    return -1;
  }
  work_size += tmp;

  return work_size;
}

/* プロセッサ作成 */
struct NARUDecodeProcessor* NARUDecodeProcessor_Create(uint8_t max_filter_order, void *work, int32_t work_size)
{
  uint8_t *work_ptr;
  struct NARUDecodeProcessor *processor;

  /* 引数チェック */
  if ((work == NULL)
      || (work_size < NARUDecodeProcessor_CalculateWorkSize(max_filter_order))) {
    return NULL;
  }

  /* 無効なフィルタ次数 */
  if ((max_filter_order == 0)
      || !(NARUUTILITY_IS_POWERED_OF_2(max_filter_order))) {
    return NULL;
  }

  /* 構造体配置 */
  work_ptr = (uint8_t *)NARUUTILITY_ROUNDUP((uintptr_t)work, NARU_MEMORY_ALIGNMENT);
  processor = (struct NARUDecodeProcessor *)work_ptr;
  work_ptr += sizeof(struct NARUDecodeProcessor);

  /* SAフィルタ作成 */
  {
    int32_t tmp_work_size = NARUSAFilter_CalculateWorkSize(max_filter_order);
    if ((processor->sa = NARUSAFilter_Create(max_filter_order, work_ptr, tmp_work_size)) == NULL) {
      return NULL;
    }
    work_ptr += tmp_work_size;
  }

  /* NGSAフィルタ作成 */
  {
    int32_t tmp_work_size = NARUNGSAFilter_CalculateWorkSize(max_filter_order);
    if ((processor->ngsa = NARUNGSAFilter_Create(max_filter_order, work_ptr, tmp_work_size)) == NULL) {
      return NULL;
    }
    work_ptr += tmp_work_size;
  }

  /* オーバーフローチェック */
  NARU_ASSERT((work_ptr - (uint8_t *)work) <= work_size);

  return processor;
}

/* プロセッサ破棄 */
void NARUDecodeProcessor_Destroy(struct NARUDecodeProcessor *processor)
{
  /* 特に何もしない */
  NARU_ASSERT(processor != NULL);
}

/* プロセッサのリセット */
void NARUDecodeProcessor_Reset(struct NARUDecodeProcessor *processor)
{
  /* 引数チェック */
  NARU_ASSERT(processor != NULL);

  /* 各フィルタのリセット */
  NARUNGSAFilter_Reset(processor->ngsa);
  NARUSAFilter_Reset(processor->sa);

  processor->deemphasis_prev = 0;
}

/* フィルタ次数の設定 */
void NARUDecodeProcessor_SetFilterOrder(
  struct NARUDecodeProcessor *processor, int32_t filter_order,
  int32_t ar_order, int32_t second_filter_order)
{
  /* 引数チェック */
  NARU_ASSERT(processor != NULL);

  NARUNGSAFilter_SetFilterOrder(processor->ngsa, filter_order, ar_order);
  NARUSAFilter_SetFilterOrder(processor->sa, second_filter_order);
}

/* NGSAフィルタの状態取得 */
static void NARUNGSAFilter_GetFilterState(
    struct NARUNGSAFilter *filter, struct NARUBitStream *stream)
{
  int32_t ord;
  uint32_t shift;

  /* 引数チェック */
  NARU_ASSERT(filter != NULL);
  NARU_ASSERT(stream != NULL);

  /* AR係数 */ 
  NARU_GETUINT32(stream, &shift, NARU_BLOCKHEADER_ARCOEFSHIFT_BITWIDTH);
  for (ord = 0; ord < filter->ar_order; ord++) {
    NARU_GETSINT32(stream, &filter->ar_coef[ord], NARU_BLOCKHEADER_ARCOEF_BITWIDTH);
    filter->ar_coef[ord] <<= shift;
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
    /* 履歴アクセス高速化のために、次数だけ離れた位置にも記録 */
    filter->history[filter->filter_order + ord] = filter->history[ord];
  }
}

/* SAフィルタの状態取得 */
static void NARUSAFilter_GetFilterState(
    struct NARUSAFilter *filter, struct NARUBitStream *stream)
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
    /* 履歴アクセス高速化のために、次数だけ離れた位置にも記録 */
    filter->history[filter->filter_order + ord] = filter->history[ord];
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
  NARUNGSAFilter_GetFilterState(processor->ngsa, stream);

  /* SAフィルタの状態 */
  NARUSAFilter_GetFilterState(processor->sa, stream);
}

/* 1サンプルをデエンファシス */
static int32_t NARUDecodeProcessor_DeEmphasis(struct NARUDecodeProcessor *processor, int32_t residual)
{
  const int32_t coef_numer = ((1 << NARU_EMPHASIS_FILTER_SHIFT) - 1);

  NARU_ASSERT(processor != NULL);

  /* 直前の値に固定フィルタ出力を加算 */
  residual += (int32_t)NARUUTILITY_SHIFT_RIGHT_ARITHMETIC(processor->deemphasis_prev * coef_numer, NARU_EMPHASIS_FILTER_SHIFT);

  /* 直前の値を更新 */
  processor->deemphasis_prev = residual;

  return residual;
}

/* NGSAフィルタの1サンプル合成処理 */
static int32_t NARUNGSAFilter_Synthesize(struct NARUNGSAFilter *filter, int32_t residual)
{
  int32_t ord, synth, predict;
  int32_t *ngrad, *history, *ar_coef, *weight;
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
  predict = NARUUTILITY_SHIFT_RIGHT_ARITHMETIC(predict, NARU_FIXEDPOINT_DIGITS);

  /* 合成復元 */
  synth = residual + predict;

  /* バッファ参照位置更新 */
  filter->buffer_pos = (filter->buffer_pos - 1) & filter->buffer_pos_mask;

  /* 自然勾配更新 */
  ngrad = &filter->ngrad[filter->buffer_pos];
  for (ord = 0; ord < ar_order; ord++) {
    const int32_t pos = (filter->buffer_pos + filter_order - 1 - ord) & filter->buffer_pos_mask;
    filter->ngrad[pos] += NARUUTILITY_SHIFT_RIGHT_ARITHMETIC(ar_coef[ord] * ngrad[filter_order], NARU_FIXEDPOINT_DIGITS);
    filter->ngrad[pos + filter_order] = filter->ngrad[pos];
  }
  ngrad[0] = 0;
  for (ord = 0; ord < ar_order; ord++) {
    ngrad[0] -= ar_coef[ord] * history[ord + 1];
  }
  ngrad[0] = NARUUTILITY_SHIFT_RIGHT_ARITHMETIC(ngrad[0], NARU_FIXEDPOINT_DIGITS);
  ngrad[0] += history[0];
  for (ord = 0; ord < ar_order; ord++) {
    const int32_t pos = (filter->buffer_pos + ord + 1) & filter->buffer_pos_mask;
    filter->ngrad[pos] -= NARUUTILITY_SHIFT_RIGHT_ARITHMETIC(ar_coef[ord] * ngrad[0], NARU_FIXEDPOINT_DIGITS);
    filter->ngrad[pos + filter_order] = filter->ngrad[pos];
  }

  /* フィルタ係数更新 */
  NARU_ASSERT(filter->pdelta_table == &filter->delta_table[1]);
  {
    const int32_t half = (1 << (filter->delta_rshift - 1));
    const int32_t delta = filter->pdelta_table[NARUUTILITY_SIGN(residual)];
    for (ord = 0; ord < filter_order; ord++) {
      int32_t mul = delta * ngrad[ord];
      mul += half;
      mul = NARUUTILITY_SHIFT_RIGHT_ARITHMETIC(mul, filter->delta_rshift);
      weight[ord] += mul;
    }
  }

  /* 入力データ履歴更新 */
  filter->history[filter->buffer_pos]
    = filter->history[filter->buffer_pos + filter_order] = synth;

  return synth;
}

/* SAフィルタの1サンプル合成処理 */
static int32_t NARUSAFilter_Synthesize(struct NARUSAFilter *filter, int32_t residual)
{
  int32_t ord, synth, predict, sign;
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
  predict = NARUUTILITY_SHIFT_RIGHT_ARITHMETIC(predict, NARU_FIXEDPOINT_DIGITS);

  /* 合成復元 */
  synth = residual + predict;

  /* 係数更新 */
  sign = NARUUTILITY_SIGN(residual);
  for (ord = 0; ord < filter_order; ord++) {
    weight[ord] += sign * history[ord];
  }

  /* 入力データ履歴更新 */
  filter->buffer_pos = (filter->buffer_pos - 1) & filter->buffer_pos_mask;
  filter->history[filter->buffer_pos]
    = filter->history[filter->buffer_pos + filter_order] = synth;

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
  NARU_ASSERT(processor->ngsa->filter_order <= processor->ngsa->max_filter_order);
  NARU_ASSERT(processor->ngsa->ar_order <= processor->ngsa->max_filter_order);
  NARU_ASSERT(processor->ngsa->filter_order > (2 * processor->ngsa->ar_order));
  NARU_ASSERT(processor->sa->filter_order <= processor->sa->max_filter_order);

  /* 自然勾配の初期化 */
  NARUNGSAFilter_InitializeNaturalGradient(processor->ngsa);

  /* 1サンプル毎に合成 
   * 補足）static関数なので、最適化時に展開されることを期待 */
  for (smpl = 0; smpl < num_samples; smpl++) {
    /* SA */
    buffer[smpl] = NARUSAFilter_Synthesize(processor->sa, buffer[smpl]);
    /* NGSA */
    buffer[smpl] = NARUNGSAFilter_Synthesize(processor->ngsa, buffer[smpl]);
    /* デエンファシス */
    buffer[smpl] = NARUDecodeProcessor_DeEmphasis(processor, buffer[smpl]);
  }
}

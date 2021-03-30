#include "naru_filter.h"
#include "naru_internal.h"
#include "naru_utility.h"

#include <string.h>

/* 固定小数点数の乗算（丸め対策込み） */
#define NARUFILTER_FIXEDPOINT_MUL(a, b, shift) NARUUTILITY_SHIFT_RIGHT_ARITHMETIC((a) * (b) + (1 << ((shift) - 1)), (shift))

/* SAフィルタの作成に必要なワークサイズ計算 */
int32_t NARUSAFilter_CalculateWorkSize(uint8_t max_filter_order)
{
  int32_t work_size;

  /* 無効な次数 */
  if ((max_filter_order == 0)
      || !(NARUUTILITY_IS_POWERED_OF_2(max_filter_order))) {
    return -1;
  }

  /* 構造体本体分のサイズ（+アラインメント） */
  work_size = sizeof(struct NARUSAFilter) + NARU_MEMORY_ALIGNMENT;
  /* 入力データ履歴分のサイズ */
  work_size += sizeof(int32_t) * 2 * max_filter_order;
  /* フィルタ係数サイズ */
  work_size += sizeof(int32_t) * max_filter_order;

  return work_size;
}

/* SAフィルタの作成（自己割当不可） */
struct NARUSAFilter* NARUSAFilter_Create(uint8_t max_filter_order, void *work, int32_t work_size)
{
  uint8_t *work_ptr;
  struct NARUSAFilter *filter;

  /* 引数チェック */
  if ((work == NULL)
      || (work_size < NARUSAFilter_CalculateWorkSize(max_filter_order))) {
    return NULL;
  }

  /* 無効なフィルタ次数 */
  if ((max_filter_order == 0)
      || !(NARUUTILITY_IS_POWERED_OF_2(max_filter_order))) {
    return NULL;
  }

  /* 構造体配置 */
  work_ptr = (uint8_t *)NARUUTILITY_ROUNDUP((uintptr_t)work, NARU_MEMORY_ALIGNMENT);
  filter = (struct NARUSAFilter *)work_ptr;
  work_ptr += sizeof(struct NARUSAFilter);

  /* 最大フィルタ次数設定 */
  filter->max_filter_order = max_filter_order;

  /* 入力データ履歴配置 */
  filter->history = (int32_t *)work_ptr;
  work_ptr += sizeof(int32_t) * 2 * max_filter_order;

  /* フィルタ係数配置 */
  filter->weight = (int32_t *)work_ptr;
  work_ptr += sizeof(int32_t) * max_filter_order;

  /* オーバーフローチェック */
  NARU_ASSERT((work_ptr - (uint8_t *)work) <= work_size);

  return filter;
}

/* SAフィルタのリセット */
void NARUSAFilter_Reset(struct NARUSAFilter *filter)
{
  NARU_ASSERT(filter != NULL);

  memset(filter->history, 0, sizeof(int32_t) * 2 * (size_t)filter->max_filter_order);
  memset(filter->weight, 0, sizeof(int32_t) * (size_t)filter->max_filter_order);

  filter->buffer_pos = 0;
  filter->buffer_pos_mask = 0;
}

/* SAフィルタの次数をセット */
void NARUSAFilter_SetFilterOrder(struct NARUSAFilter *filter, int32_t order)
{
  NARU_ASSERT(filter != NULL);

  /* 範囲チェック */
  NARU_ASSERT(order > 0);
  NARU_ASSERT(order <= filter->max_filter_order);

  /* フィルタ次数は2の冪数を要求（高速化目的） */
  NARU_ASSERT(NARUUTILITY_IS_POWERED_OF_2(order));

  filter->filter_order = order;
  filter->buffer_pos_mask = order - 1;
}

/* NGSAフィルタの作成に必要なワークサイズ計算 */
int32_t NARUNGSAFilter_CalculateWorkSize(uint8_t max_filter_order)
{
  int32_t work_size;

  /* 無効な次数 */
  if ((max_filter_order == 0)
      || !(NARUUTILITY_IS_POWERED_OF_2(max_filter_order))) {
    return -1;
  }

  /* 構造体本体分のサイズ（+アラインメント） */
  work_size = sizeof(struct NARUNGSAFilter) + NARU_MEMORY_ALIGNMENT;
  /* 入力データ履歴分のサイズ */
  work_size += sizeof(int32_t) * 2 * max_filter_order;
  /* フィルタ係数サイズ */
  work_size += sizeof(int32_t) * max_filter_order;
  /* AR係数サイズ */
  work_size += sizeof(int32_t) * NARU_MAX_ARORDER_FOR_FILTERORDER(max_filter_order);
  /* 自然勾配サイズ */
  work_size += sizeof(int32_t) * 2 * max_filter_order;

  return work_size;
}

/* NGSAフィルタの作成（自己割当不可） */
struct NARUNGSAFilter* NARUNGSAFilter_Create(uint8_t max_filter_order, void *work, int32_t work_size)
{
  uint8_t *work_ptr;
  struct NARUNGSAFilter *filter;

  /* 引数チェック */
  if ((work == NULL)
      || (work_size < NARUNGSAFilter_CalculateWorkSize(max_filter_order))) {
    return NULL;
  }

  /* 無効なフィルタ次数 */
  if ((max_filter_order == 0)
      || !(NARUUTILITY_IS_POWERED_OF_2(max_filter_order))) {
    return NULL;
  }

  /* 構造体配置 */
  work_ptr = (uint8_t *)NARUUTILITY_ROUNDUP((uintptr_t)work, NARU_MEMORY_ALIGNMENT);
  filter = (struct NARUNGSAFilter *)work_ptr;
  work_ptr += sizeof(struct NARUNGSAFilter);

  /* 最大フィルタ次数設定 */
  filter->max_filter_order = max_filter_order;

  /* 入力データ履歴配置 */
  filter->history = (int32_t *)work_ptr;
  work_ptr += sizeof(int32_t) * 2 * max_filter_order;

  /* フィルタ係数配置 */
  filter->weight = (int32_t *)work_ptr;
  work_ptr += sizeof(int32_t) * max_filter_order;

  /* AR係数配置 */
  filter->ar_coef = (int32_t *)work_ptr;
  work_ptr += sizeof(int32_t) * NARU_MAX_ARORDER_FOR_FILTERORDER(max_filter_order);

  /* 自然勾配配置 */
  filter->ngrad = (int32_t *)work_ptr;
  work_ptr += sizeof(int32_t) * 2 * max_filter_order;

  /* オーバーフローチェック */
  NARU_ASSERT((work_ptr - (uint8_t *)work) <= work_size);

  return filter;
}

/* NGSAフィルタのリセット */
void NARUNGSAFilter_Reset(struct NARUNGSAFilter *filter)
{
  NARU_ASSERT(filter != NULL);

  memset(filter->history, 0, sizeof(int32_t) * 2 * (size_t)filter->max_filter_order);
  memset(filter->weight, 0, sizeof(int32_t) * (size_t)filter->max_filter_order);
  memset(filter->ar_coef, 0, sizeof(int32_t) * (size_t)NARU_MAX_ARORDER_FOR_FILTERORDER(filter->max_filter_order));
  memset(filter->ngrad, 0, sizeof(int32_t) * 2 * (size_t)filter->max_filter_order);
  memset(filter->delta_table, 0, sizeof(int32_t) * 3);

  filter->pdelta_table = NULL; /* 不定領域を差すよりはNULLでクラッシュさせたい */

  filter->buffer_pos = 0;
  filter->buffer_pos_mask = 0;
}

/* NGSAフィルタの次数をセット */
void NARUNGSAFilter_SetFilterOrder(struct NARUNGSAFilter *filter, int32_t filter_order, int32_t ar_order)
{
  NARU_ASSERT(filter != NULL);

  /* 範囲チェック */
  NARU_ASSERT(filter_order > 0);
  NARU_ASSERT(ar_order > 0);
  NARU_ASSERT(filter_order <= filter->max_filter_order);
  NARU_ASSERT(ar_order <= NARU_MAX_ARORDER_FOR_FILTERORDER(filter->max_filter_order));

  /* フィルタ次数は2の冪数を要求（高速化目的） */
  NARU_ASSERT(NARUUTILITY_IS_POWERED_OF_2(filter_order));

  filter->ar_order = ar_order;
  filter->filter_order = filter_order;
  filter->buffer_pos_mask = filter_order - 1;

  filter->delta_rshift = NARUNGSA_STEPSIZE_SCALE_BITWIDTH + (int32_t)NARUUTILITY_LOG2CEIL((uint32_t)filter_order);
}

/* NGSAフィルタの自然勾配初期化 */
void NARUNGSAFilter_InitializeNaturalGradient(struct NARUNGSAFilter *filter)
{
  const int32_t filter_order = filter->filter_order;
  int32_t *ngrad, *history, stepsize_scale;

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
        = history[ord] - NARUFILTER_FIXEDPOINT_MUL(ar_coef[0], history[ord + 1], NARU_FIXEDPOINT_DIGITS);
    }
    for (ord = 1; ord < filter_order - 1; ord++) {
      ngrad[ord] 
        -= NARUFILTER_FIXEDPOINT_MUL(ar_coef[0], ngrad[ord - 1], NARU_FIXEDPOINT_DIGITS);
    }
    ngrad[filter_order - 1]
      = history[filter_order - 1] - NARUFILTER_FIXEDPOINT_MUL(ar_coef[0], history[filter_order - 2], NARU_FIXEDPOINT_DIGITS);
  } else {
    /* 1より大きい場合は履歴を初期値とする */
    memcpy(ngrad, history, sizeof(int32_t) * (uint32_t)filter_order);
  }

  /* バッファアクセス高速化のために、フィルタ次数分離れた場所にコピー */
  memcpy(&ngrad[filter->filter_order], ngrad, sizeof(int32_t) * (uint32_t)filter->filter_order);

  /* ステップサイズに乗じる係数の計算 */
  if (filter->ar_order == 1) {
    int32_t scale_int, ar_coef;
    /* (-1.0f, 1.0f)の範囲内に丸める: 1.0f, -1.0fだと2乗がオーバーフローするため */
    ar_coef = NARUUTILITY_INNER_VALUE(filter->ar_coef[0],
        -(1 << NARU_FIXEDPOINT_DIGITS) + 1, (1 << NARU_FIXEDPOINT_DIGITS) - 1);
    /* 1.0f / (1.0f - ar_coef[0] ** 2) */
    scale_int = (1 << NARU_FIXEDPOINT_DIGITS) - NARUFILTER_FIXEDPOINT_MUL(ar_coef, ar_coef, NARU_FIXEDPOINT_DIGITS);
    scale_int = (1 << (2 * NARU_FIXEDPOINT_DIGITS)) / scale_int;
    stepsize_scale = scale_int >> (NARU_FIXEDPOINT_DIGITS - NARUNGSA_STEPSIZE_SCALE_BITWIDTH);
    /* 係数が大きくなりすぎないようにクリップ */
    stepsize_scale = NARUUTILITY_MIN(NARUNGSA_MAX_STEPSIZE_SCALE, stepsize_scale);
  } else {
    stepsize_scale = 1 << NARUNGSA_STEPSIZE_SCALE_BITWIDTH;
  }

  /* 更新テーブルの値をセット */
  filter->delta_table[0] = -stepsize_scale;
  filter->delta_table[1] = 0;
  filter->delta_table[2] = stepsize_scale;

  /* 参照位置をずらした位置にポインタをセット(signの値で参照するため) */
  filter->pdelta_table = &filter->delta_table[1];
}

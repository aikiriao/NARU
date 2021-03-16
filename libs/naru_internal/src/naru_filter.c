#include "naru_filter.h"
#include "naru_internal.h"
#include "naru_utility.h"

#include <string.h>

/* SAフィルタのリセット */
void NARUSAFilter_Reset(struct NARUSAFilter *filter)
{
  NARU_ASSERT(filter != NULL);

  memset(filter, 0, sizeof(struct NARUSAFilter));
}

/* SAフィルタの次数をセット */
void NARUSAFilter_SetFilterOrder(struct NARUSAFilter *filter, int32_t order)
{
  NARU_ASSERT(filter != NULL);

  /* 範囲チェック */
  NARU_ASSERT(order > 0);
  NARU_ASSERT(order <= NARU_MAX_FILTER_ORDER);

  /* フィルタ次数は2の冪数を要求（高速化目的） */
  NARU_ASSERT(NARUUTILITY_IS_POWERED_OF_2(order));

  filter->filter_order = order;
  filter->buffer_pos_mask = order - 1;
}

/* NGSAフィルタのリセット */
void NARUNGSAFilter_Reset(struct NARUNGSAFilter *filter)
{
  NARU_ASSERT(filter != NULL);

  memset(filter, 0, sizeof(struct NARUNGSAFilter));
}

/* NGSAフィルタの次数をセット */
void NARUNGSAFilter_SetFilterOrder(struct NARUNGSAFilter *filter, int32_t filter_order, int32_t ar_order)
{
  NARU_ASSERT(filter != NULL);

  /* 範囲チェック */
  NARU_ASSERT(filter_order > 0);
  NARU_ASSERT(ar_order > 0);
  NARU_ASSERT(filter_order <= NARU_MAX_FILTER_ORDER);
  NARU_ASSERT(ar_order <= NARU_MAX_AR_ORDER);

  /* フィルタ次数は2の冪数を要求（高速化目的） */
  NARU_ASSERT(NARUUTILITY_IS_POWERED_OF_2(filter_order));

  filter->ar_order = ar_order;
  filter->filter_order = filter_order;
  filter->buffer_pos_mask = filter_order - 1;
}

/* NGSAフィルタの自然勾配初期化 */
void NARUNGSAFilter_InitializeNaturalGradient(struct NARUNGSAFilter *filter)
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

  /* バッファアクセス高速化のために、フィルタ次数分離れた場所にコピー */
  memcpy(&ngrad[filter->filter_order], ngrad, sizeof(int32_t) * (uint32_t)filter->filter_order);

  /* ステップサイズに乗じる係数の計算 */
  if (filter->ar_order == 1) {
    int32_t scale_int, ar_coef;
    /* (-1.0f, 1.0f)の範囲内に丸める: 1.0f, -1.0fだと2乗がオーバーフローするため */
    ar_coef = NARUUTILITY_INNER_VALUE(filter->ar_coef[0],
        -(1 << NARU_FIXEDPOINT_DIGITS) + 1, (1 << NARU_FIXEDPOINT_DIGITS) - 1);
    /* 1.0f / (1.0f - ar_coef[0] ** 2) */
    scale_int = (1 << 15) - NARU_FIXEDPOINT_MUL(ar_coef, ar_coef, NARU_FIXEDPOINT_DIGITS);
    scale_int = (1 << 30) / scale_int;
    filter->stepsize_scale = scale_int >> (NARU_FIXEDPOINT_DIGITS - NARUNGSA_STEPSIZE_SCALE_BITWIDTH);
    /* 係数が大きくなりすぎないようにクリップ */
    filter->stepsize_scale = NARUUTILITY_MIN(NARUNGSA_MAX_STEPSIZE_SCALE, filter->stepsize_scale);
  } else {
    filter->stepsize_scale = 1 << NARUNGSA_STEPSIZE_SCALE_BITWIDTH;
  }
}

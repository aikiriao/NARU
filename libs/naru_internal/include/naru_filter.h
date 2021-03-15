#ifndef NARU_FILTER_H_INCLUDED
#define NARU_FILTER_H_INCLUDED

#include "naru.h"
#include "naru_stdint.h"

/* NGSAフィルタ */
struct NARUNGSAFilter {
  int32_t filter_order;                       /* フィルタ次数 */
  int32_t ar_order;                           /* AR次数 */
  int32_t history[2 * NARU_MAX_FILTER_ORDER]; /* 入力データ履歴（高速化のために2倍確保） */
  int32_t weight[NARU_MAX_FILTER_ORDER];      /* フィルタ係数 */
  int32_t ar_coef[NARU_MAX_AR_ORDER];         /* AR係数 */
  int32_t ngrad[2 * NARU_MAX_FILTER_ORDER];   /* 自然勾配（高速化のために2倍確保） */
  int32_t stepsize_scale;                     /* ステップサイズに乗じる係数 */
  int32_t buffer_pos;                         /* バッファ参照位置 */
  int32_t buffer_pos_mask;                    /* バッファ参照位置補正のためのビットマスク */
};

/* SAフィルタ */
struct NARUSAFilter {
  int32_t filter_order;                       /* フィルタ次数 */
  int32_t history[2 * NARU_MAX_FILTER_ORDER]; /* 入力データ履歴（高速化のために2倍確保） */
  int32_t weight[NARU_MAX_FILTER_ORDER];      /* フィルタ係数 */
  int32_t buffer_pos;                         /* バッファ参照位置 */
  int32_t buffer_pos_mask;                    /* バッファ参照位置補正のためのビットマスク */
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* SAフィルタのリセット */
void NARUSAFilter_Reset(struct NARUSAFilter *filter);

/* SAフィルタの次数をセット */
void NARUSAFilter_SetFilterOrder(struct NARUSAFilter *filter, int32_t order);

/* NGSAフィルタのリセット */
void NARUNGSAFilter_Reset(struct NARUNGSAFilter *filter);

/* NGSAフィルタの次数をセット */
void NARUNGSAFilter_SetFilterOrder(struct NARUNGSAFilter *filter, int32_t filter_order, int32_t ar_order);

/* NGSAフィルタの自然勾配初期化 */
void NARUNGSAFilter_InitializeNaturalGradient(struct NARUNGSAFilter *filter);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NARU_FILTER_H_INCLUDED */

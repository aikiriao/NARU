#ifndef NARU_FILTER_H_INCLUDED
#define NARU_FILTER_H_INCLUDED

#include "naru.h"
#include "naru_stdint.h"

/* NGSAフィルタ */
struct NARUNGSAFilter {
    int32_t filter_order;         /* フィルタ次数 */
    int32_t max_filter_order;     /* 最大フィルタ次数 */
    int32_t ar_order;             /* AR次数 */
    int32_t *history;             /* 入力データ履歴（高速化のために2倍確保） */
    int32_t *weight;              /* フィルタ係数 */
    int32_t *ar_coef;             /* AR係数 */
    int32_t *ngrad;               /* 自然勾配（高速化のために2倍確保） */
    int32_t delta_table[3];       /* フィルタ係数の変更量キャッシュ */
    const int32_t *pdelta_table;  /* = &delta_table[1](signの値を使って更新するため, 参照位置をずらしたポインタを持つ */
    int32_t buffer_pos;           /* バッファ参照位置 */
    int32_t buffer_pos_mask;      /* バッファ参照位置補正のためのビットマスク */
    int32_t delta_rshift;         /* 係数更新時の右シフト量 */
};

/* SAフィルタ */
struct NARUSAFilter {
    int32_t filter_order;     /* フィルタ次数 */
    int32_t max_filter_order; /* 最大フィルタ次数 */
    int32_t *history;         /* 入力データ履歴（高速化のために2倍確保） */
    int32_t *weight;          /* フィルタ係数 */
    int32_t buffer_pos;       /* バッファ参照位置 */
    int32_t buffer_pos_mask;  /* バッファ参照位置補正のためのビットマスク */
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* SAフィルタの作成に必要なワークサイズ計算 */
int32_t NARUSAFilter_CalculateWorkSize(uint8_t max_filter_order);

/* SAフィルタの作成（自己割当不可） */
struct NARUSAFilter* NARUSAFilter_Create(uint8_t max_filter_order, void *work, int32_t work_size);

/* SAフィルタのリセット */
void NARUSAFilter_Reset(struct NARUSAFilter *filter);

/* SAフィルタの次数をセット */
void NARUSAFilter_SetFilterOrder(struct NARUSAFilter *filter, int32_t order);

/* NGSAフィルタの作成に必要なワークサイズ計算 */
int32_t NARUNGSAFilter_CalculateWorkSize(uint8_t max_filter_order);

/* NGSAフィルタの作成（自己割当不可） */
struct NARUNGSAFilter* NARUNGSAFilter_Create(uint8_t max_filter_order, void *work, int32_t work_size);

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

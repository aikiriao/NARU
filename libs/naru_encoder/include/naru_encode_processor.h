#ifndef NARU_ENCODE_PROCESSOR_H_INCLUDED
#define NARU_ENCODE_PROCESSOR_H_INCLUDED

#include "naru.h"
#include "naru_internal.h"
#include "naru_stdint.h"
#include "naru_filter.h"
#include "naru_bit_stream.h"
#include "lpc_calculator.h"

/* 1chあたりの信号処理を担うプロセッサハンドル */
struct NARUEncodeProcessor;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* プロセッサ作成に必要なワークサイズ計算 */
int32_t NARUEncodeProcessor_CalculateWorkSize(uint8_t max_filter_order);

/* プロセッサ作成 */
struct NARUEncodeProcessor* NARUEncodeProcessor_Create(uint8_t max_filter_order, void *work, int32_t work_size);

/* プロセッサ破棄 */
void NARUEncodeProcessor_Destroy(struct NARUEncodeProcessor *processor);

/* プロセッサのリセット */
void NARUEncodeProcessor_Reset(struct NARUEncodeProcessor *processor);

/* フィルタ次数の設定 */
void NARUEncodeProcessor_SetFilterOrder(
        struct NARUEncodeProcessor *processor, int32_t filter_order,
        int32_t ar_order, int32_t second_filter_order);

/* AR係数の計算とプロセッサへの設定 */
void NARUEncodeProcessor_CalculateARCoef(
        struct NARUEncodeProcessor *processor, struct LPCCalculator *lpcc,
        const double *input, uint32_t num_samples);

/* 現在のプロセッサの状態を出力（注意: 係数は丸め等の副作用を受ける） */
void NARUEncodeProcessor_PutFilterState(
        struct NARUEncodeProcessor *processor, struct NARUBitStream *stream);

/* 予測 */
void NARUEncodeProcessor_Predict(
        struct NARUEncodeProcessor *processor, int32_t *buffer, uint32_t num_samples);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NARU_ENCODE_PROCESSOR_H_INCLUDED */

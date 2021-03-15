#ifndef NARU_DECODE_PROCESSOR_H_INCLUDED
#define NARU_DECODE_PROCESSOR_H_INCLUDED

#include "naru_stdint.h"
#include "naru_internal.h"
#include "naru_filter.h"
#include "naru_bit_stream.h"

/* 1chあたりの信号処理を担うプロセッサハンドル */
struct NARUDecodeProcessor {
  /* SA Filter */
  struct NARUSAFilter sa;
  /* NGSA Filter */
  struct NARUNGSAFilter ngsa;
  /* De Emphasis */
  int32_t deemphasis_prev;
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* プロセッサのリセット */
void NARUDecodeProcessor_Reset(struct NARUDecodeProcessor *processor);

/* フィルタ次数の設定 */
void NARUDecodeProcessor_SetFilterOrder(
  struct NARUDecodeProcessor *processor, int32_t filter_order,
  int32_t ar_order, int32_t second_filter_order);

/* プロセッサの状態を取得 */
void NARUDecodeProcessor_GetFilterState(
    struct NARUDecodeProcessor *processor, struct NARUBitStream *stream);

/* 合成 */
void NARUDecodeProcessor_Synthesize(
    struct NARUDecodeProcessor *processor, int32_t *buffer, uint32_t num_samples);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NARU_DECODE_PROCESSOR_H_INCLUDED */

#ifndef NARU_DECODE_PROCESSOR_H_INCLUDED
#define NARU_DECODE_PROCESSOR_H_INCLUDED

#include "naru_stdint.h"
#include "naru_internal.h"
#include "naru_stdint.h"
#include "naru_bit_stream.h"

/* 1chあたりの信号処理を担うプロセッサハンドル */
struct NARUDecodeProcessor {
  /* SA Filter */
  struct SAFilter sa;
  /* NGSA Filter */
  struct NGSAFilter ngsa;
  /* De Emphasis */
  int32_t deemphasis_prev;
};

/* プロセッサのリセット */
void NARUDecodeProcessor_Reset(struct NARUDecodeProcessor *processor);

/* プロセッサの状態を取得 */
void NARUDecodeProcessor_GetFilterState(
    struct NARUDecodeProcessor *processor, struct NARUBitStream *stream);

/* 合成 */
void NARUDecodeProcessor_Synthesize(
    struct NARUDecodeProcessor *processor, int32_t *buffer, uint32_t num_samples);

#endif /* NARU_DECODE_PROCESSOR_H_INCLUDED */

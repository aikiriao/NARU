#ifndef NARU_DECODE_PROCESSOR_H_INCLUDED
#define NARU_DECODE_PROCESSOR_H_INCLUDED

#include "naru_stdint.h"

/* 1chあたりの信号処理を担うプロセッサハンドル */
struct NARUDecodeProcessor {
  int32_t *weight;  /* フィルタ係数 */
  int32_t *ar_coef; /* AR係数 */
  int32_t *grad;    /* 勾配 */
};

struct NARUDecodeProcessor* NARUDecodeProcessor_Create(uint8_t max_filter_order);

void NARUDecodeProcessor_Destroy(struct NARUDecodeProcessor *processor);

void NARUDecodeProcessor_Synthesize(
    struct NARUDecodeProcessor *processor,
    const int32_t *residual, uint32_t num_samples, int32_t *output);

#endif /* NARU_DECODE_PROCESSOR_H_INCLUDED */

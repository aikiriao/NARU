#ifndef NARU_ENCODE_PROCESSOR_H_INCLUDED
#define NARU_ENCODE_PROCESSOR_H_INCLUDED

#include "naru_stdint.h"

/* 1chあたりの信号処理を担うプロセッサハンドル */
struct NARUEncodeProcessor {
  int32_t *weight;  /* フィルタ係数 */
  int32_t *ar_coef; /* AR係数 */
  int32_t *grad;    /* 勾配 */
};

struct NARUEncodeProcessor* NARUEncodeProcessor_Create(uint8_t max_filter_order);

void NARUEncodeProcessor_Destroy(struct NARUEncodeProcessor *processor);

void NARUEncodeProcessor_Predict(
    struct NARUEncodeProcessor *processor,
    const int32_t *input, uint32_t num_samples, int32_t *residual);

#endif /* NARU_ENCODE_PROCESSOR_H_INCLUDED */

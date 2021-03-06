#ifndef NARU_ENCODE_PROCESSOR_H_INCLUDED
#define NARU_ENCODE_PROCESSOR_H_INCLUDED

#include "naru.h"
#include "naru_internal.h"
#include "naru_stdint.h"
#include "naru_bit_stream.h"
#include "lpc_calculator.h"

/* 1chあたりの信号処理を担うプロセッサハンドル */
struct NARUEncodeProcessor {
  /* Pre Emphasis */
  int32_t preemphasis_prev;
  /* NGSA Filter */
  struct NGSAFilter ngsa;
  /* SA Filter */
  struct SAFilter sa;
};

/* プロセッサのリセット */
void NARUEncodeProcessor_Reset(struct NARUEncodeProcessor *processor);

/* AR係数の計算とプロセッサへの設定 */
void NARUEncodeProcessor_CalculateARCoef(
    struct NARUEncodeProcessor *processor, struct LPCCalculator *lpcc,
    const double *input, uint32_t num_samples, int32_t ar_order);

/* 現在のプロセッサの状態を出力（注意: 係数は丸め等の副作用を受ける） */
void NARUEncodeProcessor_PutFilterState(
    struct NARUEncodeProcessor *processor, struct NARUBitStream *stream);

/* 予測 */
void NARUEncodeProcessor_Predict(
  struct NARUEncodeProcessor *processor, int32_t *buffer, uint32_t num_samples);

#endif /* NARU_ENCODE_PROCESSOR_H_INCLUDED */

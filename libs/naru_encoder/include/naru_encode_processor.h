#ifndef NARU_ENCODE_PROCESSOR_H_INCLUDED
#define NARU_ENCODE_PROCESSOR_H_INCLUDED

#include "naru.h"
#include "naru_stdint.h"
#include "naru_bit_stream.h"
#include "lpc_calculator.h"

struct NGSAFilter {
  int32_t filter_order;
  int32_t ar_order;
  int32_t history[NARU_MAX_FILTER_ORDER];   /* 入力データ履歴 */
  int32_t weight[NARU_MAX_FILTER_ORDER];    /* フィルタ係数 */
  int32_t ar_coef[NARU_MAX_AR_ORDER];       /* AR係数 */
  int32_t ngrad[NARU_MAX_FILTER_ORDER];     /* 自然勾配 */
  int32_t stepsize_scale;
};

struct SAFilter {
  int32_t filter_order;
  int32_t history[NARU_MAX_FILTER_ORDER];   /* 入力データ履歴 */
  int32_t weight[NARU_MAX_FILTER_ORDER];    /* フィルタ係数 */
};

/* 1chあたりの信号処理を担うプロセッサハンドル */
struct NARUEncodeProcessor {
  /* Pre Emphasis */
  int32_t preemphasis_prev;
  /* NGSA Filter */
  struct NGSAFilter ngsa;
  /* SA Filter */
  struct SAFilter sa;
};

void NARUEncodeProcessor_Reset(struct NARUEncodeProcessor *processor);

void NARUEncodeProcessor_CalculateARCoef(
    struct NARUEncodeProcessor *processor, struct LPCCalculator *lpcc,
    const double *input, uint32_t num_samples, int32_t ar_order);

void NARUEncodeProcessor_PutFilterState(
    struct NARUEncodeProcessor *processor, struct NARUBitStream *strm);

void NARUEncodeProcessor_PreEmphasis(
  struct NARUEncodeProcessor *processor, int32_t *buffer, uint32_t num_samples);

void NARUEncodeProcessor_Predict(
  struct NARUEncodeProcessor *processor, int32_t *buffer, uint32_t num_samples);

#endif /* NARU_ENCODE_PROCESSOR_H_INCLUDED */

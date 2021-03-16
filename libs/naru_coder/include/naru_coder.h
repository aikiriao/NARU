#ifndef NARU_CODER_H_INCLUDED
#define NARU_CODER_H_INCLUDED

#include "naru_stdint.h"
#include "naru_bit_stream.h"

/* 符号化ハンドル */
struct NARUCoder;

#ifdef __cplusplus
extern "C" {
#endif 

/* 符号化ハンドルの作成 */
struct NARUCoder* NARUCoder_Create(uint32_t max_num_channels, uint32_t max_num_parameters);

/* 符号化ハンドルの破棄 */
void NARUCoder_Destroy(struct NARUCoder *coder);

/* 初期パラメータの計算 */
void NARUCoder_CalculateInitialRecursiveRiceParameter(
  struct NARUCoder *coder, uint32_t num_parameters,
  const int32_t **data, uint32_t num_channels, uint32_t num_samples);

/* 再帰的ライス符号の初期パラメータを符号化 */
void NARUCoder_PutInitialRecursiveRiceParameter(
  struct NARUCoder *coder, struct NARUBitStream *stream, uint32_t num_parameters, uint32_t channel_index);

/* 再帰的ライス符号の初期パラメータを取得 */
void NARUCoder_GetInitialRecursiveRiceParameter(
  struct NARUCoder *coder, struct NARUBitStream *stream, uint32_t num_parameters, uint32_t channel_index);

/* 符号付き整数配列の符号化 */
void NARUCoder_PutDataArray(
  struct NARUCoder *coder, struct NARUBitStream *stream,
  uint32_t num_parameters, const int32_t **data, uint32_t num_channels, uint32_t num_samples);

/* 符号付き整数配列の復号 */
void NARUCoder_GetDataArray(
  struct NARUCoder *coder, struct NARUBitStream *stream,
  uint32_t num_parameters, int32_t **data, uint32_t num_channels, uint32_t num_samples);

#ifdef __cplusplus
}
#endif 

#endif /* NARU_CODER_H_INCLUDED */

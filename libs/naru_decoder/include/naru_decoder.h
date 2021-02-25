#ifndef NARU_DECODER_H_INCLUDED
#define NARU_DECODER_H_INCLUDED

#include "naru.h"
#include "naru_stdint.h"

/* デコーダハンドル */
struct NARUDecoder;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ヘッダデコード */
NARUApiResult NARUDecoder_DecodeHeader(
    const uint8_t *data, uint32_t data_size, struct NARUHeaderInfo *header);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NARU_DECODER_H_INCLUDED */

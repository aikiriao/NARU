#ifndef NARU_ENCODER_H_INCLUDED
#define NARU_ENCODER_H_INCLUDED

#include "naru.h"
#include "naru_stdint.h"

/* エンコーダハンドル */
struct NARUEncoder;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ヘッダエンコード */
NARUApiResult NARUEncoder_EncodeHeader(
    const struct NARUHeaderInfo *header_info, uint8_t *data, uint32_t data_size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NARU_ENCODER_H_INCLUDED */

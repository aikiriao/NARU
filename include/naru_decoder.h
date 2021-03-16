#ifndef NARU_DECODER_H_INCLUDED
#define NARU_DECODER_H_INCLUDED

#include "naru.h"
#include "naru_stdint.h"

/* デコーダコンフィグ */
struct NARUDecoderConfig {
	uint32_t max_num_channels; /* エンコード可能な最大チャンネル数 */
	uint8_t  max_filter_order; /* 最大フィルタ次数 */
};

/* デコーダハンドル */
struct NARUDecoder;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ヘッダデコード */
NARUApiResult NARUDecoder_DecodeHeader(
    const uint8_t *data, uint32_t data_size, struct NARUHeader *header);

/* デコーダハンドルの作成 */
struct NARUDecoder* NARUDecoder_Create(const struct NARUDecoderConfig *condig);

/* デコーダハンドルの破棄 */
void NARUDecoder_Destroy(struct NARUDecoder *decoder);

/* デコーダにヘッダをセット */
NARUApiResult NARUDecoder_SetHeader(
    struct NARUDecoder *decoder, const struct NARUHeader *header);

/* ヘッダを含めて全ブロックデコード */
NARUApiResult NARUDecoder_DecodeWhole(
    struct NARUDecoder *decoder,
    const uint8_t *data, uint32_t data_size,
    int32_t **buffer, uint32_t buffer_num_channels, uint32_t buffer_num_samples);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NARU_DECODER_H_INCLUDED */

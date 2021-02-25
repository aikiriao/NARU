#include "naru_decoder.h"
#include "naru_internal.h"
#include "naru_utility.h"
#include "byte_array.h"

#include <stdlib.h>

/* ヘッダデコード */
NARUApiResult NARUDecoder_DecodeHeader(
    const uint8_t *data, uint32_t data_size, struct NARUHeaderInfo *header)
{
  const uint8_t *data_pos;
  uint32_t u32buf;
  uint16_t u16buf;
  uint8_t  u8buf;
  struct NARUHeaderInfo tmp_header;

  /* 引数チェック */
  if ((data == NULL) || (header == NULL)) {
    return NARU_APIRESULT_INVALID_ARGUMENT;
  }

  /* データサイズが足りない */
  if (data_size < NARU_HEADER_SIZE) {
    return NARU_APIRESULT_INSUFFICIENT_DATA;
  }

  /* 読み出し用ポインタ設定 */
  data_pos = data;

  /* シグネチャ */
  {
    uint8_t buf[4];
    ByteArray_GetUint8(data_pos, &buf[0]);
    ByteArray_GetUint8(data_pos, &buf[1]);
    ByteArray_GetUint8(data_pos, &buf[2]);
    ByteArray_GetUint8(data_pos, &buf[3]);
    if ((buf[0] != 'N') || (buf[1] != 'A')
        || (buf[2] != 'R') || (buf[3] != '\0')) {
      return NARU_APIRESULT_INVALID_FORMAT;
    }
  }

  /* シグネチャ検査に通ったら、エラーを起こさずに読み切る */

  /* フォーマットバージョン */
  ByteArray_GetUint32BE(data_pos, &u32buf);
  tmp_header.format_version = u32buf;
  /* エンコーダバージョン */
  ByteArray_GetUint32BE(data_pos, &u32buf);
  tmp_header.encoder_version = u32buf;
  /* チャンネル数 */
  ByteArray_GetUint16BE(data_pos, &u16buf);
  tmp_header.num_channels = u16buf;
  /* サンプル数 */
  ByteArray_GetUint32BE(data_pos, &u32buf);
  tmp_header.num_samples = u32buf;
  /* サンプリングレート */
  ByteArray_GetUint32BE(data_pos, &u32buf);
  tmp_header.sampling_rate = u32buf;
  /* サンプルあたりビット数 */
  ByteArray_GetUint16BE(data_pos, &u16buf);
  tmp_header.bits_per_sample = u16buf;
  /* ブロックあたりサンプル数 */
  ByteArray_GetUint32BE(data_pos, &u32buf);
  tmp_header.num_samples_per_block = u32buf;
  /* フィルタ次数 */
  ByteArray_GetUint8(data_pos, &u8buf);
  tmp_header.filter_order = u8buf;
  /* AR次数 */
  ByteArray_GetUint8(data_pos, &u8buf);
  tmp_header.ar_order = u8buf;
  /* マルチチャンネル処理法 */
  ByteArray_GetUint8(data_pos, &u8buf);
  tmp_header.ch_process_method = (NARUChannelProcessMethod)u8buf;

  /* ヘッダサイズチェック */
  NARU_ASSERT((data_pos - data) == NARU_HEADER_SIZE);

  /* 成功終了 */
  (*header) = tmp_header;
  return NARU_APIRESULT_OK;
}

/* ヘッダのフォーマットチェック */
static NARUError NARUDecoder_CheckHeaderFormat(const struct NARUHeaderInfo *header)
{
  /* 内部モジュールなのでNULLが渡されたら落とす */
  NARU_ASSERT(header != NULL);

  /* フォーマットバージョン */
  /* 補足）今のところは不一致なら無条件でエラー */
  if (header->format_version != NARU_FORMAT_VERSION) {
    return NARU_ERROR_INVALID_FORMAT;
  }
  /* コーデックバージョン */
  /* 補足）今のところは不一致なら無条件でエラー */
  if (header->encoder_version != NARU_CODEC_VERSION) {
    return NARU_ERROR_INVALID_FORMAT;
  }
  /* チャンネル数 */
  if (header->num_channels == 0) {
    return NARU_ERROR_INVALID_FORMAT;
  }
  /* サンプル数 */
  if (header->num_samples == 0) {
    return NARU_ERROR_INVALID_FORMAT;
  }
  /* サンプリングレート */
  if (header->sampling_rate == 0) {
    return NARU_ERROR_INVALID_FORMAT;
  }
  /* ビット深度 */
  if (header->bits_per_sample == 0) {
    return NARU_ERROR_INVALID_FORMAT;
  }
  /* ブロックあたりサンプル数 */
  if (header->num_samples_per_block == 0) {
    return NARU_ERROR_INVALID_FORMAT;
  }
  /* フィルタ次数: 2の冪数限定 */
  if ((header->filter_order == 0)
      || !NARUUTILITY_IS_POWERED_OF_2(header->filter_order)) {
    return NARU_ERROR_INVALID_FORMAT;
  }
  /* AR次数: filter_order > 2 * ar_order を満たす必要がある */
  if ((header->ar_order == 0)
      || (header->filter_order <= (2 * header->ar_order))) {
    return NARU_ERROR_INVALID_FORMAT;
  }
  /* マルチチャンネル処理法 */
  if (header->ch_process_method >= NARU_CH_PROCESS_METHOD_INVALID) {
    return NARU_ERROR_INVALID_FORMAT;
  }
  /* モノラルではMS処理はできない */
  if ((header->ch_process_method == NARU_CH_PROCESS_METHOD_MS) 
      && (header->num_channels == 1)) {
    return NARU_ERROR_INVALID_FORMAT;
  }

  return NARU_ERROR_OK;
}

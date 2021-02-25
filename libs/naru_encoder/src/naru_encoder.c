#include "naru_encoder.h"
#include "naru_internal.h"
#include "naru_utility.h"
#include "byte_array.h"

#include <stdlib.h>

/* ヘッダエンコード */
NARUApiResult NARUEncoder_EncodeHeader(
    const struct NARUHeaderInfo *header, uint8_t *data, uint32_t data_size)
{
  uint8_t *data_pos;

  /* 引数チェック */
  if ((header == NULL) || (data == NULL)) {
    return NARU_APIRESULT_INVALID_ARGUMENT;
  }

  /* 出力先バッファサイズ不足 */
  if (data_size < NARU_HEADER_SIZE) {
    return NARU_APIRESULT_INSUFFICIENT_BUFFER;
  }

  /* ヘッダ異常値のチェック */
  /* データに書き出す（副作用）前にできる限りのチェックを行う */
  /* チャンネル数 */
  if (header->num_channels == 0) {
    return NARU_APIRESULT_INVALID_FORMAT;
  }
  /* サンプル数 */
  if (header->num_samples == 0) {
    return NARU_APIRESULT_INVALID_FORMAT;
  }
  /* サンプリングレート */
  if (header->sampling_rate == 0) {
    return NARU_APIRESULT_INVALID_FORMAT;
  }
  /* ビット深度 */
  if (header->bits_per_sample == 0) {
    return NARU_APIRESULT_INVALID_FORMAT;
  }
  /* ブロックあたりサンプル数 */
  if (header->num_samples_per_block == 0) {
    return NARU_APIRESULT_INVALID_FORMAT;
  }
  /* フィルタ次数: 2の冪数限定 */
  if ((header->filter_order == 0)
      || !NARUUTILITY_IS_POWERED_OF_2(header->filter_order)) {
    return NARU_APIRESULT_INVALID_FORMAT;
  }
  /* AR次数: filter_order > 2 * ar_order を満たす必要がある */
  if ((header->ar_order == 0)
      || (header->filter_order <= (2 * header->ar_order))) {
    return NARU_APIRESULT_INVALID_FORMAT;
  }
  /* マルチチャンネル処理法 */
  if (header->ch_process_method >= NARU_CH_PROCESS_METHOD_INVALID) {
    return NARU_APIRESULT_INVALID_FORMAT;
  }
  /* モノラルではMS処理はできない */
  if ((header->ch_process_method == NARU_CH_PROCESS_METHOD_MS) 
      && (header->num_channels == 1)) {
    return NARU_APIRESULT_INVALID_FORMAT;
  }

  /* 書き出し用ポインタ設定 */
  data_pos = data;

  /* シグネチャ */
  ByteArray_PutUint8(data_pos, 'N');
  ByteArray_PutUint8(data_pos, 'A');
  ByteArray_PutUint8(data_pos, 'R');
  ByteArray_PutUint8(data_pos, '\0');
  /* フォーマットバージョン
   * 補足）ヘッダの設定値は無視してマクロ値を書き込む */
  ByteArray_PutUint32BE(data_pos, NARU_FORMAT_VERSION);
  /* エンコーダバージョン
   * 補足）ヘッダの設定値は無視してマクロ値を書き込む */
  ByteArray_PutUint32BE(data_pos, NARU_ENCODER_VERSION);
  /* チャンネル数 */
  ByteArray_PutUint16BE(data_pos, header->num_channels);
  /* サンプル数 */
  ByteArray_PutUint32BE(data_pos, header->num_samples);
  /* サンプリングレート */
  ByteArray_PutUint32BE(data_pos, header->sampling_rate);
  /* サンプルあたりビット数 */
  ByteArray_PutUint16BE(data_pos, header->bits_per_sample);
  /* ブロックあたりサンプル数 */
  ByteArray_PutUint32BE(data_pos, header->num_samples_per_block);
  /* フィルタ次数 */
  ByteArray_PutUint8(data_pos, header->filter_order);
  /* AR次数 */
  ByteArray_PutUint8(data_pos, header->ar_order);
  /* マルチチャンネル処理法 */
  ByteArray_PutUint8(data_pos, header->ch_process_method);

  /* ヘッダサイズチェック */
  NARU_ASSERT((data_pos - data) == NARU_HEADER_SIZE);

  /* 成功終了 */
  return NARU_APIRESULT_OK;
}

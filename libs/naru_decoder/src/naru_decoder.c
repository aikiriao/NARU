#include "naru_decoder.h"
#include "naru_internal.h"
#include "naru_utility.h"
#include "byte_array.h"
#include "naru_bit_stream.h"
#include "naru_coder.h"
#include "naru_decode_processor.h"

#include <stdlib.h>

/* デコーダハンドル */
struct NARUDecoder {
  struct NARUHeaderInfo      header;
  struct NARUDecodeProcessor *processor;  /* 信号処理ハンドル */
  struct NARUCoder           *coder;      /* 符号化ハンドル */
  uint8_t                    set_header;  /* ヘッダセット済みか？ */
  struct NARUDecoderConfig   config;      /* 生成時コンフィグ */
};

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
        || (buf[2] != 'R') || (buf[3] != 'U')) {
      return NARU_APIRESULT_INVALID_FORMAT;
    }
  }

  /* シグネチャ検査に通ったら、エラーを起こさずに読み切る */

  /* フォーマットバージョン */
  ByteArray_GetUint32BE(data_pos, &u32buf);
  tmp_header.format_version = u32buf;
  /* エンコーダバージョン */
  ByteArray_GetUint32BE(data_pos, &u32buf);
  tmp_header.codec_version = u32buf;
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
  /* 2段目フィルタ次数 */
  ByteArray_GetUint8(data_pos, &u8buf);
  tmp_header.second_filter_order = u8buf;
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
  if (header->codec_version != NARU_CODEC_VERSION) {
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
  if (header->filter_order <= (2 * header->ar_order)) {
    return NARU_ERROR_INVALID_FORMAT;
  }
  /* 2段目フィルタ次数: 2の冪数限定 */
  if ((header->second_filter_order == 0)
      || !NARUUTILITY_IS_POWERED_OF_2(header->second_filter_order)) {
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

/* デコーダハンドル作成 */
struct NARUDecoder *NARUDecoder_Create(const struct NARUDecoderConfig *config)
{
  uint32_t ch;
  struct NARUDecoder *decoder;

  /* 引数チェック */
  if (config == NULL) {
    return NULL;
  }

  /* コンフィグチェック */
  if ((config->max_num_channels == 0)
      || (config->max_filter_order == 0)
      || !NARUUTILITY_IS_POWERED_OF_2(config->max_filter_order)) {
    return NULL;
  }

  /* デコーダの領域確保 */
  decoder = (struct NARUDecoder *)malloc(sizeof(struct NARUDecoder));

  /* 生成時コンフィグを保持 */
  decoder->config = (*config);

  /* 各種ハンドルの作成 */
  decoder->coder = NARUCoder_Create(config->max_num_channels, NARUCODER_NUM_RECURSIVERICE_PARAMETER);
  decoder->processor = (struct NARUDecodeProcessor *)malloc(sizeof(struct NARUDecodeProcessor) * config->max_num_channels);
  for (ch = 0; ch < config->max_num_channels; ch++) {
    NARUDecodeProcessor_Reset(&decoder->processor[ch]);
  }

  /* ヘッダは未セットに */
  decoder->set_header = 0;
  
  return decoder;
}

/* デコーダハンドルの破棄 */
void NARUDecoder_Destroy(struct NARUDecoder *decoder)
{
  if (decoder != NULL) {
    NARU_NULLCHECK_AND_FREE(decoder->processor);
    NARUCoder_Destroy(decoder->coder);
  }
}

/* デコーダにヘッダをセット */
NARUApiResult NARUDecoder_SetHeader(
    struct NARUDecoder *decoder, const struct NARUHeaderInfo *header)
{
  uint32_t ch;

  /* 引数チェック */
  if ((decoder == NULL) || (header == NULL)) {
    return NARU_APIRESULT_INVALID_ARGUMENT;
  }

  /* ヘッダの有効性チェック */
  if (NARUDecoder_CheckHeaderFormat(header) != NARU_ERROR_OK) {
    return NARU_APIRESULT_INVALID_FORMAT;
  }

  /* フィルタパラメータ設定 */
  for (ch = 0; ch < header->num_channels; ch++) {
    decoder->processor[ch].ngsa.filter_order = header->filter_order;
    decoder->processor[ch].ngsa.ar_order = header->ar_order;
    decoder->processor[ch].sa.filter_order = header->second_filter_order;
  }

  /* ヘッダセット */
  decoder->header = (*header);
  decoder->set_header = 1;

  return NARU_APIRESULT_OK;
}

/* 単一データブロックデコード */
static NARUApiResult NARUDecoder_DecodeBlock(
    struct NARUDecoder *decoder,
    const uint8_t *data, uint32_t data_size, 
    int32_t **buffer, uint32_t buffer_num_samples, 
    uint32_t *decode_size, uint32_t *num_decode_samples)
{
  uint32_t buf;
  uint32_t ch, tmp_num_decode_samples;
  const struct NARUHeaderInfo *header;
  struct NARUBitStream stream;

  /* 引数チェック */
  if ((decoder == NULL) || (data == NULL)
      || (buffer == NULL) || (decode_size == NULL)
      || (num_decode_samples == NULL)) {
    return NARU_APIRESULT_INVALID_ARGUMENT;
  }

  /* ヘッダがまだセットされていない */
  if (decoder->set_header != 1) {
    return NARU_APIRESULT_PARAMETER_NOT_SET;
  }

  /* ヘッダ取得 */
  header = &(decoder->header);

  /* デコードするサンプル数の確定 */
  tmp_num_decode_samples
    = NARUUTILITY_MIN(header->num_samples_per_block, buffer_num_samples);

  /* ビットリーダ作成 */
  NARUBitReader_Open(&stream, (uint8_t *)data, data_size);

  /* 同期コード */
  NARUBitReader_GetBits(&stream, &buf, 16);
  NARU_ASSERT(buf == NARU_BLOCK_SYNC_CODE); /* TODO: 将来的にはエラーで落とす */

  /* ブロックデータタイプ */
  NARUBitReader_GetBits(&stream, &buf, 2);
  NARU_ASSERT(buf == NARU_BLOCK_DATA_TYPE_COMPRESSDATA); /* TODO: 将来的には他も対応 */

  /* 信号処理ハンドルの状態取得 */
  for (ch = 0; ch < header->num_channels; ch++) {
    NARUDecodeProcessor_GetFilterState(&decoder->processor[ch], &stream);
  }
  
  /* 符号化初期パラメータ取得 */
  for (ch = 0; ch < header->num_channels; ch++) {
    NARUCoder_GetInitialRecursiveRiceParameter(decoder->coder, &stream,
        NARUCODER_NUM_RECURSIVERICE_PARAMETER, header->bits_per_sample, ch);
  }

  /* 残差復号 */
  NARUCoder_GetDataArray(decoder->coder, &stream, 
      NARUCODER_NUM_RECURSIVERICE_PARAMETER,
      buffer, header->num_channels, tmp_num_decode_samples);

  /* バイト境界に揃える */
  NARUBitStream_Flush(&stream);

  /* 読み出しサイズの取得 */
  NARUBitStream_Tell(&stream, (int32_t *)decode_size);

  /* チャンネル毎に合成処理 */
  for (ch = 0; ch < header->num_channels; ch++) {
    NARUDecodeProcessor_Synthesize(&decoder->processor[ch], buffer[ch], tmp_num_decode_samples);
  }

  /* MS -> LR */
  if (header->ch_process_method == NARU_CH_PROCESS_METHOD_MS) {
    /* チャンネル数チェック */
    if (header->num_channels < 2) {
      return NARU_APIRESULT_INVALID_FORMAT;
    }
    NARUUtility_MStoLRInt32(buffer, tmp_num_decode_samples);
  }

  /* デコードサンプル数をセット */
  (*num_decode_samples) = tmp_num_decode_samples;

  /* 成功終了 */
  return NARU_APIRESULT_OK;
}

/* ヘッダを含めて全ブロックデコード */
NARUApiResult NARUDecoder_DecodeWhole(
    struct NARUDecoder *decoder,
    const uint8_t *data, uint32_t data_size,
    int32_t **buffer, uint32_t buffer_num_channels, uint32_t buffer_num_samples)
{
  NARUApiResult ret;
  uint32_t progress, ch, read_offset, read_block_size, num_decode_samples;
  const uint8_t *read_pos;
  int32_t *buffer_ptr[NARU_MAX_NUM_CHANNELS];
  struct NARUHeaderInfo tmp_header;
  const struct NARUHeaderInfo *header;

  /* 引数チェック */
  if ((decoder == NULL) || (data == NULL) || (buffer == NULL)) {
    return NARU_APIRESULT_INVALID_ARGUMENT;
  }

  /* ヘッダデコードとデコーダへのセット */
  if ((ret = NARUDecoder_DecodeHeader(data, data_size, &tmp_header)) 
      != NARU_APIRESULT_OK) {
    return ret;
  }
  if ((ret = NARUDecoder_SetHeader(decoder, &tmp_header))
      != NARU_APIRESULT_OK) {
    return ret;
  }
  header = &(decoder->header);

  /* バッファサイズチェック */
  if ((buffer_num_channels < header->num_channels)
      || (buffer_num_samples < header->num_samples)) {
    return NARU_APIRESULT_INSUFFICIENT_BUFFER;
  }

  progress = 0;
  read_offset = NARU_HEADER_SIZE;
  read_pos = data + NARU_HEADER_SIZE;
  while ((progress < header->num_samples) && (read_offset < data_size)) {
    /* 出力データサイズが足らない */
    if (read_offset > data_size) {
      return NARU_APIRESULT_INSUFFICIENT_DATA;
    }
    /* サンプル書き出し位置のセット */
    for (ch = 0; ch < header->num_channels; ch++) {
      buffer_ptr[ch] = &buffer[ch][progress];
    }
    /* ブロックデコード */
    if ((ret = NARUDecoder_DecodeBlock(decoder,
          read_pos, data_size - read_offset,
          buffer_ptr, buffer_num_samples - progress, 
          &read_block_size, &num_decode_samples)) != NARU_APIRESULT_OK) {
      return ret;
    }
    /* 進捗更新 */
    read_pos    += read_block_size;
    read_offset += read_block_size;
    progress    += num_decode_samples;
    NARU_ASSERT(progress <= buffer_num_samples);
    NARU_ASSERT(read_offset <= data_size);
  }

  /* 成功終了 */
  return NARU_APIRESULT_OK;
}

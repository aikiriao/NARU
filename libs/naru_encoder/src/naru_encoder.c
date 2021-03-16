#include "naru_encoder.h"
#include "naru_internal.h"
#include "naru_utility.h"
#include "byte_array.h"
#include "lpc_calculator.h"
#include "naru_bit_stream.h"
#include "naru_coder.h"
#include "naru_encode_processor.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* エンコーダハンドル */
struct NARUEncoder {
  struct NARUHeaderInfo      header;      /* ヘッダ */
  struct LPCCalculator       *lpcc;       /* LPC計算ハンドル */
  struct NARUEncodeProcessor *processor;  /* 信号処理ハンドル */
  struct NARUCoder           *coder;      /* 符号化ハンドル */
  uint8_t set_parameter;                  /* パラメータセット済み？ */
  struct NARUEncoderConfig   config;      /* 生成時コンフィグ */
  double  *window;                        /* 窓 */
  int32_t **buffer;                       /* 信号バッファ */
  double  *buffer_double;                 /* 信号処理バッファ（浮動小数） */
};

/* エンコードパラメータをヘッダに変換 */
static NARUError NARUEncoder_ConvertParameterToHeader(
    const struct NARUEncodeParameter *parameter, uint32_t num_samples,
    struct NARUHeaderInfo *header);
/* 単一データブロックエンコード */
/* 補足）実装の簡略化のため、ストリーミングエンコードには対応しない。そのため非公開。 */
static NARUApiResult NARUEncoder_EncodeBlock(
    struct NARUEncoder *encoder,
    const int32_t *const *input, uint32_t num_samples, 
    uint8_t *data, uint32_t data_size, uint32_t *output_size);
/* ブロックデータタイプの判定 */
static NARUBlockDataType NARUEncoder_DecideBlockDataType(
    struct NARUEncoder *encoder, const int32_t *const *input, uint32_t num_samples);
/* 解析用のdouble信号作成 */
static void NARUEncoder_MakeAnalyzingSignal(
    const double *window, const int32_t *data_int, uint32_t num_samples,
    uint32_t bits_per_sample, double *data_double);

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
  if (header->filter_order <= (2 * header->ar_order)) {
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
  ByteArray_PutUint8(data_pos, 'U');
  /* フォーマットバージョン
   * 補足）ヘッダの設定値は無視してマクロ値を書き込む */
  ByteArray_PutUint32BE(data_pos, NARU_FORMAT_VERSION);
  /* コーデックバージョン
   * 補足）ヘッダの設定値は無視してマクロ値を書き込む */
  ByteArray_PutUint32BE(data_pos, NARU_CODEC_VERSION);
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
  /* 2段目フィルタ次数 */
  ByteArray_PutUint8(data_pos, header->second_filter_order);
  /* マルチチャンネル処理法 */
  ByteArray_PutUint8(data_pos, header->ch_process_method);

  /* ヘッダサイズチェック */
  NARU_ASSERT((data_pos - data) == NARU_HEADER_SIZE);

  /* 成功終了 */
  return NARU_APIRESULT_OK;
}

/* エンコードパラメータをヘッダに変換 */
static NARUError NARUEncoder_ConvertParameterToHeader(
    const struct NARUEncodeParameter *parameter, uint32_t num_samples,
    struct NARUHeaderInfo *header)
{
  struct NARUHeaderInfo tmp_header = { 0, };

  /* 引数チェック */
  if ((parameter == NULL) || (header == NULL)) {
    return NARU_ERROR_INVALID_ARGUMENT;
  }

  /* パラメータのチェック */
  if (parameter->num_channels == 0) {
    return NARU_ERROR_INVALID_FORMAT;
  }
  if (parameter->bits_per_sample == 0) {
    return NARU_ERROR_INVALID_FORMAT;
  }
  if (parameter->sampling_rate == 0) {
    return NARU_ERROR_INVALID_FORMAT;
  }
  if (parameter->num_samples_per_block == 0) {
    return NARU_ERROR_INVALID_FORMAT;
  }
  if ((parameter->filter_order == 0)
       || !NARUUTILITY_IS_POWERED_OF_2(parameter->filter_order)) {
    return NARU_ERROR_INVALID_FORMAT;
  }
  if (parameter->filter_order <= (2 * parameter->ar_order)) {
    return NARU_ERROR_INVALID_FORMAT;
  }
  if ((parameter->second_filter_order == 0)
       || !NARUUTILITY_IS_POWERED_OF_2(parameter->second_filter_order)) {
    return NARU_ERROR_INVALID_FORMAT;
  }
  if (parameter->ch_process_method >= NARU_CH_PROCESS_METHOD_INVALID) {
    return NARU_ERROR_INVALID_FORMAT;
  }

  /* 総サンプル数 */
  tmp_header.num_samples = num_samples;

  /* 対応するメンバをコピー */
  tmp_header.num_channels = parameter->num_channels;
  tmp_header.sampling_rate = parameter->sampling_rate;
  tmp_header.bits_per_sample = parameter->bits_per_sample;
  tmp_header.num_samples_per_block = parameter->num_samples_per_block;
  tmp_header.filter_order = parameter->filter_order;
  tmp_header.ar_order = parameter->ar_order;
  tmp_header.second_filter_order = parameter->second_filter_order;
  tmp_header.ch_process_method = parameter->ch_process_method;

  /* 成功終了 */
  (*header) = tmp_header;
  return NARU_ERROR_OK;
}

/* エンコーダハンドル作成 */
struct NARUEncoder *NARUEncoder_Create(const struct NARUEncoderConfig *config)
{
  uint32_t ch;
  struct NARUEncoder *encoder;

  /* 引数チェック */
  if (config == NULL) {
    return NULL;
  }

  /* コンフィグチェック */
  if ((config->max_num_channels == 0)
      || (config->max_num_samples_per_block == 0)
      || (config->max_filter_order == 0)
      || !NARUUTILITY_IS_POWERED_OF_2(config->max_filter_order)) {
    return NULL;
  }

  /* エンコーダの領域確保 */
  encoder = (struct NARUEncoder *)malloc(sizeof(struct NARUEncoder));

  /* 生成時コンフィグを保持 */
  encoder->config = (*config);

  /* 各種ハンドルの作成 */
  encoder->lpcc = LPCCalculator_Create(config->max_filter_order, NULL, 0);
  encoder->coder = NARUCoder_Create(config->max_num_channels, NARUCODER_NUM_RECURSIVERICE_PARAMETER);
  encoder->processor = (struct NARUEncodeProcessor *)malloc(sizeof(struct NARUEncodeProcessor) * config->max_num_channels);
  for (ch = 0; ch < config->max_num_channels; ch++) {
    NARUEncodeProcessor_Reset(&encoder->processor[ch]);
  }

  /* バッファ領域の確保 */
  encoder->window = (double *)malloc(sizeof(double) * config->max_num_samples_per_block);
  encoder->buffer = (int32_t **)malloc(sizeof(int32_t *) * config->max_num_channels);
  for (ch = 0; ch < config->max_num_channels; ch++) {
    encoder->buffer[ch] = (int32_t *)malloc(sizeof(int32_t) * config->max_num_samples_per_block);
  }
  encoder->buffer_double = (double *)malloc(sizeof(double) * config->max_num_samples_per_block);

  /* パラメータは未セットに */
  encoder->set_parameter = 0;
  
  return encoder;
}

/* エンコーダハンドルの破棄 */
void NARUEncoder_Destroy(struct NARUEncoder *encoder)
{
  if (encoder != NULL) {
    uint32_t ch;
    for (ch = 0; ch < encoder->config.max_num_channels; ch++) {
      NARU_NULLCHECK_AND_FREE(encoder->buffer[ch]);
    }
    NARU_NULLCHECK_AND_FREE(encoder->window);
    NARU_NULLCHECK_AND_FREE(encoder->buffer);
    NARU_NULLCHECK_AND_FREE(encoder->buffer_double);
    LPCCalculator_Destroy(encoder->lpcc);
    NARUCoder_Destroy(encoder->coder);
    NARU_NULLCHECK_AND_FREE(encoder->processor);
  }
}

/* エンコードパラメータの設定 */
NARUApiResult NARUEncoder_SetEncodeParameter(
    struct NARUEncoder *encoder, const struct NARUEncodeParameter *parameter)
{
  uint32_t ch;
  struct NARUHeaderInfo tmp_header;

  /* 引数チェック */
  if ((encoder == NULL) || (parameter == NULL)) {
    return NARU_APIRESULT_INVALID_ARGUMENT;
  }

  /* パラメータ設定がおかしくないか、ヘッダへの変換を通じて確認 */
  /* 総サンプル数はダミー値を入れる */
  if (NARUEncoder_ConvertParameterToHeader(parameter, 0, &tmp_header) != NARU_ERROR_OK) {
    return NARU_APIRESULT_INVALID_FORMAT;
  }

  /* ヘッダ設定 */
  encoder->header = tmp_header;

  /* フィルタパラメータ設定 */
  for (ch = 0; ch < tmp_header.num_channels; ch++) {
    NARUEncodeProcessor_SetFilterOrder(&encoder->processor[ch],
      tmp_header.filter_order, tmp_header.ar_order, tmp_header.second_filter_order);
  }

  /* パラメータ設定済みフラグを立てる */
  encoder->set_parameter = 1;
  
  return NARU_APIRESULT_OK;
}

/* ブロックデータタイプの判定 */
static NARUBlockDataType NARUEncoder_DecideBlockDataType(
    struct NARUEncoder *encoder, const int32_t *const *input, uint32_t num_samples)
{
  uint32_t ch, smpl;
  double parcor_coef[NARU_MAX_AR_ORDER + 1], mean_length;
  const struct NARUHeaderInfo *header;
  LPCCalculatorApiResult ret;

  NARU_ASSERT(encoder != NULL);
  NARU_ASSERT(input != NULL);

  header = &encoder->header;

  /* 平均符号長の計算 */
  mean_length = 0.0f;
  for (ch = 0; ch < header->num_channels; ch++) {
    double tmp_length;
    /* 入力をdouble化 */
    for (smpl = 0; smpl < num_samples; smpl++) {
      encoder->buffer_double[smpl] = input[ch][smpl] * pow(2.0f, -(int32_t)(header->bits_per_sample - 1));
    }
    /* 推定符号長計算 */
    ret = LPCCalculator_CalculatePARCORCoef(
        encoder->lpcc, encoder->buffer_double, num_samples, parcor_coef, header->ar_order);
    NARU_ASSERT(ret == LPCCALCULATOR_APIRESULT_OK);
    ret = LPCCalculator_EstimateCodeLength(
        encoder->buffer_double, num_samples, header->bits_per_sample, parcor_coef, header->ar_order, &tmp_length);
    NARU_ASSERT(ret == LPCCALCULATOR_APIRESULT_OK);
    mean_length += tmp_length;
  }
  mean_length /= header->num_channels;

  /* ビット幅に占める比に変換 */
  mean_length /= header->bits_per_sample;

  /* データタイプ判定 */

  /* 圧縮が効きにくい: 生データ出力 */
  if (mean_length >= NARU_ESTIMATED_CODELENGTH_THRESHOLD) {
    return NARU_BLOCK_DATA_TYPE_RAWDATA;
  }

  /* TODO: 無音判定 */

  /* それ以外は圧縮データ */
  return NARU_BLOCK_DATA_TYPE_COMPRESSDATA;
}

/* 解析用のdouble信号作成 */
static void NARUEncoder_MakeAnalyzingSignal(
    const double *window, const int32_t *data_int, uint32_t num_samples,
    uint32_t bits_per_sample, double *data_double)
{
  uint32_t smpl;

  NARU_ASSERT(window != NULL);
  NARU_ASSERT(data_int != NULL);
  NARU_ASSERT(data_double != NULL);
  NARU_ASSERT(bits_per_sample > 0);

  /* [-1, 1]の範囲に丸め込む */
  for (smpl = 0; smpl < num_samples; smpl++) {
    data_double[smpl] = (double)data_int[smpl] * pow(2.0f, -(double)(bits_per_sample - 1));
  }

  /* プリエンファシス */
  NARUUtility_PreEmphasisDouble(data_double, num_samples, NARU_EMPHASIS_FILTER_SHIFT);

  /* 窓適用 */
  NARUUtility_ApplyWindow(window, data_double, num_samples);
}

/* 生データブロックエンコード */
static NARUApiResult NARUEncoder_EncodeRawData(
    struct NARUEncoder *encoder,
    const int32_t *const *input, uint32_t num_samples, 
    uint8_t *data, uint32_t data_size, uint32_t *output_size)
{
  uint32_t ch, smpl;
  const struct NARUHeaderInfo *header;
  uint8_t *data_ptr;

  /* 内部関数なので不正な引数はアサートで落とす */
  NARU_ASSERT(encoder != NULL);
  NARU_ASSERT(input != NULL);
  NARU_ASSERT(num_samples > 0);
  NARU_ASSERT(data != NULL);
  NARU_ASSERT(data_size > 0);
  NARU_ASSERT(output_size != NULL);

  header = &(encoder->header);

  /* 書き込み先のバッファサイズチェック */
  if (data_size < (header->bits_per_sample * num_samples * header->num_channels) / 8) {
    return NARU_APIRESULT_INSUFFICIENT_BUFFER;
  }

  /* 生データをチャンネルインターリーブして出力 */
  data_ptr = data;
  for (smpl = 0; smpl < num_samples; smpl++) {
    for (ch = 0; ch < header->num_channels; ch++) {
      switch (header->bits_per_sample) {
        case  8:    ByteArray_PutUint8(data_ptr, NARUUTILITY_SINT32_TO_UINT32(input[ch][smpl])); break;
        case 16: ByteArray_PutUint16BE(data_ptr, NARUUTILITY_SINT32_TO_UINT32(input[ch][smpl])); break;
        case 24: ByteArray_PutUint24BE(data_ptr, NARUUTILITY_SINT32_TO_UINT32(input[ch][smpl])); break;
        default: NARU_ASSERT(0);
      }
      NARU_ASSERT((uint32_t)(data_ptr - data) < data_size);
    }
  }

  /* 書き込みサイズ取得 */
  (*output_size) = (uint32_t)(data_ptr - data);

  return NARU_APIRESULT_OK;
}

/* 圧縮データブロックエンコード */
static NARUApiResult NARUEncoder_EncodeCompressData(
    struct NARUEncoder *encoder,
    const int32_t *const *input, uint32_t num_samples, 
    uint8_t *data, uint32_t data_size, uint32_t *output_size)
{
  uint32_t ch;
  struct NARUBitStream stream;
  int32_t *buffer[NARU_MAX_NUM_CHANNELS];
  const struct NARUHeaderInfo *header;

  /* 内部関数なので不正な引数はアサートで落とす */
  NARU_ASSERT(encoder != NULL);
  NARU_ASSERT(input != NULL);
  NARU_ASSERT(num_samples > 0);
  NARU_ASSERT(data != NULL);
  NARU_ASSERT(data_size > 0);
  NARU_ASSERT(output_size != NULL);

  header = &(encoder->header);

  /* 入力をバッファにコピー */
  for (ch = 0; ch < header->num_channels; ch++) {
    /* ポインタ取得 */
    buffer[ch] = encoder->buffer[ch];
    memcpy(buffer[ch], input[ch], sizeof(int32_t) * num_samples);
  }

  /* マルチチャンネル処理 */
  if (header->ch_process_method == NARU_CH_PROCESS_METHOD_MS) {
    /* チャンネル数チェック */
    if (header->num_channels < 2) {
      return NARU_APIRESULT_INVALID_FORMAT;
    }
    /* LR -> MS */
    NARUUtility_LRtoMSInt32(buffer, num_samples);
  }

  /* 窓作成 */
  NARUUtility_MakeSinWindow(encoder->window, num_samples);

  /* チャンネル毎にパラメータ計算 */
  for (ch = 0; ch < header->num_channels; ch++) {
    /* 解析用double信号生成 */
    NARUEncoder_MakeAnalyzingSignal(encoder->window, 
        buffer[ch], num_samples, header->bits_per_sample, encoder->buffer_double);
    /* AR係数計算 */
    NARUEncodeProcessor_CalculateARCoef(&encoder->processor[ch],
        encoder->lpcc, encoder->buffer_double, num_samples);
  }

  /* ビットライタ作成 */
  NARUBitWriter_Open(&stream, data, data_size);

  /* 信号処理ハンドルの状態出力 */
  for (ch = 0; ch < header->num_channels; ch++) {
    NARUEncodeProcessor_PutFilterState(&encoder->processor[ch], &stream);
  }

  /* チャンネル毎に予測 */
  for (ch = 0; ch < header->num_channels; ch++) {
    NARUEncodeProcessor_Predict(&encoder->processor[ch], buffer[ch], num_samples);
  }

  /* 符号化初期パラメータ計算 */
  NARUCoder_CalculateInitialRecursiveRiceParameter(encoder->coder,
      NARUCODER_NUM_RECURSIVERICE_PARAMETER,
      (const int32_t **)buffer, header->num_channels, num_samples);

  /* 符号化パラメータ出力 */
  for (ch = 0; ch < header->num_channels; ch++) {
    NARUCoder_PutInitialRecursiveRiceParameter(encoder->coder,
        &stream, NARUCODER_NUM_RECURSIVERICE_PARAMETER, ch);
  }

  /* 残差符号化 */
  NARUCoder_PutDataArray(encoder->coder, &stream, 
      NARUCODER_NUM_RECURSIVERICE_PARAMETER,
      (const int32_t **)buffer, header->num_channels, num_samples);

  /* バイト境界に揃える */
  NARUBitStream_Flush(&stream);

  /* 書き込みサイズの取得 */
  NARUBitStream_Tell(&stream, (int32_t *)output_size);

  /* ビットライタ破棄 */
  NARUBitStream_Close(&stream);

  return NARU_APIRESULT_OK;
}

/* 単一データブロックエンコード */
static NARUApiResult NARUEncoder_EncodeBlock(
    struct NARUEncoder *encoder,
    const int32_t *const *input, uint32_t num_samples, 
    uint8_t *data, uint32_t data_size, uint32_t *output_size)
{
  uint8_t *data_ptr;
  const struct NARUHeaderInfo *header;
  NARUBlockDataType block_type;
  NARUApiResult ret;
  uint32_t block_header_size, block_data_size;

  /* 引数チェック */
  if ((encoder == NULL) || (input == NULL) || (num_samples == 0)
      || (data == NULL) || (data_size == 0) || (output_size == NULL)) {
    return NARU_APIRESULT_INVALID_ARGUMENT;
  }
  header = &(encoder->header);

  /* パラメータがセットされてない */
  if (encoder->set_parameter != 1) {
    return NARU_APIRESULT_PARAMETER_NOT_SET;
  }

  /* エンコードサンプル数チェック */
  if (num_samples > header->num_samples_per_block) {
    return NARU_APIRESULT_INSUFFICIENT_BUFFER;
  }

  /* 圧縮手法の判定 */
  block_type = NARUEncoder_DecideBlockDataType(encoder, input, num_samples);
  NARU_ASSERT(block_type != NARU_BLOCK_DATA_TYPE_INVALID);

  /* ブロックヘッダをエンコード */
  data_ptr = data;
  /* ブロック先頭の同期コード */
  ByteArray_PutUint16BE(data_ptr, NARU_BLOCK_SYNC_CODE);
  /* ブロックサイズ: 仮値で埋めておく */
  ByteArray_PutUint32BE(data_ptr, 0);
  /* ブロックCRC16: 仮値で埋めておく TODO: 処理追加 */
  ByteArray_PutUint16BE(data_ptr, 0);
  /* ブロックデータタイプ */
  ByteArray_PutUint8(data_ptr, block_type);
  /* ブロックヘッダサイズ */
  block_header_size = (uint32_t)(data_ptr - data);

  /* データ部のエンコード */
  /* 手法によりエンコードする関数を呼び分け */
  switch (block_type) {
    case NARU_BLOCK_DATA_TYPE_RAWDATA:
      ret = NARUEncoder_EncodeRawData(encoder, input, num_samples,
          data_ptr, data_size - block_header_size, &block_data_size);
      break;
    case NARU_BLOCK_DATA_TYPE_COMPRESSDATA:
      ret = NARUEncoder_EncodeCompressData(encoder, input, num_samples,
          data_ptr, data_size - block_header_size, &block_data_size);
      break;
    default:
      ret = NARU_APIRESULT_INVALID_FORMAT;
      break;
  }

  /* データデコードに失敗している */
  if (ret != NARU_APIRESULT_OK) {
    return ret;
  }

  /* ブロックサイズ書き込み: CRC16(2byte) + ブロックデータタイプ(1byte) */
  ByteArray_WriteUint32BE(&data[2], block_data_size + 3);

  /* 出力サイズ */
  (*output_size) = block_header_size + block_data_size;

  /* エンコード成功 */
  return NARU_APIRESULT_OK;
}

/* ヘッダ含めファイル全体をエンコード */
NARUApiResult NARUEncoder_EncodeWhole(
    struct NARUEncoder *encoder,
    const int32_t *const *input, uint32_t num_samples,
    uint8_t *data, uint32_t data_size, uint32_t *output_size)
{
  NARUApiResult ret;
  uint32_t progress, ch, write_size, write_offset, num_encode_samples;
  uint8_t *data_pos;
  const int32_t *input_ptr[NARU_MAX_NUM_CHANNELS];
  const struct NARUHeaderInfo *header;

  /* 引数チェック */
  if ((encoder == NULL) || (input == NULL)
      || (data == NULL) || (output_size == NULL)) {
    return NARU_APIRESULT_INVALID_ARGUMENT;
  }

  /* パラメータがセットされてない */
  if (encoder->set_parameter != 1) {
    return NARU_APIRESULT_PARAMETER_NOT_SET;
  }

  /* 書き出し位置を取得 */
  data_pos = data;

  /* ヘッダエンコード */
  encoder->header.num_samples = num_samples;
  if ((ret = NARUEncoder_EncodeHeader(&(encoder->header), data_pos, data_size))
      != NARU_APIRESULT_OK) {
    return ret;
  }
  header = &(encoder->header);

  /* 進捗状況初期化 */
  progress = 0;
  write_offset = NARU_HEADER_SIZE;
  data_pos = data + NARU_HEADER_SIZE;

  /* ブロックを時系列順にエンコード */
  while (progress < num_samples) {
    /* エンコードサンプル数の確定 */
    num_encode_samples
      = NARUUTILITY_MIN(header->num_samples_per_block, num_samples - progress);
    /* サンプル参照位置のセット */
    for (ch = 0; ch < header->num_channels; ch++) {
      input_ptr[ch] = &input[ch][progress];
    }

    /* ブロックエンコード */
    if ((ret = NARUEncoder_EncodeBlock(encoder,
            input_ptr, num_encode_samples,
            data_pos, data_size - write_offset, &write_size)) != NARU_APIRESULT_OK) {
      return ret;
    }

    /* 進捗更新 */
    data_pos      += write_size;
    write_offset  += write_size;
    progress      += num_encode_samples;
    NARU_ASSERT(write_offset <= data_size);
  }

  /* 成功終了 */
  (*output_size) = write_offset;
  return NARU_APIRESULT_OK;
}

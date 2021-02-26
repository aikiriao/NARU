#include "wav.h"
#include "naru_encoder.h"
#include "naru_decoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* エンコード 成功時は0、失敗時は0以外を返す */
static int do_encode(const char* in_filename, const char* out_filename)
{  
  FILE *out_fp;
  struct WAVFile *in_wav;
  struct NARUEncoder *encoder;
  struct NARUEncoderConfig config;
  struct NARUEncodeParameter parameter;
  struct stat fstat;
  int32_t *input[NARU_MAX_NUM_CHANNELS];
  uint8_t *buffer;
  uint32_t buffer_size, encoded_data_size;
  NARUApiResult ret;
  uint32_t ch, smpl, num_channels, num_samples;

  /* エンコーダ作成 */
  config.max_num_channels = NARU_MAX_NUM_CHANNELS;
  config.max_num_samples_per_block = 4096;
  config.max_filter_order = 32;
  if ((encoder = NARUEncoder_Create(&config)) == NULL) {
    fprintf(stderr, "Failed to create encoder handle. \n");
    return 1;
  }

  /* WAVファイルオープン */
  if ((in_wav = WAV_CreateFromFile(in_filename)) == NULL) {
    fprintf(stderr, "Failed to open %s. \n", in_filename);
    return 1;
  }
  num_channels = in_wav->format.num_channels;
  num_samples = in_wav->format.num_samples;

  /* エンコードパラメータセット */
  parameter.num_channels = (uint16_t)num_channels;
  parameter.bits_per_sample = (uint16_t)in_wav->format.bits_per_sample;
  parameter.sampling_rate = in_wav->format.sampling_rate;
  parameter.num_samples_per_block = 4096;
  parameter.filter_order = 8;
  parameter.ar_order = 1;
  parameter.ch_process_method
    = (num_channels >= 2) ? NARU_CH_PROCESS_METHOD_MS : NARU_CH_PROCESS_METHOD_NONE;
  if ((ret = NARUEncoder_SetEncodeParameter(encoder, &parameter)) != NARU_APIRESULT_OK) {
    fprintf(stderr, "Failed to set encode parameter: %d \n", ret);
    return 1;
  }

  /* 入力ファイルのサイズを拾っておく */
  stat(in_filename, &fstat);
  /* 入力wavの2倍よりは大きくならないだろうという想定 */
  buffer_size = (uint32_t)(2 * fstat.st_size);

  /* エンコードデータ/入力データ領域を作成 */
  buffer = (uint8_t *)malloc(buffer_size);
  for (ch = 0; ch < num_channels; ch++) {
    input[ch] = (int32_t *)malloc(sizeof(int32_t) * num_samples);
  }

  /* 情報が失われない程度に右シフト */
  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      input[ch][smpl] = (int32_t)(WAVFile_PCM(in_wav, smpl, ch) >> (32 - in_wav->format.bits_per_sample));
    }
  }

  /* 一括エンコード */
  if ((ret = NARUEncoder_EncodeWhole(encoder, 
        (const int32_t* const *)input, num_samples,
        buffer, buffer_size, &encoded_data_size)) != NARU_APIRESULT_OK) {
    fprintf(stderr, "Encoding error! %d \n", ret);
    return 1;
  }

  /* ファイル書き出し */
  out_fp = fopen(out_filename, "wb");
  if (fwrite(buffer, sizeof(uint8_t), encoded_data_size, out_fp) < encoded_data_size) {
    fprintf(stderr, "File output error! %d \n", ret);
    return 1;
  }

  /* リソース破棄 */
  fclose(out_fp);
  free(buffer);
  for (ch = 0; ch < num_channels; ch++) {
    free(input[ch]);
  }
  WAV_Destroy(in_wav);
  NARUEncoder_Destroy(encoder);

  return 0;
}

/* デコード 成功時は0、失敗時は0以外を返す */
static int do_decode(const char* in_filename, const char* out_filename)
{
  FILE* in_fp;
  struct WAVFile* out_wav;
  struct WAVFileFormat wav_format;
  struct stat fstat;
  struct NARUDecoder* decoder;
  struct NARUDecoderConfig config;
  struct NARUHeaderInfo header;
  uint8_t* buffer;
  uint32_t ch, smpl, buffer_size;
  NARUApiResult ret;

  /* デコーダハンドルの作成 */
  config.max_num_channels = NARU_MAX_NUM_CHANNELS;
  config.max_num_samples_per_block = 4096;
  config.max_filter_order = 32;
  if ((decoder = NARUDecoder_Create(&config)) == NULL) {
    fprintf(stderr, "Failed to create decoder handle. \n");
    return 1;
  }

  /* 入力ファイルオープン */
  in_fp = fopen(in_filename, "rb");
  /* 入力ファイルのサイズ取得 / バッファ領域割り当て */
  stat(in_filename, &fstat);
  buffer_size = (uint32_t)fstat.st_size;
  buffer = (uint8_t *)malloc(buffer_size);
  /* バッファ領域にデータをロード */
  fread(buffer, sizeof(uint8_t), buffer_size, in_fp);
  fclose(in_fp);

  /* ヘッダデコード */
  if ((ret = NARUDecoder_DecodeHeader(buffer, buffer_size, &header))
      != NARU_APIRESULT_OK) {
    fprintf(stderr, "Failed to get header information: %d \n", ret);
    return 1;
  }

  /* 出力wavハンドルの生成 */
  wav_format.data_format     = WAV_DATA_FORMAT_PCM;
  wav_format.num_channels    = header.num_channels;
  wav_format.sampling_rate   = header.sampling_rate;
  wav_format.bits_per_sample = header.bits_per_sample;
  wav_format.num_samples     = header.num_samples;
  if ((out_wav = WAV_Create(&wav_format)) == NULL) {
    fprintf(stderr, "Failed to create wav handle. \n");
    return 1;
  }

  /* ヘッダから読み取ったパラメータをデコーダにセット */
  if ((ret = NARUDecoder_SetHeader(decoder, &header)) != NARU_APIRESULT_OK) {
    fprintf(stderr, "Failed to set header: %d \n", ret);
    return 1;
  }

  /* 一括デコード */
  if ((ret = NARUDecoder_DecodeWhole(decoder, 
          buffer, buffer_size,
          (int32_t **)out_wav->data,
          out_wav->format.num_channels, out_wav->format.num_samples)
        != NARU_APIRESULT_OK)) {
    fprintf(stderr, "Decoding error! %d \n", ret);
    return 1;
  }

  /* エンコード時に右シフトした分を戻し、32bit化 */
  for (ch = 0; ch < out_wav->format.num_channels; ch++) {
    for (smpl = 0; smpl < out_wav->format.num_samples; smpl++) {
      WAVFile_PCM(out_wav, smpl, ch) <<= (32 - out_wav->format.bits_per_sample);
    }
  }

  /* WAVファイル書き出し */
  if (WAV_WriteToFile(out_filename, out_wav) != WAV_APIRESULT_OK) {
    fprintf(stderr, "Failed to write wav file. \n");
    return 1;
  }

  free(buffer);
  WAV_Destroy(out_wav);
  NARUDecoder_Destroy(decoder);

  return 0;
}

/* 使用法の表示 */
static void print_usage(char** argv)
{
  printf("NARU -- Natural-gradient AutoRegressive Unlossy Audio Compressor \n");
  printf("Usage: %s -[ed] INPUT_FILE_NAME OUTPUT_FILE_NAME \n", argv[0]);
}

/* メインエントリ */
int main(int argc, char** argv)
{
  const char* option;
  const char* input_file;
  const char* output_file;

  /* 引数が足らない */
  if (argc < 4) {
    print_usage(argv);
    return 1;
  }

  /* 引数文字列の取得 */
  option      = argv[1];
  input_file  = argv[2];
  output_file = argv[3];

  /* エンコード/デコード呼び分け */
  if (strcmp(option, "-e") == 0) {
    if (do_encode(input_file, output_file) != 0) {
      fprintf(stderr, "Failed to encode. \n");
      return 1;
    }
  } else if (strcmp(option, "-d") == 0) {
    if (do_decode(input_file, output_file) != 0) {
      fprintf(stderr, "Failed to decode. \n");
      return 1;
    }
  } else {
    print_usage(argv);
    return 1;
  }

  return 0;
}

#include "naru_encoder.h"
#include "naru_decoder.h"
#include "wav.h"
#include "command_line_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* コマンドライン仕様 */
static struct CommandLineParserSpecification command_line_spec[] = {
  { 'e', "encode", COMMAND_LINE_PARSER_FALSE, 
    "Encode mode", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 'd', "decode", COMMAND_LINE_PARSER_FALSE, 
    "Decode mode", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 'm', "mode", COMMAND_LINE_PARSER_TRUE, 
    "Specify compress mode: 0(fast decode), ..., 4(high compression) default:2", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 'c', "crc-check", COMMAND_LINE_PARSER_TRUE, 
    "Whether to check CRC16 at decoding(yes or no) default:yes", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 'h', "help", COMMAND_LINE_PARSER_FALSE, 
    "Show command help message", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 'v', "version", COMMAND_LINE_PARSER_FALSE, 
    "Show version information", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 0, }
};

/* エンコードプリセット */
static const struct {
  uint16_t num_samples_per_block; /* ブロックあたりサンプル数 */
  uint8_t filter_order; /* フィルタ次数 */
  uint8_t ar_order; /* AR次数 */
  uint8_t second_filter_order; /* 2段目フィルタ次数 */
  NARUChannelProcessMethod ch_process_method; /* マルチチャンネル処理法 */
  uint8_t num_encode_trials; /* エンコード繰り返し回数 */
} encode_preset[] = {
  {  8 * 1024,  4, 1, 4, NARU_CH_PROCESS_METHOD_MS, 1 }, /* プリセット0 */
  { 16 * 1024,  8, 1, 8, NARU_CH_PROCESS_METHOD_MS, 2 }, /* プリセット1 */
  { 16 * 1024, 16, 1, 8, NARU_CH_PROCESS_METHOD_MS, 2 }, /* プリセット2 */
  { 32 * 1024, 32, 1, 8, NARU_CH_PROCESS_METHOD_MS, 3 }, /* プリセット3 */
  { 48 * 1024, 64, 1, 8, NARU_CH_PROCESS_METHOD_MS, 4 }  /* プリセット4 */
};

/* エンコードプリセット数 */
static const uint32_t num_encode_preset = sizeof(encode_preset) / sizeof(encode_preset[0]);

/* デフォルトのプリセット番号 */
static const uint32_t default_preset_no = 2;

/* エンコード 成功時は0、失敗時は0以外を返す */
static int do_encode(const char* in_filename, const char* out_filename, uint32_t encode_preset_no)
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
  config.max_num_samples_per_block = 60 * 1024;
  config.max_filter_order = NARU_MAX_FILTER_ORDER;
  if ((encoder = NARUEncoder_Create(&config, NULL, 0)) == NULL) {
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

  /* 今のところは16bit/sampleのみ対応 */
  if (in_wav->format.bits_per_sample != 16) {
    fprintf(stderr, "This codec supports only 16-bit/sample wav. (input file:%d bit/sample) \n", in_wav->format.bits_per_sample);
    return 1;
  }

  /* エンコードパラメータセット */
  parameter.num_channels = (uint16_t)num_channels;
  parameter.bits_per_sample = (uint16_t)in_wav->format.bits_per_sample;
  parameter.sampling_rate = in_wav->format.sampling_rate;
  /* プリセットの反映 */
  parameter.num_samples_per_block = encode_preset[encode_preset_no].num_samples_per_block;
  parameter.filter_order = encode_preset[encode_preset_no].filter_order;
  parameter.ar_order = encode_preset[encode_preset_no].ar_order;
  parameter.second_filter_order = encode_preset[encode_preset_no].second_filter_order;
  parameter.ch_process_method = encode_preset[encode_preset_no].ch_process_method;
  parameter.num_encode_trials = encode_preset[encode_preset_no].num_encode_trials;
  /* 2ch未満の信号にはMS処理できないので無効に */
  if (num_channels < 2) {
    parameter.ch_process_method = NARU_CH_PROCESS_METHOD_NONE;
  }
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
        (const int32_t* const *)input, num_samples, buffer, buffer_size, &encoded_data_size)) != NARU_APIRESULT_OK) {
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
static int do_decode(const char* in_filename, const char* out_filename, uint8_t check_crc)
{
  FILE* in_fp;
  struct WAVFile* out_wav;
  struct WAVFileFormat wav_format;
  struct stat fstat;
  struct NARUDecoder* decoder;
  struct NARUDecoderConfig config;
  struct NARUHeader header;
  uint8_t* buffer;
  uint32_t ch, smpl, buffer_size;
  NARUApiResult ret;

  /* デコーダハンドルの作成 */
  config.max_num_channels = NARU_MAX_NUM_CHANNELS;
  config.max_filter_order = NARU_MAX_FILTER_ORDER;
  config.check_crc        = check_crc;
  if ((decoder = NARUDecoder_Create(&config, NULL, 0)) == NULL) {
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

  /* 一括デコード */
  if ((ret = NARUDecoder_DecodeWhole(decoder, 
          buffer, buffer_size,
          (int32_t **)out_wav->data, out_wav->format.num_channels, out_wav->format.num_samples)
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
  printf("Usage: %s [options] INPUT_FILE_NAME OUTPUT_FILE_NAME \n", argv[0]);
}

/* バージョン情報の表示 */
static void print_version_info(void)
{
  printf("NARU -- Natural-gradient AutoRegressive Unlossy Audio Compressor Version.%d \n", NARU_CODEC_VERSION);
}

/* メインエントリ */
int main(int argc, char** argv)
{
  const char* filename_ptr[2] = { NULL, NULL };
  const char* input_file;
  const char* output_file;

  /* 引数が足らない */
  if (argc == 1) {
    print_usage(argv);
    /* 初めて使った人が詰まらないようにヘルプの表示を促す */
    printf("Type `%s -h` to display command helps. \n", argv[0]);
    return 1;
  }

  /* コマンドライン解析 */
  if (CommandLineParser_ParseArguments(command_line_spec,
        argc, argv, filename_ptr, sizeof(filename_ptr) / sizeof(filename_ptr[0]))
      != COMMAND_LINE_PARSER_RESULT_OK) {
    return 1;
  }

  /* ヘルプやバージョン情報の表示判定 */
  if (CommandLineParser_GetOptionAcquired(command_line_spec, "help") == COMMAND_LINE_PARSER_TRUE) {
    print_usage(argv);
    printf("options: \n");
    CommandLineParser_PrintDescription(command_line_spec);
    return 0;
  } else if (CommandLineParser_GetOptionAcquired(command_line_spec, "version") == COMMAND_LINE_PARSER_TRUE) {
    print_version_info();
    return 0;
  }

  /* 入力ファイル名の取得 */
  if ((input_file = filename_ptr[0]) == NULL) {
    fprintf(stderr, "%s: input file must be specified. \n", argv[0]);
    return 1;
  }
  
  /* 出力ファイル名の取得 */
  if ((output_file = filename_ptr[1]) == NULL) {
    fprintf(stderr, "%s: output file must be specified. \n", argv[0]);
    return 1;
  }

  /* エンコードとデコードは同時に指定できない */
  if ((CommandLineParser_GetOptionAcquired(command_line_spec, "decode") == COMMAND_LINE_PARSER_TRUE)
      && (CommandLineParser_GetOptionAcquired(command_line_spec, "encode") == COMMAND_LINE_PARSER_TRUE)) {
      fprintf(stderr, "%s: encode and decode mode cannot specify simultaneously. \n", argv[0]);
      return 1;
  }

  if (CommandLineParser_GetOptionAcquired(command_line_spec, "decode") == COMMAND_LINE_PARSER_TRUE) {
    /* デコード */
    uint8_t crc_check = 1;
    /* CRC有効フラグを取得 */
    if (CommandLineParser_GetOptionAcquired(command_line_spec, "crc-check") == COMMAND_LINE_PARSER_TRUE) {
      const char* crc_check_arg
        = CommandLineParser_GetArgumentString(command_line_spec, "crc-check");
      crc_check = (strcmp(crc_check_arg, "yes") == 0) ? 1 : 0;
    }
    /* 一括デコード実行 */
    if (do_decode(input_file, output_file, crc_check) != 0) {
      fprintf(stderr, "%s: failed to decode %s. \n", argv[0], input_file);
      return 1;
    }
  } else if (CommandLineParser_GetOptionAcquired(command_line_spec, "encode") == COMMAND_LINE_PARSER_TRUE) {
    /* エンコード */
    uint32_t encode_preset_no = default_preset_no;
    /* エンコードプリセット番号取得 */
    if (CommandLineParser_GetOptionAcquired(command_line_spec, "mode") == COMMAND_LINE_PARSER_TRUE) {
      encode_preset_no = (uint32_t)strtol(CommandLineParser_GetArgumentString(command_line_spec, "mode"), NULL, 10);
      if (encode_preset_no >= num_encode_preset) {
        fprintf(stderr, "%s: encode preset number is out of range. \n", argv[0]);
        return 1;
      }
    }
    /* 一括エンコード実行 */
    if (do_encode(input_file, output_file, encode_preset_no) != 0) {
      fprintf(stderr, "%s: failed to encode %s. \n", argv[0], input_file);
      return 1;
    }
  } else {
    fprintf(stderr, "%s: decode(-d) or encode(-e) option must be specified. \n", argv[0]);
    return 1;
  }

  return 0;
}

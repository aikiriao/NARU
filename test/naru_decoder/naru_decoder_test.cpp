#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

/* エンコーダを使用 */
#include "naru_encoder.h"

/* テスト対象のモジュール */
extern "C" {
#include "../../libs/naru_decoder/src/naru_decoder.c"
}

/* 有効なヘッダをセット */
#define NARU_SetValidHeader(p_header)\
  do {\
    struct NARUHeader *header__p     = p_header;\
    header__p->format_version        = NARU_FORMAT_VERSION;\
    header__p->codec_version         = NARU_CODEC_VERSION;\
    header__p->num_channels          = 1;\
    header__p->sampling_rate         = 44100;\
    header__p->bits_per_sample       = 16;\
    header__p->num_samples           = 1024;\
    header__p->num_samples_per_block = 32;\
    header__p->filter_order          = 8;\
    header__p->ar_order              = 1;\
    header__p->second_filter_order   = 4;\
    header__p->ch_process_method     = NARU_CH_PROCESS_METHOD_NONE;\
  } while (0);

/* ヘッダにある情報からエンコードパラメータを作成 */
#define NARUEncoder_ConvertHeaderToParameter(p_header, p_parameter)\
  do {\
    const struct NARUHeader *header__p = p_header;\
    struct NARUEncodeParameter *param__p = p_parameter;\
    param__p->num_channels = header__p->num_channels;\
    param__p->sampling_rate = header__p->sampling_rate;\
    param__p->bits_per_sample = header__p->bits_per_sample;\
    param__p->num_samples_per_block = header__p->num_samples_per_block;\
    param__p->filter_order = header__p->filter_order;\
    param__p->ar_order = header__p->ar_order;\
    param__p->second_filter_order = header__p->second_filter_order;\
    param__p->ch_process_method = header__p->ch_process_method;\
  } while (0);

/* 有効なエンコーダコンフィグをセット */
#define NARUEncoder_SetValidConfig(p_config)\
  do {\
    struct NARUEncoderConfig *config__p = p_config;\
    config__p->max_num_channels           = 8;\
    config__p->max_num_samples_per_block  = 8192;\
    config__p->max_filter_order           = 32;\
  } while (0);

/* 有効なデコーダコンフィグをセット */
#define NARUDecoder_SetValidConfig(p_config)\
  do {\
    struct NARUDecoderConfig *config__p = p_config;\
    config__p->max_num_channels = 8;\
    config__p->max_filter_order = 32;\
  } while (0);

/* ヘッダデコードテスト */
TEST(NARUDecoderTest, DecodeHeaderTest)
{
  /* 成功例 */
  {
    uint8_t data[NARU_HEADER_SIZE] = { 0, };
    struct NARUHeader header, tmp_header;

    NARU_SetValidHeader(&header);

    /* エンコード->デコード */
    EXPECT_EQ(NARU_APIRESULT_OK, NARUEncoder_EncodeHeader(&header, data, sizeof(data)));
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &tmp_header));

    /* デコードしたヘッダの一致確認 */
    EXPECT_EQ(NARU_FORMAT_VERSION, tmp_header.format_version);
    EXPECT_EQ(NARU_CODEC_VERSION, tmp_header.codec_version);
    EXPECT_EQ(header.num_channels, tmp_header.num_channels);
    EXPECT_EQ(header.sampling_rate, tmp_header.sampling_rate);
    EXPECT_EQ(header.bits_per_sample, tmp_header.bits_per_sample);
    EXPECT_EQ(header.num_samples, tmp_header.num_samples);
    EXPECT_EQ(header.num_samples_per_block, tmp_header.num_samples_per_block);
    EXPECT_EQ(header.filter_order, tmp_header.filter_order);
    EXPECT_EQ(header.ar_order, tmp_header.ar_order);
    EXPECT_EQ(header.ch_process_method, tmp_header.ch_process_method);
  }

  /* ヘッダデコード失敗ケース */
  {
    struct NARUHeader header, getheader;
    uint8_t valid_data[NARU_HEADER_SIZE] = { 0, };
    uint8_t data[NARU_HEADER_SIZE];
    
    /* 有効な内容を作っておく */
    NARU_SetValidHeader(&header);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUEncoder_EncodeHeader(&header, valid_data, sizeof(valid_data)));
    /* 有効であることを確認 */
    ASSERT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(valid_data, sizeof(valid_data), &getheader));

    /* シグネチャが不正 */
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint8(&data[0], 'a');
    EXPECT_EQ(NARU_APIRESULT_INVALID_FORMAT, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    
    /* 以降のテストケースでは、ヘッダは取得できるが、内部のチェック関数で失敗する */

    /* 異常なフォーマットバージョン */
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint32BE(&data[4], 0);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint32BE(&data[4], NARU_FORMAT_VERSION + 1);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));

    /* 異常なエンコーダバージョン */
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint32BE(&data[8], 0);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint32BE(&data[8], NARU_CODEC_VERSION + 1);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));

    /* 異常なチャンネル数 */
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint16BE(&data[12], 0);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));

    /* 異常なサンプル数 */
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint32BE(&data[14], 0);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));

    /* 異常なサンプリングレート */
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint32BE(&data[18], 0);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));

    /* 異常なサンプルあたりビット数 */
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint16BE(&data[22], 0);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));

    /* 異常なブロックあたりサンプル数 */
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint32BE(&data[24], 0);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));

    /* 異常なフィルタ次数 */
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint8(&data[28], 0);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint8(&data[28], 3);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));

    /* 異常なAR次数 */
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint8(&data[29], header.filter_order / 2);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint8(&data[29], header.filter_order);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));

    /* 異常な2段目フィルタ次数 */
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint8(&data[30], 0);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint8(&data[30], 3);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));

    /* 異常なチャンネル処理法 */
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint8(&data[31], NARU_CH_PROCESS_METHOD_INVALID);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));

    /* チャンネル処理法とチャンネル数指定の組み合わせがおかしい */
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint16BE(&data[12], 1);
    ByteArray_WriteUint8(&data[31], NARU_CH_PROCESS_METHOD_MS);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));
  }
}

/* 1ブロックデコードテスト */
TEST(NARUDecoderTest, DecodeBlockTest)
{

  /* ヘッダ設定前にデコードしてエラー */
  {
    struct NARUDecoder *decoder;
    struct NARUDecoderConfig config;
    struct NARUHeader header;
    uint8_t *data;
    int32_t *output[NARU_MAX_NUM_CHANNELS];
    uint32_t ch, sufficient_size, output_size, out_num_samples;

    NARU_SetValidHeader(&header);
    NARUDecoder_SetValidConfig(&config);

    /* 十分なデータサイズ */
    sufficient_size = (2 * header.num_channels * header.num_samples_per_block * header.bits_per_sample) / 8;

    /* データ領域確保 */
    data = (uint8_t *)malloc(sufficient_size);
    for (ch = 0; ch < header.num_channels; ch++) {
      output[ch] = (int32_t *)malloc(sizeof(int32_t) * header.num_samples_per_block);
    }

    /* デコーダ作成 */
    decoder = NARUDecoder_Create(&config);
    ASSERT_TRUE(decoder != NULL);

    /* ヘッダセット前にデコーダしようとする */
    EXPECT_EQ(NARU_APIRESULT_PARAMETER_NOT_SET,
      NARUDecoder_DecodeBlock(decoder, data, sufficient_size, output, header.num_samples_per_block, &output_size, &out_num_samples));

    /* ヘッダをセット */
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_SetHeader(decoder, &header));

    /* 領域の開放 */
    for (ch = 0; ch < header.num_channels; ch++) {
      free(output[ch]);
    }
    free(data);
    NARUDecoder_Destroy(decoder);
  }

  /* 無音データをエンコードデコードしてみる */
  {
    struct NARUEncoder *encoder;
    struct NARUDecoder *decoder;
    struct NARUEncoderConfig encoder_config;
    struct NARUDecoderConfig decoder_config;
    struct NARUEncodeParameter parameter;
    struct NARUHeader header, tmp_header;
    uint8_t *data;
    int32_t *input[NARU_MAX_NUM_CHANNELS];
    int32_t *output[NARU_MAX_NUM_CHANNELS];
    uint32_t ch, sufficient_size, output_size, decode_output_size, out_num_samples;

    NARU_SetValidHeader(&header);
    NARUEncoder_SetValidConfig(&encoder_config);
    NARUDecoder_SetValidConfig(&decoder_config);

    /* 十分なデータサイズ */
    sufficient_size = (2 * header.num_channels * header.num_samples_per_block * header.bits_per_sample) / 8;

    /* データ領域確保 */
    data = (uint8_t *)malloc(sufficient_size);
    for (ch = 0; ch < header.num_channels; ch++) {
      input[ch] = (int32_t *)malloc(sizeof(int32_t) * header.num_samples_per_block);
      output[ch] = (int32_t *)malloc(sizeof(int32_t) * header.num_samples_per_block);
    }

    /* エンコーダデコーダ作成 */
    encoder = NARUEncoder_Create(&encoder_config);
    decoder = NARUDecoder_Create(&decoder_config);
    ASSERT_TRUE(encoder != NULL);
    ASSERT_TRUE(decoder != NULL);

    /* 入力に無音セット */
    for (ch = 0; ch < header.num_channels; ch++) {
      memset(input[ch], 0, sizeof(int32_t) * header.num_samples_per_block);
    }

    /* ヘッダを元にパラメータを設定 */
    NARUEncoder_ConvertHeaderToParameter(&header, &parameter);

    /* 入力データをエンコード */
    EXPECT_EQ(NARU_APIRESULT_OK, NARUEncoder_SetEncodeParameter(encoder, &parameter));
    EXPECT_EQ(NARU_APIRESULT_OK,
      NARUEncoder_EncodeWhole(encoder, input, header.num_samples_per_block, data, sufficient_size, &output_size));

    /* エンコードデータを簡易チェック */
    EXPECT_TRUE(output_size > NARU_HEADER_SIZE);
    EXPECT_TRUE(output_size < sufficient_size);

    /* デコード */
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, output_size, &tmp_header));
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_SetHeader(decoder, &tmp_header));
    EXPECT_EQ(NARU_APIRESULT_OK,
      NARUDecoder_DecodeBlock(decoder, data + NARU_HEADER_SIZE, output_size - NARU_HEADER_SIZE,
        output, tmp_header.num_samples_per_block, &decode_output_size, &out_num_samples));

    /* 出力チェック */
    EXPECT_EQ(output_size - NARU_HEADER_SIZE, decode_output_size);
    EXPECT_EQ(header.num_samples_per_block, out_num_samples);
    for (ch = 0; ch < header.num_channels; ch++) {
      EXPECT_EQ(0, memcmp(input[ch], output[ch], sizeof(int32_t) * header.num_samples_per_block));
    }

    /* 領域の開放 */
    for (ch = 0; ch < header.num_channels; ch++) {
      free(output[ch]);
      free(input[ch]);
    }
    free(data);
    NARUDecoder_Destroy(decoder);
    NARUEncoder_Destroy(encoder);
  }

  /* デコード失敗テスト */
  {
    struct NARUEncoder *encoder;
    struct NARUDecoder *decoder;
    struct NARUEncoderConfig encoder_config;
    struct NARUDecoderConfig decoder_config;
    struct NARUEncodeParameter parameter;
    struct NARUHeader header, tmp_header;
    uint8_t *data;
    int32_t *input[NARU_MAX_NUM_CHANNELS];
    int32_t *output[NARU_MAX_NUM_CHANNELS];
    uint32_t ch, sufficient_size, output_size, decode_output_size, out_num_samples;

    NARU_SetValidHeader(&header);
    NARUEncoder_SetValidConfig(&encoder_config);
    NARUDecoder_SetValidConfig(&decoder_config);

    /* 十分なデータサイズ */
    sufficient_size = (2 * header.num_channels * header.num_samples_per_block * header.bits_per_sample) / 8;

    /* データ領域確保 */
    data = (uint8_t *)malloc(sufficient_size);
    for (ch = 0; ch < header.num_channels; ch++) {
      input[ch] = (int32_t *)malloc(sizeof(int32_t) * header.num_samples_per_block);
      output[ch] = (int32_t *)malloc(sizeof(int32_t) * header.num_samples_per_block);
    }

    /* エンコーダデコーダ作成 */
    encoder = NARUEncoder_Create(&encoder_config);
    decoder = NARUDecoder_Create(&decoder_config);
    ASSERT_TRUE(encoder != NULL);
    ASSERT_TRUE(decoder != NULL);

    /* 入力に無音セット */
    for (ch = 0; ch < header.num_channels; ch++) {
      memset(input[ch], 0, sizeof(int32_t) * header.num_samples_per_block);
    }

    /* ヘッダを元にパラメータを設定 */
    NARUEncoder_ConvertHeaderToParameter(&header, &parameter);

    /* 入力データをエンコード */
    EXPECT_EQ(NARU_APIRESULT_OK, NARUEncoder_SetEncodeParameter(encoder, &parameter));
    EXPECT_EQ(NARU_APIRESULT_OK, NARUEncoder_EncodeWhole(encoder, input, header.num_samples_per_block, data, sufficient_size, &output_size));

    /* エンコードデータを簡易チェック */
    EXPECT_TRUE(output_size > NARU_HEADER_SIZE);
    EXPECT_TRUE(output_size < sufficient_size);

    /* ヘッダデコード */
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, output_size, &tmp_header));
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_SetHeader(decoder, &tmp_header));

    /* 不正な引数 */
    EXPECT_EQ(NARU_APIRESULT_INVALID_ARGUMENT,
      NARUDecoder_DecodeBlock(NULL, data, output_size, output, tmp_header.num_samples_per_block, &decode_output_size, &out_num_samples));
    EXPECT_EQ(NARU_APIRESULT_INVALID_ARGUMENT,
      NARUDecoder_DecodeBlock(decoder, NULL, output_size, output, tmp_header.num_samples_per_block, &decode_output_size, &out_num_samples));
    EXPECT_EQ(NARU_APIRESULT_INVALID_ARGUMENT,
      NARUDecoder_DecodeBlock(decoder, data, output_size, NULL, tmp_header.num_samples_per_block, &decode_output_size, &out_num_samples));
    EXPECT_EQ(NARU_APIRESULT_INVALID_ARGUMENT,
      NARUDecoder_DecodeBlock(decoder, data, output_size, output, tmp_header.num_samples_per_block, NULL, &out_num_samples));
    EXPECT_EQ(NARU_APIRESULT_INVALID_ARGUMENT,
      NARUDecoder_DecodeBlock(decoder, data, output_size, output, tmp_header.num_samples_per_block, &decode_output_size, NULL));

    /* データサイズ不足 */
    EXPECT_EQ(NARU_APIRESULT_INSUFFICIENT_DATA,
      NARUDecoder_DecodeBlock(decoder, data + NARU_HEADER_SIZE, output_size - NARU_HEADER_SIZE - 1,
        output, tmp_header.num_samples_per_block, &decode_output_size, &out_num_samples));

    /* データを一部破壊した場合にエラーを返すか */

    /* 同期コード（データ先頭16bit）破壊 */
    EXPECT_EQ(NARU_APIRESULT_OK, NARUEncoder_EncodeWhole(encoder, input, header.num_samples_per_block, data, sufficient_size, &output_size));
    data[NARU_HEADER_SIZE] ^= 0xFF;
    EXPECT_EQ(NARU_APIRESULT_INVALID_FORMAT,
      NARUDecoder_DecodeBlock(decoder, data + NARU_HEADER_SIZE, output_size - NARU_HEADER_SIZE,
        output, tmp_header.num_samples_per_block, &decode_output_size, &out_num_samples));
    EXPECT_EQ(NARU_APIRESULT_OK, NARUEncoder_EncodeWhole(encoder, input, header.num_samples_per_block, data, sufficient_size, &output_size));
    data[NARU_HEADER_SIZE + 1] ^= 0xFF;
    EXPECT_EQ(NARU_APIRESULT_INVALID_FORMAT,
      NARUDecoder_DecodeBlock(decoder, data + NARU_HEADER_SIZE, output_size - NARU_HEADER_SIZE,
        output, tmp_header.num_samples_per_block, &decode_output_size, &out_num_samples));
    /* ブロックデータタイプ不正 */
    EXPECT_EQ(NARU_APIRESULT_OK, NARUEncoder_EncodeWhole(encoder, input, header.num_samples_per_block, data, sufficient_size, &output_size));
    data[NARU_HEADER_SIZE + 8] = 0xC0;
    EXPECT_EQ(NARU_APIRESULT_INVALID_FORMAT,
      NARUDecoder_DecodeBlock(decoder, data + NARU_HEADER_SIZE, output_size - NARU_HEADER_SIZE,
        output, tmp_header.num_samples_per_block, &decode_output_size, &out_num_samples));

    /* 領域の開放 */
    for (ch = 0; ch < header.num_channels; ch++) {
      free(output[ch]);
      free(input[ch]);
    }
    free(data);
    NARUDecoder_Destroy(decoder);
    NARUEncoder_Destroy(encoder);
  }
}

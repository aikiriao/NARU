#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

/* テスト対象のモジュール */
extern "C" {
#include "../../libs/naru_encoder/src/naru_encoder.c"
}

/* 有効なヘッダをセット */
#define NARU_SetValidHeader(p_header)\
  do {\
    struct NARUHeaderInfo *header__p = p_header;\
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

/* 有効なエンコードパラメータをセット */
#define NARUEncoder_SetValidEncodeParameter(p_parameter)\
  do {\
    struct NARUEncodeParameter *param__p = p_parameter;\
    param__p->num_channels          = 1;\
    param__p->sampling_rate         = 44100;\
    param__p->bits_per_sample       = 16;\
    param__p->num_samples_per_block = 32;\
    param__p->filter_order          = 8;\
    param__p->ar_order              = 1;\
    param__p->second_filter_order   = 4;\
    param__p->ch_process_method     = NARU_CH_PROCESS_METHOD_NONE;\
  } while (0);

/* 有効なコンフィグをセット */
#define NARUEncoder_SetValidConfig(p_config)\
  do {\
    struct NARUEncoderConfig *config__p = p_config;\
    config__p->max_num_channels           = 8;\
    config__p->max_num_samples_per_block  = 8192;\
    config__p->max_filter_order           = 32;\
  } while (0);

/* ヘッダエンコードテスト */
TEST(NARUEncoderTest, EncodeHeaderTest)
{
  /* ヘッダエンコード成功ケース */
  {
    struct NARUHeaderInfo header;
    uint8_t data[NARU_HEADER_SIZE] = { 0, };

    NARU_SetValidHeader(&header);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUEncoder_EncodeHeader(&header, data, sizeof(data)));

    /* 簡易チェック */
    EXPECT_EQ('N', data[0]);
    EXPECT_EQ('A', data[1]);
    EXPECT_EQ('R', data[2]);
    EXPECT_EQ('U', data[3]);
  }

  /* ヘッダエンコード失敗ケース */
  {
    struct NARUHeaderInfo header;
    uint8_t data[NARU_HEADER_SIZE] = { 0, };

    /* 引数が不正 */
    NARU_SetValidHeader(&header);
    EXPECT_EQ(NARU_APIRESULT_INVALID_ARGUMENT, NARUEncoder_EncodeHeader(NULL, data, sizeof(data)));
    EXPECT_EQ(NARU_APIRESULT_INVALID_ARGUMENT, NARUEncoder_EncodeHeader(&header, NULL, sizeof(data)));

    /* データサイズ不足 */
    NARU_SetValidHeader(&header);
    EXPECT_EQ(NARU_APIRESULT_INSUFFICIENT_BUFFER, NARUEncoder_EncodeHeader(&header, data, sizeof(data) - 1));
    EXPECT_EQ(NARU_APIRESULT_INSUFFICIENT_BUFFER, NARUEncoder_EncodeHeader(&header, data, NARU_HEADER_SIZE - 1));

    /* 異常なチャンネル数 */
    NARU_SetValidHeader(&header);
    header.num_channels = 0;
    EXPECT_EQ(NARU_APIRESULT_INVALID_FORMAT, NARUEncoder_EncodeHeader(&header, data, sizeof(data)));

    /* 異常なサンプル数 */
    NARU_SetValidHeader(&header);
    header.num_samples = 0;
    EXPECT_EQ(NARU_APIRESULT_INVALID_FORMAT, NARUEncoder_EncodeHeader(&header, data, sizeof(data)));

    /* 異常なサンプリングレート */
    NARU_SetValidHeader(&header);
    header.sampling_rate = 0;
    EXPECT_EQ(NARU_APIRESULT_INVALID_FORMAT, NARUEncoder_EncodeHeader(&header, data, sizeof(data)));

    /* 異常なビット深度 */
    NARU_SetValidHeader(&header);
    header.bits_per_sample = 0;
    EXPECT_EQ(NARU_APIRESULT_INVALID_FORMAT, NARUEncoder_EncodeHeader(&header, data, sizeof(data)));

    /* 異常なブロックあたりサンプル数 */
    NARU_SetValidHeader(&header);
    header.num_samples_per_block = 0;
    EXPECT_EQ(NARU_APIRESULT_INVALID_FORMAT, NARUEncoder_EncodeHeader(&header, data, sizeof(data)));

    /* 異常なフィルタ次数 */
    NARU_SetValidHeader(&header);
    header.filter_order = 0;
    EXPECT_EQ(NARU_APIRESULT_INVALID_FORMAT, NARUEncoder_EncodeHeader(&header, data, sizeof(data)));
    NARU_SetValidHeader(&header);
    header.filter_order = 1;
    EXPECT_EQ(NARU_APIRESULT_INVALID_FORMAT, NARUEncoder_EncodeHeader(&header, data, sizeof(data)));

    /* 異常なAR次数 */
    NARU_SetValidHeader(&header);
    header.ar_order = header.filter_order / 2 + 1;
    EXPECT_EQ(NARU_APIRESULT_INVALID_FORMAT, NARUEncoder_EncodeHeader(&header, data, sizeof(data)));
    NARU_SetValidHeader(&header);
    header.ar_order = 2 * header.filter_order;
    EXPECT_EQ(NARU_APIRESULT_INVALID_FORMAT, NARUEncoder_EncodeHeader(&header, data, sizeof(data)));

    /* 異常なチャンネル処理法 */
    NARU_SetValidHeader(&header);
    header.ch_process_method = NARU_CH_PROCESS_METHOD_INVALID;
    EXPECT_EQ(NARU_APIRESULT_INVALID_FORMAT, NARUEncoder_EncodeHeader(&header, data, sizeof(data)));

    /* チャンネル処理法とチャンネル数指定の組み合わせがおかしい */
    NARU_SetValidHeader(&header);
    header.num_channels = 1;
    header.ch_process_method = NARU_CH_PROCESS_METHOD_MS;
    EXPECT_EQ(NARU_APIRESULT_INVALID_FORMAT, NARUEncoder_EncodeHeader(&header, data, sizeof(data)));
  }

}

/* 1ブロックエンコードテスト */
TEST(NARUEncoderTest, EncodeBlockTest)
{
  /* 無効な引数 */
  {
    struct NARUEncoder *encoder;
    struct NARUEncoderConfig config;
    struct NARUEncodeParameter parameter;
    int32_t *input[NARU_MAX_NUM_CHANNELS];
    uint8_t *data;
    uint32_t ch, sufficient_size, output_size, num_samples;
    
    NARUEncoder_SetValidEncodeParameter(&parameter);
    NARUEncoder_SetValidConfig(&config);
    
    /* 十分なデータサイズ */
    sufficient_size = (2 * parameter.num_channels * parameter.num_samples_per_block * parameter.bits_per_sample) / 8;

    /* データ領域確保 */
    data = (uint8_t *)malloc(sufficient_size);
    for (ch = 0; ch < parameter.num_channels; ch++) {
      input[ch] = (int32_t *)malloc(sizeof(int32_t) * parameter.num_samples_per_block);
    }

    /* エンコーダ作成 */
    encoder = NARUEncoder_Create(&config);
    ASSERT_TRUE(encoder != NULL);

    /* 無効な引数を渡す */
    EXPECT_EQ(
      NARU_APIRESULT_INVALID_ARGUMENT,
      NARUEncoder_EncodeBlock(NULL, input, parameter.num_samples_per_block,
        data, sufficient_size, &output_size));
    EXPECT_EQ(
      NARU_APIRESULT_INVALID_ARGUMENT,
      NARUEncoder_EncodeBlock(encoder, NULL, parameter.num_samples_per_block,
        data, sufficient_size, &output_size));
    EXPECT_EQ(
      NARU_APIRESULT_INVALID_ARGUMENT,
      NARUEncoder_EncodeBlock(encoder, input, 0,
        data, sufficient_size, &output_size));
    EXPECT_EQ(
      NARU_APIRESULT_INVALID_ARGUMENT,
      NARUEncoder_EncodeBlock(encoder, input, parameter.num_samples_per_block,
        NULL, sufficient_size, &output_size));
    EXPECT_EQ(
      NARU_APIRESULT_INVALID_ARGUMENT,
      NARUEncoder_EncodeBlock(encoder, input, parameter.num_samples_per_block,
        data, 0, &output_size));
    EXPECT_EQ(
      NARU_APIRESULT_INVALID_ARGUMENT,
      NARUEncoder_EncodeBlock(encoder, input, parameter.num_samples_per_block,
        data, sufficient_size, NULL));

    /* 領域の開放 */
    for (ch = 0; ch < parameter.num_channels; ch++) {
      free(input[ch]);
    }
    free(data);
    NARUEncoder_Destroy(encoder);
  }

  /* パラメータ未セットでエンコード */
  {
    struct NARUEncoder *encoder;
    struct NARUEncoderConfig config;
    struct NARUEncodeParameter parameter;
    int32_t *input[NARU_MAX_NUM_CHANNELS];
    uint8_t *data;
    uint32_t ch, sufficient_size, output_size, num_samples;
    
    NARUEncoder_SetValidEncodeParameter(&parameter);
    NARUEncoder_SetValidConfig(&config);
    
    /* 十分なデータサイズ */
    sufficient_size = (2 * parameter.num_channels * parameter.num_samples_per_block * parameter.bits_per_sample) / 8;

    /* データ領域確保 */
    data = (uint8_t *)malloc(sufficient_size);
    for (ch = 0; ch < parameter.num_channels; ch++) {
      input[ch] = (int32_t *)malloc(sizeof(int32_t) * parameter.num_samples_per_block);
      /* 無音セット */
      memset(input[ch], 0, sizeof(int32_t) * parameter.num_samples_per_block);
    }

    /* エンコーダ作成 */
    encoder = NARUEncoder_Create(&config);
    ASSERT_TRUE(encoder != NULL);

    /* パラメータセット前にエンコード: エラー */
    EXPECT_EQ(
      NARU_APIRESULT_PARAMETER_NOT_SET,
      NARUEncoder_EncodeBlock(encoder, input, parameter.num_samples_per_block,
        data, sufficient_size, &output_size));

    /* パラメータ設定 */
    EXPECT_EQ(
      NARU_APIRESULT_OK,
      NARUEncoder_SetEncodeParameter(encoder, &parameter));

    /* 1ブロックエンコード */
    EXPECT_EQ(
      NARU_APIRESULT_OK,
      NARUEncoder_EncodeBlock(encoder, input, parameter.num_samples_per_block,
        data, sufficient_size, &output_size));

    /* 領域の開放 */
    for (ch = 0; ch < parameter.num_channels; ch++) {
      free(input[ch]);
    }
    free(data);
    NARUEncoder_Destroy(encoder);
  }

  /* 無音エンコード */
  {
    struct NARUEncoder *encoder;
    struct NARUEncoderConfig config;
    struct NARUEncodeParameter parameter;
    struct NARUBitStream stream;
    int32_t *input[NARU_MAX_NUM_CHANNELS];
    uint8_t *data;
    uint32_t ch, sufficient_size, output_size, num_samples;
    uint32_t bitbuf;
    
    NARUEncoder_SetValidEncodeParameter(&parameter);
    NARUEncoder_SetValidConfig(&config);
    
    /* 十分なデータサイズ */
    sufficient_size = (2 * parameter.num_channels * parameter.num_samples_per_block * parameter.bits_per_sample) / 8;

    /* データ領域確保 */
    data = (uint8_t *)malloc(sufficient_size);
    for (ch = 0; ch < parameter.num_channels; ch++) {
      input[ch] = (int32_t *)malloc(sizeof(int32_t) * parameter.num_samples_per_block);
      /* 無音セット */
      memset(input[ch], 0, sizeof(int32_t) * parameter.num_samples_per_block);
    }

    /* エンコーダ作成 */
    encoder = NARUEncoder_Create(&config);
    ASSERT_TRUE(encoder != NULL);

    /* パラメータ設定 */
    EXPECT_EQ(
      NARU_APIRESULT_OK,
      NARUEncoder_SetEncodeParameter(encoder, &parameter));

    /* 1ブロックエンコード */
    EXPECT_EQ(
      NARU_APIRESULT_OK,
      NARUEncoder_EncodeBlock(encoder, input, parameter.num_samples_per_block,
        data, sufficient_size, &output_size));

    /* ブロック先頭の同期コードがあるので2バイトよりは大きいはず */
    EXPECT_TRUE(output_size > 2);

    /* 内容の確認 */
    NARUBitReader_Open(&stream, data, output_size);
    /* 同期コード */
    NARUBitReader_GetBits(&stream, &bitbuf, 16);
    EXPECT_EQ(NARU_BLOCK_SYNC_CODE, bitbuf);
    /* ブロックデータタイプ */
    NARUBitReader_GetBits(&stream, &bitbuf, 2);
    EXPECT_TRUE((bitbuf == NARU_BLOCK_DATA_TYPE_COMPRESSDATA)
        || (bitbuf == NARU_BLOCK_DATA_TYPE_SILENT)
        || (bitbuf == NARU_BLOCK_DATA_TYPE_RAWDATA));
    /* この後データがエンコードされているので、まだ終端ではないはず */
    NARUBitStream_Tell(&stream, (int32_t *)&bitbuf);
    EXPECT_TRUE(bitbuf < output_size);
    NARUBitStream_Close(&stream);

    /* 領域の開放 */
    for (ch = 0; ch < parameter.num_channels; ch++) {
      free(input[ch]);
    }
    free(data);
    NARUEncoder_Destroy(encoder);
  }
}

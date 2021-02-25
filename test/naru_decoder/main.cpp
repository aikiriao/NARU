#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

/* エンコーダを使用 */
#include "naru_encoder.h"

/* テスト対象のモジュール */
extern "C" {
#include "../../libs/naru_decoder/src/naru_decoder.c"
}

/* ヘッダエンコードテスト */
TEST(NARUEncoderTest, EncodeHeaderTest)
{
  /* 有効なヘッダをセット */
#define NARU_SetValidHeader(p_header)\
  do {\
    struct NARUHeaderInfo *header__p = p_header;\
    header__p->num_channels          = 1;\
    header__p->sampling_rate         = 44100;\
    header__p->bits_per_sample       = 16;\
    header__p->num_samples           = 1024;\
    header__p->num_samples_per_block = 32;\
    header__p->filter_order          = 8;\
    header__p->ar_order              = 1;\
    header__p->ch_process_method     = NARU_CH_PROCESS_METHOD_NONE;\
  } while (0);

  /* 成功例 */
  {
    uint8_t data[NARU_HEADER_SIZE] = { 0, };
    struct NARUHeaderInfo header, tmp_header;

    NARU_SetValidHeader(&header);

    /* エンコード->デコード */
    EXPECT_EQ(NARU_APIRESULT_OK, NARUEncoder_EncodeHeader(&header, data, sizeof(data)));
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &tmp_header));

    /* デコードしたヘッダの一致確認 */
    EXPECT_EQ(NARU_FORMAT_VERSION, tmp_header.format_version);
    EXPECT_EQ(NARU_ENCODER_VERSION, tmp_header.encoder_version);
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
    struct NARUHeaderInfo header, getheader;
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
    ByteArray_WriteUint32BE(&data[8], NARU_ENCODER_VERSION + 1);
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
    ByteArray_WriteUint8(&data[28], 1);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));

    /* 異常なAR次数 */
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint8(&data[29], 0);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint8(&data[29], header.filter_order);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));

    /* 異常なチャンネル処理法 */
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint8(&data[30], NARU_CH_PROCESS_METHOD_INVALID);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));

    /* チャンネル処理法とチャンネル数指定の組み合わせがおかしい */
    memcpy(data, valid_data, sizeof(valid_data));
    memset(&getheader, 0xCD, sizeof(getheader));
    ByteArray_WriteUint16BE(&data[12], 1);
    ByteArray_WriteUint8(&data[30], NARU_CH_PROCESS_METHOD_MS);
    EXPECT_EQ(NARU_APIRESULT_OK, NARUDecoder_DecodeHeader(data, sizeof(data), &getheader));
    EXPECT_EQ(NARU_ERROR_INVALID_FORMAT, NARUDecoder_CheckHeaderFormat(&getheader));
  }
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

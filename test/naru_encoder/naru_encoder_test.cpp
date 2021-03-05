#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

/* テスト対象のモジュール */
extern "C" {
#include "../../libs/naru_encoder/src/naru_encoder.c"
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
    header__p->second_filter_order   = 4;\
    header__p->ch_process_method     = NARU_CH_PROCESS_METHOD_NONE;\
  } while (0);

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

#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

/* テスト対象のモジュール */
extern "C" {
#include "../../libs/naru_bit_stream/src/naru_bit_stream.c"
}

/* インスタンス作成破棄テスト */
TEST(NARUBitStreamTest, CreateDestroyTest)
{
  /* インスタンス作成・破棄（メモリ） */
  {
    struct NARUBitStream strm;
    uint8_t test_memory[] = {'A', 'I', 'K', 'A', 'T', 'S', 'U'};
    const uint32_t test_memory_size = sizeof(test_memory) / sizeof(test_memory[0]);

    /* 書きモードでインスタンス作成 */
    NARUBitWriter_Open(&strm, test_memory, test_memory_size);
    EXPECT_TRUE(strm.memory_image == test_memory);
    EXPECT_EQ(test_memory_size, strm.memory_size);
    EXPECT_TRUE(strm.memory_p == test_memory);
    EXPECT_EQ(0, strm.bit_buffer);
    EXPECT_EQ(8, strm.bit_count);
    EXPECT_TRUE(!(strm.flags & NARUBITSTREAM_FLAGS_MODE_READ));
    NARUBitStream_Close(&strm);

    /* 読みモードでインスタンス作成 */
    NARUBitReader_Open(&strm, test_memory, test_memory_size);
    EXPECT_TRUE(strm.memory_image == test_memory);
    EXPECT_EQ(test_memory_size, strm.memory_size);
    EXPECT_TRUE(strm.memory_p == test_memory);
    EXPECT_EQ(0, strm.bit_buffer);
    EXPECT_EQ(0, strm.bit_count);
    EXPECT_TRUE(strm.flags & NARUBITSTREAM_FLAGS_MODE_READ);
    NARUBitStream_Close(&strm);
  }
}

/* PutBit関数テスト */
TEST(NARUBitStreamTest, PutGetTest)
{
  {
    struct NARUBitStream strm;
    uint8_t bit_pattern[] = { 1, 1, 1, 1, 0, 0, 0, 0 };
    uint8_t memory_image[256];
    uint32_t bit_pattern_length = sizeof(bit_pattern) / sizeof(bit_pattern[0]);
    uint32_t i, is_ok;

    /* 書き込んでみる */
    NARUBitWriter_Open(&strm, memory_image, sizeof(memory_image));
    for (i = 0; i < bit_pattern_length; i++) { 
      NARUBitWriter_PutBits(&strm, bit_pattern[i], 1);
    }
    NARUBitStream_Close(&strm);

    /* 正しく書き込めているか？ */
    NARUBitReader_Open(&strm, memory_image, sizeof(memory_image));
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      uint32_t buf;
      NARUBitReader_GetBits(&strm, &buf, 1);
      if ((uint8_t)buf != bit_pattern[i]) {
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    NARUBitStream_Close(&strm);
  }

  /* PutBit関数テスト2 8bitパターンチェック */
  {
    struct NARUBitStream strm;
    uint8_t memory_image[256];
    uint32_t i, is_ok, nbits;

    for (nbits = 1; nbits <= 8; nbits++) {
      /* 書き込んでみる */
      NARUBitWriter_Open(&strm, memory_image, sizeof(memory_image));
      for (i = 0; i < (1 << nbits); i++) { 
        NARUBitWriter_PutBits(&strm, i, nbits);
      }
      NARUBitStream_Close(&strm);

      /* 正しく書き込めているか？ */
      NARUBitReader_Open(&strm, memory_image, sizeof(memory_image));
      is_ok = 1;
      for (i = 0; i < (1 << nbits); i++) { 
        uint32_t buf;
        NARUBitReader_GetBits(&strm, &buf, nbits);
        if (buf != i) {
          is_ok = 0;
          break;
        }
      }
      EXPECT_EQ(1, is_ok);
      NARUBitStream_Close(&strm);
    }

  }

  /* Flushテスト */
  {
    struct NARUBitStream strm;
    uint8_t memory_image[256];
    uint32_t bits;

    NARUBitWriter_Open(&strm, memory_image, sizeof(memory_image));
    NARUBitWriter_PutBits(&strm, 1, 1);
    NARUBitWriter_PutBits(&strm, 1, 1);
    /* 2bitしか書いていないがフラッシュ */
    NARUBitStream_Flush(&strm);
    EXPECT_EQ(0, strm.bit_buffer);
    EXPECT_EQ(8, strm.bit_count);
    NARUBitStream_Close(&strm);

    /* 1バイトで先頭2bitだけが立っているはず */
    NARUBitReader_Open(&strm, memory_image, sizeof(memory_image));
    NARUBitReader_GetBits(&strm, &bits, 8);
    EXPECT_EQ(0xC0, bits);
    NARUBitStream_Flush(&strm);
    EXPECT_EQ(0, strm.bit_count);
    EXPECT_EQ(0xC0, strm.bit_buffer);
    NARUBitStream_Close(&strm);
  }

}

/* seek, tellなどのストリーム操作系APIテスト */
TEST(NARUBitStreamTest, StreamOperationTest)
{
  /* Seek/Tellテスト */
  {
    struct NARUBitStream   strm;
    int32_t               tell_result;
    uint8_t               test_memory[8];

    /* テスト用に適当にデータ作成 */
    NARUBitWriter_Open(&strm, test_memory, sizeof(test_memory));
    NARUBitWriter_PutBits(&strm, 0xDEADBEAF, 32);
    NARUBitWriter_PutBits(&strm, 0xABADCAFE, 32);
    NARUBitStream_Tell(&strm, &tell_result);
    EXPECT_EQ(8, tell_result);
    NARUBitStream_Close(&strm);

    /* ビットリーダを使ったseek & tellテスト */
    NARUBitReader_Open(&strm, test_memory, sizeof(test_memory));
    NARUBitStream_Seek(&strm, 0, NARUBITSTREAM_SEEK_SET);
    NARUBitStream_Tell(&strm, &tell_result);
    EXPECT_EQ(0, tell_result);
    NARUBitStream_Seek(&strm, 1, NARUBITSTREAM_SEEK_CUR);
    NARUBitStream_Tell(&strm, &tell_result);
    EXPECT_EQ(1, tell_result);
    NARUBitStream_Seek(&strm, 2, NARUBITSTREAM_SEEK_CUR);
    NARUBitStream_Tell(&strm, &tell_result);
    EXPECT_EQ(3, tell_result);
    NARUBitStream_Seek(&strm, 0, NARUBITSTREAM_SEEK_END);
    NARUBitStream_Tell(&strm, &tell_result);
    EXPECT_EQ(7, tell_result);
    NARUBitStream_Close(&strm);

    /* ビットライタを使ったseek & tellテスト */
    NARUBitWriter_Open(&strm, test_memory, sizeof(test_memory));
    NARUBitStream_Seek(&strm, 0, NARUBITSTREAM_SEEK_SET);
    NARUBitStream_Tell(&strm, &tell_result);
    EXPECT_EQ(0, tell_result);
    NARUBitStream_Seek(&strm, 1, NARUBITSTREAM_SEEK_CUR);
    NARUBitStream_Tell(&strm, &tell_result);
    EXPECT_EQ(1, tell_result);
    NARUBitStream_Seek(&strm, 2, NARUBITSTREAM_SEEK_CUR);
    NARUBitStream_Tell(&strm, &tell_result);
    EXPECT_EQ(3, tell_result);
    NARUBitStream_Seek(&strm, 0, NARUBITSTREAM_SEEK_END);
    NARUBitStream_Tell(&strm, &tell_result);
    EXPECT_EQ(7, tell_result);
    NARUBitStream_Close(&strm);
  }
}

/* ランレングス取得テスト */
TEST(NARUBitStreamTest, GetZeroRunLengthTest)
{
  {
    struct NARUBitStream strm;
    uint8_t data[5];
    uint32_t test_length, run;

    for (test_length = 1; test_length <= 32; test_length++) {
      /* ラン長だけ0を書き込み、1で止める */
      NARUBitWriter_Open(&strm, data, sizeof(data));
      NARUBitWriter_PutBits(&strm, 0, test_length);
      NARUBitWriter_PutBits(&strm, 1, 1);
      NARUBitStream_Close(&strm);

      NARUBitReader_Open(&strm, data, sizeof(data));
      NARUBitReader_GetZeroRunLength(&strm, &run);
      EXPECT_EQ(test_length, run);
    }
  }
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

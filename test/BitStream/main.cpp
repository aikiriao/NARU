#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

/* テスト対象のモジュール */
extern "C" {
#include "../../libs/BitStream/src/bit_stream.c"
}

/* インスタンス作成破棄テスト */
TEST(BitStreamTest, CreateDestroyTest)
{
  /* ワークサイズ計算テスト */
  {
    EXPECT_TRUE((int32_t)sizeof(struct BitStream) <= BitStream_CalculateWorkSize());
  }

  /* インスタンス作成・破棄 */
  {
    struct BitStream* strm;
    void*             work;
    const char test_filename[] = "test.bin";

    /* 書きモードでインスタンス作成 */
    strm = BitStream_Open(test_filename, "w", NULL, 0);
    EXPECT_TRUE(strm != NULL);
    EXPECT_TRUE(strm->stm.fp != NULL);
    EXPECT_TRUE(!(strm->flags & BITSTREAM_FLAGS_MEMORYALLOC_BYWORK));
    EXPECT_TRUE(strm->work_ptr != NULL);
    EXPECT_EQ(0, strm->bit_buffer);
    EXPECT_TRUE(!(strm->flags & BITSTREAM_FLAGS_FILEOPENMODE_READ));
    EXPECT_EQ(8, strm->bit_count);
    BitStream_Close(strm);
    EXPECT_TRUE(strm->work_ptr == NULL);

    /* 読みモードでインスタンス作成 */
    strm = BitStream_Open(test_filename, "r", NULL, 0);
    EXPECT_TRUE(strm != NULL);
    EXPECT_TRUE(strm->stm.fp != NULL);
    EXPECT_TRUE(!(strm->flags & BITSTREAM_FLAGS_MEMORYALLOC_BYWORK));
    EXPECT_TRUE(strm->work_ptr != NULL);
    EXPECT_EQ(0, strm->bit_buffer);
    EXPECT_TRUE(strm->flags & BITSTREAM_FLAGS_FILEOPENMODE_READ);
    EXPECT_EQ(0, strm->bit_count);
    BitStream_Close(strm);
    EXPECT_TRUE(strm->work_ptr == NULL);

    /* ワークメモリを渡してインスタンス作成・破棄 */
    work = malloc(BitStream_CalculateWorkSize());

    strm = BitStream_Open(test_filename, "w", work, BitStream_CalculateWorkSize());
    EXPECT_TRUE(strm != NULL);
    EXPECT_TRUE(strm->stm.fp != NULL);
    EXPECT_TRUE(strm->flags & BITSTREAM_FLAGS_MEMORYALLOC_BYWORK);
    EXPECT_TRUE(strm->work_ptr != NULL);
    EXPECT_EQ(0, strm->bit_buffer);
    EXPECT_TRUE(!(strm->flags & BITSTREAM_FLAGS_FILEOPENMODE_READ));
    EXPECT_EQ(8, strm->bit_count);
    BitStream_Close(strm);

    strm = BitStream_Open(test_filename, "r", work, BitStream_CalculateWorkSize());
    EXPECT_TRUE(strm != NULL);
    EXPECT_TRUE(strm->stm.fp != NULL);
    EXPECT_TRUE(strm->flags & BITSTREAM_FLAGS_MEMORYALLOC_BYWORK);
    EXPECT_TRUE(strm->work_ptr != NULL);
    EXPECT_EQ(0, strm->bit_buffer);
    EXPECT_TRUE(strm->flags & BITSTREAM_FLAGS_FILEOPENMODE_READ);
    EXPECT_EQ(0, strm->bit_count);
    BitStream_Close(strm);

    free(work);
  }

  /* 作成失敗テスト */
  {
    struct BitStream* strm;
    void*             work;
    const char test_filename[] = "test.bin";

    /* テスト用にファイル作成 */
    fclose(fopen(test_filename, "w"));

    /* モードが不正/NULL */
    strm = BitStream_Open("test.bin", "a", NULL, 0);
    EXPECT_TRUE(strm == NULL);
    strm = BitStream_Open("test.bin", "+", NULL, 0);
    EXPECT_TRUE(strm == NULL);
    strm = BitStream_Open("test.bin", NULL, NULL, 0);
    EXPECT_TRUE(strm == NULL);

    /* ワークサイズが不正 */
    strm = BitStream_Open("test.bin", "w", NULL, -1);
    EXPECT_TRUE(strm == NULL);
    {
      work = malloc(BitStream_CalculateWorkSize());
      strm = BitStream_Open("test.bin", "w", work, -1);
      EXPECT_TRUE(strm == NULL);
      strm = BitStream_Open("test.bin", "w", work, BitStream_CalculateWorkSize()-1);
      EXPECT_TRUE(strm == NULL);
      free(work);
    }
  }

  /* インスタンス作成・破棄（メモリ） */
  {
    struct BitStream* strm;
    uint8_t test_memory[] = {'A', 'I', 'K', 'A', 'T', 'S', 'U'};
    const uint32_t test_memory_size = sizeof(test_memory) / sizeof(test_memory[0]);
    void* work;

    /* 書きモードでインスタンス作成 */
    strm = BitStream_OpenMemory(test_memory, test_memory_size, "w", NULL, 0);
    EXPECT_TRUE(strm != NULL);
    EXPECT_TRUE(strm->stm.mem.memory_image == test_memory);
    EXPECT_EQ(test_memory_size, strm->stm.mem.memory_size);
    EXPECT_EQ(0, strm->stm.mem.memory_p);
    EXPECT_TRUE(!(strm->flags & BITSTREAM_FLAGS_MEMORYALLOC_BYWORK));
    EXPECT_TRUE(strm->work_ptr != NULL);
    EXPECT_EQ(0, strm->bit_buffer);
    EXPECT_TRUE(!(strm->flags & BITSTREAM_FLAGS_FILEOPENMODE_READ));
    EXPECT_EQ(8, strm->bit_count);
    BitStream_Close(strm);
    EXPECT_TRUE(strm->work_ptr == NULL);

    /* 読みモードでインスタンス作成 */
    strm = BitStream_OpenMemory(test_memory, test_memory_size, "r", NULL, 0);
    EXPECT_TRUE(strm != NULL);
    EXPECT_TRUE(strm->stm.mem.memory_image == test_memory);
    EXPECT_EQ(test_memory_size, strm->stm.mem.memory_size);
    EXPECT_EQ(0, strm->stm.mem.memory_p);
    EXPECT_TRUE(!(strm->flags & BITSTREAM_FLAGS_MEMORYALLOC_BYWORK));
    EXPECT_TRUE(strm->work_ptr != NULL);
    EXPECT_EQ(0, strm->bit_buffer);
    EXPECT_TRUE(strm->flags & BITSTREAM_FLAGS_FILEOPENMODE_READ);
    EXPECT_EQ(0, strm->bit_count);
    BitStream_Close(strm);
    EXPECT_TRUE(strm->work_ptr == NULL);

    /* ワークメモリを渡して作成・破棄 */
    work = malloc(BitStream_CalculateWorkSize());

    strm = BitStream_OpenMemory(test_memory, test_memory_size, "w", work, BitStream_CalculateWorkSize());
    EXPECT_TRUE(strm != NULL);
    EXPECT_TRUE(strm->stm.mem.memory_image == test_memory);
    EXPECT_EQ(test_memory_size, strm->stm.mem.memory_size);
    EXPECT_EQ(0, strm->stm.mem.memory_p);
    EXPECT_TRUE(strm->flags & BITSTREAM_FLAGS_MEMORYALLOC_BYWORK);
    EXPECT_TRUE(strm->work_ptr != NULL);
    EXPECT_EQ(0, strm->bit_buffer);
    EXPECT_TRUE(!(strm->flags & BITSTREAM_FLAGS_FILEOPENMODE_READ));
    EXPECT_EQ(8, strm->bit_count);
    BitStream_Close(strm);

    strm = BitStream_OpenMemory(test_memory, test_memory_size, "r", work, BitStream_CalculateWorkSize());
    EXPECT_TRUE(strm != NULL);
    EXPECT_TRUE(strm->stm.mem.memory_image == test_memory);
    EXPECT_EQ(test_memory_size, strm->stm.mem.memory_size);
    EXPECT_EQ(0, strm->stm.mem.memory_p);
    EXPECT_TRUE(strm->flags & BITSTREAM_FLAGS_MEMORYALLOC_BYWORK);
    EXPECT_TRUE(strm->work_ptr != NULL);
    EXPECT_EQ(0, strm->bit_buffer);
    EXPECT_TRUE(strm->flags & BITSTREAM_FLAGS_FILEOPENMODE_READ);
    EXPECT_EQ(0, strm->bit_count);
    BitStream_Close(strm);

    free(work);
  }

  /* 作成失敗テスト（メモリ） */
  {
    struct BitStream* strm;
    void*             work;
    uint8_t test_memory[] = {'A', 'I', 'K', 'A', 'T', 'S', 'U'};
    const uint32_t test_memory_size = sizeof(test_memory) / sizeof(test_memory[0]);

    /* メモリがNULL */
    strm = BitStream_OpenMemory(NULL, 0, "w", NULL, 0);
    EXPECT_TRUE(strm == NULL);

    /* モードが不正/NULL */
    strm = BitStream_OpenMemory(test_memory, test_memory_size, "a", NULL, 0);
    EXPECT_TRUE(strm == NULL);
    strm = BitStream_OpenMemory(test_memory, test_memory_size, "+", NULL, 0);
    EXPECT_TRUE(strm == NULL);
    strm = BitStream_OpenMemory(test_memory, test_memory_size, NULL, NULL, 0);
    EXPECT_TRUE(strm == NULL);

    /* ワークサイズが不正 */
    strm = BitStream_Open("test.bin", "w", NULL, -1);
    EXPECT_TRUE(strm == NULL);
    {
      work = malloc(BitStream_CalculateWorkSize());
      strm = BitStream_OpenMemory(test_memory, test_memory_size, "w", work, -1);
      EXPECT_TRUE(strm == NULL);
      strm = BitStream_OpenMemory(test_memory, test_memory_size, "w", work, BitStream_CalculateWorkSize()-1);
      EXPECT_TRUE(strm == NULL);
      free(work);
    }
  }

}

/* 入出力テスト */
TEST(BitStreamTest, PutGetTest)
{
  /* 失敗テスト */
  {
    struct BitStream* strm;
    const char test_filename[] = "test_put.bin";

    strm = BitStream_Open(test_filename, "w", NULL, 0);
    EXPECT_NE(BITSTREAM_APIRESULT_OK, BitStream_PutBit(NULL, 1));
    EXPECT_NE(BITSTREAM_APIRESULT_OK, BitStream_PutBit(NULL, 0));
    EXPECT_NE(BITSTREAM_APIRESULT_OK, BitStream_PutBits(NULL,  1, 1));
    EXPECT_NE(BITSTREAM_APIRESULT_OK, BitStream_PutBits(NULL,  1, 0));
    EXPECT_NE(BITSTREAM_APIRESULT_OK, BitStream_PutBits(strm, 65, 0));

    BitStream_Close(strm);
  }

  /* PutBit関数テスト（ファイル） */
  {
    struct BitStream* strm;
    uint8_t bit;
    const char test_filename[] = "test_putbit.bin";
    uint8_t bit_pattern[] = { 1, 1, 1, 1, 0, 0, 0, 0 };
    uint32_t bit_pattern_length = sizeof(bit_pattern) / sizeof(bit_pattern[0]);
    uint32_t i, is_ok;

    /* 書き込んでみる */
    strm = BitStream_Open(test_filename, "w", NULL, 0);
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (BitStream_PutBit(strm, bit_pattern[i]) != BITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    BitStream_Close(strm);

    /* 正しく書き込めているか？ */
    strm = BitStream_Open(test_filename, "r", NULL, 0);
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (BitStream_GetBit(strm, &bit) != BITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
      if (bit != bit_pattern[i]) {
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    BitStream_Close(strm);
  }

  /* PutBits関数テスト（ファイル） */
  {
    struct BitStream* strm;
    uint64_t bits;
    const char test_filename[] = "test_putbits.bin";
    uint16_t bit_pattern[] = { 0xDEAD, 0xBEAF, 0xABAD, 0xCAFE };
    uint32_t bit_pattern_length = sizeof(bit_pattern) / sizeof(bit_pattern[0]);
    uint32_t i, is_ok;

    /* 書き込んでみる */
    strm = BitStream_Open(test_filename, "w", NULL, 0);
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (BitStream_PutBits(strm, 16, bit_pattern[i]) != BITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    BitStream_Close(strm);

    /* 正しく書き込めているか？ */
    strm = BitStream_Open(test_filename, "r", NULL, 0);
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (BitStream_GetBits(strm, 16, &bits) != BITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
      if ((uint16_t)bits != bit_pattern[i]) {
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    BitStream_Close(strm);
  }

  /* PutBit関数テスト（ワークメモリ渡し、ファイル） */
  {
    struct BitStream* strm;
    uint8_t bit;
    const char test_filename[] = "test_putbit.bin";
    uint8_t bit_pattern[] = { 1, 1, 1, 1, 0, 0, 0, 0 };
    uint32_t bit_pattern_length = sizeof(bit_pattern) / sizeof(bit_pattern[0]);
    uint32_t i, is_ok;
    void* work;

    /* 書き込んでみる */
    work = malloc(BitStream_CalculateWorkSize());
    strm = BitStream_Open(test_filename, "w", work, BitStream_CalculateWorkSize());
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (BitStream_PutBit(strm, bit_pattern[i]) != BITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    BitStream_Close(strm);
    free(work);

    /* 正しく書き込めているか？ */
    work = malloc(BitStream_CalculateWorkSize());
    strm = BitStream_Open(test_filename, "r", work, BitStream_CalculateWorkSize());
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (BitStream_GetBit(strm, &bit) != BITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
      if (bit != bit_pattern[i]) {
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    BitStream_Close(strm);
    free(work);
  }

  /* PutBits関数テスト（ワークメモリ渡し、ファイル） */
  {
    struct BitStream* strm;
    uint64_t bits;
    const char test_filename[] = "test_putbits.bin";
    uint16_t bit_pattern[] = { 0xDEAD, 0xBEAF, 0xABAD, 0xCAFE };
    uint32_t bit_pattern_length = sizeof(bit_pattern) / sizeof(bit_pattern[0]);
    uint32_t i, is_ok;
    void* work;

    /* 書き込んでみる */
    work = malloc(BitStream_CalculateWorkSize());
    strm = BitStream_Open(test_filename, "w", work, BitStream_CalculateWorkSize());
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (BitStream_PutBits(strm, 16, bit_pattern[i]) != BITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    BitStream_Close(strm);
    free(work);

    /* 正しく書き込めているか？ */
    work = malloc(BitStream_CalculateWorkSize());
    strm = BitStream_Open(test_filename, "r", work, BitStream_CalculateWorkSize());
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (BitStream_GetBits(strm, 16, &bits) != BITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
      if ((uint16_t)bits != bit_pattern[i]) {
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    BitStream_Close(strm);
    free(work);
  }

  /* PutBit関数テスト（メモリ） */
  {
    struct BitStream* strm;
    uint8_t bit;
    uint8_t bit_pattern[] = { 1, 1, 1, 1, 0, 0, 0, 0 };
    uint8_t memory_image[256];
    uint32_t bit_pattern_length = sizeof(bit_pattern) / sizeof(bit_pattern[0]);
    uint32_t i, is_ok;

    /* 書き込んでみる */
    strm = BitStream_OpenMemory(memory_image, sizeof(memory_image), "w", NULL, 0);
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (BitStream_PutBit(strm, bit_pattern[i]) != BITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    BitStream_Close(strm);

    /* 正しく書き込めているか？ */
    strm = BitStream_OpenMemory(memory_image, sizeof(memory_image), "r", NULL, 0);
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (BitStream_GetBit(strm, &bit) != BITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
      if (bit != bit_pattern[i]) {
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    BitStream_Close(strm);
  }

  /* PutBits関数テスト（ファイル） */
  {
    struct BitStream* strm;
    uint64_t bits;
    uint16_t bit_pattern[] = { 0xDEAD, 0xBEAF, 0xABAD, 0xCAFE };
    uint32_t bit_pattern_length = sizeof(bit_pattern) / sizeof(bit_pattern[0]);
    uint8_t memory_image[256];
    uint32_t i, is_ok;

    /* 書き込んでみる */
    strm = BitStream_OpenMemory(memory_image, sizeof(memory_image), "w", NULL, 0);
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (BitStream_PutBits(strm, 16, bit_pattern[i]) != BITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    BitStream_Close(strm);

    /* 正しく書き込めているか？ */
    strm = BitStream_OpenMemory(memory_image, sizeof(memory_image), "r", NULL, 0);
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (BitStream_GetBits(strm, 16, &bits) != BITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
      if ((uint16_t)bits != bit_pattern[i]) {
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    BitStream_Close(strm);
  }

  /* PutBit関数テスト（ワークメモリ渡し、ファイル） */
  {
    struct BitStream* strm;
    uint8_t bit;
    uint8_t bit_pattern[] = { 1, 1, 1, 1, 0, 0, 0, 0 };
    uint32_t bit_pattern_length = sizeof(bit_pattern) / sizeof(bit_pattern[0]);
    uint8_t memory_image[256];
    uint32_t i, is_ok;
    void* work;

    /* 書き込んでみる */
    work = malloc(BitStream_CalculateWorkSize());
    strm = BitStream_OpenMemory(memory_image,
        sizeof(memory_image), "w", work, BitStream_CalculateWorkSize());
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (BitStream_PutBit(strm, bit_pattern[i]) != BITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    BitStream_Close(strm);
    free(work);

    /* 正しく書き込めているか？ */
    work = malloc(BitStream_CalculateWorkSize());
    strm = BitStream_OpenMemory(memory_image,
        sizeof(memory_image), "r", work, BitStream_CalculateWorkSize());
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (BitStream_GetBit(strm, &bit) != BITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
      if (bit != bit_pattern[i]) {
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    BitStream_Close(strm);
    free(work);
  }

  /* PutBits関数テスト（ワークメモリ渡し、ファイル） */
  {
    struct BitStream* strm;
    uint64_t bits;
    uint8_t memory_image[256];
    uint16_t bit_pattern[] = { 0xDEAD, 0xBEAF, 0xABAD, 0xCAFE };
    uint32_t bit_pattern_length = sizeof(bit_pattern) / sizeof(bit_pattern[0]);
    uint32_t i, is_ok;
    void* work;

    /* 書き込んでみる */
    work = malloc(BitStream_CalculateWorkSize());
    strm = BitStream_OpenMemory(memory_image,
        sizeof(memory_image), "w", work, BitStream_CalculateWorkSize());
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (BitStream_PutBits(strm, 16, bit_pattern[i]) != BITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    BitStream_Close(strm);
    free(work);

    /* 正しく書き込めているか？ */
    work = malloc(BitStream_CalculateWorkSize());
    strm = BitStream_OpenMemory(memory_image,
        sizeof(memory_image), "r", work, BitStream_CalculateWorkSize());
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (BitStream_GetBits(strm, 16, &bits) != BITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
      if ((uint16_t)bits != bit_pattern[i]) {
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    BitStream_Close(strm);
    free(work);
  }

  /* Flushテスト（ファイル） */
  {
    struct BitStream* strm;
    const char test_filename[] = "test_flush.bin";
    uint64_t bits;

    strm = BitStream_Open(test_filename, "w", NULL, 0);
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, BitStream_PutBit(strm, 1));
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, BitStream_PutBit(strm, 1));
    /* 2bitしか書いていないがフラッシュ */
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, BitStream_Flush(strm));
    EXPECT_EQ(0, strm->bit_buffer);
    EXPECT_EQ(8, strm->bit_count);
    BitStream_Close(strm);

    /* 1バイトで先頭2bitだけが立っているはず */
    strm = BitStream_Open(test_filename, "r", NULL, 0);
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, BitStream_GetBits(strm, 8, &bits));
    EXPECT_EQ(0xC0, bits);
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, BitStream_Flush(strm));
    EXPECT_EQ(0, strm->bit_count);
    EXPECT_EQ(0xC0, strm->bit_buffer);
    BitStream_Close(strm);
  }

  /* Flushテスト（メモリ） */
  {
    struct BitStream* strm;
    uint8_t memory_image[256];
    uint64_t bits;

    strm = BitStream_OpenMemory(memory_image, sizeof(memory_image), "w", NULL, 0);
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, BitStream_PutBit(strm, 1));
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, BitStream_PutBit(strm, 1));
    /* 1bitしか書いていないがフラッシュ */
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, BitStream_Flush(strm));
    EXPECT_EQ(0, strm->bit_buffer);
    EXPECT_EQ(8, strm->bit_count);
    BitStream_Close(strm);

    /* 1バイトで先頭2bitだけが立っているはず */
    strm = BitStream_OpenMemory(memory_image, sizeof(memory_image), "r", NULL, 0);
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, BitStream_GetBits(strm, 8, &bits));
    EXPECT_EQ(0xC0, bits);
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, BitStream_Flush(strm));
    EXPECT_EQ(0, strm->bit_count);
    EXPECT_EQ(0xC0, strm->bit_buffer);
    BitStream_Close(strm);
  }

}

/* seek, ftellなどのストリーム操作系APIテスト */
TEST(BitStreamTest, FileOperationTest)
{
  /* Seek/Tellテスト */
  {
    struct BitStream* strm;
    BitStreamApiResult result;
    int32_t            tell_result;
    const char test_filename[] = "test_fileseek.bin";

    /* テスト用に適当にファイル作成 */
    strm = BitStream_Open(test_filename, "w", NULL, 0);
    ASSERT_TRUE(strm != NULL);
    BitStream_PutBits(strm, 32, 0xDEADBEAF);
    BitStream_PutBits(strm, 32, 0xABADCAFE);
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, BitStream_Tell(strm, &tell_result));
    EXPECT_EQ(8, tell_result);
    BitStream_Close(strm);

    strm = BitStream_Open(test_filename, "r", NULL, 0);

    result = BitStream_Seek(strm, 0, BITSTREAM_SEEK_SET);
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, result);
    result = BitStream_Tell(strm, &tell_result);
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, result);
    EXPECT_EQ(0, tell_result);

    result = BitStream_Seek(strm, 1, BITSTREAM_SEEK_CUR);
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, result);
    result = BitStream_Tell(strm, &tell_result);
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, result);
    EXPECT_EQ(1, tell_result);

    result = BitStream_Seek(strm, 2, BITSTREAM_SEEK_CUR);
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, result);
    result = BitStream_Tell(strm, &tell_result);
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, result);
    EXPECT_EQ(3, tell_result);

    /* 本当はプラットフォーム依存で不定値になるのでやりたくない */
    result = BitStream_Seek(strm, 0, BITSTREAM_SEEK_END);
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, result);
    result = BitStream_Tell(strm, &tell_result);
    EXPECT_EQ(BITSTREAM_APIRESULT_OK, result);
    EXPECT_EQ(8, tell_result);

    BitStream_Close(strm);
  }
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

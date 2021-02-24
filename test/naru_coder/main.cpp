#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

/* テスト対象のモジュール */
extern "C" {
#include "../../libs/naru_coder/src/naru_coder.c"
}

/* 再帰的ライス符号テスト */
TEST(NARUCoderTest, RecursiveRiceTest)
{
  /* 簡単に出力テスト */
  {
    uint32_t code;
    uint8_t data[16];
    struct NARUBitStream strm;
    NARURecursiveRiceParameter param_array[2] = {0, 0};

    /* 0を4回出力 */
    memset(data, 0, sizeof(data));
    NARUCODER_PARAMETER_SET(param_array, 0, 1);
    NARUCODER_PARAMETER_SET(param_array, 1, 1);
    NARUBitWriter_Open(&strm, data, sizeof(data));
    NARURecursiveRice_PutCode(&strm, param_array, 2, 0);
    NARURecursiveRice_PutCode(&strm, param_array, 2, 0);
    NARURecursiveRice_PutCode(&strm, param_array, 2, 0);
    NARURecursiveRice_PutCode(&strm, param_array, 2, 0);
    NARUBitStream_Close(&strm);

    /* 取得 */
    NARUCODER_PARAMETER_SET(param_array, 0, 1);
    NARUCODER_PARAMETER_SET(param_array, 1, 1);
    NARUBitReader_Open(&strm, data, sizeof(data));
    code = NARURecursiveRice_GetCode(&strm, param_array, 2);
    EXPECT_EQ(0, code);
    code = NARURecursiveRice_GetCode(&strm, param_array, 2);
    EXPECT_EQ(0, code);
    code = NARURecursiveRice_GetCode(&strm, param_array, 2);
    EXPECT_EQ(0, code);
    code = NARURecursiveRice_GetCode(&strm, param_array, 2);
    EXPECT_EQ(0, code);
    NARUBitStream_Close(&strm);

    /* 1を4回出力 */
    memset(data, 0, sizeof(data));
    NARUCODER_PARAMETER_SET(param_array, 0, 1);
    NARUCODER_PARAMETER_SET(param_array, 1, 1);
    NARUBitWriter_Open(&strm, data, sizeof(data));
    NARURecursiveRice_PutCode(&strm, param_array, 2, 1);
    NARURecursiveRice_PutCode(&strm, param_array, 2, 1);
    NARURecursiveRice_PutCode(&strm, param_array, 2, 1);
    NARURecursiveRice_PutCode(&strm, param_array, 2, 1);
    NARUBitStream_Close(&strm);

    /* 取得 */
    NARUCODER_PARAMETER_SET(param_array, 0, 1);
    NARUCODER_PARAMETER_SET(param_array, 1, 1);
    NARUBitReader_Open(&strm, data, sizeof(data));
    code = NARURecursiveRice_GetCode(&strm, param_array, 2);
    EXPECT_EQ(1, code);
    code = NARURecursiveRice_GetCode(&strm, param_array, 2);
    EXPECT_EQ(1, code);
    code = NARURecursiveRice_GetCode(&strm, param_array, 2);
    EXPECT_EQ(1, code);
    code = NARURecursiveRice_GetCode(&strm, param_array, 2);
    EXPECT_EQ(1, code);
    NARUBitStream_Close(&strm);


    /* パラメータを変えて0を4回出力 */
    memset(data, 0, sizeof(data));
    NARUCODER_PARAMETER_SET(param_array, 0, 2);
    NARUCODER_PARAMETER_SET(param_array, 1, 2);
    NARUBitWriter_Open(&strm, data, sizeof(data));
    NARURecursiveRice_PutCode(&strm, param_array, 2, 0);
    NARURecursiveRice_PutCode(&strm, param_array, 2, 0);
    NARURecursiveRice_PutCode(&strm, param_array, 2, 0);
    NARURecursiveRice_PutCode(&strm, param_array, 2, 0);
    NARUBitStream_Close(&strm);

    /* 取得 */
    NARUCODER_PARAMETER_SET(param_array, 0, 2);
    NARUCODER_PARAMETER_SET(param_array, 1, 2);
    NARUBitReader_Open(&strm, data, sizeof(data));
    code = NARURecursiveRice_GetCode(&strm, param_array, 2);
    EXPECT_EQ(0, code);
    code = NARURecursiveRice_GetCode(&strm, param_array, 2);
    EXPECT_EQ(0, code);
    code = NARURecursiveRice_GetCode(&strm, param_array, 2);
    EXPECT_EQ(0, code);
    code = NARURecursiveRice_GetCode(&strm, param_array, 2);
    EXPECT_EQ(0, code);
    NARUBitStream_Close(&strm);

    /* パラメータを変えて3を4回出力 */
    memset(data, 0, sizeof(data));
    NARUCODER_PARAMETER_SET(param_array, 0, 2);
    NARUCODER_PARAMETER_SET(param_array, 1, 2);
    NARUBitWriter_Open(&strm, data, sizeof(data));
    NARURecursiveRice_PutCode(&strm, param_array, 2, 3);
    NARURecursiveRice_PutCode(&strm, param_array, 2, 3);
    NARURecursiveRice_PutCode(&strm, param_array, 2, 3);
    NARURecursiveRice_PutCode(&strm, param_array, 2, 3);
    NARUBitStream_Close(&strm);

    /* 取得 */
    NARUCODER_PARAMETER_SET(param_array, 0, 2);
    NARUCODER_PARAMETER_SET(param_array, 1, 2);
    NARUBitReader_Open(&strm, data, sizeof(data));
    code = NARURecursiveRice_GetCode(&strm, param_array, 2);
    EXPECT_EQ(3, code);
    code = NARURecursiveRice_GetCode(&strm, param_array, 2);
    EXPECT_EQ(3, code);
    code = NARURecursiveRice_GetCode(&strm, param_array, 2);
    EXPECT_EQ(3, code);
    code = NARURecursiveRice_GetCode(&strm, param_array, 2);
    EXPECT_EQ(3, code);
    NARUBitStream_Close(&strm);
  }

  /* 長めの信号を出力してみる */
  {
#define TEST_OUTPUT_LENGTH (128)
    uint32_t i, code, is_ok;
    struct NARUBitStream strm;
    NARURecursiveRiceParameter param_array[3] = {0, 0, 0};
    uint32_t test_output_pattern[TEST_OUTPUT_LENGTH];
    uint8_t data[TEST_OUTPUT_LENGTH * 2];

    /* 出力の生成 */
    for (i = 0; i < TEST_OUTPUT_LENGTH; i++) {
      test_output_pattern[i] = i;
    }

    /* 出力 */
    NARUCODER_PARAMETER_SET(param_array, 0, 1);
    NARUCODER_PARAMETER_SET(param_array, 1, 1);
    NARUCODER_PARAMETER_SET(param_array, 2, 1);
    NARUBitWriter_Open(&strm, data, sizeof(data));
    for (i = 0; i < TEST_OUTPUT_LENGTH; i++) {
      NARURecursiveRice_PutCode(&strm,
          param_array, 3, test_output_pattern[i]);
    }
    NARUBitStream_Close(&strm);

    /* 取得 */
    NARUCODER_PARAMETER_SET(param_array, 0, 1);
    NARUCODER_PARAMETER_SET(param_array, 1, 1);
    NARUCODER_PARAMETER_SET(param_array, 2, 1);
    NARUBitReader_Open(&strm, data, sizeof(data));
    is_ok = 1;
    for (i = 0; i < TEST_OUTPUT_LENGTH; i++) {
      code = NARURecursiveRice_GetCode(&strm, param_array, 3);
      if (code != test_output_pattern[i]) {
        printf("actual:%d != test:%d \n", code, test_output_pattern[i]);
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    NARUBitStream_Close(&strm);
#undef TEST_OUTPUT_LENGTH
  }

  /* 長めの信号を出力してみる（乱数） */
  {
#define TEST_OUTPUT_LENGTH (128)
    uint32_t i, code, is_ok;
    struct NARUBitStream strm;
    NARURecursiveRiceParameter param_array[3] = {0, 0, 0};
    uint32_t test_output_pattern[TEST_OUTPUT_LENGTH];
    uint8_t data[TEST_OUTPUT_LENGTH * 2];

    /* 出力の生成 */
    srand(0);
    for (i = 0; i < TEST_OUTPUT_LENGTH; i++) {
      test_output_pattern[i] = rand() % 0xFF;
    }

    /* 出力 */
    NARUCODER_PARAMETER_SET(param_array, 0, 1);
    NARUCODER_PARAMETER_SET(param_array, 1, 1);
    NARUCODER_PARAMETER_SET(param_array, 2, 1);
    NARUBitWriter_Open(&strm, data, sizeof(data));
    for (i = 0; i < TEST_OUTPUT_LENGTH; i++) {
      NARURecursiveRice_PutCode(&strm,
          param_array, 3, test_output_pattern[i]);
    }
    NARUBitStream_Close(&strm);

    /* 取得 */
    NARUCODER_PARAMETER_SET(param_array, 0, 1);
    NARUCODER_PARAMETER_SET(param_array, 1, 1);
    NARUCODER_PARAMETER_SET(param_array, 2, 1);
    NARUBitReader_Open(&strm, data, sizeof(data));
    is_ok = 1;
    for (i = 0; i < TEST_OUTPUT_LENGTH; i++) {
      code = NARURecursiveRice_GetCode(&strm, param_array, 3);
      if (code != test_output_pattern[i]) {
        printf("actual:%d != test:%d \n", code, test_output_pattern[i]);
        is_ok = 0;
        break;
      }
    }
    EXPECT_EQ(1, is_ok);
    NARUBitStream_Close(&strm);
#undef TEST_OUTPUT_LENGTH
  }

  /* 実データを符号化してみる */
  {
    uint32_t i, encsize;
    struct stat fstat;
    const char* test_infile_name  = "PriChanIcon.png";
    uint8_t*    fileimg;
    uint8_t*    encimg;
    uint8_t*    decimg;
    FILE*       fp;
    struct NARUBitStream strm;
    NARURecursiveRiceParameter param_array[8];
    const uint32_t num_params = sizeof(param_array) / sizeof(param_array[0]);

    /* 入力データ読み出し */
    stat(test_infile_name, &fstat);
    fileimg = (uint8_t *)malloc(fstat.st_size);
    encimg  = (uint8_t *)malloc(2 * fstat.st_size);  /* PNG画像のため増えることを想定 */
    decimg  = (uint8_t *)malloc(fstat.st_size);
    fp = fopen(test_infile_name, "rb");
    fread(fileimg, sizeof(uint8_t), fstat.st_size, fp);
    fclose(fp);

    /* 書き込み */
    NARUBitWriter_Open(&strm, encimg, 2 * fstat.st_size);
    for (i = 0; i < num_params; i++) {
      NARUCODER_PARAMETER_SET(param_array, i, 1);
    }
    for (i = 0; i < fstat.st_size; i++) {
      NARURecursiveRice_PutCode(&strm, param_array, num_params, fileimg[i]);
    }
    NARUBitStream_Flush(&strm);
    NARUBitStream_Tell(&strm, (int32_t *)&encsize);
    NARUBitStream_Close(&strm);

    /* 読み込み */
    NARUBitReader_Open(&strm, encimg, encsize);
    for (i = 0; i < num_params; i++) {
      NARUCODER_PARAMETER_SET(param_array, i, 1);
    }
    for (i = 0; i < fstat.st_size; i++) {
      decimg[i] = (uint8_t)NARURecursiveRice_GetCode(&strm, param_array, num_params);
    }
    NARUBitStream_Close(&strm);

    /* 一致確認 */
    EXPECT_EQ(0, memcmp(fileimg, decimg, sizeof(uint8_t) * fstat.st_size));

    free(decimg);
    free(fileimg);
  }

}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

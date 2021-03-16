#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

/* テスト対象のモジュール */
extern "C" {
#include "../../libs/naru_coder/src/naru_coder.c"
}

/* ハンドル作成破棄テスト */
TEST(NARUCoderTest, CreateDestroyHandleTest)
{
  /* ワークサイズ計算テスト */
  {
    int32_t work_size;

    /* 最低限構造体本体よりは大きいはず */
    work_size = NARUCoder_CalculateWorkSize(1, 1);
    ASSERT_TRUE(work_size > sizeof(struct NARUCoder));

    /* 不正なコンフィグ */
    EXPECT_TRUE(NARUCoder_CalculateWorkSize(0, 1) < 0);
    EXPECT_TRUE(NARUCoder_CalculateWorkSize(1, 0) < 0);
  }

  /* ワーク領域渡しによるハンドル作成（成功例） */
  {
    void *work;
    int32_t work_size;
    struct NARUCoder *coder;

    work_size = NARUCoder_CalculateWorkSize(1, 1);
    work = malloc(work_size);

    coder = NARUCoder_Create(1, 1, work, work_size);
    ASSERT_TRUE(coder != NULL);
    EXPECT_TRUE(coder->work == work);
    EXPECT_EQ(coder->alloced_by_own, 0);
    EXPECT_EQ(coder->max_num_channels, 1);
    EXPECT_EQ(coder->max_num_parameters, 1);
    EXPECT_TRUE(coder->rice_parameter != NULL);
    EXPECT_TRUE(coder->rice_parameter[0] != NULL);
    EXPECT_TRUE(coder->init_rice_parameter != NULL);
    EXPECT_TRUE(coder->init_rice_parameter[0] != NULL);

    NARUCoder_Destroy(coder);
    free(work);
  }

  /* 自前確保によるハンドル作成（成功例） */
  {
    struct NARUCoder *coder;

    coder = NARUCoder_Create(1, 1, NULL, 0);
    ASSERT_TRUE(coder != NULL);
    EXPECT_TRUE(coder->work != NULL);
    EXPECT_EQ(coder->alloced_by_own, 1);
    EXPECT_EQ(coder->max_num_channels, 1);
    EXPECT_EQ(coder->max_num_parameters, 1);
    EXPECT_TRUE(coder->rice_parameter != NULL);
    EXPECT_TRUE(coder->rice_parameter[0] != NULL);
    EXPECT_TRUE(coder->init_rice_parameter != NULL);
    EXPECT_TRUE(coder->init_rice_parameter[0] != NULL);

    NARUCoder_Destroy(coder);
  }

  /* ワーク領域渡しによるハンドル作成（失敗ケース） */
  {
    void *work;
    int32_t work_size;
    struct NARUCoder *coder;

    work_size = NARUCoder_CalculateWorkSize(1, 1);
    work = malloc(work_size);

    /* 引数が不正 */
    coder = NARUCoder_Create(1, 1, NULL, work_size);
    EXPECT_TRUE(coder == NULL);
    coder = NARUCoder_Create(1, 1, work, 0);
    EXPECT_TRUE(coder == NULL);

    /* ワークサイズ不足 */
    coder = NARUCoder_Create(1, 1, work, work_size - 1);
    EXPECT_TRUE(coder == NULL);

    /* 生成コンフィグが不正 */
    coder = NARUCoder_Create(0, 1, work, work_size);
    EXPECT_TRUE(coder == NULL);
    coder = NARUCoder_Create(1, 0, work, work_size);
    EXPECT_TRUE(coder == NULL);
  }

  /* 自前確保によるハンドル作成（失敗ケース） */
  {
    struct NARUCoder *coder;

    /* 生成コンフィグが不正 */
    coder = NARUCoder_Create(0, 1, NULL, 0);
    EXPECT_TRUE(coder == NULL);
    coder = NARUCoder_Create(1, 0, NULL, 0);
    EXPECT_TRUE(coder == NULL);
  }
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

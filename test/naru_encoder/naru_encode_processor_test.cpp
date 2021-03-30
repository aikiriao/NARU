#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

/* テスト対象のモジュール */
extern "C" {
#include "../../libs/naru_encoder/src/naru_encode_processor.c"
}

/* ハンドル作成破棄テスト */
TEST(NARUEncodeProcessorTest, CreateDestroyHandleTest)
{
  /* ワークサイズ計算テスト */
  {
    int32_t work_size;

    /* 最低限構造体本体よりは大きいはず */
    work_size = NARUEncodeProcessor_CalculateWorkSize(1);
    ASSERT_TRUE(work_size > sizeof(struct NARUEncodeProcessor));

    /* 不正な引数 */
    EXPECT_TRUE(NARUEncodeProcessor_CalculateWorkSize(0) < 0);
    EXPECT_TRUE(NARUEncodeProcessor_CalculateWorkSize(3) < 0);
  }

  /* ワーク領域渡しによるハンドル作成（成功例） */
  {
    void *work;
    int32_t work_size;
    struct NARUEncodeProcessor *processor;

    work_size = NARUEncodeProcessor_CalculateWorkSize(1);
    work = malloc(work_size);

    processor = NARUEncodeProcessor_Create(1, work, work_size);
    ASSERT_TRUE(processor != NULL);
    EXPECT_TRUE(processor->ngsa != NULL);
    EXPECT_TRUE(processor->sa != NULL);

    NARUEncodeProcessor_Destroy(processor);
    free(work);
  }

  /* ワーク領域渡しによるハンドル作成（失敗ケース） */
  {
    void *work;
    int32_t work_size;
    struct NARUEncodeProcessor *processor;

    work_size = NARUEncodeProcessor_CalculateWorkSize(4);
    work = malloc(work_size);

    /* 引数が不正 */
    processor = NARUEncodeProcessor_Create(0, work, work_size);
    EXPECT_TRUE(processor == NULL);
    processor = NARUEncodeProcessor_Create(4, NULL, work_size);
    EXPECT_TRUE(processor == NULL);
    processor = NARUEncodeProcessor_Create(4, work, 0);
    EXPECT_TRUE(processor == NULL);

    /* ワークサイズ不足 */
    processor = NARUEncodeProcessor_Create(4, work, work_size - 1);
    EXPECT_TRUE(processor == NULL);
    processor = NARUEncodeProcessor_Create(8, work, work_size);
    EXPECT_TRUE(processor == NULL);

    /* コンフィグが不正 */
    processor = NARUEncodeProcessor_Create(3, work, work_size);
    EXPECT_TRUE(processor == NULL);

    free(work);
  }
}

#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

/* テスト対象のモジュール */
extern "C" {
#include "../../libs/naru_decoder/src/naru_decode_processor.c"
}

/* ハンドル作成破棄テスト */
TEST(NARUDecodeProcessorTest, CreateDestroyHandleTest)
{
    /* ワークサイズ計算テスト */
    {
        int32_t work_size;

        /* 最低限構造体本体よりは大きいはず */
        work_size = NARUDecodeProcessor_CalculateWorkSize(1);
        ASSERT_TRUE(work_size > sizeof(struct NARUDecodeProcessor));

        /* 不正な引数 */
        EXPECT_TRUE(NARUDecodeProcessor_CalculateWorkSize(0) < 0);
        EXPECT_TRUE(NARUDecodeProcessor_CalculateWorkSize(3) < 0);
    }

    /* ワーク領域渡しによるハンドル作成（成功例） */
    {
        void *work;
        int32_t work_size;
        struct NARUDecodeProcessor *processor;

        work_size = NARUDecodeProcessor_CalculateWorkSize(1);
        work = malloc(work_size);

        processor = NARUDecodeProcessor_Create(1, work, work_size);
        ASSERT_TRUE(processor != NULL);
        EXPECT_TRUE(processor->ngsa != NULL);
        EXPECT_TRUE(processor->sa != NULL);

        NARUDecodeProcessor_Destroy(processor);
        free(work);
    }

    /* ワーク領域渡しによるハンドル作成（失敗ケース） */
    {
        void *work;
        int32_t work_size;
        struct NARUDecodeProcessor *processor;

        work_size = NARUDecodeProcessor_CalculateWorkSize(4);
        work = malloc(work_size);

        /* 引数が不正 */
        processor = NARUDecodeProcessor_Create(0, work, work_size);
        EXPECT_TRUE(processor == NULL);
        processor = NARUDecodeProcessor_Create(4, NULL, work_size);
        EXPECT_TRUE(processor == NULL);
        processor = NARUDecodeProcessor_Create(4, work, 0);
        EXPECT_TRUE(processor == NULL);

        /* ワークサイズ不足 */
        processor = NARUDecodeProcessor_Create(4, work, work_size - 1);
        EXPECT_TRUE(processor == NULL);
        processor = NARUDecodeProcessor_Create(8, work, work_size);
        EXPECT_TRUE(processor == NULL);

        /* コンフィグが不正 */
        processor = NARUDecodeProcessor_Create(3, work, work_size);
        EXPECT_TRUE(processor == NULL);

        free(work);
    }
}

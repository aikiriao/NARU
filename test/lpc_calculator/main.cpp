#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

/* テスト対象のモジュール */
extern "C" {
#include "../../libs/lpc_calculator/src/lpc_calculator.c"
}

/* ハンドル作成破棄テスト */
TEST(LPCCalculatorTest, CreateDestroyHandleTest)
{
    /* ワークサイズ計算テスト */
    {
        int32_t work_size;

        /* 最低限構造体本体よりは大きいはず */
        work_size = LPCCalculator_CalculateWorkSize(1);
        ASSERT_TRUE(work_size > sizeof(struct LPCCalculator));

        /* 不正なコンフィグ */
        EXPECT_TRUE(LPCCalculator_CalculateWorkSize(0) < 0);
    }

    /* ワーク領域渡しによるハンドル作成（成功例） */
    {
        void *work;
        int32_t work_size;
        struct LPCCalculator *lpcc;

        work_size = LPCCalculator_CalculateWorkSize(4);
        work = malloc(work_size);

        lpcc = LPCCalculator_Create(4, work, work_size);
        ASSERT_TRUE(lpcc != NULL);
        EXPECT_TRUE(lpcc->work == work);
        EXPECT_EQ(lpcc->alloced_by_own, 0);

        LPCCalculator_Destroy(lpcc);
        free(work);
    }

    /* 自前確保によるハンドル作成（成功例） */
    {
        struct LPCCalculator *lpcc;

        lpcc = LPCCalculator_Create(4, NULL, 0);
        ASSERT_TRUE(lpcc != NULL);
        EXPECT_TRUE(lpcc->work != NULL);
        EXPECT_EQ(lpcc->alloced_by_own, 1);

        LPCCalculator_Destroy(lpcc);
    }

    /* ワーク領域渡しによるハンドル作成（失敗ケース） */
    {
        void *work;
        int32_t work_size;
        struct LPCCalculator *lpcc;

        work_size = LPCCalculator_CalculateWorkSize(4);
        work = malloc(work_size);

        /* 引数が不正 */
        lpcc = LPCCalculator_Create(4, NULL, work_size);
        EXPECT_TRUE(lpcc == NULL);
        lpcc = LPCCalculator_Create(4, work, 0);
        EXPECT_TRUE(lpcc == NULL);

        /* コンフィグパラメータが不正 */
        lpcc = LPCCalculator_Create(0, work, work_size);
        EXPECT_TRUE(lpcc == NULL);

        free(work);
    }

    /* 自前確保によるハンドル作成（失敗ケース） */
    {
        struct LPCCalculator *lpcc;

        /* コンフィグパラメータが不正 */
        lpcc = LPCCalculator_Create(0, NULL, 0);
        EXPECT_TRUE(lpcc == NULL);
    }
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

/* テスト対象のモジュール */
extern "C" {
#include "../../libs/naru_internal/src/naru_utility.c"
}

/* データビット幅取得テスト */
TEST(NARUUtilityTest, GetDataBitWidthTest)
{
  uint32_t bitwidth;

  /* 0のみの場合: 1が正解 */
  {
    const int32_t testcase[] = { 0, 0, 0 };
    bitwidth = NARUUtility_GetDataBitWidth(testcase, sizeof(testcase) / sizeof(testcase[0]));
    EXPECT_EQ(1, bitwidth);
  }

  /* 0と1が混在: 符号を含めるので2が正解 */
  {
    const int32_t testcase[] = { 1, 0, 0 };
    bitwidth = NARUUtility_GetDataBitWidth(testcase, sizeof(testcase) / sizeof(testcase[0]));
    EXPECT_EQ(2, bitwidth);
  }

  /* 0, 1, -1: 2が正解 */
  {
    const int32_t testcase[] = { 1, -1, 0 };
    bitwidth = NARUUtility_GetDataBitWidth(testcase, sizeof(testcase) / sizeof(testcase[0]));
    EXPECT_EQ(2, bitwidth);
  }

  /* 1, -1, -2: 2が正解 */
  {
    const int32_t testcase[] = { 1, -1, -2 };
    bitwidth = NARUUtility_GetDataBitWidth(testcase, sizeof(testcase) / sizeof(testcase[0]));
    EXPECT_EQ(2, bitwidth);
  }

  /* 2, -1, -2: 3が正解 */
  {
    const int32_t testcase[] = { 2, -1, -2 };
    bitwidth = NARUUtility_GetDataBitWidth(testcase, sizeof(testcase) / sizeof(testcase[0]));
    EXPECT_EQ(3, bitwidth);
  }

  /* 2, -1, -3: 3が正解 */
  {
    const int32_t testcase[] = { 0, -1, -3 };
    bitwidth = NARUUtility_GetDataBitWidth(testcase, sizeof(testcase) / sizeof(testcase[0]));
    EXPECT_EQ(3, bitwidth);
  }

  /* 8bit符号付き整数の限界値 */
  {
    const int32_t testcase[] = { INT8_MAX, INT8_MIN, 0 };
    bitwidth = NARUUtility_GetDataBitWidth(testcase, sizeof(testcase) / sizeof(testcase[0]));
    EXPECT_EQ(8, bitwidth);
  }

  /* 8bit符号付き整数の限界値-1 */
  {
    const int32_t testcase[] = { INT8_MAX - 1, INT8_MIN, 0 };
    bitwidth = NARUUtility_GetDataBitWidth(testcase, sizeof(testcase) / sizeof(testcase[0]));
    EXPECT_EQ(8, bitwidth);
  }

  /* 8bit符号付き整数の限界値-1 */
  {
    const int32_t testcase[] = { INT8_MAX, INT8_MIN + 1, 0 };
    bitwidth = NARUUtility_GetDataBitWidth(testcase, sizeof(testcase) / sizeof(testcase[0]));
    EXPECT_EQ(8, bitwidth);
  }

  /* 8bit符号付き整数の限界値+1 */
  {
    const int32_t testcase[] = { INT8_MAX + 1, INT8_MIN, 0 };
    bitwidth = NARUUtility_GetDataBitWidth(testcase, sizeof(testcase) / sizeof(testcase[0]));
    EXPECT_EQ(9, bitwidth);
  }

  /* 8bit符号付き整数の限界値+1 */
  {
    const int32_t testcase[] = { INT8_MAX, INT8_MIN - 1, 0 };
    bitwidth = NARUUtility_GetDataBitWidth(testcase, sizeof(testcase) / sizeof(testcase[0]));
    EXPECT_EQ(9, bitwidth);
  }

  /* 16bit符号付き整数の限界値 */
  {
    const int32_t testcase[] = { INT16_MAX, INT16_MIN, 0 };
    bitwidth = NARUUtility_GetDataBitWidth(testcase, sizeof(testcase) / sizeof(testcase[0]));
    EXPECT_EQ(16, bitwidth);
  }

  /* 16bit符号付き整数の限界値-1 */
  {
    const int32_t testcase[] = { INT16_MAX - 1, INT16_MIN, 0 };
    bitwidth = NARUUtility_GetDataBitWidth(testcase, sizeof(testcase) / sizeof(testcase[0]));
    EXPECT_EQ(16, bitwidth);
  }

  /* 16bit符号付き整数の限界値-1 */
  {
    const int32_t testcase[] = { INT16_MAX, INT16_MIN + 1, 0 };
    bitwidth = NARUUtility_GetDataBitWidth(testcase, sizeof(testcase) / sizeof(testcase[0]));
    EXPECT_EQ(16, bitwidth);
  }

  /* 16bit符号付き整数の限界値+1 */
  {
    const int32_t testcase[] = { INT16_MAX + 1, INT16_MIN, 0 };
    bitwidth = NARUUtility_GetDataBitWidth(testcase, sizeof(testcase) / sizeof(testcase[0]));
    EXPECT_EQ(17, bitwidth);
  }

  /* 16bit符号付き整数の限界値+1 */
  {
    const int32_t testcase[] = { INT16_MAX, INT16_MIN - 1, 0 };
    bitwidth = NARUUtility_GetDataBitWidth(testcase, sizeof(testcase) / sizeof(testcase[0]));
    EXPECT_EQ(17, bitwidth);
  }

  /* 32bit符号付き整数の限界値 */
  {
    const int32_t testcase[] = { INT32_MAX, INT32_MIN, 0 };
    bitwidth = NARUUtility_GetDataBitWidth(testcase, sizeof(testcase) / sizeof(testcase[0]));
    EXPECT_EQ(32, bitwidth);
  }

  /* 32bit符号付き整数の限界値-1 */
  {
    const int32_t testcase[] = { INT32_MAX - 1, INT16_MIN + 1, 0 };
    bitwidth = NARUUtility_GetDataBitWidth(testcase, sizeof(testcase) / sizeof(testcase[0]));
    EXPECT_EQ(32, bitwidth);
  }
}

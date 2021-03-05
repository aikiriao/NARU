#include "naru_utility.h"
#include "naru_internal.h"

#include <math.h>
#include <stdlib.h>

/* NLZ計算のためのテーブル */
#define UNUSED 99
static const uint32_t st_nlz10_table[64] = {
      32,     20,     19, UNUSED, UNUSED,     18, UNUSED,      7,
      10,     17, UNUSED, UNUSED,     14, UNUSED,      6, UNUSED,
  UNUSED,      9, UNUSED,     16, UNUSED, UNUSED,      1,     26,
  UNUSED,     13, UNUSED, UNUSED,     24,      5, UNUSED, UNUSED,
  UNUSED,     21, UNUSED,      8,     11, UNUSED,     15, UNUSED,
  UNUSED, UNUSED, UNUSED,      2,     27,      0,     25, UNUSED,
      22, UNUSED,     12, UNUSED, UNUSED,      3,     28, UNUSED,
      23, UNUSED,      4,     29, UNUSED, UNUSED,     30,     31
};
#undef UNUSED

/* 窓の適用 */
void NARUUtility_ApplyWindow(const double* window, double* data, uint32_t num_samples)
{
  uint32_t smpl;

  NARU_ASSERT(window != NULL && data != NULL);

  for (smpl = 0; smpl < num_samples; smpl++) {
    data[smpl] *= window[smpl];
  }
}

/* サイン窓を作成 */
void NARUUtility_MakeSinWindow(double* window, uint32_t window_size)
{
  uint32_t  smpl;
  double    x;

  NARU_ASSERT(window != NULL);
  NARU_ASSERT(window_size > 0);

  /* 0除算対策 */
  if (window_size == 1) {
    window[0] = 1.0f;
    return;
  }

  for (smpl = 0; smpl < window_size; smpl++) {
    x = (double)smpl / (window_size - 1);
    window[smpl] = sin(NARUUTILITY_PI * x);
  }
}

/* NLZ（最上位ビットから1に当たるまでのビット数）の計算 */
uint32_t NARUUtility_NLZSoft(uint32_t x)
{
  /* ハッカーのたのしみ参照 */
  x = x | (x >> 1);
  x = x | (x >> 2);
  x = x | (x >> 4);
  x = x | (x >> 8);
  x = x & ~(x >> 16);
  x = (x << 9) - x;
  x = (x << 11) - x;
  x = (x << 14) - x;
  return st_nlz10_table[x >> 26];
}

/* 2の冪乗数に切り上げる */
uint32_t NARUUtility_RoundUp2PoweredSoft(uint32_t val)
{
  /* ハッカーのたのしみ参照 */
  val--;
  val |= val >> 1;
  val |= val >> 2;
  val |= val >> 4;
  val |= val >> 8;
  val |= val >> 16;
  return val + 1;
}

/* LR -> MS（int32_t） */
void NARUUtility_LRtoMSInt32(int32_t **data, uint32_t num_samples)
{
  uint32_t  smpl;
  int32_t   mid, side;

  NARU_ASSERT(data != NULL);
  NARU_ASSERT(data[0] != NULL);
  NARU_ASSERT(data[1] != NULL);

  for (smpl = 0; smpl < num_samples; smpl++) {
    mid   = (data[0][smpl] + data[1][smpl]) >> 1; /* 注意: 右シフト必須(/2ではだめ。0方向に丸められる) */
    side  = data[0][smpl] - data[1][smpl];
    /* 戻るかその場で確認 */
    NARU_ASSERT(data[0][smpl] == ((((mid << 1) | (side & 1)) + side) >> 1));
    NARU_ASSERT(data[1][smpl] == ((((mid << 1) | (side & 1)) - side) >> 1));
    data[0][smpl] = mid; 
    data[1][smpl] = side;
  }
}

/* MS -> LR（int32_t） */
void NARUUtility_MStoLRInt32(int32_t **data, uint32_t num_samples)
{
  uint32_t  smpl;
  int32_t   mid, side;

  NARU_ASSERT(data != NULL);
  NARU_ASSERT(data[0] != NULL);
  NARU_ASSERT(data[1] != NULL);

  for (smpl = 0; smpl < num_samples; smpl++) {
    side  = data[1][smpl];
    mid   = (data[0][smpl] << 1) | (side & 1);
    data[0][smpl] = (mid + side) >> 1;
    data[1][smpl] = (mid - side) >> 1;
  }
}

/* round関数（C89で定義されてない） */
double NARUUtility_Round(double d)
{
    return (d >= 0.0f) ? floor(d + 0.5f) : -floor(-d + 0.5f);
}

/* log2関数（C89で定義されていない） */
double NARUUtility_Log2(double x)
{
#define INV_LOGE2 (1.4426950408889634)  /* 1 / log(2) */
  return log(x) * INV_LOGE2;
#undef INV_LOGE2
}

/* 入力データをもれなく表現できるビット幅の取得 */
uint32_t NARUUtility_GetDataBitWidth(
    const int32_t* data, uint32_t num_samples)
{
  uint32_t smpl;
  uint32_t maxabs, abs;

  NARU_ASSERT(data != NULL);

  /* 最大絶対値の計測 */
  maxabs = 0;
  for (smpl = 0; smpl < num_samples; smpl++) {
    abs = (uint32_t)NARUUTILITY_ABS(data[smpl]);
    if (abs > maxabs) {
      maxabs = abs;
    }
  }

  /* 符号ビットを付け加えてビット幅とする */
  return (maxabs > 0) ? (NARUUTILITY_LOG2CEIL(maxabs) + 1) : 1;
}

#ifndef NARU_UTILITY_H_INCLUDED
#define NARU_UTILITY_H_INCLUDED

#include "NARUStdint.h"

/* 円周率 */
#define NARU_PI              3.1415926535897932384626433832795029
/* 未使用引数 */
#define NARUUTILITY_UNUSED_ARGUMENT(arg)  ((void)(arg))
/* 算術右シフト */
#if ((((int32_t)-1) >> 1) == ((int32_t)-1))
/* 算術右シフトが有効な環境では、そのまま右シフト */
#define NARUUTILITY_SHIFT_RIGHT_ARITHMETIC(sint32, rshift) ((sint32) >> (rshift))
#else
/* 算術右シフトが無効な環境では、自分で定義する ハッカーのたのしみのより引用 */
/* 注意）有効範囲:0 <= rshift <= 32 */
#define NARUUTILITY_SHIFT_RIGHT_ARITHMETIC(sint32, rshift) ((((uint64_t)(sint32) + 0x80000000UL) >> (rshift)) - (0x80000000UL >> (rshift)))
#endif
/* 符号関数 ハッカーのたのしみより引用 補足）val==0の時は0を返す */
#define NARUUTILITY_SIGN(val)  (int32_t)((-(((uint32_t)(val)) >> 31)) | (((uint32_t)-(val)) >> 31))
/* 最大値の取得 */
#define NARUUTILITY_MAX(a,b) (((a) > (b)) ? (a) : (b))
/* 最小値の取得 */
#define NARUUTILITY_MIN(a,b) (((a) < (b)) ? (a) : (b))
/* 最小値以上最小値以下に制限 */
#define NARUUTILITY_INNER_VALUE(val, min, max) (NARUUTILITY_MIN((max), NARUUTILITY_MAX((min), (val))))
/* 2の冪乗か？ */
#define NARUUTILITY_IS_POWERED_OF_2(val) (!((val) & ((val) - 1)))
/* 符号付き32bit数値を符号なし32bit数値に一意変換 */
#define NARUUTILITY_SINT32_TO_UINT32(sint) (((int32_t)(sint) < 0) ? ((uint32_t)((-((sint) << 1)) - 1)) : ((uint32_t)(((sint) << 1))))
/* 符号なし32bit数値を符号付き32bit数値に一意変換 */
#define NARUUTILITY_UINT32_TO_SINT32(uint) ((int32_t)((uint) >> 1) ^ -(int32_t)((uint) & 1))
/* 絶対値の取得 */
#define NARUUTILITY_ABS(val)               (((val) > 0) ? (val) : -(val))
/* 32bit整数演算のための右シフト量を計算 */
#define NARUUTILITY_CALC_RSHIFT_FOR_SINT32(bitwidth) (((bitwidth) > 16) ? ((bitwidth) - 16) : 0)

/* NLZ（最上位ビットから1に当たるまでのビット数）の計算 */
#if defined(__GNUC__)
/* ビルトイン関数を使用 */
#define NARUUTILITY_NLZ(x) (((x) > 0) ? (uint32_t)__builtin_clz(x) : 32U)
#else
/* ソフトウェア実装を使用 */
#define NARUUTILITY_NLZ(x) NARUUtility_NLZSoft(x)
#endif

/* ceil(log2(val))の計算 */
#define NARUUTILITY_LOG2CEIL(x) (32U - NARUUTILITY_NLZ((uint32_t)((x) - 1U)))
/* floor(log2(val))の計算 */
#define NARUUTILITY_LOG2FLOOR(x) (31U - NARUUTILITY_NLZ(x))

/* 2の冪乗数(1,2,4,8,16,...)への切り上げ */
#if defined(__GNUC__)
/* ビルトイン関数を使用 */
#define NARUUTILITY_ROUNDUP2POWERED(x) (1U << NARUUTILITY_LOG2CEIL(x))
#else 
/* ソフトウェア実装を使用 */
#define NARUUTILITY_ROUNDUP2POWERED(x) NARUUtility_RoundUp2PoweredSoft(x)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 窓の適用 */
void NARUUtility_ApplyWindow(const double* window, double* data, uint32_t num_samples);

/* サイン窓を作成 */
void NARUUtility_MakeSinWindow(double* window, uint32_t window_size);

/* NLZ（最上位ビットから1に当たるまでのビット数）の計算 */
uint32_t NARUUtility_NLZSoft(uint32_t val);

/* 2の冪乗に切り上げる */
uint32_t NARUUtility_RoundUp2PoweredSoft(uint32_t val);

/* LR -> MS（double） */
void NARUUtility_LRtoMSDouble(double **data, uint32_t num_channels, uint32_t num_samples);

/* LR -> MS（int32_t） */
void NARUUtility_LRtoMSInt32(int32_t **data, uint32_t num_channels, uint32_t num_samples);

/* MS -> LR（int32_t） */
void NARUUtility_MStoLRInt32(int32_t **data, uint32_t num_channels, uint32_t num_samples);

/* round関数（C89で定義されてない） */
double NARUUtility_Round(double d);

/* log2関数（C89で定義されていない） */
double NARUUtility_Log2(double x);

/* 入力データをもれなく表現できるビット幅の取得 */
uint32_t NARUUtility_GetDataBitWidth(
    const int32_t* data, uint32_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* NARU_UTILITY_H_INCLUDED */

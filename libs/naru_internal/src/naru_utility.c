#include "naru_utility.h"
#include "naru_internal.h"

#include <math.h>
#include <stdlib.h>

/* CRC16(IBM:多項式0x8005を反転した0xa001によるもの) の計算用テーブル */
static const uint16_t st_crc16_ibm_byte_table[0x100] = {
    0x0000, 0xc0c1, 0xc181, 0x0140, 0xc301, 0x03c0, 0x0280, 0xc241,
    0xc601, 0x06c0, 0x0780, 0xc741, 0x0500, 0xc5c1, 0xc481, 0x0440,
    0xcc01, 0x0cc0, 0x0d80, 0xcd41, 0x0f00, 0xcfc1, 0xce81, 0x0e40,
    0x0a00, 0xcac1, 0xcb81, 0x0b40, 0xc901, 0x09c0, 0x0880, 0xc841,
    0xd801, 0x18c0, 0x1980, 0xd941, 0x1b00, 0xdbc1, 0xda81, 0x1a40,
    0x1e00, 0xdec1, 0xdf81, 0x1f40, 0xdd01, 0x1dc0, 0x1c80, 0xdc41,
    0x1400, 0xd4c1, 0xd581, 0x1540, 0xd701, 0x17c0, 0x1680, 0xd641,
    0xd201, 0x12c0, 0x1380, 0xd341, 0x1100, 0xd1c1, 0xd081, 0x1040,
    0xf001, 0x30c0, 0x3180, 0xf141, 0x3300, 0xf3c1, 0xf281, 0x3240,
    0x3600, 0xf6c1, 0xf781, 0x3740, 0xf501, 0x35c0, 0x3480, 0xf441,
    0x3c00, 0xfcc1, 0xfd81, 0x3d40, 0xff01, 0x3fc0, 0x3e80, 0xfe41,
    0xfa01, 0x3ac0, 0x3b80, 0xfb41, 0x3900, 0xf9c1, 0xf881, 0x3840,
    0x2800, 0xe8c1, 0xe981, 0x2940, 0xeb01, 0x2bc0, 0x2a80, 0xea41,
    0xee01, 0x2ec0, 0x2f80, 0xef41, 0x2d00, 0xedc1, 0xec81, 0x2c40,
    0xe401, 0x24c0, 0x2580, 0xe541, 0x2700, 0xe7c1, 0xe681, 0x2640,
    0x2200, 0xe2c1, 0xe381, 0x2340, 0xe101, 0x21c0, 0x2080, 0xe041,
    0xa001, 0x60c0, 0x6180, 0xa141, 0x6300, 0xa3c1, 0xa281, 0x6240,
    0x6600, 0xa6c1, 0xa781, 0x6740, 0xa501, 0x65c0, 0x6480, 0xa441,
    0x6c00, 0xacc1, 0xad81, 0x6d40, 0xaf01, 0x6fc0, 0x6e80, 0xae41,
    0xaa01, 0x6ac0, 0x6b80, 0xab41, 0x6900, 0xa9c1, 0xa881, 0x6840,
    0x7800, 0xb8c1, 0xb981, 0x7940, 0xbb01, 0x7bc0, 0x7a80, 0xba41,
    0xbe01, 0x7ec0, 0x7f80, 0xbf41, 0x7d00, 0xbdc1, 0xbc81, 0x7c40,
    0xb401, 0x74c0, 0x7580, 0xb541, 0x7700, 0xb7c1, 0xb681, 0x7640,
    0x7200, 0xb2c1, 0xb381, 0x7340, 0xb101, 0x71c0, 0x7080, 0xb041,
    0x5000, 0x90c1, 0x9181, 0x5140, 0x9301, 0x53c0, 0x5280, 0x9241,
    0x9601, 0x56c0, 0x5780, 0x9741, 0x5500, 0x95c1, 0x9481, 0x5440,
    0x9c01, 0x5cc0, 0x5d80, 0x9d41, 0x5f00, 0x9fc1, 0x9e81, 0x5e40,
    0x5a00, 0x9ac1, 0x9b81, 0x5b40, 0x9901, 0x59c0, 0x5880, 0x9841,
    0x8801, 0x48c0, 0x4980, 0x8941, 0x4b00, 0x8bc1, 0x8a81, 0x4a40,
    0x4e00, 0x8ec1, 0x8f81, 0x4f40, 0x8d01, 0x4dc0, 0x4c80, 0x8c41,
    0x4400, 0x84c1, 0x8581, 0x4540, 0x8701, 0x47c0, 0x4680, 0x8641,
    0x8201, 0x42c0, 0x4380, 0x8341, 0x4100, 0x81c1, 0x8081, 0x4040
};

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
void NARUUtility_ApplyWindow(const double *window, double *data, uint32_t num_samples)
{
    uint32_t smpl;

    NARU_ASSERT(window != NULL && data != NULL);

    for (smpl = 0; smpl < num_samples; smpl++) {
        data[smpl] *= window[smpl];
    }
}

/* サイン窓を作成 */
void NARUUtility_MakeSinWindow(double *window, uint32_t window_size)
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

/* CRC16(IBM)の計算 */
uint16_t NARUUtility_CalculateCRC16(const uint8_t *data, uint64_t data_size)
{
    uint16_t crc16;

    /* 引数チェック */
    NARU_ASSERT(data != NULL);

    /* 初期値 */
    crc16 = 0x0000;

    /* modulo2計算 */
    while (data_size--) {
        /* 補足）多項式は反転済みなので、この計算により入出力反転済みとできる */
        crc16 = (crc16 >> 8) ^ st_crc16_ibm_byte_table[(crc16 ^ (*data++)) & 0xFF];
    }

    return crc16;
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

/* プリエンファシス(double, in-place) */
void NARUUtility_PreEmphasisDouble(
        double *data, uint32_t num_samples, int32_t coef_shift)
{
    uint32_t smpl;
    double prev, tmp;
    double coef;

    NARU_ASSERT(data);
    NARU_ASSERT(num_samples > 0);
    NARU_ASSERT(coef_shift > 0);

    /* フィルタ係数の計算 */
    coef = (pow(2.0f, (double)coef_shift) - 1.0f) * pow(2.0f, (double)-coef_shift);

    /* フィルタ適用 */
    prev = 0.0f;
    for (smpl = 0; smpl < num_samples; smpl++) {
        tmp         = data[smpl];
        data[smpl] -= prev * coef;
        prev        = tmp;
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
uint32_t NARUUtility_GetDataBitWidth(const int32_t* data, uint32_t num_samples)
{
    uint32_t smpl, usbitwidth;
    int32_t maxpos, minneg;

    NARU_ASSERT(data != NULL);

    /* 最大正数と最小負数の計測 */
    maxpos =  0;
    minneg = -1;
    for (smpl = 0; smpl < num_samples; smpl++) {
        if (maxpos < data[smpl]) {
            maxpos = data[smpl];
        }
        if (minneg > data[smpl]) {
            minneg = data[smpl];
        }
    }

    /* 符号を除く部分のビット幅を計算 */
    /* 正数の場合は2の冪乗数の場合の対処が必要 */
    usbitwidth = NARUUTILITY_MAX(
            NARUUTILITY_LOG2CEIL((uint32_t)maxpos + 1), NARUUTILITY_LOG2CEIL((uint32_t)(-minneg)));

    /* 符号ビットを付加 */
    return usbitwidth + 1;
}

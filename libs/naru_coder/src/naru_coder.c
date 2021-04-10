#include "naru_coder.h"
#include "naru_utility.h"
#include "naru_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* 再帰的ライス符号の最大段数 */
#define NARUCODER_MAX_NUM_RECURSIVERICE_PARAMETER 8
/* 固定小数の小数部ビット数 */
#define NARUCODER_NUM_FRACTION_PART_BITS          8
/* 固定小数の0.5 */
#define NARUCODER_FIXED_FLOAT_0_5                (1UL << ((NARUCODER_NUM_FRACTION_PART_BITS) - 1))
/* 符号なし整数を固定小数に変換 */
#define NARUCODER_UINT32_TO_FIXED_FLOAT(u32)     ((u32) << (NARUCODER_NUM_FRACTION_PART_BITS))
/* 固定小数を符号なし整数に変換 */
#define NARUCODER_FIXED_FLOAT_TO_UINT32(fixed)   (uint32_t)(((fixed) + (NARUCODER_FIXED_FLOAT_0_5)) >> (NARUCODER_NUM_FRACTION_PART_BITS))
/* ゴロム符号パラメータ直接設定 */
#define NARUCODER_PARAMETER_SET(param_array, order, val)\
    do {\
        ((param_array)[(order)]) = NARUCODER_UINT32_TO_FIXED_FLOAT(val);\
    } while (0);
/* ゴロム符号パラメータ取得 : 1以上であることを担保 */
#define NARUCODER_PARAMETER_GET(param_array, order)\
    (NARUUTILITY_MAX(NARUCODER_FIXED_FLOAT_TO_UINT32((param_array)[(order)]), 1UL))
/* Rice符号のパラメータ更新式 */
/* 指数平滑平均により平均値を推定 */
#define NARURICE_PARAMETER_UPDATE(param_array, order, code)\
    do {\
        (param_array)[(order)] = (NARURecursiveRiceParameter)(119 * (param_array)[(order)] + 9 * NARUCODER_UINT32_TO_FIXED_FLOAT(code) + (1UL << 6)) >> 7;\
    } while (0);
/* Rice符号のパラメータ計算 2 ** ceil(log2(E(x)/2)) = E(x)/2の2の冪乗切り上げ */
#define NARURICE_CALCULATE_RICE_PARAMETER(param_array, order)\
    NARUUTILITY_ROUNDUP2POWERED(NARUUTILITY_MAX(NARUCODER_FIXED_FLOAT_TO_UINT32((param_array)[(order)] >> 1), 1UL))
#define NARURICE_CALCULATE_LOG2_RICE_PARAMETER(param_array, order)\
    NARUUTILITY_LOG2CEIL(NARUUTILITY_MAX(NARUCODER_FIXED_FLOAT_TO_UINT32((param_array)[(order)] >> 1), 1UL))

/* 再帰的ライス符号パラメータ型 */
typedef uint64_t NARURecursiveRiceParameter;

/* 符号化ハンドル */
struct NARUCoder {
    NARURecursiveRiceParameter** rice_parameter;
    NARURecursiveRiceParameter** init_rice_parameter;
    uint32_t max_num_channels;
    uint32_t max_num_parameters;
    uint8_t alloced_by_own;
    void *work;
};

/* ゴロム符号化の出力 */
static void NARUGolomb_PutCode(struct NARUBitStream *stream, uint32_t m, uint32_t val)
{
    uint32_t quot;
    uint32_t rest;
    uint32_t b, two_b;

    NARU_ASSERT(stream != NULL);
    NARU_ASSERT(m != 0);

    /* 商部分長と剰余部分の計算 */
    quot = val / m;
    rest = val % m;

    /* 前半部分の出力(unary符号) */
    while (quot > 0) {
        NARUBitWriter_PutBits(stream, 0, 1);
        quot--;
    }
    NARUBitWriter_PutBits(stream, 1, 1);

    /* 剰余部分の出力 */
    if (NARUUTILITY_IS_POWERED_OF_2(m)) {
        /* mが2の冪: ライス符号化 m == 1の時は剰余0だから何もしない */
        if (m > 1) {
            NARUBitWriter_PutBits(stream, rest, NARUUTILITY_LOG2CEIL(m));
        }
        return;
    }

    /* ゴロム符号化 */
    b = NARUUTILITY_LOG2CEIL(m);
    two_b = (uint32_t)(1UL << b);
    if (rest < (two_b - m)) {
        NARUBitWriter_PutBits(stream, rest, b - 1);
    } else {
        NARUBitWriter_PutBits(stream, rest + two_b - m, b);
    }
}

/* ゴロム符号の取得 */
static uint32_t NARUGolomb_GetCode(struct NARUBitStream *stream, uint32_t m)
{
    uint32_t quot, rest, b, two_b;

    NARU_ASSERT(stream != NULL);
    NARU_ASSERT(m != 0);

    /* 前半のunary符号部分を読み取り */
    NARUBitReader_GetZeroRunLength(stream, &quot);

    /* 剰余部の桁数 */
    b = NARUUTILITY_LOG2CEIL(m);

    /* 剰余部分の読み取り */
    if (NARUUTILITY_IS_POWERED_OF_2(m)) {
        /* mが2の冪: ライス符号化 */
        NARUBitReader_GetBits(stream, &rest, b);
        return (uint32_t)((quot << b) + rest);
    }

    /* ゴロム符号化 */
    two_b = (uint32_t)(1UL << b);
    NARUBitReader_GetBits(stream, &rest, b - 1);
    if (rest < (two_b - m)) {
        return (uint32_t)(quot * m + rest);
    } else {
        uint32_t buf;
        rest <<= 1;
        NARUBitReader_GetBits(stream, &buf, 1);
        rest += buf;
        return (uint32_t)(quot * m + rest - (two_b - m));
    }
}

/* ガンマ符号の出力 */
static void NARUGamma_PutCode(struct NARUBitStream *stream, uint32_t val)
{
    uint32_t ndigit;

    NARU_ASSERT(stream != NULL);

    if (val == 0) {
        /* 符号化対象が0ならば1を出力して終了 */
        NARUBitWriter_PutBits(stream, 1, 1);
        return;
    }

    /* 桁数を取得 */
    ndigit = NARUUTILITY_LOG2CEIL(val + 2);
    /* 桁数-1だけ0を続ける */
    NARUBitWriter_PutBits(stream, 0, ndigit - 1);
    /* 桁数を使用して符号語を2進数で出力 */
    NARUBitWriter_PutBits(stream, val + 1, ndigit);
}

/* ガンマ符号の取得 */
static uint32_t NARUGamma_GetCode(struct NARUBitStream *stream)
{
    uint32_t ndigit;
    uint32_t bitsbuf;

    NARU_ASSERT(stream != NULL);

    /* 桁数を取得 */
    /* 1が出現するまで桁数を増加 */
    NARUBitReader_GetZeroRunLength(stream, &ndigit);
    /* 最低でも1のため下駄を履かせる */
    ndigit++;

    /* 桁数が1のときは0 */
    if (ndigit == 1) {
        return 0;
    }

    /* 桁数から符号語を出力 */
    NARUBitReader_GetBits(stream, &bitsbuf, ndigit - 1);
    return (uint32_t)((1UL << (ndigit - 1)) + bitsbuf - 1);
}

/* 商部分（アルファ符号）を出力 */
static void NARURecursiveRice_PutQuotPart(
        struct NARUBitStream *stream, uint32_t quot)
{
    NARU_ASSERT(stream != NULL);

    if (quot == 0) {
        NARUBitWriter_PutBits(stream, 1, 1);
        return;
    }

    NARU_ASSERT(quot < 32);
    NARUBitWriter_PutBits(stream, 0, quot);
    NARUBitWriter_PutBits(stream, 1, 1);
}

/* 商部分（アルファ符号）を取得 */
static uint32_t NARURecursiveRice_GetQuotPart(struct NARUBitStream *stream)
{
    uint32_t quot;

    NARU_ASSERT(stream != NULL);

    NARUBitReader_GetZeroRunLength(stream, &quot);

    return quot;
}

/* 剰余部分を出力 kは剰余部の桁数（logを取ったライス符号） */
static void NARURecursiveRice_PutRestPart(
        struct NARUBitStream *stream, uint32_t val, uint32_t k)
{
    NARU_ASSERT(stream != NULL);

    /* k == 0の時はスキップ（剰余は0で確定だから） */
    if (k > 0) {
        NARUBitWriter_PutBits(stream, val, k);
    }
}

/* 剰余部分を取得 kは剰余部の桁数（logを取ったライス符号） */
static uint32_t NARURecursiveRice_GetRestPart(struct NARUBitStream *stream, uint32_t k)
{
    uint32_t rest;

    NARU_ASSERT(stream != NULL);

    /* ライス符号の剰余部分取得 */
    NARUBitReader_GetBits(stream, &rest, k);

    return rest;
}

/* 再帰的ライス符号の出力 */
static void NARURecursiveRice_PutCode(
        struct NARUBitStream *stream, NARURecursiveRiceParameter* rice_parameters, uint32_t num_params, uint32_t val)
{
    uint32_t i, reduced_val, k, param;

    NARU_ASSERT(stream != NULL);
    NARU_ASSERT(rice_parameters != NULL);
    NARU_ASSERT(num_params != 0);
    NARU_ASSERT(NARUCODER_PARAMETER_GET(rice_parameters, 0) != 0);

    reduced_val = val;
    for (i = 0; i < (num_params - 1); i++) {
        k = NARURICE_CALCULATE_LOG2_RICE_PARAMETER(rice_parameters, i);
        param = (1U << k);
        /* 現在のパラメータ値よりも小さければ、符号化を行う */
        if (reduced_val < param) {
            /* 商部分としてはパラメータ段数 */
            NARURecursiveRice_PutQuotPart(stream, i);
            /* 剰余部分 */
            NARURecursiveRice_PutRestPart(stream, reduced_val, k);
            /* パラメータ更新 */
            NARURICE_PARAMETER_UPDATE(rice_parameters, i, reduced_val);
            /* これで終わり */
            return;
        }
        /* パラメータ更新 */
        NARURICE_PARAMETER_UPDATE(rice_parameters, i, reduced_val);
        /* 現在のパラメータ値で減じる */
        reduced_val -= param;
    }

    /* 末尾のパラメータに達した */
    NARU_ASSERT(i == (num_params - 1));
    {
        uint32_t quot;
        k = NARURICE_CALCULATE_LOG2_RICE_PARAMETER(rice_parameters, i);
        quot = i + (reduced_val >> k);
        /* 商が大きい場合はガンマ符号を使用する */
        if (quot < NARUCODER_QUOTPART_THRESHOULD) {
            NARURecursiveRice_PutQuotPart(stream, quot);
        } else {
            NARURecursiveRice_PutQuotPart(stream, NARUCODER_QUOTPART_THRESHOULD);
            NARUGamma_PutCode(stream, quot - NARUCODER_QUOTPART_THRESHOULD);
        }
        NARURecursiveRice_PutRestPart(stream, reduced_val, k);
        /* パラメータ更新 */
        NARURICE_PARAMETER_UPDATE(rice_parameters, i, reduced_val);
    }

}

/* 再帰的ライス符号の取得 */
static uint32_t NARURecursiveRice_GetCode(
        struct NARUBitStream *stream, NARURecursiveRiceParameter* rice_parameters, uint32_t num_params)
{
    uint32_t i, quot, val;
    uint32_t k, param, tmp_val;
    uint32_t param_cache[NARUCODER_MAX_NUM_RECURSIVERICE_PARAMETER];

    NARU_ASSERT(stream != NULL);
    NARU_ASSERT(rice_parameters != NULL);
    NARU_ASSERT(num_params != 0);
    NARU_ASSERT(NARUCODER_PARAMETER_GET(rice_parameters, 0) != 0);
    NARU_ASSERT(num_params <= NARUCODER_MAX_NUM_RECURSIVERICE_PARAMETER);

    /* 商部分を取得 */
    quot = NARURecursiveRice_GetQuotPart(stream);

    /* 得られたパラメータ段数までのパラメータを加算 */
    val = 0;
    for (i = 0; (i < quot) && (i < (num_params - 1)); i++) {
        param = NARURICE_CALCULATE_RICE_PARAMETER(rice_parameters, i);
        val += param;
        param_cache[i] = param;
    }

    /* ライス符号の剰余部を使うためlog2でパラメータ取得 */
    k = NARURICE_CALCULATE_LOG2_RICE_PARAMETER(rice_parameters, i);
    param_cache[i] = (1U << k);

    /* 剰余部を取得 */
    if (quot >= (num_params - 1)) {
        /* 末尾のパラメータに達している */
        if (quot == NARUCODER_QUOTPART_THRESHOULD) {
            quot += NARUGamma_GetCode(stream);
        }
        val += ((quot - (num_params - 1)) << k);
    }
    val += NARURecursiveRice_GetRestPart(stream, k);

    /* パラメータ更新 */
    /* 補足）符号語が分かってからでないと更新できない */
    tmp_val = val;
    for (i = 0; (i <= quot) && (i < num_params); i++) {
        NARURICE_PARAMETER_UPDATE(rice_parameters, i, tmp_val);
        tmp_val -= param_cache[i];
    }

    return val;
}

/* 符号化ハンドルの作成に必要なワークサイズの計算 */
int32_t NARUCoder_CalculateWorkSize(uint32_t max_num_channels, uint32_t max_num_parameters)
{
    int32_t work_size;

    /* 引数チェック */
    if ((max_num_channels == 0) || (max_num_parameters == 0)) {
        return -1;
    }

    /* ハンドル分のサイズ */
    work_size = sizeof(struct NARUCoder) + NARU_MEMORY_ALIGNMENT;

    /* パラメータ領域分のサイズ（初期パラメータを含むので2倍） */
    work_size += 2 * sizeof(NARURecursiveRiceParameter *) * max_num_channels;
    work_size += 2 * sizeof(NARURecursiveRiceParameter) * max_num_channels * max_num_parameters;

    return work_size;
}

/* 符号化ハンドルの作成 */
struct NARUCoder* NARUCoder_Create(
        uint32_t max_num_channels, uint32_t max_num_parameters, void *work, int32_t work_size)
{
    uint32_t ch;
    struct NARUCoder *coder;
    uint8_t tmp_alloc_by_own = 0;
    uint8_t *work_ptr;

    /* ワーク領域時前確保の場合 */
    if ((work == NULL) && (work_size == 0)) {
        /* 引数を自前の計算値に差し替える */
        if ((work_size = NARUCoder_CalculateWorkSize(max_num_channels, max_num_parameters)) < 0) {
            return NULL;
        }
        work = malloc((uint32_t)work_size);
        tmp_alloc_by_own = 1;
    }

    /* 引数チェック */
    if ((work == NULL) || (max_num_channels == 0) || (max_num_parameters == 0)
            || (work_size < NARUCoder_CalculateWorkSize(max_num_channels, max_num_parameters))) {
        return NULL;
    }

    /* ワーク領域先頭取得 */
    work_ptr = (uint8_t *)work;

    /* ハンドル領域確保 */
    work_ptr = (uint8_t *)NARUUTILITY_ROUNDUP((uintptr_t)work_ptr, NARU_MEMORY_ALIGNMENT);
    coder = (struct NARUCoder *)work_ptr;
    work_ptr += sizeof(struct NARUCoder);

    /* ハンドルメンバ設定 */
    coder->max_num_channels = max_num_channels;
    coder->max_num_parameters = max_num_parameters;
    coder->alloced_by_own = tmp_alloc_by_own;
    coder->work = work;

    /* パラメータ領域確保 */
    coder->rice_parameter = (NARURecursiveRiceParameter **)work_ptr;
    work_ptr += sizeof(NARURecursiveRiceParameter *) * max_num_channels;
    coder->init_rice_parameter = (NARURecursiveRiceParameter **)work_ptr;
    work_ptr += sizeof(NARURecursiveRiceParameter *) * max_num_channels;

    for (ch = 0; ch < max_num_channels; ch++) {
        coder->rice_parameter[ch] = (NARURecursiveRiceParameter *)work_ptr;
        work_ptr += sizeof(NARURecursiveRiceParameter) * max_num_parameters;
    }
    for (ch = 0; ch < max_num_channels; ch++) {
        coder->init_rice_parameter[ch] = (NARURecursiveRiceParameter *)work_ptr;
        work_ptr += sizeof(NARURecursiveRiceParameter) * max_num_parameters;
    }

    return coder;
}

/* 符号化ハンドルの破棄 */
void NARUCoder_Destroy(struct NARUCoder *coder)
{
    if (coder != NULL) {
        /* 自前確保していたら領域開放 */
        if (coder->alloced_by_own == 1) {
            free(coder->work);
        }
    }
}

/* 初期パラメータの計算 */
void NARUCoder_CalculateInitialRecursiveRiceParameter(
        struct NARUCoder *coder, uint32_t num_parameters,
        const int32_t **data, uint32_t num_channels, uint32_t num_samples)
{
    uint32_t ch, smpl, i, init_param;
    uint64_t sum;

    NARU_ASSERT((coder != NULL) && (data != NULL));
    NARU_ASSERT(num_parameters <= coder->max_num_parameters);

    for (ch = 0; ch < num_channels; ch++) {
        /* パラメータ初期値（符号平均値）の計算 */
        sum = 0;
        for (smpl = 0; smpl < num_samples; smpl++) {
            sum += NARUUTILITY_SINT32_TO_UINT32(data[ch][smpl]);
        }

        init_param = (uint32_t)NARUUTILITY_MAX(sum / num_samples, 1);

        /* 初期パラメータのセット */
        for (i = 0; i < num_parameters; i++) {
            NARUCODER_PARAMETER_SET(coder->init_rice_parameter[ch], i, init_param);
            NARUCODER_PARAMETER_SET(coder->rice_parameter[ch], i, init_param);
        }
    }
}

/* 再帰的ライス符号のパラメータを符号化（量子化による副作用あり） */
void NARUCoder_PutInitialRecursiveRiceParameter(
        struct NARUCoder *coder, struct NARUBitStream *stream, uint32_t num_parameters, uint32_t channel_index)
{
    uint32_t i, first_order_param, log2_param;

    NARUUTILITY_UNUSED_ARGUMENT(num_parameters);
    NARU_ASSERT((stream != NULL) && (coder != NULL));
    NARU_ASSERT(num_parameters <= coder->max_num_parameters);
    NARU_ASSERT(channel_index < coder->max_num_channels);

    /* 1次パラメータを取得 */
    first_order_param = NARUCODER_PARAMETER_GET(coder->init_rice_parameter[channel_index], 0);

    /* 記録のためlog2をとる */
    log2_param = NARUUTILITY_LOG2CEIL(first_order_param);

    /* 2の冪乗を取って量子化 */
    first_order_param = 1U << log2_param;

    /* 書き出し */
    NARU_ASSERT(log2_param < 32);
    NARUBitWriter_PutBits(stream, log2_param, 5);

    /* パラメータ反映 */
    for (i = 0; i < num_parameters; i++) {
        NARUCODER_PARAMETER_SET(coder->init_rice_parameter[channel_index], i, first_order_param);
        NARUCODER_PARAMETER_SET(coder->rice_parameter[channel_index], i, first_order_param);
    }
}

/* 再帰的ライス符号のパラメータを取得 */
void NARUCoder_GetInitialRecursiveRiceParameter(
        struct NARUCoder *coder, struct NARUBitStream *stream, uint32_t num_parameters, uint32_t channel_index)
{
    uint32_t i, first_order_param, log2_param;

    NARU_ASSERT((stream != NULL) && (coder != NULL));
    NARU_ASSERT(num_parameters <= coder->max_num_parameters);
    NARU_ASSERT(channel_index < coder->max_num_channels);

    /* 初期パラメータの取得 */
    NARUBitReader_GetBits(stream, &log2_param, 5);
    NARU_ASSERT(log2_param < 32);
    first_order_param = 1U << log2_param;

    /* 初期パラメータの設定 */
    for (i = 0; i < num_parameters; i++) {
        NARUCODER_PARAMETER_SET(coder->init_rice_parameter[channel_index], i, (uint32_t)first_order_param);
        NARUCODER_PARAMETER_SET(coder->rice_parameter[channel_index], i, (uint32_t)first_order_param);
    }
}

/* 符号付き整数配列の符号化 */
void NARUCoder_PutDataArray(
        struct NARUCoder *coder, struct NARUBitStream *stream,
        uint32_t num_parameters, const int32_t **data, uint32_t num_channels, uint32_t num_samples)
{
    uint32_t smpl, ch;
    uint32_t param_ch_avg;

    NARU_ASSERT((stream != NULL) && (data != NULL) && (coder != NULL));
    NARU_ASSERT((num_parameters != 0) && (num_parameters <= coder->max_num_parameters));
    NARU_ASSERT(num_samples != 0);
    NARU_ASSERT(num_channels != 0);

    /* 全チャンネルでのパラメータ平均を算出 */
    param_ch_avg = 0;
    for (ch = 0; ch < num_channels; ch++) {
        param_ch_avg += NARUCODER_PARAMETER_GET(coder->init_rice_parameter[ch], 0);
    }
    param_ch_avg /= num_channels;

    /* チャンネルインターリーブしつつ符号化 */
    if (param_ch_avg > NARUCODER_LOW_THRESHOULD_PARAMETER) {
        for (smpl = 0; smpl < num_samples; smpl++) {
            for (ch = 0; ch < num_channels; ch++) {
                NARURecursiveRice_PutCode(stream,
                        coder->rice_parameter[ch], num_parameters, NARUUTILITY_SINT32_TO_UINT32(data[ch][smpl]));
            }
        }
    } else {
        /* パラメータが小さい場合はパラメータ固定で符号 */
        for (smpl = 0; smpl < num_samples; smpl++) {
            for (ch = 0; ch < num_channels; ch++) {
                NARUGolomb_PutCode(stream, NARUCODER_PARAMETER_GET(coder->init_rice_parameter[ch], 0), NARUUTILITY_SINT32_TO_UINT32(data[ch][smpl]));
            }
        }
    }

}

/* 符号付き整数配列の復号 */
void NARUCoder_GetDataArray(
        struct NARUCoder *coder, struct NARUBitStream *stream,
        uint32_t num_parameters, int32_t **data, uint32_t num_channels, uint32_t num_samples)
{
    uint32_t ch, smpl, abs;
    uint32_t param_ch_avg;

    NARU_ASSERT((stream != NULL) && (data != NULL) && (coder != NULL));
    NARU_ASSERT((num_parameters != 0) && (num_samples != 0));

    /* 全チャンネルでのパラメータ平均を算出 */
    param_ch_avg = 0;
    for (ch = 0; ch < num_channels; ch++) {
        param_ch_avg += NARUCODER_PARAMETER_GET(coder->init_rice_parameter[ch], 0);
    }
    param_ch_avg /= num_channels;

    /* パラメータを適応的に変更しつつ符号化 */

    if (param_ch_avg > NARUCODER_LOW_THRESHOULD_PARAMETER) {
        /* パラメータが小さい場合はパラメータ固定でゴロム符号化 */
        for (smpl = 0; smpl < num_samples; smpl++) {
            for (ch = 0; ch < num_channels; ch++) {
                abs = NARURecursiveRice_GetCode(stream, coder->rice_parameter[ch], num_parameters);
                data[ch][smpl] = NARUUTILITY_UINT32_TO_SINT32(abs);
            }
        }
    } else {
        for (smpl = 0; smpl < num_samples; smpl++) {
            for (ch = 0; ch < num_channels; ch++) {
                abs = NARUGolomb_GetCode(stream, NARUCODER_PARAMETER_GET(coder->init_rice_parameter[ch], 0));
                data[ch][smpl] = NARUUTILITY_UINT32_TO_SINT32(abs);
            }
        }
    }
}

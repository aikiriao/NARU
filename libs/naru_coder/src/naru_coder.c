#include "naru_coder.h"
#include "naru_utility.h"
#include "naru_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* 固定小数の小数部ビット数 */
#define NARUCODER_NUM_FRACTION_PART_BITS         8
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

/* 再帰的ライス符号パラメータ型 */
typedef uint64_t NARURecursiveRiceParameter;

/* 符号化ハンドル */
struct NARUCoder {
  NARURecursiveRiceParameter** rice_parameter;
  NARURecursiveRiceParameter** init_rice_parameter;
  uint32_t                    max_num_channels;
  uint32_t                    max_num_parameters;
};

#if 0
/* ゴロム符号化の出力 */
static void NARUGolomb_PutCode(struct NARUBitStream* strm, uint32_t m, uint32_t val)
{
  uint32_t quot;
  uint32_t rest;
  uint32_t b, two_b;

  NARU_ASSERT(strm != NULL);
  NARU_ASSERT(m != 0);

  /* 商部分長と剰余部分の計算 */
  quot = val / m;
  rest = val % m;

  /* 前半部分の出力(unary符号) */
  while (quot > 0) {
    NARUBitWriter_PutBits(strm, 0, 1);
    quot--;
  }
  NARUBitWriter_PutBits(strm, 1, 1);

  /* 剰余部分の出力 */
  if (NARUUTILITY_IS_POWERED_OF_2(m)) {
    /* mが2の冪: ライス符号化 m == 1の時は剰余0だから何もしない */
    if (m > 1) {
      NARUBitWriter_PutBits(strm, rest, NARUUTILITY_LOG2CEIL(m));
    }
    return;
  }

  /* ゴロム符号化 */
  b = NARUUTILITY_LOG2CEIL(m);
  two_b = (uint32_t)(1UL << b);
  if (rest < (two_b - m)) {
    NARUBitWriter_PutBits(strm, rest, b - 1);
  } else {
    NARUBitWriter_PutBits(strm, rest + two_b - m, b);
  }
}

/* ゴロム符号化の取得 */
static uint32_t NARUGolomb_GetCode(struct NARUBitStream* strm, uint32_t m) 
{
  uint32_t quot;
  uint64_t rest;
  uint32_t b, two_b;

  NARU_ASSERT(strm != NULL);
  NARU_ASSERT(m != 0);

  /* 前半のunary符号部分を読み取り */
  NARUBitReader_GetZeroRunLength(strm, &quot);

  /* 剰余部分の読み取り */
  if (NARUUTILITY_IS_POWERED_OF_2(m)) {
    /* mが2の冪: ライス符号化 */
    NARUBitReader_GetBits(strm, &rest, NARUUTILITY_LOG2CEIL(m));
    return (uint32_t)(quot * m + rest);
  }

  /* ゴロム符号化 */
  b = NARUUTILITY_LOG2CEIL(m);
  two_b = (uint32_t)(1UL << b);
  NARUBitReader_GetBits(strm, &rest, b - 1);
  if (rest < (two_b - m)) {
    return (uint32_t)(quot * m + rest);
  } else {
    uint64_t buf;
    rest <<= 1;
    NARUBitReader_GetBits(strm, &buf, 1);
    rest += buf;
    return (uint32_t)(quot * m + rest - (two_b - m));
  }
}
#endif

/* ガンマ符号の出力 */
static void NARUGamma_PutCode(struct NARUBitStream* strm, uint32_t val)
{
  uint32_t ndigit;

  NARU_ASSERT(strm != NULL);

  if (val == 0) {
    /* 符号化対象が0ならば1を出力して終了 */
    NARUBitWriter_PutBits(strm, 1, 1);
    return;
  } 

  /* 桁数を取得 */
  ndigit = NARUUTILITY_LOG2CEIL(val + 2);
  /* 桁数-1だけ0を続ける */
  NARUBitWriter_PutBits(strm, 0, ndigit - 1);
  /* 桁数を使用して符号語を2進数で出力 */
  NARUBitWriter_PutBits(strm, val + 1, ndigit);
}

/* ガンマ符号の取得 */
static uint32_t NARUGamma_GetCode(struct NARUBitStream* strm)
{
  uint32_t ndigit;
  uint64_t bitsbuf;

  NARU_ASSERT(strm != NULL);

  /* 桁数を取得 */
  /* 1が出現するまで桁数を増加 */
  NARUBitReader_GetZeroRunLength(strm, &ndigit);
  /* 最低でも1のため下駄を履かせる */
  ndigit++;

  /* 桁数が1のときは0 */
  if (ndigit == 1) {
    return 0;
  }

  /* 桁数から符号語を出力 */
  NARUBitReader_GetBits(strm, &bitsbuf, ndigit - 1);
  return (uint32_t)((1UL << (ndigit - 1)) + bitsbuf - 1);
}

/* 商部分（アルファ符号）を出力 */
static void NARURecursiveRice_PutQuotPart(
    struct NARUBitStream* strm, uint32_t quot)
{
  NARU_ASSERT(strm != NULL);

  while (quot > 0) {
    NARUBitWriter_PutBits(strm, 0, 1);
    quot--;
  }
  NARUBitWriter_PutBits(strm, 1, 1);
}

/* 商部分（アルファ符号）を取得 */
static uint32_t NARURecursiveRice_GetQuotPart(struct NARUBitStream* strm)
{
  uint32_t quot;
  
  NARU_ASSERT(strm != NULL);

  NARUBitReader_GetZeroRunLength(strm, &quot);

  return quot;
}

/* 剰余部分を出力 TODO:mはシフトパラメータでもいいはず */
static void NARURecursiveRice_PutRestPart(
    struct NARUBitStream* strm, uint32_t val, uint32_t m)
{
  NARU_ASSERT(strm != NULL);
  NARU_ASSERT(m != 0);
  NARU_ASSERT(NARUUTILITY_IS_POWERED_OF_2(m));

  /* m == 1の時はスキップ（剰余は0で確定だから） */
  if (m != 1) {
    NARUBitWriter_PutBits(strm, val & (m - 1), NARUUTILITY_LOG2CEIL(m));
  }
}

/* 剰余部分を取得 TODO:mはシフトパラメータでもいいはず */
static uint32_t NARURecursiveRice_GetRestPart(struct NARUBitStream* strm, uint32_t m)
{
  uint64_t rest;

  NARU_ASSERT(strm != NULL);
  NARU_ASSERT(m != 0);
  NARU_ASSERT(NARUUTILITY_IS_POWERED_OF_2(m));

  /* 1の剰余は0 */
  if (m == 1) {
    return 0;
  }

  /* ライス符号の剰余部分取得 */
  NARUBitReader_GetBits(strm, &rest, NARUUTILITY_LOG2CEIL(m));
  
  return (uint32_t)rest;
}

/* 再帰的ライス符号の出力 */
static void NARURecursiveRice_PutCode(
    struct NARUBitStream* strm, NARURecursiveRiceParameter* rice_parameters, uint32_t num_params, uint32_t val)
{
  uint32_t i, reduced_val, param;

  NARU_ASSERT(strm != NULL);
  NARU_ASSERT(rice_parameters != NULL);
  NARU_ASSERT(num_params != 0);
  NARU_ASSERT(NARUCODER_PARAMETER_GET(rice_parameters, 0) != 0);

  reduced_val = val;
  for (i = 0; i < (num_params - 1); i++) {
    param = NARURICE_CALCULATE_RICE_PARAMETER(rice_parameters, i);
    /* 現在のパラメータ値よりも小さければ、符号化を行う */
    if (reduced_val < param) {
        /* 商部分としてはパラメータ段数 */
        NARURecursiveRice_PutQuotPart(strm, i);
        /* 剰余部分 */
        NARURecursiveRice_PutRestPart(strm, reduced_val, param);
        /* パラメータ更新 */
        NARURICE_PARAMETER_UPDATE(rice_parameters, i, reduced_val);
        break;
    }
    /* パラメータ更新 */
    NARURICE_PARAMETER_UPDATE(rice_parameters, i, reduced_val);
    /* 現在のパラメータ値で減じる */
    reduced_val -= param;
  }

  /* 末尾のパラメータに達した */
  if (i == (num_params - 1)) {
    uint32_t tail_param = NARURICE_CALCULATE_RICE_PARAMETER(rice_parameters, i);
    uint32_t tail_quot  = i + reduced_val / tail_param; /* TODO: tail_paramは2の冪数だから、シフトに置き換えられる */ 
    NARU_ASSERT(NARUUTILITY_IS_POWERED_OF_2(tail_param));
    /* 商が大きい場合はガンマ符号を使用する */
    if (tail_quot < NARUCODER_QUOTPART_THRESHOULD) {
      NARURecursiveRice_PutQuotPart(strm, tail_quot);
    } else {
      NARURecursiveRice_PutQuotPart(strm, NARUCODER_QUOTPART_THRESHOULD);
      NARUGamma_PutCode(strm, tail_quot - NARUCODER_QUOTPART_THRESHOULD);
    }
    NARURecursiveRice_PutRestPart(strm, reduced_val, tail_param);
    /* パラメータ更新 */
    NARURICE_PARAMETER_UPDATE(rice_parameters, i, reduced_val);
  }

}

/* 再帰的ライス符号の取得 */
static uint32_t NARURecursiveRice_GetCode(
    struct NARUBitStream* strm, NARURecursiveRiceParameter* rice_parameters, uint32_t num_params)
{
  uint32_t  i, quot, val;
  uint32_t  param, tmp_val;

  NARU_ASSERT(strm != NULL);
  NARU_ASSERT(rice_parameters != NULL);
  NARU_ASSERT(num_params != 0);
  NARU_ASSERT(NARUCODER_PARAMETER_GET(rice_parameters, 0) != 0);

  /* 商部分を取得 */
  quot = NARURecursiveRice_GetQuotPart(strm);

  /* 得られたパラメータ段数までのパラメータを加算 */
  val = 0;
  for (i = 0; (i < quot) && (i < (num_params - 1)); i++) {
    val += NARURICE_CALCULATE_RICE_PARAMETER(rice_parameters, i);
  }

  if (quot < (num_params - 1)) {
    /* 指定したパラメータ段数で剰余部を取得 */
    val += NARURecursiveRice_GetRestPart(strm, 
        NARURICE_CALCULATE_RICE_PARAMETER(rice_parameters, i));
  } else {
    /* 末尾のパラメータで取得 */
    uint32_t tail_param = NARURICE_CALCULATE_RICE_PARAMETER(rice_parameters, i);
    uint32_t tail_rest;
    if (quot == NARUCODER_QUOTPART_THRESHOULD) {
      quot += NARUGamma_GetCode(strm);
    }
    tail_rest  = tail_param * (quot - (num_params - 1)) + NARURecursiveRice_GetRestPart(strm, tail_param);
    val       += tail_rest;
  }

  /* パラメータ更新 */
  /* 補足）符号語が分かってからでないと更新できない */
  tmp_val = val;
  for (i = 0; (i <= quot) && (i < num_params); i++) {
    param = NARURICE_CALCULATE_RICE_PARAMETER(rice_parameters, i);
    NARURICE_PARAMETER_UPDATE(rice_parameters, i, tmp_val);
    tmp_val -= param;
  }

  return val;
}

/* 符号化ハンドルの作成 */
struct NARUCoder* NARUCoder_Create(uint32_t max_num_channels, uint32_t max_num_parameters)
{
  uint32_t ch;
  struct NARUCoder* coder;
  
  coder = (struct NARUCoder *)malloc(sizeof(struct NARUCoder));
  coder->max_num_channels   = max_num_channels;
  coder->max_num_parameters = max_num_parameters;

  coder->rice_parameter       = (NARURecursiveRiceParameter **)malloc(sizeof(NARURecursiveRiceParameter *) * max_num_channels);
  coder->init_rice_parameter  = (NARURecursiveRiceParameter **)malloc(sizeof(NARURecursiveRiceParameter *) * max_num_channels);

  for (ch = 0; ch < max_num_channels; ch++) {
    coder->rice_parameter[ch] 
      = (NARURecursiveRiceParameter *)malloc(sizeof(NARURecursiveRiceParameter) * max_num_parameters);
    coder->init_rice_parameter[ch] 
      = (NARURecursiveRiceParameter *)malloc(sizeof(NARURecursiveRiceParameter) * max_num_parameters);
  }

  return coder;
}

/* 符号化ハンドルの破棄 */
void NARUCoder_Destroy(struct NARUCoder* coder)
{
  uint32_t ch;

  if (coder != NULL) {
    for (ch = 0; ch < coder->max_num_channels; ch++) {
      NARU_NULLCHECK_AND_FREE(coder->rice_parameter[ch]);
      NARU_NULLCHECK_AND_FREE(coder->init_rice_parameter[ch]);
    }
    NARU_NULLCHECK_AND_FREE(coder->rice_parameter);
    NARU_NULLCHECK_AND_FREE(coder->init_rice_parameter);
    free(coder);
  }

}

/* 初期パラメータの計算 */
void NARUCoder_CalculateInitialRecursiveRiceParameter(
    struct NARUCoder* coder, uint32_t num_parameters,
    const int32_t** data, uint32_t num_channels, uint32_t num_samples)
{
  uint32_t ch, smpl, i, init_param;
  uint64_t sum;

  NARU_ASSERT((coder != NULL) && (data != NULL));
  NARU_ASSERT(num_parameters <= coder->max_num_parameters);

  for (ch = 0; ch < num_channels; ch++) {
    /* パラメータ初期値（平均値）の計算 */
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

/* 再帰的ライス符号のパラメータを符号化 */
void NARUCoder_PutInitialRecursiveRiceParameter(
    struct NARUCoder* coder, struct NARUBitStream* strm,
    uint32_t num_parameters, uint32_t bitwidth, uint32_t channel_index)
{
  uint64_t first_order_param;

  NARUUTILITY_UNUSED_ARGUMENT(num_parameters);
  NARU_ASSERT((strm != NULL) && (coder != NULL));
  NARU_ASSERT(num_parameters <= coder->max_num_parameters);
  NARU_ASSERT(channel_index < coder->max_num_channels);

  /* 1次パラメータを取得 */
  first_order_param = NARUCODER_PARAMETER_GET(coder->init_rice_parameter[channel_index], 0);
  /* 書き出し */
  NARU_ASSERT(first_order_param < (1UL << bitwidth));
  NARUBitWriter_PutBits(strm, first_order_param, bitwidth);
}

/* 再帰的ライス符号のパラメータを取得 */
void NARUCoder_GetInitialRecursiveRiceParameter(
    struct NARUCoder* coder, struct NARUBitStream* strm,
    uint32_t num_parameters, uint32_t bitwidth, uint32_t channel_index)
{
  uint32_t i;
  uint64_t first_order_param;

  NARU_ASSERT((strm != NULL) && (coder != NULL));
  NARU_ASSERT(num_parameters <= coder->max_num_parameters);
  NARU_ASSERT(channel_index < coder->max_num_channels);

  /* 初期パラメータの取得 */
  NARUBitReader_GetBits(strm, &first_order_param, bitwidth);
  NARU_ASSERT(first_order_param < (1UL << bitwidth));
  /* 初期パラメータの取得 */
  for (i = 0; i < num_parameters; i++) {
    NARUCODER_PARAMETER_SET(coder->init_rice_parameter[channel_index], i, (uint32_t)first_order_param);
    NARUCODER_PARAMETER_SET(coder->rice_parameter[channel_index], i, (uint32_t)first_order_param);
  }
}

/* 符号付き整数配列の符号化 */
void NARUCoder_PutDataArray(
    struct NARUCoder* coder, struct NARUBitStream* strm,
    uint32_t num_parameters,
    const int32_t** data, uint32_t num_channels, uint32_t num_samples)
{
  uint32_t smpl, ch;

  NARU_ASSERT((strm != NULL) && (data != NULL) && (coder != NULL));
  NARU_ASSERT((num_parameters != 0) && (num_parameters <= coder->max_num_parameters));
  NARU_ASSERT(num_samples != 0);
  NARU_ASSERT(num_channels != 0);

  /* チャンネルインターリーブしつつ符号化 */
  for (smpl = 0; smpl < num_samples; smpl++) {
    for (ch = 0; ch < num_channels; ch++) {
      NARURecursiveRice_PutCode(strm,
          coder->rice_parameter[ch], num_parameters, NARUUTILITY_SINT32_TO_UINT32(data[ch][smpl]));
    }
  }
}

/* 符号付き整数配列の復号 */
void NARUCoder_GetDataArray(
    struct NARUCoder* coder, struct NARUBitStream* strm,
    uint32_t num_parameters,
    int32_t** data, uint32_t num_channels, uint32_t num_samples)
{
  uint32_t ch, smpl, abs;

  NARU_ASSERT((strm != NULL) && (data != NULL) && (coder != NULL));
  NARU_ASSERT((num_parameters != 0) && (num_samples != 0));

  /* パラメータを適応的に変更しつつ符号化 */
  for (smpl = 0; smpl < num_samples; smpl++) {
    for (ch = 0; ch < num_channels; ch++) {
      abs = NARURecursiveRice_GetCode(strm, coder->rice_parameter[ch], num_parameters);
      data[ch][smpl] = NARUUTILITY_UINT32_TO_SINT32(abs);
    }
  }
}

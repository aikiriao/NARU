#ifndef LPC_CALCULATOR_H_INCLUDED
#define LPC_CALCULATOR_H_INCLUDED

#include <stdint.h>

/* API結果型 */
typedef enum LPCCalculatorApiResultTag {
  LPCCALCULATOR_APIRESULT_OK = 0,                 /* OK */
  LPCCALCULATOR_APIRESULT_NG,                     /* 分類不能なエラー */
  LPCCALCULATOR_APIRESULT_INVALID_ARGUMENT,       /* 不正な引数 */
  LPCCALCULATOR_APIRESULT_EXCEED_MAX_ORDER,       /* 最大次数を超えた */
  LPCCALCULATOR_APIRESULT_FAILED_TO_CALCULATION   /* 計算に失敗 */
} LPCCalculatorApiResult;

/* LPC計算ハンドル */
struct LPCCalculator;

#ifdef __cplusplus
extern "C" {
#endif

/* LPC係数計算ハンドルの作成 */
struct LPCCalculator* LPCCalculator_Create(uint32_t max_order);

/* LPC係数計算ハンドルの破棄 */
void LPCCalculator_Destroy(struct LPCCalculator* lpcc);

/* Levinson-Durbin再帰計算によりLPC係数を求める */
/* 係数parcor_coefはorder+1個の配列 */
LPCCalculatorApiResult LPCCalculator_CalculateLPCCoef(
    struct LPCCalculator* lpcc,
    const double* data, uint32_t num_samples,
    double* lpc_coef, uint32_t order);

/* Levinson-Durbin再帰計算によりPARCOR係数を求める（倍精度） */
/* 係数parcor_coefはorder+1個の配列 */
LPCCalculatorApiResult LPCCalculator_CalculatePARCORCoef(
    struct LPCCalculator* lpcc,
    const double* data, uint32_t num_samples,
    double* parcor_coef, uint32_t order);

/* 入力データとPARCOR係数からサンプルあたりの推定符号長を求める */
LPCCalculatorApiResult LPCCalculator_EstimateCodeLength(
    const double* data, uint32_t num_samples, uint32_t bits_per_sample,
    const double* parcor_coef, uint32_t order, 
    double* length_per_sample_bits);

#ifdef __cplusplus
}
#endif

#endif /* LPC_CALCULATOR_H_INCLUDED */

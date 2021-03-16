#include "lpc_calculator.h"

#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <assert.h>

/* メモリアラインメント */
#define LPCCALCULATOR_ALIGNMENT 16

/* nの倍数切り上げ */
#define LPCCALCULATOR_ROUNDUP(val, n) ((((val) + ((n) - 1)) / (n)) * (n))

/* 内部エラー型 */
typedef enum LPCCalculatorErrorTag {
  LPCCALCULATOR_ERROR_OK = 0,
  LPCCALCULATOR_ERROR_NG,
  LPCCALCULATOR_ERROR_INVALID_ARGUMENT
} LPCCalculatorError;

/* LPC計算ハンドル */
struct LPCCalculator {
  uint32_t max_order;     /* 最大次数           */
  /* 内部的な計算結果は精度を担保するため全てdoubleで持つ */
  /* floatだとサンプル数を増やすと標本自己相関値の誤差に起因して出力の計算結果がnanになる */
  double *a_vec;         /* 計算用ベクトル1    */
  double *e_vec;         /* 計算用ベクトル2    */
  double *u_vec;         /* 計算用ベクトル3    */
  double *v_vec;         /* 計算用ベクトル4    */
  double *auto_corr;     /* 標本自己相関       */
  double *lpc_coef;      /* LPC係数ベクトル    */
  double *parcor_coef;   /* PARCOR係数ベクトル */
  uint8_t alloced_by_own; /* 自分で領域確保したか？ */
  void *work;             /* ワーク領域先頭ポインタ */
};

/*（標本）自己相関の計算 */
static LPCCalculatorError LPCCalculator_CalculateAutoCorrelation(
    const double *data, uint32_t num_samples,
    double *auto_corr, uint32_t order);
/* Levinson-Durbin再帰計算 */
static LPCCalculatorError LPCCalculator_LevinsonDurbinRecursion(
    struct LPCCalculator *lpc, const double *auto_corr,
    double *lpc_coef, double *parcor_coef, uint32_t order);
/* 係数計算の共通関数 */
static LPCCalculatorError LPCCalculator_CalculateCoef(
    struct LPCCalculator *lpc, 
    const double *data, uint32_t num_samples, uint32_t order);

/* log2関数（C89で定義されていない） */
static double LPCCalculator_Log2(double x);

/* LPC係数計算ハンドルのワークサイズ計算 */
int32_t LPCCalculator_CalculateWorkSize(uint32_t max_order)
{
  int32_t work_size;

  /* 引数チェック */
  if (max_order == 0) {
    return -1;
  }

  work_size = sizeof(struct LPCCalculator) + LPCCALCULATOR_ALIGNMENT;
  work_size += sizeof(double) * (max_order + 2) * 4; /* a, e, u, v ベクトル分の領域 */
  work_size += sizeof(double) * (max_order + 1); /* 標本自己相関の領域 */
  work_size += sizeof(double) * (max_order + 1) * 2; /* 係数ベクトルの領域 */

  return work_size;
}

/* LPC係数計算ハンドルの作成 */
struct LPCCalculator *LPCCalculator_Create(uint32_t max_order, void *work, int32_t work_size)
{
  struct LPCCalculator *lpcc;
  uint8_t *work_ptr;
  uint8_t tmp_alloc_by_own = 0;

  /* 自前でワーク領域確保 */
  if ((work == NULL) && (work_size == 0)) {
    if ((work_size = LPCCalculator_CalculateWorkSize(max_order)) < 0) {
      return NULL;
    }
    work = malloc((uint32_t)work_size);
    tmp_alloc_by_own = 1;
  }

  /* 引数チェック */
  if ((work == NULL) || (work_size < LPCCalculator_CalculateWorkSize(max_order))) {
    if (tmp_alloc_by_own == 1) {
      free(work);
    }
    return NULL;
  }

  /* ワーク領域取得 */
  work_ptr = (uint8_t *)work;

  /* ハンドル領域確保 */
  work_ptr = (uint8_t *)LPCCALCULATOR_ROUNDUP((uintptr_t)work_ptr, LPCCALCULATOR_ALIGNMENT);
  lpcc = (struct LPCCalculator *)work_ptr;
  work_ptr += sizeof(struct LPCCalculator);

  /* ハンドルメンバの設定 */
  lpcc->max_order = max_order;
  lpcc->work = work;
  lpcc->alloced_by_own = tmp_alloc_by_own;

  /* 計算用ベクトルの領域割当 */
  lpcc->a_vec = (double *)work_ptr;
  work_ptr += sizeof(double) * (max_order + 2); /* a_0, a_k+1を含めるとmax_order+2 */
  lpcc->e_vec = (double *)work_ptr;
  work_ptr += sizeof(double) * (max_order + 2); /* e_0, e_k+1を含めるとmax_order+2 */
  lpcc->u_vec = (double *)work_ptr;
  work_ptr += sizeof(double) * (max_order + 2);
  lpcc->v_vec = (double *)work_ptr;
  work_ptr += sizeof(double) * (max_order + 2);

  /* 標本自己相関の領域割当 */
  lpcc->auto_corr = (double *)work_ptr;
  work_ptr += sizeof(double) * (max_order + 1);

  /* 係数ベクトルの領域割当 */
  lpcc->lpc_coef = (double *)work_ptr;
  work_ptr += sizeof(double) * (max_order + 1);
  lpcc->parcor_coef = (double *)work_ptr;
  work_ptr += sizeof(double) * (max_order + 1);

  return lpcc;
}

/* LPC係数計算ハンドルの破棄 */
void LPCCalculator_Destroy(struct LPCCalculator *lpcc)
{
  if (lpcc != NULL) {
    /* ワーク領域を時前確保していたときは開放 */
    if (lpcc->alloced_by_own == 1) {
      free(lpcc->work);
    }
  }
}

/* Levinson-Durbin再帰計算によりLPC係数を求める */
/* 係数parcor_coefはorder+1個の配列 */
LPCCalculatorApiResult LPCCalculator_CalculateLPCCoef(
    struct LPCCalculator *lpcc,
    const double *data, uint32_t num_samples,
    double *lpc_coef, uint32_t order)
{
  /* 引数チェック */
  if ((lpcc == NULL) || (data == NULL) || (lpc_coef == NULL)) {
    return LPCCALCULATOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* 次数チェック */
  if (order > lpcc->max_order) {
    return LPCCALCULATOR_APIRESULT_EXCEED_MAX_ORDER;
  }

  /* 係数計算 */
  if (LPCCalculator_CalculateCoef(lpcc, data, num_samples, order) != LPCCALCULATOR_ERROR_OK) {
    return LPCCALCULATOR_APIRESULT_FAILED_TO_CALCULATION;
  }

  /* 計算成功時は結果をコピー */
  /* parcor_coef と lpc->parcor_coef が同じ場所を指しているときもあるのでmemmove */
  memmove(lpc_coef, lpcc->parcor_coef, sizeof(double) * (order + 1));

  return LPCCALCULATOR_APIRESULT_OK;
}

/* Levinson-Durbin再帰計算によりPARCOR係数を求める（倍精度） */
/* 係数parcor_coefはorder+1個の配列 */
LPCCalculatorApiResult  LPCCalculator_CalculatePARCORCoef(
    struct LPCCalculator *lpcc,
    const double *data, uint32_t num_samples,
    double *parcor_coef, uint32_t order)
{
  /* 引数チェック */
  if (lpcc == NULL || data == NULL || parcor_coef == NULL) {
    return LPCCALCULATOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* 次数チェック */
  if (order > lpcc->max_order) {
    return LPCCALCULATOR_APIRESULT_EXCEED_MAX_ORDER;
  }

  /* 係数計算 */
  if (LPCCalculator_CalculateCoef(lpcc, data, num_samples, order) != LPCCALCULATOR_ERROR_OK) {
    return LPCCALCULATOR_APIRESULT_FAILED_TO_CALCULATION;
  }

  /* 計算成功時は結果をコピー */
  /* parcor_coef と lpc->parcor_coef が同じ場所を指しているときもあるのでmemmove */
  memmove(parcor_coef, lpcc->parcor_coef, sizeof(double) * (order + 1));

  return LPCCALCULATOR_APIRESULT_OK;
}

/* 係数計算の共通関数 */
static LPCCalculatorError LPCCalculator_CalculateCoef(
    struct LPCCalculator *lpc, 
    const double *data, uint32_t num_samples, uint32_t order)
{
  /* 引数チェック */
  if (lpc == NULL) {
    return LPCCALCULATOR_ERROR_INVALID_ARGUMENT;
  }

  /* 自己相関を計算 */
  if (LPCCalculator_CalculateAutoCorrelation(
        data, num_samples, lpc->auto_corr, order + 1) != LPCCALCULATOR_ERROR_OK) {
    return LPCCALCULATOR_ERROR_NG;
  }

  /* 入力サンプル数が少ないときは、係数が発散することが多数
   * => 無音データとして扱い、係数はすべて0とする */
  if (num_samples < order) {
    uint32_t ord;
    for (ord = 0; ord < order + 1; ord++) {
      lpc->lpc_coef[ord] = lpc->parcor_coef[ord] = 0.0f;
    }
    return LPCCALCULATOR_ERROR_OK;
  }

  /* 再帰計算を実行 */
  if (LPCCalculator_LevinsonDurbinRecursion(
        lpc, lpc->auto_corr,
        lpc->lpc_coef, lpc->parcor_coef, order) != LPCCALCULATOR_ERROR_OK) {
    return LPCCALCULATOR_ERROR_NG;
  }

  return LPCCALCULATOR_ERROR_OK;
}

/* Levinson-Durbin再帰計算 */
static LPCCalculatorError LPCCalculator_LevinsonDurbinRecursion(
    struct LPCCalculator *lpc, const double *auto_corr,
    double *lpc_coef, double *parcor_coef, uint32_t order)
{
  uint32_t delay, i;
  double gamma;      /* 反射係数 */
  /* オート変数にポインタをコピー */
  double *a_vec = lpc->a_vec;
  double *e_vec = lpc->e_vec;
  double *u_vec = lpc->u_vec;
  double *v_vec = lpc->v_vec;

  /* 引数チェック */
  if (lpc == NULL || lpc_coef == NULL || auto_corr == NULL) {
    return LPCCALCULATOR_ERROR_INVALID_ARGUMENT;
  }

  /* 
   * 0次自己相関（信号の二乗和）が小さい場合
   * => 係数は全て0として無音出力システムを予測.
   */
  if (fabs(auto_corr[0]) < FLT_EPSILON) {
    for (i = 0; i < order + 1; i++) {
      lpc_coef[i] = parcor_coef[i] = 0.0f;
    }
    return LPCCALCULATOR_ERROR_OK;
  }

  /* 初期化 */
  for (i = 0; i < order + 2; i++) {
    a_vec[i] = u_vec[i] = v_vec[i] = 0.0f;
  }

  /* 最初のステップの係数をセット */
  a_vec[0]        = 1.0f;
  e_vec[0]        = auto_corr[0];
  a_vec[1]        = - auto_corr[1] / auto_corr[0];
  parcor_coef[0]  = 0.0f;
  parcor_coef[1]  = auto_corr[1] / e_vec[0];
  e_vec[1]        = auto_corr[0] + auto_corr[1] * a_vec[1];
  u_vec[0]        = 1.0f; u_vec[1] = 0.0f; 
  v_vec[0]        = 0.0f; v_vec[1] = 1.0f; 

  /* 再帰処理 */
  for (delay = 1; delay < order; delay++) {
    gamma = 0.0f;
    for (i = 0; i < delay + 1; i++) {
      gamma += a_vec[i] * auto_corr[delay + 1 - i];
    }
    gamma /= (-e_vec[delay]);
    e_vec[delay + 1] = (1.0f - gamma * gamma) * e_vec[delay];
    /* 誤差分散（パワー）は非負 */
    assert(e_vec[delay] >= 0.0f);

    /* u_vec, v_vecの更新 */
    for (i = 0; i < delay; i++) {
      u_vec[i + 1] = v_vec[delay - i] = a_vec[i + 1];
    }
    u_vec[0] = 1.0f; u_vec[delay+1] = 0.0f;
    v_vec[0] = 0.0f; v_vec[delay+1] = 1.0f;

    /* 係数の更新 */
    for (i = 0; i < delay + 2; i++) {
       a_vec[i] = u_vec[i] + gamma * v_vec[i];
    }
    /* PARCOR係数は反射係数の符号反転 */
    parcor_coef[delay + 1] = -gamma;
    /* PARCOR係数の絶対値は1未満（収束条件） */
    assert(fabs(gamma) < 1.0f);
  }

  /* 結果を取得 */
  memcpy(lpc_coef, a_vec, sizeof(double) * (order + 1));

  return LPCCALCULATOR_ERROR_OK;
}

/*（標本）自己相関の計算 */
static LPCCalculatorError LPCCalculator_CalculateAutoCorrelation(
    const double *data, uint32_t num_samples,
    double *auto_corr, uint32_t order)
{
  uint32_t i, lag;

  /* 引数チェック */
  if (data == NULL || auto_corr == NULL) {
    return LPCCALCULATOR_ERROR_INVALID_ARGUMENT;
  }

  /* 次数（最大ラグ）がサンプル数を超えている */
  /* -> 次数をサンプル数に制限 */
  if (order > num_samples) {
    order = num_samples;
  }

  /* 自己相関初期化 */
  for (i = 0; i < order; i++) {
    auto_corr[i] = 0.0f;
  }

  /* 0次は係数は単純計算 */
  for (i = 0; i < num_samples; i++) {
    auto_corr[0] += data[i] * data[i];
  }

  /* 1次以降の係数 */
  for (lag = 1; lag < order; lag++) {
    uint32_t i, l, L;
    uint32_t Llag2;
    const uint32_t lag2 = lag << 1;

    /* 被乗数が重複している連続した項の集まりの数 */
    if ((3 * lag) < num_samples) {
      L = 1 + (num_samples - (3 * lag)) / lag2;
    } else {
      L = 0;
    }
    Llag2 = L * lag2;

    /* 被乗数が重複している分を積和 */
    for (i = 0; i < lag; i++) {
      for (l = 0; l < Llag2; l += lag2) {
        /* 一般的に lag < L なので、ループはこの順 */
        auto_corr[lag] += data[l + lag + i] * (data[l + i] + data[l + lag2 + i]);
      }
    }

    /* 残りの項を単純に積和（TODO:この中でも更にまとめることはできる...） */
    for (i = 0; i < (num_samples - Llag2 - lag); i++) {
      auto_corr[lag] += data[Llag2 + lag + i] * data[Llag2 + i];
    }

  }

  return LPCCALCULATOR_ERROR_OK;
}

/* log2関数（C89で定義されていない） */
static double LPCCalculator_Log2(double x)
{
#define INV_LOGE2 (1.4426950408889634)  /* 1 / log(2) */
  return log(x) * INV_LOGE2;
#undef INV_LOGE2
}

/* 入力データとPARCOR係数からサンプルあたりの推定符号長を求める */
LPCCalculatorApiResult LPCCalculator_EstimateCodeLength(
    const double *data, uint32_t num_samples, uint32_t bits_per_sample,
    const double *parcor_coef, uint32_t order,
    double *length_per_sample_bits)
{
  uint32_t smpl, ord;
  double log2_mean_res_power, log2_var_ratio;

  /* 定数値 */
#define BETA_CONST_FOR_LAPLACE_DIST   (1.9426950408889634)  /* sqrt(2 * E * E) */
#define BETA_CONST_FOR_GAUSS_DIST     (2.047095585180641)   /* sqrt(2 * E * PI) */
  /* 引数チェック */
  if ((data == NULL) || (parcor_coef == NULL) || (length_per_sample_bits == NULL)) {
    return LPCCALCULATOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* log2(パワー平均)の計算 */
  log2_mean_res_power = 0.0f;
  for (smpl = 0; smpl < num_samples; smpl++) {
    log2_mean_res_power += data[smpl] * data[smpl];
  }
  /* 整数PCMの振幅に変換（doubleの密度保障） */
  log2_mean_res_power *= pow(2, (double)(2 * (bits_per_sample - 1)));
  if (fabs(log2_mean_res_power) <= FLT_MIN) {
    /* ほぼ無音だった場合は符号長を0とする */
    (*length_per_sample_bits) = 0.0f;
    return LPCCALCULATOR_APIRESULT_OK;
  } 
  log2_mean_res_power = LPCCalculator_Log2((double)log2_mean_res_power) - LPCCalculator_Log2((double)num_samples);

  /* sum(log2(1 - (parcor * parcor)))の計算 */
  /* 1次の係数は0で確定だから飛ばす */
  log2_var_ratio = 0.0f;
  for (ord = 1; ord <= order; ord++) {
    log2_var_ratio += LPCCalculator_Log2(1.0f - parcor_coef[ord] * parcor_coef[ord]);
  }

  /* エントロピー計算 */
  /* →サンプルあたりの最小のビット数が得られる */
  (*length_per_sample_bits) = BETA_CONST_FOR_LAPLACE_DIST + 0.5f * (log2_mean_res_power + log2_var_ratio);

  /* 推定ビット数が負値の場合は、1サンプルあたり1ビットで符号化できることを期待する */
  /* 補足）このケースは入力音声パワーが非常に低い */
  if ((*length_per_sample_bits) <= 0) {
    (*length_per_sample_bits) = 1.0f;
    return LPCCALCULATOR_APIRESULT_OK;
  }

#undef BETA_CONST_FOR_LAPLACE_DIST
#undef BETA_CONST_FOR_GAUSS_DIST
  
  return LPCCALCULATOR_APIRESULT_OK;
}

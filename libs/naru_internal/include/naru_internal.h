#ifndef NARU_INTERNAL_H_INCLUDED
#define NARU_INTERNAL_H_INCLUDED

#include "naru.h"

/* 内部エンコードパラメータ */
#define NARU_BLOCK_SYNC_CODE                  0xFFFF  /* ブロック先頭の同期コード                 */
#define NARUCODER_NUM_RECURSIVERICE_PARAMETER 2       /* 再帰的ライス符号のパラメータ数 */
#define NARUCODER_QUOTPART_THRESHOULD         16      /* 再帰的ライス符号の商部分の閾値 これ以上の大きさの商はガンマ符号化 */
#define NARU_FIXEDPOINT_DIGITS                15      /* 固定小数点の小数桁 */
#define NARU_FIXEDPOINT_0_5                   (1 << (NARU_FIXEDPOINT_DIGITS - 1)) /* 固定小数点の0.5 */
#define NARU_EMPHASIS_FILTER_SHIFT            5                                   /* プリ（デ）エンファシスフィルタのシフト量  */
/* NGSAのステップサイズに乗じる係数のビット幅 */
#define NARUNGSA_STEPSIZE_SCALE_BITWIDTH 10
/* NGSAのステップサイズに乗じる係数の右シフト量 */
#define NARUNGSA_STEPSIZE_SCALE_SHIFT 6
/* SAの右シフト量 */
#define NARUSA_STEPSIZE_SHIFT 2
/* フィルタ係数のbit幅 */
#define NARU_FILTER_WEIGHT_RANGE_BITWIDTH 18
/* ブロックヘッダに記録するデータのビット幅 */
#define NARU_BLOCKHEADER_DATA_BITWIDTH 8
/* ブロックヘッダに記録するデータのシフト数のビット幅 */
#define NARU_BLOCKHEADER_SHIFT_BITWIDTH 4

/* NULLチェックと領域解放 */
#define NARU_NULLCHECK_AND_FREE(ptr)\
  do {\
    if ((ptr) != NULL) {\
      free(ptr);\
      (ptr) = NULL;\
    }\
  } while (0);

/* アサートマクロ */
#ifdef NDEBUG
/* 未使用変数警告を明示的に回避 */
#define NARU_ASSERT(condition) ((void)(condition))
#else
#include <assert.h>
#define NARU_ASSERT(condition) assert(condition)
#endif

/* 静的アサートマクロ */
#define NARU_STATIC_ASSERT(expr) extern void assertion_failed(char dummy[(expr) ? 1 : -1])

/* ブロックデータタイプ */
typedef enum NARUBlockDataTypeTag {
  NARU_BLOCK_DATA_TYPE_COMPRESSDATA  = 0,     /* 圧縮済みデータ */
  NARU_BLOCK_DATA_TYPE_SILENT        = 1,     /* 無音データ     */
  NARU_BLOCK_DATA_TYPE_RAWDATA       = 2,     /* 生データ       */
  NARU_BLOCK_DATA_TYPE_INVAILD       = 3      /* 無効           */
} NARUBlockDataType;

/* 内部エラー型 */
typedef enum NARUErrorTag {
  NARU_ERROR_OK = 0,              /* OK */
  NARU_ERROR_NG,                  /* 分類不能な失敗 */
  NARU_ERROR_INVALID_ARGUMENT,    /* 不正な引数 */
  NARU_ERROR_INVALID_FORMAT,      /* 不正なフォーマット       */
  NARU_ERROR_INSUFFICIENT_BUFFER, /* バッファサイズが足りない */
  NARU_ERROR_INSUFFICIENT_DATA    /* データサイズが足りない   */
} NARUError;

/* NGSAフィルタ */
struct NGSAFilter {
  int32_t filter_order;
  int32_t ar_order;
  int32_t history[NARU_MAX_FILTER_ORDER];   /* 入力データ履歴 */
  int32_t weight[NARU_MAX_FILTER_ORDER];    /* フィルタ係数 */
  int32_t ar_coef[NARU_MAX_AR_ORDER];       /* AR係数 */
  int32_t ngrad[NARU_MAX_FILTER_ORDER];     /* 自然勾配 */
  int32_t stepsize_scale;                   /* ステップサイズに乗じる係数 */
};

/* SAフィルタ */
struct SAFilter {
  int32_t filter_order;
  int32_t history[NARU_MAX_FILTER_ORDER];   /* 入力データ履歴 */
  int32_t weight[NARU_MAX_FILTER_ORDER];    /* フィルタ係数 */
};

#endif /* NARU_INTERNAL_H_INCLUDED */

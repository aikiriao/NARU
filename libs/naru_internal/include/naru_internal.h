#ifndef NARU_INTERNAL_H_INCLUDED
#define NARU_INTERNAL_H_INCLUDED

/* 内部エンコードパラメータ */
#define NARU_BLOCK_SYNC_CODE                         0xFFFF  /* ブロック先頭の同期コード                 */
#define NARUCODER_NUM_RECURSIVERICE_PARAMETER        2       /* 再帰的ライス符号のパラメータ数 */
#define NARUCODER_QUOTPART_THRESHOULD                16      /* 再帰的ライス符号の商部分の閾値 これ以上の大きさの商はガンマ符号化 */

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
#define NARU_Assert(condition) ((void)(condition))
#else
#include <assert.h>
#define NARU_Assert(condition) assert(condition)
#endif

/* ブロックデータタイプ */
typedef enum NARUBlockDataTypeTag {
  NARU_BLOCK_DATA_TYPE_COMPRESSDATA  = 0,     /* 圧縮済みデータ */
  NARU_BLOCK_DATA_TYPE_SILENT        = 1,     /* 無音データ     */
  NARU_BLOCK_DATA_TYPE_RAWDATA       = 2,     /* 生データ       */
  NARU_BLOCK_DATA_TYPE_INVAILD       = 3      /* 無効           */
} NARUBlockDataType;

#endif /* NARU_INTERNAL_H_INCLUDED */

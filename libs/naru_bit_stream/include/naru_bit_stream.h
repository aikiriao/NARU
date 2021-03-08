#ifndef NARU_BITSTREAM_H_INCLUDED
#define NARU_BITSTREAM_H_INCLUDED

#include "naru_stdint.h"
#include <stdio.h>

/* マクロ展開して処理を行うか？ */
#define NARUBITSTREAM_PROCESS_BY_MACRO

/* NARUBitStream_Seek関数の探索コード */
#define NARUBITSTREAM_SEEK_SET  (int32_t)SEEK_SET
#define NARUBITSTREAM_SEEK_CUR  (int32_t)SEEK_CUR
#define NARUBITSTREAM_SEEK_END  (int32_t)SEEK_END

/* 読みモードか？（0で書きモード） */
#define NARUBITSTREAM_FLAGS_MODE_READ  (1 << 0)

/* ビットストリーム構造体 */
struct NARUBitStream {
  uint32_t        bit_buffer;
  uint32_t        bit_count;
  const uint8_t*  memory_image;
  size_t          memory_size;
  uint8_t*        memory_p;
  uint8_t         flags;
};

#if defined(NARUBITSTREAM_PROCESS_BY_MACRO) 

#include "naru_internal.h"
#include "naru_utility.h"

/* valの下位nbitsを取得 */
#define NARUBITSTREAM_GETLOWERBITS(val, nbits) ((val) & g_naru_bitstream_lower_bits_mask[(nbits)])

/* マクロ処理を行う場合の実装をヘッダに展開 */

/* 下位ビットを取り出すためのマスク */
extern const uint32_t g_naru_bitstream_lower_bits_mask[33];

/* ラン長のパターンテーブル */
extern const uint32_t g_naru_bitstream_zerobit_runlength_table[0x100];

/* ビットリーダのオープン */
#define NARUBitReader_Open(stream, memory, size)\
  do {\
    /* 引数チェック */\
    NARU_ASSERT((void *)(stream) != NULL);\
    NARU_ASSERT((void *)(memory) != NULL);\
\
    /* 内部状態リセット */\
    (stream)->flags = 0;\
\
    /* バッファ初期化 */\
    (stream)->bit_count   = 0;\
    (stream)->bit_buffer  = 0;\
\
    /* メモリセット */\
    (stream)->memory_image = (memory);\
    (stream)->memory_size  = (size);\
\
    /* 読み出し位置は先頭に */\
    (stream)->memory_p = (memory);\
\
    /* 読みモードとしてセット */\
    (stream)->flags |= (uint8_t)NARUBITSTREAM_FLAGS_MODE_READ;\
  } while (0)

/* ビットライタのオープン */
#define NARUBitWriter_Open(stream, memory, size)\
  do {\
    /* 引数チェック */\
    NARU_ASSERT((void *)(stream) != NULL);\
    NARU_ASSERT((void *)(memory) != NULL);\
\
    /* 内部状態リセット */\
    (stream)->flags = 0;\
\
    /* バッファ初期化 */\
    (stream)->bit_count   = 8;\
    (stream)->bit_buffer  = 0;\
\
    /* メモリセット */\
    (stream)->memory_image = (memory);\
    (stream)->memory_size  = (size);\
\
    /* 読み出し位置は先頭に */\
    (stream)->memory_p = (memory);\
\
    /* 書きモードとしてセット */\
    (stream)->flags &= (uint8_t)(~NARUBITSTREAM_FLAGS_MODE_READ);\
  } while (0)

/* ビットストリームのクローズ */
#define NARUBitStream_Close(stream)\
  do {\
    /* 引数チェック */\
    NARU_ASSERT((void *)(stream) != NULL);\
\
    /* 残ったデータをフラッシュ */\
    NARUBitStream_Flush(stream);\
\
    /* バッファのクリア */\
    (stream)->bit_buffer = 0;\
\
    /* メモリ情報のクリア */\
    (stream)->memory_image = NULL;\
    (stream)->memory_size  = 0;\
\
    /* 内部状態のクリア */\
    (stream)->memory_p     = NULL;\
    (stream)->flags        = 0;\
  } while (0)

/* シーク(fseek準拠) */
#define NARUBitStream_Seek(stream, offset, origin)\
  do {\
    uint8_t* __pos = NULL;\
\
    /* 引数チェック */\
    NARU_ASSERT((void *)(stream) != NULL);\
\
    /* 内部バッファをクリア（副作用が起こる） */\
    NARUBitStream_Flush(stream);\
\
    /* 起点をまず定める */\
    switch (origin) {\
      case NARUBITSTREAM_SEEK_CUR:\
        __pos = (stream)->memory_p;\
        break;\
      case NARUBITSTREAM_SEEK_SET:\
        __pos = (uint8_t *)(stream)->memory_image;\
        break;\
      case NARUBITSTREAM_SEEK_END:\
        __pos = (uint8_t *)((stream)->memory_image\
            + ((stream)->memory_size - 1));\
        break;\
      default:\
        NARU_ASSERT(0);\
    }\
\
    /* オフセット分動かす */\
    __pos += (offset);\
\
    /* 範囲チェック */\
    NARU_ASSERT(__pos\
        < ((stream)->memory_image + (stream)->memory_size));\
    NARU_ASSERT(__pos >= (stream)->memory_image);\
\
    /* 結果の保存 */\
    (stream)->memory_p = __pos;\
  } while (0)

/* 現在位置(ftell)準拠 */
#define NARUBitStream_Tell(stream, result)\
  do {\
    /* 引数チェック */\
    NARU_ASSERT((void *)(stream) != NULL);\
    NARU_ASSERT((void *)(result) != NULL);\
\
    /* アクセスオフセットを返す */\
    (*result) = (int32_t)\
      ((stream)->memory_p - (stream)->memory_image);\
  } while (0)

/* valの右側（下位）nbits 出力（最大32bit出力可能） */
#define NARUBitWriter_PutBits(stream, val, nbits)\
  do {\
    uint32_t __nbits;\
\
    /* 引数チェック */\
    NARU_ASSERT((void *)(stream) != NULL);\
\
    /* 読み込みモードでは実行不可能 */\
    NARU_ASSERT(!((stream)->flags & NARUBITSTREAM_FLAGS_MODE_READ));\
\
    /* 出力可能な最大ビット数を越えている */\
    NARU_ASSERT((nbits) <= (sizeof(uint32_t) * 8));\
\
    /* 0ビット出力は冗長なのでアサートで落とす */\
    NARU_ASSERT((nbits) > 0);\
\
    /* valの上位ビットから順次出力\
     * 初回ループでは端数（出力に必要なビット数）分を埋め出力\
     * 2回目以降は8bit単位で出力 */\
    __nbits = (nbits);\
    while (__nbits >= (stream)->bit_count) {\
      __nbits -= (stream)->bit_count;\
      (stream)->bit_buffer\
        |= (uint32_t)NARUBITSTREAM_GETLOWERBITS(\
              (uint32_t)(val) >> __nbits, (stream)->bit_count);\
\
      /* 終端に達していないかチェック */\
      NARU_ASSERT((stream)->memory_p\
          < ((stream)->memory_p + (stream)->memory_size));\
\
      /* メモリに書き出し */\
      (*(stream)->memory_p) = ((stream)->bit_buffer & 0xFF);\
      (stream)->memory_p++;\
\
      /* バッファをリセット */\
      (stream)->bit_buffer  = 0;\
      (stream)->bit_count   = 8;\
    }\
\
    /* 端数ビットの処理:\
     * 残った分をバッファの上位ビットにセット */\
    NARU_ASSERT(__nbits <= 8);\
    (stream)->bit_count  -= __nbits;\
    (stream)->bit_buffer\
      |= (uint32_t)NARUBITSTREAM_GETLOWERBITS(\
            (uint32_t)(val), __nbits) << (stream)->bit_count;\
  } while (0)

/* nbits 取得（最大32bit）し、その値を右詰めして出力 */
#define NARUBitReader_GetBits(stream, val, nbits)\
  do {\
    uint8_t  __ch;\
    uint32_t __nbits;\
    uint32_t __tmp = 0;\
\
    /* 引数チェック */\
    NARU_ASSERT((void *)(stream) != NULL);\
    NARU_ASSERT((void *)(val) != NULL);\
\
    /* 読み込みモードでない場合はアサート */\
    NARU_ASSERT((stream)->flags & NARUBITSTREAM_FLAGS_MODE_READ);\
\
    /* 入力可能な最大ビット数を越えている */\
    NARU_ASSERT((nbits) <= (sizeof(uint32_t) * 8));\
\
    /* 最上位ビットからデータを埋めていく\
     * 初回ループではtmpの上位ビットにセット\
     * 2回目以降は8bit単位で入力しtmpにセット */\
    __nbits = (nbits);\
    while (__nbits > (stream)->bit_count) {\
      __nbits -= (stream)->bit_count;\
      __tmp\
        |= NARUBITSTREAM_GETLOWERBITS(\
          (stream)->bit_buffer, (stream)->bit_count) << __nbits;\
\
      /* 終端に達していないかチェック */\
      NARU_ASSERT((stream)->memory_p\
          < ((stream)->memory_p + (stream)->memory_size));\
\
      /* メモリから読み出し */\
      __ch = (*(stream)->memory_p);\
      (stream)->memory_p++;\
\
      (stream)->bit_buffer  = __ch;\
      (stream)->bit_count   = 8;\
    }\
\
    /* 端数ビットの処理\
     * 残ったビット分をtmpの最上位ビットにセット */\
    (stream)->bit_count -= __nbits;\
    __tmp\
      |= (uint32_t)NARUBITSTREAM_GETLOWERBITS(\
            (stream)->bit_buffer >> (stream)->bit_count, __nbits);\
\
    /* 正常終了 */\
    (*val) = __tmp;\
  } while (0)

/* つぎの1にぶつかるまで読み込み、その間に読み込んだ0のランレングスを取得 */
#define NARUBitReader_GetZeroRunLength(stream, runlength)\
  do {\
    uint32_t __run;\
\
    /* 引数チェック */\
    NARU_ASSERT((void *)(stream) != NULL);\
    NARU_ASSERT((void *)(runlength) != NULL);\
\
    /* 上位ビットからの連続する0をNLZで計測 */\
    /* (1 << (31 - (stream)->bit_count)) はラン長が\
     * 伸びすぎないようにするためのビット */\
    __run = NARUUTILITY_NLZ(\
        (uint32_t)(\
          ((stream)->bit_buffer << (32 - (stream)->bit_count))\
            | (1UL << (31 - (stream)->bit_count)))\
          );\
\
    /* 読み込んだ分カウントを減らす */\
    (stream)->bit_count -= __run;\
\
    /* バッファが空の時 */\
    while ((stream)->bit_count == 0) {\
      /* 1バイト読み込みとエラー処理 */\
      uint8_t   __ch;\
      uint32_t  __tmp_run;\
\
      /* 終端に達していないかチェック */\
      NARU_ASSERT((stream)->memory_p\
          < ((stream)->memory_p + (stream)->memory_size));\
\
      /* メモリから読み出し */\
      __ch = (*(stream)->memory_p);\
      (stream)->memory_p++;\
\
      /* ビットバッファにセットし直して再度ランを計測 */\
      (stream)->bit_buffer  = __ch;\
      /* テーブルによりラン長を取得 */\
      __tmp_run\
        = g_naru_bitstream_zerobit_runlength_table[(stream)->bit_buffer];\
      (stream)->bit_count   = 8 - __tmp_run;\
      /* ランを加算 */\
      __run                 += __tmp_run;\
    }\
\
    /* 続く1を空読み */\
    (stream)->bit_count -= 1;\
\
    /* 正常終了 */\
    (*(runlength)) = __run;\
  } while (0)

/* バッファにたまったビットをクリア */
#define NARUBitStream_Flush(stream)\
  do {\
    /* 引数チェック */\
    NARU_ASSERT((void *)(stream) != NULL);\
\
    /* 既に先頭にあるときは何もしない */\
    if ((stream)->bit_count < 8) {\
      /* 読み込み位置を次のバイト先頭に */\
      if ((stream)->flags & NARUBITSTREAM_FLAGS_MODE_READ) {\
        /* 残りビット分を空読み */\
        uint32_t dummy;\
        NARUBitReader_GetBits((stream), &dummy, (stream)->bit_count);\
      } else {\
        /* バッファに余ったビットを強制出力 */\
        NARUBitWriter_PutBits((stream), 0, (stream)->bit_count);\
      }\
    }\
  } while (0)

#else /* NARUBITSTREAM_PROCESS_BY_MACRO */

#ifdef __cplusplus
extern "C" {
#endif 

/* ビットライタのオープン */
void NARUBitWriter_Open(struct NARUBitStream* stream, uint8_t* memory_image, size_t memory_size);

/* ビットリーダのオープン */
void NARUBitReader_Open(struct NARUBitStream* stream, uint8_t* memory_image, size_t memory_size);

/* ビットストリームのクローズ */
void NARUBitStream_Close(struct NARUBitStream* stream);

/* シーク(fseek準拠)
 * 注意）バッファをクリアするので副作用がある */
void NARUBitStream_Seek(struct NARUBitStream* stream, int32_t offset, int32_t origin);

/* 現在位置(ftell)準拠 */
void NARUBitStream_Tell(struct NARUBitStream* stream, int32_t* result);

/* valの右側（下位）n_bits 出力（最大32bit出力可能） */
void NARUBitWriter_PutBits(struct NARUBitStream* stream, uint32_t val, uint32_t nbits);

/* n_bits 取得（最大32bit）し、その値を右詰めして出力 */
void NARUBitReader_GetBits(struct NARUBitStream* stream, uint32_t* val, uint32_t nbits);

/* つぎの1にぶつかるまで読み込み、その間に読み込んだ0のランレングスを取得 */
void NARUBitReader_GetZeroRunLength(struct NARUBitStream* stream, uint32_t* runlength);

/* バッファにたまったビットをクリア */
void NARUBitStream_Flush(struct NARUBitStream* stream);

#ifdef __cplusplus
}
#endif 

#endif /* NARUBITSTREAM_PROCESS_BY_MACRO */

#endif /* NARU_BITSTREAM_H_INCLUDED */

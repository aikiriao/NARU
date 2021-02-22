#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "bit_stream.h"

/* アラインメント */
#define BITSTREAM_ALIGNMENT                   16
/* 読みモードか？（0で書きモード） */
#define BITSTREAM_FLAGS_FILEOPENMODE_READ     (1 << 0)
/* メモリ操作モードか否か？ */
#define BITSTREAM_FLAGS_READWRITE_ON_MEMORY   (1 << 1)
/* メモリはワーク渡しか？（1:ワーク渡し, 0:mallocで自前確保） */
#define BITSTREAM_FLAGS_MEMORYALLOC_BYWORK    (1 << 2)

/* 下位n_bitsを取得 */
/* 補足）((1UL << (n_bits)) - 1)は下位の数値だけ取り出すマスクになる */
#define BITSTREAM_GETLOWERBITS(n_bits, val) ((val) & ((1UL << (n_bits)) - 1))

/* 内部エラー型 */
typedef enum BitStreamErrorTag {
  BITSTREAM_ERROR_OK = 0,
  BITSTREAM_ERROR_NG,
  BITSTREAM_ERROR_EOS,
  BITSTREAM_ERROR_IOERROR,
} BitStreamError;

/* ストリーム */
typedef union StreamTag {
  FILE* fp;                   /* ファイル構造体   */
  struct {
    uint8_t*  memory_image;   /* メモリイメージ   */
    uint64_t  memory_size;    /* メモリサイズ     */
    uint64_t  memory_p;       /* 読み書き参照位置 */
  } mem;
} Stream;

/* ストリームに対する操作インターフェース */
typedef struct BitStreamInterfaceTag {
  /* 1文字読み関数 */
  BitStreamError (*SGetChar)(Stream* strm, int32_t* character);
  /* 1文字書き関数 */
  BitStreamError (*SPutChar)(Stream* strm, int32_t  character);
  /* シーク関数 */
  BitStreamError (*SSeek)(Stream* strm, int32_t offset, int32_t wherefrom);
  /* tell関数 */
  BitStreamError (*STell)(const Stream* strm, int32_t* result);
} BitStreamInterface;

/* ビットストリーム構造体 */
struct BitStream {
  Stream                      stm;        /* ストリーム                 */
  const BitStreamInterface*   stm_if;     /* ストリームインターフェース */
  uint8_t                     flags;      /* 内部状態フラグ             */
  uint8_t                     bit_buffer; /* 内部ビット入出力バッファ   */
  int8_t                      bit_count;  /* 内部ビット入出力カウント   */
  void*                       work_ptr;   /* ワーク領域先頭ポインタ     */
};

/* オープン関数の共通ルーチン */
static struct BitStream* BitStream_OpenCommon(
    const char* mode, void *work, int32_t work_size);

/* ファイルに対する一文字取得 */
static BitStreamError BitStream_FGet(Stream* strm, int32_t* ch);
/* ファイルに対する一文字書き込み */
static BitStreamError BitStream_FPut(Stream* strm, int32_t ch);
/* ファイルに対するSeek関数 */
static BitStreamError BitStream_FSeek(Stream* strm, int32_t offset, int32_t wherefrom);
/* ファイルに対するTell関数 */
static BitStreamError BitStream_FTell(const Stream* strm, int32_t* result);
/* メモリに対する一文字取得 */
static BitStreamError BitStream_MGet(Stream* strm, int32_t* ch);
/* メモリに対する一文字書き込み */
static BitStreamError BitStream_MPut(Stream* strm, int32_t ch);
/* メモリに対するSeek関数 */
static BitStreamError BitStream_MSeek(Stream* strm, int32_t offset, int32_t wherefrom);
/* メモリに対するTell関数 */
static BitStreamError BitStream_MTell(const Stream* strm, int32_t* result);

/* ファイルIO用のインターフェース */
static const BitStreamInterface st_filestream_io_if = {
   BitStream_FGet,
   BitStream_FPut,
   BitStream_FSeek,
   BitStream_FTell,
};

/* メモリIO用のインターフェース */
static const BitStreamInterface st_memstream_io_if = {
   BitStream_MGet,
   BitStream_MPut,
   BitStream_MSeek,
   BitStream_MTell,
};

/* ファイルに対する一文字取得 */
static BitStreamError BitStream_FGet(Stream* strm, int32_t* ch)
{
  int32_t ret;

  assert(strm != NULL && ch != NULL);

  /* getcを発行 */
  if ((ret = getc(strm->fp)) == EOF) {
    return BITSTREAM_ERROR_EOS;
  }

  (*ch) = ret;
  return BITSTREAM_ERROR_OK;
}

/* ファイルに対する一文字書き込み */
static BitStreamError BitStream_FPut(Stream* strm, int32_t ch)
{
  assert(strm != NULL);
  if (fputc(ch, strm->fp) == EOF) {
    return BITSTREAM_ERROR_IOERROR;
  }
  return BITSTREAM_ERROR_OK;
}

/* ファイルに対するシーク関数 */
static BitStreamError BitStream_FSeek(Stream* strm, int32_t offset, int32_t wherefrom)
{
  assert(strm != NULL);
  return (fseek(strm->fp, offset, wherefrom) == 0) ? BITSTREAM_ERROR_OK : BITSTREAM_ERROR_IOERROR;
}

/* ファイルに対するTell関数 */
static BitStreamError BitStream_FTell(const Stream* strm, int32_t* result)
{
  assert(strm != NULL);
  /* ftell実行 */
  *result = ftell(strm->fp);
  return ((*result != -1) ? BITSTREAM_ERROR_OK : BITSTREAM_ERROR_IOERROR);
}

/* メモリに対する一文字取得 */
static BitStreamError BitStream_MGet(Stream* strm, int32_t* ch)
{
  assert(strm != NULL && ch != NULL);

  /* 終端に達している */
  if (strm->mem.memory_p >= strm->mem.memory_size) {
    return BITSTREAM_ERROR_EOS;
  }

  /* メモリから読み出し */
  (*ch) = strm->mem.memory_image[strm->mem.memory_p];
  strm->mem.memory_p++;
  return BITSTREAM_ERROR_OK;
}

/* メモリに対する一文字書き込み */
static BitStreamError BitStream_MPut(Stream* strm, int32_t ch)
{
  assert(strm != NULL);

  /* 終端に達している */
  if (strm->mem.memory_p >= strm->mem.memory_size) {
    return BITSTREAM_ERROR_EOS;
  }

  /* メモリに書き出し */
  strm->mem.memory_image[strm->mem.memory_p] = (uint8_t)(ch & 0xFF);
  strm->mem.memory_p++;
  return BITSTREAM_ERROR_OK;
}

/* メモリに対するSeek関数 */
static BitStreamError BitStream_MSeek(Stream* strm, int32_t offset, int32_t wherefrom)
{
  int64_t pos;

  assert(strm != NULL);

  /* オフセットをまず定める */
  switch (offset) {
    case BITSTREAM_SEEK_CUR:
      pos = strm->mem.memory_p;
      break;
    case BITSTREAM_SEEK_SET:
      pos = 0;
      break;
    case BITSTREAM_SEEK_END:
      pos = strm->mem.memory_size - 1;
      break;
    default:
      return BITSTREAM_ERROR_IOERROR;
  }

  /* 動かす */
  pos += wherefrom;

  /* 範囲チェック */
  if (pos >= (int64_t)strm->mem.memory_size || pos < 0) {
    return BITSTREAM_ERROR_NG;
  }

  strm->mem.memory_p = pos;
  return BITSTREAM_ERROR_OK;
}

/* メモリに対するTell関数 */
static BitStreamError BitStream_MTell(const Stream* strm, int32_t* result)
{
  assert(strm != NULL && result != NULL);

  /* 位置を返すだけ */
  (*result) = strm->mem.memory_p;
  return BITSTREAM_ERROR_OK;
}

/* ワークサイズの取得 */
int32_t BitStream_CalculateWorkSize(void)
{
  return (sizeof(struct BitStream) + BITSTREAM_ALIGNMENT);
}

/* メモリビットストリームのオープン */
struct BitStream* BitStream_OpenMemory(
    uint8_t* memory_image, uint64_t memory_size,
    const char* mode, void *work, int32_t work_size)
{
  struct BitStream* stream;

  /* 引数チェック */
  if (memory_image == NULL) {
    return NULL;
  }

  /* 共通インスタンス作成ルーチンを通す */
  stream = BitStream_OpenCommon(mode, work, work_size);
  if (stream == NULL) {
    return NULL;
  }

  /* このストリームはメモリ上 */
  stream->flags     |= BITSTREAM_FLAGS_READWRITE_ON_MEMORY;

  /* インターフェースのセット */
  stream->stm_if    = &st_memstream_io_if;

  /* メモリセット */
  stream->stm.mem.memory_image = memory_image;
  stream->stm.mem.memory_size  = memory_size;

  /* 内部状態初期化 */
  stream->stm.mem.memory_p = 0;
  stream->bit_buffer  = 0;

  return stream;
}

/* ビットストリームのオープン */
struct BitStream* BitStream_Open(const char* filepath,
    const char* mode, void *work, int32_t work_size)
{
  struct BitStream* stream;
  FILE* tmp_fp;

  /* 共通インスタンス作成ルーチンを通す */
  stream = BitStream_OpenCommon(mode, work, work_size);
  if (stream == NULL) {
    return NULL;
  }

  /* このストリームはファイル上 */
  stream->flags     &= ~BITSTREAM_FLAGS_READWRITE_ON_MEMORY;

  /* インターフェースのセット */
  stream->stm_if    = &st_filestream_io_if;

  /* ファイルオープン */
  tmp_fp = fopen(filepath, mode);
  if (tmp_fp == NULL) {
    return NULL;
  }
  stream->stm.fp = tmp_fp;

  /* 内部状態初期化 */
  fseek(stream->stm.fp, SEEK_SET, 0);
  stream->bit_buffer  = 0;

  return stream;
}

/* オープン関数の共通ルーチン */
static struct BitStream* BitStream_OpenCommon(
    const char* mode, void *work, int32_t work_size)
{
  struct BitStream* stream;
  int8_t            is_malloc_by_work = 0;
  uint8_t*          work_ptr = (uint8_t *)work;

  /* 引数チェック */
  if ((mode == NULL)
      || (work_size < 0)
      || ((work != NULL) && (work_size < BitStream_CalculateWorkSize()))) {
    return NULL;
  }

  /* ワーク渡しか否か？ */
  if ((work == NULL) && (work_size == 0)) {
    is_malloc_by_work = 0;
    work = (uint8_t *)malloc(BitStream_CalculateWorkSize());
    if (work == NULL) {
      return NULL;
    }
  } else {
    is_malloc_by_work = 1;
  }

  /* アラインメント切り上げ */
  work_ptr = (uint8_t *)(((uintptr_t)work + (BITSTREAM_ALIGNMENT-1)) & ~(uintptr_t)(BITSTREAM_ALIGNMENT-1));

  /* 構造体の配置 */
  stream            = (struct BitStream *)work_ptr;
  work_ptr          += sizeof(struct BitStream);
  stream->work_ptr  = work;
  stream->flags     = 0;

  /* モードの1文字目でオープンモードを確定
   * 内部カウンタもモードに合わせて設定 */
  switch (mode[0]) {
    case 'r':
      stream->flags     |= BITSTREAM_FLAGS_FILEOPENMODE_READ;
      stream->bit_count = 0;
      break;
    case 'w':
      stream->flags     &= ~BITSTREAM_FLAGS_FILEOPENMODE_READ;
      stream->bit_count = 8;
      break;
    default:
      return NULL;
  }

  /* メモリアロケート方法を記録 */
  if (is_malloc_by_work != 0) {
    stream->flags |= BITSTREAM_FLAGS_MEMORYALLOC_BYWORK;
  }

  return stream;
}

/* ビットストリームのクローズ */
void BitStream_Close(struct BitStream* stream)
{
  /* 引数チェック */
  if (stream == NULL) {
    return;
  }

  /* バッファのクリア */
  BitStream_Flush(stream);

  if (!(stream->flags & BITSTREAM_FLAGS_READWRITE_ON_MEMORY)) {
    /* ファイルハンドルクローズ */
    fclose(stream->stm.fp);
  }

  /* 必要ならばメモリ解放 */
  if (!(stream->flags & BITSTREAM_FLAGS_MEMORYALLOC_BYWORK)) {
    free(stream->work_ptr);
    stream->work_ptr = NULL;
  }
}

/* シーク(fseek準拠) */
BitStreamApiResult BitStream_Seek(struct BitStream* stream, int32_t offset, int32_t wherefrom)
{
  /* 引数チェック */
  if (stream == NULL) {
    return BITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* 内部バッファをクリア（副作用が起こる） */
  if (BitStream_Flush(stream) != BITSTREAM_APIRESULT_OK) {
    return BITSTREAM_APIRESULT_NG;
  }

  /* シーク実行 */
  if (stream->stm_if->SSeek(
        &(stream->stm), offset, wherefrom) != BITSTREAM_ERROR_OK) {
    return BITSTREAM_APIRESULT_NG;
  }

  return BITSTREAM_APIRESULT_OK;
}

/* 現在位置(ftell)準拠 */
BitStreamApiResult BitStream_Tell(struct BitStream* stream, int32_t* result)
{
  BitStreamError err;

  /* 引数チェック */
  if (stream == NULL || result == NULL) {
    return BITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* ftell実行 */
  err = stream->stm_if->STell(&(stream->stm), result);

  return (err == BITSTREAM_ERROR_OK) ? BITSTREAM_APIRESULT_OK : BITSTREAM_APIRESULT_NG;
}

/* 1bit出力 */
BitStreamApiResult BitStream_PutBit(struct BitStream* stream, uint8_t bit)
{
  /* 引数チェック */
  if (stream == NULL) {
    return BITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* 読み込みモードでは実行不可能 */
  if (stream->flags & BITSTREAM_FLAGS_FILEOPENMODE_READ) {
    return BITSTREAM_APIRESULT_INVALID_MODE;
  }

  /* バイト出力するまでのカウントを減らす */
  stream->bit_count--;

  /* ビット出力バッファに値を格納 */
  if (bit != 0) {
    stream->bit_buffer |= (1 << stream->bit_count);
  }

  /* バッファ出力・更新 */
  if (stream->bit_count == 0) {
    if (stream->stm_if->SPutChar(
          &(stream->stm), stream->bit_buffer) != BITSTREAM_ERROR_OK) {
      return BITSTREAM_APIRESULT_IOERROR;
    }
    stream->bit_buffer = 0;
    stream->bit_count  = 8;
  }

  return BITSTREAM_APIRESULT_OK;
}

/*
 * valの右側（下位）n_bits 出力（最大64bit出力可能）
 * BitStream_PutBits(stream, 3, 6);は次と同じ:
 * BitStream_PutBit(stream, 1); BitStream_PutBit(stream, 1); BitStream_PutBit(stream, 0); 
 */
BitStreamApiResult BitStream_PutBits(struct BitStream* stream, uint16_t n_bits, uint64_t val)
{
  /* 引数チェック */
  if (stream == NULL) {
    return BITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* 読み込みモードでは実行不可能 */
  if (stream->flags & BITSTREAM_FLAGS_FILEOPENMODE_READ) {
    return BITSTREAM_APIRESULT_INVALID_MODE;
  }

  /* 出力可能な最大ビット数を越えている */
  if (n_bits > sizeof(uint64_t) * 8) {
    return BITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* 0ビット出力では何もしない */
  if (n_bits == 0) {
    return BITSTREAM_APIRESULT_OK;
  }

  /* valの上位ビットから順次出力
   * 初回ループでは端数（出力に必要なビット数）分を埋め出力
   * 2回目以降は8bit単位で出力 */
  while (n_bits >= stream->bit_count) {
    n_bits              -= stream->bit_count;
    stream->bit_buffer  |= BITSTREAM_GETLOWERBITS(stream->bit_count, val >> n_bits);
    if (stream->stm_if->SPutChar(
          &(stream->stm), stream->bit_buffer) != BITSTREAM_ERROR_OK) {
      return BITSTREAM_APIRESULT_IOERROR;
    }
    stream->bit_buffer  = 0;
    stream->bit_count   = 8;
  }

  /* 端数ビットの処理:
   * 残った分をバッファの上位ビットにセット */
  stream->bit_count   -= n_bits;
  stream->bit_buffer  |= BITSTREAM_GETLOWERBITS(n_bits, val) << stream->bit_count;

  return BITSTREAM_APIRESULT_OK;
}

/* 1bit取得 */
BitStreamApiResult BitStream_GetBit(struct BitStream* stream, uint8_t* bit)
{
  int32_t ch;

  /* 引数チェック */
  if (stream == NULL || bit == NULL) {
    return BITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* 読み込みモードでない場合は即時リターン */
  if (!(stream->flags & BITSTREAM_FLAGS_FILEOPENMODE_READ)) {
    return BITSTREAM_APIRESULT_INVALID_MODE;
  }

  /* 入力ビットカウントを1減らし、バッファの対象ビットを出力 */
  stream->bit_count--;
  if (stream->bit_count >= 0) {
    (*bit) = (stream->bit_buffer >> stream->bit_count) & 1;
    return BITSTREAM_APIRESULT_OK;
  }

  /* 1バイト読み込みとエラー処理 */
  if (stream->stm_if->SGetChar(
        &(stream->stm), &ch) != BITSTREAM_ERROR_OK) {
    return BITSTREAM_APIRESULT_IOERROR;
  }

  /* カウンタとバッファの更新 */
  stream->bit_count   = 7;
  stream->bit_buffer  = ch;

  /* 取得したバッファの最上位ビットを出力 */
  (*bit) = (stream->bit_buffer >> 7) & 1;
  return BITSTREAM_APIRESULT_OK;
}

/* n_bits 取得（最大64bit）し、その値を右詰めして出力 */
BitStreamApiResult BitStream_GetBits(struct BitStream* stream, uint16_t n_bits, uint64_t *val)
{
  int32_t  ch;
  uint64_t tmp = 0;
  BitStreamError err;

  /* 引数チェック */
  if (stream == NULL || val == NULL) {
    return BITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* 読み込みモードでない場合は即時リターン */
  if (!(stream->flags & BITSTREAM_FLAGS_FILEOPENMODE_READ)) {
    return BITSTREAM_APIRESULT_INVALID_MODE;
  }

  /* 入力可能な最大ビット数を越えている */
  if (n_bits > sizeof(uint64_t) * 8) {
    return BITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* 最上位ビットからデータを埋めていく
   * 初回ループではtmpの上位ビットにセット
   * 2回目以降は8bit単位で入力しtmpにセット */
  while (n_bits > stream->bit_count) {
    n_bits  -= stream->bit_count;
    tmp     |= BITSTREAM_GETLOWERBITS(stream->bit_count, stream->bit_buffer) << n_bits;
    /* 1バイト読み込みとエラー処理 */
    err = stream->stm_if->SGetChar(&(stream->stm), &ch);
    switch (err) {
      case BITSTREAM_APIRESULT_OK:
        break;
      case BITSTREAM_ERROR_EOS:
        /* ファイル終端であればループを抜ける */
        goto END_OF_STREAM;
      case BITSTREAM_ERROR_IOERROR:  /* FALLTRHU */
      default:
        return BITSTREAM_APIRESULT_IOERROR;
    }
    stream->bit_buffer  = ch;
    stream->bit_count   = 8;
  }

END_OF_STREAM:
  /* 端数ビットの処理 
   * 残ったビット分をtmpの最上位ビットにセット */
  stream->bit_count -= n_bits;
  tmp               |= BITSTREAM_GETLOWERBITS(n_bits, stream->bit_buffer >> stream->bit_count);

  /* 正常終了 */
  *val = tmp;
  return BITSTREAM_APIRESULT_OK;
}

/* バッファにたまったビットをクリア */
BitStreamApiResult BitStream_Flush(struct BitStream* stream)
{
  /* 引数チェック */
  if (stream == NULL) {
    return BITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* 既に先頭にあるときは何もしない */
  if (stream->bit_count == 8) {
    return BITSTREAM_APIRESULT_OK;
  }

  /* 読み込み位置を次のバイト先頭に */
  if (stream->flags & BITSTREAM_FLAGS_FILEOPENMODE_READ) {
    /* 残りビット分を空読み */
    uint64_t dummy;
    return BitStream_GetBits(stream, stream->bit_count, &dummy);
  } else {
    /* バッファに余ったビットを強制出力 */
    return BitStream_PutBits(stream, stream->bit_count, 0);
  }
}

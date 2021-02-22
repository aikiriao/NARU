#ifndef BIT_STREAM_H_INCLUDED
#define BIT_STREAM_H_INCLUDED

#include <stdint.h>
#include <stdio.h>

/* BitStream_Seek関数の探索コード */
#define BITSTREAM_SEEK_SET  SEEK_SET
#define BITSTREAM_SEEK_CUR  SEEK_CUR
#define BITSTREAM_SEEK_END  SEEK_END

/* ビットストリーム構造体 */
struct BitStream;

/* API結果型 */
typedef enum BitStreamApiResultTag {
  BITSTREAM_APIRESULT_OK = 0,           /* 成功                       */
  BITSTREAM_APIRESULT_NG,               /* 分類不能なエラー           */
  BITSTREAM_APIRESULT_INVALID_ARGUMENT, /* 不正な引数                 */
  BITSTREAM_APIRESULT_INVALID_MODE,     /* 不正なモード               */
  BITSTREAM_APIRESULT_IOERROR,          /* 分類不能なI/Oエラー        */
  BITSTREAM_APIRESULT_EOS,              /* ストリーム終端に達している */
} BitStreamApiResult;

#ifdef __cplusplus
extern "C" {
#endif 

/* ビットストリーム構造体生成に必要なワークサイズ計算 */
int32_t BitStream_CalculateWorkSize(void);

/* ビットストリームのオープン */
struct BitStream* BitStream_Open(const char* filepath, 
    const char *mode, void *work, int32_t work_size);

/* メモリビットストリームのオープン */
struct BitStream* BitStream_OpenMemory(
    uint8_t* memory_image, uint64_t memory_size,
    const char* mode, void *work, int32_t work_size);

/* ビットストリームのクローズ */
void BitStream_Close(struct BitStream* stream);

/* シーク(fseek準拠)
 * 注意）バッファをクリアするので副作用がある */
BitStreamApiResult BitStream_Seek(struct BitStream* stream, int32_t offset, int32_t wherefrom);

/* 現在位置(ftell)準拠 */
BitStreamApiResult BitStream_Tell(struct BitStream* stream, int32_t* result);

/* 1bit出力 */
BitStreamApiResult BitStream_PutBit(struct BitStream* stream, uint8_t bit);

/*
 * valの右側（下位）n_bits 出力（最大64bit出力可能）
 * BitStream_PutBits(stream, 3, 6);は次と同じ:
 * BitStream_PutBit(stream, 1); BitStream_PutBit(stream, 1); BitStream_PutBit(stream, 0); 
 */
BitStreamApiResult BitStream_PutBits(struct BitStream* stream, uint16_t n_bits, uint64_t val);

/* 1bit取得 */
BitStreamApiResult BitStream_GetBit(struct BitStream* stream, uint8_t* bit);

/* n_bits 取得（最大64bit）し、その値を右詰めして出力 */
BitStreamApiResult BitStream_GetBits(struct BitStream* stream, uint16_t n_bits, uint64_t *val);

/* バッファにたまったビットをクリア */
BitStreamApiResult BitStream_Flush(struct BitStream* stream);

#ifdef __cplusplus
}
#endif 

#endif /* BIT_STREAM_H_INCLUDED */

#ifndef NARU_H_INCLDED
#define NARU_H_INCLDED

#include "naru_stdint.h"

/* フォーマットバージョン */
#define NARU_FORMAT_VERSION   1

/* コーデックバージョン */
#define NARU_CODEC_VERSION    1

/* 処理可能な最大チャンネル数 */
#define NARU_MAX_NUM_CHANNELS 8

/* ヘッダサイズ[byte] */
#define NARU_HEADER_SIZE      31

/* API結果型 */
typedef enum NARUApiResultTag {
  NARU_APIRESULT_OK = 0,              /* 成功                         */
  NARU_APIRESULT_INVALID_ARGUMENT,    /* 無効な引数                   */
  NARU_APIRESULT_INVALID_FORMAT,      /* 不正なフォーマット           */
  NARU_APIRESULT_INSUFFICIENT_BUFFER, /* バッファサイズが足りない     */
  NARU_APIRESULT_INSUFFICIENT_DATA,   /* データが足りない             */
  NARU_APIRESULT_PARAMETER_NOT_SET,   /* パラメータがセットされてない */
  NARU_APIRESULT_NG                   /* 分類不能な失敗               */
} NARUApiResult; 

/* マルチチャンネル処理法 */
typedef enum NARUChannelProcessMethodTag {
  NARU_CH_PROCESS_METHOD_NONE = 0,  /* 何もしない     */
  NARU_CH_PROCESS_METHOD_MS,        /* ステレオMS処理 */
  NARU_CH_PROCESS_METHOD_INVALID    /* 無効値         */
} NARUChannelProcessMethod;

/* ヘッダ情報 */
struct NARUHeaderInfo {
  uint32_t format_version;                    /* フォーマットバージョン         */
  uint32_t codec_version;                     /* エンコーダバージョン           */
  uint16_t num_channels;                      /* チャンネル数                   */
  uint32_t num_samples;                       /* 1チャンネルあたり総サンプル数  */
  uint32_t sampling_rate;                     /* サンプリングレート             */
  uint16_t bits_per_sample;                   /* サンプルあたりビット数         */
  uint32_t num_samples_per_block;             /* ブロックあたりサンプル数       */
  uint16_t filter_order;                      /* フィルタ次数                   */
  uint16_t ar_order;                          /* AR次数                         */
  NARUChannelProcessMethod ch_process_method; /* マルチチャンネル処理法         */
};

#endif /* NARU_H_INCLDED */

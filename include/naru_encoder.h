#ifndef NARU_ENCODER_H_INCLUDED
#define NARU_ENCODER_H_INCLUDED

#include "naru.h"
#include "naru_stdint.h"

/* エンコードパラメータ */
struct NARUEncodeParameter {
  uint16_t num_channels;    /* 入力波形のチャンネル数 */
  uint16_t bits_per_sample; /* 入力波形のサンプルあたりビット数 */
  uint32_t sampling_rate;   /* 入力波形のサンプリングレート */
  uint16_t num_samples_per_block; /* ブロックあたりサンプル数 */
  uint8_t filter_order;    /* フィルタ次数 */
  uint8_t ar_order;        /* AR次数 */
  uint8_t second_filter_order; /* 2段目フィルタ次数 */
  NARUChannelProcessMethod ch_process_method;  /* マルチチャンネル処理法     */
};

/* エンコーダコンフィグ */
struct NARUEncoderConfig {
  uint16_t max_num_channels;          /* エンコード可能な最大チャンネル数 */
  uint16_t max_num_samples_per_block; /* ブロックあたり最大サンプル数 */
  uint8_t max_filter_order;           /* 最大フィルタ次数 */
};

/* エンコーダハンドル */
struct NARUEncoder;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ヘッダエンコード */
NARUApiResult NARUEncoder_EncodeHeader(
  const struct NARUHeader *header, uint8_t *data, uint32_t data_size);

/* エンコーダハンドル作成に必要なワークサイズ計算 */
int32_t NARUEncoder_CalculateWorkSize(const struct NARUEncoderConfig *config);

/* エンコーダハンドル作成 */
struct NARUEncoder *NARUEncoder_Create(const struct NARUEncoderConfig *config, void *work, int32_t work_size);

/* エンコーダハンドルの破棄 */
void NARUEncoder_Destroy(struct NARUEncoder *encoder);

/* エンコードパラメータの設定 */
NARUApiResult NARUEncoder_SetEncodeParameter(
  struct NARUEncoder *encoder, const struct NARUEncodeParameter *parameter);

/* ヘッダ含めファイル全体をエンコード */
NARUApiResult NARUEncoder_EncodeWhole(
  struct NARUEncoder *encoder,
  const int32_t *const *input, uint32_t num_samples,
  uint8_t *data, uint32_t data_size, uint32_t *output_size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NARU_ENCODER_H_INCLUDED */

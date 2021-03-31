#ifndef NARUPLAYER_H_INCLUDED
#define NARUPLAYER_H_INCLUDED

#include <stdint.h>

/* 出力要求コールバック */
typedef void (*NARUSampleRequestCallback)(
        int32_t **buffer, uint32_t num_channels, uint32_t num_samples);

/* プレイヤー初期化コンフィグ */
struct NARUPlayerConfig {
    uint32_t sampling_rate;
    uint16_t num_channels;
    uint16_t bits_per_sample;
    NARUSampleRequestCallback sample_request_callback;
};

#ifdef __cplusplus
extern "C" {
#endif

/* 初期化 この関数内でデバイスドライバの初期化を行い、再生開始 */
void NARUPlayer_Initialize(const struct NARUPlayerConfig *config);

/* 終了 初期化したときのリソースの開放はここで */
void NARUPlayer_Finalize(void);

#ifdef __cplusplus
}
#endif

#endif /* NARUPLAYER_H_INCLUDED */

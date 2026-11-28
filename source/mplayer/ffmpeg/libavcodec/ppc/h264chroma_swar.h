/* WitFLIX: compensacao de movimento de croma H.264 (8 bits), 4 pixels por vez. */
#ifndef WITFLIX_H264CHROMA_SWAR_H
#define WITFLIX_H264CHROMA_SWAR_H

#include <stdint.h>

void witflix_put_h264_chroma_mc8(uint8_t *dst, uint8_t *src, int stride, int h, int x, int y);
void witflix_put_h264_chroma_mc4(uint8_t *dst, uint8_t *src, int stride, int h, int x, int y);
void witflix_avg_h264_chroma_mc8(uint8_t *dst, uint8_t *src, int stride, int h, int x, int y);
void witflix_avg_h264_chroma_mc4(uint8_t *dst, uint8_t *src, int stride, int h, int x, int y);

#endif

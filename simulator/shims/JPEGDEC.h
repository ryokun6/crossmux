#pragma once
// Stub for bitbank2/JPEGDEC. Header shape only — no decoding on host.

#include <cstddef>
#include <cstdint>

#define JPEG_SUCCESS 1
#define JPEG_INVALID_PARAMETER 0
#define JPEG_DECODE_ERROR -1

#define JPEG_PIXEL_GRAYSCALE 0
#define JPEG_PIXEL_RGB565_BIG_ENDIAN 1
#define JPEG_PIXEL_RGB565_LITTLE_ENDIAN 2

#define JPEG_AUTO_ROTATE 1
#define JPEG_SCALE_HALF 1
#define JPEG_SCALE_QUARTER 2
#define JPEG_SCALE_EIGHTH 3

typedef struct JPEGIMAGE {
  int iWidth, iHeight, ucBpp;
} JPEGIMAGE;

typedef struct JPEGDRAW {
  int x, y, iWidth, iHeight;
  uint16_t* pPixels;
} JPEGDRAW;

typedef int32_t (*JPEG_READ_CALLBACK)(class JPEGFILE*, uint8_t*, int32_t);
typedef int32_t (*JPEG_SEEK_CALLBACK)(class JPEGFILE*, int32_t);
typedef int (*JPEG_DRAW_CALLBACK)(JPEGDRAW*);

class JPEGFILE {
 public:
  void* fHandle;
};

class JPEGDEC {
 public:
  int open(const char*, JPEG_READ_CALLBACK, JPEG_READ_CALLBACK, JPEG_SEEK_CALLBACK, JPEG_DRAW_CALLBACK) {
    return JPEG_INVALID_PARAMETER;
  }
  int open(uint8_t*, int, JPEG_DRAW_CALLBACK) { return JPEG_INVALID_PARAMETER; }
  int openRAM(uint8_t*, int, JPEG_DRAW_CALLBACK) { return JPEG_INVALID_PARAMETER; }
  int decode(int, int, int) { return JPEG_DECODE_ERROR; }
  void close() {}
  int getWidth() { return 0; }
  int getHeight() { return 0; }
  int getBpp() { return 0; }
  int getLastError() { return JPEG_DECODE_ERROR; }
  void setPixelType(int) {}
  void setMaxOutputSize(int) {}
};

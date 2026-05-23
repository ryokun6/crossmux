#pragma once
// Stub for bitbank2/PNGdec. Header shape only — no decoding on host. Consumers can
// link, but actual PNG paths will return errors at runtime.

#include <cstddef>
#include <cstdint>

#define PNG_SUCCESS 0
#define PNG_INVALID_PARAMETER -1
#define PNG_DECODE_ERROR -2
#define PNG_TOO_BIG -3
#define PNG_NO_BUFFER -4

#define PNG_PIXEL_GRAYSCALE 0
#define PNG_PIXEL_TRUECOLOR 2
#define PNG_PIXEL_INDEXED 3
#define PNG_PIXEL_GRAY_ALPHA 4
#define PNG_PIXEL_TRUECOLOR_ALPHA 6

typedef struct PNGIMAGE {
  int iWidth, iHeight, ucBpp;
  int iPixelType;
} PNGIMAGE;

typedef struct PNGDRAW {
  int y, iWidth;
  const uint8_t* pPixels;
  int iPitch;
  uint8_t ucBpp;
  int iPixelType;
  uint8_t* pPalette;
} PNGDRAW;

typedef int32_t (*PNG_READ_CALLBACK)(class PNGFILE*, uint8_t*, int32_t);
typedef int32_t (*PNG_SEEK_CALLBACK)(class PNGFILE*, int32_t);
typedef void (*PNG_DRAW_CALLBACK)(PNGDRAW*);

class PNGFILE {
 public:
  void* fHandle;
};

class PNG {
 public:
  int open(const char*, PNG_READ_CALLBACK, PNG_READ_CALLBACK, PNG_SEEK_CALLBACK, PNG_DRAW_CALLBACK) {
    return PNG_DECODE_ERROR;
  }
  int open(uint8_t*, int, PNG_DRAW_CALLBACK) { return PNG_DECODE_ERROR; }
  int openRAM(uint8_t*, int, PNG_DRAW_CALLBACK) { return PNG_DECODE_ERROR; }
  int decode(void*, int) { return PNG_DECODE_ERROR; }
  void close() {}
  int getWidth() { return 0; }
  int getHeight() { return 0; }
  int getBpp() { return 0; }
  int getPixelType() { return 0; }
  int getLastError() { return PNG_DECODE_ERROR; }
};

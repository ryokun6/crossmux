#pragma once

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <stdint.h>

#include <cassert>

// Direct framebuffer writer that eliminates per-pixel overhead from the image
// and glyph rendering hot paths. Pre-computes orientation transform as linear
// coefficients and caches render-mode state so the inner loop is: one multiply,
// one add, one shift, and one AND per pixel — no branches on the rotate path.
//
// Caller is responsible for ensuring (outX, outY) are within screen bounds
// (band clipping still applies). ImageBlock::render() already validates this
// before entering the pixel loop; glyph blits rely on glyphIntersectsStrip plus
// bandColRange.
struct DirectPixelWriter {
  uint8_t* fb = nullptr;
  // Second plane for GRAYSCALE_DUAL strip AA (LSB in fb, MSB here). Null for
  // single-target modes.
  uint8_t* fbMsb = nullptr;
  GfxRenderer::RenderMode mode = GfxRenderer::BW;
  uint16_t displayWidthBytes = 0;  // Runtime framebuffer stride (X4: 100, X3: 99)
  int displayWidthPixels = 0;
  // Active write target: for tiled grayscale, fb is the band scratch, originY is
  // the band's top physical row, and clipRows is the band height. Off-band
  // pixels are dropped. With no strip active these collapse to the full frame
  // (originY 0, clipRows panelHeight) so the clip doubles as a bounds guard.
  int originY = 0;
  int clipRows = 0;
  // BW ink polarity: true = black (clear bit), false = white (set bit). Gray
  // planes always mark with set-bit (state=false) regardless of this flag.
  bool blackInk = true;

  // Orientation is collapsed into a linear transform:
  //   phyX = phyXBase + x * phyXStepX + y * phyXStepY
  //   phyY = phyYBase + x * phyYStepX + y * phyYStepY
  int phyXBase = 0, phyYBase = 0;
  int phyXStepX = 0, phyYStepX = 0;  // per logical-X step
  int phyXStepY = 0, phyYStepY = 0;  // per logical-Y step

  // Row-precomputed: the Y-dependent portion of the physical coords
  int rowPhyXBase = 0, rowPhyYBase = 0;

  void init(const GfxRenderer& renderer) {
    fb = renderer.getWriteTarget();
    fbMsb = renderer.getWriteTargetMsb();
    originY = renderer.getWriteOriginY();
    clipRows = renderer.getWriteRows();
    mode = renderer.getRenderMode();
    displayWidthBytes = renderer.getDisplayWidthBytes();
    displayWidthPixels = renderer.getDisplayWidth();
    blackInk = true;

    const int phyW = renderer.getDisplayWidth();
    const int phyH = renderer.getDisplayHeight();

    switch (renderer.getOrientation()) {
      case GfxRenderer::Portrait:
        // phyX = y, phyY = (phyH-1) - x
        phyXBase = 0;
        phyYBase = phyH - 1;
        phyXStepX = 0;
        phyYStepX = -1;
        phyXStepY = 1;
        phyYStepY = 0;
        break;
      case GfxRenderer::LandscapeClockwise:
        // phyX = (phyW-1) - x, phyY = (phyH-1) - y
        phyXBase = phyW - 1;
        phyYBase = phyH - 1;
        phyXStepX = -1;
        phyYStepX = 0;
        phyXStepY = 0;
        phyYStepY = -1;
        break;
      case GfxRenderer::PortraitInverted:
        // phyX = (phyW-1) - y, phyY = x
        phyXBase = phyW - 1;
        phyYBase = 0;
        phyXStepX = 0;
        phyYStepX = 1;
        phyXStepY = -1;
        phyYStepY = 0;
        break;
      case GfxRenderer::LandscapeCounterClockwise:
        // phyX = x, phyY = y
        phyXBase = 0;
        phyYBase = 0;
        phyXStepX = 1;
        phyYStepX = 0;
        phyXStepY = 0;
        phyYStepY = 1;
        break;
      default:
        // Fallback to LandscapeCounterClockwise (identity transform)
        phyXBase = 0;
        phyYBase = 0;
        phyXStepX = 1;
        phyYStepX = 0;
        phyXStepY = 0;
        phyYStepY = 1;
        break;
    }
  }

  // Call once per row before the column loop.
  // Pre-computes the Y-dependent portion so writePixel() only needs the X part.
  inline void beginRow(int logicalY) {
    rowPhyXBase = phyXBase + logicalY * phyXStepY;
    rowPhyYBase = phyYBase + logicalY * phyYStepY;
  }

  // Intersect [colStart, colEnd) with logical X values whose transformed
  // coordinate falls in [lower, upper]. Transform steps are always -1, 0, or 1.
  inline void clipColRangeForAxis(const int coordBase, const int coordStep, const int xBase, const int lower,
                                  const int upper, int& colStart, int& colEnd) const {
    assert(coordStep == 0 || coordStep == 1 || coordStep == -1);
    if (colStart >= colEnd) return;
    if (coordStep == 0) {
      if (coordBase < lower || coordBase > upper) colEnd = colStart;
      return;
    }

    int xLo, xHi;
    if (coordStep > 0) {
      xLo = lower - coordBase;
      xHi = upper - coordBase;
    } else {
      xLo = coordBase - upper;
      xHi = coordBase - lower;
    }
    const int cs = xLo - xBase;
    const int ce = xHi - xBase + 1;  // exclusive
    if (cs > colStart) colStart = cs;
    if (ce < colEnd) colEnd = ce;
    if (colStart > colEnd) colStart = colEnd;
  }

  // For the current row (set via beginRow), narrow [colStart, colEnd) to pixels
  // inside both the physical panel width and the active strip/full-frame rows.
  // Clipping once per glyph row keeps the inner pixel loop branch-free.
  inline void bandColRange(int xBase, int width, int& colStart, int& colEnd) const {
    colStart = 0;
    colEnd = width;
    clipColRangeForAxis(rowPhyXBase, phyXStepX, xBase, 0, displayWidthPixels - 1, colStart, colEnd);
    clipColRangeForAxis(rowPhyYBase, phyYStepX, xBase, originY, originY + clipRows - 1, colStart, colEnd);
    if (colStart < 0) colStart = 0;
    if (colEnd > width) colEnd = width;
    if (colStart > colEnd) colStart = colEnd;
  }

  inline void writeBit(uint8_t* target, uint16_t byteIndex, uint8_t bitMask, bool state) const {
    if (state) {
      target[byteIndex] &= ~bitMask;  // Clear bit (draw black)
    } else {
      target[byteIndex] |= bitMask;  // Set bit (draw white / gray mark)
    }
  }

  // Write a single 2-bit pixel value to the framebuffer.
  // Must be called after beginRow() for the current row.
  // bandColRange() guarantees physical X bounds; the unsigned Y compare remains
  // as a defensive strip/full-frame row guard.
  inline void writePixel(int logicalX, uint8_t pixelValue) const {
    const int phyX = rowPhyXBase + logicalX * phyXStepX;
    const int phyY = rowPhyYBase + logicalX * phyYStepX;

    // Band-local row. The unsigned compare drops both off-band pixels (strip
    // mode) and any out-of-frame row (full-frame mode) in one branch.
    const int sy = phyY - originY;
    if (static_cast<unsigned>(sy) >= static_cast<unsigned>(clipRows)) return;

    const uint16_t byteIndex = static_cast<uint16_t>(sy * displayWidthBytes + (phyX >> 3));
    const uint8_t bitMask = static_cast<uint8_t>(1u << (7 - (phyX & 7)));

    switch (mode) {
      case GfxRenderer::BW:
        if (pixelValue < 3) {
          writeBit(fb, byteIndex, bitMask, blackInk);
        }
        break;
      case GfxRenderer::GRAYSCALE_MSB:
        if (pixelValue == 1 || pixelValue == 2) {
          writeBit(fb, byteIndex, bitMask, false);
        }
        break;
      case GfxRenderer::GRAYSCALE_LSB:
        if (pixelValue == 1) {
          writeBit(fb, byteIndex, bitMask, false);
        }
        break;
      case GfxRenderer::GRAYSCALE_DUAL:
        // One glyph walk fills both gray planes: dark gray (1) marks LSB+MSB,
        // light gray (2) marks MSB only. Matches the two single-plane passes.
        if (pixelValue == 1) {
          writeBit(fb, byteIndex, bitMask, false);
          if (fbMsb) writeBit(fbMsb, byteIndex, bitMask, false);
        } else if (pixelValue == 2 && fbMsb) {
          writeBit(fbMsb, byteIndex, bitMask, false);
        }
        break;
      default:
        break;
    }
  }
};

// Direct cache writer that eliminates per-pixel overhead from PixelCache::setPixel().
// Pre-computes row pointer so the inner loop is just byte index + bit manipulation.
//
// The cache buffer is a small streaming band (e.g. 16 rows), not the full image,
// so a band-relative row/column that lands outside it would corrupt adjacent
// heap. This writer therefore bounds-checks every access: beginRow() invalidates
// the row when it falls outside the band, and writePixel() drops out-of-range
// columns. This path only runs during the single decode that populates the
// cache, never on the screen render hot path, so the checks are cheap.
struct DirectCacheWriter {
  uint8_t* buffer;
  int bytesPerRow;
  int bandRows;
  int originX;
  uint8_t* rowPtr;  // Pre-computed for current row; nullptr if row is out of band

  void init(uint8_t* cacheBuffer, int cacheBytesPerRow, int cacheBandRows, int cacheOriginX) {
    buffer = cacheBuffer;
    bytesPerRow = cacheBytesPerRow;
    bandRows = cacheBandRows;
    originX = cacheOriginX;
    rowPtr = nullptr;
  }

  // Call once per row before the column loop. Drops rows outside the band.
  inline void beginRow(int screenY, int cacheOriginY) {
    const int localRow = screenY - cacheOriginY;
    rowPtr = (static_cast<unsigned>(localRow) < static_cast<unsigned>(bandRows))
                 ? buffer + (size_t)localRow * bytesPerRow
                 : nullptr;
  }

  // Write a 2-bit pixel value. Drops the write if the row is out of band or the
  // column is out of range.
  inline void writePixel(int screenX, uint8_t value) const {
    if (!rowPtr) return;
    const int localX = screenX - originX;
    const int byteIdx = localX >> 2;  // localX / 4
    if (static_cast<unsigned>(byteIdx) >= static_cast<unsigned>(bytesPerRow)) return;
    const int bitShift = 6 - (localX & 3) * 2;  // MSB first: pixel 0 at bits 6-7
    rowPtr[byteIdx] = (rowPtr[byteIdx] & ~(0x03 << bitShift)) | ((value & 0x03) << bitShift);
  }
};

#include "SloppyEngine.h"

#include <GfxRenderer.h>

#include <cmath>

namespace sloppy {
namespace {

// ── PRNG ────────────────────────────────────────────────────────────────────
class Xorshift32 {
 public:
  explicit Xorshift32(uint32_t seed) : state_(seed ? seed : 0xDEADBEEFu) {}
  uint32_t next() {
    uint32_t x = state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state_ = x;
    return x;
  }
  // Uniform in [lo, hi]
  float nextFloat(float lo, float hi) {
    const float u = next() / static_cast<float>(0xFFFFFFFFu);
    return lo + u * (hi - lo);
  }
  int nextInt(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + static_cast<int>(next() % static_cast<uint32_t>(hi - lo + 1));
  }
  bool chance(float p) { return nextFloat(0.0f, 1.0f) < p; }
  int8_t nextSeedByte() {
    // Uniform in [-127, 127]
    const int v = static_cast<int>(next() & 0xFF) - 128;
    return static_cast<int8_t>(v < -127 ? -127 : (v > 127 ? 127 : v));
  }

 private:
  uint32_t state_;
};

// ── Bezier sampling ─────────────────────────────────────────────────────────
constexpr int kBezierSamples = 20;  // segments per cubic; 21 points

inline float cubic(float t, float p0, float p1, float p2, float p3) {
  const float u = 1.0f - t;
  return u * u * u * p0 + 3.0f * u * u * t * p1 + 3.0f * u * t * t * p2 + t * t * t * p3;
}

// ── Per-digit transform parameters ──────────────────────────────────────────
struct DigitParams {
  float globalScale;
  float xCursor;   // pixel offset (top-left of digit box)
  float yBase;     // pixel offset (top-left of digit box)
  float slantTan;  // shear factor: x_shift = slantTan * (DIGIT_H/2 - y)
  float cs;        // cos(rotRad)
  float sn;        // sin(rotRad)
  float sx, sy;    // per-position aspect
};

inline void transformLocal(float lx, float ly, const DigitParams& p, int& outX, int& outY) {
  const float cx = DIGIT_W * 0.5f;
  const float cy = DIGIT_H * 0.5f;
  // 1) slant shear
  float x = lx + p.slantTan * (cy - ly);
  float y = ly;
  // 2) center, aspect, rotate, decenter
  x -= cx;
  y -= cy;
  x *= p.sx;
  y *= p.sy;
  const float rx = p.cs * x - p.sn * y + cx;
  const float ry = p.sn * x + p.cs * y + cy;
  // 3) global scale + screen position
  outX = static_cast<int>(rx * p.globalScale + p.xCursor + 0.5f);
  outY = static_cast<int>(ry * p.globalScale + p.yBase + 0.5f);
}

// ── Stroke segment ──────────────────────────────────────────────────────────
//
// In BW mode this is just `drawLine` with the requested thickness.  In
// the LSB/MSB passes of a grayscale-AA standby render (where the same draw
// sequence is replayed under storeBwBuffer / setRenderMode), we want each
// stroke to contribute an annular halo to the gray scratch buffer rather
// than a solid line.  Two-step technique:
//
//   1. drawLine(thickness+H, state=false)  → fills the halo+core with
//      bit=1 in the scratch buffer.  In gray-buffer semantics, bit=1 means
//      "this pixel contributes a gray level".
//   2. drawLine(thickness,    state=true)  → clears the core back to bit=0.
//      The remaining bit=1 region is an H/2-pixel annulus on each side of
//      the original stroke.
//
// LSB halo is 1px on each side (dark gray), MSB halo is 2px on each side
// (covers both light and dark gray) — composed by displayGrayBuffer with
// the BW backup into 4 levels:
//
//   core         : BW=black, LSB=0, MSB=0 → black
//   inner ring   : BW=white, LSB=1, MSB=1 → dark gray
//   outer ring   : BW=white, LSB=0, MSB=1 → light gray
//   beyond       : BW=white, LSB=0, MSB=0 → white
//
// The core MUST be cleared in the gray scratch; if BW=black and LSB/MSB=1
// coincide at the same pixel, displayGrayBuffer composes it as gray, not
// black — the font code at GfxRenderer.cpp:191-202 makes the same
// exclusion (black glyph pixels are skipped in the LSB/MSB passes).
inline void drawStrokeSegment(GfxRenderer& renderer, int x0, int y0, int x1, int y1, int strokeWidth) {
  switch (renderer.getRenderMode()) {
    case GfxRenderer::GRAYSCALE_LSB:
      renderer.drawLine(x0, y0, x1, y1, strokeWidth + 2, /*state=*/false);
      renderer.drawLine(x0, y0, x1, y1, strokeWidth, /*state=*/true);
      break;
    case GfxRenderer::GRAYSCALE_MSB:
      renderer.drawLine(x0, y0, x1, y1, strokeWidth + 4, /*state=*/false);
      renderer.drawLine(x0, y0, x1, y1, strokeWidth, /*state=*/true);
      break;
    case GfxRenderer::BW:
    default:
      renderer.drawLine(x0, y0, x1, y1, strokeWidth, /*state=*/true);
      break;
  }
}

// ── Glyph drawing ───────────────────────────────────────────────────────────
void drawGlyph(GfxRenderer& renderer, const Glyph& glyph, const PointSeed* seeds, uint8_t seedCount, float wobble,
               int strokeWidth, const DigitParams& tx) {
  if (glyph.cmdCount == 0) return;

  float curX = 0, curY = 0;
  uint8_t si = 0;  // seed index, walks cmd-by-cmd

  for (uint8_t i = 0; i < glyph.cmdCount; ++i) {
    const Cmd& cmd = glyph.cmds[i];
    if (cmd.kind == CmdKind::Move) {
      const PointSeed& s = (si < seedCount) ? seeds[si++] : PointSeed{0, 0};
      curX = cmd.x0 + (s.dx / 127.0f) * wobble;
      curY = cmd.y0 + (s.dy / 127.0f) * wobble;
    } else {
      const PointSeed& s1 = (si < seedCount) ? seeds[si++] : PointSeed{0, 0};
      const PointSeed& s2 = (si < seedCount) ? seeds[si++] : PointSeed{0, 0};
      const PointSeed& s3 = (si < seedCount) ? seeds[si++] : PointSeed{0, 0};
      const float p0x = curX, p0y = curY;
      const float p1x = cmd.x0 + (s1.dx / 127.0f) * wobble;
      const float p1y = cmd.y0 + (s1.dy / 127.0f) * wobble;
      const float p2x = cmd.x1 + (s2.dx / 127.0f) * wobble;
      const float p2y = cmd.y1 + (s2.dy / 127.0f) * wobble;
      const float p3x = cmd.x2 + (s3.dx / 127.0f) * wobble;
      const float p3y = cmd.y2 + (s3.dy / 127.0f) * wobble;

      int prevSx = 0, prevSy = 0;
      transformLocal(p0x, p0y, tx, prevSx, prevSy);
      for (int k = 1; k <= kBezierSamples; ++k) {
        const float t = static_cast<float>(k) / kBezierSamples;
        const float bx = cubic(t, p0x, p1x, p2x, p3x);
        const float by = cubic(t, p0y, p1y, p2y, p3y);
        int sx, sy;
        transformLocal(bx, by, tx, sx, sy);
        drawStrokeSegment(renderer, prevSx, prevSy, sx, sy, strokeWidth);
        prevSx = sx;
        prevSy = sy;
      }
      curX = p3x;
      curY = p3y;
    }
  }
}

}  // namespace

// ── Public API ──────────────────────────────────────────────────────────────
void rollStyle(uint32_t seed, Style& out) {
  Xorshift32 r(seed);
  out.alphabet = static_cast<AlphabetId>(r.nextInt(0, static_cast<int>(AlphabetId::Count) - 1));
  out.wobble = r.nextFloat(2.0f, 14.0f);
  out.strokeWidth = static_cast<uint8_t>(r.nextInt(2, 7));
  if (r.chance(0.40f)) {
    out.slantDeg = 0.0f;
  } else {
    out.slantDeg = r.nextFloat(6.0f, 18.0f) * (r.chance(0.5f) ? -1.0f : 1.0f);
  }
  out.digitRotateMax = r.nextFloat(0.0f, 12.0f);
  out.digitGap = static_cast<uint8_t>(r.nextInt(10, 30));
  out.oneIsPlain = r.chance(0.5f);
}

void preRollSeeds(uint32_t seed, const Alphabet& alpha, Seeds& out) {
  Xorshift32 r(seed ^ 0xA5A5A5A5u);
  for (int d = 0; d < 10; ++d) {
    const Glyph& g = alpha.glyphs[d];
    uint8_t count = 0;
    for (uint8_t i = 0; i < g.cmdCount && count < MAX_GLYPH_CONTROL_POINTS; ++i) {
      const CmdKind k = g.cmds[i].kind;
      const int n = (k == CmdKind::Move) ? 1 : 3;
      for (int j = 0; j < n && count < MAX_GLYPH_CONTROL_POINTS; ++j) {
        out.glyphSeeds[d][count].dx = r.nextSeedByte();
        out.glyphSeeds[d][count].dy = r.nextSeedByte();
        ++count;
      }
    }
    out.glyphSeedCount[d] = count;
  }
  for (int i = 0; i < kMaxTimeSlots; ++i) {
    out.positions[i].rotJitter = r.nextSeedByte();
    out.positions[i].sxJitter = r.nextSeedByte();
    out.positions[i].syJitter = r.nextSeedByte();
  }
}

void draw(GfxRenderer& renderer, const Style& style, const Seeds& seeds, const char* timeStr, Rect viewport) {
  if (!timeStr || viewport.width <= 0 || viewport.height <= 0) return;
  const Alphabet& alpha = getAlphabet(style.alphabet);

  // Parse timeStr into rows of digits. '\n' separates rows; any other non-digit
  // character is silently ignored. Typical input: "HH\nMM".
  constexpr int kMaxRows = 4;
  constexpr int kMaxDigitsPerRow = 8;
  uint8_t digits[kMaxRows][kMaxDigitsPerRow];
  uint8_t digitsPerRow[kMaxRows] = {0};
  int rowCount = 0;
  int curRow = 0;
  for (const char* p = timeStr; *p; ++p) {
    if (*p == '\n') {
      if (digitsPerRow[curRow] > 0) {
        if (curRow + 1 < kMaxRows) ++curRow;
      }
      continue;
    }
    if (*p >= '0' && *p <= '9') {
      if (digitsPerRow[curRow] < kMaxDigitsPerRow) {
        digits[curRow][digitsPerRow[curRow]++] = static_cast<uint8_t>(*p - '0');
      }
    }
  }
  rowCount = curRow + (digitsPerRow[curRow] > 0 ? 1 : 0);
  if (rowCount == 0) return;

  // Layout in template units.
  // Row width = N*DIGIT_W + (N-1)*digitGap. Block height = R*DIGIT_H + (R-1)*rowGap.
  const float rowGap = DIGIT_H * 0.18f;  // ~29 template units between rows
  float maxRowW = 0;
  for (int r = 0; r < rowCount; ++r) {
    const int n = digitsPerRow[r];
    const float w = (n > 0) ? (n * static_cast<float>(DIGIT_W) + (n - 1) * style.digitGap) : 0.0f;
    if (w > maxRowW) maxRowW = w;
  }
  const float totalH = rowCount * static_cast<float>(DIGIT_H) + (rowCount - 1) * rowGap;
  if (maxRowW <= 0 || totalH <= 0) return;

  // Uniform scale to fit viewport with 8% padding (a bit tighter than single-row to look big).
  const float pad = 0.92f;
  const float sxFit = (viewport.width * pad) / maxRowW;
  const float syFit = (viewport.height * pad) / totalH;
  const float scale = (sxFit < syFit) ? sxFit : syFit;

  const float scaledTotalH = totalH * scale;
  const float originY = viewport.y + (viewport.height - scaledTotalH) * 0.5f;

  const float slantRad = style.slantDeg * 3.14159265f / 180.0f;
  const float slantTan = tanf(slantRad);

  // Render each row, centered horizontally within the viewport.
  int posIdx = 0;
  for (int r = 0; r < rowCount; ++r) {
    const int n = digitsPerRow[r];
    if (n == 0) continue;
    const float rowW = n * static_cast<float>(DIGIT_W) + (n - 1) * style.digitGap;
    const float originX = viewport.x + (viewport.width - rowW * scale) * 0.5f;
    const float yBase = originY + r * (DIGIT_H + rowGap) * scale;

    float cursorTemplate = 0;
    for (int i = 0; i < n; ++i) {
      const int pi = posIdx < kMaxTimeSlots ? posIdx : kMaxTimeSlots - 1;
      const PositionSeed& ps = seeds.positions[pi];
      const float rotRad = (ps.rotJitter / 127.0f) * style.digitRotateMax * 3.14159265f / 180.0f;
      const float aspectSx = 1.0f + (ps.sxJitter / 127.0f) * 0.08f;
      const float aspectSy = 1.0f + (ps.syJitter / 127.0f) * 0.08f;

      DigitParams tx;
      tx.globalScale = scale;
      tx.xCursor = originX + cursorTemplate * scale;
      tx.yBase = yBase;
      tx.slantTan = slantTan;
      tx.cs = cosf(rotRad);
      tx.sn = sinf(rotRad);
      tx.sx = aspectSx;
      tx.sy = aspectSy;

      const uint8_t d = digits[r][i];
      // The plain '1' has only 4 seed slots (M+C), all already pre-rolled into
      // glyphSeeds[1]; PointSeed is relative jitter so the reuse is safe.
      const Glyph& glyph = (d == 1 && style.oneIsPlain) ? alpha.one_plain : alpha.glyphs[d];
      drawGlyph(renderer, glyph, seeds.glyphSeeds[d], seeds.glyphSeedCount[d], style.wobble, style.strokeWidth, tx);

      cursorTemplate += DIGIT_W;
      if (i + 1 < n) cursorTemplate += style.digitGap;
      ++posIdx;
    }
  }
}

}  // namespace sloppy

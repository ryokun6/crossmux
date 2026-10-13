#pragma once

#include <algorithm>

#include "Rect.h"

// Bottom-row touch buttons. Geometry matches the former GameUi helper; the
// games that used to share it are not part of this firmware.
inline Rect touchActionRect(const int screenWidth, const int screenHeight, const int sidePadding, const int gap,
                            const int height, const int index, const int count) {
  if (screenWidth <= 0 || screenHeight <= 0 || height <= 0 || count <= 0 || index < 0 || index >= count) return Rect{};
  constexpr int kMinActionGap = 6;
  const int safePadding = std::max(0, sidePadding);
  const int safeGap = count > 1 ? std::max(kMinActionGap, gap) : 0;
  const int availableWidth = screenWidth - safePadding * 2 - safeGap * (count - 1);
  if (availableWidth < count) return Rect{};
  const int width = availableWidth / count;
  return Rect{safePadding + index * (width + safeGap), screenHeight - safePadding - height, width, height};
}

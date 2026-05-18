#pragma once

#include <cstdint>
#include <string>

#include "../../../util/ButtonNavigator.h"
#include "../../Activity.h"

/**
 * Single-book hub menu — landed on after picking a book from Shelf / Search /
 * Recommend. Pure router; does not call the network itself. Each row launches
 * a dedicated fetch Activity (Notes / Best Marks / Chapters / Reviews /
 * Similar) carrying the bookId.
 */
class WeReadBookActivity final : public Activity {
 public:
  WeReadBookActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookId, std::string title);
  ~WeReadBookActivity() override = default;

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  std::string bookId_;
  std::string title_;
  ButtonNavigator buttonNavigator;
  int selected = 0;

  void onSelect();
};

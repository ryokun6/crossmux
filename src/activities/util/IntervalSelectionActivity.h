#pragma once

#include <I18n.h>

#include "MappedInputManager.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class GfxRenderer;

class IntervalSelectionActivity final : public Activity {
 public:
  explicit IntervalSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const char* activityName,
<<<<<<< HEAD
                                     StrId titleId, int initialValue, int minValue, int maxValue, int smallStep,
                                     int largeStep, StrId valueFormatId = StrId::STR_NONE_OPT,
=======
                                     StrId titleId, StrId stepHintId, int initialValue, int minValue, int maxValue,
                                     int smallStep, int largeStep, StrId valueFormatId = StrId::STR_NONE_OPT,
>>>>>>> upstream/master
                                     bool readerActivity = false, bool ignoreInitialConfirmRelease = false,
                                     StrId maxBoundaryLabelId = StrId::STR_NONE_OPT)
      : Activity(activityName, renderer, mappedInput),
        titleId(titleId),
<<<<<<< HEAD
=======
        stepHintId(stepHintId),
>>>>>>> upstream/master
        valueFormatId(valueFormatId),
        maxBoundaryLabelId(maxBoundaryLabelId),
        value(initialValue),
        minValue(minValue),
        maxValue(maxValue),
        smallStep(smallStep),
        largeStep(largeStep),
        readerActivity(readerActivity),
        ignoreConfirmRelease(ignoreInitialConfirmRelease) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return readerActivity; }

 private:
  StrId titleId;
<<<<<<< HEAD
=======
  StrId stepHintId;
>>>>>>> upstream/master
  StrId valueFormatId;
  StrId maxBoundaryLabelId;
  int value;
  int minValue;
  int maxValue;
  int smallStep;
  int largeStep;
  bool readerActivity;
  bool ignoreConfirmRelease;
<<<<<<< HEAD
  bool draggingBar = false;
=======
>>>>>>> upstream/master
  ButtonNavigator buttonNavigator;

  void adjustValue(int delta);
  int clampedValue(int candidate) const;
<<<<<<< HEAD
  void drawStepHintLine(int y, StrId labelId, int step);
=======
>>>>>>> upstream/master
};

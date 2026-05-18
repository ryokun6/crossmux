#include "WeReadStatsActivity.h"

#include <I18n.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "../../../components/UITheme.h"
#include "../../../fontIds.h"

WeReadStatsActivity::WeReadStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : WeReadFetchActivity("WeReadStats", renderer, mappedInput) {}

const char* WeReadStatsActivity::headerTitle() const { return tr(STR_WEREAD_MENU_STATS); }

void WeReadStatsActivity::buildRequest(JsonDocument& body) {
  // SKILL.md says mode defaults to monthly on the server side too, but being
  // explicit makes the cache invalidation reasoning easier later.
  body["mode"] = "monthly";
}

void WeReadStatsActivity::parseResponse(JsonDocument& resp) {
  stats_ = WeReadModels::StatsSummary{};
  stats_.totalReadTime = resp["totalReadTime"] | 0u;
  stats_.dayAverageReadTime = resp["dayAverageReadTime"] | 0u;
  stats_.readDays = resp["readDays"] | 0;
  if (!resp["compare"].isNull()) {
    stats_.compareValid = true;
    // compare is a fraction; multiply by 1000 to keep one decimal of percent
    // without floats at render time.
    const double c = resp["compare"].as<double>();
    stats_.compareTenths = static_cast<int>(c * 1000.0);
  }
  stats_.preferTimeWord = resp["preferTimeWord"] | "";
  stats_.preferCategoryWord = resp["preferCategoryWord"] | "";

  JsonArrayConst readLongest = resp["readLongest"].as<JsonArrayConst>();
  if (!readLongest.isNull()) {
    stats_.topBooks.reserve(readLongest.size());
    int count = 0;
    for (JsonVariantConst it : readLongest) {
      if (count++ >= 3) break;  // top 3 fits comfortably on one screen
      WeReadModels::StatsTopBook tb;
      JsonVariantConst bk = it["book"];
      if (!bk.isNull()) {
        tb.title = bk["title"] | "";
        tb.author = bk["author"] | "";
      } else {
        // Audio-only entry
        JsonVariantConst al = it["albumInfo"];
        tb.title = al["name"] | "";
        tb.author = al["authorName"] | "";
      }
      tb.readTime = it["readTime"] | 0u;
      if (!tb.title.empty()) stats_.topBooks.push_back(std::move(tb));
    }
  }

  JsonArrayConst readStat = resp["readStat"].as<JsonArrayConst>();
  if (!readStat.isNull()) {
    for (JsonVariantConst s : readStat) {
      const char* stat = s["stat"] | "";
      const char* counts = s["counts"] | "";
      if (stat[0] == '\0') continue;
      std::string line = std::string(stat);
      if (counts[0] != '\0') {
        line += " ";
        line += counts;
      }
      stats_.readStat.push_back(std::move(line));
    }
  }
}

namespace {

void formatHM(uint32_t seconds, char* buf, size_t bufLen) {
  const uint32_t h = seconds / 3600;
  const uint32_t m = (seconds % 3600) / 60;
  if (h > 0) {
    std::snprintf(buf, bufLen, tr(STR_WEREAD_TIME_HM_FMT), static_cast<unsigned>(h), static_cast<unsigned>(m));
  } else {
    std::snprintf(buf, bufLen, tr(STR_WEREAD_TIME_M_FMT), static_cast<unsigned>(m));
  }
}

}  // namespace

void WeReadStatsActivity::renderContent(Rect contentRect) {
  // Hand-rolled vertical layout — drawList expects per-row callbacks, but the
  // stats view is heterogeneous (big numbers, multi-line text, top-N).
  const int sw = renderer.getScreenWidth();
  (void)sw;

  int y = contentRect.y;
  const int lineH = 28;
  char buf[96];

  // 1) Total time + compare
  formatHM(stats_.totalReadTime, buf, sizeof(buf));
  std::string line1 = std::string(tr(STR_WEREAD_MONTH_READ_PREFIX)) + buf;
  renderer.drawText(NOTOSANS_16_FONT_ID, contentRect.x + 20, y + 22, line1.c_str(), true);
  y += lineH + 6;

  // 2) Days + day-average + compare
  char dayAvgBuf[64];
  formatHM(stats_.dayAverageReadTime, dayAvgBuf, sizeof(dayAvgBuf));
  std::snprintf(buf, sizeof(buf), tr(STR_WEREAD_DAYS_AVG_FMT), stats_.readDays, dayAvgBuf);
  renderer.drawText(NOTOSANS_14_FONT_ID, contentRect.x + 20, y + 20, buf, true);
  y += lineH;

  if (stats_.compareValid) {
    const int permille = stats_.compareTenths;
    const char* sign = permille >= 0 ? "↑" : "↓";
    std::snprintf(buf, sizeof(buf), tr(STR_WEREAD_COMPARE_MONTH_FMT), sign, std::abs(permille) / 10,
                  std::abs(permille) % 10);
    renderer.drawText(NOTOSANS_14_FONT_ID, contentRect.x + 20, y + 20, buf, true);
    y += lineH;
  }

  if (!stats_.preferTimeWord.empty()) {
    renderer.drawText(NOTOSANS_14_FONT_ID, contentRect.x + 20, y + 20, stats_.preferTimeWord.c_str(), true);
    y += lineH;
  }
  if (!stats_.preferCategoryWord.empty()) {
    renderer.drawText(NOTOSANS_14_FONT_ID, contentRect.x + 20, y + 20, stats_.preferCategoryWord.c_str(), true);
    y += lineH;
  }

  y += 10;

  // 3) Top books
  if (!stats_.topBooks.empty()) {
    renderer.drawText(NOTOSANS_16_FONT_ID, contentRect.x + 20, y + 22, tr(STR_WEREAD_TOP_BOOKS), true);
    y += lineH + 4;
    int idx = 1;
    for (const auto& tb : stats_.topBooks) {
      formatHM(tb.readTime, buf, sizeof(buf));
      std::string row = std::to_string(idx++) + ". " + tb.title + "  (" + buf + ")";
      renderer.drawText(NOTOSANS_14_FONT_ID, contentRect.x + 30, y + 20, row.c_str(), true);
      y += lineH;
      if (!tb.author.empty()) {
        renderer.drawText(NOTOSANS_12_FONT_ID, contentRect.x + 46, y + 16, tb.author.c_str(), true);
        y += 22;
      }
    }
    y += 6;
  }

  // 4) Read-stat one-liners (读过/读完/笔记 etc.)
  if (!stats_.readStat.empty()) {
    std::string aggregated;
    for (size_t i = 0; i < stats_.readStat.size(); ++i) {
      if (i) aggregated += " · ";
      aggregated += stats_.readStat[i];
    }
    renderer.drawText(NOTOSANS_14_FONT_ID, contentRect.x + 20, y + 20, aggregated.c_str(), true);
  }
}

#pragma once

#include <vector>

class AchievementsStore;
class ReadingStatsStore;
struct BookmarkEntry;

namespace JsonSettingsIO {

bool saveReadingStats(const ReadingStatsStore& store, const char* path);
bool loadReadingStats(ReadingStatsStore& store, const char* json);
bool loadReadingStatsFromFile(ReadingStatsStore& store, const char* path);

bool saveAchievements(const AchievementsStore& store, const char* path);
bool loadAchievements(AchievementsStore& store, const char* json);
bool loadAchievementsFromFile(AchievementsStore& store, const char* path);

bool saveBookmarks(const std::vector<BookmarkEntry>& bookmarks, const char* path);
bool loadBookmarks(std::vector<BookmarkEntry>& bookmarks, const char* json);

}  // namespace JsonSettingsIO

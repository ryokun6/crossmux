#pragma once

#include <vector>

<<<<<<< HEAD
class AchievementsStore;
class OpdsServerStore;
class ReadingStatsStore;
=======
class CrossPointSettings;
class CrossPointState;
class WifiCredentialStore;
class RecentBooksStore;
class OpdsServerStore;
>>>>>>> upstream/master
struct BookmarkEntry;

namespace JsonSettingsIO {

bool saveReadingStats(const ReadingStatsStore& store, const char* path);
bool loadReadingStatsFromFile(ReadingStatsStore& store, const char* path);

bool saveAchievements(const AchievementsStore& store, const char* path);
bool loadAchievements(AchievementsStore& store, const char* json);
bool loadAchievementsFromFile(AchievementsStore& store, const char* path);

bool saveBookmarks(const std::vector<BookmarkEntry>& bookmarks, const char* path);
bool loadBookmarks(std::vector<BookmarkEntry>& bookmarks, const char* json);

// OpdsServerStore still uses the fork JsonSettingsIO path (ryOS seed/migrate).
bool saveOpds(const OpdsServerStore& store, const char* path);
bool loadOpds(OpdsServerStore& store, const char* json, bool* needsResave = nullptr);

// Bookmarks
bool saveBookmarks(const std::vector<BookmarkEntry>& bookmarks, const char* path);
bool loadBookmarks(std::vector<BookmarkEntry>& bookmarks, const char* json);

}  // namespace JsonSettingsIO

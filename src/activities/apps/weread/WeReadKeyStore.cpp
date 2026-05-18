#include "WeReadKeyStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <ObfuscationUtils.h>

namespace WeReadKeyStore {

namespace {

bool g_loaded = false;
std::string g_cached;

constexpr size_t kMinKeyLen = 8;    // "wrk-" + at least 4 chars
constexpr size_t kMaxKeyLen = 256;  // generous upper bound; real keys are ~32

// One-shot import of /.crosspoint/weread_apikey_plain.txt if a tester dropped
// one onto the SD card. Validates, persists obfuscated form, then deletes the
// plaintext file so it never survives a successful boot.
void migrateFromPlainSeed() {
  if (!Storage.exists(kPlainSeedPath)) return;

  String raw = Storage.readFile(kPlainSeedPath);
  std::string key(raw.c_str());
  // Strip trailing whitespace/newlines — `echo "wrk-…" > file` adds a \n.
  while (!key.empty() && (key.back() == '\n' || key.back() == '\r' || key.back() == ' ' || key.back() == '\t')) {
    key.pop_back();
  }

  if (!isWellFormed(key)) {
    LOG_ERR("WEREAD", "Plaintext seed key malformed; removing seed file");
    Storage.remove(kPlainSeedPath);
    return;
  }
  if (!save(key)) {
    LOG_ERR("WEREAD", "Plaintext seed migration: save() failed");
    return;  // keep the seed file so the user can retry next boot
  }
  Storage.remove(kPlainSeedPath);
  LOG_DBG("WEREAD", "Migrated plaintext seed -> obfuscated keystore");
}

void ensureLoadedFromDisk() {
  if (g_loaded) return;
  g_loaded = true;

  // Migration runs before reading the canonical path so a freshly-dropped seed
  // file takes effect on this very access (caller sees the migrated key).
  migrateFromPlainSeed();

  if (!Storage.exists(kPath)) {
    g_cached.clear();
    return;
  }
  String encoded = Storage.readFile(kPath);
  if (encoded.length() == 0) {
    g_cached.clear();
    return;
  }
  bool ok = false;
  std::string decoded = obfuscation::deobfuscateFromBase64(encoded.c_str(), &ok);
  if (!ok || !isWellFormed(decoded)) {
    LOG_ERR("WEREAD", "Stored API key invalid; ignoring");
    g_cached.clear();
    return;
  }
  g_cached = std::move(decoded);
}

}  // namespace

bool isWellFormed(const std::string& key) {
  if (key.length() < kMinKeyLen || key.length() > kMaxKeyLen) return false;
  return key.compare(0, 4, kKeyPrefix) == 0;
}

const std::string& load() {
  ensureLoadedFromDisk();
  return g_cached;
}

bool save(const std::string& key) {
  if (!isWellFormed(key)) {
    LOG_ERR("WEREAD", "save(): key not well-formed");
    return false;
  }
  Storage.ensureDirectoryExists("/.crosspoint");
  String encoded = obfuscation::obfuscateToBase64(key);
  if (!Storage.writeFile(kPath, encoded)) {
    LOG_ERR("WEREAD", "save(): writeFile failed");
    return false;
  }
  g_cached = key;
  g_loaded = true;
  LOG_DBG("WEREAD", "API key saved (len=%u)", static_cast<unsigned>(key.length()));
  return true;
}

bool has() {
  ensureLoadedFromDisk();
  return !g_cached.empty();
}

void clear() {
  g_cached.clear();
  g_loaded = true;
  if (Storage.exists(kPath)) {
    Storage.remove(kPath);
  }
}

}  // namespace WeReadKeyStore

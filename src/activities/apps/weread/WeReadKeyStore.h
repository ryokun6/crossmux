#pragma once

#include <string>

/**
 * Persists the user's WeRead API key (wrk-xxxxxxxx).
 *
 * Stored XOR-obfuscated with the device eFuse MAC at /.crosspoint/weread_apikey.txt
 * via the shared obfuscation:: utilities — same posture as WiFi credentials,
 * not cryptographically secure but ties the key to this specific device.
 *
 * The key is cached in memory after first load; clear() wipes both.
 */
namespace WeReadKeyStore {

constexpr const char* kPath = "/.crosspoint/weread_apikey.txt";

// Dev/test seed path: if a plaintext key (one line, `wrk-…`) is dropped here,
// load() migrates it into the obfuscated kPath on next access and deletes the
// plaintext file. Lets testers avoid keyboard entry without baking keys into
// the firmware binary. Treat as ephemeral — the file disappears after one
// successful migration.
constexpr const char* kPlainSeedPath = "/.crosspoint/weread_apikey_plain.txt";

constexpr const char* kKeyPrefix = "wrk-";

/** Returns the stored key (empty string if none). Reads SD once, then cached. */
const std::string& load();

/** Validates prefix (`wrk-`) and length, persists to SD. Returns false on failure. */
bool save(const std::string& key);

/** True iff a non-empty key is currently persisted. */
bool has();

/** Wipe in-memory cache and delete the SD file. */
void clear();

/** Cheap prefix/length sanity check without touching SD. */
bool isWellFormed(const std::string& key);

}  // namespace WeReadKeyStore

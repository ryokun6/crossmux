#pragma once

#include <ArduinoJson.h>

#include <string>

/**
 * Thin client for the WeChat Read (微信读书) Agent Gateway.
 *
 * All calls go through a single POST endpoint and require a Bearer API key
 * (format: wrk-xxxxxxxx) that the user pastes once via WeReadSetupActivity.
 * skill_version is injected automatically — callers only fill business params.
 *
 * Blocking: post() does synchronous TLS + HTTP + JSON. Activities call it from
 * a dedicated FreeRTOS task to keep the main render loop responsive.
 */
class WeReadClient {
 public:
  enum class Err {
    Ok = 0,
    NoWifi,       // WiFi.status() != WL_CONNECTED before call
    NoApiKey,     // WeReadKeyStore returned empty
    Http,         // TCP / TLS / non-200 HTTP
    Parse,        // body was not valid JSON
    Server,       // errcode != 0 in response
    Upgrade,      // upgrade_info present — skill version mismatch
  };

  /**
   * POST {api_name, skill_version, ...body} to the gateway.
   *
   * `body` is mutated in place (api_name and skill_version are added). On
   * success, `outResp` is filled with the parsed JSON response. Callers should
   * treat `outResp` as read-only afterwards.
   *
   * Caller owns both documents. Pass freshly-constructed JsonDocuments — the
   * client does not clear them.
   */
  // `filter`, when non-null and non-empty, is passed through to ArduinoJson
  // as a DeserializationOption::Filter — the response JsonDocument will
  // contain only fields that the filter sets to `true`. Critical for big
  // shelf/notes responses on a fragmented heap.
  static Err post(const char* apiName, JsonDocument& body, JsonDocument& outResp, int httpTimeoutMs = 10000,
                  const JsonDocument* filter = nullptr);

  /** Last server errcode (when Err::Server is returned). 0 otherwise. */
  static int lastErrCode();

  /** Last upgrade message (when Err::Upgrade is returned). Empty otherwise. */
  static const std::string& lastUpgradeMessage();

  /** Convert Err to a tr()-style hint id for UI display. Caller maps to STR_*. */
  static const char* errorName(Err err);
};

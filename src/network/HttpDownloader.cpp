#include "HttpDownloader.h"

// clang-format off
// WiFi.h/SdFat macros collide with lwip unless esp_wifi is ordered carefully
// (same pattern as OtaUpdater.cpp).
#include <Arduino.h>
#include <Logging.h>
#include <Memory.h>
#include <Stream.h>
#include <WiFi.h>
#include <base64.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_wifi.h>
// clang-format on

#include <cstring>
#include <functional>
#include <string>

namespace {
// RX holds the response headers. 4096 fits real OPDS servers; GitHub's release
// CDN sends more and logs HTTP_HEADER "Buffer length is small", but that's
// non-fatal: the headers we read (Location, Content-Length) come first and
// survive. Smaller keeps contiguous heap free while WiFi and TLS are up.
// TX must fit the full request line + headers for GitHub CDN GETs: signed
// release-assets URLs are ~900+ chars in the path alone, so 1024 truncated
// the request and produced garbage status codes (e.g. 618) on X3.
constexpr int HTTP_RX_BUF = 4096;
constexpr int HTTP_TX_BUF = 3072;

// Drop lingering AP-scan rows before a TLS handshake. Those small allocations
// fragment the internal DRAM arena; X3 OTA/fonts fail when MaxAlloc can't fit
// mbedTLS record buffers even though Free heap still looks healthy.
void reclaimWifiScanHeap() {
  WiFi.scanDelete();
  LOG_DBG("HTTP", "TLS prep Free=%u MaxAlloc=%u", static_cast<unsigned>(ESP.getFreeHeap()),
          static_cast<unsigned>(ESP.getMaxAllocHeap()));
}

// Two hard constraints the mbedTLS sdkconfig (see custom_sdkconfig in
// platformio.ini) imposes on every TLS client in this firmware:
//
// 1. CONFIG_MBEDTLS_SSL_KEEP_PEER_CERTIFICATE=n means mbedtls_ssl_get_peer_cert()
//    always returns NULL. Arduino's ssl_client.cpp verify_ssl_dn() dereferences
//    it with no NULL check, so WiFiClientSecure::verify() / setFingerprint()
//    would crash; they are only absent from the image because nothing calls
//    NetworkClientSecure::verify(). Do not adopt either while that option is off.
// 2. CONFIG_MBEDTLS_DYNAMIC_FREE_CA_CERT nulls conf->ca_chain after each
//    handshake, so a reused mbedtls_ssl_config fails its *second* handshake with
//    no CA to verify against. Safe here only because a fresh esp_http_client is
//    built per hop (see runGet) — this forecloses connection pooling.

// X3 cannot afford CA verification on the GitHub release CDN. Do not "fix" this
// by attaching the bundle unconditionally — that has now been tried and measured
// twice, most recently *after* CONFIG_MBEDTLS_DYNAMIC_BUFFER and
// KEEP_PEER_CERTIFICATE=n were in place, which was the theory for why it had
// become affordable. It has not. Measured on X3 (gh_release_tc), font download:
//
//   hop=0 github.com                          verify OK, 302
//   hop=1 release-assets.githubusercontent.com 6/6 attempts:
//     esp-x509-crt-bundle: PK verify failed with error 0x4290
//     esp-x509-crt-bundle: Certificate matched but signature verification failed
//     esp-tls-mbedtls: mbedtls_ssl_handshake returned -0x3000
//
// "Certificate matched" means the bundle found the right root: this is not a
// trust or CN failure. 0x4290 is MBEDTLS_ERR_RSA_PUBLIC_FAILED (-0x4280) +
// MBEDTLS_ERR_MPI_ALLOC_FAILED (-0x0010) — the MPI temporaries for the RSA
// verify cannot be allocated. Every hop=1 attempt began at Free=46968
// MaxAlloc=32756, and the handshake itself drives the heap to Free=14584 /
// MinFree=1824 / MaxAlloc=9716, which is where the verify has to run.
//
// Raising the MaxAlloc floor in FontDownloadActivity cannot help: a freshly
// booted X3 idles at MaxAlloc=34804, so 32756 is already near this board's
// ceiling and no floor short of an unreachable one would gate it. The heap is
// not fragmented; it is simply too small.
//
// Fonts keep their CRC32 check and OTA keeps the image hash, so this costs
// authenticity on GitHub paths only (see OtaUpdater.cpp for what that means for
// firmware). X4 has more headroom and may be able to verify — that would need a
// per-device policy and its own on-device measurement.
bool shouldAttachCrtBundle(const std::string& url) {
  return url.find("github.com") == std::string::npos && url.find("githubusercontent.com") == std::string::npos;
}

// Modem sleep turns multi‑MB GitHub GETs into ~100 B/s (font download looked
// stuck at 0–1%). Match OTA: disable PS for the transfer, restore after.
struct WifiPsBoost {
  WifiPsBoost() { esp_wifi_set_ps(WIFI_PS_NONE); }
  ~WifiPsBoost() { esp_wifi_set_ps(WIFI_PS_MIN_MODEM); }
};

// Per-socket-op timeout. Some OPDS download endpoints are slow to send headers
// (>15s) and chunked catalogs stall mid-body, so 15s killed them. 60s gives
// slow servers room. When a cancelFlag is provided we use a shorter op timeout
// so Cancel can land between blocked open/read calls (font download).
// esp_http_client's timeout_ms is uint32, so unlike Arduino HTTPClient's uint16
// setTimeout it doesn't silently truncate.
constexpr int HTTP_TIMEOUT_MS = 60000;
// Cancelable downloads: short body-read timeout so Cancel is polled; open /
// fetch_headers need longer — 3s caused CDN hop status=-1 on X3.
constexpr int HTTP_CANCELABLE_OPEN_TIMEOUT_MS = 15000;
constexpr int HTTP_CANCELABLE_READ_TIMEOUT_MS = 3000;
constexpr size_t READ_CHUNK = 2048;

struct Sink {
  std::function<bool(const uint8_t*, size_t)> write;  // returns false to abort the transfer
  HttpDownloader::ProgressCallback progress;
  bool* cancelFlag = nullptr;
  size_t total = 0;
  size_t downloaded = 0;
};

bool isCancelled(const Sink& sink) { return sink.cancelFlag != nullptr && *sink.cancelFlag; }

bool isRedirect(int status) {
  return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}

// GitHub 302 Location is ~900+ chars and arrives while TLS+HTTP buffers are
// live. esp_http_client builds it with realloc(); on X3 that often returns
// null and http_utils_append_string assert(old_str) panics. Hold a contiguous
// cushion after open() and free it on the first response header so realloc has
// a block. Also reserve the std::string we copy Location into.
constexpr size_t HEADER_HEAP_CUSHION = 3072;
constexpr size_t LOCATION_RESERVE = 1280;

struct HeaderCapture {
  std::string* location = nullptr;
  void* cushion = nullptr;
};

void releaseHeaderCushion(HeaderCapture* cap) {
  if (cap == nullptr || cap->cushion == nullptr) return;
  free(cap->cushion);
  cap->cushion = nullptr;
}

// Location is kept in esp_http_client's private `location` field, not the
// generic header map, so get_header("Location") is unreliable. Capture it from
// the ON_HEADER event instead (includes the GitHub CDN JWT query string).
esp_err_t captureLocationHeader(esp_http_client_event_t* evt) {
  auto* cap = static_cast<HeaderCapture*>(evt->user_data);
  if (cap == nullptr) return ESP_OK;
  // Release before any header-value realloc inside esp_http_client.
  if (evt->event_id == HTTP_EVENT_ON_HEADER || evt->event_id == HTTP_EVENT_ON_DATA) {
    releaseHeaderCushion(cap);
  }
  if (evt->event_id == HTTP_EVENT_ON_HEADER && evt->header_key != nullptr && evt->header_value != nullptr &&
      cap->location != nullptr && strcasecmp(evt->header_key, "Location") == 0) {
    cap->location->assign(evt->header_value);
  }
  return ESP_OK;
}

// Streams a GET body through sink.write in READ_CHUNK pieces. Uses the manual
// open/fetch_headers/read path rather than esp_http_client_perform(): perform()
// pushes the whole body through an event callback and reports a chunked body
// that ends early as ESP_ERR_HTTP_INCOMPLETE_DATA, whereas the read loop streams
// large/slow files and surfaces a short read directly.
HttpDownloader::DownloadError runGet(const std::string& url, const std::string& username, const std::string& password,
                                     Sink& sink) {
  WifiPsBoost wifiPsBoost;
  // Fresh client per hop: github.com → release-assets.githubusercontent.com.
  // Reusing one client after close() still left X3's TLS heap tight enough that
  // the CDN hop's cert verify failed (PK 0x4290 / mbedtls -0x3000) while the
  // smaller fonts.json fetch on a cleaner heap succeeded.
  std::string currentUrl = url;
  std::string locationHeader;
  locationHeader.reserve(LOCATION_RESERVE);
  HeaderCapture headerCapture;
  headerCapture.location = &locationHeader;

  const int openTimeoutMs = sink.cancelFlag ? HTTP_CANCELABLE_OPEN_TIMEOUT_MS : HTTP_TIMEOUT_MS;
  const int readTimeoutMs = sink.cancelFlag ? HTTP_CANCELABLE_READ_TIMEOUT_MS : HTTP_TIMEOUT_MS;

  // Claim the read buffer while the heap is still clean: an established TLS
  // session leaves X3 with ~5KB MaxAlloc, so allocating after open() fails.
  auto buf = makeUniqueNoThrow<char[]>(READ_CHUNK);
  if (!buf) {
    LOG_ERR("HTTP", "OOM: %u byte read buffer", (unsigned)READ_CHUNK);
    return HttpDownloader::HTTP_ERROR;
  }

  for (int hop = 0; hop < 6; ++hop) {
    if (isCancelled(sink)) return HttpDownloader::ABORTED;
    reclaimWifiScanHeap();
    locationHeader.clear();
    locationHeader.reserve(LOCATION_RESERVE);
    releaseHeaderCushion(&headerCapture);
    // After github.com → CDN, give the Wi‑Fi/LwIP tasks a beat to free the
    // previous socket's DRAM before the next mbedtls_ssl_setup. X3's PK verify
    // (0x4290 = RSA_VERIFY + MPI_ALLOC_FAILED) is an OOM during cert crypto,
    // not a bad CA — list GETs succeed with ~15KB more Free than file GETs.
    if (hop > 0) {
      delay(150);
      reclaimWifiScanHeap();
    }

    esp_err_t err = ESP_FAIL;
    esp_http_client_handle_t client = nullptr;
    // Fresh client every attempt: close()+reopen on the same handle still kept
    // enough TLS debris that CDN RSA verify OOMed on X3 (MinFree ~1.3KB).
    // Cancelable path uses short timeouts. Cap retries: repeated
    // create_ssl_handle failures fragment the arena (Free drops each try).
    const int maxOpenAttempts = sink.cancelFlag ? 6 : 3;
    // hop0 github.com URLs are short; save TX DRAM for Location realloc.
    const int txBuf = (hop == 0) ? 1536 : HTTP_TX_BUF;
    for (int attempt = 0; attempt < maxOpenAttempts; ++attempt) {
      if (isCancelled(sink)) {
        if (client) esp_http_client_cleanup(client);
        releaseHeaderCushion(&headerCapture);
        return HttpDownloader::ABORTED;
      }
      if (client) {
        esp_http_client_cleanup(client);
        client = nullptr;
        reclaimWifiScanHeap();
        delay(100);
      }

      esp_http_client_config_t config = {};
      config.url = currentUrl.c_str();
      config.buffer_size = HTTP_RX_BUF;
      config.buffer_size_tx = txBuf;
      config.timeout_ms = openTimeoutMs;
      // Verified HTTPS via CA bundle for every host except the GitHub release
      // hosts, which OOM inside RSA verify on X3 (see shouldAttachCrtBundle for
      // the measurements). Plain http needs no cert config. Skipping needs
      // CONFIG_ESP_TLS_INSECURE + CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY: with no
      // CA configured esp-tls then falls back to MBEDTLS_SSL_VERIFY_NONE instead
      // of failing setup.
      if (shouldAttachCrtBundle(currentUrl)) {
        config.crt_bundle_attach = esp_crt_bundle_attach;
      } else {
        LOG_INF("HTTP", "TLS without CA verify (GitHub/X3 heap)");
      }
      config.keep_alive_enable = false;
      config.event_handler = captureLocationHeader;
      config.user_data = &headerCapture;

      client = esp_http_client_init(&config);
      if (!client) {
        LOG_ERR("HTTP", "client init failed");
        releaseHeaderCushion(&headerCapture);
        return HttpDownloader::HTTP_ERROR;
      }

      esp_http_client_set_header(client, "User-Agent", "CrossPoint-ESP32-" CROSSPOINT_VERSION);
      if (!username.empty() && !password.empty()) {
        // Preemptive Basic auth, like the prior addHeader; don't wait for a 401.
        const std::string credentials = username + ":" + password;
        const String header = "Basic " + base64::encode(credentials.c_str());
        esp_http_client_set_header(client, "Authorization", header.c_str());
      }

      LOG_INF("HTTP", "open hop=%d try=%d Free=%u MaxAlloc=%u", hop, attempt, static_cast<unsigned>(ESP.getFreeHeap()),
              static_cast<unsigned>(ESP.getMaxAllocHeap()));
      err = esp_http_client_open(client, 0);
      if (err == ESP_OK) break;
      LOG_ERR("HTTP", "open failed: %s (hop=%d try=%d)", esp_err_to_name(err), hop, attempt);
    }
    if (err != ESP_OK) {
      if (client) esp_http_client_cleanup(client);
      releaseHeaderCushion(&headerCapture);
      return HttpDownloader::HTTP_ERROR;
    }

    // After TLS/HTTP buffers are allocated, park a contiguous block and free it
    // when the first response header arrives so Location realloc can succeed.
    // Skip when MaxAlloc is already tight — the cushion itself caused
    // MaxAlloc~8KB mid-hop and CDN fetch_headers returned status=-1.
    releaseHeaderCushion(&headerCapture);
    if (ESP.getMaxAllocHeap() >= HEADER_HEAP_CUSHION + 24 * 1024) {
      headerCapture.cushion = malloc(HEADER_HEAP_CUSHION);
      if (!headerCapture.cushion) {
        LOG_ERR("HTTP", "header heap cushion alloc failed (need %u)", (unsigned)HEADER_HEAP_CUSHION);
      }
    } else {
      LOG_INF("HTTP", "skip header cushion MaxAlloc=%u", static_cast<unsigned>(ESP.getMaxAllocHeap()));
    }

    const int64_t contentLength = esp_http_client_fetch_headers(client);
    releaseHeaderCushion(&headerCapture);
    const int status = esp_http_client_get_status_code(client);
    LOG_INF("HTTP", "hop=%d status=%d", hop, status);

    if (isRedirect(status)) {
      if (locationHeader.empty()) {
        // Location lives in a private client field; without the ON_HEADER
        // capture we refuse to follow (esp_http_client_get_url drops ?query).
        LOG_ERR("HTTP", "redirect missing Location header (status=%d)", status);
        esp_http_client_cleanup(client);
        return HttpDownloader::HTTP_ERROR;
      }
      LOG_INF("HTTP", "redirect Location len=%u", static_cast<unsigned>(locationHeader.size()));
      currentUrl = std::move(locationHeader);
      esp_http_client_cleanup(client);
      continue;
    }

    if (status != 200) {
      LOG_ERR("HTTP", "unexpected status: %d (hop=%d)", status, hop);
      esp_http_client_cleanup(client);
      // 401/403: missing or wrong Basic auth (OPDS / ryOS Books).
      if (status == 401 || status == 403) return HttpDownloader::AUTH_FAILED;
      return HttpDownloader::HTTP_ERROR;
    }

    // fetch_headers returns 0 for a chunked response (no Content-Length); leave
    // total at 0 so progress stays silent and the size check is skipped.
    sink.total = contentLength > 0 ? static_cast<size_t>(contentLength) : 0;

    // Body reads: shorter timeout when cancelable so Back is polled ~3s.
    if (sink.cancelFlag) {
      esp_http_client_set_timeout_ms(client, readTimeoutMs);
    }

    while (true) {
      if (isCancelled(sink)) {
        esp_http_client_cleanup(client);
        return HttpDownloader::ABORTED;
      }
      const int read = esp_http_client_read(client, buf.get(), READ_CHUNK);
      if (read < 0) {
        // Short op timeout (cancelable downloads) surfaces as read error while
        // the body is still in flight — retry so Cancel can be polled and slow
        // Wi‑Fi can catch up instead of failing the whole transfer.
        if (isCancelled(sink)) {
          esp_http_client_cleanup(client);
          return HttpDownloader::ABORTED;
        }
        const bool bodyPending = (sink.total > 0 && sink.downloaded < sink.total) ||
                                 (sink.total == 0 && !esp_http_client_is_complete_data_received(client));
        if (bodyPending && sink.cancelFlag != nullptr) {
          continue;
        }
        LOG_ERR("HTTP", "read error after %zu bytes", sink.downloaded);
        esp_http_client_cleanup(client);
        return HttpDownloader::HTTP_ERROR;
      }
      if (read == 0) break;  // all data received
      if (!sink.write(reinterpret_cast<const uint8_t*>(buf.get()), read)) {
        esp_http_client_cleanup(client);
        return HttpDownloader::FILE_ERROR;
      }
      sink.downloaded += read;
      if (sink.progress && sink.total > 0) sink.progress(sink.downloaded, sink.total);
    }

    const bool complete = esp_http_client_is_complete_data_received(client);
    esp_http_client_cleanup(client);
    if (!complete) {
      LOG_ERR("HTTP", "incomplete: got %zu of %zu bytes", sink.downloaded, sink.total);
      return HttpDownloader::HTTP_ERROR;
    }
    return HttpDownloader::OK;
  }

  LOG_ERR("HTTP", "too many redirects");
  return HttpDownloader::HTTP_ERROR;
}

// Pull-style Stream wrapper around esp_http_client_read. Backed by a small
// refill buffer so each read()/peek() byte does not cost a syscall.
// esp_http_client_read already strips chunked transfer encoding, so the
// wrapper has no framing logic — read()==0 is the only EOF signal we need.
//
// setTimeout(0) makes Stream::timedRead bail immediately on -1 (our
// "no more data" code), so ArduinoJson stops as soon as it has parsed the
// closing token rather than spending the default 1s waiting for more input.
class EspHttpReadStream final : public Stream {
 public:
  explicit EspHttpReadStream(esp_http_client_handle_t client) : client_(client) { setTimeout(0); }

  int available() override { return static_cast<int>(len_ - pos_); }

  int read() override {
    if (pos_ >= len_ && !refill()) return -1;
    return static_cast<unsigned char>(buf_[pos_++]);
  }

  int peek() override {
    if (pos_ >= len_ && !refill()) return -1;
    return static_cast<unsigned char>(buf_[pos_]);
  }

  size_t write(uint8_t) override { return 0; }
  void flush() override {}

 private:
  static constexpr size_t kBufSize = 1024;

  bool refill() {
    const int n = esp_http_client_read(client_, buf_, static_cast<int>(kBufSize));
    if (n < 0) {
      LOG_ERR("HTTP", "read error mid-body");
      return false;
    }
    if (n == 0) return false;  // server-side EOF
    pos_ = 0;
    len_ = static_cast<size_t>(n);
    return true;
  }

  esp_http_client_handle_t client_;
  char buf_[kBufSize] = {};
  size_t pos_ = 0;
  size_t len_ = 0;
};

bool runPostJson(const std::string& url, const std::string& payload, const std::string& bearerToken,
                 const std::function<bool(Stream&)>& onResponse, int timeoutMs) {
  esp_http_client_config_t config = {};
  config.url = url.c_str();
  config.buffer_size = HTTP_RX_BUF;
  config.buffer_size_tx = HTTP_TX_BUF;
  config.timeout_ms = timeoutMs;
  // Verified HTTPS via the bundled CA roots — same trust posture as runGet.
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.method = HTTP_METHOD_POST;
  config.keep_alive_enable = true;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    LOG_ERR("HTTP", "POST client init failed");
    return false;
  }

  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_header(client, "User-Agent", "CrossPoint-ESP32-" CROSSPOINT_VERSION);
  if (!bearerToken.empty()) {
    const std::string authHeader = "Bearer " + bearerToken;
    esp_http_client_set_header(client, "Authorization", authHeader.c_str());
  }

  // open(content_length) reserves the body length for the request line;
  // write() then streams the payload. POST does not follow redirects here —
  // a 30x on a JSON RPC endpoint is a server misconfiguration we want to
  // surface, not silently re-POST against.
  esp_err_t err = esp_http_client_open(client, static_cast<int>(payload.size()));
  if (err != ESP_OK) {
    LOG_ERR("HTTP", "POST open failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return false;
  }

  if (!payload.empty()) {
    const int written = esp_http_client_write(client, payload.data(), static_cast<int>(payload.size()));
    if (written < 0 || static_cast<size_t>(written) != payload.size()) {
      LOG_ERR("HTTP", "POST write short: %d of %u", written, static_cast<unsigned>(payload.size()));
      esp_http_client_cleanup(client);
      return false;
    }
  }

  const int64_t contentLength = esp_http_client_fetch_headers(client);
  (void)contentLength;  // body length is irrelevant when streaming
  const int status = esp_http_client_get_status_code(client);
  if (status != 200) {
    LOG_ERR("HTTP", "POST unexpected status: %d", status);
    esp_http_client_cleanup(client);
    return false;
  }

  EspHttpReadStream bodyStream(client);
  const bool consumerOk = onResponse(bodyStream);
  esp_http_client_cleanup(client);
  // A successful consumer (e.g., ArduinoJson hitting the closing `}`) is the
  // ground truth for "body received". Don't require draining the stream — a
  // valid JSON document parses without reading any trailing bytes the server
  // might still send (whitespace, server-side keepalive padding). The device
  // GET path checks is_complete_data_received because it counts bytes against
  // Content-Length; here the parser is the framing.
  if (!consumerOk) {
    LOG_ERR("HTTP", "POST consumer reported failure");
    return false;
  }
  return true;
}
}  // namespace

HttpDownloader::DownloadError HttpDownloader::fetchUrl(const std::string& url, Stream& outContent,
                                                       const std::string& username, const std::string& password) {
  LOG_DBG("HTTP", "Fetching: %s", url.c_str());
  Sink sink;
  sink.write = [&outContent](const uint8_t* data, size_t len) { return outContent.write(data, len) == len; };
  return runGet(url, username, password, sink);
}

HttpDownloader::DownloadError HttpDownloader::fetchUrl(const std::string& url, std::string& outContent,
                                                       const std::string& username, const std::string& password) {
  LOG_DBG("HTTP", "Fetching: %s", url.c_str());
  outContent.clear();  // start clean; the sink appends, so don't carry prior content
  Sink sink;
  sink.write = [&outContent](const uint8_t* data, size_t len) {
    outContent.append(reinterpret_cast<const char*>(data), len);
    return true;
  };
  return runGet(url, username, password, sink);
}

HttpDownloader::DownloadError HttpDownloader::fetchUrl(const std::string& url, const DataCallback& onData,
                                                       const std::string& username, const std::string& password) {
  LOG_DBG("HTTP", "Fetching: %s", url.c_str());
  Sink sink;
  sink.write = onData;
  return runGet(url, username, password, sink);
}

bool HttpDownloader::postJson(const std::string& url, const std::string& payload, const std::string& bearerToken,
                              const std::function<bool(Stream&)>& onResponse, int timeoutMs) {
  LOG_DBG("HTTP", "POST: %s (body=%u bytes)", url.c_str(), static_cast<unsigned>(payload.size()));
  reclaimWifiScanHeap();
  return runPostJson(url, payload, bearerToken, onResponse, timeoutMs);
}

HttpDownloader::DownloadError HttpDownloader::downloadToFile(const std::string& url, const std::string& destPath,
                                                             ProgressCallback progress, bool* cancelFlag,
                                                             const std::string& username, const std::string& password) {
  LOG_DBG("HTTP", "Downloading: %s -> %s", url.c_str(), destPath.c_str());

  if (Storage.exists(destPath.c_str())) {
    Storage.remove(destPath.c_str());
  }

  // Defer the SD write open until the first body byte. Opening FAT/exFAT for
  // write before TLS holds sector buffers through github→CDN handshakes; on X3
  // that ~15KB Free gap tipped CDN RSA verify into MPI_ALLOC_FAILED (0x4290)
  // while the smaller fonts.json GET (no prior write handle) succeeded.
  HalFile file;
  bool fileOpen = false;
  Sink sink;
  sink.progress = std::move(progress);
  sink.cancelFlag = cancelFlag;
  sink.write = [&file, &fileOpen, &destPath](const uint8_t* data, size_t len) {
    if (!fileOpen) {
      if (!Storage.openFileForWrite("HTTP", destPath.c_str(), file)) {
        LOG_ERR("HTTP", "Failed to open file for writing");
        return false;
      }
      fileOpen = true;
    }
    return file.write(data, len) == len;
  };

  const DownloadError result = runGet(url, username, password, sink);
  // Close before any remove() on the same path; DESTRUCTOR_CLOSES_FILE would
  // otherwise close only after the remove.
  if (fileOpen) {
    file.close();
  }

  if (result != OK) {
    Storage.remove(destPath.c_str());
    return result;
  }
  if (sink.downloaded == 0) {
    LOG_ERR("HTTP", "no data received");
    Storage.remove(destPath.c_str());
    return HTTP_ERROR;
  }
  LOG_DBG("HTTP", "Downloaded %zu bytes", sink.downloaded);
  return OK;
}

#include <gtest/gtest.h>

<<<<<<< HEAD
#include <cctype>
#include <cstdint>
#include <cstdio>
=======
>>>>>>> upstream/master
#include <cstring>
#include <string>
#include <vector>

#include "lib/JsonParser/ReleaseJsonParser.h"

namespace {

const char* kRealisticPretty = R"({
  "url": "https://api.github.com/repos/crosspoint-reader/crosspoint-reader/releases/12345",
  "assets_url": "https://api.github.com/repos/crosspoint-reader/crosspoint-reader/releases/12345/assets",
  "upload_url": "https://uploads.github.com/repos/crosspoint-reader/crosspoint-reader/releases/12345/assets{?name,label}",
  "html_url": "https://github.com/crosspoint-reader/crosspoint-reader/releases/tag/v2.4.1",
  "id": 12345,
  "author": {
    "login": "releasebot",
    "id": 99887766,
    "node_id": "MDQ6VXNlcjk5ODg3NzY2",
    "avatar_url": "https://avatars.githubusercontent.com/u/99887766?v=4",
    "url": "https://api.github.com/users/releasebot",
    "type": "User",
    "site_admin": false
  },
  "node_id": "RE_kwDOAbCdEf4AADBN",
  "tag_name": "v2.4.1",
  "target_commitish": "main",
  "name": "CrossPoint Reader v2.4.1",
  "draft": false,
  "prerelease": false,
  "created_at": "2026-04-28T10:00:00Z",
  "published_at": "2026-04-28T10:30:00Z",
  "assets": [
    {
      "url": "https://api.github.com/repos/crosspoint-reader/crosspoint-reader/releases/assets/100001",
      "id": 100001,
      "node_id": "RA_kwDOAbCdEf4AAGHR",
      "name": "crosspoint-reader-v2.4.1-source.zip",
      "label": null,
      "uploader": {
        "login": "releasebot",
        "id": 99887766,
        "node_id": "MDQ6VXNlcjk5ODg3NzY2",
        "type": "User"
      },
      "content_type": "application/zip",
      "state": "uploaded",
      "size": 2048576,
      "download_count": 42,
      "created_at": "2026-04-28T10:15:00Z",
      "updated_at": "2026-04-28T10:15:30Z",
      "browser_download_url": "https://github.com/crosspoint-reader/crosspoint-reader/releases/download/v2.4.1/crosspoint-reader-v2.4.1-source.zip"
    },
    {
      "url": "https://api.github.com/repos/crosspoint-reader/crosspoint-reader/releases/assets/100002",
      "id": 100002,
      "node_id": "RA_kwDOAbCdEf4AAGHS",
      "name": "firmware.bin",
      "label": "ESP32-C3 Firmware",
      "uploader": {
        "login": "releasebot",
        "id": 99887766,
        "node_id": "MDQ6VXNlcjk5ODg3NzY2",
        "type": "User"
      },
      "content_type": "application/octet-stream",
      "state": "uploaded",
      "size": 1572864,
      "download_count": 187,
      "created_at": "2026-04-28T10:16:00Z",
      "updated_at": "2026-04-28T10:16:45Z",
      "browser_download_url": "https://github.com/crosspoint-reader/crosspoint-reader/releases/download/v2.4.1/firmware.bin"
    },
    {
      "url": "https://api.github.com/repos/crosspoint-reader/crosspoint-reader/releases/assets/100003",
      "id": 100003,
      "node_id": "RA_kwDOAbCdEf4AAGHR",
      "name": "checksums.sha256",
      "label": null,
      "uploader": {
        "login": "releasebot",
        "id": 99887766,
        "node_id": "MDQ6VXNlcjk5ODg3NzY2",
        "type": "User"
      },
      "content_type": "text/plain",
      "state": "uploaded",
      "size": 192,
      "download_count": 15,
      "created_at": "2026-04-28T10:17:00Z",
      "updated_at": "2026-04-28T10:17:10Z",
      "browser_download_url": "https://github.com/crosspoint-reader/crosspoint-reader/releases/download/v2.4.1/checksums.sha256"
    }
  ],
  "tarball_url": "https://api.github.com/repos/crosspoint-reader/crosspoint-reader/tarball/v2.4.1",
  "zipball_url": "https://api.github.com/repos/crosspoint-reader/crosspoint-reader/zipball/v2.4.1",
  "body": "## What's Changed\n\n* Fixed orientation crash (#123)\n* Improved EPUB rendering performance\n* Added Serbian translation\n\n**Full Changelog**: https://github.com/crosspoint-reader/crosspoint-reader/compare/v2.4.0...v2.4.1",
  "reactions": {
    "url": "https://api.github.com/repos/crosspoint-reader/crosspoint-reader/releases/12345/reactions",
    "total_count": 5,
    "+1": 3,
    "-1": 0,
    "laugh": 1,
    "hooray": 1,
    "confused": 0,
    "heart": 0,
    "rocket": 0,
    "eyes": 0
  }
})";

const char* kRealisticMinified =
    R"({"url":"https://api.github.com/repos/crosspoint-reader/crosspoint-reader/releases/12345","assets_url":"https://api.github.com/repos/crosspoint-reader/crosspoint-reader/releases/12345/assets","id":12345,"author":{"login":"releasebot","id":99887766,"node_id":"MDQ6VXNlcjk5ODg3NzY2","type":"User","site_admin":false},"tag_name":"v2.4.1","target_commitish":"main","name":"CrossPoint Reader v2.4.1","draft":false,"prerelease":false,"assets":[{"url":"https://api.github.com/repos/crosspoint-reader/crosspoint-reader/releases/assets/100001","id":100001,"name":"crosspoint-reader-v2.4.1-source.zip","uploader":{"login":"releasebot","id":99887766},"content_type":"application/zip","state":"uploaded","size":2048576,"download_count":42,"browser_download_url":"https://github.com/crosspoint-reader/crosspoint-reader/releases/download/v2.4.1/crosspoint-reader-v2.4.1-source.zip"},{"url":"https://api.github.com/repos/crosspoint-reader/crosspoint-reader/releases/assets/100002","id":100002,"name":"firmware.bin","uploader":{"login":"releasebot","id":99887766},"content_type":"application/octet-stream","state":"uploaded","size":1572864,"download_count":187,"browser_download_url":"https://github.com/crosspoint-reader/crosspoint-reader/releases/download/v2.4.1/firmware.bin"},{"url":"https://api.github.com/repos/crosspoint-reader/crosspoint-reader/releases/assets/100003","id":100003,"name":"checksums.sha256","uploader":{"login":"releasebot","id":99887766},"content_type":"text/plain","state":"uploaded","size":192,"download_count":15,"browser_download_url":"https://github.com/crosspoint-reader/crosspoint-reader/releases/download/v2.4.1/checksums.sha256"}],"body":"## What's Changed\n\n* Fixed orientation crash","reactions":{"url":"https://api.github.com/repos/crosspoint-reader/crosspoint-reader/releases/12345/reactions","total_count":5,"+1":3}})";

void feedChunked(ReleaseJsonParser& p, const char* json, size_t chunkSize) {
  size_t len = strlen(json);
  for (size_t off = 0; off < len; off += chunkSize) {
    size_t n = len - off < chunkSize ? len - off : chunkSize;
    p.feed(json + off, n);
  }
}

}  // namespace

TEST(ReleaseJsonParser, RealisticPrettyPrinted) {
  ReleaseJsonParser p;
  p.feed(kRealisticPretty, strlen(kRealisticPretty));

  EXPECT_TRUE(p.foundTag());
  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getTagName(), "v2.4.1");
  EXPECT_STREQ(p.getFirmwareUrl(),
               "https://github.com/crosspoint-reader/crosspoint-reader/releases/download/v2.4.1/firmware.bin");
  EXPECT_EQ(p.getFirmwareSize(), 1572864u);
}

TEST(ReleaseJsonParser, RealisticMinified) {
  ReleaseJsonParser p;
  p.feed(kRealisticMinified, strlen(kRealisticMinified));

  EXPECT_TRUE(p.foundTag());
  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getTagName(), "v2.4.1");
  EXPECT_STREQ(p.getFirmwareUrl(),
               "https://github.com/crosspoint-reader/crosspoint-reader/releases/download/v2.4.1/firmware.bin");
  EXPECT_EQ(p.getFirmwareSize(), 1572864u);
}

TEST(ReleaseJsonParser, PrettyAndMinifiedAgree) {
  ReleaseJsonParser pretty;
  pretty.feed(kRealisticPretty, strlen(kRealisticPretty));

  ReleaseJsonParser minified;
  minified.feed(kRealisticMinified, strlen(kRealisticMinified));

  ASSERT_TRUE(pretty.foundTag());
  ASSERT_TRUE(pretty.foundFirmware());
  ASSERT_TRUE(minified.foundTag());
  ASSERT_TRUE(minified.foundFirmware());

  EXPECT_STREQ(pretty.getTagName(), minified.getTagName());
  EXPECT_STREQ(pretty.getFirmwareUrl(), minified.getFirmwareUrl());
  EXPECT_EQ(pretty.getFirmwareSize(), minified.getFirmwareSize());
}

TEST(ReleaseJsonParser, FirmwareNotFirstAsset) {
  const char* json = R"({
      "tag_name": "v1.0.0",
      "assets": [
        {"name": "source.tar.gz", "browser_download_url": "https://example.com/src.tar.gz", "size": 500000},
        {"name": "docs.pdf", "browser_download_url": "https://example.com/docs.pdf", "size": 120000},
        {"name": "firmware.bin", "browser_download_url": "https://example.com/firmware.bin", "size": 987654},
        {"name": "checksums.txt", "browser_download_url": "https://example.com/checksums.txt", "size": 256}
      ]
    })";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundTag());
  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getTagName(), "v1.0.0");
  EXPECT_STREQ(p.getFirmwareUrl(), "https://example.com/firmware.bin");
  EXPECT_EQ(p.getFirmwareSize(), 987654u);
}

<<<<<<< HEAD
TEST(ReleaseJsonParser, SelectsRequestedTraditionalChineseFirmwareAsset) {
  const char* json = R"({
      "tag_name": "1.4.14",
      "assets": [
        {"name": "firmware.bin", "browser_download_url": "https://example.com/firmware.bin", "size": 5000000},
        {"name": "firmware-tc.bin", "browser_download_url": "https://example.com/firmware-tc.bin", "size": 6000000},
        {"name": "firmware-sc.bin", "browser_download_url": "https://example.com/firmware-sc.bin", "size": 6100000}
      ]
    })";

  ReleaseJsonParser p("firmware-tc.bin");
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundTag());
  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getTagName(), "1.4.14");
  EXPECT_STREQ(p.getFirmwareUrl(), "https://example.com/firmware-tc.bin");
  EXPECT_EQ(p.getFirmwareSize(), 6000000u);
}

TEST(ReleaseJsonParser, SelectsRequestedSimplifiedChineseFirmwareAsset) {
  const char* json = R"({
      "tag_name": "1.4.14",
      "assets": [
        {"name": "firmware.bin", "browser_download_url": "https://example.com/firmware.bin", "size": 5000000},
        {"name": "firmware-tc.bin", "browser_download_url": "https://example.com/firmware-tc.bin", "size": 6000000},
        {"name": "firmware-sc.bin", "browser_download_url": "https://example.com/firmware-sc.bin", "size": 6100000}
      ]
    })";

  ReleaseJsonParser p("firmware-sc.bin");
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundTag());
  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getTagName(), "1.4.14");
  EXPECT_STREQ(p.getFirmwareUrl(), "https://example.com/firmware-sc.bin");
  EXPECT_EQ(p.getFirmwareSize(), 6100000u);
}

TEST(ReleaseJsonParser, SelectsRequestedJapaneseFirmwareAsset) {
  const char* json = R"({
      "tag_name": "1.4.14",
      "assets": [
        {"name": "firmware.bin", "browser_download_url": "https://example.com/firmware.bin", "size": 5000000},
        {"name": "firmware-ja.bin", "browser_download_url": "https://example.com/firmware-ja.bin", "size": 5800000},
        {"name": "firmware-ko.bin", "browser_download_url": "https://example.com/firmware-ko.bin", "size": 5900000}
      ]
    })";

  ReleaseJsonParser p("firmware-ja.bin");
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundTag());
  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getFirmwareUrl(), "https://example.com/firmware-ja.bin");
  EXPECT_EQ(p.getFirmwareSize(), 5800000u);
}

TEST(ReleaseJsonParser, SelectsRequestedKoreanFirmwareAsset) {
  const char* json = R"({
      "tag_name": "1.4.14",
      "assets": [
        {"name": "firmware.bin", "browser_download_url": "https://example.com/firmware.bin", "size": 5000000},
        {"name": "firmware-ja.bin", "browser_download_url": "https://example.com/firmware-ja.bin", "size": 5800000},
        {"name": "firmware-ko.bin", "browser_download_url": "https://example.com/firmware-ko.bin", "size": 5900000}
      ]
    })";

  ReleaseJsonParser p("firmware-ko.bin");
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundTag());
  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getFirmwareUrl(), "https://example.com/firmware-ko.bin");
  EXPECT_EQ(p.getFirmwareSize(), 5900000u);
}

=======
>>>>>>> upstream/master
TEST(ReleaseJsonParser, FieldOrderUrlBeforeName) {
  const char* json = R"({
      "tag_name": "v3.0",
      "assets": [{
        "browser_download_url": "https://example.com/fw.bin",
        "name": "firmware.bin",
        "size": 2222
      }]
    })";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getFirmwareUrl(), "https://example.com/fw.bin");
  EXPECT_EQ(p.getFirmwareSize(), 2222u);
}

TEST(ReleaseJsonParser, FieldOrderSizeBeforeUrl) {
  const char* json = R"({
      "tag_name": "v3.1",
      "assets": [{
        "size": 3333,
        "browser_download_url": "https://example.com/fw2.bin",
        "name": "firmware.bin"
      }]
    })";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getFirmwareUrl(), "https://example.com/fw2.bin");
  EXPECT_EQ(p.getFirmwareSize(), 3333u);
}

TEST(ReleaseJsonParser, FieldOrderNameFirst) {
  const char* json = R"({
      "tag_name": "v3.2",
      "assets": [{
        "name": "firmware.bin",
        "size": 4444,
        "browser_download_url": "https://example.com/fw3.bin"
      }]
    })";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getFirmwareUrl(), "https://example.com/fw3.bin");
  EXPECT_EQ(p.getFirmwareSize(), 4444u);
}

TEST(ReleaseJsonParser, AssetsBeforeTagName) {
  // tag_name appears after assets in the JSON
  const char* json = R"({
      "name": "Release",
      "assets": [{
        "name": "firmware.bin",
        "browser_download_url": "https://example.com/fw.bin",
        "size": 5555
      }],
      "tag_name": "v4.0"
    })";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundTag());
  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getTagName(), "v4.0");
  EXPECT_STREQ(p.getFirmwareUrl(), "https://example.com/fw.bin");
  EXPECT_EQ(p.getFirmwareSize(), 5555u);
}

TEST(ReleaseJsonParser, ChunkedFeedingSmallChunks) {
  ReleaseJsonParser p;
  feedChunked(p, kRealisticPretty, 64);

  EXPECT_TRUE(p.foundTag());
  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getTagName(), "v2.4.1");
  EXPECT_STREQ(p.getFirmwareUrl(),
               "https://github.com/crosspoint-reader/crosspoint-reader/releases/download/v2.4.1/firmware.bin");
  EXPECT_EQ(p.getFirmwareSize(), 1572864u);
}

TEST(ReleaseJsonParser, ChunkedFeedingByteByByte) {
  ReleaseJsonParser p;
  feedChunked(p, kRealisticMinified, 1);

  EXPECT_TRUE(p.foundTag());
  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getTagName(), "v2.4.1");
  EXPECT_EQ(p.getFirmwareSize(), 1572864u);
}

TEST(ReleaseJsonParser, ChunkedFeedingVariousChunkSizes) {
  for (size_t chunkSize : {3u, 7u, 13u, 31u, 97u, 128u, 256u, 512u, 1024u}) {
    ReleaseJsonParser p;
    feedChunked(p, kRealisticPretty, chunkSize);

    EXPECT_TRUE(p.foundTag()) << "chunkSize=" << chunkSize;
    EXPECT_TRUE(p.foundFirmware()) << "chunkSize=" << chunkSize;
    EXPECT_STREQ(p.getTagName(), "v2.4.1") << "chunkSize=" << chunkSize;
    EXPECT_STREQ(p.getFirmwareUrl(),
                 "https://github.com/crosspoint-reader/crosspoint-reader/releases/download/v2.4.1/firmware.bin")
        << "chunkSize=" << chunkSize;
    EXPECT_EQ(p.getFirmwareSize(), 1572864u) << "chunkSize=" << chunkSize;
  }
}

TEST(ReleaseJsonParser, MissingTagName) {
  const char* json = R"({
      "name": "Some Release",
      "draft": false,
      "assets": [{
        "name": "firmware.bin",
        "browser_download_url": "https://example.com/fw.bin",
        "size": 1000
      }]
    })";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_FALSE(p.foundTag());
  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getTagName(), "");
}

TEST(ReleaseJsonParser, MissingFirmwareBinAsset) {
  const char* json = R"({
      "tag_name": "v1.0.0",
      "assets": [
        {"name": "source.zip", "browser_download_url": "https://example.com/src.zip", "size": 1000},
        {"name": "docs.tar.gz", "browser_download_url": "https://example.com/docs.tar.gz", "size": 2000}
      ]
    })";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundTag());
  EXPECT_FALSE(p.foundFirmware());
  EXPECT_STREQ(p.getTagName(), "v1.0.0");
  EXPECT_STREQ(p.getFirmwareUrl(), "");
  EXPECT_EQ(p.getFirmwareSize(), 0u);
}

TEST(ReleaseJsonParser, EmptyAssetsArray) {
  const char* json = R"({"tag_name": "v1.0.0", "assets": []})";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundTag());
  EXPECT_FALSE(p.foundFirmware());
}

TEST(ReleaseJsonParser, NoAssetsKey) {
  const char* json = R"({"tag_name": "v1.0.0", "name": "Release"})";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundTag());
  EXPECT_FALSE(p.foundFirmware());
}

TEST(ReleaseJsonParser, TruncatedBeforeTagValue) {
  const char* json = R"({"tag_name": )";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_FALSE(p.foundTag());
  EXPECT_FALSE(p.foundFirmware());
}

TEST(ReleaseJsonParser, TruncatedInsideTagValue) {
  const char* json = R"({"tag_name": "v2.4)";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_FALSE(p.foundTag());
}

TEST(ReleaseJsonParser, TruncatedInsideAssetsArray) {
  const char* json = R"({"tag_name": "v2.4.1", "assets": [{"name": "firm)";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundTag());
  EXPECT_STREQ(p.getTagName(), "v2.4.1");
  EXPECT_FALSE(p.foundFirmware());
}

TEST(ReleaseJsonParser, TruncatedAfterFirmwareName) {
  // Found the name but connection dropped before URL/size
  const char* json = R"({"tag_name":"v1.0","assets":[{"name":"firmware.bin","browser_dow)";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundTag());
  EXPECT_FALSE(p.foundFirmware());
}

TEST(ReleaseJsonParser, TruncatedRealisticJson) {
  std::string full(kRealisticPretty);
  for (size_t cutPoint : {10u, 50u, 100u, 200u, 500u, 1000u, 1500u, 2000u}) {
    if (cutPoint >= full.size()) continue;

    ReleaseJsonParser p;
    p.feed(full.c_str(), cutPoint);
    (void)p.foundTag();
    (void)p.foundFirmware();
  }
  SUCCEED();
}

TEST(ReleaseJsonParser, NestedObjectsInAsset) {
  // Asset with deeply nested "uploader" object -- should not confuse depth tracking
  const char* json = R"({
      "tag_name": "v5.0",
      "assets": [{
        "name": "firmware.bin",
        "uploader": {
          "login": "bot",
          "id": 42,
          "permissions": {"admin": false, "push": true, "pull": true}
        },
        "browser_download_url": "https://example.com/fw5.bin",
        "size": 8888
      }]
    })";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getFirmwareUrl(), "https://example.com/fw5.bin");
  EXPECT_EQ(p.getFirmwareSize(), 8888u);
}

TEST(ReleaseJsonParser, NestedObjectsAtTopLevel) {
  // Multiple nested objects at the top level before/after tag_name and assets
  const char* json = R"({
      "author": {"login": "dev", "id": 1, "nested": {"deep": true}},
      "tag_name": "v6.0",
      "reactions": {"url": "https://reactions", "total_count": 0, "+1": 0},
      "assets": [{"name": "firmware.bin", "browser_download_url": "https://fw6", "size": 1111}],
      "mentions_count": 3
    })";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundTag());
  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getTagName(), "v6.0");
  EXPECT_EQ(p.getFirmwareSize(), 1111u);
}

TEST(ReleaseJsonParser, ArraysAtTopLevel) {
  // A non-assets array at the top level should not interfere
  const char* json = R"({
      "tag_name": "v7.0",
      "labels": ["release", "stable"],
      "assets": [{"name": "firmware.bin", "browser_download_url": "https://fw7", "size": 7070}]
    })";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundTag());
  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getTagName(), "v7.0");
  EXPECT_EQ(p.getFirmwareSize(), 7070u);
}

TEST(ReleaseJsonParser, ResetAndReuse) {
  ReleaseJsonParser p;

  const char* json1 =
      R"({"tag_name":"v1.0","assets":[{"name":"firmware.bin","browser_download_url":"https://a","size":1}]})";
  p.feed(json1, strlen(json1));
  EXPECT_TRUE(p.foundTag());
  EXPECT_STREQ(p.getTagName(), "v1.0");
  EXPECT_STREQ(p.getFirmwareUrl(), "https://a");
  EXPECT_EQ(p.getFirmwareSize(), 1u);

  p.reset();

  const char* json2 =
      R"({"tag_name":"v2.0","assets":[{"name":"firmware.bin","browser_download_url":"https://b","size":2}]})";
  p.feed(json2, strlen(json2));
  EXPECT_TRUE(p.foundTag());
  EXPECT_STREQ(p.getTagName(), "v2.0");
  EXPECT_STREQ(p.getFirmwareUrl(), "https://b");
  EXPECT_EQ(p.getFirmwareSize(), 2u);
}

TEST(ReleaseJsonParser, ResetClearsState) {
  ReleaseJsonParser p;

  const char* json =
      R"({"tag_name":"v1.0","assets":[{"name":"firmware.bin","browser_download_url":"https://a","size":100}]})";
  p.feed(json, strlen(json));
  EXPECT_TRUE(p.foundTag());
  EXPECT_TRUE(p.foundFirmware());

  p.reset();

  EXPECT_FALSE(p.foundTag());
  EXPECT_FALSE(p.foundFirmware());
  EXPECT_STREQ(p.getTagName(), "");
  EXPECT_STREQ(p.getFirmwareUrl(), "");
  EXPECT_EQ(p.getFirmwareSize(), 0u);
}

TEST(ReleaseJsonParser, PartialAssetNameMatch) {
  // "firmware.bin.bak" should NOT match "firmware.bin"
  const char* json = R"({
      "tag_name": "v1.0",
      "assets": [
        {"name": "firmware.bin.bak", "browser_download_url": "https://bak", "size": 100},
        {"name": "firmware.bin.old", "browser_download_url": "https://old", "size": 200}
      ]
    })";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundTag());
  EXPECT_FALSE(p.foundFirmware());
}

TEST(ReleaseJsonParser, FirmwareBinExactMatch) {
  // Only exact "firmware.bin" matches, not similar names
  const char* json = R"({
      "tag_name": "v1.0",
      "assets": [
        {"name": "FIRMWARE.BIN", "browser_download_url": "https://upper", "size": 100},
        {"name": "firmware.bin", "browser_download_url": "https://exact", "size": 200},
        {"name": "firmware.bin2", "browser_download_url": "https://suffix", "size": 300}
      ]
    })";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getFirmwareUrl(), "https://exact");
  EXPECT_EQ(p.getFirmwareSize(), 200u);
}

TEST(ReleaseJsonParser, LargeSize) {
  // 16MB firmware (maximum flash size)
  const char* json =
      R"({"tag_name":"v1.0","assets":[{"name":"firmware.bin","browser_download_url":"https://fw","size":16777216}]})";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_EQ(p.getFirmwareSize(), 16777216u);
}

TEST(ReleaseJsonParser, SizeZero) {
  const char* json =
      R"({"tag_name":"v1.0","assets":[{"name":"firmware.bin","browser_download_url":"https://fw","size":0}]})";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundFirmware());
  EXPECT_EQ(p.getFirmwareSize(), 0u);
}

TEST(ReleaseJsonParser, MinimalValidJson) {
  const char* json = R"({"tag_name":"v0","assets":[{"name":"firmware.bin","browser_download_url":"u","size":1}]})";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  EXPECT_TRUE(p.foundTag());
  EXPECT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getTagName(), "v0");
  EXPECT_STREQ(p.getFirmwareUrl(), "u");
  EXPECT_EQ(p.getFirmwareSize(), 1u);
}

<<<<<<< HEAD
// --- Asset digest ("digest": "sha256:<64 hex>") --------------------------------

namespace {

// Live shape from the release API: asset firmware-tc.bin, size 6309648.
const char* kDigestHex = "d0f18ad5d5a1841489ff56ef7a6cefe94a568b48d3cd019bb611c8617f5e342e";

std::string digestJson(const std::string& digestValue) {
  std::string json = R"({"tag_name":"1.5.0","assets":[{"name":"firmware-tc.bin",)";
  if (!digestValue.empty()) json += "\"digest\":\"" + digestValue + "\",";
  json += R"("browser_download_url":"https://example.com/firmware-tc.bin","size":6309648}]})";
  return json;
}

std::string toHex(const uint8_t* bytes, size_t len) {
  std::string out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; i++) {
    char pair[3];
    snprintf(pair, sizeof(pair), "%02x", bytes[i]);
    out += pair;
  }
  return out;
}

}  // namespace

TEST(ReleaseJsonParser, DigestPresentAndWellFormed) {
  const std::string json = digestJson(std::string("sha256:") + kDigestHex);

  ReleaseJsonParser p("firmware-tc.bin");
  p.feed(json.c_str(), json.size());

  ASSERT_TRUE(p.foundFirmware());
  EXPECT_EQ(p.getFirmwareSize(), 6309648u);
  ASSERT_TRUE(p.foundFirmwareDigest());
  EXPECT_EQ(toHex(p.getFirmwareDigest(), ReleaseJsonParser::DIGEST_SIZE), kDigestHex);
}

TEST(ReleaseJsonParser, DigestUppercaseHexAccepted) {
  std::string upper(kDigestHex);
  for (char& c : upper) c = static_cast<char>(toupper(c));
  const std::string json = digestJson("sha256:" + upper);

  ReleaseJsonParser p("firmware-tc.bin");
  p.feed(json.c_str(), json.size());

  ASSERT_TRUE(p.foundFirmwareDigest());
  EXPECT_EQ(toHex(p.getFirmwareDigest(), ReleaseJsonParser::DIGEST_SIZE), kDigestHex);
}

TEST(ReleaseJsonParser, DigestAbsent) {
  const std::string json = digestJson("");

  ReleaseJsonParser p("firmware-tc.bin");
  p.feed(json.c_str(), json.size());

  ASSERT_TRUE(p.foundFirmware());
  EXPECT_FALSE(p.foundFirmwareDigest());
}

TEST(ReleaseJsonParser, DigestNullValue) {
  const char* json = R"({"tag_name":"1.5.0","assets":[{"name":"firmware.bin","digest":null,)"
                     R"("browser_download_url":"https://example.com/fw.bin","size":100}]})";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  ASSERT_TRUE(p.foundFirmware());
  EXPECT_FALSE(p.foundFirmwareDigest());
}

TEST(ReleaseJsonParser, DigestWrongPrefix) {
  const std::string json = digestJson(std::string("sha512:") + kDigestHex);

  ReleaseJsonParser p("firmware-tc.bin");
  p.feed(json.c_str(), json.size());

  ASSERT_TRUE(p.foundFirmware());
  EXPECT_FALSE(p.foundFirmwareDigest());
}

TEST(ReleaseJsonParser, DigestMissingPrefix) {
  const std::string json = digestJson(kDigestHex);

  ReleaseJsonParser p("firmware-tc.bin");
  p.feed(json.c_str(), json.size());

  EXPECT_FALSE(p.foundFirmwareDigest());
}

TEST(ReleaseJsonParser, DigestWrongLength) {
  std::string tooShort(kDigestHex);
  tooShort.pop_back();
  const std::string shortJson = digestJson("sha256:" + tooShort);

  ReleaseJsonParser shortParser("firmware-tc.bin");
  shortParser.feed(shortJson.c_str(), shortJson.size());
  EXPECT_FALSE(shortParser.foundFirmwareDigest());

  const std::string longJson = digestJson(std::string("sha256:") + kDigestHex + "ab");
  ReleaseJsonParser longParser("firmware-tc.bin");
  longParser.feed(longJson.c_str(), longJson.size());
  EXPECT_FALSE(longParser.foundFirmwareDigest());
}

TEST(ReleaseJsonParser, DigestNonHexCharacters) {
  std::string bad(kDigestHex);
  bad[10] = 'z';
  const std::string json = digestJson("sha256:" + bad);

  ReleaseJsonParser p("firmware-tc.bin");
  p.feed(json.c_str(), json.size());

  EXPECT_FALSE(p.foundFirmwareDigest());
}

TEST(ReleaseJsonParser, DigestOfNonMatchingAssetIsNotAdopted) {
  // The source zip has a digest, the firmware asset does not: the parser must not
  // carry the previous asset's digest over.
  const char* json = R"({"tag_name":"1.5.0","assets":[)"
                     R"({"name":"source.zip","digest":"sha256:)"
                     "d0f18ad5d5a1841489ff56ef7a6cefe94a568b48d3cd019bb611c8617f5e342e"
                     R"(","browser_download_url":"https://example.com/src.zip","size":10},)"
                     R"({"name":"firmware.bin","browser_download_url":"https://example.com/fw.bin","size":20}]})";

  ReleaseJsonParser p;
  p.feed(json, strlen(json));

  ASSERT_TRUE(p.foundFirmware());
  EXPECT_STREQ(p.getFirmwareUrl(), "https://example.com/fw.bin");
  EXPECT_FALSE(p.foundFirmwareDigest());
}

TEST(ReleaseJsonParser, DigestChunkedAtEveryBoundary) {
  const std::string json = digestJson(std::string("sha256:") + kDigestHex);

  for (size_t split = 0; split <= json.size(); ++split) {
    ReleaseJsonParser p("firmware-tc.bin");
    if (split > 0) p.feed(json.c_str(), split);
    if (split < json.size()) p.feed(json.c_str() + split, json.size() - split);

    ASSERT_TRUE(p.foundFirmwareDigest()) << "split=" << split;
    EXPECT_EQ(toHex(p.getFirmwareDigest(), ReleaseJsonParser::DIGEST_SIZE), kDigestHex) << "split=" << split;
  }
}

TEST(ReleaseJsonParser, ResetClearsDigest) {
  const std::string json = digestJson(std::string("sha256:") + kDigestHex);

  ReleaseJsonParser p("firmware-tc.bin");
  p.feed(json.c_str(), json.size());
  ASSERT_TRUE(p.foundFirmwareDigest());

  p.reset();
  EXPECT_FALSE(p.foundFirmwareDigest());
}

// --- Per-asset callback (multi-SKU asset selection) ----------------------------

namespace {

// What the OTA updater does with the callback: keep the handful of assets it
// recognizes, each with the digest and size published for *that* asset.
struct CollectedAsset {
  std::string name;
  std::string url;
  size_t size = 0;
  std::string digestHex;  // empty when the asset carried no usable digest
};

struct Collector {
  std::vector<CollectedAsset> assets;

  static void trampoline(void* ctx, const ReleaseJsonParser::Asset& asset) {
    auto* self = static_cast<Collector*>(ctx);
    CollectedAsset out;
    out.name = asset.name;
    out.url = asset.url;
    out.size = asset.size;
    if (asset.digestValid) out.digestHex = toHex(asset.digest, ReleaseJsonParser::DIGEST_SIZE);
    self->assets.push_back(out);
  }

  const CollectedAsset* find(const std::string& name) const {
    for (const CollectedAsset& a : assets) {
      if (a.name == name) return &a;
    }
    return nullptr;
  }
};

// One asset per SKU, each with a distinct digest, so a parser that leaks state
// between assets pairs the wrong digest with the wrong file and the test fails.
const char* kSkuNames[] = {"firmware.bin", "firmware-tc.bin", "firmware-sc.bin", "firmware-ja.bin", "firmware-ko.bin"};
const size_t kSkuSizes[] = {5398800u, 6258736u, 6084688u, 5430448u, 5685520u};
const char* kSkuDigests[] = {"510f1bd8ca6c178dde70d152dc141e8d8faefa57ed70833ad901d01971ae1e25",
                             "aec8f31709161edbac359081615112ef9665529b44f3bd6feb0eebb907358dd7",
                             "ae3f26a71df7511d540d3d670529c4df1e7a4d85e0cf0e77917fb41eaf11a2f7",
                             "450c32283674c4e45db13dc91464b5b13e71e4901e08e92e2fbcd0f30ecd854e",
                             "22da10b98837d260291625ae3e69cadfc4c7275c29f425b62150d01f2b612a2f"};

// Shaped like a real release: bootloader/partitions around the five firmware
// images, digests on every asset.
std::string allSkuReleaseJson() {
  std::string json = R"({"tag_name":"1.4.17","assets":[)";
  json +=
      R"({"name":"bootloader.bin","digest":"sha256:6e6b0d386095f0783e9f0513ea29ea2ce413790e7f7153a06a2b04e2b5f59fb6",)"
      R"("browser_download_url":"https://example.com/bootloader.bin","size":18672},)";
  for (size_t i = 0; i < 5; i++) {
    json += std::string(R"({"name":")") + kSkuNames[i] + R"(","digest":"sha256:)" + kSkuDigests[i] +
            R"(","browser_download_url":"https://example.com/)" + kSkuNames[i] + R"(","size":)" +
            std::to_string(kSkuSizes[i]) + "},";
  }
  json += R"({"name":"partitions.bin","browser_download_url":"https://example.com/partitions.bin","size":3072}]})";
  return json;
}

}  // namespace

TEST(ReleaseJsonParser, AssetCallbackReportsEveryAssetWithItsOwnDigest) {
  const std::string json = allSkuReleaseJson();

  Collector collector;
  ReleaseJsonParser p("firmware-tc.bin");
  p.setAssetCallback(&Collector::trampoline, &collector);
  p.feed(json.c_str(), json.size());

  ASSERT_EQ(collector.assets.size(), 7u);
  for (size_t i = 0; i < 5; i++) {
    const CollectedAsset* asset = collector.find(kSkuNames[i]);
    ASSERT_NE(asset, nullptr) << kSkuNames[i];
    EXPECT_EQ(asset->url, std::string("https://example.com/") + kSkuNames[i]);
    EXPECT_EQ(asset->size, kSkuSizes[i]);
    EXPECT_EQ(asset->digestHex, kSkuDigests[i]) << kSkuNames[i];
  }
  // The asset with no digest must not inherit the previous one's.
  const CollectedAsset* partitions = collector.find("partitions.bin");
  ASSERT_NE(partitions, nullptr);
  EXPECT_EQ(partitions->digestHex, "");
}

TEST(ReleaseJsonParser, AssetCallbackAgreesWithLatchedFirmware) {
  const std::string json = allSkuReleaseJson();

  Collector collector;
  ReleaseJsonParser p("firmware-ko.bin");
  p.setAssetCallback(&Collector::trampoline, &collector);
  p.feed(json.c_str(), json.size());

  ASSERT_TRUE(p.foundFirmware());
  ASSERT_TRUE(p.foundFirmwareDigest());
  const CollectedAsset* asset = collector.find("firmware-ko.bin");
  ASSERT_NE(asset, nullptr);
  EXPECT_EQ(asset->url, std::string(p.getFirmwareUrl()));
  EXPECT_EQ(asset->size, p.getFirmwareSize());
  EXPECT_EQ(asset->digestHex, toHex(p.getFirmwareDigest(), ReleaseJsonParser::DIGEST_SIZE));
}

TEST(ReleaseJsonParser, AssetCallbackDigestNotCarriedToNextAsset) {
  // firmware.bin has a digest, firmware-tc.bin does not: picking TC must come out
  // unverifiable rather than silently adopting the international image's digest.
  const std::string json = std::string(R"({"tag_name":"1.4.17","assets":[)") + R"({"name":"firmware.bin","digest":")" +
                           "sha256:" + kSkuDigests[0] +
                           R"(","browser_download_url":"https://example.com/firmware.bin","size":10},)" +
                           R"({"name":"firmware-tc.bin","browser_download_url":"https://example.com/tc","size":20}]})";

  Collector collector;
  ReleaseJsonParser p;
  p.setAssetCallback(&Collector::trampoline, &collector);
  p.feed(json.c_str(), json.size());

  ASSERT_EQ(collector.assets.size(), 2u);
  EXPECT_EQ(collector.assets[0].digestHex, kSkuDigests[0]);
  EXPECT_EQ(collector.assets[1].digestHex, "");
}

TEST(ReleaseJsonParser, AssetCallbackChunkedAtEveryBoundary) {
  const std::string json = allSkuReleaseJson();

  for (size_t split = 0; split <= json.size(); ++split) {
    Collector collector;
    ReleaseJsonParser p("firmware-tc.bin");
    p.setAssetCallback(&Collector::trampoline, &collector);
    if (split > 0) p.feed(json.c_str(), split);
    if (split < json.size()) p.feed(json.c_str() + split, json.size() - split);

    ASSERT_EQ(collector.assets.size(), 7u) << "split=" << split;
    for (size_t i = 0; i < 5; i++) {
      const CollectedAsset* asset = collector.find(kSkuNames[i]);
      ASSERT_NE(asset, nullptr) << "split=" << split << " " << kSkuNames[i];
      EXPECT_EQ(asset->size, kSkuSizes[i]) << "split=" << split << " " << kSkuNames[i];
      EXPECT_EQ(asset->digestHex, kSkuDigests[i]) << "split=" << split << " " << kSkuNames[i];
    }
  }
}

TEST(ReleaseJsonParser, AssetCallbackSkipsAssetsWithNoName) {
  // A response cut mid-asset, and an asset object that carries no name at all:
  // neither is something a consumer can act on, so neither is announced.
  const char* json = R"({"tag_name":"1.4.17","assets":[)"
                     R"({"size":1,"browser_download_url":"https://example.com/anon"},)"
                     R"({"name":"firmware.bin","browser_download_url":"https://example.com/fw","size":2}]})";

  Collector collector;
  ReleaseJsonParser p;
  p.setAssetCallback(&Collector::trampoline, &collector);
  p.feed(json, strlen(json));

  ASSERT_EQ(collector.assets.size(), 1u);
  EXPECT_EQ(collector.assets[0].name, "firmware.bin");
}

TEST(ReleaseJsonParser, AssetCallbackSurvivesReset) {
  const std::string json = allSkuReleaseJson();

  Collector collector;
  ReleaseJsonParser p;
  p.setAssetCallback(&Collector::trampoline, &collector);
  p.feed(json.c_str(), json.size());
  ASSERT_EQ(collector.assets.size(), 7u);

  // reset() drops parsed content, not the owner's wiring.
  p.reset();
  collector.assets.clear();
  p.feed(json.c_str(), json.size());
  EXPECT_EQ(collector.assets.size(), 7u);
}

=======
>>>>>>> upstream/master
TEST(ReleaseJsonParser, ChunkedRealisticEveryBoundary) {
  // Two-chunk split at every byte boundary on a compact JSON
  const char* json =
      R"({"tag_name":"v2.0","assets":[{"name":"firmware.bin","browser_download_url":"https://example.com/fw","size":9999}]})";
  size_t len = strlen(json);

  for (size_t split = 0; split <= len; ++split) {
    ReleaseJsonParser p;
    if (split > 0) p.feed(json, split);
    if (split < len) p.feed(json + split, len - split);

    EXPECT_TRUE(p.foundTag()) << "split=" << split;
    EXPECT_TRUE(p.foundFirmware()) << "split=" << split;
    EXPECT_STREQ(p.getTagName(), "v2.0") << "split=" << split;
    EXPECT_STREQ(p.getFirmwareUrl(), "https://example.com/fw") << "split=" << split;
    EXPECT_EQ(p.getFirmwareSize(), 9999u) << "split=" << split;
  }
}

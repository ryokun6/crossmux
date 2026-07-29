# Upstream Integration Plan (`0x1abin/crossmux` → ryOS)

> Living plan for absorbing CrossMux upstream `main` (currently **1.5.1**) into
> this fork while preserving ryOS identity: multi-SKU CJK (TC/SC/JA/KO),
> vertical EPUB, no games, GenSen/SD-font stack, and thin `AGENTS.md`.
>
> Snapshot when written: fork tip `1.4.20` / `13d6b967`; upstream tip `1.5.1` /
> `6fbc1cdc`; merge-base `767678a8` (2026-07-06); **~38** upstream-only commits;
> `git merge-tree` reports **~146** content/modify-delete conflicts. Re-measure
> before each sync wave.

Companion policy for docs-only conflicts:
[`upstream-merge-policy.md`](upstream-merge-policy.md).

## 1. Goals and non-goals

### Goals

1. Ship upstream reading-core and reliability gains (CrossPoint develop syncs
   through 1.5.0, reader hot-path, SD-font flash cache, clock/UTC work).
2. Replace the WeRead **Companion** (`wrk-` API key) with upstream’s **offline
   QR-login EPUB reader** (1.5.0 `#49` + 1.5.1 follow-ups).
3. Keep every ryOS differentiator listed in §2.
4. Leave the tree buildable and testable after each phase (no multi-week
   “broken main”).

### Non-goals

- Importing games / Avatar / Sokoban (out of [`SCOPE.md`](../../SCOPE.md)).
- Replacing GenSen / multi-SKU CJK / vertical layout with upstream’s single-CN
  trim. (SDK: this sync **did** adopt `freeink-sdk` + wolfSSL for WeRead; see §10.)
- Reintroducing monolithic `.skills/SKILL.md` / `CLAUDE.md`.
- Collapsing TC/SC/JA/KO into upstream’s single `gh_release_cn`.

## 2. What we keep (fork-owned)

These are **ours** on conflict. Upstream edits that touch the same files must
be re-applied *around* them, never by taking upstream wholesale.

| Area | Keep | Why |
|---|---|---|
| SKU matrix | `gh_release_{tc,sc,ja,ko}` (+ RC), `ENABLE_CJK_VERSION`, OpenCC `tw2sp` SC UI | Upstream only has `gh_release_cn` |
| Vertical CJK EPUB | `WritingMode`, `VerticalPunctuation`, kinsoku, 標點擠壓, TC/JA vertical defaults | Upstream has no vertical layout |
| Section cache versions | Fork `SECTION_FILE_VERSION` 55 / 81–84 (CJK axes + TextBlock arena) | Upstream uses different CN counters; layouts differ |
| Builtin CJK fonts | GenSen TW / SC-keyed / JP / KO pipelines, coverage tiers, LTO strategy | Upstream trimmed to Noto SC 3500 pool |
| Apps surface | Stats + WeRead (CN SKUs) + Standby (+ OPDS entry) — **no games** | Matches SCOPE; upstream still ships games |
| Branding / sync UX | ryOS Books, Cloud Sync, account banners | Fork product identity |
| Docs map | Thin `AGENTS.md` + `docs/engineering/*` | See merge policy |
| SDK submodule | `freeink-sdk` (adopted with 1.5.1 sync) | Keep fork HAL wrappers; do not call SDK UI paths directly |
| Locale READMEs | `README.{zh,ja,ko}.md` | Upstream uses `README.zh-CN.md` |
| TLS for general HTTPS | Fork’s `esp_http_client` + mbedTLS CA verify (fonts/OTA/OPDS) | WeRead uses a **separate** wolfSSL path; keep both roles distinct |
| Heap guards | `ScopedSdFontUnload`, CJK reader heap hardening, X3 mbedTLS buffer work | Fork-specific stability |

## 3. What we take (upstream-owned)

Take these as the new baseline for their subsystems (adapt to SKUs/gates).

| Area | Upstream source | Notes |
|---|---|---|
| WeRead offline client | `#49` + `#54`–`#56`, `#59`–`#61` | **Replace** companion tree; see §5 |
| WeRead unit tests | `test/weread/*` | Port with simulator shims |
| SD-font inactive-OTA flash cache | `#57` | Merge into our `SdCardFont*` carefully |
| Reader render hot-path | `#50` | Rebase onto our `ParsedText`/vertical code |
| Built-in font scan elision / prompt limits | `#52` | Keep our “System” family UX |
| Per-app visibility | `#58` | Adopt bitmask **only for kept apps** |
| Popup confirm release-through | `#53` + related input-edge docs | Take |
| Oversized URL wrap | `#51` | Take |
| UTC clock / X3 RTC / Date & Time | `#40`,`#42`,`#45`,`#46` | Merge with our existing DateTime activities (§4) |
| CN SD-font completion guidance | `#44` | Port copy/UX; retarget TC/SC (and JA/KO as useful) |
| Trimmed CN coverage *ideas* | `#43` | **Do not** take their headers; optionally adopt “prompt to SD font” UX |
| CrossPoint develop syncs in 1.5.0 | `#34`,`#36`,`#41`,`#47`,`#48` | Dictionary/bookmarks/ruby/lazy indexing etc. — verify what we already have before re-porting |
| WeRead transport docs / security notice | `chinese-build.md` WeRead sections + release notes | Route into our chinese-build doc; disclose CA non-verify |

## 4. Overlap matrix (keep / take / merge)

Legend: **K** = keep ours · **T** = take upstream · **M** = manual merge · **D** = delete ours / drop upstream

| Subsystem | Decision | Resolution rule |
|---|---|---|
| `src/activities/apps/weread/**` | **T→adapt** | Delete companion activities/key page; bring upstream files; gate with `ENABLE_CHINESE_VERSION` (SC primary; decide TC later) |
| `WeReadKeyPage.html`, `WeReadKeyStore.*` | **D** | Removed with companion; update `docs/webserver-endpoints.md` |
| `AppsMenuActivity` | **M** | Keep no-games list; add upstream `AppId` + `hiddenAppsMask` for Stats/WeRead/Standby/(OPDS if counted); WeRead first on CN |
| Games / Avatar / GameUi | **D** (upstream) | Never merge those directories |
| `lib/Epub/**` (ParsedText, Section, Page, parsers) | **M** | Start from **ours** (vertical/kinsoku/punct); cherry-pick upstream perf + CrossPoint layout fixes hunk-by-hunk |
| Section cache version | **K** then bump | After merging layout semantics, bump *our* CJK version integers; document in `file-formats.md` |
| `lib/EpdFont/builtinFonts/notosans_cjk_*.h` | **K** | Ignore upstream font header conflicts |
| Font build scripts / charsets | **K** + selective **T** | Keep GenSen/OpenCC pipelines; borrow upstream “complete SD font” guidance strings/flow |
| `SdCardFont*` + flash cache | **M** | Take `#57` API (`allowFlashCache`, inactive slot) onto our SD font manager; keep `ScopedSdFontUnload` |
| `HalClock` / `TimeUtils` / DateTime settings | **M** | Prefer upstream UTC-everywhere + X3 RTC restore; manual edit lives in `DateTimeSettingsActivity` (orphan `DateTimeEditActivity` removed) |
| `platformio.ini` | **M** | Keep SKU envs; wolfSSL + `patch_wolfssl.py` for WeRead; keep mbedTLS custom_sdkconfig for fonts/OTA/OPDS |
| `.gitmodules` / SDK | **T→adapt** | Adopted `freeink-sdk` with 1.5.1; keep HAL-only app code; isolate WeRead wolfSSL from mbedTLS HTTPS |
| `SCOPE.md` / READMEs | **K** | Keep ryOS scope; fold upstream WeRead security notice into USER_GUIDE / chinese-build |
| `AGENTS.md` / `.skills` | **K** + route | Per `upstream-merge-policy.md` |
| I18n YAML (english/chinese + others) | **M** | Union keys: keep ryOS strings; import WeRead/clock/visibility strings; regenerate; SC still via OpenCC |
| `reading-stats` / `standby` | **M** | Small diffs — take upstream consolidations that don’t break Chinese calendar face |
| KOReader sync / OPDS / web server | **M** | Keep ryOS naming; take upstream progress-sync branch that calls WeRead when shelf match (`#59`) |
| CI / release workflows | **M** | Keep multi-SKU artifacts (`firmware-{tc,sc,ja,ko}.bin`); don’t collapse to `firmware-cn.bin` only |
| Simulator | **M** | Keep multi-SKU builds; import WeRead native-sim shims from upstream; WASM WeRead stays excluded |

## 5. WeRead replacement (largest product change)

### Current fork (Companion)

- Browser/`wrk-` API key → `i.weread.qq.com` agent gateway
- Many activities (shelf, notes, reviews, search, stats, sync-all, …)
- Docs: `/weread`, `/api/weread-key`

### Upstream (Offline reader)

- Disclaimer → QR login → cookie session on SD (`/.crosspoint/weread/`)
- Cover shelf, detail+images, chapter-range cache → EPUB under `/WeRead/`
- Progress sync for matched shelf books
- wolfSSL transport **without** CA verify on device (document clearly)

### Port checklist

1. Inventory ActivityManager entry points; replace `goToWeRead*` companion graph with upstream `WeReadActivity` (+ progress sync activity if separate).
2. Delete companion sources and HTML key page; remove web routes.
3. Add `lib`/pio deps for wolfSSL **only if** community-sdk lacks an equivalent SecureClient; isolate from font/OTA mbedTLS path (upstream already keeps WeRead HTTP out of `HttpDownloader`).
4. Gate: compile into **SC and TC** under `ENABLE_CHINESE_VERSION`; **not** JA/KO.
5. i18n: import upstream WeRead strings into `chinese.yaml` / `english.yaml`; drop obsolete companion strings after UI switch.
6. Migrate users: one-time note that `wrk-` keys and companion cache are obsolete; QR login required; old offline companion JSON is not the new EPUB cache.
7. Tests: land `test/weread` + sim smoke (QR state machine with mocked HTTP).
8. Docs: rewrite WeRead sections in `chinese-build.md`, `webserver-endpoints.md`, USER_GUIDE; keep security notice.

**Do not** try to keep Companion and Offline side-by-side — upstream explicitly replaced the surface; dual stacks would blow RAM/flash.

## 6. Recommended integration strategy

Naive `git merge upstream/main` will conflict in ~146 files (fonts, Epub,
i18n, WeRead, pio, docs). Prefer **phased merges** on a long-lived branch
`cursor/upstream-sync-1.5.1-*` with green CI between phases.

```text
Phase 0  Inventory freeze (this doc) + pin upstream SHA
Phase 1  Infrastructure: clock/UTC, input-edge fixes, URL wrap, settings consolidations
Phase 2  Reader/Epub: cherry-pick perf + CrossPoint layout onto our vertical base; bump cache vers
Phase 3  SD fonts: flash-cache (#57) + CN guidance (#44) without swapping builtin headers
Phase 4  WeRead: full tree replace + wolfSSL + tests + docs
Phase 5  Apps menu visibility + WeRead-first ordering (no games)
Phase 6  Docs/AGENTS/CI/release SKU polish + version bump toward 1.5.x-ryos
```

### Phase mechanics

For each phase:

1. `git fetch upstream` and record tip SHA in the sync PR.
2. Prefer `git merge -s ort -Xours` **only** for pure font-header paths; otherwise merge then resolve by the §4 matrix (not blind `-Xours` on Epub/settings).
3. Where history is too tangled (WeRead), use directory replace:
   `git checkout upstream/main -- src/activities/apps/weread/` then delete
   companion-only leftovers and fix compile errors.
4. Run gates (§7) before opening the next phase PR (or before pushing the next
   commit on the sync branch).

### Alternative if merge pain is too high

Rebuild forward: branch from upstream 1.5.1, replay fork-owned patches
(SKU matrix, vertical Epub, GenSen fonts, no-games, ryOS branding) as a
ordered patch series. Only choose this if Phases 2–4 stall; it rewrites
familiar history for contributors.

## 7. Verification gates (per phase)

Cloud VM (no device) — CI-equivalent:

```bash
pio run                                 # default
pio run -e gh_release_tc                # after CJK-touching phases
pio run -e gh_release_sc                # required after WeRead phase
pio check --fail-on-defect low --fail-on-defect medium --fail-on-defect high
cmake -S test -B build/test -G Ninja -DCMAKE_BUILD_TYPE=Release \
  && cmake --build build/test && ctest --test-dir build/test --output-on-failure -j
PATH="/usr/lib/llvm-21/bin:$PATH" ./bin/clang-format-check
```

Simulator (WeRead + vertical):

```bash
cmake -S simulator -B simulator/build_sc -DSIMULATOR_CHINESE_UI_SIMPLIFIED=ON
cmake --build simulator/build_sc -j2
# keyboard-only computer-use: Apps → WeRead disclaimer/QR states; Reader vertical book
```

Device (human / later): X3 RTC clock, WeRead QR on trusted Wi-Fi, OTA + SD-font
flash cache, heap during WeRead download + silent restart into Reader.

## 8. Risk register

| Risk | Mitigation |
|---|---|
| wolfSSL + mbedTLS both linked → flash overflow on CJK SKUs | Size-check `gh_release_sc` after Phase 4; trim companion dead code first; keep WeRead CN-only |
| wolfSSL + freeink-sdk flash pressure on TC (~97%) | Size-check after each WeRead/feature land; prefer arena/heap wins over more flash |
| Section cache chaos after Epub merge | Explicit version bumps per SKU; document; accept one-time rebuild |
| Vertical regressions when taking `#50` | Keep vertical tests (`test/vertical_punctuation`, kinsoku, punct compression) in gate |
| User backlash losing Companion notes/reviews UI | Release note: offline book download is the product; notes/reviews not ported |
| Unofficial WeRead protocol / MITM | In-app disclaimer + USER_GUIDE security notice (upstream text) |
| Sync PR too large to review | One PR per phase; WeRead alone is its own PR |

## 9. Decisions (locked 2026-07-29)

1. **WeRead on SC and TC.** Gate with `ENABLE_CHINESE_VERSION` (both Chinese
   SKUs). JA/KO stay without WeRead.
2. **Keep GenSen** builtin coverage / font pipelines. Do not take upstream’s
   trimmed Noto 3500 headers. Optionally borrow their “complete SD font”
   prompt UX later.
3. **Bump to `1.5.1`** when the full sync (Phases 1–6) lands.
4. **OPDS stays an Apps entry** on ryOS (outside the upstream visibility catalog
   of games); include it in our kept-app visibility set if we adopt the bitmask.

## 10. Implementation status (sync PR)

Branch: `cursor/upstream-sync-1.5.1-2423` — merged `upstream/main` @ 1.5.1.

| Gate | Result |
|---|---|
| `pio run` (default / intl) | SUCCESS — Flash ~87.8% |
| `pio run -e gh_release_sc` | SUCCESS — Flash ~95.4%, WeRead linked, GenSen kept |
| `pio run -e gh_release_tc` | SUCCESS — Flash ~98.0%, WeRead linked, GenSen kept |
| Native `ctest` | **230/230 passed** |
| `pio check` | PASSED (WordList typed FreeDeleter) |
| Version | `1.5.1` in `platformio.ini` |

**TC flash audit (post-#64/#70):** ~98.0% used (6,423,027 / 6,553,600). SP RSA +
disclaimer strings + LWW sync grew flash vs post-arena ~97.5%. Arena `TextBlock`
is a RAM/fragmentation win, not a flash shrink. No further safe flash cut without
product tradeoffs (WeRead, GenSen coverage, or wolfSSL). Keep TC growth gated.

Deferred items completed on this branch:
- Full SD-font inactive-OTA flash cache (`SdCardFontCache` + `FontFile` / `preferFlash` on fork `SdCardFont`)
- Dictionary reader activities (TextBlock accessors + menu/long-press entry)
- Removed orphan `DateTimeEditActivity` (`DateTimeSettingsActivity::ManualEdit` covers UX)
- Background SNTP auto-sync on HalClock (`esp_netif_sntp_*` + DS3231 persist)
- Arena `TextBlock` (flat per-line heap blob; section cache 55 / 81–84)
- Adopted `freeink-sdk` + WeRead wolfSSL (plan originally preferred staying on open-x4)

Follow-ups still open:
- TC flash headroom (~98.0%) before more CN-only growth
- Full ruby layout pipeline (arena already serializes empty ruby strings)
- Host `HalOtaSlot` completeness for inactive-slot sim paths
- Post-1.5.1 still deferred: AirPage standalone app, Nightly OTA, Lyra theme, SDK bump `e514a868`

Landed after the 1.5.1 pin on this branch:
- Page deserialize reserve clamp + null TextBlock check
- ImageBlock row-buffer RAII (`makeUniqueNoThrow`)
- CJK/web-novel `<br>` margin strip (`fromBrElement`) + section cache 56 / 85–88
- Dictionary typed lookup/index failures
- WeRead disclaimer (#70) + last-write-wins progress sync (#64)
- Reader WeRead sync re-wired after the 1.5.1 merge drop

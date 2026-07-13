# ryOS CrossMux

[English](./README.md) | **中文**

**ryOS CrossMux** 是面向 Xteink X3 / X4 的閱讀優先固件。它源自
[CrossMux](https://github.com/0x1abin/crossmux)，並建立在
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) 之上。

本 fork 專注於中文與 CJK 書籍：直排 EPUB、更廣的字體覆蓋、可靠的 SD 卡字體，以及更快的 4 級灰階文字。閱讀統計、微信讀書與待機表盤予以保留；舊有的遊戲與玩具類應用不在固件中。

**當前固件版本：** 1.4.4

![ryOS CrossMux 運行在 Xteink 設備上](./docs/images/cover.jpg)

## 本 fork 新增了什麼

### 直排 CJK EPUB 閱讀

在閱讀設定或書內選單選擇 `排版方向 > 豎排（右起）`。版面引擎會：

- 由右至左排欄
- 使用直排標點呈現形
- 將重複省略號、破折號逐字堆疊
- 旋轉拉丁文段落，並保持短數字參考可讀
- 對緊湊橫向片段套用縱中橫
- 讓段落間距與區塊邊距沿欄軸方向作用
- 依閱讀方向反轉翻頁控制

直排僅在 EPUB 語言中繼資料標為中文、日文或韓文時啓用。其他書籍即使全域設定為直排，仍維持橫排。

### 中文固件

`gh_release_tc` 環境會產出以中文爲優先的固件，包含：

- 首次開機預設語言爲中文
- 英文與中文 UI 字串（中文 UI 爲繁體）
- 內嵌 GenSen Rounded TW 點陣字體
- CJK 斷行與標點規則
- 微信讀書書架、筆記、書評、搜尋、推薦與 SD 離線快取
- 從 `ryokun6/crossmux` Release 進行 OTA，並選用中文固件資源
- 與國際版相同的雙槽固件配置與回滾支援

預設 14pt 閱讀字體約涵蓋 7000 個常用漢字，以及現代 EPUB 常用符號。較小的 UI 字號使用更精簡的子集以符合 ESP32-C3 Flash 預算。簡體碼位會在查字時對應到內嵌的繁體字形，因此不必爲每個點陣各存一份。

### 更好的 SD 卡字體

`.cpfont` 字族可放在 SD 卡的 `/.fonts/` 或 `/fonts/`。載入器會按需索引大型 CJK 字族、預熱即將用到的頁面字形，並在缺少粗體／斜體字形時回退到 Regular。

倉庫亦包含 EB Garamond + Source Han Serif TC 建置腳本。詳見 [SD-card fonts](./docs/sd-card-fonts.md)。

### 更快的灰階文字

文字反鋸齒使用顯示器的四級灰階。本 fork 以窄條帶渲染兩個灰階平面並直接寫入顯示器，而不再在 RAM 中保留兩個額外的全螢幕緩衝。

### 更精簡的 Apps 選單

固件僅內建與閱讀相關的應用：

- OPDS 瀏覽器
- 閱讀統計（歷史、熱力圖、檔案與成就）
- 中文版的微信讀書
- 待機表盤（含潦草時鐘、AirPage，以及中文版的傳統日曆）

數獨、五子棋、踩地雷、2048、中國象棋、生命遊戲與頭像產生器已刻意排除。

## 閱讀功能

ryOS CrossMux 保留上游 CrossPoint 的主要閱讀能力：EPUB 2/3、章節導覽、腳註、書籤、跳轉百分比、內嵌樣式、圖片、字距、連字符、專注閱讀、自動翻頁、方向控制、螢幕截圖、QR、KOReader 進度同步，以及 `.epub` / `.txt` / `.xtc` / `.xtch` / `.bmp` 等格式。

無線工具包含檔案傳輸、EPUB Optimizer、網頁設定、WebSocket 上傳、WebDAV、Calibre 無線連線、OPDS，以及從最新 `ryokun6/crossmux` GitHub Release 進行網路 OTA。OTA 會依目前安裝的版本選擇 `firmware.bin` 或 `firmware-tc.bin`。也可透過 USB、網頁刷機器或「SD 卡固件更新」安裝。

## X3 與 X4 支援

同一份固件可在兩種裝置上運行。開機時偵測硬體，並調整面板尺寸、按鍵、電池來源與可用周邊。詳見 [device variants](./docs/engineering/device-variants.md)。

## 刷機前注意

> **USB 鎖定裝置警告**
>
> Xteink Unlocker 官方支援 CrossPoint 與 CrossInk。ryOS CrossMux 是社群 fork。在 USB 鎖定的裝置上刷入，可能導致無法以官方途徑救援。除非你已有可驗證的救援方式，否則請勿在鎖定裝置上安裝本 fork。

## 建置與安裝

### 前置條件

- [pioarduino](https://github.com/pioarduino/pioarduino) 或其 VS Code 外掛
- Python 3.8+
- 支援 submodule 的 Git
- 支援資料傳輸的 USB-C 線

### 取得原始碼

```bash
git clone --recursive https://github.com/ryokun6/crossmux.git
cd crossmux
```

若克隆時未帶 submodule：

```bash
git submodule update --init --recursive
```

### 建置

```bash
# 國際版固件
pio run -e gh_release

# 中文固件
pio run -e gh_release_tc

# 建置並燒錄
pio run -e gh_release_tc -t upload
```

產物位於 `.pio/build/<env>/firmware.bin`。

### 重新產生 CJK 字體（可選）

僅在修改字集或更新內嵌字體時需要。將 `GenSenRounded2TW-R.otf` 放到
`lib/EpdFont/builtinFonts/source/GenSenRounded2TW/`，然後執行：

```bash
bash lib/EpdFont/scripts/build-cn-builtin-fonts.sh
```

完整說明見 [docs/engineering/chinese-build.md](./docs/engineering/chinese-build.md)。

## 中文閱讀注意事項

中文固件在有限的 Flash 空間內做了取捨：

- **內嵌字體以繁體字形爲準**；簡體碼位會在執行時對應到繁體點陣。
- **UI 字串爲繁體中文**（語言選單顯示爲「中文」）。
- **大號字下中文正文可能留白**：16pt / 18pt（LARGE / EXTRA_LARGE）僅內嵌 UI 所需小字集，這兩檔是爲英文 EPUB 調優。讀中文請切到 MEDIUM。
- **無 CJK 粗體／斜體字形**：會回退爲 Regular。若需要更多字重，可在 SD 卡載入自訂字體。

## 授權與致謝

ryOS CrossMux 建立在 [CrossMux](https://github.com/0x1abin/crossmux)、
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) 與
[diy-esp32-epub-reader](https://github.com/atomic14/diy-esp32-epub-reader) 的工作之上。

本專案與 Xteink 及任何裝置廠商均無隸屬關係。

以 [MIT License](./LICENSE) 授權。

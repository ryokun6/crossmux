# ryOS CrossMux

[English](./README.en.md) | [中文](./README.md) | **日本語** | [한국어](./README.ko.md)

**ryOS CrossMux** は Xteink X3 / X4 向けの「読書優先」ファームウェアです。
[CrossMux](https://github.com/0x1abin/crossmux) からのフォークで、
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) を土台にしています。

このフォークは中国語・日本語・韓国語 (CJK) の書籍に注力しています: 縦書き EPUB レイアウト、広い文字カバレッジ、信頼性の高い SD カードフォント、そして高速な 4 階調グレースケール文字描画。読書統計とスタンバイ表示は維持し、旧来のゲーム・玩具系アプリはファームウェアに含めていません。

**現在のファームウェアバージョン:** 1.4.7

![Xteink 端末で動作する ryOS CrossMux](./docs/images/xteink-vertical-reading.jpg)

## このフォークの追加機能

### 縦書き CJK EPUB 読書

リーダー設定または書籍内メニューで `組み方向 > 縦書き (右から左)` を選ぶと、レイアウトエンジンは:

- 段組を右から左へ配置
- CJK 約物に縦書き用字形を使用
- 連続する三点リーダーやダッシュを 1 マスずつ縦に連結
- 欧文を回転しつつ、短い数字は縦中横で読みやすく維持
- 段落間隔とブロック余白を段組軸に沿って適用
- ページ送りボタンを読書方向に合わせて反転

縦書きは EPUB の言語メタデータが中国語・日本語・韓国語の場合のみ有効になります。それ以外の書籍は、全体設定が縦書きでも横書きのまま表示されます。

### 日本語ファームウェア

国際版・中国語版に加えて、日本語 / 韓国語 SKU を提供しています:

| 環境 | ロケール | UI | OTA アセット |
| --- | --- | --- | --- |
| `gh_release_ja` | `ja-JP` | 日本語 | `firmware-ja.bin` |
| `gh_release_ko` | `ko-KR` | 韓国語 | `firmware-ko.bin` |

日本語 SKU の内容:

- 初回起動時の UI 言語は日本語 (英語 + 日本語 UI を内蔵)
- 内蔵 CJK ビットマップフォントは **GenSen Rounded 2 JP** (源泉丸ゴシック JP) Regular
- 簡体字⇔繁体字の変換 (OpenCC) は **行いません** — 日本語の字体をそのまま表示します
- `ryokun6/crossmux` からのデュアルスロット OTA (`firmware-ja.bin` を自動選択)

文字カバレッジはポイントサイズごとに段階化されています:

| サイズ | 収録内容 | 出典 |
| --- | --- | --- |
| 8/10/12pt | 教育漢字 1,026 字 + かな + UI 文字 | 学年別漢字配当表 (2017) |
| 14pt (既定の「中」) | 常用漢字 2,136 字 + かな + UI 文字 + EPUB 記号 | 常用漢字表 (2010) |
| 16/18pt | かな + UI 文字のみ | — |

詳細は [docs/engineering/japanese-korean-build.md](./docs/engineering/japanese-korean-build.md) を参照してください。

### より良い SD カードフォント

`.cpfont` フォントファミリーは SD カードの `/.fonts/` または `/fonts/` に配置できます。ローダーは大型 CJK ファミリーをオンデマンドで索引化し、次ページの字形を先読みし、CJK の太字・斜体字形が無い場合は Regular へフォールバックします。

変換コマンドや Unicode プリセット、CJK フォントビルダーは [SD-card fonts](./docs/sd-card-fonts.md) に記載しています。

### 高速なグレースケール文字

文字アンチエイリアスにはディスプレイの 4 階調を使用します。このフォークは 2 枚のグレースケールプレーンを細いストリップ単位で描画してそのままディスプレイへ書き込むため、RAM に全画面バッファを 2 枚余分に持ちません。

### 精簡な Apps メニュー

ファームウェアには読書関連アプリのみを内蔵しています:

- OPDS ブラウザー
- 読書統計 (履歴・ヒートマップ・プロフィール・実績)
- スタンバイ表示 (Sloppy Clock、AirPage など)

数独・五目並べ・マインスイーパー・2048・シャンチー・ライフゲーム・アバター生成は意図的に除外しています。

## リーダー機能

ryOS CrossMux は上流 CrossPoint の主要機能を維持しています: EPUB 2/3 レンダリング、章ナビゲーション、脚注、しおり、パーセント移動、埋め込みスタイル、画像、カーニング、ハイフネーション、集中読書、自動ページめくり、画面向き制御、スクリーンショット、QR 表示、KOReader 進捗同期、`.epub` / `.txt` / `.xtc` / `.xtch` / `.bmp` 対応。

ワイヤレス機能はファイル転送、EPUB Optimizer、Web 設定、WebSocket アップロード、WebDAV、Calibre ワイヤレス接続、OPDS、そして最新の `ryokun6/crossmux` GitHub Release からのネットワーク OTA を含みます。OTA はインストール済みビルドに合わせて `firmware.bin` / `firmware-ja.bin` などを自動選択します。USB・Web フラッシャー・「SD カードファームウェア更新」からのインストールも可能です。

## X3 / X4 対応

1 つのファームウェアイメージが両機種で動作します。起動時にハードウェアを検出し、パネルサイズ・ボタン・バッテリー・周辺機器を自動調整します。詳細は [device variants](./docs/engineering/device-variants.md) を参照してください。

## フラッシュ前の注意

> **USB ロック端末に関する警告**
>
> Xteink Unlocker が公式にサポートするのは CrossPoint と CrossInk です。ryOS CrossMux はコミュニティフォークであり、USB ロックされた端末に書き込むと公式の復旧手段が使えなくなる可能性があります。検証済みの復旧方法がない限り、ロックされた端末にはインストールしないでください。

## ビルドとインストール

### 必要なもの

- [pioarduino](https://github.com/pioarduino/pioarduino) またはその VS Code 拡張
- Python 3.8 以降
- submodule 対応の Git
- データ通信対応の USB-C ケーブル

### ソースの取得

```bash
git clone --recursive https://github.com/ryokun6/crossmux.git
cd crossmux
```

submodule なしでクローンした場合:

```bash
git submodule update --init --recursive
```

### ビルド

```bash
# 日本語ファームウェア (ja-JP)
pio run -e gh_release_ja

# ビルドして書き込み
pio run -e gh_release_ja -t upload
```

成果物は `.pio/build/gh_release_ja/firmware.bin` に生成されます。

[CrossPoint web flasher](https://crosspointreader.com/#flash-tools) で `Custom .bin` を選んで書き込むこともできます。

### CJK フォントの再生成 (任意)

文字集合を変更したり内蔵フォントを更新するときのみ必要です。`GenSenRounded2JP-R.otf` ([ButTaiwan/gensen-font](https://github.com/ButTaiwan/gensen-font)) を `lib/EpdFont/builtinFonts/source/GenSenRounded2JP/` に置いてから:

```bash
bash lib/EpdFont/scripts/build-ja-builtin-fonts.sh
```

## 日本語読書の注意点

限られた Flash 容量のため、次のトレードオフがあります:

- **読書には 14pt (「中」) を推奨** — 常用漢字 2,136 字をフル収録するのはこのサイズだけです。8–12pt は教育漢字 1,026 字、16/18pt は UI 文字のみのため、小説の漢字が □ になることがあります。
- **内蔵 CJK フォントは 1 ウェイトのみ** — 太字・斜体は Regular にフォールバックします。字重が必要な場合は SD カードフォントを利用してください。
- **簡体字⇔繁体字の変換は行いません** — 中国語の EPUB を開くと、日本語サブセットにない字は □ になる場合があります。
- 縦書きは EPUB の言語メタデータ (`ja` など) に依存します。

## ライセンスとクレジット

ryOS CrossMux は [CrossMux](https://github.com/0x1abin/crossmux)、
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)、
[diy-esp32-epub-reader](https://github.com/atomic14/diy-esp32-epub-reader) の成果の上に構築されています。

本プロジェクトは Xteink およびいかなる端末メーカーとも無関係です。

[MIT License](./LICENSE) の下で公開されています。

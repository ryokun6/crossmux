# ryOS CrossMux

[English](./README.en.md) | [中文](./README.md) | **日本語** | [한국어](./README.ko.md)

**ryOS CrossMux** は Xteink X3 / X4 向けの「読書優先」ファームウェアです。
[CrossMux](https://github.com/0x1abin/crossmux) からのフォークで、
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) を土台にしています。

このフォークは中国語・日本語・韓国語 (CJK) の書籍に注力しています: 縦書き EPUB レイアウト、広い文字カバレッジ、信頼性の高い SD カードフォント、そして高速な 4 階調グレースケール文字描画。読書統計、微信読書（中国語 SKU）、スタンバイ表示は維持し、旧来のゲーム・玩具系アプリはファームウェアに含めていません。

**現在のファームウェアバージョン:** 1.4.8

![Xteink 端末で動作する ryOS CrossMux](./docs/images/xteink-vertical-reading.jpg)

## このフォークの追加機能

### 縦書き CJK EPUB 読書

リーダー設定または書籍内メニューで `組み方向 > 縦書き (右から左)` を選ぶと、レイアウトエンジンは:

- 段組を右から左へ配置
- CJK 約物に縦書き用字形を使用
- 連続する三点リーダーやダッシュを 1 マスずつ縦に連結
- 欧文を回転しつつ、短い数字は縦中横で読みやすく維持
- コンパクトな横書き断片に縦中横を適用
- 段落間隔とブロック余白を段組軸に沿って適用
- ページ送りボタンを読書方向に合わせて反転

縦書きは EPUB の言語メタデータが中国語・日本語・韓国語の場合のみ有効になります。それ以外の書籍は、全体設定が縦書きでも横書きのまま表示されます。

### ローカライズ CJK ファームウェア (繁体 / 簡体 / 日本語 / 韓国語)

国際版に加えて 4 つの CJK SKU を提供しています:

| 環境 | ロケール | UI | OTA アセット |
| --- | --- | --- | --- |
| `gh_release_tc` | `zh-TW` | 繁体字中国語 | `firmware-tc.bin` |
| `gh_release_sc` | `zh-CN` | 簡体字中国語（台湾用語 YAML を OpenCC `tw2sp` で生成） | `firmware-sc.bin` |
| `gh_release_ja` | `ja-JP` | 日本語 | `firmware-ja.bin` |
| `gh_release_ko` | `ko-KR` | 韓国語 | `firmware-ko.bin` |

**日本語 SKU** の内容:

- 初回起動時の UI 言語は日本語（英語 + 日本語 UI を内蔵）
- 内蔵 CJK ビットマップフォントは **GenSen Rounded 2 JP**（源泉丸ゴシック JP）Regular
- 簡体字⇔繁体字の変換（OpenCC）は **行いません** — 日本語の字体をそのまま表示します
- `ryokun6/crossmux` からのデュアルスロット OTA（`firmware-ja.bin` を自動選択）
- 微信読書・中国伝統暦フェイスは中国語 SKU 専用であり、日本語ビルドには含まれません

文字カバレッジはポイントサイズごとに段階化されています:

| サイズ | 収録内容 | 出典 |
| --- | --- | --- |
| 8/10/12pt | 教育漢字 1,026 字 + かな + UI 文字 | 学年別漢字配当表 (2017) |
| 14pt（既定の「中」） | 常用漢字 2,136 字 + かな + UI 文字 + EPUB 記号 | 常用漢字表 (2010) |
| 16/18pt | かな + UI 文字のみ | — |

詳細は [docs/engineering/japanese-korean-build.md](./docs/engineering/japanese-korean-build.md) を参照してください。中国語 SKU の SC↔TC 変換などは [chinese-build.md](./docs/engineering/chinese-build.md) を参照。

### より良い SD カードフォント

`.cpfont` フォントファミリーは SD カードの `/.fonts/` または `/fonts/` に配置できます。ローダーは大型 CJK ファミリーをオンデマンドで索引化し、次ページの字形を先読みし、CJK の太字・斜体字形が無い場合は Regular へフォールバックします。

リポジトリには EB Garamond + Source Han Serif TC のビルダースクリプトも含まれます。変換コマンドや Unicode プリセットは [SD-card fonts](./docs/sd-card-fonts.md) に記載しています。

### 高速なグレースケール文字

文字アンチエイリアスにはディスプレイの 4 階調を使用します。このフォークは 2 枚のグレースケールプレーンを細いストリップ単位で描画してそのままディスプレイへ書き込むため、RAM に全画面バッファを 2 枚余分に持ちません。高速な字形 blit とストリップ棄却により、SD フォントや縦書きページでの重複作業を削減します。

### キャッシュと画像処理

セクションキャッシュの再構築は、有効なキャッシュを置き換える前に `.tmp` サイドカーへ書き込みます。SD カードが古いセクションファイルの truncate / rename を拒否しても、完成したサイドカーを使い続けて章の再構築失敗を避けられます。

`大きい画像のみ` モードでは、インラインアイコン・em サイズの区切り・小さな単独画像を落としつつ、本格的な図版は残します。

### 精簡な Apps メニュー

ファームウェアには読書関連アプリのみを内蔵しています:

- OPDS ブラウザー
- 読書統計（履歴・ヒートマップ・プロフィール・実績）
- 微信読書（中国語ビルドのみ）
- スタンバイ表示（Sloppy Clock、AirPage など。中国伝統暦は中国語ビルドのみ）

数独・五目並べ・マインスイーパー・2048・シャンチー・ライフゲーム・アバター生成は意図的に除外しています。

## リーダー機能

ryOS CrossMux は上流 CrossPoint の主要機能を維持しています:

- EPUB 2 / EPUB 3 レンダリング
- 章ナビゲーション、脚注、しおり、パーセント移動
- 埋め込みスタイル、画像、カーニング、ハイフネーション、集中読書
- 自動ページめくり、画面向き制御、スクリーンショット、QR 表示
- KOReader 進捗同期
- `.epub` / `.txt` / `.xtc` / `.xtch` / `.bmp`
- 最近の本、フォルダ閲覧、キャッシュ管理、長押し削除
- インストール可能な SD カードフォント（Regular / Bold / Italic / Bold-Italic）
- 国際化 UI 翻訳と RTL インターフェース

ワイヤレス機能はファイル転送、EPUB Optimizer、Web 設定、WebSocket アップロード、WebDAV、Calibre ワイヤレス接続、OPDS、そして最新の `ryokun6/crossmux` GitHub Release からのネットワーク OTA を含みます。OTA はインストール済みビルドに合わせて `firmware.bin` / `firmware-tc.bin` / `firmware-sc.bin` / `firmware-ja.bin` / `firmware-ko.bin` を自動選択します。USB・Web フラッシャー・「SD カードファームウェア更新」からのインストールも可能です。

## X3 / X4 対応

1 つのファームウェアイメージが両機種で動作します。起動時にハードウェアを検出し、パネルサイズ・ボタン・バッテリー・周辺機器を自動調整します。

- X4: 800 × 480 SSD1677 ディスプレイ
- X3: 792 × 528 UC81xx ディスプレイ、DS3231 時計、燃料ゲージ、傾きページ送り

X3 専用ビルドはありません。いずれかの言語バリアントをビルドし、Web フラッシャーで対応する実機ターゲットに同じ `firmware.bin` を書き込みます。詳細は [device variants](./docs/engineering/device-variants.md) を参照してください。

## フラッシュ前の注意

> **USB ロック端末に関する警告**
>
> Xteink Unlocker が公式にサポートするのは CrossPoint と CrossInk です。ryOS CrossMux はコミュニティフォークであり、USB ロックされた端末に書き込むと公式の復旧手段が使えなくなる可能性があります。検証済みの復旧方法がない限り、ロックされた端末にはインストールしないでください。

xteink.com から直接購入した端末は通常 USB ロックされていません。ブラウザがシリアルデバイスを認識しない場合は、データ通信対応ケーブル・USB ポート・Chromium 系ブラウザを試してから、ロック済みと判断してください。

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
# 国際版
pio run -e gh_release

# 繁体字中国語 (zh-TW)
pio run -e gh_release_tc

# 簡体字中国語 (zh-CN)
pio run -e gh_release_sc

# 日本語 (ja-JP)
pio run -e gh_release_ja

# 韓国語 (ko-KR)
pio run -e gh_release_ko
```

成果物:

```text
.pio/build/gh_release/firmware.bin
.pio/build/gh_release_tc/firmware.bin
.pio/build/gh_release_sc/firmware.bin
.pio/build/gh_release_ja/firmware.bin
.pio/build/gh_release_ko/firmware.bin
```

### USB で書き込み

```bash
# 国際版
pio run -e gh_release -t upload

# 繁体字中国語
pio run -e gh_release_tc -t upload

# 簡体字中国語
pio run -e gh_release_sc -t upload

# 日本語
pio run -e gh_release_ja -t upload

# 韓国語
pio run -e gh_release_ko -t upload
```

[CrossPoint web flasher](https://crosspointreader.com/#flash-tools) で実機を選び、`Custom .bin` で対応するビルド成果物をアップロードすることもできます。ターゲット選択は端末ブートローダ向けのパッチであり、別言語のビルドを選ぶものではありません。

公式ファームウェアに戻す場合は、公式の
[CrossPoint release](https://github.com/crosspoint-reader/crosspoint-reader/releases)
を書き込んでください。

### CJK フォントの再生成（任意）

文字集合を変更したり内蔵フォントを更新するときのみ必要です。`GenSenRounded2JP-R.otf`（[ButTaiwan/gensen-font](https://github.com/ButTaiwan/gensen-font)）を `lib/EpdFont/builtinFonts/source/GenSenRounded2JP/` に置いてから:

```bash
bash lib/EpdFont/scripts/build-ja-builtin-fonts.sh
```

中国語・韓国語の再生成手順は
[chinese-build.md](./docs/engineering/chinese-build.md) および
[japanese-korean-build.md](./docs/engineering/japanese-korean-build.md) を参照してください。

## カスタムフォントのインストール

ファームウェアを再ビルドせずに `.cpfont` をインストールできます:

1. 端末で `設定 > システム > フォント管理` を開く
2. またはファイル転送 Web UI からアップロード
3. または SD カードの `/.fonts/FamilyName/` か `/fonts/FamilyName/` にファミリーをコピー
4. `設定 > リーダー > リーダーフォントファミリー` で選択

同じファミリーが両方のルートにある場合、隠しディレクトリ `/.fonts/` が優先されます。詳細は [docs/sd-card-fonts.md](./docs/sd-card-fonts.md) を参照してください。

## 日本語読書の注意点

限られた Flash 容量のため、次のトレードオフがあります:

- **読書には 14pt（「中」）を推奨** — 常用漢字 2,136 字をフル収録するのはこのサイズだけです。8–12pt は教育漢字 1,026 字、16/18pt は UI 文字のみのため、小説の漢字が □ になることがあります。
- **内蔵 CJK フォントは 1 ウェイトのみ** — 太字・斜体は Regular にフォールバックします。字重が必要な場合は SD カードフォントを利用してください。
- **簡体字⇔繁体字の変換は行いません** — 中国語の EPUB を開くと、日本語サブセットにない字は □ になる場合があります。
- 縦書きは EPUB の言語メタデータ（`ja` など）に依存します。
- デスクトップシミュレータは現在 X4 幾何のみをモデル化しています。X3 の表示・周辺機器は実機が必要です。

## 開発

プルリクエストを開く前の確認例:

```bash
./bin/clang-format-fix
pio check -e default
pio run -e default
pio run -e gh_release_tc
pio run -e gh_release_sc
pio run -e gh_release_ja
pio run -e gh_release_ko
```

ESP32-C3 の利用可能 RAM は約 380 KB です。リーダーキャッシュは SD カードの `/.crosspoint/` にあり、永続的なヒープ圧を増やさないよう注意してください。

入り口:

- [ユーザーガイド](./USER_GUIDE.md)
- [開発ガイド](./docs/contributing/README.md)
- [アーキテクチャ](./docs/contributing/architecture.md)
- [中国語ビルド](./docs/engineering/chinese-build.md)
- [日本語 / 韓国語ビルド](./docs/engineering/japanese-korean-build.md)
- [キャッシュ管理](./docs/engineering/cache-management.md)
- [バイナリファイル形式](./docs/file-formats.md)
- [Web サーバー](./docs/webserver.md)
- [デスクトップ / WebAssembly シミュレータ](./simulator/README.md)

## ライセンスとクレジット

ryOS CrossMux は [CrossMux](https://github.com/0x1abin/crossmux)、
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)、
[diy-esp32-epub-reader](https://github.com/atomic14/diy-esp32-epub-reader) の成果の上に構築されています。

本プロジェクトは Xteink およびいかなる端末メーカーとも無関係です。

[MIT License](./LICENSE) の下で公開されています。

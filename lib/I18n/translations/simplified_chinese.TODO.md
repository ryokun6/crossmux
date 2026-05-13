# 简体中文翻译——待补充清单

本文件用于追踪 `simplified_chinese.yaml` 中**未翻译**或**待复核**的字符串。
未在 YAML 中提供的键会自动 fallback 到 `english.yaml` 中的英文文本。

## 1. Apps 文案(暂缓,后续单独翻译)

按用户决定,Apps（游戏/小工具）相关的字符串本轮保留英文。涉及的 key 命名前缀:

- `STR_SUDOKU_*` —— 数独
- `STR_GOMOKU_*` —— 五子棋
- `STR_CHINESE_CHESS_*` —— 中国象棋
- `STR_GAME_*` —— 游戏通用 UI（继续/新游戏/统计/最佳时间等）
- `STR_UGLY_AVATAR`, `STR_SAVE`, `STR_RANDOM` —— Ugly Avatar 应用

这些字符串目前在中文界面下显示为英文（gen_i18n 自动 fallback）。
后续单独 PR 增量补全即可。

## 2. 已翻译但需要 native speaker 复核的术语

- `STR_HYPHENATION` -> "连字符"（也可考虑"自动断词"/"软连字符"）
- `STR_OPEN_DYSLEXIC` -> 保留英文（专有字体名）
- `STR_THEME_LYRA` / `STR_THEME_ROUNDEDRAFF` -> 保留英文（主题专有名）
- `STR_BOOK_S_STYLE` -> "书籍样式"（vs "书籍内嵌样式"）
- `STR_FOCUS_READING` -> "专注阅读"
- `STR_SUNLIGHT_FADING_FIX` -> "阳光褪色修正"（电子墨水屏阳光下显示恢复,术语待对齐）

## 3. printf 占位符已逐键审计

含 `%d`/`%s`/`%zu`/`%.2f%%`/`%u` 的键已确认翻译中保留占位符完整。

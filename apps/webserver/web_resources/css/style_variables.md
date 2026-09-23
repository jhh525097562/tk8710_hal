# Style Variables

定义位置：`web_resources/html/layout_header.html`。

亮色映射：
- `--tm-nav-bg`: `#2f67b3`（顶部与侧边栏同色）
- `--tm-nav-hover`: `#3f7ed2`
- `--tm-page-bg`: `#f4f8fd`
- `--tm-card-bg`: `#ffffff`
- `--tm-border`: `#d9e5f3`
- `--tm-text-title`: `#1d2e44`
- `--tm-text-body`: `#2e445f`
- `--tm-text-hint`: `#4f6783`
- `--tm-link`: `#1f62bb`
- `--tm-accent`: `#2b72cd`

暗色映射（`prefers-color-scheme: dark`）：
- `--tm-nav-bg`: `#1f3f68`
- `--tm-nav-hover`: `#2d5788`
- `--tm-page-bg`: `#15202b`
- `--tm-card-bg`: `#1f2b37`
- `--tm-border`: `#2d3f54`
- `--tm-text-title`: `#e8f1ff`
- `--tm-text-body`: `#d7e3f2`
- `--tm-text-hint`: `#aec2d9`
- `--tm-link`: `#8fc1ff`
- `--tm-accent`: `#7eb3ff`

状态规则：
- `hover`: 使用 `--tm-nav-hover` / `--tm-btn-hover`
- `active`: 菜单白底 + `--tm-accent` 左侧高亮
- `disabled`: 按钮使用 `--tm-btn-disabled`

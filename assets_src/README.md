# 素材与资源

此版本全部是原创 C 程序绘制的像素图，不依赖外部贴图下载。

- 像素素材源：`../main/game_view.c` 中的 `knight`、`tile_sprite`、`castle`。
- 地图和敌人数值：`../main/tower.c` 的 `initial_map` 和敌人定义。
- 界面配色：game_view.c 文件开头的 BG/INK/PANEL/STONE/EDGE/BRASS/MINT 等常量。
- 字体：官方固定引擎提供的位图字。当前短英文标签，避免擅自添加不支持的中文字形。
- 生成的 GSP 画布：官方模板 `../main/make_canvas_png.py`；构建时生成并嵌入，不是额外刷写的素材分区。
- 来源/许可：原创玩法与图形代码按 MIT 许可；官方模板/引擎及 raylib_screen_mirror 适配层沿用各自许可（见 NOTICE）。没有复制米环社区包、商业游戏图形、音乐或源码。

预览：`../artifacts/title.png`、`dungeon.png`、`combat.png`、`summit.png`。

如后续改成图片资源，保留原文件和来源/许可，走官方 atlas/asset pipeline；不要另写浏览器/Python 渲染器替代设备 C 代码。

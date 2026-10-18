# 研究报告索引

三份针对 Mosaico Magic Tower 的研究，全部基于本仓库的真实源码与真实通关数据，
以及已读过的开源魔塔引擎 / 成品塔 / 官方引擎文档。

| 文档 | 回答的问题 | 主要方法 |
|---|---|---|
| [`magic-tower-map-design-skill.md`](magic-tower-map-design-skill.md) | **魔塔的地图应该怎么设计？** | 开源引擎 `mota-js` + 真实成品塔 `arcxingye/MagicTower`（胖老鼠 24 层）+ h5mota 论坛四篇核心指南（tid=1183 避雷指南 / tid=1052 拒发审核 / tid=241 阻击夹域网 / tid=210 如何造爽塔）；产出可执行的规范、预算表、反模式清单与 37 条校验规则 |
| [`performance-and-thermal.md`](performance-and-thermal.md) | **为什么玩久了会发热？帧率 30 到底有没有必要？** | 把 `main/game_view.c` 链接到带计数器和真实帧缓冲的图形桩上，逐帧统计并做像素级差分；再对照 `raylib-lite-engine` 的公开 API 与 ESP32-S31 的功耗资料 |
| [`gameplay-improvements.md`](gameplay-improvements.md) | **怎么让这个游戏更好玩？** | 跑真实的通关路线（`tests/win_route.json`）统计战斗占比、血量余量、金币流向；对照参考塔的数值经济学 |

---

## 复现

两个探针都是 host 侧的，不改动固件、不需要 ESP-IDF：

```sh
python tools/run_probes.py            # 编译并运行两个探针
python tools/analyze_maps.py          # 地图分析报告
python tools/analyze_maps.py --check  # 只跑检查项，有失败则退出码 1
```

| 工具 | 说明 |
|---|---|
| `tools/analyze_maps.py` | 直接解析 `main/tower.c` 的 `initial_map`，输出逐层统计 + 14 条自动检查（含对称性、平面复用、闲置怪、战斗占比） |
| `tools/render_probe/mosaico_raylib_fast.h` | 图形 API 的计数 + 帧缓冲桩件 |
| `tools/render_probe/measure_render.c` | 每帧绘制成本与帧间像素差分 |
| `tools/render_probe/verify_model_defects.c` | 存档校验与失败条件验证（链接真实 `tower.c`） |

---

## 三句话结论

1. **地图**：十层塔的骨架是稳的（钥匙/门总数比 1.08 落在健康区间），但空间形态、
   楼梯惯例、敌人密度、物件密度、红钥匙稀缺性、水层、对称性、平面复用、闲置怪、
   战斗占比这十条全部不合格。**14 条自动检查只过 3 条。**
   最刺眼的三条：**7/10 层形状镜像对称**、**10 层只有 7 种平面（5/8/10 与 7/9 各共用一个）**、
   **4 只 0 伤怪（闲置怪）占了 17 场战斗的 23.5%**。

2. **性能**：实测一帧 **1,376 次绘制调用、831,558 次像素写入**（3.6 屏）；
   而**一次真实操作只改变 1,814 个像素（0.787%）**——99.8% 的重绘是冗余的。
   空闲时 **83.7% 的帧与上一帧逐字节相同**。30 fps 确实没有必要，
   全游戏最快的动画只有 2 Hz。
   另外查到三件更要紧的事：**这块屏是 AMOLED 不是 LCD（没有背光可调）**、
   **当前 40 MHz QSPI 下单帧传输 23.04 ms 超过 60 Hz 的 TE 周期 16.67 ms，画面其实在撕裂**、
   **GSP 服务循环默认跑 100 Hz**。

3. **玩法**：**战斗只占全局的 3.0%**，而且每一场战斗在开打之前就被判定为"必胜"
   （打不赢的仗游戏不让你打）。同时**血量预算的余量只有 6.0%**——
   数值其实调得很紧，只是因为没有死亡条件，这份紧度完全没被兑现。

---

## 附带修掉的两个真 bug

`tools/render_probe/verify_model_defects.c` 链接真实的 `main/tower.c` 后复现：

- **哨兵会永久损坏存档**：`tower_enemy_preview()` 发 0 金币，
  `valid_state()` 却按 60 金币重构、并且显式拒绝 `consumed[SENTINEL]`。
  杀掉哨兵后存档再也读不出来。
- **游戏没有失败条件，但有软锁**：HP 无上限、药水无限喝、地图无陷阱，
  玩家永远不会死；但如果 HP 耗光而药水也喝完了，就会卡在"打不过也死不了"。

详见 `gameplay-improvements.md` §6。

---

## 社区判据的出处

h5mota 是 Vue SPA，`web_fetch` / `curl` 只能拿到空壳；下面四篇是**用 Chromium 渲染后
逐字读取 `document.body.innerText`** 得到的完整正文（含全部楼层与楼中楼）：

- tid=1183「【面向新作者】新人造塔避雷指南，看完保证做不出紫塔！」— https://h5mota.com/bbs/thread/?p=1&tid=1183
- tid=1052「拒发审核简易指南」（又名堆水空指南）— https://h5mota.com/bbs/thread/?p=1&tid=1052
- tid=241「关于部分作者在作品中大量使无意义阻击夹域网之事」— https://h5mota.com/bbs/thread/?p=1&tid=241
- tid=210「如何造爽塔」— https://h5mota.com/bbs/thread/?p=1&tid=210

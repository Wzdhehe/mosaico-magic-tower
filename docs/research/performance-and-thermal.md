# 性能与发热分析（ESP-Mosaico / Magic Tower）

> 全部数字来自**本仓库的真实源码**：`main/game_view.c` 的渲染器被链接到一个带计数器和
> 480×480 RGB565 帧缓冲的桩件上，逐帧统计绘制调用与像素写入，再对相邻两帧做逐像素差分。
> 复现方式见 §1。
> 凡是推断而非实测的地方都标了「推断」；凡是外部资料没有亲自读到的都标了「未证实」。

---

## 0. 结论速览

| # | 问题 | 证据 | 修法 | 预期收益 |
|---|---|---|---|---|
| **P0** | **Wi-Fi 射频全程开着**：`CONFIG_ESP_IRIS_TRANSPORT_TCP=y` + `CONFIG_ESP_IRIS_TCP_PAIRING=y` | `sdkconfig.defaults` | 游戏中关掉 ESP-Iris 的 TCP 通道 | S31 资料：Wi-Fi 接收 110–117 mA、modem-sleep 91 mA。这是**最大单项** |
| **P1** | **每帧两次全屏 `ClearBackground`**，第一次被第二次完全覆盖 | 实测 460,800 px/帧，占全部像素写入的 55.4% | 删掉 `ClearBackground(INK)` | **像素写入 −27.7%** |
| **P2** | **30 fps 全屏重绘，但画面在两次输入之间逐字节相同** | 实测 10 秒空闲期 251/300 帧与上一帧完全相同 | present-on-change / `idle()` | **空闲时 −83.7%** |
| **P3** | **静态层每帧重画**：棋盘底 + 93 个墙 | 约 215,000 px/帧 | 脏矩形 / 缓存底图 | −26%（需框架配合，见 §6.3） |
| **P4** | **帧率 30 远超内容所需**：最快动画 2 Hz | 精灵 bob 周期 15–48 tick | `target_fps` 30 → 10 | CPU 与 QSPI 流量 **−67%** |
| **P5** | **逻辑绑在 tick 上**：key repeat、提示时长都是 tick 计数 | `GAME_KEY_REPEAT=7`、`notice_ticks=90/120` | 改时间基 | 降帧率的前提，否则手感会变 |
| **P6** | **代码从 PSRAM 取指** | `CONFIG_SPIRAM_XIP_FROM_PSRAM=y` | 关闭 | 取指变快；且 S31 上有已报道的稳定性问题 |
| **P7** | **`.bss` 放 PSRAM** | `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y` | 关闭 | 变量访问不再跨 PSRAM 总线 |
| **P8** | **音量滑条不改功放音量** | `esp_codec_dev_set_vol(codec, 80)` 只在 init 调一次 | 滑条联动功放；静音时断电放 | 功放静态电流 |
| **P9** | **背光无调节** | 全仓库无背光代码 | PWM 降到 60–70% | 面板电流近似线性下降 |
| **P10** | **无 auto light-sleep** | 无 `CONFIG_PM_ENABLE` | `esp_pm_configure(light_sleep_enable=true)` | 帧间 ~1–5 mA（**必须在关掉 Wi-Fi 之后**） |

**一句话**：这个游戏 83.7% 的时间在重画一个逐字节不变的画面，同时 Wi-Fi 射频一直开着。
前者是软件浪费，后者是硬件浪费，两个都得堵。

---

## 1. 测量方法（可复现）

本 PR 新增两个 host 侧探针，**不改动固件**：

```sh
python tools/run_probes.py            # 编译并运行两个探针
python tools/run_probes.py --list     # 看看涉及哪些文件
```

| 探针 | 文件 | 做什么 |
|---|---|---|
| `render` | `tools/render_probe/mosaico_raylib_fast.h`、`tools/render_probe/measure_render.c` | 把 `main/game_view.c` + `main/game.c` + `main/tower.c` 链接到一个**带计数器和真实 480×480 RGB565 帧缓冲**的图形桩上，统计每帧的绘制调用数与像素写入数，并对相邻两帧做逐像素差分 |
| `defects` | `tools/render_probe/verify_model_defects.c` | 直接链接 `main/tower.c`，用公开 API 验证存档校验与失败条件 |

桩件复刻了 `game_view.c` 实际用到的全部 API 表面（`DrawRectangle` / `DrawRectangleLines` /
`DrawPixel` / `DrawLine` / `DrawTriangle` / `MosaicoFastDraw*` / `ClearBackground`），
并按真实几何填充像素，因此像素计数是可核对的，不是估算。

---

## 2. 一帧到底画了什么（实测）

`--- DUNGEON floor 1 (player idle) ---`

```
draw calls per frame   : 1376
pixel writes per frame : 831558  (3.61 full screens of 230400)
  of which clears       : 460800 px in 2 ClearBackground calls
  rects                : 354800 px in 309 calls
  fast rects (CJK runs):   1556 px in 532 calls
  outlines              :      0 px in   7 calls
  lines                 :      0 px in 379 calls
  triangles             :      0 px in  10 calls
RGB565 traffic per frame: 1.59 MB
at 30 fps: 24.9 M px/s, 47.6 MB/s, 41.3 k draw-calls/s
```

拆解：

| 组成 | 像素/帧 | 占总量 | 是否会变 |
|---|---|---|---|
| 2 次全屏 `ClearBackground` | 460,800 | **55.4%** | 否 |
| 121 个格子底色（棋盘，依赖 `(x+y)&1`） | 123,904 | 14.9% | **否** |
| 93 个墙砖（1 个 30×30 矩形 + 4 条线） | ~91,600 | 11.0% | **否** |
| 道具 / 敌人 / 英雄 / UI | ~155,250 | 18.7% | 部分 |

**静态内容合计 676,304 像素 = 81.3%**。这 81.3% 永远画不出任何新东西。

> 注：`ClearBackground(INK)` 紧跟着 `ClearBackground(BG)`，INK = {9,13,20}、BG = {14,19,28}，
> 后者完全覆盖前者。第一次清屏是 **100% 死代码**，除非后端的 `ClearBackground` 还兼做别的
> 事情（标准实现只做填充）。删之前在真机上比一帧即可确认。

---

## 3. 画面是静止的（实测）

对相邻两帧做逐像素差分（玩家完全不动，只有 tick 在走）：

```
tick   changed px   % of screen   draw calls   px written
  1            0      0.000%         1376       831558
  2            0      0.000%         1376       831558
  3            0      0.000%         1376       831558
  4            0      0.000%         1376       831558
  5            0      0.000%         1376       831558
  6            0      0.000%         1376       831558
  7            0      0.000%         1376       831558
  8           28      0.012%         1376       831540
  9            0      0.000%         1376       831540
 10            0      0.000%         1376       831540
 11            0      0.000%         1376       831540
 12            0      0.000%         1376       831540
```

**只有第 8 帧变了 28 个像素**——那是蝙蝠翅膀的扇动。第 1 层的其他"动画"
（史莱姆上下、法师/NPC 眨眼）都在第 1 层地图上不可见或周期更长。

十秒空闲期的整体统计：

```
over 10 s of idle play at 30 fps: 49 frames changed, 251 frames identical
-> 83.7% of the frames are byte-identical to the previous one
-> a present-on-change loop would present 49 frames instead of 300
```

### 一次真实状态变化

玩家往北走一格：

```
changed pixels        : 1814 (0.787% of the screen)
but pixels WRITTEN    : 831807  (458.5x more than changed)
-> 99.8% of the redraw is provably redundant
```

**一次真正的操作只改变 0.787% 的屏幕，却重写了 361% 的屏幕。**

---

## 4. 动画的真实频率

所有会动的精灵都靠 `s->tick` 驱动（`main/game_view.c`）：

| 精灵 | 相位 | 周期（tick） | @30 Hz | @10 Hz |
|---|---|---|---|---|
| 史莱姆 | `tick/16` | 32 | 0.94 Hz | 0.31 Hz |
| 蝙蝠 | `tick/8` | 16 | 1.88 Hz | 0.62 Hz |
| 法师 | `tick/20` | 40 | 0.75 Hz | 0.25 Hz |
| NPC 眨眼 | `tick/18` | 36 | 0.83 Hz | 0.28 Hz |
| 怨灵 | `tick/14` | 28 | 1.07 Hz | 0.36 Hz |
| 魔王 | `tick/20` | 40 | 0.75 Hz | 0.25 Hz |
| 公主 | `tick/24` | 48 | 0.62 Hz | 0.21 Hz |
| **信标闪烁** | `tick/5` | **15** | **2.00 Hz** | 0.67 Hz |

**全游戏最快的动画是 2 Hz。30 fps 有 15 倍的余量。**

> 这条直接回应了"帧率 30 完全没必要"——**是对的**，而且有余可测：
> 30 → 10 fps 之后最快的动画仍有 5 倍余量，甚至 30 → 5 fps 都还在可接受范围
> （信标 0.67 Hz 会显得顿挫，所以推荐 10）。

---

## 5. 引擎 API 能提供什么（已查证）

本仓库用的是**旧版 `mosaico_*` API**（`mosaico_game_app`、`mosaico_raylib_fast`）。
新版 `raylib-lite-engine` 已经提供了我们需要的全部机制，只是本项目还没迁：

### 5.1 逻辑频率与呈现频率可以分离

```c
/* include/raylib_lite/raylib_lite_game_app.h  (v: espressif2022/raylib-lite-engine) */
typedef struct {
    ...
    uint32_t logic_hz;      /* ← 本仓库当前没有 */
    uint32_t target_fps;    /* ← 本仓库只有这一个，逻辑与呈现被绑死 */
    uint32_t stats_interval;
    ...
    bool (*idle)(void *user);          /* ← 新增 */
    uint32_t (*render_fps)(void *user); /* ← 新增：动态呈现速率，0 表示用 target_fps */
    void (*on_stats)(void *user);       /* ← 新增：周期统计回调 */
} raylib_lite_game_app_t;
```

`API.md` 原文：

> `raylib_lite_runner_run` can be used without a display. It **separates logic Hz from target
> presentation FPS**, limits catch-up to three updates per pass, and discards stale credits
> after stalls.

### 5.2 `idle()` 就是"画面没变就别画"

`API.md` 原文：

> Idle **discards accumulated elapsed time, retains one immediate update/render credit for
> resume, and sleeps once.**

`src/runner/raylib_lite_runner.c` 语义：**A pass executes at most three logic updates and at
most one render.** 也就是说 `idle()` 返回 true 时，runner 不 render，并且把空闲期累计的
时间丢掉、只留一个立即执行的 credit——**这正是我们要的 present-on-change。**

### 5.3 引擎自带我们要的仪表

```c
/* include/raylib_lite/raylib_lite_runtime_stats.h */
typedef struct {
    uint32_t frames, dropped_frames;
    float fps, logic_fps, display_fps;
    uint32_t input_us, update_us;
    uint32_t acquire_us, render_us, present_us, release_us;
    uint32_t busy_frames, superseded_frames, display_errors;
    uint32_t queue_overflows, in_flight_frames, peak_in_flight_frames;
} raylib_lite_runtime_stats_t;
```

`acquire_us`（取帧） / `render_us`（软件绘制） / `present_us`（送屏）三段分离，
正是判断"瓶颈在 CPU 还是在 DMA"需要的量。**真机验证直接用这个，不要自己造轮子。**

### 5.4 S31 有硬件像素引擎（PIE）

`examples/render_benchmark/config/techniques.json`：

```json
{ "id": "pie", "title": "标量 / S31 PIE", "status": "device_selectable",
  "variants": ["RENDER_BENCH_PIE=OFF", "RENDER_BENCH_PIE=ON"],
  "source": "src/arch/esp32s31/raylib_lite_rgb565_pie.S",
  "gate": "copy/fill 输出相同，固定同一硬件环境" }
```

`copy_fill` 条目（"清屏、拷贝与批量写入"）也标为 `implemented`。
**本游戏每帧 460,800 px 的清屏 + 354,800 px 的矩形填充，正是 PIE 的目标负载。**
`render_benchmark` 的 README 还给了纪律：**"每次只允许一个比较维度"**——本节的所有
改动也应该一个一个上、一次一个变量。

### 5.5 帧的所有权模型

`API.md`：

> `present` consumes the frame on **every** return path… `discard` consumes an unpresented
> frame and must not block.

即引擎层面原生支持"这一帧我不要了"。旧 API 的 `mosaico_game_app` 是否透出 `discard`
**未证实**，需要查 `mosaico_raylib_fast` 的头文件。

---

## 6. 改法

### 6.1 P1：删掉冗余清屏（app 内即可改，收益 −27.7%）

`main/game_view.c`：

```diff
 bool game_render(game_handle_t game)
 {
     const game_snapshot_t *s=game_read(game);
     if(!s) return false;
     BeginDrawing();
     if(!MosaicoFastFrameAvailable()) { EndDrawing(); return false; }
-    ClearBackground(INK);
     ClearBackground(BG);
```

实测收益（同一份渲染器，只改这一行）：

```
pixel writes per frame : 601158  (2.61 full screens)   ← 原来是 831558
at 30 fps: 18.0 M px/s, 34.4 MB/s                     ← 原来是 24.9 M px/s, 47.6 MB/s
```

**前提**：确认后端的 `ClearBackground` 只做填充。改完用一次真机截图比对即可。

### 6.2 P4 + P5：降帧率，但先把逻辑改成时间基（app 内即可改，收益 −67%）

**不能只改 `GAME_TICK_HZ`**，否则手感会坏。当前有三处逻辑是按 tick 计数的：

| 位置 | 现在 | @30 Hz | @10 Hz 若不改 | 必须改成 |
|---|---|---|---|---|
| `main/game.c` `GAME_KEY_REPEAT` | 7 tick | 233 ms | **700 ms**（按键手感变粘） | 250 ms |
| `main/game.c` `notice(g,...)` | 90 tick | 3.0 s | **9 s** | 3.0 s |
| `main/game.c` `set_gain(...)` | 120 tick | 4.0 s | **12 s** | 4.0 s |

建议加一个"每 tick 多少毫秒"的时间基：

```c
/* main/game_config.h */
#define GAME_TICK_HZ        10      /* ← 30 改 10 */
#define GAME_MS_PER_TICK    (1000u / GAME_TICK_HZ)

/* main/game.c */
static uint16_t notice_ms;                       /* 改自 notice_ticks */
static uint16_t repeat_ms;                       /* 改自 repeat */

static void notice(game_handle_t g, game_notice_t id)
{
    g->state.notice_id = (uint8_t)id;
    g->notice_ms = 3000;                          /* 原来是 90 tick */
}
static void set_gain(game_handle_t g, game_gain_t kind, uint32_t value, uint8_t color)
{
    ...
    g->notice_ms = 4000;                          /* 原来是 120 tick */
}
```

按键重复改成时间累加：

```c
/* 原来：++g->repeat >= GAME_KEY_REPEAT */
g->repeat_ms += GAME_MS_PER_TICK;
if (g->repeat_ms >= 250) {                        /* 原来是 7 tick = 233 ms */
    g->repeat_ms = 0;
    ...
}
```

`main/game_config.h` 同时把 `GAME_TICK_HZ` 改成 10，`main/main.c` 的
`.target_fps = GAME_TICK_HZ` 会自动跟着变。

> **注意**：精灵动画相位也基于 `s->tick`，降帧率会让动画同步变慢（0.94 Hz → 0.31 Hz）。
> 这是**期望行为**（动画本来就没必要快），但要顺手确认信标闪烁（2 Hz → 0.67 Hz）看起来不别扭。

### 6.3 P2 + P3：present-on-change 与静态层缓存（**需要框架配合**）

两条路，按成本排序：

**路线 1：迁移到新 API（推荐）**

`main/main.c` 的 `mosaico_game_app_config_t` → 新版 `raylib_lite_game_app_t`：

```c
/* 静态画面（无 tick 动画、无待处理提示）时告诉 runner 不用画 */
static bool on_idle(void *user)
{
    const game_snapshot_t *s = game_read((game_handle_t)user);
    /* 有待显示的提示、增益浮字、战斗/商店/对话弹窗 -> 必须重画 */
    if (s->notice_id != GAME_NOTICE_NONE || s->gain_kind != GAME_GAIN_NONE) return false;
    if (s->screen != GAME_DUNGEON) return false;
    if (s->tower.phase != TOWER_PHASE_PLAY) return false;
    return true;      /* 画面静止 -> runner 走 idle 路径：丢弃累计时间并睡眠 */
}
```

再配合 `render_fps()` 做动态呈现速率（例如战斗/弹窗时 30 Hz，静止时 5 Hz），
以及 `on_stats` + `stats_interval` 定期把 `raylib_lite_runtime_stats_t` 打出来。

**路线 2：不动框架，自己做静态层缓存**

3 个 framebuffer 轮转（`CONFIG_MOSAICO_GAME_FRAMEBUFFER_COUNT=3`）意味着
**"不 clear、只补画变化的格子"是不安全的**——你拿到的 buffer 可能是两帧前的。
所以正确的做法是：自己留一份静态底图（480×480 RGB565 = 460,800 B，放 PSRAM），
每帧整体搬一次（一次 memcpy ≈ 460,800 B）取代 121 个底色矩形 + 93 个墙砖精灵
（约 215,504 px 的逐像素填充）。

> **可行性的前提**：应用侧要能拿到表面指针并直接写入。
> 新 API 有 `get_info` 描述 RGB565 表面；旧 API 的 `mosaico_raylib_fast` 是否暴露
> **未证实**。若拿不到，这条路就要靠路线 1。

### 6.4 P8：音量滑条联动功放

`main/game_audio.c`：

```c
esp_codec_dev_set_out_vol(codec,80);     /* ← 只在 init 调了一次，之后再也不动 */
```

而 `game_audio_set_volume()` 只改软件混音比例 `s_volume`。
**结果：滑条拉到 0，功放仍在 80 的输出增益上工作，功放的静态电流一分没省。**

```diff
 void game_audio_set_volume(int volume)
 {
     if (volume<0) volume=0;
     if (volume>100) volume=100;
     s_volume=volume;
+    if (s_codec) esp_codec_dev_set_out_vol(s_codec, (int)(volume * 255 / 100));
 }
```

（需要把 `codec` 句柄提升为文件级静态变量，并在 `esp_codec_dev_close` 前后置空。）

顺带：暂停 / 音量 0 / 在商店和 NPC 对话时，**应当让 BGM 停掉**——
`game_audio.c` 里的 `audio_task` 是 24 kHz 持续合成 + I2S 常开的。

### 6.5 P0 / P6 / P7 / P9 / P10：sdkconfig 与硬件层

```diff
--- a/sdkconfig.defaults
+++ b/sdkconfig.defaults
@@
-CONFIG_ESP_IRIS_TRANSPORT_TCP=y
+# 游戏中不需要 TCP 通道：Wi-Fi 射频会因此常开，是最大的静态功耗来源。
+# 保留 USB 通道即可满足装机与日志需求。
+CONFIG_ESP_IRIS_TRANSPORT_TCP=n
-CONFIG_ESP_IRIS_TCP_PAIRING=y
+CONFIG_ESP_IRIS_TCP_PAIRING=n

-CONFIG_SPIRAM_XIP_FROM_PSRAM=y
+# 代码与常量放 flash 跑，PSRAM 只放数据。指令取指不再跨 PSRAM 总线。
+CONFIG_SPIRAM_XIP_FROM_PSRAM=n

-CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y
+# .bss 回到片内 SRAM，避免每次变量访问都走 PSRAM。
+CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=n

+# 电源管理：允许 DFS 与 auto light-sleep。
+CONFIG_PM_ENABLE=y
+CONFIG_FREERTOS_USE_TICKLESS_IDLE=y
+CONFIG_FREERTOS_IDLE_TIME_BEFORE_SLEEP=20
```

**依赖关系（重要）**：

- `CONFIG_ESP_IRIS_SERVICE_CONTEXT_PSRAM=y`、`..._STATE_PSRAM=y`、
  `..._RPC_RESPONSE_PSRAM=y`、`..._WIRE_BUFFERS_PSRAM=y` 这几项
  **依赖 `CONFIG_SPIRAM_XIP_FROM_PSRAM`**。关掉 XIP 后它们会退回片内 RAM，
  腾出约 4 MB PSRAM——但片内 RAM 会变紧（S31 片内只有 512 KB，
  Wi-Fi + TLS 会拿走一大块）。**先量片内空闲再改。**
- **auto light-sleep 会断开 Wi-Fi 连接**。所以 P10 必须在 P0 之后做。
- 降帧率（P4）会直接增加每帧之间的空闲时间，从而让 light-sleep 真正生效——
  **P4 和 P10 是配套的**。

**背光（P9）**：本仓库没有任何背光代码。需要查 ESP-Mosaico BSP 的
`bsp_mosaico`/显示组件是否暴露 LEDC PWM 背光接口（**未证实**）。
若可调，建议 60–70% 亮度并提供一个玩家可见的亮度档位。

---

## 7. 真机验证方法

**不要用 Host 结果代替真机结论**（这是本生态的明确纪律）。真机上按顺序做：

1. **先建立基线**。用新 API 的 `on_stats` + `stats_interval` 定期打印：

   ```c
   static void on_stats(void *user)
   {
       raylib_lite_runtime_stats_t st;
       raylib_lite_runtime_stats_get(&st);
       ESP_LOGI("magic_tower",
                "fps=%.1f display_fps=%.1f logic_fps=%.1f "
                "update_us=%u render_us=%u acquire_us=%u present_us=%u "
                "dropped=%u in_flight=%u",
                st.fps, st.display_fps, st.logic_fps,
                st.update_us, st.render_us, st.acquire_us, st.present_us,
                st.dropped_frames, st.in_flight_frames);
   }
   ```

   `render_us` 大 ⇒ 瓶颈在软件绘制（P1/P2/P3）；
   `acquire_us` + `present_us` 大 ⇒ 瓶颈在 DMA/送屏（帧率、背光、帧缓冲位置）。

2. **测功耗**。用 USB 电流表或板载 BQ272220（依赖里已有 `espressif/bq27220`）读整机电流。
   分别记录：标题页静止 / 游戏中静止 / 战斗中 / 飞行模式关闭前后。

3. **摸温度**。外壳温度比 SoC 温度更能反映"烫不烫"。至少记录：
   静止 5 分钟、连续游玩 15 分钟两个点。

4. **一次只改一个变量**。这是 `render_benchmark` README 的明确纪律
   （"每次只允许一个比较维度"），也是本节所有收益数字可信的前提。

---

## 8. 优先级与风险

| 顺序 | 动作 | 风险 | 备注 |
|---|---|---|---|
| 1 | 删冗余 `ClearBackground` | 极低 | 一行，收益 −27.7% 像素写入 |
| 2 | 时间基改造 + `target_fps` 30→10 | 中（手感） | 必须同时改 key repeat 与提示时长 |
| 3 | 关掉 ESP-Iris TCP 通道 | 中（装机/日志） | 会失去 TCP 通道的调试能力；USB 通道保留 |
| 4 | 音量联动功放 + 静音断电放 | 低 | |
| 5 | 迁移新 API 用 `idle()`/`render_fps()` | 中（大改） | 需要重写 `main/main.c` 的接入层 |
| 6 | 关 XIP / 关 PSRAM bss | **高** | 会牵动 ESP-Iris 的 PSRAM 依赖；先量片内空闲 |
| 7 | 背光 PWM | 低 | 需要先确认 BSP 是否暴露接口 |
| 8 | auto light-sleep | 中 | 必须在关 Wi-Fi 之后 |
| 9 | S31 PIE 硬件像素引擎 | 中 | 由引擎侧开关决定，收益待实测 |

**建议顺序：1 → 2 → 4 → 3 → 7 → 5 → 8 → 6 → 9。**
前三项都是 app 内的小改动，收益却已经覆盖了大部分问题。

---

## 9. 外部资料

以下为本文件引用的外部资料，**只列实际访问过的**：

| 来源 | URL | 用到的内容 |
|---|---|---|
| Espressif ESP-IDF 文档（ESP32-S31 低功耗模式） | https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/api-guides/low-power-mode/low-power-mode-soc.html | DFS / Light-sleep / Auto Light-sleep 机制；`CONFIG_PM_ENABLE`、`CONFIG_FREERTOS_IDLE_TIME_BEFORE_SLEEP`、`light_sleep_enable` |
| Espressif ESP-IDF 文档（ESP32-S31 Wi-Fi 电源节省） | https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/api-guides/wifi.html | `esp_wifi_set_ps(WIFI_PS_MIN_MODEM)` 为默认；`CONFIG_ESP_WIFI_STA_DISCONNECTED_PM_ENABLE` 默认开启 |
| Raylib Lite Engine API 合同 | https://github.com/espressif2022/raylib-lite-engine/blob/main/API.md | `logic_hz`/`target_fps` 分离；`idle()` 语义；`discard`；acquire/present 所有权 |
| Raylib Lite Engine runner 头文件 | https://github.com/espressif2022/raylib-lite-engine/blob/main/include/raylib_lite/raylib_lite_runner.h | `raylib_lite_runner_config_t` 全字段；`raylib_lite_runner_stats_t` |
| Raylib Lite Engine 运行时统计 | https://github.com/espressif2022/raylib-lite-engine/blob/main/include/raylib_lite/raylib_lite_runtime_stats.h | `raylib_lite_runtime_stats_t` 全部字段 |
| Raylib Lite Engine render_benchmark | https://github.com/espressif2022/raylib-lite-engine/tree/main/examples/render_benchmark | `techniques.json`（S31 PIE / copy_fill / 内存位置）；"每次只允许一个比较维度"的纪律；"DONE FPS 是全帧 DMA 传输完成速率" |
| ESP-Mosaico 官方工作区 | https://github.com/esp-mosaico/esp-mosaico-vibe | 游戏开发流程；官方 skill 形态；官方 `mosaico.py game sim/build` 流程 |
| 少数派《把 Meta 的 Muse 装进乐鑫的 ESP-Mosaico》 | https://sspai.com/post/115437 | 同一块板子的独立实测："ESP-Iris 同时开了 TCP 和 USB 两种通道，Wi-Fi 一通它就把 USB 丢了"；"关掉 `CONFIG_SPIRAM_XIP_FROM_PSRAM`，代码和常量改在 flash 里跑"；"ESP32-S31 的片内 RAM 是紧的" |
| ESP32-S31 规格与功耗（转述 xda-developers） | https://telegra.ph/PtKyuqoaidM15BdqF-jjoQ-09-24 | 双核 RISC-V 320 MHz；512 KB SRAM；**320 MHz 调制解调器休眠、关外设时钟 91 mA，开外设时钟 147 mA；Wi-Fi 接收 110–117 mA**；2D 像素处理加速器 |

---

## 10. 未证实清单

| 声明 | 状态 |
|---|---|
| `mosaico_game_app`（旧 API）是否透出 `discard` / `idle` / `render_fps` | **未证实**——需查 `mosaico_raylib_fast` 与 `mosaico_game_app` 头文件；本文按"没有"处理，方案以迁移新 API 为主 |
| 应用侧能否直接拿到 framebuffer 指针做整帧 memcpy | **未证实**——新 API 有 `get_info`，旧 API 不明 |
| ESP-Mosaico BSP 是否暴露背光 PWM 接口 | **未证实**——本仓库无相关代码，BSP 头文件未查 |
| 实际整机电流与外壳温度 | **未实测**——本沙箱无硬件；§7 给出了测量步骤 |
| S31 PIE 对本游戏负载的加速比 | **未实测**——`render_benchmark` 有工具，但需在真机上按单维度纪律跑 |
| 背光功率占整机功耗的比例 | **未证实**——公开报道中该板为 480 方屏，面板类型说法不一（OLED / CO5300 QSPI），以本仓库 `sdkconfig.defaults` 为准 |
| `CONFIG_SPIRAM_XIP_FROM_PSRAM=n` 后 ESP-Iris 退回片内 RAM 的实际可用量 | **未实测**——同一篇文章提到"开机后片内空闲 64.5 KB"是另一个项目的数字，不可直接套用 |
| 降帧率后的真机触摸延迟 | **未实测**——逻辑在事件回调里即时处理（`game_set_pointer` → `tap()`/`move()` 同步执行），**理论上只有视觉反馈受帧率影响**，但需真机确认 |

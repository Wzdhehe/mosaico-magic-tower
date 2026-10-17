# 性能与发热分析（ESP-Mosaico / Magic Tower）

> **渲染侧的数字全部来自本仓库的真实源码**：`main/game_view.c` 的渲染器被链接到一个带计数器和
> 480×480 RGB565 帧缓冲的桩件上，逐帧统计绘制调用与像素写入，再对相邻两帧做逐像素差分。
> 复现方式见 §1。
> **硬件与框架侧的数字来自官方文档与组件源码**，逐条标注来源与置信度。
> 凡是推断而非实测的地方都标了「推断」；凡是外部资料没有亲自读到的都标了「未证实」。

---

## 0. 先纠正三个前提（否则结论会错）

| # | 常见假设 | 实际情况 | 来源 |
|---|---|---|---|
| 1 | "480×480 CO5300 **LCD**，可以用背光 PWM 省电" | **CO5300 是 AMOLED 驱动 IC，没有背光。** Mosaico 的屏只有 `VCC_3V3=GPIO60` 屏供电使能脚和 `TE=GPIO38`，**没有任何背光 GPIO/LEDC**。省电杠杆是 **AMOLED 亮度（`0x51`）+ 深色内容 + 断屏供电轨**，不是背光占空比 | `esp_lcd_co5300` v2.2.0 源码；`esp-gsp` examples/scenarios/thermostat 的 Mosaico 参考实现 `hw_lcd_qspi.c` |
| 2 | "ESP32-S31 ≈ ESP32-S3，240 MHz" | **S31 是双核 32-bit RISC-V，最高 320 MHz**（与 P4 同源），带 **PPA 2D 像素加速器 + 2D-DMA**、真 MMU(Sv32)、WiFi 6。IDF 里 `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ` 默认 **320**（可选 40/240/320） | S31 datasheet v0.5；`esp32s31/Kconfig.cpu` |
| 3 | "30 fps 只是软件帧率" | **送屏本身才是大头**：TE_SYNC 模式下每帧固定做 460,800 B cache writeback + 未渲染区 PSRAM→PSRAM 拷贝 + 460,800 B PSRAM→QSPI DMA，**与画面内容无关**。而且当前 40 MHz QSPI 下单帧传输 **23.04 ms > 60 Hz TE 周期 16.67 ms**，已经落进 `SLOW_END` 策略——**当前配置下画面其实是在撕裂的** | `esp_display_present` v1.0.2 源码 `present_te_transport.c` / `present_te.c` |

---

## 1. 结论速览

| # | 问题 | 证据 | 修法 | 预期收益 |
|---|---|---|---|---|
| **P0** | **WiFi 射频常驻**，且**持 `ESP_PM_APB_FREQ_MAX` 锁 → 自动 light-sleep 永不触发** | `CONFIG_ESP_IRIS_TRANSPORT_TCP=y` + `CONFIG_ESP_IRIS_TCP_PAIRING=y`；IDF 文档：WiFi 驱动全程持 APB 锁 | 关掉 ESP-Iris 的 TCP 通道 | S31 datasheet：240 MHz 双核 CoreMark 外设开 **126 mA** vs light-sleep **1 mA**。**解锁下面一半的措施的前提** |
| **P1** | **每帧两次全屏 `ClearBackground`**，第一次被第二次完全覆盖 | 实测 460,800 px/帧，占全部像素写入 **55.4%** | 删掉 `ClearBackground(INK)` | **像素写入 −27.7%** |
| **P2** | **30 fps 全屏重绘，但画面在两次输入之间逐字节相同** | 实测 10 秒空闲期 **251/300 帧与上一帧完全相同** | present-on-change（TE_SYNC **没有**内置跳帧，必须主循环决定不 `begin`/`commit`） | **空闲时 −83.7%** |
| **P3** | **GSP 服务循环默认 100 Hz**（`CONFIG_ESP_GSP_ACTIVE_TICK_MS=10`），而游戏只有 30 fps | GSP Kconfig 默认值 | 改成 33 | 唤醒/调度次数 ~3× 降低 |
| **P4** | **QSPI 40 MHz → 单帧 23.04 ms > TE 周期 16.67 ms → 撕裂** | `present_te.c` 的 `te_estimate_transfer_time_us` 算式 | `io_config.pclk_hz` 提到 **80 MHz** | 单帧传输 **23.04 → 11.52 ms**；**消除撕裂**；腾出一半帧预算 |
| **P5** | **`trans_queue_depth = 10`，低于 README 要求的 16** | `esp_display_present` README：460,800 B 帧 ÷ 32 KiB chunk = 15 块，队列深度需 ≥16 | 改成 16 | 消除管线塌缩 |
| **P6** | **静态层每帧重画**：棋盘底 + 93 个墙 | 约 215,000 px/帧 | **GSP 官方 damage API**：`esp_gsp_canvas_push_dirty()` / `esp_gsp_canvas_invalidate_dirty()` / `esp_gsp_canvas_submit_frame()` | −26%（实测 99.8% 的重绘冗余） |
| **P7** | **帧率 30 远超内容所需**：最快动画 2 Hz | 精灵 bob 周期 15–48 tick | `target_fps` 30 → 10 | CPU 与 QSPI 流量 **−67%** |
| **P8** | **逻辑绑在 tick 上**：key repeat、提示时长都是 tick 计数 | `GAME_KEY_REPEAT=7`、`notice_ticks=90/120` | 改时间基 | 降帧率的前提，否则手感会变 |
| **P9** | **AMOLED 开机即 100% 亮度**（`{0x51,0xFF}`、`{0x63,0xFF}`） | Mosaico 参考实现的 init 表 | 降到 50–60%；配色偏深 | AMOLED 电流与亮度近线性、与黑像素数强相关（幅度未证实） |
| **P10** | **CPU 320 MHz 常开** | `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ` 默认 320 | DFS 降到 240 | datasheet Table 5-9：满载 **147 → 126 mA（−21 mA，−14%）** |
| **P11** | **代码从 PSRAM 执行** | `CONFIG_SPIRAM_XIP_FROM_PSRAM=y`（它还会 `SPIRAM_FLASH_LOAD_TO_PSRAM`，启动时把整个固件搬进 PSRAM） | 关闭（**必须 A/B**） | 取指不再跨 PSRAM 总线；本游戏每帧在搬 >32 KB 的块，正是 cache 最容易 miss 的场景 |
| **P12** | **`.bss` 放 PSRAM ⇒ 整个 WiFi 协议栈的 .bss 进 PSRAM** | `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y`；`external-ram.rst` 明确点名 lwIP/net80211/wpa_supplicant/bluedroid | 关闭（**必须 A/B**） | 建议作为第一个做的 A/B 项 |
| **P13** | **音量滑条不改功放音量** | `esp_codec_dev_set_out_vol(codec, 80)` 只在 init 调一次 | 滑条联动功放；静音时断电放 | 功放静态电流 |
| **P14** | **不要初始化 BT controller** | 非 esp32 目标上 BT 全程持 `ESP_PM_NO_LIGHT_SLEEP` | 不 init | 解锁 light-sleep |
| **P15** | **`CONFIG_ESP_SLEEP_FLASH_LEAKAGE_WORKAROUND` 已是默认 y** | IDF Kconfig | 保持别关 | 睡眠时 flash 漏电 −10 µA 量级，白拿 |

> **最重要的两条**：P0（WiFi 锁死 light-sleep）和 P2（83.7% 的帧是重复的）。
> 其余都是在这两条之上的优化。

---

## 2. 测量方法（可复现）

两个 host 侧探针，**不改动固件、不需要 ESP-IDF**：

```sh
python tools/run_probes.py            # 编译并运行两个探针
python tools/run_probes.py --list     # 看看涉及哪些文件
```

| 探针 | 文件 | 做什么 |
|---|---|---|
| `render` | `tools/render_probe/mosaico_raylib_fast.h`、`tools/render_probe/measure_render.c` | 把 `main/game_view.c` + `main/game.c` + `main/tower.c` 链接到一个**带计数器和真实 480×480 RGB565 帧缓冲**的图形桩上，统计每帧的绘制调用数与像素写入数，并对相邻两帧做逐像素差分 |
| `defects` | `tools/render_probe/verify_model_defects.c` | 直接链接 `main/tower.c`，用公开 API 验证存档校验与失败条件 |

桩件复刻了 `game_view.c` 实际用到的全部 API 表面，并按真实几何填充像素，
因此像素计数是可核对的，不是估算。

**真机侧用官方诊断接口**（`esp-gsp` 的 `esp_gsp_debug.h`，已证实存在）：

```c
esp_gsp_region_stats(gsp, &st);   /* input_pixels vs output_pixels -> 量化 damage 空间 */
esp_gsp_render_phases(gsp, &render_us, &submit_us);  /* 分离"画"和"送" */
esp_gsp_service_stats(gsp, &iters, &service_us, &cmds);
lcd.perf_log = true;              /* 每 5 s 打一次 fps */
```

> **`input_pixels` vs `output_pixels` 是 P6 的直接判据**：
> 如果 output/input 接近 1，说明 damage 路径已经在生效；
> 如果接近 3.6（= 831,558 / 230,400），说明每帧都在整幅重画。

---

## 3. 一帧到底画了什么（实测）

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

| 组成 | 像素/帧 | 占总量 | 会变吗 |
|---|---|---|---|
| 2 次全屏 `ClearBackground` | 460,800 | **55.4%** | 否 |
| 121 个格子底色（棋盘，依赖 `(x+y)&1`） | 123,904 | 14.9% | **否** |
| 93 个墙砖（1 个 30×30 矩形 + 4 条线） | ~91,600 | 11.0% | **否** |
| 道具 / 敌人 / 英雄 / UI | ~155,250 | 18.7% | 部分 |

**静态内容合计 676,304 像素 = 81.3%**，永远画不出新东西。

> `ClearBackground(INK)` 紧跟着 `ClearBackground(BG)`，INK={9,13,20}、BG={14,19,28}，
> 后者完全覆盖前者。**第一次清屏是 100% 死代码**（除非后端的 `ClearBackground` 还兼做别的
> 事情，标准实现只做填充）。删之前在真机上比一帧即可确认。

---

## 4. 画面是静止的（实测）

```
tick   changed px   % of screen   draw calls   px written
  1            0      0.000%         1376       831558
  ...
  8           28      0.012%         1376       831540     <- 蝙蝠翅膀扇动
  9            0      0.000%         1376       831540
  ...
```

十秒空闲期整体统计：

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

## 5. 动画的真实频率

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

**全游戏最快的动画是 2 Hz。30 fps 有 15 倍余量。**
30 → 10 fps 之后最快的动画仍有 5 倍余量（信标会掉到 0.67 Hz，略显顿挫）。

---

## 6. 送屏才是真正贵的地方

### 6.1 TE_SYNC 每帧的固定成本（`esp_display_present` v1.0.2 源码）

配置解析：`CO5300(GRAM) + TE` ⇒ **`ESP_DISPLAY_PRESENT_MODE_TE_SYNC`**，
`frame_buffer_count = 0`（GRAM 不用 framebuffer），契约 = **PARTITION**。

`te_compose_buffers = 2` 实际分配：

```
draw_bytes = stride_bytes * height = 480*2*480 = 460800
for (i < te_compose_buffers)   // = 2
    heap_caps_malloc(draw_bytes, MALLOC_CAP_SPIRAM|MALLOC_CAP_DMA|...|CACHE_ALIGNED)
```

⇒ **921,600 字节 PSRAM 被 compose buffer 永久占用。**

每帧 commit 的确切序列（`present_te_transport.c::submit_te_pipeline`）：

```
present_transfer_wait_idle(...)                 // 等上一帧 DMA 完成
te_wait_submit_window(...)                      // 等 TE 沿
esp_display_present_cache_msync_framebuffer()   // 整帧 460800 B cache writeback
tracker_submit_transfer_ticket()
present_te_pool_commit(...)
esp_display_present_blit_area(panel,0,0,480,480,pixels)  // == esp_lcd_panel_draw_bitmap
present_transfer_wait_idle(...)                 // 再等本次完成
present_te_pool_acquire_next(...)
```

外加 `present_te_compose_repair()` 会把**未渲染区域**从上一个 display buffer
**PSRAM→PSRAM 拷贝**过来。

**关键：这些成本与画面内容无关。** 画面没变，这 23.04 ms 的 QSPI 推送 +
460,800 B writeback + repair 拷贝照烧不误。

### 6.2 当前 QSPI 配置已经在撕裂

`present_te.c` 的调度判定：

```c
estimated_tx = transfer_bytes*8 / (bus_freq_hz * data_lines)
if (transfer <= active_scan)             → FAST_START
else if (transfer <= frame_period)       → LEAD_START
else if (transfer <  period+active_scan) → SLOW_END   // 起点=blanking 结束沿，允许跨帧
else                                     → OVERRUN     // "no safe full-frame window"
```

代入本配置（QSPI 40 MHz × 4 线）：

```
transfer = 460800*8 / (40e6*4) = 23,040 µs
60 Hz TE period               = 16,667 µs
23,040 > 16,667  ⇒ 落进 SLOW_END  ⇒ 允许跨 TE 周期推送 ⇒ 画面会撕裂
```

`esp_display_present` README 也明确警告：

> "If draw_bitmap() takes approximately the full wire-transfer time, the transaction queue is
> too shallow … the render/transfer pipeline collapses to serial execution."

### 6.3 30 fps 预算

```
30 fps 预算 = 33.3 ms
  40 MHz: 23.04 ms 占 69%，仅余 10.3 ms 给 渲染 + msync + repair + 同步
  80 MHz: 11.52 ms 占 35%，余 21.8 ms
```

---

## 7. 引擎 API 能提供什么（已查证）

本仓库用的是**旧版 `mosaico_*` API**（`mosaico_game_app`、`mosaico_raylib_fast`）。
新版 `raylib-lite-engine` 已经提供了我们需要的全部机制，只是本项目还没迁。

### 7.1 逻辑频率与呈现频率可以分离

```c
/* include/raylib_lite/raylib_lite_game_app.h */
typedef struct {
    ...
    uint32_t logic_hz;      /* ← 本仓库当前没有 */
    uint32_t target_fps;    /* ← 本仓库只有这一个，逻辑与呈现被绑死 */
    uint32_t stats_interval;
    bool (*idle)(void *user);          /* ← 新增 */
    uint32_t (*render_fps)(void *user); /* ← 新增：动态呈现速率，0 表示用 target_fps */
    void (*on_stats)(void *user);       /* ← 新增：周期统计回调 */
} raylib_lite_game_app_t;
```

`API.md` 原文：

> `raylib_lite_runner_run` can be used without a display. It **separates logic Hz from target
> presentation FPS**, limits catch-up to three updates per pass, and discards stale credits
> after stalls.

### 7.2 `idle()` 就是"画面没变就别画"

`API.md`：

> Idle **discards accumulated elapsed time, retains one immediate update/render credit for
> resume, and sleeps once.**

`src/runner/raylib_lite_runner.c` 语义：**A pass executes at most three logic updates and at
most one render.** `idle()` 返回 true 时 runner 不 render，并丢掉空闲期累计的时间。

### 7.3 GSP 官方的 damage 路径

`esp-gsp` 的 `esp_gsp.h` 提供：

- `esp_gsp_canvas_push_dirty(gsp, bind, pixels, stride, rect, on_release, ctx)`
- `esp_gsp_canvas_invalidate_dirty(...)`
- `esp_gsp_canvas_submit_frame(...)`（带 `frame->dirty`）

> ⚠️ 语义约束（头文件原文）：`dirty` 之外的像素**必须与上一显示帧一致**，否则画面会撕裂错乱。

**这正是 P6 需要的 API**，而且是官方支持的。

### 7.4 TE_SYNC 没有内置跳帧

- `DIRECT`/`FULL` 契约：`esp_display_presenter_submit_t.coverage = ESP_DISPLAY_PRESENT_COVERAGE_AREAS`，
  头文件注释 "**No-op updates are handled before acquire/submit**"；
- `TE_SYNC`（**本项目的模式**）契约是 `PARTITION`：`esp_display_present_te_compose_commit_frame()`
  **无条件**做整帧推送，`te_submit_is_valid()` 即使 `coverage != AREAS` 也直接接受。

⇒ **唯一的省电做法是不发起帧**（不 `begin`/`commit`），这必须由游戏主循环决定。
旧 API 的 `mosaico_game_app` 是否透出 `discard` **未证实**。

### 7.5 S31 有硬件像素引擎

`esp-gsp` / `raylib-lite-engine` 的 `render_benchmark/config/techniques.json`：

```json
{ "id": "pie", "title": "标量 / S31 PIE", "status": "device_selectable",
  "variants": ["RENDER_BENCH_PIE=OFF", "RENDER_BENCH_PIE=ON"],
  "source": "src/arch/esp32s31/raylib_lite_rgb565_pie.S",
  "gate": "copy/fill 输出相同，固定同一硬件环境" }
```

`copy_fill`（"清屏、拷贝与批量写入"）也标为 `implemented`。
**本游戏每帧 460,800 px 的清屏 + 354,800 px 的矩形填充，正是 PIE 的目标负载。**
`render_benchmark` 的 README 给了纪律：**"每次只允许一个比较维度"**——本节所有改动
也应该一个一个上、一次一个变量。

### 7.6 引擎自带我们要的仪表

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

`acquire_us`（取帧）/ `render_us`（软件绘制）/ `present_us`（送屏）三段分离，
正是判断"瓶颈在 CPU 还是在 DMA"需要的量。

---

## 8. 改法

### 8.1 P1：删掉冗余清屏（app 内即可改，−27.7%）

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

### 8.2 P7 + P8：降帧率，但先把逻辑改成时间基

**不能只改 `GAME_TICK_HZ`**，否则手感会坏。三处逻辑是按 tick 计数的：

| 位置 | 现在 | @30 Hz | @10 Hz 若不改 | 改成 |
|---|---|---|---|---|
| `main/game.c` `GAME_KEY_REPEAT` | 7 tick | 233 ms | **700 ms** | 250 ms |
| `main/game.c` `notice(g,...)` | 90 tick | 3.0 s | **9 s** | 3.0 s |
| `main/game.c` `set_gain(...)` | 120 tick | 4.0 s | **12 s** | 4.0 s |

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

/* 按键重复改成时间累加 */
g->repeat_ms += GAME_MS_PER_TICK;
if (g->repeat_ms >= 250) {                        /* 原来是 7 tick = 233 ms */
    g->repeat_ms = 0;
    ...
}
```

`main/main.c` 的 `.target_fps = GAME_TICK_HZ` 会自动跟着变。

> 精灵动画相位也基于 `s->tick`，降帧率会让动画同步变慢（0.94 Hz → 0.31 Hz）。
> 这是**期望行为**，但要顺手确认信标闪烁（2 Hz → 0.67 Hz）看起来不别扭。

### 8.3 P2：present-on-change（主循环不 `begin`/`commit`）

TE_SYNC 没有内置跳帧，所以这一条必须由游戏主循环做。两种落法：

**新 API（推荐）**——用 `idle()`：

```c
/* main/main.c */
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

再配合 `render_fps()` 做动态呈现速率（战斗/弹窗 30 Hz，静止 5 Hz），
以及 `on_stats` + `stats_interval` 定期把 `raylib_lite_runtime_stats_t` 打出来。

**旧 API**——在 `on_render` 里自己判断：画面没变就**不调用绘制**，
让引擎把上一帧原样送屏（`MosaicoFastFrameAvailable()` 返回 false 时正合适）。

### 8.4 P4 + P5：QSPI 时钟与队列深度

```c
/* 屏的 io 配置 */
esp_lcd_panel_io_spi_config_t io_config = CO5300_PANEL_IO_QSPI_CONFIG(cs, NULL, NULL);
io_config.pclk_hz = 80 * 1000 * 1000;   /* ← 改这里，不是 te_sync.bus_freq_hz */
io_config.trans_queue_depth = 16;        /* ← README 对 460800 B / 32 KiB chunk 的要求 */
io_config.flags.psram_dma_direct = true;

/* 80 MHz 需要驱动能力 */
for (p in {pclk, d0, d1, d2, d3}) gpio_set_drive_capability(p, GPIO_DRIVE_CAP_3);

/* max_transfer_sz 必须 >= 整帧物理字节 */
spi_bus_config_t bus = CO5300_PANEL_BUS_QSPI_CONFIG(pclk,d0,d1,d2,d3, 480*480*2);
```

> `esp_display_present` README 明确说：
> "**`bus_freq_hz` and `data_lines` only seed the first-frame transfer estimate** before a real
> transfer duration has been observed" ⇒ **要改的是 `io_config.pclk_hz`，不是 `te_sync.bus_freq_hz`。**

⚠️ **CO5300 Quad-SPI 的最大 PCLK 未证实**（同板 ST77916 参考 profile 用 80 MHz @800×480，
说明这个板级环境可行，但面板本身必须实测）。先短时跑，抓 TE 抖动与花屏。

### 8.5 P3 + P6：GSP 节拍与 damage 路径

```diff
-CONFIG_ESP_GSP_ACTIVE_TICK_MS=10     # 默认 100 Hz 服务循环
+CONFIG_ESP_GSP_ACTIVE_TICK_MS=33     # 对齐 30 fps
+CONFIG_ESP_GSP_IDLE_POLL_MS=200       # 默认 100，idle 时更省
```

```c
/* 只把真正变化的矩形声明为 dirty */
esp_gsp_canvas_push_dirty(gsp, CANVAS_BIND,
                           frame_pixels, stride_bytes,
                           (gsp_rect_t){ x1, y1, x2, y2 },   /* 半开区间 */
                           on_release, release_ctx);
```

> ⚠️ `dirty` 之外的像素**必须与上一显示帧一致**。
> 结合 §8.2 的时间基改造：只把"这一 tick 变了什么"记下来（移动的格子、
> 变化的数字、动画中的那几个精灵），其余一律不画。

### 8.6 P13：音量滑条联动功放

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
`game_audio.c` 的 `audio_task` 是 24 kHz 持续合成 + I2S 常开的。

> ⚠️ **I2S 注意**：IDF 文档说 I2S 在 `SOC_I2S_HW_VERSION_1` 且用 APLL 时改持
> **`ESP_PM_NO_LIGHT_SLEEP`**。24 kHz BGM 任务如果配了 APLL，**会直接杀死 light sleep**。
> 请确认没走 APLL，或只在传输事务期间持锁。

### 8.7 P0 / P10 / P11 / P12 / P14 / P15：sdkconfig 与硬件层

```diff
--- a/sdkconfig.defaults
+++ b/sdkconfig.defaults
@@
-CONFIG_ESP_IRIS_TRANSPORT_TCP=y
+# 游戏中不需要 TCP 通道：WiFi 射频会因此常开，并持有 ESP_PM_APB_FREQ_MAX 锁，
+# 导致自动 light-sleep 永远不触发。这是最大的单点静态功耗来源。
+CONFIG_ESP_IRIS_TRANSPORT_TCP=n
-CONFIG_ESP_IRIS_TCP_PAIRING=y
+CONFIG_ESP_IRIS_TCP_PAIRING=n

+# 电源管理：DFS + 自动 light-sleep。
+CONFIG_PM_ENABLE=y
+CONFIG_FREERTOS_USE_TICKLESS_IDLE=y
+CONFIG_FREERTOS_IDLE_TIME_BEFORE_SLEEP=3
+CONFIG_PM_POWER_DOWN_CPU_IN_LIGHT_SLEEP=y     # 默认 y，-650 uA
+CONFIG_PM_SLP_DISABLE_GPIO=y                   # 默认 y，-200~300 uA
+CONFIG_PM_RESTORE_CACHE_TAGMEM_AFTER_LIGHT_SLEEP=y   # 默认 y，别关
+CONFIG_ESP_SLEEP_FLASH_LEAKAGE_WORKAROUND=y   # 默认 y，别关

-CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_320=y
+# 240 MHz：datasheet Table 5-9 满载 147 -> 126 mA
+CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y

-CONFIG_SPIRAM_XIP_FROM_PSRAM=y
+# 关闭后固件在 flash 里跑。取指不再跨 PSRAM 总线。
+CONFIG_SPIRAM_XIP_FROM_PSRAM=n

-CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y
+# 关掉后 lwIP/net80211/wpa_supplicant/bluedroid 的 .bss 回到片内 RAM。
+CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=n
```

**依赖关系（重要）**

- `CONFIG_ESP_IRIS_SERVICE_CONTEXT_PSRAM=y`、`..._STATE_PSRAM=y`、
  `..._RPC_RESPONSE_PSRAM=y`、`..._WIRE_BUFFERS_PSRAM=y`
  **依赖 `CONFIG_SPIRAM_XIP_FROM_PSRAM`**。关掉 XIP 后它们会退回片内 RAM，
  腾出约 4 MB PSRAM——但片内 RAM 会变紧（S31 片内只有 512 KB，WiFi + TLS 会拿走一大块）。
  **先量片内空闲再改。**
- **auto light-sleep 会断开 WiFi 连接**，所以 P0 必须先于它。
- **降帧率（P7）会直接增加帧间空闲时间**，从而让 light-sleep 真正生效——**P7 与 light-sleep 是配套的**。
- `CONFIG_ESP_SLEEP_POWER_DOWN_FLASH=y` 在本配置下**根本选不了**（Kconfig
  `depends on !SPIRAM || ESP_LDO_RESERVE_PSRAM || SOC_PSRAM_HAS_DEDICATED_LDO`，
  而 `CONFIG_SPIRAM=y` 且未预留 LDO）。网上那篇"开这个省 76%"的文章**不可照抄**。
  官方推荐的第一选择是 `CONFIG_ESP_SLEEP_SET_FLASH_DPD`（<1 µA、无唤醒延迟），
  **但 S31 上是否存在未证实**，请在 menuconfig "Sleep Config" 下确认。
- **不要初始化 BT controller**：非 esp32 目标上 BT 全程持 `ESP_PM_NO_LIGHT_SLEEP`。

**AMOLED 亮度与内容（P9）**

```c
/* BSP init 表里把这两行改掉 */
{0x51, {0xFF}},   /* Normal mode 100% */
{0x63, {0xFF}},   /* HBM mode 100% */
/* 改成 */
{0x51, {0x80}},   /* ~50% */
{0x63, {0x80}},
/* 或运行时 */
esp_lcd_panel_co5300_set_brightness(panel, 50);
```

- AMOLED 电流与亮度近线性、**与黑色像素数强相关**——纯白/亮色 UI 会显著抬高峰值电流。
  这是这块屏独有的、且**免费的**杠杆。
- **不要**把 `esp_lcd_panel_disp_sleep()` 用作回合间休眠：Mosaico 有 RST 脚(GPIO42)，
  驱动会强制进 Deep Standby，唤醒走硬件 reset + 完整 re-init
  （`0x11` delay 600 ms + `0x29` delay 600 ms）≈ **1.36 s**。只用于关机。
- **屏供电轨 GPIO60**（active-low）是关屏省电的现成钩子。

---

## 9. ESP32-S31 功耗参考（datasheet v0.5 Table 5-9 / 5-10，Typ，mA）

| CPU MHz | 双核 WAITI（外设关 / 开） | 双核 CoreMark（外设关 / 开） |
|---|---|---|
| 320 | 25 / 75 | 91 / **147** |
| **240** | 23 / 72 | **74 / 126** |
| 160 | 21 / 70 | 57 / 107 |
| 80 | 19 / 68 | 37 / 86 |
| 40 | 13 / 56 | 22 / 65 |

| 低功耗模式 | 电流 |
|---|---|
| Light-sleep（全电源域使能） | 1 mA |
| Light-sleep（多数外设关 + USB 连接） | 0.07 mA |
| Light-sleep（全部外设关，数据在 HP memory） | 0.05 mA |
| Deep-sleep | 12 µA |
| Power off | 1 µA |

> **脚注**：light-sleep 的电流指 PSRAM **未供电**时测得。PSRAM 保持供电时
> 芯片内部电流**增加约 0.17 mA**，另加 PSRAM 自身工作电流。
> 本配置 `CONFIG_SPIRAM=y` 常开 ⇒ light-sleep 地板 ≈ **0.22 mA + PSRAM 待机电流**。

WiFi 峰值（Table 5-5）：TX 11b@21 dBm **377 mA**；11ax MCS9 **265 mA**；
**RX 110–117 mA**。BLE：TX@20 dBm 340 mA，RX 105 mA。

**关键换算**：从 320 降到 240 MHz，满载省 **21 mA（−14%）**；
而"外设时钟开 vs 关"在 240 MHz 满载下差 **52 mA**——
后者才是"WiFi + QSPI + I2S + 触控全开"的总账。

---

## 10. 真机验证方法

**不要用 Host 结果代替真机结论**（本生态的明确纪律）。

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

   `render_us` 大 ⇒ 瓶颈在软件绘制（P1/P2/P6）；
   `acquire_us` + `present_us` 大 ⇒ 瓶颈在 DMA/送屏（QSPI 时钟、帧率、帧缓冲位置）。

2. **同时看 GSP 的统计**：`esp_gsp_region_stats()` 的 `input_pixels / output_pixels`
   直接量化 damage 路径的效果；`esp_gsp_render_phases()` 分离"画"和"送"。

3. **看 TE 调度日志**：出现 `TE schedule=OVERRUN` 或 `no safe full-frame window`
   ⇒ QSPI 太慢（§8.4）；出现 `TE line silent; using free-running transfers`
   ⇒ TE 没接对。

4. **测功耗**。用 USB 电流表或板载 BQ272220（依赖里已有 `espressif/bq27220`）读整机电流。
   分别记录：标题页静止 / 游戏中静止 / 战斗中 / 关掉 TCP 通道前后。

5. **摸温度**。外壳温度比 SoC 温度更能反映"烫不烫"。至少记录：
   静止 5 分钟、连续游玩 15 分钟两个点。

6. **一次只改一个变量**。这是 `render_benchmark` README 的明确纪律
   （"每次只允许一个比较维度"），也是本节所有收益数字可信的前提。

---

## 11. 优先级与风险

| 顺序 | 动作 | 风险 | 备注 |
|---|---|---|---|
| 1 | 删冗余 `ClearBackground`（P1） | 极低 | 一行，−27.7% 像素写入 |
| 2 | 时间基改造 + `target_fps` 30→10（P7/P8） | 中（手感） | 必须同时改 key repeat 与提示时长 |
| 3 | 关掉 ESP-Iris TCP 通道（P0） | 中（装机/日志） | 会失去 TCP 通道的调试能力；USB 通道保留 |
| 4 | `CONFIG_ESP_GSP_ACTIVE_TICK_MS` 10→33（P3） | 低 | 一行 |
| 5 | AMOLED 亮度 100%→50% + 深色配色（P9） | 低（可读性） | 顺带白拿"深色内容"这个 AMOLED 独有杠杆 |
| 6 | 音量联动功放（P13） | 低 | |
| 7 | QSPI 40→80 MHz + 队列深度 16（P4/P5） | 中 | **同时消除撕裂**；面板 PCLK 上限未证实 |
| 8 | 迁移新 API 用 `idle()` / `render_fps()` / damage API（P2/P6） | 中（大改） | 需要重写 `main/main.c` 的接入层 |
| 9 | CPU 320→240 MHz（P10） | 低 | −21 mA；顺带 APB 上限 53.3→40 MHz，**要注意与 QSPI 80 MHz 冲突** |
| 10 | auto light-sleep（P15 等） | 中 | 必须在关 Wi-Fi 之后；确认没 init BT |
| 11 | 关 XIP / 关 PSRAM bss（P11/P12） | **高** | 会牵动 ESP-Iris 的 PSRAM 依赖；先量片内空闲；**必须 A/B** |
| 12 | S31 PIE 硬件像素引擎 | 中 | 引擎侧开关，收益待实测 |

**建议顺序：1 → 2 → 4 → 3 → 5 → 6 → 7 → 8 → 9 → 10 → 11 → 12。**
前三项都是 app 内的小改动，收益却已经覆盖了大部分问题。

> ⚠️ **第 7 项和第 9 项有冲突**：S31 的 APB 上限随 `max_freq_mhz` 走
> （max=320 → APB 53.3 MHz，max=240 → APB 40 MHz），而 LCD QSPI 走 SPI master。
> 如果 QSPI 80 MHz 真的被 APB 40 MHz 卡住，那么**要么保持 320 MHz，
> 要么放弃 80 MHz**。这两项必须一起测，不能分开下结论。

---

## 12. 来源

### 本仓库实测

`main/game_view.c`、`main/game.c`、`main/tower.c`，
经 `tools/render_probe/` 逐帧统计与像素级差分；
`tests/win_route.json` 的实际通关路线。

### 官方文档（已抓取全文）

- ESP32-S31 datasheet v0.5 — https://documentation.espressif.com/esp32-s31_datasheet_en.html
  （§5.5 功耗表 Table 5-5/5-6/5-9/5-10、§4.1.4.6 PMU）
- IDF `api-reference/system/power_management.rst`、`sleep_modes.rst`、
  `api-guides/low-power-mode/low-power-mode-{soc,wifi}.rst`、
  `api-guides/wifi-driver/wifi-performance-and-power-save.rst`、
  `api-guides/external-ram.rst`、`components/esp_pm/Kconfig`
  （均取自 `https://raw.githubusercontent.com/espressif/esp-idf/master/docs/en/...`）
- Raylib Lite Engine — https://github.com/espressif2022/raylib-lite-engine
  （`API.md`、`include/raylib_lite/raylib_lite_{game_app,runner,runtime_stats}.h`、
  `src/runtime/raylib_lite_game_app.c`、
  `examples/render_benchmark/{README.md,config/techniques.json}`）

### 组件源码（下载压缩包解包阅读）

- `espressif/esp_display_present` v1.0.2 —
  `src/target/present_profile.c`、`src/producer/present_te_{render,repair}.c`、
  `src/producer/stage/present_te_transport.c`、`src/platform/present_te.c`、
  `src/pixel/present_blit.c`、`src/producer/present_buffer_repair.c`、
  `include/*.h`、`STATE_MACHINE.md`、README
- `espressif/esp_lcd_co5300` v2.2.0 — `include/esp_lcd_co5300.h`、
  `esp_lcd_co5300_spi.c`、`esp_lcd_co5300.c`
- `espressif/esp-gsp` — `Kconfig`、`include/esp_gsp.h`、`include/esp_gsp_debug.h`、
  `docs/en/guide/display.md`、
  `examples/scenarios/thermostat/components/hw_init/hw_lcd_qspi.c`（Mosaico CO5300 参考实现）

### 其他

- ESP-Mosaico 官方工作区 — https://github.com/esp-mosaico/esp-mosaico-vibe
- 少数派《把 Meta 的 Muse 装进乐鑫的 ESP-Mosaico》 — https://sspai.com/post/115437
  （同一块板子的独立实测："ESP-Iris 同时开了 TCP 和 USB 两种通道，Wi-Fi 一通它就把 USB 丢了"；
  "关掉 `CONFIG_SPIRAM_XIP_FROM_PSRAM`，代码和常量改在 flash 里跑"；
  "ESP32-S31 的片内 RAM 是紧的"）

---

## 13. 未证实清单

| 声明 | 状态 |
|---|---|
| **AMOLED 面板（CO5300 + 480×480 模组）的实际电流** —— 亮度-电流曲线、全白 vs 全黑电流差 | **未证实**。CO5300 规格书 V0.0x 未公开可下载，只拿到第三方模组页面的营销性描述，不可引用为数据 |
| **ESP32-S31 的 WiFi 空闲实测**（modem-sleep / light-sleep / DTIM 表） | **未证实**。IDF 源码里有 `.. todo - add sleep-current/esp32s31_summary.inc`，官方未发布。S3 的数字只能作代际参考（S3: auto light-sleep DTIM1/3/10 = 2.45/1.33/0.93 mA avg，min 0.28 mA） |
| **CO5300 Quad-SPI 的最大 PCLK** | **未证实**。80 MHz 的可行性来自同板 ST77916 参考 profile（800×480 @80 MHz）+ Espressif 已设 `GPIO_DRIVE_CAP_3`，**不是** CO5300 的规格声明 |
| **QSPI 40 MHz 是否被 S31 的 40 MHz APB 上限卡住** | **推断**。S31 APB 上限确实是 40 MHz（max=240），但 SPI 是否真跑在 APB 时钟域上未验证 |
| **CO5300 能否通过标准命令降低 GRAM 扫描率**（做"面板降到 30 Hz"） | **未证实**。`0x53` Write Control Display 可能有相关语义，但没有 CO5300 命令手册 |
| **S31 datasheet v0.5 Table 5-12 "PSRAM 最大 80 MHz" vs Kconfig `SPIRAM_SPEED_250M`** | **官方文档自相矛盾**。已列为 v0.5 PRELIMINARY 缺陷。你的 250 MHz 是 Kconfig 认的，但需长时间稳定性验证 |
| **`CONFIG_ESP_SLEEP_SET_FLASH_DPD` / `CONFIG_ESP_SLEEP_PSRAM_LEAKAGE_WORKAROUND` 在 S31 上是否存在** | **未证实**。需在 menuconfig "Sleep Config" 下确认 |
| **esp32s31 支持的合法 `min_freq_mhz` 取值集合** | **未证实**。用文档推荐的 `CONFIG_XTAL_FREQ` 最安全 |
| **S31 上 WiFi 6 TWT 的可用 IDF API 与省电收益** | **未证实**。数据手册列了 TWT 特性，但没找到 API 也没找到电流数据。**潜在收益大，值得单独查** |
| **`swap_bytes=true` 时 blit 是否走 PPA**（还是退化为软件 `__builtin_bswap16` 逐像素） | **未证实**。若是软件路径，230,400 次 bswap+store 会是第 3 大 CPU 开销 |
| **`mosaico_game_app`（旧 API）是否透出 `discard` / `idle` / `render_fps`；应用侧能否直接拿到 framebuffer 指针** | **未证实**——需查 `mosaico_raylib_fast` 与 `mosaico_game_app` 头文件 |
| **ESP-Mosaico BSP 实际使用的 `pclk_hz` 与 `trans_queue_depth`** | **未证实**。只看到 Espressif 参考代码是 40 MHz / depth=10（depth=10 低于 README 要求的 16） |
| **实际整机电流与外壳温度** | **未实测**——本沙箱无硬件；§10 给出了测量步骤 |
| **S31 PIE 对本游戏负载的加速比** | **未实测**——`render_benchmark` 有工具，但需在真机上按单维度纪律跑 |
| **降帧率后的真机触摸延迟** | **未实测**——逻辑在事件回调里即时处理（`game_set_pointer` → `tap()`/`move()` 同步执行），**理论上只有视觉反馈受帧率影响**，但需真机确认 |
| **Espressif 官方的"电池供电掌上游戏机"功耗预算方法论** | **不存在**（查过）。这是本次调研最大的空缺 |

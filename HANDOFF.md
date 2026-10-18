# 接力说明：按操作刷新（2026-10-18）

## 当前目标

用户同意继续性能 PR #3，并要求去掉 NPC 动画、只在操作时刷新。
本次只修改该性能分支，不合并地图/研究 PR，不安装固件。
硬件是 **480×480 方屏 ESP-Mosaico**。

## 工程位置

- 本次源码工作树：`D:\espbulid\worktrees\magic-tower-demand-render`
- Git 分支：`codex/event-driven-render`，基于 PR #3 的 `cec9a06`。
- PR：<https://github.com/Wzdhehe/mosaico-magic-tower/pull/3>
- 验证工程：`D:\espbulid\esp-mosaico-vibe\projects\magic_tower_demand_render`
  （通过官方 blank 模板创建，再复制本仓库文件）。
- 原 `projects/magic_tower`、`projects/snake` 和恢复备份继续保留。

## 已实现

- `game.c` 每实例管理待刷新状态；空闲回调允许引擎跳过更新/绘制。
- NPC、敌人、公主、终点固定为静态帧；拾取、保存/读取提示不再计时消失。
- 操作、变化的音量及页面切换请求刷新；重复触摸采样不触发额外绘制。
- 保留长按重复移动、战斗/对话后的释放抑制；暂停/重置不丢失按键边沿状态。
- 音轨更新与绘图状态分离，强制截图不会吞掉音轨切换。
- `game_render()` 用于完整 Host 截图；设备通过 `game_device_render.c`
  只绘制待刷新帧。帧获取失败保留待刷新标志，提交拒绝时通过官方统计重新请求刷新。
- 沿用官方首次绘图、flush、mark healthy、Iris 和 Recovery 顺序。

## 验证与证据

工作树 `artifacts/` 保存状态测试红/绿日志、渲染调度日志、提交失败回归红日志及模型测试日志。
验证工程 `artifacts/` 保存官方 Host ABI 交互截图与状态，`build-host/frame.png` 是官方模拟输出。

- 严格 C 状态/输入/字形检查通过，包含十层路线和两种商店购买。
- 便携模型回归通过，存档仍是事务式 2 KiB。
- 真正 RGB565 引擎测试：标题和拾取后各 300 次空闲轮询均为 **0 次新帧获取/提交**；
  获取失败、提交拒绝均能重试；活动 tick 改变时像素仍静止；新截图目标完整填充。
- 官方 Host 交互回归 11 项：10 通过，1 个原有商店路线测试跳过；
  新增 C 状态测试覆盖商店购买，不应将该旧跳过描述成已恢复。
- 官方 headless 模拟运行 120 帧；已检查方屏游戏/战斗图像。
- 独立代码审查未发现可操作问题。主板触摸、TE 扫描、音频、功耗/温度仍需硬件验证。

编译/PR 最终状态及日志路径见下方“交付记录”；没有记录成功前，不要假定已编译或已推送。

## 继续开发的约束与命令

先读官方工作区 `AGENTS.md`、`projects/AGENTS.md` 及相关技能。
引擎固定在 `ca3200a6b7f63e2bdea07851e19fdbd3a42eb742`。
当前官方工作区引擎有**其他任务已有的 TE 修改**，不要撤销或覆盖；本功能无需再改引擎。
ESP-IDF 必须为 `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`，已通过 doctor 核对。

```powershell
$env:CC='D:\espbulid\mingw64\bin\gcc.exe'
$env:Path='D:\espbulid\mingw64\bin;'+$env:Path
$env:MOSAICO_ENGINE_ROOT='D:\espbulid\esp-mosaico-vibe\submodule\raylib-lite-engine'
python tests/run_game_timing.py
python tests/run_tests.py --label demand-render
python tests/run_render_schedule.py
```

在官方验证工程中运行 `python tests/test_integration.py`。
在官方工作区设置 `IDF_PATH=D:\espbulid\esp-idf`、
`IDF_TOOLS_PATH=D:\espbulid\.espressif`，以及
`MOSAICO_IDF_PYTHON=D:\espbulid\.espressif\python_env\idf6.2_py3.13_env\Scripts\python.exe`，
清除继承的 `MSYSTEM`、`MSYSTEM_PREFIX`、`SHELL` 后运行：

```powershell
python mosaico.py game build --project projects/magic_tower_demand_render
```

保留项目 CMake 中已有的 GSPC Windows depfile 修正，不要为普通编译问题 fullclean。
素材来源、字体生成器、代码绘制精灵、音轨来源及许可在 `assets_src/`、`tools/`、
`main/game_view.c`、`main/game_audio.c`、`NOTICE` 中。设计和计划在 `docs/superpowers/`。

硬件更新以后只走官方 `mosaico.py iris` 流程，先重新确认设备身份和模式。
本次没有改分区、写开发板、合并 PR 或测出温度下降。

## 交付记录

- 官方 `game build` 退出 0，完整编译 2363 个步骤；固件 1,410,464 字节，工具报告 3 个警告。
- 固件：`D:\espbulid\esp-mosaico-vibe\projects\magic_tower_demand_render\build\magic_tower.bin`
- 原始编译日志：`D:\espbulid\esp-mosaico-vibe\.codex-runs\mosaico\20261018T020337Z-game-build\build\20261018-100337-build-11952\raw.log`
- 已完成独立审查；本次性能提交将推送到现有 PR #3，保留草稿、不合并。
- 用户随后明确要求继续地图优化；后续工作树为
  `D:\espbulid\worktrees\magic-tower-map-integration`，以 PR #6（叠在 #1 上）为基础，
  后续需和本次性能改动一起验证。不要把本性能构建误当成新地图构建。

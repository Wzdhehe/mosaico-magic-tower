# DS：五十层版本的便携测试迁移

工作目录：`D:\espbulid\worktrees\magic-tower-level-scenes`。

你只负责机械迁移测试，不负责关卡设计。主代理正在修改其他文件；不要回退、覆盖、清理别人的改动，不要提交、推送、合并或刷机。

## 允许修改的文件

- `tests/test_game_timing.c`
- `tests/tower_floors_stub.c`
- `tests/run_game_timing.py`
- `tests/test_render_schedule.c`
- `tests/run_render_schedule.py`

禁止修改 `main/`、关卡蓝图、对白、音乐、其他测试、CI 和官方引擎。发现生产问题请报告具体重现，不要自行改生产代码或降低断言要求。

## 当前状态

主代理已经把批准的50层地图、隐藏房间、三条交付支线、水闸和星记接入生产模型。真实生产模型五条路线、真实 game 输入的一条完整路线和108组双语对白渲染检查已通过。此任务不是重新设计地图。

存档现在是 v8，仍2632字节。暗地砖 `%` 现在一次走入就进入并揭示，不再先撞一次。`game.c` 根据实际楼层与坐标派发 NPC/事件对白；模型也有固定坐标的隐藏区、任务及水闸规则，因此旧的通用地图桩可能与这些坐标相撞。

重开流程现在是 `GAME_RESTART_MENU`（检查点重试／新游戏／取消）；新游戏还有 `GAME_NEW_CONFIRM` 二次确认。几何位置见 `main/game_config.h`。保持这个行为，不能改回直接重开。

## 任务一：计时与检查点回归

运行 `tests/run_game_timing.py`。主代理已迁移 v8、重开菜单和单次进入暗砖，但当前在 `step_up` 的换层断言失败；请先确定哪一层、哪一格和哪个状态导致失败。不要盲目增加按键次数。

`tests/tower_floors_stub.c` 是独立行为夹具，不是发布地图。允许调整夹具坐标和对应测试路线，使它与生产模型的坐标规则相容；注释应明确这是隔离夹具。禁止用宏绕开生产规则。

保留并跑过全部有意义的覆盖：新游戏写检查点、真实换层写一次、同层及死亡不写、写失败真实报告、六锚点及章节解锁、死亡检查点恢复、通关继续探索、有效／无／损坏存档、设置和按住重复输入、空闲无刷新。不要删除失败用例或把断言改成仅“不崩溃”。

## 任务二：真实渲染调度回归

`tests/run_render_schedule.py` 当前缺少楼层表链接；现有 C 测试路线还依赖旧10层地图。优先链接真实 `main/tower_floors.c`，按实际第一层坐标迁移短路线（出生点 D=(5,9)）；需要夹具时必须说明理由并确保夹具不冒充生产地图证据。

引擎位置：`D:\espbulid\esp-mosaico-vibe\submodule\raylib-lite-engine`。它已有合法的显示所有权补丁，不要重置或重复应用。使用 `MOSAICO_ENGINE_ROOT` 指向它；可改 runner 默认路径查找以适应 worktree，但显式错误路径要报错。

保留真实 RGB565 引擎、帧端口计数 spy，以及全部采集失败、提交失败、异步丢帧恢复、空闲不获取／提交帧、截图可重画、静止怪物／提示不动画、语言切换、音量拖动、亮度拖动和释放后可继续操作的覆盖。不要改成假渲染器或仅检查函数返回值。

## 环境与交付

Python：`C:\Users\mjc39\anaconda3\python.exe`。
GCC：`D:\espbulid\mingw64\bin\gcc.exe`。

PowerShell 环境：

```powershell
$env:CC='D:\espbulid\mingw64\bin\gcc.exe'
$env:PATH='D:\espbulid\mingw64\bin;'+$env:PATH
$env:TEMP='D:\espbulid\tmp'
$env:TMP=$env:TEMP
$env:PYTHONIOENCODING='utf-8'
$env:MOSAICO_ENGINE_ROOT='D:\espbulid\esp-mosaico-vibe\submodule\raylib-lite-engine'
```

两套 runner 都保持 `-Wall -Wextra -Werror` 等原有编译要求。交付时列出根因、修改的断言／路线及理由、两条实际命令的退出码、日志路径，以及仍未解决的问题。改动留在工作树，不提交、不刷机。

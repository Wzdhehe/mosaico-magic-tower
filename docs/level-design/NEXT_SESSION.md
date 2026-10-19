# 当前接力入口

2026-10-19。先读 [PRODUCTION_INTEGRATION.md](PRODUCTION_INTEGRATION.md)。

五十层生产地图、模型、隐藏房/支线/水闸/星记、双语分页、重开菜单、自动边缘换视角和8首音乐均已接入。50F保持11×11一屏对称王座厅；不要恢复19格终局或箭头/手动视角。

当前验证入口是 `python tools/verify_campaign_release.py`，不是旧10层测试。每条日志保存在 `artifacts/campaign-validation/`。设计规则和坐标仍见 CAMPAIGN50_HANDOFF.md、CAMPAIGN50_MAPS.md 与 assets_src/level_design。

还需要真机验收和路线平衡试玩。此轮已按用户要求用官方app-update安装，ELF身份和healthy启动验证通过；完整实机验收仍待用户试玩。触控驱动报文告警已记录。不要根据Host结果宣称温度、续航、触控或音质已验证。不要重新应用 campaign_model.patch / campaign_view.patch；它们是集成前的历史恢复资料。

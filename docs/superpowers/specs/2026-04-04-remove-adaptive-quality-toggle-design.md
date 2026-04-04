# 移除自适应质量开关设计

日期：2026-04-04

## 1. 背景

当前 `adaptiveQuality` 同时影响帧率降档与 CPU fallback 解码输出策略，但对用户而言开关“体感弱”，
且维护了不必要的配置、托盘入口与分支逻辑。现决定移除开关入口，固定为“始终开启”。

## 2. 目标

- 删除 `adaptiveQuality` 配置与托盘入口。
- 自适应质量固定为始终开启（高负载降档 + CPU fallback 输出策略保留）。
- 清理相关接口字段与分支代码，减少无效状态同步。
- 允许破坏性变更：`config.json` 字段忽略且不再写回，`metrics.csv` 去掉 `adaptive_quality` 列。

## 3. 非目标

- 不改变自适应质量的具体判定阈值与策略逻辑。
- 不新增 UI 或额外配置项。
- 不改动解码/渲染主流程结构。

## 4. 方案

### 4.1 行为收口（固定开启）

- `QualityGovernor` 永远启用，高负载场景仍可将 `effective_fps` 降到 30。
- CPU fallback 输出尺寸 hint / video processing 重试等策略保持现有逻辑。

### 4.2 配置与托盘收口

- 移除 `Config::adaptiveQuality` 与 config 读写逻辑。
- 移除托盘菜单“Enable/Disable Adaptive Quality”与对应动作。
- 移除 `TrayMenuState::adaptiveQuality`，托盘状态不再展示该项。

### 4.3 解码输出策略字段清理

- `DecodeOpenProfile` 与 `DecodeOutputOptions` 不再携带 `adaptiveQualityEnabled`。
- 相关策略函数默认按“开启”路径执行，删除条件分支。

### 4.4 指标格式

- `metrics.csv` 移除 `adaptive_quality` 列；列顺序相应收口。

## 5. 影响面（文件清单）

- 配置：`include/wallpaper/config.h`、`src/config_store.cpp`、`tests/config_store_tests.cpp`
- 托盘与接口：`include/wallpaper/interfaces.h`、`src/win/tray_controller_win.cpp`、`src/app.cpp`
- 解码策略：`include/wallpaper/decode_output_policy.h`、`src/decode_output_policy.cpp`、`src/win/decode_pipeline_stub.cpp`、`tests/decode_output_policy_tests.cpp`
- 指标：`src/metrics_log_line.cpp`、`tests/metrics_log_line_tests.cpp`

## 6. 兼容性与风险

- 破坏性变更：旧 `config.json` 中 `adaptiveQuality` 被忽略且不再写回。
- 破坏性变更：`metrics.csv` 列结构变更，需要下游读取者更新。
- 行为风险较低：仅去除关闭路径，主策略保持原状。

## 7. 验证

- 单测：更新并新增与字段移除相关的断言。
- 构建：`scripts/run_tests.ps1` 与 `scripts/build_app.ps1` 通过。
- 运行：`metrics.csv` 列结构符合新定义，功能可用。

## 8. 里程碑

- M1：移除配置与托盘入口。
- M2：清理解码策略字段与指标列。
- M3：单测与构建验证通过。

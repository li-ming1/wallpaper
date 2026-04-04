# 自动帧率上限与手动 FPS 移除设计

日期：2026-04-04

## 1. 背景

用户希望完全自动判断，无需手动指定帧率。当前存在 `fpsCap` + `renderCapMode` + 托盘 30/60 入口，
容易给出“手动帧率无效”的体感；同时与“无需手动指定”的目标冲突。

## 2. 目标

- 移除所有手动 FPS 配置与 UI 入口。
- 自动上限：源帧率稳定可用时随源帧率收敛；未知时默认 60。
- 不降低分辨率、采样、码率或清晰度。
- 保留 `adaptiveQuality` 作为高负载保护路径。

## 3. 非目标

- 不新增额外 UI。
- 不引入连续帧率（仍使用离散档位）。
- 不重写解码/渲染主流程。

## 4. 方案（推荐）

### 4.1 自动目标上限

- 若 `sourceFps > 0`：`autoTargetFps = NormalizeFpsCap(sourceFps)`。
- 若 `sourceFps <= 0`：`autoTargetFps = 60`。
- `QualityGovernor::SetTargetFps(autoTargetFps)`，`effectiveFps` 仍可在高负载下自动降至 30。
- 现有 `ClampRenderFpsForCompactCpuFallback` 逻辑保留，作为 CPU fallback 降负载兜底。

### 4.2 配置与托盘收口

- 删除配置字段：`fpsCap`、`renderCapMode`。
- `config_store` 不再读取/写入上述字段；旧配置中的残留字段被忽略。
- 移除托盘菜单项：`30 FPS` / `60 FPS`。
- 删除 `TrayActionType::kSetFps30/kSetFps60` 与 `TrayMenuState::fpsCap`。
- `App::HandleTrayActions` 中移除对应分支。

### 4.3 指标语义

- CSV `target_fps` 字段保留，但语义调整为“自动目标上限”（源帧率或默认 60）。

## 5. 数据流

- `source_frame_rate_policy` 更新 `sourceFps`。
- App 计算 `autoTargetFps` 并更新 `QualityGovernor` 目标。
- `ApplyRenderFpsCap(qualityGovernor_.CurrentFps())` 继续驱动调度与解码泵节奏。

## 6. 兼容性与风险

- `config.json` 中的 `fpsCap/renderCapMode` 将被忽略且不再写回，属于破坏性变更（允许）。
- 若源帧率长期无法稳定识别，将保持默认 60；高负载时仍可自动降到 30。

## 7. 验证

- 单测：更新 `config_store_tests` 与相关策略测试，覆盖“源帧率未知 => 60”。
- 托盘：菜单项不再出现 FPS 入口。
- 运行：metrics `target_fps` 与源帧率、负载变化的关系符合预期。

## 8. 里程碑

- M1：移除配置字段与托盘 FPS 入口。
- M2：接入自动目标上限策略。
- M3：单测与构建验证通过。
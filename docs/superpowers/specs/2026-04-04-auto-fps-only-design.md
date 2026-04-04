# 自动帧率上限与手动 FPS 移除设计

日期：2026-04-04

## 1. 背景

用户希望完全自动判断，无需手动指定帧率。当前存在 `fpsCap` + `renderCapMode` + 托盘 30/60 入口，
容易给出“手动帧率无效”的体感；同时与“无需手动指定”的目标冲突。

## 2. 目标

- 移除所有手动 FPS 配置与 UI 入口。
- 自动上限：源帧率稳定可用时随源帧率收敛；未知时默认 60。
- 不降低分辨率、采样、码率或清晰度（本次不新增任何下采样或质量降级行为）。
- 保留 `adaptiveQuality` 作为高负载保护路径，本次改动仅影响 FPS 目标，不改动其现有解码输出策略。

## 3. 非目标

- 不新增额外 UI。
- 不引入连续帧率（仍使用离散档位）。
- 不重写解码/渲染主流程。

## 4. 方案（推荐）

### 4.1 自动目标上限

- 继续复用现有 `source_frame_rate_policy` 的离散识别，不引入连续帧率：
  - 仅识别 `24/25/30/60`，稳定阈值为连续命中 `4` 次（现有 `kStableSampleThreshold`）。
  - 23.976/29.97/59.94/50 等非整数会按现有时间戳区间落入最近档位（例如 50/59.94 -> 60）。
  - 未稳定识别前 `sourceFps` 为 `0`，稳定后保持最后一次识别结果，直到管线重置。
- 若 `sourceFps > 0`：`autoTargetFps = NormalizeFpsCap(sourceFps)`（本质仍是 `24/25/30/60`）。
- 若 `sourceFps <= 0`：`autoTargetFps = 60`。
- 若未来出现 `>60` 的 `sourceFps`，`NormalizeFpsCap` 会归一到 `60`。
- `QualityGovernor::SetTargetFps(autoTargetFps)`，高负载下仅在 `autoTargetFps > 30` 时会自动降至 `30`；
  若 `autoTargetFps <= 30`，则 `effectiveFps` 保持该目标值。
- 现有 `ClampRenderFpsForCompactCpuFallback` 逻辑保留，作为 CPU fallback 降负载兜底。

### 4.2 配置与托盘收口

- 删除配置字段：`fpsCap`、`renderCapMode`。
- `config_store` 不再读取/写入上述字段；旧配置中的残留字段被忽略且不会再写回。
- 移除托盘菜单项：`30 FPS` / `60 FPS`。
- 删除 `TrayActionType::kSetFps30/kSetFps60` 与 `TrayMenuState::fpsCap`。
- `App::HandleTrayActions` 中移除对应分支。
- 代码库中当前不存在 CLI/环境变量/脚本 API 的 FPS 入口；若未来新增，则同属移除范围。

### 4.3 指标语义

- CSV `target_fps` 字段保留，但语义调整为“自动目标上限”（源帧率或默认 60）。

## 5. 数据流

- `source_frame_rate_policy` 按既有阈值更新 `sourceFps`（稳定阈值 4 次，非稳定保持上次结果）。
- App 计算 `autoTargetFps` 并更新 `QualityGovernor` 目标。
- 高负载下仅在 `autoTargetFps > 30` 时将 `effectiveFps` 降至 `30`，否则保持 `autoTargetFps`。
- `ApplyRenderFpsCap(qualityGovernor_.CurrentFps())` 驱动调度与解码泵节奏。
- CPU fallback + 小分辨率情况下，`ClampRenderFpsForCompactCpuFallback` 作为最终上限。

## 6. 兼容性与风险

- `config.json` 中的 `fpsCap/renderCapMode` 将被忽略且不再写回，属于破坏性变更（允许）。
- 若源帧率长期无法稳定识别，将保持默认 60；高负载时仍可自动降到 30。
- `target_fps` 字段语义调整为“自动目标上限”，需同步接受此口径变化。

## 7. 验证

- 单测：
  - `config_store_tests` 删除 `fpsCap/renderCapMode` 读写断言。
  - 新增 “`sourceFps <= 0` => `autoTargetFps=60`” 用例。
  - 新增 “`sourceFps=24/25/30/60` => `autoTargetFps` 等于该离散档位” 用例。
  - 新增 “`autoTargetFps=60` 且高负载 => `effectiveFps=30`，`target_fps` 仍为 60” 用例。
  - 新增 “`autoTargetFps=24/25/30` 高负载时 `effectiveFps` 保持目标值” 用例。
- 托盘：菜单项不再出现 FPS 入口。
- 运行：metrics `target_fps` 与源帧率、负载变化关系符合预期。

## 8. 里程碑

- M1：移除配置字段与托盘 FPS 入口。
- M2：接入自动目标上限策略。
- M3：单测与构建验证通过。

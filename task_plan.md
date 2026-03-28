# Task Plan: Windows Dynamic Wallpaper V1

## Goal
Implement a Windows 10/11 dynamic wallpaper app from an empty repo with strict performance-first architecture (WorkerW + MF + D3D11), plus tests for core logic.

## Current Phase
Phase 6

## Phases
### Phase 1: Requirements & Discovery
- [x] Capture user goals and hard constraints
- [x] Lock architecture and acceptance targets
- [x] Initialize planning memory files
- **Status:** complete

### Phase 2: TDD Red Phase
- [x] Create failing tests for config, scheduler, and arbiter
- [x] Verify failures are caused by missing behavior
- **Status:** complete

### Phase 3: Green Phase Implementation
- [x] Implement core modules to pass tests
- [x] Implement Windows host/decode/render integration skeleton
- [x] Build app entrypoint and platform stubs
- **Status:** complete

### Phase 4: Verification
- [x] Run tests and collect results
- [x] Validate test script for local repeatability
- [x] Update findings and progress logs
- **Status:** complete

### Phase 5: Delivery
- [x] Review created files and architecture boundaries
- [x] Deliver implementation notes and limitations
- **Status:** complete

### Phase 6: Performance Hardening (In Progress)
- [x] Add tray context menu exit path
- [x] Add virtual desktop multi-monitor render coverage
- [x] Add MF-driven decode timeline with fallback ticker
- [x] Bind real decoded video frame texture to renderer
- [x] Add adaptive quality governor (load spike -> 30fps, stable -> target fps)
- [x] Add tray toggles for adaptive quality and persist to config
- [x] Add bounded metrics log writer with auto-truncate on threshold
- [x] Add daily metrics shards with retention window (keep last 7 days)
- [x] Add metrics session-level fields (session_id/target/effective/adaptive)
- [x] Add decode mode field in metrics (mf/fallback/unknown)
- [x] Fix MF RGB32 negotiation to avoid false fallback on valid videos
- [x] Fix desktop interaction regression (cursor busy + icons hidden)
- [x] Harden wallpaper parent attach (multi-candidate WorkerW + parent-relative coordinates)
- [x] Replace unsafe Progman/Desktop fallback with safe attach order (WorkerW -> DefView)
- [x] Fix main entry DPI initialization compile break
- [x] Prevent duplicate decode restart when selecting equivalent video path (relative/absolute/case mismatch)
- [x] Reinforce desktop icon layer visibility when wallpaper host attaches/resizes
- [x] Restore DefView fallback with strict icon-list z-order control (video visible + icons visible)
- [x] Reduce MF decode path memory footprint (buffer reuse + low-latency source reader + video-only stream)
- [x] Fix tray UTF-8 conversion for non-ASCII video paths
- [x] Fix false fullscreen pause when clicking desktop shell windows
- [x] Keep presenting last frame when decode has no new sample to remove 30fps-source stutter artifact
- [x] Build wallpaper_app as Windows subsystem binary (no console window on startup)
- [x] Apply source-frame-rate aware render cap path to reduce cadence jitter
- **Status:** in_progress

## Key Questions
1. Can we keep decode/render pipeline modular while maintaining zero-copy path readiness? (Yes)
2. What minimal UI/control surface delivers usability without performance penalty? (Tray loop skeleton ready)

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| Windows-only V1 | Best chance to hit extreme performance target |
| C++20 + Win32 + MF + D3D11 | Maximum low-level control and efficiency |
| Default 30 FPS, optional 60 FPS | Balance smoothness and power |
| Pause when not desktop context | Keep desktop-involved scenarios dynamic while minimizing non-desktop overhead |
| g++ test script fallback | Current shell lacks cmake in PATH |

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| cmake not found in PATH | 1 | Added `scripts/run_tests.ps1` using g++ |
| MinGW link failed (`FOLDERID_LocalAppData` / `WinMain`) | 1 | Switched to `SHGetFolderPathW` and added dual entrypoints (`main` + `wWinMain`) |
| `shellapi.h` compile break in tray module | 1 | Fixed include order (`windows.h` before `shellapi.h`) and icon resource loading |
| `GUID_NULL` unresolved in MinGW MF path | 1 | Replaced with local zero-initialized GUID constant |

## Notes
- Enforce TDD for each new behavior.
- Keep modules split when readability threshold is exceeded.

### Phase 7: 启动遮盖层与无效路径治理（In Progress）
- [ ] Red: 新增启动策略失败测试（路径有效性与首帧前不呈现）
- [ ] Green: 实现启动策略模块并接入 App 初始化/换源
- [ ] Green: 调整 Windows 宿主窗口首帧后再可见
- [ ] Verification: 运行测试与构建验证
- **Status:** in_progress

### Phase 7: 启动遮盖层与无效路径治理（Completed）
- [x] Red: 新增启动策略失败测试（路径有效性与首帧前不呈现）
- [x] Green: 实现启动策略模块并接入 App 初始化/换源
- [x] Green: 调整 Windows 宿主窗口首帧后再可见
- [x] Verification: 单测通过 + `wallpaper_app.exe` 构建通过
- **Status:** complete

### Phase 8: CPU/内存优化（Completed）
- [x] Red: 新增循环睡眠策略测试与调度器剩余等待测试
- [x] Green: 主循环改为按下一帧时间自适应睡眠
- [x] Green: 解码泵加入退避等待，降低无帧空转
- [x] Green: 壁纸层分离时主动清理帧桥接缓存
- [x] Verification: 单测通过 + 构建通过
- **Status:** complete

### Phase 9: CPU/内存优化迭代二（Completed）
- [x] Red: 新增探测节流与帧缓冲容量策略测试
- [x] Green: 接入 runtime probe 节流，降低每帧 Win32 查询频率
- [x] Green: 接入帧缓冲容量回收策略，降低分辨率切换后的峰值驻留
- [x] Verification: 单测通过 + 构建通过
- **Status:** complete

### Phase 10: CPU/内存优化迭代三（Completed）
- [x] Red: 调整循环睡眠策略测试上限（主循环 20ms、解码泵 12ms）
- [x] Green: 扩大主循环与解码泵退避区间，降低唤醒频率
- [x] Green: pause 处理改为边沿触发并在进入 pause 时清理帧缓存
- [x] Verification: 单测通过 + 构建通过
- **Status:** complete

### Phase 11: 全屏资源释放优化（Completed）
- [x] Red: 新增 pause 资源策略测试并收紧睡眠阈值测试
- [x] Green: 全屏进入时释放解码+渲染资源，退出全屏后恢复
- [x] Green: pause 档位睡眠进一步拉长（主循环 80ms，解码泵 no-ready 30ms）
- [x] Verification: 单测通过 + 构建通过
- **Status:** complete

### Phase 12: 全屏静态壁纸无缝恢复（Completed）
- [x] Red: 新增“暂停保留壁纸层”策略测试并先触发失败
- [x] Green: 全屏时仅停解码，保留最后一帧壁纸层
- [x] Green: 退出全屏自动恢复解码并无缝续播
- [x] Verification: 单测通过 + 构建通过
- **Status:** complete

### Phase 13: 全屏识别鲁棒性修复（Completed）
- [x] Red: 新增监视器覆盖容差测试并先触发失败
- [x] Green: 引入近似覆盖判定 + 扩展帧边界读取
- [x] Verification: 单测通过，临时目录构建通过（主产物被运行中进程占用）
- **Status:** complete

### Phase 14: 全屏暂停触发与恢复稳定性修复（Completed）
- [x] Red: 新增/调整策略测试并先触发失败
- [x] Green: 全屏时保持壁纸层，不再因 pause 退回系统原壁纸
- [x] Green: 恢复路径增加自动重试，避免一次恢复失败后永久不恢复
- [x] Green: 放宽全屏识别对可见性依赖
- [x] Verification: 单测通过 + 构建通过
- **Status:** complete

### Phase 15: 全屏识别迟钝修复（Completed）
- [x] Red: 先让全屏策略测试失败（壁纸层保持 + 可见性依赖）
- [x] Green: 覆盖率+样式联合判定全屏
- [x] Green: Unknown 状态不覆盖已缓存前台状态
- [x] Green: 保持壁纸层与恢复重试逻辑继续生效
- [x] Verification: 单测通过 + 构建通过
- **Status:** complete

### Phase 16: 最大化暂停开关（Completed）
- [x] Red: 新增配置与仲裁测试（pauseOnMaximized / kMaximized）
- [x] Green: 增加 Config 字段与持久化
- [x] Green: 前台状态新增 maximized 判定并接入 ResourceArbiter
- [x] Verification: 单测通过 + 构建通过
- **Status:** complete

### Phase 17: 全屏检测精度与切换丝滑优化（Completed）
- [x] Red: 新增 pause 迟滞状态机与全屏覆盖规则测试并先触发失败
- [x] Green: 引入 pause transition policy（enter/exit 迟滞）
- [x] Green: 增强前台识别（最小化/被 DWM cloaked 过滤 + 覆盖率规则）
- [x] Verification: 单测通过 + 构建通过
- **Status:** complete

### Phase 18: 桌面上下文驱动暂停（Completed）
- [x] Red: 新增 desktop-context 策略测试 + config/resource_arbiter 扩展测试
- [x] Green: 引入 desktop_context_policy 并接入 Tick 探测
- [x] Green: 新增配置 `pauseWhenNotDesktopContext`（默认 true）
- [x] Green: ResourceArbiter 支持“非桌面上下文暂停”
- [x] Verification: 单测通过 + 构建通过
- **Status:** complete

### Phase 19: 配置收敛与切换丝滑优化（Completed）
- [x] Red: 移除旧配置语义测试 + 新增分阶段暂停策略测试
- [x] Green: 仅保留 `pauseWhenNotDesktopContext`
- [x] Green: 暂停切换改为二阶段（先 Pause，长暂停再 Stop）
- [x] Verification: 单测通过 + 构建通过
- **Status:** complete

### Phase 20: 切换顿挫细化优化（Completed）
- [x] Green: 非桌面上下文场景保留最后一帧，不再在轻暂停时清空帧桥接
- [x] Green: 仅在会话不可交互/桌面不可见时允许硬挂起，普通全屏保持轻暂停
- [x] Green: 延长硬挂起阈值（8s），降低短中时长切换的恢复卡顿
- [x] Verification: 单测通过（67/67） + 构建通过
- **Status:** complete

### Phase 21: CPU/内存迭代优化四（Completed）
- [x] Green: 暂停态主循环睡眠与解码泵退避进一步降频
- [x] Green: 暂停态 runtime probe 改为稀疏采样，降低 Win32 探测频率
- [x] Green: 轻暂停持续后仅回收解码帧缓存（不 Stop 管线），降低内存驻留峰值
- [x] Verification: 单测通过（72/72） + 构建通过
- **Status:** complete

### Phase 22: 长静态内存回收与抖动抑制（Completed）
- [x] Green: 长静态场景允许保守阈值深度挂起（12s），释放解码链路内存
- [x] Green: 深度挂起后触发进程工作集回收，提升内存下降可见性
- [x] Green: 解码泵“有帧”最小睡眠从 0ms 调整到 2ms，抑制长期动态忙轮询
- [x] Verification: 单测通过（74/74） + 构建通过
- **Status:** complete

### Phase 23: 深挂起恢复卡顿修复（Completed）
- [x] Green: 退出 pause 迟滞窗口内预热 `Open`，恢复时仅执行 `Start`
- [x] Green: 深挂起恢复场景延长 exit delay（360ms）用于预热，减少解冻顿挫
- [x] Green: 预热后若再次回到暂停态自动回收，防止频繁切换反向抬升内存
- [x] Verification: 单测通过（75/75） + 构建通过
- **Status:** complete

### Phase 24: 动态高负载与恢复顿挫联合优化（Completed）
- [x] Green: 修复解码泵“拿到帧后未有效睡眠”的忙轮询路径
- [x] Green: 新增按渲染帧率上限的解码泵 hot-sleep 策略（60fps->4ms, 30fps->8ms）
- [x] Green: 深挂起恢复阶段提前启动解码，减少切回动态的首帧等待
- [x] Verification: 单测通过（76/76） + 构建通过
- **Status:** complete

### Phase 25: 清除/重选/退出卡顿与托盘状态可视化（Completed）
- [x] Green: 托盘菜单支持实时状态勾选与已生效项置灰
- [x] Green: App 主动同步真实状态（FPS/AutoStart/Adaptive/HasVideo）
- [x] Green: 解码 Stop 路径去除 MF 频繁 shutdown，降低清除/重选/退出卡顿
- [x] Verification: 单测通过（76/76） + 构建通过
- **Status:** complete

### Phase 26: 托盘交互误暂停修复与切换顺滑性优化（Completed）
- [x] Green: 前台属于本进程时强制判定桌面上下文，修复“选视频后变静态”
- [x] Green: 增加托盘交互后 1.2s 桌面上下文保护窗口，抑制菜单/对话框抖动误判
- [x] Green: 视频切换路径去除不必要 Stop->Open 双重重置，降低清除后重选顿挫
- [x] Verification: 单测通过（76/76） + 构建通过
- **Status:** complete

### Phase 27: 托盘菜单动态误切修复与退出切换降卡顿（Completed）
- [x] Green: 菜单打开/关闭显式事件化，交互窗口内冻结桌面上下文探测
- [x] Green: 去除“托盘交互强制切动态”策略，改为保持当前上下文状态
- [x] Green: 解码泵睡眠改可中断等待，退出与切换时降低 join 卡顿
- [x] Verification: 单测通过（76/76） + 构建通过
- **Status:** complete

### Phase 28: 长动态负载压降与恢复唤醒优化（Completed）
- [x] Green: 解码泵 no-ready 睡眠提升到 70ms，进一步降低静态/空闲 CPU
- [x] Green: hot-sleep 调整为 60fps=6ms、30fps=12ms，降低长动态忙轮询
- [x] Green: 所有 `Start()` 路径主动唤醒解码线程，避免降频后恢复迟滞
- [x] Verification: 单测通过（76/76） + 构建通过
- **Status:** complete

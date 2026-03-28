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
| Pause on fullscreen | Guarantee no foreground stutter |
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

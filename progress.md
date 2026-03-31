# Progress Log

## Session: 2026-03-27

### Phase 1: Discovery and Planning Setup
- **Status:** complete
- **Started:** 2026-03-27 22:00
- Actions taken:
  - Confirmed repository is empty.
  - Collected skill constraints (brainstorming, planning-with-files, TDD).
  - Finalized architecture and acceptance criteria.
  - Created persistent planning files.
- Files created/modified:
  - task_plan.md
  - findings.md
  - progress.md

### Phase 2: TDD Red
- **Status:** complete
- Actions taken:
  - Added tests for ConfigStore, RenderScheduler, ResourceArbiter.
  - Ran compile in red state and confirmed missing implementation failures.
- Files created/modified:
  - tests/test_main.cpp
  - tests/test_support.h
  - tests/config_store_tests.cpp
  - tests/render_scheduler_tests.cpp
  - tests/resource_arbiter_tests.cpp

### Phase 3: Green Implementation
- **Status:** complete
- Actions taken:
  - Implemented core config/scheduler/arbiter/metrics modules.
  - Implemented app orchestration layer.
  - Added Windows platform skeleton (wallpaper host, decode pipeline stub, tray controller).
  - Added cross-platform fallback stubs for non-Windows builds.
- Files created/modified:
  - include/wallpaper/*.h
  - src/*.cpp
  - src/win/*.cpp
  - CMakeLists.txt

### Phase 4: Verification
- **Status:** complete
- Actions taken:
  - Added scripts/run_tests.ps1 for deterministic local test runs.
  - Executed script and confirmed all tests pass.
  - Compiled wallpaper_app with MinGW and fixed link issues in entry/config-path code.
- Files created/modified:
  - scripts/run_tests.ps1
  - src/main.cpp

### Phase 5: Parallel Performance Hardening
- **Status:** in_progress
- Actions taken:
  - Enabled virtual-screen render sizing in wallpaper host for multi-monitor coverage.
  - Upgraded decode pipeline to MF-first timeline mode with fallback ticker.
  - Added tray icon context menu exit behavior and fixed MinGW compatibility.
  - Added arbiter regression tests for session/desktop visibility pause behavior.
  - Added frame bridge and video texture render path (decode publish -> render consume).
  - Added tray hot actions: set 30/60 FPS, select video, clear video, auto-start on/off.
  - Added periodic runtime metrics logging to `%LOCALAPPDATA%\\wallpaper\\metrics.csv`.
- Files created/modified:
  - src/win/wallpaper_host_win.cpp
  - src/win/decode_pipeline_stub.cpp
  - src/win/tray_controller_win.cpp
  - src/app.cpp
  - include/wallpaper/frame_bridge.h
  - src/frame_bridge.cpp
  - include/wallpaper/interfaces.h
  - include/wallpaper/app.h
  - tests/resource_arbiter_tests.cpp
  - tests/frame_bridge_tests.cpp
  - scripts/run_tests.ps1

## Session: 2026-03-28

### Phase 6: Adaptive Quality Iteration
- **Status:** complete
- Actions taken:
  - Added TDD red tests for `Config.adaptiveQuality` persistence and new `QualityGovernor` behavior.
  - Implemented `QualityGovernor` with hysteresis (high-load immediate downgrade, stable-load delayed recovery).
  - Integrated governor into app metrics sampling loop and connected effective fps to scheduler.
  - Added tray actions for enable/disable adaptive quality and config persistence.
  - Fixed `scripts/run_tests.ps1` false-green path by enforcing compile exit code check.
  - Verified full test suite and built `wallpaper_app.exe` with MinGW g++.
- Files created/modified:
  - include/wallpaper/quality_governor.h
  - src/quality_governor.cpp
  - tests/quality_governor_tests.cpp
  - include/wallpaper/config.h
  - src/config_store.cpp
  - include/wallpaper/interfaces.h
  - include/wallpaper/app.h
  - src/app.cpp
  - src/win/tray_controller_win.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1
  - tests/config_store_tests.cpp

### Phase 6: Metrics Log Bounding Iteration
- **Status:** complete
- Actions taken:
  - Added TDD red tests for metrics log initialization and threshold truncate behavior.
  - Implemented `MetricsLogFile` module with fixed max-bytes limit and header-preserving recycle.
  - Integrated app metrics write path with `MetricsLogFile`, replacing direct append stream writes.
  - Re-verified unit tests and rebuilt `wallpaper_app.exe` after integration.
- Files created/modified:
  - include/wallpaper/metrics_log_file.h
  - src/metrics_log_file.cpp
  - tests/metrics_log_file_tests.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1

### Phase 6: Metrics Daily Sharding Iteration
- **Status:** complete
- Actions taken:
  - Added TDD red test for daily shard naming and retention cleanup behavior.
  - Extended `MetricsLogFile` to support date-key based shard path resolution.
  - Added retention pruning to keep only recent N daily shards (current app setting: 7 days).
  - Kept per-file size cap and header-preserving truncate behavior unchanged.
  - Fixed prune timing bug found by test: prune now runs after active shard is ensured.
  - Re-verified full tests and app build.
- Files created/modified:
  - include/wallpaper/metrics_log_file.h
  - src/metrics_log_file.cpp
  - tests/metrics_log_file_tests.cpp
  - src/app.cpp

### Phase 6: Metrics Session Context Iteration
- **Status:** complete
- Actions taken:
  - Added TDD red tests for CSV header and row formatting with extended context fields.
  - Implemented `metrics_log_line` module to centralize header/row construction.
  - Extended runtime metrics row to include `session_id`, `target_fps`, `effective_fps`, `adaptive_quality`.
  - Added per-process metrics session id generation in app startup path.
  - Re-verified full tests and `wallpaper_app.exe` build.
- Files created/modified:
  - include/wallpaper/metrics_log_line.h
  - src/metrics_log_line.cpp
  - tests/metrics_log_line_tests.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1

### Phase 6: Metrics Decode Mode Iteration
- **Status:** complete
- Actions taken:
  - Added TDD red tests to require `decode_mode` column and value encoding in metrics CSV.
  - Introduced `DecodeMode` enum in frame token and propagated decode mode from decode pipeline outputs.
  - Extended metrics CSV builder to encode decode mode (`mf` / `fallback` / `unknown`).
  - Wired app metrics flush path to emit latest decode mode each sample window.
  - Re-ran full tests and app build.
- Files created/modified:
  - include/wallpaper/interfaces.h
  - src/win/decode_pipeline_stub.cpp
  - include/wallpaper/metrics_log_line.h
  - src/metrics_log_line.cpp
  - tests/metrics_log_line_tests.cpp
  - include/wallpaper/app.h
  - src/app.cpp

### Phase 6: MF Decode Recovery Iteration
- **Status:** complete
- Actions taken:
  - Diagnosed purple wallpaper symptom as fallback renderer path (`decode_mode=fallback`).
  - Probed MF negotiation and confirmed `SetCurrentMediaType(..., MFVideoFormat_RGB32)` failure without video processing attributes.
  - Enabled Source Reader attributes: `MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING` and `MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS`.
  - Rebuilt app and validated runtime metrics switched to `decode_mode=mf` for target MP4.
- Files created/modified:
  - src/win/decode_pipeline_stub.cpp

### Phase 6: Desktop Interaction Fix Iteration
- **Status:** complete
- Actions taken:
  - Diagnosed desktop abnormal behavior to two causes: wrong WorkerW host selection and missing main-thread message pumping.
  - Updated WorkerW discovery to target the sibling WorkerW behind desktop icon host.
  - Added main-loop `PeekMessage` pump so render host window does not appear unresponsive to shell.
  - Re-ran unit tests and rebuilt app binary.
- Files created/modified:
  - src/win/wallpaper_host_win.cpp
  - src/app.cpp

### Phase 6: Attach Robustness Iteration
- **Status:** complete
- Actions taken:
  - Reproduced startup/visibility issue to `CreateWindowEx` denial on certain WorkerW parents.
  - Hardened parent selection by trying multiple WorkerW candidates before Progman/Desktop fallback.
  - Normalized child placement to parent space (`x=0,y=0`) to avoid multi-monitor negative offset placement issues.
  - Verified tests and rebuild are clean.
- Files created/modified:
  - src/win/wallpaper_host_win.cpp

### Phase 6: DefView Composition Fallback Iteration
- **Status:** complete
- Actions taken:
  - Confirmed shell environment has no usable full-screen visible WorkerW (only tiny hidden WorkerW windows).
  - Added DefView fallback path: reparent `SysListView32` icon layer into wallpaper render host when parent is `SHELLDLL_DefView`.
  - Added lifecycle restore: return icon list to original parent on normal detach.
  - Re-verified tests and rebuild after integration.
- Files created/modified:
  - src/win/wallpaper_host_win.cpp

### Phase 6: Desktop Attach Safety Iteration
- **Status:** complete
- Actions taken:
  - Added TDD red test set for desktop attach policy (`WorkerW` preferred, `DefView` fallback, reject unsafe fallback).
  - Added `desktop_attach_policy` module and switched host attach order to `WorkerW -> SHELLDLL_DefView`.
  - Removed `Progman`/`Desktop` fallback to prevent desktop icon occlusion and DPI-virtualized parent sizing.
  - Added `WM_SETCURSOR` handling with `IDC_ARROW` to avoid busy-cursor inheritance on wallpaper surface.
  - Fixed `main.cpp` DPI setup compile error (removed invalid constexpr pointer cast, added `SetProcessDPIAware` fallback).
  - Rebuilt app and probed runtime: render host parent changed from `Progman` (`1229x691`) to `SHELLDLL_DefView` (`1536x864`).
- Files created/modified:
  - include/wallpaper/desktop_attach_policy.h
  - src/desktop_attach_policy.cpp
  - tests/desktop_attach_policy_tests.cpp
  - src/win/wallpaper_host_win.cpp
  - src/main.cpp
  - scripts/run_tests.ps1
  - CMakeLists.txt

### Phase 6: Same-Video Reselect + Icon Layer Stability Iteration
- **Status:** complete
- Actions taken:
  - Added TDD tests for video path equivalence (`empty`, `relative/absolute`, `windows case+separator`, `different path`).
  - Added `video_path_matcher` module and switched `App::ApplyVideoPath` equality guard to canonical-equivalence comparison.
  - Updated wallpaper host to reinforce desktop icon layer visibility (`SysListView32` show + top) after attach and resize.
  - Enabled `WS_EX_TRANSPARENT` for wallpaper child host to reduce overlap side effects against desktop icon controls.
  - Re-ran full tests and rebuilt `wallpaper_app.exe`.
- Files created/modified:
  - include/wallpaper/video_path_matcher.h
  - src/video_path_matcher.cpp
  - tests/video_path_matcher_tests.cpp
  - src/app.cpp
  - src/win/wallpaper_host_win.cpp
  - scripts/run_tests.ps1
  - CMakeLists.txt

### Phase 6: 4K Memory + Z-Order Hardening Iteration
- **Status:** complete
- Actions taken:
  - Changed no-WorkerW fallback target from `DefView` to `Progman` and enforced `DefView` / `SysListView32` z-order above wallpaper host.
  - Updated desktop attach policy tests to `WorkerW -> Progman` fallback semantics.
  - Reused MF frame publish buffer to avoid per-frame heap allocation churn.
  - Enabled SourceReader low-latency mode and restricted stream selection to video only.
  - Verified runtime parent/size probe: wallpaper host is `Progman` child with full virtual desktop bounds (`1536x864` local env).
  - Verified runtime memory drop on 4K video from ~`760-830MB` to ~`300-360MB` in local measurements.
- Files created/modified:
  - src/win/wallpaper_host_win.cpp
  - include/wallpaper/desktop_attach_policy.h
  - src/desktop_attach_policy.cpp
  - tests/desktop_attach_policy_tests.cpp
  - src/win/decode_pipeline_stub.cpp

### Phase 6: Video No-Response Regression Fix Iteration
- **Status:** complete
- Actions taken:
  - Located regression root cause: decode path was active (`decode_mode=mf`), but fallback parent/z-order made wallpaper visually hidden under desktop shell composition.
  - Switched fallback attach target back to `SHELLDLL_DefView` and enforced `SysListView32` top z-order within same parent.
  - Fixed tray `WideToUtf8` conversion bug (null-terminator size mismatch) that could corrupt selected non-ASCII paths.
  - Rebuilt app and probed runtime: wallpaper host parent is `SHELLDLL_DefView`, with `SysListView32` visible and above wallpaper child.
- Files created/modified:
  - src/win/wallpaper_host_win.cpp
  - src/win/tray_controller_win.cpp
  - include/wallpaper/desktop_attach_policy.h
  - src/desktop_attach_policy.cpp
  - tests/desktop_attach_policy_tests.cpp

### Phase 6: Pause & Stutter Behavior Iteration
- **Status:** complete
- Actions taken:
  - Added foreground policy module and tests; shell window classes are now excluded from fullscreen pause decisions.
  - Removed maximized-window shortcut from foreground detection to avoid desktop clicks being treated as fullscreen.
  - Changed render path to present last frame when decoder has no new sample, reducing perceived stutter for 30fps video under 60fps schedule.
  - Ensured stale video texture is released when decode mode leaves MF path.
  - Re-ran tests and rebuilt app.
- Files created/modified:
  - include/wallpaper/foreground_policy.h
  - src/foreground_policy.cpp
  - tests/foreground_policy_tests.cpp
  - src/app.cpp
  - include/wallpaper/app.h
  - src/win/wallpaper_host_win.cpp

### Phase 6: GUI Subsystem + Cadence Cap Iteration
- **Status:** complete
- Actions taken:
  - Added `ApplyRenderFpsCap` path in app to combine governor cap with source cadence cap.
  - Added source timestamp delta heuristics (`30/60`) for MF decode frames to reduce render cadence mismatch jitter.
  - Updated metrics logging to persist actually applied fps cap (scheduler cap), not only governor output.
  - Added `scripts/build_app.ps1` with `-mwindows -O2` to build as Windows subsystem app (no console popup).
  - Updated CMake `wallpaper_app` target to `WIN32`.
- Files created/modified:
  - src/app.cpp
  - include/wallpaper/app.h
  - scripts/build_app.ps1
  - CMakeLists.txt

## Test Results
| Test | Input | Expected | Actual | Status |
|------|-------|----------|--------|--------|
| ResourceArbiter_PausesOnFullscreen | run_tests.ps1 | pause=true on fullscreen | pass | ✓ |
| ResourceArbiter_KeepsRunningOnNormalWindow | run_tests.ps1 | pause=false on windowed | pass | ✓ |
| RenderScheduler_RespectsFpsCap30 | run_tests.ps1 | render cadence at ~33ms | pass | ✓ |
| RenderScheduler_SwitchCapTo60 | run_tests.ps1 | cadence updates to ~16ms | pass | ✓ |
| ConfigStore_LoadsDefaultsWhenMissing | run_tests.ps1 | default config values | pass | ✓ |
| ConfigStore_RoundTripsCoreFields | run_tests.ps1 | persisted values match | pass | ✓ |
| ResourceArbiter_PausesWhenSessionInactive | run_tests.ps1 | pause=true when session inactive | pass | ✓ |
| ResourceArbiter_PausesWhenDesktopHidden | run_tests.ps1 | pause=true when desktop hidden | pass | ✓ |
| FrameBridge_PublishAndReadLatestFrame | run_tests.ps1 | latest frame retrievable after publish | pass | ✓ |
| FrameBridge_ClearRemovesFrame | run_tests.ps1 | no frame after clear | pass | ✓ |
| QualityGovernor_DegradesTo30OnHighLoad | run_tests.ps1 | high load downshift to 30fps | pass | ✓ |
| QualityGovernor_RecoversToTargetAfterStableSamples | run_tests.ps1 | stable samples recover to target fps | pass | ✓ |
| QualityGovernor_DisabledFollowsTargetFps | run_tests.ps1 | disabled governor keeps target fps | pass | ✓ |
| QualityGovernor_Target30AlwaysRemains30 | run_tests.ps1 | target 30fps remains 30 | pass | ✓ |
| MetricsLogFile_AppendsWithHeaderWhenMissing | run_tests.ps1 | create file with header and append row | pass | ✓ |
| MetricsLogFile_TruncatesToHeaderWhenExceedingLimit | run_tests.ps1 | recycle to header when exceeding cap | pass | ✓ |
| MetricsLogFile_SplitsByDayAndKeepsRecentShards | run_tests.ps1 | daily shards + keep recent days only | pass | ✓ |
| MetricsLogLine_HeaderIncludesExtendedColumns | run_tests.ps1 | header includes session and policy fields | pass | ✓ |
| MetricsLogLine_EncodesAllFields | run_tests.ps1 | row encodes metrics + policy context | pass | ✓ |
| MetricsLogLine_EncodesUnknownDecodeMode | run_tests.ps1 | unknown decode mode is explicitly encoded | pass | ✓ |
| Runtime_VerifyDecodeModeMfAfterFix | manual run + metrics tail | selected mp4 uses MF decode mode | pass | ✓ |
| Build_VerifyDesktopInteractionFix | run_tests.ps1 + app rebuild | no compile regression after WorkerW/message-loop fix | pass | ✓ |
| Build_VerifyAttachRobustness | run_tests.ps1 + app rebuild | no regression after multi-parent attach fallback | pass | ✓ |
| Build_VerifyDefViewFallback | run_tests.ps1 + app rebuild | no regression after DefView/listview composition fallback | pass | ✓ |
| DesktopAttachPolicy_PrefersWorkerWOverDefView | run_tests.ps1 | attach order keeps WorkerW first | pass | ✓ |
| DesktopAttachPolicy_UsesDefViewWhenWorkerWMissing | run_tests.ps1 | attach order falls back to DefView only | pass | ✓ |
| DesktopAttachPolicy_RejectsUnsafeFallbacks | run_tests.ps1 | no Progman/Desktop fallback target emitted | pass | ✓ |
| Runtime_VerifyRenderParentSwitch | app run + _probe_renderwin_by_pid | parent should be `SHELLDLL_DefView` and full-screen bounds | pass | ✓ |
| VideoPathMatcher_EmptyPathsAreSame | run_tests.ps1 | empty-equals-empty, empty-not-nonempty | pass | ✓ |
| VideoPathMatcher_RelativeAndAbsolutePathAreSame | run_tests.ps1 | same file path style variants are equivalent | pass | ✓ |
| VideoPathMatcher_DifferentPathIsNotSame | run_tests.ps1 | different video paths are not equivalent | pass | ✓ |
| VideoPathMatcher_WindowsCaseAndSeparatorInsensitive | run_tests.ps1 | Windows style and case differences still match same file | pass | ✓ |
| DesktopAttachPolicy_PrefersWorkerWOverProgman | run_tests.ps1 | attach order keeps WorkerW first and Progman fallback second | pass | ✓ |
| DesktopAttachPolicy_UsesProgmanWhenWorkerWMissing | run_tests.ps1 | attach order falls back to Progman only | pass | ✓ |
| Runtime_VerifyProgmanParentFullBounds | app run + _probe_renderwin_by_pid | parent should be `Progman` and full virtual bounds | pass | ✓ |
| Runtime_Verify4KMemoryAfterLowLatency | app run + process sampling | 4K playback memory stays around ~300-360MB | pass | ✓ |
| Runtime_VerifyDefViewParentAfterRegressionFix | app run + _probe_renderwin_by_pid | parent is `SHELLDLL_DefView` | pass | ✓ |
| Runtime_VerifyListViewAboveWallpaper | app run + _probe_def_children | `SysListView32` remains child[0], wallpaper child[1] | pass | ✓ |
| ForegroundPolicy_ShellClassesAreNotFullscreen | run_tests.ps1 | desktop shell classes must never trigger fullscreen pause | pass | ✓ |
| ForegroundPolicy_NormalWindowCanBeFullscreen | run_tests.ps1 | normal app class covering monitor is fullscreen | pass | ✓ |
| ForegroundPolicy_NotVisibleOrNotCoveringIsNotFullscreen | run_tests.ps1 | non-visible/non-covering foreground stays windowed | pass | ✓ |
| Runtime_VerifyEffectiveFpsRecovers60 | app run + metrics tail | effective fps recovers from 30 to 60 with dropped ratio 0 | pass | ✓ |
| BuildApp_WindowsSubsystemNoConsole | scripts/build_app.ps1 | startup should not show console window | pass | ✓ |

## Error Log
| Timestamp | Error | Attempt | Resolution |
|-----------|-------|---------|------------|
| 2026-03-27 22:24 | cmake not recognized | 1 | switched verification to g++ script |
| 2026-03-27 22:49 | MinGW link failed (`FOLDERID_LocalAppData`, `WinMain`) | 1 | switched to `SHGetFolderPathW` and added `main` + `wWinMain` |
| 2026-03-27 23:14 | tray module compile failure (`shellapi.h` types unresolved) | 1 | reordered includes, fixed icon resource usage |
| 2026-03-27 23:21 | MF path link failure (`GUID_NULL`) | 1 | replaced with local zero GUID |
| 2026-03-27 23:32 | `psapi.h` compile failure in app metrics | 1 | included `windows.h` before `psapi.h`, switched to portable `PROCESS_MEMORY_COUNTERS` |
| 2026-03-28 00:18 | tests script reported pass after compile errors | 1 | added compile exit code guard in `scripts/run_tests.ps1` |
| 2026-03-28 00:52 | daily shard retention test failed (old shard not pruned) | 1 | moved prune timing to run after active shard creation |
| 2026-03-28 11:08 | wallpaper rendered fallback color after selecting video | 1 | enabled MF Source Reader video processing/hardware transform attributes |
| 2026-03-28 11:20 | desktop icons hidden and cursor busy over wallpaper | 1 | corrected WorkerW target and added thread message pump |
| 2026-03-28 11:42 | wallpaper selected but no visible change on some shells | 1 | added multi-candidate parent attach and parent-relative placement |
| 2026-03-28 12:08 | wallpaper still invisible with decode active (`mf`) | 1 | added DefView fallback composition with icon list reparent |
| 2026-03-28 12:26 | `main.cpp` compile failed (`constexpr reinterpret_cast<HANDLE>(-4)`) | 1 | switched to runtime cast and added `SetProcessDPIAware` fallback |
| 2026-03-28 12:38 | render host attached to `Progman` with non-native bounds, causing visual mismatch | 1 | replaced attach fallback with `WorkerW -> DefView` policy and removed unsafe parent targets |
| 2026-03-28 13:02 | selecting same video could still trigger heavy reopen due path-format mismatch | 1 | introduced canonical path matcher and used equivalence guard in `ApplyVideoPath` |
| 2026-03-28 13:24 | 4K MF decode path kept memory near 800MB | 1 | enabled low-latency SourceReader + video-only stream + publish-buffer reuse; measured reduction to ~300-360MB |

## 5-Question Reboot Check
| Question | Answer |
|----------|--------|
| Where am I? | Phase 5 hardening in progress |
| Where am I going? | Next: settings window and live quality telemetry panel |
| What's the goal? | Performance-first Windows wallpaper app |
| What have I learned? | See findings.md |
| What have I done? | See above |

### Phase 7: 启动遮盖层与无效路径治理
- **Status:** in_progress
- Actions taken:
  - 复盘启动链路与渲染回退路径，确认三类问题的触发代码位于 `src/app.cpp` 与 `src/win/wallpaper_host_win.cpp`。
  - 形成提审方案：无效/缺失视频路径不附着壁纸层，首帧到达后再显示窗口。
  - 准备进入 TDD Red 阶段新增测试。
- Actions taken:
  - Red: 新增 `tests/startup_policy_tests.cpp` 并先跑出编译失败（缺失 `startup_policy`）。
  - Green: 新增 `include/wallpaper/startup_policy.h`、`src/startup_policy.cpp`，接入 `src/app.cpp`。
  - Green: 重构 App 启动与换源流程，引入 `EnsureWallpaperAttached/DetachWallpaper/StartVideoPipelineForPath`，无效路径降级到仅托盘运行。
  - Green: 修改 `src/win/wallpaper_host_win.cpp`，窗口默认隐藏，首个视频帧可绘制后再显示。
  - Verification: `./scripts/run_tests.ps1` 通过；`./scripts/build_app.ps1` 初次失败后修复源文件列表，再次构建通过。
- Files created/modified:
  - include/wallpaper/startup_policy.h
  - src/startup_policy.cpp
  - tests/startup_policy_tests.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - src/win/wallpaper_host_win.cpp
  - scripts/run_tests.ps1
  - scripts/build_app.ps1
  - CMakeLists.txt
### Phase 8: CPU/内存优化
- **Status:** complete
- Actions taken:
  - Red: 新增 `tests/loop_sleep_policy_tests.cpp` 与 `RenderScheduler_TimeUntilNextRenderTracksInterval` 测试。
  - Green: 新增 `include/wallpaper/loop_sleep_policy.h`、`src/loop_sleep_policy.cpp`。
  - Green: 在 `src/app.cpp` 主循环引入自适应睡眠；解码泵引入退避等待；`DetachWallpaper` 增加缓存清理。
  - Green: 为 `RenderScheduler` 增加 `TimeUntilNextRender()` 并在主循环接入。
  - Verification: `./scripts/run_tests.ps1` 通过；`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - include/wallpaper/loop_sleep_policy.h
  - src/loop_sleep_policy.cpp
  - tests/loop_sleep_policy_tests.cpp
  - include/wallpaper/render_scheduler.h
  - src/render_scheduler.cpp
  - tests/render_scheduler_tests.cpp
  - src/app.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1
  - scripts/build_app.ps1
### Phase 9: CPU/内存优化迭代二
- **Status:** complete
- Actions taken:
  - Red: 新增 `tests/probe_cadence_policy_tests.cpp`、`tests/frame_buffer_policy_tests.cpp`。
  - Green: 新增 `include/wallpaper/probe_cadence_policy.h`、`src/probe_cadence_policy.cpp` 并接入 `src/app.cpp`。
  - Green: 新增 `include/wallpaper/frame_buffer_policy.h`、`src/frame_buffer_policy.cpp` 并接入 `src/win/decode_pipeline_stub.cpp`。
  - Green: 扩展 `App` 缓存探测时间戳与缓存状态字段，避免高频重复探测。
  - Verification: `./scripts/run_tests.ps1` 与 `./scripts/build_app.ps1` 均通过。
- Files created/modified:
  - include/wallpaper/probe_cadence_policy.h
  - src/probe_cadence_policy.cpp
  - include/wallpaper/frame_buffer_policy.h
  - src/frame_buffer_policy.cpp
  - tests/probe_cadence_policy_tests.cpp
  - tests/frame_buffer_policy_tests.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - src/win/decode_pipeline_stub.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1
  - scripts/build_app.ps1
### Phase 10: CPU/内存优化迭代三
- **Status:** complete
- Actions taken:
  - Red: 修改 `tests/loop_sleep_policy_tests.cpp` 以定义更激进的退避目标并验证失败。
  - Green: 更新 `src/loop_sleep_policy.cpp`（主循环最大睡眠 20ms、解码泵最大退避 12ms）。
  - Green: 在 `src/app.cpp` 引入成员态 `wasPaused_`，将 pause 重置与缓存清理改为边沿触发。
  - Green: `ResetPlaybackState()` 增加 pause 状态复位，避免状态跨视频切换残留。
  - Verification: `./scripts/run_tests.ps1` 与 `./scripts/build_app.ps1` 均通过。
- Files created/modified:
  - tests/loop_sleep_policy_tests.cpp
  - src/loop_sleep_policy.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - task_plan.md
  - findings.md
  - progress.md
### Phase 11: 全屏资源释放优化
- **Status:** complete
- Actions taken:
  - Red: 新增 `tests/pause_resource_policy_tests.cpp`，并调高 `loop_sleep_policy` 目标阈值形成失败。
  - Green: 新增 `include/wallpaper/pause_resource_policy.h`、`src/pause_resource_policy.cpp`。
  - Green: 修改 `src/app.cpp`：暂停边沿执行 `decodePipeline->Stop()` 与 `DetachWallpaper()`，恢复边沿按配置 `StartVideoPipelineForPath()`。
  - Green: 调整 `src/loop_sleep_policy.cpp`（pause 80ms、decode not-ready 30ms）。
  - Verification: `./scripts/run_tests.ps1` 通过；`./scripts/build_app.ps1` 通过。
- Files created/modified:
  - include/wallpaper/pause_resource_policy.h
  - src/pause_resource_policy.cpp
  - tests/pause_resource_policy_tests.cpp
  - src/loop_sleep_policy.cpp
  - tests/loop_sleep_policy_tests.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1
  - scripts/build_app.ps1
  - task_plan.md
  - findings.md
  - progress.md
### Phase 12: 全屏静态壁纸无缝恢复
- **Status:** complete
- Actions taken:
  - Red: 在 `tests/pause_resource_policy_tests.cpp` 增加 `ShouldKeepWallpaperLayerDuringPause` 用例并验证编译失败。
  - Green: 在 `pause_resource_policy` 新增 `ShouldKeepWallpaperLayerDuringPause` 策略实现。
  - Green: 在 `src/app.cpp` pause 边沿改为“释放解码资源但按策略保留壁纸层”，不再一刀切 `DetachWallpaper()`。
  - Green: 仅在不保留壁纸层时清空 `lastPresentedFrame`，以维持静态壁纸观感。
  - Verification: `./scripts/run_tests.ps1` 与 `./scripts/build_app.ps1` 均通过。
- Files created/modified:
  - include/wallpaper/pause_resource_policy.h
  - src/pause_resource_policy.cpp
  - tests/pause_resource_policy_tests.cpp
  - src/app.cpp
  - task_plan.md
  - findings.md
  - progress.md
### Phase 13: 全屏识别鲁棒性修复
- **Status:** complete
- Actions taken:
  - Red: 在 `tests/foreground_policy_tests.cpp` 新增 `IsNearlyCoveringMonitor` 用例并验证链接失败。
  - Green: 在 `foreground_policy` 新增 `IsNearlyCoveringMonitor`，支持带容差的监视器覆盖判定。
  - Green: 在 `DetectForegroundState` 中增加 `DwmGetWindowAttribute(...DWMWA_EXTENDED_FRAME_BOUNDS...)` 路径，并接入容差判定（12px）。
  - Verification: `./scripts/run_tests.ps1` 通过；`./scripts/build_app.ps1` 因输出文件被占用失败；`./scripts/build_app.ps1 -BuildDir build_tmp` 通过。
- Files created/modified:
  - include/wallpaper/foreground_policy.h
  - src/foreground_policy.cpp
  - src/app.cpp
  - tests/foreground_policy_tests.cpp
  - task_plan.md
  - findings.md
  - progress.md
### Phase 14: 全屏暂停触发与恢复稳定性修复
- **Status:** complete
- Actions taken:
  - Red: 修改 `pause_resource_policy` 与 `foreground_policy` 相关测试，先验证失败。
  - Green: `ShouldKeepWallpaperLayerDuringPause` 改为只要壁纸层已附着就保持层级。
  - Green: `Tick` 中 pause 边沿改为仅释放解码资源，不再 Detach 壁纸层；resume 引入 `resumePipelinePending_` 自动重试。
  - Green: `ShouldTreatForegroundAsFullscreen` 不再以 `isVisible` 作为必要条件。
  - Verification: `./scripts/run_tests.ps1` 与 `./scripts/build_app.ps1` 均通过。
- Files created/modified:
  - include/wallpaper/app.h
  - src/app.cpp
  - src/foreground_policy.cpp
  - src/pause_resource_policy.cpp
  - tests/foreground_policy_tests.cpp
  - tests/pause_resource_policy_tests.cpp
  - task_plan.md
  - findings.md
  - progress.md
### Phase 15: 全屏识别迟钝修复
- **Status:** complete
- Actions taken:
  - Red: 调整 `foreground_policy` 与 `pause_resource_policy` 测试用例，先触发失败。
  - Green: 新增 `ComputeCoverageRatio`，在 `DetectForegroundState` 中引入覆盖率+样式联合判定。
  - Green: `Tick` 中前台状态更新改为“unknown 不覆盖缓存”。
  - Verification: `./scripts/run_tests.ps1` 通过；`./scripts/build_app.ps1` 与 `-BuildDir build_tmp` 均通过。
- Files created/modified:
  - include/wallpaper/foreground_policy.h
  - src/foreground_policy.cpp
  - src/app.cpp
  - tests/foreground_policy_tests.cpp
  - src/pause_resource_policy.cpp
  - tests/pause_resource_policy_tests.cpp
  - task_plan.md
  - findings.md
  - progress.md
### Phase 16: 最大化暂停开关
- **Status:** complete
- Actions taken:
  - Red: 修改 `tests/config_store_tests.cpp` 与 `tests/resource_arbiter_tests.cpp`，新增 `pauseOnMaximized` 与 `kMaximized` 行为测试并验证失败。
  - Green: 更新 `Config` 与 `ConfigStore` 的读写字段（`pauseOnMaximized`）。
  - Green: 扩展 `ForegroundState` 与 `ResourceArbiter`（新增 `kMaximized` 和 `SetPauseOnMaximized`）。
  - Green: `DetectForegroundState` 增加最大化识别，`App::Initialize` 接入新开关。
  - Verification: `./scripts/run_tests.ps1` 与 `./scripts/build_app.ps1` 均通过。
- Files created/modified:
  - include/wallpaper/config.h
  - src/config_store.cpp
  - tests/config_store_tests.cpp
  - include/wallpaper/resource_arbiter.h
  - src/resource_arbiter.cpp
  - tests/resource_arbiter_tests.cpp
  - src/app.cpp
  - task_plan.md
  - findings.md
  - progress.md
### Phase 17: 全屏检测精度与切换丝滑优化
- **Status:** complete
- Actions taken:
  - Red: 新增 `tests/pause_transition_policy_tests.cpp` 与 `foreground_policy` 新规则测试，并验证链接失败。
  - Green: 新增 `include/wallpaper/pause_transition_policy.h`、`src/pause_transition_policy.cpp`。
  - Green: `App` 接入 pause transition 状态机；`DetectForegroundState` 增加最小化/cloaked 过滤与 `IsLikelyFullscreenWindow` 规则。
  - Green: 更新 CMake 与脚本纳入新源文件和测试。
  - Verification: `./scripts/run_tests.ps1` 与 `./scripts/build_app.ps1` 均通过。
- Files created/modified:
  - include/wallpaper/pause_transition_policy.h
  - src/pause_transition_policy.cpp
  - tests/pause_transition_policy_tests.cpp
  - include/wallpaper/foreground_policy.h
  - src/foreground_policy.cpp
  - tests/foreground_policy_tests.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1
  - scripts/build_app.ps1
  - task_plan.md
  - findings.md
  - progress.md
### Phase 18: 桌面上下文驱动暂停
- **Status:** complete
- Actions taken:
  - Red: 扩展 `config_store` 与 `resource_arbiter` 测试，并新增 `desktop_context_policy` 测试；先验证失败。
  - Green: 新增 `desktop_context_policy` 模块，定义桌面上下文判定规则。
  - Green: `Config` 新增 `pauseWhenNotDesktopContext`，完成读写持久化。
  - Green: `ResourceArbiter` 新增 `SetPauseWhenNotDesktopContext/SetDesktopContextActive`，纳入暂停决策。
  - Green: `App::Tick` 前台探测接入桌面上下文状态，并与现有 pause transition 状态机协同。
  - Verification: `./scripts/run_tests.ps1` 与 `./scripts/build_app.ps1` 均通过。
- Files created/modified:
  - include/wallpaper/desktop_context_policy.h
  - src/desktop_context_policy.cpp
  - tests/desktop_context_policy_tests.cpp
  - include/wallpaper/config.h
  - src/config_store.cpp
  - tests/config_store_tests.cpp
  - include/wallpaper/resource_arbiter.h
  - src/resource_arbiter.cpp
  - tests/resource_arbiter_tests.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1
  - scripts/build_app.ps1
  - task_plan.md
  - findings.md
  - progress.md
### Phase 19: 配置收敛与切换丝滑优化
- **Status:** complete
- Actions taken:
  - Red: 更新 `config_store`/`resource_arbiter` 测试以删除旧配置语义，并新增 `pause_suspend_policy` 测试。
  - Green: `Config` 与 `ConfigStore` 移除 `pauseOnFullscreen/pauseOnMaximized` 读写，仅保留 `pauseWhenNotDesktopContext`。
  - Green: `ResourceArbiter` 移除 fullscreen/maximized 配置控制，仅依据 session/desktop/desktop-context 决策。
  - Green: `App` 接入二阶段暂停策略：软暂停立即执行，长暂停阈值后升级硬挂起。
  - Verification: `./scripts/run_tests.ps1` 与 `./scripts/build_app.ps1` 均通过。
- Files created/modified:
  - include/wallpaper/config.h
  - src/config_store.cpp
  - tests/config_store_tests.cpp
  - include/wallpaper/resource_arbiter.h
  - src/resource_arbiter.cpp
  - tests/resource_arbiter_tests.cpp
  - include/wallpaper/pause_suspend_policy.h
  - src/pause_suspend_policy.cpp
  - tests/pause_suspend_policy_tests.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1
  - scripts/build_app.ps1
  - task_plan.md
  - findings.md
  - progress.md
### Phase 20: 切换顿挫细化优化
- **Status:** complete
- Actions taken:
  - Green: `ResourceArbiter` 新增 `ShouldAllowHardSuspend()`，仅在 `sessionInactive` 或 `desktopHidden` 时允许硬挂起。
  - Green: `App::Tick` 轻暂停路径不再清空最后一帧与解码 token，保持静态帧连续性，减少切换“跳一下”的体感。
  - Green: 硬挂起阈值提升到 `8000ms`，并在非桌面上下文（常见全屏/分屏应用）场景禁止硬挂起，优先快速恢复。
  - Red/Verification: 扩展 `tests/resource_arbiter_tests.cpp`，验证 hard suspend 许可条件；`./scripts/run_tests.ps1` 全绿（67/67），`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - include/wallpaper/resource_arbiter.h
  - src/resource_arbiter.cpp
  - src/app.cpp
  - tests/resource_arbiter_tests.cpp
  - task_plan.md
  - findings.md
  - progress.md
### Phase 21: CPU/内存迭代优化四
- **Status:** complete
- Actions taken:
  - Green: 调整 `loop_sleep_policy`（pause 主循环 110ms、无视频 70ms、active 下限 2ms；decode no-ready 45ms、no-frame 退避上限 16ms）。
  - Green: 新增 `SelectRuntimeProbeIntervals`，暂停态将 session/foreground 探测间隔提升到 `1200ms/260ms`。
  - Green: `App` 引入 `stablePauseForLoopSleep_`，主循环睡眠不再用原始 pause 信号，避免抖动导致的无效唤醒。
  - Green: `IDecodePipeline` 新增 `TrimMemory()`；轻暂停超过 1500ms 后只回收解码帧缓存，不破坏软暂停恢复路径。
  - Verification: `./scripts/run_tests.ps1` 全绿（72/72）；`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - include/wallpaper/interfaces.h
  - src/platform_stubs.cpp
  - src/win/decode_pipeline_stub.cpp
  - include/wallpaper/loop_sleep_policy.h
  - src/loop_sleep_policy.cpp
  - tests/loop_sleep_policy_tests.cpp
  - include/wallpaper/probe_cadence_policy.h
  - src/probe_cadence_policy.cpp
  - tests/probe_cadence_policy_tests.cpp
  - include/wallpaper/pause_suspend_policy.h
  - src/pause_suspend_policy.cpp
  - tests/pause_suspend_policy_tests.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - task_plan.md
  - findings.md
  - progress.md
### Phase 22: 长静态内存回收与抖动抑制
- **Status:** complete
- Actions taken:
  - Green: `pause_suspend_policy` 新增 `SelectHardSuspendThreshold`，支持激进/保守两档深挂起阈值选择。
  - Green: `App::Tick` 中将非系统态（普通非桌面上下文）深挂起阈值设为 12s，长静态可释放 MF 解码链路。
  - Green: 深挂起后调用 `EmptyWorkingSet(GetCurrentProcess())`，让长静态内存下降更可见。
  - Green: 解码泵拿到帧后的 sleep 从 0ms 改为 2ms，降低长期动态时线程空转 CPU。
  - Verification: `./scripts/run_tests.ps1` 全绿（74/74）；`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - include/wallpaper/pause_suspend_policy.h
  - src/pause_suspend_policy.cpp
  - tests/pause_suspend_policy_tests.cpp
  - src/app.cpp
  - src/loop_sleep_policy.cpp
  - tests/loop_sleep_policy_tests.cpp
  - task_plan.md
  - findings.md
  - progress.md
### Phase 23: 深挂起恢复卡顿修复
- **Status:** complete
- Actions taken:
  - Green: `pause_suspend_policy` 新增 `ShouldWarmResumeDuringPause`，用于“离开 pause 且已深挂起”条件下的预热判定。
  - Green: `App::Tick` 在仍处于 pause 迟滞窗口时预热 `decodePipeline->Open()`，恢复时只执行 `Start()`，降低恢复卡顿。
  - Green: 深挂起恢复期间 exit delay 从 180ms 动态提升到 360ms，为预热留出缓冲；若用户再次回到暂停态，自动回收预热资源。
  - Verification: `./scripts/run_tests.ps1` 全绿（75/75）；`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - include/wallpaper/pause_suspend_policy.h
  - src/pause_suspend_policy.cpp
  - tests/pause_suspend_policy_tests.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - task_plan.md
  - findings.md
  - progress.md
### Phase 24: 动态高负载与恢复顿挫联合优化
- **Status:** complete
- Actions taken:
  - Green: 新增 `ComputeDecodePumpHotSleepMs` 并接入 `ApplyRenderFpsCap`，按渲染上限动态设置解码泵有帧睡眠（60fps=4ms, 30fps=8ms）。
  - Green: 修复 `StartDecodePump` 的关键路径：拿到帧后也执行 sleep，避免解码线程长期忙轮询导致 CPU 偏高。
  - Green: 深挂起恢复预热从“仅 Open”升级为“Open 后提前 Start”，在退出 pause 时直接进入连续解码。
  - Verification: `./scripts/run_tests.ps1` 全绿（76/76）；`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - include/wallpaper/loop_sleep_policy.h
  - src/loop_sleep_policy.cpp
  - tests/loop_sleep_policy_tests.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - task_plan.md
  - findings.md
  - progress.md
### Phase 25: 清除/重选/退出卡顿与托盘状态可视化
- **Status:** complete
- Actions taken:
  - Green: `ITrayController` 新增 `UpdateMenuState`，`App` 新增 `SyncTrayMenuState` 并在初始化与配置变更后同步真实状态。
  - Green: Windows 托盘菜单增加实时勾选与灰置逻辑（Set 30/60、Enable/Disable Auto Start、Enable/Disable Adaptive Quality、Clear Video）。
  - Green: `decode_pipeline_stub` 停止路径去除频繁 `MFShutdown`，保留 runtime 常驻以降低 Clear/Select/Exit 的同步阻塞。
  - Verification: `./scripts/run_tests.ps1` 全绿（76/76）；`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - include/wallpaper/interfaces.h
  - include/wallpaper/app.h
  - src/app.cpp
  - src/platform_stubs.cpp
  - src/win/tray_controller_win.cpp
  - src/win/decode_pipeline_stub.cpp
  - task_plan.md
  - progress.md
  - findings.md
### Phase 26: 托盘交互误暂停修复与切换顺滑性优化
- **Status:** complete
- Actions taken:
  - Green: `TryDetectDesktopContextActive` 新增“前台窗口属于当前进程”判定，避免托盘菜单/文件对话框被误判为非桌面上下文。
  - Green: `App` 新增托盘交互保护窗口（1.2s），在短时菜单交互后强制 `desktopContextActive=true` 抑制抖动。
  - Green: `desktop_context_policy` 增加 `WallpaperTrayMessageWindow` 类别判定兜底。
  - Green: `ApplyVideoPath` 去除非空切换前的冗余 `Stop`，减少 Stop/Open 双重重置引发的顿挫。
  - Verification: `./scripts/run_tests.ps1` 全绿（76/76）；`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - src/app.cpp
  - include/wallpaper/app.h
  - src/desktop_context_policy.cpp
  - tests/desktop_context_policy_tests.cpp
  - task_plan.md
  - progress.md
  - findings.md
### Phase 27: 托盘菜单动态误切修复与退出切换降卡顿
- **Status:** complete
- Actions taken:
  - Green: `TrayActionType` 新增 `kMenuOpened/kMenuClosed`，托盘菜单显示周期显式上报给主循环。
  - Green: `Tick` 中引入 `suppressDesktopContextProbe`（菜单可见或交互冻结窗口内），仅冻结探测不改写当前上下文，修复“打开托盘菜单就切动态”。
  - Green: 取消托盘交互后的“强制 desktopContext=true”逻辑，避免状态被错误重置。
  - Green: 解码泵睡眠改为 `condition_variable` 可中断等待，`StopDecodePump` 通过 `WakeDecodePump()` 立即唤醒，降低退出和切换卡顿。
  - Verification: `./scripts/run_tests.ps1` 全绿（76/76）；`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - include/wallpaper/interfaces.h
  - include/wallpaper/app.h
  - src/app.cpp
  - src/win/tray_controller_win.cpp
  - task_plan.md
  - progress.md
  - findings.md
### Phase 28: 长动态负载压降与恢复唤醒优化
- **Status:** complete
- Actions taken:
  - Green: `loop_sleep_policy` 调整解码泵 no-ready 睡眠到 70ms，hot-sleep 调整为 60fps=6ms、30fps=12ms。
  - Green: 为降低 hot-sleep 带来的恢复迟滞，`App` 在视频启动/恢复/预热启动路径统一 `WakeDecodePump()`。
  - Green: `StopDecodePump` 保持即时唤醒停止，确保退出无额外等待放大。
  - Verification: `./scripts/run_tests.ps1` 全绿（76/76）；`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - src/loop_sleep_policy.cpp
  - tests/loop_sleep_policy_tests.cpp
  - src/app.cpp
  - task_plan.md
  - progress.md
  - findings.md
### Phase 29: 全量性能迭代（长动态 CPU + 切换丝滑）
- **Status:** complete
- Actions taken:
  - Red: 修改 `tests/loop_sleep_policy_tests.cpp` 与 `tests/probe_cadence_policy_tests.cpp` 目标值并验证失败。
  - Green: `loop_sleep_policy` hot-sleep 调整为 60fps=8ms、30fps=16ms，降低长动态解码线程轮询频率。
  - Green: `probe_cadence_policy` 运行态探测调整为 session=450ms、foreground=180ms，压低 Win32 高频探测开销。
  - Green: `App` 解码泵等待改为支持显式唤醒信号，修复运行态 `WakeDecodePump()` 不能立即打断 wait 的问题。
  - Green: `TryDetectDesktopContextActive` 增加桌面类窗口快速返回、本进程窗口快速返回，并给进程名查询增加 PID 缓存。
  - Verification: `./scripts/run_tests.ps1` 全绿（76/76）；`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - src/loop_sleep_policy.cpp
  - src/probe_cadence_policy.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - tests/loop_sleep_policy_tests.cpp
  - tests/probe_cadence_policy_tests.cpp
  - task_plan.md
  - progress.md
  - findings.md
### Phase 30: 非桌面启动快速降载优化
- **Status:** complete
- Actions taken:
  - Red: 在 `tests/probe_cadence_policy_tests.cpp` 增加前台探测失败计数与保守降载判定测试并验证失败。
  - Green: 在 `probe_cadence_policy` 新增 `UpdateForegroundProbeFailureStreak` 与 `ShouldUseConservativeDesktopContext`。
  - Green: 在 `App::Tick` 接入前台探测失败计数；失败达到阈值（3）后强制 `desktopContextActive=false`。
  - Green: 探测失败未达阈值时不更新 `lastForegroundProbeAt_`，允许快速重试，减少启动阶段误保持动态时长。
  - Verification: `./scripts/run_tests.ps1` 全绿（79/79）；`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - include/wallpaper/probe_cadence_policy.h
  - src/probe_cadence_policy.cpp
  - tests/probe_cadence_policy_tests.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - task_plan.md
  - progress.md
  - findings.md
### Phase 31: 长时间运行高负载治理
- **Status:** complete
- Actions taken:
  - Red: 新增 `tests/long_run_load_policy_tests.cpp` 并验证缺失实现导致失败。
  - Green: 新增 `include/wallpaper/long_run_load_policy.h` 与 `src/long_run_load_policy.cpp`，实现长期压力迟滞与内存 trim 冷却策略。
  - Green: `App` 增加 `decodePumpDynamicBoostMs` 与 `LongRunLoadState`，每秒按指标动态调节 decode hot-sleep。
  - Green: `ApplyRenderFpsCap` 叠加长期负载增量并限制上限（4~36ms）。
  - Green: `decode_pipeline_stub` 运行态 trim 改为延迟收缩（`trimRequested_`），减少可见闪烁。
  - Green: 更新 CMake 与 PowerShell 构建脚本纳入新模块与测试。
  - Verification: `./scripts/run_tests.ps1` 全绿（83/83）；`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - include/wallpaper/long_run_load_policy.h
  - src/long_run_load_policy.cpp
  - tests/long_run_load_policy_tests.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - src/win/decode_pipeline_stub.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1
  - scripts/build_app.ps1
  - task_plan.md
  - progress.md
  - findings.md
### Phase 32: 官方最佳实践驱动的 CPU/内存优化
- **Status:** complete
- Actions taken:
  - Red: 在 `tests/loop_sleep_policy_tests.cpp` 新增高精度计时器启用策略测试，并验证缺失实现导致失败。
  - Green: `loop_sleep_policy` 新增 `ShouldUseHighResolutionTimer`，仅在活跃 60fps 且低压力状态启用高精度计时器。
  - Green: `App::Run` 将高精度计时器改为动态开关，退出 60fps 动态场景时自动 `timeEndPeriod`。
  - Green: `wallpaper_host_win` 初始化阶段调用 `IDXGIDevice1::SetMaximumFrameLatency(1)`，限制 DXGI 帧队列。
  - Green: 视频纹理上传路径改为 `D3D11_USAGE_DYNAMIC + Map(WRITE_DISCARD)`，减少长期热路径拷贝开销。
  - Verification: `./scripts/run_tests.ps1` 全绿（85/85）；`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - include/wallpaper/loop_sleep_policy.h
  - src/loop_sleep_policy.cpp
  - tests/loop_sleep_policy_tests.cpp
  - src/app.cpp
  - src/win/wallpaper_host_win.cpp
  - task_plan.md
  - findings.md
  - progress.md
### Phase 33: 渲染链路进一步降耗
- **Status:** complete
- Actions taken:
  - Green: `wallpaper_host_win` 的 D3D 设备创建 flags 切到 `D3D11_CREATE_DEVICE_SINGLETHREADED`，减少单线程场景锁开销。
  - Green: swapchain `SwapEffect` 优先 `DXGI_SWAP_EFFECT_FLIP_DISCARD`，若创建失败回退 `DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL`。
  - Verification: `./scripts/run_tests.ps1` 全绿（85/85）；`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - src/win/wallpaper_host_win.cpp
  - task_plan.md
  - findings.md
  - progress.md
### Phase 34: DXGI 等待对象降载优化
- **Status:** complete
- Actions taken:
  - Green: `wallpaper_host_win` 引入 `dxgi1_3.h`，swapchain 创建优先启用 `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT`。
  - Green: 成功创建后查询 `IDXGISwapChain2`，设置 `SetMaximumFrameLatency(1)` 并获取 `GetFrameLatencyWaitableObject` 句柄。
  - Green: `Present` 前执行 `WaitForSingleObjectEx(..., 0)` 非阻塞检查；队列未就绪时跳过本帧提交，降低无效 CPU 消耗。
  - Green: `ReleaseD3D` 关闭 waitable handle 并释放 `IDXGISwapChain2`。
  - Verification: `./scripts/run_tests.ps1` 全绿（85/85）；`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - src/win/wallpaper_host_win.cpp
  - task_plan.md
  - findings.md
  - progress.md
### Phase 35: waitable-object 回归修复
- **Status:** complete
- Actions taken:
  - Red: 新增 `tests/frame_latency_policy_tests.cpp`，并在未实现 `src/frame_latency_policy.cpp` 时确认编译失败。
  - Green: 新增 `frame_latency_policy`，定义 wait timeout 跳过规则（阈值前跳过，阈值后强制提交）。
  - Green: `wallpaper_host_win` 改为“首帧成功后再启用门控”，并增加连续超时计数与 `WAIT_FAILED` 自动降级。
  - Green: 将新模块接入 CMake、`scripts/run_tests.ps1`、`scripts/build_app.ps1`。
  - Verification: `./scripts/run_tests.ps1` 全绿（89/89）；`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - include/wallpaper/frame_latency_policy.h
  - src/frame_latency_policy.cpp
  - tests/frame_latency_policy_tests.cpp
  - src/win/wallpaper_host_win.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1
  - scripts/build_app.ps1
  - task_plan.md
  - findings.md
  - progress.md
### Phase 36: 稳定优先回滚
- **Status:** complete
- Actions taken:
  - Green: 在 `wallpaper_host_win` 增加 `kEnableFrameLatencyWaitableObject=false`，默认关闭 waitable-object 路径。
  - Green: swapchain 创建标志改为条件启用 waitable flag，默认不携带该标志。
  - Green: 保留 waitable 代码以便后续按设备白名单逐步开放。
  - Verification: `./scripts/run_tests.ps1` 全绿（89/89）；`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - src/win/wallpaper_host_win.cpp
  - task_plan.md
  - findings.md
  - progress.md
### Phase 37: 图标修复与遮挡降载
- **Status:** complete
- Actions taken:
  - Green: `resources/app_icon.rc` 增加 `1 ICON` 标准资源 ID，并保留 `101 ICON` 兼容既有托盘加载路径。
  - Green: `tray_controller_win` 和 `wallpaper_host_win` 的窗口类注册补齐 `hIcon/hIconSm`。
  - Green: `wallpaper_host_win` 增加 occlusion 处理：`DXGI_STATUS_OCCLUDED` 时暂停提交并按 250ms 节流探测恢复。
  - Verification: `./scripts/run_tests.ps1` 全绿（89/89）；`./scripts/build_app.ps1` 构建通过。
- Files created/modified:
  - resources/app_icon.rc
  - src/win/tray_controller_win.cpp
  - src/win/wallpaper_host_win.cpp
  - task_plan.md
  - findings.md
  - progress.md
### Phase 38: CPU/内存卡点专项治理与图标链路修复
- **Status:** complete
- Actions taken:
  - 修复 `LongRunLoadPolicy` 冷却迟滞测试与新阈值不一致问题，恢复全绿。
  - 将 `QualityGovernor` 与 `LongRunLoadPolicy` 负载阈值改为匹配真实进程 CPU 量纲（约 0~12% 区间），避免“阈值过高导致策略长期不触发”。
  - 调整 `ComputeMainLoopSleepMs` 上限（24ms -> 40ms）与 `ComputeDecodePumpHotSleepMs`（60fps=14ms, 30fps=28ms），消除 30fps 下仍高频拉帧的隐藏开销。
  - `decode_pipeline_stub` 引入 SourceReader 双路径：先尝试 `MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS`（不启用软件视频处理），失败再回退 `MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING`，兼顾降载与兼容。
  - 托盘与窗口类图标加载改为 `LoadImage` 按系统图标尺寸取 `hIcon/hIconSm`，减少任务管理器回退默认图标概率。
- Verification:
  - `./scripts/run_tests.ps1` -> 89/89 PASS
  - `./scripts/build_app.ps1` -> build/wallpaper_app.exe 成功
- Files created/modified:
  - src/quality_governor.cpp
  - src/long_run_load_policy.cpp
  - src/loop_sleep_policy.cpp
  - src/win/decode_pipeline_stub.cpp
  - src/win/tray_controller_win.cpp
  - src/win/wallpaper_host_win.cpp
  - tests/quality_governor_tests.cpp
  - tests/long_run_load_policy_tests.cpp
  - tests/loop_sleep_policy_tests.cpp
  - assets/app_icon.ico
  - task_plan.md
  - progress.md
  - findings.md
### Phase 39: DXVA主路径与指标口径重构
- **Status:** complete
- Actions taken:
  - `RuntimeMetrics` 拆分 `privateBytes` 与 `workingSetBytes`，修复“private_bytes 实际记录 working set”口径偏差。
  - `metrics_log_line` 扩展字段：`decode_path`、`working_set_bytes`、`long_run_level`、`decode_hot_sleep_ms`、`decode_copy_bytes_per_sec`。
  - `frame_bridge` 新增 GPU 帧发布（纹理句柄+格式+subresource）与 CPU 外部缓冲视图发布接口。
  - `decode_pipeline_stub` 接入 `MF_SOURCE_READER_D3D_MANAGER`（共享 D3D 设备）并优先尝试 D3D 互操作路径。
  - CPU 回退路径移除中间 `vector memcpy`，改为锁定 MF buffer 并通过桥接直传，减少一份 CPU 拷贝。
  - `wallpaper_host_win` 渲染路径改为“GPU帧优先（CopySubresourceRegion）/CPU帧回退（Map）”。
- Verification:
  - `./scripts/run_tests.ps1` -> 90/90 PASS
  - `./scripts/build_app.ps1` -> build/wallpaper_app.exe 成功
- Runtime spot-check:
  - 新日志字段已生效，能直接观测 `decode_path` 与 `decode_copy_bytes_per_sec`。
### Phase 40: CPU-only回退链路降载迭代
- **Status:** complete
- Actions taken:
  - `App::ApplyRenderFpsCap` 在 `cpu_rgb32_fallback` 路径下增加额外 hot-sleep（60fps:+10ms, 30fps:+14ms），并将总上限从 36ms 提升到 64ms。
  - `LongRunLoadPolicy` 私有内存阈值下调（80/100/104MB），让 medium/high/trim 更早触发。
  - `DecodePipeline::TrimMemory` 在运行态对 SourceReader 执行 `Flush`，并在 CPU 回退路径释放桥接帧，减少高压期驻留。
  - `MaybeSampleAndLogMetrics` 增加 CPU 回退场景下的 working-set 受控回收（>=100MB 且 long-run>=1，15s 节流）。
- Verification:
  - `./scripts/run_tests.ps1` -> 90/90 PASS
  - `./scripts/build_app.ps1` -> build/wallpaper_app.exe 成功
- Runtime spot-check:
  - 日志已体现 `decode_hot_sleep_ms` 在 CPU 回退路径提高到 42/24。
  - 回退链路短测工作集出现从 ~115MB 回落到 ~74MB 的样本（受场景影响会波动）。
### Phase 41: 黑幕回归修复与主循环trim安全化
- **Status:** complete
- Actions taken:
  - Red: 新增 `tests/runtime_trim_policy_tests.cpp` 并更新 `scripts/run_tests.ps1`，首次编译失败确认 `runtime_trim_policy` 缺失（真实红灯）。
  - Green: 新增 `include/wallpaper/runtime_trim_policy.h` 与 `src/runtime_trim_policy.cpp`，主循环 trim 改为策略门控。
  - Green: `src/app.cpp` 接入 `ShouldExecuteLongRunDecodeTrim(...)`，阻断 CPU 回退动态路径的主循环 trim。
  - Green: `src/win/decode_pipeline_stub.cpp` 运行态 trim 去掉 CPU 路径 `ClearLatestFrame`，并将 `Flush` 限制在 GPU 零拷贝路径。
  - Green: 同步构建清单：`CMakeLists.txt`、`scripts/run_tests.ps1`、`scripts/build_app.ps1` 增加 `runtime_trim_policy` 源/测试。
- Verification:
  - `./scripts/run_tests.ps1` -> 94/94 PASS
  - `./scripts/build_app.ps1` -> build/wallpaper_app.exe 成功
- Files created/modified:
  - include/wallpaper/runtime_trim_policy.h
  - src/runtime_trim_policy.cpp
  - src/app.cpp
  - src/win/decode_pipeline_stub.cpp
  - tests/runtime_trim_policy_tests.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1
  - scripts/build_app.ps1
  - task_plan.md
  - findings.md
  - progress.md
### Phase 42: 自适应质量升级为“帧率+分辨率”联合治理
- **Status:** complete
- Actions taken:
  - Red: 新增 `tests/decode_output_policy_tests.cpp`，并通过 `scripts/run_tests.ps1` 验证缺失实现导致编译失败。
  - Green: 新增 `include/wallpaper/decode_output_policy.h` 与 `src/decode_output_policy.cpp`，实现 CPU 回退路径自适应限像素策略。
  - Green: `IDecodePipeline::Open` 增加 `adaptiveQuality` 参数，联动更新 `src/app.cpp`、`src/win/decode_pipeline_stub.cpp`、`src/platform_stubs.cpp`。
  - Green: 托盘启用/禁用自适应质量时，若视频正在运行则热重开管线，确保 MF 输出尺寸策略立即生效。
  - Green: `App::StartDecodePump` 接入 Windows 线程优先级调度（活跃解码 below-normal，空闲 idle）。
  - Green: 构建/测试脚本与 CMake 清单补齐新增源与测试文件。
- Verification:
  - `./scripts/run_tests.ps1` -> 99/99 PASS
  - `./scripts/build_app.ps1` -> build/wallpaper_app.exe 成功
- Files created/modified:
  - include/wallpaper/decode_output_policy.h
  - src/decode_output_policy.cpp
  - tests/decode_output_policy_tests.cpp
  - include/wallpaper/interfaces.h
  - src/win/decode_pipeline_stub.cpp
  - src/platform_stubs.cpp
  - src/app.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1
  - scripts/build_app.ps1
  - task_plan.md
  - findings.md
  - progress.md
### Phase 43: 长时运行系统态仲裁与 CPU 回退进一步压降
- **Status:** complete
- Actions taken:
  - Red: 扩展 `tests/decode_output_policy_tests.cpp`、`tests/long_run_load_policy_tests.cpp`、`tests/resource_arbiter_tests.cpp`、`tests/loop_sleep_policy_tests.cpp`、`tests/metrics_log_line_tests.cpp`，确认新增接口与行为先失败再实现。
  - Green: `DecodeOpenProfile` 接入 `IDecodePipeline::Open`，并在 `App` / Win / stub 贯通；CPU 回退输出策略升级为 `720p -> 540p` 双档。
  - Green: `LongRunLoadPolicy` 新增 `decode_path` 维度；CPU 回退路径对 medium/high 压力输出更激进的 hot-sleep boost。
  - Green: `ResourceArbiter` 新增 `battery saver` / `remote session` / `display off` 电源态，并暴露 `CurrentPowerState()`。
  - Green: `RuntimeMetrics` 与 CSV 日志新增 `decode_output_pixels`、`thread_qos`、`occluded`、`power_state`。
  - Green: decode 线程接入 `SetThreadInformation(ThreadPowerThrottling/ThreadMemoryPriority)`，常态走 EcoQoS + low memory priority；壁纸宿主新增 `IsOccluded()` 并接入 App 采样/仲裁。
- Verification:
  - `./scripts/run_tests.ps1` -> 106/106 PASS
  - `./scripts/build_app.ps1` -> build/wallpaper_app.exe 成功
- Files modified:
  - include/wallpaper/app.h
  - include/wallpaper/decode_output_policy.h
  - include/wallpaper/interfaces.h
  - include/wallpaper/long_run_load_policy.h
  - include/wallpaper/loop_sleep_policy.h
  - include/wallpaper/metrics_sampler.h
  - include/wallpaper/resource_arbiter.h
  - src/app.cpp
  - src/decode_output_policy.cpp
  - src/long_run_load_policy.cpp
  - src/loop_sleep_policy.cpp
  - src/metrics_log_line.cpp
  - src/platform_stubs.cpp
  - src/resource_arbiter.cpp
  - src/win/decode_pipeline_stub.cpp
  - src/win/wallpaper_host_win.cpp
  - tests/decode_output_policy_tests.cpp
  - tests/long_run_load_policy_tests.cpp
  - tests/loop_sleep_policy_tests.cpp
  - tests/metrics_log_line_tests.cpp
  - tests/resource_arbiter_tests.cpp
  - task_plan.md
  - findings.md
  - progress.md
### Phase 44: 低帧率源感知降唤醒
- **Status:** complete
- Actions taken:
  - Red: 新增 `tests/source_frame_rate_policy_tests.cpp`，并扩展 `tests/loop_sleep_policy_tests.cpp` 覆盖 24/25fps 低帧率热睡眠。
  - Green: 新增 `include/wallpaper/source_frame_rate_policy.h` 与 `src/source_frame_rate_policy.cpp`，实现 24/25/30/60fps 时间戳识别与迟滞。
  - Green: `ComputeDecodePumpHotSleepMs` 改为 `renderFpsCap + sourceFps` 双参策略，24/25fps 素材下降 decode pump 长期唤醒频率。
  - Green: `App` 侧移除 `sourceFpsHint30/sourceFpsHint60`，统一改用 `SourceFrameRateState` 驱动 cadence。
  - Green: 构建/测试清单补齐 `source_frame_rate_policy` 新增源与测试文件。
- Verification:
  - `./scripts/run_tests.ps1` -> 112/112 PASS
  - `./scripts/build_app.ps1` -> build/wallpaper_app.exe 成功
- Files modified:
  - include/wallpaper/source_frame_rate_policy.h
  - src/source_frame_rate_policy.cpp
  - include/wallpaper/loop_sleep_policy.h
  - src/loop_sleep_policy.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - tests/source_frame_rate_policy_tests.cpp
  - tests/loop_sleep_policy_tests.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1
  - scripts/build_app.ps1
  - task_plan.md
  - findings.md
  - progress.md
### Phase 45: MF 异步单请求缓存化解码
- **Status:** complete
- Actions taken:
  - Red: 新增 `tests/decode_async_read_policy_tests.cpp`，并更新 `CMakeLists.txt`、`scripts/run_tests.ps1`、`scripts/build_app.ps1`，首次编译失败确认 `decode_async_read_policy` 缺失。
  - Green: 新增 `include/wallpaper/decode_async_read_policy.h` 与 `src/decode_async_read_policy.cpp`，实现单请求状态机：启动 prime、sample ready、pause、reset、EOF seek。
  - Green: `src/win/decode_pipeline_stub.cpp` 接入 `MF_SOURCE_READER_ASYNC_CALLBACK` 与 `IMFSourceReaderCallback`，改为异步回调缓存 `IMFSample`。
  - Green: `TryAcquireLatestFrame` 改为消费缓存样本并在消费后补发下一次异步请求，不再在轮询热路径里同步 `ReadSample`。
  - Green: 运行态 `TrimMemory()` 去掉 Source Reader flush，避免异步 in-flight request 与 flush 交错。
- Verification:
  - `./scripts/run_tests.ps1` -> 117/117 PASS
  - `./scripts/build_app.ps1` -> build/wallpaper_app.exe 成功
- Files modified:
  - include/wallpaper/decode_async_read_policy.h
  - src/decode_async_read_policy.cpp
  - src/win/decode_pipeline_stub.cpp
  - tests/decode_async_read_policy_tests.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1
  - scripts/build_app.ps1
  - task_plan.md
  - findings.md
  - progress.md

### Phase 46: CPU-only NV12 回退链路打通
- **Status:** complete
- Actions taken:
  - Green: `src/win/decode_pipeline_stub.cpp` 在非 D3D 互操作路径优先协商 `MFVideoFormat_NV12`，并在发布阶段优先走 `IMF2DBuffer::Lock2D` 直连平面视图，失败时回退连续缓冲。
  - Green: `src/win/wallpaper_host_win.cpp` 新增 NV12 双平面动态纹理（`R8_UNORM` + `R8G8_UNORM`）和专用像素着色器，避免 CPU 侧先转 RGBA。
  - Green: `src/app.cpp` 把 `kCpuNv12Fallback` 纳入 CPU fallback 统一判定，复用现有 hot-sleep、working-set trim、adaptive reopen 降载策略。
- Verification:
  - `./scripts/run_tests.ps1` -> 122/122 PASS
  - `./scripts/build_app.ps1` -> build/wallpaper_app.exe 成功
- Files modified:
  - src/win/decode_pipeline_stub.cpp
  - src/win/wallpaper_host_win.cpp
  - src/app.cpp
  - task_plan.md
  - findings.md
  - progress.md

### Phase 47: NV12 顶部绿条修复与对齐布局收敛
- **Status:** complete
- Actions taken:
  - Red: 新增 `include/wallpaper/nv12_layout_policy.h`、`tests/nv12_layout_policy_tests.cpp`，首次执行 `./scripts/run_tests.ps1` 因 `src/nv12_layout_policy.cpp` 缺失而编译失败，确认红灯真实存在。
  - Green: 新增 `src/nv12_layout_policy.cpp`，按 `frameHeight / pitch / totalBytes` 推导 NV12 对齐布局，覆盖“可见高度小于对齐高度”的 UV 偏移场景。
  - Green: `src/win/decode_pipeline_stub.cpp` 的 NV12 `Lock2D` 路径改为只在单 buffer sample 时启用，并使用 `sample->GetTotalLength()` + `ComputeNv12Layout(...)` 计算真实 UV 起点；多 buffer sample 自动回退连续缓冲。
  - Green: 构建清单补齐 `nv12_layout_policy` 新源与测试文件。
- Verification:
  - `./scripts/run_tests.ps1` -> 125/125 PASS
  - `./scripts/build_app.ps1` -> build/wallpaper_app.exe 成功
- Files modified:
  - include/wallpaper/nv12_layout_policy.h
  - src/nv12_layout_policy.cpp
  - tests/nv12_layout_policy_tests.cpp
  - src/win/decode_pipeline_stub.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1
  - scripts/build_app.ps1
  - task_plan.md
  - findings.md
  - progress.md

### Phase 48: 渲染参数稳定态去唤醒优化
- **Status:** complete
- Actions taken:
  - Red: 在 `tests/loop_sleep_policy_tests.cpp` 新增 `LoopSleepPolicy_DecodePumpWakeDependsOnRenderCapChange`，首次执行 `./scripts/run_tests.ps1` 编译失败（`ShouldWakeDecodePumpForRenderCapUpdate` 未定义）。
  - Green: 在 `include/wallpaper/loop_sleep_policy.h` / `src/loop_sleep_policy.cpp` 新增 `ShouldWakeDecodePumpForRenderCapUpdate`。
  - Green: 在 `src/app.cpp` 的 `ApplyRenderFpsCap` 中改为条件更新与条件唤醒，避免稳定态重复 `WakeDecodePump()`。
- Verification:
  - `./scripts/run_tests.ps1` -> 126/126 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp` -> build_tmp/wallpaper_app.exe 成功
- Files modified:
  - include/wallpaper/loop_sleep_policy.h
  - src/loop_sleep_policy.cpp
  - src/app.cpp
  - tests/loop_sleep_policy_tests.cpp
  - task_plan.md
  - findings.md
  - progress.md

### Phase 49: 互斥热点与退避节奏迭代优化
- **Status:** complete
- Actions taken:
  - Red: 在 `tests/frame_bridge_tests.cpp` 新增 `FrameBridge_TryGetLatestFrameIfNewerSkipsSameSequence`，首次执行 `./scripts/run_tests.ps1` 编译失败（`TryGetLatestFrameIfNewer` 未定义）。
  - Red: 调整 `tests/loop_sleep_policy_tests.cpp` 的 decode pump 无帧退避期望，锁定更激进降唤醒策略。
  - Green: `include/wallpaper/frame_bridge.h` / `src/frame_bridge.cpp` 增加 `TryGetLatestFrameIfNewer`，并引入原子序列号快判。
  - Green: `src/win/wallpaper_host_win.cpp` 改为 `TryGetLatestFrameIfNewer(lastVideoSequence_, ...)`。
  - Green: `src/loop_sleep_policy.cpp` 将无帧退避改为 `2->4->8->(+4)`，上限 24ms。
  - Green: `src/app.cpp` 对 `presentSamplesMs_` 预留容量并移除 `shrink_to_fit`。
  - Green: `src/win/decode_pipeline_stub.cpp` 移除 `Locked2DBufferHolder` 的每帧 `new`，改为别名持有资源。
- Verification:
  - `./scripts/run_tests.ps1` -> 127/127 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp` -> build_tmp/wallpaper_app.exe 成功
- Files modified:
  - include/wallpaper/frame_bridge.h
  - src/frame_bridge.cpp
  - src/win/wallpaper_host_win.cpp
  - src/loop_sleep_policy.cpp
  - src/app.cpp
  - src/win/decode_pipeline_stub.cpp
  - tests/frame_bridge_tests.cpp
  - tests/loop_sleep_policy_tests.cpp
  - task_plan.md
  - findings.md
  - progress.md

### Phase 50: 唤醒去重与渲染系统调用收敛
- **Status:** complete
- Actions taken:
  - Red: 在 `tests/loop_sleep_policy_tests.cpp` 新增 `LoopSleepPolicy_DecodePumpWakeNotificationUsesDedupe`，首次执行 `./scripts/run_tests.ps1` 编译失败（`ShouldNotifyDecodePumpWake` 未定义）。
  - Green: 在 `include/wallpaper/loop_sleep_policy.h` / `src/loop_sleep_policy.cpp` 新增 `ShouldNotifyDecodePumpWake`。
  - Green: `src/app.cpp` 的 `WakeDecodePump` 改为“去重标记 + notify_one”，避免重复广播唤醒。
  - Green: `src/win/wallpaper_host_win.cpp` 增加 viewport 宽高缓存，初始化/resize 时更新，`DrawVideoResources` 移除每帧 `GetClientRect`。
  - Green: `src/win/decode_pipeline_stub.cpp` 清理未使用字段 `asyncReadyFlags_` 与相关写入。
- Verification:
  - `./scripts/run_tests.ps1` -> 128/128 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp` -> build_tmp/wallpaper_app.exe 成功
- Files modified:
  - include/wallpaper/loop_sleep_policy.h
  - src/loop_sleep_policy.cpp
  - src/app.cpp
  - src/win/wallpaper_host_win.cpp
  - src/win/decode_pipeline_stub.cpp
  - tests/loop_sleep_policy_tests.cpp
  - task_plan.md
  - findings.md
  - progress.md

### Phase 51: 解码 token 消费前置门控
- **Status:** complete
- Actions taken:
  - Red: 在 `tests/loop_sleep_policy_tests.cpp` 增加 `DecodeTokenGatePolicy_*` 用例，锁定“无新序列不消费”的门控行为。
  - Green: 新增 `include/wallpaper/decode_token_gate_policy.h`，实现 `ShouldAttemptDecodedTokenConsume(...)`。
  - Green: `include/wallpaper/app.h` 增加 `latestDecodedSequence_` 原子字段。
  - Green: `src/app.cpp` 在 decode pump 写入 token 时同步更新原子序列；`Tick` 中先用序列门控，再决定是否进入 `decodedTokenMu_`。
- Verification:
  - `./scripts/run_tests.ps1` -> 132/132 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp` -> build_tmp/wallpaper_app.exe 成功
- Files modified:
  - include/wallpaper/decode_token_gate_policy.h
  - include/wallpaper/app.h
  - src/app.cpp
  - tests/loop_sleep_policy_tests.cpp
  - task_plan.md
  - findings.md
  - progress.md

### Phase 52: 首次运行峰值压降
- **Status:** complete
- Actions taken:
  - Red: 在 `tests/startup_policy_tests.cpp` 增加 `ShouldDeferVideoDecodeStart` 相关测试；在 `tests/config_store_tests.cpp` 增加 `ConfigStore_ExistsReflectsConfigFileState`，首次执行 `./scripts/run_tests.ps1` 编译失败（缺失实现）。
  - Green: `include/wallpaper/config_store.h` / `src/config_store.cpp` 新增 `ConfigStore::Exists()`。
  - Green: `include/wallpaper/startup_policy.h` / `src/startup_policy.cpp` 新增 `ShouldDeferVideoDecodeStart(...)`。
  - Green: `include/wallpaper/app.h` / `src/app.cpp` 为 `StartVideoPipelineForPath` 增加 `startDecodeImmediately` 参数，并在 `Initialize` 接入首启延迟启动策略。
  - Green: `src/app.cpp` 将 `Run` 循环中 `hasActiveVideo` 判定收敛为 `decodeOpened && decodeRunning`。
- Verification:
  - `./scripts/run_tests.ps1` -> 136/136 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp` -> build_tmp/wallpaper_app.exe 成功
- Files modified:
  - include/wallpaper/config_store.h
  - src/config_store.cpp
  - include/wallpaper/startup_policy.h
  - src/startup_policy.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - tests/startup_policy_tests.cpp
  - tests/config_store_tests.cpp
  - task_plan.md
  - findings.md
  - progress.md

### Phase 53: 桌面常驻帧缓存释放
- **Status:** complete
- Actions taken:
  - Red: 在 `tests/frame_bridge_tests.cpp` 新增 `FrameBridge_ReleaseConsumedKeepsNewerFrameIntact` 与 `FrameBridge_ReleaseConsumedDropsMatchedFrame`，首次执行 `./scripts/run_tests.ps1` 编译失败（缺失 `ReleaseLatestFrameIfSequenceConsumed`）。
  - Green: `include/wallpaper/frame_bridge.h` / `src/frame_bridge.cpp` 新增 `ReleaseLatestFrameIfSequenceConsumed`，按序列匹配安全释放 bridge 当前帧资源。
  - Green: `src/win/wallpaper_host_win.cpp` 在新帧成功上传/拷贝并绘制后，调用 `ReleaseLatestFrameIfSequenceConsumed(lastVideoSequence_)` 释放已消费帧持有。
- Verification:
  - `./scripts/run_tests.ps1` -> 138/138 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp` -> build_tmp/wallpaper_app.exe 成功
- Files modified:
  - include/wallpaper/frame_bridge.h
  - src/frame_bridge.cpp
  - src/win/wallpaper_host_win.cpp
  - tests/frame_bridge_tests.cpp
  - task_plan.md
  - findings.md
  - progress.md

### Phase 54: 首启窗口降载 + 单实例保护
- **Status:** complete
- Actions taken:
  - Red: 在 `tests/startup_policy_tests.cpp` 新增 `StartupPolicy_DeferredDecodeWaitsBeforeStart` 等用例，在 `tests/decode_async_read_policy_tests.cpp` 新增 `DecodeAsyncReadPolicy_DoesNotPrefetchImmediatelyAfterConsume`，首次执行 `./scripts/run_tests.ps1` 编译失败（缺失策略函数）。
  - Green: 新增 `ShouldStartDeferredDecodeNow(...)`，并在 `App::Tick` 对首启 deferred decode 增加 2.5s 最小等待窗口。
  - Green: `App` 新增 `startupDecodeDeferred_` 与 `startupDecodeDeferredAt_`，首启 deferred 成功初始化后记录时间窗。
  - Green: `decode_async_read_policy` 新增 `ShouldIssueReadImmediatelyAfterConsume()` 并返回 false；`decode_pipeline_stub` 消费样本后不再立即 prefetch 下一帧。
  - Green: `main.cpp` 增加 `ScopedSingleInstanceMutex`，使用命名互斥锁限制仅一个实例运行。
- Verification:
  - `./scripts/run_tests.ps1` -> 142/142 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp` -> build_tmp/wallpaper_app.exe 成功
- Files modified:
  - include/wallpaper/startup_policy.h
  - src/startup_policy.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - include/wallpaper/decode_async_read_policy.h
  - src/decode_async_read_policy.cpp
  - src/win/decode_pipeline_stub.cpp
  - src/main.cpp
  - tests/startup_policy_tests.cpp
  - tests/decode_async_read_policy_tests.cpp
  - task_plan.md
  - findings.md
  - progress.md

### Phase 55: 单实例守卫漏洞修复
- **Status:** complete
- Actions taken:
  - Red: 在 `tests/single_instance_policy_tests.cpp` 新增 `SingleInstancePolicy_FallbackToLocalMutexOnlyOnNonExistingErrors`，首次编译失败（缺少 `ShouldFallbackToLocalMutex`）。
  - Green: 在 `include/wallpaper/single_instance_policy.h` 新增 `ShouldFallbackToLocalMutex(errorCode)`，并将“已有实例”错误码明确为不可回退条件。
  - Green: `src/main.cpp` 的 `ScopedSingleInstanceMutex` 重构为：
    - `Global` 失败后仅在策略允许时才尝试 `Local`；
    - 显式区分 `mutex_acquired` / `lock_file_acquired`；
    - 补齐 lock file 句柄释放，避免进程生命周期内句柄泄漏。
  - Green: 移除 `CountProcessesWithSameExecutablePath` 进程枚举守卫，降低启动 CPU 干扰并避免权限差异导致的识别误判。
  - Verification supplement: 使用 `build_tmp/wallpaper_app.exe` 连续启动两次，第二实例退出，进程计数保持 1。
- Verification:
  - `./scripts/run_tests.ps1` -> 146/146 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp` -> build_tmp/wallpaper_app.exe 成功
- Files modified:
  - include/wallpaper/single_instance_policy.h
  - tests/single_instance_policy_tests.cpp
  - src/main.cpp
  - task_plan.md
  - findings.md
  - progress.md

### Phase 56: C++23 基线升级 + C++26 门控落地
- **Status:** complete
- Actions taken:
  - Red: 在 `tests/config_store_tests.cpp` 新增 `LoadExpected/SaveExpected` 测试、在 `tests/metrics_sampler_tests.cpp` 新增环形缓冲行为测试、在 `tests/cpp26_feature_support_tests.cpp` 新增特性门控测试，先触发编译失败。
  - Green: `include/wallpaper/config_store.h` / `src/config_store.cpp` 引入 `std::expected` 接口，并保留 `Load/Save` 包装；`src/app.cpp` 去除 `LoadAsync().get()` 与 `SaveAsync`，改为同步 `LoadExpected/SaveExpected`。
  - Green: `include/wallpaper/metrics_sampler.h` / `src/metrics_sampler.cpp` 改为固定容量环形缓冲，`PushSample` O(1)。
  - Green: 新增 `include/wallpaper/compiler_assume.h`，并在 `src/win/wallpaper_host_win.cpp` 上传热路径加入 `WP_ASSUME`。
  - Green: 新增 `include/wallpaper/cpp26_feature_support.h` 与测试，统一 C++26 能力门控。
  - Green: `CMakeLists.txt` 与 `scripts/build_app.ps1`、`scripts/run_tests.ps1` 升级到 C++23，脚本新增 `-UseCxx2c` 开关。
- Verification:
  - `./scripts/run_tests.ps1` -> 152/152 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp` -> build_tmp/wallpaper_app.exe 成功
  - `./scripts/run_tests.ps1 -UseCxx2c` -> 152/152 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp -UseCxx2c` -> build_tmp/wallpaper_app.exe 成功
- Files modified:
  - include/wallpaper/config_store.h
  - src/config_store.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - include/wallpaper/metrics_sampler.h
  - src/metrics_sampler.cpp
  - include/wallpaper/compiler_assume.h
  - include/wallpaper/cpp26_feature_support.h
  - src/win/wallpaper_host_win.cpp
  - tests/config_store_tests.cpp
  - tests/metrics_sampler_tests.cpp
  - tests/cpp26_feature_support_tests.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1
  - scripts/build_app.ps1
  - task_plan.md
  - findings.md
  - progress.md

### Phase 57: FrameBridge 热路径拷贝瘦身
- **Status:** complete
- Actions taken:
  - Red: 更新 `tests/frame_bridge_tests.cpp`，将断言从 `rgbaPixels` 切换到 `rgbaDataHolder/rgbaDataBytes`，先锁定新的持有语义。
  - Green: `include/wallpaper/frame_bridge.h` 删除 `LatestFrame::rgbaPixels` 字段。
  - Green: `src/frame_bridge.cpp` 的 `PublishLatestFrame` 改为直接从传入 `shared_ptr<vector<uint8_t>>` 构建 alias holder；其余发布路径移除对 `rgbaPixels` 的 reset 操作。
- Verification:
  - `./scripts/run_tests.ps1` -> 152/152 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp` -> build_tmp/wallpaper_app.exe 成功
- Files modified:
  - include/wallpaper/frame_bridge.h
  - src/frame_bridge.cpp
  - tests/frame_bridge_tests.cpp
  - task_plan.md
  - findings.md
  - progress.md

### Phase 58: 解码帧就绪通知驱动退避
- **Status:** complete
- Actions taken:
  - Red: `include/wallpaper/loop_sleep_policy.h` 改签名增加 `frameReadyNotifierAvailable` 后，先更新 `tests/loop_sleep_policy_tests.cpp` 新增 `LoopSleepPolicy_DecodePumpNoFrameUsesLongerBackoffWithNotifier` 并触发编译失败。
  - Green: `include/wallpaper/interfaces.h` 为 `IDecodePipeline` 增加 `SetFrameReadyNotifier(...)` 与 `SupportsFrameReadyNotifier()`，并同步更新 `src/platform_stubs.cpp`。
  - Green: `src/win/decode_pipeline_stub.cpp` 在异步 `OnReadSample` 收到有效样本后通知上层，支持事件驱动唤醒解码泵。
  - Green: `src/app.cpp` 在初始化时注入 notifier，按当前管线能力选择 notifier-aware 退避策略。
  - Green: `src/loop_sleep_policy.cpp` 对通知可用路径放宽无帧退避上限（24ms->40ms），减少空轮询。
- Verification:
  - `./scripts/run_tests.ps1` -> 153/153 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp` -> build_tmp/wallpaper_app.exe 成功
  - `./scripts/run_tests.ps1 -UseCxx2c` -> 153/153 PASS
- Files modified:
  - include/wallpaper/interfaces.h
  - src/platform_stubs.cpp
  - include/wallpaper/loop_sleep_policy.h
  - src/loop_sleep_policy.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - src/win/decode_pipeline_stub.cpp
  - tests/loop_sleep_policy_tests.cpp
  - task_plan.md
  - findings.md
  - progress.md

### Phase 59: C++26 标志统一 + 帧桥接无锁化 + 锁域收敛迭代
- **Status:** complete
- Actions taken:
  - Green: `scripts/run_tests.ps1`、`scripts/build_app.ps1` 新增 `-UseCxx26` 参数，并实现 `-std=c++26` 优先、`-std=c++2c` 自动回退探测。
  - Green: `src/frame_bridge.cpp` 改为原子 `shared_ptr` 发布/读取模型，移除全局 `mutex` 热点。
  - Green: `src/win/decode_pipeline_stub.cpp` 引入 `ReadySampleSnapshot`，`TryAcquireLatestFrame` 改为媒体路径锁外发布，缩短 `mu_` 持锁时间。
  - Green: `src/app.cpp` 解码泵等待改造，在 notifier 可用时扩大事件等待窗口（最小 90ms），降低无帧轮询调度。
  - Green: 新增 `scripts/bench_perf.ps1`，支持三场景采样并输出 CSV/JSON 汇总。
- Verification:
  - `./scripts/run_tests.ps1` -> 153/153 PASS
  - `./scripts/run_tests.ps1 -UseCxx26` -> 153/153 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp` -> 成功
  - `./scripts/build_app.ps1 -BuildDir build_tmp -UseCxx26` -> 成功
  - `./scripts/bench_perf.ps1 -ExePath build_tmp/wallpaper_app.exe -Scenario startup -DurationSec 5 -Tag smoke` -> 成功，输出 `build_tmp/bench/startup_*_smoke.{csv,json}`
- Files modified:
  - src/frame_bridge.cpp
  - src/win/decode_pipeline_stub.cpp
  - src/app.cpp
  - scripts/run_tests.ps1
  - scripts/build_app.ps1
  - scripts/bench_perf.ps1
  - task_plan.md
  - findings.md
  - progress.md

### Phase 60: 退避与池化分配迭代
- **Status:** complete
- Actions taken:
  - Green: `src/loop_sleep_policy.cpp` notifier 路径无帧退避上限由 40ms 提升到 64ms。
  - Green: `src/app.cpp` notifier 事件等待窗口最小值提升到 140ms。
  - Green: `src/frame_bridge.cpp` 引入 `std::pmr::synchronized_pool_resource`，`FramePayload` 改为 `allocate_shared` 池化分配。
  - Green: `tests/loop_sleep_policy_tests.cpp` 同步更新 notifier 路径断言。
  - Green: 运行 `bench_perf` 冒烟，确认 startup/desktop 场景输出 JSON/CSV。
- Verification:
  - `./scripts/run_tests.ps1` -> 153/153 PASS
  - `./scripts/run_tests.ps1 -UseCxx26` -> 153/153 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp` -> 成功
  - `./scripts/build_app.ps1 -BuildDir build_tmp -UseCxx26` -> 成功
  - `./scripts/bench_perf.ps1 -Scenario startup -DurationSec 8 -Tag iter2` -> 成功
  - `./scripts/bench_perf.ps1 -Scenario desktop -DurationSec 8 -WarmupSec 3 -Tag iter2_seq` -> 成功
- Notes:
  - 并行启动两个 `bench_perf` 会触发单实例保护，第二个实例采样为空；基准需顺序执行。
- Files modified:
  - src/frame_bridge.cpp
  - src/loop_sleep_policy.cpp
  - src/app.cpp
  - tests/loop_sleep_policy_tests.cpp
  - task_plan.md
  - findings.md
  - progress.md

### Phase 61: 主循环唤醒收敛 + 解码冗余原子清理
- **Status:** complete
- Actions taken:
  - Red: 在 `tests/loop_sleep_policy_tests.cpp` 新增 `LoopSleepPolicy_MainLoopMessageAwareWaitOnlyForPauseOrIdle`，先锁定“仅 pause/idle 使用消息等待”的策略。
  - Green: `include/wallpaper/loop_sleep_policy.h` / `src/loop_sleep_policy.cpp` 新增 `ShouldUseMainLoopMessageAwareWait`。
  - Green: `src/app.cpp` 的 `WaitMainLoopInterval` 增加 `useMessageAwareWait` 开关；动态桌面路径改为 `Sleep`，pause/idle 路径消息等待改用 `QS_POSTMESSAGE|QS_SENDMESSAGE|QS_TIMER`。
  - Green: `src/win/decode_pipeline_stub.cpp` 删除 `ConsumeFrameBufferCapacityHint` 与 `previousPublishedCpuBytes_/trimRequested_` 原子状态，移除每帧无收益原子读写。
  - Green: `include/wallpaper/app.h` 修正方法声明缩进，保持头文件可读性一致。
- Verification:
  - `./scripts/run_tests.ps1 -BuildDir build_tmp/tests_phase61_cxx23` -> 154/154 PASS
  - `./scripts/run_tests.ps1 -BuildDir build_tmp/tests_phase61_cxx26 -UseCxx26` -> 154/154 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp/phase61_cxx23` -> 成功
  - `./scripts/build_app.ps1 -BuildDir build_tmp/phase61_cxx26 -UseCxx26` -> 成功
  - `./scripts/build_app.ps1 -BuildDir build_tmp` -> 成功
  - `./scripts/bench_perf.ps1 -ExePath build_tmp/wallpaper_app.exe -Scenario startup -DurationSec 12 -SampleMs 500 -Tag phase61_before_seq` -> 成功
  - `./scripts/bench_perf.ps1 -ExePath build_tmp/wallpaper_app.exe -Scenario startup -DurationSec 12 -SampleMs 500 -Tag phase61_after_samepath` -> 成功
  - `./scripts/bench_perf.ps1 -ExePath build_tmp/wallpaper_app.exe -Scenario desktop -DurationSec 12 -WarmupSec 6 -SampleMs 500 -Tag phase61_before` -> 成功
  - `./scripts/bench_perf.ps1 -ExePath build_tmp/wallpaper_app.exe -Scenario desktop -DurationSec 12 -WarmupSec 6 -SampleMs 500 -Tag phase61_after_samepath` -> 成功
- Notes:
  - 单实例保护存在时，基准场景必须串行运行；并行会导致一个场景采样为空。
- Files modified:
  - include/wallpaper/loop_sleep_policy.h
  - src/loop_sleep_policy.cpp
  - tests/loop_sleep_policy_tests.cpp
  - src/app.cpp
  - src/win/decode_pipeline_stub.cpp
  - include/wallpaper/app.h
  - task_plan.md
  - findings.md
  - progress.md

### Phase 62: 并发/生命周期高危修复 + C++26 条件增强
- **Status:** complete
- Actions taken:
  - Added `tray_thread_stop_policy` + tests to lock down stop/join behavior.
  - Fixed tray worker shutdown path to always join joinable thread.
  - Hardened MF async callback ownership with atomic owner pointer.
  - Added decode pipeline destructor cleanup (state reset + MF shutdown fallback).
  - Fixed D3D init failure cleanup and swapchain resize RTV fallback recreation.
  - Promoted `decodeFrameReadyNotifierAvailable_` to atomic for cross-thread correctness.
  - Added compile-time full-screen quad via `consteval` and span-based row copy helper in upload paths.
  - Updated CMake and test script source lists for new tests.
- Files created/modified:
  - include/wallpaper/tray_thread_stop_policy.h
  - tests/tray_thread_stop_policy_tests.cpp
  - src/win/tray_controller_win.cpp
  - src/win/decode_pipeline_stub.cpp
  - src/win/wallpaper_host_win.cpp
  - include/wallpaper/app.h
  - CMakeLists.txt
  - scripts/run_tests.ps1
- Verification:
  - `scripts/run_tests.ps1` -> pass (157/157)
  - `scripts/run_tests.ps1 -UseCxx26` -> pass (157/157)
  - `scripts/build_app.ps1 -BuildDir build_tmp/phase62_cxx23` -> pass
  - `scripts/build_app.ps1 -BuildDir build_tmp/phase62_cxx26 -UseCxx26` -> pass
  - `g++ --version` -> 15.2.0; `-fexperimental-library` not recognized in this toolchain

### Phase 63: 恢复慢修复（重试退避）
- **Status:** complete
- Actions taken:
  - Added retry-delay policy APIs in `pause_suspend_policy`.
  - Added tests for warm-resume and resume-pipeline retry delay escalation and caps.
  - Wired retry failure counters into `App` pause->resume flow.
  - Replaced fixed warmup retry `500ms` and resume retry `1s` with fast-to-capped backoff.
- Files created/modified:
  - include/wallpaper/pause_suspend_policy.h
  - src/pause_suspend_policy.cpp
  - tests/pause_suspend_policy_tests.cpp
  - include/wallpaper/app.h
  - src/app.cpp
- Verification:
  - `scripts/run_tests.ps1` -> pass (159/159)
  - `scripts/build_app.ps1 -BuildDir build_tmp/phase63_resume_fix_cxx23` -> pass
  - `scripts/build_app.ps1 -BuildDir build_tmp/phase63_resume_fix_cxx26 -UseCxx26` -> pass

### Phase 64: Video Path Probe Cache for Resume Hot Path
- **Status:** complete
- Actions taken:
  - Added `video_path_probe_policy` module with cache-reuse guard and TTL selector.
  - Added unit tests for cache-hit/miss and TTL behavior.
  - Integrated cache into `App` resume hot paths and added cache invalidation hook.
  - Extended `StartVideoPipelineForPath` with optional cached probe mode.
  - Updated CMake/tests/build script source lists.
- Files created/modified:
  - include/wallpaper/video_path_probe_policy.h
  - src/video_path_probe_policy.cpp
  - tests/video_path_probe_policy_tests.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1
  - scripts/build_app.ps1
- Verification:
  - `scripts/run_tests.ps1 -BuildDir build_tmp/phase64_tests` -> pass (163/163)
  - `scripts/build_app.ps1 -BuildDir build_tmp/phase64_probe_cache_cxx23` -> pass
  - `scripts/build_app.ps1 -BuildDir build_tmp/phase64_probe_cache_cxx26 -UseCxx26` -> pass

## Session: 2026-03-30

### Phase 62: 多显示器独立渲染修复
- **Status:** in_progress
- **Started:** 2026-03-30 09:26:52
- Actions taken:
  - 扫描并定位到 wallpaper_host_win.cpp 中单视口全屏渲染路径。
  - 确认当前实现未做按显示器独立视口拆分。
  - 制定 TDD 修复路线：先策略测试，再接入宿主渲染。
- Files created/modified:
  - task_plan.md
  - findings.md
  - progress.md

### Phase 62: 多显示器独立渲染修复
- **Status:** complete
- **Completed:** 2026-03-30 09:37:48
- Actions taken:
  - 新增 monitor_layout_policy 头/源与 4 个策略单测（并排、堆叠、越界裁剪、空输入）。
  - 在 Windows 壁纸宿主中接入显示器枚举与多视口绘制循环。
  - 将测试/构建脚本与 CMake 源清单更新为包含新模块。
  - 执行 Red -> Green -> 回归验证流程。
- Verification:
  - ./scripts/run_tests.ps1 通过（167/167）
  - ./scripts/build_app.ps1 通过（uild/wallpaper_app.exe）
- Files created/modified:
  - include/wallpaper/monitor_layout_policy.h
  - src/monitor_layout_policy.cpp
  - tests/monitor_layout_policy_tests.cpp
  - src/win/wallpaper_host_win.cpp
  - CMakeLists.txt
  - scripts/run_tests.ps1
  - scripts/build_app.ps1

### Phase 63: 静态恢复时延与资源权衡优化
- **Status:** complete
- **Completed:** 2026-03-30 09:55:36
- Actions taken:
  - 先新增失败测试：默认暂停阈值配置策略测试。
  - 实现 DefaultPauseSuspendThresholdProfile() 并在 App::Tick 统一使用。
  - 调整暂停态循环与探测节奏，缩短恢复检测延迟。
  - 执行全量单测和应用构建验证。
- Verification:
  - ./scripts/run_tests.ps1 通过（168/168）
  - ./scripts/build_app.ps1 通过（uild/wallpaper_app.exe）
- Files modified:
  - include/wallpaper/pause_suspend_policy.h
  - src/pause_suspend_policy.cpp
  - src/app.cpp
  - src/loop_sleep_policy.cpp
  - src/probe_cadence_policy.cpp
  - tests/pause_suspend_policy_tests.cpp
  - tests/loop_sleep_policy_tests.cpp
  - tests/probe_cadence_policy_tests.cpp

### Phase 73: 静帧呈现节流 + 指标写盘路径降载
- **Status:** complete
- **Completed:** 2026-03-30 19:48:13
- Actions taken:
  - 先补 Red 测试：`startup_policy` 静帧 keepalive 呈现门控测试；`metrics_log_file` 日期 provider 调用次数约束测试。
  - 实现静帧呈现门控：`ShouldPresentFrame` 增加 stale-frame due 入参；新增 `ShouldPresentStaleFrame`。
  - 在 `App::Tick` 接入 `250ms` 静帧 keepalive 呈现节流，并在 `ResetPlaybackState/DetachWallpaper` 重置 `lastPresentedAt_`。
  - 优化指标写盘路径：`MetricsLogFile::Append` 改为单次 active path 计算；新增 `EnsureReadyForPath/MaybePruneShards`，将 shard prune 改为“路径切换或 10 分钟到期”触发。
- Verification:
  - `./scripts/run_tests.ps1 -BuildDir build_tmp/phase73_green` -> pass (172/172)
  - `./scripts/build_app.ps1 -BuildDir build_tmp/phase73_app` -> pass
  - `./scripts/bench_perf.ps1 -ExePath build_tmp/phase73_app/wallpaper_app.exe -Scenario desktop -DurationSec 10 -WarmupSec 5 -SampleMs 500 -Tag phase73_desktop`
    - `cpu_avg_percent=0.0192`
    - `cpu_p95_percent=0.1909`
    - `working_set_bytes_delta=0`
- Files modified:
  - include/wallpaper/startup_policy.h
  - src/startup_policy.cpp
  - include/wallpaper/app.h
  - src/app.cpp
  - include/wallpaper/metrics_log_file.h
  - src/metrics_log_file.cpp
  - tests/startup_policy_tests.cpp
  - tests/metrics_log_file_tests.cpp

### Phase 74: 前台探测稳定窗口复用优化
- **Status:** complete
- **Completed:** 2026-03-30 20:30:18
- Actions taken:
  - 先补 Red 测试：`probe_cadence_policy` 增加 4 个“前台窗口稳定复用”测试。
  - 新增策略函数 `ShouldReuseForegroundProbeResult(...)`。
  - `App` 新增 `lastForegroundWindowHandle_` 与 `lastForegroundDeepProbeAt_` 状态；`ResetPlaybackState` 同步重置。
  - 前台探测路径改为“稳定窗口先复用，未命中再深度探测”，并将 `TryDetectDesktopContextActive` 改成接收已采样句柄，避免重复前台窗口查询。
- Verification:
  - `./scripts/run_tests.ps1 -BuildDir build_tmp/phase74_green` -> pass (176/176)
  - `./scripts/build_app.ps1 -BuildDir build_tmp/phase74_app` -> pass
  - `./scripts/bench_perf.ps1 -ExePath build_tmp/phase74_app/wallpaper_app.exe -Scenario desktop -DurationSec 10 -WarmupSec 5 -SampleMs 500 -Tag phase74_desktop`
    - `cpu_avg_percent=0.0095`
    - `cpu_p95_percent=0.0`
    - `working_set_bytes_delta=0`
- Files modified:
  - include/wallpaper/probe_cadence_policy.h
  - src/probe_cadence_policy.cpp
  - tests/probe_cadence_policy_tests.cpp
  - include/wallpaper/app.h
  - src/app.cpp

### Phase 75: 会话探测状态感知降频优化
- **Status:** complete
- **Completed:** 2026-03-30 20:57:17
- Actions taken:
  - 先补 Red 测试：为 `probe_cadence_policy` 新增 3 组会话探测间隔状态策略测试。
  - 新增 `SelectSessionProbeIntervalForState(...)`：
    - 正常稳定态（interactive=true 且非 battery saver 且非 remote）按 `2x` 放宽。
    - 放宽后间隔上限 `1200ms`。
    - 非正常态保持基础间隔，确保状态恢复感知速度。
  - `App::Tick` 接入该策略，替代固定 `probeIntervals.session` 调用。
- Verification:
  - `./scripts/run_tests.ps1 -BuildDir build_tmp/phase75_green` -> pass (179/179)
  - `./scripts/build_app.ps1 -BuildDir build_tmp/phase75_app` -> pass
  - 同配置短基准（均加载 `kuroha_1080p30_h264.mp4`）：
    - phase74: `cpu_avg_percent=1.4748`（`desktop_20260330_205746_phase74_desktop_with_video_r3.json`）
    - phase75: `cpu_avg_percent=1.4038`（`desktop_20260330_205717_phase75_desktop_with_video.json`）
- Files modified:
  - include/wallpaper/probe_cadence_policy.h
  - src/probe_cadence_policy.cpp
  - tests/probe_cadence_policy_tests.cpp
  - src/app.cpp

### Phase 76: 指标采样状态感知降频优化
- **Status:** complete
- **Completed:** 2026-03-30 21:22:03
- Actions taken:
  - 先补 Red 测试：新增 `SelectMetricsSampleInterval` 的 3 组策略测试（活跃/暂停或非活跃/遮挡）。
  - 在 `probe_cadence_policy` 新增 `SelectMetricsSampleInterval(hasActiveVideo, stablePaused, occluded)`。
  - `App::MaybeSampleAndLogMetrics` 接入状态感知采样窗口：
    - 活跃播放：`1000ms`
    - 非活跃、暂停、遮挡：`2000ms`
  - 采样门控前移，避免非采样周期执行 `GetProcessTimes/GetProcessMemoryInfo` 与 CSV 拼装写盘。
- Verification:
  - `./scripts/run_tests.ps1 -BuildDir build_tmp/phase76_green` -> pass (182/182)
  - `./scripts/build_app.ps1 -BuildDir build_tmp/phase76_app` -> pass
  - 受控同配置对比（`pauseWhenNotDesktopContext=false`）：
    - phase75: `cpu_avg_percent=1.4988`（`desktop_20260330_212136_phase76_ab_phase75_forceactive.json`）
    - phase76: `cpu_avg_percent=1.2087`（`desktop_20260330_212203_phase76_ab_phase76_forceactive_r2.json`）
- Files modified:
  - include/wallpaper/probe_cadence_policy.h
  - src/probe_cadence_policy.cpp
  - tests/probe_cadence_policy_tests.cpp
  - src/app.cpp

### Phase 77: CPU 回退输出尺寸协商重试
- **Status:** complete
- **Completed:** 2026-03-30 21:59:42
- Actions taken:
  - 新增 `decode_output_policy` 测试，锁定“CPU fallback + adaptive 且协商超 hint 时应重试 software video processing”行为。
  - 新增 `ShouldRetryDecodeOpenWithVideoProcessing(...)` 并接入 `decode_pipeline_stub`。
  - 将 SourceReader 打开流程改为“首次协商 + 命中策略时一次性重开 + 最终兜底回退”，避免重复无效 reopen。
- Verification:
  - `./scripts/run_tests.ps1 -BuildDir build_tmp/phase77_green` -> pass (185/185)
  - `./scripts/build_app.ps1 -BuildDir build_tmp/phase77_app` -> pass
- Files modified:
  - include/wallpaper/decode_output_policy.h
  - src/decode_output_policy.cpp
  - src/win/decode_pipeline_stub.cpp
  - tests/decode_output_policy_tests.cpp

### Phase 78: Working-Set 精细回收门控
- **Status:** complete
- **Completed:** 2026-03-30 22:20:31
- Actions taken:
  - 新增 `runtime_trim_policy` 测试，锁定 working-set 回收在活跃视频 CPU fallback 场景的阈值规则。
  - 新增 `ShouldRequestWorkingSetTrim(...)`（L0=64MB/L1=40MB/L2+=32MB）。
  - `App::MaybeSampleAndLogMetrics` 改为策略门控，并将 working-set 回收冷却缩短到 `8s`。
- Verification:
  - `./scripts/run_tests.ps1 -BuildDir build_tmp/phase78_green` -> pass (188/188)
  - `./scripts/build_app.ps1 -BuildDir build_tmp/phase78_app` -> pass
  - `./scripts/bench_perf.ps1 -ExePath build_tmp/phase77_app/wallpaper_app.exe -Scenario desktop -DurationSec 12 -WarmupSec 6 -SampleMs 500 -Tag phase78_ab_phase77`
    - CPU avg `1.3472%`, WS min `46624768`, WS max `53280768`
  - `./scripts/bench_perf.ps1 -ExePath build_tmp/phase78_app/wallpaper_app.exe -Scenario desktop -DurationSec 12 -WarmupSec 6 -SampleMs 500 -Tag phase78_ab_phase78`
    - CPU avg `1.3824%`, WS min `34709504`, WS max `47448064`
- Files modified:
  - include/wallpaper/runtime_trim_policy.h
  - src/runtime_trim_policy.cpp
  - src/app.cpp
  - tests/runtime_trim_policy_tests.cpp

### Phase 79: Advanced Video Processing 协商增强
- **Status:** complete
- **Completed:** 2026-03-30 22:31:06
- Actions taken:
  - Red: 在 `tests/decode_output_policy_tests.cpp` 新增 advanced video processing 启用策略测试，确认缺失实现导致红灯。
  - Green: 新增 `ShouldEnableAdvancedVideoProcessing(...)`。
  - Green: `decode_pipeline_stub` software processing 路径接入 `MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING`（编译可用时）。
  - Green: `QueryDesktopFrameHint` 在 Open 流程内改为单次读取，多处策略复用同一 hint，减少重复系统调用。
- Verification:
  - `./scripts/run_tests.ps1 -BuildDir build_tmp/phase79_red` -> expected fail（缺少新策略函数）
  - `./scripts/run_tests.ps1 -BuildDir build_tmp/phase79_green` -> pass (190/190)
  - `./scripts/build_app.ps1 -BuildDir build_tmp/phase79_app` -> pass
  - `./scripts/bench_perf.ps1 -ExePath build_tmp/phase78_app/wallpaper_app.exe -Scenario desktop -DurationSec 12 -WarmupSec 6 -SampleMs 500 -Tag phase79_ab_phase78`
    - CPU avg `1.4874%`, CPU p95 `2.3036%`, WS delta `-11071488`
  - `./scripts/bench_perf.ps1 -ExePath build_tmp/phase79_app/wallpaper_app.exe -Scenario desktop -DurationSec 12 -WarmupSec 6 -SampleMs 500 -Tag phase79_ab_phase79`
    - CPU avg `1.4225%`, CPU p95 `2.1101%`, WS delta `-15671296`
- Notes:
  - `build_tmp/phase79_app/metrics_20260330.csv` 的 `decode_output_pixels` 仍为 `2073600`，输出尺寸 hint 仍未真正落到 CPU fallback 结果。
- Files modified:
  - include/wallpaper/decode_output_policy.h
  - src/decode_output_policy.cpp
  - src/win/decode_pipeline_stub.cpp
  - tests/decode_output_policy_tests.cpp
  - task_plan.md
  - findings.md
  - progress.md

### Phase 80: CPU/内存硬指标攻坚（进行中）
- **Status:** in_progress
- **Started:** 2026-03-31 09:33
- Actions taken:
  - 复盘 `task_plan/findings` 并锁定上一阶段剩余风险：CPU fallback 输出尺寸 hint 未生效。
  - 读取 `app.cpp`、`decode_pipeline_stub.cpp`、`loop_sleep_policy.cpp`、`runtime_trim_policy.cpp` 关键路径，定位调度与回收门控现状。
  - 重新执行基准：
    - `./scripts/bench_perf.ps1 -ExePath build_tmp/phase79_app/wallpaper_app.exe -Scenario desktop -DurationSec 12 -WarmupSec 6 -SampleMs 500 -Tag phase80_baseline`
    - 输出：`CPU avg 1.5125%`、`CPU p95 2.8729%`、`WS max 47.49MB`
  - 抽查 `build_tmp/phase79_app/metrics_20260331.csv`，确认运行主路径仍为 `mf + cpu_nv12_fallback + 1080p`（`decode_output_pixels=2073600`）。
- Next:
  - 进入方案提审并锁定默认实现路径。
  - 按 TDD 先补失败测试，再做实现与回归。
- Files modified:
  - task_plan.md
  - findings.md
  - progress.md

### Phase 80: 无损降载与互操作修正
- **Status:** complete
- **Completed:** 2026-03-31 10:02
- Actions taken:
  - Red: 增加 `decode_output_policy` 与 `loop_sleep_policy` 新策略测试并验证红灯。
  - Green: 新增 `ShouldPreserveD3DInteropOnVideoProcessingRetry` 并接入 `decode_pipeline_stub`，避免 video-processing 重试时丢失 D3D interop 优先级。
  - Green: 新增 `ShouldPreferEventDrivenDecodePumpWait` 并接入解码泵分支，notifier 可用时统一事件驱动等待。
  - Green: D3D 初始化增加 `VIDEO_SUPPORT/BGRA_SUPPORT`，WARP 路径保留稳态回退。
  - Green: `decode_pipeline_stub` 补 `MF_SOURCE_READER_DISABLE_DXVA=FALSE`（可用时）、RGB32 互操作路径识别与重试策略收敛。
- Verification:
  - `./scripts/run_tests.ps1 -BuildDir build_tmp/phase80_red` -> expected fail
  - `./scripts/run_tests.ps1 -BuildDir build_tmp/phase80_green` -> pass (194/194)
  - `./scripts/build_app.ps1 -BuildDir build_tmp/phase80_app` -> pass
  - `./scripts/bench_perf.ps1 -ExePath build_tmp/phase80_app/wallpaper_app.exe -Scenario desktop -DurationSec 12 -WarmupSec 6 -SampleMs 500 -Tag phase80_ab_phase80_cfg_r1`
    - CPU avg `1.0409%`, CPU p95 `2.2542%`, WS max `46.77MB`
  - `./scripts/bench_perf.ps1 -ExePath build_tmp/phase80_app/wallpaper_app.exe -Scenario desktop -DurationSec 12 -WarmupSec 6 -SampleMs 500 -Tag phase80_ab_phase80_cfg_r2`
    - CPU avg `0.6801%`, CPU p95 `1.5378%`, WS max `44.67MB`
- Files modified:
  - tests/decode_output_policy_tests.cpp
  - tests/loop_sleep_policy_tests.cpp
  - include/wallpaper/decode_output_policy.h
  - src/decode_output_policy.cpp
  - include/wallpaper/loop_sleep_policy.h
  - src/loop_sleep_policy.cpp
  - src/app.cpp
  - src/win/decode_pipeline_stub.cpp
  - src/win/wallpaper_host_win.cpp

### Phase 81: Working-Set 激进回收门控
- **Status:** complete
- **Completed:** 2026-03-31 10:13
- Actions taken:
  - Red: 下探 `runtime_trim_policy` 单测阈值到目标导向档位（20/18/16MB）。
  - Green: 回收阈值下探并将 `App` 的 working-set trim 冷却从 `8s` 收紧到 `2s`。
  - Green: 补 `ID3D10Multithread::SetMultithreadProtected(TRUE)` 到 D3D 设备初始化路径。
- Verification:
  - `./scripts/run_tests.ps1 -BuildDir build_tmp/phase81_red` -> expected fail（阈值断言）
  - `./scripts/run_tests.ps1 -BuildDir build_tmp/phase81_green` -> pass (194/194)
  - `./scripts/build_app.ps1 -BuildDir build_tmp/phase81_app` -> pass
  - `./scripts/bench_perf.ps1 -ExePath build_tmp/phase81_app/wallpaper_app.exe -Scenario desktop -DurationSec 12 -WarmupSec 6 -SampleMs 500 -Tag phase81_ab_phase81_r2`
    - CPU avg `1.2180%`, CPU p95 `2.7628%`, WS min `19.81MB`, WS max `41.39MB`
  - `./scripts/build_app.ps1 -BuildDir build_tmp/phase82_app` -> pass
  - `./scripts/bench_perf.ps1 -ExePath build_tmp/phase82_app/wallpaper_app.exe -Scenario desktop -DurationSec 12 -WarmupSec 6 -SampleMs 500 -Tag phase82_ab_phase82_r1`
    - CPU avg `0.9490%`, CPU p95 `1.6957%`, WS max `43.03MB`
- Files modified:
  - tests/runtime_trim_policy_tests.cpp
  - src/runtime_trim_policy.cpp
  - src/app.cpp
  - src/win/wallpaper_host_win.cpp

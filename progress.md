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

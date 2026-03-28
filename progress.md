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

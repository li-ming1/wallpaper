# Findings & Decisions

## Requirements
- Build a dynamic wallpaper app from scratch.
- Primary target: very low memory usage and very high performance.
- Must avoid perceptible stutter.
- Windows 10/11 first.
- Video wallpaper first, synchronized across monitors.
- Pause and release resources on fullscreen foreground apps.
- Tray + minimal settings window.

## Research Findings
- Repository started empty; now bootstrapped with core C++20 architecture.
- cmake tool is unavailable in current PATH; g++ is available at D:\msys64\ucrt64\bin\g++.exe.
- Tests are runnable and green via PowerShell script.
- Tray controller now has actual notify icon + context menu exit flow.
- Wallpaper host now sizes by virtual desktop metrics for multi-monitor coverage.
- Decode pipeline now tries Media Foundation first and falls back to internal ticker mode.
- MF sample now publishes RGBA frame to frame bridge; renderer consumes bridge for on-screen video texture draw.
- App now supports tray-driven hot updates: 30/60fps, select video, clear video, enable/disable auto-start.
- App now supports adaptive quality: when runtime load spikes it drops to 30fps, and recovers to target fps after stable samples.
- Tray menu now supports enable/disable adaptive quality, with config persistence.
- Runtime metrics now append once per second through a bounded writer, auto-truncating to header when file size threshold is exceeded.
- Metrics log now writes daily shards (`metrics_YYYYMMDD.csv`) and keeps a rolling retention window (last 7 days).
- Metrics rows now include session and runtime policy context (`session_id`, `target_fps`, `effective_fps`, `adaptive_quality`, `decode_mode`).
- Fixed MF decode open path: enabled Source Reader video processing/hardware transform attributes so RGB32 output negotiation succeeds on common Windows setups.
- Fixed desktop interaction path: corrected WorkerW target selection and pumped main-thread window messages to avoid busy cursor + blocked desktop icons.
- Hardened attach path by attempting multiple WorkerW candidates and falling back safely, with child-window coordinates normalized to parent space.
- Replaced unsafe attach fallback (`Progman`/`Desktop`) with safe attach order (`WorkerW` then `SHELLDLL_DefView`), preventing icon-layer occlusion.
- Verified runtime probe: render host parent is now `SHELLDLL_DefView` with full desktop bounds (`1536x864` in current environment).
- Added explicit `WM_SETCURSOR` handling to force arrow cursor on wallpaper host and avoid inherited busy cursor state.
- Added path-equivalence matcher for selected videos (absolute/relative, separator, case) to avoid unnecessary decoder restart on same file reselect.
- Added icon-layer visibility reinforcement (`SysListView32` show + top z-order) after wallpaper attach/resize to reduce desktop file visibility regressions.
- Restored non-WorkerW fallback to `DefView-child` and explicitly keeps `SysListView32` above wallpaper host in same parent z-order, fixing “select video no visual change” regression.
- Reduced MF decode memory pressure: reused frame publish buffer, selected video stream only, and enabled low-latency source-reader mode.
- Fixed tray wide-string to UTF-8 conversion to avoid non-ASCII path corruption when selecting videos with Chinese filenames.
- Reworked fullscreen pause detection to exclude shell classes (`Progman`/`WorkerW`/`DefView`/`SysListView32`/wallpaper host), preventing pause when user clicks desktop.
- Render loop now re-presents last frame when decoder has no new sample, avoiding false drop accounting and visible stutter on 30fps content under 60fps scheduling.
- Added source-timestamp based render-cap path (30/60) so scheduler can track decoded stream cadence rather than blindly running at target cap.
- Added dedicated GUI-subsystem build script (`-mwindows`) so startup no longer shows console window.

## Technical Decisions
| Decision | Rationale |
|----------|-----------|
| Single process architecture | Avoid IPC overhead and simplify resource control |
| WorkerW host abstraction | Native wallpaper integration model on Windows |
| MF timeline decode with fallback mode | Preserve dynamic output even when media init fails |
| Frame bridge between decode and render | Decouple decode timing from render thread while keeping latest-frame semantics |
| Bounded and deterministic scheduling | Prevent memory/latency drift |
| Config I/O with async wrappers | Align with async-first I/O requirement |
| Adaptive quality governor with hysteresis | Avoid FPS oscillation while preserving high-FPS targets under stable load |
| Bounded metrics log with threshold truncate | Prevent long-running disk growth while keeping recent samples |
| Daily metrics shards + retention cleanup | Keep observability history while bounding disk usage over long uptime |
| Extended metrics row context fields | Make adaptive-quality and decode-path behavior analyzable across process sessions |
| Enable MF Source Reader video processing | Avoid false fallback when RGB32 output type negotiation fails |
| Correct WorkerW parenting + message pump | Keep wallpaper behind icons and prevent UI non-responsiveness symptoms |
| Multi-candidate WorkerW attach fallback | Avoid no-op wallpaper when one shell host denies child window creation |
| Safe attach order: WorkerW first, DefView second, no Progman/Desktop | Avoid icon occlusion and DPI-virtualized parent size issues |
| Path equivalence compare before decode reopen | Prevent reselecting same video from triggering heavy MF reopen and memory spikes |
| Force desktop list view visible/top after host placement | Stabilize desktop icon visibility when shell child z-order fluctuates |
| DefView fallback + ListView top z-order in same parent | Keep icons visible while ensuring wallpaper content is rendered behind icons |
| SourceReader low-latency + reusable publish buffer | Lower high-resolution video runtime memory footprint |
| Safe UTF-8 conversion in tray file picker path | Ensure Chinese/non-ASCII video paths can be selected and opened correctly |
| Foreground shell-class exclusion in fullscreen policy | Prevent click-on-desktop from being misdetected as fullscreen app |
| Present-last-frame strategy on no-new-sample | Smooth rendering cadence and avoid quality governor being driven by synthetic drops |
| Source timestamp aware render cap | Reduce cadence jitter when source fps is below configured cap |
| Windows subsystem build (`-mwindows`) | Remove startup console window for desktop app UX |
| Hard fallback on pipeline failures | Protect foreground performance target |

## Issues Encountered
| Issue | Resolution |
|-------|------------|
| Empty repo means no baseline tests | Created new lightweight native test harness |
| Missing CMake in shell PATH | Added scripts/run_tests.ps1 with g++ path detection |
| MinGW link errors (`FOLDERID_LocalAppData`, `WinMain`) | Reworked entry to dual `main`/`wWinMain`, switched config path lookup to `SHGetFolderPathW` |
| Tray module failed to compile with MinGW due include order | Included `windows.h` before `shellapi.h` |
| MF `GUID_NULL` unresolved with MinGW linker | Used local zero-GUID constant for seek call |
| `psapi.h` compile failure | Fixed include order (`windows.h` before `psapi.h`) and used portable memory counter fields |
| `run_tests.ps1` false-green on compile failure | Added compile exit-code check before executing test binary |
| Wallpaper stuck in fallback color despite selected video | Enabled `MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING` and hardware transform attr during reader creation |
| Cursor showed busy state and desktop icons disappeared | Parented to proper WorkerW and added per-loop `PeekMessage` pumping on main thread |
| Dynamic wallpaper selected but visually no effect | Added multi-parent attach attempts and parent-relative placement |
| Dynamic wallpaper selected but render host attached to Progman (`1229x691`) and icon layer could be occluded | Introduced attach policy (`WorkerW -> DefView`) and removed Progman/Desktop fallback path |
| `main.cpp` failed to compile due constexpr integer-to-pointer cast | Replaced constexpr cast with runtime conversion and added `SetProcessDPIAware` fallback |
| Selecting same video could still reopen pipeline due path representation mismatch | Added `IsSameVideoPath` canonical matcher and switched `ApplyVideoPath` guard to equivalence check |
| 4K video playback memory stayed near 800MB | Enabled SourceReader low-latency and video-only stream, reused frame publish buffer; observed runtime drop to ~300-360MB in local measurement |

## Resources
- task_plan.md
- progress.md
- scripts/run_tests.ps1

## Visual/Browser Findings
- None

## 2026-03-28 启动体验问题调研
- 现状：`App::Initialize()` 对 `decodePipeline_->Open(config.videoPath)` 失败直接返回 false，导致“无效路径时程序看起来无响应”。
- 现状：配置缺失时 `videoPath` 为空，解码 stub 会走 fallback ticker，渲染端 `DrawFallback()` 持续绘制动态底色，形成“幕布遮盖层”。
- 现状：配置正常时首帧尚未解码前，`Tick()` 也会用 synthetic token 驱动 `DrawFallback()`，产生启动瞬时幕布闪现。
- 决策：移除“无视频也强制动态色场”的启动策略，改为“仅在有有效视频并拿到首帧后显示壁纸层”。
- 修复策略落地：新增 `startup_policy`（路径有效性 + 首帧呈现门控），并将 App 启动/换源改为“无有效视频则仅托盘运行”。
- 修复策略落地：Windows 壁纸宿主窗口改为默认隐藏，只有拿到可绘制视频纹理后才 `ShowWindow`，启动瞬时不再出现幕布闪现。
- 验证：`scripts/run_tests.ps1` 全绿（36/36），`scripts/build_app.ps1` 构建成功（含 `src/startup_policy.cpp`）。
- 新增 `loop_sleep_policy`：主循环按下一帧剩余时间睡眠（1~8ms 夹紧），无活动视频走 50ms 空闲档，pause 走 30ms。
- 解码泵从固定 1/4ms 轮询改为退避策略（无解码 12ms、无帧逐步到 8ms），降低 CPU 空转。
- 新增 `RenderScheduler::TimeUntilNextRender()`，用于主循环睡眠决策。
- `DetachWallpaper()` 现在清理 `frame_bridge` 与统计采样缓存，减少无视频态内存占用。
- 验证：`run_tests` 全绿（43/43），`build_app` 成功。
- 新增 `probe_cadence_policy`，将 `IsSessionInteractive/DetectForegroundState` 从每帧调用改为按 300ms/120ms 节流采样。
- 新增 `frame_buffer_policy`，在 MF 帧发布路径引入容量回收规则（容量 > 需求 2x 时缩容）。
- 目标：进一步降低 CPU 系统调用开销与长期运行后的帧缓存驻留内存。
- 验证：`run_tests` 全绿（51/51），`build_app` 成功。
- 循环调度继续降频：主循环 active wait 上限从 8ms 提升到 20ms，解码泵无帧退避上限从 8ms 提升到 12ms。
- `Tick` 的 pause 路径从“每轮重置”改为“进入 pause 边沿重置一次”，并清空 frame bridge + 最近呈现缓存，降低 pause 期间 CPU 与驻留内存。
- 验证：`run_tests` 全绿（51/51），`build_app` 成功。
- 新增 `pause_resource_policy` 并接入 `App::Tick`，全屏进入时由 `Pause` 升级为 `Stop + Detach`，退出全屏后按配置重建视频管线。
- 全屏档继续降唤醒：主循环 pause 睡眠 80ms，解码泵无解码准备睡眠 30ms。
- 目标：让全屏场景下 CPU/内存变化“可感知”，而非仅小幅波动。
- 验证：`run_tests` 全绿（53/53），`build_app` 成功。
- 调整全屏策略：从“Stop + Detach”升级为“Stop decode + Keep wallpaper layer when last frame exists”。
- 效果预期：全屏期间看起来像静态壁纸；退出全屏后自动恢复动态播放，避免退回系统原壁纸。
- 验证：`run_tests` 全绿（54/54），`build_app` 成功。
- 修复全屏识别：Windows 路径优先读取 `DWMWA_EXTENDED_FRAME_BOUNDS`，并使用 12px 容差判定窗口是否覆盖监视器。
- 目的：解决边框/DPI/窗口边界偏差导致“明明全屏却未识别”的问题。
- 验证：`run_tests` 全绿（55/55）；`build_app` 因 `build/wallpaper_app.exe` 被占用失败，改 `-BuildDir build_tmp` 构建通过。
- 修复 1：暂停时保留壁纸层常驻（不再默认 Detach），避免长时间全屏后回桌面直接露出系统静态壁纸。
- 修复 2：退出全屏恢复解码加入每秒重试机制，防止首次恢复失败导致后续永不恢复。
- 修复 3：全屏识别放宽：只要窗口覆盖监视器且非 shell 类窗口，即可判定全屏（不再强依赖可见性标记）。
- 验证：`run_tests` 全绿（55/55），`build_app` 成功。
- 全屏识别修复：由“严格覆盖”升级为“严格覆盖 OR 无边框弹窗且覆盖率>=90%”，提升对边框化全屏/独占切换态的识别率。
- 状态稳定修复：前台检测返回 `kUnknown` 时不覆盖缓存状态，避免短时 API 抖动把全屏误回退为窗口态。
- 策略保持：全屏暂停时壁纸层常驻，退出全屏恢复支持重试。
- 验证：`run_tests` 全绿（56/56），`build` 和 `build_tmp` 均构建成功。
- 新增配置项 `pauseOnMaximized`（默认 false）；当为 true 时，最大化窗口也会触发壁纸暂停策略。
- 前台状态新增 `kMaximized`，并在 Windows 路径用 `GetWindowPlacement(...SW_SHOWMAXIMIZED...)` 识别最大化。
- 保持现有逻辑：全屏与最大化分别由独立开关控制，避免误暂停。
- 验证：`run_tests` 全绿（58/58），`build_app` 成功。
- 新增 `pause_transition_policy`，将 raw pause 信号转为稳定状态，避免前台抖动导致停播/恢复抖动。
- `Tick` 中引入 enter/exit 迟滞（160ms/240ms），切换更平滑。
- 前台检测增强：过滤最小化窗口、DWM cloaked 窗口；全屏候选规则升级为 `coverage>=98.5%` 或 `无边框且coverage>=88%`。
- 验证：`run_tests` 全绿（62/62），`build_app` 成功。
- 采用“桌面上下文驱动”替代单纯全屏触发：当前台不属于桌面上下文时默认暂停为静态，返回桌面上下文后自动恢复动态。
- 桌面上下文判定：shell 类窗口 + `CabinetWClass` + `explorer.exe` 进程。
- 新增配置 `pauseWhenNotDesktopContext`（默认 true），并保留 `pauseOnFullscreen/pauseOnMaximized` 作为独立开关。
- 验证：`run_tests` 全绿（67/67），`build_app` 成功。
- 配置收敛：移除 `pauseOnFullscreen` / `pauseOnMaximized` 配置通路，仅保留 `pauseWhenNotDesktopContext`。
- 丝滑优化：暂停进入先软暂停（`Pause`，保留解码上下文），仅在持续暂停 >= 1800ms 才硬挂起（`Stop`），恢复时短暂停直接 `Start`，长暂停走重建。
- 迟滞参数微调：enter 110ms，exit 180ms，降低体感顿挫。
- 验证：`run_tests` 全绿（67/67），`build_app` 成功。
- 进一步丝滑优化：轻暂停路径不再清空 frame bridge 与最新解码 token，保持最后一帧静态连续显示，减少“从动态切静态”的顿挫感。
- 硬挂起触发策略收敛：仅在会话不可交互或桌面不可见时允许硬挂起；普通非桌面上下文（全屏/分屏应用）保持轻暂停，恢复更快更稳。
- 硬挂起阈值从 1800ms 提升到 8000ms，避免中短时切换频繁触发 Stop/Open 带来的恢复卡顿。
- 验证：`run_tests` 全绿（67/67），`build_app` 成功。
- CPU 迭代：主循环与解码泵暂停态/空闲态降频（pause 110ms、no-video 70ms、decode-no-ready 45ms、decode-backoff cap 16ms），减少无效唤醒。
- CPU 迭代：探测节流改为“运行态高频、暂停态低频”（session/foreground = 300/120ms -> 1200/260ms）。
- 内存迭代：轻暂停持续超过 1500ms 后仅释放解码帧缓存（`TrimMemory()`），在不 Stop 管线的前提下降低 CPU 侧帧缓存驻留。
- 验证：`run_tests` 全绿（72/72），`build_app` 成功。
- 进一步抑制长期动态 CPU：解码泵拿到帧后不再 0ms 忙轮询，最小睡眠调整为 2ms。
- 进一步提升长静态内存回收：非系统态暂停达到 12s 后执行保守深挂起（Stop 解码），并调用 `EmptyWorkingSet` 做进程工作集回收。
- 深挂起阈值改为双档：系统不可交互/桌面不可见保持 8s，普通非桌面上下文使用 12s，兼顾频繁切换流畅性与长静态内存下降。
- 验证：`run_tests` 全绿（74/74），`build_app` 成功。
- 修复“长静态后恢复卡顿”：在退出 pause 的迟滞窗口中预热 `Open`，真正恢复时仅执行 `Start`，减少深挂起后的首帧恢复卡顿。
- 预热窗口优化：深挂起场景 exit delay 动态提升到 360ms，为预热让路；若检测到又回到暂停态，立即回收预热资源避免反复抬升内存。
- 验证：`run_tests` 全绿（75/75），`build_app` 成功。
- 新增解码泵热路径策略：按渲染上限设置有帧 sleep（60fps=4ms, 30fps=8ms），降低长时间动态时解码线程 CPU 占用。
- 修复解码泵忙轮询根因：拿到帧分支现在也执行 sleep，避免长期动态壁纸下出现高频自旋。
- 恢复丝滑度继续优化：深挂起恢复阶段从“Open 预热”提升到“Open + Start 预热”，切回动态时减少首帧等待顿挫。
- 验证：`run_tests` 全绿（76/76），`build_app` 成功。
- 托盘状态可视化：Set 30/60、Enable/Disable Auto Start、Enable/Disable Adaptive Quality 增加勾选状态，当前生效项置灰。
- App 侧状态同步：新增 `SyncTrayMenuState`，在初始化及每次配置变更后同步真实状态，避免“点了什么看不出来”。
- 清除/重选/退出卡顿优化：解码停止路径移除频繁 `MFShutdown`，将 MF runtime 常驻进程生命周期，降低同步停启阻塞。
- 验证：`run_tests` 全绿（76/76），`build_app` 成功。
- 修复“选视频后变静态直到再点托盘”：前台属于本进程时直接判定为桌面上下文，避免托盘/文件对话框误触发 pause。
- 托盘交互去抖：新增 1.2s 桌面上下文保护窗口，消除菜单关闭后的短暂误判和切换顿挫。
- 视频切换路径减阻：去掉非空切换前的冗余 `Stop`，避免 Stop/Open 双重重置造成清除后重选卡顿。
- 验证：`run_tests` 全绿（76/76），`build_app` 成功。
- 修复“打开托盘菜单必切动态”：新增菜单开/关事件并在交互期间冻结上下文探测，保持原有动态/静态状态不被托盘交互改写。
- 取消托盘交互强制动态策略：不再把 `desktopContextActive` 直接置 true，仅在冻结窗口结束后恢复正常探测。
- 退出/切换卡顿优化：解码泵 sleep 改为可中断等待，停止时即时唤醒，降低线程 join 阻塞时间。
- 验证：`run_tests` 全绿（76/76），`build_app` 成功。
- 长动态 CPU 压降：解码泵 no-ready 睡眠提升到 70ms，hot-sleep 提升到 60fps=6ms、30fps=12ms，降低持续动态场景轮询频率。
- 丝滑恢复兜底：所有 `Start()` 恢复路径显式唤醒解码线程，避免更保守睡眠策略造成恢复迟滞。
- 验证：`run_tests` 全绿（76/76），`build_app` 成功。
- 本轮进一步压降长动态 CPU：hot-sleep 调整为 `60fps=8ms, 30fps=16ms`，降低解码线程高频拉帧。
- 本轮进一步压降探测开销：运行态 probe 调整为 `session=450ms, foreground=180ms`，减少长期 Win32 查询频次。
- 丝滑修复：修正 `decodePump` 等待条件，`WakeDecodePump()` 现在可即时打断等待；并为前台进程名查询增加 PID 级缓存与桌面类/本进程快速判定。
- 验证：`run_tests` 全绿（76/76），`build_app` 成功。
- 非桌面启动优化：引入前台探测失败计数策略，连续失败达到阈值后保守判定为非桌面上下文，避免启动阶段长期保持动态导致的 CPU/内存偏高。
- 非桌面启动优化：探测失败初期不立即进入探测节流窗口，按帧快速重试，缩短“先动态后降载”的过渡时长。
- 验证：`run_tests` 全绿（79/79），`build_app` 成功。

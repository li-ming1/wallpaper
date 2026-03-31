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
- 新增 `long_run_load_policy`：按 1s 采样累积“中压/高压/冷却”计数，分级输出 decode hot-sleep 增量（`+4ms/+8ms`）并带迟滞回落。
- 新增长期内存治理：工作集超过高水位后触发 `TrimMemory`，并设置 30s 冷却窗口，避免频繁 trim 导致体验抖动。
- `App` 接入长期负载决策：在 `MaybeSampleAndLogMetrics` 中每秒更新策略状态，动态调整 decode pump 节奏。
- `decode_pipeline_stub` 的运行态 trim 改为“延迟到下一帧收缩缓冲”，避免直接清桥接帧带来的可见闪烁。
- 验证：`run_tests` 全绿（83/83），`build_app` 成功。
- 参考微软文档后继续优化：高精度计时器并非应全程开启，改为仅在“活跃 60fps 动态且低压力”场景启用，减少长时间运行系统级定时器开销。
- 参考 DXGI 官方建议：设置 `SetMaximumFrameLatency(1)`，降低预渲染队列带来的额外延迟与内存占用。
- 参考 D3D11 官方建议：将高频 CPU 更新纹理路径切到 `D3D11_USAGE_DYNAMIC + Map(WRITE_DISCARD)`，替代默认 `UpdateSubresource` 热路径。
- 验证：`run_tests` 全绿（85/85），`build_app` 成功。
- 继续参考前人资料（微软 DXGI flip model 文档）：推荐使用 flip model 获得更高效的桌面合成路径。
- 本轮渲染链路补充：D3D 设备启用 `D3D11_CREATE_DEVICE_SINGLETHREADED`；swapchain 优先 `FLIP_DISCARD` 并保留 `FLIP_SEQUENTIAL` 回退，提高兼容性。
- 验证：`run_tests` 全绿（85/85），`build_app` 成功。
- 新增 DXGI waitable object 优化：swapchain 创建优先启用 `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT`，并在可用时改用 `IDXGISwapChain2` 的帧延迟控制。
- Present 前增加等待对象就绪检查（非阻塞）：队列未就绪则跳过本帧，减少主线程无效渲染提交导致的长时 CPU 波动。
- 验证：`run_tests` 全绿（85/85），`build_app` 成功。
- 回归问题修复：waitable-object 路径在部分机型上会连续 `WAIT_TIMEOUT`，导致长期不 present，表现为“动态壁纸消失”。
- 新增 `frame_latency_policy`：连续超时达到阈值后强制提交一帧（2 次超时后强制），并要求首帧成功后才启用 waitable 门控。
- 异常降级：`WaitForSingleObjectEx` 返回 `WAIT_FAILED` 时自动关闭 waitable 门控，优先保证可见性。
- 验证：`run_tests` 全绿（89/89），`build_app` 成功。
- 用户现场反馈“没用”后做稳定优先处置：默认关闭 waitable-object 门控与对应 swapchain 标志，仅保留代码实现，避免不同驱动组合下再次触发显示回归。
- 结论：先保证动态壁纸稳定可见，再做设备白名单/运行时探测后按机型启用该优化。
- 原因复盘（结合 Win 官方“waiting when occluded”文档）：壁纸窗口在某些桌面合成路径下可能进入 occluded 状态，waitable-object 与非阻塞跳帧叠加后会放大“长期不 present”风险。
- 新增遮挡降载策略：`Present` 返回 `DXGI_STATUS_OCCLUDED` 时停止提交并按 250ms 节流探测恢复，降低不可见时无效 CPU/GPU 消耗。
- 图标修复：资源文件增加标准图标 ID `1`，并在托盘/壁纸窗口类上设置 `hIcon/hIconSm`，改善任务管理器中显示默认图标的问题。
- 2026-03-28 追加发现：30fps 场景存在两个隐藏轮询点导致 CPU 基线偏高：
  1) 主循环 sleep 上限 24ms 低于 30fps 帧间隔（33ms），导致每帧出现额外唤醒。
  2) 解码热睡眠（30fps=16ms）接近 60Hz 拉帧，形成“解码频率 > 渲染频率”的无效开销。
- 2026-03-28 追加决策：
  - 主循环 active sleep 上限调至 40ms；解码热睡眠调整为 60fps=14ms、30fps=28ms。
  - SourceReader 打开策略改为“硬件变换优先，软件视频处理回退”，优先争取低 CPU 的转换路径。
  - 图标加载链路改用 `LoadImage` 多尺寸，提升任务管理器图标命中率。
- 2026-03-28 追加发现：即便目标帧率已降至 30fps，CPU 峰值仍受“解码输出到 CPU + 上传到 GPU”的双拷贝结构限制。
- 2026-03-28 追加决策：
  - 打通 MF D3D manager 互操作主路径并新增 `decode_path` 可观测字段；
  - 回退链路先消除一份无效 CPU 拷贝（外部缓冲视图桥接），避免继续“拆东墙补西墙”。
- 2026-03-28 追加发现：在纯 CPU 回退路径下，解码拉帧频率即使已降到 30fps 仍偏高，导致 CPU 峰值黏在 7~8%。
- 2026-03-28 追加决策：对 CPU 回退链路单独拉大 hot-sleep 与高压回收力度，优先压低长期运行基线负载。
- 2026-03-28 追加发现：黑幕闪现并非单点故障，而是“主循环运行态 trim + CPU 回退路径清帧”叠加引发；即使渲染侧有保帧逻辑，也会在清帧窗口出现可见瞬断。
- 2026-03-28 追加决策：新增 `runtime_trim_policy`，将长时压力 trim 限制为“非 CPU 回退动态路径”执行；CPU-only 机器优先保连续画面，用工作集回收承担内存治理。
- 2026-03-28 追加决策：`DecodePipeline::TrimMemory` 在运行态不再清桥接帧，且只在 GPU 零拷贝路径执行 `SourceReader->Flush`，降低动态播放顿挫风险。
- 2026-03-28 追加发现：CPU-only 场景下，单靠帧率降档（30fps）仍会受“高分辨率 RGB32 解码 + 每帧上传”限制，CPU 基线难继续下探。
- 2026-03-28 追加决策：把“自适应视频质量”升级为联合策略；在 MF CPU 回退路径对输出像素总量做上限控制（默认上限约 1280x720），从源头减少解码与内存带宽压力。
- 2026-03-28 追加决策：解码泵线程按运行态切换线程优先级（`THREAD_PRIORITY_BELOW_NORMAL/IDLE`），用 Windows 原生调度降低后台竞争与长期占用。
- 2026-03-29 新增发现：现有 CPU 回退链路即使有 720p 限幅，长时压力下仍会受 RGB32 上传和后台调度竞争影响，单纯调 sleep 的边际收益已经很低。
- 2026-03-29 新增决策：
  - `DecodeOpenProfile` 取代 `Open(path, codecPolicy, adaptiveQuality)`，把长期负载档位与硬件变换偏好显式传入解码层。
  - `decode_output_policy` 升级为双档：CPU 回退常态上限 720p，高压长时上限 540p；720p 及以下输入保持 native，不再继续缩。
  - `long_run_load_policy` 增加 `decode_path` 维度，CPU 回退 medium/high hot-sleep boost 从 `8/16ms` 提升到 `14/28ms`。
  - `ResourceArbiter` 新增 `battery saver`、`remote session`、`display off/occluded` 电源态，并将其统一视为硬暂停来源。
  - `RuntimeMetrics`/CSV 新增 `decode_output_pixels`、`thread_qos`、`occluded`、`power_state`，便于区分“内容复杂度”与“系统态降载”。
  - decode 线程已接入 `SetThreadInformation(ThreadPowerThrottling/ThreadMemoryPriority)`，常态走 EcoQoS + low memory priority。
- 验证：`run_tests` 全绿（106/106），`build_app` 成功。
- 2026-03-29 追加发现：大量动态壁纸素材实际是 24fps / 25fps；现有 30/60 双档会把这类素材按 30fps 拉帧，长期运行时 decode pump 存在额外唤醒。
- 2026-03-29 追加决策：
  - 新增 `source_frame_rate_policy`，根据 MF 时间戳识别 24/25/30/60fps 源帧率，并带 4 样本迟滞。
  - `ComputeDecodePumpHotSleepMs` 改成按“渲染档位 + 源帧率”联合决策：30fps=28ms、25fps=34ms、24fps=36ms。
  - App 侧移除旧的 `sourceFpsHint30/sourceFpsHint60` 双档逻辑，统一改用 `SourceFrameRateState`。
- 验证：`run_tests` 全绿（112/112），`build_app` 成功。
- 2026-03-29 新增发现：`decode_pipeline_stub` 仍在 decode pump 轮询路径内同步调用 `IMFSourceReader::ReadSample(...)`，这使 MF 解码线程仍依赖外层唤醒节奏，属于长动态 CPU 热点之一。
- 2026-03-29 官方文档核对：
  - Source Reader 异步模式需要在创建 reader 时设置 `MF_SOURCE_READER_ASYNC_CALLBACK`。
  - 异步模式下 `ReadSample` 立即返回，最后四个出参必须传 `NULL`，每次请求对应一次 `IMFSourceReaderCallback::OnReadSample` 回调。
- 2026-03-29 追加决策：
  - 新增 `decode_async_read_policy`，用纯状态机约束“单 in-flight 请求、ready sample 缓存、EOF seek 回绕”。
  - `decode_pipeline_stub` 改为异步 Source Reader：回调线程只负责缓存样本；`TryAcquireLatestFrame` 只消费缓存并在消费后补发下一次请求。
  - 运行态 `TrimMemory()` 暂停对 Source Reader 执行 flush，先避免异步 in-flight request 与 flush 交错带来的状态复杂度和潜在 `MF_E_NOTACCEPTING` 问题。
- 验证：`run_tests` 全绿（117/117），`build_app` 成功。
- 2026-03-29 新增发现：策略层虽然已识别 `kCpuNv12Fallback`，但 Win 运行态仍停留在 `RGB32` CPU 回退上传；这意味着 CPU-only 机器仍在承受 4 字节/像素上传带宽，NV12 只存在于类型层而未落到数据面。
- 2026-03-29 官方/实现侧结论：
  - 在非 D3D 互操作路径下优先协商 `MFVideoFormat_NV12`，可以把 CPU 回退的桥接/上传数据量从 `RGBA 4Bpp` 降到 `NV12 1.5Bpp`。
  - 渲染端不必先做 CPU 颜色转换；直接上传 `Y=R8_UNORM`、`UV=R8G8_UNORM` 两张动态纹理，在像素着色器里做 BT.709 limited-range 变换即可。
- 2026-03-29 追加决策：
  - `decode_pipeline_stub`：非 D3D 互操作优先 `NV12 -> RGB32` 协商；命中 `NV12` 时优先走 `IMF2DBuffer::Lock2D` 发布平面视图，失败再回退连续缓冲。
  - `wallpaper_host_win`：新增 NV12 双平面动态纹理与专用像素着色器，保持“只上传、不在 CPU 上转 RGBA”。
  - `App`：把 `NV12` 与 `RGB32` 统一视为 CPU fallback，沿用现有 hot-sleep、working-set trim、adaptive reopen 策略。
- 验证：`run_tests` 全绿（122/122），`build_app` 成功。
- 2026-03-29 追加发现：顶部绿色色带不是通用颜色矩阵问题，更像 `NV12` UV 平面起点计算错误。
  - 现有 `Lock2D` 路径默认 `uv_offset = visibleHeight * pitch`。
  - 但部分 MF/NV12 buffer 会按更高的对齐高度分配，例如 `540 -> 544`；此时 UV 实际起点在 `alignedHeight * pitch`，顶端会读到错误 chroma，表现为顶边绿条。
  - 另外 sample 可能包含多个 buffer；直接对 `GetBufferByIndex(0)` 做单-buffer 假设并不稳。
- 2026-03-29 追加决策：
  - 新增 `nv12_layout_policy`，根据 `frameHeight / pitch / totalBytes` 反推对齐后的 Y 行数，统一得到 UV 平面真实偏移。
  - `decode_pipeline_stub` 的 NV12 直视图路径改为“仅单 buffer sample 且 `IMF2DBuffer` 可用时启用”；其余情况走 `ConvertToContiguousBuffer` 保守回退，优先保证正确性。
  - 这次修复同时保留了安全场景下的 `Lock2D` 直视图快路径，没有把全部 NV12 强行退回成每帧额外拷贝。
- 验证：`run_tests` 全绿（125/125），`build_app` 成功。
- 2026-03-29 新增发现（系统审查续轮）：`App::ApplyRenderFpsCap` 在高频路径会无条件 `WakeDecodePump()`；当 `fpsCap` 与 `decode hot-sleep` 未变化时，这属于纯额外线程唤醒，会带来不必要 CPU 调度开销。
- 2026-03-29 新增决策：
  - 增加 `ShouldWakeDecodePumpForRenderCapUpdate`，只在“渲染帧率档位变化”或“解码热睡眠变化”时唤醒解码泵。
  - `App::ApplyRenderFpsCap` 改为条件更新：`decodePumpHotSleepMs_` 仅在值变更时写入，`WakeDecodePump()` 仅在策略判定需要时触发。
- 验证：`run_tests` 全绿（126/126），`build_app -BuildDir build_tmp` 成功。
- 2026-03-29 继续迭代发现：`wallpaper_host_win` 每次 `Present` 都调用 `frame_bridge::TryGetLatestFrame`，即使无新帧也会进入互斥锁并复制 `LatestFrame`（含多个 `shared_ptr`），属于长期动态热点。
- 2026-03-29 继续迭代决策：
  - `frame_bridge` 增加 `TryGetLatestFrameIfNewer(lastSeenSequence, outFrame)`，先走原子序列号快判，只有新帧才进入锁区复制，降低无效锁竞争。
  - `wallpaper_host_win` 接入新接口，去掉“取完再比较 sequence”的低效路径。
  - 解码泵无帧退避从线性 `+1` 调整为分段增长（`2->4->8->12...->24ms`），更快进入低唤醒区，进一步压低空转 CPU。
  - `presentSamplesMs_` 改为固定预留容量并移除 `shrink_to_fit`，避免 attach/detach 周期带来的堆收缩抖动与碎片化风险。
  - NV12 `Lock2D` 快路径移除 `new Locked2DBufferHolder`，改为共享对象别名持有，减少每帧额外堆分配。
- 验证：`run_tests` 全绿（127/127），`build_app -BuildDir build_tmp` 成功。
- 2026-03-29 继续迭代发现：`WakeDecodePump` 在高频路径可能连续重复 `notify_all`，而解码泵仅单线程等待，重复广播属于无效调度开销。
- 2026-03-29 继续迭代决策：
  - 新增 `ShouldNotifyDecodePumpWake`，仅在 wake 标记从 false->true 时发送通知，并改用 `notify_one`，降低条件变量广播成本。
  - `wallpaper_host_win` 增加 viewport 尺寸缓存（初始化/resize/swapchain resize 时更新），绘制路径直接使用缓存，避免每帧 `GetClientRect` Win32 调用。
  - 清理 `decode_pipeline_stub` 中未使用的 `asyncReadyFlags_` 字段与相关写入，减少状态噪声。
- 验证：`run_tests` 全绿（128/128），`build_app -BuildDir build_tmp` 成功。
- 2026-03-29 继续迭代发现：`App::Tick` 在每个渲染周期都会进入 `decodedTokenMu_`，即使解码序列未变化也要走一次锁检查；在 `30fps` 源 + `60fps` 渲染场景属于稳定的无效锁竞争热点。
- 2026-03-29 继续迭代决策：
  - 新增 `decode_token_gate_policy`，先用原子序列号判断“是否可能有新 token”，仅在序列前进时才进入互斥区复制 token。
  - `App` 新增 `latestDecodedSequence_` 原子镜像，解码泵发布新 token 后同步 `store(release)`；渲染侧 `load(acquire)` 做前置门控。
  - 在 `hasLastPresentedFrame_` 且序列未前进时，`Tick` 直接复用 `lastPresentedFrame_`，避免无意义锁进入。
- 验证：`run_tests` 全绿（132/132），`build_app -BuildDir build_tmp` 成功。
- 2026-03-29 新增现场问题：首次运行阶段 CPU/内存峰值偏高，用户体感明显。
- 2026-03-29 新增决策（首启降峰）：
  - 新增 `ConfigStore::Exists()` 用于识别“配置文件是否在本次启动前已存在”。
  - 新增 `ShouldDeferVideoDecodeStart(configExistedBeforeLoad, hasValidVideoPath)`：
    - 首次运行且存在有效视频时，初始化阶段先 `Open` 管线但不 `Start` 解码。
    - 真正 `Start` 推迟到运行循环首轮探测之后，由既有 pause/desktop-context 策略决定是否启动。
  - `StartVideoPipelineForPath` 新增 `startDecodeImmediately` 参数，保持非首启路径行为不变。
  - `Run` 中 `hasActiveVideo` 收敛为 `decodeOpened && decodeRunning`，避免“已 Open 但未解码”阶段被当作活跃视频，减少不必要高精度定时与调度。
- 预期效果：首启时解码线程和帧上传热路径推迟启动，可明显降低启动瞬时 CPU 抬升和内存峰值。
- 验证：`run_tests` 全绿（136/136），`build_app -BuildDir build_tmp` 成功。
- 2026-03-29 用户反馈复现：桌面上下文长期运行比进入全屏/非桌面上下文高约 30MB，且不回收；切换到非桌面后可回收。
- 根因定位：
  - 桌面动态路径下 `frame_bridge` 会持有“最新帧”的底层样本内存（CPU/NV12/GPU holder），用于跨线程交接。
  - 在高分辨率素材下这一帧可能接近 30MB；而进入非桌面 pause 后会清桥接帧，所以出现“立刻降 30MB”的体感。
- 2026-03-29 新增决策（常驻差值回收）：
  - 新增 `ReleaseLatestFrameIfSequenceConsumed`：仅当渲染线程确认已消费某序列帧时，释放 bridge 对该帧的持有。
  - `wallpaper_host_win` 在成功 `Copy/Upload + Draw` 后按序列调用释放，确保不会误删更新序列。
  - 这样桌面动态路径也不会长期常驻那一帧样本内存，缩小与 pause 场景的常驻差值。
- 验证：`run_tests` 全绿（138/138），`build_app -BuildDir build_tmp` 成功。
- 2026-03-29 用户二次反馈：首启峰值仍高，且要求单实例保护。
- 2026-03-29 新增决策（再次压峰）：
  - 首启延迟解码从“仅 deferred”升级为“deferred + 2.5s 最小等待窗口”，降低首次运行进入解码热路径的即时冲击。
  - MF 异步读取策略新增 `ShouldIssueReadImmediatelyAfterConsume=false`，消费样本后不立即 prefetch 下一帧，减少桌面动态场景 ready-sample 常驻内存。
  - `main.cpp` 增加单实例互斥锁（`Local\\WallpaperDynamicDesktop.Singleton`），重复启动直接退出，避免多实例叠加导致 CPU/内存暴涨。
- 验证：`run_tests` 全绿（142/142），`build_app -BuildDir build_tmp` 成功。
- 2026-03-29 新增现场问题：用户反馈“仍可多实例”，说明单实例实现在真实环境下仍有放行路径。
- 根因定位：
  - `main.cpp` 里 `TryAcquire` 逻辑在 `Global\\...` 获取失败后无条件尝试 `Local\\...`。
  - 当失败原因本身是“已有实例”（如 `ERROR_ALREADY_EXISTS`）时，回退 `Local` 会错误放行第二实例。
  - 额外的进程枚举守卫启动成本高，且在权限不对等场景可能无法稳定识别既有进程，带来误判风险。
- 2026-03-29 新增决策（单实例强一致修复）：
  - 新增 `ShouldFallbackToLocalMutex`，仅在非“已有实例”错误下允许从 `Global` 回退 `Local`。
  - `ScopedSingleInstanceMutex` 改为显式区分 mutex 与 lock file 获取状态，`ShouldAllowSingleInstanceStartup` 按双守卫判定。
  - 移除进程枚举守卫，保留命名互斥 + 独占 lock file 两级守卫；同时修复 lock file 句柄未释放问题。
- 验证：
  - `run_tests` 全绿（146/146）
  - `build_app -BuildDir build_tmp` 成功
  - 本机双开验证：第二实例立即退出，仅保留 1 个进程
- 2026-03-29 新增需求：按 C++26（可用即启用，不可用回退）进行系统级性能/内存优化，并要求可在 MSYS2/GCC 编译落地。
- 2026-03-29 环境核查结论：
  - 当前工具链：MSYS2 `g++ 15.2.0`。
  - 可用：`if consteval`、`consteval`、`std::expected`、`[[assume]]`（可门控）。
  - 不可直接用（当前标准库/编译器组合）：标准化静态反射、标准模式匹配、标准 P2300 sender/receiver、`std::mdspan` 宏未稳定暴露。
- 2026-03-29 新增决策（C++23->C++26 门控）：
  - 项目基线从 C++20 升级到 C++23；脚本支持 `-UseCxx2c` 开关，满足“可用即启用”的 C++26 实验切换。
  - 增加 `cpp26_feature_support` 统一门控，避免“特性名可写但本地不可编译”的伪优化。
  - `ConfigStore` 新增 `LoadExpected/SaveExpected`，`App` 去除 `std::async` 配置读写路径，降低首启线程抖动。
  - `MetricsSampler` 从 `erase(begin)` 改为环形缓冲 O(1) 推入，消除线性搬移。
  - 新增 `WP_ASSUME` 并用于 D3D 上传热路径不变量，减少编译器保守分支。
- 验证：
  - `./scripts/run_tests.ps1` -> 152/152 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp` -> 成功
  - `./scripts/run_tests.ps1 -UseCxx2c` -> 152/152 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp -UseCxx2c` -> 成功
- 2026-03-29 继续迭代发现：`frame_bridge::LatestFrame` 持有 `rgbaPixels` 与 `rgbaDataHolder` 双通道，其中前者在运行态基本不使用，但会参与每帧结构复制。
- 2026-03-29 新增决策：
  - 删除 `LatestFrame::rgbaPixels`，统一依赖 `rgbaData + rgbaDataHolder` 表达数据视图与所有权。
  - `PublishLatestFrame` 直接对入参构建 alias holder，减少成员赋值与每帧复制体积。
- 验证：
  - `./scripts/run_tests.ps1` -> 152/152 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp` -> 成功
- 2026-03-29 继续迭代发现：解码泵在“已打开但暂时无新帧”时仍依赖短周期退避轮询；即使有异步 SourceReader 回调，也未直接驱动唤醒，存在额外 CPU 调度。
- 2026-03-29 新增决策：
  - `IDecodePipeline` 增加帧就绪通知接口，统一为上层提供“有新样本”事件。
  - `decode_pipeline_stub` 在 `OnReadSample` 缓存新样本后触发通知，上层 `App` 直接 `WakeDecodePump()` 去重唤醒。
  - `ComputeDecodePumpSleepMs` 新增 `frameReadyNotifierAvailable` 维度，在通知可用场景把无帧退避上限从 `24ms` 拉高到 `40ms`，降低空转唤醒密度。
- 验证：
  - `./scripts/run_tests.ps1` -> 153/153 PASS
  - `./scripts/build_app.ps1 -BuildDir build_tmp` -> 成功
  - `./scripts/run_tests.ps1 -UseCxx2c` -> 153/153 PASS
- 2026-03-29 Phase 59 新增结论：
  - `C++26` 与 `C++2c` 在当前 `g++ 15.2.0` 均可用，构建脚本改为优先 `-std=c++26` 自动回退 `-std=c++2c`，避免命名歧义。
  - `frame_bridge` 全局锁已替换为原子 `shared_ptr` 发布模型，`TryGetLatestFrame*` 不再进入互斥区，减少渲染线程与解码线程锁竞争。
  - `decode_pipeline_stub` 媒体发布链路完成锁域拆分：样本快照在锁内提取，`PublishSampleToBridge` 重操作在锁外执行，降低临界区占用。
  - 解码泵在 frame-ready notifier 可用时改为事件优先等待（最小 90ms 窗口），进一步压降“无新帧”状态的轮询唤醒。
  - 新增 `scripts/bench_perf.ps1`，统一输出 CPU/PrivateBytes/WorkingSet 采样 CSV+JSON，支持 startup/desktop/fullscreen 场景，便于做前后版本量化对比。
- 2026-03-29 Phase 60 新增结论：
  - `ComputeDecodePumpSleepMs` 在 notifier 路径上限扩展到 64ms，配合 `app` 最小 140ms 等待窗口，进一步压降无帧轮询 CPU。
  - `frame_bridge` payload 分配改为 PMR 同步池，针对每帧发布的 `shared_ptr` 对象分配减少堆碎片风险。
  - 基准脚本在并行运行时会受单实例互斥影响导致第二个场景采样为空；后续基准需顺序执行场景。
- 2026-03-29 Phase 61 新增结论：
  - 新增 `ShouldUseMainLoopMessageAwareWait`：主循环仅在 `pause/idle` 场景启用消息等待，动态桌面路径改为固定 sleep，避免输入消息导致的提前唤醒。
  - Win32 消息等待掩码从 `QS_ALLINPUT` 收敛到 `QS_POSTMESSAGE|QS_SENDMESSAGE|QS_TIMER`，减少高频输入噪声对调度的干扰。
  - `decode_pipeline_stub` 删除 `ConsumeFrameBufferCapacityHint` 及相关原子状态（`previousPublishedCpuBytes_/trimRequested_`），移除每帧无收益原子操作。
  - 验证：`run_tests` 与 `run_tests -UseCxx26` 均为 `154/154 PASS`，`build_app` 双链路成功。
  - 同路径基准对比（`build_tmp/wallpaper_app.exe`）：
    - `startup` CPU avg `0.1437% -> 0.1259%`（约 `-12.4%`），CPU p95 `0.3832% -> 0.1918%`（约 `-50.0%`）。
    - `desktop` CPU avg `0.0320% -> 0.0237%`（约 `-25.9%`），private bytes max 维持同量级（约 `98.2MB`）。

## 2026-03-29 高危修复补充
- 修复托盘线程停止崩溃路径：`running_` 已被置 false 时，`StopMessageLoop()` 仍会 join `worker_`，避免 `std::thread` 析构触发 `std::terminate`。
- 修复 MF 异步回调竞态：`AsyncSourceReaderCallback::owner_` 改为原子指针，`Detach/OnReadSample` 使用 release/acquire，消除跨线程数据竞争。
- 解码管线新增析构清理：析构时执行 `ResetStateLocked()`，并在 `mfStarted_` 时调用 `MFShutdown()` 兜底，避免进程退出阶段资源残留。
- 修复 D3D 失败泄漏：`AttachToDesktop()` 在 `InitializeD3D()` 失败后补 `ReleaseD3D()`，防止失败重试累计泄漏。
- 修复 resize 失效：`ResizeBuffers` 失败后仍尝试 `CreateRenderTargetView()` 回退，避免渲染链路永久黑屏。
- 跨线程标志修复：`decodeFrameReadyNotifierAvailable_` 从普通 `bool` 改为 `std::atomic<bool>`，消除 decode pump 与主线程的 UB。
- C++23/26 强化：`wallpaper_host_win.cpp` 引入 `consteval` 全屏四边形顶点生成与 `std::span/subspan` 行拷贝，减少热路径重复模板化代码并保持零额外分配。
- 验证：`scripts/run_tests.ps1`（157/157），`scripts/run_tests.ps1 -UseCxx26`（157/157），`scripts/build_app.ps1`（C++23）与 `scripts/build_app.ps1 -UseCxx26` 均通过。
- 工具链观察（2026-03-29）：当前 MSYS2 `g++ 15.2.0` 不识别 `-fexperimental-library`，应改用 `-std=c++26` 直编。

## 2026-03-29 长暂停恢复慢问题修复
- 问题定位：硬挂起后恢复链路中存在固定重试等待（warmup 500ms、pipeline 1s），在桌面上下文/WorkerW 切换抖动或首次恢复失败时，用户体感会出现“恢复很久”。
- 修复策略：引入恢复重试退避策略，前几次快速重试，后续再平滑退避到上限，兼顾恢复速度与失败风暴抑制。
- warm resume 重试：`120ms -> 220ms -> 360ms -> 500ms(cap)`。
- pipeline resume 重试：`160ms -> 260ms -> 420ms -> 700ms -> 1000ms(cap)`。
- App 接入：新增 `warmResumeRetryFailures_` 与 `resumePipelineRetryFailures_`，在成功路径清零，失败路径按策略推进下一次尝试。
- 预期收益：长暂停后首次恢复失败场景的“恢复到动态”平均等待从秒级下降到亚秒级起步（首轮约 160ms/120ms）。
- 验证：`run_tests` 全绿（159/159），`build_app` C++23/C++26 均通过。

## 2026-03-29 恢复热路径补充优化（路径探测缓存）
- 问题：恢复重试期间会反复调用 `ShouldActivateVideoPipeline(path)`，其内部走 `std::filesystem::exists/is_regular_file`，在慢盘/网络盘下会拉长恢复耗时并抬高 CPU/I/O 抖动。
- 决策：新增 `video_path_probe_policy`，仅在恢复重试路径启用短时缓存（TTL 1500ms），普通换源/首次启动仍走实时探测。
- App 接入点：warm resume open 预热、resume pipeline pending、adaptive quality reopen 判定。
- 设计目标：避免恢复期高频磁盘探测，同时不牺牲路径状态变更可见性。
- 验证：`run_tests` 163/163，全构建（C++23/C++26）通过。
- 预期收益：恢复失败重试阶段 CPU 额外下降约 1~3%，慢路径恢复抖动降低，主观恢复速度提升（尤其网络路径素材）。

## 2026-03-30 09:26:52 多显示器错位问题调研
- 现状定位：src/win/wallpaper_host_win.cpp 当前渲染路径为“单窗口 + 单全屏四边形”，默认把渲染目标当作一个连续画布。
- 现状定位：宿主尺寸主要使用虚拟桌面/父窗口尺寸（GetVirtualChildBounds + GetParentBoundsOrVirtual），未按物理显示器逐块绘制。
- 风险：在分辨率/比例不一致或非规则排列（上下/错位）时，会出现跨屏拉伸、裁切和错位。
- 初步方向：引入“显示器视口拆分策略”，渲染时按每个显示器视口重复绘制同一视频纹理，避免跨屏拼接。

## 2026-03-30 09:37:48 多显示器独立渲染修复结论
- Red 结果：新增 	ests/monitor_layout_policy_tests.cpp 后，首次运行失败（缺少 monitor_layout_policy.h），证明测试能捕获缺失行为。
- Green 实现：新增 monitor_layout_policy，将“虚拟桌面坐标系下的显示器矩形”转换为“渲染目标相对视口列表”。
- 宿主接线：wallpaper_host_win.cpp 新增显示器枚举与视口缓存，DrawVideoResources() 改为按视口循环 Draw，每个屏幕独立播放同一帧，避免跨屏拼接拉伸。
- 安全回退：当渲染目标尺寸与虚拟桌面不一致或枚举失败时，回退单全屏视口，保证可见性与稳定性。
- 验证：scripts/run_tests.ps1 全绿（167/167）；scripts/build_app.ps1 构建通过。

## 2026-03-30 09:55:36 静态恢复时延优化结论
- 根因：1分钟静态后必然进入硬挂起（Stop），恢复需冷启动 Open/Start，导致体感慢。
- 新策略：分层暂停保持短中暂停走 Pause/Trim，长暂停再 Hard Suspend。
- 参数：Trim 阈值 2500ms；激进硬挂起阈值 20s（电源/会话敏感）；保守硬挂起阈值 90s（普通桌面场景）。
- 额外降延迟：暂停态主循环睡眠 110ms -> 90ms；暂停态前台探测 260ms -> 180ms。
- 预期效果：60s 静态后恢复不再走硬挂起冷启动路径，恢复时间显著缩短，同时保留长静态场景的CPU/内存节约收益。

## 2026-03-30 19:48:13 静帧呈现节流 + 指标写盘路径降载结论
- 新增呈现门控：`ShouldPresentFrame` 由“有 last frame 就呈现”升级为“新帧立即呈现 + 静帧按 keepalive 到期呈现”。
- `App` 接入静帧 keepalive 间隔 `250ms`；无新解码帧时不再每个渲染周期都 `Present`，降低 CPU/GPU 提交频率。
- `MetricsLogFile` 路径优化：
  - `Append` 改为单次 `ActivePath()` 解析，消除每次写入双次日期键计算。
  - shard 清理从“每次 EnsureReady 都执行”改为“activePath 变化或 10 分钟超时”触发，降低文件系统维护开销。
- 验证结果：
  - `run_tests` 172/172 全绿，`build_app` 通过。
  - `bench_perf`（desktop, 10s, warmup 5s）`cpu_avg_percent=0.0192`，低于此前同类样本（`~0.0284`）的量级。

## 2026-03-30 20:30:18 前台探测稳定窗口复用结论
- 问题：前台探测按固定节奏执行，即使前台窗口长时间不变，也会重复进入 `GetClassName + (可能) OpenProcess/QueryFullProcessImageNameW` 深度路径。
- 决策：新增 `ShouldReuseForegroundProbeResult`；当“前台窗口句柄未变化且距离上次深度探测未超过复用窗口（1200ms）”时，直接复用上次结论。
- 落地：
  - `App` 新增 `lastForegroundWindowHandle_` / `lastForegroundDeepProbeAt_`，在 probe 成功或保守失败路径更新。
  - `TryDetectDesktopContextActive` 改为接收外部采样句柄，避免同一轮内重复 `GetForegroundWindow()`。
- 验证结果：
  - `run_tests` 176/176 全绿，`build_app` 通过。
  - `bench_perf`（desktop, 10s, warmup 5s）`cpu_avg_percent=0.0095`，继续低于 phase73 的 `0.0192`。

## 2026-03-30 20:57:17 会话探测状态感知降频结论
- 问题：会话探测（`IsSessionInteractive/GetSystemPowerStatus/SM_REMOTESESSION`）在稳定桌面态仍按固定间隔执行，存在无效系统调用。
- 决策：新增 `SelectSessionProbeIntervalForState`，对正常稳定态做可控降频，异常态保持原频避免恢复迟滞：
  - 正常稳定态：`base * 2`（上限 `1200ms`）
  - 异常态（非交互 / 省电模式 / 远程会话）：保持 `base`
- 落地：
  - `probe_cadence_policy` 增加策略函数及测试。
  - `App::Tick` 用状态感知间隔替代固定 `probeIntervals.session`。
- 验证结果：
  - `run_tests` 179/179 全绿，`build_app` 通过。
  - 同配置短基准（均加载 `kuroha_1080p30_h264.mp4`）：
    - phase74: `cpu_avg_percent=1.4748`
    - phase75: `cpu_avg_percent=1.4038`
  - 结论：短窗样本下 CPU 平均占用小幅下降，且内存指标无异常增长。

## 2026-03-30 21:22:03 指标采样状态感知降频结论
- 问题：`MaybeSampleAndLogMetrics` 固定 1s 采样，即使在暂停/遮挡/无活跃视频状态，仍按活跃态频率调用 `GetProcessTimes/GetProcessMemoryInfo` 并拼装 CSV。
- 决策：新增 `SelectMetricsSampleInterval`，将采样节奏状态化：
  - 活跃视频且未暂停且未遮挡：`1000ms`
  - 非活跃/暂停/遮挡：`2000ms`
- 落地：在 `App::MaybeSampleAndLogMetrics` 中先做状态化时间门控，再执行系统采样与日志写盘。
- 验证结果：
  - `run_tests` 182/182 全绿，`build_app` 通过。
  - 受控同配置对比（`pauseWhenNotDesktopContext=false`，同视频路径）：
    - phase75: `cpu_avg_percent=1.4988`
    - phase76: `cpu_avg_percent=1.2087`
  - 结论：在持续播放场景下仍观察到下降（短窗样本），且内存无异常抬升。

## 2026-03-30 21:59:42 Phase 77 结论（CPU 回退输出协商重试）
- 新增 `ShouldRetryDecodeOpenWithVideoProcessing(...)`：
  - 条件：`adaptiveQuality=true` + `cpuFallbackPath=true` + 协商尺寸高于策略 hint。
  - 行为：仅触发一次 software video processing 重开，避免重复重链路。
- `decode_pipeline_stub` 落地：
  - 第一次打开走原路径；
  - 命中策略时重开一次 software video processing；
  - 若重试后仍失败，按原逻辑进入最终回退。
- 价值：减少“明知协商超标却继续常态运行”的无效路径，给后续降分辨率提供稳定钩子。

## 2026-03-30 22:20:31 Phase 78 结论（Working-Set 精细回收）
- 新增 `ShouldRequestWorkingSetTrim(...)` 策略：
  - 仅在 `hasActiveVideo` + `CPU fallback decode path` 下允许 working-set 回收。
  - long-run 分档阈值：`L0=64MB`、`L1=40MB`、`L2+=32MB`。
- `App::MaybeSampleAndLogMetrics` 接入策略，并将回收间隔从 `15s` 收紧到 `8s`。
- 受控基准（phase77 vs phase78，desktop 12s）：
  - phase77: CPU avg `1.3472%`, WS `[46.62MB, 53.28MB]`
  - phase78: CPU avg `1.3824%`, WS `[34.71MB, 47.45MB]`
- 结论：working set 明显下降，CPU 平均值基本同量级（轻微波动）。

## 2026-03-30 22:31:06 Phase 79 结论（Advanced Video Processing 协商增强）
- 新增 `ShouldEnableAdvancedVideoProcessing(...)` 策略：
  - 条件：software processing + adaptive + CPU fallback + 有效 desktop hint。
- `decode_pipeline_stub` 在 software fallback 路径启用 `MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING`（若 SDK 可用），并把同轮 `desktop hint` 复用到所有协商决策，减少重复探测。
- 受控基准（phase78 vs phase79，desktop 12s）：
  - phase78: CPU avg `1.4874%`, CPU p95 `2.3036%`, WS delta `-11.07MB`
  - phase79: CPU avg `1.4225%`, CPU p95 `2.1101%`, WS delta `-15.67MB`
- 关键风险仍在：
  - `build_tmp/phase79_app/metrics_20260330.csv` 中 `decode_output_pixels` 仍为 `2073600`（1080p），说明 CPU fallback 输出尺寸 hint 尚未真正生效。
  - 下一轮应优先增加“协商尺寸链路诊断字段”并验证具体失败点（hint 写入、media type 选择、reader 输出回读）。

## 2026-03-31 09:40:11 Phase 80 发现（硬指标复测与瓶颈定位）
- 复测基线（`build_tmp/phase79_app/wallpaper_app.exe`，desktop，12s，warmup 6s）：
  - `cpu_avg_percent = 1.5125`
  - `cpu_p95_percent = 2.8729`
  - `working_set_bytes_max = 47.49MB`
  - `private_bytes_max = 101.51MB`
- 最新运行日志（`build_tmp/phase79_app/metrics_20260331.csv`）持续显示：
  - `decode_mode = mf`
  - `decode_path = cpu_nv12_fallback`
  - `decode_output_pixels = 2073600`（1920x1080）
  - `decode_hot_sleep_ms = 64`（已在长时负载档）
- 结论：
  - 当前主路径并未进入 DXVA 零拷贝，仍是 CPU NV12 回退链路。
  - 输出像素仍为 1080p，说明既有 hint/重试策略没有把 CPU 回退链路真正压到 720p/540p。
  - 在该路径下，CPU 与 working set 很难自然落到 `1.5%/20MB` 以下，需要策略级强制降载与更激进回收门控。

## 2026-03-31 10:15:39 无损优化迭代结论（Phase 80/81）
- 变更摘要（不降分辨率、不降目标帧率）：
  - 解码重试策略：`video processing retry` 优先保留 D3D interop，不再直接强制 software-only。
  - 解码线程等待策略：notifier 可用时统一走事件驱动等待，减少“有帧场景的周期轮询”。
  - D3D 初始化：新增 `D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT`，并开启 `ID3D10Multithread::SetMultithreadProtected(TRUE)`。
  - 内存回收策略：working-set trim 阈值下探到 `20/18/16MB`，回收冷却 `8s -> 2s`。
- 受控基准（desktop, 12s, warmup 6s, 同视频 config）：
  - phase79 baseline：`CPU avg 1.5125%`, `CPU p95 2.8729%`, `WS max 47.49MB`
  - phase80 r2：`CPU avg 0.6801%`, `CPU p95 1.5378%`, `WS max 44.67MB`
  - phase82 r1：`CPU avg 0.9490%`, `CPU p95 1.6957%`, `WS max 43.03MB`
  - phase81（激进 trim）：`CPU avg 1.2180%`, `CPU p95 2.7628%`, `WS min 19.81MB`, `WS max 41.39MB`
- 关键事实：
  - 运行链路仍然稳定在 `decode_path=cpu_nv12_fallback`。
  - `decode_output_pixels` 仍是 `2073600`（1080p），CPU 回退链路像素规模没有真正下降。
  - 无损路线已显著压低平均 CPU，但内存峰值仍远高于 20MB。

## 2026-03-31 10:39:55 互操作强制绑定与 processing 规避结论（Phase 83/84）
- 新增策略：
  - `ShouldRequireD3DInteropBinding(...)`：硬件优先轮次若未成功绑定 D3D interop，则该轮判定失败，进入后续降级轮次。
  - `ShouldUseLegacySourceReaderVideoProcessing(...)`：在 `D3D interop + advanced processing` 组合下禁用 legacy processing，避免无意回到系统内存样本。
- 落地后结果（同视频配置，desktop 12s）：
  - phase83：`CPU avg 1.1850%`, `CPU p95 2.0834%`, `WS max 42.94MB`
  - phase84：`CPU avg 0.8978%`, `CPU p95 1.7074%`, `WS max 41.43MB`
- 结论：
  - 平均 CPU 继续下降，但 `decode_path` 仍固定为 `cpu_nv12_fallback`，未进入 `dxva_zero_copy`。
  - 在不降分辨率/不降目标帧率前提下，`WS max < 20MB` 仍未接近达标。

## 2026-03-31 11:35:12 Phase 89 结论（CPU 优先收敛 + WS 上限实验）
- 本轮实现：
  - `decode_output_policy` 从 `720p/540p` 下探到 `540p/432p/360p`（CPU fallback + adaptive）。
  - `decode_pipeline_stub` 增加“重试后协商结果再校验”路径，避免重试轮次静默跳过失败状态。
  - 回滚 `MFStartup(MFSTARTUP_FULL)` 到 `MFSTARTUP_LITE`，保持轻量运行时初始化。
  - 回滚“硬工作集封顶”主线策略，避免长期换页抖动。
- 主线验证（`phase89_final_app`, desktop 12s, warmup 6s）：
  - `cpu_avg_percent = 0.9438`
  - `cpu_p95_percent = 1.5334`
  - `working_set_bytes_max = 45.14MB`
  - `private_bytes_max = 105.80MB`
- 关键观测：
  - `metrics_20260331.csv` 仍显示 `decode_path=cpu_nv12_fallback`、`decode_output_pixels=2073600`。
  - 说明 SourceReader 输出尺寸 hint 在该机型/该链路上仍未真正落地，CPU fallback 仍按 1080p 处理。
- 对照实验（max-only hard cap 20MB）：
  - 结果可把 WS 压到约 `20.94MB`，但 `cpu_p95` 拉高到 `3.86%`，并出现 `decode_copy_bytes_per_sec -> 0` 的停摆样本。
  - 判定：该策略属于“以内存数字换可用性”，不满足质量要求，已回滚。
- 当前真实边界：
  - 在“保持动态壁纸连续可用 + CPU fallback + 1080p 输出未降级”前提下，可稳定做到 CPU 接近目标；
  - 但内存峰值仍显著高于 `20MB`，且强压到 20MB 会触发明显性能/连续性副作用。
- 2026-03-31 新增发现：`App` 运行态 adaptive reopen 之前只按 `longRun>=2` 才切到 decode open level2，导致 `longRun=1` 且 `decode_output_pixels` 长期 1080p 时不会触发真正降档重开。
- 2026-03-31 新增决策：
  - 在 `App::MaybeSampleAndLogMetrics` 接入 `SelectDecodeOpenLongRunLevel(...)`，让 CPU fallback 在高像素输出下可触发 level1 重开。
  - 新增 `preferHardwareTransforms` 贯穿 open/resume 路径，并记录当前 open 偏好用于恢复。
  - 新增重开失败自动回滚到旧档位，避免降档尝试导致播放中断回归。
- 2026-03-31 本机验证（`build_tmp/app_iter`）：
  - 单测：`test_red_iter` 按预期失败 1 例，`test_green_iter` 202/202 PASS；构建 PASS。
  - 基准（desktop 12s / warmup 6s）：`decode_path=cpu_nv12_fallback`、`decode_output_pixels=2073600` 仍未下降。
  - 结论：仅靠 SourceReader 尺寸 hint 仍不足以稳定压到目标，下一轮必须引入可控显式缩放链路。
- 2026-03-31 速度问题排查结论：慢放主因是 `ApplyRenderFpsCap` 在 CPU fallback 下叠加 boost 后，`decode hot-sleep` 可达到 40ms+；在 lazy-read 模型下会直接拉低解码取样节奏，体感低于 1x。
- 2026-03-31 修复决策：
  - 新增 `ClampDecodePumpHotSleepForRealtime(requested, renderFpsCap, sourceFps)`，按源帧间隔设置安全上限（预留 2ms 裕量）。
  - 在 `App::ApplyRenderFpsCap` 对所有 boost 后结果做实时上限裁剪，保留 CPU 降载但不允许慢放。
- 2026-03-31 方案取舍：
  - 试验“consume 后立即续读”（非 lazy read）可提升激进追帧，但本项目当前策略下会显著抬高 CPU 并触发 `fallback_ticker` 回归，已回退。
- 2026-03-31 验证结果（`app_speed_final`）：`cpu_avg=0.8466`、`cpu_p95=1.3432`，`decode_hot_sleep_ms` 稳定在 `31`（30fps素材），`decode_mode` 保持 `mf`，未复现 `fallback_ticker`。
- 2026-03-31 追加速度迭代：将 realtime hot-sleep 裁剪裕量从 2ms 提高到 4ms，30fps 素材稳定上限从 31ms 下调到 29ms（24fps 从 39ms 下调到 37ms）。
- 验证：`app_speed_boost` 样本中 `decode_hot_sleep_ms` 长段稳定在 `29`，`decode_mode=mf` 保持稳定，未再出现 `fallback_ticker`。
- 性能观测：`cpu_avg` 下降到 `0.7534`，`cpu_p95` 有波动（`1.7334`）但仍在可控范围。

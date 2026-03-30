# Task Plan: Windows Dynamic Wallpaper V1

## Goal
Implement a Windows 10/11 dynamic wallpaper app from an empty repo with strict performance-first architecture (WorkerW + MF + D3D11), plus tests for core logic.

## Current Phase
Phase 69

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

### Phase 29: 全量性能迭代（长动态 CPU + 切换丝滑）（Completed）
- [x] Red: 调整 `loop_sleep_policy` / `probe_cadence_policy` 测试期望并验证失败
- [x] Green: 解码泵 hot-sleep 下调频率（60fps=8ms, 30fps=16ms）
- [x] Green: 运行态 runtime probe 降频（session=450ms, foreground=180ms）
- [x] Green: 修复 `WakeDecodePump` 在运行态无法即时打断等待的问题
- [x] Green: 前台进程名查询增加 PID 缓存 + 桌面类窗口/本进程快速判定
- [x] Verification: 单测通过（76/76） + 构建通过
- **Status:** complete

### Phase 30: 非桌面启动快速降载优化（Completed）
- [x] Red: 新增 foreground-probe 失败策略测试并验证失败
- [x] Green: 连续探测失败计数策略（成功清零、失败累加）接入 probe policy
- [x] Green: 连续失败达到阈值后启用保守上下文（非桌面）以快速降载
- [x] Green: 探测失败初期不立即节流，按帧快速重试，缩短“启动后先动态一段时间”
- [x] Verification: 单测通过（79/79） + 构建通过
- **Status:** complete

### Phase 31: 长时间运行高负载治理（Completed）
- [x] Red: 新增 `long_run_load_policy` 测试并验证失败
- [x] Green: 引入长期负载迟滞策略（中/高压两级）动态提升 decode hot-sleep
- [x] Green: 引入内存高水位 trim 冷却策略（避免频繁回收引发抖动）
- [x] Green: `App` 接入长期负载策略并参与 `ApplyRenderFpsCap` 热路径计算
- [x] Green: 运行态 `TrimMemory` 改为“下一帧收缩缓冲”模式，降低可见顿挫
- [x] Verification: 单测通过（83/83） + 构建通过
- **Status:** complete

### Phase 32: 官方最佳实践驱动的 CPU/内存优化（Completed）
- [x] Red: 新增高精度计时器启用策略测试并验证失败
- [x] Green: 高精度计时器改按场景启用（仅 60fps 动态且低压力时打开）
- [x] Green: D3D 设备设置 `SetMaximumFrameLatency(1)` 限制帧队列
- [x] Green: 视频纹理上传改为 `D3D11_USAGE_DYNAMIC + Map(WRITE_DISCARD)` 路径
- [x] Verification: 单测通过（85/85） + 构建通过
- **Status:** complete

### Phase 33: 渲染链路进一步降耗（Completed）
- [x] Green: D3D 设备启用 `D3D11_CREATE_DEVICE_SINGLETHREADED`（主线程独占渲染调用）
- [x] Green: swapchain 优先 `FLIP_DISCARD`，失败回退 `FLIP_SEQUENTIAL`
- [x] Verification: 单测通过（85/85） + 构建通过
- **Status:** complete

### Phase 34: DXGI 等待对象降载优化（Completed）
- [x] Green: swapchain 优先启用 `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT`
- [x] Green: 接入 `IDXGISwapChain2` + `GetFrameLatencyWaitableObject` + `SetMaximumFrameLatency(1)`
- [x] Green: Present 前无阻塞检查等待对象，队列未就绪时跳过本帧无效提交
- [x] Verification: 单测通过（85/85） + 构建通过
- **Status:** complete

### Phase 35: waitable-object 回归修复（Completed）
- [x] Red: 新增 `frame_latency_policy` 测试并验证失败
- [x] Green: 引入“连续超时阈值后强制提交”策略，避免长期不 present
- [x] Green: waitable 门控改为首帧成功后才启用，异常等待自动降级
- [x] Verification: 单测通过（89/89） + 构建通过
- **Status:** complete

### Phase 36: 稳定优先回滚（Completed）
- [x] Green: 默认关闭 waitable-object 门控路径，保留实现待白名单启用
- [x] Green: swapchain 创建不再默认携带 waitable-object 标志
- [x] Verification: 单测通过（89/89） + 构建通过
- **Status:** complete

### Phase 37: 图标修复与遮挡降载（Completed）
- [x] Green: 应用图标资源增加标准 ID `1`，提升进程管理器识别命中率
- [x] Green: 托盘/壁纸窗口类注册补齐 `hIcon/hIconSm`
- [x] Green: 接入 DXGI occlusion 降载策略（`DXGI_STATUS_OCCLUDED` + 250ms 探测）
- [x] Verification: 单测通过（89/89） + 构建通过
- **Status:** complete
### Phase 38: CPU/内存卡点专项治理与图标链路修复（Completed）
- [x] Red: 修复长期负载策略测试与阈值期望不一致（`LongRunLoadPolicy_CoolsDownWithHysteresis`）
- [x] Green: 负载阈值按真实进程 CPU 标尺收敛（`QualityGovernor` 与 `LongRunLoadPolicy`）
- [x] Green: 主循环/解码泵睡眠策略重调，消除 30fps 场景的隐性高频轮询
- [x] Green: SourceReader 增加“硬件变换优先，软件视频处理回退”打开策略
- [x] Green: 托盘与窗口类图标改为按系统尺寸 `LoadImage` 多分辨率加载
- [x] Verification: 单测通过（89/89） + 构建通过
- **Status:** complete
### Phase 39: DXVA主路径与指标口径重构（Completed）
- [x] Green: RuntimeMetrics 拆分 `privateBytes/workingSetBytes`
- [x] Green: 指标日志新增 `decode_path/long_run_level/decode_hot_sleep_ms/decode_copy_bytes_per_sec`
- [x] Green: frame_bridge 支持 GPU 纹理帧与 CPU 外部缓冲视图
- [x] Green: MF 解码接入 D3D manager 互操作尝试（DXVA 主路径）
- [x] Green: CPU 回退路径改为单拷贝（去除中间大向量拷贝）
- [x] Verification: 单测通过（90/90） + 构建通过
- **Status:** complete
### Phase 40: CPU-only回退链路降载迭代（Completed）
- [x] Green: CPU回退链路增加额外 decode hot-sleep 放缓（并扩展上限到 64ms）
- [x] Green: 长时负载策略内存阈值下调，提前触发 trim
- [x] Green: 运行态 `TrimMemory` 增加 SourceReader Flush 与桥接帧释放
- [x] Green: CPU回退高水位场景增加 15s 节流 working-set trim
- [x] Verification: 单测通过（90/90） + 构建通过
- **Status:** complete
### Phase 41: 黑幕回归修复与主循环trim安全化（Completed）
- [x] Red: 新增 `runtime_trim_policy` 测试并确认缺失实现导致红灯
- [x] Green: 主循环 `requestDecodeTrim` 增加策略门控，CPU回退动态路径禁止执行破坏性trim
- [x] Green: `decode_pipeline_stub` 运行态 trim 去除 CPU 回退清帧，并限制 flush 仅在 GPU 零拷贝路径
- [x] Green: 测试/构建脚本补齐新源文件，避免“新增测试未被执行”
- [x] Verification: 单测通过（94/94） + 构建通过
- **Status:** complete
### Phase 42: 自适应质量升级为“帧率+分辨率”联合治理（Completed）
- [x] Red: 新增 `decode_output_policy` 测试并验证缺失实现导致编译红灯
- [x] Green: 新增 CPU 回退输出尺寸策略（自适应开启时限像素）
- [x] Green: `IDecodePipeline::Open` 增加 `adaptiveQuality` 参数并贯通 App/Win/stub
- [x] Green: 启用/禁用自适应质量时热重开视频管线，使策略即时生效
- [x] Green: 解码线程按状态切换优先级（active: below-normal / idle: idle）
- [x] Verification: 单测通过（99/99） + 构建通过
- **Status:** complete

### Phase 43: 长时运行系统态仲裁与 CPU 回退进一步压降（Completed）
- [x] Red: 新增/调整 `decode_output_policy`、`long_run_load_policy`、`resource_arbiter`、`loop_sleep_policy`、`metrics_log_line` 测试并验证失败
- [x] Green: CPU 回退输出策略升级为 `720p -> 540p` 双档，并引入 `DecodeOpenProfile`
- [x] Green: 长时负载策略按 `decode_path` 区分 DXVA / CPU 回退 hot-sleep 增量
- [x] Green: `ResourceArbiter` 接入 battery saver / remote session / display off 电源态
- [x] Green: 指标日志新增 `decode_output_pixels/thread_qos/occluded/power_state`
- [x] Green: decode 线程接入 `SetThreadInformation`（EcoQoS + MemoryPriority）并记录实时 QoS
- [x] Green: 壁纸宿主暴露 occlusion 状态，并接入 App 仲裁与指标采样
- [x] Verification: 单测通过（106/106） + 构建通过
- **Status:** complete

### Phase 44: 低帧率源感知降唤醒（Completed）
- [x] Red: 新增 `source_frame_rate_policy` 测试与 `loop_sleep_policy` 低帧率热睡眠测试并验证失败
- [x] Green: 新增 24/25/30/60fps 源帧率识别策略
- [x] Green: decode hot-sleep 改为按“渲染档位 + 源帧率”联合决策
- [x] Green: App 从原先 30/60 双档 heuristics 切换到 `SourceFrameRateState`
- [x] Verification: 单测通过（112/112） + 构建通过
- **Status:** complete

### Phase 45: MF 异步单请求缓存化解码（Completed）
- [x] Red: 新增 `decode_async_read_policy` 测试并验证缺失实现导致编译失败
- [x] Green: 实现异步单请求状态机（单 in-flight / ready sample / EOF seek）
- [x] Green: `decode_pipeline_stub` 切到 `MF_SOURCE_READER_ASYNC_CALLBACK`，`TryAcquireLatestFrame` 改为消费缓存样本
- [x] Green: 运行态 trim 去除 Source Reader flush，避免异步 in-flight 请求与 flush 交错复杂化
- [x] Verification: 单测通过（117/117） + 构建通过
- **Status:** complete

### Phase 46: CPU-only NV12 回退链路打通（Completed）
- [x] Green: `decode_pipeline_stub` 在非 D3D 互操作路径优先协商 `MFVideoFormat_NV12`
- [x] Green: `frame_bridge` / `wallpaper_host_win` 打通 NV12 双平面发布与渲染
- [x] Green: `App` 将 NV12 统一纳入 CPU fallback 降载/trim/热重开判定
- [x] Verification: 单测通过（122/122） + 构建通过
- **Status:** complete

### Phase 47: NV12 顶部绿条修复与对齐布局收敛（Completed）
- [x] Red: 新增 `nv12_layout_policy` 测试并先触发编译失败
- [x] Green: 引入 NV12 对齐布局推导，修复 UV 平面起点不能简单按 `visibleHeight * pitch` 计算的问题
- [x] Green: `decode_pipeline_stub` 仅在单 buffer sample 时走 `Lock2D` 直视图，多 buffer 回退连续缓冲
- [x] Verification: 单测通过（125/125） + 构建通过
- **Status:** complete

### Phase 48: 渲染参数稳定态去唤醒优化（Completed）
- [x] Red: 新增 `LoopSleepPolicy_DecodePumpWakeDependsOnRenderCapChange` 测试并先触发编译失败
- [x] Green: 新增 `ShouldWakeDecodePumpForRenderCapUpdate` 策略函数
- [x] Green: `App::ApplyRenderFpsCap` 改为“仅在帧率档位或 hot-sleep 变化时才唤醒解码泵”
- [x] Verification: 单测通过（126/126） + 构建通过
- **Status:** complete

### Phase 49: 互斥热点与退避节奏迭代优化（Completed）
- [x] Red: 新增 `FrameBridge_TryGetLatestFrameIfNewerSkipsSameSequence` 与 decode pump 退避测试并先触发失败
- [x] Green: `frame_bridge` 增加 `TryGetLatestFrameIfNewer`，仅新帧才进入互斥区复制
- [x] Green: `wallpaper_host_win` 呈现路径改为基于序列号的“新帧读取”
- [x] Green: 解码泵无帧退避升级为“2->4->8->(+4) ... ->24ms”以减少空转唤醒
- [x] Green: 去除 `presentSamplesMs_` 的 `shrink_to_fit` 热路径并预留固定容量，降低内存碎片风险
- [x] Green: NV12 `Lock2D` 快路径移除额外 `new` 持有对象分配
- [x] Verification: 单测通过（127/127） + 构建通过
- **Status:** complete

### Phase 50: 唤醒去重与渲染系统调用收敛（Completed）
- [x] Red: 新增 `LoopSleepPolicy_DecodePumpWakeNotificationUsesDedupe` 并先触发编译失败
- [x] Green: 新增 `ShouldNotifyDecodePumpWake`，`WakeDecodePump` 改为去重通知 + `notify_one`
- [x] Green: `wallpaper_host_win` 引入 viewport 尺寸缓存，移除每帧 `GetClientRect`
- [x] Green: 清理 `decode_pipeline_stub` 未使用字段 `asyncReadyFlags_`
- [x] Verification: 单测通过（128/128） + 构建通过
- **Status:** complete

### Phase 51: 解码 token 消费前置门控（Completed）
- [x] Red: 新增 `DecodeTokenGatePolicy_*` 测试，锁定“仅新序列才尝试消费 token”策略
- [x] Green: 新增 `decode_token_gate_policy` 并接入 `App::Tick`，无新序列时跳过 `decodedTokenMu_` 锁
- [x] Green: 新增 `latestDecodedSequence_` 原子镜像，解码线程发布 token 时同步更新
- [x] Verification: 单测通过（132/132） + 构建通过
- **Status:** complete

### Phase 52: 首次运行峰值压降（Completed）
- [x] Red: 新增 `StartupPolicy_ShouldDeferVideoDecodeStart` 与 `ConfigStore_Exists` 测试并先触发失败
- [x] Green: 新增 `ConfigStore::Exists()`，用于判定首启场景
- [x] Green: 新增 `ShouldDeferVideoDecodeStart`，首次运行且有效视频时初始化阶段仅 Open，不立即 Start 解码
- [x] Green: `StartVideoPipelineForPath` 增加 `startDecodeImmediately` 参数并接入 `Initialize`
- [x] Green: `Run` 中 active-video 判定收敛为 `decodeOpened && decodeRunning`，避免“仅 open 未解码”误判为活跃
- [x] Verification: 单测通过（136/136） + 构建通过
- **Status:** complete

### Phase 53: 桌面常驻帧缓存释放（Completed）
- [x] Red: 新增 `FrameBridge_ReleaseConsumed*` 测试并先触发失败
- [x] Green: `frame_bridge` 新增 `ReleaseLatestFrameIfSequenceConsumed`
- [x] Green: `wallpaper_host_win` 在新帧上传/拷贝成功后按序列释放 bridge 持有帧资源
- [x] Verification: 单测通过（138/138） + 构建通过
- **Status:** complete

### Phase 54: 首启窗口降载 + 单实例保护（Completed）
- [x] Red: 新增 `StartupPolicy_DeferredDecode*`、`DecodeAsyncReadPolicy_DoesNotPrefetchImmediatelyAfterConsume` 测试并先触发失败
- [x] Green: 首启延迟启动增加最小等待窗口（2.5s），避免首次运行进入解码热路径过快
- [x] Green: MF async consume 后改为 lazy read（不立即 prefetch 下一帧），压降动态桌面常驻内存
- [x] Green: 主进程增加单实例互斥（重复启动直接退出）
- [x] Verification: 单测通过（142/142） + 构建通过
- **Status:** complete

### Phase 55: 单实例守卫漏洞修复（Completed）
- [x] Red: 新增 `SingleInstancePolicy_FallbackToLocalMutexOnlyOnNonExistingErrors` 测试并先触发失败
- [x] Green: 新增 `ShouldFallbackToLocalMutex` 策略，禁止在“已有实例”错误码下回退到 `Local` 互斥量
- [x] Green: `main.cpp` 的 `ScopedSingleInstanceMutex` 改为“全局互斥失败仅在可回退错误下尝试本地互斥”
- [x] Green: 移除启动时进程枚举守卫，降低首启 CPU 干扰；补齐 lock file 句柄释放避免句柄泄漏
- [x] Verification: 单测通过（146/146） + 构建通过 + 本机双开验证第二实例退出
- **Status:** complete

### Phase 56: C++23 基线升级 + C++26 门控落地（Completed）
- [x] Red: 新增 `ConfigStore_LoadExpected*`、`MetricsSampler_*`、`Cpp26FeatureSupport_*` 测试并先触发编译失败
- [x] Green: `ConfigStore` 新增 `std::expected` 接口（`LoadExpected/SaveExpected`），`App` 去除 `std::async` 配置读写路径
- [x] Green: `MetricsSampler` 由 `erase(begin)` 改为固定环形缓冲（O(1) push），快照按时间顺序输出
- [x] Green: 新增 `compiler_assume.h` 并在 `wallpaper_host_win` 热路径接入 `WP_ASSUME`
- [x] Green: 标准基线切换到 C++23，构建脚本新增 `-UseCxx2c` 开关（默认 C++23，可选 c++2c）
- [x] Green: 新增 `cpp26_feature_support.h`，统一门控 `consteval/if consteval/deducing this/expected/mdspan/反射/模式匹配`
- [x] Verification: `run_tests` 通过（152/152）+ `build_app` 通过（C++23 与 `-UseCxx2c` 双链路）
- **Status:** complete

### Phase 57: FrameBridge 热路径拷贝瘦身（Completed）
- [x] Red: 调整 `frame_bridge_tests` 断言，移除对 `rgbaPixels` 字段依赖
- [x] Green: `LatestFrame` 删除冗余 `rgbaPixels` 成员，统一使用 `rgbaData + rgbaDataHolder` 持有语义
- [x] Green: `PublishLatestFrame` 改为直接构建别名 holder，避免额外成员赋值与每帧结构拷贝负担
- [x] Verification: 单测通过（152/152） + 构建通过
- **Status:** complete

### Phase 58: 解码帧就绪通知驱动退避（Completed）
- [x] Red: `loop_sleep_policy` 增加“通知可用时更长退避”测试并先触发编译失败
- [x] Green: `IDecodePipeline` 增加 `SetFrameReadyNotifier` / `SupportsFrameReadyNotifier`
- [x] Green: Windows 解码管线在 `OnReadSample` 命中新样本后通知上层，触发解码泵唤醒
- [x] Green: 解码泵退避策略支持 notifier-aware 上限（24ms -> 40ms），减少无帧空转唤醒
- [x] Verification: 单测通过（153/153） + 构建通过
- **Status:** complete

### Phase 59: C++26 标志统一 + 帧桥接无锁化 + 解码发布锁域收敛（Completed）
- [x] Green: `run_tests/build_app` 脚本新增 `-UseCxx26`，优先 `-std=c++26`，不支持时自动回退 `-std=c++2c`
- [x] Green: `frame_bridge` 从全局互斥改为原子 `shared_ptr` 发布/读取，降低热路径锁竞争
- [x] Green: `decode_pipeline_stub` 媒体路径改为“锁内取样本快照 + 锁外发布”，缩短 `mu_` 临界区
- [x] Green: `app` 解码泵在 notifier 可用场景扩大事件等待窗口，降低无帧轮询唤醒
- [x] Green: 新增 `scripts/bench_perf.ps1` 基准脚本（startup/desktop/fullscreen 三场景采样）
- [x] Verification: 单测通过（153/153）+ C++26 链路通过 + 构建通过 + 基准脚本 smoke 通过
- **Status:** complete

### Phase 60: 退避策略与分配器碎片优化（Completed）
- [x] Green: notifier 可用场景下 decode pump 无帧退避上限由 40ms 提升到 64ms，进一步降低无帧唤醒
- [x] Green: 解码泵事件等待窗口最小值从 90ms 提升到 140ms（仅 notifier 路径）
- [x] Green: `frame_bridge` payload 分配切换为 `std::pmr::synchronized_pool_resource`，降低高频 payload 分配碎片
- [x] Verification: C++23/C++26 双链路单测与构建全部通过
- [x] Verification: 基准脚本在单实例约束下可稳定输出 startup/desktop 指标
- **Status:** complete

### Phase 61: 主循环唤醒收敛 + 解码冗余原子清理（Completed）
- [x] Red: 新增 `LoopSleepPolicy_MainLoopMessageAwareWaitOnlyForPauseOrIdle` 测试并先锁定策略
- [x] Green: 主循环新增 `ShouldUseMainLoopMessageAwareWait`，动态桌面路径改为固定睡眠，避免输入消息触发提前唤醒
- [x] Green: Win32 消息等待掩码从 `QS_ALLINPUT` 收敛为 `QS_POSTMESSAGE|QS_SENDMESSAGE|QS_TIMER`（仅 pause/idle 路径）
- [x] Green: `decode_pipeline_stub` 删除失效容量提示路径（`ConsumeFrameBufferCapacityHint` 与相关原子字段）
- [x] Verification: C++23/C++26 双链路单测通过（154/154）+ 双链路构建通过
- [x] Verification: `bench_perf` 同路径对比（startup/desktop）显示 CPU 均值下降
- **Status:** complete

### Phase 62: 并发与资源生命周期高危修复（Completed）
- [x] Red: 新增托盘线程停止策略测试（join 决策）
- [x] Green: 修复托盘线程 StopMessageLoop 提前返回导致 join 漏掉的崩溃路径
- [x] Green: 修复 SourceReader 回调 owner 指针并发读写数据竞争（atomic owner）
- [x] Green: 为 DecodePipeline 增加析构清理（Reset + MFShutdown 兜底）
- [x] Green: 修复 D3D 初始化失败路径资源释放缺失与 ResizeBuffers 失败后 RTV 回退重建
- [x] Green: App 跨线程标志 decodeFrameReadyNotifierAvailable 原子化
- [x] Green: 引入 consteval 顶点表 + span/subspan 行拷贝辅助
- [x] Verification: `run_tests` 全绿（157/157），`build_app` 在 C++23/C++26 构建通过
- **Status:** complete

### Phase 63: 长暂停后恢复过慢修复（Completed）
- [x] Red: 新增恢复重试退避策略测试（warm resume / pipeline resume）
- [x] Green: 将 warm resume 失败重试从固定 500ms 改为快速退避（120/220/360/500ms）
- [x] Green: 将 resume pipeline 重试从固定 1s 改为快速退避（160/260/420/700/1000ms）
- [x] Green: 接入失败计数与成功清零，避免长时间恢复链路秒级卡顿
- [x] Verification: 单测通过（159/159）+ C++23/C++26 构建通过
- **Status:** complete

### Phase 64: 恢复热路径 I/O 探测缓存优化（Completed）
- [x] Red: 新增 `video_path_probe_policy` 测试（缓存命中/过期/路径变化/TTL 选择）
- [x] Green: 新增路径探测缓存策略模块并接入 App
- [x] Green: 恢复重试链路启用短 TTL 缓存，避免高频 `filesystem` I/O 探测
- [x] Green: 非恢复路径保持实时探测，确保换源/首次启动行为不漂移
- [x] Verification: 单测通过（163/163）+ C++23/C++26 构建通过
- **Status:** complete

### Phase 62: 多显示器独立渲染修复（In Progress）
- [ ] Brainstorming: 明确多显示器期望行为与验收标准
- [ ] TDD Red: 新增多显示器视口策略测试并确认失败
- [ ] Green: 实现显示器视口拆分策略并接入 Windows 宿主渲染
- [ ] Verification: 运行测试与构建验证
- **Status:** in_progress

### Phase 62: 多显示器独立渲染修复（Completed）
- [x] Brainstorming: 明确多显示器期望行为与验收标准（默认值：每屏独立完整显示）
- [x] TDD Red: 新增多显示器视口策略测试并确认失败
- [x] Green: 实现显示器视口拆分策略并接入 Windows 宿主渲染
- [x] Verification: 运行测试与构建验证
- **Status:** complete

### Phase 63: 静态恢复时延与资源权衡优化（Completed）
- [x] Red: 新增暂停阈值配置测试（默认配置）并验证失败
- [x] Green: 引入默认暂停阈值配置（Trim=2.5s, AggressiveSuspend=20s, ConservativeSuspend=90s）
- [x] Green: 暂停态主循环睡眠从 110ms 调整到 90ms
- [x] Green: 暂停态前台探测间隔从 260ms 调整到 180ms
- [x] Verification: 全量测试与构建验证
- **Status:** complete

### Phase 64: 壁纸清晰度回归修复（In Progress）
- [x] Discovery: 排查解码输出尺寸、采样器和交换链路径
- [x] Discovery: 确认默认 CPU fallback + adaptive quality 会把高分辨率桌面输出压到 720p
- [x] Design: 确认默认质量策略取舍并锁定修复方案（默认保清晰，仅长期高压降档）
- [x] TDD Red: 新增/调整解码输出策略测试，覆盖“默认保清晰”和“高压再降档”
- [x] Green: 收紧默认降分辨率条件，避免常态桌面被过早降画质
- [x] Verification: 运行测试与构建验证
- **Status:** complete

### Phase 65: 播放速度偏慢排查（In Progress）
- [x] Discovery: 复核 MF 时间戳、渲染调度和解码泵睡眠策略
- [x] Discovery: 确认 CPU fallback 路径的 decode hot-sleep 可能高于 24/25fps 帧间隔
- [x] Design: 锁定“严格 1x 播放优先”方案
- [x] TDD Red: 为低帧率素材和 CPU fallback 热路径补充失败测试
- [x] Green: 修正解码泵节奏，避免帧获取间隔超过源帧间隔
- [x] Verification: 运行测试与构建验证
- **Status:** complete

### Phase 66: 渲染清晰度与色彩链路优化（Completed）
- [x] Design: 锁定“高质量上采样 + 分辨率驱动色彩矩阵”方案
- [x] TDD Red: 新增视频渲染策略测试，覆盖高质量上采样启用条件与 BT.601/BT.709 选择
- [x] Green: 引入 `video_render_policy` 并接入 Windows 渲染宿主
- [x] Green: 为 RGBA/NV12 像素着色器增加高质量上采样常量与色彩空间切换
- [x] Verification: 全量单测、构建、HLSL 试编验证通过
- **Status:** complete

### Phase 67: 静态阶段渲染缓存回收（Completed）
- [x] Design: 锁定“保留最后一帧 + 回收渲染侧视频纹理”方案
- [x] TDD: 新增 `video_surface_cache_policy` 测试，覆盖换源与长静态 pause 的 trim 条件
- [x] Green: 为 `IWallpaperHost` 增加 `TrimMemory()`，Windows 宿主仅释放渲染侧视频纹理
- [x] Green: 在长静态 pause 与视频路径切换时主动触发渲染缓存回收
- [x] Verification: 单测、构建、桌面/前台窗口 pause 基准验证通过
- **Status:** complete

### Phase 68: 真实运行链路复盘（Completed）
- [x] Discovery: 确认用户真实运行入口为 `build/wallpaper_app.exe + build/config.json`
- [x] Discovery: 确认此前“超低占用”采样来自错误实例/错误状态
- [x] Verification: 在真实链路重测并记录 CPU/内存与 `decode_path`
- **Status:** complete

### Phase 69: DXVA 源帧率提示收敛（Completed）
- [x] Red: 为 `source_frame_rate_policy` 增加“NTSC 29.97 归一化 + 立即应用提示”失败测试
- [x] Green: 新增 `NormalizeSourceFrameRateHint/ApplySourceFrameRateHint` 并接入 App 调度
- [x] Green: `decode_pipeline_stub` 从 MF 输出媒体类型读取 `MF_MT_FRAME_RATE`，透传到 `FrameToken`
- [x] Verification: 单测通过（188/188）+ 构建通过 + 真实链路指标 `effective_fps=30`（30fps 素材）
- **Status:** complete

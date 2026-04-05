# 性能与可维护性一揽子优化设计

日期：2026-04-05

## 1. 背景
- 现状：`src/app.cpp` 与 `src/win/decode_pipeline_stub.cpp` 体量过大，逻辑交叉，阻碍理解与验证；主线程在渲染循环中同步写 `metricsLogFile` 与配置；构建脚本与 CMake 旗标分裂且优化力度不足（仅 `-O2`）。
- 目标：在不降级画质/功能的前提下，降低主线程抖动与 I/O 阻塞，统一高性能编译配置，并拆分巨石文件以便后续优化与测试。

## 2. 设计目标
- 编译链：默认开启 `-O3 -march=native -flto -DNDEBUG`，保持可移植降级开关；脚本与 CMake 源表/旗标保持一致。
- I/O：主线程零阻塞，指标与配置写盘通过异步单线程队列批量落盘，容量有界、失败计数可观测。
- 结构：拆分巨石文件，接口保持向后兼容（无需触及头文件公共 API），便于按模块优化与测试。

## 3. 方案概览
- 编译优化：
  - 默认路径：`-O3 -march=native -flto -DNDEBUG`，保留异常；统一于 CMake 与 `scripts/build_app.ps1`。
  - 可移植模式：`-DBUILD_PORTABLE=ON` / `-Portable` 切回 `-O2 -march=x86-64-v3`，禁用 LTO。
  - 链接：保留 `-Wl,--gc-sections`，不引入 PGO（留作后续阶段）。
- 异步写盘：
  - 新增 `AsyncFileWriter`（单线程后台 + 有界锁自由队列），支持 fire-and-forget 任务，写失败计数并可选择降级同步写或丢弃最旧。
  - `MetricsLogFile::Append` 与 `ConfigStore::SaveExpected` 改为提交任务；生命周期由 `App` 拥有，关闭时 flush。
- 拆分文件：
  - `app`：
    - `src/app_core.cpp` 主循环与状态、指标采样。
    - `src/app_decode_control.cpp` 解码泵控制、暂停/恢复、Warm Resume。
    - `src/app_tray.cpp` 托盘与用户交互、配置变更入口。
    - 原头文件保持不变，仅调整内部函数声明为 `static` 或放入匿名命名空间。
  - `decode pipeline stub`：
    - `src/win/decode_pipeline_core.cpp` 管理开放/启动/暂停状态机与通用数据结构。
    - `src/win/decode_pipeline_mf.cpp` Media Foundation + D3D11 互操作、异步回调。
    - `src/win/decode_pipeline_fallback.cpp` fallback ticker 与 CPU 路径。
    - 对外仍用 `CreateDecodePipeline()`，共用头文件无需变动。

## 4. 详细设计
### 4.1 编译与构建
- CMake：
  - `BUILD_PORTABLE` option（默认 OFF）。
  - 设置 `CMAKE_INTERPROCEDURAL_OPTIMIZATION` 受 option 控制；`CMAKE_CXX_FLAGS_RELEASE` 追加 `-O3 -march=native`（或 portable 配置）。
  - 统一源文件列表：测试目标链接 `wallpaper_core`，不再重复列出核心源，减少漂移。
- PowerShell 脚本：
  - 添加 `-Portable` 开关；默认追加 `-O3 -march=native -flto -DNDEBUG`，portable 模式使用 `-O2 -march=x86-64-v3`，关闭 LTO。
  - 复用同一源文件列表（从单一数组生成），避免双写。

### 4.2 异步文件写
- 结构：
  - `AsyncFileWriter`：有界 `SPSC` 队列 + 后台线程；任务类型为 `{path, append(bool), data(string)}`。
  - 超出容量时策略：丢弃最旧并计数（避免主线程阻塞）。
  - 关闭：析构时请求退出、flush 队列并等待线程结束。
- 集成：
  - `MetricsLogFile` 改为将 CSV 记录打包交给 writer；落盘失败计入内部计数，必要时采样日志中写入“drop count”。
  - `ConfigStore::SaveExpected` 改为提交任务；读取仍同步（初始化路径）。
  - 主线程仅在退出时调用 `writer.FlushAndStop()`，避免悬空任务。

### 4.3 拆分与内部依赖
- 使用内部命名空间封装原来文件级静态函数，跨文件共享逻辑通过内部头或匿名 namespace 前置声明，避免扩大公共 API。
- `App` 内部状态保持在类内；拆分文件仅分离实现，接口签名不变。
- `DecodePipelineStub` 内部状态拆分后通过一个私有结构共享，确保线程安全语义与当前一致（持有同一 `std::mutex`）。

## 5. 测试计划
- 现有单测全量运行。
- 新增小型单测：
  - `AsyncFileWriter` 有界队列丢弃策略 & flush 行为。
  - `MetricsLogFile` 在 writer 注入的假件下验证不会阻塞、计数正确。
  - `ConfigStore` 保存走异步路径时保持写入内容正确（可用临时文件夹）。

## 6. 风险与回退
- LTO/native 可能触发特定 CPU 不兼容：用户可用 `-Portable`/`BUILD_PORTABLE=ON` 回退。
- 异步落盘丢失数据风险：通过计数与 flush-on-exit 缓解；必要时可切换同步策略（保留旧接口做可控 fallback）。
- 拆分带来链接/ODR 退化风险：保持单一内部头与 `anonymous namespace`，构建后跑全测。

## 7. 里程碑
1) 同步 CMake + 脚本旗标 & 源清单，验证可移植开关。 
2) 落地 `AsyncFileWriter` 并接入 metrics/config；补充测试。 
3) 拆分 `app.cpp` 与 `decode_pipeline_stub.cpp`，通过测试。 
4) 基线测试（单测 + 手动 smoke），更新 `progress.md`。

# Decode Pipeline 拆分设计 - 2026-04-05

## 目标
- 拆分 MF 解码路径与 fallback ticker，降低单文件复杂度与编译时间。
- 清晰隔离 Windows/MF 依赖，核心逻辑可测试可维护。
- 保持现有接口 `CreateDecodePipeline` 行为。

## 现状
- 单文件 1400+ 行，匿名类包含所有逻辑，难以维护/优化。
- Windows/MF 头与 fallback 混在一起，耦合度高。

## 方案对比
1. 内部头文件声明单一类，按职责将实现分布到 `decode_pipeline_core.cpp`（生命周期+fallback+外观）、`decode_pipeline_mf.cpp`（MF 专属），推荐。优点：最小侵入，保持状态集中；缺点：需要梳理声明/定义。
2. 拆成组合式子组件（例如 `MediaFoundationPath` + `FallbackTicker`），主类委托。优点：更解耦；缺点：改动更大、风险较高。
3. 保留单文件仅分区域标记。优点：工作量最小；缺点：问题未解决。

**推荐方案 1。**

## 架构与文件
- 新增 `src/win/decode_pipeline_internal.h`：声明 `DecodePipelineStub`、成员、跨文件私有方法、`AsyncSourceReaderCallback`。仅供 win 模块内部使用。
- 新增 `src/win/decode_pipeline_core.cpp`：实现构造/析构、`Open/Start/Pause/Stop/TrimMemory`、fallback ticker、通知绑定、`CreateDecodePipeline`。
- 新增 `src/win/decode_pipeline_mf.cpp`（Win32）：实现 MF 启动/释放、DXGI 设备管理、SourceReader 配置、异步读取调度、样本发布、回调类方法。
- 删除旧的 `decode_pipeline_stub.cpp` 或缩减为包含 header（本设计选择替换为上述两个实现，不保留旧文件）。
- CMake：`wallpaper_app` target_sources 改为包含两个新 cpp。

## 错误处理与资源回收
- 保持原有无异常约束；所有 HRESULT 路径继续更新 `interopStage_/interopHresult_`。
- 析构与 `ResetStateLocked` 仍在 core 中统一调用 MF 清理逻辑，确保先停异步再释放资源。

## 测试与验证
- `scripts/run_tests.ps1 -BuildDir build_tmp`
- `scripts/build_app.ps1 -BuildDir build_tmp`
- 手动记录 EXE 体积变化用于回归对比。

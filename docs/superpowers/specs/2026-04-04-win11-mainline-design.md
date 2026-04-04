# Win11-only 新主线设计

日期：2026-04-04

## 1. 背景

当前仓库的运行主体已经是明显的 Windows 专用工程，核心依赖集中在 Win32、Media Foundation、D3D11、DXGI 与 Explorer 桌面附着链路；但默认构建入口仍然是 `MSYS2 UCRT64 + g++ + windres`，`CMakeLists.txt` 不是唯一真相源，脚本与 CMake 双维护源文件清单，存在漂移与误构建风险。

用户已明确以下约束：

- 新工作树将成为未来替代当前主线的新主线
- 不做向下兼容，也不适配其他系统
- 最低支持系统锁定为 `Windows 11 25H2`
- 最低运行版本锁定为 `OS Build 26200.8037`
- 构建体系切换为 `MSVC + CMake + Visual Studio/Windows SDK`
- 迁移方式选择立即硬切，不保留 `MinGW` 作为默认链路

## 2. 目标

- 建立独立的 `Win11-only` 新主线工作树，作为未来默认主线候选
- 将 `CMake` 收敛为唯一构建真相源
- 将默认工具链切换为 `MSVC x64 + Windows SDK`
- 删除仅为 `MinGW/MSYS2` 或旧兼容心智存在的构建与文档包袱
- 保留并继承现有稳定的策略模块与测试资产
- 以 Win11 真实行为契约而非旧平台兼容约束来组织运行时逻辑

## 3. 非目标

- 不为旧版 Windows 保留兼容代码路径
- 不保留 `MinGW`、`MSYS2`、`g++`、`windres` 作为过渡默认入口
- 不引入面向未来未知平台的伪抽象
- 不把本次主线迁移与所有运行时性能实验绑定为一次性大重写
- 不维持 README、脚本、CMake 三套不同的构建真相

## 4. 新主线定位

新主线建议使用独立分支 `win11-mainline` 和独立 worktree，例如 `../wallpaper-win11-mainline`。该工作树的身份不是实验分支，而是未来默认主线候选。

它的硬约束如下：

- 平台：仅 `Windows 11 25H2+`
- 工具链：仅 `MSVC + CMake + Visual Studio/Build Tools + Windows SDK`
- 构建入口：仅 `CMake`，PowerShell 脚本如保留，仅作为薄包装
- 调试与诊断：以 `PDB`、Visual Studio、ETW/WPA 为基线

## 5. 仓库切分与迁移边界

保留内容：

- `include/` 与 `src/` 中的核心策略模块
- Win11 下实际需要的桌面附着、渲染、解码实现
- `tests/` 中已有的纯策略回归资产

重构内容：

- `CMakeLists.txt` 及后续 `CMakePresets.json`
- 应用与测试目标的源文件归属
- 构建、测试、打包与调试入口
- 文档中的默认开发和排障路径

删除内容：

- `MinGW/MSYS2` 默认链路说明
- `g++/windres` 主构建脚本链路
- 仅为编译器差异、资源编译差异、入口差异存在的 workaround

约束：

- `CMake` 必须成为唯一真相源
- PowerShell 不再维护独立源文件列表
- 任何长期保留的文件都必须服务于 `Win11-only` 新主线

## 6. Win11-only 技术收口点

### 6.1 构建与链接

新主线统一使用 `MSVC x64 + CMake Presets`，仅保留 `Debug`、`RelWithDebInfo`、`Release` 三套配置。

建议默认参数：

- 编译：`/O2 /GL /Gw /Gy /Oi /utf-8 /MP`
- 链接：`/LTCG /OPT:REF /OPT:ICF`
- 警告：统一由 `CMake` 控制，不允许脚本各自拼接

### 6.2 进程入口与 DPI

`wWinMain` 作为唯一入口。`PerMonitorV2 DPI` 作为硬前提，不再保留旧 DPI API 回退逻辑。

### 6.3 桌面附着

`WorkerW / Progman / SHELLDLL_DefView` 相关逻辑不能盲删，因为其本质是 Explorer 桌面宿主契约的一部分，在 Win11 仍成立。需要做的是把“兼容多版本 shell”的表达和额外分支，收缩为“Win11 Explorer 下的最小附着顺序”。

### 6.4 图形与解码

`D3D11 + Media Foundation + DXGI` 作为唯一主路径。CPU fallback 允许保留，但其目的仅是保证 Win11 环境下的稳定输出，而不是承担跨系统或跨工具链兼容。

### 6.5 诊断与调试

默认产出完整 `PDB`，以 Visual Studio Profiler、WPA、ETW 为性能诊断基线。开发体验与排障路径围绕 Windows 原生工具链设计。

## 7. 验证矩阵与主线切换准入标准

### 7.1 构建准入

- `Debug`、`RelWithDebInfo`、`Release` 三套配置均可稳定构建
- `CMake` 成为唯一入口
- 不再依赖 `g++/windres`

### 7.2 行为准入

至少覆盖以下 Win11 真实场景：

- 桌面常驻启动
- 多显示器
- Explorer 重启恢复
- 托盘退出
- 前台全屏抑制
- 锁屏与解锁
- 睡眠与唤醒
- 远程会话与本地会话切换

### 7.3 回归准入

- 现有纯策略测试全部保留并持续全绿
- 不允许主线迁移破坏调度、trim、上传、探测、回退等核心策略行为

### 7.4 性能准入

新主线必须重建并满足 Win11 基线，至少包含：

- 冷启动到首帧时间
- 常驻播放 CPU `avg/p95`
- working set `min/max`
- 多显示器 present 稳定性
- Explorer 重启恢复时间
- 主路径与 fallback 路径切换时的抖动

### 7.5 扶正条件

仅当以下条件同时满足时，允许替换当前主线：

- `MSVC + CMake` 已成为唯一构建真相源
- 全量测试通过
- Win11 核心场景验证通过
- 性能指标不劣于当前主线基线
- 文档、构建说明、排障入口均已切换为新链路口径

## 8. 实际迁移步骤

### Phase 1: 建立新工作树

- 创建 `win11-mainline` 分支
- 创建独立 worktree，例如 `../wallpaper-win11-mainline`
- 明确其身份为未来默认主线候选

### Phase 2: 构建主权转移

- 修复并补齐 `CMake` 中的应用/测试目标清单
- 引入 `CMakePresets.json`
- 固定 `MSVC x64 / Windows SDK / Debug / RelWithDebInfo / Release`
- 将 PowerShell 脚本降级为 `cmake --preset ...` 的薄包装，随后可删除

完成标志：

- 不依赖 `g++/windres` 即可完整构建应用和测试

### Phase 3: Win11-only 清理

- 删除 `MinGW/MSYS2` 文档和构建脚本
- 删除仅为工具链差异存在的 workaround
- 收紧入口、DPI、资源、链接参数到 Win11 假设
- 清理语义含混的兼容层表达

### Phase 4: 运行时契约收口

- 验证桌面附着
- 验证 Explorer 重启恢复
- 验证多显示器、托盘、全屏抑制、会话切换

### Phase 5: 性能基线重建

- 在新主线上重新记录 CPU、内存、首帧、恢复时间等指标
- 仅允许用数据判断新主线是否优于或不劣于旧主线

### Phase 6: 主线替换

- 更新默认文档与默认开发入口
- 将旧主线降级为历史分支
- 后续功能开发仅进入 `win11-mainline`

## 9. 风险、取舍与不做事项

### 9.1 接受短期破坏性变更

新主线初期允许旧脚本失效、旧文档失真、旧链路不可构建。这是硬切的自然结果，不视为失败。

### 9.2 不把性能调优和主线迁移绑死

当前仓库仍有运行时 CPU/working set 优化工作在进行。主线迁移期间不应把所有性能实验同时卷入，否则会失去归因能力。应先完成构建与主线收口，再在新主线上继续性能榨取。

### 9.3 不做伪抽象

仅为了“未来也许支持别的平台/工具链”而保留的抽象层，全部视为负资产。

### 9.4 不保留双重真相

新主线只允许一个默认入口、一个默认工具链、一个默认排障路径。

### 9.5 不做无证据优化

切换到 `MSVC` 后，任何“更快了”的结论都必须来自基线对比数据。

## 10. 首个实施子项目建议

第一个实施子项目建议聚焦为：

`建立 Win11-only worktree，并把 MSVC + CMake 收敛为唯一可用构建链路`

原因：

- 这是所有后续 Win11-only 优化的前置条件
- 它能最快消灭当前“脚本/CMake 双维护”的结构债
- 它能为后续行为验证与性能基线重建提供稳定地基

在该子项目完成前，不建议继续向旧 `MinGW` 主链路追加新的长期能力建设

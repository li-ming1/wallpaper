# Win11-only 新主线实施计划

日期：2026-04-04
关联设计文档：`docs/superpowers/specs/2026-04-04-win11-mainline-design.md`

## 1. 目标

把仓库迁移为仅支持 `Windows 11 25H2+`、仅使用 `MSVC + CMake + Visual Studio/Windows SDK` 的新主线，并将其推进为未来默认主线候选。

## 2. 约束

- 不做向下兼容
- 不保留 `MinGW` 作为默认或并行入口
- `CMake` 必须成为唯一构建真相源
- 先完成构建主权转移，再继续 Win11-only 运行时清理与性能重建
- 迁移过程中的结论必须可验证，不允许靠主观判断

## 3. 当前已知环境

- Visual Studio Community 2022 `17.14.19`
- MSVC 工具集 `14.44.35207`
- Visual Studio 自带 `cmake.exe`：
  - `D:\vs2022cs\vs22\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
- Windows SDK 根目录：
  - `D:\Windows Kits\10\`
- 当前 shell 未预热 VS 开发环境，不能直接假设 `cmake`、`cl` 在 `PATH`

## 4. 执行批次

### Batch 1: 建立新主线工作树

任务：

- 创建 `win11-mainline` 分支
- 创建独立 worktree，例如 `../wallpaper-win11-mainline`
- 将该工作树标记为未来主线候选

产出：

- 独立 worktree
- 初始迁移说明

验收：

- 新 worktree 可独立操作，不污染当前工作树

### Batch 2: 建立 Win11-only 开发环境入口

任务：

- 引入 `CMakePresets.json`
- 固定 `MSVC x64`、Visual Studio 生成器、`Debug/RelWithDebInfo/Release`
- 增加 VS 开发者环境启动入口
- 统一 `cmake.exe` 调用路径策略

产出：

- 预设化构建入口
- 面向本机环境的可执行配置

验收：

- 仅用 `cmake --preset ...` 即可完成 configure

### Batch 3: 修复 CMake 目标清单

任务：

- 审核 `wallpaper_app`、`wallpaper_core`、`wallpaper_tests` 源文件归属
- 修复脚本清单与 CMake 清单不一致问题
- 确保 `.rc`、Win32 库链接、测试目标均在 CMake 中完整表达

当前已知重点：

- 应用构建链路依赖 `src/decode_async_read_policy.cpp`
- 现有 `CMakeLists.txt` 并未完全作为唯一真相源使用

产出：

- 可在 MSVC 下独立编译应用与测试的 CMake 目标

验收：

- `Debug` 下应用构建通过
- `Debug` 下测试构建通过

### Batch 4: 建立仅基于 CMake 的薄包装脚本

任务：

- 将现有 PowerShell 脚本改造为仅调用 `cmake --preset ...`
- 删除脚本中的独立源文件列表
- 分离 `configure/build/test` 命令职责

产出：

- 简化后的构建脚本
- 无双维护清单

验收：

- 任一脚本失效不会影响 CMake 自身的构建真相
- 脚本只承担调用入口职责

### Batch 5: 移除 MinGW 默认入口

任务：

- 删除 `README` 中的 `MSYS2/g++/windres` 默认描述
- 删除或冻结旧 `MinGW` 构建脚本
- 修改文档、排障、开发入口为 `MSVC + CMake`

产出：

- 文档口径统一
- 新主线不再表达多套默认工具链

验收：

- 新工作树中默认构建文档只描述 `MSVC + CMake`

### Batch 6: Win11-only 代码收口

任务：

- 收紧入口为 `wWinMain`
- 去除非必要 DPI 回退
- 删除仅为旧工具链差异存在的 workaround
- 保留对 Win11 真实运行稳定性仍有价值的 fallback

重点原则：

- 不盲删 `WorkerW / Progman / SHELLDLL_DefView` 这类 Win11 仍真实需要的桌面契约逻辑
- 只删“兼容旧链路”的代码，不删“保障 Win11 稳定”的代码

产出：

- 更窄、更清晰的 Win11-only 运行时入口

验收：

- 新代码表达与设计约束一致
- 不再出现明显的旧工具链兼容心智残留

### Batch 7: 行为与回归验证

任务：

- 全量跑通策略测试
- 验证桌面启动、多显示器、Explorer 重启恢复、托盘退出、全屏抑制、锁屏/解锁、睡眠/唤醒、会话切换

产出：

- 行为验证记录
- 回归测试结果

验收：

- 测试全绿
- Win11 核心场景通过

### Batch 8: 性能基线重建与扶正判定

任务：

- 记录首帧时间、CPU `avg/p95`、working set `min/max`
- 验证多显示器 present 稳定性
- 验证 Explorer 重启恢复耗时
- 对比当前主线基线

产出：

- 新主线性能基线
- 主线替换判断结论

验收：

- 性能不劣于当前主线
- 达到设计文档中的扶正条件

## 5. 实施顺序原则

- 优先级 1：构建主权转移
- 优先级 2：文档与入口统一
- 优先级 3：Win11-only 代码清理
- 优先级 4：行为验证
- 优先级 5：性能重建

在 `Batch 3` 完成前，不做大规模运行时重构；在 `Batch 7` 完成前，不宣称新主线可替代当前主线；在 `Batch 8` 完成前，不宣称新主线更优。

## 6. TDD 要求

任何会改变行为的代码调整，必须遵循：

1. 先补失败测试
2. 验证失败原因正确
3. 写最小实现通过
4. 再做重构

说明：

- 构建系统、文档和工作树建立本身不一定需要单测
- 但所有影响现有策略、入口、运行时行为的修改，都必须先写失败测试

## 7. 首个执行批次建议

建议立即执行的首个批次组合：

- `Batch 1`
- `Batch 2`
- `Batch 3`

原因：

- 这三步能最快消灭“构建入口分裂”
- 这是后续 Win11-only 清理与验证的基础
- 不先完成这三步，后面的运行时与性能讨论都缺乏稳定地基

# wallpaper

Windows 动态壁纸实验项目。

当前仓库主要包含三部分：

- `wallpaper_app.exe`：托盘常驻的动态壁纸程序
- `wallpaper_tests.exe`：策略与调度逻辑的单元测试
- 一组偏纯函数化的策略模块：用于控制解码、渲染、暂停、采样与资源治理

## 环境

推荐工具链：

- Windows
- MSYS2 UCRT64
- `g++`
- `windres`
- PowerShell

项目内置的日常构建脚本默认走 `MinGW g++`，不是 `MSVC`。

## 构建

构建应用：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_app.ps1 -BuildDir build
```

执行测试：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_tests.ps1 -BuildDir build
```

默认产物位于 `build/`：

- `wallpaper_app.exe`
- `wallpaper_tests.exe`
- `app_icon_res.o`

## CMake

仓库包含 `CMakeLists.txt`，但当前日常开发链路以 `scripts/build_app.ps1` 和 `scripts/run_tests.ps1` 为准。

如果你要切到其他生成器或工具链，先确认依赖和编码设置一致。

## 目录

- `include/`：头文件
- `src/`：应用与策略实现
- `tests/`：单元测试
- `resources/`：Windows 资源文件
- `scripts/`：构建与测试脚本
- `assets/`：项目资源


# wallpaper

Windows 动态壁纸实验项目（托盘常驻 + 解码/渲染策略模块 + 单元测试）。

## 配置文件（重点）

### 位置

程序启动时会读取 `config.json`，路径规则：

- 优先使用可执行文件同目录：`<wallpaper_app.exe目录>/config.json`
- 仅在无法解析 exe 路径时回退为当前工作目录下的 `config.json`

### 配置结构

当前支持字段如下（不保证向后兼容，旧字段会被忽略）：

```json
{
  "videoPath": "D:/videos/demo.mp4",
  "playbackProfile": "balanced",
  "autoStart": false,
  "debugMetrics": false,
  "pauseWhenNotDesktopContext": true
}
```

字段说明：

- `videoPath`：视频文件路径；空字符串表示未选择视频。
- `playbackProfile`：播放策略档位；缺失时默认 `balanced`。
  - `balanced`：默认；从 decode open 起偏向较低驻留的路径，CPU 可能高于 `low_cpu`，但更容易压住内存。
  - `low_cpu`：持续强制硬件 transform 偏好，并跳过部分内存回收与 runtime 调优；CPU 更低，但内存占用通常更高。
- `autoStart`：是否开机自启。
- `debugMetrics`：是否启用完整 metrics 调试模式；默认 `false`。
  - `false`：不写 `metrics.csv`，并关闭非必要字段采集；运行控制所需的最小指标仍会保留。
  - `true`：保持当前完整 metrics 行为，采集扩展字段并写入 `metrics.csv`。
- `pauseWhenNotDesktopContext`：前台不是桌面上下文时是否暂停。

### 配置示例

更偏平衡 / 控内存：

```json
{
  "videoPath": "D:/videos/demo.mp4",
  "playbackProfile": "balanced",
  "autoStart": false,
  "debugMetrics": false,
  "pauseWhenNotDesktopContext": true
}
```

更偏极低 CPU：

```json
{
  "videoPath": "D:/videos/demo.mp4",
  "playbackProfile": "low_cpu",
  "autoStart": false,
  "debugMetrics": false,
  "pauseWhenNotDesktopContext": true
}
```

手动编辑 `config.json` 后，需要重启应用；当前不会热加载该文件。

### 兼容策略

- 当前只保证识别 `videoPath`、`playbackProfile`、`autoStart`、`debugMetrics`、`pauseWhenNotDesktopContext` 五个字段。
- 旧字段或未知字段会被忽略，不会自动迁移或重写。
- 配置写入优先异步；当异步通道不可用时会自动回退同步写盘，避免配置丢失。

## 编译与测试（重点）

### 推荐环境

- Windows
- PowerShell
- MSYS2 UCRT64（建议）
- `g++`
- `windres`

示例（MSYS2）：

```powershell
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-binutils
```

确保 `g++`、`windres` 在 `PATH` 中可见。

### 构建应用

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_app.ps1 -BuildDir build
```

常用参数：

- `-BuildDir <dir>`：指定输出目录（默认 `build`）。
- `-Portable`：使用便携优化参数（`-O2 -march=x86-64-v3`）。
- `-UseCxx26` 或 `-UseCxx2c`：尝试 C++26/C++2c 编译（脚本会自动探测支持）。

### 运行测试

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_tests.ps1 -BuildDir build
```

测试脚本支持与构建脚本相同的 `-Portable`、`-UseCxx26`、`-UseCxx2c` 参数。

### 产物

默认位于 `build/`：

- `wallpaper_app.exe`
- `wallpaper_tests.exe`
- `app_icon_res.o`

## CMake 说明

仓库提供 `CMakeLists.txt`，但日常开发链路以 `scripts/build_app.ps1` 与 `scripts/run_tests.ps1` 为准（脚本参数和链接选项最完整）。

## 常见问题

- `g++ not found in PATH`：未安装或未加入环境变量。
- `windres not found in PATH`：未安装 MinGW binutils 或未加入环境变量。
- 切换工具链（如 MSVC）时，请先确认编译选项、字符集与链接库保持一致。

## 目录结构

- `include/`：头文件
- `src/`：应用与策略实现
- `tests/`：单元测试
- `resources/`：Windows 资源文件
- `scripts/`：构建与测试脚本
- `assets/`：项目资源

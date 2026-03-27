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

## Technical Decisions
| Decision | Rationale |
|----------|-----------|
| Single process architecture | Avoid IPC overhead and simplify resource control |
| WorkerW host abstraction | Native wallpaper integration model on Windows |
| MF decode abstraction (stub for now) | Keep interface stable while iterating implementation |
| Bounded and deterministic scheduling | Prevent memory/latency drift |
| Config I/O with async wrappers | Align with async-first I/O requirement |
| Hard fallback on pipeline failures | Protect foreground performance target |

## Issues Encountered
| Issue | Resolution |
|-------|------------|
| Empty repo means no baseline tests | Created new lightweight native test harness |
| Missing CMake in shell PATH | Added scripts/run_tests.ps1 with g++ path detection |
| MinGW link errors (`FOLDERID_LocalAppData`, `WinMain`) | Reworked entry to dual `main`/`wWinMain`, switched config path lookup to `SHGetFolderPathW` |

## Resources
- task_plan.md
- progress.md
- scripts/run_tests.ps1

## Visual/Browser Findings
- None

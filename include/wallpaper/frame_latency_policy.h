#pragma once

namespace wallpaper {

// waitable-object 超时时是否跳过本帧提交：
// - 未启用 waitable 或门控未就绪时，永不跳过；
// - 达到最大连续超时次数后强制提交一帧，避免“永远不 present”。
[[nodiscard]] bool ShouldSkipPresentOnWaitTimeout(bool waitableEnabled, bool gateArmed,
                                                  int timeoutSkips,
                                                  int maxTimeoutSkipsBeforeForce) noexcept;

}  // namespace wallpaper

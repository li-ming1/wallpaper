cmake_minimum_required(VERSION 3.24)
project(wallpaper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
  add_compile_options(/W4 /permissive- /Zc:__cplusplus)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(wallpaper_core STATIC
  src/config_store.cpp
  src/desktop_context_policy.cpp
  src/desktop_attach_policy.cpp
  src/decode_output_policy.cpp
  src/foreground_policy.cpp
  src/loop_sleep_policy.cpp
  src/metrics_log_line.cpp
  src/metrics_log_file.cpp
  src/pause_suspend_policy.cpp
  src/pause_transition_policy.cpp
  src/pause_resource_policy.cpp
  src/quality_governor.cpp
  src/video_path_matcher.cpp
  src/render_scheduler.cpp
  src/resource_arbiter.cpp
  src/source_frame_rate_policy.cpp
  src/metrics_sampler.cpp
  src/frame_bridge.cpp
  src/frame_buffer_policy.cpp
  src/frame_latency_policy.cpp
  src/long_run_load_policy.cpp
  src/probe_cadence_policy.cpp
  src/nv12_layout_policy.cpp
  src/runtime_trim_policy.cpp
  src/startup_policy.cpp
)
target_include_directories(wallpaper_core PUBLIC include)

add_executable(wallpaper_app WIN32
  src/main.cpp
  src/app.cpp
  src/platform_stubs.cpp
)
target_include_directories(wallpaper_app PRIVATE include)
target_link_libraries(wallpaper_app PRIVATE wallpaper_core)

if (WIN32)
  target_sources(wallpaper_app PRIVATE
    resources/app_icon.rc
    src/win/wallpaper_host_win.cpp
    src/win/decode_pipeline_stub.cpp
    src/win/tray_controller_win.cpp
  )
  target_compile_definitions(wallpaper_app PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(wallpaper_app PRIVATE
    user32
    shell32
    advapi32
    comdlg32
    psapi
    winmm
    d3d11
    dxgi
    dwmapi
    d3dcompiler
  )
  target_link_libraries(wallpaper_app PRIVATE mfplat mfreadwrite mfuuid ole32)
endif()

enable_testing()

add_executable(wallpaper_tests
  tests/test_main.cpp
  tests/config_store_tests.cpp
  tests/cpp26_feature_support_tests.cpp
  tests/decode_async_read_policy_tests.cpp
  tests/desktop_context_policy_tests.cpp
  tests/desktop_attach_policy_tests.cpp
  tests/decode_output_policy_tests.cpp
  tests/frame_buffer_policy_tests.cpp
  tests/frame_latency_policy_tests.cpp
  tests/frame_bridge_tests.cpp
  tests/foreground_policy_tests.cpp
  tests/loop_sleep_policy_tests.cpp
  tests/long_run_load_policy_tests.cpp
  tests/metrics_sampler_tests.cpp
  tests/metrics_log_line_tests.cpp
  tests/metrics_log_file_tests.cpp
  tests/nv12_layout_policy_tests.cpp
  tests/pause_resource_policy_tests.cpp
  tests/pause_suspend_policy_tests.cpp
  tests/pause_transition_policy_tests.cpp
  tests/quality_governor_tests.cpp
  tests/probe_cadence_policy_tests.cpp
  tests/render_scheduler_tests.cpp
  tests/resource_arbiter_tests.cpp
  tests/source_frame_rate_policy_tests.cpp
  tests/runtime_trim_policy_tests.cpp
  tests/single_instance_policy_tests.cpp
  tests/startup_policy_tests.cpp
  tests/tray_thread_stop_policy_tests.cpp
  tests/video_path_matcher_tests.cpp
  src/config_store.cpp
  src/decode_async_read_policy.cpp
  src/desktop_context_policy.cpp
  src/desktop_attach_policy.cpp
  src/decode_output_policy.cpp
  src/frame_bridge.cpp
  src/frame_buffer_policy.cpp
  src/frame_latency_policy.cpp
  src/foreground_policy.cpp
  src/loop_sleep_policy.cpp
  src/long_run_load_policy.cpp
  src/metrics_sampler.cpp
  src/metrics_log_line.cpp
  src/metrics_log_file.cpp
  src/nv12_layout_policy.cpp
  src/pause_suspend_policy.cpp
  src/pause_transition_policy.cpp
  src/pause_resource_policy.cpp
  src/quality_governor.cpp
  src/probe_cadence_policy.cpp
  src/render_scheduler.cpp
  src/resource_arbiter.cpp
  src/source_frame_rate_policy.cpp
  src/runtime_trim_policy.cpp
  src/startup_policy.cpp
  src/video_path_matcher.cpp
)

target_include_directories(wallpaper_tests PRIVATE include)

add_test(NAME wallpaper_tests COMMAND wallpaper_tests)


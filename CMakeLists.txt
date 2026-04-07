cmake_minimum_required(VERSION 3.24)
project(wallpaper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_PORTABLE "Build with portable flags (disable LTO/native)" OFF)

if (MSVC)
  add_compile_options(/W4 /permissive- /Zc:__cplusplus)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if (NOT MSVC)
  if (BUILD_PORTABLE)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -march=x86-64-v3 -DNDEBUG")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
  else()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -DNDEBUG")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  endif()
endif()

set(WALLPAPER_CORE_SOURCES
  src/config_store.cpp
  src/cpu_frame_downscale.cpp
  src/desktop_context_policy.cpp
  src/desktop_attach_policy.cpp
  src/decode_output_policy.cpp
  src/decode_async_read_policy.cpp
  src/foreground_policy.cpp
  src/loop_sleep_policy.cpp
  src/metrics_log_line.cpp
  src/metrics_log_file.cpp
  src/monitor_layout_policy.cpp
  src/pause_suspend_policy.cpp
  src/pause_transition_policy.cpp
  src/pause_resource_policy.cpp
  src/quality_governor.cpp
  src/video_path_matcher.cpp
  src/render_scheduler.cpp
  src/resource_arbiter.cpp
  src/source_frame_rate_policy.cpp
  src/swap_chain_policy.cpp
  src/metrics_sampler.cpp
  src/frame_bridge.cpp
  src/frame_buffer_policy.cpp
  src/long_run_load_policy.cpp
  src/probe_cadence_policy.cpp
  src/nv12_layout_policy.cpp
  src/runtime_trim_policy.cpp
  src/startup_policy.cpp
  src/app_autostart.cpp
  src/video_path_probe_policy.cpp
  src/async_file_writer.cpp
  src/upload_copy_policy.cpp
  src/upload_scale_policy.cpp
  src/upload_texture_policy.cpp
)

add_library(wallpaper_core STATIC ${WALLPAPER_CORE_SOURCES})
target_include_directories(wallpaper_core PUBLIC include)

add_executable(wallpaper_app WIN32
  src/main.cpp
  src/app.cpp
  src/app_metrics.cpp
  src/app_decode_control.cpp
  src/app_tray.cpp
)
target_include_directories(wallpaper_app PRIVATE include)
target_link_libraries(wallpaper_app PRIVATE wallpaper_core)

if (WIN32)
  target_sources(wallpaper_app PRIVATE
    resources/app_icon.rc
    src/win/wallpaper_host_win.cpp
    src/win/decode_pipeline_core.cpp
    src/win/decode_pipeline_mf.cpp
    src/win/tray_controller_win.cpp
  )
  if (NOT MSVC)
    target_compile_options(wallpaper_app PRIVATE
      -fno-exceptions
      -fno-rtti
      -fno-asynchronous-unwind-tables
      -fno-unwind-tables
      -fomit-frame-pointer)
  endif()
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
  tests/cpu_frame_downscale_tests.cpp
  tests/cpp26_feature_support_tests.cpp
  tests/decode_async_read_policy_tests.cpp
  tests/desktop_context_policy_tests.cpp
  tests/desktop_attach_plan_tests.cpp
  tests/desktop_attach_policy_tests.cpp
  tests/decode_output_policy_tests.cpp
  tests/async_file_writer_tests.cpp
  tests/fixed_task_queue_tests.cpp
  tests/frame_buffer_policy_tests.cpp
  tests/frame_bridge_tests.cpp
  tests/foreground_policy_tests.cpp
  tests/loop_sleep_policy_tests.cpp
  tests/long_run_load_policy_tests.cpp
  tests/metrics_sampler_tests.cpp
  tests/metrics_log_line_tests.cpp
  tests/metrics_shard_retain_set_tests.cpp
  tests/metrics_log_file_tests.cpp
  tests/monitor_layout_policy_tests.cpp
  tests/nv12_layout_policy_tests.cpp
  tests/pause_resource_policy_tests.cpp
  tests/pause_suspend_policy_tests.cpp
  tests/pause_transition_policy_tests.cpp
  tests/playback_profile_policy_tests.cpp
  tests/present_sample_window_tests.cpp
  tests/quality_governor_tests.cpp
  tests/probe_cadence_policy_tests.cpp
  tests/render_scheduler_tests.cpp
  tests/resource_arbiter_tests.cpp
  tests/sample_publish_policy_tests.cpp
  tests/shared_device_cache_policy_tests.cpp
  tests/source_frame_rate_policy_tests.cpp
  tests/swap_chain_policy_tests.cpp
  tests/runtime_trim_policy_tests.cpp
  tests/single_instance_policy_tests.cpp
  tests/startup_policy_tests.cpp
  tests/tray_action_queue_tests.cpp
  tests/tray_thread_stop_policy_tests.cpp
  tests/upload_copy_policy_tests.cpp
  tests/upload_scale_policy_tests.cpp
  tests/upload_texture_policy_tests.cpp
  tests/video_path_probe_policy_tests.cpp
  tests/video_path_matcher_tests.cpp
  tests/video_frame_route_policy_tests.cpp
  tests/win11_cleanup_tests.cpp
)

target_include_directories(wallpaper_tests PRIVATE include)
target_link_libraries(wallpaper_tests PRIVATE wallpaper_core)

add_test(NAME wallpaper_tests COMMAND wallpaper_tests)


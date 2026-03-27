cmake_minimum_required(VERSION 3.24)
project(wallpaper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
  add_compile_options(/W4 /permissive- /Zc:__cplusplus)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(wallpaper_core STATIC
  src/config_store.cpp
  src/render_scheduler.cpp
  src/resource_arbiter.cpp
  src/metrics_sampler.cpp
)
target_include_directories(wallpaper_core PUBLIC include)

add_executable(wallpaper_app
  src/main.cpp
  src/app.cpp
  src/platform_stubs.cpp
)
target_include_directories(wallpaper_app PRIVATE include)
target_link_libraries(wallpaper_app PRIVATE wallpaper_core)

if (WIN32)
  target_sources(wallpaper_app PRIVATE
    src/win/wallpaper_host_win.cpp
    src/win/decode_pipeline_stub.cpp
    src/win/tray_controller_win.cpp
  )
  target_compile_definitions(wallpaper_app PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(wallpaper_app PRIVATE user32 shell32 advapi32 d3d11 dxgi dwmapi)
  target_link_libraries(wallpaper_app PRIVATE mfplat mfreadwrite mfuuid ole32)
endif()

enable_testing()

add_executable(wallpaper_tests
  tests/test_main.cpp
  tests/config_store_tests.cpp
  tests/render_scheduler_tests.cpp
  tests/resource_arbiter_tests.cpp
  src/config_store.cpp
  src/render_scheduler.cpp
  src/resource_arbiter.cpp
)

target_include_directories(wallpaper_tests PRIVATE include)

add_test(NAME wallpaper_tests COMMAND wallpaper_tests)

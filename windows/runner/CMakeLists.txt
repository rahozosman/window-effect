cmake_minimum_required(VERSION 3.14)
project(runner LANGUAGES CXX)

# Define the application target. To change its name, change BINARY_NAME in the
# top-level CMakeLists.txt, not the value here, or `flutter run` will no longer
# work.
#
# Any new source files that you add to the application should be added here.
add_executable(${BINARY_NAME} WIN32
  "flutter_window.cpp"
  "main.cpp"
  "utils.cpp"
  "win32_window.cpp"
  "effects/active_window_effect.cpp"
  "effects/animation.cpp"
  "effects/backdrop_effect.cpp"
  "effects/compatibility.cpp"
  "effects/config.cpp"
  "effects/corner_effect.cpp"
  "effects/dispatcher.cpp"
  "effects/dwm_api.cpp"
  "effects/effects_bridge.cpp"
  "effects/engine.cpp"
  "effects/exclusion_manager.cpp"
  "effects/exe_picker.cpp"
  "effects/log.cpp"
  "effects/managed_window.cpp"
  "effects/proxy_window.cpp"
  "effects/os_version.cpp"
  "effects/self_backdrop.cpp"
  "effects/shadow_effect.cpp"
  "effects/single_instance.cpp"
  "effects/startup.cpp"
  "effects/transparency_effect.cpp"
  "effects/tray.cpp"
  "effects/window_classifier.cpp"
  "effects/window_detection.cpp"
  "${FLUTTER_MANAGED_DIR}/generated_plugin_registrant.cc"
  "Runner.rc"
  "runner.exe.manifest"
)

# Apply the standard set of build settings. This can be removed for applications
# that need different build settings.
apply_standard_settings(${BINARY_NAME})

# Add preprocessor definitions for the build version.
target_compile_definitions(${BINARY_NAME} PRIVATE "FLUTTER_VERSION=\"${FLUTTER_VERSION}\"")
target_compile_definitions(${BINARY_NAME} PRIVATE "FLUTTER_VERSION_MAJOR=${FLUTTER_VERSION_MAJOR}")
target_compile_definitions(${BINARY_NAME} PRIVATE "FLUTTER_VERSION_MINOR=${FLUTTER_VERSION_MINOR}")
target_compile_definitions(${BINARY_NAME} PRIVATE "FLUTTER_VERSION_PATCH=${FLUTTER_VERSION_PATCH}")
target_compile_definitions(${BINARY_NAME} PRIVATE "FLUTTER_VERSION_BUILD=${FLUTTER_VERSION_BUILD}")

# Disable Windows macros that collide with C++ standard library functions.
target_compile_definitions(${BINARY_NAME} PRIVATE "NOMINMAX")

# Add dependency libraries and include directories. Add any application-specific
# dependencies here.
target_link_libraries(${BINARY_NAME} PRIVATE flutter flutter_wrapper_app)
target_link_libraries(${BINARY_NAME} PRIVATE "dwmapi.lib")
# Named rather than left to the MSVC defaults: the tray icon, the executable
# picker and the Run-key entry each depend on one of these.
target_link_libraries(${BINARY_NAME} PRIVATE "shell32.lib" "ole32.lib" "advapi32.lib")
# timeBeginPeriod, so the animation frame clock gets a 1 ms tick.
target_link_libraries(${BINARY_NAME} PRIVATE "winmm.lib")
target_include_directories(${BINARY_NAME} PRIVATE "${CMAKE_SOURCE_DIR}")

# Run the Flutter tool portions of the build. This must not be removed.
add_dependencies(${BINARY_NAME} flutter_assemble)

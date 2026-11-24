cmake_minimum_required(VERSION 3.24)
project(Conflict VERSION 0.7.0 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "Conflict 0.7.0 currently targets Windows 10/11.")
endif()

find_program(CARGO_EXECUTABLE cargo REQUIRED)
add_executable(Conflict WIN32 src/main.cpp)
target_compile_features(Conflict PRIVATE cxx_std_17)
target_compile_definitions(Conflict PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
if(MSVC)
  target_compile_options(Conflict PRIVATE /W4 /permissive- /utf-8)
endif()
target_link_libraries(Conflict PRIVATE d3d11 dxgi d3dcompiler user32 gdi32)
set_target_properties(Conflict PROPERTIES OUTPUT_NAME "Conflict")

add_custom_command(TARGET Conflict POST_BUILD
  COMMAND "${CARGO_EXECUTABLE}" build --manifest-path "${CMAKE_SOURCE_DIR}/rust/Cargo.toml" --release
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_SOURCE_DIR}/rust/target/release/conflict_core.dll"
    "$<TARGET_FILE_DIR:Conflict>/conflict_core.dll"
  COMMENT "Building the Rust real-time simulation and packaging conflict_core.dll"
  VERBATIM
)
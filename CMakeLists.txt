cmake_minimum_required(VERSION 3.20)
project(Conflict VERSION 0.3.0 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "Conflict currently targets Windows.")
endif()

add_executable(Conflict WIN32 src/main.cpp)
target_compile_features(Conflict PRIVATE cxx_std_17)
target_compile_definitions(Conflict PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(Conflict PRIVATE user32 gdi32)
if(MSVC)
  target_compile_options(Conflict PRIVATE /W4 /permissive- /utf-8)
endif()
set_target_properties(Conflict PROPERTIES OUTPUT_NAME "Conflict")

# Rust is a separate native DLL loaded by the C++ game at runtime.
# The C++ fallback keeps the prototype playable when Cargo is not installed.
find_program(CARGO_EXECUTABLE cargo)
if(CARGO_EXECUTABLE)
  add_custom_command(TARGET Conflict POST_BUILD
    COMMAND "${CARGO_EXECUTABLE}" build --manifest-path "${CMAKE_SOURCE_DIR}/rust/Cargo.toml" --release
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${CMAKE_SOURCE_DIR}/rust/target/release/conflict_core.dll"
      "$<TARGET_FILE_DIR:Conflict>/conflict_core.dll"
    COMMENT "Building and packaging the Rust combat core"
    VERBATIM
  )
else()
  message(WARNING "Cargo was not found. Conflict will use its C++ combat fallback.")
endif()

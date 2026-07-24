cmake_minimum_required(VERSION 3.26)

project(underhanders)

set(PACKAGESOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/.packages/cmake_prefix_path")
if(EXISTS ${PACKAGESOURCE})
  message(STATUS "File ${PACKAGESOURCE} exists")
  file(STRINGS ${PACKAGESOURCE} ADDITIONAL_PREFIX_PATH)
  foreach(PREFIX_PATH_ENTRY ${ADDITIONAL_PREFIX_PATH})
    message(STATUS "Adding ${PREFIX_PATH_ENTRY} to CMAKE_PREFIX_PATH")
    list(APPEND CMAKE_PREFIX_PATH "${PREFIX_PATH_ENTRY}")
    if(EMSCRIPTEN)
      message(STATUS "Adding ${PREFIX_PATH_ENTRY} to CMAKE_FIND_ROOT_PATH")
      list(APPEND CMAKE_FIND_ROOT_PATH "${PREFIX_PATH_ENTRY}")
    endif()
  endforeach()
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug" AND NOT EMSCRIPTEN)
  set(NO_HOTRELOAD_DEFAULT OFF)
else()
  set(NO_HOTRELOAD_DEFAULT ON)
endif()

message(
  "Build type is ${CMAKE_BUILD_TYPE}, *not* using hotreload: ${NO_HOTRELOAD_DEFAULT}..."
)

option(
  GAME_NO_HOTRELOAD
  "Disable building code into a separate library that gets reloaded on code change"
  ${NO_HOTRELOAD_DEFAULT})

if(NOT GAME_NO_HOTRELOAD AND EMSCRIPTEN)
  message(
    FATAL_ERROR
      "Cannot specify GAME_NO_HOTRELOAD=OFF and build for emscripten target, shared libraries don't work under emscripten."
  )
endif()

if(NOT CMAKE_SYSTEM_NAME MATCHES "Emscripten")
  find_program(MOLD_PROGRAM mold)
  if(MOLD_PROGRAM)
    set(CMAKE_LINKER_TYPE MOLD)
    message(STATUS "mold found: ${MOLD_PROGRAM}")
  endif()
endif()

# common source files cannot be hot reloaded but are shared by main.cpp and the
# reloaded game. useful for persistent symbols such as virtual functions or
# anything we need function pointers to
set(common_source_files src/logging.cpp src/arena.cpp src/allocator.cpp)
set(hotreloaded_source_files
    src/entry.cpp src/assets.cpp src/player.cpp
    vendor/raylib_box2d_debugdraw/raylib_box2d_debugdraw.c src/bullet.cpp)
set(main_source_files src/main.cpp)
set(include_directories src/ vendor/raylib_box2d_debugdraw)

add_executable(pool_test src/pool_test.cpp src/allocator.cpp src/arena.cpp
                         src/logging.cpp)
target_include_directories(pool_test PRIVATE src/)

# the game may run `cmake --build HOTRELOAD_BUILD_DIR --target
# HOTRELOAD_LIB_TARGET` in debug mode, and re-link to it
if(NOT GAME_NO_HOTRELOAD)
  add_library(${PROJECT_NAME}_lib SHARED ${hotreloaded_source_files})
  add_library(common SHARED ${common_source_files})
  add_executable(${PROJECT_NAME} ${main_source_files})
  add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_lib)
  target_compile_definitions(
    ${PROJECT_NAME}
    PRIVATE HOTRELOAD_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/src"
            HOTRELOAD_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}"
            HOTRELOAD_LIB_TARGET="${PROJECT_NAME}_lib"
            HOTRELOAD_LIB_PATH=\"$<TARGET_FILE:${PROJECT_NAME}_lib>\")
else()
  add_executable(
    ${PROJECT_NAME} ${main_source_files} ${hotreloaded_source_files}
                    ${common_source_files})
endif()

set(DEBUG_COMPILE_OPTIONS -fsanitize=address -O0 -ggdb -fPIC)
set(DEBUG_LINK_OPTIONS -fsanitize=address)

if(CMAKE_BUILD_TYPE MATCHES "Debug" AND NOT MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE ${DEBUG_COMPILE_OPTIONS})
  target_link_options(${PROJECT_NAME} PRIVATE ${DEBUG_LINK_OPTIONS})

  if(NOT GAME_NO_HOTRELOAD)
    target_compile_options(${PROJECT_NAME}_lib PRIVATE ${DEBUG_COMPILE_OPTIONS})
    target_link_options(${PROJECT_NAME}_lib PRIVATE ${DEBUG_LINK_OPTIONS})
    target_compile_options(common PRIVATE ${DEBUG_COMPILE_OPTIONS})
    target_link_options(common PRIVATE ${DEBUG_LINK_OPTIONS})
  endif()
endif()

if(MSVC
   AND WIN32
   AND NOT MSVC_VERSION VERSION_LESS 142)
  target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:/INCREMENTAL>)
  target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:/ZI>)

  if(NOT GAME_NO_HOTRELOAD)
    target_link_options(${PROJECT_NAME}_lib PRIVATE
                        $<$<CONFIG:Debug>:/INCREMENTAL>)
    target_compile_options(${PROJECT_NAME}_lib PRIVATE $<$<CONFIG:Debug>:/ZI>)
    target_link_options(common PRIVATE $<$<CONFIG:Debug>:/INCREMENTAL>)
    target_compile_options(common PRIVATE $<$<CONFIG:Debug>:/ZI>)
  endif()
endif()

if(CMAKE_BUILD_TYPE MATCHES "Release")
  target_compile_definitions(${PROJECT_NAME} PRIVATE ASSERTS_DISABLED)
  if(NOT GAME_NO_HOTRELOAD)
    message(FATAL_ERROR "hotreload not supported in release build")
  endif()
endif()

if(EMSCRIPTEN)
  set(SHELL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/web/shell.html")
  set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS "${SHELL_FILE}")
  set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
  target_link_options(${PROJECT_NAME} PRIVATE -sUSE_GLFW=3
                      -sALLOW_MEMORY_GROWTH --shell-file "${SHELL_FILE}")
endif()

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES CXX_STANDARD_REQUIRED ON
             CXX_STANDARD 20
             CXX_SCAN_FOR_MODULES OFF
             CXX_MODULE_STD OFF
             CXX_EXTENSIONS OFF)

set_target_properties(
  pool_test
  PROPERTIES CXX_STANDARD_REQUIRED ON
             CXX_STANDARD 20
             CXX_SCAN_FOR_MODULES OFF
             CXX_MODULE_STD OFF
             CXX_EXTENSIONS OFF)
target_include_directories(pool_test PRIVATE src/)
target_compile_options(pool_test PRIVATE -O0 -ggdb)

if(NOT GAME_NO_HOTRELOAD)
  set_target_properties(
    ${PROJECT_NAME}_lib
    PROPERTIES CXX_STANDARD_REQUIRED ON
               CXX_STANDARD 20
               CXX_SCAN_FOR_MODULES OFF
               CXX_MODULE_STD OFF
               CXX_EXTENSIONS OFF)
  target_include_directories(${PROJECT_NAME}_lib PRIVATE ${include_directories})
  set_target_properties(
    common
    PROPERTIES CXX_STANDARD_REQUIRED ON
               CXX_STANDARD 20
               CXX_SCAN_FOR_MODULES OFF
               CXX_MODULE_STD OFF
               CXX_EXTENSIONS OFF)
  target_include_directories(common PRIVATE ${include_directories})
endif()
target_include_directories(${PROJECT_NAME} PRIVATE ${include_directories})

find_package(raylib REQUIRED NO_CMAKE_SYSTEM_PATH)
find_package(box2d REQUIRED NO_CMAKE_SYSTEM_PATH)
find_package(imgui REQUIRED NO_CMAKE_SYSTEM_PATH)
find_package(fmt REQUIRED NO_CMAKE_SYSTEM_PATH)

add_subdirectory(assets)

set(LIBS_TO_LINK raylib imgui::imgui fmt::fmt box2d::box2d)
target_link_libraries(${PROJECT_NAME} ${LIBS_TO_LINK} game::resources)
target_compile_definitions(${PROJECT_NAME} PRIVATE CMRC_NO_EXCEPTIONS=1)

# logging uses raylib enums/types and formatter
target_link_libraries(pool_test PRIVATE fmt::fmt raylib)

if(NOT GAME_NO_HOTRELOAD)
  target_link_libraries(common ${LIBS_TO_LINK})
  target_link_libraries(${PROJECT_NAME}_lib ${LIBS_TO_LINK} common
                        game::resources)
  target_link_libraries(${PROJECT_NAME} common)
  target_compile_definitions(${PROJECT_NAME}_lib PRIVATE CMRC_NO_EXCEPTIONS=1)
  target_compile_definitions(common PRIVATE CMRC_NO_EXCEPTIONS=1)
endif()

# on windows, copy all DLLs into build result folder
if(WIN32)
  add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND_EXPAND_LISTS)
endif()

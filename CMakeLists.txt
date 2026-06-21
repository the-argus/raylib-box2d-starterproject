cmake_minimum_required(VERSION 3.28)

project(underhanders)

set(PACKAGESOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/.packages/cmake_prefix_path")
if(EXISTS ${PACKAGESOURCE})
  message(STATUS "File ${PACKAGESOURCE} exists")
  file(STRINGS ${PACKAGESOURCE} ADDITIONAL_PREFIX_PATH)
  foreach(PREFIX_PATH_ENTRY ${ADDITIONAL_PREFIX_PATH})
    message(STATUS "Adding ${PREFIX_PATH_ENTRY} to CMAKE_PREFIX_PATH")
    list(APPEND CMAKE_PREFIX_PATH "${PREFIX_PATH_ENTRY}")
  endforeach()
endif()

find_program(MOLD_PROGRAM mold)
if(MOLD_PROGRAM)
  set(CMAKE_LINKER_TYPE MOLD)
  message(STATUS "mold found: ${MOLD_PROGRAM}")
endif()

set(common_source_files src/logging.cpp)
set(hotreloaded_source_files src/entry.cpp src/arena.cpp)
set(main_source_files src/main.cpp)

# the game may run `cmake --build HOTRELOAD_BUILD_DIR --target
# HOTRELOAD_LIB_TARGET` in debug mode, and re-link to it
if(CMAKE_BUILD_TYPE MATCHES "Debug")
  add_library(${PROJECT_NAME}_lib SHARED ${hotreloaded_source_files}
                                         ${common_source_files})
  add_executable(${PROJECT_NAME} ${main_source_files} ${common_source_files})
  add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_lib)
  target_compile_definitions(
    ${PROJECT_NAME}
    PRIVATE HOTRELOAD_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/src"
            HOTRELOAD_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}"
            HOTRELOAD_LIB_TARGET="${PROJECT_NAME}_lib"
            HOTRELOAD_LIB_PATH=\"$<TARGET_FILE:${PROJECT_NAME}_lib>\")
else()
  # in release mode, _lib and the executable are the same
  add_executable(
    ${PROJECT_NAME} ${main_source_files} ${hotreloaded_source_files}
                    ${common_source_files})
endif()

set(DEBUG_COMPILE_OPTIONS -fsanitize=address -O0 -ggdb -fPIC)
set(DEBUG_LINK_OPTIONS -fsanitize=address)

if(CMAKE_BUILD_TYPE MATCHES "Debug" AND NOT MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE ${DEBUG_COMPILE_OPTIONS})
  target_link_options(${PROJECT_NAME} PRIVATE ${DEBUG_LINK_OPTIONS})
  target_compile_options(${PROJECT_NAME}_lib PRIVATE ${DEBUG_COMPILE_OPTIONS})
  target_link_options(${PROJECT_NAME}_lib PRIVATE ${DEBUG_LINK_OPTIONS})
endif()

if(CMAKE_BUILD_TYPE MATCHES "Release")
  target_compile_definitions(${PROJECT_NAME} PRIVATE ASSERTS_DISABLED)
endif()

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES CXX_STANDARD_REQUIRED ON
             CXX_STANDARD 20
             CXX_SCAN_FOR_MODULES OFF
             CXX_MODULE_STD OFF
             CXX_EXTENSIONS OFF)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  set_target_properties(
    ${PROJECT_NAME}_lib
    PROPERTIES CXX_STANDARD_REQUIRED ON
               CXX_STANDARD 20
               CXX_SCAN_FOR_MODULES OFF
               CXX_MODULE_STD OFF
               CXX_EXTENSIONS OFF)
  target_include_directories(${PROJECT_NAME}_lib PRIVATE src/)
endif()
target_include_directories(${PROJECT_NAME} PRIVATE src/)

find_package(raylib REQUIRED NO_CMAKE_SYSTEM_PATH)
find_package(box2d REQUIRED NO_CMAKE_SYSTEM_PATH)
find_package(imgui REQUIRED NO_CMAKE_SYSTEM_PATH)
find_package(fmt REQUIRED NO_CMAKE_SYSTEM_PATH)

# main needs raylib and imgui because those have some static/global
# initialization that needs to stick around
target_link_libraries(${PROJECT_NAME} raylib imgui::imgui fmt::fmt box2d::box2d)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  target_link_libraries(${PROJECT_NAME}_lib raylib imgui::imgui fmt::fmt box2d::box2d)
endif()

# on windows, copy all DLLs into build result folder
if(WIN32)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND_EXPAND_LISTS)
endif()

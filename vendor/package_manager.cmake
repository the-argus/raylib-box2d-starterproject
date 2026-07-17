cmake_minimum_required(VERSION 3.14)

get_filename_component(SCRIPT_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)

set(OUTPUT_ROOT_DIR "${SCRIPT_DIR}/.packages")
set(INSTALLS_DIR "${OUTPUT_ROOT_DIR}/prefixes")
set(BUILDS_DIR "${OUTPUT_ROOT_DIR}/builds")

set(RAYLIB_INSTALL_DIR "${INSTALLS_DIR}/raylib")
set(BOX2D_INSTALL_DIR "${INSTALLS_DIR}/box2d")
set(IMGUI_INSTALL_DIR "${INSTALLS_DIR}/imgui")
set(FMT_INSTALL_DIR "${INSTALLS_DIR}/fmt")

set(RAYLIB_SRC_DIR "${SCRIPT_DIR}/raylib")
set(BOX2D_SRC_DIR "${SCRIPT_DIR}/box2d")
set(IMGUI_SRC_DIR "${SCRIPT_DIR}/imgui")
set(FMT_SRC_DIR "${SCRIPT_DIR}/fmt")

set(RAYLIB_BUILD_DIR "${BUILDS_DIR}/raylib")
set(BOX2D_BUILD_DIR "${BUILDS_DIR}/box2d")
set(IMGUI_BUILD_DIR "${BUILDS_DIR}/imgui")
set(FMT_BUILD_DIR "${BUILDS_DIR}/fmt")

if(NOT EXISTS "${RAYLIB_SRC_DIR}/CMakeLists.txt")
  message(
    FATAL_ERROR
      "missing Raylib, make sure to run git submodule update --init --recursive"
  )
endif()

file(MAKE_DIRECTORY ${INSTALLS_DIR})
file(MAKE_DIRECTORY ${BUILDS_DIR})

find_program(MOLD_EXECUTABLE mold)
if(MOLD_EXECUTABLE)
  set(LINKER_TYPE "MOLD")
else()
  set(LINKER_TYPE "DEFAULT")
endif()

set(BUILD_TYPE "Debug")

# NOTE: disabling PIC on windows isn't exactly right, clang and mingw could use it.
# pretending win32 == MSVC
if(NOT WIN32 AND ${BUILD_TYPE} MATCHES Debug)
  set(COMPILE_FLAGS -fPIC)
endif()

# Configure raylib
message(STATUS "Configuring Raylib...")
set(RAYLIB_COMMON_FLAGS -DBUILD_EXAMPLES=OFF)
set(COMMON_FLAGS)

if(UNIX AND NOT APPLE)
  # USE_EXTERNAL_GLFW is for nixos where there seems to be some wizardry needed to
  # build a working glfw
  list(APPEND RAYLIB_COMMON_FLAGS -DGLFW_BUILD_WAYLAND=OFF
       -DGLFW_BUILD_X11=ON -DUSE_EXTERNAL_GLFW=ON)
endif()

if(${BUILD_TYPE} MATCHES Debug)
  if(NOT WIN32)
	list(APPEND RAYLIB_COMMON_FLAGS -DENABLE_ASAN=ON)
  endif()
  list(APPEND COMMON_FLAGS -DBUILD_SHARED_LIBS=ON)
endif()

execute_process(
  COMMAND
    cmake -S ${RAYLIB_SRC_DIR} -B ${RAYLIB_BUILD_DIR}
    -DCMAKE_LINKER_TYPE=${LINKER_TYPE} -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ${RAYLIB_COMMON_FLAGS} ${COMMON_FLAGS}
    -DCMAKE_CXX_FLAGS=${COMPILE_FLAGS} -DCMAKE_C_FLAGS=${COMPILE_FLAGS})
execute_process(COMMAND cmake --build ${RAYLIB_BUILD_DIR} --config ${BUILD_TYPE} --parallel)
execute_process(COMMAND cmake --install ${RAYLIB_BUILD_DIR} --config ${BUILD_TYPE} --prefix
                        ${RAYLIB_INSTALL_DIR})

# Configure, build, and install Box2D
message(STATUS "Configuring Box2D...")
execute_process(
  COMMAND
    cmake -S ${BOX2D_SRC_DIR} -B ${BOX2D_BUILD_DIR}
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DCMAKE_LINKER_TYPE=${LINKER_TYPE} -DBOX2D_SAMPLES=OFF ${COMMON_FLAGS}
    -DBOX2D_BENCHMARKS=OFF -DBOX2D_DOCS=OFF -DBOX2D_PROFILE=OFF
    -DBOX2D_VALIDATE=ON -DBOX2D_UNIT_TESTS=OFF
    -DCMAKE_CXX_FLAGS=${COMPILE_FLAGS} -DCMAKE_C_FLAGS=${COMPILE_FLAGS})
execute_process(COMMAND cmake --build ${BOX2D_BUILD_DIR} --config ${BUILD_TYPE} --parallel)
execute_process(COMMAND cmake --install ${BOX2D_BUILD_DIR} --config ${BUILD_TYPE} --prefix
                        ${BOX2D_INSTALL_DIR})

# Configure, build, and install ImGui
message(STATUS "Configuring ImGui...")
execute_process(
  COMMAND
    cmake -S ${IMGUI_SRC_DIR} -B ${IMGUI_BUILD_DIR}
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DCMAKE_LINKER_TYPE=${LINKER_TYPE} ${COMMON_FLAGS}
    -DCMAKE_PREFIX_PATH=${RAYLIB_INSTALL_DIR} -DIMGUI_BUILD_RAYLIB_BINDING=ON
    -DIMGUI_BUILD_GLFW_BINDING=OFF -DCMAKE_CXX_FLAGS=${COMPILE_FLAGS}
    -DCMAKE_C_FLAGS=${COMPILE_FLAGS})
execute_process(COMMAND cmake --build ${IMGUI_BUILD_DIR} --config ${BUILD_TYPE} --parallel)
execute_process(COMMAND cmake --install ${IMGUI_BUILD_DIR} --config ${BUILD_TYPE} --prefix
                        ${IMGUI_INSTALL_DIR})

# Configure, build, and install fmt
message(STATUS "Configuring fmt...")
execute_process(
  COMMAND
    cmake -S ${FMT_SRC_DIR} -B ${FMT_BUILD_DIR}
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DCMAKE_LINKER_TYPE=${LINKER_TYPE} -DFMT_DOC=OFF -DFMT_TEST=OFF
    -DFMT_FUZZ=OFF -DFMT_CUDA_TEST=OFF -DFMT_UNICODE=ON ${COMMON_FLAGS}
    -DCMAKE_CXX_FLAGS=${COMPILE_FLAGS} -DCMAKE_C_FLAGS=${COMPILE_FLAGS})
execute_process(COMMAND cmake --build ${FMT_BUILD_DIR} --config ${BUILD_TYPE} --parallel)
execute_process(COMMAND cmake --install ${FMT_BUILD_DIR} --config ${BUILD_TYPE} --prefix
                        ${FMT_INSTALL_DIR})

# Write cmake prefix path to output file
set(OUTSTRINGS "${OUTPUT_ROOT_DIR}/cmake_prefix_path")
file(REMOVE ${OUTSTRINGS})
file(APPEND ${OUTSTRINGS} "${RAYLIB_INSTALL_DIR}\n")
file(APPEND ${OUTSTRINGS} "${BOX2D_INSTALL_DIR}\n")
file(APPEND ${OUTSTRINGS} "${IMGUI_INSTALL_DIR}\n")
file(APPEND ${OUTSTRINGS} "${FMT_INSTALL_DIR}\n")

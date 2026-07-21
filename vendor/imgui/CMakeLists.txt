# copied from
# https://github.com/microsoft/vcpkg/blob/master/ports/imgui/CMakeLists.txt as
# of 05442024c3fda64320bd25d2251cc9807b84fb6f

cmake_minimum_required(VERSION 3.16)
project(imgui CXX)

set(IMGUI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
set(RLIMGUI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/rlImGui")
set(CMAKE_DEBUG_POSTFIX d)

if(APPLE)
  set(CMAKE_CXX_STANDARD 11)
  enable_language(OBJCXX)
endif()

option(IMGUI_BUILD_ALLEGRO5_BINDING "Build Allegro5 binding" OFF)
option(IMGUI_BUILD_ANDROID_BINDING "Build Android binding" OFF)
option(IMGUI_BUILD_DX9_BINDING "Build DirectX 9 binding" OFF)
option(IMGUI_BUILD_DX10_BINDING "Build DirectX 10 binding" OFF)
option(IMGUI_BUILD_DX11_BINDING "Build DirectX 11 binding" OFF)
option(IMGUI_BUILD_DX12_BINDING "Build DirectX 12 binding" OFF)
option(IMGUI_BUILD_GLFW_BINDING "Build GLFW binding" OFF)
option(IMGUI_BUILD_RAYLIB_BINDING "Build Raylib binding" OFF)
option(IMGUI_BUILD_GLUT_BINDING "Build GLUT binding" OFF)
option(IMGUI_BUILD_METAL_BINDING "Build Apple Metal binding" OFF)
option(IMGUI_BUILD_OPENGL2_BINDING "Build OpenGL 2 binding" OFF)
option(IMGUI_BUILD_OPENGL3_BINDING "Build OpenGL 3 binding" OFF)
option(IMGUI_BUILD_OSX_BINDING "Build OSX binding" OFF)
option(IMGUI_BUILD_SDL3_BINDING "Build SDL3 binding" OFF)
option(IMGUI_BUILD_SDLGPU3_BINDING "Build SDL3 SDLGpu API binding" OFF)
option(IMGUI_BUILD_SDL3_RENDERER_BINDING "Build SDL3 renderer binding" OFF)
option(IMGUI_BUILD_VULKAN_BINDING "Build Vulkan binding" OFF)
option(IMGUI_BUILD_WIN32_BINDING "Build WIN32 binding" OFF)
option(IMGUI_FREETYPE "Include Freetype support" OFF)
option(IMGUI_FREETYPE_SVG "Include plutosvg for freetype support" OFF)
option(IMGUI_USE_WCHAR32 "Use wide chars in strings" OFF)
option(IMGUI_TEST_ENGINE "Include the imgui testing framework" OFF)
option(IMGUI_TEST_ENGINE "Include the imgui testing framework (requires STB)"
       OFF)
option(IMGUI_SKIP_HEADERS "Skip headers during installation process" OFF)

add_library(${PROJECT_NAME} "")
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_include_directories(
  ${PROJECT_NAME}
  PUBLIC
    "$<BUILD_INTERFACE:${IMGUI_SOURCE_DIR};${IMGUI_SOURCE_DIR}/test-engine>"
    $<INSTALL_INTERFACE:include>)

if(BUILD_SHARED_LIBS AND WIN32)
  target_compile_definitions(${PROJECT_NAME}
    PRIVATE   "IMGUI_API=__declspec(dllexport)"
    INTERFACE "IMGUI_API=__declspec(dllimport)")
endif()

target_sources(
  ${PROJECT_NAME}
  PRIVATE ${IMGUI_SOURCE_DIR}/imgui.cpp
          ${IMGUI_SOURCE_DIR}/imgui_demo.cpp
          ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
          ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
          ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
          ${IMGUI_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)

if(IMGUI_BUILD_ALLEGRO5_BINDING)
  find_package(Allegro CONFIG REQUIRED)
  target_link_libraries(
    ${PROJECT_NAME} PRIVATE Allegro::allegro Allegro::allegro_ttf
                            Allegro::allegro_font Allegro::allegro_main)
  target_sources(${PROJECT_NAME}
                 PRIVATE ${IMGUI_SOURCE_DIR}/backends/imgui_impl_allegro5.cpp)
endif()

if(IMGUI_BUILD_ANDROID_BINDING)
  target_sources(${PROJECT_NAME}
                 PRIVATE ${IMGUI_SOURCE_DIR}/backends/imgui_impl_android.cpp)
endif()

if(IMGUI_BUILD_DX9_BINDING)
  target_sources(${PROJECT_NAME}
                 PRIVATE ${IMGUI_SOURCE_DIR}/backends/imgui_impl_dx9.cpp)
endif()

if(IMGUI_BUILD_DX10_BINDING)
  target_sources(${PROJECT_NAME}
                 PRIVATE ${IMGUI_SOURCE_DIR}/backends/imgui_impl_dx10.cpp)
endif()

if(IMGUI_BUILD_DX11_BINDING)
  target_sources(${PROJECT_NAME}
                 PRIVATE ${IMGUI_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
endif()

if(IMGUI_BUILD_DX12_BINDING)
  target_sources(${PROJECT_NAME}
                 PRIVATE ${IMGUI_SOURCE_DIR}/backends/imgui_impl_dx12.cpp)
endif()

if(IMGUI_BUILD_GLFW_BINDING)
  if(NOT EMSCRIPTEN)
    find_package(glfw3 CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PUBLIC glfw)
  endif()
  target_sources(${PROJECT_NAME}
                 PRIVATE ${IMGUI_SOURCE_DIR}/backends/imgui_impl_glfw.cpp)
endif()

if(IMGUI_BUILD_GLUT_BINDING)
  find_package(GLUT REQUIRED)
  target_link_libraries(${PROJECT_NAME} PUBLIC GLUT::GLUT)
  target_sources(${PROJECT_NAME}
                 PRIVATE ${IMGUI_SOURCE_DIR}/backends/imgui_impl_glut.cpp)
endif()

if(IMGUI_BUILD_METAL_BINDING)
  target_sources(${PROJECT_NAME}
                 PRIVATE ${IMGUI_SOURCE_DIR}/backends/imgui_impl_metal.mm)
  set_source_files_properties(${IMGUI_SOURCE_DIR}/backends/imgui_impl_metal.mm
                              PROPERTIES COMPILE_FLAGS -fobjc-weak)
endif()

if(IMGUI_BUILD_OPENGL2_BINDING)
  target_sources(${PROJECT_NAME}
                 PRIVATE ${IMGUI_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp)
endif()

if(IMGUI_BUILD_OPENGL3_BINDING)
  target_sources(${PROJECT_NAME}
                 PRIVATE ${IMGUI_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
endif()

if(IMGUI_BUILD_OSX_BINDING)
  target_sources(${PROJECT_NAME}
                 PRIVATE ${IMGUI_SOURCE_DIR}/backends/imgui_impl_osx.mm)
endif()

if(IMGUI_BUILD_RAYLIB_BINDING)
  find_package(raylib REQUIRED)
  target_sources(
    ${PROJECT_NAME}
    PRIVATE ${RLIMGUI_SOURCE_DIR}/rlImGui.cpp
    # PUBLIC FILE_SET HEADERS BASE_DIRS ${RLIMGUI_SOURCE_DIR} FILES
    # ${RLIMGUI_SOURCE_DIR}/imgui_impl_raylib.h ${RLIMGUI_SOURCE_DIR}/rlImGui.h
    # ${RLIMGUI_SOURCE_DIR}/rlImGuiColors.h
  )
  # install(TARGETS ${PROJECT_NAME} FILE_SET HEADERS)

  target_include_directories(${PROJECT_NAME} PRIVATE ${RLIMGUI_SOURCE_DIR})

  target_link_libraries(${PROJECT_NAME} PUBLIC raylib)
  target_compile_definitions(${PROJECT_NAME} PUBLIC NO_FONT_AWESOME)
endif()

if(IMGUI_BUILD_SDL3_BINDING)
  find_package(SDL3 CONFIG REQUIRED)
  target_link_libraries(${PROJECT_NAME} PUBLIC SDL3::SDL3)
  target_sources(${PROJECT_NAME}
                 PRIVATE ${IMGUI_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp)
endif()

if(IMGUI_BUILD_SDLGPU3_BINDING)
  find_package(SDL3 CONFIG REQUIRED)
  target_link_libraries(${PROJECT_NAME} PUBLIC SDL3::SDL3)
  target_sources(${PROJECT_NAME}
                 PRIVATE ${IMGUI_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp)
endif()

if(IMGUI_BUILD_SDL3_RENDERER_BINDING)
  find_package(SDL3 CONFIG REQUIRED)
  target_link_libraries(${PROJECT_NAME} PUBLIC SDL3::SDL3)
  target_sources(
    ${PROJECT_NAME}
    PRIVATE ${IMGUI_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp)
endif()

if(IMGUI_BUILD_VULKAN_BINDING)
  find_package(Vulkan REQUIRED)
  target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan)
  target_sources(${PROJECT_NAME}
                 PRIVATE ${IMGUI_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
endif()

if(IMGUI_BUILD_WIN32_BINDING)
  target_sources(${PROJECT_NAME}
                 PRIVATE ${IMGUI_SOURCE_DIR}/backends/imgui_impl_win32.cpp)
endif()

if(IMGUI_FREETYPE)
  find_package(freetype CONFIG REQUIRED)
  target_link_libraries(${PROJECT_NAME} PUBLIC freetype)
  target_sources(${PROJECT_NAME}
                 PRIVATE ${IMGUI_SOURCE_DIR}/misc/freetype/imgui_freetype.cpp)
  target_compile_definitions(${PROJECT_NAME} PUBLIC IMGUI_ENABLE_FREETYPE)
endif()

if(IMGUI_FREETYPE_SVG)
  find_package(plutosvg CONFIG REQUIRED)
  target_link_libraries(${PROJECT_NAME} PUBLIC plutosvg::plutosvg)
  target_compile_definitions(${PROJECT_NAME}
                             PUBLIC IMGUI_ENABLE_FREETYPE_PLUTOSVG)
endif()

if(IMGUI_USE_WCHAR32)
  target_compile_definitions(${PROJECT_NAME} PUBLIC IMGUI_USE_WCHAR32)
endif()

if(IMGUI_TEST_ENGINE)
  find_package(Stb REQUIRED)
  target_include_directories(${PROJECT_NAME} PRIVATE ${Stb_INCLUDE_DIR})
  target_sources(
    ${PROJECT_NAME}
    PRIVATE ${IMGUI_SOURCE_DIR}/test-engine/imgui_capture_tool.cpp
            ${IMGUI_SOURCE_DIR}/test-engine/imgui_te_context.cpp
            ${IMGUI_SOURCE_DIR}/test-engine/imgui_te_coroutine.cpp
            ${IMGUI_SOURCE_DIR}/test-engine/imgui_te_engine.cpp
            ${IMGUI_SOURCE_DIR}/test-engine/imgui_te_exporters.cpp
            ${IMGUI_SOURCE_DIR}/test-engine/imgui_te_perftool.cpp
            ${IMGUI_SOURCE_DIR}/test-engine/imgui_te_ui.cpp
            ${IMGUI_SOURCE_DIR}/test-engine/imgui_te_utils.cpp)
endif()

list(REMOVE_DUPLICATES BINDINGS_SOURCES)

install(
  TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}_target
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

foreach(BINDING_TARGET ${BINDING_TARGETS})
  install(
    TARGETS ${BINDING_TARGET}
    EXPORT ${PROJECT_NAME}_target
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
endforeach()

if(NOT IMGUI_SKIP_HEADERS)
  install(
    FILES ${IMGUI_SOURCE_DIR}/imgui.h
          ${IMGUI_SOURCE_DIR}/imconfig.h
          ${IMGUI_SOURCE_DIR}/imgui_internal.h
          ${IMGUI_SOURCE_DIR}/imstb_textedit.h
          ${IMGUI_SOURCE_DIR}/imstb_rectpack.h
          ${IMGUI_SOURCE_DIR}/imstb_truetype.h
          ${IMGUI_SOURCE_DIR}/misc/cpp/imgui_stdlib.h
    DESTINATION include)

  if(IMGUI_BUILD_ALLEGRO5_BINDING)
    install(FILES ${IMGUI_SOURCE_DIR}/backends/imgui_impl_allegro5.h
            DESTINATION include)
  endif()

  if(IMGUI_BUILD_ANDROID_BINDING)
    install(FILES ${IMGUI_SOURCE_DIR}/backends/imgui_impl_android.h
            DESTINATION include)
  endif()

  if(IMGUI_BUILD_DX9_BINDING)
    install(FILES ${IMGUI_SOURCE_DIR}/backends/imgui_impl_dx9.h
            DESTINATION include)
  endif()

  if(IMGUI_BUILD_DX10_BINDING)
    install(FILES ${IMGUI_SOURCE_DIR}/backends/imgui_impl_dx10.h
            DESTINATION include)
  endif()

  if(IMGUI_BUILD_DX11_BINDING)
    install(FILES ${IMGUI_SOURCE_DIR}/backends/imgui_impl_dx11.h
            DESTINATION include)
  endif()

  if(IMGUI_BUILD_DX12_BINDING)
    install(FILES ${IMGUI_SOURCE_DIR}/backends/imgui_impl_dx12.h
            DESTINATION include)
  endif()

  if(IMGUI_BUILD_GLFW_BINDING)
    install(FILES ${IMGUI_SOURCE_DIR}/backends/imgui_impl_glfw.h
            DESTINATION include)
  endif()

  if(IMGUI_BUILD_GLUT_BINDING)
    install(FILES ${IMGUI_SOURCE_DIR}/backends/imgui_impl_glut.h
            DESTINATION include)
  endif()

  if(IMGUI_BUILD_METAL_BINDING)
    install(FILES ${IMGUI_SOURCE_DIR}/backends/imgui_impl_metal.h
            DESTINATION include)
  endif()

  if(IMGUI_BUILD_OPENGL2_BINDING)
    install(FILES ${IMGUI_SOURCE_DIR}/backends/imgui_impl_opengl2.h
            DESTINATION include)
  endif()

  if(IMGUI_BUILD_OPENGL3_BINDING)
    install(FILES ${IMGUI_SOURCE_DIR}/backends/imgui_impl_opengl3.h
                  ${IMGUI_SOURCE_DIR}/backends/imgui_impl_opengl3_loader.h
            DESTINATION include)
  endif()

  if(IMGUI_BUILD_OSX_BINDING)
    install(FILES ${IMGUI_SOURCE_DIR}/backends/imgui_impl_osx.h
            DESTINATION include)
  endif()

  if(IMGUI_BUILD_RAYLIB_BINDING)
    install(FILES ${RLIMGUI_SOURCE_DIR}/imgui_impl_raylib.h
                  ${RLIMGUI_SOURCE_DIR}/rlImGui.h
                  ${RLIMGUI_SOURCE_DIR}/rlImGuiColors.h DESTINATION include)
    install(FILES ${RLIMGUI_SOURCE_DIR}/extras/IconsFontAwesome6.h
                  ${RLIMGUI_SOURCE_DIR}/extras/FA6FreeSolidFontData.h
            DESTINATION include/extras)
  endif()

  if(IMGUI_BUILD_SDL3_BINDING)
    install(FILES ${IMGUI_SOURCE_DIR}/backends/imgui_impl_sdl3.h
            DESTINATION include)
  endif()

  if(IMGUI_BUILD_SDLGPU3_BINDING)
    install(FILES ${IMGUI_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.h
                  ${IMGUI_SOURCE_DIR}/backends/imgui_impl_sdlgpu3_shaders.h
            DESTINATION include)
  endif()

  if(IMGUI_BUILD_SDL3_RENDERER_BINDING)
    install(FILES ${IMGUI_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.h
            DESTINATION include)
  endif()

  if(IMGUI_BUILD_VULKAN_BINDING)
    install(FILES ${IMGUI_SOURCE_DIR}/backends/imgui_impl_vulkan.h
            DESTINATION include)
  endif()

  if(IMGUI_BUILD_WIN32_BINDING)
    install(FILES ${IMGUI_SOURCE_DIR}/backends/imgui_impl_win32.h
            DESTINATION include)
  endif()

  if(IMGUI_FREETYPE)
    install(FILES ${IMGUI_SOURCE_DIR}/misc/freetype/imgui_freetype.h
            DESTINATION include)
  endif()

  if(IMGUI_TEST_ENGINE)
    install(
      FILES ${IMGUI_SOURCE_DIR}/test-engine/imgui_capture_tool.h
            ${IMGUI_SOURCE_DIR}/test-engine/imgui_te_context.h
            ${IMGUI_SOURCE_DIR}/test-engine/imgui_te_coroutine.h
            ${IMGUI_SOURCE_DIR}/test-engine/imgui_te_engine.h
            ${IMGUI_SOURCE_DIR}/test-engine/imgui_te_exporters.h
            ${IMGUI_SOURCE_DIR}/test-engine/imgui_te_imconfig.h
            ${IMGUI_SOURCE_DIR}/test-engine/imgui_te_internal.h
            ${IMGUI_SOURCE_DIR}/test-engine/imgui_te_perftool.h
            ${IMGUI_SOURCE_DIR}/test-engine/imgui_te_ui.h
            ${IMGUI_SOURCE_DIR}/test-engine/imgui_te_utils.h
      DESTINATION include)
  endif()
endif()

include(CMakePackageConfigHelpers)
configure_package_config_file(imgui-config.cmake.in imgui-config.cmake
                              INSTALL_DESTINATION share/imgui)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/imgui-config.cmake
        DESTINATION share/imgui)

install(
  EXPORT ${PROJECT_NAME}_target
  NAMESPACE ${PROJECT_NAME}::
  FILE ${PROJECT_NAME}-targets.cmake
  DESTINATION share/${PROJECT_NAME})

cmake_minimum_required(VERSION 3.12)
project(renderer_Vulkan LANGUAGES C CXX)

find_package(PkgConfig)
pkg_check_modules(RENDERER_Vulkan REQUIRED IMPORTED_TARGET
  vulkan
)

include(MakeObject)
function(build_vulkan_shaders out_var type)
  foreach(shader ${ARGN})
    set(out_f "${CMAKE_CURRENT_BINARY_DIR}/${shader}.spv")
    add_custom_command(OUTPUT "${out_f}"
      COMMAND glslangValidator --target-env vulkan1.0 -S "${type}" -o "${out_f}"
            "${CMAKE_CURRENT_SOURCE_DIR}/${shader}"
      MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/${shader}"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/shader"
      COMMENT "Preprocessing shader ${shader}"
      VERBATIM
    )
  endforeach()

  list(TRANSFORM ARGN APPEND ".spv" OUTPUT_VARIABLE ARGN_SPV)

  set(CMAKE_CURRENT_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
  make_object(
    VULKAN_SHADER
    ${ARGN_SPV}
  )

  set(${out_var}_OBJS "${VULKAN_SHADER_OBJS}" PARENT_SCOPE)
  set(${out_var}_INCS "${VULKAN_SHADER_INCS}" PARENT_SCOPE)
endfunction()

build_vulkan_shaders(
  VULKAN_VERT_SHADER
  vert
  shader/basic.vert
)

build_vulkan_shaders(
  VULKAN_FRAG_SHADER
  frag
  shader/basic.frag
)

add_library(renderer_Vulkan STATIC
  extensions.c
  imgui.c
  vulkan.c
  vulkan_util.c
  ${VULKAN_VERT_SHADER_OBJS}
  ${VULKAN_FRAG_SHADER_OBJS}
  ${PROJECT_TOP}/repos/cimgui/imgui/backends/imgui_impl_vulkan.cpp
)

target_compile_definitions(renderer_Vulkan PRIVATE CIMGUI_DEFINE_ENUMS_AND_STRUCTS=1 CIMGUI_USE_VULKAN=1)

target_link_libraries(renderer_Vulkan
  PkgConfig::RENDERER_Vulkan
  lg_common

  cimgui
)

target_include_directories(renderer_Vulkan
  PRIVATE
    src
    ${VULKAN_VERT_SHADER_INCS}
    ${VULKAN_FRAG_SHADER_INCS}
)

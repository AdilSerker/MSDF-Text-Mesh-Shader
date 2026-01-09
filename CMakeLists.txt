cmake_minimum_required(VERSION 3.20)
project(msdf_text_meshshader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

add_executable(app
  src/main.cpp
  src/platform/Window.cpp
  src/vk/VulkanContext.cpp
  src/vk/VulkanUtils.cpp
  src/vk/Swapchain.cpp
  src/vk/MeshTestPipeline.cpp
  src/vk/MeshTestRenderer.cpp
  src/vk/Texture2D.cpp
  src/vk/MsdfAtlas.cpp
  src/vk/MsdfFont.cpp
)

target_include_directories(app PRIVATE src)

# --- Vulkan ---
find_package(Vulkan REQUIRED)
target_link_libraries(app PRIVATE Vulkan::Vulkan)

# --- GLFW ---
find_package(glfw3 CONFIG QUIET)
if (glfw3_FOUND)
  target_link_libraries(app PRIVATE glfw)
else()
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.9
  )

  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(glfw)
  target_link_libraries(app PRIVATE glfw)
endif()

# --- nlohmann/json ---
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)
target_link_libraries(app PRIVATE nlohmann_json::nlohmann_json)

# --- Warnings ---
if (MSVC)
  target_compile_options(app PRIVATE /W4 /permissive-)
else()
  target_compile_options(app PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Shaders (glslangValidator) ---
find_program(GLSLANG_VALIDATOR glslangValidator
  HINTS ENV VULKAN_SDK
  PATH_SUFFIXES Bin
)

if (NOT GLSLANG_VALIDATOR)
  message(FATAL_ERROR "glslangValidator not found. Install Vulkan SDK and ensure it's in PATH.")
endif()

set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_OUT_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_OUT_DIR}")

# Собираем все .glsl в папке shaders
file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS
  "${SHADER_SRC_DIR}/*.glsl"
)

set(SPIRV_BINARIES "")

foreach(SHADER ${SHADER_SOURCES})
  get_filename_component(FILE_NAME ${SHADER} NAME)          # например lb_parenthesis.mesh.glsl
  string(REPLACE ".glsl" "" FILE_BASE ${FILE_NAME})         # -> lb_parenthesis.mesh
  set(SPIRV "${SHADER_OUT_DIR}/${FILE_BASE}.spv")           # -> .../lb_parenthesis.mesh.spv

  # Определяем стадию по имени файла
  if (FILE_NAME MATCHES "\\.vert\\.glsl$")
    set(STAGE vert)
  elseif (FILE_NAME MATCHES "\\.frag\\.glsl$")
    set(STAGE frag)
  elseif (FILE_NAME MATCHES "\\.comp\\.glsl$")
    set(STAGE comp)
  elseif (FILE_NAME MATCHES "\\.mesh\\.glsl$")
    set(STAGE mesh)
  elseif (FILE_NAME MATCHES "\\.task\\.glsl$")
    set(STAGE task)
  else()
    message(FATAL_ERROR "Unknown shader stage for file: ${FILE_NAME}. Name must end with .vert.glsl/.frag.glsl/.mesh.glsl/.task.glsl/.comp.glsl")
  endif()

  add_custom_command(
    OUTPUT "${SPIRV}"
    COMMAND "${GLSLANG_VALIDATOR}" -V --target-env vulkan1.3 -S ${STAGE} -o "${SPIRV}" "${SHADER}"
    DEPENDS "${SHADER}"
    COMMENT "Compiling ${STAGE} shader: ${FILE_NAME}"
    VERBATIM
  )

  list(APPEND SPIRV_BINARIES "${SPIRV}")
endforeach()

add_custom_target(shaders DEPENDS ${SPIRV_BINARIES})
add_dependencies(app shaders)

# --- Compile-time paths ---
file(TO_CMAKE_PATH "${SHADER_OUT_DIR}" APP_SHADER_DIR_PATH)
target_compile_definitions(app PRIVATE APP_SHADER_DIR="${APP_SHADER_DIR_PATH}")


set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
file(TO_CMAKE_PATH "${ASSETS_DIR}" APP_ASSETS_DIR_PATH)
target_compile_definitions(app PRIVATE APP_ASSETS_DIR="${APP_ASSETS_DIR_PATH}")

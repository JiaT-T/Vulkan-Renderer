cmake_minimum_required (VERSION 3.29)

set (TEMPLATE_NAME "Vulkan-Renderer")

project (${TEMPLATE_NAME})

# Add option to enable/disable C++ 20 module
option(ENABLE_CPP20_MODULE "Enable C++ 20 module support for Vulkan" OFF)

# Enable C++ module dependency scanning only if C++ 20 module is enabled
if(ENABLE_CPP20_MODULE)
  set(CMAKE_CXX_SCAN_FOR_MODULES ON)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Vulkan 1.4.335 REQUIRED)
find_package(tinyobjloader REQUIRED)
find_package(tinygltf REQUIRED)
find_package(KTX REQUIRED)

# Set up Vulkan C++ module only if enabled
if(ENABLE_CPP20_MODULE)
  add_library(VulkanCppModule)
  add_library(Vulkan::cppm ALIAS VulkanCppModule)

  target_compile_definitions(VulkanCppModule PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1)
  target_include_directories(VulkanCppModule PUBLIC "${Vulkan_INCLUDE_DIR}")
  target_link_libraries(VulkanCppModule PUBLIC Vulkan::Vulkan)

  set_target_properties(VulkanCppModule PROPERTIES CXX_STANDARD 20)

  # Add MSVC-specific compiler options for proper C++ module support
  if(MSVC)
    target_compile_options(VulkanCppModule PRIVATE
      /std:c++latest      # Use latest C++ standard for better module support
      /permissive-        # Standards conformance mode
      /Zc:__cplusplus     # Enable correct __cplusplus macro
      /EHsc               # Enable C++ exception handling
      /Zc:preprocessor    # Use conforming preprocessor
      /translateInclude   # Automatically translate #include to import for standard library
    )
  endif()

  target_sources(VulkanCppModule
    PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES
    BASE_DIRS
    "${Vulkan_INCLUDE_DIR}"
    FILES
    "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm"
  )


  # Add the vulkan.cppm file directly as a source file
  target_sources(VulkanCppModule
    PRIVATE
    "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm"
  )
else()
  # Create a dummy interface library when C++ 20 module is disabled
  add_library(VulkanCppModule INTERFACE)
  add_library(Vulkan::cppm ALIAS VulkanCppModule)
  target_link_libraries(VulkanCppModule INTERFACE Vulkan::Vulkan)
  target_compile_definitions(VulkanCppModule
    INTERFACE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
  )
endif()

find_package(stb REQUIRED)
set(STB_INCLUDEDIR ${stb_INCLUDE_DIRS})

find_program(SLANGC_EXECUTABLE slangc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin" REQUIRED)
find_program(GLSLC_EXECUTABLE glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin" REQUIRED)

set(TEMPLATE_DIR ${CMAKE_BINARY_DIR}/${TEMPLATE_NAME})

add_executable (${TEMPLATE_NAME} main.cpp Chapter8.cpp stb_image.cpp)
set_target_properties (${TEMPLATE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEMPLATE_DIR})
set_target_properties (${TEMPLATE_NAME} PROPERTIES CXX_STANDARD 20)
target_link_libraries (${TEMPLATE_NAME} Vulkan::cppm glfw)
target_include_directories (${TEMPLATE_NAME} PRIVATE ${STB_INCLUDEDIR})
if(MSVC)
  target_compile_options(${TEMPLATE_NAME} PRIVATE /utf-8)
endif()

# Add compile definition if C++ 20 module is enabled
if(ENABLE_CPP20_MODULE)
  target_compile_definitions(${TEMPLATE_NAME} PRIVATE USE_CPP20_MODULES=1)
endif()

# Define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS to treat VK_ERROR_OUT_OF_DATE_KHR as a success code
target_compile_definitions(${TEMPLATE_NAME} PRIVATE "VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS" )

if(WIN32)
  if(${CMAKE_GENERATOR} MATCHES "Visual Studio.*")
    set_target_properties(${TEMPLATE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${TEMPLATE_DIR})
  endif()
endif()

# Slang shader
set (SHADER_TARGET ${TEMPLATE_NAME}Shader)
set (SHADERS_DIR ${TEMPLATE_DIR}/shaders)
set (SHADER_OUTPUT ${SHADERS_DIR}/slang.spv)
set (SHADER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/shader.slang")
set (ENTRY_POINTS -entry vertMain -entry fragMain)
add_custom_command(
  OUTPUT  ${SHADER_OUTPUT}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
  COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${TEMPLATE_DIR}/shaders/slang.spv
  WORKING_DIRECTORY ${SHADERS_DIR}
  DEPENDS ${SHADER_SOURCES}
  COMMENT "Compiling Slang Shaders"
  VERBATIM
)
add_custom_target(${SHADER_TARGET} DEPENDS ${SHADER_OUTPUT})
add_dependencies(${TEMPLATE_NAME} ${SHADER_TARGET})

# GLSL shaders. Each source keeps its stage in the filename, for example
# Offscreen.vert.shader -> Offscreen.vert.spv.
set (GLSL_SHADER_TARGET ${TEMPLATE_NAME}GlslShaders)
set (GLSL_SHADER_DIR ${TEMPLATE_DIR}/shader)
file(GLOB GLSL_SHADER_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/shader/*.vert.shader"
  "${CMAKE_CURRENT_SOURCE_DIR}/shader/*.frag.shader")
set(GLSL_SHADER_OUTPUTS)
foreach(GLSL_SHADER_SOURCE IN LISTS GLSL_SHADER_SOURCES)
  get_filename_component(GLSL_SHADER_NAME ${GLSL_SHADER_SOURCE} NAME)
  string(REGEX REPLACE "\\.shader$" ".spv" GLSL_SHADER_BINARY ${GLSL_SHADER_NAME})
  set(GLSL_SHADER_OUTPUT "${GLSL_SHADER_DIR}/${GLSL_SHADER_BINARY}")
  add_custom_command(
    OUTPUT ${GLSL_SHADER_OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GLSL_SHADER_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${GLSL_SHADER_SOURCE} -o ${GLSL_SHADER_OUTPUT}
    DEPENDS ${GLSL_SHADER_SOURCE}
    COMMENT "Compiling GLSL shader ${GLSL_SHADER_NAME}"
    VERBATIM)
  list(APPEND GLSL_SHADER_OUTPUTS ${GLSL_SHADER_OUTPUT})
endforeach()
add_custom_target(${GLSL_SHADER_TARGET} DEPENDS ${GLSL_SHADER_OUTPUTS})
add_dependencies(${TEMPLATE_NAME} ${GLSL_SHADER_TARGET})

# Assets
set (ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
file (COPY ${ASSET_DIR}/viking_room.obj DESTINATION ${TEMPLATE_DIR}/models)
file (COPY ${ASSET_DIR}/viking_room.png DESTINATION ${TEMPLATE_DIR}/textures)
file (COPY ${ASSET_DIR}/uv_orientation_test.png DESTINATION ${TEMPLATE_DIR}/textures)

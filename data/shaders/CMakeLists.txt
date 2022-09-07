# Shaders
set(GLSLANG_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")

file(GLOB_RECURSE GLSL_SHADER_FILES "*.vert" "*.frag")

foreach(GLSL_SHADER_FILE ${GLSL_SHADER_FILES})
  get_filename_component(FILE_NAME ${GLSL_SHADER_FILE} NAME)
  set(SPIRV_SHADER_FILE "./${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV_SHADER_FILE}
    COMMAND ${GLSLANG_VALIDATOR} -V ${GLSL_SHADER_FILE} -o ${SPIRV_SHADER_FILE}
    DEPENDS ${SHADER})
  list(APPEND SPIRV_SHADER_FILES ${SPIRV_SHADER_FILE})
endforeach(GLSL_SHADER_FILE)

add_custom_target(Shaders DEPENDS ${SPIRV_SHADER_FILES})

add_dependencies(${NAME} Shaders)
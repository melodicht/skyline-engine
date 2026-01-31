add_library(skl-utils STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/skl_math_utils.cpp
${CMAKE_CURRENT_SOURCE_DIR}/debug.cpp)

if (DEFINED SKL_EXTERNAL_GAME)
    target_include_directories(skl-utils PUBLIC
            ${SKL_ENGINE_PATH}/include/math
            ${SKL_ENGINE_PATH}/include/shared)
else()
    target_include_directories(skl-utils PUBLIC
            ${CMAKE_SOURCE_DIR}/include/math
            ${CMAKE_SOURCE_DIR}/include/shared)
endif()

target_link_libraries(skl-utils PRIVATE SKL_COMPILE_DEFINITIONS)

target_link_libraries(skl-utils PUBLIC glm::glm)

add_library(skl-utils STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/skl_math_utils.cpp
${CMAKE_CURRENT_SOURCE_DIR}/debug.cpp)

target_include_directories(skl-utils PUBLIC
        ${CMAKE_SOURCE_DIR}/include/math
        ${CMAKE_SOURCE_DIR}/include/shared)

target_link_libraries(skl-utils PRIVATE SKL_COMPILE_DEFINITIONS)

target_link_libraries(skl-utils PUBLIC glm::glm)

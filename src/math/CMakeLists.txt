add_library(skl-math STATIC ${CMAKE_CURRENT_SOURCE_DIR}/skl_math_utils.cpp)

target_include_directories(skl-math PUBLIC
        ${CMAKE_SOURCE_DIR}/include/math
        ${CMAKE_SOURCE_DIR}/include/shared)

target_link_libraries(skl-math PRIVATE COMPILE_OPTIONS)

target_link_libraries(skl-math PUBLIC glm::glm)

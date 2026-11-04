# Network code
# Exactly one backend is compiled, so that stub builds never touch WebRTC.
if(SKL_ENABLE_WEBRTC)
        add_library(network STATIC ${CMAKE_CURRENT_SOURCE_DIR}/webrtc/network_webrtc.cpp)
        target_link_libraries(network PRIVATE WEBRTC_BACKEND)
else()
        add_library(network STATIC ${CMAKE_CURRENT_SOURCE_DIR}/stub/network_stub.cpp)
endif()

target_include_directories(network PUBLIC ${CMAKE_SOURCE_DIR}/include/network)
target_link_libraries(network PUBLIC skl-utils SKL_COMPILE_DEFINITIONS)

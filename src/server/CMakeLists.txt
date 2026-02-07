add_library(http_server STATIC server.cpp)
target_include_directories(http_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(http_server PRIVATE 
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

set_target_properties(http_server PROPERTIES
    CXX_STANDARD 20
)
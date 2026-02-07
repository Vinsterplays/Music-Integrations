cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
  set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(MusicIntegrations VERSION 1.0.0)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER SOURCES EXCLUDE REGEX "src/server/.*")

include_directories("${CMAKE_SOURCE_DIR}/include/winrt")

# Set up the mod binary
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# gh actions taken from EclipseMenu
if(DEFINED ENV{GITHUB_ACTIONS})
  # Enable LTO for smaller binaries
  set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  target_compile_definitions(${PROJECT_NAME} PRIVATE GITHUB_ACTIONS)
  message(STATUS "GitHub Actions build detected")
endif()

#debug macro taken from EclipseMenu
if (NOT DEFINED ENV{GITHUB_ACTIONS})
  target_compile_definitions(
    ${PROJECT_NAME} PRIVATE
      $<$<CONFIG:Debug>:DEBUG_BUILD>
      $<$<CONFIG:RelWithDebInfo>:DEBUG_BUILD>
  )
endif()

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# Link against windowsapp.lib
add_subdirectory(src/server)

setup_geode_mod(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} windowsapp http_server)
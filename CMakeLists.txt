cmake_minimum_required(VERSION 3.10)
project(CacheLineDetection C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall")

# Source files
set(SOURCES
    "Cache Line Detection/main.c"
    "Cache Line Detection/cache.c"
    "Cache Line Detection/fast_math.c"
    "Cache Line Detection/format.c"
)

# Executable
add_executable(cacheline_detect ${SOURCES})

# Platform-specific settings
if(APPLE)
    target_compile_definitions(cacheline_detect PRIVATE PLATFORM_MACOS=1)
endif()

if(WIN32)
    target_compile_definitions(cacheline_detect PRIVATE PLATFORM_WINDOWS=1)
endif()


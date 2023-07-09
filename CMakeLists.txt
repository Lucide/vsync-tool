cmake_minimum_required(VERSION 3.25)
project(VSync C)

set(CMAKE_C_STANDARD 99)

add_library(nvapi STATIC IMPORTED)
set_target_properties(nvapi PROPERTIES
                      IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/nvapi/amd64/nvapi64.lib")
target_include_directories(nvapi
                           INTERFACE
                           "nvapi")

add_executable(vsync main.c)
set_target_properties(vsync PROPERTIES
                      OUTPUT_NAME vsync-tool
                      COMPILE_DEFINITIONS _CRT_NONSTDC_NO_WARNINGS
                      MSVC_RUNTIME_LIBRARY "$<IF:$<CONFIG:Debug>,MultiThreadedDebugDLL,MultiThreaded>")
target_compile_options(vsync PRIVATE "/W4")
target_link_libraries(vsync
                      PRIVATE
                      nvapi)


add_executable(test test.c)
set_target_properties(test PROPERTIES
                      EXCLUDE_FROM_ALL YES)
target_link_libraries(test
                      PRIVATE
                      nvapi)

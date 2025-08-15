cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
cmake_policy(VERSION 3.16)

project(web)

set(CMAKE_CXX_STANDARD 23)
if (CMAKE_BUILD_TYPE STREQUAL Debug)
    add_compile_definitions(DEBUG)
    add_compile_options(-g -O0 -fsanitize=leak)
else ()
    add_compile_options(-O3)
endif ()

set(SDLTTF_VENDORED ON)

add_subdirectory(modules/coroutine)
add_subdirectory(modules/databases)
add_subdirectory(modules/network)

add_executable(web main.cpp)
#target_link_libraries(web PUBLIC web_coroutine)
aux_source_directory(test ALL_TEST)
foreach (TEST ${ALL_TEST})
    get_filename_component(EXE_NAME ${TEST} NAME_WE)
    string(REGEX REPLACE "^.*/" "" module_name "${DIRECTORY_NAME}")
    add_executable("${EXE_NAME}" ${TEST})
    target_link_libraries("${EXE_NAME}" PUBLIC web_coroutine network databases)
endforeach ()

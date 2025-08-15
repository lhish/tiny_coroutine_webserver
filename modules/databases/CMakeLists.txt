set(LIB databases)
set(FULL_PATH_TO_MYSQL_CONNECTOR_CPP_DIR /usr/local/mysql/connector-c++-9.1.0)
aux_source_directory(src SRC)
add_library(${LIB} SHARED ${SRC})
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(STATUS "Building on macOS")
    target_include_directories(
            ${LIB}
            PUBLIC include
            ${FULL_PATH_TO_MYSQL_CONNECTOR_CPP_DIR}/include
    )
    target_link_libraries(
            ${LIB}
            ${FULL_PATH_TO_MYSQL_CONNECTOR_CPP_DIR}/lib64/debug/libmysqlcppconnx.2.9.1.0.dylib
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Building on Linux")
    target_include_directories(
            ${LIB}
            PUBLIC include
            PUBLIC /usr/local/mysql/connector-c++-9.4.0/include
    )
    target_link_libraries(
            ${LIB}
            PUBLIC /usr/local/mysql/connector-c++-9.4.0/lib64/debug/libmysqlcppconnx.so
    )
else ()
    message(FATAL_ERROR "Unsupported operating system: ${CMAKE_SYSTEM_NAME}")
endif ()
set(LIB web_coroutine)

aux_source_directory(src SRC)
add_library(${LIB} SHARED ${SRC})

target_include_directories(
        ${LIB}
        PUBLIC include
)

project(pss)

set(SOURCES 
	"src/pss.c" 
	"src/decode.c"
)

add_library(pss ${SOURCES})

add_subdirectory("ext/libmpeg2")
target_link_libraries(pss PRIVATE libmpeg2)

# macOS has POSIX fmemopen; the shim is Windows-only.
if (NOT APPLE)
	add_subdirectory("ext/libfmemopen")
	target_link_libraries(pss PRIVATE libfmemopen)
endif()

target_include_directories(pss PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ext")
target_include_directories(pss PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
set(TargetName "Job")

set(SOURCES 
	"thread-pool/include/BS_thread_pool.hpp" 
)

# Header-only: Apple's ar refuses to create an empty archive, so use an interface library there.
if (APPLE)
	add_library(${TargetName} INTERFACE)
	target_include_directories(${TargetName} INTERFACE "${CMAKE_CURRENT_LIST_DIR}/thread-pool/include")
	return()
endif()

add_library(${TargetName} ${SOURCES})

set_target_properties(${TargetName} PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(${TargetName} PUBLIC "${CMAKE_CURRENT_LIST_DIR}/thread-pool/include")
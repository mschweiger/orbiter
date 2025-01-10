Include(FetchContent)

if(NOT DEFINED ORBITER_BINARY_ROOT_DIR)
    message(FATAL_ERROR "ORBITER_BINARY_ROOT_DIR is not set")
endif(NOT DEFINED ORBITER_BINARY_ROOT_DIR)

add_subdirectory(lua)
add_subdirectory(zlib)
add_subdirectory(imgui)

## LFS
add_library(lfs SHARED luafilesystem/src/lfs.c)
install(TARGETS lfs	DESTINATION ${ORBITER_BINARY_ROOT_DIR})
install(TARGETS lfs DESTINATION ${ORBITER_INSTALL_SDK_DIR}/Utils/)
target_include_directories(lfs PUBLIC Lua)
target_link_libraries(lfs lua)
SET_TARGET_PROPERTIES(lfs PROPERTIES LINK_FLAGS "/DEF:\"${CMAKE_CURRENT_SOURCE_DIR}/luafilesystem/src/lfs.def\"")

install(TARGETS lfs	DESTINATION ${ORBITER_BINARY_ROOT_DIR})

set_target_properties(lfs PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
)

add_custom_target(CopyLDoc ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/ldoc/ ${ORBITER_BINARY_ROOT_DIR}/packages/ldoc
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Penlight/lua/ ${ORBITER_BINARY_ROOT_DIR}/
)

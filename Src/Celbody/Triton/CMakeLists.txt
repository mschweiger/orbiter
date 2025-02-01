# Copyright (c) Martin Schweiger
# Licensed under the MIT License

set(CELBODY "Triton")

add_custom_command(
	OUTPUT ${ORBITER_BINARY_CELBODY_DIR}/${CELBODY}.dll
	COMMAND ${CMAKE_COMMAND} -E make_directory ${ORBITER_BINARY_CELBODY_DIR}
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${CELBODY}.dll ${ORBITER_BINARY_CELBODY_DIR}
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${CELBODY}.dll
)

srcdir_to_tgtlist("${CMAKE_CURRENT_SOURCE_DIR}/Config" "${ORBITER_BINARY_CONFIG_DIR}" srclist tgtlist)
add_custom_command(
	OUTPUT ${tgtlist}
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Config/ ${ORBITER_BINARY_CONFIG_DIR}
	DEPENDS ${srclist}
)
add_custom_target(${CELBODY}
	DEPENDS ${tgtlist} ${ORBITER_BINARY_CELBODY_DIR}/${CELBODY}.dll
)
add_dependencies(Sol
	${CELBODY}
)

set_target_properties(${CELBODY}
	PROPERTIES
	FOLDER Celbody
)
if(NOT CMAKE_CL_64) #remove me when this celbody has source code that can be built
# Installation
install(PROGRAMS
	${CELBODY}.dll
	DESTINATION ${ORBITER_INSTALL_CELBODY_DIR}
)
install(DIRECTORY Config
	DESTINATION ${ORBITER_INSTALL_ROOT_DIR}
)
endif()
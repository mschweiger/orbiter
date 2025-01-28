# Copyright (c) Martin Schweiger
# Licensed under the MIT License

# Static library containing the DLL entry point for Orbiter plugins.
# To be included by all Orbiter plugin DLLs.

add_library(Orbitersdk STATIC
	Orbitersdk.cpp
)

set_target_properties(Orbitersdk
	PROPERTIES
	FOLDER Core
)

# Copy library to its Orbitersdk/lib location so that external project integrated into the build can find it
add_custom_command(TARGET Orbitersdk
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${ORBITER_BINARY_SDK_DIR}/lib
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Orbitersdk> ${ORBITER_BINARY_SDK_DIR}/lib/ 
)

target_link_libraries(Orbitersdk PUBLIC lua::lib)

# Installation: Orbiter addon entry interface library to SDK folder to be included by addons
install(TARGETS Orbitersdk
	ARCHIVE
	DESTINATION ${ORBITER_INSTALL_SDK_DIR}/lib
)

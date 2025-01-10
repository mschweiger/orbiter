# Copyright (c) Martin Schweiger
# Licensed under the MIT License

# Static library containing the DLL entry point for Orbiter plugins.
# To be included by all Orbiter plugin DLLs.
FetchContent_MakeAvailable(imgui)

add_library(Orbitersdk STATIC
	Orbitersdk.cpp
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_demo.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
)

set_target_properties(Orbitersdk
	PROPERTIES
	FOLDER Core
)

# Copy library to its Orbitersdk/lib location so that external project integrated into the build can find it
add_custom_command(TARGET Orbitersdk
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${ORBITER_BINARY_SDK_DIR}/lib
	COMMAND ${CMAKE_COMMAND} -E copy ${ORBITER_SDK_LIB} ${ORBITER_BINARY_SDK_DIR}/lib/ 
)

# Installation: Orbiter addon entry interface library to SDK folder to be included by addons
install(TARGETS Orbitersdk
	ARCHIVE
	DESTINATION ${ORBITER_INSTALL_SDK_DIR}/lib
)

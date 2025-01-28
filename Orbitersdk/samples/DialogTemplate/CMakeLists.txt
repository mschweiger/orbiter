# Copyright (c) Martin Schweiger
# Licensed under the MIT License

# The target directory for plugin DLLs
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ORBITER_BINARY_PLUGIN_DIR})

add_library(DialogTemplate SHARED
	DialogTemplate.cpp
	DialogTemplate.rc
)

target_include_directories(DialogTemplate
	PUBLIC ${ORBITER_SOURCE_SDK_INCLUDE_DIR}
)

target_link_libraries(DialogTemplate
	${ORBITER_LIB}
	Orbitersdk
)

add_dependencies(DialogTemplate
	${OrbiterTgt}
	Orbitersdk
)

set_target_properties(DialogTemplate
	PROPERTIES
	FOLDER SDKSamples
)

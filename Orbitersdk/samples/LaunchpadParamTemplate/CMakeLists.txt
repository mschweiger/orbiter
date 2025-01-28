# Copyright (c) Martin Schweiger
# Licensed under the MIT License

# The target directory for plugin DLLs
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ORBITER_BINARY_PLUGIN_DIR})

add_library(LaunchpadParamTemplate SHARED
	LParam.cpp
	LParam.rc
)

target_include_directories(LaunchpadParamTemplate
	PUBLIC ${ORBITER_SOURCE_SDK_INCLUDE_DIR}
)

target_link_libraries(LaunchpadParamTemplate
	${ORBITER_LIB}
	Orbitersdk
)

add_dependencies(LaunchpadParamTemplate
	${OrbiterTgt}
	Orbitersdk
)

set_target_properties(LaunchpadParamTemplate
	PROPERTIES
	FOLDER SDKSamples
)

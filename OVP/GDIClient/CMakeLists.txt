# Copyright (c) Martin Schweiger
# Licensed under the MIT License

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ORBITER_BINARY_MODULE_DIR}/Server)

add_definitions(-DGDICLIENT_EXPORTS)

add_library(GDIClient SHARED
	GDIClient.cpp
)

target_include_directories(GDIClient
	PUBLIC ${CMAKE_SOURCE_DIR}/Orbitersdk/include
)

add_dependencies(GDIClient
	${OrbiterTgt}
	Orbitersdk
)

target_link_libraries(GDIClient
	${ORBITER_LIB}
	Orbitersdk
)

# Installation
install(TARGETS
	GDIClient
	RUNTIME
	DESTINATION ${ORBITER_INSTALL_MODULE_DIR}/Server
)

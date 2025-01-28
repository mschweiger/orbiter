# Copyright (c) Martin Schweiger
# Licensed under the MIT License

find_package(ZLIB REQUIRED)

add_executable(texpack
	texpack.cpp
)

target_link_libraries(texpack
	Shlwapi.lib
	ZLIB::ZLIB
)

set_target_properties(texpack
	PROPERTIES
	FOLDER Tools
)

# Installation
install(TARGETS
	texpack
	RUNTIME
	DESTINATION ${ORBITER_INSTALL_UTILS_DIR}
)

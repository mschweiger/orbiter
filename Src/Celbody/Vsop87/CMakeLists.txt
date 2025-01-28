set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Modules)

add_library(Vsop87 SHARED
	Vsop87.cpp
)

set_target_properties(Vsop87
	PROPERTIES
	FOLDER Celbody/Vsop87
)

add_dependencies(Vsop87
	${OrbiterTgt}
)

target_include_directories(Vsop87
	PUBLIC ${CMAKE_SOURCE_DIR}/Orbitersdk/include
)

target_link_libraries(Vsop87
	${ORBITER_LIB}
	${ORBITER_SDK_LIB}
)

set(VSOP87_LIB $<TARGET_LINKER_FILE:Vsop87>)

add_subdirectory(Sun)
add_subdirectory(Mercury)
add_subdirectory(Venus)
add_subdirectory(Earth)
add_subdirectory(Mars)
add_subdirectory(Jupiter)
add_subdirectory(Saturn)
add_subdirectory(Uranus)
add_subdirectory(Neptune)

# Installation
install(TARGETS
	Vsop87
	RUNTIME
	DESTINATION ${ORBITER_INSTALL_MODULE_DIR}
)

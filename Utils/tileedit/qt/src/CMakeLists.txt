# Copyright (c) Martin Schweiger
# Licensed under the MIT License

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(CMAKE_VERSION VERSION_LESS "3.7.0")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

add_executable(tileedit
	tileedit.cpp
	tileedit.qrc
	cmap.cpp
	colorbar.cpp
	ddsread.cpp
	dlgconfig.cpp
	dlgelevconfig.cpp
	dlgelevexport.cpp
	dlgelevimport.cpp
	dlgsurfimport.cpp
	dxt_io.cpp
	elevtile.cpp
	elv_io.cpp
	imagetools.cpp
	main.cpp
	tile.cpp
	tileblock.cpp
	tilecanvas.cpp
	ZTreeMgr.cpp
	tileedit.ui
	dlgConfig.ui
	dlgElevConfig.ui
	dlgElevExport.ui
	dlgElevImport.ui
	dlgSurfImport.ui
)

add_dependencies(tileedit
	fastdxt
)

target_include_directories(tileedit PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../extern/libpng/include
	${CMAKE_CURRENT_SOURCE_DIR}/../extern/zlib/include
	${CMAKE_CURRENT_SOURCE_DIR}/../extern/fastdxt
)

target_link_libraries(tileedit
	Qt5::Widgets
	Qt5::Core
	Qt5::Gui
	ZLIB::ZLIB
	${CMAKE_CURRENT_SOURCE_DIR}/../extern/libpng/lib/libpng16_static.lib
	${FASTDXT_LIB}
)

set_target_properties(tileedit
	PROPERTIES
	WIN32_EXECUTABLE true
)

# deploy the Qt libraries
add_custom_command(TARGET tileedit
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/tmp
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:tileedit> ${CMAKE_CURRENT_BINARY_DIR}/tmp
	COMMAND ${Qt5_DIR}/../../../bin/windeployqt ${CMAKE_CURRENT_BINARY_DIR}/tmp/$<TARGET_FILE_NAME:tileedit>
)

# Installation - this is currently skipped

install(TARGETS
	tileedit
	RUNTIME
	DESTINATION ${INSTALLDIR}
)
# install all dlls that windeployqt has dropped in the build dir
file(GLOB QT_DEPLOY_DLL
	${CMAKE_CURRENT_BINARY_DIR}/tmp/*.dll
)
install(FILES ${QT_DEPLOY_DLL}
	DESTINATION ${INSTALLDIR}
)

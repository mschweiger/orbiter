# Copyright (c) Martin Schweiger
# Licensed under the MIT License
find_package(ZLIB REQUIRED)

# SAFESEH linker flag must be turned off because the DX7 libraries don't support it'
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -SAFESEH:NO")

# Sources for all Orbiter executable targets
set(common_src
# General source files
	Astro.cpp
	Camera.cpp
	cmdline.cpp
	Config.cpp
	console_ng.cpp
	Element.cpp
	elevmgr.cpp
	Help.cpp
	Input.cpp
	Keymap.cpp
	LightEmitter.cpp
	Mesh.cpp
	Nav.cpp
	Orbiter.cpp
	PlaybackEd.cpp
	Psys.cpp
	Script.cpp
	Shadow.cpp
	State.cpp
	Vecmat.cpp
	VectorMap.cpp
    ConsoleManager.cpp
	TimeData.cpp
# Launchpad
	Launchpad.cpp
	LpadTab.cpp
	TabAbout.cpp
	TabExtra.cpp
	TabModule.cpp
	TabOptions.cpp
	TabScenario.cpp
	TabVideo.cpp
# Dialogs
	CustomControls.cpp
	Dialogs.cpp
	DialogWin.cpp
	DlgCamera.cpp
	DlgCapture.cpp
	DlgFocus.cpp
	DlgFunction.cpp
	DlgHelp.cpp
	DlgInfo.cpp
	DlgMap.cpp
	DlgMenuCfg.cpp
	DlgMgr.cpp
	DlgOptions.cpp
	DlgRecorder.cpp
	DlgTacc.cpp
	htmlctrl.c
	Select.cpp
	OptionsPages.cpp
# Body classes
	Body.cpp
	BodyIntegrator.cpp
	PinesGrav.cpp
	Celbody.cpp
	Planet.cpp
	Rigidbody.cpp
	Star.cpp
# Vessel classes
	FlightRecorder.cpp
	SuperVessel.cpp
	Vessel.cpp
	Vesselbase.cpp
	Vesselstatus.cpp
# Surface base classes
	Base.cpp
	Baseobj.cpp
# Cockpit classes
	Defpanel.cpp
	hud.cpp
	MenuInfoBar.cpp
	Pane.cpp
	Panel.cpp
	Panel2D.cpp
	VCockpit.cpp
# MFD classes
	Mfd.cpp
	MfdAlign.cpp
	MfdComms.cpp
	MfdDocking.cpp
	MfdHsi.cpp
	MfdLanding.cpp
	MfdMap.cpp
	MfdMap_old.cpp
	MfdOrbit.cpp
	MfdSurface.cpp
	MfdSync.cpp
	MfdTransfer.cpp
	MfdUser.cpp
# API implementations
	CamAPI.cpp
	CelSphereAPI.cpp
	DrawAPI.cpp
	GraphicsAPI.cpp
	MFDAPI.cpp
	ModuleAPI.cpp
	OrbiterAPI.cpp
# Graphics utils
	D3d7util.cpp
	D3dmath.cpp
	Di7frame.cpp
# Graphics interface base class for GDI clients
	${GDICLIENT_DIR}/GDIClient.cpp
# Utils
	Log.cpp
	Memstat.cpp
	Util.cpp
	ZTreeMgr.cpp
# Resources
	Orbiter.rc
	Orbiter.ico
)

set(Orbiter_includes
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/Orbitersdk/include
	${CMAKE_SOURCE_DIR}/OVP
	${CMAKE_CURRENT_BINARY_DIR}
	${HTMLHELP_INCLUDE_DIR}
)

set(Orbiter_common_libs
	Winmm.dll
	comctl32.lib
	UxTheme.lib
	version.lib
	${HTML_HELP_LIBRARY}
	ZLIB::ZLIB
	lua::lib
	${HTMLHELP_LIB}
	$<TARGET_FILE:Orbitersdk>
	$<TARGET_FILE:DlgCtrl>
)
set(Orbiter_libs
	dxguid.lib
	dinput8.lib
)

set(Orbiter_depends
	Orbitersdk
	DlgCtrl
)

# Orbiter executable (graphics server version)
add_executable(Orbiter ${common_src})

set_target_properties(Orbiter
	PROPERTIES
	LINK_FLAGS "/SUBSYSTEM:CONSOLE /ENTRY:WinMainCRTStartup"
	RUNTIME_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
	OUTPUT_NAME Orbiter
	VS_DEBUGGER_WORKING_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
	COMPILE_DEFINITIONS "NOGRAPHICS;DIRECTINPUT_VERSION=0x0800"
	FOLDER Core
)

target_include_directories(Orbiter PUBLIC ${Orbiter_includes})

target_link_libraries(Orbiter ${Orbiter_common_libs} ${Orbiter_libs})

add_dependencies(Orbiter ${Orbiter_depends})

# Installation: Orbiter executable (graphics-server versions)
install(TARGETS Orbiter	RUNTIME DESTINATION ${ORBITER_INSTALL_ROOT_DIR})

# If the standard Orbiter executable is not built...
set_target_properties(Orbiter PROPERTIES ENABLE_EXPORTS 1)

# ...we have to use the NG version to export the API links
install(FILES $<TARGET_LINKER_FILE:Orbiter>	DESTINATION ${ORBITER_INSTALL_SDK_DIR}/lib)

# Copy library to its Orbitersdk/lib location so that external project integrated into the build can find it
add_custom_command(TARGET ${OrbiterTgt}	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${ORBITER_BINARY_SDK_DIR}/lib
	COMMAND ${CMAKE_COMMAND} -E copy ${ORBITER_LIB} ${ORBITER_BINARY_SDK_DIR}/lib/ 
)

string(TIMESTAMP DATE "%d %b %Y")
string(TIMESTAMP VERSION "%y%m%d")
configure_file(about.hpp.in about.hpp @ONLY)

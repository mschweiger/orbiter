# set the project name and version
project(D3D9Client VERSION 31.0)

configure_file(D3D9ClientConfig.h.in D3D9ClientConfig.h)
FetchContent_MakeAvailable(imgui)

# specify the C++ standard
# set(CMAKE_CXX_STANDARD 11)
# set(CMAKE_CXX_STANDARD_REQUIRED True)

set(DXSDK_LIB_DIR ${DXSDK_DIR}/lib/${ARCH})
set(DXSDK_LIB_DIR2 "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.19041.0/um/${ARCH}")  #Must acquire d3d9.lib from here

set(D3D9_SOURCE_DIR ${CMAKE_SOURCE_DIR}/OVP/D3D9Client)
set(ShaderDir ${D3D9_SOURCE_DIR}/shaders)
set(INCLUDE_TARGET_DIR ${ORBITER_BINARY_SDK_DIR}/include)

set(InterfaceBuilder ${D3D9_SOURCE_DIR}/samples/InterfaceBuilder/InterfaceBuilder.exe)

set(SourceFiles
	AABBUtil.cpp
	AtmoControls.cpp
	BeaconArray.cpp
	CelSphere.cpp
	CloudMgr.cpp
	Cloudmgr2.cpp
	CSphereMgr.cpp
	D3D9Client.cpp
	D3D9Config.cpp
	D3D9ControlPanel.cpp
	D3D9Effect.cpp
	D3D9Frame.cpp
	D3D9Pad.cpp
	D3D9Pad2.cpp
	D3D9Pad3.cpp
	D3D9Surface.cpp
	D3D9TextMgr.cpp
	D3D9Util.cpp
	DebugControls.cpp
	gcCore.cpp
	gcConst.cpp
	GDIPad.cpp
	HazeMgr.cpp
	IProcess.cpp
	Junction.cpp
	Log.cpp
	MaterialMgr.cpp
	Mesh.cpp
	MeshMgr.cpp
	OapiExtension.cpp
	Particle.cpp
	RingMgr.cpp
	RunwayLights.cpp
	Scene.cpp
	Spherepatch.cpp
	SurfMgr.cpp
	Surfmgr2.cpp
	TileLabel.cpp
	TileMgr.cpp
	Tilemgr2.cpp
	VBase.cpp
	VideoTab.cpp
	VObject.cpp
	VPlanet.cpp
	VPlanetAtmo.cpp
	VStar.cpp
	VVessel.cpp
	WindowMgr.cpp
	ZTreeMgr.cpp
	Tilemgr2_imp.hpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_dx9.cpp
)

set(IncludeFiles
	AABBUtil.h
	AtmoControls.h
	BeaconArray.h
	CelSphere.h
	CloudMgr.h
	Cloudmgr2.h
	CSphereMgr.h
	D3D9Catalog.h
	D3D9Client.h
	D3D9Config.h
	D3D9Effect.h
	D3D9Frame.h
	D3D9Pad.h
	D3D9Surface.h
	D3D9TextMgr.h
	D3D9Util.h
	DebugControls.h
	GDIPad.h
	HazeMgr.h
	IProcess.h
	Junction.h
	Log.h
	MaterialMgr.h
	Mesh.h
	MeshMgr.h
	OapiExtension.h
	Particle.h
	Qtree.h
	resource.h
	RingMgr.h
	RunwayLights.h
	Scene.h
	Spherepatch.h
	SurfMgr.h
	Surfmgr2.h
	TileLabel.h
	TileMgr.h
	Tilemgr2.h
	VBase.h
	VectorHelpers.h
	VideoTab.h
	VObject.h
	VPlanet.h
	VStar.h
	VVessel.h
	WindowMgr.h
	ZTreeMgr.h
	gcConst.h
	gcCore.h
)

set(APIHeaders
    ${INCLUDE_TARGET_DIR}/gcGUI.h
	${INCLUDE_TARGET_DIR}/gcCoreAPI.h
)

set(ShaderFiles
	${ShaderDir}/BeaconArray.fx
	${ShaderDir}/Common.hlsl
	${ShaderDir}/D3D9Client.fx
	${ShaderDir}/EnvMapBlur.hlsl
	${ShaderDir}/GDIOverlay.hlsl
	${ShaderDir}/HorizonHaze.fx
	${ShaderDir}/IPI.hlsl
	${ShaderDir}/IrradianceInteg.hlsl
	${ShaderDir}/LensFlare.hlsl
	${ShaderDir}/LightBlur.hlsl
    ${ShaderDir}/CelSphere.hlsl
    ${ShaderDir}/NewMesh.hlsl
    ${ShaderDir}/NewPlanet.hlsl
    ${ShaderDir}/Scatter.hlsl
    ${ShaderDir}/Glare.hlsl
	${ShaderDir}/Mesh.fx
	${ShaderDir}/Metalness.fx
	${ShaderDir}/Particle.fx
	${ShaderDir}/PBR.fx
	${ShaderDir}/Planet.fx
	${ShaderDir}/SceneTech.fx
	${ShaderDir}/Sketchpad.fx
	${ShaderDir}/Vessel.fx
)

source_group(APIHeaders FILES ${APIHeaders})

source_group(Shaders FILES ${ShaderFiles})

set_property(SOURCE ${ShaderFiles}
    PROPERTY VS_SETTINGS "ExcludedFromBuild=true"
)

add_library(D3D9Client MODULE
	${SourceFiles}
	${IncludeFiles}
    ${APIHeaders}
	${ShaderFiles}
	D3D9Client.rc
)

target_include_directories(D3D9Client PUBLIC
	${ORBITER_SOURCE_SDK_INCLUDE_DIR}
	${DXSDK_DIR}/Include
	${imgui_SOURCE_DIR}/
	${imgui_SOURCE_DIR}/backends/
)

target_link_directories(D3D9Client PUBLIC
	${ORBITER_BINARY_SDK_DIR}/lib
	${DXSDK_LIB_DIR2}
	${DXSDK_LIB_DIR}
)

target_link_libraries(D3D9Client
	orbiter.lib
	orbitersdk.lib
	d3d9.lib
	debug d3dx9d.lib
	optimized d3dx9.lib
	kernel32.lib
	user32.lib
	gdi32.lib
	winspool.lib
	comdlg32.lib
	advapi32.lib
	shell32.lib
	ole32.lib
	oleaut32.lib
	uuid.lib
	odbc32.lib
	odbccp32.lib
	version.lib
	msimg32.lib
)

set_target_properties(D3D9Client
	PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Modules/Plugin
	LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Modules/Plugin
	COMPILE_DEFINITIONS D3D9CLIENT_EXPORTS
	FOLDER OVP
)

add_custom_command(
	TARGET D3D9Client PRE_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${D3D9_SOURCE_DIR}/shaders/ ${ORBITER_BINARY_MODULE_DIR}/D3D9Client
)

add_custom_command(OUTPUT ${INCLUDE_TARGET_DIR}/gcCoreAPI.h ${INCLUDE_TARGET_DIR}/gcGUI.h
	DEPENDS ${D3D9_SOURCE_DIR}/gcGUI.h ${D3D9_SOURCE_DIR}/gcCore.h ${D3D9_SOURCE_DIR}/gcCore.cpp
	COMMAND ${InterfaceBuilder} ${D3D9_SOURCE_DIR}/gcCore.h ${INCLUDE_TARGET_DIR}/gcCoreAPI.h ${D3D9_SOURCE_DIR}/gcCore.cpp
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${D3D9_SOURCE_DIR}/gcGUI.h ${INCLUDE_TARGET_DIR}/gcGUI.h
	VERBATIM
)
add_custom_target(D3D9Client_Interface
	DEPENDS ${INCLUDE_TARGET_DIR}/gcCoreAPI.h ${INCLUDE_TARGET_DIR}/gcGUI.h
)

add_dependencies(D3D9Client
	${OrbiterTgt}
	Orbitersdk
	D3D9Client_Interface
)

install(TARGETS D3D9Client
	LIBRARY	
	DESTINATION ${ORBITER_INSTALL_PLUGIN_DIR}
)

add_subdirectory(samples/DrawOrbits)
add_subdirectory(samples/DX9ExtMFD)
add_subdirectory(samples/GenericCamera)
add_subdirectory(samples/TerrainToolKit)

set(Exclude
	PATTERN CMakeLists.txt EXCLUDE
)

install(DIRECTORY ${D3D9_SOURCE_DIR}/shaders/ DESTINATION ${ORBITER_INSTALL_ROOT_DIR}/Modules/D3D9Client)
install(DIRECTORY ${D3D9_SOURCE_DIR}/samples/DrawOrbits/ DESTINATION ${ORBITER_INSTALL_ROOT_DIR}/Orbitersdk/samples/DrawOrbits ${Exclude})
install(DIRECTORY ${D3D9_SOURCE_DIR}/samples/DX9ExtMFD/ DESTINATION ${ORBITER_INSTALL_ROOT_DIR}/Orbitersdk/samples/DX9ExtMFD ${Exclude})
install(DIRECTORY ${D3D9_SOURCE_DIR}/samples/GenericCamera/ DESTINATION ${ORBITER_INSTALL_ROOT_DIR}/Orbitersdk/samples/GenericCamera ${Exclude})
install(FILES ${INCLUDE_TARGET_DIR}/gcCoreAPI.h DESTINATION ${ORBITER_INSTALL_ROOT_DIR}/Orbitersdk/include)
install(FILES ${INCLUDE_TARGET_DIR}/gcGUI.h DESTINATION ${ORBITER_INSTALL_ROOT_DIR}/Orbitersdk/include)

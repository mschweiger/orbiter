# Copyright (c) Martin Schweiger
# Licensed under the MIT License

cmake_minimum_required(VERSION 3.19)

# Set the project name
project (Orbiter VERSION 21.7.24)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

#Add /permissive if using C++20 or higher
#add_compile_options(/permissive)

# Some functions to simplify debugging CMake scripts
include(CMakePrintHelpers)

# Support for including runtime libraries in packages
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
include(InstallRequiredSystemLibraries)

# Set the pools for compilers that can't run concurrently
set_property(GLOBAL PROPERTY JOB_POOLS latex=1 soffice=1 htmlhelp=1)

# Group projects in VS
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Locally defined cmake modules
set(CMAKE_MODULE_PATH
	${CMAKE_SOURCE_DIR}/cmake
	${CMAKE_MODULE_PATH}
)

if(MSVC AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(BUILD64 0)
	set(ARCH "x86")
  add_link_options("/LARGEADDRESSAWARE")
else()
	set(BUILD64 1)
	set(ARCH "x64")
endif()

# This is important for building using vs generator (jarmonik)
if (NOT ${CMAKE_GENERATOR} STREQUAL "Ninja")
	add_compile_options(/MP)
endif()

# Define directories and file paths for some common binaries
set(OVP_DIR "${CMAKE_SOURCE_DIR}/OVP")
set(GDICLIENT_DIR "${OVP_DIR}/GDIClient")

set(ORBITER_SOURCE_ROOT_DIR ${CMAKE_SOURCE_DIR})
set(ORBITER_SOURCE_DIR ${ORBITER_SOURCE_ROOT_DIR}/Src/Orbiter)
set(ORBITER_SOURCE_MODULE_DIR ${ORBITER_SOURCE_ROOT_DIR}/Src/Plugin)
set(ORBITER_SOURCE_SDK_DIR ${ORBITER_SOURCE_ROOT_DIR}/Orbitersdk)
set(ORBITER_SOURCE_SDK_INCLUDE_DIR ${ORBITER_SOURCE_SDK_DIR}/include)
set(ORBITER_SOURCE_HTML_DIR ${ORBITER_SOURCE_ROOT_DIR}/Html)

set(ORBITER_BINARY_ROOT_DIR ${CMAKE_BINARY_DIR})
set(ORBITER_BINARY_CONFIG_DIR ${ORBITER_BINARY_ROOT_DIR}/Config)
set(ORBITER_BINARY_MESH_DIR ${ORBITER_BINARY_ROOT_DIR}/Meshes)
set(ORBITER_BINARY_MODULE_DIR ${ORBITER_BINARY_ROOT_DIR}/Modules)
set(ORBITER_BINARY_STARTUP_DIR ${ORBITER_BINARY_MODULE_DIR}/Startup)
set(ORBITER_BINARY_CELBODY_DIR ${ORBITER_BINARY_MODULE_DIR}/Celbody)
set(ORBITER_BINARY_PLUGIN_DIR ${ORBITER_BINARY_MODULE_DIR}/Plugin)
set(ORBITER_BINARY_VESSEL_DIR ${ORBITER_BINARY_MODULE_DIR})
set(ORBITER_BINARY_TEXTURES_DIR ${ORBITER_BINARY_ROOT_DIR}/Textures)
set(ORBITER_BINARY_TEXTURES2_DIR ${ORBITER_BINARY_ROOT_DIR}/Textures2)
set(ORBITER_BINARY_HTML_DIR ${ORBITER_BINARY_ROOT_DIR}/Html)
set(ORBITER_BINARY_DOC_DIR ${ORBITER_BINARY_ROOT_DIR}/Doc)
set(ORBITER_BINARY_SDK_DIR ${ORBITER_BINARY_ROOT_DIR}/Orbitersdk)

set(ORBITER_INSTALL_ROOT_DIR "Orbiter") # This path is relative to CMAKE_INSTALL_PREFIX
set(ORBITER_INSTALL_MESH_DIR ${ORBITER_INSTALL_ROOT_DIR}/Meshes)
set(ORBITER_INSTALL_MODULE_DIR ${ORBITER_INSTALL_ROOT_DIR}/Modules)
set(ORBITER_INSTALL_CELBODY_DIR ${ORBITER_INSTALL_MODULE_DIR}/Celbody)
set(ORBITER_INSTALL_PLUGIN_DIR ${ORBITER_INSTALL_MODULE_DIR}/Plugin)
set(ORBITER_INSTALL_TEXTURES_DIR ${ORBITER_INSTALL_ROOT_DIR}/Textures)
set(ORBITER_INSTALL_TEXTURES2_DIR ${ORBITER_INSTALL_ROOT_DIR}/Textures2)
set(ORBITER_INSTALL_HTML_DIR  ${ORBITER_INSTALL_ROOT_DIR}/Html)
set(ORBITER_INSTALL_DOC_DIR ${ORBITER_INSTALL_ROOT_DIR}/Doc)
set(ORBITER_INSTALL_UTILS_DIR ${ORBITER_INSTALL_ROOT_DIR}/Utils)
set(ORBITER_INSTALL_SDK_DIR ${ORBITER_INSTALL_ROOT_DIR}/Orbitersdk)

set(ORBITER_SDK_LIB Orbitersdk)
set(ORBITER_DLGCTRL_LIB $<TARGET_FILE:DlgCtrl>)
set(LUAINTERPRETER_LIB $<TARGET_LINKER_FILE:LuaInterpreter>)
set(GDICLIENT_LIB $<TARGET_LINKER_FILE:GDIClient>)

set(EXTERN_DIR "${CMAKE_SOURCE_DIR}/Extern")

set(HTMLHELP_DIR "${EXTERN_DIR}/Htmlhelp")
set(HTMLHELP_INCLUDE_DIR ${HTMLHELP_DIR}/include)
set(HTMLHELP_LIB_DIR ${HTMLHELP_DIR}/lib-${ARCH})
set(HTMLHELP_LIB ${HTMLHELP_LIB_DIR}/Htmlhelp.Lib)

set(ORBITER_TOOL_MESHC "$<TARGET_FILE:meshc>")
set(ldoc ${ORBITER_BINARY_ROOT_DIR}/lua.exe ${ORBITER_BINARY_ROOT_DIR}/packages/LDoc/ldoc.lua -v --multimodule)

find_package(OpenGL QUIET)
find_package(HTMLHelp)
find_package(LATEX)		
find_package(MFC)

# We don't query Qt with find_package because we need the 64-bit version here
set(Qt5_x64_DIR
	"Qt5_x64_DIR_NOTFOUND"
	CACHE
	PATH
	"Qt5-x64 installation path."
)

if(MSVC)
  add_compile_options(/we4311 /DNOMINMAX)
  add_link_options("/NODEFAULTLIB:\"LIBCMTD\"")
else()
  set(CMAKE_BUILD_WITH_INSTALL_RPATH true)
endif()

# Copy data and binary assets from source to binary tree to provide a working Orbiter environment
add_custom_target(CopyData ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/BinAssets/ ${CMAKE_BINARY_DIR}
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Scenarios/ ${CMAKE_BINARY_DIR}/Scenarios
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Textures/ ${ORBITER_BINARY_TEXTURES_DIR}
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Meshes/ ${CMAKE_BINARY_DIR}/Meshes
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Script/ ${CMAKE_BINARY_DIR}/Script
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Config/ ${CMAKE_BINARY_DIR}/Config
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/GravityModels/ ${CMAKE_BINARY_DIR}/GravityModels
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Flights/ ${CMAKE_BINARY_DIR}/Flights
	COMMAND ${CMAKE_COMMAND} -E make_directory ${ORBITER_BINARY_SDK_DIR}/lib/Lua
)
set_target_properties(CopyData
	PROPERTIES
	FOLDER Data
)

# =====================================================================================
# work around Html Help compiler bug returning error code

set(HHC_CMD ${CMAKE_CURRENT_BINARY_DIR}/cmake/hhc_fix.bat)
file(TO_NATIVE_PATH "${HTML_HELP_COMPILER}" HHC_PATH_W)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/hhc_fix.bat.in ${HHC_CMD})


# =====================================================================================
# work around pdftex returning an error because of doxygen bug

set(PDFTEX_CMD ${CMAKE_CURRENT_BINARY_DIR}/cmake/pdftex_fix.bat)
file(TO_NATIVE_PATH "${PDFLATEX_COMPILER}" PDFTEX_PATH_W)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/pdftex_fix.bat.in ${PDFTEX_CMD})


# =====================================================================================

set(OrbiterTgt Orbiter)
set(ORBITER_LIB $<TARGET_LINKER_FILE:Orbiter>)

# =====================================================================================
# Configure Orbiter.cfg for externally installed planet textures

set(ORBITER_PLANET_TEXTURE_INSTALL_DIR
	${ORBITER_BINARY_ROOT_DIR}/Textures
	CACHE
	PATH
	"Planet texture installation directory (e.g. C:/Orbiter2016/Textures)"
)

if(DEFINED ENV{ORBITER_PLANET_TEXTURE_INSTALL_DIR})
  set(ORBITER_PLANET_TEXTURE_INSTALL_DIR $ENV{ORBITER_PLANET_TEXTURE_INSTALL_DIR})
  message(STATUS "Using environment: ORBITER_PLANET_TEXTURE_INSTALL_DIR=${ORBITER_PLANET_TEXTURE_INSTALL_DIR}")
endif()

string(COMPARE NOTEQUAL ${ORBITER_PLANET_TEXTURE_INSTALL_DIR} "${ORBITER_BINARY_ROOT_DIR}/Textures" CUSTOM_PLANET_TEXTURE_DIR)

if (${CUSTOM_PLANET_TEXTURE_DIR})
	string(REPLACE  "/" "\\" ORBITER_PLANET_TEXTURE_INSTALL_DIR_W "${ORBITER_PLANET_TEXTURE_INSTALL_DIR}")

	configure_file(${ORBITER_SOURCE_DIR}/Orbiter.cfg.in ${ORBITER_BINARY_ROOT_DIR}/Orbiter.cfg)
	install(FILES ${ORBITER_BINARY_ROOT_DIR}/Orbiter.cfg DESTINATION ${ORBITER_INSTALL_ROOT_DIR})
endif()


# =====================================================================================
# Find required and optional build tools

option(
	ORBITER_BUILD_D3D9CLIENT
	"Build the D3D9 graphics client"
	ON
)

option(
	ORBITER_BUILD_XRSOUND
	"Build XRSound module to enable sound in Orbiter"
	ON
)

option(ORBITER_MAKE_DOC
	"Create documentation"
	OFF
)

option(ORBITER_MAKE_TESTS
	"Build unit tests"
	ON
)

option(ORBITER_SANITIZER
	"Build binaries with Address Sanitizer"
	OFF
)

if(ORBITER_BUILD_XRSOUND)
	set(IRRKLANG_DIR "" CACHE PATH "Path to the irrKlang library")
endif()

if(ORBITER_BUILD_D3D9CLIENT)
	find_package(DXSDK REQUIRED)
	set(DXSDK_DIR
		${DXSDK_DIR}
		CACHE
		PATH
		"Installation directory of DirectX SDK (June 2010)"
	)
	if(${DXSDK_DIR} STREQUAL "DXSDK_DIR-NOTFOUND")
		message(FATAL_ERROR "DirectX SDK not found. The D3D9 graphics client will not be built.")
	endif()
endif()

if (ORBITER_MAKE_DOC)
	find_package(Doxygen REQUIRED)
endif()

# Enable Address SANitizer
include(cmake/sanitizer.cmake)
if (ORBITER_SANITIZER AND MSVC)
	enable_sanitizer(${ORBITER_SANITIZER})
endif()


# Given a source directory (srcdir) and a target root directory (tgtroot),
# generate a list of all files found in srcdir (srclist) and a list of output files
# when the source directory is copied to the target root (tgtlist)
# For a copy_directory command, srclist can be used as dependencies, and tgtlist as output

function(srcdir_to_tgtlist srcdir tgtroot srclist tgtlist)
	file(GLOB_RECURSE infiles
		RELATIVE ${srcdir}
		${srcdir}/*
	)
	set(local_srclist ${infiles})
	list(TRANSFORM local_srclist PREPEND "${srcdir}/")
	set(local_tgtlist ${infiles})
	list(TRANSFORM local_tgtlist PREPEND "${tgtroot}/")
	set(srclist ${local_srclist} PARENT_SCOPE)
	set(tgtlist ${local_tgtlist} PARENT_SCOPE)
endfunction()


# Installation
install(DIRECTORY Scenarios Textures Meshes Config GravityModels Script Flights BinAssets/
	DESTINATION ${ORBITER_INSTALL_ROOT_DIR}
)

set(CMAKE_MODULE_PATH
	${ORBITER_BINARY_ROOT_DIR}
    ${CMAKE_MODULE_PATH}
)

# Package required runtime libraries
INSTALL(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
	DESTINATION ${ORBITER_INSTALL_ROOT_DIR}
	COMPONENT RuntimeLibraries
)

# readme and license
INSTALL(FILES readme.txt
	DESTINATION ${ORBITER_INSTALL_ROOT_DIR}
)
INSTALL(FILES LICENSE
	DESTINATION ${ORBITER_INSTALL_ROOT_DIR}
)
#COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/Doc/TransX\ readme.txt ${ORBITER_BINARY_DOC_DIR}/TransX/TransX\ readme.txt

# Package dependencies
# See vcpkg.json
set(INSTALL_TOOLS ON)
find_package(Lua REQUIRED)
find_package(ZLIB REQUIRED)
find_package(unofficial-luafilesystem CONFIG REQUIRED)

# Aliases and convenience additions
add_library(lua::lfs ALIAS unofficial::luafilesystem::lfs)
add_library(lua::lib ALIAS lua)
add_executable(luai IMPORTED)
set_target_properties(luai PROPERTIES IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/tools/lua/lua${CMAKE_EXECUTABLE_SUFFIX})
add_executable(lua::exe ALIAS luai)

# Sub-projects
add_subdirectory(Extern)
add_subdirectory(Src)
add_subdirectory(Utils)
add_subdirectory(Orbitersdk)
add_subdirectory(OVP)
add_subdirectory(Sound)
add_subdirectory(Html)

if(ORBITER_MAKE_DOC)
	add_subdirectory(Doc)
endif()

# =============================================================================
# Set up Distribution package options

configure_file(${CMAKE_SOURCE_DIR}/cmake/CPackOptions.cmake.in
	${CMAKE_BINARY_DIR}/OrbiterCPackOptions.cmake)

set(CPACK_GENERATOR "WIX;ZIP")
set(CPACK_PACKAGE_NAME OpenOrbiter)
set(CPACK_PACKAGE_VENDOR Orbitersim)
set(CPACK_PACKAGE_DESCRIPTION "Orbiter is an Open-source spaceflight simulator for Windows PCs.")
set(CPACK_PACKAGE_HOMEPAGE_URL http://orbit.medphys.ucl.ac.uk)
set(CPACK_PACKAGE_EXECUTABLES Orbiter;OpenOrbiter)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_COMPONENT_Unspecified_DISPLAY_NAME "Orbiter Core Components")
set(CPACK_COMPONENT_Unspecified_REQUIRED TRUE)
set(CPACK_COMPONENT_RuntimeLibraries_DESCRIPTION "System libraries used by Orbiter. Install if Orbiter fails to launch.")
set(CPACK_RESOURCE_FILE_LICENSE ${ORBITER_SOURCE_ROOT_DIR}/cmake/LICENSE.rtf)
set(CPACK_RESOURCE_FILE_README ${ORBITER_SOURCE_ROOT_DIR}/README.md)
set(CPACK_WIX_UI_BANNER ${ORBITER_SOURCE_ROOT_DIR}/cmake/install_banner.bmp)
set(CPACK_WIX_UI_DIALOG ${ORBITER_SOURCE_ROOT_DIR}/cmake/install_bkg.bmp)
set(CPACK_WIX_PRODUCT_ICON ${ORBITER_SOURCE_ROOT_DIR}/cmake/orbiter_install.ico)
set(CPACK_WIX_UPGRADE_GUID AB3F6483-A1A4-4ECD-ADAE-D33DF3A5AADA)
set(CPACK_WIX_COMPONENT_INSTALL)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/OrbiterCPackOptions.cmake")

include(OrbiterCPackOptions)
include(CPack)

cpack_add_component(Unspecified
	DISPLAY_NAME "OrbiterCore"
	DESCRIPTION "Orbiter Core Components"
	REQUIRED
)
cpack_add_component(RuntimeLibraries
	DISPLAY_NAME "RuntimeLibraries"
	DESCRIPTION "System libraries used by Orbiter. Install if Orbiter fails to launch."
)

# There appears to be a CMake (3.20.5) bug that prevents CMAKE_INSTALL_PREFIX to be set
# correctly in the CPACK_PRE_BUILD_SCRIPTS, so the script has been disabled for now
# This means that the planet texture installation directory is not reset to default
# for the packages. This needs to be fixed

#set(CPACK_PRE_BUILD_SCRIPTS ${ORBITER_SOURCE_ROOT_DIR}/cmake/cpack_install.cmake)

if (ORBITER_MAKE_TESTS)
      enable_testing()
      add_subdirectory(Tests)
endif()

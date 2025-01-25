project(imgui)

Include(FetchContent)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.7-docking
  PATCH_COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/imconfig.h .
  UPDATE_DISCONNECTED 1
)
FetchContent_MakeAvailable(imgui)

install(FILES ${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf
	DESTINATION ${ORBITER_INSTALL_ROOT_DIR}
)

install(FILES ${imgui_SOURCE_DIR}/imgui.h ${imgui_SOURCE_DIR}/imconfig.h
	DESTINATION ${ORBITER_INSTALL_SDK_DIR}/include
)

add_library(imgui INTERFACE)

set(IMGUI_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
file(COPY ${imgui_SOURCE_DIR}/imgui.h ${imgui_SOURCE_DIR}/imconfig.h DESTINATION ${IMGUI_INCLUDE_DIR})
target_include_directories(imgui INTERFACE ${IMGUI_INCLUDE_DIR})

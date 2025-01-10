project(imgui)

Include(FetchContent)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.6-docking
  PATCH_COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/imconfig.h .
  UPDATE_DISCONNECTED 1
)
FetchContent_MakeAvailable(imgui)

install(FILES ${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf
	DESTINATION ${ORBITER_INSTALL_ROOT_DIR}
)

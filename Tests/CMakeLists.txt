find_package(Catch2 REQUIRED)

# Utility function
function(add_test_file test_name)
	add_executable(${test_name} "${test_name}.cpp")

	set_target_properties( ${test_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${ORBITER_BINARY_ROOT_DIR}" )

	target_include_directories(${test_name}
		PRIVATE ${ORBITER_SOURCE_SDK_INCLUDE_DIR}
		PRIVATE ${MODULE_COMMON_DIR}
		PRIVATE ${ORBITER_SOURCE_ROOT_DIR}/Src/Module/LuaScript/LuaInterpreter
	)

	target_link_libraries(${test_name}
		${ORBITER_LIB}
		Orbitersdk
		lua::lib
		${LUAINTERPRETER_LIB}
		Catch2::Catch2WithMain
	)

	add_dependencies(${test_name}
		${OrbiterTgt}
		Orbitersdk
		LuaInterpreter
	)

	add_test( 
		NAME ${test_name} 
		COMMAND $<TARGET_FILE:${test_name}>
		WORKING_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
	)
	set_tests_properties(${test_name} PROPERTIES TIMEOUT 30)
endfunction()

# Register unit tests
add_test_file(Lua.Interpreter)

if (BUILD_ORBITER_SERVER)

	# Sanity check for scenario tests
	add_test(
		NAME "Scenario.SanityCheck"
		COMMAND $<TARGET_FILE:Orbiter_server> "--scenariox=${CMAKE_SOURCE_DIR}/Scenarios/Delta-glider/Smack!.scn" "--maxframes=60"
		WORKING_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
	)
	set_tests_properties(Scenario.SanityCheck PROPERTIES TIMEOUT 60)

	# Register scenario tests
	file(GLOB TestScenarios "${CMAKE_SOURCE_DIR}/Scenarios/Tests/*.scn")
	foreach(Scenario ${TestScenarios})
		get_filename_component(test_name ${Scenario} NAME_WE)
		add_test(
			NAME "Scenario.${test_name}"
			COMMAND $<TARGET_FILE:Orbiter_server> "--scenariox=${Scenario}"
			WORKING_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
		)
		set_tests_properties(Scenario.${test_name} PROPERTIES TIMEOUT 60)
	endforeach()

endif()

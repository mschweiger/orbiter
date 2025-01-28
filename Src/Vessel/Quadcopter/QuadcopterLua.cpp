// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ==============================================================
//                ORBITER MODULE: Quadcopter
//                  Part of the ORBITER SDK
//
// QuadcopterLua.cpp
// Implementation of Quadcopter-specific Lua interface methods
// ==============================================================

#include "QuadcopterLua.h"
#include "Quadcopter.h"
#include "PropulsionSubsys.h"

int LuaInterface::InitInterpreter(void *context)
{
	lua_State *L = (lua_State*)context;

	// perform any initialisations here

	return 0;
}

int LuaInterface::InitInstance(void *context)
{
	lua_State *L = (lua_State*)context;

	// check if interpreter has QC table loaded already
	luaL_getmetatable(L, "VESSEL.QC");

	if (lua_isnil(L, -1)) { // register new functions
		lua_pop(L, 1);
		static const struct luaL_Reg qcLib[] = {
			{ "set_directmode", setDirectMode},
			{ "set_autoheading", setAutoHeading},
			{ "set_heading", setHeading},
			{ "set_course", setCourse},
			{ "set_hspd", setHspd},
			{ "set_vspd", setVspd},
			{ "set_alt", setAlt},
			{ NULL, NULL }
		};

		// create metatable for vessel userdata
		luaL_newmetatable(L, "QC.vtable");

		// create a new anonymous table for the overloaded methods
		luaL_newlib(L, qcLib);

		// create metatable for accessing inherited methods from VESSEL
		luaL_newmetatable(L, "QC.base");
		lua_pushstring(L, "__index");
		luaL_getmetatable(L, "VESSEL.vtable");
		lua_settable(L, -3);

		// set DG.base as metatable for DG.method
		lua_setmetatable(L, -2);

		// point vessel userdata to DG.method
		lua_pushstring(L, "__index");
		lua_pushvalue(L, -2); // push DG.method
		lua_settable(L, -4);

		// pop DG.method from the stack
		lua_pop(L, 1);
	}

	lua_setmetatable(L, -2);

	return 0;
}

// ==========================================================================
// Quadcopter Lua API extensions

/***
Quadcopter Lua API extensions

In the following list of methods, 'Quadcopter' represents an instance of a vessel of class Quadcopter.
Using these methods for other vessel types will generally result in an error.
To check the class of a vessel object, use the vessel:get_classname() method.
@classmod Quadcopter
*/

Quadcopter *LuaInterface::lua_toQC(lua_State *L, int idx)
{
	VESSEL **pv = (VESSEL **)luaL_checkudata(L, idx, "QC.vtable");
	Quadcopter *qc = (Quadcopter*)*pv;
	return qc;
}

/***
Enable or disable the quadcopters direct steering mode.
@function set_directmode
@tparam bool action Whether to set direct mode.
*/
int LuaInterface::setDirectMode(lua_State *L)
{
	Quadcopter *qc = lua_toQC(L, 1);
	int action = lua_tointeger(L, 2);
	qc->ssysPropulsion()->setDirectMode(action);

	return 0;
}

/***
Enable or disable the quadcopters auto heading mode.
@function set_autoheading
@tparam bool action Whether to set auto heading.
*/
int LuaInterface::setAutoHeading(lua_State *L)
{
	Quadcopter *qc = lua_toQC(L, 1);
	int action = lua_tointeger(L, 2);
	qc->ssysPropulsion()->setAutoHeading(action);

	return 0;
}

/***
Set or unset the quadcopters heading.
@function set_heading
@tparam number heading The desired heading [**deg**].
*/
int LuaInterface::setHeading(lua_State *L)
{
	Quadcopter *qc = lua_toQC(L, 1);
	double hdg = lua_tonumber(L, 2) * RAD;
	qc->ssysPropulsion()->setHeadingCmd(hdg);

	return 0;
}

/***
Set or unset the quadcopters course.
@function set_course
@tparam number|nil course The desired course [**deg**].  
- __nil__ to unset the course command mode.
*/
int LuaInterface::setCourse(lua_State *L)
{
	Quadcopter *qc = lua_toQC(L, 1);
	if (lua_isnil(L, 2)) {
		qc->ssysPropulsion()->unsetCourseCmd();
	}
	else {
		double crs = lua_tonumber(L, 2) * RAD;
		double spd = lua_tonumber(L, 3);
		qc->ssysPropulsion()->setCourseCmd(crs, spd);
	}
	return 0;
}

/***
Set or unset the quadcopters horizontal speed.
@function set_hspd
@tparam number|nil hspd The desired horizontal speed [**m/s**].  
- __nil__ to unset the horizontal speed command mode.
*/
int LuaInterface::setHspd(lua_State *L)
{
	Quadcopter *qc = lua_toQC(L, 1);
	if (lua_isnil(L, 2)) {
		qc->ssysPropulsion()->unsetHspdCmd();
	}
	else {
		double hspd = lua_tonumber(L, 2);
		qc->ssysPropulsion()->setHspdCmd(hspd);
	}
	return 0;
}

/***
Set or unset the quadcopters vertical speed.
@function set_vspd
@tparam number|nil vspd The desired vertical speed [**m/s**].  
- __nil__ to unset the vertical speed command mode.
*/
int LuaInterface::setVspd(lua_State *L)
{
	Quadcopter *qc = lua_toQC(L, 1);
	if (lua_isnil(L, 2)) {
		qc->ssysPropulsion()->unsetVspdCmd();
	}
	else {
		double vspd = lua_tonumber(L, 2);
		qc->ssysPropulsion()->setVspdCmd(vspd);
	}
	return 0;
}

/***
Set or unset the quadcopters altitude.
@function set_alt
@tparam number|nil alt The desired altitude [**m**].  
- __nil__ to unset the altitude command mode.
*/
int LuaInterface::setAlt(lua_State *L)
{
	Quadcopter *qc = lua_toQC(L, 1);
	if (lua_isnil(L, 2)) {
		qc->ssysPropulsion()->unsetAltCmd();
	}
	else {
		double alt = lua_tonumber(L, 2);
		qc->ssysPropulsion()->setAltCmd(alt);
	}
	return 0;
}

// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ==============================================================
//                    ORBITER MODULE: HST
//                  Part of the ORBITER SDK
//
// HST_Lua.cpp
// Script extensions to HST module
// ==============================================================

#include "HST.h"

#ifdef SCRIPTSUPPORT

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

// ==========================================================================
// API function prototypes

HST *lua_toHST (lua_State *L, int idx = 1);
int hstAntenna (lua_State *L);
int hstHatch (lua_State *L);
int hstArray (lua_State *L);

// ==========================================================================
// API initialisation

int HST::Lua_InitInterpreter(void *context)
{
	lua_State *L = (lua_State*)context;

	// add interpreter initialisation here

	return 0;
}

int HST::Lua_InitInstance(void *context)
{
	lua_State *L = (lua_State*)context;

	// check if interpreter has HST table loaded already
	luaL_getmetatable (L, "VESSEL.HST");

	if (lua_isnil (L, -1)) { // register new functions
		lua_pop (L, 1);
		static const struct luaL_Reg hstLib[] = {
			{"antenna", hstAntenna},
			{"hatch", hstHatch},
			{"array", hstArray},
			{NULL, NULL}
		};

		// create metatable for vessel userdata
		luaL_newmetatable (L, "HST.vtable");

		// create a table for the overloaded methods
		luaL_newlib(L, hstLib);
		lua_setglobal(L, "HST.method");
		lua_getglobal(L, "HST.method");

		// create metatable for accessing inherited methods from VESSEL
		luaL_newmetatable (L, "HST.base");
		lua_pushstring (L, "__index");
		luaL_getmetatable (L, "VESSEL.vtable");
		lua_settable (L, -3);

		// set HST.base as metatable for HST.method
		lua_setmetatable (L, -2);

		// point vessel userdata to HST.method
		lua_pushstring (L, "__index");
		lua_pushvalue (L, -2);
		lua_settable (L, -4);

		// pop HST.method from the stack
		lua_pop (L, 1);
	}

	lua_setmetatable (L, -2);

	return 0;
}

// ==========================================================================
// Script API functions

/***
Hubble Space Telescope (HST) Lua API extensions

In the following list of methods, 'HST' represents an instance of a vessel of class HST.
Using these methods for other vessel types will generally result in an error.
To check the class of a vessel object, use the vessel:get_classname() method.
@classmod HST
*/

HST *lua_toHST (lua_State *L, int idx)
{
	VESSEL **pv = (VESSEL **)luaL_checkudata(L, idx, "HST.vtable");
	HST *hst = (HST*)*pv;
	return hst;
}

static HST::DoorStatus HSTaction[2] = {
	HST::DOOR_CLOSING,
	HST::DOOR_OPENING
};

/***
Activate the HST antenna.
@function antenna
@tparam bool action Whether to activate the antenna.
*/
static int hstAntenna (lua_State *L)
{
	HST *hst = lua_toHST (L, 1);
	int action = lua_tointeger (L, 2);
	if (hst && action >= 0 && action < 2)
		hst->ActivateAntenna (HSTaction[action]);
	return 0;
}

/***
Open or close the HST hatch.
@function hatch
@tparam bool action Whether to open the hatch.
*/
static int hstHatch (lua_State *L)
{
	HST *hst = lua_toHST (L, 1);
	int action = lua_tointeger (L, 2);
	if (hst && action >= 0 && action < 2)
		hst->ActivateHatch (HSTaction[action]);
	return 0;
}

/***
Activate or deactivate the HST solar arrays.
@function array
@tparam bool action Whether to activate the arrays.
*/
static int hstArray (lua_State *L)
{
	HST *hst = lua_toHST (L, 1);
	int action = lua_tointeger (L, 2);
	if (hst && action >= 0 && action < 2)
		hst->ActivateArray (HSTaction[action]);
	return 0;
}

// ==========================================================================

#else // skip script support: dummy stubs

int HST::Lua_InitInterpreter(void *context)
{
	return 0;
}

int HST::Lua_InitInstance (void *context)
{
	return 0;
}

#endif // SCRIPTSUPPORT

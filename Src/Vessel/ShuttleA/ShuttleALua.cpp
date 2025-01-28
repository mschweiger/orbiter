// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "ShuttleA.h"
#include "adiball.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

// ==========================================================================
// API function prototypes

ShuttleA *lua_toShuttleA (lua_State *L, int idx = 1);
int lua_gear (lua_State *L);
int set_adilayout (lua_State *L);
int set_attrefmode (lua_State *L);
int set_attreftgtmode (lua_State *L);
int set_attrefoffset (lua_State *L);
int set_atttgtoffset (lua_State *L);
int set_attoffsetmode (lua_State *L);
int set_atttgtframemode (lua_State *L);

// ==========================================================================
// ShuttleA Lua instance initialisation

int ShuttleA::Lua_InitInstance (void *context)
{
	lua_State *L = (lua_State*)context;

	// check if interpreter has ShuttleA table loaded already
	luaL_getmetatable (L, "VESSEL.SHUTTLEA");

	if (lua_isnil (L, -1)) { // register new functions
		lua_pop (L, 1);
		static const struct luaL_Reg shuttleaLib[] = {
			{"gear", lua_gear},
			{"set_adilayout", set_adilayout},
			{"set_attrefmode", set_attrefmode},
			{"set_attreftgtmode", set_attreftgtmode},
			{"set_attrefoffset", set_attrefoffset},
			{"set_atttgtoffset", set_atttgtoffset},
			{"set_attoffsetmode", set_attoffsetmode},
			{"set_atttgtframemode", set_atttgtframemode},
			{NULL, NULL}
		};

		// create metatable for vessel userdata
		luaL_newmetatable (L, "SHUTTLEA.vtable");

		// create a table for the overloaded methods
		lua_newtable(L);
		luaL_setfuncs(L, shuttleaLib, 0);
		lua_setglobal(L, "SHUTTLEA.method");

		// create metatable for accessing inherited methods from VESSEL
		luaL_newmetatable (L, "SHUTTLEA.base");
		lua_pushstring (L, "__index");
		luaL_getmetatable (L, "VESSEL.vtable");
		lua_settable (L, -3);

		// set SHUTTLEA.base as metatable for SHUTTLEA.method
		lua_setmetatable (L, -2);

		// point vessel userdata to SHUTTLEA.method
		lua_pushstring (L, "__index");
		lua_pushvalue (L, -2); // push SHUTTLEA.method
		lua_settable (L, -4);

		// pop SHUTTLEA.method from the stack
		lua_pop (L, 1);
	}

	lua_setmetatable (L, -2);

	return 0;
}

// ==========================================================================
// Shuttle-A Lua API extensions

/***
Shuttle-A Lua API extensions

In the following list of methods, 'ShuttleA' represents an instance of a vessel of class ShuttleA.
Using these methods for other vessel types will generally result in an error.
To check the class of a vessel object, use the vessel:get_classname() method.
@classmod ShuttleA
*/

VECTOR3 lua_tovector (lua_State *L, int idx)
{
	VECTOR3 vec;
	lua_getfield (L, idx, "x");
	vec.x = lua_tonumber (L, -1); lua_pop (L,1);
	lua_getfield (L, idx, "y");
	vec.y = lua_tonumber (L, -1); lua_pop (L,1);
	lua_getfield (L, idx, "z");
	vec.z = lua_tonumber (L, -1); lua_pop (L,1);
	return vec;
}

ShuttleA *lua_toShuttleA (lua_State *L, int idx)
{
	VESSEL **pv = (VESSEL **)luaL_checkudata(L, idx, "SHUTTLEA.vtable");
	ShuttleA *sh = (ShuttleA*)*pv;
	return sh;
}

/***
Raise or lower the Shuttles gear.
@function gear
@tparam integer action Whether to raise or lower the gear.  
- action == 2 : raises the gear,  
- action == 3 : lowers the gear.
*/
static int lua_gear (lua_State *L)
{
	ShuttleA *sh = lua_toShuttleA (L, 1);
	int action = lua_tointeger (L, 2);
	if (sh && action >= 2 && action < 4)
		sh->ActivateLandingGear (action == 2 ? ShuttleA::DOOR_CLOSING : ShuttleA::DOOR_OPENING);
	return 0;
}

/***
Set the layout of the ADI ball.
@function set_adilayout
@tparam integer layout ADI ball layout.  
- layout == 0 : pitch range -90..90,  
- layout == 1 : pitch range  0..360.
*/
static int set_adilayout (lua_State *L)
{
	ShuttleA *sh = lua_toShuttleA (L, 1);
	int layout = lua_tointeger (L, 2);
	sh->SetADILayout (layout);
	return 0;
}

/***
Set the attitude reference mode.
@function set_attrefmode
@tparam integer mode Attitude reference mode selector.  
- mode == 0 : ecliptic,  
- mode == 1 : equator,  
- mode == 2 : orbit,  
- mode == 3 : local horizon,  
- mode >= 4 : NAV receiver #(N-4).
*/
static int set_attrefmode (lua_State *L)
{
	ShuttleA *sh = lua_toShuttleA (L, 1);
	int mode = lua_tointeger (L, 2);
	sh->SetAttrefMode (mode);
	return 0;
}

/***
Set the attitude reference target mode.
@function set_attreftgtmode
@tparam integer mode Attitude reference target mode selector.  
- mode == 0 : no target,  
- mode == 1 : fixed,  
- mode == 2 : direction of current NAV source,  
- mode == 3 : relative velocity of current NAV source.
*/
static int set_attreftgtmode (lua_State *L)
{
	ShuttleA *sh = lua_toShuttleA (L, 1);
	int mode = lua_tointeger (L, 2);
	sh->SetAttrefTgtMode (mode);
	return 0;
}

/***
Set the attitude reference offset.
@function set_attrefoffset
@tparam VECTOR3 ofs Attitude reference offset.
*/
static int set_attrefoffset (lua_State *L)
{
	ShuttleA *sh = lua_toShuttleA (L, 1);
	VECTOR3 ofs = lua_tovector (L, 2);
	sh->SetAttrefOffset (ofs);
	return 0;
}

/***
Set the attitude target offset.
@function set_atttgtoffset
@tparam VECTOR3 ofs Attitude target offset.
*/
static int set_atttgtoffset (lua_State *L)
{
	ShuttleA *sh = lua_toShuttleA (L, 1);
	VECTOR3 ofs = lua_tovector (L, 2);
	sh->SetAtttgtOffset (ofs);
	return 0;
}

/***
Set the attitude offset mode.
@function set_attoffsetmode
@tparam integer mode Attitude offset mode selector.  
- mode == 0 : Attitude offset mode 0,  
- mode == 1 : Attitude offset mode 1.
*/
static int set_attoffsetmode (lua_State *L)
{
	ShuttleA *sh = lua_toShuttleA (L, 1);
	int mode = lua_tointeger (L, 2);
	sh->SetAttOffsetMode (mode);
	return 0;
}

/***
Set the attitude target frame mode.
@function set_atttgtframemode
@tparam integer mode attitude target frame mode selector.  
- mode == 0 : Attitude target frame mode 0,  
- mode == 1 : Attitude target frame mode 1.
*/
static int set_atttgtframemode (lua_State *L)
{
	ShuttleA *sh = lua_toShuttleA (L, 1);
	int mode = lua_tointeger (L, 2);
	sh->SetAtttgtFrameMode (mode);
	return 0;
}

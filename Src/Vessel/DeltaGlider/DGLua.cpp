// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "DeltaGlider.h"
#include "MainRetroSubsys.h"
#include "GearSubsys.h"
#include "DockingSubsys.h"
#include "AerodynSubsys.h"
#include "PressureSubsys.h"
#include "ThermalSubsys.h"
#include <stdio.h>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

// ==========================================================================
// API function prototypes

DeltaGlider *lua_toDG (lua_State *L, int idx = 1);
int dgGear (lua_State *L);
int dgNosecone (lua_State *L);
int dgHatch (lua_State *L);
int dgRetro (lua_State *L);
int dgOLock (lua_State *L);
int dgILock (lua_State *L);
int dgRadiator (lua_State *L);
int dgABrake (lua_State *L);

// ==========================================================================
// API initialisation

int DeltaGlider::Lua_InitInterpreter (void *context)
{
	lua_State *L = (lua_State*)context;

	// load atmospheric autopilot
	luaL_dofile (L, "Script\\dg\\aap.lua");

	return 0;
}

int DeltaGlider::Lua_InitInstance (void *context)
{
	lua_State *L = (lua_State*)context;

	// check if interpreter has DG table loaded already
	luaL_getmetatable (L, "VESSEL.DG");

	if (lua_isnil (L, -1)) { // register new functions
		lua_pop (L, 1);
		static const struct luaL_Reg dgLib[] = {
			{"Gear", dgGear},
			{"Nosecone", dgNosecone},
			{"Hatch", dgHatch},
			{"Retro", dgRetro},
			{"OLock", dgOLock},
			{"ILock", dgILock},
			{"Radiator", dgRadiator},
			{"ABrake", dgABrake},
			{NULL, NULL}
		};

		// create metatable for vessel userdata
		luaL_newmetatable (L, "DG.vtable");

		// create a table for the overloaded methods
		luaL_newlib(L, dgLib);
		lua_setglobal(L, "DG.method");
		lua_getglobal(L, "DG.method");

		// create metatable for accessing inherited methods from VESSEL
		luaL_newmetatable (L, "DG.base");
		lua_pushstring (L, "__index");
		luaL_getmetatable (L, "VESSEL.vtable");
		lua_settable (L, -3);

		// set DG.base as metatable for DG.method
		lua_setmetatable (L, -2);

		// point vessel userdata to DG.method
		lua_pushstring (L, "__index");
		lua_pushvalue (L, -2); // push DG.method
		lua_settable (L, -4);

		// pop DG.method from the stack
		lua_pop (L, 1);
	}

	lua_setmetatable (L, -2);

	return 0;
}

// ==========================================================================
// DeltaGlider Lua API extensions

/***
DeltaGlider Lua API extensions

In the following list of methods, 'DeltaGlider' represents an instance of a vessel of class DeltaGlider or DG-S (the scramjet version). Using
these methods for other vessel types will generally result in an error. To check the class of a vessel object, use the vessel:get_classname() method.
@classmod DeltaGlider
*/

DeltaGlider *lua_toDG (lua_State *L, int idx)
{
	VESSEL **pv = (VESSEL **)luaL_checkudata(L, idx, "DG.vtable");
	DeltaGlider *dg = (DeltaGlider*)*pv;
	return dg;
}

/***
Raise or lower the DeltaGliders gear.
@function Gear
@tparam bool action Whether to lower the gear.
*/
static int dgGear (lua_State *L)
{
	DeltaGlider *dg = lua_toDG (L, 1);
	int action = lua_tointeger (L, 2);
	if (action & 1) dg->SubsysGear()->LowerGear();
	else            dg->SubsysGear()->RaiseGear();
	return 0;
}

/***
Open or close the DeltaGliders nosecone.
@function Nosecone
@tparam bool action Whether to open the nosecode.
*/
static int dgNosecone (lua_State *L)
{
	DeltaGlider *dg = lua_toDG (L, 1);
	int action = lua_tointeger (L, 2);
	if (dg && action >= 0 && action < 2) {
		if (action == 0) dg->SubsysDocking()->CloseNcone();
		else             dg->SubsysDocking()->OpenNcone();
	}
	return 0;
}

/***
Open or close the DeltaGliders hatch.
@function Hatch
@tparam bool action Whether to open the hatch.
*/
static int dgHatch (lua_State *L)
{
	DeltaGlider *dg = lua_toDG (L, 1);
	int action = lua_tointeger (L, 2);
	if (dg && action >= 0 && action < 2) {
		if (action == 0) dg->SubsysPressure()->CloseHatch();
		else             dg->SubsysPressure()->OpenHatch();
	}
	return 0;
}

/***
Open or close the DeltaGliders retro covers.
@function Retro
@tparam bool action Whether to open the retro covers.
*/
static int dgRetro (lua_State *L)
{
	DeltaGlider *dg = lua_toDG (L, 1);
	int action = lua_tointeger (L, 2);
	if (dg && action >= 0 && action < 2) {
		if (action == 0) dg->SubsysMainRetro()->CloseRetroCover();
		else             dg->SubsysMainRetro()->OpenRetroCover();
	}
	return 0;
}

/***
Open or close the DeltaGliders outer airlock.
@function OLock
@tparam bool action Whether to to open the outer airlock.
*/
static int dgOLock (lua_State *L)
{
	DeltaGlider *dg = lua_toDG (L, 1);
	int action = lua_tointeger (L, 2);
	if (dg && action >= 0 && action < 2) {
		if (action == 0) dg->SubsysPressure()->CloseOuterAirlock();
		else             dg->SubsysPressure()->OpenOuterAirlock();
	}
	return 0;
}

/***
Open or close the DeltaGliders inner airlock.
@function ILock
@tparam bool action Whether to open the outer airlock.
*/
static int dgILock (lua_State *L)
{
	DeltaGlider *dg = lua_toDG (L, 1);
	int action = lua_tointeger (L, 2);
	if (dg && action >= 0 && action < 2) {
		if (action == 0) dg->SubsysPressure()->CloseInnerAirlock();
		else             dg->SubsysPressure()->OpenInnerAirlock();
	}
	return 0;
}

/***
Open or close the DeltaGliders radiators.
@function Radiator
@tparam bool action Whether to open the radiators.
*/
static int dgRadiator (lua_State *L)
{
	DeltaGlider *dg = lua_toDG (L, 1);
	int action = lua_tointeger (L, 2);
	if (dg && action >= 0 && action < 2) {
		if (action == 0) dg->SubsysThermal()->CloseRadiator();
		else             dg->SubsysThermal()->OpenRadiator();
	}
	return 0;
}

/***
Extend or retract the DeltaGliders airbrake.
@function ABrake
@tparam bool action Whether to extend the airbrake
*/
static int dgABrake (lua_State *L)
{
	DeltaGlider *dg = lua_toDG (L, 1);
	int action = lua_tointeger (L, 2);
	if (dg && action >= 0 && action < 2) {
		if (action == 0) dg->SubsysAerodyn()->RetractAirbrake();
		else             dg->SubsysAerodyn()->ExtendAirbrake();
	}
	return 0;
}

// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ==============================================================
//                 ORBITER MODULE: ScriptVessel
//                   Part of the ORBITER SDK
//
// ScriptVessel.cpp
// Control module for ScriptVessel vessel class
//
// Notes:
// Implementation of a generic vessel class that acts as an
// interface to script-driven vessel definitions.
// This class creates an interpreter instance, loads a vessel class-
// specific script and and implements the VESSEL2 callback functions
// by calling corresponding script functions.
// ==============================================================

#define STRICT
#define ORBITER_MODULE
#include <set>
#include <vector>
extern "C" {
#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>
}
#include "orbitersdk.h"
#include <filesystem>
namespace fs = std::filesystem;

enum {
	SETCLASSCAPS,
	POSTCREATION,
	PRESTEP,
	POSTSTEP,
	SAVESTATE,
	LOADSTATEEX,
	CONSUMEBUFFEREDKEY,
	CONSUMEDIRECTKEY,
	FOCUSCHANGED,
	PLAYBACKEVENT,
	RCSMODE,
	ADCTRLMODE,
	HUDMODE,
	MFDMODE,
	NAVMODE,
	DOCKEVENT,
	ANIMATE,
	LOADGENERICCOCKPIT,
	PANELMOUSEEVENT,
	PANELREDRAWEVENT,
	LOADVC,
	VISUALCREATED,
	VISUALDESTROYED,
	VCMOUSEEVENT,
	VCREDRAWEVENT,
	DRAWHUD,
	NAVPROCESS,
	LOADPANEL2D,
	RENDERHUD,
	GETRADIATIONFORCE,
	NCLBK // must be last to represent the number of available callbacks
};

const char *CLBKNAME[NCLBK] = {
	"setclasscaps",
	"postcreation",
	"prestep",
	"poststep",
	"savestate",
	"loadstateex",
	"consumebufferedkey",
	"consumedirectkey",
	"focuschanged",
	"playbackevent",
	"RCSmode",
	"ADctrlmode",
	"HUDmode",
	"MFDmode",
	"NAVmode",
	"dockevent",
	"animate",
	"loadgenericcockpit",
	"panelmouseevent",
	"panelredrawevent",
	"loadVC",
	"visualcreated",
	"visualdestroyed",
	"VCmouseevent",
	"VCredrawevent",
	"drawHUD",
	"navprocess",
	"loadpanel2d",
	"renderHUD",
	"getradiationforce"
};

DLLCLBK void InitModule (HINSTANCE hDLL)
{
}

DLLCLBK void ExitModule (HINSTANCE hDLL)
{
}

static VECTOR3 lua_tovector(lua_State* L, int idx)
{
	VECTOR3 vec;
	lua_getfield(L, idx, "x");
	vec.x = lua_tonumber(L, -1); lua_pop(L, 1);
	lua_getfield(L, idx, "y");
	vec.y = lua_tonumber(L, -1); lua_pop(L, 1);
	lua_getfield(L, idx, "z");
	vec.z = lua_tonumber(L, -1); lua_pop(L, 1);
	return vec;
}

static void lua_pushvector(lua_State* L, const VECTOR3& vec)
{
	lua_createtable(L, 0, 3);
	lua_pushnumber(L, vec.x);
	lua_setfield(L, -2, "x");
	lua_pushnumber(L, vec.y);
	lua_setfield(L, -2, "y");
	lua_pushnumber(L, vec.z);
	lua_setfield(L, -2, "z");
}

static int lua_isvector(lua_State* L, int idx)
{
	if (!lua_istable(L, idx)) return 0;
	static char fieldname[3] = { 'x','y','z' };
	static char field[2] = "x";
	int i, ii, n;
	bool fail;

	lua_pushnil(L);
	ii = (idx >= 0 ? idx : idx - 1);
	n = 0;
	while (lua_next(L, ii)) {
		lua_pop(L, 1);
		n++;
	}
	if (n != 3) return 0;

	for (i = 0; i < 3; i++) {
		field[0] = fieldname[i];
		lua_getfield(L, idx, field);
		fail = (lua_isnil(L, -1));
		lua_pop(L, 1);
		if (fail) return 0;
	}
	return 1;
}

// ==============================================================
// ScriptVessel class interface
// ==============================================================

class ScriptVessel: public VESSEL4 {
public:
	ScriptVessel (OBJHANDLE hVessel, int flightmodel);
	~ScriptVessel ();
	int Lua_InitInstance(void* context);
	// VESSEL2
	void clbkSetClassCaps (FILEHANDLE cfg) override;
	void clbkPostCreation () override;
	void clbkPreStep (double simt, double simdt, double mjd) override;
	void clbkPostStep (double simt, double simdt, double mjd) override;
	void clbkSaveState(FILEHANDLE scn) override;
	void clbkLoadStateEx(FILEHANDLE scn, void* vs) override;
	int  clbkConsumeDirectKey(char* kstate) override;
	int  clbkConsumeBufferedKey(DWORD key, bool down, char* kstate) override;
	void clbkFocusChanged(bool getfocus, OBJHANDLE hNewVessel, OBJHANDLE hOldVessel) override;
	bool clbkPlaybackEvent(double simt, double event_t, const char* event_type, const char* event) override;
	void clbkRCSMode(int mode) override;
	void clbkADCtrlMode(DWORD mode) override;
	void clbkHUDMode(int mode) override;
	void clbkMFDMode(int mfd, int mode) override;
	void clbkNavMode(int mode, bool active) override;
	void clbkDockEvent(int dock, OBJHANDLE mate) override;
	void clbkAnimate(double simt) override;
	bool clbkLoadGenericCockpit() override;
	bool clbkPanelMouseEvent(int id, int event, int mx, int my, void *context) override;
	bool clbkPanelRedrawEvent(int id, int event, SURFHANDLE surf, void *context) override;
	bool clbkLoadVC(int id) override;
	void clbkVisualCreated(VISHANDLE vis, int refcount) override;
	void clbkVisualDestroyed(VISHANDLE vis, int refcount) override;
	bool clbkVCMouseEvent(int id, int event, VECTOR3& p) override;
	bool clbkVCRedrawEvent(int id, int event, SURFHANDLE surf) override;
	bool clbkLoadPanel2D (int id, PANELHANDLE hPanel, DWORD viewW, DWORD viewH) override;

	// VESSEL3
	int clbkGeneric(int msgid, int prm, void* context) override;
	bool clbkDrawHUD(int mode, const HUDPAINTSPEC* hps, oapi::Sketchpad* skp) override;
	void clbkRenderHUD (int mode, const HUDPAINTSPEC *hps, SURFHANDLE hTex) override;
	void clbkGetRadiationForce (const VECTOR3 &mflux, VECTOR3 &F, VECTOR3 &pos) override;

	// VESSEL4
	int clbkNavProcess (int mode) override;

	lua_State* GetState() { return L; }
	int LuaCall(lua_State *L, int nargs, int nres);
protected:
	INTERPRETERHANDLE hInterp;
	lua_State *L;
	std::vector<std::string> exports;

	bool bclbk[NCLBK];
	char func[256];
	int fmodel;
};

// ==============================================================
// Constructor/destructor
// ==============================================================
ScriptVessel::ScriptVessel (OBJHANDLE hVessel, int flightmodel): VESSEL4 (hVessel, flightmodel)
{
	// create the interpreter instance to run the vessel script
	hInterp = oapiCreateInterpreter();
	L = oapiGetLua (hInterp);

	strcpy (func, "clbk_");
	fmodel = flightmodel;
}

ScriptVessel::~ScriptVessel ()
{
	// Call pseudo destructor if the script needs to do some cleanup
	lua_getfield (L, LUA_GLOBALSINDEX, "clbk_destroy");
	if(lua_isfunction (L,-1)) {
		LuaCall (L, 0, 0);
	}
	lua_pop(L,1);

	// delete the interpreter instance
	oapiDelInterpreter (hInterp);
}

static int traceback(lua_State *L) {
    lua_getfield(L, LUA_GLOBALSINDEX, "debug");
    lua_getfield(L, -1, "traceback");
    lua_pushvalue(L, 1);
    lua_pushinteger(L, 2);
    lua_call(L, 2, 1);
    return 1;
}

int ScriptVessel::LuaCall(lua_State *L, int narg, int nres)
{
	int base = lua_gettop(L) - narg;
	lua_pushcfunction(L, traceback);
	lua_insert(L, base);
	int res = lua_pcall(L, narg, nres, base);
	lua_remove(L, base);
	if(res != 0) {
		oapiWriteLogError("%s", lua_tostring(L, -1));
		oapiAddNotification(OAPINOTIF_ERROR, "Lua vessel error", lua_tostring(L, -1));
	}
	return res;
}

// ==============================================================
// Overloaded callback functions
// ==============================================================

// --------------------------------------------------------------
// Set the capabilities of the vessel class
// --------------------------------------------------------------
#define lua_pushglobaltable(L) lua_pushvalue(L,LUA_GLOBALSINDEX)

static std::set<std::string> GetGlobalFunctions(lua_State* L)
{
	std::set<std::string> ret;
	// find all global functions
	lua_pushglobaltable(L);
	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		if (lua_isfunction(L, -1))
			ret.insert(lua_tostring(L, -2));
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	return ret;
}

void ScriptVessel::clbkSetClassCaps (FILEHANDLE cfg)
{
	char script[256], cmd[256];
	int i;

	// Save global functions provided by lua
	auto globals = GetGlobalFunctions(L);

	oapiReadItem_string (cfg, (char*)"Script", script);
	fs::path script_path(script);
	std::string parent_path = script_path.parent_path().u8string();
	// Add the script path to the package path so that we can "require" additional files
	sprintf(cmd, "package.path = package.path .. ';Config/Vessels/%s/?.lua'", parent_path.c_str());
	oapiExecScriptCmd(hInterp, cmd);

	bool strictmode = false;
	oapiReadItem_bool (cfg, (char*)"StrictMode", strictmode);
	if(strictmode) {
		// Load the 'strict' module
		sprintf (cmd, "run_global('Script/strict.lua')");
		oapiExecScriptCmd (hInterp, cmd);
	}

	// Load the vessel script
	sprintf (cmd, "run_global('Config/Vessels/%s')", script);
	oapiExecScriptCmd (hInterp, cmd);

	// find new global functions provided by the module
	lua_pushglobaltable(L);
	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		if(lua_isfunction(L, -1) && globals.count(lua_tostring(L, -2)) == 0 && strncmp(lua_tostring(L, -2), "clbk_", 5)) {
			exports.push_back(lua_tostring(L, -2));
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);

	// Define the vessel instance
	lua_pushlightuserdata(L, GetHandle());  // push vessel handle
	lua_setfield(L, LUA_GLOBALSINDEX, "hVessel");
	strcpy(cmd, "vi = vessel.get_interface(hVessel)");
	oapiExecScriptCmd(hInterp, cmd);

	// check for defined callback functions in script
	for (i = 0; i < NCLBK; i++) {
		strcpy (func+5, CLBKNAME[i]);
		lua_getfield (L, LUA_GLOBALSINDEX, func);
		bclbk[i] = (lua_isfunction (L,-1) != 0);
		lua_pop(L,1);
	}

	// Call pseudo constructor method now that we have loaded the script
	lua_getfield (L, LUA_GLOBALSINDEX, "clbk_new");
	if(lua_isfunction (L,-1)) {
		lua_pushnumber(L, fmodel);
		LuaCall (L, 1, 0);
	}
	lua_pop(L,1);


	// Run the SetClassCaps function
	if (bclbk[SETCLASSCAPS]) {
		strcpy (func+5, "setclasscaps");
		lua_getfield (L, LUA_GLOBALSINDEX, func);
		lua_pushlightuserdata (L, cfg);
		LuaCall (L, 1, 0);
	}
}

void ScriptVessel::clbkPostCreation ()
{
	if (bclbk[POSTCREATION]) {
		strcpy (func+5, "postcreation");
		lua_getfield (L, LUA_GLOBALSINDEX, func);
		LuaCall (L, 0, 0);
	}
}

void ScriptVessel::clbkPreStep (double simt, double simdt, double mjd)
{
	if (bclbk[PRESTEP]) {
		strcpy (func+5, "prestep");
		lua_getfield (L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L,simt);
		lua_pushnumber(L,simdt);
		lua_pushnumber(L,mjd);
		LuaCall (L, 3, 0);
	}
}

void ScriptVessel::clbkPostStep (double simt, double simdt, double mjd)
{
	if (bclbk[POSTSTEP]) {
		strcpy (func+5, "poststep");
		lua_getfield (L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L,simt);
		lua_pushnumber(L,simdt);
		lua_pushnumber(L,mjd);
		LuaCall (L, 3, 0);
	}
	oapiExecScriptCmd (hInterp, "--"); // update background threads count
}

void ScriptVessel::clbkSaveState(FILEHANDLE scn)
{
	VESSEL2::clbkSaveState(scn);
	if (bclbk[SAVESTATE]) {
		strcpy(func + 5, "savestate");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushlightuserdata(L, scn);
		LuaCall(L, 1, 0);
	}
}

void ScriptVessel::clbkLoadStateEx(FILEHANDLE scn, void* vs)
{
	if (bclbk[LOADSTATEEX]) {
		strcpy(func + 5, "loadstateex");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushlightuserdata(L, scn);
		VESSELSTATUS2* status = (VESSELSTATUS2*)lua_newuserdata(L, sizeof(VESSELSTATUS2));
		luaL_getmetatable(L, "VESSELSTATUS2.table");   // push metatable
		lua_setmetatable(L, -2);              // set metatable for VESSELSTATUS2
		*status = *(VESSELSTATUS2*)vs;
		if(LuaCall(L, 2, 0) !=0) {
			// If the call fails, odds are that the scn file parsing was stopped before reaching the END token
			// We try to finish it anyway to prevent fatal errors later 
			char *line;
			while (oapiReadScenario_nextline (scn, line)) {
				ParseScenarioLineEx (line, vs);
			}
		}
		*(VESSELSTATUS2*)vs = *status;
	} else {
		VESSEL2::clbkLoadStateEx(scn, vs);
	}
}

int ScriptVessel::clbkConsumeDirectKey(char* kstate)
{
	if (bclbk[CONSUMEDIRECTKEY]) {
		strcpy(func + 5, "consumedirectkey");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushlightuserdata(L, kstate);
		LuaCall(L, 1, 1);
		bool consumed = (lua_toboolean(L, -1) ? true : false);
		lua_pop(L, 1);
		return consumed ? 1 : 0;
	}
	return 0;
}

int ScriptVessel::clbkConsumeBufferedKey(DWORD key, bool down, char* kstate)
{
	if (bclbk[CONSUMEBUFFEREDKEY]) {
		strcpy(func + 5, "consumebufferedkey");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L, key);
		lua_pushboolean(L, down);
		lua_pushlightuserdata(L, kstate);
		LuaCall(L, 3, 1);
		bool consumed = lua_toboolean(L, -1) ? true : false;
		lua_pop(L, 1);
		return consumed ? 1 : 0;
	}
	return 0;
}

void ScriptVessel::clbkFocusChanged(bool getfocus, OBJHANDLE hNewVessel, OBJHANDLE hOldVessel)
{
	if (bclbk[FOCUSCHANGED]) {
		strcpy(func + 5, "focuschanged");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushboolean(L, getfocus);
		lua_pushlightuserdata(L, hNewVessel);
		if(hOldVessel)
			lua_pushlightuserdata(L, hOldVessel);
		else
			lua_pushnil(L);
		LuaCall(L, 3, 0);
	}
}

bool ScriptVessel::clbkPlaybackEvent(double simt, double event_t, const char* event_type, const char* event)
{
	if (bclbk[PLAYBACKEVENT]) {
		strcpy(func + 5, "playbackevent");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L, simt);
		lua_pushnumber(L, event_t);
		lua_pushstring(L, event_type);
		lua_pushstring(L, event);
		LuaCall(L, 4, 1);
		bool consumed = (lua_toboolean(L, -1) ? true : false);
		lua_pop(L, 1);
		return consumed ? 1 : 0;
	}
	return 0;
}

void ScriptVessel::clbkRCSMode(int mode)
{
	if (bclbk[RCSMODE]) {
		strcpy(func + 5, "RCSmode");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L, mode);
		LuaCall(L, 1, 0);
	}
}

void ScriptVessel::clbkADCtrlMode(DWORD mode)
{
	if (bclbk[ADCTRLMODE]) {
		strcpy(func + 5, "ADctrlmode");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L, mode);
		LuaCall(L, 1, 0);
	}
}

void ScriptVessel::clbkHUDMode(int mode)
{
	if (bclbk[HUDMODE]) {
		strcpy(func + 5, "HUDmode");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L, mode);
		LuaCall(L, 1, 0);
	}
}

void ScriptVessel::clbkMFDMode(int mfd, int mode)
{
	if (bclbk[MFDMODE]) {
		strcpy(func + 5, "MFDmode");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L, mfd);
		lua_pushnumber(L, mode);
		LuaCall(L, 2, 0);
	}
}

void ScriptVessel::clbkNavMode(int mode, bool active)
{
	if (bclbk[NAVMODE]) {
		strcpy(func + 5, "NAVmode");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L, mode);
		lua_pushboolean(L, active);
		LuaCall(L, 2, 0);
	}
}

void ScriptVessel::clbkDockEvent(int dock, OBJHANDLE mate)
{
	if (bclbk[DOCKEVENT]) {
		strcpy(func + 5, "dockevent");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L, dock);
		if (mate)
			lua_pushlightuserdata(L, mate);
		else
			lua_pushnil(L);
		LuaCall(L, 2, 0);
	}
}

void ScriptVessel::clbkAnimate(double simt)
{
	if (bclbk[ANIMATE]) {
		strcpy(func + 5, "animate");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L, simt);
		LuaCall(L, 1, 0);
	}
}

bool ScriptVessel::clbkLoadGenericCockpit()
{
	if (bclbk[LOADGENERICCOCKPIT]) {
		strcpy(func + 5, "loadgenericcockpit");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		LuaCall(L, 0, 1);

		bool supported = lua_toboolean(L, -1) ? true : false;
		lua_pop(L, 1);
		return supported;
	} else {
		return VESSEL2::clbkLoadGenericCockpit();
	}
}

bool ScriptVessel::clbkPanelMouseEvent(int id, int event, int mx, int my, void *context)
{
	if (bclbk[PANELMOUSEEVENT]) {
		strcpy(func + 5, "panelmouseevent");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L, id);
		lua_pushnumber(L, event);
		lua_pushnumber(L, mx);
		lua_pushnumber(L, my);

		lua_rawgeti(L, LUA_REGISTRYINDEX, (int)(ptrdiff_t)context);

		LuaCall(L, 5, 1);

		bool processed = lua_toboolean(L, -1) ? true : false;
		lua_pop(L, 1);
		return processed;
	}
	return false;
}

bool ScriptVessel::clbkPanelRedrawEvent(int id, int event, SURFHANDLE surf, void *context)
{
	if (bclbk[PANELREDRAWEVENT]) {
		strcpy(func + 5, "panelredrawevent");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L, id);
		lua_pushnumber(L, event);
		lua_pushlightuserdata(L, surf);

		lua_rawgeti(L, LUA_REGISTRYINDEX, (int)(ptrdiff_t)context);

		LuaCall(L, 4, 1);

		bool processed = lua_toboolean(L, -1) ? true : false;
		lua_pop(L, 1);
		return processed;
	}
	return false;
}

bool ScriptVessel::clbkLoadVC(int id)
{
	if (bclbk[LOADVC]) {
		strcpy(func + 5, "loadVC");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L, id);
		LuaCall(L, 1, 1);

		bool supported = lua_toboolean(L, -1) ? true : false;
		lua_pop(L, 1);
		return supported;
	}
	return false;
}

void ScriptVessel::clbkVisualCreated(VISHANDLE vis, int refcount)
{
	if (bclbk[VISUALCREATED]) {
		strcpy(func + 5, "visualcreated");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushlightuserdata(L, vis);
		lua_pushnumber(L, refcount);
		LuaCall(L, 2, 0);
	}
}
void ScriptVessel::clbkVisualDestroyed(VISHANDLE vis, int refcount)
{
	if (bclbk[VISUALDESTROYED]) {
		strcpy(func + 5, "visualdestroyed");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushlightuserdata(L, vis);
		lua_pushnumber(L, refcount);
		LuaCall(L, 2, 0);
	}
}

bool ScriptVessel::clbkVCMouseEvent(int id, int event, VECTOR3& p)
{
	if (bclbk[VCMOUSEEVENT]) {
		strcpy(func + 5, "VCmouseevent");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L, id);
		lua_pushnumber(L, event);
		lua_pushvector(L, p);
		LuaCall(L, 3, 1);

		bool processed = (lua_toboolean(L, -1) ? true : false);
		lua_pop(L, 1);
		return processed;
	}
	return false;
}
bool ScriptVessel::clbkVCRedrawEvent(int id, int event, SURFHANDLE surf)
{
	if (bclbk[VCREDRAWEVENT]) {
		strcpy(func + 5, "VCredrawevent");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L, id);
		lua_pushnumber(L, event);
		lua_pushlightuserdata(L, surf);
		LuaCall(L, 3, 1);

		bool processed = (lua_toboolean(L, -1) ? true : false);
		lua_pop(L, 1);
		return processed;
	}
	return false;
}

bool ScriptVessel::clbkLoadPanel2D (int id, PANELHANDLE hPanel, DWORD viewW, DWORD viewH)
{
	if (bclbk[LOADPANEL2D]) {
		strcpy(func + 5, "loadpanel2d");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L, id);
		lua_pushlightuserdata(L, hPanel);
		lua_pushnumber(L, viewW);
		lua_pushnumber(L, viewH);
		LuaCall(L, 4, 1);

		bool processed = (lua_toboolean(L, -1) ? true : false);
		lua_pop(L, 1);
		return processed;
		
	}
	return false;
}

static ScriptVessel* lua_toScriptVessel(lua_State* L, int idx)
{
	VESSEL** pv = (VESSEL**)lua_touserdata(L, idx);
	if(pv)
		return (ScriptVessel*)*pv;

	return nullptr;
}


// Extract the arguments from the original lua state,
// push them into the target lua state and call the method
// then finally push back the result fro; the target state to the original state
static int lua_crosscall(lua_State* L) {
	ScriptVessel* sh = lua_toScriptVessel(L, 1);
	if (!sh)
		return 0;
	lua_State* Ltgt = sh->GetState();

	// Recover method name for the target interpreter
	const char* method = lua_tostring(L, lua_upvalueindex(1));

	// Save target interpreter stacksize (used later to compute the number of results)
	int stacksize = lua_gettop(Ltgt);

	// Push the function to be called in the target
	lua_getfield(Ltgt, LUA_GLOBALSINDEX, method);

	// Number of arguments pushed in the original lua_State
	int nargs = lua_gettop(L);

	// "Forward" arguments to the target lua_State (Lto)
	// Warning : only basic types and vectors are supported (no tables/userdata)
	// we start at 2 to skip the ScriptVessel * that was passed as first argument
	for (int i = 2; i <= nargs; i++) {
		switch (lua_type(L, i)) {
		case LUA_TNIL:
			lua_pushnil(Ltgt);
			break;
		case LUA_TTABLE:
		{
			if (lua_isvector(L, i)) {
				VECTOR3 v = lua_tovector(L, i);
				lua_pushvector(Ltgt, v);
			} else {
				lua_pushnil(Ltgt);
			}
			break;
		}
		case LUA_TBOOLEAN:
		{
			int v = lua_toboolean(L, i);
			lua_pushboolean(Ltgt, v);
			break;
		}
		case LUA_TLIGHTUSERDATA:
		{
			void *ud = lua_touserdata(L, i);
			lua_pushlightuserdata(Ltgt, ud);
			break;
		}
		case LUA_TNUMBER:
		{
			lua_Number v = lua_tonumber(L, i);
			lua_pushnumber(Ltgt, v);
			break;
		}
		case LUA_TSTRING:
		{
			const char *str = lua_tostring(L, i);
			lua_pushstring(Ltgt, str);
			break;
		}
		default:
			// Warning : we push nil for unsupported args to simplify processing (no need to clean up)
			lua_pushnil(Ltgt);
			break;
		}
	}
	// Clean up original state
	lua_pop(L, nargs);

	// Call the function in the target lua state
	// use nargs - 1 because the ScriptVessel * argument has been skipped
	if (lua_pcall(Ltgt, nargs - 1, LUA_MULTRET, 0) != 0) {
		printf("Error cross-calling %s: %s\n", method, lua_tostring(Ltgt, -1));
	}
	int nret = lua_gettop(Ltgt) - stacksize; // number of results

	// "Forward" results to the original lua_State *in reverse order*
	// Warning : only basic types are supported (no tables/userdata)
	for (int i = nret; i >= 1; i--) {
		switch (lua_type(Ltgt, -i)) {
		case LUA_TNIL:
			lua_pushnil(L);
			break;
		case LUA_TTABLE:
			if (lua_isvector(Ltgt, -i)) {
				VECTOR3 v = lua_tovector(Ltgt, -i);
				lua_pushvector(L, v);
			}
			else {
				lua_pushnil(L);
			}
			break;
		case LUA_TBOOLEAN:
		{
			int v = lua_toboolean(Ltgt, -i);
			lua_pushboolean(L, v);
			break;
		}
		case LUA_TLIGHTUSERDATA:
		{
			void* ud = lua_touserdata(Ltgt, -i);
			lua_pushlightuserdata(L, ud);
			break;
		}
		case LUA_TNUMBER:
		{
			lua_Number v = lua_tonumber(Ltgt, -i);
			lua_pushnumber(L, v);
			break;
		}
		case LUA_TSTRING:
		{
			const char* str = lua_tostring(Ltgt, -i);
			lua_pushstring(L, str);
			break;
		}
		default:
			// Warning : we push nil for unsupported args to simplify processing (no need to clean up)
			lua_pushnil(L);
			break;
		}

	}

	// Cleanup target state
	lua_pop(Ltgt, nret);

	return nret;
}

// Lua_InitInstance is called when we do a push_vessel from C
// It's purpose is to add methods to the userdata encapulating the VESSEL pointer
// so we can call them
// If we push a lua vessel inside another lua_State than its self, we must forward them (through lua_crosscall)
// If we push a vessel inside its own state, we populate its metatable with the global functions declared in its lua file
int ScriptVessel::Lua_InitInstance(void* context)
{
	if (exports.size() == 0)
		return 0;

	lua_State* Linto = (lua_State*)context;

	char metatablename[256];
	snprintf(metatablename, 255, "VESSEL.SCRIPT.%s.vtable", GetClassName());

	// check if interpreter has metatable table available already
	luaL_getmetatable(Linto, metatablename);

	if (lua_isnil(Linto, -1)) { // register new functions
		lua_pop(Linto, 1);

		// create metatable for vessel userdata
		luaL_newmetatable(Linto, metatablename);

		if (Linto == L) {
			// in local context, we push the global functions to the vessel metatable
			lua_pushglobaltable(Linto);
			// create methods table for exported functions
			lua_newtable(Linto);
			for (const auto& method : exports) {
				lua_getfield(Linto, -2, method.c_str());
				lua_setfield(Linto, -2, method.c_str());
			}
			lua_remove(L, -2); // remove global table
		} else {
			// for other states, we use the lua_crosscall proxy
			lua_newtable(Linto);
			for (const auto& method : exports) {
				lua_pushstring(Linto, method.c_str());
				lua_pushcclosure(Linto, lua_crosscall, 1);
				lua_setfield(Linto, -2, method.c_str());
			}
		}

		// create metatable for accessing inherited methods from VESSEL
		snprintf(metatablename, 255, "VESSEL.SCRIPT.%s.base", GetClassName());
		luaL_newmetatable(Linto, metatablename);
		lua_pushstring(Linto, "__index");
		luaL_getmetatable(Linto, "VESSEL.vtable");
		lua_settable(Linto, -3);  //SCRIPT.base.__index = VESSEL.vtable
		
		// set SHUTTLEA.base as metatable for SHUTTLEA.method
		lua_setmetatable(Linto, -2);

		// point vessel userdata to SHUTTLEA.method
		lua_pushstring(Linto, "__index");
		lua_pushvalue(Linto, -2); // push SHUTTLEA.method
		lua_settable(Linto, -4);

		// pop SHUTTLEA.method from the stack
		lua_pop(Linto, 1);
	}

	lua_setmetatable(Linto, -2);

	return 0;
}

int ScriptVessel::clbkGeneric(int msgid, int prm, void* context)
{
	switch (msgid) {
	case VMSG_LUAINTERPRETER:
		return 0;
	case VMSG_LUAINSTANCE:
		return Lua_InitInstance(context);
	}
	return 0;
}

static void lua_pushsketchpad(lua_State* L, oapi::Sketchpad* skp)
{
	lua_pushlightuserdata(L, skp);       // use object pointer as key
	lua_gettable(L, LUA_REGISTRYINDEX);  // retrieve object from registry
	if (lua_isnil(L, -1)) {              // object not found
		lua_pop(L, 1);                   // pop nil
		oapi::Sketchpad** pskp = (oapi::Sketchpad**)lua_newuserdata(L, sizeof(oapi::Sketchpad*));
		*pskp = skp;
		luaL_getmetatable(L, "SKP.vtable"); // retrieve metatable
		lua_setmetatable(L, -2);             // and attach to new object
		lua_pushlightuserdata(L, skp);        // create key
		lua_pushvalue(L, -2);                 // push object
		lua_settable(L, LUA_REGISTRYINDEX);  // and store in registry
		// note that now the object is on top of the stack
	}
}

bool ScriptVessel::clbkDrawHUD(int mode, const HUDPAINTSPEC* hps, oapi::Sketchpad* skp)
{
	// draw the default HUD
	VESSEL3::clbkDrawHUD(mode, hps, skp);
	if (bclbk[DRAWHUD]) {

		strcpy(func + 5, "drawHUD");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L, mode);

		lua_createtable(L, 0, 6);
		lua_pushnumber(L, hps->W);
		lua_setfield(L, -2, "W");
		lua_pushnumber(L, hps->H);
		lua_setfield(L, -2, "H");
		lua_pushnumber(L, hps->CX);
		lua_setfield(L, -2, "CX");
		lua_pushnumber(L, hps->CY);
		lua_setfield(L, -2, "CY");
		lua_pushnumber(L, hps->Scale);
		lua_setfield(L, -2, "Scale");
		lua_pushnumber(L, hps->Markersize);
		lua_setfield(L, -2, "Markersize");

		lua_pushsketchpad(L, skp);
		LuaCall(L, 3, 1);

		bool supported = (lua_toboolean(L, -1) ? true : false);
		lua_pop(L, 1);
		return supported;
	}
	return false;
}

void ScriptVessel::clbkRenderHUD (int mode, const HUDPAINTSPEC *hps, SURFHANDLE hTex)
{
	// draw the default HUD
	VESSEL3::clbkRenderHUD(mode, hps, hTex);
	if (bclbk[RENDERHUD]) {
		strcpy(func + 5, "renderHUD");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L, mode);
		lua_createtable(L, 0, 6);
		lua_pushnumber(L, hps->W);
		lua_setfield(L, -2, "W");
		lua_pushnumber(L, hps->H);
		lua_setfield(L, -2, "H");
		lua_pushnumber(L, hps->CX);
		lua_setfield(L, -2, "CX");
		lua_pushnumber(L, hps->CY);
		lua_setfield(L, -2, "CY");
		lua_pushnumber(L, hps->Scale);
		lua_setfield(L, -2, "Scale");
		lua_pushnumber(L, hps->Markersize);
		lua_setfield(L, -2, "Markersize");
		lua_pushlightuserdata(L, hTex);
		LuaCall(L, 3, 0);
	}
}

void ScriptVessel::clbkGetRadiationForce (const VECTOR3 &mflux, VECTOR3 &F, VECTOR3 &pos)
{
	if (bclbk[GETRADIATIONFORCE]) {
		strcpy(func + 5, "getradiationforce");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushvector(L, mflux);
		if(LuaCall(L, 1, 2) != 0) {
			lua_settop(L, 0);
			F={0,0,0};
			pos={0,0,0};
		} else {
			F = lua_tovector(L, -2);
			pos = lua_tovector(L, -1);
			lua_pop(L, 2);
		}
	} else {
		VESSEL3::clbkGetRadiationForce(mflux, F, pos);
	}
}


int ScriptVessel::clbkNavProcess(int mode)
{
	if (bclbk[NAVPROCESS]) {
		strcpy(func + 5, "navprocess");
		lua_getfield(L, LUA_GLOBALSINDEX, func);
		lua_pushnumber(L, mode);
		if(LuaCall(L, 1, 1) != 0) {
			// If the callback failed, return the original mode so the default autopilots can take over
			lua_settop(L, 0);
			return mode;
		} else {
			int retmode = (lua_tonumber(L, -1));
			lua_pop(L, 1);
			return retmode;
		}
	} else {
		return VESSEL4::clbkNavProcess(mode);
	}
}


// ==============================================================
// API callback interface
// ==============================================================

// --------------------------------------------------------------
// Vessel initialisation
// --------------------------------------------------------------
DLLCLBK VESSEL *ovcInit (OBJHANDLE hvessel, int flightmodel)
{
	return new ScriptVessel (hvessel, flightmodel);
}

// --------------------------------------------------------------
// Vessel cleanup
// --------------------------------------------------------------
DLLCLBK void ovcExit (VESSEL *vessel)
{
	if (vessel) delete (ScriptVessel*)vessel;
}

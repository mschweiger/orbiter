// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ==============================================================
//                 ORBITER MODULE: ScriptMFD
//                  Part of the ORBITER SDK
//
// ScriptMFD.cpp
//
// This module loads MFD modes defined via Lua scripts.
// ==============================================================

#define STRICT
#define ORBITER_MODULE
#include "windows.h"
#include "orbitersdk.h"
#include "ScriptMFD.h"

#undef DLLEXPORT
#define DLLEXPORT  // hack - this could be solved a bit more elegantly
#include "Interpreter.h"

using namespace std;

// ==============================================================
// Global variables

int g_MFDmode; // identifier for new MFD mode

SCRIPTMFDMODESPEC *modespec;
int nmode = 0;

struct VINTERP { // list of vessel-based interpreters
	INTERPRETERHANDLE hInterp;
	VESSEL *vessel;
	char *name;
	bool bprestep, bpoststep;
} **vinterp;
int nvinterp = 0;

static const char *cfgfile = "Config\\MFD\\ScriptMFD.cfg";

// clears the global list of vessel-based interpreters
static void ClearVinterpList()
{
	if (nvinterp) {
		for (int i = 0; i < nvinterp; i++) {
			oapiDelInterpreter (vinterp[i]->hInterp);
			if (vinterp[i]->name) delete []vinterp[i]->name;
			delete vinterp[i];
		}
		delete []vinterp;
		nvinterp = 0;
	}
}

static int traceback(lua_State *L) {
    lua_getfield(L, LUA_GLOBALSINDEX, "debug");
    lua_getfield(L, -1, "traceback");
    lua_pushvalue(L, 1);
    lua_pushinteger(L, 2);
    lua_call(L, 2, 1);
    return 1;
}

int LuaCall(lua_State *L, int narg, int nres)
{
	int base = lua_gettop(L) - narg;
	lua_pushcfunction(L, traceback);
	lua_insert(L, base);
	int res = lua_pcall(L, narg, nres, base);
	lua_remove(L, base);
	if(res != 0) {
		oapiWriteLogError("%s", lua_tostring(L, -1));
		oapiAddNotification(OAPINOTIF_ERROR, "Lua MFD error", lua_tostring(L, -1));
	}
	return res;
}

// ==============================================================
// API interface

DLLCLBK void InitModule (HINSTANCE hDLL)
{
	int i;
	char cbuf[256], name[256], script[256], key[256], persist[256];

	// Scan the list of script MFD modes
	ifstream ifs (cfgfile);
	while (ifs.getline (cbuf, 256)) {
		FILEHANDLE hFile = oapiOpenFile (cbuf, FILE_IN, CONFIG);
		if (oapiReadItem_string (hFile, (char*)"Name", name) &&
			oapiReadItem_string (hFile, (char*)"Script", script) &&
			oapiReadItem_string (hFile, (char*)"Key", key)) {
				SCRIPTMFDMODESPEC *tmp = new SCRIPTMFDMODESPEC[nmode+1];
				if (nmode) {
					memcpy (tmp, modespec, nmode*sizeof(SCRIPTMFDMODESPEC));
					delete []modespec;
				}
				modespec = tmp;
				modespec[nmode].name = new char[strlen(name)+1];
				strcpy (modespec[nmode].name, name);
				modespec[nmode].script = new char[strlen(script)+1];
				strcpy (modespec[nmode].script, script);
				modespec[nmode].key = 0;
				if (key[0] == '0' && toupper(key[1]) == 'X')
					sscanf (key+2, "%x", &modespec[nmode].key);
				else
					sscanf (key, "%d", &modespec[nmode].key);
				modespec[nmode].persist = 0;
				if (oapiReadItem_string (hFile, (char*)"Persist", persist))
					if (!_stricmp(persist, "vessel"))
						modespec[nmode].persist = 1;
				nmode++;
		}
		oapiCloseFile (hFile, FILE_IN);
	}

	MFDMODESPECEX spec;
	spec.msgproc = ScriptMFD::MsgProc;
	for (i = 0; i < nmode; i++) {
		spec.key = modespec[i].key;
		spec.name = modespec[i].name;
		spec.context = modespec+i;
		modespec[i].mode = oapiRegisterMFDMode (spec);
	}

	nvinterp = 0;
}

DLLCLBK void ExitModule (HINSTANCE hDLL)
{
	int i;
	for (i = 0; i < nmode; i++) {
		oapiUnregisterMFDMode (modespec[i].mode);
	}
	ClearVinterpList();
}

DLLCLBK void opcOpenRenderViewport(HWND,DWORD,DWORD,BOOL)
{
}

DLLCLBK void opcCloseRenderViewport ()
{
	ClearVinterpList();
}

DLLCLBK void opcPreStep (double simt, double simdt, double mjd)
{
	int i;
	for (i = 0; i < nvinterp; i++) {
		if (vinterp[i]->bprestep) {
			lua_State *L = oapiGetLua (vinterp[i]->hInterp);
			lua_getfield (L, LUA_GLOBALSINDEX, CLBKNAME[PRESTEP]);
			lua_pushnumber(L,simt);
			lua_pushnumber(L,simdt);
			lua_pushnumber(L,mjd);
			LuaCall (L, 3, 0);
		}
	}
}

DLLCLBK void opcPostStep (double simt, double simdt, double mjd)
{
	int i;
	for (i = 0; i < nvinterp; i++) {
		if (vinterp[i]->bpoststep) {
			lua_State *L = oapiGetLua (vinterp[i]->hInterp);
			lua_getfield (L, LUA_GLOBALSINDEX, CLBKNAME[POSTSTEP]);
			lua_pushnumber(L,simt);
			lua_pushnumber(L,simdt);
			lua_pushnumber(L,mjd);
			LuaCall (L, 3, 0);
		}
	}
}

// ==============================================================
// MFD class implementation

// Constructor
ScriptMFD::ScriptMFD (DWORD w, DWORD h, VESSEL *vessel, const SCRIPTMFDMODESPEC *spec)
: MFD2 (w, h, vessel)
{
	int i;
	char cmd[256];
	hInterp = 0;

	if (persist = spec->persist) { // link to vessel-based interpreter
		for (i = 0; i < nvinterp; i++) {
			if (vinterp[i]->vessel == vessel) {
				if (spec->name && vinterp[i]->name && !strcmp(spec->name, vinterp[i]->name)) {
					hInterp = vinterp[i]->hInterp;
					L = oapiGetLua (hInterp);
					// redefine interpreter 'mfd' object with current MFD instance
					Interpreter::lua_pushmfd (L, this);
					lua_setfield (L, LUA_GLOBALSINDEX, "mfd");
					break;
				}
			}
		}
	}

	if (!hInterp) {
		// create the interpreter instance
		hInterp = oapiCreateInterpreter();
		L = oapiGetLua (hInterp);

		// define the MFD instance
		Interpreter::lua_pushmfd (L, this);
		lua_setfield (L, LUA_GLOBALSINDEX, "mfd");

		// run the MFD script
		sprintf (cmd, "run_global('Config/MFD/%s')", spec->script);
		oapiExecScriptCmd (hInterp, cmd);

		if (persist) {
			VINTERP **tmp = new VINTERP*[nvinterp+1];
			if (nvinterp) {
				memcpy (tmp, vinterp, nvinterp*sizeof(VINTERP*));
				delete []vinterp;
			}
			vinterp = tmp;
			vinterp[nvinterp] = new VINTERP;
			vinterp[nvinterp]->hInterp = hInterp;
			vinterp[nvinterp]->vessel = vessel;
			if (spec->name) {
				vinterp[nvinterp]->name = new char[strlen(spec->name)+1];
				strcpy (vinterp[nvinterp]->name, spec->name);
			} else vinterp[nvinterp]->name = 0;
			strcpy (cmd, CLBKNAME[PRESTEP]);
			lua_getfield (L, LUA_GLOBALSINDEX, cmd);
			vinterp[nvinterp]->bprestep = (lua_isfunction(L,-1) != 0);
			lua_pop(L,1);
			strcpy (cmd, CLBKNAME[POSTSTEP]);
			lua_getfield (L, LUA_GLOBALSINDEX, cmd);
			vinterp[nvinterp]->bpoststep = (lua_isfunction(L,-1) != 0);
			lua_pop(L,1);
			nvinterp++;
		}
	}

	// check for defined callback functions in script
	for (i = 0; i < NCLBK; i++) {
		strcpy (cmd, CLBKNAME[i]);
		lua_getfield (L, LUA_GLOBALSINDEX, cmd);
		bclbk[i] = (lua_isfunction (L,-1) != 0);
		lua_pop(L,1);
	}

	if (bclbk[SETUP]) {
		lua_getfield (L, LUA_GLOBALSINDEX, CLBKNAME[SETUP]);
		lua_pushnumber(L, w);
		lua_pushnumber(L, h);
		LuaCall (L, 2, 0);
	}
}

// Destructor
ScriptMFD::~ScriptMFD ()
{
	if (!persist)
		oapiDelInterpreter (hInterp);
}

// React to a button press
bool ScriptMFD::ConsumeButton (int bt, int event)
{
	if (bclbk[CONSUMEBUTTON]) {
		lua_getfield (L, LUA_GLOBALSINDEX, CLBKNAME[CONSUMEBUTTON]);
		lua_pushnumber (L, bt);
		lua_pushnumber (L, event);
		LuaCall (L, 2, 1);
		bool consumed = (lua_toboolean (L, -1) ? true : false);
		lua_pop (L, 1);
		return consumed;
	}
	return MFD2::ConsumeButton (bt, event);
}

// React to a buffered key
bool ScriptMFD::ConsumeKeyBuffered (DWORD key)
{
	if (bclbk[CONSUMEKEYBUFFERED]) {
		lua_getfield (L, LUA_GLOBALSINDEX, CLBKNAME[CONSUMEKEYBUFFERED]);
		lua_pushnumber (L, key);
		LuaCall (L, 1, 1);
		bool consumed = (lua_toboolean (L, -1) ? true : false);
		lua_pop (L, 1);
		return consumed;
	}
	return MFD2::ConsumeKeyBuffered (key);
}

bool ScriptMFD::ConsumeKeyImmediate (char *kstate)
{
	if (bclbk[CONSUMEKEYIMMEDIATE]) {
		lua_getfield (L, LUA_GLOBALSINDEX, CLBKNAME[CONSUMEKEYIMMEDIATE]);
		lua_pushlightuserdata (L, kstate);
		LuaCall (L, 1, 1);
		bool consumed = (lua_toboolean (L, -1) ? true : false);
		lua_pop (L, 1);
		return consumed;
	}
	return MFD2::ConsumeKeyImmediate (kstate);
}

// Return button labels
char *ScriptMFD::ButtonLabel (int bt)
{
	char *label = 0;

	if (bclbk[BUTTONLABEL]) {
		lua_getfield (L, LUA_GLOBALSINDEX, CLBKNAME[BUTTONLABEL]);
		lua_pushnumber (L, bt);
		LuaCall (L, 1, 1);
		if (lua_isstring (L, -1)) {
			label = (char*)lua_tostring (L,-1);
		}
		lua_pop(L,1);
	}
	return label;
}

// Return button menus
int ScriptMFD::ButtonMenu (const MFDBUTTONMENU **menu) const
{
	int i, nbt = 0;

	if (bclbk[BUTTONMENU]) {
		static MFDBUTTONMENU *mnu = 0;
		static int nmnu = 0;
		lua_getfield (L, LUA_GLOBALSINDEX, CLBKNAME[BUTTONMENU]);
		LuaCall (L, 0, 2);
		if (lua_isnumber(L,-1)) {
			nbt = lua_tointeger(L,-1);
			if (menu) {
				if (nmnu) {
					for (i = 0; i < nmnu; i++) {
						if (mnu[i].line1) delete []mnu[i].line1;
						if (mnu[i].line2) delete []mnu[i].line2;
					}
					delete []mnu;
					nmnu = 0;
				}
				if (nbt) {
					mnu = new MFDBUTTONMENU[nmnu = nbt];
					for (i = 0; i < nbt; i++) {
						mnu[i].line1 = 0;
						mnu[i].line2 = 0;
						mnu[i].selchar = 'x';
					}
					if (lua_istable(L,-2)) {
						for (i = 0; i < nbt; i++) {
							lua_pushnumber(L,i+1);
							lua_gettable(L,-3);
							if (lua_istable (L,-1)) {
								lua_getfield(L,-1,"l1");
								if (lua_isstring(L,-1)) {
									const char *line = lua_tostring(L,-1);
									char *linebuf = new char[strlen(line)+1];
									strcpy (linebuf, line);
									mnu[i].line1 = linebuf;
								}
								lua_pop(L,1);
								lua_getfield(L,-1,"l2");
								if (lua_isstring(L,-1)) {
									const char *line = lua_tostring(L,-1);
									char *linebuf = new char[strlen(line)+1];
									strcpy (linebuf, line);
									mnu[i].line2 = linebuf;
								}
								lua_pop(L,1);
								lua_getfield(L,-1,"sel");
								if (lua_isstring(L,-1)) {
									const char *line = lua_tostring(L,-1);
									mnu[i].selchar = line[0];
								}
								lua_pop(L,1);
							}
							lua_pop(L,1);
						}
					}
				}
				*menu = mnu;
			}
		}
		lua_pop (L,2);
	}
	return nbt;
}


// Repaint the MFD
bool ScriptMFD::Update (oapi::Sketchpad *skp)
{
	if (bclbk[UPDATE]) {
		lua_getfield (L, LUA_GLOBALSINDEX, CLBKNAME[UPDATE]);
		Interpreter::lua_pushsketchpad (L, skp);
		LuaCall (L, 1, 1);
		bool consumed = (lua_toboolean (L, -1) ? true : false);
		lua_pop (L, 1);
		return true; //consumed;
	}
	return false;
}

// Store MFD status
void ScriptMFD::StoreStatus () const
{
	if (bclbk[STORESTATUS]) {
		lua_getfield (L, LUA_GLOBALSINDEX, CLBKNAME[STORESTATUS]);
		LuaCall (L, 0, 0);
	}
}

// Recall MFD status
void ScriptMFD::RecallStatus ()
{
	if (bclbk[RECALLSTATUS]) {
		lua_getfield (L, LUA_GLOBALSINDEX, CLBKNAME[RECALLSTATUS]);
		LuaCall (L, 0, 0);
	}
}

// Write MFD status to file
void ScriptMFD::WriteStatus (FILEHANDLE scn) const
{
	if (bclbk[WRITESTATUS]) {
		lua_getfield (L, LUA_GLOBALSINDEX, CLBKNAME[WRITESTATUS]);
		lua_pushlightuserdata (L, scn);
		LuaCall (L, 1, 0);
	}
}

// Read MFD status from file
void ScriptMFD::ReadStatus (FILEHANDLE scn)
{
	if (bclbk[READSTATUS]) {
		lua_getfield (L, LUA_GLOBALSINDEX, CLBKNAME[READSTATUS]);
		lua_pushlightuserdata (L, scn);
		LuaCall (L, 1, 0);
	}
}

// MFD message parser
OAPI_MSGTYPE ScriptMFD::MsgProc (UINT msg, UINT mfd, WPARAM wparam, LPARAM lparam)
{
	switch (msg) {
	case OAPI_MSG_MFD_OPENEDEX: {
		MFDMODEOPENSPEC* ospec = (MFDMODEOPENSPEC*)wparam;
		return (OAPI_MSGTYPE)new ScriptMFD(ospec->w, ospec->h, (VESSEL*)lparam, (const SCRIPTMFDMODESPEC*)ospec->spec->context);
		}
	}
	return 0;
}


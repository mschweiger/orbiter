// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#define INTERPRETER_IMPLEMENTATION

#include "Interpreter.h"
#include "VesselAPI.h"
#include "MFDAPI.h"
#include "DrawAPI.h"
#include "gcCoreAPI.h"

#include <list>

#include <lauxlib.h>

using std::min;
using std::max;

typedef struct {
	NTVERTEX *vtx;  // vertex array
	int nVtx;       // number of vertices in the array
	int nVtxUsed;
	bool owning;    // do we need to handle vtx memory
} ntv_data;

typedef struct {
	WORD *idx;  // vertex array
	int nIdx;       // number of vertices in the array
	int nIdxUsed;
	bool owning;    // do we need to handle vtx memory
} index_data;



std::list<NOTEHANDLE *> g_notehandles;

int OpenHelp (void *context);

// ============================================================================
// nonmember functions

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

// ============================================================================
// class Interpreter

Interpreter::Interpreter ()
{
	L = luaL_newstate();  // create new Lua context
	is_busy = false;      // waiting for input
	is_term = false;      // no attached terminal by default
	bExecLocal = false;   // flag for locally created mutexes
	bWaitLocal = false;
	jobs = 0;             // background jobs
	status = 0;           // normal
	term_verbose = 0;     // verbosity level
	postfunc = 0;
	postcontext = 0;
	// store interpreter context in the registry
	lua_pushlightuserdata (L, this);
	lua_setfield (L, LUA_REGISTRYINDEX, "interp");

	hExecMutex = CreateMutex (NULL, TRUE, NULL);
	hWaitMutex = CreateMutex (NULL, FALSE, NULL);

}

void Interpreter::LazyInitGCCore() {
	if(gcCoreInitialized) return;
	gcCoreInitialized = true;
	pCore = gcGetCoreInterface();
}

static int traceback(lua_State *L) {
    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "traceback");
    lua_pushvalue(L, 1);
    lua_pushinteger(L, 2);
    lua_call(L, 2, 1);
    return 1;
}

int Interpreter::LuaCall(lua_State *L, int narg, int nres)
{
	int base = lua_gettop(L) - narg;
	lua_pushcfunction(L, traceback);
	lua_insert(L, base);
	int res = lua_pcall(L, narg, nres, base);
	lua_remove(L, base);
	if(res != 0) {
		oapiWriteLogError("%s", lua_tostring(L, -1));
		oapiAnnotationSetText(errorbox, const_cast<char *>(lua_tostring(L, -1)));
	}
	return res;
}

Interpreter::~Interpreter ()
{
	lua_close (L);

	if (hExecMutex) CloseHandle (hExecMutex);
	if (hWaitMutex) CloseHandle (hWaitMutex);
}

void Interpreter::Initialise ()
{
	luaL_openlibs (L);    // load the default libraries
	LoadAPI ();           // load default set of API interface functions
	LoadVesselAPI ();     // load vessel-specific part of API
	LoadLightEmitterMethods (); // load light source methods
	LoadBeaconMethods ();
	LoadCustomCameraMethods ();
	LoadMFDAPI ();        // load MFD methods
	LoadNTVERTEXAPI();
	LoadBitAPI();         // load bit library
	LoadSketchpadAPI ();  // load Sketchpad methods
	LoadAnnotationAPI (); // load screen annotation methods
	LoadVesselStatusAPI ();
	LoadXRSoundAPI ();
	LoadStartupScript (); // load default initialisation script
}

int Interpreter::Status () const
{
	return status;
}

bool Interpreter::IsBusy () const
{
	return is_busy;
}

void Interpreter::Terminate ()
{
	status = 1;
}

void Interpreter::PostStep (double simt, double simdt, double mjd)
{
	if (postfunc) {
		postfunc (postcontext);
		postfunc = 0;
		postcontext = 0;
	}
}

int Interpreter::lua_tointeger_safe (lua_State *L, int idx, int prmno, const char *funcname)
{
	AssertPrmType(L, idx, prmno, PRMTP_NUMBER, funcname);
	return lua_tointeger(L, idx);
}

int Interpreter::lua_tointeger_safe (lua_State *L, int idx, const char *funcname)
{
	return lua_tointeger_safe (L, idx, idx, funcname);
}

int Interpreter::luamtd_tointeger_safe (lua_State *L, int idx, const char *funcname)
{
	return lua_tointeger_safe (L, idx, idx-1, funcname);
}

double Interpreter::lua_tonumber_safe (lua_State *L, int idx, int prmno, const char *funcname)
{
	AssertPrmType(L, idx, prmno, PRMTP_NUMBER, funcname);
	return lua_tonumber(L, idx);
}

double Interpreter::lua_tonumber_safe (lua_State *L, int idx, const char *funcname)
{
	return lua_tonumber_safe (L, idx, idx, funcname);
}

double Interpreter::luamtd_tonumber_safe (lua_State *L, int idx, const char *funcname)
{
	return lua_tonumber_safe (L, idx, idx-1, funcname);
}

bool Interpreter::lua_toboolean_safe (lua_State *L, int idx, int prmno, const char *funcname)
{
	AssertPrmType(L, idx, prmno, PRMTP_BOOLEAN, funcname);
	return lua_toboolean(L, idx) != 0;
}

bool Interpreter::lua_toboolean_safe (lua_State *L, int idx, const char *funcname)
{
	return lua_toboolean_safe (L, idx, idx, funcname);
}

bool Interpreter::luamtd_toboolean_safe (lua_State *L, int idx, const char *funcname)
{
	return lua_toboolean_safe (L, idx, idx-1, funcname);
}

const char *Interpreter::lua_tostring_safe (lua_State *L, int idx, int prmno, const char *funcname)
{
	AssertPrmType(L, idx, prmno, PRMTP_STRING, funcname);
	return lua_tostring(L, idx);
}

const char *Interpreter::lua_tostring_safe (lua_State *L, int idx, const char *funcname)
{
	return lua_tostring_safe (L, idx, idx, funcname);
}

const char *Interpreter::luamtd_tostring_safe (lua_State *L, int idx, const char *funcname)
{
	return lua_tostring_safe (L, idx, idx-1, funcname);
}

void *Interpreter::lua_tolightuserdata_safe (lua_State *L, int idx, int prmno, const char *funcname)
{
	AssertPrmType(L, idx, prmno, PRMTP_LIGHTUSERDATA, funcname);
	return lua_touserdata(L, idx);
}

void *Interpreter::lua_tolightuserdata_safe (lua_State *L, int idx, const char *funcname)
{
	return lua_tolightuserdata_safe (L, idx, idx, funcname);
}

void *Interpreter::luamtd_tolightuserdata_safe (lua_State *L, int idx, const char *funcname)
{
	return lua_tolightuserdata_safe (L, idx, idx-1, funcname);
}

VECTOR3 Interpreter::lua_tovector_safe (lua_State *L, int idx, int prmno, const char *funcname)
{
	AssertPrmType(L, idx, prmno, PRMTP_VECTOR, funcname);
	return lua_tovector(L, idx);
}

VECTOR3 Interpreter::lua_tovector_safe (lua_State *L, int idx, const char *funcname)
{
	return lua_tovector_safe (L, idx, idx, funcname);
}

VECTOR3 Interpreter::luamtd_tovector_safe (lua_State *L, int idx, const char *funcname)
{
	return lua_tovector_safe (L, idx, idx-1, funcname);
}

MATRIX3 Interpreter::lua_tomatrix_safe (lua_State *L, int idx, int prmno, const char *funcname)
{
	AssertPrmType(L, idx, prmno, PRMTP_MATRIX, funcname);
	return lua_tomatrix(L, idx);
}
MATRIX3 Interpreter::lua_tomatrix_safe (lua_State *L, int idx, const char *funcname)
{
	return lua_tomatrix_safe (L, idx, idx - 1, funcname);
}

MATRIX3 Interpreter::luamtd_tomatrix_safe (lua_State *L, int idx, const char *funcname)
{
	return lua_tomatrix_safe (L, idx, idx - 1, funcname);
}

double Interpreter::lua_field_tonumber_safe (lua_State *L, int idx, int prmno, const char *fieldname, const char *funcname)
{
	lua_getfield(L, idx, fieldname);
	AssertPrmType(L, -1, prmno, PRMTP_NUMBER, funcname, fieldname);
	double v = lua_tonumber(L, -1);
	lua_pop(L, 1);
	return v;
}

double Interpreter::lua_field_tonumber_safe (lua_State *L, int idx, const char *fieldname, const char *funcname)
{
	return lua_field_tonumber_safe (L, idx, idx, fieldname, funcname);
}

double Interpreter::luamtd_field_tonumber_safe (lua_State *L, int idx, const char *fieldname, const char *funcname)
{
	return lua_field_tonumber_safe (L, idx, idx-1, fieldname, funcname);
}

void *Interpreter::lua_field_tolightuserdata_safe (lua_State *L, int idx, int prmno, const char *fieldname, const char *funcname)
{
	lua_getfield(L, idx, fieldname);
	AssertPrmType(L, -1, prmno, PRMTP_LIGHTUSERDATA, funcname, fieldname);
	void *v = lua_touserdata(L, -1);
	lua_pop(L, 1);
	return v;
}

void *Interpreter::lua_field_tolightuserdata_safe (lua_State *L, int idx, const char *fieldname, const char *funcname)
{
	return lua_field_tolightuserdata_safe (L, idx, idx, fieldname, funcname);
}

void *Interpreter::luamtd_field_tolightuserdata_safe (lua_State *L, int idx, const char *fieldname, const char *funcname)
{
	return lua_field_tolightuserdata_safe (L, idx, idx-1, fieldname, funcname);
}

VECTOR3 Interpreter::lua_field_tovector_safe (lua_State *L, int idx, int prmno, const char *fieldname, const char *funcname)
{
	lua_getfield(L, idx, fieldname);
	AssertPrmType(L, -1, prmno, PRMTP_VECTOR, funcname, fieldname);
	VECTOR3 v = lua_tovector(L, -1);
	lua_pop(L, 1);
	return v;
}

VECTOR3 Interpreter::lua_field_tovector_safe (lua_State *L, int idx, const char *fieldname, const char *funcname)
{
	return lua_field_tovector_safe (L, idx, idx, fieldname, funcname);
}

VECTOR3 Interpreter::luamtd_field_tovector_safe (lua_State *L, int idx, const char *fieldname, const char *funcname)
{
	return lua_field_tovector_safe (L, idx, idx-1, fieldname, funcname);
}

const char *Interpreter::lua_tostringex (lua_State *L, int idx, char *cbuf)
{
	static char cbuf_loc[256];
	if (!cbuf) cbuf = cbuf_loc;
	const char *str = lua_tostring (L,idx);
	if (str) {
		return str;
	} else if (lua_isvector (L,idx)) {
		VECTOR3 v = lua_tovector (L,idx);
		sprintf (cbuf, "[%g %g %g]", v.x, v.y, v.z);
		return cbuf;
	} else if (lua_ismatrix (L,idx)) {
		MATRIX3 m = lua_tomatrix(L,idx);
		int i, len[9], lmax[3];
		for (i = 0; i < 9; i++) {
			sprintf (cbuf, "%g", m.data[i]);
			len[i] = strlen(cbuf);
		}
		lmax[0] = max(len[0], max(len[3], len[6]));
		lmax[1] = max(len[1], max(len[4], len[7]));
		lmax[2] = max(len[2], max(len[5], len[8]));

		sprintf (cbuf, "[%*g %*g %*g]\n[%*g %*g %*g]\n[%*g %*g %*g]",
			lmax[0], m.m11, lmax[1], m.m12, lmax[2], m.m13,
			lmax[0], m.m21, lmax[1], m.m22, lmax[2], m.m23,
			lmax[0], m.m31, lmax[1], m.m32, lmax[2], m.m33);
		return cbuf;
	}
	else if (lua_istouchdownvtx (L,idx)) {
		TOUCHDOWNVTX tdvx = lua_totouchdownvtx (L,idx);
		sprintf (cbuf, "{pos=[%g %g %g] stiffness=%g damping=%g mu=%g mu_lng=%g}",
			tdvx.pos.x, tdvx.pos.y, tdvx.pos.z,
			tdvx.stiffness, tdvx.damping, tdvx.mu, tdvx.mu_lng);
		return cbuf;
	} else if (lua_isnil (L,idx)) {
		strcpy (cbuf, "nil");
		return cbuf;
	} else if (lua_isboolean (L,idx)) {
		int res = lua_toboolean (L,idx);
		strcpy (cbuf, res ? "true":"false");
		return cbuf;
	} else if (lua_islightuserdata (L,idx)) {
		void *p = lua_touserdata(L,idx);
		sprintf (cbuf, "0x%08p [data]", p);
		return cbuf;
	} else if (lua_isuserdata (L,idx)) {
		void *p = lua_touserdata(L,idx);
		sprintf (cbuf, "0x%08p [object]", p);
		return cbuf;
	} else if (lua_istable (L, idx)) {
		if (idx < 0) idx--;
		lua_pushnil(L);  /* first key */
		static char tbuf[1024]; tbuf[0] = '\0';
		while (lua_next(L, idx) != 0) {
			/* uses 'key' (at index -2) and 'value' (at index -1) */
			char fieldstr[256] = "\0";
			if (lua_isstring(L,-2)) sprintf (fieldstr, "%s=", lua_tostring(L,-2));
			if(lua_istable(L, -1)) // cut the tree to prevent stack overflow with recursive table
				strcat (fieldstr, "[table]");
			else
				strcat (fieldstr, lua_tostringex (L,-1));
			strcat (tbuf, fieldstr); strcat (tbuf, "\n");
			lua_pop(L, 1);
		}
		return tbuf;
	} else if (lua_isfunction (L, idx)) {
		strcpy (cbuf, "[function]");
		return cbuf;
	} else {
		cbuf[0] = '\0';
		return cbuf;
	}
}

void Interpreter::lua_pushvector (lua_State *L, const VECTOR3 &vec)
{
	lua_createtable (L, 0, 3);
	lua_pushnumber (L, vec.x);
	lua_setfield (L, -2, "x");
	lua_pushnumber (L, vec.y);
	lua_setfield (L, -2, "y");
	lua_pushnumber (L, vec.z);
	lua_setfield (L, -2, "z");
}

int Interpreter::lua_isvector (lua_State *L, int idx)
{
	if (!lua_istable (L, idx)) return 0;
	static char fieldname[3] = {'x','y','z'};
	static char field[2] = "x";
	int i, ii, n;
	bool fail;

	lua_pushnil(L);
	ii = (idx >= 0 ? idx : idx-1);
	n = 0;
	while(lua_next(L,ii)) {
		lua_pop(L,1);
		n++;
	}
	if (n != 3) return 0;

	for (i = 0; i < 3; i++) {
		field[0] = fieldname[i];
		lua_getfield (L, idx, field);
		fail = (lua_isnil (L,-1));
		lua_pop (L,1);
		if (fail) return 0;
	}
	return 1;
}

void Interpreter::lua_pushmatrix (lua_State *L, const MATRIX3 &mat)
{
	lua_createtable(L,0,9);
	lua_pushnumber(L,mat.m11);  lua_setfield(L,-2,"m11");
	lua_pushnumber(L,mat.m12);  lua_setfield(L,-2,"m12");
	lua_pushnumber(L,mat.m13);  lua_setfield(L,-2,"m13");
	lua_pushnumber(L,mat.m21);  lua_setfield(L,-2,"m21");
	lua_pushnumber(L,mat.m22);  lua_setfield(L,-2,"m22");
	lua_pushnumber(L,mat.m23);  lua_setfield(L,-2,"m23");
	lua_pushnumber(L,mat.m31);  lua_setfield(L,-2,"m31");
	lua_pushnumber(L,mat.m32);  lua_setfield(L,-2,"m32");
	lua_pushnumber(L,mat.m33);  lua_setfield(L,-2,"m33");
}

MATRIX3 Interpreter::lua_tomatrix (lua_State *L, int idx)
{
	MATRIX3 mat;
	lua_getfield (L, idx, "m11");  mat.m11 = lua_tonumber (L, -1);  lua_pop (L,1);
	lua_getfield (L, idx, "m12");  mat.m12 = lua_tonumber (L, -1);  lua_pop (L,1);
	lua_getfield (L, idx, "m13");  mat.m13 = lua_tonumber (L, -1);  lua_pop (L,1);
	lua_getfield (L, idx, "m21");  mat.m21 = lua_tonumber (L, -1);  lua_pop (L,1);
	lua_getfield (L, idx, "m22");  mat.m22 = lua_tonumber (L, -1);  lua_pop (L,1);
	lua_getfield (L, idx, "m23");  mat.m23 = lua_tonumber (L, -1);  lua_pop (L,1);
	lua_getfield (L, idx, "m31");  mat.m31 = lua_tonumber (L, -1);  lua_pop (L,1);
	lua_getfield (L, idx, "m32");  mat.m32 = lua_tonumber (L, -1);  lua_pop (L,1);
	lua_getfield (L, idx, "m33");  mat.m33 = lua_tonumber (L, -1);  lua_pop (L,1);
	return mat;
}

int Interpreter::lua_ismatrix (lua_State *L, int idx)
{
	if (!lua_istable (L, idx)) return 0;
	static const char *fieldname[9] = {"m11","m12","m13","m21","m22","m23","m31","m32","m33"};
	int i, ii, n;
	bool fail;

	lua_pushnil(L);
	ii = (idx >= 0 ? idx : idx-1);
	n = 0;
	while(lua_next(L,ii)) {
		lua_pop(L,1);
		n++;
	}
	if (n != 9) return 0;

	for (i = 0; i < 9; i++) {
		lua_getfield (L, idx, fieldname[i]);
		fail = (lua_isnil (L,-1));
		lua_pop (L,1);
		if (fail) return 0;
	}
	return 1;
}

COLOUR4 Interpreter::lua_torgba (lua_State *L, int idx)
{
	COLOUR4 col = {0,0,0,0};
	lua_getfield (L, idx, "r");
	if (lua_isnumber(L,-1)) col.r = (float)lua_tonumber (L, -1);
	lua_pop (L,1);
	lua_getfield (L, idx, "g");
	if (lua_isnumber(L,-1)) col.g = (float)lua_tonumber (L, -1);
	lua_pop (L,1);
	lua_getfield (L, idx, "b");
	if (lua_isnumber(L,-1)) col.b = (float)lua_tonumber (L, -1);
	lua_pop (L,1);
	lua_getfield (L, idx, "a");
	if (lua_isnumber(L,-1)) col.a = (float)lua_tonumber (L, -1);
	lua_pop (L,1);
	return col;
}

void Interpreter::lua_pushrgba(lua_State* L, const COLOUR4& col)
{
	lua_createtable(L, 0, 4);
	lua_pushnumber(L, col.r);  lua_setfield(L, -2, "r");
	lua_pushnumber(L, col.g);  lua_setfield(L, -2, "g");
	lua_pushnumber(L, col.b);  lua_setfield(L, -2, "b");
	lua_pushnumber(L, col.a);  lua_setfield(L, -2, "a");
}

void Interpreter::lua_pushvessel (lua_State *L, VESSEL *v)
{
	lua_pushlightuserdata(L,v);         // use object pointer as key
	lua_gettable(L,LUA_REGISTRYINDEX);  // retrieve object from registry
	if (lua_isnil(L,-1)) {              // object not found
		lua_pop(L,1);                   // pop nil
		VESSEL **pv = (VESSEL**)lua_newuserdata(L,sizeof(VESSEL*));
		*pv = v;
		knownVessels.insert(v);
		luaL_getmetatable (L, "VESSEL.vtable"); // retrieve metatable
		lua_setmetatable (L,-2);             // and attach to new object
		LoadVesselExtensions(L,v);           // vessel environment
		lua_pushlightuserdata(L,v);          // create key
		lua_pushvalue(L,-2);                 // push object
		lua_settable(L,LUA_REGISTRYINDEX);   // and store in registry
		// note that now the object is on top of the stack
	}
}

void Interpreter::lua_pushmfd (lua_State *L, MFD2 *mfd)
{
	lua_pushlightuserdata(L,mfd);       // use object pointer as key
	lua_gettable(L,LUA_REGISTRYINDEX);  // retrieve object from registry
	if (lua_isnil(L,-1)) {              // object not found
		lua_pop(L,1);                   // pop nil
		MFD2 **pmfd = (MFD2**)lua_newuserdata(L,sizeof(MFD2*));
		*pmfd = mfd;
		luaL_getmetatable (L, "MFD.vtable"); // retrieve metatable
		lua_setmetatable (L,-2);             // and attach to new object
		lua_pushlightuserdata(L, mfd);       // create key
		lua_pushvalue(L,-2);                 // push object
		lua_settable(L, LUA_REGISTRYINDEX);  // and store in registry
		// note that now the object is on top of the stack
	}
}

MFD2 *Interpreter::lua_tomfd (lua_State *L, int idx)
{
	MFD2 **pmfd = (MFD2**)lua_touserdata(L,idx);
	return *pmfd;
}

#ifdef UNDEF
void Interpreter::lua_pushmfd (lua_State *L, MFD2 *mfd)
{
	lua_pushlightuserdata(L,mfd);
	//MFD2 **pm = (MFD2**)lua_newuserdata (L, sizeof(MFD*));
	//*pm = mfd;
	luaL_getmetatable (L, "MFD.vtable");
	lua_setmetatable (L, -2);
}
#endif

void Interpreter::lua_pushlightemitter (lua_State *L, const LightEmitter *le)
{
	lua_pushlightuserdata (L, (void*)le);   // use object pointer as key
	lua_gettable (L, LUA_REGISTRYINDEX);    // retrieve object from registry
	if (lua_isnil (L,-1)) {                 // object not found
		lua_pop (L,1);                      // pop nil
		const LightEmitter **ple = (const LightEmitter**)lua_newuserdata(L,sizeof(const LightEmitter*));
		*ple = le;
		luaL_getmetatable (L, "LightEmitter.vtable"); // retrieve metatable
		lua_setmetatable (L,-2);            // and attach to new object
		lua_pushlightuserdata (L, (void*)le);  // create key
		lua_pushvalue (L,-2);               // push object
		lua_settable (L,LUA_REGISTRYINDEX); // and store in registry
		// note that now the object is on top of the stack
	}
}

/***
numberref class: number reference.

This type is returned by a few functions to modify objects behavior after their creation.

@classmod numberref
@see oapi.particle_getlevelref
@see vessel:create_variabledragelement
@see vessel:add_particlestream
*/

/***
Get the value of a number reference.
@function get
@treturn number value of the reference
*/
static int numberref_get(lua_State* L)
{
	lua_Number* inst = (lua_Number*)luaL_checkudata(L, 1, "numberref");
	lua_pushnumber(L, *inst);
	return 1;
}

/***
Set the value of a number reference.
@function set
@tparam number value value to set
*/
static int numberref_set(lua_State* L)
{
	lua_Number* inst = (lua_Number*)luaL_checkudata(L, 1, "numberref");
	*inst = luaL_checknumber(L, 2);
	return 0;
}

int Interpreter::lua_pushnumberref(lua_State* L)
{
		lua_Number* ref = (lua_Number*)lua_newuserdata(L, sizeof(lua_Number));
		*ref = 0.0;
		if (luaL_newmetatable(L, "numberref")) {
			lua_pushstring(L, "__index");
			lua_pushvalue(L, -2); // push metatable
			lua_settable(L, -3);  // metatable.__index = metatable
			lua_pushcfunction(L, numberref_set);
			lua_setfield(L, -2, "set");
			lua_pushcfunction(L, numberref_get);
			lua_setfield(L, -2, "get");
			lua_pushstring(L, "numberref");
			lua_setfield(L, -2, "__metatable");
		}
		lua_setmetatable(L, -2);
		return 1;
}

LightEmitter *Interpreter::lua_tolightemitter (lua_State *L, int idx)
{
	LightEmitter **le = (LightEmitter**)lua_touserdata (L, idx);
	return *le;
}

void Interpreter::lua_pushsketchpad (lua_State *L, oapi::Sketchpad *skp)
{
	lua_pushlightuserdata(L,skp);       // use object pointer as key
	lua_gettable(L,LUA_REGISTRYINDEX);  // retrieve object from registry
	if (lua_isnil(L,-1)) {              // object not found
		lua_pop(L,1);                   // pop nil
		oapi::Sketchpad **pskp = (oapi::Sketchpad**)lua_newuserdata(L,sizeof(oapi::Sketchpad*));
		*pskp = skp;
		luaL_getmetatable (L, "SKP.vtable"); // retrieve metatable
		lua_setmetatable (L,-2);             // and attach to new object
		lua_pushlightuserdata(L,skp);        // create key
		lua_pushvalue(L,-2);                 // push object
		lua_settable(L, LUA_REGISTRYINDEX);  // and store in registry
		// note that now the object is on top of the stack
	}
#ifdef UNDEF
	//lua_pushlightuserdata(L,skp);
	oapi::Sketchpad **ps = (oapi::Sketchpad**)lua_newuserdata (L, sizeof(oapi::Sketchpad*));
	*ps = skp;
	luaL_getmetatable (L, "SKP.vtable");
	lua_setmetatable (L, -2);
#endif
}

void Interpreter::WaitExec (DWORD timeout)
{
	// Called by orbiter thread or interpreter thread to wait its turn
	// Orbiter waits for the script for 1 second to return
	WaitForSingleObject (hWaitMutex, timeout); // wait for synchronisation mutex
	WaitForSingleObject (hExecMutex, timeout); // wait for execution mutex
	ReleaseMutex (hWaitMutex);              // release synchronisation mutex
}

void Interpreter::EndExec ()
{
	// called by orbiter thread or interpreter thread to hand over control
	ReleaseMutex (hExecMutex);
}

void Interpreter::frameskip (lua_State *L)
{
	if (status == 1) { // termination request
		lua_pushboolean(L, 1);
		lua_setglobal (L, "wait_exit");
	} else {
		EndExec();
		WaitExec();
	}
}

int Interpreter::ProcessChunk (const char *chunk, int n)
{
	WaitExec();
	int res = RunChunk (chunk, n);
	EndExec();
	return res;
}

int Interpreter::RunChunk (const char *chunk, int n)
{
	int res = 0;
	if (chunk[0]) {
		is_busy = true;
		// run command
		luaL_loadbuffer (L, chunk, n, "line");
		res = LuaCall (L, 0, 0);
		if (res) {
			auto error = lua_tostring(L, -1);
			if (error) { // can be nullptr
				if (is_term) {
					// term_strout ("Execution error.");
					term_strout(error, true);
				}
				is_busy = false;
				return res;
			}
		}
		// check for leftover background jobs
		lua_getglobal(L, "_nbranch");
		LuaCall (L, 0, 1);
		jobs = lua_tointeger (L, -1);
		lua_pop (L, 1);
		is_busy = false;
	} else {
		// idle loop: execute background jobs
		lua_getglobal(L, "_idle");
		LuaCall (L, 0, 1);
		jobs = lua_tointeger (L, -1);
		lua_pop (L, 1);
		res = -1;
	}
	return res;
}

void Interpreter::term_out (lua_State *L, bool iserr)
{
	const char *str = lua_tostringex (L,-1);
	if (str) term_strout (str, iserr);
}

void Interpreter::LoadAPI ()
{
	// Load global functions
	static const struct luaL_Reg glob[] = {
		{"help", help},
		//{"api", help_api},
		{NULL, NULL}
	};
	for (int i = 0; i < ARRAYSIZE(glob) && glob[i].name; i++) {
		lua_pushcfunction (L, glob[i].func);
		lua_setglobal (L, glob[i].name);
	}

	// Load the vector library
	static const struct luaL_Reg vecLib[] = {
		{"set", vec_set},
		{"add", vec_add},
		{"sub", vec_sub},
		{"mul", vec_mul},
		{"div", vec_div},
		{"dotp", vec_dotp},
		{"crossp", vec_crossp},
		{"length", vec_length},
		{"dist", vec_dist},
		{"unit", vec_unit},
		{NULL, NULL}
	};
	luaL_newlib(L, vecLib);
	lua_setglobal(L, "vec");
	lua_getglobal(L, "vec");

	static const struct luaL_Reg matLib[] = {
		{"identity", mat_identity},
		{"mul", mat_mul},
		{"tmul", mat_tmul},
		{"mmul", mat_mmul},
		{"rotm", mat_rotm},
		{NULL, NULL}
	};
	luaL_newlib(L, matLib);
	lua_setglobal(L, "mat");
	lua_getglobal(L, "mat");

	// Load the process library
	static const struct luaL_Reg procLib[] = {
		{"Frameskip", procFrameskip},
		{NULL, NULL}
	};
	luaL_newlib(L, procLib);
	lua_setglobal(L, "proc");
	lua_getglobal(L, "proc");

	// Load the oapi library
	static const struct luaL_Reg oapiLib[] = {
		{"get_orbiter_version", oapi_get_orbiter_version},
		{"get_viewport_size", oapi_get_viewport_size},

		{"get_objhandle", oapiGetObjectHandle},
		{"get_objcount", oapiGetObjectCount},
		{"get_objname", oapiGetObjectName},
		{"create_annotation", oapiCreateAnnotation},
		{"del_annotation", oapiDelAnnotation},
		{"get_annotations", oapiGetAnnotations},
		{"dbg_out", oapiDbgOut},
		{"write_log", oapiWriteLog},
		{"open_help", oapiOpenHelp},
		{"exit", oapiExit},
		{"open_inputbox", oapiOpenInputBox},
		{"receive_input", oapiReceiveInput},
		{"open_inputboxex", oapi_open_inputboxex},
		{"del_vessel", oapi_del_vessel},
		{"create_vessel", oapi_create_vessel},
		{"set_focusobject", oapi_set_focusobject},

		{"get_rotationmatrix", oapi_get_rotationmatrix},

		// textures
		{"register_exhausttexture", oapi_register_exhausttexture},
		{"register_reentrytexture", oapi_register_reentrytexture},
		{"register_particletexture", oapi_register_particletexture},
		{"get_texturehandle", oapi_get_texturehandle},
		{"load_texture", oapi_load_texture},
		{"release_texture", oapi_release_texture},
		{"set_texture", oapi_set_texture},
		{"create_surface", oapi_create_surface},
		{"destroy_surface", oapi_destroy_surface},
		{"save_surface", oapi_save_surface},
		{"clear_surface", oapi_clear_surface},
		
		// GC
		{"set_materialex", oapi_set_materialex},
		{"set_material", oapi_set_material},
		{"set_meshproperty", oapi_set_meshproperty},

		// VC
		{"VC_trigger_redrawarea", oapi_VC_trigger_redrawarea},
		{"VC_set_areaclickmode_quadrilateral", oapi_VC_set_areaclickmode_quadrilateral},
		{"VC_set_areaclickmode_spherical", oapi_VC_set_areaclickmode_spherical},
		{"VC_register_area", oapi_VC_register_area},
		{"VC_set_neighbours", oapi_VC_set_neighbours},
		{"VC_registerHUD", oapi_VC_registerHUD},
		{"VC_registermfd", oapi_VC_registermfd},
		{"cockpit_mode", oapi_cockpit_mode},
		{"render_hud", oapi_render_hud},
		{"get_hudintensity", oapi_get_hudintensity},
		{"set_hudintensity", oapi_set_hudintensity},
		{"inc_hudintensity", oapi_inc_hudintensity},
		{"dec_hudintensity", oapi_dec_hudintensity},
		{"toggle_hudcolour", oapi_toggle_hudcolour},

		// time functions
		{"get_simtime", oapi_get_simtime},
		{"get_simstep", oapi_get_simstep},
		{"get_systime", oapi_get_systime},
		{"get_sysstep", oapi_get_sysstep},
		{"get_simmjd", oapi_get_simmjd},
		{"set_simmjd", oapi_set_simmjd},
		{"get_sysmjd", oapi_get_sysmjd},
		{"time2mjd", oapi_time2mjd},
		{"get_tacc", oapi_get_tacc},
		{"set_tacc", oapi_set_tacc},
		{"get_pause", oapi_get_pause},
		{"set_pause", oapi_set_pause},

		// menu functions
		{"get_mainmenuvisibilitymode", oapi_get_mainmenuvisibilitymode},
		{"set_mainmenuvisibilitymode", oapi_set_mainmenuvisibilitymode},
		{"get_maininfovisibilitymode", oapi_get_maininfovisibilitymode},
		{"set_maininfovisibilitymode", oapi_set_maininfovisibilitymode},

		// coordinate transformations
		{"global_to_equ", oapi_global_to_equ},
		{"global_to_local", oapi_global_to_local},
		{"local_to_equ", oapi_local_to_equ},
		{"equ_to_global", oapi_equ_to_global},
		{"orthodome", oapi_orthodome},

		// body functions
		{"get_size", oapi_get_size},
		{"get_mass", oapi_get_mass},
		{"get_globalpos", oapi_get_globalpos},
		{"get_globalvel", oapi_get_globalvel},
		{"get_relativepos", oapi_get_relativepos},
		{"get_relativevel", oapi_get_relativevel},

		// planet functions
		{"get_planetperiod", oapi_get_planetperiod},
		{"get_objecttype", oapi_get_objecttype},
		{"get_gbody", oapi_get_gbody},
		{"get_gbodycount", oapi_get_gbodycount},
		{"get_gbodyparent", oapi_get_gbodyparent},
		{"get_planetobliquity", oapi_get_planetobliquity},
		{"get_planettheta", oapi_get_planettheta},
		{"get_planetobliquitymatrix", oapi_get_planetobliquitymatrix},
		{"get_planetcurrentrotation", oapi_get_planetcurrentrotation},
		{"planet_hasatmosphere", oapi_planet_hasatmosphere},
		{"get_planetatmparams", oapi_get_planetatmparams},
		{"get_groundvector", oapi_get_groundvector},
		{"get_windvector", oapi_get_windvector},
		{"get_planetjcoeffcount", oapi_get_planetjcoeffcount},
		{"get_planetjcoeff", oapi_get_planetjcoeff},

		// vessel functions
		{"get_propellanthandle", oapi_get_propellanthandle},
		{"get_propellantmass", oapi_get_propellantmass},
		{"get_propellantmaxmass", oapi_get_propellantmaxmass},
		{"get_fuelmass", oapi_get_fuelmass},
		{"get_maxfuelmass", oapi_get_maxfuelmass},
		{"get_emptymass", oapi_get_emptymass},
		{"set_emptymass", oapi_set_emptymass},
		{"get_altitude", oapi_get_altitude},
		{"get_pitch", oapi_get_pitch},
		{"get_bank", oapi_get_bank},
		{"get_heading", oapi_get_heading},
		{"get_groundspeed", oapi_get_groundspeed},
		{"get_groundspeedvector", oapi_get_groundspeedvector},
		{"get_airspeed", oapi_get_airspeed},
		{"get_airspeedvector", oapi_get_airspeedvector},
		{"get_shipairspeedvector", oapi_get_shipairspeedvector},
		{"get_equpos", oapi_get_equpos},
		{"get_atm", oapi_get_atm},
		{"get_induceddrag", oapi_get_induceddrag},
		{"get_wavedrag", oapi_get_wavedrag},
		{"particle_getlevelref", oapi_particle_getlevelref},

		// Docking
		{"get_dockhandle", oapi_get_dockhandle},
		{"get_dockstatus", oapi_get_dockstatus},
		{"get_dockowner", oapi_get_dockowner},
		{"set_autocapture", oapi_set_autocapture},

		// Navigation radio transmitter functions
		{"get_navpos", oapi_get_navpos},
		{"get_navchannel", oapi_get_navchannel},
		{"get_navrange", oapi_get_navrange},
		{"get_navdata", oapi_get_navdata},
		{"get_navsignal", oapi_get_navsignal},
		{"get_navtype", oapi_get_navtype},

		// Camera functions
		{"set_cameramode", oapi_set_cameramode},
		{"get_cameratarget", oapi_get_cameratarget},
		{"set_cameratarget", oapi_set_cameratarget},
		{"get_cameraaperture", oapi_get_cameraaperture},
		{"set_cameraaperture", oapi_set_cameraaperture},
		{"get_cameraglobalpos", oapi_get_cameraglobalpos},
		{"get_cameraglobaldir", oapi_get_cameraglobaldir},
		{"move_groundcamera", oapi_move_groundcamera},
		{"set_cameracockpitdir", oapi_set_cameracockpitdir},
			
		// Custom camera
		{"setup_customcamera", oapi_setup_customcamera},
		{"delete_customcamera", oapi_delete_customcamera},
		{"customcamera_overlay", oapi_customcamera_overlay},
		{"customcamera_onoff", oapi_customcamera_onoff},

		// animation functions
		{"create_animationcomponent", oapi_create_animationcomponent},
		{"del_animationcomponent", oapi_del_animationcomponent},

		// instrument panel functions
		{"open_mfd", oapi_open_mfd},
		{"set_hudmode", oapi_set_hudmode},
		{"get_hudmode", oapi_get_hudmode},
		{"set_panelblink", oapi_set_panelblink },
		{"get_mfdmode", oapi_get_mfdmode },
		{"mfd_buttonlabel", oapi_mfd_buttonlabel },
		{"disable_mfdmode", oapi_disable_mfdmode },
		{"register_mfd", oapi_register_mfd },
		{"process_mfdbutton", oapi_process_mfdbutton },
		{"send_mfdkey", oapi_send_mfdkey },
		{"refresh_mfdbuttons", oapi_refresh_mfdbuttons },
		{"toggle_mfdon", oapi_toggle_mfdon },
		{"get_mfdmodespec", oapi_get_mfdmodespec },
		{"set_defnavdisplay", oapi_set_defnavdisplay },
		{"set_defrcsdisplay", oapi_set_defrcsdisplay },
			
		// user i/o functions
		{"keydown", oapi_keydown},
		{"resetkey", oapi_resetkey},
		{"simulatebufferedkey", oapi_simulatebufferedkey},
		{"simulateimmediatekey", oapi_simulateimmediatekey},
		{"acceptdelayedkey", oapi_acceptdelayedkey},
			
		// file i/o functions
		{"openfile", oapi_openfile},
		{"closefile", oapi_closefile},
		{"savescenario", oapi_savescenario},
		{"writeline", oapi_writeline},
		// {"writelog", oapi_writelog}, // see "write_log" above!
		// {"writelogv", oapi_writelogv}, //  ???
		{"writescenario_string", oapi_writescenario_string},
		{"writescenario_int", oapi_writescenario_int},
		{"writescenario_float", oapi_writescenario_float},
		{"writescenario_vec", oapi_writescenario_vec},
		{"readscenario_nextline", oapi_readscenario_nextline},
		{"readitem_string", oapi_readitem_string},
		{"readitem_float", oapi_readitem_float},
		{"readitem_int", oapi_readitem_int},
		{"readitem_bool", oapi_readitem_bool},
		{"readitem_vec", oapi_readitem_vec},
		{"writeitem_string", oapi_writeitem_string},
		{"writeitem_float", oapi_writeitem_float},
		{"writeitem_int", oapi_writeitem_int},
		{"writeitem_bool", oapi_writeitem_bool},
		{"writeitem_vec", oapi_writeitem_vec},

		// utility functions
		{"rand", oapi_rand},
		{"deflate", oapi_deflate},
		{"inflate", oapi_inflate},
		{"get_color", oapi_get_color},
		{"formatvalue", oapi_formatvalue},

		// sketchpad
		{"get_sketchpad", oapi_get_sketchpad },
		{"release_sketchpad", oapi_release_sketchpad },
		{"create_font", oapi_create_font },
		{"create_pen", oapi_create_pen },
		{"create_brush", oapi_create_brush },
		{"release_font", oapi_release_font },
		{"release_pen", oapi_release_pen },
		{"release_brush", oapi_release_brush },

		// Blt
		{"blt", oapi_blt },
		{"blt_panelareabackground", oapi_blt_panelareabackground },

		// Panel
		{"set_panelneighbours", oapi_set_panelneighbours },
		
		// mesh
		{"load_meshglobal", oapi_load_meshglobal },
		{"mesh_group", oapi_mesh_group },
		{"create_mesh", oapi_create_mesh },
		{"delete_mesh", oapi_delete_mesh },
		{"add_meshgroupblock", oapi_add_meshgroupblock },
		{"edit_meshgroup", oapi_edit_meshgroup },
		{"get_meshgroup", oapi_get_meshgroup },
			
		{"create_ntvertexarray", oapi_create_ntvertexarray },
		{"del_ntvertexarray", oapi_del_ntvertexarray },
		{"create_indexarray", oapi_create_indexarray },
		{"del_indexarray", oapi_del_indexarray },

		{"create_beacon", oapi_create_beacon },


		{NULL, NULL}
	};
	luaL_newlib(L, oapiLib);
	lua_setglobal(L, "oapi");
	lua_getglobal(L, "oapi");

	// Load the (dummy) term library
	static const struct luaL_Reg termLib[] = {
		{"out", termOut},
		{NULL, NULL}
	};
	luaL_newlib(L, termLib);
	lua_setglobal(L, "term");
	lua_getglobal(L, "term");

	// Load XRSound library
	static const struct luaL_Reg XRSoundLib[] = {
		{"create_instance", xrsound_create_instance},
		{NULL, NULL}
	};
	luaL_newlib(L, XRSoundLib);
	lua_setglobal(L, "xrsound");
	lua_getglobal(L, "xrsound");

	// Set up global tables of constants

	// Key ID table
	lua_createtable (L, 0, 100);
	lua_pushnumber (L, OAPI_KEY_ESCAPE);      lua_setfield (L, -2, "ESCAPE");
	lua_pushnumber (L, OAPI_KEY_1);           lua_setfield (L, -2, "1");
	lua_pushnumber (L, OAPI_KEY_2);           lua_setfield (L, -2, "2");
	lua_pushnumber (L, OAPI_KEY_3);           lua_setfield (L, -2, "3");
	lua_pushnumber (L, OAPI_KEY_4);           lua_setfield (L, -2, "4");
	lua_pushnumber (L, OAPI_KEY_5);           lua_setfield (L, -2, "5");
	lua_pushnumber (L, OAPI_KEY_6);           lua_setfield (L, -2, "6");
	lua_pushnumber (L, OAPI_KEY_7);           lua_setfield (L, -2, "7");
	lua_pushnumber (L, OAPI_KEY_8);           lua_setfield (L, -2, "8");
	lua_pushnumber (L, OAPI_KEY_9);           lua_setfield (L, -2, "9");
	lua_pushnumber (L, OAPI_KEY_0);           lua_setfield (L, -2, "0");
	// Duplicate numbers to have dot notation (OAPI_KEY.KEY1 instead of OAPI_KEY["1"])
	lua_pushnumber (L, OAPI_KEY_1);           lua_setfield (L, -2, "KEY1");
	lua_pushnumber (L, OAPI_KEY_2);           lua_setfield (L, -2, "KEY2");
	lua_pushnumber (L, OAPI_KEY_3);           lua_setfield (L, -2, "KEY3");
	lua_pushnumber (L, OAPI_KEY_4);           lua_setfield (L, -2, "KEY4");
	lua_pushnumber (L, OAPI_KEY_5);           lua_setfield (L, -2, "KEY5");
	lua_pushnumber (L, OAPI_KEY_6);           lua_setfield (L, -2, "KEY6");
	lua_pushnumber (L, OAPI_KEY_7);           lua_setfield (L, -2, "KEY7");
	lua_pushnumber (L, OAPI_KEY_8);           lua_setfield (L, -2, "KEY8");
	lua_pushnumber (L, OAPI_KEY_9);           lua_setfield (L, -2, "KEY9");
	lua_pushnumber (L, OAPI_KEY_0);           lua_setfield (L, -2, "KEY0");
	lua_pushnumber (L, OAPI_KEY_MINUS);       lua_setfield (L, -2, "MINUS");
	lua_pushnumber (L, OAPI_KEY_EQUALS);      lua_setfield (L, -2, "EQUALS");
	lua_pushnumber (L, OAPI_KEY_BACK);        lua_setfield (L, -2, "BACK");
	lua_pushnumber (L, OAPI_KEY_TAB);         lua_setfield (L, -2, "TAB");
	lua_pushnumber (L, OAPI_KEY_Q);           lua_setfield (L, -2, "Q");
	lua_pushnumber (L, OAPI_KEY_W);           lua_setfield (L, -2, "W");
	lua_pushnumber (L, OAPI_KEY_E);           lua_setfield (L, -2, "E");
	lua_pushnumber (L, OAPI_KEY_R);           lua_setfield (L, -2, "R");
	lua_pushnumber (L, OAPI_KEY_T);           lua_setfield (L, -2, "T");
	lua_pushnumber (L, OAPI_KEY_Y);           lua_setfield (L, -2, "Y");
	lua_pushnumber (L, OAPI_KEY_U);           lua_setfield (L, -2, "U");
	lua_pushnumber (L, OAPI_KEY_I);           lua_setfield (L, -2, "I");
	lua_pushnumber (L, OAPI_KEY_O);           lua_setfield (L, -2, "O");
	lua_pushnumber (L, OAPI_KEY_P);           lua_setfield (L, -2, "P");
	lua_pushnumber (L, OAPI_KEY_LBRACKET);    lua_setfield (L, -2, "LBRACKET");
	lua_pushnumber (L, OAPI_KEY_RBRACKET);    lua_setfield (L, -2, "RBRACKET");
	lua_pushnumber (L, OAPI_KEY_RETURN);      lua_setfield (L, -2, "RETURN");
	lua_pushnumber (L, OAPI_KEY_LCONTROL);    lua_setfield (L, -2, "LCONTROL");
	lua_pushnumber (L, OAPI_KEY_A);           lua_setfield (L, -2, "A");
	lua_pushnumber (L, OAPI_KEY_S);           lua_setfield (L, -2, "S");
	lua_pushnumber (L, OAPI_KEY_D);           lua_setfield (L, -2, "D");
	lua_pushnumber (L, OAPI_KEY_F);           lua_setfield (L, -2, "F");
	lua_pushnumber (L, OAPI_KEY_G);           lua_setfield (L, -2, "G");
	lua_pushnumber (L, OAPI_KEY_H);           lua_setfield (L, -2, "H");
	lua_pushnumber (L, OAPI_KEY_J);           lua_setfield (L, -2, "J");
	lua_pushnumber (L, OAPI_KEY_K);           lua_setfield (L, -2, "K");
	lua_pushnumber (L, OAPI_KEY_L);           lua_setfield (L, -2, "L");
	lua_pushnumber (L, OAPI_KEY_SEMICOLON);   lua_setfield (L, -2, "SEMICOLON");
	lua_pushnumber (L, OAPI_KEY_APOSTROPHE);  lua_setfield (L, -2, "APOSTROPHE");
	lua_pushnumber (L, OAPI_KEY_GRAVE);       lua_setfield (L, -2, "GRAVE");
	lua_pushnumber (L, OAPI_KEY_LSHIFT);      lua_setfield (L, -2, "LSHIFT");
	lua_pushnumber (L, OAPI_KEY_BACKSLASH);   lua_setfield (L, -2, "BACKSLASH");
	lua_pushnumber (L, OAPI_KEY_Z);           lua_setfield (L, -2, "Z");
	lua_pushnumber (L, OAPI_KEY_X);           lua_setfield (L, -2, "X");
	lua_pushnumber (L, OAPI_KEY_C);           lua_setfield (L, -2, "C");
	lua_pushnumber (L, OAPI_KEY_V);           lua_setfield (L, -2, "V");
	lua_pushnumber (L, OAPI_KEY_B);           lua_setfield (L, -2, "B");
	lua_pushnumber (L, OAPI_KEY_N);           lua_setfield (L, -2, "N");
	lua_pushnumber (L, OAPI_KEY_M);           lua_setfield (L, -2, "M");
	lua_pushnumber (L, OAPI_KEY_COMMA);       lua_setfield (L, -2, "COMMA");
	lua_pushnumber (L, OAPI_KEY_PERIOD);      lua_setfield (L, -2, "PERIOD");
	lua_pushnumber (L, OAPI_KEY_SLASH);       lua_setfield (L, -2, "SLASH");
	lua_pushnumber (L, OAPI_KEY_RSHIFT);      lua_setfield (L, -2, "RSHIFT");
	lua_pushnumber (L, OAPI_KEY_MULTIPLY);    lua_setfield (L, -2, "MULTIPLY");
	lua_pushnumber (L, OAPI_KEY_LALT);        lua_setfield (L, -2, "LALT");
	lua_pushnumber (L, OAPI_KEY_SPACE);       lua_setfield (L, -2, "SPACE");
	lua_pushnumber (L, OAPI_KEY_CAPITAL);     lua_setfield (L, -2, "CAPITAL");
	lua_pushnumber (L, OAPI_KEY_F1);          lua_setfield (L, -2, "F1");
	lua_pushnumber (L, OAPI_KEY_F2);          lua_setfield (L, -2, "F2");
	lua_pushnumber (L, OAPI_KEY_F3);          lua_setfield (L, -2, "F3");
	lua_pushnumber (L, OAPI_KEY_F4);          lua_setfield (L, -2, "F4");
	lua_pushnumber (L, OAPI_KEY_F5);          lua_setfield (L, -2, "F5");
	lua_pushnumber (L, OAPI_KEY_F6);          lua_setfield (L, -2, "F6");
	lua_pushnumber (L, OAPI_KEY_F7);          lua_setfield (L, -2, "F7");
	lua_pushnumber (L, OAPI_KEY_F8);          lua_setfield (L, -2, "F8");
	lua_pushnumber (L, OAPI_KEY_F9);          lua_setfield (L, -2, "F9");
	lua_pushnumber (L, OAPI_KEY_F10);         lua_setfield (L, -2, "F10");
	lua_pushnumber (L, OAPI_KEY_NUMLOCK);     lua_setfield (L, -2, "NUMLOCK");
	lua_pushnumber (L, OAPI_KEY_SCROLL);      lua_setfield (L, -2, "SCROLL");
	lua_pushnumber (L, OAPI_KEY_NUMPAD7);     lua_setfield (L, -2, "NUMPAD7");
	lua_pushnumber (L, OAPI_KEY_NUMPAD8);     lua_setfield (L, -2, "NUMPAD8");
	lua_pushnumber (L, OAPI_KEY_NUMPAD9);     lua_setfield (L, -2, "NUMPAD9");
	lua_pushnumber (L, OAPI_KEY_SUBTRACT);    lua_setfield (L, -2, "SUBTRACT");
	lua_pushnumber (L, OAPI_KEY_NUMPAD4);     lua_setfield (L, -2, "NUMPAD4");
	lua_pushnumber (L, OAPI_KEY_NUMPAD5);     lua_setfield (L, -2, "NUMPAD5");
	lua_pushnumber (L, OAPI_KEY_NUMPAD6);     lua_setfield (L, -2, "NUMPAD6");
	lua_pushnumber (L, OAPI_KEY_ADD);         lua_setfield (L, -2, "ADD");
	lua_pushnumber (L, OAPI_KEY_NUMPAD1);     lua_setfield (L, -2, "NUMPAD1");
	lua_pushnumber (L, OAPI_KEY_NUMPAD2);     lua_setfield (L, -2, "NUMPAD2");
	lua_pushnumber (L, OAPI_KEY_NUMPAD3);     lua_setfield (L, -2, "NUMPAD3");
	lua_pushnumber (L, OAPI_KEY_NUMPAD0);     lua_setfield (L, -2, "NUMPAD0");
	lua_pushnumber (L, OAPI_KEY_DECIMAL);     lua_setfield (L, -2, "DECIMAL");
	lua_pushnumber (L, OAPI_KEY_OEM_102);     lua_setfield (L, -2, "OEM_102");
	lua_pushnumber (L, OAPI_KEY_F11);         lua_setfield (L, -2, "F11");
	lua_pushnumber (L, OAPI_KEY_F12);         lua_setfield (L, -2, "F12");
	lua_pushnumber (L, OAPI_KEY_NUMPADENTER); lua_setfield (L, -2, "NUMPADENTER");
	lua_pushnumber (L, OAPI_KEY_RCONTROL);    lua_setfield (L, -2, "RCONTROL");
	lua_pushnumber (L, OAPI_KEY_DIVIDE);      lua_setfield (L, -2, "DIVIDE");
	lua_pushnumber (L, OAPI_KEY_RALT);        lua_setfield (L, -2, "RALT");
	lua_pushnumber (L, OAPI_KEY_HOME);        lua_setfield (L, -2, "HOME");
	lua_pushnumber (L, OAPI_KEY_UP);          lua_setfield (L, -2, "UP");
	lua_pushnumber (L, OAPI_KEY_PRIOR);       lua_setfield (L, -2, "PRIOR");
	lua_pushnumber (L, OAPI_KEY_LEFT);        lua_setfield (L, -2, "LEFT");
	lua_pushnumber (L, OAPI_KEY_RIGHT);       lua_setfield (L, -2, "RIGHT");
	lua_pushnumber (L, OAPI_KEY_END);         lua_setfield (L, -2, "END");
	lua_pushnumber (L, OAPI_KEY_DOWN);        lua_setfield (L, -2, "DOWN");
	lua_pushnumber (L, OAPI_KEY_NEXT);        lua_setfield (L, -2, "NEXT");
	lua_pushnumber (L, OAPI_KEY_INSERT);      lua_setfield (L, -2, "INSERT");
	lua_pushnumber (L, OAPI_KEY_DELETE);      lua_setfield (L, -2, "DELETE");
	lua_setglobal (L, "OAPI_KEY");

	// mouse event identifiers
	lua_createtable (L, 0, 11);
	lua_pushnumber (L, PANEL_MOUSE_IGNORE);   lua_setfield (L, -2, "IGNORE");
	lua_pushnumber (L, PANEL_MOUSE_LBDOWN);   lua_setfield (L, -2, "LBDOWN");
	lua_pushnumber (L, PANEL_MOUSE_RBDOWN);   lua_setfield (L, -2, "RBDOWN");
	lua_pushnumber (L, PANEL_MOUSE_LBUP);     lua_setfield (L, -2, "LBUP");
	lua_pushnumber (L, PANEL_MOUSE_RBUP);     lua_setfield (L, -2, "RBUP");
	lua_pushnumber (L, PANEL_MOUSE_LBPRESSED);lua_setfield (L, -2, "LBPRESSED");
	lua_pushnumber (L, PANEL_MOUSE_RBPRESSED);lua_setfield (L, -2, "RBPRESSED");
	lua_pushnumber (L, PANEL_MOUSE_DOWN);     lua_setfield (L, -2, "DOWN");
	lua_pushnumber (L, PANEL_MOUSE_UP);       lua_setfield (L, -2, "UP");
	lua_pushnumber (L, PANEL_MOUSE_PRESSED);  lua_setfield (L, -2, "PRESSED");
	lua_pushnumber (L, PANEL_MOUSE_ONREPLAY); lua_setfield (L, -2, "ONREPLAY");
	lua_setglobal (L, "PANEL_MOUSE");

	lua_createtable(L, 0, 6);
	lua_pushnumber(L, PANEL_REDRAW_NEVER);     lua_setfield(L, -2, "NEVER");
	lua_pushnumber(L, PANEL_REDRAW_ALWAYS);    lua_setfield(L, -2, "ALWAYS");
	lua_pushnumber(L, PANEL_REDRAW_MOUSE);     lua_setfield(L, -2, "MOUSE");
	lua_pushnumber(L, PANEL_REDRAW_INIT);      lua_setfield(L, -2, "INIT");
	lua_pushnumber(L, PANEL_REDRAW_USER);      lua_setfield(L, -2, "USER");
	lua_pushnumber(L, PANEL_REDRAW_SKETCHPAD); lua_setfield(L, -2, "SKETCHPAD");
	lua_setglobal(L, "PANEL_REDRAW");

	lua_createtable(L, 0, 5);
	lua_pushnumber(L, PANEL_MAP_NONE);         lua_setfield(L, -2, "NONE");
	lua_pushnumber(L, PANEL_MAP_BACKGROUND);   lua_setfield(L, -2, "BACKGROUND");
	lua_pushnumber(L, PANEL_MAP_CURRENT);      lua_setfield(L, -2, "CURRENT");
	lua_pushnumber(L, PANEL_MAP_BGONREQUEST);  lua_setfield(L, -2, "BGONREQUEST");
	lua_pushnumber(L, PANEL_MAP_DIRECT);       lua_setfield(L, -2, "DIRECT");
	lua_setglobal(L, "PANEL_MAP");

	lua_createtable(L, 0, 3);
	lua_pushnumber(L, COCKPIT_GENERIC);        lua_setfield(L, -2, "GENERIC");
	lua_pushnumber(L, COCKPIT_PANELS);         lua_setfield(L, -2, "PANELS");
	lua_pushnumber(L, COCKPIT_VIRTUAL);        lua_setfield(L, -2, "VIRTUAL");
	lua_setglobal(L, "COCKPIT");

	// HUD mode
	lua_createtable (L, 0, 4);
	lua_pushnumber (L, HUD_NONE);    lua_setfield (L, -2, "NONE");
	lua_pushnumber (L, HUD_ORBIT);   lua_setfield (L, -2, "ORBIT");
	lua_pushnumber (L, HUD_SURFACE); lua_setfield (L, -2, "SURFACE");
	lua_pushnumber (L, HUD_DOCKING); lua_setfield (L, -2, "DOCKING");
	lua_setglobal (L, "HUD");

	// frame of reference identifiers
	lua_createtable (L, 0, 4);
	lua_pushnumber (L, FRAME_GLOBAL);   lua_setfield (L, -2, "GLOBAL");
	lua_pushnumber (L, FRAME_LOCAL);    lua_setfield (L, -2, "LOCAL");
	lua_pushnumber (L, FRAME_REFLOCAL); lua_setfield (L, -2, "REFLOCAL");
	lua_pushnumber (L, FRAME_HORIZON);  lua_setfield (L, -2, "HORIZON");
	lua_setglobal (L, "REFFRAME");

	// altitude mode identifiers
	lua_createtable (L, 0, 2);
	lua_pushnumber (L, ALTMODE_MEANRAD); lua_setfield (L, -2, "MEANRAD");
	lua_pushnumber (L, ALTMODE_GROUND);  lua_setfield (L, -2, "GROUND");
	lua_setglobal (L, "ALTMODE");

	// file access mode identifiers
	lua_createtable(L, 0, 4);
	lua_pushnumber(L, FileAccessMode::FILE_IN);            lua_setfield(L, -2, "FILE_IN");
	lua_pushnumber(L, FileAccessMode::FILE_OUT);           lua_setfield(L, -2, "FILE_OUT");
	lua_pushnumber(L, FileAccessMode::FILE_APP);           lua_setfield(L, -2, "FILE_APP");
	lua_pushnumber(L, FileAccessMode::FILE_IN_ZEROONFAIL); lua_setfield(L, -2, "FILE_IN_ZEROONFAIL");
	lua_setglobal(L, "FILE_ACCESS_MODE");

	// path root identifiers
	lua_createtable(L, 0, 4);
	lua_pushnumber(L, PathRoot::ROOT);      lua_setfield(L, -2, "ROOT");
	lua_pushnumber(L, PathRoot::CONFIG);    lua_setfield(L, -2, "CONFIG");
	lua_pushnumber(L, PathRoot::SCENARIOS); lua_setfield(L, -2, "SCENARIOS");
	lua_pushnumber(L, PathRoot::TEXTURES);  lua_setfield(L, -2, "TEXTURES");
	lua_pushnumber(L, PathRoot::TEXTURES2); lua_setfield(L, -2, "TEXTURES2");
	lua_pushnumber(L, PathRoot::MESHES);    lua_setfield(L, -2, "MESHES");
	lua_pushnumber(L, PathRoot::MODULES);   lua_setfield(L, -2, "MODULES");
	lua_setglobal(L, "PATH_ROOT");

	// metatables for userdata checks
	luaL_newmetatable(L, "DEVMESHHANDLE"); lua_pop(L, 1);
	luaL_newmetatable(L, "MESHHANDLE"); lua_pop(L, 1);

	// Fonts
	lua_createtable (L, 0, 7);
	lua_pushnumber (L, FONT_NORMAL);    lua_setfield (L, -2, "NORMAL");
	lua_pushnumber (L, FONT_BOLD);      lua_setfield (L, -2, "BOLD");
	lua_pushnumber (L, FONT_ITALIC);    lua_setfield (L, -2, "ITALIC");
	lua_pushnumber (L, FONT_UNDERLINE); lua_setfield (L, -2, "UNDERLINE");
	lua_pushnumber (L, FONT_STRIKEOUT); lua_setfield (L, -2, "STRIKEOUT");
	lua_pushnumber (L, FONT_CRISP);     lua_setfield (L, -2, "CRISP");
	lua_pushnumber (L, FONT_ANTIALIAS); lua_setfield (L, -2, "ANTIALIAS");
	lua_setglobal (L, "FONT");

	// Surface
	lua_createtable (L, 0, 12);
	lua_pushnumber (L, OAPISURFACE_TEXTURE     ); lua_setfield (L, -2, "TEXTURE");
	lua_pushnumber (L, OAPISURFACE_RENDERTARGET); lua_setfield (L, -2, "RENDERTARGET");
	lua_pushnumber (L, OAPISURFACE_SKETCHPAD   ); lua_setfield (L, -2, "SKETCHPAD");
	lua_pushnumber (L, OAPISURFACE_MIPMAPS     ); lua_setfield (L, -2, "MIPMAPS");
	lua_pushnumber (L, OAPISURFACE_NOMIPMAPS   ); lua_setfield (L, -2, "NOMIPMAPS");
	lua_pushnumber (L, OAPISURFACE_ALPHA       ); lua_setfield (L, -2, "ALPHA");
	lua_pushnumber (L, OAPISURFACE_NOALPHA     ); lua_setfield (L, -2, "NOALPHA");
	lua_pushnumber (L, OAPISURFACE_UNCOMPRESS  ); lua_setfield (L, -2, "UNCOMPRESS");
	lua_pushnumber (L, OAPISURFACE_SYSMEM      ); lua_setfield (L, -2, "SYSMEM");
	lua_pushnumber (L, OAPISURFACE_RENDER3D    ); lua_setfield (L, -2, "RENDER3D");
	lua_pushnumber (L, OAPISURFACE_ANTIALIAS   ); lua_setfield (L, -2, "ANTIALIAS");
	lua_pushnumber (L, OAPISURFACE_SHARED      ); lua_setfield (L, -2, "SHARED");
	lua_setglobal (L, "OAPISURFACE");

	// GROUP EDIT
	lua_createtable (L, 0, 28);
	lua_pushnumber (L, GRPEDIT_SETUSERFLAG); lua_setfield (L, -2, "SETUSERFLAG");
	lua_pushnumber (L, GRPEDIT_ADDUSERFLAG); lua_setfield (L, -2, "ADDUSERFLAG");
	lua_pushnumber (L, GRPEDIT_DELUSERFLAG); lua_setfield (L, -2, "DELUSERFLAG");
	lua_pushnumber (L, GRPEDIT_VTXCRDX    ); lua_setfield (L, -2, "VTXCRDX");
	lua_pushnumber (L, GRPEDIT_VTXCRDY    ); lua_setfield (L, -2, "VTXCRDY");
	lua_pushnumber (L, GRPEDIT_VTXCRDZ    ); lua_setfield (L, -2, "VTXCRDZ");
	lua_pushnumber (L, GRPEDIT_VTXCRD     ); lua_setfield (L, -2, "VTXCRD");
	lua_pushnumber (L, GRPEDIT_VTXNMLX    ); lua_setfield (L, -2, "VTXNMLX");
	lua_pushnumber (L, GRPEDIT_VTXNMLY    ); lua_setfield (L, -2, "VTXNMLY");
	lua_pushnumber (L, GRPEDIT_VTXNMLZ    ); lua_setfield (L, -2, "VTXNMLZ");
	lua_pushnumber (L, GRPEDIT_VTXNML     ); lua_setfield (L, -2, "VTXNML");
	lua_pushnumber (L, GRPEDIT_VTXTEXU    ); lua_setfield (L, -2, "VTXTEXU");
	lua_pushnumber (L, GRPEDIT_VTXTEXV    ); lua_setfield (L, -2, "VTXTEXV");
	lua_pushnumber (L, GRPEDIT_VTXTEX     ); lua_setfield (L, -2, "VTXTEX");
	lua_pushnumber (L, GRPEDIT_VTX        ); lua_setfield (L, -2, "VTX");
	lua_pushnumber (L, GRPEDIT_VTXCRDADDX ); lua_setfield (L, -2, "VTXCRDADDX");
	lua_pushnumber (L, GRPEDIT_VTXCRDADDY ); lua_setfield (L, -2, "VTXCRDADDY");
	lua_pushnumber (L, GRPEDIT_VTXCRDADDZ ); lua_setfield (L, -2, "VTXCRDADDZ");
	lua_pushnumber (L, GRPEDIT_VTXCRDADD  ); lua_setfield (L, -2, "VTXCRDADD");
	lua_pushnumber (L, GRPEDIT_VTXNMLADDX ); lua_setfield (L, -2, "VTXNMLADDX");
	lua_pushnumber (L, GRPEDIT_VTXNMLADDY ); lua_setfield (L, -2, "VTXNMLADDY");
	lua_pushnumber (L, GRPEDIT_VTXNMLADDZ ); lua_setfield (L, -2, "VTXNMLADDZ");
	lua_pushnumber (L, GRPEDIT_VTXNMLADD  ); lua_setfield (L, -2, "VTXNMLADD");
	lua_pushnumber (L, GRPEDIT_VTXTEXADDU ); lua_setfield (L, -2, "VTXTEXADDU");
	lua_pushnumber (L, GRPEDIT_VTXTEXADDV ); lua_setfield (L, -2, "VTXTEXADDV");
	lua_pushnumber (L, GRPEDIT_VTXTEXADD  ); lua_setfield (L, -2, "VTXTEXADD");
	lua_setglobal (L, "GRPEDIT");

	lua_createtable (L, 0, 5);
	lua_pushnumber (L, oapi::ImageFileFormat::IMAGE_BMP); lua_setfield (L, -2, "BMP");
	lua_pushnumber (L, oapi::ImageFileFormat::IMAGE_PNG); lua_setfield (L, -2, "PNG");
	lua_pushnumber (L, oapi::ImageFileFormat::IMAGE_JPG); lua_setfield (L, -2, "JPG");
	lua_pushnumber (L, oapi::ImageFileFormat::IMAGE_TIF); lua_setfield (L, -2, "TIF");
	lua_pushnumber (L, oapi::ImageFileFormat::IMAGE_DDS); lua_setfield (L, -2, "DDS");
	lua_setglobal (L, "IMAGEFORMAT");

	lua_createtable (L, 0, 7);
	lua_pushnumber (L, OBJTP_INVALID); lua_setfield (L, -2, "INVALID");
	lua_pushnumber (L, OBJTP_GENERIC); lua_setfield (L, -2, "GENERIC");
	lua_pushnumber (L, OBJTP_CBODY); lua_setfield (L, -2, "CBODY");
	lua_pushnumber (L, OBJTP_STAR); lua_setfield (L, -2, "STAR");
	lua_pushnumber (L, OBJTP_PLANET); lua_setfield (L, -2, "PLANET");
	lua_pushnumber (L, OBJTP_VESSEL); lua_setfield (L, -2, "VESSEL");
	lua_pushnumber (L, OBJTP_SURFBASE); lua_setfield (L, -2, "SURFBASE");
	lua_setglobal (L, "OBJTP");

	lua_createtable (L, 0, 1);
	lua_pushnumber (L, USRINPUT_NEEDANSWER); lua_setfield (L, -2, "NEEDANSWER");
	lua_setglobal (L, "USRINPUT");

	lua_createtable (L, 0, 1);
	lua_pushnumber (L, MESHPROPERTY_MODULATEMATALPHA); lua_setfield (L, -2, "MODULATEMATALPHA");
	lua_setglobal (L, "MESHPROPERTY");
}

void Interpreter::LoadMFDAPI ()
{
	static const struct luaL_Reg mfdLib[] = {
		{"get_size", mfd_get_size},
		{"set_title", mfd_set_title},
		{"get_defaultpen", mfd_get_defaultpen},
		{"get_defaultfont", mfd_get_defaultfont},
		{"invalidate_display", mfd_invalidate_display},
		{"invalidate_buttons", mfd_invalidate_buttons},
		{NULL, NULL}
	};

	luaL_newmetatable (L, "MFD.vtable");
	lua_pushstring (L, "__index");
	lua_pushvalue (L, -2); // push metatable
	lua_settable (L, -3);  // metatable.__index = metatable
	luaL_setfuncs(L, mfdLib, 0);
}

void Interpreter::LoadNTVERTEXAPI ()
{
	static const struct luaL_Reg ntvLib[] = {
		{"size", ntv_size},
		{"extract", ntv_extract},
		{"reset", ntv_reset},
		{"zeroize", ntv_zeroize},
		{"append", ntv_append},
		{"copy", ntv_copy},
		{"write", ntv_write},
		{"view", ntv_view},
		{"__gc", ntv_collect},
		{"__index", ntv_get},
		{"__newindex", ntv_set},
		{"__len", ntv_size},
		{NULL, NULL}
	};

	luaL_newmetatable (L, "NTV.vtable");
	lua_pushstring (L, "__index");
	lua_pushvalue (L, -2); // push metatable
	lua_settable (L, -3);  // metatable.__index = metatable
	luaL_setfuncs(L, ntvLib, 0);

      /* now the stack has the metatable at index 1 and
         `array' at index 2 */
#if 0
      lua_pushstring(L, "__index");
      lua_pushstring(L, "get");
      lua_gettable(L, 2);  /* get array.get */
      lua_settable(L, 1);  /* metatable.__index = array.get */
    
      lua_pushstring(L, "__newindex");
      lua_pushstring(L, "set");
      lua_gettable(L, 2); /* get array.set */
      lua_settable(L, 1); /* metatable.__newindex = array.set */

      lua_pushstring(L, "__len");
      lua_pushstring(L, "size");
      lua_gettable(L, 2); /* get array.set */
      lua_settable(L, 1); /* metatable.__newindex = array.set */
#endif
	
	luaL_newmetatable(L, "NTVPROXY.vtable");
	lua_pushcfunction(L, ntvproxy_set);
	lua_setfield(L, -2, "__newindex");
	lua_pushcfunction(L, ntvproxy_get);
	lua_setfield(L, -2, "__index");
	lua_pushstring(L, "NTVPROXY.vtable");
	lua_setfield(L, -2, "__metatable");
	lua_pop(L, 1);
	
	static const struct luaL_Reg idxLib[] = {
		{"size", idx_size},
		{"reset", idx_reset},
		{"append", idx_append},
		{"set", idx_set},
		{"get", idx_get},
		{"__gc", idx_collect},
		{"__index", idx_get},
		{"__newindex", idx_set},
		{"__len", idx_size},
		{NULL, NULL}
	};

	luaL_newmetatable (L, "Index.vtable");
	lua_pushstring (L, "__index");
	lua_pushvalue (L, -2); // push metatable
	lua_settable (L, -3);  // metatable.__index = metatable
	luaL_setfuncs(L, idxLib, 0);
}

void Interpreter::LoadBitAPI()
{
	// Load the bit library
	static const struct luaL_Reg bitLib[] = {
		{"anyset", bit_anyset},
		{"allset", bit_allset},
		{"band", bit_and},
		{"bor", bit_or},
		{"bxor", bit_xor},
		{"bnot", bit_not},
		{"mask", bit_mask},
		{"lshift", bit_lshift},
		{"rshift", bit_rshift},
		{"arshift", bit_arshift},
		{"rol", bit_rol},
		{"ror", bit_ror},
		{NULL, NULL}
	};
	luaL_newlib(L, bitLib);
	lua_setglobal(L, "bit");
	lua_getglobal(L, "bit");
}

void Interpreter::LoadLightEmitterMethods ()
{
	static const struct luaL_Reg methodLib[] = {
		{"get_position", le_get_position},
		{"set_position", le_set_position},
		{"get_direction", le_get_direction},
		{"set_direction", le_set_direction},
		{"get_intensity", le_get_intensity},
		{"set_intensity", le_set_intensity},
		{"get_range", le_get_range},
		{"set_range", le_set_range},
		{"get_attenuation", le_get_attenuation},
		{"set_attenuation", le_set_attenuation},
		{"get_spotaperture", le_get_spotaperture},
		{"set_spotaperture", le_set_spotaperture},
		{"activate", le_activate},
		{"is_active", le_is_active},
		{"get_visibility", le_get_visibility},
		{"set_visibility", le_set_visibility},
		{NULL, NULL}
	};

	luaL_newmetatable (L, "LightEmitter.vtable");
	lua_pushstring (L, "__index");
	lua_pushvalue (L, -2); // push metatable
	lua_settable (L, -3); // metatable.__index = metatable
	luaL_setfuncs(L, methodLib, 0);

	lua_createtable(L, 0, 3);
	lua_pushnumber(L, LightEmitter::VIS_EXTERNAL); lua_setfield(L, -2, "EXTERNAL");
	lua_pushnumber(L, LightEmitter::VIS_COCKPIT);  lua_setfield(L, -2, "COCKPIT");
	lua_pushnumber(L, LightEmitter::VIS_ALWAYS);lua_setfield(L, -2, "ALWAYS");
	lua_setglobal(L, "VIS");
}

void Interpreter::LoadBeaconMethods ()
{
	static const struct luaL_Reg beaconLib[] = {
		{"__gc", beacon_collect},
		{"__index", beacon_get},
		{"__newindex", beacon_set},
		{NULL, NULL}
	};

	luaL_newmetatable (L, "Beacon.vtable");
	lua_pushstring (L, "__index");
	lua_pushvalue (L, -2); // push metatable
	lua_settable (L, -3);  // metatable.__index = metatable
	luaL_setfuncs(L, beaconLib, 0);

	lua_createtable(L, 0, 3);
	lua_pushnumber(L, BEACONSHAPE_COMPACT); lua_setfield(L, -2, "COMPACT");
	lua_pushnumber(L, BEACONSHAPE_DIFFUSE); lua_setfield(L, -2, "DIFFUSE");
	lua_pushnumber(L, BEACONSHAPE_STAR);	lua_setfield(L, -2, "STAR");
	lua_setglobal(L, "BEACONSHAPE");
}


void Interpreter::LoadCustomCameraMethods ()
{
	static const struct luaL_Reg CustomCameraLib[] = {
		{"__gc", customcamera_collect},
		{NULL, NULL}
	};

	luaL_newmetatable (L, "CustomCamera.vtable");
	lua_pushstring (L, "__index");
	lua_pushvalue (L, -2); // push metatable
	lua_settable (L, -3);  // metatable.__index = metatable
	luaL_setfuncs(L, CustomCameraLib, 0);
}


void Interpreter::LoadSketchpadAPI ()
{
	static const struct luaL_Reg skpLib[] = {
		{"text", skp_text},
		{"moveto", skp_moveto},
		{"lineto", skp_lineto},
		{"line", skp_line},
		{"rectangle", skp_rectangle},
		{"ellipse", skp_ellipse},
		{"polygon", skp_polygon},
		{"polyline", skp_polyline},
		{"set_origin", skp_set_origin},
		{"set_textalign", skp_set_textalign},
		{"set_textcolor", skp_set_textcolor},
		{"set_backgroundcolor", skp_set_backgroundcolor},
		{"set_backgroundmode", skp_set_backgroundmode},
		{"set_pen", skp_set_pen},
		{"set_font", skp_set_font},
		{"set_brush", skp_set_brush},
		{"get_charsize", skp_get_charsize},
		{"get_textwidth", skp_get_textwidth},
		{"copy_rect", skp_copy_rect},
		{"stretch_rect", skp_stretch_rect},
		{"rotate_rect", skp_rotate_rect},
		{"quick_pen", skp_quick_pen},
		{"quick_brush", skp_quick_brush},
		{"get_surface", skp_get_surface},
		{"set_brightness", skp_set_brightness},
		{"set_renderparam", skp_set_renderparam},
		{"set_worldtransform2d", skp_set_worldtransform2d},
		{NULL, NULL}
	};

	luaL_newmetatable (L, "SKP.vtable");
	lua_pushstring (L, "__index");
	lua_pushvalue (L, -2); // push metatable
	lua_settable (L, -3); // metatable.__index = metatable
	luaL_setfuncs(L, skpLib, 0);

	lua_createtable (L, 0, 8);
	lua_pushnumber (L, oapi::Sketchpad::BK_OPAQUE);      lua_setfield (L, -2, "OPAQUE");
	lua_pushnumber (L, oapi::Sketchpad::BK_TRANSPARENT); lua_setfield (L, -2, "TRANSPARENT");
	lua_pushnumber (L, oapi::Sketchpad::LEFT);           lua_setfield (L, -2, "LEFT");
	lua_pushnumber (L, oapi::Sketchpad::CENTER);         lua_setfield (L, -2, "CENTER");
	lua_pushnumber (L, oapi::Sketchpad::RIGHT);          lua_setfield (L, -2, "RIGHT");
	lua_pushnumber (L, oapi::Sketchpad::TOP);            lua_setfield (L, -2, "TOP");
	lua_pushnumber (L, oapi::Sketchpad::BASELINE);       lua_setfield (L, -2, "BASELINE");
	lua_pushnumber (L, oapi::Sketchpad::BOTTOM);         lua_setfield (L, -2, "BOTTOM");
	lua_setglobal (L, "SKP");

	lua_createtable (L, 0, 2);
	lua_pushnumber (L, oapi::Sketchpad::PRM_GAMMA);      lua_setfield (L, -2, "GAMMA");
	lua_pushnumber (L, oapi::Sketchpad::PRM_NOISE);      lua_setfield (L, -2, "NOISE");
	lua_setglobal (L, "PRM");
}

void Interpreter::LoadAnnotationAPI ()
{
	static const struct luaL_Reg noteMtd[] = {
		{"set_text", noteSetText},
		{"set_pos", noteSetPos},
		{"set_size", noteSetSize},
		{"set_colour", noteSetColour},
		{NULL, NULL}
	};
	luaL_newmetatable (L, "NOTE.table");
	lua_pushstring (L, "__index");
	lua_pushvalue (L, -2); // push metatable
	lua_settable (L, -3);  // metatable.__index = metatable
	luaL_setfuncs(L, noteMtd, 0);
}

void Interpreter::LoadVesselStatusAPI()
{
	static const struct luaL_Reg vs[] = {
		{"get", vsget},
		{"set", vsset},
		{NULL, NULL}
	};
	luaL_newmetatable(L, "VESSELSTATUS.table");
	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2); // push metatable
	lua_settable(L, -3);  // metatable.__index = metatable
	luaL_setfuncs(L, vs, 0);

	static const struct luaL_Reg vs2[] = {
		{"get", vs2get},
		{"set", vs2set},
		{NULL, NULL}
	};
	luaL_newmetatable(L, "VESSELSTATUS2.table");
	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2); // push metatable
	lua_settable(L, -3);  // metatable.__index = metatable
	luaL_setfuncs(L, vs2, 0);
}

void Interpreter::LoadStartupScript ()
{
	luaL_dofile (L, "./Script/oapi_init.lua");
}

bool Interpreter::InitialiseVessel (lua_State *L, VESSEL *v)
{
	if (v->Version() < 2) return false;
	VESSEL3 *v3 = (VESSEL3*)v;
	return (v3->clbkGeneric (VMSG_LUAINTERPRETER, 0, (void*)L) != 0);
}

bool Interpreter::LoadVesselExtensions (lua_State *L, VESSEL *v)
{
	if (v->Version() < 2) return false;
	VESSEL3 *v3 = (VESSEL3*)v;
	return (v3->clbkGeneric (VMSG_LUAINSTANCE, 0, (void*)L) != 0);
}

void Interpreter::DeleteVessel (OBJHANDLE hVessel)
{
	VESSEL *v = oapiGetVesselInterface(hVessel);
	knownVessels.erase(v);
}

Interpreter *Interpreter::GetInterpreter (lua_State *L)
{
	lua_getfield (L, LUA_REGISTRYINDEX, "interp");
	Interpreter *interp = (Interpreter*)lua_touserdata (L, -1);
	lua_pop (L, 1);
	return interp;
}

void Interpreter::term_echo (lua_State *L, int level)
{
	if (is_term && term_verbose >= level) term_out (L);
}

void Interpreter::term_strout (lua_State *L, const char *str, bool iserr)
{
	Interpreter *interp = GetInterpreter(L);
	fprintf(stderr, "%s\n", str);
	interp->term_strout (str, iserr);
}

void Interpreter::warn_obsolete(lua_State *L, const char *funcname)
{
	char cbuf[1024];
	sprintf(cbuf, "Obsolete function used: %s", funcname);
	term_strout(L, cbuf, true);
}

// ============================================================================

int Interpreter::AssertPrmtp(lua_State *L, const char *fname, int idx, int prm, int tp)
{
	char *tpname = (char*)"";
	int res = 1;

	if (lua_gettop(L) < idx) {
		luaL_error(L, "%s: too few arguments", fname);
		return 0;
	}

	switch (tp) {
	case PRMTP_NUMBER:
		tpname = (char*)"number";
		res = lua_isnumber(L,idx);
		break;
	case PRMTP_VECTOR:
		tpname = (char*)"vector";
		res = lua_isvector(L,idx);
		break;
	case PRMTP_STRING:
		tpname = (char*)"string";
		res = lua_isstring(L,idx);
		break;
	case PRMTP_LIGHTUSERDATA:
		tpname = (char*)"handle";
		res = lua_islightuserdata(L,idx);
		break;
	case PRMTP_TABLE:
		tpname = (char*)"table";
		res = lua_istable(L,idx);
		break;
	case PRMTP_BOOLEAN:
		tpname = (char*)"boolean";
		res = lua_isboolean(L, idx);
		break;
	case PRMTP_MATRIX:
		tpname = (char*)"matrix";
		res = lua_ismatrix(L, idx);
		break;
	case PRMTP_USERDATA:
		tpname = (char*)"userdata";
		res = lua_isuserdata(L, idx);
		break;

	}
	if (!res) {
		luaL_error(L, "%s: argument %d: invalid type (expected %s)", fname, prm, tpname);
	}
	return res;
}

int Interpreter::AssertMtdMinPrmCount(lua_State *L, int n, const char *funcname)
{
	if (lua_gettop(L) >= n) {
		return 1;
	} else {
		luaL_error(L, "%s: too few arguments (expected %d)", funcname, n - 1);
		return 0;
	}
}

int Interpreter::AssertPrmType(lua_State *L, int idx, int prmno, int tp, const char *funcname, const char *fieldname)
{
	if (tp & PRMTP_NIL)
		if (lua_isnil(L, idx))
			return 1;
	
	if (tp & PRMTP_NUMBER)
		if (lua_isnumber(L, idx))
			return 1;

	if (tp & PRMTP_BOOLEAN)
		if (lua_isboolean(L, idx))
			return 1;

	if (tp & PRMTP_STRING)
		if (lua_isstring(L, idx))
			return 1;

	if (tp & PRMTP_LIGHTUSERDATA)
		if (lua_islightuserdata(L, idx))
			return 1;

	if (tp & PRMTP_TABLE)
		if (lua_istable(L, idx))
			return 1;

	if (tp & PRMTP_VECTOR)
		if (lua_isvector(L, idx))
			return 1;

	if (tp & PRMTP_USERDATA)
		if (lua_isuserdata(L, idx))
			return 1;

	char cbuf[1024];
	if (fieldname)
		sprintf(cbuf, "%s: argument %d: field %s: invalid type (expected", funcname, prmno, fieldname);
	else
		sprintf(cbuf, "%s: argument %d: invalid type (expected", funcname, prmno);
	if (tp & PRMTP_NIL)
		strcat(cbuf, " nil or");
	if (tp & PRMTP_NUMBER)
		strcat(cbuf, " number or");
	if (tp & PRMTP_BOOLEAN)
		strcat(cbuf, " boolean or");
	if (tp & PRMTP_STRING)
		strcat(cbuf, " string or");
	if (tp & PRMTP_LIGHTUSERDATA)
		strcat(cbuf, " handle or");
	if (tp & PRMTP_TABLE)
		strcat(cbuf, " table or");
	if (tp & PRMTP_VECTOR)
		strcat(cbuf, " vector or");
	if (tp & PRMTP_USERDATA)
		strcat(cbuf, " userdata or");

	cbuf[strlen(cbuf)-3] = ')';
	cbuf[strlen(cbuf)-2] = ' ';
	cbuf[strlen(cbuf)-1] = '\0';

	strcat(cbuf, lua_typename(L, lua_type(L, idx)));
	strcat(cbuf, " given");

	luaL_error(L, cbuf);

	return 0;
}

int Interpreter::AssertPrmType(lua_State *L, int idx, int tp, const char *funcname)
{
	return AssertPrmType(L, idx, idx, tp, funcname);
}

int Interpreter::AssertMtdPrmType(lua_State *L, int idx, int tp, const char *funcname)
{
	return AssertPrmType(L, idx, idx-1, tp, funcname);
}

int Interpreter::AssertMtdNumber(lua_State *L, int idx, const char *funcname)
{
	if (lua_isnumber(L, idx)) {
		return 1;
	} else {
		luaL_error(L, "%s: argument %d: invalid type (expected number)", funcname, idx - 1);

		return 0;
	}
}

int Interpreter::AssertMtdHandle(lua_State *L, int idx, const char *funcname)
{
	if (lua_islightuserdata(L, 2)) { // necessary but not sufficient
		return 1;
	} else {
		luaL_error(L, "%s: argument %d: invalid type (expected handle)", funcname, idx - 1);
		return 0;
	}
}

// ============================================================================
// global functions
/***
Module oapi: General Orbiter API interface functions
@module oapi
*/

int Interpreter::help (lua_State *L)
{
	Interpreter *interp = GetInterpreter (L);
	int narg = lua_gettop (L);

	if (!narg) {
		if (!interp->is_term) return 0; // no terminal help without terminal - sorry
		static const int nline = 10;
		static const char *stdhelp[nline] = {
			"Orbiter script interpreter",
			"Based on Lua script language (" LUA_RELEASE ")",
			"  " LUA_COPYRIGHT,
			"  " LUA_AUTHORS,
			"For general orbiter-related help,",
			"  type: help(orbiter).",
			"For Orbiter-specific script extensions",
			"  type: help(api).",
			"For general help on the Lua language,",
			"  see the resources at www.lua.org."
		};
		for (int i = 0; i < nline; i++) {
			interp->term_strout (stdhelp[i]);
		}
	} else if (lua_isstring (L,1)) {
		// call a help page from the main Orbiter help file
		char topic[256];
		strncpy (topic, lua_tostring (L, 1), 255); lua_pop (L, 1);
		lua_pushstring (L, "html/orbiter.chm");
		lua_pushstring (L, topic);
		interp->oapiOpenHelp (L);
	} else if (lua_istable (L,1)) {
		// call a help page from an external help file
		char file[256], topic[256];
		lua_getfield (L, 1, "file");
		lua_getfield (L, 1, "topic");
		strcpy (file, lua_tostring(L,-2));
		if (!lua_isnil(L,-1))
			strcpy (topic, lua_tostring(L,-1));
		else topic[0] = '\0';
		lua_settop (L, 0);
		lua_pushstring (L, file);
		if (topic[0])
			lua_pushstring (L, topic);
		interp->oapiOpenHelp (L);
	}

	return 0;
}

int Interpreter::oapiOpenHelp (lua_State *L)
{
	static char fname[256], topic[256];
	static HELPCONTEXT hc = {fname, 0, 0, 0};

	Interpreter *interp = GetInterpreter (L);
	int narg = lua_gettop(L);
	if (narg) {
		strncpy (fname, lua_tostring (L,1), 255);
		if (narg > 1) {
			strncpy (topic, lua_tostring (L,2), 255);
			hc.topic = topic;
		} else
			hc.topic = 0;
		interp->postfunc = OpenHelp;
		interp->postcontext = &hc;
	}
	return 0;
}

int Interpreter::help_api (lua_State *L)
{
	lua_getglobal (L, "oapi");
	lua_getfield (L, -1, "open_help");
	lua_pushstring (L, "Html/Script/API/Reference.chm");
	LuaCall (L, 1, 0);
	return 0;
}


/***
Bit level manipulation on numbers.
@module bit
*/

/***
Test if a bitfield contains at least one bit from a mask.
@function anyset
@tparam number value value to test
@tparam number mask bitmask to test
@treturn boolean (value & mask) != 0
@usage if bit.anyset(event, PANEL_MOUSE.LBDOWN) then ... end
*/
int Interpreter::bit_anyset(lua_State* L)
{
	ASSERT_SYNTAX(lua_isnumber(L, 1), "Argument 1: expected number");
	uint32_t v = (uint32_t)lua_tonumber(L, 1);
	ASSERT_SYNTAX(lua_isnumber(L, 2), "Argument 2: expected number");
	uint32_t mask = lua_tonumber(L, 2);
	lua_pushboolean(L, (v & mask) != 0);
	return 1;
}

/***
Test if a bitfield contains all bits from a mask.
@function allset
@tparam number value value to test
@tparam number mask bitmask to test
@treturn boolean (value & mask) == mask
@usage if bit.allset(event, PANEL_MOUSE.LBDOWN) then ... end
*/
int Interpreter::bit_allset(lua_State* L)
{
	ASSERT_SYNTAX(lua_isnumber(L, 1), "Argument 1: expected number");
	uint32_t v = (uint32_t)lua_tonumber(L, 1);
	ASSERT_SYNTAX(lua_isnumber(L, 2), "Argument 2: expected number");
	uint32_t mask = lua_tonumber(L, 2);
	lua_pushboolean(L, (v & mask) == mask);
	return 1;
}

/***
Logical and between two bitfields.
@function band
@tparam number a
@tparam number b
@treturn number (a & b)
@usage v = bit.band(a,b)
*/
int Interpreter::bit_and(lua_State* L)
{
	ASSERT_SYNTAX(lua_isnumber(L, 1), "Argument 1: expected number");
	uint32_t a = (uint32_t)lua_tonumber(L, 1);
	ASSERT_SYNTAX(lua_isnumber(L, 2), "Argument 2: expected number");
	uint32_t b = lua_tonumber(L, 2);
	lua_pushnumber(L, a & b);
	return 1;
}

/***
Logical or between two or more bitfields.
@function bor
@tparam number a
@tparam number b
@treturn number (a | b | ...)
@usage v = bit.bor(a,b,c,...)
*/
int Interpreter::bit_or(lua_State* L)
{
	ASSERT_SYNTAX(lua_isnumber(L, 1), "Argument 1: expected number");
	uint32_t ret = (uint32_t)lua_tonumber(L, 1);
	int nb_extras = lua_gettop(L) - 1;
	for (int i = 0; i < nb_extras; i++) {
		ASSERT_SYNTAX(lua_isnumber(L, i + 2), "Argument : expected number");
		ret |= (uint32_t)lua_tonumber(L, i + 2);
	}

	lua_pushnumber(L, ret);
	return 1;
}
/***
Logical exclusive or between two bitfields.
@function bxor
@tparam number a
@tparam number b
@treturn number a xor b 
@usage v = bit.bxor(a,b)
*/
int Interpreter::bit_xor(lua_State* L)
{
	ASSERT_SYNTAX(lua_isnumber(L, 1), "Argument 1: expected number");
	uint32_t a = (uint32_t)lua_tonumber(L, 1);
	ASSERT_SYNTAX(lua_isnumber(L, 2), "Argument 2: expected number");
	uint32_t b = lua_tonumber(L, 2);
	lua_pushnumber(L, a ^ b);
	return 1;
}

/***
Logical not of a value.
@function not
@tparam number a
@treturn number (~a)
@usage v = bit.not(a)
*/
int Interpreter::bit_not(lua_State* L)
{
	ASSERT_SYNTAX(lua_isnumber(L, 1), "Argument 1: expected number");
	uint32_t notv = ~(uint32_t)lua_tonumber(L, 1);
	lua_pushnumber(L, notv);
	return 1;
}

/***
Left shift a value.
@function lshift
@tparam number a
@tparam number b
@treturn number a << b
@usage v = bit.lshift(a,2)
*/
int Interpreter::bit_lshift(lua_State* L)
{
	ASSERT_SYNTAX(lua_isnumber(L, 1), "Argument 1: expected number");
	ASSERT_SYNTAX(lua_isnumber(L, 2), "Argument 1: expected number");
	uint32_t v = lua_tonumber(L, 1);
	uint32_t s = lua_tonumber(L, 2);
	lua_pushnumber(L, v<<s);
	return 1;
}

/***
Right shift a value.
@function rshift
@tparam number a
@tparam number b
@treturn number a << b
@usage v = bit.rshift(a,2)
*/
int Interpreter::bit_rshift(lua_State* L)
{
	ASSERT_SYNTAX(lua_isnumber(L, 1), "Argument 1: expected number");
	ASSERT_SYNTAX(lua_isnumber(L, 2), "Argument 1: expected number");
	uint32_t v = lua_tonumber(L, 1);
	uint32_t s = lua_tonumber(L, 2);
	lua_pushnumber(L, v>>s);
	return 1;
}

/***
Right shift a value arithmetically.
@function arshift
@tparam number a
@tparam number b
@treturn number a << b
@usage v = bit.arshift(a,2)
*/
int Interpreter::bit_arshift(lua_State* L)
{
	ASSERT_SYNTAX(lua_isnumber(L, 1), "Argument 1: expected number");
	ASSERT_SYNTAX(lua_isnumber(L, 2), "Argument 1: expected number");
	int32_t v = lua_tonumber(L, 1);
	uint32_t s = lua_tonumber(L, 2);
	lua_pushnumber(L, v>>s);
	return 1;
}

/***
Right rotate a value.
@function ror
@tparam number a
@tparam number b
@treturn number a << b
@usage v = bit.ror(a,2)
*/
int Interpreter::bit_ror(lua_State* L)
{
	ASSERT_SYNTAX(lua_isnumber(L, 1), "Argument 1: expected number");
	ASSERT_SYNTAX(lua_isnumber(L, 2), "Argument 1: expected number");
	uint32_t v = lua_tonumber(L, 1);
	uint32_t s = lua_tonumber(L, 2);
	lua_pushnumber(L, (v>>s)|(v<<(32-s)));
	return 1;
}

/***
Left rotate a value.
@function rol
@tparam number a
@tparam number b
@treturn number a << b
@usage v = bit.rol(a,2)
*/
int Interpreter::bit_rol(lua_State* L)
{
	ASSERT_SYNTAX(lua_isnumber(L, 1), "Argument 1: expected number");
	ASSERT_SYNTAX(lua_isnumber(L, 2), "Argument 1: expected number");
	uint32_t v = lua_tonumber(L, 1);
	uint32_t s = lua_tonumber(L, 2);
	lua_pushnumber(L, (v<<s)|(v>>(32-s)));
	return 1;
}

/***
Mask a value.
@function mask
@tparam number bitfield
@tparam number mask
@treturn number (bitfield & ~mask)
@usage v = bit.mask(a, mask)
*/
int Interpreter::bit_mask(lua_State* L)
{
	ASSERT_SYNTAX(lua_isnumber(L, 1), "Argument 1: expected number");
	uint32_t val = (uint32_t)lua_tonumber(L, 1);
	ASSERT_SYNTAX(lua_isnumber(L, 2), "Argument 2: expected number");
	uint32_t mask = lua_tonumber(L, 2);
	lua_pushnumber(L, val & ~mask);
	return 1;
}


// ============================================================================
// 

/***
Vector library functions.
@module vec
*/

/***
Define a vector from its components.

You can also use standard Lua syntax to define the components of the vector
(vectors are defined as tables with fields 'x', 'y' and 'z').

The _V function provides a handier notation :
    v = _V(0,0,1)

@function set
@tparam number x
@tparam number y
@tparam number z
@treturn vector vector
@usage v = vec.set(x,y,z)
*/
int Interpreter::vec_set (lua_State *L)
{
	int i;
	VECTOR3 v;
	for (i = 0; i < 3; i++) {
		ASSERT_SYNTAX(lua_isnumber(L,i+1), "expected three numeric arguments");
		v.data[i] = lua_tonumber(L,i+1);
	}
	lua_pushvector(L,v);
	return 1;
}

/***
Sum of two vectors.

Each argument can be either a vector or a number.
The return value is a vector, unless both a and b are numbers

When operating on a number and a vector, the number is replaced with the vector {x=number, y=number, z=number}
@function add
@tparam (vector|number) a
@tparam (vector|number) b
@treturn (vector|number) result of a+b
@usage v = vec.add(a,b)
*/
int Interpreter::vec_add (lua_State *L)
{
	VECTOR3 va, vb;
	double fa, fb;
	if (lua_isvector(L,1)) {
		va = lua_tovector (L,1);
		if (lua_isvector(L,2)) {
			vb = lua_tovector (L,2);
			lua_pushvector (L, va+vb);
		} else {
			ASSERT_SYNTAX (lua_isnumber(L,2), "Argument 2: expected vector or number");
			fb = lua_tonumber (L,2);
			lua_pushvector (L, _V(va.x+fb, va.y+fb, va.z+fb));
		}
	} else {
		ASSERT_SYNTAX (lua_isnumber(L,1), "Argument 1: expected vector or number");
		fa = lua_tonumber (L,1);
		if (lua_isvector (L,2)) {
			vb = lua_tovector (L,2);
			lua_pushvector (L, _V(fa+vb.x, fa+vb.y, fa+vb.z));
		} else {
			ASSERT_SYNTAX (lua_isnumber(L,2), "Argument 2: expected vector or number");
			fb = lua_tonumber (L,2);
			lua_pushnumber (L, fa+fb);
		}
	}
	return 1;
}

/***
Difference of two vectors.

Each argument can be either a vector or a number.
The return value is a vector, unless both a and b are numbers

Substracting a number to a vector results in substracting the number to each component of the vector
@function sub
@tparam (vector|number) a
@tparam (vector|number) b
@treturn (vector|number) result of a-b
@usage v = vec.sub(a,b)
*/
int Interpreter::vec_sub (lua_State *L)
{
	VECTOR3 va, vb;
	double fa, fb;
	if (lua_isvector(L,1)) {
		va = lua_tovector (L,1);
		if (lua_isvector(L,2)) {
			vb = lua_tovector (L,2);
			lua_pushvector (L, va-vb);
		} else {
			ASSERT_SYNTAX (lua_isnumber(L,2), "Argument 2: expected vector or number");
			fb = lua_tonumber (L,2);
			lua_pushvector (L, _V(va.x-fb, va.y-fb, va.z-fb));
		}
	} else {
		ASSERT_SYNTAX (lua_isnumber(L,1), "Argument 1: expected vector or number");
		fa = lua_tonumber (L,1);
		if (lua_isvector (L,2)) {
			vb = lua_tovector (L,2);
			lua_pushvector (L, _V(fa-vb.x, fa-vb.y, fa-vb.z));
		} else {
			ASSERT_SYNTAX (lua_isnumber(L,2), "Argument 2: expected vector or number");
			fb = lua_tonumber (L,2);
			lua_pushnumber (L, fa-fb);
		}
	}
	return 1;
}

/***
Elementwise vector multiplication.

Each argument can be either a vector or a number.
The return value is a vector, unless both a and b are numbers

Multiplying a number with a vector results in multiplying the number with each component of the vector
@function mul
@tparam (vector|number) a
@tparam (vector|number) b
@treturn (vector|number) result of a*b
@usage v = vec.mul(a,b)
*/
int Interpreter::vec_mul (lua_State *L)
{
	VECTOR3 v1, v2, res;
	double f1, f2;
	int i;
	if (lua_isvector(L,1)) {
		v1 = lua_tovector(L,1);
		if (lua_isvector(L,2)) {
			v2 = lua_tovector(L,2);
			for (i = 0; i < 3; i++) res.data[i] = v1.data[i]*v2.data[i];
		} else {
			ASSERT_SYNTAX (lua_isnumber(L,2), "Argument 2: expected vector or number");
			f2 = lua_tonumber(L,2);
			for (i = 0; i < 3; i++) res.data[i] = v1.data[i]*f2;
		}
	} else {
		ASSERT_SYNTAX (lua_isnumber(L,1), "Argument 1: expected vector or number");
		f1 = lua_tonumber(L,1);
		if (lua_isvector(L,2)) {
			v2 = lua_tovector(L,2);
			for (i = 0; i < 3; i++) res.data[i] = f1*v2.data[i];
		} else {
			ASSERT_SYNTAX (lua_isnumber(L,2), "Argument 2: expected vector or number");
			f2 = lua_tonumber(L,2);
			lua_pushnumber (L,f1*f2);
			return 1;
		}
	}
	lua_pushvector(L,res);
	return 1;
}

/***
Elementwise vector division.

Each argument can be either a vector or a number.
The return value is a vector, unless both a and b are numbers

Dividing a number with a vector results in dividing the number with each component of the vector
@function div
@tparam (vector|number) a
@tparam (vector|number) b
@treturn (vector|number) result of a/b
@usage v = vec.div(a,b)
*/
int Interpreter::vec_div (lua_State *L)
{
	VECTOR3 v1, v2, res;
	double f1, f2;
	int i;
	if (lua_isvector(L,1)) {
		v1 = lua_tovector(L,1);
		if (lua_isvector(L,2)) {
			v2 = lua_tovector(L,2);
			for (i = 0; i < 3; i++) res.data[i] = v1.data[i]/v2.data[i];
		} else {
			ASSERT_SYNTAX (lua_isnumber(L,2), "Argument 2: expected vector or number");
			f2 = lua_tonumber(L,2);
			for (i = 0; i < 3; i++) res.data[i] = v1.data[i]/f2;
		}
	} else {
		ASSERT_SYNTAX (lua_isnumber(L,1), "Argument 1: expected vector or number");
		f1 = lua_tonumber(L,1);
		if (lua_isvector(L,2)) {
			v2 = lua_tovector(L,2);
			for (i = 0; i < 3; i++) res.data[i] = f1/v2.data[i];
		} else {
			ASSERT_SYNTAX (lua_isnumber(L,2), "Argument 2: expected vector or number");
			f2 = lua_tonumber(L,2);
			lua_pushnumber(L,f1/f2);
			return 1;
		}
	}
	lua_pushvector(L,res);
	return 1;
}

/***
Scalar (inner, dot) product of two vectors.
@function dotp
@tparam vector a
@tparam vector b
@treturn number scalar product ab
@usage v = vec.dotp(a,b)
*/
int Interpreter::vec_dotp (lua_State *L)
{
	VECTOR3 v1, v2;
	ASSERT_SYNTAX(lua_isvector(L,1), "Argument 1: expected vector");
	v1 = lua_tovector(L,1);
	ASSERT_SYNTAX(lua_isvector(L,2), "Argument 2: expected vector");
	v2 = lua_tovector(L,2);
	lua_pushnumber (L, dotp(v1,v2));
	return 1;
}

/***
Cross product of two vectors.
@function crossp
@tparam vector a
@tparam vector b
@treturn vector crossp product a * b
@usage v = vec.crossp(a,b)
*/
int Interpreter::vec_crossp (lua_State *L)
{
	VECTOR3 v1, v2;
	ASSERT_SYNTAX(lua_isvector(L,1), "Argument 1: expected vector");
	v1 = lua_tovector(L,1);
	ASSERT_SYNTAX(lua_isvector(L,2), "Argument 2: expected vector");
	v2 = lua_tovector(L,2);
	lua_pushvector (L, crossp(v1,v2));
	return 1;
}

/***
Length of a vector.
@function length
@tparam vector a
@treturn number length of a
@usage len = vec.length(a)
*/
int Interpreter::vec_length (lua_State *L)
{
	VECTOR3 v;
	ASSERT_SYNTAX(lua_isvector(L,1), "Argument 1: expected vector");
	v = lua_tovector(L,1);
	lua_pushnumber (L, length(v));
	return 1;
}

/***
Distance between two points.
@function dist
@tparam vector a (point position)
@tparam vector b (point position)
@treturn number distance between point a and point b
@usage dist = vec.dist(a, b)
*/
int Interpreter::vec_dist (lua_State *L)
{
	VECTOR3 v1, v2;
	ASSERT_SYNTAX(lua_isvector(L,1), "Argument 1: expected vector");
	v1 = lua_tovector(L,1);
	ASSERT_SYNTAX(lua_isvector(L,2), "Argument 2: expected vector");
	v2 = lua_tovector(L,2);
	lua_pushnumber (L, dist(v1,v2));
	return 1;
}

/***
Unit vector.
@function unit
@tparam vector a
@treturn vector unit vector constructed from a
@usage u = vec.unit(a)
*/
int Interpreter::vec_unit (lua_State *L)
{
	VECTOR3 v;
	ASSERT_SYNTAX(lua_isvector(L,1), "Argument 1: expected vector");
	v = lua_tovector(L,1);
	lua_pushvector (L, unit(v));
	return 1;
}

/***
Matrix library functions.
@module mat
*/

/***
Identity matrix.
@function identity
@treturn matrix identity matrix
@usage I = mat.identity()
*/
int Interpreter::mat_identity (lua_State *L)
{
	lua_pushmatrix (L,identity());
	return 1;
}

/***
Matrix vector multiplication.
@function mul
@tparam matrix m
@tparam vector v
@treturn vector result of M * v
@usage newpos = mat.mul(rot, pos)
*/
int Interpreter::mat_mul (lua_State *L)
{
	ASSERT_SYNTAX(lua_ismatrix(L,1), "Argument 1: expected matrix");
	ASSERT_SYNTAX(lua_isvector(L,2), "Argument 2: expected vector");
	lua_pushvector (L, mul (lua_tomatrix(L,1), lua_tovector(L,2)));
	return 1;
}

/***
Matrix-transpose vector multiplication.
@function tmul
@tparam matrix m
@tparam vector v
@treturn vector result of M<sup>T</sup> * v
@usage p = mat.tmul(op, v)
*/
int Interpreter::mat_tmul (lua_State *L)
{
	ASSERT_SYNTAX(lua_ismatrix(L,1), "Argument 1: expected matrix");
	ASSERT_SYNTAX(lua_isvector(L,2), "Argument 2: expected vector");
	lua_pushvector (L, tmul (lua_tomatrix(L,1), lua_tovector(L,2)));
	return 1;
}

/***
Matrix matrix multiplication.
@function mmul
@tparam matrix A
@tparam matrix B
@treturn matrix result of A * B
@usage R = mat.mmul(A, B)
*/
int Interpreter::mat_mmul (lua_State *L)
{
	ASSERT_SYNTAX(lua_ismatrix(L,1), "Argument 1: expected matrix");
	ASSERT_SYNTAX(lua_ismatrix(L,2), "Argument 2: expected matrix");
	lua_pushmatrix (L, mul(lua_tomatrix(L,1), lua_tomatrix(L,2)));
	return 1;
}
/***
Construct a rotation matrix from an axis and an angle.
@function rotm
@tparam vector axis (must be normalized)
@tparam number angle [rad]
@treturn matrix rotation matrix
@usage R = mat.rotm(dir, angle)
*/

int Interpreter::mat_rotm (lua_State *L) {
	ASSERT_SYNTAX(lua_isvector(L,1), "Argument 1: expected vector");
	ASSERT_SYNTAX(lua_isnumber(L,2), "Argument 2: expected number");
	VECTOR3 axis = lua_tovector(L, 1);
	double angle = lua_tonumber(L, 2);
	double c = cos(angle), s = sin(angle);
	double t = 1-c, x = axis.x, y = axis.y, z = axis.z;

	MATRIX3 rot = _M(t*x*x+c, t*x*y-z*s, t*x*z+y*s,
		      t*x*y+z*s, t*y*y+c, t*y*z-x*s,
			  t*x*z-y*s, t*y*z+x*s, t*z*z+c);
	lua_pushmatrix(L, rot);
	return 1;
}

// ============================================================================
// process library functions

int Interpreter::procFrameskip (lua_State *L)
{
	// return control to the orbiter core for execution of one time step
	// This should be called in the loop of any "wait"-type function

	Interpreter *interp = GetInterpreter(L);
	interp->frameskip (L);
	return 0;
}

// ============================================================================
// oapi library functions

/***
Module oapi: General Orbiter API interface functions.
@module oapi
*/


/***
Time functions
@section oapi_time
*/

/***
Returns the current simulation time.
(the simulated time in seconds since the start of the session).
@function get_simtime
@treturn number simulation time [s]
@see get_systime, get_simstep, get_simmjd
*/
int Interpreter::oapi_get_simtime (lua_State *L)
{
	lua_pushnumber (L, oapiGetSimTime());
	return 1;
}

/***
Returns the length of the last simulation time step.
(from previous to current frame) in seconds.
@function get_simstep
@treturn number simulation time step [s]
@see get_simtime, get_sysstep
*/
int Interpreter::oapi_get_simstep (lua_State *L)
{
	lua_pushnumber (L, oapiGetSimStep());
	return 1;
}

/***
Returns the true time since the start of the session.
@function get_systime
@treturn number session up-time [s]
@see get_simtime, get_sysstep
*/
int Interpreter::oapi_get_systime (lua_State *L)
{
	lua_pushnumber (L, oapiGetSysTime());
	return 1;
}

/***
Returns the true length of the last simulation time step.
@function get_sysstep
@treturn system time step [s]
@see get_systime, get_simstep
*/
int Interpreter::oapi_get_sysstep (lua_State *L)
{
	lua_pushnumber (L, oapiGetSysStep());
	return 1;
}

/***
Returns the Modified Julian Data (MJD) of the current simulation state.
The MJD is the number of days that have elapsed since midnight of November 17, 1858.
The MJD is used as an absolute time reference in Orbiter.
@function get_simmjd
@treturn number current Modified Julian Date [days]
@see get_simtime, set_simmjd
*/
int Interpreter::oapi_get_simmjd (lua_State *L)
{
	lua_pushnumber (L, oapiGetSimMJD());
	return 1;
}

/***
Set the current simulation date.
The simulation session performs a jump to the new time.
The date is provided in MJD format (the number of days that have elapsed since midnight of November 17, 1858).
The new time can be set to before or after the current simulation time.
Deterministic objects (planets controlled by Keplerian elements or perturbation code) are propagated directly.
Vessels are propagated according to pmode, which can be a combination of values in Propagation modes.
If pmode is not specified, the propagation modes resort to propagation along Keplerian orbits for both orbital and suborbital vessels.
@function set_simmjd
@tparam number mjd new simulation time in Modified Julian Date format [days]
@tparam[opt] PROP pmode vessel propagation modes 
@see get_simmjd, get_simtime
*/
int Interpreter::oapi_set_simmjd (lua_State *L)
{
	ASSERT_SYNTAX (lua_isnumber (L,1), "Argument 1: invalid type (expected number)");
	double mjd = lua_tonumber (L,1);
	int pmode = 0;
	if (lua_gettop (L) >= 2) {
		ASSERT_SYNTAX (lua_isnumber (L,2), "Argument 2: invalid type (expected number)");
		pmode = (int)(lua_tonumber (L,2)+0.5);
	}
	oapiSetSimMJD (mjd, pmode);
	return 0;
}

/***
Retrieve the current computer system time in Modified Julian Date (MJD) format.

The returned value is the UTC time obtained from the computer system clock,
   plus dt=66.184 seconds to map from UTC to TDB (Barycentric Dynamical Time) used
   internally by Orbiter. The dt offset was not added in previous Orbiter releases.

@function get_sysmjd
@treturn number Computer system time in MJD format
@see get_systime
*/
int Interpreter::oapi_get_sysmjd (lua_State *L)
{
	lua_pushnumber (L, oapiGetSysMJD());
	return 1;
}

/***
Convert a simulation up time value into a Modified Julian Date.

@function time2mjd
@tparam number simt simulation time (seconds)
@treturn number Modified Julian Date (MJD) corresponding to simt.
*/
int Interpreter::oapi_time2mjd (lua_State *L)
{
	ASSERT_SYNTAX (lua_isnumber (L,1), "Argument 1: invalid type (expected number)");
	double simt = lua_tonumber(L,1);
	double mjd = oapiTime2MJD(simt);
	lua_pushnumber (L, mjd);
	return 1;
}

/***
Returns simulation time acceleration factor.

This function will not return 0 when the simulation is paused. Instead it will
  return the acceleration factor at which the simulation will resume when
  unpaused. Use oapiGetPause to obtain the pause/resume state.

@function get_tacc
@treturn number time acceleration factor
@see set_tacc
*/
int Interpreter::oapi_get_tacc (lua_State *L)
{
	lua_pushnumber (L, oapiGetTimeAcceleration());
	return 1;
}

/***
Set the simulation time acceleration factor.

Warp factors will be clamped to the valid range [1,100000]. If the new warp
  factor is different from the previous one, all DLLs (including the one that
  called oapiSetTimeAcceleration()) will be sent a opcTimeAccChanged() message.

@function set_tacc
@tparam number warp new time acceleration factor
@see get_tacc
*/
int Interpreter::oapi_set_tacc (lua_State *L)
{
	double warp = lua_tonumber (L, -1);
	oapiSetTimeAcceleration (warp);
	return 0;
}

/***
Returns the current simulation pause state.

@function get_pause
@treturn bool _true_ if simulation is currently paused, _false_ if it is running.
@see set_pause
*/
int Interpreter::oapi_get_pause (lua_State *L)
{
	lua_pushboolean (L, oapiGetPause() ? 1:0);
	return 1;
}

/***
Sets the simulation pause state.

@function set_pause
@tparam bool pause _true_ to pause the simulation, _false_ to resume.
@see get_pause
*/
int Interpreter::oapi_set_pause (lua_State *L)
{
	oapiSetPause (lua_toboolean (L, -1) != 0);
	return 0;
}

/***
Object access functions
@section object_access
*/

/***
Returns the version number of the Orbiter core system.

Orbiter version numbers are derived from the build date.
   The version number is constructed as
   (year%100)*10000 + month*100 + day, resulting in a decimal
   version number of the form YYMMDD

@function get_orbiter_version
@treturn int version number
*/
int Interpreter::oapi_get_orbiter_version (lua_State *L)
{
	lua_pushnumber(L, oapiGetOrbiterVersion());
	return 1;
}

/***
Returns the dimensions of the render viewport.

This function writes the viewport width, height and (optionally)
   colour depth values into the variables pointed to by the function
   parameters.

For fullscreen modes, the viewport size corresponds to the
   fullscreen resolution. For windowed modes, the viewport size corresponds
   to the client area of the render window.

@function get_viewport_size
@treturn int w pointer to viewport width [pixel]
@treturn int h pointer to viewport height [pixel]
@treturn int bpp pointer to colour depth [bits per pixel]
*/
int Interpreter::oapi_get_viewport_size (lua_State *L)
{
	DWORD w, h, bpp;
	oapiGetViewportSize(&w, &h, &bpp);
	lua_createtable(L, 0, 3);
	lua_pushnumber(L, w);
	lua_setfield(L, -2, "w");
	lua_pushnumber(L, h);
	lua_setfield(L, -2, "h");
	lua_pushnumber(L, bpp);
	lua_setfield(L, -2, "bpp");
	return 1;
}

/***
Returns a handle for a simulation object.
@function get_objhandle
@tparam ?string|integer id object identifier: either the object name, or object index (&ge; 0)
@treturn handle object handle
*/
int Interpreter::oapiGetObjectHandle (lua_State *L)
{
	OBJHANDLE hObj;
	if (lua_isnumber (L, 1)) { // select by index
		int idx = (int)lua_tointeger (L, 1);
		hObj = oapiGetObjectByIndex (idx);
	} else {
		char *name = (char*)luaL_checkstring (L, 1);
		hObj = oapiGetObjectByName (name);
	}
	if (hObj) lua_pushlightuserdata (L, hObj);
	else lua_pushnil (L);
	return 1;
}

/***
Returns the number of objects in the current simulation.
@function get_objcount
@treturn int object count (&ge; 0)
*/
int Interpreter::oapiGetObjectCount (lua_State *L)
{
	lua_pushinteger (L, ::oapiGetObjectCount());
	return 1;
}

/***
Returns the name of an object.
@function get_objname
@tparam handle hobj object handle
@treturn string object name
@see get_objhandle
*/
int Interpreter::oapiGetObjectName (lua_State *L)
{
	OBJHANDLE hObj;
	if (lua_islightuserdata (L, 1) && (hObj = lua_toObject (L, 1))) {
		char name[1024];
		::oapiGetObjectName (hObj, name, 1024);
		lua_pushstring (L, name);
	} else lua_pushnil (L);
	return 1;
}

/***
Delete a vessel object.
@function del_vessel
@tparam ?string|handle id vessel identifier: either the vessel name, or vessel handle
*/
int Interpreter::oapi_del_vessel (lua_State *L)
{
	OBJHANDLE hObj;
	if (lua_islightuserdata (L,1) && (hObj = lua_toObject (L,1))) {
		oapiDeleteVessel (hObj);
	} else if (lua_isstring (L,1)) {
		const char *name = lua_tostring (L,1);
		if (hObj = oapiGetVesselByName ((char*)name))
			oapiDeleteVessel (hObj);
	}
	return 0;
}

/***
Create a vessel object.
@function create_vessel
@tparam string name vessel name
@tparam string classname vessel class name
@treturn handle|nil vessel handle
*/
int Interpreter::oapi_create_vessel(lua_State* L)
{
	const char* name = lua_tostring(L, 1);
	const char* classname = lua_tostring(L, 2);
	VESSELSTATUS *vs = (VESSELSTATUS*)lua_touserdata(L, 3);
	OBJHANDLE hObj = oapiCreateVessel(name, classname, *vs);
	if (hObj) lua_pushlightuserdata(L, hObj);
	else lua_pushnil(L);
	return 1;
}

/***
Set focus on object.
@function set_focusobject
@tparam ?handle|number|string id vessel identifier: either the vessel name, index or handle
@treturn handle|nil handle of vessel loosing focus or nil if focus did not change
*/
int Interpreter::oapi_set_focusobject(lua_State* L)
{
	OBJHANDLE hObj = 0;
	if (lua_islightuserdata(L, 1)) { // select by handle
		hObj = lua_toObject(L, 1);
	}
	else if (lua_isnumber(L, 1)) { // select by index
		int idx = (int)lua_tointeger(L, 1);
		hObj = oapiGetVesselByIndex(idx);
	}
	else if (lua_isstring(L, 1)) {  // select by name
		const char* name = lua_tostring(L, 1);
		if (name)
			hObj = oapiGetVesselByName((char*)name);
	}

	if (hObj) {
		OBJHANDLE prev = oapiSetFocusObject(hObj);
		if(prev)
			lua_pushlightuserdata(L, prev);
		else
			lua_pushnil(L);
	}
	else {
		lua_pushnil(L);
		lua_pushstring(L, "Invalid argument for vessel.set_focus, expected handle, string or index number");
		return 2;
	}

	return 1;
}

/***
Get current rotation matrix of on object.
@function get_rotationmatrix
@tparam handle vessel handle
@treturn matrix|nil rotation matrix of the vessel or nil is handle is incorrect
*/
int Interpreter::oapi_get_rotationmatrix(lua_State* L)
{
	OBJHANDLE hObj;
	if (lua_islightuserdata(L, 1) && (hObj = lua_toObject(L, 1))) {
		MATRIX3 mat;
		oapiGetRotationMatrix(hObj, &mat);
		lua_pushmatrix(L, mat);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/***
Register exhaust texture.
@function register_exhausttexture
@tparam string fname texture filename
@treturn handle|nil texture handle or nil if texture not found
*/
int Interpreter::oapi_register_exhausttexture(lua_State* L)
{
	const char* name = lua_tostring(L, 1);
	SURFHANDLE surf = oapiRegisterExhaustTexture(const_cast<char*>(name));
	if (surf)
		lua_pushlightuserdata(L, surf);
	else
		lua_pushnil(L);
	return 1;
}

/***
Register reentry texture.
@function register_reentrytexture
@tparam string fname texture filename
@treturn handle|nil texture handle or nil if texture not found
*/
int Interpreter::oapi_register_reentrytexture(lua_State* L)
{
	const char* name = lua_tostring(L, 1);
	SURFHANDLE surf = oapiRegisterReentryTexture(const_cast<char*>(name));
	if (surf)
		lua_pushlightuserdata(L, surf);
	else
		lua_pushnil(L);
	return 1;
}

/***
Register particle texture.
@function register_particletexture
@tparam string fname texture filename
@treturn handle|nil texture handle or nil if texture not found
*/
int Interpreter::oapi_register_particletexture(lua_State* L)
{
	const char* name = lua_tostring(L, 1);
	SURFHANDLE surf = oapiRegisterParticleTexture(const_cast<char*>(name));
	if (surf)
		lua_pushlightuserdata(L, surf);
	else
		lua_pushnil(L);
	return 1;
}

/***
Retrieve a surface handle for a mesh texture.
@function get_texturehandle
@tparam handle hMesh mesh handle
@tparam number texidx texture index (>=1)
@treturn handle|nil texture handle or nil if texture not found
*/
int Interpreter::oapi_get_texturehandle(lua_State* L)
{
	MESHHANDLE hMesh = lua_tomeshhandle(L, 1);
	DWORD idx = lua_tonumber(L, 2);
	SURFHANDLE surf = oapiGetTextureHandle(hMesh, idx);
	if (surf)
		lua_pushlightuserdata(L, surf);
	else
		lua_pushnil(L);
	return 1;
}

/***
Load a texture from a file.

File names can contain search paths. Orbiter searches for textures in the
standard way, i.e. first searches the HitexDir directory (usually Textures2),
then the TextureDir directory (usually Textures). All search paths are relative
to the texture root directories. For example, oapi.load_texture()
("myvessel/mytex.dds") would first search for Textures2/myvessel/mytex.dds, 
then for Textures/myvessel/mytex.dds.

@function load_texture
@tparam string fname texture filename
@tparam[opt=false] boolean dynamic allow dynamic modification
@treturn handle|nil texture handle or nil if texture not found
@see release_texture
*/
int Interpreter::oapi_load_texture(lua_State* L)
{
	const char *file = lua_tostring(L, 1);
	bool dynamic = false;
	if(lua_gettop(L) > 1)
		dynamic = lua_toboolean(L, 2);

	SURFHANDLE surf = oapiLoadTexture(file, dynamic);
	if (surf)
		lua_pushlightuserdata(L, surf);
	else
		lua_pushnil(L);
	return 1;
}

/***
Release a texture.
@function release_texture
@tparam handle hTex texture handle
*/
int Interpreter::oapi_release_texture(lua_State* L)
{
	SURFHANDLE surf = (SURFHANDLE)lua_touserdata(L, 1);
	oapiReleaseTexture(surf);
	return 0;
}

/***
Create a surface.
@function create_surface
@tparam number w texture width
@tparam number h texture height
@tparam[opt] number attrib surface creation attributes
@treturn handle|nil texture handle or nil if surface creation failed
@see destroy_surface
*/
int Interpreter::oapi_create_surface(lua_State* L)
{
	int w = luaL_checknumber(L, 1);
	int h = luaL_checknumber(L, 2);
	SURFHANDLE surf;
	if(lua_gettop(L) >= 3) {
		int attrib = luaL_checknumber(L, 3);
		surf = oapiCreateSurfaceEx(w, h, attrib);
	} else {
		surf = oapiCreateSurface(w, h);
	}

	if(surf)
		lua_pushlightuserdata(L, surf);
	else
		lua_pushnil(L);
	return 1;
}

/***
Destroy a surface.
@function destroy_surface
@tparam handle hSurf surface handle
*/
int Interpreter::oapi_destroy_surface(lua_State* L)
{
	SURFHANDLE surf = (SURFHANDLE)lua_touserdata(L, 1);
	oapiDestroySurface(surf);
	return 0;
}

/***
Clear a surface.
@function clear_surface
@tparam handle hSurf surface handle
*/
int Interpreter::oapi_clear_surface(lua_State* L)
{
	SURFHANDLE surf = (SURFHANDLE)lua_touserdata(L, 1);
	oapiClearSurface(surf);
	return 0;
}

/***
Save a surface to a file.
@function save_surface
@tparam string fname file name for the saved surface (excluding file extension)
@tparam handle hSurf surface handle
@tparam number format file format
@tparam[opt=0.7] number quality [0-1]
@treturn boolean true on success
*/
int Interpreter::oapi_save_surface(lua_State* L)
{
	const char *name = luaL_checkstring(L, 1);
	SURFHANDLE surf = (SURFHANDLE)lua_touserdata(L, 2);
	oapi::ImageFileFormat format = (oapi::ImageFileFormat)luaL_checkinteger(L, 3);
	float quality = 0.7;
	if(lua_gettop(L)>=4)
		quality = luaL_checknumber(L, 4);

	bool ret = oapiSaveSurface(name, surf, format, quality);
	lua_pushboolean(L, ret);
	return 1;
}

/***
Replace a mesh texture.
@function set_texture
@tparam handle hMesh mesh handle
@tparam number texidx texture index
@tparam handle hSurf texture handle
@treturn boolean true on success
*/
int Interpreter::oapi_set_texture(lua_State* L)
{
	DWORD texid = luaL_checknumber(L, 2);
	SURFHANDLE surf = (SURFHANDLE)lua_touserdata(L, 3);
	MESHHANDLE *hMesh = (MESHHANDLE *)luaL_tryudata(L, 1, "MESHHANDLE");
	if(hMesh) {
		bool ret = oapiSetTexture(*hMesh, texid, surf);
		lua_pushboolean(L, ret);
		return 1;
	}
	DEVMESHHANDLE hDevMesh = lua_todevmeshhandle(L, 1);
	bool ret = oapiSetTexture(hDevMesh, texid, surf);
	lua_pushboolean(L, ret);
	return 1;
}

/***
Reset the properties of a mesh material.
@function set_materialex
@tparam handle hMesh mesh handle
@tparam number matidx material index
@tparam number matprp material property to be set
@tparam colour col material property value
@treturn boolean true on success
*/
int Interpreter::oapi_set_materialex(lua_State* L)
{
	DEVMESHHANDLE hMesh = lua_todevmeshhandle(L, 1);
	int idx = (int)lua_tointeger(L, 2);
	MatProp prp = (MatProp)lua_tointeger(L, 3);
	COLOUR4 col = lua_torgba(L, 4);
	oapi::FVECTOR4 mat(col);
	int err = oapiSetMaterialEx(hMesh, idx, prp, &mat);
	if (err) {
		lua_pushnil(L);
		lua_pushfstring(L, "oapiSetMaterialEx failed with error %d", err);
		return 2;
	}
	else {
		lua_pushboolean(L, 1);
		return 1;
	}
}

/***
Reset the properties of a mesh material.

The properties must be provided as a table with the following fields :

- diffuse : colour
- ambient : colour
- specular : colour
- emissive : colour
- power : number

@function set_material
@tparam handle hMesh mesh handle
@tparam number matidx material index
@tparam table mat material properties
@treturn boolean true on success
*/
int Interpreter::oapi_set_material(lua_State* L)
{
	DEVMESHHANDLE hMesh = lua_todevmeshhandle(L, 1);
	int idx = (int)lua_tointeger(L, 2);
	MATERIAL mat;

	lua_getfield(L, 3, "diffuse");
	mat.diffuse = lua_torgba(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 3, "ambient");
	mat.ambient = lua_torgba(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 3, "specular");
	mat.specular = lua_torgba(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 3, "emissive");
	mat.emissive = lua_torgba(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 3, "power");
	mat.power = lua_tonumber(L, -1);
	lua_pop(L, 1);


	int err = oapiSetMaterial(hMesh, idx, &mat);
	if (err) {
		lua_pushnil(L);
		lua_pushfstring(L, "oapiSetMaterial failed with error %d", err);
		return 2;
	}
	else {
		lua_pushboolean(L, 1);
		return 1;
	}
}

/***
Set custom properties for a mesh.

Note : Currently only a single mesh property is recognised, but this may be extended in future versions:

- MESHPROPERTY.MODULATEMATALPHA:
	if value==0 (default) disable material alpha information in textured mesh groups (only use texture alpha channel).\n
	if value<>0 modulate (mix) material alpha values with texture alpha maps.


@function set_meshproperty
@tparam handle hMesh mesh handle
@tparam number property property tag
@tparam number value new mesh property value
@treturn boolean  true if the property tag was recognised and the request could be executed, false otherwise.
*/
int Interpreter::oapi_set_meshproperty(lua_State* L)
{
	DWORD property = luaL_checknumber(L, 2);
	DWORD value = luaL_checknumber(L, 3);
	MESHHANDLE *hMesh = (MESHHANDLE *)luaL_tryudata(L, 1, "MESHHANDLE");
	if(hMesh) {
		bool ret = oapiSetMeshProperty(*hMesh, property, value);
		lua_pushboolean(L, ret);
		return 1;
	}
	DEVMESHHANDLE hDevMesh = lua_todevmeshhandle(L, 1);
	bool ret = oapiSetMeshProperty(hDevMesh, property, value);
	lua_pushboolean(L, ret);
	return 1;
}

/***
Trigger a redraw notification for a virtual cockpit area.

This function triggers a call to the clbk_VCredrawevent callback function in the vessel module.

The redraw notification is normally only sent if vc_id is equal to the currently
active virtual cockpit position (>=0). To invoke the redraw notification
independent of the currently active position, set vc_id to -1.

@function VC_trigger_redrawarea
@tparam number vc_id cockpit identifier
@tparam number area_id area identifier (as specified during area registration)
*/
int Interpreter::oapi_VC_trigger_redrawarea(lua_State* L)
{
	int vc_id = (int)lua_tointeger(L, 1);
	int area_id = (int)lua_tointeger(L, 2);
	oapiVCTriggerRedrawArea(vc_id, area_id);
	return 0;
}

/***
Associate a quadrilateral region in the virtual cockpit with a registered area to receive mouse events.

This function will trigger mouse events when the user clicks within the
projection of the quadrilateral region on the render window. The mouse
event handler will receive the relative position within the area at which the
mouse event occurred, where the top left corner has coordinates (0,0), and
the bottom right corner has coordinates (1,1). 

The area can define any flat quadrilateral in space. It is not limited to
rectangles, but all 4 points should be in the same plane.

@function VC_set_areaclickmode_quadrilateral
@tparam number area_id area identifier (as specified during area registration)
@tparam vector p1 top left corner of region
@tparam vector p2 top right corner
@tparam vector p3 bottom left corner
@tparam vector p4 bottom right corner
@usage oapi.VC_set_areaclickmode_quadrilateral(ID, area[1], area[2], area[3], area[4])
*/
/***
Associate a quadrilateral region in the virtual cockpit with a registered area to receive mouse events.
Variant of the 5 parameters function that takes an array as a second parameter
@function VC_set_areaclickmode_quadrilateral
@tparam number area_id area identifier (as specified during area registration)
@tparam table area table of 4 vectors ({p1,p2,p3,p4})
@usage oapi.VC_set_areaclickmode_quadrilateral(ID, area)
*/
int Interpreter::oapi_VC_set_areaclickmode_quadrilateral(lua_State* L)
{
	int id = (int)lua_tointeger(L, 1);
	if(lua_isvector(L, 2)) {
		VECTOR3 p1 = lua_tovector(L, 2);
		VECTOR3 p2 = lua_tovector(L, 3);
		VECTOR3 p3 = lua_tovector(L, 4);
		VECTOR3 p4 = lua_tovector(L, 5);
		oapiVCSetAreaClickmode_Quadrilateral(id, p1, p2, p3, p4);
	} else {
		lua_rawgeti(L, 2, 1);
		VECTOR3 p1 = lua_tovector(L, -1); lua_pop(L, 1);
		lua_rawgeti(L, 2, 2);
		VECTOR3 p2 = lua_tovector(L, -1); lua_pop(L, 1);
		lua_rawgeti(L, 2, 3);
		VECTOR3 p3 = lua_tovector(L, -1); lua_pop(L, 1);
		lua_rawgeti(L, 2, 4);
		VECTOR3 p4 = lua_tovector(L, -1); lua_pop(L, 1);
		oapiVCSetAreaClickmode_Quadrilateral(id, p1, p2, p3, p4);
	}
	return 0;
}

/***
Associate a spherical region in the virtual cockpit with a registered area to receive mouse events.

The area identifier must refer to an area which has previously been
registered with a call to oapi.VC_register_area(), with the required mouse event modes.

This function can be called repeatedly, to change the mouse-sensitive area.

@function VC_set_areaclickmode_spherical
@tparam number area_id area identifier (as specified during area registration)
@tparam vector cnt centre of active area in the local vessel frame
@tparam number radius  radius of active area [m]
@usage oapi.VC_set_areaclickmode_spherical(ID_HUDCOL, HUD_COLBTN_ref, VC_HUD_COLBTN_rad)
*/
int Interpreter::oapi_VC_set_areaclickmode_spherical(lua_State* L)
{
	int id = (int)lua_tointeger(L, 1);
	VECTOR3 cnt = lua_tovector(L, 2);
	double radius = lua_tonumber(L, 3);
	oapiVCSetAreaClickmode_Spherical(id,cnt, radius);
	return 0;
}

/***
Define an active area in a virtual cockpit.
Active areas can be repainted.

The target texture can be retrieved from a mesh by using the
oapi.get_texturehandle() method. Dynamic textures must be marked with flag "D" in the mesh file.

Redraw events can be used not only to update mesh textures dynamically,
but also to animate mesh groups, or edit mesh vertices or texture coordinates.

If no dynamic texture repaints are required during redraw events, use the
alternative version of oapiVCRegisterArea() instead.

To define a mouse-sensitive volume in the virtual cockpit, use one of the
oapiVCSetAreaClickmode_XXX functions.

@function VC_register_area
@tparam number area_id area identifier
@tparam rectangle tgtrect bounding box of the active area in the target texture (pixels)
@tparam number draw_event redraw condition
@tparam number mouse_event mouse event
@tparam number bkmode background mode
@tparam handle tgt target texture to be updated

@usage oapi.VC_register_area(ID, _R(0,0,1,1), PANEL_REDRAW.USER,
                      PANEL_MOUSE.IGNORE, PANEL_MAP.NONE, tex)
*/
/***
Define an active area in a virtual cockpit.
This version is used when no dynamic texture update is required during redraw events.

@function VC_register_area
@tparam number area_id area identifier (as specified during area registration)
@tparam number draw_event redraw condition
@tparam number mouse_event mouse event
@usage oapi.VC_register_area(ID_DISPLAY, PANEL_REDRAW.ALWAYS, PANEL_MOUSE.IGNORE)
*/
int Interpreter::oapi_VC_register_area(lua_State* L)
{
	int id = (int)lua_tointeger(L, 1);
	if (lua_isnumber(L, 2)) {
		int draw_event = (int)lua_tointeger(L, 2);
		int mouse_event = (int)lua_tointeger(L, 3);
		oapiVCRegisterArea(id, draw_event, mouse_event);
	} else {
		RECT tgtrect = lua_torect(L, 2);
		int draw_event = (int)lua_tointeger(L, 3);
		int mouse_event = (int)lua_tointeger(L, 4);
		int bkmode = (int)lua_tointeger(L, 5);
		SURFHANDLE tgt = (SURFHANDLE)lua_touserdata(L, 6);
		oapiVCRegisterArea(id, tgtrect, draw_event, mouse_event, bkmode, tgt);
	}
	return 0;
}

/***
Defines the neighbouring virtual cockpit camera positions in relation to the current
position.

The user can switch to neighbour positions with Ctrl-Arrow keys.

This function should be called during virtual cockpit registration (in clbk_loadVC())
to define the neighbouring cockpit camera positions, if any.

The left, right, top and bottom values specify the (zero-based) identifiers of
the VC positions to switch to when the user presses Ctrl and an arrow
button, or -1 if no position is available in this direction.

The neighbour relations should normally be reciprocal, i.e. if position 0
defines position 1 as its right neighbour, then position 1 should define
position 0 as its left neighbour.

If only a single VC position (id 0) is defined, this function doesn't need to be called.

Orbiter calls clbk_loadVC() with the appropriate id whenever the user switches to a new position.


Define an active area in a virtual cockpit.
This version is used when no dynamic texture update is required during redraw events.

@function VC_set_neighbours
@tparam number left panel id of left neighbour position (or -1 if none)
@tparam number right panel id of right neighbour position (or -1 if none)
@tparam number top panel id of top neighbour position (or -1 if none)
@tparam number bottom panel id of bottom neighbour position (or -1 if none)
@usage oapi.VC_set_neighbours (1, 2, -1, -1)
*/
int Interpreter::oapi_VC_set_neighbours(lua_State* L)
{
	int left = luaL_checkinteger(L, 1);
	int right = luaL_checkinteger(L, 2);
	int top = luaL_checkinteger(L, 3);
	int bottom = luaL_checkinteger(L, 4);
	oapiVCSetNeighbours(left, right, top, bottom);
	return 0;
}

/***
Define a render target for the head-up display (HUD) in a virtual cockpit.

This function should be placed in the body of the clbk_loadVC() vessel module callback function.

The hud specification is a table with the following fields :

- nmesh: number (mesh index)
- ngroup: number (group index)
- hudcnt: vector (HUD centre in vessel frame)
- size: number (physical size of the HUD [m])

The mesh group specified by nmesh and ngroup should be a square panel in
front of the camera position in the virtual cockpit. This group is rendered
separately from the rest of the mesh and should therefore have FLAG 2 set
in the mesh file. The group material and texture can be set to 0.

The HUD centre position and size are required to allow Orbiter to correctly scale the display.

Orbiter renders the HUD with completely transparent background. Rendering
the glass pane, brackets, etc. is up to the vessel designer.

@function VC_registerHUD
@tparam table hudspec hud specification
@usage local hud={nmesh=1, ngroup=GRP_VC.HUD, hudcnt=_V(0,1.462,7.09), size=0.15}
oapi.VC_registerHUD(hud)
*/
int Interpreter::oapi_VC_registerHUD(lua_State* L)
{
	VCHUDSPEC hs;
	lua_getfield(L, 1, "nmesh"); ASSERT_SYNTAX(lua_isnumber(L, -1), "Argument : missing field 'nmesh'");
	hs.nmesh = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 1, "ngroup"); ASSERT_SYNTAX(lua_isnumber(L, -1), "Argument : missing field 'ngroup'");
	hs.ngroup = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 1, "hudcnt"); ASSERT_SYNTAX(lua_isvector(L, -1), "Argument : missing field 'hudcnt'");
	hs.hudcnt = lua_tovector(L, -1); lua_pop(L, 1);
	lua_getfield(L, 1, "size"); ASSERT_SYNTAX(lua_isnumber(L, -1), "Argument : missing field 'size'");
	hs.size = lua_tonumber(L, -1); lua_pop(L, 1);

	oapiVCRegisterHUD(&hs);
	return 0;
}

/***
Define a render target for rendering an MFD display in a virtual cockpit.

The render target specification is defined as a table with the following fields :

- nmesh: number (mesh index)
- ngroup: number (group index)

This function should be placed in the body of the clbk_loadVC vessel module callback function.
The addressed mesh group should define a simple square (4 vertices, 2 triangles). The group materials and textures can be set to 0.

@function VC_registermfd
@tparam number mfd MFD identifier (e.g. MFDID.LEFT, MFDID.RIGHT)
@tparam table spec render target specification
@usage 	local mfds_left  = {nmesh=1, ngroup=GRP_VC.LMFD_DISPLAY}
oapi.VC_registermfd (MFDID.LEFT, mfds_left)
*/
int Interpreter::oapi_VC_registermfd(lua_State* L)
{
	VCMFDSPEC spec;
	int mfd = luaL_checkinteger(L, 1);
	lua_getfield(L, 2, "nmesh"); ASSERT_SYNTAX(lua_isnumber(L, -1), "Argument : missing field 'nmesh'");
	spec.nmesh = luaL_checkinteger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 2, "ngroup"); ASSERT_SYNTAX(lua_isnumber(L, -1), "Argument : missing field 'ngroup'");
	spec.ngroup = luaL_checkinteger(L, -1); lua_pop(L, 1);

	oapiVCRegisterMFD(mfd, &spec);
	return 0;
}

/***
Returns the current cockpit display mode.

This function also works if the camera is not currently in cockpit mode.

@function cockpit_mode
@treturn number current cockpit display mode
@usage local isVC = oapi.cockpit_mode() == COCKPIT.VIRTUAL
*/
int Interpreter::oapi_cockpit_mode(lua_State* L)
{
	lua_pushnumber(L, oapiCockpitMode());
	return 1;
}

/***
Render custom HUD elements.

This function should only be called from within clbk_renderHUD.

It can be used to render custom HUD elements in glass cockpit and 2-D panel mode.

The mesh handle must refer to a 2-D mesh (z-components of all vertices
are zero). The x and y components are in units of screen pixels.

The mesh may have multiple groups, but generally a single group should
be sufficient. The texture indices of each group refer to the textures in
the hTex list (starting with 0). If only a single texture is used, the
texture index in the mesh should be set to 0, and hTex should be a pointer
to the surface handle.

Mesh animations can be applied by modifying vertex and/or texture
coordinates at each frame.

@function render_hud
@tparam handle hMesh HUD mesh handle
@tparam table hTex array of texture handles
@usage oapi.render_hud(hmesh, {hTex})
*/
int Interpreter::oapi_render_hud(lua_State* L)
{
	MESHHANDLE hMesh = lua_tomeshhandle(L, 1);
	int nSurf = lua_rawlen(L, 2);
	SURFHANDLE *hSurf = new SURFHANDLE[nSurf];

	for ( int i=1 ; i <= nSurf; i++ ) {
		lua_rawgeti(L,2,i);
		if ( lua_isnil(L,-1) ) {
			return luaL_error(L, "Error iterating over surfaces array");
		}
		hSurf[i-1] = (SURFHANDLE)lua_touserdata(L, -1);
		lua_pop(L,1);
	}

	oapiRenderHUD(hMesh, hSurf);
	delete []hSurf;
	return 0;
}

/***
Return the current HUD brightness setting.

@function get_hudintensity
@treturn number brightness value [0-1]
@usage local isVC = oapi.cockpit_mode() == COCKPIT.VIRTUAL
*/
int Interpreter::oapi_get_hudintensity(lua_State* L)
{
	double val = oapiGetHUDIntensity ();
	lua_pushnumber(L, val);
	return 1;
}

/***
Set the HUD brightness.

@function set_hudintensity
@tparam number val brightness setting [0-1]
@usage oapi.set_hudintensity(hud_brightness)
*/

int Interpreter::oapi_set_hudintensity(lua_State* L)
{
	double val = luaL_checknumber(L, 1);
	oapiSetHUDIntensity(val);
	return 0;
}

/***
Increase the brightness of the HUD display.

Calling this function will increase the intensity (in virtual cockpit modes) or
brightness (in other modes) of the HUD display up to a maximum value.

This function should be called repeatedly (e.g. while the user presses a key).

@function inc_hudintensity
*/
int Interpreter::oapi_inc_hudintensity(lua_State* L)
{
	oapiIncHUDIntensity();
	return 0;
}

/***
Decrease the brightness of the HUD display.

Calling this function will decrease the intensity (in virtual cockpit modes) or
brightness (in other modes) of the HUD display up to a maximum value.

This function should be called repeatedly (e.g. while the user presses a key).

@function dec_hudintensity
*/
int Interpreter::oapi_dec_hudintensity(lua_State* L)
{
	oapiDecHUDIntensity();
	return 0;
}

/***
Switch the HUD display to a different colour.

Orbiter currently defines  HUD colours: green, red, yellow and blue. Calls to
oapi.toggle_hudcolour will cycle through these.

@function toggle_hudcolour
*/
int Interpreter::oapi_toggle_hudcolour(lua_State* L)
{
	oapiToggleHUDColour();
	return 0;
}

/***
Return the display mode of the main menu bar.

@function get_mainmenuvisibilitymode
@treturn number 0=show, 1=hide, 2=auto-hide
*/
int Interpreter::oapi_get_mainmenuvisibilitymode(lua_State* L)
{
	lua_pushnumber (L, oapiGetMainMenuVisibilityMode());
	return 1;
}

/***
Set the display mode for the main menu bar.

@function set_mainmenuvisibilitymode
@tparam number mode (0=show, 1=hide, 2=auto-hide)
*/
int Interpreter::oapi_set_mainmenuvisibilitymode (lua_State *L)
{
	ASSERT_SYNTAX (lua_isnumber (L,1), "Argument 1: invalid type (expected number)");
	DWORD mode = (DWORD)lua_tonumber (L,1);
	ASSERT_SYNTAX (mode <= 2, "Argument 1: out of range");
	oapiSetMainMenuVisibilityMode (mode);
	return 0;
}

/***
Returns the display mode of the two info blocks at the top left and right screen corners.

@function get_maininfovisibilitymode
@treturn number 0=show, 1=hide, 2=auto-hide
*/
int Interpreter::oapi_get_maininfovisibilitymode (lua_State *L)
{
	lua_pushnumber (L, oapiGetMainInfoVisibilityMode());
	return 1;
}

/***
Set the display mode for the two info blocks at the top left and right screen corners.

@function set_maininfovisibilitymode
@tparam number mode (0=show, 1=hide, 2=auto-hide)
*/
int Interpreter::oapi_set_maininfovisibilitymode (lua_State *L)
{
	ASSERT_SYNTAX (lua_isnumber (L,1), "Argument 1: invalid type (expected number)");
	DWORD mode = (DWORD)lua_tonumber (L,1);
	ASSERT_SYNTAX (mode <= 2, "Argument 1: out of range");
	oapiSetMainInfoVisibilityMode (mode);
	return 0;
}

/***
Create an annotation for displaying onscreen text during a simulation.

@function create_annotation
@treturn annotation Annotation object
*/
int Interpreter::oapiCreateAnnotation (lua_State *L)
{
	NOTEHANDLE *pnote = (NOTEHANDLE*)lua_newuserdata (L, sizeof(NOTEHANDLE));
	*pnote = ::oapiCreateAnnotation (true, 1.0, _V(1,0.8,0.6));
	oapiAnnotationSetPos (*pnote, 0.03, 0.2, 0.4, 0.4);

	g_notehandles.push_back(pnote);

	luaL_getmetatable (L, "NOTE.table");   // push metatable
	lua_setmetatable (L, -2);              // set metatable for annotation objects
	return 1;
}

int Interpreter::oapiGetAnnotations (lua_State *L)
{
	for (auto it = g_notehandles.begin(); it != g_notehandles.end(); ++it) {
		lua_pushlightuserdata(L, *it);
		luaL_getmetatable (L, "NOTE.table");   // push metatable
		lua_setmetatable (L, -2);              // set metatable for annotation objects
	}
	return g_notehandles.size();
}

/***
Delete an annotation.

@function del_annotation
@tparam annotation note Annotation object
*/
int Interpreter::oapiDelAnnotation (lua_State *L)
{
	NOTEHANDLE *pnote = (NOTEHANDLE*)lua_touserdata (L, 1);
	::oapiDelAnnotation (*pnote);

	g_notehandles.remove(pnote);

	*pnote = NULL;
	return 0;
}

/***
Display a string in the lower left corner of the viewport.

This function should only be used for debugging purposes. 
Do not use it in published modules!

If the string is written to more than once per time step (either within a single
module or by multiple modules) the last state before rendering will be displayed.

@function dbg_out
@tparam string str string to display
*/
int Interpreter::oapiDbgOut (lua_State *L)
{
	const char *str = lua_tostringex (L, 1);
	strcpy (oapiDebugString(), str);
	return 0;
}

/***
Writes a line to the Orbiter log file (orbiter.log) in the main orbiter directory.

This function is intended for diagnostic initialisation and error messages by
plugin modules. The messages should make it easier to track problems.

Avoid unnecessary output. In particular, don't write to the log file continously
from within the simulation loop.

@function write_log
@tparam string str string to be written
*/
int Interpreter::oapiWriteLog(lua_State* L)
{
	const char* str = lua_tostringex(L, 1);
	::oapiWriteLog(const_cast<char*>(str));
	return 0;
}

/***
Abort the simulation.

This function abruptly exits the simulation without saving the current simulation state.

This function should only be used for debugging purposes. 
Do not use it in published modules!

@function exit
*/
int Interpreter::oapiExit(lua_State* L)
{
	auto code = lua_tointeger(L, 1);
	exit(code);
	return 0; // compiler warnings
}

static bool bInputClosed;
static char cInput[1024];

bool inputClbk (void *id, char *str, void *usrdata)
{
	strncpy (cInput, str, 1024);
	bInputClosed = true;
	return true;
}

bool inputCancel (void *id, char *str, void *usrdata)
{
	cInput[0] = '\0';
	bInputClosed = true;
	return true;
}

/***
Open a modal input box - deprecated.

This function opens an input box requesting a string from the user.
The input box is modal, i.e. all keyboard input is redirected into the dialog
Normal key functions resume after the box is closed.

The dialog will stay open until the user enters a value (cannot be cancelled).

Use the oapi.receive_input function to recover the input text.

This function is primarily used internally, you should prefer using open_inputboxex in your own modules since it's more flexible.

@function open_inputbox
@tparam string title input box title
@see open_inputboxex
@usage oapi.open_inputbox(title)
 -- elsewhere
 local ans = oapi.receive_input ()
 if ans then
     oapi.dbg_out(ans)
 end
*/
int Interpreter::oapiOpenInputBox (lua_State *L)
{
	const char *title = lua_tostring (L, 1);
	int vislen = lua_tointeger (L, 2);
	bInputClosed = false;
	oapiOpenInputBoxEx (title, inputClbk, inputCancel, 0, 40, 0, USRINPUT_NEEDANSWER);
	return 0;
}

/***
Get value from a modal input box - deprecated.

This function returns the last value entered by the user in conjuction with an oapi.open_inputbox call.

Since the value is not reset after reading, you must provide your own logic to detect when the user entered a value.

Prefer using open_inputboxex which does not have this limitation.

@function receive_input
@treturn string|nil value entered or nil if the input box is still open
@see open_inputbox
@see open_inputboxex
*/
int Interpreter::oapiReceiveInput (lua_State *L)
{
	if (bInputClosed)
		lua_pushstring (L, cInput);
	else
		lua_pushnil (L);
	return 1;
}

typedef struct {
	int ref_enter;
	int ref_cancel;
	int usr_data;
	lua_State *L;
} lua_inputbox_ctx;

static bool Clbk_enter(void *id, char *str, void *ctx)
{
	lua_inputbox_ctx *ibctx = (lua_inputbox_ctx *)ctx;
	lua_rawgeti(ibctx->L, LUA_REGISTRYINDEX, ibctx->ref_enter);   // push the callback function
	lua_pushstring (ibctx->L, str);
	lua_rawgeti(ibctx->L, LUA_REGISTRYINDEX, ibctx->usr_data);   // push the usr_data
	Interpreter::LuaCall (ibctx->L, 2, 1);
	bool ret = lua_toboolean(ibctx->L, -1);
	if(ret) {
		luaL_unref(ibctx->L, LUA_REGISTRYINDEX, ibctx->ref_enter);
		luaL_unref(ibctx->L, LUA_REGISTRYINDEX, ibctx->ref_cancel);
		luaL_unref(ibctx->L, LUA_REGISTRYINDEX, ibctx->usr_data);
		delete ibctx;
	}
	return ret;
}

static bool Clbk_cancel(void *id, char *str, void *ctx)
{
	lua_inputbox_ctx *ibctx = (lua_inputbox_ctx *)ctx;
	if(ibctx->ref_cancel != LUA_REFNIL) {
		lua_rawgeti(ibctx->L, LUA_REGISTRYINDEX, ibctx->ref_cancel); // push the callback function
		lua_pushstring (ibctx->L, str);
		lua_rawgeti(ibctx->L, LUA_REGISTRYINDEX, ibctx->usr_data);   // push the usr_data
		Interpreter::LuaCall (ibctx->L, 2, 0);
	}
	luaL_unref(ibctx->L, LUA_REGISTRYINDEX, ibctx->ref_enter);
	luaL_unref(ibctx->L, LUA_REGISTRYINDEX, ibctx->ref_cancel);
	luaL_unref(ibctx->L, LUA_REGISTRYINDEX, ibctx->usr_data);
	delete ibctx;
	return true;
}

/***
Opens a modal input box requesting a string from the user.

The callback functions both take the current string and the usrdata as parameters.
The cbEnter callback should return true if it accepts the string, false
otherwise (the box will not be closed if the callback function returns false).

The box can be closed by the user by pressing Enter ("OK") or Esc
("Cancel"). The corresponding callback is then called.

The input box is modal, i.e. all keyboard input is redirected into the dialog
box. Normal key functions resume after the box is closed.

CAVEAT: In Lua, function names are values so they must be declared before using them.

@function open_inputboxex
@tparam string title input box title
@tparam function cbEnter callback function receiving the result of the user input
@tparam[opt=nil] function cbCancel callback function called when the user cancels the inputbox
@tparam[opt=""] string init initial value of the inputbox
@tparam[opt=20] number vislen number of characters visible in input box
@tparam[opt] any usrdata user-defined data passed to the callback function
@tparam[opt=0] number flags USRINPUT.NEEDANSWER (not cancellable) or 0
@usage function cbEnter(str, obj)
    local val = tonumber(str)
    if val then
        obj:update_value(val)
        return true
    else -- not a number
        return false
    end
 end

 function cbCancel(str) -- we don't use the user data so no need to declare it
    oapi.dbg_out("Cancelled")
 end

 function myclass:ask_number()
    oapi.open_inputboxex("Enter a number", cbEnter, cbCancel, "", 20, self)
 end

*/
int Interpreter::oapi_open_inputboxex (lua_State *L)
{
	const char *title = luaL_checkstring(L, 1);
	int ref_enter = LUA_REFNIL;
	int ref_cancel = LUA_REFNIL;
	int usr_data = LUA_REFNIL;
	char *buf = NULL;
	int vislen = 20;
	int flags = 0;
	if (lua_isfunction(L, 2)) {
		lua_pushvalue(L, 2);
		ref_enter = luaL_ref(L, LUA_REGISTRYINDEX);
	} else {
		luaL_error(L, "Argument 2 must be a function");
	}
	if (lua_isfunction(L, 3)) {
		lua_pushvalue(L, 3);
		ref_cancel = luaL_ref(L, LUA_REGISTRYINDEX);
	}
	if (lua_isstring(L, 4)) {
		buf = const_cast<char *>(lua_tostring(L, 4));
	}
	if (lua_isnumber(L, 5)) {
		vislen = lua_tointeger(L, 5);
	}
	lua_pushvalue(L, 6);
	usr_data = luaL_ref(L, LUA_REGISTRYINDEX);
	if (lua_isnumber(L, 7)) {
		flags = lua_tointeger(L, 7);
	}
	lua_inputbox_ctx *ctx = new lua_inputbox_ctx();
	ctx->ref_enter = ref_enter;
	ctx->ref_cancel = ref_cancel;
	ctx->usr_data = usr_data;
	ctx->L = L;

	oapiOpenInputBoxEx (title, Clbk_enter, Clbk_cancel, buf, vislen, ctx, flags);
	return 0;
}

/***
Return the equatorial coordinates with respect to an object
of a point given in the global reference frame.

@function global_to_equ
@tparam handle hObj object handle
@tparam vector glob point in global coordinates
@treturn table|nil
The returned table contains the equatorial coordinates and has the following fields:

- lng: number (longitude [rad])
- lat: number (latitude [rad])
- rad: number (radial distance [m])
*/
int Interpreter::oapi_global_to_equ(lua_State* L)
{
	OBJHANDLE hObj;
	if (lua_islightuserdata(L, 1) && (hObj = lua_toObject(L, 1))) {
		VECTOR3 glob = lua_tovector(L, 2);
		double lng, lat, rad;
		oapiGlobalToEqu(hObj, glob, &lng, &lat, &rad);
		lua_createtable(L, 0, 3);
		lua_pushnumber(L, lng);
		lua_setfield(L, -2, "lng");
		lua_pushnumber(L, lat);
		lua_setfield(L, -2, "lat");
		lua_pushnumber(L, rad);
		lua_setfield(L, -2, "rad");
	}
	else lua_pushnil(L);
	return 1;
}

/***
Map a point from the global frame to a local object frame.

@function global_to_local
@tparam handle hObj object handle
@tparam vector glob point in global coordinates
@treturn vector|nil point mapped into local coordinates
*/
int Interpreter::oapi_global_to_local(lua_State* L)
{
	OBJHANDLE hObj;
	if (lua_islightuserdata(L, 1) && (hObj = lua_toObject(L, 1))) {
		VECTOR3 glob = lua_tovector(L, 2);
		VECTOR3 loc;
		oapiGlobalToLocal(hObj, &glob, &loc);
		lua_pushvector(L, loc);
	}
	else lua_pushnil(L);
	return 1;
}

/***
Return the equatorial coordinates of a point given in the local frame of an object.

@function local_to_equ
@tparam handle hObj object handle
@tparam vector loc point in cartesian coordinates of the local object frame [m]
@treturn table|nil
The returned table contains the equatorial coordinates and has the following fields:

- lng: number (longitude [rad])
- lat: number (latitude [rad])
- rad: number (radial distance [m])
*/
int Interpreter::oapi_local_to_equ(lua_State* L) {
	OBJHANDLE hObj;
	if (lua_islightuserdata(L, 1) && (hObj = lua_toObject(L, 1))) {
		VECTOR3 loc = lua_tovector(L, 2);
		double lng, lat, rad;
		oapiLocalToEqu(hObj, loc, &lng, &lat, &rad);
		lua_createtable(L, 0, 3);
		lua_pushnumber(L, lng);
		lua_setfield(L, -2, "lng");
		lua_pushnumber(L, lat);
		lua_setfield(L, -2, "lat");
		lua_pushnumber(L, rad);
		lua_setfield(L, -2, "rad");
	}
	else lua_pushnil(L);
	return 1;
	
}

/***
 Return the global cartesian position of a point given in equatorial coordinates of an object.
 
@function equ_to_global
@tparam handle hObj object handle
@tparam table pos Equatorial coordinates provided as a table with the following fields:

- lng: number (longitude [rad])
- lat: number (latitude [rad])
- rad: number (radial distance [m])
@treturn vector point in cartesian coordinates of the global reference frame [m]
*/
int Interpreter::oapi_equ_to_global (lua_State *L)
{
	OBJHANDLE hObj;
	double lng, lat, rad;
	VECTOR3 glob;
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
	ASSERT_SYNTAX (lua_istable (L,2), "Argument 2: invalid type (expected table)");
	lua_getfield (L,2,"lng");
	ASSERT_SYNTAX (lua_isnumber (L,-1), "Argument 2: missing field 'lng'");
	lng = (double)lua_tonumber (L,-1); lua_pop (L,1);
	lua_getfield (L,2,"lat");
	ASSERT_SYNTAX (lua_isnumber (L,-1), "Argument 2: missing field 'lat'");
	lat = (double)lua_tonumber (L,-1); lua_pop (L,1);
	lua_getfield (L,2,"rad");
	ASSERT_SYNTAX (lua_isnumber (L,-1), "Argument 2: missing field 'rad'");
	rad = (double)lua_tonumber (L,-1); lua_pop (L,1);

	oapiEquToGlobal (hObj, lng, lat, rad, &glob);
	lua_pushvector (L, glob);
	return 1;
}

/***
Returns the angular distance of two points on a sphere.

 Given two points on the surface of a sphere, this function returns
 the orthodome (shortest) angular distance between them.
 
 The shortest surface path between the points is an arc on a great
 circle containing the two points, and its length is given by
 d = a R, where a is the angular distance returned by oapiOrthodome, and
 R is the radius of the sphere.

Positions are given in the form of tables with the following fields :

- lng: number (longitude [rad])
- lat: number (latitude [rad])
 
@function orthodome
@tparam table p1 first point position
@tparam table p2 second point position
@treturn number angular distance of two points [rad]
*/
int Interpreter::oapi_orthodome (lua_State *L)
{
	double lng1, lat1, lng2, lat2, alpha;
	ASSERT_SYNTAX (lua_gettop (L) >= 2, "Too few arguments");
	ASSERT_SYNTAX (lua_istable (L,1), "Argument 1: invalid type (expected table)");
	ASSERT_SYNTAX (lua_istable (L,2), "Argument 2: invalid type (expected table)");
	
	lua_getfield (L, 1, "lng");
	ASSERT_SYNTAX (lua_isnumber (L,-1), "Argument 1: missing field 'lng'");
	lng1 = (double)lua_tonumber (L,-1); lua_pop (L,1);
	lua_getfield (L, 1, "lat");
	ASSERT_SYNTAX (lua_isnumber (L,-1), "Argument 1: missing field 'lat'");
	lat1 = (double)lua_tonumber (L,-1); lua_pop (L,1);

	lua_getfield (L, 2, "lng");
	ASSERT_SYNTAX (lua_isnumber (L,-1), "Argument 2: missing field 'lng'");
	lng2 = (double)lua_tonumber (L,-1); lua_pop (L,1);
	lua_getfield (L, 2, "lat");
	ASSERT_SYNTAX (lua_isnumber (L,-1), "Argument 2: missing field 'lat'");
	lat2 = (double)lua_tonumber (L,-1); lua_pop (L,1);

	alpha = oapiOrthodome (lng1, lat1, lng2, lat2);
	lua_pushnumber (L, alpha);
	return 1;
}

/***
Return the size (mean radius) of an object.
 
@function get_size
@tparam handle hObj object handle
@treturn number object size (mean radius) [m]
*/
int Interpreter::oapi_get_size (lua_State *L)
{
	OBJHANDLE hObj;
	ASSERT_SYNTAX(lua_gettop(L) >= 1, "Too few arguments");
	ASSERT_SYNTAX(lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(hObj = lua_toObject (L,1), "Argument 1: invalid object");
	lua_pushnumber (L, oapiGetSize(hObj));
	return 1;
}

/***
Return the mass of an object. For vessels, this is the total mass, including current fuel mass.
 
@function get_mass
@tparam handle hObj object handle
@treturn number object mass [kg]
@see get_emptymass
*/
int Interpreter::oapi_get_mass (lua_State *L)
{
	OBJHANDLE hObj;
	ASSERT_SYNTAX(lua_gettop(L) >= 1, "Too few arguments");
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
	lua_pushnumber (L, oapiGetMass (hObj));
	return 1;
}

/***
Return the position of an object in the global reference frame.

The global reference frame is the heliocentric ecliptic system at ecliptic and
equinox of J2000.
 
@function get_globalpos
@tparam[opt=current focus object] handle hObj object handle
@treturn vector coordinates [m]
*/
int Interpreter::oapi_get_globalpos (lua_State *L)
{
	VECTOR3 pos;
	if (lua_gettop(L) < 1) {
		oapiGetFocusGlobalPos (&pos);
	} else {
		OBJHANDLE hObj;
		ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
		ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
		oapiGetGlobalPos (hObj, &pos);
	}
	lua_pushvector (L, pos);
	return 1;
}

/***
Return the velocity of an object in the global reference frame.

The global reference frame is the heliocentric ecliptic system at ecliptic and
equinox of J2000.
 
@function get_globalvel
@tparam[opt=current focus object] handle hObj object handle
@treturn vector coordinates [m/s]
*/
int Interpreter::oapi_get_globalvel (lua_State *L)
{
	VECTOR3 vel;
	if (lua_gettop(L) < 1) {
		oapiGetFocusGlobalVel (&vel);
	} else {
		OBJHANDLE hObj;
		ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
		ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
		oapiGetGlobalVel (hObj, &vel);
	}
	lua_pushvector (L, vel);
	return 1;
}

/***
Return the distance vector between 2 objects in the ecliptic reference frame.

Results are w.r.t. ecliptic frame at equinox and ecliptic of J2000.0.
 
@function get_relativepos
@tparam[opt=current focus object] handle hObj object handle
@tparam handle hRef object handle
@treturn vector distance from hRef to hObj [m]
*/
int Interpreter::oapi_get_relativepos (lua_State *L)
{
	OBJHANDLE hObj, hRef;
	VECTOR3 pos;
	int narg = min(lua_gettop(L),2);
	ASSERT_SYNTAX (lua_islightuserdata (L,narg), "Argument 2: invalid type (expected handle)");
	ASSERT_SYNTAX (hRef = lua_toObject (L,narg), "Argument 2: invalid object");
	if (narg > 1) {
		ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
		ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
		oapiGetRelativePos (hObj, hRef, &pos);
	} else {
		oapiGetFocusRelativePos (hRef, &pos);
	}
	lua_pushvector (L, pos);
	return 1;
}

/***
Return the velocity difference in the ecliptic reference frame.

Results are w.r.t. ecliptic frame at equinox and ecliptic of J2000.0.
 
@function get_relativevel
@tparam[opt=current focus object] handle hObj object handle
@tparam handle hRef object handle
@treturn vector velocity difference vector of hObj relative to hRef [m/s]
*/
int Interpreter::oapi_get_relativevel (lua_State *L)
{
	OBJHANDLE hObj, hRef;
	VECTOR3 vel;
	int narg = min(lua_gettop(L),2);
	ASSERT_SYNTAX (lua_islightuserdata (L,narg), "Argument 2: invalid type (expected handle)");
	ASSERT_SYNTAX (hRef = lua_toObject (L,narg), "Argument 2: invalid object");
	if (narg > 1) {
		ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
		ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
		oapiGetRelativeVel (hObj, hRef, &vel);
	} else {
		oapiGetFocusRelativeVel (hRef, &vel);
	}
	lua_pushvector (L, vel);
	return 1;
}

/***
Return the rotation period (the length of a siderial day) of a planet.
 
@function get_planetperiod
@tparam handle hPlanet planet handle
@treturn number planet rotation period [s]
*/
int Interpreter::oapi_get_planetperiod(lua_State* L)
{
	OBJHANDLE hRef;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(hRef = lua_toObject(L, 1), "Argument 1: invalid object");
	double T = oapiGetPlanetPeriod(hRef);

	lua_pushnumber(L, T);
	return 1;
}

/***
Return atmospheric constants for a planet.
 
@function get_planetatmconstants
@tparam handle hPlanet planet handle
@treturn table planet atmospheric coefficients, with the following fields:

- p0: number (pressure at mean radius ('sea level') [Pa])
- rho0: number (density at mean radius [kg/m3])
- R: number (specific gas constant [J/(K kg)])
- gamma: number (ratio of specific heats, c_p/c_v)
- C: number (exponent for pressure equation (temporary))
- O2pp: number (partial pressure of oxygen)
- altlimit: number (atmosphere altitude limit [m])
- radlimit: number (radius limit (altlimit + mean radius))
- horizonalt: number (horizon rendering altitude)
- color0: vector (sky colour at sea level during daytime)
*/
int Interpreter::oapi_get_planetatmconstants(lua_State* L)
{
	OBJHANDLE hRef;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 2: invalid type (expected handle)");
	ASSERT_SYNTAX(hRef = lua_toObject(L, 1), "Argument 2: invalid object");
	const ATMCONST *c = oapiGetPlanetAtmConstants(hRef);

	if(c) {
		lua_createtable (L, 0, 10);
		lua_pushnumber (L, c->p0);
		lua_setfield (L, -2, "p0");
		lua_pushnumber (L, c->rho0);
		lua_setfield (L, -2, "rho0");
		lua_pushnumber (L, c->R);
		lua_setfield (L, -2, "R");
		lua_pushnumber (L, c->gamma);
		lua_setfield (L, -2, "gamma");
		lua_pushnumber (L, c->C);
		lua_setfield (L, -2, "C");
		lua_pushnumber (L, c->O2pp);
		lua_setfield (L, -2, "O2pp");
		lua_pushnumber (L, c->altlimit);
		lua_setfield (L, -2, "altlimit");
		lua_pushnumber (L, c->radlimit);
		lua_setfield (L, -2, "radlimit");
		lua_pushnumber (L, c->horizonalt);
		lua_setfield (L, -2, "horizonalt");
		lua_pushvector (L, c->color0);
		lua_setfield (L, -2, "color0");
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/***
Return the type of an object identified by its handle.
 
@function get_objecttype
@tparam handle hObj object handle
@treturn number object type
*/
int Interpreter::oapi_get_objecttype(lua_State* L)
{
	OBJHANDLE hRef;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(hRef = lua_toObject(L, 1), "Argument 1: invalid object");
	int type = oapiGetObjectType(hRef);

	lua_pushnumber(L, type);
	return 1;
}

/***
Return the parent object of a celestial body.

The parent is the body being orbited by hBody, e.g. the central star if
hBody is a planet, or the planet if hBody is a moon.

hBody must refer to a celestial body (type = OBJTP.PLANET or OBJTP.STAR),
otherwise the result is undefined.

@function get_gbodyparent
@tparam handle hBody celestial body handle
@treturn handle|nil parent body handle or nil if no parent.
*/
int Interpreter::oapi_get_gbodyparent(lua_State* L)
{
	OBJHANDLE hRef;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(hRef = lua_toObject(L, 1), "Argument 1: invalid object");
	OBJHANDLE hObj = oapiGetGbodyParent(hRef);

	if(hObj)
		lua_pushlightuserdata(L, hObj);
	else
		lua_pushnil(L);
	return 1;
}

/***
Return the handle of a celestial body (sun, planet or moon) identified
by its name or list index.

Celestial bodies in orbiter are objects that act as sources for
gravitational fields.

@function get_gbody
@tparam string|number param celestial object name (not case-sensitive) or object index (0 <= index < oapi.get_gbodycount())
@treturn handle|nil body handle or nil if not found.
@see get_gbodycount
*/
int Interpreter::oapi_get_gbody(lua_State* L)
{
	OBJHANDLE hObj = NULL;
	if(lua_isnumber(L, 1)) {
		int idx = lua_tointeger(L, 1);
		hObj = oapiGetGbodyByIndex(idx);
	} else if(lua_isstring(L, 1)) {
		char *name = const_cast<char *>(lua_tostring(L, 1));
		hObj = oapiGetGbodyByName(name);
	} else {
		ASSERT_SYNTAX(false, "Argument 1: name(string) or index(number) required");
	}
	
	if(hObj)
		lua_pushlightuserdata(L, hObj);
	else
		lua_pushnil(L);
	return 1;
}

/***
Return the number of celestial bodies (sun, planets and moons) currently
present in the simulation.
 
@function get_gbodycount
@treturn number Number of objects.
*/
int Interpreter::oapi_get_gbodycount(lua_State *L)
{
	int nBody = oapiGetGbodyCount();
	lua_pushnumber(L, nBody);
	return 1;
}

/***
Returns the obliquity of the planet's rotation axis (the angle between the rotation axis
and the ecliptic zenith).

In Orbiter, the ecliptic zenith (at epoch J2000) is the positive y-axis of the
global frame of reference.

@function get_planetobliquity
@tparam handle hPlanet planet handle
@treturn number obliquity [rad]
*/
int Interpreter::oapi_get_planetobliquity(lua_State *L)
{
	OBJHANDLE hRef;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(hRef = lua_toObject(L, 1), "Argument 1: invalid object");
	double ob = oapiGetPlanetObliquity(hRef);
	lua_pushnumber(L, ob);
	return 1;
}

/***
Returns the longitude of the ascending node.

Returns the longitude of the ascending node of the equatorial plane,
that is, the angle between the vernal equinox and the ascending node of the equator w.r.t. the ecliptic.

For Earth, this function will return 0. (The ascending node of Earth's
equatorial plane is the definition of the vernal equinox).

@function get_planettheta
@tparam handle hPlanet planet handle
@treturn number longitude of ascending node of the equator [rad]
*/
int Interpreter::oapi_get_planettheta(lua_State *L)
{
	OBJHANDLE hRef;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(hRef = lua_toObject(L, 1), "Argument 1: invalid object");
	double theta = oapiGetPlanetTheta(hRef);
	lua_pushnumber(L, theta);
	return 1;
}

/***
Returns a rotation matrix which performs the transformation from the planet's tilted
coordinates into global coordinates.

@function get_planetobliquitymatrix
@tparam handle hPlanet planet handle
@treturn matrix rotation data
*/
int Interpreter::oapi_get_planetobliquitymatrix(lua_State *L)
{
	OBJHANDLE hRef;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(hRef = lua_toObject(L, 1), "Argument 1: invalid object");
	MATRIX3 ob;
	oapiGetPlanetObliquityMatrix(hRef, &ob);
	lua_pushmatrix(L, ob);
	return 1;
}

/***
Returns the current rotation angle of the planet around its axis.

@function get_planetcurrentrotation
@tparam handle hPlanet planet handle
@treturn matrix Rotation angle [rad]
*/
int Interpreter::oapi_get_planetcurrentrotation(lua_State *L)
{
	OBJHANDLE hRef;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(hRef = lua_toObject(L, 1), "Argument 1: invalid object");
	double rot = oapiGetPlanetCurrentRotation(hRef);
	lua_pushnumber(L, rot);
	return 1;
}

/***
Test for existence of planetary atmosphere.

@function planet_hasatmosphere
@tparam handle hPlanet planet handle
@treturn boolean true if an atmosphere has been defined for the planet, false otherwise.
*/
int Interpreter::oapi_planet_hasatmosphere(lua_State *L)
{
	OBJHANDLE hRef;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(hRef = lua_toObject(L, 1), "Argument 1: invalid object");
	bool atm = oapiPlanetHasAtmosphere(hRef);
	lua_pushboolean(L, atm);
	return 1;
}

/***
Returns atmospheric parameters as a function of distance from the planet centre.

If the planet has no atmosphere, or if the defined radius is beyond the
defined upper atmosphere limit, all parameters are set to 0.

If the atmosphere model is position- as well as altitude-dependent, this
function assumes longitude=0 and latitude=0.

The returned table has the following fields:

- T: number (temperature [K])
- p: number (pressure [Pa])
- rho: number (density [kg/m^3])

@function get_planetatmparams
@tparam handle hPlanet planet handle
@tparam number rad radius from planet centre [m]
@treturn table atmosphere parameters
*/
/***
Returns atmospheric parameters of a planet as a function of altitude
and geographic position.

The returned table has the following fields:

- T: number (temperature [K])
- p: number (pressure [Pa])
- rho: number (density [kg/m^3])

@function get_planetatmparams
@tparam handle hPlanet planet handle
@tparam number alt altitude above planet mean radius [m]
@tparam number lng longitude [rad]
@tparam number lat latitude [rad]
@treturn table atmospehere parameters
*/
int Interpreter::oapi_get_planetatmparams(lua_State* L)
{
	OBJHANDLE hRef;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(hRef = lua_toObject(L, 1), "Argument 1: invalid object");
	ATMPARAM ap;

	if (lua_gettop(L) == 2) {
		double rad = luaL_checknumber(L, 2);
		oapiGetPlanetAtmParams(hRef, rad, &ap);
	} else {
		double alt = luaL_checknumber(L, 2);
		double lng = luaL_checknumber(L, 3);
		double lat = luaL_checknumber(L, 4);
		oapiGetPlanetAtmParams(hRef, alt, lng, lat, &ap);
	}
	lua_createtable (L, 0, 3);
	lua_pushnumber (L, ap.T);
	lua_setfield (L, -2, "T");
	lua_pushnumber (L, ap.p);
	lua_setfield (L, -2, "p");
	lua_pushnumber (L, ap.rho);
	lua_setfield (L, -2, "rho");

	return 1;
}

/***
Returns the velocity vector of a surface point.

The frame flag can be used to specify the reference frame to which the
returned vector refers. The following values are supported:

- 0: surface-relative (relative to local horizon)
- 1: planet-local (relative to local planet frame)
- 2: planet-local non-rotating
- 3: global (maps to global frame and adds planet velocity)

@function get_groundvector
@tparam handle hPlanet planet handle
@tparam number lng longitude [rad]
@tparam number lat latitude [rad]
@tparam number frame reference frame flag
@treturn vector surface velocity [m]
*/
int Interpreter::oapi_get_groundvector(lua_State *L)
{
	OBJHANDLE hRef;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(hRef = lua_toObject(L, 1), "Argument 1: invalid object");
	double lng = luaL_checknumber(L, 2);
	double lat = luaL_checknumber(L, 3);
	int frame = luaL_checkinteger(L, 4);
	VECTOR3 gv = oapiGetGroundVector(hRef, lng, lat, frame);
	lua_pushvector(L, gv);
	return 1;
}

/***
Returns the wind velocity at a given position in a planet's atmosphere.

The frame flag can be used to specify the reference frame to which the
returned vector refers. The following values are supported:

- 0: surface-relative (relative to local horizon)
- 1: planet-local (relative to local planet frame)
- 2: planet-local non-rotating (as 1, but adds the surface velocity, see \ref oapiGetGroundVector)
- 3: global (maps to global frame and adds planet velocity)

Warning: Local wind velocities are not currently implemented. The surface-relative
wind velocity is always (0,0,0). To ensure forward compatibility, plugins
should not rely on this limitation, but use this function instead.

@function get_windvector
@tparam handle hPlanet planet handle
@tparam number lng longitude [rad]
@tparam number lat latitude [rad]
@tparam number altitude above mean planet radius [m]
@tparam number frame reference frame flag
@treturn vector wind velocity vector relative to surface [m]
@treturn number wind speed magnitude in the local horizon frame, independent of the frame selected [m/s]
*/
int Interpreter::oapi_get_windvector(lua_State *L)
{
	OBJHANDLE hRef;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(hRef = lua_toObject(L, 1), "Argument 1: invalid object");
	double lng = luaL_checknumber(L, 2);
	double lat = luaL_checknumber(L, 3);
	double alt = luaL_checknumber(L, 4);
	int frame = luaL_checkinteger(L, 5);
	double windspeed;
	VECTOR3 gv = oapiGetWindVector(hRef, lng, lat, alt, frame, &windspeed);
	lua_pushvector(L, gv);
	lua_pushnumber(L, windspeed);
	return 1;
}

/***
Returns the number of perturbation coefficients defined for a planet.

Returns the number of perturbation coefficients defined for a planet to describe the
latitude-dependent perturbation of its gaviational potential. A return value of 0 indicates
that the planet is considered to have a spherically symmetric gravity field.

Note: even if a planet defines perturbation coefficients, its gravity perturbation may
be ignored, if the user disabled nonspherical gravity sources, or if orbit
stabilisation is active at a given time step. Use the
vessel.is_nonsphericalgravityenabled() function to check if a vessel uses the
perturbation terms in the update of its state vectors.

Note: depending on the distance to the planet, Orbiter may use fewer perturbation
terms than defined, if their contribution is negligible.

@function get_planetjcoeffcount
@tparam handle hPlanet planet handle
@treturn number Number of perturbation coefficients
*/
int Interpreter::oapi_get_planetjcoeffcount(lua_State *L)
{
	OBJHANDLE hRef;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(hRef = lua_toObject(L, 1), "Argument 1: invalid object");
	DWORD n = oapiGetPlanetJCoeffCount(hRef);
	lua_pushnumber(L, n);
	return 1;
}

/***
Returns a perturbation coefficient for the calculation of a planet's gravitational potential.

Note: Orbiter currently considers perturbations to be only a function of latitude
(polar), not of longitude.

Note: The first coefficient, n = 0, returns J2, which accounts for the ellipsoid shape
of a planet (flattening). Higher perturbation terms are usually small compared
to J2 (and not known for most planets).

@function get_planetjcoeff
@tparam handle hPlanet planet handle
@tparam number n coefficient index
@treturn number Perturbation coefficient
*/
int Interpreter::oapi_get_planetjcoeff(lua_State *L)
{
	OBJHANDLE hRef;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(hRef = lua_toObject(L, 1), "Argument 1: invalid object");
	DWORD n = luaL_checkinteger(L, 2);
	double coeff = oapiGetPlanetJCoeff(hRef, n);
	lua_pushnumber(L, coeff);
	return 1;
}


/***
Return an identifier of a vessel's propellant resource.

@function get_propellanthandle
@tparam handle hVessel vessel handle
@tparam number idx propellant resource index (>=0)
@treturn handle|nil propellant resource id, or NULL if idx >= # propellant resources
*/
int Interpreter::oapi_get_propellanthandle (lua_State *L)
{
	OBJHANDLE hObj;
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
	ASSERT_SYNTAX (lua_isnumber (L,2), "Argument 2: invalid type (expected number)");
	int idx = lua_tointeger (L,2);

	PROPELLANT_HANDLE hp = oapiGetPropellantHandle (hObj, idx);
	if (hp) lua_pushlightuserdata (L, hp);
	else    lua_pushnil (L);
	return 1;
}

/***
Return the current fuel mass [kg] of a propellant resource.

@function get_propellantmass
@tparam handle ph propellant resource identifier
@treturn number current fuel mass [kg] of the resource.
@see get_propellanthandle
*/
int Interpreter::oapi_get_propellantmass (lua_State *L)
{
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	PROPELLANT_HANDLE hp = (PROPELLANT_HANDLE)lua_touserdata (L, 1);
	ASSERT_SYNTAX(hp, "Argument 1: invalid object");
	lua_pushnumber (L, oapiGetPropellantMass (hp));
	return 1;
}

/***
Return the maximum capacity [kg] of a propellant resource.

@function get_propellantmaxmass
@tparam handle ph propellant resource identifier
@treturn number maximum fuel capacity [kg] of the resource.
@see get_propellanthandle
*/
int Interpreter::oapi_get_propellantmaxmass (lua_State *L)
{
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	PROPELLANT_HANDLE hp = (PROPELLANT_HANDLE)lua_touserdata (L, 1);
	ASSERT_SYNTAX(hp, "Argument 1: invalid object");
	lua_pushnumber (L, oapiGetPropellantMaxMass (hp));
	return 1;
}

/***
Return current fuel mass of the first propellant resource of a vessel.

This function is equivalent to
	oapi.get_propellantmass(oapi.get_propellanthandle(hVessel, 0))

hVessel must be a vessel handle. Other object types are invalid.

For multistage configurations, this returns the current fuel mass of active stages only.

@function get_fuelmass
@tparam handle hVessel vessel handle
@treturn number Current fuel mass [kg]
*/
int Interpreter::oapi_get_fuelmass (lua_State *L)
{
	OBJHANDLE hObj;
	double fmass;
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
	fmass = oapiGetFuelMass (hObj);
	lua_pushnumber (L, fmass);
	return 1;
}

/***
Return maximum fuel capacity of the first propellant resource of a vessel.

This function is equivalent to
	oapi.get_propellantmaxmass(oapi.get_propellanthandle(hVessel, 0))

hVessel must be a vessel handle. Other object types are invalid.

For multistage configurations, this returns the sum of the max fuel mass of active stages only.

@function get_maxfuelmass
@tparam handle hVessel vessel handle
@treturn number Maximum fuel mass [kg]
*/
int Interpreter::oapi_get_maxfuelmass (lua_State *L)
{
	OBJHANDLE hObj;
	double fmass;
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
	fmass = oapiGetMaxFuelMass (hObj);
	lua_pushnumber (L, fmass);
	return 1;
}

/***
Return empty mass of a vessel, excluding fuel.

hVessel must be a vessel handle. Other object types are invalid.

Do not rely on a constant empty mass. Structural changes (e.g. discarding a
rocket stage) will affect the empty mass.

For multistage configurations, the fuel mass of all currently inactive stages
contributes to the empty mass. Only the fuel mass of active stages is excluded.

@function get_emptymass
@tparam handle hVessel vessel handle
@treturn number empty vessel mass [kg]
*/
int Interpreter::oapi_get_emptymass (lua_State *L)
{
	OBJHANDLE hObj;
	double emass;
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
	emass = oapiGetEmptyMass (hObj);
	lua_pushnumber (L, emass);
	return 1;
}

/***
Set the empty mass of a vessel (excluding fuel).

Use this function to register structural mass changes, for example as a result
of jettisoning a fuel tank, etc.

@function set_emptymass
@tparam handle hVessel vessel handle
@tparam number mass empty mass [kg]
*/
int Interpreter::oapi_set_emptymass (lua_State *L)
{
	OBJHANDLE hObj;
	double emass;
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
	ASSERT_SYNTAX (lua_isnumber (L,2), "Argument 2: invalid type (expected number)");
	emass = lua_tonumber(L,2);
	ASSERT_SYNTAX (emass >= 0, "Argument 2: value >= 0 required");
	oapiSetEmptyMass (hObj, emass);
	return 0;
}

/***
Return the altitude of a vessel over a planetary surface.

If mode==ALTMODE.MEANRAD, the function returns the altitude above/below
the planet mean radius.

If mode==ALTMODE.GROUND, the altitude above the local ground elevation is returned.

The handle passed to the function must refer to a vessel.

@function get_altitude
@tparam[opt=current focus vessel] handle hVessel vessel handle
@tparam[opt=ALTMODE.MEANRAD] number alt altitude mode
@treturn number|nil altitude above closest planet [m] or nil in case of error
*/
int Interpreter::oapi_get_altitude (lua_State *L)
{
	OBJHANDLE hObj = oapiGetFocusObject ();
	AltitudeMode mode = ALTMODE_MEANRAD;
	int modeidx = 1;
	double alt;
	if (lua_gettop(L) >= 1) {
		if (lua_islightuserdata (L,1)) {
			ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
			modeidx++;
		}
	}
	if (lua_gettop(L) >= modeidx) {
		if (lua_isnumber(L,modeidx))
			mode = (AltitudeMode)(int)lua_tonumber(L,modeidx);
	}
	if (oapiGetAltitude (hObj, mode, &alt))
		lua_pushnumber (L, alt);
	else
		lua_pushnil (L);
	return 1;
}

/***
Return a vessel's pitch angle w.r.t. the local horizon.

The local horizon is the plane whose normal is defined by the distance
vector from the planet centre to the vessel.

The handle passed to the function must refer to a vessel.

@function get_pitch
@tparam[opt=current focus vessel] handle hVessel vessel handle
@treturn number|nil pitch angle w.r.t. closest planet [rad] or nil in case of error
*/
int Interpreter::oapi_get_pitch (lua_State *L)
{
	OBJHANDLE hObj;
	double pitch;
	if (lua_gettop(L) < 1) {
		hObj = oapiGetFocusObject ();
	} else {
		ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
		ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
	}
	if (oapiGetPitch (hObj, &pitch))
		lua_pushnumber (L, pitch);
	else
		lua_pushnil (L);
	return 1;
}

/***
Return a vessel's bank angle w.r.t. the local horizon.

The local horizon is the plane whose normal is defined by the distance
vector from the planet centre to the vessel.

The handle passed to the function must refer to a vessel.

@function get_bank
@tparam[opt=current focus vessel] handle hVessel vessel handle
@treturn number|nil bank angle w.r.t. closest planet [rad] or nil in case of error
*/
int Interpreter::oapi_get_bank (lua_State *L)
{
	OBJHANDLE hObj;
	double bank;
	if (lua_gettop(L) < 1) {
		hObj = oapiGetFocusObject ();
	} else {
		ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
		ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
	}
	if (oapiGetBank (hObj, &bank))
		lua_pushnumber (L, bank);
	else
		lua_pushnil (L);
	return 1;
}

/***
Return a vessel's heading (against geometric north) calculated for the local horizon plane.

@function get_heading
@tparam[opt=current focus vessel] handle hVessel vessel handle
@treturn number|nil heading value [rad] 0=north, PI/2=east, etc or nil in case of error
*/
int Interpreter::oapi_get_heading (lua_State *L)
{
	OBJHANDLE hObj;
	double heading;
	if (lua_gettop(L) < 1) {
		hObj = oapiGetFocusObject ();
	} else {
		ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
		ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
	}
	if (oapiGetHeading (hObj, &heading))
		lua_pushnumber (L, heading);
	else
		lua_pushnil (L);
	return 1;
}

/***
Return a vessel's ground speed w.r.t. the closest planet or moon.

@function get_groundspeed
@tparam[opt=current focus vessel] handle hVessel vessel handle
@treturn number|nil groundspeed ground speed value [m/s]
*/
int Interpreter::oapi_get_groundspeed (lua_State *L)
{
	OBJHANDLE hObj;
	double speed;
	if (lua_gettop(L) < 1) {
		hObj = oapiGetFocusObject ();
	} else {
		ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
		ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
	}
	if (oapiGetGroundspeed (hObj, &speed))
		lua_pushnumber (L, speed);
	else
		lua_pushnil (L);
	return 1;
}

/***
Return a vessel's groundspeed vector w.r.t. the closest planet or moon in the
 requested frame of reference.
 
The ground speed vector is defined as the vessel's
velocity vector with respect to a point at the vessel position fixed
in the planet's rotating frame of reference.

Valid entries for frame are :

- REFFRAME.GLOBAL: Return velocity vector in the global frame of reference
- REFFRAME.LOCAL: Return velocity vector in the vessel's local frame of
  reference
- REFFRAME.REFLOCAL: Return velocity vector in the celestial reference
  body's local frame of reference
- REFFRAME.HORIZON: Return velocity vector in the local horizon frame
  (x = longitudinal component, y = vertical component, z = latitudinal
  component)

@function get_groundspeedvector
@tparam[opt=current focus vessel] handle hVessel vessel handle
@tparam number frame frame of reference flag
@treturn vector|nil groundspeed ground speed vector [m/s in x,y,z]
*/
int Interpreter::oapi_get_groundspeedvector (lua_State *L)
{
	OBJHANDLE hObj;
	VECTOR3 speedv;
	int idx = 2;
	if (lua_gettop(L) < 2) {
		hObj = oapiGetFocusObject ();
		idx = 1;
	} else {
		ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
		ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
	}
	ASSERT_NUMBER(L,idx);
	REFFRAME frame = (REFFRAME)lua_tointeger (L, idx);
	if (oapiGetGroundspeedVector (hObj, frame, &speedv))
		lua_pushvector (L, speedv);
	else
		lua_pushnil (L);
	return 1;
}

/***
Return a vessel's true airspeed w.r.t. the closest planet or moon.

This function works even for planets or moons without atmosphere. In that case
it returns the ground speed.

@function get_airspeed
@tparam[opt=current focus vessel] handle hVessel vessel handle
@treturn number|nil airspeed airspeed value [m/s]
*/
int Interpreter::oapi_get_airspeed (lua_State *L)
{
	OBJHANDLE hObj;
	double speed;
	if (lua_gettop(L) < 1) {
		hObj = oapiGetFocusObject ();
	} else {
		ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
		ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
	}
	if (oapiGetAirspeed (hObj, &speed))
		lua_pushnumber (L, speed);
	else
		lua_pushnil (L);
	return 1;
}

/***
Return a vessel's true airspeed vector w.r.t. the closest planet or moon in the
requested frame of reference.

This method returns the true airspeed vector in the requested frame
of reference. The airspeed vector is defined as the vessel's
velocity vector with respect to the surrounding freestream air flow.

If the vessel is not within an a planetary atmosphere, the returned
vector is equal to the groundspeed vector.

Valid entries for frame are :

- REFFRAME.GLOBAL: Return velocity vector in the global frame of reference
- REFFRAME.LOCAL: Return velocity vector in the vessel's local frame of
  reference
- REFFRAME.REFLOCAL: Return velocity vector in the celestial reference
  body's local frame of reference
- REFFRAME.HORIZON: Return velocity vector in the local horizon frame
  (x = longitudinal component, y = vertical component, z = latitudinal
  component)

@function get_airspeedvector
@tparam[opt=current focus vessel] handle hVessel vessel handle
@tparam number frame frame of reference flag
@treturn vector|nil airspeed airspeed vector [m/s in x,y,z]
*/
int Interpreter::oapi_get_airspeedvector (lua_State *L)
{
	OBJHANDLE hObj;
	VECTOR3 speedv;
	int idx = 2;
	if (lua_gettop(L) < 2) {
		hObj = oapiGetFocusObject ();
		idx = 1;
	} else {
		ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
		ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
	}
	ASSERT_NUMBER(L,idx);
	REFFRAME frame = (REFFRAME)lua_tointeger (L, idx);
	if (oapiGetAirspeedVector (hObj, frame, &speedv))
		lua_pushvector (L, speedv);
	else
		lua_pushnil (L);
	return 1;
}

/***
Return velocity vector in the vessel's local frame of reference

This function is deprecated, use oapi.get_airspeedvector instead

@function get_shipairspeedvector
@tparam[opt=current focus vessel] handle hVessel vessel handle
@treturn vector|nil airspeed airspeed vector [m/s in x,y,z]
*/
int Interpreter::oapi_get_shipairspeedvector (lua_State *L)
{
	GetInterpreter(L)->term_strout (L, "Obsolete function used: oapi.get_shipairspeedvector.\nUse oapi.get_airspeedvector instead", true);
	OBJHANDLE hObj;
	VECTOR3 speedv;
	if (lua_gettop(L) < 1) {
		hObj = oapiGetFocusObject ();
	} else {
		ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
		ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
	}
	if (oapiGetAirspeedVector(hObj, FRAME_LOCAL, &speedv))
		lua_pushvector (L, speedv);
	else
		lua_pushnil (L);
	return 1;
}

/***
Get reference used to override a particle stream intensity (opacity).

The reference should be set to values between 0 (lowest intensity) and 1 (highest intensity).

By default, exhaust streams are linked to the thrust level setting of the
thruster they are associated with. Reentry streams are set to a fixed level of 1 by default.

This function allows to customise the appearance of the particle streams directly by the module.

Other parameters besides the intensity level, such as atmospheric density
can also have an effect on the particle intensity.

@function particle_getlevelref
@tparam handle ph particle stream handle
@treturn numberref reference used to define the particles intensity
@usage part_intensity = oapi.particle_getlevelref(ph)
 -- elsewhere
 part_intensity:set(0.5)
@see numberref
*/
int Interpreter::oapi_particle_getlevelref(lua_State* L)
{
	static const char* funcname = "particle_getlevelref";
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	PSTREAM_HANDLE ph = (PSTREAM_HANDLE)lua_touserdata (L, 1);
	ASSERT_SYNTAX(ph, "Argument 1: invalid object");
	lua_pushnumberref(L);
	double *lvl = (double *)lua_touserdata(L, -1);

	oapiParticleSetLevelRef(ph, lvl);
	return 1;
}

/***
Return a vessel's spherical equatorial coordinates (longitude, latitude and radius) with
respect to the closest planet or moon.

The handle passed to the function must refer to a vessel.

@function get_equpos
@tparam[opt=current focus vessel] handle hVessel vessel handle
@treturn table|nil
The returned table contains the equatorial coordinates and has the following fields:

- lng: number (longitude [rad])
- lat: number (latitude [rad])
- rad: number (radial distance [m])
*/
int Interpreter::oapi_get_equpos (lua_State *L)
{
	OBJHANDLE hObj;
	double lng, lat, rad;
	if (lua_gettop(L) < 1) {
		hObj = oapiGetFocusObject ();
	} else {
		ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
		ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
	}
	if (oapiGetEquPos (hObj, &lng, &lat, &rad)) {
		lua_createtable (L, 0, 3);
		lua_pushnumber (L, lng);
		lua_setfield (L, -2, "lng");
		lua_pushnumber (L, lat);
		lua_setfield (L, -2, "lat");
		lua_pushnumber (L, rad);
		lua_setfield (L, -2, "rad");
	} else {
		lua_pushnil (L);
	}
	return 1;
}

/***
Return the atmospheric parameters at the current vessel position.

If the vessel is not within range of any planet atmosphere model, all
fields of returned table are set to 0.

Currently, atmospheric values only depend on altitude, and don't take
into account local weather variations.

@function get_atm
@tparam[opt=current focus vessel] handle hVessel vessel handle
@treturn table atmospheric parameters
The returned table contains the equatorial coordinates and has the following fields:

- p: number (pressure [Pa])
- rho: number (density [kg/m^3])
- T: number (temperature [K])
- ref: handle|nil (handle of the celestial body contributing the atmospheric parameters.)
*/
int Interpreter::oapi_get_atm (lua_State *L)
{
	OBJHANDLE hObj;
	ATMPARAM prm;
	if (lua_gettop(L) < 1) {
		hObj = 0;
	} else {
		ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
		ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
	}
	OBJHANDLE hAtmRef;
	oapiGetAtm(hObj, &prm, &hAtmRef);
	lua_createtable (L, 0, 3);
	lua_pushnumber (L, prm.p);
	lua_setfield (L, -2, "p");
	lua_pushnumber (L, prm.rho);
	lua_setfield (L, -2, "rho");
	lua_pushnumber (L, prm.T);
	lua_setfield (L, -2, "T");
	if(hAtmRef)
		lua_pushlightuserdata(L, hAtmRef);
	else
		lua_pushnil(L);
	lua_setfield (L, -2, "ref");
	return 1;
}

/***
Aerodynamics induced drag helper function.

This is a helper function which is useful when implementing the callback function
calculating the aerodynamics coefficients for an airfoil.
It computes the lift-induced component of the drag coefficient as a function of lift
coefficient, wing aspect ratio and wing efficiency factor.

@function get_induceddrag
@tparam number cl lift coefficient
@tparam number A wing aspect ratio
@tparam number e wing efficiency factor
@treturn number Induced drag coefficient
@see vessel:create_airfoil
*/
int Interpreter::oapi_get_induceddrag (lua_State *L)
{
	ASSERT_SYNTAX(lua_isnumber(L,1), "Argument 1: invalid type (expected number)");
	double cl = lua_tonumber(L,1);
	ASSERT_SYNTAX(lua_isnumber(L,2), "Argument 2: invalid type (expected number)");
	double A = lua_tonumber(L,2);
	ASSERT_SYNTAX(lua_isnumber(L,3), "Argument 3: invalid type (expected number)");
	double e = lua_tonumber(L,3);
	lua_pushnumber(L,oapiGetInducedDrag(cl,A,e));
	return 1;
}

/***
Aerodynamics wave drag helper function.

This is a helper function which is useful when implementing the callback function
calculating the aerodynamics coefficients for an airfoil (see vessel:create_airfoil). It
uses a simple model to compute the wave drag component of the drag coefficient.

Wave drag significantly affects the vessel drag
around Mach 1, and falls off towards lower and higher airspeeds.

@function get_wavedrag
@tparam number M current Mach number
@tparam number M1 characteristic Mach number
@tparam number M2 characteristic Mach number
@tparam number M3 characteristic Mach number
@tparam number cmax maximum wave drag coefficient
@treturn number Wave drag coefficient
@see vessel:create_airfoil
*/
int Interpreter::oapi_get_wavedrag (lua_State *L)
{
	ASSERT_SYNTAX(lua_isnumber(L,1), "Argument 1: invalid type (expected number)");
	double M = lua_tonumber(L,1);
	ASSERT_SYNTAX(lua_isnumber(L,2), "Argument 2: invalid type (expected number)");
	double M1 = lua_tonumber(L,2);
	ASSERT_SYNTAX(lua_isnumber(L,3), "Argument 3: invalid type (expected number)");
	double M2 = lua_tonumber(L,3);
	ASSERT_SYNTAX(lua_isnumber(L,4), "Argument 4: invalid type (expected number)");
	double M3 = lua_tonumber(L,4);
	ASSERT_SYNTAX(lua_isnumber(L,5), "Argument 5: invalid type (expected number)");
	double cmax = lua_tonumber(L,5);
	lua_pushnumber(L,oapiGetWaveDrag(M,M1,M2,M3,cmax));
	return 1;
}

/***
Return a handle to a vessel docking port.

@function get_dockhandle
@tparam[opt=current focus vessel] handle hVessel vessel handle
@tparam number n docking port index (>= 0)
@treturn handle|nil docking port handle, or nil if index is out of range
*/
int Interpreter::oapi_get_dockhandle(lua_State* L)
{
	OBJHANDLE hObj;
	double lng, lat, rad;
	if (lua_gettop(L) < 1) {
		hObj = oapiGetFocusObject();
	}
	else {
		ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
		ASSERT_SYNTAX(hObj = lua_toObject(L, 1), "Argument 1: invalid object");
	}
	ASSERT_SYNTAX(lua_isnumber(L, 2), "Argument 2: invalid type (expected number)");
	double n = lua_tonumber(L, 2);

	DOCKHANDLE hDock = oapiGetDockHandle(hObj, n);
	if(hDock)
		lua_pushlightuserdata(L, hDock);
	else
		lua_pushnil(L);
	return 1;
}

/***
Return the handle of a vessel docked at a port.

@function get_dockstatus
@tparam handle hDock docking port handle
@treturn handle|nil handle of docked vessel, or nil if no vessel is docked at the port.
*/
int Interpreter::oapi_get_dockstatus(lua_State* L)
{
	DOCKHANDLE hDock = (DOCKHANDLE)lua_tolightuserdata_safe(L, 1, "get_dockstatus");
	OBJHANDLE hDockedVessel = oapiGetDockStatus(hDock);
	if (hDockedVessel) {
		lua_pushlightuserdata(L, hDockedVessel);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/***
Set a docking port to auto capture when in close proximity with some other docking port.

Auto capture is enabled by default.

@function set_autocapture
@tparam handle hDock docking port handle
@tparam boolean enable Enable or disable auto capture
*/
int Interpreter::oapi_set_autocapture(lua_State* L)
{
	DOCKHANDLE hDock = (DOCKHANDLE)lua_tolightuserdata_safe(L, 1, "set_autocapture");
	if(!lua_isboolean(L, 2)) {
		return luaL_error(L, "Argument 2: set_autocapture expects a boolean");
	}
	bool enable = lua_toboolean(L, 2);
	oapiSetAutoCapture(hDock, enable);
	return 0;
}

/***
Get the vessel a docking port belongs to.

@function get_dockowner
@tparam handle hDock docking port handle
@treturn handle Vessel owning the docking port.
*/
int Interpreter::oapi_get_dockowner(lua_State* L)
{
	DOCKHANDLE hDock = (DOCKHANDLE)lua_tolightuserdata_safe(L, 1, "get_dockowner");
	OBJHANDLE hOwner = oapiGetDockOwner(hDock);
	if (hOwner) {
		lua_pushlightuserdata(L, hOwner);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/***
Navigation radio transmitters.
@section navradio
*/

/***
Return the current position of a NAV transmitter 
(in global coordinates, i.e. heliocentric ecliptic).

@function get_navpos
@tparam handle hNav NAV transmitter handle
@treturn vector global position
*/
int Interpreter::oapi_get_navpos (lua_State *L)
{
	NAVHANDLE hNav;
	VECTOR3 pos;
	ASSERT_SYNTAX (lua_gettop(L) >= 1, "Too few arguments");
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX (hNav = (NAVHANDLE)lua_touserdata (L,1), "Argument 1: invalid object");
	oapiGetNavPos (hNav, &pos);
	lua_pushvector (L, pos);
	return 1;
}

/***
Return the channel number of a NAV transmitter.

Channel numbers range from 0 to 639.

To convert a channel number ch into a frequency, use
	f = (108.0 + 0.05 ch) MHz

@function get_navchannel
@tparam handle hNav NAV transmitter handle
@treturn number channel number
*/
int Interpreter::oapi_get_navchannel (lua_State *L)
{
	NAVHANDLE hNav;
	ASSERT_SYNTAX (lua_gettop(L) >= 1, "Too few arguments");
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX (hNav = (NAVHANDLE)lua_touserdata (L,1), "Argument 1: invalid object");
	DWORD ch = oapiGetNavChannel (hNav);
	lua_pushnumber (L, ch);
	return 1;
}

/***
Return the range of a NAV transmitter.

A NAV receiver will only receive a signal when within the range of a transmitter.

Variable receiver sensitivity is not currently implemented.

Shadowing of a transmitter by obstacles between transmitter and receiver is
not currently implemented.

Because the range of the transmitter depends on receiver gain as well
as transmitter power, the range is not strictly a property of the
transmitter. It is preferred to calculate the range for a given
receiver gain by using the oapi.get_navdata or oapi.get_navsignal functions.

@function get_navrange
@tparam handle hNav NAV transmitter handle
@treturn number Transmitter range [m]
*/
int Interpreter::oapi_get_navrange (lua_State *L)
{
	NAVHANDLE hNav;
	ASSERT_SYNTAX (lua_gettop(L) >= 1, "Too few arguments");
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX (hNav = (NAVHANDLE)lua_touserdata (L,1), "Argument 1: invalid object");
	float range = oapiGetNavRange (hNav);
	lua_pushnumber (L, range);
	return 1;
}

/***
Return information about a NAV transmitter.

Available NAV types :

- TRANSMITTER.VOR
- TRANSMITTER.VTOL
- TRANSMITTER.ILS
- TRANSMITTER.IDS
- TRANSMITTER.XPDR

@function get_navdata
@tparam handle hNav NAV transmitter handle
@treturn table NAV information table with the following fields :

- type: number (NAV type)
- ch: number (transmitter channel [0-639])
- power: number (transmitter power [arbitrary units])
- descr: string (transmitter description)

Additionnal fields by NAV type :

- VOR :

    - hplanet: handle
    - lng: number
    - lat: number
- VTOL :

    - hbase: handle
    - npad: number
- ILS :

    - hbase: handle
	- appdir: number
- IDS :

    - hvessel: handle
	- hdock: handle
- XPDR :

    - hvessel: handle
*/
int Interpreter::oapi_get_navdata (lua_State *L)
{
	NAVHANDLE hNav;
	ASSERT_SYNTAX (lua_gettop(L) >= 1, "Too few arguments");
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX (hNav = (NAVHANDLE)lua_touserdata (L,1), "Argument 1: invalid object");
	NAVDATA ndata;
	oapiGetNavData (hNav, &ndata);
	lua_newtable (L);
	lua_pushnumber (L, ndata.type);
	lua_setfield (L, -2, "type");
	lua_pushnumber (L, ndata.ch);
	lua_setfield (L, -2, "ch");
	lua_pushnumber (L, ndata.power);
	lua_setfield (L, -2, "power");
	char descr[256];
	oapiGetNavDescr(hNav,descr,256);
	lua_pushstring (L, descr);
	lua_setfield (L, -2, "descr");
	switch (ndata.type) {
	case TRANSMITTER_VOR:
		lua_pushlightuserdata (L, ndata.vor.hPlanet);
		lua_setfield (L, -2, "hplanet");
		lua_pushnumber (L, ndata.vor.lng);
		lua_setfield (L, -2, "lng");
		lua_pushnumber (L, ndata.vor.lat);
		lua_setfield (L, -2, "lat");
		break;
	case TRANSMITTER_VTOL:
		lua_pushlightuserdata (L, ndata.vtol.hBase);
		lua_setfield (L, -2, "hbase");
		lua_pushnumber (L, ndata.vtol.npad);
		lua_setfield (L, -2, "npad");
		break;
	case TRANSMITTER_ILS:
		lua_pushlightuserdata (L, ndata.ils.hBase);
		lua_setfield (L, -2, "hbase");
		lua_pushnumber (L, ndata.ils.appdir);
		lua_setfield (L, -2, "appdir");
		break;
	case TRANSMITTER_IDS:
		lua_pushlightuserdata (L, ndata.ids.hVessel);
		lua_setfield (L, -2, "hvessel");
		lua_pushlightuserdata (L, ndata.ids.hDock);
		lua_setfield (L, -2, "hdock");
		break;
	case TRANSMITTER_XPDR:
		lua_pushlightuserdata (L, ndata.xpdr.hVessel);
		lua_setfield (L, -2, "hvessel");
		break;
	}
	return 1;
}

/***
Return the signal strength of a transmitter at a given position.

The transmitter signal strength drops off with the square of
distance to the transmitter. The units are chosen so that a 'default'
receiver will be able to detect signals above a strength of 1.

@function get_navsignal
@tparam handle hNav NAV transmitter handle
@tparam vector global position
@treturn number signal strength in arbitrary units
*/
int Interpreter::oapi_get_navsignal (lua_State *L)
{
	NAVHANDLE hNav;
	ASSERT_SYNTAX (lua_gettop(L) >= 1, "Too few arguments");
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX (hNav = (NAVHANDLE)lua_touserdata (L,1), "Argument 1: invalid object");
	ASSERT_SYNTAX (lua_isvector (L, 2), "Argument 2: invalid type (expected vector)");
	VECTOR3 gpos = lua_tovector(L,2);
	double sig = oapiGetNavSignal (hNav, gpos);
	lua_pushnumber (L, sig);
	return 1;	
}

/***
Return the type id of a NAV transmitter.

The following transmitter types are currently supported:

- TRANSMITTER.VOR (omnidirectional beacon)
- TRANSMITTER.VTOL (launchpad homing beacon)
- TRANSMITTER.ILS (instrument landing system)
- TRANSMITTER.IDS (instrument docking system)
- TRANSMITTER.XPDR (transponder)

@function get_navtype
@tparam handle hNav NAV transmitter handle
@treturn number transmitter type identifier
*/
int Interpreter::oapi_get_navtype (lua_State *L)
{
	NAVHANDLE hNav;
	ASSERT_SYNTAX (lua_gettop(L) >= 1, "Too few arguments");
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX (hNav = (NAVHANDLE)lua_touserdata (L,1), "Argument 1: invalid object");
	DWORD ntype = oapiGetNavType (hNav);
	lua_pushnumber (L, ntype);
	return 1;
}

/***
Camera.
@section camera
*/

/***
Return a handle to the current camera target.

The camera target is not necessarily a vessel, and if it is a vessel, it is not
necessarily the focus object (the vessel receiving user input).

@function get_cameratarget
@treturn handle|nil handle to the current camera target (i.e. the object the camera is pointing at in
external mode, or the handle of the vessel in cockpit mode)
*/
int Interpreter::oapi_get_cameratarget (lua_State *L)
{
	OBJHANDLE hObj = oapiCameraTarget();
	if (hObj)
		lua_pushlightuserdata (L, hObj);
	else
		lua_pushnil (L);
	return 1;
}

/***
Attach the camera to a new target, or switch between internal and external camera mode.

If the new target is not a vessel, the camera mode is always set to external,
regardless of the value of mode.

This is equivalent to the oapiCameraAttach API in C++

@function set_cameratarget
@tparam handle handle of the new camera target
@tparam[opt=2] number mode camera mode (0=internal, 1=external, 2=don't change)
*/
int Interpreter::oapi_set_cameratarget (lua_State *L)
{
	OBJHANDLE hObj;
	int mode = 2;
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX (hObj = (OBJHANDLE)lua_touserdata (L,1), "Argument 1: invalid object");
	if (lua_gettop(L) > 1) {
		ASSERT_SYNTAX (lua_isnumber (L,2), "Argument 2: invalid type (expected number)");
		mode = (int)lua_tonumber (L,2);
		ASSERT_SYNTAX (mode >= 0 && mode <= 2, "Argument 2: out of range");
	}
	oapiCameraAttach (hObj, mode);
	return 0;
}

/***
Return the current camera aperture (the field of view).

Orbiter defines the aperture as 1/2 of the vertical field of view, between
the viewport centre and the top edge of the viewport.

@function get_cameraaperture
@treturn number camera aperture [rad]
*/
int Interpreter::oapi_get_cameraaperture (lua_State *L)
{
	double ap = oapiCameraAperture();
	lua_pushnumber (L, ap);
	return 1;
}

/***
Change the camera aperture (field of view).

Orbiter restricts the aperture to the range from RAD*0.1 to RAD*80 (i. e. field
of view between 0.2 and 160 deg. Very wide angles (> 90 deg) and very narrow angles
(< 5 deg) should only be used to implement specific optical devices, e.g. telescopes
or wide-angle cameras, not for standard observer views.

The Orbiter user interface does not accept apertures > 45 deg or < 5 deg. As
soon as the user manipulates the aperture manually, it will be clamped back to the
range from 5 to 45 deg.

@function set_cameraaperture
@tparam number aperture new aperture [rad]
*/
int Interpreter::oapi_set_cameraaperture (lua_State *L)
{
	ASSERT_SYNTAX (lua_isnumber (L,1), "Argument 1: invalid type (expected number)");
	double ap = lua_tonumber (L,1);
	oapiCameraSetAperture (ap);
	return 0;
}

/***
Return current camera position in global coordinates.

The global coordinate system is the heliocentric ecliptic frame at epoch J2000.0.

@function get_cameraglobalpos
@treturn vector global camera coordinates
*/
int Interpreter::oapi_get_cameraglobalpos (lua_State *L)
{
	VECTOR3 pos;
	oapiCameraGlobalPos (&pos);
	lua_pushvector (L, pos);
	return 1;
}

/***
Returns current camera direction in global coordinates.

@function get_cameraglobaldir
@treturn vector global camera direction
*/
int Interpreter::oapi_get_cameraglobaldir (lua_State *L)
{
	VECTOR3 dir;
	oapiCameraGlobalDir (&dir);
	lua_pushvector (L, dir);
	return 1;
}

/***
Set the camera mode.

The camera mode is a table with the following fields :

- mode: string ("ground"|"track"|"cockpit")

Additional fields by mode :

- ground:

    - ref: string
	- lng: number
	- lat: number
	- alt: number
	- alt\_above\_ground: number
	- phi: number
	- tht: number
- track:

    - trackmode: string
	- reldist: number
	- phi: number
	- tht: number
	- ref: string
- cockpit:

    - cockpitmode: string
	- pos: number
	- lean: number
	- lean_smooth: number

@function set_cameramode
@tparam table camera mode
*/
int Interpreter::oapi_set_cameramode (lua_State *L)
{
	char initstr[1024], modestr[256];
	double lng, lat, alt, phi=0.0, tht=0.0;
	CameraMode *cm = 0;
	ASSERT_TABLE(L,1);

	lua_getfield(L,1,"mode");
	ASSERT_STRING(L,-1);
	strcpy(modestr, lua_tostring(L,-1));
	lua_pop(L,1);
	if (!_stricmp(modestr, "ground")) {

		lua_getfield(L,1,"ref");
		ASSERT_STRING(L,-1);
		strcpy (initstr,lua_tostring(L,-1));
		lua_pop(L,1);
		lua_getfield(L,1,"lng");
		ASSERT_NUMBER(L,-1);
		lng = lua_tonumber(L,-1);
		lua_pop(L,1);
		lua_getfield(L,1,"lat");
		ASSERT_NUMBER(L,-1);
		lat = lua_tonumber(L,-1);
		lua_pop(L,1);
		lua_getfield(L,1,"alt");
		ASSERT_NUMBER(L,-1);
		alt = lua_tonumber(L,-1);
		lua_pop(L,1);
		sprintf (initstr + strlen(initstr), " %lf %lf %lf", lng, lat, alt);
		lua_getfield(L,1,"alt_above_ground");
		if (lua_isnumber(L,-1) && lua_tonumber(L,-1) == 0)
			strcat(initstr, "M");
		lua_pop(L,1);
		lua_getfield(L,1,"phi");
		if (lua_isnumber(L,-1)) {
			phi = lua_tonumber(L,-1);
			lua_getfield(L,1,"tht");
			if (lua_isnumber(L,-1)) {
				tht = lua_tonumber(L,-1);
				sprintf (initstr+strlen(initstr), " %lf %lf", phi, tht);
			}
			lua_pop(L,1);
		}
		lua_pop(L,1);
		cm = new CameraMode_Ground();

	} else if (!_stricmp(modestr, "track")) {

		lua_getfield(L,1,"trackmode");
		ASSERT_STRING(L,-1);
		strcpy (initstr, lua_tostring(L,-1));
		lua_pop(L,1);
		lua_getfield(L,1,"reldist");
		ASSERT_NUMBER(L,-1);
		double reldist = lua_tonumber(L,-1);
		lua_pop(L,1);
		lua_getfield(L,1,"phi");
		if (lua_isnumber(L,-1))
			phi = lua_tonumber(L,-1);
		lua_pop(L,1);
		lua_getfield(L,1,"tht");
		if (lua_isnumber(L,-1))
			tht = lua_tonumber(L,-1);
		lua_pop(L,1);
		sprintf (initstr+strlen(initstr), " %lf %lf %lf", reldist, phi, tht);
		lua_getfield(L,1,"ref");
		if (lua_isstring(L,-1)) {
			strcat(initstr, " ");
			strcat(initstr, lua_tostring(L,-1));
		}
		lua_pop(L,1);
		cm = new CameraMode_Track();

	} else if (!_stricmp(modestr, "cockpit")) {

		lua_getfield(L,1,"cockpitmode");
		if (lua_isstring(L,-1)) {
			strcpy (initstr, lua_tostring(L,-1));
			lua_getfield(L,1,"pos");
			if (lua_isnumber(L,-1)) {
				sprintf (initstr+strlen(initstr), ":%d", (int)lua_tonumber(L,-1));
				lua_getfield(L,1,"lean");
				if (lua_isnumber(L,-1)) {
					sprintf (initstr+strlen(initstr), ":%d", (int)lua_tonumber(L,-1));
				} else {
					lua_getfield(L,1,"lean_smooth");
					if (lua_isnumber(L,-1)) {
						sprintf (initstr+strlen(initstr), ":%dS", (int)lua_tonumber(L,-1));
					}
					lua_pop(L,1);
				}
				lua_pop(L,1);
			}
			lua_pop(L,1);

		} else
			initstr[0] = '\0';
		lua_pop(L,1);
		cm = new CameraMode_Cockpit();

	}

	if (cm) {
		cm->Init(initstr);
		oapiSetCameraMode (*cm);
		delete cm;
	}
	return 0;
}

/***
Move the ground observer camera.

The camera mode is a table with the following fields :

- f: number (forward distance in camera-forward direction (< 0 for backward) [m])
- r: number (right distance in camera-right direction (< 0 for left) [m])
- u: number (up distance in planet-up direction (< 0 for down) [m])

@function move_groundcamera
@tparam table camera mode
*/
int Interpreter::oapi_move_groundcamera (lua_State *L)
{
	double forward=0.0, right=0.0, up=0.0;
	ASSERT_TABLE(L,1);
	lua_getfield(L,1,"f");
	if (lua_isnumber(L,-1))
		forward = lua_tonumber(L,-1);
	lua_pop(L,1);
	lua_getfield(L,1,"r");
	if (lua_isnumber(L,-1))
		right = lua_tonumber(L,-1);
	lua_pop(L,1);
	lua_getfield(L,1,"u");
	if (lua_isnumber(L,-1))
		up = lua_tonumber(L,-1);
	lua_pop(L,1);
	oapiMoveGroundCamera (forward, right, up);
	return 0;
}

/***
Set the camera direction in cockpit mode.

This function is ignored if the camera is not currently in cockpit mode.

The polar and azimuth angles are relative to the default view direction

The requested direction should be within the current rotation ranges

If transition==false, the new direction is set instantaneously; otherwise the
camera swings from the current to the new direction (not yet implemented).

@function set_cameracockpitdir
@tparam number polar polar angle [rad]
@tparam number azimuth azimuth angle [rad]
@tparam[opt=false] boolean transition transition flag
*/
int Interpreter::oapi_set_cameracockpitdir(lua_State *L)
{
	double polar = luaL_checknumber(L, 1);
	double azimuth = luaL_checknumber(L, 2);
	bool transition = false;
	if(lua_gettop(L)>=3) {
		transition = lua_toboolean(L, 3);
	}
	oapiCameraSetCockpitDir(polar, azimuth, transition);
	return 0;
}

/***
Create/Update custom camera.

Create a new custom camera that can be used to render views into a surfaces and textures.

Note: Camera count is unlimited.

Note: Only cameras attached to currently active vessel are operational and recording.

Note: Having multiple cameras active at the same time doesn't impact in a frame rate, however, camera refresh rates are reduced.

@function setup_customcamera
@tparam handle hCam camera handle to modify an existing camera or, nil to create a new one
@tparam handle hVessel handle to a vessel where the camera is attached to.
@tparam vector vPos camera position in vessel's local coordinate system
@tparam vector vDir camera direction in vessel's local coordinate system. [Unit Vector]
@tparam vector vUp camera up vector. Must be perpendicular to vDir. [Unit Vector]
@tparam number dFow camera field of view in radians
@tparam handle hSurf rendering surface. Must be created at least with OAPISURFACE.RENDER3D and OAPISURFACE.RENDERTARGET. Multiple cameras can share the same surface.
@tparam number flags Flags to controls what is drawn and what is not.
@treturn handle Camera handle, or nil if an error occurred or if the custom camera interface is disabled.
*/

typedef struct {
	lua_State *L;
	int clbk;
	CAMERAHANDLE hCam;
} CustomCamera_Lua;


int Interpreter::customcamera_collect(lua_State *L)
{
	CustomCamera_Lua *cc = (CustomCamera_Lua *)luaL_checkudata(L, 1, "CustomCamera.vtable");
	luaL_unref(L, LUA_REGISTRYINDEX, cc->clbk);
	LazyInitGCCore();
	if(cc->hCam && pCore) { // in case the script did not delete the camera
		pCore->DeleteCustomCamera(cc->hCam);
	}
	return 0;
}

static void lua_pushcustomcamera(lua_State *L, CAMERAHANDLE hCam)
{
	CustomCamera_Lua *cc = (CustomCamera_Lua *)lua_newuserdata(L, sizeof(CustomCamera_Lua));
	cc->L = L;
	cc->hCam = hCam;
	cc->clbk = LUA_REFNIL;

	luaL_getmetatable(L, "CustomCamera.vtable");
	lua_setmetatable(L, -2);
}

int Interpreter::oapi_setup_customcamera(lua_State *L)
{
	LazyInitGCCore();
	if(pCore) {
		CAMERAHANDLE hCam = nullptr;
		CustomCamera_Lua *cc = nullptr;
		if(!lua_isnil(L,1)) {
			cc = (CustomCamera_Lua *)luaL_checkudata(L, 1, "CustomCamera.vtable");
			hCam = cc->hCam;
		}
		OBJHANDLE hVessel = nullptr;
		void *ud = lua_touserdata(L,2);
		if(oapiIsVessel(ud)) {
			hVessel = (OBJHANDLE)ud;
		} else if(VESSEL *v = lua_tovessel(L, 2)) {
			hVessel = v->GetHandle();
		} else {
			luaL_error(L, "vessel of vessel handle expected");
		}

		VECTOR3 pos = lua_tovector(L, 3);
		VECTOR3 dir = lua_tovector(L, 4);
		VECTOR3 up = lua_tovector(L, 5);
		double fov = luaL_checknumber(L, 6);
		SURFHANDLE hSurf = (SURFHANDLE)lua_touserdata(L,7);
		DWORD flags = 255;
		if(lua_gettop(L)>=8) {
			flags = luaL_checkinteger(L, 8);
		}
		hCam = pCore->SetupCustomCamera(hCam, hVessel, pos, dir, up, fov, hSurf, flags);
		if(cc) {
			cc->hCam = hCam;
			lua_pushvalue(L, 1);
		} else {
			lua_pushcustomcamera(L, hCam);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}


/***
Delete/Release a custom camera.

Note : Always delete all cameras bound to a render surface before releasing the rendering surface it-self.

@function delete_customcamera
@tparam handle hCam camera handle to delete.
@treturn number zero or an error code if the camara didn't work properly. (-1 if graphics client does not support custom cameras)
*/
int Interpreter::oapi_delete_customcamera(lua_State *L)
{
	LazyInitGCCore();
	if(pCore) {
		CustomCamera_Lua *cc = (CustomCamera_Lua *)luaL_checkudata(L, 1, "CustomCamera.vtable");
		lua_pushnumber(L, pCore->DeleteCustomCamera(cc->hCam));
		cc->hCam = nullptr;
		// The object will be garbage collected later, but unref potential callback here
		luaL_unref(L, LUA_REGISTRYINDEX, cc->clbk);
		cc->clbk = LUA_REFNIL;
	} else {
		lua_pushnumber(L, -1);
	}
	return 1;
}

/***
Toggle camera on and off.

Note : If multiple cameras are sharing the same rendering surface. Flickering will occur if more than one camera is turned on.

@function customcamera_onoff
@tparam handle hCam camera handle to toggle.
@tparam boolean bOn true to turn on the camera.
*/
int Interpreter::oapi_customcamera_onoff(lua_State *L)
{
	LazyInitGCCore();
	if(pCore) {
		CustomCamera_Lua *cc = (CustomCamera_Lua *)luaL_checkudata(L, 1, "CustomCamera.vtable");
		bool bOn = lua_toboolean(L, 2);
		pCore->CustomCameraOnOff(cc->hCam, bOn);
	}
	return 0;
}

void Interpreter::customcamera_clbk(oapi::Sketchpad *skp, void *pParam)
{
	CustomCamera_Lua *cc = (CustomCamera_Lua *)pParam;

	if(cc->clbk != LUA_REFNIL) {
		lua_rawgeti(cc->L, LUA_REGISTRYINDEX, cc->clbk); // push the callback function
		lua_pushsketchpad(cc->L, skp);
		LuaCall (cc->L, 1, 0);
	}
}

/***
Camera overlay.

Setup a custom camera overlay drawing callback.

@function customcamera_overlay
@tparam handle hCam camera handle to toggle.
@tparam function clbk pointer to a function to be called after each frame.
*/
int Interpreter::oapi_customcamera_overlay(lua_State *L)
{
	LazyInitGCCore();
	if(pCore) {
		CustomCamera_Lua *cc = (CustomCamera_Lua *)luaL_checkudata(L, 1, "CustomCamera.vtable");
		// unref previous callback if any
		luaL_unref(L, LUA_REGISTRYINDEX, cc->clbk);
		cc->clbk = LUA_REFNIL;

		if (lua_isfunction(L, 2)) {
			lua_pushvalue(L, 2);
			cc->clbk = luaL_ref(L, LUA_REGISTRYINDEX);
		} else {
			luaL_error(L, "Argument 2: function expected");
		}

		pCore->CustomCameraOverlay(cc->hCam, customcamera_clbk, cc);
	}
	return 0;
}

/***
Animations.
@section animations
*/

/***
Create an animation component object.

3 helper functions are provided to mimic C++ syntax :

- MGROUP_TRANSLATE(...)
- MGROUP_ROTATE(...)
- MGROUP_SCALE(...)

@function create_animationcomponent
@tparam table ac animation component definition
@treturn handle animation component object
@usage
p1={type='rotation',mesh=0,grp=3,ref={x=1,y=0,z=1},axis={x=1,y=0,z=0},angle=2}
acomp1=oapi.create_animationcomponent(p1)
p2={type='translation',mesh=1,grp={2,3,5},shift={x=10,y=5,z=0}}
acomp2=oapi.create_animationcomponent(p2)
p3={type='scaling',mesh=2,grp={7,0},ref={x=0,y=0,z=20},scale={x=2,y=2,z=2}}
acomp3=oapi.create_animationcomponent(p3)

@usage
-- C++ like
acomp1=MGROUP_ROTATE(0, 3, _V(1,0,1), _V(1,0,0), 2)
acomp2=MGROUP_TRANSLATE(1, {2,3,5}, _V(10,5,0))
acomp3=MGROUP_SCALE(2, {7,0}, _V(0,0,20), _V(2,2,2))
@see types.scalingcomponent
@see types.translationcomponent
@see types.rotationcomponent
*/
int Interpreter::oapi_create_animationcomponent (lua_State *L)
{
	MGROUP_TRANSFORM *trans;
	UINT mesh, *grp = nullptr;
	size_t ngrp, nbuf;
	ASSERT_TABLE(L,1);
	lua_getfield(L,1,"type");
	ASSERT_STRING(L,-1);
	char typestr[128];
	strcpy (typestr,lua_tostring(L,-1));
	lua_pop(L,1);
	lua_getfield(L,1,"mesh");
	ASSERT_NUMBER(L,-1);
	mesh = (UINT)lua_tointeger(L,-1);
	lua_pop(L,1);
	lua_getfield(L,1,"grp");
	if (lua_isnumber(L,-1)) { // single group index
		grp = new UINT[1];
		*grp = (UINT)lua_tointeger(L,-1);
		ngrp = 1;
	} else {
		ASSERT_TABLE(L,-1);
		ngrp = nbuf = 0;
		lua_pushnil(L);
		while(lua_next(L,-2)) {
			if (ngrp == nbuf) { // grow buffer
				UINT *tmp = new UINT[nbuf+=16];
				if (ngrp) {
					memcpy (tmp, grp, ngrp*sizeof(UINT));
					delete []grp;
				}
				grp = tmp;
			}
			grp[ngrp++] = (UINT)lua_tointeger(L,-1);
			lua_pop(L,1);
		}
	}
	lua_pop(L,1); // pop table of group indices

	if (!_stricmp(typestr, "rotation")) {
		lua_getfield(L,1,"ref");
		ASSERT_VECTOR(L,-1);
		VECTOR3 ref = lua_tovector(L,-1);
		lua_pop(L,1);
		lua_getfield(L,1,"axis");
		ASSERT_VECTOR(L,-1);
		VECTOR3 axis = lua_tovector(L,-1);
		lua_pop(L,1);
		lua_getfield(L,1,"angle");
		ASSERT_NUMBER(L,-1);
		double angle = lua_tonumber(L,-1);
		lua_pop(L,1);
		trans = new MGROUP_ROTATE(mesh,grp,ngrp,ref,axis,(float)angle);
	} else if (!_stricmp(typestr, "translation")) {
		lua_getfield(L,1,"shift");
		ASSERT_VECTOR(L,-1);
		VECTOR3 shift = lua_tovector(L,-1);
		lua_pop(L,1);
		trans = new MGROUP_TRANSLATE(mesh,grp,ngrp,shift);
	} else if (!_stricmp(typestr, "scaling")) {
		lua_getfield(L,1,"ref");
		ASSERT_VECTOR(L,-1);
		VECTOR3 ref = lua_tovector(L,-1);
		lua_pop(L,1);
		lua_getfield(L,1,"scale");
		ASSERT_VECTOR(L,-1);
		VECTOR3 scale = lua_tovector(L,-1);
		lua_pop(L,1);
		trans = new MGROUP_SCALE(mesh,grp,ngrp,ref,scale);
	} else {
		ASSERT_SYNTAX(0,"Invalid animation type");
	}
	lua_pushlightuserdata(L,trans);
	return 1;
}

/***
Delete an animation component object.

@function del_animationcomponent
@tparam handle ac animation component
*/
int Interpreter::oapi_del_animationcomponent (lua_State *L)
{
	ASSERT_LIGHTUSERDATA(L,1);
	MGROUP_TRANSFORM *trans = (MGROUP_TRANSFORM*)lua_touserdata(L,1);
	delete[] trans->grp;
	delete trans;
	return 0;
}

/***
MFD.
@section MFD
*/

/***
Set an MFD (multifunctional display) to a specific mode.

mode MFDMODE.NONE will turn off the MFD.

For the on-screen instruments, only MFDID.LEFT and MFDID.RIGHT are
supported. Custom panels may support (up to 3) additional MFDs.

@function open_mfd
@tparam number mode MFD mode
@tparam number mfd MFD identifier (e.g. MFDID.LEFT, MFDID.RIGHT)
*/

int Interpreter::oapi_open_mfd (lua_State *L)
{
	ASSERT_NUMBER(L,1);
	int mfdid = lua_tointeger(L,1);
	ASSERT_NUMBER(L,2);
	int mfdmode = lua_tointeger(L,2);
	oapiOpenMFD (mfdmode, mfdid);
	return 0;
}

/***
Set HUD (head up display) mode.

Mode HUD.NONE will turn off the HUD display.

See constants HUD.xxx for currently supported HUD modes.

@function set_hudmode
@tparam number mode new HUD mode
@treturn boolean true if mode has changed, false otherwise.
*/
int Interpreter::oapi_set_hudmode (lua_State *L)
{
	ASSERT_NUMBER(L,1);
	int hudmode = lua_tointeger(L,1);
	oapiSetHUDMode (hudmode);
	return 0;
}

/***
Query current HUD (head up display) mode.

@function get_hudmode
@treturn number current HUD mode.
*/
int Interpreter::oapi_get_hudmode (lua_State *L)
{
	int mode = oapiGetHUDMode();
	lua_pushnumber(L, mode);
	return 1;
}

/***
Define a panel blinking quad.

If provided with 4 vectors, this function will make a blinking quad appear in the panel.

Only the x and y fields of the vertices are used.

Calling this function with no arguments will make the quad disappear.

@function set_panelblink
@tparam vector p1 vertex.
@tparam vector p2 vertex.
@tparam vector p3 vertex.
@tparam vector p4 vertex.
*/
int Interpreter::oapi_set_panelblink (lua_State *L)
{
	int i;
	VECTOR3 v[4];
	if (lua_gettop(L) == 0) {
		oapiSetPanelBlink (NULL);
	} else {
		for (i = 0; i < 4; i++) {
			ASSERT_VECTOR(L,i+1);
			v[i] = lua_tovector(L,i+1);
		}
		oapiSetPanelBlink (v);
	}
	return 0;
}

/***
Get the current mode of the specified MFD.

@function get_mfdmode
@tparam number mfd MFD identifier (e.g. MFDID.LEFT, MFDID.RIGHT)
@treturn number MFD Mode
*/
int Interpreter::oapi_get_mfdmode(lua_State* L)
{
	ASSERT_NUMBER(L, 1);
	int mfd = lua_tointeger(L, 1);
	int mode = oapiGetMFDMode(mfd);
	lua_pushnumber(L, mode);
	return 1;
}

/***
Disable an MFD mode.

The list of disabled MFDs is cleared whenever the focus switches to a new
vessel. To disable MFD modes permanently for a particular vessel type,
oapi.disable_mfdmode() should be called from within the clbk_focuschanged() callback function.

For builtin MFD modes, mode can be any of the MFDMODE.xxx constants. For
MFD modes defined in plugin modules, the mode id must be obtained by a
call to oapi.get_mfdmodespec().

@function disable_mfdmode
@tparam number mode MFD mode to be disabled.
*/
int Interpreter::oapi_disable_mfdmode(lua_State* L)
{
	ASSERT_NUMBER(L, 1);
	int mode = lua_tointeger(L, 1);
	oapiDisableMFDMode(mode);
	return 0;
}

/***
Register an MFD position for a custom panel or virtual cockpit.

Should be called in the body of clbk\_loadpanel2D() or
clbk\_loadVC to define MFD instruments for 2-D instrument panels
or 3-D virtual cockpits.

The _spec_ parameter is a table with the following fields :

- pos: rectangle (position of MFD in panel [pixel])
- nmesh: number (mesh index (>=0))
- ngroup: number (mesh group index (>=0))
- flag: number (parameter flags (see below))
- nbt1: number (number of buttons in array 1 (e.g. left side of MFD display))
- nbt2: number (number of buttons in array 2 (e.g. right side of MFD display)
- bt_yofs: number (y-offset of top button from top display edge [pixel])
- bt_ydist: number (y-distance between buttons [pixel])

flag is a bitmask which can be set to a combination of the following options :

- MFDFLAG.SHOWMODELABELS: Show 3-letter abbreviations for MFD modes when displaying the
mode selection page (default: only show carets ">"). This is useful
if the buttons are not located next to the list display.
- MFDFLAG.TRANSPARENT\_WHEN\_OFF

If this function is used during initialisation of a 2-D instrument panel, pos
defines the rectangle of the MFD display in the panel bitmap (in pixels), while
nmesh and ngroup are ignored.

If it is used during initialisation of a virtual cockpit, nmesh and ngroup define
the mesh and group index of the mesh element which will receive the MFD
display texture, while pos is ignored.

@function register_mfd
@tparam number mfd MFD identifier (e.g. MFDID.LEFT, MFDID.RIGHT)
@tparam table spec MFD parameters (see above)
*/
int Interpreter::oapi_register_mfd(lua_State* L)
{
	EXTMFDSPEC spec;
	int mfd = lua_tointeger(L, 1);
	lua_getfield(L, 2, "pos"); ASSERT_SYNTAX(lua_istable(L, -1), "Argument : missing field 'pos'");
	spec.pos = lua_torect(L, -1); lua_pop(L, 1);
	lua_getfield(L, 2, "nmesh"); ASSERT_SYNTAX(lua_isnumber(L, -1), "Argument : missing field 'nmesh'");
	spec.nmesh = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 2, "ngroup"); ASSERT_SYNTAX(lua_isnumber(L, -1), "Argument : missing field 'ngroup'");
	spec.ngroup = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 2, "flag"); ASSERT_SYNTAX(lua_isnumber(L, -1), "Argument : missing field 'flag'");
	spec.flag = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 2, "nbt1"); ASSERT_SYNTAX(lua_isnumber(L, -1), "Argument : missing field 'nbt1'");
	spec.nbt1 = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 2, "nbt2"); ASSERT_SYNTAX(lua_isnumber(L, -1), "Argument : missing field 'nbt2'");
	spec.nbt2 = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 2, "bt_yofs"); ASSERT_SYNTAX(lua_isnumber(L, -1), "Argument : missing field 'bt_yofs'");
	spec.bt_yofs = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, 2, "bt_ydist"); ASSERT_SYNTAX(lua_isnumber(L, -1), "Argument : missing field 'bt_ydist'");
	spec.bt_ydist = lua_tointeger(L, -1); lua_pop(L, 1);

	oapiRegisterMFD(mfd, &spec);
	return 0;
}

/***
Request a default action as a result of a MFD button event.

Orbiter assigns default button actions for the various MFD modes. For
example, in Orbit mode the action assigned to button 0 is Select reference.
Calling oapi.process\_mfdbutton (for example as a reaction to a mouse button
event) will execute this action.

@function process_mfdbutton
@tparam number mfd MFD identifier (e.g. MFDID.LEFT, MFDID.RIGHT)
@tparam number bt button number (>=0)
@tparam number event mouse event (a combination of panel\_mouse "PANEL\_MOUSE.xxx" flags)
@treturn boolean true if the button was processed, false if no action was assigned to the button.
*/
int Interpreter::oapi_process_mfdbutton(lua_State* L)
{
	ASSERT_NUMBER(L, 1);
	int mfd = lua_tointeger(L, 1);
	ASSERT_NUMBER(L, 2);
	int bt = lua_tointeger(L, 2);
	ASSERT_NUMBER(L, 3);
	int event = lua_tointeger(L, 3);
	bool ret = oapiProcessMFDButton(mfd, bt, event);
	lua_pushboolean(L, ret);
	return 1;
}

/***
Send a keystroke to an MFD.

This function can be used to interact with the MFD as if the user had pressed
Shift-key, for example to select a different MFD mode, to select a target body, etc.

@function send_mfdkey
@tparam number mfd MFD identifier (e.g. MFDID.LEFT, MFDID.RIGHT)
@tparam number key key code (see keycodes "OAPI_KEY.xxx" Constants)
@treturn boolean true if the MFD understood and processed the key.
*/
int Interpreter::oapi_send_mfdkey(lua_State* L)
{
	ASSERT_NUMBER(L, 1);
	int mfd = lua_tointeger(L, 1);
	ASSERT_NUMBER(L, 2);
	int key = lua_tointeger(L, 2);
	int ret = oapiSendMFDKey(mfd, key);
	lua_pushboolean(L, ret);
	return 1;
}

/***
Sends clbk_MFDmode call to the current focus vessel.

This call allows the vessel to dynamically update its button labels.

This message will only be sent to the current input focus vessel. If hVessel ~= nil,
the function will not have any effect unless hVessel points to the focus vessel.

The recipient vessel will receive a clbk_MFDmode call, with the mode
parameter set to MFDMODE.REFRESHBUTTONS.

This function can be used to force an MFD to refresh its button labels even if
the mode has not changed. This is useful to update the labels for modes that
dynamically update their labels.

You don't need to call oapiRefreshMFDButtons after an actual mode
change, because a clbk_MFDmode call will be sent automatically by Orbiter.

@function refresh_mfdbuttons
@tparam number mfd MFD identifier (e.g. MFDID.LEFT, MFDID.RIGHT)
@tparam handle hVessel recipient vessel handle
*/
int Interpreter::oapi_refresh_mfdbuttons(lua_State* L)
{
	ASSERT_NUMBER(L, 1);
	int mfd = lua_tointeger(L, 1);
	ASSERT_LIGHTUSERDATA(L,2);
	OBJHANDLE hObj = lua_toObject (L, 2);
	oapiRefreshMFDButtons (mfd, hObj);
	return 0;
}

/***
Switch an MFD on or off.

Flips the on/off state of an MFD. Typically used to respond to
the user pressing the "power" button.

@function toggle_mfdon
@tparam number mfd MFD identifier (e.g. MFDID.LEFT, MFDID.RIGHT)
*/
int Interpreter::oapi_toggle_mfdon(lua_State* L)
{
	int mfd = luaL_checkinteger(L, 1);
	oapiToggleMFD_on(mfd);
	return 0;
}

/***
Return the mode identifier and spec for an MFD mode defined by its name.

This function returns the same value as vessel:register\_mfdmode() for the given mode.

If no matching mode is found, the return value is MFDMODE.NONE.

The mode identifiers for custom MFD modes can not be assumed to persist
across simulation runs, since they will change if the user loads or unloads MFD plugins.

This function can also be used for built-in MFD modes, with the following names :

- 'Orbit' : MFDMODE.ORBIT
- 'Surface' : MFDMODE.SURFACE
- 'Map' : MFDMODE.MAP
- 'HSI' : MFDMODE.HSI
- 'VOR/VTOL' : MFDMODE.LANDING
- 'Docking' : MFDMODE.DOCKING
- 'Align Planes' : MFDMODE.OPLANEALIGN
- 'Sync Orbit' : MFDMODE.OSYNC
- 'Transfer' : MFDMODE.TRANSFER
- 'COM/NAV' : MFDMODE.COMMS

@function get_mfdmodespec
@tparam string name MFD name (as defined in name during register\_mfdmode())
@treturn number mode identifier
@treturn table mode specification with the following fields :

- name: string
- key: number (shortcut keycode)
*/
int Interpreter::oapi_get_mfdmodespec(lua_State* L)
{
	const char *name = luaL_checkstring(L, 1);
	MFDMODESPECEX *t;
	int mode = oapiGetMFDModeSpecEx(const_cast<char *>(name), &t);
	lua_pushinteger(L, mode);
	if(mode != MFD_NONE) {
		lua_newtable(L);
		lua_pushstring (L, t->name);
		lua_setfield (L, -2, "name");
		lua_pushnumber (L, t->key);
		lua_setfield (L, -2, "key");
		// we don't return the msgproc or context since they or more or less useless for the Lua side
		return 2;
	}
	return 1;
}

/***
Define how the navigation mode buttons will be displayed in a default cockpit view.

This function should usually be called in the body of the overloaded clbk_loadgenericcockpit()

It defines if the buttons for navigation modes (e.g. "Killrot" or "Prograde") are
displayed in the generic (non-panel) cockpit camera mode, and if the buttons
can be operated with the mouse.

The following values for mode are defined :

- 0 buttons are not shown
- 1 buttons are shown and can be operated with the mouse (default)
- 2 only buttons representing active modes are shown, and can not be operated with the mouse

@function set_defnavdisplay
@tparam number mode display mode [0-2]
*/
int Interpreter::oapi_set_defnavdisplay(lua_State* L)
{
	ASSERT_NUMBER(L, 1);
	int mode = lua_tointeger(L, 1);
	oapiSetDefNavDisplay (mode);
	return 0;
}

/***
Enable or disable the display of the reaction control system indicators/controls in
default cockpit view.

This function should usually be called in the body of the overloaded clbk_loadgenericcockpit()

The RCS display consists of three buttons in the engine status display at the
top left of the generic cockpit view. If displayed (mode=1), the buttons show
the RCS mode (off/rotational/linear), and can be clicked with the mouse to switch modes.

The following values for mode are defined :

- 0 RCS buttons are not shown
- 1 RCS buttons are shown and can be operated with the mouse (default)

@function set_defrcsdisplay
@tparam number mode display mode [0-1]
*/
int Interpreter::oapi_set_defrcsdisplay(lua_State* L)
{
	ASSERT_NUMBER(L, 1);
	int mode = lua_tointeger(L, 1);
	oapiSetDefRCSDisplay (mode);
	return 0;
}

/***
Retrieve a default label for an MFD button.

Labels contain 1 to 3 characters.

This function can be used to paint the labels on the MFD buttons of a custom panel.

@function mfd_buttonlabel
@tparam number mfd MFD identifier (e.g. MFDID.LEFT, MFDID.RIGHT)
@tparam number bt button number (>=0)
@treturn string button label
*/
int Interpreter::oapi_mfd_buttonlabel(lua_State* L)
{
	ASSERT_NUMBER(L, 1);
	int mfd = lua_tointeger(L, 1);
	ASSERT_NUMBER(L, 2);
	int bt = lua_tointeger(L, 2);
	const char *label = oapiMFDButtonLabel(mfd, bt);
	lua_pushstring(L, label);
	return 1;
}

/***
Keyboard.
@section keystroke
*/

/***
Test if a key is pressed.

This function can be used with the clbk\_consumedirectkey/clbk\_consumebufferedkey callbacks
to test the state of a key.

@function keydown
@tparam handle kstate handle
@tparam number key code
@treturn boolean true is the specified key is pressed
@usage
-- example from HST Lua sample
function clbk_consumebufferedkey(key, down, kstate)
    if not down then -- only process keydown events
        return false
    end

    if oapi.keydown(kstate, OAPI_KEY.LCONTROL)
    or oapi.keydown(kstate, OAPI_KEY.RCONTROL) then
        if key == OAPI_KEY.KEY1 then     -- deploy/retract antenna
            ant_status = revert_status(ant_status)
            return true
        elseif key == OAPI_KEY.KEY2 then -- open/close hatch
            hatch_status = revert_status(hatch_status)
            return true
        elseif key == OAPI_KEY.KEY3 then -- open/fold solar arrays
            array_status = revert_status(array_status)
            return true
        end
    end
	return false
end

*/
int Interpreter::oapi_keydown (lua_State *L)
{
	ASSERT_LIGHTUSERDATA(L,1);
	char *kstate = (char*)lua_touserdata(L,1);
	ASSERT_NUMBER(L,2);
	int key = lua_tointeger(L, 2);
	lua_pushboolean (L, KEYDOWN(kstate,key));
	return 1;
}

/***
Clear a key from a key state.

@function resetkey
@tparam handle kstate handle
@tparam number key code
*/
int Interpreter::oapi_resetkey (lua_State *L)
{
	ASSERT_LIGHTUSERDATA(L,1);
	char *kstate = (char*)lua_touserdata(L,1);
	ASSERT_NUMBER(L,2);
	int key = lua_tointeger(L, 2);
	RESETKEY(kstate,key);
	return 0;
}

/***
Send a buffered key event to Orbiter, to be treated like a user keypress.

You can specify any number of "modifier keys", they will be reflected in the kstate passed to the key callback.

@function simulatebufferedkey
@tparam number key keycode
@tparam[opt] number modifier keys
@usage oapi.simulatebufferedkey(OAPI_KEY.MINUS, OAPI_KEY.LSHIFT)
*/
int Interpreter::oapi_simulatebufferedkey (lua_State *L)
{
	ASSERT_NUMBER(L,1);
	DWORD key = (DWORD)lua_tointeger(L,1);
	DWORD nmod = lua_gettop(L)-1;
	DWORD *mod = 0;
	if (nmod) {
		mod = new DWORD[nmod];
		for (DWORD i = 0; i < nmod; i++)
			mod[i] = (DWORD)lua_tointeger(L,i+2);
	}
	oapiSimulateBufferedKey (key, mod, nmod);
	if (nmod) delete []mod;
	return 0;
}

/***
Send a key state to Orbiter for one frame, to be treated like user keyboard input.

The keystate is constructed from a list of keys provided as separated arguments.

Orbiter doesn't process the simulated key state request directly, but merges
all requests and the actual key state for the current frame, and submits the result
once per frame.

To simulate a continuous key press event, this function must be called for multiple
frames for the duration of the simulated input.

@function simulateimmediatekey
@tparam number key keycode
@tparam[opt] number key2 additional keys
@usage oapi.simulateimmediatekey(OAPI_KEY.PLUS, OAPI_KEY.LSHIFT)
*/
int Interpreter::oapi_simulateimmediatekey (lua_State *L)
{
	unsigned char kstate[256] = {0};
	DWORD i, key, nkey = lua_gettop(L);
	for (i = 0; i < nkey; i++) {
		key = (DWORD)lua_tointeger(L,i+1);
		kstate[key] = 0x80;
	}
	oapiSimulateImmediateKey ((char*)kstate);
	return 0;
}

/***
Test if a key has been pressed for a duration.

WARNING: you cannot use it with several keys at the same time

@function acceptdelayedkey
@tparam number key keycode
@tparam number interval duration
@treturn boolean true if the was has been pressed for the specified duration
@usage
function clbk_consumedirectkey(kstate)
	if oapi.keydown(kstate, OAPI_KEY.E) then
		if oapi.acceptdelayedkey(OAPI_KEY.E, 1.0) then
			...
		end
	end
end
*/
int Interpreter::oapi_acceptdelayedkey (lua_State *L)
{
	ASSERT_NUMBER(L,1);
	ASSERT_NUMBER(L,2);
	char key = lua_tointeger(L, 1);
	double interval = lua_tonumber(L, 2);
	bool ret = oapiAcceptDelayedKey (key, interval);
	lua_pushboolean(L, ret);
	return 1;
}

/***
File I/O.
@section iofunctions
*/

/***
Open a file for reading or writing.

Note: The following access modes are supported:

   - FILE_IN read
   - FILE_IN_ZEROONFAIL read
   - FILE_OUT write (overwrite)
   - FILE_APP write (append)

The file path defined in fname is relative to either the main Orbiter folder or
   to one of Orbiter's default subfolders, depending on the root parameter:

   - ROOT Orbiter main directory
   - CONFIG Orbiter config folder
   - SCENARIOS Orbiter scenarios folder
   - TEXTURES Orbiter standard texture folder
   - TEXTURES2 Orbiter high-res texture folder
   - MESHES Orbiter mesh folder
   - MODULES Orbiter module folder

You should always specify a standard Orbiter subfolder by the above
   mechanism, rather than manually as a path in fname, because Orbiter
   installations can redirect these directories.
Access mode FILE_IN will always return a valid file handle, even if the file
   doesn't exist or can't be opened for reading (in which case all subsequent read
   attempts will fail). By contrast, FILE_IN_ZEROONFAIL will return 0 if the requested
   file can't be opened for reading.
Be careful when opening a file for writing in the standard Orbiter subfolders:
   except for ROOT and SCENARIOS, all other standard folders may be readonly
   (e.g. for CD installations)

@function openfile
@tparam string fname file name (with optional path)
@tparam FILE_ACCESS_MODE mode read/write mode (see notes)
@tparam PATH_ROOT root path origin (see notes)
@treturn FILEHANDLE file handle
@see closefile
*/
int Interpreter::oapi_openfile (lua_State* L)
{
	ASSERT_STRING(L, 1);
	ASSERT_NUMBER(L, 2);

	const char*    fname = lua_tostringex(L, 1);
	FileAccessMode mode = (FileAccessMode)lua_tointeger(L, 2);
	PathRoot       root = PathRoot::ROOT; // default
	if (lua_gettop(L) > 2) {
		ASSERT_NUMBER(L, 3);
		root = (PathRoot)lua_tointeger(L, 3);
	}

	FILEHANDLE f = oapiOpenFile(fname, mode, root);

	if (f) {
		lua_pushlightuserdata(L, f);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/***
Close a file after reading or writing.

Note: Use this function on files opened with oapiOpenFile after finishing with it.
   The file access mode passed to closefile must be the same as used to open it.

@function closefile
@tparam FILEHANDLE f file handle
@tparam FILE_ACCESS_MODE mode access mode with which the file was opened
*/
int Interpreter::oapi_closefile (lua_State* L)
{
	// oapiCloseFile noops on NULLs so we return early to prevent failed ASSERTS later
	if(lua_isnil(L, 1)) {
		return 0;
	}
	FILEHANDLE file;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle or nil)");
	ASSERT_SYNTAX(file = lua_toObject(L, 1), "Argument 1: invalid object");

	ASSERT_NUMBER(L, 2);
	FileAccessMode mode = (FileAccessMode)lua_tointeger(L, 2);

	oapiCloseFile(file, mode);
	return 0;
}

/***
Writes the current simulation state to a scenario file.

Note: The file name is always calculated relative from the default orbiter scenario
   folder (usually Orbiter/Scenarios). The file name can contain a relative path
   starting from that directory, but the subdirectories must already exist. The
   function will not create new directories. The file name should not contain an
   absolute path.
   The file name should not contain an extension. Orbiter will automatically add
   a .scn extension.
   The description string can be empty ("").

@function savescenario
@tparam string fname scenario file name
@tparam string desc scenario description
@treturn boolean _true_ if scenario could be written successfully, _false_ if an error occurred.
*/
int Interpreter::oapi_savescenario (lua_State* L)
{
	ASSERT_STRING(L, 1);
	ASSERT_STRING(L, 2);
	const char* fname = lua_tostringex(L, 1);
	const char* desc = lua_tostringex(L, 2);
	lua_pushboolean(L, oapiSaveScenario(fname, desc));
	return 1;
}

/***
Writes a line to a file.

@function writeline
@tparam FILEHANDLE f file handle
@tparam string line line to be written (zero-terminated)
*/
int Interpreter::oapi_writeline (lua_State* L)
{
	FILEHANDLE file;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(file = lua_toObject(L, 1), "Argument 1: invalid object");
	ASSERT_STRING(L, 2);
	const char* line = lua_tostringex(L, 2);
	oapiWriteLine(file, const_cast<char*>(line));
	return 0;
}

// int Interpreter::oapi_writelogv (lua_State * L);
// {
// 	return 1;
// }

/***
Writes a string-valued item to a scenario file.

@function writescenario_string
@tparam FILEHANDLE scn scenario file handle
@tparam string item item id
@tparam  string string to be written (zero-terminated)
*/
int Interpreter::oapi_writescenario_string (lua_State* L)
{
	FILEHANDLE scn;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(scn = lua_toObject(L, 1), "Argument 1: invalid object");
	ASSERT_STRING(L, 2);
	const char* item = lua_tostringex(L, 2);
	ASSERT_STRING(L, 3);
	const char* string = lua_tostringex(L, 3);
	oapiWriteScenario_string(scn, const_cast<char*>(item), const_cast<char*>(string));
	return 0;
}

/***
Writes an integer-valued item to a scenario file.

@function writescenario_int
@tparam FILEHANDLE scn scenario file handle
@tparam string item item id
@tparam integer i integer value to be written
*/
int Interpreter::oapi_writescenario_int (lua_State* L)
{
	FILEHANDLE scn;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(scn = lua_toObject(L, 1), "Argument 1: invalid object");
	ASSERT_STRING(L, 2);
	const char* item = lua_tostringex(L, 2);
	ASSERT_NUMBER(L, 3);
	int i = lua_tointeger(L, 3);
	oapiWriteScenario_int(scn, const_cast<char*>(item), i);
	return 0;
}

/***
Writes a floating point-valued item to a scenario file.

@function writescenario_float
@tparam FILEHANDLE scn scenario file handle
@tparam string item item id
@tparam double d floating point value to be written
*/
int Interpreter::oapi_writescenario_float (lua_State* L)
{
	FILEHANDLE scn;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(scn = lua_toObject(L, 1), "Argument 1: invalid object");
	ASSERT_STRING(L, 2);
	const char* item = lua_tostringex(L, 2);
	ASSERT_NUMBER(L, 3);
	double d = lua_tonumber(L, 3);
	oapiWriteScenario_float(scn, const_cast<char*>(item), d);
	return 0;
}

/***
Writes a vector-valued item to a scenario file.

@function writescenario_vec
@tparam FILEHANDLE scn scenario file handle
@tparam string item item id
@tparam VECTOR3 vec vector to be written
*/
int Interpreter::oapi_writescenario_vec (lua_State* L)
{
	FILEHANDLE scn;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(scn = lua_toObject(L, 1), "Argument 1: invalid object");
	ASSERT_STRING(L, 2);
	const char* item = lua_tostringex(L, 2);
	ASSERT_VECTOR(L, 3);
	const VECTOR3 vec = lua_tovector(L, 3);
	oapiWriteScenario_vec(scn, const_cast<char*>(item), vec);
	return 0;
}

/***
Reads an item from a scenario file.

Note: The function returns lines as long as an item for the current block could be
   read. It returns _nil_ at EOF, or when an "END" token is read.
   Leading and trailing whitespace, and trailing comments (from ";" to EOL) are
   automatically removed.
   "line" points to an internal static character buffer. The buffer grows
   automatically to hold lines of arbitrary length.
   The buffer is overwritten on the next call to readscenario_nextline,
   so it must be copied or processed before the next call.

@function readscenario_nextline
@tparam FILEHANDLE scn scenario file handle
@treturn line pointer to the scanned line as long as an item for the current block
   could be read, _nil_ if not.
*/
int Interpreter::oapi_readscenario_nextline (lua_State* L)
{
	FILEHANDLE scn;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(scn = lua_toObject(L, 1), "Argument 1: invalid object");

	char* line;
	bool ok = oapiReadScenario_nextline(scn, line);
	if (ok) {
		lua_pushstring(L, line);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/***
Read the value of a tag from a configuration file.

Note: The tag-value entries of a configuration file have the format \<tag\> = \<value\>
   The functions search the complete file independent of the current position of the file pointer.
   Whitespace around tag and value are discarded, as well as comments
   beginning with a semicolon (;) to the end of the line.
   String values can contain internal whitespace.

@function readitem_string
@tparam FILEHANDLE f file handle
@tparam string item pointer to tag string
@treturn string value if tag was found in the file, _nil_ if not.
*/
int Interpreter::oapi_readitem_string (lua_State* L)
{
	FILEHANDLE f;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(f = lua_toObject(L, 1), "Argument 1: invalid object");

	ASSERT_STRING(L, 2);
	const char* item = lua_tostringex(L, 2);

	char cbuf[1024];
	bool ok = oapiReadItem_string(f, const_cast<char*>(item), cbuf);
	if (ok) {
		lua_pushstring(L, cbuf);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/***
Read the value of a tag from a configuration file.

@function readitem_float
@tparam FILEHANDLE f file handle
@tparam string item pointer to tag string
@treturn float value if tag was found in the file, _nil_ if not.
@see readitem_string
*/
int Interpreter::oapi_readitem_float (lua_State* L)
{
	FILEHANDLE f;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(f = lua_toObject(L, 1), "Argument 1: invalid object");

	ASSERT_STRING(L, 2);
	const char* item = lua_tostringex(L, 2);

	double d;
	bool ok = oapiReadItem_float(f, const_cast<char*>(item), d);
	if (ok) {
		lua_pushnumber(L, d);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/***
Read the value of a tag from a configuration file.

@function readitem_int
@tparam FILEHANDLE f file handle
@tparam string item pointer to tag string
@treturn integer value if tag was found in the file, _nil_ if not.
@see readitem_string
*/
int Interpreter::oapi_readitem_int (lua_State* L)
{
	FILEHANDLE f;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(f = lua_toObject(L, 1), "Argument 1: invalid object");

	ASSERT_STRING(L, 2);
	const char* item = lua_tostringex(L, 2);

	int i;
	bool ok = oapiReadItem_int(f, const_cast<char*>(item), i);
	if (ok) {
		lua_pushnumber(L, i);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/***
Read the value of a tag from a configuration file.

Note: In a file boolean values are represented by the strings "FALSE" and "TRUE".

@function readitem_bool
@tparam FILEHANDLE f file handle
@tparam string item pointer to tag string
@treturn boolean value if tag was found in the file, _nil_ if not.
@see readitem_string
*/
int Interpreter::oapi_readitem_bool (lua_State* L)
{
	FILEHANDLE f;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(f = lua_toObject(L, 1), "Argument 1: invalid object");

	ASSERT_STRING(L, 2);
	const char* item = lua_tostringex(L, 2);

	bool b;
	bool ok = oapiReadItem_bool(f, const_cast<char*>(item), b);
	if (ok) {
		lua_pushboolean(L, b);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/***
Read the value of a tag from a configuration file.

Note: Vector values are represented by space-separated triplets of floating point values.

@function readitem_vec
@tparam FILEHANDLE f file handle
@tparam string item pointer to tag string
@treturn VECTOR3 value if tag was found in the file, _nil_ if not.
@see readitem_string
*/
int Interpreter::oapi_readitem_vec (lua_State* L)
{
	FILEHANDLE f;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(f = lua_toObject(L, 1), "Argument 1: invalid object");

	ASSERT_STRING(L, 2);
	const char* item = lua_tostringex(L, 2);

	VECTOR3 vec;
	bool ok = oapiReadItem_vec(f, const_cast<char*>(item), vec);
	if (ok) {
		lua_pushvector(L, vec);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/***
Write a tag and its value to a configuration file.

Note: Use these functions to write items (tags and values) to configuration files.
   The format of the written items is recognised by the corresponding readitem_xxx functions.

For historic reasons, the format for scenario file entries is different.
   Use the writeline function.

@function writeitem_string
@tparam FILEHANDLE f file handle
@tparam string item pointer to tag string
@tparam string string character-string value
@see readitem_string
*/
int Interpreter::oapi_writeitem_string (lua_State* L)
{
	FILEHANDLE f;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(f = lua_toObject(L, 1), "Argument 1: invalid object");
	ASSERT_STRING(L, 2);
	ASSERT_STRING(L, 3);

	const char* item = lua_tostringex(L, 2);
	const char* string = lua_tostringex(L, 3);

	oapiWriteItem_string(f, const_cast<char*>(item), const_cast<char*>(string));
	return 0;
}

/***
Write a tag and its value to a configuration file.

@function writeitem_float
@tparam FILEHANDLE f file handle
@tparam string item pointer to tag string
@tparam number d double value
@see writeitem_string
*/
int Interpreter::oapi_writeitem_float (lua_State* L)
{
	FILEHANDLE f;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(f = lua_toObject(L, 1), "Argument 1: invalid object");
	ASSERT_STRING(L, 2);
	ASSERT_NUMBER(L, 3);

	const char* item = lua_tostringex(L, 2);
	double d = lua_tonumber(L, 3);

	oapiWriteItem_float(f, const_cast<char*>(item), d);
	return 0;
}

/***
Write a tag and its value to a configuration file.

@function writeitem_int
@tparam FILEHANDLE f file handle
@tparam string item pointer to tag string
@tparam int i integer value
@see writeitem_string
*/
int Interpreter::oapi_writeitem_int (lua_State* L)
{
	FILEHANDLE f;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(f = lua_toObject(L, 1), "Argument 1: invalid object");
	ASSERT_STRING(L, 2);
	ASSERT_NUMBER(L, 3);

	const char* item = lua_tostringex(L, 2);
	int i = lua_tointeger(L, 3);

	oapiWriteItem_int(f, const_cast<char*>(item), i);
	return 0;
}

/***
Write a tag and its value to a configuration file.

Note: In a file boolean values are represented by the strings "FALSE" and "TRUE".

@function writeitem_bool
@tparam FILEHANDLE f file handle
@tparam string item pointer to tag string
@tparam bool b boolean value
@see writeitem_string
*/
int Interpreter::oapi_writeitem_bool (lua_State* L)
{
	FILEHANDLE f;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(f = lua_toObject(L, 1), "Argument 1: invalid object");
	ASSERT_STRING(L, 2);
	ASSERT_BOOLEAN(L, 3);

	const char* item = lua_tostringex(L, 2);
	bool b = lua_toboolean(L, 3);

	oapiWriteItem_bool(f, const_cast<char*>(item), b);
	return 0;
}

/***
Write a tag and its value to a configuration file.

Note: Vector values are represented by space-separated triplets of floating point values.

@function writeitem_vec
@tparam FILEHANDLE f file handle
@tparam string item pointer to tag string
@tparam VECTOR3 vec vector value
@see writeitem_string
*/
int Interpreter::oapi_writeitem_vec (lua_State* L)
{
	FILEHANDLE f;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(f = lua_toObject(L, 1), "Argument 1: invalid object");
	ASSERT_STRING(L, 2);
	ASSERT_VECTOR(L, 3);

	const char* item = lua_tostringex(L, 2);
	VECTOR3 vec = lua_tovector(L, 3);

	oapiWriteItem_vec(f,  const_cast<char*>(item), vec);
	return 0;
}


// ============================================================================
// utility functions

/***
Return uniformly distributed pseudo-random number in the range [0..1].

This function uses the system call rand(), so the quality of the random
   sequence depends on the system implementation. If you need high-quality
   random sequences you may need to implement your own generator.

Orbiter seeds the generator with the system time on startup, so the
   generated sequences are not reproducible.

@function rand
@treturn number Random value between 0 and 1.
*/
int Interpreter::oapi_rand (lua_State *L)
{
	lua_pushnumber(L, oapiRand());
	return 1;
}

/***
Deflate (or pack) a string.

This function is called with one string (a bytes array, as Lua strings can
  contain binary zero as well)

@function deflate
@tparam string inp unpacked input data buffer
@treturn string out packed output data buffer
@see inflate
*/
int Interpreter::oapi_deflate (lua_State *L)
{
	ASSERT_STRING(L, 1);

	const BYTE *ebuf = (BYTE*)lua_tostring(L, 1);
	DWORD      nebuf = lua_rawlen(L, 1);
	BYTE       *zbuf = NULL;
	DWORD      nzbuf = 0;

	for (DWORD nbuf = 1024; !nzbuf; nbuf *= 2)
	{
		if (zbuf) delete[] zbuf;
		zbuf = new BYTE[nbuf];
		nzbuf = oapiDeflate(ebuf, nebuf, zbuf, nbuf);
	}

	lua_pushlstring(L, (const char *)zbuf, nzbuf);

	delete[] zbuf;
	return 1;
}

/***
Inflate (or unpack) a packed string that was packed by @{deflate} or by the
according Orbiter core function.

The new tree-data files for example are packed this way.

This function is called with one string (a bytes array, as Lua strings can
   contain binary zero as well)

@function inflate
@tparam string inp packed input data buffer
@treturn string out unpacked output data buffer
@see deflate
*/
int Interpreter::oapi_inflate (lua_State *L)
{
	ASSERT_STRING(L, 1);

	const BYTE *zbuf = (BYTE*)lua_tostring(L, 1);
	DWORD      nzbuf = lua_rawlen(L, 1);
	BYTE       *ebuf = NULL;
	DWORD      nebuf = 0;

	for (DWORD nbuf = 1024; !nebuf; nbuf *= 2)
	{
		if (ebuf) delete[] ebuf;
		ebuf = new BYTE[nbuf];
		nebuf = oapiInflate(zbuf, nzbuf, ebuf, nbuf);
	}

	lua_pushlstring(L, (const char *)ebuf, nebuf);

	delete[] ebuf;
	return 1;
}

/***
Return a colour value adapted to the current screen colour depth for given
red, green and blue components.

Colour values are required for some surface functions like @{clear_surface}.
   The colour key for a given RGB triplet depends
   on the screen colour depth. This function returns the colour value for the
   closest colour match which can be displayed in the current screen mode.

In 24 and 32 bit modes the requested colour can always be matched. The
   colour value in that case is (red \<\< 16) + (green \<\< 8) + blue.

For 16 bit displays the colour value is calculated as
   ((red*31)/255) \<\< 11 + ((green*63)/255 \<\< 5 + (blue*31)/255
   assuming a "565" colour mode (5 bits for red, 6, for green, 5 for blue). This
   means that a requested colour may not be perfectly matched.

These colour values should not be used for Windows (GDI) drawing
   functions where a COLORREF value is expected.

@function get_color
@tparam int red red component (0-255)
@tparam int green green component (0-255)
@tparam int blue blue component (0-255)
@treturn int colour value
*/
int Interpreter::oapi_get_color (lua_State *L)
{
	ASSERT_NUMBER(L, 1);
	ASSERT_NUMBER(L, 2);
	ASSERT_NUMBER(L, 3);
	DWORD r = lua_tointeger(L, 1);
	DWORD g = lua_tointeger(L, 2);
	DWORD b = lua_tointeger(L, 3);
	lua_pushnumber(L, oapiGetColour(r, g, b));
	return 1;
}

/***
Format floating point value f in the standard Orbiter convention,
   with given precision, using 'k', 'M' and 'G' postfixes as required.

@function formatvalue
@tparam number f floating point value
@tparam int precision output precision (optional, default: 4)
@treturn string formatted string
*/
int Interpreter::oapi_formatvalue (lua_State* L)
{
	ASSERT_NUMBER(L, 1);
	double f = lua_tonumber(L, 1);
	int p = 4; // default
	if (lua_gettop(L) >= 2) {
		ASSERT_NUMBER(L, 2);
		p = lua_tointeger(L, 2);
	}
	char cbuf[64];
	FormatValue(cbuf, 64, f, p);
	lua_pushfstring(L, cbuf);
	return 1;
}

/***
Create a beacon object.

- shape: number (beacon shape identifier: BEACONSHAPE.COMPACT, BEACONSHAPE.DIFFUSE or BEACONSHAPE.STAR)
- pos: vector (position in vessel coordinates)
- col: vector (beacon RGB colour)
- size: number (beacon radius)
- fallof: number (distance falloff parameter)
- period: number (strobe period (0 for continuous))
- duration: number (strobe duration)
- tofs: number (strobe time offset)
- active: boolean (beacon lit?)

The object is intended to be used with the vessel:add_beacon() function.

@function create_beacon
@tparam table prm beacon parameters
@treturn beacon object
@usage -- DG example :
local beaconpos = {_V(-8.6,0,-3.3), _V(8.6,0,-3.3), _V(0,0.5,-7.5), _V(0,2.2,2),
             _V(0,-1.4,2), _V(-8.9,2.5,-5.4), _V(8.9,2.5,-5.4), _V(2.5,-0.5,6.5)}
local beaconpos_scram = _V(0,-1.8,2)
local beaconcol = {_V(1.0,0.5,0.5), _V(0.5,1.0,0.5), _V(1,1,1), _V(1,0.6,0.6),
             _V(1,0.6,0.6), _V(1,1,1), _V(1,1,1) , _V(1,1,1)}
self.beacon = {}
for i=1,8 do
	self.beacon[i] = oapi.create_beacon({
		shape = i < 4 and BEACONSHAPE.DIFFUSE or BEACONSHAPE.STAR,
		pos = beaconpos[i],
		col = beaconcol[i],
		size = (i < 4 or i == 8) and 0.3 or 0.55,
		falloff = i < 4 and 0.4 or 0.6,
		period = i < 4 and 0 or (i < 6 and 2 or (i < 8 and 1.13 or 0)),
		duration = i < 6 and 0.1 or 0.05,
		tofs = (6-i-1)*0.2,
		active = false
	})
	self:add_beacon(self.beacon[i])
end
if self.ssys_scram then
	self.beacon[5].pos = beaconpos_scram
end

*/
int Interpreter::oapi_create_beacon(lua_State *L)
{
	BEACONLIGHTSPEC_Lua *beacon = (BEACONLIGHTSPEC_Lua *)lua_newuserdata(L, sizeof(BEACONLIGHTSPEC_Lua));
	beacon->bs.pos = &beacon->pos;
	beacon->bs.col = &beacon->col;
	beacon->vessel = nullptr;
	luaL_getmetatable(L, "Beacon.vtable");
	lua_setmetatable(L, -2);

	lua_getfield (L, 1, "shape");  beacon->bs.shape = luaL_checkinteger (L, -1);  lua_pop (L,1);
	lua_getfield (L, 1, "pos");  beacon->pos = lua_tovector_safe (L, -1, "create_beacon");  lua_pop (L,1);
	lua_getfield (L, 1, "col");  beacon->col = lua_tovector_safe (L, -1, "create_beacon");  lua_pop (L,1);
	lua_getfield (L, 1, "size");  beacon->bs.size = luaL_checknumber (L, -1);  lua_pop (L,1);
	lua_getfield (L, 1, "falloff");  beacon->bs.falloff = luaL_checknumber (L, -1);  lua_pop (L,1);
	lua_getfield (L, 1, "period");  beacon->bs.period = luaL_checknumber (L, -1);  lua_pop (L,1);
	lua_getfield (L, 1, "duration");  beacon->bs.duration = luaL_checknumber (L, -1);  lua_pop (L,1);
	lua_getfield (L, 1, "tofs");  beacon->bs.tofs = luaL_checknumber (L, -1);  lua_pop (L,1);
	lua_getfield (L, 1, "active");  beacon->bs.active = lua_toboolean (L, -1);  lua_pop (L,1);
	
	return 1;
}

/***
Vertex arrays.
@section vertexarray
*/

NTVERTEX lua_tontvertex(lua_State *L, int idx)
{
	int type = lua_type(L, idx);
	if(type != LUA_TTABLE || lua_rawlen(L, idx) != 8) {
		luaL_error(L, "invalid argument for ntvertex creation");
	}
	NTVERTEX ret;
	lua_rawgeti(L, 1, idx);
	ret.x = luaL_checknumber(L, -1); lua_pop(L, 1);
	lua_rawgeti(L, 2, idx);
	ret.y = luaL_checknumber(L, -1); lua_pop(L, 1);
	lua_rawgeti(L, 3, idx);
	ret.z = luaL_checknumber(L, -1); lua_pop(L, 1);
	lua_rawgeti(L, 4, idx);
	ret.nx = luaL_checknumber(L, -1); lua_pop(L, 1);
	lua_rawgeti(L, 5, idx);
	ret.ny = luaL_checknumber(L, -1); lua_pop(L, 1);
	lua_rawgeti(L, 6, idx);
	ret.nz = luaL_checknumber(L, -1); lua_pop(L, 1);
	lua_rawgeti(L, 7, idx);
	ret.tu = luaL_checknumber(L, -1); lua_pop(L, 1);
	lua_rawgeti(L, 8, idx);
	ret.tv = luaL_checknumber(L, -1); lua_pop(L, 1);
	return ret;
}

void Interpreter::push_ntvertexarray(lua_State *L, NTVERTEX *vtx, int nVtx)
{
	ntv_data *array = (ntv_data *)lua_newuserdata(L, sizeof(ntv_data));
    
	luaL_getmetatable(L, "NTV.vtable");
	lua_setmetatable(L, -2);
    
	array->nVtx = nVtx;
	array->nVtxUsed = nVtx;
	array->vtx = vtx;
	array->owning = false;
}

/***
Create a vertex array object.

An array can be created with a specific size, or initialized from an explicit list of vertices.

@function create_ntvertexarray
@tparam table|number def definition of the vertex array
@treturn ntvertexarray
@usage
    local VTX = oapi.create_ntvertexarray({
        -- VS tape
        {xcnt-22,ycnt-59,0,  0,0,0,  tapex0/texw,        tapey0/texh},
        {xcnt+22,ycnt-59,0,  0,0,0,  (tapex0+tapew)/texw,tapey0/texh},
        {xcnt-22,ycnt+59,0,  0,0,0,  tapex0/texw,        (tapey0+tapeh)/texh},
        {xcnt+22,ycnt+59,0,  0,0,0,  (tapex0+tapew)/texw,(tapey0+tapeh)/texh},
        ...
    })
    local vgear = oapi.create_ntvertexarray(12)
*/
int Interpreter::oapi_create_ntvertexarray(lua_State *L)
{
	int type = lua_type(L, 1);
	int nVtx;
	if(type == LUA_TTABLE) {
		nVtx = lua_rawlen(L,1);
	} else if (type == LUA_TNUMBER) {
		nVtx = lua_tointeger(L, 1);
	} else {
		return luaL_error(L, "Invalid type for create_ntvertexarray, number or table expected");
	}

	ntv_data *array = (ntv_data *)lua_newuserdata(L, sizeof(ntv_data));
    
	luaL_getmetatable(L, "NTV.vtable");
	lua_setmetatable(L, -2);
    
	array->nVtx = nVtx;
	array->nVtxUsed = nVtx;
	array->vtx = new NTVERTEX[nVtx];
	array->owning = true;

	if(type == LUA_TTABLE) {
		lua_pushnil(L);
		int i = 0;
		while (lua_next(L, 1) != 0) {
			array->vtx[i] = lua_tontvertex(L, -1);
			lua_pop(L, 1);
			i++;
		}
	}


	return 1;
}

/***
Delete a vertex array object.

@function del_ntvertexarray
@tparam ntvertexarray vtx vertex array to delete
*/
int Interpreter::oapi_del_ntvertexarray(lua_State *L)
{
	ntv_data* inst = (ntv_data*)luaL_checkudata(L, 1, "NTV.vtable");
	if(inst->owning) {
		delete []inst->vtx;
		inst->owning = false;
	}
	inst->vtx = nullptr;
	return 0;
}

/***
Create an index array object.

An array can be created with a specific size, or initialized from an explicit list of indices.

@function create_indexarray
@tparam table|number def definition of the vertex array
@treturn indexarray
@usage
-- explicit construction
local vidx = oapi.create_indexarray({0,1,4,5,20,21,8,9,24,25,16,17,12,13,28,29})
-- create by size then initialize
self.vperm = oapi.create_indexarray(4)
for i=1,4 do
   self.vperm[i] = i + VC_VSTAPE_vofs - 1
end
*/
int Interpreter::oapi_create_indexarray(lua_State *L)
{
	int type = lua_type(L, 1);
	int nIdx;
	if(type == LUA_TTABLE) {
		nIdx = lua_rawlen(L,1);
	} else if (type == LUA_TNUMBER) {
		nIdx = lua_tointeger(L, 1);
	} else {
		return luaL_error(L, "Invalid type for create_indexarray, number or table expected");
	}

	index_data *array = (index_data *)lua_newuserdata(L, sizeof(index_data));
    
	luaL_getmetatable(L, "Index.vtable");
	lua_setmetatable(L, -2);
    
	array->nIdx = nIdx;
	array->nIdxUsed = nIdx;
	array->idx = new WORD[nIdx];
	array->owning = true;

	if(type == LUA_TTABLE) {
		lua_pushnil(L);
		int i = 0;
		while (lua_next(L, 1) != 0) {
			array->idx[i] = luaL_checkinteger(L, -1);
			lua_pop(L, 1);
			i++;
		}
	}

	return 1;
}

/***
Delete an index array object.

@function del_indexarray
@tparam ntvertexarray idx index array to delete
*/
int Interpreter::oapi_del_indexarray(lua_State *L)
{
	index_data* inst = (index_data*)luaL_checkudata(L, 1, "Index.vtable");
	if(inst->owning) {
		delete []inst->idx;
		inst->owning = false;
	}
	inst->idx = nullptr;
	return 0;
}

/***
Sketchpad.
@section sketchpad
*/


/***
Obtain a drawing context for a surface.

The context must be released with oapi.release_sketchpad after drawing.

Most graphics clients must lock the surface data buffer (and copy it
to main memory, if necessary) before drawing access can be provided. This
means that read/write access to the surface (e.g. for blitting) may be
disabled between oapi.get_sketchpad and oapi.release_sketchpad, and should
be avoided.

@function get_sketchpad
@tparam handle surface handle
@treturn sketchpad drawing context
*/
int Interpreter::oapi_get_sketchpad(lua_State* L)
{
	ASSERT_LIGHTUSERDATA(L, 1);
	SURFHANDLE surf = (SURFHANDLE)lua_touserdata(L, 1);
	oapi::Sketchpad *skp = oapiGetSketchpad(surf);
	lua_pushsketchpad(L, skp);
	return 1;
}

/***
Release a drawing device context instance.

@function release_sketchpad
@tparam sketchpad skp drawing context
*/
int Interpreter::oapi_release_sketchpad(lua_State* L)
{
	oapi::Sketchpad* skp = lua_tosketchpad(L, 1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	oapiReleaseSketchpad(skp);
	return 0;
}

/***
Create a font resource for drawing text into surfaces.

The following generic typeface names should be understood
by all graphics systems:

- Fixed (fixed pitch font)
- Sans (sans-serif proportional font)
- Serif (serif proportional font)

Other font names may not be recognised by all graphics clients.
In that case, the default fixed or sans-serif font will be used,
depending on the value of prop.

The decoration style flags allow bold, italic and underlining.

After use, the font should be deallocated with oapi.release_font.

@function create_font
@tparam number height font height [pixel]
@tparam boolean prop flag for proportional/fixed pitch font
@tparam string face typeface name (see notes)
@tparam[opt=FONT.NORMAL] number style font decoration style (see notes)
@treturn handle font handle
*/
int Interpreter::oapi_create_font(lua_State* L)
{
	ASSERT_NUMBER(L, 1);
	int height = lua_tonumber(L, 1);
	ASSERT_BOOLEAN(L, 2);
	bool prop = lua_toboolean(L, 2);
	ASSERT_STRING(L, 3);
	const char* face = lua_tostring(L, 3);

	FontStyle style = FONT_NORMAL;
	if(lua_gettop(L) >=4)
		style = (FontStyle)luaL_checkinteger(L, 4);

	oapi::Font* font = oapiCreateFont(height, prop, const_cast<char*>(face), style);

	if (font) lua_pushlightuserdata(L, font);
	else     lua_pushnil(L);
	return 1;
}

/***
Release a font resource.

@function release_font
@tparam handle font handle
*/
int Interpreter::oapi_release_font(lua_State* L)
{
	ASSERT_LIGHTUSERDATA(L, 1);
	oapi::Font* font = (oapi::Font*)lua_touserdata(L, 1);

	oapiReleaseFont(font);

	return 0;
}

/***
Create a pen resource for drawing lines and shape outlines.

After use, the pen should be deallocated with oapi.release_pen.

@function create_pen
@tparam number style line style (0=invisible, 1=solid, 2=dashed)
@tparam number width line width [pixel]
@tparam number col line colour (format: 0xBBGGRR)
@treturn handle pen ressource
*/
int Interpreter::oapi_create_pen(lua_State* L)
{
	ASSERT_NUMBER(L, 3);
	int style = lua_tonumber(L, 1);
	int width = lua_tonumber(L, 2);
	DWORD col = lua_tonumber(L, 3);

	oapi::Pen* pen = oapiCreatePen(style, width, col);
	if (pen) lua_pushlightuserdata(L, pen);
	else     lua_pushnil(L);
	return 1;
}

/***
Release a pen resource.

@function release_pen
@tparam handle pen handle
*/
int Interpreter::oapi_release_pen(lua_State* L)
{
	ASSERT_LIGHTUSERDATA(L, 1);
	oapi::Pen* pen = (oapi::Pen*)lua_touserdata(L, 1);

	oapiReleasePen(pen);

	return 0;
}


/***
Create a brush resource for filling shapes.

After use, the brush should be deallocated with oapi.release_brush.

@function create_brush
@tparam number col shape fill colour (format: 0xBBGGRR)
@treturn handle pen ressource
*/
int Interpreter::oapi_create_brush(lua_State* L)
{
	ASSERT_NUMBER(L, 1);
	DWORD col = lua_tonumber(L, 1);

	oapi::Brush* brush = oapiCreateBrush(col);
	if (brush) lua_pushlightuserdata(L, brush);
	else     lua_pushnil(L);
	return 1;
}

/***
Release a pen resource.

@function release_brush
@tparam handle pen handle
*/
int Interpreter::oapi_release_brush(lua_State* L)
{
	ASSERT_LIGHTUSERDATA(L, 1);
	oapi::Brush* brush = (oapi::Brush*)lua_touserdata(L, 1);

	oapiReleaseBrush(brush);

	return 0;
}

/***
Copy a rectangular area from one surface to another.

This function copies rectangular areas between two surfaces, or between two
locations of the same surface.

A typical use is the dynamic update of instrument panels, e.g. in the
body of clbk_panelredrawevent.

This function must not be used while a device context is acquired for the
target surface (i.e. between oapi.get_sketchpad() and oapi.release_sketchpad calls).

@function blt
@tparam handle tgt target surface
@tparam handle src source surface
@tparam number tgtx left edge of target rectangle [pixel]
@tparam number tgty top edge of target rectangle [pixel]
@tparam number srcx left edge of source rectangle [pixel]
@tparam number srcy top edge of source rectangle [pixel]
@tparam number w width of copied rectangle [pixel]
@tparam number h height of copied rectangle [pixel]
*/
int Interpreter::oapi_blt(lua_State* L)
{
	SURFHANDLE tgt = (SURFHANDLE)lua_touserdata(L, 1);
	SURFHANDLE src = (SURFHANDLE)lua_touserdata(L, 2);
	int tgtx = lua_tonumber(L, 3);
	int tgty = lua_tonumber(L, 4);
	int srcx = lua_tonumber(L, 5);
	int srcy = lua_tonumber(L, 6);
	int w = lua_tonumber(L, 7);
	int h = lua_tonumber(L, 8);

	oapiBlt(tgt, src, tgtx, tgty, srcx, srcy, w, h);
	return 0;
}

/***
Copie the stored background of a panel area into the provided surface.

This function should only be called from within the repaint callback 
function of an area registered with the PANEL_MAP.BGONREQUEST flag.

Areas defined with the PANEL_MAP.BGONREQUEST receive a surface
with undefined contents when their repaint callback is called. They can use
oapi.blt_panelareabackground to copy the area background into the surface.

For areas not registered with the PANEL_MAP.BGONREQUEST, this
function will do nothing.

Using PANEL_MAP.BGONREQUEST is more efficient than
PANEL_MAP_BACKGROUND if the area doesn't need to be repainted at
each call of the callback function, because it delays blitting the background
until the module requests the background. This is particularly significant for
areas which are updated at each time step.

@function blt_panelareabackground
@tparam number area_id area identifier
@tparam handle surf surface handle
@treturn boolean true if the operation succeeded
*/
int Interpreter::oapi_blt_panelareabackground(lua_State* L)
{
	int area_id = luaL_checkinteger(L, 1);
	SURFHANDLE surf = (SURFHANDLE)lua_touserdata(L, 2);
	bool ret = oapiBltPanelAreaBackground(area_id, surf);
	lua_pushboolean(L, ret);
	return 1;
}

/***
Define the neighbour panels of the current panel.

These are the panels the user can switch to via Ctrl-Arrow keys.

This function should be called during panel registration (in
clbk_loadpanel2d()) to define the neighbours of the registered panel.

Every panel (except panel 0) must be listed as a neighbour by at least one
other panel, otherwise it is inaccessible.

@function set_panelneighbours
@tparam number left panel id of left neighbour (or -1 if none)
@tparam number right panel id of right neighbour (or -1 if none)
@tparam number top panel id of top neighbour (or -1 if none)
@tparam number bottom panel id of bottom neighbour (or -1 if none)
*/
int Interpreter::oapi_set_panelneighbours(lua_State* L)
{
	int left   = luaL_checkinteger(L, 1);
	int right  = luaL_checkinteger(L, 2);
	int top    = luaL_checkinteger(L, 3);
	int bottom = luaL_checkinteger(L, 4);
	oapiSetPanelNeighbours(left, right, top, bottom);
	return 0;
}

/***
Mesh handling.
@section meshes
*/

/***
Retrieve a mesh handle from the global mesh manager. 

When called for the first time for any given file name, the mesh is loaded from file 
and stored as a system resource. Every further request for the same mesh directly returns
a handle to the stored mesh without additional file I/O.

Once a mesh is globally loaded it remains in memory until the user closes
the simulation window.

This function can be used to pre-load meshes to avoid load delays during
the simulation. For example, parent objects may pre-load meshes for any
child objects they may create later.

Do NOT delete any meshes obtained by this function with oapi.delete_mesh()
Orbiter takes care of deleting globally managed meshes.

If you assign the mesh to a vessel with a subsequent vessel:add_mesh()
call, a copy of the global mesh is created every time the vessel creates its
visual, and discarded as soon as the visual is deleted. The global mesh can
therefore be regarded as a template from which individual vessel instances
make copies whenever they need to initialise their visual representation.
Handles for the individual mesh copies can be obtained within the
clbk_visualcreated() callback function, using the
vessel:get_devmesh() method.
Vessels should only modify their individual
meshes, never the global template, since the latter is shared across all vessel instances.

For external graphics clients, the Orbiter core forwards the mesh data
to the client for conversion to a device-specific format. The mesh template
referred to by the handle returned by oapi.load_meshglobal is then no longer
used, so any changes made to it will be ignored.

@function load_meshglobal
@tparam string fname mesh file name
@treturn handle mesh handle
*/
int Interpreter::oapi_load_meshglobal(lua_State* L)
{
	ASSERT_STRING(L, 1);
	const char* fname = lua_tostring(L, 1);
	MESHHANDLE hMesh = oapiLoadMeshGlobal(fname);
	if (hMesh) {
		lua_pushmeshhandle(L, hMesh);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/***
Remove a mesh from memory.

@function delete_mesh
@tparam handle hMesh mesh handle
*/
int Interpreter::oapi_delete_mesh(lua_State* L)
{
	MESHHANDLE hMesh = lua_tomeshhandle(L, 1);
	oapiDeleteMesh(hMesh);
	return 0;
}

/***
Get group specification of a mesh group.

This method can be used to edit a mesh group directly (for geometry
animation, texture animation, etc.)

This function should only be applied to device-independent meshes,
such as mesh templates.

For device-dependent mesh instances (such as returned by
vessel:get_devmesh()) use oapi.edit_meshgroup instead.

@function mesh_group
@tparam handle hMesh mesh handle
@tparam number idx group index (>=0)
@treturn table mesh group specification (or nil if idx out of range) :

- Vtx: ntvertexarray (vertex list)
- Idx: indexarray (index list)
- MtrlIdx: number (material index (>= 1, 0=none))
- TexIdx: number (texture index (>= 1, 0=none))
- UsrFlag: number (user-defined flag)
- zBias: number (z bias)
- Flags: number (internal flags)
*/
int Interpreter::oapi_mesh_group(lua_State* L)
{
	MESHHANDLE hMesh = lua_tomeshhandle(L, 1);
	int idx = lua_tointeger(L, 2);

	MESHGROUP *mg = oapiMeshGroup(hMesh, idx);
	if (mg) {
		lua_newtable (L);
		push_ntvertexarray(L, mg->Vtx, mg->nVtx);
		lua_setfield (L, -2, "Vtx");
		push_indexarray(L, mg->Idx, mg->nIdx);
		lua_setfield (L, -2, "Idx");
		lua_pushnumber (L, mg->MtrlIdx);
		lua_setfield (L, -2, "MtrlIdx");
		lua_pushnumber (L, mg->TexIdx);
		lua_setfield (L, -2, "TexIdx");
		lua_pushnumber (L, mg->UsrFlag);
		lua_setfield (L, -2, "UsrFlag");
		lua_pushnumber (L, mg->zBias);
		lua_setfield (L, -2, "zBias");
		lua_pushnumber (L, mg->Flags);
		lua_setfield (L, -2, "Flags");
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/***
Create a new mesh from a list of mesh group definitions.

@function create_mesh
@tparam {table,...} grps list of mesh groups
@treturn handle newly created mesh.
@usage
	local grp = {}
	grp.Vtx = vtx
	grp.Idx = idx
	hmesh = oapi.create_mesh({grp})
*/
int Interpreter::oapi_create_mesh(lua_State *L)
{
	int nGrp = lua_rawlen(L, 1);
	MESHGROUP *grp = new MESHGROUP[nGrp];

	lua_pushnil(L);
	int i = 0;
	while (lua_next(L, 1) != 0) {
		MESHGROUP *g = &grp[i];
		memset(g, 0, sizeof(*g));
		i++;
		lua_getfield(L, -1, "Vtx");
		if(!lua_isnil(L, -1)) {
			ntv_data* inst = (ntv_data*)luaL_checkudata(L, lua_gettop(L), "NTV.vtable");
			g->Vtx = inst->vtx;
			g->nVtx = inst->nVtxUsed;
		}
		lua_pop(L, 1);
		lua_getfield(L, -1, "Idx");
		if(!lua_isnil(L, -1)) {
			index_data* inst = (index_data*)luaL_checkudata(L, lua_gettop(L), "Index.vtable");
			g->Idx = inst->idx;
			g->nIdx = inst->nIdxUsed;
		}
		lua_pop(L, 2);
	}
	lua_pop(L, 1);

	MESHHANDLE hMesh = oapiCreateMesh(nGrp, grp);
	delete []grp;
	lua_pushmeshhandle(L, hMesh);
	return 1;
}

/***
Add geometry (vertices and indices) to an existing mesh group.
When adding indices to the group, index offsets are added automatically.

@function add_meshgroupblock
@tparam handle hMesh mesh handle
@tparam number idx group index (>=0)
@tparam ntvertexarray vtx vertex buffer
@tparam indexarray idx index buffer
@treturn boolean false if the mesh does no contain the group specified, true otherwise.
*/
int Interpreter::oapi_add_meshgroupblock(lua_State* L)
{
	MESHHANDLE hMesh = lua_tomeshhandle(L, 1);
	int grpidx = luaL_checkinteger(L, 2);
	ntv_data *ntv = (ntv_data *)luaL_checkudata(L, 3, "NTV.vtable");
	index_data *idx = (index_data *)luaL_checkudata(L, 4, "Index.vtable");

	bool ret = oapiAddMeshGroupBlock(hMesh, grpidx, ntv->vtx, ntv->nVtxUsed, idx->idx, idx->nIdxUsed);
	lua_pushboolean(L, ret);
	return 1;
}

/***
Modify mesh group data.

This function allows to modify a mesh group, by replacing vertex data,
or group flags.

The ges table can contain the following fields :

- flags: number (flags, see GRP_EDIT)
- UsrFlag: number (Replacement for group UsrFlag entry)
- Vtx: ntvertexarray (Replacement for group vertices)
- nVtx: number (Number of vertices to be replaced. Optional, will use the ntvertexarray size by default)
- vIdx: indexarray (Index list for vertices to be replaced)

@function edit_meshgroup
@tparam handle hMesh mesh handle
@tparam number grpidx mesh group index (>= 0)
@tparam table ges replacement/modification data for the group
@treturn number 0 on success, or error code
*/
int Interpreter::oapi_edit_meshgroup(lua_State* L)
{
	DWORD grpidx = luaL_checkinteger(L, 2);
	GROUPEDITSPEC ges;
	memset(&ges, 0, sizeof(ges));

	lua_getfield (L, 3, "flags");
	if(lua_isnil(L, -1)) {
		luaL_error(L, "Missing flags member in GROUPEDITSPEC");
	}
	ges.flags = luaL_checkinteger(L, -1);  lua_pop (L, 1);
	lua_getfield (L, 3, "UsrFlag");
	if(!lua_isnil(L, -1)) {
		ges.UsrFlag = luaL_checkinteger(L, -1);
	}
	lua_pop (L, 1);
	lua_getfield (L, 3, "Vtx");
	if(!lua_isnil(L, -1)) {
		ntv_data* inst = (ntv_data*)luaL_checkudata(L, lua_gettop(L), "NTV.vtable");
		ges.Vtx = inst->vtx;
		ges.nVtx = inst->nVtxUsed;
		lua_getfield (L, 3, "nVtx");
		if(!lua_isnil(L, -1)) {
			ges.nVtx = luaL_checkinteger(L, -1);
			if(ges.nVtx > inst->nVtxUsed) {
				luaL_error(L, "nVtx to big for current ntvertexarray");
			}
		}
		lua_pop (L, 1);
	}
	lua_pop (L, 1);
	lua_getfield (L, 3, "vIdx");
	if(!lua_isnil(L, -1)) {
		index_data* inst = (index_data*)luaL_checkudata(L, lua_gettop(L), "Index.vtable");
		ges.vIdx = inst->idx;
	}
	lua_pop (L, 1);


	MESHHANDLE *hMesh = (MESHHANDLE *)luaL_tryudata(L, 1, "MESHHANDLE");
	if(hMesh) {
		int ret = oapiEditMeshGroup(*hMesh, grpidx, &ges);
		lua_pushinteger(L, ret);
		return 1;
	}
	DEVMESHHANDLE hDevMesh = lua_todevmeshhandle(L, 1);
	int ret = oapiEditMeshGroup(hDevMesh, grpidx, &ges);
	lua_pushinteger(L, ret);

	return 1;
}

/***
Retrieve mesh group data.

The ges table can contain the following fields :

- Vtx: ntvertexarray
- nVtx: number
- Idx: indexarray
- nIdx: number
- VtxPerm: indexarray
- IdxPerm: indexarray


Vtx must be allocated by the caller with oapi.create_ntvertexarray() to sufficient size.

Idx, VtxPerm and IdxPerm must be allocated with oapi.create_indexarray().

nVtx and nIdx can by used to override the buffer sizes if you don't want to fill them completely.

If vertex data should be returned, Vtx must be allocated to at least the maximum number of
vertices to return. If the group contains fewer vertices, the Vtx buffer is only partially filled,
and its used size is set to the actual number of returned vertices.
If the group contains more vertices, and VtxPerm is nil, the buffer is filled to its capacity.

If an arbitrary subset of vertices should be returned, assign the VtxPerm buffer,
and fill it with the indices of the vertices you want returned.
The order of vertices returned in Vtx will correspond to VtxPerm. If VtxPerm 
contains any indices outside the valid range, the corresponding entries in Vtx will
be filled with {0} vertices, and the function will return 2.

If no vertex data are requested, set Vtx to nil and/or nVtx to 0.

Similar for triangle index data: If index data should be returned, allocate Idx to
the appropriate size.

If you want indices returned from the beginning, set IdxPerm to nil. Otherwise,
allocate IdxPerm and fill it with the requested triangle indices.

The MtrlIdx and TexIdx entries are always returned.

oapi.get\_meshgroup can be an expensive operation. It involves data copying, and
Graphics clients may have to retrieve data from video memory. Avoid continuous
oapi.get_meshgroup cycles and instead keep the data stored in your own
buffers once retrieved.
 
@function get_meshgroup
@tparam handle hMesh mesh handle
@tparam number idx group index (>=0)
@tparam table grs data buffers
@treturn number 0 on success, or error code :

- -1: no graphics client attached
- -2: graphics client hasn't implemented this function
-  1: grpidx is out of bounds
-  2: some indices in VtxPerm or IdxPerm were out of bounds (but data are still returned for the rest)

*/
int Interpreter::oapi_get_meshgroup(lua_State* L)
{
	DEVMESHHANDLE hDevMesh = lua_todevmeshhandle(L, 1);
	DWORD grpidx = luaL_checkinteger(L, 2);
	GROUPREQUESTSPEC grs;
	memset(&grs, 0, sizeof(grs));
	ntv_data* Vtx = NULL;
	index_data* Idx = NULL;

	lua_getfield (L, 3, "Vtx");
	if(!lua_isnil(L, -1)) {
		ntv_data* inst = (ntv_data*)luaL_checkudata(L, lua_gettop(L), "NTV.vtable");
		Vtx = inst;
		grs.Vtx = inst->vtx;
		grs.nVtx = inst->nVtx;
		lua_getfield (L, 3, "nVtx");
		if(!lua_isnil(L, -1)) {
			grs.nVtx = luaL_checkinteger(L, -1);
			if(grs.nVtx > inst->nVtx) {
				luaL_error(L, "nVtx to big for current ntvertexarray");
			}
		}
		lua_pop (L, 1);
	}
	lua_pop (L, 1);
	lua_getfield (L, 3, "VtxPerm");
	if(!lua_isnil(L, -1)) {
		index_data* inst = (index_data*)luaL_checkudata(L, lua_gettop(L), "Index.vtable");
		grs.VtxPerm = inst->idx;
	}
	lua_pop (L, 1);
	lua_getfield (L, 3, "Idx");
	if(!lua_isnil(L, -1)) {
		index_data* inst = (index_data*)luaL_checkudata(L, lua_gettop(L), "Index.vtable");
		Idx = inst;
		grs.Idx = inst->idx;
		grs.nIdx = inst->nIdx;
		lua_getfield (L, 3, "nIdx");
		if(!lua_isnil(L, -1)) {
			grs.nIdx = luaL_checkinteger(L, -1);
			if(grs.nIdx > inst->nIdx) {
				luaL_error(L, "nIdx to big for current indexarray");
			}
		}
		lua_pop (L, 1);
	}
	lua_pop (L, 1);
	lua_getfield (L, 3, "IdxPerm");
	if(!lua_isnil(L, -1)) {
		index_data* inst = (index_data*)luaL_checkudata(L, lua_gettop(L), "Index.vtable");
		grs.IdxPerm = inst->idx;
	}
	lua_pop (L, 1);

	int ret = oapiGetMeshGroup(hDevMesh, grpidx, &grs);
	if(Idx)
		Idx->nIdxUsed = grs.nIdx;
	if(Vtx)
		Vtx->nVtxUsed = grs.nVtx;

	lua_pushinteger(L, grs.MtrlIdx);
	lua_setfield(L, 3, "MtrlIdx");
	lua_pushinteger(L, grs.TexIdx);
	lua_setfield(L, 3, "TexIdx");

	lua_pushinteger(L, ret);

	return 1;
}


// ============================================================================
// terminal library functions

int Interpreter::termOut (lua_State *L)
{
	return 0;
}

int Interpreter::termClear (lua_State *L)
{
	return 0;
}

/***
Screen annotation library functions
@classmod annotation
@see oapi.create_annotation
*/

/***
Write a new annotation to screen, or overwrite the previous text.

If note is nil, the annotation text is cleared.

@function set_text
@tparam string|nil note annotation text
*/
int Interpreter::noteSetText (lua_State *L)
{
	NOTEHANDLE *pnote = (NOTEHANDLE*)lua_touserdata (L, 1);
	char *str = nullptr;
	if(!lua_isnil(L, 2)) {
		str = const_cast<char *>(lua_tostringex (L, 2));
	}
	oapiAnnotationSetText (*pnote, str);
	return 0;
}

/***
Set the bounding box of the annotation display area.

boundary values are specified in units of the render window area, with (0,0)
being the top left corner, and (1,1) the bottom right corner.

If the bounding box is set too small, part of the annotation may not be
visible.

@function set_pos
@tparam number x1 left edge of bounding box (0 <= x1 < x2)
@tparam number y1 top edge of bounding box (0 <= y1 < y2)
@tparam number x2 right edge of bounding box (x1 < x2 <= 1)
@tparam number y2 bottom edge of bounding box (y1 < y2 <= 1)
*/
int Interpreter::noteSetPos (lua_State *L)
{
	NOTEHANDLE *pnote = (NOTEHANDLE*)lua_touserdata (L, 1);
	double x1 = lua_tonumber (L, 2);
	double y1 = lua_tonumber (L, 3);
	double x2 = lua_tonumber (L, 4);
	double y2 = lua_tonumber (L, 5);
	oapiAnnotationSetPos (*pnote, x1, y1, x2, y2);
	return 0;
}

/***
Set the font size of the annotation text.

Annotations are sized in relation to the simulation window size. Size 1 is
the default annotation size.

@function set_size
@tparam number size font size in relative units (> 0)
*/
int Interpreter::noteSetSize (lua_State *L)
{
	NOTEHANDLE *pnote = (NOTEHANDLE*)lua_touserdata (L, 1);
	double size = lua_tonumber (L, 2);
	oapiAnnotationSetSize (*pnote, size);
	return 0;
}

/***
Set the font colour of the annotation text.

col must be a table with r, g and b number fields in the range [0-1].

@function set_colour
@tparam table col annotation colour
*/
int Interpreter::noteSetColour (lua_State *L)
{
	NOTEHANDLE *pnote = (NOTEHANDLE*)lua_touserdata (L, 1);
	VECTOR3 col;
	lua_getfield (L, 2, "r");
	col.x = lua_tonumber (L, -1);  lua_pop (L, 1);
	lua_getfield (L, 2, "g");
	col.y = lua_tonumber (L, -1);  lua_pop (L, 1);
	lua_getfield (L, 2, "b");
	col.z = lua_tonumber (L, -1);  lua_pop (L, 1);
	oapiAnnotationSetColour (*pnote, col);
	return 0;
}


RECT Interpreter::lua_torect(lua_State* L, int idx)
{
	RECT r;
	lua_getfield(L, idx, "left");
	r.left = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, idx, "top");
	r.top = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, idx, "right");
	r.right = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_getfield(L, idx, "bottom");
	r.bottom = lua_tointeger(L, -1); lua_pop(L, 1);
	return r;
}

// ============================================================================
// vessel library functions

OBJHANDLE Interpreter::lua_toObject (lua_State *L, int idx)
{
	return (OBJHANDLE)lua_touserdata (L, idx); 
}

oapi::Sketchpad *Interpreter::lua_tosketchpad (lua_State *L, int idx)
{
	oapi::Sketchpad **skp = (oapi::Sketchpad**)lua_touserdata (L, idx);
	return *skp;
	//oapi::Sketchpad *skp = (oapi::Sketchpad*)lua_touserdata(L,idx);
	//return skp;
}

/***
Vessel module
@module vessel
*/

/***
Return the handle of a vessel identified by its name or index.

@function get_handle
@tparam string|number key vessel name (case insensitive) or index
@treturn handle Vessel object handle, or nil if the vessel could not be found.
*/
int Interpreter::vesselGetHandle (lua_State *L)
{
	OBJHANDLE hObj;
	if (lua_isnumber (L, 1)) { // select by index
		int idx = (int)lua_tointeger (L, 1);
		hObj = oapiGetVesselByIndex (idx);
	} else {                   // select by name
		char *name = (char*)luaL_checkstring (L, 1);
		hObj = oapiGetVesselByName (name);
	}
	if (hObj) lua_pushlightuserdata (L, hObj);  // push vessel handle
	else lua_pushnil (L);
	return 1;
}

/***
Return the handle for the current focus object.

The focus object is the user-controlled vessel which receives keyboard and
joystick input.

@function get_focushandle
@treturn handle Focus object handle
*/
int Interpreter::vesselGetFocusHandle (lua_State *L)
{
	lua_pushlightuserdata (L, oapiGetFocusObject());
	return 1;
}

/***
Return a class instance for a vessel.

@function get_interface
@tparam string|number|handle key vessel name (case insensitive), index or handle
@treturn vessel vessel object or nil if vessel not found
*/
int Interpreter::vesselGetInterface (lua_State *L)
{
	OBJHANDLE hObj = 0;
	if (lua_islightuserdata (L, 1)) { // select by handle
		hObj = lua_toObject (L, 1);
	} else if (lua_isnumber (L, 1)) { // select by index
		int idx = (int)lua_tointeger (L, 1);
		hObj = oapiGetVesselByIndex (idx);
	} else if (lua_isstring(L, 1)) {  // select by name
		const char *name = lua_tostring (L, 1);
		if (name)
			hObj = oapiGetVesselByName ((char*)name);
	}
	if (hObj) {
		VESSEL *v = oapiGetVesselInterface(hObj);
		lua_pushvessel(L,v);
	} else {
		lua_pushnil (L);
	}
	return 1;
}

/***
Return a class instance for the current focus object.

@function get_focusinterface
@treturn vessel vessel object
*/
int Interpreter::vesselGetFocusInterface (lua_State *L)
{
	VESSEL *v = oapiGetFocusInterface();
	lua_pushvessel (L, v);
	return 1;
}

/***
Return the number of vessels currently present in the simulation.

@function get_count
@treturn number vessel count
*/
int Interpreter::vesselGetCount (lua_State *L)
{
	lua_pushinteger (L, oapiGetVesselCount());
	return 1;
}

/***
MFD class
@classmod mfd
*/

/***
Return the size of an MFD.

@function get_size
@treturn number MFD width
@treturn number MFD height
*/
int Interpreter::mfd_get_size(lua_State* L)
{
	MFD2* mfd = lua_tomfd(L, 1);
	ASSERT_SYNTAX(mfd, "Invalid MFD object");
	lua_pushnumber(L, mfd->GetWidth());
	lua_pushnumber(L, mfd->GetHeight());
	return 2;
}

/***
Set the title of an MFD.

@function set_title
@tparam sketchpad skp drawing context
@tparam string title MFD title
*/
int Interpreter::mfd_set_title (lua_State *L)
{
	MFD2 *mfd = lua_tomfd(L,1);
	ASSERT_SYNTAX(mfd, "Invalid MFD object");
	oapi::Sketchpad *skp = lua_tosketchpad (L,2);
	ASSERT_SYNTAX(skp, "Invalid Sketchpad object");
	ASSERT_MTDSTRING(L,3);
	const char *title = lua_tostring(L,3);
	mfd->Title (skp, title);
	return 0;
}

/***
Return a predefined MFD pen resource.

Valid colour indices are 0 to 4 :

- 0 : Main MFD colour (green)
- 1 : Auxiliary colour 1(yellow)
- 2 : Auxiliary colour 2(white)
- 3 : Auxiliary colour 3(red)
- 4 : Auxiliary colour 4(blue)

The default colours can be overridden by editing Config/MFD/default.cfg.

In principle, an MFD mode may create its own pen resources using the
oapi.create_pen function, but using predefined pens is
preferred to provide a consistent MFD look, and to avoid excessive allocation
of drawing resources.

@function get_defaultpen
@tparam number colidx pen colour index
@tparam number intens pen brightness (0=bright, 1=dark)
@tparam number style pen style (1=solid, 2=dashed)
@treturn handle pen resource
*/
int Interpreter::mfd_get_defaultpen (lua_State *L)
{
	MFD2 *mfd = lua_tomfd(L,1);
	ASSERT_SYNTAX(mfd, "Invalid MFD object");
	ASSERT_MTDNUMBER(L,2);
	DWORD intens = 0, style = 1, colidx = (DWORD)lua_tointeger(L,2);
	if (lua_gettop(L) >= 3) {
		ASSERT_MTDNUMBER(L,3);
		intens = (DWORD)lua_tointeger(L,3);
		if (lua_gettop(L) >= 4) {
			ASSERT_MTDNUMBER(L,4);
			style = lua_tointeger(L,4);
		}
	}
	oapi::Pen *pen = mfd->GetDefaultPen (colidx,intens,style);
	if (pen) lua_pushlightuserdata(L,pen);
	else     lua_pushnil(L);
	return 1;
}

/***
Return a predefined MFD font resource.

Currently supported are font indices 0-2 :

- 0 : standard MFD font (Courier, fixed pitch)
- 1 : small font (Arial, variable pitch)
- 2 : small font, rotated 90 degrees (Arial, variable pitch)

In principle, an MFD mode may create its own fonts using the oapi.create_font()
function, but using the predefined fonts is preferred to provide a consistent MFD look.

Default fonts are scaled automatically according to the MFD display size.

@function get_defaultfont
@tparam number fontidx font index
@treturn handle font resource
*/
int Interpreter::mfd_get_defaultfont (lua_State *L)
{
	MFD2 *mfd = lua_tomfd(L,1);
	ASSERT_SYNTAX(mfd, "Invalid MFD object");
	ASSERT_MTDNUMBER(L,2);
	DWORD fontidx = (DWORD)lua_tointeger(L,2);
	oapi::Font *font = mfd->GetDefaultFont (fontidx);
	if (font) lua_pushlightuserdata(L,font);
	else     lua_pushnil(L);
	return 1;
}

/***
Force a display update in the next frame.

This function causes Orbiter to call the
MFD's Update method in the next frame.

@function invalidate_display
*/
int Interpreter::mfd_invalidate_display (lua_State *L)
{
	MFD2 *mfd = lua_tomfd(L,1);
	ASSERT_SYNTAX(mfd, "Invalid MFD object");
	mfd->InvalidateDisplay();
	return 0;
}

/***
Force the MFD buttons to be redrawn. 

This is useful to alert Orbiter that the MFD mode has dynamically modified its button labels.

Orbiter will call the MFD buttonlabel method to retrieve the new button
labels. Therefore this must have been updated to return the new labels
before calling invalidate_buttons().

If the MFD is part of a 2-D panel view or 3-D virtual cockpit view, Orbiter
calls the clbk_MFDmode() method to allow the vessel to update its
button labels. If the MFD is one of the two glass cockpit MFD displays, the
buttons are updated internally.


@function invalidate_buttons
*/
int Interpreter::mfd_invalidate_buttons (lua_State *L)
{
	MFD2 *mfd = lua_tomfd(L,1);
	ASSERT_SYNTAX(mfd, "Invalid MFD object");
	mfd->InvalidateButtons();
	return 0;
}

/***
LightEmitter class
@classmod lightemitter
*/

/***
Return the current source position.

The source position is only relevant for point and spot lights. It is
ignored for directional lights

If the source is attached to an object, the returned
vector is the source position in local object coordinates. Otherwise, the
returned vector is the global source position.

@function get_position
@treturn vector source position [m]
*/
int Interpreter::le_get_position (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	VECTOR3 pos = le->GetPosition();
	lua_pushvector (L,pos);
	return 1;
}

/***
Set light source position.

The source position is only relevant for point and spot lights. It is
ignored for directional lights

If the source is attached to an object, the position is
interpreted in the local object coordinates. Otherwise, the position is
taken to be in global coordinates.

After a displacement of the vessel's centre of mass,
all light sources that define their position via set_position are
updated automatically.

@function set_position
@tparam vector pos new position [m] (in object or global coordinates)
*/
int Interpreter::le_set_position (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	ASSERT_MTDVECTOR(L,2);
	VECTOR3 pos = lua_tovector(L,2);
	le->SetPosition (pos);
	return 0;
}

/***
Return the light visibility mode.

@function get_visibility
@treturn number visibility mode
*/
int Interpreter::le_get_visibility (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	LightEmitter::VISIBILITY visibility = le->GetVisibility();
	lua_pushinteger (L,visibility);
	return 1;
}

/***
Set the light visibility mode

@function set_visibility
@tparam number vis visibility mode
*/
int Interpreter::le_set_visibility (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	LightEmitter::VISIBILITY visibility = (LightEmitter::VISIBILITY)luaL_checkinteger(L,2);
	le->SetVisibility (visibility);
	return 0;
}

/***
Return the current source direction.

The source direction is only relevant for spot and directional lights.
It is ignored for point lights.

If the source is attached to an object,  the returned
vector is the source direction in local object coordinates. Otherwise, the
returned vector is the global source direction.

@function get_direction
@treturn vector source direction.
*/
int Interpreter::le_get_direction (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	VECTOR3 dir = le->GetDirection();
	lua_pushvector (L,dir);
	return 1;
}

/***
Set light source direction.

The vector argument should be normalised to length 1.

The source direction is only relevant for spot and directional lights.
It is ignored for point lights.

If the source is attached to an object, the direction is
interpreted in the local object coordinates. Otherwise, the direction is
taken to be in global coordinates.

@function set_direction
@tparam vector p new direction (in object or global coordinates)
*/
int Interpreter::le_set_direction (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	ASSERT_MTDVECTOR(L,2);
	VECTOR3 dir = lua_tovector(L,2);
	le->SetDirection (dir);
	return 0;
}

/***
Return the light intensity.

@function get_intensity
@treturn number light intensity
*/
int Interpreter::le_get_intensity (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	double intens = le->GetIntensity();
	lua_pushnumber (L,intens);
	return 1;
}

/***
Set the light intensity.

@function set_intensity
@tparam number light intensity
*/
int Interpreter::le_set_intensity (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	ASSERT_MTDNUMBER(L,2);
	double intens = lua_tonumber(L,2);
	le->SetIntensity (intens);
	return 0;
}

/***
Return the light source range.

@function get_range
@treturn number light source range [m]
*/
int Interpreter::le_get_range (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	if (le->GetType() == LightEmitter::LT_POINT || le->GetType() == LightEmitter::LT_SPOT) {
		PointLight *point = (PointLight*)le;
		lua_pushnumber (L, point->GetRange());
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/***
Set the light source range.

When changing the range, the attenuation factors usually should be adjusted
accordingly, to avoid sharp cutoff edges or large areas of negligible intensity.

@function set_range
@tparam number range new light source range [m]
*/
int Interpreter::le_set_range (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	if (le->GetType() == LightEmitter::LT_POINT || le->GetType() == LightEmitter::LT_SPOT) {
		PointLight *point = (PointLight*)le;
		ASSERT_MTDNUMBER(L,2);
		double range = lua_tonumber(L,2);
		point->SetRange (range);
	}
	return 0;
}

/***
Return light attenuation coefficients.

The attenuation coefficients define the fractional light intensity I/I0 as
a function of distance d:

	I/I0 = att_0 + d * att_1 + d^2 * att_2

@function get_attenuation
@treturn number att0
@treturn number att1
@treturn number att2
*/
int Interpreter::le_get_attenuation (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	if (le->GetType() == LightEmitter::LT_POINT || le->GetType() == LightEmitter::LT_SPOT) {
		PointLight *point = (PointLight*)le;
		const double *att = point->GetAttenuation();
		lua_pushnumber (L,att[0]);
		lua_pushnumber (L,att[1]);
		lua_pushnumber (L,att[2]);
		return 3;
	} else {
		lua_pushnil(L);
		return 1;
	}
}

/***
Set light attenuation coefficients.

The attenuation coefficients define the fractional light intensity I/I0 as
a function of distance d:

	I/I0 = att_0 + d * att_1 + d^2 * att_2

@function set_attenuation
@tparam number att0 attenuation coefficient
@tparam number att1 attenuation coefficient
@tparam number att2 attenuation coefficient
*/
int Interpreter::le_set_attenuation (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	if (le->GetType() == LightEmitter::LT_POINT || le->GetType() == LightEmitter::LT_SPOT) {
		PointLight *point = (PointLight*)le;
		ASSERT_MTDNUMBER(L,2);
		ASSERT_MTDNUMBER(L,3);
		ASSERT_MTDNUMBER(L,4);
		double att0 = lua_tonumber(L,2);
		double att1 = lua_tonumber(L,3);
		double att2 = lua_tonumber(L,4);
		point->SetAttenuation (att0, att1, att2);
	}
	return 0;
}

/***
Returns the cone geometry of a spotlight source.

@function get_spotaperture
@treturn number aperture of inner spotlight (maximum intensity) cone [rad]
@treturn number angular aperture of outer (zero intensity) cone [rad]
@usage umbra,penumbra = le:get_spotaperture()
*/
int Interpreter::le_get_spotaperture (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	if (le->GetType() == LightEmitter::LT_SPOT) {
		SpotLight *spot = (SpotLight*)le;
		lua_pushnumber(L,spot->GetUmbra());
		lua_pushnumber(L,spot->GetPenumbra());
		return 2;
	} else {
		lua_pushnil(L);
		return 1;
	}
}

/***
Set the cone geometry of a spotlight source.

@function set_spotaperture
@tparam number aperture of inner spotlight (maximum intensity) cone [rad]
@tparam number angular aperture of outer (zero intensity) cone [rad]
@usage le:set_spotaperture(umbra,penumbra)
*/
int Interpreter::le_set_spotaperture (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	if (le->GetType() == LightEmitter::LT_SPOT) {
		SpotLight *spot = (SpotLight*)le;
		ASSERT_MTDNUMBER(L,2);
		ASSERT_MTDNUMBER(L,3);
		double umbra = lua_tonumber(L,2);
		double penumbra = lua_tonumber(L,3);
		spot->SetAperture (umbra, penumbra);
	}
	return 0;
}

/***
Activate or deactivate the light source

@function activate
@tparam boolean act if true, activates the light source. Otherwise, deactivates the light source
*/
int Interpreter::le_activate (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	ASSERT_MTDBOOLEAN(L,2);
	int activate = lua_toboolean(L,2);
	le->Activate (activate != 0);
	return 0;
}

/***
@function is_active
@treturn boolean true if source is active, false otherwise.
*/
int Interpreter::le_is_active (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	bool active = le->IsActive();
	lua_pushboolean (L,active);
	return 1;
}

/***
Sketchpad class: Lua access to Sketchpad objects.
@classmod sketchpad
*/

/***
Draw a text string.

@function text
@tparam number x reference x position [pixel]
@tparam number y reference y position [pixel]
@tparam string str text string
@tparam[opt] number size string length for output
@treturn \e true on success, \e false on failure.
*/
int Interpreter::skp_text (lua_State *L)
{
	int x, y, len;
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	ASSERT_MTDNUMBER(L,2);
	x = (int)lua_tointeger(L,2);
	ASSERT_MTDNUMBER(L,3);
	y = (int)lua_tointeger(L,3);
	ASSERT_MTDSTRING(L,4);
	const char *str = lua_tostring(L,4);
	if (lua_gettop(L) == 5) {
		ASSERT_MTDNUMBER(L, 5);
		len = (int)lua_tointeger(L, 5);
	} else {
		len = strlen(str);
	}
	bool ok = skp->Text (x, y, str, len);
	lua_pushboolean (L, ok ? 1:0);
	return 1;
}

/***
Move the drawing reference to a new point.

Some methods use the drawing reference point for
drawing operations.

@function moveto
@tparam number x x-coordinate of new reference point [pixel]
@tparam number y y-coordinate of new reference point [pixel]
*/
int Interpreter::skp_moveto (lua_State *L)
{
	int x, y;
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	ASSERT_MTDNUMBER(L,2);
	x = (int)lua_tointeger(L,2);
	ASSERT_MTDNUMBER(L,3);
	y = (int)lua_tointeger(L,3);
	skp->MoveTo (x, y);
	return 0;
}

/***
Draw a line to a specified point.

The line starts at the current drawing reference point.

@function lineto
@tparam number x x-coordinate of line end point [pixel]
@tparam number y y-coordinate of line end point [pixel]
*/
int Interpreter::skp_lineto (lua_State *L)
{
	int x, y;
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	ASSERT_MTDNUMBER(L,2);
	x = (int)lua_tointeger(L,2);
	ASSERT_MTDNUMBER(L,3);
	y = (int)lua_tointeger(L,3);
	skp->LineTo (x, y);
	return 0;
}

/***
Draw a line between two points.

The line is drawn with the currently selected pen.

@function line
@tparam number x0 x-coordinate of first point [pixel]
@tparam number y0 y-coordinate of first point [pixel]
@tparam number x1 x-coordinate of second point [pixel]
@tparam number y1 y-coordinate of second point [pixel]
*/
int Interpreter::skp_line (lua_State *L)
{
	int x0, y0, x1, y1;
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	ASSERT_MTDNUMBER(L,2);
	x0 = (int)lua_tointeger(L,2);
	ASSERT_MTDNUMBER(L,3);
	y0 = (int)lua_tointeger(L,3);
	ASSERT_MTDNUMBER(L,4);
	x1 = (int)lua_tointeger(L,4);
	ASSERT_MTDNUMBER(L,5);
	y1 = (int)lua_tointeger(L,5);
	skp->Line (x0, y0, x1, y1);
	return 0;
}

/***
Draw a rectangle (filled or outline).

Draws the rectangle from 4 line segments by
calling moveto and lineto.

Implementations should fill the rectangle with the
currently selected brush resource.

@function rectangle
@tparam number x0 left edge of rectangle [pixel]
@tparam number y0 top edge of rectangle [pixel]
@tparam number x1 right edge of rectangle [pixel]
@tparam number y1 bottom edge of rectangle [pixel]
*/
int Interpreter::skp_rectangle (lua_State *L)
{
	int x0, y0, x1, y1;
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	ASSERT_MTDNUMBER(L,2);
	x0 = (int)lua_tointeger(L,2);
	ASSERT_MTDNUMBER(L,3);
	y0 = (int)lua_tointeger(L,3);
	ASSERT_MTDNUMBER(L,4);
	x1 = (int)lua_tointeger(L,4);
	ASSERT_MTDNUMBER(L,5);
	y1 = (int)lua_tointeger(L,5);
	skp->Rectangle (x0, y0, x1, y1);
	return 0;
}

/***
Draw an ellipse from its bounding box.

Implementations should fill the ellipse with the
currently selected brush resource.

@function ellipse
@tparam number x0 left edge of bounding box [pixel]
@tparam number y0 y0 top edge of bounding box [pixel]
@tparam number x1 right edge of bounding box [pixel]
@tparam number y1 bottom edge of bounding box [pixel]
*/
int Interpreter::skp_ellipse (lua_State *L)
{
	int x0, y0, x1, y1;
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	ASSERT_MTDNUMBER(L,2);
	x0 = (int)lua_tointeger(L,2);
	ASSERT_MTDNUMBER(L,3);
	y0 = (int)lua_tointeger(L,3);
	ASSERT_MTDNUMBER(L,4);
	x1 = (int)lua_tointeger(L,4);
	ASSERT_MTDNUMBER(L,5);
	y1 = (int)lua_tointeger(L,5);
	skp->Ellipse (x0, y0, x1, y1);
	return 0;
}

/***
Draw a closed polygon given by vertex points.

The polygon is closed by connecting the last and first vertices.

The polygon outline is drawn with the current pen and filled with
the current brush.

Each vertex in the pt table is represented by a sub-table
containing the x and y integer coordinates as unnamed fields.
 
@function polygon
@tparam table pt list of 2-D integer vertices [pixel]
@usage skp:polygon({{1,2},{4,7},{-3,2}})
*/
int Interpreter::skp_polygon (lua_State *L)
{
	oapi::IVECTOR2 *pt = 0;
	size_t npt = 0, nbuf = 0;
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	ASSERT_MTDTABLE(L,2);
	lua_pushnil(L);
	while(lua_next(L,2)) {
		ASSERT_TABLE(L,-1);
		if (npt == nbuf) { // grow buffer
			oapi::IVECTOR2 *tmp = new oapi::IVECTOR2[nbuf+=32];
			if (npt) {
				memcpy (tmp, pt, npt*sizeof(oapi::IVECTOR2));
				delete []pt;
			}
			pt = tmp;
		}
		lua_pushnil(L);
		for (auto i = 0; i < 2; i++) {
			ASSERT_SYNTAX(lua_next(L,-2),"Inconsistent vertex array");
			pt[npt].data[i] = (long)lua_tointeger(L,-1);
			lua_pop(L,1);
		}
		npt++;
		lua_pop(L,2); // pop last key and table
	}
	if (npt) {
		skp->Polygon (pt, npt);
		delete []pt;
	}
	return 0;
}

/***
Draw a line of piecewise straight segments.

The polyline is drawn with the current pen.

Each vertex in the pt table is represented by a sub-table
containing the x and y integer coordinates as unnamed fields.
 
@function polyline
@tparam table pt list of 2-D integer vertices [pixel]
@usage skp:polyline({{1,2},{4,7},{-3,2}})
*/
int Interpreter::skp_polyline (lua_State *L)
{
	oapi::IVECTOR2 *pt = 0;
	size_t npt = 0, nbuf = 0;
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	ASSERT_MTDTABLE(L,2);
	lua_pushnil(L);
	while(lua_next(L,2)) {
		ASSERT_TABLE(L,-1);
		if (npt == nbuf) { // grow buffer
			oapi::IVECTOR2 *tmp = new oapi::IVECTOR2[nbuf+=32];
			if (npt) {
				memcpy (tmp, pt, npt*sizeof(oapi::IVECTOR2));
				delete []pt;
			}
			pt = tmp;
		}
		lua_pushnil(L);
		for (auto i = 0; i < 2; i++) {
			ASSERT_SYNTAX(lua_next(L,-2),"Inconsistent vertex array");
			pt[npt].data[i] = (long)lua_tointeger(L,-1);
			lua_pop(L,1);
		}
		npt++;
		lua_pop(L,2); // pop last key and table
	}
	if (npt) {
		skp->Polyline (pt, npt);
		delete []pt;
	}
	return 0;
}

/***
Set surface reference point.
 
Set the position in the surface which is mapped to the
origin of the coordinate system for all drawing functions.

By default, the reference point for drawing function coordinates is
the top left corner of the bitmap, with positive x-axis to the right,
and positive y-axis down.

set_origin can be used to shift the logical reference point to a
different position in the surface (but not to change the
orientation of the axes).

@function set_origin
@tparam number x horizontal position of the origin [pixel]
@tparam number y vertical position of the origin [pixel]
@usage skp:set_origin(x,y)
*/
int Interpreter::skp_set_origin (lua_State *L)
{
	int x, y;
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	ASSERT_MTDNUMBER(L,2);
	x = (int)lua_tointeger(L,2);
	ASSERT_MTDNUMBER(L,3);
	y = (int)lua_tointeger(L,3);
	skp->SetOrigin (x, y);
	return 0;
}

/***
Set horizontal and vertical text alignment.
 
@function set_textalign
@tparam number tah horizontal alignment (SKP.LEFT, SKP.CENTER, SKP.RIGHT)
@tparam[opt=SKP.TOP] number tav vertical alignment (SKP.TOP, SKP.BASELINE, SKP.BOTTOM)
*/
int Interpreter::skp_set_textalign (lua_State *L)
{
	oapi::Sketchpad::TAlign_horizontal tah = oapi::Sketchpad::LEFT;
	oapi::Sketchpad::TAlign_vertical   tav = oapi::Sketchpad::TOP;
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	ASSERT_MTDNUMBER(L,2);
	tah = (oapi::Sketchpad::TAlign_horizontal)lua_tointeger(L,2);
	if (lua_gettop(L) >= 3) {
		ASSERT_MTDNUMBER(L,3);
		tav = (oapi::Sketchpad::TAlign_vertical)lua_tointeger(L,3);
	}
	skp->SetTextAlign (tah, tav);
	return 0;
}

/***
Set the foreground colour for text output.

To set a colour with given R (red), G (green) and B (blue) components (each
in the range from 0 to 255), use
	col = B*65536 + G*256 + R

@function set_textcolor
@tparam number col colour description (format: 0xBBGGRR)
@treturn number previous colour setting.
*/
int Interpreter::skp_set_textcolor (lua_State *L)
{
	DWORD col, pcol;
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	ASSERT_MTDNUMBER(L,2);
	col = (DWORD)lua_tointeger(L,2);
	pcol = skp->SetTextColor(col);
	lua_pushnumber (L, pcol);
	return 1;
}

/***
Set the background colour for text output.

To set a colour with given R (red), G (green) and B (blue) components (each
in the range from 0 to 255), use
	col = B*65536 + G*256 + R

@function set_backgroundcolor
@tparam number col background colour description (format: 0xBBGGRR)
@treturn number previous colour setting.
*/
int Interpreter::skp_set_backgroundcolor (lua_State *L)
{
	DWORD col, pcol;
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	ASSERT_MTDNUMBER(L,2);
	col = (DWORD)lua_tointeger(L,2);
	pcol = skp->SetBackgroundColor(col);
	lua_pushnumber (L, pcol);
	return 1;
}

/***
Set the background mode for text and drawing operations.

This function affects text output and dashed line drawing.

In opaque background mode, text background and the gaps
between dashed lines are drawn in the current background colour

In transparent mode, text background and line gaps are not modified.

The default background mode (before the first call of
set_backgroundmode) should be transparent.

@function set_backgroundmode
@tparam number mode background mode (SKP.OPAQUE or SKP.TRANSPARENT)
*/
int Interpreter::skp_set_backgroundmode (lua_State *L)
{
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	ASSERT_MTDNUMBER(L,2);
	oapi::Sketchpad::BkgMode mode = (oapi::Sketchpad::BkgMode)lua_tointeger(L, 2);
	skp->SetBackgroundMode (mode);
	return 0;
}

/***
Select a new pen to use.

@function set_pen
@tparam handle pen pen resource handle, or nil to disable outlines
@treturn handle previously selected pen.
@usage ppen = skp:set_pen(pen)
*/
int Interpreter::skp_set_pen(lua_State* L)
{
	oapi::Sketchpad* skp = lua_tosketchpad(L, 1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	oapi::Pen* pen = NULL;
	
	if (!lua_isnil(L, 2)) {
		ASSERT_MTDLIGHTUSERDATA(L, 2);
		pen = (oapi::Pen*)lua_touserdata(L, 2);
	}
	oapi::Pen* ppen = skp->SetPen(pen);
	if (ppen) lua_pushlightuserdata(L, ppen);
	else      lua_pushnil(L);
	return 1;
}

/***
Select a new brush to use.

@function set_brush
@tparam handle brush brush resource handle, or nil to disable fill mode
@treturn handle previously selected brush.
@usage pbrush = skp:set_brush(brush)
*/
int Interpreter::skp_set_brush(lua_State* L)
{
	oapi::Sketchpad* skp = lua_tosketchpad(L, 1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	oapi::Brush* brush = NULL;
	
	if (!lua_isnil(L, 2)) {
		ASSERT_MTDLIGHTUSERDATA(L, 2);
		brush = (oapi::Brush*)lua_touserdata(L, 2);
	}

	oapi::Brush* pbrush = skp->SetBrush(brush);
	if (pbrush) lua_pushlightuserdata(L, pbrush);
	else      lua_pushnil(L);
	return 1;
}

/***
Select a new font to use.

@function set_font
@tparam handle font font resource
@treturn handle previously selected font.
*/
int Interpreter::skp_set_font (lua_State *L)
{
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	ASSERT_MTDLIGHTUSERDATA(L,2);
	oapi::Font *font = (oapi::Font*)lua_touserdata(L,2);
	oapi::Font *pfont = skp->SetFont (font);
	if (pfont) lua_pushlightuserdata(L,pfont);
	else       lua_pushnil(L);
	return 1;
}

/***
Return height and (average) width of a character in the currently selected font.

@function get_charsize
@treturn number height of character cell [pixel]
@treturn number (average) width of character cell [pixel]
*/
int Interpreter::skp_get_charsize (lua_State *L)
{
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	DWORD size = skp->GetCharSize ();
	lua_pushnumber(L, LOWORD(size));
	lua_pushnumber(L, HIWORD(size));
	return 2;
}

/***
Return the width of a text string in the currently selected font.

@function get_textwidth
@tparam string str text string
@treturn number string width when drawn in current font [pixel]
*/
int Interpreter::skp_get_textwidth (lua_State *L)
{
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	ASSERT_MTDSTRING(L,2);
	const char *str = lua_tostring(L,2);
	DWORD w = skp->GetTextWidth (str);
	lua_pushnumber (L,w);
	return 1;
}

/***
[DX9] Copy 'Blit' a rectangle.

Note : Can alpha-blend and mirror by a use of negative width/height in source rectangle

@function copy_rect
@tparam handle hSrc Source surface handle
@tparam table src Source rectangle, (or nil for whole surface)
@tparam number tx Target x-coordinate
@tparam number ty Target y-coordinate
*/
int Interpreter::skp_copy_rect (lua_State *L)
{
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	SURFHANDLE hSrc = (SURFHANDLE)lua_touserdata(L, 2);
	RECT *src = nullptr;
	RECT r;
	if(!lua_isnil(L, 3)) {
		r = lua_torect(L, 3);
		src = &r;
	}
	int tx = luaL_checkinteger(L, 4);
	int ty = luaL_checkinteger(L, 5);

	skp->CopyRect(hSrc, src, tx, ty);
	return 0;
}

/***
[DX9] Copy 'Blit' a rectangle

Note : Can alpha-blend and mirror by a use of negative width/height in source rectangle

@function stretch_rect
@tparam handle hSrc Source surface handle
@tparam table src Source rectangle, (or nil for whole surface)
@tparam table tgt Target rectangle, (or nil for whole surface)
*/
int Interpreter::skp_stretch_rect (lua_State *L)
{
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	SURFHANDLE hSrc = (SURFHANDLE)lua_touserdata(L, 2);
	RECT *src = nullptr;
	RECT r;
	if(!lua_isnil(L, 3)) {
		r = lua_torect(L, 3);
		src = &r;
	}

	RECT *tgt = nullptr;
	RECT t;
	if(!lua_isnil(L, 4)) {
		t = lua_torect(L, 4);
		tgt = &t;
	}

	skp->StretchRect(hSrc, src, tgt);
	return 0;
}

/***
[DX9] Copy 'Blit' a rectangle with rotation and scaling

Note : Can alpha-blend and mirror by a use of negative width/height in source rectangle

@function rotate_rect
@tparam handle hSrc Source surface handle
@tparam table src Source rectangle, (or nil for whole surface)
@tparam number cx Target center x-coordinate
@tparam number cy Target center y-coordinate
@tparam number angle Rotation angle in radians
@tparam number sw Width scale factor
@tparam number sh Height scale factor
*/
int Interpreter::skp_rotate_rect (lua_State *L)
{
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	SURFHANDLE hSrc = (SURFHANDLE)lua_touserdata(L, 2);
	RECT *src = nullptr;
	RECT r;
	if(!lua_isnil(L, 3)) {
		r = lua_torect(L, 3);
		src = &r;
	}
	int cx = luaL_checkinteger(L, 4);
	int cy = luaL_checkinteger(L, 5);
	float angle = 0.0f;
	float sw = 1.0f;
	float sh = 1.0f;

	if(lua_gettop(L)>=6)
		angle = luaL_checknumber(L, 6);
	if(lua_gettop(L)>=7)
		sw = luaL_checknumber(L, 7);
	if(lua_gettop(L)>=8)
		sh = luaL_checknumber(L, 8);


	skp->RotateRect(hSrc, src, cx, cy, angle, sw, sh);
	return 0;
}

/***
[DX9] Setup a quick pen, removes any other pen from use. Set to zero to disable a pen from use.

@function quick_pen
@tparam number color Pen color in 0xAABBGGRR
@tparam number width Pen width in pixels
@tparam number style 0 = Disabled, 1 = Solid, 2 = Dashed
*/
int Interpreter::skp_quick_pen (lua_State *L)
{
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	DWORD color = luaL_checkinteger(L, 2);
	float width = 1.0;
	if(lua_gettop(L)>=3)
		width = luaL_checknumber(L, 3);
	DWORD style = 1UL;
	if(lua_gettop(L)>=4)
		style = luaL_checkinteger(L, 4);
	skp->QuickPen(color, width, style);
	return 0;
}

/***
[DX9] Setup a quick brush, removes any other brush from use. Set to zero to disable a brush from use.

@function quick_brush
@tparam number color Brush color in 0xAABBGGRR
*/
int Interpreter::skp_quick_brush (lua_State *L)
{
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	DWORD color = luaL_checkinteger(L, 2);
	skp->QuickBrush(color);
	return 0;
}

/***
Returns the surface associated with the drawing object.

@function get_surface
@treturn handle Surface handle
*/
int Interpreter::skp_get_surface (lua_State *L)
{
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	SURFHANDLE surf = skp->GetSurface();
	if (surf)
		lua_pushlightuserdata(L, surf);
	else
		lua_pushnil(L);

	return 1;
}

/***
[DX9] Automatically set a ColorMatrix for brightness control. nil to restore default settings.

@function set_brightness
@tparam colour brightness value
*/
int Interpreter::skp_set_brightness (lua_State *L)
{
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	if(lua_gettop(L)>=2 && !lua_isnil(L, 2)) {
		COLOUR4 col = lua_torgba(L, 2);
		FVECTOR4 c = {col.r, col.g, col.b, col.a};
		skp->SetBrightness(&c);
	} else {
		skp->SetBrightness();
	}
	return 0;
}

/***
Set a render configuration paramater or "effect".

@function set_renderparam
@tparam number A setting ID to set [PRM.GAMMA or PRM.NOISE]
@tparam colour|nil setting value or nil to disable the effect
*/
int Interpreter::skp_set_renderparam (lua_State *L)
{
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");

	Sketchpad::RenderParam rp = (Sketchpad::RenderParam)luaL_checkinteger(L, 2);

	if(lua_gettop(L)>=3 && !lua_isnil(L, 3)) {
		COLOUR4 col = lua_torgba(L, 3);
		FVECTOR4 c = {col.r, col.g, col.b, col.a};
		skp->SetRenderParam(rp, &c);
	} else {
		skp->SetRenderParam(rp);
	}

	return 0;
}

/***
[DX9] Set up a global world transformation matrix.

Note : This function will conflict and resets any settings set by SetOrigin(). Setting to nil does not restore set_origin().

Note : Everything is transformed including copy_rect() and text().

Warning : Graphics results from a copy_rect() and text() can be blurry when non-default transform is in use
due to source-target pixels miss aligments.

@function set_worldtransform2d
@tparam number[opt=1] scale Graphics scale factor.
@tparam number[opt=1] rot Rotation angle [rad]
@tparam table[opt=nil] ctr table containing a rotation center (x,y fields) or nil for origin.
@tparam table[opt=nil] trl table containing a translation (x,y fields) or nil.
*/
int Interpreter::skp_set_worldtransform2d (lua_State *L)
{
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	float scale = 1.0f;
	float rot = 0.0f;
	IVECTOR2 ctr;
	IVECTOR2 *pctr = nullptr;
	IVECTOR2 trl;
	IVECTOR2 *ptrl = nullptr;

	if(lua_gettop(L)>=2) {
		scale = luaL_checknumber(L,2);
	}
	if(lua_gettop(L)>=3) {
		rot = luaL_checknumber(L,3);
	}
	if(lua_gettop(L)>=4 && !lua_isnil(L,4)) {
		lua_getfield (L, 4, "x");
		ctr.x = lua_tonumber (L, -1); lua_pop (L,1);
		lua_getfield (L, 4, "y");
		ctr.y = lua_tonumber (L, -1); lua_pop (L,1);
		pctr = &ctr;
	}

	if(lua_gettop(L)>=5 && !lua_isnil(L,5)) {
		lua_getfield (L, 5, "x");
		trl.x = lua_tonumber (L, -1); lua_pop (L,1);
		lua_getfield (L, 5, "y");
		trl.y = lua_tonumber (L, -1); lua_pop (L,1);
		ptrl = &trl;
	}

	skp->SetWorldTransform2D(scale, rot, pctr, ptrl);
	return 0;
}


/***
This type provides an encapsulation of C++ NTVERTEX buffers that
can be used to describe 3D meshes.

It can be accessed as a regular Lua table of vertices with the following fields :

- x: number (vertex x position)
- y: number (vertex y position)
- z: number (vertex z position)
- pos: vector (vertex position vector)
- nx: number (vertex x normal)
- ny: number (vertex y normal)
- nz: number (vertex z normal)
- normal: vector (vertex normal vector)
- tu: number (vertex u texture coordinate)
- tv: number (vertex v texture coordinate)

When accessing a vertex, you get a _reference like_ object that can be used to
manipulate the underlying NTVERTEX data :
	local vgear = oapi.create_ntvertexarray(12)
	for i=1,12 do
		local vtx = vgear[i]
		vtx.x = cx + x[i]*scl
		vtx.y = cy + y[i]*scl
		vtx.tu = (405.0 + (18.0 * ((i-1)%2)))/texw
		vtx.tv = (104.0 - (18.0 * math.floor(((i-1)%4)/2)))/texh
	end

An ntvertexarray is of fixed capacity, but it can contain a variable amount of vertices, up to the max capacity.
This can be used to concatenate vertex arrays :
	local vtx = oapi.create_ntvertexarray(12+16+4)
	vtx:reset()

	if gear_deployed then
		vtx:append(vgear)
	end

	if nosecone_open then
		vtx:append(vnose)
	end

	if airbrake_open then
		vtx:append(vbrk)
	end
	if #vtx > 0 then
		-- render vertices
	end

@classmod ntvertexarray
*/

// NTVERTEX proxy object
void Interpreter::ntvproxy_create(lua_State *L, NTVERTEX *vtx)
{
	NTVERTEX **proxy = (NTVERTEX **)lua_newuserdata(L, sizeof(NTVERTEX *));
    *proxy = vtx;
	luaL_getmetatable(L, "NTVPROXY.vtable");
	lua_setmetatable(L, -2);
}
int Interpreter::ntvproxy_get(lua_State *L)
{
	NTVERTEX ** vtx = (NTVERTEX **)luaL_checkudata(L, 1, "NTVPROXY.vtable");
    const char *member = luaL_checkstring(L, 2);
	if(!strcmp(member, "x"))
		lua_pushnumber(L, (*vtx)->x);
	else if(!strcmp(member, "y"))
		lua_pushnumber(L, (*vtx)->y);
	else if(!strcmp(member, "z"))
		lua_pushnumber(L, (*vtx)->z);
	else if(!strcmp(member, "pos")) {
		VECTOR3 pos;
		pos.x = (*vtx)->x;
		pos.y = (*vtx)->y;
		pos.z = (*vtx)->z;
		lua_pushvector(L, pos);
	} else if(!strcmp(member, "tu"))
		lua_pushnumber(L, (*vtx)->tu);
	else if(!strcmp(member, "tv"))
		lua_pushnumber(L, (*vtx)->tv);
	else if(!strcmp(member, "nx"))
		lua_pushnumber(L, (*vtx)->nx);
	else if(!strcmp(member, "ny"))
		lua_pushnumber(L, (*vtx)->ny);
	else if(!strcmp(member, "nz"))
		lua_pushnumber(L, (*vtx)->nz);
	else if(!strcmp(member, "normal")) {
		VECTOR3 normal;
		normal.x = (*vtx)->nx;
		normal.y = (*vtx)->ny;
		normal.z = (*vtx)->nz;
		lua_pushvector(L, normal);
	} else
		luaL_error(L, "Invalid member access for vertex: %s", member);

	return 1;
}
int Interpreter::ntvproxy_set(lua_State *L)
{
	NTVERTEX ** vtx = (NTVERTEX **)luaL_checkudata(L, 1, "NTVPROXY.vtable");
    const char *member = luaL_checkstring(L, 2);
	
	if(!strcmp(member, "x"))
		(*vtx)->x = luaL_checknumber(L, 3);
	else if(!strcmp(member, "y"))
		(*vtx)->y = luaL_checknumber(L, 3);
	else if(!strcmp(member, "z"))
		(*vtx)->z = luaL_checknumber(L, 3);
	else if(!strcmp(member, "pos")) {
		VECTOR3 pos = lua_tovector(L, 3);
		(*vtx)->x = pos.x;
		(*vtx)->y = pos.y;
		(*vtx)->z = pos.z;
	} else if(!strcmp(member, "tu"))
		(*vtx)->tu = luaL_checknumber(L, 3);
	else if(!strcmp(member, "tv"))
		(*vtx)->tv = luaL_checknumber(L, 3);
	else if(!strcmp(member, "nx"))
		(*vtx)->nx = luaL_checknumber(L, 3);
	else if(!strcmp(member, "ny"))
		(*vtx)->ny = luaL_checknumber(L, 3);
	else if(!strcmp(member, "nz"))
		(*vtx)->nz = luaL_checknumber(L, 3);
	else if(!strcmp(member, "normal")) {
		VECTOR3 normal = lua_tovector(L, 3);
		(*vtx)->nx = normal.x;
		(*vtx)->ny = normal.y;
		(*vtx)->nz = normal.z;
	} else
		luaL_error(L, "Invalid member access for vertex: %s", member);

	return 0;
}

int Interpreter::ntv_collect(lua_State *L)
{
	ntv_data* inst = (ntv_data*)luaL_checkudata(L, 1, "NTV.vtable");
	if(inst->owning && inst->vtx)
		delete []inst->vtx;

	return 0;
}

/***
Get the size of the array.

@function size
@treturn number number of vertices used in the array
*/
int Interpreter::ntv_size (lua_State *L) {
	ntv_data* inst = (ntv_data*)luaL_checkudata(L, 1, "NTV.vtable");
    lua_pushnumber(L, inst->nVtxUsed);
    return 1;
}

int Interpreter::ntv_get(lua_State *L) {
	ntv_data* inst = (ntv_data*)luaL_checkudata(L, 1, "NTV.vtable");
	if(lua_isnumber(L, 2)) { // return proxy object for vertex
		int index = luaL_checkinteger(L, 2);
		char cbuf[256];
		if(!(1 <= index && index <= inst->nVtxUsed)) {
			sprintf(cbuf, "index out of range (%d/%d)", index, inst->nVtxUsed);
			luaL_argcheck(L, false, 2, cbuf);
		}
		/* return element address */
		NTVERTEX *vtx = &inst->vtx[index - 1];
		ntvproxy_create(L, vtx);
		return 1;
	} else {
		const char *method = luaL_checkstring(L, 2);
		if(!strcmp(method, "zeroize")) {
			lua_pushcfunction(L, ntv_zeroize);
			return 1;
		}
		if(!strcmp(method, "reset")) {
			lua_pushcfunction(L, ntv_reset);
			return 1;
		}
		if(!strcmp(method, "size")) {
			lua_pushcfunction(L, ntv_size);
			return 1;
		}
		if(!strcmp(method, "extract")) {
			lua_pushcfunction(L, ntv_extract);
			return 1;
		}
		if(!strcmp(method, "append")) {
			lua_pushcfunction(L, ntv_append);
			return 1;
		}
		if(!strcmp(method, "copy")) {
			lua_pushcfunction(L, ntv_copy);
			return 1;
		}
		if(!strcmp(method, "view")) {
			lua_pushcfunction(L, ntv_view);
			return 1;
		}
		if(!strcmp(method, "write")) {
			lua_pushcfunction(L, ntv_write);
			return 1;
		}
		
		return luaL_error(L, "invalid ntvertex method %s", method);
	}
}

/***
Extract vertex data.

This function returns a Lua table containing vertex informations.

The table has the following fields:

- x: number (vertex x position)
- y: number (vertex y position)
- z: number (vertex z position)
- nx: number (vertex x normal)
- ny: number (vertex y normal)
- nz: number (vertex z normal)
- tu: number (vertex u texture coordinate)
- tv: number (vertex v texture coordinate)

The table is a _copy_ of the data, so modifiying the table will not change the
vertex it was extracted from.

To effect modifications, you need to rewrite the modified table into the vertex.

@function extract
@tparam number idx vertex index
@treturn table vertex description
*/
int Interpreter::ntv_extract(lua_State *L) {
	ntv_data* inst = (ntv_data*)luaL_checkudata(L, 1, "NTV.vtable");
    int index = luaL_checkinteger(L, 2);
    luaL_argcheck(L, 1 <= index && index <= inst->nVtxUsed, 2, "index out of range");
    
    /* return element address */
    NTVERTEX *vtx = &inst->vtx[index - 1];
	lua_newtable(L);
	lua_pushnumber (L, vtx->x);
	lua_setfield (L, -2, "x");
	lua_pushnumber (L, vtx->y);
	lua_setfield (L, -2, "y");
	lua_pushnumber (L, vtx->z);
	lua_setfield (L, -2, "z");

	lua_pushnumber (L, vtx->nx);
	lua_setfield (L, -2, "nx");
	lua_pushnumber (L, vtx->ny);
	lua_setfield (L, -2, "ny");
	lua_pushnumber (L, vtx->nz);
	lua_setfield (L, -2, "nz");

	lua_pushnumber (L, vtx->tu);
	lua_setfield (L, -2, "tu");
	lua_pushnumber (L, vtx->tv);
	lua_setfield (L, -2, "tv");
	return 1;
}

/***
Reset the array.

This function reset the size of the array to 0 (but does not affect the capacity)

@function reset
*/
int Interpreter::ntv_reset(lua_State *L) {
	ntv_data* inst = (ntv_data*)luaL_checkudata(L, 1, "NTV.vtable");
	inst->nVtxUsed = 0;
	return 0;
}

/***
Clear the array.

This function resets all vertices fields value to 0.

@function zeroize
*/
int Interpreter::ntv_zeroize(lua_State *L) {
	ntv_data* inst = (ntv_data*)luaL_checkudata(L, 1, "NTV.vtable");
	memset(inst->vtx, 0, inst->nVtx * sizeof(NTVERTEX));
	return 0;
}

/***
Append an array.

Append the used portion of an array.
If the capacity is too small, generate a Lua error.

@function append
@tparam ntvertexarray vtx vertex array to append
*/
int Interpreter::ntv_append(lua_State *L) {
	ntv_data* dst = (ntv_data*)luaL_checkudata(L, 1, "NTV.vtable");
	ntv_data* src = (ntv_data*)luaL_checkudata(L, 2, "NTV.vtable");

	if(dst->nVtxUsed + src->nVtxUsed > dst->nVtx)
		return luaL_error(L, "Cannot append ntvertexarray, not enough room");

	memcpy(dst->vtx + dst->nVtxUsed, src->vtx, sizeof(NTVERTEX) * src->nVtxUsed);
	dst->nVtxUsed += src->nVtxUsed;
	return 0;
}

/***
Write data from an array.

Copie vertices from src into the object, increasing its size if necessary (but not its capacity).

If the capacity is too small, generate a Lua error.

@function write
@tparam ntvertexarray src vertex array to read from
@tparam[opt=1] number offset offset to write the data to
@tparam[opt=src.size()] number size number of vertices to copy
*/
int Interpreter::ntv_write(lua_State *L) {
	ntv_data* self = (ntv_data*)luaL_checkudata(L, 1, "NTV.vtable");
	ntv_data* from = (ntv_data*)luaL_checkudata(L, 2, "NTV.vtable");
	int size = from->nVtxUsed;
	int start = 0;

	if(lua_gettop(L)>=3) {
		start = luaL_checkinteger(L,3) - 1;
		if(start < 0) {
			luaL_error(L, "Invalid write offset (%d)", start+1);
		}
		if(start > self->nVtx) {
			luaL_error(L, "Write out of bound (%d/%d)", start+1,self->nVtx);
		}
	}
	if(lua_gettop(L)>=4) {
		size = luaL_checkinteger(L,4);
		if(size+start > self->nVtx) {
			luaL_error(L, "Write out of bound (%d/%d)", start+size,self->nVtx);
		}
	}
	memcpy(self->vtx + start, from->vtx, size*sizeof(NTVERTEX));
	self->nVtxUsed = std::max(self->nVtxUsed, start + size);
	return 0;
}

/***
Make a [sub]copy.

This function can be used to create a new ntvertexarray containing the
same data as the original, or a subrange of it.

@function copy
@tparam ntvertexarray src vertex array to read from
@tparam[opt=1] number offset offset to write the data to
@tparam[opt] number size number of vertices to copy
*/
int Interpreter::ntv_copy(lua_State *L) {
	ntv_data* from = (ntv_data*)luaL_checkudata(L, 1, "NTV.vtable");

	int start = 0;
	int size = from->nVtx;
	if(lua_gettop(L) >= 2) {
		start = luaL_checkinteger(L, 2) - 1; // 1 based
		if(start < 0) {
			return luaL_error(L, "Invalid start offset (%d)", start + 1);
		}
		if(start > from->nVtx) {
			return luaL_error(L, "Start offset outside of vertex array (%d/%d)", start + 1, from->nVtx);
		}
		size -= start;
	}
	if(lua_gettop(L) >= 3) {
		size = luaL_checkinteger(L, 3);
		if(size <= 0) {
			return luaL_error(L, "Invalid size (%d)", size);
		}
		if(start + size > from->nVtx) {
			return luaL_error(L, "Trying to copy outside of vertex array (%d/%d)", start + size, from->nVtx);
		}
	}

	ntv_data *copy = (ntv_data *)lua_newuserdata(L, sizeof(ntv_data));

	luaL_getmetatable(L, "NTV.vtable");
	lua_setmetatable(L, -2);
    
	copy->nVtx = size;
	copy->nVtxUsed = size;
	copy->vtx = new NTVERTEX[size];
	memcpy(copy->vtx, from->vtx + start, size*sizeof(NTVERTEX));
	copy->owning = true;
	return 1;
}

/***
Create a view.

This function works similarly to copy but it creates a non owning view of the original data
instead of copying it.

This is useful if you need to provide a subrange of the array, and don't want the performance
impact of allocating/copying the new data.

Generate a Lua error if the size/offset is not compatible with the array.

WARNING: a view must not outlive its original array, trying to access data when
the original has been destroyed will likely result in a crash.

@function view
@tparam number offset start offset
@tparam[opt] number size view size (default = until the end of the buffer)
@usage self:redraw(self.vtx:view(self.vtxofs))
*/
int Interpreter::ntv_view(lua_State *L) {
	ntv_data* from = (ntv_data*)luaL_checkudata(L, 1, "NTV.vtable");
	int start = lua_tointeger(L, 2) - 1;  // 1 based indexing

	if(start < 0) {
		return luaL_error(L, "Invalid start offset (%d)", start + 1);
	}

	int size = lua_tointeger(L, 3);

	if(size < 0) {
		return luaL_error(L, "Invalid view size (%d)", size);
	}

	// if size is 0 or not specified then create a view to the end
	if(size == 0)
		size = from->nVtxUsed - start;

	if( start+size > from->nVtx) {
		return luaL_error(L, "Cannot create a view out of the array (%d>%d)", start+size > from->nVtx);
	}

	ntv_data *view = (ntv_data *)lua_newuserdata(L, sizeof(ntv_data));
	luaL_getmetatable(L, "NTV.vtable");
	lua_setmetatable(L, -2);
    
	view->nVtx = size;
	view->nVtxUsed = size;
	view->vtx = from->vtx + start;
	view->owning = false;
	return 1;
}

int Interpreter::ntv_set(lua_State *L) {
	ntv_data* inst = (ntv_data*)luaL_checkudata(L, 1, "NTV.vtable");
    int index = luaL_checkinteger(L, 2);
    luaL_argcheck(L, 1 <= index && index <= inst->nVtxUsed, 2, "index out of range");
    
    /* return element address */
    NTVERTEX *vtx = &inst->vtx[index - 1];

	lua_getfield(L, 3, "x"); vtx->x = luaL_checknumber(L, -1); lua_pop (L,1);
	lua_getfield(L, 3, "y"); vtx->y = luaL_checknumber(L, -1); lua_pop (L,1);
	lua_getfield(L, 3, "z"); vtx->z = luaL_checknumber(L, -1); lua_pop (L,1);

	lua_getfield(L, 3, "nx"); vtx->nx = luaL_checknumber(L, -1); lua_pop (L,1);
	lua_getfield(L, 3, "ny"); vtx->ny = luaL_checknumber(L, -1); lua_pop (L,1);
	lua_getfield(L, 3, "nz"); vtx->nz = luaL_checknumber(L, -1); lua_pop (L,1);

	lua_getfield(L, 3, "tu"); vtx->tu = luaL_checknumber(L, -1); lua_pop (L,1);
	lua_getfield(L, 3, "tv"); vtx->tv = luaL_checknumber(L, -1); lua_pop (L,1);

	return 0;
}

void Interpreter::push_indexarray(lua_State *L, WORD *idx, int nIdx)
{
	index_data *array = (index_data *)lua_newuserdata(L, sizeof(index_data));
    
	luaL_getmetatable(L, "Index.vtable");
	lua_setmetatable(L, -2);
    
	array->nIdx = nIdx;
	array->nIdxUsed = nIdx;
	array->idx = idx;
	array->owning = false;
}

int Interpreter::idx_collect(lua_State *L)
{
	index_data* inst = (index_data*)luaL_checkudata(L, 1, "Index.vtable");
	if(inst->owning && inst->idx)
		delete []inst->idx;

	return 0;
}

/***
This type provides an encapsulation of C++ index buffers that
can be used to describe 3D meshes.

It can be accessed as a regular Lua table of numbers.

An indexarray is of fixed capacity, but it can contain a variable amount of indices, up to the max capacity.
This can be used to concatenate index arrays :
	local idx = oapi.create_indexarray(12+16+4)
	idx:reset()

	if gear_deployed then
		idx:append(igear)
	end

	if nosecone_open then
		idx:append(inose)
	end

	if airbrake_open then
		idx:append(ibrk)
	end

@classmod indexarray
*/

/***
Get the size of the array.

@function size
@treturn number number of indices used in the array
*/
int Interpreter::idx_size (lua_State *L) {
	index_data* inst = (index_data*)luaL_checkudata(L, 1, "Index.vtable");
    lua_pushnumber(L, inst->nIdxUsed);
    return 1;
}

/***
Reset the array.

This function reset the size of the array to 0 (but does not affect the capacity)

@function reset
*/
int Interpreter::idx_reset (lua_State *L) {
	index_data* inst = (index_data*)luaL_checkudata(L, 1, "Index.vtable");
	inst->nIdxUsed = 0;
	return 0;
}

/***
Append an array.

Append the used portion of an array.
If the capacity is too small, generate a Lua error.

@function append
@tparam indexarray idx index array to append
*/
int Interpreter::idx_append (lua_State *L) {
	index_data* dst = (index_data*)luaL_checkudata(L, 1, "Index.vtable");
	index_data* src = (index_data*)luaL_checkudata(L, 2, "Index.vtable");

	int offset = lua_tointeger(L, 3);

	if(dst->nIdxUsed + src->nIdxUsed > dst->nIdx)
		luaL_error(L, "Cannot append ntvertexarray, not enough room");

	for(int i=0; i<src->nIdxUsed;i++) {
		dst->idx[dst->nIdxUsed + i] = src->idx[i] + offset;
	}

	dst->nIdxUsed += src->nIdxUsed;
	return 0;
}

int Interpreter::idx_get(lua_State *L) {
	index_data* inst = (index_data*)luaL_checkudata(L, 1, "Index.vtable");
	if(lua_isnumber(L, 2)) { // return proxy object for vertex
		int index = luaL_checkinteger(L, 2);
		luaL_argcheck(L, 1 <= index && index <= inst->nIdxUsed, 2, "index out of range");
    
		/* return element address */
		lua_pushnumber(L, inst->idx[index - 1]);
		return 1;
	} else {
		const char *method = luaL_checkstring(L, 2);
		if(!strcmp(method, "reset")) {
			lua_pushcfunction(L, idx_reset);
			return 1;
		}
		if(!strcmp(method, "size")) {
			lua_pushcfunction(L, idx_size);
			return 1;
		}
		if(!strcmp(method, "append")) {
			lua_pushcfunction(L, idx_append);
			return 1;
		}
		return luaL_error(L, "invalid indexarray method %s", method);	}
}

int Interpreter::idx_set(lua_State *L) {
	index_data* inst = (index_data*)luaL_checkudata(L, 1, "Index.vtable");
    int index = luaL_checkinteger(L, 2);
    luaL_argcheck(L, 1 <= index && index <= inst->nIdxUsed, 2, "index out of range");
    WORD value = luaL_checkinteger(L, 3);

	inst->idx[index - 1] = value;
	return 0;
}


void *Interpreter::luaL_tryudata (lua_State *L, int ud, const char *tname) {
  void *p = lua_touserdata(L, ud);
  if(p == NULL) return NULL;
  if(!lua_getmetatable(L, ud)) return NULL;

  lua_getfield(L, LUA_REGISTRYINDEX, tname);
  
  if(!lua_rawequal(L, -1, -2)) {
	p = NULL;
  }
  lua_pop(L, 2);
  return p;
}

void Interpreter::lua_pushmeshhandle(lua_State *L, MESHHANDLE hMesh)
{
	MESHHANDLE *h = (MESHHANDLE *)lua_newuserdata(L, sizeof(MESHHANDLE));
	*h = hMesh;
	luaL_getmetatable(L, "MESHHANDLE");
	lua_setmetatable(L, -2);
}

MESHHANDLE Interpreter::lua_tomeshhandle(lua_State *L, int idx)
{
	return (MESHHANDLE)*(MESHHANDLE *)luaL_checkudata (L, idx, "MESHHANDLE");
}
int Interpreter::lua_ismeshhandle(lua_State *L, int idx)
{
	return luaL_tryudata(L, idx, "MESHHANDLE") != NULL;
}


void Interpreter::lua_pushdevmeshhandle(lua_State *L, DEVMESHHANDLE hMesh)
{
	DEVMESHHANDLE *h = (DEVMESHHANDLE *)lua_newuserdata(L, sizeof(DEVMESHHANDLE));
	*h = hMesh;
	luaL_getmetatable(L, "DEVMESHHANDLE");
	lua_setmetatable(L, -2);
}

DEVMESHHANDLE Interpreter::lua_todevmeshhandle(lua_State *L, int idx)
{
	return (DEVMESHHANDLE)*(DEVMESHHANDLE *)luaL_checkudata (L, idx, "DEVMESHHANDLE");
}
int Interpreter::lua_isdevmeshhandle(lua_State *L, int idx)
{
	return luaL_tryudata(L, idx, "DEVMESHHANDLE") != NULL;
}

/***
This type represents a beacon object.

The object has the following members which can be modified to change the behavior of the beacon :

- shape: number (shape : BEACONSHAPE.COMPACT, BEACONSHAPE.DIFFUSE or BEACONSHAPE.STAR)
- pos: vector (position in vessel coordinates)
- col: vector (beacon RGB colour)
- size: number (beacon radius)
- fallof: number (distance falloff parameter)
- period: number (strobe period (0 for continuous))
- duration: number (strobe duration)
- tofs: number (strobe time offset)
- active: boolean (beacon lit?)

@classmod beacon
@see oapi.create_beacon
@see vessel:add_beacon
@see vessel:del_beacon
*/

int Interpreter::beacon_collect(lua_State *L)
{
	BEACONLIGHTSPEC_Lua* beacon = (BEACONLIGHTSPEC_Lua*)luaL_checkudata(L, 1, "Beacon.vtable");
	if(beacon->vessel) {
		// Remove the association in case the reference is lost to prevent potential use after free bugs
		beacon->vessel->DelBeacon(&beacon->bs);
	}

	return 1;
}

int Interpreter::beacon_get(lua_State *L)
{
	BEACONLIGHTSPEC_Lua *beacon = (BEACONLIGHTSPEC_Lua *)luaL_checkudata(L, 1, "Beacon.vtable");
    const char *member = luaL_checkstring(L, 2);
	if(!strcmp(member, "shape"))
		lua_pushinteger(L, beacon->bs.shape);
	else if(!strcmp(member, "pos"))
		lua_pushvector(L, beacon->pos);
	else if(!strcmp(member, "col"))
		lua_pushvector(L, beacon->col);
	else if(!strcmp(member, "size"))
		lua_pushnumber(L, beacon->bs.size);
	else if(!strcmp(member, "falloff"))
		lua_pushnumber(L, beacon->bs.falloff);
	else if(!strcmp(member, "period"))
		lua_pushnumber(L, beacon->bs.period);
	else if(!strcmp(member, "duration"))
		lua_pushnumber(L, beacon->bs.duration);
	else if(!strcmp(member, "tofs"))
		lua_pushnumber(L, beacon->bs.tofs);
	else if(!strcmp(member, "active"))
		lua_pushboolean(L, beacon->bs.active);
	else
		luaL_error(L, "Trying to access unknown beacon field '%s'", member);

	return 1;
}

int Interpreter::beacon_set (lua_State *L)
{
	BEACONLIGHTSPEC_Lua *beacon = (BEACONLIGHTSPEC_Lua *)luaL_checkudata(L, 1, "Beacon.vtable");
    const char *member = luaL_checkstring(L, 2);

	if(!strcmp(member, "shape"))
		beacon->bs.shape = luaL_checkinteger(L, 3);
	else if(!strcmp(member, "pos"))
		beacon->pos = lua_tovector_safe(L, 3, "beacon_set");
	else if(!strcmp(member, "col"))
		beacon->col = lua_tovector_safe(L, 3, "beacon_set");
	else if(!strcmp(member, "size"))
		beacon->bs.size = luaL_checknumber(L, 3);
	else if(!strcmp(member, "falloff"))
		beacon->bs.falloff = luaL_checknumber(L, 3);
	else if(!strcmp(member, "period"))
		beacon->bs.period = luaL_checknumber(L, 3);
	else if(!strcmp(member, "duration"))
		beacon->bs.duration = luaL_checknumber(L, 3);
	else if(!strcmp(member, "tofs"))
		beacon->bs.tofs = luaL_checknumber(L, 3);
	else if(!strcmp(member, "active"))
		beacon->bs.active = lua_toboolean(L, 3);
		
	return 0;
}


// ============================================================================
// core thread functions

int OpenHelp (void *context)
{
	HELPCONTEXT *hc = (HELPCONTEXT*)context;
	oapiOpenHelp (hc);
	return 0;

}

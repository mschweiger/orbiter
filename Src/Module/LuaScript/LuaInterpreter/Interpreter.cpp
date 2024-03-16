// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#define INTERPRETER_IMPLEMENTATION

#include "Interpreter.h"
#include "VesselAPI.h"
#include "MFDAPI.h"
#include "DrawAPI.h"
#include <list>

using std::min;
using std::max;

/***
Module oapi: General Orbiter API interface functions
@module oapi
*/

std::list<NOTEHANDLE *> g_notehandles;

int OpenHelp (void *context);

// ============================================================================
// nonmember functions

/***
A table representing a 3D cartesian vector.
@field x x-component
@field y y-component
@field z z-component
@table vector
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

// ============================================================================
// class Interpreter

Interpreter::Interpreter () : sem_exec(0), sem_wait(0)
{
	L = luaL_newstate();  // create new Lua context
	is_busy = false;      // waiting for input
	is_term = false;      // no attached terminal by default
	jobs = 0;             // background jobs
	status = 0;           // normal
	term_verbose = 0;     // verbosity level
	postfunc = 0;
	postcontext = 0;
	// store interpreter context in the registry
	lua_pushlightuserdata (L, this);
	lua_setfield (L, LUA_REGISTRYINDEX, "interp");
}

Interpreter::~Interpreter ()
{
	lua_close (L);
}

void Interpreter::Initialise ()
{
	luaL_openlibs (L);    // load the default libraries
	LoadAPI ();           // load default set of API interface functions
	LoadVesselAPI ();     // load vessel-specific part of API
	LoadLightEmitterMethods (); // load light source methods
	LoadMFDAPI ();        // load MFD methods
	LoadSketchpadAPI ();  // load Sketchpad methods
	LoadAnnotationAPI (); // load screen annotation methods
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
	sem_exec.release();
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
			strcat (fieldstr, lua_tostringex (L,-1));
			strcat (tbuf, fieldstr); strcat (tbuf, "\n");
			lua_pop(L, 1);
		}
		return tbuf;
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

void Interpreter::lua_pushvessel (lua_State *L, VESSEL *v)
{
	lua_pushlightuserdata(L,v);         // use object pointer as key
	lua_gettable(L,LUA_REGISTRYINDEX);  // retrieve object from registry
	if (lua_isnil(L,-1)) {              // object not found
		lua_pop(L,1);                   // pop nil
		VESSEL **pv = (VESSEL**)lua_newuserdata(L,sizeof(VESSEL*));
		*pv = v;
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

void Interpreter::StepInterpreter ()
{
	sem_exec.release();
	sem_wait.acquire();
}

void Interpreter::WaitForStep ()
{
	sem_exec.acquire();
}
void Interpreter::StepDone()
{
	sem_wait.release();
}

void Interpreter::frameskip (lua_State *L)
{
	if (status == 1) { // termination request
		lua_pushboolean(L, 1);
		lua_setfield (L, LUA_GLOBALSINDEX, "wait_exit");
	} else {
		StepDone();
		// yield to the orbiter thread
		WaitForStep();
	}
}

int Interpreter::RunChunk (const char *chunk, int n)
{
	int res = 0;
	if (chunk[0]) {
		is_busy = true;
		// run command
		luaL_loadbuffer (L, chunk, n, "line");
		res = lua_pcall (L, 0, 0, 0);
		if (res) {
			auto error = lua_tostring(L, -1);
			if (error) { // can be nullptr
				if (is_term) {
					// term_strout ("Execution error.");
					term_strout(error, true);
				}
				else {
					oapiWriteLogError(error);
				}
				is_busy = false;
				return res;
			}
		}
		// check for leftover background jobs
		lua_getfield (L, LUA_GLOBALSINDEX, "_nbranch");
		lua_call (L, 0, 1);
		jobs = lua_tointeger (L, -1);
		lua_pop (L, 1);
		is_busy = false;
	} else {
		// idle loop: execute background jobs
		lua_getfield (L, LUA_GLOBALSINDEX, "_idle");
		lua_call (L, 0, 1);
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
	static const struct luaL_reg glob[] = {
		{"help", help},
		//{"api", help_api},
		{NULL, NULL}
	};
	for (int i = 0; i < ARRAYSIZE(glob) && glob[i].name; i++) {
		lua_pushcfunction (L, glob[i].func);
		lua_setglobal (L, glob[i].name);
	}

	// Load the vector library
	static const struct luaL_reg vecLib[] = {
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
	luaL_openlib (L, "vec", vecLib, 0);

	static const struct luaL_reg matLib[] = {
		{"identity", mat_identity},
		{"mul", mat_mul},
		{"tmul", mat_tmul},
		{"mmul", mat_mmul},
		{NULL, NULL}
	};
	luaL_openlib (L, "mat", matLib, 0);

	// Load the process library
	static const struct luaL_reg procLib[] = {
		{"Frameskip", procFrameskip},
		{NULL, NULL}
	};
	luaL_openlib (L, "proc", procLib, 0);

	// Load the oapi library
	static const struct luaL_reg oapiLib[] = {
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
		{"del_vessel", oapi_del_vessel},

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
		{"equ_to_global", oapi_equ_to_global},
		{"orthodome", oapi_orthodome},

		// body functions
		{"get_size", oapi_get_size},
		{"get_mass", oapi_get_mass},
		{"get_globalpos", oapi_get_globalpos},
		{"get_globalvel", oapi_get_globalvel},
		{"get_relativepos", oapi_get_relativepos},
		{"get_relativevel", oapi_get_relativevel},

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

		// animation functions
		{"create_animationcomponent", oapi_create_animationcomponent},
		{"del_animationcomponent", oapi_del_animationcomponent},

		// instrument panel functions
		{"open_mfd", oapi_open_mfd},
		{"set_hudmode", oapi_set_hudmode},
		{"set_panelblink", oapi_set_panelblink},

		// user i/o functions
		{"keydown", oapi_keydown},
		{"resetkey", oapi_resetkey},
		{"simulatebufferedkey", oapi_simulatebufferedkey},
		{"simulateimmediatekey", oapi_simulateimmediatekey},

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

		{NULL, NULL}
	};
	luaL_openlib (L, "oapi", oapiLib, 0);

	// Load the (dummy) term library
	static const struct luaL_reg termLib[] = {
		{"out", termOut},
		{NULL, NULL}
	};
	luaL_openlib (L, "term", termLib, 0);

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
}

void Interpreter::LoadMFDAPI ()
{
	static const struct luaL_reg mfdLib[] = {
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
	luaL_openlib (L, NULL, mfdLib, 0);
}

void Interpreter::LoadLightEmitterMethods ()
{
	static const struct luaL_reg methodLib[] = {
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
		{NULL, NULL}
	};

	luaL_newmetatable (L, "LightEmitter.vtable");
	lua_pushstring (L, "__index");
	lua_pushvalue (L, -2); // push metatable
	lua_settable (L, -3); // metatable.__index = metatable
	luaL_openlib (L, NULL, methodLib, 0);
}

void Interpreter::LoadSketchpadAPI ()
{
	static const struct luaL_reg skpLib[] = {
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
		{"get_charsize", skp_get_charsize},
		{"get_textwidth", skp_get_textwidth},
		{NULL, NULL}
	};

	luaL_newmetatable (L, "SKP.vtable");
	lua_pushstring (L, "__index");
	lua_pushvalue (L, -2); // push metatable
	lua_settable (L, -3); // metatable.__index = metatable
	luaL_openlib (L, NULL, skpLib, 0);

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
}

void Interpreter::LoadAnnotationAPI ()
{
	static const struct luaL_reg noteMtd[] = {
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
	luaL_openlib (L, NULL, noteMtd, 0);
}

void Interpreter::LoadStartupScript ()
{
	luaL_dofile (L, ".\\Script\\oapi_init.lua");
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
	char cbuf[1024];
	int res = 1;

	if (lua_gettop(L) < idx) {
		sprintf (cbuf, "%s: too few arguments", fname);
		term_strout(L, cbuf, true);
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
	}
	if (!res) {
		sprintf (cbuf, "%s: argument %d: invalid type (expected %s)", fname, prm, tpname);
		term_strout(L, cbuf, true);
	}
	return res;
}

int Interpreter::AssertMtdMinPrmCount(lua_State *L, int n, const char *funcname)
{
	if (lua_gettop(L) >= n) {
		return 1;
	} else {
		char cbuf[1024];
		sprintf(cbuf, "%s: too few arguments (expected %d)", funcname, n-1);
		term_strout(L, cbuf);
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
	cbuf[strlen(cbuf)-3] = ')';
	cbuf[strlen(cbuf)-2] = '\0';
	term_strout(L, cbuf);
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
		char cbuf[1024];
		sprintf(cbuf, "%s: argument %d: invalid type (expected number)", funcname, idx-1);
		return 0;
	}
}

int Interpreter::AssertMtdHandle(lua_State *L, int idx, const char *funcname)
{
	if (lua_islightuserdata(L, 2)) { // necessary but not sufficient
		return 1;
	} else {
		char cbuf[1024];
		sprintf(cbuf, "%s: argument %d: invalid type (expected handle)", funcname, idx-1);
		return 0;
	}
}

// ============================================================================
// global functions

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
	lua_pcall (L, 1, 0, 0);
	return 0;
}

// ============================================================================
// vector library functions

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

int Interpreter::vec_length (lua_State *L)
{
	VECTOR3 v;
	ASSERT_SYNTAX(lua_isvector(L,1), "Argument 1: expected vector");
	v = lua_tovector(L,1);
	lua_pushnumber (L, length(v));
	return 1;
}

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

int Interpreter::vec_unit (lua_State *L)
{
	VECTOR3 v;
	ASSERT_SYNTAX(lua_isvector(L,1), "Argument 1: expected vector");
	v = lua_tovector(L,1);
	lua_pushvector (L, unit(v));
	return 1;
}

int Interpreter::mat_identity (lua_State *L)
{
	lua_pushmatrix (L,identity());
	return 1;
}

int Interpreter::mat_mul (lua_State *L)
{
	ASSERT_SYNTAX(lua_ismatrix(L,1), "Argument 1: expected matrix");
	ASSERT_SYNTAX(lua_isvector(L,2), "Argument 2: expected vector");
	lua_pushvector (L, mul (lua_tomatrix(L,1), lua_tovector(L,2)));
	return 1;
}

int Interpreter::mat_tmul (lua_State *L)
{
	ASSERT_SYNTAX(lua_ismatrix(L,1), "Argument 1: expected matrix");
	ASSERT_SYNTAX(lua_isvector(L,2), "Argument 2: expected vector");
	lua_pushvector (L, tmul (lua_tomatrix(L,1), lua_tovector(L,2)));
	return 1;
}

int Interpreter::mat_mmul (lua_State *L)
{
	ASSERT_SYNTAX(lua_ismatrix(L,1), "Argument 1: expected matrix");
	ASSERT_SYNTAX(lua_ismatrix(L,2), "Argument 2: expected matrix");
	lua_pushmatrix (L, mul(lua_tomatrix(L,1), lua_tomatrix(L,2)));
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

int Interpreter::oapi_get_mainmenuvisibilitymode (lua_State *L)
{
	lua_pushnumber (L, oapiGetMainMenuVisibilityMode());
	return 1;
}

int Interpreter::oapi_set_mainmenuvisibilitymode (lua_State *L)
{
	ASSERT_SYNTAX (lua_isnumber (L,1), "Argument 1: invalid type (expected number)");
	DWORD mode = (DWORD)lua_tonumber (L,1);
	ASSERT_SYNTAX (mode <= 2, "Argument 1: out of range");
	oapiSetMainMenuVisibilityMode (mode);
	return 0;
}

int Interpreter::oapi_get_maininfovisibilitymode (lua_State *L)
{
	lua_pushnumber (L, oapiGetMainInfoVisibilityMode());
	return 1;
}

int Interpreter::oapi_set_maininfovisibilitymode (lua_State *L)
{
	ASSERT_SYNTAX (lua_isnumber (L,1), "Argument 1: invalid type (expected number)");
	DWORD mode = (DWORD)lua_tonumber (L,1);
	ASSERT_SYNTAX (mode <= 2, "Argument 1: out of range");
	oapiSetMainInfoVisibilityMode (mode);
	return 0;
}

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

int Interpreter::oapiDelAnnotation (lua_State *L)
{
	NOTEHANDLE *pnote = (NOTEHANDLE*)lua_touserdata (L, 1);
	::oapiDelAnnotation (*pnote);

	g_notehandles.remove(pnote);

	*pnote = NULL;
	return 0;
}

int Interpreter::oapiDbgOut (lua_State *L)
{
	const char *str = lua_tostringex (L, 1);
	strcpy (oapiDebugString(), str);
	return 0;
}

int Interpreter::oapiWriteLog(lua_State* L)
{
	const char* str = lua_tostringex(L, 1);
	::oapiWriteLog(const_cast<char*>(str));
	return 0;
}

int Interpreter::oapiExit(lua_State* L)
{
	Interpreter* interp = GetInterpreter(L);
	interp->exitCode = (int)lua_tointeger(L, 1);;
	return 0;
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

int Interpreter::oapiOpenInputBox (lua_State *L)
{
	const char *title = lua_tostring (L, 1);
	int vislen = lua_tointeger (L, 2);
	bInputClosed = false;
	oapiOpenInputBoxEx (title, inputClbk, inputCancel, 0, 40, 0, USRINPUT_NEEDANSWER);
	return 0;
}

int Interpreter::oapiReceiveInput (lua_State *L)
{
	if (bInputClosed)
		lua_pushstring (L, cInput);
	else
		lua_pushnil (L);
	return 1;
}

int Interpreter::oapi_global_to_equ (lua_State *L)
{
	OBJHANDLE hObj;
	if (lua_islightuserdata (L,1) && (hObj = lua_toObject (L,1))) {
		VECTOR3 glob = lua_tovector(L,2);
		double lng, lat, rad;
		oapiGlobalToEqu (hObj, glob, &lng, &lat, &rad);
		lua_createtable (L, 0, 3);
		lua_pushnumber (L, lng);
		lua_setfield (L, -2, "lng");
		lua_pushnumber (L, lat);
		lua_setfield (L, -2, "lat");
		lua_pushnumber (L, rad);
		lua_setfield (L, -2, "rad");
	} else lua_pushnil (L);
	return 1;
}

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

int Interpreter::oapi_get_size (lua_State *L)
{
	OBJHANDLE hObj;
	ASSERT_SYNTAX(lua_gettop(L) >= 1, "Too few arguments");
	ASSERT_SYNTAX(lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(hObj = lua_toObject (L,1), "Argument 1: invalid object");
	lua_pushnumber (L, oapiGetSize(hObj));
	return 1;
}

int Interpreter::oapi_get_mass (lua_State *L)
{
	OBJHANDLE hObj;
	ASSERT_SYNTAX(lua_gettop(L) >= 1, "Too few arguments");
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX (hObj = lua_toObject (L,1), "Argument 1: invalid object");
	lua_pushnumber (L, oapiGetMass (hObj));
	return 1;
}

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

int Interpreter::oapi_get_propellantmass (lua_State *L)
{
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	PROPELLANT_HANDLE hp = (PROPELLANT_HANDLE)lua_touserdata (L, 1);
	ASSERT_SYNTAX(hp, "Argument 1: invalid object");
	lua_pushnumber (L, oapiGetPropellantMass (hp));
	return 1;
}

int Interpreter::oapi_get_propellantmaxmass (lua_State *L)
{
	ASSERT_SYNTAX (lua_islightuserdata (L,1), "Argument 1: invalid type (expected handle)");
	PROPELLANT_HANDLE hp = (PROPELLANT_HANDLE)lua_touserdata (L, 1);
	ASSERT_SYNTAX(hp, "Argument 1: invalid object");
	lua_pushnumber (L, oapiGetPropellantMaxMass (hp));
	return 1;
}

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
	oapiGetAtm(hObj, &prm);
	lua_createtable (L, 0, 3);
	lua_pushnumber (L, prm.p);
	lua_setfield (L, -2, "p");
	lua_pushnumber (L, prm.rho);
	lua_setfield (L, -2, "rho");
	lua_pushnumber (L, prm.T);
	lua_setfield (L, -2, "T");
	return 1;
}

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

int Interpreter::oapi_get_cameratarget (lua_State *L)
{
	OBJHANDLE hObj = oapiCameraTarget();
	if (hObj)
		lua_pushlightuserdata (L, hObj);
	else
		lua_pushnil (L);
	return 1;
}

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

int Interpreter::oapi_get_cameraaperture (lua_State *L)
{
	double ap = oapiCameraAperture();
	lua_pushnumber (L, ap);
	return 1;
}

int Interpreter::oapi_set_cameraaperture (lua_State *L)
{
	ASSERT_SYNTAX (lua_isnumber (L,1), "Argument 1: invalid type (expected number)");
	double ap = lua_tonumber (L,1);
	oapiCameraSetAperture (ap);
	return 0;
}

int Interpreter::oapi_get_cameraglobalpos (lua_State *L)
{
	VECTOR3 pos;
	oapiCameraGlobalPos (&pos);
	lua_pushvector (L, pos);
	return 1;
}

int Interpreter::oapi_get_cameraglobaldir (lua_State *L)
{
	VECTOR3 dir;
	oapiCameraGlobalDir (&dir);
	lua_pushvector (L, dir);
	return 1;
}

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

int Interpreter::oapi_del_animationcomponent (lua_State *L)
{
	ASSERT_LIGHTUSERDATA(L,1);
	MGROUP_TRANSFORM *trans = (MGROUP_TRANSFORM*)lua_touserdata(L,1);
	delete trans;
	return 0;
}

int Interpreter::oapi_open_mfd (lua_State *L)
{
	ASSERT_NUMBER(L,1);
	int mfdid = lua_tointeger(L,1);
	ASSERT_NUMBER(L,2);
	int mfdmode = lua_tointeger(L,2);
	oapiOpenMFD (mfdmode, mfdid);
	return 0;
}

int Interpreter::oapi_set_hudmode (lua_State *L)
{
	ASSERT_NUMBER(L,1);
	int hudmode = lua_tointeger(L,1);
	oapiSetHUDMode (hudmode);
	return 0;
}

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

int Interpreter::oapi_keydown (lua_State *L)
{
	ASSERT_LIGHTUSERDATA(L,1);
	char *kstate = (char*)lua_touserdata(L,1);
	ASSERT_NUMBER(L,2);
	int key = lua_tointeger(L, 2);
	lua_pushboolean (L, KEYDOWN(kstate,key));
	return 1;
}

int Interpreter::oapi_resetkey (lua_State *L)
{
	ASSERT_LIGHTUSERDATA(L,1);
	char *kstate = (char*)lua_touserdata(L,1);
	ASSERT_NUMBER(L,2);
	int key = lua_tointeger(L, 2);
	RESETKEY(kstate,key);
	return 0;
}

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

// ============================================================================
// file i/o functions

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
	FILEHANDLE file;
	ASSERT_SYNTAX(lua_islightuserdata(L, 1), "Argument 1: invalid type (expected handle)");
	ASSERT_SYNTAX(file = lua_toObject(L, 1), "Argument 1: invalid object");

	ASSERT_NUMBER(L, 2);
	FileAccessMode mode = (FileAccessMode)lua_tointeger(L, 2);

	oapiCloseFile(file, mode);
	return 0;
}

/***
Writes the current simulation state to a scenario file.

Note: The file name is always calculated relative from the default orbiter scenario
   folder (usually Orbiter\\Scenarios). The file name can contain a relative path
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
int Interpreter:: oapi_writescenario_string (lua_State* L)
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
@see readitem_string for more details
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
@see readitem_string for more details
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
@see readitem_string for more details
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
@see readitem_string for more details
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
@see readitem_string for more details
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
@see writeitem_string for more details
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
@see writeitem_string for more details
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
@see writeitem_string for more details
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
@see writeitem_string for more details
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
Returns uniformly distributed pseudo-random number in the range [0..1].

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
Deflates (or packs) a string.

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
	DWORD      nebuf = lua_objlen(L, 1);
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
Inflates (or unpacks) a packed string that was packed by @{deflate} or by the
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
	DWORD      nzbuf = lua_objlen(L, 1);
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
Returns a colour value adapted to the current screen colour depth for given
red, green and blue components.

Colour values are required for some surface functions like @{clear_surface}
   or @{set_surfacecolourkey}. The colour key for a given RGB triplet depends
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
Formats floating point value f in the standard Orbiter convention,
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

// ============================================================================
// screen annotation library functions

int Interpreter::noteSetText (lua_State *L)
{
	NOTEHANDLE *pnote = (NOTEHANDLE*)lua_touserdata (L, -2);
	const char *str = lua_tostringex (L, -1);
	oapiAnnotationSetText (*pnote, (char*)str);
	return 0;
}

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

int Interpreter::noteSetSize (lua_State *L)
{
	NOTEHANDLE *pnote = (NOTEHANDLE*)lua_touserdata (L, 1);
	double size = lua_tonumber (L, 2);
	oapiAnnotationSetSize (*pnote, size);
	return 0;
}

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

int Interpreter::vesselGetFocusHandle (lua_State *L)
{
	lua_pushlightuserdata (L, oapiGetFocusObject());
	return 1;
}

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

int Interpreter::vesselGetFocusInterface (lua_State *L)
{
	VESSEL *v = oapiGetFocusInterface();
	lua_pushvessel (L, v);
	return 1;
}

int Interpreter::vesselGetCount (lua_State *L)
{
	lua_pushinteger (L, oapiGetVesselCount());
	return 1;
}

// ============================================================================
// MFD methods

int Interpreter::mfd_get_size (lua_State *L)
{
	MFD2 *mfd = lua_tomfd(L,1);
	ASSERT_SYNTAX(mfd, "Invalid MFD object");
	lua_pushnumber (L, mfd->GetWidth());
	lua_pushnumber (L, mfd->GetHeight());
	return 2;
}

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

int Interpreter::mfd_invalidate_display (lua_State *L)
{
	MFD2 *mfd = lua_tomfd(L,1);
	ASSERT_SYNTAX(mfd, "Invalid MFD object");
	mfd->InvalidateDisplay();
	return 0;
}

int Interpreter::mfd_invalidate_buttons (lua_State *L)
{
	MFD2 *mfd = lua_tomfd(L,1);
	ASSERT_SYNTAX(mfd, "Invalid MFD object");
	mfd->InvalidateButtons();
	return 0;
}

// ============================================================================
// LightEmitter methods

int Interpreter::le_get_position (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	VECTOR3 pos = le->GetPosition();
	lua_pushvector (L,pos);
	return 1;
}

int Interpreter::le_set_position (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	ASSERT_MTDVECTOR(L,2);
	VECTOR3 pos = lua_tovector(L,2);
	le->SetPosition (pos);
	return 0;
}

int Interpreter::le_get_direction (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	VECTOR3 dir = le->GetDirection();
	lua_pushvector (L,dir);
	return 1;
}

int Interpreter::le_set_direction (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	ASSERT_MTDVECTOR(L,2);
	VECTOR3 dir = lua_tovector(L,2);
	le->SetDirection (dir);
	return 0;
}

int Interpreter::le_get_intensity (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	double intens = le->GetIntensity();
	lua_pushnumber (L,intens);
	return 1;
}

int Interpreter::le_set_intensity (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	ASSERT_MTDNUMBER(L,2);
	double intens = lua_tonumber(L,2);
	le->SetIntensity (intens);
	return 0;
}

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

int Interpreter::le_activate (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	ASSERT_MTDBOOLEAN(L,2);
	int activate = lua_toboolean(L,2);
	le->Activate (activate != 0);
	return 0;
}

int Interpreter::le_is_active (lua_State *L)
{
	LightEmitter *le = lua_tolightemitter(L,1);
	ASSERT_SYNTAX(le, "Invalid emitter object");
	bool active = le->IsActive();
	lua_pushboolean (L,active);
	return 1;
}

// ============================================================================
// Sketchpad methods

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
	ASSERT_MTDNUMBER(L,5);
	len = (int)lua_tointeger(L,5);
	bool ok = skp->Text (x, y, str, len);
	lua_pushboolean (L, ok ? 1:0);
	return 1;
}

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

int Interpreter::skp_set_backgroundmode (lua_State *L)
{
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	ASSERT_MTDNUMBER(L,2);
	oapi::Sketchpad::BkgMode mode = (oapi::Sketchpad::BkgMode)lua_tointeger(L, 2);
	skp->SetBackgroundMode (mode);
	return 0;
}

int Interpreter::skp_set_pen (lua_State *L)
{
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	ASSERT_MTDLIGHTUSERDATA(L,2);
	oapi::Pen *pen = (oapi::Pen*)lua_touserdata(L,2);
	oapi::Pen *ppen = skp->SetPen (pen);
	if (ppen) lua_pushlightuserdata(L,ppen);
	else      lua_pushnil(L);
	return 1;
}

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

int Interpreter::skp_get_charsize (lua_State *L)
{
	oapi::Sketchpad *skp = lua_tosketchpad (L,1);
	ASSERT_SYNTAX(skp, "Invalid sketchpad object");
	DWORD size = skp->GetCharSize ();
	lua_pushnumber(L, LOWORD(size));
	lua_pushnumber(L, HIWORD(size));
	return 2;
}

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

// ============================================================================
// core thread functions

int OpenHelp (void *context)
{
	HELPCONTEXT *hc = (HELPCONTEXT*)context;
	oapiOpenHelp (hc);
	return 0;

}

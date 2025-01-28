// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef __QUADCOPTERLUA_H
#define __QUADCOPTERLUA_H

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}
class Quadcopter;

class LuaInterface {
public:
	LuaInterface() {}
	int InitInterpreter(void *context);
	int InitInstance(void *context);
	static Quadcopter *lua_toQC(lua_State *L, int idx);

	static int setDirectMode(lua_State *L);
	static int setAutoHeading(lua_State *L);
	static int setHeading(lua_State *L);
	static int setCourse(lua_State *L);
	static int setHspd(lua_State *L);
	static int setVspd(lua_State *L);
	static int setAlt(lua_State *L);
};

#endif // !__QUADCOPTERLUA_H

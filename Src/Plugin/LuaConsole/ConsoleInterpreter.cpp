// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "ConsoleInterpreter.h"

// ==============================================================
// Console interpreter class implementation

ConsoleInterpreter::ConsoleInterpreter (LuaConsole *_console): Interpreter ()
{
	is_term = true;
	console = _console;
}

void ConsoleInterpreter::LoadAPI ()
{
	Interpreter::LoadAPI();

	static const struct luaL_Reg termLib [] = {
		{"out", termOut},
		{"clear", termClear},
		{"lineup", termLineUp},
		{"SetVerbosity", termSetVerbosity},
		{NULL, NULL}
	};
	luaL_newlib(L, termLib);
	lua_setglobal(L, "term");
	lua_getglobal(L, "term");

	// also replace built-in print so it works as expected in the console
	static const struct luaL_Reg printlib[] = {
		{"print", termOut},
		{NULL, NULL}
	};
	lua_getglobal(L, "_G");
	luaL_setfuncs(L, printlib, 0);
}

void ConsoleInterpreter::term_strout (const char *str, bool iserr)
{
	if (strchr (str, '\n')) {
		char *cbuf = new char[strlen(str)+1];
		strcpy (cbuf, str);
		char *s = strtok (cbuf, "\n");
		while (s) {
			console->AddLine (s, iserr ? 2:1);
			s = strtok (NULL, "\n");
		}
		delete []cbuf;
	} else console->AddLine (str, iserr ? 2:1);
}

void ConsoleInterpreter::term_clear() {
	console->Clear();
}

int ConsoleInterpreter::termOut (lua_State *L)
{
	Interpreter *interp = GetInterpreter (L);
	interp->term_out (L);
	return 0;
}

int ConsoleInterpreter::termLineUp (lua_State *L)
{
#ifdef UNDEF
	if (g_Param.hConsoleTh) {
		HANDLE hStdO = GetStdHandle (STD_OUTPUT_HANDLE);
		if (hStdO) {
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			GetConsoleScreenBufferInfo (hStdO, &csbi);
			csbi.dwCursorPosition.Y--;
			SetConsoleCursorPosition (hStdO, csbi.dwCursorPosition);
		}
	}
#endif
	return 0;
}

int ConsoleInterpreter::termClear(lua_State *L)
{
	Interpreter *interp = GetInterpreter(L);
	interp->term_clear();
	return 0;
}

int ConsoleInterpreter::termSetVerbosity (lua_State *L)
{
	Interpreter *interp = GetInterpreter (L);
	int level = lua_tointeger (L, -1);
	interp->term_setverbosity (level);
	return 0;
}


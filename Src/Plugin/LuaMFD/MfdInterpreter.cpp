// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "MfdInterpreter.h"

// ==============================================================
// MFD interpreter class implementation

MFDInterpreter::MFDInterpreter (): Interpreter ()
{
	is_term = true;
	lineFirst = lineLast = NULL;
	nline = 0;
}

void MFDInterpreter::SetSelf (OBJHANDLE hV)
{
	VESSEL **pv = (VESSEL**)lua_newuserdata (L, sizeof(VESSEL*));
	*pv = oapiGetVesselInterface (hV);
	luaL_getmetatable (L, "VESSEL.vtable");  // push metatable
	lua_setmetatable (L, -2);               // set metatable for user data
	LoadVesselExtensions (L, *pv);
	lua_setfield (L, LUA_GLOBALSINDEX, "V");
	InitialiseVessel (L, *pv);
}

void MFDInterpreter::LoadAPI ()
{
	Interpreter::LoadAPI();

	static const struct luaL_reg termLib [] = {
		{"out", termOut},
		{"clear", termClear},
		//{"lineup", termLineUp},
		{"SetVerbosity", termSetVerbosity},
		{NULL, NULL}
	};
	luaL_openlib (L, "term", termLib, 0);

	// also replace built-in print so it works as expected in the MFD
	static const struct luaL_Reg printlib[] = {
		{"print", termOut},
		{NULL, NULL}
	};
	lua_getglobal(L, "_G");
	luaL_register(L, NULL, printlib);
}

void MFDInterpreter::term_strout (const char *str, bool iserr)
{
	AddLine (str, iserr ? 0x0000FF:0x00FF00);
}

void MFDInterpreter::term_out (lua_State *L, bool iserr)
{
	const char *str = lua_tostringex (L, -1);
	char *s0 = (char*)str, *s;
	for(;;) {
		s = strchr (s0, '\n');
		if (!s) {
			//AddLine (s0, 0x00FF00);
			term_strout (s0, iserr);
			break;
		} else {
			char cbuf[1024], *c, *cc;
			for (c = s0, cc = cbuf; c < s; c++) *cc++ = *c;
			*cc++ = '\0';
			//AddLine (cbuf, 0x00FF00);
			term_strout (cbuf, iserr);
			s0 = s+1;
		}
	}
}

int MFDInterpreter::termOut (lua_State *L)
{
	Interpreter *interp = GetInterpreter (L);
	interp->term_out (L);
	return 0;
}

void MFDInterpreter::term_clear ()
{
	// Clean terminal buffer
	while (lineFirst)
	{
		LineSpec *tmp = lineFirst;
		lineFirst = lineFirst->next;
		delete tmp;
	}
	// Set parameter for a 'clean' terminal
	lineFirst = lineLast = NULL;
	nline = 0;
}

int MFDInterpreter::termClear (lua_State *L)
{
	Interpreter *interp = GetInterpreter(L);
	interp->term_clear();
	return 0;
}

int MFDInterpreter::termSetVerbosity (lua_State *L)
{
	Interpreter *interp = GetInterpreter (L);
	int level = lua_tointeger (L, -1);
	interp->term_setverbosity (level);
	return 0;
}

void MFDInterpreter::AddLine (const char *line, COLORREF col)
{
	LineSpec *ls = new LineSpec;
	strncpy (ls->buf, line, NCHAR);
	ls->buf[NCHAR-1] = '\0';
	ls->col = col;
	ls->prev = lineLast;
	ls->next = NULL;
	if (lineLast) lineLast->next = ls;
	lineLast = ls;
	if (!lineFirst) lineFirst = ls;
	if (++nline > NLINE) {   // max line buffer size reached: drop first line
		LineSpec *tmp = lineFirst;
		lineFirst = lineFirst->next;
		delete tmp;
		nline--;
	}
}


// ==============================================================
// class InterpreterList::Environment: implementation

InterpreterList::Environment::Environment (OBJHANDLE hV)
{
	cmd[0] = '\0';
	interp = CreateInterpreter (hV);
}

InterpreterList::Environment::~Environment()
{
	if (interp) {
		if (hThread.joinable()) {
			interp->Terminate();
			interp->StepInterpreter();
			hThread.join();
		}
		delete interp;
	}
}

MFDInterpreter *InterpreterList::Environment::CreateInterpreter (OBJHANDLE hV)
{
	unsigned int id;
	interp = new MFDInterpreter ();
	interp->Initialise();
	interp->SetSelf (hV);
	hThread = std::thread(InterpreterThreadProc, this);
	return interp;
}

// Interpreter thread function
unsigned int InterpreterList::Environment::InterpreterThreadProc (void *context)
{
	InterpreterList::Environment *env = (InterpreterList::Environment*)context;
	MFDInterpreter *interp = (MFDInterpreter*)env->interp;

	// interpreter loop
	for (;;) {
		interp->WaitForStep(); // wait for execution permission
		if (interp->Status() == 1) break; // close thread requested
		interp->RunChunk (env->cmd, strlen (env->cmd)); // run command from buffer
		if (interp->Status() == 1) break;
		env->cmd[0] = '\0'; // free buffer
		interp->StepDone();  // return control
	}
	interp->StepDone();  // release the orbiter thread
	return 0;
}

// ==============================================================
// Interpreter repository implementation

InterpreterList::InterpreterList ()
{
	nlist = nbuf = 0;
}

InterpreterList::~InterpreterList ()
{
	DeleteList();
}

void InterpreterList::Update (double simt, double simdt, double mjd)
{
	DWORD i, j;
	for (i = 0; i < nlist; i++) {
		for (j = 0; j < list[i].nenv; j++) {
			Environment *env = list[i].env[j];
			if (env->interp->IsBusy() || env->cmd[0] || env->interp->nJobs()) { // let the interpreter do some work
				env->interp->StepInterpreter();
			}
			env->interp->PostStep (simt, simdt, mjd);
		}
	}
}

InterpreterList::Environment *InterpreterList::AddInterpreter (OBJHANDLE hV)
{
	VesselInterp *vi = FindVesselInterp (hV);

	if (!vi) { // no interpreters defined for this vessel yet
		if (nlist == nbuf) { // increase buffer size
			VesselInterp *tmp = new VesselInterp[nbuf+16];
			if (nbuf) {
				memcpy (tmp, list, nbuf*sizeof(VesselInterp));
				delete []list;
			}
			list = tmp;
			nbuf += 16;
		}
		vi = list + nlist++;
		vi->hVessel = hV;
		vi->nenv = 0;
	}

	// add the new interpreter to the vessel's interpreter list
	Environment **tmp = new Environment*[vi->nenv+1];
	if (vi->nenv) {
		memcpy (tmp, vi->env, vi->nenv*sizeof(Environment*));
		delete []vi->env;
	}
	vi->env = tmp;

	Environment *env = new Environment (hV);
	vi->env[vi->nenv++] = env;
	
	return env;
}

bool InterpreterList::DeleteInterpreter (OBJHANDLE hV, DWORD idx)
{
	VesselInterp *vi = FindVesselInterp (hV);
	if (!vi || idx >= vi->nenv) return false;
	if (vi->nenv <= 1) return false; // can't delete the last interpreter

	Environment *env = vi->env[idx];
	delete env;

	Environment **tmp = NULL;
	if (vi->nenv > 1) {
		DWORD i, j;
		tmp = new Environment*[vi->nenv-1];
		for (i = j = 0; i < vi->nenv; i++)
			if (i != idx) tmp[j++] = vi->env[i];
	}
	delete []vi->env;
	vi->env = tmp;
	vi->nenv--;
	return true;
}

bool InterpreterList::DeleteInterpreters (OBJHANDLE hV)
{
	VesselInterp *vi = FindVesselInterp (hV);
	if (!vi) return false;

	DWORD i;
	for (i = 0; i < vi->nenv; i++) {
		Environment *env = vi->env[i];
		delete env;
	}
	if (vi->nenv) {
		delete []vi->env;
		vi->env = 0;
		vi->nenv = 0;
	}
	return true;
}

void InterpreterList::DeleteList ()
{
	DWORD i, j;
	for (j = 0; j < nlist; j++) {
		VesselInterp *vi = list+j;
		for (i = 0; i < vi->nenv; i++) {
			Environment *env = vi->env[i];
			delete env;
		}
		if (vi->nenv) delete []vi->env;
	}
	if (nbuf) delete []list;
	nbuf = nlist = 0;
}

InterpreterList::VesselInterp *InterpreterList::FindVesselInterp (OBJHANDLE hV)
{
	DWORD i;
	for (i = 0; i < nlist; i++)
		if (list[i].hVessel == hV) return list+i;
	return NULL;
}

InterpreterList::Environment *InterpreterList::FindInterpreter (OBJHANDLE hV, DWORD idx)
{
	VesselInterp *vi = FindVesselInterp (hV);
	if (!vi) return NULL;
	return (vi->nenv > idx ? vi->env[idx] : NULL);
}

DWORD InterpreterList::InterpreterCount (OBJHANDLE hV)
{
	VesselInterp *vi = FindVesselInterp (hV);
	return (vi ? vi->nenv : 0);
}

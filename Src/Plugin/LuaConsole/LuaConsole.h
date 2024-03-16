// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef __LUACONSOLE_H
#define __LUACONSOLE_H

#include "OrbiterAPI.h"
#include "ModuleAPI.h"
#include "ConsoleInterpreter.h"
#include <thread>

#define NLINE 100 // number of buffered lines

class LuaConsole: public oapi::Module {
	friend class ConsoleInterpreter;
	friend class ConsoleConfig;

public:
	LuaConsole (HINSTANCE hDLL);
	~LuaConsole ();

	void clbkSimulationStart (RenderMode mode);
	void clbkSimulationEnd ();
	void clbkPreStep (double simt, double simdt, double mjd);

	HWND Open ();
	void Close ();
	void SetFontSize (DWORD size);
	void Resize (DWORD w, DWORD h);
	void RefreshTerminal ();
	void PaintTerminal ();

protected:
	bool SetParams ();
	void SetTermGeometry (HWND hTerm);
	void SetFontGeometry (HWND hTerm);
	void AutoScroll ();
	void ScrollTo (int pos);
	void ScrollBy (int dpos);
	void UpdateScrollbar ();
	LRESULT WINAPI TermProc (HWND, UINT, WPARAM, LPARAM);

private:
	static INT_PTR CALLBACK DlgProc (HWND, UINT, WPARAM, LPARAM);
	static LRESULT WINAPI TermProcHook (HWND, UINT, WPARAM, LPARAM);
	static unsigned int InterpreterThreadProc (LuaConsole* console);
	static void OpenDlgClbk (void *context); // called when user requests console window
	Interpreter *CreateInterpreter ();
	void AddLine (const char *str, int mode=1); // add line to buffer
	void Clear ();
	void InputLine (const char *str); // user input
	bool ScanHistory (int step); // recall previous command to input buffer
	std::thread hThread;    // interpreter thread handle
	bool termInterp;

	Interpreter *interp; // interpreter instance
	HWND hWnd;      // console window handle
	HWND hTerm;     // terminal text sub-window
	HFONT hFont;    // font resource
	DWORD fW, fH;   // font width, height
	DWORD dwCmd;    // custom command id

	struct LineSpec { // terminal history buffer
		char *buf;
		int mode;   // 0=input, 1=output, 2=error output
		//bool isInp;
	} line[NLINE];
	char *inp;      // input buffer
	int line0;      // buffer index of first line
	int hline;      // current history scan line
	int nline;      // number of lines in input buffer
	int ninp;       // number of characters in input buffer
	int tline;      // number of lines visible in terminal window
	int topline;    // topmost displayed line
	int caret;      // caret position in input buffer
	bool bRefresh;  // display refresh flag
	COLORREF col[3]; // colour for input/output/error text
};

#endif // !__LUA_CONSOLE_H
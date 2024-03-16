// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#define ORBITER_MODULE
#include "Orbitersdk.h"
#include "LuaConsole.h"
#include "ConsoleCfg.h"
#include "resource.h"
#include <set>

using std::min;
using std::max;
using namespace oapi;

// ==============================================================
// Global parameters

LuaConsole *g_Module = NULL;
ConsoleConfig *g_Config = NULL;

char cConsoleCmd[1024] = "\0";

// ==============================================================
// class LuaConsole

LuaConsole::LuaConsole (HINSTANCE hDLL): Module (hDLL)
{
	hWnd = NULL;
	interp = NULL;
	bRefresh = false;
	fW = 0;

	SetParams (); // may not be necessary here

	// Register a custom command for opening the console window
	dwCmd = oapiRegisterCustomCmd ((char*)"Lua console window",
		(char*)"Open a Lua script interpreter window.",
		OpenDlgClbk, this);

	// terminal history buffer
	for (DWORD i = 0; i < NLINE; i++) {
		line[i].buf = NULL;
		line[i].mode = 0;
	}
	line0 = 0;
	hline = 0;
	nline = 0;
	tline = 0;
	topline = 0;

	// input buffer
	inp = new char[1024];
	memset (inp, 0, 1024);
	caret = 0;
	ninp = 0;

	// Register a window class for the terminal display
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = TermProcHook;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hDLL;
	wc.hIcon = NULL;
	wc.hCursor = LoadCursor (NULL, IDC_IBEAM);
	wc.hbrBackground = (HBRUSH)GetStockObject (WHITE_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "ConsoleDsp";
	RegisterClass (&wc);
}

// ==============================================================

LuaConsole::~LuaConsole ()
{
	// Unregister the custom function in Orbiter
	oapiUnregisterCustomCmd (dwCmd);

	// Unregister the terminal window class
	UnregisterClass ("ConsoleDsp", hModule);

	// Delete terminal buffer
	for (DWORD i = 0; i < NLINE; i++)
		if (line[i].buf) delete []line[i].buf;

	// Delete input buffer
	delete []inp;
}

// ==============================================================

void LuaConsole::clbkSimulationStart (RenderMode mode)
{
	// Read configuration parameters, if available
	SetParams ();

	// GDI resources
	hFont = CreateFont (-(int)fH, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 3, 2, 1, 49, "Courier New");

	// make user-selectable
	col[0] = 0x000000;
	col[1] = 0x008000;
	col[2] = 0x0000FF;

	AddLine ("==== Orbiter Terminal (" LUA_RELEASE ") ====");
	AddLine ("Type 'help()' for help.");
	AddLine ("");
}

// ==============================================================

void LuaConsole::clbkSimulationEnd ()
{
	// Kill the interpreter thread
	if (interp) {
		termInterp = true;
		if (hThread.joinable()) {
			interp->Terminate();
			interp->StepInterpreter(); // give the thread opportunity to close
			hThread.join();
		}
		delete interp;
		interp = NULL;
	}

	// Free GDI resources
	DeleteObject (hFont);
}

// ==============================================================

void LuaConsole::clbkPreStep (double simt, double simdt, double mjd)
{
	if (interp) {
		if (interp->IsBusy() || cConsoleCmd[0] || interp->nJobs()) { // let the interpreter do some work
			interp->StepInterpreter();
		}
		if (bRefresh) {
			UpdateScrollbar();
			RefreshTerminal();
			bRefresh = false;
		}
		interp->PostStep (simt, simdt, mjd);
	}
}

// ==============================================================

HWND LuaConsole::Open ()
{
	// open the terminal dialog
	if (oapiFindDialog (hModule, IDD_CONSOLE)) return NULL; // console open already
	hWnd = oapiOpenDialogEx (hModule, IDD_CONSOLE, DlgProc, 0, this);
	hTerm = GetDlgItem (hWnd, IDC_TERM);

	// get some text parameters
	SetFontGeometry (hTerm);

	// get geometry information
	SetTermGeometry (hTerm);

	// create the interpreter and execution thread
	if (!interp)
		interp = CreateInterpreter ();

	return hWnd;
}

// ==============================================================

void LuaConsole::Close ()
{
	// Note that we keep the interpreter running when closing the
	// console window. Any active tasks (e.g. autopilots will continue
	// running in the background.

	oapiCloseDialog (hWnd);
	hWnd = NULL;
	hTerm = NULL;
	tline = 0;
}

// ==============================================================

bool LuaConsole::SetParams ()
{
	// Set defaults in case they are not defined in the file
	fH = 14; // font size

	if (!g_Config) return false;
	fH = g_Config->fontsize;
	return true;
}

// ==============================================================

void LuaConsole::SetFontSize (DWORD size)
{
	if (size == fH) return; // nothing to do
	fH = size;
	if (hFont) DeleteObject (hFont);
	hFont = CreateFont (-(int)fH, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 3, 2, 1, 49, "Courier New");

	// get some text parameters
	SetFontGeometry (hTerm);
}

// ==============================================================

void LuaConsole::Resize (DWORD w, DWORD h)
{
	if (hTerm) {
		SetWindowPos (hTerm, hWnd,
			0, 0, w, h,
			SWP_NOZORDER);
	}
	SetTermGeometry (hTerm);

	// adjust scrollbar
	UpdateScrollbar();
}

// ==============================================================

void LuaConsole::SetFontGeometry (HWND hTerm)
{
	HDC hDC = GetDC (hTerm);
	HFONT pFont = (HFONT)SelectObject (hDC, hFont);
	if (!fW) {
		TEXTMETRIC tm;
		GetTextMetrics (hDC, &tm);
		fW = tm.tmAveCharWidth;
	}
	SelectObject (hDC, pFont);
	ReleaseDC (hTerm, hDC);
}

// ==============================================================

void LuaConsole::SetTermGeometry (HWND hTerm)
{
	RECT rc;
	GetClientRect (hTerm, &rc);
	int w = rc.right, h = rc.bottom;
	tline = h/fH; // terminal lines
}

// ==============================================================

void LuaConsole::RefreshTerminal ()
{
	if (hTerm) {
		InvalidateRect (hTerm, NULL, TRUE);
		UpdateWindow (hTerm);
		PaintTerminal ();
	}
}

// ==============================================================

void LuaConsole::PaintTerminal ()
{
	if (!tline) return;

	bool bPrompt = !interp->IsBusy();
	DWORD i, idx, x0, x, y;
	DWORD nnline = nline;
	if (bPrompt) nnline++;
	HFONT pFont;
	HDC hDC = GetDC (hTerm);
	SelectObject (hDC, GetStockObject (NULL_BRUSH));
	SelectObject (hDC, GetStockObject (BLACK_PEN));
	pFont = (HFONT)SelectObject (hDC, hFont);
	SetTextColor (hDC, col[0]);
	int pmode = 0;
	SetBkMode (hDC, TRANSPARENT);
	x0 = x = 2; y = 1;
	int dtop = (topline-line0+NLINE)%NLINE;
	DWORD ndisp = min (nline-dtop,tline-1);
	for (i = 0; i < ndisp; i++) {
		idx = (topline+i)%NLINE;
		if (!i || line[idx].mode != pmode) {
			pmode = line[idx].mode;
			SetTextColor (hDC, col[pmode]);
			x = (!pmode ? x0+fW : x0);
		}
		if (!pmode) TextOut (hDC, x0, y, "%", 1);
		TextOut (hDC, x, y, line[idx].buf, strlen(line[idx].buf));
		y += fH;
	}
	if (bPrompt && nline-dtop >= 0 && nline-dtop < tline) {
		x = x0+fW;
		if (pmode) SetTextColor (hDC, col[0]);
		TextOut (hDC, x0, y, "%", 1);
		if (inp[0])
			TextOut (hDC, x, y, inp, ninp);
		// draw caret
		MoveToEx (hDC, x+fW*caret, y, NULL);
		LineTo (hDC, x+fW*caret, y+fH);
	}

	SelectObject (hDC, pFont);
	ReleaseDC (hTerm, hDC);
}

// ==============================================================

void LuaConsole::Clear ()
{
	// Clean terminal history buffer, keeping only *unique* *inputs*
	std::set<std::string> s;
	for (int i = 0, j = 0; i < nline/*NLINE*/; ++i)
	{
		if (!line[i].mode &&                 // mode == input ...AND...
		     line[i].buf && *line[i].buf &&  // buffer neither NULL nor empty ...AND...
		     s.insert(line[i].buf).second )  // unique
		{                                    // => keep!
			if (i != j) // move?
			{
				line[j] = line[i];
				line[i].buf = NULL;
			}
			++j;
		}
		else // remove!
		{
			delete[] line[i].buf;
			line[i].buf = NULL;
			line[i].mode = 0;
		}
	}

	// Set parameter for a 'clean' terminal
	line0 = 0;         // buffer index of first line

	nline =            // number of lines in input buffer
	topline =          // topmost displayed line
	hline = s.size();  // current history scan line

	PaintTerminal();
}

// ==============================================================

void LuaConsole::AddLine (const char *str, int mode)
{
	int idx = (line0+nline)%NLINE;
	LineSpec *ln = line + idx;
	if (ln->buf) delete []ln->buf;
	int ncol = strlen(str);
	ln->mode = mode;
	ln->buf = new char[ncol+1];
	strcpy (ln->buf, str);
	int dtop = (topline-line0+NLINE)%NLINE;
	int ddsp = nline-dtop;
	bool vis = (ddsp >= 0 && ddsp < tline);
	if (nline == NLINE) line0 = (line0+1)%NLINE;
	else nline++;
	if (!mode || vis) AutoScroll();
	bRefresh = true;
}

// ==============================================================

void LuaConsole::AutoScroll ()
{
	if (!tline) return;
	int dtop = (topline-line0+NLINE)%NLINE;
	if (nline < dtop) {
		ScrollTo ((line0+dtop)%NLINE);
	} else if (nline-dtop >= tline) {
		ScrollTo ((line0+nline-tline+1)%NLINE);
	}
}

// ==============================================================

void LuaConsole::ScrollTo (int pos)
{
	pos = max (0, min (nline-1, pos));
	pos = (pos+line0)%NLINE;
	if (pos != topline) {
		topline = pos;
		bRefresh = true;
	}
}

// ==============================================================

void LuaConsole::ScrollBy (int dpos)
{
	int dtop = (topline-line0+NLINE)%NLINE;
	int pos = dtop+dpos;
	ScrollTo (pos);
}

// ==============================================================

void LuaConsole::UpdateScrollbar ()
{
	if (!tline) return;
	static SCROLLINFO si = {
		sizeof(SCROLLINFO),
		SIF_POS | SIF_RANGE,
		0, 0, 0, 0, 0
	};
	si.nMax = max (0, nline-1);
	si.nPos = (topline-line0+NLINE)%NLINE;
	SetScrollInfo (hTerm, SB_VERT, &si, TRUE);
}

// ==============================================================

void LuaConsole::InputLine (const char *str)
{
	AddLine (str, 0);
	hline = nline;
	strcpy (cConsoleCmd, str);
}

// ==============================================================

bool LuaConsole::ScanHistory (int step)
{
	DWORD ln = nline, phline = hline;
	bool found = false;
	if (step < 0) { // step back
		if (!hline) return false;
		for (--hline; hline != (DWORD)-1; hline--) {
			ln = (line0+hline)%NLINE;
			if (!line[ln].mode && line[ln].buf) { found = true; break; }
		}
	} else { // step forward
		if (hline == nline) return false;
		for (++hline; hline != nline; hline++) {
			ln = (line0+hline)%NLINE;
			if (!line[ln].mode && line[ln].buf) { found = true; break; }
		}
	}
	if (found) {
		if (ln == nline) {
			inp[0] = '\0';
			ninp = caret = 0;
		} else {
			strncpy (inp, line[ln].buf, 1024);
			ninp = caret = min (strlen (line[ln].buf), (size_t)1024);
		}
	} else if (step > 0) {
		inp[0] = '\0';
		ninp = caret = 0;
	} else {
		hline = phline;
		return false;
	}

	bRefresh = true;
	return true;
}

// ==============================================================

LRESULT WINAPI LuaConsole::TermProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_CHAR:
		switch (wParam) {
		case 8:  // Backspace
			if (caret) {
				for (int i = caret; i < ninp; i++)
					inp[i-1] = inp[i];
				caret--; ninp--;
			} else return 0;
			break;
		case 13: // Enter
			inp[ninp] = '\0';
			InputLine (inp);
			inp[0] = '\0';
			caret = ninp = 0;
			return 0;
		default:
			if (isprint (wParam)) {
				for (int i = ninp; i > caret; i--)
					inp[i] = inp[i-1];
				inp[caret++] = (char)wParam;
				ninp++;
			//} else { // debugging only
			//	sprintf (oapiDebugString(), "c=%c, id=%d", wParam, wParam);
			}
			break;
		}
		AutoScroll();
		RefreshTerminal();
		return 0;
	case WM_KEYDOWN:
		switch (wParam) {
		case 37: // left arrow
			if (caret) caret--;
			else return 0;
			break;
		case 39: // right arrow
			if (caret < ninp) caret++;
			else return 0;
			break;
		case 38: // up arrow (command history)
			if (!ScanHistory(-1)) return 0;
			break;
		case 40: // down arrow (command history)
			if (!ScanHistory(1)) return 0;
			break;
		case 46: // Del
			if (ninp > caret) {
				ninp--;
				for (int i = caret; i < ninp; ++i)
					inp[i] = inp[i + 1];
			} else return 0;
			break;
		case 35: // End
			caret = ninp;
			break;
		case 36: // Pos1
			caret = 0;
			break;
		//default: // debugging only
		//	sprintf (oapiDebugString(), "virt=%d", wParam);
		//	return 0;
		}
		AutoScroll();
		RefreshTerminal();
		return 0;
	case WM_VSCROLL:
		switch (LOWORD(wParam)) {
		case SB_THUMBPOSITION:
		case SB_THUMBTRACK:
			ScrollTo (HIWORD(wParam));
			break;
		case SB_LINEDOWN:
			ScrollBy (1);
			break;
		case SB_LINEUP:
			ScrollBy (-1);
			break;
		case SB_PAGEDOWN:
			ScrollBy (10);
			break;
		case SB_PAGEUP:
			ScrollBy (-10);
			break;
		}
		return 0;
	}
	return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

// ==============================================================

LRESULT WINAPI LuaConsole::TermProcHook (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LuaConsole *pConsole = (LuaConsole*)GetWindowLongPtr (hWnd, GWLP_USERDATA);
	return pConsole->TermProc (hWnd, uMsg, wParam, lParam);
}

// ==============================================================

INT_PTR CALLBACK LuaConsole::DlgProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LuaConsole *pConsole = (LuaConsole*)oapiGetDialogContext (hWnd);

	switch (uMsg) {
	case WM_INITDIALOG:
		SetFocus (GetDlgItem (hWnd, IDC_TERM));
		pConsole = (LuaConsole*)lParam;
		SetWindowLongPtr (GetDlgItem (hWnd, IDC_TERM), GWLP_USERDATA, (LONG_PTR)pConsole);
		return FALSE;
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDCANCEL:
			pConsole->Close();
			return TRUE;
		}
		break;
	case WM_PAINT:
		pConsole->RefreshTerminal();
		break;
	case WM_SIZE:
		pConsole->Resize (LOWORD(lParam), HIWORD(lParam));
		return 0;
	}
	return oapiDefDialogProc (hWnd, uMsg, wParam, lParam);
}

// ==============================================================

void LuaConsole::OpenDlgClbk (void *context)
{
	LuaConsole *pConsole = (LuaConsole*)context;
	pConsole->Open();
}

// ==============================================================
// DLL entry and exit points

DLLCLBK void InitModule (HINSTANCE hDLL)
{
	// Create the configurator
	g_Config = new ConsoleConfig (hDLL);
	oapiRegisterLaunchpadItem (g_Config, NULL);

	// Create the console instance
	g_Module = new LuaConsole (hDLL);
	oapiRegisterModule (g_Module);
}

DLLCLBK void ExitModule (HINSTANCE hDLL)
{
	// Delete the configurator
	oapiUnregisterLaunchpadItem (g_Config);
	delete g_Config;
}

// ==============================================================

Interpreter *LuaConsole::CreateInterpreter ()
{
	termInterp = false;
	interp = new ConsoleInterpreter (this);
	interp->Initialise();
	hThread = std::thread(InterpreterThreadProc, this);
	return interp;
}
// Interpreter thread function
unsigned int LuaConsole::InterpreterThreadProc (LuaConsole* console)
{
	int res;
	ConsoleInterpreter *interp = (ConsoleInterpreter*)console->interp;

	// interpreter loop
	for (;;) {
		interp->WaitForStep(); // wait for execution permission
		if (console->termInterp) break; // close thread requested
		res = interp->RunChunk (cConsoleCmd, strlen (cConsoleCmd)); // run command from buffer
		if (interp->Status() == 1) break; // close thread requested
		cConsoleCmd[0] = '\0';    // free buffer
		console->bRefresh = (res != -1); // signal terminal refresh
		interp->StepDone();        // return control
	}
	interp->StepDone();  // wake the main thread when we die
	return 0;
}

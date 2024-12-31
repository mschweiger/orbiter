// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
// Help window
// ======================================================================

#define STRICT 1

#include "DlgHelp.h"
#include "DlgMgr.h"
#include "Orbiter.h"
#include "resource.h"
#include <htmlhelp.h>
#include <io.h>

extern Orbiter *g_pOrbiter;
extern HELPCONTEXT DefHelpContext;

static char helpf[256] = "html/orbiter.chm";
static char deftopic[256] = "html/orbiter.chm::/intro.htm";
static char vstopic[256] = "";
static char jmp2url[256] = "";
static char topic[256] = "/intro.htm";
static DWORD_PTR pTopic = (DWORD_PTR)topic;
static bool  bScnHelp = false;
static bool  bVsHelp = false;
static HWND hHelpClient = NULL;

// ======================================================================

DlgHelp::DlgHelp (HINSTANCE hInstance, HWND hParent, void *context)
: DialogWin (hInstance, hParent, IDD_HELP, (DLGPROC)DlgProc, 0, context)
{
	hfooter = 0;
	RegisterClientClass (hInstance);
}

// ======================================================================

DlgHelp::~DlgHelp ()
{
	UnregisterClientClass (hInst);
}

// ======================================================================

void DlgHelp::RegisterClientClass (HINSTANCE hInstance)
{
	// Register help window class
	WNDCLASS wndClass;
	wndClass.style         = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc   = ClientProc;
	wndClass.cbClsExtra    = 0;
	wndClass.cbWndExtra    = 0;
	wndClass.hInstance     = hInstance;
	wndClass.hIcon         = NULL;
	wndClass.hCursor       = NULL;
	wndClass.hbrBackground = (HBRUSH)GetStockObject (HOLLOW_BRUSH);
	wndClass.lpszMenuName  = NULL;
	wndClass.lpszClassName = "OrbiterHelp";
	RegisterClass (&wndClass);
}

// ======================================================================

void DlgHelp::UnregisterClientClass (HINSTANCE hInstance)
{
	UnregisterClass ("OrbiterHelp", hInstance);
}

// ======================================================================

void DlgHelp::SetScenarioHelp (const char *_helpf)
{
	if (_helpf) {
		char cbuf[256];
		strcpy (cbuf, _helpf);
		sprintf (helpf, "html/Scenarios/%s.chm", strtok (cbuf, ","));
		if (_access(helpf,0) == 0) {
			sprintf (deftopic, "%s::/%s.htm", helpf, strtok (NULL, ","));
			pTopic = 0;
			bScnHelp = true;
			return;
		}
	}
	strcpy (helpf, "html/orbiter.chm");
	strcpy (deftopic, "html/orbiter.chm::/intro.htm");
	pTopic = (DWORD_PTR)topic;
	bScnHelp = false;
}

// ======================================================================

void DlgHelp::SetVesselHelp (const char *_helpf)
{
	if (_helpf) {
		char cbuf[256];
		strcpy (cbuf, _helpf);
		sprintf (vstopic, "html/Vessels/%s.chm", strtok (cbuf, ","));
		sprintf (jmp2url, "%s::/%s.htm", vstopic, strtok (NULL, ","));
		bVsHelp = true;
	} else {
		bVsHelp = false;
	}
}

// ======================================================================

void DlgHelp::InitHelp (HWND hWnd, HELPCONTEXT *hcontext)
{
	HH_WINTYPE hhwt;
	memset (&hhwt, 0, sizeof(hhwt));
	hhwt.cbStruct = sizeof (hhwt);
	hhwt.fUniCodeStrings = FALSE;
	hhwt.pszType = "Default";
	hhwt.fsValidMembers = HHWIN_PARAM_PROPERTIES | HHWIN_PARAM_TB_FLAGS |
		HHWIN_PARAM_STYLES | HHWIN_PARAM_EXSTYLES;
	hhwt.fsWinProperties = HHWIN_PROP_TRI_PANE | HHWIN_PROP_NOTITLEBAR |
		HHWIN_PROP_TAB_SEARCH | HHWIN_PROP_AUTO_SYNC | HHWIN_PROP_NODEF_STYLES;
	hhwt.pszCaption = "Orbiter Help";
	hhwt.dwStyles = WS_CHILD | WS_VISIBLE;
	hhwt.dwExStyles = WS_EX_NOPARENTNOTIFY;
	hhwt.nShowState = WS_VISIBLE;
	hhwt.hwndHelp = NULL;
	hhwt.hwndCaller = hWnd;
	hhwt.hwndToolBar = NULL;
	hhwt.hwndNavigation = NULL;
	hhwt.hwndHTML = NULL;
	hhwt.iNavWidth = 0;
	if (hcontext) {
		hhwt.pszToc = (hcontext->toc ? hcontext->toc : "html/orbiter.chm::/Orbiter.hhc");
		hhwt.pszIndex = (hcontext->index ? hcontext->index : "html/orbiter.chm::/Orbiter.hhk");
		hhwt.pszFile = hcontext->helpfile;
		hhwt.pszHome = "html/orbiter.chm::/intro.htm";
	} else {
		hhwt.pszToc = "html/orbiter.chm::/Orbiter.hhc";
		hhwt.pszIndex = "html/orbiter.chm::/Orbiter.hhk";
		hhwt.pszFile = helpf;
		hhwt.pszHome = "html/orbiter.chm::/intro.htm";
	}
	hhwt.fsToolBarFlags = HHWIN_BUTTON_EXPAND | HHWIN_BUTTON_BACK |
		HHWIN_BUTTON_FORWARD | HHWIN_BUTTON_HOME;
	if (bScnHelp) hhwt.fsToolBarFlags |= HHWIN_BUTTON_JUMP1;
	if (bVsHelp)  hhwt.fsToolBarFlags |= HHWIN_BUTTON_JUMP2;
	hhwt.fNotExpanded = FALSE;
	hhwt.curNavType = 0;
	hhwt.tabpos = 0;
	hhwt.idNotify = 0;
	hhwt.cHistory = 0;
	hhwt.pszJump1 = "Scenario";
	hhwt.pszJump2 = "Vessel";
	hhwt.pszUrlJump1 = deftopic;
	hhwt.pszUrlJump2 = jmp2url;
	hhwt.rcMinSize.left = 0;
	hhwt.rcMinSize.right = 0;
	hhwt.rcMinSize.top = 0;
	hhwt.rcMinSize.bottom = 0;

	HtmlHelp (hWnd, NULL, HH_SET_WIN_TYPE, (DWORD_PTR)&hhwt);
}

// ======================================================================

BOOL DlgHelp::OnInitdialog (HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	RECT r, rh;
	GetClientRect (hWnd, &r);
	dlgw = r.right, dlgh = r.bottom;
	GetWindowRect (GetDlgItem (hWnd, IDC_CUSTOM1), &rh);
	hfooter = r.bottom - (rh.bottom-rh.top);
	GetWindowRect (GetDlgItem (hWnd, IDCANCEL), &rh);
	pClose.x = rh.left, pClose.y = rh.top;
	ScreenToClient (hWnd, &pClose);

	return OnRequest (hWnd, wParam, lParam);
}

// ======================================================================

BOOL DlgHelp::OnRequest (HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	SendDlgItemMessage (hWnd, IDC_CUSTOM1, WM_USER, wParam, lParam);
	return TRUE;
}

// ======================================================================

BOOL DlgHelp::OnSize (HWND hWnd, WPARAM wParam, int w, int h)
{
	RECT r;
	GetClientRect (hWnd, &r);
	int dx = r.right - dlgw, dy = r.bottom - dlgh;
	SetWindowPos (GetDlgItem (hWnd, IDC_CUSTOM1), HWND_TOP, 0, 0, r.right, r.bottom-hfooter, SWP_SHOWWINDOW);
	SetWindowPos (GetDlgItem (hWnd, IDCANCEL), HWND_TOP, pClose.x+dx, pClose.y+dy, 0, 0, SWP_NOSIZE);
	return DialogWin::OnSize (hWnd, wParam, w, h);
}

// ======================================================================

INT_PTR CALLBACK DlgHelp::DlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG:
		return ((DlgHelp*)GetDialogWin (hDlg))->OnInitdialog (hDlg, wParam, lParam);
	case WM_USER+1:
		return ((DlgHelp*)GetDialogWin (hDlg))->OnRequest (hDlg, wParam, lParam);
	case WM_SIZE:
		return ((DlgHelp*)GetDialogWin (hDlg))->OnSize (hDlg, wParam, LOWORD (lParam), HIWORD (lParam));
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDHELP:
			DefHelpContext.topic = (char*)"/help.htm";
			g_pOrbiter->OpenHelp (&DefHelpContext);
			return TRUE;
		case IDCANCEL:
			g_pOrbiter->CloseDialog (hDlg);
			return TRUE;
		}
		break;
	}
	return OrbiterDefDialogProc (hDlg, uMsg, wParam, lParam);
}

// ======================================================================

LRESULT FAR PASCAL DlgHelp::ClientProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	RECT r;

	switch (msg) {
	case WM_CREATE:
		//InitHelp (hwnd);
		return 0;
	case WM_USER: {
		char cbuf[256];
		if (lParam == 0) {
			DlgHelp::InitHelp (hwnd);
			strcpy (cbuf, helpf); strcat (cbuf, ">Default");
			hHelpClient = HtmlHelp (hwnd, cbuf, HH_DISPLAY_TOPIC, pTopic);
		} else {
			HELPCONTEXT *hcontext = (HELPCONTEXT*)lParam;
			InitHelp (hwnd, hcontext);
			strcpy (cbuf, hcontext->helpfile);
			strcat (cbuf, ">Default");
			hHelpClient = HtmlHelp (hwnd, cbuf, HH_DISPLAY_TOPIC, (DWORD_PTR)hcontext->topic);
		}
		GetClientRect (hwnd, &r);
		SetWindowPos (hHelpClient, HWND_TOP, 0, 0, r.right, r.bottom, 0);
		} break;
	case WM_SIZE:
	case WM_PAINT:
		GetClientRect (hwnd, &r);
		SetWindowPos (hHelpClient, HWND_TOP, 0, 0, r.right, r.bottom, SWP_HIDEWINDOW);
		SetWindowPos (hHelpClient, HWND_TOP, 0, 0, r.right, r.bottom, SWP_SHOWWINDOW);
		// there must be something better! (everything else I tried produces artefacts)
		break;
	case WM_NOTIFY:
	//	if (wParam == NEWTOPIC) {
	//		InitHelp (hwnd);
	//		hHelpClient = HtmlHelp (hwnd, "orbiter.chm>Default", HH_DISPLAY_TOPIC, (DWORD)topic);
	//	}
		return TRUE;
	}
	return DefWindowProc (hwnd, msg, wParam, lParam);
}


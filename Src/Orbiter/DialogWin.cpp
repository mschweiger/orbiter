// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "DialogWin.h"
#include "DlgMgr.h"
#include "OrbiterAPI.h"
#include "Orbiter.h"
#include "resource.h"
#include "Log.h"

#define DLG_CAPTIONBUTTON (DLG_CAPTIONCLOSE|DLG_CAPTIONHELP)

extern Orbiter *g_pOrbiter;

static int x_sizeframe = GetSystemMetrics (SM_CXSIZEFRAME);
static int y_sizeframe = GetSystemMetrics (SM_CYSIZEFRAME);
static int x_fixedframe = GetSystemMetrics (SM_CXFIXEDFRAME);
static int y_fixedframe = GetSystemMetrics (SM_CYFIXEDFRAME);

DialogWin *DialogWin::dlg_create = 0;

// ======================================================================

DialogWin::DialogWin (HINSTANCE hInstance, HWND hParent, int resourceId,
					  DLGPROC pDlg, DWORD flags, void *pContext)
{
	gc      = g_pOrbiter->GetGraphicsClient();
	hInst   = hInstance;
	resId   = resourceId;
	flag    = flags;
	context = pContext;
	hPrnt   = hParent;
	hWnd    = NULL;
	pos     = NULL;
	dlgproc = (pDlg ? pDlg : DlgProc);

	memset (tbtn, 0, 5*sizeof(TitleBtn));
	//int i = 0;
	//if (flag & DLG_CAPTIONCLOSE) tbtn[i++].DlgMsg = IDCANCEL;
	//if (flag & DLG_CAPTIONHELP)  tbtn[i++].DlgMsg = IDHELP;
}

// ======================================================================

DialogWin::DialogWin (HINSTANCE hInstance, HWND hWindow, HWND hParent, DWORD flags)
{
	gc      = g_pOrbiter->GetGraphicsClient();
	hInst   = hInstance;
	resId   = 0;
	flag    = flags;
	context = 0;
	hWnd    = hWindow;
	hPrnt   = hParent;
	dlgproc = NULL;

	memset (tbtn, 0, 5*sizeof(TitleBtn));
	//int i = 0;
	//if (flag & DLG_CAPTIONCLOSE) tbtn[i++].DlgMsg = IDCANCEL;
	//if (flag & DLG_CAPTIONHELP)  tbtn[i++].DlgMsg = IDHELP;
}

// ======================================================================

DialogWin::~DialogWin ()
{
	if (hWnd) {
		if (!DestroyWindow(hWnd))
			LOGOUT_LASTERR();
	}
}

// ======================================================================

HWND DialogWin::OpenWindow ()
{
	bool newwin = false;
	dlg_create = this; // is this still necessary ?

	if (gc) gc->clbkPreOpenPopup();

	if (!hWnd) { // otherwise window exists already
		hWnd = CreateDialogParam (hInst, MAKEINTRESOURCE(resId), hPrnt, dlgproc,
			(LPARAM)context);
		newwin = true;
	}
	SetWindowLongPtr (hWnd, DWLP_USER, (LONG_PTR)this);
	if (newwin && pos && pos->right-pos->left) {
		if (GetWindowLongPtr (hWnd, GWL_STYLE) & WS_SIZEBOX)
			SetWindowPos (hWnd, NULL, pos->left, pos->top, pos->right-pos->left, pos->bottom-pos->top, SWP_NOZORDER);
		else
			SetWindowPos (hWnd, NULL, pos->left, pos->top, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
	}

	RECT r;
	ShowWindow (hWnd, SW_SHOWNOACTIVATE);
	GetWindowRect (hWnd, &r);
	psize = r.bottom - r.top;

	dlg_create = 0;

	return hWnd;
}

// ======================================================================

void DialogWin::Update ()
{
}

// ======================================================================

INT_PTR CALLBACK DialogWin::DlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	BOOL res = MSG_DEFAULT;
	switch (uMsg) {
	case WM_INITDIALOG:
		res = GetDialogWin (hDlg)->OnInitDialog (hDlg, wParam, lParam);
		break;
	case WM_MOVE:
		res = GetDialogWin (hDlg)->OnMove (hDlg, LOWORD(lParam), HIWORD(lParam));
		break;
	case WM_SIZE:
		res = GetDialogWin (hDlg)->OnSize (hDlg, wParam, LOWORD(lParam), HIWORD(lParam));
		break;
	case WM_COMMAND:
		res = GetDialogWin (hDlg)->OnCommand (hDlg, LOWORD(wParam), HIWORD(wParam), (HWND)lParam);
		break;
	case WM_HSCROLL:
		res = GetDialogWin (hDlg)->OnHScroll (hDlg, LOWORD(wParam), HIWORD(wParam), (HWND)lParam);
		break;
	case WM_VSCROLL:
		res = GetDialogWin (hDlg)->OnVScroll(hDlg, LOWORD(wParam), HIWORD(wParam), (HWND)lParam);
		break;
	case WM_NOTIFY:
		res = GetDialogWin (hDlg)->OnNotify (hDlg, (int)wParam, (LPNMHDR)lParam);
		break;
	case WM_MOUSEWHEEL:
		res = GetDialogWin (hDlg)->OnMouseWheel (hDlg, LOWORD(wParam), HIWORD(wParam), LOWORD(lParam), HIWORD(lParam));
		break;
	case WM_LBUTTONDBLCLK:
		res = GetDialogWin (hDlg)->OnLButtonDblClk (hDlg, wParam, LOWORD(lParam), HIWORD(lParam));
		break;
	case WM_APP:
		res = GetDialogWin (hDlg)->OnApp (hDlg, wParam, lParam);
		break;
	case WM_USER+1:
		res = GetDialogWin (hDlg)->OnUser1 (hDlg, wParam, lParam);
		break;
	case WM_USERMESSAGE:
		res = GetDialogWin (hDlg)->OnUserMessage (hDlg, wParam, lParam);
		break;
	}
	return (res != MSG_DEFAULT ? res : OrbiterDefDialogProc (hDlg, uMsg, wParam, lParam));
}

// ======================================================================

BOOL DialogWin::OnCommand (HWND hDlg, WORD id, WORD code, HWND hControl)
{
	switch (id) {
	case IDCANCEL:
		g_pOrbiter->CloseDialog (hDlg);
		return TRUE;
	}
	return MSG_DEFAULT;
}

// ======================================================================

int DialogWin::OnSize (HWND hWnd, WPARAM wParam, int w, int h)
{
	if (pos) GetWindowRect (hWnd, pos);
	return 0;
}

// ======================================================================

int DialogWin::OnMove (HWND hWnd, int x, int y)
{
	if (pos) GetWindowRect (hWnd, pos);
	return 0;
}

// ======================================================================

void DialogWin::Message (DWORD msg, void *data)
{
	PostMessage (hWnd, WM_USERMESSAGE, msg, (LPARAM)data);
}

// ======================================================================

void DialogWin::ToggleShrink ()
{
	RECT r;
	GetWindowRect (hWnd, &r);
	int hw = r.bottom - r.top;
	int h0 = GetSystemMetrics (SM_CYMIN);
	if (hw == h0) { // restore window
		hw = psize;
		SetWindowPos (hWnd, 0, r.left, r.top, r.right-r.left, hw, SWP_SHOWWINDOW);
	} else {
		psize = hw;
		SetWindowPos (hWnd, 0, r.left, r.top, r.right-r.left, h0, SWP_SHOWWINDOW);
	}
}

// ======================================================================

DialogWin *DialogWin::GetDialogWin (HWND hDlg)
{
	DialogWin *dlg = (DialogWin*)GetWindowLongPtr (hDlg, DWLP_USER);
	if (!dlg)
		dlg = dlg_create;
	return dlg;
}

// ======================================================================

bool DialogWin::AddTitleButton (DWORD msg, HBITMAP hBmp, DWORD flag)
{
	for (int i = 0; i < 5; i++) {
		if (tbtn[i].DlgMsg == 0) {
			tbtn[i].DlgMsg = msg;
			tbtn[i].hBmp = hBmp;
			tbtn[i].flag = flag;
			return true;
		}
	}
	return false;
}

// ======================================================================

DWORD DialogWin::GetTitleButtonState (DWORD msg)
{
	for (int i = 0; i < 5; i++)
		if (tbtn[i].DlgMsg == msg)
			return (tbtn[i].flag & 0x80000000 ? 1:0);
	return 0;
}

// ======================================================================

bool DialogWin::SetTitleButtonState (DWORD msg, DWORD state)
{
	for (int i = 0; i < 5; i++)
		if (tbtn[i].DlgMsg == msg) {
			if (tbtn[i].flag & DLG_CB_TWOSTATE) {
				DWORD oldstate = (tbtn[i].flag & 0x80000000 ? 1:0);
				if (oldstate != state) {
					tbtn[i].flag ^= 0x80000000;
					PaintTitleButtons ();
					PostMessage (hWnd, WM_COMMAND, MAKELONG (tbtn[i].DlgMsg, state), 0);
					return true;
				}
			}
			return false;
		}
	return false;
}

// ======================================================================

void DialogWin::PaintTitleButtons ()
{
	if (!(flag & DLG_CAPTIONBUTTON)) return;
	RECT r;
	int x0, y0;
	GetWindowRect (hWnd, &r);
	if (GetWindowLongPtr (hWnd, GWL_STYLE) & WS_THICKFRAME) {
		x0 = -y_sizeframe,  y0 = x_sizeframe;
	} else {
		x0 = -y_fixedframe, y0 = x_fixedframe;
	}
	x0 += r.right-r.left-15;
	HDC hDC = GetWindowDC (hWnd);
	HDC hDCsrc = CreateCompatibleDC (hDC);
	HBITMAP hBmp = (HBITMAP)LoadImage (g_pOrbiter->GetInstance(), MAKEINTRESOURCE(IDB_DEFBUTTON), IMAGE_BITMAP, 15, 30, 0);
	SelectObject (hDCsrc, hBmp);
	int i = 0;
	if (flag & DLG_CAPTIONCLOSE) {
		BOOL res = BitBlt (hDC, x0, y0, 15, 15, hDCsrc, 0, 0, SRCCOPY);
		x0 -= 16;
		i++;
	}
	if (flag & DLG_CAPTIONHELP) {
		BitBlt (hDC, x0, y0, 15, 15, hDCsrc, 0, 15, SRCCOPY);
		x0 -= 16;
		i++;
	}
	for (; i < 5; i++) {
		if (tbtn[i].DlgMsg && tbtn[i].hBmp) {
			SelectObject (hDCsrc, tbtn[i].hBmp);
			BitBlt (hDC, x0, y0, 15, 15, hDCsrc, 0, tbtn[i].flag & 0x80000000 ? 15:0, SRCCOPY);
			x0 -= 16;
		}
	}
	DeleteDC (hDCsrc);
	DeleteObject (hBmp);
	ReleaseDC (hWnd, hDC);
}

// ======================================================================

bool DialogWin::CheckTitleButtons (const POINTS &pt)
{
	if (!(flag & DLG_CAPTIONBUTTON)) return false;

	RECT r;
	GetWindowRect (hWnd, &r);
	int xm = pt.x-r.left;
	int ym = pt.y-r.top;
	int x0, y0;
	if (GetWindowLongPtr (hWnd, GWL_STYLE) & WS_THICKFRAME) {
		x0 = y_sizeframe,  y0 = x_sizeframe;
	} else {
		x0 = y_fixedframe, y0 = x_fixedframe;
	}
	if (ym < y0 || ym >= y0+15) return false;
	int nbt = (r.right-pt.x-x0)/16;
	if (nbt >= 0 && nbt < 5 && tbtn[nbt].DlgMsg) {
		WORD state = 0;
		if (tbtn[nbt].flag & DLG_CB_TWOSTATE) {
			tbtn[nbt].flag ^= 0x80000000;
			state = (tbtn[nbt].flag & 0x80000000 ? 1:0);
			PaintTitleButtons ();
		}
		PostMessage (hWnd, WM_COMMAND, MAKELONG (tbtn[nbt].DlgMsg, state), 0);
		return true;
	} else return false;
}

// ======================================================================

bool DialogWin::Create_AddTitleButton (DWORD msg, HBITMAP hBmp, DWORD flag)
{
	if (dlg_create) return dlg_create->AddTitleButton (msg, hBmp, flag);
	else            return false;
}

// ======================================================================

bool DialogWin::Create_SetTitleButtonState (DWORD msg, DWORD state)
{
	if (dlg_create) return dlg_create->SetTitleButtonState (msg, state);
	else            return false;
}

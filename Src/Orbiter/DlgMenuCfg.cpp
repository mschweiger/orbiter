// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
// Menu bar configuration dialog
// ======================================================================

#define STRICT 1

#include "Orbiter.h"
#include "Pane.h"
#include "MenuInfoBar.h"
#include "DlgMenuCfg.h"
#include "resource.h"
#include "resource2.h"
#include "DlgCtrl.h"

extern Orbiter *g_pOrbiter;
extern Pane *g_pane;
extern HELPCONTEXT DefHelpContext;

// ======================================================================

DlgMenuCfg::DlgMenuCfg (HINSTANCE hInstance, HWND hParent, void *context)
: DialogWin (hInstance, hParent, IDD_MENU_CONFIG, 0, 0, context)
{
}

// ======================================================================

DlgMenuCfg::~DlgMenuCfg ()
{
}

// ======================================================================

BOOL DlgMenuCfg::OnInitDialog (HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	SendDlgItemMessage (hDlg, IDC_MNUCFG_SHOWMENU + g_pOrbiter->Cfg()->CfgUIPrm.MenuMode,
		BM_SETCHECK, BST_CHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_MNUCFG_SHOWINFO + g_pOrbiter->Cfg()->CfgUIPrm.InfoMode,
		BM_SETCHECK, BST_CHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_MNUCFG_LABELONLY, BM_SETCHECK,
		g_pOrbiter->Cfg()->CfgUIPrm.bMenuLabelOnly ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_MNUCFG_SHOWWARP, BM_SETCHECK,
		g_pOrbiter->Cfg()->CfgUIPrm.bWarpAlways ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_MNUCFG_SCIWARP, BM_SETCHECK,
		g_pOrbiter->Cfg()->CfgUIPrm.bWarpScientific ? BST_CHECKED : BST_UNCHECKED, 0);
	for (int i = 0; i < 2; i++) {
		int id = IDC_MNUCFG_LINFO+i;
		SendDlgItemMessage (hDlg, id, CB_RESETCONTENT, 0, 0);
		SendDlgItemMessage (hDlg, id, CB_ADDSTRING, 0, (LPARAM)"None");
		SendDlgItemMessage (hDlg, id, CB_ADDSTRING, 0, (LPARAM)"Frame rate");
		SendDlgItemMessage (hDlg, id, CB_ADDSTRING, 0, (LPARAM)"Render statistics");
		SendDlgItemMessage (hDlg, id, CB_ADDSTRING, 0, (LPARAM)"Viewport info");
		SendDlgItemMessage (hDlg, id, CB_SETCURSEL, g_pOrbiter->Cfg()->CfgUIPrm.InfoAuxIdx[i], 0);
	}
	SendDlgItemMessage (hDlg, IDC_PAUSEINDICATOR, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage (hDlg, IDC_PAUSEINDICATOR, CB_ADDSTRING, 0, (LPARAM)"Flash on action");
	SendDlgItemMessage (hDlg, IDC_PAUSEINDICATOR, CB_ADDSTRING, 0, (LPARAM)"Show on pause/record/playback");
	SendDlgItemMessage (hDlg, IDC_PAUSEINDICATOR, CB_ADDSTRING, 0, (LPARAM)"Don't show");
	SendDlgItemMessage (hDlg, IDC_PAUSEINDICATOR, CB_SETCURSEL, g_pOrbiter->Cfg()->CfgUIPrm.PauseIndMode, 0);

	GAUGEPARAM gp1 = { 0, 10, GAUGEPARAM::LEFT, GAUGEPARAM::BLACK };
	oapiSetGaugeParams (GetDlgItem (hDlg, IDC_MNUCFG_MENUOPACITY), &gp1);
	int scl = g_pOrbiter->Cfg()->CfgUIPrm.MenuOpacity;
	oapiSetGaugePos (GetDlgItem (hDlg, IDC_MNUCFG_MENUOPACITY), scl);

	GAUGEPARAM gp2 = { 0, 10, GAUGEPARAM::LEFT, GAUGEPARAM::BLACK };
	oapiSetGaugeParams (GetDlgItem (hDlg, IDC_MNUCFG_INFOOPACITY), &gp2);
	scl = g_pOrbiter->Cfg()->CfgUIPrm.InfoOpacity;
	oapiSetGaugePos (GetDlgItem (hDlg, IDC_MNUCFG_INFOOPACITY), scl);

	GAUGEPARAM gp3 = { 1, 20, GAUGEPARAM::LEFT, GAUGEPARAM::BLACK };
	oapiSetGaugeParams (GetDlgItem (hDlg, IDC_MNUCFG_SCROLLSPEED), &gp3);
	scl = g_pOrbiter->Cfg()->CfgUIPrm.MenuScrollspeed;
	oapiSetGaugePos (GetDlgItem (hDlg, IDC_MNUCFG_SCROLLSPEED), scl);

	return TRUE;
}

// ======================================================================

BOOL DlgMenuCfg::OnCommand (HWND hDlg, WORD id, WORD code, HWND hControl)
{
	switch (id) {
	case IDHELP:
		DefHelpContext.topic = (char*)"/menucfg.htm";
		g_pOrbiter->OpenHelp (&DefHelpContext);
		return TRUE;
	case IDC_MNUCFG_SHOWMENU:
	case IDC_MNUCFG_SHOWMENU+1:
	case IDC_MNUCFG_SHOWMENU+2:
		if (code == BN_CLICKED) {
			int mode = id-IDC_MNUCFG_SHOWMENU;
			g_pOrbiter->Cfg()->CfgUIPrm.MenuMode = mode;
			g_pane->MIBar()->SetMenuMode (mode);
		}
		break;
	case IDC_MNUCFG_SHOWINFO:
	case IDC_MNUCFG_SHOWINFO+1:
	case IDC_MNUCFG_SHOWINFO+2:
		if (code == BN_CLICKED) {
			int mode = id-IDC_MNUCFG_SHOWINFO;
			g_pOrbiter->Cfg()->CfgUIPrm.InfoMode = mode;
			g_pane->MIBar()->SetInfoMode (mode);
			return TRUE;
		}
		break;
	case IDC_MNUCFG_LABELONLY:
		if (code == BN_CLICKED) {
			bool check = (SendDlgItemMessage (hDlg, id, BM_GETCHECK, 0, 0) == TRUE);
			g_pOrbiter->Cfg()->CfgUIPrm.bMenuLabelOnly = check;
			g_pane->MIBar()->SetLabelOnly (check);
			return TRUE;
		}
		break;
	case IDC_MNUCFG_SHOWWARP:
		if (code == BN_CLICKED) {
			bool check = (SendDlgItemMessage (hDlg, id, BM_GETCHECK, 0, 0) == TRUE);
			g_pOrbiter->Cfg()->CfgUIPrm.bWarpAlways = check;
			g_pane->MIBar()->SetWarpAlways (check);
			return TRUE;
		}
		break;
	case IDC_MNUCFG_SCIWARP:
		if (code == BN_CLICKED) {
			bool check = (SendDlgItemMessage (hDlg, id, BM_GETCHECK, 0, 0) == TRUE);
			g_pOrbiter->Cfg()->CfgUIPrm.bWarpScientific = check;
			g_pane->MIBar()->SetWarpScientific (check);
			return TRUE;
		}
		break;
	case IDC_MNUCFG_LINFO:
	case IDC_MNUCFG_RINFO:
		if (code == CBN_SELCHANGE) {
			int side = id-IDC_MNUCFG_LINFO;
			int idx = SendDlgItemMessage (hDlg, IDC_MNUCFG_LINFO+side, CB_GETCURSEL, 0, 0);
			if (idx != CB_ERR) g_pOrbiter->Cfg()->CfgUIPrm.InfoAuxIdx[side] = idx;
			if (idx && g_pOrbiter->Cfg()->CfgUIPrm.InfoAuxIdx[1-side] == idx) {
				g_pOrbiter->Cfg()->CfgUIPrm.InfoAuxIdx[1-side] = 0;
				g_pane->MIBar()->SetAuxInfobar (1-side,0);
				SendDlgItemMessage (hDlg, IDC_MNUCFG_LINFO+1-side, CB_SETCURSEL, 0, 0);
			}
			g_pane->MIBar()->SetAuxInfobar (side,idx);
			return TRUE;
		}
		break;
	case IDC_PAUSEINDICATOR:
		if (code == CBN_SELCHANGE) {
			int idx = SendDlgItemMessage (hDlg, IDC_PAUSEINDICATOR, CB_GETCURSEL, 0, 0);
			if (idx != CB_ERR && idx != g_pOrbiter->Cfg()->CfgUIPrm.PauseIndMode) {
				g_pOrbiter->Cfg()->CfgUIPrm.PauseIndMode = idx;
				g_pane->MIBar()->SetPauseIndicatorMode (idx);
			}
		}
	}
	return DialogWin::OnCommand (hDlg, id, code, hControl);
}

// ======================================================================

BOOL DlgMenuCfg::OnHScroll (HWND hDlg, WORD request, WORD curpos, HWND hControl)
{
	switch (GetDlgCtrlID (hControl)) {
	case IDC_MNUCFG_MENUOPACITY:
		switch (request) {
		case SB_THUMBTRACK:
		case SB_LINELEFT:
		case SB_LINERIGHT:
			g_pOrbiter->Cfg()->CfgUIPrm.MenuOpacity = curpos;
			g_pane->MIBar()->SetOpacity (curpos);
			return 0;
		}
		break;
	case IDC_MNUCFG_INFOOPACITY:
		switch (request) {
		case SB_THUMBTRACK:
		case SB_LINELEFT:
		case SB_LINERIGHT:
			g_pOrbiter->Cfg()->CfgUIPrm.InfoOpacity = curpos;
			g_pane->MIBar()->SetOpacityInfo (curpos);
			return 0;
		}
		break;
	case IDC_MNUCFG_SCROLLSPEED:
		switch (request) {
		case SB_THUMBTRACK:
		case SB_LINELEFT:
		case SB_LINERIGHT:
			g_pOrbiter->Cfg()->CfgUIPrm.MenuScrollspeed = curpos;
			g_pane->MIBar()->SetScrollspeed (curpos);
			return 0;
		}
		break;
	}
	return DialogWin::OnHScroll (hDlg, request, curpos, hControl);
}

// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
// In-session options dialog
// ======================================================================

#define STRICT 1

#include <io.h>
#include <array>
#include "DlgOptions.h"
#include "Orbiter.h"
#include "OrbiterAPI.h"
#include "Psys.h"
#include "Camera.h"
#include "DlgCtrl.h"
#include "resource.h"
#include <uxtheme.h>
#include <commctrl.h>

extern Orbiter* g_pOrbiter;
extern PlanetarySystem* g_psys;
extern Camera* g_camera;
extern HELPCONTEXT DefHelpContext;

// ======================================================================

DlgOptions::DlgOptions(HINSTANCE hInstance, HWND hParent, void* context)
	: DialogWin(hInstance, hParent, IDD_OPTIONS, 0, 0, context)
	, OptionsPageContainer(OptionsPageContainer::INLINE, g_pOrbiter->Cfg())
{
	pos = &g_pOrbiter->Cfg()->CfgWindowPos.DlgOptions;
}

// ----------------------------------------------------------------------

DlgOptions::~DlgOptions()
{
}

// ----------------------------------------------------------------------

void DlgOptions::Update()
{
	UpdatePages(false);
}

// ----------------------------------------------------------------------

BOOL DlgOptions::OnInitDialog(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	SetWindowHandles(hDlg, GetDlgItem(hDlg, IDC_OPT_SPLIT), GetDlgItem(hDlg, IDC_OPT_PAGELIST), GetDlgItem(hDlg, IDC_OPT_PAGECONTAINER));
	SetSize(hDlg);
	CreatePages();
	ExpandAll();

	return TRUE;
}

// ----------------------------------------------------------------------

BOOL DlgOptions::OnCommand(HWND hDlg, WORD ctrlId, WORD notification, HWND hCtrl)
{
	switch (ctrlId) {
	case IDHELP:
		if (notification == BN_CLICKED) {
			if (HelpContext())
			g_pOrbiter->OpenHelp(HelpContext());
			return TRUE;
		}
		break;
	}
	return DialogWin::OnCommand(hDlg, ctrlId, notification, hCtrl);
}

// ----------------------------------------------------------------------

BOOL DlgOptions::OnSize(HWND hDlg, WPARAM wParam, int w, int h)
{
	SetSize(hDlg);
	return 0;
}

// ----------------------------------------------------------------------

BOOL DlgOptions::OnVScroll(HWND hDlg, WORD request, WORD curpos, HWND hControl)
{
	return VScroll(hDlg, request, curpos, hControl);
}

// ----------------------------------------------------------------------

BOOL DlgOptions::OnNotify(HWND hDlg, int idCtrl, LPNMHDR pnmh)
{
	if (idCtrl == IDC_OPT_PAGELIST) {
		OnNotifyPagelist(pnmh);
		return TRUE;
	}
	return FALSE;
}

// ----------------------------------------------------------------------

void DlgOptions::SetSize(HWND hDlg)
{
	RECT r0;

	//GetClientRect(GetDlgItem(hDlg, IDCANCEL), &r0);
	GetClientRect(hDlg, &r0);
	SetWindowPos(GetDlgItem(hDlg, IDC_OPT_SPLIT), HWND_BOTTOM, 9, 10, r0.right - 25, r0.bottom - 52, SWP_NOACTIVATE | SWP_NOOWNERZORDER);
	SetWindowPos(GetDlgItem(hDlg, IDC_SCROLLBAR1), NULL, r0.right - 16, 10, 14, r0.bottom - 52, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
	SetWindowPos(GetDlgItem(hDlg, IDCANCEL), NULL, r0.right - 84, r0.bottom - 33, 0, 0, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOSIZE);
	SetWindowPos(GetDlgItem(hDlg, IDHELP), NULL, r0.right - 165, r0.bottom - 33, 0, 0, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOSIZE);

	InvalidateRect(hDlg, NULL, TRUE);
	UpdateWindow(hDlg);

	SetPageSize(hDlg);
}

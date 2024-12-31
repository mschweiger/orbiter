// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
// Dialogs.cpp
// Orbiter in-game dialog functions
// ======================================================================

#define STRICT 1
#define OEMRESOURCE
#include <io.h>
#include "Orbiter.h"
#include "Vessel.h"
#include "Psys.h"
#include "Camera.h"
#include "Pane.h"
#include "Nav.h"
#include "resource.h"
#include "Dialogs.h"
#include "DlgMgr.h" // to be removed

extern Orbiter *g_pOrbiter;
extern PlanetarySystem *g_psys;
extern Camera *g_camera;
extern Vessel *g_focusobj;

#ifdef UNDEF
// ======================================================================
// "Navaid" dialog
// ======================================================================

void Navaid_BuildNavList (HWND hDlg)
{
	char cbuf[256];
	double lng, lat;

	SetWindowText (GetDlgItem (hDlg, IDC_INFO_TEXT), "");
	GetWindowText (GetDlgItem (hDlg, IDC_INFO_NAME), cbuf, 256);
	Planet *p = g_psys->GetPlanet (cbuf[0] == ' ' ? cbuf+5:cbuf);
	if (p && p->nNav()) {
		for (DWORD n = 0; n < p->nNav(); n++) {
			cbuf[0] = '\0';
			const Nav *nav = p->NavMgr().GetNav(n);
			switch (nav->Type()) {
			case TRANSMITTER_VOR: {
				const Nav_VOR *vor = (const Nav_VOR*)nav;
				vor->GetEquPos (lng, lat);
				sprintf (cbuf, "%s\t%06.2f\t%06.3f°%c\t%07.3f°%c\t%0.0fkm\r\n",
					nav->GetId(), nav->GetFreq(),
					fabs(lat)*DEG, lat >= 0.0 ? 'N':'S',
					fabs(lng)*DEG, lng >= 0.0 ? 'E':'W',
					nav->GetRange()*0.001
				);
				} break;
			}
			if (cbuf[0])
				SendDlgItemMessage (hDlg, IDC_INFO_TEXT, EM_REPLACESEL, FALSE, (LPARAM)cbuf);
		}
	}
}

void Navaid_BuildPlanetList (HWND hDlg)
{
	char cbuf[256];
	const Planet *p;

	SendDlgItemMessage (hDlg, IDC_INFO_NAME, CB_RESETCONTENT, 0, 0);
	for (int i = 0; i < g_psys->nPlanet(); i++) {
		p = g_psys->GetPlanet(i);
		if (p->isMoon()) strcpy (cbuf, "     ");
		strcpy (cbuf + (p->isMoon()?5:0), p->Name());
		SendDlgItemMessage (hDlg, IDC_INFO_NAME, CB_ADDSTRING, 0, (LPARAM)cbuf);
	}

	// select a default planet from the list
	int idx = 0;
	if (p = g_focusobj->ProxyPlanet()) {
		if (p->isMoon()) strcpy (cbuf, "     ");
		strcpy (cbuf + (p->isMoon()?5:0), p->Name());
		idx = SendDlgItemMessage (hDlg, IDC_INFO_NAME, CB_FINDSTRINGEXACT, 0, (LPARAM)cbuf);
		if (idx == CB_ERR) idx = 0;
	}
	if (SendDlgItemMessage (hDlg, IDC_INFO_NAME, CB_SETCURSEL, idx, 0) != CB_ERR)
		Navaid_BuildNavList (hDlg);
}

INT_PTR CALLBACK Navaid_DlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static DWORD ts[4] = {32, 68, 110, 143};

	switch (uMsg) {
	case WM_INITDIALOG:
		Navaid_BuildPlanetList (hDlg);
		SendDlgItemMessage (hDlg, IDC_INFO_TEXT, EM_SETTABSTOPS, 4, (LPARAM)ts);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCANCEL:
			g_pOrbiter->CloseDialog (hDlg);
			return TRUE;
		case IDC_INFO_NAME:
			if (HIWORD(wParam) == CBN_SELCHANGE)
				Navaid_BuildNavList (hDlg);
			break;
		}
		break;
	}
	return OrbiterDefDialogProc (hDlg, uMsg, wParam, lParam);
}
#endif

// ======================================================================
// "Recorder/player" dialog
// ======================================================================

INT_PTR CALLBACK FRecorderMsg_DlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			g_pOrbiter->ToggleRecorder (true);
			// fall through
		case IDCANCEL:
			g_pOrbiter->CloseDialog (hDlg);
			break;
		}
		break;
	}
	return OrbiterDefDialogProc (hDlg, uMsg, wParam, lParam);
}

// =========================================================================
// Set up a combo box dialog control for selecting a celestial body

static const char *pathsep = " > ";

void CBodySelectComboBox::Init (HWND hDlg, int resid)
{
	BuildListFromNode (hDlg, resid);
}

void CBodySelectComboBox::BuildListFromNode (HWND hDlg, int resid, const CelestialBody *node)
{
	if (!node) node = g_camera->ProxyPlanet();

	SendDlgItemMessage (hDlg, resid, CB_RESETCONTENT, 0, 0);

	char path[256] = "", cbuf[256];
	int i, len;

	// add parent node, if exists
	const CelestialBody *parent = node->ElRef();
	if (parent) {
		strcpy (path, parent->Name());
		while (parent->ElRef()) {
			parent = parent->ElRef();
			strcpy (cbuf, path);
			strcpy (path, parent->Name());
			strcat (path, pathsep);
			strcat (path, cbuf);
		}
		SendDlgItemMessage (hDlg, resid, CB_ADDSTRING, 0, (LPARAM)path);
		strcat (path, pathsep);
	}

	// add node itself
	strcat (path, node->Name());
	SendDlgItemMessage (hDlg, resid, CB_ADDSTRING, 0, (LPARAM)path);
	strcat (path, pathsep);
	len = strlen (path);

	// add child nodes, if exist
	for (i = 0; i < node->nSecondary(); i++) {
		strcpy (path+len, node->Secondary(i)->Name());
		SendDlgItemMessage (hDlg, resid, CB_ADDSTRING, 0, (LPARAM)path);
	}

	// select node
	SendDlgItemMessage (hDlg, resid, CB_SETCURSEL, parent ? 1:0, 0);
}

CelestialBody *CBodySelectComboBox::OnSelectionChanged (HWND hDlg, int resid)
{
	char cbuf[256], *c;
	int i;

	GetWindowText (GetDlgItem (hDlg, resid), cbuf, 256);
	c = cbuf;
	for (i = strlen (cbuf)-1; i; i--)
		if (cbuf[i] == pathsep[1]) { c = cbuf+i+2; break; }
	CelestialBody *cbody = g_psys->GetGravObj(c);
	if (cbody)
		CBodySelectComboBox::BuildListFromNode (hDlg, resid, cbody);
	return cbody;
}

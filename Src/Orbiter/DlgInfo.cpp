// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
// Object info window
// ======================================================================

#define STRICT 1
#define _WIN32_WINNT 0x0501
#define OAPI_IMPLEMENTATION

#include <windows.h>
#include "DlgInfo.h"
#include "Dialogs.h"
#include "resource.h"
#include "DlgMgr.h" // to be removed
#include "Orbiter.h"
#include "Vessel.h"
#include "Camera.h"
#include "Psys.h"
#include "Celbody.h"
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include "Element.h"

extern Orbiter *g_pOrbiter;
extern Vessel *g_focusobj;
extern Camera *g_camera;
extern PlanetarySystem *g_psys;
extern TimeData td;
extern HELPCONTEXT DefHelpContext;
extern char DBG_MSG[256];

const char *na = "N/A";

// ======================================================================

DlgInfo::DlgInfo (HINSTANCE hInstance, HWND hParent, void *context)
: DialogWin (hInstance, hParent, IDD_OBJINFO, 0, 0, context)
{
	upd_t = 0.0;
	upd_dt = 1.0;
	body = NULL;
	listmode = LIST_NONE;
	pos = &g_pOrbiter->Cfg()->CfgWindowPos.DlgInfo;

	// note: shared icon resources should probably be loaded globally by orbiter
	hIcon_dd = LoadIcon (hInstance, MAKEINTRESOURCE(IDI_DDOWN));
	hIcon_du = LoadIcon (hInstance, MAKEINTRESOURCE(IDI_DUP));
}

// ======================================================================

void DlgInfo::Init (HWND hDlg)
{
	RECT r;
	POINT pt;
	PARAFORMAT2 pfmt;
	pfmt.cbSize = sizeof(PARAFORMAT2);
	pfmt.dwMask = PFM_TABSTOPS;
	pfmt.cTabCount = 1;
	pfmt.rgxTabs[0] = 200;
	int objtp = 0;

	SendDlgItemMessage (hDlg, IDC_INFO_TYPE, CB_ADDSTRING, 0, (LPARAM)"Focus vessel");
	SendDlgItemMessage (hDlg, IDC_INFO_TYPE, CB_ADDSTRING, 0, (LPARAM)"Camera target");
	SendDlgItemMessage (hDlg, IDC_INFO_TYPE, CB_ADDSTRING, 0, (LPARAM)"Vessel");
	SendDlgItemMessage (hDlg, IDC_INFO_TYPE, CB_ADDSTRING, 0, (LPARAM)"Base");
	SendDlgItemMessage (hDlg, IDC_INFO_TYPE, CB_ADDSTRING, 0, (LPARAM)"Celestial body");
	if (body) {
		switch (body->Type()) {
		case OBJTP_VESSEL: objtp = 2; break;
		case OBJTP_SURFBASE: objtp = 3; break;
		case OBJTP_PLANET:
		case OBJTP_STAR: objtp = 4; break;
		}
	}
	SendDlgItemMessage (hDlg, IDC_INFO_TYPE, CB_SETCURSEL, objtp, 0);

	SendDlgItemMessage (hDlg, IDC_INFO_DDN, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon_dd);
	SendDlgItemMessage (hDlg, IDC_INFO_DUP, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon_du);

	int res = SendDlgItemMessage (hDlg, IDC_INFOBOX, EM_SETPARAFORMAT, 0, (LPARAM)&pfmt);
	GetClientRect (hDlg, &r);
	client_w = r.right;
	client_h = r.bottom;
	GetWindowRect (GetDlgItem (hDlg, IDC_INFOLIST), &r);
	list_w = r.right-r.left;
	list_h = r.bottom-r.top;
	pt.x = r.left;
	pt.y = r.top;
	ScreenToClient (hDlg, &pt);
	list_top = pt.y;
	pl.OnInitDialog (hDlg, IDC_INFOLIST);
	pl.SetColWidth (0, 180);

	Body *b = body;
	if (!b) b = g_focusobj;
	body = NULL;

	SetBody (hDlg, b);
	BuildObjectList (hDlg, b);
	if (objtp) {
		int idx = SendDlgItemMessage (hDlg, IDC_INFO_NAME, CB_FINDSTRINGEXACT, 0, (LPARAM)body->Name());
		if (idx != CB_ERR)
			SendDlgItemMessage (hDlg, IDC_INFO_NAME, CB_SETCURSEL, idx, 0);
	}
}

// ======================================================================

void DlgInfo::SetBody (Body *bd)
{
	SetBody (hWnd, bd);
	Init (hWnd);
}

// ======================================================================

void DlgInfo::BuildObjectList (HWND hDlg, Body *b)
{
	Body *obj;
	char cbuf[256];
	int j;
	DWORD k;
	int idx = SendDlgItemMessage (hDlg, IDC_INFO_TYPE, CB_GETCURSEL, 0, 0);
	SendDlgItemMessage (hDlg, IDC_INFO_NAME, CB_RESETCONTENT, 0, 0);

	switch (idx) {
	case 0: // focus vessel
		strcpy (cbuf, g_focusobj->Name());
		SendDlgItemMessage (hDlg, IDC_INFO_NAME, CB_ADDSTRING, 0, (LPARAM)cbuf);
		break;
	case 1: // camera target
		strcpy (cbuf, g_camera->Target()->Name());
		SendDlgItemMessage (hDlg, IDC_INFO_NAME, CB_ADDSTRING, 0, (LPARAM)cbuf);
		break;
	case 2: // vessels
		for (k = 0; k < g_psys->nVessel(); k++) {
			strcpy (cbuf, g_psys->GetVessel(k)->Name());
			SendDlgItemMessage (hDlg, IDC_INFO_NAME, CB_ADDSTRING, 0, (LPARAM)cbuf);
		}
		break;
	case 3: // spaceports
		for (k = 0; k < g_psys->nGrav(); k++) {
			obj = g_psys->GetGravObj (k);
			if (obj->Type() != OBJTP_PLANET) continue;
			Planet *planet = (Planet*)obj;
			for (j = 0; j < g_psys->nBase(planet); j++) {
				strcpy (cbuf, g_psys->GetBase (planet,j)->Name());
				SendDlgItemMessage (hDlg, IDC_INFO_NAME, CB_ADDSTRING, 0, (LPARAM)cbuf);
			}
		}
		break;
	case 4: // celestial bodies
		CBodySelectComboBox::BuildListFromNode (hDlg, IDC_INFO_NAME, (CelestialBody*)b);
		return;
	}
	SendDlgItemMessage (hDlg, IDC_INFO_NAME, CB_SETCURSEL, 0, 0);
	//Info_ShowData (hDlg);
}

// ======================================================================

int DlgInfo::Size (DWORD width, DWORD height)
{
	int i, id;
	RECT r;
	POINT pt;
	GetClientRect (hWnd, &r);
	int dw = r.right-client_w;
	int dh = r.bottom-client_h;
	list_w += dw;
	list_h += dh;
	client_w = r.right;
	client_h = r.bottom;
	pl.Move (0, list_top, list_w, list_h);

	const int nbtl = 2;
	const int btlid[nbtl] = {IDC_INFO_DDN, IDC_INFO_DUP};
	for (i = 0; i < nbtl; i++) {
		id = btlid[i];
		GetWindowRect (GetDlgItem (hWnd, id), &r);
		pt.x = r.left;
		pt.y = r.top;
		ScreenToClient (hWnd, &pt);
		MoveWindow (GetDlgItem (hWnd, id), pt.x, pt.y+dh, r.right-r.left, r.bottom-r.top, TRUE);
	}
	const int nbtr = 3;
	const int btrid[nbtr] = {IDC_INFO_MAP, IDCANCEL, IDHELP};
	for (i = 0; i < nbtr; i++) ShowWindow (GetDlgItem (hWnd, btrid[i]), SW_HIDE);
	for (i = 0; i < nbtr; i++) {
		id = btrid[i];
		GetWindowRect (GetDlgItem (hWnd, id), &r);
		pt.x = r.left;
		pt.y = r.top;
		ScreenToClient (hWnd, &pt);
		MoveWindow (GetDlgItem (hWnd, id), pt.x+dw, pt.y+dh, r.right-r.left, r.bottom-r.top, TRUE);
	}
	for (i = 0; i < nbtr; i++) ShowWindow (GetDlgItem (hWnd, btrid[i]), SW_SHOW);
	return 0;
}

// ======================================================================

void DlgInfo::OpenMap ()
{
	if (body) {
		DlgMap *pMap = g_pOrbiter->DlgMgr()->EnsureEntry<DlgMap> ();
		pMap->SetSelection (body);
	}
}

// ======================================================================

void DlgInfo::ExpandAll (HWND hDlg)
{
	pl.ExpandAll (true);
}

// ======================================================================

void DlgInfo::CollapseAll (HWND hDlg)
{
	pl.ExpandAll (false);
}

// ======================================================================

void DlgInfo::Update ()
{
	if (td.SysT0 > upd_t + upd_dt || td.SysT0 < upd_t) {
		switch (listmode) {
		case LIST_VESSEL:
			UpdateItems_vessel ();
			break;
		case LIST_CBODY:
			UpdateItems_celbody ();
			break;
		case LIST_BASE:
			UpdateItems_base ();
			break;
		}
		pl.Update ();
		upd_t = td.SysT0;
	}
}

// ======================================================================

void DlgInfo::SelectionChanged (HWND hDlg)
{
	char cbuf[256];
	int i, j;

	int tp = SendDlgItemMessage (hDlg, IDC_INFO_TYPE, CB_GETCURSEL, 0, 0);
	GetWindowText (GetDlgItem (hDlg, IDC_INFO_NAME), cbuf, 256);

	if (tp == 1) { // camera target
		Body *b = g_psys->GetObj (cbuf);
		if (!b) b = g_psys->GetBase (cbuf);
		if (!b) return;
		switch (b->Type()) {
		case OBJTP_VESSEL:
			tp = 2;
			break;
		case OBJTP_SURFBASE:
			tp = 3;
			break;
		case OBJTP_STAR:
		case OBJTP_PLANET:
			tp = 4;
			break;
		}
	}
	switch (tp) {
	case 0: // focus vessel
		SetBody (hDlg, g_focusobj);
		break;
	case 2: // vessel
		SetBody (hDlg, g_psys->GetVessel (cbuf));
		break;
	case 3: // surface base
		for (i = 0; i < g_psys->nGrav(); i++) {
			Body *obj;
			obj = g_psys->GetGravObj (i);
			if (obj->Type() != OBJTP_PLANET) continue;
			Planet *planet = (Planet*)obj;
			for (j = 0; j < g_psys->nBase(planet); j++) {
				if (!_stricmp (g_psys->GetBase (planet,j)->Name(), cbuf)) {
					SetBody (hDlg, g_psys->GetBase (planet,j));
					break;
				}
			}
		}
		break;
	case 4: { // celestial bodies
		CelestialBody *cbody = CBodySelectComboBox::OnSelectionChanged (hDlg, IDC_INFO_NAME);
		if (cbody) SetBody (hDlg, cbody);
		} break;
	}
}

// ======================================================================

void DlgInfo::SetBody (HWND hDlg, Body *bd)
{
	if (bd == body) return; // nothing to do
	body = bd;
	switch (bd->Type()) {
	case OBJTP_VESSEL:
		listmode = LIST_VESSEL;
		break;
	case OBJTP_STAR:
	case OBJTP_PLANET:
		listmode = LIST_CBODY;
		break;
	case OBJTP_SURFBASE:
		listmode = LIST_BASE;
		break;
	}
	switch (listmode) {
	case LIST_VESSEL:
		InitItems_vessel (hDlg, (Vessel*)body);
		UpdateItems_vessel ();
		break;
	case LIST_CBODY:
		InitItems_celbody (hDlg, (CelestialBody*)body);
		UpdateItems_celbody ();
		break;
	case LIST_BASE:
		InitItems_base (hDlg, (Base*)body);
		UpdateItems_base ();
		break;
	}
	pl.Redraw();
}

// ======================================================================

void DlgInfo::InitItems_vessel (HWND hDlg, const Vessel *vessel)
{
	const int nlabel_des = 3;
	const char *label_des[nlabel_des] = {
		"Name",
		"Class",
		"Transponder frequency"
	};
	const int nlabel_prm = 5;
	const char *label_prm[nlabel_prm] = {
		"Total mass",
		"Dry mass",
		"Propellant mass",
		"Mean radius",
		"P. moments of inertia"
	};
	const int nlabel_thr = 3;
	const char *label_thr[nlabel_thr] = {
		"Main",
		"Retro",
		"Hover"
	};
	const int nlabel_els = 7;
	const char *label_els[nlabel_els] = {
		"Reference",
		"Semi-major axis (a)",
		"Eccentricity (e)",
		"Inclination (i)",
		"Longitude of asc. node",
		"Longitude of periapsis",
		"Mean longitude"
	};
	const int nlabel_srf = 8;
	const char *label_srf[nlabel_srf] = {
		"Reference",
		"Position",
		"Altitude",
		"Ground speed",
		"Vertical speed",
		"Heading",
		"Pitch",
		"Bank"
	};
	const int nlabel_atm = 3;
	const char *label_atm[nlabel_atm] = {
		"Temperature",
		"Density",
		"Pressure"
	};
	const int nlabel_aer = 8;
	const char *label_aer[nlabel_aer] = {
		"Dynamic pressure",
		"True airspeed",
		"Mach number",
		"Lift",
		"Drag",
		"Weight",
		"Lift/drag ratio",
		"Angle of attack"
	};
	const int nlabel_prp = 4;
	const char *label_prp[nlabel_prp] = {
		"Update mode",
		"State propagator",
		"Subsamples",
		"Gravity sources"
	};

	int i;
	PropertyItem *item;
	pl.ClearGroups();

	vlist.des = pl.AppendGroup();
	vlist.des->SetTitle ("Designation");
	for (i = 0; i < nlabel_des; i++) {
		item = pl.AppendItem (vlist.des);
		item->SetLabel (label_des[i]);
	}

	vlist.prm = pl.AppendGroup();
	vlist.prm->SetTitle ("Physical parameters");
	for (i = 0; i < nlabel_prm; i++) {
		item = pl.AppendItem (vlist.prm);
		item->SetLabel (label_prm[i]);
	}

	const THGROUP_TYPE thgrp[3] = {THGROUP_MAIN, THGROUP_RETRO, THGROUP_HOVER};
	bool showthgrp = false;
	for (i = 0; i < 3; i++) {
		showth[i] = (vessel->GetThrusterGroupMaxth (thgrp[i]) > 0.0);
		if (showth[i]) showthgrp = true;
	}
	if (showthgrp) {
		vlist.thr = pl.AppendGroup();
		vlist.thr->SetTitle ("Thruster group ratings (vacuum)");
		for (i = 0; i < nlabel_thr; i++) {
			if (showth[i]) {
				item = pl.AppendItem (vlist.thr);
				item->SetLabel (label_thr[i]);
			}
		}
	} else vlist.thr = NULL;

	vlist.els = pl.AppendGroup ();
	vlist.els->SetTitle ("Osculating elements (ecliptic frame)");
	for (i = 0; i < nlabel_els; i++) {
		item = pl.AppendItem (vlist.els);
		item->SetLabel (label_els[i]);
	}

	vlist.srf = pl.AppendGroup();
	vlist.srf->SetTitle ("Surface-relative parameters");
	for (i = 0; i < nlabel_srf; i++) {
		item = pl.AppendItem (vlist.srf);
		item->SetLabel (label_srf[i]);
	}

	vlist.atm = pl.AppendGroup();
	vlist.atm->SetTitle ("Atmospheric parameters");
	for (i = 0; i < nlabel_atm; i++) {
		item = pl.AppendItem (vlist.atm);
		item->SetLabel (label_atm[i]);
	}

	vlist.aer = pl.AppendGroup();
	vlist.aer->SetTitle ("Aerodynamic parameters");
	for (i = 0; i < nlabel_aer; i++) {
		item = pl.AppendItem (vlist.aer);
		item->SetLabel (label_aer[i]);
	}

	if (vessel->nDock()) {
		vlist.dck = pl.AppendGroup();
		vlist.dck->SetTitle ("Docking ports");
		char cbuf[64];
		for (i = 0; i < vessel->nDock(); i++) {
			item = pl.AppendItem (vlist.dck);
			sprintf (cbuf, "Port %d", i+1);
			item->SetLabel (cbuf);
		}
	} else vlist.dck = NULL;

	vlist.prp = pl.AppendGroup();
	vlist.prp->SetTitle ("State propagation");
	for (i = 0; i < nlabel_prp; i++) {
		item = pl.AppendItem (vlist.prp);
		item->SetLabel (label_prp[i]);
	}
}

// ======================================================================

void DlgInfo::UpdateItems_vessel ()
{
	const THGROUP_TYPE thgrp[3] = {THGROUP_MAIN, THGROUP_RETRO, THGROUP_HOVER};
	int i, j;
	float f;
	char cbuf[256];

	Vessel *vessel = (Vessel*)body;
	const Body *ref = vessel->ElRef();

	vlist.des->GetItem (0)->SetValue (vessel->Name());
	vlist.des->GetItem (1)->SetValue (vessel->ClassName());
	if (vessel->GetXpdrFreq (f)) sprintf (cbuf, "%0.2fMHz", f);
	else strcpy (cbuf, na);
	vlist.des->GetItem (2)->SetValue (cbuf);

	sprintf (cbuf, "%g kg", vessel->Mass());
	vlist.prm->GetItem (0)->SetValue (cbuf);
	sprintf (cbuf, "%g kg", vessel->EmptyMass());
	vlist.prm->GetItem (1)->SetValue (cbuf);
	sprintf (cbuf, "%g kg", vessel->FuelMass());
	vlist.prm->GetItem (2)->SetValue (cbuf);
	strcpy (cbuf, DistStr (vessel->Size())+1); strcat (cbuf, "m");
	vlist.prm->GetItem (3)->SetValue (cbuf);
	sprintf (cbuf, "(%4g, %4g, %4g) m²", vessel->PMI().x, vessel->PMI().y, vessel->PMI().z);
	vlist.prm->GetItem (4)->SetValue (cbuf);

	if (vlist.thr) {
		for (i = j = 0; i < 3; i++) {
			if (showth[i]) {
				double th = vessel->GetThrusterGroupMaxth (thgrp[i]);
				if (th) {
					sprintf (cbuf, "%sN", FloatStr (th)+1);
					vlist.thr->GetItem (j)->SetValue (cbuf);
				} else vlist.thr->GetItem (j)->SetValue (na);
				j++;
			}
		}
	}
	
	const Elements *el = vessel->Els();
	if (el && ref) {
		vlist.els->GetItem (0)->SetValue (ref->Name());
		sprintf (cbuf, "%s m", SciStr (el->a, 5));
		vlist.els->GetItem (1)->SetValue (cbuf);
		sprintf (cbuf, "%g", el->e);
		vlist.els->GetItem (2)->SetValue (cbuf);
		sprintf (cbuf, "%0.2f°", el->i*DEG);
		vlist.els->GetItem (3)->SetValue (cbuf);
		sprintf (cbuf, "%0.2f°", el->theta*DEG);
		vlist.els->GetItem (4)->SetValue (cbuf);
		sprintf (cbuf, "%0.2f°", el->omegab*DEG);
		vlist.els->GetItem (5)->SetValue (cbuf);
		sprintf (cbuf, "%0.2f°", el->MeanLng()*DEG);
		vlist.els->GetItem (6)->SetValue (cbuf);
	} else {
		for (i = 0; i < 7; i++)
			vlist.els->GetItem (i)->SetValue (na);
	}

	const SurfParam *sp = vessel->GetSurfParam();
	if (sp) {
		vlist.srf->GetItem (0)->SetValue (sp->ref->Name());
		sprintf (cbuf, "%07.3f°%c  %06.3f°%c", fabs(sp->lng)*DEG, sp->lng >= 0.0 ? 'E':'W', fabs(sp->lat)*DEG, sp->lat >= 0.0 ? 'N':'S');
		vlist.srf->GetItem (1)->SetValue (cbuf);
		strcpy (cbuf, DistStr (sp->alt)); strcat (cbuf, "m");
		vlist.srf->GetItem (2)->SetValue (cbuf+1);
		sprintf (cbuf, "%sm/s", FloatStr (sp->groundspd)+1);
		vlist.srf->GetItem (3)->SetValue (cbuf);
		Vector V (mul (sp->L2H, tmul (vessel->ProxyBody()->GRot(), sp->groundvel_glob)));
		sprintf (cbuf, "%sm/s", FloatStr (V.y)+1);
		vlist.srf->GetItem (4)->SetValue (cbuf);
		sprintf (cbuf, "%0.0f°", sp->dir*DEG);
		vlist.srf->GetItem (5)->SetValue (cbuf);
		sprintf (cbuf, "%0.0f°", sp->pitch*DEG);
		vlist.srf->GetItem (6)->SetValue (cbuf);
		sprintf (cbuf, "%0.0f° %s", fabs(sp->bank)*DEG, sp->bank >= 0.0 ? "left":"right");
		vlist.srf->GetItem (7)->SetValue (cbuf);
	} else {
		for (i = 0; i < 8; i++)
			vlist.srf->GetItem (i)->SetValue (na);
	}

	if (vessel->isInAtmosphere()) {
		double T, rho, p;
		vessel->AtmTemperature (T);
		vessel->AtmPressureAndDensity (p, rho);
		sprintf (cbuf, "%0.2f K", T);
		vlist.atm->GetItem (0)->SetValue (cbuf);
		sprintf (cbuf, "%0.4g kg/m^3", rho);
		vlist.atm->GetItem (1)->SetValue (cbuf);
		sprintf (cbuf, "%sPa", FloatStr(p)+1);
		vlist.atm->GetItem (2)->SetValue (cbuf);
	} else {
		for (i = 0; i < 3; i++)
			vlist.atm->GetItem (i)->SetValue (na);
	}

	if (vessel->isInAtmosphere()) {
		double dynp, M, L, D;
		vessel->DynPressure (dynp);
		L = vessel->GetLift();
		D = vessel->GetDrag();
		sprintf (cbuf, "%sPa", FloatStr(dynp)+1);
		vlist.aer->GetItem (0)->SetValue (cbuf);
		if (sp) sprintf (cbuf, "%sm/s", FloatStr(sp->airspd)+1);
		else    strcpy (cbuf, na);
		vlist.aer->GetItem (1)->SetValue (cbuf);
		vessel->MachNumber (M);
		sprintf (cbuf, "%g", M);
		vlist.aer->GetItem (2)->SetValue (cbuf);
		sprintf (cbuf, "%sN", FloatStr(L)+(L >= 0 ? 1:0));
		vlist.aer->GetItem (3)->SetValue (cbuf);
		sprintf (cbuf, "%sN", FloatStr(D)+1);
		vlist.aer->GetItem (4)->SetValue (cbuf);
		sprintf (cbuf, "%sN", FloatStr(vessel->GetWeight())+1);
		vlist.aer->GetItem (5)->SetValue (cbuf);
		if (D) sprintf (cbuf, "%g", L/D);
		else   strcpy (cbuf, na);
		vlist.aer->GetItem (6)->SetValue (cbuf);
		if (sp) sprintf (cbuf, "%+0.1f°", -atan2 (sp->groundvel_ship.y, sp->groundvel_ship.z)*DEG);
		else    strcpy (cbuf, na);
		vlist.aer->GetItem (7)->SetValue (cbuf);
	} else {
		for (i = 0; i < 8; i++)
			vlist.aer->GetItem (i)->SetValue (na);
	}

	if (vlist.dck) {
		for (i = 0; i < vlist.dck->ItemCount(); i++) {
			if (i < vessel->nDock()) {
				if (vessel->GetDockParams(i)->ids)
					sprintf (cbuf, "IDS %06.2f ", vessel->GetDockParams(i)->ids->GetFreq());
				else cbuf[0] = '\0';
				Vessel *mate = vessel->DockMate (i);
				if (mate) sprintf (cbuf+strlen(cbuf), "[Docked to %s]", mate->Name());
				else strcat (cbuf, "[free]");
				vlist.dck->GetItem (i)->SetValue (cbuf);
			} else vlist.dck->GetItem (i)->SetValue (na);
		}
	}
	if (vessel->GetStatus() == FLIGHTSTATUS_LANDED) {
		vlist.prp->GetItem (0)->SetValue ("IDLE (landed)");
		for (i = 1; i < 4; i++) vlist.prp->GetItem (i)->SetValue (na);
	} else if (vessel->isAttached()) {
		vlist.prp->GetItem (0)->SetValue ("PASSIVE (attached)");
		for (i = 1; i < 4; i++) vlist.prp->GetItem (i)->SetValue (na);
	} else {
		vlist.prp->GetItem (0)->SetValue ("ACTIVE (dynamic)");
		vlist.prp->GetItem (1)->SetValue (vessel->isOrbitStabilised() ? "stabilised" : vessel->CurPropagatorStr());
		sprintf (cbuf, "%d", vessel->CurPropagatorSubsamples());
		vlist.prp->GetItem (2)->SetValue (cbuf);
		cbuf[0] = '\0';
		const GFieldData &gfd = vessel->GetGFieldData();
		for (i = 0; i < gfd.ngrav; i++) {
			if (i) strcat (cbuf, ", ");
			strcat (cbuf, g_psys->GetGravObj(gfd.gravidx[i])->Name());
		}
		vlist.prp->GetItem (3)->SetValue (cbuf);
	}
}

// ======================================================================

void DlgInfo::InitItems_celbody (HWND hDlg, const CelestialBody *cbody)
{
	const int nlabel_des = 3;
	const char *label_des[nlabel_des] = {
		"Name",
		"Primary",
		"Solar system"
	};
	const int nlabel_prm = 7;
	const char *label_prm[nlabel_prm] = {
		"Mass",
		"Mean radius",
		"Gravitational moments",
		"Siderial day",
		"Orbit period",
		"Obliquity of ecliptic",
		"Atmosphere"
	};
	const int nlabel_atm = 5;
	const char *label_atm[nlabel_atm] = {
		"Atmosphere model",
		"Surface pressure",
		"Surface density",
		"Specific gas constant",
		"Specific heat ratio"
	};
	const int nlabel_els = 6;
	const char *label_els[nlabel_els] = {
		"Semi-major axis (a)",
		"Eccentricity (e)",
		"Inclination (i)",
		"Longitude of asc. node",
		"Longitude of periapsis",
		"Mean longitude"
	};
	const int nlabel_loc = 2;
	const char *label_loc[nlabel_loc] = {
		"Right ascension (RA)",
		"Declination (Dec)"
	};
	const int nlabel_ecl = 3;
	const char *label_ecl[nlabel_ecl] = {
		"Longitude",
		"Latitude",
		"Radial distance"
	};
	const int nlabel_prp = 2;
	const char *label_prp[nlabel_prp] = {
		"Mode",
		"Gravity sources"
	};

	int i;
	PropertyItem *item;
	pl.ClearGroups();

	cblist.des = pl.AppendGroup();
	cblist.des->SetTitle ("Designation");
	for (i = 0; i < nlabel_des; i++) {
		item = pl.AppendItem (cblist.des);
		item->SetLabel (label_des[i]);
	}
	cblist.prm = pl.AppendGroup();
	cblist.prm->SetTitle ("Physical parameters");
	for (i = 0; i < nlabel_prm; i++) {
		item = pl.AppendItem (cblist.prm);
		item->SetLabel (label_prm[i]);
	}
	if (cbody->Type() == OBJTP_PLANET && ((Planet*)cbody)->AtmParams()) {
		cblist.atm = pl.AppendGroup();
		cblist.atm->SetTitle ("Atmosphere");
		for (i = 0; i < nlabel_atm; i++) {
			item = pl.AppendItem (cblist.atm);
			item->SetLabel (label_atm[i]);
		}
	} else cblist.atm = NULL;
	cblist.els = pl.AppendGroup();
	cblist.els->SetTitle ("Osculating elements (ecliptic frame)");
	for (i = 0; i < nlabel_els; i++) {
		item = pl.AppendItem (cblist.els);
		item->SetLabel (label_els[i]);
	}
	if (strcmp (cbody->Name(), "Earth")) {
		cblist.loc = pl.AppendGroup();
		cblist.loc->SetTitle ("Geocentric celestial position");
		for (i = 0; i < nlabel_loc; i++) {
			item = pl.AppendItem (cblist.loc);
			item->SetLabel (label_loc[i]);
		}
	} else cblist.loc = NULL;
	if (cbody->Els()) {
		cblist.ecl = pl.AppendGroup();
		cblist.ecl->SetTitle ("Ecliptic position from primary");
		for (i = 0; i < nlabel_ecl; i++) {
			item = pl.AppendItem (cblist.ecl);
			item->SetLabel (label_ecl[i]);
		}
	} else cblist.ecl = NULL;
	cblist.prp = pl.AppendGroup();
	cblist.prp->SetTitle ("State propagation");
	for (i = 0; i < nlabel_prp - (cbody->canDynamicPosVel() ? 0:1); i++) {
		item = pl.AppendItem (cblist.prp);
		item->SetLabel (label_prp[i]);
	}
}

// ======================================================================

void DlgInfo::UpdateItems_celbody ()
{
	CelestialBody *cbody = (CelestialBody*)body;
	CELBODY *cb = cbody->GetModuleInterface();
	Planet *planet = 0;
	const Elements *el = 0;
	char cbuf[256];
	int i, nj;

	switch (body->Type()) {
	case OBJTP_PLANET:
		planet = (Planet*)body;
		el = planet->Els();
		break;
	}

	cblist.des->GetItem (0)->SetValue (cbody->Name());
	cblist.des->GetItem (1)->SetValue (cbody->ElRef() ? cbody->ElRef()->Name() : na);
	const std::string& psys_name = g_psys->Name();
	cblist.des->GetItem (2)->SetValue (psys_name.empty() ? na : psys_name.c_str());

	sprintf (cbuf, "%s kg", SciStr (cbody->Mass(), 4));
	cblist.prm->GetItem (0)->SetValue (cbuf);
	sprintf (cbuf, "%s m", SciStr (cbody->Size(), 4));
	cblist.prm->GetItem (1)->SetValue (cbuf);
	if (nj = cbody->nJcoeff()) {
		cbuf[0] = '\0';
		for (i = 0; i < nj; i++) {
			sprintf (cbuf + strlen(cbuf), "J%d=%s", i+2, SciStr (cbody->Jcoeff (i),3));
			if (i < nj-1) strcat (cbuf, ", ");
		}
	} else strcpy (cbuf, na);
	cblist.prm->GetItem (2)->SetValue (cbuf);
	sprintf (cbuf, "%s s", SciStr (cbody->RotT(), 4));
	cblist.prm->GetItem (3)->SetValue (cbuf);
	if (el) sprintf (cbuf, "%s s", SciStr (el->OrbitT(), 4));
	else    strcpy (cbuf, na);
	cblist.prm->GetItem (4)->SetValue (cbuf);
	if (planet) sprintf (cbuf, "%0.2f°", planet->Obliquity()*DEG);
	else strcpy (cbuf, na);
	cblist.prm->GetItem (5)->SetValue (cbuf);
	cblist.prm->GetItem (6)->SetValue (planet && cblist.atm ? "Yes":"No");

	if (planet && cblist.atm) {
		const ATMCONST *ap = planet->AtmParams();
		if (ap) {
			strcpy (cbuf, "Generic");
			if (cb && cb->Version() >= 2) {
				CELBODY2 *cb2 = (CELBODY2*)cb;
				ATMOSPHERE *atm = cb2->GetAtmosphere();
				if (atm) strcpy (cbuf, atm->clbkName());
			}
			cblist.atm->GetItem (0)->SetValue (cbuf);
			sprintf (cbuf, "%sPa", FloatStr (ap->p0)+1);
			cblist.atm->GetItem (1)->SetValue (cbuf);
			sprintf (cbuf, "%skg/m^3", SciStr (ap->rho0));
			cblist.atm->GetItem (2)->SetValue (cbuf);
			sprintf (cbuf, "%0.2fJ/(K kg)", ap->R);
			cblist.atm->GetItem (3)->SetValue (cbuf);
			sprintf (cbuf, "%0.2f", ap->gamma);
			cblist.atm->GetItem (4)->SetValue (cbuf);
		} else {
			for (i = 0; i < 5; i++)
				cblist.atm->GetItem (i)->SetValue (na);
		}
	}

	if (el) {
		sprintf (cbuf, "%s m", SciStr (el->a, 5));
		cblist.els->GetItem (0)->SetValue (cbuf);
		sprintf (cbuf, "%0.5g", el->e);
		cblist.els->GetItem (1)->SetValue (cbuf);
		sprintf (cbuf, "%0.2f°", el->i*DEG);
		cblist.els->GetItem (2)->SetValue (cbuf);
		sprintf (cbuf, "%0.2f°", el->theta*DEG);
		cblist.els->GetItem (3)->SetValue (cbuf);
		sprintf (cbuf, "%0.2f°", el->omegab*DEG);
		cblist.els->GetItem (4)->SetValue (cbuf);
		sprintf (cbuf, "%0.2f°", el->MeanLng()*DEG);
		cblist.els->GetItem (5)->SetValue (cbuf);
	} else {
		for (i = 0; i < 6; i++)
			cblist.els->GetItem (i)->SetValue (na);
	}

	if (cblist.loc) {
		Planet *earth = g_psys->GetPlanet ("Earth");
		if (earth && earth != cbody) {
			Vector p (cbody->GPos() - earth->GPos());
			double r   = p.length();
			double lng = atan2 (p.z, p.x);
			double lat = p.y/r;
			double ra, dc, rah, ram, ras, dcd, dcm, dcs;
			static double ob = earth->Obliquity();
			static double cosob = cos(ob), sinob = sin(ob);
			Ecl2Equ (cosob, sinob, lng, lat, ra, dc);
			ram = modf (posangle(ra) * 24.0/Pi2, &rah) * 60.0;
			ras = modf (ram, &ram) * 60.0;
			dcm = fabs (modf (dc*DEG, &dcd)) * 60.0;
			dcs = modf (dcm, &dcm) * 60.0;
			sprintf (cbuf, "%02.0fh %02.0fm %02.2fs", rah, ram, ras);
			cblist.loc->GetItem (0)->SetValue (cbuf);
			sprintf (cbuf, "%+02.0f° %02.0f' %02.2f''", dcd, dcm, dcs);
			cblist.loc->GetItem (1)->SetValue (cbuf);
		} else {
			for (i = 0; i < 2; i++)
				cblist.loc->GetItem (i)->SetValue (na);
		}
	}

	if (cblist.ecl) {
		if (el) {
			Vector p (cbody->GPos() - cbody->ElRef()->GPos());
			double r   = p.length();
			double lng = atan2 (p.z, p.x);
			double lat = p.y/r;
			sprintf (cbuf, "%0.3f°", DEG*posangle(lng));
			cblist.ecl->GetItem (0)->SetValue (cbuf);
			sprintf (cbuf, "%0.3f°", DEG*lat);
			cblist.ecl->GetItem (1)->SetValue (cbuf);
			sprintf (cbuf, "%s m", SciStr (r));
			cblist.ecl->GetItem (2)->SetValue (cbuf);
		} else {
			for (i = 0; i < 3; i++)
				cblist.ecl->GetItem (i)->SetValue (na);
		}
	}

	if (cbody->canDynamicPosVel()) {
		cblist.prp->GetItem (0)->SetValue ("Numerical");
		if (cblist.prp->ItemCount() > 1) {
			const GFieldData &gfd = cbody->GetGFieldData();
			if (gfd.ngrav) {
				cbuf[0] = '\0';
				for (i = 0; i < gfd.ngrav; i++) {
					strcat (cbuf, g_psys->GetGravObj(gfd.gravidx[i])->Name());
					if (i < gfd.ngrav-1) strcat (cbuf, ", ");
				}
				cblist.prp->GetItem (1)->SetValue (cbuf);
			} else {
				cblist.prp->GetItem (1)->SetValue (na);
			}
		}
	} else {
		sprintf (cbuf, "Analytic (%s)", cb ? "from module" : "2-body");
		cblist.prp->GetItem (0)->SetValue (cbuf);
	}
}

// ======================================================================

void DlgInfo::InitItems_base (HWND hDlg, const Base *base)
{
	const int nlabel_des = 3;
	const char *label_des[nlabel_des] = {
		"Name",
		"Located on",
		"Position"
	};

	int i;
	DWORD j;
	PropertyItem *item;
	pl.ClearGroups();
	blist.des = pl.AppendGroup();
	blist.des->SetTitle ("Designation");
	for (i = 0; i < nlabel_des; i++) {
		item = pl.AppendItem (blist.des);
		item->SetLabel (label_des[i]);
	}
	if (base->nPad()) {
		char cbuf[64];
		blist.pad = pl.AppendGroup();
		blist.pad->SetTitle ("Landing pads");
		for (i = 0; i < base->nPad(); i++) {
			item = pl.AppendItem (blist.pad);
			sprintf (cbuf, "Pad %d", i+1);
			item->SetLabel (cbuf);
		}
	} else blist.pad = NULL;

	if (base->nRwy()) {
		char cbuf[64];
		blist.rwy = pl.AppendGroup();
		blist.rwy->SetTitle ("Runways");
		for (j = 0; j < base->nRwy(); j++) {
			item = pl.AppendItem (blist.rwy);
			const RwySpec *rwy = base->RwyStatus (j);
			int dir = (int)(posangle(rwy->appr1)*DEG*0.1+0.5);
			sprintf (cbuf, "Runway %02d/%02d", dir, (dir+18)%36);
			item->SetLabel (cbuf);
		}
	} else blist.rwy = NULL;

	if (base->nVOR()) {
		for (j = 0; j < base->nVOR(); j++)
			if (base->VOR(j)->Type() == TRANSMITTER_VOR)
				break;
		if (j < base->nVOR()) {
			blist.vor = pl.AppendGroup();
			blist.vor->SetTitle ("VOR transmitters");
			for (j = 0; j < base->nVOR(); j++) {
				const Nav *nav = base->VOR(j);
				if (nav->Type() == TRANSMITTER_VOR) {
					item = pl.AppendItem (blist.vor);
					item->SetLabel (nav->GetId());
				}
			}
		}
	} else blist.vor = NULL;
}

// ======================================================================

void DlgInfo::UpdateItems_base ()
{
	const char *c, *statusstr[3] = {"free", "", "reserved"};
	char cbuf[256];
	double lng, lat;
	int i, j, status;

	Base *base = (Base*)body;
	blist.des->GetItem (0)->SetValue (base->Name());
	blist.des->GetItem (1)->SetValue (base->RefPlanet()->Name());
	base->EquPos (lng, lat);
	sprintf (cbuf, "%07.3f°%c  %06.3f°%c",
		fabs(lng)*DEG, lng >= 0.0 ? 'E':'W',
		fabs(lat)*DEG, lat >= 0.0 ? 'N':'S'
	);
	blist.des->GetItem (2)->SetValue (cbuf);

	if (blist.pad) {
		for (i = 0; i < blist.pad->ItemCount(); i++) {
			if (i < base->nPad()) {
				cbuf[0] = '\0';
				status = base->PadStatus(i)->status;
				if (status == 1) c = base->PadStatus(i)->vessel->Name();
				else c = statusstr[status];
				if (base->PadStatus(i)->nav)
					sprintf (cbuf, "ILS %06.2f ", base->PadStatus(i)->nav->GetFreq());
				sprintf (cbuf+strlen (cbuf), "[%s]", c);
				blist.pad->GetItem(i)->SetValue(cbuf);
			} else blist.pad->GetItem(i)->SetValue(na);
		}
	}

	if (blist.rwy) {
		for (i = 0; i < blist.rwy->ItemCount(); i++) {
			if (i < base->nRwy()) {
				const RwySpec *rwy = base->RwyStatus (i);
				char ils1[20], ils2[20];
				if (rwy->ils1) sprintf (ils1, "%06.2f", rwy->ils1->GetFreq());
				else strcpy (ils1, "--");
				if (rwy->ils2) sprintf (ils2, "%06.2f", rwy->ils2->GetFreq());
				else strcpy (ils2, "--");
				sprintf (cbuf, "ILS %s/%s, length %0.0fm", ils1, ils2, rwy->length);
				blist.rwy->GetItem(i)->SetValue(cbuf);
			} else blist.rwy->GetItem(i)->SetValue(na);
		}
	}

	if (blist.vor) {
		for (i = j = 0; i < base->nVOR(); i++) {
			const Nav *nav = base->VOR(i);
			if (nav->Type() == TRANSMITTER_VOR && j < blist.vor->ItemCount()) {
				sprintf (cbuf, "%06.2f, range %sm", nav->GetFreq(), DistStr (nav->GetRange()));
				blist.vor->GetItem(j++)->SetValue(cbuf);
			}
		}
	}
}

// ======================================================================

BOOL DlgInfo::OnInitDialog (HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	Init (hDlg);
	return TRUE;
}

// ======================================================================

BOOL DlgInfo::OnSize (HWND hDlg, WPARAM wParam, int w, int h)
{
	Size (w, h);
	return DialogWin::OnSize (hDlg, wParam, w, h); // allow default processing
}

// ======================================================================

BOOL DlgInfo::OnCommand (HWND hDlg, WORD id, WORD code, HWND hControl)
{
	switch (id) {
	case IDHELP:
		DefHelpContext.topic = (char*)"/objinfo.htm";
		g_pOrbiter->OpenHelp (&DefHelpContext);
		return TRUE;
	case IDC_INFO_MAP:
		OpenMap();
		return TRUE;
	case IDC_INFO_DDN:
		ExpandAll (hDlg);
		return TRUE;
	case IDC_INFO_DUP:
		CollapseAll (hDlg);
		return TRUE;
	case IDC_INFO_TYPE:
		if (code == CBN_SELCHANGE) {
			BuildObjectList (hDlg);
			SelectionChanged (hDlg);
			return TRUE;
		}
		break;
	case IDC_INFO_NAME:
		if (code == CBN_SELCHANGE) {
			SelectionChanged (hDlg);
			return TRUE;
		}
		break;
	}
	return DialogWin::OnCommand (hDlg, id, code, hControl);
}

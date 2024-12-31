// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
// Camera configuration dialog
// ======================================================================

#define STRICT 1

#include "DlgCamera.h"
#include "DlgMgr.h"
#include "resource.h"
#include "resource2.h"
#include "Orbiter.h"
#include "Psys.h"
#include "Camera.h"
#include <uxtheme.h>
#include "DlgCtrl.h"

using std::min;
using std::max;

extern Orbiter *g_pOrbiter;
extern PlanetarySystem *g_psys;
extern Camera *g_camera;
extern Vessel *g_focusobj;
extern TimeData td;
extern HELPCONTEXT DefHelpContext;

// ======================================================================

DlgCamera::DlgCamera (HINSTANCE hInstance, HWND hParent, void *context)
: DialogWin (hInstance, hParent, IDD_CAMERA, 0, 0, context)
{
	nTab = 0;
	hcontext = 0;
	pos = &g_pOrbiter->Cfg()->CfgWindowPos.DlgCamera;
}

// ======================================================================

DlgCamera::~DlgCamera ()
{
	Clear ();
}

// ======================================================================

void DlgCamera::Update ()
{
	for (int i = 0; i < nTab; i++)
		pTab[i]->Update ();
}

// ======================================================================

void DlgCamera::ModeChanged ()
{
	pTabControl->ModeChanged ();
}

// ======================================================================

void DlgCamera::FovChanged (double fov)
{
	pTabFov->FovChanged (fov);
}

// ======================================================================

void DlgCamera::GObserverChanged (const char *str)
{
	pTabGround->GObserverChanged (str);
}

// ======================================================================

void DlgCamera::Refresh (const Camera *cam)
{
	pTabGround->LockChanged (cam->GroundObserver_TargetLock());
	//pTabFov->FovChanged (cam->Aperture() * 2.0*DEG);
}

// ======================================================================

BOOL DlgCamera::OnInitDialog (HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	HWND hTabFrame = hDlg;

	AddTab (hDlg, pTabControl = new TabControl (hTabFrame), "Control");
	AddTab (hDlg, new TabTarget (hTabFrame), "Target");
	AddTab (hDlg, new TabView (hTabFrame), "Track");
	AddTab (hDlg, pTabGround = new TabGround (hTabFrame), "Ground");
	AddTab (hDlg, pTabFov = new TabFov (hTabFrame), "FoV");
	AddTab (hDlg, new TabPreset (hTabFrame), "Preset");

	SwitchTab (hDlg);
	return TRUE;
}

// ======================================================================

BOOL DlgCamera::OnCommand (HWND hDlg, WORD id, WORD code, HWND hControl)
{
	switch (id) {
	case IDHELP:
		DefHelpContext.topic = HelpContext ();
		g_pOrbiter->OpenHelp (&DefHelpContext);
		return TRUE;
	}
	return DialogWin::OnCommand (hDlg, id, code, hControl);
}

// ======================================================================

BOOL DlgCamera::OnNotify (HWND hDlg, int idCtrl, LPNMHDR pnmh)
{
	if (pnmh->idFrom == IDC_CAM_TAB) {
		if (pnmh->code == TCN_SELCHANGE) SwitchTab (hDlg);
		return TRUE;
	}
	return MSG_DEFAULT;
}

// ======================================================================

BOOL DlgCamera::OnApp (HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	switch (LOWORD (wParam)) {
	case 0: // FOV change notification
		FovChanged (2.0*DEG * *(double*)lParam);
		return TRUE;
	case 1: // Ground observer position changed
		GObserverChanged ((const char*)lParam);
		return TRUE;
	case 2: // Ground camera target lock mode changed
		Refresh ((const Camera*)lParam);
		return TRUE;
	case 3: // Camera mode changed
		ModeChanged ();
		return TRUE;
	}
	return FALSE;
}

// ======================================================================

void DlgCamera::Clear ()
{
	if (nTab) {
		for (int i = 0; i < nTab; i++)
			delete pTab[i];
		delete []pTab;
		pTab = NULL;
		nTab = 0;
	}
}

// ======================================================================

int DlgCamera::AddTab (HWND hDlg, CameraTab *tab, const char *label)
{
	char cbuf[256];
	strcpy (cbuf, label);
	TC_ITEM tie;
	tie.mask = TCIF_TEXT;
	tie.iImage = -1;
	tie.pszText = cbuf;
	SendDlgItemMessage (hDlg, IDC_CAM_TAB, TCM_INSERTITEM, nTab, (LPARAM)&tie);

	CameraTab **tmp = new CameraTab*[nTab+1];
	if (nTab) {
		memcpy (tmp, pTab, nTab*sizeof(CameraTab*));
		delete []pTab;
	}
	pTab = tmp;
	pTab[nTab] = tab;
	return nTab++;
}

// ======================================================================

void DlgCamera::SwitchTab (HWND hDlg)
{
	int pg, cpg = TabCtrl_GetCurSel (GetDlgItem (hDlg, IDC_CAM_TAB));
	for (pg = 0; pg < nTab; pg++)
		if (pg != cpg) pTab[pg]->Show (false);
	pTab[cpg]->Show (true);
	hcontext = pTab[cpg]->HelpContext();
}

// ======================================================================
// ======================================================================

CameraTab::CameraTab (HWND hParentTab, int dlgId, DLGPROC dlgProc)
{
	active = false;
	hParent = hParentTab;
	hTab = CreateDialogParam (g_pOrbiter->GetInstance(), MAKEINTRESOURCE(dlgId), hParentTab, dlgProc, (LPARAM)this);
}

// ======================================================================

CameraTab::~CameraTab ()
{
	DestroyWindow (hTab);
}

// ======================================================================

void CameraTab::Show (bool show)
{
	ShowWindow (hTab, show ? SW_SHOW : SW_HIDE);
	active = show;
}

// ======================================================================

INT_PTR CALLBACK CameraTab::DlgProcInit (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG:
		EnableThemeDialogTexture (hWnd, ETDT_ENABLETAB);
		SetWindowLongPtr (hWnd, DWLP_USER, lParam);
		return TRUE;
	}
	return FALSE;
}


// ======================================================================
// ======================================================================

TabControl::TabControl (HWND hParentTab): CameraTab (hParentTab, IDD_CAM_PG_CONTROL, DlgProc)
{
}

// ======================================================================

char *TabControl::HelpContext () const
{
	return (char*)"/cam_control.htm";
}

// ======================================================================

BOOL TabControl::Init (HWND hWnd)
{
	HICON hIcon;
	HINSTANCE hInst = g_pOrbiter->GetInstance ();
	hIcon = LoadIcon (hInst, MAKEINTRESOURCE(IDI_LARROW));
	SendDlgItemMessage (hWnd, IDC_CAM_CTRL_ROTL, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
	hIcon = LoadIcon (hInst, MAKEINTRESOURCE(IDI_RARROW));
	SendDlgItemMessage (hWnd, IDC_CAM_CTRL_ROTR, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
	hIcon = LoadIcon (hInst, MAKEINTRESOURCE(IDI_UARROW));
	SendDlgItemMessage (hWnd, IDC_CAM_CTRL_ROTU, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
	SendDlgItemMessage (hWnd, IDC_CAM_CTRL_MOVEU, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
	hIcon = LoadIcon (hInst, MAKEINTRESOURCE(IDI_DARROW));
	SendDlgItemMessage (hWnd, IDC_CAM_CTRL_ROTD, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
	SendDlgItemMessage (hWnd, IDC_CAM_CTRL_MOVED, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
	hIcon = LoadIcon (hInst, MAKEINTRESOURCE(IDI_UARROW_TILT));
	SendDlgItemMessage (hWnd, IDC_CAM_CTRL_MOVEIN, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
	hIcon = LoadIcon (hInst, MAKEINTRESOURCE(IDI_DARROW_TILT));
	SendDlgItemMessage (hWnd, IDC_CAM_CTRL_MOVEOUT, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
	hIcon = LoadIcon (hInst, MAKEINTRESOURCE(IDI_LARROW_TILT));
	SendDlgItemMessage (hWnd, IDC_CAM_CTRL_MOVEL, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
	hIcon = LoadIcon (hInst, MAKEINTRESOURCE(IDI_RARROW_TILT));
	SendDlgItemMessage (hWnd, IDC_CAM_CTRL_MOVER, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);

	GAUGEPARAM gp = { 0, 60, GAUGEPARAM::LEFT, GAUGEPARAM::BLACK };
	oapiSetGaugeParams (GetDlgItem (hWnd, IDC_CAM_CTRL_SENSITIVITY), &gp, true);
	int pos = max (0, min (100, (int)((log10 (g_camera->GroundObserver_PanSpeed())+1.0)*10.0)));
	oapiSetGaugePos (GetDlgItem (hWnd, IDC_CAM_CTRL_SENSITIVITY), pos);

	return TRUE;
}

// ======================================================================

void TabControl::Show (bool show)
{
	CameraTab::Show (show);
	if (show) {
		GetCamMode ();
		SetLayout ();
	}
}

// ======================================================================

void TabControl::Update ()
{
	double dt = td.SysDT;

	// Get press status of control buttons
	if (SendDlgItemMessage (hTab, IDC_CAM_CTRL_ROTL, BM_GETSTATE, 0, 0) & BST_PUSHED) {
		if (rot_is_tilt) g_camera->Rotate   ( dt,  0);
		else             g_camera->ShiftPhi   (-dt);
	}
	else if (SendDlgItemMessage (hTab, IDC_CAM_CTRL_ROTR, BM_GETSTATE, 0, 0) & BST_PUSHED) {
		if (rot_is_tilt) g_camera->Rotate   (-dt,  0);
		else             g_camera->ShiftPhi   ( dt);
	}
	else if (SendDlgItemMessage (hTab, IDC_CAM_CTRL_ROTU, BM_GETSTATE, 0, 0) & BST_PUSHED) {
		if (rot_is_tilt) g_camera->Rotate   ( 0,  dt);
		else             g_camera->ShiftTheta (-dt);
	}
	else if (SendDlgItemMessage (hTab, IDC_CAM_CTRL_ROTD, BM_GETSTATE, 0, 0) & BST_PUSHED) {
		if (rot_is_tilt) g_camera->Rotate   ( 0, -dt);
		else             g_camera->ShiftTheta ( dt);
	}
	else if (SendDlgItemMessage (hTab, IDC_CAM_CTRL_MOVEIN, BM_GETSTATE, 0, 0) & BST_PUSHED) {
		if (rot_is_pan)  g_camera->ShiftTheta (-dt);
		else             g_camera->ShiftDist (-dt);
	}
	else if (SendDlgItemMessage (hTab, IDC_CAM_CTRL_MOVEOUT, BM_GETSTATE, 0, 0) & BST_PUSHED) {
		if (rot_is_pan)  g_camera->ShiftTheta ( dt);
		else             g_camera->ShiftDist (dt);
	}
	else if (SendDlgItemMessage (hTab, IDC_CAM_CTRL_MOVEL, BM_GETSTATE, 0, 0) & BST_PUSHED) {
		g_camera->AddPhi (-dt);
	}
	else if (SendDlgItemMessage (hTab, IDC_CAM_CTRL_MOVER, BM_GETSTATE, 0, 0) & BST_PUSHED) {
		g_camera->AddPhi ( dt);
	}
	else if (SendDlgItemMessage (hTab, IDC_CAM_CTRL_MOVEU, BM_GETSTATE, 0, 0) & BST_PUSHED) {
		g_camera->ChangeDist (1.0+dt);
	}
	else if (SendDlgItemMessage (hTab, IDC_CAM_CTRL_MOVED, BM_GETSTATE, 0, 0) & BST_PUSHED) {
		g_camera->ChangeDist (1.0/(1.0+dt));
	}
}

// ======================================================================

void TabControl::SetLayout ()
{
	const int nCockpitCtrl = 5;
	const int CockpitCtrl[nCockpitCtrl] = {
		IDC_CAM_CTRL_ROTL, IDC_CAM_CTRL_ROTR, IDC_CAM_CTRL_ROTU, IDC_CAM_CTRL_ROTD,
		IDC_CAM_CTRL_FORWARD
	};
	const int nTrackCtrl = 6;
	const int TrackCtrl[nTrackCtrl] = {
		IDC_CAM_CTRL_ROTL, IDC_CAM_CTRL_ROTR, IDC_CAM_CTRL_ROTU, IDC_CAM_CTRL_ROTD,
		IDC_CAM_CTRL_MOVEIN, IDC_CAM_CTRL_MOVEOUT
	};
	const int nToFromCtrl = 2;
	const int ToFromCtrl[nToFromCtrl] = {
		IDC_CAM_CTRL_MOVEIN, IDC_CAM_CTRL_MOVEOUT
	};
	const int nGroundFreeCtrl = 12;
	const int GroundFreeCtrl[nGroundFreeCtrl] = {
		IDC_CAM_CTRL_ROTL, IDC_CAM_CTRL_ROTR, IDC_CAM_CTRL_ROTU, IDC_CAM_CTRL_ROTD,
		IDC_CAM_CTRL_MOVEIN, IDC_CAM_CTRL_MOVEOUT, IDC_CAM_CTRL_MOVEL, IDC_CAM_CTRL_MOVER,
		IDC_CAM_CTRL_MOVEU, IDC_CAM_CTRL_MOVED,
		IDC_CAM_CTRL_SENSITIVITY, IDC_CAM_CTRL_SENSITIVITYLABEL
	};
	const int nGroundLockCtrl = 8;
	const int GroundLockCtrl[nGroundLockCtrl] = {
		IDC_CAM_CTRL_ROTL, IDC_CAM_CTRL_ROTR,
		IDC_CAM_CTRL_MOVEIN, IDC_CAM_CTRL_MOVEOUT,
		IDC_CAM_CTRL_MOVEU, IDC_CAM_CTRL_MOVED,
		IDC_CAM_CTRL_SENSITIVITY, IDC_CAM_CTRL_SENSITIVITYLABEL
	};

	int i, nCtrl = 0;
	const int *Ctrl = 0;
	if (extcam) {
		switch (extmode) {
		case CAMERA_TARGETRELATIVE:
		case CAMERA_ABSDIRECTION:
		case CAMERA_GLOBALFRAME:
			nCtrl = nTrackCtrl;
			Ctrl = TrackCtrl;
			break;
		case CAMERA_TARGETTOOBJECT:
		case CAMERA_TARGETFROMOBJECT:
			nCtrl = nToFromCtrl;
			Ctrl = ToFromCtrl;
			break;
		case CAMERA_GROUNDOBSERVER:
			if (ground_lock) {
				nCtrl = nGroundLockCtrl;
				Ctrl = GroundLockCtrl;
			} else {
				nCtrl = nGroundFreeCtrl;
				Ctrl = GroundFreeCtrl;
			}
			break;
		}
	} else {
		nCtrl = nCockpitCtrl;
		Ctrl = CockpitCtrl;
	}

	for (i = IDC_CAM_CTRL_MIN; i < IDC_CAM_CTRL_MAX; i++)
		ShowWindow (GetDlgItem (hTab, i), SW_HIDE);
	for (i = 0; i < nCtrl; i++)
		ShowWindow (GetDlgItem (hTab, Ctrl[i]), SW_SHOW);

	SetWindowText (GetDlgItem (hTab, IDC_CAM_CTRL_ROTGROUP), rot_is_tilt ? "Tilt" : "Rotate");
}

// ======================================================================

void TabControl::ModeChanged ()
{
	if (active) {
		GetCamMode ();
		SetLayout ();
	}
}

// ======================================================================

void TabControl::GetCamMode ()
{
	extmode = g_camera->GetExtMode ();
	intmode = g_camera->GetIntMode ();
	extcam = (g_camera->GetMode () != CAM_COCKPIT);
	ground_lock = g_camera->GroundObserver_TargetLock();
	rot_is_tilt = (!extcam || (extmode == CAMERA_GROUNDOBSERVER && !ground_lock));
	rot_is_pan = (extcam && extmode == CAMERA_GROUNDOBSERVER);
}

// ======================================================================

INT_PTR CALLBACK TabControl::DlgProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CameraTab::DlgProcInit (hWnd, uMsg, wParam, lParam);
	TabControl *pTab = (TabControl*)(uMsg == WM_INITDIALOG ? lParam : GetWindowLongPtr(hWnd,DWLP_USER));

	switch (uMsg) {
	case WM_INITDIALOG:
		return pTab->Init (hWnd);
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_CAM_CTRL_FORWARD:
			g_camera->ResetCockpitDir();
			return TRUE;
		}
		break;
	case WM_HSCROLL:
		switch (GetDlgCtrlID ((HWND)lParam)) {
		case IDC_CAM_CTRL_SENSITIVITY:
			switch (LOWORD(wParam)) {
			case SB_THUMBTRACK:
			case SB_LINELEFT:
			case SB_LINERIGHT:
				g_camera->SetGroundObserver_PanSpeed (pow (10, HIWORD(wParam)/10.0-1.0));
				return 0;
			}
			break;
		}
		break;
	}
	return FALSE;
}


// ======================================================================
// ======================================================================

TabTarget::TabTarget (HWND hParentTab): CameraTab (hParentTab, IDD_CAM_PG_TARGET, DlgProc)
{
}

// ======================================================================

char *TabTarget::HelpContext () const
{
	return (char*)"/cam_target.htm";
}

// ======================================================================

void TabTarget::AddCbodyNode (HWND hWnd, const CelestialBody *cbody, HTREEITEM parent)
{
	char cbuf[256]; cbuf[255] = '\0';
	TV_INSERTSTRUCT tvis;
	tvis.hParent = parent;
	tvis.hInsertAfter = TVI_LAST;
	tvis.item.mask = TVIF_TEXT | TVIF_CHILDREN;
	tvis.item.pszText = cbuf;
	tvis.item.cChildren = (cbody->nSecondary() ? 1:0);
	strncpy (cbuf, cbody->Name(), 255);
	HTREEITEM hti = (HTREEITEM)SendDlgItemMessage (hWnd, IDC_CAM_TGT_TREE, TVM_INSERTITEM, 0, (LPARAM)&tvis);

	// recursively add children
	for (DWORD i = 0; i < cbody->nSecondary(); i++) {
		AddCbodyNode (hWnd, cbody->Secondary(i), hti);
	}
}

// ======================================================================

void TabTarget::AddVessels (HWND hWnd)
{
	char cbuf[256]; cbuf[255] = '\0';
	HTREEITEM hti;
	TV_INSERTSTRUCT tvis;
	tvis.hParent = NULL;
	tvis.hInsertAfter = TVI_LAST;
	tvis.item.mask = TVIF_TEXT | TVIF_CHILDREN;
	tvis.item.pszText = cbuf;
	tvis.item.cChildren = 1;
	strcpy (cbuf, "Vessels");
	hti = (HTREEITEM)SendDlgItemMessage (hWnd, IDC_CAM_TGT_TREE, TVM_INSERTITEM, 0, (LPARAM)&tvis);
	tvis.hParent = hti;
	tvis.item.cChildren = 0;
	for (DWORD i = 0; i < g_psys->nVessel(); i++) {
		strncpy (cbuf, g_psys->GetVessel(i)->Name(), 255);
		SendDlgItemMessage (hWnd, IDC_CAM_TGT_TREE, TVM_INSERTITEM, 0, (LPARAM)&tvis);
	}
}

// ======================================================================

void TabTarget::AddSurfbases (HWND hWnd)
{
	int i, j;
	char cbuf[256]; cbuf[255] = '\0';
	HTREEITEM hti;
	TV_INSERTSTRUCT tvis;
	tvis.hParent = NULL;
	tvis.hInsertAfter = TVI_LAST;
	tvis.item.mask = TVIF_TEXT | TVIF_CHILDREN;
	tvis.item.pszText = cbuf;
	tvis.item.cChildren = 1;
	strcpy (cbuf, "Spaceports");
	hti = (HTREEITEM)SendDlgItemMessage (hWnd, IDC_CAM_TGT_TREE, TVM_INSERTITEM, 0, (LPARAM)&tvis);
	tvis.hParent = hti;
	tvis.item.cChildren = 0;
	for (i = 0; i < g_psys->nGrav(); i++) {
		Body *obj = g_psys->GetGravObj (i);
		if (obj->Type() != OBJTP_PLANET) continue;
		Planet *planet = (Planet*)obj;
		for (j = 0; j < g_psys->nBase(planet); j++) {
			strncpy (cbuf, g_psys->GetBase (planet,j)->Name(), 255);
			SendDlgItemMessage (hWnd, IDC_CAM_TGT_TREE, TVM_INSERTITEM, 0, (LPARAM)&tvis);
		}
	}
}

// ======================================================================

void TabTarget::Show (bool show)
{
	CameraTab::Show (show);

	if (show)
		SetWindowText (GetDlgItem (hTab, IDC_CAM_TGT_INPUT), g_camera->Target()->Name());
}

// ======================================================================

BOOL TabTarget::Init (HWND hWnd)
{
	int i;

	// Fill tree list
	for (i = 0; i < g_psys->nStar(); i++)
		AddCbodyNode (hWnd, g_psys->GetStar(i), NULL);
	AddSurfbases (hWnd);
	AddVessels (hWnd);

	return TRUE;
}

// ======================================================================

INT_PTR CALLBACK TabTarget::DlgProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CameraTab::DlgProcInit (hWnd, uMsg, wParam, lParam);
	TabTarget *pTab = (TabTarget*)(uMsg == WM_INITDIALOG ? lParam : GetWindowLongPtr(hWnd,DWLP_USER));

	switch (uMsg) {
	case WM_INITDIALOG:
		return pTab->Init (hWnd);
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDC_CAM_TGT_APPLY: {
			char cbuf[256];
			GetWindowText (GetDlgItem (hWnd, IDC_CAM_TGT_INPUT), cbuf, 256);
			Body *obj = g_psys->GetObj (cbuf, true);
			if (!obj) obj = g_psys->GetBase (cbuf, true);
			if (obj) {
				g_camera->SetDistance(2.0);
				g_pOrbiter->SetView(obj, 1);
			}
			else {
				SetWindowText (GetDlgItem (hWnd, IDC_CAM_TGT_INPUT), g_camera->Target()->Name());
				MessageBeep (MB_ICONEXCLAMATION);
			}
			} return TRUE;
		case IDC_CAM_TGT_FCOCKPIT:
			if (HIWORD(wParam) == BN_CLICKED) {
				g_camera->SetDistance(2.0);
				g_pOrbiter->SetView (g_focusobj, 0);
				return TRUE;
			}
			break;
		case IDC_CAM_TGT_FEXTERN:
			if (HIWORD(wParam) == BN_CLICKED) {
				g_camera->SetDistance(2.0);
				g_pOrbiter->SetView (g_focusobj, 1);
				return TRUE;
			}
			break;
		}
		break;
	case WM_NOTIFY: {
		NM_TREEVIEW *pnmtv = (NM_TREEVIEW FAR *)lParam;
		if (pnmtv->hdr.code == TVN_SELCHANGING) {
			HTREEITEM hti = pnmtv->itemNew.hItem;
			TV_ITEM tvi;
			char cbuf[256];
			tvi.mask = TVIF_HANDLE|TVIF_TEXT;
			tvi.hItem = hti;
			tvi.pszText = cbuf;
			tvi.cchTextMax = 256;
			SendDlgItemMessage (hWnd, IDC_CAM_TGT_TREE, TVM_GETITEM, 0, (LPARAM)&tvi);
			SetWindowText (GetDlgItem (hWnd, IDC_CAM_TGT_INPUT), cbuf);
		}
		} break;
	}
	return FALSE;
}


// ======================================================================
// ======================================================================

TabView::TabView (HWND hParentTab): CameraTab (hParentTab, IDD_CAM_PG_VIEW, DlgProc)
{
}

// ======================================================================

char *TabView::HelpContext () const
{
	return (char*)"/cam_track.htm";
}

// ======================================================================

BOOL TabView::Init (HWND hWnd)
{
	int i, j;
	DWORD k;

	// Fill camera reference combo box
	for (i = 0; i < g_psys->nStar(); i++)
		SendDlgItemMessage (hWnd, IDC_CAM_VIEW_REFLIST, CB_ADDSTRING, 0, (LPARAM)g_psys->GetStar(i)->Name());
	for (i = 0; i < g_psys->nPlanet(); i++) {
		Planet *planet = g_psys->GetPlanet(i);
		if (planet->isMoon()) continue;
		SendDlgItemMessage (hWnd, IDC_CAM_VIEW_REFLIST, CB_ADDSTRING, 0, (LPARAM)planet->Name());
		for (j = 0; j < planet->nSecondary(); j++)
			SendDlgItemMessage (hWnd, IDC_CAM_VIEW_REFLIST, CB_ADDSTRING, 0, (LPARAM)planet->Secondary(j)->Name());
	}
	for (k = 0; k < g_psys->nVessel(); k++)
		SendDlgItemMessage (hWnd, IDC_CAM_VIEW_REFLIST, CB_ADDSTRING, 0, (LPARAM)g_psys->GetVessel(k)->Name());
	for (k = 0; k < g_psys->nGrav(); k++) {
		if (g_psys->GetGravObj(k)->Type() != OBJTP_PLANET) continue;
		Planet *planet = (Planet*)g_psys->GetGravObj(k);
		for (j = 0; j < g_psys->nBase(planet); j++)
			SendDlgItemMessage (hWnd, IDC_CAM_VIEW_REFLIST, CB_ADDSTRING, 0, (LPARAM)g_psys->GetBase(planet,j)->Name());
	}
	// remove camera target from list
	//i = SendDlgItemMessage (hWnd, IDC_CAM_VIEW_REFLIST, CB_FINDSTRINGEXACT, 0, (LPARAM)g_camera->Target()->Name());
	//if (i != CB_ERR) SendDlgItemMessage (hWnd, IDC_CAM_VIEW_REFLIST, CB_DELETESTRING, i, 0);
		
	// select first item, usually the central star
	SendDlgItemMessage (hWnd, IDC_CAM_VIEW_REFLIST, CB_SETCURSEL, 0, 0);
	return TRUE;

	return TRUE;
}

// ======================================================================

INT_PTR CALLBACK TabView::DlgProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CameraTab::DlgProcInit (hWnd, uMsg, wParam, lParam);
	TabView *pTab = (TabView*)(uMsg == WM_INITDIALOG ? lParam : GetWindowLongPtr(hWnd,DWLP_USER));

	switch (uMsg) {
	case WM_INITDIALOG:
		pTab->Init(hWnd);
		return FALSE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_CAM_VIEW_TGTREL:
			g_camera->SetTrackMode (CAMERA_TARGETRELATIVE);
			return FALSE;
		case IDC_CAM_VIEW_ABSDIR:
			g_camera->SetTrackMode (CAMERA_ABSDIRECTION);
			return FALSE;
		case IDC_CAM_VIEW_GLOBAL:
			g_camera->SetTrackMode (CAMERA_GLOBALFRAME);
			return FALSE;
		case IDC_CAM_VIEW_TGTTO:
		case IDC_CAM_VIEW_TGTFROM: {
			SetFocus(GetDlgItem(hWnd, LOWORD(wParam)));
			char cbuf[256];
			GetWindowText (GetDlgItem (hWnd, IDC_CAM_VIEW_REFLIST), cbuf, 256);
			Body *obj = g_psys->GetObj (cbuf, true);
			if (!obj) obj = g_psys->GetBase (cbuf, true);
			if (obj && obj != g_camera->Target())
				g_camera->SetTrackMode (LOWORD(wParam) == IDC_CAM_VIEW_TGTTO ? CAMERA_TARGETTOOBJECT : CAMERA_TARGETFROMOBJECT, obj);
			} return FALSE;
		}
		break;
	}
	return FALSE;
}


// ======================================================================
// ======================================================================

TabGround::TabGround (HWND hParentTab): CameraTab (hParentTab, IDD_CAM_PG_GROUND, DlgProc)
{
}

// ======================================================================

void TabGround::Show (bool show)
{
	EchoCurrentGroundpos (hTab);
	CameraTab::Show (show);
}

// ======================================================================

char *TabGround::HelpContext () const
{
	return (char*)"/cam_ground.htm";
}

// ======================================================================

void TabGround::SelectPlanet (HWND hWnd, int idx)
{
	int i;
	char cbuf[256];
	SendDlgItemMessage (hWnd, IDC_CAM_GRND_REFBODY, CB_GETLBTEXT, idx, (LPARAM)cbuf);
	SendDlgItemMessage (hWnd, IDC_CAM_GRND_SITE, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage (hWnd, IDC_CAM_GRND_ADDR, CB_RESETCONTENT, 0, 0);
	Planet *planet = g_psys->GetPlanet (cbuf);
	if (!planet) return;

	for (i = 0; i < planet->nGroundObserver(); i++) {
		const GROUNDOBSERVERSPEC *go = planet->GetGroundObserver (i);
		if (SendDlgItemMessage (hWnd, IDC_CAM_GRND_SITE, CB_FINDSTRINGEXACT, 0, (LPARAM)go->site) == CB_ERR)
			SendDlgItemMessage (hWnd, IDC_CAM_GRND_SITE, CB_ADDSTRING, 0, (LPARAM)go->site);
	}
	if (i) {
		SendDlgItemMessage (hWnd, IDC_CAM_GRND_SITE, CB_SETCURSEL, 0, 0);
		SelectSite (hWnd, 0);
	}
}

// ======================================================================

void TabGround::SelectSite (HWND hWnd, int idx)
{
	int i;
	char cbuf[256];
	bool found = false;
	double lng, lat, alt;
	int pidx = SendDlgItemMessage (hWnd, IDC_CAM_GRND_REFBODY, CB_GETCURSEL, 0, 0);
	SendDlgItemMessage (hWnd, IDC_CAM_GRND_REFBODY, CB_GETLBTEXT, pidx, (LPARAM)cbuf);
	Planet *planet = g_psys->GetPlanet (cbuf);
	if (!planet) return;

	SendDlgItemMessage (hWnd, IDC_CAM_GRND_SITE, CB_GETLBTEXT, idx, (LPARAM)cbuf);
	SendDlgItemMessage (hWnd, IDC_CAM_GRND_ADDR, CB_RESETCONTENT, 0, 0);

	for (i = 0; i < planet->nGroundObserver(); i++) {
		const GROUNDOBSERVERSPEC *go = planet->GetGroundObserver (i);
		if (!strcmp (go->site, cbuf)) {
			SendDlgItemMessage (hWnd, IDC_CAM_GRND_ADDR, CB_ADDSTRING, 0, (LPARAM)go->addr);
			if (!found) {
				lng = go->lng;
				lat = go->lat;
				alt = go->alt;
			}
			found = true;
		}
	}
	if (found) {
		SendDlgItemMessage (hWnd, IDC_CAM_GRND_ADDR, CB_SETCURSEL, 0, 0);
		EchoGroundpos (hWnd, lng, lat, alt);
	}
}

// ======================================================================

void TabGround::SelectAddr (HWND hWnd, int idx)
{
	char cbuf[256], sitestr[256], addrstr[256];

	int pidx = SendDlgItemMessage (hWnd, IDC_CAM_GRND_REFBODY, CB_GETCURSEL, 0, 0);
	SendDlgItemMessage (hWnd, IDC_CAM_GRND_REFBODY, CB_GETLBTEXT, pidx, (LPARAM)cbuf);
	Planet *planet = g_psys->GetPlanet (cbuf);
	if (!planet) return;

	GetWindowText (GetDlgItem (hWnd, IDC_CAM_GRND_SITE), sitestr, 256);
	SendDlgItemMessage (hWnd, IDC_CAM_GRND_ADDR, CB_GETLBTEXT, idx, (LPARAM)addrstr);
	const GROUNDOBSERVERSPEC *go = planet->GetGroundObserver (sitestr, addrstr);
	if (go) {
		EchoGroundpos (hWnd, go->lng, go->lat, go->alt);
	}
}

// ======================================================================

void TabGround::EchoCurrentGroundpos (HWND hWnd)
{
	char cbuf[256];
	SendDlgItemMessage (hWnd, IDC_CAM_GRND_REFBODY, WM_GETTEXT, 256, (LPARAM)cbuf);
	Planet *planet = g_psys->GetPlanet (cbuf);
	if (!planet) return;

	double lng, lat, alt;
	planet->GlobalToEquatorial (*g_camera->GPosPtr(), lng, lat, alt);
	alt -= planet->Size() + planet->Elevation (lng, lat);
	bool changed = EchoGroundpos (hWnd, lng, lat, alt);

	if (changed) {
		SetWindowText (GetDlgItem (hWnd, IDC_CAM_GRND_SITE), "");
		SetWindowText (GetDlgItem (hWnd, IDC_CAM_GRND_ADDR), "");
	}
}

// ======================================================================

bool TabGround::EchoGroundpos (HWND hWnd, double lng, double lat, double alt)
{
	bool changed = false;
	char cbuf[256], ccbuf[256];

	sprintf (cbuf, "%+0.6f", lng*DEG);
	GetWindowText (GetDlgItem (hWnd, IDC_CAM_GRND_LNG), ccbuf, 256);
	if (strcmp (cbuf, ccbuf)) {
		SetWindowText (GetDlgItem (hWnd, IDC_CAM_GRND_LNG), cbuf);
		changed = true;
	}
	sprintf (cbuf, "%+0.6f", lat*DEG);
	GetWindowText (GetDlgItem (hWnd, IDC_CAM_GRND_LAT), ccbuf, 256);
	if (strcmp (cbuf, ccbuf)) {
		SetWindowText (GetDlgItem (hWnd, IDC_CAM_GRND_LAT), cbuf);
		changed = true;
	}
	sprintf (cbuf, "%0.4g", alt);
	GetWindowText (GetDlgItem (hWnd, IDC_CAM_GRND_ALT), ccbuf, 256);
	if (strcmp (cbuf, ccbuf)) {
		SetWindowText (GetDlgItem (hWnd, IDC_CAM_GRND_ALT), cbuf);
		changed = true;
	}
	return changed;
}

// ======================================================================

void TabGround::ApplyObserver (HWND hWnd)
{
	char cbuf[256], site[256], addr[256];
	SendDlgItemMessage (hWnd, IDC_CAM_GRND_REFBODY, WM_GETTEXT, 256, (LPARAM)cbuf);
	Planet *planet = g_psys->GetPlanet (cbuf);
	if (!planet) return;

	double lng, lat, alt = 1.7;
	bool ok;
	GetWindowText (GetDlgItem (hWnd, IDC_CAM_GRND_LNG), cbuf, 256);
	ok = (sscanf (cbuf, "%lf", &lng) == 1);
	GetWindowText (GetDlgItem (hWnd, IDC_CAM_GRND_LAT), cbuf, 256);
	ok = ok && (sscanf (cbuf, "%lf", &lat) == 1);
	GetWindowText (GetDlgItem (hWnd, IDC_CAM_GRND_ALT), cbuf, 256);
	ok = ok && (sscanf (cbuf, "%lf", &alt) == 1);

	double altlimit;
	GetWindowText (GetDlgItem (hWnd, IDC_EDIT1), cbuf, 256);
	if (sscanf (cbuf, "%lf", &altlimit))
		g_camera->SetGroundObserver_TerrainLimit (max(1.0,altlimit));

	if (ok)
		g_camera->SetGroundMode (CAMERA_GROUNDOBSERVER, planet, lng*RAD, lat*RAD, alt, NULL);

	return;


	SendDlgItemMessage (hWnd, IDC_CAM_GRND_SITE, WM_GETTEXT, 256, (LPARAM)site);
	SendDlgItemMessage (hWnd, IDC_CAM_GRND_ADDR, WM_GETTEXT, 256, (LPARAM)addr);

	if (sscanf (site, "%lf%lf%lf", &lng, &lat, &alt) >= 2) {
		lng *= RAD; lat *= RAD; alt = max (alt, 1.0);
		SendDlgItemMessage (hWnd, IDC_CAM_GRND_ADDR, WM_SETTEXT, 0, (LPARAM)"");
	} else {
		const GROUNDOBSERVERSPEC *go = planet->GetGroundObserver (site, addr);
		if (!go) return;
		lng = go->lng, lat = go->lat, alt = go->alt;
	}

	Camera::GroundObserverSite gos;
	strncpy (gos.planet, cbuf, 63);
	strncpy (gos.site, site, 63);
	strncpy (gos.addr, addr, 63);
	g_camera->SetGroundMode (CAMERA_GROUNDOBSERVER, planet, lng, lat, alt, &gos);
}

// ======================================================================

void TabGround::SetCurrentGroundpos (HWND hWnd)
{
	EchoCurrentGroundpos (hWnd);
	if (g_camera->GetExtMode() == CAMERA_GROUNDOBSERVER) return; // nothing to do
	ApplyObserver (hWnd);
	if (SendDlgItemMessage (hWnd, IDC_CAM_GRND_LOCK, BM_GETCHECK, 0, 0) == BST_UNCHECKED) {
		// force lock to target to initialise camera direction
		g_camera->SetGroundObserver_TargetLock (true);
		g_camera->SetGroundObserver_TargetLock (false);
	}
	char cbuf[256];
	double alt;
	GetWindowText (GetDlgItem (hWnd, IDC_EDIT1), cbuf, 256);
	if (sscanf (cbuf, "%lf", &alt))
		g_camera->SetGroundObserver_TerrainLimit (max(1.0,alt));
}

// ======================================================================

void TabGround::GObserverChanged (const char *str)
{
	if (active)
		EchoCurrentGroundpos (hTab);
}

// ======================================================================

void TabGround::LockChanged (bool lock)
{
	SendDlgItemMessage (hTab, IDC_CAM_GRND_LOCK, BM_SETCHECK, lock ? BST_CHECKED : BST_UNCHECKED, 0);
}

// ======================================================================

BOOL TabGround::Init (HWND hWnd)
{
	int i, j;

	for (i = 0; i < g_psys->nPlanet(); i++) {
		Planet *planet = g_psys->GetPlanet(i);
		if (planet->isMoon()) continue;
		SendDlgItemMessage (hWnd, IDC_CAM_GRND_REFBODY, CB_ADDSTRING, 0, (LPARAM)planet->Name());
		for (j = 0; j < planet->nSecondary(); j++)
			SendDlgItemMessage (hWnd, IDC_CAM_GRND_REFBODY, CB_ADDSTRING, 0, (LPARAM)planet->Secondary(j)->Name());
	}
	SelectPlanet (hWnd, 0);

	// restore last ground observer address
	const Camera::GroundObserverSite *gos = g_camera->GetGroundObserverSite();
	if (gos->planet[0]) {
		i = SendDlgItemMessage (hWnd, IDC_CAM_GRND_REFBODY, CB_FINDSTRINGEXACT, -1, (LPARAM)gos->planet);
		SendDlgItemMessage (hWnd, IDC_CAM_GRND_REFBODY, CB_SETCURSEL, i, 0);
		SelectPlanet (hWnd, i);
		i = SendDlgItemMessage (hWnd, IDC_CAM_GRND_SITE, CB_FINDSTRINGEXACT, -1, (LPARAM)gos->site);
		SendDlgItemMessage (hWnd, IDC_CAM_GRND_SITE, CB_SETCURSEL, i, 0);
		SelectSite (hWnd, i);
		i = SendDlgItemMessage (hWnd, IDC_CAM_GRND_ADDR, CB_FINDSTRINGEXACT, -1, (LPARAM)gos->addr);
		SendDlgItemMessage (hWnd, IDC_CAM_GRND_ADDR, CB_SETCURSEL, i, 0);
	} else { // nothing stored; use planet closest to camera instead
		i = SendDlgItemMessage (hWnd, IDC_CAM_GRND_REFBODY, CB_FINDSTRINGEXACT, -1, (LPARAM)g_camera->ProxyPlanet()->Name());
		if (i == CB_ERR) i = 0;
		SendDlgItemMessage (hWnd, IDC_CAM_GRND_REFBODY, CB_SETCURSEL, i, 0);
		SelectPlanet (hWnd, i);
	}

	SendDlgItemMessage (hWnd, IDC_CAM_GRND_LOCK, BM_SETCHECK, (WPARAM)g_camera->GroundObserver_TargetLock(), 0);

	char cbuf[256];
	sprintf (cbuf, "%0.0lf", g_camera->GroundObserver_TerrainLimit());
	SetWindowText (GetDlgItem (hWnd, IDC_EDIT1), cbuf);

	return TRUE;
}

// ======================================================================

INT_PTR CALLBACK TabGround::DlgProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CameraTab::DlgProcInit (hWnd, uMsg, wParam, lParam);
	TabGround *pTab = (TabGround*)(uMsg == WM_INITDIALOG ? lParam : GetWindowLongPtr (hWnd, DWLP_USER));

	switch (uMsg) {
	case WM_INITDIALOG:
		return pTab->Init (hWnd);
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_CAM_GRND_REFBODY:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				int idx = SendDlgItemMessage (hWnd, IDC_CAM_GRND_REFBODY, CB_GETCURSEL, 0, 0);
				pTab->SelectPlanet (hWnd, idx);
			}
			return TRUE;
		case IDC_CAM_GRND_SITE:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				int idx = SendDlgItemMessage (hWnd, IDC_CAM_GRND_SITE, CB_GETCURSEL, 0, 0);
				pTab->SelectSite (hWnd, idx);
			}
			return TRUE;
		case IDC_CAM_GRND_ADDR:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				int idx = SendDlgItemMessage (hWnd, IDC_CAM_GRND_ADDR, CB_GETCURSEL, 0, 0);
				pTab->SelectAddr (hWnd, idx);
			}
			return TRUE;
		case IDC_CAM_GRND_CURRENT:
			pTab->SetCurrentGroundpos (hWnd);
			return TRUE;
		case IDC_CAM_GRND_APPLY:
			pTab->ApplyObserver (hWnd);
			return TRUE;
		case IDC_CAM_GRND_LOCK: {
			int idx = SendDlgItemMessage (hWnd, IDC_CAM_GRND_LOCK, BM_GETCHECK, 0, 0);
			g_camera->SetGroundObserver_TargetLock (idx == BST_CHECKED ? true : false);
			} return TRUE;
		case IDC_EDIT1: {
			char cbuf[256];
			double alt;
			GetWindowText (GetDlgItem (hWnd, IDC_EDIT1), cbuf, 256);
			if (sscanf (cbuf, "%lf", &alt)) g_camera->SetGroundObserver_TerrainLimit (alt);
			} return TRUE;
		}
		break;
	}
	return FALSE;
}


// ======================================================================
// ======================================================================

TabFov::TabFov (HWND hParentTab): CameraTab (hParentTab, IDD_CAM_PG_FOV, DlgProc)
{
}

// ======================================================================

char *TabFov::HelpContext () const
{
	return (char*)"/cam_fov.htm";
}

// ======================================================================

void TabFov::FovChanged (double fov)
{
	RegisterFov (hTab, fov);
}

// ======================================================================

void TabFov::RegisterFov (HWND hWnd, double fov)
{
	char cbuf[128];
	SendDlgItemMessage (hWnd, IDC_CAM_FOV_SLIDER, TBM_SETPOS, TRUE, (LONG)fov);
	GetWindowText (GetDlgItem (hWnd, IDC_CAM_FOV_INPUT), cbuf, 127);
	if (fabs (fov-atof(cbuf)) > 1e-5) {
		sprintf (cbuf, "%0.2f", fov);
		SetWindowText (GetDlgItem (hWnd, IDC_CAM_FOV_INPUT), cbuf);
	}
}

// ======================================================================

void TabFov::SetFov (double fov_deg)
{
	if (fov_deg >= 10.0 && fov_deg <= 90.0)
		g_pOrbiter->SetFOV (fov_deg * 0.5*RAD);
}

// ======================================================================

INT_PTR CALLBACK TabFov::DlgProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CameraTab::DlgProcInit (hWnd, uMsg, wParam, lParam);
	TabFov *pTab = (TabFov*)(uMsg == WM_INITDIALOG ? lParam : GetWindowLongPtr(hWnd,DWLP_USER));

	switch (uMsg) {
	case WM_INITDIALOG:
		// initialise the slider
		SendDlgItemMessage (hWnd, IDC_CAM_FOV_SLIDER, TBM_SETRANGE, FALSE, MAKELONG(10,90));
		SendDlgItemMessage (hWnd, IDC_CAM_FOV_SLIDER, TBM_SETPAGESIZE, 0, 10L);
		SendDlgItemMessage (hWnd, IDC_CAM_FOV_SLIDER, TBM_SETTICFREQ, 10, 0);
		// set controls to current FOV setting
		pTab->RegisterFov (hWnd, g_camera->Aperture()*2.0*DEG);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDC_CAM_FOV_30:
		case IDC_CAM_FOV_40:
		case IDC_CAM_FOV_50:
		case IDC_CAM_FOV_60:
		case IDC_CAM_FOV_70:
			pTab->SetFov ((LOWORD(wParam)-IDC_CAM_FOV_30+3)*10.0);
			return TRUE;
		}
		case IDC_CAM_FOV_INPUT:
			if (HIWORD (wParam) == EN_UPDATE) {
				char cbuf[256];
				GetWindowText ((HWND)lParam, cbuf, 256);
				pTab->SetFov (atof(cbuf));
				return TRUE;
			}
			break;
		break;
	case WM_HSCROLL: // trackbar notifications
		if (LOWORD (wParam) == TB_THUMBTRACK) {
			pTab->SetFov ((double)SendDlgItemMessage (hWnd, IDC_CAM_FOV_SLIDER, TBM_GETPOS, 0, 0));
			return TRUE;
		}
		break;
	}
	return FALSE;
}


// ======================================================================
// ======================================================================

TabPreset::TabPreset (HWND hParentTab): CameraTab (hParentTab, IDD_CAM_PG_PRESET, DlgProc)
{
}

// ======================================================================

char *TabPreset::HelpContext () const
{
	return (char*)"/cam_preset.htm";
}

// ======================================================================

INT_PTR CALLBACK TabPreset::DlgProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CameraTab::DlgProcInit (hWnd, uMsg, wParam, lParam);

	DWORD i, j;
	char cbuf[256];

	switch (uMsg) {
	case WM_INITDIALOG:
		for (i = 0; i < g_camera->nPreset(); i++) {
			g_camera->GetPreset(i)->GetDescr (cbuf, 256);
			SendDlgItemMessage (hWnd, IDC_CAM_PRE_LIST, LB_ADDSTRING, 0, (LPARAM)cbuf);
		}
		if (i) SendDlgItemMessage (hWnd, IDC_CAM_PRE_LIST, LB_SETCURSEL, 0, 0);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDHELP:
			DefHelpContext.topic = (char*)"/cam_preset.htm";
			g_pOrbiter->OpenHelp (&DefHelpContext);
			return TRUE;
		case IDC_CAM_PRE_NEW:
			i = g_camera->AddPreset();
			g_camera->GetPreset(i)->GetDescr (cbuf, 256);
			SendDlgItemMessage (hWnd, IDC_CAM_PRE_LIST, LB_ADDSTRING, 0, (LPARAM)cbuf);
			SendDlgItemMessage (hWnd, IDC_CAM_PRE_LIST, LB_SETCURSEL, i, 0);
			return TRUE;
		case IDC_CAM_PRE_DEL:
			i = SendDlgItemMessage (hWnd, IDC_CAM_PRE_LIST, LB_GETCURSEL, 0, 0);
			if (i != LB_ERR && g_camera->DelPreset (i)) {
				SendDlgItemMessage (hWnd, IDC_CAM_PRE_LIST, LB_DELETESTRING, i, 0);
				j = SendDlgItemMessage (hWnd, IDC_CAM_PRE_LIST, LB_GETCOUNT, 0, 0);
				if (j > i) SendDlgItemMessage (hWnd, IDC_CAM_PRE_LIST, LB_SETCURSEL, i, 0);
				else if (i > 0) SendDlgItemMessage (hWnd, IDC_CAM_PRE_LIST, LB_SETCURSEL, i-1, 0);
			}
			return TRUE;
		case IDC_CAM_PRE_RECALL:
			i = SendDlgItemMessage (hWnd, IDC_CAM_PRE_LIST, LB_GETCURSEL, 0, 0);
			if (i != LB_ERR) g_camera->RecallPreset (i);
			return TRUE;
		case IDC_CAM_PRE_CLEAR:
			g_camera->ClearPresets();
			SendDlgItemMessage (hWnd, IDC_CAM_PRE_LIST, LB_RESETCONTENT, 0, 0);
			return TRUE;
		case IDC_CAM_PRE_LIST:
			if (HIWORD(wParam) == LBN_DBLCLK) {
				i = SendDlgItemMessage (hWnd, IDC_CAM_PRE_LIST, LB_GETCURSEL, 0, 0);
				if (i != LB_ERR) g_camera->RecallPreset (i);
				return TRUE;
			}
			break;
		}
		break;
	}
	return FALSE;
}

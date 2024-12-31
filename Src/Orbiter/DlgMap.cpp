// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
// "Map" dialog
// ======================================================================

#define STRICT 1

#include <windows.h>
#include "DlgMap.h"
#include "Dialogs.h"
#include "Orbiter.h"
#include "Psys.h"
#include "Planet.h"
#include "resource.h"
#include "DlgMgr.h" // to be removed
#include "Pane.h"
#include "Util.h"

using std::min;
using std::max;

extern Orbiter *g_pOrbiter;
extern TimeData td;
extern Vessel *g_focusobj;
extern PlanetarySystem *g_psys;
extern HELPCONTEXT DefHelpContext;
extern char DBG_MSG[256];

static int count = 0;

inline bool GetMapPos (double lng, double lat, int &x, int &y);

MapWin *MapWin::map_in_creation = 0;

// ======================================================================

MapWin::MapWin (DlgMap *pDlg): VectorMap ()
{
	dlg = pDlg;
	hWnd = NULL;
	bPaintPending = false;
	bForceUpdate = false;
	bDragActive = false;
	storeflag = dispflag;
	mousemode = MOUSE_DRAG;
	RegisterWindow (pDlg->GetHinst());
}

// ======================================================================

MapWin::~MapWin ()
{
	UnregisterWindow (dlg->GetHinst());
}

// ======================================================================

void MapWin::RegisterWindow (HINSTANCE hInst)
{
	// Register map window class
	WNDCLASS wndClass;
	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc   = Map_WndProc;
	wndClass.cbClsExtra    = 0;
	wndClass.cbWndExtra    = 0;
	wndClass.hInstance     = hInst;
	wndClass.hIcon         = NULL;
	wndClass.hCursor       = NULL;
	wndClass.hbrBackground = NULL;
	wndClass.lpszMenuName  = NULL;
	wndClass.lpszClassName = "MapWindow";
	RegisterClass (&wndClass);
}

// ======================================================================

void MapWin::UnregisterWindow (HINSTANCE hInst)
{
	UnregisterClass ("MapWindow", hInst);
}

// ======================================================================

int MapWin::Create ()
{
	DlgMap::MAP_PARAM *prm = &dlg->MapPrm;
	font = CreateFont (-9, 0, 0, 0, 400, 0, 0, 0, 0, 3, 2, 1, 49, "Arial");
	pen[0] = CreatePen (PS_SOLID, 3, 0xffff00);
	pen[1] = CreatePen (PS_SOLID, 1, 0xff0000);

	return 0;
}

// ======================================================================

int MapWin::Destroy ()
{
	int i;

	DeleteObject (font);
	for (i = 0; i < 2; i++) DeleteObject (pen[i]);
	return 0;
}

// ======================================================================

void MapWin::PostCreation (HWND hw)
{
	hWnd = hw;
	SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR)this);
}

// ======================================================================

void MapWin::Update (bool force)
{
	const double updDT = 1.0;
	static double updTmin = 0.0, updTmax = 0.0;

	if (!hWnd) return;
	if (force) bForceUpdate = true;

#ifdef ASYNC_DRAWMAP
	// needs a rethink
	if (!ThreadBusy()) {
		bPaintPending = true;
		InvalidateRect (hWnd, NULL, FALSE);
	}
	if (!bPaintPending && (bForceUpdate || td.SysT > updTmax || td.SysT < updTmin)) {
		updTmax = td.SysT + updDT;
		updTmin = td.SysT - updDT;
		VectorMap::Update ();
		AsyncDrawMap ();
		bForceUpdate = false;
	}
#else
	if (bForceUpdate || td.SysT1 > updTmax || td.SysT1 < updTmin) {
		updTmax = td.SysT1 + updDT;
		updTmin = td.SysT1 - updDT;
		VectorMap::Update ();
		DrawMap();
		InvalidateRect(hWnd, NULL, FALSE);
		bPaintPending = true; // ?
		bForceUpdate = false;
	}
#endif
}

// ======================================================================

bool MapWin::SetCBody (const CelestialBody *body)
{
	bool redraw;
	if (redraw = VectorMap::SetCBody (body)) {
		for (int i = 0; i < mkrset.nset; i++)
			mkrset.set[i].active = true;
		if (hWnd) Update (true);
	}
	return redraw;
}

// ======================================================================

int MapWin::Paint ()
{
	HDC hDCtgt;
	PAINTSTRUCT ps;

	hDCtgt = BeginPaint (hWnd, &ps);
	if (bPaintPending) {
		HDC hDCsrc = GetDeviceContext();
		HBITMAP pBmp = (HBITMAP)SelectObject (hDCsrc, GetMap());
		BitBlt (hDCtgt, 0, 0, cw, ch, hDCsrc, 0, 0, SRCCOPY);
		SelectObject (hDCsrc, pBmp);
	}
	EndPaint (hWnd, &ps);
	bPaintPending = false;
	return 0;
}

// ======================================================================

int MapWin::Size (DWORD w, DWORD h)
{
	HDC hDC = GetDC(NULL);
	SetCanvas (hDC, w, h);
	ReleaseDC (NULL, hDC);
	Update (true);
	return 0;
}

// ======================================================================

void MapWin::Pan (int dx, int dy)
{
	double lng = lngc - dx/scalefac;
	double lat = max (-Pi05, min (Pi05, latc + dy/scalefac));
	if (lng != lngc || lat != latc) {
		SetCenter (lng, lat);
		Update(true);
	}
}

// ======================================================================

void MapWin::OnLButtonDown (int mx, int my)
{
	if (mousemode == MOUSE_DRAG) {
		bDragActive = true;
		mousex = mx;
		mousey = my;
		storeflag = GetDisplayFlags();
		SetDisplayFlags (storeflag & ~(DISP_NAVAID | DISP_CUSTOM1));
		Update(true);
	} else {
		FindTarget (mx, my);
	}
}

// ======================================================================

void MapWin::OnLButtonUp (int mx, int my)
{
	if (bDragActive) {
		bDragActive = false;
		SetDisplayFlags (storeflag);
		dlg->prm_store.lngcnt = lngc;
		dlg->prm_store.latcnt = latc;
		Update(true);
	}
}

// ======================================================================

void MapWin::OnMouseMove (int mx, int my)
{
	if (GetForegroundWindow() == dlg->hWnd) SetFocus (hWnd);
	SetCursor (LoadCursor (NULL, mousemode == MOUSE_DRAG ? IDC_HAND : IDC_CROSS));
	if (bDragActive) {
		int dx = mx-mousex;
		int dy = my-mousey;
		mousex = mx;
		mousey = my;
		Pan (dx, dy);
	}
}

// ======================================================================

void MapWin::OnMouseWheel (int wdelta)
{
	if (!wdelta) return; // sanity check
	double zscale;
	if (wdelta > 0)
		zscale = (double)wdelta/(double)WHEEL_DELTA*2.0;
	else
		zscale = -(double)WHEEL_DELTA/(double)wdelta*0.5;
	int z = (int)(ZoomFac() * zscale + 0.5);
	dlg->SetZoom (z);
}

// ======================================================================

void MapWin::SetMouseMode (MouseMode mode)
{
	mousemode = mode;
}

// ======================================================================

bool MapWin::FindTarget (int mx, int my)
{
	const OBJTYPE obj = FindObject (mx, my);
	if (obj.type) {
		SetSelection (obj);
		dlg->EchoSelection (obj);
	}
	dlg->EnableInfo (obj.type == DISP_BASE || obj.type == DISP_VESSEL || obj.type == DISP_MOON);

	Update (true);
	return (obj.type != 0);
}

// ======================================================================

MapWin *MapWin::GetMapInstance (HWND hw)
{
	MapWin *map = (MapWin*)GetWindowLongPtr (hw, GWLP_USERDATA);
	if (!map) map = map_in_creation;
	return map;
}

// ======================================================================

LRESULT FAR PASCAL MapWin::Map_WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_CREATE:
		return GetMapInstance (hWnd)->Create ();
	case WM_DESTROY:
		return GetMapInstance (hWnd)->Destroy ();
	case WM_SIZE:
		return GetMapInstance (hWnd)->Size (LOWORD(lParam), HIWORD(lParam));
	case WM_PAINT:
		return GetMapInstance (hWnd)->Paint ();
	//case WM_LBUTTONDBLCLK:
	//	return GetMapInstance (hWnd)->GetInfo (LOWORD(lParam), HIWORD(lParam));
	case WM_LBUTTONDOWN:
		SetCapture (hWnd);
		GetMapInstance(hWnd)->OnLButtonDown (LOWORD(lParam), HIWORD(lParam));
		return 0;
	case WM_LBUTTONUP:
		ReleaseCapture();
		GetMapInstance(hWnd)->OnLButtonUp (LOWORD(lParam), HIWORD(lParam));
		return 0;
	case WM_MOUSEMOVE:
		GetMapInstance(hWnd)->OnMouseMove (LOWORD(lParam), HIWORD(lParam));
		return 0;
	case WM_MOUSEWHEEL:
		GetMapInstance(hWnd)->OnMouseWheel (GET_WHEEL_DELTA_WPARAM(wParam));
		return 0;
	}
	return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

// ======================================================================
// ======================================================================

const int nbtl = 1;
const int btlid[nbtl] = {IDC_MAP_TRACK};
const int nbtr = 4;
const int btrid[nbtr] = {IDC_MAP_INFO, IDC_MAP_OPTIONS, IDCANCEL, IDHELP};

DlgMap::MapPrmPersist DlgMap::prm_store = { NULL };

DlgMap::DlgMap (HINSTANCE hInstance, HWND hParent, void *context)
: DialogWin (hInstance, hParent, IDD_MAP, 0, 0, context)
{
	map = new MapWin (this);
	prm = &g_pOrbiter->Cfg()->CfgMapPrm;
	pos = &g_pOrbiter->Cfg()->CfgWindowPos.DlgMap;
	memset (&MapPrm, 0, sizeof(MAP_PARAM));
}

// ======================================================================

DlgMap::~DlgMap ()
{
	prm_store.sel = map->GetSelection();
	prm_store.track = (map->GetCenterMode() != 0);
	delete map;
}

// ======================================================================

void DlgMap::GlobalInit ()
{
	prm_store.cbody = NULL;
	prm_store.sel.obj = NULL;
	prm_store.sel.type = 0;
	prm_store.zoom = 1;
	prm_store.lngcnt = 0.0;
	prm_store.latcnt = 0.0;
	prm_store.track = false;
}

#ifdef UNDEF
// ======================================================================

HWND DlgMap::Open ()
{
	DialogManager *dlgmgr = g_pOrbiter->DlgMgr();
	if (!dlgmgr) return 0;
	HINSTANCE hInst = g_pOrbiter->GetInstance();
	HWND hParent = g_pOrbiter->GetRenderWnd();
	if (dlgmgr->IsEntry (hInst, IDD_MAP)) return 0; // already open
	DlgMap *dlg = new DlgMap (hInst, hParent);
	return dlgmgr->AddEntry (dlg);
}
#endif

// ======================================================================

HWND DlgMap::OpenWindow ()
{
	MapWin::map_in_creation = map;
	HWND hw = DialogWin::OpenWindow ();
	HWND hMap = GetDlgItem (hw, IDC_MAP);
	map->PostCreation (hMap);
	MapWin::map_in_creation = 0;
	return hw;
}

// ======================================================================

void DlgMap::Update ()
{
	map->Update();
}

// ======================================================================

void DlgMap::VesselDeleting (Vessel *v)
{
	if (v == map->GetSelection().obj) {
		VectorMap::OBJTYPE nullobj = {NULL,0};
		EchoSelection (nullobj);
		map->UnsetSelection();
	}
}

// ======================================================================

void DlgMap::SetCBody (CelestialBody *cbody)
{
	if (cbody != prm_store.cbody) {
		prm_store.cbody = cbody;
		map->SetCBody (cbody);
		VectorMap::OBJTYPE nullobj = {NULL,0};
		EchoSelection (nullobj);
	}
}

// ======================================================================

void DlgMap::SetPlanet (const char *name)
{
	CelestialBody *cbody = g_psys->GetGravObj (name, true);
	if (cbody != prm_store.cbody) {
		prm_store.cbody = cbody;
		map->SetCBody (cbody);
	}
}

// ======================================================================

void DlgMap::EchoSelection (const VectorMap::OBJTYPE &obj)
{
	HWND hNameBox = GetDlgItem (hWnd, IDC_MAP_FINDNAME);
	switch (obj.type) {
	case DISP_VESSEL:
	case DISP_BASE:
	case DISP_MOON:
		SetWindowText (hNameBox, ((Body*)obj.obj)->Name());
		break;
	case DISP_NAVAID:
		SetWindowText (hNameBox, ((Nav*)obj.obj)->GetId());
		break;
	default:
		SetWindowText (hNameBox, "");
		break;
	}
}

// ======================================================================

void DlgMap::SetSelection (const Body *body)
{
	const CelestialBody *ref;
	VectorMap::OBJTYPE sel;
	sel.obj = body;
	switch (body->Type()) {
		case OBJTP_VESSEL:
			sel.type = DISP_VESSEL;
			ref = ((Vessel*)body)->ElRef();
			break;
		case OBJTP_PLANET:
			sel.type = DISP_MOON;
			ref = ((CelestialBody*)body)->ElRef();
			break;
		case OBJTP_STAR:
			// Only way for this is to select the target as the cbody, and drop the selection
			ref = (const CelestialBody*)body;
			sel.obj = NULL;
			sel.type = 0;
			break;
		case OBJTP_SURFBASE:
			sel.type = DISP_BASE;
			ref = ((Base*)body)->RefPlanet();
			break;
	}
	const CelestialBody *cbody = GetMapWin()->GetCBody();
	if (cbody != ref) {
		GetMapWin()->SetCBody(ref);
		CBodySelectComboBox::BuildListFromNode (GetHwnd(), IDC_MAP_REFERENCE, ref);
	}
	SetSelection (sel);
}

// ======================================================================

void DlgMap::SetSelection (const VectorMap::OBJTYPE &obj)
{
	if (map->SetSelection (obj))
		EchoSelection (obj);
}

// ======================================================================

bool DlgMap::SetSelection (const char *name, int type)
{
	if (!name[0]) return false; // sanity check

	const int maxhit = 15;
	const char *hitstr[maxhit];
	int nhit = 0, len = strlen(name);
	DWORD i;
	VectorMap::OBJTYPE sel;
	bool found_exact = false;

	if ((type == 1 || type == 0) && map->GetDisplayFlags() & DISP_VESSEL) { // search for vessel
		for (i = 0; i < g_psys->nVessel(); i++) {
			Vessel *v = g_psys->GetVessel(i);
			if (!_strnicmp (v->Name(), name, len)) {
				if (nhit < maxhit) hitstr[nhit] = v->Name();
				nhit++;
				if (!found_exact && !_stricmp (v->Name(), name)) {
					sel.type = DISP_VESSEL;
					sel.obj = v;
					found_exact = true;
				}
			}
		}
	}
	if ((type == 2 || type == 0) && map->GetDisplayFlags() & DISP_BASE) { // search for bases
		const Planet *planet = map->GetPlanet();
		if (planet) {
			for (i = 0; i < planet->nBase(); i++) {
				const Base *base = planet->GetBase(i);
				if (!_strnicmp (base->Name(), name, len)) {
					if (nhit < maxhit) hitstr[nhit] = base->Name();
					nhit++;
					if (!found_exact && !_stricmp (base->Name(), name)) {
						sel.type = DISP_BASE;
						sel.obj = base;
						found_exact = true;
					}
				}
			}
		}
	}
	if ((type == 3 || type == 0) && map->GetDisplayFlags() & DISP_NAVAID) { // search for VOR transmitters
		const Planet *planet = map->GetPlanet();
		if (planet) {
			for (i = 0; i < planet->nNav(); i++) {
				const Nav *nav = planet->NavMgr().GetNav(i);
				if (nav->Type() == TRANSMITTER_VOR) {
					const Nav_VOR *vor = (const Nav_VOR*)nav;
					if (!_strnicmp (vor->GetId(), name, len)) {
						if (nhit < maxhit) hitstr[nhit] = vor->GetId();
						nhit++;
						if (!found_exact && !_stricmp (vor->GetId(), name)) {
							sel.type = DISP_NAVAID;
							sel.obj = vor;
							found_exact = true;
						}
					}
				}
			}
		}
	}
	if ((type == 5 || type == 0) && map->GetDisplayFlags() & DISP_MOON) { // search for moons
		for (i = 0; i < map->GetCBody()->nSecondary(); i++) {
			const CelestialBody *moon = map->GetCBody()->Secondary (i);
			if (!_strnicmp (moon->Name(), name, len)) {
				if (nhit < maxhit) hitstr[nhit] = moon->Name();
				nhit++;
				if (!found_exact && !_stricmp (moon->Name(), name)) {
					sel.type = DISP_MOON;
					sel.obj = moon;
					found_exact = true;
				}
			}
		}
	}

	SendDlgItemMessage (hWnd, IDC_MAP_FINDNAME, CB_RESETCONTENT, 0, 0);

	if (found_exact) {
		map->SetSelection (sel);
		SendDlgItemMessage (hWnd, IDC_MAP_FINDNAME, CB_ADDSTRING, 0, (LPARAM)name);
		SendDlgItemMessage (hWnd, IDC_MAP_FINDNAME, CB_SHOWDROPDOWN, FALSE, 0);
		SendDlgItemMessage (hWnd, IDC_MAP_FINDNAME, CB_SETCURSEL, 0, 0);
		EnableInfo (sel.type == DISP_BASE || sel.type == DISP_VESSEL || sel.type == DISP_MOON);
	} else {
		if (nhit <= maxhit) {
			for (i = 0; i < nhit; i++)
				SendDlgItemMessage (hWnd, IDC_MAP_FINDNAME, CB_ADDSTRING, 0, (LPARAM)hitstr[i]);
			SendDlgItemMessage (hWnd, IDC_MAP_FINDNAME, CB_SHOWDROPDOWN, nhit > 0 ? TRUE:FALSE, 0);
		}
		SetWindowText (GetDlgItem (hWnd, IDC_MAP_FINDNAME), name);
	}
	SendDlgItemMessage (hWnd, IDC_MAP_FINDNAME, CB_SETEDITSEL, 0, MAKELPARAM(len,len));
	SendMessage (GetDlgItem (hWnd, IDC_MAP_FINDNAME), WM_SETCURSOR, 0, 0);

	return found_exact;
}

// ======================================================================

void DlgMap::ToggleDispFlags (DWORD dflag)
{
	SetDispFlags (prm->DispFlag ^ dflag);
}

// ======================================================================

void DlgMap::SetDispFlags (DWORD flag)
{
	prm->DispFlag = flag;
	map->SetDisplayFlags (prm->DispFlag);
	map->Update (true);
}

// ======================================================================

void DlgMap::SetDragMode (bool drag)
{
	map->SetMouseMode (drag ? MapWin::MOUSE_DRAG : MapWin::MOUSE_SELECT);
}

// ======================================================================

void DlgMap::EnableInfo (bool enable)
{
	EnableWindow (GetDlgItem (hWnd, IDC_MAP_INFO), enable ? 1:0);
}

// ======================================================================

void DlgMap::OpenInfo ()
{
	VectorMap::OBJTYPE obj = map->GetSelection ();
	if (obj.type == DISP_BASE || obj.type == DISP_VESSEL || obj.type == DISP_MOON) {
		DlgInfo *pInfo = g_pOrbiter->DlgMgr()->EnsureEntry<DlgInfo> ();
		pInfo->SetBody ((Body*)obj.obj);

		RECT r;
		GetWindowRect (hWnd, &r);
		SetWindowPos (pInfo->GetHwnd(), NULL, r.right, r.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	}
}

// ======================================================================

BOOL DlgMap::OnInitDialog (HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	int i;
	RECT rect;
	char cbuf[64];

	SendDlgItemMessage (hDlg, IDC_MAP_SELECT, BM_SETIMAGE, IMAGE_ICON, (LPARAM)LoadIcon (g_pOrbiter->GetInstance(), MAKEINTRESOURCE(IDI_CROSS)));
	SendDlgItemMessage (hDlg, IDC_MAP_DRAG, BM_SETIMAGE, IMAGE_ICON, (LPARAM)LoadIcon (g_pOrbiter->GetInstance(), MAKEINTRESOURCE(IDI_FINGER)));
	SendDlgItemMessage (hDlg, IDC_MAP_DRAG, BM_SETCHECK, BST_CHECKED, 0);
	map->SetZoom (prm_store.zoom);
	sprintf (cbuf, "Zoom %dx", prm_store.zoom);
	SetWindowText (GetDlgItem (hDlg, IDC_MAP_ZOOMBOX), cbuf);
	map->SetDisplayFlags (prm->DispFlag);
	GetClientRect (hDlg, &rect);
	MapPrm.dlgw = rect.right, MapPrm.dlgh = rect.bottom;
	GetWindowRect (GetDlgItem (hDlg, IDC_MAP), &rect);
	MapPrm.mapw = rect.right-rect.left, MapPrm.maph = rect.bottom-rect.top;
	for (i = 0; i < nbtl; i++) {
		GetWindowRect (GetDlgItem (hDlg, btlid[i]), &rect);
		MapPrm.btlp[i].x = rect.left, MapPrm.btlp[i].y = rect.top;
		ScreenToClient (hDlg, &MapPrm.btlp[i]);
	}
	for (i = 0; i < nbtr; i++) {
		GetWindowRect (GetDlgItem (hDlg, btrid[i]), &rect);
		MapPrm.btrp[i].x = rect.left, MapPrm.btrp[i].y = rect.top;
		ScreenToClient (hDlg, &MapPrm.btrp[i]);
	}

	if (!prm_store.cbody) prm_store.cbody = g_focusobj->ProxyPlanet();
	CBodySelectComboBox::BuildListFromNode (hDlg, IDC_MAP_REFERENCE, prm_store.cbody);
	map->SetCBody (prm_store.cbody);
	map->SetCenter (prm_store.lngcnt, prm_store.latcnt);
	if (map->SetSelection (prm_store.sel))
		EchoSelection (prm_store.sel);
	if (prm_store.track) {
		SendDlgItemMessage (hDlg, IDC_MAP_TRACK, BM_SETCHECK, BST_CHECKED, 0);
		map->SetCenterMode (2);
	}
	SendDlgItemMessage (hDlg, IDC_MAP_FINDTYPE, CB_RESETCONTENT, 0, 0);
	const char *tpstr[6] = {"Any", "Vessel", "Base", "VOR", "Marker", "Moons"};
	for (i = 0; i < 6; i++)
		SendDlgItemMessage (hDlg, IDC_MAP_FINDTYPE, CB_ADDSTRING, 0, (LPARAM)tpstr[i]);
	SendDlgItemMessage (hDlg, IDC_MAP_FINDTYPE, CB_SETCURSEL, 0, 0);

	return TRUE;
}

// ======================================================================

BOOL DlgMap::OnSize (HWND hDlg, WPARAM wParam, int w, int h)
{
	RECT rect;
	int i, dx, dy;

	GetClientRect (hWnd, &rect);
	dx = rect.right - MapPrm.dlgw, dy = rect.bottom - MapPrm.dlgh;
	SetWindowPos (GetDlgItem (hWnd, IDC_MAP), HWND_TOP, 0, 0, MapPrm.mapw+dx, MapPrm.maph+dy, SWP_NOMOVE);
	for (i = 0; i < nbtr; i++) ShowWindow (GetDlgItem (hWnd, btrid[i]), SW_HIDE);
	for (i = 0; i < nbtr; i++)
		SetWindowPos (GetDlgItem (hWnd, btrid[i]), HWND_TOP, MapPrm.btrp[i].x+dx, MapPrm.btrp[i].y+dy, 0, 0, SWP_NOSIZE);
	for (i = 0; i < nbtr; i++) ShowWindow (GetDlgItem (hWnd, btrid[i]), SW_SHOW);
	for (i = 0; i < nbtl; i++)
		SetWindowPos (GetDlgItem (hWnd, btlid[i]), HWND_TOP, MapPrm.btlp[i].x, MapPrm.btlp[i].y+dy, 0, 0, SWP_NOSIZE);
	return DialogWin::OnSize (hDlg, wParam, w, h);
}

// ======================================================================

BOOL DlgMap::OnCommand (HWND hDlg, WORD id, WORD code, HWND hControl)
{
	char cbuf[1024];
	int i;

	switch (id) {
	case IDHELP:
		DefHelpContext.topic = (char*)"/map.htm";
		g_pOrbiter->OpenHelp (&DefHelpContext);
		return TRUE;
	case IDC_MAP_OPTIONS:
		g_pOrbiter->DlgMgr()->EnsureEntry<DlgMapOpt> (0, this);
		return TRUE;
	case IDC_MAP_INFO:
		OpenInfo();
		return TRUE;
	case IDC_MAP_REFERENCE:
		if (code == CBN_SELCHANGE) {
			CelestialBody *cbody = CBodySelectComboBox::OnSelectionChanged (hDlg, IDC_MAP_REFERENCE);
			if (cbody) SetCBody (cbody);
		}
		break;
	case IDC_MAP_FINDTYPE:
		if (code == CBN_SELCHANGE) {
			SetFindMask ();
		}
		break;
	case IDC_MAP_FINDNAME:
		if (code == CBN_EDITCHANGE) {
			GetWindowText (GetDlgItem (hDlg, IDC_MAP_FINDNAME), cbuf, 256);
			i = SendDlgItemMessage (hDlg, IDC_MAP_FINDTYPE, CB_GETCURSEL, 0, 0);
			SetSelection (cbuf, i);
		} else if (code == CBN_SELCHANGE) {
			i = SendDlgItemMessage (hDlg, IDC_MAP_FINDNAME, CB_GETCURSEL, 0, 0);
			if (i != CB_ERR) {
				SendDlgItemMessage (hDlg, IDC_MAP_FINDNAME, CB_GETLBTEXT, i, (LPARAM)cbuf);
				i = SendDlgItemMessage (hDlg, IDC_MAP_FINDTYPE, CB_GETCURSEL, 0, 0);
				SetSelection (cbuf, i);
			}
		} else if (code == CBN_SELENDCANCEL) {
			i = SendDlgItemMessage (hDlg, IDC_MAP_FINDNAME, CB_GETCURSEL, 0, 0);
			if (i == CB_ERR) return 0;
		}
		break;
	case IDC_MAP_TRACK:
		if (code == BN_CLICKED) {
			bool track = (SendDlgItemMessage (hDlg, IDC_MAP_TRACK, BM_GETCHECK, 0, 0) == BST_CHECKED);
			map->SetCenterMode (track ? 2:0);
		}
		return TRUE;
	case IDC_MAP_DRAG:
	case IDC_MAP_SELECT:
		if (code == BN_CLICKED)
			SetDragMode (id == IDC_MAP_DRAG);
		return TRUE;
	}
	return DialogWin::OnCommand (hDlg, id, code, hControl);
}

// ======================================================================

BOOL DlgMap::OnNotify (HWND hWnd, int idCtrl, LPNMHDR pnmh)
{
	if (pnmh->code == UDN_DELTAPOS) {
		NMUPDOWN *nmud = (NMUPDOWN*)pnmh;
		switch (pnmh->idFrom) {
		case IDC_MAP_ZOOM:
			if (nmud->iDelta < 0) ZoomIn();
			else                  ZoomOut();
			break;
		}
	}
	return DialogWin::OnNotify (hWnd, idCtrl, pnmh);
}

// ======================================================================

BOOL DlgMap::OnUserMessage (HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	switch (wParam) {
	case MSG_KILLVESSEL:
		VesselDeleting ((Vessel*)lParam);
		break;
	}
	return DialogWin::OnUserMessage (hWnd, wParam, lParam);
}

// ======================================================================

BOOL DlgMap::OnMouseWheel (HWND hDlg, int vk, int dist, int x, int y)
{
	PostMessage (GetDlgItem (hDlg, IDC_MAP), WM_MOUSEWHEEL, MAKEWPARAM(vk,dist), MAKELPARAM(x,y));
	return 1;
}

// ======================================================================

void DlgMap::SetZoom (int zoom)
{
	char cbuf[32];
	zoom = max(1, min (128, zoom));
	if (zoom != prm_store.zoom) {
		map->SetZoom (zoom);
		prm_store.zoom = zoom;
		map->Update (true);
		sprintf (cbuf, "Zoom %dx", zoom);
		SetWindowText (GetDlgItem (hWnd, IDC_MAP_ZOOMBOX), cbuf);
		InvalidateRect (GetDlgItem (hWnd, IDC_MAP), NULL, FALSE);
	}
}

// ======================================================================

void DlgMap::ZoomIn ()
{
	SetZoom (prm_store.zoom*2);
}

// ======================================================================

void DlgMap::ZoomOut ()
{
	SetZoom (prm_store.zoom/2);
}

// ======================================================================

void DlgMap::SetFindMask ()
{
	DWORD mask;
	int idx = SendDlgItemMessage (hWnd, IDC_MAP_FINDTYPE, CB_GETCURSEL, 0, 0);
	switch (idx) {
	case 0: mask = DISP_NAVAID | DISP_BASE | DISP_CUSTOM1 | DISP_VESSEL | DISP_MOON; break;
	case 1: mask = DISP_VESSEL; break;
	case 2: mask = DISP_BASE; break;
	case 3: mask = DISP_NAVAID; break;
	case 4: mask = DISP_CUSTOM1; break;
	case 5: mask = DISP_MOON; break;
	}
	map->SetFindFlags (mask);
}

// ======================================================================
// ======================================================================

DlgMapOpt::DlgMapOpt (HINSTANCE hInstance, HWND hParent, void *context)
: DialogWin (hInstance, hParent, IDD_MAP_CONFIG, 0, 0, context)
{
	dlgmap = (DlgMap*)context;
}

// ======================================================================

BOOL DlgMapOpt::OnInitDialog (HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	DWORD dispflag = GetMapDlg()->GetMapWin()->GetDisplayFlags();
	SendDlgItemMessage (hDlg, IDC_MAPOPT_ORBITFOCUS, BM_SETCHECK, dispflag & DISP_ORBITFOCUS ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_MAPOPT_ORBITSEL, BM_SETCHECK, dispflag & DISP_ORBITSEL ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_MAPOPT_ORBITPLANE, BM_SETCHECK, dispflag & DISP_ORBITPLANE ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_MAPOPT_ORBITGTRACK, BM_SETCHECK, dispflag & DISP_GROUNDTRACK ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_MAPOPT_HORIZON, BM_SETCHECK, dispflag & DISP_HORIZONLINE ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_MAPOPT_TLINE, BM_SETCHECK, dispflag & DISP_TERMINATOR_LINE ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_MAPOPT_TSHADE, BM_SETCHECK, dispflag & DISP_TERMINATOR_SHADE ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_MAPOPT_GRIDLINES, BM_SETCHECK, dispflag & DISP_GRIDLINE ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_MAPOPT_COASTLINES, BM_SETCHECK, dispflag & DISP_COASTLINE ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_MAPOPT_CONTOURS, BM_SETCHECK, dispflag & DISP_CONTOURS ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_MAPOPT_BASES, BM_SETCHECK, dispflag & DISP_BASE ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_MAPOPT_VORS, BM_SETCHECK, dispflag & DISP_NAVAID ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_MAPOPT_LANDMARKS, BM_SETCHECK, dispflag & DISP_CUSTOM1 ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_MAPOPT_SATELLITES, BM_SETCHECK, dispflag & DISP_MOON ? BST_CHECKED:BST_UNCHECKED, 0);
	int i, vesselmode = (dispflag & DISP_VESSEL ? dispflag & DISP_FOCUSONLY ? 1:0:2);
	const int modeid[3] = {IDC_MAPOPT_VESSELALL, IDC_MAPOPT_VESSELFOCUS, IDC_MAPOPT_VESSELNONE};
	for (i = 0; i < 3; i++)
		SendDlgItemMessage (hDlg, modeid[i], BM_SETCHECK, i==vesselmode ? BST_CHECKED:BST_UNCHECKED, 0);
	return 0;
}

// ======================================================================

BOOL DlgMapOpt::OnCommand (HWND hDlg, WORD id, WORD code, HWND hControl)
{
	switch (id) {
	case IDC_MAPOPT_VESSELALL:
	case IDC_MAPOPT_VESSELFOCUS:
	case IDC_MAPOPT_VESSELNONE: {
		int i, vmode;
		const int modeid[3] = {IDC_MAPOPT_VESSELALL, IDC_MAPOPT_VESSELFOCUS, IDC_MAPOPT_VESSELNONE};
		for (i = 0; i < 3; i++)
			if (SendDlgItemMessage (hDlg, modeid[i], BM_GETCHECK, 0, 0) == BST_CHECKED)
				break;
		vmode = (i==1 ? DISP_VESSEL | DISP_FOCUSONLY : i==2 ? 0 : DISP_VESSEL);
		SetVesselMode (vmode);
		} return TRUE;
	case IDC_MAPOPT_ORBITFOCUS:
		ToggleDispFlag (DISP_ORBITFOCUS);
		return TRUE;
	case IDC_MAPOPT_ORBITSEL:
		ToggleDispFlag (DISP_ORBITSEL);
		return TRUE;
	case IDC_MAPOPT_ORBITPLANE:
	case IDC_MAPOPT_ORBITGTRACK:
		SetOrbitMode (SendDlgItemMessage (hDlg, IDC_MAPOPT_ORBITPLANE, BM_GETCHECK, 0, 0) == BST_CHECKED ? DISP_ORBITPLANE : DISP_GROUNDTRACK);
		return TRUE;
	case IDC_MAPOPT_HORIZON:
		ToggleDispFlag (DISP_HORIZONLINE);
		return TRUE;
	case IDC_MAPOPT_TLINE:
		ToggleDispFlag (DISP_TERMINATOR_LINE);
		return TRUE;
	case IDC_MAPOPT_TSHADE:
		ToggleDispFlag (DISP_TERMINATOR_SHADE);
		return TRUE;
	case IDC_MAPOPT_GRIDLINES:
		ToggleDispFlag (DISP_GRIDLINE);
		return TRUE;
	case IDC_MAPOPT_COASTLINES:
		ToggleDispFlag (DISP_COASTLINE);
		return TRUE;
	case IDC_MAPOPT_CONTOURS:
		ToggleDispFlag (DISP_CONTOURS);
		return TRUE;
	case IDC_MAPOPT_BASES:
		ToggleDispFlag (DISP_BASE);
		return TRUE;
	case IDC_MAPOPT_VORS:
		ToggleDispFlag (DISP_NAVAID);
		return TRUE;
	case IDC_MAPOPT_LANDMARKS:
		ToggleDispFlag (DISP_CUSTOM1);
		return TRUE;
	case IDC_MAPOPT_SATELLITES:
		ToggleDispFlag (DISP_MOON);
		return TRUE;
	}
	return DialogWin::OnCommand (hDlg, id, code, hControl);
}

// ======================================================================

void DlgMapOpt::ToggleDispFlag (DWORD flag)
{
	dlgmap->ToggleDispFlags (flag);
}

// ======================================================================

void DlgMapOpt::SetOrbitMode (DWORD flag)
{
	DWORD dflag = dlgmap->GetMapWin()->GetDisplayFlags();
	dflag &= ~(DISP_ORBITPLANE|DISP_GROUNDTRACK);
	dlgmap->SetDispFlags (dflag | flag);
}

// ======================================================================

void DlgMapOpt::SetVesselMode (DWORD flag)
{
	DWORD dflag = dlgmap->GetMapWin()->GetDisplayFlags();
	dflag &= ~(DISP_VESSEL | DISP_FOCUSONLY);
	dlgmap->SetDispFlags (dflag | flag);
}

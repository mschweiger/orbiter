// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifdef OAPIFUNC
#undef OAPIFUNC
#endif
#include "DrawAPI.h"

#include "VectorMap.h"
#include "Psys.h"
#include "Mfd.h"
#include "Util.h"

#define OUTLINE_COAST 1
#define OUTLINE_CONTOUR 2
#define OUTLINE_HORIZON 3
#define OUTLINE_ORBITPLANE 4
#define OUTLINE_GROUNDTRACK 5

static DWORD col_navaid = 0x808080;

using namespace std;

extern PlanetarySystem *g_psys;
extern Vessel *g_focusobj;
extern TimeData td;
extern char DBG_MSG[256];

bool VectorMap::bsetup = true;
double VectorMap::cosp[NVTX_CIRCLE] = {0};
double VectorMap::sinp[NVTX_CIRCLE] = {0};

//static COLORREF ObjectCol[3] = {
//	Instrument::draw[0][0].col,
//	Instrument::draw[1][0].col,
//	Instrument::draw[2][0].col
//};
static COLORREF labelcol[6] = {0x00FFFF, 0xFFFF00, 0x4040FF, 0xFF00FF, 0x40FF40, 0xFF8080};

// =======================================================================

VectorMap::VectorMap ()
{
	SetDefaults();
}

// =======================================================================

VectorMap::VectorMap (const Planet *pl)
{
	SetDefaults();
}

// =======================================================================

VectorMap::~VectorMap ()
{
#ifdef ASYNC_DRAWMAP
	WaitThread (true);
	WaitForSingleObject (hCommMutex, INFINITE);
	threaddata.taskid = TASKID_TERMINATE;
	ReleaseMutex (hCommMutex);
	SetEvent (hActivateThread);
	DWORD res = WaitForSingleObject (hRedrawThread, 100); // wait for thread termination
	if (res == WAIT_TIMEOUT)
		TerminateThread (hRedrawThread, 0); // force termination

	CloseHandle (hRedrawThread);
	CloseHandle (hActivateThread);
	CloseHandle (hCommMutex);
#endif

	if (hBmpDraw) DeleteObject (hBmpDraw);
	if (hDCmem) DeleteDC (hDCmem);
	CloseGDIResources();
}

// =======================================================================

void VectorMap::SetDefaults ()
{
	cbody = NULL;
	planet = NULL;
	cw = ch = 0;
	cntx = cnty = 0;
	lngc = latc = 0.0;
	dlng = Pi, dlat = Pi05;
	zoom = 1.0;
	scalefac = 1.0;
	labelsize = 9;
	lngmin = lngc-dlng;
	lngmax = lngc+dlng;
	latmin = latc-dlat;
	latmax = latc+dlat;
	centermode = 0;
	dispflag = DISP_GRIDLINE | DISP_COASTLINE | DISP_CONTOURS | DISP_BASE | DISP_VESSEL | DISP_ORBITFOCUS | DISP_ORBITSEL | DISP_ORBITPLANE;
	findflag = DISP_BASE | DISP_NAVAID | DISP_CUSTOM1 | DISP_VESSEL | DISP_MOON;
	drawdata.focus_disp = drawdata.tgtv_disp = drawdata.tgtb_disp = drawdata.sun_disp = false;
	selection.obj = NULL;
	selection.type = 0;

	if (bsetup) {
		for (int i = 0; i < NVTX_CIRCLE; i++) {
			double alpha = (double)i/(double)NVTX_CIRCLE * Pi2;
			cosp[i] = cos(alpha);
			sinp[i] = sin(alpha);
		}
		bsetup = false;
	}

	hDCmem = NULL;
	hBmpDraw = NULL;
	InitGDIResources();

#ifdef ASYNC_DRAWMAP
	// Initialise the redraw thread
	threaddata.taskid = 0;
	DWORD id;
	hActivateThread = CreateEvent (NULL, FALSE, TRUE, "MapEvent");
	hCommMutex     = CreateMutex (NULL, FALSE, "MapCommMutex");
	hRedrawThread  = CreateThread (NULL, 2048, Redraw_ThreadProc, this, 0, &id);
	SetThreadPriority (hRedrawThread, THREAD_PRIORITY_IDLE);
#endif
}

// =======================================================================

void VectorMap::InitGDIResources ()
{
	int i;

	fontLabel = NULL;
	// for default label size, query screen resolution
	int screenh = GetSystemMetrics(SM_CYSCREEN);
	labelsize = screenh / 100;
	SetLabelSize(labelsize);

	penGridline = CreatePen (PS_SOLID, 1, 0x505050);
	penCoast = CreatePen (PS_SOLID, 1, Instrument::draw[4][0].col);
	penContour = CreatePen (PS_SOLID, 1, 0x0070C0);
	penTerminator = CreatePen (PS_SOLID, 1, 0xC0C0C0);
	int idx[3] = {0,1,3};
	for (i = 0; i < 3; i++) {
		penOrbitFuture[i] = CreatePen (PS_SOLID, 1, Instrument::draw[idx[i]][0].col);
		penOrbitPast[i] = CreatePen (PS_SOLID, 1, Instrument::draw[idx[i]][1].col);
	}
	penFocusHorizon = CreatePen (PS_SOLID, 1, Instrument::draw[0][1].col);
	penTargetHorizon = CreatePen (PS_SOLID, 1, Instrument::draw[1][1].col);
	penNavmkr = CreatePen (PS_SOLID, 1, col_navaid);
	penBase   = CreatePen (PS_SOLID, 1, Instrument::draw[2][0].col);
	penSelection = CreatePen (PS_SOLID, 1, Instrument::draw[1][0].col);
	for (i = 0; i < 3; i++)
		penMarker[i] = CreatePen (PS_SOLID, 3, Instrument::draw[idx[i]][0].col);
	nCustomMkr = 0;
	LOGBRUSH lb = {BS_SOLID, 0x303030, 0};
	brushDay = CreateBrushIndirect (&lb);
}

// =======================================================================

void VectorMap::SetLabelSize(int size)
{
	if (fontLabel) {
		if (size == labelsize) return; // nothing to do
		else DeleteObject(fontLabel);
	}
	labelsize = size;
	fontLabel = CreateFont(-labelsize, 0, 0, 0, 400, 0, 0, 0, 0, 3, 2, 1, 49, "Arial");
}

// =======================================================================

void VectorMap::AllocCustomResources ()
{
	DWORD i;

	if (nCustomMkr) {
		for (i = 0; i < nCustomMkr; i++)
			DeleteObject (penCustomMkr[i]);
		delete []penCustomMkr;
		penCustomMkr = NULL;
		delete []colCustomMkr;
		colCustomMkr = NULL;
	}
	nCustomMkr = mkrset.nset;
	if (nCustomMkr) {
		penCustomMkr = new HPEN[nCustomMkr];
		colCustomMkr = new COLORREF[nCustomMkr];
		for (i = 0; i < nCustomMkr; i++) {
			colCustomMkr[i] = labelcol[mkrset.set[i].list->colour];
			penCustomMkr[i] = CreatePen (PS_SOLID, 1, colCustomMkr[i]);
		}
	}
}

// =======================================================================

void VectorMap::CloseGDIResources ()
{
	DWORD i;
	DeleteObject (fontLabel);
	DeleteObject (penGridline);
	DeleteObject (penCoast);
	DeleteObject (penContour);
	DeleteObject (penTerminator);
	//DeleteObject (penFocusGroundtrackFuture);
	//DeleteObject (penFocusGroundtrackPast);
	DeleteObject (penFocusHorizon);
	//DeleteObject (penTargetGroundtrackFuture);
	//DeleteObject (penTargetGroundtrackPast);
	DeleteObject (penTargetHorizon);
	DeleteObject (penNavmkr);
	DeleteObject (penBase);
	DeleteObject (penSelection);
	DeleteObject (brushDay);
	for (i = 0; i < 3; i++) {
		DeleteObject(penOrbitFuture[i]);
		DeleteObject(penOrbitPast[i]);
	}
	for (i = 0; i < 3; i++)
		DeleteObject (penMarker[i]);
	if (nCustomMkr) {
		for (i = 0; i < nCustomMkr; i++)
			DeleteObject (penCustomMkr[i]);
		delete []penCustomMkr;
		penCustomMkr = NULL;
		delete []colCustomMkr;
		colCustomMkr = NULL;
		nCustomMkr = 0;
	}
}

// =======================================================================

void VectorMap::Update ()
{
#ifdef ASYNC_DRAWMAP
	if (ThreadBusy()) return;
	// Don't update data if drawing in progress
	// Should we rather wait here until the thread is free again?
#endif

	double lng, lat, rad;
	if (!cbody) return;

	if (centermode == 1) {
		const SurfParam *sp = g_focusobj->GetSurfParam();
		if (sp && sp->ref == cbody)
			SetCenter (sp->lng, sp->lat);
	} else if (centermode == 2) {
		if (GetObjPos (selection, lng, lat))
			SetCenter (lng, lat);
	}

	if (drawdata.focus_disp = (true/*planet == g_focusobj->ProxyPlanet()*/)) {
		strncpy (drawdata.focusname, g_focusobj->Name(), 64);
		cbody->GlobalToEquatorial (g_focusobj->GPos(), lng, lat, rad);
		drawdata.focuslng = lng;
		drawdata.focuslat = lat;
		drawdata.focusrad = rad/cbody->Size();
		drawdata.focusel.Set (*g_focusobj->Els());
		//if (focuscenter) SetCenter (drawdata.focuslng, drawdata.focuslat);
	}
	if (drawdata.sun_disp = true) {
		cbody->GlobalToEquatorial (Vector(0,0,0), lng, lat, rad);
		drawdata.sunlng = lng;
		drawdata.sunlat = lat;
	}

	const int nstep = 10;
	int i;

	if (dispflag & DISP_GROUNDTRACK) {
		if (dispflag & DISP_ORBITFOCUS && g_focusobj->ElRef() == cbody) {
			for (i = 0; i < nstep; i++) gt_this.Update();
		}
		if (dispflag & DISP_ORBITSEL && selection.type == DISP_VESSEL) {
			Vessel *v = (Vessel*)selection.obj;
			if ((v != g_focusobj) || ((dispflag & DISP_ORBITFOCUS) == 0)) {
				if (v->ElRef() == cbody)
					for (i = 0; i < nstep; i++) gt_tgt.Update();
			}
		} else if (selection.type == DISP_MOON) {
			for (i = 0; i < nstep; i++) gt_tgt.Update();
		}
	}
}

// =======================================================================

#ifdef UNDEF
void VectorMap::UpdateGroundtrack ()
{
#ifdef ASYNC_DRAWMAP
	if (ThreadBusy()) return;
#endif
	if (drawdata.focus_disp) gt_this.Update();

	if (selection.type == DISP_VESSEL) gt_tgt.Update();
	//if (drawdata.tgtv_disp) gt_tgt.Update();
}
#endif

// =======================================================================

bool VectorMap::SetCBody (const CelestialBody *body)
{
	if (body != cbody) {
		cbody = body;
		planet = (cbody->Type() == OBJTP_PLANET ? (const Planet*)cbody : NULL);
		if (planet) {
			char path[MAX_PATH], relpath[MAX_PATH];
			sprintf (relpath, "%s\\data\\coast.vec", cbody->Name());
			strcpy (path, g_pOrbiter->Cfg()->ConfigPathNoext (relpath));
			coast.Load (path, OUTLINE_COAST);
			sprintf (relpath, "%s\\data\\contour.vec", cbody->Name());
			strcpy (path, g_pOrbiter->Cfg()->ConfigPathNoext (relpath));
			contour.Load (path, OUTLINE_CONTOUR);
			gt_this.Reset (cbody, g_focusobj->Els());
			mkrlist = planet->LabelList (&nmkrlist);
			mkrset.Connect (mkrlist, nmkrlist);
			AllocCustomResources();
		} else {
			coast.Clear();
			contour.Clear();
		}
		UnsetSelection();
		return true;
	}
	return false;
}

// =======================================================================

void VectorMap::SetCanvas (void *device_context, int w, int h)
{
#ifdef ASYNC_DRAWMAP
	WaitThread (true);
	// make sure the drawing thread is not busy before replacing the bitmap
#endif

	cw = w;
	ch = h;
	cntx = w/2;
	cnty = h/2;
	SetZoom (zoom);

	// Create the drawing bitmap
	HDC hDCtgt = GetDC (NULL);
	if (hDCmem) {
		DeleteDC (hDCmem);                             // delete current memory DC
	}
	if (hBmpDraw) DeleteObject (hBmpDraw);             // delete drawing bitmap
	if (hDCtgt) {
		hDCmem = CreateCompatibleDC (hDCtgt);
		hBmpDraw = CreateCompatibleBitmap (hDCtgt, w, h); // create new drawing bitmap
		SetBkMode (hDCmem, TRANSPARENT);
		SelectObject (hDCmem, GetStockObject (NULL_BRUSH));
	} else {
		hDCmem = NULL;
		hBmpDraw = NULL;
	}
	ReleaseDC (NULL, hDCtgt);
}

// =======================================================================

void VectorMap::SetMapArea (double lngcnt, double latcnt, double lngext, double latext)
{
	lngc = lngcnt;
	latc = latcnt;
	dlng = lngext;
	dlat = latext;
}

// =======================================================================

void VectorMap::SetCenter (double lngcnt, double latcnt)
{
	while (lngcnt < -Pi) lngcnt += Pi2;
	while (lngcnt >= Pi) lngcnt -= Pi2;
	lngc = lngcnt;
	latc = latcnt;

	lngmin = lngc-dlng;
	lngmax = lngc+dlng;
	latmin = latc-dlat;
	latmax = latc+dlat;

	mapx_ofs = dlng-lngc;
	mapx_scale = cw/(2.0*dlng);
	mapy_ofs = latc+dlat;
	mapy_scale = ch/(2.0*dlat);
}

// =======================================================================

void VectorMap::SetZoom (double newzoom)
{
	zoom = newzoom;
	double scale = min (cw, ch*2);
	scalefac = zoom*scale/Pi2;
	dlng = cw*Pi/(zoom*scale);
	dlat = ch*Pi/(zoom*scale);

	lngmin = lngc-dlng;
	lngmax = lngc+dlng;
	latmin = latc-dlat;
	latmax = latc+dlat;

	mapx_ofs = dlng-lngc;
	mapx_scale = cw/(2.0*dlng);
	mapy_ofs = latc+dlat;
	mapy_scale = ch/(2.0*dlat);

}

// ======================================================================

bool VectorMap::SetSelection (const OBJTYPE &obj)
{
	if (obj.obj == selection.obj && obj.type == selection.type)
		return false;
	if (obj.type == DISP_VESSEL || obj.type == DISP_MOON) {
		Body *body = (Body*)obj.obj;
		if (!g_psys->isObject (body))
			return false;
	}
	selection = obj;
	if (cbody) {
		if (obj.type == DISP_VESSEL)
			gt_tgt.Reset (cbody, ((Vessel*)obj.obj)->Els());
		else if (obj.type == DISP_MOON)
			gt_tgt.Reset (cbody, ((CelestialBody*)obj.obj)->Els());
	}
	return true;
}

// ======================================================================

void VectorMap::UnsetSelection ()
{
	if (selection.type) {
		selection.type = 0;
		selection.obj = NULL;
	}
}

// ======================================================================

void VectorMap::SetDisplayFlags (DWORD flag)
{
	dispflag = flag;
	CheckSelection();
}

// ======================================================================

void VectorMap::ToggleDisplayFlags (DWORD flag)
{
	dispflag ^= flag;
	CheckSelection();
}

// ======================================================================

void VectorMap::CheckSelection ()
{
	bool clearselection = false;
	switch (selection.type) {
	case DISP_VESSEL:
		if (!(dispflag & DISP_VESSEL) || (dispflag & DISP_FOCUSONLY) && selection.obj != g_focusobj)
			clearselection = true;
		break;
	case DISP_MOON:
		break;
	default:
		if (selection.type && !(dispflag & selection.type))
			clearselection = true;
		break;
	}
	if (clearselection) {
		OBJTYPE nullobj = {0,NULL};
		SetSelection (nullobj);
	}
}

// ======================================================================

bool VectorMap::GetObjPos (const OBJTYPE &obj, double &lng, double &lat)
{
	switch (obj.type) {
	case DISP_VESSEL: {
		const Vessel *v = (const Vessel*)obj.obj;
		const SurfParam *sp = v->GetSurfParam();
		if (sp->ref == cbody) {
			lng = sp->lng, lat = sp->lat;
			return true;
		}
		} break;
	case DISP_MOON: {
		double rad;
		const CelestialBody *moon = (const CelestialBody*)obj.obj;
		cbody->GlobalToEquatorial (moon->GPos(), lng, lat, rad);
		} return true;
	case DISP_BASE: {
		const Base *base = (const Base*)obj.obj;
		base->EquPos (lng, lat);
		} return true;
	case DISP_NAVAID: {
		const Nav_VOR *vor = (const Nav_VOR*)obj.obj;
		vor->GetEquPos (lng, lat);
		} return true;
	case DISP_CUSTOM1: {
		const VPoint *vp = (const VPoint*)obj.obj;
		lng = vp->lng;
		lat = vp->lat;
		} return true;
	}
	return false;
}

// ======================================================================

bool VectorMap::GetMapPos (double lng, double lat, int &x, int &y) const
{
	if (lat < latmin || lat > latmax) return false;
	if      (lng > lngc+Pi) lng -= Pi2;
	else if (lng < lngc-Pi) lng += Pi2;
	if (lng < lngmin || lng > lngmax) return false;
	x = cntx + (int)((lng-lngc)*scalefac);
	y = cnty - (int)((lat-latc)*scalefac);
	return true;
}

// =======================================================================

bool VectorMap::GetMapCrd (int x, int y, double &lng, double &lat) const
{
	lng = (x-cntx)/scalefac+lngc;
	while (lng >= Pi2) lng -= Pi2;
	while (lng < 0.0)  lng += Pi2;
	lat = (cnty-y)/scalefac+latc;
	return (lat >= -Pi05 && lat <= Pi05);
}

// =======================================================================

const VectorMap::OBJTYPE VectorMap::FindObject (int x, int y) const
{
	OBJTYPE obj = {NULL, 0};
	if (!planet) return obj;

	const double hitrad2 = 25.0;
	DWORD i;
	int j, px, py;
	double lng, lat, rad, dx, dy, dst2;
	double dst2min = 1e10;

	if (dispflag & DISP_BASE && findflag & DISP_BASE && g_psys->nBase (planet)) {
		for (i = 0; i < g_psys->nBase (planet); i++) {
			Base *base = g_psys->GetBase (planet, i);
			base->EquPos (lng, lat);
			if (GetMapPos (lng, lat, px, py)) {
				dx = (double)(x-px);
				dy = (double)(y-py);
				dst2 = dx*dx + dy*dy;
				if (dst2 < dst2min) {
					dst2min = dst2;
					obj.obj = base;
					obj.type = DISP_BASE;
				}
			}
		}
	}

	if (dispflag & DISP_NAVAID && findflag & DISP_NAVAID) {
		for (i = 0; i < planet->nNav(); i++) {
			const Nav *nav = planet->NavMgr().GetNav(i);
			if (nav->Type() == TRANSMITTER_VOR) {
				const Nav_VOR *vor = (const Nav_VOR*)nav;
				vor->GetEquPos (lng, lat);
				if (GetMapPos (lng, lat, px, py)) {
					dx = (double)(x-px);
					dy = (double)(y-py);
					dst2 = dx*dx + dy*dy;
					if (dst2 < dst2min) {
						dst2min = dst2;
						obj.obj = nav;
						obj.type = DISP_NAVAID;
					}
				}
			}
		}
	}

	if (dispflag & DISP_CUSTOM1 && findflag & DISP_CUSTOM1) {
		for (i = 0; i < mkrset.nset; i++) {
			if (mkrset.set[i].active) {
				const CustomMkrSpec *set = mkrset.set+i;
				for (j = 0; j < set->nvtx; j++) {
					if (GetMapPos (set->vtx[j].lng, set->vtx[j].lat, px, py)) {
						dx = (double)(x-px);
						dy = (double)(y-py);
						dst2 = dx*dx + dy*dy;
						if (dst2 < dst2min) {
							dst2min = dst2;
							obj.obj = set->vtx+j;
							obj.type = DISP_CUSTOM1;
						}
					}
				}
			}
		}
	}

	if (dispflag & DISP_VESSEL && findflag & DISP_VESSEL) {
		for (i = 0; i < g_psys->nVessel(); i++) {
			Vessel *v = g_psys->GetVessel(i);
			if (dispflag & DISP_FOCUSONLY && v != g_focusobj)
				continue;
			const SurfParam *sp = v->GetSurfParam();
			if (sp->ref == planet) {
				if (GetMapPos (sp->lng, sp->lat, px, py)) {
					dx = (double)(x-px);
					dy = (double)(y-py);
					dst2 = dx*dx + dy*dy;
					if (dst2 < dst2min) {
						dst2min = dst2;
						obj.obj = v;
						obj.type = DISP_VESSEL;
					}
				}
			}
		}
	}

	if (dispflag & DISP_MOON && findflag & DISP_MOON) {
		for (i = 0; i < cbody->nSecondary(); i++) {
			const CelestialBody *moon = cbody->Secondary (i);
			cbody->GlobalToEquatorial (moon->GPos(), lng, lat, rad);
			if (GetMapPos (lng, lat, px, py)) {
				dx = (double)(x-px);
				dy = (double)(y-py);
				dst2 = dx*dx + dy*dy;
				if (dst2 < dst2min) {
					dst2min = dst2;
					obj.obj = moon;
					obj.type = DISP_MOON;
				}
			}
		}
	}

	return obj;
}

// =======================================================================

HBITMAP VectorMap::GetMap ()
{
#ifdef ASYNC_DRAWMAP
	//WaitThread(); // wait for thread to finish drawing
#endif
	return hBmpDraw;
}

// =======================================================================

HDC VectorMap::GetDeviceContext ()
{
#ifdef ASYNC_DRAWMAP
	WaitThread(); // wait for thread to finish drawing
#endif
	return hDCmem;
}

// =======================================================================

void VectorMap::DrawMap ()
{
#ifdef ASYNC_DRAWMAP
	if (ThreadBusy()) return;
#endif

	DrawMap_engine();
}

// =======================================================================

void VectorMap::DrawMap_engine ()
{
	// called either directly (by DrawMap) or from the drawing thread

	tic();

	// clear the bitmap
	HBITMAP pBmp = (HBITMAP)SelectObject (hDCmem, hBmpDraw);
	BitBlt (hDCmem, 0, 0, cw, ch, NULL, 0, 0, BLACKNESS);
	HFONT pFont = (HFONT)SelectObject (hDCmem, fontLabel);

	// terminator line
	if (planet && dispflag & DISP_TERMINATOR) {
		switch (dispflag & DISP_TERMINATOR) {
		case DISP_TERMINATOR_LINE:
			DrawTerminatorLine (drawdata.sunlng, drawdata.sunlat);
			break;
		case DISP_TERMINATOR_SHADE:
			DrawSunnySide (drawdata.sunlng, drawdata.sunlat, false);
			break;
		case DISP_TERMINATOR_BOTH:
			DrawSunnySide (drawdata.sunlng, drawdata.sunlat, true);
			break;
		}
	}

	// grid lines
	if (dispflag & DISP_GRIDLINE)
		DrawGridlines ();

	if (planet) {

		// coastlines
		if (dispflag & DISP_COASTLINE)
			DrawPolySet (&coast);

		// contour lines
		if (dispflag & DISP_CONTOURS)
			DrawPolySet (&contour);

		// navaids
		if (dispflag & DISP_NAVAID)
			DrawNavaids ();

		// custom marker sets
		if (dispflag & DISP_CUSTOM1) {
			for (DWORD i = 0; i < mkrset.nset; i++) {
				if (mkrset.set[i].active)
					DrawCustomMarkerSet (i);
			}
		}

		// base markers
		if (dispflag & DISP_BASE)
			DrawBases ();

		// target base
		if (drawdata.tgtb_disp) {
			DrawMarker (drawdata.tgtblng, drawdata.tgtblat, drawdata.basename, 2);
		}

		// natural satellites
		if (dispflag & DISP_MOON || selection.type == DISP_MOON)
			DrawMoons ();

		// vessels
		if (dispflag & DISP_VESSEL)
			DrawVessels ();
	}

	// target orbiter
	if (drawdata.tgtv_disp) {
		if (dispflag & DISP_HORIZONLINE)
			DrawHorizon (drawdata.tgtvlng, drawdata.tgtvlat, drawdata.tgtvrad, false);
		if (dispflag & DISP_GROUNDTRACK)
			DrawGroundtrack (gt_tgt, 1);
		else if (dispflag & DISP_ORBITPLANE)
			DrawOrbitPlane (&drawdata.tgtvel, 1);
		DrawMarker (drawdata.tgtvlng, drawdata.tgtvlat, drawdata.tgtname, 1);
	}

	// selection marker
	if (selection.type)
		DrawSelectionMarker (selection);

	SelectObject (hDCmem, pFont);
	SelectObject (hDCmem, pBmp);
}

// =======================================================================

void VectorMap::DrawGridlines ()
{
	HPEN ppen = (HPEN)SelectObject (hDCmem, penGridline);
	const double step = 30.0/DEG;
	const double eps = 1e-8;
	double lat0 = max(latmin,-Pi05);
	double lat1 = min(latmax, Pi05);
	double lng0 = max(lngmin, lngc-Pi);
	double lng1 = min(lngmax, lngc+Pi);
	int x, y;
	int x0 = mapx(lng0), x1 = mapx(lng1);
	int y0 = mapy(lat0), y1 = mapy(lat1);
	double lat = ceil(lat0/step) * step;
	while (lat < lat1+eps) {
		y = mapy(lat);
		MoveToEx (hDCmem, x0, y, NULL);
		LineTo (hDCmem, x1, y);
		lat += step;
	}
	double lng = ceil(lng0/step) * step;
	while (lng < lng1) {
		x = mapx(lng);
		MoveToEx (hDCmem, x, y0, NULL);
		LineTo (hDCmem, x, y1);
		lng += step;
	}
}

// =======================================================================

void VectorMap::DrawPolySet (const PolyLineSet *pls)
{
	int j, n;
	int mapw = (int)(cw*PI/dlng);
	VPoint *v0;

	HGDIOBJ ppen = NULL;

	switch (pls->type) {
	case OUTLINE_COAST:
		ppen = SelectObject (hDCmem, penCoast);
		break;
	case OUTLINE_CONTOUR:
		ppen = SelectObject (hDCmem, penContour);
		break;
	}

	for (j = 0; j < pls->npoly; j++) {
		v0 = pls->vtx + pls->poly[j].vofs;
		n = (pls->poly[j].close ? pls->poly[j].nvtx : pls->poly[j].nvtx-1);
		DrawPolyline (0, v0, n);
	}
	if (ppen) SelectObject (hDCmem, ppen);
}
	
// =======================================================================

void VectorMap::DrawPolyline (int type, VPoint *vp, int n, bool close)
{
	int i, x0, x1, y0, y1;
	int mapw = (int)(cw*PI/dlng);
	VPoint *va, *vb;

	for (i = 0; i < n; i++) {
		va = vp+i;
		if (i == n-1) {
			if (close) vb = vp;
			else break;
		} else 
			vb = va+1;
		x0 = mapx (va->lng);
		x1 = mapx (vb->lng);
		if ((x0 < 0 && x1 < 0) || (x0 >= cw && x1 >= cw)) continue;
		if (abs (x0-x1) > mapw/2) { // wrapping condition
			if (x0 >= 0 && x0 < cw)
				x1 = (x1 < x0 ? x1+mapw : x1-mapw);
			else if (x1 >= 0 && x1 < cw)
				x0 = (x0 < x1 ? x0+mapw : x0-mapw);
			else
				continue;
		}
		y0 = mapy (va->lat);
		y1 = mapy (vb->lat);
		if ((y0 < 0 && y1 < 0) || (y0 >= ch && y1 >= ch)) continue;
		MoveToEx (hDCmem, x0, y0, NULL);
		LineTo (hDCmem, x1, y1);
	}
}

// =======================================================================

void VectorMap::DrawGroundtrackLine (int type, VPointGT *vp, int n, int n0, int n1)
{
	int i, x0, x1, y0, y1;
	int mapw = (int)(cw*PI/dlng);
	VPointGT *va, *vb;
	bool replicate;

	if (n1 < n0) n1 += n;

	for (i = n0; i < n1; i++) {
		replicate = false;
		va = vp+(i%n);
		vb = vp+((i+1)%n);
		if (va->t >= vb->t) continue;
		x0 = mapx (va->lng);
		x1 = mapx (vb->lng);
		if ((x0 < 0 && x1 < 0) || (x0 >= cw && x1 >= cw)) continue;
		if (va->rad < 1.0 && vb->rad < 1.0) continue;
		if (abs (x0-x1) > mapw/2) { // wrapping condition
			if (x0 >= 0 && x0 < cw) {
				if (x1 >= 0 && x1 < cw) replicate = true;
				x1 = (x1 < x0 ? x1+mapw : x1-mapw);
			} else if (x1 >= 0 && x1 < cw)
				x0 = (x0 < x1 ? x0+mapw : x0-mapw);
			else
				continue;
		}
		y0 = mapy (va->lat);
		y1 = mapy (vb->lat);
		if ((y0 < 0 && y1 < 0) || (y0 >= ch && y1 >= ch)) continue;
		if (va->rad < 1.0) {
			double scl = (1.0-va->rad)/(vb->rad-va->rad);
			x0 += (int)((x1-x0)*scl);
			y0 += (int)((y1-y0)*scl);
			Rectangle (hDCmem, x0-2, y0-2, x0+3, y0+3);
			if (replicate)
				Rectangle (hDCmem, x0-2-mapw, y0-2, x0+3-mapw, y0+3);
		} else if (vb->rad < 1.0) {
			double scl = (1.0-vb->rad)/(vb->rad-va->rad);
			x1 += (int)((x1-x0)*scl);
			y1 += (int)((y1-y0)*scl);
			Rectangle (hDCmem, x1-2, y1-2, x1+3, y1+3);
			if (replicate)
				Rectangle (hDCmem, x1-2-mapw, y1-2, x1+3-mapw, y1+3);
		}
		if (replicate && x0 != x1) {
			int xm = (cw-mapw)/2;
			if (x0 > cntx) xm += mapw;
			int ym = y0 + ((xm-x0)*(y1-y0))/(x1-x0);
			MoveToEx (hDCmem, x0, y0, NULL);
			LineTo (hDCmem, xm, ym);
			int dx = (x0 > cntx ? -mapw:mapw);
			MoveToEx (hDCmem, xm+dx, ym, NULL);
			LineTo (hDCmem, x1+dx, y1);
		} else {
			MoveToEx (hDCmem, x0, y0, NULL);
			LineTo (hDCmem, x1, y1);
		}
	}
}

// =======================================================================

void VectorMap::DrawNavaids ()
{
	SetTextColor (hDCmem, col_navaid);
	HPEN ppen = (HPEN)SelectObject (hDCmem, penNavmkr);
	SelectObject (hDCmem, GetStockObject (NULL_BRUSH));

	if (planet && planet->nNav()) {
		static char cbuf[32];
		double lng, lat;
		int x, y;
		bool drawdot = (mapx_scale < 400);
		bool drawlabel = (mapx_scale > 1400);
		for (DWORD n = 0; n < planet->nNav(); n++) {
			const Nav *nav = planet->NavMgr().GetNav(n);
			switch (nav->Type()) {
			case TRANSMITTER_VOR: {
				const Nav_VOR *vor = (const Nav_VOR*)nav;
				vor->GetEquPos (lng, lat);
				if (GetMapPos (lng, lat, x, y)) {
					if (drawdot) {
						SetPixel (hDCmem, x, y, col_navaid);
					} else {
						Ellipse (hDCmem, x-2, y-2, x+3, y+3);
						if (drawlabel) {
							sprintf (cbuf, "%s %0.2f", vor->GetId(), vor->GetFreq());
							TextOut (hDCmem, x+3, y, cbuf, strlen(cbuf));
						}
					}
				}
				} break;
			}
		}
	}

	SelectObject (hDCmem, ppen);
}

// =======================================================================

void VectorMap::DrawVesselOrbit (Vessel *v)
{
	bool isfocus = (v == g_focusobj);
	if (dispflag & DISP_HORIZONLINE) {
		const SurfParam *sp = v->GetSurfParam();
		if (sp && sp->ref == cbody) {
			DrawHorizon (sp->lng, sp->lat, sp->rad/cbody->Size(), isfocus);
		}
	}
	if (v->ElRef() == cbody) {
		if (dispflag & DISP_GROUNDTRACK)
			DrawGroundtrack (isfocus ? gt_this : gt_tgt, isfocus ? 0:1);
		if (dispflag & DISP_ORBITPLANE)
			DrawOrbitPlane (v->Els(), isfocus ? 0:1);
	}
}

// =======================================================================

void VectorMap::DrawVessels ()
{
	Vessel *v;
	bool focus_drawn = false;

	if (dispflag & DISP_ORBITSEL && selection.type == DISP_VESSEL) {
		v = (Vessel*)selection.obj;
		if (v == g_focusobj || !(dispflag & DISP_FOCUSONLY)) {
			DrawVesselOrbit (v);
			focus_drawn = (v == g_focusobj);
		}
	}
	if (dispflag & DISP_ORBITFOCUS && !focus_drawn)
		DrawVesselOrbit (g_focusobj);

	for (DWORD i = 0; i < g_psys->nVessel(); i++) {
		v = g_psys->GetVessel(i);
		if (dispflag & DISP_FOCUSONLY && v != g_focusobj)
			continue;
		const SurfParam *sp = v->GetSurfParam();
		if (sp && sp->ref == planet) {
			DrawMarker (sp->lng, sp->lat, v->Name(), v == g_focusobj ? 0:1);
		}
	}
}

// =======================================================================

void VectorMap::DrawMoons ()
{
	const CelestialBody *moon;
	double lng, lat, rad;

	if (dispflag & DISP_ORBITSEL && selection.type == DISP_MOON) {
		moon = (const CelestialBody*)selection.obj;
		if (moon->ElRef() == cbody) {
			if (dispflag & DISP_GROUNDTRACK) {
				DrawGroundtrack (gt_tgt, 2);
			} if (dispflag & DISP_ORBITPLANE) {
				DrawOrbitPlane (moon->Els(), 2);
			}
		}
	}

	if (dispflag & DISP_MOON)
		for (DWORD i = 0; i < cbody->nSecondary(); i++) {
			moon = cbody->Secondary (i);
			cbody->GlobalToEquatorial (moon->GPos(), lng, lat, rad);
			DrawMarker (lng, lat, moon->Name(), 2);
		}
}

// =======================================================================

void VectorMap::DrawBases ()
{
	bool drawlabel = (mapx_scale > 700);
	if (drawlabel) SetTextColor (hDCmem, Instrument::draw[2][0].col);
	HPEN ppen = (HPEN)SelectObject (hDCmem, penBase);

	if (planet && g_psys->nBase (planet)) {
		int x, y;
		double lng, lat;
		for (DWORD i = 0; i < g_psys->nBase (planet); i++) {
			Base *base = g_psys->GetBase (planet, i);
			base->EquPos (lng, lat);
			if (GetMapPos (lng, lat, x, y)) {
				Rectangle (hDCmem, x-3, y-3, x+4, y+4);
				if (drawlabel)
					TextOut (hDCmem, x+3, y, base->Name(), strlen(base->Name()));
			}
		}
	}

	SelectObject (hDCmem, ppen);
}

// =======================================================================

void VectorMap::DrawCustomMarkerSet (int idx)
{
	HPEN ppen = NULL;
	int i, x, y;
	const char *label;

	CustomMkrSpec *set = mkrset.set+idx;
	if (!set->nvtx) set->Convert();

	bool drawdot = (mapx_scale < 400);
	bool drawlabel = (mapx_scale > 1400);

	if (drawlabel) SetTextColor (hDCmem, colCustomMkr[idx]);
	if (!drawdot)  ppen = (HPEN)SelectObject (hDCmem, penCustomMkr[idx]);

	for (i = 0; i < set->nvtx; i++) {
		if (GetMapPos (set->vtx[i].lng, set->vtx[i].lat, x, y)) {
			if (drawdot) {
				SetPixel (hDCmem, x, y, colCustomMkr[idx]);
			} else {
				Ellipse (hDCmem, x-2, y-2, x+3, y+3);
				if (drawlabel && set->list->marker[i].label[0].size()) {
					WCHAR wlabel[256];
					MultiByteToWideChar(CP_UTF8, 0, set->list->marker[i].label[0].c_str(), -1, wlabel, 256);
					TextOutW (hDCmem, x+3, y, wlabel, wcslen(wlabel));
				}
			}
		}
	}
	if (ppen) SelectObject (hDCmem, ppen);
}

// =======================================================================

void VectorMap::DrawMarker (double lng, double lat, const char *name, int which)
{
	int x, y;
	if (!GetMapPos (lng, lat, x, y))
		return; // position not on map

	HPEN ppen = (HPEN)SelectObject (hDCmem, penMarker[which]);
	MoveToEx (hDCmem, x-10, y, NULL);
	LineTo (hDCmem, x+11, y);
	MoveToEx (hDCmem, x, y-10, NULL);
	LineTo (hDCmem, x, y+11);
	SelectObject (hDCmem, ppen);
	SetTextColor (hDCmem, Instrument::draw[which][0].col);
	TextOut (hDCmem, x+3, which==2 ? y:y-labelsize-3, name, min((size_t)64,strlen(name)));
}

// =======================================================================

void VectorMap::DrawSelectionMarker (const OBJTYPE obj)
{
	double lng, lat;
	int x, y;
	if (GetObjPos (obj, lng, lat))
		if (GetMapPos (lng, lat, x, y)) {
			HPEN ppen = (HPEN)SelectObject (hDCmem, penSelection);
			Ellipse (hDCmem, x-5, y-5, x+6, y+6);
			Ellipse (hDCmem, x-8, y-8, x+9, y+9);
			SelectObject (hDCmem, ppen);
		}
}

// =======================================================================

void VectorMap::DrawTerminatorLine (double sunlng, double sunlat)
{
	HPEN ppen = (HPEN)SelectObject (hDCmem, penTerminator);
	VPoint *p = GreatCircle (sunlng, sunlat);
	DrawPolyline (0, p, NVTX_CIRCLE);
	SelectObject (hDCmem, ppen);
}

// =======================================================================

void VectorMap::DrawSunnySide (double sunlng, double sunlat, bool terminator)
{
	int i, cut, ybase, idx0, idx1;
	int mapw = (int)(cw*PI/dlng);
	if (sunlat >= 0) ybase = max (0, mapy(Pi05));
	else             ybase = min (ch, mapy(-Pi05));
	VPoint *p = GreatCircle (sunlng, sunlat);
	POINT pt[NVTX_CIRCLE], ptt[NVTX_CIRCLE+4];
	for (i = 0; i < NVTX_CIRCLE; i++) {
		pt[i].x = mapx(p[i].lng);
		pt[i].y = mapy(p[i].lat);
	}
	for (cut = 1; cut <= NVTX_CIRCLE; cut++) {
		if (abs(pt[cut-1].x-pt[cut % NVTX_CIRCLE].x) > mapw/2) break;
	}
	bool prograde = (pt[(cut-1) % NVTX_CIRCLE].x > pt[cut % NVTX_CIRCLE].x);
	if (prograde) {
		for (i = 0; i < NVTX_CIRCLE; i++)
			ptt[2+i] = pt[(cut+i) % NVTX_CIRCLE];
	} else {
		for (i = 0; i < NVTX_CIRCLE; i++)
			ptt[1+NVTX_CIRCLE-i] = pt[(cut+i) % NVTX_CIRCLE];
	}
	if (ptt[2].x > 0) {
		ptt[1].x = ptt[NVTX_CIRCLE+1].x - mapw;
		ptt[1].y = ptt[NVTX_CIRCLE+1].y;
		idx0 = 1;
	} else {
		for (idx0 = 2; idx0 < NVTX_CIRCLE+1; idx0++) {
			if (ptt[idx0+1].x > 0) break;
		}
	}
	if (ptt[NVTX_CIRCLE+1].x < cw) {
		ptt[NVTX_CIRCLE+2].x = ptt[2].x + mapw;
		ptt[NVTX_CIRCLE+2].y = ptt[2].y;
		idx1 = NVTX_CIRCLE+2;
	} else {
		for (idx1 = NVTX_CIRCLE+1; idx1 > 2; idx1--) {
			if (ptt[idx1-1].x < cw) break;
		}
	}
	if (idx1 <= idx0) return;
	ptt[idx0-1].x = ptt[idx0].x;
	ptt[idx0-1].y = ybase;
	ptt[idx1+1].x = ptt[idx1].x;
	ptt[idx1+1].y = ybase;

	HPEN ppen = (HPEN)SelectObject (hDCmem, terminator ? penTerminator : GetStockObject (NULL_PEN));
	SelectObject (hDCmem, brushDay);
	Polygon (hDCmem, ptt+(idx0-1), idx1-idx0+3);
	SelectObject (hDCmem, ppen);
	SelectObject (hDCmem, GetStockObject (NULL_BRUSH));
}

// =======================================================================

void VectorMap::DrawOrbitPlane (const Elements *el, int which)
{
	HPEN ppen = (HPEN)SelectObject (hDCmem, penOrbitFuture[which]);
	static VPoint p[NVTX_CIRCLE];
	CalcOrbitProj (el, cbody, p);
	DrawPolyline (OUTLINE_ORBITPLANE, p, NVTX_CIRCLE);
	SelectObject (hDCmem, ppen);
}

// =======================================================================

void VectorMap::DrawGroundtrack (Groundtrack &gt, int which)
{
	DrawGroundtrack_past (gt, which);
	DrawGroundtrack_future (gt, which);
}

void VectorMap::DrawGroundtrack_past (Groundtrack &gt, int which)
{
	HPEN ppen = (HPEN)SelectObject (hDCmem, penOrbitPast[which]);
#ifdef UNDEF
	if (gt.vfirst <= gt.vcurr) {
		DrawGroundtrackLine (OUTLINE_GROUNDTRACK, gt.vtx+gt.vfirst, gt.vcurr-gt.vfirst+1);
	} else {
		DrawGroundtrackLine (OUTLINE_GROUNDTRACK, gt.vtx+gt.vfirst, gt.nvtx-gt.vfirst);
		DrawGroundtrackLine (OUTLINE_GROUNDTRACK, gt.vtx, gt.vcurr+1);
		MoveToEx (hDCmem, mapx(gt.vtx[gt.nvtx-1].lng), mapy(gt.vtx[gt.nvtx-1].lat), NULL);
		LineTo (hDCmem, mapx(gt.vtx[0].lng), mapy(gt.vtx[0].lat));
	}
#endif
	DrawGroundtrackLine (OUTLINE_GROUNDTRACK, gt.vtx, gt.nvtx, gt.vfirst, gt.vcurr);
	SelectObject (hDCmem, ppen);
}

void VectorMap::DrawGroundtrack_future (Groundtrack &gt, int which)
{
	HPEN ppen = (HPEN)SelectObject (hDCmem, penOrbitFuture[which]);
#ifdef UNDEF
	if (gt.vcurr <= gt.vlast) {
		DrawGroundtrackLine (OUTLINE_GROUNDTRACK, gt.vtx+gt.vcurr, gt.vlast-gt.vcurr+1);
	} else {
		DrawGroundtrackLine (OUTLINE_GROUNDTRACK, gt.vtx+gt.vcurr, gt.nvtx-gt.vcurr);
		DrawGroundtrackLine (OUTLINE_GROUNDTRACK, gt.vtx, gt.vlast+1);
		MoveToEx (hDCmem, mapx(gt.vtx[gt.nvtx-1].lng), mapy(gt.vtx[gt.nvtx-1].lat), NULL);
		LineTo (hDCmem, mapx(gt.vtx[0].lng), mapy(gt.vtx[0].lat));
	}
#endif
	DrawGroundtrackLine (OUTLINE_GROUNDTRACK, gt.vtx, gt.nvtx, gt.vcurr, gt.vlast);
	SelectObject (hDCmem, ppen);
}

// =======================================================================

void VectorMap::DrawHorizon (double lng, double lat, double rad, bool focus)
{
	HPEN ppen = (HPEN)SelectObject (hDCmem, focus ? penFocusHorizon:penTargetHorizon);
	double dst = 1.0/rad;
	VPoint *vp = SmallCircle (lng, lat, dst);
	DrawPolyline (OUTLINE_HORIZON, vp, NVTX_CIRCLE);
	SelectObject (hDCmem, ppen);
}

// =======================================================================

void VectorMap::CalcOrbitProj (const Elements *el, const CelestialBody *body, VPoint *p)
{
	int i;
	double obl = body->Obliquity();
	double rot = body->Rotation();
	double sino = sin(obl), coso = cos(obl);
	double eqlng = body->EqLng();
	double sine = sin(eqlng), cose= cos(eqlng);

	Matrix R (1,  0,        0,
		      0,  el->cosi, el->sini,
			  0, -el->sini, el->cosi);
	R.premul (Matrix (el->cost, 0, -el->sint,
		              0,        1,  0,
                      el->sint, 0,  el->cost));
	R.tpremul (body->GRot());

	Vector r, rt;

	for (i = 0; i < NVTX_CIRCLE; i++) {
		r.x = cosp[i];
		r.z = sinp[i];
		rt.Set (mul (R, r));
		p[i].lng = atan2 (rt.z, rt.x);
		p[i].lat = Pi05-acos(rt.y);
	}
}

// =======================================================================

VPoint *VectorMap::GreatCircle (double lng, double lat)
{
	const int nvtx = NVTX_CIRCLE;
	int i;
	double clng = cos(lng), slng = sin(lng);
	double clat = cos(Pi05-lat), slat = sin(Pi05-lat);
	Matrix R(clat,slat,0,  -slat,clat,0,  0,0,1);
	Matrix R2(clng,0,-slng,  0,1,0,  slng,0,clng);
	R.premul(R2);
	Vector pt, ptt;
	static VPoint vp[nvtx];

	for (i = 0; i < nvtx; i++) {
		pt.x = cosp[i];
		pt.z = sinp[i];
		pt.y = 0.0;
		ptt = mul(R,pt);
		vp[i].lat = Pi05-acos(ptt.y);
		vp[i].lng = atan2(ptt.z, ptt.x);
	}
	return vp;
}

// =======================================================================

VPoint *VectorMap::SmallCircle (double lng, double lat, double dst)
{
	const int nvtx = NVTX_CIRCLE;
	int i;
	double clng = cos(lng), slng = sin(lng);
	double clat = cos(Pi05-lat), slat = sin(Pi05-lat);
	double rad = sqrt(1-dst*dst);
	Matrix R(clat,slat,0,  -slat,clat,0,  0,0,1);
	Matrix R2(clng,0,-slng,  0,1,0,  slng,0,clng);
	R.premul(R2);
	Vector pt, ptt;
	static VPoint vp[nvtx];

	for (i = 0; i < nvtx; i++) {
		pt.x = cosp[i]*rad;
		pt.z = sinp[i]*rad;
		pt.y = dst;
		ptt = mul(R,pt);
		vp[i].lat = Pi05-acos(ptt.y);
		vp[i].lng = atan2(ptt.z, ptt.x);
	}
	return vp;
}


// =======================================================================
// Thread interface
// =======================================================================
#ifdef ASYNC_DRAWMAP
// =======================================================================

void VectorMap::WaitThread (bool abortOp)
{
	if (!ThreadBusy()) return;
	if (abortOp) {
		WaitForSingleObject (hCommMutex, INFINITE);
		threaddata.taskid = TASKID_ABORT;
		ReleaseMutex (hCommMutex);
	}
	while (ThreadBusy()) {
		Sleep(10);
	}
}

// =======================================================================
// Sends a request for a redraw to the draw thread and returns immediately

bool VectorMap::AsyncDrawMap ()
{
	if (ThreadBusy()) return false; // redraw is already in progress
	DWORD res = WaitForSingleObject (hCommMutex, 10);
	if (res == WAIT_TIMEOUT) return false; // could not get mutex in time
	threaddata.taskid = TASKID_DRAW;
	ReleaseMutex (hCommMutex);
	SetEvent (hActivateThread);
	return true;
}

// =======================================================================

void VectorMap::thEngine ()
{
	const DWORD idle = 100;
	DWORD flag;
	bool keep_going = true;
	while (keep_going) {
		WaitForSingleObject (hActivateThread, INFINITE); // wait for task
		WaitForSingleObject (hCommMutex, INFINITE); // secure comm data
		flag = threaddata.taskid;
		ReleaseMutex (hCommMutex);
		switch (flag) {
		case TASKID_TERMINATE:
			keep_going = false;
			break;
		case TASKID_DRAW:
			DrawMap_engine ();
			break;
		}
		WaitForSingleObject (hCommMutex, INFINITE);
		threaddata.taskid = 0; // signal task complete
		ReleaseMutex (hCommMutex);
	}
}

// =======================================================================

DWORD WINAPI VectorMap::Redraw_ThreadProc (void *data)
{
	VectorMap *map = (VectorMap*)data;
	map->thEngine();
	return 0;
}
#endif // ASYNC_DRAWMAP


// =======================================================================
// =======================================================================

PolyLineSet::PolyLineSet ()
{
	vtx = NULL;
	nvtx = 0;
	poly = NULL;
	npoly = 0;
}

PolyLineSet::~PolyLineSet ()
{
	Clear();
}

void PolyLineSet::Clear()
{
	if (nvtx) {
		delete []vtx;
		vtx = NULL;
		nvtx = 0;
	}
	if (npoly) {
		delete []poly;
		poly = NULL;
		npoly = 0;
	}
}

int PolyLineSet::Load (const char *path, int type_id)
{
	char cbuf[256];
	int npt, ofs, i, j;
	double lng, lat;

	Clear ();

	ifstream ifs(path);
	if (!ifs) return 0;

	ifs.getline (cbuf, 256);
	sscanf (cbuf, "%d", &nvtx);
	ifs.getline (cbuf, 256);
	sscanf (cbuf, "%d", &npoly);

	type = type_id;
	vtx = new VPoint[nvtx];
	poly = new PolyLineSpec[npoly];

	for (i = ofs = 0; i < npoly; i++) {
		ifs.getline (cbuf, 256);
		sscanf (cbuf, "%d", &npt);
		poly[i].nvtx = npt;
		poly[i].vofs = ofs;
		poly[i].close = false; // for now
		for (j = 0; j < npt; j++) {
			ifs.getline (cbuf, 256);
			sscanf (cbuf, "%lf%lf", &lng, &lat);
			vtx[ofs].lng = lng*RAD;
			vtx[ofs].lat = lat*RAD;
			ofs++;
		}
	}
	return 0;
}

// =======================================================================
// =======================================================================

CustomMkrSpec::CustomMkrSpec()
{
	list = NULL;
	active = false;
	vtx = NULL;
	nvtx = 0;
}

CustomMkrSpec::~CustomMkrSpec()
{
	if (nvtx) {
		delete []vtx;
		vtx = NULL;
		nvtx = 0;
	}
}

void CustomMkrSpec::Connect (oapi::GraphicsClient::LABELLIST *ll)
{
	list = ll;
}

void CustomMkrSpec::Convert ()
{
	if (nvtx) return;
	if (!list || !list->marker.size()) return;
	nvtx = list->marker.size();
	vtx = new VPoint[nvtx];
	for (int i = 0; i < nvtx; i++) {
		VECTOR3 &p = list->marker[i].pos;
		vtx[i].lng = atan2 (p.z, p.x);
		vtx[i].lat = asin  (p.y / length(p));
	}
}

// =======================================================================
// =======================================================================

CustomMkrSet::CustomMkrSet()
{
	set = NULL;
	nset = 0;
}

CustomMkrSet::~CustomMkrSet()
{
	Clear();
}

void CustomMkrSet::Clear()
{
	if (nset) delete []set;
	set = NULL;
	nset = 0;
}

void CustomMkrSet::Connect (oapi::GraphicsClient::LABELLIST *ll, int nlist)
{
	int i, j;
	Clear();
	for (i = 0, nset = 0; i < nlist; i++)
		if (ll[i].active) nset++;
	if (!nset) return;
	set = new CustomMkrSpec[nset];
	for (i = j = 0; i < nlist; i++)
		if (ll[i].active) set[j++].Connect (ll+i);
}

// =======================================================================
// =======================================================================

const double Groundtrack::tgtstep = 10.0*RAD;
const double Groundtrack::tstep_max = 5*60;

Groundtrack::Groundtrack()
{
	nvtx = 128;
	vtx = new VPointGT[nvtx];
	vfirst = vlast = vcurr = 0;
}

Groundtrack::~Groundtrack()
{
	if (nvtx) {
		delete []vtx;
		vtx = NULL;
	}
}

void Groundtrack::Reset (const CelestialBody *body, const Elements *_el)
{
	int i;
	const double tstep = 120; // arbitrary first interval
	cbody = body;
	if (!cbody) return;
	el = _el;
	prad = cbody->Size();
	
	// initialise the time points
	vfirst = vupdt = 0;
	vcurr = 1;
	vlast = 2;

	vtx[vfirst].t = td.SimT0;
	for (i = 1; i < nvtx; i++) {
		vtx[i].t = td.SimT0 + (i-1)*tstep;
		vtx[i].dt = vtx[i].t-vtx[i-1].t;
		vtx[i].rad = 0.5;
	}

	// initialise vertex list with current point and its neighbours
	for (i = 0; i < 3; i++) CalcPoint (vtx[i], &omega_updt);
}

void Groundtrack::CalcPoint (VPointGT &p, double *angvel)
{
	double r, ta, lng, lat, rad;
	Vector pos, loc;
	el->RelPos (r, ta, p.t);
	el->Pol2Crt (r, ta, pos);
	loc.Set (tmul (cbody->GRot(), pos));
	cbody->LocalToEquatorial (loc, lng, lat, rad);
	p.lng = normangle(lng - Pi2*(p.t-td.SimT0)/cbody->RotT());
	p.lat = lat;
	p.rad = r/prad;

	if (angvel)
		*angvel = sqrt (el->Mu()*(2.0/r - 1.0/el->a))/r - Pi2/cbody->RotT();
}

double Groundtrack::VtxDst (const VPointGT &vp1, const VPointGT &vp2)
{
	double dlat = fabs(vp1.lat - vp2.lat);
	double dlng = fabs(vp1.lng - vp2.lng);
	if (dlng > Pi) dlng = Pi2-dlng;
	return std::hypot (dlng, dlat);
}

void Groundtrack::Update ()
{
	const double dtmax = 3600.0;

	if (!cbody) return;
	if (td.SimT1 < vtx[vcurr].t) { // simulation has jumped back in time!
		Reset (cbody, el);
		return;
	}

	int v0 = vfirst, v1 = vlast, nv, vp;
	if (v0 > vcurr) v0 -= nvtx; // unwrap
	if (v1 < vcurr) v1 += nvtx; // unwrap
	nv = v1-v0+1;  // number of active vertices

	vp = (vcurr ? vcurr-1 : nvtx-1);
	vtx[vcurr].t =  td.SimT1;
	vtx[vcurr].dt = vtx[vcurr].t - vtx[vp].t;
	CalcPoint (vtx[vcurr], &omega_curr);
	if (VtxDst (vtx[vcurr], vtx[vp]) > tgtstep || vtx[vcurr].t >= vtx[(vcurr+1)%nvtx].t) {
		// drop this point and advance to the next one
		vp = vcurr;
		vcurr = (vcurr+1) % nvtx;
		vtx[vcurr] = vtx[vp];
		vtx[vcurr].dt = 0.0;
		if (v1-vcurr < nvtx/2 && nv == nvtx) { // ring is full - release first vertex
			vfirst = (vfirst+1) % nvtx;
			v0++;
			nv--;
		}
	}

	// ring is not yet complete
	if (nv < nvtx) {
		vp = vlast;
		vlast = (vlast < nvtx-1 ? vlast+1 : 0);
		vtx[vlast].dt = min(tgtstep/omega_updt,dtmax) * max(0.1, cos(vtx[vp].lat));
		vtx[vlast].t = vtx[vp].t + vtx[vlast].dt;
		CalcPoint (vtx[vlast], &omega_updt);
		v1++;
	} else { // ring is complete - update one of the future vertices
		if (vupdt <= vcurr) vupdt += nvtx;
		if (vupdt > v1) {
			vupdt = vcurr+1;
			omega_updt = omega_curr;
		}
		vupdt = vupdt % nvtx;
		vp = (vupdt ? vupdt-1 : nvtx-1);
		if (vp == vcurr) vp = (vp ? vp-1 : nvtx-1);

		vtx[vupdt].dt = min(tgtstep/omega_updt,dtmax) * max(1e-2, cos(vtx[vp].lat));
		vtx[vupdt].t = vtx[vp].t + vtx[vupdt].dt;
		CalcPoint (vtx[vupdt], &omega_updt);
		vupdt = (vupdt+1) % nvtx;
	}
	omega_updt = fabs (omega_updt);
}

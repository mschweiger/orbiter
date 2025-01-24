// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "Orbiter.h"
#include "VCockpit.h"
#include "Pane.h"
#include "Camera.h"
#include "Vessel.h"
#include "Texture.h"
#include "Log.h"
#include "Util.h"

using namespace std;

extern Orbiter *g_pOrbiter;
extern Camera *g_camera;
extern Vessel *g_focusobj;
extern char DBG_MSG[256];

// =======================================================================
// class VirtualCockpit

VirtualCockpit::VirtualCockpit (int _id, const Pane *_pane)
{
	int i;

	gc         = g_pOrbiter->GetGraphicsClient();
	id         = _id;
	pane       = _pane;
	narea      = nareabuf = 0;
	idx_mfocus = -1;
	mstate     = 0;
	hud.surf   = NULL;
	if (g_pOrbiter->IsFullscreen())
		cwnd = 0;
	else
		cwnd = g_pOrbiter->GetRenderWnd();
	for (i = 0; i < 4; i++)
		connect[i] = -1;
}

VirtualCockpit::~VirtualCockpit ()
{
	ReleaseAreas ();
	DestroyHUDSurface ();
}

void VirtualCockpit::SetConnections (int left, int right, int top, int bottom)
{
	connect[0] = left;
	connect[1] = right;
	connect[2] = top;
	connect[3] = bottom;
}

void VirtualCockpit::Shift (const Vector &shift)
{
	ShiftHUDPos (shift);
	ShiftAreas (shift);
}

void VirtualCockpit::DefineArea (int aid, const RECT &texrect, int draw_mode, int mouse_mode, int bkmode, SURFHANDLE tgt)
{
	int draw_event_mode = draw_mode & 0xFF; // strip access flags

	// sanity-checks
	if (draw_event_mode == PANEL_REDRAW_NEVER) bkmode = PANEL_MAP_NONE;

	if (narea == nareabuf) {
		Area **tmp = new Area*[nareabuf += 32]; TRACENEW
		if (narea) {
			memcpy (tmp, area, narea*sizeof(Area*));
			delete []area;
		}
		area = tmp;
	}
	area[narea] = new Area; TRACENEW
	area[narea]->id      = aid;
	area[narea]->texrect = texrect;
	area[narea]->w       = texrect.right-texrect.left;
	area[narea]->h       = texrect.bottom-texrect.top;
	area[narea]->bltmode = bkmode;
	area[narea]->redraw  = draw_event_mode;
	area[narea]->mouse   = mouse_mode;
	area[narea]->tgt     = tgt;
	area[narea]->cmode   = Area::CMODE_NONE;

	// allocate surfaces for area
	if (!gc || draw_event_mode == PANEL_REDRAW_NEVER || area[narea]->w == 0 || area[narea]->h == 0) {
		area[narea]->surf = NULL;
		area[narea]->bksurf = NULL;
	} else if (bkmode == PANEL_MAP_DIRECT) {
		area[narea]->surf = tgt;
		area[narea]->bksurf = NULL;
		area[narea]->bltmode = PANEL_MAP_NONE;
	} else {
		DWORD attrib = OAPISURFACE_RENDERTARGET;
		if (draw_mode & PANEL_REDRAW_GDI) attrib |= OAPISURFACE_GDI;
		if (draw_mode & PANEL_REDRAW_SKETCHPAD) attrib |= OAPISURFACE_SKETCHPAD;
		area[narea]->surf = gc->clbkCreateSurfaceEx (area[narea]->w, area[narea]->h, attrib);
		switch (bkmode) {
		case PANEL_MAP_NONE:
			area[narea]->bksurf = NULL;
			area[narea]->bltmode = PANEL_MAP_NONE;
			break;
		case PANEL_MAP_CURRENT:
			area[narea]->bksurf = NULL;
			area[narea]->bltmode = PANEL_MAP_NONE;
			gc->clbkBlt (area[narea]->surf, 0, 0, tgt, area[narea]->texrect.left, area[narea]->texrect.top,
				area[narea]->texrect.right-area[narea]->texrect.left, area[narea]->texrect.bottom-area[narea]->texrect.top);
			break;
		case PANEL_MAP_BACKGROUND:
		case PANEL_MAP_BGONREQUEST:
			if (area[narea]->bksurf = gc->clbkCreateSurfaceEx (area[narea]->w, area[narea]->h, OAPISURFACE_RENDERTARGET)) {
				gc->clbkBlt (area[narea]->bksurf, 0, 0, tgt, area[narea]->texrect.left, area[narea]->texrect.top,
					area[narea]->texrect.right-area[narea]->texrect.left, area[narea]->texrect.bottom-area[narea]->texrect.top);
			} else {
				area[narea]->bltmode = PANEL_MAP_NONE;
			}
			break;
		}
	}
	narea++;
}

int VirtualCockpit::AreaIndex (int aid)
{
	for (int i = 0; i < narea; i++)
		if (area[i]->id == aid) return i;
	return -1;
}

SURFHANDLE VirtualCockpit::GetArea (int idx)
{
	if (gc) {
		Area *a = area[idx];
		switch (a->bltmode) {
			case PANEL_MAP_NONE:
			case PANEL_MAP_BGONREQUEST:
				return a->surf;
			case PANEL_MAP_BACKGROUND:
				gc->clbkBlt (a->surf, 0, 0, a->bksurf, 0, 0, a->w, a->h);
				return a->surf;
			case PANEL_MAP_CURRENT:
				gc->clbkBlt (a->surf, 0, 0, a->tgt, a->texrect.left, a->texrect.top,
					a->texrect.right-a->texrect.left, a->texrect.bottom-a->texrect.top);
				return a->surf;
		}
	}
	return NULL;
}

bool VirtualCockpit::BltAreaBackground (int idx, SURFHANDLE s)
{
	if (gc && s) {
		Area *a = area[idx];
		if (a->bltmode == PANEL_MAP_BGONREQUEST && a->bksurf) {
			RECT r = {0,0,a->w,a->h};
			gc->clbkBlt (s, 0, 0, a->bksurf, 0, 0, a->w, a->h);
			return true;
		}
	}
	return false;
}

void VirtualCockpit::SetArea (int idx, SURFHANDLE s)
{
	if (gc && s) {
		Area *a = area[idx];
		if (a->tgt) {
			gc->clbkBlt (a->tgt, a->texrect.left, a->texrect.top, s, 0, 0, a->w, a->h);
		}
	}
}

void VirtualCockpit::RedrawArea (int idx, int event)
{
	if (idx < 0) return;
	SURFHANDLE s = GetArea(idx);
	if (g_focusobj->VCRedrawEvent (area[idx]->id, event, s) && s) {
		SetArea (idx, s);
	}
}

void VirtualCockpit::RedrawAllAreas (int event)
{
	for (int i = 0; i < narea; i++) {
		if ((area[i]->redraw & event) ||
			(area[i]->redraw && event == PANEL_REDRAW_INIT)) {
			RedrawArea (i, event);
		}
	}
}

void VirtualCockpit::ReleaseAreas ()
{
	if (!nareabuf) return;
	for (int i = 0; i < narea; i++) {
		switch (area[i]->bltmode) {
		case PANEL_MAP_BACKGROUND:
		case PANEL_MAP_BGONREQUEST:
			if (area[i]->bksurf) SetArea (i, area[i]->bksurf); // restore background
			break;
		}

		if (area[i]->surf && area[i]->surf != area[i]->tgt) {
			gc->clbkReleaseSurface (area[i]->surf);
			area[i]->surf = NULL;
		}
		if (area[i]->bksurf) {
			gc->clbkReleaseSurface (area[i]->bksurf);
			area[i]->bksurf = NULL;
		}
		delete area[i];
	}
	delete []area;
	area = NULL;
	narea = nareabuf = 0;
}

void VirtualCockpit::ShiftAreas (const Vector &shift)
{
	for (int i = 0; i < narea; i++) {
		switch (area[i]->cmode) {
		case Area::CMODE_SPHERICAL:
			area[i]->cnt += shift;
			break;
		case Area::CMODE_QUAD:
			SetClickZone_Quadrilateral (i, area[i]->p[0]+shift, area[i]->p[1]+shift, area[i]->p[2]+shift, area[i]->p[3]+shift);
			break;
		}
	}
}

SURFHANDLE VirtualCockpit::CreateHUDSurface (const VCHUDSPEC *spec, COLORREF col, double intens)
{
	const int HUDSIZE = g_pOrbiter->Cfg()->CfgInstrumentPrm.PanelMFDHUDSize;
	if (!hud.surf) {
		hud.surf = gc->clbkCreateSurfaceEx (HUDSIZE, HUDSIZE, OAPISURFACE_SKETCHPAD | OAPISURFACE_TEXTURE);
	}
	memcpy (&hud.spec, spec, sizeof (VCHUDSPEC));
	return hud.surf;
}

void VirtualCockpit::DestroyHUDSurface ()
{
	if (hud.surf) {
		gc->clbkReleaseSurface (hud.surf);
		//hud.surf->SetPalette (NULL);
		//hud.surf->Release();
		//hud.surf = 0;
	}
}

void VirtualCockpit::ShiftHUDPos (const Vector &shift)
{
	if (hud.surf) {
		hud.spec.hudcnt.x += (float)shift.x;
		hud.spec.hudcnt.y += (float)shift.y;
		hud.spec.hudcnt.z += (float)shift.z;
	}
}

void VirtualCockpit::ClearHUD ()
{
	if (hud.surf) {
		gc->clbkFillSurface (hud.surf, 0);
		//static DDBLTFX fx = {sizeof(DDBLTFX), 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
		//hud.surf->Blt (NULL, NULL, NULL, DDBLT_COLORFILL, &fx);
	}
}

void VirtualCockpit::SetHUDCol (COLORREF col, double intens)
{
#ifdef UNDEF
	if (hud.tpal) {
		if (col) hud.col = col;
		if (intens) hud.intens = (BYTE)(intens*200.0);

		PALETTEENTRY pe[256];
		memset (pe, 0, 256*sizeof(PALETTEENTRY));
		pe[1].peRed   /*** = pe[0].peRed ***/  = (BYTE)( hud.col        & 0xff);
		pe[1].peGreen /*** = pe[0].peGreen ***/ = (BYTE)((hud.col >>  8) & 0xff);
		pe[1].peBlue  /*** = pe[0].peBlue ***/ = (BYTE)((hud.col >> 16) & 0xff);
		/*** pe[1].peFlags = hud.intens; ***/
		hud.tpal->SetEntries (0, 0, 256, pe);
		pe[1].peFlags = pe[0].peRed = pe[0].peGreen = pe[0].peBlue = 0;
		hud.spal->SetEntries (0, 0, 256, pe);
	}
#endif
}

bool VirtualCockpit::SetClickZone_Spherical (int i, const Vector &cnt, double rad)
{
	area[i]->cnt.Set (cnt);
	area[i]->rad = rad;
	area[i]->cmode = Area::CMODE_SPHERICAL;
	return true;
}

bool VirtualCockpit::SetClickZone_Quadrilateral (int i,
	const Vector &p1, const Vector &p2, const Vector &p3, const Vector &p4)
{
	const double EPS = 1e-8;
	int j;

	// save corner points
	area[i]->p[0].Set (p1);
	area[i]->p[1].Set (p2);
	area[i]->p[2].Set (p3);
	area[i]->p[3].Set (p4);

	// global coefficients of equation of the plane: ax+by+cz+d = 0
	double a, b, c, d;
	PlaneCoeffs (p1, p2, p3, a, b, c, d);
	area[i]->a = (float)a;
	area[i]->b = (float)b;
	area[i]->c = (float)c;
	area[i]->d = (float)d;

	double pdst = fabs(PointPlaneDist(p4, a, b, c, d));
	if (pdst < EPS) { // the 4 points are coplanar, so we need to avoid singularity
		Vector nml(PlaneNormal(a, b, c, d));
		area[i]->p[3].Set(p4 + nml * EPS);
	}

	// calculate coefficients for mapping global quadrilateral to local square (0,1)x(0,1)
	// x' = u0 x + u1 y + u2 z + u3;     y' = v0 x + v1 y + v2 z + v3
	Matrix4 P1(
		area[i]->p[0].x, area[i]->p[0].y, area[i]->p[0].z, 1,
		area[i]->p[1].x, area[i]->p[1].y, area[i]->p[1].z, 1,
		area[i]->p[2].x, area[i]->p[2].y, area[i]->p[2].z, 1,
		area[i]->p[3].x, area[i]->p[3].y, area[i]->p[3].z, 1
	);
	Matrix4 P2(P1);
	Vector4 cc, dd, r;
	Vector4 u(0,1,0,1), v(0,0,1,1);
	qrdcmp (P1, cc, dd);
	qrsolv (P1, cc, dd, u);
	for (j = 0; j < 4; j++) area[i]->u[j] = (float)u(j);
	qrdcmp (P2, cc, dd);
	qrsolv (P2, cc, dd, v);
	for (j = 0; j < 4; j++) area[i]->v[j] = (float)v(j);

	area[i]->cmode = Area::CMODE_QUAD;
	return true;
}

void VirtualCockpit::GetClickZones(std::list<VCClickZone>* p_List)
{
	if (!p_List) return;
	for (int i = 0; i < narea; i++) {
		auto& a = area[i];
		VCClickZone A;
		A.id = a->id;
		A.mode = a->cmode;
		if (a->cmode == Area::ClickMode::CMODE_QUAD) for (int k = 0; k < 4; k++) {
			A.pt[k].x = a->p[k].x;
			A.pt[k].y = a->p[k].y;
			A.pt[k].z = a->p[k].z;
		}
		if (a->cmode == Area::ClickMode::CMODE_SPHERICAL) { 
			A.cnt.x = a->cnt.x; 
			A.cnt.y = a->cnt.y;
			A.cnt.z = a->cnt.z;
			A.rad = a->rad; 
		}
		p_List->push_back(A);
	}	
}

bool VirtualCockpit::ProcessMouse (UINT event, DWORD state, int x, int y)
{
	mstate = 0;
	switch (event) {
	case WM_LBUTTONDOWN:
		state = PANEL_MOUSE_LBDOWN | PANEL_MOUSE_LBPRESSED;
		break;
	case WM_RBUTTONDOWN:
		state = PANEL_MOUSE_RBDOWN | PANEL_MOUSE_RBPRESSED;
		break;
	case WM_LBUTTONUP:
		state = PANEL_MOUSE_LBUP;
		break;
	case WM_RBUTTONUP:
		state = PANEL_MOUSE_RBUP;
		break;
	}
	if (state & PANEL_MOUSE_DOWN) { // locate mouse event
		idx_mfocus = -1;

		// convert mouse position into vessel-local ray
		Vector gdir, ldir;
		g_camera->ViewportToGlobalDir (x, y, gdir);
		ldir = tmul (g_focusobj->GRot(), gdir);

		// vessel-local camera position
		Vector cpos (tmul (g_focusobj->GRot(), *g_camera->GPosPtr() - g_focusobj->GPos()));

		int i, imatch;
		double minreldist, mx = 0, my = 0;
		for (i = 0, imatch = -1, minreldist = 1.0; i < narea; i++) {

			switch (area[i]->cmode) {
			case Area::CMODE_SPHERICAL: {
				if (dotp(ldir, area[i]->cnt-cpos) > 0.0) { // otherwise target is behind camera
					double d = PointLineDist (area[i]->cnt, cpos, ldir);
					if (d < area[i]->rad) {
						if (d/area[i]->rad < minreldist) {
							mouse_r.x = d;
							mouse_r.y = mouse_r.z = 0.0;
							imatch = i, minreldist = d/area[i]->rad;
						}
					}
				}
				} break;
			case Area::CMODE_QUAD: {
				Vector r;
				if (LinePlaneIntersect (area[i]->a, area[i]->b, area[i]->c, area[i]->d, cpos, ldir, r)) {
					if (dotp(ldir, r-cpos) > 0.0) { // otherwise target is behind camera
						mx = area[i]->u[0]*r.x + area[i]->u[1]*r.y + area[i]->u[2]*r.z + area[i]->u[3];
						my = area[i]->v[0]*r.x + area[i]->v[1]*r.y + area[i]->v[2]*r.z + area[i]->v[3];
						if (mx >= 0 && mx <= 1 && my >= 0 && my <= 1) {
							mouse_r.x = mx;
							mouse_r.y = my;
							mouse_r.z = 0.0;
							imatch = i;
						}
					}
				}
				} break;
			}

		}
		if (imatch < 0) return false;
		idx_mfocus = imatch;

		if (!(mstate = state & area[imatch]->mouse)) return false;
		if (g_focusobj->bPlayback() && !(area[imatch]->mouse & PANEL_MOUSE_ONREPLAY)) {
			mstate = 0;
			return false;
		}
		return true;
	} else {
		if (idx_mfocus < 0 || !(mstate = state & area[idx_mfocus]->mouse)) return false;
		return true;
	}
}

void VirtualCockpit::GetMouseState (int &idx, int &state, Vector &xs) const
{
	if (mstate & PANEL_MOUSE_PRESSED) {
		POINT pt;
		GetCursorPos (&pt);
		if (cwnd) // need to subtract client window offset
			ScreenToClient (cwnd, &pt);

		// calculate ray intersection with current focus area
		Vector gdir, ldir;
		g_camera->ViewportToGlobalDir (pt.x, pt.y, gdir);
		ldir = tmul (g_focusobj->GRot(), gdir);

		// vessel-local camera position
		Vector cpos (tmul (g_focusobj->GRot(), *g_camera->GPosPtr() - g_focusobj->GPos()));

		switch (area[idx_mfocus]->cmode) {
		case Area::CMODE_SPHERICAL: {
			double d = PointLineDist (area[idx_mfocus]->cnt, cpos, ldir);
			// need to check that cnt is not BEHIND the camera!
			mouse_r.x = d;
			mouse_r.y = mouse_r.z = 0.0;
			} break;
		case Area::CMODE_QUAD: {
			Vector r;
			LinePlaneIntersect (area[idx_mfocus]->a, area[idx_mfocus]->b, area[idx_mfocus]->c, area[idx_mfocus]->d, cpos, ldir, r);
			mouse_r.x = area[idx_mfocus]->u[0]*r.x + area[idx_mfocus]->u[1]*r.y + area[idx_mfocus]->u[2]*r.z + area[idx_mfocus]->u[3];
			mouse_r.y = area[idx_mfocus]->v[0]*r.x + area[idx_mfocus]->v[1]*r.y + area[idx_mfocus]->v[2]*r.z + area[idx_mfocus]->v[3];
			mouse_r.z = 0.0;
			} break;
		}
	}
	idx = idx_mfocus; state = mstate;
	xs = mouse_r;
}

bool VirtualCockpit::Read (ifstream &ifs)
{
	if (!FindLine (ifs, "BEGIN_VC")) return false;
	// read panel parameters
	return true;
}

void VirtualCockpit::Write (ostream &ofs) const
{
	ofs << "BEGIN_VC" << endl;
	ofs << "END_VC" << endl;
}

void VirtualCockpit::OptionChanged(DWORD cat, DWORD item)
{
}

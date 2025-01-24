// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#define OAPI_IMPLEMENTATION

#include "Pane.h"
#include "Defpanel.h"
#include "Panel2D.h"
#include "Panel.h"
#include "MenuInfoBar.h"
#include "VCockpit.h"
#include <stdio.h>
#include "Camera.h"
#include "D3dmath.h"
#include "Log.h"
#include <assert.h>

using namespace std;

extern Orbiter *g_pOrbiter;
extern TimeData td;
extern Camera *g_camera;
extern Vessel *g_focusobj;
extern char DBG_MSG[256];

static COLORREF normalColor = RGB (  0, 255, 0);
static COLORREF infoColor   = RGB (224, 192, 0);
static COLORREF brightColor = RGB (255, 224, 128);

			   
Pane::Pane (oapi::GraphicsClient *gclient, HWND hwnd, int width, int height, int bpp)
{
	// Note: gclient is assumed to be a valid pointer. Nongraphics orbiter
	// instances should not create a Pane.

	int i;

	gc        = gclient;
	W         = width;
	H         = height;
	BPP       = bpp;
	hWnd      = hwnd;
	hud       = 0;
	hudmode   = HUD_NONE;

	for (i = 0; i < MAXMFD; i++) {
		mfd[i].instr     = 0;
		mfd[i].lastmode  = MFD_NONE;
		mfd[i].exist     = false;
		mfd[i].active    = false;
		mfd[i].upDTscale = 1.0;
	}
	nemfd = 0;

	panelmode = 0;
	defpanel  = 0;
	panel2d   = 0;
	panel     = 0;
	vcockpit  = 0;
	
	if (gc) mibar = new MenuInfoBar (this);
	else    mibar = NULL;

	
	i = g_pOrbiter->Cfg()->CfgInstrumentPrm.bMfdPow2;
	if (i == 2) {
		DWORD val;
		gc->clbkGetRenderParam (RP_REQUIRETEXPOW2, &val);
		mfdsize_pow2 = (val > 0);
	} else mfdsize_pow2 = (i > 0);
	mfd_hires_threshold = g_pOrbiter->Cfg()->CfgInstrumentPrm.MfdHiresThreshold;
	mfd_vc_size = g_pOrbiter->Cfg()->CfgInstrumentPrm.VCMFDSize;

	InitResources ();
	SetHUDColour (g_pOrbiter->Cfg()->CfgCameraPrm.HUDCol, 1.0, true);

	blinkmesh.mesh2d = NULL;

	mfdTex_blank = NULL;
	if (gc) {
		mfdTex_blank = gc->clbkCreateTexture (2,2);
		static DDBLTFX bltfx = {sizeof(DDBLTFX), 0};
		bltfx.dwFillColor = 0;
		gc->clbkFillSurface (mfdTex_blank, 0);
	}
}

Pane::~Pane ()
{
	DWORD i;
	for (i = 0; i < MAXMFD; i++)
		if (mfd[i].instr) delete mfd[i].instr;
	if (nemfd) {
		for (i = 0; i < nemfd; i++)
			delete emfd[i];
		delete []emfd;
		emfd = NULL;
	}
	FreeResources ();
	if (mibar)    delete mibar;
	if (defpanel) delete defpanel;
	if (panel2d)  delete panel2d;
	if (panel)    delete panel;
	if (vcockpit) delete vcockpit;
	if (hud)      delete hud;
	if (blinkmesh.mesh2d) oapiDeleteMesh (blinkmesh.mesh2d);
	if (mfdTex_blank) gc->clbkReleaseSurface (mfdTex_blank);
}

void Pane::RestoreDeviceObjects (LPDIRECT3D7 d3d, LPDIRECT3DDEVICE7 dev)
{
	if (defpanel) defpanel->RestoreDeviceObjects (d3d, dev);
	for (int i = 0; i < MAXMFD; i++)
		if (mfd[i].instr) mfd[i].instr->RestoreDeviceObjects (d3d, dev);
}

void Pane::FocusChanged (const Vessel *focus)
{
	// Turn off MFDs to avoid problems
	//OpenMFD (0, MFD_NONE);
	//OpenMFD (1, MFD_NONE);
	if (panelmode) SetPanelMode (panelmode, true);
	Instrument::ClearDisabledModes();
	for (DWORD i = 0; i < nemfd; i++)
		emfd[i]->clbkFocusChanged ((OBJHANDLE)focus);
}

void Pane::DelVessel (const Vessel *vessel)
{
	for (DWORD i = 0; i < nemfd; i++)
		if ((Vessel*)emfd[i]->GetVessel() == vessel)
			emfd[i]->SetVessel ((OBJHANDLE)g_focusobj);
	BroadcastMFDMessage (MSG_KILLVESSEL, (void*)vessel);
	if (hud) hud->ProcessMessage (MSG_KILLVESSEL, (void*)vessel);
}

bool Pane::MFDConsumeKeyBuffered (int id, DWORD key)
{
	if (key == OAPI_KEY_ESCAPE) {
		ToggleMFD_on (id);
		return true;
	} else if (mfd[id].instr) {
		return mfd[id].instr->ConsumeKeyBuffered (key);
	}
	return false;
}

bool Pane::ProcessMouse_System(UINT event, DWORD state, DWORD x, DWORD y, const char *kstate)
{
	if (g_camera->IsExternal()) return false;
	// respond to mouse events only in cockpit mode

	bool consumed = false;
	if (defpanel)      /*consumed = defpanel->ProcessMouse(event, state, x, y)*/;
	else if (panel2d)  consumed = panel2d->ProcessMouse_System(event, state, x, y, kstate);
	else if (panel)    /*consumed = panel->ProcessMouse(event, state, x, y)*/;
	else if (vcockpit) /*consumed = vcockpit->ProcessMouse(event, state, x, y)*/;
	return consumed;
}

bool Pane::ProcessMouse_OnRunning (UINT event, DWORD state, DWORD x, DWORD y, const char *kstate)
{
	if (g_camera->IsExternal()) return false;
	// respond to mouse events only in cockpit mode

	bool consumed = false;
	if (defpanel)      consumed = defpanel->ProcessMouse (event, state, x, y);
	else if (panel2d)  consumed = panel2d->ProcessMouse_OnRunning (event, state, x, y, kstate);
	else if (panel)    consumed = panel->ProcessMouse (event, state, x, y);
	else if (vcockpit) consumed = vcockpit->ProcessMouse (event, state, x, y);
	return consumed;
}

void Pane::TogglePanelMode ()
{
	for (int i = 0; i < 3 && !SetPanelMode ((panelmode%3) + 1); i++);
}

void Pane::RedrawCockpitAreas (int mode)
{
	if (panel2d)       panel2d->RedrawAllAreas (mode);
	else if (vcockpit) vcockpit->RedrawAllAreas (mode);
	else if (panel)    panel->RedrawAllAreas (mode);
}

bool Pane::SetPanelMode (int pmode, bool force)
{
	int i;
	bool ok = false;

	if (pmode == panelmode && !force) return true; // nothing to do
	panelmode = pmode;
	
	Vessel::UnsetCameraMovement();

	for (i = 0; i < MAXMFD; i++) {
		if (mfd[i].exist) UnregisterMFD (i);
		mfd[i].upDTscale = 1.0;
	}

	for (i = 0; i < 3; i++) {

		if (defpanel) { delete defpanel; defpanel = 0; }
		if (panel)    { delete panel;    panel    = 0; }
		if (panel2d)  { delete panel2d;  panel2d  = 0; }
		if (vcockpit) { delete vcockpit; vcockpit = 0; }

		switch (panelmode) {
		case 1:
			defpanel = new DefaultPanel (this, colidx); TRACENEW
			ok = g_focusobj->SetGenericCockpit();
			break;
		case 2:
			panel2d = new Panel2D (0, this, g_pOrbiter->Cfg()->CfgLogicPrm.PanelScale); TRACENEW
			if (ok = g_focusobj->LoadPanel2D (0, panel2d)) {
				//panel2d->RedrawAllAreas (PANEL_REDRAW_INIT);
				panel2d->Setup();
				panel_dx = panel_dy = 0.0;
				break;
			} else {
				delete panel2d;
				panel2d = 0;
			}
			panel = new Panel (0, this, g_pOrbiter->Cfg()->CfgLogicPrm.PanelScale); TRACENEW
			if (ok = g_focusobj->LoadPanel (0)) {
				//panel->RedrawAllAreas (PANEL_REDRAW_INIT);
				panel->Setup ();
				panel_dx = panel_dy = 0.0;
			} else {
				delete panel;
				panel = 0;
			}
			break;
		case 3:
			vcockpit = new VirtualCockpit (0, this); TRACENEW
			if (ok = g_focusobj->LoadVC (0)) {
				vcid = 0;
				//vcockpit->RedrawAllAreas (PANEL_REDRAW_INIT);
			} else {
				delete vcockpit;
				vcockpit = 0;
			}
			break;
		}
		if (ok) break;
		else panelmode = panelmode%3 + 1;
	}

	for (i = 0; i < MAXMFD; i++) {
		if      (panelmode == 0) mfd[i].exist = false;
		else if (panelmode == 1) mfd[i].exist = (i < 2);
		if (mfd[i].exist && mfd[i].lastmode != MFD_NONE && mfd[i].active)
			OpenMFD (i, mfd[i].lastmode);
	}

	if (hud)
		if (!vcockpit || !vcockpit->GetHUDSurf()) hud->Resize (false);

	static IntCamMode cmode[3] = {CAMERA_GENERICCOCKPIT,CAMERA_2DPANELCOCKPIT,CAMERA_VIRTUALCOCKPIT};
	g_camera->SetIntMode (cmode[panelmode-1]);
	RedrawCockpitAreas (PANEL_REDRAW_INIT);

	return ok;
}

int Pane::SelectPanel (int id)
{
	int i;

	if (panelmode != 2 || g_camera->IsExternal()) return -1;

	for (i = 0; i < MAXMFD; i++) {
		if (mfd[i].exist) UnregisterMFD (i);
		mfd[i].upDTscale = 1.0;
	}

	if (panel2d) delete panel2d;
	if (panel) delete panel;

	panel2d = new Panel2D (id, this, g_pOrbiter->Cfg()->CfgLogicPrm.PanelScale); TRACENEW
	if (g_focusobj->LoadPanel2D (id, panel2d)) {
		panel2d->RedrawAllAreas (PANEL_REDRAW_INIT);
		panel2d->Setup ();
		panel_dx = panel_dy = 0.0;
		for (i = 0; i < MAXMFD; i++) {
			if (mfd[i].exist && mfd[i].active)
				OpenMFD (i, mfd[i].lastmode);
		}
		return id;
	} else {
		delete panel2d;
		panel2d = 0;
	}

	panel = new Panel (id, this, g_pOrbiter->Cfg()->CfgLogicPrm.PanelScale); TRACENEW
	if (g_focusobj->LoadPanel (id)) {
		panel->RedrawAllAreas (PANEL_REDRAW_INIT);
		panel->Setup ();
		panel_dx = panel_dy = 0.0;

		for (i = 0; i < MAXMFD; i++) {
			if (mfd[i].exist && mfd[i].active)
				OpenMFD (i, mfd[i].lastmode);
		}
		return id;
	} else {
		delete panel;
		panel = 0;
		return -1;
	}
}

int Pane::SelectVC (int id)
{
	int i;

	if (panelmode != 3 || g_camera->IsExternal()) return -1;
	for (i = 0; i < MAXMFD; i++) {
		if (mfd[i].exist) UnregisterMFD (i);
		mfd[i].upDTscale = 1.0;
	}

	if (vcockpit) delete vcockpit;
	vcockpit = new VirtualCockpit (id, this); TRACENEW
	if (g_focusobj->LoadVC (id)) {
		vcid = id;
		g_camera->ResetCockpitPos();
		g_camera->ResetCockpitDir (false);
		for (i = 0; i < MAXMFD; i++) {
			if (mfd[i].exist && mfd[i].active)
				OpenMFD (i, mfd[i].lastmode);
		}
		vcockpit->RedrawAllAreas (PANEL_REDRAW_INIT);
		return id;
	} else {
		delete vcockpit;
		vcockpit = 0;
		return -1;
	}
}

int Pane::SwitchPanel (int dir)
{
	int id = -1;

	if (g_camera->IsExternal()) return -1;

	switch (panelmode) {
	case 2:
		if (panel2d) {
			id = panel2d->Connect (dir);
			if (id >= 0) id = SelectPanel (id);
		} else if (panel) {
			id = panel->Connect (dir);
			if (id >= 0) id = SelectPanel (id);
		}
		break;
	case 3:
		if (vcockpit) {
			id = vcockpit->Connect (dir);
			if (id >= 0) id = SelectVC (id);
		}
		break;
	}
	return id;
}

void Pane::ShiftPanel (double dx, double dy)
{
	if ((!panel && !panel2d) || g_camera->IsExternal()) return;

	// only move in whole pixels to avoid aliasing problems
	panel_dx += dx;
	panel_dy += dy;
	int ix = (int)panel_dx;
	int iy = (int)panel_dy;
	if (ix || iy) {
		if (ix) panel_dx -= ix;
		if (iy) panel_dy -= iy;
		if (panel2d) panel2d->Move (ix, iy);
		else         panel->Move (ix, iy);
	}
}

void Pane::ShowHUD (bool yes)
{
	SetHUDMode (yes ? hudmode : HUD_NONE);
}

void Pane::SwitchHUDMode ()
{
	SetHUDMode (hudmode == HUD_DOCKING ? HUD_ORBIT : hudmode+1);
}

bool Pane::SetHUDMode (int _hudmode)
{
	if (hud) {
		if (_hudmode == hud->Mode()) return false; // nothing to do
		delete hud;
		hud = 0;
	} else if (_hudmode == HUD_NONE) {
		return false; // nothing to do
	}
	switch (_hudmode) {
	case HUD_ORBIT:
		hud = new HUD_Orbit (this); TRACENEW
		break;
	case HUD_SURFACE:
		hud = new HUD_Surface (this); TRACENEW
		break;
	case HUD_DOCKING:
		hud = new HUD_Docking (this); TRACENEW
		break;
	default:
		hud = 0;
		break;
	}
	if (_hudmode != HUD_NONE) hudmode = _hudmode;
	g_focusobj->HUDchanged (_hudmode);
	return true;
}

bool Pane::SetHUDMode (int _hudmode, const HUDPARAM *prm)
{
	bool changed = SetHUDMode (_hudmode);
	if (hud && hud->Mode() == _hudmode) {
		switch (_hudmode) {
			case HUD_ORBIT: {
				const Body *curRef = ((HUD_Orbit*)hud)->GetReference();
				Body *newRef = (Body*)prm->HUDorbit.hRef;
				if (newRef != curRef) {
					((HUD_Orbit*)hud)->SetReference (newRef);
					changed = true;
				}
				} break;
			case HUD_DOCKING: {
				DWORD curNav = ((HUD_Docking*)hud)->GetNav();
				DWORD newNav = prm->HUDdocking.NavIdx;
				if (curNav != newNav) {
					((HUD_Docking*)hud)->SetNav (newNav);
					changed = true;
				}
				} break;
		}
	}
	return changed;
}

int Pane::GetHUDMode (HUDPARAM *prm) const
{
	if (hud) {
		if (prm) {
			switch (hud->Mode()) {
			case HUD_ORBIT:
				prm->HUDorbit.hRef = (OBJHANDLE)((HUD_Orbit*)hud)->GetReference();
				break;
			case HUD_DOCKING:
				prm->HUDdocking.NavIdx = ((HUD_Docking*)hud)->GetNav();
				break;
			}
		}
		return hud->Mode();
	} else
		return 0;
}

void Pane::SetHUDColour (int idx, double intens, bool force)
{
	COLORREF col[4] = {0x00ff00, 0x0044cc, 0x80ffff, 0xcca300};

	bool change_col    = (idx >= 0 && (idx != colidx || force));
	bool change_intens = (intens >= 0.0 && (intens != hudIntens || force));
	if (!change_col && !change_intens) return;

	if (change_col)    colidx = idx;
	if (change_intens) hudIntens = intens;
	COLORREF hue = col[colidx];
	hudCol = ((DWORD)((hue & 0xff) * hudIntens)) +
			 ((DWORD)(((hue >> 8) & 0xff) * hudIntens) << 8) +
             ((DWORD)((hue >> 16) * hudIntens) << 16);
	if (hudpen) gc->clbkReleasePen (hudpen);
	hudpen = gc->clbkCreatePen (1, 0, hudCol);
	if (change_col) {
		if (defpanel) defpanel->SwitchColour (colidx);
		if (hud) hud->SwitchColour (colidx);
		if (panelmode == 1) {
			if (mfd[0].instr) mfd[0].instr->Refresh();
			if (mfd[1].instr) mfd[1].instr->Refresh();
		}
		g_pOrbiter->Cfg()->CfgCameraPrm.HUDCol = colidx;
	}
}

void Pane::ToggleHUDColour ()
{
	SetHUDColour ((colidx+1)%4, -1.0);
}

void Pane::SetHUDIntens (double val)
{
	SetHUDColour (-1, min (1.0, max (0.0, val)));
}

void Pane::IncHUDIntens ()
{
	SetHUDColour (-1, min (1.0, hudIntens + td.SysDT*0.3));
}

void Pane::DecHUDIntens ()
{
	SetHUDColour (-1, max (0.0, hudIntens - td.SysDT*0.3));
}

void Pane::RenderCustomHUD (MESHHANDLE hMesh, SURFHANDLE *hTex)
{
	if (hud) hud->RenderCustom (hMesh, hTex);
}

void Pane::SetFOV (double _fov)
{
	if (mibar) mibar->SetFOV (_fov);
}

void Pane::SetWarp (double _warp)
{
	if (mibar) mibar->SetWarp (_warp);
}

// Update GDI elements of 2D pane
void Pane::Update (double simt, double syst)
{
	DWORD j;

	// update menu bar
	if (mibar) mibar->Update (simt);

	// update external MFDs
	for (j = 0; j < nemfd; j++)
		emfd[j]->clbkUpdate();

	bool global_hud = true;
	int i, trimy = (13*f1H)/2;

	// perform any BLT updates
	if (!g_camera->IsExternal()) {
		// update MFDs
		for (i = 0; i < MAXMFD; i++) {
			if (mfd[i].instr) {
				mfd[i].instr->Update (mfd[i].upDTscale);
			}
		}
		if (panel2d) {
			int idx, aid, mstate, mx, my;
			void *context;
			panel2d->Update (simt, syst);
			panel2d->GetMouseState (idx, mstate, mx, my);
			if (mstate) {
				aid = panel2d->area[idx]->id;
				context = panel2d->area[idx]->context;
				if (g_focusobj->PanelMouseEvent (aid, mstate, mx, my, context) && (panel2d->area[idx]->redraw & PANEL_REDRAW_MOUSE))
					panel2d->RedrawArea (idx, PANEL_REDRAW_MOUSE);
				int nmstate = 0;
				if (panel2d->area[idx]->mouse & PANEL_MOUSE_LBPRESSED && mstate & (PANEL_MOUSE_LBDOWN | PANEL_MOUSE_LBPRESSED)) nmstate |= PANEL_MOUSE_LBPRESSED;
				if (panel2d->area[idx]->mouse & PANEL_MOUSE_RBPRESSED && mstate & (PANEL_MOUSE_RBDOWN | PANEL_MOUSE_RBPRESSED)) nmstate |= PANEL_MOUSE_RBPRESSED;
				panel2d->SetMouseState (nmstate);
			}
			panel2d->RedrawAllAreas (PANEL_REDRAW_ALWAYS);
		} else if (panel) { // update other panel instruments
			int idx, aid, mstate, mx, my;
			panel->GetMouseState (idx, mstate, mx, my);
			if (mstate) {
				aid = panel->area[idx]->id;
				if (g_focusobj->PanelMouseEvent (aid, mstate, mx, my, 0) && (panel->area[idx]->redraw & PANEL_REDRAW_MOUSE))
					panel->RedrawArea (idx, PANEL_REDRAW_MOUSE);
				int nmstate = 0;
				if (panel->area[idx]->mouse & PANEL_MOUSE_LBPRESSED && mstate & (PANEL_MOUSE_LBDOWN | PANEL_MOUSE_LBPRESSED)) nmstate |= PANEL_MOUSE_LBPRESSED;
				if (panel->area[idx]->mouse & PANEL_MOUSE_RBPRESSED && mstate & (PANEL_MOUSE_RBDOWN | PANEL_MOUSE_RBPRESSED)) nmstate |= PANEL_MOUSE_RBPRESSED;
				panel->SetMouseState (nmstate);
			}
			panel->RedrawAllAreas (PANEL_REDRAW_ALWAYS);
		} else if (vcockpit) {
			int idx, aid, mstate;
			Vector vx;
			vcockpit->GetMouseState (idx, mstate, vx);
			if (mstate) {
				aid = vcockpit->area[idx]->id;
				if (g_focusobj->VCMouseEvent (aid, mstate, vx) && (vcockpit->area[idx]->redraw & PANEL_REDRAW_MOUSE))
					vcockpit->RedrawArea (idx, PANEL_REDRAW_MOUSE);
				int nmstate = 0;
				if (vcockpit->area[idx]->mouse & PANEL_MOUSE_LBPRESSED && mstate & (PANEL_MOUSE_LBDOWN | PANEL_MOUSE_LBPRESSED)) nmstate |= PANEL_MOUSE_LBPRESSED;
				if (vcockpit->area[idx]->mouse & PANEL_MOUSE_RBPRESSED && mstate & (PANEL_MOUSE_RBDOWN | PANEL_MOUSE_RBPRESSED)) nmstate |= PANEL_MOUSE_RBPRESSED;
				vcockpit->SetMouseState (nmstate);
			}
			vcockpit->RedrawAllAreas (PANEL_REDRAW_ALWAYS);
		} else if (defpanel) {
			int mstate, mf, bt;
			defpanel->GetButtonState (mstate, mf, bt);
			if (mstate) {
				int nmstate = 0;
				if (mstate & (PANEL_MOUSE_LBDOWN | PANEL_MOUSE_LBPRESSED)) nmstate |= PANEL_MOUSE_LBPRESSED;
				if (mstate & (PANEL_MOUSE_RBDOWN | PANEL_MOUSE_RBPRESSED)) nmstate |= PANEL_MOUSE_RBPRESSED;
				defpanel->SetMouseState (nmstate);
				if (mfd[mf].instr)
					mfd[mf].instr->ConsumeButton (bt, mstate);
			}
		}

		// update GDI HUD elements
		if (hud && vcockpit) {
			SURFHANDLE hudsurf = vcockpit->GetHUDSurf();
			if (hudsurf) {
				vcockpit->ClearHUD();
				oapi::Sketchpad *skp = gc->clbkGetSketchpad (hudsurf);
				if (skp) {
					SetSketchpadDefault (skp);
					hud->Draw (skp);
					gc->clbkReleaseSketchpad (skp);
				}
			}
		}
	}
}

void Pane::Draw ()
{
	if (g_camera && !g_camera->IsExternal() && !vcockpit && hud) {
		oapi::Sketchpad *skp = gc->clbkGetSketchpad (0);
		if (skp) {
			SetSketchpadDefault (skp);
			hud->Draw (skp);
			gc->clbkReleaseSketchpad (skp);
		}
	}
}

// Update rendered elements of 2D pane
void Pane::Render ()
{
	// display the generic panel after the GDI updates so that the MFDs end up on top of the HUD
	if (g_camera->IsInternal()) {
		if (panel2d) {           // 2D instrument panel mode
			panel2d->Render ();
			if (hud) hud->Render ();
		} else if (panel) {      // legacy panel mode
			panel->Display (0);
			if (hud) hud->Render ();
		} else if (defpanel) {   // generic glass cockpit mode
			if (hud) hud->Render ();
			defpanel->Render ();
		}
	}

	// Render info boxes at top left and right screen corners
	if (mibar) mibar->Render ();
}

void Pane::Timejump ()
{
	int i;
	DWORD j;
	for (i = 0; i < MAXMFD; i++)
		if (mfd[i].instr) mfd[i].instr->Timejump();
	for (j = 0; j < nemfd; j++)
		if (emfd[j]->instr) emfd[j]->instr->Timejump();
}

void Pane::TriggerRedrawArea (int pid, int vcid, int area_id)
{
	if      (panel || panel2d) TriggerPanelRedrawArea (pid, area_id);
	else if (vcockpit)         TriggerVCRedrawArea (vcid, area_id);
}

void Pane::TriggerPanelRedrawArea (int pid, int aid)
{
	if (panel) {
		if (panel->GetId() == pid) {
			int idx = panel->AreaIndex (aid);
			if (idx >= 0)
				panel->RedrawArea (idx, PANEL_REDRAW_USER);
		}
	} else if (panel2d) {
		if (panel2d->GetId() == pid) {
			int idx = panel2d->AreaIndex (aid);
			if (idx >= 0)
				panel2d->RedrawArea (idx, PANEL_REDRAW_USER);
		}
	}
}

void Pane::SetPanel2DBlink (VECTOR3 v[4])
{
	if (blinkmesh.mesh2d) oapiDeleteMesh (blinkmesh.mesh2d);
	if (v) {
		NTVERTEX vtx[4] = {
			{(float)v[0].x,(float)v[0].y,0,  0,0,0,  0,0},
			{(float)v[1].x,(float)v[1].y,0,  0,0,0,  0,1},
			{(float)v[2].x,(float)v[2].y,0,  0,0,0,  1,0},
			{(float)v[3].x,(float)v[3].y,0,  0,0,0,  1,1}
		};
		WORD idx[6] = {
			0,1,2,  3,2,1
		};
		MESHGROUP grp = {vtx, idx, 4, 6, 0, 0, 0, 0, 0};
		blinkmesh.mesh2d = oapiCreateMesh (1, &grp);
	} else {
		blinkmesh.mesh2d = NULL;
	}
}

bool Pane::BltPanelAreaBackground (int aid, SURFHANDLE surf)
{
	if (panel) {
		int idx = panel->AreaIndex (aid);
		if (idx >= 0)
			return panel->BltAreaBackground (idx, surf);
	} else if (vcockpit) {
		int idx = vcockpit->AreaIndex (aid);
		if (idx >= 0)
			return vcockpit->BltAreaBackground (idx, surf);
	}
	return false;
}

void Pane::TriggerVCRedrawArea (int vcid, int aid)
{
	if (vcockpit && ((vcid < 0) || (vcockpit->GetId () == vcid))) {
		int idx = vcockpit->AreaIndex (aid);
		vcockpit->RedrawArea (idx, PANEL_REDRAW_USER);
	}
}

void Pane::InitResources ()
{
	oapi::Sketchpad *skp = gc->clbkGetSketchpad(NULL);
	int h = H/50;
	f2H = max(12, min(20, H/50));

	hudfont[0] = gc->clbkCreateFont (-h, false, "Fixed");
	hudfont[1] = gc->clbkCreateFont (f2H, true, "Sans");
	hudpen  = gc->clbkCreatePen (1, 0, RGB(0,255,0));

	DWORD charsize;
	if (skp) {
		skp->SetFont (hudfont[0]);
		charsize = skp->GetCharSize ();
		f1W = HIWORD(charsize);
		f1H = LOWORD(charsize);
		scaleW = f1W * 10;
		skp->SetFont (hudfont[1]);
		charsize = skp->GetCharSize ();
		f2W = HIWORD(charsize);
		f2H = LOWORD(charsize);
		gc->clbkReleaseSketchpad (skp);
	} else {
		f1W = f1H = 0;
	}

	// OBSOLETE
	hPen[0] = CreatePen (PS_SOLID, 0, RGB(0,255,0));
	hPen[1] = CreatePen (PS_SOLID, 0, RGB(0,255,0));
	hPen[2] = CreatePen (PS_SOLID, 0, RGB(0,128,0));
	hPen[3] = CreatePen (PS_SOLID, 0, RGB(128,128,0));
	hPen[4] = CreatePen (PS_SOLID, 0, RGB(64,64,0));
	hPen[5] = CreatePen (PS_SOLID, 0, RGB(128,128,128));
	static LOGBRUSH lbrush1 = {BS_SOLID, RGB(0,128,0), 0};
	static LOGBRUSH lbrush2 = {BS_SOLID, RGB(96,96,0), 0};
	hBrush1 = CreateBrushIndirect (&lbrush1);
	hBrush2 = CreateBrushIndirect (&lbrush2);
	// END OBSOLETE

	blinkmesh.tex = (gc ? gc->clbkLoadTexture ("transp.dds", 0x4) : NULL);

#ifdef UNDEF
	const int nvtx = 8;
	const int nidx = 12;
	const float texh = 256.0f;
	const float texw = 512.0f;
	const float lineh = 16.0f;
	const float boxh  = (float)(max(H/20.0,sqrt((double)H)*2)), boxw = boxh*3.6f;
	const float ofsx = 4.0f, ofsy = 4.0f;
	//const float boxh = lineh*4, boxw = lineh*16;
	NTVERTEX vtx[nvtx] = {
		{W-boxw,ofsy,0, 0,0,0, 0,(texh-4*lineh)/texh},  // right box
		{W-boxw,ofsy+boxh,0, 0,0,0, 0,1},
		{(float)W,ofsy,0, 0,0,0, 16*lineh/texw,(texh-4*lineh)/texh},
		{(float)W,ofsy+boxh,0, 0,0,0, 16*lineh/texw,1},
		{ofsx,ofsy,0, 0,0,0, 0,(texh-8*lineh-2)/texh},       // left box
		{ofsx,ofsy+boxh,0, 0,0,0, 0,(texh-4*lineh-2)/texh},
		{ofsx+boxw,ofsy,0, 0,0,0, 16*lineh/texw,(texh-8*lineh-2)/texh},
		{ofsx+boxw,ofsy+boxh,0, 0,0,0, 16*lineh/texw,(texh-4*lineh-2)/texh}
	};
	WORD idx[nidx] = {0,2,1, 2,3,1, 4,6,5, 6,7,5};
	infoMesh.AddGroup (vtx, nvtx, idx, nidx, 0, 0, 0, 0, true);
#endif
}

void Pane::FreeResources ()
{
	int i;

	for (i = 0; i < 2; i++) gc->clbkReleaseFont (hudfont[i]);
	gc->clbkReleasePen (hudpen);
	for (i = 0; i < 6; i++) DeleteObject (hPen[i]);
	DeleteObject (hBrush1);
	DeleteObject (hBrush2);
	if (blinkmesh.tex) gc->clbkReleaseSurface (blinkmesh.tex);
}

void Pane::SetSketchpadDefault (oapi::Sketchpad *skp)
{
	skp->SetTextColor (RGB(0,255,0));
	skp->SetPen (hudpen);
	skp->SetFont (hudfont[0]);
}

bool Pane::GlobalToScreen (const Vector &glob, int &x, int &y) const
{
	D3DVECTOR homog;
	bool vis = GlobalToHomog (glob, homog);
	if (vis) {
		x = (int)(W*0.5*(1.0f+homog.x));
		y = (int)(H*0.5*(1.0f-homog.y));
	}
	return vis;
}

bool Pane::GlobalToScreen (const Vector &glob, double &x, double &y) const
{
	D3DVECTOR homog;
	bool vis = GlobalToHomog (glob, homog);
	if (vis) {
		x = W*0.5*(1.0f+homog.x);
		y = H*0.5*(1.0f-homog.y);
	}
	return vis;
}

bool Pane::GlobalToHomog (const Vector &glob, D3DVECTOR &homog) const
{
	//D3DVECTOR gpos = {-(D3DVALUE)glob.x, -(D3DVALUE)glob.y, -(D3DVALUE)glob.z};
	D3DVECTOR gpos = {(D3DVALUE)glob.x, (D3DVALUE)glob.y, (D3DVALUE)glob.z};
	return (D3DMath_VectorMatrixMultiply (homog, gpos, *g_camera->D3D_ProjViewMatrix()) == S_OK &&
		homog.x >= -1.0f && homog.x <= 1.0f &&
		homog.y >= -1.0f && homog.y <= 1.0f &&
		/* homog.z >=  0.0 && */ homog.z <= g_camera->HomogZlimit());
}

void Pane::ScreenToGlobal (int x, int y, Vector &glob) const
{
	D3DVECTOR homog, gpos;
	homog.x = (float)(x*2.0/W-1.0);
	homog.y = (float)(1.0-y*2.0/H);
	homog.z = 0.0f;
	D3DMATRIX IP;
	D3DMath_MatrixInvert (IP, *g_camera->D3D_ProjViewMatrix());
	D3DMath_VectorMatrixMultiply (gpos, homog, IP);
	//D3DMath_VectorTMatrixMultiply (gpos, homog, *g_camera->D3D_ProjViewMatrix());
	glob.Set (-gpos.x, -gpos.y, -gpos.z);
	glob.unify();
}

bool Pane::OpenMFD (INT_PTR id, int type, ifstream *ifs)
{
	if (id >= MAXMFD || id < 0) return ((ExternMFD*)id)->SetMode (type);
	if (panelmode == 1 && id >= 2) return false;
	if (panelmode == 2 && !mfd[id].exist) return false;

	if (mfd[id].instr && mfd[id].instr->Type() == type) return false; // nothing to do

	Instrument::Spec spec;
	switch (panelmode) {
	case 1:
		spec = defpanel->GetMFDSpec();
		break;
	case 2:
		if (panel) {
			panel->MFDSize (id, spec.w, spec.h);
			spec.nbtl  = mfd[id].prm.nbt1;
			spec.nbtr  = mfd[id].prm.nbt2;
			spec.bt_y0 = (int)(panel->GetScale()*mfd[id].prm.bt_yofs+0.5);
			spec.bt_dy = (int)(panel->GetScale()*mfd[id].prm.bt_ydist+0.5);
			spec.flag = mfd[id].prm.flag;
		} else if (panel2d) {
			spec = panel2d->GetMFDSpec(id);
		}
		break;
	case 3:
		spec.w = spec.h = mfd_vc_size;
		spec.nbtl  = mfd[id].prm.nbt1;
		spec.nbtr  = mfd[id].prm.nbt2;
		spec.bt_y0 = mfd[id].prm.bt_yofs;
		spec.bt_dy = mfd[id].prm.bt_ydist;
		spec.flag = mfd[id].prm.flag;
		break;
	}

	// force pow2 texture size for MFD displays
	if (panelmode != 3 && mfdsize_pow2) {
		int h = spec.h;
		spec.w = spec.h = (spec.w >= mfd_hires_threshold ? 512:256);
		spec.bt_y0 = (spec.bt_y0*spec.h)/h;
		spec.bt_dy = (spec.bt_dy*spec.h)/h;
	}

	// Try to create a new mode with this key
	Instrument *newinstr = 0;
	if (ifs || type != MFD_NONE) {
		newinstr = (ifs ? 
			Instrument::Create (*ifs, this, id, spec, g_focusobj) :
			Instrument::Create (type, this, id, spec, g_focusobj) );
		if (!newinstr) return false; // no mode found
	}

	if (mfd[id].instr) {
		mfd[id].lastmode = mfd[id].instr->Type();
		delete mfd[id].instr;
	}
	mfd[id].instr = newinstr;
	g_focusobj->MFDchanged (id, type);
	if (defpanel) defpanel->MFDModeChanged(id, type); //defpanel->RepaintMFDButtons (id);

	return true;
}

void Pane::ToggleMFD_on (int id)
{
	if (mfd[id].instr) CloseMFD (id);
	else {
		if (!OpenMFD (id, mfd[id].lastmode ? mfd[id].lastmode : MFD_ORBIT))
			OpenMFD(id, MFD_ORBIT);
	}
}

void Pane::RefreshMFD (int id)
{
	if (mfd[id].instr) {
		CloseMFD (id);
		OpenMFD (id, mfd[id].lastmode);
	}
}

double Pane::SetMFDRefreshIntervalMultiplier (int id, double multiplier)
{
	double prev = mfd[id].upDTscale;
	mfd[id].upDTscale = multiplier;
	return prev;
}

bool Pane::CloseMFD (int id)
{
	if (panelmode == 1 && id >= 2) return false;
	if (panelmode == 2 && !mfd[id].exist) return false;
	if (!mfd[id].instr) return false;

	mfd[id].lastmode = mfd[id].instr->Type();
	delete mfd[id].instr;
	mfd[id].instr = 0;
	g_focusobj->MFDchanged (id, MFD_NONE);
	return true;
}

void Pane::MFDModeDisabled (int mode)
{
	for (int i = 0; i < MAXMFD; i++)
		if (mfd[i].instr && mfd[i].instr->Type() == mode)
			CloseMFD (i);
}

int Pane::BroadcastMFDMessage (int mfdmode, int msg, void *data)
{
	int nproc = 0;
	for (int i = 0; i < MAXMFD; i++)
		if (mfd[i].instr && mfd[i].instr->Type() == mfdmode)
			if (mfd[i].instr->ProcessMessage (msg, data)) nproc++;
	for (DWORD j = 0; j < nemfd; j++)
		if (emfd[j]->Active() && emfd[j]->instr->Type() == mfdmode)
			if (emfd[j]->instr->ProcessMessage (msg, data)) nproc++;
	return nproc;
}

int Pane::BroadcastMFDMessage (int msg, void *data)
{
	int nproc = 0;
	for (int i = 0; i < MAXMFD; i++)
		if (mfd[i].instr)
			if (mfd[i].instr->ProcessMessage (msg, data)) nproc++;
	for (DWORD j = 0; j < nemfd; j++)
		if (emfd[j]->Active())
			if (emfd[j]->instr->ProcessMessage (msg, data)) nproc++;
	return nproc;
}

void Pane::OptionChanged(DWORD cat, DWORD item)
{
	if (cat == OPTCAT_INSTRUMENT) {
		if (item == OPTITEM_INSTRUMENT_MFDVCSIZE)
			mfd_vc_size = g_pOrbiter->Cfg()->CfgInstrumentPrm.VCMFDSize;
		for (size_t i = 0; i < nemfd; i++)
			if (emfd[i]->Active())
				emfd[i]->instr->OptionChanged(cat, item);
		if (defpanel)
			defpanel->OptionChanged(cat, item);
		if (vcockpit)
			vcockpit->OptionChanged(cat, item);
		for (size_t i = 0; i < MAXMFD; i++)
			if (mfd[i].instr) {
				RefreshMFD(i);
				mfd[i].instr->OptionChanged(cat, item);
			}
		if (item == OPTITEM_INSTRUMENT_PANELSCALE && panelmode == 2) {
			SetPanelMode(panelmode, true);
		}
	}
}

void Pane::RegisterMFD (int id, const MFDSPEC &spec)
{
	// obsolete
	mfd[id].exist = true;
	mfd[id].prm.nbt1     = spec.nbt_left;
	mfd[id].prm.nbt2     = spec.nbt_right;
	mfd[id].prm.bt_yofs  = spec.bt_yofs;
	mfd[id].prm.bt_ydist = spec.bt_ydist;
	if (panel) panel->RegisterMFD (id, spec);
}

void Pane::RegisterMFD (int id, const EXTMFDSPEC *spec)
{
	mfd[id].exist = true;
	memcpy (&mfd[id].prm, spec, sizeof (EXTMFDSPEC));
}

void Pane::UnregisterMFD (int id)
{
	mfd[id].exist = false;
	if (mfd[id].instr) {
		mfd[id].lastmode = mfd[id].instr->Type();
		mfd[id].active = true;
		delete mfd[id].instr;
		mfd[id].instr = 0;
	} else {
		mfd[id].active = false;
	}
}

void Pane::RegisterExternMFD (ExternMFD *mfd, const MFDSPEC &spec)
{
	ExternMFD **tmp = new ExternMFD*[nemfd+1]; TRACENEW
	if (nemfd) {
		memcpy (tmp, emfd, nemfd*sizeof(ExternMFD*));
		delete []emfd;
	}
	emfd = tmp;
	emfd[nemfd] = mfd;

//	// find a unique identifier
//	DWORD i;
//	emfd[nemfd]->id = maxMFD;
//	for (i = 0; i < nemfd; i++)
//		if (emfd[i]->id >= emfd[nemfd]->id)
//			emfd[nemfd]->id = emfd[i]->id+1;

	mfd->SetMode (MFD_ORBIT); // temporary!!!
	nemfd++;
}

bool Pane::UnregisterExternMFD (ExternMFD *mfd)
{
	DWORD i, j, k;
	for (i = 0; i < nemfd; i++) {
		if (emfd[i] == mfd) {
			delete emfd[i];
			ExternMFD **tmp;
			if (nemfd > 1) {
				tmp = new ExternMFD*[nemfd-1]; TRACENEW
				for (j = k = 0; j < nemfd; j++)
					if (j != i) tmp[k++] = emfd[j];
			} else {
				tmp = 0;
			}
			delete []emfd;
			emfd = tmp;
			nemfd--;
			return true;
		}
	}
	return false;
}

Instrument *Pane::MFD (int which)
{
	if (which >= 0 && which < MAXMFD)
		return mfd[which].instr;
	for (DWORD i = 0; i < nemfd; i++) {
		assert(false); // This code section doesn't seem to run. If it does that's trouble
		if (which == (INT_PTR)emfd[i]->Id())
			return emfd[i]->instr;
	}
	return NULL;
}

SURFHANDLE Pane::GetMFDSurface (int id)
{
	Instrument *instr = MFD(id);
	if (instr)
		return instr->Texture();
	else if (id < MAXMFD && mfd[id].prm.flag & MFD_TRANSPARENT_WHEN_OFF)
		return 0;
	else
		return mfdTex_blank;
}

void Pane::RegisterVCMFD (int id, const VCMFDSPEC *spec)
{
	// obsolete
	mfd[id].exist = true;
	EXTMFDSPEC &prm = mfd[id].prm;
	prm.nmesh = spec->nmesh;
	prm.ngroup = spec->ngroup;
	prm.flag = 0;
	prm.nbt1 = prm.nbt2 = 6;
	prm.bt_yofs = mfd_vc_size/6;
	prm.bt_ydist = mfd_vc_size/7;
}

void Pane::RegisterVCHUD (const VCHUDSPEC *spec)
{
	if (vcockpit) {
		vcockpit->CreateHUDSurface (spec, hudCol, hudIntens);
		if (hud) hud->Resize (true);
	}
}

void Pane::ShiftVC (const Vector &shift)
{
	if (vcockpit) {
		vcockpit->Shift (shift);
		if (hud) hud->Resize (true);
	}
}

const VCMFDSPEC *Pane::GetVCMFDParams (int id)
{
	if (mfd[id].exist) {
		static VCMFDSPEC spec[MAXMFD];
		spec[id].nmesh = mfd[id].prm.nmesh;
		spec[id].ngroup = mfd[id].prm.ngroup;
		return spec+id;
	} else return 0;
	//return (mfd[id].exist ? mfd[id].vcs : 0);
}

void Pane::RegisterVCArea (int id, const RECT &tgtrect, int draw_mode, int mouse_mode, int bkmode, SURFHANDLE tgt)
{
	if (vcockpit) vcockpit->DefineArea (id, tgtrect, draw_mode, mouse_mode, bkmode, tgt);
}

void Pane::SetVCAreaClickmode_Spherical (int aid, const Vector &cnt, double rad)
{
	if (vcockpit) {
		int id = vcockpit->AreaIndex (aid);
		if (id >= 0) vcockpit->SetClickZone_Spherical (id, cnt, rad);
	}
}

void Pane::SetVCAreaClickmode_Quadrilateral (int aid, const Vector &p1, const Vector &p2, const Vector &p3, const Vector &p4)
{
	if (vcockpit) {
		int id = vcockpit->AreaIndex (aid);
		if (id >= 0) vcockpit->SetClickZone_Quadrilateral (id, p1, p2, p3, p4);
	}
}

void Pane::GetVCAreaClickZones(std::list<VCClickZone>* p_List)
{
	if (vcockpit) vcockpit->GetClickZones(p_List);
}

Instrument::Spec Pane::GetVCMFDSpec ()
{
	Instrument::Spec spec;
	spec.w = spec.h = 256;
	spec.nbtl = spec.nbtr = 6;
	spec.bt_y0 = spec.h/6;
	spec.bt_dy = spec.h/7;
	return spec;
}

void Pane::RepaintMFDButtons (INT_PTR id, Instrument *instr)
{
	if (id < MAXMFD && id >= 0 && instr == mfd[id].instr) {
		if (panelmode >= 1) g_focusobj->MFDchanged (id, mfd[id].instr->Type());
		if (defpanel) defpanel->RepaintMFDButtons (id);
	} else {
		// check if external MFDs actually need to be repainted here
		for (DWORD i = 0; i < nemfd; i++) {
			if (emfd[i]->instr == instr) {
				emfd[i]->clbkRefreshButtons();
				break;
			}
		}
	}
}

void Pane::RegisterPanelBackground (HBITMAP hBmp, DWORD flag, DWORD ck)
{
	if (panel) panel->DefineBackground (hBmp, flag, ck);
	DeleteObject ((HGDIOBJ)hBmp);
}

void Pane::RegisterPanelBackground (SURFHANDLE hSurf, DWORD flag)
{
	// todo
}

void Pane::RegisterPanelArea (int aid, const RECT &pos, int draw_mode, int mouse_mode, int bkmode)
{
	/*if      (panel2d) panel2d->DefineArea (aid, pos, draw_mode, mouse_mode, bkmode);
	else*/ if (panel)   panel->DefineArea (aid, pos, draw_mode, mouse_mode, bkmode);
}

void Pane::SetPanelNeighbours (int left, int right, int top, int bottom)
{
	if      (panel2d) panel2d->SetConnections (left, right, top, bottom);
	else if (panel)   panel->SetConnections (left, right, top, bottom);
}

void Pane::SetVCNeighbours (int left, int right, int top, int bottom)
{
	if (vcockpit) vcockpit->SetConnections (left, right, top, bottom);
}

void Pane::InitState (const char *scn)
{
	ifstream ifs (scn);
	if (ifs) Read (ifs);
}

bool Pane::Read (ifstream &ifs)
{
	if (hud = HUD::Create (ifs, this, gc)) {
		hudmode = hud->Mode();
		if (g_focusobj) g_focusobj->HUDchanged (hudmode);
	}
	if (Panel::Read (ifs)) {
		SetPanelMode (2);
	} else if (VirtualCockpit::Read (ifs)) {
		SetPanelMode (3);
	} else {
		SetPanelMode (1);
	}
	for (int i = 0; i < MAXMFD; i++) OpenMFD (i, 0, &ifs);
	return true;
}

void Pane::Write (ostream &ofs) const
{
	if (hud) {
		hud->Write (ofs);
		ofs << endl;
	}
	for (int i = 0; i < MAXMFD; i++)
		if (mfd[i].instr) {
			mfd[i].instr->Write (ofs);
			ofs << endl;
		}
	if (panel2d) {
		panel2d->Write (ofs);
		ofs << endl;
	}
	if (panel) {
		panel->Write (ofs);
		ofs << endl;
	}
	if (vcockpit) {
		vcockpit->Write (ofs);
		ofs << endl;
	}
}

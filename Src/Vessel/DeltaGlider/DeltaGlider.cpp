// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ==============================================================
//                ORBITER MODULE: DeltaGlider
//                  Part of the ORBITER SDK
//
// DeltaGlider.cpp
// Reference implementation of delta glider vessel class module
// ==============================================================

#define ORBITER_MODULE

#include "DeltaGlider.h"
#include "HudCtrl.h"
#include "MainRetroSubsys.h"
#include "HoverSubsys.h"
#include "RcsSubsys.h"
#include "ScramSubsys.h"
#include "AerodynSubsys.h"
#include "GearSubsys.h"
#include "DockingSubsys.h"
#include "AvionicsSubsys.h"
#include "MfdSubsys.h"
#include "ScnEditorAPI.h"
#include "PressureSubsys.h"
#include "ThermalSubsys.h"
#include "LightSubsys.h"
#include "FailureSubsys.h"
#include "DlgCtrl.h"
#include "resource.h"
#include "meshres.h"
#include "meshres_vc.h"
#include "meshres_p0.h"
#include "DrawAPI.h"
#include <stdio.h>
#include <math.h>
#include <time.h>

using std::min;
using std::max;

// ==============================================================
// Global parameters
// ==============================================================

GDIParams g_Param;

static HELPCONTEXT g_hc = {
	(char*)"html/vessels/deltaglider.chm",
	0,
	(char*)"html/vessels/deltaglider.chm::/deltaglider.hhc",
	(char*)"html/vessels/deltaglider.chm::/deltaglider.hhk"
};

static const DWORD ntdvtx_geardown = 13;
static TOUCHDOWNVTX tdvtx_geardown[ntdvtx_geardown] = {
	{_V( 0   ,-2.57,10   ), 1e6, 1e5, 1.6, 0.1},
	{_V(-3.5 ,-2.57,-1   ), 1e6, 1e5, 3.0, 0.2},
	{_V( 3.5 ,-2.57,-1   ), 1e6, 1e5, 3.0, 0.2},
	{_V(-8.5 ,-0.3 ,-7.05), 1e7, 1e5, 3.0},
	{_V( 8.5 ,-0.3 ,-7.05), 1e7, 1e5, 3.0},
	{_V(-8.5 ,-0.4 ,-3   ), 1e7, 1e5, 3.0},
	{_V( 8.5 ,-0.4 ,-3   ), 1e7, 1e5, 3.0},
	{_V(-8.85, 2.3 ,-5.05), 1e7, 1e5, 3.0},
	{_V( 8.85, 2.3 ,-5.05), 1e7, 1e5, 3.0},
	{_V(-8.85, 2.3 ,-7.05), 1e7, 1e5, 3.0},
	{_V( 8.85, 2.3 ,-7.05), 1e7, 1e5, 3.0},
	{_V( 0   , 2   , 6.2 ), 1e7, 1e5, 3.0},
	{_V( 0   ,-0.6 ,10.65), 1e7, 1e5, 3.0}
};

static const DWORD ntdvtx_gearup = 13;
static TOUCHDOWNVTX tdvtx_gearup[ntdvtx_gearup] = {
	{_V( 0   ,-1.5 ,9),     1e7, 1e5, 3.0, 3.0},
	{_V(-6   ,-0.8 ,-5),    1e7, 1e5, 3.0, 3.0},
	{_V( 3   ,-1.2 ,-5),    1e7, 1e5, 3.0, 3.0},
	{_V(-8.5 ,-0.3 ,-7.05), 1e7, 1e5, 3.0},
	{_V( 8.5 ,-0.3 ,-7.05), 1e7, 1e5, 3.0},
	{_V(-8.5 ,-0.4 ,-3   ), 1e7, 1e5, 3.0},
	{_V( 8.5 ,-0.4 ,-3   ), 1e7, 1e5, 3.0},
	{_V(-8.85, 2.3 ,-5.05), 1e7, 1e5, 3.0},
	{_V( 8.85, 2.3 ,-5.05), 1e7, 1e5, 3.0},
	{_V(-8.85, 2.3 ,-7.05), 1e7, 1e5, 3.0},
	{_V( 8.85, 2.3 ,-7.05), 1e7, 1e5, 3.0},
	{_V( 0   , 2   , 6.2 ), 1e7, 1e5, 3.0},
	{_V( 0   ,-0.6 ,10.65), 1e7, 1e5, 3.0}
};

// ==============================================================
// Local prototypes

INT_PTR CALLBACK Ctrl_DlgProc (HWND, UINT, WPARAM, LPARAM);
void UpdateCtrlDialog (DeltaGlider *dg, HWND hWnd = 0);

//INT_PTR CALLBACK Damage_DlgProc (HWND, UINT, WPARAM, LPARAM);
//void UpdateDamageDialog (DeltaGlider *dg, HWND hWnd = 0);

// ==============================================================
// Airfoil coefficient functions
// Return lift, moment and zero-lift drag coefficients as a
// function of angle of attack (alpha or beta)
// ==============================================================

// 1. vertical lift component (wings and body)

void VLiftCoeff (VESSEL *v, double aoa, double M, double Re, void *context, double *cl, double *cm, double *cd)
{
	const int nabsc = 9;
	static const double AOA[nabsc] = {-180*RAD,-60*RAD,-30*RAD, -2*RAD, 15*RAD,20*RAD,25*RAD,60*RAD,180*RAD};
	static const double CL[nabsc]  = {       0,      0,   -0.4,      0,    0.7,     1,   0.8,     0,      0};
	static const double CM[nabsc]  = {       0,      0,  0.014, 0.0039, -0.006,-0.008,-0.010,     0,      0};
	int i;
	for (i = 0; i < nabsc-1 && AOA[i+1] < aoa; i++);
	if (i < nabsc - 1) {
		double f = (aoa - AOA[i]) / (AOA[i + 1] - AOA[i]);
		*cl = CL[i] + (CL[i + 1] - CL[i]) * f;  // aoa-dependent lift coefficient
		*cm = CM[i] + (CM[i + 1] - CM[i]) * f;  // aoa-dependent moment coefficient
	}
	else {
		*cl = CL[nabsc - 1];
		*cm = CM[nabsc - 1];
	}
	double saoa = sin(aoa);
	double pd = 0.015 + 0.4*saoa*saoa;  // profile drag
	*cd = pd + oapiGetInducedDrag (*cl, 1.5, 0.7) + oapiGetWaveDrag (M, 0.75, 1.0, 1.1, 0.04);
	// profile drag + (lift-)induced drag + transonic/supersonic wave (compressibility) drag
}

// 2. horizontal lift component (vertical stabilisers and body)

void HLiftCoeff (VESSEL *v, double beta, double M, double Re, void *context, double *cl, double *cm, double *cd)
{
	int i;
	const int nabsc = 8;
	static const double BETA[nabsc] = {-180*RAD,-135*RAD,-90*RAD,-45*RAD,45*RAD,90*RAD,135*RAD,180*RAD};
	static const double CL[nabsc]   = {       0,    +0.3,      0,   -0.3,  +0.3,     0,   -0.3,      0};
	for (i = 0; i < nabsc-1 && BETA[i+1] < beta; i++);
	if (i < nabsc - 1) {
		*cl = CL[i] + (CL[i + 1] - CL[i]) * (beta - BETA[i]) / (BETA[i + 1] - BETA[i]);
	}
	else {
		*cl = CL[nabsc - 1];
	}
	*cm = 0.0;
	*cd = 0.015 + oapiGetInducedDrag (*cl, 1.5, 0.6) + oapiGetWaveDrag (M, 0.75, 1.0, 1.1, 0.04);
}

// ==============================================================
// Specialised vessel class DeltaGlider
// ==============================================================

// --------------------------------------------------------------
// Constructor
// --------------------------------------------------------------
DeltaGlider::DeltaGlider (OBJHANDLE hObj, int fmodel)
: ComponentVessel (hObj, fmodel)
{
	int i;

	oapiWriteLogVerbose("[DG] Constructor");

	modelidx = (fmodel ? 1 : 0);

	// Subsystem definitions
	AddSubsystem (ssys_mainretro    = new MainRetroSubsystem (this));
	AddSubsystem (ssys_hoverctrl    = new HoverSubsystem (this));
	AddSubsystem (ssys_rcs          = new RcsSubsystem (this));
	AddSubsystem (ssys_aerodyn      = new AerodynCtrlSubsystem (this));
	AddSubsystem (ssys_gear         = new GearSubsystem (this));
	AddSubsystem (ssys_hud          = new HUDControl (this));
	AddSubsystem (ssys_pressurectrl = new PressureSubsystem (this));
	AddSubsystem (ssys_thermal      = new ThermalSubsystem (this));
	AddSubsystem (ssys_docking      = new DockingCtrlSubsystem (this));
	AddSubsystem (ssys_light        = new LightCtrlSubsystem (this));
	AddSubsystem (ssys_avionics     = new AvionicsSubsystem (this));
	AddSubsystem (ssys_failure      = new FailureSubsystem (this));
	for (i = 0; i < 2; i++)
		AddSubsystem (ssys_mfd[i] = new MfdSubsystem (this, MFD_LEFT+i));
	ssys_scram = 0; // creation deferred to clbkSetClassCaps

	visual            = NULL;
	exmesh            = NULL;
	vcmesh            = NULL;
	vcmesh_tpl        = NULL;
	insignia_tex      = NULL;
	contrail_tex      = NULL;
	hPanelMesh        = NULL;
	panelcol          = 0;
	campos            = CAM_GENERIC;
	th_main_level     = 0.0;
	skinpath[0] = '\0';
	for (i = 0; i < 3; i++)
		skin[i] = 0;
	for (i = 0; i < 4; i++)
		psngr[i] = false;
	for (i = 0; i < 2; i++) {
		scflowidx[i] = 0;
		mainflowidx[i] = retroflowidx[i] = -1;
		scTSFCidx[i] = -1;
	}

	mainTSFCidx = hoverflowidx = -1;
	
	// damage parameters
	bDamageEnabled = (GetDamageModel() != 0);
	lwingstatus = rwingstatus = 1.0;
	for (i = 0; i < 4; i++) aileronfail[i] = false;

	DefineAnimations();
}

// --------------------------------------------------------------
// Destructor
// --------------------------------------------------------------
DeltaGlider::~DeltaGlider ()
{
	oapiWriteLogVerbose("[DG] Destructor");
	DWORD i;

	if (insignia_tex) oapiDestroySurface(insignia_tex);

	for (i = 0; i < 3; i++)
		if (skin[i]) oapiReleaseTexture (skin[i]);
}

// --------------------------------------------------------------
// Set vessel mass excluding propellants
// --------------------------------------------------------------
void DeltaGlider::SetEmptyMass () const
{
	double emass = (ssys_scram ? EMPTY_MASS_SC : EMPTY_MASS);
	// add passengers+life support to empty vessel mass
	for (int i = 0; i < 4; i++)
		if (psngr[i]) emass += PSNGR_MASS;

	ComponentVessel::SetEmptyMass (emass);
}

// --------------------------------------------------------------
// Define animation sequences for moving parts
// --------------------------------------------------------------
void DeltaGlider::DefineAnimations ()
{
	// ***** Rudder animation *****
	static UINT RRudderGrp[2] = {GRP_RRudder1,GRP_RRudder2};
	static MGROUP_ROTATE RRudder (0, RRudderGrp, 2,
		_V( 8.668,0.958,-6.204), _V( 0.143,0.975,-0.172), (float)(-60*RAD));
	static UINT LRudderGrp[2] = {GRP_LRudder1,GRP_LRudder2};
	static MGROUP_ROTATE LRudder (0, LRudderGrp, 2,
		_V(-8.668,0.958,-6.204), _V(-0.143,0.975,-0.172), (float)(-60*RAD));
	anim_rudder = CreateAnimation (0.5);
	AddAnimationComponent (anim_rudder, 0, 1, &RRudder);
	AddAnimationComponent (anim_rudder, 0, 1, &LRudder);

	// ***** Elevator animation *****
	static UINT ElevatorGrp[8] = {GRP_LUAileron1,GRP_LUAileron2,GRP_LLAileron1,GRP_LLAileron2,GRP_RUAileron1,GRP_RUAileron2,GRP_RLAileron1,GRP_RLAileron2};
	static MGROUP_ROTATE Elevator (0, ElevatorGrp, 8,
		_V(0,-0.4,-6.0), _V(1,0,0), (float)(40*RAD));
	anim_elevator = CreateAnimation (0.5);
	AddAnimationComponent (anim_elevator, 0, 1, &Elevator);

	// ***** Elevator trim animation *****
	static MGROUP_ROTATE ElevatorTrim (0, ElevatorGrp, 8,
		_V(0,-0.4,-6.0), _V(1,0,0), (float)(10*RAD));
	anim_elevatortrim = CreateAnimation (0.5);
	AddAnimationComponent (anim_elevatortrim, 0, 1, &ElevatorTrim);

	// ***** Aileron animation *****
	static UINT LAileronGrp[4] = {GRP_LUAileron1,GRP_LUAileron2,GRP_LLAileron1,GRP_LLAileron2};
	static MGROUP_ROTATE LAileron (0, LAileronGrp, 4,
		_V(0,-0.4,-6.0), _V(1,0,0), (float)(-20*RAD));
	anim_laileron = CreateAnimation (0.5);
	AddAnimationComponent (anim_laileron, 0, 1, &LAileron);

	static UINT RAileronGrp[4] = {GRP_RUAileron1,GRP_RUAileron2,GRP_RLAileron1,GRP_RLAileron2};
	static MGROUP_ROTATE RAileron (0, RAileronGrp, 4,
		_V(0,-0.4,-6.0), _V(1,0,0), (float)(20*RAD));
	anim_raileron = CreateAnimation (0.5);
	AddAnimationComponent (anim_raileron, 0, 1, &RAileron);
}

// --------------------------------------------------------------
// Apply custom skin to the current mesh instance
// --------------------------------------------------------------
void DeltaGlider::ApplySkin ()
{
	if (!exmesh) return;
	if (skin[0]) oapiSetTexture (exmesh, 2, skin[0]);
	if (skin[1]) oapiSetTexture (exmesh, 3, skin[1]);
	oapiSetTexture (exmesh, 5, insignia_tex);

	static const MATERIAL panelmat[5] = {
		{{0.5,0.6,0.6,1},{0.5,0.6,0.6,1},{0.2,0.2,0.2,1},{0.15,0.15,0.15,1},10}, // default
		{{0.6,0.55,0.45,1},{0.6,0.55,0.45,1},{0.1,0.1,0.1,1},{0.15,0.15,0.15,1},5}, // brown
		{{0.2,0.2,0.15,1},{0.5,0.5,0.5,1},{0.6,0.6,0.6,1},{0.15,0.15,0.15,1},20}, // shiny anthrazit
		{{0.75,0.75,0.65,1},{0.75,0.75,0.65,1},{0.3,0.3,0.3,1},{0.15,0.15,0.15,1},20}, // shiny anthrazit
		{{0.5,0.1,0.1,1},{0.5,0.1,0.1,1},{0.2,0.2,0.2,1},{0.15,0.15,0.15,1},10} // DG-red
	};
	if (panelcol > 0 && panelcol <= 4)
		oapiSetMaterial (vcmesh, 14, panelmat+panelcol);

	time_t lt; time(&lt); struct tm *st = localtime(&lt);
	if (vcmesh && st->tm_mon==3 && st->tm_mday==1) {
		SURFHANDLE t = oapiLoadTexture ("generic\\noisep.dds");
		if (t) oapiSetTexture (vcmesh, 17, t);
	}
}

// --------------------------------------------------------------
// Paint individual vessel markings
// --------------------------------------------------------------
void DeltaGlider::PaintMarkings (SURFHANDLE tex)
{
	oapi::Sketchpad *skp = oapiGetSketchpad (tex);
	if (skp) {
		oapi::Font *font1 = oapiCreateFont(38, true, (char*)"Sans", FONT_BOLD);
		skp->SetFont (font1);
		skp->SetTextColor (0xD0D0D0);
		skp->SetTextAlign (oapi::Sketchpad::CENTER);
		char cbuf[32];
		strncpy (cbuf, GetName(), 10);
		int len = min(strlen(GetName()), (size_t)10);
		skp->Text (193, 10, cbuf, len);
		skp->Text (193, 74, cbuf, len);
		oapiReleaseFont(font1);
		oapi::Font *font2 = oapiCreateFont(36, true, (char*)"Sans", FONT_BOLD);
		skp->SetFont(font2);
		skp->SetTextColor (0x808080);
		skp->SetTextAlign (oapi::Sketchpad::RIGHT);
		skp->Text (125, 156, cbuf, len);
		skp->SetTextAlign (oapi::Sketchpad::LEFT);
		skp->Text (131, 156, cbuf, len);
		oapiReleaseSketchpad (skp);
		oapiReleaseFont(font2);
	}
}

// --------------------------------------------------------------
// Load panel animation bitmaps and initialise panel state
// --------------------------------------------------------------
void DeltaGlider::InitPanel (int panel)
{
	ComponentVessel::clbkReset2D (panel, hPanelMesh);

	switch (panel) {
	case 0: // main panel
		for (DWORD i = 0; i < 2; i++)
			mainflowidx[i] = retroflowidx[i] = scTSFCidx[i] = scflowidx[i] = -1;
		hoverflowidx = mainTSFCidx = -1;
		break;
	case 1: // overhead panel
		break;
	}
}

// --------------------------------------------------------------
// Load VC animation bitmaps and initialise VC state
// --------------------------------------------------------------
void DeltaGlider::InitVC (int vc)
{
	int i;

	switch (vc) {
	case 0:
		// reset state flags for panel instruments
		for (i = 0; i < 2; i++)
			mainflowidx[i] = retroflowidx[i] = scTSFCidx[i] = scflowidx[i] = -1;
		hoverflowidx = mainTSFCidx = -1;
		break;
	}
}

bool DeltaGlider::clbkDrawHUD (int mode, const HUDPAINTSPEC *hps, oapi::Sketchpad *skp)
{
	if (oapiCockpitMode() != COCKPIT_VIRTUAL) return false;

	// Default HUD elements and subsystem HUD drawing
	ComponentVessel::clbkDrawHUD (mode, hps, skp);

	int cx = hps->CX, cy = hps->CY;

	// show RCS mode
	switch (GetAttitudeMode()) {
	case RCS_ROT:
		skp->Text (0, hps->H-20, "RCS ROT", 7);
		break;
	case RCS_LIN:
		skp->Text (0, hps->H-20, "RCS_LIN", 7);
		break;
	}

	// show airbrake status
	if (!ssys_aerodyn->AirbrakeState().IsClosed()) {
		double tmp;
		if (!ssys_aerodyn->AirbrakeState().IsActive() || modf(oapiGetSimTime(), &tmp) < 0.5) {
			skp->SetTextAlign(oapi::Sketchpad::CENTER, oapi::Sketchpad::BOTTOM);
			skp->Text(hps->CX, hps->CY - hps->Markersize/2, "AIRBRK", 6);
		}
	}


	if (oapiGetHUDMode() == HUD_DOCKING) {
		if (!ssys_docking->NconeState().IsOpen()) {
			int d = hps->Markersize*5;
			double tmp;
			if (ssys_docking->NconeState().IsClosed() || modf (oapiGetSimTime(), &tmp) < 0.5) {
				skp->Line (cx-d,cy-d,cx+d,cy+d);
				skp->Line (cx-d,cy+d,cx+d,cy-d);
			}
			const char *str = "NOSECONE";
			int w = skp->GetTextWidth (str);
			skp->Text (cx-w/2, cy-d, str, 8);
		}
	}
	return true;
}

void DeltaGlider::clbkRenderHUD (int mode, const HUDPAINTSPEC *hps, SURFHANDLE hTex)
{
	ComponentVessel::clbkRenderHUD (mode, hps, hTex);

	static float texw = 512.0f, texh = 256.0f;
	float cx = (float)hps->CX, cy = (float)hps->CY;
	DWORD i, nvtx = 0, nidx = 0;
	static NTVERTEX vtx[12+16+4];
	static WORD idx[18+36+6];
	static float scl = 0;
	static NTVERTEX vgear[12];
	static NTVERTEX vnose[16];
	static NTVERTEX vbrk[4];
	static WORD igear[18] = {
		0,3,1, 3,0,2,
		4,7,5, 7,4,6,
		8,11,9, 11,8,10
	};
	static WORD inose[36] = {
		0,1,2, 2,3,0,
		0,6,1, 6,7,1,
		1,8,2, 8,9,2,
		2,10,3, 10,11,3,
		3,4,0, 0,4,5,
		12,15,13, 12,14,15
	};
	static WORD ibrk[6] = {
		0,3,1, 0,2,3
	};

	if (scl != hps->Markersize*0.25f) { // resize
		scl = hps->Markersize*0.25f;
		memset (vgear, 0, 12*sizeof(NTVERTEX));
		float x[12] = {-4,-2,-4,-2,2,4,2,4,-1,1,-1,1};
		float y[12] = {-2,-2,-4,-4,-2,-2,-4,-4,-6,-6,-8,-8};
		for (i = 0; i < 12; i++) {
			vgear[i].x = cx + x[i]*scl;
			vgear[i].y = cy + y[i]*scl;
			vgear[i].tu = (405.0f + (18.0f * (i%2)))/texw;
			vgear[i].tv = (104.0f - (18.0f * ((i%4)/2)))/texh;
		}
		memset (vnose, 0, 16*sizeof(NTVERTEX));
		float xn[16] = {0,1,0,-1,-31,-30,30,31,31,30,-30,-31,  -13,13,-13,13};
		float yn[16] = {-1,0,1,0,-30,-31,-31,-30,30,31,31,30,  -25,-25,-28.9,-28.9};
		float un[16] = {392.5f, 397.0f, 392.5f, 388.0f, 388.0f, 392.5f, 392.5f, 397.0f, 397.0f, 392.5f, 392.5f, 388.0f,    124.0f, 204.0f, 124.0f, 204.0f};
		float vn[16] = {92.0f, 96.5f, 101.0f, 96.5f, 96.5f, 92.0f, 92.0f, 96.5f, 96.5f, 101.0f, 101.0f, 96.5f,             118.0f, 118.0f, 106.0f, 106.0f};
		for (i = 0; i < 16; i++) {
			vnose[i].x = cx + xn[i]*scl*0.4f;
			vnose[i].y = cy + yn[i]*scl*0.4f;
			vnose[i].tu = un[i]/texw;
			vnose[i].tv = vn[i]/texh;
		}
		memset (vbrk, 0, 4*sizeof(NTVERTEX));
		float xb[4] = {-9.1f, 9.1f, -9.1f, 9.1f};
		float yb[4] = {-30.0f, -30.0f, -33.9f, -33.9f};
		float ub[4] = {205.0f, 261.0f, 205.0f, 261.0f};
		float vb[4] = {118.0f, 118.0f, 106.0f, 106.0f};
		for (i = 0; i < 4; i++) {
			vbrk[i].x = cx + xb[i]*scl*0.4f;
			vbrk[i].y = cy + yb[i]*scl*0.4f;
			vbrk[i].tu = ub[i]/texw;
			vbrk[i].tv = vb[i]/texh;
		}
	}

	// show gear deployment status
	if (ssys_gear->GearState().IsOpen() || (!ssys_gear->GearState().IsClosed() && fmod (oapiGetSimTime(), 1.0) < 0.5)) {
		memcpy (vtx+nvtx, vgear, 12*sizeof(NTVERTEX));
		for (i = 0; i < 18; i++) idx[nidx+i] = igear[i]+nvtx;
		nvtx += 12;
		nidx += 18;
	}

	// show nosecone status
	if (oapiGetHUDMode() == HUD_DOCKING && !ssys_docking->NconeState().IsOpen()) {
		double tmp;
		if (ssys_docking->NconeState().IsClosed() || modf (oapiGetSimTime(), &tmp) < 0.5) {
			memcpy (vtx+nvtx, vnose, 16*sizeof(NTVERTEX));
			for (i = 0; i < 36; i++) idx[nidx+i] = inose[i]+nvtx;
			nvtx += 16;
			nidx += 36;
		}
	}

	// show airbrake status
	if (!ssys_aerodyn->AirbrakeState().IsClosed()) {
		double tmp;
		if (!ssys_aerodyn->AirbrakeState().IsActive() || modf (oapiGetSimTime(), &tmp) < 0.5) {
			memcpy (vtx+nvtx, vbrk, 4*sizeof(NTVERTEX));
			for (i = 0; i < 6; i++) idx[nidx+i] = ibrk[i]+nvtx;
			nvtx += 4;
			nidx += 6;
		}
	}

	if (nvtx) {
		MESHGROUP grp = {vtx, idx, nvtx, nidx, 0, 0, 0, 0, 0};
		MESHHANDLE hmesh = oapiCreateMesh (1, &grp);
		oapiRenderHUD (hmesh, &hTex);
		oapiDeleteMesh (hmesh);
	}
}

void DeltaGlider::SetGearParameters (double state)
{
	if (state == 1.0) {
		if (!bGearIsDown) {
			SetTouchdownPoints (tdvtx_geardown, ntdvtx_geardown);
			SetNosewheelSteering (true);
			bGearIsDown = true;
		}
	} else {
		if (bGearIsDown) {
			SetTouchdownPoints (tdvtx_gearup, ntdvtx_gearup);
			SetNosewheelSteering (false);
			bGearIsDown = false;
		}
	}
}

void DeltaGlider::SetMainRetroLevel (int which, double lmain, double lretro)
{
	if (which == 2) {   // set main thruster group
		SetThrusterGroupLevel (THGROUP_MAIN,  lmain);
		SetThrusterGroupLevel (THGROUP_RETRO, lretro);
	} else {            // set individual engine
		SetThrusterLevel (th_main [which], lmain);
		SetThrusterLevel (th_retro[which], lretro);
	}
}

void DeltaGlider::EnableRetroThrusters (bool state)
{
	for (int i = 0; i < 2; i++)
		SetThrusterResource (th_retro[i], state ? ph_main : NULL);
}

double DeltaGlider::GetMaxHoverThrust () const
{
	double th0 = 0.0;
	for (int i = 0; i < 3; i++)
		th0 += GetThrusterMax (th_hover[i]);
	return th0;
}

void DeltaGlider::TestDamage ()
{
	bool newdamage = false;
	double dt = oapiGetSimStep();

	// airframe damage as a result of wingload stress
	// or excessive dynamic pressure

	double load = GetLift() / 190.0; // L/S
	double dynp = GetDynPressure();  // dynamic pressure
	if (load > WINGLOAD_MAX || load < WINGLOAD_MIN || dynp > DYNP_MAX) {
		double alpha = max ((dynp-DYNP_MAX) * 1e-5,
			(load > 0 ? load-WINGLOAD_MAX : WINGLOAD_MIN-load) * 5e-5);
		double p = 1.0 - exp (-alpha*dt); // probability of failure
		if (oapiRand() < p) {
			// simulate structural failure by distorting the airfoil definition
			int rfail = rand();
			switch (rfail & 3) {
			case 0: // fail left wing
				lwingstatus *= exp (-alpha*oapiRand());
				break;
			case 1: // fail right wing
				rwingstatus *= exp (-alpha*oapiRand());
				break;
			case 2: { // fail left aileron
				if (hlaileron) {
					DelControlSurface (hlaileron);
					hlaileron = NULL;
				}
				aileronfail[rfail&4?0:1] = true;
				} break;
			case 3: { // fail right aileron
				if (hraileron) {
					DelControlSurface (hraileron);
					hraileron = NULL;
				}
				aileronfail[rfail&4?2:3] = true;
				} break;
			}
			newdamage = true;
		}
	}

	if (newdamage) {
		ssys_failure->MWSActivate();
		ApplyDamage ();
		//UpdateDamageDialog (this);
	}
}

void DeltaGlider::ApplyDamage ()
{
	double balance = (rwingstatus-lwingstatus)*3.0;
	double surf    = (rwingstatus+lwingstatus)*35.0 + 20.0;
	EditAirfoil (hwing, 0x09, _V(balance,0,-0.3), 0, 0, surf, 0);

	if (rwingstatus < 1 || lwingstatus < 1)
		ssys_failure->MWSActivate();

	SetDamageVisuals();
}

void DeltaGlider::RepairDamage ()
{
	int i;
	lwingstatus = rwingstatus = 1.0;
	EditAirfoil (hwing, 0x09, _V(0,0,-0.3), 0, 0, 90.0, 0);
	if (!hlaileron)
		hlaileron = CreateControlSurface2 (AIRCTRL_AILERON, 0.3, 1.5, _V( 7.5,0,-7.2), AIRCTRL_AXIS_XPOS, anim_raileron);
	if (!hraileron)
		hraileron = CreateControlSurface2 (AIRCTRL_AILERON, 0.3, 1.5, _V(-7.5,0,-7.2), AIRCTRL_AXIS_XNEG, anim_laileron);
	for (i = 0; i < 4; i++)
		aileronfail[i] = false;
	ssys_pressurectrl->RepairDamage ();
	ssys_failure->MWSReset ();
	//UpdateDamageDialog (this);
	SetDamageVisuals();
}

bool DeltaGlider::RedrawPanel_ScramFlow (SURFHANDLE surf)
{
	bool redraw = false;
	for (int i = 0; i < 2; i++) {
		int p = min (66, (int)(ssys_scram->DMF(i)/3.0*67.0));
		if (p != scflowidx[i])
			scflowidx[i] = p, redraw = true;
	}
	if (redraw) {
		oapiBltPanelAreaBackground (AID_SCRAMDISP2, surf);
		return true;
		//return RedrawPanel_IndicatorPair (surf, scflowidx, 66);
	} else return false;
}

bool DeltaGlider::RedrawPanel_ScramTempDisp (SURFHANDLE surf)
{
	double T, phi;
	static const double rad = 19.0;
	int i, j, x0, y0, dx, dy;
	bool isVC = (oapiCockpitMode() == COCKPIT_VIRTUAL);

	oapi::Sketchpad *pSkp = oapiGetSketchpad(surf);
	pSkp->SetPen(g_Param.pen[0]);
	for (j = 0; j < 3; j++) {
		for (i = 0; i < 2; i++) {
			T = ssys_scram->Temp (i, j);
			phi = PI * min (T,3900.0)/2000.0;
			dx = (int)(rad*sin(phi)), dy = (int)(rad*cos(phi));
			x0 = (isVC ? 20 : 22-j) + i*43;
			y0 = 19+j*46;
			pSkp->MoveTo (x0, y0); pSkp->LineTo (x0+dx, y0-dy);
		}
	}
	oapiReleaseSketchpad (pSkp);
	return true;
}

// D. Beachy: begin refactored section to fix flow rate panels
bool DeltaGlider::RedrawPanel_MainFlow (SURFHANDLE surf)
{
	double gaugeSize = 66.99;  // pointer can move 66 pixels; also round up to next pixel
	bool redraw = false;
	for (int i = 0; i < 2; i++) {
		double flowRate = GetThrusterFlowRate(th_main[i]);
        int p = (int)min(flowRate*gaugeSize/5.1, gaugeSize);  // gauge maxes out at 5.1
		if (p != mainflowidx[i])
			mainflowidx[i] = p, redraw = true;
	}
	if (redraw) {
		oapiBltPanelAreaBackground (AID_MAINDISP1, surf);
		return true;
		//return RedrawPanel_IndicatorPair (surf, mainflowidx, 66);
	} else return false;
}

bool DeltaGlider::RedrawPanel_RetroFlow (SURFHANDLE surf)
{
    double gaugeSize = 66.99;  // pointer can move 66 pixels; also round up to next pixel
	bool redraw = false;
	for (int i = 0; i < 2; i++) {
		double flowRate = GetThrusterFlowRate(th_retro[i]); 
		int p = (int)min(flowRate*gaugeSize/0.9,gaugeSize); // gauge maxes out at 0.9
		if (p != retroflowidx[i])
			retroflowidx[i] = p, redraw = true;
	}
	if (redraw) {
		oapiBltPanelAreaBackground (AID_MAINDISP2, surf);
		return true;
		//return RedrawPanel_IndicatorPair (surf, retroflowidx, 66);
	} else return false;
}

bool DeltaGlider::RedrawPanel_HoverFlow (SURFHANDLE surf)
{
    double gaugeSize = 66.99;  // pointer can move 66 pixels; also round up to next pixel
    // since hover flow rates are always locked we can assume the second hover thruster has the same flow as the first
    double flowRate = GetThrusterFlowRate(th_hover[0]); 
	int p = (int)min(flowRate*gaugeSize/3.6,gaugeSize); // gauge maxes out at 3.6
	if (p != hoverflowidx) {
		hoverflowidx = p;
		oapiBltPanelAreaBackground (AID_MAINDISP3, surf);
		// draw a pair of matching indicators
		//oapiBlt (surf, srf[8], 0, 66-hoverflowidx, 0, 0, 6, 7, SURF_PREDEF_CK);
		//oapiBlt (surf, srf[8], 7, 66-hoverflowidx, 6, 0, 6, 7, SURF_PREDEF_CK);
		return true;
	} else return false;
}

double DeltaGlider::GetThrusterFlowRate(THRUSTER_HANDLE th)
{
    double level  = GetThrusterLevel(th); // throttle level
    double isp    = GetThrusterIsp0(th);
    double thrust = GetThrusterMax0(th);
    double flow   = thrust*level/isp;
    
    return flow;
}
// D. Beachy: end refactored section

void DeltaGlider::UpdateStatusIndicators ()
{
	if (!vcmesh) return;
	float x, xon = 0.845f, xoff = 0.998f;
	double d;

	static NTVERTEX vtx[16];
	static WORD vidx[16] = {0,1,4,5,20,21,8,9,24,25,16,17,12,13,28,29};
	GROUPEDITSPEC ges;
	ges.flags = GRPEDIT_VTXTEXU;
	ges.nVtx = 16;
	ges.vIdx = vidx;
	ges.Vtx = vtx;

	// gear indicator
	x = (ssys_gear->GearState().IsClosed() ? xoff : ssys_gear->GearState().IsOpen() ? xon : modf (oapiGetSimTime(), &d) < 0.5 ? xon : xoff);
	vtx[0].tu = vtx[1].tu = x;

	// retro cover indicator
	x = (ssys_mainretro->RetroCoverState().IsClosed() ? xoff : ssys_mainretro->RetroCoverState().IsOpen() ? xon : modf (oapiGetSimTime(), &d) < 0.5 ? xon : xoff);
	vtx[2].tu = vtx[3].tu = x;

	// airbrake indicator
	x = (ssys_aerodyn->AirbrakeState().IsClosed() ? xoff : ssys_aerodyn->AirbrakeState().IsActive() ? (modf(oapiGetSimTime(), &d) < 0.5 ? xon : xoff) : xon);
	vtx[4].tu = vtx[5].tu = x;

	// nose cone indicator
	x = (ssys_docking->NconeState().IsClosed() ? xoff : ssys_docking->NconeState().IsOpen() ? xon : modf (oapiGetSimTime(), &d) < 0.5 ? xon : xoff);
	vtx[6].tu = vtx[7].tu = x;

	// top hatch indicator
	x = (ssys_pressurectrl->HatchState().IsClosed() ? xoff : ssys_pressurectrl->HatchState().IsOpen() ? xon : modf (oapiGetSimTime(), &d) < 0.5 ? xon : xoff);
	vtx[8].tu = vtx[9].tu = x;

	// radiator indicator
	x = (ssys_thermal->RadiatorState().IsClosed() ? xoff : ssys_thermal->RadiatorState().IsOpen() ? xon : modf (oapiGetSimTime(), &d) < 0.5 ? xon : xoff);
	vtx[10].tu = vtx[11].tu = x;

	// outer airlock indicator
	x = (ssys_pressurectrl->OLockState().IsClosed() ? xoff : ssys_pressurectrl->OLockState().IsOpen() ? xon : modf (oapiGetSimTime(), &d) < 0.5 ? xon : xoff);
	vtx[12].tu = vtx[13].tu = x;

	// inner airlock indicator
	x = (ssys_pressurectrl->ILockState().IsClosed() ? xoff : ssys_pressurectrl->ILockState().IsOpen() ? xon : modf (oapiGetSimTime(), &d) < 0.5 ? xon : xoff);
	vtx[14].tu = vtx[15].tu = x;

	oapiEditMeshGroup (vcmesh, GRP_STATUS_INDICATOR_VC, &ges);
}

void DeltaGlider::SetPassengerVisuals ()
{
	if (!(vcmesh && exmesh)) return;
	GROUPEDITSPEC ges;

	static int expsngridx[4] = {GRP_Psngr1, GRP_Psngr2, GRP_Psngr3, GRP_Psngr4};
	static int exvisoridx[4] = {GRP_Visor1, GRP_Visor2, GRP_Visor3, GRP_Visor4};
	static int vcpsngridx[4] = {GRP_PASSENGER1_VC, GRP_PASSENGER2_VC, GRP_PASSENGER3_VC, GRP_PASSENGER4_VC};
	static int vcvisoridx[4] = {GRP_PASSENGER1_VISOR_VC, GRP_PASSENGER2_VISOR_VC, GRP_PASSENGER3_VISOR_VC, GRP_PASSENGER4_VISOR_VC};

	for (DWORD i = 0; i < 4; i++) {
		if (psngr[i]) {
			ges.flags = GRPEDIT_SETUSERFLAG;
			ges.UsrFlag = 1;
		} else {
			ges.flags = GRPEDIT_ADDUSERFLAG;
			ges.UsrFlag = 3;
		}
		oapiEditMeshGroup (exmesh, expsngridx[i], &ges);
		oapiEditMeshGroup (exmesh, exvisoridx[i], &ges);
		oapiEditMeshGroup (vcmesh, vcpsngridx[i], &ges);
		oapiEditMeshGroup (vcmesh, vcvisoridx[i], &ges);
	}
}

static UINT AileronGrp[8] = {GRP_RUAileron1,GRP_LUAileron1,GRP_LUAileron2,GRP_RUAileron2,GRP_LLAileron1,GRP_RLAileron1,GRP_LLAileron2,GRP_RLAileron2};

void DeltaGlider::SetDamageVisuals ()
{
	if (!exmesh) return;
	GROUPEDITSPEC ges;

	int i, j;

	// ailerons
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 2; j++)
			if (aileronfail[i]) {
				ges.flags = GRPEDIT_ADDUSERFLAG;
				ges.UsrFlag = 3;
				oapiEditMeshGroup (exmesh, AileronGrp[i*2+j], &ges);
			} else {
				ges.flags = GRPEDIT_SETUSERFLAG;
				ges.UsrFlag = 0;
				oapiEditMeshGroup (exmesh, AileronGrp[i*2+j], &ges);
			}
	}
}

/* Not it use 
void DeltaGlider::DrawNeedle (oapi::Sketchpad *pSkp, int x, int y, double rad, double angle, double *pangle, double vdial)
{
	if (pangle) { // needle response delay
		double dt = oapiGetSimStep();
		if (fabs (angle - *pangle)/dt >= vdial)
			angle = (angle > *pangle ? *pangle+vdial*dt : *pangle-vdial*dt);
		*pangle = angle;
	}
	double dx = rad * cos(angle), dy = rad * sin(angle);
	pSkp->SetPen (g_Param.pen[1]);
	pSkp->MoveTo (x, y); pSkp->LineTo (x + (int)(0.85*dx+0.5), y - (int)(0.85*dy+0.5));
	pSkp->SetPen (g_Param.pen[0]);
	pSkp->MoveTo (x, y); pSkp->LineTo (x + (int)(dx+0.5), y - (int)(dy+0.5));
}*/

void DeltaGlider::InitVCMesh()
{
	if (vcmesh) {
		// hide pilot head in VCPILOT position
		GROUPEDITSPEC ges;
		ges.flags = (campos < CAM_VCPSNGR1 ? GRPEDIT_ADDUSERFLAG : GRPEDIT_DELUSERFLAG);
		ges.UsrFlag = 3;
		oapiEditMeshGroup (vcmesh, GRP_PILOT_HEAD_VC, &ges);
		oapiEditMeshGroup (vcmesh, GRP_PILOT_VISOR_VC, &ges);

		//ComponentVessel::clbkResetVC (0, vcmesh);
	}
}

// ==============================================================
// Overloaded callback functions
// ==============================================================

// --------------------------------------------------------------
// Set vessel class parameters
// --------------------------------------------------------------
void DeltaGlider::clbkSetClassCaps (FILEHANDLE cfg)
{
	// *************** physical parameters **********************
	oapiWriteLogVerbose("[DG] clbkSetClassCaps");

	bool b;
	int i;
	if (oapiReadItem_bool (cfg, (char*)"SCRAMJET", b) && b) // set up scramjet configuration
		AddSubsystem (ssys_scram = new ScramSubsystem (this));

	ComponentVessel::SetEmptyMass (ssys_scram ? EMPTY_MASS_SC : EMPTY_MASS);
	VECTOR3 r[2] = {{0,0,6}, {0,0,-4}};
	SetSize (10.0);
	SetVisibilityLimit (7.5e-4, 1.5e-3);
	SetAlbedoRGB (_V(0.77,0.20,0.13));
	SetGravityGradientDamping (20.0);
	SetCW (0.09, 0.09, 2, 1.4);
	SetWingAspect (0.7);
	SetWingEffectiveness (2.5);
	SetCrossSections (_V(53.0,186.9,25.9));
	SetMaxWheelbrakeForce (2e5);
	SetPMI (_V(15.5,22.1,7.7));

	SetDockParams (_V(0,-0.49,10.076), _V(0,0,1), _V(0,1,0));
	SetTouchdownPoints (tdvtx_geardown, ntdvtx_geardown);
	SetNosewheelSteering (true);
	bGearIsDown = true;
	EnableTransponder (true);
	bool render_cockpit = true;

	// ******************** NAV radios **************************

	InitNavRadios (4);

	// ****************** propellant specs **********************

	tankconfig = (ssys_scram ? 1:0);
	switch (tankconfig) {
	case 0:
		max_rocketfuel = TANK1_CAPACITY + TANK2_CAPACITY;
		break;
	case 1:
		max_rocketfuel = TANK1_CAPACITY;
		break;
	}

	ph_main  = CreatePropellantResource (max_rocketfuel);    // main tank (fuel + oxydant)
	ph_rcs   = CreatePropellantResource (RCS_FUEL_CAPACITY); // RCS tank  (fuel + oxydant)
	SetDefaultPropellantResource (ph_main);

	// **************** thruster definitions ********************

	double ispscale = (modelidx ? 0.8 : 1.0);
	// Reduction of thrust efficiency at normal pressure

	contrail_tex = oapiRegisterParticleTexture ((char*)"Contrail1a");

	PARTICLESTREAMSPEC contrail = {
		0, 8.0, 4, 150, 0.25, 3.0, 4, 2.0, PARTICLESTREAMSPEC::DIFFUSE,
		PARTICLESTREAMSPEC::LVL_PSQRT, 0, 2,
		PARTICLESTREAMSPEC::ATM_PLOG, 1e-4, 1,
		contrail_tex
	};
	PARTICLESTREAMSPEC exhaust_main = {
		0, 2.0, 13, 150, 0.1, 0.2, 16, 1.0, PARTICLESTREAMSPEC::EMISSIVE,
		PARTICLESTREAMSPEC::LVL_SQRT, 0, 1,
		PARTICLESTREAMSPEC::ATM_PLOG, 1e-5, 0.1
	};
	PARTICLESTREAMSPEC exhaust_hover = {
		0, 1.5, 30, 150, 0.1, 0.1, 12, 1.0, PARTICLESTREAMSPEC::EMISSIVE,
		PARTICLESTREAMSPEC::LVL_SQRT, 0, 1,
		PARTICLESTREAMSPEC::ATM_PLOG, 1e-5, 0.1
	};

	// main thrusters
	th_main[0] = CreateThruster (_V(-1,0.0,-7.7), _V(0,0,1), MAX_MAIN_THRUST[modelidx], ph_main, ISP, ISP*ispscale);
	th_main[1] = CreateThruster (_V( 1,0.0,-7.7), _V(0,0,1), MAX_MAIN_THRUST[modelidx], ph_main, ISP, ISP*ispscale);
	thg_main = CreateThrusterGroup (th_main, 2, THGROUP_MAIN);
	EXHAUSTSPEC es_main[2] = {
		{th_main[0], NULL, NULL, NULL, 12, 1, 0, 0.1, NULL},
		{th_main[1], NULL, NULL, NULL, 12, 1, 0, 0.1, NULL}
	};
	for (i = 0; i < 2; i++) AddExhaust (es_main+i);
	AddExhaustStream (th_main[0], _V(-1,0,-15), &contrail);
	AddExhaustStream (th_main[1], _V( 1,0,-15), &contrail);
	AddExhaustStream (th_main[0], _V(-1,0,-10), &exhaust_main);
	AddExhaustStream (th_main[1], _V( 1,0,-10), &exhaust_main);
	//DWORD i = GetGroupThrusterCount (THGROUP_MAIN);

	// retro thrusters
	// note that we have to tilt retros slightly downwards to avoid inducing
	// an angular momentum, since they are mounted below the level of CG.
	// This also means that retros will induce an upward linear component.
	th_retro[0] = CreateThruster (_V(-3,-0.236,5.6), _V(0,0.04210548,-0.99911317), MAX_RETRO_THRUST, ph_main, ISP, ISP*ispscale);
	th_retro[1] = CreateThruster (_V( 3,-0.236,5.6), _V(0,0.04210548,-0.99911317), MAX_RETRO_THRUST, ph_main, ISP, ISP*ispscale);
	thg_retro = CreateThrusterGroup (th_retro, 2, THGROUP_RETRO);
	EXHAUSTSPEC es_retro[2] = {{th_retro[0], NULL, NULL, NULL, 3, 0.4, 0, 0.1, NULL}, {th_retro[1], NULL, NULL, NULL, 3, 0.4, 0, 0.1, NULL}};
	for (i = 0; i < 2; i++) AddExhaust (es_retro+i);

	// hover thrusters
	th_hover[0] = CreateThruster (_V(0,0,3), _V(0,1,0), MAX_HOVER_THRUST[modelidx], ph_main, ISP, ISP*ispscale);
	th_hover[1] = CreateThruster (_V(-3,0,-4.55), _V(0,1,0), 3.0/4.55*0.5*MAX_HOVER_THRUST[modelidx], ph_main, ISP, ISP*ispscale);
	th_hover[2] = CreateThruster (_V( 3,0,-4.55), _V(0,1,0), 3.0/4.55*0.5*MAX_HOVER_THRUST[modelidx], ph_main, ISP, ISP*ispscale);
	thg_hover = CreateThrusterGroup (th_hover, 3, THGROUP_HOVER);
	VECTOR3 hoverp0 = {0,-1.5, 3}, hoverp1 = {-3,-1.3,-4.55}, hoverp2 = {3,-1.3,-4.55}, hoverd = {0,1,0};
	EXHAUSTSPEC es_hover[3] = {
		{th_hover[0], NULL, &hoverp0, &hoverd, 6, 0.5, 0, 0.1, NULL, EXHAUST_CONSTANTPOS|EXHAUST_CONSTANTDIR},
		{th_hover[1], NULL, &hoverp1, &hoverd, 6, 0.5, 0, 0.1, NULL, EXHAUST_CONSTANTPOS|EXHAUST_CONSTANTDIR},
		{th_hover[2], NULL, &hoverp2, &hoverd, 6, 0.5, 0, 0.1, NULL, EXHAUST_CONSTANTPOS|EXHAUST_CONSTANTDIR}
	};
	for (i = 0; i < 3; i++) AddExhaust (es_hover+i);
	AddExhaustStream (th_hover[0], _V(0,-4,0), &contrail);
	AddExhaustStream (th_hover[0], _V(0,-2,3), &exhaust_hover);
	AddExhaustStream (th_hover[1], _V(-3,-2,-4.55), &exhaust_hover);
	AddExhaustStream (th_hover[2], _V( 3,-2,-4.55), &exhaust_hover);

	// set of attitude thrusters (idealised). The arrangement is such that no angular
	// momentum is created in linear mode, and no linear momentum is created in rotational mode.
	THRUSTER_HANDLE th_att_rot[4], th_att_lin[4];
	th_att_rot[0] = th_att_lin[0] = CreateThruster (_V(0,0, 8), _V(0, 1,0), MAX_RCS_THRUST, ph_rcs, ISP);
	th_att_rot[1] = th_att_lin[3] = CreateThruster (_V(0,0,-8), _V(0,-1,0), MAX_RCS_THRUST, ph_rcs, ISP);
	th_att_rot[2] = th_att_lin[2] = CreateThruster (_V(0,0, 8), _V(0,-1,0), MAX_RCS_THRUST, ph_rcs, ISP);
	th_att_rot[3] = th_att_lin[1] = CreateThruster (_V(0,0,-8), _V(0, 1,0), MAX_RCS_THRUST, ph_rcs, ISP);
	CreateThrusterGroup (th_att_rot,   2, THGROUP_ATT_PITCHUP);
	CreateThrusterGroup (th_att_rot+2, 2, THGROUP_ATT_PITCHDOWN);
	CreateThrusterGroup (th_att_lin,   2, THGROUP_ATT_UP);
	CreateThrusterGroup (th_att_lin+2, 2, THGROUP_ATT_DOWN);
	AddExhaust (th_att_rot[0], 0.6,  0.078, _V( -0.816081, -0.616431, 9.594813 ), _V(0,-1,0));
	AddExhaust (th_att_rot[0], 0.6,  0.078, _V( 0.816081, -0.616431, 9.594813 ), _V(0,-1,0));
	AddExhaust (th_att_rot[1], 0.79, 0.103, _V( -0.120063, 0.409999, -7.357354 ), _V(0, 1,0));
	AddExhaust (th_att_rot[1], 0.79, 0.103, _V( 0.120063, 0.409999, -7.357354 ), _V(0, 1,0));
	AddExhaust (th_att_rot[2], 0.6,  0.078, _V( -0.816081, -0.35857, 9.594813 ), _V(0, 1,0));
	AddExhaust (th_att_rot[2], 0.6,  0.078, _V( 0.816081, -0.35857, 9.594813 ), _V(0, 1,0));
	AddExhaust (th_att_rot[3], 0.79, 0.103, _V( -0.120063, -0.409999, -7.357354 ), _V(0,-1,0));
	AddExhaust (th_att_rot[3], 0.79, 0.103, _V( 0.120063, -0.409999, -7.357354 ), _V(0,-1,0));

	th_att_rot[0] = th_att_lin[0] = CreateThruster (_V(0,0, 6), _V(-1,0,0), MAX_RCS_THRUST, ph_rcs, ISP);
	th_att_rot[1] = th_att_lin[3] = CreateThruster (_V(0,0,-6), _V( 1,0,0), MAX_RCS_THRUST, ph_rcs, ISP);
	th_att_rot[2] = th_att_lin[2] = CreateThruster (_V(0,0, 6), _V( 1,0,0), MAX_RCS_THRUST, ph_rcs, ISP);
	th_att_rot[3] = th_att_lin[1] = CreateThruster (_V(0,0,-6), _V(-1,0,0), MAX_RCS_THRUST, ph_rcs, ISP);
	CreateThrusterGroup (th_att_rot,   2, THGROUP_ATT_YAWLEFT);
	CreateThrusterGroup (th_att_rot+2, 2, THGROUP_ATT_YAWRIGHT);
	CreateThrusterGroup (th_att_lin,   2, THGROUP_ATT_LEFT);
	CreateThrusterGroup (th_att_lin+2, 2, THGROUP_ATT_RIGHT);
	AddExhaust (th_att_rot[0], 0.6,  0.078, _V( 0.888971, -0.488177, 9.3408 ), _V(1,0,0));
	AddExhaust (th_att_rot[1], 0.94, 0.122, _V( -2.029295, 0.182903, -6.043046 ), _V(-1,0,0));
	AddExhaust (th_att_rot[2], 0.6,  0.078, _V( -0.888971, -0.488177, 9.3408 ), _V(-1,0,0));
	AddExhaust (th_att_rot[3], 0.94, 0.122, _V( 2.029295, 0.182903, -6.043046 ), _V(1,0,0));

	th_att_rot[0] = CreateThruster (_V( 6,0,0), _V(0, 1,0), MAX_RCS_THRUST, ph_rcs, ISP);
	th_att_rot[1] = CreateThruster (_V(-6,0,0), _V(0,-1,0), MAX_RCS_THRUST, ph_rcs, ISP);
	th_att_rot[2] = CreateThruster (_V(-6,0,0), _V(0, 1,0), MAX_RCS_THRUST, ph_rcs, ISP);
	th_att_rot[3] = CreateThruster (_V( 6,0,0), _V(0,-1,0), MAX_RCS_THRUST, ph_rcs, ISP);
	CreateThrusterGroup (th_att_rot, 2, THGROUP_ATT_BANKLEFT);
	CreateThrusterGroup (th_att_rot+2, 2, THGROUP_ATT_BANKRIGHT);
	AddExhaust (th_att_rot[0], 1.03, 0.134, _V( -5.121185, -0.073903, 0.375386 ), _V(0, 1,0));
	AddExhaust (th_att_rot[1], 1.03, 0.134, _V( 5.121185, -0.654322, 0.375386 ), _V(0,-1,0));
	AddExhaust (th_att_rot[2], 1.03, 0.134, _V( 5.121185, -0.073903, 0.375386 ), _V(0, 1,0));
	AddExhaust (th_att_rot[3], 1.03, 0.134, _V( -5.121185, -0.654322, 0.375386 ), _V(0,-1,0));

	th_att_lin[0] = CreateThruster (_V(0,0,-7), _V(0,0, 1), 2*MAX_RCS_THRUST, ph_rcs, ISP);
	th_att_lin[1] = CreateThruster (_V(0,0, 7), _V(0,0,-1), 2*MAX_RCS_THRUST, ph_rcs, ISP);
	CreateThrusterGroup (th_att_lin,   1, THGROUP_ATT_FORWARD);
	CreateThrusterGroup (th_att_lin+1, 1, THGROUP_ATT_BACK);
	AddExhaust (th_att_lin[0], 0.6, 0.078, _V( 0.0, -0.228914, -7.462329 ), _V(0,0,-1));
	AddExhaust (th_att_lin[0], 0.6, 0.078, _V( 0.0, 0.229, -7.462329 ), _V(0,0,-1));
	AddExhaust (th_att_lin[1], 0.6, 0.078, _V( -0.817096, -0.488177, 9.729635 ), _V(0,0,1));
	AddExhaust (th_att_lin[1], 0.6, 0.078, _V( 0.817096, -0.488177, 9.729635 ), _V(0,0,1));

	COLOUR4 col_d = {0.9,0.8,1,0};
	COLOUR4 col_s = {1.9,0.8,1,0};
	COLOUR4 col_a = {0,0,0,0};
	COLOUR4 col_white = {1,1,1,0};
	LightEmitter *le = AddPointLight (_V(0,0,-10), 200, 1e-3, 0, 2e-3, col_d, col_s, col_a);
	le->SetIntensityRef (&th_main_level);

	// ********************* aerodynamics ***********************

	hwing = CreateAirfoil3 (LIFT_VERTICAL, _V(0,0,-0.3), VLiftCoeff, 0, 5, 90, 1.5);
	// wing and body lift+drag components

	CreateAirfoil3 (LIFT_HORIZONTAL, _V(0,0,-4), HLiftCoeff, 0, 5, 15, 1.5);
	// vertical stabiliser and body lift and drag components

	CreateControlSurface3 (AIRCTRL_ELEVATOR,     1.4, 1.7, _V(   0,0,-7.2), AIRCTRL_AXIS_XPOS, 1.0, anim_elevator);
	CreateControlSurface3 (AIRCTRL_RUDDER,       0.8, 1.7, _V(   0,0,-7.2), AIRCTRL_AXIS_YPOS, 1.0, anim_rudder);
	hlaileron = CreateControlSurface3 (AIRCTRL_AILERON, 0.3, 1.7, _V( 7.5,0,-7.2), AIRCTRL_AXIS_XPOS, 1.0, anim_raileron);
	hraileron = CreateControlSurface3 (AIRCTRL_AILERON, 0.3, 1.7, _V(-7.5,0,-7.2), AIRCTRL_AXIS_XNEG, 1.0, anim_laileron);
	CreateControlSurface3 (AIRCTRL_ELEVATORTRIM, 0.3, 1.7, _V(   0,0,-7.2), AIRCTRL_AXIS_XPOS, 1.0, anim_elevatortrim);

	CreateVariableDragElement (ssys_gear->GearState().StatePtr(), 0.8, _V(0, -1, 0));     // landing gear
	CreateVariableDragElement (ssys_mainretro->RetroCoverState().StatePtr(), 0.2, _V(0,-0.5,6.5)); // retro covers
	CreateVariableDragElement (ssys_docking->NconeState().StatePtr(), 3, _V(0, 0, 8));        // nose cone
	CreateVariableDragElement (ssys_thermal->RadiatorState().StatePtr(), 1, _V(0,1.5,-4));   // radiator
	CreateVariableDragElement (ssys_aerodyn->AirbrakeState().StatePtr(), 4, _V(0,0,-8));        // airbrake

	SetRotDrag (_V(0.10,0.13,0.04));
	// angular damping

	// ************************* mesh ***************************

	// ********************* beacon lights **********************
	static VECTOR3 beaconpos[8] = {{-8.6,0,-3.3}, {8.6,0,-3.3}, {0,0.5,-7.5}, {0,2.2,2}, {0,-1.4,2}, {-8.9,2.5,-5.4}, {8.9,2.5,-5.4}, {2.5,-0.5,6.5}};
	static VECTOR3 beaconpos_scram = {0,-1.8,2};
	static VECTOR3 beaconcol[8] = {{1.0,0.5,0.5}, {0.5,1.0,0.5}, {1,1,1}, {1,0.6,0.6}, {1,0.6,0.6}, {1,1,1}, {1,1,1} , {1,1,1}};
	for (i = 0; i < 8; i++) {
		beacon[i].shape = (i < 3 ? BEACONSHAPE_DIFFUSE : BEACONSHAPE_STAR);
		beacon[i].pos = beaconpos+i;
		beacon[i].col = beaconcol+i;
		beacon[i].size = (i < 3 || i == 7 ? 0.3 : 0.55);
		beacon[i].falloff = (i < 3 ? 0.4 : 0.6);
		beacon[i].period = (i < 3 ? 0 : i < 5 ? 2 : i < 7 ? 1.13 : 0);
		beacon[i].duration = (i < 5 ? 0.1 : 0.05);
		beacon[i].tofs = (6-i)*0.2;
		beacon[i].active = false;
		AddBeacon (beacon+i);
	}
	if (ssys_scram) beacon[4].pos = &beaconpos_scram;

	SetMeshVisibilityMode (AddMesh (exmesh_tpl = oapiLoadMeshGlobal (ScramVersion() ? "DG\\deltaglider" : "DG\\deltaglider_ns")), MESHVIS_EXTERNAL);
	//SetMeshVisibilityMode (AddMesh (vcmesh_tpl = oapiLoadMeshGlobal ("DG\\deltaglider_vc")), MESHVIS_VC);
	panelmesh0 = oapiLoadMeshGlobal ("DG\\dg_2dpanel0");
	panelmesh1 = oapiLoadMeshGlobal ("DG\\dg_2dpanel1");

	vcmesh_tpl = oapiLoadMeshGlobal ("DG\\deltaglider_vc");
	SetMeshVisibilityMode (AddMesh (vcmesh_tpl), MESHVIS_VC);

	// **************** vessel-specific insignia ****************

	insignia_tex = oapiCreateSurfaceEx (256, 256, OAPISURFACE_RENDERTARGET | OAPISURFACE_TEXTURE | OAPISURFACE_MIPMAPS);
	SURFHANDLE hTex = oapiGetTextureHandle (exmesh_tpl, 5);
	if (hTex) oapiBlt (insignia_tex, hTex, 0, 0, 0, 0, 256, 256);
}

// --------------------------------------------------------------
// Read status from scenario file
// --------------------------------------------------------------
void DeltaGlider::clbkLoadStateEx (FILEHANDLE scn, void *vs)
{
	oapiWriteLogVerbose("[DG] clbkLoadStateEx");
    char *line;

	while (oapiReadScenario_nextline (scn, line)) {
		if (!_strnicmp (line, "TANKCONFIG", 10)) {
			if (ssys_scram) sscanf (line+10, "%d", &tankconfig);
		} else if (!_strnicmp (line, "PSNGR", 5)) {
			DWORD i, res, pi[4];
			res = sscanf (line+5, "%d%d%d%d", pi+0, pi+1, pi+2, pi+3);
			for (i = 0; i < res; i++)
				if (pi[i]-1 < 4) psngr[pi[i]-1] = true;
		} else if (!_strnicmp (line, "SKIN", 4)) {
			sscanf (line+4, "%s", skinpath);
			char fname[256];
			strcpy (fname, "DG\\Skins\\");
			strcat (fname, skinpath);
			int n = strlen(fname); fname[n++] = '\\';
			strcpy (fname+n, "dgmk4_1.dds");  skin[0] = oapiLoadTexture (fname);
			strcpy (fname+n, ssys_scram ? "dgmk4_2.dds" : "dgmk4_2_ns.dds");  skin[1] = oapiLoadTexture (fname);
			strcpy (fname+n, "idpanel1.dds"); skin[2] = oapiLoadTexture (fname);
			if (skin[2]) {
				oapiBlt (insignia_tex, skin[2], 0, 0, 0, 0, 256, 256);
				oapiReleaseTexture (skin[2]);
				skin[2] = NULL;
			}
		} else if (!_strnicmp (line, "PANELCOL", 8)) {
			sscanf (line+8, "%d", &panelcol);
        } else {
			// offer the line to all subsystems
			if (!ComponentVessel::clbkParseScenarioLine (line))
				// unrecognised option - pass to Orbiter's generic parser
	            ParseScenarioLineEx (line, vs);
        }
    }
}

// --------------------------------------------------------------
// Write status to scenario file
// --------------------------------------------------------------
void DeltaGlider::clbkSaveState (FILEHANDLE scn)
{
	oapiWriteLogVerbose("[DG] clbkSaveState");
	char cbuf[256];
	int i;

	// Write default and subsystem vessel parameters
	ComponentVessel::clbkSaveState (scn);

	// Write custom parameters
	for (i = 0; i < 4; i++)
		if (psngr[i]) {
			snprintf (cbuf, sizeof(cbuf) - 1, "%d", i+1);
			for (++i; i < 4; i++)
				if (psngr[i]) sprintf (cbuf+strlen(cbuf), " %d", i+1);
			oapiWriteScenario_string (scn, (char*)"PSNGR", cbuf);
			break;
		}
	if (skinpath[0])
		oapiWriteScenario_string (scn, (char*)"SKIN", skinpath);
	if (panelcol) {
		oapiWriteScenario_int (scn, (char*)"PANELCOL", panelcol);
	}
	for (i = 0; i < 8; i++)
		if (beacon[i].active) {
			snprintf (cbuf, sizeof(cbuf) - 1, "%d %d %d %d", beacon[0].active, beacon[3].active, beacon[5].active, beacon[7].active);
			oapiWriteScenario_string (scn, (char*)"LIGHTS", cbuf);
			break;
		}

	if (tankconfig)
		oapiWriteScenario_int (scn, (char*)"TANKCONFIG", tankconfig);
}

// --------------------------------------------------------------
// Finalise vessel creation
// --------------------------------------------------------------
void DeltaGlider::clbkPostCreation ()
{
	oapiWriteLogVerbose("[DG] clbkPostCreation");
	ComponentVessel::clbkPostCreation ();

	SetEmptyMass ();
	if (tankconfig) {
		if (!ssys_scram) {
			tankconfig = 0;
		} else {
			double max_scramfuel;
			switch (tankconfig) {
			case 1:
				max_rocketfuel = TANK1_CAPACITY;
				max_scramfuel = TANK2_CAPACITY;
				break;
			case 2:
				max_rocketfuel = TANK2_CAPACITY;
				max_scramfuel  = TANK1_CAPACITY;
				break;
			}
			SetPropellantMaxMass (ph_main, max_rocketfuel);
			ssys_scram->SetPropellantMaxMass (max_scramfuel);
		}
	}

	if (insignia_tex)
		PaintMarkings (insignia_tex);
}

// --------------------------------------------------------------
// Create DG visual
// --------------------------------------------------------------
void DeltaGlider::clbkVisualCreated (VISHANDLE vis, int refcount)
{
	oapiWriteLogVerbose("[DG] clbkVisualCreated");
	visual = vis;
	exmesh = GetDevMesh (vis, 0);
	vcmesh = GetDevMesh (vis, 1);
	SetPassengerVisuals();
	SetDamageVisuals();

	if (vcmesh && !ScramVersion()) { // disable scram-specific components
		GROUPEDITSPEC ges;
		ges.flags = GRPEDIT_ADDUSERFLAG;
		ges.UsrFlag = 3;
		static int vcscramidx[11] = {
			GRP_SCRAMGIMBAL_L_VC,GRP_SCRAMGIMBAL_R_VC,
			GRP_SCRAM_GIMBAL_FRAME_VC,GRP_SCRAM_INDICATOR_LABEL_VC,GRP_SCRAM_STATUS_VC,
			GRP_SCRAM_TEMP_VC,GRP_SCRAM_GIMBAL_INDICATOR_VC,
			GRP_THROTTLE_SCRAM_L1_VC,GRP_THROTTLE_SCRAM_R1_VC,
			GRP_THROTTLE_SCRAM_L2_VC,GRP_THROTTLE_SCRAM_R2_VC};
		for (DWORD i = 0; i < 11; i++)
			oapiEditMeshGroup (vcmesh, vcscramidx[i], &ges);
	}

	ApplySkin();

	//if (oapiCameraInternal()) {
		UpdateStatusIndicators();
		if (oapiCockpitMode() == COCKPIT_VIRTUAL)
			InitVCMesh();
	//}
		if (vcmesh)
			ComponentVessel::clbkResetVC (0, vcmesh);

}

// --------------------------------------------------------------
// Destroy DG visual
// --------------------------------------------------------------
void DeltaGlider::clbkVisualDestroyed (VISHANDLE vis, int refcount)
{
	oapiWriteLogVerbose("[DG] clbkVisualDestroyed");
	visual = NULL;
	exmesh = NULL;
	vcmesh = NULL;
}

// --------------------------------------------------------------
// Respond to MFD mode change
// --------------------------------------------------------------
void DeltaGlider::clbkMFDMode (int mfd, int mode)
{
	ssys_mfd[mfd-MFD_LEFT]->ModeChanged ();
}

// --------------------------------------------------------------
// Respond to RCS mode change
// --------------------------------------------------------------
void DeltaGlider::clbkRCSMode (int mode)
{
	ssys_rcs->SetMode (mode);
}

// --------------------------------------------------------------
// Respond to control surface mode change
// --------------------------------------------------------------
void DeltaGlider::clbkADCtrlMode (DWORD mode)
{
	ssys_aerodyn->SetMode (mode);
}

// --------------------------------------------------------------
// Respond to HUD mode change
// --------------------------------------------------------------
void DeltaGlider::clbkHUDMode (int mode)
{
	ssys_hud->SetHUDMode (mode);
}

// --------------------------------------------------------------
// Respond to navmode change
// --------------------------------------------------------------
void DeltaGlider::clbkNavMode (int mode, bool active)
{
	if (mode == NAVMODE_HOLDALT) {
		ssys_hoverctrl->ActivateHold (active);
	} else {
		ssys_rcs->SetProg (mode, active);
	}
}

// --------------------------------------------------------------
// Respond to docking/undocking event
// --------------------------------------------------------------
void DeltaGlider::clbkDockEvent (int dock, OBJHANDLE mate)
{
	ssys_docking->clbkDockEvent (dock, mate);
}

// --------------------------------------------------------------
// Respond to navmode processing request
// --------------------------------------------------------------
int DeltaGlider::clbkNavProcess (int mode)
{
	if (mode & NAVBIT_HOLDALT) {
		//mode ^= NAVBIT_HOLDALT;
		//ProcessHoverHoldalt();
	}
	return mode;
}

// --------------------------------------------------------------
// Frame update
// --------------------------------------------------------------
void DeltaGlider::clbkPostStep (double simt, double simdt, double mjd)
{
	th_main_level = GetThrusterGroupLevel (THGROUP_MAIN);

	// damage/failure system
	if (bDamageEnabled) TestDamage ();

	ComponentVessel::clbkPostStep (simt, simdt, mjd);
}

bool DeltaGlider::clbkLoadGenericCockpit ()
{
	SetCameraOffset (_V(0,1.467,6.782));
	oapiSetDefNavDisplay (1);
	oapiSetDefRCSDisplay (1);
	campos = CAM_GENERIC;
	return true;
}

// --------------------------------------------------------------
// Load 2-D instrument panel mode
// --------------------------------------------------------------

bool DeltaGlider::clbkLoadPanel2D (int id, PANELHANDLE hPanel, DWORD viewW, DWORD viewH)
{
	oapiWriteLogVerbose("[DG] clbkLoadPanel2D");
	// set up subsystem panel elements
	ComponentVessel::clbkLoadPanel2D (id, hPanel, viewW, viewH);

	switch (id) {
	case 0:
		DefinePanelMain (hPanel);
		SetPanelScale (hPanel, viewW, viewH);
		oapiSetPanelNeighbours (-1,-1,1,-1);
		SetCameraDefaultDirection (_V(0,0,1)); // forward
		oapiCameraSetCockpitDir (0,0);         // look forward
		return true;
	case 1:
		DefinePanelOverhead (hPanel);
		SetPanelScale (hPanel, viewW, viewH);
		oapiSetPanelNeighbours (-1,-1,-1,0);
		SetCameraDefaultDirection (_V(0,0,1)); // forward
		oapiCameraSetCockpitDir (0,20*RAD);    // look up
		return true;
	default:
		return false;
	}
}

void DeltaGlider::SetPanelScale (PANELHANDLE hPanel, DWORD viewW, DWORD viewH)
{
	double defscale = (double)viewW/PANEL2D_WIDTH;
	double extscale = max (defscale, 1.0);
	SetPanelScaling (hPanel, defscale, extscale);
}

void DeltaGlider::DefinePanelMain (PANELHANDLE hPanel)
{
	hPanelMesh = panelmesh0;
	SURFHANDLE panel2dtex = oapiGetTextureHandle(hPanelMesh,1);

	const DWORD panelw = PANEL2D_WIDTH, panelh = 572;

	SetPanelBackground (hPanel, 0, 0, hPanelMesh, panelw, panelh, 190,
		PANEL_ATTACH_BOTTOM | PANEL_MOVEOUT_BOTTOM);

	// Define MFD layout (display and buttons)
	RegisterPanelMFDGeometry (hPanel, MFD_LEFT, 0, GRP_LMFD_DISPLAY_P0);
	RegisterPanelMFDGeometry (hPanel, MFD_RIGHT, 0, GRP_RMFD_DISPLAY_P0);

	MESHGROUP *grp = oapiMeshGroup(hPanelMesh, GRP_SCRAM_INSTRUMENTS_P0);
	grp->UsrFlag = (ScramVersion() ? 0 : 3);

	//aap->RegisterPanel (hPanel);
	InitPanel (0);
}

void DeltaGlider::DefinePanelOverhead (PANELHANDLE hPanel)
{
	hPanelMesh = panelmesh1;
	const DWORD panelw = PANEL2D_WIDTH, panelh = 283;

	SetPanelBackground (hPanel, 0, 0, hPanelMesh, panelw, panelh, 0,
		PANEL_ATTACH_TOP | PANEL_MOVEOUT_TOP);

	InitPanel (1);
}

// --------------------------------------------------------------
// Respond to panel mouse event
// --------------------------------------------------------------
bool DeltaGlider::clbkPanelMouseEvent (int id, int event, int mx, int my, void *context)
{
	if (context) {
		PanelElement *pe = (PanelElement*)context;
		return pe->ProcessMouse2D (event, mx, my);
	}
	return false;
}

// --------------------------------------------------------------
// Respond to panel redraw event
// --------------------------------------------------------------
bool DeltaGlider::clbkPanelRedrawEvent (int id, int event, SURFHANDLE surf, void *context)
{
	if (context) {
		PanelElement *pe = (PanelElement*)context;
		return pe->Redraw2D (surf);
	}

	return false;
}

// --------------------------------------------------------------
// Load virtual cockpit mode
// --------------------------------------------------------------
bool DeltaGlider::clbkLoadVC (int id)
{
	oapiWriteLogVerbose("[DG] clbkLoadVC");

	static VCMFDSPEC mfds_left  = {1, GRP_LMFD_DISPLAY_VC};
	static VCMFDSPEC mfds_right = {1, GRP_RMFD_DISPLAY_VC};
	static VCHUDSPEC huds = {1, GRP_HUDDISP_VC, {0,1.462,7.09}, 0.15};
	SURFHANDLE tex1 = oapiGetTextureHandle (vcmesh_tpl, 16);
	SURFHANDLE tex2 = oapiGetTextureHandle (vcmesh_tpl, 18);
	SURFHANDLE tex3 = oapiGetTextureHandle (vcmesh_tpl, 14);
	vctex = oapiGetTextureHandle (vcmesh_tpl, 20);

	InitVC (id);

	SetCameraDefaultDirection (_V(0,0,1)); // forward
	oapiVCRegisterHUD (&huds); // HUD parameters
	oapiVCRegisterMFD (MFD_LEFT, &mfds_left);   // left MFD
	oapiVCRegisterMFD (MFD_RIGHT, &mfds_right); // right MFD

	switch (id) {
	case 0: // pilot
		SetCameraOffset (_V(0,1.467,6.782));
		SetCameraShiftRange (_V(0,0,0.1), _V(-0.2,0,0), _V(0.2,0,0));
		oapiVCSetNeighbours (1, 2, -1, -1);

		// main/retro/hover engine indicators
		oapiVCRegisterArea (AID_MAINDISP1, _R( 50,16, 63,89), PANEL_REDRAW_ALWAYS, PANEL_MOUSE_IGNORE, PANEL_MAP_BGONREQUEST, tex1);
		oapiVCRegisterArea (AID_MAINDISP2, _R( 85,16, 98,89), PANEL_REDRAW_ALWAYS, PANEL_MOUSE_IGNORE, PANEL_MAP_BGONREQUEST, tex1);
		oapiVCRegisterArea (AID_MAINDISP3, _R(120,16,133,89), PANEL_REDRAW_ALWAYS, PANEL_MOUSE_IGNORE, PANEL_MAP_BGONREQUEST, tex1);

		// scram engine indicators
		if (ScramVersion()) {
			oapiVCRegisterArea (AID_SCRAMDISP2, _R(195,16,208,89), PANEL_REDRAW_ALWAYS, PANEL_MOUSE_IGNORE, PANEL_MAP_BGONREQUEST, tex1);
			oapiVCRegisterArea (AID_SCRAMTEMPDISP, _R(6,10,87,140), PANEL_REDRAW_ALWAYS, PANEL_MOUSE_IGNORE, PANEL_MAP_BACKGROUND, tex2);
		}

		campos = CAM_VCPILOT;
		break;

	case 1: // front left passenger
		SetCameraOffset (_V(-0.7, 1.15, 5.55));
		SetCameraMovement (_V(0.2,-0.05,0.3), -10*RAD, 10*RAD, _V(-0.3,0,0), 80*RAD, 0, _V(0.4,0,0), -90*RAD, 0);
		oapiVCSetNeighbours (-1, 2, 0, 3);
		campos = CAM_VCPSNGR1;
		break;

	case 2: // front right passenger
		SetCameraOffset (_V(0.7, 1.15, 5.55));
		SetCameraMovement (_V(-0.2,-0.05,0.3), 10*RAD, 10*RAD, _V(-0.4,0,0), 90*RAD, 0, _V(0.3,0,0), -80*RAD, 0);
		oapiVCSetNeighbours (1, -1, 0, 4);
		campos = CAM_VCPSNGR2;
		break;

	case 3: // rear left passenger
		SetCameraOffset (_V(-0.8, 1.2, 4.4));
		SetCameraMovement (_V(0.4,0,0), 0, 0, _V(-0.3,0,0), 70*RAD, 0, _V(0.4,0,0), -90*RAD, 0);
		oapiVCSetNeighbours (-1, 4, 1, -1);
		campos = CAM_VCPSNGR3;
		break;

	case 4: // rear right passenger
		SetCameraOffset (_V(0.8, 1.2, 4.4));
		SetCameraMovement (_V(-0.4,0,0), 0, 0, _V(-0.4,0,0), 90*RAD, 0, _V(0.3,0,0), -70*RAD, 0);
		oapiVCSetNeighbours (3, -1, 2, -1);
		campos = CAM_VCPSNGR4;
		break;

	default:
		return false;
	}

	ComponentVessel::clbkLoadVC (id);
	InitVCMesh();
	return true;
}

// --------------------------------------------------------------
// Respond to virtual cockpit mouse event
// --------------------------------------------------------------
bool DeltaGlider::clbkVCMouseEvent (int id, int event, VECTOR3 &p)
{
	// standalone id
	switch (id) {
	case AID_MFD1_PWR:
		oapiToggleMFD_on (MFD_LEFT);
		return true;
	case AID_MFD2_PWR:
		oapiToggleMFD_on (MFD_RIGHT);
		return true;
	}

	// distribute to subsystems
	return ComponentVessel::clbkVCMouseEvent (id, event, p);
}

// --------------------------------------------------------------
// Respond to virtual cockpit area redraw request
// --------------------------------------------------------------
bool DeltaGlider::clbkVCRedrawEvent (int id, int event, SURFHANDLE surf)
{
	if (!vcmesh) return false;

	// standalone id
	switch (id) {
	case AID_MAINDISP1:
		return RedrawPanel_MainFlow (surf);
	case AID_MAINDISP2:
		return RedrawPanel_RetroFlow (surf);
	case AID_MAINDISP3:
		return RedrawPanel_HoverFlow (surf);
	case AID_SCRAMDISP2:
		return RedrawPanel_ScramFlow (surf);
	case AID_SCRAMTEMPDISP:
		return RedrawPanel_ScramTempDisp (surf);
	}

	// distribute to subsystems
	return ComponentVessel::clbkVCRedrawEvent (id, event, vcmesh, surf);
}

// --------------------------------------------------------------
// Process buffered key events
// --------------------------------------------------------------
int DeltaGlider::clbkConsumeBufferedKey (DWORD key, bool down, char *kstate)
{
	if (!down) return 0; // only process keydown events
	if (Playback()) return 0; // don't allow manual user input during a playback

	if (!KEYMOD_ALT (kstate) && !KEYMOD_SHIFT (kstate) && KEYMOD_CONTROL (kstate) && key == OAPI_KEY_SPACE) {
		oapiOpenDialogEx (g_Param.hDLL, IDD_CTRL, Ctrl_DlgProc, DLG_CAPTIONCLOSE, this);
		return 1;
	}
	return ComponentVessel::clbkConsumeBufferedKey (key, down, kstate);
}

// --------------------------------------------------------------
// Respond to generic messages
// --------------------------------------------------------------
int DeltaGlider::clbkGeneric (int msgid, int prm, void *context)
{
	switch (msgid) {
	case VMSG_LUAINTERPRETER:
		return Lua_InitInterpreter (context);
	case VMSG_LUAINSTANCE:
		return Lua_InitInstance (context);
	}
	return 0;
}


// ==============================================================
// API callback interface
// ==============================================================

// --------------------------------------------------------------
// Module initialisation
// --------------------------------------------------------------
DLLCLBK void InitModule (HINSTANCE hModule)
{
	g_Param.hDLL = hModule;
	oapiRegisterCustomControls (hModule);

	// allocate SketchPad resources
	g_Param.pen[0] = oapiCreatePen (PS_SOLID, 1, RGB(224,224,224));
	g_Param.pen[1] = oapiCreatePen (PS_SOLID, 3, RGB(164,164,164));
	g_Param.surf = oapiLoadTexture ("DG\\blitsrc1.dds", true);
}

// --------------------------------------------------------------
// Module cleanup
// --------------------------------------------------------------
DLLCLBK void ExitModule (HINSTANCE hModule)
{
	oapiUnregisterCustomControls (hModule);

	int i;

	// deallocate SketchPad resources
	for (i = 0; i < 2; i++) oapiReleasePen(g_Param.pen[i]);
	oapiReleaseTexture (g_Param.surf);
}

// --------------------------------------------------------------
// Vessel initialisation
// --------------------------------------------------------------
DLLCLBK VESSEL *ovcInit (OBJHANDLE hvessel, int flightmodel)
{
	// need to init device-dependent resources here in case the screen mode has changed
	g_Param.col[2] = oapiGetColour(80,80,224);
	g_Param.col[3] = oapiGetColour(160,120,64);

	return new DeltaGlider (hvessel, flightmodel);
}

// --------------------------------------------------------------
// Vessel cleanup
// --------------------------------------------------------------
DLLCLBK void ovcExit (VESSEL *vessel)
{
	if (vessel) delete (DeltaGlider*)vessel;
}


// ==============================================================
// Scenario editor interface
// ==============================================================

DeltaGlider *GetDG (HWND hDlg)
{
	// retrieve DG interface from scenario editor
	OBJHANDLE hVessel;
	SendMessage (hDlg, WM_SCNEDITOR, SE_GETVESSEL, (LPARAM)&hVessel);
	return (DeltaGlider*)oapiGetVesselInterface (hVessel);
}

void UpdateDamage (HWND hTab, DeltaGlider *dg)
{
	int i;
	char cbuf[256];

	i = (int)(dg->lwingstatus*100.0+0.5);
	snprintf (cbuf, sizeof(cbuf) - 1, "%d %%", i);
	SetWindowText (GetDlgItem (hTab, IDC_LEFTWING_STATUS), cbuf);
	oapiSetGaugePos (GetDlgItem (hTab, IDC_LEFTWING_SLIDER), i);
	i = (int)(dg->rwingstatus*100.0+0.5);
	snprintf (cbuf, sizeof(cbuf) - 1, "%d %%", i);
	SetWindowText (GetDlgItem (hTab, IDC_RIGHTWING_STATUS), cbuf);
	oapiSetGaugePos (GetDlgItem (hTab, IDC_RIGHTWING_SLIDER), i);
}

// --------------------------------------------------------------
// Message procedure for editor page 1 (animation settings)
// --------------------------------------------------------------
INT_PTR CALLBACK EdPg1Proc (HWND hTab, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDHELP:
			g_hc.topic = (char*)"/SE_Anim.htm";
			oapiOpenHelp (&g_hc);
			return TRUE;
		case IDC_GEAR_UP:
			GetDG(hTab)->SubsysGear()->RaiseGear();
			return TRUE;
		case IDC_GEAR_DOWN:
			GetDG(hTab)->SubsysGear()->LowerGear();
			return TRUE;
		case IDC_RETRO_CLOSE:
			GetDG(hTab)->SubsysMainRetro()->CloseRetroCover();
			return TRUE;
		case IDC_RETRO_OPEN:
			GetDG(hTab)->SubsysMainRetro()->OpenRetroCover();
			return TRUE;
		case IDC_OLOCK_CLOSE:
			GetDG(hTab)->SubsysPressure()->CloseOuterAirlock();
			return TRUE;
		case IDC_OLOCK_OPEN:
			GetDG(hTab)->SubsysPressure()->OpenOuterAirlock();
			return TRUE;
		case IDC_ILOCK_CLOSE:
			GetDG(hTab)->SubsysPressure()->CloseInnerAirlock();
			return TRUE;
		case IDC_ILOCK_OPEN:
			GetDG(hTab)->SubsysPressure()->OpenInnerAirlock();
			return TRUE;
		case IDC_NCONE_CLOSE:
			GetDG(hTab)->SubsysDocking()->CloseNcone();
			return TRUE;
		case IDC_NCONE_OPEN:
			GetDG(hTab)->SubsysDocking()->OpenNcone();
			return TRUE;
		case IDC_LADDER_RETRACT:
			GetDG(hTab)->SubsysDocking()->RetractLadder();
			return TRUE;
		case IDC_LADDER_EXTEND:
			GetDG(hTab)->SubsysDocking()->ExtendLadder();
			return TRUE;
		case IDC_HATCH_CLOSE:
			GetDG(hTab)->SubsysPressure()->CloseHatch();
			return TRUE;
		case IDC_HATCH_OPEN:
			GetDG(hTab)->SubsysPressure()->OpenHatch();
			return TRUE;
		case IDC_RADIATOR_RETRACT:
			GetDG(hTab)->SubsysThermal()->CloseRadiator();
			return TRUE;
		case IDC_RADIATOR_EXTEND:
			GetDG(hTab)->SubsysThermal()->OpenRadiator();
			return TRUE;
		}
		break;
	}
	return FALSE;
}

// --------------------------------------------------------------
// Message procedure for editor page 2 (passengers)
// --------------------------------------------------------------
INT_PTR CALLBACK EdPg2Proc (HWND hTab, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DeltaGlider *dg;
	int i;

	switch (uMsg) {
	case WM_INITDIALOG: {
		char cbuf[256];
		dg = (DeltaGlider*)oapiGetVesselInterface ((OBJHANDLE)lParam);
		for (i = 0; i < 4; i++)
			SendDlgItemMessage (hTab, IDC_CHECK1+i, BM_SETCHECK, dg->psngr[i] ? BST_CHECKED : BST_UNCHECKED, 0);
		snprintf (cbuf, sizeof(cbuf) - 1, "%0.2f kg", dg->GetMass());
		SetWindowText (GetDlgItem (hTab, IDC_MASS), cbuf);
		} break;
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDC_CHECK1:
		case IDC_CHECK2:
		case IDC_CHECK3:
		case IDC_CHECK4: {
			char cbuf[256];
			i = SendDlgItemMessage (hTab, LOWORD(wParam), BM_GETCHECK, 0, 0);
			dg = GetDG(hTab);
			dg->psngr[LOWORD(wParam)-IDC_CHECK1] = (i ? true:false);
			dg->SetPassengerVisuals();
			dg->SetEmptyMass();
			snprintf (cbuf, sizeof(cbuf) - 1, "%0.2f kg", dg->GetMass());
			SetWindowText (GetDlgItem (hTab, IDC_MASS), cbuf);
			} break;
		}
		break;
	}
	return FALSE;
}

// --------------------------------------------------------------
// Message procedure for editor page 3 (damage)
// --------------------------------------------------------------
INT_PTR CALLBACK EdPg3Proc (HWND hTab, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DeltaGlider *dg;

	switch (uMsg) {
	case WM_INITDIALOG: {
		dg = (DeltaGlider*)oapiGetVesselInterface ((OBJHANDLE)lParam);
		GAUGEPARAM gp = { 0, 100, GAUGEPARAM::LEFT, GAUGEPARAM::BLACK };
		oapiSetGaugeParams (GetDlgItem (hTab, IDC_LEFTWING_SLIDER), &gp);
		oapiSetGaugeParams (GetDlgItem (hTab, IDC_RIGHTWING_SLIDER), &gp);
		UpdateDamage (hTab, dg);
		} break;
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDC_REPAIR:
			dg = GetDG(hTab);
			dg->RepairDamage ();
			UpdateDamage (hTab, dg);
			return TRUE;
		}
		break;
	case WM_HSCROLL:
		dg = GetDG(hTab);
		int id = GetDlgCtrlID ((HWND)lParam);
		switch (id) {
		case IDC_LEFTWING_SLIDER:
		case IDC_RIGHTWING_SLIDER:
			switch (LOWORD (wParam)) {
			case SB_THUMBTRACK:
			case SB_LINELEFT:
			case SB_LINERIGHT:
				if (id == IDC_LEFTWING_SLIDER)
					dg->lwingstatus = HIWORD(wParam)*0.01;
				else
					dg->rwingstatus = HIWORD(wParam)*0.01;
				dg->ApplyDamage ();
				UpdateDamage (hTab, dg);
				return TRUE;
			}
			break;
		}
		break;
	}
	return FALSE;
}

// --------------------------------------------------------------
// Add vessel-specific pages into scenario editor
// --------------------------------------------------------------
DLLCLBK void secInit (HWND hEditor, OBJHANDLE hVessel)
{
	DeltaGlider *dg = (DeltaGlider*)oapiGetVesselInterface (hVessel);

	EditorPageSpec eps1 = {"Animations", g_Param.hDLL, IDD_EDITOR_PG1, EdPg1Proc};
	SendMessage (hEditor, WM_SCNEDITOR, SE_ADDPAGEBUTTON, (LPARAM)&eps1);
	EditorPageSpec eps2 = {"Passengers", g_Param.hDLL, IDD_EDITOR_PG2, EdPg2Proc};
	SendMessage (hEditor, WM_SCNEDITOR, SE_ADDPAGEBUTTON, (LPARAM)&eps2);
	if (dg->bDamageEnabled) {
		EditorPageSpec eps3 = {"Damage", g_Param.hDLL, IDD_EDITOR_PG3, EdPg3Proc};
		SendMessage (hEditor, WM_SCNEDITOR, SE_ADDPAGEBUTTON, (LPARAM)&eps3);
	}
}

// ==============================================================
// Message callback function for control dialog box
// ==============================================================

INT_PTR CALLBACK Ctrl_DlgProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DeltaGlider *dg = (uMsg == WM_INITDIALOG ? (DeltaGlider*)lParam : (DeltaGlider*)oapiGetDialogContext (hWnd));
	// pointer to vessel instance was passed as dialog context

	switch (uMsg) {
	case WM_INITDIALOG:
		UpdateCtrlDialog (dg, hWnd);
		return FALSE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCANCEL:
			oapiCloseDialog (hWnd);
			return TRUE;
		case IDC_GEAR_UP:
			dg->SubsysGear()->RaiseGear();
			return 0;
		case IDC_GEAR_DOWN:
			dg->SubsysGear()->LowerGear();
			return 0;
		case IDC_RETRO_CLOSE:
			dg->SubsysMainRetro()->CloseRetroCover();
			return 0;
		case IDC_RETRO_OPEN:
			dg->SubsysMainRetro()->OpenRetroCover();
			return 0;
		case IDC_AIRBRAKE_RETRACT:
			if (SendDlgItemMessage (hWnd, IDC_AIRBRAKE_RETRACT, BM_GETCHECK, 0, 0) == BST_CHECKED)
				dg->SubsysAerodyn()->RetractAirbrake();
			return 0;
		case IDC_AIRBRAKE_EXTEND:
			if (SendDlgItemMessage (hWnd, IDC_AIRBRAKE_EXTEND, BM_GETCHECK, 0, 0) == BST_CHECKED)
				dg->SubsysAerodyn()->ExtendAirbrake();
			return 0;
		case IDC_NCONE_CLOSE:
			dg->SubsysDocking()->CloseNcone();
			return 0;
		case IDC_NCONE_OPEN:
			dg->SubsysDocking()->OpenNcone();
			return 0;
		case IDC_OLOCK_CLOSE:
			dg->SubsysPressure()->CloseOuterAirlock();
			return 0;
		case IDC_OLOCK_OPEN:
			dg->SubsysPressure()->OpenOuterAirlock();
			return 0;
		case IDC_ILOCK_CLOSE:
			dg->SubsysPressure()->CloseInnerAirlock();
			return 0;
		case IDC_ILOCK_OPEN:
			dg->SubsysPressure()->OpenInnerAirlock();
			return 0;
		case IDC_LADDER_RETRACT:
			dg->SubsysDocking()->RetractLadder();
			return 0;
		case IDC_LADDER_EXTEND:
			dg->SubsysDocking()->ExtendLadder();
			return 0;
		case IDC_HATCH_CLOSE:
			dg->SubsysPressure()->CloseHatch();
			return 0;
		case IDC_HATCH_OPEN:
			dg->SubsysPressure()->OpenHatch();
			return 0;
		case IDC_RADIATOR_RETRACT:
			dg->SubsysThermal()->CloseRadiator();
			return 0;
		case IDC_RADIATOR_EXTEND:
			dg->SubsysThermal()->OpenRadiator();
			return 0;
		case IDC_INSTRUMENTLIGHT:
			dg->SubsysLights()->InstrumentlightSubsys()->SetLight(SendDlgItemMessage (hWnd, IDC_INSTRUMENTLIGHT, BM_GETCHECK, 0, 0) == BST_CHECKED);
			return 0;
		case IDC_FLOODWLIGHT:
			dg->SubsysLights()->CockpitlightSubsys()->SetLight(SendDlgItemMessage (hWnd, IDC_FLOODWLIGHT, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0);
			break;
		case IDC_FLOODRLIGHT:
			dg->SubsysLights()->CockpitlightSubsys()->SetLight(SendDlgItemMessage (hWnd, IDC_FLOODRLIGHT, BM_GETCHECK, 0, 0) == BST_CHECKED ? 2 : 0);
			break;
		case IDC_DOCKINGLIGHT:
			dg->SubsysLights()->LandDocklightSubsys()->SetLight(SendDlgItemMessage (hWnd, IDC_DOCKINGLIGHT, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0);
			return 0;
		case IDC_LANDINGLIGHT:
			dg->SubsysLights()->LandDocklightSubsys()->SetLight(SendDlgItemMessage (hWnd, IDC_LANDINGLIGHT, BM_GETCHECK, 0, 0) == BST_CHECKED ? 2 : 0);
			return 0;
		case IDC_STROBELIGHT:
			dg->SubsysLights()->StrobelightSubsys()->SetLight(SendDlgItemMessage (hWnd, IDC_STROBELIGHT, BM_GETCHECK, 0, 0) == BST_CHECKED);
			return 0;
		case IDC_NAVLIGHT:
			dg->SubsysLights()->NavlightSubsys()->SetLight(SendDlgItemMessage (hWnd, IDC_NAVLIGHT, BM_GETCHECK, 0, 0) == BST_CHECKED);
			return 0;
		//case IDC_DAMAGE:
		//	oapiOpenDialog (g_Param.hDLL, IDD_DAMAGE, Damage_DlgProc, dg);

		}
		break;
	}
	return oapiDefDialogProc (hWnd, uMsg, wParam, lParam);
}

void UpdateCtrlDialog (DeltaGlider *dg, HWND hWnd)
{
	static int bstatus[2] = {BST_UNCHECKED, BST_CHECKED};

	if (!hWnd) hWnd = oapiFindDialog (g_Param.hDLL, IDD_CTRL);
	if (!hWnd) return;

	int op;

	op = (dg->SubsysGear()->GearState().IsOpening() ? 1 :
		  dg->SubsysGear()->GearState().IsClosing() ? 0 :
		  dg->SubsysGear()->GearState().State() > 0.5 ? 1 : 0);
	SendDlgItemMessage (hWnd, IDC_GEAR_DOWN, BM_SETCHECK, bstatus[op], 0);
	SendDlgItemMessage (hWnd, IDC_GEAR_UP, BM_SETCHECK, bstatus[1-op], 0);

	op = (dg->SubsysMainRetro()->RetroCoverState().IsOpening() ? 1 :
		  dg->SubsysMainRetro()->RetroCoverState().IsClosing() ? 0 :
		  dg->SubsysMainRetro()->RetroCoverState().State() > 0.5 ? 1 : 0);
	SendDlgItemMessage (hWnd, IDC_RETRO_OPEN, BM_SETCHECK, bstatus[op], 0);
	SendDlgItemMessage (hWnd, IDC_RETRO_CLOSE, BM_SETCHECK, bstatus[1-op], 0);

	op = dg->SubsysAerodyn()->AirbrakeSubsys()->TargetState();
	SendDlgItemMessage (hWnd, IDC_AIRBRAKE_RETRACT, BM_SETCHECK, op == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hWnd, IDC_AIRBRAKE_EXTEND, BM_SETCHECK, op == 2 ? BST_CHECKED : BST_UNCHECKED, 0);

	op = (dg->SubsysDocking()->NconeState().IsOpening() ? 1 :
		  dg->SubsysDocking()->NconeState().IsClosing() ? 0 :
		  dg->SubsysDocking()->NconeState().State() > 0.5 ? 1 : 0);
	SendDlgItemMessage (hWnd, IDC_NCONE_OPEN, BM_SETCHECK, bstatus[op], 0);
	SendDlgItemMessage (hWnd, IDC_NCONE_CLOSE, BM_SETCHECK, bstatus[1-op], 0);

	op = (dg->SubsysPressure()->OLockState().IsOpening() ? 1 :
		  dg->SubsysPressure()->OLockState().IsClosing() ? 0 :
		  dg->SubsysPressure()->OLockState().State() > 0.5 ? 1 : 0);
	SendDlgItemMessage (hWnd, IDC_OLOCK_OPEN, BM_SETCHECK, bstatus[op], 0);
	SendDlgItemMessage (hWnd, IDC_OLOCK_CLOSE, BM_SETCHECK, bstatus[1-op], 0);

	op = (dg->SubsysPressure()->ILockState().IsOpening() ? 1 :
		  dg->SubsysPressure()->ILockState().IsClosing() ? 0 :
		  dg->SubsysPressure()->ILockState().State() > 0.5 ? 1 : 0);
	SendDlgItemMessage (hWnd, IDC_ILOCK_OPEN, BM_SETCHECK, bstatus[op], 0);
	SendDlgItemMessage (hWnd, IDC_ILOCK_CLOSE, BM_SETCHECK, bstatus[1-op], 0);

	op = (dg->SubsysDocking()->LadderState().IsOpening() ? 1 :
		  dg->SubsysDocking()->LadderState().IsClosing() ? 0 :
		  dg->SubsysDocking()->LadderState().State() > 0.5 ? 1 : 0);
	SendDlgItemMessage (hWnd, IDC_LADDER_EXTEND, BM_SETCHECK, bstatus[op], 0);
	SendDlgItemMessage (hWnd, IDC_LADDER_RETRACT, BM_SETCHECK, bstatus[1-op], 0);

	op = (dg->SubsysPressure()->HatchState().IsOpening() ? 1 :
		  dg->SubsysPressure()->HatchState().IsClosing() ? 0 :
		  dg->SubsysPressure()->HatchState().State() > 0.5 ? 1 : 0);
	SendDlgItemMessage (hWnd, IDC_HATCH_OPEN, BM_SETCHECK, bstatus[op], 0);
	SendDlgItemMessage (hWnd, IDC_HATCH_CLOSE, BM_SETCHECK, bstatus[1-op], 0);

	op = (dg->SubsysThermal()->RadiatorState().IsOpening() ? 1 :
		  dg->SubsysThermal()->RadiatorState().IsClosing() ? 0 :
		  dg->SubsysThermal()->RadiatorState().State() > 0.5 ? 1 : 0);
	SendDlgItemMessage (hWnd, IDC_RADIATOR_EXTEND, BM_SETCHECK, bstatus[op], 0);
	SendDlgItemMessage (hWnd, IDC_RADIATOR_RETRACT, BM_SETCHECK, bstatus[1-op], 0);

	op = dg->SubsysLights()->InstrumentlightSubsys()->GetLight();
	SendDlgItemMessage (hWnd, IDC_INSTRUMENTLIGHT, BM_SETCHECK, bstatus[op], 0);

	op = dg->SubsysLights()->CockpitlightSubsys()->GetLight();
	SendDlgItemMessage (hWnd, IDC_FLOODWLIGHT, BM_SETCHECK, op == 1 ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hWnd, IDC_FLOODRLIGHT, BM_SETCHECK, op == 2 ? BST_CHECKED : BST_UNCHECKED, 0);

	op = dg->SubsysLights()->LandDocklightSubsys()->GetLight();
	SendDlgItemMessage (hWnd, IDC_DOCKINGLIGHT, BM_SETCHECK, op == 1 ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hWnd, IDC_LANDINGLIGHT, BM_SETCHECK, op == 2 ? BST_CHECKED : BST_UNCHECKED, 0);

	op = dg->beacon[0].active ? 1:0;
	SendDlgItemMessage (hWnd, IDC_NAVLIGHT, BM_SETCHECK, bstatus[op], 0);
	op = dg->beacon[5].active ? 1:0;
	SendDlgItemMessage (hWnd, IDC_STROBELIGHT, BM_SETCHECK, bstatus[op], 0);
}

// ==============================================================
// Message callback function for damage dialog box
// ==============================================================

#ifdef UNDEF
INT_PTR CALLBACK Damage_DlgProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DeltaGlider *dg = (uMsg == WM_INITDIALOG ? (DeltaGlider*)lParam : (DeltaGlider*)oapiGetDialogContext (hWnd));
	// pointer to vessel instance was passed as dialog context

	switch (uMsg) {
	case WM_INITDIALOG:
		return FALSE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCANCEL:
			oapiCloseDialog (hWnd);
			return TRUE;
		case IDC_REPAIR:
			dg->RepairDamage();
			return 0;
		}
		break;
	}
	return oapiDefDialogProc (hWnd, uMsg, wParam, lParam);
}

void UpdateDamageDialog (DeltaGlider *dg, HWND hWnd)
{
	if (!hWnd) hWnd = oapiFindDialog (g_Param.hDLL, IDD_DAMAGE);
	if (!hWnd) return;

	char cbuf[16];
	snprintf (cbuf, sizeof(cbuf) - 1, "%0.0f %%", dg->lwingstatus*100.0);
	SetWindowText (GetDlgItem (hWnd, IDC_LEFTWING_STATUS), cbuf);
	snprintf (cbuf, sizeof(cbuf) - 1, "%0.0f %%", dg->rwingstatus*100.0);
	SetWindowText (GetDlgItem (hWnd, IDC_RIGHTWING_STATUS), cbuf);
}
#endif

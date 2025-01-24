// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ==============================================================
//                 ORBITER MODULE: Atlantis
//                  Part of the ORBITER SDK
//
// Atlantis.cpp
// Reference implementation of Atlantis (Space Shuttle) vessel
// class module
//
// RMS, grappling and MMU capabilities by Robert Conley
// ==============================================================

#define STRICT 1
#define ORBITER_MODULE
#include "Atlantis.h"
#include "PlBayOp.h"
#include "AscentAP.h"
#include "DlgCtrl.h"
#include "meshres.h"
#include "meshres_vc.h"
#include "resource.h"
#include "DrawAPI.h"
#include <stdio.h>
#include <fstream>

using std::min;
using std::max;
using namespace oapi;

#ifdef _DEBUG
    // D. Beachy: for BoundsChecker debugging
    extern int GrowStack();
#endif

#define LOADBMP(id) (LoadBitmap (g_Param.hDLL, MAKEINTRESOURCE (id)))

// ==============================================================
// Global (class-wide) parameters

GDIParams g_Param;

const char *ActionString[5] = {"STOPPED", "ISCLOSED", "ISOPEN", "CLOSE", "OPEN"};

HELPCONTEXT g_hc = {
	(char*)"html/vessels/Atlantis.chm",
	0,
	(char*)"html/vessels/Atlantis.chm::/Atlantis.hhc",
	(char*)"html/vessels/Atlantis.chm::/Atlantis.hhk"
};


// ==============================================================
// Local prototypes

INT_PTR CALLBACK Atlantis_DlgProc (HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK RMS_DlgProc (HWND, UINT, WPARAM, LPARAM);
extern void GetSRB_State (double met, double &thrust_level, double &prop_level);

// ==============================================================
// Specialised vessel class Atlantis
// ==============================================================

// --------------------------------------------------------------
// Constructor
// --------------------------------------------------------------
Atlantis::Atlantis (OBJHANDLE hObj, int fmodel)
: VESSEL4 (hObj, fmodel)
{
#ifdef _DEBUG
        // D. Beachy: for BoundsChecker debugging
        GrowStack();
#endif
	int i;

	plop            = new PayloadBayOp (this);
	ascap           = new AscentAP (this);
	ascapMfdId      = RegisterAscentApMfd ();
	status          = 3;
	gear_status     = AnimState::CLOSED;
	gear_proc       = 0.0;
	ldoor_drag      = rdoor_drag = 0.0;
	spdb_status     = AnimState::CLOSED;
	spdb_proc       = 0.0;

	engine_light_level = 0.0;
	COLOUR4 col_diff = {1,0.8,0.8,0};
	COLOUR4 col_zero = {0,0,0,0};
	engine_light = AddPointLight (_V(0,0,-25), 300, 2e-4, 0, 4e-4, col_diff, col_zero, col_zero);
	engine_light->SetIntensityRef (&engine_light_level);

	LoadMeshes();
	reset_sat       = false;
	render_cockpit  = false;

	for (i = 0; i < 10; i++)
		mfdbright[i] = 1.0;

	// propellant resources
	ph_oms  = CreatePropellantResource (ORBITER_MAX_PROPELLANT_MASS); // OMS propellant
	SetDefaultPropellantResource (ph_oms); // display OMS tank level in generic HUD

	// Orbiter engines	
	CreateSSME(); // main thrusters
	CreateOMS();  // OMS thrusters (activated only after tank separation)
	CreateRCS();  // Reaction control system (activated only after tank separation)

	// Animations
	DefineAnimations();

	// Aerodynamics
	CreateAirfoils();

	CreateDock (ORBITER_DOCKPOS, _V(0,1,0), _V(0,0,-1));
	hDockET = CreateDock (_V(0.0,-2.48, 8.615), _V(0,-1,0), _V(0,0,1));
	pET = NULL; // ET reference

	center_arm      = false;
	arm_moved       = arm_scheduled = false;
	bManualSeparate = false;
	ofs_sts_sat     = _V(0,0,0);      
	do_eva          = false;
	do_plat         = false;
	do_cargostatic  = false;
	vis             = NULL;
	cargo_static_ofs   =_V(0,0,0);

	// default arm status: stowed
	arm_sy = 0.5;
	arm_sp = 0.0;
	arm_ep = 0.0;
	arm_wp = 0.5;
	arm_wy = 0.5;
	arm_wr = 0.5;
	arm_tip[0] = _V(-2.26,1.71,-6.5);
	arm_tip[1] = _V(-2.26,1.71,-7.5);
	arm_tip[2] = _V(-2.26,2.71,-6.5);

	sat_attach = CreateAttachment (false, ofs_sts_sat, _V(0,1,0), _V(0,0,1), "X");
	rms_attach = CreateAttachment (false, arm_tip[0], arm_tip[1]-arm_tip[0], arm_tip[2]-arm_tip[0], "G", true);

	// Entry particle stream
	PARTICLESTREAMSPEC rps = {
		0, 20, 20, 0, 0.03, 0.5, 100, 3, PARTICLESTREAMSPEC::DIFFUSE,
		PARTICLESTREAMSPEC::LVL_FLAT, 1, 1, PARTICLESTREAMSPEC::ATM_PLIN, 6e7, 12e7
	};
	AddReentryStream (&rps);

	ascentApDlg = 0;
}

// --------------------------------------------------------------
// Destructor
// --------------------------------------------------------------
Atlantis::~Atlantis ()
{
	UnregisterMFDMode (ascapMfdId);
	if (ascentApDlg) delete ascentApDlg;
	delete ascap;
	delete plop;
	int i;
	for (i = 0; i < 6; i++) delete rms_anim[i];
}

// --------------------------------------------------------------
// Load the meshes for cockpit and exterior
// --------------------------------------------------------------
void Atlantis::LoadMeshes()
{
	// Retrieve mesh handles
	hOrbiterMesh        = oapiLoadMeshGlobal ("Atlantis\\Atlantis");
	hOrbiterCockpitMesh = oapiLoadMeshGlobal ("Atlantis\\AtlantisCockpit");
	hOrbiterVCMesh      = oapiLoadMeshGlobal ("Atlantis\\AtlantisVC");

	// Load meshes
	mesh_cockpit = AddMesh (hOrbiterCockpitMesh);
	SetMeshVisibilityMode (mesh_cockpit, MESHVIS_EXTERNAL);

	mesh_orbiter = AddMesh (hOrbiterMesh);
	SetMeshVisibilityMode (mesh_orbiter, MESHVIS_EXTERNAL|MESHVIS_VC|MESHVIS_EXTPASS);

	mesh_vc = AddMesh (hOrbiterVCMesh);
	SetMeshVisibilityMode (mesh_vc, MESHVIS_VC);

	// Optonal meshes
	mesh_cargo      = NULL;
	mesh_platform   = NULL;

	// Visual handle
	vis             = NULL;
}

// --------------------------------------------------------------
// Initialise the thrusters for the shuttle main engines
// --------------------------------------------------------------
void Atlantis::CreateSSME()
{
	// Not connected to a propellant resource - they connect to the ET's tank as long
	// as it is attached (checked in clbkPreStep)
	th_main[0] = CreateThruster (THRUSTREF_SSME0, THRUSTGIMBAL_LAUNCH, ORBITER_MAIN_THRUST, NULL, ORBITER_MAIN_ISP0, ORBITER_MAIN_ISP1);
	th_main[1] = CreateThruster (THRUSTREF_SSME1, THRUSTGIMBAL_LAUNCH, ORBITER_MAIN_THRUST, NULL, ORBITER_MAIN_ISP0, ORBITER_MAIN_ISP1);
	th_main[2] = CreateThruster (THRUSTREF_SSME2, THRUSTGIMBAL_LAUNCH, ORBITER_MAIN_THRUST, NULL, ORBITER_MAIN_ISP0, ORBITER_MAIN_ISP1);
	thg_main = CreateThrusterGroup (th_main, 3, THGROUP_MAIN);
	gimbal_pos = THRUSTGIMBAL_LAUNCH; // the initial pitch gimbal setting positions the SSMEs to cancel pitch moment in launch configuration

	SURFHANDLE tex_main = oapiRegisterExhaustTexture ((char*)"Exhaust_atsme");
	for (int i = 0; i < 3; i++)
		AddExhaust (th_main[i], 30.0, 2.0, tex_main);
}

// --------------------------------------------------------------
// Initialise the thrusters for the orbital maneuvering system
// --------------------------------------------------------------
void Atlantis::CreateOMS()
{
	th_oms[0] = CreateThruster (THRUSTREF_OMSL, THRUSTDIR_OMSL, ORBITER_OMS_THRUST, ph_oms, ORBITER_OMS_ISP0, ORBITER_OMS_ISP1);
	th_oms[1] = CreateThruster (THRUSTREF_OMSR, THRUSTDIR_OMSR, ORBITER_OMS_THRUST, ph_oms, ORBITER_OMS_ISP0, ORBITER_OMS_ISP1);
	// we don't yet define a thruster group for the OMS engines
	// They will be assigned to the MAIN group as soon as the ET is jettisoned
	for (int i = 0; i < 2; i++)
		AddExhaust (th_oms[i], 4.0, 0.5);
}

// --------------------------------------------------------------
// Attitude controls (RCS) during orbital phase
// Inactive by default. Activated with EnableRCS()
// --------------------------------------------------------------
void Atlantis::CreateRCS()
{
	SURFHANDLE tex_rcs = oapiRegisterExhaustTexture ((char*)"Exhaust_atrcs");
	const double eh = 6.0;             // exhaust length scale
	const double ew1 = 0.4, ew2 = 0.8; // exhaust width scales

	// set of attitude thrusters (idealised). The arrangement is such that no angular
	// momentum is created in linear mode, and no linear momentum is created in rotational mode.
	THRUSTER_HANDLE th_att_rot[4], th_att_lin[4];
	th_att_rot[0] = th_att_lin[0] = CreateThruster (_V(0,0, 15.5), _V(0, 1,0), ORBITER_RCS_THRUST, NULL, ORBITER_RCS_ISP0, ORBITER_RCS_ISP1);
	th_att_rot[1] = th_att_lin[3] = CreateThruster (_V(0,0,-15.5), _V(0,-1,0), ORBITER_RCS_THRUST, NULL, ORBITER_RCS_ISP0, ORBITER_RCS_ISP1);
	th_att_rot[2] = th_att_lin[2] = CreateThruster (_V(0,0, 15.5), _V(0,-1,0), ORBITER_RCS_THRUST, NULL, ORBITER_RCS_ISP0, ORBITER_RCS_ISP1);
	th_att_rot[3] = th_att_lin[1] = CreateThruster (_V(0,0,-15.5), _V(0, 1,0), ORBITER_RCS_THRUST, NULL, ORBITER_RCS_ISP0, ORBITER_RCS_ISP1);
	CreateThrusterGroup (th_att_rot,   2, THGROUP_ATT_PITCHUP);
	CreateThrusterGroup (th_att_rot+2, 2, THGROUP_ATT_PITCHDOWN);
	CreateThrusterGroup (th_att_lin,   2, THGROUP_ATT_UP);
	CreateThrusterGroup (th_att_lin+2, 2, THGROUP_ATT_DOWN);

	AddExhaust (th_att_rot[0], eh, ew1, _V( 1.60,-0.20, 18.78), _V( 0.4339,-0.8830,-0.1793), tex_rcs);//F2D
	AddExhaust (th_att_rot[0], eh, ew1, _V( 1.68,-0.18, 18.40), _V( 0.4339,-0.8830,-0.1793), tex_rcs);//F4D
	AddExhaust (th_att_rot[0], eh, ew1, _V(-1.55,-0.20, 18.78), _V(-0.4339,-0.8830,-0.1793), tex_rcs);//F1D
	AddExhaust (th_att_rot[0], eh, ew1, _V(-1.63,-0.18, 18.40), _V(-0.4339,-0.8830,-0.1793), tex_rcs);//F3D

	AddExhaust (th_att_rot[1], eh, ew1, _V(-3.46, 3.20,-12.30), _V(0, 1,0), tex_rcs);//L4U
	AddExhaust (th_att_rot[1], eh, ew1, _V(-3.46, 3.20,-12.70), _V(0, 1,0), tex_rcs);//L2U
	AddExhaust (th_att_rot[1], eh, ew1, _V(-3.46, 3.20,-13.10), _V(0, 1,0), tex_rcs);//L1U

	AddExhaust (th_att_rot[1], eh, ew1, _V( 3.43, 3.20,-12.30), _V(0, 1,0), tex_rcs);//R4U
	AddExhaust (th_att_rot[1], eh, ew1, _V( 3.43, 3.20,-12.70), _V(0, 1,0), tex_rcs);//R2U
	AddExhaust (th_att_rot[1], eh, ew1, _V( 3.43, 3.20,-13.10), _V(0, 1,0), tex_rcs);//R1U

	AddExhaust (th_att_rot[2], eh, ew1, _V(-0.4 , 1.10, 18.3 ), _V(0, 1,0), tex_rcs);//F1U	
	AddExhaust (th_att_rot[2], eh, ew1, _V( 0.0 , 1.15 ,18.3 ), _V(0, 1,0), tex_rcs);//F3U
	AddExhaust (th_att_rot[2], eh, ew1, _V( 0.4 , 1.10, 18.3 ), _V(0, 1,0), tex_rcs);//F2U

	AddExhaust (th_att_rot[3], eh, ew1, _V(-3.1 , 1.55,-12.45), _V(-0.2844,-0.9481,-0.1422), tex_rcs);//L4D
	AddExhaust (th_att_rot[3], eh, ew1, _V(-3.1 , 1.6 ,-12.8 ), _V(-0.2844,-0.9481,-0.1422), tex_rcs);//L2D
	AddExhaust (th_att_rot[3], eh, ew1, _V(-3.1 , 1.65,-13.15), _V(-0.2844,-0.9481,-0.1422), tex_rcs);//L3D

	AddExhaust (th_att_rot[3], eh, ew1, _V( 3.15, 1.55,-12.45), _V( 0.2844,-0.9481,-0.1422), tex_rcs);//R4D
	AddExhaust (th_att_rot[3], eh, ew1, _V( 3.15, 1.6 ,-12.8 ), _V( 0.2844,-0.9481,-0.1422), tex_rcs);//R2D
	AddExhaust (th_att_rot[3], eh, ew1, _V( 3.15, 1.65,-13.15), _V( 0.2844,-0.9481,-0.1422), tex_rcs);//R3D

	th_att_rot[0] = th_att_lin[0] = CreateThruster (_V(0,0, 15.5), _V(-1,0,0), ORBITER_RCS_THRUST, NULL, ORBITER_RCS_ISP0, ORBITER_RCS_ISP1);
	th_att_rot[1] = th_att_lin[3] = CreateThruster (_V(0,0,-15.5), _V( 1,0,0), ORBITER_RCS_THRUST, NULL, ORBITER_RCS_ISP0, ORBITER_RCS_ISP1);
	th_att_rot[2] = th_att_lin[2] = CreateThruster (_V(0,0, 15.5), _V( 1,0,0), ORBITER_RCS_THRUST, NULL, ORBITER_RCS_ISP0, ORBITER_RCS_ISP1);
	th_att_rot[3] = th_att_lin[1] = CreateThruster (_V(0,0,-15.5), _V(-1,0,0), ORBITER_RCS_THRUST, NULL, ORBITER_RCS_ISP0, ORBITER_RCS_ISP1);
	CreateThrusterGroup (th_att_rot,   2, THGROUP_ATT_YAWLEFT);
	CreateThrusterGroup (th_att_rot+2, 2, THGROUP_ATT_YAWRIGHT);
	CreateThrusterGroup (th_att_lin,   2, THGROUP_ATT_LEFT);
	CreateThrusterGroup (th_att_lin+2, 2, THGROUP_ATT_RIGHT);

	AddExhaust (th_att_rot[0], eh, ew2, _V( 1.8 ,-0.3 , 18.0 ), _V( 1,0,0), tex_rcs);//F4R
	AddExhaust (th_att_rot[0], eh, ew2, _V( 1.75, 0.1 , 18.05), _V( 1,0,0), tex_rcs);//F2R
	AddExhaust (th_att_rot[2], eh, ew2, _V(-1.7 ,-0.3 , 18.0 ), _V(-1,0,0), tex_rcs);//F1L
	AddExhaust (th_att_rot[2], eh, ew2, _V(-1.65,-0.1 , 18.05), _V(-1,0,0), tex_rcs);//F3L

	AddExhaust (th_att_rot[1], eh, ew2, _V(-4.0 , 2.35,-12.35), _V(-1,0,0), tex_rcs);//L4L
	AddExhaust (th_att_rot[1], eh, ew2, _V(-4.0 , 2.35,-12.6 ), _V(-1,0,0), tex_rcs);//L2L
	AddExhaust (th_att_rot[1], eh, ew2, _V(-4.0 , 2.35,-13.0 ), _V(-1,0,0), tex_rcs);//L3L
	AddExhaust (th_att_rot[1], eh, ew2, _V(-4.0 , 2.35,-13.35), _V(-1,0,0), tex_rcs);//L1L

	AddExhaust (th_att_rot[3], eh, ew2, _V( 4.0 , 2.35,-12.35), _V( 1,0,0), tex_rcs);//R4R
	AddExhaust (th_att_rot[3], eh, ew2, _V( 4.0 , 2.35,-12.6 ), _V( 1,0,0), tex_rcs);//R2R
	AddExhaust (th_att_rot[3], eh, ew2, _V( 4.0 , 2.35,-13.0 ), _V( 1,0,0), tex_rcs);//R3R
	AddExhaust (th_att_rot[3], eh, ew2, _V( 4.0,  2.35,-13.35), _V( 1,0,0), tex_rcs);//R1R

	th_att_rot[0] = CreateThruster (_V( 2.7,0,0), _V(0, 1,0), ORBITER_RCS_THRUST, NULL, ORBITER_RCS_ISP0, ORBITER_RCS_ISP1);
	th_att_rot[1] = CreateThruster (_V(-2.7,0,0), _V(0,-1,0), ORBITER_RCS_THRUST, NULL, ORBITER_RCS_ISP0, ORBITER_RCS_ISP1);
	th_att_rot[2] = CreateThruster (_V(-2.7,0,0), _V(0, 1,0), ORBITER_RCS_THRUST, NULL, ORBITER_RCS_ISP0, ORBITER_RCS_ISP1);
	th_att_rot[3] = CreateThruster (_V( 2.7,0,0), _V(0,-1,0), ORBITER_RCS_THRUST, NULL, ORBITER_RCS_ISP0, ORBITER_RCS_ISP1);
	CreateThrusterGroup (th_att_rot,   2, THGROUP_ATT_BANKLEFT);
	CreateThrusterGroup (th_att_rot+2, 2, THGROUP_ATT_BANKRIGHT);

	AddExhaust (th_att_rot[0], eh, ew1, _V( 1.60,-0.20, 18.78), _V( 0.4339,-0.8830,-0.1793), tex_rcs);//F2D
	AddExhaust (th_att_rot[0], eh, ew1, _V( 1.68,-0.18, 18.40), _V( 0.4339,-0.8830,-0.1793), tex_rcs);//F4D
	AddExhaust (th_att_rot[2], eh, ew1, _V(-1.55,-0.20, 18.78), _V(-0.4339,-0.8830,-0.1793), tex_rcs);//F1D
	AddExhaust (th_att_rot[2], eh, ew1, _V(-1.63,-0.18, 18.40), _V(-0.4339,-0.8830,-0.1793), tex_rcs);//F3D

	AddExhaust (th_att_rot[1], eh, ew1, _V(-3.46, 3.20,-12.30), _V(0, 1,0), tex_rcs);//L4U
	AddExhaust (th_att_rot[1], eh, ew1, _V(-3.46, 3.20,-12.70), _V(0, 1,0), tex_rcs);//L2U
	AddExhaust (th_att_rot[1], eh, ew1, _V(-3.46, 3.20,-13.10), _V(0, 1,0), tex_rcs);//L1U

	AddExhaust (th_att_rot[3], eh, ew1, _V( 3.43, 3.20,-12.30), _V(0, 1,0), tex_rcs);//R4U
	AddExhaust (th_att_rot[3], eh, ew1, _V( 3.43, 3.20,-12.70), _V(0, 1,0), tex_rcs);//R2U
	AddExhaust (th_att_rot[3], eh, ew1, _V( 3.43, 3.20,-13.10), _V(0, 1,0), tex_rcs);//R1U

	AddExhaust (th_att_rot[2], eh, ew1, _V(-3.1 , 1.55,-12.45), _V(-0.2844,-0.9481,-0.1422), tex_rcs);//L4D
	AddExhaust (th_att_rot[2], eh, ew1, _V(-3.1 , 1.6 ,-12.8 ), _V(-0.2844,-0.9481,-0.1422), tex_rcs);//L2D
	AddExhaust (th_att_rot[2], eh, ew1, _V(-3.1 , 1.65,-13.15), _V(-0.2844,-0.9481,-0.1422), tex_rcs);//L3D

	AddExhaust (th_att_rot[0], eh, ew1, _V( 3.15, 1.55,-12.45), _V( 0.2844,-0.9481,-0.1422), tex_rcs);//R4D
	AddExhaust (th_att_rot[0], eh, ew1, _V( 3.15, 1.6 ,-12.8 ), _V( 0.2844,-0.9481,-0.1422), tex_rcs);//R2D
	AddExhaust (th_att_rot[0], eh, ew1, _V( 3.15, 1.65,-13.15), _V( 0.2844,-0.9481,-0.1422), tex_rcs);//R3D

	th_att_lin[0] = CreateThruster (_V(0,0,-16), _V(0,0, 1), ORBITER_RCS_THRUST, NULL, ORBITER_RCS_ISP0, ORBITER_RCS_ISP1);
	th_att_lin[1] = CreateThruster (_V(0,0, 16), _V(0,0,-1), ORBITER_RCS_THRUST, NULL, ORBITER_RCS_ISP0, ORBITER_RCS_ISP1);
	CreateThrusterGroup (th_att_lin,   1, THGROUP_ATT_FORWARD);
	CreateThrusterGroup (th_att_lin+1, 1, THGROUP_ATT_BACK);

	AddExhaust (th_att_lin[0], eh, ew1, _V(-3.59, 2.8 ,-13.6 ), _V(0,0,-1), tex_rcs);//L1A
	AddExhaust (th_att_lin[0], eh, ew1, _V(-3.27, 2.8 ,-13.6 ), _V(0,0,-1), tex_rcs);//L3A
	AddExhaust (th_att_lin[0], eh, ew1, _V( 3.64, 2.8 ,-13.6 ), _V(0,0,-1), tex_rcs);//R1A
	AddExhaust (th_att_lin[0], eh, ew1, _V( 3.27, 2.8 ,-13.6 ), _V(0,0,-1), tex_rcs);//R3A

	AddExhaust (th_att_lin[1], eh, ew1, _V( 0.0 , 0.75, 19.2 ), _V(0, 0.0499, 0.9988), tex_rcs);//F3F
	AddExhaust (th_att_lin[1], eh, ew1, _V(-0.4 , 0.7 , 19.2 ), _V(0, 0.0499, 0.9988), tex_rcs);//F1F
	AddExhaust (th_att_lin[1], eh, ew1, _V( 0.4 , 0.7 , 19.2 ), _V(0, 0.0499, 0.9988), tex_rcs);//F2F
}

// --------------------------------------------------------------
// Initialise airfoils, aerodynamic control surfaces and drag elements
// --------------------------------------------------------------
void Atlantis::CreateAirfoils ()
{
	CreateAirfoil (LIFT_VERTICAL,   _V(0,0,-0.5), VLiftCoeff, 20, 270, 2.266);
	CreateAirfoil (LIFT_HORIZONTAL, _V(0,0,-4), HLiftCoeff, 20,  50, 1.5);

	CreateControlSurface (AIRCTRL_ELEVATOR, 5.0, 1.5, _V( 0, 0,  -15), AIRCTRL_AXIS_XPOS, anim_elev);
	CreateControlSurface (AIRCTRL_RUDDER,   2.0, 1.5, _V( 0, 3,  -16), AIRCTRL_AXIS_YPOS, anim_rudder);
	CreateControlSurface (AIRCTRL_AILERON,  3.0, 1.5, _V( 7,-0.5,-15), AIRCTRL_AXIS_XPOS, anim_raileron);
	CreateControlSurface (AIRCTRL_AILERON,  3.0, 1.5, _V(-7,-0.5,-15), AIRCTRL_AXIS_XNEG, anim_laileron);

	CreateVariableDragElement (&spdb_proc, 5, _V(0, 7.5, -14)); // speedbrake drag
	CreateVariableDragElement (&gear_proc, 2, _V(0,-3,0));      // landing gear drag
	CreateVariableDragElement (&rdoor_drag, 7, _V(2.9,0,10));   // right cargo door drag
	CreateVariableDragElement (&ldoor_drag, 7, _V(-2.9,0,10));  // right cargo door drag
}

// --------------------------------------------------------------
// Airfoil coefficient function
// Return lift, moment and zero-lift drag coefficients as a
// function of angle of attack
// 1. vertical lift component (wings and body)
// --------------------------------------------------------------
void Atlantis::VLiftCoeff (double aoa, double M, double Re, double *cl, double *cm, double *cd)
{
	static const double step = RAD*15.0;
	static const double istep = 1.0/step;
	static const int nabsc = 25;
	static const double CL[nabsc] = {0.1, 0.17, 0.2, 0.2, 0.17, 0.1, 0, -0.11, -0.24, -0.38,  -0.5,  -0.5, -0.02, 0.6355,    0.63,   0.46, 0.28, 0.13, 0.0, -0.16, -0.26, -0.29, -0.24, -0.1, 0.1};
	static const double CM[nabsc] = {  0,    0,   0,   0,    0,   0, 0,     0,    0,0.002,0.004, 0.0025,0.0012,      0,-0.0012,-0.0007,    0,    0,   0,     0,     0,     0,     0,    0,   0};
	// lift and moment coefficients from -180 to 180 in 15 degree steps.
	// This uses a documented lift slope of 0.0437/deg, everything else is rather ad-hoc

	aoa += PI;
	int idx = max (0, min (23, (int)(aoa*istep)));
	double d = aoa*istep - idx;
	*cl = CL[idx] + (CL[idx+1]-CL[idx])*d;
	*cm = CM[idx] + (CM[idx+1]-CM[idx])*d;
	*cd = 0.055 + oapiGetInducedDrag (*cl, 2.266, 0.6);
}

// --------------------------------------------------------------
// Airfoil coefficient functions
// Return lift, moment and zero-lift drag coefficients as a
// function of slip angle (beta)
// 2. horizontal lift component (vertical stabiliser and body)
// --------------------------------------------------------------
void Atlantis::HLiftCoeff (double beta, double M, double Re, double *cl, double *cm, double *cd)
{
	static const double step = RAD*22.5;
	static const double istep = 1.0/step;
	static const int nabsc = 17;
	static const double CL[nabsc] = {0, 0.2, 0.3, 0.2, 0, -0.2, -0.3, -0.2, 0, 0.2, 0.3, 0.2, 0, -0.2, -0.3, -0.2, 0};

	beta += PI;
	int idx = max (0, min (15, (int)(beta*istep)));
	double d = beta*istep - idx;
	*cl = CL[idx] + (CL[idx+1]-CL[idx])*d;
	*cm = 0.0;
	*cd = 0.02 + oapiGetInducedDrag (*cl, 1.5, 0.6);
}

// --------------------------------------------------------------
// Enable/disable Space Shuttle Main Engines
// --------------------------------------------------------------
bool Atlantis::EnableSSME (bool enable)
{
	PROPELLANT_HANDLE hProp = NULL;

	if (enable) {
		if (pET) hProp = pET->GetPropHandle();
		else return false; // can't activate without attached ET
	}

	for (DWORD i = 0; i < 3; i++)
		SetThrusterResource (th_main[i], hProp);

	if (enable) {
		if (GetGroupThrusterCount (THGROUP_MAIN) != 3) {
			// switch MAIN group to SSME
			DelThrusterGroup (THGROUP_MAIN);
			thg_main = CreateThrusterGroup (th_main, 3, THGROUP_MAIN);
		}
		SetDefaultPropellantResource (hProp);
	}
	return true;
}

// --------------------------------------------------------------
// Enable/disable Orbital Maneuvering System
// --------------------------------------------------------------
void Atlantis::EnableOMS (bool enable)
{
	PROPELLANT_HANDLE hProp = (enable ? ph_oms : NULL);

	for (DWORD i = 0; i < 2; i++)
		SetThrusterResource (th_oms[i], hProp);

	if (enable) {
		if (GetGroupThrusterCount (THGROUP_MAIN) > 2) {
			// switch MAIN group to OMS
			DelThrusterGroup (THGROUP_MAIN);
			thg_main = CreateThrusterGroup (th_oms, 2, THGROUP_MAIN);
		}
		SetDefaultPropellantResource (ph_oms);
	}
}

// --------------------------------------------------------------
// Enable/disable Reaction Control System
// --------------------------------------------------------------
void Atlantis::EnableRCS (int mode)
{
	bool enable = (mode != RCS_NONE);

	PROPELLANT_HANDLE hProp = (enable ? ph_oms : NULL);
	DWORD i;

	for (i = 0; i < GetGroupThrusterCount (THGROUP_ATT_PITCHUP); i++)
		SetThrusterResource (GetGroupThruster (THGROUP_ATT_PITCHUP, i), hProp);
	for (i = 0; i < GetGroupThrusterCount (THGROUP_ATT_PITCHDOWN); i++)
		SetThrusterResource (GetGroupThruster (THGROUP_ATT_PITCHDOWN, i), hProp);

	for (i = 0; i < GetGroupThrusterCount (THGROUP_ATT_YAWLEFT); i++)
		SetThrusterResource (GetGroupThruster (THGROUP_ATT_YAWLEFT, i), hProp);
	for (i = 0; i < GetGroupThrusterCount (THGROUP_ATT_YAWRIGHT); i++)
		SetThrusterResource (GetGroupThruster (THGROUP_ATT_YAWRIGHT, i), hProp);

	for (i = 0; i < GetGroupThrusterCount (THGROUP_ATT_BANKLEFT); i++)
		SetThrusterResource (GetGroupThruster (THGROUP_ATT_BANKLEFT, i), hProp);
	for (i = 0; i < GetGroupThrusterCount (THGROUP_ATT_BANKRIGHT); i++)
		SetThrusterResource (GetGroupThruster (THGROUP_ATT_BANKRIGHT, i), hProp);

	for (i = 0; i < GetGroupThrusterCount (THGROUP_ATT_UP); i++)
		SetThrusterResource (GetGroupThruster (THGROUP_ATT_UP, i), hProp);
	for (i = 0; i < GetGroupThrusterCount (THGROUP_ATT_DOWN); i++)
		SetThrusterResource (GetGroupThruster (THGROUP_ATT_DOWN, i), hProp);

	for (i = 0; i < GetGroupThrusterCount (THGROUP_ATT_LEFT); i++)
		SetThrusterResource (GetGroupThruster (THGROUP_ATT_LEFT, i), hProp);
	for (i = 0; i < GetGroupThrusterCount (THGROUP_ATT_RIGHT); i++)
		SetThrusterResource (GetGroupThruster (THGROUP_ATT_RIGHT, i), hProp);

	for (i = 0; i < GetGroupThrusterCount (THGROUP_ATT_FORWARD); i++)
		SetThrusterResource (GetGroupThruster (THGROUP_ATT_FORWARD, i), hProp);
	for (i = 0; i < GetGroupThrusterCount (THGROUP_ATT_BACK); i++)
		SetThrusterResource (GetGroupThruster (THGROUP_ATT_BACK, i), hProp);

	SetAttitudeMode(mode);
}

// --------------------------------------------------------------
// Define animation sequences for moving parts
// --------------------------------------------------------------
void Atlantis::DefineAnimations (void)
{
	UINT midx = 1; // mesh index for all external animations
	UINT vidx = 2; // mesh index for all VC animations

	// ***** 1. Cargo door and radiator animations *****

	static UINT RCargoDoorGrp[4] = {GRP_cargodooroutR,GRP_cargodoorinR,GRP_radiatorFR,GRP_radiatorBR};
	static MGROUP_ROTATE RCargoDoor (midx, RCargoDoorGrp, 4,
		_V(2.88, 1.3, 0), _V(0,0,1), (float)(-175.5*RAD));
	static UINT LCargoDoorGrp[4] = {GRP_cargodooroutL,GRP_cargodoorinL,GRP_radiatorFL,GRP_radiatorBL};
	static MGROUP_ROTATE LCargoDoor (midx, LCargoDoorGrp, 4,
		_V(-2.88, 1.3, 0), _V(0,0,1), (float)(175.5*RAD));

	anim_door = CreateAnimation (0);
	AddAnimationComponent (anim_door, 0.0, 0.4632, &RCargoDoor);
	AddAnimationComponent (anim_door, 0.5368, 1.0, &LCargoDoor);

	static UINT RRadiatorGrp[1] = {GRP_radiatorFR};
	static MGROUP_ROTATE RRadiator (midx, RRadiatorGrp, 1,
		_V(2.88, 1.3, 0), _V(0,0,1), (float)(35.5*RAD));
	static UINT LRadiatorGrp[1] = {GRP_radiatorFL};
	static MGROUP_ROTATE LRadiator (midx, LRadiatorGrp, 1,
		_V(-2.88, 1.3, 0), _V(0,0,1), (float)(-35.5*RAD));

	anim_rad = CreateAnimation (0);
	AddAnimationComponent (anim_rad, 0, 1, &RRadiator);
	AddAnimationComponent (anim_rad, 0, 1, &LRadiator);

	// ***** 2. Landing gear animation *****

	static UINT LNosewheelDoorGrp[1] = {GRP_nosedoorL};
	static MGROUP_ROTATE LNosewheelDoor (midx, LNosewheelDoorGrp, 1,
		_V(-0.78, -2.15, 17), _V(0, 0.195, 0.981), (float)(-60.0*RAD));
	static UINT RNosewheelDoorGrp[1] = {GRP_nosedoorR};
	static MGROUP_ROTATE RNosewheelDoor (midx, RNosewheelDoorGrp, 1,
		_V(0.78, -2.15, 17), _V(0, 0.195, 0.981), (float)(60.0*RAD));
	static UINT NosewheelGrp[2] = {GRP_nosewheel,GRP_nosegear};
	static MGROUP_ROTATE Nosewheel (midx, NosewheelGrp, 2,
		_V(0.0, -1.95, 17.45), _V(1, 0, 0), (float)(109.0*RAD));
	static UINT RGearDoorGrp[1] = {GRP_geardoorR};
	static MGROUP_ROTATE RGearDoor (midx, RGearDoorGrp, 1,
		_V(4.35, -2.64, -1.69), _V(0, 0.02, 0.9), (float)(96.2*RAD));
	static UINT LGearDoorGrp[1] = {GRP_geardoorL};
	static MGROUP_ROTATE LGearDoor (midx, LGearDoorGrp, 1,
		_V(-4.35, -2.64, -1.69), _V(0, 0.02, 0.9), (float)(-96.2*RAD));
	static UINT MainGearGrp[4] = {GRP_wheelR,GRP_gearR,GRP_wheelL,GRP_gearL};
	static MGROUP_ROTATE MainGear (midx, MainGearGrp, 4,
		_V(0, -2.66, -3.68), _V(1, 0, 0), (float)(94.5*RAD));

	anim_gear = CreateAnimation (0);
	AddAnimationComponent (anim_gear, 0,   0.5, &LNosewheelDoor);
	AddAnimationComponent (anim_gear, 0,   0.5, &RNosewheelDoor);
	AddAnimationComponent (anim_gear, 0.4, 1.0, &Nosewheel);
	AddAnimationComponent (anim_gear, 0,   0.5, &RGearDoor);
	AddAnimationComponent (anim_gear, 0,   0.5, &LGearDoor);
	AddAnimationComponent (anim_gear, 0.4, 1.0, &MainGear);

	// ***** 3. Ku-band antenna animation *****

	static UINT KuBand1Grp[3] = {GRP_startrackers,GRP_KUband1,GRP_KUband2};
	static MGROUP_ROTATE KuBand1 (midx, KuBand1Grp, 3,
		_V(2.85, 0.85, 0), _V(0,0,1), (float)(-18*RAD));
	static UINT KuBand2Grp[1] = {GRP_KUband2};
	static MGROUP_ROTATE KuBand2 (midx, KuBand2Grp, 1,
		_V(2.78, 1.7, 0), _V(0,0,1), (float)(-90*RAD));
	static UINT KuBand3Grp[2] = {GRP_KUband1,GRP_KUband2};
	static MGROUP_ROTATE KuBand3 (midx, KuBand3Grp, 2,
		_V(2.75, 2.05, 11.47), _V(0,1,0), (float)(-113*RAD));

	anim_kubd = CreateAnimation (0);
	AddAnimationComponent (anim_kubd, 0,     0.333, &KuBand1);
	AddAnimationComponent (anim_kubd, 0.333, 0.667, &KuBand2);
	AddAnimationComponent (anim_kubd, 0.667, 0.999, &KuBand3);

	// ***** 4. Elevator animation of elevons *****

	static UINT ElevGrp[4] = {GRP_flapR,GRP_flapL,GRP_aileronL,GRP_aileronR};
	static MGROUP_ROTATE Elevator (midx, ElevGrp, 4,
		_V(0,-2.173,-8.84), _V(1,0,0), (float)(30.0*RAD));
	anim_elev = CreateAnimation (0.5);
	AddAnimationComponent (anim_elev, 0, 1, &Elevator);

	// ***** 5. Aileron animation of elevons *****

	static UINT LAileronGrp[2] = {GRP_flapL,GRP_aileronL};
	static MGROUP_ROTATE LAileron (midx, LAileronGrp, 2,
		_V(0,-2.173,-8.84), _V(-1,0,0), (float)(10.0*RAD));
	static UINT RAileronGrp[2] = {GRP_flapR,GRP_aileronR};
	static MGROUP_ROTATE RAileron (midx, RAileronGrp, 2,
		_V(0,-2.173,-8.84), _V(1,0,0), (float)(10.0*RAD));
	anim_laileron = CreateAnimation (0.5);
	AddAnimationComponent (anim_laileron, 0, 1, &LAileron);
	anim_raileron = CreateAnimation (0.5);
	AddAnimationComponent (anim_raileron, 0, 1, &RAileron);

	// ***** 6. Rudder animation *****

	static UINT RudderGrp[2] = {GRP_rudderR,GRP_rudderL};
	static MGROUP_ROTATE Rudder (midx, RudderGrp, 2,
		_V(0,5.77,-12.17), _V(-0.037,0.833,-0.552), (float)(-54.2*RAD));
	anim_rudder = CreateAnimation (0.5);
	AddAnimationComponent (anim_rudder, 0, 1, &Rudder);

	// ***** 7. Speedbrake animation *****

	static UINT SB1Grp[1] = {GRP_rudderR};
	static MGROUP_ROTATE SB1 (midx, SB1Grp, 1,
		_V(0.32,5.77,-12.17), _V(-0.037,0.833,-0.552), (float)(-49.3*RAD));
	static UINT SB2Grp[1] = {GRP_rudderL};
	static MGROUP_ROTATE SB2 (midx, SB2Grp, 1,
		_V(-0.32,5.77,-12.17), _V(0.037,0.833,-0.552), (float)(49.3*RAD));

	anim_spdb = CreateAnimation (0);
	AddAnimationComponent (anim_spdb, 0, 1, &SB1);
	AddAnimationComponent (anim_spdb, 0, 1, &SB2);

	// ***** 8. RMS arm animation *****
	// Note that the animation components can't be declared static here, since
	// their rotation parameters are modified by the respective parent transforms

	ANIMATIONCOMPONENT_HANDLE parent;

	static UINT RMSShoulderYawGrp[1] = {GRP_Shoulder};
	rms_anim[0] = new MGROUP_ROTATE (midx, RMSShoulderYawGrp, 1,
		_V(-2.26, 1.70, 9.65), _V(0, 1, 0), (float)(-360*RAD)); // -180 .. +180
	anim_arm_sy = CreateAnimation (0.5);
	parent = AddAnimationComponent (anim_arm_sy, 0, 1, rms_anim[0]);

	static UINT RMSShoulderPitchGrp[1] = {GRP_Humerus};
	rms_anim[1] = new MGROUP_ROTATE (midx, RMSShoulderPitchGrp, 1,
		_V(-2.26, 1.70, 9.65), _V(1, 0, 0), (float)(147*RAD)); // -2 .. +145
	anim_arm_sp = CreateAnimation (0.0136);
	parent = AddAnimationComponent (anim_arm_sp, 0, 1, rms_anim[1], parent);

	static UINT RMSElbowPitchGrp[3] = {GRP_radii,GRP_RMScamera,GRP_RMScamera_pivot};
	rms_anim[2] = new MGROUP_ROTATE (midx, RMSElbowPitchGrp, 3,
		_V(-2.26,1.55,3.10), _V(1,0,0), (float)(-162*RAD)); // -160 .. +2
	anim_arm_ep = CreateAnimation (0.0123);
	parent = AddAnimationComponent (anim_arm_ep, 0, 1, rms_anim[2], parent);

	static UINT RMSWristPitchGrp[1] = {GRP_wrist};
	rms_anim[3] = new MGROUP_ROTATE (midx, RMSWristPitchGrp, 1,
		_V(-2.26,1.7,-3.55), _V(1,0,0), (float)(240*RAD)); // -120 .. +120
	anim_arm_wp = CreateAnimation (0.5);
	parent = AddAnimationComponent (anim_arm_wp, 0, 1, rms_anim[3], parent);

	static UINT RMSWristYawGrp[1] = {GRP_endeffecter};
	rms_anim[4] = new MGROUP_ROTATE (midx, RMSWristYawGrp, 1,
		_V(-2.26,1.7,-4.9), _V(0,1,0), (float)(-240*RAD)); // -120 .. +120
	anim_arm_wy = CreateAnimation (0.5);
	parent = AddAnimationComponent (anim_arm_wy, 0, 1, rms_anim[4], parent);

	rms_anim[5] = new MGROUP_ROTATE (LOCALVERTEXLIST, MAKEGROUPARRAY(arm_tip), 3,
		_V(-2.26,1.7,-6.5), _V(0,0,1), (float)(894*RAD)); // -447 .. +447
	anim_arm_wr = CreateAnimation (0.5);
	hAC_arm = AddAnimationComponent (anim_arm_wr, 0, 1, rms_anim[5], parent);

	// ***** 9. SSME pitch gimbal animations
	double init_gimbal = -10*RAD;
	float max_gimbal = (float)(-0.2*PI);
	anim_ssme = CreateAnimation (init_gimbal/max_gimbal);

	static UINT SSMEL_Grp = GRP_SSMEL;
	ssme_anim[0] = new MGROUP_ROTATE (midx, &SSMEL_Grp, 1, _V(-1.55,-0.37,-12.5), _V(-1,0,0), max_gimbal);
	AddAnimationComponent (anim_ssme, 0, 1, ssme_anim[0]);

	static UINT SSMER_Grp = GRP_SSMER;
	ssme_anim[1] = new MGROUP_ROTATE (midx, &SSMER_Grp, 1, _V( 1.55,-0.37,-12.5), _V(-1,0,0), max_gimbal);
	AddAnimationComponent (anim_ssme, 0, 1, ssme_anim[1]);

	static UINT SSMET_Grp = GRP_SSMET;
	ssme_anim[2] = new MGROUP_ROTATE (midx, &SSMET_Grp, 1, _V(0.0,  2.7, -12.5), _V(-1,0,0), max_gimbal);
	AddAnimationComponent (anim_ssme, 0, 1, ssme_anim[2]);

	// ======================================================
	// VC animation definitions
	// ======================================================
	plop->DefineAnimations (vidx);
}

// --------------------------------------------------------------
// Register the MFD interface for the ascent autopilot
// --------------------------------------------------------------
int Atlantis::RegisterAscentApMfd ()
{
	static const char *name = "AscentAP";
	MFDMODESPECEX spec;
	spec.name = (char*)name;
	spec.key = OAPI_KEY_B;
	spec.context = NULL;
	spec.msgproc = AscentApMfd::MsgProc;
	return RegisterMFDMode (spec);
}

// --------------------------------------------------------------
// Open the dialog interface for the ascent autopilot
// --------------------------------------------------------------
void Atlantis::CreateAscentAPDlg ()
{
	if (!ascentApDlg) {
		ascentApDlg = new AscentAPDlg(ascap);
		ascentApDlg->Open (g_Param.hDLL, true);
	}
}

// --------------------------------------------------------------
// Close the dialog interface for the ascent autopilot
// --------------------------------------------------------------
void Atlantis::DestroyAscentAPDlg ()
{
	if (ascentApDlg) {
		delete ascentApDlg;
		ascentApDlg = 0;
	}
}

// --------------------------------------------------------------
// Jettison both SRBs from ET
// --------------------------------------------------------------
void Atlantis::SeparateBoosters (double met)
{
	OBJHANDLE hET = GetDockStatus (GetDockHandle (1));
	if (hET) {
		VESSEL *pET = oapiGetVesselInterface (hET);
		((Atlantis_Tank*)pET)->SeparateSRBs();
	}

	// reconfigure
	status = 2;
	RecordEvent ("JET", "SRB");
}

// --------------------------------------------------------------
// Jettison ET from orbiter
// --------------------------------------------------------------
void Atlantis::SeparateTank ()
{
	if (hDockET) {
		Undock (1);
		DelDock (hDockET);
		hDockET = NULL;
	}
	pET = NULL;
	EnableSSME (false);
	EnableRCS (RCS_ROT);
	EnableOMS (true);

	// reconfigure
	status = 3;
	RecordEvent ("JET", "ET");
}

void Atlantis::AttachChildWithMass(OBJHANDLE hChild, ATTACHMENTHANDLE attachment, ATTACHMENTHANDLE child_attachment)
{
	AttachChild(hChild, attachment, child_attachment);
	SetEmptyMass(GetEmptyMass() + oapiGetEmptyMass(hChild));
}


void Atlantis::DetachChildWithMass(ATTACHMENTHANDLE attachment, double vel)
{
	OBJHANDLE hChild = GetAttachmentStatus(attachment);
	if (hChild) {
		DetachChild(attachment, vel);
		SetEmptyMass(GetEmptyMass() - oapiGetEmptyMass(hChild));
	}
}

void Atlantis::ToggleGrapple (void)
{
	HWND hDlg;
	OBJHANDLE hV = GetAttachmentStatus (rms_attach);

	if (hV) {  // release satellite

		ATTACHMENTHANDLE hAtt = CanArrest();
		DetachChildWithMass(rms_attach);
		if (hDlg = oapiFindDialog (g_Param.hDLL, IDD_RMS)) {
			SetWindowText (GetDlgItem (hDlg, IDC_GRAPPLE), "Grapple");
			EnableWindow (GetDlgItem (hDlg, IDC_STOW), TRUE);
		}
		// check whether the object being ungrappled is ready to be clamped into the payload bay
		if (hAtt) {
			AttachChildWithMass(hV, sat_attach, hAtt);
			if (hDlg) {
				SetWindowText (GetDlgItem (hDlg, IDC_PAYLOAD), "Purge");
				EnableWindow (GetDlgItem (hDlg, IDC_PAYLOAD), TRUE);
			}
		}

#ifdef UNDEF
		VECTOR3 pos, dir, rot, gbay, gpos;
		GetAttachmentParams (sat_attach, pos, dir, rot);
		Local2Global (pos, gbay);
		VESSEL *v = oapiGetVesselInterface (hV);
		DWORD nAttach = v->AttachmentCount (true);
		for (DWORD j = 0; j < nAttach; j++) { // now scan all attachment points
			ATTACHMENTHANDLE hAtt = v->GetAttachmentHandle (true, j);
			v->GetAttachmentParams (hAtt, pos, dir, rot);
			v->Local2Global (pos, gpos);
			if (dist (gpos, gbay) < MAX_GRAPPLING_DIST) {
				AttachChild (hV, sat_attach, hAtt);
				return;
			}
		}
#endif

	} else {             // grapple satellite

		VECTOR3 gpos, grms, pos, dir, rot;
		Local2Global (arm_tip[0], grms);  // global position of RMS tip
		
		// Search the complete vessel list for a grappling candidate.
		// Not very scalable ...
		for (DWORD i = 0; i < oapiGetVesselCount(); i++) {
			OBJHANDLE hV = oapiGetVesselByIndex (i);
			if (hV == GetHandle()) continue; // we don't want to grapple ourselves ...
			oapiGetGlobalPos (hV, &gpos);
			if (dist (gpos, grms) < oapiGetSize (hV)) { // in range
				VESSEL *v = oapiGetVesselInterface (hV);
				DWORD nAttach = v->AttachmentCount (true);
				for (DWORD j = 0; j < nAttach; j++) { // now scan all attachment points of the candidate
					ATTACHMENTHANDLE hAtt = v->GetAttachmentHandle (true, j);
					const char *id = v->GetAttachmentId (hAtt);
					if (strncmp (id, "GS", 2)) continue; // attachment point not compatible
					v->GetAttachmentParams (hAtt, pos, dir, rot);
					v->Local2Global (pos, gpos);
					if (dist (gpos, grms) < MAX_GRAPPLING_DIST) { // found one!
						// check whether satellite is currently clamped into payload bay
						if (hV == GetAttachmentStatus(sat_attach))
							DetachChildWithMass(sat_attach);
						AttachChildWithMass(hV, rms_attach, hAtt);
						if (hDlg = oapiFindDialog (g_Param.hDLL, IDD_RMS)) {
							SetWindowText (GetDlgItem (hDlg, IDC_GRAPPLE), "Release");
							EnableWindow (GetDlgItem (hDlg, IDC_STOW), FALSE);
						}
						return;
					}
				}
			}
		}

	}
}

void Atlantis::GetSSMEGimbalPos (int which, double &pitch, double &yaw)
{
	VECTOR3 dir;
	if (status < 3) {
		GetThrusterDir (th_main[which], dir);
	} else {
		dir.x = dir.y = 0.0; dir.z = 1.0;
	}
	pitch = asin(dir.y)+10.5*RAD;
	yaw = atan(dir.x/dir.z);
}

void Atlantis::GetSRBGimbalPos (int which, double &pitch, double &yaw)
{
	VECTOR3 dir;
	if (status < 2 && pET) {
		dir = pET->GetSRBThrustDir(which);
	} else {
		dir.x = dir.y = 0.0; dir.z = 1.0;
	}
	pitch = asin(dir.y);
	if (which) pitch = -pitch, yaw = -yaw;
	yaw = atan(dir.x/dir.z);
}

void Atlantis::ToggleArrest (void)
{
	HWND hDlg;
	if (SatStowed()) { // purge satellite
		DetachChildWithMass(sat_attach, 0.1);
		if (hDlg = oapiFindDialog (g_Param.hDLL, IDD_RMS)) {
			SetWindowText (GetDlgItem (hDlg, IDC_PAYLOAD), "Arrest");
			EnableWindow (GetDlgItem (hDlg, IDC_PAYLOAD), CanArrest() ? TRUE:FALSE);
		}
	} else if (CanArrest()) {           // try to arrest satellite
		ToggleGrapple();
	}
}

// check whether the currently grappled object can be stowed in the cargo bay
ATTACHMENTHANDLE Atlantis::CanArrest (void) const
{
	OBJHANDLE hV = GetAttachmentStatus (rms_attach);
	if (!hV) return 0;
	VESSEL *v = oapiGetVesselInterface (hV);
	DWORD nAttach = v->AttachmentCount (true);
	VECTOR3 pos, dir, rot, gpos, gbay;
	GetAttachmentParams (sat_attach, pos, dir, rot);
	Local2Global (pos, gbay);
	for (DWORD j = 0; j < nAttach; j++) {
		ATTACHMENTHANDLE hAtt = v->GetAttachmentHandle (true, j);
		if (strncmp (v->GetAttachmentId (hAtt), "XS", 2)) continue; // attachment point not compatible
		v->GetAttachmentParams (hAtt, pos, dir, rot);
		v->Local2Global (pos, gpos);
		if (dist (gpos, gbay) < MAX_GRAPPLING_DIST) {
			return hAtt;
		}
	}
	return 0;
}

void Atlantis::SeparateMMU (void)
{
	// Create MMU at docking port
	DOCKHANDLE hDock = GetDockHandle (0);
	if (GetDockStatus(hDock)) return; // something is already attached to this docking port

	int i;
	char name[256];
	OBJHANDLE hVessel;
	for (i = 0; ; i++) {
		sprintf (name, "%s-MMU-%d", GetName(), i+1);
		hVessel = oapiGetVesselByName(name);
		if (!hVessel) break;
	}

	VESSELSTATUS vs;
	GetStatus (vs);
	hMMU = oapiCreateVessel (name, "Nasa_MMU", vs);
	Dock (hMMU, 0, 0, 1);
	oapiSetFocusObject (hMMU);
}

double Atlantis::GetSRBThrustLevel (int which)
{
	return (pET ? pET->GetSRBThrustLevel (which) : 0);
}

void Atlantis::SetSSMEGimbal (const VECTOR3 &angle)
{
	const double pitch_gimbal_max = -0.2*PI;
	VECTOR3 dir;

	dir.x = -sin(angle.y);        // yaw gimbal
	dir.y = sin(angle.x-angle.z); // pitch+roll gimbal
	dir.z = sqrt(1.0-dir.x*dir.x-dir.y*dir.y);
	SetThrusterDir (th_main[0], dir); // left SSME

	dir.y = sin(angle.x+angle.z); // pitch+roll gimbal
	dir.z = sqrt(1.0-dir.x*dir.x-dir.y*dir.y);
	SetThrusterDir (th_main[1], dir); // right SSME

	dir.y = sin(angle.x); // pitch gimbal
	dir.z = sqrt(1.0-dir.x*dir.x-dir.y*dir.y);
	SetThrusterDir (th_main[2], dir); // top SSME

	SetSSMEPosition (gimbal_pos.x/pitch_gimbal_max);
}

// --------------------------------------------------------------
// Autopilot function: set target pitch angle [rad]
// during launch phase (until ET separation)
// --------------------------------------------------------------
double Atlantis::GetAscentPitchRate (double tgt_pitch)
{
	const double a = 0.07;
	const double b = 0.035;

	VECTOR3 avel;
	GetAngularVel(avel);
	double dpitch = avel.x;    // pitch rate
	double pitch = GetPitch(); // current pitch value

	return a*(pitch-tgt_pitch) + b*dpitch;
}

// --------------------------------------------------------------
// Automatic gimbal adjustment for SSME and SRB engines to correct
// for CG shift, SRB thrust variations, atmospheric effects, etc.
//
// NOTE: We use SSME gimbal for adjusting pitch rate, SRB gimbal for
// adjusting yaw and roll rate
//
// The gimbal changes are implemented individually for each axis as
// damped harmonic oscillators around target rates
// --------------------------------------------------------------
void Atlantis::AutoGimbal (const VECTOR3 &tgt_rate)
{
	// Harmonic oscillator design parameters
	static const double a_pitch = 2e0;
	static const double b_pitch = 1e0;
	static const double a_yaw = 1e-1;
	static const double b_yaw = 3e-2;
	static const double a_roll_srb = 1e-1;
	static const double b_roll_srb = 3e-2;
	static const double a_roll_ssme = 8e-2;
	static const double b_roll_ssme = 5e-2;

	VECTOR3 avel, aacc;
	GetAngularVel(avel);
	GetAngularAcc(aacc);
	double dt = oapiGetSimStep();
	double dgimbal, maxdg;
	const double pitch_gimbal_max = -21.0*RAD;
	const double yaw_gimbal_max = 4*RAD;
	const double roll_gimbal_max_srb = 8.0*RAD;
	const double roll_gimbal_max_ssme = 6*RAD;

	bool srb_gimbal = status < 2 && pET;
	double roll_gimbal_max = (srb_gimbal ? roll_gimbal_max_srb : roll_gimbal_max_ssme);
	double a_roll = (srb_gimbal ? a_roll_srb : a_roll_ssme);
	double b_roll = (srb_gimbal ? b_roll_srb : b_roll_ssme);

	// Pitch gimbal settings
	maxdg = dt*0.3; // max gimbal speed [rad/s]
	dgimbal = a_pitch*(avel.x-tgt_rate.x) + b_pitch*aacc.x;
	dgimbal = max(-maxdg, min(maxdg, dgimbal));
	gimbal_pos.x = min (0.0, max (pitch_gimbal_max, gimbal_pos.x+dgimbal));

	// Yaw gimbal settings
	dgimbal = a_yaw*(avel.y-tgt_rate.y) + b_yaw*aacc.y;
	gimbal_pos.y = min (yaw_gimbal_max, max(-yaw_gimbal_max, gimbal_pos.y+dgimbal));

	// Roll gimbal settings
	dgimbal = a_roll*(avel.z-tgt_rate.z) + b_roll*aacc.z;
	gimbal_pos.z = min (roll_gimbal_max, max(-roll_gimbal_max, gimbal_pos.z+dgimbal));

	// Set SRB gimbals
	if (status < 2 && pET) {
		pET->SetSRBGimbal (gimbal_pos);
		SetSSMEGimbal (_V(gimbal_pos.x,0,0)); // If SRBs are available, we gimbal the SSMEs only in pitch
	} else {
		SetSSMEGimbal (gimbal_pos);
	}
}

// --------------------------------------------------------------
// RCS automatic control for commanding a target attitude rate
// Used by the ascent autopilot when gimbal control is no longer available
// (SSME cut off)
// --------------------------------------------------------------
void Atlantis::AutoRCS (const VECTOR3 &tgt_rate)
{
	// Harmonic oscillator design parameters
	const double a_pitch = 4;
	const double b_pitch = 2;
	const double a_yaw = 2e-1;
	const double b_yaw = 6e-2;
	const double a_roll = 2e-1;
	const double b_roll = 6e-2;

	VECTOR3 avel, aacc;
	GetAngularVel(avel);
	GetAngularAcc(aacc);
	double dt = oapiGetSimStep();
	double drcs;

	// Pitch RCS settings
	drcs = a_pitch*(tgt_rate.x-avel.x) - b_pitch*aacc.x;
	if (drcs > 0.0) {
		SetThrusterGroupLevel(THGROUP_ATT_PITCHUP, min(drcs, 1.0));
		SetThrusterGroupLevel(THGROUP_ATT_PITCHDOWN, 0);
	} else {
		SetThrusterGroupLevel(THGROUP_ATT_PITCHUP, 0);
		SetThrusterGroupLevel(THGROUP_ATT_PITCHDOWN, min(-drcs, 1.0));
	}

	// Yaw RCS settings
	drcs = a_yaw*(tgt_rate.y-avel.y) - b_yaw*aacc.y;
	if (drcs > 0.0) {
		SetThrusterGroupLevel(THGROUP_ATT_YAWLEFT, min(drcs, 1.0));
		SetThrusterGroupLevel(THGROUP_ATT_YAWRIGHT, 0);
	} else {
		SetThrusterGroupLevel(THGROUP_ATT_YAWLEFT, 0);
		SetThrusterGroupLevel(THGROUP_ATT_YAWRIGHT, min(-drcs, 1.0));
	}

	// Roll RCS settings
	drcs = a_roll*(tgt_rate.z-avel.z) - b_roll*aacc.z;
	if (drcs > 0.0) {
		SetThrusterGroupLevel(THGROUP_ATT_BANKRIGHT, min(drcs, 1.0));
		SetThrusterGroupLevel(THGROUP_ATT_BANKLEFT, 0);
	} else {
		SetThrusterGroupLevel(THGROUP_ATT_BANKRIGHT, 0);
		SetThrusterGroupLevel(THGROUP_ATT_BANKLEFT, min(-drcs, 1.0));
	}
}


void Atlantis::LaunchClamps ()
{
	// TODO
#ifdef UNDEF
	VECTOR3 F, T, r = _V(0,0,0), Fc = _V(0,0,0), Tc = _V(0,0,0);
	GetThrusterMoment (th_srb[0], F, T);
	Fc.z = -2*F.z;
	Tc.x =  2*T.x;
	GetThrusterMoment (th_main[0], F, T);
	Fc.z -= 2*F.z;
	Fc.y -= 2*F.y;
	Tc.x += 2*T.x;
	GetThrusterMoment (th_main[2], F, T);
	Fc.z -= F.z;
	Fc.y -= F.y;
	Tc.x += T.x;
	r.z = (Fc.y ? Tc.x/Fc.y : 0);
	AddForce (Fc, r);

	Tc = _V(0,0,0);
	GetThrusterMoment(th_srb[0], F, T);
	Tc += T;
	GetThrusterMoment(th_srb[1], F, T);
	Tc += T;
	for (int i = 0; i < 3; i++) {
		GetThrusterMoment(th_main[i], F, T);
		Tc += T;
	}
#endif
}

void Atlantis::SetGearParameters (double state)
{
	static TOUCHDOWNVTX tdvtx[14] = {
		{_V( 0,    -3.3,18.75), 1e8, 1e6, 1.6, 0.1},
		{_V(-3.96, -5.5, -3.2), 1e8, 1e6, 3, 0.2},
		{_V( 3.96, -5.5, -3.2), 1e8, 1e6, 3, 0.2},
		{_V(-11.9, -2.1, -10),  1e8, 1e6, 3},
		{_V( 11.9, -2.1, -10),  1e8, 1e6, 3},
		{_V(-11.3, -2.1, -6),   1e8, 1e6, 3},
		{_V( 11.3, -2.1, -6),   1e8, 1e6, 3},
		{_V(-2.95, -2.0,-14.35),1e8, 1e6, 3},
		{_V( 2.95, -2.0,-14.35),1e8, 1e6, 3},
		{_V(-1.9,  -1.0,-14.8), 1e8, 1e6, 3},
		{_V( 1.9,  -1.0,-14.8), 1e8, 1e6, 3},
		{_V( 0,    11.2,-16.4), 1e8, 1e6, 3},
		{_V( 0,    11.3,-14.0), 1e8, 1e6, 3},
		{_V( 0,    -0.9, 20.6), 1e8, 1e6, 3}
	};
	if (state == 1.0) { // gear fully deployed
		static TOUCHDOWNVTX geardn_vtx[3] = {
			{_V( 0,    -3.95,17.5), 1e8, 1e6, 1.6, 0.1},
			{_V(-3.96, -5.5, -3.2), 1e8, 1e6, 3, 0.2},
			{_V( 3.96, -5.5, -3.2), 1e8, 1e6, 3, 0.2},
		};
		memcpy (tdvtx, geardn_vtx, 3*sizeof(TOUCHDOWNVTX));
		SetTouchdownPoints (tdvtx, 14);
		SetSurfaceFrictionCoeff (0.05, 0.4);
	} else {
		static TOUCHDOWNVTX gearup_vtx[3] = {
			{_V( 0,    -2.2,16.75), 1e8, 1e6, 3},
			{_V(-3.96, -2.7, -3.2), 1e8, 1e6, 3},
			{_V( 3.96, -2.7, -3.2), 1e8, 1e6, 3},
		};
		memcpy (tdvtx, gearup_vtx, 3*sizeof(TOUCHDOWNVTX));
		SetTouchdownPoints (tdvtx, 14);
		SetSurfaceFrictionCoeff (0.4, 0.4);
	}
}

void Atlantis::Jettison ()
{
	switch (status) {
	case 0:
	case 3:               // nothing to do
		break;
	case 1:               // abandon boosters
		SeparateBoosters (oapiGetSimTime()-t0); 
		break;
	case 2:               // abandon tank
		SeparateTank();
		break;
	}
}

// Update moving parts of the orbiter's visual: payload bay doors and gear
// This should only be called when the visual exists, e.g. from within
// clbkVisualCreated or clbkAnimate

void Atlantis::UpdateMesh ()
{
	// update animation states
	SetAnimation (anim_gear, gear_proc);
	SetAnimation (anim_spdb, spdb_proc);
	SetAnimation (anim_door, plop->BayDoorStatus.pos);
	SetAnimation (anim_rad,  plop->RadiatorStatus.pos);
	SetAnimation (anim_kubd, plop->KuAntennaStatus.pos);

	SetAnimationArm (anim_arm_sy, arm_sy);
	SetAnimationArm (anim_arm_sp, arm_sp);
	SetAnimationArm (anim_arm_ep, arm_ep);
	SetAnimationArm (anim_arm_wp, arm_wp);
	SetAnimationArm (anim_arm_wy, arm_wy);
	SetAnimationArm (anim_arm_wr, arm_wr);

	// update MFD brightness
	if (vis) {
		oapi::FVECTOR4 mat;
		DEVMESHHANDLE hMesh = GetDevMesh (vis, mesh_vc);
		for (int i = 0; i < 10; i++) {
			mat.rgb = (float)mfdbright[i];
			mat.a = 1.0f;
			oapiSetMaterialEx (hMesh, 10+i, MatProp::Light, &mat);
		}
	}
}

void Atlantis::SetBayDoorPosition (double pos)
{
	SetAnimation (anim_door, pos);
	rdoor_drag = sqrt (min (1.0, pos*3.0));
	ldoor_drag = sqrt (min (1.0, max(0.0, pos-0.3656)*3.0));
}

void Atlantis::SetRadiatorPosition (double pos)
{
	SetAnimation (anim_rad, pos);
}

void Atlantis::SetKuAntennaPosition (double pos)
{
	SetAnimation (anim_kubd, pos);
}

void Atlantis::SetSSMEPosition (double pos)
{
	SetAnimation (anim_ssme, pos);
}

void Atlantis::OperateLandingGear (AnimState::Action action)
{
	if (status < 3) return;
	// operate landing gear only once the orbiter is free from the tank

	if (action == AnimState::OPENING && GroundContact()) {
		VECTOR3 nml = {0,1,0}, vnml;
		HorizonInvRot(nml, vnml);
		if (vnml.y > 0.0) return;
	}
	// don't extend landing gear if standing on the ground

	gear_status = action;
	RecordEvent ("GEAR", action == AnimState::CLOSING ? "UP" : "DOWN");
}

void Atlantis::RevertLandingGear ()
{
	if (status < 3) return;
	// operate landing gear only once the orbiter is free from the tank

	OperateLandingGear (gear_status == AnimState::CLOSED || gear_status == AnimState::CLOSING ?
		AnimState::OPENING : AnimState::CLOSING);
}

void Atlantis::OperateSpeedbrake (AnimState::Action action)
{
	spdb_status = action;
	RecordEvent ("SPEEDBRAKE", action == AnimState::CLOSING ? "CLOSE" : "OPEN");
}

void Atlantis::RevertSpeedbrake (void)
{
	OperateSpeedbrake (spdb_status == AnimState::CLOSED || spdb_status == AnimState::CLOSING ?
		AnimState::OPENING : AnimState::CLOSING);
}

void Atlantis::SetAnimationArm (UINT anim, double state)
{
	SetAnimation (anim, state);
	arm_scheduled = true;

	HWND hDlg;
	if (!SatStowed() && (hDlg = oapiFindDialog (g_Param.hDLL, IDD_RMS))) {
		SetWindowText (GetDlgItem (hDlg, IDC_PAYLOAD), "Arrest");
		EnableWindow (GetDlgItem (hDlg, IDC_PAYLOAD), CanArrest() ? TRUE : FALSE);
	}
}

void Atlantis::RedrawPanel_MFDButton (SURFHANDLE surf, int mfd)
{
	using namespace oapi;

	Sketchpad *pSkp = oapiGetSketchpad(surf);

	// D. Beachy: BUGFIX: if MFD powered off, cover separator lines and do not paint buttons
    if (oapiGetMFDMode(mfd) == MFD_NONE) {
        RECT r = { 0,0,255,13 };
		pSkp->SetPen(NULL);
		pSkp->SetBrush(g_Param.brush[0]);
        pSkp->Rectangle(r.left, r.top, r.right, r.bottom);
    } else {   // MFD powered on
		auto pOld = pSkp->SetFont(g_Param.font[0]);
		pSkp->SetTextColor (RGB(0,255,216));
		pSkp->SetTextAlign (Sketchpad::CENTER, Sketchpad::TOP);
		pSkp->SetBackgroundMode(Sketchpad::BK_TRANSPARENT);

		const char *label;
		int x = 24;

		for (int bt = 0; bt < 5; bt++) {
			if (label = oapiMFDButtonLabel (mfd, bt)) {
				pSkp->Text (x, 1, label, strlen(label));
				x += 42;
			} else break;
		}
		pSkp->Text (234, 1, "PG", 2);
		pSkp->SetFont(pOld);
	}
	oapiReleaseSketchpad(pSkp);
}

// ==============================================================
// Overloaded callback functions
// ==============================================================

// --------------------------------------------------------------
// Set vessel class capabilities from config file
// --------------------------------------------------------------
void Atlantis::clbkSetClassCaps (FILEHANDLE cfg)
{
	// *********************** physical parameters *********************************
	
	SetSize (19.6);
	SetEmptyMass (ORBITER_EMPTY_MASS);
	SetPMI (_V(78.2,82.1,10.7));
	SetGravityGradientDamping (20.0);
	SetCrossSections (ORBITER_CS);
	SetRotDrag (_V(0.43,0.43,0.29)); // angular drag
	SetTrimScale (0.05);
	launchelev = 0.0;

	if (!oapiReadItem_bool (cfg, (char*)"RenderCockpit", render_cockpit))
		render_cockpit = false;
}

// --------------------------------------------------------------
// Set status from a VESSELSTATUS2 structure
// --------------------------------------------------------------
void Atlantis::clbkSetStateEx (const void *status)
{
	// default parameter initialisation
	VESSEL4::clbkSetStateEx (status);
}

// --------------------------------------------------------------
// Read status from scenario file
// --------------------------------------------------------------
void Atlantis::clbkLoadStateEx (FILEHANDLE scn, void *vs)
{
	int action;
    char *line;
	double met = 0.0; // mission elapsed time
	double srbtime = 0.0;
	double sts_sat_x = 0.0;
	double sts_sat_y = 0.0;
	double sts_sat_z = 0.0;
	spdb_status = AnimState::CLOSED; spdb_proc = 0.0;

	while (oapiReadScenario_nextline (scn, line)) {
        if (!_strnicmp (line, "CONFIGURATION", 13)) {
            sscanf (line+13, "%d", &status);
		//} else if (!_strnicmp (line, "MET", 3)) {
		//	sscanf (line+3, "%lf", &met);
		} else if (!_strnicmp (line, "GEAR", 4)) {
			sscanf (line+4, "%d%lf", &action, &gear_proc);
			gear_status = (AnimState::Action)(action+1);
		} else if (!_strnicmp (line, "SPEEDBRAKE", 10)) {
			sscanf (line+10, "%d%lf", &action, &spdb_proc);
			spdb_status = (AnimState::Action)(action+1);
		} else if (!_strnicmp (line, "SRB_IGNITION_TIME", 17)) {
			sscanf (line+17, "%lf", &srbtime);
		} else if (!_strnicmp (line, "SAT_OFS_X", 9)) {
			sscanf (line+9, "%lf", &sts_sat_x);
		} else if (!_strnicmp (line, "SAT_OFS_Y", 9)) {
			sscanf (line+9, "%lf", &sts_sat_y);
		} else if (!_strnicmp (line, "SAT_OFS_Z", 9)) {
			sscanf (line+9, "%lf", &sts_sat_z);
		} else if (!_strnicmp (line, "CARGO_STATIC_MESH", 17)) {
			sscanf (line+17, "%s", cargo_static_mesh_name);
			do_cargostatic = true;
		} else if (!_strnicmp (line, "CARGO_STATIC_OFS", 16)) {
			sscanf (line+16, "%lf%lf%lf", &cargo_static_ofs.x, &cargo_static_ofs.y, &cargo_static_ofs.z);
		} else if (!_strnicmp (line, "ARM_STATUS", 10)) {
			sscanf (line+10, "%lf%lf%lf%lf%lf%lf", &arm_sy, &arm_sp, &arm_ep, &arm_wp, &arm_wy, &arm_wr);
        } else {
			if      (plop->ParseScenarioLine (line)) continue;  // offer the line to bay door operations
			else if (ascap->ParseScenarioLine (line)) continue; // offer to ascent autopilot
            else    ParseScenarioLineEx (line, vs);             // unrecognised option - pass to Orbiter's generic parser
        }
    }
	if (status == 0) {
		VESSELSTATUS2 *vs2 = (VESSELSTATUS2*)vs;
		if (vs2->status & 1) { // idle flag
			launchelev = max (0.0, vs2->vrot.x - 18.962);
			if (vs2->arot.x > 4.0) {   // rotation matrix not defined - need to construct manually
				double slng = sin (vs2->surf_lng), clng = cos (vs2->surf_lng);
				double slat = sin (vs2->surf_lat), clat = cos (vs2->surf_lat);
				double sdir = sin (vs2->surf_hdg), cdir = cos (vs2->surf_hdg);
				vs2->arot.x =  atan2(slat, clat*slng);
				vs2->arot.y = -asin(clng*clat);
				vs2->arot.z =  atan2(clng*slat*cdir+slng*sdir, clng*slat*sdir-slng*cdir);
			}
		} else {
			double rad = length(vs2->rpos);
			double alt = rad - oapiGetSize(vs2->rbody);
			launchelev = max (0.0, alt - 18.962);
		}
	}

	if (sts_sat_x || sts_sat_y || sts_sat_z) {
		ofs_sts_sat.x=sts_sat_x;
		ofs_sts_sat.y=sts_sat_y;
		ofs_sts_sat.z=sts_sat_z;
		SetAttachmentParams (sat_attach, ofs_sts_sat, _V(0,1,0), _V(0,0,1));
	}

	// optional meshes
	if (do_cargostatic && !mesh_cargo) {
		mesh_cargo = AddMesh (cargo_static_mesh_name, &cargo_static_ofs);
	}
	if (do_plat && !mesh_platform) {
		VECTOR3 plat_ofs = _V(-2.59805, 1.69209, -5.15524);
		mesh_platform = AddMesh("shuttle_eva_plat", &plat_ofs);
	}
	t0 = ascap->GetMT0();

	SetGearParameters (gear_proc);
}

// --------------------------------------------------------------
// Write status to scenario file
// --------------------------------------------------------------
void Atlantis::clbkSaveState (FILEHANDLE scn)
{
	char cbuf[256];

	// default vessel parameters
	VESSEL4::clbkSaveState (scn);

	// custom parameters
	oapiWriteScenario_int (scn, (char*)"CONFIGURATION", status);

	//if (status == 1)
	//	oapiWriteScenario_float (scn, "MET", oapiGetSimTime()-t0);

	sprintf (cbuf, "%d %0.4f", gear_status-1, gear_proc);
	oapiWriteScenario_string (scn, (char*)"GEAR", cbuf);

	if (spdb_status != AnimState::CLOSED) {
		sprintf (cbuf, "%d %0.4f", spdb_status-1, spdb_proc);
		oapiWriteScenario_string (scn, (char*)"SPEEDBRAKE", cbuf);
	}

	//if (status == 0 && launchelev)
	//	oapiWriteScenario_float (scn, "LAUNCHELEVATION", launchelev);

	sprintf (cbuf, "%0.4f %0.4f %0.4f %0.4f %0.4f %0.4f", arm_sy, arm_sp, arm_ep, arm_wp, arm_wy, arm_wr);
	oapiWriteScenario_string (scn, (char*)"ARM_STATUS", cbuf);

	oapiWriteScenario_float (scn, (char*)"SAT_OFS_X", ofs_sts_sat.x);
	oapiWriteScenario_float (scn, (char*)"SAT_OFS_Y", ofs_sts_sat.y);
	oapiWriteScenario_float (scn, (char*)"SAT_OFS_Z", ofs_sts_sat.z);

	if (do_cargostatic) {
		oapiWriteScenario_string (scn, (char*)"CARGO_STATIC_MESH", cargo_static_mesh_name);
		if (cargo_static_ofs.x || cargo_static_ofs.y || cargo_static_ofs.z)
			oapiWriteScenario_vec (scn, (char*)"CARGO_STATIC_OFS", cargo_static_ofs);
	}

	// save bay door operations status
	plop->SaveState (scn);
	ascap->SaveState (scn);
}

// --------------------------------------------------------------

void Atlantis::clbkPostCreation ()
{
	char name[256];
	VESSELSTATUS vs;
	VESSEL *pV;

	VESSEL4::clbkPostCreation();

	OBJHANDLE payload;
	double drymass = ORBITER_EMPTY_MASS;
	payload = GetAttachmentStatus(sat_attach);
	if (payload)
		drymass += oapiGetEmptyMass(payload);
	payload = GetAttachmentStatus(rms_attach);
	if (payload)
		drymass += oapiGetEmptyMass(payload);
	if (drymass != ORBITER_EMPTY_MASS)
		SetEmptyMass(drymass);

	if (status < 3) {
		OBJHANDLE hET = GetDockStatus (GetDockHandle (1));
		if (!hET) {
			strcpy (name, GetName());
			strcat (name, "_ET");
			hET = oapiGetVesselByName(name);
			if (!hET || strcmp (oapiGetVesselInterface(hET)->GetClassName(), "Atlantis_Tank")) {
				GetStatus (vs);
				hET = oapiCreateVessel (name, "Atlantis_Tank", vs);
			}
			Dock (hET, 1, 0, 1);
		}
		pET = (Atlantis_Tank*)oapiGetVesselInterface(hET);
		if (status < 2) {
			pV = oapiGetVesselInterface (hET);
			for (UINT i = 0; i < 2; i++) {
				OBJHANDLE hSRB = pV->GetDockStatus (pV->GetDockHandle (i+1));
				if (!hSRB) {
					sprintf (name, "%s-SRB%d", GetName(), i+1);
					hSRB = oapiGetVesselByName(name);
					if (!hSRB || strcmp (oapiGetVesselInterface(hSRB)->GetClassName(), "Atlantis_SRB")) {
						GetStatus (vs);
						hSRB = oapiCreateVessel (name, "Atlantis_SRB", vs);
					}
					pV->Dock (hSRB, i+1, 0, 1);
				}
			}
			if (status < 1) {
				if (launchelev) {
					pET->SetSRBLaunchElevation (launchelev);
				}
			}
		}
	} else {
		if (hDockET) {
			DelDock (hDockET); // remove the ET docking port
			hDockET = NULL;
		}
	}
	EnableSSME (status < 3);
	EnableRCS (status == 3 ? RCS_ROT : RCS_NONE);
	EnableOMS (status == 3);
	SetADCtrlMode (status < 4 ? 0 : 7);

	UpdateMesh ();
}

// --------------------------------------------------------------
// Vessel gains or loses input focus
// --------------------------------------------------------------
void Atlantis::clbkFocusChanged (bool getfocus, OBJHANDLE newv, OBJHANDLE oldv)
{
	if (getfocus) {
		oapiDisableMFDMode (MFD_LANDING);
		// no VTOL MFD mode for Atlantis
	}
}

// --------------------------------------------------------------
// Simulation time step
// --------------------------------------------------------------
void Atlantis::clbkPreStep (double simt, double simdt, double mjd)
{
	ascap->Update (simt);
	if (ascentApDlg) ascentApDlg->Update (simt);

	//double met = (status == 0 ? 0.0 : simt-t0);
	double met = ascap->GetMET (simt);

	engine_light_level = GetThrusterGroupLevel (THGROUP_MAIN);

	VECTOR3 tgt_rate = _V(0,0,0); // target rotation rates - used for setting engine gimbals

	if (status >= 1 && status <= 3) {
		// ascent autopilot
		ascap->GetTargetRate (met, tgt_rate);

		// manual override
		double man_pitch = GetManualControlLevel (THGROUP_ATT_PITCHUP, MANCTRL_ROTMODE, MANCTRL_ANYDEVICE);
		if (!man_pitch) man_pitch = -GetManualControlLevel (THGROUP_ATT_PITCHDOWN, MANCTRL_ROTMODE, MANCTRL_ANYDEVICE);
		if (man_pitch) tgt_rate.x = man_pitch*0.07;

		double man_yaw   = GetManualControlLevel (THGROUP_ATT_YAWLEFT, MANCTRL_ROTMODE, MANCTRL_ANYDEVICE);
		if (!man_yaw) man_yaw = -GetManualControlLevel (THGROUP_ATT_YAWRIGHT, MANCTRL_ROTMODE, MANCTRL_ANYDEVICE);
		if (man_yaw)   tgt_rate.y = man_yaw*0.07;

		double man_roll  =-GetManualControlLevel (THGROUP_ATT_BANKLEFT, MANCTRL_ROTMODE, MANCTRL_ANYDEVICE);
		if (!man_roll) man_roll = GetManualControlLevel (THGROUP_ATT_BANKRIGHT, MANCTRL_ROTMODE, MANCTRL_ANYDEVICE);
		if (man_roll)  tgt_rate.z = man_roll*0.07;
	}

	switch (status) {
	case 0: // launch configuration
		if (!ascap->Active() && pET && GetEngineLevel (ENGINE_MAIN) > 0.95) {
			pET->IgniteSRBs ();
			t0 = ascap->StartMissionTime (simt);
			//t0 = simt /*+ SRB_STABILISATION_TIME*/;   // store designated liftoff time
			status = 1;
		} else if (GetEngineLevel (ENGINE_MAIN) > 0) {
			//AutoGimbal (tgt_rate);
		}
		break;
	case 1: // SRBs ignited
		if (met > SRB_SEPARATION_TIME && !Playback() || bManualSeparate) {
			SeparateBoosters (met);
			bManualSeparate = false;
		} else {
			AutoGimbal (tgt_rate);
		}
		break;
	case 2: // Orbiter+ET configuration
		AutoGimbal (tgt_rate);
		if (pET && (/*pET->GetMainPropellantMass () < 10.0 ||*/ bManualSeparate)) {
			SeparateTank ();
			bManualSeparate = false;
		}
		break;
	case 3: // Orbiter
		if (ascap->Active())
			AutoRCS (tgt_rate);

		if (bManualSeparate && GetAttachmentStatus (sat_attach)) {
			DetachChildWithMass(sat_attach, 0.1);
			bManualSeparate = false;
		}

		if (do_eva) {
			char name[256];
			strcpy (name, GetName()); strcat (name, "-MMU");
			OBJHANDLE hvessel = oapiGetVesselByName (name);
			if (!hvessel) {
				SeparateMMU ();
			}
			do_eva = false;
		};

		if (GetDynPressure() > 1000.0) {
			// 1000Pa ~ 20psf, see Mission Profile, https://science.ksc.nasa.gov/shuttle/technology/sts-newsref/mission_profile.html
			EnableRCS(RCS_NONE);
			SetADCtrlMode(7);
			// note: in reality, control doesn't switch from RCS to control surfaces completely in one go,
			// but at different stages for different components
			status = 4;
		}
		break;
	case 4: // reentry
		break;
	}

	// Execute payload bay operations
	plop->Step (simt, simdt);

	// ***** Animate landing gear *****

	if (gear_status >= AnimState::CLOSING) {
		double da = simdt * GEAR_OPERATING_SPEED;
		if (gear_status == AnimState::CLOSING) { // retract gear
			if (gear_proc > 0.0) gear_proc = max (0.0, gear_proc-da);
			else                 gear_status = AnimState::CLOSED;
		} else {                           // deploy gear
			if (gear_proc < 1.0) gear_proc = min (1.0, gear_proc+da);
			else                 gear_status = AnimState::OPEN;
		}
		SetAnimation (anim_gear, gear_proc);
		SetGearParameters (gear_proc);
	}

	// ***** Animate speedbrake *****

	if (spdb_status >= AnimState::CLOSING) {
		double da = simdt * SPEEDBRAKE_OPERATING_SPEED;
		if (spdb_status == AnimState::CLOSING) { // retract brake
			if (spdb_proc > 0.0) spdb_proc = max (0.0, spdb_proc-da);
			else                 spdb_status = AnimState::CLOSED;
		} else {                           // deploy antenna
			if (spdb_proc < 1.0) spdb_proc = min (1.0, spdb_proc+da);
			else                 spdb_status = AnimState::OPEN;
		}
		SetAnimation (anim_spdb, spdb_proc);
	}

	// ***** Stow RMS arm *****

	if (center_arm) {
		double t0 = oapiGetSimTime();
		double dt = t0 - center_arm_t;       // time step
		double da = ARM_OPERATING_SPEED*dt;  // total rotation angle

		// work from the wrist down to the shoulder
		if (da && (arm_wr != 0.5)) {    // zero wrist roll
			if (da >= fabs(arm_wr-0.5)) // finished
				arm_wr = 0.5, da -= fabs(arm_wr-0.5);
			else
				arm_wr -= (arm_wr > 0.5 ? da:-da), da = 0;
			SetAnimationArm (anim_arm_wr, arm_wr);
		}
		if (da && (arm_wy != 0.5)) {    // zero wrist yaw
			if (da >= fabs(arm_wy-0.5)) // finished
				arm_wy = 0.5, da -= fabs(arm_wy-0.5);
			else
				arm_wy -= (arm_wy > 0.5 ? da:-da), da = 0;
			SetAnimationArm (anim_arm_wy, arm_wy);
		}
		if (da && (arm_wp != 0.5)) {    // zero wrist pitch
			if (da >= fabs(arm_wp-0.5)) // finished
				arm_wp = 0.5, da -= fabs(arm_wp-0.5);
			else
				arm_wp -= (arm_wp > 0.5 ? da:-da), da = 0;
			SetAnimationArm (anim_arm_wp, arm_wp);
		}
		if (da && arm_ep) {             // zero elbow pitch
			if (da >= arm_ep)           // finished
				arm_ep = 0.0, da -= arm_ep;
			else
				arm_ep -= da, da = 0;
			SetAnimationArm (anim_arm_ep, arm_ep);
		}
		if (da && (arm_sy != 0.5)) {    // zero shoulder yaw
			if (da >= fabs(arm_sy-0.5)) // finished
				arm_sy = 0.5, da -= fabs(arm_sy-0.5);
			else
				arm_sy -= (arm_sy > 0.5 ? da:-da), da = 0;
			SetAnimationArm (anim_arm_sy, arm_sy);
		}
		if (da && arm_sp) {             // zero shoulder pitch
			if (da >= arm_sp)           // finished
				arm_sp = 0.0, da -= arm_sp;
			else
				arm_sp -= da, da = 0;
			SetAnimationArm (anim_arm_sp, arm_sp);
		}
		center_arm_t = t0;
		if (da) {
			center_arm = false; // finished stowing
			HWND hDlg = oapiFindDialog (g_Param.hDLL, IDD_RMS);
			if (hDlg) EnableWindow (GetDlgItem (hDlg, IDC_GRAPPLE), TRUE);
		}
	}

	if (arm_moved) {
		SetAttachmentParams (rms_attach, arm_tip[0], arm_tip[1]-arm_tip[0], arm_tip[2]-arm_tip[0]);
		arm_moved = false;
	}
	if (arm_scheduled) {
		arm_scheduled = false;
		arm_moved = true;
	}
}

// --------------------------------------------------------------
// Respond to playback event
// --------------------------------------------------------------
bool Atlantis::clbkPlaybackEvent (double simt, double event_t, const char *event_type, const char *event)
{
	if (!_stricmp (event_type, "JET")) {
		if (!_stricmp (event, "SRB")) {
			bManualSeparate = true;
			return true;
		}
		else if (!_stricmp (event, "ET")) {
			bManualSeparate = true;
			return true;
		}
	} else if (!_stricmp (event_type, "STATUS")) {
		if (!_stricmp (event, "SRB_IGNITION")) {
			status = 1;
			t0 = event_t + SRB_STABILISATION_TIME;
			return true;
		}
	} else if (!_stricmp (event_type, "ADJUST_LAUNCHTIME")) {
		sscanf (event, "%lf", &t0);
		return true;
	} else if (!_stricmp (event_type, "CARGODOOR")) {
		if (!_stricmp(event, "OPEN"))       plop->SetDoorAction (AnimState::OPENING, true);
		else if (!_stricmp(event, "CLOSE")) plop->SetDoorAction (AnimState::CLOSING, true);
		else if (!_stricmp(event, "ISOPEN")) plop->SetDoorAction (AnimState::OPEN, true);
		else if (!_stricmp(event, "ISCLOSED")) plop->SetDoorAction (AnimState::CLOSED, true);
		return true;
	} else if (!_stricmp (event_type, "GEAR")) {
		OperateLandingGear (!_stricmp (event, "UP") ? AnimState::CLOSING : AnimState::OPENING);
		return true;
	} else if (!_stricmp (event_type,"SPEEDBRAKE")) {
		OperateSpeedbrake (!_stricmp (event, "CLOSE") ? AnimState::CLOSING : AnimState::OPENING);
		return true;
	} else if (!_stricmp (event_type, "KUBAND")) {
		plop->SetKuAntennaAction (!_stricmp (event, "CLOSE") ? AnimState::CLOSING : AnimState::OPENING);
		return true;
	}

	return false;
}

// --------------------------------------------------------------
// Atlantis mesh loaded
// --------------------------------------------------------------
void Atlantis::clbkVisualCreated (VISHANDLE _vis, int refcount)
{
	if (refcount > 1) return; // we don't support more than one visual per object
	vis = _vis;

	SetVisualProperty(vis, VisualProp::CREATE_VC_PROBE, 0, typeid(FVECTOR3), &FVECTOR3(0.0f, 2.6f, 14.0f));

	// make sure the RMS attachment point is in sync with the animation state of the visual
	SetAttachmentParams (rms_attach, arm_tip[0], arm_tip[1]-arm_tip[0], arm_tip[2]-arm_tip[0]);
}

// --------------------------------------------------------------
// Atlantis mesh discarded
// --------------------------------------------------------------
void Atlantis::clbkVisualDestroyed (VISHANDLE _vis, int refcount)
{
	if (vis == _vis) vis = NULL;
}

// --------------------------------------------------------------
// Update mesh animation state
// --------------------------------------------------------------
void Atlantis::clbkAnimate (double simt)
{
	UpdateMesh ();
}

// --------------------------------------------------------------
// Respond to MFD mode change
// --------------------------------------------------------------
void Atlantis::clbkMFDMode (int mfd, int mode)
{
	oapiVCTriggerRedrawArea (-1, AID_CDR1_BUTTONS+mfd-MFD_LEFT);
}

// --------------------------------------------------------------
// Load generic glass cockpit mode
// --------------------------------------------------------------
bool Atlantis::clbkLoadGenericCockpit ()
{
	SetCameraOffset (_V(-0.67,2.55,14.4));
	SetCameraDefaultDirection (_V(0,0,1));
	return true;
}

// --------------------------------------------------------------
// register VC buttons for the 2 commander MFDs
// (accessible from commander position only)
// --------------------------------------------------------------
void Atlantis::RegisterVC_CdrMFD ()
{
	// activate MFD function buttons
	oapiVCSetAreaClickmode_Quadrilateral (AID_CDR1_BUTTONS, _V(-0.9239,2.0490,15.0595), _V(-0.7448,2.0490,15.0595),  _V(-0.9239,2.0280,15.0595), _V(-0.7448,2.0280,15.0595));
	oapiVCSetAreaClickmode_Quadrilateral (AID_CDR2_BUTTONS, _V(-0.6546,2.0490,15.0595), _V(-0.4736,2.0490,15.0595),  _V(-0.6546,2.0280,15.0595), _V(-0.4736,2.0280,15.0595));

    // D. Beachy: register+activate MFD power buttons
    const double powerButtonRadius = 0.0075; // radius of power button on each MFD
	oapiVCRegisterArea (AID_CDR1_PWR, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea (AID_CDR2_PWR, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_ONREPLAY);
    oapiVCSetAreaClickmode_Spherical(AID_CDR1_PWR, _V(-0.950, 2.060, 15.060), powerButtonRadius);  
    oapiVCSetAreaClickmode_Spherical(AID_CDR2_PWR, _V(-0.680, 2.060, 15.060), powerButtonRadius);  

	// register+activate MFD brightness buttons
	oapiVCRegisterArea (AID_CDR1_BRT, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea (AID_CDR2_BRT, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY);
	oapiVCSetAreaClickmode_Quadrilateral (AID_CDR1_BRT, _V(-0.729,2.0675,15.060), _V(-0.714,2.0675,15.060), _V(-0.729,2.0525,15.060), _V(-0.714,2.0525,15.060));
	oapiVCSetAreaClickmode_Quadrilateral (AID_CDR2_BRT, _V(-0.459,2.0675,15.060), _V(-0.444,2.0675,15.060), _V(-0.459,2.0525,15.060), _V(-0.444,2.0525,15.060));
}

// --------------------------------------------------------------
// register VC buttons for the 2 pilot MFDs
// (accessible from pilot position only)
// --------------------------------------------------------------
void Atlantis::RegisterVC_PltMFD ()
{
	// activate MFD function buttons
	oapiVCSetAreaClickmode_Quadrilateral (AID_PLT1_BUTTONS, _V(0.4759,2.0490,15.0595), _V(0.6568,2.0490,15.0595),  _V(0.4759,2.0280,15.0595), _V(0.6568,2.0280,15.0595));
	oapiVCSetAreaClickmode_Quadrilateral (AID_PLT2_BUTTONS, _V(0.7461,2.0490,15.0595), _V(0.9271,2.0490,15.0595),  _V(0.7461,2.0280,15.0595), _V(0.9271,2.0280,15.0595));

    // D. Beachy: register+activate MFD power buttons
    const double powerButtonRadius = 0.0075; // radius of power button on each MFD
	oapiVCRegisterArea (AID_PLT1_PWR, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea (AID_PLT2_PWR, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_ONREPLAY);
    oapiVCSetAreaClickmode_Spherical(AID_PLT1_PWR, _V( 0.450, 2.060, 15.060), powerButtonRadius);  
    oapiVCSetAreaClickmode_Spherical(AID_PLT2_PWR, _V( 0.720, 2.060, 15.060), powerButtonRadius);  

	// register+activate MFD brightness buttons
	oapiVCRegisterArea (AID_PLT1_BRT, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea (AID_PLT2_BRT, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY);
	oapiVCSetAreaClickmode_Quadrilateral (AID_PLT1_BRT, _V(0.671,2.0675,15.060), _V(0.686,2.0675,15.060), _V(0.671,2.0525,15.060), _V(0.686,2.0525,15.060));
	oapiVCSetAreaClickmode_Quadrilateral (AID_PLT2_BRT, _V(0.941,2.0675,15.060), _V(0.956,2.0675,15.060), _V(0.941,2.0525,15.060), _V(0.956,2.0525,15.060));
}

// --------------------------------------------------------------
// register VC buttons for the 5 MFDs on the central panel
// (accessible from commander and pilot positions)
// --------------------------------------------------------------
void Atlantis::RegisterVC_CntMFD ()
{
	// activate MFD function buttons
	oapiVCSetAreaClickmode_Quadrilateral (AID_MFD1_BUTTONS, _V(-0.3579,2.1451,15.0863), _V(-0.1770,2.1451,15.0863), _V(-0.3579,2.1241,15.0863), _V(-0.1770,2.1241,15.0863));
	oapiVCSetAreaClickmode_Quadrilateral (AID_MFD2_BUTTONS, _V(-0.3579,1.9143,15.0217), _V(-0.1770,1.9143,15.0217), _V(-0.3579,1.8933,15.0217), _V(-0.1770,1.8933,15.0217));
	oapiVCSetAreaClickmode_Quadrilateral (AID_MFD3_BUTTONS, _V(-0.0888,2.0288,15.0538), _V(0.0922,2.0288,15.0538), _V(-0.0888,2.0078,15.0538), _V(0.0922,2.0078,15.0538));
	oapiVCSetAreaClickmode_Quadrilateral (AID_MFD4_BUTTONS, _V(0.1795,2.1451,15.0863), _V(0.3604,2.1451,15.0863), _V(0.1795,2.1241,15.0863), _V(0.3604,2.1241,15.0863));
	oapiVCSetAreaClickmode_Quadrilateral (AID_MFD5_BUTTONS, _V(0.1795,1.9143,15.0217), _V(0.3604,1.9143,15.0217), _V(0.1795,1.8933,15.0217), _V(0.3604,1.8933,15.0217));

    // D. Beachy: register+activate MFD power buttons
    const double powerButtonRadius = 0.0075; // radius of power button on each MFD
	oapiVCRegisterArea (AID_MFD1_PWR, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea (AID_MFD2_PWR, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea (AID_MFD3_PWR, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea (AID_MFD4_PWR, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea (AID_MFD5_PWR, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_ONREPLAY);
    oapiVCSetAreaClickmode_Spherical(AID_MFD1_PWR, _V(-0.383, 2.153, 15.090), powerButtonRadius);  
    oapiVCSetAreaClickmode_Spherical(AID_MFD2_PWR, _V(-0.383, 1.922, 15.023), powerButtonRadius);  
    oapiVCSetAreaClickmode_Spherical(AID_MFD3_PWR, _V(-0.114, 2.037, 15.058), powerButtonRadius);  
    oapiVCSetAreaClickmode_Spherical(AID_MFD4_PWR, _V( 0.155, 2.153, 15.090), powerButtonRadius);  
    oapiVCSetAreaClickmode_Spherical(AID_MFD5_PWR, _V( 0.155, 1.922, 15.023), powerButtonRadius);  

	// register+activate MFD brightness buttons
	oapiVCRegisterArea (AID_MFD1_BRT, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea (AID_MFD2_BRT, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea (AID_MFD3_BRT, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea (AID_MFD4_BRT, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea (AID_MFD5_BRT, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY);
	oapiVCSetAreaClickmode_Quadrilateral (AID_MFD1_BRT, _V(-0.162,2.1605,15.090), _V(-0.147,2.1605,15.090), _V(-0.162,2.1455,15.090), _V(-0.147,2.1455,15.090));
	oapiVCSetAreaClickmode_Quadrilateral (AID_MFD2_BRT, _V(-0.162,1.9295,15.023), _V(-0.147,1.9295,15.023), _V(-0.162,1.9145,15.023), _V(-0.147,1.9145,15.023));
	oapiVCSetAreaClickmode_Quadrilateral (AID_MFD3_BRT, _V(0.107,2.0445,15.058), _V(0.122,2.0445,15.058), _V(0.107,2.0295,15.058), _V(0.122,2.0295,15.058));
	oapiVCSetAreaClickmode_Quadrilateral (AID_MFD4_BRT, _V(0.376,2.1605,15.090), _V(0.391,2.1605,15.090), _V(0.376,2.1455,15.090), _V(0.391,2.1455,15.090));
	oapiVCSetAreaClickmode_Quadrilateral (AID_MFD5_BRT, _V(0.376,1.9295,15.023), _V(0.391,1.9295,15.023), _V(0.376,1.9145,15.023), _V(0.391,1.9145,15.023));
}

// --------------------------------------------------------------
// register VC buttons for the aft MFD at the starbord panel
// (accessible from payload control position only)
// --------------------------------------------------------------
void Atlantis::RegisterVC_AftMFD ()
{
	// register+activate aft MFD function buttons
	SURFHANDLE tex1 = oapiGetTextureHandle (hOrbiterVCMesh, 7);
	oapiVCRegisterArea (AID_MFDA_BUTTONS, _R(0,127,255,140), PANEL_REDRAW_USER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBUP|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY, PANEL_MAP_BACKGROUND, tex1);
	oapiVCSetAreaClickmode_Quadrilateral (AID_MFDA_BUTTONS, _V(1.3862,2.2570,13.8686), _V(1.3862,2.2570,13.6894), _V(1.3678,2.2452,13.8686), _V(1.3678,2.2452,13.6894));

	// register+activate MFD power button
    const double powerButtonRadius = 0.0075; // radius of power button on each MFD
	oapiVCRegisterArea (AID_MFDA_PWR, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_ONREPLAY);
    oapiVCSetAreaClickmode_Spherical(AID_MFDA_PWR, _V(1.3929,2.2632,13.8947), powerButtonRadius);

	// register+activate MFD brightness buttons
	oapiVCRegisterArea (AID_MFDA_BRT, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY);
	oapiVCSetAreaClickmode_Quadrilateral (AID_MFDA_BRT, _V(1.4024,2.2675,13.6736), _V(1.4024,2.2675,13.6586), _V(1.3893,2.2590,13.6736), _V(1.3893,2.2590,13.6586));
}

// --------------------------------------------------------------
// Load virtual cockpit mode
// --------------------------------------------------------------
bool Atlantis::clbkLoadVC (int id)
{
	static VCHUDSPEC huds = {  // common HUD specs
		mesh_vc,            // nmesh
		GRP_VirtualHUD_VC,  // ngroup
		{0,0,0},            // hudcnt (to be filled)
		0.176558            // size
	};
	//static VCMFDSPEC mfds = {
	//	mesh_vc, 0
	//};
	static EXTMFDSPEC mfds = { // common MFD specs
		{0,0,0,0},          // pos
		mesh_vc,            // nmesh
		0,                  // ngroup (to be filled)
		MFD_SHOWMODELABELS, // flag
		5, 0,               // nbt1, nbt2
		512/6, 512/7        // bt_yofs, bt_ydist
	};
	static const int mfdgrp[10] = {
		GRP_CDR1_VC,GRP_CDR2_VC,GRP_PLT1_VC,GRP_PLT2_VC,
		GRP_MFD1_VC, GRP_MFD2_VC, GRP_MFD3_VC, GRP_MFD4_VC, GRP_MFD5_VC,
		GRP_MFD_aft_VC
	};

	bool ok = false;

	// register MFD function buttons
	// this needs to be done globally, so that the labels are correctly updated from all VC positions
	SURFHANDLE tex1 = oapiGetTextureHandle (hOrbiterVCMesh, 7);

	// commander MFD function buttons
	oapiVCRegisterArea (AID_CDR1_BUTTONS, _R(0,1,255,14), PANEL_REDRAW_USER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBUP|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY, PANEL_MAP_BACKGROUND, tex1);
	oapiVCRegisterArea (AID_CDR2_BUTTONS, _R(0,15,255,28), PANEL_REDRAW_USER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBUP|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY, PANEL_MAP_BACKGROUND, tex1);
	// pilot MFD function buttons
	oapiVCRegisterArea (AID_PLT1_BUTTONS, _R(0,29,255,42), PANEL_REDRAW_USER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBUP|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY, PANEL_MAP_BACKGROUND, tex1);
	oapiVCRegisterArea (AID_PLT2_BUTTONS, _R(0,43,255,56), PANEL_REDRAW_USER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBUP|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY, PANEL_MAP_BACKGROUND, tex1);
	// central console MFD function buttons
	oapiVCRegisterArea (AID_MFD1_BUTTONS, _R(0, 57,255, 70), PANEL_REDRAW_USER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBUP|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY, PANEL_MAP_BACKGROUND, tex1);
	oapiVCRegisterArea (AID_MFD2_BUTTONS, _R(0, 71,255, 84), PANEL_REDRAW_USER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBUP|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY, PANEL_MAP_BACKGROUND, tex1);
	oapiVCRegisterArea (AID_MFD3_BUTTONS, _R(0, 85,255, 98), PANEL_REDRAW_USER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBUP|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY, PANEL_MAP_BACKGROUND, tex1);
	oapiVCRegisterArea (AID_MFD4_BUTTONS, _R(0, 99,255,112), PANEL_REDRAW_USER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBUP|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY, PANEL_MAP_BACKGROUND, tex1);
	oapiVCRegisterArea (AID_MFD5_BUTTONS, _R(0,113,255,126), PANEL_REDRAW_USER, PANEL_MOUSE_LBDOWN|PANEL_MOUSE_LBUP|PANEL_MOUSE_LBPRESSED|PANEL_MOUSE_ONREPLAY, PANEL_MAP_BACKGROUND, tex1);

	switch (id) {
	case 0: // commander position
		SetCameraOffset (_V(-0.67,2.55,14.4));
		SetCameraDefaultDirection (_V(0,0,1));
		SetCameraMovement (_V(0,0,0.3), 0, 0, _V(-0.3,0,0), 75*RAD, -5*RAD, _V(0.3,0,0), -20*RAD, -27*RAD);
		huds.hudcnt = _V(-0.671257, 2.523535, 14.969);
		oapiVCSetNeighbours (-1, 1, -1, 2);

		RegisterVC_CdrMFD (); // activate commander MFD controls
		RegisterVC_CntMFD (); // activate central panel MFD controls

		ok = true;
		break;
	case 1: // pilot position
		SetCameraOffset (_V(0.67,2.55,14.4));
		SetCameraDefaultDirection (_V(0,0,1));
		SetCameraMovement (_V(0,0,0.3), 0, 0, _V(-0.3,0,0), 20*RAD, -27*RAD, _V(0.3,0,0), -75*RAD, -5*RAD);
		huds.hudcnt = _V(0.671257, 2.523535, 14.969);
		oapiVCSetNeighbours (0, -1, -1, 2);

		RegisterVC_PltMFD (); // activate pilot MFD controls
		RegisterVC_CntMFD (); // activate central panel MFD controls

		ok = true;
		break;
	case 2: // payload view position
		SetCameraOffset (_V(0.4,3.15,13.0));
		SetCameraDefaultDirection (_V(0,0,-1));
		SetCameraMovement (_V(0,-0.1,-0.1), 0, 80.0*RAD, _V(0.3,-0.3,0.15), 60.0*RAD, -50.0*RAD, _V(-0.8,0,0), 0, 0);
		oapiVCSetNeighbours (1, 0, -1, 0);

		RegisterVC_AftMFD (); // activate aft MFD controls
		plop->RegisterVC ();  // register panel R13L interface
		ok = true;
		break;
	}

	if (ok) {
		// register the HUDs (synced)
		oapiVCRegisterHUD (&huds);
		// register all MFD displays
		for (int i = 0; i < 10; i++) {
			mfds.ngroup = mfdgrp[i];
			oapiRegisterMFD (MFD_LEFT+i, &mfds);
		}
		// update panel R13L
		plop->UpdateVC();
	}
	return ok;
}

// --------------------------------------------------------------
// Respond to virtual cockpit mouse event
// --------------------------------------------------------------
bool Atlantis::clbkVCMouseEvent (int id, int event, VECTOR3 &p)
{
	static bool counting = false;
	static double t0 = 0.0;

	switch (id) {
	// handle MFD selection buttons
	case AID_CDR1_BUTTONS:
	case AID_CDR2_BUTTONS:
	case AID_PLT1_BUTTONS:
	case AID_PLT2_BUTTONS:
	case AID_MFD1_BUTTONS:
	case AID_MFD2_BUTTONS:
	case AID_MFD3_BUTTONS:
	case AID_MFD4_BUTTONS: 
	case AID_MFD5_BUTTONS:
	case AID_MFDA_BUTTONS: {
		int mfd = id-AID_CDR1_BUTTONS+MFD_LEFT;
		int bt = (int)(p.x*5.99);
		if (bt < 5) oapiProcessMFDButton (mfd, bt, event);
		else {
			if (event & PANEL_MOUSE_LBDOWN) {
				t0 = oapiGetSysTime();
				counting = true;
			} else if ((event & PANEL_MOUSE_LBUP) && counting) {
				oapiSendMFDKey (mfd, OAPI_KEY_F2);
				counting = false;
			} else if ((event & PANEL_MOUSE_LBPRESSED) && counting && (oapiGetSysTime()-t0 >= 1.0)) {
				oapiSendMFDKey (mfd, OAPI_KEY_F1);
				counting = false;
			}
		}
		} return true;

    // D. Beachy: handle power buttons
    case AID_CDR1_PWR:
    case AID_CDR2_PWR:
	case AID_PLT1_PWR:
	case AID_PLT2_PWR:
    case AID_MFD1_PWR:
    case AID_MFD2_PWR:
    case AID_MFD3_PWR:
	case AID_MFD4_PWR: 
	case AID_MFD5_PWR:
	case AID_MFDA_PWR: {
        int mfd = id - AID_CDR1_PWR+MFD_LEFT;
        oapiSendMFDKey(mfd, OAPI_KEY_ESCAPE);
        } return true;
              
	// handle MFD brightness buttons
	case AID_CDR1_BRT:
	case AID_CDR2_BRT:
	case AID_PLT1_BRT:
	case AID_PLT2_BRT:
	case AID_MFD1_BRT:
	case AID_MFD2_BRT:
	case AID_MFD3_BRT: 
	case AID_MFD4_BRT: 
	case AID_MFD5_BRT:
	case AID_MFDA_BRT: {
		static double t0, brt0;
		static bool up;
		int mfd = id-AID_CDR1_BRT;
		if (event & PANEL_MOUSE_LBDOWN) {
			up = (p.x >= 0.5);
			t0 = oapiGetSysTime();
			brt0 = mfdbright[mfd];
		} else if (event & PANEL_MOUSE_LBPRESSED) {
			double dt = oapiGetSysTime()-t0;
			double brt, dbrt = dt * 0.2;
			if (up) brt = min (1.0, brt0 + dbrt);
			else    brt = max (0.25, brt0 - dbrt);
			mfdbright[mfd] = brt;
			if (vis) {
				MATERIAL mat;
				memset (&mat, 0, sizeof(MATERIAL));
				mat.emissive.r = mat.emissive.g = mat.emissive.b = (float)brt;
				mat.emissive.a = 1.0f;
				DEVMESHHANDLE hMesh = GetDevMesh (vis, mesh_vc);
				oapiSetMaterial (hMesh, 10+mfd, &mat);
			}
		}
		} return false;

	// handle panel R13L events (payload bay operations)
	case AID_R13L:
		return plop->VCMouseEvent (id, event, p);
	}
	return false;
}

// --------------------------------------------------------------
// Respond to virtual cockpit area redraw request
// --------------------------------------------------------------
bool Atlantis::clbkVCRedrawEvent (int id, int event, SURFHANDLE surf)
{
	switch (id) {
	case AID_CDR1_BUTTONS:
	case AID_CDR2_BUTTONS:
	case AID_PLT1_BUTTONS:
	case AID_PLT2_BUTTONS:
	case AID_MFD1_BUTTONS:
	case AID_MFD2_BUTTONS:
	case AID_MFD3_BUTTONS:
	case AID_MFD4_BUTTONS: 
	case AID_MFD5_BUTTONS:
	case AID_MFDA_BUTTONS: {
		int mfd = id-AID_CDR1_BUTTONS+MFD_LEFT;
		RedrawPanel_MFDButton (surf, mfd);
		} return true;
	default:
		if (id >= AID_R13L_MIN && id <= AID_R13L_MAX)
			return plop->VCRedrawEvent (id, event, surf);
		break;
	}
	return false;
}

// --------------------------------------------------------------
// Respond to a HUD redraw request
// --------------------------------------------------------------
bool Atlantis::clbkDrawHUD (int mode, const HUDPAINTSPEC *hps, oapi::Sketchpad *skp)
{
	// draw the default HUD
	VESSEL4::clbkDrawHUD (mode, hps, skp);
	int cx = hps->CX, cy = hps->CY;

	// show OMS thrust marker
	if (status >= 3) {
		int omsy = cy + (int)(15.0*hps->Scale);
		int dx = (int)(1.0*hps->Scale);
		skp->Line (cx-2*dx, omsy, cx+2*dx, omsy);
		skp->Line (cx, omsy-dx, cx, omsy+dx);
	}

	// show RCS mode
	if (status >= 3 && oapiCockpitMode() == COCKPIT_VIRTUAL) {
		switch (GetAttitudeMode()) {
		case RCS_ROT:
			skp->Text (0, hps->H-13, "RCS ROT", 7);
			break;
		case RCS_LIN:
			skp->Text (0, hps->H-13, "RCS_LIN", 7);
			break;
		}
	}
	return true;
}

// --------------------------------------------------------------
// Keyboard interface handler (buffered key events)
// --------------------------------------------------------------
int Atlantis::clbkConsumeBufferedKey (DWORD key, bool down, char *kstate)
{
	if (!down) return 0; // only process keydown events

	if (KEYMOD_SHIFT (kstate)) {

		switch (key) {
		case OAPI_KEY_E:
			if (status != 3) return 1; // Allow MMU only after orbiter has detached from MT
			return 1;
		}	
	} else if (KEYMOD_CONTROL (kstate)) {
		switch (key) {
		case OAPI_KEY_SPACE: // open RMS control dialog
			oapiOpenDialogEx (g_Param.hDLL, IDD_CTRL, Atlantis_DlgProc, 0, this);
			return 1;
		case OAPI_KEY_B: // deploy/retract speedbrake
			if (!Playback()) RevertSpeedbrake ();
			return 1;
		case OAPI_KEY_U: // deploy/store Ku-band antenna
			if (!Playback()) plop->RevertKuAntennaAction ();
			return 1;
		}
	} else { // unmodified keys
		switch (key) {
		case OAPI_KEY_G:  // "Landing gear"
			if (!Playback()) RevertLandingGear ();
			return 1;
		case OAPI_KEY_J:  // "Jettison"
			if (!Playback()) bManualSeparate = true;
			return 1;
		case OAPI_KEY_K:  // "Cargo bay doors"
			if (!Playback()) plop->RevertDoorAction ();
			return 1;
		case OAPI_KEY_8:
			ToggleGrapple();
			return 1;
		case OAPI_KEY_9: 
			center_arm = true;
			return 1;
		case OAPI_KEY_E:
			do_eva = true;
			return 1;
		}
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
	g_Param.tkbk_label = oapiCreateSurface (LOADBMP (IDB_TKBKLABEL));

	// allocate GDI resources
	g_Param.font[0] = oapiCreateFont(-11, false, (char*)"Arial");
	g_Param.brush[0] = oapiCreateBrush(0x000000);
}

DLLCLBK void ExitModule (HINSTANCE hModule)
{
	oapiUnregisterCustomControls (hModule);
	oapiDestroySurface (g_Param.tkbk_label);
}

// --------------------------------------------------------------
// Vessel initialisation
// --------------------------------------------------------------
DLLCLBK VESSEL *ovcInit (OBJHANDLE hvessel, int flightmodel)
{
	return new Atlantis (hvessel, flightmodel);
}

// --------------------------------------------------------------
// Vessel cleanup
// --------------------------------------------------------------
DLLCLBK void ovcExit (VESSEL *vessel)
{
	if (vessel) delete (Atlantis*)vessel;
}

// ==============================================================
// Message callback function for Atlantis control dialog box
// ==============================================================

INT_PTR CALLBACK Atlantis_DlgProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	Atlantis *sts = (uMsg == WM_INITDIALOG ? (Atlantis*)lParam : (Atlantis*)oapiGetDialogContext (hWnd));
	// pointer to vessel instance was passed as dialog context

	switch (uMsg) {
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCANCEL:
			oapiCloseDialog (hWnd);
			return TRUE;
		case IDC_ASCENTAP:
			sts->CreateAscentAPDlg();
			break;
		case IDC_PLBAYOP:
			sts->plop->OpenDialog ();
			break;
		case IDC_RMSOP:
			oapiOpenDialogEx (g_Param.hDLL, IDD_RMS, RMS_DlgProc, 0, sts);
			break;
		}
		break;
	}
	return oapiDefDialogProc (hWnd, uMsg, wParam, lParam);
}

// ==============================================================
// Message callback function for RMS control dialog box
// ==============================================================

INT_PTR CALLBACK RMS_DlgProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	Atlantis *sts = (uMsg == WM_INITDIALOG ? (Atlantis*)lParam : (Atlantis*)oapiGetDialogContext (hWnd));
	// pointer to vessel instance was passed as dialog context

	const double step = 0.05*RAD;
	static double t0;
	double t1;
	HICON hIcon;

	switch (uMsg) {
	case WM_INITDIALOG:
		hIcon = LoadIcon (g_Param.hDLL, MAKEINTRESOURCE(IDI_UP));
		SendDlgItemMessage (hWnd, IDC_WRIST_PITCHUP, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
		SendDlgItemMessage (hWnd, IDC_ELBOW_PITCHUP, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
		SendDlgItemMessage (hWnd, IDC_SHOULDER_PITCHUP, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
		hIcon = LoadIcon (g_Param.hDLL, MAKEINTRESOURCE(IDI_DOWN));
		SendDlgItemMessage (hWnd, IDC_WRIST_PITCHDOWN, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
		SendDlgItemMessage (hWnd, IDC_ELBOW_PITCHDOWN, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
		SendDlgItemMessage (hWnd, IDC_SHOULDER_PITCHDOWN, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
		hIcon = LoadIcon (g_Param.hDLL, MAKEINTRESOURCE(IDI_LEFT));
		SendDlgItemMessage (hWnd, IDC_WRIST_YAWLEFT, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
		SendDlgItemMessage (hWnd, IDC_SHOULDER_YAWLEFT, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
		hIcon = LoadIcon (g_Param.hDLL, MAKEINTRESOURCE(IDI_RIGHT));
		SendDlgItemMessage (hWnd, IDC_WRIST_YAWRIGHT, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
		SendDlgItemMessage (hWnd, IDC_SHOULDER_YAWRIGHT, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
		hIcon = LoadIcon (g_Param.hDLL, MAKEINTRESOURCE(IDI_RRIGHT));
		SendDlgItemMessage (hWnd, IDC_WRIST_ROLLRIGHT, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
		hIcon = LoadIcon (g_Param.hDLL, MAKEINTRESOURCE(IDI_RLEFT));
		SendDlgItemMessage (hWnd, IDC_WRIST_ROLLLEFT, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
		SendDlgItemMessage (hWnd, IDC_SHOWGRAPPLE, BM_SETCHECK, oapiGetShowGrapplePoints() ? BST_CHECKED:BST_UNCHECKED, 0);
		SetWindowText (GetDlgItem (hWnd, IDC_GRAPPLE), sts->SatGrappled() ? "Release" : "Grapple");
		EnableWindow (GetDlgItem (hWnd, IDC_STOW), sts->SatGrappled() ? FALSE : TRUE);
		SetWindowText (GetDlgItem (hWnd, IDC_PAYLOAD), sts->SatStowed() ? "Purge" : "Arrest");
		EnableWindow (GetDlgItem (hWnd, IDC_PAYLOAD), sts->SatStowed() || sts->CanArrest() ? TRUE:FALSE);
		SetTimer (hWnd, 1, 50, NULL);
		t0 = oapiGetSimTime();
		return FALSE;
	case WM_DESTROY:
		KillTimer (hWnd, 1);
		return 0;
	case WM_TIMER:
		if (wParam == 1) {
			t1 = oapiGetSimTime();
			if (SendDlgItemMessage (hWnd, IDC_SHOULDER_YAWLEFT, BM_GETSTATE, 0, 0) & BST_PUSHED) {
				sts->arm_sy = min (1.0, sts->arm_sy + (t1-t0)*ARM_OPERATING_SPEED);
				sts->SetAnimationArm (sts->anim_arm_sy, sts->arm_sy);
			} else if (SendDlgItemMessage (hWnd, IDC_SHOULDER_YAWRIGHT, BM_GETSTATE, 0, 0) & BST_PUSHED) {
				sts->arm_sy = max (0.0, sts->arm_sy - (t1-t0)*ARM_OPERATING_SPEED);
				sts->SetAnimationArm (sts->anim_arm_sy, sts->arm_sy);
			} else if (SendDlgItemMessage (hWnd, IDC_SHOULDER_PITCHUP, BM_GETSTATE, 0, 0) & BST_PUSHED) {
				sts->arm_sp = min (1.0, sts->arm_sp + (t1-t0)*ARM_OPERATING_SPEED);
				sts->SetAnimationArm (sts->anim_arm_sp, sts->arm_sp);
			} else if (SendDlgItemMessage (hWnd, IDC_SHOULDER_PITCHDOWN, BM_GETSTATE, 0, 0) & BST_PUSHED) {
				sts->arm_sp = max (0.0, sts->arm_sp - (t1-t0)*ARM_OPERATING_SPEED);
				sts->SetAnimationArm (sts->anim_arm_sp, sts->arm_sp);
			} else if (SendDlgItemMessage (hWnd, IDC_ELBOW_PITCHUP, BM_GETSTATE, 0, 0) & BST_PUSHED) {
				sts->arm_ep = max (0.0, sts->arm_ep - (t1-t0)*ARM_OPERATING_SPEED);
				sts->SetAnimationArm (sts->anim_arm_ep, sts->arm_ep);
			} else if (SendDlgItemMessage (hWnd, IDC_ELBOW_PITCHDOWN, BM_GETSTATE, 0, 0) & BST_PUSHED) {
				sts->arm_ep = min (1.0, sts->arm_ep + (t1-t0)*ARM_OPERATING_SPEED);
				sts->SetAnimationArm (sts->anim_arm_ep, sts->arm_ep);
			} else if (SendDlgItemMessage (hWnd, IDC_WRIST_PITCHUP, BM_GETSTATE, 0, 0) & BST_PUSHED) {
				sts->arm_wp = min (1.0, sts->arm_wp + (t1-t0)*ARM_OPERATING_SPEED);
				sts->SetAnimationArm (sts->anim_arm_wp, sts->arm_wp);
			} else if (SendDlgItemMessage (hWnd, IDC_WRIST_PITCHDOWN, BM_GETSTATE, 0, 0) & BST_PUSHED) {
				sts->arm_wp = max (0.0, sts->arm_wp - (t1-t0)*ARM_OPERATING_SPEED);
				sts->SetAnimationArm (sts->anim_arm_wp, sts->arm_wp);
			} else if (SendDlgItemMessage (hWnd, IDC_WRIST_YAWLEFT, BM_GETSTATE, 0, 0) & BST_PUSHED) {
				sts->arm_wy = min (1.0, sts->arm_wy + (t1-t0)*ARM_OPERATING_SPEED);
				sts->SetAnimationArm (sts->anim_arm_wy, sts->arm_wy);
			} else if (SendDlgItemMessage (hWnd, IDC_WRIST_YAWRIGHT, BM_GETSTATE, 0, 0) & BST_PUSHED) {
				sts->arm_wy = max (0.0, sts->arm_wy - (t1-t0)*ARM_OPERATING_SPEED);
				sts->SetAnimationArm (sts->anim_arm_wy, sts->arm_wy);
			} else if (SendDlgItemMessage (hWnd, IDC_WRIST_ROLLLEFT, BM_GETSTATE, 0, 0) & BST_PUSHED) {
				sts->arm_wr = max (0.0, sts->arm_wr - (t1-t0)*ARM_OPERATING_SPEED);
				sts->SetAnimationArm (sts->anim_arm_wr, sts->arm_wr);
			} else if (SendDlgItemMessage (hWnd, IDC_WRIST_ROLLRIGHT, BM_GETSTATE, 0, 0) & BST_PUSHED) {
				sts->arm_wr = min (1.0, sts->arm_wr + (t1-t0)*ARM_OPERATING_SPEED);
				sts->SetAnimationArm (sts->anim_arm_wr, sts->arm_wr);
			}
			t0 = t1;
		}
		if (!sts->center_arm) EnableWindow (GetDlgItem (hWnd, IDC_GRAPPLE), TRUE);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCANCEL:
			oapiCloseDialog (hWnd);
			return TRUE;
		case IDC_STOW:
			if (sts->center_arm = !sts->center_arm) {
				sts->center_arm_t = oapiGetSimTime();
				EnableWindow (GetDlgItem (hWnd, IDC_GRAPPLE), FALSE);
			}
			return 0;
		case IDC_GRAPPLE:
			sts->ToggleGrapple();
			return 0;
		case IDC_PAYLOAD:
			sts->ToggleArrest();
			return 0;
		case IDC_SHOWGRAPPLE:
			oapiSetShowGrapplePoints (SendDlgItemMessage (hWnd, IDC_SHOWGRAPPLE, BM_GETCHECK, 0, 0) == BST_CHECKED ? true : false);
			return 0;
		}
		break;
	}
	return oapiDefDialogProc (hWnd, uMsg, wParam, lParam);
}

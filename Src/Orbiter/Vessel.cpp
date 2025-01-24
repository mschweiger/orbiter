// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ==============================================================
//              This file is part of ORBITER
//          Copyright 2000-2007 Martin Schweiger
//
// Vessel.cpp
// Implementation of class Vessel
//
// Description:
//     The Vessel class is used to define all manmade spaceborne
//     objects (spacecraft, stations etc.) which are exposed to
//     the ambient gravitational field (but do not themselves
//     contribute to the field) and are optionally self-propelled.
//
// Inheritance:
//     Body --> RigidBody --> Vessel
// ==============================================================

#define OAPI_IMPLEMENTATION

#include "Orbiter.h"
#include "Vessel.h"
#include "Supervessel.h"
#include "Config.h"
#include "Camera.h"
#include "Pane.h"
#include "Panel2D.h"
#include "Element.h"
#include "Psys.h"
#include "Base.h"
#include "Mfd.h"
#include "Keymap.h"
#include "Log.h"
#include "Dialogs.h"
#include "State.h"
#include "Util.h"
#include "elevmgr.h"
#include <fstream>
#include <iomanip>
#include <stdio.h>
#include <stdlib.h>
#include <string>

using namespace std;

extern Orbiter *g_pOrbiter;
extern Vessel *g_focusobj;
extern Camera *g_camera;
extern Pane *g_pane;
extern Select *g_select;
extern InputBox *g_input;
extern TimeData td;
extern PlanetarySystem *g_psys;
extern bool g_bStateUpdate;

extern char DBG_MSG[256];

const double KEYDELAY = 0.5;
const double MIN_DOCK_DIST = 0.5;

const char noclass[] = "unknown";
const double holdaltDT = 0.1; // update interval for "hold altitude" mode

static VECTOR3 DefExhaustDir[4] = {{0,0,-1}, {0,0,1}, {0,-1,0}, {0,0,-1}};
static Vector DefAttExhaustDir[12] = {
	Vector(0,-1,0), Vector(0,1,0), Vector(0,1,0), Vector(0,-1,0),
	Vector(-1,0,0), Vector(1,0,0), Vector(1,0,0), Vector(-1,0,0),
	Vector(0,-1,0), Vector(0,1,0), Vector(0,1,0), Vector(0,-1,0)
};

// ==============================================================
// class Vessel
// ==============================================================

Vessel::Vessel (const PlanetarySystem *psys, const char *_name, const char *_classname, const VESSELSTATUS &status)
: VesselBase()
{
	name = _name;

	if (!_classname) _classname = _name;
	classname = new char[strlen(_classname)+1]; TRACENEW
	strcpy (classname, _classname);

	ifstream classf;
	if (!OpenConfigFile (classf))
		g_pOrbiter->TerminateOnError(); // PANIC!

	// Set defaults
	DefaultGenericCaps ();
	SetDefaultState ();

	// Load class DLL module if available
	LoadModule (classf);

	// Set class capabilities
	SetClassCaps (classf);

	el = new Elements; TRACENEW

	// Set status from parameters
	SetState (status);
	Setup ();
	PostCreation ();
	InitSupervessel (false); // bind vessel into superstructure if applicable
}

// ==============================================================

Vessel::Vessel (const PlanetarySystem *psys, const char *_name, const char *_classname, const void *status)
: VesselBase()
{
	name = _name;

	if (!_classname) _classname = _name;
	classname = new char[strlen(_classname)+1]; TRACENEW
	strcpy (classname, _classname);

	ifstream classf;
	if (!OpenConfigFile (classf))
		g_pOrbiter->TerminateOnError(); // PANIC!

	// Set defaults
	DefaultGenericCaps ();
	SetDefaultState ();

	// Load class DLL module if available
	LoadModule (classf);

	// Set class capabilities
	SetClassCaps (classf);

	el = new Elements; TRACENEW

	// Set status from parameters
	if (modIntf.v->Version() >= 1)
		((VESSEL2*)modIntf.v)->clbkSetStateEx (status);
	else
		SetStateEx (status);
	Setup ();
	PostCreation ();
	InitSupervessel (false); // bind vessel into superstructure if applicable
}

// ==============================================================

Vessel::Vessel (const PlanetarySystem *psys, const char *_name, const char *_classname, ifstream &ifs)
: VesselBase()
{
	char cbuf[256];

	sprintf (cbuf, "%s (%s)", _classname ? _classname : _name, _name);
	g_pOrbiter->OutputLoadStatus (cbuf, 0);

	name = _name;

	if (!_classname) _classname = _name;
	classname = new char[strlen(_classname)+1]; TRACENEW
	strcpy (classname, _classname);

	ifstream classf;
	if (!OpenConfigFile (classf))
		g_pOrbiter->TerminateOnError(); // PANIC!

	// Set defaults
	DefaultGenericCaps ();
	SetDefaultState ();

	// Load class DLL module if available
	LoadModule (classf);

	// Set class capabilities
	SetClassCaps (classf);

	el = new Elements; TRACENEW

	// Read status from scenario file
	Read (ifs);
	Setup ();

	// note that PostCreation is not called at this point, because vessels
	// created from the scenario stream get post-created by psys
	// once all are created.
}

// ==============================================================

Vessel::~Vessel ()
{
	FRecorder_Clear();
	ClearDockDefinitions ();
	if (modIntf.ovcExit) modIntf.ovcExit(modIntf.v);
	if (modIntf.coreCreated) delete modIntf.v;
	if (classname) {
		delete []classname;
		classname = NULL;
	}
	ClearMeshes();
	ClearThrusterDefinitions();
	ClearPropellantResources();
	ClearAirfoilDefinitions();
	ClearControlSurfaceDefinitions();
	ClearVariableDragElements();
	ClearBeacons();
	ClearAttachments();
	ClearAnimSeqs(); // obsolete
	ClearAnimations (false);
	ClearTouchdownPoints();
	ClearReentryStreams();
	ClearLightEmitters();
	UnregisterMFDModes();

	if (xpdr) {
		g_psys->BroadcastVessel (MSG_KILLNAVSENDER, xpdr);
		delete xpdr;
	}
	if (nnav) {
		delete []nav;
		nav = NULL;
	}
	if (onlinehelp) {
		delete []onlinehelp;
		onlinehelp = NULL;
	}

	delete[]forcevec;
	delete[]forcepos;

	ClearModule();
	g_pOrbiter->UpdateDeallocationProgress();
}

// ==============================================================

bool Vessel::OpenConfigFile (ifstream &cfgfile) const
{
	char cbuf[256];
	strcpy (cbuf, "Vessels\\");
	strcat (cbuf, classname ? classname : name.c_str());
	// first search in $CONFIGDIR\Vessels
	cfgfile.open (g_pOrbiter->ConfigPath (cbuf));
	if (cfgfile.good()) return true;
	else cfgfile.clear();
	// next search in $CONFIGDIR
	cfgfile.open (g_pOrbiter->ConfigPath (cbuf+8));
	if (cfgfile.good()) return true;
	else {
		cfgfile.clear();
		LOGOUT_ERR_FILENOTFOUND_MSG(g_pOrbiter->ConfigPath(cbuf + 8), "No vessel class configuration file found for: %s", classname ? classname : name);
		//LogOut (">>> ERROR: No vessel class configuration file found for:");
		//LOGOUT_ERR(classname ? classname : name);
		return false;
	}
}

// ==============================================================

void Vessel::SetClassCaps (ifstream &classf)
{
	// Query module for caps
	if (modIntf.v->Version() >= 1)      // VESSEL2 interface
		((VESSEL2*)modIntf.v)->clbkSetClassCaps ((FILEHANDLE)&classf);

	// Read specs from class or vessel cfg file
	ReadGenericCaps (classf);
}

// ==============================================================

void Vessel::SetDefaultState ()
{
	int i;

	VesselBase::SetDefaultState ();

	Flin.Set (0,0,0);  // sum of linear forces
	Amom.Set (0,0,0);  // torque (sum of angular moments)
	Flin_add.Set (0,0,0);
	Amom_add.Set (0,0,0);
	Thrust.Set (0,0,0);
	Weight.Set (0,0,0);
	weight_valid        = false;
	torque_valid        = false;
	fmass = pfmass      = 0.0;
	cbody               = 0;
	landtgt             = 0;
	nport               = (DWORD)-1;
	lstatus             = 0;
	navmode             = 0;
	surfprm_valid       = false;
	pyp_valid           = false;
	closedock.dist      = 1e50;
	closedock.vessel    = 0;
	closedock.dock      = 0;
	undock_t            = -1000;
	proxyvessel         = 0;
	supervessel         = 0;
	scanvessel          = 0;
	attmode             = 1;
	ctrlsurfmode        = 0;
	for (i = 0; i < 6; i++)
		ctrlsurf_level[i].ttgt = ctrlsurf_level[i].ptgt = ctrlsurf_level[i].curr = 0.0;
	CtrlSurfSyncMode    = 0;
	for (i = 0; i < 2; i++)
		wbrake[i] = wbrake_permanent[i] = wbrake_override[i] = 0.0;
	nosesteering        = false;
	nosewheeldir        = 0.0;
	bGroundProximity    = false;
	sp.is_in_atm        = false;
	m_bThrustEngaged    = false;
	bForceActive        = false;
	rpressure           = g_pOrbiter->Cfg()->CfgPhysicsPrm.bRadiationPressure;
	Lift = Drag = SideForce = 0.0;
	attach_status.pname = 0;
	hudskp              = NULL;
	next_hullvtx        = 0;

	lightfac            = 1.0;
	lightfac_T0 = lightfac_T1 = -1.0;

	nforcevec = 0;
	forcevecbuf = 10;
	forcevec = new Vector[forcevecbuf];
	forcepos = new Vector[forcevecbuf];

	proxyT    = -(double)rand()*100.0/(double)RAND_MAX - 1.0;
	commsT    = -(double)rand()*5.0/(double)RAND_MAX - 1.0;
	// distribute update times
	FRecorder_Reset();
}

// ==============================================================

void Vessel::DefaultGenericCaps ()
{
	int i;

	meshlist           = 0;
	nmesh              = 0;
	mesh_crc           = 0;
	nanim              = 0;
	size               = 10.0;
	clipradius         = 0.0; // flag for clipradius=size
	vislimit           = spotlimit = 1e-3;
	emass              = 1e3;
	m_defaultIsp       = 5e4;
	cog_elev           = 5.0;
	CWz[0]             = 0.1;
	CWz[1]             = 0.3;
	CWx                = 0.3;
	CWy                = 0.3;
	wingaspect         = 1.0;
	wingeff            = 2.8;
	trim_scale         = 0.0;
	for (i = 0; i < 6; i++)
		ctrlsurf_level[i].delay = 1.0;
	bElevTrim          = false;
	pitch_moment_scale = 0.0;
	bank_moment_scale  = 0.0;
	cs.Set             (20,20,20);
	rdrag.Set          (0.01,0.01,0.01);
	campos.Set         (0,0,0);
	camdir0.Set        (0,0,1);    // "forward"
	camtilt0           = 0.0;
	camcatchangle      = RAD*5.0;
	camdp_left = camdp_right = 0.8*Pi;
	camdt_up   = camdt_down  = 0.8*Pi05;

	// Default (arbitrary) touchdown points
	// This is only a security measure if SetTouchdownPoints is not called
	mu                 = 0.5;  // default isotropic/lateral friction coefficient
	mu_lng             = 0.1;  // default longitudinal friction coefficient
	TOUCHDOWNVTX tdvtx[3];
	tdvtx[0].pos = _V( 0,-2,10);
	tdvtx[1].pos = _V(-3,-2,-5);
	tdvtx[2].pos = _V( 3,-2,-5);
	for (i = 0; i < 3; i++) {
		tdvtx[i].stiffness = 1e6;
		tdvtx[i].damping   = 1e5;
		tdvtx[i].mu        = mu;
		tdvtx[i].mu_lng    = mu_lng;
	}
	ntouchdown_vtx = 0;
	SetTouchdownPoints (tdvtx, 3);
	max_wbrake_F       = 1e5;

	ndock              = 0;
	dockmode           = 1;
	npattach=ncattach  = 0;
	attach             = 0;
	enablefocus        = true;
	extpassmesh        = false;
	nexhaust           = 0;
	noexhaust          = 0;
	ncontrail          = 0;
	nreentrystream     = 0;
	nemitter           = 0;
	ntank              = 0;
	def_tank           = 0;
	for (i = 0; i < 6; i++)
		max_angular_moment[i] = 0.0;

	nairfoil           = 0;
	nctrlsurf          = 0;
	ndragel            = 0;
	nnav               = 0;
	nbeacon            = 0;
	exhaust_id         = 0;
	animcount          = 0;
	nanimseq           = 0; // obsolete
	LiftCoeff          = 0;
	burnfuel           = g_pOrbiter->Cfg()->CfgLogicPrm.bLimitedFuel;
	xpdr               = 0;

	nmfdmode           = 0;

	reentry.do_render  = true;
	reentry.lscale     = 9.0f;
	reentry.wscale     = 2.0f;
	reentry.plimit     = 6e7f;
	reentry.tex        = 0;
	onlinehelp         = 0;
}

// ==============================================================

void Vessel::ReadGenericCaps (ifstream &ifs)
{
	char item[256], cbuf[256];
	UINT i;
	VECTOR3 tmp;
	double d;
	bool b;

	// recursively read base class specs
	if (GetItemString (ifs, "BaseClass", cbuf)) {
		ifstream basef (g_pOrbiter->ConfigPath (cbuf));
		if (basef) ReadGenericCaps (basef);
	}

	// read base class parameters
	RigidBody::ReadGenericCaps (ifs);

	if (GetItemString(ifs, "MeshName", cbuf)) {
		// preload mesh template
		MESHHANDLE mesh = (MESHHANDLE)g_pOrbiter->meshmanager.LoadMesh(cbuf);
		if (mesh)
			AddMesh(mesh);
		else
			g_pOrbiter->TerminateOnError(); // we assume that vessel meshes are required
	}

	if (GetItemString (ifs, "CollisionHull", cbuf))
		TouchdownPointsFromFile (cbuf);

	if (onlinehelp) {
		delete []onlinehelp;
		onlinehelp = NULL;
	}
	if (GetItemString (ifs, "Help", cbuf)) {
		onlinehelp = new char[strlen(cbuf)+1]; TRACENEW
		strcpy (onlinehelp, cbuf);
	}
		
	GetItemBool   (ifs, "EnableFocus", enablefocus);
	GetItemReal   (ifs, "Size", size);
	GetItemReal   (ifs, "ClipRadius", clipradius);
	GetItemReal   (ifs, "Mass", emass);
	GetItemVector (ifs, "AlbedoRGB", albedo);

	// init transponder
	if (GetItemBool (ifs, "EnableXPDR", b) && b) {
		if (!xpdr) { xpdr = new Nav_XPDR (this, NAV_RADIO_FREQ_MIN); TRACENEW }
		int step;
		if (GetItemInt (ifs, "XPDR", step)) xpdr->SetStep (step);
	}

	// fuel resources
	for (i = 0;; i++) {
		double maxmass, efficiency = 1.0;
		sprintf (item, "PropellantResource%d", i+1);
		if (!GetItemString (ifs, item, cbuf)) break;
		if (sscanf (cbuf, "%lf%lf", &maxmass, &efficiency))
			CreatePropellantResource (maxmass, -1, efficiency);
	}

	if (!ntank && GetItemReal (ifs, "MaxFuel", d)) { // obsolete
		if (ntank) SetPropellantMaxMass (tank[0], d);
		else       CreatePropellantResource (d);
	}

	GetItemReal   (ifs, "Isp", m_defaultIsp);

	if (GetItemReal (ifs, "MaxMainThrust", d))
		SetMaxThrust_old (ENGINE_MAIN, d);
	if (GetItemReal (ifs, "MaxRetroThrust", d))
		SetMaxThrust_old (ENGINE_RETRO, d);
	if (GetItemReal (ifs, "MaxHoverThrust", d))
		SetMaxThrust_old (ENGINE_HOVER, d);
	if (GetItemReal (ifs, "MaxAttitudeThrust", d))
		CreateDefaultAttitudeSet (d);

	if (GetItemString (ifs, "TouchdownPoints", cbuf)) {
		TOUCHDOWNVTX tdvtx[3];
		for (i = 0; i < 3; i++) {
			tdvtx[i].stiffness = 1e6;
			tdvtx[i].damping   = 1e5;
			tdvtx[i].mu        = mu;
			tdvtx[i].mu_lng    = mu_lng;
		}
		if (sscanf (cbuf, "%lf%lf%lf%lf%lf%lf%lf%lf%lf",
			&tdvtx[0].pos.x, &tdvtx[0].pos.y, &tdvtx[0].pos.z,
			&tdvtx[1].pos.x, &tdvtx[1].pos.y, &tdvtx[1].pos.z,
			&tdvtx[2].pos.x, &tdvtx[2].pos.y, &tdvtx[2].pos.z) == 9)
			SetTouchdownPoints (tdvtx, 3);
	} else 
		GetItemReal (ifs, "COG_OverGround", cog_elev);  // obsolete

	if (GetItemString (ifs, "CW", cbuf))
		sscanf (cbuf, "%lf%lf%lf%lf", CWz, CWz+1, &CWx, &CWy);
	GetItemReal   (ifs, "WingAspect", wingaspect);
	GetItemReal   (ifs, "WingEffectiveness", wingeff);
	GetItemVector (ifs, "CrossSections", cs);
	GetItemVector (ifs, "RotResistance", rdrag);
	GetItemVector (ifs, "CameraOffset", campos);

	Vector ref, dir, rot;
	if (GetItemVector (ifs, "DockRef", ref) &&
		GetItemVector (ifs, "DockDir", dir) &&
		GetItemVector (ifs, "DockRot", rot)) {
		if (ndock) SetDockParams (dock[0], ref, dir, rot);
		else       CreateDock (ref, dir, rot);
	}


	// main engine exhaust refs
	if (m_thrusterGroupDef[0].ts.size()) {
		for (i = 0;; i++) {
			sprintf (cbuf, "MEngineRef%d", i+1);
			if (!GetItemVECTOR (ifs, cbuf, tmp)) break;
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[0].ts[0], NULL, &tmp, NULL, 16.0, 1.0, 0.0, 0.0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
	}
	// retro engine exhaust refs
	if (m_thrusterGroupDef[1].ts.size()) {
		for (i = 0;; i++) {
			sprintf (cbuf, "REngineRef%d", i+1);
			if (!GetItemVECTOR (ifs, cbuf, tmp)) break;
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[1].ts[0], NULL, &tmp, NULL, 8.0, 0.5, 0.0, 0.0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
	}
	// hover engine exhaust refs
	if (m_thrusterGroupDef[2].ts.size()) {
		for (i = 0;; i++) {
			sprintf (cbuf, "HEngineRef%d", i+1);
			if (!GetItemVECTOR (ifs, cbuf, tmp)) break;
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[2].ts[0], NULL, &tmp, NULL, 12.0, 1.0, 0.0, 0.0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
	}
	// attitude thruster exhaust refs
	if (GetItemVECTOR (ifs, "AttRefX00", tmp)) {
		if (m_thrusterGroupDef[THGROUP_ATT_PITCHUP].ts.size() >= 1) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_PITCHUP].ts[0], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
		if (m_thrusterGroupDef[THGROUP_ATT_UP].ts.size()) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_UP].ts[0], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
	}
	if (GetItemVECTOR (ifs, "AttRefX01", tmp)) {
		if (m_thrusterGroupDef[THGROUP_ATT_PITCHUP].ts.size() >= 2) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_PITCHUP].ts[1], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
		if (m_thrusterGroupDef[THGROUP_ATT_DOWN].ts.size()) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_DOWN].ts[0], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
	}
	if (GetItemVECTOR (ifs, "AttRefX10", tmp)) {
		if (m_thrusterGroupDef[THGROUP_ATT_PITCHDOWN].ts.size() >= 1) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_PITCHDOWN].ts[0], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
		if (m_thrusterGroupDef[THGROUP_ATT_DOWN].ts.size()) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_DOWN].ts[0], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
	}
	if (GetItemVECTOR (ifs, "AttRefX11", tmp)) {
		if (m_thrusterGroupDef[THGROUP_ATT_PITCHDOWN].ts.size() >= 2) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_PITCHDOWN].ts[1], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
		if (m_thrusterGroupDef[THGROUP_ATT_UP].ts.size()) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_UP].ts[0], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
	}
	if (GetItemVECTOR (ifs, "AttRefY01", tmp)) {
		if (m_thrusterGroupDef[THGROUP_ATT_YAWLEFT].ts.size() >= 1) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_YAWLEFT].ts[0], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
		if (m_thrusterGroupDef[THGROUP_ATT_LEFT].ts.size()) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_LEFT].ts[0], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
	}
	if (GetItemVECTOR (ifs, "AttRefY00", tmp)) {
		if (m_thrusterGroupDef[THGROUP_ATT_YAWLEFT].ts.size() >= 2) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_YAWLEFT].ts[1], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
		if (m_thrusterGroupDef[THGROUP_ATT_RIGHT].ts.size()) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_RIGHT].ts[0], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
	}
	if (GetItemVECTOR (ifs, "AttRefY11", tmp)) {
		if (m_thrusterGroupDef[THGROUP_ATT_YAWRIGHT].ts.size() >= 1) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_YAWRIGHT].ts[0], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
		if (m_thrusterGroupDef[THGROUP_ATT_RIGHT].ts.size()) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_RIGHT].ts[0], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
	}
	if (GetItemVECTOR (ifs, "AttRefY10", tmp)) {
		if (m_thrusterGroupDef[THGROUP_ATT_YAWRIGHT].ts.size() >= 2) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_YAWRIGHT].ts[1], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
		if (m_thrusterGroupDef[THGROUP_ATT_LEFT].ts.size()) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_LEFT].ts[0], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
	}
	if (GetItemVECTOR (ifs, "AttRefZ00", tmp))
		if (m_thrusterGroupDef[THGROUP_ATT_BANKRIGHT].ts.size() >= 1) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_BANKRIGHT].ts[0], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
	if (GetItemVECTOR (ifs, "AttRefZ01", tmp))
		if (m_thrusterGroupDef[THGROUP_ATT_BANKRIGHT].ts.size() >= 2) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_BANKRIGHT].ts[1], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
	if (GetItemVECTOR (ifs, "AttRefZ11", tmp))
		if (m_thrusterGroupDef[THGROUP_ATT_BANKLEFT].ts.size() >= 1) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_BANKLEFT].ts[0], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
	if (GetItemVECTOR (ifs, "AttRefZ10", tmp))
		if (m_thrusterGroupDef[THGROUP_ATT_BANKLEFT].ts.size() >= 2) {
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_BANKLEFT].ts[1], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
	if (m_thrusterGroupDef[THGROUP_ATT_BACK].ts.size()) {
		for (i = 0;; i++) {
			sprintf (cbuf, "LongAttRef0%d", i);
			if (!GetItemVECTOR (ifs, cbuf, tmp)) break;
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_BACK].ts[0], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
	}
	if (m_thrusterGroupDef[THGROUP_ATT_FORWARD].ts.size()) {
		for (i = 0;; i++) {
			sprintf (cbuf, "LongAttRef1%d", i);
			if (!GetItemVECTOR (ifs, cbuf, tmp)) break;
			EXHAUSTSPEC es = {(THRUSTER_HANDLE)m_thrusterGroupDef[THGROUP_ATT_FORWARD].ts[0], NULL, &tmp, NULL, 3, 0.39, 0, 0, NULL, EXHAUST_CONSTANTPOS};
			AddExhaust (&es);
		}
	}

	if (FindLine (ifs, "BEGIN_DOCKLIST")) {
		for (;;) {
			Vector pos, dir, rot;
			int n, ids_step;
			if (!ifs.getline (cbuf, 256) || !_strnicmp (cbuf, "END_DOCKLIST", 12)) break;
			n = sscanf (cbuf, "%lf%lf%lf%lf%lf%lf%lf%lf%lf%d",
				&pos.x, &pos.y, &pos.z,
				&dir.x, &dir.y, &dir.z,
				&rot.x, &rot.y, &rot.z,
				&ids_step);
			PortSpec *ps = CreateDock (pos, dir, rot);
			if (n >= 10)
				SetDockIDS (ps, (float)(ids_step*0.05 + NAV_RADIO_FREQ_MIN));
		}
	}

	if (FindLine (ifs, "BEGIN_ATTACHMENT")) {
		Vector pos, dir, rot;
		char type;
		int n;
		bool toparent;
		for (;;) {
			if (!ifs.getline (cbuf, 256) || !_strnicmp (cbuf, "END_ATTACHMENT", 14)) break;
			n = sscanf (trim_string (cbuf), "%c%lf%lf%lf%lf%lf%lf%lf%lf%lf%s",
				&type,
				&pos.x, &pos.y, &pos.z,
				&dir.x, &dir.y, &dir.z,
				&rot.x, &rot.y, &rot.z,
				item);
			toparent = toupper(type) == 'P';
			if (n < 11) item[0] = '\0';
			CreateAttachment (toparent, pos, dir, rot, item);
		}
	}

	if (enablefocus && !nnav) InitNavRadios (2);
	// by default, active vessels support 2 NAV radios
}

// ==============================================================

void Vessel::Setup ()
{
	vd_forw = cs.z * 0.5*CWz[0];
	vd_back = cs.z * 0.5*CWz[1];
	vd_vert = cs.y * 0.5*CWy;
	vd_side = cs.x * 0.5*CWx;
	wingfactor = wingaspect * wingeff;
	if (pmi.x < 0.0) pmi.x = 2*size;
	if (pmi.y < 0.0) pmi.y = 2*size;
	if (pmi.z < 0.0) pmi.z = size;
	UpdateMass ();
	orthoaxis = 0;
	kill_pending = false;
	if (bFRplayback = bRequestPlayback) {
		const char *playbackdir = g_pOrbiter->PState()->PlaybackDir();
		//if (!playbackdir) playbackdir = ScenarioName;
		FRecorder_Read (playbackdir);
	}
}

// ==============================================================

void Vessel::PostCreation ()
{
	DWORD i, j, k;

	// define docking status
	for (i = 0; i < ndock; i++) {
		if (dock[i]->mate && !g_psys->isVessel(dock[i]->mate)) {
			// decode vessel name
			bool found = false;
			void *tmp;
			BYTE *bt = (BYTE*)&tmp;
			for (j = 0; j < g_psys->nVessel(); j++) {
				Vessel *v = g_psys->GetVessel(j);
				const char *name = v->Name();
				tmp = 0;
				for (k = 0; name[k]; k++) bt[k%4] += name[k];
				if (tmp == dock[i]->mate) {
					dock[i]->mate = v;
					found = true;
					break;
				}
			}
			if (!found) {
				dock[i]->mate = 0;
				dock[i]->matedock = 0;
			}
		}
	}
	// if not customised at this point:
	SetDefaultReentryStream ();
}

void Vessel::ModulePostCreation ()
{
	// now let the module do its post-creation stuff
	if (modIntf.v->Version() >= 1)
		((VESSEL2*)modIntf.v)->clbkPostCreation ();
}

// ==============================================================

void Vessel::ProcessMessage (DWORD msg, void *ptr)
{
	DWORD i;

	switch (msg) {
	case MSG_KILLVESSEL:
		Destroying ((Vessel*)ptr);
		break;
	case MSG_KILLNAVSENDER:
		for (i = 0; i < nnav; i++)
			if (nav[i].sender == (const Nav*)ptr)
				nav[i].sender = 0;
		break;
	}
}

// ==============================================================

void Vessel::OptionChanged(DWORD cat, DWORD item)
{
	if (cat == OPTCAT_VESSEL) {
		switch (item) {
		case OPTITEM_VESSEL_LIMITEDFUEL:
			burnfuel = g_pOrbiter->Cfg()->CfgLogicPrm.bLimitedFuel;
			break;
		}
	}
}

// ==============================================================

void Vessel::Destroying (const Vessel *vessel)
{
	if (vessel == this) { // it's us!
		Undock (ALLDOCKS, 0, 0.0); // undock all docked vessels
		DetachFromParent();
		for (DWORD i = 0; i < ncattach; i++)
			DetachChild (cattach[i]);
		if (fstatus == FLIGHTSTATUS_LANDED && proxybase)
			proxybase->ReportTakeoff (this);
	} else {
		if (vessel == proxyvessel)
			proxyT = td.SimT0-1; // force proxy update
		if (vessel == closedock.vessel)
			closedock.vessel = 0;
	}
}

// ==============================================================

const VesselBase *Vessel::GetSuperStructure () const
{
	return supervessel;

}

// ==============================================================

void Vessel::RPlace (const Vector &rpos, const Vector &rvel)
{
	if (supervessel) supervessel->RPlace (rpos, rvel, this);
	else             RPlace_individual (rpos, rvel);
}

// ==============================================================

void Vessel::RPlace_individual (const Vector &rpos, const Vector &rvel)
{
	// this version enforces the repositioning of the vessel without reporting
	// to the supervessel

	if (bFRplayback) return; // ignore explicit state vector setting during playback
	fstatus = FLIGHTSTATUS_FREEFLIGHT;
	bSurfaceContact = false;
	RigidBody::RPlace (rpos, rvel);
	cpos = s0->pos - cbody->GPos();
 	cvel = s0->vel - cbody->GVel();

	g_psys->ScanGFieldSources (&s0->pos, this, &gfielddata);
	acc = g_psys->Gacc (s0->pos, this, &gfielddata); // init acceleration due to G-field
	el_valid = bOrbitStabilised = false; // force update of osculating elements

	// Now update all passively attached child vessels
	UpdateAttachments();
	UpdateProxies();
	UpdateSurfParams();
}

// ==============================================================

void Vessel::SetGlobalOrientation (const Vector &arot)
{
	if (supervessel) {
		supervessel->SetGlobalOrientation (arot, this);
	} else {
		double sinx = sin(arot.x), cosx = cos(arot.x);
		double siny = sin(arot.y), cosy = cos(arot.y);
		double sinz = sin(arot.z), cosz = cos(arot.z);
		s0->R.Set (cosy*cosz,                cosy*sinz,               -siny,
		           sinx*siny*cosz-cosx*sinz, sinx*siny*sinz+cosx*cosz, sinx*cosy,
				   cosx*siny*cosz+sinx*sinz, cosx*siny*sinz-sinx*cosz, cosx*cosy);
		s0->Q.Set (s0->R);
		UpdateAttachments();
	}
}

// ==============================================================

void Vessel::SetRotationMatrix (const Matrix &R)
{
	if (supervessel) {
		supervessel->SetRotationMatrix (R, this);
	} else {
		s0->R.Set (R);
		s0->Q.Set (R);
		UpdateAttachments();
	}
}

// ==============================================================

void Vessel::GetIntermediateMoments (Vector &acc, Vector &tau, const StateVectors &state, double tfrac, double dt)
{
	// TODO: Move this up to VesselBase
	Vector F(Flin_add); // linear forces excluding gravitational and ground contact forces
	Vector M(Amom_add); // angular momentum excluding gravity gradient torque and ground contact torques
	collision_during_update |=
		AddSurfaceForces (&F, &M, &state, tfrac, dt, update_with_collision); // add ground contact forces and moments
	// note: we may want to remove aerodynamic forces from Flin_add/Amom_add and calculate
	// intermediate states here instead
	RigidBody::GetIntermediateMoments (acc, tau, state, tfrac, dt);  // get gravitational component
	acc += mul (state.Q, F/mass);
	tau += M/mass;
}

// ==============================================================

void Vessel::GetIntermediateMoments_pert (Vector &acc, Vector &tau,
	const StateVectors &state_rel, double tfrac, double dt, const CelestialBody *cbody)
{
	StateVectors state(state_rel);
	state.pos += cbody->InterpolatePosition (tfrac);

	Vector F(Flin_add); // linear forces excluding gravitational and ground contact forces
	Vector M(Amom_add); // angular momentum excluding gravity gradient torque and ground contact torques
	collision_during_update |=
		AddSurfaceForces (&F, &M, &state, tfrac, dt, update_with_collision); // add ground contact forces and moments
	// note: we may want to remove aerodynamic forces from Flin_add/Amom_add and calculate
	// intermediate states here instead
	RigidBody::GetIntermediateMoments_pert (acc, tau, state_rel, tfrac, dt, cbody);  // get gravitational component
	acc += mul (state.Q, F/mass);
	tau += M/mass;
}

// ==============================================================

Vector Vessel::GetTorque () const
{
	static Vector F(0,0,0);
	Vector M(Amom);
	AddSurfaceForces (&F, &M, s0, 0, 0);
	return RigidBody::GetTorque() + M/mass;
}

// ==============================================================

Vector Vessel::GetMomentumFlux () const
{
	// map momentum flux into vessel frame
	return tmul (s0->R, g_psys->GetMomentumFlux (s0->pos));
}

// ==============================================================

void Vessel::SetAngVel (const Vector &omega)
{
	if (supervessel)
		supervessel->SetAngVel (omega, this);
	else
		SetAngVel_individual (omega);
}

// ==============================================================

void Vessel::SetAngVel_individual (const Vector &omega)
{
	RigidBody::SetAngVel (omega);
}

// ==============================================================

void Vessel::FocusChanged (bool getfocus, Vessel *newvessel, Vessel *oldvessel)
{
	if (modIntf.v->Version() >= 1)
		((VESSEL2*)modIntf.v)->clbkFocusChanged (getfocus, (OBJHANDLE)newvessel, (OBJHANDLE)oldvessel);
}

// ==============================================================

void Vessel::InitSupervessel (bool isprimary)
{
	DWORD i;

	// Initialise vessel-vessel docking
	for (i = 0; i < ndock; i++)
		if (dock[i]->mate && (!supervessel || !supervessel->isComponent (dock[i]->mate))) {
			if (isprimary) g_psys->DockVessels (this, dock[i]->mate, i, dock[i]->matedock, false);
			else           g_psys->DockVessels (dock[i]->mate, this, dock[i]->matedock, i, false);
			RegisterDocking (i, dock[i]->mate, dock[i]->matedock);
			dock[i]->mate->RegisterDocking (dock[i]->matedock, this, i);
		}

	// Initialise child-parent attachment
	if (attach_status.pname) {
		Vessel *prnt = g_psys->GetVessel (attach_status.pname, true);
		if (prnt)
			prnt->AttachChild (this, prnt->GetAttachmentFromIndex (false, attach_status.pi), GetAttachmentFromIndex (true, attach_status.ci), false);
		delete []attach_status.pname;
		attach_status.pname = 0;
	}
}

// ==============================================================

void Vessel::InitNavRadios (DWORD n)
{
	if (n == nnav) return; // nothing to do

	NavRadioSpec *tmp;
	DWORD i;

	if (n) {
		tmp = new NavRadioSpec[n]; TRACENEW
		if (nnav) memcpy (tmp, nav, min (n, nnav) * sizeof(NavRadioSpec));
		for (i = nnav; i < n; i++) {
			tmp[i].freq   = NAV_RADIO_FREQ_MIN;
			tmp[i].step   = 0;
			tmp[i].dbidx  = -1; // undefined
			tmp[i].sender = NULL;
		}
	} else
		tmp = 0;
	if (nnav) delete []nav;
	nav = tmp;
	nnav = n;
}

// ==============================================================

bool Vessel::SetNavChannel (DWORD n, DWORD ch)
{
	if (n < nnav && ch < NAV_RADIO_NSTEP) {
		nav[n].step  = ch;
		nav[n].dbidx = -1;
		nav[n].freq  = (float)(ch*0.05 + NAV_RADIO_FREQ_MIN);
		UpdateReceiverStatus (n);
		return true;
	} else return false;
}

// ==============================================================

bool Vessel::IncNavChannel (DWORD n, int dch)
{
	if (n < nnav) {
		nav[n].step = IncRadioChannel (nav[n].step, dch);
		nav[n].dbidx = -1;
		nav[n].freq  = (float)(nav[n].step*0.05 + NAV_RADIO_FREQ_MIN);
		UpdateReceiverStatus (n);
		return true;
	} else return false;
}

// ==============================================================

bool Vessel::SetIDSChannel (PortSpec *ps, DWORD ch)
{
	if (ps->ids && ch < NAV_RADIO_NSTEP) {
		ps->ids->SetStep (ch);
		return true;
	} else return false;
}

// ==============================================================

bool Vessel::SetXpdrChannel (DWORD ch)
{
	if (xpdr && ch < NAV_RADIO_NSTEP) {
		xpdr->SetStep (ch);
		return true;
	} else return false;
}

// ==============================================================

bool Vessel::IncXpdrChannel (int dch)
{
	if (xpdr) {
		xpdr->SetStep (IncRadioChannel (xpdr->GetStep(), dch));
		return true;
	} else return false;
}

// ==============================================================

DWORD Vessel::IncRadioChannel (DWORD ch, int step) const
{
	int nch = (int)ch + step;
	while (nch < 0) nch += NAV_RADIO_NSTEP;
	return (DWORD)(nch % NAV_RADIO_NSTEP);
}

// ==============================================================

DWORD Vessel::GetNavChannel (DWORD n) const
{ return (n < nnav ? nav[n].step : 0); }

// ==============================================================

float Vessel::GetNavFreq (DWORD n) const
{ return (n < nnav ? nav[n].freq : 0.0f); }

// ==============================================================

DWORD Vessel::GetXpdrChannel () const
{
	return (xpdr ? xpdr->GetStep() : 0);
}

// ==============================================================

bool Vessel::GetXpdrFreq (float &freq) const
{
	if (xpdr) {
		freq = xpdr->GetFreq();
		return true;
	} else return false;
}

// ==============================================================

bool Vessel::isOrbitStabilised () const
{
	return supervessel ? supervessel->isOrbitStabilised() : bOrbitStabilised;
}

// ==============================================================

bool Vessel::SetTouchdownPoints (const TOUCHDOWNVTX *tdvtx, DWORD ntp)
{
	dCHECK(ntp >= 3, "Vessel::SetTouchdownPoints: at least 3 points must be provided");

	static const double eps = 1e-12;
	DWORD i;
	double a, b, c, d, e, f;

	if (ntp != ntouchdown_vtx) {
		if (ntouchdown_vtx) delete []touchdown_vtx;
		touchdown_vtx = new TOUCHDOWN_VTX[ntouchdown_vtx = ntp];
	}
	for (i = 0; i < ntp; i++) {
		touchdown_vtx[i].pos.Set (MakeVector(tdvtx[i].pos));
		touchdown_vtx[i].stiffness = tdvtx[i].stiffness;
		touchdown_vtx[i].damping   = tdvtx[i].damping;
		touchdown_vtx[i].mu        = tdvtx[i].mu;
		touchdown_vtx[i].mu_lng    = tdvtx[i].mu_lng;
	}

	// The rest of this function refers to the first 3 (primary) touchdown points
	// upward normal of touchdown plane
	Vector tp[3];
	for (i = 0; i < 3; i++)
		tp[i].Set (touchdown_vtx[i].pos);

	touchdown_nm = crossp (tp[0] - (tp[1] + tp[2])*0.5, tp[2] - tp[1]);
	double len = touchdown_nm.length();
	if (len > eps) touchdown_nm /= len; // normalise
	else           return false;

	// equation of plane: E: ax+by+cz+d = 0
	a = tp[0].y*(tp[1].z-tp[2].z) - tp[1].y*(tp[0].z-tp[2].z) + tp[2].y*(tp[0].z-tp[1].z);
	b = tp[0].x*(tp[1].z-tp[2].z) - tp[1].x*(tp[0].z-tp[2].z) + tp[2].x*(tp[0].z-tp[1].z);
	c = tp[0].x*(tp[1].y-tp[2].y) - tp[1].x*(tp[0].y-tp[2].y) + tp[2].x*(tp[0].y-tp[1].y);
	c = -c; // account for left-handed coord system
	d = -tp[0].x*a - tp[0].y*b - tp[0].z*c;
	double scl = sqrt (a*a + b*b + c*c);

	// elevation of CoM over ground when landed
	cog_elev = fabs(d/scl);
	touchdown_cg.Set (-a*cog_elev/scl, -b*cog_elev/scl, -c*cog_elev/scl);

	// precompute equilibrium suspension compression parameters
	// 1. Compute rotation matrix that rotates touchdown points into y=const plane
	Vector ex((touchdown_vtx[2].pos-touchdown_vtx[1].pos).unit());
	Vector ez(crossp(ex,touchdown_nm));
	Matrix R(ex.x,ex.y,ex.z,  touchdown_nm.x,touchdown_nm.y, touchdown_nm.z, ez.x,ez.y,ez.z);
	// 2. Compute the compression factors (actual compression length is given by factor*mg)
	// See "Dynamic vessel-surface interaction" in "Orbiter Technical Reference" for details
	Vector tr[3];
	a = b = c = d = e = f = 0.0;
	for (i = 0; i < 3; i++) {
		tr[i] = mul (R, touchdown_vtx[i].pos);
		a += touchdown_vtx[i].stiffness;
		b += touchdown_vtx[i].stiffness * tr[i].x;
		c += touchdown_vtx[i].stiffness * tr[i].z;
		d += touchdown_vtx[i].stiffness * tr[i].x*tr[i].x;
		e += touchdown_vtx[i].stiffness * tr[i].x*tr[i].z;
		f += touchdown_vtx[i].stiffness * tr[i].z*tr[i].z;
	}
	double den = c*c*d - 2.0*b*c*e + b*b*f + a*(e*e-d*f);
	double dycg = (e*e-d*f)/den;
	double sinth = (b*f-c*e)/den;
	double sinph = (c*d-b*e)/den;
	for (i = 0; i < 3; i++)
		touchdown_vtx[i].compression = dycg + sinth*tr[i].x + sinph*tr[i].z;

	bForceActive = true; // notification flag
	return true;
}

// ==============================================================

void Vessel::ClearTouchdownPoints ()
{
	if (ntouchdown_vtx) {
		delete []touchdown_vtx;
		ntouchdown_vtx = 0;
	}
}

// ==============================================================

void Vessel::SetSurfaceFrictionCoeff (double mlng, double mlat)
{
	mu     = mlat;
	mu_lng = mlng;
	for (int i = 0; i < ntouchdown_vtx; i++) {
		touchdown_vtx[i].mu     = mlat;
		touchdown_vtx[i].mu_lng = mlng;
	}
}

// ==============================================================

bool Vessel::GetWeightVector (Vector &G) const
{
	if (!weight_valid) {
		Weight.Set (tmul (s0->R, g_psys->Gacc (s0->pos, this, &gfielddata)) * mass);
		weight_valid = true;
	}
	G.Set (Weight);
	return true;
}

// ==============================================================

bool Vessel::GetThrustVector (Vector &T) const
{
	if (m_bThrustEngaged) T.Set (Thrust);
	else                  T.Set (0,0,0);
	return m_bThrustEngaged;
}

// ==============================================================

bool Vessel::GetLiftVector (Vector &L) const
{
	if (!Lift) {
		L.Set (0, 0, 0);
		return false;
	} else {
		double v0 = std::hypot (sp.airvel_ship.y, sp.airvel_ship.z);
		double scale = (v0 ? Lift/v0 : 0.0);
		L.Set (0, sp.airvel_ship.z*scale, -sp.airvel_ship.y*scale);
		return true;
	}
}

// ==============================================================

bool Vessel::GetDragVector (Vector &D) const
{
	if (!Drag) {
		D.Set (0, 0, 0);
		return false;
	} else {
		double v0 = -sp.airvel_ship.length();
		D.Set (sp.airvel_ship * (v0 ? Drag/v0 : 0.0));
		return true;
	}
}

// ==============================================================

bool Vessel::GetSideForceVector (Vector &SF) const
{
	if (!SideForce) {
		SF.Set (0, 0, 0);
		return false;
	} else {
		double v0 = std::hypot(sp.airvel_ship.x, sp.airvel_ship.z);
		double scale = (v0 ? SideForce / v0 : 0.0);
		SF.Set(sp.airvel_ship.z * scale, 0, - sp.airvel_ship.x * scale);
		return true;
	}
}

// ==============================================================

bool Vessel::GetForceVector (Vector &F) const
{
	// Obtain total force vector from acceleration vector
	// This also captures ground contact effects, docking, etc.

	F.Set (tmul (s0->R, acc) * mass);
	//F.Set (tmul (s0->R, (s0->vel - prvel) * (mass*td.iSimDT)));
	return true;
}

// ==============================================================

bool Vessel::GetTorqueVector (Vector &M) const
{
	if (!torque_valid) {
		Torque.Set (GetTorque());
		torque_valid = true;
	}
	M.Set (Torque);
	return true;
}

// ==============================================================

ThrustSpec *Vessel::CreateThruster (const Vector &pos, const Vector &dir, double maxth0,
	TankSpec *ts, double isp0, double isp_ref, double p_ref)
{
	ThrustSpec* thruster = new ThrustSpec;
	thruster->ref = MakeVECTOR3(pos);
	thruster->dir = MakeVECTOR3(dir.unit());
	thruster->maxth0 = maxth0;
	thruster->tank = ts;
	thruster->isp0 = (isp0 > 0.0 ? isp0 : m_defaultIsp);
	thruster->pfac = (isp_ref > 0.0 ? (isp0 - isp_ref) / (p_ref * isp0) : 0.0);
	thruster->level = 0.0;
	thruster->level_permanent = 0.0;
	thruster->level_override = 0.0;

	m_thruster.push_back(thruster);

	return thruster;
}

// ==============================================================

bool Vessel::DelThruster (ThrustSpec *ts)
{
	DWORD i, j, k, m;
	ThrustSpec **tmp;
	ThrustGroupSpec *tgs;

	// remove all exhaust specs which link to this thruster
	for (i = nexhaust-1; (int)i >= 0; i--) {
		if (exhaust[i]->level == &ts->level)
			DelExhaust (exhaust[i]->id);
	}
	// same for particle streams
	for (i = ncontrail-1; (int)i >= 0; i--) {
		if (contrail[i]->Level() == &ts->level)
			DelExhaustStream (contrail[i]);
	}

	// remove from default thruster group lists
	for (auto it = m_thrusterGroupDef.begin(); it != m_thrusterGroupDef.end(); it++) {
		ThrustGroupSpec* tgs = &(*it);
		for (auto its = tgs->ts.begin(); its != tgs->ts.end(); its++) {
			if (*its == ts)
				tgs->ts.erase(its);
		}
	}
	// remove from user thruster group lists
	for (auto it = m_thrusterGroupUsr.begin(); it != m_thrusterGroupUsr.end(); it++) {
		ThrustGroupSpec* tgs = *it;
		for (auto its = tgs->ts.begin(); its != tgs->ts.end(); its++) {
			if (*its == ts)
				tgs->ts.erase(its);
		}
	}
	// remove from full thruster list
	auto it = std::find(m_thruster.begin(), m_thruster.end(), ts);
	if (it != m_thruster.end()) {
		delete *it;
		m_thruster.erase(it);
		return true;
	}
	return false;
}

// ==============================================================

void Vessel::ShiftThrusters (const Vector &shift)
{
	for (auto it = m_thruster.begin(); it != m_thruster.end(); it++)
		(*it)->ref += MakeVECTOR3(shift);
}

// ==============================================================

Vector Vessel::GetThrusterForce (const ThrustSpec *ts) const
{
	if (!ts->tank || !ts->tank->mass) {
		return Vector(0,0,0);
	} else {
		double th = ts->maxth0 * ts->level;
		th *= ThrusterAtmScale (ts, sp.atmp);
		return MakeVector(ts->dir *th);
	}
}

// ==============================================================

void Vessel::GetThrusterMoment (const ThrustSpec *ts, Vector &F, Vector &T) const
{
	if (!ts->tank || !ts->tank->mass) {
		F.Set(0,0,0);
		T.Set(0,0,0);
	} else {
		double th = ts->maxth0 * ts->level;
		th *= ThrusterAtmScale (ts, sp.atmp);
		F = MakeVector(ts->dir * th);
		T = crossp (F, MakeVector(ts->ref));
	}
}

// ==============================================================

void Vessel::ClearThrusterDefinitions ()
{
	// delete all thrusters, thruster groups and exhaust render definitions
	
	DWORD grp;
	ThrustGroupSpec *tgs;

	ClearExhaustDefinitions();
	ClearExhaustStreamDefinitions();

	// delete the default group definitions
	for (auto it = m_thrusterGroupDef.begin(); it != m_thrusterGroupDef.end(); it++) {
		ThrustGroupSpec* tgs = &(*it);
		tgs->ts.clear();
		tgs->maxth_sum = 0.0;
	}
	// delete the user group definitions
	for (auto it = m_thrusterGroupUsr.begin(); it != m_thrusterGroupUsr.end(); it++) {
		ThrustGroupSpec* tgs = *it;
		delete tgs;
	}
	m_thrusterGroupUsr.clear();

	// delete thrusters
	for (auto it = m_thruster.begin(); it != m_thruster.end(); it++)
		delete *it;
	m_thruster.clear();

	ResetMass(); // bring fuel mass up to date
}

// ==============================================================

void Vessel::ClearExhaustDefinitions ()
{
	DWORD i;

	// delete exhaust definitions
	if (nexhaust) {
		for (i = 0; i < nexhaust; i++)
			delete exhaust[i];
		delete []exhaust;
		exhaust = NULL;
		nexhaust = 0;
	}
	// delete old-style exhaust definitions
	if (noexhaust) {
		for (i = 0; i < noexhaust; i++)
			delete oexhaust[i];
		delete []oexhaust;
		oexhaust = NULL;
		noexhaust = 0;
	}
}

// ==============================================================

void Vessel::ClearExhaustStreamDefinitions ()
{
	if (ncontrail) {
		for (DWORD i = 0; i < ncontrail; i++) contrail[i]->Detach();
		delete []contrail;
		contrail = NULL;
		ncontrail = 0;
	}
}

// ==============================================================

void Vessel::SetThrusterMax0 (ThrustSpec *ts, double maxth0)
{
	dCHECK(ts, "Zero ThrustSpec pointer not permitted.")
	dCHECK(maxth0 >= 0.0, "Max thrust must be >= 0")

	if (ts->maxth0 != maxth0) {
		double dth = maxth0 - ts->maxth0;
		ts->maxth0 = maxth0;

		// re-calculate max group thrusts
		for (auto it = m_thrusterGroupDef.begin(); it != m_thrusterGroupDef.end(); it++) {
			ThrustGroupSpec& tgs = *it;
			if (IsGroupThruster(&tgs, ts))
				tgs.maxth_sum += dth;
		}
		for (auto it = m_thrusterGroupUsr.begin(); it != m_thrusterGroupUsr.end(); it++) {
			ThrustGroupSpec* tgs = *it;
			if (IsGroupThruster (tgs, ts))
				tgs->maxth_sum += dth;
		}
	}
}

// ==============================================================

ThrustGroupSpec *Vessel::CreateThrusterGroup (ThrustSpec **ts, DWORD nts, THGROUP_TYPE thgt)
{
	DWORD i;
	ThrustGroupSpec* tgs = GetThrusterGroup(thgt);
	if (!tgs && thgt >= THGROUP_USER) { // new user-defined group requested
		i = thgt - THGROUP_USER;
		if (i >= m_thrusterGroupUsr.size())
			m_thrusterGroupUsr.resize(i + 1);
		tgs = m_thrusterGroupUsr[i] = new ThrustGroupSpec;
	}
	dCHECK(tgs, "Invalid THGROUP_TYPE")

	tgs->ts.resize(nts);
	tgs->maxth_sum = 0.0;
	for (i = 0; i < nts; i++) {
		tgs->ts[i] = ts[i];
		tgs->maxth_sum += ts[i]->maxth0;
	}
	// rotational attitude mode: calculate max. angular momentum
	if (thgt >= THGROUP_ATT_PITCHUP && thgt <= THGROUP_ATT_BANKRIGHT) {
		Vector M;
		for (i = 0; i < nts; i++)
			M += MakeVector(crossp (ts[i]->dir * ts[i]->maxth0, ts[i]->ref));
		max_angular_moment[thgt-THGROUP_ATT_PITCHUP] = M.length();
	}

	return tgs;
}

// ==============================================================

bool Vessel::DeleteThrusterGroup (ThrustGroupSpec *tgs, bool delth)
{
	DWORD i, j, k;

	// sanity check
	if (!tgs) return false;

	// Check if default group type
	for (i = 0; i < m_thrusterGroupDef.size(); i++)
		if (tgs == &m_thrusterGroupDef[i])
			return DeleteThrusterGroup((THGROUP_TYPE)i, delth);

	// Check if user-defined group
	for (i = 0; i < m_thrusterGroupUsr.size(); i++)
		if (tgs == m_thrusterGroupUsr[i])
			return DeleteThrusterGroup((THGROUP_TYPE)(THGROUP_USER + i), delth);

	return false;
}

// ==============================================================

bool Vessel::DeleteThrusterGroup (THGROUP_TYPE thgt, bool delth)
{
	ThrustGroupSpec* tgs = GetThrusterGroup(thgt);
	if (!tgs) return false;

	bool success = (tgs->ts.size() > 0);

	if (delth) { // delete the thrusters
		while (tgs->ts.size()) DelThruster(tgs->ts[0]);
		// this also takes care of removing the thruster from the group
	}
	else // delete the list but not the thrusters
		tgs->ts.clear();

	if (thgt >= THGROUP_USER) {
		delete tgs;
		m_thrusterGroupUsr[thgt - THGROUP_USER] = nullptr;
	}

	// If the group was user-defined, the user-defined list is not shrunk here even if the
	// group was at the end of the list

	return success;
}

// ==============================================================

ThrustGroupSpec* Vessel::GetThrusterGroup(THGROUP_TYPE thgt)
{
	ThrustGroupSpec* tgs = nullptr;
	if (thgt < THGROUP_USER)
		tgs = &m_thrusterGroupDef[thgt];
	else if (thgt - THGROUP_USER < m_thrusterGroupUsr.size())
		tgs = m_thrusterGroupUsr[thgt - THGROUP_USER];
	return tgs;
}

// ==============================================================

const ThrustGroupSpec* Vessel::GetThrusterGroup(THGROUP_TYPE thgt) const
{
	const ThrustGroupSpec* tgs = nullptr;
	if (thgt < THGROUP_USER)
		tgs = &m_thrusterGroupDef[thgt];
	else if (thgt - THGROUP_USER < m_thrusterGroupUsr.size())
		tgs = m_thrusterGroupUsr[thgt - THGROUP_USER];
	return tgs;
}

// ==============================================================

DWORD Vessel::NumThrusters(THGROUP_TYPE thgt) const
{
	const ThrustGroupSpec* tgs = GetThrusterGroup(thgt);
	return (tgs ? tgs->ts.size() : 0);
}

// ==============================================================

bool Vessel::IsGroupThruster(ThrustGroupSpec* tgs, ThrustSpec* ts) const
{
	dCHECK(tgs, "Zero ThrustGroupSpec pointer not permitted.")

	for (auto it = tgs->ts.begin(); it != tgs->ts.end(); it++)
		if (ts == *it) return true;
	return false;
}

// ==============================================================

void Vessel::SetThrusterGroupLevel(THGROUP_TYPE thgt, double level)
{
	ThrustGroupSpec* tgs = GetThrusterGroup(thgt);
	if (tgs)
		SetThrusterGroupLevel(tgs, level);
}

// ==============================================================

void Vessel::SetThrusterGroupOverride(THGROUP_TYPE thgt, double level)
{
	ThrustGroupSpec* tgs = GetThrusterGroup(thgt);
	if (tgs)
		SetThrusterGroupOverride(tgs, level);
}

// ==============================================================

void Vessel::IncThrusterGroupLevel(ThrustGroupSpec *tgs, double dlevel)
{
	if (bFRplayback) return;

	dCHECK(tgs, "Zero ThrustGroupSpec pointer not permitted.")

	for (auto it = tgs->ts.begin(); it != tgs->ts.end(); it++)
		IncThrusterLevel (*it, dlevel);
}

// ==============================================================

void Vessel::IncThrusterGroupLevel(THGROUP_TYPE thgt, double dlevel)
{
	ThrustGroupSpec* tgs = GetThrusterGroup(thgt);
	if (tgs)
		IncThrusterGroupLevel(tgs, dlevel);
}

// ==============================================================

void Vessel::IncThrusterGroupOverride(THGROUP_TYPE thgt, double dlevel)
{
	ThrustGroupSpec* tgs = GetThrusterGroup(thgt);
	if (tgs)
		IncThrusterGroupOverride(tgs, dlevel);
}

// ==============================================================

double Vessel::GetThrusterGroupLevel(const ThrustGroupSpec *tgs) const
{
	dCHECK(tgs, "Zero ThrustGroupSpec pointer not permitted.")

	double level = 0.0;
	for (auto it = tgs->ts.begin(); it != tgs->ts.end(); it++)
		level += (*it)->level;
	int nt = tgs->ts.size();
	return (nt ? level / nt : 0.0);
}

// ==============================================================

double Vessel::GetThrusterGroupLevel(THGROUP_TYPE thgt) const
{
	const ThrustGroupSpec* tgs = GetThrusterGroup(thgt);
	return (tgs ? GetThrusterGroupLevel(tgs) : 0.0);
}

// ==============================================================

double Vessel::GetThrusterGroupMaxth(THGROUP_TYPE thgt) const
{
	const ThrustGroupSpec* tgs = GetThrusterGroup(thgt);
	return (tgs ? GetThrusterGroupMaxth(tgs) : 0.0);
}

// ==============================================================

Vector Vessel::GetThrusterGroupForce (THGROUP_TYPE thgt) const
{
	const ThrustGroupSpec* tgs = GetThrusterGroup(thgt);
	return (tgs ? GetThrusterGroupForce(tgs) : Vector(0, 0, 0));
}

// ==============================================================

Vector Vessel::GetThrusterGroupForce (const ThrustGroupSpec *tgs) const
{
	dCHECK(tgs, "Zero ThrustGroupSpec pointer not permitted.")

	Vector F;
	for (auto it = tgs->ts.begin(); it != tgs->ts.end(); it++)
		F += GetThrusterForce (*it);
	return F;
}

// ==============================================================

AirfoilSpec *Vessel::CreateAirfoil (AIRFOIL_ORIENTATION align, const Vector &ref, AirfoilCoeffFunc cf, double c, double S, double A)
{
	AirfoilSpec *af, **tmp = new AirfoilSpec*[nairfoil+1]; TRACENEW
	if (nairfoil) {
		memcpy (tmp, airfoil, nairfoil*sizeof(AirfoilSpec*));
		delete []airfoil;
	}
	airfoil = tmp;

	af = airfoil[nairfoil++] = new AirfoilSpec; TRACENEW
	af->version = 0;
	af->align   = align;
	af->ref.Set (ref);
	af->cf      = cf;
	af->context = 0;
	af->c       = c;
	af->S       = S;
	af->A       = A;
	return af;
}

// ==============================================================

AirfoilSpec *Vessel::CreateAirfoil (AIRFOIL_ORIENTATION align, const Vector &ref, AirfoilCoeffFuncEx cf, void *context, double c, double S, double A)
{
	AirfoilSpec *af, **tmp = new AirfoilSpec*[nairfoil+1]; TRACENEW
	if (nairfoil) {
		memcpy (tmp, airfoil, nairfoil*sizeof(AirfoilSpec*));
		delete []airfoil;
	}
	airfoil = tmp;

	af = airfoil[nairfoil++] = new AirfoilSpec; TRACENEW
	af->version = 1;
	af->align   = align;
	af->ref.Set (ref);
	af->cf      = (AirfoilCoeffFunc)cf;
	af->context = context;
	af->c       = c;
	af->S       = S;
	af->A       = A;
	return af;
}

// ==============================================================

AirfoilSpec* Vessel::CreateAirfoil(AIRFOIL_ORIENTATION align, const Vector& ref, AirfoilCoeffFuncEx2 cf, void* context, double c, double S, double A)
{
	AirfoilSpec* af, ** tmp = new AirfoilSpec * [nairfoil + 1]; TRACENEW
		if (nairfoil) {
			memcpy(tmp, airfoil, nairfoil * sizeof(AirfoilSpec*));
			delete[]airfoil;
		}
	airfoil = tmp;

	af = airfoil[nairfoil++] = new AirfoilSpec; TRACENEW
		af->version = 2;
	af->align = align;
	af->ref.Set(ref);
	af->cf = (AirfoilCoeffFunc)cf;
	af->context = context;
	af->c = c;
	af->S = S;
	af->A = A;
	return af;
}

// ==============================================================

bool Vessel::GetAirfoilParam (AirfoilSpec *af, VECTOR3 *ref, AirfoilCoeffFunc *cf, void **context, double *c, double *S, double *A)
{
	for (DWORD i = 0; i < nairfoil; i++) {
		if (af == airfoil[i]) {
			if (ref)     *ref     = MakeVECTOR3(af->ref);
			if (cf)      *cf      = af->cf;
			if (context) *context = af->context;
			if (c)       *c       = af->c;
			if (S)       *S       = af->S;
			if (A)       *A       = af->A;
			return true;
		}
	}
	return false;
}

// ==============================================================

void Vessel::EditAirfoil (AirfoilSpec *af, DWORD flag, const Vector &ref, AirfoilCoeffFunc cf, double c, double S, double A)
{
	if (flag & 0x01) af->ref.Set (ref);
	if (flag & 0x02) af->cf = cf;
	if (flag & 0x04) af->c  = c;
	if (flag & 0x08) af->S  = S;
	if (flag & 0x10) af->A  = A;
}

// ==============================================================

bool Vessel::DelAirfoil (AirfoilSpec *af)
{
	for (DWORD i = 0; i < nairfoil; i++)
		if (af == airfoil[i])
			return DelAirfoil (i);
	return false;
}

// ==============================================================

bool Vessel::DelAirfoil (DWORD i)
{
	if (i >= nairfoil) return false;
	delete airfoil[i];
	AirfoilSpec **tmp;
	if (nairfoil > 1) {
		DWORD j, k;
		tmp = new AirfoilSpec*[nairfoil-1]; TRACENEW
		for (j = k = 0; j < nairfoil; j++)
			if (j != i) tmp[k++] = airfoil[j];
	} else tmp = 0;
	delete []airfoil;
	airfoil = tmp;
	nairfoil--;
	return true;
}

// ==============================================================

void Vessel::ClearAirfoilDefinitions ()
{
	if (nairfoil) {
		for (DWORD i = 0; i < nairfoil; i++) delete airfoil[i];
		delete []airfoil;
		airfoil = NULL;
		nairfoil = 0;
	}
}

// ==============================================================

CtrlsurfSpec *Vessel::CreateControlSurface (AIRCTRL_TYPE ctrl, double area, double dCl, const Vector &ref, int axis, double delay, UINT anim)
{
	CtrlsurfSpec *css = new CtrlsurfSpec; TRACENEW
	css->ctrl = ctrl;
	css->ref.Set (ref);
	css->area = area;
	css->dCl = dCl;
	css->anim = anim;
	if (axis == AIRCTRL_AXIS_AUTO) {
		switch (ctrl) {
		case AIRCTRL_ELEVATOR:
		case AIRCTRL_ELEVATORTRIM:
			axis = AIRCTRL_AXIS_XPOS;
			break;
		case AIRCTRL_RUDDER:
		case AIRCTRL_RUDDERTRIM:
			axis = AIRCTRL_AXIS_YPOS;
			break;
		case AIRCTRL_AILERON:
			axis = (ref.x > 0 ? AIRCTRL_AXIS_XPOS : AIRCTRL_AXIS_XNEG);
			break;
		case AIRCTRL_FLAP:
			axis = AIRCTRL_AXIS_XPOS;
			break;
		}
	}
	css->axis = axis;
	if (ctrl == AIRCTRL_ELEVATORTRIM) bElevTrim = true;
	ctrlsurf_level[ctrl].delay = delay;

	CtrlsurfSpec **tmp = new CtrlsurfSpec*[nctrlsurf+1]; TRACENEW
	if (nctrlsurf) {
		memcpy (tmp, ctrlsurf, nctrlsurf*sizeof(CtrlsurfSpec*));
		delete []ctrlsurf;
	}
	ctrlsurf = tmp;
	return (ctrlsurf[nctrlsurf++] = css);
}

// ==============================================================

bool Vessel::DelControlSurface (CtrlsurfSpec *cs)
{
	for (DWORD i = 0; i < nctrlsurf; i++)
		if (cs == ctrlsurf[i])
			return DelControlSurface (i);
	return false;
}

// ==============================================================

bool Vessel::DelControlSurface (DWORD i)
{
	if (i >= nctrlsurf) return false;
	delete ctrlsurf[i];
	CtrlsurfSpec **tmp;
	if (nctrlsurf > 1) {
		DWORD j, k;
		tmp = new CtrlsurfSpec*[nctrlsurf-1]; TRACENEW
		for (j = k = 0; j < nctrlsurf; j++)
			if (j != i) tmp[k++] = ctrlsurf[j];
	} else tmp = 0;
	delete []ctrlsurf;
	ctrlsurf = tmp;
	nctrlsurf--;
	return true;
}

// ==============================================================

void Vessel::ClearControlSurfaceDefinitions ()
{
	if (nctrlsurf) {
		for (DWORD i = 0; i < nctrlsurf; i++) delete ctrlsurf[i];
		delete []ctrlsurf;
		ctrlsurf = NULL;
		nctrlsurf = 0;
	}
}

// ==============================================================

void Vessel::SetControlSurfaceLevel (AIRCTRL_TYPE ctrl, double level, bool transient, bool direct)
{
	if (transient) ctrlsurf_level[ctrl].ttgt = level;
	else           ctrlsurf_level[ctrl].ptgt = level;
	CtrlSurfSyncMode = (direct ? 2:1);
}

// ==============================================================

void Vessel::ApplyControlSurfaceLevels ()
{
	bool bDirect = (CtrlSurfSyncMode == 2);
	bool bChange = false;
	CtrlSurfSyncMode = 0;
	static bool bChangeCtrl[6];
	int i;
	double tgtlvl;

	for (i = 0; i < 6; i++) {
		tgtlvl = max (-1.0, min (1.0, ctrlsurf_level[i].ptgt + ctrlsurf_level[i].ttgt));
		if (tgtlvl != ctrlsurf_level[i].curr) {
			double level = tgtlvl;
			if (!bDirect && ctrlsurf_level[i].delay && td.SimT1) {
				double dlevel = tgtlvl-ctrlsurf_level[i].curr;
				double dmax = td.SimDT/ctrlsurf_level[i].delay;
				if      (dlevel >  dmax) level = ctrlsurf_level[i].curr+dmax, CtrlSurfSyncMode = 1;
				else if (dlevel < -dmax) level = ctrlsurf_level[i].curr-dmax, CtrlSurfSyncMode = 1;
			}
			ctrlsurf_level[i].curr = level;

			if (!bChange)
				for (int j = 0; j < 6; j++) bChangeCtrl[j] = false;
			bChange = bChangeCtrl[i] = true;
		}
	}
	if (bChange) { // apply animations
		for (i = 0; i < nctrlsurf; i++) {
			AIRCTRL_TYPE ctrl = ctrlsurf[i]->ctrl;
			if (ctrlsurf[i]->anim != (UINT)-1 && bChangeCtrl[ctrl])
				SetAnimation (ctrlsurf[i]->anim, (ctrlsurf_level[ctrl].curr+1.0)*0.5);
		}
	}
}

// ==============================================================

void Vessel::CreateVariableDragElement (const double *drag, double factor, const Vector &ref)
{
	DragElementSpec *des = new DragElementSpec; TRACENEW
	des->ref.Set (ref);
	des->drag = drag;
	des->factor = factor;

	DragElementSpec **tmp = new DragElementSpec*[ndragel+1]; TRACENEW
	if (ndragel) {
		memcpy (tmp, dragel, ndragel*sizeof(DragElementSpec*));
		delete []dragel;
	}
	dragel = tmp;
	dragel[ndragel++] = des;
}

// ==============================================================

void Vessel::ClearVariableDragElements ()
{
	if (ndragel) {
		for (DWORD i = 0; i < ndragel; i++) delete dragel[i];
		delete []dragel;
		dragel = NULL;
		ndragel = 0;
	}
}

// ==============================================================

void Vessel::ApplyUserAttitudeControls (DWORD *ctrl)
{
	if (bFRplayback) return;
	// ignore user controls during playback 

	if (attmode) { // RCS thrusters
		if (attmode == 1) { // rotational mode
			IncThrusterGroupOverride (THGROUP_ATT_PITCHUP,   0.001*ctrl[THGROUP_ATT_PITCHUP]);
			IncThrusterGroupOverride (THGROUP_ATT_PITCHDOWN, 0.001*ctrl[THGROUP_ATT_PITCHDOWN]);
			IncThrusterGroupOverride (THGROUP_ATT_YAWLEFT,   0.001*ctrl[THGROUP_ATT_YAWLEFT]);
			IncThrusterGroupOverride (THGROUP_ATT_YAWRIGHT,  0.001*ctrl[THGROUP_ATT_YAWRIGHT]);
			IncThrusterGroupOverride (THGROUP_ATT_BANKLEFT,  0.001*ctrl[THGROUP_ATT_BANKLEFT]);
			IncThrusterGroupOverride (THGROUP_ATT_BANKRIGHT, 0.001*ctrl[THGROUP_ATT_BANKRIGHT]);
		} else {            // translational mode
			IncThrusterGroupOverride (THGROUP_ATT_RIGHT,     0.001*ctrl[THGROUP_ATT_RIGHT]);
			IncThrusterGroupOverride (THGROUP_ATT_LEFT,      0.001*ctrl[THGROUP_ATT_LEFT]);
			IncThrusterGroupOverride (THGROUP_ATT_UP,        0.001*ctrl[THGROUP_ATT_UP]);
			IncThrusterGroupOverride (THGROUP_ATT_DOWN,      0.001*ctrl[THGROUP_ATT_DOWN]);
			IncThrusterGroupOverride (THGROUP_ATT_FORWARD,   0.001*ctrl[THGROUP_ATT_FORWARD]);
			IncThrusterGroupOverride (THGROUP_ATT_BACK,      0.001*ctrl[THGROUP_ATT_BACK]);
		}
	}

	if (ctrlsurfmode) { // airfoil control surfaces
		if (ctrlsurfmode & 1) SetControlSurfaceLevel (AIRCTRL_ELEVATOR, 0.001*ctrl[THGROUP_ATT_PITCHUP]-0.001*ctrl[THGROUP_ATT_PITCHDOWN],true);
		if (ctrlsurfmode & 2) SetControlSurfaceLevel (AIRCTRL_RUDDER,   0.001*ctrl[THGROUP_ATT_YAWRIGHT]-0.001*ctrl[THGROUP_ATT_YAWLEFT],true);
		if (ctrlsurfmode & 4) SetControlSurfaceLevel (AIRCTRL_AILERON,  0.001*ctrl[THGROUP_ATT_BANKRIGHT]-0.001*ctrl[THGROUP_ATT_BANKLEFT],true);
	}
}

// ==============================================================

void Vessel::IncMainRetroLevel (double dlevel)
{
	double th_main = GetThrusterGroupLevel (THGROUP_MAIN);
	double th_retro = GetThrusterGroupLevel (THGROUP_RETRO);

	if (dlevel > 0) {
		if (th_retro) {
			SetThrusterGroupLevel (THGROUP_RETRO, max (th_retro-dlevel, 0.0));
			SetThrusterGroupLevel (THGROUP_MAIN, 0.0);
		} else {
			SetThrusterGroupLevel (THGROUP_MAIN, min (th_main+dlevel, 1.0));
		}
	} else {
		if (th_main) {
			SetThrusterGroupLevel (THGROUP_MAIN, max (th_main+dlevel, 0.0));
			SetThrusterGroupLevel (THGROUP_RETRO, 0.0);
		} else {
			SetThrusterGroupLevel (THGROUP_RETRO, min (th_retro-dlevel, 1.0));
		}
	}
}

// ==============================================================

bool Vessel::SetMaxThrust_old (ENGINETYPE eng, double maxth)
{
	ThrustSpec *ts;
	TankSpec *tnk = (ntank ? tank[0] : 0);

	switch (eng) {
	case ENGINE_MAIN:
		if (!m_thrusterGroupDef[THGROUP_MAIN].ts.size()) {
			ts = CreateThruster (Vector(0,0,0), Vector(0,0,1), maxth, tnk, m_defaultIsp);
			CreateThrusterGroup (&ts, 1, THGROUP_MAIN);
			return true;
		} else if (m_thrusterGroupDef[THGROUP_MAIN].ts.size() == 1) {
			m_thrusterGroupDef[THGROUP_MAIN].ts[0]->maxth0 = 
			m_thrusterGroupDef[THGROUP_MAIN].maxth_sum = maxth;
			return true;
		} else return false;
	case ENGINE_RETRO:
		if (!m_thrusterGroupDef[THGROUP_RETRO].ts.size()) {
			ts = CreateThruster (Vector(0,0,0), Vector(0,0,-1), maxth, tnk, m_defaultIsp);
			CreateThrusterGroup (&ts, 1, THGROUP_RETRO);
			return true;
		} else if (m_thrusterGroupDef[THGROUP_RETRO].ts.size() == 1) {
			m_thrusterGroupDef[THGROUP_RETRO].ts[0]->maxth0 = 
			m_thrusterGroupDef[THGROUP_RETRO].maxth_sum = maxth;
			return true;
		} else return false;
	case ENGINE_HOVER:
		if (!m_thrusterGroupDef[THGROUP_HOVER].ts.size()) {
			ts = CreateThruster (Vector(0,0,0), Vector(0,1,0), maxth, tnk, m_defaultIsp);
			CreateThrusterGroup (&ts, 1, THGROUP_HOVER);
			return true;
		} else if (m_thrusterGroupDef[THGROUP_HOVER].ts.size() == 1) {
			m_thrusterGroupDef[THGROUP_HOVER].ts[0]->maxth0 = 
			m_thrusterGroupDef[THGROUP_HOVER].maxth_sum = maxth;
			return true;
		} else return false;
	}
	return false;
}

// ==============================================================

void Vessel::CreateDefaultAttitudeSet (double maxth)
{
	ThrustSpec *ts_lin, *ts_rot[2];
	TankSpec *tnk = (ntank ? tank[0] : 0);

	// linear groups
	ts_lin = CreateThruster (Vector (0,0,0), Vector (0, 1,0), maxth, tnk, m_defaultIsp);
	CreateThrusterGroup (&ts_lin, 1, THGROUP_ATT_UP);
	ts_lin = CreateThruster (Vector (0,0,0), Vector (0,-1,0), maxth, tnk, m_defaultIsp);
	CreateThrusterGroup (&ts_lin, 1, THGROUP_ATT_DOWN);
	ts_lin = CreateThruster (Vector (0,0,0), Vector (-1,0,0), maxth, tnk, m_defaultIsp);
	CreateThrusterGroup (&ts_lin, 1, THGROUP_ATT_LEFT);
	ts_lin = CreateThruster (Vector (0,0,0), Vector ( 1,0,0), maxth, tnk, m_defaultIsp);
	CreateThrusterGroup (&ts_lin, 1, THGROUP_ATT_RIGHT);
	ts_lin = CreateThruster (Vector (0,0,0), Vector (0,0, 1), maxth, tnk, m_defaultIsp);
	CreateThrusterGroup (&ts_lin, 1, THGROUP_ATT_FORWARD);
	ts_lin = CreateThruster (Vector (0,0,0), Vector (0,0,-1), maxth, tnk, m_defaultIsp);
	CreateThrusterGroup (&ts_lin, 1, THGROUP_ATT_BACK);

	// rotational groups
	maxth *= 0.5; // since each group has 2 thrusters
	ts_rot[0] = CreateThruster (Vector(0,0, size), Vector(0, 1,0), maxth, tnk, m_defaultIsp);
	ts_rot[1] = CreateThruster (Vector(0,0,-size), Vector(0,-1,0), maxth, tnk, m_defaultIsp);
	CreateThrusterGroup (ts_rot, 2, THGROUP_ATT_PITCHUP);
	ts_rot[0] = CreateThruster (Vector(0,0, size), Vector(0,-1,0), maxth, tnk, m_defaultIsp);
	ts_rot[1] = CreateThruster (Vector(0,0,-size), Vector(0, 1,0), maxth, tnk, m_defaultIsp);
	CreateThrusterGroup (ts_rot, 2, THGROUP_ATT_PITCHDOWN);
	ts_rot[0] = CreateThruster (Vector(0,0, size), Vector(-1,0,0), maxth, tnk, m_defaultIsp);
	ts_rot[1] = CreateThruster (Vector(0,0,-size), Vector( 1,0,0), maxth, tnk, m_defaultIsp);
	CreateThrusterGroup (ts_rot, 2, THGROUP_ATT_YAWLEFT);
	ts_rot[0] = CreateThruster (Vector(0,0, size), Vector( 1,0,0), maxth, tnk, m_defaultIsp);
	ts_rot[1] = CreateThruster (Vector(0,0,-size), Vector(-1,0,0), maxth, tnk, m_defaultIsp);
	CreateThrusterGroup (ts_rot, 2, THGROUP_ATT_YAWRIGHT);
	ts_rot[0] = CreateThruster (Vector( size,0,0), Vector(0, 1,0), maxth, tnk, m_defaultIsp);
	ts_rot[1] = CreateThruster (Vector(-size,0,0), Vector(0,-1,0), maxth, tnk, m_defaultIsp);
	CreateThrusterGroup (ts_rot, 2, THGROUP_ATT_BANKLEFT);
	ts_rot[0] = CreateThruster (Vector(-size,0,0), Vector(0, 1,0), maxth, tnk, m_defaultIsp);
	ts_rot[1] = CreateThruster (Vector( size,0,0), Vector(0,-1,0), maxth, tnk, m_defaultIsp);
	CreateThrusterGroup (ts_rot, 2, THGROUP_ATT_BANKRIGHT);
}

// ==============================================================

UINT Vessel::AddExhaust (EXHAUSTSPEC *spec)
{
	EXHAUSTSPEC **tmp = new EXHAUSTSPEC*[nexhaust+1];
	EXHAUSTSPEC *es;
	if (nexhaust) {
		memcpy (tmp, exhaust, nexhaust*sizeof(EXHAUSTSPEC*));
		delete []exhaust;
	}
	exhaust = tmp;
	es = new EXHAUSTSPEC;
	memcpy (es, spec, sizeof(EXHAUSTSPEC));

	if (es->th && !es->level) {
		es->level = &((ThrustSpec*)es->th)->level;
		es->flags &= ~EXHAUST_CONSTANTLEVEL;
	} else if (!es->level || (es->flags & EXHAUST_CONSTANTLEVEL)) {
		es->level = new double;
		*es->level = (spec->level ? *spec->level : 1.0);
		es->flags |= EXHAUST_CONSTANTLEVEL;
	}

	if (es->th && !es->lpos) {
		es->lpos = &((ThrustSpec*)es->th)->ref;
		es->flags &= ~EXHAUST_CONSTANTPOS;
	} else if (!es->lpos || (es->flags & EXHAUST_CONSTANTPOS)) {
		es->lpos = new VECTOR3;
		*es->lpos = (spec->lpos ? *spec->lpos : _V(0,0,0));
		es->flags |= EXHAUST_CONSTANTPOS;
	}

	if (es->th && !es->ldir) {
		es->ldir = &((ThrustSpec*)es->th)->dir;
		es->flags &= ~EXHAUST_CONSTANTDIR;
	} else if (!es->ldir || (es->flags & EXHAUST_CONSTANTDIR)) {
		es->ldir = new VECTOR3;
		*es->ldir = (spec->ldir ? *spec->ldir : _V(0,0,1));
		es->flags |= EXHAUST_CONSTANTDIR;
	}

	es->id = (nexhaust ? exhaust[nexhaust-1]->id+1 : 0);

	exhaust[nexhaust++] = es;
	return es->id;
}

// ==============================================================

bool Vessel::DelExhaust (UINT idx)
{
	DWORD i, j, k;
	EXHAUSTSPEC **tmp;

	for (i = 0; i < nexhaust; i++)
		if (idx == exhaust[i]->id) break;
	if (i == nexhaust) return false; // not found
	if (exhaust[i]->level && (exhaust[i]->flags & EXHAUST_CONSTANTLEVEL))
		delete exhaust[i]->level;
	if (exhaust[i]->lpos && (exhaust[i]->flags & EXHAUST_CONSTANTPOS))
		delete exhaust[i]->lpos;
	if (exhaust[i]->ldir && (exhaust[i]->flags & EXHAUST_CONSTANTDIR))
		delete exhaust[i]->ldir;
	delete exhaust[i];

	if (nexhaust > 1) {
		tmp = new EXHAUSTSPEC*[nexhaust-1];
		for (j = k = 0; j < nexhaust; j++)
			if (j != i) tmp[k++] = exhaust[j];
	} else tmp = 0;
	delete []exhaust;
	exhaust = tmp;
	nexhaust--;
	return true;
}

// ==============================================================

oapi::ParticleStream *Vessel::AddParticleStream (PARTICLESTREAMSPEC *pss, const Vector &pos, const Vector &dir, double *lvl)
{
	if (!g_pOrbiter->Cfg()->CfgVisualPrm.bParticleStreams) return 0;
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient ();
	if (!gc) return 0;

	oapi::ParticleStream **tmp = new oapi::ParticleStream*[ncontrail+1]; TRACENEW
	if (ncontrail) {
		memcpy (tmp, contrail, ncontrail*sizeof(oapi::ParticleStream*));
		delete []contrail;
	}
	contrail = tmp;
	contrail[ncontrail] = gc->clbkCreateExhaustStream (pss, (OBJHANDLE)this, lvl, MakeVECTOR3(pos), MakeVECTOR3(-dir));
	return contrail[ncontrail++];	
}

// ==============================================================

oapi::ParticleStream *Vessel::AddExhaustStream (ThrustSpec *ts, PARTICLESTREAMSPEC *pss, const Vector *pos, const Vector *dir)
{
	if (!g_pOrbiter->Cfg()->CfgVisualPrm.bParticleStreams) return 0;
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient ();
	if (!gc) return 0;

	oapi::ParticleStream **tmp = new oapi::ParticleStream*[ncontrail+1]; TRACENEW
	if (ncontrail) {
		memcpy (tmp, contrail, ncontrail*sizeof(oapi::ParticleStream*));
		delete []contrail;
	}
	contrail = tmp;
	contrail[ncontrail] = gc->clbkCreateExhaustStream (pss, (OBJHANDLE)this, &ts->level, &ts->ref, &ts->dir);
	if (pos) // local position reference
		contrail[ncontrail]->SetFixedPos (MakeVECTOR3(*pos));
	if (dir) // local direction reference
		contrail[ncontrail]->SetFixedDir (MakeVECTOR3(*dir));
	return contrail[ncontrail++];
}

// ==============================================================

bool Vessel::DelParticleStream (oapi::ParticleStream *ps)
{
	return DelExhaustStream (ps) || DelReentryStream (ps);
}

// ==============================================================

bool Vessel::DelExhaustStream (oapi::ParticleStream *ep)
{
	if (!ncontrail) return false;

	DWORD i, j, k;
	for (i = 0; i < ncontrail; i++)
		if (contrail[i] == ep) break;
	if (i == ncontrail) return false;

	ep->Detach();
	// we detach the contrail from its source, but leave it
	// to the scene to delete it once all particles have expired

	oapi::ParticleStream **tmp;
	if (ncontrail > 1) {
		tmp = new oapi::ParticleStream*[ncontrail-1]; TRACENEW
		for (j = k = 0; j < ncontrail; j++)
			if (j != i) tmp[k++] = contrail[j];
	} else tmp = 0;
	delete []contrail;
	contrail = tmp;
	ncontrail--;
	return true;
}

// ==============================================================

oapi::ParticleStream *Vessel::AddReentryStream (PARTICLESTREAMSPEC *pss)
{
	if (!g_pOrbiter->Cfg()->CfgVisualPrm.bParticleStreams) return 0;
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient ();
	if (!gc) return 0;

	oapi::ParticleStream **tmp = new oapi::ParticleStream*[nreentrystream+1]; TRACENEW
	if (nreentrystream) {
		memcpy (tmp, reentrystream, nreentrystream*sizeof(oapi::ParticleStream*));
		delete []reentrystream;
	}
	reentrystream = tmp;
	reentrystream[nreentrystream] = gc->clbkCreateReentryStream (pss, (OBJHANDLE)this);
	return reentrystream[nreentrystream++];
}

// ==============================================================

void Vessel::SetDefaultReentryStream ()
{
	PARTICLESTREAMSPEC plasmastream = {
		0, size*1.5, 15, 0, 0.01, 0.5, 3, 3, PARTICLESTREAMSPEC::EMISSIVE,
		PARTICLESTREAMSPEC::LVL_FLAT, 1, 1,
		//PARTICLESTREAMSPEC::ATM_PLIN, reentry.plimit, 2*reentry.plimit
		PARTICLESTREAMSPEC::ATM_PLOG, 2e8, 1e11
	};
	AddReentryStream (&plasmastream);
}

// ==============================================================

bool Vessel::DelReentryStream (oapi::ParticleStream *ep)
{
	if (!nreentrystream) return false;

	DWORD i, j, k;

	for (i = 0; i < nreentrystream; i++)
		if (reentrystream[i] == ep) break;
	if (i == nreentrystream) return false;

	ep->Detach();
	// we detach the contrail from its source, but leave it
	// to the scene to delete it once all particles have expired

	oapi::ParticleStream **tmp;
	if (nreentrystream > 1) {
		tmp = new oapi::ParticleStream*[nreentrystream-1]; TRACENEW
		for (j = k = 0; j < nreentrystream; j++)
			if (j != i) tmp[k++] = reentrystream[j];
	} else tmp = 0;
	delete []reentrystream;
	reentrystream = tmp;
	nreentrystream--;
	return true;
}

// ==============================================================

void Vessel::ClearReentryStreams ()
{
	if (nreentrystream) {
		for (DWORD i = 0; i < nreentrystream; i++)
			reentrystream[i]->Detach();
		delete []reentrystream;
		reentrystream = NULL;
		nreentrystream = 0;
	}
}

// ==============================================================

LightEmitter *Vessel::AddPointLight (const VECTOR3 &pos, double range, double att0, double att1, double att2, COLOUR4 col_diff, COLOUR4 col_spec, COLOUR4 col_ambi)
{
	LightEmitter **tmp = new LightEmitter*[nemitter+1];
	if (nemitter) {
		memcpy (tmp, emitter, nemitter*sizeof(LightEmitter*));
		delete []emitter;
	}
	emitter = tmp;
	LightEmitter *pl = new PointLight ((OBJHANDLE)this, pos, range, att0, att1, att2, col_diff, col_spec, col_ambi);
	emitter[nemitter++] = pl;
	return pl;
}

// ==============================================================

LightEmitter *Vessel::AddSpotLight (const VECTOR3 &pos, const VECTOR3 &dir, double range, double att0, double att1, double att2, double umbra, double penumbra, COLOUR4 col_diff, COLOUR4 col_spec, COLOUR4 col_ambi)
{
	LightEmitter **tmp = new LightEmitter*[nemitter+1];
	if (nemitter) {
		memcpy (tmp, emitter, nemitter*sizeof(LightEmitter*));
		delete []emitter;
	}
	emitter = tmp;
	LightEmitter *pl = new SpotLight ((OBJHANDLE)this, pos, dir, range, att0, att1, att2, umbra, penumbra, col_diff, col_spec, col_ambi);
	emitter[nemitter++] = pl;
	return pl;
}

// ==============================================================

void Vessel::ClearLightEmitters ()
{
	if (!nemitter) return; // nothing to do
	for (DWORD i = 0; i < nemitter; i++) {
		if (emitter[i]) delete emitter[i];
	}
	delete []emitter;
	nemitter = 0;
}

// ==============================================================

bool Vessel::DelLightEmitter (LightEmitter *le)
{
	DWORD i, j, k;
	for (i = 0; i < nemitter; i++) {
		if (emitter[i] == le) {
			LightEmitter **tmp;
			if (nemitter > 1) {
				tmp = new LightEmitter*[nemitter-1];
				for (j = k = 0; j < nemitter; j++)
					if (j != i) tmp[k++] = emitter[j];
			} else tmp = NULL;
			delete []emitter;
			emitter = tmp;
			delete le;
			nemitter--;
			return true;
		}
	}
	return false;
}

// ==============================================================

void Vessel::LightEmitterState (LightEmitter *le, int param, void *value)
{
	DWORD idx;
	char cbuf[256];
	for (idx = 0; idx < nemitter; idx++)
		if (emitter[idx] == le) break;
	if (idx == nemitter) return;
	switch (param) {
	case 0: { // activation state
		bool active = *(bool*)value;
		sprintf (cbuf, "%d ACTIVATE %d", idx, active);
		FRecorder_SaveEvent ("LIGHTSOURCE", cbuf);
	} break;
	}
}

// ==============================================================

void Vessel::ShiftLightEmitters (const VECTOR3 &ofs)
{
	DWORD i;
	for (i = 0; i < nemitter; i++) {
		emitter[i]->ShiftExplicitPosition (ofs);
	}
}

// ==============================================================

TankSpec *Vessel::CreatePropellantResource (double maxmass, double mass, double efficiency)
{
	TankSpec *ts = new TankSpec; TRACENEW
	ts->maxmass = maxmass;
	ts->mass = ts->pmass = (mass >= 0.0 ? mass : maxmass);
	ts->efficiency = efficiency;
	TankSpec **pts = new TankSpec*[ntank+1]; TRACENEW
	if (ntank) {
		memcpy (pts, tank, ntank * sizeof(TankSpec*));
		delete []tank;
	}
	tank = pts;
	if (!ntank) def_tank = ts;
	return tank[ntank++] = ts;
}

// ==============================================================

void Vessel::DelPropellantResource (TankSpec *ts)
{
	DWORD i, j, k;

	// unlink thrusters which refer to ts
	for (auto it = m_thruster.begin(); it != m_thruster.end(); it++)
		if ((*it)->tank == ts) (*it)->tank = nullptr;

	// if we delete the default tank, reset
	if (def_tank == ts) def_tank = 0;

	// remove resource
	for (i = ntank-1; (int)i >= 0; i--) {
		if (tank && tank[i] == ts) {
			TankSpec **tmp;
			if (ntank > 1) {
				tmp = new TankSpec*[ntank-1]; TRACENEW
				for (j = k = 0; j < ntank; j++)
					if (j != i) tmp[k++] = tank[j];
			} else tmp = 0;
			delete []tank;
			tank = tmp;
			ntank--;
		}
	}
}

// ==============================================================

void Vessel::ClearPropellantResources ()
{
	// unlink all thrusters from their fuel resources
	for (auto it = m_thruster.begin(); it != m_thruster.end(); it++)
		(*it)->tank = nullptr;

	// remove all fuel resources
	if (ntank) {
		for (DWORD i = 0; i < ntank; i++)
			delete tank[i];
		delete []tank;
		tank = NULL;
		ntank = 0;
	}

	// reset default tank
	def_tank = 0;
}

// ==============================================================

double Vessel::GetPropellantFlowrate (const TankSpec *ts) const
{
	return (ts->pmass - ts->mass) * td.iSimDT;
}

// ==============================================================

PortSpec *Vessel::CreateDock (const Vector &pos, const Vector &dir, const Vector &rot)
{
	PortSpec **tmp = new PortSpec*[ndock+1]; TRACENEW
	if (ndock) {
		memcpy (tmp, dock, ndock*sizeof (PortSpec*));
		delete []dock;
	}
	dock = tmp;
	dock[ndock] = new PortSpec; TRACENEW
	dock[ndock]->ref.Set (pos);
	dock[ndock]->dir.Set (dir);
	dock[ndock]->rot.Set (rot);
	dock[ndock]->mate = 0;
	dock[ndock]->ids  = 0;
	dock[ndock]->pending = 0;
	dock[ndock]->status = 0;
	dock[ndock]->owner = this;
	dock[ndock]->autodock = true;
	return dock[ndock++];
}

// ==============================================================

void Vessel::SetDockParams (PortSpec *dock, const Vector &pos, const Vector &dir, const Vector &rot)
{
	dock->ref.Set (pos);
	dock->dir.Set (dir);
	dock->rot.Set (rot);
}

// ==============================================================

void Vessel::ShiftDocks (const Vector &ofs)
{
	for (DWORD i = 0; i < ndock; i++)
		SetDockParams (dock[i], dock[i]->ref+ofs, dock[i]->dir, dock[i]->rot);
}

// ==============================================================

void Vessel::SetDockIDS (PortSpec *dock, float freq, float range)
{
	if (dock->ids) {
		dock->ids->SetFreq (freq);
		dock->ids->SetRange (range);
	} else {
		dock->ids = new Nav_IDS (this, dock, freq, range); TRACENEW
	}
}

// ==============================================================

bool Vessel::DelDock (PortSpec *dk)
{
	for (DWORD i = 0; i < ndock; i++)
		if (dock[i] == dk) return DelDock (i);
	return false;
}

// ==============================================================

bool Vessel::DelDock (DWORD i)
{
	if (i >= ndock) return false;
	if (dock[i]->mate) Undock (i, 0, 0.0); // undock docked vessel before deleting the dock
	if (dock[i]->ids) {
		g_psys->BroadcastVessel (MSG_KILLNAVSENDER, dock[i]->ids);
		delete dock[i]->ids;
	}
	delete dock[i];
	PortSpec **tmp;
	if (ndock > 1) {
		DWORD j, k;
		tmp = new PortSpec*[ndock-1]; TRACENEW
		for (j = k = 0; j < ndock; j++)
			if (j != i) tmp[k++] = dock[j];
	} else tmp = 0;
	delete []dock;
	dock = tmp;
	ndock--;
	return true;
}

// ==============================================================

void Vessel::ClearDockDefinitions ()
{
	if (ndock) {
		for (DWORD i = 0; i < ndock; i++) {
			if (dock[i]->mate) Undock (i, 0, 0.0);
			if (dock[i]->ids) {
				g_psys->BroadcastVessel (MSG_KILLNAVSENDER, dock[i]->ids);
				delete dock[i]->ids;
			}
			delete dock[i];
		}
		delete []dock;
		dock = NULL;
		ndock = 0;
	}
}

// ==============================================================

void Vessel::RegisterDocking (DWORD did, Vessel *mate, DWORD matedid)
{
	if (mate) {
		dock[did]->mate = mate;
		dock[did]->matedock = matedid;
		dock[did]->pending = 0;
		dock[did]->status = 0;
		if (modIntf.v->Version() >= 1)
			((VESSEL2*)modIntf.v)->clbkDockEvent (did, (OBJHANDLE)mate);
	}
}

// ==============================================================

void Vessel::UnregisterDocking (DWORD did)
{
	if (dock[did]->mate) {
		dock[did]->mate = 0;
		if (modIntf.v->Version() >= 1)
			((VESSEL2*)modIntf.v)->clbkDockEvent (did, 0);
		undock_t = td.SimT0;
	}
}

// ==============================================================

void Vessel::MoveDock(PortSpec* pD, const Vector& pos, const Vector& dir, const Vector& rot)
{
	pD->ref = pos;
	pD->dir = dir;
	pD->rot = rot;
	if (pD->mate) if (supervessel) supervessel->SoftDockUpdate(this, pD);
}

// ==============================================================

PortSpec* Vessel::GetProxyDock(PortSpec *pD)
{
	double maxd = 1e30;
	PortSpec* pTgt = nullptr;
	if (ndock == 0) return nullptr;
	if (pD->mate) return nullptr;

	assert(pD->owner == this);

	Vector dofs = mul(s0->R, pD->ref);

	for (auto v : g_psys->GetVessels())
	{
		if (v == this) continue;
		if (v->proxybody != proxybody) continue;
		if (v->ndock == 0) continue;
		
		double dst = s0->pos.dist(v->GPos());
		if ((dst < 1.5 * (size + v->Size()) || (dst < size + v->Size() + 1e3))) { // valid candidate		
			for (int k = 0; k < v->ndock; k++) { // loop over vessel's docks
				if (v->dock[k]->mate) continue; // dock already busy
				Vector dref = tmul(v->GRot(), dofs + s0->pos - v->GPos());
				dref = dref - v->dock[k]->ref;
				double d = dref.length();
				if (d < maxd) {
					maxd = d;
					pTgt = v->dock[k];
				}
			}	
		}
	}
	return pTgt;
}

// ==============================================================

bool Vessel::GetTargetDockAlignment(PortSpec* pD, PortSpec* pT, Vector* ref, Vector* dir, Vector* rot, Vector* vel)
{
	Vessel* pVT = pT->owner;
	if (pVT->proxybody != proxybody) return false;
	if (ref) *ref = tmul(GRot(), mul(pVT->GRot(), pT->ref) + pVT->GPos() - GPos()) - pD->ref;
	if (dir) *dir = tmul(GRot(), mul(pVT->GRot(), pT->dir));
	if (rot) *rot = tmul(GRot(), mul(pVT->GRot(), pT->rot));
	if (vel) *vel = mul(GRot(), crossp(pD->ref, s0->omega)) + s0->vel - pVT->s0->vel; // relvel ECL 
	return true;
}

// ==============================================================

int Vessel::Dock (Vessel *target, DWORD mydid, DWORD tgtdid, DWORD mode)
{
	if (dock[mydid]->mate) return 1;          // error: my dock already in use
	if (target->dock[tgtdid]->mate) return 2; // error: target dock already in use
	if (supervessel)
		if (supervessel->isComponent (target)) return 3;
		// error: we are already docked to the target (directly or indirectly)

	if (mode) {
		Vector P;
		Matrix R;
		if (mode == 3) { // Soft-dock, continue with mode 0
			Vector rf, dr, rt;
			if (GetTargetDockAlignment(dock[mydid], target->dock[tgtdid], &rf, &dr, &rt)) {
				dock[mydid]->ref = rf + dock[mydid]->ref;
				dock[mydid]->dir = -dr;
				dock[mydid]->rot = rt;
			}
			mode = 0;
		}

		if (mode == 1) { // drag target to our current position
			RelDockingPos (target, mydid, tgtdid, P, R);
			target->RPlace (s0->pos + mul(s0->R, P), s0->vel);
			R.premul (s0->R);
			target->fstatus = fstatus;
			target->SetRotationMatrix (R);
			target->UpdateProxies();
			target->UpdateSurfParams();
			target->bSurfaceContact = target->CheckSurfaceContact();
		} else { // drag vessel to target position
			target->RelDockingPos (this, tgtdid, mydid, P, R);
			RPlace (target->GPos() + mul(target->GRot(), P), target->GVel());
			R.premul (target->GRot());
			SetRotationMatrix (R);
			UpdateProxies();
			UpdateSurfParams();
			fstatus = target->fstatus;
			bSurfaceContact = CheckSurfaceContact();
		}
	}
	g_psys->DockVessels (this, target, mydid, tgtdid);
	RegisterDocking (mydid, target, tgtdid);
	target->RegisterDocking (tgtdid, this, mydid);

#ifndef INLINEGRAPHICS
	auto gc = g_pOrbiter->GetGraphicsClient();
	if (gc) gc->clbkScenarioChanged((OBJHANDLE)this, ScnChgEvent::Docked);
#endif
	return 0;
}

// ==============================================================

bool Vessel::Undock (UINT did, const Vessel *exclude, double vsep)
{
	bool undocked = false;
	DWORD n, n0, n1;

	if (did == ALLDOCKS) n0 = 0, n1 = ndock;
	else                 n1 = (n0 = did) + 1;

	for (n = n0; n < n1; n++) {
		if (n < ndock && dock[n]->mate && dock[n]->mate != exclude) {
			g_psys->UndockVessel (supervessel, this, n, vsep);
			dock[n]->mate->UnregisterDocking (dock[n]->matedock);
			UnregisterDocking (n);
			undocked = true;
		}
	}
	if (bFRrecord && undocked) { // record undock event
		std::string buf;
		for (n = n0; n < n1; ++n) buf += ' ' + std::to_string(n);
		FRecorder_SaveEvent("UNDOCK", buf.substr(1).data());
	}

#ifndef INLINEGRAPHICS
	auto gc = g_pOrbiter->GetGraphicsClient();
	if (gc) gc->clbkScenarioChanged((OBJHANDLE)this, ScnChgEvent::UnDocked);
#endif
	return undocked;
}

// ==============================================================

struct UndockSpec {
	Vessel *vessel;
	const Vessel *exclude;
	double vsep;
} undockspec;

bool Vessel::ClbkSelect_Undock (Select *menu, int item, char *str, void *data)
{
	UndockSpec *us = (UndockSpec*)data;
	DWORD i;
	char cbuf[16] = "Dock ";
	for (i = 0; i < us->vessel->ndock; i++) {
		sprintf(cbuf + 5, "%d", i + 1);
		menu->Append (cbuf, us->vessel->dock[i]->mate ? 0 : ITEM_NOHILIGHT);
	}
	return true;
}

bool Vessel::ClbkEnter_Undock (Select *menu, int item, char *str, void *data)
{
	return ClbkName_Undock (0, str+5, data);
}

bool Vessel::ClbkName_Undock (InputBox*, char *str, void *data)
{
	UINT did;
	UndockSpec *us = (UndockSpec*)data;
	if (sscanf (str, "%d", &did) && did >= 1 && did <= us->vessel->ndock)
		return us->vessel->Undock (did-1, us->exclude, us->vsep);
	else
		return false;	
}

void Vessel::UndockInteractive (const Vessel *exclude, double vsep)
{
	UINT did = 0;
	undockspec.vessel = this;
	undockspec.exclude = exclude;
	undockspec.vsep = vsep;
	if (ndock == 1) {
		Undock (0, exclude, vsep);
	} else if (ndock) {
		if (ndock <= 20) {
			g_select->Open ("Select dock to disengage", ClbkSelect_Undock, ClbkEnter_Undock, (void*)&undockspec);
		} else {
			char cbuf[128] = "Select dock to disengage ";
			sprintf (cbuf+25, "(1-%d)", ndock);
			g_input->Open (cbuf, 0, 20, ClbkName_Undock, (void*)&undockspec);
		}
	}
}

// ==============================================================

void Vessel::RelDockingPos (const Vessel *target, UINT mydid, UINT tgtdid, Vector &P, Matrix &R)
{
	// Calculate the relative position 'P' and orientation 'R' of 'target'
	// in my reference frame, if we are docked between 'mydid' and 'tgtdid'

	// relative orientation
	Vector as(target->dock[tgtdid]->dir);
	Vector bs(target->dock[tgtdid]->rot);
	Vector cs(crossp(as,bs));
	Vector at(-dock[mydid]->dir);
	Vector bt( dock[mydid]->rot);
	Vector ct(crossp(at,bt));
	double den = cs.x * (as.y*bs.z - as.z*bs.y) +
		         cs.y * (as.z*bs.x - as.x*bs.z) +
				 cs.z * (as.x*bs.y - as.y*bs.x);
	R.m11 = (ct.x * (as.y*bs.z - as.z*bs.y) +
		     bt.x * (as.z*cs.y - as.y*cs.z) +
			 at.x * (bs.y*cs.z - bs.z*cs.y)) / den;
	R.m12 = (ct.x * (as.z*bs.x - as.x*bs.z) +
			 bt.x * (as.x*cs.z - as.z*cs.x) +
		     at.x * (bs.z*cs.x - bs.x*cs.z)) / den;
	R.m13 = (ct.x * (as.x*bs.y - as.y*bs.x) +
		     bt.x * (as.y*cs.x - as.x*cs.y) +
			 at.x * (bs.x*cs.y - bs.y*cs.x)) / den;
	R.m21 = (ct.y * (as.y*bs.z - as.z*bs.y) +
		     bt.y * (as.z*cs.y - as.y*cs.z) +
			 at.y * (bs.y*cs.z - bs.z*cs.y)) / den;
	R.m22 = (ct.y * (as.z*bs.x - as.x*bs.z) +
		     bt.y * (as.x*cs.z - as.z*cs.x) +
			 at.y * (bs.z*cs.x - bs.x*cs.z)) / den;
	R.m23 = (ct.y * (as.x*bs.y - as.y*bs.x) +
		     bt.y * (as.y*cs.x - as.x*cs.y) +
			 at.y * (bs.x*cs.y - bs.y*cs.x)) / den;
	R.m31 = (ct.z * (as.y*bs.z - as.z*bs.y) +
		     bt.z * (as.z*cs.y - as.y*cs.z) +
			 at.z * (bs.y*cs.z - bs.z*cs.y)) / den;
	R.m32 = (ct.z * (as.z*bs.x - as.x*bs.z) +
		     bt.z * (as.x*cs.z - as.z*cs.x) +
			 at.z * (bs.z*cs.x - bs.x*cs.z)) / den;
	R.m33 = (ct.z * (as.x*bs.y - as.y*bs.x) +
		     bt.z * (as.y*cs.x - as.x*cs.y) +
			 at.z * (bs.x*cs.y - bs.y*cs.x)) / den;

	// relative position
	P.Set (dock[mydid]->ref - mul (R, target->dock[tgtdid]->ref));
}


AttachmentSpec *Vessel::CreateAttachment (bool toparent, const Vector &pos, const Vector &dir, const Vector &rot, const char *id, bool loose)
{
	AttachmentSpec *as;
	if (toparent) { // create attachment to parent vessel
		AttachmentSpec **tmp = new AttachmentSpec*[npattach+1]; TRACENEW
		if (npattach) {
			memcpy (tmp, pattach, npattach*sizeof (AttachmentSpec*));
			delete []pattach;
		}
		pattach = tmp;
		as = pattach[npattach++] = new AttachmentSpec; TRACENEW
	} else {        // create attachment for child vessel
		AttachmentSpec **tmp = new AttachmentSpec*[ncattach+1]; TRACENEW
		if (ncattach) {
			memcpy (tmp, cattach, ncattach*sizeof (AttachmentSpec*));
			delete []cattach;
		}
		cattach = tmp;
		as = cattach[ncattach++] = new AttachmentSpec; TRACENEW
	}
	as->ref.Set (pos);
	as->dir.Set (dir);
	as->rot.Set (rot);
	as->toparent = toparent;
	as->loose = loose;
	as->mate = 0;
	strncpy (as->id, id, 8);
	return as;
}

bool Vessel::DelAttachment (AttachmentSpec *as)
{
	int i, j, k;

	// first check the "to parent" attachment list
	for (i = 0; i < npattach; i++) {
		if (as == pattach[i]) {
			// detach from parent if required
			if (as == attach) DetachFromParent();
			// remove attachment spec from list
			AttachmentSpec **tmp = NULL;
			if (npattach > 1) {
				tmp = new AttachmentSpec*[npattach-1];
				for (j = k = 0; j < npattach; j++)
					if (j != i) tmp[k++] = pattach[j];
			}
			delete []pattach;
			pattach = tmp;
			npattach--;
			return true;
		}
	}

	// now check the "to child" attachment list
	for (i = 0; i < ncattach; i++) {
		if (as == cattach[i]) {
			// detach from child if required
			if (as->mate) DetachChild (as);
			// remove attachment spec from list
			AttachmentSpec **tmp = NULL;
			if (ncattach > 1) {
				tmp = new AttachmentSpec*[ncattach-1];
				for (j = k = 0; j < ncattach; j++)
					if (j != i) tmp[k++] = cattach[j];
			}
			delete []cattach;
			cattach = tmp;
			ncattach--;
			return true;
		}
	}

	// attachment point not found
	return false;
}

void Vessel::ClearAttachments ()
{
	if (attach) DetachFromParent();
	if (npattach) {
		for (DWORD i = 0; i < npattach; i++)
			delete pattach[i];
		delete []pattach;
		pattach = NULL;
		npattach = 0;
	}
	if (ncattach) {
		for (DWORD i = 0; i < ncattach; i++) {
			if (cattach[i]->mate) DetachChild (cattach[i]);
			delete cattach[i];
		}
		delete []cattach;
		cattach = NULL;
		ncattach = 0;
	}
}

void Vessel::SetAttachmentParams (AttachmentSpec *as, const Vector &pos, const Vector &dir, const Vector &rot)
{
	Vector at(-as->dir);
	Vector bt( as->rot);
	Vector ct(crossp(at,bt));

	as->ref.Set (pos);
	as->dir.Set (dir);
	as->rot.Set (rot);

	if (as->mate) {
		if (as->toparent) {
			// this doesn't currently allow for loose attachment updates
			InitAttachmentToParent (as);
			UpdatePassive ();
		} else {
			Matrix R1(at.x, at.y, at.z,  bt.x, bt.y, bt.z,  ct.x, ct.y, ct.z);
			at.Set (-as->dir), bt.Set (as->rot), ct.Set (crossp(at,bt));
			Matrix R2(at.x, bt.x, ct.x,  at.y, bt.y, ct.y,  at.z, bt.z, ct.z);
			as->mate->attach_rrot.premul (R1);
			as->mate->attach_rrot.premul (R2);
			as->mate->attach_rrot.orthogonalise (++orthoaxis % 3);
			as->mate->attach_rpos.Set (as->ref - mul (as->mate->attach_rrot, as->mate_attach->ref));
			as->mate->UpdatePassive ();
		}
	}
}

bool Vessel::AttachChild (Vessel *child, AttachmentSpec *as, AttachmentSpec *asc, bool allow_loose)
{
	if (!child->AttachToParent (this, as, asc, allow_loose)) return false;
	as->mate = child;
	as->mate_attach = asc;
	if (bFRrecord) {
		DWORD pidx = GetAttachmentIndex (as);
		DWORD cidx = child->GetAttachmentIndex (asc);
		char cbuf[256];
		sprintf (cbuf, "%s %d %d", child->Name(), pidx, cidx);
		if (allow_loose) strcat (cbuf, " LOOSE");
		FRecorder_SaveEvent ("ATTACH", cbuf);
	}

#ifndef INLINEGRAPHICS
	auto gc = g_pOrbiter->GetGraphicsClient();
	if (gc) gc->clbkScenarioChanged((OBJHANDLE)this, ScnChgEvent::Attached);
#endif
	return true;
}

bool Vessel::AttachToParent (Vessel *parent, AttachmentSpec *asp, AttachmentSpec *as, bool allow_loose)
{
	if (attach) DetachFromParent(); // already attached to a parent
	if (!as) return false;
	as->mate = parent;
	as->mate_attach = asp;
	InitAttachmentToParent (as, allow_loose);

#ifndef INLINEGRAPHICS
	auto gc = g_pOrbiter->GetGraphicsClient();
	if (gc) gc->clbkScenarioChanged((OBJHANDLE)this, ScnChgEvent::Attached);
#endif
	return true;
}

bool Vessel::DetachChild (AttachmentSpec *asp, double v)
{
	if (!asp->mate) return false; // nothing attached
	if (!asp->mate->DetachFromParent (v)) return false;
	asp->mate = 0;
	if (bFRrecord) {
		DWORD pidx = GetAttachmentIndex (asp);
		char cbuf[256];
		sprintf (cbuf, "%d, %0.3f", pidx, v);
		FRecorder_SaveEvent ("DETACH", cbuf);
	}

#ifndef INLINEGRAPHICS
	auto gc = g_pOrbiter->GetGraphicsClient();
	if (gc) gc->clbkScenarioChanged((OBJHANDLE)this, ScnChgEvent::Detached);
#endif
	return true;
}

bool Vessel::DetachFromParent (double v)
{
	if (!attach || !proxybody) return false; // we are not attached to any parent
	Vessel *prnt = attach->mate;

	Vector vel(s0->vel);
	cbody = prnt->cbody;
	vel += mul (prnt->GRot(), crossp (attach_rpos, prnt->s0->omega));
	if (v) { // ejection velocity
		vel += mul (s0->R, tmul (attach_rrot, attach->mate_attach->dir)) * v;
	}
	//fstatus = FLIGHTSTATUS_FREEFLIGHT;
	RPlace (s0->pos, vel);
	UpdateProxies ();
	sp.alt = s0->pos.dist (proxybody->GPos()) - proxybody->Size();
	UpdateSurfParams ();
	bSurfaceContact = CheckSurfaceContact();
	attach->mate = 0;
	attach->mate_attach->mate = 0;
	attach = 0;

	if (bFRplayback) FRecorder_CheckEnd();
	//bFRplayback = false;

#ifndef INLINEGRAPHICS
	auto gc = g_pOrbiter->GetGraphicsClient();
	if (gc) gc->clbkScenarioChanged((OBJHANDLE)this, ScnChgEvent::Detached);
#endif
	return true;
}

// ==============================================================

void Vessel::InitAttachmentToParent (AttachmentSpec *asc, bool allow_loose)
{
	// define rotation matrix from child to parent
	AttachmentSpec *asp = asc->mate_attach; // parent attachment

	if (allow_loose && asp->loose) { // freeze current relative child orientation

		attach_rrot.Set (GRot());
		attach_rrot.tpremul (asc->mate->GRot());

	} else {

		Vector as(asc->dir);
		Vector bs(asc->rot);
		Vector cs(crossp(as,bs));
		Vector at(-asp->dir);
		Vector bt( asp->rot);
		Vector ct(crossp(at,bt));
		double den = cs.x * (as.y*bs.z - as.z*bs.y) +
			     cs.y * (as.z*bs.x - as.x*bs.z) +
				 cs.z * (as.x*bs.y - as.y*bs.x);
		attach_rrot.m11 = (ct.x * (as.y*bs.z - as.z*bs.y) +
			               bt.x * (as.z*cs.y - as.y*cs.z) +
				           at.x * (bs.y*cs.z - bs.z*cs.y)) / den;
		attach_rrot.m12 = (ct.x * (as.z*bs.x - as.x*bs.z) +
				           bt.x * (as.x*cs.z - as.z*cs.x) +
				           at.x * (bs.z*cs.x - bs.x*cs.z)) / den;
		attach_rrot.m13 = (ct.x * (as.x*bs.y - as.y*bs.x) +
			               bt.x * (as.y*cs.x - as.x*cs.y) +
				           at.x * (bs.x*cs.y - bs.y*cs.x)) / den;
		attach_rrot.m21 = (ct.y * (as.y*bs.z - as.z*bs.y) +
			               bt.y * (as.z*cs.y - as.y*cs.z) +
				           at.y * (bs.y*cs.z - bs.z*cs.y)) / den;
		attach_rrot.m22 = (ct.y * (as.z*bs.x - as.x*bs.z) +
			               bt.y * (as.x*cs.z - as.z*cs.x) +
				           at.y * (bs.z*cs.x - bs.x*cs.z)) / den;
		attach_rrot.m23 = (ct.y * (as.x*bs.y - as.y*bs.x) +
			               bt.y * (as.y*cs.x - as.x*cs.y) +
				           at.y * (bs.x*cs.y - bs.y*cs.x)) / den;
		attach_rrot.m31 = (ct.z * (as.y*bs.z - as.z*bs.y) +
			               bt.z * (as.z*cs.y - as.y*cs.z) +
				           at.z * (bs.y*cs.z - bs.z*cs.y)) / den;
		attach_rrot.m32 = (ct.z * (as.z*bs.x - as.x*bs.z) +
			               bt.z * (as.x*cs.z - as.z*cs.x) +
				           at.z * (bs.z*cs.x - bs.x*cs.z)) / den;
		attach_rrot.m33 = (ct.z * (as.x*bs.y - as.y*bs.x) +
			               bt.z * (as.y*cs.x - as.x*cs.y) +
				           at.z * (bs.x*cs.y - bs.y*cs.x)) / den;
	}

	// child position relative to parent
	attach_rpos.Set (asp->ref - mul (attach_rrot, asc->ref));
	attach = asc;
}

// ==============================================================

AttachmentSpec *Vessel::GetAttachmentFromIndex (bool toparent, DWORD i)
{
	if (toparent) return (i < npattach ? pattach[i] : 0);
	else          return (i < ncattach ? cattach[i] : 0);
}

// ==============================================================

DWORD Vessel::GetAttachmentIndex (AttachmentSpec *as) const
{
	if (as->toparent) {
		for (DWORD i = 0; i < npattach; i++)
			if (as == pattach[i]) return i;
	} else {
		for (DWORD i = 0; i < ncattach; i++)
			if (as == cattach[i]) return i;
	}
	return (DWORD)-1;
}

// ==============================================================

void Vessel::ShiftAttachments (const Vector &ofs)
{
	DWORD i;
	for (i = 0; i < npattach; i++)
		SetAttachmentParams (pattach[i], pattach[i]->ref+ofs, pattach[i]->dir, pattach[i]->rot);
	for (i = 0; i < ncattach; i++)
		SetAttachmentParams (cattach[i], cattach[i]->ref+ofs, cattach[i]->dir, cattach[i]->rot);
}

// ==============================================================

Vessel* Vessel::GetAttachmentRoot()
{
	Vessel* mate = nullptr;
	int cnt = 0;
	for (DWORD i = 0; i < npattach; i++) {
		if (pattach[i]) {
			if (pattach[i]->toparent == false) {
				oapiWriteLog("[ERROR] Parent attachment with no toparent flag"); // Sanity check
				DebugBreak();
			}
			if (pattach[i]->mate) {	mate = pattach[i]->mate; cnt++;	}
		}
	}
	if (cnt > 1) {
		oapiWriteLog("[ERROR] Vessel has multiple parent attachments"); // Sanity check
		DebugBreak();
	}

	if (cnt == 1) return mate->GetAttachmentRoot();

	return this;
}

// ==============================================================

void Vessel::AddBeacon (BEACONLIGHTSPEC *bs)
{
	BEACONLIGHTSPEC **tmp = new BEACONLIGHTSPEC*[nbeacon+1]; TRACENEW
	if (nbeacon) {
		memcpy (tmp, beacon, nbeacon*sizeof(BEACONLIGHTSPEC*));
		delete []beacon;
	}
	beacon = tmp;
	beacon[nbeacon++] = bs;
}

bool Vessel::DelBeacon (BEACONLIGHTSPEC *bs)
{
	DWORD i, j, k;
	for (i = 0; i < nbeacon; i++) {
		if (beacon[i] == bs) {
			BEACONLIGHTSPEC **tmp = 0;
			if (nbeacon > 1) {
				tmp = new BEACONLIGHTSPEC*[nbeacon-1]; TRACENEW
				for (j = k = 0; j < nbeacon; j++) {
					if (j != i) tmp[k++] = beacon[j];
				}
			}
			delete []beacon;
			beacon = tmp;
			nbeacon--;
			return true;
		}
	}
	return false;
}

void Vessel::ClearBeacons ()
{
	if (nbeacon) {
		delete []beacon;
		beacon = NULL;
		nbeacon = 0;
	}
}

const BEACONLIGHTSPEC *Vessel::GetBeacon (DWORD idx) const
{
	return (idx < nbeacon ? beacon[idx] : NULL);
}

UINT Vessel::MakeFreeMeshEntry (UINT idx)
{
	if (idx == UINT(-1)) // search for an unused entry in the existing list
		for (idx = 0; idx < nmesh; idx++)
			if (!meshlist[idx]) break;

	if (idx >= nmesh) { // insert beyond end of current list
		MeshList **tmp = new MeshList*[idx+1]; TRACENEW
		if (nmesh) {
			memcpy (tmp, meshlist, nmesh*sizeof(MeshList*));
			delete []meshlist;
		}
		meshlist = tmp;
		for (UINT i = nmesh; i <= idx; i++) meshlist[i] = 0;
		nmesh = idx+1;
	}

	if (!meshlist[idx]) meshlist[idx] = new MeshList; TRACENEW
	return idx;
}

void Vessel::ScanMeshCaps ()
{
	extpassmesh = false;

	for (UINT i = 0; i < nmesh; i++) {
		if (!meshlist[i]) continue;
		if (meshlist[i]->vismode & MESHVIS_EXTPASS) {
			extpassmesh = true;
			break;
		}
	}
}

UINT Vessel::AddMesh (const char *mname, const VECTOR3 *ofs)
{
	return InsertMesh (mname, (UINT)-1, ofs);
}

UINT Vessel::InsertMesh (const char *mname, UINT idx, const VECTOR3 *ofs)
{
	UINT i;
	idx = MakeFreeMeshEntry (idx);

	strcpy (meshlist[idx]->meshname, mname);
	if (ofs) memcpy (&meshlist[idx]->meshofs, ofs, sizeof(VECTOR3));
	else     memset (&meshlist[idx]->meshofs, 0, sizeof(VECTOR3));

	meshlist[idx]->hMesh = 0; // no preloaded mesh
	meshlist[idx]->vismode = MESHVIS_EXTERNAL; // external view only by default
	meshlist[idx]->crc = 0;
	for (i = 0; i < strlen(mname); i++)
		*((BYTE*)&meshlist[idx]->crc + (i%4)) += (BYTE)mname[i];
	mesh_crc += meshlist[idx]->crc; // encode mesh name
	for (i = 0; i < sizeof(ofs); i++)
		*((BYTE*)&mesh_crc + (i%4)) += *((BYTE*)&ofs+i); // encode offset
	ScanMeshCaps();
	BroadcastVisMsg (EVENT_VESSEL_INSMESH, idx); // notify visuals

	return idx;
}

UINT Vessel::AddMesh (MESHHANDLE hMesh, const VECTOR3 *ofs)
{
	return InsertMesh (hMesh, (UINT)-1, ofs);
}

UINT Vessel::InsertMesh (MESHHANDLE hMesh, UINT idx, const VECTOR3 *ofs)
{
	UINT i;
	idx = MakeFreeMeshEntry (idx);

	meshlist[idx]->meshname[0] = '\0';
	if (ofs) memcpy (&meshlist[idx]->meshofs, ofs, sizeof(VECTOR3));
	else     memset (&meshlist[idx]->meshofs, 0, sizeof(VECTOR3));

	meshlist[idx]->hMesh = hMesh; // pointer to preloaded mesh
	meshlist[idx]->vismode = MESHVIS_EXTERNAL; // external view only by default
	meshlist[idx]->crc = (DWORD_PTR)hMesh;
	mesh_crc += meshlist[idx]->crc; // encode mesh pointer
	for (i = 0; i < sizeof(ofs); i++)
		*((BYTE*)&mesh_crc + (i%4)) += *((BYTE*)&ofs+i); // encode offset
	ScanMeshCaps();
	BroadcastVisMsg (EVENT_VESSEL_INSMESH, idx); // notify visuals

	return idx;
}

bool Vessel::DelMesh (UINT idx, bool retain_anim)
{
	if (idx >= nmesh) return false; // mesh index out of range
	if (!meshlist[idx]) return false; // mesh already deleted
	
	BroadcastVisMsg (EVENT_VESSEL_DELMESH, idx); // notify visuals
	//g_pOrbiter->VesselEvent (this, EVENT_VESSEL_DELMESH, (void*)idx);
	delete meshlist[idx];
	meshlist[idx] = 0;
	ScanMeshCaps();

	// remove associated animation components
	if (!retain_anim) {
		UINT i, j;
		for (i = 0; i < nanim; i++) {
			ANIMATION &A = anim[i];
			for (j = A.ncomp-1; j != (UINT)-1; j--) {
				ANIMATIONCOMP *AC = A.comp[j];
				if (AC->trans->mesh == idx)
					DelAnimationComponent (i, AC);
			}
			if (!A.ncomp) // no components left
				DelAnimation (i);
		}
	}

	return true;
}

void Vessel::ClearMeshes (bool retain_anim)
{
	if (!nmesh) return;

	if (!retain_anim) ClearAnimations (true);

	for (UINT i = 0; i < nmesh; i++)
		if (meshlist[i]) delete meshlist[i];
	delete []meshlist;
	meshlist = NULL;
	nmesh = 0;
	mesh_crc = 0;
	ScanMeshCaps();
	BroadcastVisMsg (EVENT_VESSEL_DELMESH, (DWORD_PTR)((UINT)-1)); // notify visual
}

bool Vessel::ShiftMesh (UINT idx, const VECTOR3 &ofs)
{
	if (idx >= nmesh || !meshlist[idx]) return false;
	meshlist[idx]->meshofs += ofs;
	BroadcastVisMsg (EVENT_VESSEL_MESHOFS, idx);
	return true;
}

const MESHHANDLE Vessel::GetMeshTemplate (UINT idx) const
{
	if (idx < nmesh && meshlist[idx]) return meshlist[idx]->hMesh;
	else return NULL;
}

const char *Vessel::GetMeshName (UINT idx) const
{
	if (idx < nmesh && meshlist[idx]->meshname[0]) return meshlist[idx]->meshname;
	else return NULL;
}

Mesh *Vessel::CopyMeshFromTemplate (UINT idx)
{
	if (idx >= nmesh) return 0; // out of range check
	Mesh *mesh = new Mesh; TRACENEW
	MeshList *meshrec = meshlist[idx];
	if (meshrec) {
		if (meshrec->hMesh) { // copy from preloaded mesh
			mesh->Set (*(Mesh*)meshrec->hMesh);
		} else {                    // load from file
			if (!LoadMesh (meshrec->meshname, *mesh)) {
				delete mesh; mesh = 0;
			}
		}
	} else mesh = 0;
	return mesh;
}

void Vessel::SetMeshVisibilityMode (UINT meshidx, WORD mode)
{
	if (meshidx < nmesh) {
		meshlist[meshidx]->vismode = mode;
		if (mode & MESHVIS_EXTPASS) extpassmesh = true;
		BroadcastVisMsg (EVENT_VESSEL_MESHVISMODE, meshidx);
	}
}

int Vessel::MeshModified (MESHHANDLE hMesh, UINT grp, DWORD modflag)
{
	struct {
		MESHHANDLE hMesh;
		UINT grp;
		DWORD modflag;
	} moddata = { hMesh, grp, modflag };

	BroadcastVisMsg (EVENT_VESSEL_MODMESHGROUP, (DWORD_PTR)&grp); // notify visual
	return 0;
}

void Vessel::InitLanded (Planet *planet, double lng, double lat, double dir, const Matrix *hrot, double cgelev, bool asComponent)
{
	dCHECK(!g_bStateUpdate, "Vessel::InitLanded must not be called during state update") // not valid during update phase

	Vector nml;
	int i;

	if (!hrot) {
		// Calculate compressed touchdown points at equilibrium
		double prad = planet->Size();
		double mg = Ggrav * planet->Mass() * mass / (prad*prad); // approximate vessel weight
		Vector tp_comp[3];
		for (i = 0; i < 3; i++)
			tp_comp[i] = touchdown_vtx[i].pos + touchdown_nm*(touchdown_vtx[i].compression*mg);

		// equilibrium touchdown plane normal
		nml = crossp (tp_comp[0]-(tp_comp[1]+tp_comp[2])*0.5, tp_comp[2]-tp_comp[1]).unit();

		// Distance of origin from compressed touchdown plane
		cgelev = dotp (nml, -tp_comp[0]);
	} else {
		double slng = sin(lng), clng = cos(lng);
		double slat = sin(lat), clat = cos(lat);
		Matrix L2H (-slng,0,clng, clat*clng,slat,clat*slng, -slat*clng,clat,-slat*slng);
		nml = tmul (*hrot, tmul (L2H, Vector(0,1,0)));
	}

	proxybody = proxyplanet = planet;
	sp.SetLanded (lng, lat, cgelev, dir, nml, planet);

	if (hrot) {
		land_rot.Set (*hrot);
	} else {
		double sdir = sin(dir), cdir = cos(dir);
		planet->EquatorialToLocal (sp.slng, sp.clng, sp.slat, sp.clat, sp.rad, sp.ploc);
		// rotation ship local -> planet local
		land_rot.Set ( sp.clng*sp.slat*sdir-sp.slng*cdir, sp.clng*sp.clat, -sp.clng*sp.slat*cdir-sp.slng*sdir,
			          -sp.clat*sdir,                      sp.slat,          sp.clat*cdir,
				       sp.slng*sp.slat*sdir+sp.clng*cdir, sp.clat*sp.slng, -sp.slng*sp.slat*cdir+sp.clng*sdir);

		// rotate ship to its proper touchdown tilt
		double tilt = acos (nml.y);
		if (tilt > 1e-8) {
			double phi = atan2 (nml.x, -nml.z);
			double sinp = sin(phi),   cosp = cos(phi);
			Matrix R (cosp, 0, sinp,   0, 1, 0,   -sinp, 0, cosp);
			double sina = sin(tilt), cosa = cos(tilt);
			R.premul (Matrix (1, 0, 0,   0, cosa, -sina,   0, sina, cosa));
			R.premul (Matrix (cosp, 0, -sinp,   0, 1, 0,   sinp, 0, cosp));
			land_rot.postmul (R);
		}
	}

	if (supervessel && !asComponent) {
		// note: need to transform the parameters
		supervessel->InitLanded (planet, lng, lat, dir, &land_rot, cgelev);
	}

	double vground = Pi2 * sp.rad * sp.clat / planet->RotT();
	s0->vel.Set (-vground*sp.slng, 0.0, vground*sp.clng);
	s0->pos.Set (mul (planet->GRot(), sp.ploc) + planet->GPos());
	s0->vel.Set (mul (planet->GRot(), s0->vel) + planet->GVel());
	rvel_base.Set (s0->vel);
	rvel_add.Set (0,0,0);
	Amom.Set (0,0,0); // this is not quite true since the planet rotates ...
	s0->Q.Set (land_rot);
	s0->Q.premul (planet->GQ());
	s0->R.Set (s0->Q);
	if (s1) { // should we allow this?
		s1->Set(*s0);
	}

	//RPlace (s0->pos, s0->vel);
	// Needs thought! Calling RPlace causes problems with orientation of Atlantis on pad
	// Leaving it out causes problems elsewhere (?)
	cpos = s0->pos - cbody->GPos();
 	cvel = s0->vel - cbody->GVel();
	UpdateSurfParams();
	g_psys->ScanGFieldSources(&s0->pos, this, &gfielddata);

	fstatus = FLIGHTSTATUS_LANDED;
	sp.SetLanded (lng, lat, cgelev, dir, nml, planet);
	bGroundProximity = true;
	bSurfaceContact = true;
	bForceActive = false;
	el_valid = false;
	FRecorder_SaveEvent ("LANDED", proxybody->Name());
}

void Vessel::InitDocked (const Vessel *vessel, int port)
{
	if (!ndock) return; // problems!
	Vector as(dock[0]->dir);
	Vector bs(dock[0]->rot);
	Vector cs(crossp(as,bs));
	Vector at(vessel->dock[0]->dir);
	Vector bt(vessel->dock[0]->rot);
	Vector ct(crossp(at,bt));
	double den = cs.x * (as.y*bs.z - as.z*bs.y) +
		         cs.y * (as.z*bs.x - as.x*bs.z) +
				 cs.z * (as.x*bs.y - as.y*bs.x);
	land_rot.m11 = (ct.x * (as.y*bs.z - as.z*bs.y) +
		            bt.x * (as.z*cs.y - as.y*cs.z) +
					at.x * (bs.y*cs.z - bs.z*cs.y)) / den;
	land_rot.m12 = (ct.x * (as.z*bs.x - as.x*bs.z) +
					bt.x * (as.x*cs.z - as.z*cs.x) +
		            at.x * (bs.z*cs.x - bs.x*cs.z)) / den;
	land_rot.m13 = (ct.x * (as.x*bs.y - as.y*bs.x) +
		            bt.x * (as.y*cs.x - as.x*cs.y) +
					at.x * (bs.x*cs.y - bs.y*cs.x)) / den;
	land_rot.m21 = (ct.y * (as.y*bs.z - as.z*bs.y) +
		            bt.y * (as.z*cs.y - as.y*cs.z) +
					at.y * (bs.y*cs.z - bs.z*cs.y)) / den;
	land_rot.m22 = (ct.y * (as.z*bs.x - as.x*bs.z) +
		            bt.y * (as.x*cs.z - as.z*cs.x) +
					at.y * (bs.z*cs.x - bs.x*cs.z)) / den;
	land_rot.m23 = (ct.y * (as.x*bs.y - as.y*bs.x) +
		            bt.y * (as.y*cs.x - as.x*cs.y) +
					at.y * (bs.x*cs.y - bs.y*cs.x)) / den;
	land_rot.m31 = (ct.z * (as.y*bs.z - as.z*bs.y) +
		            bt.z * (as.z*cs.y - as.y*cs.z) +
					at.z * (bs.y*cs.z - bs.z*cs.y)) / den;
	land_rot.m32 = (ct.z * (as.z*bs.x - as.x*bs.z) +
		            bt.z * (as.x*cs.z - as.z*cs.x) +
					at.z * (bs.z*cs.x - bs.x*cs.z)) / den;
	land_rot.m33 = (ct.z * (as.x*bs.y - as.y*bs.x) +
		            bt.z * (as.y*cs.x - as.x*cs.y) +
					at.z * (bs.x*cs.y - bs.y*cs.x)) / den;
	// can land_rot be expressed directly in quaternion representation?

	Vector dockpos;
	vessel->LocalToGlobal (vessel->dock[0]->ref, dockpos);
	s0->Q.Set (land_rot);
	s0->Q.premul (vessel->GQ());
	s0->R.Set (s0->Q);
	RPlace (dockpos - mul (s0->R, dock[0]->ref), vessel->GVel());
	fstatus = FLIGHTSTATUS_FREEFLIGHT;
}

void Vessel::InitOrbiting (const Vector &relr, const Vector &relv, const Vector &rot, const Vector *_vrot)
{
	cpos = relr, cvel = relv;

	 // sanity check: position vector must have nonzero length
	if (!cpos.x && !cpos.y && !cpos.z) cpos.z = 1.0;

	// sanity check: make sure we are above ground
	double rad, elev = 0.0;
	ElevationManager *emgr = (cbody->Type() == OBJTP_PLANET ? ((Planet*)cbody)->ElevMgr() : 0);
	if (emgr) {
		double lng, lat, alt0;
		cbody->LocalToEquatorial (tmul(cbody->GRot(), cpos), lng, lat, rad);
		alt0 = rad-cbody->Size();
		int reslvl = (int)(32.0-log(max(alt0,100.0))*LOG2);
		elev = emgr->Elevation (lat, lng, reslvl, &etile);
	} else {
		rad = cpos.length();
	}
	if (rad < cbody->Size() + elev) {
		double scale = (cbody->Size() + elev) / rad;
		cpos *= scale;
	}

	el->Calculate (cpos, cvel, td.SimT0);

	// set rotation matrix from axis rotation vector
	s0->R.Set (rot);
	s0->Q.Set (s0->R);

	if (_vrot) s0->omega.Set (*_vrot);

	RPlace(cpos + cbody->GPos(), cvel + cbody->GVel());
}

void Vessel::SetProxyplanet (Planet *pp)
{
	VesselBase::SetProxyplanet (pp);
	landtgt = 0;
	for (DWORD i = 0; i < nnav; i++) nav[i].dbidx = -1;
}

void Vessel::UpdateMass ()
{
	pfmass = fmass, fmass = 0.0;
	for (DWORD i = 0; i < ntank; i++) fmass += tank[i]->mass;
	mass = emass + fmass;
}

bool Vessel::SetNavMode (int mode, bool fromstream)
{
	if (bFRplayback && !fromstream) return false;
	// ignore manual SetNavMode during playback

	DWORD bitflag = 1 << (mode-1);
	int i;
	switch (mode) {
	case 0: // clear all modes
		for (i = 0; i < 7; i++)
			if (navmode & (1 << i)) ClrNavMode (i+1);
		return true;
	case NAVMODE_KILLROT:   // the "exclusive" nav-modes
		killrot_delay = 0;
	case NAVMODE_HLEVEL:
	case NAVMODE_PROGRADE:
	case NAVMODE_RETROGRADE:
	case NAVMODE_NORMAL:
	case NAVMODE_ANTINORMAL:
		if (!(navmode & bitflag)) {
			navmode |= bitflag;
			FRecorder_SaveEventInt ("NAVMODE", mode);
			for (i = NAVMODE_KILLROT; i <= NAVMODE_ANTINORMAL; i++) {
				if (mode == NAVMODE_KILLROT && i == NAVMODE_HLEVEL) continue;
				// "special rule": let killrot coexist with hlevel
				if (i != mode) ClrNavMode (i, false, fromstream);
			}
			ModuleSignalNavmode (mode, true);
			return true;
		}
		break;
	case NAVMODE_HOLDALT:
		SetHoverHoldAltitude (sp.alt0, false);
		return true;
	}
	return false;
}

bool Vessel::ClrNavMode (int mode, bool record, bool fromstream)
{
	if (bFRplayback && !fromstream) return false;
	// ignore manual ClrNavMode during playback

	DWORD bitflag = 1 << (mode-1);
	if (navmode & bitflag) {
		navmode ^= bitflag;
		if (record) FRecorder_SaveEventInt ("NAVMODECLR", mode);
		ModuleSignalNavmode (mode, false);
		return true;
	}
	return false;
}

bool Vessel::TglNavMode (int mode)
{
	if (NavModeActive (mode)) return ClrNavMode (mode);
	else                      return SetNavMode (mode);
}

bool Vessel::NavModeActive (int mode)
{
	DWORD bitflag = 1 << (mode-1);
	return (navmode & bitflag) != 0;
}

void Vessel::SetHoverHoldAltitude (double alt, bool terrainalt)
{
	DWORD bitflag = 1 << (NAVMODE_HOLDALT-1);
	if (!(navmode & bitflag)) {
		navmode |= bitflag;
		ModuleSignalNavmode (NAVMODE_HOLDALT, true);
	}
	hoverhold.alt = hoverhold.palt = alt;
	hoverhold.terrain = terrainalt;
	hoverhold.valt = sp.vspd; // 0.0;
	hoverhold.T = td.SimT1; // td.SimT0 + holdaltDT;
	FRecorder_SaveEventInt ("NAVMODE", NAVMODE_HOLDALT);
}

#ifdef UNDEF
double Vessel::GetSpec (VesselSpec which)
{
	switch (which) {
	case SPEC_THRUSTMAIN: return max_main_thrust;
	case SPEC_TRUSTRETRO: return max_retro_thrust;
	case SPEC_THRUSTHOVER: return max_hover_thrust;
	default: return 0.0; // just in case
	}
}
#endif

int Vessel::ConsumeDirectKey (char *buffer)
{
	if (modIntf.v->Version() >= 1) {
		return ((VESSEL2*)modIntf.v)->clbkConsumeDirectKey (buffer);
	} else {
		return 0;
	}
}

int Vessel::ConsumeBufferedKey (DWORD key, bool down, char *kstate)
{
	int res;
	const Keymap &keymap = g_pOrbiter->keymap;

	// first offer the key to the module
	if (modIntf.v->Version() >= 1)
		if (res = ((VESSEL2*)modIntf.v)->clbkConsumeBufferedKey (key, down, kstate)) return res;
	// If module processes the key, we skip default handler

	if (!down) return 0; // at the moment, only process keydown events

	// Navigation computer modes
	if (keymap.IsLogicalKey (key, kstate, OAPI_LKEY_NMHoldAltitude)) TglNavMode (NAVMODE_HOLDALT);
	if (keymap.IsLogicalKey (key, kstate, OAPI_LKEY_NMHLevel))       TglNavMode (NAVMODE_HLEVEL);
	if (keymap.IsLogicalKey (key, kstate, OAPI_LKEY_NMPrograde))     TglNavMode (NAVMODE_PROGRADE);
	if (keymap.IsLogicalKey (key, kstate, OAPI_LKEY_NMRetrograde))   TglNavMode (NAVMODE_RETROGRADE);
	if (keymap.IsLogicalKey (key, kstate, OAPI_LKEY_NMNormal))       TglNavMode (NAVMODE_NORMAL);
	if (keymap.IsLogicalKey (key, kstate, OAPI_LKEY_NMAntinormal))   TglNavMode (NAVMODE_ANTINORMAL);
	if (keymap.IsLogicalKey (key, kstate, OAPI_LKEY_NMKillrot))      TglNavMode (NAVMODE_KILLROT);
	if (keymap.IsLogicalKey (key, kstate, OAPI_LKEY_RCSEnable))      SetAttMode (attmode >= 1 ? 0 : 1);
	if (keymap.IsLogicalKey (key, kstate, OAPI_LKEY_RCSMode))        ToggleAttMode ();
	if (keymap.IsLogicalKey (key, kstate, OAPI_LKEY_Undock))         UndockInteractive ();

	// HUD control (internal view only)
	if (g_camera->IsInternal()) {
		if (keymap.IsLogicalKey (key, kstate, OAPI_LKEY_HUD))          g_pane->ToggleHUD();
		if (keymap.IsLogicalKey (key, kstate, OAPI_LKEY_HUDMode))      g_pane->SwitchHUDMode();
		if (keymap.IsLogicalKey (key, kstate, OAPI_LKEY_HUDReference))
			if (g_pane->GetHUD())                                      g_pane->GetHUD()->SelectReference();
		if (keymap.IsLogicalKey (key, kstate, OAPI_LKEY_HUDTarget))
			if (g_pane->GetHUD() && g_pane->GetHUD()->Mode() == HUD_DOCKING)    ((HUD_Docking*)g_pane->GetHUD())->SelectReferenceOld();
	}

	if (KEYMOD_CONTROL (kstate)) {  // CTRL-Key combinations

		switch (key) {
		case OAPI_KEY_C:        // landing/takeoff clearance request
			IssueClearanceRequest ();
			return 1;
		}

	} else if (KEYMOD_SHIFT (kstate)) { // virtual instrument
	} else if (KEYMOD_ALT (kstate)) {   // ALT-Key combinations

		switch (key) {
		case OAPI_KEY_DIVIDE:   // connect/disconnect user input to aerodynamic control surfaces
			ToggleADCtrlMode ();
			return 1;
		}

	}
	return 0;
}

// =======================================================================
// If vessel is part of a superstructure: set 'cg' to coordinates of
// superstructure CG in vessel coordinates and return true.
// Otherwise: set 'cg' to (0,0,0) and return false

bool Vessel::GetSuperStructCG (Vector &cg) const
{
	if (!supervessel) {
		cg.Set(0,0,0);
		return false;
	} else {
		supervessel->GetCG (this, cg);
		return true;
	}
}

// =======================================================================

double Vessel::MaxAngularMoment (int axis) const
{
	dCHECK(axis >= 0 && axis < 6, "Invalid axis index.")

	if (!supervessel) return max_angular_moment[axis];
	Vector vcg;
	VECTOR3 M = {0,0,0};
	supervessel->GetCG (this, vcg);
	const ThrustGroupSpec *tgs = &m_thrusterGroupDef[THGROUP_ATT_PITCHUP+axis];
	for (auto it = tgs->ts.begin(); it != tgs->ts.end(); it++)
		M += crossp ((*it)->dir * (*it)->maxth0, (*it)->ref-MakeVECTOR3(vcg));
	return length (M);
}

// =======================================================================

// should be moved to RigidBody
bool Vessel::SetGravityGradientDamping (double damp)
{
	if (bDistmass) {
		tidaldamp = damp;
		return true;
	} else return false;
}

// should be moved to RigidBody
double Vessel::GetGravityGradientDamping () const
{
	return (bDistmass ? tidaldamp : 0.0);
}

// =======================================================================
// calculate body forces and moments (Flin and Amom) except gravitational
// force and surface impact force (which is done separately by integration along the trajectory)

void Vessel::UpdateBodyForces ()
{
	Lift = Drag = SideForce = 0.0;

	if (m_thruster.size()) UpdateThrustForces ();
	if (CtrlSurfSyncMode) ApplyControlSurfaceLevels ();
	if (fstatus != FLIGHTSTATUS_LANDED) {
		if (sp.is_in_atm) UpdateAerodynamicForces ();
		if (rpressure) UpdateRadiationForces ();
	}
	bForceActive |= (Flin_add.x || Flin_add.y || Flin_add.z || Amom_add.x || Amom_add.y || Amom_add.z);
}

// =======================================================================
// Update thrust levels and resulting body forces

void Vessel::UpdateThrustForces ()
{
	UINT j;
	Vector F;

	// Navigation computer sequences
	if (navmode) {

		int navmode_core = (modIntf.v->Version() >= 3 ? ((VESSEL4*)modIntf.v)->clbkNavProcess(navmode) : navmode);
		// allow module to process the navigation modes itself
			
		Vector fpmi, *fullpmi;
		if (supervessel) {
			fullpmi = &fpmi;
			supervessel->GetPMI (this, fpmi);
		} else {
			fullpmi = &pmi;
		}
		//const Vector *fullpmi = (supervessel ? supervessel->GetPMI() : &pmi);
		double mam[6];
		for (int i = 0; i < 6; i++) mam[i] = MaxAngularMoment(i) * 10.0;

		// "kill rotation" navcomp mode
		if (navmode_core & NAVBIT_KILLROT) {

			double arot_tot, level;
			bool finished = true;
			// rotation around x-axis
			arot_tot = (-s0->omega.x*fullpmi->x*td.iSimDT + (fullpmi->z-fullpmi->y)*s0->omega.y*s0->omega.z)*mass;
			if (arot_tot <= 0.0) {
				if ((level = arot_tot/mam[1]) < -1.0) level = -1.0, finished = false;
				IncThrusterGroupOverride (THGROUP_ATT_PITCHDOWN, -level);
			} else {
				if ((level = arot_tot/mam[0]) >  1.0) level =  1.0, finished = false;
				IncThrusterGroupOverride (THGROUP_ATT_PITCHUP, level);
			}

			// rotation around y-axis
			arot_tot = (-s0->omega.y*fullpmi->y*td.iSimDT + (fullpmi->x-fullpmi->z)*s0->omega.z*s0->omega.x)*mass;
			if (arot_tot <= 0.0) {
				if ((level = arot_tot/mam[3]) < -1.0) level = -1.0, finished = false;
				IncThrusterGroupOverride (THGROUP_ATT_YAWRIGHT, -level);
			} else {
				if ((level = arot_tot/mam[2]) >  1.0) level =  1.0, finished = false;
				IncThrusterGroupOverride (THGROUP_ATT_YAWLEFT, level);
			}

			// rotation around z-axis
			arot_tot = (-s0->omega.z*fullpmi->z*td.iSimDT + (fullpmi->y-fullpmi->x)*s0->omega.x*s0->omega.y)*mass;
			if (arot_tot <= 0.0) {
				if ((level = arot_tot/mam[5]) < -1.0) level = -1.0, finished = false;
				IncThrusterGroupOverride (THGROUP_ATT_BANKLEFT, -level);
			} else {
				if ((level = arot_tot/mam[4]) >  1.0) level =  1.0, finished = false;
				IncThrusterGroupOverride (THGROUP_ATT_BANKRIGHT, level);
			}

			//if (finished && ++killrot_delay > 8) // add a delay to improve performance at high time acceleration
			if (s0->omega.length() < 1e-5)
				ClrNavMode (NAVMODE_KILLROT);

			//sprintf (DBG_MSG, "vrot: x=%e, y=%e, z=%e", vrot.x, vrot.y, vrot.z);
		}

		// "level horizon" navcomp mode
		if (navmode_core & NAVBIT_HLEVEL) {
			double r, v0, v1, amax, dth;

			// neutralise pitch
			r    = sp.pitch;
			amax = max_angular_moment[r >= 0.0 ? 1:0]/(pmi.x*mass); // max angular acceleration
			v0   = s0->omega.x;             // current velocity
			if (r >= 0.0) v1 = -sqrt ( 0.04*r*amax);
			else          v1 =  sqrt (-0.04*r*amax);
			dth  = (v1-v0)*td.iSimDT/amax * 0.2;
			if (dth >= 0.0) IncThrusterGroupOverride (THGROUP_ATT_PITCHUP,   min ( dth, 0.8));
			else            IncThrusterGroupOverride (THGROUP_ATT_PITCHDOWN, min (-dth, 0.8));

			// neutralise bank
			r = sp.bank;
			amax = max_angular_moment[r >= 0.0 ? 5:4]/(pmi.z*mass); // max angular acceleration
			v0 = s0->omega.z;
			if (r >= 0.0) v1 =  sqrt ( 0.04*r*amax);
			else          v1 = -sqrt (-0.04*r*amax);
			dth  = (v1-v0)*td.iSimDT/amax * 0.2;
			if (dth >= 0.0) IncThrusterGroupOverride (THGROUP_ATT_BANKRIGHT, min ( dth, 0.8));
			else            IncThrusterGroupOverride (THGROUP_ATT_BANKLEFT,  min (-dth, 0.8));
		}

		// "turn prograde/retrograde/normal/antinormal" navcomp mode
		if (navmode_core & (NAVBIT_PROGRADE|NAVBIT_RETROGRADE|NAVBIT_NORMAL|NAVBIT_ANTINORMAL)) {
			double v1, dth;
			Vector tgtdir;
			if (navmode_core & (NAVBIT_PROGRADE|NAVBIT_RETROGRADE)) {
				tgtdir.Set (tmul (GRot(), GVel() - cbody->GVel()));  // prograde direction in ship coordinates
				if (navmode_core & NAVBIT_RETROGRADE) tgtdir = -tgtdir;
			} else {
				tgtdir.Set (tmul (GRot(), crossp (GVel()-cbody->GVel(), GPos()-cbody->GPos()))); // remember left-handed system
				if (navmode_core & NAVBIT_ANTINORMAL) tgtdir = -tgtdir;
			}
			tgtdir.unify();

			const double dvmax = 5.0*RAD; // max change in angular velocity [rad/s]
			const double alpha0 = 20.0*RAD; // angular distance from target at which thrust is being reduced
			const double f0 = dvmax/alpha0;

			if ((v1 = f0 * asin(tgtdir.y)) >= 0.0) dth = (min(v1, dvmax)-s0->omega.x)/max_angular_moment[1];
			else                                   dth = (max(v1,-dvmax)-s0->omega.x)/max_angular_moment[0];
			if ((dth *= td.iSimDT*pmi.x*mass) >= 0.0)
				SetThrusterGroupOverride (THGROUP_ATT_PITCHUP,   min ( dth, 0.4));
			else
				SetThrusterGroupOverride (THGROUP_ATT_PITCHDOWN, min (-dth, 0.4));

			if ((v1 = -f0 * atan2 (tgtdir.x, tgtdir.z)) >= 0.0) dth = (min (v1, dvmax)-s0->omega.y)/max_angular_moment[3];
			else                                                dth = (max (v1,-dvmax)-s0->omega.y)/max_angular_moment[2];
			if ((dth *= td.iSimDT*pmi.y*mass) >= 0.0)
				SetThrusterGroupOverride (THGROUP_ATT_YAWLEFT,  min ( dth, 0.4));
			else
				SetThrusterGroupOverride (THGROUP_ATT_YAWRIGHT, min (-dth, 0.4));

			tgtdir.Set (tmul (GRot(), GPos()- cbody->GPos()));
			tgtdir.unify();
			v1 = asin(tgtdir.y) * ((navmode_core & (NAVBIT_PROGRADE|NAVBIT_NORMAL)) ? -f0:f0);
			if (v1 >= 0.0) dth = (min (v1, dvmax)-s0->omega.z)/max_angular_moment[5];
			else           dth = (max (v1,-dvmax)-s0->omega.z)/max_angular_moment[4];
			if ((dth *= td.iSimDT*pmi.z*mass) >= 0.0)
				SetThrusterGroupOverride (THGROUP_ATT_BANKRIGHT, min ( dth, 0.4));
			else
				SetThrusterGroupOverride (THGROUP_ATT_BANKLEFT,  min (-dth, 0.4));
		}

		// "hold altitude" navcomp mode
		if ((navmode_core & NAVBIT_HOLDALT) && (td.SimT1 > hoverhold.T)) {
			HoverHoldAltitude();
		}

	} // end navmode

	double th;
	TankSpec *ts;

	// record previous fuel mass
	for (j = 0; j < ntank; j++) tank[j]->pmass = tank[j]->mass;

	// update thruster-induced forces
	m_bThrustEngaged = false;
	Thrust.Set (0,0,0);
	for (auto it = m_thruster.begin(); it != m_thruster.end(); it++) {
		ThrustSpec* thruster = *it;
		if (thruster->level = max (0.0, min (1.0, thruster->level_permanent + thruster->level_override))) {
			if ((ts = thruster->tank) && ts->mass) {     // fuel available?
				th = thruster->maxth0 * thruster->level; // vacuum thrust
				if (burnfuel) {                          // consume fuel
					ts->mass -= th/(ts->efficiency * thruster->isp0) * td.SimDT;
					if (ts->mass < 0.0) ts->mass = 0.0;
				}
				th *= ThrusterAtmScale (thruster, sp.atmp);  // atmospheric thrust scaling
				F = MakeVector(thruster->dir * th);
				Thrust += F;
				Amom_add += crossp (F, MakeVector(thruster->ref));
				m_bThrustEngaged = true;
			} else thruster->level = thruster->level_permanent = 0.0; // no fuel
		}
		thruster->level_override = 0.0; // reset temporary thruster level
		//thruster->level = thruster->level_permanent;
	}
	if (m_bThrustEngaged) Flin_add += Thrust;
}

// =======================================================================
// "Hover hold altitude" program

void Vessel::HoverHoldAltitude ()
{
	const double vh_max = 2e1;  // max vertical velocity

	double dt = td.SimT1 - hoverhold.T;
	if (!dt) return;
	hoverhold.T = td.SimT1;

	double alt = (hoverhold.terrain ? sp.alt : sp.alt0);
	double dh = alt - hoverhold.alt;
	double sgn = (dh >= 0.0 ? 1.0 : -1.0);
	double vh = sp.vspd;
	double ah = (vh-hoverhold.valt)/dt;
	hoverhold.valt = vh;

	double vh_tgt = -dh*0.1;
	if (fabs (vh_tgt) > vh_max) vh_tgt = -vh_max*sgn;
	double dvh = vh_tgt-vh;
	double a_tgt = dvh;     // this is the acceleration we need to reach target velocity in 1s
	double da = a_tgt - ah; // this is the required change in acceleration
	double a_max = GetThrusterGroupMaxth (THGROUP_HOVER)/mass;
	double dlvl = da/a_max;
	double dlvl_max = dt;
	if (fabs(dlvl) > dlvl_max)
		dlvl = (dlvl > 0.0 ? dlvl_max : -dlvl_max);

	IncThrusterGroupLevel (THGROUP_HOVER, dlvl);
}

// =======================================================================
// Update perturbation forces due to radiation pressure

void Vessel::UpdateRadiationForces ()
{
	double illum = IlluminationFactor();
	if (!illum) return; // we are in shadow

	Vector mflux = GetMomentumFlux() * illum;
	Vector F, r;

	if (modIntf.v->Version() >= 2) {
		// retrieve customised force calculation
		VECTOR3 Mflux, p, pos;
		Mflux.x = mflux.x, Mflux.y = mflux.y, Mflux.z = mflux.z;
		((VESSEL3*)modIntf.v)->clbkGetRadiationForce (Mflux, p, pos);
		F.x = p.x, F.y = p.y, F.z = p.z;
		r.x = pos.x, r.y = pos.y, r.z = pos.z;
	} else {
		// simplistic implementation
		double cs = size*size;
		double albedo = 1.5;
		F = mflux * (cs*albedo);
	}
	Flin_add += F;
	if (r.x || r.y || r.z)
		Amom_add += crossp (F, r);
}

// =======================================================================
// This function encodes the atmospheric flight model
// NEEDS EXTENSIVE OVERHAUL!

void Vessel::UpdateAerodynamicForces ()
{
	if (!nairfoil) { UpdateAerodynamicForces_OLD (); return; }
	// use old atmospheric flight model;

	DWORD i;

	if (!sp.airvel_ship.z) return;
	// in the pathological case where there is no longitudinal airflow we stop

	double aoa  = atan2 (-sp.airvel_ship.y,sp.airvel_ship.z); // angle of attack
	double beta = atan2 (-sp.airvel_ship.x,sp.airvel_ship.z); // lateral angle of attack (slip)
	double gamma = (!sp.airvel_ship.y) ? atan2(-sp.airvel_ship.x, -sp.airvel_ship.y) : 0;

	const double mu = 1.7894e-5; // viscosity coefficient dummy - MAKE VARIABLE!
	double Re0 = sp.atmrho * sp.airspd / mu; // template for Reynolds coefficient (to be multiplied by chord length)

	// damping of angular velocity
	double dynpm = 0.5*sp.atmrho * (sp.airspd+30)*(sp.airspd+30); // modified dynamic pressure
	double fac = dynpm * cs.y;
#define OLD_AERODAMPING
#ifdef OLD_AERODAMPING
	if (s0->omega.x) Amom_add.x -= min (fac*rdrag.x, pmi.x*mass*td.iSimDT) * s0->omega.x;
	if (s0->omega.y) Amom_add.y -= min (fac*rdrag.y, pmi.y*mass*td.iSimDT) * s0->omega.y;
	if (s0->omega.z) Amom_add.z -= min (fac*rdrag.z, pmi.z*mass*td.iSimDT) * s0->omega.z;
#else
	if (vrot.x || vrot.y || vrot.z) {
		double adrag, maxdrag;
		adrag   = fac*rdrag.x*vrot.x;
		maxdrag = (vrot.x*pmi.x*iSimDT - (pmi.y-pmi.z)*vrot.y*vrot.z)*mass;
		Amom_add.x -= (fabs(adrag) < fabs(maxdrag) ? adrag:maxdrag);
		adrag   = fac*rdrag.y*vrot.y;
		maxdrag = (vrot.y*pmi.y*iSimDT - (pmi.z-pmi.x)*vrot.z*vrot.x)*mass;
		Amom_add.y -= (fabs(adrag) < fabs(maxdrag) ? adrag:maxdrag);
		adrag   = fac*rdrag.z*vrot.z;
		maxdrag = (vrot.z*pmi.z*iSimDT - (pmi.x-pmi.y)*vrot.x*vrot.y)*mass;
		Amom_add.z -= (fabs(adrag) < fabs(maxdrag) ? adrag:maxdrag);
	}

#endif

	Vector ddir (-sp.airvel_ship.unit());           // freestream airflow direction (= drag direction)
	Vector ldir (0, sp.airvel_ship.z, -sp.airvel_ship.y);  ldir.unify(); // lift direction (vertical on drag and transversal ship axis)
	Vector sdir (sp.airvel_ship.z, 0, -sp.airvel_ship.x);  sdir.unify(); // sidelift direction (vertical on drag and vertical ship axis)
	double lift, drag, side, S;
	double CL, CD;   // lift, drag coeffs (used by AirfoilCoeffFunc and AirfoilCoeffFuncEx)
	double CA, CN, CY; //force coefficients along Orbiter Z, Y, X axes (AirfoilCoeffFuncEx2)
	double Cl, Cm, Cn; //moment coefficients about Orbiter Z, X, Y axes (AirfoilCoeffFuncEx2 uses Cl and Cn)

	// airfoil lift+drag components
	for (i = 0; i < nairfoil; i++) {
		AirfoilSpec *af = airfoil[i];
		if (af->align == LIFT_VERTICAL) {
			if (af->version == 0)
				af->cf (aoa, sp.atmM, Re0*af->c, &CL, &Cm, &CD);
			else
				((AirfoilCoeffFuncEx)af->cf)((VESSEL*)modIntf.v, aoa, sp.atmM, Re0*af->c, af->context, &CL, &Cm, &CD);
			if (af->S) S = af->S;
			else       S = fabs(ddir.z)*cs.z + fabs(ddir.y)*cs.y; // use projected vessel CS as reference area
			AddForce (ldir*(lift=(CL*sp.dynp*S)) + ddir*(drag=(CD*sp.dynp*S)), af->ref);
			if (Cm) Amom_add.x += Cm*sp.dynp*af->S*af->c;
			Lift += lift, Drag += drag;
		} else if (af->align == LIFT_HORIZONTAL) { // horizontal lift component
			if (af->version == 0)
				af->cf (beta, sp.atmM, Re0*af->c, &CL, &Cm, &CD);
			else
				((AirfoilCoeffFuncEx)af->cf)((VESSEL*)modIntf.v, beta, sp.atmM, Re0*af->c, af->context, &CL, &Cm, &CD);
			if (af->S) S = af->S;
			else       S = fabs(ddir.z)*cs.z + fabs(ddir.x)*cs.z; // use projected vessel CS as reference area
			AddForce (sdir*(side = (CL*sp.dynp*S)) + ddir*(drag=(CD*sp.dynp*S)), af->ref);
			if (Cm) Amom_add.y += Cm*sp.dynp*af->S*af->c;
			Drag += drag;
			SideForce += side;
		} else if (af->align == FORCE_AND_MOMENT) {
			((AirfoilCoeffFuncEx2)af->cf)((VESSEL*)modIntf.v, -_V(sp.airvel_ship.unit().x, sp.airvel_ship.unit().y, sp.airvel_ship.unit().z), aoa, beta, gamma, sp.atmM, Re0 * af->c, af->context, &CA, &CN, &CY, &Cl, &Cm, &Cn);
			if (af->S) S = af->S;
			else       S = fabs(ddir.z) * cs.z + fabs(ddir.x) * cs.z; // use projected vessel CS as reference area

			Vector AeroForce((CY * S) * sp.dynp, (CN * S) * sp.dynp, -(CA * S) * sp.dynp);

			AddForce(AeroForce, af->ref);
			Lift = dotp(AeroForce, ldir);
			Drag = dotp(AeroForce, ddir);
			SideForce = dotp(AeroForce, sdir);
			
			Amom_add.x += Cm * sp.dynp * af->S * af->c;
			Amom_add.y -= Cn * sp.dynp * af->S * af->c;
			Amom_add.z -= Cl * sp.dynp * af->S * af->c;

		}
	}

	// airfoil control surfaces
	for (i = 0; i < nctrlsurf; i++) {
		double lvl = ctrlsurf_level[ctrlsurf[i]->ctrl].curr;
		if (lvl) {
			double fac = ctrlsurf[i]->area * sp.dynp;
			double cdrag = fabs (lvl) * fac;
			double clift = -lvl * fac * ctrlsurf[i]->dCl;
			// for now, we assume a fixed set of control axes. Should be freely definable
			switch (ctrlsurf[i]->axis) {
			case AIRCTRL_AXIS_XPOS: // horizontal control axis (+X), e.g. elevator, right aileron
				AddForce (ldir*clift + ddir*cdrag, ctrlsurf[i]->ref);
				break;
			case AIRCTRL_AXIS_XNEG: // horizontal control axis (-X), e.g. canards, left aileron
				AddForce (ldir*(-clift) + ddir*cdrag, ctrlsurf[i]->ref);
				break;
			case AIRCTRL_AXIS_YPOS: // vertical control axis (+Y), e.g. rudder
				AddForce (sdir*clift + ddir*cdrag, ctrlsurf[i]->ref);
				break;
			case AIRCTRL_AXIS_YNEG: // vertical control axis (-Y)
				AddForce (sdir*(-clift) + ddir*cdrag, ctrlsurf[i]->ref);
				break;
			}
		}
	}

	// user-defined drag elements
	for (i = 0; i < ndragel; i++) {
		double lvl = *dragel[i]->drag;
		if (lvl) {
			CD = lvl*dragel[i]->factor;
			drag = CD*sp.dynp;
			AddForce (ddir*drag, dragel[i]->ref);
			Drag += drag;
		}
	}
}

// =======================================================================
// Old-style atmospheric flight model - OBSOLETE !!!

void Vessel::UpdateAerodynamicForces_OLD ()
{
	const double eps = 1e-8;

	if (sp.airspd < eps) return; // nothing to do

	double aoa  = atan2 (-sp.airvel_ship.y,sp.airvel_ship.z); // angle of attack
	double beta = atan2 ( sp.airvel_ship.x,sp.airvel_ship.z); // lateral angle of attack (slip)

	// note: this section may cause catastrophic instabilities at high time acceleration!

	// apply force which will try to rotate longitudinal axis into flight path
	double daoa = aoa-trim_scale*ctrlsurf_level[AIRCTRL_ELEVATORTRIM].curr;
	double fac  = size * mass * sp.atmrho;
	Amom_add.x -= fac * sp.airspd * cs.y * sin(daoa) * pitch_moment_scale;
	Amom_add.y -= fac * sp.airspd * cs.x * sin(beta) * bank_moment_scale;
	// atmospheric friction (rotation deceleration)
	// linear in vrot - is this correct?
	if (s0->omega.x) Amom_add.x -= fac * s0->omega.x * rdrag.x;
	if (s0->omega.y) Amom_add.y -= fac * s0->omega.y * rdrag.y;
	if (s0->omega.z) Amom_add.z -= fac * s0->omega.z * rdrag.z;

	// lift and drag. Note: this only generates a linear force. Angular momentum is
	// generated by the hack above.

	Vector vnorm (sp.airvel_ship/sp.airspd);
	double Cl;   // lift coeff

	// === wing lift ===
	if (LiftCoeff && (Cl = LiftCoeff (aoa))) {
		Lift = Cl * sp.dynp * cs.y;    // lift magnitude
		Vector L (0, sp.airvel_ship.z, -sp.airvel_ship.y); // lift direction
		double lnorm = std::hypot (sp.airvel_ship.z, sp.airvel_ship.y);
		if (lnorm) {
			L *= Lift / lnorm;
			Flin_add += L;
		}

		// === induced drag ===
		Drag = Lift * Cl / wingfactor;     // induced drag magnitude

	} else Lift = Drag = 0.0;

	// === parasite drag ===
	// 1. vessel cross section projected into airspeed direction (incorporating also Cw)
	double cw_cs = vd_side*fabs(vnorm.x) + vd_vert*fabs(vnorm.y) + vd_forw*fabs(vnorm.z);
	Drag += cw_cs * sp.dynp;      // parasite drag magnitude
	Flin_add -= vnorm * Drag;     // drag is opposite flight path
}

// =======================================================================
// Forces caused by surface impact/friction
// Supports subsampling at fractional timestep tfrac, for vessel position at pos
// If tfrac==1, the full step is calculated
// Note that no interpolation of rotation states is performed

bool Vessel::AddSurfaceForces (Vector *F, Vector *M, const StateVectors *s, double tfrac, double dt, bool allow_groundcontact) const
{
	nforcevec = 0; // should move higher up
	E_comp = 0.0;  // compression energy

	int i, j;
	double alt = 0, tdymin = 0;
	static int *tidx = new int[3];
	static double *tdy = new double[3];
	static double *fn = new double[3];
	static double *flng = new double[3];
	static double *flat = new double[3];
	static DWORD ntdy = 3;

	static StateVectors ls; // local state
	if (!proxybody) return false;
	StateVectors ps = proxybody->InterpolateState (tfrac); // intermediate planet state; should probably be passed in as function argument
	SurfParam surfp; // intermediate surface parameters; should probably be passed in as function argument

	if (!dt) dt = td.SimDT;

	if (!s) { // full step, based on pre-step vessel position (but new planet position!)
		ls.pos.Set (s0->pos);
		ls.vel.Set (s0->vel);
		ls.Q.Set (s0->Q);
		ls.omega.Set (s0->omega);
		s = &ls;
	} else {    // partial step, based on intermediate position
		// we need to propagate the gravbody backwards, since it has been updated already
	}
	alt = s->pos.dist (ps.pos) - proxybody->Size(); // altitude over normal zero
	if (alt > sp.elev + 1e4) return false; // add safety margin

	surfp.Set (*s, ps, proxybody, &etile, &windp); // intermediate surface parameters
	alt = surfp.alt;
	if (alt > 2.0*size) return false; // no danger of surface contact

	// check for touchdown
	//Matrix T (surfp.L2H);  // transformation vessel local -> horizon
	//T.tpostmul (ps.R);
	//T.postmul (s->R);

	Matrix T (s->R); // transformation vessel local -> planet local
	T.tpremul (ps.R);

	if (ntdy < ntouchdown_vtx) {
		ntdy = ntouchdown_vtx;
		delete []tidx;
		tidx = new int[ntdy];
		delete []tdy;
		tdy = new double[ntdy];
		delete []fn;
		fn = new double[ntdy];
		delete []flng;
		flng = new double[ntdy];
		delete []flat;
		flat = new double[ntdy];
	}

	ElevationManager* emgr = (cbody->Type() == OBJTP_PLANET ? ((Planet*)cbody)->ElevMgr() : 0);
	int reslvl = 1;
	if (emgr) reslvl = (int)(32.0-log(max(alt,100.0))*LOG2);

	Vector shift = tmul(ps.R, s->pos - ps.pos);
	for (i = 0; i < ntouchdown_vtx; i++) {
		Vector p (mul (T, touchdown_vtx[i].pos) + shift);
		double lng, lat, rad, elev = 0.0;
		proxybody->LocalToEquatorial (p, lng, lat, rad);
		if (emgr)
			elev = emgr->Elevation (lat, lng, reslvl, &etile);
		tdy[i] = rad - elev - proxybody->Size();
		if (!i || tdy[i] < tdymin) {
			tdymin = tdy[i];
		}
	}

	if (tdymin >= 0.0) return false;
	if (!allow_groundcontact) return true;

	// wheel brake levels
	double wbrake_level[2];
	for (i = 0; i < 2; i++) {
		wbrake_level[i] = wbrake[i];
		//if (wbrake_override[i]) {
		//	wbrake_level[i] = wbrake_override[i];
		//} else {
		//	wbrake_level[i] = wbrake_permanent[i];
		//}
	}

	// surface effects: friction and wheel brakes
	double gv_lon, gv_lat, gv_n, fmax, mu;
	double massdt = mass/dt;
	double fn_tot_undamped = 0.0, fn_tot = 0.0, flng_tot = 0.0, flat_tot = 0.0;

	// longitudinal touchdown direction - could be precalculated
	Vector d1 ((touchdown_vtx[0].pos-(touchdown_vtx[1].pos+touchdown_vtx[2].pos)*0.5));
	// horizon normal in vessel frame
	//Vector hn (tmul (s->R, s->pos - ps.pos).unit());
	Vector hn (tmul (T, tmul (surfp.L2H, surfp.surfnml)));
	// project d1 (longitudinal touchdown direction) into horizon plane
	Vector d1h ((d1 - hn * dotp (d1, hn)).unit());
	// lateral touchdown direction
	Vector d2h (crossp (hn, d1h));

	if (bDynamicGroundContact) {

//#define NEWCONTACTMODEL
#ifdef NEWCONTACTMODEL // new version - experimental!
		if (tdymin < 0.0) {
			DWORD ntouch = 0;
			double mueff_lng, mueff_lat, gv_hor;
			double dt1 = max(dt, 1.0); // time to stop in case of Haftreibung
			double massdt1 = mass/dt1;
			for (i = 0; i < ntouchdown_vtx; i++) {
				if (tdy[i] < 0.0) {
					tidx[ntouch++] = i;
					Vector gV (surfp.groundvel_ship + crossp(touchdown_vtx[i].pos,s->omega)); // ground velocity of touchdown point in vessel frame
					gv_n   = dotp (gV, hn);                                             // gv projected on horizon normal in vessel frame
					gv_lon = dotp (gV, d1h);											// longitudinal speed component for touchdown point i
					gv_lat = dotp (gV, d2h);											// lateral speed component

					Vector gV_hor = d1h*gv_lon + d2h*gv_lat;
					Vector gV_hor2 = gV - hn*gv_n;  // is this the same?

					gv_hor = hypot(gv_lon,gv_lat);
					Vector gV_hor0 (gV_hor/gv_hor);

					fn[i] = -tdy[i]*touchdown_vtx[i].stiffness; 	// horizon-normal force component: gear compression forces

					if (i < 3 && touchdown_vtx[i].mu_lng != touchdown_vtx[i].mu) {
						flng[i] = fn[i] * touchdown_vtx[i].mu_lng;
						if (gv_lon > 0.0) flng[i] = -flng[i];
					} else
						flng[i] = -gv_lon * fn[i] * touchdown_vtx[i].mu/dt1 * 1e-0;
					flat[i] = -gv_lat * fn[i] * touchdown_vtx[i].mu/dt1 * 1e-0;

					// vertical damping
					fn[i] -= gv_n*touchdown_vtx[i].damping;
					//fn[i] = max (fn[i], 0.0);
					fn_tot += fn[i];

					// debug
					if (nforcevec < forcevecbuf) {
						forcevec[nforcevec] = d1h*flng[i] + d2h*flat[i];
						forcepos[nforcevec] = touchdown_vtx[i].pos;
						nforcevec++;
					}

					flng_tot += flng[i];
					flat_tot += flat[i];
				}
			}

			double fmax = gv_hor * massdt1;
			double fcur = hypot (flng_tot, flat_tot);
			//sprintf (DBG_MSG, "fmax=%le, fcur=%le", fmax, fcur);
			if (fcur > fmax) {
				double scale = fmax/fcur;
				flng_tot *= scale;
				flat_tot *= scale;
				for (i = 0; i < ntouch; i++) {
					j = tidx[i];
					flng[j] *= scale;
					flat[j] *= scale;
				}
			}
			*F += hn*fn_tot + d1h*flng_tot + d2h*flat_tot;

			for (i = 0; i < ntouch; i++) {
				j = tidx[i];
				if (tdy[j] < 0.0) {
					*M += crossp (hn*fn[j]+d1h*flng[j]+d2h*flat[j], touchdown_vtx[j].pos);
				}
			}
		}

#else

		DWORD ntouch = 0;
		for (i = 0; i < ntouchdown_vtx; i++) {
			if (tdy[i] < 0.0) { // ground contact on point i!
				tdy[i] = max(tdy[i], -1.0); // DEBUG
				tidx[ntouch++] = i;
				Vector gv (surfp.groundvel_ship + crossp(touchdown_vtx[i].pos,s->omega)); // ground velocity of touchdown point in vessel frame
				gv_n   = dotp (gv, hn);                                             // gv projected on horizon normal in vessel frame
				gv_lon = dotp (gv, d1h);											// longitudinal speed component for touchdown point i
				gv_lat = dotp (gv, d2h);											// lateral speed component

				fn[i] = -tdy[i]*touchdown_vtx[i].stiffness; 						// horizon-normal force component: gear compression forces
				double maxpress = min (-tdy[i], 0.1)*touchdown_vtx[i].stiffness;

				if (i < 3) {
					mu = touchdown_vtx[i].mu_lng;
					if (i && wbrake_level[i-1])
						mu += (touchdown_vtx[i].mu - touchdown_vtx[i].mu_lng)*wbrake_level[i-1];
				} else {
					mu = touchdown_vtx[i].mu;
				}
				flng[i] = mu * maxpress;												// horizon-longitudinal force component: surface friction
				if (fabs(gv_lon) < 10.0) flng[i] *= sqrt(fabs(0.1*gv_lon));             // cancel horizontal forces at low speeds
				//flng[i] = min (flng[i], fabs(gv_lon) * fn[i] * mu/dt);
				if (gv_lon > 0.0) flng[i] = -flng[i];
				flng_tot += flng[i];

				mu = touchdown_vtx[i].mu;
				flat[i] = mu * maxpress;									// horizon-lateral force component: surface friction
				if (fabs(gv_lat) < 10.0) flat[i] *= sqrt(fabs(0.1*gv_lat));
				//flat[i] = min (flat[i], fabs(gv_lat) * fn[i] * mu/dt);
				if (gv_lat > 0.0) flat[i] = -flat[i];
				flat_tot += flat[i];

				E_comp -= fn[i]*tdy[i]*0.5; // compression energy
				fn_tot_undamped += fn[i];
				fn[i] -= gv_n*touchdown_vtx[i].damping;
				fn_tot += fn[i];

				// debug
				if (nforcevec < forcevecbuf) {
					forcevec[nforcevec] = d1h*flng[i] + d2h*flat[i];
					forcepos[nforcevec] = touchdown_vtx[i].pos;
					nforcevec++;
				}

			} else {
				fn[i] = flng[i] = flat[i] = 0.0;
			}
		}

		// limit forces to avoid velocity reversal
		gv_lon = dotp (surfp.groundvel_ship, d1h);
		gv_lat = dotp (surfp.groundvel_ship, d2h);
		gv_n   = dotp (surfp.groundvel_ship, hn);
		double fmax_lon = -gv_lon * massdt;
		if ((fmax_lon >= 0.0 && flng_tot > fmax_lon) || (fmax_lon <= 0.0 && flng_tot < fmax_lon)) {
			double scale = fmax_lon/flng_tot;
			flng_tot = fmax_lon;
			for (i = 0; i < ntouch; i++)
				flng[tidx[i]] *= scale;
		}
		double fmax_lat = -gv_lat * massdt;
		if ((fmax_lat >= 0.0 && flat_tot > fmax_lat) || (fmax_lat <= 0.0 && flat_tot < fmax_lat)) {
			double scale = fmax_lat/flat_tot;
			flat_tot = fmax_lat;
			for (i = 0; i < ntouch; i++)
				flat[tidx[i]] *= scale;
		}
#ifdef UNDEF
		// limit total energy for vertical compression
		double d_eff = -E/fn_tot_undamped*2.0/ntouch; // effective compression depth
		
		E += 0.5*mass*gv_n*gv_n; // add current linear kinetic energy
		E += 0.5*mass*dotp(s->omega*pmi, s->omega);
		double dv = fn_tot/mass*dt;
		double gv_n_new = gv_n + dv;
		double d_eff_new = d_eff + dv*dt;
		double E_new = (d_eff_new >= 0.0 ? 0.0 : d_eff_new * d_eff_new / d_eff * fn_tot_undamped * ntouch / 2.0);
		E_new += 0.5 * mass * gv_n_new*gv_n_new;
		if (E_new > E) {
			double deltav = sqrt(2.0*(E_new-E)/mass);
			double deltaf = deltav*mass/dt;
			if (gv_n_new > 0.0) deltaf = -deltaf;
			double fn_tot_new = fn_tot+deltaf;
			double scale = fn_tot_new/fn_tot;
			fn_tot = fn_tot_new;
			for (i = 0; i < ntouch; i++)
				fn[tidx[i]] *= scale;
		}
#endif

		//double fmax_nml = -gv_n * massdt;
		//if ((fmax_nml >= 0.0 && fn_tot > fmax_nml) || (fmax_nml <= 0.0 && fn_tot < fmax_nml)) {
		//	double scale = fmax_nml/fn_tot;
		//	fn_tot = fmax_nml;
		//	for (i = 0; i < ntouch; i++)
		//		fn[tidx[i]] *= scale;
		//}

		Vector M_surf, F_surf = hn*fn_tot + d1h*flng_tot + d2h*flat_tot;
		for (i = 0; i < ntouch; i++) {
			j = tidx[i];
			M_surf += crossp (hn*fn[j]+d1h*flng[j]+d2h*flat[j], touchdown_vtx[j].pos);
		}

		// limit the change in angle over the current time step induced by impact forces
		Vector dA = EulerInv_full (M_surf/mass, s->omega)*dt*dt;
		double da = dA.length();
		static double da_max = 0.0;
		if (da > da_max) da_max = da;
		if (da > 10.0*RAD) {
			double scale = 10.0*RAD/da;
			M_surf *= scale;
		}


#ifdef UNDEF
		Vector V0 = surfp.groundvel_ship;
		Vector V1 = V0 + F_surf * (dt/mass);
		double E0_kin_lin = 0.5*mass*dotp(V0,V0);
		double E1_kin_lin = 0.5*mass*dotp(V1,V1);
		double dE_kin = E1_kin_lin-E0_kin_lin;                 // change in linear kinetic energy over time step
		Vector W0 = s->omega;
		Vector W1 = W0 + EulerInv_full (M_surf/mass, s->omega)*dt;
		double E0_kin_rot = 0.5*mass*dotp(W0*pmi, W0);
		double E1_kin_rot = 0.5*mass*dotp(W1*pmi, W1);
		dE_kin += E1_kin_rot-E0_kin_rot;                       // change in rotational kinetic energy over time step
		double dE_comp = E_comp-E0_comp;

		if (dE_kin + dE_comp > 1e5) { // limit kinetic energy
			double E1_kin_rot_fix = max(0, E1_kin_rot - (dE_kin+dE_comp-1e5));
			double alpha = sqrt(E1_kin_rot_fix/E1_kin_rot);
			Vector W1_fix = W1*alpha;
			Vector Wdot = (W1_fix - W0)/dt;
			Vector tau = Euler_full(Wdot, s->omega);
			M_surf = tau*mass;

			// sanity check
			W1 = W0 + EulerInv_full (M_surf/mass, s->omega)*dt;
			dE_kin -= E1_kin_rot;
			E1_kin_rot = 0.5*mass*dotp(W1*pmi, W1);
			dE_kin += E1_kin_rot;
			double de = dE_kin + dE_comp;
		}
#endif

		*F += F_surf;
		*M += M_surf;

		//sprintf (DBG_MSG, "dE_kin=%le, dE_comp=%le", dE_kin, dE_comp);

#endif
	} else {
		double v1, v2;
		for (i = 0; i < 3; i++) {
			if (tdy[i] < 0.0) {
				Vector gv (surfp.groundvel_ship + crossp(touchdown_vtx[i].pos,s->omega)); // ground velocity of touchdown point in vessel frame
				v1 = dotp (gv, d1h); // longitudinal speed component for touchdown point i
				v2 = dotp (gv, d2h); // lateral speed component

				// longitudinal forces
				flng[i] = touchdown_vtx[i].mu_lng * mass * 9.81;    // friction: generalise!
				if (i && wbrake_level[i-1])        // wheel brake
					flng[i] += wbrake_level[i-1]*max_wbrake_F*0.5;
				if (v1 < 0.0) flng[i] = -flng[i];
				flng_tot += flng[i];
				if (fabs(flng[i]) > fabs(v1)*massdt*0.01) // hack: dampen angular component
					flng[i] = v1*massdt*0.01;

				// lateral forces
				flat[i] = touchdown_vtx[i].mu * mass * 9.81;    // friction: generalise!
				flat[i] *= 10.0; // TEMPORARY!!!
				if (v2 < 0.0) flat[i] = -flat[i];
				flat_tot += flat[i];
				if (fabs(flat[i]) > fabs(v2)*massdt*0.01) // hack: dampen angular component
					flat[i] = v2*massdt*0.01;
			}
		}

		// project ground speed vector into horizon plane (vessel frame)
		Vector vh (sp.groundvel_ship - hn * dotp (sp.groundvel_ship, hn));
		v1 = dotp (vh, d1h);
		v2 = dotp (vh, d2h);
		// limit linear force
		fmax = v1*massdt;
		if (flng_tot*fmax < 0.0) {
			flng_tot = 0.0;
		} else if (fabs(flng_tot) > fabs(fmax)) {
			flng_tot = fmax;
		}
		fmax = v2*massdt;
		if (flat_tot*fmax < 0.0) {
			flat_tot = 0.0;
		} else if (fabs(flat_tot) > fabs(fmax)) {
			flat_tot = fmax;
		}
		// apply linear force
		*F -= d1h*flng_tot + d2h*flat_tot;

		// apply angular forces
		for (i = 0; i < 3; i++) {
			if (tdy[i] < 0.0) {
				Vector f_attack(touchdown_vtx[i].pos);
				//f_attack.y = 0.0; // hack to avoid nicking
				Vector F(d1h*flng[i] + d2h*flat[i]);
				*M -= crossp (F, f_attack);
			}
		}
	}



#ifdef UNDEF  // TODO!!!

	// nosewheel steering (experimental)
	if (this == g_focusobj && nosesteering && py[0] < 0.5 && fabs (v1) > 0.2) {

		static const double MAX_NOSE_MOVEMENT = 1.0;
		double newdir = g_pOrbiter->ManCtrlLevel (THGROUP_ATT_YAWRIGHT, MANCTRL_ANYMODE) -
		                g_pOrbiter->ManCtrlLevel (THGROUP_ATT_YAWLEFT, MANCTRL_ANYMODE);
		double ddir = newdir-nosewheeldir;
		if (fabs(ddir)/dt > MAX_NOSE_MOVEMENT) {
			if (ddir > 0.0) newdir = nosewheeldir + MAX_NOSE_MOVEMENT*dt;
			else            newdir = nosewheeldir - MAX_NOSE_MOVEMENT*dt;
		}

		if (newdir) {
			static const double pos_scale = 1.0;
			static const double fac = 5e1/pos_scale;
			static const double vcrit = 10.0;
			double fac2 = min (1.0, v1/vcrit);
			double df = newdir*mass*fac*fac2;
			double f1 = v1*mass/dt;
			double df1 = df*df/(2.0*f1);
			double df2 = sqrt (df*df-df1*df1);
			if (newdir < 0) df2 = -df2;
			if (v1 < 0) df2 = -df2;
			AddForce (d2h*df2 - d1h*df1, (touchdown_pt[0]+touchdown_nm*cog_elev)*pos_scale);
		}
		nosewheeldir = newdir;
	} else nosewheeldir = 0.0;
#endif

	return true;
}

// =======================================================================
// vessel state update

void Vessel::Update (bool force)
{
	// if the vessel is part of a composite structure or passively attached
	// to a parent vessel, its state is updated by the composite or parent
	if (attach)
		return;

	int i;

	if (fstatus == FLIGHTSTATUS_FREEFLIGHT) {

		if (!supervessel) {
			if (bFRplayback) {
				FRecorder_Play();          // update from playback stream
			} else {
				RigidBody::Update (force); // standard dynamic update
			}
		}

	} else if (fstatus == FLIGHTSTATUS_LANDED) {

		// simplified state update for idle vessels
		proxyplanet->LocalToGlobal_t1 (sp.ploc, s1->pos);
		double vground = Pi2 * proxyplanet->Size() * sp.clat / proxyplanet->RotT();
		s1->vel.Set (-vground*sp.slng, 0.0, vground*sp.clng);
		s1->vel.Set (mul (proxyplanet->s1->R, s1->vel) + proxyplanet->s1->vel);
		s1->R.Set (land_rot);
		s1->R.premul (proxyplanet->s1->R);
		s1->Q.Set (s1->R);
		acc = proxyplanet->Acceleration() + (proxyplanet->s1->pos-s1->pos) * (vground * Pi2/(proxyplanet->Size()*proxyplanet->RotT()));
		el_valid = false;

		if (bForceActive) {
			// switch to freeflight whenever the user engages engines
			fstatus = FLIGHTSTATUS_FREEFLIGHT;
			LandingTest.testing = false;

			// this needs thinking
			if (proxybase) proxybase->ReportTakeoff (this);
			if (lstatus == 3 || lstatus == 5) lstatus = 1;

			// need to synchronise state vectors - DO WE NEED THIS?
			rpos_base = s1->pos; rpos_add.Set (0,0,0);
			s1->vel += rvel_add; // MS-071005: rvel_add included to fix "stuck at latitude 90" bug
			rvel_base = s1->vel; rvel_add.Set (0,0,0);
			s1->omega.Set (0,0,0);
		}

		//if (bFRplayback)
		//	FRecorder_Play();

	} else { // should not get here
	}

	// update surface parameters
	if (proxybody && fstatus != FLIGHTSTATUS_LANDED)
		UpdateSurfParams();

	if (proxyplanet && fstatus != FLIGHTSTATUS_LANDED && !bFRplayback) {

		// handle planetary surface touchdown events
		if (sp.alt < 2.0*size) {

			// check for touchdown
			Matrix T (sp.L2H); // T: vessel->horizon rotation matrix
			T.tpostmul (proxyplanet->s1->R);
			T.postmul (s1->R);

			double py, pymin;
			int imin;
			for (i = 0; i < 3; i++) {
				Vector p (mul (T, touchdown_vtx[i].pos));
				py = p.y + sp.alt;                 // vertical position
				if (!i || py < pymin) pymin = py, imin = i;
			}

			if (pymin < 0.0 && !bFRplayback && !bDynamicGroundContact) { // we have (at least partial) touchdown

				// Step 1: rotate vessel towards sitting flat on the ground

				// Calculate compressed touchdown points at equilibrium
				double mg = GetWeight();
				Vector tp_comp[3];
				for (i = 0; i < 3; i++)
					tp_comp[i] = touchdown_vtx[i].pos + touchdown_nm*(touchdown_vtx[i].compression*mg);

				// equilibrium touchdown plane normal
				Vector tnm_comp = crossp (tp_comp[0]-(tp_comp[1]+tp_comp[2])*0.5, tp_comp[2]-tp_comp[1]).unit();

				// horizon normal in vessel frame
				Vector horizon_nm (T.m21, T.m22, T.m23);
				double tilt = acos (dotp (tnm_comp, horizon_nm));

				if (tilt > 1e-10) {
					// rotate rotation axis into vessel x-axis
					Vector raxis (crossp (tnm_comp, horizon_nm).unit());
					double theta = asin (raxis.y);
					double phi   = atan2 (raxis.z, raxis.x);
					double sint  = sin (theta), cost = cos (theta);
					double sinp  = sin (phi), cosp = cos (phi);
					Matrix R (cost*cosp, sint, cost*sinp,  -sint*cosp, cost, -sint*sinp,  -sinp, 0, cosp);
					// rotate around x-axis to compensate tilt
					double arm = touchdown_vtx[imin].pos.dist(touchdown_cg)/size;
					// rotate around x-axis to compensate tilt
					double dtilt = min (tilt, -pymin * arm * 0.03 /*0.005e-1*/);
					double sina = sin(dtilt), cosa = cos(dtilt);
					R.premul (Matrix (1,0,0,  0,cosa,-sina,  0,sina,cosa));
					// rotate rotation axis back
					R.premul (Matrix (cost*cosp, -sint*cosp, -sinp,  sint, cost, 0,  cost*sinp, -sint*sinp, cosp));

					// modify angular velocity
					Vector dv = raxis * (dtilt*td.iSimDT);
					s1->omega -= dv;

					// limit angular velocity when we are flat on the ground
					if (tilt < 5.0*RAD) {
						double maxv = tilt*tilt*50 + 0.1*RAD;
						for (i = 0; i < 3; i++)
							s1->omega.data[i] = min (maxv, max (-maxv, s1->omega.data[i]));
					}

					// update global rotation matrix
					//s1->R.postmul (R);
					//s1->Q.Set (s1->R);

					//T.postmul (R);
				}

				// Update touchdown points for compression
				for (i = 0; i < 3; i++) {
					Vector p (mul (T, tp_comp[i]));
					py = p.y + sp.alt;
					if (!i || py < pymin) pymin = py, imin = i;
				}

				// Step 2: lift vessel altitude so that lowest touchdown point touches the ground
				if (pymin < 0.0) {
					sp.rad -= pymin;
					sp.alt -= pymin;
					Vector loc;
					proxyplanet->EquatorialToLocal (sp.slng, sp.clng, sp.slat, sp.clat, sp.rad, loc);
					Vector pgpos(s1->pos);
					SetRPos (mul (proxyplanet->s1->R, loc) + proxyplanet->s1->pos);
					Vector dv ((s1->pos-pgpos)*td.iSimDT);
					double ay = mul (sp.L2H, tmul (proxyplanet->s1->R, acc)).y;
					if (ay < 0) dv -= mul (proxyplanet->s1->R, tmul(sp.L2H, Vector (0, ay, 0)))*0.5*td.SimDT;
					AddRVel (dv);
				}

				// update surface parameters
				UpdateSurfParams();
			}
		}
	}

	if (td.SimT1 > commsT) {

		// update NAV receivers
		if (nnav) UpdateReceiverStatus ();

		// update landing target comms setup
		if (landtgt && proxyplanet) {
			double blng, blat, adist, bdir, dist;
			landtgt->EquPos (blng, blat);
			Orthodome (sp.lng, sp.lat, blng, blat, adist, bdir);
			dist = adist * landtgt->RefPlanet()->Size();
			if (dist > 1e5) lstatus = 0;
			else if (lstatus == 0) {
				lstatus = 1;
			} else if (lstatus == 2) {
				lstatus = landtgt->RequestLanding (this, nport);
			} else if (lstatus == 3) {
				lstatus = landtgt->RequestTakeoff ();
			}
		} else {
			lstatus = 0;
		}
#ifdef UNDEF
		// update docking target comms setup
		if (docktgt) {
			double dist2;
			dist2 = gpos->dist2 (docktgt->GPos());
			if (dist2 > 1e10) dstatus = 0;
			else if (dstatus == 0) {
				dstatus = 1;
			} else if (dstatus == 2) {
				dstatus = docktgt->RequestDocking (this, nport);
			} else if (dstatus == 3) {
				dstatus = docktgt->RequestUndock ();
			}
		} else {
			dstatus = 0;
		}
#endif
		commsT = td.SimT1 + 2.0 + rand1()*4;
	}

	// state vectors w.r.t. reference body
	cpos = s1->pos - cbody->s1->pos;
 	cvel = s1->vel - cbody->s1->vel;

	// module interface calls
	if (hVis && animcount) {
		if (modIntf.v->Version() >= 1)
			((VESSEL2*)modIntf.v)->clbkAnimate (td.SimT1);
	}

	Flin.Set (Flin_add);    // store current linear force
	Amom.Set (Amom_add);    // store current torque
	Flin_add.Set (0,0,0);   // reset linear force
	Amom_add.Set (0,0,0);   // reset angular moments
	E0_comp = E_comp;       // store compression energy
	//for (i = 0; i < 2; i++) wbrake_override[i] = 0;
	weight_valid = torque_valid = false;
	bForceActive = false;
	UpdateMass(); // register fuel consumption

	// Now update all passively attached child vessels
	UpdateAttachments();
}

void Vessel::UpdatePassive ()
{
	StateVectors *s = (s1 ? s1:s0); // hack - this should really only be called during update phase

	if (!attach) return; // should not happen
	Vessel *prnt = attach->mate;
	AttachmentSpec *pa = attach->mate_attach;
	StateVectors *ps = (s1 ? prnt->s1 : prnt->s0);

	s->pos.Set (mul (ps->R, attach_rpos) + ps->pos);
	rpos_base = s->pos;
	rpos_add.Set (0,0,0);
	s->vel.Set (ps->vel);
	rvel_base = s->vel;
	rvel_add.Set (0,0,0);
	s->R.Set (ps->R);
	s->R.postmul (attach_rrot);
	s->Q.Set (s->R);
	s->omega.Set (tmul (attach_rrot, ps->omega));
	acc.Set (prnt->acc);

	Flin.Set (Flin_add);    // store current linear force
	Amom.Set (Amom_add);    // store current torque
	Flin_add.Set (0,0,0);   // reset linear force
	Amom_add.Set (0,0,0);   // reset angular moments

	proxybody = prnt->proxybody;
	proxyplanet = prnt->proxyplanet;
	fstatus = prnt->fstatus;

	if (proxybody) UpdateSurfParams();
	if (fstatus != FLIGHTSTATUS_LANDED) {
		bSurfaceContact = false;
	}
	UpdateMass(); // register fuel consumption

	// Now recursively update all passively attached child vessels
	UpdateAttachments();
}

void Vessel::UpdateAttachments ()
{
	for (DWORD j = 0; j < ncattach; j++)
		if (cattach[j]->mate) cattach[j]->mate->UpdatePassive ();
}

void Vessel::PostUpdate ()
{
	// Called after all vessels have been updated, but before
	// module clbkPostStep is called for the vessel

	VesselBase::PostUpdate ();

	DWORD j, k;

	if (bFRplayback)
		FRecorder_PlayEvent();

	if (fstatus == FLIGHTSTATUS_FREEFLIGHT && td.SimT1 > undock_t+1.0) {

		// check for vessel-vessel docking
		// check one vessel per frame (this may not be enough if universe is densely
		// populated. Instead there should be a list of vessels, sorted into 2 or 3 distance
		// bins (eg < 10km, <1000km, rest) and 1 vessel per *bin* should be checked)

		if (ndock) {
			if (++scanvessel >= g_psys->nVessel()) scanvessel = 0;
			Vessel *v = g_psys->GetVessel(scanvessel);
			if (v != this && v->ndock && v->proxybody == proxybody) {
				double dst = s0->pos.dist (v->GPos());

				if ((dst < 1.5 * (size + v->Size()) || (dst < size + v->Size() + 1e3))) { // valid candidate
					Vector dref, gref, vref;
					for (j = 0; j < ndock; j++) { // loop over my own docks
						if (dock[j]->mate) continue; // dock already busy
						if (dockmode == 0) { // legacy docking mode
							if (dotp (s0->vel - v->GVel(), mul (s0->R, dock[j]->dir)) < -0.01) continue; // moving away from dock
							for (k = 0; k < v->ndock; k++) { // loop over other vessel's docks
								if (v->dock[k]->mate) continue; // dock already busy
								dref.Set (tmul (v->GRot(), mul (s0->R, dock[j]->ref) + s0->pos - v->GPos()));
								double d = dref.dist (v->dock[k]->ref);
								if (d < MIN_DOCK_DIST) {
									if (dock[j]->autodock && v->dock[k]->autodock)
										Dock (v, j, k);
								}
							}
						} else { // new docking mode
							for (k = 0; k < v->ndock; k++) { // loop over other vessel's docks
								if (v->dock[k]->mate) continue; // dock already busy
								gref.Set (mul (s0->R, dock[j]->ref) + s0->pos);            // my dock in global frame
								vref.Set (mul (v->GRot(), v->dock[k]->ref) + v->GPos()); // target dock in global frame
								//dref.Set (tmul (v->GRot(), mul (*grot, dock[j]->ref) + *gpos - v->GPos())); // my dock in the target's frame
								double d = gref.dist(vref); //dref.dist (v->dock[k]->ref);
								if (d < MIN_DOCK_DIST) {
									if (dotp (s0->vel - v->GVel(), vref-gref) >= 0) { // on approach
										dock[j]->pending = v;
									} else if (dock[j]->pending == v) {
										if (dock[j]->autodock && v->dock[k]->autodock)
											Dock (v, j, k);
									}
								}
							}
						}
					}
					// update information about closest dock in range of our dock 0
					if (closedock.vessel && closedock.vessel->ndock && closedock.dock < closedock.vessel->ndock) {
						dref.Set (tmul (closedock.vessel->GRot(), mul (s0->R, dock[0]->ref) + s0->pos - closedock.vessel->GPos()));
						closedock.dist = dref.dist (closedock.vessel->dock[closedock.dock]->ref);
					} else {
						closedock.dist = 1e50;
					}
					for (k = 0; k < v->ndock; k++) {
						if (v->dock[k]->mate) continue;
						dref.Set (tmul (v->GRot(), mul (s0->R, dock[0]->ref) + s0->pos - v->GPos()));
						double d = dref.dist (v->dock[k]->ref);
						if (d < closedock.dist) {
							closedock.dist = d;
							closedock.vessel = v;
							closedock.dock = k;
						}
					}
				}
			}
		}
	}

	// flight recorder
	if (bFRrecord) FRecorder_Save();
	//else if (bFRplayback) FRecorder_Play();
}

bool Vessel::CheckSurfaceContact () const
{
	if (!proxybody) return false; // sanity check
	double alt = Altitude();
	if (alt > 2.0*size) return false;
	Matrix T (sp.L2H);
	T.tpostmul (proxybody->s0->R);
	T.postmul (s0->R);
	for (DWORD i = 0; i < ntouchdown_vtx; i++) {
		Vector p (mul (T, touchdown_vtx[i].pos));
		if (p.y + alt < 0.0) return true;
	}
	return false;
}

void Vessel::Timejump (double dt, int mode)
{
	if (supervessel && supervessel->GetVessel(0) != this) return;
	// let the supervessel deal with the jump

	if (fstatus == FLIGHTSTATUS_FREEFLIGHT) {

		el->Calculate (cpos, cvel, td.SimT0);
		if (el->PeDist() > cbody->Size()) // orbital
			mode = mode & PROP_ORBITAL;
		else                              // suborbital
			mode = mode & PROP_SORBITAL;

		switch (mode) {
		case PROP_ORBITAL_ELEMENTS:
			el->PosVel (cpos, cvel, td.SimT0+g_pOrbiter->tjump.dt);
			// fall through
		case PROP_ORBITAL_FIXEDSTATE:
		case PROP_SORBITAL_FIXEDSTATE:
			RPlace (cpos + cbody->GPos(), cvel + cbody->GVel());
			break;
		case PROP_ORBITAL_FIXEDSURF:
		case PROP_SORBITAL_FIXEDSURF: {
			double dphi = Pi2*dt/proxyplanet->RotT(), cphi = cos(dphi), sphi = sin(dphi);
			Matrix Rb (cphi,0,-sphi,  0,1,0,  sphi,0,cphi);
			Vector cp = mul (proxyplanet->GRot(), mul (Rb, tmul (proxyplanet->GRot(), cpos)));
			Vector cv = mul (proxyplanet->GRot(), mul (Rb, tmul (proxyplanet->GRot(), cvel)));
			s0->R.tpremul (proxyplanet->GRot());
			s0->R.premul (Rb);
			s0->R.premul (proxyplanet->GRot());
			s0->Q.Set (s0->R);
			RPlace (cp + cbody->GPos(), cv + cbody->GVel());
			if (supervessel) supervessel->SetRotationMatrix (s0->R, this);
			} break;
		case PROP_SORBITAL_DESTROY:
			RequestDestruct();
			if (supervessel)
				for (int i = 1; i < supervessel->nVessel(); i++)
					supervessel->GetVessel(i)->RequestDestruct();
			break;
		}

	} else if (fstatus == FLIGHTSTATUS_LANDED) {

		proxyplanet->LocalToGlobal (sp.ploc, s0->pos);
		double vground = Pi2 * proxyplanet->Size() * sp.clat / proxyplanet->RotT();
		s0->vel.Set (-vground*sp.slng, 0.0, vground*sp.clng);
		s0->vel.Set (mul (proxyplanet->GRot(), s0->vel) + proxyplanet->GVel());
		s0->R.Set (land_rot);
		s0->R.premul (proxyplanet->GRot());
		s0->Q.Set (s0->R);

	}
	undock_t = td.SimT0-1000;
}

void Vessel::ModulePreStep (double t, double dt, double mjd)
{
	if (modIntf.v->Version() >= 1)
		((VESSEL2*)modIntf.v)->clbkPreStep (t, dt, mjd);
}

void Vessel::ModulePostStep (double t, double dt, double mjd)
{
	if (modIntf.v->Version() >= 1)
		((VESSEL2*)modIntf.v)->clbkPostStep (t, dt, mjd);
}

void Vessel::ModuleSignalRCSmode (int mode)
{
	if (modIntf.v->Version() >= 1)
		((VESSEL2*)modIntf.v)->clbkRCSMode (mode);
}

void Vessel::ModuleSignalADCtrlmode (DWORD mode)
{
	if (modIntf.v->Version() >= 1)
		((VESSEL2*)modIntf.v)->clbkADCtrlMode (mode);
}

void Vessel::ModuleSignalNavmode (int mode, bool active)
{
	if (modIntf.v->Version() >= 1)
		((VESSEL2*)modIntf.v)->clbkNavMode (mode, active);
}

void Vessel::DrawHUD (HUD *hud, oapi::Sketchpad *skp)
{
	if (modIntf.v->Version() >= 1) {
		bool drawn = false;
		if (modIntf.v->Version() >= 2) {
			drawn = ((VESSEL3*)modIntf.v)->clbkDrawHUD (hud->Mode(), hud->PaintSpec(), skp);
		}
	} else
		hud->DrawDefault (skp);
}

void Vessel::RenderHUD (HUD *hud, int mode, HUDPAINTSPEC *spec, SURFHANDLE hTex)
{
	if (modIntf.v->Version() >= 2)
		((VESSEL3*)modIntf.v)->clbkRenderHUD (mode, spec, hTex);
	else
		hud->RenderDefault ();
}

void Vessel::HUDchanged (int mode)
{
	if (modIntf.v->Version() >= 1)
		((VESSEL2*)modIntf.v)->clbkHUDMode (mode);
}

void Vessel::MFDchanged (int mfd, int mode) const
{
	if (modIntf.v->Version() >= 1)
		((VESSEL2*)modIntf.v)->clbkMFDMode (mfd, mode);
}

void Vessel::UpdateProxies ()
{
	VesselBase::UpdateProxies ();

	int i;
	double dist2, proxydist2;

	// check for closest vessel
	proxyvessel = 0;
	for (i = g_psys->nVessel()-1, proxydist2 = 1e100; i >= 0; i--) {
		Vessel *vessel = g_psys->GetVessel(i);
		if (vessel == this) continue;
		if ((dist2 = s0->pos.dist2 (vessel->GPos())) < proxydist2) {
			proxydist2 = dist2;
			proxyvessel = vessel;
		}
	}
}

void Vessel::UpdateReceiverStatus (DWORD idx)
{
	if (!proxyplanet) return;

	int i, j;
	DWORD m, n, n0, n1, nn, r0, r1, rm, step;
	double dist2, sig;

	static DWORD nscan = 1;
	static double *navsig = new double[nscan];
	if (nnav > nscan) {
		delete []navsig;
		navsig = new double[nscan = nnav]; TRACENEW
	}
	if (idx < nnav) n0 = idx, n1 = idx + 1;
	else            n0 = 0, n1 = nnav;

	for (n = n0; n < n1; n++) {
		navsig[n] = 0.0;
		nav[n].sender = NULL;
	}

	// scan for NAV radio signals
	if (nn = proxyplanet->nNav()) { // surface-based transmitters
		Nav **nlist = proxyplanet->NavMgr().GetNavlist();
		for (n = n0; n < n1; n++) {
			step = nav[n].step;
			if (nav[n].dbidx < 0) { // search data base for current frequency
				r0 = -1; r1 = nn; // list is sorted, so do a binary search
				while (r1-r0 > 1) {
					rm = (r0+r1)/2;
					if (nlist[rm]->GetStep() < step) r0 = rm;
					else r1 = rm;
				}
				nav[n].dbidx = ++r0;
			} else r0 = nav[n].dbidx;
			for (; r0 < nn && nlist[r0]->GetStep() == step; r0++) {
				sig = nlist[r0]->FieldStrength (s0->pos);
				if (sig > 0.9 && sig > navsig[n]) {
					navsig[n] = sig;
					nav[n].sender = nlist[r0]; break;
				}
			}
		}
	}

	// scan surface-base related signal transmitters
	for (i = 0; i < proxyplanet->nBase(); i++) {
		Base *base = proxyplanet->GetBase(i);
		if ((s0->pos.dist2 (base->GPos()) < 1e12) && (nn = base->nNav())) {
			for (n = 0; n < nn; n++) {
				const Nav *navsend = base->NavMgr().GetNav (n);
				for (m = n0; m < n1; m++) {
					if (navsend->GetStep() == nav[m].step) { // && navsend->InRange (*gpos))
						sig = navsend->FieldStrength (s0->pos);
						if (sig > 0.9 && sig > navsig[m]) {
							navsig[m] = sig;
							nav[m].sender = navsend;
						}
					}
				}
			}
		}
	}

	// scan for vessel-mounted XPDR and IDS transmitters
	for (i = g_psys->nVessel()-1; i >= 0; i--) {
		Vessel *vessel = g_psys->GetVessel(i);
		if (vessel == this) continue;
		if ((dist2 = s0->pos.dist2 (vessel->GPos())) < 1e12) { // max XPDR range 1000 km

			for (n = n0; n < n1; n++) {
				if (vessel->xpdr && vessel->xpdr->GetStep() == nav[n].step) {
					sig = vessel->xpdr->FieldStrength (s0->pos);
					if (sig > 0.9 && sig > navsig[n]) {
						navsig[n] =  sig;
						nav[n].sender = vessel->xpdr;
					}
				}
			}

			if (dist2 < 1e10) { // max IDS range 100 km
				for (j = (int)vessel->nDock()-1; j >= 0; j--) {
					const PortSpec *ps = vessel->GetDockParams (j);
					if (ps->ids) {
						for (n = n0; n < n1; n++)
							if (ps->ids->GetStep() == nav[n].step) {
								sig = ps->ids->FieldStrength (s0->pos);
								if (sig > 0.9 && sig > navsig[n]) {
									navsig[n] = sig;
									nav[n].sender = ps->ids;
								}
							}
					}
				}
			}
		}
	}

	// if the signal strength is right at the edge, drop it intermittently
	for (n = n0; n < n1; n++) {
		if (navsig[n] < 1.1) {
			double p = (navsig[n] - 0.9) / 0.2; // signal probability: linear from strength 0.9 to 1.1
			if (rand1() > p)
				nav[n].sender = NULL; // drop signal
		}
	}
}

double Vessel::IlluminationFactor () const
{
	if (td.SimT1 >= lightfac_T0 && td.SimT1 <= lightfac_T1)
		return lightfac;             // current estimate still valid

	double updt = 10.0;              // default update interval
	lightfac_T0 = td.SimT1;
	lightfac = 1.0;

	const CelestialBody *cb = proxyplanet;
	Star *sun = g_psys->GetStar(0);   // should really loop over all suns
	if (!cb || !sun) {                // sanity check
		lightfac_T1 = lightfac_T0+updt;
		return lightfac;
	}
	Vector S(sun->GPos() - s0->pos);
	double s = S.length();
	double as = asin (sun->Size()/s);
	int i;

	for (i = 0;; i++) {
		Vector P(cb->GPos() - s0->pos);
		double p = P.length();
		if (p < s) {   // shadow only if planet closer than sun
			double phi = acos (dotp(S,P)/(s*p));
			double ap  = asin (cb->Size()/p);

			// for now disregard atmospheric effects
			if (ap/as > 0.1) {                 // significant planet size
				if (phi < as+ap) {             // overlap
					double lfrac;
					if (as < ap) {                 // planet disc larger than sun disc
						if (phi <= ap-as)          // totality
							lfrac = 0.0;
						else {                     // partial cover
							lfrac = (phi+as-ap)/(2.0*as);
						}
					} else {                       // sun disc larger than planet disc
						double maxcover = ap*ap / (as*as);
						if (phi < as-ap) {         // annularity
							lfrac = 1.0-maxcover;
						} else {
							lfrac = 1.0 - 0.5*maxcover * (1.0 + (as-phi)/ap);
						}
					}
					lightfac = min (lightfac, lfrac);
				}

				// check edge proximity, increase update frequency
				if (as < ap) {
					if (phi < ap+2.0*as && phi > ap-2.0*as) updt = 1.0;
				} else {
					if (phi < as+2.0*ap && phi > as-2.0*ap) updt = 1.0;
				}
			}
		}
		if (cb->isMoon()) cb = cb->ElRef();
		else break;
	}
	lightfac_T1 = lightfac_T0 + updt;
	return lightfac;
}

bool Vessel::AtmPressureAndDensity (double &p, double &rho) const
{
	p = sp.atmp, rho = sp.atmrho;
	return sp.is_in_atm;
}

void Vessel::IssueClearanceRequest () {
	if (!lstatus || !landtgt) return;
	if (lstatus != 1) {
		if (lstatus == 2 || lstatus == 4) landtgt->ClearPad (this);
		lstatus = 1; // cancel request
	} else {
		lstatus = (fstatus == FLIGHTSTATUS_LANDED ? 3:2);
	}
}

void Vessel::Refuel ()
{
	for (DWORD i = 0; i < ntank; i++)
		tank[i]->mass = tank[i]->maxmass;
	UpdateMass();
}

void Vessel::EngineStatus (ENGINESTATUS *es)
{
	es->main = GetThrusterGroupLevel (THGROUP_MAIN);
	if (!es->main) es->main = -GetThrusterGroupLevel (THGROUP_RETRO);
	es->hover = GetThrusterGroupLevel (THGROUP_HOVER);
	es->attmode = attmode;
}

void Vessel::IncTrim (AIRCTRL_TYPE ctrl)
{
	SetControlSurfaceLevel (ctrl, min (1.0, ctrlsurf_level[ctrl].ptgt + td.SimDT * 0.25),false);
}

void Vessel::DecTrim (AIRCTRL_TYPE ctrl)
{
	SetControlSurfaceLevel (ctrl, max (-1.0, ctrlsurf_level[ctrl].ptgt - td.SimDT * 0.25),false);
}

int Vessel::RegisterMFDMode (const MFDMODESPECEX *spec)
{
	// note: should check for mode already present
	MFDMODE *tmp = new MFDMODE[nmfdmode+1];
	if (nmfdmode) {
		memcpy (tmp, mfdmode, nmfdmode*sizeof(MFDMODE));
		delete []mfdmode;
	}
	mfdmode = tmp;
	mfdmode[nmfdmode].spec = new MFDMODESPECEX;
	mfdmode[nmfdmode].oldspec = new MFDMODESPEC;
	memcpy (mfdmode[nmfdmode].spec, spec, sizeof(MFDMODESPECEX));
	mfdmode[nmfdmode].spec->name = new char[strlen(spec->name)+1];
	strcpy (mfdmode[nmfdmode].spec->name, spec->name);
	mfdmode[nmfdmode].id = nmfdmode+1000;

	// check for duplicate key codes and disable if required
	if (spec->key) {
		DWORD i, k;
		for (i = 0; i < nmfdmode; i++)
			if (mfdmode[i].spec->key == spec->key) break;
		if (i < nmfdmode) { // key already assigned
			mfdmode[nmfdmode].spec->key = 0;
			for (k = OAPI_KEY_1; k <= OAPI_KEY_0; k++) {
				for (i = 0; i < nmfdmode; i++)
					if (mfdmode[i].spec->key == k) break;
				if (i == nmfdmode) { // found un-assigned key
					mfdmode[nmfdmode].spec->key = k;
					break;
				}
			}
		}
	}

	mfdmode[nmfdmode].oldspec->key = mfdmode[nmfdmode].spec->key;
	mfdmode[nmfdmode].oldspec->name = mfdmode[nmfdmode].spec->name;
	mfdmode[nmfdmode].oldspec->msgproc = mfdmode[nmfdmode].spec->msgproc;

	return mfdmode[nmfdmode++].id;
}

bool Vessel::UnregisterMFDMode (int id)
{
	DWORD i, j, k;
	for (i = 0; i < nmfdmode; i++)
		if (mfdmode[i].id == id) break;
	if (i == nmfdmode) return false;

	delete []mfdmode[i].spec->name;
	mfdmode[i].spec->name = NULL;
	delete mfdmode[i].spec;
	delete mfdmode[i].oldspec;
	MFDMODE *tmp = NULL;
	if (nmfdmode > 1) {
		tmp = new MFDMODE[nmfdmode-1];
		for (j = k = 0; j < nmfdmode; j++)
			if (j != i) {
				tmp[k].spec = mfdmode[j].spec;
				tmp[k].oldspec = mfdmode[j].oldspec;
				tmp[k].id = mfdmode[j].id;
				k++;
			}
	}
	delete []mfdmode;
	mfdmode = tmp;
	nmfdmode--;
	return true;
}

void Vessel::UnregisterMFDModes ()
{
	while (nmfdmode)
		UnregisterMFDMode (mfdmode[0].id);
}

void Vessel::SetWBrakeLevel (double level, int which, bool permanent)
{
	if (!which) which = 3;
	if (permanent) {
		if (which & 1) wbrake_permanent[0] = level;
		if (which & 2) wbrake_permanent[1] = level;
	} else {
		if (which & 1) wbrake_override[0] = level;
		if (which & 2) wbrake_override[1] = level;
	}
}

bool Vessel::SetAttMode (int mode, bool fromstream)
{
	if (bFRplayback && !fromstream) return false;
	// ignore manual SetAttMode during playback

	if (mode != attmode) {
		attmode = mode;
		ModuleSignalRCSmode (attmode);
		FRecorder_SaveEventInt ("RCSMODE", attmode);
		return true;
	}
	return false;
}

int Vessel::ToggleAttMode (void)
{
	if (bFRplayback) return attmode;
	// ignore manual SetAttMode during playback

	if (attmode >= 1) {
		attmode ^= 3;
		ModuleSignalRCSmode (attmode);
		FRecorder_SaveEventInt ("RCSMODE", attmode);
	}
	return attmode;
}

DWORD Vessel::ToggleADCtrlMode ()
{
	if (!bFRplayback) {
		ctrlsurfmode = (ctrlsurfmode ? 0 : 7);
		ModuleSignalADCtrlmode (ctrlsurfmode);
	}
	return ctrlsurfmode;
}

void Vessel::SetADCtrlMode (DWORD mode, bool fromstream)
{
	if (bFRplayback && !fromstream) return;
	// ignore manual SetAttMode during playback

	if (mode != ctrlsurfmode) {
		ctrlsurfmode = mode;
		ModuleSignalADCtrlmode (ctrlsurfmode);
		FRecorder_SaveEventInt ("ADCMODE", ctrlsurfmode);
	}
}

void Vessel::SetCameraShiftRange (const VECTOR3 &fwd, const VECTOR3 &left, const VECTOR3 &right)
{
	SetCameraMovement (fwd, 0, 0, left, 60.0*RAD, 0, right, -60.0*RAD, 0);
}

void Vessel::SetCameraMovement (const VECTOR3 &fwdpos, double fwdphi, double fwdtht,
	const VECTOR3 &lpos, double lphi, double ltht,
	const VECTOR3 &rpos, double rphi, double rtht)
{
	camfwd.pos.Set (fwdpos.x, fwdpos.y, fwdpos.z);   camfwd.phi   = fwdphi; camfwd.tht = fwdtht;
	camleft.pos.Set (lpos.x, lpos.y, lpos.z);  camleft.phi  = lphi; camleft.tht = ltht;
	camright.pos.Set (rpos.x, rpos.y, rpos.z); camright.phi = rphi; camright.tht = rtht;
}

void Vessel::UnsetCameraMovement ()
{
	camfwd.pos.Set (0,0,0);   camfwd.phi = camfwd.tht = 0;
	camleft.pos.Set (0,0,0);  camleft.phi = camleft.tht = 0;
	camright.pos.Set (0,0,0); camright.phi = camright.tht = 0;
}

void Vessel::RegisterVisual (VISHANDLE vis)
{
	if (!vis) return; // sanity check

	RigidBody::RegisterVisual (vis);

	if (modIntf.v && modIntf.v->Version() >= 1)
		((VESSEL2*)modIntf.v)->clbkVisualCreated (vis, 1);
	g_pane->RedrawCockpitAreas (PANEL_REDRAW_INIT);
}

void Vessel::UnregisterVisual ()
{
	if (!hVis) return; // nothing to do

	if (modIntf.v && modIntf.v->Version() >= 1)
		((VESSEL2*)modIntf.v)->clbkVisualDestroyed (hVis, 1);

	RigidBody::UnregisterVisual();
}

bool Vessel::SetGenericCockpit () const
{
	if (modIntf.v && modIntf.v->Version() >= 1)
		return ((VESSEL2*)modIntf.v)->clbkLoadGenericCockpit();
	else
		return true;
}

bool Vessel::LoadPanel (int which) const
{
	if (modIntf.v && modIntf.v->Version() >= 1)
		return ((VESSEL2*)modIntf.v)->clbkLoadPanel (which);
	else
		return false;
}

bool Vessel::LoadPanel2D (int which, Panel2D *panel) const
{
	if (modIntf.v && modIntf.v->Version() >= 2) {
		oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
		if (gc) {
			DWORD viewW, viewH;
			gc->clbkGetViewportSize (&viewW, &viewH);
			return ((VESSEL3*)modIntf.v)->clbkLoadPanel2D (which, (PANELHANDLE)panel, viewW, viewH);
		}
	}
	return false;
}

bool Vessel::PanelRedrawEvent (int id, int event, SURFHANDLE surf, void *context) const
{
	if (modIntf.v && modIntf.v->Version() >= 2 && ((VESSEL3*)modIntf.v)->clbkPanelRedrawEvent (id, event, surf, context))
		return true;
	else if (modIntf.v && modIntf.v->Version() >= 1)
		return ((VESSEL2*)modIntf.v)->clbkPanelRedrawEvent (id, event, surf);
	else
		return false;
}

bool Vessel::PanelMouseEvent (int id, int event, int mx, int my, void *context) const
{
	if (modIntf.v && modIntf.v->Version() >= 2 && ((VESSEL3*)modIntf.v)->clbkPanelMouseEvent (id, event, mx, my, context))
		return true;
	else if (modIntf.v && modIntf.v->Version() >= 1)
		return ((VESSEL2*)modIntf.v)->clbkPanelMouseEvent (id, event, mx, my);
	else
		return false;
}

bool Vessel::LoadVC (int id) const
{
	if (modIntf.v && modIntf.v->Version() >= 1)
		return ((VESSEL2*)modIntf.v)->clbkLoadVC (id);
	else
		return false;
}

bool Vessel::VCRedrawEvent (int id, int event, SURFHANDLE surf) const
{
	if (modIntf.v && modIntf.v->Version() >= 1)
		return ((VESSEL2*)modIntf.v)->clbkVCRedrawEvent (id, event, surf);
	else
		return false;
}

bool Vessel::VCMouseEvent (int id, int event, Vector &p) const
{
	if (modIntf.v && modIntf.v->Version() >= 1) {
		VECTOR3 v = _V(p.x,p.y,p.z);
		return ((VESSEL2*)modIntf.v)->clbkVCMouseEvent (id, event, v);
	}
	else
		return false;
}

void Vessel::LeanCamera (int dir, bool smooth)
{
	switch (dir) {
	case 0: // default position
		if (smooth) g_camera->MoveTo (Vector(0,0,0));
		else        g_camera->MoveToDirect (Vector(0,0,0));
		g_camera->ResetCockpitDir (smooth);
		break;
	case 1: // forward
		if (smooth) g_camera->MoveTo (camfwd.pos);
		else        g_camera->MoveToDirect (camfwd.pos);
		g_camera->ResetCockpitDir(camfwd.phi, camfwd.tht, smooth);
		break;
	case 2: // left
		if (smooth) g_camera->MoveTo (camleft.pos);
		else        g_camera->MoveToDirect (camleft.pos);
		g_camera->ResetCockpitDir(camleft.phi, camleft.tht, smooth);
		//g_camera->ResetCockpitDir(60*RAD, 0/*g_camera->ctheta0*/);
		break;
	case 3: // right
		if (smooth) g_camera->MoveTo (camright.pos);
		else        g_camera->MoveToDirect (camright.pos);
		g_camera->ResetCockpitDir(camright.phi, camright.tht, smooth);
		//g_camera->ResetCockpitDir(-60*RAD, 0/*g_camera->ctheta0*/);
		break;
	}
}

UINT Vessel::AddAnimSeq (double defmeshstate)
{
	ANIMSEQ *tmp = new ANIMSEQ[nanimseq+1]; TRACENEW
	if (nanimseq) {
		memcpy (tmp, animseq, nanimseq*sizeof(ANIMSEQ));
		delete []animseq;
	}
	animseq = tmp;
	animseq[nanimseq].defstate = defmeshstate;
	animseq[nanimseq].state    = defmeshstate;
	animseq[nanimseq].ncomp    = 0;
	return nanimseq++;
}

bool Vessel::AddAnimSeqComp (UINT seq, ANIMCOMP *comp)
{
	if (seq >= nanimseq) return false;
	ANIMSEQ *s = animseq+seq;
	UINT ncomp = s->ncomp;
	ANIMCOMP **tmp = new ANIMCOMP*[ncomp+1]; TRACENEW
	if (ncomp) {
		memcpy (tmp, s->comp, ncomp*sizeof(ANIMCOMP*));
		delete []s->comp;
	}
	s->comp = tmp;
	s->comp[ncomp] = comp;
	s->ncomp++;
	return true;
}

bool Vessel::SetAnimState (UINT seq, double state)
{
	if (seq >= nanimseq) return false;
	animseq[seq].state = state;
	return true;
}

void Vessel::ClearAnimSeqs (void)
{
	UINT i;
	for (i = 0; i < nanimseq; i++)
		if (animseq[i].ncomp) {
			delete []animseq[i].comp;
			animseq[i].comp = NULL;
		}
	if (nanimseq) {
		delete []animseq;
		animseq = NULL;
	}
	nanimseq = 0;
}

UINT Vessel::CreateAnimation (double initial_state)
{
	ANIMATION *tmp = new ANIMATION[nanim+1]; TRACENEW
	if (nanim) {
		memcpy (tmp, anim, nanim*sizeof(ANIMATION));
		delete []anim;
	}
	anim = tmp;
	anim[nanim].defstate = initial_state;
	anim[nanim].state    = initial_state;
	anim[nanim].ncomp    = 0;
	BroadcastVisMsg (EVENT_VESSEL_NEWANIM, nanim);
	return nanim++;
}

ANIMATIONCOMP *Vessel::AddAnimationComponent (UINT an, double state0, double state1,
	MGROUP_TRANSFORM *trans, ANIMATIONCOMP *parent)
{
	if (an >= nanim) return 0;
	ANIMATION *A = anim+an;
	UINT ncomp = A->ncomp;
	ANIMATIONCOMP **tmp = new ANIMATIONCOMP*[ncomp+1]; TRACENEW
	if (ncomp) {
		memcpy (tmp, A->comp, ncomp*sizeof(ANIMATIONCOMP*));
		delete A->comp;
	}
	A->comp = tmp;
	ANIMATIONCOMP *ac = new ANIMATIONCOMP; TRACENEW
	ac->state0     = state0;
	ac->state1     = state1;
	ac->trans      = trans;
	ac->parent     = parent;
	ac->children   = 0;
	ac->nchildren  = 0;
	A->comp[ncomp] = ac;

	if (parent) {
		ANIMATIONCOMP **ch = new ANIMATIONCOMP*[parent->nchildren+1]; TRACENEW
		if (parent->nchildren) {
			memcpy (ch, parent->children, parent->nchildren*sizeof(ANIMATIONCOMP*));
			delete []parent->children;
		}
		parent->children = ch;
		parent->children[parent->nchildren++] = ac;
	}
	A->ncomp++;
	return ac;
}

bool Vessel::DelAnimationComponent (UINT an, ANIMATIONCOMP *comp)
{
	if (an >= nanim) return false;
	ANIMATION *A = anim+an;
	UINT i, j, k, ncomp = A->ncomp;

	for (i = 0; i < ncomp; i++)
		if (A->comp[i] == comp) break;
	if (i == ncomp) return false; // not found

	// delete all children of comp
	while (comp->nchildren) {
		if (comp->children && DelAnimationComponent (an, comp->children[0])) continue;
		// If the animation component could not be found in animation an, search the complete list of animations
		// Note: it would be better if each animation component had a reference to its animation object
		for (i = 0; i < nanim; i++)
			if (comp->children && DelAnimationComponent (i, comp->children[0])) break;
		if (i == nanim) {
			// Problem: component not found in any animation object - THIS SHOULD NOT HAPPEN!
			// We just remove the child reference from the list
			ANIMATIONCOMP **ch = 0;
			if (comp->nchildren > 1) {
				ch = new ANIMATIONCOMP*[comp->nchildren-1]; TRACENEW
				for (j = 1; j < comp->nchildren; j++)
					if (comp->children) ch[j-1] = comp->children[j];
			}
			delete []comp->children;
			comp->children = ch;
			comp->nchildren--;
		}
	}

	// remove from parent's child list
	if (comp->parent) {
		ANIMATIONCOMP **ch = 0;
		if (comp->parent->nchildren > 1) {
			ch = new ANIMATIONCOMP*[comp->parent->nchildren-1]; TRACENEW
			for (j = k = 0; j < comp->parent->nchildren; j++)
				if (comp->parent->children[j] != comp) ch[k++] = comp->parent->children[j];
		}
		delete []comp->parent->children;
		comp->parent->children = ch;
		comp->parent->nchildren--;
	}

	// remove from animation list
	ANIMATIONCOMP **tmp = 0;
	if (ncomp > 1) {
		tmp = new ANIMATIONCOMP*[ncomp-1]; TRACENEW
		for (j = k = 0; j < ncomp; j++)
			if (j != i) tmp[k++] = A->comp[j];
	}
	delete []A->comp;
	A->comp = tmp;
	A->ncomp--;

	// delete component itself
	delete comp;

	return true;
}

bool Vessel::SetAnimation (UINT an, double state)
{
	if (an >= nanim) return false;
	anim[an].state = state;
	return true;
}

double Vessel::GetAnimation (UINT an)
{
	return (an < nanim ? anim[an].state : -1.0);
}

bool Vessel::DelAnimation (UINT an)
{
	// This clears the animation, but doesn't actually deallocate it
	// to avoid corrupting other animation ids.
	if (an >= nanim) return false;
	ANIMATION &A = anim[an];
	while (A.ncomp)
		DelAnimationComponent (an, A.comp[0]);
#ifdef UNDEF
	if (A.ncomp) {
		BroadcastVisMsg (EVENT_VESSEL_DELANIM, an); // reset animation on visuals
		for (UINT j = 0; j < A.ncomp; j++) {
			if (A.comp[j]->nchildren) {
				for (UINT k = 0; k < A.comp[j]->nchildren; k++)
					delete A.comp[j]->children[k];
				delete []A.comp[j]->children;
				A.comp[j]->children = NULL;
			}
			delete A.comp[j];
		}
		delete []A.comp;
		A.comp = NULL;
		A.ncomp = 0;
	}
#endif
	A.state = A.defstate = 0;
	return true;
}

void Vessel::ClearAnimations (bool reset)
{
	BroadcastVisMsg (EVENT_VESSEL_CLEARANIM, (UINT)reset); // clear animations on visuals

	for (UINT i = 0; i < nanim; i++) {
		if (anim[i].ncomp && anim[i].comp != NULL) {
			for (UINT j = 0; j < anim[i].ncomp; j++) {
				if (anim[i].comp[j]->nchildren) {
					//for (UINT k = 0; k < anim[i].comp[j]->nchildren; k++)
					//	delete anim[i].comp[j]->children[k];
					delete []anim[i].comp[j]->children;
					anim[i].comp[j]->children = NULL;
				}
				delete anim[i].comp[j];
			}
			delete []anim[i].comp;
			anim[i].comp = NULL;
		}
	}
	if (nanim) {
		delete []anim;
		anim = NULL;
	}
	nanim = 0;
}

bool Vessel::LoadModule (ifstream &classf)
{
	char cbuf[256];
	bool found;
	hMod = 0;
	ClearModule();
	modIntf.v = 0;
	modIntf.coreCreated = false;
	flightmodel = g_pOrbiter->Cfg()->CfgLogicPrm.FlightModelLevel;
	if (found = GetItemString (classf, "Module", cbuf)) {
		found = RegisterModule (cbuf);
		if (!found) {
			DWORD code = GetLastError();
			char errbuf[256];
			sprintf(errbuf, "Could not load vessel module: %s (code %d)", cbuf, code);
			LOGOUT_ERR (errbuf);
		}
		if (modIntf.ovcInit)
			modIntf.v = modIntf.ovcInit ((OBJHANDLE)this, flightmodel);
	}
	if (!modIntf.v) { // Problem: module didn't create a VESSEL instance!
		modIntf.v = new VESSEL ((OBJHANDLE)this, flightmodel); TRACENEW
		modIntf.coreCreated = true;
	}
	return found;
}

bool Vessel::RegisterModule (const char *dllname)
{
	char cbuf[256];
	sprintf (cbuf, "Modules\\%s.dll", dllname);
	hMod = LoadLibrary (cbuf);
	if (!hMod)
		return false;

	// retrieve module version
	int (*fversion)() = (int(*)())GetProcAddress (hMod, "GetModuleVersion");
	modIntf.version = (fversion ? fversion() : 0);

	modIntf.ovcInit = (VESSEL_Init)GetProcAddress (hMod, "ovcInit");
	modIntf.ovcExit = (VESSEL_Exit)GetProcAddress (hMod, "ovcExit");
	return true;
}

void Vessel::ClearModule ()
{
	if (hMod) {
		FreeLibrary (hMod);
		hMod = 0;
	}
	memset (&modIntf, 0, sizeof (modIntf));
}

bool Vessel::SetStateEx (const void *status)
{
	switch (*(DWORD*)status) { // check interface version
	case 2:  // VESSELSTATUS2 (interface version 2)
		SetState2 (status);
		return true;
	default: // unknown interface
		return false;
	}
}

bool Vessel::GetStateEx (void *status)
{
	switch (*(DWORD*)status) { // check interface version
	case 2:  // VESSELSTATUS2 (interface version 2)
		GetState2 (status);
		return true;
	default:  // unknown interface
		return false;
	}
}

void Vessel::EndStateUpdate ()
{
	Body::EndStateUpdate();
	for (int i = 0; i < 2; i++) {
		wbrake[i] = (wbrake_override[i] ? wbrake_override[i] : wbrake_permanent[i]);;
		wbrake_override[i] = 0.0;
	}
}

bool Vessel::Read (ifstream &scn)
{
	bool res;
	attmode = 1;
	attach_status.pname = 0;

	VESSELSTATUS2 vs;
	memset (&vs, 0, sizeof(VESSELSTATUS2));
	vs.version = 2;
	vs.flag = VS_FUELRESET | VS_FUELLIST | VS_THRUSTRESET | VS_THRUSTLIST | VS_DOCKINFOLIST;
	//vs.rbody   = NULL;
	//vs.base    = NULL;
	vs.port    = -1;
	//vs.xpdr    = 0;
	//vs.nfuel = vs.nthruster = vs.ndockinfo = 0;
	//vs.surf_lng = vs.surf_lat = vs.surf_hdg = 0.0;
	//veccpy (vs.vrot, _V(0,0,0));
	//veccpy (vs.arot, _V(0,0,0));

	void *vsptr = (void*)&vs;
	if (modIntf.v->Version() >= 1) {
		((VESSEL2*)modIntf.v)->clbkLoadStateEx ((FILEHANDLE)&scn, vsptr);
		res = true;
	} else {
		res = ParseScenarioEx (scn, vsptr);
	}
	SetStateEx (vsptr);
	if (vs.nfuel) {
		delete []vs.fuel;
		vs.fuel = NULL;
	}
	if (vs.nthruster) {
		delete []vs.thruster;
		vs.thruster = NULL;
	}
	return res;
}

bool Vessel::EditorModule (char *cbuf) const
{
	ifstream classf;
	if (!OpenConfigFile (classf)) return false;
	return GetItemString (classf, "EditorModule", cbuf);
}

// ==============================================================
// Parse scenario line into VESSELSTATUSx (interface version >= 2)

bool Vessel::ParseScenarioLineEx (char *line, void *status)
{
	switch (*(DWORD*)status) { // check interface version
	case 2:  // VESSELSTATUS2 (interface version 2)
		ParseScenarioLine2 (line, status);
		return true;
	default: // unknown interface
		return false;
	}
}

bool Vessel::ParseScenario (ifstream &scn, VESSELSTATUS &vs)
{
	char cbuf[256], *pc;
	
	for (;;) {
		if (!scn.getline (cbuf, 256)) break;
		pc = trim_string (cbuf);
		if (!_stricmp (pc, "END")) break;
		ParseScenarioLine (pc, vs);
	}
	return true;
}

bool Vessel::ParseScenarioEx (ifstream &scn, void *status)
{
	char cbuf[256], *pc;
	
	for (;;) {
		if (!scn.getline (cbuf, 256)) break;
		pc = trim_string (cbuf);
		if (!_stricmp (pc, "END")) break;
		ParseScenarioLineEx (pc, status);
	}
	return true;
}

void Vessel::Write (ostream &scn) const
{
	scn << name;
	if (classname) scn << ':' << classname;
	scn << endl;

	if (modIntf.v->Version() >= 1)
		((VESSEL2*)modIntf.v)->clbkSaveState ((FILEHANDLE)&scn);
	else
		WriteDefault (scn);

	scn << "END" << endl;
}

void Vessel::WriteDefault (ostream &ofs) const
{
	DWORD i;
	int pad;
	bool needlabel;

	switch (fstatus) {
	case FLIGHTSTATUS_LANDED:
		ofs << "  STATUS Landed " << proxyplanet->Name() << endl;
		if (proxybase && (pad = proxybase->LandedAtPad (this)) >= 0) 
			ofs << "  BASE " << proxybase->Name() << ':' << pad+1 << endl;
		ofs << "  POS " << setprecision(7) << sp.lng*DEG << ' ' << sp.lat*DEG << endl;
		ofs << "  HEADING " << setprecision(2) << sp.dir*DEG << endl;
		ofs << "  ALT " << setprecision(3) << sp.alt << endl;
		// new: save Euler angles of horizon-local rotation matrix
		ofs << "  AROT " << setprecision(3) <<  DEG*atan2 (land_rot.m23, land_rot.m33) << ' '
											<< -DEG*asin  (land_rot.m13) << ' '
							                <<  DEG*atan2 (land_rot.m12, land_rot.m11) << endl;
		break;
	case FLIGHTSTATUS_FREEFLIGHT:
		if (cbody) {
			ofs << "  STATUS Orbiting " << cbody->Name() << endl;
			ofs << "  RPOS " << setprecision(3) << GPos()-cbody->GPos() << endl;
			ofs << "  RVEL " << setprecision(4) << GVel()-cbody->GVel() << endl;
			ofs << "  AROT " << setprecision(3) <<  DEG*atan2 (s0->R.m23, s0->R.m33) << ' '
												<< -DEG*asin  (s0->R.m13) << ' '
							                    <<  DEG*atan2 (s0->R.m12, s0->R.m11) << endl;
			if (s0->omega.length2() > 1e-8)
				ofs << "  VROT " << setprecision(4) << DEG*s0->omega.x << ' ' << DEG*s0->omega.y << ' ' << DEG*s0->omega.z << endl;
		}
		break;
	}
	if (attach)   // vessel is passive child of another vessel
		ofs << "  ATTACHED " << GetAttachmentIndex (attach) << ':'
			<< attach->mate->GetAttachmentIndex (attach->mate_attach) << ','
			<< attach->mate->Name() << endl;

	if (attmode != 1)
		ofs << "  RCSMODE " << attmode << endl;

	if (ctrlsurfmode)
		ofs << "  AFCMODE " << ctrlsurfmode << endl;

	for (i =  0, needlabel = true; i < ntank; i++) {
		double lvl;
		if (lvl = GetPropellantLevel (tank[i])) {
			if (needlabel) { ofs << "  PRPLEVEL"; needlabel = false; }
			ofs << ' ' << i << ':' << setprecision(6) << lvl;
		}
	}
	if (!needlabel) ofs << endl;

	for (i = 0, needlabel = true; i < m_thruster.size(); i++)
		if (m_thruster[i]->level_permanent) {
			if (needlabel) { ofs << "  THLEVEL"; needlabel = false; }
			ofs << ' ' << i << ':' << m_thruster[i]->level_permanent;
		}
	if (!needlabel) ofs << endl;

	for (i = 0, needlabel = true; i < ndock; i++)
		if (dock[i]->mate) {
			if (needlabel) { ofs << "  DOCKINFO"; needlabel = false; }
			ofs << ' ' << i << ':' << dock[i]->matedock << ',' << dock[i]->mate->Name();
		}
	if (!needlabel) ofs << endl;

	for (i = 0, needlabel = true; i < ndock; i++)
		if (dock[i]->ids) {
			if (needlabel) { ofs << "  IDS"; needlabel = false; }
			ofs << ' ' << i << ':' << dock[i]->ids->GetStep() << ' ' << (int)(dock[i]->ids->GetRange()*0.001f);
		}
	if (!needlabel) ofs << endl;

	if (nnav) {
		ofs << "  NAVFREQ";
		for (i = 0; i < nnav; i++) ofs << ' ' << nav[i].step;
		ofs << endl;
	}
	if (xpdr)
		ofs << "  XPDR " << xpdr->GetStep() << endl;
	if (bFRrecord)
		ofs << "  FLIGHTDATA" << endl;
}

TOUCHDOWN_VTX *Vessel::HullvtxFirst ()
{
	next_hullvtx = 0;
	return (ntouchdown_vtx ? touchdown_vtx + next_hullvtx++ : NULL);
}

TOUCHDOWN_VTX *Vessel::HullvtxNext ()
{
	return (next_hullvtx < ntouchdown_vtx ? touchdown_vtx + next_hullvtx++ : NULL);
}

Vessel::LeanCam Vessel::camfwd;
Vessel::LeanCam Vessel::camleft;
Vessel::LeanCam Vessel::camright;

bool Vessel::rpressure = false;

// =======================================================================
// =======================================================================
// class VESSEL: module interface to vessel class

VESSEL::VESSEL (OBJHANDLE hvessel, int fmodel)
{
	vessel = (Vessel*)hvessel;
	flightmodel = (short)fmodel;
	version = 0;
}

OBJHANDLE VESSEL::Create (const char *name, const char *classname, const VESSELSTATUS &status)
{
	Vessel *vessel = new Vessel (g_psys, name, classname, status); TRACENEW
	g_pOrbiter->InsertVessel (vessel);
	return (OBJHANDLE)vessel;
}

const OBJHANDLE VESSEL::GetHandle () const
{
	return (OBJHANDLE)vessel;
}

char *VESSEL::GetName () const
{
	return (char*)vessel->Name();
}

char *VESSEL::GetClassName () const
{
	return vessel->classname;
}

int VESSEL::GetFlightModel () const
{
	return vessel->flightmodel;
}

int VESSEL::GetDamageModel () const
{
	return g_pOrbiter->Cfg()->CfgLogicPrm.DamageSetting;
}

bool VESSEL::GetEnableFocus () const
{
	return vessel->GetEnableFocus ();
}

double VESSEL::GetSize () const
{
	return vessel->Size();
}

double VESSEL::GetClipRadius () const
{
	return vessel->ClipRadius();
}

void VESSEL::SetClipRadius (double rad) const
{
	vessel->SetClipRadius (rad);
}

double VESSEL::GetMass () const
{
	return vessel->Mass();
}

double VESSEL::GetEmptyMass () const
{
	return vessel->EmptyMass();
}

double VESSEL::GetMaxFuelMass () const
{
	TankSpec *ts = vessel->DefaultPropellantHandle();
	return (ts ? ts->maxmass : 0.0);
}

double VESSEL::GetFuelMass () const
{
	TankSpec *ts = vessel->DefaultPropellantHandle();
	return (ts ? ts->mass : 0.0);
}

double VESSEL::GetFuelRate () const
{
	TankSpec *ts = vessel->DefaultPropellantHandle();
	return (ts ? vessel->GetPropellantFlowrate (ts) : 0.0);
}

double VESSEL::GetISP () const
{
	return vessel->m_defaultIsp;
}

double VESSEL::GetMaxThrust (ENGINETYPE eng) const
{
	switch (eng) {
	case ENGINE_MAIN:     return vessel->GetThrusterGroupMaxth (THGROUP_MAIN);
	case ENGINE_RETRO:    return vessel->GetThrusterGroupMaxth (THGROUP_RETRO);
	case ENGINE_HOVER:    return vessel->GetThrusterGroupMaxth (THGROUP_HOVER);
	case ENGINE_ATTITUDE: return vessel->GetThrusterGroupMaxth (THGROUP_ATT_PITCHUP);
		// assuming all attitude thruster groups are defined with the same maxth
	default:              return 0.0;
	}
}

double VESSEL::GetEngineLevel (ENGINETYPE eng) const
{
	switch (eng) {
	case ENGINE_MAIN:	  return vessel->GetThrusterGroupLevel (THGROUP_MAIN);
	case ENGINE_RETRO:	  return vessel->GetThrusterGroupLevel (THGROUP_RETRO);
	case ENGINE_HOVER:	  return vessel->GetThrusterGroupLevel (THGROUP_HOVER);
	default:              return 0.0;
	}
}

double *VESSEL::GetMainThrustModPtr (void) const
{
	static double dummy = 0.0;
	return &dummy;
}

int VESSEL::GetAttitudeMode () const
{
	return vessel->AttMode();
}

bool VESSEL::SetAttitudeMode (int mode) const
{
	return vessel->SetAttMode (mode);
}

DWORD VESSEL::GetADCtrlMode () const
{
	return vessel->ctrlsurfmode;
}

void VESSEL::SetADCtrlMode (DWORD mode) const
{
	vessel->SetADCtrlMode (mode);
}

void VESSEL::GetAttitudeRotLevel (VECTOR3 &th) const
{
	th.x = GetThrusterGroupLevel (THGROUP_ATT_PITCHUP) - GetThrusterGroupLevel (THGROUP_ATT_PITCHDOWN);
	th.y = GetThrusterGroupLevel (THGROUP_ATT_YAWLEFT) - GetThrusterGroupLevel (THGROUP_ATT_YAWRIGHT);
	th.z = GetThrusterGroupLevel (THGROUP_ATT_BANKRIGHT) - GetThrusterGroupLevel (THGROUP_ATT_BANKLEFT);
}

void VESSEL::GetAttitudeLinLevel (VECTOR3 &th) const
{
	th.x = GetThrusterGroupLevel (THGROUP_ATT_RIGHT) - GetThrusterGroupLevel (THGROUP_ATT_LEFT);
	th.y = GetThrusterGroupLevel (THGROUP_ATT_UP) - GetThrusterGroupLevel (THGROUP_ATT_DOWN);
	th.z = GetThrusterGroupLevel (THGROUP_ATT_FORWARD) - GetThrusterGroupLevel (THGROUP_ATT_BACK);
}

int VESSEL::ToggleAttitudeMode () const
{
	return vessel->ToggleAttMode();
}

double VESSEL::GetManualControlLevel (THGROUP_TYPE thgt, DWORD mode, DWORD device) const
{
	static int revmode[3] = {0,2,1};

	double lvl = g_pOrbiter->ManCtrlLevel (thgt, device);
	int md;
	switch (mode) {
	case MANCTRL_ATTMODE: md = vessel->attmode; break;
	case MANCTRL_REVMODE: md = revmode[vessel->attmode]; break;
	case MANCTRL_ROTMODE: md = 1; break;
	case MANCTRL_LINMODE: md = 2; break;
	case MANCTRL_ANYMODE: md = 3; break;
	}
	if (thgt >= THGROUP_ATT_PITCHUP && thgt <= THGROUP_ATT_BANKRIGHT)
		return ((md & 1) ? lvl : 0);
	if (thgt >= THGROUP_ATT_RIGHT && thgt <= THGROUP_ATT_BACK)
		return ((md & 2) ? lvl : 0);
	return 0;
}


double VESSEL::GetCOG_elev () const
{
	return vessel->cog_elev;
}

void VESSEL::GetCrossSections (VECTOR3 &cs) const
{
	cs.x = vessel->cs.x;
	cs.y = vessel->cs.y;
	cs.z = vessel->cs.z;
}

void VESSEL::GetCW (double &cw_z_pos, double &cw_z_neg, double &cw_x, double &cw_y) const
{
	cw_z_pos = vessel->CWz[0];
	cw_z_neg = vessel->CWz[1];
	cw_x     = vessel->CWx;
	cw_y     = vessel->CWy;
}

double VESSEL::GetWingAspect () const
{
	return vessel->wingaspect;
}

double VESSEL::GetWingEffectiveness () const
{
	return vessel->wingeff;
}

void VESSEL::GetRotDrag (VECTOR3 &rd) const
{
	rd.x = vessel->rdrag.x;
	rd.y = vessel->rdrag.y;
	rd.z = vessel->rdrag.z;
}

void VESSEL::GetPMI (VECTOR3 &pmi) const
{
	pmi.x = vessel->pmi.x;
	pmi.y = vessel->pmi.y;
	pmi.z = vessel->pmi.z;
}

void VESSEL::GetCameraOffset (VECTOR3 &co) const
{
	co.x = vessel->campos.x;
	co.y = vessel->campos.y;
	co.z = vessel->campos.z;
}

void VESSEL::GetCameraDefaultDirection (VECTOR3 &cd) const
{
	cd.x = vessel->camdir0.x;
	cd.y = vessel->camdir0.y;
	cd.z = vessel->camdir0.z;
}

double VESSEL::GetAtmTemperature () const
{
	return (vessel->sp.is_in_atm ? vessel->sp.atmT : 0.0);
}

double VESSEL::GetAtmDensity () const
{
	return (vessel->sp.is_in_atm ? vessel->sp.atmrho : 0.0);
}

double VESSEL::GetAtmPressure () const
{
	return (vessel->sp.is_in_atm ? vessel->sp.atmp : 0.0);
}

double VESSEL::GetDynPressure () const
{
	return (vessel->sp.is_in_atm ? vessel->sp.dynp : 0.0);
}

double VESSEL::GetMachNumber () const
{
	return (vessel->sp.is_in_atm ? vessel->sp.atmM : 0.0);
}

void VESSEL::GetStatus (VESSELSTATUS &status) const
{
	vessel->GetState (status);
}

void VESSEL::GetStatusEx (void *status) const
{
	vessel->GetStateEx (status);
}

DWORD VESSEL::GetFlightStatus () const
{
	DWORD status = (vessel->fstatus == FLIGHTSTATUS_LANDED ? 0x1:0x0);
	if (vessel->supervessel) status |= 0x2;
	return status;
}

const OBJHANDLE VESSEL::GetGravityRef () const
{
	return (OBJHANDLE)vessel->cbody;
}

const OBJHANDLE VESSEL::GetSurfaceRef () const
{
	const SurfParam *sp = vessel->GetSurfParam();
	if (sp)	return (OBJHANDLE)sp->ref;
	else return NULL;
}

const OBJHANDLE VESSEL::GetAtmRef () const
{
	return (vessel->sp.is_in_atm ? (OBJHANDLE)vessel->GetSurfParam()->ref : 0);
}

void VESSEL::GetGlobalPos (VECTOR3 &pos) const
{
	pos.x = vessel->s0->pos.x;
	pos.y = vessel->s0->pos.y;
	pos.z = vessel->s0->pos.z;
}

void VESSEL::GetGlobalVel (VECTOR3 &vel) const
{
	vel.x = vessel->s0->vel.x;
	vel.y = vessel->s0->vel.y;
	vel.z = vessel->s0->vel.z;
}

void VESSEL::GetRelativePos (OBJHANDLE hRef, VECTOR3 &pos) const
{
	Vector dp (vessel->GPos() - ((Body*)hRef)->GPos());
	pos.x = dp.x;
	pos.y = dp.y;
	pos.z = dp.z;
}

void VESSEL::GetRelativeVel (OBJHANDLE hRef, VECTOR3 &vel) const
{
	Vector dv (vessel->GVel() - ((Body*)hRef)->GVel());
	vel.x = dv.x;
	vel.y = dv.y;
	vel.z = dv.z;
}

void VESSEL::GetLinearMoment (VECTOR3 &F) const
{
	F.x = vessel->Flin.x;
	F.y = vessel->Flin.y;
	F.z = vessel->Flin.z;
}

void VESSEL::GetAngularVel (VECTOR3 &avel) const
{
	avel.x = vessel->s0->omega.x;
	avel.y = vessel->s0->omega.y;
	avel.z = vessel->s0->omega.z;
}

void VESSEL::SetAngularVel (const VECTOR3 &avel) const
{
	vessel->SetAngVel (MakeVector (avel));
}

void VESSEL::GetAngularAcc (VECTOR3 &aacc) const
{
	aacc.x = vessel->arot.x;
	aacc.y = vessel->arot.y;
	aacc.z = vessel->arot.z;
}

void VESSEL::GetAngularMoment (VECTOR3 &amom) const
{
	amom.x = vessel->Amom.x;
	amom.y = vessel->Amom.y;
	amom.z = vessel->Amom.z;
}

void VESSEL::GetGlobalOrientation (VECTOR3 &arot) const
{
	EulerAngles (vessel->s0->R, arot);
}

void VESSEL::SetGlobalOrientation (const VECTOR3 &arot) const
{
	vessel->SetGlobalOrientation (MakeVector (arot));
}

OBJHANDLE VESSEL::GetEquPos (double &longitude, double &latitude, double &radius) const
{
	const SurfParam *sp = vessel->GetSurfParam();
	if (sp) {
		longitude = sp->lng;
		latitude  = sp->lat;
		radius    = sp->rad;
		return (OBJHANDLE)sp->ref;
	} else {
		return NULL;
	}
}

double VESSEL::GetAltitude (void) const
{
	const SurfParam *sp = vessel->GetSurfParam();
	if (sp) return sp->alt0;
	else    return 0.0;
}

double VESSEL::GetAltitude (AltitudeMode mode, int *reslvl)
{
	const SurfParam *sp = vessel->GetSurfParam();
	if (sp) {
		if (mode == ALTMODE_MEANRAD) {
			if (reslvl) *reslvl = 0;
			return sp->alt0;
		} else {
			if (reslvl) *reslvl = sp->elev_lvl;
			return sp->alt;
		}
	} else {
		if (reslvl) *reslvl = 0;
		return 0.0;
	}
}

double VESSEL::GetGroundspeed () const
{
	const SurfParam *sp = vessel->GetSurfParam();
	if (sp) return sp->groundspd;
	else    return 0.0;
}

bool VESSEL::GetGroundspeedVector (REFFRAME frame, VECTOR3 &v) const
{
	const SurfParam *sp = vessel->GetSurfParam();
	if (sp) {
		switch (frame) {
		case FRAME_GLOBAL:
			v.x = sp->groundvel_glob.x, v.y = sp->groundvel_glob.y, v.z = sp->groundvel_glob.z;
			return true;
		case FRAME_LOCAL:
			v.x = sp->groundvel_ship.x, v.y = sp->groundvel_ship.y, v.z = sp->groundvel_ship.z;
			return true;
		case FRAME_REFLOCAL: {
			Vector hvel (tmul (sp->ref->GRot(), sp->groundvel_glob));
			v.x = hvel.x, v.y = hvel.y, v.z = hvel.z;
			} return true;
		case FRAME_HORIZON: {
			Vector hvel (tmul (sp->ref->GRot(), sp->groundvel_glob));
			hvel.Set (mul (sp->L2H, hvel));
			v.x = hvel.x, v.y = hvel.y, v.z = hvel.z;
			} return true;
		default:
			v.x = v.y = v.z = 0.0;
			return false;
		}
	} else {
		v.x = v.y = v.z = 0.0;
		return false;
	}
}

double VESSEL::GetAirspeed (void) const
{
	const SurfParam *sp = vessel->GetSurfParam();
	if (sp) return sp->airspd;
	else    return 0.0;
}

bool VESSEL::GetAirspeedVector (REFFRAME frame, VECTOR3 &v) const
{
	const SurfParam *sp = vessel->GetSurfParam();
	if (sp) {
		switch (frame) {
		case FRAME_GLOBAL:
			v.x = sp->airvel_glob.x, v.y = sp->airvel_glob.y, v.z = sp->airvel_glob.z;
			return true;
		case FRAME_LOCAL:
			v.x = sp->airvel_ship.x, v.y = sp->airvel_ship.y, v.z = sp->airvel_ship.z;
			return true;
		case FRAME_REFLOCAL: {
			Vector hvel (tmul (sp->ref->GRot(), sp->airvel_glob));
			v.x = hvel.x, v.y = hvel.y, v.z = hvel.z;
			} return true;
		case FRAME_HORIZON: {
			Vector hvel (tmul (sp->ref->GRot(), sp->airvel_glob));
			hvel.Set (mul (sp->L2H, hvel));
			v.x = hvel.x, v.y = hvel.y, v.z = hvel.z;
			} return true;
		default:
			v.x = v.y = v.z = 0.0;
			return false;
		}
	} else {
		v.x = v.y = v.z = 0.0;
		return false;
	}
}

bool VESSEL::GetShipAirspeedVector (VECTOR3 &v) const
{
	LOGOUT_OBSOLETE;
	const SurfParam *sp = vessel->GetSurfParam();
	if (sp) {
		v.x = sp->airvel_ship.x, v.y = sp->airvel_ship.y, v.z = sp->airvel_ship.z;
		return true;
	} else {
		v.x = v.y = v.z = 0.0;
		return false;
	}
}

bool VESSEL::GetHorizonAirspeedVector (VECTOR3 &v) const
{
	LOGOUT_OBSOLETE;
	const SurfParam *sp = vessel->GetSurfParam();
	if (sp) {
		Vector hvel (tmul (sp->ref->GRot(), sp->airvel_glob));
		hvel.Set (mul (sp->L2H, hvel));
		v.x = hvel.x, v.y = hvel.y, v.z = hvel.z;
		return true;
	} else {
		v.x = v.y = v.z = 0.0;
		return false;
	}
}

double VESSEL::GetAOA () const
{
	const SurfParam *sp = vessel->GetSurfParam();
	return sp ? -atan2 (sp->airvel_ship.y, sp->airvel_ship.z): 0.0;
}

double VESSEL::GetSlipAngle () const
{
	const SurfParam *sp = vessel->GetSurfParam();
	return sp ? -atan2 (sp->airvel_ship.x, sp->airvel_ship.z): 0.0;
}

double VESSEL::GetPitch () const
{
	const SurfParam *sp = vessel->GetSurfParam();
	return sp ? sp->pitch: 0.0;
}

double VESSEL::GetBank () const
{
	const SurfParam *sp = vessel->GetSurfParam();
	return sp ? sp->bank: 0.0;
}

double VESSEL::GetYaw () const
{
	const SurfParam *sp = vessel->GetSurfParam();
	return sp ? sp->dir: 0.0;
}

double VESSEL::GetSurfaceElevation () const
{
	const SurfParam *sp = vessel->GetSurfParam();
	return sp ? sp->elev: 0.0;
}

VECTOR3 VESSEL::GetSurfaceNormal () const
{
	const SurfParam *sp = vessel->GetSurfParam();
	return sp ? _V(sp->surfnml.x, sp->surfnml.y, sp->surfnml.z): _V(0,0,0);
}

double VESSEL::GetLift (void) const
{
	return vessel->Lift;
}

double VESSEL::GetDrag (void) const
{
	return vessel->Drag;
}

double VESSEL::GetSideForce() const
{
	return vessel->SideForce;
}

bool VESSEL::GetWeightVector (VECTOR3 &G) const
{
	static Vector F;
	bool bWeight = vessel->GetWeightVector (F);
	CopyVector (F, G);
	return bWeight;
}

bool VESSEL::GetThrustVector (VECTOR3 &T) const
{
	static Vector F;
	bool bThrust = vessel->GetThrustVector (F);
	CopyVector (F, T);
	return bThrust;
}

bool VESSEL::GetLiftVector (VECTOR3 &L) const
{
	static Vector F;
	bool bLift = vessel->GetLiftVector (F);
	CopyVector (F, L);
	return bLift;
}

bool VESSEL::GetDragVector (VECTOR3 &D) const
{
	static Vector F;
	bool bDrag = vessel->GetDragVector (F);
	CopyVector (F, D);
	return bDrag;
}

bool VESSEL::GetSideForceVector(VECTOR3 &SF) const
{
	static Vector F;
	bool bSideForce = vessel->GetSideForceVector(F);
	CopyVector(F, SF);
	return bSideForce;
}

bool VESSEL::GetForceVector (VECTOR3 &F) const
{
	static Vector FF;
	bool bForce = vessel->GetForceVector (FF);
	CopyVector (FF, F);
	return bForce;
}

bool VESSEL::GetTorqueVector (VECTOR3 &M) const
{
	static Vector F;
	bool bTorque = vessel->GetTorqueVector (F);
	CopyVector (F, M);
	return bTorque;
}

OBJHANDLE VESSEL::GetElements (ELEMENTS &el, double &mjd_ref) const
{
	const Elements *els = vessel->Els();
	if (els) {
		el.a      = els->a;
		el.e      = els->e;
		el.i      = els->i;
		el.theta  = els->theta;
		el.omegab = els->omegab;
		el.L      = els->L;
		mjd_ref   = els->MJDepoch();
		return (OBJHANDLE)vessel->cbody;
	} else {
		return NULL;
	}
}

bool VESSEL::GetElements (OBJHANDLE hRef, ELEMENTS &el, ORBITPARAM *prm, double mjd_ref, int frame) const
{
	const CelestialBody *ref = (hRef ? (CelestialBody*)hRef : vessel->cbody);
	Elements els (1, 0, 0, 0, 0, 0, mjd_ref ? mjd_ref : td.MJD0);
	els.SetMasses (0, ref->Mass());
	Vector p (vessel->GPos()-ref->GPos());
	Vector v (vessel->GVel()-ref->GVel());

	// rotate ecliptic -> equatorial frame
	if (frame == FRAME_EQU) {
		p = tmul (ref->RotObliq(), p);
		v = tmul (ref->RotObliq(), v);
	}
	// calculate primary elements
	els.Calculate (p, v, td.SimT0);
	el.a      = els.a;
	el.e      = els.e;
	el.i      = els.i;
	el.theta  = els.theta;
	el.omegab = els.omegab;
	el.L      = els.L;

	// retrieve additional parameters, if requested
	if (prm) {
		prm->SMi = els.SMi();
		prm->PeD = els.PeDist();
		prm->ApD = (els.e < 1.0 ? els.ApDist() : 0.0);
		prm->MnA = els.MeanAnm();
		prm->TrA = els.TrueAnm();
		prm->MnL = els.MeanLng();
		prm->TrL = els.TrueLng();
		prm->EcA = els.EccAnm();
		prm->Lec = els.LinEcc();
		prm->T = (els.e < 1.0 ? els.OrbitT() : 0.0);
		prm->PeT = els.PeT();
		prm->ApT = (els.e < 1.0 ? els.ApT() : 0.0);
	}
	return true;
}

bool VESSEL::SetElements (OBJHANDLE hRef, const ELEMENTS &el, ORBITPARAM *prm, double mjd_ref, int frame) const
{
	const CelestialBody *ref = (hRef ? (CelestialBody*)hRef : vessel->cbody);
	Elements els;
	if (!mjd_ref) mjd_ref = td.MJD0;
	els.Set (el.a, el.e, el.i, el.theta, el.omegab, el.L, mjd_ref);
	els.Setup (0, ref->Mass(), mjd_ref);
	Vector p, v;
	if (prm) {
		els.Update (p, v);
		prm->SMi = els.SMi();
		prm->PeD = els.PeDist();
		prm->ApD = (els.e < 1.0 ? els.ApDist() : 0.0);
		prm->MnA = els.MeanAnm();
		prm->TrA = els.TrueAnm();
		prm->MnL = els.MeanLng();
		prm->TrL = els.TrueLng();
		prm->EcA = els.EccAnm();
		prm->Lec = els.LinEcc();
		prm->T = (els.e < 1.0 ? els.OrbitT() : 0.0);
		prm->PeT = els.PeT();
		prm->ApT = (els.e < 1.0 ? els.ApT() : 0.0);
	} else {
		els.PosVel (p, v, td.SimT0);
	}
	if (frame == FRAME_EQU) { // rotate equatorial -> ecliptic frame
		p = mul (ref->RotObliq(), p);
		v = mul (ref->RotObliq(), v);
	}
	if (p.length() < ref->Size())
		return false; // don't allow vessel below planet surface

	vessel->RPlace (p + ref->GPos(), v + ref->GVel());

	return true;
}

OBJHANDLE VESSEL::GetArgPer (double &arg) const
{
	const Elements *els = vessel->Els();
	if (els) {
		arg = els->ArgPer();
		return (OBJHANDLE)vessel->cbody;
	} else return NULL;
}

OBJHANDLE VESSEL::GetSMi (double &smi) const
{
	const Elements *els = vessel->Els();
	if (els) {
		smi = els->SMi();
		return (OBJHANDLE)vessel->cbody;
	} else return NULL;
}

OBJHANDLE VESSEL::GetApDist (double &apdist) const
{
	const Elements *els = vessel->Els();
	if (els) {
		apdist = els->ApDist();
		return (OBJHANDLE)vessel->cbody;
	} else return NULL;
}

OBJHANDLE VESSEL::GetPeDist (double &pedist) const
{
	const Elements *els = vessel->Els();
	if (els) {
		pedist = els->PeDist();
		return (OBJHANDLE)vessel->cbody;
	} else return NULL;
}

void VESSEL::SetEnableFocus (bool enable) const
{
	vessel->SetEnableFocus (enable);
}

void VESSEL::SetSize (double size) const
{
	vessel->SetSize (size);
}

void VESSEL::SetVisibilityLimit (double vislimit, double spotlimit) const
{
	vessel->vislimit = vislimit;
	vessel->spotlimit = (spotlimit >= 0 ? spotlimit : vislimit);
}

void VESSEL::SetEmptyMass (double m) const
{
	vessel->SetEmptyMass (m);
}

void VESSEL::SetMaxFuelMass (double mass) const
{
	TankSpec *ts = vessel->DefaultPropellantHandle();
	if (ts) vessel->SetPropellantMaxMass (ts, mass);
	else    vessel->CreatePropellantResource (mass);
}

void VESSEL::SetFuelMass (double mass) const
{
	TankSpec *ts = vessel->DefaultPropellantHandle();
	if (ts) vessel->SetPropellantMass (ts, mass);
}

bool VESSEL::SetGravityGradientDamping (double damp) const
{
	return vessel->SetGravityGradientDamping (damp);
}

double VESSEL::GetGravityGradientDamping () const
{
	return vessel->GetGravityGradientDamping ();
}

void VESSEL::SetISP (double isp) const
{
	vessel->m_defaultIsp = isp;
}

void VESSEL::SetMaxThrust (ENGINETYPE eng, double th) const
{
	// obsolete; we are translating this into the new thruster interface
	// for backward compatibility

	switch (eng) {
	case ENGINE_MAIN:
	case ENGINE_RETRO:
	case ENGINE_HOVER:
		vessel->SetMaxThrust_old (eng, th);
		break;
	case ENGINE_ATTITUDE:
		vessel->CreateDefaultAttitudeSet (th);
		break;
	}
}

void VESSEL::SetEngineLevel (ENGINETYPE eng, double level) const
{
	// OBSOLETE: replaced by SetThrusterGroupLevel

	switch (eng) {
	case ENGINE_RETRO:
		level = -level; // fall through
	case ENGINE_MAIN:
		if (level >= 0) { 
			vessel->SetThrusterGroupLevel (THGROUP_MAIN, level);
			vessel->SetThrusterGroupLevel (THGROUP_RETRO, 0.0);
		} else {
			vessel->SetThrusterGroupLevel (THGROUP_MAIN, 0.0);
			vessel->SetThrusterGroupLevel (THGROUP_RETRO, -level);
		}
		break;
	case ENGINE_HOVER:
		vessel->SetThrusterGroupLevel (THGROUP_HOVER, level);
		break;
	}
}

void VESSEL::IncEngineLevel (ENGINETYPE eng, double dlevel) const
{
	// OBSOLETE: replaced by IncThrusterGroupLevel

	if (eng <= ENGINE_HOVER)
		vessel->IncThrusterGroupLevel ((THGROUP_TYPE)eng, dlevel);
	// relying on the fact that for main, retro and hover engines
	// ENGINETYPE and THGROUP_TYPE enums are equivalent
}

void VESSEL::SetAttitudeRotLevel (const VECTOR3 &th) const
{
	// First, zero all attitude thruster levels
	for (THGROUP_TYPE thg = THGROUP_ATT_PITCHUP; thg <= THGROUP_ATT_BANKRIGHT; thg = (THGROUP_TYPE)(thg+1))
		SetThrusterGroupLevel (thg, 0.0);
	
	// Then add the commanded levels
	if (th.x >= 0.0) IncThrusterGroupLevel (THGROUP_ATT_PITCHUP,    th.x);
	else             IncThrusterGroupLevel (THGROUP_ATT_PITCHDOWN, -th.x);
	if (th.y >= 0.0) IncThrusterGroupLevel (THGROUP_ATT_YAWLEFT,    th.y);
	else             IncThrusterGroupLevel (THGROUP_ATT_YAWRIGHT,  -th.y);
	if (th.z >= 0.0) IncThrusterGroupLevel (THGROUP_ATT_BANKRIGHT,  th.z);
	else             IncThrusterGroupLevel (THGROUP_ATT_BANKLEFT,  -th.z);
}

void VESSEL::SetAttitudeRotLevel (int axis, double th) const
{
	switch (axis) {
	case 0: vessel->SetAttitudeRotX (th); break;
	case 1: vessel->SetAttitudeRotY (th); break;
	case 2: vessel->SetAttitudeRotZ (th); break;
	}
}

void VESSEL::SetAttitudeLinLevel (int axis, double th) const
{
	switch (axis) {
	case 0: vessel->SetAttitudeLinX (th); break;
	case 1: vessel->SetAttitudeLinY (th); break;
	case 2: vessel->SetAttitudeLinZ (th); break;
	}
}

void VESSEL::SetAttitudeLinLevel (const VECTOR3 &th) const
{
	vessel->SetAttitudeLinX (th.x);
	vessel->SetAttitudeLinY (th.y);
	vessel->SetAttitudeLinZ (th.z);
}

bool VESSEL::ActivateNavmode (int mode)
{
	return vessel->SetNavMode (mode);
}

bool VESSEL::DeactivateNavmode (int mode)
{
	return vessel->ClrNavMode (mode);
}

bool VESSEL::ToggleNavmode (int mode)
{
	return vessel->TglNavMode (mode);
}

bool VESSEL::GetNavmodeState (int mode)
{
	return vessel->NavModeActive (mode);
}

bool VESSEL::GetHoverHoldAltitude (double &alt, bool &terrainalt)
{
	bool active = vessel->NavModeActive (NAVMODE_HOLDALT);
	if (active) {
		alt = vessel->hoverhold.alt;
		terrainalt = vessel->hoverhold.terrain;
	}
	return active;
}

void VESSEL::SetHoverHoldAltitude (double alt, bool terrainalt)
{
	vessel->SetHoverHoldAltitude (alt, terrainalt);
}

void VESSEL::SetCOG_elev (double cog) const
{
	vessel->cog_elev = cog;
}

void VESSEL::SetCrossSections (const VECTOR3 &cs) const
{
	vessel->cs.Set (cs.x, cs.y, cs.z);
	vessel->vd_forw = cs.z * 0.5*vessel->CWz[0];
	vessel->vd_back = cs.z * 0.5*vessel->CWz[1];
	vessel->vd_vert = cs.y * 0.5*vessel->CWy;
	vessel->vd_side = cs.x * 0.5*vessel->CWx;
}

void VESSEL::SetCW (double cw_z_pos, double cw_z_neg, double cw_x, double cw_y) const
{
	vessel->CWz[0] = cw_z_pos;
	vessel->CWz[1] = cw_z_neg;
	vessel->CWx    = cw_x;
	vessel->CWy    = cw_y;

	vessel->vd_forw = vessel->cs.z * 0.5*vessel->CWz[0];
	vessel->vd_back = vessel->cs.z * 0.5*vessel->CWz[1];
	vessel->vd_vert = vessel->cs.y * 0.5*vessel->CWy;
	vessel->vd_side = vessel->cs.x * 0.5*vessel->CWx;
}

void VESSEL::SetWingAspect (double aspect) const
{
	vessel->wingaspect = aspect;
	vessel->wingfactor = aspect * vessel->wingeff;
}

void VESSEL::SetWingEffectiveness (double eff) const
{
	vessel->wingeff = eff;
	vessel->wingfactor = vessel->wingaspect * eff;
}

void VESSEL::SetRotDrag (const VECTOR3 &rd) const
{
	vessel->rdrag.Set (rd.x, rd.y, rd.z);
}

double VESSEL::GetPitchMomentScale () const
{
	return vessel->pitch_moment_scale;
}

void VESSEL::SetPitchMomentScale (double scale) const
{
	vessel->pitch_moment_scale = scale;
}

double VESSEL::GetYawMomentScale () const
{
	return vessel->bank_moment_scale;
}

void VESSEL::SetYawMomentScale (double scale) const
{
	vessel->bank_moment_scale = scale;
}

double VESSEL::GetBankMomentScale () const
{
	LOGOUT_OBSOLETE;
	return vessel->bank_moment_scale;
}

void VESSEL::SetBankMomentScale (double scale) const
{
	LOGOUT_OBSOLETE;
	vessel->bank_moment_scale = scale;
}

void VESSEL::SetPMI (const VECTOR3 &pmi) const
{
	vessel->pmi.Set (pmi.x, pmi.y, pmi.z);
}

void VESSEL::SetAlbedoRGB (const VECTOR3 &albedo) const
{
	vessel->albedo.Set(albedo.x, albedo.y, albedo.z);
}

double VESSEL::GetTrimScale () const
{
	return vessel->trim_scale;
}

void VESSEL::SetTrimScale (double scale) const
{
	vessel->trim_scale = scale;
	vessel->bElevTrim = (scale > 0);
}

void VESSEL::SetCameraOffset (const VECTOR3 &co) const
{
	vessel->campos.Set (co.x, co.y, co.z);
}

void VESSEL::SetCameraDefaultDirection (const VECTOR3 &cd) const
{
	vessel->camdir0.Set (cd.x, cd.y, cd.z);
	vessel->camtilt0 = 0;
	if (vessel == g_focusobj) g_camera->SetDefaultCockpitDir (vessel->camdir0);
}

void VESSEL::SetCameraDefaultDirection (const VECTOR3 &cd, double tilt) const
{
	vessel->camdir0.Set (cd.x, cd.y, cd.z);
	vessel->camtilt0 = tilt;
	if (vessel == g_focusobj) g_camera->SetDefaultCockpitDir (vessel->camdir0, tilt);
}

void VESSEL::SetCameraRotationRange (double left, double right, double up, double down) const
{
	vessel->camdp_left = left;
	vessel->camdp_right = right;
	vessel->camdt_up = up;
	vessel->camdt_down = down;
}

void VESSEL::SetCameraCatchAngle (double cangle) const
{
	vessel->camcatchangle = cangle;
	if (vessel == g_focusobj) g_camera->SetCatchAngle (cangle);
}

void VESSEL::SetCameraShiftRange (const VECTOR3 &forward, const VECTOR3 &left, const VECTOR3 &right) const
{
	vessel->SetCameraShiftRange (forward, left, right);
}

void VESSEL::SetCameraMovement (const VECTOR3 &fwdpos, double fwdphi, double fwdtht,
	const VECTOR3 &lpos, double lphi, double ltht,
	const VECTOR3 &rpos, double rphi, double rtht) const
{
	vessel->SetCameraMovement (fwdpos, fwdphi, fwdtht, lpos, lphi, ltht, rpos, rphi, rtht);
}

void VESSEL::TriggerPanelRedrawArea (int panel_id, int area_id)
{
	if (g_pane && vessel == g_focusobj)
		g_pane->TriggerPanelRedrawArea (panel_id, area_id);
}

void VESSEL::TriggerRedrawArea (int panel_id, int vc_id, int area_id)
{
	if (g_pane && vessel == g_focusobj)
		g_pane->TriggerRedrawArea (panel_id, vc_id, area_id);
}

void VESSEL::SetLiftCoeffFunc (LiftCoeffFunc lcf) const
{
	vessel->LiftCoeff = lcf;
}

DOCKHANDLE VESSEL::CreateDock (const VECTOR3 &pos, const VECTOR3 &dir, const VECTOR3 &rot) const
{
	return (DOCKHANDLE)vessel->CreateDock (MakeVector (pos), MakeVector (dir), MakeVector (rot));
}

bool VESSEL::DelDock (DOCKHANDLE hDock) const
{
	return vessel->DelDock ((PortSpec*)hDock);
}

void VESSEL::ClearDockDefinitions () const
{
	vessel->ClearDockDefinitions();
}

void VESSEL::SetDockParams (const VECTOR3 &pos, const VECTOR3 &dir, const VECTOR3 &rot) const
{
	if (!vessel->ndock)
		vessel->CreateDock (MakeVector (pos), MakeVector (dir), MakeVector (rot));
	else
		vessel->SetDockParams (vessel->dock[0], MakeVector (pos), MakeVector (dir), MakeVector (rot));
}

void VESSEL::SetDockParams (DOCKHANDLE dock, const VECTOR3 &pos, const VECTOR3 &dir, const VECTOR3 &rot) const
{
	vessel->SetDockParams ((PortSpec*)dock, MakeVector (pos), MakeVector (dir), MakeVector (rot));
}

void VESSEL::GetDockParams (DOCKHANDLE dock, VECTOR3 &pos, VECTOR3 &dir, VECTOR3 &rot) const
{
	PortSpec *ps = (PortSpec*)dock;
	pos.x = ps->ref.x, pos.y = ps->ref.y, pos.z = ps->ref.z;
	dir.x = ps->dir.x, dir.y = ps->dir.y, dir.z = ps->dir.z;
	rot.x = ps->rot.x, rot.y = ps->rot.y, rot.z = ps->rot.z;
}

UINT VESSEL::DockCount () const
{
	return vessel->ndock;
}

DOCKHANDLE VESSEL::GetDockHandle (UINT n) const
{
	return (DOCKHANDLE)(n < vessel->ndock ? vessel->GetDockParams(n) : 0);
}

OBJHANDLE VESSEL::GetDockStatus (DOCKHANDLE dock) const
{
	return (OBJHANDLE)((PortSpec*)dock)->mate;
}

UINT VESSEL::DockingStatus (UINT port) const
{
	if (port >= vessel->ndock) return 0;
	return (vessel->GetDockParams(port)->mate ? 1 : 0);
}

void VESSEL::MoveDock(DOCKHANDLE hDock, const VECTOR3& pos, const VECTOR3& dir, const VECTOR3& rot)
{
	vessel->MoveDock((PortSpec*)hDock, MakeVector(pos), MakeVector(dir), MakeVector(rot));
}

DOCKHANDLE VESSEL::GetProxyDock(DOCKHANDLE hDock) const
{
	return (DOCKHANDLE)vessel->GetProxyDock((PortSpec*)hDock);
}

int VESSEL::GetDockIndex(DOCKHANDLE hDock) const
{
	for (int i = 0; i < vessel->ndock; i++) if (vessel->dock[i] == hDock) return i;
	return -1;
}

bool VESSEL::GetTargetDockAlignment(DOCKHANDLE pD, DOCKHANDLE pT, VECTOR3* ofs, VECTOR3* dir, VECTOR3* rot, VECTOR3* vel) const
{
	Vector o, d, r, v;
	Vector* op = ofs ? &o : nullptr; Vector* dp = dir ? &d : nullptr;
	Vector* rp = rot ? &r : nullptr; Vector* vp = vel ? &v : nullptr;
	if (vessel->GetTargetDockAlignment((PortSpec*)pD, (PortSpec*)pT, op, dp, rp, vp)) {
		if (ofs) { ofs->x = o.x; ofs->y = o.y; ofs->z = o.z; }
		if (dir) { dir->x = d.x; dir->y = d.y; dir->z = d.z; }
		if (rot) { rot->x = r.x; rot->y = r.y; rot->z = r.z; }
		if (vel) { vel->x = v.x; vel->y = v.y; vel->z = v.z; }
		return true;
	}
	return false;
}

int VESSEL::Dock (OBJHANDLE target, UINT n, UINT tgtn, UINT mode) const
{
	return vessel->Dock ((Vessel*)target, n, tgtn, mode);
}

int VESSEL::Dock(DOCKHANDLE hSrc, DOCKHANDLE hTgt, DWORD mode) const
{
	Vessel* pS = ((PortSpec*)hSrc)->owner;
	Vessel* pT = ((PortSpec*)hTgt)->owner;
	int n = -1, t = -1;
	for (int i = 0; i < pS->ndock; i++) if (pS->dock[i] == ((PortSpec*)hSrc)) { n = i; break; }
	for (int i = 0; i < pT->ndock; i++) if (pT->dock[i] == ((PortSpec*)hTgt)) { t = i; break; }
	if (n >= 0 && t >= 0) return vessel->Dock(pT, n, t, mode);
	return -4;
}

bool VESSEL::Undock (UINT n, const OBJHANDLE exclude) const
{
	return vessel->Undock (n, (Vessel*)exclude);
}

void VESSEL::SetDockMode (int mode) const
{
	vessel->SetDockMode (mode);
}

ATTACHMENTHANDLE VESSEL::CreateAttachment (bool toparent, const VECTOR3 &pos, const VECTOR3 &dir, const VECTOR3 &rot, const char *id, bool loose) const
{
	return (ATTACHMENTHANDLE)vessel->CreateAttachment (toparent, MakeVector (pos), MakeVector (dir), MakeVector (rot), id, loose);
}

bool VESSEL::DelAttachment (ATTACHMENTHANDLE attachment) const
{
	return vessel->DelAttachment ((AttachmentSpec*)attachment);
}

void VESSEL::ClearAttachments () const
{
	vessel->ClearAttachments();
}

void VESSEL::SetAttachmentParams (ATTACHMENTHANDLE attachment, const VECTOR3 &pos, const VECTOR3 &dir, const VECTOR3 &rot) const
{
	vessel->SetAttachmentParams ((AttachmentSpec*)attachment, MakeVector (pos), MakeVector (dir), MakeVector (rot));
}

void VESSEL::GetAttachmentParams (ATTACHMENTHANDLE attachment, VECTOR3 &pos, VECTOR3 &dir, VECTOR3 &rot) const
{
	AttachmentSpec *as = (AttachmentSpec*)attachment;
	pos.x = as->ref.x, pos.y = as->ref.y, pos.z = as->ref.z;
	dir.x = as->dir.x, dir.y = as->dir.y, dir.z = as->dir.z;
	rot.x = as->rot.x, rot.y = as->rot.y, rot.z = as->rot.z;
}

const char *VESSEL::GetAttachmentId (ATTACHMENTHANDLE attachment) const
{
	return ((AttachmentSpec*)attachment)->id;
}

OBJHANDLE VESSEL::GetAttachmentStatus (ATTACHMENTHANDLE attachment) const
{
	return (OBJHANDLE)((AttachmentSpec*)attachment)->mate;
}

bool VESSEL::AttachChild (OBJHANDLE child, ATTACHMENTHANDLE attachment, ATTACHMENTHANDLE child_attachment) const
{
	return vessel->AttachChild ((Vessel*)child, (AttachmentSpec*)attachment, (AttachmentSpec*)child_attachment);
}

bool VESSEL::DetachChild (ATTACHMENTHANDLE attachment, double vel) const
{
	return vessel->DetachChild ((AttachmentSpec*)attachment, vel);
}

DWORD VESSEL::AttachmentCount (bool toparent) const
{
	return (toparent ? vessel->npattach : vessel->ncattach);
}

DWORD VESSEL::GetAttachmentIndex (ATTACHMENTHANDLE attachment) const
{
	return vessel->GetAttachmentIndex ((AttachmentSpec*)attachment);
}

ATTACHMENTHANDLE VESSEL::GetAttachmentHandle (bool toparent, DWORD i) const
{
	if (toparent) {
		return (i < vessel->npattach ? (ATTACHMENTHANDLE)vessel->pattach[i] : 0);
	} else {
		return (i < vessel->ncattach ? (ATTACHMENTHANDLE)vessel->cattach[i] : 0);
	}
}

OBJHANDLE VESSEL::GetAttachmentRoot() const
{
	return vessel->GetAttachmentRoot();
}

void VESSEL::AddBeacon (BEACONLIGHTSPEC *bs)
{
	vessel->AddBeacon (bs);
}

bool VESSEL::DelBeacon (BEACONLIGHTSPEC *bs)
{
	return vessel->DelBeacon (bs);
}

void VESSEL::ClearBeacons ()
{
	vessel->ClearBeacons ();
}

const BEACONLIGHTSPEC *VESSEL::GetBeacon (DWORD idx) const
{
	return vessel->GetBeacon (idx);
}

LightEmitter *VESSEL::AddPointLight (const VECTOR3 &pos, double range, double att0, double att1, double att2, COLOUR4 diffuse, COLOUR4 specular, COLOUR4 ambient) const
{
	return vessel->AddPointLight (pos, range, att0, att1, att2, diffuse, specular, ambient);
}

LightEmitter *VESSEL::AddSpotLight (const VECTOR3 &pos, const VECTOR3 &dir, double range, double att0, double att1, double att2, double umbra, double penumbra, COLOUR4 diffuse, COLOUR4 specular, COLOUR4 ambient) const
{
	return vessel->AddSpotLight (pos, dir, range, att0, att1, att2, umbra, penumbra, diffuse, specular, ambient);
}

DWORD VESSEL::LightEmitterCount () const
{
	return vessel->LightEmitterCount();
}

const LightEmitter *VESSEL::GetLightEmitter (DWORD i) const
{
	return (i < vessel->LightEmitterCount() ? vessel->GetLightEmitter(i) : NULL);
}

bool VESSEL::DelLightEmitter (LightEmitter *le) const
{
	return vessel->DelLightEmitter (le);
}

void VESSEL::ClearLightEmitters () const
{
	vessel->ClearLightEmitters();
}

bool VESSEL::GetSuperstructureCG (VECTOR3 &cg) const
{
	Vector vcg;
	bool ok = vessel->GetSuperStructCG (vcg);
	cg.x = vcg.x;
	cg.y = vcg.y;
	cg.z = vcg.z;
	return ok;
}

void VESSEL::SetTouchdownPoints (const VECTOR3 &pt1, const VECTOR3 &pt2, const VECTOR3 &pt3) const
{
	TOUCHDOWNVTX tdvtx[3];
	tdvtx[0].pos = pt1;
	tdvtx[1].pos = pt2;
	tdvtx[2].pos = pt3;
	for (DWORD i = 0; i < 3; i++) {
		tdvtx[i].stiffness = (vessel->emass == 1e3 ? 1e6 : vessel->emass*20.0);
		tdvtx[i].damping   = tdvtx[i].stiffness*0.1;
		tdvtx[i].mu        = vessel->mu;
		tdvtx[i].mu_lng    = vessel->mu_lng;
	}
	vessel->SetTouchdownPoints (tdvtx, 3);
}

void VESSEL::SetTouchdownPoints (const TOUCHDOWNVTX *tdvtx, DWORD ntdvtx) const
{
	dCHECK(ntdvtx >= 3, "VESSEL::SetTouchdownPoints: at least 3 points must be provided")
	vessel->SetTouchdownPoints (tdvtx, ntdvtx);
}

void VESSEL::GetTouchdownPoints (VECTOR3 &pt1, VECTOR3 &pt2, VECTOR3 &pt3) const
{
	CopyVector (vessel->touchdown_vtx[0].pos, pt1);
	CopyVector (vessel->touchdown_vtx[1].pos, pt2);
	CopyVector (vessel->touchdown_vtx[2].pos, pt3);
}

bool VESSEL::GetTouchdownPoint (TOUCHDOWNVTX &tdvtx, DWORD idx) const
{
	if (idx < vessel->ntouchdown_vtx) {
		memcpy(&tdvtx, vessel->touchdown_vtx+idx, sizeof(TOUCHDOWNVTX));
		return true;
	} else return false;
}

DWORD VESSEL::GetTouchdownPointCount () const
{
	return vessel->ntouchdown_vtx;
}

void VESSEL::DefSetState (const VESSELSTATUS *status) const
{
	vessel->SetState (*status);
}

void VESSEL::DefSetStateEx (const void *status) const
{
	vessel->SetStateEx (status);
}

void VESSEL::ParseScenarioLine (char *line, VESSELSTATUS *status) const
{
	vessel->ParseScenarioLine (line, *status);
}

void VESSEL::ParseScenarioLineEx (char *line, void *status) const
{
	vessel->ParseScenarioLineEx (line, status);
}

void VESSEL::ClearMeshes () const
{
	vessel->ClearMeshes (true);
}

void VESSEL::ClearMeshes (bool retain_anim) const
{
	vessel->ClearMeshes (retain_anim);
}

UINT VESSEL::AddMesh (const char *meshname, const VECTOR3 *ofs) const
{
	if (ofs) {
		return vessel->AddMesh (meshname, ofs);
	} else
		return vessel->AddMesh (meshname);
}

UINT VESSEL::AddMesh (MESHHANDLE hMesh, const VECTOR3 *ofs) const
{
	if (ofs) {
		return vessel->AddMesh (hMesh, ofs);
	} else
		return vessel->AddMesh (hMesh);
}

UINT VESSEL::InsertMesh (const char *meshname, UINT idx, const VECTOR3 *ofs) const
{
	if (ofs) {
		return vessel->InsertMesh (meshname, idx, ofs);
	} else
		return vessel->InsertMesh (meshname, idx);
}

UINT VESSEL::InsertMesh (MESHHANDLE hMesh, UINT idx, const VECTOR3 *ofs) const
{
	if (ofs) {
		return vessel->InsertMesh (hMesh, idx, ofs);
	} else
		return vessel->InsertMesh (hMesh, idx);
}

bool VESSEL::DelMesh (UINT idx, bool retain_anim) const
{
	return vessel->DelMesh (idx, retain_anim);
}

bool VESSEL::ShiftMesh (UINT idx, const VECTOR3 &ofs) const
{
	return vessel->ShiftMesh (idx, ofs);
}

void VESSEL::ShiftMeshes (const VECTOR3 &ofs) const
{
	for (UINT i = 0; i < vessel->nmesh; i++)
		vessel->ShiftMesh (i, ofs);
}

bool VESSEL::GetMeshOffset (UINT idx, VECTOR3 &ofs) const
{
	if (idx >= vessel->nmesh) return false;
	memcpy (&ofs, &vessel->meshlist[idx]->meshofs, sizeof(VECTOR3));
	return true;
}

UINT VESSEL::GetMeshCount () const
{
	return vessel->nmesh;
}

MESHHANDLE VESSEL::GetMesh (VISHANDLE vis, UINT idx) const
{
	return NULL;
}

DEVMESHHANDLE VESSEL::GetDevMesh (VISHANDLE vis, UINT idx) const
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	return (gc ? (DEVMESHHANDLE)gc->clbkGetMesh (vis, idx) : NULL);
}

const MESHHANDLE VESSEL::GetMeshTemplate (UINT idx) const
{
	return vessel->GetMeshTemplate (idx);
}

const char *VESSEL::GetMeshName (UINT idx) const
{
	return vessel->GetMeshName (idx);
}

MESHHANDLE VESSEL::CopyMeshFromTemplate (UINT idx) const
{
	return (MESHHANDLE)vessel->CopyMeshFromTemplate (idx);
}

WORD VESSEL::GetMeshVisibilityMode (UINT idx) const
{
	return (idx < vessel->nmesh ? vessel->meshlist[idx]->vismode : 0);
}

void VESSEL::SetMeshVisibilityMode (UINT idx, WORD mode) const
{
	vessel->SetMeshVisibilityMode (idx, mode);
}

void VESSEL::SetVisualProperty(VISHANDLE vis, VisualProp prp, int idx, const type_info& t, const void* val)
{
	oapi::GraphicsClient* gc = g_pOrbiter->GetGraphicsClient();
	gc->clbkSetVisualProperty(vis, prp, idx, t, val);
}

void VESSEL::SetMeshVisibleInternal (UINT idx, bool visible) const
{
	vessel->SetMeshVisibilityMode (idx, visible ? MESHVIS_ALWAYS : MESHVIS_EXTERNAL);
}

int VESSEL::MeshModified (MESHHANDLE hMesh, UINT grp, DWORD modflag)
{
	return vessel->MeshModified (hMesh, grp, modflag);
}

UINT VESSEL::AddExhaustRef (EXHAUSTTYPE exh, VECTOR3 &pos, double lscale, double wscale, VECTOR3 *dir) const
{
	ThrustSpec *th;
	UINT iexh = (UINT)exh;

	switch (exh) {
	case EXHAUST_MAIN:
		if (!vessel->m_thrusterGroupDef[THGROUP_MAIN].ts.size()) return 0; // not defined
		th = vessel->m_thrusterGroupDef[THGROUP_MAIN].ts[0];
		break;
	case EXHAUST_RETRO:
		if (!vessel->m_thrusterGroupDef[THGROUP_RETRO].ts.size()) return 0; // not defined
		th = vessel->m_thrusterGroupDef[THGROUP_RETRO].ts[0];
		break;
	case EXHAUST_HOVER:
		if (!vessel->m_thrusterGroupDef[THGROUP_HOVER].ts.size()) return 0; // not defined
		th = vessel->m_thrusterGroupDef[THGROUP_HOVER].ts[0];
		break;
	default: return 0;
	}
	//VECTOR3 p = {pos.x, pos.y, pos.z};
	//Vector d = -(dir ? Vector(dir->x, dir->y, dir->z) : DefExhaustDir[iexh]);

	EXHAUSTSPEC es = {(THRUSTER_HANDLE)th, NULL, &pos, dir ? dir : DefExhaustDir+iexh, lscale, wscale, 0, 0, NULL, EXHAUST_CONSTANTPOS|EXHAUST_CONSTANTDIR};
	return vessel->AddExhaust (&es);
	//return vessel->AddExhaust (th, lscale, wscale, 0, &p, &d);
}

void VESSEL::DelExhaustRef (EXHAUSTTYPE exh, WORD id) const
{
#ifdef UNDEF
	UINT i, j, iexh = (UINT)exh;
	UINT &neng = vessel->neng[iexh];
	ExhaustSpec *spec = vessel->eng_spec[iexh];
	for (i = 0; i < neng; i++) {
		if (spec[i].id == id) {
			for (j = i+1; j < neng; j++)
				spec[j-1] = spec[j];
			neng--;
			return;
		}
	}
	MessageBeep (-1); // should not get here!
#endif
}

void VESSEL::ClearExhaustRefs (void) const
{
	vessel->ClearExhaustDefinitions();
}

void VESSEL::SetExhaustScales (EXHAUSTTYPE exh, WORD id, double lscale, double wscale) const
{
#ifdef UNDEF
	int i, iexh = (int)exh;
	int neng = vessel->neng[iexh];
	ExhaustSpec *spec = vessel->eng_spec[iexh];
	for (i = 0; i < neng; i++) {
		if (spec[i].id == id) {
			spec[i].lscale = lscale;
			spec[i].wscale = wscale;
			return;
		}
	}
#endif
}

void VESSEL::SetReentryTexture (SURFHANDLE tex, double plimit, double lscale, double wscale) const
{
	vessel->reentry.plimit = (float)plimit;
	vessel->reentry.lscale = (float)lscale;
	vessel->reentry.wscale = (float)wscale;
	vessel->reentry.tex = tex;
	vessel->reentry.do_render = (tex != NULL);
}

UINT VESSEL::AddAttExhaustRef (const VECTOR3 &pos, const VECTOR3 &dir, double wscale, double lscale) const
{
	DWORD n = vessel->noexhaust;
	OldExhaustSpec **tmp = new OldExhaustSpec*[n+1]; TRACENEW
	if (n) {
		memcpy (tmp, vessel->oexhaust, n*sizeof(OldExhaustSpec*));
		delete []vessel->oexhaust;
		vessel->oexhaust = NULL;
	}
	tmp[n] = new OldExhaustSpec; TRACENEW
	tmp[n]->ref = MakeVector(pos);
	tmp[n]->dir = -MakeVector(dir);
	tmp[n]->lscale = 3.0*lscale;
	tmp[n]->wscale = 0.387*wscale;
	vessel->oexhaust = tmp;
	return vessel->noexhaust++;
}

void VESSEL::AddAttExhaustMode (UINT idx, ATTITUDEMODE mode, int axis, int dir) const
{
	ThrustGroupSpec *tgs;
	switch (mode) {
	case ATTMODE_ROT:
		switch (axis) {
		case 0:
			tgs = &vessel->m_thrusterGroupDef[dir ? THGROUP_ATT_PITCHDOWN : THGROUP_ATT_PITCHUP];
			break;
		case 1:
			tgs = &vessel->m_thrusterGroupDef[dir ? THGROUP_ATT_YAWRIGHT : THGROUP_ATT_YAWLEFT];
			break;
		case 2:
			tgs = &vessel->m_thrusterGroupDef[dir ? THGROUP_ATT_BANKLEFT : THGROUP_ATT_BANKRIGHT];
			break;
		}
		break;
	case ATTMODE_LIN:
		switch (axis) {
		case 0:
			tgs = &vessel->m_thrusterGroupDef[dir ? THGROUP_ATT_LEFT : THGROUP_ATT_RIGHT];
			break;
		case 1:
			tgs = &vessel->m_thrusterGroupDef[dir ? THGROUP_ATT_DOWN : THGROUP_ATT_UP];
			break;
		case 2:
			tgs = &vessel->m_thrusterGroupDef[dir ? THGROUP_ATT_BACK : THGROUP_ATT_FORWARD];
			break;
		}
		break;
	}
	if (idx >= vessel->noexhaust) return;
	OldExhaustSpec *oes = vessel->oexhaust[idx];
	if (tgs->ts.size()) {
		VECTOR3 ref = {oes->ref.x, oes->ref.y, oes->ref.z};
		VECTOR3 dir = {oes->dir.x, oes->dir.y, oes->dir.z};
		EXHAUSTSPEC es = {(THRUSTER_HANDLE)tgs->ts[0], NULL, &ref, &dir, oes->lscale, oes->wscale, 0, 0, NULL, EXHAUST_CONSTANTPOS|EXHAUST_CONSTANTDIR};
		vessel->AddExhaust (&es);
		//vessel->AddExhaust (tgs->ts[0], oes->lscale, oes->wscale, 0, &oes->ref, &oes->dir, 0);
	}
}

void VESSEL::ClearAttExhaustRefs (void) const
{
#ifdef UNDEF
	vessel->ClearAttExhaustRefs();
#endif
}

void VESSEL::ShiftCentreOfMass (const VECTOR3 &shift)
{
	if (vessel->bFRrecord) vessel->FRecorder_Save (true);
	Vector dr (shift.x, shift.y, shift.z);
	Vector gdr (mul (vessel->GRot(), dr));
	vessel->FlushRPos ();
	if (!vessel->bFRplayback) {  // ignore during playback
		vessel->AddRPos (gdr);
		if (vessel->supervessel) vessel->supervessel->NotifyShiftVesselOrigin (vessel, dr);
	}
	g_pOrbiter->NotifyObjectJump (vessel, gdr);
	if (vessel->bFRrecord) vessel->FRecorder_Save (true);
}

void VESSEL::ShiftCG (const VECTOR3 &shift)
{
	VECTOR3 nshift = -shift;
	Vector vs = MakeVector (nshift);
	ShiftMeshes (nshift);
	vessel->ShiftThrusters (vs);
	vessel->ShiftAttachments (vs);
	vessel->ShiftDocks (vs);
	vessel->ShiftLightEmitters (nshift);
	vessel->campos.Set (vessel->campos+vs);
	// only shift the vc of this vessel
	if ((g_pane) && (g_focusobj == vessel)) g_pane->ShiftVC (vs);
	ShiftCentreOfMass (shift);
}

void VESSEL::GetRotationMatrix (MATRIX3 &R) const
{
	memcpy (R.data, vessel->s0->R.data, 9*sizeof(double));
}

void VESSEL::SetRotationMatrix (const MATRIX3 &R) const
{
	Matrix r;
	memcpy (r.data, R.data, 9*sizeof(double));
	vessel->SetRotationMatrix (r);
}

void VESSEL::GlobalRot (const VECTOR3 &rloc, VECTOR3 &rglob) const
{
	Vector loc(rloc.x, rloc.y, rloc.z);
	Vector glob (mul (vessel->GRot(), loc));
	rglob.x = glob.x;
	rglob.y = glob.y;
	rglob.z = glob.z;
}

void VESSEL::HorizonRot (const VECTOR3 &rloc, VECTOR3 &rhorizon) const
{
	if (!vessel->proxyplanet) return;
	Vector h (mul (vessel->sp.L2H, tmul (vessel->proxyplanet->GRot(), mul (vessel->GRot(), MakeVector(rloc)))));
	rhorizon = _V(h.x, h.y, h.z);
}

void VESSEL::HorizonInvRot (const VECTOR3 &rhorizon, VECTOR3 &rloc) const
{
	if (!vessel->proxyplanet) return;
	Vector r (tmul (vessel->GRot(), mul (vessel->proxyplanet->GRot(), tmul (vessel->sp.L2H, MakeVector (rhorizon)))));
	rloc = _V(r.x, r.y, r.z);
}

void VESSEL::Local2Global (const VECTOR3 &local, VECTOR3 &global) const
{
	Vector loc(local.x, local.y, local.z);
	Vector glob (mul (vessel->GRot(), loc) + vessel->GPos());
	global.x = glob.x;
	global.y = glob.y;
	global.z = glob.z;
}

void VESSEL::Global2Local (const VECTOR3 &global, VECTOR3 &local) const
{
	Vector glob(global.x, global.y, global.z);
	Vector loc (tmul (vessel->GRot(), glob - vessel->GPos()));
	local.x = loc.x;
	local.y = loc.y;
	local.z = loc.z;
}

void VESSEL::Local2Rel (const VECTOR3 &local, VECTOR3 &rel) const
{
	Vector loc(local.x, local.y, local.z);
	Vector r (mul (vessel->GRot(), loc) + vessel->GPos() - vessel->cbody->GPos());
	rel.x = r.x;
	rel.y = r.y;
	rel.z = r.z;
}

void VESSEL::RegisterAnimation (void) const
{
	vessel->animcount++;
}

void VESSEL::UnregisterAnimation (void) const
{
	if (vessel->animcount > 0)
		vessel->animcount--;
}

UINT VESSEL::RegisterAnimSequence (double defmeshstate) const
{
	return vessel->AddAnimSeq (defmeshstate);
}

bool VESSEL::AddAnimComp (UINT seq, ANIMCOMP *comp)
{
	return vessel->AddAnimSeqComp (seq, comp);
}

bool VESSEL::SetAnimState (UINT seq, double state)
{
	return vessel->SetAnimState (seq, state);
}

UINT VESSEL::CreateAnimation (double initial_state) const
{
	return vessel->CreateAnimation (initial_state);
}

bool VESSEL::DelAnimation (UINT anim) const
{
	return vessel->DelAnimation (anim);
}

ANIMATIONCOMPONENT_HANDLE VESSEL::AddAnimationComponent (UINT anim, double state0, double state1, MGROUP_TRANSFORM *trans, ANIMATIONCOMPONENT_HANDLE parent) const
{
	return (ANIMATIONCOMPONENT_HANDLE)vessel->AddAnimationComponent (anim, state0, state1, trans, (ANIMATIONCOMP*)parent);
}

bool VESSEL::DelAnimationComponent (UINT anim, ANIMATIONCOMPONENT_HANDLE hAC)
{
	return vessel->DelAnimationComponent (anim, (ANIMATIONCOMP*)hAC);
}

bool VESSEL::SetAnimation (UINT anim, double state) const
{
	return vessel->SetAnimation (anim, state);
}

double VESSEL::GetAnimation (UINT anim) const
{
	return vessel->GetAnimation (anim);
}

UINT VESSEL::GetAnimPtr (ANIMATION **anim) const
{
	*anim = vessel->anim;
	return vessel->nanim;
}

SUPERVESSELHANDLE VESSEL::GetSupervessel () const
{
	return (SUPERVESSELHANDLE)vessel->supervessel;
}

VECTOR3 VESSEL::GetSupervesselCG () const
{
	VECTOR3 cg = _V(0,0,0);
	if (vessel->supervessel) {
		Vector vcg;
		if (vessel->supervessel->GetCG (vessel, vcg))
			cg = MakeVECTOR3 (vcg);
	}
	return cg;
}

bool VESSEL::Recording (void) const
{
	return vessel->bFRrecord;
}

bool VESSEL::Playback (void) const
{
	return vessel->bFRplayback;
}

void VESSEL::RecordEvent (const char *event_type, const char *event) const
{
	if (vessel->bFRrecord) vessel->FRecorder_SaveEvent (event_type, event);
}

void VESSEL::SaveDefaultState (FILEHANDLE scn) const
{
	vessel->WriteDefault (*(ostream*)scn);
}

void VESSEL::AddForce (const VECTOR3 &F, const VECTOR3 &r) const
{
	vessel->AddForce (MakeVector(F), MakeVector(r));
}

PROPELLANT_HANDLE VESSEL::CreatePropellantResource (double maxmass, double mass, double efficiency) const
{
	return (PROPELLANT_HANDLE)vessel->CreatePropellantResource (maxmass, mass, efficiency);
}

void VESSEL::DelPropellantResource (PROPELLANT_HANDLE &ph) const
{
	vessel->DelPropellantResource ((TankSpec*)ph);
	ph = NULL;
}

void VESSEL::ClearPropellantResources () const
{
	vessel->ClearPropellantResources();
}

PROPELLANT_HANDLE VESSEL::GetPropellantHandleByIndex (DWORD idx) const
{
	if (idx >= vessel->ntank) return 0;
	return (PROPELLANT_HANDLE) vessel->tank[idx];
}

DWORD VESSEL::GetPropellantCount () const
{
	return vessel->ntank;
}

void VESSEL::SetDefaultPropellantResource (PROPELLANT_HANDLE ph) const
{
	vessel->SetDefaultPropellant ((TankSpec*)ph);
}

PROPELLANT_HANDLE VESSEL::GetDefaultPropellantResource () const
{
	return (PROPELLANT_HANDLE)vessel->DefaultPropellantHandle();
}

void VESSEL::SetPropellantMaxMass (PROPELLANT_HANDLE ph, double maxmass) const
{
	vessel->SetPropellantMaxMass ((TankSpec*)ph, maxmass);
}

void VESSEL::SetPropellantEfficiency (PROPELLANT_HANDLE ph, double efficiency) const
{
	((TankSpec*)ph)->efficiency = efficiency;
}

void VESSEL::SetPropellantMass (PROPELLANT_HANDLE ph, double mass) const
{
	((TankSpec*)ph)->mass = mass;
	vessel->UpdateMass ();
}

double VESSEL::GetPropellantMaxMass (PROPELLANT_HANDLE ph) const
{
	return ((TankSpec*)ph)->maxmass;
}

double VESSEL::GetPropellantEfficiency (PROPELLANT_HANDLE ph) const
{
	return ((TankSpec*)ph)->efficiency;
}

double VESSEL::GetPropellantMass (PROPELLANT_HANDLE ph) const
{
	return ((TankSpec*)ph)->mass;
}

double VESSEL::GetPropellantFlowrate (PROPELLANT_HANDLE ph) const
{
	return vessel->GetPropellantFlowrate ((TankSpec*)ph);
}

double VESSEL::GetTotalPropellantMass () const
{
	return vessel->fmass;
}

double VESSEL::GetTotalPropellantFlowrate () const
{
	return (vessel->pfmass-vessel->fmass)*td.iSimDT0;
}

THRUSTER_HANDLE VESSEL::CreateThruster (const VECTOR3 &pos, const VECTOR3 &dir, double maxth0,
	PROPELLANT_HANDLE hp, double isp0, double isp_ref, double p_ref) const
{
	return (THRUSTER_HANDLE)vessel->CreateThruster (MakeVector(pos), MakeVector(dir),
		maxth0, (TankSpec*)hp, isp0, isp_ref, p_ref);
}

bool VESSEL::DelThruster (THRUSTER_HANDLE &th) const
{
	bool ok = vessel->DelThruster ((ThrustSpec*)th);
	th = NULL;
	return ok;
}

void VESSEL::ClearThrusterDefinitions () const
{
	vessel->ClearThrusterDefinitions ();
}

THRUSTER_HANDLE VESSEL::GetThrusterHandleByIndex (DWORD idx) const
{
	return (THRUSTER_HANDLE)(idx < vessel->m_thruster.size() ? vessel->m_thruster[idx] : nullptr);
}

DWORD VESSEL::GetThrusterCount (void) const
{
	return (DWORD)vessel->m_thruster.size();
}

void VESSEL::SetThrusterRef (THRUSTER_HANDLE th, const VECTOR3 &pos) const
{
	((ThrustSpec*)th)->ref = pos;
}

void VESSEL::GetThrusterRef (THRUSTER_HANDLE th, VECTOR3 &pos) const
{
	pos.x = ((ThrustSpec*)th)->ref.x;
	pos.y = ((ThrustSpec*)th)->ref.y;
	pos.z = ((ThrustSpec*)th)->ref.z;
}

void VESSEL::SetThrusterDir (THRUSTER_HANDLE th, const VECTOR3 &dir) const
{
	((ThrustSpec*)th)->dir = dir;
}

void VESSEL::GetThrusterDir (THRUSTER_HANDLE th, VECTOR3 &dir) const
{
	dir.x = ((ThrustSpec*)th)->dir.x;
	dir.y = ((ThrustSpec*)th)->dir.y;
	dir.z = ((ThrustSpec*)th)->dir.z;
}

void VESSEL::SetThrusterMax0 (THRUSTER_HANDLE th, double maxth0) const
{
	vessel->SetThrusterMax0 ((ThrustSpec*)th, maxth0);
	//((ThrustSpec*)th)->maxth0 = maxth0;
}

double VESSEL::GetThrusterMax0 (THRUSTER_HANDLE th) const
{
	return ((ThrustSpec*)th)->maxth0;
}

double VESSEL::GetThrusterMax (THRUSTER_HANDLE th) const
{
	ThrustSpec *ts = (ThrustSpec*)th;
	return ts->maxth0 * vessel->ThrusterAtmScale (ts, vessel->sp.atmp);
}

double VESSEL::GetThrusterMax (THRUSTER_HANDLE th, double p_ref) const
{
	ThrustSpec *ts = (ThrustSpec*)th;
	return ts->maxth0 * vessel->ThrusterAtmScale (ts, p_ref);
}

void VESSEL::SetThrusterIsp (THRUSTER_HANDLE th, double isp) const
{
	ThrustSpec *ts = (ThrustSpec*)th;
	ts->isp0 = isp;
	ts->pfac = 0.0;  // no pressure dependence
}

void VESSEL::SetThrusterIsp (THRUSTER_HANDLE th, double isp0, double isp_ref, double p_ref) const
{
	ThrustSpec *ts = (ThrustSpec*)th;
	ts->isp0 = isp0;
	ts->pfac = (isp0-isp_ref)/(p_ref*isp0);
}

double VESSEL::GetThrusterIsp (THRUSTER_HANDLE th) const
{
	ThrustSpec *ts = (ThrustSpec*)th;
	return ts->isp0 * vessel->ThrusterAtmScale (ts, vessel->sp.atmp);
}

double VESSEL::GetThrusterIsp (THRUSTER_HANDLE th, double p_ref) const
{
	ThrustSpec *ts = (ThrustSpec*)th;
	return ts->isp0 * vessel->ThrusterAtmScale (ts, p_ref);
}

double VESSEL::GetThrusterIsp0 (THRUSTER_HANDLE th) const
{
	return ((ThrustSpec*)th)->isp0;
}

void VESSEL::SetThrusterResource (THRUSTER_HANDLE th, PROPELLANT_HANDLE ph) const
{
	((ThrustSpec*)th)->tank = (TankSpec*)ph;
	if (!ph) SetThrusterLevel (th, 0);
}

PROPELLANT_HANDLE VESSEL::GetThrusterResource (THRUSTER_HANDLE th) const
{
	return (PROPELLANT_HANDLE)((ThrustSpec*)th)->tank;
}

void VESSEL::SetThrusterLevel (THRUSTER_HANDLE th, double level) const
{
	if (vessel->bFRplayback) return;
	ThrustSpec *ts = (ThrustSpec*)th;
	ts->level_permanent = level;
	if (ts->tank && ts->tank->mass)
		ts->level = min (1.0, ts->level_permanent + ts->level_override);
}

void VESSEL::IncThrusterLevel (THRUSTER_HANDLE th, double dlevel) const
{
	if (vessel->bFRplayback) return;
	ThrustSpec *ts = (ThrustSpec*)th;
	ts->level_permanent += dlevel;
	ts->level_permanent = max (0.0, min (1.0, ts->level_permanent));
	if (ts->tank && ts->tank->mass)
		ts->level = max (0.0, min (1.0, ts->level_permanent + ts->level_override));
}

void VESSEL::SetThrusterLevel_SingleStep (THRUSTER_HANDLE th, double level) const
{
	if (vessel->bFRplayback) return;
	ThrustSpec *ts = (ThrustSpec*)th;
	ts->level_override = ts->level = max (0.0, min (1.0, level));
}

void VESSEL::IncThrusterLevel_SingleStep (THRUSTER_HANDLE th, double dlevel) const
{
	if (vessel->bFRplayback) return;
	ThrustSpec *ts = (ThrustSpec*)th;
	ts->level_override += dlevel;
	if (ts->tank && ts->tank->mass)
		ts->level = min (1.0, ts->level_permanent + ts->level_override);
}

double VESSEL::GetThrusterLevel (THRUSTER_HANDLE th) const
{
	return ((ThrustSpec*)th)->level;
}

void VESSEL::GetThrusterMoment (THRUSTER_HANDLE th, VECTOR3 &F, VECTOR3 &T) const
{
	Vector f, t;
	vessel->GetThrusterMoment ((ThrustSpec*)th, f, t);
	F.x = f.x;
	F.y = f.y;
	F.z = f.z;
	T.x = t.x;
	T.y = t.y;
	T.z = t.z;
}

THGROUP_HANDLE VESSEL::CreateThrusterGroup (THRUSTER_HANDLE *th, int nth, THGROUP_TYPE thgt) const
{
	return (THGROUP_HANDLE)vessel->CreateThrusterGroup ((ThrustSpec**)th, nth, thgt);
}

// OBSOLETE
bool VESSEL::DelThrusterGroup (THGROUP_HANDLE &thg, THGROUP_TYPE thgt, bool delth) const
{
	if (thg != (THGROUP_HANDLE)vessel->GetThrusterGroup(thgt)) return false;
	return DelThrusterGroup(thg);
}

bool VESSEL::DelThrusterGroup (THGROUP_HANDLE thg, bool delth) const
{
	return vessel->DeleteThrusterGroup ((ThrustGroupSpec*)thg, delth);
}

bool VESSEL::DelThrusterGroup (THGROUP_TYPE thgt, bool delth) const
{
	return vessel->DeleteThrusterGroup (thgt, delth);
}

THGROUP_HANDLE VESSEL::GetThrusterGroupHandle (THGROUP_TYPE thgt) const
{
	ThrustGroupSpec* tgs = vessel->GetThrusterGroup(thgt);
	return (THGROUP_HANDLE)tgs;
}

THGROUP_HANDLE VESSEL::GetUserThrusterGroupHandleByIndex (DWORD idx) const
{
	if (idx >= vessel->m_thrusterGroupUsr.size()) return 0;
	return (THGROUP_HANDLE)vessel->m_thrusterGroupUsr[idx];
}

DWORD VESSEL::GetGroupThrusterCount (THGROUP_HANDLE thg) const
{
	return ((ThrustGroupSpec*)thg)->ts.size();
}

DWORD VESSEL::GetGroupThrusterCount (THGROUP_TYPE thgt) const
{
	const ThrustGroupSpec* tgs = vessel->GetThrusterGroup(thgt);
	return (tgs ? tgs->ts.size() : 0);
}

THRUSTER_HANDLE VESSEL::GetGroupThruster (THGROUP_HANDLE thg, DWORD idx) const
{
	const ThrustGroupSpec *tgs = (const ThrustGroupSpec*)thg;
	dCHECK(tgs, "Invalid thruster group handle.")
	return (THRUSTER_HANDLE)(idx < tgs->ts.size() ? tgs->ts[idx] : nullptr);
}

THRUSTER_HANDLE VESSEL::GetGroupThruster (THGROUP_TYPE thgt, DWORD idx) const
{
	const ThrustGroupSpec* tgs = vessel->GetThrusterGroup(thgt);
	return (THRUSTER_HANDLE)(tgs && idx < tgs->ts.size() ? tgs->ts[idx] : nullptr);
}

DWORD VESSEL::GetUserThrusterGroupCount () const
{
	return vessel->m_thrusterGroupUsr.size();
}

bool VESSEL::ThrusterGroupDefined (THGROUP_TYPE thgt) const
{
	const ThrustGroupSpec* tgs = vessel->GetThrusterGroup(thgt);
	return (tgs && tgs->ts.size());
}

void VESSEL::SetThrusterGroupLevel (THGROUP_HANDLE thg, double level) const
{
	if      (level < 0.0) level = 0.0;
	else if (level > 1.0) level = 1.0;
	vessel->SetThrusterGroupLevel ((ThrustGroupSpec*)thg, level);
}

void VESSEL::SetThrusterGroupLevel (THGROUP_TYPE thgt, double level) const
{
	if      (level < 0.0) level = 0.0;
	else if (level > 1.0) level = 1.0;
	vessel->SetThrusterGroupLevel (thgt, level);
}

void VESSEL::IncThrusterGroupLevel (THGROUP_HANDLE thg, double dlevel) const
{
	vessel->IncThrusterGroupLevel ((ThrustGroupSpec*)thg, dlevel);
}

void VESSEL::IncThrusterGroupLevel (THGROUP_TYPE thgt, double dlevel) const
{
	vessel->IncThrusterGroupLevel (thgt, dlevel);
}

void VESSEL::IncThrusterGroupLevel_SingleStep (THGROUP_HANDLE thg, double dlevel) const
{
	vessel->IncThrusterGroupOverride ((ThrustGroupSpec*)thg, dlevel);
}

void VESSEL::IncThrusterGroupLevel_SingleStep (THGROUP_TYPE thgt, double dlevel) const
{
	vessel->IncThrusterGroupOverride (thgt, dlevel);
}

double VESSEL::GetThrusterGroupLevel (THGROUP_HANDLE thg) const
{
	return vessel->GetThrusterGroupLevel ((ThrustGroupSpec*)thg);
}

double VESSEL::GetThrusterGroupLevel (THGROUP_TYPE thgt) const
{
	return vessel->GetThrusterGroupLevel (thgt);
}

UINT VESSEL::AddExhaust (THRUSTER_HANDLE th, double lscale, double wscale, SURFHANDLE tex) const
{
	EXHAUSTSPEC es = {th, NULL, NULL, NULL, lscale, wscale, 0, 0, tex, 0};
	return vessel->AddExhaust (&es);
}

UINT VESSEL::AddExhaust (THRUSTER_HANDLE th, double lscale, double wscale, double lofs, SURFHANDLE tex) const
{
	EXHAUSTSPEC es = {th, NULL, NULL, NULL, lscale, wscale, lofs, 0, tex, 0};
	return vessel->AddExhaust (&es);
}

UINT VESSEL::AddExhaust (THRUSTER_HANDLE th, double lscale, double wscale, const VECTOR3 &pos, const VECTOR3 &dir, SURFHANDLE tex) const
{
	VECTOR3 p = {pos.x, pos.y, pos.z};
	VECTOR3 d = {-dir.x, -dir.y, -dir.z};
	EXHAUSTSPEC es = {th, NULL, &p, &d, lscale, wscale, 0, 0, tex, EXHAUST_CONSTANTPOS|EXHAUST_CONSTANTDIR};
	return vessel->AddExhaust (&es);
}

UINT VESSEL::AddExhaust (EXHAUSTSPEC *spec)
{
	return vessel->AddExhaust (spec);
}

bool VESSEL::DelExhaust (UINT idx) const
{
	return vessel->DelExhaust (idx);
}

DWORD VESSEL::GetExhaustCount () const
{
	return vessel->nexhaust;
}

bool VESSEL::GetExhaustSpec (UINT idx, double *lscale, double *wscale, VECTOR3 *pos, VECTOR3 *dir, SURFHANDLE *tex) const
{
	if (idx < 0 || idx >= vessel->nexhaust) return false;
	EXHAUSTSPEC *es = vessel->exhaust[idx];
	*lscale = es->lsize;
	*wscale = es->wsize;
	*pos = *es->lpos;
	*dir = *es->ldir;
	if (es->lofs) {
		pos->x -= dir->x*es->lofs;
		pos->y -= dir->y*es->lofs;
		pos->z -= dir->z*es->lofs;
	}
	*tex = es->tex;
	return true;
}

bool VESSEL::GetExhaustSpec (UINT idx, EXHAUSTSPEC *spec)
{
	if (idx < 0 || idx >= vessel->nexhaust) return false;
	memcpy (spec, vessel->exhaust[idx], sizeof(EXHAUSTSPEC));
	return true;
}

double VESSEL::GetExhaustLevel (UINT idx) const
{
	if (idx < 0 || idx >= vessel->nexhaust) return 0.0;
	return *vessel->exhaust[idx]->level;
}

PSTREAM_HANDLE VESSEL::AddParticleStream (PARTICLESTREAMSPEC *pss, const VECTOR3 &pos, const VECTOR3 &dir, double *lvl) const
{
	Vector p (pos.x, pos.y, pos.z);
	Vector d (dir.x, dir.y, dir.z);
	return (PSTREAM_HANDLE)vessel->AddParticleStream (pss, p, d, lvl);
}

PSTREAM_HANDLE VESSEL::AddExhaustStream (THRUSTER_HANDLE th, PARTICLESTREAMSPEC *pss) const
{
	return (PSTREAM_HANDLE)vessel->AddExhaustStream ((ThrustSpec*)th, pss);
}

PSTREAM_HANDLE VESSEL::AddExhaustStream (THRUSTER_HANDLE th, const VECTOR3 &pos, PARTICLESTREAMSPEC *pss) const
{
	Vector p (pos.x, pos.y, pos.z);
	return (PSTREAM_HANDLE)vessel->AddExhaustStream ((ThrustSpec*)th, pss, &p);
}

bool VESSEL::DelExhaustStream (PSTREAM_HANDLE ch) const
{
	return vessel->DelExhaustStream ((oapi::ParticleStream*)ch);
}

PSTREAM_HANDLE VESSEL::AddReentryStream (PARTICLESTREAMSPEC *pss) const
{
	return (PSTREAM_HANDLE)vessel->AddReentryStream (pss);
}

// Airfoil functions

void VESSEL::CreateAirfoil (AIRFOIL_ORIENTATION align, const VECTOR3 &ref, AirfoilCoeffFunc cf, double c, double S, double A) const
{
	Vector r(MakeVector(ref));
	vessel->CreateAirfoil (align, r, cf, c, S, A);
}

AIRFOILHANDLE VESSEL::CreateAirfoil2 (AIRFOIL_ORIENTATION align, const VECTOR3 &ref, AirfoilCoeffFunc cf, double c, double S, double A) const
{
	Vector r(MakeVector(ref));
	return (AIRFOILHANDLE)vessel->CreateAirfoil (align, r, cf, c, S, A);
}

AIRFOILHANDLE VESSEL::CreateAirfoil3 (AIRFOIL_ORIENTATION align, const VECTOR3 &ref, AirfoilCoeffFuncEx cf, void *context, double c, double S, double A) const
{
	Vector r(MakeVector(ref));
	return (AIRFOILHANDLE)vessel->CreateAirfoil (align, r, cf, context, c, S, A);
}

AIRFOILHANDLE VESSEL::CreateAirfoil4 (const VECTOR3& ref, AirfoilCoeffFuncEx2 cf, void* context, double c, double S, double A) const
{
	Vector r(MakeVector(ref));
	return (AIRFOILHANDLE)vessel->CreateAirfoil(FORCE_AND_MOMENT, r, cf, context, c, S, A);
}

bool VESSEL::GetAirfoilParam (AIRFOILHANDLE hAirfoil, VECTOR3 *ref, AirfoilCoeffFunc *cf, void **context, double *c, double *S, double *A) const
{
	return vessel->GetAirfoilParam ((AirfoilSpec*)hAirfoil, ref, cf, context, c, S, A);
}

void VESSEL::EditAirfoil (AIRFOILHANDLE hAirfoil, DWORD flag, const VECTOR3 &ref, AirfoilCoeffFunc cf, double c, double S, double A) const
{
	vessel->EditAirfoil ((AirfoilSpec*)hAirfoil, flag, MakeVector(ref), cf, c, S, A);
}

bool VESSEL::DelAirfoil (AIRFOILHANDLE hAirfoil) const
{
	return vessel->DelAirfoil ((AirfoilSpec*)hAirfoil);
}

void VESSEL::ClearAirfoilDefinitions () const
{
	vessel->ClearAirfoilDefinitions();
}

void VESSEL::CreateControlSurface (AIRCTRL_TYPE type, double area, double dCl, const VECTOR3 &ref, int axis, UINT anim) const
{
	vessel->CreateControlSurface (type, area, dCl, MakeVector(ref), axis, 1.0, anim);
}

CTRLSURFHANDLE VESSEL::CreateControlSurface2 (AIRCTRL_TYPE type, double area, double dCl, const VECTOR3 &ref, int axis, UINT anim) const
{
	return vessel->CreateControlSurface (type, area, dCl, MakeVector(ref), axis, 1.0, anim);
}

CTRLSURFHANDLE VESSEL::CreateControlSurface3 (AIRCTRL_TYPE type, double area, double dCl, const VECTOR3 &ref, int axis, double delay, UINT anim) const
{
	return vessel->CreateControlSurface (type, area, dCl, MakeVector(ref), axis, delay, anim);
}

bool VESSEL::DelControlSurface (CTRLSURFHANDLE hCtrlSurf) const
{
	return vessel->DelControlSurface ((CtrlsurfSpec*)hCtrlSurf);
}

void VESSEL::ClearControlSurfaceDefinitions () const
{
	vessel->ClearControlSurfaceDefinitions();
}

void VESSEL::SetControlSurfaceLevel (AIRCTRL_TYPE type, double level) const
{
	vessel->SetControlSurfaceLevel (type, min (1.0, max (-1.0, level)), false);
}

void VESSEL::SetControlSurfaceLevel (AIRCTRL_TYPE type, double level, bool direct) const
{
	vessel->SetControlSurfaceLevel (type, min (1.0, max (-1.0, level)), false, direct);
}

double VESSEL::GetControlSurfaceLevel (AIRCTRL_TYPE type) const
{
	return vessel->GetControlSurfaceLevel (type);
}

void VESSEL::CreateVariableDragElement (double *drag, double factor, const VECTOR3 &ref) const
{
	LOGOUT_OBSOLETE;
	vessel->CreateVariableDragElement (drag, factor, MakeVector(ref));
}

void VESSEL::CreateVariableDragElement (const double *drag, double factor, const VECTOR3 &ref) const
{
	vessel->CreateVariableDragElement (drag, factor, MakeVector(ref));
}

void VESSEL::ClearVariableDragElements () const
{
	vessel->ClearVariableDragElements ();
}

void VESSEL::SetNosewheelSteering (bool activate) const
{
	vessel->nosesteering = activate;
}

bool VESSEL::GetNosewheelSteering () const
{
	return vessel->nosesteering;
}

void VESSEL::SetSurfaceFrictionCoeff (double mu_lng, double mu_lat) const
{
	vessel->SetSurfaceFrictionCoeff (mu_lng, mu_lat);
}

void VESSEL::SetMaxWheelbrakeForce (double f) const
{
	vessel->max_wbrake_F = f;
}

void VESSEL::SetWheelbrakeLevel (double level, int which, bool permanent) const
{
	vessel->SetWBrakeLevel (level, which, permanent);
}

double VESSEL::GetWheelbrakeLevel (int which) const
{
	if (!which) {
		double w1 = vessel->wbrake[0]; //(vessel->wbrake_override[0] ? vessel->wbrake_override[0] : vessel->wbrake_permanent[0]);
		double w2 = vessel->wbrake[1]; //(vessel->wbrake_override[1] ? vessel->wbrake_override[0] : vessel->wbrake_permanent[1]);
		return 0.5*(w1+w2);
	} else {
		which--;
		return vessel->wbrake[which]; //(vessel->wbrake_override[which] ? vessel->wbrake_override[which] : vessel->wbrake_permanent[which]);
	}
}

void VESSEL::InitNavRadios (DWORD nnav) const
{
	vessel->InitNavRadios (nnav);
}

DWORD VESSEL::GetNavCount () const
{
	return vessel->nnav;
}

bool VESSEL::SetNavChannel (DWORD n, DWORD step) const
{
	return vessel->SetNavChannel (n, step);
}

DWORD VESSEL::GetNavChannel (DWORD n) const
{
	return vessel->GetNavChannel (n);
}

float VESSEL::GetNavRecvFreq (DWORD n) const
{
	return vessel->GetNavFreq (n);
}

bool VESSEL::SetTransponderChannel (DWORD ch) const
{
	return vessel->SetXpdrChannel (ch);
}

bool VESSEL::SetIDSChannel (DOCKHANDLE hDock, DWORD ch) const
{
	return vessel->SetIDSChannel ((PortSpec*)hDock, ch);
}

void VESSEL::EnableTransponder (bool enable) const
{
	if (enable) {
		if (!vessel->xpdr) { vessel->xpdr = new Nav_XPDR (vessel, NAV_RADIO_FREQ_MIN); TRACENEW }
	} else {
		if (vessel->xpdr) delete vessel->xpdr, vessel->xpdr = 0;
	}
}

void VESSEL::EnableIDS (DOCKHANDLE hDock, bool bEnable) const
{
	PortSpec *ps = (PortSpec*)hDock;
	if (bEnable) {
		if (!ps->ids) { ps->ids = new Nav_IDS (vessel, ps, NAV_RADIO_FREQ_MIN); TRACENEW }
	} else {
		g_psys->BroadcastVessel (MSG_KILLNAVSENDER, ps->ids);
		if (ps->ids) delete ps->ids, ps->ids = 0;
	}
}

NAVHANDLE VESSEL::GetTransponder () const
{
	return (NAVHANDLE)vessel->xpdr;
}

NAVHANDLE VESSEL::GetIDS (DOCKHANDLE hDock) const
{
	PortSpec *dock = (PortSpec*)hDock;
	return (NAVHANDLE)dock->ids;
}

NAVHANDLE VESSEL::GetNavSource (DWORD n) const
{
	return (n < vessel->nnav ? (NAVHANDLE)vessel->nav[n].sender : NULL);
}

bool VESSEL::GroundContact () const
{
	return vessel->bSurfaceContact;
}

bool VESSEL::OrbitStabilised () const
{
	return vessel->isOrbitStabilised();
}

bool VESSEL::NonsphericalGravityEnabled () const
{
	return vessel->bGPerturb;
}

int VESSEL::SendBufferedKey (DWORD key, bool down, char *kstate)
{
	static char def_kstate[256] = {0};
	if (!kstate) kstate = def_kstate;
	return vessel->ConsumeBufferedKey (key, down, kstate);
}

bool VESSEL::GetEditorModule (char *fname) const
{
	return vessel->EditorModule (fname);
}

bool VESSEL::MeshgroupTransform (VISHANDLE vis, const MESHGROUP_TRANSFORM &mt) const
{
	return false; // not supported yet
}

// ==============================================================
// Obsolete VESSEL methods

bool VESSEL::SetNavRecv (DWORD n, DWORD step) const
{
	LOGOUT_OBSOLETE;
	return vessel->SetNavChannel (n, step);
}

DWORD VESSEL::GetNavRecv (DWORD n) const
{
	LOGOUT_OBSOLETE;
	return vessel->GetNavChannel (n);
}

// =======================================================================
// =======================================================================
// class VESSEL2: module interface to vessel class (extended)

VESSEL2::VESSEL2 (OBJHANDLE hVessel, int fmodel): VESSEL (hVessel, fmodel)
{
	version = 1;
}

void VESSEL2::clbkSetClassCaps (FILEHANDLE cfg)
{
}

void VESSEL2::clbkSaveState (FILEHANDLE scn)
{
	vessel->WriteDefault (*(ostream*)scn);
}

void VESSEL2::clbkLoadStateEx (FILEHANDLE scn, void *status)
{
	vessel->ParseScenarioEx (*(ifstream*)scn, status);
}

void VESSEL2::clbkSetStateEx (const void *status)
{
	vessel->SetStateEx (status);
}

void VESSEL2::clbkPostCreation ()
{
}

void VESSEL2::clbkFocusChanged (bool getfocus, OBJHANDLE hNewVessel, OBJHANDLE hOldVessel)
{
}

void VESSEL2::clbkPreStep (double simt, double simdt, double mjd)
{
}

void VESSEL2::clbkPostStep (double simt, double simdt, double mjd)
{
}

bool VESSEL2::clbkPlaybackEvent (double simt, double event_t, const char *event_type, const char *event)
{
	return false;
}

void VESSEL2::clbkVisualCreated (VISHANDLE vis, int refcount)
{
}

void VESSEL2::clbkVisualDestroyed (VISHANDLE vis, int refcount)
{
}

void VESSEL2::clbkDrawHUD (int mode, const HUDPAINTSPEC *hps, HDC hDC)
{
	if (vessel->hudskp && vessel->hudskp->GetDC() == hDC)
		g_pane->DrawDefaultHUD (vessel->hudskp);
}

void VESSEL2::clbkRCSMode (int mode)
{
}

void VESSEL2::clbkADCtrlMode (DWORD mode)
{
}

void VESSEL2::clbkHUDMode (int mode)
{
}

void VESSEL2::clbkMFDMode (int mfd, int mode)
{
}

void VESSEL2::clbkNavMode (int mode, bool active)
{
}

void VESSEL2::clbkDockEvent (int dock, OBJHANDLE mate)
{
}

void VESSEL2::clbkAnimate (double simt)
{
}

int VESSEL2::clbkConsumeDirectKey (char *keystate)
{
	return 0;
}

int VESSEL2::clbkConsumeBufferedKey (DWORD key, bool down, char *keystate)
{
	return 0;
}

bool VESSEL2::clbkLoadGenericCockpit ()
{
	SetCameraDefaultDirection (_V(0,0,1));
	return true;
}

bool VESSEL2::clbkLoadPanel (int id)
{
	return false;
}

bool VESSEL2::clbkPanelMouseEvent (int id, int event, int mx, int my)
{
	return false;
}

bool VESSEL2::clbkPanelRedrawEvent (int id, int event, SURFHANDLE surf)
{
	return false;
}

bool VESSEL2::clbkLoadVC (int id)
{
	return false;
}

bool VESSEL2::clbkVCMouseEvent (int id, int event, VECTOR3 &p)
{
	return false;
}

bool VESSEL2::clbkVCRedrawEvent (int id, int event, SURFHANDLE surf)
{
	return false;
}


// =======================================================================
// =======================================================================
// class VESSEL3: module interface to vessel class (extended)

VESSEL3::VESSEL3 (OBJHANDLE hVessel, int fmodel): VESSEL2 (hVessel, fmodel)
{
	version = 2;
}

int VESSEL3::SetPanelBackground (PANELHANDLE hPanel, SURFHANDLE *hSurf, DWORD nsurf, MESHHANDLE hMesh, DWORD width, DWORD height, DWORD baseline, DWORD scrollflag)
{
	return ((Panel2D*)hPanel)->SetBackground (hSurf, nsurf, hMesh, width, height, baseline, scrollflag);
}

int VESSEL3::SetPanelScaling (PANELHANDLE hPanel, double defscale, double extscale)
{
	return ((Panel2D*)hPanel)->SetScaling (defscale, extscale);
}

int VESSEL3::RegisterPanelMFDGeometry (PANELHANDLE hPanel, int MFD_id, int nmesh, int ngroup)
{
	return ((Panel2D*)hPanel)->RegisterMFDGeometry (MFD_id, nmesh, ngroup);
}

int VESSEL3::RegisterPanelArea (PANELHANDLE hPanel, int id, const RECT &pos, const RECT &texpos, int draw_event, int mouse_event, int bkmode)
{
	return ((Panel2D*)hPanel)->DefineArea (id, pos, 0, texpos, draw_event | PANEL_REDRAW_GDI | PANEL_REDRAW_SKETCHPAD, mouse_event, bkmode);
}

int VESSEL3::RegisterPanelArea (PANELHANDLE hPanel, int id, const RECT &pos, int draw_event, int mouse_event, SURFHANDLE surf, void *context)
{
	return ((Panel2D*)hPanel)->DefineArea (id, pos, draw_event, mouse_event, surf, context);
}

bool VESSEL3::clbkPanelMouseEvent (int id, int event, int mx, int my, void *context)
{
	return false;
}

bool VESSEL3::clbkPanelRedrawEvent (int id, int event, SURFHANDLE surf, void *context)
{
	return false;
}

int VESSEL3::clbkGeneric (int msgid, int prm, void *context)
{
	return 0;
}

bool VESSEL3::clbkLoadPanel2D (int id, PANELHANDLE hPanel, DWORD viewW, DWORD viewH)
{
	return false;
}

bool VESSEL3::clbkDrawHUD (int mode, const HUDPAINTSPEC *hps, oapi::Sketchpad *skp)
{
	g_pane->DrawDefaultHUD (skp);
	return true;
}

void VESSEL3::clbkRenderHUD (int mode, const HUDPAINTSPEC *hps, SURFHANDLE hDefaultTex)
{
	g_pane->RenderDefaultHUD ();
}

void VESSEL3::clbkGetRadiationForce (const VECTOR3 &mflux, VECTOR3 &F, VECTOR3 &pos)
{
	double size = vessel->size;
	double cs = size*size;  // simplified cross section
	double albedo = 1.5;    // simplistic albedo (mixture of absorption, reflection)

	F = mflux * (cs*albedo);
	pos = _V(0,0,0);        // don't induce torque
}



// =======================================================================
// =======================================================================
// class VESSEL4: module interface to vessel class (extended)

VESSEL4::VESSEL4 (OBJHANDLE hVessel, int fmodel) : VESSEL3 (hVessel, fmodel)
{
	version = 3;
}

int VESSEL4::RegisterPanelArea (PANELHANDLE hPanel, int id, const RECT &pos, int texidx, const RECT &texpos, int draw_event, int mouse_event, int bkmode)
{
	return ((Panel2D*)hPanel)->DefineArea (id, pos, texidx, texpos, draw_event, mouse_event, bkmode);
}

int VESSEL4::RegisterMFDMode(const MFDMODESPECEX &spec)
{
	return vessel->RegisterMFDMode (&spec);
}

bool VESSEL4::UnregisterMFDMode (int mode)
{
	return vessel->UnregisterMFDMode (mode);
}

int VESSEL4::clbkNavProcess (int mode)
{
	return mode;
}

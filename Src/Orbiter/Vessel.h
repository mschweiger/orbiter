// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ==============================================================
//              This file is part of ORBITER
//          Copyright 2000-2007 Martin Schweiger
//
// Vessel.h
// Interface of class Vessel
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

#ifndef __VESSEL_H
#define __VESSEL_H

#include <array>
#include <fstream>

#include "Vesselbase.h"
#include "Log.h"

class Elements;
class CelestialBody;
class Planet;
class PlanetarySystem;
class Base;
class Station;
class HUD;
class Panel2D;
class Nav;
class Nav_IDS;
class Nav_XPDR;
class ExhaustStream;
class oapi::Sketchpad;
class LightEmitter;
class Select;
class InputBox;
struct MFDMODE;

typedef char Str64[64];

#define SD_NAME 0x0001 // include vessel name and class name in scenario data

struct ScenarioData { // packed vessel state
	DWORD size;             // size of the complete data block
	DWORD flag;
	BYTE fstate;            // flight status
	union {
		struct {
			Vector rpos;    // reference body-relative position
			Vector rvel;    // reference body-relative velocity
			Vector arot;    // orientation (Euler angles)
			Vector vrot;    // angular velocity
		};
		struct {
			double lng;     // longitude of landing site [rad]
			double lat;     // latitude of landing site [rad]
			double hdg;     // orientation on the ground
		};
	};
	char buf[1];            // pointer to variable-length parameters
};

typedef struct {      // propellant resource definition
	double maxmass;         // max propellant mass
	double mass;            // current propellant mass
	double pmass;           // propellant mass at last time step (for flow rate calculation)
	double efficiency;      // fuel efficiency factor
} TankSpec;

typedef struct {      // logical thruster definition
	VECTOR3 ref;            // thruster reference pos
	VECTOR3 dir;            // thruster direction
	double maxth0;          // max vacuum thrust rating [N]
	double isp0;            // Vacuum Isp setting (fuel-specific impulse) [m/s]
	double pfac;            // Factor governing pressure-dependency of thrust and Isp:
	                        // Isp(p) = Isp0 (1 - p pfac)
	                        // Th(p)  = Th0 (1 - p pfac)
	double level;           // current thrust level [0..1]
	double level_permanent; // transient component of thrust level
	double level_override;  // temporary component of thrust level
	  // note: actual thrust level ('level') is sum of permanent and override level,
	  // clamped to [0..1]. override levels are reset to zero after each step
	TankSpec *tank;         // propellant resource for this thruster
} ThrustSpec;

struct ThrustGroupSpec {      // thruster group definition
	std::vector<ThrustSpec*> ts; // list of thrusters
	double maxth_sum;            // sum of maxth of all components
	ThrustGroupSpec() { maxth_sum = 0.0; }
};

typedef struct {      // obsolete exhaust render definition
	Vector ref;             // exhaust reference pos
	Vector dir;             // exhaust reference dir
	double lscale, wscale;  // exhaust size scaling
} OldExhaustSpec;

typedef struct {      // airfoil definition
	int version;          // 0: uses AirfoilCoeffFunc, 1: uses AirfoilCoeffFuncEx, 3: uses AirfoilCoeffFuncEx2
	AIRFOIL_ORIENTATION align; // vertical or horizontal
	Vector ref;           //   lift,drag attack reference point
	AirfoilCoeffFunc cf;  //   pointer to coefficients callback function
	void *context;        //   user-defined pointer passed to AirfoilCoeffFuncEx
	double c;             //   airfoil chord length
	double S;             //   reference area (wing)
	double A;             //   aspect ratio (b^2/S with wingspan b)
} AirfoilSpec;

typedef struct {      // airfoil control surface definition
	AIRCTRL_TYPE ctrl;      // control type
	Vector ref;             // lift/drag attack point
	int axis;               // axis orientation: 1=+Y, 2=-Y, 3=+X, 4=-X (should allow freely defined axes)
	double area;            // surface area
	double dCl;             // lift coefficient differential
	UINT anim;              // animation reference ((UINT)-1 for none)
} CtrlsurfSpec;

typedef struct {      // variable drag element definition
	Vector ref;             // drag attack point
	const double *drag;     // pointer to external drag magnitude
	double factor;          // drag magnitude multiplier: *drag * factor = Cd
} DragElementSpec;

typedef struct {      // docking port definition
	Vector ref;             // docking port reference pos
	Vector dir;             // approach direction
	Vector rot;             // longitudinal rotation alignment direction
	Vessel *mate;           // vessel attached to port (NULL for none)
	Vessel *pending;        // vessel being currently docked/undocked
	Vessel *owner;			// vessel that's owns this docking port
	int status;             // 0=normal (docked/free), 1=docking in progress, 2=undocking in progress
	DWORD matedock;         // mate dock index
	Nav_IDS *ids;           // instrument docking system specs (NULL if not available)
	bool autodock;			// Use auto capture on docking
} PortSpec;

typedef struct tagAttachmentSpec { // parent/child attachment definition
	Vector ref;             // reference pos
	Vector dir;             // approach direction
	Vector rot;             // longitudinal rotation alignment direction
	bool toparent;          // attachment is from child to parent
	bool loose;             // loose attachment allowed (orientation not enforced)
	Vessel *mate;           // attached vessel
	tagAttachmentSpec *mate_attach; // attachment spec of mate
	char id[8];             // attachment id
} AttachmentSpec;

typedef struct {      // nav radio definition
	float freq;             // current frequency [MHz]
	DWORD step;             // discrete frequency setting (freq = MinFreq + step * 0.05MHz)
	int dbidx;              // index into NAV data base for current proxybody and frequency
	const Nav *sender;      // incoming transmitter signal
} NavRadioSpec;

typedef struct {      // reentry texture definition
	bool do_render;           // render reentry flames?
	float lscale, wscale;     // scaling factors
	float plimit;             // friction power limit
	SURFHANDLE tex;           // reentry texture
} ReentryRenderSpec;

typedef struct {      // list of animation sequences - OBSOLETE
	double defstate;        // default mesh state
	double state;           // current state
	UINT ncomp;             // # components
	ANIMCOMP **comp;        // pointer to list of components
} ANIMSEQ;

typedef enum {
	SPEC_THRUSTMAIN,
	SPEC_TRUSTRETRO,
	SPEC_THRUSTHOVER
} VesselSpec;

struct FRecord {	       // flight recorder status sample
	double simt;    	      // simulation time
	int fstatus;	          // flight status
	int frm;                  // 0=ecliptic, 1=equatorial
	int crd;                  // 0=cartesian, 1=polar
	const CelestialBody *ref; // status reference object
	Vector rpos;	          // planet-relative position
	Vector rvel;              // planet-relative velocity
};

struct FRecord_att {      // flight recorder attitude sample
	double simt;              // simulation time
	int frm;                  // 0=ecliptic, 1=local horizon
	const CelestialBody *ref; // attitude reference object
	//double att[3];            // Euler angles
	Quaternion q;             // orientation
};

// =======================================================================
// Module interface methods

typedef VESSEL* (*VESSEL_Init)(OBJHANDLE hvessel, int flightmodel);
typedef void (*VESSEL_Exit)(VESSEL *vessel);

// =======================================================================
// Class Vessel

class Vessel: public VesselBase {
	friend class Orbiter;
	friend class VESSEL;
	friend class VESSEL2;
	friend class VESSEL3;
	friend class Pane;
	friend class DefaultPanel;
	friend class VVessel;
	friend class SuperVessel;
	friend class ExhaustStream;
	friend class ReentryStream;
	friend class Instrument_Landing;
	friend class Instrument_HSI;
	friend class Instrument_Docking;
	friend class Instrument_Comms;
	friend class HUD_Docking;

public:
	Vessel (const PlanetarySystem *psys, const char *_name, const char *_classname, const VESSELSTATUS &status);
	// Constructs a vessel instance from parameters in 'status' - uses VESSELSTATUS interface (version 1)

	Vessel (const PlanetarySystem *psys, const char *_name, const char *_classname, const void *status);
	// Constructs a vessel instance from parameters in 'status' - uses VESSELSTATUSx interface (version >= 2)

	Vessel (const PlanetarySystem *psys, const char *_name, const char *_classname, std::ifstream &ifs);
	// Constructs a vessel instance from parameters in scenario file 'ifs'

	~Vessel();

	inline int Type () const { return OBJTP_VESSEL; }
	inline const char *ClassName () const { return classname; }
	inline const char *HelpContext () const { return onlinehelp; }

	void SetClassCaps (std::ifstream &classf);
	// set class capabilities (from module code or class configuration file)

	void SetState   (const VESSELSTATUS &status); // interface version 1 (defined in Vesselstatus.cpp)
	void SetState2  (const void *status);         // interface version 2 (defined in Vesselstatus.cpp)
	bool SetStateEx (const void *status);         // generic (interface version >= 2)
	// sets the vessel status from a VESSELSTATUSx status definition

	void GetState   (VESSELSTATUS &status);       // interface version 1 (defined in Vesselstatus.cpp)
	void GetState2  (void *status);               // interface version 2 (defined in Vesselstatus.cpp)
	bool GetStateEx (void *status);               // generic (interface version >= 2)
	// fills a VESSELSTATUSx status definition with the current vessel status

	//void BeginStateUpdate ();
	void EndStateUpdate ();

	void Setup();
	// this should be called by derived classes after
	// defining specs

	void PostCreation ();
	// This is called by the psys manager after all vessels have been created

	void ProcessMessage (DWORD msg, void *data);
	// Process a message sent by the system

	void Destroying (const Vessel *vessel);
	// Notification that 'vessel' is about to be destroyed. This is sent to all
	// vessels, including the one being destroyed. Used for cleanup and de-referencing

	/**
	 * \brief Called when the user interactively changes a simulation option
	 * \param cat option category (see \ref optcat)
	 * \param item option item (see \ref optitem)
	 */
	void OptionChanged(DWORD cat, DWORD item);

	inline void RequestDestruct()
	{ kill_pending = true; }
	// mark vessel for destruction

	inline bool KillPending() const
	{ return kill_pending; }

	inline bool bPlayback() const
	{ return bFRplayback; }

	double ClipRadius () const { return clipradius ? clipradius : size; }
	void SetClipRadius (double rad) { clipradius = rad; }
	// near plane render clipping distance

	void RPlace (const Vector &rpos, const Vector &rvel);
	// Set vessel position and velocity in parent coords
	// If the vessel is part of a superstructure, the complete structure
	// is moved accordingly

	void RPlace_individual (const Vector &rpos, const Vector &rvel);
	// this version enforces the repositioning of the vessel without reporting
	// to the supervessel

	void SetGlobalOrientation (const Vector &arot);
	// Set vessel orientation from vector of Euler angles
	// If the vessel is part of a superstructure, the complete structure is
	// rotated

	void SetRotationMatrix (const Matrix &R);
	// Set global vessel rotation matrix to R
	// If the vessel is part of a superstructure, the complete structure is
	// rotated

	void GetIntermediateMoments (Vector &acc, Vector &tau,
		const StateVectors &state, double tfrac, double dt);
	// Returns acceleration acc and torque tau, at time SimT0+tfrac*SimDT
	// and step size dt, given intermediate state in global frame

	void GetIntermediateMoments_pert (Vector &acc, Vector &tau,
		const StateVectors &state_rel, double tfrac, double dt, const CelestialBody *cbody);

	Vector GetTorque () const;
	// Returns mass-normalised torque at state s0.

	Vector GetMomentumFlux () const;
	// returns the momentum flux vector due to solar radiation at current spacecraft position

	void SetAngVel (const Vector &omega);
	void SetAngVel_individual (const Vector &omega);
	// Set angular velocity components to omega [rad/s]
	// If the vessel is part of a superstructure, SetAngVel applies the spin is to the
	// complete structure, while SetAngVel_individual only affects the single vessel

	inline const Vector &Flin_induced() { return Flin; }
	// Return linear forces (other than gravity) currently acting on the vessel

	void FocusChanged (bool getfocus, Vessel *newvessel, Vessel *oldvessel);
	// Called after a vessel receives input focus (getfocus=true) or loses focus (getfocus=false)
	// newvessel has gained focus, oldvessel has lost focus
	// if getfocus=true, then newvessel=this, otherwise oldvessel=this

	void InitSupervessel (bool isprimary = true);
	// bind vessel into superstructure after simulation start if required

	int TouchdownPointsFromFile (const char *fname);
	// Read touchdown point specifications from file
	// error codes:
	//   0 = ok
	//   1 = file could not be opened
	//   2 = parse error

	bool SetTouchdownPoints (const TOUCHDOWNVTX *tdvtx, DWORD ntp = 3);
	// sets the touchdown points in the vessel frame
	// tp should contain 3 points (not collinear!)

	void ClearTouchdownPoints ();
	// Delete all touchdown vertex definitions

	void SetSurfaceFrictionCoeff (double mlng, double mlat);
	// sets coefficients of friction for ground movement in longitudinal
	// (mlong) and lateral (mlat) direction

	double GetLift () const { return Lift; }
	double GetDrag () const { return Drag; }
	double GetWeight () const { Vector G; GetWeightVector(G); return G.length(); }

	bool GetWeightVector (Vector &G) const;
	// Returns gravitational force vector (weight) (in local vessel frame).

	bool GetThrustVector (Vector &T) const;
	// Returns linear thrust vector in T (in local vessel frame).
	// Return value indicates if thrust is present

	bool GetLiftVector (Vector &L) const;
	// Returns lift vector in L (in local vessel frame).
	// Return value indicates if lift is present

	bool GetDragVector (Vector &D) const;
	// Returns drag vector in D (in local vessel frame).
	// Return value indicates if drag is present

	bool GetSideForceVector(Vector& SF) const;
	// Returns side-force vector in SF (in local vessel frame).
	// Return value indicates if side-force is present

	bool GetForceVector (Vector &F) const;
	// Returns total linear force vector acting on the vessel
	// Return value is always true

	bool GetTorqueVector (Vector &M) const;
	// Returns the total torque vector acting on the vessel
	// Return value is always true

	// ========================================================================
	// thruster management

	ThrustSpec *CreateThruster (const Vector &pos, const Vector &dir, double maxth0,
		TankSpec *ts=0, double isp0=0.0, double isp_ref=0.0, double p_ref=101.4e3);
	// Add a (logical) thruster with given position, thrust direction and max vacuum thrust [N]
	// If ts=0 then the thruster is disabled until it is linked to a propellant resource
	// If isp0=0 then the current default vacuum Isp value is used
	// If isp_ref=0 then the thrust performance is not pressure-dependent, otherwise isp_ref
	// specifies the Isp value at ambient pressure p_ref.
	// Return value is pointer to thruster

	bool DelThruster (ThrustSpec *ts);
	// delete thruster ts from list

	void ShiftThrusters (const Vector &shift);
	// shift all thruster reference points (usually in response to CG shift)

	Vector GetThrusterForce (const ThrustSpec *ts) const;
	// returns linear force F currently produced by thruster ts

	void GetThrusterMoment (const ThrustSpec *ts, Vector &F, Vector &T) const;
	// returns linear force F and torque T currently produced by thruster ts

	void ClearThrusterDefinitions ();
	// remove all logical thrusters and thruster groups,
	// and all exhaust render definitions

	void ClearExhaustDefinitions ();
	// remove all exhaust render definitions without removing the logical thrusters

	void ClearExhaustStreamDefinitions ();
	// remove all particle exhaust stream render definitions

	void SetThrusterMax0 (ThrustSpec *ts, double maxth0);
	// reset max vacuum thrust [N] for thruster ts

	inline void SetThrusterLevel (ThrustSpec *ts, double level)
	{
		if (!bFRplayback) {
			double dlevel = level - ts->level_permanent;
			ts->level_permanent = level;
			if (ts->tank && ts->tank->mass)
				ts->level = std::max(0.0, std::min(1.0, ts->level+dlevel));
		}
	}
	// set the permanent level for a thruster (0-1)

	inline void IncThrusterLevel (ThrustSpec *ts, double dlevel)
	{
		if (!bFRplayback) {
			ts->level_permanent += dlevel;
			ts->level_permanent = std::max(0.0, std::min(1.0, ts->level_permanent));
			if (ts->tank && ts->tank->mass)
				ts->level = std::max(0.0, std::min(1.0, ts->level+dlevel));
		}
	}

	inline void SetThrusterLevel_playback (ThrustSpec *ts, double level)
	{
		double dlevel = level - ts->level_permanent;
		ts->level_permanent = level;
		if (ts->tank && ts->tank->mass)
			ts->level = std::max(0.0, std::min(1.0, ts->level+dlevel));

	}
	// set permanent thruster level during playback

	inline void SetThrusterOverride (ThrustSpec *ts, double level)
	{
		if (!bFRplayback) {
			ts->level_override = level;
		}
	}
	// set the single-step level for a thruster (0-1)

	inline void IncThrusterOverride (ThrustSpec *ts, double dlevel)
	{
		if (!bFRplayback) {
			ts->level_override += dlevel;
		}
	}

	// ========================================================================
	// thruster group management

	/**
	 * \brief Assemble a set of thrusters into a logical group
	 * \param ts Array of thruster specifications to be grouped
	 * \param nts Length of the thruster array
	 * \param thgt Thruster group ID (see \ref THGROUP_TYPE). This can be one of the
	 *    default types or a user-defined type (THGROUP_USER+x)
	 * \note Multiple user-defined groups can be created (x >= 0)
	 * \note If the type ID refers to a group already previously created (default or
	 *    user-defined), the previous definition is overwritten.
	 */
	ThrustGroupSpec *CreateThrusterGroup (ThrustSpec **ts, DWORD nts, THGROUP_TYPE thgt);

	/**
	 * \brief Remove a thruster group definition
	 * \param tgs Pointer to thruster group object
	 * \param delth If true, the associated thrusters are also destroyed
	 * \return True if successful, false if group had not previously been created by the
	 *    vessel or didn't contain any thrusters
	 */
	bool DeleteThrusterGroup (ThrustGroupSpec *tgs, bool delth = false);

	/**
	 * \brief Remove a thruster group definition
	 * \param thgt Thruster group ID (see \ref THGROUP_TYPE). This can be one of the
	 *    default types or a user-defined type (THGROUP_USER+x)
	 * \param delth If true, the associated thrusters are also destroyed
	 * \return True if successful, false if group had not previously been created by the vessel
	 */
	bool DeleteThrusterGroup (THGROUP_TYPE thgt, bool delth = false);

	/**
	 * \brief Return the thruster group object pointer for a thruster group type.
	 * \param thgt Thruster group ID (see \ref THGROUP_TYPE). This can be one of the
	 *    default types or a user-defined type (THGROUP_USER+x)
	 * \return Thruster group object pointer, or 0 if not defined.
	 * \note For a default group (< THGROUP_USER), this always returns a valid pointer,
	 *    even if the vessel hasn't created that group.
	 */
	ThrustGroupSpec* GetThrusterGroup(THGROUP_TYPE thgt);
	const ThrustGroupSpec* GetThrusterGroup(THGROUP_TYPE thgt) const;

	/**
	 * \brief Return the number of thrusters in a group.
	 * \param thgt Thruster group ID (see \ref THGROUP_TYPE). This can be one of the
	 *    default types or a user-defined type (THGROUP_USER+x)
	 * \return Number of thrusters assigned to the group
	 */
	DWORD NumThrusters(THGROUP_TYPE thgt) const;

	/**
	 * \brief Check if a thruster is member of a thruster group
	 * \param tgs Pointer to thruster group object (must not be 0)
	 * \param ts Pointer to thruster object
	 * \return True if ts is a member of group tgs, false otherwise.
	 */
	bool IsGroupThruster(ThrustGroupSpec* tgs, ThrustSpec* ts) const;

	/**
	 * \brief Set the permanent thruster level for all thrusters in a group
	 * \param tgs Pointer to thruster group object (must not be 0)
	 * \param level Thruster level to be applied to all thrusters in the group (0-1)
	 */
	inline void SetThrusterGroupLevel (ThrustGroupSpec *tgs, double level)
	{
		dCHECK(tgs, "Zero ThrustGroupSpec pointer not permitted.")

		for (auto it = tgs->ts.begin(); it != tgs->ts.end(); it++)
			SetThrusterLevel(*it, level);
	}

	/**
	 * \brief Set the permanent thruster level for all thrusters in a group
	 * \param thgt Thruster group specifier. This can be a default group (see \ref THGROUP_TYPE)
	 *    or a user-defined type (THGROUP_USER+x)
	 * \param level Thruster level to be applied to all thrusters in the group (0-1)
	 */
	void SetThrusterGroupLevel(THGROUP_TYPE thgt, double level);

	/**
	 * \brief Set the single-step thruster level for all thrusters in a group
	 * \param tgs Pointer to thruster group object (must not be 0)
	 * \param level Thruster level to be applied to all thrusters in the group (0-1)
	 * \note This thrust level is only applied for the current time step, after which it
	 *    reverts to the permanent level.
	 */
	inline void SetThrusterGroupOverride(ThrustGroupSpec* tgs, double level)
	{
		dCHECK(tgs, "Zero ThrustGroupSpec pointer not permitted.")

		for (auto it = tgs->ts.begin(); it != tgs->ts.end(); it++)
			SetThrusterOverride(*it, level);
	}

	/**
	 * \brief Set the single-step thruster level for all thrusters in a group
	 * \param thgt Thruster group specifier. This can be a default group (see \ref THGROUP_TYPE)
	 *    or a user-defined type (THGROUP_USER+x)
	 * \param level Thruster level to be applied to all thrusters in the group (0-1)
	 * \note This thrust level is only applied for the current time step, after which it
	 *    reverts to the permanent level.
	 */
	void SetThrusterGroupOverride(THGROUP_TYPE thgt, double level);

	/**
	 * \brief Add a differential to the level of all thrusters in a group
	 * \param tgs Pointer to thruster group object (must not be 0)
	 * \param dlevel thrust group differential (can be negative)
	 * \note The resulting level is clamped to range 0-1.
	 * \note If the initial thrust levels were different, they will still be different
	 *    after this call.
	 */
	void IncThrusterGroupLevel (ThrustGroupSpec *tgs, double dlevel);

	/**
	 * \brief Add a differential to the level of all thrusters in a group
	 * \param thgt Thruster group specifier. This can be a default group (see \ref THGROUP_TYPE)
	 *    or a user-defined type (THGROUP_USER+x)
	 * \param dlevel thrust group differential (can be negative)
	 * \note The resulting level is clamped to range 0-1.
	 * \note If the initial thrust levels were different, they will still be different
	 *    after this call.
	 */
	void IncThrusterGroupLevel(THGROUP_TYPE thgt, double dlevel);

	/**
	 * \brief Add a differential to the single-step level of all thrusters in a group
	 * \param tgs Pointer to thruster group object (must not be 0)
	 * \param dlevel thrust group differential (can be negative)
	 * \note The resulting level is clamped to range 0-1.
	 * \note If the initial thrust levels were different, they will still be different
	 *    after this call.
	 * \note The thrust level is increased only for the current time step, after which it
	 *    reverts to the permanent level.
	 */
	inline void IncThrusterGroupOverride (ThrustGroupSpec *tgs, double dlevel)
	{
		dCHECK(tgs, "Zero ThrustGroupSpec pointer not permitted.")

		for (auto it = tgs->ts.begin(); it != tgs->ts.end(); it++)
			IncThrusterOverride (*it, dlevel);
	}

	/**
	 * \brief Add a differential to the single-step level of all thrusters in a group
	 * \param thgt Thruster group specifier. This can be a default group (see \ref THGROUP_TYPE)
	 *    or a user-defined type (THGROUP_USER+x)
	 * \param dlevel thrust group differential (can be negative)
	 * \note The resulting level is clamped to range 0-1.
	 * \note If the initial thrust levels were different, they will still be different
	 *    after this call.
	 * \note The thrust level is increased only for the current time step, after which it
	 *    reverts to the permanent level.
	 */
	void IncThrusterGroupOverride(THGROUP_TYPE thgt, double dlevel);

	/**
	 * \brief Return the average thrust level of a thruster group
	 * \param tgs Pointer to thruster group object (must not be 0)
	 * \return Average current thrust level (0-1)
	 */
	double GetThrusterGroupLevel(const ThrustGroupSpec* tgs) const;

	/**
	 * \brief Return the average thrust level of a thruster group
	 * \param thgt Thruster group specifier. This can be a default group (see \ref THGROUP_TYPE)
	 *    or a user-defined type (THGROUP_USER+x)
	 * \return Average current thrust level (0-1)
	 */
	double GetThrusterGroupLevel(THGROUP_TYPE thgt) const;

	/**
	 * \brief Return sum of vaccum thrust ratings of the thrusters in a group
	 * \param tgs Pointer to thruster group object (must not be 0)
	 * \return Sum of vacuum thrust ratings [N]
	 * \note The returned value is not necessarily the effective total thrust rating of
	 *    the group, because the thrusters may not be aligned.
	 */
	inline double GetThrusterGroupMaxth (const ThrustGroupSpec *tgs) const
	{
		dCHECK(tgs, "Zero ThrustGroupSpec pointer not permitted.")
		return tgs->maxth_sum;
	}

	/**
	 * \brief Return sum of vaccum thrust ratings of the thrusters in a group
	 * \param thgt Thruster group specifier. This can be a default group (see \ref THGROUP_TYPE)
	 *    or a user-defined type (THGROUP_USER+x)
	 * \return Sum of vacuum thrust ratings [N]
	 * \note The returned value is not necessarily the effective total thrust rating of
	 *    the group, because the thrusters may not be aligned.
	 */
	double GetThrusterGroupMaxth(THGROUP_TYPE thgt) const;

	/**
	 * \brief Return the linear force vector currently produced by all thrusters in a group
	 * \param thgt Thruster group specifier. This can be a default group (see \ref THGROUP_TYPE)
	 *    or a user-defined type (THGROUP_USER+x)
	 * \return Current linear force vector generated by the group [N]
	 */
	Vector GetThrusterGroupForce (THGROUP_TYPE thgt) const;

	/**
	 * \brief Return the linear force vector currently produced by all thrusters in a group
	 * \param tgs Pointer to thruster group object (must not be 0)
	 * \return Current linear force vector generated by the group [N]
	 */
	Vector GetThrusterGroupForce (const ThrustGroupSpec *tgs) const;

	void IncMainRetroLevel (double dlevel);
	// This is a special treatment of the main/retro groups: increase forward thrust by
	// increasing main or decreasing retro thrust (and reverse for negative dlevel).
	// This function is used by Ctrl-Num+. It also equalises all thruster levels in the main
	// and retro groups

	bool SetMaxThrust_old (ENGINETYPE eng, double maxth);
	// this function is for backward compatibility only. It assigns a maximum thrust rating to
	// thruster group thg. This works only if the group is not yet assigned (in which case a new thruster
	// with generic specs is assigned) or if the group contains only one thruster. It returns false
	// if called for an existing multi-thruster group

	void CreateDefaultAttitudeSet (double maxth);
	// Shortcut for creating default attitude thrusters and thruster groups. Assumes that thrusters are
	// located ideally, at distance 'size' from CG, each with max thrust rating 'maxth'. Both rotational and
	// linear groups consist of 2 thrusters each, so the total thrust for linear modes is 2*maxth

#ifdef UNDEF
	UINT AddExhaust (ThrustSpec *ts, double lscale, double wscale, double lofs = 0,
		const Vector *pos = 0, const Vector *dir = 0, SURFHANDLE tex = 0, double modulate = 0.0);
	// Add an exhaust render specification for thruster ts, using its 'level' parameter for
	// scaling. Unless pos and/or dir are explicitly specified, they are linked to the thruster
	// 'pos' and 'dir' parameters. lscale and wscale are exhaust length and width scales.
#endif

	UINT AddExhaust (EXHAUSTSPEC *spec);

	bool DelExhaust (UINT idx);
	// removes the idx-th exhaust render specification from the list

	oapi::ParticleStream *AddParticleStream (PARTICLESTREAMSPEC *pss, const Vector &pos, const Vector &dir, double *lvl);
	// Add a generic particle stream to the vessel for given position and direction.
	// Lvl is a pointer to an external level control variable

	oapi::ParticleStream *AddExhaustStream (ThrustSpec *ts, PARTICLESTREAMSPEC *pss = 0, const Vector *pos = 0, const Vector *dir = 0);
	// Add a particle exhaust stream render specification for thruster ts, using its 'level'
	// parameter for controlling the particle emission system

	bool DelParticleStream (oapi::ParticleStream *ps);
	// Detach a particle stream (exhaust or reentry) from the vessel

	bool DelExhaustStream (oapi::ParticleStream *ep);
	// removes a contrail specification from the list and invalidates it

	oapi::ParticleStream *AddReentryStream (PARTICLESTREAMSPEC *pss);
	// Add a reentry stream specification

	void SetDefaultReentryStream ();
	// Set a default set of particle streams for reentry rendering

	bool DelReentryStream (oapi::ParticleStream *ep);
	// Detach a single reentry stream from the vessel

	void ClearReentryStreams ();
	// Detach reentry streams from vessel. Reentry streams persist until the last particle
	// has expired.

	// ========================================================================
	// light emitter management

	LightEmitter *AddPointLight (const VECTOR3 &pos, double range, double att0, double att1, double att2, COLOUR4 col_diff, COLOUR4 col_spec, COLOUR4 col_ambi);
	// Add a point light emitter to the vessel with the specified specs, intensity, position and direction references

	LightEmitter *AddSpotLight (const VECTOR3 &pos, const VECTOR3 &dir, double range, double att0, double att1, double att2, double umbra, double penumbra, COLOUR4 col_diff, COLOUR4 col_spec, COLOUR4 col_ambi);
	// Add a spot light emitter to the vessel with the specified specs, intensity, position and direction references

	bool DelLightEmitter (LightEmitter *le);
	// Delete a light emitter and remove it from the vessel's emitter list

	void ClearLightEmitters ();
	// Delete all light emitters defined for the vessel

	DWORD LightEmitterCount () const { return nemitter; }
	// Returns the number of emitters defined for the vessel

	const LightEmitter *GetLightEmitter (DWORD i) const { return emitter[i]; }
	// Returns an emitter identified by index

	void LightEmitterState (LightEmitter *le, int param, void *value);
	// Notification of emitter state change

	void ShiftLightEmitters (const VECTOR3 &ofs);
	// Shift all light emitter positions by 'ofs' (usually to react to a shift in CG)

	// ========================================================================
	// propellant resource management

	TankSpec *CreatePropellantResource (double maxmass, double mass = -1.0, double efficiency = 1.0);
	// Create a propellant resource ("tank") with 'maxmass' maximum fuel capacity [kg] and
	// 'mass' current fuel mass [kg]. If 'mass' < 0.0 then mass = maxmass
	// Return value is a pointer to the propellant resource

	void DelPropellantResource (TankSpec *ts);
	// Remove a propellant resource and unlink thrusters which pointed to this resource

	void ClearPropellantResources ();
	// Remove all propellant resources, and unlink all thrusters from their resources

	inline TankSpec *PropellantHandle (DWORD idx) const
	{ return (idx < ntank ? tank[idx] : 0); }

	inline TankSpec *DefaultPropellantHandle () const
	{ return (def_tank ? def_tank : ntank ? tank[0] : 0); }

	inline void SetPropellantMaxMass (TankSpec *ts, double m)
	{ ts->mass = ts->pmass = std::min (ts->maxmass = m, ts->mass); UpdateMass(); }
	// Reset the maximum capacity [kg] of a propellant resource

	inline void SetPropellantMass (TankSpec *ts, double m)
	{ ts->mass = ts->pmass = std::min (m, ts->maxmass); ResetMass(); }
	// Set fuel mass [kg] of a propellant resource

	inline double GetPropellantLevel (const TankSpec *ts) const
	{ return (ts->maxmass ? ts->mass / ts->maxmass : 0.0); }
	// Return fuel level for propellant resource ts

	double GetPropellantFlowrate (const TankSpec *ts) const;

	// ========================================================================
	// aerodynamics

	AirfoilSpec *CreateAirfoil (AIRFOIL_ORIENTATION align, const Vector &ref, AirfoilCoeffFunc cf, double c, double S, double A);
	// Create a new airfoil

	AirfoilSpec *CreateAirfoil (AIRFOIL_ORIENTATION align, const Vector &ref, AirfoilCoeffFuncEx cf, void *context, double c, double S, double A);
	// Create a new airfoil; extended version

	AirfoilSpec* CreateAirfoil(AIRFOIL_ORIENTATION align, const Vector& ref, AirfoilCoeffFuncEx2 cf, void* context, double c, double S, double A);
	// Create a new airfoil; extended force and moment version

	bool GetAirfoilParam (AirfoilSpec *af, VECTOR3 *ref, AirfoilCoeffFunc *cf, void **context, double *c, double *S, double *A);
	// Return airfoil parameters

	void EditAirfoil (AirfoilSpec *af, DWORD flag, const Vector &ref, AirfoilCoeffFunc cf, double c, double S, double A);
	// Edit an existing airfoil definition

	bool DelAirfoil (AirfoilSpec *af);
	// Delete an airfoil. Returns false on failure.

	bool DelAirfoil (DWORD i);
	// Delete an airfoil given by its index. Returns false on failure.

	void ClearAirfoilDefinitions ();
	// Remove all airfoil lift,drag definitions

	CtrlsurfSpec *CreateControlSurface (AIRCTRL_TYPE ctrl, double area, double dCl, const Vector &ref, int axis, double delay = 1.0, UINT anim = (UINT)-1);
	// Create a new control surface definition of the specified type

	bool DelControlSurface (CtrlsurfSpec *cs);
	// Delete a control surfce. Returns false on failure.

	bool DelControlSurface (DWORD i);
	// Delete a control surface given by its index. Returns false on failure.

	void ClearControlSurfaceDefinitions ();
	// Remove all airfoil control surfaces generated before

	void SetControlSurfaceLevel (AIRCTRL_TYPE ctrl, double level, bool transient, bool direct = false);

	inline double GetControlSurfaceLevel (AIRCTRL_TYPE ctrl) const { return ctrlsurf_level[ctrl].curr; }
	// set/get airfoil control surface levels (-1..1)

	void ApplyControlSurfaceLevels ();
	// synchronise actual with target airfoil control surface levels

	void CreateVariableDragElement (const double *drag, double factor, const Vector &ref);
	// creates a drag source whose magnitude is controlled by external variable "drag"
	// useful for drag generated by landing gear, speed brakes etc.

	void ClearVariableDragElements ();
	// Remove all drag element definitions

	void ApplyUserAttitudeControls (DWORD *ctrl);
	// translate user keyboard/joystick input into RCS or airfoil response (single frame)

	void IncTrim (AIRCTRL_TYPE ctrl);
	void DecTrim (AIRCTRL_TYPE ctrl);
	// airfoil trim and flap setting (ctrl must be one of AIRCTRL_ELEVATORTRIM, AIRCTRL_RUDDERTRIM or AIRCTRL_FLAP)

	// ========================================================================
	// MFD management

	int RegisterMFDMode (const MFDMODESPECEX *spec);
	// Register a new vessel-specific MFD mode for the vessel
	// Returns MFD mode id

	bool UnregisterMFDMode (int id);
	// Unregisters the specified MFD mode

	void UnregisterMFDModes ();
	// Un-register all locally defined MFD modes

	DWORD GetMFDModes (const MFDMODE **modelist) const
	{ *modelist = mfdmode; return nmfdmode; }

	// ========================================================================

	inline bool GetEnableFocus() const { return enablefocus; }
	inline void SetEnableFocus (bool enable) { enablefocus = enable; }
	// get/set input focus enabled state

	inline Vector *CamPos () { return &campos; }
	// camera position offset

	inline Vector *CamDir0 () { return &camdir0; }
	inline double CamTilt0 () { return camtilt0; }
	// camera default view direction and rotation around that direction

	inline void CamRange (double &left, double &right, double &up, double &down) const
	{ left = camdp_left; right = camdp_right; up = camdt_up; down = camdt_down; }
	// camera rotation ranges

	inline double CamCatchRange () const { return camcatchangle; }
	// camera autocatch angle

	static void SetCameraShiftRange (const VECTOR3 &fwd, const VECTOR3 &left, const VECTOR3 &right);
	static void SetCameraMovement (const VECTOR3 &fwdpos, double fwdphi, double fwdtht,
		const VECTOR3 &lpos, double lphi, double ltht, const VECTOR3 &rpos, double rphi, double rtht);
	static void UnsetCameraMovement ();

	inline void OverrideMainLevel (double level)
	{
		if (level >= 0.0) {
			SetThrusterGroupOverride (THGROUP_MAIN, level);
			SetThrusterGroupOverride (THGROUP_RETRO, -1.0); // kill
		} else {
			SetThrusterGroupOverride (THGROUP_MAIN, -1.0); // kill
			SetThrusterGroupOverride (THGROUP_RETRO, -level);
		}
	}
	// Single-frame override for main/retro groups (<0=retro)

	inline void SetAttitudeRotX (double level) {
		if (level >= 0.0) SetThrusterGroupLevel (THGROUP_ATT_PITCHUP,    level),
			              SetThrusterGroupLevel (THGROUP_ATT_PITCHDOWN,  0.0  );
		else              SetThrusterGroupLevel (THGROUP_ATT_PITCHUP,    0.0  ),
			              SetThrusterGroupLevel (THGROUP_ATT_PITCHDOWN, -level);
	}
	inline void SetAttitudeRotY (double level) {
		if (level >= 0.0) SetThrusterGroupLevel (THGROUP_ATT_YAWLEFT,    level),
			              SetThrusterGroupLevel (THGROUP_ATT_YAWRIGHT,   0.0  );
		else              SetThrusterGroupLevel (THGROUP_ATT_YAWLEFT,    0.0  ),
			              SetThrusterGroupLevel (THGROUP_ATT_YAWRIGHT,  -level);
	}
	inline void SetAttitudeRotZ (double level) {
		if (level >= 0.0) SetThrusterGroupLevel (THGROUP_ATT_BANKRIGHT,  level),
			              SetThrusterGroupLevel (THGROUP_ATT_BANKLEFT,   0.0  );
		else              SetThrusterGroupLevel (THGROUP_ATT_BANKRIGHT,  0.0  ),
			              SetThrusterGroupLevel (THGROUP_ATT_BANKLEFT,  -level);
	}
	inline void SetAttitudeLinX (double level) {
		if (level >= 0.0) SetThrusterGroupLevel (THGROUP_ATT_RIGHT,      level),
			              SetThrusterGroupLevel (THGROUP_ATT_LEFT,       0.0  );
		else              SetThrusterGroupLevel (THGROUP_ATT_RIGHT,      0.0  ),
			              SetThrusterGroupLevel (THGROUP_ATT_LEFT,      -level);
	}
	inline void SetAttitudeLinY (double level) {
		if (level >= 0.0) SetThrusterGroupLevel (THGROUP_ATT_UP,         level),
			              SetThrusterGroupLevel (THGROUP_ATT_DOWN,       0.0  );
		else              SetThrusterGroupLevel (THGROUP_ATT_UP,         0.0  ),
			              SetThrusterGroupLevel (THGROUP_ATT_DOWN,      -level);
	}
	inline void SetAttitudeLinZ (double level) {
		if (level >= 0.0) SetThrusterGroupLevel (THGROUP_ATT_FORWARD,    level),
			              SetThrusterGroupLevel (THGROUP_ATT_BACK,       0.0  );
		else              SetThrusterGroupLevel (THGROUP_ATT_FORWARD,    0.0  ),
			              SetThrusterGroupLevel (THGROUP_ATT_BACK,      -level);
	}
	// change permanent attitude thruster levels

	inline int AttMode () const { return attmode; }
	int ToggleAttMode ();
	bool SetAttMode (int mode, bool fromstream = false);
	// return/toggle/set attitude thruster mode (0=disable, 1=rot, 2=lin)

	DWORD ToggleADCtrlMode ();
	void SetADCtrlMode (DWORD mode, bool fromstream = false);
	// connect/disconnect user input to aerodynamic control surfaces

	void SetWBrakeLevel (double level, int which = 0, bool permanent = true);
	// wheel brake setting (0..1)
	// which: 1=left, 2=right, 0=both
	// if permanent=false, the level is only applied to the next frame, and
	// will return to the permanent level afterwards

	// ========================================================================
	// Docking port management

	PortSpec *CreateDock (const Vector &pos, const Vector &dir, const Vector &rot);
	// Create a new docking port and return a pointer to it

	void SetDockParams (PortSpec *dock, const Vector &pos, const Vector &dir, const Vector &rot);
	// Reset parameters of an existing dock

	inline void SetDockMode (int mode) { dockmode = mode; }
	inline int GetDockMode () const { return dockmode; }

	void ShiftDocks (const Vector &ofs);
	// shift all dock positions by ofs

	inline const PortSpec *GetDockParams (DWORD did) const { return dock[did]; }

	Vector GetDockGPos (const PortSpec *dock) const
	{ return (mul (s0->R, dock->ref) + s0->pos); }
	// Dock position in global coordinates

	void SetDockIDS (PortSpec *dock, float freq, float range = 2e4);
	// Set instrument docking approach specs for a docking port

	bool DelDock (PortSpec *dock);
	bool DelDock (DWORD i);
	// Delete a dock (by spec or index), after undocking any docked vessel.
	// Returns false on failure.

	void ClearDockDefinitions ();
	// Remove all dock definitions

	void RegisterDocking (DWORD did, Vessel *mate, DWORD matedid);
	// Register a docking event at dock 'did' with vessel 'mate' and mate's dock 'matedid'

	void UnregisterDocking (DWORD did);
	// Register an undocking event at dock 'did'

	void MoveDock(PortSpec* dock, const Vector& pos, const Vector& dir, const Vector& rot);
	PortSpec* GetProxyDock(PortSpec* pD);
	bool GetTargetDockAlignment(PortSpec* pD, PortSpec* pT, Vector* ofs, Vector* dir, Vector* rot, Vector* rvel = nullptr);

	int Dock (Vessel *target, DWORD mydid, DWORD tgtdid, DWORD mode = 0);
	// Dock with 'target', using dock 'mydid' and attach to target dock 'tgtdid'
	// Return values: 0=ok, 1=my dock in use, 2=target dock in use, 3=already docked to target

	bool Undock (UINT did, const Vessel *exclude = 0, double vsep = 0.2);
	// Perform undocking from object docked at 'did' with separation velocity
	// 'vsep', but excluding object 'exclude'

	void UndockInteractive (const Vessel *exclude = 0, double vsep = 0.2);
	// This version asks for a dock to disengage, if the vessel defines more
	// than one dock.

	inline DWORD nDock () const { return ndock; }
	inline Vessel *DockMate (DWORD n) const { return dock[n]->mate; }

	void RelDockingPos (const Vessel *target, UINT mydid, UINT tgtdid, Vector &P, Matrix &R);
	// Calculate the relative position 'P' and orientation 'R' of 'target'
	// in my reference frame, if we are docked between 'mydid' and 'tgtdid'

	// ========================================================================
	// attachment management

	AttachmentSpec *CreateAttachment (bool toparent, const Vector &pos, const Vector &dir, const Vector &rot, const char *id, bool loose = false);
	// create a new defintion for a parent/child attachment point
	// if loose==true, we freeze current relative orientation between child and parent;
	// otherwise, the orientation defined by the attachment orientation axes is used

	bool DelAttachment (AttachmentSpec *as);
	// Delete an attachment point

	void ClearAttachments ();
	// remove all attachment definitions

	void SetAttachmentParams (AttachmentSpec *as, const Vector &pos, const Vector &dir, const Vector &rot);
	// reset parameters of an existing attachment point

	bool AttachChild (Vessel *child, AttachmentSpec *as, AttachmentSpec *asc, bool allow_loose = true);
	// attach "child" as a child object at the attachment specified by "as"
	// return value indicates success

	bool AttachToParent (Vessel *parent, AttachmentSpec *asp, AttachmentSpec *asc, bool allow_loose = true);
	// sent by the parent to the child to notify attachment request
	// return value indicates validity of request (i.e. "false" means attaching is not possible)

	bool DetachChild (AttachmentSpec *asp, double vel = 0.0);
	// detach the child currently attached to asp
	// return value indicates success

	bool DetachFromParent (double vel = 0.0);
	// detach from current parent
	// return value indicates validity of request (i.e. "false" means child will not detach)

	inline bool isAttached() const { return attach != NULL; }
	// true if we are attached to a parent vessel

	void InitAttachmentToParent (AttachmentSpec *asc, bool allow_loose = true);
	// set relative rotation matrix and position of child after attached to a parent

	DWORD GetAttachmentIndex (AttachmentSpec *as) const;
	// returns the list index of as (either in child or parent list) or (DWORD)-1 if not present

	AttachmentSpec *GetAttachmentFromIndex (bool toparent, DWORD i);
	// returns the attachment for a given index from either the to-parent or the to-child list

	void ShiftAttachments (const Vector &ofs);
	// move all attachment points by offset ofs

	Vessel* Vessel::GetAttachmentRoot();
	// Get the root vessel. (i.e. vessel with no parent attachments)

	// ========================================================================
	// navigation radio interface

	bool SetNavChannel (DWORD n, DWORD ch);
	bool IncNavChannel (DWORD n, int dch);
	DWORD GetNavChannel (DWORD n) const;
	float GetNavFreq (DWORD n) const;

	inline Nav_XPDR *GetXPDR () const { return xpdr; }
	DWORD GetXpdrChannel () const;
	bool GetXpdrFreq (float &freq) const;
	bool SetXpdrChannel (DWORD ch);
	bool IncXpdrChannel (int dch);
	bool SetIDSChannel (PortSpec *ps, DWORD ch);

	bool SetNavMode (int mode, bool fromstream = false);
	bool ClrNavMode (int mode, bool record = true, bool fromstream = false);
	bool TglNavMode (int mode);
	bool NavModeActive (int mode);
	// set/unset/toggle a particular navigation mode
	// and check status of a navigation mode

	void SetHoverHoldAltitude (double alt, bool terrainalt);

	// ========================================================================
	// beacon light management

	void AddBeacon (BEACONLIGHTSPEC *bs);
	// add a beacon light with the provided specs

	bool DelBeacon (BEACONLIGHTSPEC *bs);
	// remove the specified beacon

	void ClearBeacons ();
	// remove all beacons

	const BEACONLIGHTSPEC *GetBeacon (DWORD idx) const;

	inline VESSEL *GetModuleInterface () { return modIntf.v; }
	// Return module interface for vessel. Guaranteed to exist even if the
	// vessle class isn't managed by a module

	int ConsumeDirectKey (char *buffer);
	// Keyboard handler for immediate keys

	int ConsumeBufferedKey (DWORD key, bool down, char *kstate);
	// Keyboard handler for buffered keys

	void Update (bool force = false);
	void UpdatePassive ();
	void UpdateAttachments();
	void UpdateBodyForces ();
	void UpdateThrustForces ();
	void UpdateRadiationForces ();
	void UpdateAerodynamicForces ();
	void UpdateAerodynamicForces_OLD ();
	bool AddSurfaceForces (Vector *F, Vector *M,
		const StateVectors *s=NULL, double tfrac=1.0, double dt=0.0,
		bool allow_groundcontact=true) const;

	void PostUpdate ();
	// called after all vessels have been updated (i.e. states are synced)
	// vessels should not change their state vectors in this function

	void Update_old (bool force = false); // legacy method

	void Timejump (double dt, int mode);
	// propagate vessel state to a new time (discontinuous)

	void ModulePostCreation();
	// Calls to VESSEL2::clbkPostCreation after the vessel has been created and
	// its state initialised

	void ModulePreStep (double t, double dt, double mjd);
	void ModulePostStep (double t, double dt, double mjd);
	// Calls to VESSEL2::clbkPreStep and VESSEL2::clbkPostStep, respectively.
	// Module time step notifications before and after simulation state update.

	void ModuleSignalRCSmode (int mode);
	// Notifies module of RCS mode change by calling VESSEL2::clbkRCSMode function

	void ModuleSignalADCtrlmode (DWORD mode);
	// Notifies module of aerodynamic control mode change by calling VESSEL2::clbkADCtrlmode

	void ModuleSignalNavmode (int mode, bool active);
	// Notifies module of nav-mode change by calling VESSEL2::clbkNavmode function

	void DrawHUD (HUD *hud, oapi::Sketchpad *skp);
	// Draw HUD display onto a GDI surface. Either use default HUD drawing method,
	// or call vessel module callback function for vessel-specific HUDs

	void RenderHUD (HUD *hud, int mode, HUDPAINTSPEC *spec, SURFHANDLE hTex);
	// Render HUD display. Either use default HUD render method, or call vessel
	// module callback function for vessel-specific HUDs

	void HUDchanged (int mode);
	// Called after a change of HUD mode. 'mode': new HUD mode

	void MFDchanged (int mfd, int mode) const;
	// Called after a change of MFD mode. 'mfd': MFD id, 'mode': new MFD mode

	inline void SetLandingTarget (Base *target) { landtgt = target; }
	inline Base *LandingTarget () const { return landtgt; }
	inline DWORD PortNo () const { return nport; }
	inline int   LCommsStatus() const { return lstatus; }
	//inline int   DCommsStatus() const { return dstatus; }
	// set and retrieve the vessel's target (base) for landing/docking

	//inline Station *ProxyStation () const { return proxystat; }
	inline Vessel *ProxyVessel() const { return proxyvessel; }

	inline SuperVessel *SuperStruct() const { return supervessel; }
	inline void SetSuperStruct (SuperVessel *sv) { supervessel = sv; }

	bool GetSuperStructCG (Vector &cg) const;
	// If vessel is part of a superstructure: set 'cg' to coordinates of
	// superstructure CG in vessel coordinates and return true.
	// Otherwise: set 'cg' to (0,0,0) and return false

	inline const SurfParam *GetSurfParam () const
	{ return (attach ? attach->mate->GetSurfParam() : proxybody ? &sp : 0); }
	// return parameters referring to the vessel's position w.r.t. the
	// surface of the closest planet (or 0 if not within range of a planet)

	inline FlightStatus GetStatus () const { return fstatus; }
	// vessel's flight status

	bool isOrbitStabilised () const;
	// true if vessel uses 2-body analytic update to stabilise orbit

	bool HasExtpassMeshes() const { return extpassmesh; }
	// returns true if visual representation contins meshes that are rendered
	// in the external render pass during cockpit view

	inline bool isInAtmosphere() const { return sp.is_in_atm; }
	// true if vessel is inside a planet's atmospheric layer

	inline bool AtmTemperature (double &T) const
	{ T = sp.atmT; return sp.is_in_atm; }
	// returns atmospheric temperature [K]

	inline double AtmDensity () const { return sp.atmrho; }
	inline double AtmPressure () const { return sp.atmp; }
	bool AtmPressureAndDensity (double &p, double &rho) const;
	// returns atmospheric pressure and/or density
	// return value indicates whether vessel is inside an atmosphere

	inline bool DynPressure (double &dynp) const
	{ dynp = sp.dynp; return sp.is_in_atm; }
	// returns dynamic pressure if in atmosphere

	inline bool MachNumber (double &M) const
	{ M = sp.atmM; return sp.is_in_atm; }
	// returns current vessel Mach number

	void IssueClearanceRequest ();
	// ask current landing target for takeoff/landing clearance

	inline double EmptyMass () const { return emass; }
	inline void   SetEmptyMass (double m) { emass = m; UpdateMass(); }
	// return/set empty vessel mass (excluding fuel and payload)

	inline double FuelMass () const { return fmass; }
	// Returns current fuel mass

	void EngineStatus (ENGINESTATUS *es);
	// OBSOLETE

	void Refuel ();
	// refill fuel tanks

	void RegisterVisual (VISHANDLE vis);
	void UnregisterVisual ();

	bool SetGenericCockpit () const;
	// calls the module's ovcGenericCockpit function if available

	bool LoadPanel (int which) const;
	// Calls VESSEL2::clbkLoadPanel to load a 2-D panel if available

	bool LoadPanel2D (int which, Panel2D *panel) const;
	// Calls VESSEL3::clbkLoadPanel2D to load a 2-D panel if available

	bool PanelRedrawEvent (int id, int event, SURFHANDLE surf, void *context) const;
	// pass an area redraw request to the vessel's panel redraw method

	bool PanelMouseEvent (int id, int event, int mx, int my, void *context) const;
	// pass a mouse event to the vessel's panel mouse event handler

	bool LoadVC (int id) const;
	// Load virtual cockpit if available

	bool VCRedrawEvent (int id, int event, SURFHANDLE surf) const;
	// pass an area redraw request to the vessel's virtual cockpit redraw method

	bool VCMouseEvent (int id, int event, Vector &p) const;
	// pass a mouse event to the vessel's virtual cockpit mouse event handler

	void LeanCamera (int dir, bool smooth = true);
	// "lean" forward, left or right in cockpit mode

	bool Read (std::ifstream &ifs);
	void Write (std::ostream &ofs) const;
	// read/write vessel status from/to stream

protected:
	bool OpenConfigFile (std::ifstream &cfgfile) const;
	// returns configuration file for the vessel
	// This first looks in Config\Vessels, then in Config

	//bool bInAtmosphere;
	// true if vessel is flying through a planet's atmosphere

	bool bGroundProximity;
	// true if vessel-proxybody distance < 1.01 proxybody radius

	bool bForceActive;
	// true if nongravitational force (thruster, atmospheric effect, user force, etc)
	// is present at current time step

	//bool bOrbitStabilised;
	// true if we use 2-body orbit stabilisation

	void HoverHoldAltitude ();

	bool CheckSurfaceContact () const;
	// Returns true if any part of the vessel is in contact with a planet surface
	// Should be called only after update phase, and assumes that sp is up to date.

	bool ThrustEngaged () const { return m_bThrustEngaged; }

	bool IsComponent () const { return supervessel != 0 || attach; }
	const VesselBase *GetSuperStructure () const;

	inline void AddForce (const Vector &F, const Vector &r)
	{
		Flin_add += F;
		Amom_add += crossp (F, r);
	}
	// given a force vector F and attack point r, this updates the linear and angular force
	// vectors Flin and Amom for the rigid-body model

	/**
	 * \brief Return the magnitude of the maximum vacuum angular moment produced by all thrusters of a given RCS attitude group
	 * \param axis Rotation axis index (0-5). The index is related to the thruster group by THGROUP_ATT_PITCHUP + axis
	 * \return Magnitude of max. angular moment [Nm]
	 * \note This also works if the vessel is part of a super-structure.
	 */
	double MaxAngularMoment (int axis) const;

	inline void SetDefaultPropellant (TankSpec *ts)
	{ def_tank = ts; }

	inline double ThrusterAtmScale (const ThrustSpec *ts, double p) const
	{
		if (!ts->pfac) return 1.0;
		return std::max (0.0, 1.0 - p*ts->pfac);    // old linear model
		//return exp (p*ts->pfac);               // new exponential model
	}
	// Returns a thrust scaling factor for thruster th in ambient pressure p

	bool ParseScenario (std::ifstream &scn, VESSELSTATUS &vs);
	// Read status parameters from scenario scn to vessel status vs - OBSOLETE

	bool ParseScenarioEx (std::ifstream &scn, void *status);
	// Read status parameters from scenario scn to vessel dynamic vessel status 'status'

	bool ParseScenarioLine   (char *line, VESSELSTATUS &vs); // interface version 1 (defined in Vesselstatus.cpp)
	bool ParseScenarioLine2  (char *line, void *status);     // interface version 2 (defined in Vesselstatus.cpp)
	bool ParseScenarioLineEx (char *line, void *status);     // generic (interface version >= 2)
	// Parse a single line from a scenario file into VESSELSTATUSx status definition

	bool ParseScenarioLineDirect (char *line);
	// Parse a single line directly into vessel structure, bypassing any VESSELSTATUSx structure
	// This is used for any status options that are not currently represented in VESSELSTATUSx

	DWORD PackDefaultState (char **data, DWORD flag);
	// pack the default vessel state parameters into a data buffer
	// (e.g. for passing between remote sessions)
	// *data is dynamically allocated and must be deallocated by the caller after use
	// return value is the size of the buffer

	void ApplyPackedState (const char *data);
	// Set the vessel state from the packed parameters

	void WriteDefault (std::ostream &ofs) const;
	// Writes standard vessel status parameters, while "Write" allows
	// custom output by vessel modules implementing clbkSaveState

	void SetDefaultState ();
	// set default status parameters

	void DefaultGenericCaps ();
	// set generic vessel caps to (fairly arbitrary) defaults to prevent
	// catastropic failures for undefined caps

	void ReadGenericCaps (std::ifstream &ifs);
	// read generic vessel caps from a class cfg file

	UINT AddMesh (const char *mname, const VECTOR3 *ofs = 0);
	// add a mesh with offset to the list (load from file). Return value is mesh index

	UINT AddMesh (MESHHANDLE hMesh, const VECTOR3 *ofs = 0);
	// add a mesh with offset to the list (copy from handle). Return value is mesh index

	UINT InsertMesh (const char *mname, UINT idx, const VECTOR3 *ofs = 0);
	// Insert a mesh at a particular position (load from file). If a mesh is already registered
	// with index 'idx', the existing mesh is replaced

	UINT InsertMesh (MESHHANDLE hMesh, UINT idx, const VECTOR3 *ofs = 0);
	// Insert a mesh at a particular position (copy from handle). If a mesh is already registered
	// with index 'idx', the existing mesh is replaced

	bool DelMesh (UINT idx, bool retain_anim = false);
	// Remove mesh with index 'idx'. Returns false if idx is out of range
	// If retain_anim=true, animation components associated with the mesh
	// are not removed.

	void ClearMeshes (bool retain_anim = false);
	// remove all entries from mesh list
	// If retain_anim=true, animations are not removed.

	bool ShiftMesh (UINT idx, const VECTOR3 &ofs);
	// shift mesh 'idx' by 'ofs' from current position

	const MESHHANDLE GetMeshTemplate (UINT idx) const;
	// returns template handle for pre-loaded mesh, or NULL if not available

	const char *GetMeshName (UINT idx) const;
	// returns mesh name for on-demand mesh, or NULL for pre-loaded mesh

	Mesh *CopyMeshFromTemplate (UINT idx);
	// Creates a new copy of the vessel's idx-th mesh, either by
	// copying from the preloaded template, or by reading from file.
	// If idx is out of range, or the mesh could not be read, the
	// return value is 0.
	// If vismode!=0, it will be set to the mesh visibility mode.

	void SetMeshVisibilityMode (UINT meshidx, WORD mode);
	// set visibility flag for mesh in cockpit view/external view

	int MeshModified (MESHHANDLE hMesh, UINT grp, DWORD modflag);
	// Notify the visualisation subsystem of a modification of a mesh group

	bool LoadModule (std::ifstream &classf);
	// Load vessel module, if defined in vessel class configuration file

	bool RegisterModule (const char *dllname);
	// register a vessel library

	void ClearModule ();
	// clear interface pointers and unload module

	HINSTANCE hMod;        // module handle
	struct {               // module interface
		VESSEL *v;
		int version;
		VESSEL_Init ovcInit;
		VESSEL_Exit ovcExit;
		bool coreCreated;
	} modIntf;

	TOUCHDOWN_VTX *HullvtxFirst ();
	TOUCHDOWN_VTX *HullvtxNext ();

private:
	void UpdateMass ();
	// update vessel mass (empty + sum of all fuel resources)

	inline void ResetMass () { UpdateMass(); pfmass = fmass; }
	// as UpdateMass, but also resets pfmass (previous fuel mass)

	inline void FlushThrusterLevel (ThrustSpec *ts)
	{
		if (ts->tank && ts->tank->mass)
			ts->level = std::max (0.0, std::min (1.0, ts->level_permanent + ts->level_override));
	}
	// set thruster level as sum of permanent and override setting

	bool SetGravityGradientDamping (double damp);
	// Sets the damping factor for gravity field gradient-induced torque
	// Returns false if gravity gradient torque is disabled in the launchpad

	double GetGravityGradientDamping () const;
	// Returns the damping factor for gravity field gradient-induced torque

	void SetProxyplanet (Planet *pp);
	// set proxy planet

	void InitLanded (Planet *planet, double lng, double lat, double dir, const Matrix *hrot=0, double cgelev=0.0, bool asComponent=false);
	// init vessel position on ground
	// Note: InitLanded must be called after the update phase (after EndStateUpdate)

	void InitDocked (const Vessel *vessel, int port);
	// init vessel docked in orbit to another vessel

	void InitOrbiting (const Vector &relr, const Vector &relv, const Vector &rot, const Vector *_vrot = 0);
	// init vessel as orbiting around cbody with rel. position relr, rel. velocity relv and
	// orientation rot (all w.r.t. ecliptic axis orientation). _vrot is rotation vector (rad/s) around the
	// three axes, if provided

	void InitNavRadios (DWORD n);
	// define the number of navigation radios supported by the vessel

	DWORD IncRadioChannel (DWORD ch, int step) const;
	// Generic functions to step through radio frequencies

	void UpdateProxies ();
	// check for grav reference body, and closest planet, base and station

	void UpdateReceiverStatus (DWORD idx = 0xffff);
	// update reception status for NAV receiver idx (or for all by default)

	double IlluminationFactor () const;
	// returns the sun illumination factor (0-1) at current position.
	// only considers shadowing effect from closest planet
	// currently ignores atmospheric effects

	UINT MakeFreeMeshEntry (UINT idx = (UINT)-1);
	// If 'idx'==-1, return a pointer to an unused entry into the mesh list (add new entry if required)
	// If 'idx' is set, force this entry to be created (may replace an existing entry)

	void ScanMeshCaps ();
	// check for mesh parameters

	// thruster specs
	std::vector<ThrustSpec*> m_thruster;         ///< list of thruster definitions
	double m_defaultIsp;                         ///< default fuel specific impulse [m/s] for new thrusters
	bool m_bThrustEngaged;                       ///< true if any thrusters are engaged at current time step

	// thruster group specs
	std::array<ThrustGroupSpec, 15> m_thrusterGroupDef; ///< list of default thruster groups (see THGROUP_TYPE in OrbiterAPI.h)
	std::vector<ThrustGroupSpec*> m_thrusterGroupUsr;   ///< list of user-defined groups

	// exhaust specs
	EXHAUSTSPEC **exhaust;                       // list of exhaust definitions
	DWORD nexhaust;                              // length of exhaust list
	OldExhaustSpec **oexhaust;                   // list of old (obsolete) exhaust definitions
	DWORD noexhaust;                             // length of oexhaust list

	// particle system
	oapi::ParticleStream **contrail;
	DWORD ncontrail;
	oapi::ParticleStream **reentrystream;
	DWORD nreentrystream;

	// propellant resource specs
	TankSpec **tank;                             // list of propellant resource definitions
	DWORD ntank;                                 // length of propellant list
	TankSpec *def_tank;                          // default propellant handle (for generic HUD display)
	double max_angular_moment[6];                // max angular momentum for the 6 standard rotational attitude thruster groups

	// airfoil specs
	AirfoilSpec **airfoil;
	DWORD nairfoil;

	// airfoil control surface specs
	CtrlsurfSpec **ctrlsurf;                     // list of airfoil control surface definitions
	DWORD nctrlsurf;                             // length of control surface list
	struct {                                     // status for elevator,rudder,aileron etc. (-1..+1)
		double ttgt;                                 // target setting (transient)
		double ptgt;                                 // target setting (permanent)
		double curr;                                 // actual current setting
		double delay;                                // response delay (neutral->full) [s]
	} ctrlsurf_level[6];
	int CtrlSurfSyncMode;                        // syncing of actual to target required? (0=no, 1=normal, 2=direct)
	bool bElevTrim;                              // flag for vessel with elevator trim control

	// drag element specs
	DragElementSpec **dragel;                    // list of variable drag element definitions
	DWORD ndragel;                               // length of drag element list

	// docking port specs
	PortSpec **dock;                             // list of docking port definitions
	DWORD ndock;                                 // length of docking port list
	int dockmode;                                // 0=2006 legacy mode, 1=2010 mode
	double undock_t;                             // time of last undock event

	// parent/child attachment specs
	AttachmentSpec **pattach;                    // "attachment to parent" list
	AttachmentSpec **cattach;                    // "attachment to children" list
	DWORD npattach, ncattach;                    // list lengths
	AttachmentSpec *attach;                      // the current attachment to a parent, if applicable
	Matrix attach_rrot;                          // rotation matrix from vessel to current parent
	Vector attach_rpos;                          // position of vessel in current parent's frame

	struct {                                     // this structure is used when parsing attachment
		DWORD ci, pi;                            // info from a scenario to prepare deferred attachment
		char *pname;                             // should be merged into VESSELSTATUS
	} attach_status;

	// navigation radios
	NavRadioSpec *nav;                           // list of nav radio settings
	DWORD nnav;                                  // number of nav radios

	// transponder
	Nav_XPDR *xpdr;

	// beacon lights
	BEACONLIGHTSPEC **beacon;
	DWORD nbeacon;

	LightEmitter **emitter;
	DWORD nemitter;

	// reentry texture definition
	ReentryRenderSpec reentry;

	// locally defined MFD modes
	DWORD nmfdmode;                              // number of locally defined modes
	MFDMODE *mfdmode;                            // list of locally defined modes

	double emass, fmass, pfmass; // empty mass, current total fuel mass, previous total fuel mass
	double cog_elev;             // elevation of centre of gravity over ground when landed
	double CWz[2], CWx, CWy;     // wind resistance form factors (forward/backward,vert,side)
	double wingfactor;           // aspect ratio * effectiveness factor // OBSOLETE
	double wingaspect, wingeff;  // wing form factors
	double pitch_moment_scale;   // scale factor for pitch moment
	double bank_moment_scale;    // scale factor for bank moment
	double mu, mu_lng;           // default touchdown friction coeffs in lateral and longitudinal directions
	double max_wbrake_F;         // max. deceleration force with fully deployed wheel brakes
	double wbrake_permanent[2];  // current permanent wheel brake level [0..1] (left and right)
	double wbrake_override[2];   // wheel brake override level for current frame [0..1] (left and right)
	double wbrake[2];            // current wheel brake levels
	Vector cs;                   // ship's cross-sections in the three axis direction (z=longitudinal) [m^2]
	Vector rdrag;                // resistance constant against rotation in the three directions
	bool enablefocus;            // can vessel get input focus?
	bool burnfuel;               // no unlimited fuel
	bool extpassmesh;
	int attmode;                 // 0=disabled, 1=rotational, 2=transversal attitude thruster mode
	DWORD ctrlsurfmode;          // disable/enable airfoil control surfaces. bit0=elevator, bit1=rudder, bit2=aileron

	bool nosesteering;           // nosewheel steering active?
	double nosewheeldir;         // nosewheel orientation (-1..1)

	double E0_comp;              // compression energy due to surface impact at current time step
	mutable double E_comp;       // compression energy due to surface impact after last AddSurfaceForces call

	double vd_forw, vd_back, vd_vert, vd_side;  // resistance constants against translation in atmosphere

	double clipradius;           // near-plane clipping radius (default=size)
	double proxydist;            // distance to proxybody
	double proxyalt;             // surface distance
	double proxyT;               // next check for proxybody
	double commsT;               // next check for comms status

	mutable double lightfac;                 // sun illumination factor (0-1)
	mutable double lightfac_T0, lightfac_T1; // time validity range of current lightfac value

	// Information about closest dock we may be docking to with our dock 0
	struct {
		double dist;
		Vessel *vessel;
		DWORD dock;
	} closedock;

	Vessel *proxyvessel;      // closest vessel
	SuperVessel *supervessel; // vessel superstructure (docking complex)
	Base    *landtgt;         // landing target (base)
	int   lstatus;            // landing/docking comms status (0=no contact, 1=contact,
	DWORD nport;              // allocated landing pad/docking port no (>=0, (DWORD)-1=none)
	UINT  scanvessel;         // next vessel to check for docking event

	mutable bool surfprm_valid;
	bool pyp_valid;
	double surf_gacc;      // gravitational acceleration when on ground
	double surf_rad;       // surface distance from planet centre at landing site
	Matrix rot_land;       // vessel's local rotation matrix when landed on a planet surface
					       // such that grot = grot(planet) * rot_land
	Vector touchdown_nm;   // upward normal of touchdown plane (vessel frame)
	Vector touchdown_cg;   // projection of CG onto touchdown plane
	TOUCHDOWN_VTX *touchdown_vtx;
	DWORD ntouchdown_vtx;    // number of touchdown vertices
	DWORD next_hullvtx;      // used by hull vertex iterator

	Vector campos;             // internal camera position (cockpit mode);
	Vector camdir0;            // internal default camera direction (cockpit mode)
	double camtilt0;           // internal default camera rotation around default direction (cockpit mode)
	double camcatchangle;      // internal camera autochenter catch range
	double camdp_left, camdp_right, camdt_up, camdt_down; // cockpit camera rotation ranges
	static struct LeanCam {    // cockpit camera 'leaning' ranges'
		LeanCam() { phi = tht = 0; }
		Vector pos;
		double phi, tht;
	} camfwd, camleft, camright;

	double trim_scale;         // effect of trimming (0 = can't trim) - OBSOLETE

	Vector Flin;               // linear moment (force)
	Vector Amom;               // angular moment (torque)
	Vector Flin_add;           // linear body force
	Vector Amom_add;           // used for collecting torque components
	mutable Vector Torque;     // torque vector
	mutable bool torque_valid; // flag for 'Torque' up to date

	Vector Thrust;             // linear thrust vector (sum of all thruster contributions)
	mutable Vector Weight;     // weight vector (due to gravitational acceleration)
	mutable bool weight_valid; // flag for 'Weight' up to date
	double Lift, Drag, SideForce;         // current lift and drag magnitudes

	DWORD navmode;             // bitflags for currently active navmodes
	struct HoverHoldAlt {      // Hover hold altitude data
		double alt;            //   hover hold target altitude
		double palt;           //   hover hold previous altitude
		double valt;
		double T;
		bool terrain;          //   hold true altitude?
	} hoverhold;
	//double refalt, palt, valt; // reference+prev altitude for "hold altitude" mode
	//double holdaltT;

	Vector *forcevec;  // list of force vectors to render
	Vector *forcepos;  // list of force vector attack points
	int forcevecbuf;   // length of vector list
	mutable int nforcevec;     // number of vectors to render

	char *classname;   // vessel class name
	char *onlinehelp;  // string for online help support (or NULL if none)

	struct MeshList {
		Str64 meshname;    // mesh file names for the visual
		MESHHANDLE hMesh;  // pointer to preloaded mesh, if present
		VECTOR3 meshofs;   // mesh offset in vessel coords
		DWORD_PTR crc;         // mesh id
		WORD vismode;      // visibility mode: 1=external only, 2=internal only, 3=both
	} **meshlist;
	UINT nmesh;        // number of meshes
	DWORD_PTR mesh_crc;    // visual state checksum

	UINT exhaust_id;   // next exhaust id to attach

	int killrot_delay;   // delay counter for killrot terminate
	bool kill_pending;
	static bool rpressure;      // flag for applying radiation pressure effects
	int orthoaxis;       // auxiliary flag
	int animcount;       // animation request ref count

	oapi::Sketchpad *hudskp;    // hack: hud drawing instance (for callback)

	LiftCoeffFunc LiftCoeff;    // function pointer for aoa-dependent lift coeff calculation - OBSOLETE

	int flightmodel;            // flight model complexity; 0=simple, 1=realistic

	static bool ClbkSelect_Undock (Select *menu, int item, char *str, void *data);
	static bool ClbkEnter_Undock (Select *menu, int item, char *str, void *data);
	static bool ClbkName_Undock (InputBox*, char *str, void *data);

	// =======================================================
	// Flight recorder routines (should be a class!)

private:
	std::ifstream *FRatc_stream;

	bool bRequestPlayback;
	bool bFRplayback;
	// True if vessel is currently played back

	bool bFRrecord;
	// True if vessel is currently recording is flight data

	FRecord frec_last;
	// Last saved flight status

	char *FRfname;
	// flight record file name

	FRecord *frec;
	int nfrec;
	int cfrec;
	// Playback sample list, list length, current sample

	FRecord_att *frec_att, frec_att_last;
	int nfrec_att;
	int cfrec_att;
	// Playback attitude sample list

	double frec_last_syst;
	double frec_att_last_syst;
	// system time for last sample output (pos, att)

	double *frec_eng;
	DWORD nfrec_eng;
	double frec_eng_simt;
	// Current engine status list

	void FRecorder_Reset ();
	// Initialise flight recorder

	void FRecorder_Activate (bool active, const char *fname, bool append = false);
	// switch recorder on/off

	void FRecorder_Save (bool force = false);
	// save current status to flight record streams

	void FRecorder_SaveEvent (const char *event_type, const char *event);
	void FRecorder_SaveEventInt (const char *event_type, int event);
	void FRecorder_SaveEventFloat (const char *event_type, double event);
	// save a single custom event to the articulation stream

	void FRecorder_Clear ();
	// Delete playback sample list

	bool FRecorder_Read (const char *fname);
	// read playback sample list from file

	void FRecorder_Play ();
	// set vessel status from playback sample list

	void FRecorder_PlayEvent ();

	void FRecorder_CheckEnd ();
	// checks if end of playback is reached, and performs
	// cleanup operations if it is

	void FRecorder_EndPlayback ();
	// terminates a running playback

	// =======================================================
	// Routines for automatic animation support

// BEGIN OBSOLETE

	UINT AddAnimSeq (double defmeshstate);
	// add an animation sequence to the list
	// defmeshstate (0..1) is the animation state of this sequence stored in the original mesh

	bool AddAnimSeqComp (UINT seq, ANIMCOMP *comp);
	// add a component to an existing animation sequence

	bool SetAnimState (UINT seq, double state);
	// sets the state (0..1) of an animation sequence

	void ClearAnimSeqs (void);
	// removes all animation sequences

	UINT nanimseq;        // number of animation sequences
	ANIMSEQ *animseq;     // list of animation sequences

// END OBSOLETE

	UINT CreateAnimation (double initial_state);
	// Create a new animation
	// initial_state (0..1) is the state of the animation of the unmodified mesh

	ANIMATIONCOMP *AddAnimationComponent (UINT an, double state0, double state1,
		MGROUP_TRANSFORM *trans, ANIMATIONCOMP *parent = NULL);
	// Add a new component to animation 'an' with end states 'state0' and 'state1'
	// 'trans' defines the transform. If 'parent' is defined, the animation is
	// inserted as a child of 'parent'

	bool DelAnimationComponent (UINT an, ANIMATIONCOMP *comp);
	// Remove a component from an animation

	bool SetAnimation (UINT an, double state);
	// Set the state (0..1) of animation 'an'

	double GetAnimation (UINT an);
	// Returns the current state of animation 'an'

	bool DelAnimation (UINT an);
	// Deletes animation 'an' by removing all its components.
	// The animation state of the mesh is reset to its default position
	// before the animation is removed.

	void ClearAnimations (bool reset);
	// Remove all animations. If reset==true, all animated groups on visuals
	// are reset to their initial states before the animations are destroyed

	UINT nanim;			// number of animations
	ANIMATION *anim;	// list of animations

	bool EditorModule (char *cbuf) const;
	// Returns the file name of the module containing the scenario editor
	// extensions for the vessel

	// API interface functions
	friend OAPIFUNC double oapiGetMaxFuelMass (OBJHANDLE hVessel);
	friend OAPIFUNC PROPELLANT_HANDLE oapiGetPropellantHandle (OBJHANDLE hVessel, DWORD idx);
};

#endif // !__VESSEL_H


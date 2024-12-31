// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// =======================================================================
// CelestialBody:
// Base class for stars, planets, moons etc.
// =======================================================================

#ifndef __CELBODY_H
#define __CELBODY_H

#include "Rigidbody.h"
#include "OrbiterAPI.h"
#include "PinesGrav.h"

// Module interface methods - OBSOLETE
typedef void   (*OPLANET_SetPrecision)(double prec);
typedef int    (*OPLANET_Ephemeris)(double mjd, double *ret, int &format);
typedef int    (*OPLANET_FastEphemeris)(double mjd, double *ret, int &format);
typedef void   (*OPLANET_AtmPrm)(double alt, ATMPARAM *prm);

// =======================================================================
// Class CelestialBody
// =======================================================================

class CelestialBody: public RigidBody {
friend class CELBODY;
friend class CELBODY2;

public:
	CelestialBody (double _mass, double _size);
	CelestialBody (char *fname);
	virtual ~CelestialBody ();

	void DefaultParam ();
	virtual void Setup ();
	virtual int Type() const { return OBJTP_CBODY; }
	virtual void Update (bool force);

	CELBODY *GetModuleInterface() { return module; }
	// module interface pointer, if available

	void Attach (CelestialBody *_parent);
	// Set the objects's central body

	bool isMoon() const { return (cbody && cbody->Type() == OBJTP_PLANET); }
	// body is not a primary planet

	inline const CelestialBody *Primary() const { return cbody; }

	inline DWORD nSecondary() const { return nsecondary; }
	inline const CelestialBody *Secondary (DWORD i) const { return secondary[i]; }

	inline double RotT() const { return rot_T; }
	// rotation period

	inline double Rotation() const { return rotation; }
	// current rotation angle

	inline double Obliquity() const { return eps_ecl; }
	// Return axis obliquity against ecliptic normal (J2000)

	inline double EqLng() const { return lan_ecl; }
	// Return axis longitude of ascending node against equinox (J2000)

	inline const Matrix &RotObliq() const { return R_ecl; }
	// WARNING: This returned R_ref_rel before - the change will have an effect
	// on calculation of orbital elements in equatorial frame, and on
	// oapiGetPlanetObliquityMatrix

	inline const Vector &RotAxis() const { return R_axis; }
	// rotation axis (direction of north pole) in global coords

	const Elements *Els() const;
	// Return elements for the planet or 0 if none

	int RelTrueAndBaryState();
	// Calculate the body's true and barycentre state with respect to
	// the parent's position. This recursively steps through all the
	// body's secondary bodies to find the barycentre offset
	// Return value: 0 if state is calculated relative to parent true state,
	// or EPHEM_PARENTBARY if calculated relative to parent barycentre

	void AbsTrueState();
	// Calculate the body's true position and velocity as the sum of the
	// parent state and its own relative state.
	// Recursively updates all secondary body states.

	bool PositionAtTime (double t, Vector *p) const;
	// Returns planet's position p at simulation time t [s] in ecliptic frame,
	// relative to planet's parent. This only works if planet updates
	// its position analytically, otherwise the function returns false.

	bool PosVelAtTime (double t, Vector *p, Vector *v) const;
	// Returns planet's position p and velocity v at simulation time t [s] in
	// ecliptic frame, relative to planet's parent. Only works if planet updates
	// position analytically, otherwise function returns false

	void GetRotation (double t, Matrix &rot) const;
	// Returns rotation matrix at time t.
	// Note: this function assumes current precession, i.e. t sufficiently close to td.SimT0

	Vector InterpolatePosition (double n) const;
	// interpolate a planet position to a time between last and current time step,
	// where n=0 refers to last step, and n=1 to current step.
	// linear interpolation of position, plus linear interpolation of radius, if
	// body's element reference exists

	StateVectors InterpolateState (double n) const;
	// Celestial body state vectors at fractional time n [0..1] between
	// s0 at td.SimT0 and s1 at td.SimT1
	// Note: This function can only be called during the state calculation phase, after
	// s1 has been evaluated, but before it is copied back to s0

	const Vector &Barycentre () const { return bpos; }
	// Returns position of barycentre (planet + secondaries)

	inline DWORD nJcoeff() const { return njcoeff; }
	inline double Jcoeff (DWORD i) const { return jcoeff[i]; }
	// returns number of coefficients and individual coefficients for planet
	// shape description for nonspherical gravity calculation. Note that the
	// first coefficient Jcoeff(0) is J2

	// returns true if the body uses Pines Algorithm to calculate gravitational acceleration from spherical harmonics
	inline bool usePines() const { return usePinesGravity; }
	inline Vector pinesAccel(const Vector rposmax, const int maxDegree, const int maxOrder){
		return pinesgrav.GetPinesGrav(rposmax, maxDegree, maxOrder);
	}
	inline unsigned int GetPinesCutoff() const {
		return pinesgrav.GetCoeffCutoff(); 
	}

protected:
	//Matrix R_ref_rel;     // rotation matrix for tilting the axis of rotation (including precession)
	Matrix R_ecl;         // precession matrix
	double rotation;      // current rotation angle
	double rotation_off;  // rotation offset due to precession

	DWORD nsecondary;
	CelestialBody **secondary;
	// List of secondary bodies (i.e. planets for a central star, or moons for a planet)

	void AddSecondary (CelestialBody *sec);
	// Add an object as a secondary (e.g. moon)
	// Secondaries are used e.g. for barycentre calculations

	Vector Barycentre2Pos (const Vector &bary) const;
	virtual Vector Pos2Barycentre (const Vector &pos) const;
	// Returns object's position from system barycentre or vice versa by scanning
	// all secondary bodies.

	enum {ELFRAME_ECLIPTIC, ELFRAME_PARENTEQU} elframe;
	// reference frame for orbital elements specified in config file
	// only used for secondary objects (moons)

	bool bInitFromElements;
	// flag for reading osculating elements from configuration file

	int ExternEphemeris (double mjd, int req, double *res) const;
	// Try to obtain ephemeris data at mjd from external module
	// req contains data request flags, return value contains satisfied requests

	int ExternFastEphemeris (double simt, int req, double *res) const;
	// Try to obtain fast sequential ephemeris data at simt from
	// external module.
	// Returns false if not supported by module

	int ExternState (double *res);
	// Try to obtain current ephemeris data (true and barycentric) from external
	// module. This tries first FastEphemeris, then Ephemeris. Return value
	// contains data flags returned by the ephemeris functions

	int ExternPosition ();
	// Try to obtain current ephemeris data (true and barycentric) from external
	// module. This tries first FastEphemeris, then Ephemeris. Return value
	// indicates which data could be updated

	void UpdatePrecession ();
	// Calculate precession parameters at the current simulation date

	void UpdateRotation ();
	// Calculate rotation parameters at the current simulation date

	void RegisterModule (char *dllname);
	void ClearModule ();
	CELBODY *module;         // pointer to module interface class, if available

	bool bFixedElements;
	// Set this to true if the object's elements never change
	// (i.e. using a 2-body approximation)

	struct {  // module interface - OBSOLETE
		OPLANET_SetPrecision oplanetSetPrecision;
		OPLANET_Ephemeris oplanetEphemeris;
		OPLANET_FastEphemeris oplanetFastEphemeris;
		OPLANET_AtmPrm oplanetAtmPrm;
	} modIntf;

private:
	HINSTANCE hMod;          // module handle, if available
	
	double eps_ref;          // precession reference axis: obliquity against ecliptic normal
	double lan_ref;          // precession reference axis: longitude of ascending node in ecliptic
	Matrix R_ref;            // rotation matrix: ecliptic normal -> precession reference axis

	double eps_rel;          // obliquity relative to reference axis
	double cos_eps, sin_eps; // cos(eps_rel), sin(eps_rel)
	double Lrel;             // longitude of ascending node relative to reference axis at current time
	double Lrel0;            // longitude of ascending node relative to reference axis at time mjd_rel
	double mjd_rel;          // date to which lan_rel refers
	double prec_T;           // precession period [days], or 0 if infinite
	double prec_omega;       // precession angular velocity [rad/day]

	double eps_ecl;          // obliquity of axis against ecliptic normal
	double lan_ecl;          // longitude of ascending node: angle between x-axis and ascending node of equator

	double rot_T, rot_omega; // siderial rotation time and angular velocity
	double Dphi;             // rotation offset at t=0
	Vector R_axis;           // rotation axis direction (north pole) in global coords

	double *jcoeff;          // coefficients Jn of the harmonic expansion of planet ellipsoid shape, starting with J2 (jcoeff[0]=J2, jcoeff[1]=J3, etc.)
	DWORD njcoeff;           // number of coefficients in the jcoeff list

	PinesGravProp pinesgrav; // coefficients and methods for calculating non-spherical gravity vectors using Pines Algorithm
	bool usePinesGravity;    // use Pines Algorithm if true, if false use the older jcoeff method

	Vector bpos, bvel;       // object's barycentre state (the barycentre of the set of bodies including *this and its children) with respect to the true position of the parent of *this
	Vector bposofs, bvelofs; // body barycentre state - true state
	bool ephem_parentbary;   // true if body calculates its state with respect to the parent barycentre, false if with respect to parent's true position
};

#endif

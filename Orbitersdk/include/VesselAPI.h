// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
//                     ORBITER SOFTWARE DEVELOPMENT KIT
// VesselAPI.h
// - VESSEL class interface
// - VESSEL2 class extensions
// - VESSEL3 class extensions
// ======================================================================

/**
 * \file VesselAPI.h
 * \brief Contains the class interfaces for vessel objects
 *   (VESSEL, VESSEL2, VESSEL3).
 */

#ifndef __VESSELAPI_H
#define __VESSELAPI_H

#if defined(_MSC_VER) && (_MSC_VER < 1920 ) // Microsoft Visual Studio Version 2017 and lower
#include <algorithm>
#endif

// reference frame flags
#define FRAME_ECL 0
#define FRAME_EQU 1

#include <typeinfo>

class Vessel; // Orbiter internal vessel class
class SuperVessel; // Orbiter internal supervessel class

/**
 * \brief Collision vertex definition
 */
typedef struct {
	VECTOR3 pos;      ///< collision point position (vessel frame)
	double stiffness; ///< suspension stiffness coefficient
	double damping;   ///< suspension damping coefficient
	double mu;        ///< isotropic/lateral friction coefficient
	double mu_lng;    ///< longitudinal friction coefficient (only used for first 3 points)
} TOUCHDOWNVTX;

enum class VisualProp {
	BAKED_LIGHT,		///< baked light level
	AMBIENT,			///< ambient light level
	EXT_PROBE_POS,		///< Exterior probe position
	CREATE_VC_PROBE,	///< Virtual cockpit probe position
	DA_CURVE,
	DA_BOUNCH,
	DA_FORCE
};

//  ======================================================================
/**
 * \brief Base class for objects of vessel type (spacecraft and similar)

 * %VESSEL is the base class for addon modules of 'vessel' type (spacecraft,
 * space stations, satellites, deep space probes, etc.)
 * This class defines the interface between the module's vessel definition
 * and the parameters maintained internally by Orbiter to define the vessel
 * state.
 * It provides access to the various status parameters and methods of
 * individual spacecraft.
 *
 * It is important to note that a %VESSEL instance represents an \e interface
 * to an existing vessel in Orbiter, rather than the vessel itself. Vessels
 * can exist without a corresponding %VESSEL instance, and deleting a
 * %VESSEL instance does not delete the vessel.
 *
 * Most of the methods provided by the %VESSEL class are of 'get' and 'set'
 * type, i.e. for retrieving vessel parameter states, or modifying them.
 * It does \e not define any callback functions that Orbiter uses to notify
 * the vessel of events. These are implemented in the VESSEL2 class (derived
 * from %VESSEL). The latest version of the interface is VESSEL3, which
 * implements additional functions. User-defined vessel casses should therefore
 * be derived from %VESSEL3 instead of %VESSEL.
 *
 * For complete vessel module implementations, see the examples in
 * Orbitersdk\\samples, for example Orbitersdk\\samples\\ShuttlePB.
 * \nosubgrouping
 */
//  ======================================================================

class OAPIFUNC VESSEL {
public:
	/// \name Construction/creation, handles and interfaces
	//@{
	/**
	 * \brief Creates a %VESSEL interface instance from a vessel handle.
	 * \param hVessel vessel handle
	 * \param fmodel level of realism requested (0=simple, 1=realistic)
	 * \note This function creates an interface to an \e existing vessel. It
	 *   does not create a new vessel. New vessels are created with the
	 *   \ref oapiCreateVessel and \ref oapiCreateVesselEx functions.
	 * \note The %VESSEL constructor (or the constructor of a derived
	 *   specialised vessel class) will normally be invoked in the ovcInit
	 *   callback function of a vessel module:
	 * \code
	 * class MyVessel: public VESSEL
     * {
     *     // MyVessel interface definition
     * };
     *
     * DLLCLBK VESSEL *ovcInit (OBJHANDLE hvessel, int flightmodel)
     * {
     *     return new MyVessel (hvessel, flightmodel);
     * }
     *
     * DLLCLBK void ovcExit (VESSEL *vessel)
     * {
     *     delete (MyVessel*)vessel;
     * }
	 * \endcode
	 * \note The %VESSEL interface instance created in ovcInit should be
	 *   deleted in ovcExit.
	 * \sa oapiCreateVessel, oapiCreateVesselEx, ovcInit
	 */
	VESSEL (OBJHANDLE hVessel, int fmodel = 1);

	/**
	 * \brief Returns the version number of the vessel interface class.
	 * \return version number
	 * \note The following interface versions are currently in use:
	 *  - class VESSEL: version 0
	 *  - class VESSEL2: version 1
	 *  - class VESSEL3: version 2
	 * \sa VESSEL2, VESSEL3
	 */
	inline int Version () const { return version; }

	/**
	 * \brief Returns a handle to the vessel object.
	 * \return vessel handle, as passed to the VESSEL constructor.
	 * \note The handle is useful for various vessel-related API function calls.
	 */
	const OBJHANDLE GetHandle () const;

	/**
	 * \brief Returns the file name of the DLL containing the vessel's
	 *   scenario editor extensions.
	 * \param [out] fname module file name
	 * \return \e true if the vessel defines an editor module,
	 *   \e false otherwise.
	 * \note The vessel's editor module, if it exists, contains extensions
	 *   for the <i>Scenario editor</i> module that allows the user to
	 *   set vessel-specific parameters (see Doc\ScenarioEditor.pdf).
	 * \note The string returned by this method is identical to the
	 *   \e EditorModule entry in the vessel's configuration file.
	 * \note If the \e EditorModule entry is not found in the configuration
	 *   file, this method returns \e false.
	 */
	bool GetEditorModule (char *fname) const;
	//@}

	/// \name General vessel properties
	//@{
	/**
	 * \brief Returns the vessel's name.
	 * \return Pointer to vessel's name
	 * \sa GetClassName
	 */
	char *GetName () const;

	/**
	 * \brief Returns the vessel's class name.
	 * \return Pointer to vessel's class name.
	 * \sa GetName
	 */
	char *GetClassName () const;

	/**
	 * \brief Returns the requested realism level for the flight model.
	 * \return Flight model realism level. These values are currently
     * supported:
	 * - 0 = simple
	 * - 1 = realistic
	 * \note The returned value corresponds to that passed to the VESSEL
	 *   constructor. This will normally be the same as the argument of the
	 *   ovcInit callback function.
	 * \note The module can use this method to implement different flavours of
	 *   the flight model (e.g. simplified and realistic), by defining
	 *   separate sets of parameters (possibly higher fuel-specific impulse
	 *   and higher thrust ratings in the simplified model, less severe
	 *   damage limits, etc.)
	 * \sa ovcInit, GetDamageModel
	 */
	int GetFlightModel () const;

	/** 
	 * \brief Returns the current user setting for damage and systems
	 *   failure simulation.
	 * \return Damage modelling flags. The following settings are currently
     *   supported:
	 *   - 0 = no damage or failures
	 *   - 1 = simulate vessel damage and system failures
	 * \note The return value depends on the user parameter selection in the
	 *   Launchpad dialog. It does not change during a simulation session
	 *   and will be the same for all vessels.
	 * \note Future versions may support more differentiated bit flags to
	 *   indicate different types of damage and failure simulation.
	 * \note A vessel implementation should query the damage flag to decide
	 *   whether to simulate failures.
	 * \sa GetFlightModel
	 */
	int GetDamageModel () const;

	/**
	 * \brief Returns true if the vessel can receive the input focus, false
	 * otherwise.
	 * \return Focus enabled status.
	 * \note The vessel can be allowed or prohibited to receive the input focus
	 *   by using the SetEnableFocus method.
	 * \note The initial state is defined by the EnableFocus setting in the
	 *   vessel's configuration file. If the entry is missing, the default
	 *   is true.
	 * \note Focus-enabled vessels can be selected by the user via the jump
	 *   vessel dialog (F3).
	 * \note Once a vessel has received the input focus, all user input via
	 *   keyboard, mouse and joystick is directed to this vessel.
	 * \note For some object types, such as jettisoned rocket stages, enabling
	 *   input focus may not be useful.
	 * \sa SetEnableFocus, clbkFocusChanged, oapiGetFocusObject,
	 * oapiSetFocusObject
	 */
	bool GetEnableFocus () const;

	/**
	 * \brief Enable or disable the vessel's ability to receive the input
	 *   focus. 
	 * \param enable focus enabled status: true to to allow the vessel to
	 *   receive input focus, false otherwise.
	 * \note The initial state is defined by the EnableFocus setting in the
	 *   vessel's configuration file. If the entry is missing, the default
	 *   is true.
	 * \note If the input focus of the current focus vessel is disabled, it
	 *   will continue to receive user input, until the focus is switched
	 *   to another vessel.
	 * \note Focus-enabled vessels can be selected by the user via the jump
	 *   vessel dialog (F3).
	 * \note Once a vessel has received the input focus, all user input via
	 *   keyboard, mouse and joystick is directed to this vessel.
	 * \note For some object types, such as jettisoned rocket stages, enabling
	 *   input focus may not be useful.
	 * \sa GetEnableFocus, clbkFocusChanged, oapiGetFocusObject,
	 *   oapiSetFocusObject
	 */
	void SetEnableFocus (bool enable) const;

	/**
	 * \brief Returns the vessel's mean radius.
	 * \return Vessel mean radius [m].
	 * \note The value returned is that set by a previous call to SetSize or
	 *   from the Size entry in the vessel's configuration file.
	 * \note There is no guarantee that the return value is correlated to the
	 *   vessel's visual representation. In particular, the size parameter
	 *   does not change (scale) the visual appearance. 
	 * \sa SetSize
	 */
	double GetSize () const;

	/**
	 * \brief Set the vessel's mean radius.
	 * \param size vessel mean radius [m].
	 * \note The size should correspond to the vessel's visual representation,
	 *   for example the mesh used to show the vessel in the simulation
	 *   window.
	 * \note The size parameter is used by Orbiter to determine the camera
	 *   distance at which the vessel is within visual range of the
	 *   observer camera. It is also used for calculating various physical
	 *   parameters.
	 * \note If SetSize is not called during the vessel setup, the value from
	 *   the Size entry in the vessel's configuration file is used.
	 * \sa GetSize
	 */
	void SetSize (double size) const;

	/**
	 * \brief Defines the vessel's range of visibility.
	 * \param vislimit apparent size limit for vessel visibility
	 * \param spotlimit apparent size limit for vessel "spot"
	 * representation.
	 * \note This function can be used to define the distance up to which a
	 *   vessel is visible, independent of screen resolution.
	 * \note The vislimit value is the limiting apparent size (as a fraction of
	 *   the render window vertical) up to which the vessel is regarded
	 *   visible. Thus, the vessel is visible if the following condition is
     *   satisfied: \f$S (d \tan a)^{-1} > vislimit\f$
	 *   where S is the vessel size, d is its camera distance, and a is the
	 *   camera aperture.
	 * \note If the defined visibility limit exceeds the distance at which the
	 *   vessel can be rendered as a mesh at the given screen resolution,
	 *   it will simply be represented by a circular spot whose size is
	 *   reduced linearly (to reach zero at the limiting distance).
	 * \note If the vessel is to be visible beyond its geometric size (e.g. due
	 *   to light beacons etc.) then the spotlimit value can be used to
	 *   define the limiting distance due to the vessel's geometry, while
	 *   vislimit defines the total visibility range including all
	 *   enhancing factors such as beacons.
	 * \note spotlimit <= vislimit is required. If spotlimit < 0 (default),
	 *   then spotlimit = vislimit is assumed.
	 * \note If SetVisibilityLimit is not called, then the default value is
	 *   vislimit = spotlimit = 1e-3.
	 * \sa SetSize, SetClipRadius
	 */
	void SetVisibilityLimit (double vislimit, double spotlimit = -1) const;

	/**
	 * \brief Returns the radius of the vessel's circumscribing sphere.
	 * \return Radius of the circumscribing sphere of the vessel's visual
	 * representation [m].
	 * \note This parameter describes the radius of the sphere around the
	 *   vessel that is protected from clipping at the observer camera's
	 *   near clipping plane. (The near clipping plane defines an area
	 *   around the view camera within which no objects are rendered. The
	 *   distance of the near clipping plane cannot be made arbitrarily
	 *   small for technical reasons.)
	 * \note By default, the clip radius is identical to the vessel's "Size"
	 *   parameter. However, the size parameter is correlated to physical
	 *   vessel properties and may therefore be smaller than the sphere
	 *   that contains the vessel's complete visual representation. In that
	 *   case, defining a clip radius that is larger than the size
	 *   parameter can avoid visual artefacts.
	 * \note The view camera's near clip plane distance is adjusted so that it
	 *   does not intersect any nearby vessel's clip radius. However, there
	 *   is a minimum near clip distance of 2.5m. This means that if the
	 *   camera approaches a vessel to less than clip radius + 2.5,
	 *   clipping may still occur.
	 * \note Visual cockpit meshes are rendered in a separate pass and are not
	 *   affected by the general near clip distance (they have a separate
	 *   near clip distance of 10cm).
	 * \sa SetClipRadius, GetSize
	 */
	double GetClipRadius () const;

	/**
	 * \brief Set the average colour distribution reflected by
	 *   the vessel.
	 * \param albedo vessel colour vector (red, green blue), range [0..1]
	 *   for each component.
	 * \note The colour passed to this function is currently used to define
	 *   the "spot" colour with which the vessel is rendered at long distances.
	 *   It should represent an average colour and brightness of the vessel
	 *   surface when fully lit.
	 * \note The values for each of the RGB components should be in the range 0-1.
	 * \note The default vessel albedo is bright white (1,1,1).
	 * \note The albedo can be overridden by the AlbedoRGB entry in the
	 *   vessel's config file.
	 */
	void SetAlbedoRGB (const VECTOR3 &albedo) const;

	/**
	 * \brief Set the radius of the vessel's circumscribing sphere.
	 * \param rad Radius of the circumscribing sphere of the vessel's
	 * visual representation [m].
	 * \note
	 * \note This parameter describes the radius of the sphere around the
	 *   vessel that is protected from clipping at the observer camera's
	 *   near clipping plane. (The near clipping plane defines an area
	 *   around the view camera within which no objects are rendered. The
	 *   distance of the near clipping plane cannot be made arbitrarily
	 *   small for technical reasons.)
	 * \note By default, the clip radius is identical to the vessel's "Size"
	 *   parameter. However, the size parameter is correlated to physical
	 *   vessel properties and may therefore be smaller than the sphere
	 *   that contains the vessel's complete visual representation. In that
	 *   case, defining a clip radius that is larger than the size
	 *   parameter can avoid visual artefacts.
	 * \note The view camera's near clip plane distance is adjusted so that it
	 *   does not intersect any nearby vessel's clip radius. However, there
	 *   is a minimum near clip distance of 2.5m. This means that if the
	 *   camera approaches a vessel to less than clip radius + 2.5,
	 *   clipping may still occur.
	 * \note Visual cockpit meshes are rendered in a separate pass and are not
	 *   affected by the general near clip distance (they have a separate
	 *   near clip distance of 10cm).
	 * \note Setting rad = 0 reverts to the default behaviour of using the
	 *   vessel's "Size" parameter to determine the clip radius.
	 * \sa GetClipRadius, SetSize
	 */
	void SetClipRadius (double rad) const;

	/**
	 * \brief Returns the vessel's empty mass (excluding propellants).
	 * \return Vessel empty mass [kg].
	 * \note The empty mass combines all parts of the vessel except propellant
	 *   resources defined via CreatePropellantResource.
	 * \note The empty mass may change during the simulation, often
	 *   discontinuously, for example as a result of stage separation.
	 * \sa oapiGetEmptyMass, GetMassDistribution, SetEmptyMass,
	 *   CreatePropellantResource
	 */
	double GetEmptyMass () const;

	/**
	 * \brief Set the vessel's empty mass (excluding propellants).
	 * \param m vessel empty mass [kg].
	 * \note The empty mass combines all parts of the vessel except propellant
	 *   resources defined via CreatePropellantResource.
	 * \note Use SetEmptyMass to account for structural changes such as stage
	 *   or booster separation, but not for fuel consumption, which is done
	 *   directly by Orbiter.
	 * \sa GetEmptyMass, SetMassDistribution, oapiSetEmptyMass,
	 *   CreatePropellantResource 
	 */
	void SetEmptyMass (double m) const;

	/**
	 * \brief Elevation of the vessel's centre of gravity (COG) above ground.
	 * \return Distance of COG from vessel ground contact plane [m].
	 * \note The COG elevation is defined as the normal distance of the vessel's
	 *   centre of gravity from the ground contact plane defined by its
	 *   three touchdown points.
	 * \note By definition, the vessel's centre of gravity coincides with the
	 *   origin of the local vessel frame. 
	 * \sa GetTouchdownPoints, SetTouchdownPoints
	 */
	double GetCOG_elev () const;

	/**
	 * \brief Returns one of the touchdown vertex definitions for the vessel.
	 * \param [out] tdvtx Reference of touchdown descriptor to be filled.
	 * \param [in] idx Vertex index (>= 0)
	 * \return True on success (index in valid range) false otherwise.
	 * \sa GetTouchdownPointCount, SetTouchdownPoints(const TOUCHDOWNVTX*,DWORD)const
	 */
	bool GetTouchdownPoint (TOUCHDOWNVTX &tdvtx, DWORD idx) const;

	/**
	 * \brief Defines an arbitrary number of vessel surface contact points
	 * \param tdvtx List of touchdown vertex points
	 * \param ntdvtx length of touchdown vertex list
	 * \note The touchdown points should define the vessel's convex hull that
	 *   governs the interaction with planetary surfaces on impact.
	 * \note ntdvtx >= 3 is required
	 * \note In addition to the vertex positions, stiffness and damping
	 *   parameters can be provided to define the compressibility of individual
	 *   points, e.g. for simulating gear suspension.
	 * \sa GetTouchdownPointCount, GetTouchdownPoint
	 */
	void SetTouchdownPoints (const TOUCHDOWNVTX *tdvtx, DWORD ntdvtx) const;

	/**
	 * \brief Returns the number of touchdown points defining the impact hull of
	 *   the vessel;
	 * \return Number of touchdown points
	 */
	DWORD GetTouchdownPointCount () const;

	/**
	 * \brief Set friction coefficients for ground contact.
	 * \param mu_lng friction coefficient in longitudinal direction.
	 * \param mu_lat friction coefficient in lateral direction.
	 * \note The coefficients of surface friction define the deceleration
	 *   forces during sliding or rolling over a surface. mu_lng is the
	 *   coefficient acting in longitudinal (forward) direction, mu_lat the
	 *   coefficient acting in lateral (sideways) direction. The friction
	 *   forces are proportional to the coefficient and the weight of the
	 *   vessel:
	 *   <center><b>F</b><sub>friction</sub> = mu <b>G</b></center>
	 * \note The higher the coefficient, the faster the vessel will come to a
	 *   halt.
	 * \note Typical parameters for a spacecraft equipped with landing wheels
	 *   would be mu_lng = 0.1 and mu_lat = 0.5. If the vessel hasn't got
	 *   wheels, mu_lng = 0.5.
	 * \note The coefficients should be adjusted for belly landings when the
	 *   landing gear is retracted.
	 * \note The longitudinal and lateral directions are defined by the
	 *   touchdown points:
	 *   <center><b>s</b><sub>lng</sub> = <b>p</b><sub>0</sub> - (<b>p</b><sub>1</sub>
	 *   + <b>p</b><sub>2</sub>)/2, &nbsp;
	 *   <b>s</b><sub>lat</sub> = <b>p</b><sub>2</sub> - <b>p</b><sub>1</sub></center>
	 * \sa SetTouchdownPoints
	 */
	void SetSurfaceFrictionCoeff (double mu_lng, double mu_lat) const;

	/**
	 * \brief Returns the vessel's cross sections projected in the
	 *   direction of the vessel's principal axes.
	 * \param cs vector receiving the cross sections of the vessel's
	 *   projection into the yz, xz and xy planes, respectively
	 *   [<b>m<sup>2</sup></b>]
	 * \sa SetCrossSections
	 */
	void GetCrossSections (VECTOR3 &cs) const;

	/**
	 * \brief Defines the vessel's cross-sectional areas, projected in the
	 *   directions of the vessel's principal axes.
	 * \param cs vector of cross-sectional areas of the vessel's projection
	 *   along the x-axis into yz-plane, along the y-axis into the xz-plane,
	 *   and along the z-axis into the xy plane, respectively
	 *   [<b>m<sup>2</sup></b>].
	 * \sa GetCrossSections
	 */
	void SetCrossSections (const VECTOR3 &cs) const;

	/**
	 * \brief Returns the vessel's mass-normalised principal moments of
	 *   inertia (PMI)
	 * \param pmi Diagonal elements of the vessel's inertia tensor
	 *   [<b>m<sup>2</sup></b>]
	 * \note The inertia tensor describes the behaviour of a rigid body
	 *   under angular acceleration. It is the analog of the body's mass in
	 *   the linear case.
	 * \note The values returned by this function are the diagonal elements
	 *   of the inertia tensor, in the local vessel frame of reference.
	 * \note Orbiter's definition of PMI is mass-normalised, that is, the
	 *   values are divided by the total vessel mass. The elements of pmi
	 *   have the following meaning:   
	 *   \f{eqnarray*}
	 *   \mathrm{pmi}_1 &=& M^{-1} \int \rho(\vec{r})(\vec{r}_y^2 + \vec{r}_z^2) d\vec{r}\\
	 *   \mathrm{pmi}_2 &=& M^{-1} \int \rho(\vec{r})(\vec{r}_z^2 + \vec{r}_x^2) d\vec{r}\\
	 *   \mathrm{pmi}_3 &=& M^{-1} \int \rho(\vec{r})(\vec{r}_x^2 + \vec{r}_y^2) d\vec{r}
	 *   \f}
	 * \note Orbiter assumes that off-diagonal elements can be neglected,
	 *   that is, that the diagonal elements are the principal moments of
	 *   inertia. This is usually a good approximation when the vessel is
	 *   sufficiently symmetric with respect to its coordinate frame.
	 *   Otherwise, a diagonalisation by rotating the local frame may be
	 *   required.
	 * \note The shipedit utility in the SDK package allows to calculate
	 *   the inertia tensor from a mesh, assuming a homogeneous mass
	 *   distribution.
	 * \sa SetPMI
	 */
	void GetPMI (VECTOR3 &pmi) const;

	/**
	 * \brief Set the vessel's mass-normalised principal moments of inertia
	 *   (PMI).
	 * \param pmi pmi Diagonal elements of the vessel's inertia tensor
	 *   [<b>m<sup>2</sup></b>]
	 * \note The inertia tensor describes the behaviour of a rigid body
	 *   under angular acceleration.
	 * \note For more information and a definition of the PMI values, see
	 *   \ref GetPMI.
	 * \sa GetPMI
	 */
	void SetPMI (const VECTOR3 &pmi) const;

	/**
	 * \brief Returns the vessel's damping coefficient for gravity field
	 *   gradient-induced torque.
	 * \return Torque damping coefficient (>= 0)
	 * \note A nonspherical object in an inhomogeneous gravitational field
	 *   experiences a torque. Orbiter calculates this torque with
	 * \f[
	 *   \vec{M}_G = \frac{3\mu m}{R^3} (\vec{R}_0 \times \vec{L}\vec{R}_0)
	 * \f]
	 *   where mu = GM, G is the gravity constant, M is the reference body
	 *   mass, m is the vessel mass, R is the distance of the vessel to the
	 *   reference body centre, R0 is the unit vector towards the reference
	 *   body, and L is the mass-normalised inertia tensor (assumed diagonal).
	 * \note This generates an undamped attitude oscillation in the vessel
	 *   orbiting the reference body.
	 * \note Damping may occur due to tidal deformation of the vessel,
	 *   movement of liquids (fuel) etc. Orbiter allows to introduce a
	 *   damping term of the form
	 * \f[
	 *   \vec{M}_D = -\alpha\omega_G
	 * \f]
	 *   where \f$ \omega_G \f$ is the angular velocity, and
	 *   \f$ \alpha = d m r \f$, with damping coefficient \e d, vessel mass
	 *   \e m and vessel radius \e r.
	 * \note If gravity gradient torque has been disabled in the launchpad
	 *   dialog, this function always returns 0.
	 * \sa SetGravityGradientDamping, GetEmptyMass, GetPMI
	 */
	double GetGravityGradientDamping () const;

	/**
	 * \brief Sets the vessel's damping coefficient for gravity field
	 *   gradient-induced torque.
	 * \param damp Torque damping coefficient.
	 * \return true if damping coefficient was applied, false if gravity
	 *   gradient torque is disabled.
	 * \note For a definition of the torque experienced by the vessel in
	 *   an inhomogeneous gravity field, and the damping term that can be
	 *   applied, see \ref GetGravityGradientDamping.
	 * \note If gravity gradient torque has been disabled in the launchpad
	 *   dialog, this function returns false and has no other effect.
	 * \sa GetGravityGradientDamping, SetEmptyMass, SetPMI
	 */
	bool SetGravityGradientDamping (double damp) const;
	//@}

	/// \name Vessel state
	//@{
	/**
	 * \brief Returns the vessel's current status parameters in a
	 *   VESSELSTATUS structure.
	 * \param status structure receiving the current vessel status.
	 * \note The VESSELSTATUS structure provides only limited information.
	 *   Applications should normally use GetStatusEx to obtain a
	 *   VESSELSTATUSx structure which contains additional parameters.
	 * \sa VESSELSTATUS, GetStatusEx
	 */
	void GetStatus (VESSELSTATUS &status) const;

	/**
	 * \brief Returns the vessel's current status parameters in a
	 *   VESSELSTATUSx structure (version x >= 2).
	 * \param status pointer to a VESSELSTATUSx structure
	 * \note This method can be used with any VESSELSTATUSx interface
	 *   version supported by Orbiter. Currently only VESSELSTATUS2 is
	 *	 supported.
	 * \note The version field of the VESSELSTATUSx structure must be set
	 *   by the caller prior to calling the method, to tell Orbiter which
	 *   interface version is required.
	 * \note In addition, the caller must set the VS_FUELLIST, VS_THRUSTLIST
	 *   and VS_DOCKINFOLIST bits in the flag field, if the corresponding
	 *   lists are required. Otherwise Orbiter will not produce these lists.
	 * \note If VS_FUELLIST is specified and the fuel field is NULL, Orbiter
	 *   will allocate memory for the list. The caller is responsible for
	 *   deleting the list after use. If the fuel field is not NULL, Orbiter
	 *   assumes that a list of sufficient length to store all propellant
	 *   resources has been allocated by the caller.
	 * \note The same applies to the thruster and dockinfo lists.
	 * \sa clbkSetStateEx, DefSetStateEx, VESSELSTATUS2
	 */
	void GetStatusEx (void *status) const;

	/**
	 * \brief Set default vessel status parameters.
	 *
	 * Invokes Orbiter's vessel state initialisation with the standard
	 * status parameters provided via a VESSELSTATUS structure.
	 * \param status structure containing vessel status parameters
	 * \note The VESSELSTATUS structure contains only a limited set of
	 *   parameters. Applications should normally use DefSetStateEx in
	 *   combination with an extended VESSELSTATUSx structure.
	 * \sa VESSELSTATUS, DefSetStateEx, GetStatus
	 */
	void DefSetState (const VESSELSTATUS *status) const;

	/**
	 * \brief Set default vessel status parameters.
	 *
	 * Invokes Orbiter's vessel state initialisation with the standard
	 * status parameters provided in a VESSELSTATUSx structure.
	 * \param status pointer to a VESSELSTATUSx structure (x >= 2).
	 * \note status must point to a VESSELSTATUSx structure. Currently only
	 *   VESSELSTATUS2 is supported, but future Orbiter versions may
	 *   introduce new interfaces.
	 * \note Typically, this function will be called in the body of an
	 *   overloaded VESSEL2::clbkSetStateEx to enable default state
	 *   initialisation.
	 * \sa VESSELSTATUS2, GetStatusEx, VESSEL2::clbkSetStateEx
	 */
	void DefSetStateEx (const void *status) const;

	/**
	 * \brief Returns a bit flag defining the vessel's current flight status.
	 * \return vessel status flags (see notes).
	 * \note The following flags are currently defined:
	 *  - bit 0:
	 *    - 0 = vessel is active (in flight),
	 *    - 1 = vessel is inactive (landed)
	 *  - bit 1:
	 *    - 0 = simple vessel (not docked to anything),
	 *    - 1 = part of superstructure, (docked to another vessel)
	 */
	DWORD GetFlightStatus () const;

	/**
	 * \brief Returns current (total) vessel mass.
	 * \return Current vessel mass [kg].
	 * \note The returned value does not include any docked or attached
	 *   vessels.
	 * \sa SetEmptyMass, GetWeightVector, oapiGetMass
	 */
	double GetMass () const;

	/**
	 * \brief Returns the vessel's current position in the global reference
	 *   frame.
	 * \param pos Vector receiving position [<b>m</b>]
	 * \note The global reference frame is the solar barycentric ecliptic
	 *   system at ecliptic and equinox of J2000.0.
	 * \sa oapiGetGlobalPos, GetGlobalVel, GetRelativePos
	 */
	void GetGlobalPos (VECTOR3 &pos) const;

	/**
	 * \brief Returns the vessel's current velocity in the global reference
	 *   frame.
	 * \param vel Vector receiving velocity [<b>m/s</b>]
	 * \note The global reference frame is the solar barycentric ecliptic
	 *   system at ecliptic and equinox of J2000.0.
	 * \sa oapiGetGlobalVel, GetGlobalPos, GetRelativeVel
	 */
	void GetGlobalVel (VECTOR3 &vel) const;

	/**
	 * \brief Returns the vessel's current position with respect to another
	 *   object.
	 * \param hRef reference object handle
	 * \param pos vector receiving position [<b>m</b>]
	 * \note This function returns the vessel's position relative to the
	 *   position of the object defined by handle hRef.
	 * \note Results are returned in the ecliptic frame (ecliptic and
	 *   equinox of J2000.0).
	 * \sa oapiGetRelativePos, GetRelativeVel, GetGlobalPos
	 */
	void GetRelativePos (OBJHANDLE hRef, VECTOR3 &pos) const;

	/**
	 * \brief Returns the vessel's current velocity relative to another
	 *   object.
	 * \param hRef reference object handle
	 * \param vel vector receiving velocity [<b>m/s</b>]
	 * \note This function returns the vessel's velocity relative to the
	 *   velocity of the object defined by handle hRef.
	 * \note Results are returned in the ecliptic frame (ecliptic and
	 *   equinox of J2000.0).
	 * \sa oapiGetRelativeVel, GetGlobalVel, GetRelativePos
	 */
	void GetRelativeVel (OBJHANDLE hRef, VECTOR3 &vel) const;

	/**
	 * \brief Returns the vessel's current angular velocity components
	 *   around its principal axes.
	 * \param [out] avel vector receiving angular velocity components [<b>rad/s</b>]
	 * \note The returned vector contains the angular velocities
	 *   \f$ \omega_x, \omega_y, \omega_z \f$ around the vessel's x, y and z
	 *   axes, in the rotating vessel frame.
	 * \note Because the change of the angular velocity components is is
	 *   governed by Euler's coupled differential equations of rigid body
	 *   motion, the values can fluctuate between the axes even if no torque
	 *   is acting on the vessel.
	 * \sa SetAngularVel, GetAngularAcc
	 */
	void GetAngularVel (VECTOR3 &avel) const;

	/**
	 * \brief Returns the vessel's current angular acceleration components
	 *   around its principal axes.
	 * \param [out] aacc angular acceleration [<b>rad/s<sup>2</sup></b>]
	 * \note The returned vector contains the angular accelerations
	 *   \f$ \partial\omega_x/\partial t, \partial\omega_y/\partial t, \partial\omega_z/\partial t \f$
	 *   around the vessel's x, y and z axes, in the rotating vessel frame.
	 * \sa GetAngularVel, GetAngularMoment
	 */
	void GetAngularAcc (VECTOR3 &aacc) const;

	/**
	 * \brief Returns the linear force vector currently acting on the vessel.
	 * \param [out] F force vector in vessel coordinates [<b>N</b>]
	 * \note The returned vector is the vector sum of all forces (gravity,
	 *   thrust, aerodynamic forces, etc.) currently acting on the vessel.
	 * \sa GetAngularMoment
	 */
	void GetLinearMoment (VECTOR3 &F) const;

	/**
	 * \brief Returns the sum of angular moments currently acting on the vessel.
	 * \param [out] amom angular moment [<b>Nm</b>]
	 * \note Given all force components <b>F</b><sub>i</sub> acting on the vessel at
	 *   positions <b>r</b><sub>i</sub>, the angular moment is defined as
	 *   \f[ \vec{M} = \sum_i \vec{F}_i \times \vec{r}_i \f]
	 *   (note the left-handed reference frame in the order of operands for the
	 *   cross product).
	 * \sa GetLinearMoment
	 */
	void GetAngularMoment (VECTOR3 &amom) const;

	/**
	 * \brief Applies new angular velocity to the vessel.
	 * \param avel vector containing the new angular velocity components [<b>rad/s</b>]
	 * \note The input vector defines the angular velocities around the
	 *   vessel's x, y and z axes. They refer to the rotating vessel frame.
	 * \sa GetAngularVel
	 */
	void SetAngularVel (const VECTOR3 &avel) const;

	/**
	 * \brief Returns the Euler angles defining the vessel's orientation.
	 * \param arot vector receiving the three Euler angles [<b>rad</b>]
	 * \note The components of the returned vector arot = \f$ (\alpha, \beta, \gamma) \f$
	 *   are the angles of rotation [rad] around the x,y,z axes in the
	 *   global (ecliptic) frame to produce the rotation matrix <b>R</b> for
	 *   mapping from the vessel's local frame of reference to the global
	 *   frame of reference:
	 *   \f[
	 *     \mathsf{R} = \left[ \begin{array}{ccc}
	 *     1 & 0 & 0 \\ 0 & \cos\alpha & \sin\alpha \\ 0 & -\sin\alpha & \cos\alpha
	 *     \end{array} \right]
	 *     \left[ \begin{array}{ccc}
	 *     \cos\beta & 0 & -\sin\beta \\ 0 & 1 & 0 \\ \sin\beta & 0 & \cos\beta
	 *     \end{array} \right]
	 *     \left[ \begin{array}{ccc}
	 *     \cos\gamma & \sin\gamma & 0 \\ -\sin\gamma & \cos\gamma & 0 \\ 0 & 0 & 1
	 *     \end{array} \right]
	 *   \f]
	 * \sa SetGlobalOrientation, GetRotationMatrix
	 */
	void GetGlobalOrientation (VECTOR3 &arot) const;

	/**
	 * \brief Sets the vessel's orientation via Euler angles.
	 * \param arot vector containing the set of Euler angles [<b>rad</b>]
	 * \note Given the rotation matrix <b>R</b> which transforms from the
	 *   local (vessel) frame to the global (ecliptic) reference frame,
	 *   the Euler angles expected by this method are defined as
	 *   \f{eqnarray*}
	 *    \alpha &=& \mathrm{atan2} (R_{23}, R_{33}) \\
	 *    \beta &=& -\mathrm{asin} (R_{13}) \\
	 *    \gamma &=& \mathrm{atan2} (R_{12}, R_{11})
	 *   \f}
	 * \sa GetGlobalOrientation, SetRotationMatrix
	 */
	void SetGlobalOrientation (const VECTOR3 &arot) const;

	/**
	 * \brief Returns a flag indicating contact with a planetary surface.
	 * \return true indicates ground contact (at least one of the vessel's
	 *   touchdown reference points is in contact with a planet surface).
	 * \sa SetTouchdownPoints
	 */
	bool GroundContact () const;

	/**
	 * \brief Flag indicating whether orbit stabilisation is used for the
	 *   vessel at the current time step.
	 * \return true indicates that the vessel's state is currently updated
	 *   by using the stabilisation algorithm, which calculates the
	 *   osculating elements with respect to the primary gravitational
	 *   source, and treats all additional forces as perturbations.
	 * \note A vessel switches to orbit stabilisation only if the user has
	 *   enabled it in the launchpad dialog, and the user-defined
	 *   perturbation and time step limits are currently satisfied.
	 * \note Stabilised mode reduces the effect of deteriorating orbits due
	 *   to accumulating numerical errors in the state vector propagation,
	 *   but is limited in handling multiple gravitational sources.
	 * \sa GetElements
	 */
	bool OrbitStabilised () const;

	/**
	 * \brief Flag for nonspherical gravity perturbations.
	 *
	 * Indicates whether the vessel considers gravity field perturbations
	 * due to nonspherical planet shapes when updating its state vectors for
	 * the current time step.
	 * \return true indicates that gravity perturbations due to nonspherical
	 *   planet shapes are taken into account.
	 * \note This function will always return false if the user has disabled
	 *   the "Nonspherical gravity sources" option in the Launchpad dialog.
	 * \note If the user has enabled orbit stabilisation in the Launchpad,
	 *   this function may sometimes return false during high time
	 *   compression, even if the nonspherical option has been selected. In
	 *   such situations Orbiter can exclude nonspherical perturbations to
	 *   avoid numerical instabilities.
	 * \sa GetWeightVector
	 */
	bool NonsphericalGravityEnabled () const;

	/**
	 * \brief Returns aerodynamic control surfaces currently under manual
	 *   control.
	 * \return Bit flags defining the current address mode for aerodynamic
	 *   control surfaces.
	 * \note The input mode defines which types of control surfaces can be
	 *   manually controlled by the user.
	 * \note The returned control mode contains bit flags as follows:
	 *   - bit 0: elevator enabled/disabled 
	 *   - bit 1 rudder enabled/disabled 
	 *   - bit 2 ailerons enabled/disabled
	 *   .
	 * Therefore, mode=0 indicates control surfaces disabled, mode=7
	 *   indicates fully enabled.
	 * \note Some vessel types may support not all, or not any, types of
	 *   control surfaces.
	 * \sa SetADCtrlMode, CreateControlSurface, CreateControlSurface2,
	 *   GetControlSurfaceLevel, SetControlSurfaceLevel
	 */
	DWORD GetADCtrlMode () const;

	/**
	 * \brief Configure manual input mode for aerodynamic control surfaces.
	 * \param mode bit flags defining the address mode for aerodynamic
	 *   control surfaces (see notes)
	 * \note The mode parameter contains bit flags as follows:
	 *   - bit 0: enable/disable elevator 
	 *   - bit 1: enable/disable rudder 
	 *   - bit 2 enable/disable ailerons
	 *   .
	 * Therefore, use mode = 0 to disable all control surfaces, mode = 7 to
	 *   enable all control surfaces. 
	 * \sa GetADCtrlMode, CreateControlSurface, CreateControlSurface2,
	 *   GetControlSurfaceLevel, SetControlSurfaceLevel
	 */
	void SetADCtrlMode (DWORD mode) const;

	/**
	 * \brief Activates one of the automated orbital navigation modes.
	 * \param mode navigation mode identifier (see \ref navmode)
	 * \return \e true if the specified navigation mode could be activated,
	 *   \e false if not available or active already.
	 * \note Navmodes are high-level navigation modes which involve e.g.
	 *   the simultaneous and timed engagement of multiple attitude thrusters
	 *   to get the vessel into a defined state. Some navmodes terminate
	 *   automatically once the target state is reached (e.g. killrot),
	 *   others remain active until explicitly terminated (hlevel). Navmodes
	 *   may also terminate if a second conflicting navmode is activated.
	 * \sa navmode, DeactivateNavmode, ToggleNavmode, GetNavmodeState
	 */
	bool ActivateNavmode (int mode);

	/**
	 * \brief Deactivates an automated orbital navigation mode.
	 * \param mode navigation mode identifier (see \ref navmode)
	 * \return \e true if the specified navigation mode could be deactivated,
	 *   \e false if not available or inactive already.
	 * \sa navmode, ActivateNavmode, ToggleNavmode, GetNavmodeState
	 */
	bool DeactivateNavmode (int mode);

	/**
	 * \brief Toggles a navigation mode on/off.
	 * \param mode navigation mode identifier (see \ref navmode)
	 * \return \e true if the specified navigation mode could be
	 *   changed, \e false if it remains unchanged.
	 * \sa navmode, ActivateNavmode, DeactivateNavmode, GetNavmodeState
	 */
	bool ToggleNavmode (int mode);

	/**
	 * \brief Returns the current active/inactive state of a navigation mode.
	 * \param mode navigation mode identifier (see \ref navmode)
	 * \return \e true if the specified navigation mode is active, \e false
	 *   otherwise.
	 * \sa navmode, ActivateNavmode, DeactivateNavmode, ToggleNavmode
	 */
	bool GetNavmodeState (int mode);

	/**
	 * \brief Returns the altitude that the holver hold altitude program tries to
	 *   maintain.
	 * \param [out] alt target altitude [m]
	 * \param [out] terrainalt indicates true altitude (terrainalt==true) or altitude
	 *   relative to mean planet radius (terrainalt==false)
	 * \return true: hold altitude program is active; false: hold altitude program
	 *   is not active
	 * \note If the function returns false, the values pointed to by alt and
	 *   terrainalt are unchanged.
	 * \sa ActivateNavmode, DeactivateNavmode, ToggleNavmode, GetNavmodeState
	 */
    bool GetHoverHoldAltitude (double &alt, bool &terrainalt);

	/**
	 * \brief Set the target altitude for the hover hold altitude program and
	 *   activate the program.
	 * \param alt target altiude [m]
	 * \param terrainalt true: hold true altitude; false: hold altitude relative to
	 *   mean planet radius
	 * \note If the hold hover altiude program is already active, the target
	 *   altitude is modified. Otherwise, the program is activated with the
	 *   specified target altitude.
	 * \note This method is more versatile than ActivateNavmode(NAVMODE_HOLDALT), which
	 *   sets the target altitude to the current altitude at activation, and always
	 *   refers to mean planet radius.
	 * \note To deactivate the hover hold alt program, use
	 *   DeactivateNavmode(NAVMODE_HOLDALT)
	 * \sa GetHoverHoldAltitude, ActivateNavmode, DeactivateNavmode
	 */
	void SetHoverHoldAltitude (double alt, bool terrainalt);
	//@}


	/**
	 * \name Orbital elements
	 * See also: \ref orbit
	 */
	//@{
	/**
	 * \brief Returns a handle to the main contributor of the gravity field
	 *   at the vessel's current position.
	 * \return Handle for gravity reference object.
	 * \note All parameters calculated by functions in this section refer
	 *   to the gravity reference object, unless explicitly stated otherwise.
	 */
	const OBJHANDLE GetGravityRef () const;

	/**
	 * \brief Returns osculating orbital elements.
	 *
	 * Calculates the set of osculating elements at the current time with
	 * respect to the dominant gravitational source.
	 * \param[out] el current osculating elements relative to dominant gravity
	 *   source, in ecliptic frame of reference.
	 * \param[out] mjd_ref reference date (in Modified Julian Date format) to
	 *   which the returned el.L (mean longitude) value refers.
	 * \return Handle of reference object. NULL indicates failure (no
	 *   elements available).
	 * \note This method will return the mean longitude at a fixed reference
	 *   date, so the value will not change over time, unless the orbit
	 *   itself changes.
	 * \note For extended functionality, see version 2 of GetElements.
	 * \sa \subpage orbit, ELEMENTS, GetElements(OBJHANDLE,ELEMENTS&,ORBITPARAM*,double,int)const,
	 *   SetElements
	 */
	OBJHANDLE GetElements (ELEMENTS &el, double &mjd_ref) const;

	/**
	 * \brief Returns osculating elements and additional orbit parameters.
	 *
	 * Returns the current osculating elements for the vessel. This version
	 * has an extended functionality: it allows to specify an arbitrary
	 * celestial body as reference object, an arbitrary reference time, and
	 * can return elements either in the ecliptic or equatorial frame of
	 * reference.
	 * \param hRef reference body handle
	 * \param el current osculating elements relative to hRef
	 * \param prm additional orbital parameters
	 * \param mjd_ref reference data (in Modified Julian Date format) to
	 *   which the el.L (mean longitude) value refers.
	 * \param frame orientation of reference frame (see notes)
	 * \return Currently always true.
	 * \note For an overview of orbital parameters, see \ref orbit.
	 * \note This version returns the elements with respect to an arbitrary
	 *   celestial body, even if that body is not the main source of the
	 *   gravity field acting on the vessel. If hRef==NULL, the default
	 *   reference body is used.
	 * \note If the prm pointer is not set to NULL, the ORBITPARAM structure
	 *   it points to will be filled with additional orbital parameters
	 *   derived from the primary elements.
	 * \note All parameters returned in the prm structure refer to the
	 *   current date, rather than the reference date mjd_ref. Therefore,
	 *   the values of el.L and prm->MnL can be different.
	 * \note Unlike GetElements(ELEMENTS&,double&), mjd_ref is a
	 *   user-provided input parameter which specifies to which date the
	 *   returned el.L (mean longitude) value will refer. An exception is
	 *   mjd_ref = 0, which is interpreted as the current time (equivalent
	 *   to mjd_ref = oapiGetSimMJD() ).
	 * \note The frame parameter can be set to one of the following:
	 *   - FRAME_ECL: returned elements are expressed in the ecliptic frame
	 *     (epoch J2000).
	 *   - FRAME_EQU: returned elements are expressed in the equatorial
	 *     frame of the reference object (hRef).
	 * \sa \subpage orbit, ELEMENTS, ORBITPARAM, GetElements(ELEMENTS&,double&)const,
	 *   SetElements
	 */
	bool GetElements (OBJHANDLE hRef, ELEMENTS &el, ORBITPARAM *prm = 0, double mjd_ref = 0, int frame = FRAME_ECL) const;

	/**
	 * \brief Set vessel state (position and velocity) by means of a set of
	 *   osculating orbital elements.
	 * \param hRef reference body handle
	 * \param el set of elements to be applied
	 * \param prm secondary orbital parameters
	 * \param mjd_ref reference date (in Modified Julian Date format) to
	 *   which the el.L (mean longitude) value refers
	 * \param frame orientation of reference frame (see notes)
	 * \return If the vessel position resulting from applying the elements
	 *   would be located below the surface of the reference body, the
	 *   method does nothing and returns false. Otherwise it returns true.
	 * \note This method resets the vessel's position and velocity
	 *   according to the specified orbital elements.
	 * \note If the prm pointer is not set to NULL, the ORBITPARAM structure
	 *   it points to will be filled with secondary orbital parameters
	 *   derived from the primary elements el. Note that this is an output
	 *   parameter, i.e. the resulting vessel state will not be influenced
	 *   by initialising this structure prior to the function call.
	 * \note All parameters returned in the prm structure refer to the
	 *   current date, rather than the reference date mjd_ref. Therefore,
	 *   the values of el.L and prm->MnL can be different.
	 * \note The elements can be supplied either in terms of the ecliptic
	 *   frame (frame = FRAME_ECL) or in the equatorial frame of the
	 *   reference body (frame = FRAME_EQU).
	 * \note mjd_ref is an input parameter which defines the date to which
	 *   the el.L (mean longitude) value refers. An exception is mjd_ref = 0,
	 *   which is interpreted as the current time (equivalent to
	 *   mjd_ref = oapiGetSimMJD() ).
	 * \note Calling SetElements will always put a vessel in freeflight
	 *   mode, even if it had been landed before.
	 * \note Currently, SetElements doesn't check for validity of the
	 *   provided elements. Setting invalid elements, or elements which put
	 *   the vessel below a planetary surface will produce undefined
	 *   results.
	 * \sa \subpage orbit, ELEMENTS, ORBITPARAM, GetElements(ELEMENTS&,double&)const,
	 *   GetElements(OBJHANDLE,ELEMENTS&,ORBITPARAM*,double,int)const
	 */
	bool SetElements (OBJHANDLE hRef, const ELEMENTS &el, ORBITPARAM *prm = 0, double mjd_ref = 0, int frame = FRAME_ECL) const;

	/**
	 * \brief Returns the magnitude of the semi-minor axis of the current
	 *   osculating orbit.
	 * \param [out] smi semi-minor axis [m]
	 * \return Handle of reference object, relative to which the orbit is
	 *   calculated. NULL indicates failure (no orbit information available)
	 * \note The semi-minor axis is the smallest semi-diameter of the
	 *   orbit ellipse (see \ref orbit).
	 * \sa \subpage orbit, ELEMENTS, ORBITPARAM, GetElements
	 */
	OBJHANDLE GetSMi (double &smi) const;

	/**
	 * \brief Returns argument of periapsis of the current osculating orbit.
	 * \param [out] arg argument of periapsis for current orbit [rad]
	 * \return Handle of reference body, relative to which the orbit is
	 *   calculated. NULL indicates failure (no orbit information available)
	 * \note The argument of periapsis is the angle between periapsis and the
	 *   ascending node (see \ref orbit_2).
	 * \sa \subpage orbit, ELEMENTS, ORBITPARAM, GetPeDist, GetApDist, GetElements
	 */
	OBJHANDLE GetArgPer (double &arg) const;

	/**
	 * \brief Returns the periapsis distance of the current osculating orbit.
	 * \param [out] pedist periapsis distance [m]
	 * \return Handle of reference body, relative to which the orbit is
	 *   calculated. NULL indicates failure (no orbit information available)
	 * \note The periapsis distance is the smallest radius of the orbit (see
	 *   \ref orbit).
	 * \sa \subpage orbit, ELEMENTS, ORBITPARAM, GetApDist, GetArgPer, GetElements
	 */
	OBJHANDLE GetPeDist (double &pedist) const;

	/**
	 * \brief Returns the apoapsis distance of the current osculating orbit.
	 * \param [out] apdist apoapsis distance [m]
	 * \return Handle of reference body, relative to which the orbit is
	 *   calculated. NULL indicates failure (no orbit information available)
	 * \note the apoapsis distance is the largest radius of the orbit (see
	 *   \ref orbit).
	 * \sa \subpage orbit, ELEMENTS, ORBITPARAM, GetPeDist, GetArgPer, GetElements
	 */
	OBJHANDLE GetApDist (double &apdist) const;
	//@}


	/// \name Surface-relative parameters
	//@{
	/**
	 * \brief Returns a handle to the surface reference object (planet or moon).
	 * \return Surface reference object handle
	 * \note The surface reference is the planet or moon whose surface is
	 *   closest to the current vessel position. All methods in this group
	 *   refer to this celestial body.
	 */
	const OBJHANDLE GetSurfaceRef () const;

	/**
	 * \brief Returns the altitude above the mean radius of the current surface
	 *   reference body.
	 * \return Altitude above mean radius [m]
	 * \note For altitude above ground, use \ref GetAltitude(AltitudeMode,int*)
	 * \note Currently all celestial bodies are assumed to be spheres. This
	 *   method therefore returns the distance to the centre of the reference
	 *   body minus the reference body radius.
	 * \note The reference body is the planet or moon whose surface is closest
	 *   to the current vessel position (i.e. the body with minimal altitude).
	 * \sa GetAltitude(AltitudeMode,int*), GetSurfaceRef
	 */
	double GetAltitude () const;

	/**
	 * \brief Returns the vessel altitude.
	 * \param [in] mode altitude mode (altitude over ground/over mean radius)
	 * \param [out] reslvl pointer to variable receiving the resolution level
	 *   at which ground altitude was calculated.
	 * \return Altitude [m]
	 * \note For mode==ALTMODE_MEANRAD, this method is equivalent to \ref GetAltitude().
	 * \note For mode==ALTMODE_GROUND, if reslvl is set, on return the int variable
	 *   it points to will be filled with the planet surface resolution level at
	 *   which the altitude was calculated. At higher altitudes, Orbiter may use a
	 *   lower resolution setting.
	 * \note For mode==ALTMODE_MEANRAD, the resolution level has no meaning, and
	 *  always returns 0.
	 * \sa GetAltitude()
	 */
	double GetAltitude (AltitudeMode mode, int *reslvl=0);

	/**
	 * \brief Returns the current pitch angle with respect to the local horizon.
	 * \return pitch angle [rad]
	 * \note The pitch angle \e p is defined as
	 *   \f[  p = \frac{\pi}{2} - q \f]
	 *   where \e q is the angle between the vessel's positive z axis (forward
	 *   direction) and the normal of the local horizon.
	 * \sa GetSurfaceRef, GetBank, GetYaw
	 */
	double GetPitch () const;

	/**
	 * \brief Returns the current bank (roll) angle with respect to the local
	 *   horizon.
	 * \return bank angle [rad]
	 * \note The bank angle \e b is defined as the angle between the vessel's
	 *   positive y axis (up direction) and the projection of the normal of the
	 *   local horizon into the x-y plane.
	 * \sa GetSurfaceRef, GetPitch, GetYaw
	 */
	double GetBank () const;

	/**
	 * \brief Returns the current yaw angle with respect to the local horizon.
	 * \return yaw angle [rad]
	 * \note The yaw angle \e y is defined as the angle between the the projection
	 *   of the vessel's positive z axis (forward direction) into the horizon
	 *   plane, and the local horizon "north" direction.
	 * \sa GetSurfaceRef, GetPitch, GetBank
	 */
	double GetYaw () const;

	/**
	 * \brief Returns the elevation of the surface at the vessel's current
	 *   longitude/latitude above the reference radius.
	 * \return surface elevation [m]
	 * 
	 */
	double GetSurfaceElevation () const;

	/**
	 * \brief Returns the normal (in local horizon frame) of the surface below the
	 *   vessel's current position
	 * \return surface normal in local horizon frame
	 */
	VECTOR3 GetSurfaceNormal () const;

	/**
	 * \brief Returns vessel's current equatorial position with respect to the
	 *   closest planet or moon.
	 * \param [out] longitude longitude coordinate [rad]
	 * \param [out] latitude latitude coordinate [rad]
	 * \param [out] radius distance from planet centre [m]
	 * \return Handle to reference body to which the parameters refer.
	 *   NULL indicates failure (no reference body available).
	 * \sa GetSurfaceRef
	 */
	OBJHANDLE GetEquPos (double &longitude, double &latitude, double &radius) const;
	//@}


	/// \name Atmospheric parameters
	//@{
	/**
	 * \brief Returns a handle to the reference body for atmospheric calculations.
	 * \return Handle for the celestial body whose atmosphere the vessel is
	 *   currently moving through, or NULL if the vessel is not inside an
	 *   atmosphere.
	 * \sa GetAtmTemperature, GetAtmDensity, GetAtmPressure
	 */
	const OBJHANDLE GetAtmRef () const;

	/**
	 * \brief Returns ambient atmospheric temperature at current vessel position.
	 * \return Ambient temperature [K] at current vessel position.
	 * \note This function returns 0 if the vessel is outside all planetary
	 *   atmospheric hulls, as defined by the planets' AtmAltLimit parameters.
	 * \sa GetAtmDensity, GetAtmPressure, GetAtmRef
	 */
	double GetAtmTemperature () const;

	/**
	 * \brief Returns atmospheric density at current vessel position.
	 * \return Atmospheric density [kg/m<sup>3</sup>] at current vessel position.
	 * \note This function returns 0 if the vessel is outside all planetary
	 *   atmospheric hulls, as defined by the planets' AtmAltLimit parameters.
	 * \sa GetAtmPressure, GetAtmTemperature, GetAtmRef
	 */
	double GetAtmDensity () const;

	/**
	 * \brief Returns static atmospheric pressure at current vessel position.
	 * \return Static atmospheric pressure [Pa] at current vessel position.
	 * \note This function returns 0 if the vessel is outside all planetary
	 *   atmospheric hulls, as defined by the planets' AtmAltLimit parameters.
	 * \sa GetDynPressure, GetAtmDensity, GetAtmTemperature, GetAtmRef
	 */
	double GetAtmPressure () const;
	//@}


	/// \name Aerodynamic state parameters
	//@{
	/**
	 * \brief Returns the current dynamic pressure for the vessel.
	 * \return Current vessel dynamic pressure [Pa].
	 * \note The dynamic pressure is defined as
	 *   \f[ q = \frac{1}{2} \rho V^2 \f]
	 *   with density \f$\rho\f$ and airflow velocity \e V. Dynamic pressure is an
	 *   important aerodynamic parameter.
	 * \sa GetAtmPressure, GetAtmRef
	 */
	double GetDynPressure () const;

	/**
	 * \brief Returns the vessel's current Mach number.
	 * \return Mach number - the ratio of current freestream airflow velocity over
	 *   speed of sound.
	 * \note The speed of sound depends on several parameters, e.g. atmospheric
	 *   composition and temperature. The Mach number can therefore vary even if
	 *   the airspeed is constant.
	 * \sa GetAirspeed, GetAtmRef
	 */
	double GetMachNumber () const;

	/**
	 * \brief Returns magnitude of the ground speed vector.
	 * \return Magnitude of ground speed velocity vector [m/s]
	 * \note The ground speed vector is defined as the ship's velocity vector
	 *   in the rotating frame of the celestial reference body.
	 * \note This function returns the length of the vector returned by
	 *   GetGroundspeedVector.
	 * \sa GetGroundspeedVector, GetAirspeed, GetAirspeedVector, GetMachNumber, GetAtmRef
	 */
	double GetGroundspeed () const;

	/**
	 * \brief Returns the vessel's ground speed vector.
	 * \param [in] frame frame of reference for returned vector.
	 * \param [out] v ground speed vector on exit [<b>m/s</b>]
	 * \return Status flag (\e false indicates error). Error conditions: invalid
	 *   \e frame parameter, or ground speed data could not be obtained.
	 * \note This method returns the ground speed vector in the requested frame
	 *   of reference. The ground speed vector is defined as the vessel's
	 *   velocity vector measured in the rotating frame of the celestial
	 *   reference body.
	 * \note Valid entries for \a frame are
	 *  - FRAME_GLOBAL: Returns velocity vector in the global frame of reference
	 *  - FRAME_LOCAL: Returns velocity vector in the vessel's local frame of
	 *    reference
	 *  - FRAME_REFLOCAL: Returns velocity vector in the celestial reference
	 *    body's local frame of reference
	 *  - FRAME_HORIZON: Returns velocity vector in the local horizon frame
	 *    (x = longitudinal component, y = vertical component, z = latitudinal
	 *    component)
	 * \sa GetGroundspeed, GetAirspeed, GetAirspeedVector, GetAtmRef
	 */
	bool GetGroundspeedVector (REFFRAME frame, VECTOR3 &v) const;

	/**
	 * \brief Returns magnitude of the true airspeed vector.
	 * \return Magnitude of airspeed velocity vector [m/s]
     * \note The airspeed vector is defined as the ship's velocity vector relative
	 *   to the velocity vector of the surrounding freestream airflow.
	 * \note This function returns the length of the vector returned by
	 *   GetAirspeedVector().
	 * \sa GetAirspeedVector, GetGroundspeed, GetGroundspeedVector, GetMachNumber, GetAtmRef
	 */
	double GetAirspeed () const;

	/**
	 * \brief Returns the vessel's true airspeed vector.
	 * \param [in] frame frame of reference for returned vector.
	 * \param [out] v airspeed vector on exit [<b>m/s</b>]
	 * \return Status flag (\e false indicates error). Error conditions: invalid
	 *   \e frame parameter, or ground speed data could not be obtained.
	 * \note This method returns the true airspeed vector in the requested frame
	 *   of reference. The ground airvector is defined as the vessel's
	 *   velocity vector with respect to the surrounding freestream air flow.
	 * \note If the vessel is not within an a planetary atmosphere, the returned
	 *   vector is equal to the groundspeed vector.
	 * \note Valid entries for \a frame are
	 *  - FRAME_GLOBAL: Returns velocity vector in the global frame of reference
	 *  - FRAME_LOCAL: Returns velocity vector in the vessel's local frame of
	 *    reference
	 *  - FRAME_REFLOCAL: Returns velocity vector in the celestial reference
	 *    body's local frame of reference
	 *  - FRAME_HORIZON: Returns velocity vector in the local horizon frame
	 *    (x = longitudinal component, y = vertical component, z = latitudinal
	 *    component)
	 * \sa GetAirspeed, GetGroundspeed, GetGroundspeedVector, GetAtmRef
	 */
	bool GetAirspeedVector (REFFRAME frame, VECTOR3 &v) const;

	/**
	 * \brief Returns the airspeed vector in local horizon coordinates.
	 * \deprecated This method has been replaced by \ref VESSEL::GetAirspeedVector
	 */
	bool GetHorizonAirspeedVector (VECTOR3 &v) const;

	/**
	 * \brief Returns the airspeed vector in vessel coordinates.
	 * \deprecated This method has been replaced by \ref VESSEL::GetAirspeedVector
	 */
	bool GetShipAirspeedVector (VECTOR3 &v) const;

	/**
	 * \brief Returns the current angle of attack.
	 * \return AOA (angle of attack) value [rad] in the range -Pi ... +Pi.
	 * \note The AOA value is defined as the angle between the vessel's positive
	 *   z axis and the flight path direction, projected into the yz-plane of the
	 *   vessel's local coordinate system.
	 * \sa GetSlipAngle
	 */
	double GetAOA () const;

	/**
	 * \brief Returns the lateral (yaw) angle between the velocity vector and the
	 *   vessel's longitudinal axis.
	 * \return slip angle [rad] in the range -Pi ... +Pi.
	 * \note The slip angle is defined as the angle between the vessel's positive
	 *   z axis and the flight path direction, projected into the xz-plane of the
	 *   vessel's local coordinate system.
	 * \sa GetAOA
	 */
	double GetSlipAngle () const;
	//@}


	/// \name Airfoils and control surfaces
	//@{
	/**
	 * \brief Creates a new airfoil and defines its aerodynamic properties.
	 * \param align orientation of the lift vector (LIFT_VERTICAL or LIFT_HORIZONTAL)
	 * \param ref centre of pressure in vessel coordinates [<b>m</b>]
	 * \param cf pointer to coefficient callback function (see notes)
	 * \param c airfoil chord length [m]
	 * \param S wing area [m<sup>2</sup>]
	 * \param A wing aspect ratio
	 * \note A vessel can define multiple airfoils (for wings, main body,
	 *   tail stabilisators, etc.). In general, it should define at least
	 *   one vertical and one horizontal component.
	 * \note Airfoil definitions for wings and horizontal stabilisers set
	 *   \a align to LIFT_VERTICAL. Vertical stabilisers (vertical tail
	 *   fin, etc.) set \a align to LIFT_HORIZONTAL.
	 * \note The centre of pressure is the point at which the lift and
	 *   drag forces generated by the airfoil are applied to the vessel.
	 *   Together with the moment coefficient it defines the aerodynamic
	 *   stability of the vessel. Usually the CoP will be aft of the CG,
	 *   and the moment coefficient will have a negative slope around the
	 *   trim angle of attack.
	 * \note The \a AirfoilCoeffFunc is a callback function which must be
	 *   supplied by the module. It calculates the lift, moment and drag
	 *   coefficients for the airfoil. It has the following interface:
	 *   \code
	 *   void AirfoilCoeffFunc (double aoa, double M, double Re,
	 *        double *cl, double *cm, double *cd)
	 *   \endcode
	 *   and returns the lift coefficient (\e cl), moment coefficient (\e
	 *   cm) and drag coefficient (\e cd) as a function of angle of attack
	 *   \e aoa [rad], Mach number \e M and Reynolds number \e Re. Note
	 *   that aoa can range over the full circle (-pi to pi). For vertical
	 *   lift components, aoa is the pitch angle of attack (a), while for
	 *   horizontal components it is the yaw angle of attack (b).
	 * \note If the wing area S is set to 0, then Orbiter uses the
	 *   projected vessel cross sections to define a reference area. Let
	 *   (v<sub>x</sub>, v<sub>y</sub>, v<sub>z</sub>) be the unit vector
	 *   of freestream air flow in vessel coordinates. Then the reference
	 *   area is calculated as S = v<sub>z</sub>C<sub>z</sub> + v<sub>y</sub>C<sub>y</sub>
	 *   for a LIFT_VERTICAL airfoil, and as S = v<sub>z</sub>C<sub>z</sub> + v<sub>x</sub>C<sub>x</sub>
	 *   for a LIFT_HORIZONTAL airfoil, where C<sub>x</sub>, C<sub>y</sub>, C<sub>z</sub>
	 *   are the vessel cross-sections in x, y and z direction, respectively.
	 * \note The wing aspect ratio is defined as defined as A = b<sup>2</sup>/S
	 *   with wing span b.
	 * \note A vessel should typically define its airfoils in the
	 *   \ref VESSEL2::clbkSetClassCaps callback function. If no airfoils
	 *   are defined, Orbiter will fall back to its legacy drag
	 *   calculation, using the cw coefficients defined in \ref SetCW.
	 *   Legacy lift calculation is no longer supported.
	 * \note For more details, see the Programmer's Guide.
     * \sa CreateAirfoil2, CreateAirfoil3, EditAirfoil, DelAirfoil
	 */
	void CreateAirfoil (AIRFOIL_ORIENTATION align, const VECTOR3 &ref, AirfoilCoeffFunc cf, double c, double S, double A) const;

	/**
	 *  \brief Creates a new airfoil and defines its aerodynamic properties.
	 * \param align orientation of the lift vector (LIFT_VERTICAL or LIFT_HORIZONTAL)
	 * \param ref centre of pressure in vessel coordinates [<b>m</b>]
	 * \param cf pointer to coefficient callback function (see notes)
	 * \param c airfoil chord length [m]
	 * \param S wing area [m<sup>2</sup>]
	 * \param A wing aspect ratio
	 * \return Handle for the new airfoil.
	 * \note This method is identical to \ref CreateAirfoil, but returns a
	 *   handle which can be used to identify the airfoil later.
	 * \sa CreateAirfoil, CreateAirfoil3, EditAirfoil, DelAirfoil
	 */
	AIRFOILHANDLE CreateAirfoil2 (AIRFOIL_ORIENTATION align, const VECTOR3 &ref, AirfoilCoeffFunc cf, double c, double S, double A) const;

	/**
	 *  \brief Creates a new airfoil and defines its aerodynamic properties.
	 * \param align orientation of the lift vector (LIFT_VERTICAL or LIFT_HORIZONTAL)
	 * \param ref centre of pressure in vessel coordinates [<b>m</b>]
	 * \param cf pointer to coefficient callback function (see notes)
	 * \param context pointer to data block passed to cf callback function
	 * \param c airfoil chord length [m]
	 * \param S wing area [m<sup>2</sup>]
	 * \param A wing aspect ratio
	 * \return Handle for the new airfoil.
	 * \note This method is an extension to \ref CreateAirfoil2, using a
	 *   more versatile coefficient callback function.
	 * \note AirfoilCoeffFuncEx has the following interface:
	 *   \code
	 *   void AirfoilCoeffFuncEx (VESSEL *v, double aoa, double M, double Re,
	 *        void *context, double *cl, double *cm, double *cd)
	 *   \endcode
	 *   where \e v is a pointer to the calling vessel instance, and
	 *   \a context is the pointer passed to CreateAirfoil3. It can be
	 *   used to make available to the callback function any additional
	 *   parameters required to calculate the lift and drag coefficients.
	 *   All other parameters are identical to AirfoilCoeffFunc (see
	 *   \ref CreateAirfoil).
	 * \sa CreateAirfoil, CreateAirfoil2, EditAirfoil, DelAirfoil
	 */
	AIRFOILHANDLE CreateAirfoil3 (AIRFOIL_ORIENTATION align, const VECTOR3 &ref, AirfoilCoeffFuncEx cf, void *context, double c, double S, double A) const;

	/**
	 *  \brief Creates a new airfoil and defines its aerodynamic properties.
	 * \param ref centre of pressure in vessel coordinates [<b>m</b>], nominally this shoud be at the CoM.
	 * \param cf pointer to coefficient callback function (see notes)
	 * \param context pointer to data block passed to cf callback function
	 * \param c airfoil chord length [m]
	 * \param S wing area [m<sup>2</sup>]
	 * \param A wing aspect ratio
	 * \return Handle for the new airfoil.
	 * \note This method is an extension to \ref CreateAirfoil3, using a
	 *   more versatile coefficient callback function.
	 * \note AirfoilCoeffFuncEx has the following interface:
	 *   \code
	 *   void (*AirfoilCoeffFuncEx2)(
	 *   VESSEL* v, 
	 *   double alpha, double beta, double gamma,
	 *   double M, double Re, void* context,
	 *   double* CA, double* CN, double* CY,
	 *   double* Cl, double* Cm, double* Cn);
	 *   \endcode
	 *   where \e v is a pointer to the calling vessel instance, and
	 *   \a context is the pointer passed to CreateAirfoil4. It can be
	 *   used to make available to the callback function any additional
	 *   parameters required to calculate the lift and drag coefficients.
	 *   The coefficients: CA, CN, and CY are force coefficients along the
	 *   vessel's Z, Y, and X body axes respectively.
	 *	 The coefficients: Cl, Cm, and Cn are moment coefficients about the
	 *   vessel's Z, X, and Y body axes respectively.
	 *   All other parameters are identical to AirfoilCoeffFunc (see
	 *   \ref CreateAirfoil).
	 * \sa CreateAirfoil, CreateAirfoil2, CreateAirfoil3, EditAirfoil, DelAirfoil
	 */
	AIRFOILHANDLE CreateAirfoil4(const VECTOR3& ref, AirfoilCoeffFuncEx2 cf, void* context, double c, double S, double A) const;

	/**
	 * \brief Returns the parameters of an existing airfoil.
	 * \param [in] hAirfoil airfoil handle
	 * \param [out] ref pointer to centre of pressure [<b>m</b>]
	 * \param [out] cf pointer to aerodynamic coefficient callback function
	 * \param [out] c pointer to chord length [m]
	 * \param [out] S pointer to wing area [m<sup>2</sup>]
	 * \param [out] A pointer to wing aspect ratio
	 * \param [out] context pointer to callback context data
	 * \return \e false indicates failure
	 * \note This function copies the airfoil parameters into the
	 *   variables referenced by the pointers in the parameter list.
	 * \note Any pointers set to NULL are ignored.
     * \code
	 *  VECTOR3 cop;
	 *  AirfoilCoeffFunc cf;
	 *  void *context;
	 *  double c, S, A;
	 *  v->GetAirfoilParam(hAirfoil,&cop,&cf,&context,&c,&S,&A);
	 * \endcode
	 * \sa CreateAirfoil, CreateAirfoil2, CreateAirfoil3, EditAirfoil, DelAirfoil
	 */
	bool GetAirfoilParam (AIRFOILHANDLE hAirfoil, VECTOR3 *ref, AirfoilCoeffFunc *cf, void **context, double *c, double *S, double *A) const;

	/**
	 * \brief Resets the parameters of an existing airfoil definition.
	 * \param hAirfoil airfoil handle
	 * \param flag bitflags to define which parameters to reset (see notes)
	 * \param ref new centre of pressure
	 * \param cf new callback function for coefficient calculation
	 * \param c new chord length [m]
	 * \param S new wing area [m<sup>2</sup>]
	 * \param A new wing aspect ratio
	 * \note This function can be used to modify the parameters of a
	 *   previously created airfoil.
	 * \note \a flag contains the bit flags defining which parameters will
	 *   be modified. It can be any combination of the following:
	 *   <table>
	 *   <tr><td>0x01</td><td>modify force attack point</td></tr>
	 *   <tr><td>0x02</td><td>modify coefficient callback function</td></tr>
	 *   <tr><td>0x04</td><td>modify chord length</td></tr>
	 *   <tr><td>0x08</td><td>modify wing area</td></tr>
	 *   <tr><td>0x10</td><td>modify wing aspect ratio</td></tr>
	 *   </table>
	 * \note If the airfoil identified by \a hAirfoil was created with
	 *   \ref CreateAirfoil3, and you want to modifiy the callback
	 *   function, then \a cf must point to a function with
	 *   \e AirfoilCoeffFuncEx interface, and must be cast to
	 *   \e AirfoilCoeffFunc when passed to EditAirfoil.
	 * \sa CreateAirfoil2, CreateAirfoil3
	 */
	void EditAirfoil (AIRFOILHANDLE hAirfoil, DWORD flag, const VECTOR3 &ref, AirfoilCoeffFunc cf, double c, double S, double A) const;

	/**
	 * \brief Deletes a previously defined airfoil.
	 * \param hAirfoil airfoil handle
	 * \return \e false indicates failure (invalid handle)
	 * \note If all the vessel's airfoils are deleted without creating
	 *   new ones, Orbiter reverts to the obsolete legacy atmospheric
	 *   flight model.
	 * \sa CreateAirfoil2, CreateAirfoil3, ClearAirfoilDefinitions
	 */
	bool DelAirfoil (AIRFOILHANDLE hAirfoil) const;

	/**
	 * \brief Removes all airfoils currently defined for the vessel.
	 * \note This function is useful if a vessel needs to re-define all
	 *   its airfoil definitions as a result of a structural change.
	 * \note After clearing all airfoils, you should generate new ones.
	 *   Even wingless objects (such as capsules) should define their
	 *   aerodynamic behaviour by airfoils (see CreateAirfoil). Vessels
	 *   without airfoil definitions revert to the obsolete legacy
	 *   atmospheric flight model.
	 * \sa DelAirfoil, CreateAirfoil, CreateAirfoil2, CreateAirfoil3
	 */
	void ClearAirfoilDefinitions () const;

	/**
	 * \brief Creates an aerodynamic control surface.
	 * \param type control surface type (see \ref airctrltype)
	 * \param area control surface area [m<sup>2</sup>]
	 * \param dCl shift in lift coefficient achieved by fully extended control
	 * \param ref centre of pressure in vessel coordinates [<b>m</b>]
	 * \param axis rotation axis (see \ref airctrlaxis)
	 * \param anim animation reference, if applicable
	 * \note Control surfaces include elevators, rudders, ailerons, flaps, etc.
	 *   They can be used to control the vessel during atmospheric flight.
	 * \note When selecting automatic axis control (axis=AIRCTRL_AXIS_AUTO), the
	 *   following axes will be used for given control surfaces:
	 *   <table>
	 *   <tr><td>Elevator</td><td>XPOS</td></tr>
	 *   <tr><td>Rudder</td><td>YPOS</td></tr>
	 *   <tr><td>Aileron</td><td>XPOS if ref.x &gt; 0, XNEG otherwise</td></tr>
	 *   <tr><td>Flap</td><td>XPOS</td></tr>
	 *   </table>
	 * \note For ailerons, at least 2 control surfaces should be defined (e.g. on
	 *   left and right wing) with opposite rotation axes, to obtain the angular
	 *   momentum for banking the vessel.
	 * \note Elevators typically use the XPOS axis, assuming the that the centre
	 *   of pressure is aft of the centre of gravity. If pitch control is provided
	 *   by a canard configuration \e ahead of the CoG, XNEG should be used instead.
	 * \note The centre of pressure defined by the \a ref parameter is the point at
	 *   which the lift and drag forces for the control surface are applied.
     * \note To improve performance, multiple control surfaces may sometimes be
	 *   defined by a single call to CreateControlSurface. For example, the elevator
	 *   controls on the left and right wing may be combined by setting a centered
	 *   attack point.
	 * \note Control surfaces can be animated, by passing an animation reference to
	 *   CreateControlSurface. The animation reference is obtained when creating the
	 *   animation with \ref CreateAnimation. The animation should support a state
	 *   in the range from 0 to 1, with neutral surface position at state 0.5.
	 * \sa CreateControlSurface2, CreateControlSurface3
	 */
	void CreateControlSurface (AIRCTRL_TYPE type, double area, double dCl,
		const VECTOR3 &ref, int axis = AIRCTRL_AXIS_AUTO, UINT anim = (UINT)-1) const;

	/**
	 * \brief Creates an aerodynamic control surface and returns a handle.
	 * \param type control surface type (see \ref airctrltype)
	 * \param area control surface area [m<sup>2</sup>]
	 * \param dCl shift in lift coefficient achieved by fully extended control
	 * \param ref centre of pressure in vessel coordinates [<b>m</b>]
	 * \param axis rotation axis (see \ref airctrlaxis)
	 * \param anim animation reference, if applicable
	 * \return Control surface handle.
	 * \note This function is identical to \ref CreateControlSurface, but it returns
	 *   a handle for later reference (e.g. to delete it with \ref DelControlSurface)
	 * \note It is equivalent to \ref CreateControlSurface3 with \a delay = 1.
	 * \sa CreateControlSurface, CreateControlSurface3, DelControlSurface
	 */
	CTRLSURFHANDLE CreateControlSurface2 (AIRCTRL_TYPE type, double area, double dCl,
		const VECTOR3 &ref, int axis = AIRCTRL_AXIS_AUTO, UINT anim = (UINT)-1) const;

	/**
	 * \brief Creates an aerodynamic control surface and returns a handle.
	 * \param type control surface type (see \ref airctrltype)
	 * \param area control surface area [m<sup>2</sup>]
	 * \param dCl shift in lift coefficient achieved by fully extended control
	 * \param ref centre of pressure in vessel coordinates [<b>m</b>]
	 * \param axis rotation axis (see \ref airctrlaxis)
	 * \param delay response delay setting [s]
	 * \param anim animation reference, if applicable
	 * \return Control surface handle.
	 * \note This function is identical to \ref CreateControlSurface2 except that
	 *   it specifies an additional 'delay' parameter which defines the response
	 *   delay for the surface (the time it takes to move from neutral to fully
	 *   deployed). Setting delay=0 provides direct response.
	 * \sa CreateControlSurface, CreateControlSurface2
	 */
	CTRLSURFHANDLE CreateControlSurface3 (AIRCTRL_TYPE type, double area, double dCl,
		const VECTOR3 &ref, int axis = AIRCTRL_AXIS_AUTO, double delay = 1.0, UINT anim = (UINT)-1) const;

	/**
	 * \brief Deletes a previously defined aerodynamic control surface.
	 * \param hCtrlSurf control surface handle
	 * \return \e false indicates error (invalid handle)
	 * \sa CreateControlSurface2, CreateControlSurface3
	 */
	bool DelControlSurface (CTRLSURFHANDLE hCtrlSurf) const;

	/**
	 * \brief Removes all aerodynamic control surfaces.
	 * \note This function is useful if the vessel has to re-define all its
	 *   control surfaces (e.g. as a result of structural change).
	 * \sa DelControlSurface
	 */
	void ClearControlSurfaceDefinitions () const;

	/**
	 * \brief Updates the position of an aerodynamic control surface.
	 * \param type control surface type (see \ref airctrltype)
	 * \param level new control surface position [-1...+1]
	 * \note Parameter \a level defines a \e target state for the surface. Control
	 *   surfaces generally require a finite amount of time to move from the
	 *   current to the target state.
	 * \note This method affects the \e permanent setting of the control surface,
	 *   while manual input via keyboard or joystick affects the \e transient
	 *   setting. The total target state of the control surface is the sum of both
	 *   settings, clamped to the range [-1...+1]
	 * \sa SetControlSurfaceLevel(AIRCTRL_TYPE,double,bool)const,
	 *   GetControlSurfaceLevel
	 */
	void SetControlSurfaceLevel (AIRCTRL_TYPE type, double level) const;

	/**
	 * \brief Updates the position of an aerodynamic control surface.
	 * \param type control surface type (see \ref airctrltype)
	 * \param level new control surface position [-1...+1]
	 * \param direct application mode
	 * \note If parameter \a direct==true then the specified level is applied
	 *   directly, bypassing any reaction delays defined for the control surface.
	 * \note If parameter \a direct==false then this method is equivalent to
	 * \ref SetControlSurfaceLevel(AIRCTRL_TYPE,double)const .
	 * \note Bypassing the response delay can be useful for debugging autopilots
	 *   etc. but should be avoided in production code, since it is unphysical.
	 *   If you want to simulate fast-responding controls, create the surface
	 *   with a small delay setting instead.
	 * \sa SetControlSurfaceLevel(AIRCTRL_TYPE,double)const, CreateControlSurface3
	 */
	void SetControlSurfaceLevel (AIRCTRL_TYPE type, double level, bool direct) const;

	/**
	 * \brief Returns the current position of a control surface.
	 * \param type control surface type (see \ref airctrltype)
	 * \return Current position of the surface [-1..+1]
	 * \note This method returns the \e actual, not the \e target position. Due to
	 *   finite response time, it may therefore not return the value set by a
	 *   preceeding call to SetControlSurfaceLevel.
	 * \sa SetControlSurfaceLevel
	 */
	double GetControlSurfaceLevel (AIRCTRL_TYPE type) const;

	/**
	 * \brief Attaches a modifyable drag component to the vessel.
	 * \param drag pointer to external drag control parameter
	 * \param factor drag magnitude scaling factor
	 * \param ref drag force attack point [<b>m</b>]
	 * \note This method is useful for defining drag produced by movable parts
	 *   such as landing gear and airbrakes.
	 * \note The magnitude of the drag force is calculated as
	 *   \f[ D = d \cdot f \cdot q_\infty \f]
	 *   where \e d is the control parameter, \e f is the scale factor, and
	 *   \e q is the freestream dynamic pressure.
	 * \note The value of \e d (the parameter pointed to by \a drag) should
	 *   be set to values between 0 (no drag) and 1 (full drag). Any changes
	 *   to the value have immediate effect.
	 * \note Depending on the attack point, the applied drag force may create
	 *   torque in addition to linear force.
	 * \sa ClearVariableDragElements
	 */
	void CreateVariableDragElement (const double *drag, double factor, const VECTOR3 &ref) const;

	/**
	 * \brief Removes all drag elements defined with CreateVariableDragElement.
	 * \sa CreateVariableDragElement
	 */
	void ClearVariableDragElements () const;
	//@}


	/// \name Aerodynamic vessel properties (legacy model)
	/// The methods in this group are used only if the vessel does not define
	/// any airfoils.
	//@{
	/**
	 * \brief Returns the vessel's wind resistance coefficients (legacy flight
	 *   model only).
	 * \param cw_z_pos resistance coefficient in positive z direction (forward)
	 * \param cw_z_neg resistance coefficient in negative z direction (back)
	 * \param cw_x resistance coefficient in lateral direction (left/right)
	 * \param cw_y resistance coefficient in vertical direction (up/down)
	 * \note <b>[Legacy aerodynamic flight model only]</b>
	 * \note The cw coefficients are only used by the legacy flight model (if
	 *   no airfoils are defined). In the presence of airfoils, drag calculations
	 *   are performed on the basis of the airfoil parameters.
	 * \note The first value (cw_z_pos) is the coefficient used if the vessel's
	 *   airspeed z-component is positive (vessel moving forward). The second
	 *   value is used if the z-component is negative (vessel moving backward).
	 * \note Lateral and vertical components are assumed symmetric.
	 * \sa SetCW, CreateAirfoil
	 */
	void GetCW (double &cw_z_pos, double &cw_z_neg, double &cw_x, double &cw_y) const;

	/**
	 * \brief Set the vessel's wind resistance coefficients along its axis directions.
	 * \param cw_z_pos coefficient in positive z direction (forward)
	 * \param cw_z_neg coefficient in negative z direction (back)
	 * \param cw_x coefficient in lateral direction (left/right)
	 * \param cw_y coefficient in vertical direction (up/down)
	 * \note <b>[Legacy aerodynamic flight model only]</b>
	 * \note The cw coefficients are only used by the legacy flight model (if
	 *   no airfoils are defined). In the presence of airfoils, drag calculations
	 *   are performed on the basis of the airfoil parameters.
	 * \note The first value (cw_z_pos) is the coefficient used if the vessel's airspeed
	 *   z-component is positive (vessel moving forward). The second value is used if
	 *   the z-component is negative (vessel moving backward).
	 * \note Lateral and vertical components are assumed symmetric.
	 * \sa GetCW, CreateAirfoil
	 */
	void SetCW (double cw_z_pos, double cw_z_neg, double cw_x, double cw_y) const;

	/**
	 * \brief Returns the vessel's wing aspect ratio (wingspan<sup>2</sup> / wing area)
	 * \return Wing aspect ratio (wingspan<sup>2</sup> / wing area)
	 * \note <b>[Legacy aerodynamic flight model only]</b>
	 * \note The aspect ratio returned by this function is only used by the
	 *   legacy aerodynamic flight model. If the vessel uses the new flight
	 *   model (i.e. defines at least one airfoil), then this value is ignored,
	 *   and the airfoil parameters are used instead.
	 * \note The aspect ratio is used in the calculation of induced drag.
	 * \sa SetWingAspect, GetWingEffectiveness, CreateAirfoil
	 */
	double GetWingAspect () const;

	/**
	 * \brief Set the wing aspect ratio (wingspan<sup>2</sup> / wing area)
	 * \param aspect wing aspect ratio
	 * \note <b>[Legacy aerodynamic flight model only]</b>
	 * \note This function defines the wing aspect ratio for the legacy flight
	 *   model. If the vessel uses the new flight model (i.e. defines at least
	 *   one airfoil), then this value is ignored, and the airfoil parameters
	 *   are used instead.
	 * \note The aspect ratio is used in the calculation of induced drag.
	 * \sa GetWingAspect, SetWingEffectiveness, CreateAirfoil
	 */
	void SetWingAspect (double aspect) const;

	/**
	 * \brief Returns the wing form factor used in aerodynamic calculations.
	 * \return wing form factor
	 * \note <b>[Legacy aerodynamic flight model only]</b>
	 * \note The form factor returned by this function is only used by the
	 *   legacy aerodynamic flight model. If the vessel uses the new flight
	 *   model (i.e. defines at least one airfoil), then this value is ignored,
	 *   and the airfoil parameters are used instead.
	 * \note The form factor, together with the aspect ratio, determines the
	 *   amount of induced drag for given lift. Higher values of the form factor
	 *   result in lower drag.
	 * \note Typical values are ~3.1 for elliptic wings, ~2.8 for tapered wings,
	 *   and ~2.5 for rectangular wings. Default is 2.8.
	 * \sa SetWingEffectiveness, GetWingAspect, CreateAirfoil
	 */
	double GetWingEffectiveness () const;

	/**
	 * \brief Set the wing form factor for aerodynamic lift and drag calculations.
	 * \param eff wing form factor
	 * \note <b>[Legacy aerodynamic flight model only]</b>
	 * \note This function defines the wing form factor for the legacy flight
	 *   model. If the vessel uses the new flight model (i.e. defines at least
	 *   one airfoil), then this value is ignored, and the airfoil parameters
	 *   are used instead.
	 * \note The form factor, together with the aspect ratio, determines the
	 *   amount of induced drag for given lift. Higher values of the form factor
	 *   result in lower drag.
	 * \note Typical values for \a eff are: ~3.1 for elliptic wings, ~2.8 for
	 *   tapered wings, ~2.5 for rectangular wings.
	 * \sa GetWingEffectiveness, SetWingAspect, CreateAirfoil
	 */
	void SetWingEffectiveness (double eff) const;

	/**
	 * \brief Returns the vessel's atmospheric rotation resistance coefficients.
	 * \param rd drag coefficients for rotation around the 3 vessel axes
	 * \note \a rd contains the components <i>r<sub>x,y,z</sub></i> against
	 *   rotation around the local vessel axes in atmosphere, where angular
	 *   deceleration due to atmospheric friction is defined as a<i><sub>x,y,z</sub></i>
	 *   = -w<i><sub>x,y,z</sub></i> <i>q</i> <i>S<sub>y</sub></i> <i>r<sub>x,y,z</sub></i>
	 *   with angular velocity w, dynamic pressure <i>q</i> and reference surface
	 *   <i>S<sub>y</sub></i>, defined by the vessel's cross section projected along
	 *   the vertical (y) axis.
	 * \sa SetRotDrag
	 */
	void GetRotDrag (VECTOR3 &rd) const;

	/**
	 * \brief Set the vessel's atmospheric rotation resistance coefficients
	 * \param rd drag coefficients for rotation around the 3 vessel axes
	 * \note \a rd contains the components <i>r<sub>x,y,z</sub></i> against
	 *   rotation around the local vessel axes in atmosphere, where angular
	 *   deceleration due to atmospheric friction is defined as a<i><sub>x,y,z</sub></i>
	 *   = -w<i><sub>x,y,z</sub></i> <i>q</i> <i>S<sub>y</sub></i> <i>r<sub>x,y,z</sub></i>
	 *   with angular velocity w, dynamic pressure <i>q</i> and reference surface
	 *   <i>S<sub>y</sub></i>, defined by the vessel's cross section projected along
	 *   the vertical (y) axis.
	 * \sa GetRotDrag
	 */
	void SetRotDrag (const VECTOR3 &rd) const;

	/**
	 * \brief Returns the scaling factor for the pitch moment.
	 * \return pitch moment scale factor
	 * \note The pitch moment is the angular moment around the vessel's lateral
	 *   (x) axis occurring in atmospheric flight. It works toward reducing
	 *   the pitch angle (angle of attack).
	 * \note The larger the scaling factor, the stronger the effect becomes
	 *   ("stiff handling")
	 * \note This value is only used with the old aerodynamic flight model,
	 *   i.e. if no airfoils have been defined.
	 * \sa SetPitchMomentScale, GetYawMomentScale, CreateAirfoil
	 */
	double GetPitchMomentScale () const;

	/**
	 * \brief Sets the scaling factor for the pitch moment.
	 * \param scale scale factor for pitch moment
	 * \note The pitch moment is the angular moment around the vessel's lateral
	 *   (x) axis occurring in atmospheric flight. It works toward reducing
	 *   the pitch angle (angle of attack) between the vessel's longitudinal
	 *   axis and the airstream vector.
	 * \note The larger the scaling factor, the stronger the effect becomes
	 *   ("stiff handling")
	 * \note This value is only used with the old aerodynamic flight model,
	 *   i.e. if not airfoils have been defined.
	 * \note The default value is 0.
	 * \sa GetPitchMomentScale, SetYawMomentScale, CreateAirfoil
	 */
	void SetPitchMomentScale (double scale) const;

	/**
	 * \brief Returns the scaling factor for the yaw moment.
	 * \return yaw moment scale factor
	 * \note The yaw moment is the angular moment around the vessel's
	 *   vertical (y) axis occurring in atmospheric flight. It works
	 *   toward reducing the slip angle between the vessel's longidudinal
	 *   axis and the airstream vector.
	 * \note This value is only used with the old aerodynamic flight model,
	 *   i.e. if no airfoils have been defined.
	 * \sa SetYawMomentScale, GetPitchMomentScale, CreateAirfoil
	 */
	double GetYawMomentScale () const;

	/**
	 * \brief Sets the scaling factor for the yaw moment.
	 * \param scale scale factor for yaw angle moment.
	 * \note The yaw moment is the angular moment around the vessel's
	 *   vertical (y) axis occurring in atmospheric flight. It works
	 *   toward reducing the slip angle between the vessel's longidudinal
	 *   axis and the airstream vector.
	 * \note This value is only used with the old aerodynamic flight model,
	 *   i.e. if not airfoils have been defined. 
	 * \note The default value is 0.
	 * \sa SetPitchMomentScale, GetYawMomentScale, CreateAirfoil
	 */
	void SetYawMomentScale (double scale) const;

	/**
	 * \brief Returns the scaling factor for the pitch trim control.
	 * \return pitch trim scale factor.
	 * \note This function returns the value previously set with SetTrimScale 
	 * \note It is only used with the old atmospheric flight model (if no
	 *   airfoils have been defined).
	 * \sa SetTrimScale, GetPitchMomentScale, GetYawMomentScale,
	 *   CreateAirfoil
	 */
	double GetTrimScale () const;

	/**
	 * \brief Sets the scaling factor for the pitch trim control.
	 * \param scale pitch trim scaling factor
	 * \note This method is used only in combination with the old flight model,
	 *   that is, if the vessel doesn't define any airfoils. In the new
	 *   flight model, this has been replaced by CreateControlSurface
	 *   (AIRCTRL_ELEVATORTRIM, ...).
	 * \note If scale is set to zero (default) the vessel does not have a pitch
	 *   trim control. 
	 * \sa GetTrimScale, SetPitchMomentScale, SetYawMomentScale,
	 *   CreateAirfoil, CreateControlSurface 
	 */
	void SetTrimScale (double scale) const;

	/**
	 * \brief Defines the callback function for aerodynamic lift calculation.
	 * \param lcf pointer to callback function (see notes)
	 * \note <b>[Legacy aerodynamic flight model only]</b>
	 * \note This method defines callback function for lift calculation as a
	 *   function of angle of attack for the legacy flight model. If the vessel
	 *   uses the new flight model (i.e. defines at least one airfoil), then this
	 *   value is ignored, and the airfoil parameters are used instead.
     * \note The interface of the callback function is defined as
	 *   \code typedef double (*LiftCoeffFunc)(double aoa) \endcode
	 *   where \e aoa is the angle of attack [rad], and the return value is the
	 *   resulting lift coefficient.
	 * \note The callback function must be able to process input aoa values in the
	 *   range -Pi ... +Pi.
	 * \note The preferred method for defining lift and drag characteristics is via
	 *   the CreateAirfoil method, which is much more versatile. Orbiter ignores the
	 *   SetLiftCoeffFunc function if any airfoils have been created.
     * \note If neither airfoils are defined, nor this method is called, then the
	 *   default behaviour is not to generate any aerodynamic lift.
	 * \sa CreateAirfoil
	 */
	void SetLiftCoeffFunc (LiftCoeffFunc lcf) const;
	//@}


	/// \name Forces
	//@{
	/**
	 * \brief Returns magnitude of aerodynamic lift force vector.
	 * \return Magnitude of lift force vector [N].
	 * \note Return value is the sum of lift components from all airfoils.
	 * \sa GetLiftVector, GetDrag
	 */
	double GetLift () const;

	/**
	 * \brief Returns magnitude of aerodynamic drag force vector.
	 * \return Magnitude of drag force vector [N].
	 * \note Return value is the sum of drag components from all airfoils.
	 * \sa GetDragVector, GetLift
	 */
	double GetDrag () const;

	/**
	 * \brief Returns magnitude of aerodynamic side-force vector.
	 * \return Magnitude of drag force vector [N].
	 * \note Return value is the sum of drag components from all airfoils.
	 * \sa GetDragVector, GetLift
	 */
	double GetSideForce() const;

	/**
	 * \brief Returns gravitational force vector in local vessel coordinates.
	 * \param[out] G gravitational force vector [<b>N</b>]
	 * \return Always true.
	 * \note When the vessel status is updated dynamically, G is composed of
	 *   all gravity sources currently used for the vessel propagation
	 *   (excluding sources with contributions below threshold).
	 * \note During orbit stabilisation, only the contribution from the
	 *   primary source is returned.
	 * \sa GetThrustVector, GetLiftVector, GetDragVector, GetForceVector
	 */
	bool GetWeightVector (VECTOR3 &G) const;

	/**
	 * \brief Returns thrust force vector in local vessel coordinates.
	 * \param[out] T thrust vector [<b>N</b>]
	 * \return false indicates zero thrust. In that case, the returned
	 *   vector is (0,0,0).
	 * \note On return, T contains the vector sum of thrust components from
	 *   all engines.
	 * \note This function provides information about the linear thrust
	 *   force, but not about the angular moment (torque) induced.
	 * \sa GetWeightVector, GetLiftVector, GetDragVector, GetForceVector
	 */
	bool GetThrustVector (VECTOR3 &T) const;

	/**
	 * \brief Returns aerodynamic lift force vector in local vessel
	 *   coordinates.
	 * \param[out] L lift vector [<b>N</b>]
	 * \return false indicates zero lift. In that case, the returned vector
	 *   is (0,0,0).
	 * \note Return value is the sum of lift components from all airfoils.
	 * \note The lift vector is perpendicular to the relative wind (and thus
	 *   to the drag vector) and has zero x-component.
	 * \sa GetLift, GetWeightVector, GetThrustVector, GetDragVector,
	 *   GetForceVector
	 */
	bool GetLiftVector (VECTOR3 &L) const;

	/**
	 * \brief Returns aerodynamic drag force vector in local vessel
	 *   coordinates.
	 * \param[out] D drag vector [<b>N</b>]
	 * \return false indicates zero drag. In that case, the returned vector
	 *   is (0,0,0).
	 * \note On return, D contains the sum of drag components from all
	 *   airfoils.
	 * \note The drag vector is parallel to the relative wind (direction of
	 *   air flow).
	 * \sa GetDrag, GetWeightVector, GetThrustVector, GetLiftVector,
	 *   GetForceVector
	 */
	bool GetDragVector (VECTOR3 &D) const;

	/**
	 * \brief Returns aerodynamic side-force force vector in local vessel
	 *   coordinates.
	 * \param[out] SF drag vector [<b>N</b>]
	 * \return false indicates zero side-force. In that case, the returned vector
	 *   is (0,0,0).
	 * \note On return, SD contains the sum of Side-force components from all
	 *   airfoils.
	 * \note The side-force vector is mutually orthogonal to the Lift and Drag vectors
	 * \sa GetDrag, GetWeightVector, GetThrustVector, GetLiftVector,
	 *   GetForceVector
	 */
	bool GetSideForceVector (VECTOR3 &SF) const;

	/**
	 * \brief Returns total force vector acting on the vessel in local
	 *   vessel coordinates.
	 * \param[out] F total force vector [<b>N</b>]
	 * \return Always true
	 * \note On return, F contains the sum of all forces acting on the
	 *   vessel.
	 * \note This may not be equal to the sum of weight, thrust, lift and
	 *   drag vectors, because it also includes surface contact forces,
	 *   user-defined forces and any other forces.
	 * \sa GetWeightVector, GetThrustVector, GetLiftVector, GetDragVector,
	 *   GetTorqueVector
	 */
	bool GetForceVector (VECTOR3 &F) const;

	/**
	 * \brief Returns the total torque vector acting on the vessel in
	 *   local vessel coordinates.
	 * \param[out] M total torque vector [<b>Nm</b>]
	 * \return Always true
	 * \note On return, M contains the total torque vector acting on the
	 *   vessel in its centre of mass. The torque vector contains
	 *   contributions from thrusters, aerodynamic forces and gravity
	 *   gradient effects (if enabled).
	 * \sa GetForceVector
	 */
	bool GetTorqueVector (VECTOR3 &M) const;

	/**
	 * \brief Add a custom body force.
	 * \param F force vector [<b>N</b>]
	 * \param r force attack point in local vessel coordinates [<b>m</b>]
	 * \note This function can be used to implement custom forces (braking
	 *   chutes, tethers, etc.). It should not be used for standard forces
	 *   such as engine thrust or aerodynamic forces which are handled
	 *   internally (although in theory this function makes it possible to
	 *   bypass Orbiter's built-in thrust and aerodynamics model completely
	 *   and replace it by a user-defined model).
	 * \note The force is applied only for the next time step. AddForce will
	 *   therefore usually be used inside the VESSEL2::clbkPreStep callback
	 *   function.
	 * \sa GetForceVector
	 */
	void AddForce (const VECTOR3 &F, const VECTOR3 &r) const;
	//@}


	/// \name Fuel management
	//@{
	/**
	 * \brief Create a new propellant resource ("fuel tank")
	 *
	 * Propellant resources are a component of the vessel's propulsion
	 * system. They can hold propellants and distribute them to connected
	 * engines to generate thrust.
	 * \param maxmass maximum propellant capacity of the tank [kg]
	 * \param mass initial propellant mass of the resource [kg]
	 * \param efficiency fuel efficiency factor (>0)
	 * \return propellant resource handle
	 * \note Orbiter doesn't distinguish between propellant and oxidant. A
	 *   "propellant resource" is assumed to be a combination of fuel and
	 *   oxidant resources.
	 * \note The interpretation of a propellant resource (liquid or solid
	 *   propulsion system, ion drive, etc.) is up to the vessel developer.
	 * \note The rate of fuel consumption depends on the thrust level and
	 *   Isp (fuel-specific impulse) of the thrusters attached to the
	 *   resource.
	 * \note The fuel efficiency rating, together with a thruster's Isp
	 *   rating, determines how much fuel is consumed per second to obtain a
	 *   given thrust: \f$R = F (e \cdot Isp)^{-1}\f$ with fuel rate R
	 *   [kg/s], thrust F [N], efficiency e and fuel-specific impulse Isp
	 *   [m/s].
	 * \note If mass < 0 then mass = maxmass is substituted.
	 * \sa DelPropellantResource, SetPropellantMaxMass, SetPropellantMass,
	 *   SetPropellantEfficiency, GetPropellantMaxMass, GetPropellantMass,
	 *   GetPropellantEfficiency
	 */
	PROPELLANT_HANDLE CreatePropellantResource (double maxmass, double mass=-1.0, double efficiency=1.0) const;

	/**
	 * \brief Remove a propellant resource.
	 * \param ph propellant resource handle (NULL on return)
	 * \note If any thrusters were attached to this fuel resource, they are
	 *   disabled until connected to a new fuel resource.
	 * \sa CreatePropellantResource, ClearPropellantResources
	 */
	void DelPropellantResource (PROPELLANT_HANDLE &ph) const;

	/**
	 * \brief Remove all propellant resources for the vessel.
	 * \note After a call to this function, all the vessel's thrusters will
	 *   be disabled until they are linked to new resources.
	 * \sa DelPropellantResource
	 */
	void ClearPropellantResources () const;

	/**
	 * \brief Returns the current number of vessel propellant resources.
	 * \return Number of propellant resources currently defined for the
	 *   vessel.
	 * \sa CreatePropellantResource, GetPropellantHandleByIndex
	 */
	DWORD GetPropellantCount () const;

	/**
	 * \brief Returns the handle of a propellant resource for a given index.
	 * \param idx propellant resource index (>= 0)
	 * \return Propellant resource handle
	 * \note The index must be in the range between 0 and
	 *   GetPropellantCount()-1. If the index is out of range, the returned
	 *   handle is NULL.
	 * \note The index of a given propellant resource may change if any
	 *   resources are deleted. The handle remains valid until the
	 *   corresponding resource is deleted.
	 * \sa CreatePropellantResource, GetPropellantCount
	 */
	PROPELLANT_HANDLE GetPropellantHandleByIndex (DWORD idx) const;

	/**
	 * \brief Returns the maximum capacity of a propellant resource.
	 * \param ph propellant resource handle
	 * \return Max. propellant capacity [kg].
	 * \sa SetPropellantMaxMass, GetPropellantMass, SetPropellantMass
	 */
	double GetPropellantMaxMass (PROPELLANT_HANDLE ph) const;

	/**
	 * \brief Reset the maximum capacity of a fuel resource.
	 * \param ph propellant resource handle
	 * \param maxmass max. fuel capacity (>= 0) [kg]
	 * \note The actual fuel mass contained in the tank is not affected by
	 *   this function, unless the new maximum propellant mass is less than
	 *   the current fuel mass, in which case the fuel mass is reduced to
	 *   the maximum capacity.
	 * \sa GetPropellantMaxMass, SetPropellantMass, GetPropellantMass,
	 *   GetTotalPropellantMass, GetFuelMass, SetPropellantEfficiency
	 */
	void SetPropellantMaxMass (PROPELLANT_HANDLE ph, double maxmass) const;

	/**
	 * \brief Returns the current mass of a propellant resource.
	 * \param ph propellant resource handle
	 * \return Current propellant mass [kg].
	 * \sa SetPropellantMass, GetPropellantMaxMass, SetPropellantMaxMass
	 */
	double GetPropellantMass (PROPELLANT_HANDLE ph) const;

	/**
	 * \brief Reset the current mass of a propellant resource.
	 * \param ph propellant resource handle
	 * \param mass propellant mass (>= 0) [kg]
	 * \note 0 <= mass <= maxmass is required, where maxmass is the maximum
	 *   capacity of the propellant resource.
	 * \note This method should be used to simulate refuelling, fuel leaks,
	 *   cross-feeding between tanks, etc. but not for normal fuel
	 *   consumption by thrusters (which is handled internally by the
	 *   Orbiter core).
	 * \sa GetPropellantMass, SetPropellantMaxMass, GetTotalPropellantMass,
	 *   GetFuelMass, SetPropellantEfficiency
	 */
	void SetPropellantMass (PROPELLANT_HANDLE ph, double mass) const;

	/**
	 * \brief Returns the vessel's current total propellant mass.
	 * \return Sum of current mass of all propellant resources defined for
	 *   the vessel [kg].
	 * \sa GetPropellantMass, GetPropellantMaxMass
	 */
	double GetTotalPropellantMass () const;

	/**
	 * \brief Returns the efficiency factor of a propellant resource.
	 * \param ph propellant resource handle
	 * \return Fuel efficiency factor
	 * \note The fuel efficiency rating, together witha thruster's Isp
	 *   rating, determines how much fuel is consumed per second to obtain
	 *   a given thrust: R = F/(e Isp) with fuel rate R [kg/s], thrust F [N],
	 *   efficiency e and fuel-specific impulse Isp [m/s].
	 * \sa SetPropellantEfficiency, GetPropellantMaxMass,
	 *   CreatePropellantResource
	 */
	double GetPropellantEfficiency (PROPELLANT_HANDLE ph) const;

	/**
	 * \brief Reset the efficiency factor of a fuel resource.
	 * \param ph propellant resource handle
	 * \param efficiency fuel efficiency factor (> 0)
	 * \note The fuel efficiency rating, together witha thruster's Isp
	 *   rating, determines how much fuel is consumed per second to obtain
	 *   a given thrust: R = F/(e Isp) with fuel rate R [kg/s], thrust F [N],
	 *   efficiency e and fuel-specific impulse Isp [m/s].
	 * \sa GetPropellantEfficiency, CreatePropellantResource,
	 *   SetPropellantMaxMass, SetPropellantMass
	 */
	void SetPropellantEfficiency (PROPELLANT_HANDLE ph, double efficiency) const;

	/**
	 * \brief Returns the current mass flow rate from a propellant resource.
	 * \param ph propellant resource handle
	 * \return Current propellant mass flow rate [kg/s].
	 * \sa GetPropellantMass, GetTotalPropellantFlowrate, GetFuelRate
	 */
	double GetPropellantFlowrate (PROPELLANT_HANDLE ph) const;

	/**
	 * \brief Returns the current total mass flow rate, summed over all
	 *   propellant resources.
	 * \return Total propellant mass flow rate [kg/s].
	 * \sa GetPropellantFlowrate, GetFuelRate
	 */
	double GetTotalPropellantFlowrate () const;

	/**
	 * \brief Define a "default" propellant resource.
	 *
	 * This is used for the various legacy fuel-related API functions, and
	 *   for the "Fuel" indicator in the generic panel-less HUD display.
	 * \param ph propellant resource handle
	 * \note If this function is not called, the first propellant resource
	 *   is used as default.
	 * \sa CreatePropellantResource, GetDefaultPropellantResource
	 */
	void SetDefaultPropellantResource (PROPELLANT_HANDLE ph) const;

	/**
	 * \brief Returns the handle for the vessel's default propellant resource.
	 * \return Default propellant resource handle
	 * \sa SetDefaultPropellantResource
	 */
	PROPELLANT_HANDLE GetDefaultPropellantResource () const;

	/**
	 * \brief Returns the maximum capacity of the vessel's default propellant
	 *   resource.
	 * \return Max. capacity of default propellant resource [kg].
	 * \note The function returns 0 if no fuel resources are defined.
	 * \sa GetPropellantMaxMass, SetDefaultPropellantResource
	 */
	double GetMaxFuelMass () const;

	/**
	 * \brief Set the maximum fuel capacity of the vessel's default
	 *   propellant resource.
	 * \param mass max. propellant mass [kg].
	 * \note If no propellant resources are defined for the vessel, a call
	 *   to this method creates a new propellant resource with the specified
	 *   capacity.
	 * \note If the vessel already contains propellant resources, this
	 *   method resets the maximum capacity of the vessel's default resource.
	 * \sa SetPropellantMaxMass, SetDefaultPropellantResource
	 */
	void SetMaxFuelMass (double mass) const;

	/**
	 * \brief Returns the current mass of the vessel's default propellant
	 *   resource.
	 * \return Current mass of the default propellant resource [kg].
	 * \sa GetPropellantMass, SetDefaultPropellantResource
	 */
	double GetFuelMass () const;

	/**
	 * \brief Reset the current mass of the vessel's default propellant
	 *   resource.
	 * \param mass new propellant mass [kg].
	 * \note mass must be between 0 and the maximum capacity of the
	 *   propellant resource.
	 * \note If the vessel has not defined any propellant resources, this
	 *   method has no effect.
	 * \sa GetFuelMass, SetPropellantMass, SetMaxFuelMass,
	 *   SetDefaultPropellantResource
	 */
	void SetFuelMass (double mass) const;

	/**
	 * \brief Returns the current mass flow rate from the default propellant
	 *   resource.
	 * \return Current mass flow rate from the default propellant resource
	 *   [kg/s].
	 * \sa GetPropellantFlowrate, SetDefaultPropellantResource
	 */
	double GetFuelRate () const;
	//@}

	/// \name Thruster management
	//@{
	/**
	 * \brief Add a logical thruster definition for the vessel.
	 * \param pos thrust force attack point in vessel coordinates [<b>m</b>]
	 * \param dir thrust force direction in vessel coordinates (normalised)
	 * \param maxth0 max. vacuum thrust rating [N]
	 * \param hp propellant resource feeding the thruster
	 * \param isp0 vacuum fuel-specific impulse (Isp) [m/s]
	 * \param isp_ref Isp value at reference pressure p_ref [m/s]
	 * \param p_ref reference pressure for Isp_ref [Pa]
	 * \return Thruster identifier
	 * \note The fuel-specific impulse defines how much thrust is produced
	 *   by burning 1kg of fuel per second. If the Isp level is not
	 *   specified or is = 0, a default value is used (see SetISP()).
	 * \note To define the thrust and Isp ratings to be pressure-dependent,
	 *   specify an isp_ref value > 0, and set p_ref to the corresponding
	 *   atmospheric pressure. Thrust and Isp at pressure p will then be
	 *   calculated as
	 *   \f[
	 *   \mathrm{Isp}(p) = \mathrm{Isp}_0(1-p\eta), \qquad
	 *   \mathrm{Th}(p) = \mathrm{Th}_0(1-p\eta), \qquad \mathrm{where}\qquad
	 *   \eta = \frac{\mathrm{Isp}_0 - \mathrm{Isp}_\mathrm{ref}}{p_\mathrm{ref} \mathrm{Isp}_0}
	 *   \f]
	 * \note If isp_ref = 0 then no pressure-dependency is assumed (\f$\eta=0\f$).
	 * \note If no propellant resource is specified, the thruster is
	 *   disabled until it is linked to a resource by SetThrusterResource().
	 * \note If isp0 <= 0, then the default Isp value is substituted (see SetISP()).
	 * \note Thruster forces can create linear as well as angular
	 *   moments, depending on the attack point and direction.
	 * \note Use CreateThrusterGroup to assemble thrusters into logical
	 *   groups.
	 * \sa DelThruster, CreateThrusterGroup, AddExhaust, SetISP,
	 *   SetThrusterIsp, SetThrusterResource
	 */
	THRUSTER_HANDLE CreateThruster (const VECTOR3 &pos, const VECTOR3 &dir, double maxth0,
		PROPELLANT_HANDLE hp=NULL, double isp0=0.0, double isp_ref=0.0, double p_ref=101.4e3) const;

	/**
	 * \brief Delete a logical thruster definition.
	 * \param th thruster handle (NULL on return)
	 * \return true on success, false if the supplied thruster handle was
	 *   invalid.
	 * \note Deleted thrusters will be automatically removed from all
	 *   thruster groups they had been assigned to.
	 * \note All exhaust render definitions which refer to the deleted
	 *   thruster are removed.
	 * \sa CreateThruster, CreateThrusterGroup, AddExhaust
	 */
	bool DelThruster (THRUSTER_HANDLE &th) const;

	/**
	 * \brief Delete all thruster and thruster group definitions.
	 * \note This function removes all thruster definitions, as well as all
	 *   the thruster group definitions.
	 * \note It also removes all previously defined exhaust render
	 *   definitions.
	 * \sa CreateThruster, DelThruster, CreateThrusterGroup, AddExhaust
	 */
	void ClearThrusterDefinitions () const;

	/**
	 * \brief Returns the number of thrusters currently defined.
	 * \return Number of logical thruster definitions.
	 * \sa CreateThruster, GetThrusterHandleByIndex
	 */
	DWORD GetThrusterCount () const;

	/**
	 * \brief Returns the handle of a thruster specified by its index.
	 * \param idx thruster index (>= 0)
	 * \return Thruster handle
	 * \note The index must be in the range between 0 and nthruster-1,
	 *   where nthruster is the thruster count returned by
	 *   GetThrusterCount(). If the index is out of range, the returned
	 *   handle is NULL. 
	 * \note The index of a given thruster may change if vessel thrusters
	 *   are deleted. The handle remains valid until the corresponding
	 *   thruster is deleted.
	 * \sa CreateThruster, DelThruster, GetThrusterCount
	 */
	THRUSTER_HANDLE GetThrusterHandleByIndex (DWORD idx) const;

	/**
	 * \brief Returns a handle for the propellant resource feeding the
	 *   thruster.
	 * \param th thruster handle
	 * \return Propellant resource handle, or NULL if the thruster is not
	 *   connected.
	 * \sa SetThrusterResource, CreateThruster
	 */
	PROPELLANT_HANDLE GetThrusterResource (THRUSTER_HANDLE th) const;

	/**
	 * \brief Connect a thruster to a propellant resource.
	 * \param th thruster handle
	 * \param ph propellant resource handle
	 * \note A thruster can only be connected to one propellant resource at
	 *   a time. Setting a new resource disconnects from the previous
	 *   resource.
	 * \note To disconnect the thruster from its current tank, use \a ph = NULL.
	 * \sa GetThrusterResource
	 */
	void SetThrusterResource (THRUSTER_HANDLE th, PROPELLANT_HANDLE ph) const;

	/**
	 * \brief Returns the thrust force attack point of a thruster.
	 * \param[in] th thruster handle
	 * \param[out] pos thrust attack point [<b>m</b>]
	 * \note \a pos is returned in the vessel frame of reference.
	 * \sa SetThrusterRef, GetThrusterDir
	 */
	void GetThrusterRef (THRUSTER_HANDLE th, VECTOR3 &pos) const;	

	/**
	 * \brief Reset the thrust force attack point of a thruster.
	 * \param th thruster handle
	 * \param pos new force attack point [<b>m</b>]
	 * \note \a pos is specified in the vessel reference system.
	 * \note This method should be used whenever a thruster has been
	 *   physically moved in the vessel's local frame of reference.
	 * \note If the vessel's centre of gravity, i.e. the origin of its
	 *   reference system, is moved with ShiftCG(), the thruster positions
	 *   are updated automatically.
	 * \note The attack point has no influence on the linear force exerted
	 *   on the vessel by the thruster, but it affects the induced torque.
	 * \sa GetThrusterRef, CreateThruster, ShiftCG, SetThrusterDir
	 */
	void SetThrusterRef (THRUSTER_HANDLE th, const VECTOR3 &pos) const;

	/**
	 * \brief Returns the force direction of a thruster.
	 * \param[in] th thruster handle
	 * \param[out] dir thrust direction (vessel frame of reference)
	 * \sa SetThrusterDir, GetThrusterRef
	 */
	void GetThrusterDir (THRUSTER_HANDLE th, VECTOR3 &dir) const;

	/**
	 * \brief Reset the force direction of a thruster.
	 * \param th thruster handle
	 * \param dir new thrust direction (vessel frame of reference)
	 * \note This method can be used to realise a tilt of the rocket
	 *   motor (e.g. for implementing a thruster gimbal mechanism)
	 * \sa GetThrusterDir, CreateThruster, SetThrusterRef
	 */
	void SetThrusterDir (THRUSTER_HANDLE th, const VECTOR3 &dir) const;

	/**
	 * \brief Returns the maximum vacuum thust rating of a thruster.
	 * \param th thruster handle
	 * \return Maximum vacuum thust rating [N]
	 * \note To retrieve the actual current maximum thrust rating (which may
	 *   be lower in the presence of ambient atmospheric pressure), use
	 *   GetThrusterMax().
	 * \sa SetThrusterMax0,\n
	 *   GetThrusterMax(THRUSTER_HANDLE)const,\n
	 *   GetThrusterMax(THRUSTER_HANDLE,double)const
	 */
	double GetThrusterMax0 (THRUSTER_HANDLE th) const;

	/**
	 * \brief Reset the maximum vacuum thrust rating of a thruster.
	 * \param th thruster handle
	 * \param maxth0 new maximum vacuum thrust rating [N]
	 * \note The max. thrust rating in the presence of atmospheric ambient
	 *   pressure may be lower than the vacuum thrust if a pressure-dependent
	 *   Isp value has been defined.
	 * \sa GetThrusterMax0, CreateThruster,\n
	 *   SetThrusterIsp(THRUSTER_HANDLE,double)const,\n
	 *   SetThrusterIsp(THRUSTER_HANDLE,double,double,double)const
	 */
	void SetThrusterMax0 (THRUSTER_HANDLE th, double maxth0) const;

	/**
	 * \brief Returns the current maximum thrust rating of a thruster.
	 * \param th thruster handle
	 * \return Max. thrust rating a the current atmospheric pressure [N].
	 * \note If a pressure-dependent Isp rating has been defined for the
	 *   thruster, and if the vessel is moving through a planetary
	 *   atmosphere, this method returns the maximum thrust rating given
	 *   the current atmospheric pressure.
	 * \note Otherwise it returns the maximum vacuum thrust rating of the
	 *   thruster.
	 * \sa GetThrusterMax(THRUSTER_HANDLE,double)const,\n
	 *   SetThrusterIsp(THRUSTER_HANDLE,double)const,\n
	 *   SetThrusterIsp(THRUSTER_HANDLE,double,double,double)const,\n
	 *   CreateThruster
	 */
	double GetThrusterMax (THRUSTER_HANDLE th) const;

	/**
	 * \brief Returns the maximum thrust rating of a thruster at a specific
	 *   ambient pressure.
	 * \param th thruster handle
	 * \param p_ref reference pressure [Pa]
	 * \return Max. thrust rating a atmospheric pressure \a p_ref [N].
	 * \note If a pressure-dependent Isp rating has been defined for the
	 *   thruster, this method returns the maximum thrust rating at
	 *   ambient pressure \a p_ref.
	 * \note Otherwise it returns the maximum vacuum thrust rating of the
	 *   thruster.
	 * \sa GetThrusterMax(THRUSTER_HANDLE)const,\n
	 *   SetThrusterIsp(THRUSTER_HANDLE,double)const,\n
	 *   SetThrusterIsp(THRUSTER_HANDLE,double,double,double)const,\n
	 *   CreateThruster
	 */
	double GetThrusterMax (THRUSTER_HANDLE th, double p_ref) const;

	/**
	 * \brief Returns the vacuum fuel-specific impulse (Isp) rating for a
	 *   thruster.
	 * \param th thruster handle
	 * \return Isp value in vacuum [m/s]
	 * \note Equivalent to GetThrusterIsp (th,0)
	 * \sa GetThrusterIsp(THRUSTER_HANDLE)const,\n
	 *   GetThrusterIsp(THRUSTER_HANDLE,double)const,\n
	 *   SetThrusterIsp(THRUSTER_HANDLE,double)const,\n
	 *   SetThrusterIsp(THRUSTER_HANDLE,double,double,double)const,\n
	 *   CreateThruster
	 */
	double GetThrusterIsp0 (THRUSTER_HANDLE th) const;

	/**
	 * \brief Returns the current fuel-specific impulse (Isp) rating of a
	 *   thruster.
	 * \param th thruster handle
	 * \return Current Isp value [m/s].
	 * \note If the vessel is moving within a planetary atmosphere, and if a
	 *   pressure-dependent Isp rating has been defined for this thruster,
	 *   the returned Isp value will vary with ambient atmospheric pressure.
	 * \sa GetThrusterIsp(THRUSTER_HANDLE,double)const,\n
	 *   GetThrusterIsp0,\n
	 *   SetThrusterIsp(THRUSTER_HANDLE,double)const,\n
	 *   SetThrusterIsp(THRUSTER_HANDLE,double,double,double)const,\n
	 *   CreateThruster
	 */
	double GetThrusterIsp (THRUSTER_HANDLE th) const;

	/**
	 * \brief Returns the fuel-specific impulse (Isp) rating of a thruster
	 *   at a specific ambient atmospheric pressure.
	 * \param th thruster handle
	 * \param p_ref reference pressure [Pa]
	 * \return Isp value at ambient pressure \a p_ref [m/s].
	 * \note If no pressure-dependent Isp rating has been defined for this
	 *   thruster, it will always return the vacuum rating, independent of
	 *   the specified pressure.
	 * \note To obtain vacuum Isp rating, set \a p_ref to 0.
	 * \note To obtain the Isp rating at (Earth) sea level, set \a p_ref =
	 *   101.4e3.
	 * \sa GetThrusterIsp(THRUSTER_HANDLE)const,\n
	 *   GetThrusterIsp0,\n
	 *   SetThrusterIsp(THRUSTER_HANDLE,double)const,\n
	 *   SetThrusterIsp(THRUSTER_HANDLE,double,double,double)const,\n
	 *   CreateThruster
	 */
	double GetThrusterIsp (THRUSTER_HANDLE th, double p_ref) const;

	/**
	 * \brief Reset the fuel-specific impulse (Isp) rating of a thruster,
	 *   assuming no pressure dependence.
	 * \param th thruster handle
	 * \param isp new Isp rating [m/s]
	 * \note The Isp value correlates the propellant mass flow rate dm/dt
	 *   with the resulting thrust force F:
	 *   F = Isp (dm/dt).
	 * \note In the engineering literature, fuel-specific impulse is
	 *   sometimes given in units of time, by dividing the Isp as defined
	 *   above by the gravitational acceleration 1g = 9.81 m/s<sup>2</sup>.
	 * \note The specified Isp value is assumed to be independent of ambient
	 *   atmospheric pressure. To define a pressure-dependent Isp value, use
	 *   SetThrusterIsp(THRUSTER_HANDLE,double,double,double)const.
	 * \sa SetThrusterIsp(THRUSTER_HANDLE,double,double,double)const,\n
	 *   GetThrusterIsp(THRUSTER_HANDLE)const,\n
	 *   GetThrusterIsp(THRUSTER_HANDLE,double)const, GetThrusterIsp0,\n
	 *   CreateThruster
	 */
	void SetThrusterIsp (THRUSTER_HANDLE th, double isp) const;

	/**
	 * \brief Reset the fuel-specific impulse (Isp) rating of a thruster
	 *   including a pressure dependency.
	 * \param th thruster handle
	 * \param isp0 vacuum Isp rating [m/s]
	 * \param isp_ref Isp rating at ambient pressure \a p_ref [m/s]
	 * \param p_ref reference pressure [Pa] for isp_ref (defaults to Earth
	 *   sea level pressure)
	 * \note See SetThrusterIsp(THRUSTER_HANDLE,double)const for a
	 *   definition of the relationship between Isp, thrust and fuel mass
	 *   flow rate.
	 * \sa SetThrusterIsp(THRUSTER_HANDLE,double)const,\n
	 *   GetThrusterIsp(THRUSTER_HANDLE)const,\n
	 *   GetThrusterIsp(THRUSTER_HANDLE,double)const,\n
	 *   GetThrusterIsp0, CreateThruster
	 */
	void SetThrusterIsp (THRUSTER_HANDLE th, double isp0, double isp_ref, double p_ref=101.4e3) const;

	/**
	 * \brief Returns the current thrust level setting of a thruster.
	 * \param th thruster handle
	 * \return Current thrust level (0...1)
	 * \note To obtain the actual force [N] currently generated by the
	 *   thruster, multiply the thrust level with the max. thrust rating
	 *   returned by GetThrusterMax().
	 * \sa GetThrusterMax, SetThrusterLevel
	 */
	double GetThrusterLevel (THRUSTER_HANDLE th) const;

	/**
	 * \brief Set thrust level for a thruster.
	 * \param th thruster handle
	 * \param level thrust level (0...1)
	 * \note At level 1 the thruster generates maximum force, as defined by
	 *   its maxth parameter.
	 * \note Certain thrusters are controlled directly by Orbiter via
	 *   primary input controls (e.g. joystick throttle control for main
	 *   thrusters), which may override this function.
	 * \sa IncThrusterLevel, GetThrusterLevel
	 */
	void SetThrusterLevel (THRUSTER_HANDLE th, double level) const;

	/**
	 * \brief Apply a change to the thrust level of a thruster.
	 * \param th thruster handle
	 * \param dlevel thrust level change (-1...1)
	 * \note The applied thrust level change is limited to give a resulting
	 *   thrust level in the range (0...1).
	 * \sa SetThrusterLevel, GetThrusterLevel
	 */
	void IncThrusterLevel (THRUSTER_HANDLE th, double dlevel) const;

	/**
	 * \brief Set the thrust level of a thruster for the current time step
	 *   only.
	 * \param th thruster handle
	 * \param level thrust level (0...1)
	 * \note At level 1 the thruster generates maximum force, as defined by
	 *   its maxth parameter.
	 * \note This method overrides the thruster's permanent thrust level
	 *   for the current time step only, so it should normally only be used
	 *   in the body of the VESSEL2::clbkPreStep() method.
	 * \sa SetThrusterLevel, VESSEL2::clbkPreStep()
	 */
	void SetThrusterLevel_SingleStep (THRUSTER_HANDLE th, double level) const;

	/**
	 * \brief Apply a thrust level change to a thruster for the current time
	 *   step only.
	 * \param th thruster handle
	 * \param dlevel thrust level change (-1...1)
	 * \note This method overrides the thruster's permanent thrust level for
	 *   the current time step only, so it should normally only be used in
	 *   the body of the VESSEL2::clbkPreStep() method.
	 * \note This method may be overridden by manual user input via keyboard
	 *   and joystick, or by automatic attitude sequences.
	 * \note The applied thrust level change is limited to give a resulting
	 *   thrust level in the range (0...1).
	 * \sa SetThrusterLevel_SingleStep, IncThrusterLevel, VESSEL2::clbkPreStep()
	 */
	void IncThrusterLevel_SingleStep (THRUSTER_HANDLE th, double dlevel) const;

	/**
	 * \brief Returns the linear moment (force) and angular moment (torque)
	 *   currently generated by a thruster.
	 * \param th thruster handle
	 * \param F linear force [N]
	 * \param T torque [Nm]
	 * \note The returned values include the influence of ambient pressure
	 *   on the thrust generated by the engine.
	 */
	void GetThrusterMoment (THRUSTER_HANDLE th, VECTOR3 &F, VECTOR3 &T) const;

	/**
	 * \brief Returns the vessel's current default fuel-specific impulse.
	 * \return Fuel-specific impulse [m/s]. The is the amount of thrust
	 *   [N] obtained by burning 1kg of fuel per second.
	 * \note The function returns the current default Isp value which will be
	 *   used for all subsequently defined thrusters which do not define
	 *   their individual Isp settings.
	 * \note To obtain an actual Isp value for a thruster, use GetThrusterISP.
	 * \note The default Isp value can be set by the SetISP() method, or via the
	 *   'Isp' entry in the vessel configuration file. If not defined, the default
	 *   value is 5e4.
	 * \sa SetISP, GetThrusterISP
	 */
	double GetISP () const;

	/**
	 * \brief Sets the default Isp value for subsequently created thrusters.
	 * \param isp fuel-specific impulse [m/s]
	 * \note The Isp value defines the amount of thrust [N] obtained by burning 1
	 *   kg of fuel per second.
	 * \note Resetting the default Isp value affects only thrusters which are
	 *   created subsequently, and which don't define individual Isp values.
	 * \note Before the first call to SetISP, the initial value is read from
	 *   the 'Isp' entry of the vessel definition file. If no entry exists, a value
	 *   of 5e4 is used.
	 * \note It is recommended to define individual Isp values during thruster
	 *   creation instead of using SetISP.
	 */
	void SetISP (double isp) const;
	//@}

	/// \name Thruster group management
	//@{
	/**
	 * \brief Combine thrusters into a logical group.
	 * \param th array of thruster handles to form a group
	 * \param nth number of thrusters in the array
	 * \param thgt thruster group type (see \ref thrusterparam)
	 * \return thruster group handle
	 * \note Thruster groups (except for user-defined groups) are engaged by
	 *   Orbiter as a result of user input. For example, pushing the stick backward
	 *   in rotational attitude mode will engage the thrusters in the
	 *   THGROUP_ATT_PITCHUP group.
	 * \note It is the responsibility of the vessel designer to make sure that the
	 *   thruster groups are designed so that they behave in a sensible way.
	 * \note Thrusters can be added to more than one group. For example, an
	 *   attitude thruster can be simultaneously grouped into THGROUP_ATT_PITCHUP
	 *   and THGROUP_ATT_UP.
	 * \note Rotational thrusters should be designed so that they don't induce a
	 *   significant linear momentum. This means rotational groups require at least
	 *   2 thrusters each.
	 * \note Linear thrusters should be designed such that they don't induce a
	 *   significant angular momentum.
	 * \note If a vessel does not define a complete set of attitude thruster
	 *   groups, certain navmode sequences (e.g. KILLROT) may fail.
	 * \note In addition to the pre-defined set of default thruster groups, multiple
	 *   user-defined groups can be created like this:
	 *   \code{.cpp}
	 *     THGROUP_TYPE MyThrusterGroup1 = THGROUP_USER;
	 *     THGROUP_TYPE MyThrusterGroup2 = (THGROUP_TYPE)(THGROUP_USER+1);
	 *     CreateThrusterGroup(th1, nth1, MyThrusterGroup1);
	 *     CreateThrusterGroup(th2, nth2, MyThrusterGroup2);
	 *   \endcode
	 * \sa DelThrusterGroup, CreateThruster, thrusterparam
	 */
	THGROUP_HANDLE CreateThrusterGroup (THRUSTER_HANDLE *th, int nth, THGROUP_TYPE thgt) const;

	/**
	 * \brief Delete a thruster group and (optionally) all associated thrusters.
	 * \param thg thruster group handle
	 * \param delth thruster destruction flag (see notes)
	 * \return \e true on success.
	 * \note If \a delth==true, all thrusters associated with the group will be
	 *   destroyed. Note that this can have side effects if the thrusters were
	 *   associated with multiple groups, since they are removed from all those
	 *   groups as well.
	 * \sa DelThrusterGroup(THGROUP_TYPE,bool)const, CreateThrusterGroup,
	 *   DelThruster, thrusterparam
	 */
	bool DelThrusterGroup (THGROUP_HANDLE thg, bool delth = false) const;

	/**
	 * \brief Delete a default thruster group and (optionally) all associated
	 *   thrusters.
	 * \param thgt thruster group type (excluding  THGROUP_USER)
	 * \param delth thruster destruction flag
	 * \return \e true on success
	 * \note This version can only be used for default thruster groups
	 *   (< THGROUP_USER).
	 * \note If \a delth==true, all thrusters associated with the group will be
	 *   destroyed. Note that this can have side effects if the thrusters were
	 *   associated with multiple groups, since they are removed from all those
	 *   groups as well.
	 * \sa DelThrusterGroup(THGROUP_HANDLE,bool)const,
	 *   CreateThrusterGroup, DelThruster, thrusterparam
	 */
	bool DelThrusterGroup (THGROUP_TYPE thgt, bool delth = false) const;

	/**
	 * \brief Returns the handle of a thruster group (default or user-defined).
	 * \param thgt thruster group type (see \ref thrusterparam)
	 * \return thruster group handle (or NULL if no group is defined for the
	 *   specified type).
	 * \note The thruster group type must not be THGROUP_USER. To retrieve the
	 *   handle of a nonstandard thruster group, use
	 *   GetUserThrusterGroupHandleByIndex().
	 * \sa GetUserThrusterGroupHandleByIndex
	 */
	THGROUP_HANDLE GetThrusterGroupHandle (THGROUP_TYPE thgt) const;

	/**
	 * \brief Returns the handle of a user-defined (nonstandard) thruster group.
	 * \param idx index of user-defined thruster group (>= 0)
	 * \return thruster group handle (or NULL if index out of range)
	 * \note Use this method only to retrieve handles for nonstandard thruster
	 *   groups (created with the THGROUP_USER flag). For standard groups, use
	 *   GetThrusterGroupHandle() instead.
	 * \note The index must be in the range between 0 and nuserthgroup-1, where
	 *   nuserthgroup is the number of nonstandard thruster groups. Use
	 *   GetUserThrusterGroupCount() to obtain this value.
	 * \sa GetThrusterGroupHandle, GetUserThrusterGroupCount
	 */
	THGROUP_HANDLE GetUserThrusterGroupHandleByIndex (DWORD idx) const;

	/**
	 * \brief Returns the number of thrusters assigned to a logical thruster group.
	 * \param thg thruster group handle
	 * \return Number of thrusters assigned to the specified thruster group.
	 * \note Thrusters can be assigned to more than one group (and some thrusters
	 *   may not be assigned to any group) so the sum of GetGroupThrusterCount
	 *   values over all groups can be different to the total number of thrusters.
	 * \sa GetGroupThrusterCount(THGROUP_TYPE)const
	 */
	DWORD GetGroupThrusterCount (THGROUP_HANDLE thg) const;

	/**
	 * \brief Returns the number of thrusters assigned to a standard logical thruster
	 *   group.
	 * \param thgt thruster group enumeration type (see \ref thrusterparam)
	 * \return Number of thrusters assigned to the specified thruster group.
	 * \note This function only works for standard group types. Do not use it with
	 *   THGROUP_USER. For user-defined groups, use
	 *   VESSEL::GetGroupThrusterCount(THGROUP_HANDLE)const instead.
	 * \note Thrusters can be assigned to more than one group (and some thrusters
	 *   may not be assigned to any group) so the sum of GetGroupThrusterCount
	 *   values over all groups can be different to the overall number of thrusters.
	 * \sa GetGroupThrusterCount(THGROUP_HANDLE)const
	 */
	DWORD GetGroupThrusterCount (THGROUP_TYPE thgt) const;

	/**
	 * \brief Returns a handle for a thruster that belongs to a specified thruster
	 *   group.
	 * \param thg thruster group handle
	 * \param idx thuster index (0 <= idx < GetGroupThrusterCount())
	 * \return Thuster handle
	 */
	THRUSTER_HANDLE GetGroupThruster (THGROUP_HANDLE thg, DWORD idx) const;

	/**
	 * \brief Returns a handle for a thruster that belongs to a thruster
	 *   group (default or user-defined).
	 * \param thgt thruster group enumeration type (see \ref thrusterparam)
	 * \param idx thruster index (0 <= idx < GetGroupThrusterCount())
	 * \return Thruster handle
	 * \note This function only works for standard group types. Do not use with
	 *   THGROUP_USER. For user-defined groups, use GetGroupThruster(THGROUP_HANDLE,DWORD)const.
	 */
	THRUSTER_HANDLE GetGroupThruster (THGROUP_TYPE thgt, DWORD idx) const;

	/**
	 * \brief Returns the number of user-defined (nonstandard) thruster groups.
	 * \return Number of user-defined thruster groups.
	 * \note The value returned by this method only includes user-defined thruster
	 *   groups (created with the THGROUP_USER flag). It does not contain any standard
	 *   thruster groups (such as THGROUP_MAIN, etc.)
	 */
	DWORD GetUserThrusterGroupCount () const;

	/**
	 * \brief Indicates if a default thruster group is defined by the vessel.
	 * \param thgt thruster group enumeration type (see \ref thrusterparam)
	 * \return \e true if the group contains any thrusters, \e false otherwise.
	 * \note This method only works for default groups. Do not use with
	 *   THGROUP_USER.
	 * \note A group is considered to be "defined" if it contains at least one
	 *   thruster.
	 * \sa GetGroupThrusterCount
	 */
	bool ThrusterGroupDefined (THGROUP_TYPE thgt) const;

	/**
	 * \brief Sets the thrust level for all thrusters in a group.
	 * \param thg thruster group identifier
	 * \param level new thrust level (range 0-1)
     * \sa SetThrusterGroupLevel(THGROUP_TYPE,double)const
	 */
	void SetThrusterGroupLevel (THGROUP_HANDLE thg, double level) const;

	/**
	 * \brief Sets the thrust level for all thrusters in a standard group.
	 * \param thgt thruster group type (see \ref thrusterparam)
	 * \param level new thrust level (range 0-1)
	 * \note This method can only be used with standard thruster group types. Do
	 *   not use with THGROUP_USER.
	 * \sa SetThrusterGroupLevel (THGROUP_HANDLE,double)const
	 */
	void SetThrusterGroupLevel (THGROUP_TYPE thgt, double level) const;

	/**
	 * \brief Increments the thrust level for all thrusters in a group.
	 * \param thg thruster group identifier
	 * \param dlevel thrust level increment
	 * \note Resulting thrust levels are automatically truncated to the range [0..1]
	 * \note Use negative \a dlevel to decrement the thrust level.
	 * \sa VESSEL::IncThrusterGroupLevel(THGROUP_TYPE,double)const
     */
	void IncThrusterGroupLevel (THGROUP_HANDLE thg, double dlevel) const;

	/**
	 * \brief Increments the thrust level for all thrusters in a standard group.
	 * \param thgt thruster group type
	 * \param dlevel thrust level increment
	 * \note This method can be used for standard thruster group types enumerated in
	 *   \ref thrusterparam except THGROUP_USER.
	 * \note Resulting thrust levels are automatically truncated to the range [0..1]
	 * \note Use negative \a dlevel to decrement the thrust level.
	 * \sa VESSEL::IncThrusterGroupLevel(THGROUP_HANDLE,double)const
	 */
	void IncThrusterGroupLevel (THGROUP_TYPE thgt, double dlevel) const;

	/**
	 * \brief Increments the thrust level of a group for a single time step.
	 * \param thg thruster group identifier
	 * \param dlevel thrust level increment
	 * \note The total thrust level of a thruster group is composed of the sum of
	 *   a \e permanent and an \e override portion, constrained to range [0..1].
	 *   The permanent setting only changes when reset explicitly, while the
	 *   override setting is reset to zero after each time step.
	 * \note This function increments the override portion of the thrust level
	 *   for the thruster group for the current time step only.
	 * \note Negative values for the override thrust level are permitted to
	 *   reduce the total thrust level below its permanent setting (down to
	 *   a minimum of 0).
	 * \note Any override adjustments of individual thrusters in the group with
	 *   \ref IncThrusterLevel_SingleStep are added to their total level.
	 * \sa IncThrusterGroupLevel_SingleStep(THGROUP_TYPE,double)const,
	 *   IncThrusterLevel_SingleStep
	 */
	void IncThrusterGroupLevel_SingleStep (THGROUP_HANDLE thg, double dlevel) const;

	/**
	 * \brief Increments the thrust level of a standard group for a single time step.
	 * \param thgt thruster group type
	 * \param dlevel thrust level increment
	 * \note The total thrust level of a thruster group is composed of the sum of
	 *   a \e permanent and an \e override portion, constrained to range [0..1].
	 *   The permanent setting only changes when reset explicitly, while the
	 *   override setting is reset to zero after each time step.
	 * \note This function increments the override portion of the thrust level
	 *   for the thruster group for the current time step only.
	 * \note Negative values for the override thrust level are permitted to
	 *   reduce the total thrust level below its permanent setting (down to
	 *   a minimum of 0).
	 * \note Any override adjustments of individual thrusters in the group with
	 *   \ref IncThrusterLevel_SingleStep are added to their total level.
	 * \sa IncThrusterGroupLevel_SingleStep(THGROUP_HANDLE,double)const,
	 *   IncThrusterLevel_SingleStep
	 */
	void IncThrusterGroupLevel_SingleStep (THGROUP_TYPE thgt, double dlevel) const;

	/**
	 * \brief Returns the mean thrust level for a thruster group.
	 * \param thg thruster group identifier
	 * \return Mean group thrust level [0..1]
	 * \note In general, this method is only useful for groups where all thrusters
	 *   have the same maximum thrust rating and the same thrust direction.
	 * \sa GetThrusterGroupLevel(THGROUP_TYPE)const
	 */
	double GetThrusterGroupLevel (THGROUP_HANDLE thg) const;

	/**
	 * \brief Returns the mean thrust level for a default thruster group.
	 * \param thgt thruster group type
	 * \return Mean group thrust level [0..1]
	 * \note In general, this method is only useful for groups where all thrusters
	 *   have the same maximum thrust rating and the same thrust direction.
	 * \sa GetThrusterGroupLevel(THGROUP_HANDLE)const
	 */
	double GetThrusterGroupLevel (THGROUP_TYPE thgt) const;

	/**
	 * \brief Returns the thrust level of an attitude thruster group set via
	 *   keyboard or mouse input.
	 * \param thgt thruster group identifier
	 * \param mode attitude control mode (see \ref manctrl_mode)
	 * \param device input device (see \ref manctrl_dev)
	 * \return Manual thrust level [0..1]
	 * \note If \a mode is not MANCTRL_ANYMODE, only thruster groups which
	 *   are of the specified mode (linear or rotational) will return
	 *   nonzero values.
	 */
	double GetManualControlLevel (THGROUP_TYPE thgt, DWORD mode = MANCTRL_ATTMODE, DWORD device = MANCTRL_ANYDEVICE) const;
	//@}

	/// \name Reaction control system
	//@{
	/**
	 * \brief Returns the current RCS (reaction control system) thruster
	 *   mode.
	 * \return Current RCS mode (see \ref rcsmode)
	 * \note The reaction control system consists of a set of small
	 *   thrusters arranged around the vessel. They can be fired in
	 *   pre-defined configurations to provide either a change in angular
	 *   velocity (in RCS_ROT mode) or in linear velocity (in RCS_LIN mode).
	 * \note RCS_NONE indicates that the RCS is disabled or not available.
	 * \note Currently Orbiter doesn't allow simultaneous linear and
	 *   rotational RCS control via keyboard or joystick. The user has to
	 *   switch between the two. However, simultaneous operation is possible
	 *   via the "RControl" plugin module.
	 * \note Not all vessel classes may define a complete RCS.
	 * \sa SetAttitudeMode, rcsmode
	 */
	int GetAttitudeMode () const;

	/**
	 * \brief Sets the vessel's RCS (reaction control system) thruster mode.
	 * \param mode New RCS mode (see \ref rcsmode)
	 * \return true on success, false for invalid argument
	 * \note The reaction control system consists of a set of small
	 *   thrusters arranged around the vessel. They can be fired in
	 *   pre-defined configurations to provide either a change in angular
	 *   velocity (in RCS_ROT mode) or in linear velocity (in RCS_LIN mode).
	 * \note Set RCS_NONE to disable the RCS.
	 * \sa GetAttitudeMode, rcsmode 
	 */
	bool SetAttitudeMode (int mode) const;

	/**
	 * \brief Switch between linear and rotational RCS mode
	 * \return New RCS mode index
	 * \note If the RCS is disabled, this method does nothing and returns 0.
	 * \note During playback, this method does nothing and returns the current RCS mode.
	 * \sa SetAttitudeMode, GetAttitudeMode
	 */
	int ToggleAttitudeMode () const;

	/**
	 * \brief Returns the current combined thrust levels for the reaction control
	 *   system thruster groups in rotational mode.
	 * \param[out] th vector containing RCS thruster group levels for rotation
	 *   around the 3 principal vessel axes (values: -1 to +1).
	 * \note The fractional thrust levels of the RCS engines for rotation around
	 *   the vessel's x, y and z axis are returned in the x, y, and z components of
	 *   \a th, respectively.
	 * \note The orientation of the vessel axes is implementation-dependent, but
	 *   usually by convention, +x is "right", +y is "up", and +z is "forward".
	 * \note A value of +1 denotes maximum thrust in the positive direction around
	 *   an axis, while -1 denotes maximum thrust in the negative direction.
	 * \note This method combines the results of calls to GetThrusterGroupLevel for
	 *   all relevant RCS thruster groups in the following combinations:
	 *   <table>
	 *   <tr><td>th.x</td><td>THGROUP_ATT_PITCHUP - THGROUP_ATT_PITCHDOWN</td></tr>
	 *   <tr><td>th.y</td><td>THGROUP_ATT_YAWLEFT - THGROUP_ATT_YAWRIGHT</td></tr>
	 *   <tr><td>th.z</td><td>THGROUP_ATT_BANKRIGHT - THGROUP_ATT_BANKLEFT</td></tr>
	 *   </table>
	 * \note To obtain the actual thrust force magnitudes [N], the absolute values
	 *   must be multiplied with the max. attitude thrust.
	 * \sa GetAttitudeLinLevel, SetAttitudeRotLevel, GetThrusterGroupLevel,
	 *   GetAttitudeMode
	 */
	void GetAttitudeRotLevel (VECTOR3 &th) const;

	/**
	 * \brief Set RCS thruster levels for rotation in all 3 vessel axes.
	 * \param th RCS thruster levels for rotation around x,y,z axes (range -1...+1)
	 * \note This method is functional even if the manual RCS input mode is set
	 *   to linear.
	 * \note If RCS thrusters are involved in multiple rotation groups, calling
	 *   this method can lead to side effects due to thruster level saturation.
	 *   In this case, the maximum commanded level should be sufficiently low that
	 *   a thruster level doesn't saturate if it is engaged by all involved groups
	 *   simultaneously. For example, if a thruster is member of the
	 *   THGROUP_ATT_PITCHUP and THGROUP_ATT_BANKRIGHT groups, then commanding
	 *   simultaneous pitch up and bank right should be limited to level 0.5:
	 *   SetAttitudeRotLevel(0.5, 0, 0.5)
	 * \note Commanding rotations around multiple axes simultaneously can often
	 *   lead to complex rotation behaviour due to moment transfer between axes
	 *   (torques and accelerations coupled by Euler's equations).
	 * \sa SetAttitudeRotLevel(int,double)const, GetAttitudeRotLevel,
	 *   SetAttitudeLinLevel
	 */
	void SetAttitudeRotLevel (const VECTOR3 &th) const;

	/**
	 * \brief Set RCS thruster level for rotation around a single axis.
	 * \param axis rotation axis (0=x, 1=y, 2=z)
	 * \param th RCS thruster level (range -1...+1)
	 * \note This method is functional even if the manual RCS input mode is set
	 *   to linear.
	 * \note Calling this method can have side effects if the RCS thrusters
	 *   for the specified axis are also registered for other attitude axes.
	 * \sa SetAttitudeRotLevel(const VECTOR3&)const, GetAttitudeRotLevel,
	 *   SetAttitudeLinLevel
	 */
	void SetAttitudeRotLevel (int axis, double th) const;

	/**
	 * \brief Returns the current combined thrust levels for the reaction control
	 *   system thruster groups in linear (translational) mode.
	 * \param[out] th vector containing RCS thruster group levels for translation
	 *   along the 3 principal vessel axes (values: -1 to +1)
	 * \note The fractional thrust levels of the RCS engines for translation along
	 *   the vessel's x, y and z axis are returned in the x, y, and z components of
	 *   \a th, respectively.
	 * \note The orientation of the vessel axes is implementation-dependent, but
	 *   usually by convention, +x is "right", +y is "up", and +z is "forward".
	 * \note A value of +1 denotes maximum thrust in the positive direction along
	 *   an axis, while -1 denotes maximum thrust in the negative direction.
	 * \note This method combines the results of calls to GetThrusterGroupLevel for
	 *   all relevant RCS thruster groups in the following combinations:
	 *   <table>
	 *   <tr><td>th.x</td><td>THGROUP_ATT_RIGHT - THGROUP_ATT_LEFT</td></tr>
	 *   <tr><td>th.y</td><td>THGROUP_ATT_UP - THGROUP_ATT_DOWN</td></tr>
	 *   <tr><td>th.z</td><td>THGROUP_ATT_FORWARD - THGROUP_ATT_BACK</td></tr>
	 *   </table>
	 * \note To obtain the actual thrust force magnitudes [N], the absolute values
	 *   must be multiplied with the max. attitude thrust.
	 * \sa GetAttitudeRotLevel, SetAttitudeLinLevel, GetThrusterGroupLevel,
	 *   GetAttitudeMode
	 */
	void GetAttitudeLinLevel (VECTOR3 &th) const;

	/**
	 * \brief Set RCS thruster levels for linear translation in all 3 vessel axes.
	 * \param th RCS thruster levels (range -1...+1)
	 * \note This method is functional even if the manual RCS input mode is set
	 *   to rotational.
	 * \sa SetAttitudeLinLevel(int,double)const, SetAttitudeLinLevel,
	 *   SetAttitudeRotLevel
	 */
	void SetAttitudeLinLevel (const VECTOR3 &th) const;

	/**
	 * \brief Set RCS thruster level for linear translation along a single axis.
	 * \param axis translation axis (0=x, 1=y, 2=z)
	 * \param th RCS thruster level (range -1...+1)
	 * \note This method is functional even if the manual RCS input mode is set
	 *   to rotational.
	 * \sa SetAttitudeLinLevel(const VECTOR3&)const, SetAttitudeLinLevel,
	 *   SetAttitudeRotLevel
	 */
	void SetAttitudeLinLevel (int axis, double th) const;
	//@}

	/// \name Communication interface
	//@{
	/**
	 * \brief Send a simulated buffered key event to the vessel
	 * \param key key code
	 * \param down key down event flag
	 * \param kstate key state map for additional modifier keys
	 * \return Process flag (0=key not processed, 1=key processed)
	 * \note This method simulates a manual keyboard press and can be used
	 *   to trigger actions associated with the key.
	 * \note If \a down = true, a key down event is simulated. Otherwise,
	 *   a key up event is simulated.
	 * \note Additional modifier keys (e.g. Ctrl, Shift, Alt) can be set
	 *   by passing a kstate array with the appropriate keys defined.
	 * \note This method triggers a call to VESSEL2::clbkConsumeBufferedKey.
	 *   If not consumed by the callback function, the key event is offered
	 *   to the default key handler.
	 * \sa VESSEL2::clbkConsumeBufferedKey
	 */
	int SendBufferedKey (DWORD key, bool down=true, char *kstate=0);
	//@}

	/// \name Navigation radio interface
	//@{
	/**
	 * \brief Defines the number of navigation (NAV) radio receivers
	 *   supported by the vessel.
	 * \param nnav number of NAV radio receivers
	 * \note A vessel requires NAV radio receivers to obtain instrument
	 *   navigation aids such as ILS or docking approach information.
	 * \note If no NAV receivers are available, then certain MFD modes such
	 *   as Landing or Docking will not be supported.
	 * \note Default is 2 NAV receivers.
	 * \sa GetNavCount
	 */
	void InitNavRadios (DWORD nnav) const;

	/**
	 * \brief Returns the number of NAV radio receivers.
	 * \return Number of NAV receivers (>= 0)
	 * \sa InitNavRadios
	 */
	DWORD GetNavCount () const;

	/**
	 * \brief Sets the channel of a NAV radio receiver.
	 * \param n receiver index (>= 0)
	 * \param ch channel (>= 0)
	 * \return \e false on error (receiver index or channel out of range),
	 *   \e true otherwise
	 * \note NAV radios can be tuned from 108.00 to 139.95 MHz in steps of
	 *   0.05 MHz, corresponding to channels 0 to 639.
	 * \sa InitNavRadios, GetNavChannel
	 */
	bool SetNavChannel (DWORD n, DWORD ch) const;

	/**
	 * \brief Returns the current channel setting of a NAV radio receiver.
	 * \param n receiver index (>= 0)
	 * \return Receiver channel [0..639]. If index \a n is out of range, the
	 *   return value is 0.
	 * \sa GetNavRecvFreq, SetNavChannel
	 */
	DWORD GetNavChannel (DWORD n) const;

	/**
	 * \brief Returns the current radio frequency of a NAV radio receiver.
	 * \param n receiver index (>= 0)
	 * \return Receiver frequency [MHz] (range 108.00 to 139.95). If index
	 *   \a n is out of range, the return value is 0.0.
	 * \sa GetNavChannel
	 */
	float GetNavRecvFreq (DWORD n) const;

	/**
	 * \brief Enable/disable transmission of transponder signal.
	 * \param enable \e true to enable the transponder, \e false to disable.
	 * \note The transponder is a radio transmitter which can be used by other
	 *   vessels to obtain navigation information, e.g. for docking
	 *   rendezvous approaches.
	 * \note If the transponder is turned on (enable = true), its initial
	 *   frequency is set to 108.00 MHz (channel 0). Use
	 *   \ref SetTransponderChannel to tune to a different frequency.
	 * \sa SetTransponderChannel, SetIDSChannel
	 */
	void EnableTransponder (bool enable) const;

	/**
	 * \brief Switch the channel number of the vessel's transponder.
	 * \param ch transponder channel [0..639]
	 * \return \e false indicates failure (transponder not enabled or
	 *   input parameter out of range)
	 * \note Transponders can be tuned from 108.00 to 139.95 MHz in steps
	 *   of 0.05 MHz. The frequency corresponding to a channel number \a ch
	 *   is given by f = (108.0 + 0.05 \a ch) MHz.
	 * \sa EnableTransponder, SetNavChannel
	 */
	bool SetTransponderChannel (DWORD ch) const;

	/**
	 * \brief Enable/disable one of the vessel's IDS (Instrument Docking
	 *   System) transmitters.
	 * \param hDock docking port handle
	 * \param bEnable \e true to enable, \e false to disable the IDS for the dock.
	 * \note If the IDS transmitter is turned on (\a bEnable = \e true), its
	 *   channel is initially set to 0 (transmitter frequency 108.00 MHz). Use
	 *   \ref SetIDSChannel to tune to a different channel.
	 * \sa SetIDSChannel, EnableTransponder
	 */
	void EnableIDS (DOCKHANDLE hDock, bool bEnable) const;

	/**
	 * \brief Switch the channel number of one of the vessel's IDS (Instrument
	 *   Docking System) transmitters.
	 * \param hDock docking port handle
	 * \param ch IDS channel [0..639]
	 * \return \e false indicates failure (IDS not enabled or input parameter
	 *   out of range)
	 * \note IDS transmitters can be tuned from 108.00 to 139.95 MHz in steps
	 *   of 0.05 MHz. The frequency corresponding to a channel number \a ch
	 *   is given by f = (108.0 + 0.05 \a ch) MHz.
	 * \sa EnableIDS, SetTransponderChannel, SetNavChannel
	 */
	bool SetIDSChannel (DOCKHANDLE hDock, DWORD ch) const;

	/**
	 * \brief Return handle of vessel transponder if available.
	 * \return Navigation radio handle of the vessel's transponder, or NULL if
	 *   not available.
	 * \note This function returns NULL unless the transponder has been enabled
	 *   by a call to \ref EnableTransponder or by setting the EnableXPDR entry
	 *   in the vessel's config file to TRUE.
	 * \note It is not safe to store the handle, because it can become invalid
	 *   as a result of disabling/enabling the transponder. Instead, the handle
	 *   should be queried when needed.
	 * \note The handle can be used to retrieve information about the transmitter,
	 *   such as current frequency.
	 * \sa EnableTransponder, SetTransponderChannel
	 */
	NAVHANDLE GetTransponder () const;

	/**
	 * \brief Return handle of one of the vessel's instrument docking system
	 *   (IDS) radio transmitters.
	 * \param hDock docking port handle
	 * \return Navigation radio handle of the vessel's IDS transmitter for docking
	 *   port \a hDock.
	 * \note This function returns NULL if \a hDock does not define an IDS
	 *   transmitter.
	 * \note Docking port handles are returned by the \ref CreateDock and
	 *   \ref GetDockHandle methods.
	 * \note The IDS handle becomes invalid when the dock is deleted (e.g. as
	 *   a result of \ref DelDock or \ref ClearDockDefinitions).
	 * \note The handle returned by this function can be used to retrieve
	 *   information about the transmitter, such as sender frequency.
	 * \sa CreateDock, GetDockHandle, DelDock, ClearDockDefinitions, EnableIDS,
	 *   GetTransponder
	 */
	NAVHANDLE GetIDS (DOCKHANDLE hDock) const;

	/**
	 * \brief Return handle of transmitter source currently received by one of
	 *   the vessel's NAV receivers.
	 * \param n NAV receiver index (>= 0)
	 * \return handle of transmitter currently received, or NULL if the receiver
	 *   is not tuned to any station, or if \a n is out of range.
	 * \note The handle returned by this function may change in consecutive calls,
	 *   depending on the radio frequency of the corresponding receiver, the vessel
	 *   position and the position of radio transmitters in the range of the receiver.
	 */
	NAVHANDLE GetNavSource (DWORD n) const;
	//@}

	/// \name Cockpit camera methods
	//@{
	/**
	 * \brief Set the camera position for internal (cockpit) view.
	 * \param co camera offset in vessel coordinates [<b>m</b>]
	 * \note The camera offset can be used to define the pilot's eye position in the
	 *   spacecraft.
	 * \note The default offset is (0,0,0).
	 * \note This function is called typically either globally in
	 *   \ref VESSEL2::clbkSetClassCaps, if the camera position doesn't change
	 *   between views, or individually in \ref VESSEL2::clbkLoadGenericCockpit,
	 *   \ref VESSEL2::clbkLoadPanel and \ref VESSEL2::clbkLoadVC for each defined
	 *   view.
	 * \sa GetCameraOffset
	 */
	void SetCameraOffset (const VECTOR3 &co) const;

	/**
	 * \brief Returns the current camera position for internal (cockpit) view.
	 * \param co camera offset in vessel coordinates [<b>m</b>]
	 * \sa SetCameraOffset
	 */
	void GetCameraOffset (VECTOR3 &co) const;

	/**
	 * \brief Set the default camera direction for internal (cockpit) view.
	 * \param cd new default direction in vessel coordinates
	 * \note By default, the default direction is (0,0,1), i.e. forward.
	 * \note The supplied direction vector must be normalised to length 1.
	 * \note Calling this function automatically sets the current actual view
	 *   direction to the default direction.
	 * \note This function can either be called during VESSEL2::clbkSetClassCaps,
	 *   to define the default camera direction globally for the vessel, or during
	 *   VESSEL2::clbkLoadGenericCockpit, VESSEL2::clbkLoadPanel and VESSEL2::clbkLoadVC,
	 *   to define different default directions for different instrument panels or
	 *   virtual cockpit positions.
	 * \note In Orbiter, the user can return to the default direction by pressing the
	 *   \e Home key on the cursor key pad.
	 * \sa SetCameraDefaultDirection(const VECTOR3&,double)const,
	 *   GetCameraDefaultDirection, VESSEL2::clbkSetClassCaps,
	 *   VESSEL2::clbkLoadGenericCockpit, VESSEL2::clbkLoadPanel, VESSEL2::clbkLoadVC
	 */
	void SetCameraDefaultDirection (const VECTOR3 &cd) const;

	/**
	 * \brief Set the default camera direction and tilt angle for internal (cockpit) view.
	 * \param cd new default direction in vessel coordinates
	 * \param tilt camera tilt angle around the default direction [rad]
	 * \note This function allows to set the camera tilt angle in addition to the default
	 *   direction.
	 * \note By default, the default direction is (0,0,1), i.e. forward, and the tilt
	 *   angle is 0 (upright).
	 * \note The supplied direction vector must be normalised to length 1.
	 * \note The tilt angle should be in the range [-Pi,+Pi]
	 * \note Calling this function automatically sets the current actual view direction to
	 *   the default direction.
	 * \sa SetCameraDefaultDirection(const VECTOR3&)const, GetCameraDefaultDirection
	 */
	void SetCameraDefaultDirection (const VECTOR3 &cd, double tilt) const;

	/**
	 * \brief Returns the default camera direction for internal (cockpit) view.
	 * \param cd default camera direction in vessel coordinates
	 * \note The default camera direction may change as a result of invoking
	 *   SetCameraDefaultDirection, typically when the user selects a different instrument
	 *   panel or virtual cockpit position.
	 * \note The returned direction vector is normalised to length 1.
	 * \sa SetCameraDefaultDirection(const VECTOR3&)const,
	 *   SetCameraDefaultDirection(const VECTOR3&,double)const
	 */
	void GetCameraDefaultDirection (VECTOR3 &cd) const;

	/**
	 * \brief Set the angle over which the cockpit camera auto-centers to default direction.
	 * \param cangle auto-center catchment angle [rad]
	 * \note The cockpit camera auto-centers to its default ("forward") direction when
	 *   it is close enough to this direction. This function can be used to specify the
	 *   angle over which auto-centering occurs.
	 * \note Setting cangle=0 disables the auto-centering function.
	 * \note The default catchment angle is 5 degrees (5.0*RAD).
	 * \note To reset the catchment angle globally for all cockpit views of the vessel,
	 *   SetCameraCatchAngle would typically used in VESSEL2::clbkSetClassCaps(). To reset
	 *   the catchment angle for individual cockpit positions, the function would be used
	 *   for the appropriate cockpit modes in VESSEL2::clbkLoadPanel() and VESSEL2::clbkLoadVC().
	 */
	void SetCameraCatchAngle (double cangle) const;

	/**
	 * \brief Sets the range over which the cockpit camera can be rotated from its default
	 *   direction.
	 * \param left rotation range to the left [rad]
	 * \param right rotation range to the right [rad]
	 * \param up rotation range up [rad]
	 * \param down rotation range down [rad]
	 * \note The meaning of the "left", "right", "up" and "down" directions is given by the
	 *   orientation of the local vessel frame. For a default view direction of (0,0,1),
	 *   "left" is a rotation towards the -x axis, "right" is a rotation towards the +x axis,
	 *   "up" is a rotation towards the +y axis, and "down" is a rotation towards the -y axis.
	 * \note All ranges must be >= 0. The left and right ranges should be < Pi. The up and
	 *   down ranges should be < Pi/2.
	 * \note The default values are 0.8Pi for left and right ranges, and 0.4Pi for up and down
	 *   ranges.
	 * \sa SetCameraShiftRange, SetCameraMovement
	 */
	void SetCameraRotationRange (double left, double right, double up, double down) const;

	/**
	 * \brief Set the linear movement range for the cockpit camera.
	 *
	 * Defining a linear movement allows the user to move the head forward or sideways, e.g. to
	 *   get a better look out of a window, or a closer view of a virtual cockpit instrument
	 *   panel.
	 * \param fpos offset vector when leaning forward [<b>m</b>]
	 * \param lpos offset vector when leaning left [<b>m</b>]
	 * \param rpos offset vector when leaning right [<b>m</b>]
	 * \note If a linear movement range is defined with this function, the user can 'lean'
	 *   forward or sideways using the 'cockpit slew' keys. Supported keys are:
	 *   <table col="3">
	 *   <tr><td><b>Name</b></td><td><b>default</b></td><td><b>action</b></td></tr>
	 *   <tr><td>CockpitCamDontLean</td><td>Ctrl+Alt+Down</td><td>return to default position</td></tr>
	 *   <tr><td>CockpitCamLeanForward</td><td>Ctrl+Alt+Up</td><td>lean forward</td></tr>
	 *   <tr><td>CockpitCamLeanLeft</td><td>Ctrl+Alt+Left</td><td>lean left</td></tr>
	 *   <tr><td>CockpitCamLeanRight</td><td>Ctrl+Alt+Right</td><td>lean right</td></tr>
	 *   </table>
	 * \note The movement vectors are taken relative to the default cockpit position defined
	 *   via SetCameraOffset.
	 * \note This function should be called when initialising a cockpit mode (e.g. in
	 *   clbkLoadPanel or clbkLoadVC). By default, Orbiter resets the linear movement range
	 *   to zero whenever the cockpit mode changes.
	 * \note In addition to the linear movement, the camera also turns left when leaning left,
	 *   turns right when leaning right, and returns to default direction when leaning forward.
	 *   For more control over camera rotation at the different positions, use SetCameraMovement
	 *   instead.
	 * \sa SetCameraMovement, SetCameraRotationRange
	 */
	void SetCameraShiftRange (const VECTOR3 &fpos, const VECTOR3 &lpos, const VECTOR3 &rpos) const;

	/**
	 * \brief Set both linear movement range and orientation of the cockpit camera when "leaning"
	 *   forward, left and right.
	 * \param fpos offset vector when leaning forward [<b>m</b>]
	 * \param fphi camera rotation azimuth angle when leaning forward [rad]
	 * \param ftht camera rotation polar angle when leaning forward [rad]
	 * \param lpos offset vector when leaning left [<b>m</b>]
	 * \param lphi camera rotation azimuth angle when leaning left [rad]
	 * \param ltht camera rotation polar angle when leaning left [rad]
	 * \param rpos offset vector when leaning right [<b>m</b>]
	 * \param rphi camera rotation azimuth angle when leaning right [rad]
	 * \param rtht camera rotation polar angle when leaning right [rad]
	 * \note This function is an extended version of \ref SetCameraShiftRange.
	 * \note It is more versatile, because in addition to the linear camera movement vectors, it
	 *   also allows to define the camera orientation (via azimuth and polar angle relative to
	 *   default view direction). This allows to point the camera to a particular cockpit window,
	 *   instrument panel, etc.
	 * \sa SetCameraShiftRange, SetCameraRotationRange
	 */
	void SetCameraMovement (const VECTOR3 &fpos, double fphi, double ftht, const VECTOR3 &lpos, double lphi, double ltht, const VECTOR3 &rpos, double rphi, double rtht) const;
	//@}

	/// \name Instrument panel and virtual cockpit methods
	//@{
	/**
	 * \brief Triggers a redraw notification for a panel area.
	 * \param panel_id panel identifier (>=0)
	 * \param area_id area identifier (>=0)
	 * \note The redraw notification is ignored if the requested panel is not currently displayed or if the calling
	 *  vessel does not have the input focus.
	 * \sa TriggerRedrawArea
	 */
	void TriggerPanelRedrawArea (int panel_id, int area_id);

	/**
	 * \brief Triggers a redraw notification to either a 2D panel or a virtual cockpit.
	 * \param panel_id identifier for the panel to receive the redraw message
	 * \param vc_id identifier for the virtual cockpit to receive the redraw message
	 * \param area_id area identifier
	 * \note This function can be used to combine the functionality of the
	 *  TriggerPanelRedrawArea() and VCTriggerRedrawArea() methods.
	 *  Depending on the current cockpit mode, Orbiter sends the redraw request to
	 *  either ovcPanelRedrawEvent() or ovcVCRedrawEvent().
	 * \note This method can only be used if the panel and virtual cockpit areas share a
	 *  common area identifier.
	 * \note If the calling vessel doesn't have input focus (and therefore doesn't own the
	 *  cockpit display) this method has no effect.
	 * \sa TriggerPanelRedrawArea
	 */
	void TriggerRedrawArea (int panel_id, int vc_id, int area_id);
	//@}

	/// \name Mesh methods
	//@{
	/**
	 * \brief Remove all mesh definitions for the vessel.
	 * \param retain_anim flag for retaining mesh animation objects
	 * \note If \a retain_anim is \e false, all animations defined for the vessel are deleted
	 *   together with the meshes. If \e true, the animations stay behind. This is only useful
	 *   if the same meshes are subsequently added again in the same order, so that the animations
	 *   point to the appropriate meshes and mesh groups and can be re-used. If different meshes
	 *   are loaded later, the behaviour of the animations becomes undefined.
	 * \note In the future, obsolete method \ref ClearMeshes()const will be removed, and
	 *   \a retain_anim will have a default value of false.
	 */
	void ClearMeshes (bool retain_anim) const;

	/**
	 * \brief Load a mesh definition for the vessel from a file.
	 * \param meshname mesh file name
	 * \param ofs optional pointer to a displacement vector which describes the offset of the
	 *   mesh origin against the vessel origin [<b>m</b>].
	 * \return mesh index
	 * \note \a meshname defines a path to an existing mesh file. The mesh must be in
	 *   Orbiter's MSH format (see 3DModel.pdf).
	 * \note The file name (including optional directory path) is relative to Orbiter's mesh
	 *   directory (usually ".\\Meshes"). The file extension must not be specified (.msh is assumed.)
	 * \note The mesh is either appended to the end of the vessel's mesh list, or inserted at the
	 *   location of a previously deleted mesh (see VESSEL::DelMesh)
	 * \note The returned value is the mesh list index at which the mesh reference was stored. It
	 *   can be used to identify the mesh later (e.g. for animations).
	 * \note This function only creates a \e reference to a mesh, but does not directly load the
	 *   mesh from frile. The mesh is physically loaded only when it is required (whenever the
	 *   vessel moves within visual range of the observer camera).
	 * \sa AddMesh(MESHHANDLE,const VECTOR3*)const, DelMesh
	 */
	UINT AddMesh (const char *meshname, const VECTOR3 *ofs=0) const;

	/**
	 * \brief Add a pre-loaded mesh definition to the vessel.
	 * \param hMesh mesh handle
	 * \param ofs optional pointer to a displacement vector which describes the offset of the
	 *   mesh origin against the vessel origin [<b>m</b>].
	 * \return mesh index
	 * \note \a hMesh is a handle to a mesh previously loaded with \ref oapiLoadMeshGlobal.
	 * \note The global handle hMesh repersents a "mesh template". Whenever the vessel needs
	 *   to create its visual representation (when moving within visual range of the observer
	 *   camera), it creates its individual mesh as a copy of the template.
	 * \sa AddMesh(const char*,const VECTOR3*)const, oapiLoadMeshGlobal, DelMesh
	 */
	UINT AddMesh (MESHHANDLE hMesh, const VECTOR3 *ofs=0) const;

	/**
	 * \brief Insert or replace a mesh at a specific index location of the vessel's mesh list.
	 * \param meshname mesh file name
	 * \param idx mesh list index (>= 0)
	 * \param ofs optional pointer to a displacement vector which describes the offset of the
	 *   mesh origin against the vessel origin [<b>m</b>].
	 * \return mesh index
	 * \note \a meshname defines a path to an existing mesh file. The mesh must be in
	 *   Orbiter's MSH format.
	 * \note The file name (including optional directory path) is relative to Orbiter's mesh
	 *   directory (usually ".\\Meshes"). The file extension should not be specified (.msh is
	 *   assumed.)
	 * \note \a idx is a zero-based index which specifies at which point the mesh reference
	 *   is added into the vessel's mesh list. If a mesh already exists at this position, it
	 *   is overwritten. If idx > number of meshes, then the required number of (empty) entries
	 *   is generated.
	 * \note The return value is always equal to \a idx.
	 * \sa InsertMesh(MESHHANDLE,UINT,const VECTOR3*)const,
	 *   AddMesh(const char*,const VECTOR3*)const,
	 *   AddMesh(MESHHANDLE,const VECTOR3*)const
	 */
	UINT InsertMesh (const char *meshname, UINT idx, const VECTOR3 *ofs=0) const;

	/**
	 * \brief Insert or replace a mesh at a specific index location of the vessel's mesh list.
	 * \param hMesh mesh handle
	 * \param idx mesh list index (>= 0)
	 * \param ofs optional pointer to a displacement vector which describes the offset of the
	 *   mesh origin against the vessel origin [<b>m</b>].
	 * \return mesh index
	 * \note \a hMesh is a handle to a mesh previously loaded with \ref oapiLoadMeshGlobal.
	 * \note The global handle \a hMesh represents a "mesh template". Whenever the vessel
	 *   needs to create its visual representation (when moving within visual range of the
	 *   observer camera), it creates its individual mesh as a copy of the template.
	 * \note \a idx is a zero-based index which specifies at which point the mesh reference
	 *   is added into the vessel's mesh list. If a mesh already exists at this position,
	 *   it is overwritten. If idx > number of meshes, then the required number of (empty)
	 *   entries is generated.
	 * \note The return value is always equal to \a idx.
	 * \sa InsertMesh(const char*,UINT,const VECTOR3*)const,
	 *   AddMesh(const char*,const VECTOR3*)const,
	 *   AddMesh(MESHHANDLE,const VECTOR3*)const
	 */
	UINT InsertMesh (MESHHANDLE hMesh, UINT idx, const VECTOR3 *ofs=0) const;

	/**
	 * \brief Remove a mesh from the vessel's mesh list.
	 * \param idx mesh list index (>= 0)
	 * \param retain_anim flag for keeping mesh animations
	 * \return \e true on success, \e false to indicate failure (index
	 *   out of range, or mesh already deleted.)
	 * \note After a mesh has been deleted, the mesh index is no longer valid,
	 *   and should not be used any more in function calls (e.g. for animations).
	 * \note If meshes are added subsequently, they are placed in the vacant list
	 *   slots, and therefore can be assigned the indices of previously deleted
	 *   meshes.
	 * \note If you want to replace a mesh, it is easier to use the \ref InsertMesh
	 *   function instead of a combination of DelMesh and \ref AddMesh.
	 * \note By default, all animation components associated with the mesh are
	 *   deleted. This can be prevented by setting retain_anim to true. In general
	 *   this is only useful if the same mesh is subsequently loaded again into the
	 *   same mesh index slot. In all other cases, retaining the animations of
	 *   deleted meshes can lead to undefined behaviour.
	 * \sa InsertMesh, AddMesh, ClearMeshes
	 */
	bool DelMesh (UINT idx, bool retain_anim=false) const;

	/**
	 * \brief Shift the position of a mesh relative to the vessel's local coordinate
	 *   system.
	 * \param idx mesh list index (>= 0)
	 * \param ofs translation vector [<b>m</b>]
	 * \return \e true on success, \e false indicates error (index out of range).
	 * \note This function does not define an animation (i.e. gradual transition),
	 *   but resets the mesh position instantly.
	 * \sa ShiftMeshes, GetMeshOffset
	 */
	bool ShiftMesh (UINT idx, const VECTOR3 &ofs) const;

	/**
	 * \brief Shift the position of all meshes relative to the vessel's local
	 *   coordinate system.
	 * \param ofs translation vector [<b>m</b>]
	 * \note This function is useful when resetting a vessel's centre of gravity,
	 *   in combination with \ref ShiftCentreOfMass.
	 * \note A more convenient way to shift the centre of gravity is a call to
	 *   \ref ShiftCG.
	 * \sa ShiftMesh, GetMeshOffset, ShiftCentreOfMass, ShiftCG
	 */
	void ShiftMeshes (const VECTOR3 &ofs) const;

	/**
	 * \brief Returns the mesh offset in the vessel frame
	 * \param idx mesh index (0 <= idx < GetMeshCount())
	 * \param[out] ofs mesh offset [<b>m</b>]
	 * \return true if idx refers to a valid mesh index
	 * \sa AddMesh, InsertMesh, ShiftMesh, ShiftMeshes
	 */
	bool GetMeshOffset (UINT idx, VECTOR3 &ofs) const;

	/**
	 * \brief Number of meshes
	 *
	 * Returns the number of meshes currently defined for the vessel
	 * \return mesh count (>= 0)
	 */
	UINT GetMeshCount () const;

	/**
	 * \brief Obtain mesh handle for a vessel mesh
	 *
	 * Returns a handle for a vessel mesh \e instance. Mesh instances only exist
	 * while the vessel is within visual range of the camera. This function should
	 * therefore only be called between VESSEL2::clbkVisualCreated and
	 * VESSEL2::clbkVisualDestroyed, with the VISHANDLE provided by these functions.
	 * \param vis identifies the visual for which the mesh was created
	 * \param idx mesh index (0 <= idx < GetMeshCount())
	 * \return mesh handle
	 * \ng The non-graphics version of Orbiter returns always NULL, even if
	 *   a graphics client is attached. To obtain a client-specific mesh handle,
	 *   use \ref GetDevMesh .
	 * \sa GetMeshTemplate, GetMeshCount, GetDevMesh
	 */
	MESHHANDLE GetMesh (VISHANDLE vis, UINT idx) const;

	/**
	 * \brief Returns a handle for a device-specific mesh instance
	 * \param vis identifies the visual for which the mesh was created.
	 * \param idx mesh index (0 <= idx < GetMeshCount())
	 * \return device mesh handle
	 * \note For Orbiter_ng, this method returns a handle for a mesh instance managed
	 *   by the external graphics client. Graphics clients may implement their own
	 *   mesh formats, so the object pointed to by the handle is client-specific.
	 * \note For inline graphics version, the returned handle points to the same object
	 *   as the handle returned by \ref GetMesh .
	 * \sa GetMesh
	 */
	DEVMESHHANDLE GetDevMesh (VISHANDLE vis, UINT idx) const;

	/**
	 * \brief Obtain a handle for a vessel mesh template
	 *
	 * Returns the mesh handle for a pre-loaded mesh template, if available.
	 * \param idx mesh index (0 <= idx < GetMeshCount())
	 * \return mesh template handle
	 * \note Mesh templates can only be returned for meshes pre-loaded with
	 *   oapiLoadMeshGlobal(). For all other (load-on-demand) meshes this
	 *   method returns NULL.
	 * \note Mesh templates are resources shared between all vessels and should
	 *   never be modified by individual vessels. Orbiter creates individual
	 *   copies of the templates whenever a vessel is rendered.
	 */
	const MESHHANDLE GetMeshTemplate (UINT idx) const;

	/**
	 * \brief Obtain mesh file name for an on-demand mesh.
	 *
	 * Returns the mesh file name (with path relative to Orbiter's main mesh
	 * directory) for a vessel mesh that is loaded on demand (i.e. not
	 * pre-loaded).
	 * \param idx mesh index (0 <= idx < GetMeshCount())
	 * \return mesh file name, or NULL if mesh is pre-loaded
	 * \note The file names for pre-loaded meshes are not retained by Orbiter.
	 * \note Graphics clients can obtain pre-loaded mesh file names by
	 *   intercepting the oapi::GraphicsClient::clbkStoreMeshPersistent() method.
	 */
	const char *GetMeshName (UINT idx) const;

	/**
	 * \brief Make a copy of one of the vessel's mesh templates.
	 * \param idx mesh index
	 * \return handle of copied mesh
	 * \note Meshes loaded with \ref oapiLoadMeshGlobal are templates shared
	 *   between all vessel instances and should never be modified by individual
	 *   vessels. If a vessel needs to modify its meshes, it should operate on
	 *   a copy of the template.
	 */
	MESHHANDLE CopyMeshFromTemplate (UINT idx) const;

	/**
	 * \brief Returns the visibility flags for a vessel mesh.
	 * \param idx mesh index (>= 0)
	 * \return Visibility mode flags (see \ref SetMeshVisibilityMode for possible
	 *   values).
	 * \sa SetMeshVisibilityMode, meshvis
	 */
	WORD GetMeshVisibilityMode (UINT idx) const;

	/**
	 * \brief Set the visibility flags for a vessel mesh.
	 * \param idx mesh index (>= 0)
	 * \param mode visibility mode flags (see \ref meshvis)
	 * \note This method can be used to specify if a mesh is visible
	 *   in particular camera modes. Some meshes may only be visible
	 *   in external views, while others should only be visible in
	 *   cockpit views.
	 * \note Turning off the unnecessary rendering of meshes can
	 *   improve the performance of the simulator.
	 * \note \a mode can be a combination of the \ref meshvis.
	 * \note The default mode after adding a mesh is MESHVIS_EXTERNAL.
	 * \note MESHVIS_EXTPASS can't be used on its own, but as a modifier to any of the
	 *   other visibility modes. If specified, it forces the mesh to be rendered in
	 *   Orbiter's external render pass, even if it is labelled as internal (e.g.
	 *   MESHVIS_COCKPIT or MESHVIS_VC). The significance of the external render pass
	 *   is that it allows the mesh to be obscured by other objects in front of it.
	 *   However, objects in the external render pass are clipped at a camera distance
	 *   of 2.5m. Meshes that are rendered during the internal pass always cover all
	 *   other objects, and have a smaller clipping distance.
	 * \note Use the MESHVIS_EXTPASS modifier for parts of the vessel that are visible
	 *   from the cockpit, but are not close to the camera and may be obscured by other
	 *   objects. An example is the Shuttle payload bay, which can be covered by payload
	 *   objects.
	 * \sa GetMeshVisibilityMode, meshvis
	 */
	void SetMeshVisibilityMode (UINT idx, WORD mode) const;

	/**
	 * \brief Set vessel visual propery
	 * \param vis visual object handle
	 * \param prp property id
	 * \param idx index of property
	 * \param val value to be set
	 */
	void SetVisualProperty(VISHANDLE vis, VisualProp prp, int idx, const type_info &t, const void *val);

	/**
	 * \brief Affine transformation of a mesh group.
	 * \param vis vessel visual handle
	 * \param mt transformation parameter structure
	 * \return \e true on success, \e false on failure (group index out of range)
	 * \ng This function is not yet supported in orbiter_ng and
	 *   always returns \e false.
	 */
	bool MeshgroupTransform (VISHANDLE vis, const MESHGROUP_TRANSFORM &mt) const;

	/**
	 * \brief Notifies Orbiter of a change in a mesh group.
	 * \param hMesh mesh handle
	 * \param grp group index (>= 0)
	 * \param modflag type of modification (currently ignored)
	 * \return error code (0=ok)
	 * \note This method should be called if the components of a mesh group
	 *   (vertices or indices) have been modified, to allow Orbiter to propagate
	 *   the changes to the render object.
	 * \note For the built-in renderer, this registration is not strictly necessary,
	 *   because it uses the mesh directly as the render object, so any changes to
	 *   the mesh groups are applied directly.
	 * \note External graphics clients however may map the mesh data into device-specific
	 *   data structures. In that case, MeshModified tells the graphics subsystem to
	 *   synchronise its mesh data.
	 * \note MeshModified does not need to be called after applying an affine transformation
	 *   of the mesh group as a whole (\ref MeshgroupTransform), because this is performed
	 *   by assigning a transformation matrix, rather than by modifying the vertex positions
	 *   themselves.
	 * \sa oapiMeshGroup, oapiMeshGroupEx
	 */
	int MeshModified (MESHHANDLE hMesh, UINT grp, DWORD modflag);
	//@}

	/// \name Animations
	//@{
	/**
	 * \brief Logs a request for calls to \ref VESSEL2::clbkAnimate
	 * \note This function allows to implement animation sequences in combination
	 *   with the %VESSEL2::clbkAnimate callback function. After a call to
	 *   RegisterAnimation, %VESSEL2::clbkAnimate is called at each time step
	 *   whenever the vessel's visual object exists.
	 * \note Use \ref UnregisterAnimation to stop further calls to
	 *   %VESSEL2::clbkAnimate.
	 * \note Each call to RegisterAnimation increments a reference counter, while
	 *   each call to UnregisterAnimation decrements the counter. Orbiter
	 *   continues calling %VESSEL2::clbkAnimate as long as the counter is greater
	 *   than 0.
	 * \note If %VESSEL2::clbkAnimate is not overloaded by the module,
	 *   RegisterAnimation has no effect.
	 * \note The RegisterAnimation mechanism leaves the actual implementation of
	 *   the animation (transformation of mesh groups, etc.) entirely to the
	 *   module. The VESSEL::CreateAnimation / VESSEL::AddAnimationComponent
	 *   mechanism is an alternative way to define animations where the
	 *   transformations are managed by the Orbiter core.
     * \sa VESSEL2::clbkAnimate, UnregisterAnimation, CreateAnimation,
	 *   AddAnimationComponent
	 */
	void RegisterAnimation () const;

	/**
	 * \brief Unlogs an animation request.
	 * \note This stops a request for animation callback calls from a previous
	 *   \ref RegisterAnimation.
	 * \note The call to UnregisterAnimation should not be placed in the body of
	 *   \ref VESSEL2::clbkAnimate, since it may be lost if the vessel's visual
	 *   doesn't exist.
	 * \sa RegisterAnimation, VESSEL2::clbkAnimate
	 */
	void UnregisterAnimation () const;

	/**
	 * \brief Create a mesh animation object.
	 *
	 * The sequence can contain multiple components (rotations, translations,
	 * scalings of mesh groups) with a fixed temporal correlation. The
	 * animation is driven by manipulating its "state", which is a number
	 * between 0 and 1 used to linearly interpolate the animation within its
	 * range. See API User's Guide for details.
	 * \param initial_state the animation state corresponding to the
	 *   unmodified mesh
	 * \return Animation identifier
	 * \note After creating an animation, components can be added with
	 *   \ref AddAnimationComponent.
	 * \note Use SetAnimation() to manipulate the animation state.
	 * \note 0 <= \a initial_state <= 1 defines at which state the animation is stored in
	 *   the mesh file. Example: Landing gear animation between retracted
	 *   state (0) and deployed state (1). If the landing gear is retracted
	 *   in the mesh file, set initial_state to 0. If it is deployed in the
	 *   mesh file, set initial_state to 1.
	 * \sa DelAnimation, AddAnimationComponent
	 */
	UINT CreateAnimation (double initial_state) const;

	/**
	 * \brief Delete an existing mesh animation object.
	 * \param anim animation identifier, as returned by CreateAnimation
	 * \return true if animation was deleted successfully
	 * \note The animation is deleted by removing all the components
	 *   associated with it. Subsequently, any calls to SetAnimation using
	 *   this animation index will not have any effect.
	 * \note Before the animation is deleted, the mesh groups associated
	 *   with the animation are reset to their default (initial) positions.
	 *   To avoid jumps in the visual appearance of the vessel, animations
	 *   should therefore only be deleted when the animation state has
	 *   returned to the default state.
	 * \sa CreateAnimation
	 */
	bool DelAnimation (UINT anim) const;

	/**
	 * \brief Add a component (rotation, translation or scaling) to an
	 *   animation.
	 *
	 * Optionally, animations can be stacked hierachically, where
	 * transforming a parent recursively also transforms all its children
	 * (e.g. a wheel spinning while the landing gear is being retracted).
	 * \param anim animation identifier, as returned by CreateAnimation()
	 * \param state0 animation cutoff state 0 for the component
	 * \param state1 animation cutoff state 1 for the component
	 * \param trans transformation data (see notes)
	 * \param parent parent transformation
	 * \return Animation component handle
	 * \note state0 and state1 (0..1) allow to define the temporal endpoints
	 *   of the component's animation within the sequence. For example,
	 *   state0=0 and state1=1 perform the animation over the whole duration
	 *   of the animation sequence, while state0=0 and state1=0.5 perform
	 *   the animation over the first half of the total animation. This
	 *   allows to build complex animations where different components are
	 *   animated in a defined temporal sequence.
	 * \note MGROUP_TRANSFORM is the base class for mesh group transforms.
	 *   Derived classes provide support for rotations, translations and
	 *   scaling.
	 * \note To animate a complete mesh, rather than individual mesh groups,
	 *   set the "grp" pointer to NULL in the constructor of the
	 *   corresponding MGROUP_TRANSFORM operator. The "ngrp" value is then
	 *   ignored.
	 * \note To define a transformation as a child of another
	 *   transformation, set parent to the handle returned by the
	 *   \ref AddAnimationComponent call for the parent.
	 * \note Instead of adding mesh groups to an animation, it is also
	 *   possible to add a local VECTOR3 array. To do this, set "mesh" to
	 *   \ref LOCALVERTEXLIST, and set "grp" to \ref MAKEGROUPARRAY(vtxptr), where
	 *   vtxptr is the VECTOR3 array. "ngrp" is set to the number of
	 *   vertices in the array. Example:
	 *   \code
	 *     VECTOR3 vtx[2] = {_V(0,0,0), _V(1,0,-1)};
	 *     MGROUP_TRANSFORM *mt = new MGROUP_TRANSFORM (LOCALVERTEXLIST,
     *        MAKEGROUPARRAY(vtx), 2);
	 *     AddAnimationComponent (anim, 0, 1, mt);
	 *   \endcode
	 *   Transforming local vertices in this way does not have an effect on
	 *   the visual appearance of the animation, but it can be used by the
	 *   module to keep track of a transformed point during animation. The
	 *   Atlantis module uses this method to track a grappled satellite
	 *   during animation of the RMS arm.
	 * \note The ANIMATIONCOMPONENT_HANDLE is a pointer to a ANIMATIONCOMP
	 *   structure.
	 * \bug When defining a scaling transformation as a child of a parent
	 *   rotation, only homogeneous scaling is supported, i.e. scale.x =
	 *   scale.y = scale.z is required.
	 * \sa CreateAnimation, DelAnimationComponent, animationflags
	 */
	ANIMATIONCOMPONENT_HANDLE AddAnimationComponent (UINT anim, double state0, double state1,
		MGROUP_TRANSFORM *trans, ANIMATIONCOMPONENT_HANDLE parent = NULL) const;

	/**
	 * \brief Remove a component from an animation.
	 * \param anim animation identifier
	 * \param hAC animation component handle
	 * \return \e false indicates failure (\a anim out of range, or \a hAC invalid)
	 * \note If the component has children belonging to the same animation,
	 *   these will be deleted as well.
	 * \note In the current implementation, the component must not have children
	 *   belonging to other animations. Trying to delete such a component will
	 *   result in undefined behaviour.
     * \sa AddAnimationComponent
	 */
	bool DelAnimationComponent (UINT anim, ANIMATIONCOMPONENT_HANDLE hAC);

	/**
	 * \brief Set the state of an animation.
	 * \param anim animation identifier
	 * \param state animation state (0 ... 1)
	 * \return \e false indicates failure (animation identifier out of range)
	 * \note Each animation is defined by its state, with extreme points state=0 and
	 *   state=1. When setting a state between 0 and 1, Orbiter carries out the
	 *   appropriate transformations to advance the animation to that state. It is
	 *   the responsibility of the code developer to call SetAnimation in such a way
	 *   as to provide a smooth movement of the animated parts.
	 * \sa GetAnimation
	 */
	bool SetAnimation (UINT anim, double state) const;

	/**
	 * \brief Return the current state of an animation
	 * \param anim animation identifier
	 * \return animation state (0 ... 1)
	 * \sa SetAnimation
	 */
	double GetAnimation (UINT anim) const;

	/**
	 * \brief Returns a pointer to the array of animations defined by the vessel
	 * \param[out] anim pointer list of vessel animations
	 * \return list length (number of animations)
	 * \note The pointer can become invalid whenever the vessel adds or deletes
	 *   animations. It should therefore not be stored, but queried on demand.
	 */
	UINT GetAnimPtr (ANIMATION **anim) const;
	//@}


	/**
	 * \name Supervessel functions
     * If the vessel is a component of a docked superstructure, this
	 * set of functions can retrieve information about the
	 * superstructure.
	 */
	//@{
	/**
	 * \brief Returns a handle to the vessel's superstructure
	 *   or NULL if the vessel is not part of a docking assembly
	 */
	SUPERVESSELHANDLE GetSupervessel () const;

	/**
	 * \brief Returns the supervessel's centre of gravity in
	 *   the local vessel frame.
	 * \note If the vessel is not part of a superstructure, the
	 *   return value is the vessel's CG, i.e. (0,0,0)
	 */
	VECTOR3 GetSupervesselCG () const;
	//@}


	/// \name Recording/playback functions
	//@{
	/**
	 * \brief Flag for active recording session.
	 * \return \e true if flight recording is active, \e false otherwise.
	 * \sa Playback, RecordEvent
	 */
	bool Recording () const;

	/**
	 * \brief Flag for active playback session.
	 * \return \e true if the current session is a playback of a recorded flight,
	 *   \e false otherwise.
	 * \sa Recording
	 */
	bool Playback () const;

	/**
	 * \brief Writes a custom tag to the vessel's articulation data stream during
	 *   a running recording session.
	 * \param event_type event tag label
	 * \param event event string
	 * \note This function can be used to record custom vessel events (e.g.
	 *   animations) to the articulation stream (.atc) of a vessel record.
	 * \note The function does nothing if no recording is active, so it is not
	 *   necessary to check for a running recording before invoking RecordEvent.
	 * \note To read the recorded articulation tags during the playback of a
	 *   recorded session, overload the VESSEL2::clbkPlaybackEvent callback
	 *   function.
	 * \sa Recording, VESSEL2::clbkPlaybackEvent
	 */
	void RecordEvent (const char *event_type, const char *event) const;
	//@}


	/// \name Coordinate transformations
	//@{
	/**
	 * \brief Register a shift in the centre of mass after a structural change
	 *   (e.g. stage separation).
	 * \param shift centre of mass displacement vector [<b>m</b>]
	 * \note This function should be called after a vessel has undergone a
	 *   structural change which resulted in a shift of the vessel's centre of
	 *   gravity (CG). Note that in Orbiter, a vessel's CG coincides by definition
	 *   always with the origin (0,0,0) of its local reference frame.
	 *   Therefore, in order to achieve a shift of the CG by a vector <b>S</b>,
	 *   this function shifts the vessel's global position by +<b>S</b>.
	 *   This allows to shift the meshes by -<b>S</b>, thus retaining their
	 *   global position.
	 *   The net result is unchanged mesh positions in the global frame, but a
	 *   shift of the local frame of reference (and thus CG) of +<b>S</b>.
	 * \note The camera position is shifted to take into account the new CG. An
	 *   external camera view performs a smooth transition.
	 * \note The shift of meshes (and any other reference positions defined in
	 *   the local vessel frame, such as docking ports, etc.) is not performed
	 *   by this function but must be executed separately.
	 *   A more convenient way to implement a transition of the centre of
	 *   mass is the function \ref ShiftCG, which automatically takes care of
	 *   translating meshes, docking ports, etc.
     * \sa ShiftCG
	 */
	void ShiftCentreOfMass (const VECTOR3 &shift);

	/**
	 * \brief Shift the centre of gravity of a vessel.
	 * \param shift centre of gravity displacement vector [<b>m</b>]
	 * \note This function should be called after a vessel has undergone a
	 *   structural change which resulted in a shift of the vessel's centre of
	 *   gravity (CG). Note that in Orbiter, a vessel's CG coincides by definition
	 *   always with the origin (0,0,0) of its local reference frame.
	 *   Therefore, in order to achieve a shift of the CG by \a shift,
	 *   this function performs the following actions:
	 *   - Calls \ref ShiftCentreOfMass (+shift) to align the vessel's global
	 *     position with the new CG position.
	 *   - Calls \ref ShiftMeshes (-shift) to compensate the mesh positions
	 *   - Applies equivalent shift to all
	 *     - thruster positions,
	 *     - docking ports,
	 *     - attachment points,
	 *     - explicitly defined light source positions,
	 *     - and to the cockpit camera position
	 *   .
	 *   The net effect is a shift of the vessel frame of reference (and thus the
	 *   CG by +shift, while the mesh positions remain in place in the global
	 *   frame.
	 * \sa ShiftCentreOfMass, ShiftMeshes
	 */
	void ShiftCG (const VECTOR3 &shift);

	/**
	 * \brief Returns the centre of gravity of the superstructure to which the
	 *   vessel belongs, if applicable.
	 * \param cg superstructure centre of gravity [<b>m</b>]
	 * \return \e true if the vessel is part of a superstructure, \e false
	 *   otherwise.
	 * \note The returned vector is the position of the superstructure centre
	 *   of gravity, in coordinates of the local vessel frame.
	 * \note If the vessel is not part of a superstructure, cg returns (0,0,0).
	 */
	bool GetSuperstructureCG (VECTOR3 &cg) const;

	/**
	 * \brief Returns the current rotation matrix for transformations
	 *   from the vessel's local frame of reference to the global frame.
	 * \param R rotation matrix
	 * \note To transform a point rlocal from local vessel coordinates to a
	 *   global point rglobal, the following formula is used: \n
	 *   rglobal = R rlocal + pvessel, \n
	 *   where pvessel is the vessel's global position.
	 * \note This transformation can be directly performed by a call to
	 *   Local2Global.
	 * \sa Local2Global, SetRotationMatrix, GlobalRot
	 */
	void GetRotationMatrix (MATRIX3 &R) const;

	/**
	 * \brief Applies a rotation by replacing the vessel's local to global
	 *   rotation matrix.
	 * \param R rotation matrix
	 * \note The rotation matrix maps from the orientation of the vessel's
	 *   local frame of reference to the orientation of the global frame
	 *   (ecliptic at 2000.0).
	 * \note The user is responsible for providing a valid rotation matrix.
	 *   The matrix must be orthogonal and normalised: the norms of all
	 *   column vectors of R must be 1, and scalar products between any
	 *   column vectors must be 0.
	 * \sa GetRotationMatrix, Local2Global
	 */
	void SetRotationMatrix (const MATRIX3 &R) const;

	/**
	 * \brief Performs a rotation of a direction from the local vessel
	 *   frame to the global frame.
	 * \param [in] rloc point in local vessel coordinates
	 * \param [out] rglob rotated point
	 * \note This function is equivalent to multiplying rloc with the
	 *   rotation matrix returned by \ref GetRotationMatrix.
	 * \note Should be used to transform \e directions. To transform
	 *   \e points, use \ref Local2Global, which additionally adds the
	 *   vessel's global position to the rotated point.
	 * \sa GetRotationMatrix, Local2Global
	 */
	void GlobalRot (const VECTOR3 &rloc, VECTOR3 &rglob) const;

	/**
	 * \brief Performs a rotation from the local vessel frame to the
	 *   current local horizon frame.
	 * \param [in] rloc vector in local vessel coordinates
	 * \param [out] rhorizon vector in local horizon coordinates
	 * \note The local horizon frame is defined as follows:
	 *   - y is "up" direction (planet centre to vessel centre)
	 *   - z is "north" direction
	 *   - x is "east" direction
     * \sa HorizonInvRot, GlobalRot, GetRotationMatrix, SetRotationMatrix
	 */
	void HorizonRot (const VECTOR3 &rloc, VECTOR3 &rhorizon) const;

	/**
	 * \brief Performs a rotation of a direction from the current local
	 *   horizon frame to the local vessel frame.
	 * \param [in] rhorizon vector in local horizon coordinates
	 * \param [out] rloc vector in local vessel coordinates
	 * \note This function performs the inverse operation of \ref
	 *   HorizonRot.
	 * \sa HorizonRot, GlobalRot, GetRotationMatrix, SetRotationMatrix
	 */
	void HorizonInvRot (const VECTOR3 &rhorizon, VECTOR3 &rloc) const;

	/**
	 * \brief Performs a transformation from local vessel coordinates to
	 *   global coordinates.
	 * \param [in] local point in local vessel coordinates [<b>m</b>]
	 * \param [out] global transformed point in global coordinates [<b>m</b>]
	 * \note This function maps a point from the vessel's local coordinate
	 *   system (centered at the vessel CG) into the global ecliptic
	 *   system (centered at the solar system barycentre).
	 * \note The transform has the form
	 *   \f[ \vec{p}_g = \mathsf{R}_v \vec{p}_l + \vec{p}_v \f]
	 *   where R<sub>v</sub> is the vessel's global rotation matrix
	 *   (as given by \ref GetRotationMatrix), and \f$\vec{p}_v\f$
	 *   is the vessel position in the global frame.
	 * \sa GetRotationMatrix, Global2Local
	 */
	void Local2Global (const VECTOR3 &local, VECTOR3 &global) const;

	/**
	 * \brief Performs a transformation from global to local vessel
	 *   coordinates.
	 * \param [in] global point in global coordinates [<b>m</b>]
	 * \param [out] local transformed point in local vessel coordinates [<b>m</b>]
	 * \note This is the inverse transform of \ref Local2Global. It maps
	 *   a point from global ecliptic coordinates into the vessel's local
	 *   frame.
	 * \note The transformation has the form
	 *   \f[ \vec{p}_l = \mathsf{R}_v^{-1} (\vec{p}_g - \vec{p}_v) \f]
	 *   where R<sub>v</sub> is the vessel's global rotation matrix
	 *   (as given by \ref GetRotationMatrix), and \f$\vec{p}_v\f$ is the
	 *   vessel position in the global frame.
	 * \sa GetRotationMatrix, Local2Global
	 */
	void Global2Local (const VECTOR3 &global, VECTOR3 &local) const;

	/**
	 * \brief Performs a transformation from local vessel coordinates
	 *   to the ecliptic frame centered at the vessel's reference body.
	 * \param [in] local point in local vessel coordinates [<b>m</b>]
	 * \param [out] rel transformed point in reference body-relative
	 *   ecliptic coordinates [<b>m</b>].
	 * \note This function maps a point from the vessel's local coordinate
	 *   system into an ecliptic system centered at the centre of mass of
	 *   the vessel's <i>gravity reference object</i> (the celestial body
	 *   that is currently being orbited).
	 * \note A handle to the reference object can be obtained via
	 *   \ref GetGravityRef. The reference object may change if the vessel
	 *   enters a different object's sphere of influence.
	 * \note The transformation has the form
	 *   \f[ \vec{p}_r = \mathsf{R}_v \vec{p}_l + \vec{p}_v - \vec{p}_\mathrm{ref} \f]
	 *   where R<sub>v</sub> is the vessel's global rotation matrix (as given
	 *   by \ref GetRotationMatrix), \f$\vec{p}_v\f$ is the vessel's global
	 *   position, and \f$\vec{p}_\mathrm{ref}\f$ is the reference body's global
	 *   position.
	 * \sa GetRotationMatrix, Global2Local, Local2Global, GetGravityRef
	 */
	void Local2Rel (const VECTOR3 &local, VECTOR3 &rel) const;
	//@}


	/**
	 * \name Docking port management
	 * See also: \ref docking_management
	 */
	//@{
	/**
	 * \brief Create a new docking port.
	 * \param pos dock reference position in vessel coordinates [<b>m</b>]
	 * \param dir approach direction in vessel coordinates.
	 * \param rot longitudinal rotation alignment vector
	 * \return Handle for the new docking port.
	 * \note The \a dir and \a rot vectors should be normalised to length 1.
	 * \note The \a rot vector should be perpendicular to the \a dir vector.
	 * \note When two vessels connect at their docking ports, the relative
	 *   orientation of the vessels is defined such that their respective
	 *   approach direction vectors (dir) are anti-parallel, and their
	 *   longitudinal alignment vectors (rot) are parallel.
	 * \sa DelDock, ClearDockDefinitions, GetDockParams, SetDockParams, DockCount,
	 *   GetDockHandle, GetDockStatus, Dock, Undock
	 */
	DOCKHANDLE CreateDock (const VECTOR3 &pos, const VECTOR3 &dir, const VECTOR3 &rot) const;

	/**
	 * \brief Delete a previously defined docking port.
	 * \param hDock dock handle
	 * \return \e false indicates failure (invalid dock handle)
	 * \note Any object docked at the port will be undocked before the
	 *   docking port is deleted.
	 * \sa CreateDock, ClearDockDefinitions, DockCount, Dock, Undock
	 */
	bool DelDock (DOCKHANDLE hDock) const;

	/**
	 * \brief Delete all docking ports defined for the vessel.
	 * \note Any docked objects will be undocked before deleting the
	 *   docking ports.
	 * \sa CreateDock, DelDock, DockCount, Dock, Undock
	 */
	void ClearDockDefinitions () const;

	/**
	 * \brief Set the parameters for the vessel's primary docking port (port 0),
	 *   or create a new dock if required.
	 * \param pos dock reference position in vessel coordinates [<b>m</b>]
	 * \param dir approach direction in vessel coordinates
	 * \param rot longitudinal rotation alignment vector
	 * \note This function creates a new docking port if none was previously
	 *   defined.
	 * \note See \ref CreateDock for additional notes on the parameters.
	 * \sa SetDockParams(DOCKHANDLE,const VECTOR3&,const VECTOR3&,const VECTOR3&)const,
	 *   GetDockParams, CreateDock, DelDock, DockCount, Dock, Undock
	 */
	void SetDockParams (const VECTOR3 &pos, const VECTOR3 &dir, const VECTOR3 &rot) const;

	/**
	 * \brief Reset the parameters for a vessel docking port.
	 * \param hDock dock handle
	 * \param pos new dock reference position [<b>m</b>]
	 * \param dir new approach direction
	 * \param rot new longitudinal rotation alignment vector
	 * \note This function should not be called while the docking
	 *   port is engaged.
	 * \note The \a dir and \a rot direction vectors should be normalised
	 *   to length 1.
	 * \sa SetDockParams(const VECTOR3&,const VECTOR3&,const VECTOR3&)const,
	 *   GetDockParams, CreateDock, DelDock, DockCount, Dock, Undock
	 */
	void SetDockParams (DOCKHANDLE hDock, const VECTOR3 &pos, const VECTOR3 &dir, const VECTOR3 &rot) const;

	/**
	 * \brief Returns the paramters of a docking port.
	 * \param [in] hDock dock handle
	 * \param [out] pos dock reference position [<b>m</b>]
	 * \param [out] dir approach direction
	 * \param [out] rot longitudinal rotation alignment vector
	 * \sa CreateDock, SetDockParams(const VECTOR3&,const VECTOR3&,const VECTOR3&)const,
	 *   SetDockParams(DOCKHANDLE,const VECTOR3&,const VECTOR3&,const VECTOR3&)const
	 */
	void GetDockParams (DOCKHANDLE hDock, VECTOR3 &pos, VECTOR3 &dir, VECTOR3 &rot) const;

	/**
	 * \brief Returns the number of docking ports defined for the vessel.
	 * \return Number of docking ports.
	 * \sa CreateDock, DelDock, ClearDockDefinitions
	 */
	UINT DockCount () const;

	/**
	 * \brief Returns a handle to a docking port.
	 * \param n docking port index (>= 0)
	 * \return Dock handle, or NULL if index is out of range.
	 * \sa CreateDock, DelDock, SetDockParams, GetDockParams, GetDockStatus, oapiGetDockHandle
	 */
	DOCKHANDLE GetDockHandle (UINT n) const;

	/**
	 * \brief Returns a handle to a docked vessel.
	 * \param hDock dock handle
	 * \return Handle of the vessel docked at the specified port, or
	 *   NULL if the docking port is not engaged.
	 * \sa CreateDock, GetDockHandle, Dock, Undock, oapiGetDockStatus
	 */
	OBJHANDLE GetDockStatus (DOCKHANDLE hDock) const;

	/**
	 * \brief Returns a status flag for a docking port.
	 * \param port docking port index (>= 0)
	 * \return Docking status (0=free, 1=engaged)
	 * \note This method has the same functionality as 
	 *   \code (GetDockStatus (GetDockHandle(port)) ? 1:0) \endcode
	 * \sa GetDockStatus, GetDockHandle
	 */
	UINT DockingStatus (UINT port) const;

	/**
	 * \brief Moves a docking port while vessel is docked. 
	 * \param hDock dock handle
	 * \param pos new dock reference position [<b>m</b>]
	 * \param dir new approach direction
	 * \param rot new longitudinal rotation alignment vector
	 * \note If no vessel is docked then does the same as SetDockParams
	 * \sa GetDockStatus, GetDockHandle
	 */
	void MoveDock(DOCKHANDLE hDock, const VECTOR3& pos, const VECTOR3& dir, const VECTOR3& rot);

	/**
	 * \brief Get closest free docking port from an other vessel
	 * \param hDock dock handle
	 * \return Docking port handle, NULL if hDock is already occupied or nothing else founds.
	 * \sa GetDockStatus, GetDockHandle
	 */
	DOCKHANDLE GetProxyDock(DOCKHANDLE hDock) const;

	/**
	 * \brief Get index of specified docking port
	 * \param hDock dock handle
	 * \return Dock index or -1 if hDock doesn't belong to a vessel.
	 */
	int GetDockIndex(DOCKHANDLE hDock) const;

	/**
	 * \brief Get target docking port alignment relative to hDock in local vessel coords
	 * \param hDock dock handle
	 * \param hTgt Target dock handle
	 * \param ref Target position, set to NULL if not needed.
	 * \param dir Target direction, set to NULL if not needed.
	 * \param rot Target rotation, set to NULL if not needed.
	 * \param vel Relative velocity between ports ECL frame, set to NULL if not needed.
	 * \sa GetDockStatus, GetDockHandle
	 */
	bool GetTargetDockAlignment(DOCKHANDLE hDock, DOCKHANDLE hTgt, VECTOR3* ref, VECTOR3* dir, VECTOR3* rot, VECTOR3* vel = nullptr) const;

	/**
	 * \brief Dock to another vessel.
	 * \param target handle of docking target vessel
	 * \param n docking port index on vessel (>= 0)
	 * \param tgtn docking port index on target (>= 0)
	 * \param mode attachment mode (see notes)
	 * \return
	 *   - 0=ok
	 *   - 1=docking port \a n on the vessel already in use
	 *   - 2=docking port \a tgtn on the target already in use
	 *   - 3=target vessel already part of the vessel's superstructure
	 * \note This function is useful for designing scenario editors and during
	 *   startup configuration, but its use should be avoided during a
	 *   running simulation, because it can lead to unphysical situations:
	 *   it allows to dock two vessels regardless of their current
	 *   separation, by teleporting one of them to the location of the other.
	 * \note During a simulation, Orbiter will dock two vessels automatically
	 *   when their docking ports are brought into close proximity.
	 * \note The mode parameter determines how the vessels are connected. The
	 *   following settings are supported:
     *   - 0: calculate the linear and angular moments of the superstructure
	 *     from the moments of the docking components. This should only be used
	 *     if the two vessels are already in close proximity and aligned for
	 *     docking.
	 *   - 1: Keep this in place, and teleport the target vessel for docking
	 *   - 2: Keep the target in place, and teleport this for docking.
	 *   - 3: Softdock. Keep the target in place and match this vessel's docking port with
	 *	 target port alignment (i.e Ref, Dir and Rot gets matched to target). Add-on side code must
	 *	 bring the vessel to alignment and hard-dock using MoveDock.
	 * \sa Undock, GetDockHandle, GetDockStatus, DockCount, MoveDock
	 */
	int Dock (OBJHANDLE target, UINT n, UINT tgtn, UINT mode) const;
	int Dock (DOCKHANDLE hSrc, DOCKHANDLE hTgt, DWORD mode) const;

	/**
	 * \brief Release a docked vessel from a docking port.
	 * \param n docking port index (>= 0 or ALLDOCKS)
	 * \param exclude optional handle of a vessel to be excluded from undocking
	 * \return \e true if at least one vessel was released from a port.
	 * \note If \a n is set to ALLDOCKS, all docking ports are released
	 *   simultaneously.
	 * \note If \a exclude is nonzero, this vessel will not be undocked. This
	 *   is useful for implementing remote undocking in combination with
	 *   ALLDOCKS.
	 * \sa Dock, GetDockHandle, GetDockStatus, DockCount
	 */
	bool Undock (UINT n, const OBJHANDLE exclude = 0) const;

	/**
	 * \brief Set the docking approach mode for all docking ports.
	 * \param mode docking mode (see notes)
	 * \note Defines the method Orbiter applies to establish a docking
	 *   connection between two vessels. Supported values are:
	 *   - 0: use legacy (2006) method: snap to dock as soon as two docking
	 *     ports are within 0.5m and closing.
	 *   - 1 (default): use new (2010) method: snap to dock as soon as one docking
	 *     reference point passes through the reference plane of the other
	 *     dock within 0.5m.
	 * \note If the two docking vessels use different docking modes, the
	 *   method used is unpredictable, depending on which vessel initiates
	 *   the docking event.
	 */
	void SetDockMode (int mode) const;
	//@}


	/**
	 * \name Passive attachment management
	 * See also: \ref attachment_management
	 */
	//@{
	/**
	 * \brief Define a new attachment point for a vessel.
	 * \param toparent If \e true, the attachment can be used to connect to
	 *   a parent (i.e. the vessel acts as a child). Otherwise, attachment is
	 *   used to connect to a child (i.e. vessel acts as parent)
	 * \param pos attachment point position in vessel coordinates [<b>m</b>]
	 * \param dir attachment direction in vessel coordinates
	 * \param rot longitudinal alignment vector in vessel coordinates
	 * \param id compatibility identifier
	 * \param loose If \e true, allow loose connections (see notes)
	 * \return Handle to new attachment point
	 * \note A vessel can define multiple parent and child attachment points,
	 *   and can subsequently have multiple children attached, but it can
	 *   only be attached to a single parent at any one time.
	 * \note The \a dir and \a rot vectors should both be normalised to length
	 *   1, and they should be orthogonal.
	 * \note The identifier string can contain up to 8 characters. It can be
	 *   used to define compatibility between attachment points.
	 * \note If the attachment point is defined as \a loose, then the relative
	 *   orientation between the two attached objects is frozen to the
	 *   orientation between them at the time the connection was established.
	 *   Otherwise, the two objects snap to the orientation defined by their
	 *   \a dir vectors.
	 * \sa SetAttachmentParams, GetAttachmentParams, GetAttachmentId,
	 *   GetAttachmentStatus, AttachmentCount, GetAttachmentIndex, GetAttachmentHandle,
	 *   AttachChild, DetachChild
	 */
	ATTACHMENTHANDLE CreateAttachment (bool toparent, const VECTOR3 &pos, const VECTOR3 &dir, const VECTOR3 &rot, const char *id, bool loose = false) const;

	/**
	 * \brief Delete an attachment point.
	 * \param attachment attachment handle
	 * \return \e false indicates failure (invalid attachment handle)
	 * \note The attachment handle can refer to either a child or parent
	 *   attachment point.
	 * \note Any object attached to this point will be released.
	 * \note After this function returns successfully, the attachment handle
	 *   is no longer valid.
	 * \sa CreateAttachment
	 */
	bool DelAttachment (ATTACHMENTHANDLE attachment) const;

	/**
	 * \brief Delete all attachment points defined for the vessel.
	 * \note Any attached parent or child vessels will be released.
	 * \note After this function returns, all previously defined attachment
	 *   handles will no longer be valid.
	 */
	void ClearAttachments () const;

	/**
	 * \brief Reset attachment position and orientation for an existing
	 *   attachment point.
	 * \param attachment attachment handle
	 * \param pos new attachment point position in vessel coordinates [<b>m</b>]
	 * \param dir new attachment direction in vessel coordinates
	 * \param rot new longitudinal alignment vector in vessel coordinates
	 * \note If the parameters of an attachment point are changed while a vessel
	 *   is attached to that point, the attached vessel will be shifted to the
	 *   new position automatically.
	 * \note The \a dir and \a rot vectors should both be normalised to length 1,
	 *   and they should be orthogonal.
	 * \sa CreateAttachment, GetAttachmentParams, GetAttachmentId,
	 *   GetAttachmentStatus, AttachmentCount, GetAttachmentIndex, GetAttachmentHandle,
	 *   AttachChild, DetachChild
	 */
	void SetAttachmentParams (ATTACHMENTHANDLE attachment, const VECTOR3 &pos, const VECTOR3 &dir, const VECTOR3 &rot) const;

	/**
	 * \brief Retrieve the parameters of an attachment point.
	 * \param [in] attachment attachment handle
	 * \param [out] pos attachment point position in vessel coordinates [<b>m</b>]
	 * \param [out] dir attachment direction in vessel coordinates
	 * \param [out] rot longitudinal alignment vector in vessel coordinates
	 * \sa CreateAttachment, SetAttachmentParams, GetAttachmentId,
	 *   GetAttachmentStatus, AttachmentCount, GetAttachmentIndex, GetAttachmentHandle,
	 *   AttachChild, DetachChild
	 */
	void GetAttachmentParams (ATTACHMENTHANDLE attachment, VECTOR3 &pos, VECTOR3 &dir, VECTOR3 &rot) const;

	/**
	 * \brief Retrieve attachment identifier string.
	 * \param attachment attachment handle
	 * \return Pointer to attachment string [8 characters]
	 * \sa CreateAttachment, SetAttachmentParams, GetAttachmentParams,
	 *   GetAttachmentStatus, AttachmentCount, GetAttachmentIndex, GetAttachmentHandle,
	 *   AttachChild, DetachChild
	 */
	const char *GetAttachmentId (ATTACHMENTHANDLE attachment) const;

	/**
	 * \brief Return the current status of an attachment point.
	 * \param attachment attachment handle
	 * \return Handle of tha attached vessel, or NULL if no vessel is attached
	 *   to this point.
	 * \sa CreateAttachment, SetAttachmentParams, GetAttachmentParams,
	 *   GetAttachmentId, AttachmentCount, GetAttachmentIndex, GetAttachmentHandle,
	 *   AttachChild, DetachChild
	 */
	OBJHANDLE GetAttachmentStatus (ATTACHMENTHANDLE attachment) const;

	/**
	 * \brief Return the number of child or parent attachment points defined
	 *   for the vessel.
	 * \param toparent If \e true, return the number of attachment points to
	 *   parents. Otherwise, return the number of attachment points to children.
	 * \return Number of defined attachment points to connect to parents or to
	 *   children.
	 * \sa CreateAttachment, SetAttachmentParams, GetAttachmentParams,
	 *   GetAttachmentId, GetAttachmentStatus, GetAttachmentIndex, GetAttachmentHandle,
	 *   AttachChild, DetachChild
	 */
	DWORD AttachmentCount (bool toparent) const;

	/**
	 * \brief Return the list index of the vessel's attachment point defined
	 *   by its handle.
	 * \param attachment attachment handle
	 * \return List index (>= 0)
	 * \note A vessel defines separate lists for child and parent attachment
	 *   points. Therefore two different attachment points may return the same
	 *   index.
	 * \note The index for a given attachment point can change when the vessel
	 *   deletes any of its attachments. The returned index should therefore be
	 *   used only within the current frame.
	 * \sa CreateAttachment, SetAttachmentParams, GetAttachmentParams,
	 *   GetAttachmentId, GetAttachmentStatus, AttachmentCount, GetAttachmentHandle,
	 *   AttachChild, DetachChild
	 */
	DWORD GetAttachmentIndex (ATTACHMENTHANDLE attachment) const;

	/**
	 * \brief Return the handle of an attachment point identified by its list
	 *   index.
	 * \param toparent If \e true, return a handle for an attachment point to
	 *   a parent. Otherwise, return a handle for an attachment point to a child.
	 * \param i attachment index (>= 0)
	 * \return Attachment handle, or NULL if index out of range.
	 * \sa CreateAttachment, SetAttachmentParams, GetAttachmentParams,
	 *   GetAttachmentId, GetAttachmentStatus, AttachmentCount, GetAttachmentIndex,
	 *   AttachChild, DetachChild
	 */
	ATTACHMENTHANDLE GetAttachmentHandle (bool toparent, DWORD i) const;

	/**
	 * \brief Return the handle of a root object in a attachment hierarchy. (i.e. vessel that has not parent)
	 * \return Vessel handle. Returns vessel's own handle "this" if no parent exists.
	 */
	OBJHANDLE GetAttachmentRoot() const;

	/**
	 * \brief Attach a child vessel to an attachment point.
	 * \param child handle of child vessel to be attached.
	 * \param attachment attachment point to which the child will be attached.
	 * \param child_attachment attachment point on the child to which we want
	 *   to attach.
	 * \return \e true indicates success, \e false indicates failure (child
	 *   refuses attachment)
	 * \note The \a attachment handle must refer to an attachment "to child"
	 *   (i.e. created with toparent=false); the \a child_attachment handle
	 *   must refer to an attachment "to parent" on the child object (i.e.
	 *   created with toparent=true). It is not possible to connect two parent
	 *   or two child attachment points.
	 * \note A child can only be connected to a single parent at any one time.
	 *   If the child is already connected to a parent, the previous parent
	 *   connection is severed.
	 * \note The child may check the parent attachment's id string and,
	 *   depending on the value, refuse to connect. In that case, the function
	 *   returns \e false.
	 * \sa CreateAttachment, SetAttachmentParams, GetAttachmentParams,
	 *   GetAttachmentId, GetAttachmentStatus, AttachmentCount, GetAttachmentIndex,
	 *   GetAttachmentHandle, DetachChild
	 */
	bool AttachChild (OBJHANDLE child, ATTACHMENTHANDLE attachment, ATTACHMENTHANDLE child_attachment) const;

	/**
	 * \brief Break an existing attachment to a child.
	 * \param attachment attachment handle
	 * \param vel separation velocity [m/s]
	 * \return \e true when detachment is successful, \e false if no child was
	 *   attached, or if child refuses to detach.
	 * \sa CreateAttachment, SetAttachmentParams, GetAttachmentParams,
	 *   GetAttachmentId, GetAttachmentStatus, AttachmentCount, GetAttachmentIndex,
	 *   GetAttachmentHandle, AttachChild
	 */
	bool DetachChild (ATTACHMENTHANDLE attachment, double vel = 0.0) const;
	//@}


	/// \name Exhaust and entry render functions
	//@{
	/**
	 * \brief Add an exhaust render definition for a thruster.
	 * \param th thruster handle
	 * \param lscale exhaust flame length [m]
	 * \param wscale exhaust flame width [m]
	 * \param tex texture handle for custom exhaust flames
	 * \return Exhaust identifier
	 * \note Thrusters defined with \ref CreateThruster do not by default render
	 *   exhaust effects, until an exhaust definition has been specified with
	 *   AddExhaust.
     * \note The size of the exhaust flame is automatically scaled by the thrust
	 *   level.
	 * \note This version retrieves exhaust reference position and direction
	 *   directly from the thruster setting, and will therefore automatically
	 *   reflect any changes caused by \ref SetThrusterRef and \ref SetThrusterDir.
	 * \note To use a custom exhaust texture, set \a tex to a surface handle returned
	 *   by \ref oapiRegisterExhaustTexture. If \a tex == 0, the default texture
	 *   is used.
	 * \sa AddExhaust(THRUSTER_HANDLE,double,double,double,SURFHANDLE)const,
	 *   AddExhaust(THRUSTER_HANDLE,double,double,const VECTOR3&,const VECTOR3&,SURFHANDLE)const,
	 *   DelExhaust,CreateThruster,SetThrusterRef,SetThrusterDir,SetThrusterLevel,
	 *   oapiRegisterExhaustTexture
	 */
	UINT AddExhaust (THRUSTER_HANDLE th, double lscale, double wscale, SURFHANDLE tex = 0) const;

	/**
	 * \brief Add an exhaust render definition for a thruster with additional
	 *   offset.
	 * \param th thruster handle
	 * \param lscale exhaust flame length [m]
	 * \param wscale exhaust flame width [m]
	 * \param lofs longitudinal offset [m]
	 * \param tex texture handle for custom exhaust flames
	 * \return Exhaust identifier
	 * \note This method allows to add an additional longitudinal offset between
	 *   thruster position and exhaust.
	 * \sa AddExhaust(THRUSTER_HANDLE,double,double,SURFHANDLE)const,
	 *   AddExhaust(THRUSTER_HANDLE,double,double,const VECTOR3&,const VECTOR3&,SURFHANDLE)const,
	 *   DelExhaust,CreateThruster,SetThrusterRef,SetThrusterDir,SetThrusterLevel,
	 *   oapiRegisterExhaustTexture
	 */
	UINT AddExhaust (THRUSTER_HANDLE th, double lscale, double wscale, double lofs, SURFHANDLE tex = 0) const;

	/**
	 * \brief Add an exhaust render definition for a thruster with explicit
	 *   reference position and direction.
	 * \param th thruster handle
	 * \param lscale exhaust flame length [m]
	 * \param wscale exhaust flame width [m]
	 * \param pos reference position in vessel coordinates [<b>m</b>]
	 * \param dir exhaust direction in vessel coordinates
	 * \param tex texture handle for custom exhaust flames
	 * \note This version uses the explicitly provided reference position
	 *   and direction, rather than using the thruster parameters.
	 * \note This allows multiple exhaust render definitions to refer to a
	 *   single thruster definition, e.g. where multiple thrusters have been
	 *   combined into a single "logical" thruster definition. This
	 *   technique can be used to simplify the description of thruster
	 *   groups which are always addressed synchronously.
	 * \note The exhaust direction should be opposite to the thrust
	 *   direction of the thruster it refers to.
	 * \note Exhaust positions and directions are fixed in this version, so
	 *   they will not react to changes caused by \ref SetThrusterRef and
	 *   \ref SetThrusterDir.
	 * \note To use a custom exhaust texture, set \a tex to a surface handle
	 *   returned by \ref oapiRegisterExhaustTexture. If \a tex == 0, the
	 *   default texture is used.
	 * \sa AddExhaust(THRUSTER_HANDLE,double,double,SURFHANDLE)const,
	 *   AddExhaust(THRUSTER_HANDLE,double,double,double,SURFHANDLE)const,
	 *   DelExhaust,CreateThruster,SetThrusterRef,SetThrusterDir,SetThrusterLevel,
	 *   oapiRegisterExhaustTexture
	 */
	UINT AddExhaust (THRUSTER_HANDLE th, double lscale, double wscale, const VECTOR3 &pos, const VECTOR3 &dir, SURFHANDLE tex = 0) const;

	/**
	 * \brief Add an exhaust render definition defined by a parameter structure.
	 * \param spec exhaust specification
	 * \return Exhaust identifier
	 * \note This method is more versatile than the other AddExhaust versions.
	 *  It allows dynamic custom control of exhaust level, position and direction,
	 *  and it can be defined independently of thrusters.
	 * \note To let the exhaust appearance be automatically controlled by a thruster,
	 *  set spec->th to the thruster handle. The fields spec->level, spec->lpos and
	 *  spec->ldir can then be set to NULL, to indicate that they should be linked to
	 *  the thruster parameters.
	 * \note If spec->th == NULL (thruster-independent exhaust definition), then
	 *  spec->level, spec->lpos and spec->ldir must not be NULL. They must point to
	 *  variables that continuously define the level, position and negative direction of the
	 *  exhaust cone. The variables themselves must persist during the lifetime of the
	 *  exhaust definition.
	 * \note An exeption is the definition of a constant parameter. For example, if the
	 *  exhaust position is to be set to a fixed position, set the spec->flags
	 *  field to EXHAUST_CONSTANTPOS. In this case, the value pointed to by spec->lpos 
	 *  is copied by Orbiter, and the variable can be discarded after the call to
	 *  AddExhaust. In a similar fashion, the bit flags EXHAUST_CONSTANTDIR and
	 *  EXHAUST_CONSTANTLEVEL can be added to indicate fixed direction and exhaust
	 *  level, respectively.
	 * \note If the spec->ldir parameter is provided, it must specify the engine thrust
	 *  direction (= the negative exhaust direction), in contrast to the other
	 *  AddExhaust functions, which refer to the positive exhaust direction.
	 * \note spec->lsize and spec->wsize define the length and width of the exhaust
	 *  flame [m].
	 * \note spec->lofs defines a longitudinal offset between the reference position
	 *  and the exhaust flame.
	 * \note spec->modulate defines the amplitude of a random variation in exhaust
	 *  level, between 0 (none) and 1 (max).
	 * \note spec->tex can be used to provide a custom exhaust texture. If spec->tex
	 *  == NULL, then the default exhaust texture is used.
	 */
	UINT AddExhaust (EXHAUSTSPEC *spec);

	/**
	 * \brief Removes an exhaust render definition.
	 * \param idx exhaust identifier
	 * \return \e false if exhaust definition does not exist, \e true otherwise.
	 * \sa AddExhaust, GetExhaustCount
	 */
	bool DelExhaust (UINT idx) const;

	/**
	 * \brief Returns the number of exhaust render definitions for the vessel.
	 * \return Number of exhaust render definitions
	 * \sa AddExhaust, DelExhaust
	 */
	DWORD GetExhaustCount () const;

	/**
	 * \brief Returns the parameters of an exhaust definition.
	 * \param [in] idx exhaust identifier
	 * \param [out] lscale exhaust flame length [m]
	 * \param [out] wscale exhaust flame width [m]
	 * \param [out] pos reference position [<b>m</b>]
	 * \param [out] dir exhaust direction
	 * \param [out] tex texture handle for custom exhaust flames, if any
	 * \return \e false if \a idx out of range, \e true otherwise.
	 * \sa AddExhaust
	 */
	bool GetExhaustSpec (UINT idx, double *lscale, double *wscale, VECTOR3 *pos, VECTOR3 *dir, SURFHANDLE *tex) const;

	/**
	 * \brief Returns the parameters of an exhaust definition in a structure.
	 * \param [in] idx exhaust identifier
	 * \param [out] spec pointer to EXHAUSTSPEC structure
	 * \return \e false if \a idx is out of range, \e true otherwise.
	 * \note On return the parameters of the specified exhaust object are
	 *   copied into the structure pointed to by spec.
	 */
	bool GetExhaustSpec (UINT idx, EXHAUSTSPEC *spec);

	/**
	 * \brief Returns the current level of an exhaust source.
	 * \param idx exhaust identifier
	 * \return Exhaust level (0..1)
	 * \note The exhaust level is equivalent to the thrust level of the thruster
	 *   to which the exhaust definition is attached.
	 * \sa AddExhaust, GetThrusterLevel
	 */
	double GetExhaustLevel (UINT idx) const;

	/**
	 * \brief Select a previously registered texture to be used for rendering reentry flames.
	 * \param tex texture handle
	 * \param plimit friction power limit
	 * \param lscale texture length scaling factor
	 * \param wscale texture width scaling factor
	 * \note The texture handle is obtained by a previous call to \ref oapiRegisterReentryTexture.
	 * \note If a custom texture is not explicitly set, Orbiter uses a default
	 *   texture (reentry.dds) for rendering reentry flames. To suppress reentry
	 *   flames altogether for a vessel, call SetReentryTexture(NULL).
	 * \sa oapiRegisterReentryTexture
	 */
	void SetReentryTexture (SURFHANDLE tex, double plimit=6e7, double lscale=1.0, double wscale=1.0) const;
	//@}


	/// \name Particle systems
	//@{
	/**
	 * \brief Adds a custom particle stream to a vessel.
	 * \param pss pointer to particle stream definition structure
	 * \param pos particle source position in vessel coordinates [<b>m</b>]
	 * \param dir particle emission direction in vessel coordinates
	 * \param lvl pointer to scaling factor
	 * \return Particle stream handle
	 * \note This function can be used to add venting effects and similar.
	 *   For engine-specific effects such as exhaust and contrails, use the
	 *   \ref AddExhaustStream functions instead.
	 * \note The \ref PARTICLESTREAMSPEC structure defined the properties of
	 *   the particle stream.
	 * \note The position and direction variables are in vessel-relative
	 *   coordinates. They cannot be redefined.
	 * \note \a lvl points to a variable which defines the strength of the
	 *   particle emission. Its value should be set in the range from 0
	 *   (particle generation off) to 1 (emission at full strength). It can
	 *   be changed continuously to modulate the particle generation.
	 * \sa AddExhaustStream, AddReentryStream
	 */
	PSTREAM_HANDLE AddParticleStream (PARTICLESTREAMSPEC *pss, const VECTOR3 &pos, const VECTOR3 &dir, double *lvl) const;

	/**
	 * \brief Adds an exhaust particle stream to a vessel.
	 * \param th thruster handle
	 * \param pss particle stream specification
	 * \return Particle stream handle
	 * \note Exhaust streams can be emissive (to simulate "glowing" ionised
	 *   gases) or diffuse (e.g. for simulating vapour trails).
	 * \note The \ref PARTICLESTREAMSPEC structure defined the properties of
	 *   the particle stream.
	 * \note Multiple streams can be defined for a single engine. For
	 *   example, an emissive stream with short lifetime may represent the
	 *   ionised exhaust gases, while a diffuse stream with longer lifetime
	 *   represents the vapour trail.
	 * \note To improve performance, closely packed engines may share a
	 *   single exhaust stream.
	 * \note If the user has disabled particle streams in the launchpad
	 *   dialog, this function will return NULL. The module must be able
	 *   to cope with this case.
	 * \sa AddExhaustStream(THRUSTER_HANDLE,const VECTOR3&,PARTICLESTREAMSPEC*)const,
	 *   AddParticleStream, AddReentryStream
	 */
	PSTREAM_HANDLE AddExhaustStream (THRUSTER_HANDLE th, PARTICLESTREAMSPEC *pss = 0) const;

	/**
	 * \brief Adds an exhaust particle stream to a vessel.
	 * \param th thruster handle
	 * \param pos particle emission reference point
	 * \param pss particle stream specification
	 * \return Particle stream handle
	 * \note This version allows to pass an explicit particle emission
	 *   reference position, independent of the engine reference point.
	 * \note If the user has disabled particle streams in the launchpad
	 *   dialog, this function will return NULL. The module must be able
	 *   to cope with this case.
	 * \sa AddExhaustStream(THRUSTER_HANDLE,PARTICLESTREAMSPEC*)const,
	 *   AddParticleStream, AddReentryStream
	 */
	PSTREAM_HANDLE AddExhaustStream (THRUSTER_HANDLE th, const VECTOR3 &pos, PARTICLESTREAMSPEC *pss = 0) const;

	/**
	 * \brief Adds a reentry particle stream to a vessel.
	 * \param pss particle stream specification
	 * \return Particle stream handle
	 * \note Vessels automatically define a default emissive particle stream,
	 *   but you may want to add further stream to customise the appearance.
	 * \sa AddParticleStream, AddExhaustStream
	 */
	PSTREAM_HANDLE AddReentryStream (PARTICLESTREAMSPEC *pss) const;

	/**
	 * \brief Delete an existing particle stream.
	 * \param ch particle stream handle
	 * \return \e false indicates failure (particle stream not found)
	 * \note If a thruster is deleted (with ref DelThruster), any attached
	 *   particle streams are deleted automatically.
	 * \note A deleted particle stream will no longer emit particles, but
	 *   existing particles persist until they expire.
	 * \sa AddParticleStream, AddExhaustStream, AddReentryStream
	 */
	bool DelExhaustStream (PSTREAM_HANDLE ch) const;
	//@}


	/// \name Nosewheel-steering and wheel brakes
	//@{
	/**
	 * \param activate \e true to activate, \e false to deactivate
	 * \note With nose-wheel steering active, the yaw controls will
	 *   apply a lateral force on the front touchdown-point when in
	 *   ground contact.
	 * \note By default, nose-wheel steering is inactive. This
	 *   function should only be called for appropriate vessel types.
	 * \sa GetNosewheelSteering
	 */
	void SetNosewheelSteering (bool activate) const;

	/**
	 * \brief Returns the activation state of the nose-wheel steering
	 *   system.
	 * \return \e true indicates nose-wheel steering is active,
	 *   \e false indicates disabled.
	 * \sa SetNosewheelSteering
	 */
	bool GetNosewheelSteering () const;

	/**
	 * \brief Define the maximum force which can be provided by the
	 *   vessel's wheel brake system.
	 * \param f maximum force [N]
	 * \sa SetWheelbrakeLevel, GetWheelbrakeLevel
	 */
	void SetMaxWheelbrakeForce (double f) const;

	/**
	 * \brief Apply the wheel brake.
	 * \param level wheelbrake level [0..1]
	 * \param which 0 = both, 1 = left, 2 = right main gear
	 * \param permanent \e true sets the level permanently, \e false
	 *   only applies to current time step
	 * \sa SetMaxWheelbrakeForce, GetWheelbrakeLevel
	 */
	void SetWheelbrakeLevel (double level, int which = 0, bool permanent = true) const;

	/**
	 * \brief Returns the current wheel brake level.
	 * \param which 0 = average of both main gear levels, 1 = left, 2 = right
	 * \return wheel brake level [0..1]
	 * \sa SetMaxWheelbrakeForce, SetWheelbrakeLevel
	 */
	double GetWheelbrakeLevel (int which) const;
	//@}


	/// \name Beacon management
	//@{
	/**
	 * \brief Add a light beacon definition to a vessel.
	 * \param bs structure defining the beacon parameters
	 * \note The BEACONLIGHTSPEC variable passed to AddBeacon (as well as
	 *   the pos and col vectors pointed to by the structure) must remain
	 *   valid until the beacon is removed (with DelBeacon, ClearBeacons,
	 *   or by deleting the vessel). It should therefore either be defined
	 *   static, or as a member of the derived vessel class.
	 * \note The BEACONLIGHTSPEC parameters can be modified at any time by
	 *   the module after the call to AddBeacon, to modify the beacon
	 *   appearance. The changes take effect immediately.
	 * \note To turn the beacon off temporarily, don't delete the beacon
	 *   but simply set the \e active element to false.
	 * \note \a shape defines the appearance of the beacon. Currently
	 *   supported are:
	 *   - BEACONSHAPE_COMPACT (a compact blob)
	 *   - BEACONSHAPE_DIFFUSE (a more diffuse blob)
	 *   - BEACONSHAPE_STAR (a starlike appearance)
	 * \note \a falloff detemines how the render size of the beacon
	 *   changes with distance. The value should be between 0 and 1, where
	 *   0 means that the apparent size of the beacon is proportional to
	 *   1/distance, and 1 means that the apparent size doesn't change at
	 *   all with distance. The higher the value, the further away the
	 *   beacon will remain visible. (but note that visibility is limited
	 *   to the range defined by \ref SetVisibilityLimit).
	 * \note \a period, \a duration and \a tofs are used to define a
	 *   periodically blinking beacon (strobe). To define a continuous
	 *   beacon, set period = 0. The two other parameters are then ignored.
	 * \sa DelBeacon, ClearBeacons, SetVisibilityLimit
	 */
	void AddBeacon (BEACONLIGHTSPEC *bs);

	/**
	 * \brief Remove a beacon definition from the vessel.
	 * \param bs pointer to the BEACONLIGHTSPEC structure previously use to
	 *   define the beacon with AddBeacon.
	 * \return \e true if the beacon definition was found and removed,
	 *   \e false otherwise.
	 * \note DelBeacon removes the beacon reference from the vessel's list
	 *   of beacons, but does not deallocate the beacon itself. If the
	 *   vessel had defined the beacon specification dynamically, it should
	 *   deallocate it after this call.
	 * \sa AddBeacon, ClearBeacons
	 */
	bool DelBeacon (BEACONLIGHTSPEC *bs);

	/**
	 * \brief Remove all beacon definitions from the vessel.
	 * \sa AddBeacon, DelBeacon
	 */
	void ClearBeacons ();

	/**
	 * \brief Returns a pointer to one of the vessel's beacon specifications.
	 * \param idx beacon list index (>= 0)
	 * \return Pointer to specification for vessel beacon at list index
	 *   \a idx, or NULL if \a idx is out of range.
	 * \note The list index for a given beacon can change when the vessel
	 *   adds or deletes beacons.
	 */
	const BEACONLIGHTSPEC *GetBeacon (DWORD idx) const;
	//@}


	///\ name Light emitters
	//@{
	/**
	 * \brief Add an isotropic point light source to the vessel.
	 * \param pos source position [<b>m</b>] in vessel coordinates 
	 * \param range light source range [m]
	 * \param att0 attenuation coefficients (see notes)
	 * \param att1 attenuation coefficients (see notes)
	 * \param att2 attenuation coefficients (see notes)
	 * \param diffuse source contribution to diffuse object colours
	 * \param specular source contribution to specular object colours
	 * \param ambient source contribution to ambient object colours
	 * \return pointer to new emitter object
	 * \note The intensity \e I of the light source as a function of distance
	 *   \e d is defined via the coefficients \att by
	 *   \f[ I = \frac{1}{ \mathrm{att}_0 + d \mathrm{att}_1 + d^2 \mathrm{att}_2} \f]
	 */
	LightEmitter *AddPointLight (const VECTOR3 &pos, double range, double att0, double att1, double att2, COLOUR4 diffuse, COLOUR4 specular, COLOUR4 ambient) const;

	/**
	 * \brief Add a directed spot light source to the vessel.
	 * \param pos source position [<b>m</b>] in vessel coordinates
	 * \param dir light direction in vessel coordinates
	 * \param range light source range [m]
	 * \param att0 attenuation coefficients (see notes)
	 * \param att1 attenuation coefficients (see notes)
	 * \param att2 attenuation coefficients (see notes)
	 * \param umbra aperture of inner (maximum intensity) cone [rad]
	 * \param penumbra aperture of outer (zero intensity) cone [rad]
	 * \param diffuse source contribution to diffuse object colours
	 * \param specular source contribution to specular object colours
	 * \param ambient source contribution to ambient object colours
	 * \return pointer to new emitter object
	 * \note The intensity \e I of the light source as a function of distance
	 *   \e d is defined via the coefficients \att by
	 *   \f[ I = \frac{1}{ \mathrm{att}_0 + d \mathrm{att}_1 + d^2 \mathrm{att}_2} \f]
	 */
	LightEmitter *AddSpotLight (const VECTOR3 &pos, const VECTOR3 &dir, double range, double att0, double att1, double att2, double umbra, double penumbra, COLOUR4 diffuse, COLOUR4 specular, COLOUR4 ambient) const;

	/**
	 * \brief Returns the number of light sources defined for the vessel.
	 * \return Number of light sources.
	 */
	DWORD LightEmitterCount () const;

	/**
	 * \brief Returns a pointer to a light source object identified by index.
	 * \param i emitter index (>= 0)
	 * \return Pointer to light source object, or NULL if index out of range
	 * \note The index of a given source object can change if other objects in
	 *   the list are deleted.
	 * \sa LightEmitterCount
	 */
	const LightEmitter *GetLightEmitter (DWORD i) const;

	/**
	 * \brief Deletes the specified light source from the vessel.
	 * \param le pointer to light emitter object
	 * \return \e true if the emitter was successfully deleted, \e false if
	 *   the source was not recognised by the vessel.
	 * \note If the method returns \e true, the emitter (le) was deallocated
	 *   and the pointer should no longer be used.
	 * \sa ClearLightEmitters, LightEmitterCount
	 */
	bool DelLightEmitter (LightEmitter *le) const;

	/**
	 * \brief Remove all light sources defined for the vessel.
	 * \sa AddPointLight, AddSpotLight, LightEmitterCount
	 */
	void ClearLightEmitters () const;
	//@}


	/// \name File I/O
	//@{
	/**
	 * \brief Pass a line read from a scenario file to Orbiter for default processing.
	 * \param line line to be interpreted
	 * \param status status parameters (points to a VESSELSTATUSx variable).
	 * \note This function should be used within the body of \ref VESSEL2::clbkLoadStateEx.
	 * \note The parser clbkLoadStateEx should forward all lines not recognised
	 *   by the module to Orbiter via ParseScenarioLineEx to allow processing of
	 *   standard vessel settings.
	 * \note clbkLoadStateEx currently provides a VESSELSTATUS2 status definition.
	 *   This may change in future versions, so status should not be used within
	 *   clbkLoadStateEx other than passing it to ParseScenarioLineEx.
	 * \sa VESSEL2::clbkLoadStateEx
	 */
	void ParseScenarioLineEx (char *line, void *status) const;
	//@}


	/// \name Obsolete methods
	//@{
	/**
	 * \brief Set the thrust level for an engine group.
	 * \deprecated This method has been replaced by \ref VESSEL::SetThrusterGroupLevel.
	 * \param eng engine group identifier
	 * \param level thrust level [0..1]
	 * \sa SetThrusterGroupLevel, IncEngineLevel
	 */
	void SetEngineLevel (ENGINETYPE eng, double level) const;

	/**
	 * \brief Increase or decrease the thrust level for an engine group.
	 * \deprecated This method has been replaced by \ref VESSEL::IncThrusterGroupLevel.
	 * \param eng engine group identifier
	 * \param dlevel thrust increment
	 * \note Use negative dlevel to decrease the engine's thrust level.
	 * \note Levels are clipped to valid range.
	 * \sa IncThrusterGroupLevel, SetEngineLevel
	 */
	void IncEngineLevel (ENGINETYPE eng, double dlevel) const;

	double GetMaxThrust (ENGINETYPE eng) const;
	void SetMaxThrust (ENGINETYPE eng, double th) const;
	double GetEngineLevel (ENGINETYPE eng) const;
	double *GetMainThrustModPtr () const;

	/**
	 * \deprecated This method no longer performs any action.
	 *   It has been replaced by the VESSEL::AddExhaust methods.
	 * \sa AddExhaust(THRUSTER_HANDLE,double,double,SURFHANDLE)const,
	 *   AddExhaust(THRUSTER_HANDLE,double,double,double,SURFHANDLE)const,
	 *   AddExhaust(THRUSTER_HANDLE,double,double,const VECTOR3&,const VECTOR3&,SURFHANDLE)const
	 */
	void SetExhaustScales (EXHAUSTTYPE exh, WORD id, double lscale, double wscale) const;

	/**
	 * \brief Delete a thruster group and (optionally) all associated thrusters.
	 * \deprecated This method has been replaced by VESSEL::DelThrusterGroup(THGROUP_HANDLE,bool)const.
	 * \param thg thruster group handle (NULL on return)
	 * \param thgt thruster group type (see \ref thrusterparam)
	 * \param delth thruster destruction flag (see notes)
	 * \return \e true on success.
	 * \note If \a delth==true, all thrusters associated with the group will be
	 *   destroyed. Note that this can have side effects if the thrusters were
	 *   associated with multiple groups, since they are removed from all those
	 *   groups as well.
	 * \sa DelThrusterGroup(THGROUP_TYPE,bool)const, CreateThrusterGroup,
	 *   DelThruster, thrusterparam
	 */
	bool DelThrusterGroup (THGROUP_HANDLE &thg, THGROUP_TYPE thgt, bool delth = false) const;

	UINT   AddExhaustRef (EXHAUSTTYPE exh, VECTOR3 &pos, double lscale = -1.0, double wscale = -1.0, VECTOR3 *dir = 0) const; // obsolete
	void   DelExhaustRef (EXHAUSTTYPE exh, WORD id) const;  // obsolete
	void   ClearExhaustRefs (void) const; // obsolete
	UINT   AddAttExhaustRef (const VECTOR3 &pos, const VECTOR3 &dir, double wscale = 1.0, double lscale = 1.0) const; // obsolete
	void   AddAttExhaustMode (UINT idx, ATTITUDEMODE mode, int axis, int dir) const; // obsolete
	void   ClearAttExhaustRefs (void) const; // obsolete

	/**
	 * \brief Defines the three points defining the vessel's ground contact plane.
	 * \deprecated This method has been replaced by VESSEL::SetTouchdownPoints(const TOUCHDOWNVTX*,DWORD)const
	 * \param pt1 touchdown point of nose wheel (or equivalent)
	 * \param pt2 touchdown point of left main wheel (or equivalent)
	 * \param pt3 touchdown point of right main wheel (or equivalent)
	 * \note The points are the positions at which the vessel's undercarriage
	 *   (or equivalent) touches the surface, specified in local vessel
	 *   coordinates.
	 * \note The order of points is significant since it defines the direction
	 *   of the normal. The points should be specified such that the cross
	 *   product pt3-pt1 x pt2-pt1 defines the horizon "up" direction for
	 *   the landed vessel (given a left-handed coordinate system). 
	 * \note Modifying the touchdown points during the simulation while the
	 *   vessel is on the ground can result in jumps due to instantaneous
	 *   position changes (infinite acceleration). To avoid this, the
	 *   touchdown points should be modified gradually by small amounts
	 *   over time (proportional to simulation time steps). 
	 * \note This method is retained only for backward compatibility. Vessels should now use
	 *   SetTouchdownPoints(const TOUCHDOWNVTX*,DWORD)const to define a convex hull of touchdown points.
	 * \note The touchdown stiffness and damping parameters are guessed according to the vessel
	 *   empty mass. Therefore, SetTouchdownPoints should be called \e after defining the empty
	 *   vessel mass with SetEmptyMass.
	 * \sa GetTouchdownPoints, GetCOG_elev, \ref SetTouchdownPoints(const TOUCHDOWNVTX*, DWORD)const
	 *   SetEmptyMass
	 */
	void SetTouchdownPoints (const VECTOR3 &pt1, const VECTOR3 &pt2, const VECTOR3 &pt3) const;

	/**
	 * \brief Returns the three points defining the vessel's ground contact plane.
	 * \deprecated This method has been replaced by VESSEL::GetTouchdownPoint(TOUCHDOWNVTX&,DWORD)const
	 * \param pt1 touchdown point of nose wheel (or equivalent)
	 * \param pt2 touchdown point of left main wheel (or equivalent)
	 * \param pt3 touchdown point of right main wheel (or equivalent)
	 * \note The function returns 3 reference points defining the vessel's
	 *   surface contact points when touched down on a planetary surface
	 *   (e.g. landing gear).
	 * \note This function is superseded by \ref GetTouchdownPoint(TOUCHDOWNVTX&,DWORD)const, which
	 *   provides access to additional parameters and can be used for touchdown points >= 3.
	 * \sa GetTouchdownPoint, SetTouchdownPoints, GetCOG_elev
	 */
	void GetTouchdownPoints (VECTOR3 &pt1, VECTOR3 &pt2, VECTOR3 &pt3) const;

	/**
	 * \brief Returns the scaling factor for the yaw moment.
	 * \deprecated This method has been replaced by VESSEL::GetYawMomentScale.
	 * \return yaw moment scale factor
	 * \note The method is misnamed. It refers to the vessel's yaw moment.
	 * \sa GetYawMomentScale
	 */
	double GetBankMomentScale () const;

	/**
	 * \brief Sets the scaling factor for the yaw moment.
	 * \deprecated This method has been replaced by VESSEL::SetYawMomentScale.
	 * \param scale scale factor for slip angle moment.
	 * \note The method is misnamed. It refers to the vessel's yaw moment.
	 * \sa SetYawMomentScale
	 */
	void SetBankMomentScale (double scale) const;

	/**
	 * \brief Sets the channel of a NAV radio receiver.
	 * \deprecated This method has been replaced by VESSEL::SetNavChannel
	 * \param n receiver index (>= 0)
	 * \param ch channel (>= 0)
	 * \return \e false on error (index out of range), \e true otherwise
	 */
	bool SetNavRecv (DWORD n, DWORD ch) const;

	/**
	 * \brief Returns the current channel setting of a NAV radio receiver.
	 * \deprecated This method has been replaced by VESSEL::GetNavChannel
	 * \param n receiver index (>= 0)
	 * \return Receiver channel [0..639]. If index \a n is out of range, the
	 *   return value is 0.
	 */
	DWORD GetNavRecv (DWORD n) const;

	/**
	 * \brief Set the altitude of the vessel's centre of gravity over ground level when
	 *   landed.
	 * \param h elevation of the vessel's centre of gravity above the surface plane when
	 *   landed [m].
	 * \deprecated This method is obsolete and should no longer be used. It has been replaced
	 *   by \ref VESSEL::SetTouchdownPoints.
	 */
	void SetCOG_elev (double h) const;

	/**
	 * \brief Remove all mesh definitions for the vessel.
	 * \deprecated This version is obsolete and has been replaced by
	 *   \ref VESSEL::ClearMeshes(bool)const .
	 * \note Equivalent to ClearMeshes(true). This method is only retained for
	 *   backward compatibility, and may be removed in future versions.
	 * \sa ClearMeshes(bool)const
	 */
	void ClearMeshes () const;

	/**
	 * \brief Marks a mesh as visible from internal cockpit view.
	 * \param idx mesh index (>= 0)
	 * \param visible visibility flag
	 * \deprecated This method is obsolete and has been replaced by
	 *   \ref VESSEL::SetMeshVisibilityMode.
	 * \note By default, a vessel is not rendered when the camera is in internal
	 *   (cockpit) view. This function can be used to force rendering of some or
	 *   all of the vessel's meshes.
	 * \sa SetMeshVisibilityMode
	 */
	void SetMeshVisibleInternal (UINT idx, bool visible) const;

	UINT   RegisterAnimSequence (double defmeshstate) const; // obsolete
	bool   AddAnimComp (UINT seq, ANIMCOMP *comp); // obsolete
	bool   SetAnimState (UINT seq, double state); // obsolete

	/**
	 * \brief Causes Orbiter to write default vessel parameters to a scenario file.
	 * \deprecated Use a call to the base class VESSEL2::clbkSaveState from within
	 *   the overloaded callback function instead.
	 * \param scn scenario file handle
	 * \note This method saves the vessel's default state parameters (such as 
	 *   position, velocity, orientation, etc.) to a scenario file.
	 * \note This functionality is now included in the default implementation of
	 *   VESSEL2::clbkSaveState. Therefore, vessel classes which overload this
	 *   method to save custom vessel parameters should call the base class method
	 *   to allow Orbiter to save the default vessel parameters.
	 * \sa VESSEL2::clbkSaveState
	 */
	void SaveDefaultState (FILEHANDLE scn) const;

	/**
	 * \brief Pass a line read from a scenario file to Orbiter for default
	 *   processing.
	 * \deprecated This function is retained for backward compatibility only.
	 *   New modules should overload the \ref VESSEL2::clbkLoadStateEx function and
	 *   use \ref VESSEL::ParseScenarioLineEx for default state parsing.
	 * \param line line to be interpreted
	 * \param status state parameter set
	 * \sa ParseScenarioLineEx, VESSELSTATUS
	 */
	void ParseScenarioLine (char *line, VESSELSTATUS *status) const;

	/**
	 * \brief Add a variable drag element
	 * \deprecated This method has been replaced with
	 *   \ref CreateVariableDragElement(const double*,double,const VECTOR3&)const.
	 */
	void CreateVariableDragElement (double *drag, double factor, const VECTOR3 &ref) const;

	/**
	 * \brief Vessel creation
	 * \deprecated This method has been replaced with \ref oapiCreateVessel
	 *   and \ref oapiCreateVesselEx.
	 */
	static OBJHANDLE Create (const char *name, const char *classname, const VESSELSTATUS &status);
	//@}

protected:
	Vessel *vessel;     ///< Orbiter internal vessel class
	short  flightmodel; ///< realism level
	short  version;     ///< interface version
};

// ======================================================================
// class VESSEL2
// ======================================================================
/**
 * \brief Callback extensions to the VESSEL class
 *
 * The VESSEL2 class adds a variety of callback functions to the VESSEL
 * interface (clbk*). These are called by Orbiter to notify the vessel
 * about different types of events and allow it to react to them. The
 * VESSEL2 class implements these as virtual functions which act as
 * placeholders to be overwritten by derived classes whenever a non-default
 * behaviour is required.
 */
// ======================================================================
// NOTE: Do NOT add or remove methods to this class, or re-arrange the
// order of the exisiting methods, to avoid breaking addons (incompatible
// virtual tables)!
// ======================================================================

class OAPIFUNC VESSEL2: public VESSEL {
public:
	/**
	 * \brief Creates a VESSEL2 interface for a vessel object.
	 *
	 * An instance of a vessel class derived from VESSEL2 is typically
	 * called during the initialisation of a vessel module (during ovcInit)
	 * to create an interface to the vessel instance controlled by the
	 * module. However, a VESSEL2 instance for any existing vessel can be
	 * created by any module.
	 * \param hVessel vessel object handle
	 * \param fmodel requested level of realism (0=simple, 1=realistic)
	 * \note This function creates an interface to an \e existing vessel.
	 *   It does not create a new vessel. New vessels are created with the
	 *   oapiCreateVessel and oapiCreateVesselEx functions.
	 * \note The VESSEL2 interface instance created in ovcInit should be
	 *   deleted in ovcExit.
	 * \sa oapiCreateVessel, oapiCreateVesselEx, ovcInit
	 */
	VESSEL2 (OBJHANDLE hVessel, int fmodel=1);

	// ===== Callback functions =====

	/**
	 * \brief Initialisation of vessel capabilities
	 *
	 * Called after vessel creation, this function allows to set vessel
	 * class capabilities and parameters. This can include definition of
	 * physical properties (size, mass, docking ports, etc.), creation of
	 * propellant resources and engines, aerodynamic parameters, including
	 * airfoil definitions, lift and drag properties, or active control
	 * surfaces.
	 * \param cfg handle for the vessel class configuration file
	 * \default None.
	 * \note This function is called after the vessel has been created, but
	 *   before its state is read from the scenario file. This means that
	 *   its state (position, velocity, fuel level, etc.) is undefined at
	 *   this point.
	 * \note Use this function to set vessel class capabilities, not vessel
	 *   state parameters.
	 * \note Orbiter will scan the vessel class configuration file for generic
	 *   parameters (like mass or size) after clbkSetClassCaps returns.
	 *   This allows to override generic caps defined in the module by
	 *   editing the configuration file.
	 * \note The configuration file handle is also passed to clbkSetClassCaps,
	 *   to allow reading of vessel class-specific parameters from file.
	 * \sa \ref progflow1
	 */
	virtual void clbkSetClassCaps (FILEHANDLE cfg);

	/**
	 * \brief Called when the vessel needs to save its current status to a
	 *   scenario file.
	 * \param scn scenario file handle
	 * \default Saves the generic vessel state parameters.
	 * \note clbkSaveState is called by Orbiter at the end of a simulation
	 *   session while creating the save scenario for the current
	 *   simulation state.
	 * \note This function only needs to be overloaded if the vessel must
	 *   save nonstandard parameters.
	 * \note If clbkSaveState is overloaded, generic state parameters will only
	 *   be written if the base class VESSEL2::clbkSaveState is called.
	 * \note To write custom parameters to the scenario file, use the
	 *   oapiWriteLine function.
	 * \sa \ref progflow1
	 */
	virtual void clbkSaveState (FILEHANDLE scn);

	/**
	 * \brief Called when the vessel needs to load its initial state from a
	 *   scenario file.
	 * \param scn scenario file handle
	 * \param status pointer to VESSELSTATUSx structure (x >= 2)
	 * \default Loads the generic vessel state parameters.
	 * \note This callback function allows to read custom vessel status
	 *   parameters from a scenario file.
	 * \note The function should define a loop which parses lines from the
	 *   scenario file via oapiReadScenario_nextline.
	 * \note You should not call the base class clbkLoadStateEx to parse
	 *   generic parameters, because this will skip over any custom
	 *   scenario entries. Instead, any lines which the module parser does
	 *   not recognise should be forwarded to Orbiter's default scenario
	 *   parser via VESSEL::ParseScenarioLineEx.
	 * \sa VESSELSTATUS2, ParseScenarioLineEx, oapiReadScenario_nextline,
	 *   \ref progflow1
	 */
	virtual void clbkLoadStateEx (FILEHANDLE scn, void *status);

	/**
	 * \brief Set state parameters during vessel creation
	 * \param status pointer to a VESSELSTATUSx structure
	 * \default Invokes Orbiter's default state initialisation.
	 * \par Calling sequence:
	 *   This function is called when the vessel is being created with
	 *   oapiCreateVesselEx, after its clbkSetClassCaps has been invoked and
	 *   before its clbkPostCreation method is invoked. Vessels that are
	 *   created during simulation start as a result of parsing the scenario
	 *   file invoke clbkLoadStateEx instead.
	 * \note This callback function receives the VESSELSTATUSx structure
	 *   passed to oapiCreateVesselEx. It must therefore be able to process
	 *   the interface version used by those functions.
	 * \note This function remains valid even if future versions of Orbiter
	 *   introduce new VESSELSTATUSx interfaces.
	 * \note If an overloaded method does not call VESSEL2::clbkSetStateEx,
	 *   no default state initialisation is performed. Default state
	 *   initialisation can also be done by calling VESSEL::DefSetStateEx.
	 */
	virtual void clbkSetStateEx (const void *status);

	/**
	 * \brief Called after a vessel has been created and its state has been
	 *   set.
	 * \default None.
	 * \par Calling sequence:
	 *   This function is called during vessel creation after clbkSetStateEx
	 *   or clbkLoadStateEx have been called and before the vessel enters the
	 *   update loop, i.e. before its clbkPreStep is invoked for the first
	 *   time.
	 *   Vessels that are created at the start of the simulation (i.e. are
	 *   listed in the scenario) call their clbkPostCreation after all
	 *   scenario vessels have been created.
	 * \note This function can be used to perform the final setup steps for
	 *   the vessel, such as animation states and instrument panel states.
	 *   When this function is called, the vessel state (e.g. position,
	 *   thruster levels, etc.) have been defined.
	 * \sa \ref progflow1
	 */
	virtual void clbkPostCreation ();

	/**
	 * \brief Called after a vessel gained or lost input focus.
	 * \param getfocus true if the vessel gained focus, false if it lost focus
	 * \param hNewVessel handle of vessel gaining focus
	 * \param hOldVessel handle of vessel losing focus
	 * \default None.
	 * \note Whenever the input focus is switched to a new vessel (e.g. via
	 *   user selection F3), this method is called for both the vessel
	 *   losing focus (getfocus=false) and the vessel gaining focus
	 *   (getfocus=true).
	 * \note In both calls, hNewVessel and hOldVessel are the vessel handles
	 *   for the vessel gaining and the vessel losing focus, respectively.
	 * \note This method is also called at the beginning of the simulation for
	 *   the initial focus object. In this case hOldVessel is NULL.
	 */
	virtual void clbkFocusChanged (bool getfocus, OBJHANDLE hNewVessel, OBJHANDLE hOldVessel);

	/**
	 * \brief Time step notification before state update.
	 *
	 * Called at each simulation time step before the state is updated to
	 * the current simulation time. This function allows to define actions
	 * which need to be controlled continuously.
	 * \param simt next simulation run time [s]
	 * \param simdt step length over which the current state will be
	 *   integrated [s]
	 * \param mjd next absolute simulation time (days) in Modified Julian
	 *   Date format
	 * \default None
	 * \note This function is called at each frame of the simulation, after the
	 *   integration step length has been determined, but before the time
	 *   integration is applied to the current simulation state.
	 * \note This method is useful when the step length Dt is required in
	 *   advance of the time integration, for example to apply a force that
	 *   produces a given Dv, since the AddForce request will be applied in
	 *   the next update. Using clbkPostStep for this purpose would be
	 *   wrong, because its Dt parameter refers to the previous step length.
	 * \sa clbkPostStep, \ref progflow1
	 */
	virtual void clbkPreStep (double simt, double simdt, double mjd);

	/**
	 * \brief Time step notification after state update
	 *
	 * Called at each simulation time step after the state has been updated
	 * to the current simulation time. This function allows to define
	 * actions which need to be controlled continuously.
	 * \param simt current simulation run time [s]
	 * \param simdt last time step length [s]
	 * \param mjd absolute simulation time (days) in Modified Julian Date
	 *   format.
	 * \default None.
	 * \note This function, if implemented, is called at each frame for each
	 *   instance of this vessel class, and is therefore time-critical.
	 *   Avoid any unnecessary calculations here which may degrade
	 *   performance.
	 * \sa clbkPreStep, \ref progflow1
	 */
	virtual void clbkPostStep (double simt, double simdt, double mjd);

	/**
	 * \brief Playback event notification
	 *
	 * Called during playback of a recording session when a custom event
	 * tag in the vessel's articulation stream is encountered.
	 * \param simt current simulation time [s]
	 * \param event_t recorded event time [s]
	 * \param event_type event tag string
	 * \param event event data string
	 * \return Should return true if the event type is recognised and
	 *   processed, false otherwise.
	 * \default Do nothing, return false.
	 * \note This function can be used to process any custom vessel events that
	 *   have been recorded with VESSEL::RecordEvent during a recording
	 *   session.
	 */
	virtual bool clbkPlaybackEvent (double simt, double event_t, const char *event_type, const char *event);

	/**
	 * \brief Called after a vessel visual has been created by the renderer.
	 * \param vis handle for the newly created visual
	 * \param refcount visual reference count
	 * \default None.
	 * \note The logical interface to a vessel exists as long as the vessel is
	 *   present in the simulation. However, the visual interface exists
	 *   only when the vessel is within visual range of the camera. Orbiter
	 *   creates and destroys visuals as required. This enhances simulation
	 *   performance in the presence of a large number of objects in the
	 *   simulation.
	 * \note Whenever Orbiter creates a vessel's visual it reverts to its
	 *   initial configuration (e.g. as defined in the mesh file). The
	 *   module can use this function to update the visual to the current
	 *   state, wherever dynamic changes are required.
	 * \note More than one visual representation of an object may exist. The
	 *   refcount parameter defines how many visual interfaces to the
	 *   object exist.
	 * \sa \ref progflow1
	 */
	virtual void clbkVisualCreated (VISHANDLE vis, int refcount);

	/**
	 * \brief Called before a vessel visual is destroyed.
	 * \param vis handle for the visual to be destroyed
	 * \param refcount visual reference count
	 * \default None.
	 * \note Orbiter calls this function before it destroys a visual
	 *   representation of the vessel. This may be in response to the
	 *   destruction of the actual vessel, but in general simply means that
	 *   the vessel has moved out of visual range of the current camera
	 *   location.
	 * \sa \ref progflow1
	 */
	virtual void clbkVisualDestroyed (VISHANDLE vis, int refcount);

	/**
	 * \brief HUD redraw notification
	 *
	 * Called when the vessel's head-up display (HUD) needs to be redrawn
	 * (usually at each time step, unless the HUD is turned off).
	 * Overwriting this function allows to implement vessel-specific
	 * modifications of the HUD display (or to suppress the HUD altogether).
	 * \param mode HUD mode (see HUD_* constants in OrbiterAPI.h)
	 * \param hps pointer to a HUDPAINTSPEC structure
	 * \param hDC GDI drawing device context
	 * \default Draws a standard HUD display with Orbiter's default display
	 *   layout.
	 * \deprecated This method contains a device-dependent drawing context and
	 *   may not work with all graphics clients. It has been superseded by
	 *   VESSEL3::clbkDrawHUD.
	 * \note For vessels derived from VESSEL3 orbiter will not call this method,
	 *   but will call the VESSEL3::clbkDrawHUD method instead. The VESSEL3
	 *   version uses a generic \e Sketchpad drawing context instead of a HDC.
	 * \sa VESSEL3::clbkDrawHUD, \ref progflow1
	 */
	virtual void clbkDrawHUD (int mode, const HUDPAINTSPEC *hps, HDC hDC);

	/**
	 * \brief Reaction Control System mode change notification.
	 *
	 * Called when a vessel's RCS (reaction control system) mode changes.
	 * Usually the RCS consists of a set of small thrusters arranged so as
	 * to allow controlled attitude changes. In Orbiter, the RCS can be
	 * driven in either rotational mode (to change the vessel's angular
	 * velocity) or in linear mode (to change its linear velocity), or be
	 * switched off.
	 * \param mode new RCS mode: 0=disabled, 1=rotational, 2=linear
	 * \default None.
	 * \note This callback function is invoked when the user switches RCS mode
	 *   via the keyboard ("/" or "Ctrl-/" on numerical keypad) or after a
	 *   call to VESSEL::SetAttitudeMode or VESSEL::ToggleAttitudeMode.
	 * \note Not all vessel types may support a reaction control system. In
	 *   that case, the callback function can be ignored by the module.
	 * \sa \ref progflow1
	 */
	virtual void clbkRCSMode (int mode);

	/**
	 * \brief Aerodynamic control surface mode change notification.
	 *
	 * Called when user input mode for aerodynamic control surfaces
	 * (elevator, rudder, aileron) changes.
	 * \param mode control mode
	 * \default None.
	 * \note The returned control mode contains bit flags as follows:
	 *   - bit 0: elevator enabled/disabled
	 *   - bit 1: rudder enabled/disabled
	 *   - bit 2: ailerons enabled/disabled
	 *   .
	 *   Therefore, mode=0 indicates control surfaces disabled, mode=7
	 *   indicates fully enabled.
	 * \sa \ref progflow1
	 */
	virtual void clbkADCtrlMode (DWORD mode);

	/**
	 * \brief HUD mode change notification
	 *
	 * Called after a change of the vessel's HUD (head-up-display) mode.
	 * \param mode new HUD mode
	 * \default None.
	 * \note For currently supported HUD modes see HUD_* constants in
	 *   OrbiterAPI.h
	 * \note mode HUD_NONE indicates that the HUD has been turned off.
	 * \sa Section hudmode for a list of default mode identifiers,
	 *   \ref progflow1
	 */
	virtual void clbkHUDMode (int mode);

	/**
	 * \brief MFD mode change modification
	 *
	 * Called when the user has switched one of the MFD (multi-functional
	 * display) instruments to a different display mode.
	 * \param mfd MFD instrument identifier
	 * \param mode new MFD mode identifier
	 * \default None.
	 * \note This callback function can be used to refresh the MFD button 
	 *   labels after the MFD mode has changed, or if a mode requires a
	 *   dynamic label update.
	 * \note The mode parameter can be one of the MFD mode identifiers
	 *   MFD_* listed in OrbiterAPI.h, or MFD_REFRESHBUTTONS. The latter
	 *   is sent as a result of a call to oapiRefreshMFDButtons. It
	 *   indicates not a mode change, but the need to refresh the button
	 *   labels within a mode (i.e. a mode that dynamically changed its
	 *   labels).
	 * \sa Section mfdmode for a list of default mode identifiers,
	 *   \ref progflow1
	 */
	virtual void clbkMFDMode (int mfd, int mode);

	/**
	 * \brief Navigation mode change notification
	 *
	 * Called when an automated "navigation mode" is activated or
	 * deactivated for a vessel. Most navigation modes engage the vessel's
	 * RCS to attain a specific attitude, including pro/retrograde, normal
	 * to the orbital plane, level with the local horizon, etc.
	 * \param mode navmode identifier
	 * \param active true if activated, false if deactivated
	 * \default None.
	 * \sa Section navmode for a list of available navigation modes.
	 */
	virtual void clbkNavMode (int mode, bool active);

	/**
	 * \brief Docking event notification
	 *
	 * Called after a docking or undocking event at one of the vessel's
	 * docking ports.
	 * \param dock docking port index
	 * \param mate handle to docked vessel, or NULL for undocking event
	 * \default None.
	 * \note dock is the index (>= 0) of the vessel's docking port at which
	 *   the docking/undocking event takes place.
	 * \note mate is a handle to the vessel docking at the port, or NULL to
	 *   indicate an undocking event.
	 */
	virtual void clbkDockEvent (int dock, OBJHANDLE mate);

	/**
	 * \brief Manual animation notification
	 *
	 * Called at each simulation time step if the module has registered at
	 * least one animation notification request and if the vessel's visual
	 * exists.
	 * \param simt simulation time [s]
	 * \default None.
	 * \note This callback allows the module to animate the vessel's visual
	 *   representation (moving undercarriage, cargo bay doors, etc.)
	 * \note It is only called as long as the vessel has registered an
	 *   animation request (between matching VESSEL::RegisterAnimation and
	 *   VESSEL::UnregisterAnimation calls) and if the vessel's visual
	 *   exists.
	 * \note This callback is \e not used for the "semi-automatic"
	 *   animation mechanism (VESSEL::CreateAnimation,
	 *   VESSEL::AddAnimationComponent)
	 * \sa VESSEL::RegisterAnimation, VESSEL::UnregisterAnimation,
	 *   VESSEL::CreateAnimation, VESSEL::AddAnimationComponent
	 */
	virtual void clbkAnimate (double simt);

	/**
	 * \brief Keyboard status notification
	 *
	 * Called at each simulation time step to allow the module to
	 * query the current keyboard status. This callback can be used to
	 * install a custom keyboard interface for the vessel.
	 * \param kstate keyboard state
	 * \return A nonzero return value will completely disable default
	 *   processing of the key state for the current time step. To disable
	 *   the default processing of selected keys only, use the RESETKEY
	 *   macro (see OrbiterAPI.h) and return 0.
	 * \default None, returns 0.
	 * \note The keystate contains the current keyboard state. Use the
	 *   KEYDOWN macro in combination with the key identifiers as defined
	 *   in OrbiterAPI.h (OAPI_KEY_*) to check for particular keys being
	 *   pressed. Example:
	 * \code
	 *   if (KEYDOWN (kstate, OAPI_KEY_F10)) {
	 *     // perform action
	 *     RESETKEY (kstate, OAPI_KEY_F10);
	 *     // optional: prevent default processing of the key
	 *   }
	 * \endcode
	 * \note This function should be used where a key state, rather than a
	 *   key event is required, for example when engaging thrusters or
	 *   similar. To test for key events (key pressed, key released) use
	 *   clbkConsumeBufferedKey() instead.
	 * \sa clbkConsumeBufferedKey, \ref progflow1
	 */
	virtual int clbkConsumeDirectKey (char *kstate);

	/**
	 * \brief Keyboard event notification
	 *
	 * This callback function notifies the vessel of a buffered key event
	 * (key pressed or key released).
	 * \param key key scan code (see OAPI_KEY_* constants in OrbiterAPI.h)
	 * \param down true if key was pressed, false if key was released
	 * \param kstate current keyboard state
	 * \return The function should return 1 if Orbiter's default processing
	 *   of the key event should be skipped, 0 otherwise.
	 * \default None, returns 0.
	 * \note The key state (kstate) can be used to test for key modifiers
	 *   (Shift, Ctrl, etc.). The KEYMOD_xxx macros defined in OrbiterAPI.h
	 *   are useful for this purpose.
	 * \note This function may be called repeatedly during a single frame,
	 *   if multiple key events have occurred in the last time step.
	 * \sa clbkConsumeDirectKey, \ref progflow1
	 */
	virtual int clbkConsumeBufferedKey (DWORD key, bool down, char *kstate);

	/**
	 * \brief Generic cockpit view mode request notification
	 *
	 * Called when the vessel's generic "glass cockpit" view (consisting of
	 * two "floating" MFD instruments and a HUD, displayed on top of the
	 * 3-D render window) is selected by the user pressing F8, or by a
	 * function call.
	 * \return The function should return true if it supports generic
	 *   cockpit view, false otherwise.
	 * \default Sets camera direction to "forward" (0,0,1) and returns true.
	 * \note The generic cockpit view is available for all vessel types by
	 *   default, unless this function is overwritten to return false.
	 * \note Only disable the generic view if the vessel supports either
	 *   2-D instrument panels (see clbkLoadPanel) or a virtual cockpit
	 *   (see clbkLoadVC). If no valid cockpit view at all is available for
	 *   a vessel, Orbiter will crash.
	 * \note Even if the vessel supports panels or virtual cockpits, you
	 *   shouldn't normally disable the generic view, because it provides
	 *   the best performance on slower computers.
	 * \sa clbkLoadPanel, clbkLoadVC, \ref progflow1
	 */
	virtual bool clbkLoadGenericCockpit ();

	/**
	 * \brief 2-D instrument panel view mode request notification
	 *
	 * Called when Orbiter tries to switch the cockpit view to a 2-D
	 * instrument panel.
	 * \param id panel identifier (>= 0)
	 * \return The function should return true if it supports the requested
	 *   panel, false otherwise.
	 * \default None, returns false.
	 * \note In the body of this function the module should define the
	 *   panel background bitmap and panel capabilities, e.g. the position
	 *   of MFDs and other instruments, active areas (mouse hotspots) etc.
	 * \note A vessel which implements panels must at least support panel
	 *   id 0 (the main panel). If any panels register neighbour panels
	 *   (see oapiSetPanelNeighbours), all the neighbours must be
	 *   supported, too.
	 * \note This is a legacy function. The preferred method is now
	 *   VESSEL3::clbkLoadPanel2D
	 * \sa VESSEL3::clbkLoadPanel2D, oapiRegisterPanelBackground,
	 *   oapiRegisterPanelArea, oapiRegisterMFD, clbkLoadGenericCockpit,
	 *   clbkLoadVC
	 */
	virtual bool clbkLoadPanel (int id);

	/**
	 * \brief Mouse event notification for 2-D panel views.
	 *
	 * Called when a mouse-activated panel area receives a mouse event.
	 * \param id panel area identifier
	 * \param event mouse event (see \ref panel_mouse)
	 * \param mx,my relative mouse position in area at event
	 * \return The function should return true if it processes the event,
	 *   false otherwise.
	 * \default None, returns false.
	 * \note Mouse events are only sent for areas which requested
	 *   notification during definition (see oapiRegisterPanelArea).
	 * \sa clbkPanelRedrawEvent, \ref progflow1
	 */
	virtual bool clbkPanelMouseEvent (int id, int event, int mx, int my);

	/**
	 * \brief Redraw event notification for 2-D panel views.
	 *
	 * Called when a registered panel area needs to be redrawn.
	 * \param id panel area identifier
	 * \param event redraw event (see \ref panel_redraw)
	 * \param surf area surface handle
	 * \return The function should return true if it processes the event,
	 *   false otherwise.
	 * \default None, returns false.
	 * \note This callback function is only called for areas which were
	 *   not registered with the PANEL_REDRAW_NEVER flag.
	 * \note All redrawable panel areas receive a PANEL_REDRAW_INIT redraw
	 *   notification when the panel is created, in addition to any
	 *   registered redraw notification events.
	 * \note The surface handle surf contains either the current area
	 *   state, or the area background, depending on the flags passed
	 *   during area registration.
	 * \note The surface handle may be used for blitting operations, or to
	 *   receive a Windows device context (DC) for Windows-style redrawing
	 *   operations.
	 * \sa oapiGetDC, oapiReleaseDC, oapiTriggerPanelRedrawArea, clbkPanelMouseEvent, \ref progflow1
	 */
	virtual bool clbkPanelRedrawEvent (int id, int event, SURFHANDLE surf);

	/**
	 * \brief 3-D virtual cockpit view mode request notification
	 *
	 * Called when Orbiter tries to switch the cockpit view to a 3-D
	 * virtual cockpit mode (for example in response to the user switching
	 * cockpit modes with F8).
	 * \param id virtual cockpit identifier (>= 0)
	 * \return true if the vessel supports the requested virtual cockpit,
	 *   false otherwise.
	 * \default None, returning false (i.e. virtual cockpit mode not
	 *   supported).
	 * \note Multiple virtual cockpit camera positions (e.g. for pilot and
	 *   co-pilot) can be defined. In this case, the body of clbkLoadVC
	 *   should examine the value of id and set the VC parameters
	 *   accordingly. Multiple positions are defined by specifying the
	 *   neighbour positions of the current position via a call to
	 *   oapiVCSetNeighbours.
	 * \note In the body of this function the module should define MFD
	 *   display targets (with oapiVCRegisterMFD) and other active areas
	 *   (with oapiVCRegisterArea) for the requested virtual cockpit.
	 * \sa clbkLoadGenericCockpit, clbkLoadPanel, oapiVCSetNeighbours,
	 *   oapiVCRegisterArea, \ref progflow1
	 */
	virtual bool clbkLoadVC (int id);

	/**
	 * \brief Mouse event notification for 3-D virtual cockpit views.
	 *
	 * Called when a mouse-activated virtual cockpit area receives a mouse
	 * event.
	 * \param id area identifier
	 * \param event mouse event (see \ref panel_mouse)
	 * \param p parameter vector (area type-dependent, see notes)
	 * \return The function should return true if it processes the event,
	 *   false otherwise.
	 * \default None, returning false.
	 * \note To generate a mouse-activated area in a virtual cockpit, you
	 *   must do the following when registering the area during clbkLoadVC:
	 *   - register the area with a call to oapiVCRegisterArea with a
	 *     mouse mode other than PANEL_MOUSE_IGNORE.
	 *   - define a mouse-click area in the vessel's local frame. Use one
	 *     of the oapiVCRegisterAreaClickmode_XXX functions. You can define
	 *     spherical or quadrilateral click areas.
	 * \note Parameter p returns information about the mouse position at
	 *   the mouse event. The type of information returned depends on the
	 *   area type for which the event was generated:
	 *   - spherical area:
	 *     - p.x is distance of mouse event from area centre
	 *     - p.y and p.z not used
	 *   - quadrilateral area:
	 *     - p.x and p.y are the area-relative mouse x and y positions (top
	 *       left = (0,0), bottom right = (1,1)
	 *     - p.z not used
	 * \sa clbkLoadVC, clbkPanelMouseEvent, oapiVCRegisterArea, \ref progflow1
	 */
	virtual bool clbkVCMouseEvent (int id, int event, VECTOR3 &p);

	/**
	 * \brief Redraw event notification for 3-D virtual cockpit views.
	 *
	 * Called when a registered virtual cockpit area needs to be redrawn.
	 * \param id area identifier
	 * \param event redraw event (see \ref panel_redraw)
	 * \param surf associated texture handle
	 * \return The function should return true if it processes the event,
	 *   false otherwise.
	 * \default None, returning false.
	 * \note To allow an area of the virtual cockpit to be redrawn
	 *   dynamically, the area must be registered with oapiVCRegisterArea
	 *   during clbkLoadVC, using a redraw mode other than
	 *   PANEL_REDRAW_NEVER.
	 * \note When registering the area with oapiVCRegisterArea, you must
	 *   also provide a handle to the texture onto which the redrawn
	 *   surface is mapped. This texture must be part of the virtual
	 *   cockpit mesh, and it must be listed in the mesh file with the 'D'
	 *   ("dynamic") flag (see 3DModel.pdf).
	 * \note "Redrawing" an area is not limited to dynamically updating
	 *   textures. It may also involve mesh transforms (e.g. to animate
	 *   levers and switches rendered in 3D).
	 * \sa \ref progflow1
	 */
	virtual bool clbkVCRedrawEvent (int id, int event, SURFHANDLE surf);
};

// ======================================================================
// class VESSEL3
// ======================================================================
/**
 * \brief Callback extensions to the VESSEL class
 *
 * The VESSEL3 class extends VESSEL2 with additional functionality.
 * Developers should use this class for new projects. Existing vessel
 * addons can make use of the new features by switching the base
 * class from VESSEL2 to VESSEL3.
 */
// ======================================================================
// NOTE: Do NOT add or remove methods to this class, or re-arrange the
// order of the exisiting methods, to avoid breaking addons (incompatible
// virtual tables)!
// ======================================================================

class OAPIFUNC VESSEL3: public VESSEL2 {
public:
	/**
	 * \brief Creates a VESSEL3 interface for a vessel object.
	 * \sa VESSEL2
	 */
	VESSEL3 (OBJHANDLE hVessel, int fmodel=1);

	/**
	 * \brief Set the background surface for a 2-D instrument panel.
	 * \param hPanel panel handle
	 * \param hSurf array of surface handles
	 * \param nsurf number of surfaces
	 * \param hMesh mesh handle defining the billboard geometry
	 * \param width panel width [pixel]
	 * \param height panel height [pixel]
	 * \param baseline base line for edge attachment
	 * \param scrollflag panel attachment and scrolling bitflags
	 * \return Always returns 0.
	 * \note This method should be applied in the body of \ref clbkLoadPanel2D.
	 * \note The mesh defines the size and layout of the billboard mesh used for
	 *   rendering the panel surface. Its vertex coordinates are interpreted as
	 *   transformed, i.e. in terms of screen coordinates (pixels). The z-coordinate
	 *   should be zero. Normals are ignored. Texture coordinates define which part
	 *   of the surfaces are rendered.
	 * \note The groups are rendered in the order they appear in the mesh. Later
	 *   groups cover earlier ones. Therefore the groups should be arranged from
	 *   backmost to frontmost elements.
	 * \note In the simplest case, the mesh consists of a single rectangular area
	 *   (4 nodes, 2 triangles) and a single surface, but can be more elaborate.
	 * \note The texture indices of the mesh groups (TexIdx) are interpreted as
	 *   indices into the hSurf list (zero-based).
	 * \note This method increases the reference counters for the surfaces, so the
	 *   caller should release them at some point.
	 * \note The surfaces can contain an alpha channel to handle transparency.
	 */
	int SetPanelBackground (PANELHANDLE hPanel, SURFHANDLE *hSurf, DWORD nsurf, MESHHANDLE hMesh, DWORD width, DWORD height, DWORD baseline = 0, DWORD scrollflag = 0);

	/**
	 * \brief Set scaling factors for 2-D instrument panel.
	 * \param hPanel panel handle
	 * \param defscale default scale factor
	 * \param extscale additional scale factor
	 * \return Always returns 0.
	 * \note The scaling factors define the scaling between mesh coordinates
	 *   and screen pixels.
	 * \note \a defscale is the default factor, \a extscale is an additional scale
	 *   which can be selected by the user via the mouse wheel.
	 * \note Examples: scale=1: one mesh unit corresponds to one screen pixel,
	 *   scale=viewW/panelW: panel fits screen width
	 */
	int SetPanelScaling (PANELHANDLE hPanel, double defscale, double extscale);

	/**
	 * \brief Define an MFD display in the panel mesh.
	 * \param hPanel panel handle
	 * \param MFD_id MFD identifier (>= 0)
	 * \param nmesh panel mesh index (>= 0)
	 * \param ngroup mesh group index (>= 0)
	 * \return Always returns 0.
	 * \note This method reserves a mesh group for rendering the contents of
	 *   an MFD display. The group should define a square area (typically
	 *   consisting of 4 nodes and 2 triangles) with appropriate texture
	 *   coordinates. When rendering the panel, the texture for this group is
	 *   set to the current contents of the MFD display.
	 * \note The order of mesh groups defines the rendering order. To render
	 *   the MFD display on top of the panel, define it as the last group in
	 *   the mesh. Alternatively, the MFD can be rendered first, if the panel
	 *   texture contains a transparent area through which to view the MFD.
	 */
	int RegisterPanelMFDGeometry (PANELHANDLE hPanel, int MFD_id, int nmesh, int ngroup);

	/**
	 * \brief Register an area of the panel to receive mouse and redraw events.
	 * \deprecated This method has been superseded by VESSEL4::RegisterPanelArea.
	 * \param hPanel panel handle
	 * \param id area identifier
	 * \param pos area boundary coordinates (mesh coordinates)
	 * \param texpos area boundary (texture coordinates)
	 * \param draw_event event flags for redraw event triggers (see \ref panel_redraw)
	 * \param mouse_event event flags for mouse event triggers (see \ref panel_mouse)
	 * \param bkmode flag for texture background provided to redraw callback function (see \ref panel_map)
	 * \return Always returns 0.
	 * \note This method activates a rectangular area of the panel for receiving mouse
	 *    and redraw events.
	 * \note \a pos specifies the borders of the area in 'logical' coordinates
	 *   (0,0,width,height) as specified by \ref SetPanelBackground. Registered mouse
	 *   events within this area will trigger a call to \ref VESSEL2::clbkPanelMouseEvent.
	 * \note If the area needs to be able to update the panel texture, it should pass
	 *   an appropriate redraw flag in \a draw_event, and specify the texture coordinates
	 *   of the redraw area in \a texpos.
	 * \note If the panel contains multiple background textures, only the first texture
	 *   can be redrawn with this function. To redraw other textures in the background
	 *   texture array, use VESSEL4::RegisterPanelArea instead.
	 * \note For backward compatibility, this method automatically adds the
	 *   PANEL_REDRAW_GDI and PANEL_REDRAW_SKETCHPAD flags to \a draw_event. If GDI
	 *   and/or Sketchpad access to the area drawing surface is not required, using
	 *   VESSEL4::RegisterPanelArea can improve graphics performance.
	 * \sa VESSEL4::RegisterPanelArea
	 */
	int RegisterPanelArea (PANELHANDLE hPanel, int id, const RECT &pos, const RECT &texpos, int draw_event, int mouse_event, int bkmode);

	/**
	 * \brief Register an area of the panel to receive mouse and redraw events.
	 * \param hPanel panel handle
	 * \param id area identifier
	 * \param pos area boundary coordinates (mesh coordinates)
	 * \param draw_event event flags for redraw event triggers (see \ref panel_redraw)
	 * \param mouse_event event flags for mouse event triggers (see \ref panel_mouse)
	 * \param surf surface handle passed to the redraw callback function
	 * \param context user-defined data passed to the mouse and redraw callback functions
	 * \return Always returns 0.
	 * \note This version passes the provided surface handle directly to the redraw
	 *   callback, rather making a copy of the area. This is useful if the area
	 *   either doesn't need to modify any surfaces, or blits parts of the same
	 *   surface (e.g. a texture that contains both the panel background and various
	 *   elements (switches, dials, etc.) to be copied on top.
	 * \note Since the surface returned to the redraw function is not restricted to
	 *   the registered area, it is the responsibility of the caller not to draw
	 *   outside the area.
	 * \note The area boundaries defined in \a pos are only used for generating
	 *   mouse events. If the area does not process mouse events (PANEL_MOUSE_IGNORE),
	 *   the pos parameter is ignored.
	 * \note The PANEL_REDRAW_GDI and PANEL_REDRAW_SKETCHPAD flags can not be used in
	 *   the \a draw_event parameter. If GDI or Sketchpad access is required during
	 *   redraw events, either the surface \a surf must have been created with the
	 *   appropriate attributes, or VESSEL4::RegisterPanelArea should be used instead.
	 * \sa VESSEL4::RegisterPanelArea, oapiCreateSurfaceEx
	 */
	int RegisterPanelArea (PANELHANDLE hPanel, int id, const RECT &pos, int draw_event, int mouse_event, SURFHANDLE surf = NULL, void *context = NULL);

	/**
	 * \brief Mouse event notification for 2-D panel views.
	 *
	 * Called when a mouse-activated panel area receives a mouse event.
	 * \param id panel area identifier
	 * \param event mouse event (see \ref panel_mouse)
	 * \param mx,my relative mouse position in area at event
	 * \param context user-supplied pointer to context data (defined in \ref RegisterPanelArea)
	 * \return The function should return true if it processes the event,
	 *   false otherwise.
	 * \default None, returns false.
	 * \note If a vessel class overloads this method, it should return true. On a
	 *   \e false return, Orbiter will try VESSEL2::clbkPanelMouseEvent instead.
	 * \note Mouse events are only sent for areas which requested
	 *   notification during definition (see RegisterPanelArea).
	 * \sa RegisterPanelArea, \ref progflow1
	 */
	virtual bool clbkPanelMouseEvent (int id, int event, int mx, int my, void *context);

	/**
	 * \brief Redraw event notification for 2-D panel views.
	 *
	 * Called when a registered panel area needs to be redrawn.
	 * \param id panel area identifier
	 * \param event redraw event (see \ref panel_redraw)
	 * \param surf area surface handle
	 * \param context user-supplied pointer to context data (defined in \ref RegisterPanelArea)
	 * \return The function should return true if it processes the event,
	 *   false otherwise.
	 * \default None, returns false.
	 * \note This callback function is only called for areas which were
	 *   not registered with the PANEL_REDRAW_NEVER flag.
	 * \note If a vessel class overloads this method, it should return true. On a
	 *   \e false return, Orbiter will try VESSEL2::clbkPanelRedrawEvent instead.
	 * \note All redrawable panel areas receive a PANEL_REDRAW_INIT redraw
	 *   notification when the panel is created, in addition to any
	 *   registered redraw notification events.
	 * \note The surface handle surf contains either the current area
	 *   state, or the area background, depending on the flags passed
	 *   during area registration.
	 * \note The surface handle may be used for blitting operations, or to
	 *   receive a Windows device context (DC) for Windows-style redrawing
	 *   operations.
	 * \sa RegisterPanelArea, oapiGetDC, oapiReleaseDC, oapiTriggerPanelRedrawArea,
	 *   , \ref progflow1
	 */
	virtual bool clbkPanelRedrawEvent (int id, int event, SURFHANDLE surf, void *context);

	/**
	 * \brief Generic multi-purpose callback function.
	 * \param msgid message identifier (see \ref vmsg)
	 * \param prm message parameter
	 * \param context pointer to additional message data
	 * \return Result flag.
	 */
	virtual int clbkGeneric (int msgid = 0, int prm = 0, void *context = NULL);

	/**
	 * \brief Request for a 2D instrument panel definition in cockpit view.
	 * \param id panel identifier (>= 0)
	 * \param hPanel panel handle
	 * \param viewW viewport width [pixel]
	 * \param viewH viewport height [pixel]
	 * \return The function should return \e true if it supports the requested
	 *   panel, false otherwise.
	 * \default None, returns false.
	 * \note This method replaces \ref VESSEL2::clbkLoadPanel. It defines the
	 *   panels via SURFHANDLES instead of bitmaps.
	 * \sa \ref progflow1
	 */
	virtual bool clbkLoadPanel2D (int id, PANELHANDLE hPanel, DWORD viewW, DWORD viewH);

	/**
	 * \brief HUD redraw notification
	 *
	 * Called when the vessel's head-up display (HUD) needs to be redrawn
	 * (usually at each time step, unless the HUD is turned off).
	 * Overwriting this function allows to implement vessel-specific
	 * modifications of the HUD display (or to suppress the HUD altogether).
	 * \param mode HUD mode (see HUD_* constants in OrbiterAPI.h)
	 * \param hps pointer to a HUDPAINTSPEC structure (see notes)
	 * \param skp drawing context instance
	 * \return Overloaded methods should return \e true. If the return value
	 *   is \e false, orbiter assumes that this method is disabled and will 
	 *   try VESSEL2::clbkDrawHUD.
	 * \default Draws a standard HUD display with Orbiter's default display
	 *   layout and returns \e true.
	 * \note If a vessel overwrites this method, Orbiter will draw the 
	 *   default HUD only if the base class VESSEL3::clbkDrawHUD is called.
	 * \note hps points to a HUDPAINTSPEC structure containing information
	 *   about the HUD drawing surface. It has the following format:
	 * \code
	 * typedef struct {
	 *   int W, H;
     *   int CX, CY;
     *   double Scale;
     *   int Markersize;
	 * } HUDPAINTSPEC;
	 * \endcode
	 *   where W and H are width and height of the HUD drawing surface in
	 *   pixels, CX and CY are the x and y coordinates of the HUD centre
	 *   (the position of the "forward marker", which is not guaranteed to
	 *   be in the middle of the drawing surface or even within the drawing
	 *   surface!), Scale represents an angular aperture of 1 deg. expressed in
	 *   HUD pixels, and Markersize is a "typical" size which can be used
	 *   to scale objects like direction markers.
	 * \note The device context passed to clbkDrawHUD contains the
	 *   appropriate settings for the current HUD display (font, pen,
	 *   colours). If you need to change any of the GDI settings, make sure
	 *   to restore the defaults before calling the base class clbkDrawHUD.
	 *   Otherwise the default display will be corrupted.
	 * \note clbkDrawHUD can be used to implement entirely new vessel-
	 *   specific HUD modes. In this case, the module would maintain its
	 *   own record of the current HUD mode, and ignore the mode parameter
	 *   passed to clbkDrawHUD.
	 * \note In glass cockpit and 2-D panel mode, the HUD display can be a
	 *   combination of drawn elements (via clbkDrawHUD) and rendered elements
	 *   (via \ref clbkRenderHUD). In VC mode, the HUD is always drawn.
	 * \note To disable all default HUD display elements, a derived vessel
	 *   should overload both clbkDrawHUD and clbkRenderHUD.
	 * \sa clbkRenderHUD, Section hudmode for a list of default mode identifiers,
	 *   \ref progflow1
	 */
	virtual bool clbkDrawHUD (int mode, const HUDPAINTSPEC *hps, oapi::Sketchpad *skp);

	/**
	 * \brief HUD render notification
	 *
	 * Called when the vessel's head-up display (HUD) needs to be rendered
	 * (usually at each time step, unless the HUD is turned off).
	 * Overwriting this function allows to implement vessel-specific
	 * modifications of the HUD display (or to suppress the HUD altogether).
	 * \param mode HUD mode (see HUD_* constants in OrbiterAPI.h)
	 * \param hps pointer to a HUDPAINTSPEC structure
	 * \param hDefaultTex handle for default HUD texture
	 * \default Renders a standard HUD display with Orbiter's default display
	 *   layout.
	 * \note This function is only called in glass cockpit or 2-D panel mode,
	 *   not in VC (virtual cockpit mode).
	 * \note In glass cockpit or 2-D panel mode, the programmer has a choice of
	 *   using clbkRenderHUD or \ref clbkDrawHUD to display vessel-specific HUD
	 *   elements. The use of clbkRenderHUD is preferred, because it provides
	 *   smoother animation, better performance and is better supported by
	 *   external render engines.
	 * \note To disable all default HUD display, a derived vessel class should
	 *   overload both clbkRenderHUD and \ref clbkDrawHUD.
	 * \note To render custom HUD elements, the \ref oapiRenderHUD function should
	 *   be called from within this callback function.
	 * \sa clbkDrawHUD, oapiRenderHUD, Section hudmode for a list of default mode identifiers,
	 *   \ref progflow1
	 */
	virtual void clbkRenderHUD (int mode, const HUDPAINTSPEC *hps, SURFHANDLE hDefaultTex);

	/**
	 * \brief Returns force due to radiation pressure.
	 * \param[in] mflux momentum flux vector [N/m^2] at current spacecraft position,
	 *    transformed into vessel frame
	 * \param[out] F radiation force vector [<b>N</b>] in vessel frame
	 * \param[out] pos force attack point [<b>m</b>] in vessel frame
	 * \default Sets F = mflux * size^2 * a, where a (albedo coefficient) is fixed
	 *   to 1.5. Sets pos = (0,0,0). This simple formula ignores any attitude-dependent
	 *   variations in surface area, and any non-radial force components due to oblique
	 *   reflections. Does not induce any torque. For more sophisticated treatment,
	 *   vessels should re-implement this method.
	 * \note This method is called by orbiter when perturbation forces due to
	 *   radiation pressure need to be evaluated. The implementation should take
	 *   into account geometric factors (cross sections), surface factors (absorption,
	 *   reflection) and spacecraft attitude relative to the sun.
	 * \note The momentum flux parameter, \a mflux, takes into account shadow
	 *   effects from the closest planet, or from the closest moon and its parent
	 *   planet, if applicable.
	 * \note If the returned force attack point \a pos is not set to the centre
	 *   of gravity, (0,0,0), then a torque may be induced as well as a linear
	 *   force.
	 * \note If the vessel contains multiple distinct surfaces, the returned
	 *   force should be the vector sum of all individual contributions, and the
	 *   returned position should be the weighted barycentre of all individual
	 *   contributions w.r.t. the vessel centre of gravity.
	 */
	virtual void clbkGetRadiationForce (const VECTOR3 &mflux, VECTOR3 &F, VECTOR3 &pos);
};


// ======================================================================
// class VESSEL4
// ======================================================================
/**
 * \brief Extensions to the VESSEL class
 *
 * The VESSEL4 class extends VESSEL3 with additional functionality.
 * Developers should use this class for new projects. Existing vessel
 * addons can make use of the new features by switching the base
 * class to VESSEL4.
 */
// ======================================================================
// NOTE: Do NOT add or remove methods to this class, or re-arrange the
// order of the exisiting methods, to avoid breaking addons (incompatible
// virtual tables)!
// ======================================================================

class OAPIFUNC VESSEL4: public VESSEL3 {
public:
	/**
	 * \brief Creates a VESSEL4 interface for a vessel object.
	 * \sa VESSEL3
	 */
	VESSEL4 (OBJHANDLE hVessel, int fmodel=1);

	/**
	 * \brief Register an area of the panel to receive mouse and redraw events.
	 * \param hPanel panel handle
	 * \param id area identifier
	 * \param pos area boundary coordinates (mesh coordinates)
	 * \param texidx background texture index
	 * \param texpos area boundary (texture coordinates)
	 * \param draw_event event flags for redraw event triggers (see \ref panel_redraw)
	 * \param mouse_event event flags for mouse event triggers (see \ref panel_mouse)
	 * \param bkmode flag for texture background provided to redraw callback function (see \ref panel_map)
	 * \return Always returns 0.
	 * \note This method activates a rectangular area of the panel for receiving mouse
	 *    and redraw events.
	 * \note \a pos specifies the borders of the area in 'logical' coordinates
	 *   (0,0,width,height) as specified by \ref SetPanelBackground. Registered mouse
	 *   events within this area will trigger a call to \ref VESSEL2::clbkPanelMouseEvent.
	 * \note \a texidx is the index of the panel background texture the area texture should
	 *   be copied into, in the order the textures were specified in the array passed to
	 *   VESSEL3::SetPanelBackground. If only a single texture is used for the panel,
	 *   \a texidx should be set to 0. If the area doesn't need to be redrawn
	 *   (PANEL_REDRAW_NEVER), this parameter is ignored.
	 * \note If the area texture should allow GDI and/or Sketchpad access during redraw
	 *   events, the PANEL_REDRAW_GDI and/or PANEL_REDRAW_SKETCHPAD flags should be added
	 *   to draw_event. If only blitting access is required, these flags should be omitted
	 *   for improved performance.
	 */
	int RegisterPanelArea (PANELHANDLE hPanel, int id, const RECT &pos, int texidx, const RECT &texpos, int draw_event, int mouse_event, int bkmode);

	using VESSEL3::RegisterPanelArea; // keep previous interface valid

	/**
	 * \brief Register a user-defined MFD mode for the vessel.
	 * \param spec MFD mode specifications
	 * \note This method is similar to the global oapiRegisterMFDMode function,
	 *   but it registers the MFD mode only for an individual vessel instance.
	 *   This allows to create vessel-specific MFD modes directly in the vessel
	 *   module. Typically this method would be called in the vessel constructor.
 	 * \note MFDMODESPECEX is a struct defining the parameters of the new mode:
	 * \code
	 * typedef struct {
	 *   char *name;    // points to the name of the new mode
	 *   DWORD key;     // mode selection key
	 *   void *context; // mode-specific context pointer
	 *   int (*msgproc)(UINT,UINT,WPARAM,LPARAM);   // address of MFD message parser
	 * } MFDMODESPEC; \endcode
	 * \note The mode identifier retrieved by oapiGetMFDMode() for MFD modes
	 *   registered by this method starts with 1000 for the first registered mode
	 *   and is incremented by 1 for each subsequently registered mode.
	 * \sa VESSEL4::UnregisterMFDMode, oapiRegisterMFDMode
	 */
	int RegisterMFDMode (const MFDMODESPECEX &spec);

	/**
	 * \brief Unregister a previously registered vessel-specific MFD mode.
	 * \param mode mode identifier, as returned by \ref VESSEL4::RegisterMFDMode
	 * \return \e true on success (mode was successfully unregistered).
	 * \sa VESSEL4::RegisterMFDMode
	 */
	bool UnregisterMFDMode (int mode);

	/**
	 * \brief Processing of navigation autopilot programmes
	 * \param mode Bit-flags for active nav programmes (see \ref nav_bitflag)
	 * \return Modified nav programme bitflags (see notes)
	 * \default Does nothing and returns \p mode (i.e. leaves all navmode processing
	 *   to the default Orbiter core routines.
	 * \note This method is called at each frame while at least one nav programme
	 *   is active. It is only called once per frame even if multiple programmes are
	 *   active. Check the \p mode parameter to see which.
	 * \note The module is free to process all, a subset, or none of the active
	 *   programmes. The return value indicates to Orbiter which of the programmes
	 *   have been processed: clear the flags for all processed programmes, and leave
	 *   the flags for any skipped programmes.
	 * \note You cannot set any flags in the return value that were not set already
	 *   in the input parameter. Activating/deactivating navmodes should be done via
	 *   VESSEL::ActivateNavmode, VESSEL::DeactivateNavmode, VESSEL::ToggleNavmode
	 */
	virtual int clbkNavProcess (int mode);
};

// ======================================================================
// class AnimState
// Auxiliary class for defining animation states
// ======================================================================

class AnimState {
public:
	enum Action {STOPPED, CLOSED, OPEN, CLOSING, OPENING} action;
	double pos;
	void Set (Action a, double p) { action = a, pos = p; }
	bool Move (double dp) {
		if (!Moving()) return false;
		if (Closing()) {
			if ((pos = (std::max)(0.0, pos - dp)) == 0.0) action = CLOSED;
		} else {
			if ((pos = (std::min)(1.0, pos + dp)) == 1.0) action = OPEN;
		}
		return true;
	}
	bool Moving() const { return action >= CLOSING; }
	bool Static() const { return action < CLOSING; }
	bool Stopped() const { return action == STOPPED; }
	bool Closed() const { return action == CLOSED; }
	bool Open() const { return action == OPEN; }
	bool Closing() const { return action == CLOSING; }
	bool Opening() const { return action == OPENING; }
	friend OAPIFUNC void WriteScenario_state (FILEHANDLE f, char *tag, const AnimState &s);
	friend OAPIFUNC void sscan_state (char *str, AnimState &s);
};

#endif // !__VESSELAPI_H

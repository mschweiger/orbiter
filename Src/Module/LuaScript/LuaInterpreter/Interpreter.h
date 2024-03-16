// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef __INTERPRETER_H
#define __INTERPRETER_H

extern "C" {
#include "Lua/lua.h"
#include "Lua/lualib.h"
#include "Lua/lauxlib.h"
}

#include "OrbiterAPI.h"
#include "VesselAPI.h" // for TOUCHDOWNVTX
#include <semaphore>
#include <optional>

#define PRMTP_NIL           0x01
#define PRMTP_NUMBER        0x02
#define PRMTP_BOOLEAN       0x04
#define PRMTP_STRING        0x08
#define PRMTP_LIGHTUSERDATA 0x10
#define PRMTP_TABLE         0x20
#define PRMTP_VECTOR        0x40
#define PRMTP_MATRIX        0x80

#define ASSERT_SYNTAX(cond,msg) { if(!(cond)) { char cbuf[1024]; sprintf (cbuf, "%s: %s", __FUNCTION__+13, msg); term_strout(L,cbuf,true); return 0; } }
#define ASSERT_FUNCPRM(L,idx,tp) { if (!AssertPrmtp(L,__FUNCTION__,idx,idx,tp)) return 0; }

#define ASSERT_PRM(L,idx,tp)     { if (!AssertPrmtp(L,__FUNCTION__,idx,idx,tp)) return 0; }
#define ASSERT_NUMBER(L,idx)           ASSERT_PRM(L,idx,PRMTP_NUMBER)
#define ASSERT_VECTOR(L,idx)           ASSERT_PRM(L,idx,PRMTP_VECTOR)
#define ASSERT_STRING(L,idx)           ASSERT_PRM(L,idx,PRMTP_STRING)
#define ASSERT_LIGHTUSERDATA(L,idx)    ASSERT_PRM(L,idx,PRMTP_LIGHTUSERDATA)
#define ASSERT_FILEHANDLE(L,idx)       ASSERT_PRM(L,idx,PRMTP_LIGHTUSERDATA)
#define ASSERT_TABLE(L,idx)            ASSERT_PRM(L,idx,PRMTP_TABLE)
#define ASSERT_BOOLEAN(L,idx)          ASSERT_PRM(L,idx,PRMTP_BOOLEAN)
#define ASSERT_MATRIX(L,idx)           ASSERT_PRM(L,idx,PRMTP_MATRIX)

#define ASSERT_MTDPRM(L,idx,tp)  { if (!AssertPrmtp(L,__FUNCTION__,idx,idx-1,tp)) return 0; }
#define ASSERT_MTDNUMBER(L,idx)        ASSERT_MTDPRM(L,idx,PRMTP_NUMBER)
#define ASSERT_MTDVECTOR(L,idx)        ASSERT_MTDPRM(L,idx,PRMTP_VECTOR)
#define ASSERT_MTDSTRING(L,idx)        ASSERT_MTDPRM(L,idx,PRMTP_STRING)
#define ASSERT_MTDLIGHTUSERDATA(L,idx) ASSERT_MTDPRM(L,idx,PRMTP_LIGHTUSERDATA)
#define ASSERT_MTDTABLE(L,idx)         ASSERT_MTDPRM(L,idx,PRMTP_TABLE)
#define ASSERT_MTDBOOLEAN(L,idx)       ASSERT_MTDPRM(L,idx,PRMTP_BOOLEAN)
#define ASSERT_MTDMATRIX(L,idx)        ASSERT_MTDPRM(L,idx,PRMTP_MATRIX)

#define ASSERT_MINPRMCOUNT(L,n) ASSERT_SYNTAX(lua_gettop(L) >= n, "Too few arguments")

#ifdef INTERPRETER_IMPLEMENTATION
#define INTERPRETERLIB DLLEXPORT
#else
#define INTERPRETERLIB DLLIMPORT
#endif

class VESSEL;
class MFD2;

struct AirfoilContext {
	lua_State *L;
	char funcname[128];
};


// ======================================================================
// ======================================================================
// Nonmember functions

// converts the vector at stack position 'idx' into a VECTOR3
INTERPRETERLIB VECTOR3 lua_tovector (lua_State *L, int idx);

// ======================================================================
// class Interpreter

class INTERPRETERLIB Interpreter {
public:
	Interpreter ();
	virtual ~Interpreter ();

	/**
	 * \brief Set up the interpreter (load libraries etc.)
	 */
	virtual void Initialise ();

	/**
	 * \brief Return the Lua object
	 */
	lua_State *GetState() { return L; }

	/**
	 * \brief Returns interpreter status.
	 * \return 0=normal, 1=kill pending
	 */
	int Status () const;

	/**
	 * \brief Returns interpreter execution status.
	 * \return \e true if interpreter is busy (in the process of running a
	 *   command or script), \e false if it is waiting for intput.
	 */
	bool IsBusy () const;

	/**
	 * \brief Returns the number of background jobs active during idle phase.
	 * \return number of background jobs
	 * \note A command may create background jobs that are still active after
	 *   the command returns. The interpreter idle loop continues processing
	 *   the remaining jobs until all are finished, or until a new command
	 *   is entered which takes over control of the background jobs.
	 */
	inline int nJobs () const { return jobs; }

	/**
	 * \brief Request interpreter termination.
	 * \note This sets the interpreter Status() to 1 (kill pending). It is
	 *   up to the client to delete the interpreter instance and clean up
	 *   (terminate interpreter thread etc.)
	 */
	void Terminate ();
	
	void PostStep (double simt, double simdt, double mjd);

	/**
	 * \brief Handover execution to the interpreter thread.
	 * \note This is called by the orbiter thread to signal the interpreter
	 *   thread to run. The orbiter thread blocks until the interpreter has
	 *   done its work.
	 */
	void StepInterpreter();

	/**
	 * \brief Wait for thread execution.
	 * \note This is called by the interpreter thread when it's
	 *   waiting to regain execution control.
	 */
	void WaitForStep ();

	/**
	 * \brief Release thread execution.
	 * \note This is called by the interpreter thread after
	 *   finishing a cycle to hand control over to the orbiter thread.
	 */
	void StepDone ();

	/**
	 * \brief Define functions for interfacing with Orbiter API
	 */
	virtual void LoadAPI ();

	virtual void LoadVesselAPI ();

	virtual void LoadMFDAPI ();

	virtual void LoadLightEmitterMethods ();

	virtual void LoadSketchpadAPI ();

	/**
	 * \brief Run the interpreter initialisation script
	 */
	virtual void LoadStartupScript ();

	/**
	 * \brief Executes a command or script.
	 * \param chunk command line string
	 * \param n string length
	 * \return Execution status as returned by lua_pcall (0=no error)
	 */
	virtual int RunChunk (const char *chunk, int n);

	/**
	 * \brief Copies a string to the terminal.
	 * \param str string to be displayed.
	 * \note Terminal-type derived classes should implement this method.
	 * \note Default action: none.
	 */
	virtual void term_strout (const char *str, bool iserr=false) {}

	/**
	 * \brief Copies the string on top of the stack to the terminal.
	 * \param L Lua interpreter instance
	 * \note Default action: Passes the string on top of the stack to term_echo().
	 */
	virtual void term_out (lua_State *L, bool iserr=false);

	/**
	* \brief Clears the terminal.
	*/
	virtual void term_clear () {}

	/**
	 * \brief Push an MFD object onto the stack
	 * \param L Lua interpreter instance
	 * \param mfd pointer to MFD object
	 */
	static void lua_pushmfd (lua_State *L, MFD2 *mfd);

	/**
	 * \brief Push a light source object onto the stack
	 * \param L Lua interpreter instance
	 * \param le pointer to LightEmitter object
	 */
	static void lua_pushlightemitter (lua_State *L, const LightEmitter *le);

	/**
	 * \brief Push a Sketchpad object onto the stack
	 * \param L Lua interpreter instance
	 * \param skp pointer to Sketchpad object
	 */
	static void lua_pushsketchpad (lua_State *L, oapi::Sketchpad *skp);

	void term_setverbosity (int level) { term_verbose = level; }

	std::optional<int> exitCode;

protected:
	lua_State *L;         // Lua main context

	/**
	 * \brief Load screen annotation methods.
	 */
	void LoadAnnotationAPI ();

	static bool InitialiseVessel (lua_State *L, VESSEL *v);
	static bool LoadVesselExtensions (lua_State *L, VESSEL *v);

	// terminal functions (to be implemented by terminal-based subclasses)
	void term_echo (lua_State *L, int level=1); // terminal verbose echo
	int term_verbose;                       // terminal verbosity level
	bool is_term;                           // interpreter attached to a terminal
	static void term_strout (lua_State *L, const char *str, bool iserr=false);
	static void warn_obsolete(lua_State *L, const char *funcname);

	static int AssertPrmtp(lua_State *L, const char *fname, int idx, int prm, int tp);

	// assertion functions (general)
	static int AssertPrmType(lua_State *L, int idx, int tp, const char *funcname);

	// assertion functions (methods)
	static int AssertMtdPrmType(lua_State *L, int idx, int tp, const char *funcname);
	static int AssertMtdMinPrmCount(lua_State *L, int n, const char *funcname);
	static int AssertMtdNumber(lua_State *L, int idx, const char *funcname);
	static int AssertMtdHandle(lua_State *L, int idx, const char *funcname);

	// suspend script execution for one cycle
	void frameskip (lua_State *L);

	// extract interpreter pointer from lua state
	static Interpreter *GetInterpreter (lua_State *L);

	// Extended version of the built-in lua_tostring function:
	// This also handles vector and nil entries.
	static const char *lua_tostringex (lua_State *L, int idx, char *cbuf = 0);

	// pushes vector 'vec' into a table on top of the stack
	static void lua_pushvector (lua_State *L, const VECTOR3 &vec);

	// returns 1 if stack entry idx is a vector, 0 otherwise
	static int lua_isvector (lua_State *L, int idx);

	// pushes matrix 'mat' into a table on top of the stack
	static void lua_pushmatrix (lua_State *L, const MATRIX3 &mat);

	// converts the matrix at stack position 'idx' into a MATRIX3
	static MATRIX3 lua_tomatrix (lua_State *L, int idx);

	// returns 1 if stack entry idx is a matrix, 0 otherwise
	static int lua_ismatrix (lua_State *L, int idx);

	static COLOUR4 lua_torgba (lua_State *L, int idx);

	// pops an OBJHANDLE from the stack
	static OBJHANDLE lua_toObject (lua_State *L, int idx=-1);

	// pushes a vessel object on the stack
	static void lua_pushvessel (lua_State *L, VESSEL *v);

	// Pops a VESSEL interface from the stack and returns it.
	// A NULL return indicates an invalid data type at the specified stack position,
	// but a nonzero return does not guarantee a valid vessel pointer
	static VESSEL *lua_tovessel (lua_State *L, int idx=-1);

	// type extraction with checks
	static VESSEL *lua_tovessel_safe (lua_State *L, int idx, const char *funcname);

	static int lua_tointeger_safe (lua_State *L, int idx, const char *funcname);
	static double lua_tonumber_safe (lua_State *L, int idx, const char *funcname);
	static bool lua_toboolean_safe (lua_State *L, int idx, const char *funcname);
	static const char *lua_tostring_safe (lua_State *L, int idx, const char *funcname);
	static void *lua_tolightuserdata_safe (lua_State *L, int idx, const char *funcname);
	static VECTOR3 lua_tovector_safe (lua_State *L, int idx, const char *funcname);
	static MATRIX3 lua_tomatrix_safe (lua_State *L, int idx, const char *funcname);
	static double lua_field_tonumber_safe (lua_State *L, int idx, const char *fieldname, const char *funcname);
	static void *lua_field_tolightuserdata_safe (lua_State *L, int idx, const char *fieldname, const char *funcname);
	static VECTOR3 lua_field_tovector_safe (lua_State *L, int idx, const char *fieldname, const char *funcname);

	static int luamtd_tointeger_safe (lua_State *L, int idx, const char *funcname);
	static double luamtd_tonumber_safe (lua_State *L, int idx, const char *funcname);
	static bool luamtd_toboolean_safe (lua_State *L, int idx, const char *funcname);
	static const char *luamtd_tostring_safe (lua_State *L, int idx, const char *funcname);
	static void *luamtd_tolightuserdata_safe (lua_State *L, int idx, const char *funcname);
	static VECTOR3 luamtd_tovector_safe (lua_State *L, int idx, const char *funcname);
	static MATRIX3 luamtd_tomatrix_safe (lua_State *L, int idx, const char *funcname);
	static double luamtd_field_tonumber_safe (lua_State *L, int idx, const char *fieldname, const char *funcname);
	static void *luamtd_field_tolightuserdata_safe (lua_State *L, int idx, const char *fieldname, const char *funcname);
	static VECTOR3 luamtd_field_tovector_safe (lua_State *L, int idx, const char *fieldname, const char *funcname);


	// pops an MFD2 interface from the stack
	static MFD2 *lua_tomfd (lua_State *L, int idx=-1);

	// pops a light emitter object from the stack
	static LightEmitter *lua_tolightemitter (lua_State *L, int idx=-1);

	// pops a Sketchpad interface from the stack
	static oapi::Sketchpad *lua_tosketchpad (lua_State *L, int idx=-1);

	// global functions
	static int help (lua_State *L);
	static int help_api (lua_State *L);

	// vector library functions
	static int vec_set (lua_State *L);
	static int vec_add (lua_State *L);
	static int vec_sub (lua_State *L);
	static int vec_mul (lua_State *L);
	static int vec_div (lua_State *L);
	static int vec_dotp (lua_State *L);
	static int vec_crossp (lua_State *L);
	static int vec_length (lua_State *L);
	static int vec_dist (lua_State *L);
	static int vec_unit (lua_State *L);
	static int mat_identity (lua_State *L);
	static int mat_mul (lua_State *L);
	static int mat_tmul (lua_State *L);
	static int mat_mmul (lua_State *L);

	// process library functions
	static int procFrameskip (lua_State *L);

	// -------------------------------------------
	// oapi library functions
	// -------------------------------------------
	static int oapi_get_orbiter_version (lua_State *L);
	static int oapi_get_viewport_size (lua_State *L);

	static int oapiGetObjectHandle (lua_State *L);
	static int oapiGetObjectCount (lua_State *L);
	static int oapiGetObjectName (lua_State *L);
	static int oapiCreateAnnotation (lua_State *L);
	static int oapiDelAnnotation (lua_State *L);
	static int oapiGetAnnotations (lua_State *L);
	static int oapiDbgOut (lua_State *L);
	static int oapiWriteLog(lua_State* L);
	static int oapiExit(lua_State *L);
	static int oapiOpenHelp (lua_State *L);
	static int oapiOpenInputBox (lua_State *L);
	static int oapiReceiveInput (lua_State *L);
	static int oapi_global_to_equ (lua_State *L);
	static int oapi_equ_to_global (lua_State *L);
	static int oapi_orthodome (lua_State *L);
	static int oapi_del_vessel (lua_State *L);

	// menu functions
	static int oapi_get_mainmenuvisibilitymode (lua_State *L);
	static int oapi_set_mainmenuvisibilitymode (lua_State *L);
	static int oapi_get_maininfovisibilitymode (lua_State *L);
	static int oapi_set_maininfovisibilitymode (lua_State *L);

	// Time functions
	static int oapi_get_simtime (lua_State *L);
	static int oapi_get_simstep (lua_State *L);
	static int oapi_get_systime (lua_State *L);
	static int oapi_get_sysstep (lua_State *L);
	static int oapi_get_simmjd (lua_State *L);
	static int oapi_set_simmjd (lua_State *L);
	static int oapi_get_sysmjd (lua_State *L);
	static int oapi_time2mjd (lua_State *L);
	static int oapi_get_tacc (lua_State *L);
	static int oapi_set_tacc (lua_State *L);
	static int oapi_get_pause (lua_State *L);
	static int oapi_set_pause (lua_State *L);

	// Body functions
	static int oapi_get_mass (lua_State *L);
	static int oapi_get_size (lua_State *L);
	static int oapi_get_globalpos (lua_State *L);
	static int oapi_get_globalvel (lua_State *L);
	static int oapi_get_relativepos (lua_State *L);
	static int oapi_get_relativevel (lua_State *L);

	// Vessel functions
	static int oapi_get_propellanthandle (lua_State *L);
	static int oapi_get_propellantmass (lua_State *L);
	static int oapi_get_propellantmaxmass (lua_State *L);
	static int oapi_get_fuelmass (lua_State *L);
	static int oapi_get_maxfuelmass (lua_State *L);
	static int oapi_get_emptymass (lua_State *L);
	static int oapi_set_emptymass (lua_State *L);
	static int oapi_get_altitude (lua_State *L);
	static int oapi_get_pitch (lua_State *L);
	static int oapi_get_bank (lua_State *L);
	static int oapi_get_heading (lua_State *L);
	static int oapi_get_groundspeed (lua_State *L);
	static int oapi_get_groundspeedvector (lua_State *L);
	static int oapi_get_airspeed (lua_State *L);
	static int oapi_get_airspeedvector (lua_State *L);
	static int oapi_get_shipairspeedvector (lua_State *L);
	static int oapi_get_equpos (lua_State *L);
	static int oapi_get_atm (lua_State *L);
	static int oapi_get_induceddrag (lua_State *L);
	static int oapi_get_wavedrag (lua_State *L);

	// Navigation radio transmitter functions
	static int oapi_get_navpos (lua_State *L);
	static int oapi_get_navchannel (lua_State *L);
	static int oapi_get_navrange (lua_State *L);
	static int oapi_get_navdata (lua_State *L);
	static int oapi_get_navsignal (lua_State *L);
	static int oapi_get_navtype (lua_State *L);

	// Camera functions
	static int oapi_set_cameramode (lua_State *L);
	static int oapi_get_cameratarget (lua_State *L);
	static int oapi_set_cameratarget (lua_State *L);
	static int oapi_get_cameraaperture (lua_State *L);
	static int oapi_set_cameraaperture (lua_State *L);
	static int oapi_get_cameraglobalpos (lua_State *L);
	static int oapi_get_cameraglobaldir (lua_State *L);
	static int oapi_move_groundcamera (lua_State *L);

	// animation functions
	static int oapi_create_animationcomponent (lua_State *L);
	static int oapi_del_animationcomponent (lua_State *L);

	// instrument panel functions
	static int oapi_open_mfd (lua_State *L);
	static int oapi_set_hudmode (lua_State *L);
	static int oapi_set_panelblink (lua_State *L);

	// user i/o functions
	static int oapi_keydown (lua_State *L);
	static int oapi_resetkey (lua_State *L);
	static int oapi_simulatebufferedkey (lua_State *L);
	static int oapi_simulateimmediatekey (lua_State *L);

	// file i/o functions
	static int oapi_openfile (lua_State* L);
	static int oapi_closefile (lua_State* L);
	static int oapi_savescenario (lua_State* L);
	static int oapi_writeline (lua_State* L);
	// static int oapi_writelog (lua_State * L);    // see oapiWriteLog(lua_State* L) above!
	// static int oapi_writelogv (lua_State * L);
	static int oapi_writescenario_string (lua_State* L);
	static int oapi_writescenario_int (lua_State *L);
	static int oapi_writescenario_float (lua_State* L);
	static int oapi_writescenario_vec (lua_State* L);
	static int oapi_readscenario_nextline (lua_State* L);
	static int oapi_readitem_string (lua_State* L);
	static int oapi_readitem_float (lua_State* L);
	static int oapi_readitem_int (lua_State* L);
	static int oapi_readitem_bool (lua_State* L);
	static int oapi_readitem_vec (lua_State* L);
	static int oapi_writeitem_string (lua_State* L);
	static int oapi_writeitem_float (lua_State* L);
	static int oapi_writeitem_int (lua_State* L);
	static int oapi_writeitem_bool (lua_State* L);
	static int oapi_writeitem_vec (lua_State* L);

	// utility functions
	static int oapi_rand (lua_State *L);
	static int oapi_deflate (lua_State *L);
	static int oapi_inflate (lua_State *L);
	static int oapi_get_color (lua_State *L);
	static int oapi_formatvalue (lua_State* L);

	// term library functions
	static int termOut (lua_State *L);
	static int termClear (lua_State *L);

	// screen annotation library functions
	static int noteSetText (lua_State *L);
	static int noteSetPos (lua_State *L);
	static int noteSetSize (lua_State *L);
	static int noteSetColour (lua_State *L);

	// -------------------------------------------
	// vessel access functions
	// -------------------------------------------
	static int vesselGetHandle (lua_State *L);
	static int vesselGetFocusHandle (lua_State *L);
	static int vesselGetInterface (lua_State *L);
	static int vesselGetFocusInterface (lua_State *L);
	static int vesselGetCount (lua_State *L);

	// -------------------------------------------
	// vessel methods
	// -------------------------------------------
	static int v_version (lua_State *L);
	static int v_get_handle (lua_State *L);
	static int v_send_bufferedkey (lua_State *L);
	static int v_is_landed (lua_State *L);
	static int v_get_groundcontact (lua_State *L);

	// general vessel properties
	static int v_get_name (lua_State *L);
	static int v_get_classname (lua_State *L);
	static int v_get_flightmodel (lua_State *L);
	static int v_get_damagemodel (lua_State *L);
	static int v_get_enablefocus (lua_State *L);
	static int v_set_enablefocus (lua_State *L);
	static int v_get_size (lua_State *L);
	static int v_set_size (lua_State *L);
	static int v_get_emptymass (lua_State *L);
	static int v_set_emptymass (lua_State *L);
	static int v_get_pmi (lua_State *L);
	static int v_set_pmi (lua_State *L);
	static int v_get_crosssections (lua_State *L);
	static int v_set_crosssections (lua_State *L);
	static int v_get_gravitygradientdamping (lua_State *L);
	static int v_set_gravitygradientdamping (lua_State *L);
	static int v_get_touchdownpointcount (lua_State *L);
	static int v_get_touchdownpoints (lua_State *L);
	static int v_set_touchdownpoints (lua_State *L);
	static int v_set_visibilitylimit (lua_State *L);
	static int v_get_clipradius (lua_State *L);
	static int v_set_albedoRGB (lua_State *L);
	static int v_set_clipradius (lua_State *L);
	static int v_set_surfacefrictioncoeff (lua_State *L);
	static int v_get_COG_elev (lua_State *L);

	// vessel state
	static int v_get_mass (lua_State *L);
	static int v_get_globalpos (lua_State *L);
	static int v_get_globalvel (lua_State *L);
	static int v_get_relativepos (lua_State *L);
	static int v_get_relativevel (lua_State *L);
	static int v_get_rotationmatrix (lua_State *L);
	static int v_get_status (lua_State *L);
	static int v_defset_status (lua_State *L);
	static int v_get_angvel (lua_State *L);
	static int v_set_angvel (lua_State *L);
	static int v_get_angularacc (lua_State *L);
	static int v_get_linearmoment (lua_State *L);
	static int v_get_angularmoment (lua_State *L);
	static int v_get_globalorientation (lua_State *L);
	static int v_set_globalorientation (lua_State *L);
	static int v_is_orbitstabilised (lua_State *L);
	static int v_is_nonsphericalgravityenabled (lua_State *L);
	static int v_toggle_navmode (lua_State *L);
	static int v_get_hoverholdaltitude (lua_State *L);
	static int v_set_hoverholdaltitude (lua_State *L);

	// orbital parameters
	static int v_get_gravityref (lua_State *L);
	static int v_get_elements (lua_State *L);
	static int v_get_elementsex (lua_State *L);
	static int v_set_elements (lua_State *L);
	static int v_get_progradedir (lua_State *L);
	static int v_get_smi (lua_State *L);
	static int v_get_argper (lua_State *L);
	static int v_get_pedist (lua_State *L);
	static int v_get_apdist (lua_State *L);

	// surface-relative parameters
	static int v_get_surfaceref (lua_State *L);
	static int v_get_altitude (lua_State *L);
	static int v_get_pitch (lua_State *L);
	static int v_get_bank (lua_State *L);
	static int v_get_yaw (lua_State *L);
	static int v_get_surfaceelevation (lua_State *L);
	static int v_get_surfacenormal (lua_State *L);

	// atmospheric parameters
	static int v_get_atmref (lua_State *L);
	static int v_get_atmtemperature (lua_State *L);
	static int v_get_atmdensity (lua_State *L);
	static int v_get_atmpressure (lua_State *L);

	// propellant methods
	static int v_create_propellantresource (lua_State *L);
	static int v_del_propellantresource (lua_State *L);
	static int v_clear_propellantresources (lua_State *L);
	static int v_get_propellantcount (lua_State *L);
	static int v_get_propellanthandle (lua_State *L);
	static int v_get_propellantmaxmass (lua_State *L);
	static int v_set_propellantmaxmass (lua_State *L);
	static int v_get_propellantmass (lua_State *L);
	static int v_set_propellantmass (lua_State *L);
	static int v_get_totalpropellantmass (lua_State *L);
	static int v_get_propellantefficiency (lua_State *L);
	static int v_set_propellantefficiency (lua_State *L);
	static int v_get_propellantflowrate (lua_State *L);
	static int v_get_totalpropellantflowrate (lua_State *L);

	// thruster methods
	static int v_create_thruster (lua_State *L);
	static int v_del_thruster (lua_State *L);
	static int v_clear_thrusters (lua_State *L);
	static int v_get_thrustercount (lua_State *L);
	static int v_get_thrusterhandle (lua_State *L);
	static int v_get_thrusterresource (lua_State *L);
	static int v_set_thrusterresource (lua_State *L);
	static int v_get_thrusterpos (lua_State *L);
	static int v_set_thrusterpos (lua_State *L);
	static int v_get_thrusterdir (lua_State *L);
	static int v_set_thrusterdir (lua_State *L);
	static int v_get_thrustermax0 (lua_State *L);
	static int v_set_thrustermax0 (lua_State *L);
	static int v_get_thrustermax (lua_State *L);
	static int v_get_thrusterisp0 (lua_State *L);
	static int v_get_thrusterisp (lua_State *L);
	static int v_set_thrusterisp (lua_State *L);
	static int v_get_thrusterlevel (lua_State *L);
	static int v_set_thrusterlevel (lua_State *L);
	static int v_inc_thrusterlevel (lua_State *L);
	static int v_set_thrusterlevel_singlestep (lua_State *L);
	static int v_inc_thrusterlevel_singlestep (lua_State *L);

	// thruster group methods
	static int v_create_thrustergroup (lua_State *L);
	static int v_del_thrustergroup (lua_State *L);
	static int v_get_thrustergrouphandle (lua_State *L);
	static int v_get_thrustergrouphandlebyindex (lua_State *L);
	static int v_get_groupthrustercount (lua_State *L);
	static int v_get_groupthruster (lua_State *L);
	static int v_get_thrustergrouplevel (lua_State *L);
	static int v_set_thrustergrouplevel (lua_State *L);
	static int v_inc_thrustergrouplevel (lua_State *L);
	static int v_inc_thrustergrouplevel_singlestep (lua_State *L);

	// reaction control system
	static int v_get_navmode (lua_State *L);
	static int v_set_navmode (lua_State *L);
	static int v_get_rcsmode (lua_State *L);
	static int v_set_rcsmode (lua_State *L);
	static int v_toggle_RCSmode (lua_State *L);

	// aerodynamic state parameters
	static int v_get_dynpressure (lua_State *L);
	static int v_get_machnumber (lua_State *L);
	static int v_get_groundspeed (lua_State *L);
	static int v_get_groundspeedvector (lua_State *L);
	static int v_get_airspeed (lua_State *L);
	static int v_get_airspeedvector (lua_State *L);
	static int v_get_shipairspeedvector (lua_State *L);
	static int v_get_horizonairspeedvector (lua_State *L);
	static int v_get_aoa (lua_State *L);
	static int v_get_slipangle (lua_State *L);
	static int v_get_lift(lua_State *L);
	static int v_get_drag(lua_State *L);

	// airfoils and aerodynamic controls
	static int v_create_airfoil (lua_State *L);
	static int v_edit_airfoil (lua_State *L);
	static int v_del_airfoil (lua_State *L);
	static int v_create_controlsurface (lua_State *L);
	static int v_get_adcmode (lua_State *L);
	static int v_set_adcmode (lua_State *L);
	static int v_get_adclevel (lua_State *L);
	static int v_set_adclevel (lua_State *L);

	// aerodynamic properties (legacy model)
	static int v_get_cw (lua_State *L);
	static int v_set_cw (lua_State *L);
	static int v_get_wingaspect (lua_State *L);
	static int v_set_wingaspect (lua_State *L);
	static int v_get_wingeffectiveness (lua_State *L);
	static int v_set_wingeffectiveness (lua_State *L);
	static int v_get_rotdrag (lua_State *L);
	static int v_set_rotdrag (lua_State *L);
	static int v_get_pitchmomentscale (lua_State *L);
	static int v_set_pitchmomentscale (lua_State *L);
	static int v_get_yawmomentscale (lua_State *L);
	static int v_set_yawmomentscale (lua_State *L);
	static int v_get_trimscale (lua_State *L);
	static int v_set_trimscale (lua_State *L);

	// forces
	static int v_get_weightvector (lua_State *L);
	static int v_get_thrustvector (lua_State *L);
	static int v_get_liftvector (lua_State *L);
	static int v_get_dragvector (lua_State *L);
	static int v_get_forcevector (lua_State *L);
	static int v_get_torquevector (lua_State *L);
	static int v_add_force (lua_State *L);

	// docking port management
	static int v_create_dock (lua_State *L);
	static int v_del_dock (lua_State *L);
	static int v_clear_dockdefinitions(lua_State *L);
	static int v_set_dockparams (lua_State *L);
	static int v_get_dockparams (lua_State *L);
	static int v_get_dockcount (lua_State *L);
	static int v_get_dockhandle (lua_State *L);
	static int v_get_dockstatus (lua_State *L);
	static int v_dockingstatus (lua_State *L);
	static int v_undock (lua_State *L);

	// attachment management
	static int v_create_attachment (lua_State *L);
	static int v_del_attachment (lua_State *L);
	static int v_clear_attachments (lua_State *L);
	static int v_set_attachmentparams (lua_State *L);
	static int v_get_attachmentparams (lua_State *L);
	static int v_get_attachmentid (lua_State *L);
	static int v_get_attachmentstatus (lua_State *L);
	static int v_get_attachmentcount (lua_State *L);
	static int v_get_attachmentindex (lua_State *L);
	static int v_get_attachmenthandle (lua_State *L);
	static int v_attach_child (lua_State *L);
	static int v_detach_child (lua_State *L);

	// navigation radio interface
	static int v_enable_transponder (lua_State *L);
	static int v_get_transponder (lua_State *L);
	static int v_set_transponderchannel (lua_State *L);
	static int v_enable_ids (lua_State *L);
	static int v_get_ids (lua_State *L);
	static int v_set_idschannel (lua_State *L);
	static int v_init_navradios (lua_State *L);
	static int v_get_navcount (lua_State *L);
	static int v_set_navchannel (lua_State *L);
	static int v_get_navchannel (lua_State *L);
	static int v_get_navsource (lua_State *L);

	// exhaust and reentry render options
	static int v_add_exhaust (lua_State *L);
	static int v_del_exhaust (lua_State *L);
	static int v_get_exhaustcount (lua_State *L);
	static int v_add_exhauststream (lua_State *L);

	// Nosewheel-steering and wheel brakes
	static int v_set_nosewheelsteering (lua_State *L);
	static int v_get_nosewheelsteering (lua_State *L);
	static int v_set_maxwheelbrakeforce (lua_State *L);
	static int v_set_wheelbrakelevel (lua_State *L);
	static int v_get_wheelbrakelevel (lua_State *L);

	// light source methods
	static int v_add_pointlight (lua_State *L);
	static int v_add_spotlight (lua_State *L);
	static int v_get_lightemitter (lua_State *L);
	static int v_get_lightemittercount (lua_State *L);
	static int v_del_lightemitter (lua_State *L);
	static int v_clear_lightemitters (lua_State *L);

	// camera management
	static int v_get_cameraoffset (lua_State *L);
	static int v_set_cameraoffset (lua_State *L);
	static int v_set_cameradefaultdirection (lua_State *L);
	static int v_get_cameradefaultdirection (lua_State *L);
	static int v_set_cameracatchangle (lua_State *L);
	static int v_set_camerarotationrange (lua_State *L);
	static int v_set_camerashiftrange (lua_State *L);
	static int v_set_cameramovement (lua_State *L);

	// Instrument panel and virtual cockpit methods
	static int v_trigger_panelredrawarea (lua_State *L);
	static int v_trigger_redrawarea (lua_State *L);

	// mesh methods
	static int v_add_mesh (lua_State *L);
	static int v_insert_mesh (lua_State *L);
	static int v_del_mesh (lua_State *L);
	static int v_clear_meshes (lua_State *L);
	static int v_get_meshcount (lua_State *L);
	static int v_shift_mesh (lua_State *L);
	static int v_shift_meshes (lua_State *L);
	static int v_get_meshoffset (lua_State *L);

	// animation methods
	static int v_create_animation (lua_State *L);
	static int v_del_animation (lua_State *L);
	static int v_set_animation (lua_State *L);
	static int v_get_animation (lua_State *L);
	static int v_add_animationcomponent (lua_State *L);
	static int v_del_animationcomponent (lua_State *L);
	static int v_register_animation (lua_State *L);
	static int v_unregister_animation (lua_State *L);

	// coordinate transformations
	static int v_shift_centreofmass (lua_State *L);
	static int v_shiftCG (lua_State *L);
	static int v_get_superstructureCG (lua_State *L);
	static int v_set_rotationmatrix (lua_State *L);
	static int v_globalrot (lua_State *L);
	static int v_horizonrot (lua_State *L);
	static int v_horizoninvrot (lua_State *L);
	static int v_local2global (lua_State *L);
	static int v_global2local (lua_State *L);
	static int v_local2rel (lua_State *L);

	// -------------------------------------------
	// MFD methods
	// -------------------------------------------
	static int mfd_get_size (lua_State *L);
	static int mfd_set_title (lua_State *L);
	static int mfd_get_defaultpen (lua_State *L);
	static int mfd_get_defaultfont (lua_State *L);
	static int mfd_invalidate_display (lua_State *L);
	static int mfd_invalidate_buttons (lua_State *L);

	// -------------------------------------------
	// Light emitter methods
	// -------------------------------------------
	static int le_get_position (lua_State *L);
	static int le_set_position (lua_State *L);
	static int le_get_direction (lua_State *L);
	static int le_set_direction (lua_State *L);
	static int le_get_intensity (lua_State *L);
	static int le_set_intensity (lua_State *L);
	static int le_get_range (lua_State *L);
	static int le_set_range (lua_State *L);
	static int le_get_attenuation (lua_State *L);
	static int le_set_attenuation (lua_State *L);
	static int le_get_spotaperture (lua_State *L);
	static int le_set_spotaperture (lua_State *L);
	static int le_activate (lua_State *L);
	static int le_is_active (lua_State *L);

	// -------------------------------------------
	// Sketchpad methods
	// -------------------------------------------
	static int skp_text (lua_State *L);
	static int skp_moveto (lua_State *L);
	static int skp_lineto (lua_State *L);
	static int skp_line (lua_State *L);
	static int skp_rectangle (lua_State *L);
	static int skp_ellipse (lua_State *L);
	static int skp_polygon (lua_State *L);
	static int skp_polyline (lua_State *L);
	static int skp_set_origin (lua_State *L);
	static int skp_set_textalign (lua_State *L);
	static int skp_set_textcolor (lua_State *L);
	static int skp_set_backgroundcolor (lua_State *L);
	static int skp_set_backgroundmode (lua_State *L);
	static int skp_set_pen (lua_State *L);
	static int skp_set_font (lua_State *L);
	static int skp_get_charsize (lua_State *L);
	static int skp_get_textwidth (lua_State *L);

	friend int OpenHelp (void *context);

private:
	std::binary_semaphore sem_exec;
	std::binary_semaphore sem_wait;

	int status;              // interpreter status
	bool is_busy;            // interpreter busy (running a script)
	int jobs;                // number of background jobs left over after command terminates
	int (*postfunc)(void*);
	void *postcontext;

	static int lua_tointeger_safe (lua_State *L, int idx, int prmno, const char *funcname);
	static double lua_tonumber_safe (lua_State *L, int idx, int prmno, const char *funcname);
	static bool lua_toboolean_safe (lua_State *L, int idx, int prmno, const char *funcname);
	static const char *lua_tostring_safe (lua_State *L, int idx, int prmno, const char *funcname);
	static void *lua_tolightuserdata_safe (lua_State *L, int idx, int prmno, const char *funcname);
	static VECTOR3 lua_tovector_safe (lua_State *L, int idx, int prmno, const char *funcname);
	static MATRIX3 lua_tomatrix_safe (lua_State *L, int idx, int prmno, const char *funcname);
	static double lua_field_tonumber_safe (lua_State *L, int idx, int prmno, const char *fieldname, const char *funcname);
	static void *lua_field_tolightuserdata_safe (lua_State *L, int idx, int prmno, const char *fieldname, const char *funcname);
	static VECTOR3 lua_field_tovector_safe (lua_State *L, int idx, int prmno, const char *fieldname, const char *funcname);
	static int AssertPrmType(lua_State *L, int idx, int prmno, int tp, const char *funcname, const char *fieldname=0);

	// Touchdown Vertex ------------------------------------------------------

	// pushes touchdown vertex 'tdvtx' into a table on top of the stack
	static void lua_pushtouchdownvtx (lua_State *L, const TOUCHDOWNVTX &tdvtx);

	// converts the touchdown vertex at stack position 'idx' into a TOUCHDOWNVTX
	static TOUCHDOWNVTX lua_totouchdownvtx (lua_State *L, int idx);

	// returns 1 if stack entry idx is a touchdown vertex, 0 otherwise
	static int lua_istouchdownvtx (lua_State *L, int idx);


	// Vessel Status ---------------------------------------------------------

	// pushes VESSELSTATUS 'vs' into a table on top of the stack
	static void lua_push_vessel_status (lua_State *L, const VESSELSTATUS &vs);

	// pushes VESSELSTATUS2 'vs' into a table on top of the stack
	static void lua_push_vessel_status (lua_State *L, const VESSELSTATUS2 &vs);

	// checks whether stack entry idx is a VESSELSTATUS or a VESSELSTATUS2
	static int lua_get_vesselstatus_version (lua_State *L, int idx);
};

#endif // !__INTERPRETER_H

// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef __INTERPRETER_H
#define __INTERPRETER_H

extern "C" {
#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>
}

#include "OrbiterAPI.h"
#include "VesselAPI.h" // for TOUCHDOWNVTX
#include <unordered_set>

class gcCore;

#define PRMTP_NIL           0x01
#define PRMTP_NUMBER        0x02
#define PRMTP_BOOLEAN       0x04
#define PRMTP_STRING        0x08
#define PRMTP_LIGHTUSERDATA 0x10
#define PRMTP_TABLE         0x20
#define PRMTP_VECTOR        0x40
#define PRMTP_MATRIX        0x80
#define PRMTP_USERDATA     0x100

#define ASSERT_SYNTAX(cond,msg) { if(!(cond)) { luaL_error(L, "%s: %s", __FUNCTION__+13, msg); return 0; } }
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
class XRSound;

struct AirfoilContext {
	lua_State *L;
	int funcref;
};

struct VesselMFDContext {
	lua_State* L;
	int msgproc;
};

typedef struct  {
	BEACONLIGHTSPEC bs;
	VECTOR3 pos;
	VECTOR3 col;
	VESSEL *vessel;
} BEACONLIGHTSPEC_Lua;

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
	 * \brief Wait for thread execution.
	 * \note This is called by either the orbiter thread or the interpreter
	 *   thread when they are waiting to regain execution control.
	 */
	virtual void WaitExec (DWORD timeout = INFINITE);

	/**
	 * \brief Release thread execution.
	 * \param timeout time [ms] to wait for the mutex. Default is infinite
	 *   (wait does not time out).
	 * \note This is called by either the orbiter thread or the interpreter
	 *   thread after finishing a cycle to hand control over to the other
	 *   thread.
	 */
	virtual void EndExec ();

	/**
	 * \brief Define functions for interfacing with Orbiter API
	 */
	virtual void LoadAPI ();

	virtual void LoadVesselAPI ();

	virtual void LoadMFDAPI ();

	virtual void LoadNTVERTEXAPI ();

	virtual void LoadLightEmitterMethods ();

	virtual void LoadBeaconMethods ();

	virtual void LoadCustomCameraMethods ();

	virtual void LoadSketchpadAPI ();

	virtual void LoadBitAPI();
	/**
	 * \brief Run the interpreter initialisation script
	 */
	virtual void LoadStartupScript ();

	virtual int ProcessChunk (const char *chunk, int n);

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

	static int lua_pushnumberref(lua_State* L);


	void term_setverbosity (int level) { term_verbose = level; }

	static int LuaCall(lua_State *L, int nargs, int nres);
	static void DeleteVessel (OBJHANDLE hVessel);
protected:
	lua_State *L;         // Lua main context

	/**
	 * \brief Load screen annotation methods.
	 */
	void LoadAnnotationAPI ();
	void LoadVesselStatusAPI ();

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
	static void lua_pushrgba(lua_State* L, const COLOUR4&);

	// pops an OBJHANDLE from the stack
	static OBJHANDLE lua_toObject (lua_State *L, int idx=-1);

	static RECT lua_torect(lua_State* L, int idx);

	// pushes a vessel object on the stack
	static void lua_pushvessel (lua_State *L, VESSEL *v);

	// Pops a VESSEL interface from the stack and returns it.
	// A NULL return indicates an invalid data type at the specified stack position,
	// a nonzero return guarantees a valid vessel pointer
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


	static void *luaL_tryudata (lua_State *L, int ud, const char *tname);

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
	static int mat_rotm (lua_State *L);

	// bit manipulations
	static int bit_anyset(lua_State* L);
	static int bit_allset(lua_State* L);
	static int bit_and(lua_State* L);
	static int bit_or(lua_State* L);
	static int bit_xor(lua_State* L);
	static int bit_not(lua_State* L);
	static int bit_mask(lua_State* L);
	static int bit_lshift(lua_State* L);
	static int bit_rshift(lua_State* L);
	static int bit_arshift(lua_State* L);
	static int bit_rol(lua_State* L);
	static int bit_ror(lua_State* L);

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
	static int oapi_open_inputboxex (lua_State *L);
	static int oapi_add_notification (lua_State *L);
	static int oapi_global_to_equ(lua_State* L);
	static int oapi_global_to_local(lua_State* L);
	static int oapi_local_to_equ(lua_State* L);
	static int oapi_equ_to_global (lua_State *L);
	static int oapi_orthodome (lua_State *L);
	static int oapi_del_vessel(lua_State* L);
	static int oapi_create_vessel(lua_State* L);
	static int oapi_set_focusobject(lua_State* L);

	static int oapi_get_rotationmatrix(lua_State* L);

	// textures
	static int oapi_register_exhausttexture(lua_State* L);
	static int oapi_register_reentrytexture(lua_State* L);
	static int oapi_register_particletexture(lua_State* L);
	static int oapi_get_texturehandle(lua_State* L);
	static int oapi_load_texture(lua_State* L);
	static int oapi_release_texture(lua_State* L);
	static int oapi_set_texture(lua_State* L);
	static int oapi_create_surface(lua_State* L);
	static int oapi_destroy_surface(lua_State* L);
	static int oapi_save_surface(lua_State* L);
	static int oapi_clear_surface(lua_State* L);

	// GC
	static int oapi_set_materialex(lua_State* L);
	static int oapi_set_material(lua_State* L);
	static int oapi_set_meshproperty(lua_State* L);

	// VC
	static int oapi_VC_trigger_redrawarea(lua_State* L);
	static int oapi_VC_set_areaclickmode_quadrilateral(lua_State* L);
	static int oapi_VC_set_areaclickmode_spherical(lua_State* L);
	static int oapi_VC_register_area(lua_State* L);
	static int oapi_VC_set_neighbours(lua_State* L);
	static int oapi_VC_registerHUD(lua_State* L);
	static int oapi_VC_registermfd(lua_State* L);
	static int oapi_cockpit_mode(lua_State* L);
	static int oapi_render_hud(lua_State* L);
	static int oapi_get_hudintensity(lua_State* L);
	static int oapi_set_hudintensity(lua_State* L);
	static int oapi_inc_hudintensity(lua_State* L);
	static int oapi_dec_hudintensity(lua_State* L);
	static int oapi_toggle_hudcolour(lua_State* L);

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

	// Planets
	static int oapi_get_planetperiod(lua_State* L);
	static int oapi_get_objecttype(lua_State* L);
	static int oapi_get_gbodyparent(lua_State* L);
	static int oapi_get_gbody(lua_State* L);
	static int oapi_get_gbodycount(lua_State* L);
	static int oapi_get_planetatmconstants(lua_State* L);
	static int oapi_get_planetobliquity(lua_State* L);
	static int oapi_get_planettheta(lua_State* L);
	static int oapi_get_planetobliquitymatrix(lua_State* L);
	static int oapi_get_planetcurrentrotation(lua_State* L);
	static int oapi_planet_hasatmosphere(lua_State* L);
	static int oapi_get_planetatmparams(lua_State* L);
	static int oapi_get_groundvector(lua_State* L);
	static int oapi_get_windvector(lua_State* L);
	static int oapi_get_planetjcoeffcount(lua_State* L);
	static int oapi_get_planetjcoeff(lua_State* L);

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

	// docking
	static int oapi_get_dockhandle(lua_State* L);
	static int oapi_get_dockstatus(lua_State* L);
	static int oapi_set_autocapture(lua_State* L);
	static int oapi_get_dockowner(lua_State* L);

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
	static int oapi_set_cameracockpitdir (lua_State *L);

	// Custom camera
	static int oapi_delete_customcamera (lua_State *L);
	static int oapi_setup_customcamera (lua_State *L);
	static int oapi_customcamera_overlay (lua_State *L);
	static int oapi_customcamera_onoff (lua_State *L);
	static void customcamera_clbk(oapi::Sketchpad *pSkp, void *pParam);
	static int customcamera_collect (lua_State *L);


	// animation functions
	static int oapi_create_animationcomponent (lua_State *L);
	static int oapi_del_animationcomponent (lua_State *L);

	// instrument panel functions
	static int oapi_open_mfd (lua_State *L);
	static int oapi_set_hudmode (lua_State *L);
	static int oapi_get_hudmode (lua_State *L);
	static int oapi_set_panelblink (lua_State *L);
	static int oapi_get_mfdmode(lua_State* L);
	static int oapi_mfd_buttonlabel(lua_State* L);
	static int oapi_disable_mfdmode(lua_State* L);
	static int oapi_register_mfd(lua_State* L);
	static int oapi_process_mfdbutton(lua_State* L);
	static int oapi_send_mfdkey(lua_State* L);
	static int oapi_refresh_mfdbuttons(lua_State* L);
	static int oapi_toggle_mfdon(lua_State* L);
	static int oapi_get_mfdmodespec(lua_State* L);

	static int oapi_set_defnavdisplay(lua_State* L);
	static int oapi_set_defrcsdisplay(lua_State* L);

	// user i/o functions
	static int oapi_keydown (lua_State *L);
	static int oapi_resetkey (lua_State *L);
	static int oapi_simulatebufferedkey (lua_State *L);
	static int oapi_simulateimmediatekey (lua_State *L);
	static int oapi_acceptdelayedkey (lua_State *L);

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

	// sketchpad
	static int oapi_get_sketchpad(lua_State* L);
	static int oapi_release_sketchpad(lua_State* L);
	static int oapi_create_font(lua_State* L);
	static int oapi_create_pen(lua_State* L);
	static int oapi_create_brush(lua_State* L);
	static int oapi_release_font(lua_State* L);
	static int oapi_release_pen(lua_State* L);
	static int oapi_release_brush(lua_State* L);

	// Blt
	static int oapi_blt(lua_State* L);
	static int oapi_blt_panelareabackground(lua_State* L);

	// Panel
	static int oapi_set_panelneighbours(lua_State* L);

	// Mesh
	static int oapi_load_meshglobal(lua_State* L);
	static int oapi_mesh_group(lua_State* L);
	static int oapi_create_mesh(lua_State* L);
	static int oapi_delete_mesh(lua_State* L);
	static int oapi_add_meshgroupblock(lua_State* L);
	static int oapi_edit_meshgroup(lua_State* L);
	static int oapi_get_meshgroup(lua_State* L);
		
	static void lua_pushmeshhandle(lua_State *L, MESHHANDLE);
	static void lua_pushdevmeshhandle(lua_State *L, DEVMESHHANDLE);
	static MESHHANDLE lua_tomeshhandle(lua_State *L, int idx);
	static DEVMESHHANDLE lua_todevmeshhandle(lua_State *L, int idx);
	static int lua_ismeshhandle(lua_State *L, int idx);
	static int lua_isdevmeshhandle(lua_State *L, int idx);

	// term library functions
	static int termOut (lua_State *L);
	static int termClear (lua_State *L);

	// screen annotation library functions
	static int noteSetText(lua_State* L);
	static int noteSetPos(lua_State* L);
	static int noteSetSize(lua_State* L);
	static int noteSetColour(lua_State* L);

	// vesselstatus library functions
	static int vsget(lua_State* L);
	static int vsset(lua_State* L);
	static int vs2get(lua_State* L);
	static int vs2set(lua_State* L);

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
	static int v_version(lua_State* L);
	static int v_get_handle (lua_State *L);
	static int v_send_bufferedkey (lua_State *L);
	static int v_is_landed (lua_State *L);
	static int v_get_groundcontact (lua_State *L);

	// general vessel properties
	static int v_get_name (lua_State *L);
	static int v_get_classname (lua_State *L);
	static int v_get_flightmodel (lua_State *L);
	static int v_get_flightstatus (lua_State *L);
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
	static int v_get_status (lua_State* L);
	static int v_get_rawstatus (lua_State* L);
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
	static int v_get_apdist(lua_State* L);
	static int v_get_equpos(lua_State* L);

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
	static int v_set_default_propellantresource(lua_State* L);
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
	static int v_inc_thrustergrouplevel_singlestep(lua_State* L);
	static int v_get_manualcontrollevel(lua_State* L);

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
	static int v_del_controlsurface (lua_State *L);
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
	static int v_create_variabledragelement(lua_State *L);
	static int v_clear_variabledragelements(lua_State *L);

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
	static int v_undock(lua_State* L);
	static int v_dock(lua_State* L);
	static int v_get_proxydock(lua_State* L);
	static int v_get_dockindex(lua_State* L);
	static int v_get_targetdockalignment(lua_State* L);
	static int v_move_dock(lua_State* L);

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
	static int v_add_exhauststream(lua_State* L);
	static int v_add_reentrystream(lua_State* L);
	static int v_add_particlestream(lua_State* L);
	static int v_del_exhauststream(lua_State* L);
	static int oapi_particle_getlevelref(lua_State* L);

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

	// beacons
	static int oapi_create_beacon (lua_State *L);
	static int v_add_beacon (lua_State *L);
	static int v_del_beacon (lua_State *L);
	static int v_clear_beacons (lua_State *L);
	static int beacon_collect (lua_State *L);
	static int beacon_get (lua_State *L);
	static int beacon_set (lua_State *L);

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

	// MFD
	static int v_register_mfdmode(lua_State* L);
	static int v_unregister_mfdmode(lua_State* L);

	// mesh methods
	static int v_add_mesh (lua_State *L);
	static int v_insert_mesh (lua_State *L);
	static int v_del_mesh (lua_State *L);
	static int v_clear_meshes (lua_State *L);
	static int v_get_meshcount (lua_State *L);
	static int v_shift_mesh (lua_State *L);
	static int v_shift_meshes (lua_State *L);
	static int v_get_meshoffset (lua_State *L);
	static int v_get_devmesh(lua_State* L);
	static int v_set_mesh_visibility_mode(lua_State* L);

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

	// File I/O
	static int v_parse_scenario_line_ex(lua_State* L);

	// Recording
	static int v_record_event(lua_State* L);
	static int v_playback(lua_State* L);

	// Panel handling
	static int v_register_panelarea(lua_State* L);
	static int v_register_panelmfdgeometry(lua_State* L);
	static int v_set_panelscaling(lua_State* L);
	static int v_set_panelbackground(lua_State* L);

	// -------------------------------------------
	// MFD methods
	// -------------------------------------------
	static int mfd_get_size(lua_State* L);
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
	static int le_get_visibility (lua_State *L);
	static int le_set_visibility (lua_State *L);

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
	static int skp_set_font(lua_State* L);
	static int skp_set_brush(lua_State* L);
	static int skp_get_charsize (lua_State *L);
	static int skp_get_textwidth (lua_State *L);
	static int skp_copy_rect (lua_State *L);
	static int skp_stretch_rect (lua_State *L);
	static int skp_rotate_rect (lua_State *L);
	static int skp_quick_pen (lua_State *L);
	static int skp_quick_brush (lua_State *L);
	static int skp_get_surface (lua_State *L);
	static int skp_set_brightness (lua_State *L);
	static int skp_set_renderparam (lua_State *L);
	static int skp_set_worldtransform2d (lua_State *L);

	// -------------------------------------------
	// NTVERTEX methods
	// -------------------------------------------
	static int oapi_create_ntvertexarray(lua_State *L);
	static int oapi_del_ntvertexarray (lua_State *L);
	static int ntv_size(lua_State *L);
	static int ntv_reset(lua_State *L);
	static int ntv_zeroize(lua_State *L);
	static int ntv_append(lua_State *L);
	static int ntv_copy(lua_State *L);
	static int ntv_write(lua_State *L);
	static int ntv_extract(lua_State *L); // create a table containing the NTVERTEX data
	static int ntv_set(lua_State *L);     // assignment operator array[i] = table/NTV proxy
	static int ntv_get(lua_State *L);// proxy = array[i]
	static int ntv_collect(lua_State *L);
	static int ntv_view(lua_State *L); // create a temporary view similar to C++ string_views
		
	static void push_ntvertexarray(lua_State *L, NTVERTEX *, int);


	static void ntvproxy_create(lua_State *L, NTVERTEX *);
	static int ntvproxy_get(lua_State *L);  // val = proxy.x
	static int ntvproxy_set(lua_State *L);  // proxy.x = val

	// -------------------------------------------
	// Index array methods
	// -------------------------------------------
	static int oapi_create_indexarray(lua_State *L);
	static int oapi_del_indexarray (lua_State *L);
	static int idx_size(lua_State *L);
	static int idx_reset(lua_State *L);
	static int idx_append(lua_State *L);
	static int idx_get(lua_State *L);
	static int idx_set(lua_State *L);
	static int idx_collect(lua_State *L);
	static void push_indexarray(lua_State *L, WORD *, int);

	
	friend int OpenHelp (void *context);

	// -------------------------------------------
	// XRSound
	// -------------------------------------------
	virtual void LoadXRSoundAPI ();
	static int lua_isxrsound(lua_State *L, int idx);
	static XRSound *lua_toxrsound(lua_State *L, int idx);
	static int xrsound_create_instance(lua_State *L);
	static int xrsound_is_present(lua_State *L);
	static int xrsound_get_version(lua_State *L);
	static int xrsound_load_wav(lua_State *L);
	static int xrsound_play_wav(lua_State *L);
	static int xrsound_stop_wav(lua_State *L);
	static int xrsound_is_wavplaying(lua_State *L);
	static int xrsound_set_paused(lua_State *L);
	static int xrsound_is_paused(lua_State *L);
	static int xrsound_set_defaultsoundenabled(lua_State *L);
	static int xrsound_get_defaultsoundenabled(lua_State *L);
	static int xrsound_set_defaultsoundgroupfolder(lua_State *L);
	static int xrsound_get_defaultsoundgroupfolder(lua_State *L);
	static int xrsound_set_pan(lua_State *L);
	static int xrsound_get_pan(lua_State *L);
	static int xrsound_set_playbackspeed(lua_State *L);
	static int xrsound_get_playbackspeed(lua_State *L);
	static int xrsound_set_playposition(lua_State *L);
	static int xrsound_get_playposition(lua_State *L);
	static int xrsound_collect(lua_State *L);

private:
	HANDLE hExecMutex; // flow control synchronisation
	HANDLE hWaitMutex;
	static inline gcCore *pCore;
	static inline bool gcCoreInitialized = false;

	static void LazyInitGCCore();

	bool bExecLocal;   // flag for locally created mutexes
	bool bWaitLocal;

	int status;              // interpreter status
	bool is_busy;            // interpreter busy (running a script)
	int jobs;                // number of background jobs left over after command terminates
	int (*postfunc)(void*);
	void *postcontext;

	static inline std::unordered_set<VESSEL *>knownVessels; // for lua_isvessel


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

	static OAPI_MSGTYPE MsgProcMFD(UINT msg, UINT mfd, WPARAM wparam, LPARAM lparam);
};

#endif // !__INTERPRETER_H

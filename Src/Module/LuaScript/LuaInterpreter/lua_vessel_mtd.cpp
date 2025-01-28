// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#define INTERPRETER_IMPLEMENTATION

#include "Interpreter.h"
#include "VesselAPI.h"
#include "MfdApi.h"

extern "C" {
#include <lauxlib.h>
}

/*
VesselMFD: Class instantiated for MFDs declared inside Lua Vessel modules
*/
class VesselMFD : public MFD2
{
public:
	VesselMFD(DWORD w, DWORD h, VESSEL* vessel, VesselMFDContext* ctx);
	virtual ~VesselMFD();
	bool ConsumeButton(int bt, int event) override;
	bool ConsumeKeyBuffered(DWORD key) override;
	bool ConsumeKeyImmediate(char* kstate) override;
	char* ButtonLabel(int bt) override;
	int ButtonMenu(const MFDBUTTONMENU** menu) const override;
	bool Update(oapi::Sketchpad* skp) override;
	void StoreStatus() const override;
	void RecallStatus() override;
	void WriteStatus(FILEHANDLE scn) const override;
	void ReadStatus(FILEHANDLE scn) override;

	lua_State* L;
	int mfd_ref;
};

/***
Vessel class: Lua access to VESSEL objects
@classmod vessel
*/

/// @lookup types

// A placeholder flag for "focus vessel" where usually a vessel pointer would be expected
VESSEL *vfocus = (VESSEL*)0x1;

VESSEL *Interpreter::lua_tovessel (lua_State *L, int idx)
{
	VESSEL **pv = (VESSEL**)lua_touserdata (L, idx);
	if(pv) {
		if (*pv == vfocus) { // returns current focused vessel when using the pseudo vessel "focus"
			VESSEL *v = oapiGetFocusInterface();
			knownVessels.insert(v);
			return v;
		} else if(knownVessels.find(*pv) == knownVessels.end()) {
			return NULL;
		}
		return *pv;
	}
	return NULL;
}

VESSEL *Interpreter::lua_tovessel_safe(lua_State *L, int idx, const char *funcname)
{
	VESSEL *v = lua_tovessel(L,idx);
	if (!v) {
		luaL_error(L, "Invalid vessel object for self");
	}
	return v;
}

// ===========================================================================
// Touchdown Vertex

void Interpreter::lua_pushtouchdownvtx (lua_State *L, const TOUCHDOWNVTX &tdvtx)
{
	lua_createtable(L, 0, 5);
	lua_pushvector(L, tdvtx.pos);        lua_setfield(L, -2, "pos");
	lua_pushnumber(L, tdvtx.stiffness);  lua_setfield(L, -2, "stiffness");
	lua_pushnumber(L, tdvtx.damping);    lua_setfield(L, -2, "damping");
	lua_pushnumber(L, tdvtx.mu);         lua_setfield(L, -2, "mu");
	lua_pushnumber(L, tdvtx.mu_lng);     lua_setfield(L, -2, "mu_lng");
}

TOUCHDOWNVTX Interpreter::lua_totouchdownvtx (lua_State *L, int idx)
{
	TOUCHDOWNVTX tdvtx;
	lua_getfield(L, idx, "pos"      ); tdvtx.pos       = lua_tovector(L, -1); lua_pop(L, 1);
	lua_getfield(L, idx, "stiffness"); tdvtx.stiffness = lua_tonumber(L, -1); lua_pop(L, 1);
	lua_getfield(L, idx, "damping"  ); tdvtx.damping   = lua_tonumber(L, -1); lua_pop(L, 1);
	lua_getfield(L, idx, "mu"       ); tdvtx.mu        = lua_tonumber(L, -1); lua_pop(L, 1);
	lua_getfield(L, idx, "mu_lng"   ); tdvtx.mu_lng    = lua_tonumber(L, -1); lua_pop(L, 1);
	return tdvtx;
}

int Interpreter::lua_istouchdownvtx (lua_State *L, int idx)
{
	if (!lua_istable(L, idx)) return 0;
	static const char *fieldname[5] = { "pos", "stiffness", "damping", "mu", "mu_lng" };
	int i, ii, n;
	bool fail;

	lua_pushnil(L);
	ii = (idx >= 0 ? idx : idx - 1);
	n = 0;
	while (lua_next(L, ii)) {
		lua_pop(L, 1);
		n++;
	}
	if (n != 5) return 0;

	for (i = 0; i < 5; i++) {
		lua_getfield(L, idx, fieldname[i]);
		fail = (lua_isnil(L, -1));
		lua_pop(L, 1);
		if (fail) return 0;
	}
	return 1;
}

// ===========================================================================
// Vessel Status

int Interpreter::lua_get_vesselstatus_version (lua_State *L, int idx) // Returns either 1, 2, or 0 (unknown)
{
	if (!lua_istable(L, idx)) return 0;

	// Very crude and simple check
	lua_getfield(L, idx, "version");
	DWORD version = lua_tointeger(L, -1);

	return (version > 0 && version <= 2) ? version : 0;
}

void Interpreter::lua_push_vessel_status (lua_State *L, const VESSELSTATUS &vs)
{
	lua_createtable(L, 0, 14);
	lua_pushvector       (L, vs.rpos);      lua_setfield(L, -2, "rpos");
	lua_pushvector       (L, vs.rvel);      lua_setfield(L, -2, "rvel");
	lua_pushvector       (L, vs.vrot);      lua_setfield(L, -2, "vrot");
	lua_pushvector       (L, vs.arot);      lua_setfield(L, -2, "arot");
	lua_pushnumber       (L, vs.fuel);      lua_setfield(L, -2, "fuel");
	lua_pushnumber       (L, vs.eng_main);  lua_setfield(L, -2, "eng_main");
	lua_pushnumber       (L, vs.eng_hovr);  lua_setfield(L, -2, "eng_hovr");
	lua_pushlightuserdata(L, vs.rbody);     lua_setfield(L, -2, "rbody");
	lua_pushlightuserdata(L, vs.base);      lua_setfield(L, -2, "base");
	lua_pushnumber       (L, vs.port);      lua_setfield(L, -2, "port");
	lua_pushnumber       (L, vs.status);    lua_setfield(L, -2, "status");
	lua_pushvector       (L, vs.vdata[0]);  lua_setfield(L, -2, "vdata"); // ignore [1]...[9]
	lua_pushnumber       (L, vs.fdata[0]);  lua_setfield(L, -2, "fdata"); //   "
	lua_pushnumber       (L, vs.flag[0]);   lua_setfield(L, -2, "flag");  //   "
}

void Interpreter::lua_push_vessel_status (lua_State *L, const VESSELSTATUS2 &vs)
{
	lua_createtable(L, 0, 17);

	lua_pushnumber       (L, vs.version);    lua_setfield(L, -2, "version"); // should always be 2!
	lua_pushnumber       (L, vs.flag);       lua_setfield(L, -2, "flag");
	lua_pushlightuserdata(L, vs.rbody);      lua_setfield(L, -2, "rbody");
	lua_pushlightuserdata(L, vs.base);       lua_setfield(L, -2, "base");
	lua_pushnumber       (L, vs.port);       lua_setfield(L, -2, "port");
	lua_pushnumber       (L, vs.status);     lua_setfield(L, -2, "status");
	lua_pushvector       (L, vs.rpos);       lua_setfield(L, -2, "rpos");
	lua_pushvector       (L, vs.rvel);       lua_setfield(L, -2, "rvel");
	lua_pushvector       (L, vs.vrot);       lua_setfield(L, -2, "vrot");
	lua_pushvector       (L, vs.arot);       lua_setfield(L, -2, "arot");
	lua_pushnumber       (L, vs.surf_lng);   lua_setfield(L, -2, "surf_lng");
	lua_pushnumber       (L, vs.surf_lat);   lua_setfield(L, -2, "surf_lat");
	lua_pushnumber       (L, vs.surf_hdg);   lua_setfield(L, -2, "surf_hdg");
	lua_pushnumber       (L, vs.xpdr);       lua_setfield(L, -2, "xpdr");

	// Fuel list
	lua_createtable(L, vs.nfuel, 0);
	for (DWORD i = 0; i < vs.nfuel; ++i) {
		lua_pushnumber(L, i + 1);  // Put key of the Nth child table on-top of Lua VM stack

		lua_createtable(L, 0, 2);  // Create Nth child table of size 2 non-array elements
		lua_pushinteger(L, vs.fuel[i].idx);
		lua_setfield(L, -2, "idx");
		lua_pushnumber(L, vs.fuel[i].level);
		lua_setfield(L, -2, "level");
		// Child table is on-top of the stack.
		lua_settable(L, -3);       // lua_settable() pops key, value pair from Lua VM stack
	}
	lua_setfield(L, -2, "fuel");

	// Thruster list
	lua_createtable(L, vs.nthruster, 0);
	for (DWORD i = 0; i < vs.nthruster; ++i) {
		lua_pushnumber(L, i + 1);
		lua_createtable(L, 0, 2);
		lua_pushinteger(L, vs.thruster[i].idx);
		lua_setfield(L, -2, "idx");
		lua_pushnumber(L, vs.thruster[i].level);
		lua_setfield(L, -2, "level");
		lua_settable(L, -3);
	}
	lua_setfield(L, -2, "thruster");

	// Dock info list
	lua_createtable(L, vs.ndockinfo, 0);
	for (DWORD i = 0; i < vs.ndockinfo; ++i) {
		lua_pushnumber(L, i + 1);
		lua_createtable(L, 0, 3);
		lua_pushinteger(L, vs.dockinfo[i].idx);
		lua_setfield(L, -2, "idx");
		lua_pushinteger(L, vs.dockinfo[i].ridx);
		lua_setfield(L, -2, "ridx");
		lua_pushlightuserdata(L, vs.dockinfo[i].rvessel);
		lua_setfield(L, -2, "rvessel");
		lua_settable(L, -3);
	}
	lua_setfield(L, -2, "dockinfo");
}


void Interpreter::LoadVesselAPI ()
{
	static const struct luaL_Reg vesselAcc[] = {
		{"get_handle", vesselGetHandle},
		{"get_focushandle", vesselGetFocusHandle},
		{"get_interface", vesselGetInterface},
		{"get_focusinterface", vesselGetFocusInterface},
		{"get_count", vesselGetCount},
		{NULL, NULL}
	};
	static const struct luaL_Reg vesselLib[] = {
		{"version", v_version},
		{"get_handle", v_get_handle},
		{"send_bufferedkey", v_send_bufferedkey},

		{"register_mfdmode", v_register_mfdmode},
		{"unregister_mfdmode", v_unregister_mfdmode},

		// General vessel properties
		{"get_name", v_get_name},
		{"get_classname", v_get_classname},
		{"get_flightmodel", v_get_flightmodel},
		{"get_flightstatus", v_get_flightstatus},
		{"get_damagemodel", v_get_damagemodel},
		{"get_enablefocus", v_get_enablefocus},
		{"set_enablefocus", v_set_enablefocus},
		{"get_size", v_get_size},
		{"set_size", v_set_size},
		{"get_emptymass", v_get_emptymass},
		{"set_emptymass", v_set_emptymass},
		{"get_pmi", v_get_pmi},
		{"set_pmi", v_set_pmi},
		{"get_crosssections", v_get_crosssections},
		{"set_crosssections", v_set_crosssections},
		{"get_gravitygradientdamping", v_get_gravitygradientdamping},
		{"set_gravitygradientdamping", v_set_gravitygradientdamping},
		{"get_touchdownpointcount", v_get_touchdownpointcount},
		{"get_touchdownpoints", v_get_touchdownpoints},
		{"set_touchdownpoints", v_set_touchdownpoints},
		{"set_visibilitylimit", v_set_visibilitylimit},
		{"get_clipradius",v_get_clipradius},
		{"set_albedoRGB", v_set_albedoRGB},
		{"set_clipradius", v_set_clipradius},
		{"set_surfacefrictioncoeff", v_set_surfacefrictioncoeff},

		// vessel state
		{"get_mass", v_get_mass},
		{"get_globalpos", v_get_globalpos},
		{"get_globalvel", v_get_globalvel},
		{"get_relativepos", v_get_relativepos},
		{"get_relativevel", v_get_relativevel},
		{"get_rotationmatrix", v_get_rotationmatrix},
		{"get_status", v_get_status},
		{"get_rawstatus", v_get_rawstatus},
		{"defset_status", v_defset_status},
		{"get_angvel", v_get_angvel},
		{"set_angvel", v_set_angvel},

		// orbital parameters
		{"get_gravityref", v_get_gravityref},
		{"get_elements", v_get_elements},
		{"get_elementsex", v_get_elementsex},
		{"set_elements", v_set_elements},
		{"get_progradedir", v_get_progradedir},

		// surface-relative parameters
		{"get_surfaceref", v_get_surfaceref},
		{"get_altitude", v_get_altitude},
		{"get_pitch", v_get_pitch},
		{"get_bank", v_get_bank},
		{"get_yaw", v_get_yaw},

		// atmospheric parameters
		{"get_atmref", v_get_atmref},
		{"get_atmtemperature", v_get_atmtemperature},
		{"get_atmdensity", v_get_atmdensity},
		{"get_atmpressure", v_get_atmpressure},

		// aerodynamic state parameters
		{"get_dynpressure", v_get_dynpressure},
		{"get_machnumber", v_get_machnumber},
		{"get_airspeed", v_get_airspeed},
		{"get_airspeedvector", v_get_airspeedvector},
		{"get_shipairspeedvector", v_get_shipairspeedvector},
		{"get_horizonairspeedvector", v_get_horizonairspeedvector},
		{"get_groundspeed", v_get_groundspeed},
		{"get_groundspeedvector", v_get_groundspeedvector},
		{"get_aoa", v_get_aoa},
		{"get_slipangle", v_get_slipangle},

		// airfoils and aerodynamic controls
		{"create_airfoil", v_create_airfoil},
		{"edit_airfoil", v_edit_airfoil},
		{"del_airfoil", v_del_airfoil},
		{"create_controlsurface", v_create_controlsurface},
		{"del_controlsurface", v_del_controlsurface},
		{"get_adcmode", v_get_adcmode},
		{"set_adcmode", v_set_adcmode},
		{"get_adclevel", v_get_adclevel},
		{"set_adclevel", v_set_adclevel},

		// aerodynamic properties (legacy model)
		{"get_cw", v_get_cw},
		{"set_cw", v_set_cw},
		{"get_wingaspect", v_get_wingaspect},
		{"set_wingaspect", v_set_wingaspect},
		{"get_wingeffectiveness", v_get_wingeffectiveness},
		{"set_wingeffectiveness", v_set_wingeffectiveness},
		{"get_rotdrag", v_get_rotdrag},
		{"set_rotdrag", v_set_rotdrag},
		{"get_pitchmomentscale", v_get_pitchmomentscale},
		{"set_pitchmomentscale", v_set_pitchmomentscale},
		{"get_yawmomentscale", v_get_yawmomentscale},
		{"set_yawmomentscale", v_set_yawmomentscale},
		{"get_trimscale", v_get_trimscale},
		{"set_trimscale", v_set_trimscale},

		// forces
		{"get_lift", v_get_lift},
		{"get_drag", v_get_drag},
		{"get_weightvector", v_get_weightvector},
		{"get_thrustvector", v_get_thrustvector},
		{"get_liftvector", v_get_liftvector},

		// vessel status
		{"is_landed", v_is_landed},
		{"get_groundcontact", v_get_groundcontact},

		// fuel management
		{"create_propellantresource", v_create_propellantresource },
		{"set_default_propellantresource", v_set_default_propellantresource },
		{"del_propellantresource", v_del_propellantresource},
		{"clear_propellantresources", v_clear_propellantresources},
		{"get_propellantcount", v_get_propellantcount},
		{"get_propellanthandle", v_get_propellanthandle},
		{"get_propellantmaxmass", v_get_propellantmaxmass},
		{"set_propellantmaxmass", v_set_propellantmaxmass},
		{"get_propellantmass", v_get_propellantmass},
		{"set_propellantmass", v_set_propellantmass},
		{"get_totalpropellantmass", v_get_totalpropellantmass},
		{"get_propellantefficiency", v_get_propellantefficiency},
		{"set_propellantefficiency", v_set_propellantefficiency},
		{"get_propellantflowrate", v_get_propellantflowrate},
		{"get_totalpropellantflowrate", v_get_totalpropellantflowrate},

		// Thruster management
		{"create_thruster", v_create_thruster},
		{"del_thruster", v_del_thruster},
		{"clear_thrusters", v_clear_thrusters},
		{"get_thrustercount", v_get_thrustercount},
		{"get_thrusterhandle", v_get_thrusterhandle},
		{"get_thrusterresource", v_get_thrusterresource},
		{"set_thrusterresource", v_set_thrusterresource},
		{"get_thrusterpos", v_get_thrusterpos},
		{"set_thrusterpos", v_set_thrusterpos},
		{"get_thrusterdir", v_get_thrusterdir},
		{"set_thrusterdir", v_set_thrusterdir},
		{"get_thrustermax0", v_get_thrustermax0},
		{"set_thrustermax0", v_set_thrustermax0},
		{"get_thrustermax", v_get_thrustermax},
		{"get_thrusterisp0", v_get_thrusterisp0},
		{"get_thrusterisp", v_get_thrusterisp},
		{"set_thrusterisp", v_set_thrusterisp},
		{"get_thrusterlevel", v_get_thrusterlevel},
		{"set_thrusterlevel", v_set_thrusterlevel},
		{"inc_thrusterlevel", v_inc_thrusterlevel},
		{"set_thrusterlevel_singlestep", v_set_thrusterlevel_singlestep},
		{"inc_thrusterlevel_singlestep", v_inc_thrusterlevel_singlestep},

		// Thruster group management
		{"create_thrustergroup", v_create_thrustergroup},
		{"del_thrustergroup", v_del_thrustergroup},
		{"get_thrustergrouphandle", v_get_thrustergrouphandle},
		{"get_thrustergrouphandlebyindex", v_get_thrustergrouphandlebyindex},
		{"get_groupthrustercount", v_get_groupthrustercount},
		{"get_groupthruster", v_get_groupthruster},
		{"get_thrustergrouplevel", v_get_thrustergrouplevel},
		{"set_thrustergrouplevel", v_set_thrustergrouplevel},
		{"inc_thrustergrouplevel", v_inc_thrustergrouplevel},
		{"inc_thrustergrouplevel_singlestep", v_inc_thrustergrouplevel_singlestep},
		{"get_manualcontrollevel", v_get_manualcontrollevel },
			

		// Reaction control system
		{"get_navmode", v_get_navmode},
		{"set_navmode", v_set_navmode},
		{"get_rcsmode", v_get_rcsmode},
		{"set_rcsmode", v_set_rcsmode},

		// Docking port management
		{"create_dock", v_create_dock},
		{"del_dock", v_del_dock},
		{"clear_dockdefinitions", v_clear_dockdefinitions},
		{"set_dockparams", v_set_dockparams},
		{"get_dockparams", v_get_dockparams},
		{"get_dockcount", v_get_dockcount},
		{"get_dockhandle", v_get_dockhandle},
		{"get_dockstatus", v_get_dockstatus},
		{"dockingstatus", v_dockingstatus},
		{"undock", v_undock },
		{"dock", v_dock },
		{"get_proxydock", v_get_proxydock },
		{"get_dockindex", v_get_dockindex },
		{"get_targetdockalignment", v_get_targetdockalignment },
		{"move_dock", v_move_dock },

		// Attachment management
		{"create_attachment", v_create_attachment},
		{"del_attachment", v_del_attachment},
		{"clear_attachments", v_clear_attachments},
		{"set_attachmentparams", v_set_attachmentparams},
		{"get_attachmentparams", v_get_attachmentparams},
		{"get_attachmentid", v_get_attachmentid},
		{"get_attachmentstatus", v_get_attachmentstatus},
		{"get_attachmentcount", v_get_attachmentcount},
		{"get_attachmentindex", v_get_attachmentindex},
		{"get_attachmenthandle", v_get_attachmenthandle},
		{"attach_child", v_attach_child},
		{"detach_child", v_detach_child},

		// Navigation radio interface
		{"enable_transponder", v_enable_transponder},
		{"get_transponder", v_get_transponder},
		{"set_transponderchannel", v_set_transponderchannel},
		{"enable_ids", v_enable_ids},
		{"get_ids", v_get_ids},
		{"set_idschannel", v_set_idschannel},
		{"init_navradios", v_init_navradios},
		{"get_navcount", v_get_navcount},
		{"set_navchannel", v_set_navchannel},
		{"get_navchannel", v_get_navchannel},
		{"get_navsource", v_get_navsource},

		// exhaust and reentry render options
		{"add_exhaust", v_add_exhaust},
		{"del_exhaust", v_del_exhaust},
		{"get_exhaustcount", v_get_exhaustcount},
		{"add_exhauststream", v_add_exhauststream },
		{"add_reentrystream", v_add_reentrystream },
		{"del_exhauststream", v_del_exhauststream },
		{"add_particlestream", v_add_particlestream },
			
		// Nosewheel-steering and wheel brakes
		{"set_nosewheelsteering", v_set_nosewheelsteering},
		{"get_nosewheelsteering", v_get_nosewheelsteering},
		{"set_maxwheelbrakeforce", v_set_maxwheelbrakeforce},
		{"set_wheelbrakelevel", v_set_wheelbrakelevel},
		{"get_wheelbrakelevel", v_get_wheelbrakelevel},

		// light source methods
		{"add_pointlight", v_add_pointlight},
		{"add_spotlight", v_add_spotlight},
		{"get_lightemitter", v_get_lightemitter},
		{"get_lightemittercount", v_get_lightemittercount},
		{"del_lightemitter", v_del_lightemitter},
		{"clear_lightemitters", v_clear_lightemitters},

		// beacons
		{"add_beacon", v_add_beacon},
		{"del_beacon", v_del_beacon},
		{"clear_beacons", v_clear_beacons},

		// Camera management
		{"get_cameraoffset", v_get_cameraoffset},
		{"set_cameraoffset", v_set_cameraoffset},
		{"set_cameradefaultdirection", v_set_cameradefaultdirection},
		{"get_cameradefaultdirection", v_get_cameradefaultdirection},
		{"set_cameracatchangle", v_set_cameracatchangle},
		{"set_camerarotationrange", v_set_camerarotationrange},
		{"set_camerashiftrange", v_set_camerashiftrange},
		{"set_cameramovement", v_set_cameramovement},

		// Instrument panel and virtual cockpit methods
		{"trigger_panelredrawarea", v_trigger_panelredrawarea},
		{"trigger_redrawarea", v_trigger_redrawarea},

		// mesh methods
		{"add_mesh", v_add_mesh},
		{"insert_mesh", v_insert_mesh},
		{"del_mesh", v_del_mesh},
		{"clear_meshes", v_clear_meshes},
		{"get_meshcount", v_get_meshcount},
		{"shift_mesh", v_shift_mesh},
		{"shift_meshes", v_shift_meshes},
		{"get_meshoffset", v_get_meshoffset },
		{"get_devmesh", v_get_devmesh },
		{"set_mesh_visibility_mode", v_set_mesh_visibility_mode },

		// animation methods
		{"create_animation", v_create_animation},
		{"del_animation", v_del_animation},
		{"set_animation", v_set_animation},
		{"get_animation", v_get_animation},
		{"add_animationcomponent", v_add_animationcomponent},
		{"del_animationcomponent", v_del_animationcomponent},
		{"register_animation", v_register_animation},
		{"unregister_animation", v_unregister_animation},

		// coordinate transformations
		{"shift_centreofmass", v_shift_centreofmass},
		{"shiftCG", v_shiftCG},
		{"get_superstructureCG", v_get_superstructureCG},
		{"set_rotationmatrix", v_set_rotationmatrix},
		{"globalrot", v_globalrot},
		{"horizonrot", v_horizonrot},
		{"horizoninvrot", v_horizoninvrot},
		{"local2global", v_local2global},
		{"global2local", v_global2local},
		{"local2rel", v_local2rel},

        // not yet grouped functions
        {"get_surfaceelevation", v_get_surfaceelevation},
        {"get_surfacenormal", v_get_surfacenormal},
        {"get_angularacc", v_get_angularacc},
        {"get_linearmoment", v_get_linearmoment},
        {"get_angularmoment", v_get_angularmoment},
        {"get_globalorientation", v_get_globalorientation},
        {"set_globalorientation", v_set_globalorientation},
        {"get_smi", v_get_smi},
        {"get_argper", v_get_argper},
        {"get_pedist", v_get_pedist},
		{"get_apdist", v_get_apdist},
		{"get_equpos", v_get_equpos},
        {"get_dragvector", v_get_dragvector},
        {"get_forcevector", v_get_forcevector},
        {"get_torquevector", v_get_torquevector},
        {"add_force", v_add_force},
        {"create_variabledragelement", v_create_variabledragelement },
        {"clear_variabledragelements", v_clear_variabledragelements },
        {"is_orbitstabilised", v_is_orbitstabilised},
        {"is_nonsphericalgravityenabled", v_is_nonsphericalgravityenabled},
        {"toggle_navmode", v_toggle_navmode},
        {"get_hoverholdaltitude", v_get_hoverholdaltitude},
        {"set_hoverholdaltitude", v_set_hoverholdaltitude},
        {"toggle_RCSmode", v_toggle_RCSmode},
        {"get_COG_elev", v_get_COG_elev},
		{"parse_scenario_line_ex", v_parse_scenario_line_ex},

		// Recording
		{"record_event", v_record_event},
		{"playback", v_playback},

		// Panels
		{"register_panelarea", v_register_panelarea},
		{"register_panelmfdgeometry", v_register_panelmfdgeometry},
		{"set_panelscaling", v_set_panelscaling},
		{"set_panelbackground", v_set_panelbackground},
			
		{NULL, NULL}
	};
	luaL_newmetatable (L, "VESSEL.vtable");

	lua_pushstring (L, "__index");
	lua_pushvalue (L, -2); // push metatable
	lua_settable (L, -3);  // metatable.__index = metatable

	luaL_setfuncs(L, vesselLib, 0);
	luaL_newlib(L, vesselAcc);
	lua_setglobal(L, "vessel");

	// put vessel back on the stack after registering global (lua 5.4)
	lua_getglobal(L, "vessel");

	// create pseudo-instance "focus"
	lua_pushlightuserdata (L, &vfocus);
	luaL_getmetatable (L, "VESSEL.vtable");  // push metatable
	lua_setmetatable (L, -2);               // set metatable for user data
	lua_setglobal (L, "focus");

	lua_createtable(L, 0, 4);
	lua_pushnumber(L, OAPI_MSG_MFD_OPENED);     lua_setfield(L, -2, "MFD_OPENED");
	lua_pushnumber(L, OAPI_MSG_MFD_CLOSED);     lua_setfield(L, -2, "MFD_CLOSED");
	lua_pushnumber(L, OAPI_MSG_MFD_UPDATE);     lua_setfield(L, -2, "MFD_UPDATE");
	lua_pushnumber(L, OAPI_MSG_MFD_OPENEDEX);   lua_setfield(L, -2, "MFD_OPENEDEX");
	lua_setglobal(L, "OAPI_MSG");


	// store thruster group identifiers in global "THGROUP" table
	// C identifiers "THGROUP_xxx" become table entries "THGROUP.xxx"
	lua_createtable (L, 0, 15);
	lua_pushnumber (L, THGROUP_MAIN);          lua_setfield (L, -2, "MAIN");
	lua_pushnumber (L, THGROUP_RETRO);         lua_setfield (L, -2, "RETRO");
	lua_pushnumber (L, THGROUP_HOVER);         lua_setfield (L, -2, "HOVER");
	lua_pushnumber (L, THGROUP_ATT_PITCHUP);   lua_setfield (L, -2, "ATT_PITCHUP");
	lua_pushnumber (L, THGROUP_ATT_PITCHDOWN); lua_setfield (L, -2, "ATT_PITCHDOWN");
	lua_pushnumber (L, THGROUP_ATT_YAWLEFT);   lua_setfield (L, -2, "ATT_YAWLEFT");
	lua_pushnumber (L, THGROUP_ATT_YAWRIGHT);  lua_setfield (L, -2, "ATT_YAWRIGHT");
	lua_pushnumber (L, THGROUP_ATT_BANKLEFT);  lua_setfield (L, -2, "ATT_BANKLEFT");
	lua_pushnumber (L, THGROUP_ATT_BANKRIGHT); lua_setfield (L, -2, "ATT_BANKRIGHT");
	lua_pushnumber (L, THGROUP_ATT_RIGHT);     lua_setfield (L, -2, "ATT_RIGHT");
	lua_pushnumber (L, THGROUP_ATT_LEFT);      lua_setfield (L, -2, "ATT_LEFT");
	lua_pushnumber (L, THGROUP_ATT_UP);        lua_setfield (L, -2, "ATT_UP");
	lua_pushnumber (L, THGROUP_ATT_DOWN);      lua_setfield (L, -2, "ATT_DOWN");
	lua_pushnumber (L, THGROUP_ATT_FORWARD);   lua_setfield (L, -2, "ATT_FORWARD");
	lua_pushnumber (L, THGROUP_ATT_BACK);      lua_setfield (L, -2, "ATT_BACK");
	lua_setglobal (L, "THGROUP");

	// store navmode identifiers in global NAVMODE table
	lua_createtable (L, 0, 7);
	lua_pushnumber (L, NAVMODE_KILLROT);       lua_setfield (L, -2, "KILLROT");
	lua_pushnumber (L, NAVMODE_HLEVEL);        lua_setfield (L, -2, "HLEVEL");
	lua_pushnumber (L, NAVMODE_PROGRADE);      lua_setfield (L, -2, "PROGRADE");
	lua_pushnumber (L, NAVMODE_RETROGRADE);    lua_setfield (L, -2, "RETROGRADE");
	lua_pushnumber (L, NAVMODE_NORMAL);        lua_setfield (L, -2, "NORMAL");
	lua_pushnumber (L, NAVMODE_ANTINORMAL);    lua_setfield (L, -2, "ANTINORMAL");
	lua_pushnumber (L, NAVMODE_HOLDALT);       lua_setfield (L, -2, "HOLDALT");
	lua_setglobal (L, "NAVMODE");

	// store RCS mode identifiers in global RCSMODE table
	lua_createtable (L, 0, 3);
	lua_pushnumber (L, RCS_NONE);              lua_setfield (L, -2, "OFF");
	lua_pushnumber (L, RCS_ROT);               lua_setfield (L, -2, "ROT");
	lua_pushnumber (L, RCS_LIN);               lua_setfield (L, -2, "LIN");
	lua_setglobal (L, "RCSMODE");


	lua_createtable(L, 0, 8);
	lua_pushnumber(L, MANCTRL_ATTMODE);              lua_setfield(L, -2, "ATTMODE");
	lua_pushnumber(L, MANCTRL_REVMODE);              lua_setfield(L, -2, "REVMODE");
	lua_pushnumber(L, MANCTRL_ROTMODE);              lua_setfield(L, -2, "ROTMODE");
	lua_pushnumber(L, MANCTRL_LINMODE);              lua_setfield(L, -2, "LINMODE");
	lua_pushnumber(L, MANCTRL_ANYMODE);              lua_setfield(L, -2, "ANYMODE");
	lua_pushnumber(L, MANCTRL_KEYBOARD);             lua_setfield(L, -2, "KEYBOARD");
	lua_pushnumber(L, MANCTRL_JOYSTICK);             lua_setfield(L, -2, "JOYSTICK");
	lua_pushnumber(L, MANCTRL_ANYDEVICE);            lua_setfield(L, -2, "ANYDEVICE");
	lua_setglobal(L, "MANCTRL");

	// store aerodynamic control surface mode identifiers in global ADCMODE table
	lua_createtable (L, 0, 5);
	lua_pushnumber (L, 0);                     lua_setfield (L, -2, "OFF");
	lua_pushnumber (L, 0x1);                   lua_setfield (L, -2, "ELEVATOR");
	lua_pushnumber (L, 0x2);                   lua_setfield (L, -2, "RUDDER");
	lua_pushnumber (L, 0x4);                   lua_setfield (L, -2, "AILERON");
	lua_pushnumber (L, 0x7);                   lua_setfield (L, -2, "ON");
	lua_setglobal (L, "ADCMODE");

	// store control surface types in global AIRCTRL table
	lua_createtable (L, 0, 6);
	lua_pushnumber (L, AIRCTRL_ELEVATOR);      lua_setfield (L, -2, "ELEVATOR");
	lua_pushnumber (L, AIRCTRL_RUDDER);        lua_setfield (L, -2, "RUDDER");
	lua_pushnumber (L, AIRCTRL_AILERON);       lua_setfield (L, -2, "AILERON");
	lua_pushnumber (L, AIRCTRL_FLAP);          lua_setfield (L, -2, "FLAP");
	lua_pushnumber (L, AIRCTRL_ELEVATORTRIM);  lua_setfield (L, -2, "ELEVATORTRIM");
	lua_pushnumber (L, AIRCTRL_RUDDERTRIM);    lua_setfield (L, -2, "RUDDERTRIM");
	lua_setglobal (L, "AIRCTRL");

	// store control surface axis orientations in global AIRCTRL_AXIS table
	lua_createtable (L, 0, 5);
	lua_pushnumber (L, AIRCTRL_AXIS_AUTO);     lua_setfield (L, -2, "AUTO");
	lua_pushnumber (L, AIRCTRL_AXIS_YPOS);     lua_setfield (L, -2, "YPOS");
	lua_pushnumber (L, AIRCTRL_AXIS_YNEG);     lua_setfield (L, -2, "YNEG");
	lua_pushnumber (L, AIRCTRL_AXIS_XPOS);     lua_setfield (L, -2, "XPOS");
	lua_pushnumber (L, AIRCTRL_AXIS_XNEG);     lua_setfield (L, -2, "XNEG");
	lua_setglobal (L, "AIRCTRL_AXIS");

	// store airfoil orientation types in global LIFT table
	lua_createtable (L, 0, 2);
	lua_pushnumber (L, LIFT_VERTICAL);         lua_setfield (L, -2, "VERTICAL");
	lua_pushnumber (L, LIFT_HORIZONTAL);       lua_setfield (L, -2, "HORIZONTAL");
	lua_setglobal (L, "LIFT");

	// store HUD modes in global HUDMODE table
	lua_createtable(L, 0, 4);
	lua_pushnumber(L, HUD_NONE);               lua_setfield(L, -2, "NONE");
	lua_pushnumber(L, HUD_ORBIT);              lua_setfield(L, -2, "ORBIT");
	lua_pushnumber(L, HUD_SURFACE);            lua_setfield(L, -2, "SURFACE");
	lua_pushnumber(L, HUD_DOCKING);            lua_setfield(L, -2, "DOCKING");
	lua_setglobal(L, "HUDMODE");

	// store MFD modes in global MFDMODE table
	lua_createtable(L, 0, 13);
	lua_pushnumber(L, MFD_REFRESHBUTTONS);     lua_setfield(L, -2, "REFRESHBUTTONS");
	lua_pushnumber(L, MFD_NONE);               lua_setfield(L, -2, "NONE");
	lua_pushnumber(L, MFD_ORBIT);              lua_setfield(L, -2, "ORBIT");
	lua_pushnumber(L, MFD_SURFACE);            lua_setfield(L, -2, "SURFACE");
	lua_pushnumber(L, MFD_MAP);                lua_setfield(L, -2, "MAP");
	lua_pushnumber(L, MFD_HSI);                lua_setfield(L, -2, "HSI");
	lua_pushnumber(L, MFD_LANDING);            lua_setfield(L, -2, "LANDING");
	lua_pushnumber(L, MFD_DOCKING);            lua_setfield(L, -2, "DOCKING");
	lua_pushnumber(L, MFD_OPLANEALIGN);        lua_setfield(L, -2, "OPLANEALIGN");
	lua_pushnumber(L, MFD_OSYNC);              lua_setfield(L, -2, "OSYNC");
	lua_pushnumber(L, MFD_TRANSFER);           lua_setfield(L, -2, "TRANSFER");
	lua_pushnumber(L, MFD_COMMS);              lua_setfield(L, -2, "COMMS");
	lua_pushnumber(L, MFD_USERTYPE);           lua_setfield(L, -2, "USERTYPE");
	lua_setglobal(L, "MFDMODE");

	// store MFD IDs in global MFDID table
	lua_createtable(L, 0, 13);
	lua_pushnumber(L, MFD_LEFT);               lua_setfield(L, -2, "LEFT");
	lua_pushnumber(L, MFD_RIGHT);              lua_setfield(L, -2, "RIGHT");
	lua_pushnumber(L, MFD_USER1);              lua_setfield(L, -2, "USER1");
	lua_pushnumber(L, MFD_USER2);              lua_setfield(L, -2, "USER2");
	lua_pushnumber(L, MFD_USER3);              lua_setfield(L, -2, "USER3");
	lua_pushnumber(L, MFD_USER4);              lua_setfield(L, -2, "USER4");
	lua_pushnumber(L, MFD_USER5);              lua_setfield(L, -2, "USER5");
	lua_pushnumber(L, MFD_USER6);              lua_setfield(L, -2, "USER6");
	lua_pushnumber(L, MFD_USER7);              lua_setfield(L, -2, "USER7");
	lua_pushnumber(L, MFD_USER8);              lua_setfield(L, -2, "USER8");
	lua_pushnumber(L, MFD_USER9);              lua_setfield(L, -2, "USER9");
	lua_pushnumber(L, MFD_USER10);             lua_setfield(L, -2, "USER10");
	lua_pushnumber(L, MAXMFD);                 lua_setfield(L, -2, "MAX");
	lua_setglobal(L, "MFDID");

	lua_createtable(L, 0, 2);
	lua_pushnumber(L, MFD_SHOWMODELABELS);             lua_setfield(L, -2, "SHOWMODELABELS");
	lua_pushnumber(L, MFD_TRANSPARENT_WHEN_OFF);       lua_setfield(L, -2, "TRANSPARENT_WHEN_OFF");
	lua_setglobal(L, "MFDFLAG");


	// store mesh visibility modes in global MESHVIS table
	lua_createtable(L, 0, 6);
	lua_pushnumber(L, MESHVIS_NEVER);           lua_setfield(L, -2, "NEVER");
	lua_pushnumber(L, MESHVIS_EXTERNAL);        lua_setfield(L, -2, "EXTERNAL");
	lua_pushnumber(L, MESHVIS_COCKPIT);         lua_setfield(L, -2, "COCKPIT");
	lua_pushnumber(L, MESHVIS_EXTERNAL | MESHVIS_COCKPIT);  lua_setfield(L, -2, "ALWAYS");
	lua_pushnumber(L, MESHVIS_VC);              lua_setfield(L, -2, "VC");
	lua_pushnumber(L, MESHVIS_EXTPASS);         lua_setfield(L, -2, "EXTPASS");
	lua_setglobal(L, "MESHVIS");


	// store materian properties for oapiSetMaterialEx in MATPROP table
	lua_createtable(L, 0, 10);
	lua_pushnumber(L, MatProp::Diffuse);        lua_setfield(L, -2, "DIFFUSE");
	lua_pushnumber(L, MatProp::Ambient);        lua_setfield(L, -2, "AMBIENT");
	lua_pushnumber(L, MatProp::Specular);       lua_setfield(L, -2, "SPECULAR");
	lua_pushnumber(L, MatProp::Light);          lua_setfield(L, -2, "LIGHT");
	lua_pushnumber(L, MatProp::Emission);       lua_setfield(L, -2, "EMISSION");
	lua_pushnumber(L, MatProp::Reflect);        lua_setfield(L, -2, "REFLECT");
	lua_pushnumber(L, MatProp::Smooth);         lua_setfield(L, -2, "SMOOTH");
	lua_pushnumber(L, MatProp::Metal);          lua_setfield(L, -2, "METAL");
	lua_pushnumber(L, MatProp::Fresnel);        lua_setfield(L, -2, "FRESNEL");
	lua_pushnumber(L, MatProp::SpecialFX);      lua_setfield(L, -2, "SPECIALFX");
	lua_setglobal(L, "MATPROP");


	// store vessel propagation modes in global PROP table
	/***
	Table of orbit propagation modes
	@table PROP
	@field ORBITAL_ELEMENTS Propagate orbital vessels along the Keplerian elements.
	@field ORBITAL_FIXEDSTATE Keep the orbital vessels' relative position and velocity with respect to the central body fixed in a non-rotating frame.
	@field ORBITAL_FIXEDSURF Keep the orbital vessels' position velocity and attitude fixed relative to the planet surface.
	@field SORBITAL_ELEMENTS Propagate suborbital vessels along the Keplerian elements.
	@field SORBITAL_FIXEDSTATE Keep the suborbital vessels' relative position and velocity with respect to the central body fixed in a non-rotating frame.
	@field SORBITAL_FIXEDSURF Keep the suborbital vessels' position velocity and attitude fixed relative to the planet surface.
	@field SORBITAL_DESTROY Destroy any suborbital vessels (i.e. assume that the vessels impacted on the ground during time propagation).
	*/
	lua_createtable (L, 0, 7);
	lua_pushnumber (L, PROP_ORBITAL_ELEMENTS);   lua_setfield (L, -2, "ORBITAL_ELEMENTS");
	lua_pushnumber (L, PROP_ORBITAL_FIXEDSTATE); lua_setfield (L, -2, "ORBITAL_FIXEDSTATE");
	lua_pushnumber (L, PROP_ORBITAL_FIXEDSURF);  lua_setfield (L, -2, "ORBITAL_FIXEDSURF");
	lua_pushnumber (L, PROP_SORBITAL_ELEMENTS);  lua_setfield (L, -2, "SORBITAL_ELEMENTS");
	lua_pushnumber (L, PROP_SORBITAL_FIXEDSTATE);lua_setfield (L, -2, "SORBITAL_FIXEDSTATE");
	lua_pushnumber (L, PROP_SORBITAL_FIXEDSURF); lua_setfield (L, -2, "SORBITAL_FIXEDSURF");
	lua_pushnumber (L, PROP_SORBITAL_DESTROY);   lua_setfield (L, -2, "SORBITAL_DESTROY");
	lua_setglobal (L, "PROP");

	// store navigation radio transmitter types in global TRANSMITTER table
	lua_createtable (L, 0, 6);
	lua_pushnumber (L, TRANSMITTER_NONE);   lua_setfield (L, -2, "NONE");
	lua_pushnumber (L, TRANSMITTER_VOR);    lua_setfield (L, -2, "VOR");
	lua_pushnumber (L, TRANSMITTER_VTOL);   lua_setfield (L, -2, "VTOL");
	lua_pushnumber (L, TRANSMITTER_ILS);    lua_setfield (L, -2, "ILS");
	lua_pushnumber (L, TRANSMITTER_IDS);    lua_setfield (L, -2, "IDS");
	lua_pushnumber (L, TRANSMITTER_XPDR);   lua_setfield (L, -2, "XPDR");
	lua_setglobal (L, "TRANSMITTER");

	// store particle stream identifiers in global PARTICLE table
	lua_createtable(L, 0, 10);
	lua_pushnumber (L, PARTICLESTREAMSPEC::EMISSIVE);  lua_setfield(L,-2,"EMISSIVE");
	lua_pushnumber (L, PARTICLESTREAMSPEC::DIFFUSE);   lua_setfield(L,-2,"DIFFUSE");
	lua_pushnumber (L, PARTICLESTREAMSPEC::LVL_FLAT);  lua_setfield(L,-2,"LVL_FLAT");
	lua_pushnumber (L, PARTICLESTREAMSPEC::LVL_LIN);   lua_setfield(L,-2,"LVL_LIN");
	lua_pushnumber (L, PARTICLESTREAMSPEC::LVL_SQRT);  lua_setfield(L,-2,"LVL_SQRT");
	lua_pushnumber (L, PARTICLESTREAMSPEC::LVL_PLIN);  lua_setfield(L,-2,"LVL_PLIN");
	lua_pushnumber (L, PARTICLESTREAMSPEC::LVL_PSQRT); lua_setfield(L,-2,"LVL_PSQRT");
	lua_pushnumber (L, PARTICLESTREAMSPEC::ATM_FLAT);  lua_setfield(L,-2,"ATM_FLAT");
	lua_pushnumber (L, PARTICLESTREAMSPEC::ATM_PLIN);  lua_setfield(L,-2,"ATM_PLIN");
	lua_pushnumber (L, PARTICLESTREAMSPEC::ATM_PLOG);  lua_setfield(L,-2,"ATM_PLOG");
	lua_setglobal (L, "PARTICLE");

	// some useful global constants
	lua_pushnumber (L, 0); lua_setglobal (L, "CLOSE");
	lua_pushnumber (L, 1); lua_setglobal (L, "OPEN");
	lua_pushnumber (L, 2); lua_setglobal (L, "UP");
	lua_pushnumber (L, 3); lua_setglobal (L, "DOWN");
	lua_pushnumber (L, ALLDOCKS); lua_setglobal (L, "ALLDOCKS");

	// predefined help contexts
	lua_pushstring (L, "intro.htm"); lua_setglobal (L, "orbiter");
	lua_pushstring (L, "script/ScriptRef.htm"); lua_setglobal (L, "api");

	// Panels
	lua_createtable(L, 0, 8);
	lua_pushnumber (L, PANEL_ATTACH_BOTTOM   ); lua_setfield(L,-2, "ATTACH_BOTTOM"  );
	lua_pushnumber (L, PANEL_ATTACH_TOP      ); lua_setfield(L,-2, "ATTACH_TOP"     );
	lua_pushnumber (L, PANEL_ATTACH_LEFT     ); lua_setfield(L,-2, "ATTACH_LEFT"    );
	lua_pushnumber (L, PANEL_ATTACH_RIGHT    ); lua_setfield(L,-2, "ATTACH_RIGHT"   );
	lua_pushnumber (L, PANEL_MOVEOUT_BOTTOM  ); lua_setfield(L,-2, "MOVEOUT_BOTTOM" );
	lua_pushnumber (L, PANEL_MOVEOUT_TOP     ); lua_setfield(L,-2, "MOVEOUT_TOP"    );
	lua_pushnumber (L, PANEL_MOVEOUT_LEFT    ); lua_setfield(L,-2, "MOVEOUT_LEFT"   );
	lua_pushnumber (L, PANEL_MOVEOUT_RIGHT   ); lua_setfield(L,-2, "MOVEOUT_RIGHT"  );
	lua_setglobal (L, "PANEL");

}

/***
General properties
@section vessel_mtd_props
*/

/***
Get vessel version.

This function returns the underlying C++ vessel version used by the vessel :

- 0: VESSEL
- 1: VESSEL2
- 2: VESSEL3
- 3: VESSEL4

@function version
@treturn number vessel version
*/
int Interpreter::v_version (lua_State *L)
{
	static const char *funcname = "version";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushinteger(L, v->Version());
	return 1;
}

/***
Get vessel handle.

@function get_handle
@treturn handle vessel handle
*/
int Interpreter::v_get_handle (lua_State *L)
{
	static const char *funcname = "get_handle";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	const OBJHANDLE hV = v->GetHandle();
	if (hV) lua_pushlightuserdata (L, hV);
	else lua_pushnil (L);
	return 1;
}

/***
Get vessel name.

Another way to obtain a vessel name is by using the oapi.get_objname function with
the vessel handle. So you can use either

	v = vessel.get_interface(hvessel)
	name = v:get_name()

or

	name = oapi.get_objname(hvessel)

@function get_name
@treturn string vessel name
@see vessel:get_classname
*/
int Interpreter::v_get_name (lua_State *L)
{
	static const char *funcname = "get_name";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushstring (L, v->GetName());
	GetInterpreter(L)->term_echo(L);
	return 1;
}

/***
Get vessel class name.
The class name identifies the vessel type.

@function get_classname
@treturn string vessel class name
@see vessel:get_name
*/
int Interpreter::v_get_classname (lua_State *L)
{
	static const char *funcname = "get_classname";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushstring (L, v->GetClassName());
	GetInterpreter(L)->term_echo(L);
	return 1;
}

/***
Get flight model realism level.

The following realism levels are currently supported:

- 0: simplified
- 1: realistic

The returned value corresponds to that passed to the VESSEL constructor.

A vessel implementation can use this flag to implement different flavours
of the flight model (e.g. handling suitable for beginners or advanced users),
by defining separate sets of parameters, e.g. higher fuel-specific impulse
and higher thrust ratings in the simplified model, less severe damage limits, etc.

@function get_flightmodel
@treturn int Flight model realism level
@see vessel:get_damagemodel
*/
int Interpreter::v_get_flightmodel (lua_State *L)
{
	static const char *funcname = "get_flightmodel";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetFlightModel());
	return 1;
}

/***
Get flight status.

Returns a bit flag defining the vessel's current flight status.

The following flags are currently defined:

- bit 0:

      - 0 = vessel is active (in flight),
      - 1 = vessel is inactive (landed)
- bit 1:

      - 0 = simple vessel (not docked to anything),
      - 1 = part of superstructure, (docked to another vessel)

@function get_flightstatus
@treturn number flight status bitfield
*/
int Interpreter::v_get_flightstatus (lua_State *L)
{
	static const char *funcname = "get_flightstatus";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetFlightStatus());
	return 1;
}

/***
Get damage model.

Returns the current user setting for damage and systems failure simulation.
The following settings are currently supported:

- 0: no damage or failures
- 1: simulate vessel damage and system failures

The return value depends on the user parameter selection in the Launchpad dialog.
It does not change during a simulation session and will be the same for all
vessels.

Future versions may support more differentiated bit flags to indicate different
types of damage and failure simulation.

A vessel implementation should query the damage flag to decide whether to simulate
failures.

@function get_damagemodel
@treturn int Damage modelling level
@see vessel:get_flightmodel
*/
int Interpreter::v_get_damagemodel (lua_State *L)
{
	static const char *funcname = "get_damagemodel";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetDamageModel());
	return 1;
}

/***
Get focus control status.

Returns true if the vessel can receive the input focus, false otherwise.

The vessel can be allowed or prohibited to receive the input focus by using the
@{vessel:set_enablefocus} method.

The initial state is defined by the EnableFocus setting in the vessel's
configuration file. If the entry is missing, the default is true.

Focus-enabled vessels can be selected by the user via the jump vessel dialog (F3).

Once a vessel has received the input focus, all user input via keyboard, mouse and
joystick is directed to this vessel.

For some object types, such as jettisoned rocket stages, enabling input focus may
not be useful.

@function get_enablefocus
@treturn bool Focus enabled status
@see vessel:set_enablefocus
*/
int Interpreter::v_get_enablefocus (lua_State *L)
{
	static const char *funcname = "get_enablefocus";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushboolean (L, v->GetEnableFocus());
	return 1;
}

/***
Set focus on vessel.

Enable or disable the vessel's ability to receive the input focus.

The initial state is defined by the EnableFocus setting in the vessel's
configuration file. If the entry is missing, the default is true.

If the input focus of the current focus vessel is disabled, it will continue to
receive user input, until the focus is switched to another vessel.

Focus-enabled vessels can be selected by the user via the jump vessel dialog (F3).

Once a vessel has received the input focus, all user input via keyboard, mouse and
joystick is directed to this vessel.

For some object types, such as jettisoned rocket stages, enabling input focus may
not be useful.

@function set_enablefocus
@tparam bool enable true to allow the vessel to receive input focus, false otherwise
@see vessel:get_enablefocus
*/
int Interpreter::v_set_enablefocus (lua_State *L)
{
	static const char *funcname = "set_enablefocus";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	bool enable = luamtd_toboolean_safe(L, 2, funcname);
	v->SetEnableFocus (enable);
	return 0;
}

/***
Get vessel size.

Returns the mean vessel radius.

Provides an approximate measure of the vessel size.

@function get_size
@treturn number mean vessel radius [m]
@see vessel:set_size
*/
int Interpreter::v_get_size (lua_State *L)
{
	static const char *funcname = "get_size";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetSize());
	GetInterpreter(L)->term_echo(L);
	return 1;
}

/***
Set vessel size.

Set the vessel's mean radius.

The size should correspond to the vessel's visual representation, for example the
mesh used to show the vessel in the simulation window.

The size parameter is used by Orbiter to determine the camera distance at which
the vessel is within visual range of the observer camera. It is also used for
calculating various physical parameters.

Usually this function is only called during vessel setup, and is only required
for vessels entirely configured by Lua script (see ScriptVessel).

@function set_size
@tparam number size new mean vessel radius [m]
@see vessel:get_size
*/
int Interpreter::v_set_size (lua_State *L)
{
	static const char *funcname = "set_size";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_Number size = luamtd_tonumber_safe(L, 2, funcname);
	v->SetSize(size);
	return 0;
}

/***
Get dry mass.

Returns the vessel's empty (dry) mass, excluding propellant mass.

@function get_emptymass
@treturn number vessel dry mass [kg]
@see vessel:set_emptymass
*/
int Interpreter::v_get_emptymass (lua_State *L)
{
	static const char *funcname = "get_emptymass";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetEmptyMass());
	GetInterpreter(L)->term_echo(L);
	return 1;
}

/***
Set dry mass.

Set the vessel's empty (dry) mass, excluding propellants.

The empty mass combines all parts of the vessel except propellant resources
defined via @{vessel:create_propellantresource}.

Use set_emptymass to account for structural changes such as stage or booster
separation, but not for fuel consumption, which is done directly by Orbiter.

@function set_emptymass
@tparam number emass vessel dry mass [kg]
@see vessel:get_emptymass
*/
int Interpreter::v_set_emptymass (lua_State *L)
{
	static const char *funcname = "set_emptymass";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_Number emass = luamtd_tonumber_safe(L, 2, funcname);
	v->SetEmptyMass(emass);
	return 0;
}

/***
Get vessel PMI.

Returns the vessel's mass-normalised principal moments of inertia.

The inertia tensor describes the behaviour of a rigid body under angular
acceleration. It is the analog of the body's mass in the linear case.

The values returned by this function are the diagonal elements of the inertia
tensor, in the local vessel frame of reference.

Orbiter's definition of PMI is mass-normalised, that is, the values are divided
by the total vessel mass.

Orbiter assumes that off-diagonal elements can be neglected, that is, that the
diagonal elements are the principal moments of inertia. This is usually a good
approximation when the vessel is sufficiently symmetric with respect to its
coordinate frame. Otherwise, a diagonalisation by rotating the local frame may be
required.

For more details, see "Inertia calculations for composite vessels" in "Orbiter Technical Reference".

@function get_pmi
@return (<i><b>@{types.vector|vector}</b></i>) diagonal elements of the mass-normalised inertia tensor [<b>m</b>&sup2;]
@see vessel:set_pmi
*/
int Interpreter::v_get_pmi (lua_State *L)
{
	static const char *funcname = "get_pmi";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 pmi;
	v->GetPMI (pmi);
	lua_pushvector (L, pmi);
	return 1;
}

/***
Set vessel PMI.

Set the vessel's mass-normalised principal moments of inertia (PMI).

PMI are the diagonal elements of the inertia tensor, which describes the
behaviour of a rigid body under angular acceleration.

For more details, see "Inertia calculations for composite vessels" in "Orbiter Technical Reference".

@function set_pmi
@param pmi (<i></b>@{types.vector|vector}</b></i>) pmi Diagonal elements of the vessel's mass-normalised inertia tensor [<b>m</b>&sup2;]
@see vessel:get_pmi
*/
int Interpreter::v_set_pmi (lua_State *L)
{
	static const char *funcname = "set_pmi";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 pmi = luamtd_tovector_safe(L, 2, funcname);
	v->SetPMI (pmi);
	return 0;
}

/***
Get vessel cross sections.

Returns the vessel's cross sections projected in the direction of the vessel's principal axes.

@function get_crosssections
@return (<i><b>@{types.vector|vector}</b></i>) Vector of cross sections of the vessel's projection into the yz, xz and xy planes, respectively [<b>m</b>&sup2;]
@see vessel:set_crosssections
*/
int Interpreter::v_get_crosssections (lua_State *L)
{
	static const char *funcname = "get_crosssections";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 cs;
	v->GetCrossSections (cs);
	lua_pushvector(L,cs);
	return 1;
}

/***
Set vessel cross sections.

Defines the vessel's cross-sectional areas, projected in the directions of the vessel's principal axes.

@function set_crosssections
@param cs (<i><b>@{types.vector|vector}</b></i>) Vector of cross-sectional areas of the vessel's projection along
the x-axis into yz-plane, along the y-axis into the xz-plane, and along the
z-axis into the xy plane, respectively [<b>m</b>&sup2;]
@see vessel:get_crosssections
*/
int Interpreter::v_set_crosssections (lua_State *L)
{
	static const char *funcname = "set_crosssections";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 cs = luamtd_tovector_safe(L, 2, funcname);
	v->SetCrossSections (cs);
	return 0;
}

/***
Get gravity damping coefficient.

Returns the vessel's damping coefficient for gravity field gradient-induced torque.

A nonspherical object in an inhomogeneous gravitational field experiences a
torque. This generates an undamped attitude oscillation in the vessel orbiting
the reference body.

Damping may occur due to tidal deformation of the vessel, movement of liquids
(fuel) etc.

If gravity gradient torque has been disabled in the launchpad dialog, this
function always returns 0.

@function get_gravitygradientdamping
@treturn number Torque damping coefficient (&ge; 0)
@see vessel:set_gravitygradientdamping, vessel:get_emptymass, vessel:get_pmi
*/
int Interpreter::v_get_gravitygradientdamping (lua_State *L)
{
	static const char *funcname = "get_gravitygradientdamping";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double ggd = v->GetGravityGradientDamping();
	lua_pushnumber(L,ggd);
	return 1;
}

/***
Set gravity damping coefficient.

Sets the vessel's damping coefficient for gravity field gradient-induced torque.

If gravity gradient torque has been disabled in the launchpad dialog, this
function returns false and has no other effect.

@function set_gravitygradientdamping
@tparam number ggd Torque damping coefficient.
@treturn bool <i>true</i> if damping coefficient was applied, <i>false</i> if gravity gradient torque is disabled.
@see vessel:get_gravitygradientdamping, vessel:set_emptymass, vessel:set_pmi
*/
int Interpreter::v_set_gravitygradientdamping (lua_State *L)
{
	static const char *funcname = "set_gravitygradientdamping";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double ggd = luamtd_tonumber_safe(L, 2, funcname);
	bool ok = v->SetGravityGradientDamping (ggd);
	lua_pushboolean (L, ok);
	return 1;
}

/***
Get number of touchdown points.

Returns the number of touchdown points defining the impact hull of the vessel.

@function get_touchdownpointcount
@treturn int Number of touchdown points
*/
int Interpreter::v_get_touchdownpointcount (lua_State *L)
{
	static const char *funcname = "get_touchdownpointcount";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushinteger(L, v->GetTouchdownPointCount());
	return 1;
}

/***
Get touchdown points.

Returns the three points defining the vessel's ground contact plane.

The function returns 3 reference points defining the vessel's surface contact
points when touched down on a planetary surface (e.g. landing gear).

@function get_touchdownpoints
@return (<i><b>@{types.vector|vector}</b></i>) touchdown point of nose wheel (or equivalent)
@return (<i><b>@{types.vector|vector}</b></i>) touchdown point of left main wheel (or equivalent)
@return (<i><b>@{types.vector|vector}</b></i>) touchdown point of right main wheel (or equivalent)
@see vessel:set_touchdownpoints
*/
int Interpreter::v_get_touchdownpoints (lua_State *L)
{
	static const char *funcname = "get_touchdownpoints";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);

	if (lua_gettop(L) >= 2) // new API
	{
		DWORD idx = luamtd_tointeger_safe(L, 2, funcname);
		TOUCHDOWNVTX tdvtx;
		if (v->GetTouchdownPoint(tdvtx, idx)) {
			lua_pushtouchdownvtx(L, tdvtx);
		}
		else {
			lua_pushnil(L);
		}
		return 1;
	}
	else // old API
	{
		VECTOR3 pt1, pt2, pt3;
		v->GetTouchdownPoints (pt1, pt2, pt3);
		lua_pushvector(L, pt1);
		lua_pushvector(L, pt2);
		lua_pushvector(L, pt3);
		return 3;
	}
}

/***
Set touchdown points.

Defines the three points defining the vessel's ground contact plane.

The points are the positions at which the vessel's undercarriage (or equivalent)
touches the surface, specified in local vessel coordinates.

The order of points is significant since it defines the direction of the normal.
The points should be specified such that the cross product pt3-pt1 x pt2-pt1
defines the horizon "up" direction for the landed vessel (given a left-handed
coordinate system).

Modifying the touchdown points during the simulation while the vessel is on the
ground can result in jumps due to instantaneous position changes (infinite
acceleration). To avoid this, the touchdown points should be modified gradually
by small amounts over time (proportional to simulation time steps).

@function set_touchdownpoints
@param pt1 (<i><b>@{types.vector|vector}</b></i>) touchdown point of nose wheel (or equivalent)
@param pt2 (<i><b>@{types.vector|vector}</b></i>) touchdown point of left main wheel (or equivalent)
@param pt3 (<i><b>@{types.vector|vector}</b></i>) touchdown point of right main wheel (or equivalent)
@see vessel:get_touchdownpoints
*/
int Interpreter::v_set_touchdownpoints (lua_State *L)
{
	static const char *funcname = "set_touchdownpoints";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);

	if (lua_gettop(L) == 2 && lua_istable(L, -1)) // new API
	{
		AssertPrmType(L, -1, PRMTP_TABLE, funcname);
		TOUCHDOWNVTX *tdvtx;
		DWORD ntdvtx, nbuf;

		ntdvtx = nbuf = 0;
		lua_pushnil(L);
		while (lua_next(L, -2)) {
			if (lua_istouchdownvtx(L, -1)) {
				if (ntdvtx == nbuf) { // grow buffer
					TOUCHDOWNVTX *tmp = new TOUCHDOWNVTX[nbuf += 3];
					if (ntdvtx) {
						memcpy(tmp, tdvtx, ntdvtx * sizeof(TOUCHDOWNVTX));
						delete[]tdvtx;
					}
					tdvtx = tmp;
				}
				tdvtx[ntdvtx++] = lua_totouchdownvtx(L, -1);
			}
			lua_pop(L, 1);
		}
		ASSERT_SYNTAX(ntdvtx >= 3, "Too few arguments");

		v->SetTouchdownPoints(tdvtx, ntdvtx);

		delete[]tdvtx;
	}
	else // old API
	{
		//AssertMtdMinPrmCount(L, 4, funcname);
		VECTOR3 pt1 = luamtd_tovector_safe(L, 2, funcname);
		VECTOR3 pt2 = luamtd_tovector_safe(L, 3, funcname);
		VECTOR3 pt3 = luamtd_tovector_safe(L, 4, funcname);
		v->SetTouchdownPoints(pt1, pt2, pt3);
	}
	return 0;
}

/***
Set vessel's visibility range.

This function can be used to define the distance up to which a vessel is visible,
independent of screen resolution.

The vislimit value is the limiting apparent size (as a fraction of the render
window vertical) up to which the vessel is regarded visible. Thus, the vessel is
visible if the following condition is satisfied:

S / (d tan a) > vislimit

where S is the vessel size, d is its camera distance, and a is the camera aperture.

If the defined visibility limit exceeds the distance at which the vessel can be
rendered as a mesh at the given screen resolution, it will simply be represented
by a circular spot whose size is reduced linearly (to reach zero at the limiting
distance).

If the vessel is to be visible beyond its geometric size (e.g. due to light
beacons etc.) then the spotlimit value can be used to define the limiting
distance due to the vessel's geometry, while vislimit defines the total
visibility range including all enhancing factors such as beacons.

spotlimit ? vislimit is required. If spotlimit < 0 (default), then spotlimit
= vislimit is assumed.

If set_visibilitylimit is not called, then the default value is vislimit =
spotlimit = 1e-3.

@function set_visibilitylimit
@tparam number vislimit apparent size limit for vessel visibility
@tparam[opt] number splotlimit apparent size limit for vessel "spot" representation.
@see vessel:set_size, vessel:set_clipradius
*/
int Interpreter::v_set_visibilitylimit (lua_State *L)
{
	static const char *funcname = "set_visibilitylimit";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double vislimit, spotlimit = -1.0;
	vislimit = luamtd_tonumber_safe(L, 2, funcname);
	if (lua_gettop (L) > 2) {
		spotlimit = luamtd_tonumber_safe(L, 3, funcname);
	}
	v->SetVisibilityLimit (vislimit, spotlimit);
	return 0;
}

/***
Get the radius of the vessel's circumscribing sphere.

This parameter describes the radius of the sphere around the vessel that is
protected from clipping at the observer camera's near clipping plane. (The
near clipping plane defines an area around the view camera within which no
objects are rendered. The distance of the near clipping plane cannot be made
arbitrarily small for technical reasons.)

By default, the clip radius is identical to the vessel's "Size" parameter.
However, the size parameter is correlated to physical vessel properties and
may therefore be smaller than the sphere that contains the vessel's complete
visual representation. In that case, defining a clip radius that is larger
than the size parameter can avoid visual artefacts.

The view camera's near clip plane distance is adjusted so that it does not
intersect any nearby vessel's clip radius. However, there is a minimum near
clip distance of 2.5m. This means that if the camera approaches a vessel to
less than clip radius + 2.5, clipping may still occur.

Visual cockpit meshes are rendered in a separate pass and are not affected by
the general near clip distance (they have a separate near clip distance of
10cm).

@function get_clipradius
@treturn number Radius of the circumscribing sphere of the vessel's visual
representation [m].
@see set_clipradius, get_size
				*/
int Interpreter::v_get_clipradius (lua_State *L)
{
	static const char *funcname = "get_clipradius";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);

	lua_pushnumber(L, v->GetClipRadius());
	return 1;
}

/***
Set the average colour distribution reflected by the vessel.

The colour passed to this function is currently used to define the "spot"
colour with which the vessel is rendered at long distances. It should
represent an average colour and brightness of the vessel surface when fully
lit.

The values for each of the RGB components should be in the range 0-1.

The default vessel albedo is bright white (1,1,1).

The albedo can be overridden by the AlbedoRGB entry in the vessel's config
file.

@function set_albedoRGB
@tparam vector albedo vessel colour vector (red, green blue), range [0..1]
for each component.
*/
int Interpreter::v_set_albedoRGB (lua_State *L)
{
	static const char *funcname = "set_albedoRGB";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 albedo = luamtd_tovector_safe(L, 2, funcname);
	v->SetAlbedoRGB(albedo);
	return 0;
}

/***
Set the radius of the vessel's circumscribing sphere.

This parameter describes the radius of the sphere around the
   vessel that is protected from clipping at the observer camera's
   near clipping plane. (The near clipping plane defines an area
   around the view camera within which no objects are rendered. The
   distance of the near clipping plane cannot be made arbitrarily
   small for technical reasons.)

By default, the clip radius is identical to the vessel's "Size"
   parameter. However, the size parameter is correlated to physical
   vessel properties and may therefore be smaller than the sphere
   that contains the vessel's complete visual representation. In that
   case, defining a clip radius that is larger than the size
   parameter can avoid visual artefacts.

The view camera's near clip plane distance is adjusted so that it
   does not intersect any nearby vessel's clip radius. However, there
   is a minimum near clip distance of 2.5m. This means that if the
   camera approaches a vessel to less than clip radius + 2.5,
   clipping may still occur.

Visual cockpit meshes are rendered in a separate pass and are not
   affected by the general near clip distance (they have a separate
   near clip distance of 10cm).

Setting rad = 0 reverts to the default behaviour of using the
   vessel's "Size" parameter to determine the clip radius.

@function set_clipradius
@tparam number rad Radius of the circumscribing sphere of the vessel's visual
representation [m].
@see get_clipradius, set_size
*/
int Interpreter::v_set_clipradius (lua_State *L)
{
	static const char *funcname = "set_clipradius";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double rad = luamtd_tonumber_safe(L ,2, funcname);
	v->SetClipRadius (rad);
	return 0;
}

/***
Set friction coefficients for ground contact.

The coefficients of surface friction define the deceleration
   forces during sliding or rolling over a surface. mu_lng is the
   coefficient acting in longitudinal (forward) direction, mu_lat the
   coefficient acting in lateral (sideways) direction. The friction
   forces are proportional to the coefficient and the weight of the
   vessel:
   <center><b>F</b><sub>friction</sub> = mu <b>G</b></center>

The higher the coefficient, the faster the vessel will come to a
   halt.

Typical parameters for a spacecraft equipped with landing wheels
   would be mu_lng = 0.1 and mu_lat = 0.5. If the vessel hasn't got
   wheels, mu_lng = 0.5.

The coefficients should be adjusted for belly landings when the
   landing gear is retracted.

The longitudinal and lateral directions are defined by the
   touchdown points:
   <center><b>s</b><sub>lng</sub> = <b>p</b><sub>0</sub> - (<b>p</b><sub>1</sub>
   + <b>p</b><sub>2</sub>)/2, &nbsp;
   <b>s</b><sub>lat</sub> = <b>p</b><sub>2</sub> - <b>p</b><sub>1</sub></center>

@function set_surfacefrictioncoeff
@tparam number mu_lng friction coefficient in longitudinal direction.
@tparam number mu_lat friction coefficient in lateral direction.
@see set_touchdownpoints
*/
int Interpreter::v_set_surfacefrictioncoeff (lua_State *L)
{
	static const char *funcname = "set_surfacefrictioncoeff";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double mu_lng = luamtd_tonumber_safe(L, 2, funcname);
	double mu_lat = luamtd_tonumber_safe(L, 3, funcname);
	v->SetSurfaceFrictionCoeff(mu_lng, mu_lat);
	return 0;
}

/***
Elevation of the vessel's centre of gravity (COG) above ground.

The COG elevation is defined as the normal distance of the vessel's
   centre of gravity from the ground contact plane defined by its
   three touchdown points.

By definition, the vessel's centre of gravity coincides with the
   origin of the local vessel frame.

@function get_COG_elev
@treturn number Distance of COG from vessel ground contact plane [m].
@see get_touchdownpoints, set_touchdownpoints
*/
int Interpreter::v_get_COG_elev (lua_State *L)
{
	static const char *funcname = "get_COG_elev";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber(L, v->GetCOG_elev());
	return 1;
}

/***
Vessel state
@section vessel_mtd_state
*/

/***
Get current total mass.

@function get_mass
@treturn number current total vessel mass [kg]
*/
int Interpreter::v_get_mass (lua_State *L)
{
	static const char *funcname = "get_mass";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetMass());
	GetInterpreter(L)->term_echo(L);
	return 1;
}

/***
Get position vector in global coordinates.

Orbiter's global reference frame is the solar system's barycentric
ecliptic frame at epoch J2000.0.

@function get_globalpos
@return (<i><b>@{types.vector|vector}</b></i>) cartesian position vector [<b>m</b>]
@see vessel:get_globalvel, vessel:get_relativepos
*/
int Interpreter::v_get_globalpos (lua_State *L)
{
	static const char *funcname = "get_globalpos";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 pos;
	v->GetGlobalPos (pos);
	lua_pushvector (L, pos);
	return 1;
}

/***
Get velocity vector in global coordinates.

Orbiter's global reference frame is the solar system's barycentric
ecliptic frame at epoch J2000.0.

@function get_globalvel
@return (<i><b>@{types.vector|vector}</b></i>) cartesian velocity vector [<b>m/s</b>]
@see vessel:get_globalpos, vessel:get_relativevel
*/
int Interpreter::v_get_globalvel (lua_State *L)
{
	static const char *funcname = "get_globalvel";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 vel;
	v->GetGlobalVel (vel);
	lua_pushvector (L, vel);
	return 1;
}

/***
Get current position with respect to another object.

This function returns the vessel's position relative to the position of the
object defined by handle `href`.

Results are returned in the ecliptic frame (ecliptic and equinox of J2000.0).

@function get_relativepos
@tparam handle href reference object handle
@return (<i><b>@{types.vector|vector}</b></i>) relative position vector [<b>m</b>]
@see vessel:get_globalpos, vessel:get_relativevel
*/
int Interpreter::v_get_relativepos (lua_State *L)
{
	static const char *funcname = "get_relativepos";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	OBJHANDLE hRef = (OBJHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	VECTOR3 pos;
	v->GetRelativePos (hRef, pos);
	lua_pushvector (L, pos);
	return 1;
}

/***
Get current velocity relative to another object.

This function returns the vessel's velocity relative to the velocity of the
object defined by handle `href`.

Results are returned in the ecliptic frame (ecliptic and equinox of J2000.0).

@function get_relativevel
@tparam handle href reference object handle
@return (<i><b>@{types.vector|vector}</b></i>) relative velocity vector [<b>m/s</b>]
@see vessel:get_globalvel, vessel:get_relativepos
*/
int Interpreter::v_get_relativevel (lua_State *L)
{
	static const char *funcname = "get_relativevel";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	OBJHANDLE hRef = (OBJHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	VECTOR3 vel;
	v->GetRelativeVel (hRef, vel);
	lua_pushvector (L, vel);
	return 1;
}

/***
Get current rotation matrix in the global frame.

The returned matrix can be multiplied with a direction vector in the vessel's
local frame to rotate it into the global frame.

The transpose of the returned matrix can be multiplied with a direction vector
in the global frame to rotate it into the vessel's local frame.

Together with @{get_globalpos}, a point can be transformed from the vessel's
local frame to the global frame and vice versa. Example:

	vglob = v:get_globalpos
	vR = v:get_rotationmatrix()
	-- a point in the local vessel frame
	ploc = {x=1,y=2,z=3}
	-- the point transformed to the global frame
	pglob = vec.add(vglob, mat.mul(vR, ploc))

The global frame is defined by the ecliptic and equinox of J2000.0, with
origin at the solar system's barycentre.

@function get_rotationmatrix
@return (<i><b>@{types.matrix|matrix}</b></i>) rotation matrix
@see vessel:get_globalpos
*/
int Interpreter::v_get_rotationmatrix (lua_State *L)
{
	static const char *funcname = "get_rotationmatrix";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	MATRIX3 rot;
	v->GetRotationMatrix (rot);
	lua_pushmatrix (L, rot);
	return 1;
}

/***
Get vessel status.

Returns the vessel's current status parameters as a Lua table.

@function get_status
@tparam[opt=2] number status version
@treturn table vessel status
*/
int Interpreter::v_get_status(lua_State* L)
{
	static const char* funcname = "get_status";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);

	// For version 2 (or greater) the caller has to set version number as 1st parameter
	DWORD version = (lua_gettop(L) >= 2) ? lua_tointeger(L, 2) : 2; // default to version 2
	ASSERT_SYNTAX((version > 0 && version <= 2), "Invalid version");

	if (version == 1)
	{
		VESSELSTATUS status = { 0 };
		v->GetStatus(status);
		lua_push_vessel_status(L, status);
		return 1;
	}
	else if (version == 2)
	{
		VESSELSTATUS2 status = { 0 };
		status.version = 2;
		// check additional flags...(currently not implemented)
		status.flag |= VS_FUELLIST | VS_THRUSTLIST | VS_DOCKINFOLIST;
		//status.fuel = new VESSELSTATUS2::FUELSPEC[256]();

		v->GetStatusEx(&status);
		lua_push_vessel_status(L, status);

		// Who frees these resources?
		//if (status.fuel) delete[] status.fuel;
		//if (status.thruster) delete[] status.thruster;
		//if (status.dockinfo) delete[] status.dockinfo;
		return 1;
	}
	return 0;
}

/***
Get vessel status.

Returns the vessel's current status parameters as an object.

@function get_status
@tparam[opt=2] number v status version
@treturn vesselstatus|vesselstatus2 vessel status
*/
int Interpreter::v_get_rawstatus(lua_State* L)
{
	static const char* funcname = "get_rawstatus";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);

	// For version 2 (or greater) the caller has to set version number as 1st parameter
	DWORD version = (lua_gettop(L) >= 2) ? lua_tointeger(L, 2) : 2; // default to version 2
	ASSERT_SYNTAX((version > 0 && version <= 2), "Invalid version");

	if (version == 1)
	{
		VESSELSTATUS *status = (VESSELSTATUS *)lua_newuserdata(L, sizeof(VESSELSTATUS));
		memset(status, 0, sizeof(VESSELSTATUS));
		luaL_getmetatable(L, "VESSELSTATUS.table");   // push metatable
		lua_setmetatable(L, -2);              // set metatable for annotation objects

		v->GetStatus(*status);
		return 1;
	}
	else if (version == 2)
	{
		VESSELSTATUS2* status = (VESSELSTATUS2*)lua_newuserdata(L, sizeof(VESSELSTATUS2));
		memset(status, 0, sizeof(VESSELSTATUS2));
		luaL_getmetatable(L, "VESSELSTATUS2.table");   // push metatable
		lua_setmetatable(L, -2);              // set metatable for annotation objects
		status->version = 2;
		// check additional flags...(currently not implemented)
		status->flag |= VS_FUELLIST | VS_THRUSTLIST | VS_DOCKINFOLIST;
		//status.fuel = new VESSELSTATUS2::FUELSPEC[256]();

		v->GetStatusEx(status);

		// Who frees these resources?
		//if (status.fuel) delete[] status.fuel;
		//if (status.thruster) delete[] status.thruster;
		//if (status.dockinfo) delete[] status.dockinfo;
		return 1;
	}
	return 0;
}

int Interpreter::vsget(lua_State* L)
{
	static const char* funcname = "vsget";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSELSTATUS* vs = (VESSELSTATUS*)lua_touserdata(L, 1);
	lua_push_vessel_status(L, *vs);
	return 1;
}

int Interpreter::vs2get(lua_State* L)
{
	static const char* funcname = "vs2get";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSELSTATUS2* vs = (VESSELSTATUS2*)lua_touserdata(L, 1);
	lua_push_vessel_status(L, *vs);
	return 1;
}

int Interpreter::vsset(lua_State* L)
{
	static const char* funcname = "vsset";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSELSTATUS* vs = (VESSELSTATUS*)lua_touserdata(L, 1);
	// Extract known values from table
	lua_getfield(L, 2, "rpos");

	if (lua_isvector(L, -1)) vs->rpos = lua_tovector(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "rvel");
	if (lua_isvector(L, -1)) vs->rvel = lua_tovector(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "vrot");
	if (lua_isvector(L, -1)) vs->vrot = lua_tovector(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "arot");
	if (lua_isvector(L, -1)) vs->arot = lua_tovector(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "fuel");
	if (lua_isnumber(L, -1)) vs->fuel = lua_tonumber(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "eng_main");
	if (lua_isnumber(L, -1)) vs->eng_main = lua_tonumber(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "eng_hovr");
	if (lua_isnumber(L, -1)) vs->eng_hovr = lua_tonumber(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "rbody");
	if (lua_islightuserdata(L, -1)) vs->rbody = lua_toObject(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "base");
	if (lua_islightuserdata(L, -1)) vs->base = lua_toObject(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "port");
	if (lua_isnumber(L, -1)) vs->port = lua_tointeger(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "status");
	if (lua_isnumber(L, -1)) vs->status = lua_tointeger(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "vdata");
	if (lua_isvector(L, -1)) vs->vdata[0] = lua_tovector(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "fdata");
	if (lua_isnumber(L, -1)) vs->fdata[0] = lua_tonumber(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "flag");
	if (lua_isnumber(L, -1)) vs->flag[0] = lua_tointeger(L, -1);
	lua_pop(L, 1);
	return 0;
}

int Interpreter::vs2set(lua_State* L)
{
	static const char* funcname = "vs2set";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSELSTATUS2* vs = (VESSELSTATUS2*)lua_touserdata(L, 1);

	// Extract known values from table
	lua_getfield(L, 2, "flags");
	if (lua_isnumber(L, -1)) vs->flag = lua_tointeger(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "port");
	if (lua_isnumber(L, -1)) vs->port = lua_tointeger(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "status");
	if (lua_isnumber(L, -1)) vs->status = lua_tointeger(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "surf_lng");
	if (lua_isnumber(L, -1)) vs->surf_lng = lua_tonumber(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "surf_lat");
	if (lua_isnumber(L, -1)) vs->surf_lat = lua_tonumber(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "surf_hdg");
	if (lua_isnumber(L, -1)) vs->surf_hdg = lua_tonumber(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "rpos");
	if (lua_isvector(L, -1)) vs->rpos = lua_tovector(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "rvel");
	if (lua_isvector(L, -1)) vs->rvel = lua_tovector(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "vrot");
	if (lua_isvector(L, -1)) vs->vrot = lua_tovector(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "arot");
	if (lua_isvector(L, -1)) vs->arot = lua_tovector(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "rbody");
	if (lua_islightuserdata(L, -1)) vs->rbody = lua_toObject(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "base");
	if (lua_islightuserdata(L, -1)) vs->base = lua_toObject(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 2, "xpdr");
	if (lua_isnumber(L, -1)) vs->xpdr = lua_tointeger(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 2, "fuel");
	if (lua_istable(L, -1)) {
		int n = lua_rawlen(L, -1);
		if (n) {
			vs->fuel = new VESSELSTATUS2::FUELSPEC[n]();

			lua_pushnil(L);
			while (lua_next(L, -2)) {
				// stack now contains: -1 => value; -2 => key; -3 => table (value is FUELSPEC)
				if (lua_istable(L, -1)) // is_fuelspec() ?
				{
					size_t i = lua_tointeger(L, -2) - 1; //lua_next() does not always iteerate in order :(
					lua_getfield(L, -1, "idx");
					vs->fuel[i].idx = lua_tointeger(L, -1);
					lua_pop(L, 1);
					lua_getfield(L, -1, "level");
					vs->fuel[i].level = lua_tonumber(L, -1);
					lua_pop(L, 1);
					vs->nfuel++;
				}
				lua_pop(L, 1);
			}
		}
	}
	lua_pop(L, 1);

	lua_getfield(L, 2, "thruster");
	if (lua_istable(L, -1)) {
		int n = lua_rawlen(L, -1);
		if (n) {
			vs->thruster = new VESSELSTATUS2::THRUSTSPEC[n]();
			lua_pushnil(L);
			while (lua_next(L, -2)) {
				if (lua_istable(L, -1)) { // is_thrustspec()
					size_t i = lua_tointeger(L, -2) - 1;
					lua_getfield(L, -1, "idx");
					vs->thruster[i].idx = lua_tointeger(L, -1);
					lua_pop(L, 1);
					lua_getfield(L, -1, "level");
					vs->thruster[i].level = lua_tonumber(L, -1);
					lua_pop(L, 1);
					vs->nthruster++;
				}
				lua_pop(L, 1);
			}
		}
	}
	lua_pop(L, 1);

	lua_getfield(L, 2, "dockinfo");
	if (lua_istable(L, -1)) {
		int n = lua_rawlen(L, -1);
		if (n) {
			vs->dockinfo = new VESSELSTATUS2::DOCKINFOSPEC[n]();
			lua_pushnil(L);
			while (lua_next(L, -2)) {
				if (lua_istable(L, -1)) { // is_dockinfospec()
					size_t i = lua_tointeger(L, -2) - 1;
					lua_getfield(L, -1, "idx");
					vs->dockinfo[i].idx = lua_tointeger(L, -1);
					lua_pop(L, 1);
					lua_getfield(L, -1, "ridx");
					vs->dockinfo[i].ridx = lua_tointeger(L, -1);
					lua_pop(L, 1);
					lua_getfield(L, -1, "rvessel");
					vs->dockinfo[i].rvessel = lua_touserdata(L, -1);
					lua_pop(L, 1);
					vs->ndockinfo++;
				}
				lua_pop(L, 1);
			}
		}
	}
	lua_pop(L, 1);

	

	return 0;
}

/***
Set default vessel status parameters.

@function defset_status
@tparam number v status version
@tparam table status vessel status
*/
int Interpreter::v_defset_status (lua_State *L)
{
	static const char *funcname = "defset_status";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);

	int version = lua_get_vesselstatus_version(L, 2);
	ASSERT_SYNTAX((version > 0 && version <= 2), "Invalid version or not a valid VESSELSTATUS table");

	if (version == 1)
	{
		VESSELSTATUS status = {0};
		v->GetStatus(status);

		// Extract known values from table
		lua_getfield(L, 2, "rpos");
		if (lua_isvector(L, -1)) status.rpos = lua_tovector(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "rvel");
		if (lua_isvector(L, -1)) status.rvel = lua_tovector(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "vrot");
		if (lua_isvector(L, -1)) status.vrot = lua_tovector(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "arot");
		if (lua_isvector(L, -1)) status.arot = lua_tovector(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "fuel");
		if (lua_isnumber(L, -1)) status.fuel = lua_tonumber(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "eng_main");
		if (lua_isnumber(L, -1)) status.eng_main = lua_tonumber(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "eng_hovr");
		if (lua_isnumber(L, -1)) status.eng_hovr = lua_tonumber(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "rbody");
		if (lua_islightuserdata(L, -1)) status.rbody = lua_toObject(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "base");
		if (lua_islightuserdata(L, -1)) status.base = lua_toObject(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "port");
		if (lua_isnumber(L, -1)) status.port = lua_tointeger(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "status");
		if (lua_isnumber(L, -1)) status.status = lua_tointeger(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "vdata");
		if (lua_isvector(L, -1)) status.vdata[0] = lua_tovector(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "fdata");
		if (lua_isnumber(L, -1)) status.fdata[0] = lua_tonumber(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "flag");
		if (lua_isnumber(L, -1)) status.flag[0] = lua_tointeger(L, -1);
		lua_pop(L, 1);

		// Apply changes
		v->DefSetState(&status);
	}
	else if (version == 2)
	{
		VESSELSTATUS2 status = {0};
		status.version = 2;
		v->GetStatusEx(&status);

		// Extract known values from table
		lua_getfield(L, 2, "flags");
		if (lua_isnumber(L, -1)) status.flag = lua_tointeger(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "port");
		if (lua_isnumber(L, -1)) status.port = lua_tointeger(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "status");
		if (lua_isnumber(L, -1)) status.status = lua_tointeger(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "surf_lng");
		if (lua_isnumber(L, -1)) status.surf_lng = lua_tonumber(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "surf_lat");
		if (lua_isnumber(L, -1)) status.surf_lat = lua_tonumber(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "surf_hdg");
		if (lua_isnumber(L, -1)) status.surf_hdg = lua_tonumber(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "rpos");
		if (lua_isvector(L, -1)) status.rpos = lua_tovector(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "rvel");
		if (lua_isvector(L, -1)) status.rvel = lua_tovector(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "vrot");
		if (lua_isvector(L, -1)) status.vrot = lua_tovector(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "arot");
		if (lua_isvector(L, -1)) status.arot = lua_tovector(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "rbody");
		if (lua_islightuserdata(L, -1)) status.rbody = lua_toObject(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "base");
		if (lua_islightuserdata(L, -1)) status.base = lua_toObject(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 2, "xpdr");
		if (lua_isnumber(L, -1)) status.xpdr = lua_tointeger(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 2, "fuel");
		if (lua_istable(L, -1)) {
			int n = lua_rawlen(L, -1);
			if (n) {
				status.fuel = new VESSELSTATUS2::FUELSPEC[n]();

				lua_pushnil(L);
				while (lua_next(L, -2)) {
					// stack now contains: -1 => value; -2 => key; -3 => table (value is FUELSPEC)
					if (lua_istable(L, -1)) // is_fuelspec() ?
					{
						size_t i = lua_tointeger(L, -2) - 1; //lua_next() does not always iteerate in order :(
						lua_getfield(L, -1, "idx");
						status.fuel[i].idx = lua_tointeger(L, -1);
						lua_pop(L, 1);
						lua_getfield(L, -1, "level");
						status.fuel[i].level = lua_tonumber(L, -1);
						lua_pop(L, 1);
						status.nfuel++;
					}
					lua_pop(L, 1);
				}
			}
		}
		lua_pop(L, 1);

		lua_getfield(L, 2, "thruster");
		if (lua_istable(L, -1)) {
			int n = lua_rawlen(L, -1);
			if (n) {
				status.thruster = new VESSELSTATUS2::THRUSTSPEC[n]();
				lua_pushnil(L);
				while (lua_next(L, -2)) {
					if (lua_istable(L, -1)) { // is_thrustspec()
						size_t i = lua_tointeger(L, -2) - 1;
						lua_getfield(L, -1, "idx");
						status.thruster[i].idx = lua_tointeger(L, -1);
						lua_pop(L, 1);
						lua_getfield(L, -1, "level");
						status.thruster[i].level = lua_tonumber(L, -1);
						lua_pop(L, 1);
						status.nthruster++;
					}
					lua_pop(L, 1);
				}
			}
		}
		lua_pop(L, 1);

		lua_getfield(L, 2, "dockinfo");
		if (lua_istable(L, -1)) {
			int n = lua_rawlen(L, -1);
			if (n) {
				status.dockinfo = new VESSELSTATUS2::DOCKINFOSPEC[n]();
				lua_pushnil(L);
				while (lua_next(L, -2)) {
					if (lua_istable(L, -1)) { // is_dockinfospec()
						size_t i = lua_tointeger(L, -2) - 1;
						lua_getfield(L, -1, "idx");
						status.dockinfo[i].idx = lua_tointeger(L, -1);
						lua_pop(L, 1);
						lua_getfield(L, -1, "ridx");
						status.dockinfo[i].ridx = lua_tointeger(L, -1);
						lua_pop(L, 1);
						lua_getfield(L, -1, "rvessel");
						status.dockinfo[i].rvessel = lua_touserdata(L, -1);
						lua_pop(L, 1);
						status.ndockinfo++;
					}
					lua_pop(L, 1);
				}
			}
		}
		lua_pop(L, 1);

		// Apply changes
		v->DefSetStateEx(&status);

		// Who frees these resources?
		//if (status.fuel) delete[] status.fuel;
		//if (status.thruster) delete[] status.thruster;
		//if (status.dockinfo) delete[] status.dockinfo;
	}
	return 0;
}

/***
Get angular velocity.

Returns the vessel's angular velocity components around its principal axes
as a vector.

@function get_angvel
@return (<i><b>@{types.vector|vector}</b></i>) angular velocity components [<b>rad/s</b>]
@see vessel:set_angvel
*/
int Interpreter::v_get_angvel (lua_State *L)
{
	static const char *funcname = "get_angvel";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 av;
	v->GetAngularVel (av);
	lua_pushvector (L, av);
	return 1;
}

/***
Set angular velocity.

Sets the vessel's angular velocity components around its principal axes.

@function set_angvel
@param av (<i><b>@{types.vector|vector}</b></i>) angular velocity components [<b>rad/s</b>]
@see vessel:get_angvel
*/
int Interpreter::v_set_angvel (lua_State *L)
{
	static const char *funcname = "set_angvel";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 av = luamtd_tovector_safe(L, 2, funcname);
	v->SetAngularVel (av);
	return 0;
}

/***
Get landing status.

If the vessel is inactive (landed on a planetary surfaces),
this function returns the handle of the celestial body the vessel is landed on.
Otherwise it returns nil.

@function is_landed
@treturn handle Landing target handle
@see vessel:get_groundcontact, vessel:get_surfaceref
*/
int Interpreter::v_is_landed (lua_State *L)
{
	static const char *funcname = "is_landed";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DWORD status = v->GetFlightStatus();
	if (status & 1) {
		OBJHANDLE hBody = v->GetSurfaceRef ();
		lua_pushlightuserdata (L, hBody);
	} else lua_pushnil (L);
	return 1;
}

/***
Planetary contct.

Returns a flag indicating contact with a planetary surface.

_true_ indicates ground contact (at least one of the vessel's touchdown
reference points is in contact with a planet surface).

@function get_groundcontact
@treturn bool ground contact status
@see vessel:is_landed, vessel:get_surfaceref
*/
int Interpreter::v_get_groundcontact (lua_State *L)
{
	static const char *funcname = "get_groundcontact";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushboolean (L, v->GroundContact() ? 1:0);
	return 1;
}

/***
Angular acceleration.

Returns the vessel's current angular acceleration components around its
principal axes.

The returned vector contains the angular accelerations
   \f$ \partial\omega_x/\partial t, \partial\omega_y/\partial t, \partial\omega_z/\partial t \f$
   around the vessel's x, y and z axes, in the rotating vessel frame.

@function get_angularacc
@return (<i><b>@{types.vector|vector}</b></i>) angular acceleration [<b>rad/s&sup2;</b>]
@see vessel:get_angvel, vessel:get_angularmoment
*/
int Interpreter::v_get_angularacc (lua_State *L)
{
	static const char *funcname = "get_angularacc";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 aacc;
	v->GetAngularAcc(aacc);
	lua_pushvector(L, aacc);
	return 1;
}

/***
Linear moment.

Returns the linear force vector currently acting on the vessel.

The returned vector is the vector sum of all forces (gravity,
   thrust, aerodynamic forces, etc.) currently acting on the vessel.

@function get_linearmoment
@return (<i><b>@{types.vector|vector}</b></i>) force vector in vessel coordinates [<b>N</b>]
@see get_angularmoment
*/
int Interpreter::v_get_linearmoment (lua_State *L)
{
	static const char *funcname = "get_linearmoment";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 F;
	v->GetLinearMoment(F);
	lua_pushvector(L, F);
	return 1;
}

/***
Angular moment.

Returns the sum of angular moments currently acting on the vessel.

Given all force components <b>F</b><sub>i</sub> acting on the vessel at
   positions <b>r</b><sub>i</sub>, the angular moment is defined as
   \f[ \vec{M} = \sum_i \vec{F}_i \times \vec{r}_i \f]
   (note the left-handed reference frame in the order of operands for the
   cross product).

@function get_angularmoment
@return (<i><b>@{types.vector|vector}</b></i>) angular moment [<b>Nm</b>]
@see get_linearmoment
*/
int Interpreter::v_get_angularmoment (lua_State *L)
{
	static const char *funcname = "get_angularmoment";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 amom;
	v->GetAngularMoment(amom);
	lua_pushvector(L, amom);
	return 1;
}

/***
Get global orientation.

Returns the Euler angles defining the vessel's orientation.

@function get_globalorientation
@return (<i><b>@{types.vector|vector}</b></i>) vector containing the three Euler angles [<b>rad</b>]
@see set_globalorientation, get_rotationmatrix
*/
int Interpreter::v_get_globalorientation (lua_State *L)
{
	static const char *funcname = "get_globalorientation";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 arot;
	v->GetGlobalOrientation(arot);
	lua_pushvector(L, arot);
	return 1;
}

/***
Sets the vessel's orientation via Euler angles.

@function set_globalorientation
@tparam vector arot vector containing the set of Euler angles [<b>rad</b>]
@see get_globalorientation, set_rotationmatrix
*/
int Interpreter::v_set_globalorientation (lua_State *L)
{
	static const char *funcname = "set_globalorientation";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 arot = luamtd_tovector_safe(L, 2, funcname);
	v->SetGlobalOrientation(arot);
	return 0;
}

/***
Orbit stabilisation.

Returns whether orbit stabilisation is used for the vessel at the
current time step.

A vessel switches to orbit stabilisation only if the user has
   enabled it in the launchpad dialog, and the user-defined
   perturbation and time step limits are currently satisfied.

Stabilised mode reduces the effect of deteriorating orbits due
   to accumulating numerical errors in the state vector propagation,
   but is limited in handling multiple gravitational sources.

@function is_orbitstabilised
@treturn bool _true_ indicates that the vessel's state is currently updated
   by using the stabilisation algorithm, which calculates the osculating
   elements with respect to the primary gravitational source, and treats all
   additional forces as perturbations.
@see get_elements
*/
int Interpreter::v_is_orbitstabilised (lua_State *L)
{
	static const char *funcname = "is_orbitstabilised";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushboolean(L, v->OrbitStabilised());
	return 1;
}

/***
Nonspherical gravity perturbations.

Indicates whether the vessel considers gravity field perturbations
due to nonspherical planet shapes when updating its state vectors for
the current time step.

This function will always return false if the user has disabled
   the "Nonspherical gravity sources" option in the Launchpad dialog.

If the user has enabled orbit stabilisation in the Launchpad,
   this function may sometimes return false during high time
   compression, even if the nonspherical option has been selected. In
   such situations Orbiter can exclude nonspherical perturbations to
   avoid numerical instabilities.

@function is_nonsphericalgravityenabled
@treturn bool _true_ indicates that gravity perturbations due to nonspherical
planet shapes are taken into account.
@see get_weightvector
*/
int Interpreter::v_is_nonsphericalgravityenabled (lua_State *L)
{
	static const char *funcname = "is_nonsphericalgravityenabled";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushboolean(L, v->NonsphericalGravityEnabled());
	return 1;
}

/***
Toggle a navigation mode on/off.

@function toggle_navmode
@tparam int mode navigation mode identifier (see @{types.NAVMODE|Navmode identifiers})
@treturn bool _true_ if the specified navigation mode could be
   changed, _false_ if it remains unchanged.
@see types.NAVMODE, get_navmode, set_navmode
*/
int Interpreter::v_toggle_navmode (lua_State *L)
{
	static const char *funcname = "toggle_navmode";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	int mode = luamtd_tointeger_safe(L, 2, funcname);
	lua_pushboolean(L, v->ToggleNavmode(mode));
	return 1;
}

/***
Get hover hold altitude.

Returns the altitude that the hover hold altitude program tries to maintain.

If the function returns false, the values pointed to by alt and
   terrainalt are unchanged.

@function get_hoverholdaltitude
@treturn number target altitude [m]
@treturn bool indicates true altitude (==_true_) or altitude
   relative to mean planet radius (==_false_)
@treturn bool _true_ if hold altitude program is active
@see toggle_navmode, get_navmode
*/
int Interpreter::v_get_hoverholdaltitude (lua_State *L)
{
	static const char *funcname = "get_hoverholdaltitude";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double alt = 0;
	bool terrainalt = false;
	bool result = v->GetHoverHoldAltitude(alt, terrainalt);
	lua_pushnumber(L, alt);
	lua_pushboolean(L, terrainalt);
	lua_pushboolean(L, result);
	return 3;
}

/***
Set hover hold altitude.

Set the target altitude for the hover hold altitude program and activate the
program.

If the hold hover altiude program is already active, the target
   altitude is modified. Otherwise, the program is activated with the
   specified target altitude.

This method is more versatile than ActivateNavmode(NAVMODE.HOLDALT), which
   sets the target altitude to the current altitude at activation, and always
   refers to mean planet radius.

To deactivate the hover hold alt program, use
   DeactivateNavmode(NAVMODE.HOLDALT)

@function set_hoverholdaltitude
@tparam number alt target altiude [m]
@tparam bool terrainalt _true_: hold true altitude;
   _false_: hold altitude relative to mean planet radius
@see get_hoverholdaltitude
*/
int Interpreter::v_set_hoverholdaltitude (lua_State *L)
{
	static const char *funcname = "set_hoverholdaltitude";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double alt = luamtd_tonumber_safe(L, 2, funcname);
	bool terrainalt = luamtd_toboolean_safe(L, 3, funcname)!=0;
	v->SetHoverHoldAltitude(alt, terrainalt);
	return 0;
}


/***
Orbital parameters
@section vessel_mtd_orbit
*/

/***
Get gravity reference.

Returns a handle to the main contributor of the gravity field at the
vessel's current position.

@function get_gravityref
@treturn handle Handle to gravity reference object
@see vessel:get_elements, vessel:get_elementsex, vessel:set_elements
*/
int Interpreter::v_get_gravityref (lua_State *L)
{
	static const char *funcname = "get_gravityref";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushlightuserdata (L, v->GetGravityRef());
	return 1;
}

/***
Get osculating elements.

Returns the osculating elements at the current time with respect to the
dominant gravity sources.

The returned mean longitude parameter (L) refers to the the current epoch.

@function get_elements
@treturn table list of orbital elements (see @{types.ELEMENTS|Elements table})
@see vessel:get_gravityref, vessel:get_elementsex, vessel:set_elements
*/
int Interpreter::v_get_elements (lua_State *L)
{
	static const char *funcname = "get_elements";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	ELEMENTS el;
	v->GetElements (0, el);
	lua_createtable (L, 0, 6);
	lua_pushnumber (L, el.a);
	lua_setfield (L, -2, "a");
	lua_pushnumber (L, el.e);
	lua_setfield (L, -2, "e");
	lua_pushnumber (L, el.i);
	lua_setfield (L, -2, "i");
	lua_pushnumber (L, el.theta);
	lua_setfield (L, -2, "theta");
	lua_pushnumber (L, el.omegab);
	lua_setfield (L, -2, "omegab");
	lua_pushnumber (L, el.L);
	lua_setfield (L, -2, "L");
	return 1;
}

/***
Get osculating orbital elements and additional orbital parameters.

The returned mean longitude parameter (L) refers to the the current epoch.

@function get_elementsex
@tparam[opt] objhandle to query
@treturn table list of orbital elements (see @{types.ELEMENTS|Elements table})
@treturn table list of additional orbital parameters (see @{types.ORBITPARAMS|Orbital parameters})
@see vessel:get_gravityref, vessel:get_elements, vessel:set_elements
*/
int Interpreter::v_get_elementsex (lua_State *L)
{
	static const char *funcname = "get_elementsex";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	ELEMENTS el;
	ORBITPARAM prm;
	OBJHANDLE hObj = 0;

	if (lua_gettop(L) >= 2) {
		hObj = lua_toObject(L, 2);
	}

	v->GetElements (hObj, el, &prm);
	lua_createtable (L, 0, 6);
	lua_pushnumber (L, el.a);
	lua_setfield (L, -2, "a");
	lua_pushnumber (L, el.e);
	lua_setfield (L, -2, "e");
	lua_pushnumber (L, el.i);
	lua_setfield (L, -2, "i");
	lua_pushnumber (L, el.theta);
	lua_setfield (L, -2, "theta");
	lua_pushnumber (L, el.omegab);
	lua_setfield (L, -2, "omegab");
	lua_pushnumber (L, el.L);
	lua_setfield (L, -2, "L");
	lua_createtable (L, 0, 12);
	lua_pushnumber (L, prm.SMi);
	lua_setfield (L, -2, "SMi");
	lua_pushnumber (L, prm.PeD);
	lua_setfield (L, -2, "PeD");
	lua_pushnumber (L, prm.ApD);
	lua_setfield (L, -2, "ApD");
	lua_pushnumber (L, prm.MnA);
	lua_setfield (L, -2, "MnA");
	lua_pushnumber (L, prm.TrA);
	lua_setfield (L, -2, "TrA");
	lua_pushnumber (L, prm.MnL);
	lua_setfield (L, -2, "MnL");
	lua_pushnumber (L, prm.TrL);
	lua_setfield (L, -2, "TrL");
	lua_pushnumber (L, prm.EcA);
	lua_setfield (L, -2, "EcA");
	lua_pushnumber (L, prm.Lec);
	lua_setfield (L, -2, "Lec");
	lua_pushnumber (L, prm.T);
	lua_setfield (L, -2, "T");
	lua_pushnumber (L, prm.PeT);
	lua_setfield (L, -2, "PeT");
	lua_pushnumber (L, prm.ApT);
	lua_setfield (L, -2, "ApT");
	return 2;
}

/***
Set vessel state.

Sets the vessel state (position and velocity) by means of a set of
osculating orbital elements.

The _el_ table containing the orbital elements must contain the six named
fields as defined in @{types.ELEMENTS|Elements}.

The optional _prm_ table can be provided to specify additional parameters
defining the elements. If it is omitted, default values are assumed.

_prm_ can contain the following fields:

- href (handle): orbit reference object. Default: current dominant gravity
source (see @{get_gravityref}).
- mjd\_ref (number): reference date (MJD format) to which the el.L (mean
longitude) value refers. mjd\_ref=0 is interpreted as the current time.
Default: 0.
- frame (string): reference frame. Choices are 'ecl' (ecliptic frame) or
'equ' (equatorial frame of the reference object). Default: 'ecl'.

@function set_elements
@tparam table el set of osculating elements (see @{types.ELEMENTS|Elements})
@tparam[opt] table prm additional parameters
@usage v:set_elements({a=4000e3, e=0, i=0, theta=0, omegab=0, L=0},
               {href=oapi.get_objhandle('moon'), mjd_ref=0, frame='equ'})
@see vessel:get_gravityref, vessel:get_elements, vessel:get_elementsex
*/
int Interpreter::v_set_elements (lua_State *L)
{
	static const char *funcname = "set_elements";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);

	ELEMENTS el;
	OBJHANDLE hRef = v->GetGravityRef();
	double mjd_ref = 0;
	int frame = FRAME_ECL;

	AssertMtdPrmType(L, 2, PRMTP_TABLE, funcname);
	el.a      = luamtd_field_tonumber_safe(L, 2, "a", funcname);
	el.e      = luamtd_field_tonumber_safe(L, 2, "e", funcname);
	el.i      = luamtd_field_tonumber_safe(L, 2, "i", funcname);
	el.theta  = luamtd_field_tonumber_safe(L, 2, "theta", funcname);
	el.omegab = luamtd_field_tonumber_safe(L, 2, "omegab", funcname);
	el.L      = luamtd_field_tonumber_safe(L, 2, "L", funcname);

	if (lua_gettop (L) >= 3) {
		AssertMtdPrmType(L, 3, PRMTP_TABLE, funcname);
		lua_getfield (L, 3, "href");
		if (lua_islightuserdata (L, -1)) hRef = (OBJHANDLE)lua_touserdata (L, -1);
		lua_pop (L, 1);
		lua_getfield (L, 3, "mjd_ref");
		if (lua_isnumber (L, -1)) mjd_ref = (double)lua_tonumber (L, -1);
		lua_pop (L, 1);
		lua_getfield (L, 3, "frame");
		if (lua_isstring (L, -1)) {
			const char *framestr = lua_tostring (L, -1);
			if (!_stricmp (framestr, "equ")) frame = FRAME_EQU;
		}
		lua_pop (L, 1);
	}

	v->SetElements (hRef, el, 0, mjd_ref, frame);
	return 0;
}

/***
Get prograde direction.

Returns the direction of the orbital velocity vector in vessel coordinates.

The returned direction is a normalised vector in the direction of the
orbital velocity (relative to the orbit reference object) rotated into the
vessel's frame of reference.

@function get_progradedir
@return (<i><b>@{types.vector|vector}</b></i>) prograde direction
@see vessel:get_gravityref
*/
int Interpreter::v_get_progradedir (lua_State *L)
{
	static const char *funcname = "get_progradedir";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	OBJHANDLE hRef = v->GetGravityRef();
	VECTOR3 vel;
	MATRIX3 rot;
	v->GetRelativeVel (hRef, vel);
	v->GetRotationMatrix (rot);
	vel = tmul (rot, vel);  // rotate into vessel frame
	normalise (vel);
	lua_pushvector (L, vel);
	return 1;
}

/***
Get semi-minor axis.

Returns the magnitude of the semi-minor axis of the current osculating orbit.

The semi-minor axis is the smallest semi-diameter of the
orbit ellipse.

@function get_smi
@treturn number semi-minor axis [m]
@treturn handle Handle of reference object, relative to which the orbit is
   calculated. _nil_ indicates failure (no orbit information available)
@see types.ELEMENTS, types.ORBITPARAMS, get_elements
*/
int Interpreter::v_get_smi (lua_State *L)
{
	static const char *funcname = "get_smi";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double smi;
	OBJHANDLE refbody = v->GetSMi(smi);
	if (refbody) {
		lua_pushnumber(L, smi);
		lua_pushlightuserdata(L, refbody);
	} else {
		lua_pushnil(L);
		lua_pushnil(L);
	}
	return 2;
}

/***
Get argument of periapsis.

Returns argument of periapsis of the current osculating orbit.

The argument of periapsis is the angle between periapsis and the
ascending node.

@function get_argper
@treturn number argument of periapsis for current orbit [rad]
@treturn handle Handle of reference body, relative to which the orbit is
   calculated. nil indicates failure (no orbit information available)
@see types.ELEMENTS, types.ORBITPARAMS, get_pedist, get_apdist, get_elements
*/
int Interpreter::v_get_argper (lua_State *L)
{
	static const char *funcname = "get_argper";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double arg;
	OBJHANDLE refbody = v->GetArgPer(arg);
	if (refbody) {
		lua_pushnumber(L, arg);
		lua_pushlightuserdata(L, refbody);
	} else {
		lua_pushnil(L);
		lua_pushnil(L);
	}
	return 2;
}

/***
Get periapsis distance.

Returns the periapsis distance of the current osculating orbit.

The periapsis distance is the smallest radius of the orbit (see
   \ref orbit).

@function get_pedist
@treturn number periapsis distance [m]
@treturn handle Handle of reference body, relative to which the orbit is
   calculated. NULL indicates failure (no orbit information available)
@see types.ELEMENTS, types.ORBITPARAMS, get_apdist, get_argper, get_elements
*/
int Interpreter::v_get_pedist (lua_State *L)
{
	static const char *funcname = "get_pedist";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double pedist;
	OBJHANDLE refbody = v->GetPeDist(pedist);
	if (refbody) {
		lua_pushnumber(L, pedist);
		lua_pushlightuserdata(L, refbody);
	} else {
		lua_pushnil(L);
		lua_pushnil(L);
	}
	return 2;
}

/***
Get apoapsis distance.

Returns the apoapsis distance of the current osculating orbit.

The apoapsis distance is the largest radius of the orbit (see
   \ref orbit).

@function get_apdist
@treturn number apoapsis distance [m]
@treturn handle Handle of reference body, relative to which the orbit is
   calculated. NULL indicates failure (no orbit information available)
@see types.ELEMENTS, types.ORBITPARAMS, get_pedist, get_argper, get_elements
*/
int Interpreter::v_get_apdist(lua_State* L)
{
	static const char* funcname = "get_apdist";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	double apdist;
	OBJHANDLE refbody = v->GetApDist(apdist);
	if (refbody) {
		lua_pushnumber(L, apdist);
		lua_pushlightuserdata(L, refbody);
	}
	else {
		lua_pushnil(L);
		lua_pushnil(L);
	}
	return 2;
}

/***
Get equatorial position.

Returns vessel's current equatorial position with respect to the
closest planet or moon.

The position is given as a table with the following fields :

- lng: number (longitude coordinate [rad])
- lat: number (latitude coordinate [rad])
- rad: number (distance from planet centre [m])

@function get_equpos
@treturn table position. nil indicates failure (no orbit information available)
@treturn handle reference body to which the parameters refer.
 */
int Interpreter::v_get_equpos(lua_State* L)
{
	static const char* funcname = "get_equpos";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	double longitude, latitude, radius;
	OBJHANDLE h = v->GetEquPos(longitude, latitude, radius);
	if(h) {
		lua_createtable(L, 0, 3);
		lua_pushnumber(L, longitude);
		lua_setfield(L, -2, "lng");
		lua_pushnumber(L, latitude);
		lua_setfield(L, -2, "lat");
		lua_pushnumber(L, radius);
		lua_setfield(L, -2, "rad");
		lua_pushlightuserdata(L, h);
		return 2;
	} else {
		lua_pushnil(L);
		return 1;
	}
}


/***
Surface-relative parameters
@section vessel_mtd_surf
*/

/***
Get surface reference.

Returns a handle to the surface reference object (planet or moon).

A vessel's _surface reference object_ is the planet or moon whose surface is
closest to the current vessel position.

@function get_surfaceref
@treturn handle Handle to surface reference object.
@see vessel:get_altitude, vessel:get_pitch, vessel:get_bank, vessel:get_yaw
*/
int Interpreter::v_get_surfaceref (lua_State *L)
{
	static const char *funcname = "get_surfaceref";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	OBJHANDLE hRef = v->GetSurfaceRef();
	if(hRef)
		lua_pushlightuserdata (L, hRef);
	else
		lua_pushnil(L);
	return 1;
}

/***
Get altitude.

Returns the current vessel altitude above the surface reference object.

The mode parameter can be set to ALTMODE.MEANRAD (altitude relative to
mean planet radius) or ALTMODE.GROUND (altitude relative to local ground
elevation).

If the parameter is omitted, ALTMODE.MEANRAD is assumed.

For ALTMODE.MEANRAD, the return value may be negative to indicate a
position below the planet mean radius.

The surface reference object is the celestial body whose surface is
closest to the current vessel position.

@function get_altitude
@tparam[opt] int mode altitude mode (see @{types.ALTMODE|altitude mode table})
@treturn number altitude [m]
@see vessel:get_surfaceref
*/
int Interpreter::v_get_altitude (lua_State *L)
{
	static const char *funcname = "get_altitude";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	AltitudeMode mode = ALTMODE_MEANRAD;
	if (lua_gettop(L) >= 2) {
		mode = (AltitudeMode)(int)luamtd_tointeger_safe(L, 2, funcname);
		if (mode != ALTMODE_GROUND)
			mode = ALTMODE_MEANRAD;
	}
	lua_pushnumber (L, v->GetAltitude(mode));
	GetInterpreter(L)->term_echo(L);
	return 1;
}

/***
Get pitch.

Returns the vessel's current pitch angle with respect to the local horizon.

The pitch angle is defined as 90 degrees minus the angle between the
vessel's positive z-axis and the normal to the local horizon.

@function get_pitch
@treturn number pitch angle [rad]
@see vessel:get_surfaceref, vessel:get_bank, vessel:get_yaw
*/
int Interpreter::v_get_pitch (lua_State *L)
{
	static const char *funcname = "get_pitch";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetPitch());
	GetInterpreter(L)->term_echo(L);
	return 1;
}

/***
Get bank.

Returns the vessel's current bank angle with respect to the local horizon.

The bank angle is defined as the angle between the vessel's positive y
axis (up direction) and the projection of the normal of the local horizon
into the x-y plane.

@function get_bank
@treturn number bank angle [rad]
@see vessel:get_surfaceref, vessel:get_pitch, vessel:get_yaw
*/
int Interpreter::v_get_bank (lua_State *L)
{
	static const char *funcname = "get_bank";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetBank());
	GetInterpreter(L)->term_echo(L);
	return 1;
}

/***
Get yaw.

Returns the vessel's current yaw angle with respect to the local horizon.

The yaw angle is defined as the angle between the the projection of the
vessel's positive z axis (forward direction) into the horizon plane, and
the local horizon "north" direction.

@function get_yaw
@treturn number yaw angle [rad]
@see vessel:get_surfaceref, vessel:get_pitch, vessel:get_bank
*/
int Interpreter::v_get_yaw (lua_State *L)
{
	static const char *funcname = "get_yaw";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetYaw());
	GetInterpreter(L)->term_echo(L);
	return 1;
}

/***
Get surface elevation.

Returns the elevation of the surface at the vessel's current longitude /
latitude above the reference radius.

@function get_surfaceelevation
@treturn number surface elevation [m]
*/
int Interpreter::v_get_surfaceelevation (lua_State *L)
{
	static const char *funcname = "get_surfaceelevation";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber(L, v->GetSurfaceElevation());
	return 1;
}

/***
Get surface normal.

Returns the normal (in local horizon frame) of the surface below the vessel's
current position.

@function get_surfacenormal
@return (<i><b>@{types.vector|vector}</b></i>) surface normal in local horizon frame
*/
int Interpreter::v_get_surfacenormal (lua_State *L)
{
	static const char *funcname = "get_surfacenormal";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushvector(L, v->GetSurfaceNormal());
	return 1;
}


/***
Atmospheric parameters
@section vessel_mtd_atm
*/

/***
Get atmospheric reference.

Returns a handle to the reference body for atmospheric calculations.

@function get_atmref
@treturn handle Handle for the celestial body whose atmosphere the vessel is
currently moving through, or nil if the vessel is not inside an atmosphere.
*/
int Interpreter::v_get_atmref (lua_State *L)
{
	static const char *funcname = "get_atmref";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	OBJHANDLE hA = v->GetAtmRef();
	if (hA) lua_pushlightuserdata (L, hA);
	else lua_pushnil (L);
	return 1;
}

/***
Get atmospheric temperature.

Returns ambient atmospheric temperature at current vessel position.

This function returns 0 if the vessel is outside all planetary atmospheric hulls,
as defined by the planets' AtmAltLimit parameters.

@function get_atmtemperature
@treturn number Ambient temperature [K] at current vessel position.
@see vessel:get_atmdensity, vessel:get_atmpressure, vessel:get_atmref
*/
int Interpreter::v_get_atmtemperature (lua_State *L)
{
	static const char *funcname = "get_atmtemperature";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double temp = v->GetAtmTemperature();
	lua_pushnumber (L,temp);
	return 1;
}

/***
Get atmospheric density.

Returns atmospheric density at current vessel position.

This function returns 0 if the vessel is outside all planetary atmospheric hulls,
as defined by the planets' AtmAltLimit parameters.

@function get_atmdensity
@treturn number Atmospheric density [kg/m&sup3;] at current vessel position.
@see vessel:get_atmtemperature, vessel:get_atmpressure, vessel:get_atmref
*/
int Interpreter::v_get_atmdensity (lua_State *L)
{
	static const char *funcname = "get_atmdensity";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double rho = v->GetAtmDensity();
	lua_pushnumber (L,rho);
	return 1;
}

/***
Get atmospheric pressure.

Returns static atmospheric pressure at current vessel position.

This function returns 0 if the vessel is outside all planetary atmospheric hulls,
as defined by the planets' AtmAltLimit parameters.

@function get_atmpressure
@treturn number Static atmospheric pressure [Pa] at current vessel position.
@see vessel:get_dynpressure, vessel:get_atmtemperature, vessel:get_atmdensity, vessel:get_atmref
*/
int Interpreter::v_get_atmpressure (lua_State *L)
{
	static const char *funcname = "get_atmpressure";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double p = v->GetAtmPressure();
	lua_pushnumber (L,p);
	return 1;
}

/***
Aerodynamic state
@section vessel_mtd_aerodynamics
*/

/***
Get dynamic pressure.

Returns the current dynamic pressure for the vessel.

The dynamic pressure is defined as q = 1/2 &rho; V&sup2; with density &rho; and
airflow vector V.

@function get_dynpressure
@treturn number current vessel dynamic pressure [Pa]
@see vessel:get_atmpressure, vessel:get_atmref
*/
int Interpreter::v_get_dynpressure (lua_State *L)
{
	static const char *funcname = "get_dynpressure";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetDynPressure());
	return 1;
}

/***
Get current Mach number.

The speed of sound depends on several parameters, e.g. atmospheric composition
and temperature. The Mach number can therefore vary even if the airspeed is
constant.

@function get_machnumber
@treturn number Mach number - the ratio of current freestream airflow velocity
over speed of sound.
@see vessel:get_airspeed, vessel:get_atmref
*/
int Interpreter::v_get_machnumber (lua_State *L)
{
	static const char *funcname = "get_machnumber";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetMachNumber());
	return 1;
}

/***
Get ground speed.

Returns magnitude of the vessel's current ground speed vector.

The ground speed is defined as the vessel's velocity relative to a point at the
vessel location fixed in the reference planet's frame of reference.

@function get_groundspeed
@treturn number magnitude of ground speed vector [m/s]
@see vessel:get_groundspeedvector, vessel:get_airspeed
*/
int Interpreter::v_get_groundspeed (lua_State *L)
{
	static const char *funcname = "get_groundspeed";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetGroundspeed ());
//	GetInterpreter(L)->term_echo(L);
	return 1;
}

/***
Get ground speed vector.

Returns the ground speed vector in the requested frame of reference.

Valid entries for frame are:

- REFFRAME.GLOBAL: ground speed in global frame
- REFFRAME.LOCAL: ground speed in local vessel frame
- REFFRAME.REFLOCAL: ground speed in local planet frame
- REFFRAME.HORIZON: ground speed in local horizon frame

@function get_groundspeedvector
@tparam int frame frame of reference flag (see @{types.REFFRAME|REFFRAME} identifiers)
@return (<i><b>@{types.vector|vector}</b></i>) ground speed vector [<b>m/s</b>]
@see vessel:get_groundspeed, vessel:get_airspeedvector
*/
int Interpreter::v_get_groundspeedvector (lua_State *L)
{
	static const char *funcname = "get_groundspeedvector";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	REFFRAME frame = (REFFRAME)lua_tointeger_safe(L, 2, funcname);
	VECTOR3 sp;
	v->GetGroundspeedVector (frame, sp);
	lua_pushvector (L, sp);
	return 1;
}

/***
Get airspeed.

Returns magnitude of the vessel's current true airspeed vector.

This function also works in the absence of an atmosphere. At low altitudes, the
returned value is a ground-speed equivalent. At high altitudes the value diverges
from ground speed, since an atmospheric drag effect is assumed.

@function get_airspeed
@treturn number magnitude of true airspeed vector [m/s]
@see vessel:get_airspeedvector, vessel:get_groundspeed
*/
int Interpreter::v_get_airspeed (lua_State *L)
{
	static const char *funcname = "get_airspeed";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetAirspeed ());
	GetInterpreter(L)->term_echo(L);
	return 1;
}

/***
Get airspeed vector.

Returns the true airspeed vector in the requested frame of reference.

Valid entries for frame are:

- REFFRAME.GLOBAL: airspeed in global frame
- REFFRAME.LOCAL: airspeed in local vessel frame
- REFFRAME.REFLOCAL: airspeed in local planet frame
- REFFRAME.HORIZON: airspeed in local horizon frame

@function get_airspeedvector
@tparam int frame frame of reference flag (see @{types.REFFRAME|REFFRAME} constants)
@return (<i><b>@{types.vector|vector}</b></i>) true airspeed vector [<b>m/s</b>]
@see vessel:get_airspeed, vessel:get_groundspeedvector
*/
int Interpreter::v_get_airspeedvector (lua_State *L)
{
	static const char *funcname = "get_airspeedvector";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	REFFRAME frame = (REFFRAME)luamtd_tointeger_safe(L, 2, funcname);
	VECTOR3 sp;
	v->GetAirspeedVector (frame, sp);
	lua_pushvector (L, sp);
	return 1;
}

// intentionally not documented
int Interpreter::v_get_shipairspeedvector (lua_State *L)
{
	static const char *funcname = "get_shipairspeedvector";
	warn_obsolete(L, funcname);
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 sp;
	v->GetAirspeedVector (FRAME_LOCAL, sp);
	lua_pushvector (L, sp);
	return 1;
}

// intentionally not documented
int Interpreter::v_get_horizonairspeedvector (lua_State *L)
{
	static const char *funcname = "get_horizonairspeedvector";
	warn_obsolete(L, funcname);
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 sp;
	v->GetAirspeedVector (FRAME_HORIZON, sp);
	lua_pushvector (L, sp);
	return 1;
}

/***
Get angle of attack.

Returns the current angle of attack (AOA).

The AOA value is defined as the angle between the vessel's positive z axis and
the flight path direction, projected into the yz-plane of the vessel's local
coordinate system.

@function get_aoa
@treturn number Angle of attack [rad] (range: -&pi; ... +&pi;)
@see vessel:get_slipangle
*/
int Interpreter::v_get_aoa (lua_State *L)
{
	static const char *funcname = "get_aoa";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetAOA());
	GetInterpreter(L)->term_echo(L);
	return 1;
}

/***
Get slip angle.

Returns the current slip angle.

The slip angle is defined as the lateral (yaw) angle between the velocity vector
and the vessel's longitudinal axis.

@function get_slipangle
@treturn number Slip angle [rad] (range -&pi; ... +&pi;)
@see vessel:get_aoa
*/
int Interpreter::v_get_slipangle (lua_State *L)
{
	static const char *funcname = "get_slipangle";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetSlipAngle());
	GetInterpreter(L)->term_echo(L);
	return 1;
}

/***
Airfoils and aerodynamic controls
@section vessel_mtd_airfoil
*/

void AirfoilFunc (VESSEL *v, double aoa, double M, double Re,
        void *context, double *cl, double *cm, double *cd)
{
	// The airfoil callback function for aerodynamic coefficients
	// The call is passed on to the designated script function

	AirfoilContext *ac = (AirfoilContext*)context;
	lua_State *L = ac->L;                             // interpreter instance
	lua_rawgeti(L, LUA_REGISTRYINDEX, ac->funcref);   // push the callback function

	// push callback arguments
	lua_pushlightuserdata (L, v->GetHandle());  // vessel handle
	lua_pushnumber (L, aoa);                    // angle of attack
	lua_pushnumber (L, M);                      // Mach number
	lua_pushnumber (L, Re);                     // Reynolds number

	// call the script callback function
	Interpreter::LuaCall(L, 4, 3);

	// retrieve results
	*cl = lua_tonumber (L,-3);
	*cm = lua_tonumber (L,-2);
	*cd = lua_tonumber (L,-1);
	lua_pop(L,3);
}

/***
Creates a new airfoil and defines its aerodynamic properties.

A vessel can define multiple airfoils (for wings, main body, tail stabilisators,
etc.). In general, it should define at least one vertical and one horizontal
component.

Airfoil definitions for wings and horizontal stabilisers set align to
LIFT.VERTICAL. Vertical stabilisers (vertical tail fin, etc.) set align to
LIFT.HORIZONTAL.

The centre of pressure is the point at which the lift and drag forces generated
by the airfoil are applied to the vessel. Together with the moment coefficient
it defines the aerodynamic stability of the vessel. Usually the CoP will be aft
of the CG, and the moment coefficient will have a negative slope around the trim
angle of attack.

The coeff_func is a callback function which must be supplied by the script. It
calculates the lift, moment and drag coefficients for the airfoil. It has the
following interface:

	function <func_name>(hVessel,aoa,M,Re)
	...
	   return cl,cm,cd
	end

The function must return three values in the following order: the lift
coefficient (cl), moment coefficient (cm) and drag coefficient (cd), as a
function of angle of attack aoa [rad], Mach number M and Reynolds number Re. Note
that aoa can range over the full circle (-pi to pi). For vertical lift components,
aoa is the pitch angle of attack (a), while for horizontal components it is the
yaw angle of attack (b).

If the wing area S is set to 0, then Orbiter uses the projected vessel cross
sections to define a reference area. Let (vx, vy, vz) be the unit vector of
freestream air flow in vessel coordinates. Then the reference area is calculated
as S = vz * Cz + vy * Cy for a LIFT.VERTICAL airfoil, and as S = vz * Cz + vx * Cx for a
LIFT.HORIZONTAL airfoil, where Cx, Cy, Cz are the vessel cross-sections in x, y
and z direction, respectively.

The wing aspect ratio is defined as defined as A = b&sup2;/S with wing span b.

If no airfoils are defined, Orbiter will fall back to its legacy drag
calculation, using the cw coefficients defined in @{vessel:set_cw}. The legacy model does
not support lift forces.

@function create_airfoil
@tparam int orientation orientation of the lift vector (see @{types.LIFT|Airfoil orientation})
@param ref (<i><b>@{types.vector|vector}</b></i>) centre of pressure in vessel coordinates [<b>m</b>]
@tparam string|function coeff_func callback function (name or function reference)
@tparam number c airfoil chord length [m]
@tparam number S wing area [m&sup2;]
@tparam number A wing aspect ratio
@treturn handle airfoil handle
@see vessel:del_airfoil
*/
int Interpreter::v_create_airfoil (lua_State *L)
{
	static const char *funcname = "create_airfoil";
	AssertMtdMinPrmCount(L, 7, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	AIRFOIL_ORIENTATION ao = (AIRFOIL_ORIENTATION)(int)(luamtd_tointeger_safe(L, 2, funcname));
	VECTOR3 ref = luamtd_tovector_safe(L, 3, funcname);
	int funcref;
	if (lua_isstring(L, 4)) {
		const char* fname = luamtd_tostring_safe(L, 4, funcname);
		lua_getglobal(L, fname);
		funcref = luaL_ref(L, LUA_REGISTRYINDEX);
	} else if (lua_isfunction(L, 4)) {
		lua_pushvalue(L, 4);
		funcref = luaL_ref(L, LUA_REGISTRYINDEX);
	} else {
		return luaL_error(L, "Invalid callback parameter, string or function expected");
	}
	double c = luamtd_tonumber_safe(L, 5, funcname);
	double S = luamtd_tonumber_safe(L, 6, funcname);
	double A = luamtd_tonumber_safe(L, 7, funcname);
	AirfoilContext *ac = new AirfoilContext;
	ac->L = L;
	ac->funcref = funcref;

	AIRFOILHANDLE ha = v->CreateAirfoil3 (ao, ref, AirfoilFunc, ac, c, S, A);
	lua_pushlightuserdata (L, ha);
	return 1;
}

/***
Edit airfoil.

Resets the parameters of an existing airfoil definition.

flag contains the bit flags defining which parameters will
be modified. It can be any combination of the following:

- 0x01: modify force attack point
- 0x02: modify coefficient callback function
- 0x04: modify chord length
- 0x08: modify wing area
- 0x10: modify wing aspect ratio

@function edit_airfoil
@tparam handle hAirfoil airfoil handle
@tparam number flag bitflags to define which parameters to reset
@tparam vector ref new centre of pressure
@tparam string|function|nil cf new callback function for coefficient calculation
@tparam number c new chord length [m]
@tparam number S new wing area [m<sup>2</sup>]
@tparam number A new wing aspect ratio
@see vessel:create_airfoil
*/
int Interpreter::v_edit_airfoil (lua_State *L)
{
	static const char *funcname = "edit_airfoil";
	AssertMtdMinPrmCount(L, 7, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);

	// @todo: better check for nil ... (optionals) ... and build 'flag' based on that

	AIRFOILHANDLE hAirfoil = (AIRFOILHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	DWORD flag = luamtd_tointeger_safe(L, 3, funcname);
	VECTOR3 ref = luamtd_tovector_safe(L, 4, funcname);
	double c = luamtd_tonumber_safe(L, 6, funcname),
	       S = luamtd_tonumber_safe(L, 7, funcname),
	       A = luamtd_tonumber_safe(L, 8, funcname);

	if (flag & 0x02) { // only update the context if we want to change the callback
		AirfoilContext* ac;
		if (v->GetAirfoilParam(hAirfoil, NULL, NULL, (void**)&ac, NULL, NULL, NULL)) {
			int funcref;
			if (lua_isstring(L, 5)) {
				const char* fname = luamtd_tostring_safe(L, 5, funcname);
				lua_getglobal(L, fname);
				funcref = luaL_ref(L, LUA_REGISTRYINDEX);
			}
			else if (lua_isfunction(L, 5)) {
				lua_pushvalue(L, 5);
				funcref = luaL_ref(L, LUA_REGISTRYINDEX);
			}
			else {
				term_strout(L, "Invalid callback parameter in edit_airfoil, string or function expected", true);
				return 0;
			}

			ac->L = L;
			luaL_unref(L, LUA_REGISTRYINDEX, ac->funcref);
			ac->funcref = funcref;
		}
	}

	v->EditAirfoil(hAirfoil, flag, ref, (AirfoilCoeffFunc)AirfoilFunc, c, S, A);
	return 0;
}

/***
Delete airfoil.

Deletes a previously defined airfoil.

If all the vessel's airfoils are deleted without creating new ones, Orbiter reverts to
the obsolete legacy atmospheric flight model.

@function del_airfoil
@tparam handle hAirfoil airfoil handle
@treturn bool _false_ indicates failure (invalid handle)
@see vessel:create_airfoil
*/
int Interpreter::v_del_airfoil (lua_State *L)
{
	static const char *funcname = "del_airfoil";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	AIRFOILHANDLE ha = (AIRFOILHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	AirfoilContext *ac;
	if (v->GetAirfoilParam (ha, 0, 0, (void**)&ac, 0, 0, 0)) {
		if (ac) {
			luaL_unref(L, LUA_REGISTRYINDEX, ac->funcref);
			delete ac; // delete the context buffer before deleting the airfoil
		}
	}
	bool ok = v->DelAirfoil (ha);
	lua_pushboolean (L, ok?1:0);
	return 1;
}

/***
Create control surface.

Creates an aerodynamic control surface and returns a handle.

Control surfaces include elevators, rudders, ailerons, flaps, etc. They can be used to
control the vessel during atmospheric flight.

When selecting automatic axis control (axis=AIRCTRL_AXIS.AUTO), the following axes will
be used for given control surfaces:

- Elevator: XPOS
- Rudder: YPOS
- Aileron: XPOS if ref.x > 0, XNEG otherwise
- Flap XPOS

For ailerons, at least 2 control surfaces should be defined (e.g. on left and right
wing) with opposite rotation axes, to obtain the angular momentum for banking the vessel.

Elevators typically use the XPOS axis, assuming the that the centre of pressure is aft
of the centre of gravity. If pitch control is provided by a canard configuration ahead
of the CoG, XNEG should be used instead.

The centre of pressure defined by the ref parameter is the point at which the lift and
drag forces for the control surface are applied.

To improve performance, multiple control surfaces may sometimes be defined by a single
call to v:create_controlsurface. For example, the elevator controls on the left and
right wing may be combined by setting a centered attack point.

Control surfaces can be animated, by passing an animation reference to
v:create_controlsurface. The animation reference is obtained when creating the animation
with v:create_animation. The animation should support a state in the range from 0 to 1,
with neutral surface position at state 0.5.

@function create_controlsurface
@tparam int type control surface type (see @{types.AIRCTRL|AIRCTRL} table)
@tparam number area control surface area [m&sup2;]
@tparam number dcl shift in lift coefficient achieved by fully extended control
@param ref (<i><b>@{types.vector|vector}</b></i>) centre of pressure in vessel coordinates [<b>m</b>]
@tparam[opt] int axis rotation axis (see @{types.AIRCTRL_AXIS|AIRCTRL_AXIS} table; default: AIRCTRL_AXIS.AUTO)
@tparam[opt] number delay response delay setting [s] (default: 1)
@tparam[opt] int anim animation reference (default: not linked to animation)
*/
int Interpreter::v_create_controlsurface (lua_State *L)
{
	static const char *funcname = "create_controlsurface";
	AssertMtdMinPrmCount(L, 5, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	AIRCTRL_TYPE type = (AIRCTRL_TYPE)luamtd_tointeger_safe(L, 2, funcname);
	double area = luamtd_tonumber_safe(L, 3, funcname);
	double dCl = luamtd_tonumber_safe(L, 4, funcname);
	VECTOR3 ref = luamtd_tovector_safe(L, 5, funcname);
	int axis = AIRCTRL_AXIS_AUTO;
	double delay = 1.0;
	UINT anim = (UINT)-1;
	if (lua_gettop(L) >= 6) {
		axis = (int)luamtd_tointeger_safe(L, 6, funcname);
		if (lua_gettop(L) >= 7) {
			delay = luamtd_tonumber_safe(L, 7, funcname);
			if (lua_gettop(L) >= 8) {
				anim = (UINT)luamtd_tointeger_safe(L, 8, funcname);
			}
		}
	}
	CTRLSURFHANDLE hctrl = v->CreateControlSurface3 (type, area, dCl, ref, axis, delay, anim);
	lua_pushlightuserdata (L, hctrl);
	return 1;
}

/***
Delete control surface.

Deletes a previously defined aerodynamic control surface.

@function del_controlsurface
@tparam handle hCtrlSurf control surface handle
@treturn bool _false_ indicates failure (invalid handle)
@see vessel:create_controlsurface
*/
int Interpreter::v_del_controlsurface (lua_State *L)
{
	static const char *funcname = "del_controlsurface";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	CTRLSURFHANDLE hCtrl = (CTRLSURFHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	bool ret = v->DelControlSurface(hCtrl);
	lua_pushboolean(L, ret);
	return 1;
}

/***
Get control surfaces mode.

Returns aerodynamic control surfaces currently under manual control.

The input mode defines which types of control surfaces can be manually
controlled by the user.

The returned control mode contains bit flags as follows:

- bit 0: elevator enabled/disabled
- bit 1: rudder enabled/disabled
- bit 2: ailerons enabled/disabled

Therefore, mode=0 indicates control surfaces disabled, mode=7 indicates
fully enabled.

Some vessel types may support not all, or not any, types of control
surfaces.

@function get_adcmode
@treturn int mode bit flags
@see vessel:set_adcmode, vessel:get_adclevel, vessel:set_adclevel, vessel:create_controlsurface
*/
int Interpreter::v_get_adcmode (lua_State *L)
{
	static const char *funcname = "get_adcmode";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	int mode = v->GetADCtrlMode();
	lua_pushnumber (L, mode);
	return 1;
}

/***
Set control surfaces mode.

Configure manual input mode for aerodynamic control surfaces.

The _mode_ contains bit flags interpreted as follows:

- bit 0: enable/disable elevator
- bit 1: enable/disable rudder
- bit 2: enable/disable ailerons

Therefore, _mode_ = 0 disables all control surfaces, _mode_ = 7 enables
all control surfaces.

@function set_adcmode
@tparam int mode bit flags defining the address mode for aerodynamic control surfaces
@see vessel:get_adcmode, vessel:get_adclevel, vessel:set_adclevel, vessel:create_controlsurface
*/
int Interpreter::v_set_adcmode (lua_State *L)
{
	static const char *funcname = "set_adcmode";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	ASSERT_MTDNUMBER(L,2);
	int mode = (int)lua_tointeger (L,2);
	v->SetADCtrlMode (mode);
	return 0;
}

/***
Get control surface position.

Returns the current position of an aerodynamic control surface.

This method returns the actual, not the target position. Due to finite
response time, it may therefore not return the value set by a preceeding
call to @{set_adclevel}.

@function get_adclevel
@tparam int adctype control surface type (see @{types.AIRCTRL|Aerodynamic control surface types})
@treturn number current position of the surface [-1...+1]
@see vessel:create_controlsurface, vessel:set_adclevel, vessel:get_adcmode, vessel:set_adcmode
*/
int Interpreter::v_get_adclevel (lua_State *L)
{
	static const char *funcname = "get_adclevel";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	AIRCTRL_TYPE surfid = (AIRCTRL_TYPE)luamtd_tointeger_safe(L, 2, funcname);
	double lvl = v->GetControlSurfaceLevel (surfid);
	lua_pushnumber (L, lvl);
	return 1;
}

/***
Set control surface position.

Updates the position of an aerodynamic control surface.

Parameter _lvl_ defines a target state for the surface. Control surfaces
generally require a finite amount of time to move from the current to the
target state.

This method affects the permanent setting of the control surface, while
manual input via keyboard or joystick affects the transient setting. The
total target state of the control surface is the sum of both settings,
clamped to the range [-1...+1]

@function set_adclevel
@tparam int adctype control surface type (see @{types.AIRCTRL|Aerodynamic control surface types})
@tparam number lvl new level [-1..+1]
@see vessel:create_controlsurface, vessel:get_adclevel, vessel:get_adcmode, vessel:set_adcmode
*/
int Interpreter::v_set_adclevel (lua_State *L)
{
	static const char *funcname = "set_adclevel";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	AIRCTRL_TYPE surfid = (AIRCTRL_TYPE)luamtd_tointeger_safe(L, 2, funcname);
	double lvl = luamtd_tonumber_safe(L, 3, funcname);
	v->SetControlSurfaceLevel (surfid, lvl);
	return 0;
}


/***
Aerodynamics (legacy model)
@section vessel_mtd_aero_legacy
*/

/***
Get wind resistance coefficients.

Returns the vessel's wind resistance coefficients (legacy flight model only).

The cw coefficients are only used by the legacy flight model (if no airfoils are
defined). In the presence of airfoils, drag calculations are performed on the basis of
the airfoil parameters.

The function returns a table with 4 fields: 'x', 'y', 'z',
'zn'., where 'z' is the coefficient in +z (forward) direction, 'zn' is the coefficient
in -z (backward) direction, 'x' is the coefficient in lateral (left/right) direction,
and 'y' is the coefficient in vertical (up/down) direction. Drag coefficients in lateral
and vertical direction are assumed symmetric.

@function get_cw
@treturn table resistance coefficients in principal direction.
@see vessel:set_cw
*/
int Interpreter::v_get_cw (lua_State *L)
{
	static const char *funcname = "get_cw";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 cw;
	double cw_zn;
	v->GetCW(cw.z, cw_zn, cw.x, cw.y);
	lua_pushvector(L, cw);
	lua_pushnumber(L, cw_zn);
	lua_setfield(L, -2, "zn");
	return 1;
}

/***
Set wind resistance coefficients.

Set the vessel's wind resistance coefficients along its axis directions.

The cw coefficients are only used by the legacy flight model (if no airfoils are
defined). In the presence of airfoils, drag calculations are performed on the basis of
the airfoil parameters.

The parameter passed to this function must be a table with 4 fields: 'x', 'y', 'z',
'zn'., where 'z' is the coefficient in +z (forward) direction, 'zn' is the coefficient
in -z (backward) direction, 'x' is the coefficient in lateral (left/right) direction,
and 'y' is the coefficient in vertical (up/down) direction. Drag coefficients in lateral
and vertical direction are assumed symmetric.

The 'x', 'y' and 'z' fields are identical to a standard vector variable, and can thus
be assigned via vector operations, but the 'zn' field must be added manually.

@function set_cw
@tparam table cw resistance coefficients in principal directions
@see vessel:get_cw
*/
int Interpreter::v_set_cw (lua_State *L)
{
	static const char *funcname = "set_cw";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 cw = lua_tovector(L, 2);
	lua_getfield(L, 2, "zn");
	double zn = lua_tonumber(L,-1);
	v->SetCW (cw.z, zn, cw.x, cw.y);
	return 0;
}

/***
Get wing aspect ratio.

The aspect ratio returned by this function is only used by the legacy aerodynamic
flight model. If the vessel uses the new flight model (i.e. defines at least one
airfoil), then this value is ignored, and the airfoil parameters are used instead.

The aspect ratio is used in the calculation of induced drag.

@function get_wingaspect
@treturn number Wing aspect ratio (wingspan&sup2; / wing area)
@see vessel:set_wingaspect
*/
int Interpreter::v_get_wingaspect (lua_State *L)
{
	static const char *funcname = "get_wingaspect";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double aspect = v->GetWingAspect ();
	lua_pushnumber (L, aspect);
	return 1;
}

/***
Set wing aspect ratio.

This function defines the wing aspect ratio for the legacy flight model. If the vessel
uses the new flight model (i.e. defines at least one airfoil), then this value is
ignored, and the airfoil parameters are used instead.

The aspect ratio is used in the calculation of induced drag.

@function set_wingaspect
@tparam number aspect Wing aspect ratio (wingspan&sup2; / wing area)
@see vessel:get_wingaspect
*/
int Interpreter::v_set_wingaspect (lua_State *L)
{
	static const char *funcname = "set_wingaspect";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double aspect = luamtd_tonumber_safe(L, 2, funcname);
	v->SetWingAspect (aspect);
	return 0;
}

/***
Get wing effectiveness.

Returns the wing form factor used in aerodynamic calculations.

The form factor returned by this function is only used by the legacy aerodynamic flight
model. If the vessel uses the new flight model (i.e. defines at least one airfoil), then
this value is ignored, and the airfoil parameters are used instead.

The form factor, together with the aspect ratio, determines the amount of induced drag
for given lift. Higher values of the form factor result in lower drag.

Typical values are ~3.1 for elliptic wings, ~2.8 for tapered wings, and ~2.5 for
rectangular wings.

@function get_wingeffectiveness
@treturn number wing form factor
@see vessel:set_wingeffectiveness
*/
int Interpreter::v_get_wingeffectiveness (lua_State *L)
{
	static const char *funcname = "get_wingeffectiveness";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double eff = v->GetWingEffectiveness ();
	lua_pushnumber (L, eff);
	return 1;
}

/***
Set wing effectiveness.

Sets the wing form factor for aerodynamic lift and drag calculations.

This function defines the wing form factor for the legacy flight model. If the vessel
uses the new flight model (i.e. defines at least one airfoil), then this value is
ignored, and the airfoil parameters are used instead.

The form factor, together with the aspect ratio, determines the amount of induced drag
for given lift. Higher values of the form factor result in lower drag.

Typical values for eff are: ~3.1 for elliptic wings, ~2.8 for tapered wings, ~2.5 for
rectangular wings. If set_wingeffectiveness is not called, the default value is 2.8.

@function set_wingeffectiveness
@tparam number eff wing form factor
@see vessel:get_wingeffectiveness
*/
int Interpreter::v_set_wingeffectiveness (lua_State *L)
{
	static const char *funcname = "set_wingeffectiveness";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double eff = luamtd_tonumber_safe(L, 2, funcname);
	v->SetWingEffectiveness (eff);
	return 0;
}

/***
Get atmospheric rotation drag.

Returns the vessel's atmospheric rotation resistance coefficients.

The returned vector contains the components x = dx, y = dy, z = dz of the drag
coefficient d for rotation around the local vessel axes in atmosphere. The angular
deceleration due to atmospheric friction is defined as

a(x,y,z) = -&omega;(x,y,z) q Sy d(x,y,z)

with angular velocity &omega;, dynamic pressure q and reference surface Sy, defined by
the vessel's cross section projected along the vertical (y) axis.

@function get_rotdrag
@return (<i><b>@{types.vector|vector}</b></i>) drag coefficients for rotation around the 3 vessel axes
@see vessel:set_rotdrag
*/
int Interpreter::v_get_rotdrag (lua_State *L)
{
	static const char *funcname = "get_rotdrag";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 rd;
	v->GetRotDrag (rd);
	lua_pushvector (L,rd);
	return 1;
}

/***
Set atmospheric rotation drag.

Sets the vessel's atmospheric rotation resistance coefficients.

The input vector contains the components x = dx, y = dy, z = dz of the new drag
coefficient d for rotation around the local vessel axes in atmosphere. The angular
deceleration due to atmospheric friction is defined as

a(x,y,z) = -&omega;(x,y,z) q Sy d(x,y,z)

with angular velocity &omega;, dynamic pressure q and reference surface Sy, defined by
the vessel's cross section projected along the vertical (y) axis.

@function set_rotdrag
@param d (<i><b>@{types.vector|vector}</b></i>) drag coefficients for rotation around the 3 vessel axes
@see vessel:get_rotdrag
*/
int Interpreter::v_set_rotdrag (lua_State *L)
{
	static const char *funcname = "set_rotdrag";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 rd = luamtd_tovector_safe(L, 2, funcname);
	v->SetRotDrag (rd);
	return 0;
}

/***
Get scaling factor for the pitch moment.

The pitch moment is the angular moment around the vessel's lateral (x) axis occurring
in atmospheric flight. It works toward reducing the pitch angle (angle of attack).

The larger the scaling factor, the stronger the effect becomes ("stiff handling")

This value is only used with the old aerodynamic flight model, i.e. if no airfoils have
been defined.

@function get_pitchmomentscale
@treturn number pitch moment scale factor
@see vessel:set_pitchmomentscale, vessel:get_yawmomentscale, vessel:set_yawmomentscale
*/
int Interpreter::v_get_pitchmomentscale (lua_State *L)
{
	static const char *funcname = "get_pitchmomentscale";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double pms = v->GetPitchMomentScale ();
	lua_pushnumber (L,pms);
	return 1;
}

/***
Sets scaling factor for the pitch moment.

The pitch moment is the angular moment around the vessel's lateral (x) axis occurring in
atmospheric flight. It works toward reducing the pitch angle (angle of attack).

The larger the scaling factor, the stronger the effect becomes ("stiff handling")

This value is only used with the old aerodynamic flight model, i.e. if no airfoils have
been defined.

The default value is 0.

@function set_pitchmomentscale
@tparam number pms pitch moment scale factor
@see vessel:get_pitchmomentscale, vessel:get_yawmomentscale, vessel:set_yawmomentscale
*/
int Interpreter::v_set_pitchmomentscale (lua_State *L)
{
	static const char *funcname = "set_pitchmomentscale";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double pms = luamtd_tonumber_safe(L, 2, funcname);
	v->SetPitchMomentScale (pms);
	return 0;
}

/***
Get scaling factor for the yaw moment.

The yaw moment is the angular moment around the vessel's vertical (y) axis occurring in
atmospheric flight. It works toward reducing the slip angle between the vessel's
longidudinal axis and the airstream vector.

This value is only used with the old aerodynamic flight model, i.e. if no airfoils have
been defined.

@function get_yawmomentscale
@treturn number yaw moment scale factor
@see vessel:get_pitchmomentscale, vessel:set_pitchmomentscale, vessel:set_yawmomentscale
*/
int Interpreter::v_get_yawmomentscale (lua_State *L)
{
	static const char *funcname = "get_yawmomentscale";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double yms = v->GetYawMomentScale ();
	lua_pushnumber (L,yms);
	return 1;
}

/***
Set scaling factor for the yaw moment.

The yaw moment is the angular moment around the vessel's vertical (y) axis occurring in
atmospheric flight. It works toward reducing the slip angle between the vessel's
longidudinal axis and the airstream vector.

This value is only used with the old aerodynamic flight model, i.e. if not airfoils have
been defined.

The default value is 0.

@function set_yawmomentscale
@tparam number yms scale factor for slip angle moment
@see vessel:get_pitchmomentscale, vessel:set_pitchmomentscale, vessel:get_yawmomentscale
*/
int Interpreter::v_set_yawmomentscale (lua_State *L)
{
	static const char *funcname = "set_yawmomentscale";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double yms = luamtd_tonumber_safe(L, 2, funcname);
	v->SetYawMomentScale (yms);
	return 0;
}

/***
Get scaling factor for the pitch trim control.

The pitch trim scale is a factor defining the effect of the pitch trim controls.

It is only used with the old atmospheric flight model (if no airfoils have been defined).

@function get_trimscale
@treturn number pitch trim scale factor
@see vessel:set_trimscale
*/
int Interpreter::v_get_trimscale (lua_State *L)
{
	static const char *funcname = "get_trimscale";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double ts = v->GetTrimScale ();
	lua_pushnumber (L, ts);
	return 1;
}

/***
Set scaling factor for the pitch trim control.

This method is used only in combination with the old flight model, that is, if the
vessel doesn't define any airfoils. In the new flight model, this has been replaced by
@{vessel:create_controlsurface}(AIRCTRL.ELEVATORTRIM, ...).

If ts is set to zero (default) the vessel does not have a pitch trim control.

@function set_trimscale
@tparam number ts new pitch trim scale factor
@see vessel:get_trimscale
*/
int Interpreter::v_set_trimscale (lua_State *L)
{
	static const char *funcname = "set_trimscale";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double ts = luamtd_tonumber_safe(L, 2, funcname);
	v->SetTrimScale (ts);
	return 0;
}


/***
Forces
@section vessel_mtd_force
*/

/***
Get lift magnitude.

Returns magnitude of aerodynamic lift force vector.

Return value is the sum of lift components from all airfoils.

@function get_lift
@treturn number Magnitude of lift force vector [N].
@see vessel:get_liftvector, vessel:get_drag
*/
int Interpreter::v_get_lift (lua_State *L)
{
	static const char *funcname = "get_lift";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetLift());
	return 1;
}

/***
Get drag magnitude.

Returns magnitude of aerodynamic drag force vector.

Return value is the sum of drag components from all airfoils.

@function get_drag
@treturn number Magnitude of drag force vector [N].
@see vessel:get_dragvector, vessel:get_lift
*/
int Interpreter::v_get_drag (lua_State *L)
{
	static const char *funcname = "get_drag";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetDrag());
	return 1;
}

/***
Get weight vector.

Returns the gravitational force vector in local vessel coordinates.

When the vessel status is updated dynamically, G is composed of all
gravity sources currently used for the vessel propagation (excluding
sources with contributions below threshold).

During orbit stabilisation, only the contribution from the primary source
is returned.

@function get_weightvector
@return (<i><b>@{types.vector|vector}</b></i>) gravitational force [<b>N</b>]
@see vessel:get_thrustvector, vessel:get_liftvector
*/
int Interpreter::v_get_weightvector (lua_State *L)
{
	static const char *funcname = "get_weightvector";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 G;
	if (v->GetWeightVector (G)) {
		lua_pushvector (L, G);
	} else {
		lua_pushnil (L);
	}
	return 1;
}

/***
Get thrust vector.

Returns the thrust force vector in local vessel coordinates.

On return, T contains the vector sum of thrust components from all engines.

This function provides information about the linear thrust force, but not
about the angular moment (torque) induced.

@function get_thrustvector
@return (<i><b>@{types.vector|vector}</b></i>) thrust vector [<b>N</b>]
@see vessel:get_weightvector, vessel:get_liftvector
*/
int Interpreter::v_get_thrustvector (lua_State *L)
{
	static const char *funcname = "get_thrustvector";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 T;
	v->GetThrustVector (T);
	lua_pushvector (L, T);
	return 1;
}

/***
Get lift vector.

Returns the aerodynamic lift force vector in local vessel coordinates.

Return value is the sum of lift components from all airfoils.

The lift vector is perpendicular to the relative wind (and thus to the
drag vector) and has zero x-component.

@function get_liftvector
@return (<i><b>@{types.vector|vector}</b></i>) lift vector [<b>N</b>]
@see vessel:get_weightvector, vessel:get_thrustvector
*/
int Interpreter::v_get_liftvector (lua_State *L)
{
	static const char *funcname = "get_liftvector";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 Lf;
	v->GetLiftVector (Lf);
	lua_pushvector (L, Lf);
	return 1;
}

/***
Get drag vector.

Returns aerodynamic drag force vector in local vessel coordinates.

On return, D contains the sum of drag components from all
   airfoils.

The drag vector is parallel to the relative wind (direction of
   air flow).

@function get_dragvector
@return (<i><b>@{types.vector|vector}</b></i>) drag vector D [<b>N</b>]
@treturn bool _false_ indicates zero drag. In that case, the returned vector
   is (0,0,0).
@see get_drag, get_weightvector, get_thrustvector, get_liftvector,
   get_forcevector
*/
int Interpreter::v_get_dragvector (lua_State *L)
{
	static const char *funcname = "get_dragvector";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 D;
	bool result = v->GetDragVector(D);
	lua_pushvector(L, D);
	lua_pushboolean(L, result);
	return 2;
}

/***
Get total forces.

Returns total force vector acting on the vessel in local vessel coordinates.

On return, F contains the sum of all forces acting on the
   vessel.

This may not be equal to the sum of weight, thrust, lift and
   drag vectors, because it also includes surface contact forces,
   user-defined forces and any other forces.

@function get_forcevector
@return (<i><b>@{types.vector|vector}</b></i>) total force vector F [<b>N</b>]
@treturn bool Always _true_
@see get_weightvector, get_thrustvector, get_liftvector, get_dragvector,
   get_torquevector
*/
int Interpreter::v_get_forcevector (lua_State *L)
{
	static const char *funcname = "get_forcevector";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 F;
	bool result = v->GetForceVector(F);
	lua_pushvector(L, F);
	lua_pushboolean(L, result);
	return 2;
}

/***
Get total torque.

Returns the total torque vector acting on the vessel in local vessel
coordinates.

On return, M contains the total torque vector acting on the
   vessel in its centre of mass. The torque vector contains
   contributions from thrusters, aerodynamic forces and gravity
   gradient effects (if enabled).

@function get_torquevector
@return (<i><b>@{types.vector|vector}</b></i>) total torque vector M [<b>Nm</b>]
@treturn bool Always _true_
@see get_forcevector
*/
int Interpreter::v_get_torquevector (lua_State *L)
{
	static const char *funcname = "get_torquevector";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 M;
	bool result = v->GetTorqueVector(M);
	lua_pushvector(L, M);
	lua_pushboolean(L, result);
	return 2;
}

/***
Add a custom body force.

This function can be used to implement custom forces (braking
   chutes, tethers, etc.). It should not be used for standard forces
   such as engine thrust or aerodynamic forces which are handled
   internally (although in theory this function makes it possible to
   bypass Orbiter's built-in thrust and aerodynamics model completely
   and replace it by a user-defined model).

The force is applied only for the next time step. add_force will
   therefore usually be used inside the clbk_prestep callback
   function.

@function add_force
@tparam vector force vector F [<b>N</b>]
@tparam vector force attack point in local vessel coordinates [<b>m</b>]
@see get_forcevector
*/
int Interpreter::v_add_force (lua_State *L)
{
	static const char *funcname = "add_force";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 F = luamtd_tovector_safe(L, 2, funcname),
	        r = luamtd_tovector_safe(L, 3, funcname);
	v->AddForce(F, r);
	return 0;
}

/***
Create variable drag element.

Attaches a modifyable drag component to the vessel.

The variable drag can be changed by using the set method of the returned numberref.

The value of the drag should be set to values between 0 (no drag) and 1 (full drag).
Any changes to the numberref have immediate effect.

Depending on the attack point, the applied drag force may create
torque in addition to linear force.

@function create_variabledragelement
@tparam number factor drag magnitude scaling factor
@tparam vector ref drag force attack point [<b>m</b>]
@treturn numberref variable drag reference
@usage
-- vessel creation: create variable drag element
spdb_drag = vi:create_variabledragelement(5, _V(0, 7.5, -14)) -- speedbrake drag

-- in clbk_prestep: change the drag value according to the animation state
spdb_drag:set(spdb_state.proc)
*/
int Interpreter::v_create_variabledragelement(lua_State* L)
{
	static const char* funcname = "create_variabledragelement";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	double factor = lua_tonumber(L, 2);
	VECTOR3 ref = lua_tovector(L, 3);
	lua_pushnumberref(L);
	double* ptr = (double*)lua_touserdata(L, -1);
	*ptr = 0.0;
	v->CreateVariableDragElement((const double *)ptr, factor, ref);
	return 1;
}

/***
Clear variable drag elements.

Removes all drag elements defined with create_variabledragelement.

@function clear_variabledragelements
@see create_variabledragelement
*/
int Interpreter::v_clear_variabledragelements(lua_State* L)
{
	static const char* funcname = "clear_variabledragelements";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	// numberrefs allocated in v_create_variabledragelement should be handled by Lua's GC when we stop referencing them
	v->ClearVariableDragElements();
	return 0;
}

/***
Fuel management
@section vessel_mtd_fuel
*/

/***
Create propellant resource.

Orbiter doesn't distinguish between propellant and oxidant. A "propellant resource" is
assumed to be a combination of fuel and oxidant resources.

The interpretation of a propellant resource (liquid or solid propulsion system, ion
drive, etc.) is up to the vessel developer.

The rate of fuel consumption depends on the thrust level and Isp (fuel-specific impulse)
of the thrusters attached to the resource.

The fuel efficiency rating, together with a thruster's Isp rating, determines how much
fuel is consumed per second to obtain a given thrust: R = F (e Isp)-1 with fuel rate R
[kg/s], thrust F [N], efficiency e and fuel-specific impulse Isp [m/s].

If mass < 0 or not specified, then mass = maxmass is substituted.

@function create_propellantresource
@tparam number maxmass maximum propellant capacity [kg]
@tparam[opt=-1] number mass initial propellant mass [kg]
@tparam[opt=1] number efficiency fuel efficiency factor (> 0)
@treturn handle propellant resource handle
@see vessel:del_propellantresource, vessel:clear_propellantresources
*/
int Interpreter::v_create_propellantresource (lua_State *L)
{
	static const char *funcname = "create_propellantresource";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double maxmass = 1.0;
	double mass = -1.0;
	double efficiency = 1.0;
	if (lua_gettop(L) >= 2)
		maxmass = luamtd_tonumber_safe(L, 2, funcname);
	if (lua_gettop(L) >= 3)
		mass = luamtd_tonumber_safe(L, 3, funcname);
	if (lua_gettop(L) >= 4)
		efficiency = luamtd_tonumber_safe(L, 4, funcname);
	PROPELLANT_HANDLE hPrp = v->CreatePropellantResource (maxmass, mass, efficiency);
	lua_pushlightuserdata (L, hPrp);
	return 1;
}

/***
Remove propellant resource.

If any thrusters were attached to this fuel resource, they are disabled until connected
to a new fuel resource.

@function del_propellantresource
@tparam handle hProp propellant resource handle
@see vessel:create_propellantresource, vessel:clear_propellantresources
*/
int Interpreter::v_del_propellantresource(lua_State* L)
{
	static const char* funcname = "del_propellantresource";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	PROPELLANT_HANDLE hPrp = (PROPELLANT_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	v->DelPropellantResource(hPrp);
	return 0;
}

/***
Define a "default" propellant resource.

This is used for the various legacy fuel-related API functions, and
for the "Fuel" indicator in the generic panel-less HUD display.

If this function is not called, the first propellant resource
is used as default.

@function set_default_propellantresource
@tparam handle hProp propellant resource handle
@see vessel:create_propellantresource, vessel:clear_propellantresources
*/
int Interpreter::v_set_default_propellantresource(lua_State* L)
{
	static const char* funcname = "set_default_propellantresource";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	PROPELLANT_HANDLE hPrp = (PROPELLANT_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	v->SetDefaultPropellantResource(hPrp);
	return 0;
}

/***
Remove all propellant resources for the vessel.

After a call to this function, all the vessel's thrusters will be disabled until they
are linked to new resources.

@function clear_propellantresources
@see vessel:create_propellantresource, vessel:del_propellantresource
*/
int Interpreter::v_clear_propellantresources (lua_State *L)
{
	static const char *funcname = "clear_propellantresources";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	v->ClearPropellantResources();
	return 0;
}

/***
Number of propellant resources.

Return the current number of vessel propellant resources.

@function get_propellantcount
@treturn int Number of propellant resources currently defined for the vessel
@see vessel:create_propellantresource, vessel:del_propellantresource, vessel:clear_propellantresources
*/
int Interpreter::v_get_propellantcount (lua_State *L)
{
	static const char *funcname = "get_propellantcount";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DWORD n = v->GetPropellantCount();
	lua_pushnumber (L, n);
	return 1;
}

/***
Get handle of a propellant resource.

The index must be in the range between 0 and @{get_propellantcount}-1. If the index is
out of range, the returned handle is nil.

The index of a given propellant resource may change if any resources are deleted. The
handle remains valid until the corresponding resource is deleted.

@function get_propellanthandle
@tparam int idx propellant resource index (0 &le; idx &lt; @{get_propellantcount})
@treturn handle propellant resource handle
@see vessel:create_propellantresource, vessel:del_propellantresource, vessel:get_propellantcount
*/
int Interpreter::v_get_propellanthandle (lua_State *L)
{
	static const char *funcname = "get_propellanthandle";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	int idx = luamtd_tointeger_safe(L, 2, funcname);
	PROPELLANT_HANDLE hp = v->GetPropellantHandleByIndex (idx);
	ASSERT_SYNTAX(hp, "Invalid propellant index");
	lua_pushlightuserdata (L, hp);
	return 1;
}

/***
Get maximum capacity of a propellant resource.

@function get_propellantmaxmass
@tparam handle hProp propellant resource handle
@treturn number Max. propellant capacity [kg]
@see vessel:create_propellantresource, vessel:set_propellantmaxmass, vessel:get_propellanthandle, vessel:get_propellantmass
*/
int Interpreter::v_get_propellantmaxmass (lua_State *L)
{
	static const char *funcname = "get_propellantmaxmass";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	PROPELLANT_HANDLE hp = (PROPELLANT_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	lua_pushnumber (L, v->GetPropellantMaxMass (hp));
	return 1;
}

/***
Reset maximum capacity of a fuel resource.

The actual fuel mass contained in the tank is not affected by this function, unless the
new maximum propellant mass is less than the current fuel mass, in which case the fuel
mass is reduced to the maximum capacity.

@function set_propellantmaxmass
@tparam handle hProp propellant resource handle
@tparam number maxm max. fuel capacity [kg]
@see vessel:create_propellantresource, vessel:get_propellanthandle, vessel:get_propellantmaxmass, vessel:set_propellantmass
*/
int Interpreter::v_set_propellantmaxmass (lua_State *L)
{
	static const char *funcname = "set_propellantmaxmass";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	PROPELLANT_HANDLE hp = (PROPELLANT_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	double maxmass = (double)luamtd_tonumber_safe(L, 3, funcname);
	if (hp && maxmass >= 0) {
		v->SetPropellantMaxMass (hp, maxmass);
	}
	return 0;
}

/***
Get current mass of a propellant resource.

@function get_propellantmass
@tparam handle hProp propellant resource handle
@treturn number propellant mass [kg]
@see vessel:create_propellantresource, vessel:get_propellantmaxmass, vessel:set_propellantmass
*/
int Interpreter::v_get_propellantmass (lua_State *L)
{
	static const char *funcname = "get_propellantmass";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	PROPELLANT_HANDLE hProp = (PROPELLANT_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	lua_pushnumber (L, v->GetPropellantMass (hProp));
	return 1;
}

/***
Reset current mass of a propellant resource.

0 &le; mass &le; maxmass is required, where maxmass is the maximum capacity of the
propellant resource.

This method should be used to simulate refuelling, fuel leaks, cross-feeding between
tanks, etc. but not for normal fuel consumption by thrusters (which is handled
internally by the Orbiter core).

@function set_propellantmass
@tparam handle hProp propellant resource handle
@tparam number mass propellant mass (&ge; 0) [kg]
@see vessel:create_propellantresource, vessel:get_propellantmass, vessel:set_propellantmaxmass, vessel:get_propellantmaxmass
*/
int Interpreter::v_set_propellantmass (lua_State *L)
{
	static const char *funcname = "set_propellantmass";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	PROPELLANT_HANDLE hp = (PROPELLANT_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	double mass = luamtd_tonumber_safe(L, 3, funcname);
	ASSERT_SYNTAX(mass >= 0, "Argument 2: expected value >= 0");
	v->SetPropellantMass (hp, mass);
	return 0;
}

/***
Get current total propellant mass.

@function get_totalpropellantmass
@treturn number Sum of current masses of propellant resources defined for the vessel [kg]
@see vessel:create_propellantresource, vessel:get_propellantmass, vessel:set_propellantmass
*/
int Interpreter::v_get_totalpropellantmass (lua_State *L)
{
	static const char *funcname = "get_totalpropellantmass";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetTotalPropellantMass());
	return 1;
}

/***
Get efficiency factor of a propellant resource.

The fuel efficiency rating, together witha thruster's Isp rating, determines how much
fuel is consumed per second to obtain a given thrust value: R = F/(e Isp) with fuel rate
R [kg/s], thrust F [N], efficiency e and fuel-specific impulse Isp [m/s].

@function get_propellantefficiency
@tparam handle hProp propellant resource handle
@treturn number fuel efficiency factor
@see vessel:create_propellantresource, vessel:set_propellantefficiency
*/
int Interpreter::v_get_propellantefficiency (lua_State *L)
{
	static const char *funcname = "get_propellantefficiency";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	PROPELLANT_HANDLE hp = (PROPELLANT_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	lua_pushnumber (L, v->GetPropellantEfficiency(hp));
	return 1;
}

/***
Reset efficiency factor of a propellant resource.

The fuel efficiency rating, together witha thruster's Isp rating, determines how much
fuel is consumed per second to obtain a given thrust value: R = F/(e Isp) with fuel rate
R [kg/s], thrust F [N], efficiency e and fuel-specific impulse Isp [m/s].

@function set_propellantefficiency
@tparam handle hProp propellant resource handle
@tparam number eff fuel efficiency factor
@see vessel:create_propellantresource, vessel:get_propellantefficiency
*/
int Interpreter::v_set_propellantefficiency (lua_State *L)
{
	static const char *funcname = "set_propellantefficiency";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	PROPELLANT_HANDLE hp = (PROPELLANT_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	double eff = (double)luamtd_tonumber_safe(L, 3, funcname);
	v->SetPropellantEfficiency (hp,eff);
	return 0;
}

/***
Reset efficiency factor of a propellant resource.

The fuel efficiency rating, together witha thruster's Isp rating, determines how much
fuel is consumed per second to obtain a given thrust value: R = F/(e Isp) with fuel rate
R [kg/s], thrust F [N], efficiency e and fuel-specific impulse Isp [m/s].

@function get_propellantflowrate
@tparam handle hProp propellant resource handle
@treturn number current propellant mass flow rate [kg/s]
@see vessel:create_propellantresource, vessel:get_totalpropellantflowrate
*/
int Interpreter::v_get_propellantflowrate (lua_State *L)
{
	static const char *funcname = "get_propellantflowrate";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	PROPELLANT_HANDLE hp = (PROPELLANT_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	double rate = v->GetPropellantFlowrate (hp);
	lua_pushnumber(L,rate);
	return 1;
}

/***
Get total propellant flow rate.

Returns the current total mass flow rate, summed over all propellant resources.

@function get_totalpropellantflowrate
@treturn number total propellant mass flow rate [kg/s]
@see vessel:create_propellantresource, vessel:get_propellantflowrate
*/
int Interpreter::v_get_totalpropellantflowrate (lua_State *L)
{
	static const char *funcname = "get_totalpropellantflowrate";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double rate = v->GetTotalPropellantFlowrate ();
	lua_pushnumber(L,rate);
	return 1;
}

/***
Thruster management
@section vessel_mtd_thruster
*/

/***
Create a thruster.

Adds a logical thruster definition for the vessel.

_spec_ is a table whose fields describe the thruster specifications. The following
fields are recognised:

- pos (vector): thrust force attack point in vessel coordinates [m]
- dir (vector): thrust force direction in vessel coordinates (normalised)
- maxth0 (number): max. vacuum thrust rating [N]
- hprop (handle): [optional] handle of attached propellant resource
- isp0 (number): [optional] vacuum fuel-specific impulse (Isp) [m/s]
- ispr (number): [optional] Isp value at reference pressure pr [m/s]
- pr (number): [optional] reference pressure for ispr [Pa]

If no propellant resource handle is provided, the thruster is not connected to a
propellant resource and cannot be activated until connected.

The fuel-specific impulse defines how much thrust is produced by burning 1kg of fuel
per second. If _isp0_ is not specified or is 0, a default value is used (see set_isp).

To define the thrust and Isp ratings to be pressure-dependent, specify an _ispr_
value > 0, and set _pr_ to the corresponding atmospheric pressure.

If _ispr_ = 0 then no pressure-dependency is assumed.

If _ispr_ is specified, but not _pr_, then a default reference pressure of
_pr_=101.4e3 Pa is assumed (Earth surface pressure).

Example:

	ph = v:get_propellanthandle(0)
	th_pos = {x=0, y=0, z=-5}
	th_dir = {x=0, y=0, z=1}
	th = v:create_thruster({pos=th_pos, dir=th_dir, maxth0=1e5,
	                        hprop=ph, isp0=3000, ispr=2500,
							pr=101.4e3})

@function create_thruster
@tparam table spec thruster specification (see Notes)
@treturn handle thruster handle
@see vessel:del_thruster, vessel:clear_thrusters
*/
int Interpreter::v_create_thruster (lua_State *L)
{
	static const char *funcname = "create_thruster";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	AssertMtdPrmType(L, 2, PRMTP_TABLE, funcname);

	VECTOR3 pos = luamtd_field_tovector_safe(L, 2, "pos", funcname);
	VECTOR3 dir = luamtd_field_tovector_safe(L, 2, "dir", funcname);
	double maxth0 = lua_field_tonumber_safe(L, 2, "maxth0", funcname);

	PROPELLANT_HANDLE hp = NULL;
	lua_getfield(L,2,"hprop");
	if (lua_islightuserdata(L,-1)) hp = (PROPELLANT_HANDLE)lua_touserdata(L,-1);
	lua_pop(L,1);

	double isp0 = 0.0;
	lua_getfield(L,2,"isp0");
	if (lua_isnumber(L,-1)) isp0 = (double)lua_tonumber(L,-1);
	lua_pop(L,1);

	double ispr = 0.0;
	lua_getfield(L,2,"ispr");
	if (lua_isnumber(L,-1)) ispr = (double)lua_tonumber(L,-1);
	lua_pop(L,1);

	double pr = 101.4e3;
	lua_getfield(L,2,"pr");
	if (lua_isnumber(L,-1)) pr = (double)lua_tonumber(L,-1);
	lua_pop(L,1);

	THRUSTER_HANDLE th = v->CreateThruster (pos, dir, maxth0, hp, isp0, ispr, pr);
	lua_pushlightuserdata (L, th);
	return 1;
}

/***
Delete thruster definition.

Deleted thrusters will be automatically removed from all thruster groups they had been
assigned to.

All exhaust render definitions which refer to the deleted thruster are removed.

@function del_thruster
@tparam handle hThruster thruster handle
@treturn bool _true_ on success, _false_ if the supplied thruster handle was invalid.
@see vessel:create_thruster, vessel:clear_thrusters
*/
int Interpreter::v_del_thruster (lua_State *L)
{
	static const char *funcname = "del_thruster";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	bool ok = v->DelThruster (ht);
	lua_pushboolean (L, ok);
	return 1;
}

/***
Delete all thrusters and thruster group definitions.

This function removes all thruster definitions, as well as all the thruster group
definitions.

It also removes all previously defined exhaust render definitions.

@function clear_thrusters
@see vessel:create_thruster, vessel:del_thruster
*/
int Interpreter::v_clear_thrusters (lua_State *L)
{
	static const char *funcname = "clear_thrusters";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	v->ClearThrusterDefinitions();
	return 0;
}

/***
Get number of thrusters currently defined.

@function get_thrustercount
@treturn int Number of logical thruster definitions
@see vessel:create_thruster, vessel:del_thruster, vessel:clear_thrusters
*/
int Interpreter::v_get_thrustercount (lua_State *L)
{
	static const char *funcname = "get_thrustercount";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DWORD count = v->GetThrusterCount();
	lua_pushnumber(L, count);
	return 1;
}

/***
Get handle of a thruster specified by its index.

The index must be in the range between 0 and nthruster-1, where nthruster is the
thruster count returned by @{get_thrustercount}. If the index is out of range, the
returned handle is _nil_.

The index of a given thruster may change if vessel thrusters are deleted. The handle
remains valid until the corresponding thruster is deleted.

@function get_thrusterhandle
@tparam int idx thruster index (0 &le; idx &lt; @{get_thrustercount})
@treturn handle thruster handle
@see vessel:create_thruster, vessel:get_thrustercount
*/
int Interpreter::v_get_thrusterhandle (lua_State *L)
{
	static const char *funcname = "get_thrusterhandle";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	int idx = luamtd_tointeger_safe(L, 2, funcname);
	THRUSTER_HANDLE ht = v->GetThrusterHandleByIndex(idx);
	if (ht) lua_pushlightuserdata (L, ht);
	else    lua_pushnil (L);
	return 1;
}

/***
Get handle for the propellant resource feeding the thruster.

@function get_thrusterresource
@tparam handle hThruster thruster handle
@treturn handle propellant resource handle, or _nil_ if thruster is not connected
@see vessel:create_thruster, vessel:set_thrusterresource
*/
int Interpreter::v_get_thrusterresource (lua_State *L)
{
	static const char *funcname = "get_thrusterresource";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	PROPELLANT_HANDLE hp = v->GetThrusterResource(ht);
	if (hp) lua_pushlightuserdata (L, hp);
	else    lua_pushnil (L);
	return 1;
}

/***
Connect a thruster to a propellant resource.

A thruster can only be connected to one propellant resource at a time. Setting a new
resource disconnects from the previous resource.

To disconnect the thruster from its current tank, use _hProp_ = _nil_.

@function set_thrusterresource
@tparam handle hThruster thruster handle
@tparam handle hProp propellant resource handle
@see vessel:create_thruster, vessel:get_thrusterresource, vessel:create_propellantresource, vessel:get_propellanthandle
*/
int Interpreter::v_set_thrusterresource (lua_State *L)
{
	static const char *funcname = "set_thrusterresource";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	AssertMtdPrmType(L, 3, PRMTP_LIGHTUSERDATA | PRMTP_NIL, funcname);
	PROPELLANT_HANDLE hp = (lua_isnil(L,3) ? NULL:(PROPELLANT_HANDLE)lua_touserdata(L,3));
	v->SetThrusterResource (ht, hp);
	return 0;
}

/***
Get thrust force attack point of a thruster.

The returned point is the position at which the thrust force is applied, in the vessel
frame of reference.

@function get_thrusterpos
@tparam handle hThruster thruster handle
@return (<i><b>@{types.vector|vector}</b></i>) thrust attack point [<b>m</b>]
@see vessel:create_thruster, vessel:set_thrusterpos, vessel:get_thrusterdir
*/
int Interpreter::v_get_thrusterpos (lua_State *L)
{
	static const char *funcname = "get_thrusterpos";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	VECTOR3 pos;
	v->GetThrusterRef (ht, pos);
	lua_pushvector(L, pos);
	return 1;
}

/***
Reset thrust force attack point of a thruster.

_pos_ is specified in the vessel reference system.

This method should be used whenever a thruster has been physically moved in the vessel's
local frame of reference.

If the vessel's centre of gravity, i.e. the origin of its reference system, is moved
with @{shiftCG}, the thruster positions are updated automatically.

The attack point has no influence on the linear force exerted on the vessel by the
thruster, but it affects the induced torque.

@function set_thrusterpos
@tparam handle hThruster thruster handle
@param pos (<i><b>@{types.vector|vector}</b></i>) new force attack point [<b>m</b>]
@see vessel:create_thruster, vessel:get_thrusterpos, vessel:set_thrusterdir
*/
int Interpreter::v_set_thrusterpos (lua_State *L)
{
	static const char *funcname = "set_thrusterpos";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	VECTOR3 pos = luamtd_tovector_safe(L, 3, funcname);
	v->SetThrusterRef (ht, pos);
	return 0;
}

/***
Get force direction of a thruster.

@function get_thrusterdir
@tparam handle hThruster thruster handle
@return (<i><b>@{types.vector|vector}</b></i>) thrust direction (vessel frame of reference)
@see vessel:create_thruster, vessel:set_thrusterdir, vessel:get_thrusterpos
*/
int Interpreter::v_get_thrusterdir (lua_State *L)
{
	static const char *funcname = "get_thrusterdir";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	VECTOR3 dir;
	v->GetThrusterDir (ht, dir);
	lua_pushvector(L, dir);
	return 1;
}

/***
Reset force direction of a thruster.

This method can be used to realise a tilt of the rocket motor (e.g. for implementing a
thruster gimbal mechanism)

@function set_thrusterdir
@tparam handle hThruster thruster handle
@param dir (<i><b>@{types.vector|vector}</b></i>) thrust direction (vessel frame of reference)
@see vessel:create_thruster, vessel:get_thrusterdir, vessel:set_thrusterpos
*/
int Interpreter::v_set_thrusterdir (lua_State *L)
{
	static const char *funcname = "set_thrusterdir";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	VECTOR3 dir = luamtd_tovector_safe(L, 3, funcname);
	v->SetThrusterDir (ht, dir);
	return 0;
}

/***
Get maximum vacuum thust rating of a thruster.

To retrieve the actual current maximum thrust rating (which may be lower in the
presence of ambient atmospheric pressure), use @{get_thrustermax}.

@function get_thrustermax0
@tparam handle hThruster thruster handle
@treturn number Maximum vacuum thust rating [N]
@see vessel:create_thruster, vessel:set_thrustermax0, vessel:get_thrustermax
*/
int Interpreter::v_get_thrustermax0 (lua_State *L)
{
	static const char *funcname = "get_thrustermax0";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	double max0 = v->GetThrusterMax0 (ht);
	lua_pushnumber(L, max0);
	return 1;
}

/***
Reset maximum vacuum thrust rating of a thruster.

The max. thrust rating in the presence of atmospheric ambient pressure may be lower
than the vacuum thrust if a pressure-dependent Isp value has been defined.

@function set_thrustermax0
@tparam handle hThruster thruster handle
@tparam number max0 new maximum vacuum thrust rating [N]
@see vessel:create_thruster, vessel:get_thrustermax0, vessel:get_thrustermax
*/
int Interpreter::v_set_thrustermax0 (lua_State *L)
{
	static const char *funcname = "set_thrustermax0";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	double max0 = luamtd_tonumber_safe(L, 3, funcname);
	v->SetThrusterMax0 (ht, max0);
	return 0;
}

/***
Get pressure-dependent maximum thrust rating of a thruster.

This method returns the pressure-corrected maximum thrust rating of a thruster.

If the pressure parameter is provided, the returned value is the maximum thrust rating
at that pressure. Otherwise, the returned value refers to the ambient pressure at the
current vessel position.

@function get_thrustermax
@tparam handle hThruster thruster handle
@tparam[opt] number pr ambient pressure [Pa]
@treturn number Max. thrust rating [N]
@see vessel:create_thruster, vessel:get_thrustermax0, vessel:set_thrustermax0
*/
int Interpreter::v_get_thrustermax (lua_State *L)
{
	static const char *funcname = "get_thrustermax";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	double thmax;
	if (lua_gettop(L) >= 3) {
		double pr = luamtd_tonumber_safe(L, 3, funcname);
		thmax = v->GetThrusterMax (ht, pr);
	} else {
		thmax = v->GetThrusterMax (ht);
	}
	lua_pushnumber(L, thmax);
	return 1;
}

/***
Get vacuum fuel-specific impulse (Isp) rating for a thruster.

Equivalent to @{get_thrusterisp} (hThruster,0).

@function get_thrusterisp0
@tparam handle hThruster thruster handle
@treturn number Isp value in vacuum [m/s]
*/
int Interpreter::v_get_thrusterisp0 (lua_State *L)
{
	static const char *funcname = "get_thrusterisp0";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	lua_pushnumber (L, v->GetThrusterIsp0 (ht));
	return 1;
}

/***
Get pressure-dependent fuel-specific impulse (Isp) rating for a thruster.

If the thruster has been defined with a pressure-dependent Isp rating, the value
returned by this method will vary with pressure.

If the pressure parameter is provided, the returned value is the Isp rating at that
pressure. Otherwise, the returned value refers to the ambient pressure at the current
vessel position.

@function get_thrusterisp
@tparam handle hThruster thruster handle
@tparam[opt] number pr ambient pressure [Pa]
@treturn number Pressure-corrected Isp rating [m/s]
@see vessel:create_thruster, vessel:get_thrusterisp0, vessel:set_thrusterisp
*/
int Interpreter::v_get_thrusterisp (lua_State *L)
{
	static const char *funcname = "get_thrusterisp";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	double isp;
	if (lua_gettop(L) >= 3) {
		double pr = luamtd_tonumber_safe(L, 3, funcname);
		isp = v->GetThrusterIsp (ht, pr);
	} else {
		isp = v->GetThrusterIsp (ht);
	}
	lua_pushnumber (L, isp);
	return 1;
}

/***
Reset fuel-specific impulse (Isp) rating of a thruster.

This method can be used to define pressure-independent or pressure-dependent Isp
ratings.

For the pressure-independent version, only the vacuum Isp value (_isp0_) should be
specified.

For the pressure-dependent version, both the vacuum value and the value at a reference
pressure (_ispr_) should be provided. In addition, the reference pressure (_pr_) can be
specified. If _pr_ is not provided, a default value of 101.4kPa is assumed, corresponding
to Earth surface pressure.

The Isp rating at arbitrary pressure p is then computed as

	isp = isp0 * (1 - p * e)
	e = (isp0 - ispr) / (pr * isp0)

@function set_thrusterisp
@tparam handle hThruster thruster handle
@tparam number isp0 vacuum Isp rating [m/s]
@tparam[opt] number ispr Isp rating at reference pressure [m/s]
@tparam[opt] number pr reference pressure [Pa]
@see vessel:create_thruster, vessel:get_thrusterisp, vessel:get_thrusterisp0
*/
int Interpreter::v_set_thrusterisp (lua_State *L)
{
	static const char *funcname = "set_thrusterisp";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	double isp0 = (double)luamtd_tonumber_safe(L, 3, funcname);
	if (lua_gettop(L) >= 4) {
		double ispr = (double)luamtd_tonumber_safe(L, 4, funcname);
		double pr = 101.4e3;
		if (lua_gettop(L) >= 5) {
			pr = (double)luamtd_tonumber_safe(L, 5, funcname);
		}
		v->SetThrusterIsp (ht, isp0, ispr, pr);
	} else {
		v->SetThrusterIsp (ht, isp0);
	}
	return 0;
}

/***
Get current thrust level setting of a thruster.

To obtain the actual force [N] currently generated by the thruster, multiply the thrust
level with the max. thrust rating returned by @{get_thrustermax}.

@function get_thrusterlevel
@tparam handle hThruster thruster handle
@treturn number thrust level [0..1]
@see vessel:create_thruster, vessel:get_thrustermax, vessel:set_thrusterlevel
*/
int Interpreter::v_get_thrusterlevel (lua_State *L)
{
	static const char *funcname = "get_thrusterlevel";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	lua_pushnumber (L, v->GetThrusterLevel (ht));
	return 1;
}

/***
Set thrust level for a thruster.

At level 1, the thruster generates maximum force, as defined by its maxth parameter.

Certain thrusters are controlled directly by Orbiter via primary input controls (e.g.
joystick throttle control for main thrusters), which may override this function.

@function set_thrusterlevel
@tparam handle hThruster thruster handle
@tparam number lvl thrust level [0..1]
@see vessel:create_thruster, vessel:get_thrusterlevel, vessel:inc_thrusterlevel
*/
int Interpreter::v_set_thrusterlevel (lua_State *L)
{
	static const char *funcname = "set_thrusterlevel";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	double level = (double)luamtd_tonumber_safe(L, 3, funcname);
	ASSERT_SYNTAX(level>=0 && level<=1, "Argument 2: value out of range (expected 0..1)");
	v->SetThrusterLevel (ht, level);
	return 0;
}

/***
Apply a change to the thrust level of a thruster.

The applied thrust level change is limited to give a resulting thrust level in the
range [0..1].

@function inc_thrusterlevel
@tparam handle hThruster thruster handle
@tparam number dlvl thrust level increment [-1..+1]
@see vessel:create_thruster, vessel:set_thrusterlevel, vessel:get_thrusterlevel, vessel:inc_thrusterlevel_singlestep
*/
int Interpreter::v_inc_thrusterlevel (lua_State *L)
{
	static const char *funcname = "inc_thrusterlevel";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	double dlevel = (double)luamtd_tonumber_safe(L, 3, funcname);
	v->IncThrusterLevel (ht, dlevel);
	return 0;
}

/***
Set the thrust level of a thruster for the current time step only.

At level 1 the thruster generates maximum force, as defined by
   its maxth parameter.

This method overrides the thruster's permanent thrust level
   for the current time step only, so it should normally only be used
   in the body of the clbk_prestep() method.

@function set_thrusterlevel_singlestep
@tparam handle th thruster handle
@tparam number level thrust level (0...1)
@see set_thrusterlevel
*/
int Interpreter::v_set_thrusterlevel_singlestep (lua_State *L)
{
	static const char *funcname = "set_thrusterlevel_singlestep";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE th = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	double level = luamtd_tonumber_safe(L, 3, funcname);
	v->SetThrusterLevel_SingleStep(th, level);
	return 0;
}

/***
Apply a change of the thruster level for the current frame only.

Unlike @{inc_thrusterlevel}, which applies a permanent change in a thruster level, this
method applies a level change only for the current frame. It is useful for continously
controlling a thruster output on a per-frame basis, e.g. for attitude control.

When the function is no longer called, the thruster level returns to its permanent
setting.

@function inc_thrusterlevel_singlestep
@tparam handle hThruster thruster handle
@tparam number dlvl thrust level increment [-1..+1]
@see vessel:create_thruster, vessel:inc_thrusterlevel, vessel:set_thrusterlevel
*/
int Interpreter::v_inc_thrusterlevel_singlestep (lua_State *L)
{
	static const char *funcname = "inc_thrusterlevel_singlestep";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	double dlevel = (double)luamtd_tonumber_safe(L, 3, funcname);
	v->IncThrusterLevel_SingleStep (ht, dlevel);
	return 0;
}


/***
Thruster group management
@section vessel_mtd_thgroup
*/

/***
Combine thrusters into a logical group.

If the _type_ parameter is omitted, THGROUP.USER is assumed.


@function create_thrustergroup
@tparam table hThrusterArray array of thruster handles
@tparam int type thruster group identifier (see @{types.THGROUP|Thruster group identifiers})
@treturn handle hThgrp thruster group handle
@usage v = vessel.get_focusinterface()
h1 = v:create_thruster({pos={x=-1,y=0,z=0}, dir={x=0,y=0,z=1}, maxth0=1e5})
h2 = v:create_thruster({pos={x=1,y=0,z=0}, dir={x=0,y=0,z=1}, maxth0=1e5})
hmain = v:create_thrustergroup({h1,h2},THGROUP.MAIN)

@see vessel:del_thrustergroup
*/
int Interpreter::v_create_thrustergroup (lua_State *L)
{
	static const char *funcname = "create_thrustergroup";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	AssertMtdPrmType(L, 2, PRMTP_TABLE, funcname);
	THGROUP_TYPE thgt;
	if (lua_gettop(L) >= 3) {
		thgt = (THGROUP_TYPE)luamtd_tointeger_safe(L, 3, funcname);
	} else {
		thgt = THGROUP_USER;
	}

	// traverse the thruster array
	static int nht = 1;
	static THRUSTER_HANDLE *ht = new THRUSTER_HANDLE[nht];

	lua_pushnil(L);
	int i = 0;
	while (lua_next(L,2)) {
		if (i >= nht) {
			THRUSTER_HANDLE *tmp = new THRUSTER_HANDLE[i+1];
			memcpy(tmp, ht, nht*sizeof(THRUSTER_HANDLE));
			delete []ht;
			ht = tmp;
			nht = i+1;
		}
		ht[i++] = (THRUSTER_HANDLE)lua_touserdata(L,-1);
		lua_pop(L,1);
	}
	lua_pop(L,1);
	THGROUP_HANDLE htg = v->CreateThrusterGroup (ht, i, thgt);
	lua_pushlightuserdata(L,htg);
	return 1;
}

/***
Delete a thruster group.

This deletes only the logical group, not the individual thrusters associated with the
group.

Example: A thruster group created with

	thgrp = vessel:create_thrustergroup({h1,h2}, THGROUP.MAIN)

can be deleted either with

	vessel:del_thrustergroup(thgrp)

or with

	vessel:del_thrustergroup(THGROUP.MAIN)

However, thruster groups created with THGROUP.USER can only be deleted by handle.

@function del_thrustergroup
@tparam ?handle|int idThgroup thruster group identifier; either a thruster group handle, or @{types.THGROUP|THGROUP} entry.
@see vessel:create_thrustergroup
*/
int Interpreter::v_del_thrustergroup (lua_State *L)
{
	static const char *funcname = "del_thrustergroup";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	AssertMtdPrmType(L, 2, PRMTP_NUMBER | PRMTP_LIGHTUSERDATA, funcname);
	if (lua_isnumber(L,2)) {
		THGROUP_TYPE thgt = (THGROUP_TYPE)lua_tointeger(L,2);
		v->DelThrusterGroup (thgt);
	} else {
		THGROUP_HANDLE htg = (THGROUP_HANDLE)lua_touserdata(L,2);
		v->DelThrusterGroup (htg);
	}
	return 0;
}

/***
Get handle of a default thruster group.

If the requested thruster group is not defined by the vessel, this method returns nil.

@function get_thrustergrouphandle
@tparam int type thruster group identifier (see @{types.THGROUP|Thruster group identifiers})
@treturn handle thruster group handle
@see vessel:create_thrustergroup, vessel:get_thrustergrouphandlebyindex
*/
int Interpreter::v_get_thrustergrouphandle (lua_State *L)
{
	static const char *funcname = "get_thrustergrouphandle";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	int i = luamtd_tointeger_safe(L, 2, funcname);
	ASSERT_SYNTAX (i >= THGROUP_MAIN && i <= THGROUP_ATT_BACK, "Argument 1: index out of range");
	THGROUP_TYPE thgt = (THGROUP_TYPE)i;
	THGROUP_HANDLE htg = v->GetThrusterGroupHandle(thgt);
	if (htg) lua_pushlightuserdata (L, htg);
	else     lua_pushnil(L);
	return 1;
}

/***
Get handle of a user-defined (nonstandard) thruster group.

Use this method only to retrieve handles for nonstandard thruster groups. For standard
groups, use @{get_thrustergrouphandle} instead.

@function get_thrustergrouphandlebyindex
@tparam int idx index of user-defined thruster group (&ge; 0)
@treturn handle thruster group handle
@see vessel:create_thrustergroup, vessel:get_thrustergrouphandle
*/
int Interpreter::v_get_thrustergrouphandlebyindex (lua_State *L)
{
	static const char *funcname = "get_thrustergrouphandlebyindex";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	int idx = luamtd_tointeger_safe(L, 2, funcname);
	THGROUP_HANDLE htg = v->GetUserThrusterGroupHandleByIndex(idx);
	if (htg) lua_pushlightuserdata (L, htg);
	else     lua_pushnil(L);
	return 1;
}

/***
Get number of thrusters assigned to a logical thruster group.

This method can be called by providing either a thruster group handle, or an identifier for a
default thruster group.

Thrusters can be assigned to more than one group (and some thrusters may not be assigned to any
group) so the sum of GetGroupThrusterCount values over all groups can be different to the total
number of thrusters.

@function get_groupthrustercount
@tparam ?handle|int idThgroup thruster group identifier; either a thruster group handle, or @{types.THGROUP|THGROUP} entry.
@treturn int Number of thrusters assigned to the specified thruster group.
@see vessel:create_thrustergroup
*/
int Interpreter::v_get_groupthrustercount (lua_State *L)
{
	static const char *funcname = "get_groupthrustercount";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	int count;
	AssertMtdPrmType(L, 2, PRMTP_NUMBER | PRMTP_LIGHTUSERDATA, funcname);
	if (lua_islightuserdata (L,2)) { // identify by handle
		THGROUP_HANDLE htg = (THGROUP_HANDLE)lua_touserdata (L,2);
		count = v->GetGroupThrusterCount (htg);
	} else { // identify by type
		THGROUP_TYPE thgt = (THGROUP_TYPE)lua_tointeger (L,2);
		count = v->GetGroupThrusterCount (thgt);
	}
	lua_pushnumber (L, count);
	return 1;
}

/***
Get handle for a thruster that belongs to a thruster group.

If the specified group is not defined, or if the index is out of range, this function returns
_nil_.

@function get_groupthruster
@tparam ?handle|int idThgroup thruster group identifier; either a thruster group handle, or @{types.THGROUP|THGROUP} entry.
@tparam idx int thruster index (&ge; 0)
@treturn handle thruster handle
@see vessel:create_thrustergroup
*/
int Interpreter::v_get_groupthruster (lua_State *L)
{
	static const char *funcname = "get_groupthruster";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	int idx = (int)luamtd_tointeger_safe(L, 3, funcname);
	THRUSTER_HANDLE ht;
	AssertMtdPrmType(L, 2, PRMTP_NUMBER | PRMTP_LIGHTUSERDATA, funcname);
	if (lua_islightuserdata (L,2)) { // identify by handle
		THGROUP_HANDLE htg = (THGROUP_HANDLE)lua_touserdata (L,2);
		ht = v->GetGroupThruster (htg, idx);
	} else { // identify by type
		THGROUP_TYPE thgt = (THGROUP_TYPE)lua_tointeger (L,2);
		ASSERT_SYNTAX(thgt <= THGROUP_ATT_BACK, "Argument 1: out of range");
		ht = v->GetGroupThruster (thgt, idx);
	}
	if (ht) lua_pushlightuserdata (L, ht);
	else    lua_pushnil (L);
	return 1;
}

/***
Get mean thrust level for a thruster group.

For ease of use, the default thruster group idenfiers have been enumerated in the
@{types.THGROUP|THGROUP} table. For example, THGROUP.MAIN identifies the main thruster group.

In general, this method is only useful for groups where all thrusters have the same maximum
thrust rating and the same thrust direction.

@function get_thrustergrouplevel
@tparam ?handle|int idThgroup thruster group identifier; either a thruster group handle, or @{types.THGROUP|THGROUP} entry.
@treturn number group level [0..1]
@see vessel:create_thrustergroup, vessel:set_thrustergrouplevel
*/
int Interpreter::v_get_thrustergrouplevel (lua_State *L)
{
	static const char *funcname = "get_thrustergrouplevel";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double level;
	AssertMtdPrmType(L, 2, PRMTP_NUMBER | PRMTP_LIGHTUSERDATA, funcname);
	if (lua_islightuserdata(L, 2)) { // identified by handle
		THGROUP_HANDLE htg = (THGROUP_HANDLE)lua_touserdata (L,2);
		level = v->GetThrusterGroupLevel (htg);
	} else {                        // identified by type
		THGROUP_TYPE thgt = (THGROUP_TYPE)lua_tointeger (L,2);
		level = v->GetThrusterGroupLevel (thgt);
	}
	lua_pushnumber (L, level);
	return 1;
}

/***
Get thrust level for all thrusters in a group.

@function set_thrustergrouplevel
@tparam ?handle|int idThgroup thruster group identifier; either a thruster group handle, or @{types.THGROUP|THGROUP} entry
@tparam number lvl new group thrust level [0..1]
@see vessel:create_thrustergroup, vessel:get_thrustergrouplevel, vessel:inc_thrustergrouplevel
*/
int Interpreter::v_set_thrustergrouplevel (lua_State *L)
{
	static const char *funcname = "set_thrustergrouplevel";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double level = luamtd_tonumber_safe(L, 3, funcname);
	AssertMtdPrmType(L, 2, PRMTP_NUMBER | PRMTP_LIGHTUSERDATA, funcname);
	if (lua_islightuserdata(L, 2)) { // identified by handle
		THGROUP_HANDLE htg = (THGROUP_HANDLE)lua_touserdata(L,2);
		v->SetThrusterGroupLevel(htg, level);
	} else { // identified by type
		THGROUP_TYPE thgt = (THGROUP_TYPE)(int)lua_tonumber (L,2);
		v->SetThrusterGroupLevel (thgt, level);
	}
	return 0;
}

/***
Increment the thrust level for all thrusters in a group.

The resulting thrust levels are automatically truncated to the range
[0..1]. Use negative dlvl to decrement the thrust level.

@function inc_thrustergrouplevel
@tparam ?handle|int idThgroup thruster group identifier; either a thruster group handle, or @{types.THGROUP|THGROUP} entry
@tparam number dlvl thrust increment [-1..+1]
@see vessel:create_thrustergroup, vessel:set_thrustergrouplevel, vessel:get_thrustergrouplevel, vessel:inc_thrustergrouplevel_singlestep
*/
int Interpreter::v_inc_thrustergrouplevel (lua_State *L)
{
	static const char *funcname = "inc_thrustergrouplevel";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double dlevel = luamtd_tonumber_safe(L, 3, funcname);
	AssertMtdPrmType(L, 2, PRMTP_NUMBER | PRMTP_LIGHTUSERDATA, funcname);
	if (lua_islightuserdata (L, 2)) { // identify by handle
		THGROUP_HANDLE htg = (THGROUP_HANDLE)lua_touserdata (L,2);
		v->IncThrusterGroupLevel (htg, dlevel);
	} else { // identify by type
		THGROUP_TYPE thgt = (THGROUP_TYPE)lua_tointeger (L,2);
		v->IncThrusterGroupLevel (thgt, dlevel);
	}
	return 0;
}

/***
Increment the thrust level for the group thrusters for the current frame only.

Unlike @{inc_thrustergrouplevel}, this method modifies the thrust level of
the group for the current frame only. For continuous thrust control, it
must be called at each frame. It is useful in situations where the thrust
level needs to be modulated continuously (e.g. attitude control, etc.).

@function inc_thrustergrouplevel_singlestep
@tparam ?handle|int idThgroup thruster group identifier; either a thruster group handle, or @{types.THGROUP|THGROUP} entry
@tparam number dlvl thrust increment [-1..+1]
@see vessel:create_thrustergroup, vessel:inc_thrustergrouplevel
*/
int Interpreter::v_inc_thrustergrouplevel_singlestep (lua_State *L)
{
	static const char *funcname = "inc_thrustergrouplevel_singlestep";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double dlevel = luamtd_tonumber_safe(L, 3, funcname);
	AssertMtdPrmType(L, 2, PRMTP_NUMBER | PRMTP_LIGHTUSERDATA, funcname);
	if (lua_islightuserdata (L, 2)) { // identify by handle
		THGROUP_HANDLE htg = (THGROUP_HANDLE)lua_touserdata (L,2);
		v->IncThrusterGroupLevel_SingleStep (htg, dlevel);
	} else { // identify by type
		THGROUP_TYPE thgt = (THGROUP_TYPE)lua_tointeger (L,2);
		v->IncThrusterGroupLevel_SingleStep (thgt, dlevel);
	}
	return 0;
}

/***
Get manual control level.

Returns the thrust level of an attitude thruster group set via keyboard or mouse input.

If mode is not MANCTRL.ANYMODE, only thruster groups which
are of the specified mode (linear or rotational) will return
nonzero values.

@function get_manualcontrollevel
@tparam number id thruster group identifier (@{types.THGROUP|THGROUP})
@tparam[opt=0] number mode attitude control mode
@tparam[opt=2] number device  input device
@treturn number manual thrust level [0..1]
*/
int Interpreter::v_get_manualcontrollevel(lua_State* L) {
	static const char* funcname = "get_manualcontrollevel";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	THGROUP_TYPE thgt = (THGROUP_TYPE)lua_tointeger(L, 2);
	int mode = 0;
	if(lua_gettop(L)>=3)
		mode = luaL_checkinteger(L, 3);
	int device = 2;
	if(lua_gettop(L)>=4)
		device = luaL_checkinteger(L, 4);

	double lvl = v->GetManualControlLevel(thgt, mode, device);
	lua_pushnumber(L, lvl);
	return 1;
}


/***
Reaction control system
@section vessel_mtd_rcs
*/

/***
Get RCS mode.

Returns the current RCS (reaction control system) thruster mode.

The reaction control system consists of a set of small thrusters arranged
around the vessel. They can be fired in pre-defined configurations to
provide either a change in angular velocity (in RCSMODE.ROT mode) or in
linear velocity (in RCSMODE.LIN mode).

RCSMODE.OFF indicates that the RCS is disabled or not available.

Currently Orbiter doesn't allow simultaneous linear and rotational RCS
control via keyboard or joystick. The user has to switch between the two.
However, simultaneous operation is possible via the "RControl" plugin
module.

Not all vessel classes may define a complete RCS.

@function get_rcsmode
@treturn int Current RCS mode (see @{types.RCSMODE|RCS mode identifiers})
@see vessel:set_rcsmode, vessel:create_thrustergroup
*/
int Interpreter::v_get_rcsmode (lua_State *L)
{
	static const char *funcname = "get_rcsmode";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	int mode = v->GetAttitudeMode();
	lua_pushnumber (L, mode);
	return 1;
}

/***
Set RCS mode.

Sets the vessel's RCS (reaction control system) thruster mode.

The reaction control system consists of a set of small thrusters arranged
around the vessel. They can be fired in pre-defined configurations to
provide either a change in angular velocity (in RCS_ROT mode) or in linear
velocity (in RCS_LIN mode).

Set mode=RCSMODE.OFF to disable the RCS.

@function set_rcsmode
@tparam int mode new RCS mode (see @{types.RCSMODE|RCS mode identifiers})
@see vessel:get_rcsmode, vessel:create_thrustergroup
*/
int Interpreter::v_set_rcsmode (lua_State *L)
{
	static const char *funcname = "set_rcsmode";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	int mode = luamtd_tointeger_safe(L, 2, funcname);
	v->SetAttitudeMode (mode);
	return 0;
}

/***
Switch between linear and rotational RCS mode.

If the RCS is disabled, this method does nothing and returns 0.

During playback, this method does nothing and returns the current RCS mode.

@function toggle_RCSmode
@treturn int New RCS mode index
@see set_rcsmode, get_rcsmode
*/
int Interpreter::v_toggle_RCSmode (lua_State *L)
{
	static const char *funcname = "toggle_RCSmode";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushinteger(L, v->ToggleAttitudeMode());
	return 1;
}

/***
Get activation state of an automated orbital navigation mode.

@function get_navmode
@tparam int mode navigation mode identifier (see @{types.NAVMODE|Navmode identifiers})
@treturn bool _true_ if specified navmode is active, _false_ otherwise.
@see vessel:set_navmode
*/
int Interpreter::v_get_navmode (lua_State *L)
{
	static const char *funcname = "get_navmode";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	int mode = luamtd_tointeger_safe(L, 2, funcname);
	bool active = v->GetNavmodeState (mode);
	lua_pushboolean (L, active?1:0);
	return 1;
}

/***
Activate or deactivate one of the automated orbital navigation modes.

Navmodes are high-level navigation modes which involve e.g. the
simultaneous and timed engagement of multiple attitude thrusters to get
the vessel into a defined state. Some navmodes terminate automatically
once the target state is reached (e.g. killrot), others remain active
until explicitly terminated (hlevel). Navmodes may also terminate if a
second conflicting navmode is activated.

@function set_navmode
@tparam int mode navigation mode identifier (see @{types.NAVMODE|Navmode identifiers})
@tparam[opt=true] bool activate _true_ to activate navmode, _false_ to deactivate
@see vessel:get_navmode
*/
int Interpreter::v_set_navmode (lua_State *L)
{
	static const char *funcname = "set_navmode";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	int mode = (int)luamtd_tointeger_safe(L, 2, funcname);
	int active = true;
	if (lua_gettop(L) > 2)
		active = lua_toboolean_safe(L, 3, funcname);
	if (active)
		v->ActivateNavmode (mode);
	else
		v->DeactivateNavmode (mode);
	return 0;
}


/***
Docking ports
@section vessel_mtd_docking
*/

/***
Create a new docking port.

The _dir_ and _rot_ vectors should be normalised to length 1.

The _rot_ vector should be perpendicular to the _dir_ vector.

When two vessels connect at their docking ports, they are aligned so that their dock
positions (_pos_) coincide in the global frame, and the relative orientation of the
vessels is defined such that their respective approach direction vectors (_dir_) are
anti-parallel, and their longitudinal alignment vectors (_rot_) are parallel.

@function create_dock
@param pos (<i><b>@{types.vector|vector}</b></i>) dock reference position in vessel coordinates [<b>m</b>]
@param dir (<i><b>@{types.vector|vector}</b></i>) approach direction in vessel coordinates
@param rot (<i><b>@{types.vector|vector}</b></i>) longitudinal rotation alignment vector
@treturn handle docking port handle
@see vessel:del_dock, vessel:get_dockcount, vessel:get_dockhandle, vessel:get_dockstatus, vessel:undock
*/
int Interpreter::v_create_dock (lua_State *L)
{
	static const char *funcname = "create_dock";
	AssertMtdMinPrmCount(L, 4, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 pos = luamtd_tovector_safe(L, 2, funcname);
	VECTOR3 dir = luamtd_tovector_safe(L, 3, funcname);
	VECTOR3 rot = luamtd_tovector_safe(L, 4, funcname);
	lua_pushlightuserdata (L, v->CreateDock (pos, dir, rot));
	return 1;
}

/***
Delete a previously defined docking port.

Any object docked at the port will be undocked before the docking port is deleted.

After the function returns, the docking port handle is invalid and should no longer be
used.

@function del_dock
@tparam handle hDock docking port handle
@treturn bool _false_ indicates failure (invalid handle)
@see vessel:create_dock, vessel:get_dockcount, vessel:get_dockhandle, vessel:get_dockstatus, vessel:undock
*/
int Interpreter::v_del_dock (lua_State *L)
{
	static const char *funcname = "del_dock";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DOCKHANDLE hDock = (DOCKHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	lua_pushboolean (L, v->DelDock (hDock));
	return 1;
}

/***
Delete all docking ports defined for the vessel.

Any docked objects will be undocked before deleting the docking ports.

@function clear_dockdefinitions
@see vessel:create_dock, vessel:del_dock, vessel:get_dockcount, vessel:dock, vessel:undock
*/
int Interpreter::v_clear_dockdefinitions (lua_State *L)
{
	static const char *funcname = "clear_dockdefinitions";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	v->ClearDockDefinitions();
	return 0;
}

/***
Reset the parameters for a vessel docking port.

If the _hDock_ handle is omitted, the parameters of the vessel's primary docking port
are modified. In this case, if no docking port was previously defined, a new one is
created on the fly.

This function should not be called while the docking port is engaged.

The _dir_ and _rot_ direction vectors should be normalised to length 1.

@function set_dockparams
@tparam[opt] handle hDock docking port handle
@param pos (<i><b>@{types.vector|vector}</b></i>) dock reference position [<b>m</b>]
@param dir (<i><b>@{types.vector|vector}</b></i>) dock approach direction
@param rot (<i><b>@{types.vector|vector}</b></i>) longitudinal alignment vector
@see vessel:get_dockparams, vessel:create_dock, vessel:del_dock, vessel:get_dockcount, vessel:get_dockhandle, vessel:get_dockstatus, vessel:undock
*/
int Interpreter::v_set_dockparams (lua_State *L)
{
	static const char *funcname = "set_dockparams";
	AssertMtdMinPrmCount(L, 4, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DOCKHANDLE hDock = 0;
	int idx = 2;
	if (lua_islightuserdata(L,2)) {
		AssertMtdMinPrmCount(L, 5, funcname);
		hDock = (DOCKHANDLE)lua_touserdata(L,2);
		idx++;
	}
	VECTOR3 pos = luamtd_tovector_safe(L, idx++, funcname);
	VECTOR3 dir = luamtd_tovector_safe(L, idx++, funcname);
	VECTOR3 rot = luamtd_tovector_safe(L, idx++, funcname);
	if (hDock)
		v->SetDockParams(hDock,pos,dir,rot);
	else
		v->SetDockParams(pos,dir,rot);
	return 0;
}

/***
Get paramters of a docking port.

@function get_dockparams
@tparam handle hDock docking port handle
@return (<i><b>@{types.vector|vector}</b></i>) dock reference position [<b>m</b>]
@return (<i><b>@{types.vector|vector}</b></i>) dock approach direction
@return (<i><b>@{types.vector|vector}</b></i>) longitudinal alignment vector
@see vessel:set_dockparams, vessel:create_dock
*/
int Interpreter::v_get_dockparams (lua_State *L)
{
	static const char *funcname = "get_dockparams";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DOCKHANDLE hDock = (DOCKHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	VECTOR3 pos, dir, rot;
	v->GetDockParams (hDock, pos, dir, rot);
	lua_pushvector(L,pos);
	lua_pushvector(L,dir);
	lua_pushvector(L,rot);
	return 3;
}

/***
Get number of docking ports available on the vessel.

@function get_dockcount
@treturn int number of docking ports (&ge; 0)
@see vessel:create_dock, vessel:del_dock
*/
int Interpreter::v_get_dockcount (lua_State *L)
{
	static const char *funcname = "get_dockcount";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushinteger (L, v->DockCount());
	return 1;
}

/***
Get handle for a vessel docking port.

@function get_dockhandle
@tparam int idx dock index (0 &le; idx &lt; @{get_dockcount})
@treturn handle docking port handle (or nil if index out of range)
@see vessel:create_dock, vessel:get_dockcount
*/
int Interpreter::v_get_dockhandle (lua_State *L)
{
	static const char *funcname = "get_dockhandle";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	int idx = luamtd_tointeger_safe(L, 2, funcname);
	DOCKHANDLE h = v->GetDockHandle (idx);
	if (h) lua_pushlightuserdata (L, h);
	else lua_pushnil (L);
	return 1;
}

/***
Get current status of a docking port.
If the dock is engaged, the return value contains a handle for the docked object.
Otherwise the function returns nil.

@function get_dockstatus
@tparam handle hDock docking port handle
@treturn handle Handle for docked object, or nil if dock is not engaged
@see vessel:create_dock, vessel:get_dockhandle
*/
int Interpreter::v_get_dockstatus (lua_State *L)
{
	static const char *funcname = "get_dockstatus";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DOCKHANDLE hDock = (DOCKHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	OBJHANDLE hObj = v->GetDockStatus (hDock);
	if (hObj) lua_pushlightuserdata (L, hObj);
	else lua_pushnil (L);
	return 1;
}

/***
Get status flag for a docking port.

This method has the same functionality as
   <code> (GetDockStatus (GetDockHandle(port)) ? 1:0) </code>

@function dockingstatus
@tparam int port docking port index (>= 0)
@treturn int Docking status (0=free, 1=engaged)
@see get_dockstatus, get_dockhandle
*/
int Interpreter::v_dockingstatus (lua_State *L)
{
	static const char *funcname = "dockingstatus";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	UINT port = luamtd_tointeger_safe(L, 2, funcname);
	lua_pushinteger(L, v->DockingStatus(port));
	return 1;
}

/***
Undock any vessel attached to a docking port.

If the index parameter is set to -1, all docked vessels are undocked
simultaneously from all docking ports.

@function undock
@tparam int idx docking port index (0 &le; idx &lt; @{get_dockcount}, or -1)
*/
int Interpreter::v_undock(lua_State* L)
{
	static const char* funcname = "undock";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	UINT idx = (UINT)luamtd_tointeger_safe(L, 2, funcname);
	v->Undock(idx);
	return 0;
}

/***
Dock to another vessel.

This function is useful for designing scenario editors and during
startup configuration, but its use should be avoided during a
running simulation, because it can lead to unphysical situations:
it allows to dock two vessels regardless of their current
separation, by teleporting one of them to the location of the other.

During a simulation, Orbiter will dock two vessels automatically
when their docking ports are brought into close proximity.

The mode parameter determines how the vessels are connected. The
following settings are supported:

- 0: calculate the linear and angular moments of the superstructure
     from the moments of the docking components. This should only be used
     if the two vessels are already in close proximity and aligned for
     docking.
- 1: Keep this in place, and teleport the target vessel for docking
- 2: Keep the target in place, and teleport this for docking.
- 3: Softdock. Keep the target in place and match this vessel's docking port with
     target port alignment (i.e Ref, Dir and Rot gets matched to target). Add-on side code must
     bring the vessel to alignment and hard-dock using MoveDock.

@function dock
@tparam handle target handle of docking target vessel
@tparam number n docking port index on vessel (>= 0)
@tparam number tgtn docking port index on target (>= 0)
@tparam number mode attachment mode
@treturn number 0 if success, else error code :

- 1: docking port n on the vessel already in use
- 2: docking port tgtn on the target already in use
- 3: target vessel already part of the vessel's superstructure
 
*/
/***
Dock to another vessel.

This function is useful for designing scenario editors and during
startup configuration, but its use should be avoided during a
running simulation, because it can lead to unphysical situations:
it allows to dock two vessels regardless of their current
separation, by teleporting one of them to the location of the other.

During a simulation, Orbiter will dock two vessels automatically
when their docking ports are brought into close proximity.

The mode parameter determines how the vessels are connected. The
following settings are supported:

- 0: calculate the linear and angular moments of the superstructure
     from the moments of the docking components. This should only be used
     if the two vessels are already in close proximity and aligned for
     docking.
- 1: Keep this in place, and teleport the target vessel for docking
- 2: Keep the target in place, and teleport this for docking.
- 3: Softdock. Keep the target in place and match this vessel's docking port with
     target port alignment (i.e Ref, Dir and Rot gets matched to target). Add-on side code must
     bring the vessel to alignment and hard-dock using MoveDock.

@function dock
@tparam handle hSrc dock handle of source vessel
@tparam handle hTgt dock handle of target vessel
@tparam number mode attachment mode
@treturn number 0 if success, else error code :

- 1: docking port n on the vessel already in use
- 2: docking port tgtn on the target already in use
- 3: target vessel already part of the vessel's superstructure
 
*/int Interpreter::v_dock(lua_State* L)
{
	static const char* funcname = "dock";
	AssertMtdMinPrmCount(L, 4, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	int ret;
	if(lua_isnumber(L, 3)) {
		OBJHANDLE target = (OBJHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
		UINT n = (UINT)luamtd_tointeger_safe(L, 3, funcname);
		UINT tgtn = (UINT)luamtd_tointeger_safe(L, 4, funcname);
		UINT mode = (UINT)luamtd_tointeger_safe(L, 5, funcname);
		ret = v->Dock(target, n, tgtn, mode);
	} else {
		DOCKHANDLE source = (DOCKHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
		DOCKHANDLE target = (DOCKHANDLE)luamtd_tolightuserdata_safe(L, 3, funcname);
		UINT mode = (UINT)luamtd_tointeger_safe(L, 4, funcname);
		ret = v->Dock(source, target, mode);
	}
	lua_pushinteger(L, ret);
	return 1;
}

/***
Get closest free docking port from an other vessel.

@function get_proxydock
@tparam handle hDock docking port handle
@treturn handle docking port handle, nil if hDock is already occupied or nothing else founds.
*/
int Interpreter::v_get_proxydock(lua_State* L)
{
	static const char *funcname = "get_proxydock";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DOCKHANDLE hDock = (DOCKHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);

	DOCKHANDLE hProxy = v->GetProxyDock(hDock);

	if(hProxy)
		lua_pushlightuserdata(L, hProxy);
	else
		lua_pushnil(L);
	return 1;
}

/***
Get index of specified docking port.

@function get_dockindex
@tparam handle hDock docking port handle
@treturn number dock index or -1 if hDock doesn't belong to a vessel.
*/
int Interpreter::v_get_dockindex(lua_State* L)
{
	static const char *funcname = "get_dockindex";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DOCKHANDLE hDock = (DOCKHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);

	int idx = v->GetDockIndex(hDock);

	lua_pushinteger(L, idx);
	return 1;
}

/***
Get target docking port alignment relative to hDock in local vessel coords.

The function returns nil in case of error.

@function get_targetdockalignment
@tparam handle hDock dock handle
@tparam handle hTgt Target dock handle
@treturn vector target position
@treturn vector target direction
@treturn vector target rotation
@treturn vector target velocity between ports ECL frame
*/
int Interpreter::v_get_targetdockalignment(lua_State* L)
{
	static const char *funcname = "get_targetdockalignment";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DOCKHANDLE hDock = (DOCKHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	DOCKHANDLE hTgt = (DOCKHANDLE)luamtd_tolightuserdata_safe(L, 3, funcname);

	VECTOR3 ref, dir, rot, vel;

	bool ret = v->GetTargetDockAlignment(hDock, hTgt, &ref, &dir, &rot, &vel);

	if(ret) {
		lua_pushvector(L, ref);
		lua_pushvector(L, dir);
		lua_pushvector(L, rot);
		lua_pushvector(L, vel);
		return 4;
	}
	return 0;
}

/***
Move a docking port while vessel is docked. 

If no vessel is docked then does the same as set_dockparams.

@function move_dock
@tparam handle hDock dock handle
@tparam vector pos new dock reference position [<b>m</b>]
@tparam vector dir new approach direction
@tparam vector rot new longitudinal rotation alignment vector
*/
int Interpreter::v_move_dock(lua_State* L)
{
	static const char *funcname = "move_dock";
	AssertMtdMinPrmCount(L, 5, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DOCKHANDLE hDock = (DOCKHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);

	VECTOR3 pos = luamtd_tovector_safe(L, 3, funcname);
	VECTOR3 dir = luamtd_tovector_safe(L, 4, funcname);
	VECTOR3 rot = luamtd_tovector_safe(L, 5, funcname);

	v->MoveDock(hDock, pos, dir, rot);
	return 0;
}


/***
Attachments
@section vessel_mtd_attachment
*/

/***
Create a new attachment point.

A vessel can define multiple parent and child attachment points, and can subsequently
have multiple children attached, but it can only be attached to a single parent at any
one time.

The _dir_ and _rot_ vectors should both be normalised to length 1, and they should be
orthogonal.

The identifier string can contain up to 8 characters. It can be used to define
compatibility between attachment points.

If the attachment point is defined as loose, then the relative orientation between the
two attached objects is frozen to the orientation between them at the time the
connection was established. Otherwise, the two objects snap to the orientation defined
by their _dir_ vectors.

@function create_attachment
@tparam bool toParent If _true_, the attachment can be used to connect to a parent
(i.e. the vessel acts as a child). Otherwise, attachment is used to connect to a child
(i.e. vessel acts as parent)
@param pos (<i><b>@{types.vector|vector}</b></i>) attachment point position in vessel coordinates [<b>m</b>]
@param dir (<i><b>@{types.vector|vector}</b></i>) attachment direction in vessel coordinates
@param rot (<i><b>@{types.vector|vector}</b></i>) longitudinal alignment vector in vessel coordinates
@tparam string id compatibility identifier
@tparam[opt=false] bool loose If _true_, allow loose connections
@treturn handle attachment point handle
@see vessel:del_attachment, vessel:clear_attachments, vessel:set_attachmentparams,
vessel:get_attachmentparams, vessel:get_attachmenthandle, vessel:attach_child, vessel:detach_child
*/
int Interpreter::v_create_attachment (lua_State *L)
{
	static const char *funcname = "create_attachment";
	AssertMtdMinPrmCount(L, 6, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	bool toparent = luamtd_toboolean_safe(L, 2, funcname);
	VECTOR3 pos = luamtd_tovector_safe(L, 3, funcname);
	VECTOR3 dir = luamtd_tovector_safe(L, 4, funcname);
	VECTOR3 rot = luamtd_tovector_safe(L, 5, funcname);
	const char *id = lua_tostring_safe(L, 6, funcname);
	bool loose = false;
	if (lua_gettop(L) >= 7) {
		loose = luamtd_toboolean_safe(L, 7, funcname);
	}
	lua_pushlightuserdata (L, v->CreateAttachment (toparent, pos, dir, rot, id, loose));
	return 1;
}

/***
Delete an attachment point.

The attachment handle can refer to either a child or parent attachment point.

Any object attached to this point will be released.

After this function returns successfully, the attachment handle is no longer valid.

@function del_attachment
@tparam handle hAttachment attachment point handle
@treturn bool _false_ indicates failure (invalid attachment handle)
@see vessel:create_attachment, vessel:clear_attachments, vessel:get_attachmentcount
*/
int Interpreter::v_del_attachment (lua_State *L)
{
	static const char *funcname = "del_attachment";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	ATTACHMENTHANDLE hAttachment = (ATTACHMENTHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	lua_pushboolean (L, v->DelAttachment (hAttachment));
	return 1;
}

/***
Delete all attachment points defined for the vessel.

Any attached parent or child vessels will be released.

After this function returns, all previously defined attachment handles will no longer be valid.

@function clear_attachments
@see vessel:create_attachment, vessel:del_attachment, vessel:get_attachmentcount
*/
int Interpreter::v_clear_attachments (lua_State *L)
{
	static const char *funcname = "clear_attachments";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	v->ClearAttachments();
	return 0;
}

/***
Reset attachment position and orientation for an existing attachment point.

If the parameters of an attachment point are changed while a vessel is attached to that
point, the attached vessel will be shifted to the new position automatically.

The _dir_ and _rot_ vectors should both be normalised to length 1, and they should be
orthogonal.

@function set_attachmentparams
@tparam handle hAttachment attachment point handle
@param pos (<i><b>@{types.vector|vector}</b></i>) attachment point position in vessel coordinates [<b>m</b>]
@param dir (<i><b>@{types.vector|vector}</b></i>) attachment direction in vessel coordinates
@param rot (<i><b>@{types.vector|vector}</b></i>) longitudinal alignment vector in vessel coordinates
@see vessel:create_attachment, vessel:get_attachmentparams, vessel:get_attachmenthandle
*/

int Interpreter::v_set_attachmentparams (lua_State *L)
{
	static const char *funcname = "set_attachmentparams";
	AssertMtdMinPrmCount(L, 5, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	ATTACHMENTHANDLE hAttachment = (ATTACHMENTHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	VECTOR3 pos = luamtd_tovector_safe(L, 3, funcname);
	VECTOR3 dir = luamtd_tovector_safe(L, 4, funcname);
	VECTOR3 rot = luamtd_tovector_safe(L, 5, funcname);
	v->SetAttachmentParams (hAttachment, pos, dir, rot);
	return 0;
}

/***
Retrieve parameters of an attachment point.

@function get_attachmentparams
@tparam handle hAttachment attachment point handle
@return (<i><b>@{types.vector|vector}</b></i>) attachment point position in vessel coordinates [<b>m</b>]
@return (<i><b>@{types.vector|vector}</b></i>) attachment direction in vessel coordinates
@return (<i><b>@{types.vector|vector}</b></i>) longitudinal alignment vector in vessel coordinates
@see vessel:create_attachment, vessel:set_attachmentparams, vessel:get_attachmenthandle
*/
int Interpreter::v_get_attachmentparams (lua_State *L)
{
	static const char *funcname = "get_attachmentparams";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	ATTACHMENTHANDLE hAttachment = (ATTACHMENTHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	VECTOR3 pos, dir, rot;
	v->GetAttachmentParams (hAttachment, pos, dir, rot);
	lua_pushvector(L,pos);
	lua_pushvector(L,dir);
	lua_pushvector(L,rot);
	return 3;
}

/***
Retrieve attachment identifier string.

@function get_attachmentid
@tparam handle hAttachment attachment point handle
@treturn string attachment identifier string [up to 8 characters]
@see vessel:create_attachment, vessel:get_attachmenthandle, vessel:get_attachmentparams
*/
int Interpreter::v_get_attachmentid (lua_State *L)
{
	static const char *funcname = "get_attachmentid";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	ATTACHMENTHANDLE hAttachment = (ATTACHMENTHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	const char *id = v->GetAttachmentId (hAttachment);
	lua_pushstring (L, id);
	return 1;
}

/***
Get status of an attachment point.

This function returns either the handle of the attached vessel, or nil if nothing is
attached.

@function get_attachmentstatus
@tparam handle hAttachment attachment point handle
@treturn handle Handle of tha attached vessel, or nil
@see vessel:create_attachment, vessel:attach_child, vessel:detach_child
*/
int Interpreter::v_get_attachmentstatus (lua_State *L)
{
	static const char *funcname = "get_attachmentstatus";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	ATTACHMENTHANDLE hAttachment = (ATTACHMENTHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	OBJHANDLE hVessel = v->GetAttachmentStatus (hAttachment);
	if (hVessel) lua_pushlightuserdata (L, hVessel);
	else         lua_pushnil (L);
	return 1;
}

/***
Get number of child or parent attachment points defined for the vessel.

@function get_attachmentcount
@tparam bool toParent If _true_, return the number of attachment points to parents.
Otherwise, return the number of attachment points to children.
@treturn int Number of defined attachment points to connect to parents or to children.
@see vessel:create_attachment, vessel:del_attachment vessel:clear_attachments
*/
int Interpreter::v_get_attachmentcount (lua_State *L)
{
	static const char *funcname = "get_attachmentcount";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	bool toparent = luamtd_toboolean_safe(L, 2, funcname);
	lua_pushinteger(L,v->AttachmentCount(toparent));
	return 1;
}

/***
Get list index of the vessel's attachment point defined by its handle.

A vessel defines separate lists for child and parent attachment points. Therefore two
different attachment points may return the same index.

The index for a given attachment point can change when the vessel deletes any of its
attachments. The returned index should therefore be used only within the current frame.

@function get_attachmentindex
@tparam handle hAttachment attachment point handle
@treturn int List index (&ge; 0)
@see vessel:create_attachment, vessel:del_attachment, vessel:get_attachmenthandle
*/
int Interpreter::v_get_attachmentindex (lua_State *L)
{
	static const char *funcname = "get_attachmentindex";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	ATTACHMENTHANDLE hAttachment = (ATTACHMENTHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	lua_pushinteger(L,v->GetAttachmentIndex(hAttachment));
	return 1;
}

/***
Get handle of an attachment point identified by its list index.

@function get_attachmenthandle
@tparam bool toParent If _true_, return a handle for an attachment point to a parent.
Otherwise, return a handle for an attachment point to a child.
@tparam int idx attachment index (0 &le; idx &lt; @{get_attachmentcount})
@treturn handle attachment point handle, or nil if index out of range
@see vessel:create_attachment, vessel:get_attachmentcount, vessel:get_attachmentindex
*/
int Interpreter::v_get_attachmenthandle (lua_State *L)
{
	static const char *funcname = "get_attachmenthandle";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	bool toparent = luamtd_toboolean_safe(L, 2, funcname);
	DWORD idx = (DWORD)luamtd_tointeger_safe(L, 3, funcname);
	ATTACHMENTHANDLE hAttachment = v->GetAttachmentHandle (toparent, idx);
	if (hAttachment) lua_pushlightuserdata (L, hAttachment);
	else lua_pushnil (L);
	return 1;
}

/***
Attache a child vessel to an attachment point.

The hAttachment handle must refer to an attachment "to child" (i.e. created with
_toParent_=_false_); the _hChildAttachment_ handle must refer to an attachment
"to parent" on the child object (i.e. created with _toParent_=_true_). It is not
possible to connect two parent or two child attachment points.

A child can only be connected to a single parent at any one time. If the child is
already connected to a parent, the previous parent connection is severed.

The child may check the parent attachment's id string and, depending on the value,
refuse to connect. In that case, the function returns _false_.

@function attach_child
@tparam handle hChild handle of child vessel to be attached
@tparam handle hAttachment attachment point to which the child will be attached
@tparam handle hChildAttachment attachment point on the child to which we want to attach
@treturn bool _true_ indicates success, _false_ indicates failure (child refuses attachment)
@see vessel:create_attachment, vessel:detach_child, vessel:get_attachmenthandle, vessel:get_attachmentid
*/
int Interpreter::v_attach_child (lua_State *L)
{
	static const char *funcname = "attach_child";
	AssertMtdMinPrmCount(L, 4, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	OBJHANDLE hChild = (OBJHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	ATTACHMENTHANDLE hAttach = (ATTACHMENTHANDLE)luamtd_tolightuserdata_safe(L, 3, funcname);
	ATTACHMENTHANDLE hChildAttach = (ATTACHMENTHANDLE)luamtd_tolightuserdata_safe(L, 4, funcname);
	bool ok = v->AttachChild (hChild, hAttach, hChildAttach);
	lua_pushboolean (L, ok ? 1:0);
	return 1;
}

/***
Break an existing attachment to a child.

@function detach_child
@tparam handle hAttachment attachment point handle
@tparam[opt=0] number vel separation velocity [m/s]
@treturn bool _true_ when detachment is successful, _false_ if no child was attached, or if child refuses to detach.
@see vessel:create_attachment, vessel:attach_child, vessel:get_attachmenthandle
*/
int Interpreter::v_detach_child (lua_State *L)
{
	static const char *funcname = "detach_child";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	ATTACHMENTHANDLE hAttachment = (ATTACHMENTHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	double vel = 0.0;
	if (lua_gettop(L) >= 3) {
		vel = luamtd_tonumber_safe(L, 3, funcname);
	}
	bool ok = v->DetachChild (hAttachment, vel);
	lua_pushboolean (L, ok ? 1:0);
	return 1;
}


/***
Navigation radio
@section vessel_mtd_radio
*/

/***
Enable/disable transmission of transponder signal.

The transponder is a radio transmitter which can be used by other vessels
to obtain navigation information, e.g. for docking rendezvous approaches.

If the transponder is turned on (_enable_ = true), its initial frequency
is set to 108.00 MHz (channel 0). Use @{set_transponderchannel} to tune to
a different frequency.

@function enable_transponder
@tparam bool enable _true_ to enable the transponder, _false_ to disable it.
@see vessel:set_transponderchannel, vessel:get_transponder, vessel:enable_ids
*/
int Interpreter::v_enable_transponder (lua_State *L)
{
	static const char *funcname = "enable_transponder";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	int enable = luamtd_toboolean_safe(L, 2, funcname);
	v->EnableTransponder (enable!=0);
	return 0;
}

/***
Get handle of vessel transponder if available.

This function returns _nil_ unless the transponder has been enabled by a
call to @{enable_transponder} or by setting the EnableXPDR entry in the
vessel's config file to TRUE.

It is not safe to store the handle, because it can become invalid as a
result of disabling/enabling the transponder. Instead, the handle should
be queried when needed.

The handle can be used to retrieve information about the transmitter, such
as current frequency.

@function get_transponder
@treturn handle transponder handle, or _nil_ if not available
@see vessel:enable_transponder
*/
int Interpreter::v_get_transponder (lua_State *L)
{
	static const char *funcname = "get_transponder";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	NAVHANDLE hTrans = v->GetTransponder();
	if (hTrans) lua_pushlightuserdata (L, hTrans);
	else        lua_pushnil (L);
	return 1;
}

/***
Switch the channel number of the vessel's transponder.

The transponder channel can only be set if the transponder is enabled
(see @{enable_transponder}).

Transponders can be tuned from 108.00 to 140.00 MHz in steps of 0.05 MHz.
The frequency corresponding to a channel number ch is given by
f = (108.0 + 0.05 ch) MHz.

@function set_transponderchannel
@tparam int channel transponder channel (0..639)
@see vessel:enable_transponder
*/
int Interpreter::v_set_transponderchannel (lua_State *L)
{
	static const char *funcname = "set_transponderchannel";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DWORD ch = (DWORD)luamtd_tointeger_safe(L, 2, funcname);
	ASSERT_SYNTAX(ch < 640, "Argument 1: out of range");
	v->SetTransponderChannel (ch);
	return 0;
}

/***
Enable/disable one of the vessel's IDS (Instrument Docking System) transmitters.

If the IDS transmitter is turned on (_enable_ = true), its channel is
initially set to 0 (transmitter frequency 108.00 MHz). Use
@{set_idschannel} to tune to a different channel.

@function enable_ids
@tparam handle hDock docking port handle
@tparam bool enable _true_ to enable the transmitter, _false_ to disable it.
@see vessel:set_idschannel, vessel:get_ids
*/
int Interpreter::v_enable_ids (lua_State *L)
{
	static const char *funcname = "enable_ids";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DOCKHANDLE hDock = (DOCKHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	int enable = luamtd_toboolean_safe(L, 3, funcname);
	v->EnableIDS (hDock, enable!=0);
	return 0;
}

/***
Get handle of one of the vessel's instrument docking system (IDS) transmitters.

This function returns _nil_ if hDock does not define an IDS transmitter.

Docking port handles are returned by the @{create_dock} and @{get_dockhandle} methods.

The IDS handle becomes invalid when the dock is deleted (e.g. as a result
of @{del_dock} or @{clear_dockdefinitions}.

The handle returned by this function can be used to retrieve information
about the transmitter, such as sender frequency.

@function get_ids
@tparam handle hDock docking port handle
@treturn handle Navigation radio handle of the vessel's IDS transmitter for docking port hDock.
@see vessel:enable_ids, vessel:set_idschannel, vessel:create_dock, vessel:get_dockhandle
*/
int Interpreter::v_get_ids (lua_State *L)
{
	static const char *funcname = "get_ids";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DOCKHANDLE hDock = (DOCKHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	NAVHANDLE hIDS = v->GetIDS(hDock);
	if (hIDS) lua_pushlightuserdata (L, hIDS);
	else      lua_pushnil (L);
	return 1;
}

/***
Switche an IDS (Instrument Docking System) transmitter channel.

This function allows to switch an IDS transmitter for one of the vessel's
docking ports to a different channel.

Docking port handles are returned by the @{create_dock} and
@{get_dockhandle} methods.

The IDS handle becomes invalid when the dock is deleted (e.g. as a result
of @{del_dock} or @{clear_dockdefinitions}.

@function set_idschannel
@tparam handle hDock docking port handle
@tparam int ch IDS channel (0..639)
@see vessel:enable_ids, vessel:get_ids, vessel:set_transponderchannel
*/
int Interpreter::v_set_idschannel (lua_State *L)
{
	static const char *funcname = "set_idschannel";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DOCKHANDLE hDock = (DOCKHANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	DWORD ch = (DWORD)luamtd_tointeger_safe(L, 3, funcname);
	ASSERT_SYNTAX(ch < 640, "Argument 2: out of range");
	v->SetIDSChannel (hDock, ch);
	return 0;
}

/***
Set number of navigation (NAV) radio receivers supported by the vessel.

A vessel requires NAV radio receivers to obtain instrument navigation aids
such as ILS or docking approach information.

If no NAV receivers are available, then certain MFD modes such as Landing
or Docking will not be supported.

Default is 2 NAV receivers.

@function init_navradios
@tparam int nnav number of NAV radio receivers
@see vessel:get_navcount, vessel:get_navchannel, vessel:set_navchannel
*/
int Interpreter::v_init_navradios (lua_State *L)
{
	static const char *funcname = "init_navradios";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DWORD nnav = (DWORD)luamtd_tointeger_safe(L, 2, funcname);
	ASSERT_SYNTAX(nnav < 100, "Argument 1: out of range"); // sanity check
	v->InitNavRadios (nnav);
	return 0;
}

/***
Get number of NAV receivers.

@function get_navcount
@treturn int number of NAV receivers (&ge; 0)
@see vessel:init_navradios
*/
int Interpreter::v_get_navcount (lua_State *L)
{
	static const char *funcname = "get_navcount";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushnumber (L, v->GetNavCount());
	return 1;
}

/***
Set channel of a NAV radio receiver.

NAV radios can be tuned from 108.00 to 139.95 MHz in steps of 0.05 MHz,
corresponding to channels 0 to 639.

@function set_navchannel
@tparam int navIdx receiver index (&ge; 0)
@tparam int channel channel number [0..639]
@see vessel:init_navradios, vessel:get_navchannel
*/
int Interpreter::v_set_navchannel (lua_State *L)
{
	static const char *funcname = "set_navchannel";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DWORD n = (DWORD)lua_tointeger_safe(L, 2, funcname);
	DWORD ch = (DWORD)lua_tointeger_safe(L, 3, funcname);
	ASSERT_SYNTAX(ch < 640, "Argument 2: out of range");
	v->SetNavChannel (n, ch);
	return 0;
}

/***
Get current channel setting of a NAV radio receiver.

If the receiver index _navIdx_ is out of range, this function returns 0.

@function get_navchannel
@tparam int navIdx receiver index (&ge; 0)
@treturn int channel number [0..639]
@see vessel:set_navchannel, vessel:init_navradios
*/
int Interpreter::v_get_navchannel (lua_State *L)
{
	static const char *funcname = "get_navchannel";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DWORD n = (DWORD)lua_tointeger_safe(L, 2, funcname);
	DWORD ch = v->GetNavChannel (n);
	lua_pushnumber (L, ch);
	return 1;
}

/***
Get handle of transmitter source currently received by one of the vessel's NAV receivers.

This function returns the handle of the NAV transmitter currently received
by radio n, or nil if the radio is not tuned to any transmitter, or if
index _navIdx_ is out of range.

The handle returned by this function may change in consecutive calls,
depending on the radio frequency of the corresponding receiver, the vessel
position and the position of radio transmitters in the range of the
receiver.

@function get_navsource
@tparam int navIdx receiver index (&ge; 0)
@treturn handle NAV transmitter handle
@see vessel:get_navchannel
*/
int Interpreter::v_get_navsource (lua_State *L)
{
	static const char *funcname = "get_navsource";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DWORD n = (DWORD)lua_tointeger_safe(L, 2, funcname);
	NAVHANDLE hNav = v->GetNavSource (n);
	if (hNav) lua_pushlightuserdata(L,hNav);
	else      lua_pushnil (L);
	return 1;
}


/***
Nosewheel-steering and wheel brakes
@section vessel_mtd_nosewheel
*/

/***
Set activation state of the nose-wheel steering system.

@function set_nosewheelsteering
@treturn bool _true_ indicates nose-wheel steering is active.
@see set_nosewheelsteering
*/
int Interpreter::v_set_nosewheelsteering (lua_State *L)
{
	static const char *funcname = "set_nosewheelsteering";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	bool activate = luamtd_toboolean_safe(L, 2, funcname)!=0;
	v->SetNosewheelSteering(activate);
	return 0;
}

/***
Get activation state of the nose-wheel steering system.

@function get_nosewheelsteering
@treturn bool _true_ indicates nose-wheel steering is active.
@see set_nosewheelsteering
*/
int Interpreter::v_get_nosewheelsteering (lua_State *L)
{
	static const char *funcname = "get_nosewheelsteering";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	lua_pushboolean(L, v->GetNosewheelSteering());
	return 1;
}

/***
Define maximum force which can be provided by the vessel's wheel brake
system.

@function set_maxwheelbrakeforce
@tparam number f maximum force [N]
@see set_wheelbrakelevel, get_wheelbrakelevel
*/
int Interpreter::v_set_maxwheelbrakeforce (lua_State *L)
{
	static const char *funcname = "set_maxwheelbrakeforce";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double f = luamtd_tonumber_safe(L, 2, funcname);
	v->SetMaxWheelbrakeForce(f);
	return 0;
}

/***
Apply wheel brake.

@function set_wheelbrakelevel
@tparam number level wheelbrake level [0..1]
@tparam int which 0 = both, 1 = left, 2 = right main gear
@tparam bool permanent _true_ sets the level permanently, _false_
   only applies to current time step
@see set_maxwheelbrakeforce, get_wheelbrakelevel
*/
int Interpreter::v_set_wheelbrakelevel (lua_State *L)
{
	static const char *funcname = "set_wheelbrakelevel";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double level = luamtd_tonumber_safe(L, 2, funcname);
	int which      = lua_isnumber (L, 3) ? luamtd_tointeger_safe(L, 3, funcname)     : 0;
	bool permanent = lua_isboolean(L, 4) ? luamtd_toboolean_safe(L, 4, funcname)!= 0 : true;
	v->SetWheelbrakeLevel(level, which, permanent);
	return 0;
}

/***
Get current wheel brake level.

@function get_wheelbrakelevel
@tparam int which 0 = average of both main gear levels, 1 = left, 2 = right
@treturn number wheel brake level [0..1]
@see set_maxwheelbrakeforce, set_wheelbrakelevel
*/
int Interpreter::v_get_wheelbrakelevel (lua_State *L)
{
	static const char *funcname = "get_wheelbrakelevel";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	int which = luamtd_tointeger_safe(L, 2, funcname);
	lua_pushnumber(L, v->GetWheelbrakeLevel(which));
	return 1;
}


/***
Light sources
@section vessel_mtd_light
*/

/***
Add new isotropic point light source to the vessel.

The param table contains the geometric parameters of the light source. It
can contain the following fields:

- range (number): light source range [m] (default: 100)
- att0 (number): constant attenuation coefficient (see below) (default: 1e-3)
- att1 (number): linear attenuation coefficient (see below) (default: 0)
- att2 (number): quadratic attenuation coefficient (see below) (default: 1e-3)

Default values are substituted for any missing fields. The att parameters
define the light attenuation as a function of distance from the source.
Intensity _I_ as a function of distance _d_ is calculated as

I = I0 / (att0 + d att1 + d&sup2; att2)

where I0 is the source intensity multiplied with the RGB value of the given
colour component.

Each of the colour arguments (diffuse, specular and ambient) are expected to
be tables containing the fields r, g and b. Each value is normally in the
range 0 to 1, but can be &gt; 1 or even negative for special effects. All
three colour tables are optional. If ambient is missing, it is substituted
with {r=0,g=0,b=0}. If specular is missing, it is assumed to be the same as
diffuse. If diffuse is also missing, it is substituted with {r=1,g=1,b=1}.

@function add_pointlight
@param pos (<i><b>@{types.vector|vector}</b></i>) source position in vessel frame [<b>m</b>]
@tparam[opt] table param geometric parameters (see notes)
@tparam[opt] rgb-table diffuse source contribution to diffuse object colours
@tparam[opt] rgb-table specular source contribution to specular object colours
@tparam[opt] rgb-table ambient source contribution to ambient object colours
@treturn object the newly created light emitter object
@usage l = v:add_pointlight({x=1,y=1,z=0},{range=90,att0=1e-3,att2=2e-3},{r=1,g=0.8,b=0})

@see vessel:add_spotlight
*/
int Interpreter::v_add_pointlight (lua_State *L)
{
	static const char *funcname = "add_pointlight";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 pos = luamtd_tovector_safe(L, 2, funcname);

	int narg = lua_gettop(L);
	double att0 = 1e-3, att1 = 0, att2 = 1e-3;
	double range = 100;
	COLOUR4 col_diff = {1,1,1,0}, col_spec = {1,1,1,0}, col_ambi = {0,0,0,0};

	if (narg >= 3) {
		AssertMtdPrmType(L, 3, PRMTP_TABLE, funcname);
		lua_getfield(L,3,"range");
		if (lua_isnumber(L,-1)) range = lua_tonumber(L,-1);
		lua_pop(L,1);
		lua_getfield(L,3,"att0");
		if (lua_isnumber(L,-1)) att0 = lua_tonumber(L,-1);
		lua_pop(L,1);
		lua_getfield(L,3,"att1");
		if (lua_isnumber(L,-1)) att1 = lua_tonumber(L,-1);
		lua_pop(L,1);
		lua_getfield(L,3,"att2");
		if (lua_isnumber(L,-1)) att2 = lua_tonumber(L,-1);
		lua_pop(L,1);
		if (narg >= 4) {
			col_diff = lua_torgba(L,4);
			if (narg >= 5) {
				col_spec = lua_torgba(L,5);
				if (narg >= 6) {
					col_ambi = lua_torgba(L,6);
				}
			} else col_spec = col_diff;
		}
	}
	LightEmitter *le = v->AddPointLight (pos, range, att0, att1, att2, col_diff, col_spec, col_ambi);
	lua_pushlightemitter (L, le);
	return 1;
}

/***
Add new directed spot light source to the vessel.

The param table contains the geometric parameters of the light source. It
can contain the following fields:

- range (number): light source range [m] (default: 100)
- att0 (number): constant attenuation coefficient (see below) (default: 1e-3)
- att1 (number): linear attenuation coefficient (see below) (default: 0)
- att2 (number): quadratic attenuation coefficient (see below) (default: 1e-3)
- umbra (number): angular aperture of inner (maximum intensity) cone [rad] (default: 20&pi;/180)
- penumbra (number): angular aperture of outer (zero intensity) cone [rad] (default: 40&pi;/180)

Default values are substituted for any missing fields. The att parameters
define the light attenuation as a function of distance from the source.
Intensity _I_ as a function of distance _d_ is calculated as

I = I0 / (att0 + d att1 + d&sup2; att2)

where I0 is the source intensity multiplied with the RGB value of the given
colour component.

The light intensity drops linearly from maximum at the inner (umbra) radius to zero at the
outer (penumbra) radius.

Each of the colour arguments (diffuse, specular and ambient) are expected to
be tables containing the fields r, g and b. Each value is normally in the
range 0 to 1, but can be &gt; 1 or even negative for special effects. All
three colour tables are optional. If ambient is missing, it is substituted
with {r=0,g=0,b=0}. If specular is missing, it is assumed to be the same as
diffuse. If diffuse is also missing, it is substituted with {r=1,g=1,b=1}.

Example:

@function add_spotlight
@param pos (<i><b>@{types.vector|vector}</b></i>) source position in vessel frame [<b>m</b>]
@param dir (<i><b>@{types.vector|vector}</b></i>) source direction in vessel frame
@tparam[opt] table param geometric parameters (see notes)
@tparam[opt] rgb-table diffuse source contribution to diffuse object colours
@tparam[opt] rgb-table specular source contribution to specular object colours
@tparam[opt] rgb-table ambient source contribution to ambient object colours
@treturn object the newly created light emitter object
@usage l=v:add_spotlight({x=10,y=1,z=0},{x=0,y=0,z=1},
                {range=200,att0=1e-3,att1=0,att2=2e-3,umbra=0.3,penumbra=0.5},
                {r=1,g=0.8,b=0.7})

@see vessel:add_pointlight
*/
int Interpreter::v_add_spotlight (lua_State *L)
{
	static const char *funcname = "add_spotlight";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 pos = luamtd_tovector_safe(L, 2, funcname);
	VECTOR3 dir = luamtd_tovector_safe(L, 3, funcname);

	int narg = lua_gettop(L);
	double att0 = 1e-3, att1 = 0, att2 = 1e-3;
	double range = 100, umbra = 20*RAD, penumbra = 40*RAD;
	COLOUR4 col_diff = {1,1,1,0}, col_spec = {1,1,1,0}, col_ambi = {0,0,0,0};

	if (narg >= 4) {
		AssertMtdPrmType(L, 4, PRMTP_TABLE, funcname);
		lua_getfield(L,4,"range");
		if (lua_isnumber(L,-1)) range = lua_tonumber(L,-1);
		lua_pop(L,1);
		lua_getfield(L,4,"att0");
		if (lua_isnumber(L,-1)) att0 = lua_tonumber(L,-1);
		lua_pop(L,1);
		lua_getfield(L,4,"att1");
		if (lua_isnumber(L,-1)) att1 = lua_tonumber(L,-1);
		lua_pop(L,1);
		lua_getfield(L,4,"att2");
		if (lua_isnumber(L,-1)) att2 = lua_tonumber(L,-1);
		lua_pop(L,1);
		lua_getfield(L,4,"umbra");
		if (lua_isnumber(L,-1)) umbra = lua_tonumber(L,-1);
		lua_pop(L,1);
		lua_getfield(L,4,"penumbra");
		if (lua_isnumber(L,-1)) penumbra = lua_tonumber(L,-1);
		lua_pop(L,1);
		if (narg >= 5) {
			col_diff = lua_torgba(L,5);
			if (narg >= 6) {
				col_spec = lua_torgba(L,6);
				if (narg >= 7) {
					col_ambi = lua_torgba(L,7);
				}
			} else col_spec = col_diff;
		}
	}
	LightEmitter *le = v->AddSpotLight (pos, dir, range, att0, att1, att2, umbra, penumbra, col_diff, col_spec, col_ambi);
	lua_pushlightemitter (L, le);
	return 1;
}

/***
Get light source object identified by index.

@function get_lightemitter
@tparam int idx light source index (&ge; 0)
@treturn lightemitter light source object, or nil if index out of range
@see vessel:add_pointlight, vessel:add_spotlight, vessel:get_lightemittercount
*/
int Interpreter::v_get_lightemitter (lua_State *L)
{
	static const char *funcname = "get_lightemitter";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DWORD idx = (DWORD)luamtd_tointeger_safe(L, 2, funcname);
	const LightEmitter *le = v->GetLightEmitter (idx);
	if (le) lua_pushlightemitter (L, le);
	else    lua_pushnil (L);
	return 1;
}

/***
Get number of light sources defined for the vessel.

@function get_lightemittercount
@treturn int number of light sources
@see vessel:add_pointlight, vessel:add_spotlight, vessel:del_lightemitter, vessel:clear_lightemitters
*/
int Interpreter::v_get_lightemittercount (lua_State *L)
{
	static const char *funcname = "get_lightemittercount";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DWORD n = v->LightEmitterCount();
	lua_pushinteger (L, n);
	return 1;
}

/***
Delete one of the vessel's light sources.

If the function returns _true_, the light source object was deallocated and should no longer
be referenced.

@function del_lightemitter
@tparam lightemitter le light emitter object
@treturn bool _true_ if light source was successfully deleted, _false_ if the vessel didn't recognise the object.
@see vessel:add_pointlight, vessel:add_spotlight, vessel:clear_lightemitters
*/
int Interpreter::v_del_lightemitter (lua_State *L)
{
	static const char *funcname = "del_lightemitter";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	LightEmitter *le = lua_tolightemitter(L, 2);
	bool ok = v->DelLightEmitter (le);
	lua_pushboolean (L, ok);
	return 1;
}

/***
Remove all light sources defined for the vessel.

@function clear_lightemitters
@see vessel:add_pointlight, vessel:add_spotlight, vessel:del_lightemitter
*/
int Interpreter::v_clear_lightemitters (lua_State *L)
{
	static const char *funcname = "clear_lightemitters";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	v->ClearLightEmitters();
	return 0;
}


/***
Meshes
@section vessel_mtd_mesh
*/

/***
Load mesh definition for the vessel from a file or from a pre-loaded mesh template.

_meshName_ defines a path to an existing mesh file. The mesh must be in Orbiter's MSH format
(see 3DModel.pdf).

The file name (including optional directory path) is relative to Orbiter's mesh directory
(usually ".\Meshes"). The file extension must not be specified (.msh is assumed.)

_hMesh_ is a handle to a mesh previously loaded with oapi.load_meshglobal.

The global handle _hMesh_ represents a "mesh template". Whenever the vessel needs to create
its visual representation (when moving within visual range of the observer camera), it
creates its individual mesh as a copy of the template.

The mesh is either appended to the end of the vessel's mesh list, or inserted at the
location of a previously deleted mesh (see @{del_mesh}).

The returned value is the mesh list index at which the mesh reference was stored. It can be
used to identify the mesh later (e.g. for animations).

This function only creates a reference to a mesh, but does not directly load the mesh from
frile. The mesh is physically loaded only when it is required (whenever the vessel moves
within visual range of the observer camera).

@function add_mesh
@tparam ?string|handle mesh mesh file name (meshName) or handle of template mesh (hMesh)
@param[opt] ofs (<i><b>@{types.vector|vector}</b></i>) vector defining the offset of mesh origin from vessel origin [<b>m</b>]
@treturn int idx mesh index (&ge; 0)
@see vessel:del_mesh, vessel:insert_mesh, oapi.load_meshglobal
*/
int Interpreter::v_add_mesh (lua_State *L)
{
	static const char *funcname = "add_mesh";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	UINT midx;
	VECTOR3 ofs, *pofs = 0;
	if (lua_gettop(L) >= 3) {
		ofs = luamtd_tovector_safe(L, 3, funcname);
		pofs = &ofs;
	}
	AssertMtdPrmType(L, 2, PRMTP_STRING | PRMTP_USERDATA, funcname);
	if (lua_isstring(L, 2)) {
		const char *str = lua_tostring(L,2);
		midx = v->AddMesh (str, pofs);
	} else {
		MESHHANDLE hMesh = lua_tomeshhandle(L, 2);
		midx = v->AddMesh (hMesh, pofs);
	}
	lua_pushnumber (L, midx);
	return 1;
}

/***
Insert or replace a mesh at a specific index location of the vessel's mesh list.

_meshName_ defines a path to an existing mesh file. The mesh must be in Orbiter's MSH format
(see 3DModel.pdf).

The file name (including optional directory path) is relative to Orbiter's mesh directory
(usually ".\Meshes"). The file extension must not be specified (.msh is assumed.)

_hMesh_ is a handle to a mesh previously loaded with oapi.load_meshglobal.

The global handle _hMesh_ represents a "mesh template". Whenever the vessel needs to create
its visual representation (when moving within visual range of the observer camera), it
creates its individual mesh as a copy of the template.

_idx_ is a zero-based index which specifies at which point the mesh reference is added into
the vessel's mesh list. If a mesh already exists at this position, it is overwritten. If _idx_
&gt; number of meshes, then the required number of (empty) entries is generated.

The return value is always equal to _idx_.

@function insert_mesh
@tparam ?string|handle mesh mesh file name (meshName) or handle of template mesh (hMesh)
@tparam int idx mesh index (&ge; 0)
@param[opt] ofs (<i><b>@{types.vector|vector}</b></i>) vector defining the offset of mesh origin from vessel origin [<b>m</b>]
@treturn int mesh index (&ge; 0)
@see vessel:del_mesh, vessel:add_mesh, oapi.load_meshglobal
*/
int Interpreter::v_insert_mesh (lua_State *L)
{
	static const char *funcname = "insert_mesh";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	UINT midx, idx = (UINT)lua_tointeger_safe(L, 3, funcname);
	VECTOR3 ofs, *pofs = 0;
	if (lua_gettop(L) >= 4) {
		ofs = luamtd_tovector_safe(L, 4, funcname);
		pofs = &ofs;
	}
	AssertMtdPrmType(L, 2, PRMTP_STRING | PRMTP_USERDATA, funcname);
	if (lua_isstring(L, 2)) {
		const char *str = lua_tostring(L,2);
		midx = v->InsertMesh (str, idx, pofs);
	} else {
		MESHHANDLE hMesh = lua_tomeshhandle(L,2);
		midx = v->InsertMesh (hMesh, idx, pofs);
	}
	lua_pushnumber (L, midx);
	return 1;
}

/***
Remove a mesh from the vessel's mesh list.

After a mesh has been deleted, the mesh index is no longer valid, and should not be used
any more in function calls (e.g. for animations).

If meshes are added subsequently, they are placed in the vacant list slots, and therefore
can be assigned the indices of previously deleted meshes.

If you want to replace a mesh, it is easier to use the @{insert_mesh} function instead of a
combination of del_mesh and @{add_mesh}.

By default, all animation components associated with the mesh are deleted. This can be
prevented by setting _retainAnim_ to _true_. In general this is only useful if the same
mesh is subsequently loaded again into the same mesh index slot. In all other cases,
retaining the animations of deleted meshes can lead to undefined behaviour.

@function del_mesh
@tparam int idx mesh index (&ge; 0)
@tparam[opt=false] bool retainAnim flag for keeping mesh animations
@treturn bool _true_ on success, _false_ to indicate failure (index out of range, or mesh already deleted.)
@see vessel:insert_mesh, vessel:add_mesh
*/
int Interpreter::v_del_mesh (lua_State *L)
{
	static const char *funcname = "del_mesh";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	ASSERT_MTDNUMBER(L,2);
	UINT idx = (UINT)luamtd_tointeger_safe(L, 2, funcname);
	bool retain_anim = false;
	if (lua_gettop(L) >= 3) {
		retain_anim = luamtd_toboolean_safe(L, 3, funcname);
	}
	bool ok = v->DelMesh (idx, retain_anim);
	lua_pushboolean (L,ok);
	return 1;
}

/***
Remove all mesh definitions for the vessel.

If _retainAnim_ is _false_, all animations defined for the vessel are deleted together
with the meshes. If _true_, the animations stay behind. This is only useful if the same
meshes are subsequently added again in the same order, so that the animations point to the
appropriate meshes and mesh groups and can be re-used. If different meshes are loaded later,
the behaviour of the animations becomes undefined.

@function clear_meshes
@tparam[opt=false] retainAnim flag for keeping mesh animations
@see vessel:insert_mesh, vessel:add_mesh, vessel:del_mesh
*/
int Interpreter::v_clear_meshes (lua_State *L)
{
	static const char *funcname = "clear_meshes";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	bool retain_anim = false;
	if (lua_gettop(L) >= 2) {
		retain_anim = luamtd_toboolean_safe(L, 2, funcname);
	}
	v->ClearMeshes (retain_anim);
	return 0;
}

/***
Get number of meshes defined for the vessel.

@function get_meshcount
@treturn int mesh count (&ge; 0)
@see vessel:add_mesh, vessel:insert_mesh, vessel:del_mesh, vessel:clear_meshes
*/
int Interpreter::v_get_meshcount (lua_State *L)
{
	static const char *funcname = "get_meshcount";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	UINT count = v->GetMeshCount();
	lua_pushnumber (L, count);
	return 1;
}

/***
Shift the position of a mesh relative to the vessel's local coordinate system.

This function does not define an animation (i.e. gradual transition), but resets the mesh
position instantly.

@function shift_mesh
@tparam int idx mesh index (&ge; 0)
@param ofs (<i><b>@{types.vector|vector}</b></i>) translation vector [<b>m</b>]
@treturn bool _true_ on success, _false_ indicates error (index out of range).
@usage v = vessel.get_focusinterface()
v:shift_mesh(0,{x=10,y=20,z=30})
term.out(v:get_meshoffset(0))

@see vessel:shift_meshes, vessel:get_meshoffset
*/
int Interpreter::v_shift_mesh (lua_State *L)
{
	static const char *funcname = "shift_mesh";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	ASSERT_MTDNUMBER(L,2);
	UINT idx = (UINT)luamtd_tointeger_safe(L, 2, funcname);
	VECTOR3 ofs = luamtd_tovector_safe(L, 3, funcname);
	bool ok = v->ShiftMesh (idx, ofs);
	lua_pushboolean (L,ok);
	return 1;
}

/***
Shift position of all meshes relative to the vessel's local coordinate system.

This function is useful when resetting a vessel's centre of gravity, in combination with
@{shift_centreofmass}.

@function shift_meshes
@param ofs (<i><b>@{types.vector|vector}</b></i>) translation vector [<b>m</b>]
@see vessel:shift_mesh, vessel:get_meshoffset, vessel:shift_centreofmass, vessel:shiftCG
*/
int Interpreter::v_shift_meshes (lua_State *L)
{
	static const char *funcname = "shift_meshes";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 ofs = luamtd_tovector_safe(L, 2, funcname);
	v->ShiftMeshes (ofs);
	return 0;
}

/***
Get mesh offset in the vessel frame.

@function get_meshoffset
@tparam int idx mesh index (&ge; 0)
@return (<i><b>@{types.vector|vector}</b></i>) mesh offset [<b>m</b>], or _nil_ if index out of range
@see vessel:add_mesh, vessel:insert_mesh, vessel:shift_mesh, vessel:shift_meshes
*/
int Interpreter::v_get_meshoffset(lua_State* L)
{
	static const char* funcname = "get_meshoffset";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	UINT idx = (UINT)luamtd_tointeger_safe(L, 2, funcname);
	VECTOR3 ofs;
	bool ok = v->GetMeshOffset(idx, ofs);
	if (ok) lua_pushvector(L, ofs);
	else lua_pushnil(L);
	return 1;
}

/***
Get handle for a device-specific mesh instance

For Orbiter_ng, this method returns a handle for a mesh instance managed
by the external graphics client. Graphics clients may implement their own
mesh formats, so the object pointed to by the handle is client-specific.

For inline graphics version, the returned handle points to the same object
as the handle returned by get_mesh .

@function get_devmesh
@tparam handle vis identifies the visual for which the mesh was created.
@tparam number idx mesh index (0 <= idx < get_meshcount())
@return handle device mesh handle
*/
int Interpreter::v_get_devmesh(lua_State* L)
{
	static const char* funcname = "get_devmesh";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	VISHANDLE h = (VISHANDLE)lua_tolightuserdata_safe(L, 2, funcname);
	UINT idx = (UINT)luamtd_tointeger_safe(L, 3, funcname);
	DEVMESHHANDLE hDM = v->GetDevMesh(h, idx);
	if (hDM) lua_pushdevmeshhandle(L, hDM);
	else lua_pushnil(L);
	return 1;
}

/***
Set the visibility flags for a vessel mesh.

This method can be used to specify if a mesh is visible
in particular camera modes. Some meshes may only be visible
in external views, while others should only be visible in
cockpit views.

Turning off the unnecessary rendering of meshes can
improve the performance of the simulator.

mode can be a combination of the meshvis.

The default mode after adding a mesh is MESHVIS.EXTERNAL.

MESHVIS.EXTPASS can't be used on its own, but as a modifier to any of the
other visibility modes. If specified, it forces the mesh to be rendered in
Orbiter's external render pass, even if it is labelled as internal (e.g.
MESHVIS.COCKPIT or MESHVIS.VC). The significance of the external render pass
is that it allows the mesh to be obscured by other objects in front of it.
However, objects in the external render pass are clipped at a camera distance
of 2.5m. Meshes that are rendered during the internal pass always cover all
other objects, and have a smaller clipping distance.

Use the MESHVIS.EXTPASS modifier for parts of the vessel that are visible
from the cockpit, but are not close to the camera and may be obscured by other
objects. An example is the Shuttle payload bay, which can be covered by payload
objects.

@function set_mesh_visibility_mode
@tparam number idx mesh index (>= 0)
@tparam number mode visibility mode flags
*/
int Interpreter::v_set_mesh_visibility_mode(lua_State* L)
{
	static const char* funcname = "set_mesh_visibility_mode";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	UINT idx = (UINT)luamtd_tointeger_safe(L, 2, funcname);
	UINT mode = (UINT)luamtd_tointeger_safe(L, 3, funcname);
	v->SetMeshVisibilityMode(idx, mode);
	return 0;
}

/***
Animations
@section vessel_mtd_anim
*/

/***
Create a mesh animation object.

After creating an animation, components can be added with @{add_animationcomponent}.

Use @{set_animation} to manipulate the animation state.

0 &le; initialState &le; 1 defines at which state the animation is stored in the mesh file.
Example: Landing gear animation between retracted state (0) and deployed state (1). If the
landing gear is retracted in the mesh file, set initialState to 0. If it is deployed in the
mesh file, set initialState to 1.

@function create_animation
@tparam number initialState the animation state corresponding to the unmodified mesh
@treturn int Animation identifier
@see vessel:add_animationcomponent, vessel:set_animation
*/
int Interpreter::v_create_animation (lua_State *L)
{
	static const char *funcname = "create_animation";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double istate = luamtd_tonumber_safe(L, 2, funcname);
	UINT anim = v->CreateAnimation (istate);
	lua_pushnumber (L, anim);
	return 1;
}

/***
Delete an existing mesh animation object.

The animation is deleted by removing all the components associated with it. Subsequently,
any calls to SetAnimation using this animation index will not have any effect.

Before the animation is deleted, the mesh groups associated with the animation are reset to
their default (initial) positions. To avoid jumps in the visual appearance of the vessel,
animations should therefore only be deleted when the animation state has returned to the
default state.

@function del_animation
@tparam int anim animation identifier
@treturn bool _true_ if animation was deleted successfully
@see vessel:create_animation
*/
int Interpreter::v_del_animation (lua_State *L)
{
	static const char *funcname = "del_animation";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	UINT anim = (UINT)luamtd_tointeger_safe(L, 2, funcname);
	bool ok = v->DelAnimation (anim);
	lua_pushboolean (L,ok);
	return 1;
}

/***
Set the state of an animation.

Each animation is defined by its state, with extreme points state=0 and state=1. When
setting a state between 0 and 1, Orbiter carries out the appropriate transformations to
advance the animation to that state. It is the responsibility of the code developer to call
SetAnimation in such a way as to provide a smooth movement of the animated parts.

@function set_animation
@tparam int anim animation identifier
@tparam number state animation state (0...1)
@treturn bool _false_ indicates failure (animation identifier out of range)
@see vessel:create_animation, vessel:del_animation, vessel:add_animationcomponent
*/
int Interpreter::v_set_animation (lua_State *L)
{
	static const char *funcname = "set_animation";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	UINT anim = (UINT)luamtd_tointeger_safe(L, 2, funcname);
	double state = luamtd_tonumber_safe(L, 3, funcname);
	bool ok = v->SetAnimation (anim, state);
	lua_pushboolean (L,ok);
	return 1;
}

/***
Get the current state of an animation

@function set_animation
@tparam number anim animation identifier
@treturn number animation state [0..1]
*/
int Interpreter::v_get_animation (lua_State *L)
{
	static const char *funcname = "get_animation";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	UINT anim = luamtd_tointeger_safe(L, 2, funcname);
	lua_pushnumber(L, v->GetAnimation(anim));
	return 1;
}

/***
Add component (rotation, translation or scaling) to an animation.

_state0_ and _state1_ (0..1) allow to define the temporal endpoints of the component's
animation within the sequence. For example, _state0_=0 and _state1_=1 perform the
animation over the whole duration of the animation sequence, while _state0_=0 and
_state1_=0.5 perform the animation over the first half of the total animation. This
allows to build complex animations where different components are animated in a defined
temporal sequence.

The transformation (rotation, translation or scaling) is defined by the trans object
which is created by a call to @{oapi.create_animationcomponent}. The transformation
object must not be deleted before the animation component is deleted.

To define a transformation as a child of another transformation, set _hParent_ to the
handle returned by the add_animationcomponent call for the parent.

@function add_animationcomponent
@tparam int anim animation identifier, as returned by @{create_animation}
@tparam number state0 animation cutoff state 0 for the component
@tparam number state1 animation cutoff state 1 for the component
@tparam handle hTrans transformation object (see @{oapi.create_animationcomponent})
@tparam[opt] handle hParentAnim parent transformation
@treturn handle Animation component handle
@see vessel:create_animation
*/
int Interpreter::v_add_animationcomponent (lua_State *L)
{
	static const char *funcname = "add_animationcomponent";
	AssertMtdMinPrmCount(L, 5, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	UINT anim = (UINT)luamtd_tointeger_safe(L, 2, funcname);
	double state0 = luamtd_tonumber_safe(L, 3, funcname);
	double state1 = luamtd_tonumber_safe(L, 4, funcname);
	MGROUP_TRANSFORM *trans = (MGROUP_TRANSFORM*)luamtd_tolightuserdata_safe(L, 5, funcname);
	ANIMATIONCOMPONENT_HANDLE hparent = NULL;
	if (lua_gettop(L) >= 6)
		hparent = (ANIMATIONCOMPONENT_HANDLE)luamtd_tolightuserdata_safe(L, 6, funcname);
	ANIMATIONCOMPONENT_HANDLE hanimcomp =
		v->AddAnimationComponent (anim, state0, state1, trans, hparent);
	if(hanimcomp)
		lua_pushlightuserdata (L,hanimcomp);
	else
		lua_pushnil(L);
	return 1;
}

/***
Remove component from an animation.

If the component has children belonging to the same animation,
   these will be deleted as well.

In the current implementation, the component must not have children
   belonging to other animations. Trying to delete such a component will
   result in undefined behaviour.

@function del_animationcomponent
@tparam int anim animation identifier
@tparam handle hAC animation component handle
@treturn bool _false_ indicates failure (\a anim out of range, or \a hAC invalid)
@see add_animationcomponent
*/
int Interpreter::v_del_animationcomponent (lua_State *L)
{
	static const char *funcname = "del_animationcomponent";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	UINT anim = luamtd_tointeger_safe(L, 2, funcname);
	ANIMATIONCOMPONENT_HANDLE hAC = (ANIMATIONCOMPONENT_HANDLE)luamtd_tolightuserdata_safe(L, 3, funcname);
	lua_pushboolean(L, v->DelAnimationComponent(anim, hAC));
	return 1;
}

/***
Log a request for calls to clbk_animate

This function allows to implement animation sequences in combination
with the clbk_animate callback function. After a call to
@{register_animation}, clbk_animate is called at each time step
whenever the vessel's visual object exists.

Use @{unregister_animation} to stop further calls to clbk_animate

Each call to @{register_animation} increments a reference counter, while
each call to @{unregister_animation} decrements the counter. Orbiter
continues calling clbk_animate as long as the counter is greater
than 0.

If clbk_animate is not overloaded by the module,
@{register_animation} has no effect.

The @{register_animation} mechanism leaves the actual implementation of
the animation (transformation of mesh groups, etc.) entirely to the
module. The @{create_animation} / @{add_animationcomponent}
mechanism is an alternative way to define animations where the
transformations are managed by the Orbiter core.

@function register_animation
@see unregister_animation, create_animation, add_animationcomponent
*/
int Interpreter::v_register_animation (lua_State *L)
{
	static const char *funcname = "register_animation";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	v->RegisterAnimation();
	return 0;
}

/***
Unlogs an animation request.

This stops a request for animation callback calls from a previous
   @{register_animation}.

The call to UnregisterAnimation should not be placed in the body of
   clbk_animate, since it may be lost if the vessel's visual
   doesn't exist.

@function unregister_animation
@see register_animation
*/
int Interpreter::v_unregister_animation (lua_State *L)
{
	static const char *funcname = "unregister_animation";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	v->UnregisterAnimation();
	return 0;
}


/***
Coordinate transformations
@section vessel_mtd_coordtrans
*/

/***
Register a shift in the centre of mass after a structural change (e.g. stage
separation).

This function should be called after a vessel has undergone a
structural change which resulted in a shift of the vessel's centre of
gravity (CG). Note that in Orbiter, a vessel's CG coincides by definition
always with the origin (0,0,0) of its local reference frame.
Therefore, in order to achieve a shift of the CG by a vector <b>S</b>,
this function shifts the vessel's global position by +<b>S</b>.
This allows to shift the meshes by -<b>S</b>, thus retaining their
global position.
The net result is unchanged mesh positions in the global frame, but a
shift of the local frame of reference (and thus CG) of +<b>S</b>.

The camera position is shifted to take into account the new CG. An
external camera view performs a smooth transition.

The shift of meshes (and any other reference positions defined in
the local vessel frame, such as docking ports, etc.) is not performed
by this function but must be executed separately.
A more convenient way to implement a transition of the centre of
mass is the function @{shiftCG}, which automatically takes care of
translating meshes, docking ports, etc.

@function shift_centreofmass
@tparam shift centre of mass displacement vector [<b>m</b>]
@see shiftCG
*/
int Interpreter::v_shift_centreofmass (lua_State *L)
{
	static const char *funcname = "shift_centreofmass";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 shift = luamtd_tovector_safe(L, 2, funcname);
	v->ShiftCentreOfMass(shift);
	return 0;
}

/***
Shift the centre of gravity of a vessel.

This function should be called after a vessel has undergone a
structural change which resulted in a shift of the vessel's centre of
gravity (CG). Note that in Orbiter, a vessel's CG coincides by definition
always with the origin (0,0,0) of its local reference frame.
Therefore, in order to achieve a shift of the CG by shift,
this function performs the following actions:

- Calls @{shift_centreofmass} (+shift) to align the vessel's global
position with the new CG position.
- Calls @{shift_meshes} (-shift) to compensate the mesh positions
- Applies equivalent shift to all :

  - thruster positions,
  - docking ports,
  - attachment points,
  - explicitly defined light source positions,
  - and to the cockpit camera position

The net effect is a shift of the vessel frame of reference (and thus the
CG by +shift, while the mesh positions remain in place in the global
frame.

@function shiftCG
@tparam vector shift centre of gravity displacement vector [<b>m</b>]
@see shift_centreofmass, shift_meshes
*/
int Interpreter::v_shiftCG (lua_State *L)
{
	static const char *funcname = "shiftCG";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 shift = luamtd_tovector_safe(L, 2, funcname);
	v->ShiftCG(shift);
	return 0;
}

/***
Get centre of gravity of the superstructure to which the vessel
belongs, if applicable.

The returned vector is the position of the superstructure centre
   of gravity, in coordinates of the local vessel frame.

If the vessel is not part of a superstructure, cg returns (0,0,0).

@function get_superstructureCG
@return (<i><b>@{types.vector|vector}</b></i>) cg superstructure centre of gravity [<b>m</b>] if the vessel is
   part of a superstructure, _nil_ otherwise.
*/
int Interpreter::v_get_superstructureCG (lua_State *L)
{
	static const char *funcname = "get_superstructureCG";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 cg;
	bool result = v->GetSuperstructureCG(cg);
	if (result) {
		lua_pushvector(L, cg);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/***
Apply a rotation by replacing the vessel's local to global rotation matrix.

The rotation matrix maps from the orientation of the vessel's
   local frame of reference to the orientation of the global frame
   (ecliptic at 2000.0).

The user is responsible for providing a valid rotation matrix.
   The matrix must be orthogonal and normalised: the norms of all
   column vectors of R must be 1, and scalar products between any
   column vectors must be 0.

@function set_rotationmatrix
@tparam matrix R rotation matrix
@see get_rotationmatrix, local2global
*/
int Interpreter::v_set_rotationmatrix (lua_State *L)
{
	static const char *funcname = "set_rotationmatrix";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	MATRIX3 R = luamtd_tomatrix_safe(L, 2, funcname);
	v->SetRotationMatrix(R);
	return 0;
}

/***
Perform a rotation of a direction from the local vessel frame to the global
frame.

This function is equivalent to multiplying rloc with the
   rotation matrix returned by @{get_rotationmatrix}.

Should be used to transform \e directions. To transform
   \e points, use @{local2global}, which additionally adds the
   vessel's global position to the rotated point.

@function globalrot
@tparam vector rloc point in local vessel coordinates
@return (<i><b>@{types.vector|vector}</b></i>) rglob rotated point
@see get_rotationmatrix, local2global
*/
int Interpreter::v_globalrot (lua_State *L)
{
	static const char *funcname = "globalrot";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 rloc = luamtd_tovector_safe(L, 2, funcname);
	VECTOR3 rglob;
	v->GlobalRot(rloc, rglob);
	lua_pushvector(L, rglob);
	return 1;
}

/***
Perform a rotation from the local vessel frame to the current local horizon
frame.

The local horizon frame is defined as follows:

   - y is "up" direction (planet centre to vessel centre)
   - z is "north" direction
   - x is "east" direction

@function horizonrot
@tparam vector rloc vector in local vessel coordinates
@return (<i><b>@{types.vector|vector}</b></i>) rhorizon vector in local horizon coordinates
@see horizoninvrot, globalrot, get_rotationmatrix, set_rotationmatrix
*/
int Interpreter::v_horizonrot (lua_State *L)
{
	static const char *funcname = "horizonrot";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 rloc = luamtd_tovector_safe(L, 2, funcname);
	VECTOR3 rhorizon;
	v->HorizonRot(rloc, rhorizon);
	lua_pushvector(L, rhorizon);
	return 1;
}

/***
Perform a rotation of a direction from the current local horizon frame to the
local vessel frame.

This function performs the inverse operation of \ref
   HorizonRot.

@function horizoninvrot
@tparam vector rhorizon vector in local horizon coordinates
@return (<i><b>@{types.vector|vector}</b></i>) rloc vector in local vessel coordinates
@see horizonrot, globalrot, get_rotationmatrix, set_rotationmatrix
*/
int Interpreter::v_horizoninvrot (lua_State *L)
{
	static const char *funcname = "horizoninvrot";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 rhorizon = luamtd_tovector_safe(L, 2, funcname);
	VECTOR3 rloc;
	v->HorizonInvRot(rhorizon, rloc);
	lua_pushvector(L, rloc);
	return 1;
}

/***
Perform a transformation from local vessel coordinates to global coordinates.

This function maps a point from the vessel's local coordinate
   system (centered at the vessel CG) into the global ecliptic
   system (centered at the solar system barycentre).

The transform has the form
   \f[ \vec{p}_g = \mathsf{R}_v \vec{p}_l + \vec{p}_v \f]
   where R<sub>v</sub> is the vessel's global rotation matrix
   (as given by @{get_rotationmatrix}), and \f$\vec{p}_v\f$
   is the vessel position in the global frame.

@function local2global
@tparam vector local point in local vessel coordinates [<b>m</b>]
@return (<i><b>@{types.vector|vector}</b></i>) global transformed point in global coordinates [<b>m</b>]
@see get_rotationmatrix, global2local
*/
int Interpreter::v_local2global (lua_State *L)
{
	static const char *funcname = "local2global";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 local = luamtd_tovector_safe(L, 2, funcname);
	VECTOR3 global;
	v->Local2Global(local, global);
	lua_pushvector(L, global);
	return 1;
}

/***
Perform a transformation from global to local vessel coordinates.

This is the inverse transform of @{local2global}. It maps
   a point from global ecliptic coordinates into the vessel's local
   frame.

The transformation has the form
   \f[ \vec{p}_l = \mathsf{R}_v^{-1} (\vec{p}_g - \vec{p}_v) \f]
   where R<sub>v</sub> is the vessel's global rotation matrix
   (as given by @{get_rotationmatrix}), and \f$\vec{p}_v\f$ is the
   vessel position in the global frame.

@function global2local
@tparam vector global point in global coordinates [<b>m</b>]
@return (<i><b>@{types.vector|vector}</b></i>) local transformed point in local vessel coordinates [<b>m</b>]
@see get_rotationmatrix, local2global
*/
int Interpreter::v_global2local (lua_State *L)
{
	static const char *funcname = "global2local";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 global = luamtd_tovector_safe(L, 2, funcname);
	VECTOR3 local;
	v->Global2Local(global, local);
	lua_pushvector(L, local);
	return 1;
}

/***
Perform a transformation from local vessel coordinates to the ecliptic frame
centered at the vessel's reference body.

This function maps a point from the vessel's local coordinate
system into an ecliptic system centered at the centre of mass of
the vessel's <i>gravity reference object</i> (the celestial body
that is currently being orbited).

A handle to the reference object can be obtained via
@{get_gravityref}. The reference object may change if the vessel
enters a different object's sphere of influence.

@function local2rel
@tparam vector local point in local vessel coordinates [<b>m</b>]
@return (<i><b>@{types.vector|vector}</b></i>) rel transformed point in reference body-relative ecliptic
   coordinates [<b>m</b>].
@see get_rotationmatrix, global2local, local2global, get_gravityref
*/
int Interpreter::v_local2rel (lua_State *L)
{
	static const char *funcname = "local2rel";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 local = luamtd_tovector_safe(L, 2, funcname);
	VECTOR3 rel;
	v->Local2Rel(local, rel);
	lua_pushvector(L, rel);
	return 1;
}

/***
Pass a line read from a scenario file to Orbiter for default processing.

This function should be used within the body of clbk_loadstateex.

The parser clbk_loadstateex should forward all lines not recognised
by the module to Orbiter via parse\_scenario\_line\_ex to allow processing of
standard vessel settings.

clbk\_loadstateex currently provides a VESSELSTATUS2 status definition.
This may change in future versions, so status should not be used within
clbk\_loadstateex other than passing it to parse\_scenario\_line\_ex.

@function v_parse_scenario_line_ex
@tparam line string obtain via oapi\_readscenario\_nextline
@tparam status status obtained from the clbk\_loadstateex callback
@usage
function clbk_loadstateex(scn, vs)
   for line in scenario_lines(scn) do
      local met = line:match("MET (%S+)")
      if met ~= nil then
         ...
      else
         vi:parse_scenario_line_ex(line, vs)
      end
   end
end

*/
int Interpreter::v_parse_scenario_line_ex(lua_State* L)
{
	static const char* funcname = "parse_scenario_line_ex";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	const char* line = lua_tostring_safe(L, 2, funcname);
	void* status = lua_touserdata(L, 3);
	v->ParseScenarioLineEx(const_cast<char*>(line), status);
	return 0;
}

/***
Record an event.

Writes a custom tag to the vessel's articulation data stream during
a running recording session.

This function can be used to record custom vessel events (e.g.
animations) to the articulation stream (.atc) of a vessel record.

The function does nothing if no recording is active, so it is not
necessary to check for a running recording before invoking RecordEvent.

To read the recorded articulation tags during the playback of a
recorded session, overload the clbk\_playbackevent callback
function.

@function record_event
@tparam string event\_type event tag label
@tparam string event event string
*/
int Interpreter::v_record_event(lua_State* L)
{
	static const char* funcname = "record_event";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	const char* event_type = lua_tostring_safe(L, 2, funcname);
	const char* event = lua_tostring_safe(L, 3, funcname);
	v->RecordEvent(event_type, event);
	return 0;
}

/***
Playback session.

@function playback
@treturn boolean if the current session is a playback of a recorded flight, false otherwise.
*/
int Interpreter::v_playback(lua_State* L)
{
	static const char* funcname = "playback";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	bool pb = v->Playback();
	lua_pushboolean(L, pb);
	return 1;
}

/***
Panels
@section 2dpanels
*/

/***
Set scaling factors for 2-D instrument panel.

The scaling factors define the scaling between mesh coordinates
and screen pixels.

defscale is the default factor, extscale is an additional scale
which can be selected by the user via the mouse wheel.

Examples: scale=1: one mesh unit corresponds to one screen pixel,
scale=viewW/panelW: panel fits screen width

@function set_panelscaling
@tparam handle hPanel panel handle
@tparam number defscale default scale factor
@tparam number extscale additional scale factor
*/
int Interpreter::v_set_panelscaling(lua_State* L)
{
	static const char* funcname = "set_panelscaling";
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	if (v->Version() < 2) {
		lua_pushnil(L);
		lua_pushstring(L, "Invalid vessel version in set_panelscaling");
		return 2;
	}
	VESSEL3* v3 = (VESSEL3*)v;

	PANELHANDLE hPanel = lua_touserdata(L, 2);
	double defscale = luaL_checknumber(L, 3);
	double extscale = luaL_checknumber(L, 4);
	
	v3->SetPanelScaling(hPanel, defscale, extscale);
	return 0;
}

/***
Set the background surface for a 2-D instrument panel.

This method should be applied in the body of clbk_loadpanel2d.

The mesh defines the size and layout of the billboard mesh used for
rendering the panel surface. Its vertex coordinates are interpreted as
transformed, i.e. in terms of screen coordinates (pixels). The z-coordinate
should be zero. Normals are ignored. Texture coordinates define which part
of the surfaces are rendered.

The groups are rendered in the order they appear in the mesh. Later
groups cover earlier ones. Therefore the groups should be arranged from
backmost to frontmost elements.

In the simplest case, the mesh consists of a single rectangular area
(4 nodes, 2 triangles) and a single surface, but can be more elaborate.

The texture indices of the mesh groups (TexIdx) are interpreted as
indices into the hSurf list (zero-based).

This method increases the reference counters for the surfaces, so the
caller should release them at some point.

The surfaces can contain an alpha channel to handle transparency.

@function set_panelbackground
@tparam handle hPanel panel handle
@tparam table  hSurf array of surface handles
@tparam handle hMesh mesh handle defining the billboard geometry
@tparam number width panel width [pixel]
@tparam number height panel width [pixel]
@tparam number baseline base line for edge attachment
@tparam number scrollflag panel attachment and scrolling bitflags
*/
int Interpreter::v_set_panelbackground(lua_State* L)
{
	static const char* funcname = "set_panelbackground";
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	if (v->Version() < 2) {
		lua_pushnil(L);
		lua_pushstring(L, "Invalid vessel version in set_panelbackground");
		return 2;
	}
	PANELHANDLE hPanel = lua_touserdata(L, 2);
	SURFHANDLE *hSurf = NULL;  //FIXME: implement textures
	int nsurf = 0;
	if(!lua_isnil(L, 3)) {
		luaL_error(L, "FIXME: texture not supported in set_panelbackground");
	}
	
	MESHHANDLE hMesh = lua_tomeshhandle(L, 4);
	DWORD width = luaL_checkinteger(L, 5);
	DWORD height = luaL_checkinteger(L, 6);
	DWORD baseline =  luaL_checkinteger(L, 7);
	DWORD scrollflag =  luaL_checkinteger(L, 8);
	VESSEL3* v3 = (VESSEL3*)v;
	v3->SetPanelBackground(hPanel, hSurf, nsurf, hMesh, width, height, baseline, scrollflag);
	return 0;
}

/***
Register an area of the panel to receive mouse and redraw events.

This version passes the provided surface handle directly to the redraw
callback, rather making a copy of the area. This is useful if the area
either doesn't need to modify any surfaces, or blits parts of the same
surface (e.g. a texture that contains both the panel background and various
elements (switches, dials, etc.) to be copied on top.

Since the surface returned to the redraw function is not restricted to
the registered area, it is the responsibility of the caller not to draw
outside the area.

The area boundaries defined in pos are only used for generating
mouse events. If the area does not process mouse events (PANEL\_MOUSE.IGNORE),
the pos parameter is ignored.

The PANEL\_REDRAW.SKETCHPAD flags can not be used in
the draw_event parameter. If Sketchpad access is required during
redraw events, either the surface surf must have been created with the
appropriate attributes, or another version of register_panelarea must be used.

@function register_panelarea
@tparam handle hPanel panel handle
@tparam number id area identifier
@tparam table pos area boundary coordinates (mesh coordinates, table with left, top, right and bottom fields)
@tparam number draw_event event flags for redraw event triggers
@tparam number mouse_event event flags for mouse event triggers
@tparam handle surf surface handle passed to the redraw callback function
@tparam object context user-defined data passed to the mouse and redraw callback functions
*/
/***
Register an area of the panel to receive mouse and redraw events (deprecated).

This method activates a rectangular area of the panel for receiving mouse
and redraw events.

pos specifies the borders of the area in 'logical' coordinates
(0,0,width,height) as specified by set_panelbackground. Registered mouse
events within this area will trigger a call to clbk_panelmouseevent.

If the area needs to be able to update the panel texture, it should pass
an appropriate redraw flag in draw_event, and specify the texture coordinates
of the redraw area in texpos.

If the panel contains multiple background textures, only the first texture
can be redrawn with this function. To redraw other textures in the background
texture array, use another version of register_panelarea instead.

For backward compatibility, this method automatically adds the
PANEL\_REDRAW.SKETCHPAD flags to draw_event. If Sketchpad access to the area drawing surface
is not required, using another version of register_panelarea can improve graphics performance.

@function register_panelarea
@tparam handle hPanel panel handle
@tparam number id area identifier
@tparam table pos area boundary coordinates (mesh coordinates, table with left, top, right and bottom fields)
@tparam table texpos area boundary (texture coordinates, table with left, top, right and bottom fields)
@tparam number draw_event event flags for redraw event triggers
@tparam number mouse_event event flags for mouse event triggers
@tparam number bkmode flag for texture background provided to redraw callback function

*/
/***
Register an area of the panel to receive mouse and redraw events.

This method activates a rectangular area of the panel for receiving mouse
and redraw events.

pos specifies the borders of the area in 'logical' coordinates
(0,0,width,height) as specified by set\_panelbackground. Registered mouse
events within this area will trigger a call to clbk\_panelmouseevent.

texidx is the index of the panel background texture the area texture should
be copied into, in the order the textures were specified in the array passed to
set_panelbackground. If only a single texture is used for the panel,
texidx should be set to 0. If the area doesn't need to be redrawn
(PANEL\_REDRAW.NEVER), this parameter is ignored.

If the area texture should allow Sketchpad access during redraw
events, the PANEL\_REDRAW.SKETCHPAD flags should be added
to draw_event. If only blitting access is required, these flags should be omitted
for improved performance.
	 
@function register_panelarea
@tparam handle hPanel panel handle
@tparam number id area identifier
@tparam table pos area boundary coordinates (mesh coordinates, table with left, top, right and bottom fields)
@tparam number texidx background texture index
@tparam table texpos area boundary (texture coordinates, table with left, top, right and bottom fields)
@tparam number draw_event event flags for redraw event triggers
@tparam number mouse_event event flags for mouse event triggers
@tparam number bkmode flag for texture background provided to redraw callback function

*/
int Interpreter::v_register_panelarea(lua_State* L)
{
	static const char* funcname = "register_panelarea";
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	if (v->Version() < 3) {
		lua_pushnil(L);
		lua_pushstring(L, "Invalid vessel version in register_panelarea");
		return 2;
	}
	VESSEL4* v4 = (VESSEL4*)v;

	PANELHANDLE hPanel = lua_touserdata(L, 2);
	int id = luaL_checkinteger(L, 3);
	RECT pos = lua_torect(L, 4);

	if(lua_istable(L, 5)) {
		//int RegisterPanelArea (PANELHANDLE hPanel, int id, const RECT &pos, const RECT &texpos, int draw_event, int mouse_event, int bkmode);
		RECT texpos = lua_torect(L, 5);
		int draw_event = luaL_checkinteger(L, 6);
		int mouse_event = luaL_checkinteger(L, 7);
		int bkmode = luaL_checkinteger(L, 8);
		v4->RegisterPanelArea (hPanel, id, pos, texpos, draw_event, mouse_event, bkmode);
	} else {
		if(lua_istable(L, 6)) {
			//int RegisterPanelArea (PANELHANDLE hPanel, int id, const RECT &pos, int texidx, const RECT &texpos, int draw_event, int mouse_event, int bkmode);
			int texidx = luaL_checkinteger(L, 5);
			RECT texpos = lua_torect(L, 6);
			int draw_event = luaL_checkinteger(L, 7);
			int mouse_event = luaL_checkinteger(L, 8);
			int bkmode = luaL_checkinteger(L, 9);
			v4->RegisterPanelArea(hPanel, id, pos, texidx, texpos, draw_event, mouse_event, bkmode);
		} else {
			//int RegisterPanelArea (PANELHANDLE hPanel, int id, const RECT &pos, int draw_event, int mouse_event, SURFHANDLE surf = NULL, void *context = NULL);
			int draw_event = luaL_checkinteger(L, 5);
			int mouse_event = luaL_checkinteger(L, 6);

			SURFHANDLE surf = NULL;
			if(lua_gettop(L) >= 7) {
				surf = lua_touserdata(L, 7);
			}
			
			void *context = NULL;
			if(lua_gettop(L) >= 8) {
				lua_pushvalue(L, 8);
				context = (void *)(ptrdiff_t)(luaL_ref(L, LUA_REGISTRYINDEX));
			}
			v4->RegisterPanelArea(hPanel, id, pos, draw_event, mouse_event, surf, context);
		}
	}


	lua_pushnumber(L, 0);
	return 1;
}

/***
Define an MFD display in the panel mesh.

This method reserves a mesh group for rendering the contents of
an MFD display. The group should define a square area (typically
consisting of 4 nodes and 2 triangles) with appropriate texture
coordinates. When rendering the panel, the texture for this group is
set to the current contents of the MFD display.

The order of mesh groups defines the rendering order. To render
the MFD display on top of the panel, define it as the last group in
the mesh. Alternatively, the MFD can be rendered first, if the panel
texture contains a transparent area through which to view the MFD.

@function register_panelmfdgeometry
@tparam handle hPanel panel handle
@tparam number MFD_id MFD identifier (>= 0)
@tparam number nmesh panel mesh index (>= 0)
@tparam number ngroup mesh group index (>= 0)
*/
int Interpreter::v_register_panelmfdgeometry (lua_State *L)
{
	static const char* funcname = "register_panelmfdgeometry";
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	if (v->Version() < 2) {
		lua_pushnil(L);
		lua_pushstring(L, "Invalid vessel version in register_panelmfdgeometry");
		return 2;
	}
	VESSEL3* v3 = (VESSEL3*)v;

	PANELHANDLE hPanel = lua_touserdata(L, 2);
	int id = luaL_checkinteger(L, 3);
	int nmesh = luaL_checkinteger(L, 4);
	int ngroup = luaL_checkinteger(L, 5);

	v3->RegisterPanelMFDGeometry(hPanel, id, nmesh, ngroup);
	return 0;
}

/***
Exhaust rendering
@section vessel_mtd_exhaust
*/

/***
Add an exhaust render definition for a thruster (automatic position).

Thrusters defined with @{create_thruster} do not by default render exhaust effects,
until an exhaust definition has been specified with add_exhaust.

The size of the exhaust flame is automatically scaled by the thrust level.

If the exhaust position _pos_ and direction _dir_ are not specified explicitly,
they are obtained directly from the thruster setting, and automatically reflect any
changes caused by @{set_thrusterpos} and @{set_thrusterdir}.

It is also possible to add a longitudinal offset to the exhaust position, relative
to the associated thruster position.

If no explicit exhaust texture is specified, the default texture is used.

@function add_exhaust
@tparam handle hThrust thruster handle
@tparam number lscale exhaust flame length [m]
@tparam number wscale exhaust flame width [m]
@tparam[opt=0] number lofs longitudinal offset of exhaust position from thruster [m]
@tparam[opt] handle hTex texture handle for custom exhaust flames
@treturn int integer exhaust identifier (&ge; 0)
@see vessel:create_thruster, vessel:set_thrusterpos, vessel:set_thrusterdir, vessel:set_thrusterlevel
*/
/***
Add an exhaust render definition for a thruster (manual position).

This version uses a user-defined position and direction for the exhaust.

@function add_exhaust
@tparam handle hThrust thruster handle
@tparam number lscale exhaust flame length [m]
@tparam number wscale exhaust flame width [m]
@param pos (<i><b>@{types.vector|vector}</b></i>) exhaust source position in vessel frame [<b>m</b>]
@param dir (<i><b>@{types.vector|vector}</b></i>) exhaust direction in vessel frame
@tparam[opt] handle hTex texture handle for custom exhaust flames
@treturn int integer exhaust identifier (&ge; 0)
@see vessel:create_thruster, vessel:set_thrusterpos, vessel:set_thrusterdir, vessel:set_thrusterlevel
*/
int Interpreter::v_add_exhaust (lua_State *L)
{
	static const char *funcname = "add_exhaust";
	AssertMtdMinPrmCount(L, 4, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	double lscale = luamtd_tonumber_safe(L, 3, funcname);
	double wscale = luamtd_tonumber_safe(L, 4, funcname);
	int idx = 5;
	VECTOR3 pos, dir;
	double lofs;
	SURFHANDLE tex = 0;
	bool do_posdir = false;
	bool do_lofs = false;

	if (lua_gettop(L) >= idx && lua_isvector(L, idx)) { // explicit position and direction arguments
		pos = lua_tovector(L, idx++);
		AssertMtdMinPrmCount(L, idx, funcname);
		dir = luamtd_tovector_safe(L, idx++, funcname);
		do_posdir = true;
	} else if (lua_gettop(L) >= idx && lua_isnumber(L, idx)) {
		lofs = lua_tonumber(L, idx++);
		do_lofs = true;
	}
	if (lua_gettop(L) >= idx)
		tex = (SURFHANDLE)luamtd_tolightuserdata_safe(L, idx++, funcname);

	UINT exh;
	if (do_posdir)
		exh = v->AddExhaust (ht, lscale, wscale, pos, dir, tex);
	else if (do_lofs)
		exh = v->AddExhaust (ht, lscale, wscale, lofs, tex);
	else
		exh = v->AddExhaust (ht, lscale, wscale, tex);
	lua_pushnumber(L,(lua_Number)exh);
	return 1;
}

/***
Remove an exhaust render definition.

@function del_exhaust
@tparam int idx exhaust identifier (&ge; 0)
@treturn bool _false_ if exhaust definition does not exist, _true_ otherwise.
@see vessel:add_exhaust, vessel:get_exhaustcount
*/
int Interpreter::v_del_exhaust (lua_State *L)
{
	static const char *funcname = "del_exhaust";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	UINT idx = (UINT)luamtd_tointeger_safe(L, 2, funcname);
	bool ok = v->DelExhaust (idx);
	lua_pushboolean (L,ok);
	return 1;
}

/***
Get number of exhaust render definitions for the vessel.

@function get_exhaustcount
@treturn int number of exhaust render definitions
@see vessel:add_exhaust, vessel:del_exhaust
*/
int Interpreter::v_get_exhaustcount (lua_State *L)
{
	static const char *funcname = "get_exhaustcount";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	DWORD count = v->GetExhaustCount();
	lua_pushnumber (L, count);
	return 1;
}

/***
Add an exhaust particle stream to a vessel.

Exhaust streams can be emissive (to simulate "glowing" ionised gases)
or diffuse (e.g. for simulating vapour trails).

Multiple streams can be defined for a single engine. For example, an
emissive stream with short lifetime may represent the ionised exhaust
gases, while a diffuse stream with longer lifetime represents the vapour
trail.

If the source position _pos_ is not specified, the particles are created
at the thruster reference position.

To improve performance, closely packed engines may share a single exhaust
stream.

If the user has disabled particle streams in the launchpad dialog, this
function will return nil. The module must be able to cope with this case.

@function add_exhauststream
@tparam handle hThrust thruster handle
@param[opt] pos (<i><b>@{types.vector|vector}</b></i>) exhaust source position [<b>m</b>]
@tparam table pss particle stream spec structure (see @{types.PARTICLESTREAMSPEC|PARTICLESTREAMSPEC})
@treturn handle particle stream handle
@see vessel:add_exhaust
*/
int Interpreter::v_add_exhauststream(lua_State* L)
{
	static const char* funcname = "add_exhauststream";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	THRUSTER_HANDLE ht = (THRUSTER_HANDLE)luamtd_tolightuserdata_safe(L, 2, funcname);
	PARTICLESTREAMSPEC pss;  memset(&pss, 0, sizeof(PARTICLESTREAMSPEC));
	VECTOR3 pos;
	bool do_pos = false;
	int idx = 3;
	if (lua_isvector(L, idx)) {
		pos = lua_tovector(L, idx++);
		do_pos = true;
		AssertMtdMinPrmCount(L, idx, funcname);
	}
	AssertMtdPrmType(L, idx, PRMTP_TABLE, funcname);

	lua_getfield(L, idx, "flags");
	pss.flags = (lua_isnumber(L, -1) ? (DWORD)(lua_tonumber(L, -1) + 0.5) : 0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "srcsize");
	pss.srcsize = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 1.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "srcrate");
	pss.srcrate = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 1.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "v0");
	pss.v0 = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "srcspread");
	pss.srcspread = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "lifetime");
	pss.lifetime = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 10.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "growthrate");
	pss.growthrate = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "atmslowdown");
	pss.atmslowdown = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "ltype");
	pss.ltype = (lua_isnumber(L, -1) ? (PARTICLESTREAMSPEC::LTYPE)(int)(lua_tonumber(L, -1) + 0.5) : PARTICLESTREAMSPEC::DIFFUSE);
	lua_pop(L, 1);

	lua_getfield(L, idx, "levelmap");
	pss.levelmap = (lua_isnumber(L, -1) ? (PARTICLESTREAMSPEC::LEVELMAP)(int)(lua_tonumber(L, -1) + 0.5) : PARTICLESTREAMSPEC::LVL_LIN);
	lua_pop(L, 1);

	lua_getfield(L, idx, "lmin");
	pss.lmin = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "lmax");
	pss.lmax = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 1.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "atmsmap");
	pss.atmsmap = (lua_isnumber(L, -1) ? (PARTICLESTREAMSPEC::ATMSMAP)(int)(lua_tonumber(L, -1) + 0.5) : PARTICLESTREAMSPEC::ATM_FLAT);
	lua_pop(L, 1);

	lua_getfield(L, idx, "amin");
	pss.amin = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "amax");
	pss.amax = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 1.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "tex");
	pss.tex = (lua_islightuserdata(L, -1) ? (SURFHANDLE)lua_touserdata(L, -1) : NULL);
	lua_pop(L, 1);

	PSTREAM_HANDLE hp;
	if (do_pos) hp = v->AddExhaustStream(ht, pos, &pss);
	else        hp = v->AddExhaustStream(ht, &pss);
	if(hp) 
		lua_pushlightuserdata(L, hp);
	else
		lua_pushnil(L);
	return 1;
}

/***
Delete an existing particle stream.

If a thruster is deleted (with del_thruster), any attached
particle streams are deleted automatically.

A deleted particle stream will no longer emit particles, but
existing particles persist until they expire.

@function del_exhauststream
@tparam handle ch particle stream handle
@treturn boolean false indicates failure (particle stream not found)
*/
int Interpreter::v_del_exhauststream(lua_State* L)
{
	static const char* funcname = "del_exhauststream";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	PSTREAM_HANDLE hp = lua_tolightuserdata_safe(L, 2, "del_exhauststream");

	// remove reference to numberref
	lua_pushlightuserdata(L, hp);
    lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);

	bool ret = v->DelExhaustStream(hp);
	lua_pushboolean(L, ret);
	return 1;
}

/***
Add a reentry particle stream to a vessel.

Vessels automatically define a default emissive particle stream,
but you may want to add further stream to customise the appearance.

@function add_reentrystream
@tparam table pss particle stream specification (PARTICLESTREAMSPEC)
@treturn handle particle stream handle
*/
int Interpreter::v_add_reentrystream(lua_State* L)
{
	static const char* funcname = "add_reentrystream";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	PARTICLESTREAMSPEC pss;  memset(&pss, 0, sizeof(PARTICLESTREAMSPEC));
	int idx = 2;
	AssertMtdPrmType(L, idx, PRMTP_TABLE, funcname);

	lua_getfield(L, idx, "flags");
	pss.flags = (lua_isnumber(L, -1) ? (DWORD)(lua_tonumber(L, -1) + 0.5) : 0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "srcsize");
	pss.srcsize = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 1.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "srcrate");
	pss.srcrate = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 1.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "v0");
	pss.v0 = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "srcspread");
	pss.srcspread = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "lifetime");
	pss.lifetime = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 10.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "growthrate");
	pss.growthrate = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "atmslowdown");
	pss.atmslowdown = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "ltype");
	pss.ltype = (lua_isnumber(L, -1) ? (PARTICLESTREAMSPEC::LTYPE)(int)(lua_tonumber(L, -1) + 0.5) : PARTICLESTREAMSPEC::DIFFUSE);
	lua_pop(L, 1);

	lua_getfield(L, idx, "levelmap");
	pss.levelmap = (lua_isnumber(L, -1) ? (PARTICLESTREAMSPEC::LEVELMAP)(int)(lua_tonumber(L, -1) + 0.5) : PARTICLESTREAMSPEC::LVL_LIN);
	lua_pop(L, 1);

	lua_getfield(L, idx, "lmin");
	pss.lmin = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "lmax");
	pss.lmax = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 1.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "atmsmap");
	pss.atmsmap = (lua_isnumber(L, -1) ? (PARTICLESTREAMSPEC::ATMSMAP)(int)(lua_tonumber(L, -1) + 0.5) : PARTICLESTREAMSPEC::ATM_FLAT);
	lua_pop(L, 1);

	lua_getfield(L, idx, "amin");
	pss.amin = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "amax");
	pss.amax = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 1.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "tex");
	pss.tex = (lua_islightuserdata(L, -1) ? (SURFHANDLE)lua_touserdata(L, -1) : NULL);
	lua_pop(L, 1);

	PSTREAM_HANDLE hp;
	hp = v->AddReentryStream(&pss);
	if(hp)
		lua_pushlightuserdata(L, hp);
	else
		lua_pushnil(L);

	return 1;
}

/***
Add custom particle stream to a vessel.

This function can be used to add venting effects and similar.
For engine-specific effects such as exhaust and contrails, use the
add_exhauststream functions instead.

The PARTICLESTREAMSPEC structure defined the properties of
the particle stream.

The position and direction variables are in vessel-relative
coordinates. They cannot be redefined.

The returned level factor numberref can be used to define the strength
of the particle emission. Its value should be set in the range from 0
(particle generation off) to 1 (emission at full strength). It can
be changed continuously to modulate the particle generation.

@function add_particlestream
@tparam table pss particle stream specification (PARTICLESTREAMSPEC)
@tparam vector pos particle source position in vessel coordinates [m]
@tparam vector dir particle emission direction in vessel coordinates
@treturn handle particle stream handle
@treturn numberref particle level factor
*/
int Interpreter::v_add_particlestream(lua_State* L)
{
	static const char* funcname = "add_particlestream";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	PARTICLESTREAMSPEC pss;  memset(&pss, 0, sizeof(PARTICLESTREAMSPEC));
	int idx = 2;
	AssertMtdPrmType(L, idx, PRMTP_TABLE, funcname);

	lua_getfield(L, idx, "flags");
	pss.flags = (lua_isnumber(L, -1) ? (DWORD)(lua_tonumber(L, -1) + 0.5) : 0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "srcsize");
	pss.srcsize = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 1.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "srcrate");
	pss.srcrate = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 1.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "v0");
	pss.v0 = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "srcspread");
	pss.srcspread = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "lifetime");
	pss.lifetime = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 10.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "growthrate");
	pss.growthrate = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "atmslowdown");
	pss.atmslowdown = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "ltype");
	pss.ltype = (lua_isnumber(L, -1) ? (PARTICLESTREAMSPEC::LTYPE)(int)(lua_tonumber(L, -1) + 0.5) : PARTICLESTREAMSPEC::DIFFUSE);
	lua_pop(L, 1);

	lua_getfield(L, idx, "levelmap");
	pss.levelmap = (lua_isnumber(L, -1) ? (PARTICLESTREAMSPEC::LEVELMAP)(int)(lua_tonumber(L, -1) + 0.5) : PARTICLESTREAMSPEC::LVL_LIN);
	lua_pop(L, 1);

	lua_getfield(L, idx, "lmin");
	pss.lmin = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "lmax");
	pss.lmax = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 1.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "atmsmap");
	pss.atmsmap = (lua_isnumber(L, -1) ? (PARTICLESTREAMSPEC::ATMSMAP)(int)(lua_tonumber(L, -1) + 0.5) : PARTICLESTREAMSPEC::ATM_FLAT);
	lua_pop(L, 1);

	lua_getfield(L, idx, "amin");
	pss.amin = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "amax");
	pss.amax = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 1.0);
	lua_pop(L, 1);

	lua_getfield(L, idx, "tex");
	pss.tex = (lua_islightuserdata(L, -1) ? (SURFHANDLE)lua_touserdata(L, -1) : NULL);
	lua_pop(L, 1);

	VECTOR3 pos = lua_tovector_safe(L, 3, "add_particlestream");
	VECTOR3 dir = lua_tovector_safe(L, 4, "add_particlestream");

	// AddParticleStream's last argument must be a pointer to a double whose lifetime must be compatible with the particle stream itself
	// We create a "numberref" object to do that

	lua_pushnumberref(L);
	double* lvl = (double*)lua_touserdata(L, -1);
	*lvl = lua_tonumber(L, 5);

	PSTREAM_HANDLE hp = v->AddParticleStream(&pss, pos, dir , lvl);
	if(!hp) {
		lua_pushnil(L);
		return 1;
	}

	// Add the numberref in the registry to prevent its collection if the script does not recover it
	// Use the PSTREAM_HANDLE as the key so we can remove it when deleting the stream
	lua_pushlightuserdata(L, hp);  /* push address */
    lua_pushvalue(L, -2);
    lua_settable(L, LUA_REGISTRYINDEX);

	lua_pushlightuserdata(L, hp);
	lua_insert(L, -2); // swap the 2 top elements (numberref & handle)
	return 2;
}


/***
Camera functions
@section vessel_mtd_camera
*/

/***
Get current camera position for internal (cockpit) view.

@function get_cameraoffset
@return (<i><b>@{types.vector|vector}</b></i>) camera offset in vessel coordinates [<b>m</b>]
@see vessel:set_cameraoffset
*/
int Interpreter::v_get_cameraoffset (lua_State *L)
{
	static const char *funcname = "get_cameraoffset";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 ofs;
	v->GetCameraOffset (ofs);
	lua_pushvector (L, ofs);
	return 1;
}

/***
Set camera position for internal (cockpit) view.

The camera offset can be used to define the pilot's eye position in the spacecraft.
The default offset is (0,0,0).

@function set_cameraoffset
@param ofs (<i><b>@{types.vector|vector}</b></i>) camera offset in vessel coordinates [<b>m</b>]
@see vessel:get_cameraoffset
*/
int Interpreter::v_set_cameraoffset (lua_State *L)
{
	static const char *funcname = "set_cameraoffset";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 ofs = luamtd_tovector_safe(L, 2, funcname);
	v->SetCameraOffset (ofs);
	return 0;
}

/***
Set default camera direction and tilt angle for internal (cockpit) view.

This function allows to set the camera tilt angle in addition to the default
direction.

By default, the default direction is (0,0,1), i.e. forward, and the tilt
angle is 0 (upright).

The supplied direction vector must be normalised to length 1.

The tilt angle should be in the range [-Pi,+Pi]

Calling this function automatically sets the current actual view direction to
the default direction.

@function set_cameradefaultdirection
@tparam vector cd new default direction in vessel coordinates
@tparam number tilt camera tilt angle around the default direction [rad]
@see get_cameradefaultdirection
*/
/***
Set default camera direction for internal (cockpit) view.

By default, the default direction is (0,0,1), i.e. forward.

The supplied direction vector must be normalised to length 1.

Calling this function automatically sets the current actual view
direction to the default direction.

This function can either be called during clbk\_setclasscaps,
to define the default camera direction globally for the vessel, or during
clbk\_loadgenericcockpit, clbk\_loadpanel2d and clbk\_loadVC,
to define different default directions for different instrument panels or
virtual cockpit positions.

In Orbiter, the user can return to the default direction by pressing the
Home key on the cursor key pad.

@function set_cameradefaultdirection
@tparam vector cd new default direction in vessel coordinates
@see get_cameradefaultdirection
*/
int Interpreter::v_set_cameradefaultdirection (lua_State *L)
{
	static const char *funcname = "set_cameradefaultdirection";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 cd = luamtd_tovector_safe(L, 2, funcname);
	if (lua_isnumber(L, 3)) {
		double tilt = luamtd_tonumber_safe(L, 3, funcname);
		v->SetCameraDefaultDirection(cd, tilt);
	} else {
		v->SetCameraDefaultDirection(cd);
	}
	return 0;
}

/***
Get default camera direction for internal (cockpit) view.

The default camera direction may change as a result of invoking
   SetCameraDefaultDirection, typically when the user selects a different
   instrument panel or virtual cockpit position.

The returned direction vector is normalised to length 1.

@function get_cameradefaultdirection
@return (<i><b>@{types.vector|vector}</b></i>) cd default camera direction in vessel coordinates
@see set_cameradefaultdirection
*/
int Interpreter::v_get_cameradefaultdirection (lua_State *L)
{
	static const char *funcname = "get_cameradefaultdirection";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 cd;
	v->GetCameraDefaultDirection(cd);
	lua_pushvector(L, cd);
	return 1;
}

/***
Set angle over which the cockpit camera auto-centers to default direction.

The cockpit camera auto-centers to its default ("forward") direction when
   it is close enough to this direction. This function can be used to specify the
   angle over which auto-centering occurs.

Setting cangle=0 disables the auto-centering function.

The default catchment angle is 5 degrees (5.0*RAD).

To reset the catchment angle globally for all cockpit views of the vessel,
   @{set_cameracatchangle} would typically used in VESSEL2::clbkSetClassCaps(). To reset
   the catchment angle for individual cockpit positions, the function would be used
   for the appropriate cockpit modes in clbk_loadpanel2d() and clbk_loadVC().

@function set_cameracatchangle
@tparam number cangle auto-center catchment angle [rad]
*/
int Interpreter::v_set_cameracatchangle (lua_State *L)
{
	static const char *funcname = "set_cameracatchangle";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double cangle = luamtd_tonumber_safe(L, 2, funcname);
	v->SetCameraCatchAngle(cangle);
	return 0;
}

/**
Set range over which the cockpit camera can be rotated from its default
direction.

The meaning of the "left", "right", "up" and "down" directions is given by the
   orientation of the local vessel frame. For a default view direction of (0,0,1),
   "left" is a rotation towards the -x axis, "right" is a rotation towards the +x axis,
   "up" is a rotation towards the +y axis, and "down" is a rotation towards the -y axis.

All ranges must be >= 0. The left and right ranges should be < Pi. The up and
   down ranges should be < Pi/2.

The default values are 0.8Pi for left and right ranges, and 0.4Pi for up and down
   ranges.

@function set_camerarotationrange
@tparam left rotation range to the left [rad]
@tparam right rotation range to the right [rad]
@tparam up rotation range up [rad]
@tparam down rotation range down [rad]
@see set_camerashiftrange, set_cameramovement
*/
int Interpreter::v_set_camerarotationrange (lua_State *L)
{
	static const char *funcname = "set_camerarotationrange";
	AssertMtdMinPrmCount(L, 5, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	double left  = luamtd_tonumber_safe(L, 2, funcname);
	double right = luamtd_tonumber_safe(L, 3, funcname);
	double up    = luamtd_tonumber_safe(L, 4, funcname);
	double down  = luamtd_tonumber_safe(L, 5, funcname);
	v->SetCameraRotationRange(left, right, up, down);
	return 0;
}

/**
Set linear movement range for the cockpit camera.

Defining a linear movement allows the user to move the head forward or sideways, e.g. to
   get a better look out of a window, or a closer view of a virtual cockpit instrument
   panel.

If a linear movement range is defined with this function, the user can 'lean'
   forward or sideways using the 'cockpit slew' keys. Supported keys are:
   <table col="3">
   <tr><td><b>Name</b></td><td><b>default</b></td><td><b>action</b></td></tr>
   <tr><td>CockpitCamDontLean</td><td>Ctrl+Alt+Down</td><td>return to default position</td></tr>
   <tr><td>CockpitCamLeanForward</td><td>Ctrl+Alt+Up</td><td>lean forward</td></tr>
   <tr><td>CockpitCamLeanLeft</td><td>Ctrl+Alt+Left</td><td>lean left</td></tr>
   <tr><td>CockpitCamLeanRight</td><td>Ctrl+Alt+Right</td><td>lean right</td></tr>
   </table>

The movement vectors are taken relative to the default cockpit position defined
   via set_cameraoffset.

This function should be called when initialising a cockpit mode (e.g. in
   clbkLoadPanel or clbkLoadVC). By default, Orbiter resets the linear movement range
   to zero whenever the cockpit mode changes.

In addition to the linear movement, the camera also turns left when leaning left,
   turns right when leaning right, and returns to default direction when leaning forward.
   For more control over camera rotation at the different positions, use set_cameramovement
   instead.

@function set_camerashiftrange
@tparam vector fpos offset vector when leaning forward [<b>m</b>]
@tparam vector lpos offset vector when leaning left [<b>m</b>]
@tparam vector rpos offset vector when leaning right [<b>m</b>]
@see set_cameramovement, set_camerarotationrange
*/
int Interpreter::v_set_camerashiftrange (lua_State *L)
{
	static const char *funcname = "set_camerashiftrange";
	AssertMtdMinPrmCount(L, 4, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 fpos = luamtd_tovector_safe(L, 2, funcname);
	VECTOR3 lpos = luamtd_tovector_safe(L, 3, funcname);
	VECTOR3 rpos = luamtd_tovector_safe(L, 4, funcname);
	v->SetCameraShiftRange(fpos, lpos, rpos);
	return 0;
}

/**
Set both linear movement range and orientation of the cockpit camera when
"leaning" forward, left and right.

This function is an extended version of @{set_camerashiftrange}.

It is more versatile, because in addition to the linear camera movement vectors, it
   also allows to define the camera orientation (via azimuth and polar angle relative to
   default view direction). This allows to point the camera to a particular cockpit window,
   instrument panel, etc.

@function set_cameramovement
@tparam fpos vector offset vector when leaning forward [<b>m</b>]
@tparam fphi number camera rotation azimuth angle when leaning forward [rad]
@tparam ftht number camera rotation polar angle when leaning forward [rad]
@tparam lpos vector offset vector when leaning left [<b>m</b>]
@tparam lphi number camera rotation azimuth angle when leaning left [rad]
@tparam ltht number camera rotation polar angle when leaning left [rad]
@tparam rpos vector offset vector when leaning right [<b>m</b>]
@tparam rphi number camera rotation azimuth angle when leaning right [rad]
@tparam rtht number camera rotation polar angle when leaning right [rad]
@see set_camerashiftrange, set_camerarotationrange
*/
int Interpreter::v_set_cameramovement (lua_State *L)
{
	static const char *funcname = "set_cameramovement";
	AssertMtdMinPrmCount(L, 10, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	VECTOR3 fpos, lpos, rpos;
	double fphi, ftht, lphi, ltht, rphi, rtht;

	fpos = luamtd_tovector_safe(L, 2, funcname);
	fphi = luamtd_tonumber_safe(L, 3, funcname);
	ftht = luamtd_tonumber_safe(L, 4, funcname);

	lpos = luamtd_tovector_safe(L, 5, funcname);
	lphi = luamtd_tonumber_safe(L, 6, funcname);
	ltht = luamtd_tonumber_safe(L, 7, funcname);

	rpos = luamtd_tovector_safe(L, 8, funcname);
	rphi = luamtd_tonumber_safe(L, 9, funcname);
	rtht = luamtd_tonumber_safe(L, 10, funcname);

	v->SetCameraMovement(fpos, fphi, ftht, lpos, lphi, ltht, rpos, rphi, rtht);
	return 0;
}


/***
Instrument panel and virtual cockpit methods
@section vessel_mtd_panel
*/

/***
Trigger redraw notification for a panel area.

The redraw notification is ignored if the requested panel is not currently
   displayed or if the calling vessel does not have the input focus.

@function trigger_panelredrawarea
@tparam int panel_id panel identifier (>=0)
@tparam int area_id area identifier (>=0)
@see trigger_redrawarea
*/
int Interpreter::v_trigger_panelredrawarea (lua_State *L)
{
	static const char *funcname = "trigger_panelredrawarea";
	AssertMtdMinPrmCount(L, 3, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	int panel_id = luamtd_tointeger_safe(L, 2, funcname);
	int area_id = luamtd_tointeger_safe(L, 3, funcname);
	v->TriggerPanelRedrawArea(panel_id, area_id);
	return 0;
}

/***
Trigger redraw notification to either a 2D panel or a virtual cockpit.

This function can be used to combine the functionality of the
   trigger_panelredrawarea() and VC_trigger_redrawarea() methods.
   Depending on the current cockpit mode, Orbiter sends the redraw request to
   either clbk_panelredrawevent() or clbk_VCredrawevent().

This method can only be used if the panel and virtual cockpit areas share a
   common area identifier.

If the calling vessel doesn't have input focus (and therefore doesn't own the
   cockpit display) this method has no effect.

@function trigger_redrawarea
@tparam int panel_id identifier for the panel to receive the redraw message
@tparam int vc_id identifier for the virtual cockpit to receive the redraw message
@tparam int area_id area identifier
@see trigger_panelredrawarea
*/
int Interpreter::v_trigger_redrawarea (lua_State *L)
{
	static const char *funcname = "trigger_redrawarea";
	AssertMtdMinPrmCount(L, 4, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	int panel_id = luamtd_tointeger_safe(L, 2, funcname);
	int vc_id = luamtd_tointeger_safe(L, 3, funcname);
	int area_id = luamtd_tointeger_safe(L, 4, funcname);
	v->TriggerRedrawArea(panel_id, vc_id, area_id);
	return 0;
}

// We create a VesselMFD, the lua side is in charge of overloading the metatable,
// then we save the lua object reference to be used when then core will call VesselMFD callbacks
OAPI_MSGTYPE Interpreter::MsgProcMFD(UINT msg, UINT mfd, WPARAM wparam, LPARAM lparam)
{
	switch (msg) {
		case OAPI_MSG_MFD_OPENEDEX:
		{
			MFDMODEOPENSPEC* ospec = (MFDMODEOPENSPEC*)wparam;
			DWORD w = ospec->w;
			DWORD h = ospec->h;
			VesselMFDContext* ctx = (VesselMFDContext*)ospec->spec->context;
			VESSEL* vessel = (VESSEL*)lparam;
			VesselMFD* vmfd = new VesselMFD(w, h, vessel, ctx);

			lua_State* L = ctx->L;

			lua_rawgeti(L, LUA_REGISTRYINDEX, ctx->msgproc);   // push the callback function
			lua_pushnumber(L, msg);
			lua_pushnumber(L, mfd);
			lua_pushnumber(L, w);
			lua_pushnumber(L, h);
			lua_pushvessel(L, vessel);
			lua_pushmfd(L, vmfd);

			if (lua_pcall(L,  6, 1, 0) != 0) {
				fprintf(stderr, "Error MsgProcMFD: %s\n", lua_tostring(L, -1));
				//return 0;
			} else {
				vmfd->mfd_ref = luaL_ref(L, LUA_REGISTRYINDEX);
			}
			return (OAPI_MSGTYPE)(vmfd);
		}
	}
	return 0;
}

/***
Register a user-defined MFD mode for the vessel.

This method registers the MFD mode only for an individual vessel instance.
This allows to create vessel-specific MFD modes directly in the vessel
module. Typically this method would be called in the vessel constructor.

spec is a table defining the parameters of the new mode:

- name: string (name of the new mode)
- key: number (mode selection key)
- msgproc function (MFD message parser)

The mode identifier retrieved by oapi.get_mfdmode() for MFD modes
registered by this method starts with 1000 for the first registered mode
and is incremented by 1 for each subsequently registered mode.

@function register_mfdmode
@tparam table spec MFD mode specifications
@treturn number mode identifier
@usage
function msgproc(msg, ...)
   if msg == OAPI_MSG.MFD_OPENEDEX then
      return APMFD(...)
   end
   return 0
end

function register_APMFD()
   local spec = {
      name = "AscentAP",
      key = OAPI_KEY.B,
      msgproc = msgproc
   }
   return vi:register_mfdmode(spec)
end

*/
int Interpreter::v_register_mfdmode(lua_State* L)
{
	static const char* funcname = "register_mfdmode";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	if (v->Version() < 3) {
		lua_pushnil(L);
		lua_pushstring(L, "Invalid vessel version in register_mfdmode");
		return 2;
	}
	VESSEL4* v4 = (VESSEL4*)v;

	MFDMODESPECEX spec;
	spec.msgproc = MsgProcMFD;
	lua_getfield(L, 2, "name");
	spec.name = const_cast<char *>(lua_tostring(L, -1));
	lua_getfield(L, 2, "key");
	spec.key = lua_tonumber(L, -1);
	lua_getfield(L, 2, "msgproc");
	VesselMFDContext *ctx = new VesselMFDContext;
	ctx->L = L;
	ctx->msgproc = luaL_ref(L, LUA_REGISTRYINDEX);
	spec.context = ctx;

	int mode = v4->RegisterMFDMode(spec);
	lua_pop(L, 2);
	lua_pushnumber(L, mode);
	return 1;
}

/***
Unregister a previously registered vessel-specific MFD mode.

@function unregister_mfdmode
@tparam number mode mode identifier, as returned by vessel:register_mfdmode()
@treturn boolean true on success (mode was successfully unregistered).
*/
int Interpreter::v_unregister_mfdmode(lua_State* L)
{
	static const char* funcname = "unregister_mfdmode";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	if (v->Version() < 3) {
		lua_pushnil(L);
		lua_pushstring(L, "Invalid vessel version in unregister_mfdmode");
		return 2;
	} else {
		VESSEL4* v4 = (VESSEL4*)v;
		int mode = luamtd_tointeger_safe(L, 2, funcname);
		//FIXME: The mode gives us no way to retrieve the context so we leak 16 bytes per unregistered MFD
		//       Should probably add a table in the lua_State indexed by mode to track the contexts
		bool res = v4->UnregisterMFDMode(mode);
		lua_pushboolean(L, res);
		return 1;
	}
}

/***
Beacons.
@section vessel_mtd_beacon
*/

/***
Add light beacon definition to a vessel.

@function add_beacon
@tparam beacon b beacon created via oapi.create_beacon
*/
int Interpreter::v_add_beacon(lua_State *L)
{
	static const char* funcname = "add_beacon";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	BEACONLIGHTSPEC_Lua* beacon = (BEACONLIGHTSPEC_Lua*)luaL_checkudata(L, 2, "Beacon.vtable");

	v->AddBeacon(&beacon->bs);
	beacon->vessel = v;

	return 0;
}

/***
Remove beacon definition from the vessel.

@function del_beacon
@tparam beacon b beacon created via oapi.create_beacon
*/
int Interpreter::v_del_beacon(lua_State *L)
{
	static const char* funcname = "del_beacon";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	BEACONLIGHTSPEC_Lua* beacon = (BEACONLIGHTSPEC_Lua*)luaL_checkudata(L, 2, "Beacon.vtable");

	bool ret = v->DelBeacon(&beacon->bs);
	if(ret)
		beacon->vessel = nullptr;

	lua_pushboolean(L, ret);

	return 1;
}

/***
Remove all beacon definitions from the vessel.

@function clear_beacons
*/
int Interpreter::v_clear_beacons(lua_State *L)
{
	static const char* funcname = "clear_beacons";
	AssertMtdMinPrmCount(L, 1, funcname);
	VESSEL* v = lua_tovessel_safe(L, 1, funcname);
	
	v->ClearBeacons();

	return 0;
}


/***
User interface
@section vessel_mtd_io
*/

/***
Send keycode message to the vessel.

The key codes correspond to the values for the OAPI\_KEY.xxx constants defined in OrbiterAPI.h.
A convenient way to pick a keycode is via the _ktable_ table. For example, ktable.A has value 0x1E,
which represents the keycode for A. Only a subset of keycodes is currently defined in the ktable
table.

The return value is 1 if the vessel could process the key message, or 0 otherwise.

@function send_bufferedkey
@tparam int keycode key identifier
@treturn int processing flag (1=key was processed, 0=key was ignored)
*/
int Interpreter::v_send_bufferedkey (lua_State *L)
{
	static const char *funcname = "send_bufferedkey";
	AssertMtdMinPrmCount(L, 2, funcname);
	VESSEL *v = lua_tovessel_safe(L, 1, funcname);
	int key = luamtd_tointeger_safe (L, 2, funcname);
	int res = v->SendBufferedKey (key);
	lua_pushnumber (L, res);
	return 1;
}


VesselMFD::VesselMFD(DWORD w, DWORD h, VESSEL* vessel, VesselMFDContext* ctx) : MFD2(w, h, vessel) {
	L = ctx->L;
	mfd_ref = LUA_REFNIL;
}

VesselMFD::~VesselMFD() {
	lua_rawgeti(L, LUA_REGISTRYINDEX, mfd_ref);
	lua_getfield(L, -1, "destroy");
	if (lua_isfunction(L, -1)) {
		lua_pushvalue(L, -2); //self
		Interpreter::LuaCall(L, 1, 0);
	}
}

bool VesselMFD::ConsumeButton(int bt, int event) {
	lua_rawgeti(L, LUA_REGISTRYINDEX, mfd_ref);
	lua_getfield(L, -1, "consumebutton");
	bool consumed = false;
	if (lua_isfunction(L, -1)) {
		lua_pushvalue(L, -2); //self
		lua_pushnumber(L, bt);
		lua_pushnumber(L, event);
		Interpreter::LuaCall(L, 3, 1);
		consumed = (lua_toboolean(L, -1) ? true : false);
	}
	else {
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	return consumed;
}
bool VesselMFD::ConsumeKeyBuffered(DWORD key) {
	lua_rawgeti(L, LUA_REGISTRYINDEX, mfd_ref);
	lua_getfield(L, -1, "consumekeybuffered");
	bool consumed = false;
	if (lua_isfunction(L, -1)) {
		lua_pushvalue(L, -2); //self
		lua_pushnumber(L, key);
		Interpreter::LuaCall(L, 2, 1);
		consumed = (lua_toboolean(L, -1) ? true : false);
	}
	else {
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	return consumed;
}
bool VesselMFD::ConsumeKeyImmediate(char* kstate) {
	lua_rawgeti(L, LUA_REGISTRYINDEX, mfd_ref);
	lua_getfield(L, -1, "consumekeyimmediate");
	bool consumed = false;
	if (lua_isfunction(L, -1)) {
		lua_pushvalue(L, -2); //self
		lua_pushlightuserdata(L, kstate);
		Interpreter::LuaCall(L, 2, 1);
		consumed = (lua_toboolean(L, -1) ? true : false);
	}
	else {
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	return consumed;
}
char* VesselMFD::ButtonLabel(int bt) {
	lua_rawgeti(L, LUA_REGISTRYINDEX, mfd_ref); //self
	lua_getfield(L, -1, "buttonlabel");
	char* label = NULL;
	if (lua_isfunction(L, -1)) {
		lua_pushvalue(L, -2); //self
		lua_pushnumber(L, bt);
		Interpreter::LuaCall(L, 2, 1);
		if (lua_isstring(L, -1)) {
			label = (char*)lua_tostring(L, -1);
		}
	}
	else {
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	return label;
}
int VesselMFD::ButtonMenu(const MFDBUTTONMENU** menu) const {
	int i, nbt = 0;
	lua_rawgeti(L, LUA_REGISTRYINDEX, mfd_ref);
	lua_getfield(L, -1, "buttonmenu");
	if (lua_isfunction(L, -1)) {
		static MFDBUTTONMENU* mnu = 0;
		static int nmnu = 0;
		lua_pushvalue(L, -2); //self
		Interpreter::LuaCall(L, 1, 2);
		if (lua_isnumber(L, -1)) {
			nbt = lua_tointeger(L, -1);
			if (menu) {
				if (nmnu) {
					for (i = 0; i < nmnu; i++) {
						if (mnu[i].line1) delete[]mnu[i].line1;
						if (mnu[i].line2) delete[]mnu[i].line2;
					}
					delete[]mnu;
					nmnu = 0;
				}
				if (nbt) {
					mnu = new MFDBUTTONMENU[nmnu = nbt];
					for (i = 0; i < nbt; i++) {
						mnu[i].line1 = 0;
						mnu[i].line2 = 0;
						mnu[i].selchar = 'x';
					}
					if (lua_istable(L, -2)) {
						for (i = 0; i < nbt; i++) {
							lua_pushnumber(L, i + 1);
							lua_gettable(L, -3);
							if (lua_istable(L, -1)) {
								lua_getfield(L, -1, "l1");
								if (lua_isstring(L, -1)) {
									const char* line = lua_tostring(L, -1);
									char* linebuf = new char[strlen(line) + 1];
									strcpy(linebuf, line);
									mnu[i].line1 = linebuf;
								}
								lua_pop(L, 1);
								lua_getfield(L, -1, "l2");
								if (lua_isstring(L, -1)) {
									const char* line = lua_tostring(L, -1);
									char* linebuf = new char[strlen(line) + 1];
									strcpy(linebuf, line);
									mnu[i].line2 = linebuf;
								}
								lua_pop(L, 1);
								lua_getfield(L, -1, "sel");
								if (lua_isstring(L, -1)) {
									const char* line = lua_tostring(L, -1);
									mnu[i].selchar = line[0];
								}
								lua_pop(L, 1);
							}
							lua_pop(L, 1);
						}
					}
				}
				*menu = mnu;
			}
		}
		lua_pop(L, 2);
	}
	else {
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	return nbt;
}
bool VesselMFD::Update(oapi::Sketchpad* skp) {
	lua_rawgeti(L, LUA_REGISTRYINDEX, mfd_ref);
	lua_getfield(L, -1, "update");

	if (lua_isfunction(L, -1)) {
		lua_pushvalue(L, -2); //self
		Interpreter::lua_pushsketchpad(L, skp);
		Interpreter::LuaCall(L, 2, 1);
		bool consumed = (lua_toboolean(L, -1) ? true : false);
		lua_pop(L, 1);
		return true; //consumed;
	}
	lua_pop(L, 1);
	return false;
}
void VesselMFD::StoreStatus() const {
	lua_rawgeti(L, LUA_REGISTRYINDEX, mfd_ref);
	lua_getfield(L, -1, "storestatus");

	if (lua_isfunction(L, -1)) {
		lua_pushvalue(L, -2); //self
		Interpreter::LuaCall(L, 1, 0);
	}
	lua_pop(L, 1);
}
void VesselMFD::RecallStatus() {
	lua_rawgeti(L, LUA_REGISTRYINDEX, mfd_ref);
	lua_getfield(L, -1, "recallstatus");

	if (lua_isfunction(L, -1)) {
		lua_pushvalue(L, -2); //self
		Interpreter::LuaCall(L, 1, 0);
	}
	lua_pop(L, 1);
}
void VesselMFD::WriteStatus(FILEHANDLE scn) const {
	lua_rawgeti(L, LUA_REGISTRYINDEX, mfd_ref);
	lua_getfield(L, -1, "writestatus");

	if (lua_isfunction(L, -1)) {
		lua_pushvalue(L, -2); //self
		lua_pushlightuserdata(L, scn);
		Interpreter::LuaCall(L, 2, 0);
	}
	lua_pop(L, 1);
}
void VesselMFD::ReadStatus(FILEHANDLE scn) {
	lua_rawgeti(L, LUA_REGISTRYINDEX, mfd_ref);
	lua_getfield(L, -1, "readstatus");

	if (lua_isfunction(L, -1)) {
		lua_pushvalue(L, -2); //self
		lua_pushlightuserdata(L, scn);
		Interpreter::LuaCall(L, 2, 0);
	}
	lua_pop(L, 1);
}

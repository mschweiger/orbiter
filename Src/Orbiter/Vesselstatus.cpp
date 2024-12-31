// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ==============================================================
// Contains the portion of the Vessel class definition
// concerned with VESSELCLASS interfaces
// ==============================================================

#include "Orbiter.h"
#include "Vessel.h"
#include "SuperVessel.h"
#include "Vvessel.h"
#include "Config.h"
#include "Pane.h"
#include "Element.h"
#include "Psys.h"
#include "Base.h"
#include "Util.h"
#include "Log.h"
#include <fstream>
#include <iomanip>
#include <stdio.h>
#include <stdlib.h>

using std::min;

extern Orbiter *g_pOrbiter;
extern TimeData td;
extern PlanetarySystem *g_psys;
extern char DBG_MSG[256];

// ==============================================================
// ParseScenarioLineX versions
// ==============================================================

// ==============================================================
// Read a line from a scenario file into a VESSELSTATUS interface

bool Vessel::ParseScenarioLine (char *line, VESSELSTATUS &vs)
{
	char cbuf[256], *pd, c;
	DWORD n;
	double lvl;

	if (!_strnicmp (line, "STATUS", 6)) {
		line = trim_string (line+6);
		if (!_strnicmp (line, "LANDED", 6)) {
			vs.rbody = (OBJHANDLE)g_psys->GetGravObj (trim_string (line+6));
			vs.status = 1;
			vs.vdata[0].z = 0.0f; // default when landed
		} else if (!_strnicmp (line, "ORBITING", 8)) {
			vs.rbody = (OBJHANDLE)g_psys->GetGravObj (trim_string (line+8));
			vs.status = 0;
		}
	} else if (!_strnicmp (line, "BASE", 4)) {
		line = trim_string (line+4);
		if (pd = strtok (line, ":")) {
			strcpy (cbuf, pd);
			// at this point we assume that vs.rbody has already been assigned,
			// i.e. that the STATUS LANDED line has already been parsed
			Base *base = ((Planet*)vs.rbody)->GetBase (trim_string(cbuf));
			vs.base = (OBJHANDLE)base;
			if (pd = strtok (NULL, ":")) {
				sscanf (pd, "%d", &vs.port);
				vs.port--;
				base->Pad_EquPos (vs.port, vs.vdata[0].x, vs.vdata[0].y);
				// place ship in centre of landing pad by default
			}
		}
	} else if (!_strnicmp (line, "POS", 3)) {
		sscanf (line+3, "%lf%lf", &vs.vdata[0].x, &vs.vdata[0].y);
		vs.vdata[0].x *= RAD;
		vs.vdata[0].y *= RAD;
	} else if (!_strnicmp (line, "HEADING", 7)) {
		sscanf (line+7, "%lf", &vs.vdata[0].z);
		vs.vdata[0].z *= RAD;
	} else if (!_strnicmp (line, "RPOS", 4)) {
		sscanf (line+4, "%lf%lf%lf", &vs.rpos.x, &vs.rpos.y, &vs.rpos.z);
	} else if (!_strnicmp (line, "RVEL", 4)) {
		sscanf (line+4, "%lf%lf%lf", &vs.rvel.x, &vs.rvel.y, &vs.rvel.z);
	} else if (!_strnicmp (line, "ELEMENTS", 8)) {
		double a, e, i, theta, omegab, L, elmjd;
		Vector rpos, rvel;
		sscanf (line+8, "%lf%lf%lf%lf%lf%lf%lf",  &a, &e, &i, &theta, &omegab, &L, &elmjd);
		if (vs.rbody) {
			el->Set (a, e, i*RAD, theta*RAD, omegab*RAD, L*RAD, elmjd);
			el->Setup (mass, ((Body*)vs.rbody)->Mass(), td.MJD_ref);
			el->Update (rpos, rvel);
			vs.rpos.x = rpos.x, vs.rpos.y = rpos.y, vs.rpos.z = rpos.z;
			vs.rvel.x = rvel.x, vs.rvel.y = rvel.y, vs.rvel.z = rvel.z;
			el_valid = true;
		}
	} else if (!_strnicmp (line, "AROT", 4)) {
		sscanf (line+4, "%lf%lf%lf", &vs.arot.x, &vs.arot.y, &vs.arot.z);
		vs.arot.x *= RAD, vs.arot.y *= RAD, vs.arot.z *= RAD;
	} else if (!_strnicmp (line, "VROT", 4)) {
		sscanf (line+4, "%lf%lf%lf", &vs.vrot.x, &vs.vrot.y, &vs.vrot.z);
		vs.vrot.x *= RAD, vs.vrot.y *= RAD, vs.vrot.z *= RAD;
	} else if (!_strnicmp (line, "FUEL", 4)) {  // old style propellant interface
		sscanf (line+4, "%lf", &vs.fuel);
	} else if (!_strnicmp (line, "PRPLEVEL", 8)) { // new style propellant interface
		for (pd = strtok (line+8, " "); pd; pd = strtok (NULL, " "))
			if (sscanf (pd, "%d%c%lf", &n, &c, &lvl) == 3)
				if (n < ntank && tank[n] == def_tank) vs.fuel = lvl;
	} else if (!_strnicmp (line, "THLEVEL", 7)) {
		for (pd = strtok (line+7, " "); pd; pd = strtok (NULL, " ")) {
			if (sscanf (pd, "%d%c%lf", &n, &c, &lvl) == 3 && n < m_thruster.size()) {
				m_thruster[n]->level = m_thruster[n]->level_permanent = lvl;
				m_thruster[n]->level_override = 0.0;
			}
		}
	} else if (!_strnicmp (line, "IDS", 3)) {
		DWORD step, irange, m, i;
		for (pd = strtok (line+3, " "), n = 0; n < ndock && pd; pd = strtok (NULL, " ")) {
			if ((m = sscanf (pd, "%d%c%d%c%d", &i, &c, &step, &c, &irange)) >= 3 && i < ndock) {
				if (m < 5) irange = 20;
				SetDockIDS (dock[i], (float)(step*0.05 + NAV_RADIO_FREQ_MIN), (float)(irange*1e3));
			}
		}
	} else if (!_strnicmp (line, "NAVFREQ", 7)) {
		DWORD step;
		for (pd = strtok (line+7, " "), n = 0; n < nnav && pd; pd = strtok (NULL, " "))
			if (sscanf (pd, "%d", &step) == 1) {
				nav[n].freq = (float)((nav[n].step = step)*0.05 + NAV_RADIO_FREQ_MIN);
				n++;
			}
	} else return ParseScenarioLineDirect (line);
	return true;
}

// ==============================================================
// Read a line from a scenario file into a VESSELSTATUS2 interface

bool Vessel::ParseScenarioLine2 (char *line, void *status)
{
	char cbuf[256], *pd, c;
	DWORD nn, n;
	double lvl;
	VESSELSTATUS2 *vs = (VESSELSTATUS2*)status;

	if (!_strnicmp (line, "STATUS", 6)) {

		line = trim_string (line+6);
		if (!_strnicmp (line, "LANDED", 6)) {
			vs->rbody = (OBJHANDLE)g_psys->GetGravObj (trim_string (line+6));
			vs->surf_hdg = 0.0; // default when landed
			vs->status = 1;
			vs->arot.x = 10; // flag for 'not set'
		} else if (!_strnicmp (line, "ORBITING", 8)) {
			vs->rbody = (OBJHANDLE)g_psys->GetGravObj (trim_string (line+8));
			vs->status = 0;
#ifdef UNDEF
		} else if (!strnicmp (line, "DOCKED", 6)) {
			line = trim_string (line+6);
			if (pd = strtok (line, ":")) {
				strcpy (cbuf, pd);
				vs->rbody = (OBJHANDLE)g_psys->GetGravObj (trim_string (cbuf));
				if (pd = strtok (NULL, ":")) {
					strcpy (cbuf, pd);
					vs->base = (OBJHANDLE)g_psys->GetStation (trim_string (cbuf));
					if (pd = strtok (NULL, ":")) {
							sscanf (pd, "%d", &vs->port);
						vs->status = 3;
					}
				}
			}
#endif
		}

	} else if (!_strnicmp (line, "BASE", 4)) {

		line = trim_string (line+4);
		if (pd = strtok (line, ":")) {
			strcpy (cbuf, pd);
			// at this point we assume that vs.rbody has already been assigned,
			// i.e. that the STATUS LANDED line has already been parsed
			Base *base = ((Planet*)vs->rbody)->GetBase (trim_string(cbuf));
			if (!base) {
				char cerr[1024];
				sprintf (cerr, "Scenario parse error for vessel %s: base '%s' not found on body '%s'.", name.c_str(), trim_string(cbuf), ((Planet*)vs->rbody)->Name());
				LOGOUT_ERR(cerr);
				g_pOrbiter->TerminateOnError();
			}
			vs->base = (OBJHANDLE)base;
			if (pd = strtok (NULL, ":")) {
				sscanf (pd, "%d", &vs->port);
				vs->port--;
				base->Pad_EquPos (vs->port, vs->surf_lng, vs->surf_lat);
				// place ship in centre of landing pad by default
			}
		}

	} else if (!_strnicmp (line, "POS", 3)) {

		sscanf (line+3, "%lf%lf", &vs->surf_lng, &vs->surf_lat);
		vs->surf_lng *= RAD;
		vs->surf_lat *= RAD;

	} else if (!_strnicmp (line, "HEADING", 7)) {

		sscanf (line+7, "%lf", &vs->surf_hdg);
		vs->surf_hdg *= RAD;

	} else if (!_strnicmp (line, "PRPLEVEL", 8)) { // propellant status

		if (vs->nfuel) delete []vs->fuel;
		// pass 1: find out how many propellant definitions there are
		for (nn = 0, pd = line+8; *pd; pd++) {
			if (*pd == ':') nn++;
		}
		vs->nfuel = nn;
		vs->fuel = new VESSELSTATUS2::FUELSPEC[nn]; TRACENEW
		// pass 2: read propellant definitions
		for (nn = 0, pd = strtok (line+8, " "); pd; pd = strtok (NULL, " "))
			if (sscanf (pd, "%d%c%lf", &n, &c, &lvl) == 3) {
				vs->fuel[nn].idx = n;
				vs->fuel[nn].level = lvl;
				nn++;
			}

	} else if (!_strnicmp (line, "FUEL", 4)) { // global propellant resource setting

		if (sscanf (line+4, "%lf", &lvl)) { // old style fuel definition
			if (vs->nfuel) delete []vs->fuel;
			vs->fuel = new VESSELSTATUS2::FUELSPEC[vs->nfuel = 1]; TRACENEW
			vs->fuel[0].idx = (DWORD)-1; // mark 'global'
			vs->fuel[0].level = lvl;
		}

	} else if (!_strnicmp (line, "THLEVEL", 7)) { // read thruster status

		if (vs->nthruster) delete []vs->thruster;
		// pass 1: find out how many thruster defintions there are
		for (nn = 0, pd = line+7; *pd; pd++) if (*pd == ':') nn++;
		vs->nthruster = nn;
		vs->thruster = new VESSELSTATUS2::THRUSTSPEC[nn]; TRACENEW
		// pass 2: read thruster definitions
		for (nn = 0, pd = strtok (line+7, " "); pd; pd = strtok (NULL, " "))
			if (sscanf (pd, "%d%c%lf", &n, &c, &lvl) == 3) {
				vs->thruster[nn].idx = n;
				vs->thruster[nn].level = lvl;
				nn++;
			}

	} else if (!_strnicmp (line, "ENGINE_MAIN", 11)) { // old style main/retro thruster status

		if (sscanf (line+11, "%lf", &lvl)) {
			if (lvl > 0) {
				ThrustGroupSpec& tgs = m_thrusterGroupDef[THGROUP_MAIN];
				for (auto it = tgs.ts.begin(); it != tgs.ts.end(); it++) {
					for (nn = 0; nn < m_thruster.size(); nn++) {
						if (m_thruster[nn] == *it) {
							VESSELSTATUS2::THRUSTSPEC *tmp = new VESSELSTATUS2::THRUSTSPEC[vs->nthruster+1]; TRACENEW
							if (vs->nthruster) { memcpy (tmp, vs->thruster, vs->nthruster*sizeof (VESSELSTATUS2::THRUSTSPEC)); delete []vs->thruster; }
							tmp[vs->nthruster].idx = nn;
							tmp[vs->nthruster].level = lvl;
							vs->thruster = tmp;
							vs->nthruster++;
						}
					}
				}
			} else if (lvl < 0) {
				ThrustGroupSpec& tgs = m_thrusterGroupDef[THGROUP_RETRO];
				for (auto it = tgs.ts.begin(); it != tgs.ts.end(); it++) {
					for (nn = 0; nn < m_thruster.size(); nn++) {
						if (m_thruster[nn] == *it) {
							VESSELSTATUS2::THRUSTSPEC *tmp = new VESSELSTATUS2::THRUSTSPEC[vs->nthruster+1]; TRACENEW
							if (vs->nthruster) { memcpy (tmp, vs->thruster, vs->nthruster*sizeof (VESSELSTATUS2::THRUSTSPEC)); delete []vs->thruster; }
							tmp[vs->nthruster].idx = nn;
							tmp[vs->nthruster].level = -lvl;
							vs->thruster = tmp;
							vs->nthruster++;
						}
					}
				}
			}
		}
	} else if (!_strnicmp (line, "ENGINE_HOVR", 11)) { // old style hover thruster status

		if (sscanf (line+11, "%lf", &lvl) && lvl > 0) {
			ThrustGroupSpec& tgs = m_thrusterGroupDef[THGROUP_HOVER];
			for (auto it = tgs.ts.begin(); it != tgs.ts.end(); it++) {
				for (nn = 0; nn < m_thruster.size(); nn++) {
					if (m_thruster[nn] == *it) {
						VESSELSTATUS2::THRUSTSPEC *tmp = new VESSELSTATUS2::THRUSTSPEC[vs->nthruster+1]; TRACENEW
						if (vs->nthruster) { memcpy (tmp, vs->thruster, vs->nthruster*sizeof (VESSELSTATUS2::THRUSTSPEC)); delete []vs->thruster; }
						tmp[vs->nthruster].idx = nn;
						tmp[vs->nthruster].level = lvl;
						vs->thruster = tmp;
						vs->nthruster++;
					}
				}
			}
		}

	} else if (!_strnicmp (line, "DOCKINFO", 8)) {

		if (vs->ndockinfo) delete []vs->dockinfo;
		// pass 1: find number of dock info records
		for (nn = 0, pd = line+8; *pd; pd++) if (*pd == ':') nn++;
		vs->ndockinfo = nn;
		vs->dockinfo = new VESSELSTATUS2::DOCKINFOSPEC[nn]; TRACENEW
		// pass 2: read dock info records
		for (nn = 0, pd = strtok (line+8, " "); pd; pd = strtok (NULL, " ")) {
			sscanf (pd, "%d:%d,%s", &vs->dockinfo[nn].idx, &vs->dockinfo[nn].ridx, cbuf);
			// DODGY - cast name into 4 bytes of vessel pointer!
			vs->dockinfo[nn].rvessel = 0;
			BYTE *tmp = (BYTE*)&vs->dockinfo[nn].rvessel;
			for (int i = 0; cbuf[i]; i++) tmp[i%4] += cbuf[i];
			nn++;
		}

	} else if (!_strnicmp (line, "RPOS", 4)) {
		sscanf (line+4, "%lf%lf%lf", &vs->rpos.x, &vs->rpos.y, &vs->rpos.z);
	} else if (!_strnicmp (line, "RVEL", 4)) {
		sscanf (line+4, "%lf%lf%lf", &vs->rvel.x, &vs->rvel.y, &vs->rvel.z);
	} else if (!_strnicmp (line, "AROT", 4)) {
		sscanf (line+4, "%lf%lf%lf", &vs->arot.x, &vs->arot.y, &vs->arot.z);
		vs->arot.x *= RAD, vs->arot.y *= RAD, vs->arot.z *= RAD;
	} else if (!_strnicmp (line, "VROT", 4)) {
		sscanf (line+4, "%lf%lf%lf", &vs->vrot.x, &vs->vrot.y, &vs->vrot.z);
		vs->vrot.x *= RAD, vs->vrot.y *= RAD, vs->vrot.z *= RAD;
	} else if (!_strnicmp (line, "ALT", 3)) { // NOTE: 'ALT' and 'VROT' cannot be used together. ALT is used for landed vessels
		sscanf (line+3, "%lf", &vs->vrot.x);
	} else if (!_strnicmp (line, "ELEMENTS", 8)) {
		double a, e, i, theta, omegab, L, elmjd;
		Vector rpos, rvel;
		sscanf (line+8, "%lf%lf%lf%lf%lf%lf%lf",  &a, &e, &i, &theta, &omegab, &L, &elmjd);
		if (vs->rbody) {
			el->Set (a, e, i*RAD, theta*RAD, omegab*RAD, L*RAD, elmjd);
			el->Setup (mass, ((Body*)vs->rbody)->Mass(), td.MJD_ref);
			el->Update (rpos, rvel);
			vs->rpos.x = rpos.x, vs->rpos.y = rpos.y, vs->rpos.z = rpos.z;
			vs->rvel.x = rvel.x, vs->rvel.y = rvel.y, vs->rvel.z = rvel.z;
			el_valid = true;
		}
	} else if (!_strnicmp (line, "IDS", 3)) {
		DWORD step, irange, m, i, n = 0;
		char c;
		for (pd = strtok (line+3, " "); n < ndock && pd; pd = strtok (NULL, " ")) {
			if ((m = sscanf (pd, "%d%c%d%c%d", &i, &c, &step, &c, &irange)) >= 3 && i < ndock) {
				if (m < 5) irange = 20;
				SetDockIDS (dock[i], (float)(step*0.05 + NAV_RADIO_FREQ_MIN), (float)(irange*1e3));
			}
		}
	} else if (!_strnicmp (line, "NAVFREQ", 7)) {
		DWORD step, n = 0;
		for (pd = strtok (line+7, " "); n < nnav && pd; pd = strtok (NULL, " "))
			if (sscanf (pd, "%d", &step) == 1) {
				nav[n].freq = (float)((nav[n].step = step)*0.05 + NAV_RADIO_FREQ_MIN);
				n++;
			}
	} else if (!_strnicmp (line, "XPDR", 4)) {
		sscanf (line+4, "%d", &vs->xpdr);
	} else return ParseScenarioLineDirect (line);
	return true;
}


// ==============================================================
// The following options are read directly from the scenario file,
// bypassing the VESSELSTATUSx structure. They should be collected
// into an extended VESSELSTATUSx (3?) structure.
// ==============================================================

bool Vessel::ParseScenarioLineDirect (char *line)
{
	if (!_strnicmp (line, "RCSMODE", 7)) {
		sscanf (line+7, "%d", &attmode);
		return true;
	} else if (!_strnicmp (line, "AFCMODE", 7)) {
		sscanf (line+7, "%d", &ctrlsurfmode);
		return true;
	} else if (!_strnicmp (line, "ATTACHED", 8)) {
		char cbuf[256];
		sscanf (line+8, "%d:%d,%s", &attach_status.ci, &attach_status.pi, cbuf);
		if (attach_status.pname) delete []attach_status.pname;
		attach_status.pname = new char[strlen(cbuf)+1]; TRACENEW
		strcpy (attach_status.pname, cbuf);
		return true;
	} else if (!_strnicmp (line, "FLIGHTDATA", 10)) {
		bRequestPlayback = true;
	}
	return false;
}

DWORD Vessel::PackDefaultState (char **data, DWORD flag)
{
	DWORD size = sizeof(ScenarioData)-sizeof(char*);
	const char *cname = (fstatus == FLIGHTSTATUS_FREEFLIGHT ? cbody->Name() : proxyplanet->Name());
	size += strlen(cname)+1;
	if (flag & SD_NAME) {
		size += name.size() + 1;
		size += (classname ? strlen(classname)+1 : 1);
	}
	*data = new char[size];
	ScenarioData *sd = (ScenarioData*)*data; TRACENEW
	sd->size = size;
	sd->flag = flag;
	sd->fstate = (BYTE)fstatus;
	switch (fstatus) {
	case FLIGHTSTATUS_FREEFLIGHT:
		sd->rpos = cpos;
		sd->rvel = cvel;
		EulerAngles (s0->R, sd->arot);
		sd->vrot = s0->omega;
		break;
	case FLIGHTSTATUS_LANDED:
		sd->lng = sp.lng;
		sd->lat = sp.lat;
		sd->hdg = sp.dir;
		break;
	}
	strcpy (sd->buf, cname);
	if (flag & SD_NAME)
		sprintf (sd->buf + strlen(sd->buf), "\n%s\n%s", name.c_str(), classname ? classname : "");
	return size;
}

void Vessel::ApplyPackedState (const char *data)
{
	char *pc;
	ScenarioData *sd = (ScenarioData*)data;

	pc = strtok (sd->buf, "\n");
	cbody = g_psys->GetGravObj (pc);
	if (!cbody) cbody = g_psys->GetStar(0); // a rather desparate default to keep things going
	el->Setup (mass, cbody->Mass(), td.MJD_ref);

	switch (sd->fstate) {
	case 0: // freeflight
		InitOrbiting (sd->rpos, sd->rvel, sd->arot, &sd->vrot);
		break;
	case 1: // landed
		InitLanded ((Planet*)cbody, sd->lng, sd->lat, sd->hdg);
		if (proxybase) proxybase->ReportTouchdown (this, sd->lng, sd->lat);
		break;
	}
}

// ==============================================================
// SetStateX versions
// ==============================================================

// ==============================================================
// Set a vessel state from a VESSELSTATUS interface

void Vessel::SetState (const VESSELSTATUS &status)
{
	double lng, lat, dir;
	Vector rpos, rvel, orient, vrot;

	cbody = (CelestialBody*)status.rbody;
	if (!cbody) cbody = g_psys->GetStar(0); // use first sun if no reference is set
	if (!cbody) return;                     // big trouble!

	if (status.flag[0] & 2 && def_tank) {
		// status currently only supports default propellant resource
		SetPropellantMass (def_tank, status.fuel*def_tank->maxmass);
	}
	UpdateMass(); pfmass = fmass;
	el->Setup (mass, cbody->Mass(), td.MJD_ref);

	if (status.flag[0] & 1) { // old-style thruster definition
		if (status.eng_main >= 0.0) {
			SetThrusterGroupLevel (THGROUP_MAIN, status.eng_main);
			SetThrusterGroupLevel (THGROUP_RETRO, 0.0);
		} else {
			SetThrusterGroupLevel (THGROUP_MAIN, 0.0);
			SetThrusterGroupLevel (THGROUP_RETRO, -status.eng_main);
		}
		SetThrusterGroupLevel (THGROUP_HOVER, status.eng_hovr);
	}

	switch (status.status) {
	case 0: // freeflight
		rpos.Set (status.rpos.x, status.rpos.y, status.rpos.z);
		rvel.Set (status.rvel.x, status.rvel.y, status.rvel.z);
		orient.Set (status.arot.x, status.arot.y, status.arot.z);
		vrot.Set (status.vrot.x, status.vrot.y, status.vrot.z);
		if (rpos.length() < cbody->Size()) { // sanity check
			rpos.x = rpos.y = 0.0;
			rpos.z = 1.1*cbody->Size(); // desparate default
		}
		InitOrbiting (rpos, rvel, orient, &vrot);
		break;
	case 1: // landed
		lng = status.vdata[0].x, lat = status.vdata[0].y, dir = status.vdata[0].z;
		if (status.base) {
			landtgt = (Base*)status.base;
			nport = landtgt->OccupyPad (this, status.port, true);
			lstatus = 1;
			//landtgt->Pad_EquPos (nport, lng, lat);
			proxybase = landtgt;
		}
		InitLanded ((Planet*)cbody, lng, lat, dir);
		break;
	default:
		break;
	}
}

// ==============================================================
// Set a vessel state from a VESSELSTATUS2 interface

void Vessel::SetState2 (const void *status)
{
	VESSELSTATUS2 *vs = (VESSELSTATUS2*)status;
	DWORD i, idx;
	double lvl;

	cbody = (CelestialBody*)vs->rbody;
	if (!cbody) cbody = g_psys->GetStar(0); // a rather desparate default to keep things going

	// should we call SetDefaultState() at this point?
	landtgt = 0;
	nport = (DWORD)-1;
	lstatus = 0;

	// set propellant status
	if (vs->flag & VS_FUELRESET)
		for (idx = 0; idx < ntank; idx++)
			SetPropellantMass (tank[idx], 0);
	if (vs->flag & VS_FUELLIST) {
		for (i = 0; i < vs->nfuel; i++) {
			idx = vs->fuel[i].idx;
			lvl = vs->fuel[i].level;
			if (idx < ntank)
				SetPropellantMass (tank[idx], lvl*tank[idx]->maxmass);
			else if (idx == (DWORD)-1) // global setting
				for (idx = 0; idx < ntank; idx++)
					SetPropellantMass (tank[idx], lvl*tank[idx]->maxmass);
		}
	}
	el->Setup (mass, cbody->Mass(), td.MJD_ref);

	// set thruster status
	if (vs->flag & VS_THRUSTRESET)
		for (auto it = m_thruster.begin(); it != m_thruster.end(); it++)
			(*it)->level_permanent = 0;
	if (vs->flag & VS_THRUSTLIST) {
		for (i = 0; i < vs->nthruster; i++) {
			idx = vs->thruster[i].idx;
			lvl = vs->thruster[i].level;
			if (idx < m_thruster.size()) {
				m_thruster[idx]->level_permanent = lvl;
				m_thruster[idx]->level = min (1.0, lvl + m_thruster[idx]->level_override);
			}
		}
	}

	// set dock status
	if (vs->flag & VS_DOCKINFOLIST) {
		for (i = 0; i < vs->ndockinfo; i++) {
			idx = vs->dockinfo[i].idx;
			if ((idx = vs->dockinfo[i].idx) < ndock) {
				dock[idx]->matedock = vs->dockinfo[i].ridx;
				dock[idx]->mate = (Vessel*)vs->dockinfo[i].rvessel;
			}
		}
	}

	// state vectors
	switch (vs->status) {
	case 0: { // freeflight
		if (!attach_status.pname) {
			Vector rp (vs->rpos.x, vs->rpos.y, vs->rpos.z);
			Vector rv (vs->rvel.x, vs->rvel.y, vs->rvel.z);
			Vector orient (vs->arot.x, vs->arot.y, vs->arot.z);
			Vector vr (vs->vrot.x, vs->vrot.y, vs->vrot.z);
			InitOrbiting (rp, rv, orient, &vr);
		}
		} break;
	case 1: // landed
		if (vs->base) {
			landtgt = (Base*)vs->base;
			nport = landtgt->OccupyPad (this, vs->port, true);
			lstatus = 1;
			proxybase = landtgt;
		}
		if (vs->arot.x <= 4.0) { // extended information available
			Matrix lrot;
			lrot.Set (MakeVector(vs->arot));
			double alt = vs->vrot.x;
			InitLanded ((Planet*)cbody, vs->surf_lng, vs->surf_lat, vs->surf_hdg, &lrot, alt);
		} else {
			InitLanded ((Planet*)cbody, vs->surf_lng, vs->surf_lat, vs->surf_hdg);
		}
		if (proxybase = landtgt)
			proxybase->ReportTouchdown (this, vs->surf_lng, vs->surf_lat);
		break;
	}
	if (xpdr && vs->xpdr) xpdr->SetStep (vs->xpdr);
}

// ==============================================================
// GetStateX versions
// ==============================================================

// ==============================================================
// Write a vessel state to a VESSELSTATUS interface

void Vessel::GetState (VESSELSTATUS &status)
{
	memset (&status, 0, sizeof(VESSELSTATUS));
	status.rbody = (OBJHANDLE)cbody;
	Vector dp (GPos() - cbody->GPos());
	status.rpos.x   = dp.x;
	status.rpos.y   = dp.y;
	status.rpos.z   = dp.z;
	Vector dv (GVel() - cbody->GVel());
	status.rvel.x   = dv.x;
	status.rvel.y   = dv.y;
	status.rvel.z   = dv.z;
	if (fstatus == FLIGHTSTATUS_LANDED) {
		status.vrot.x = sp.alt;
		status.vrot.y = 0.0;
		status.vrot.z = 0.0;
		EulerAngles (land_rot, status.arot);
	} else {
		status.vrot.x   = s0->omega.x;
		status.vrot.y   = s0->omega.y;
		status.vrot.z   = s0->omega.z;
		EulerAngles (s0->R, status.arot);
	}
	TankSpec *ts = PropellantHandle(0);
	status.fuel     = (ts ? GetPropellantLevel(ts) : 0.0);
	status.eng_main = GetThrusterGroupLevel (THGROUP_MAIN);
	if (!status.eng_main) status.eng_main = -GetThrusterGroupLevel (THGROUP_RETRO);
	status.eng_hovr = GetThrusterGroupLevel (THGROUP_HOVER);
	status.port     = nport;
	status.base     = landtgt; // should also include docking target
	switch (fstatus) {
	case FLIGHTSTATUS_FREEFLIGHT:
		status.status = 0;
		break;
	case FLIGHTSTATUS_LANDED:
		status.status = 1;
		status.vdata[0].x = sp.lng;
		status.vdata[0].y = sp.lat;
		status.vdata[0].z = sp.dir;
		break;
	case FLIGHTSTATUS_TAXIING:
		status.status = 2;
		break;
	case FLIGHTSTATUS_DOCKED:
		status.status = 3;
		break;
	default:
		status.status = 99;
		break;
	}
}

// ==============================================================
// Write a vessel state to a VESSELSTATUS2 interface

void Vessel::GetState2 (void *status)
{
	VESSELSTATUS2 *vs = (VESSELSTATUS2*)status;
	DWORD i;

	vs->rbody = (OBJHANDLE)cbody;
	vs->base  = proxybase;
	vs->port  = nport;
	Vector dp (GPos() - cbody->GPos());
	Vector dv (GVel() - cbody->GVel());
	vs->rpos  = _V(dp.x, dp.y, dp.z);
	vs->rvel  = _V(dv.x, dv.y, dv.z);
	vs->vrot  = (fstatus == FLIGHTSTATUS_LANDED ? _V(sp.alt, 0, 0) : _V(s0->omega.x, s0->omega.y, s0->omega.z));
	EulerAngles (fstatus == FLIGHTSTATUS_LANDED ? land_rot : s0->R, vs->arot);
	vs->surf_lng = sp.lng;
	vs->surf_lat = sp.lat;
	vs->surf_hdg = sp.dir;

	vs->status = (fstatus == FLIGHTSTATUS_LANDED ? 1 :
	              fstatus == FLIGHTSTATUS_DOCKED ? 2 : 0);

	if (vs->flag & VS_FUELLIST) {
		vs->nfuel = ntank;
		if (!vs->fuel) { vs->fuel = new VESSELSTATUS2::FUELSPEC[ntank]; TRACENEW }
		for (i = 0; i < ntank; i++) {
			vs->fuel[i].idx = i;
			vs->fuel[i].level = tank[i]->mass / tank[i]->maxmass;
		}
	}
	if (vs->flag & VS_THRUSTLIST) {
		vs->nthruster = m_thruster.size();
		if (!vs->thruster) { vs->thruster = new VESSELSTATUS2::THRUSTSPEC[m_thruster.size()]; TRACENEW }
		for (i = 0; i < m_thruster.size(); i++) {
			vs->thruster[i].idx = i;
			vs->thruster[i].level = m_thruster[i]->level;
		}
	}
	if (vs->flag & VS_DOCKINFOLIST) {
		vs->ndockinfo = ndock;
		if (!vs->dockinfo) { vs->dockinfo = new VESSELSTATUS2::DOCKINFOSPEC[ndock]; TRACENEW }
		for (i = 0; i < ndock; i++) {
			vs->dockinfo[i].idx = i;
			vs->dockinfo[i].ridx = dock[i]->matedock;
			vs->dockinfo[i].rvessel = (OBJHANDLE)dock[i]->mate;
		}
	}
	vs->xpdr = (xpdr ? xpdr->GetStep() : 0);
}

// ==============================================================

int Vessel::TouchdownPointsFromFile (const char *fname)
{
	// open the file
	char line[512], *c;
	strcpy (line, fname);
	strcat (line, ".dat");
	std::ifstream ifs (g_pOrbiter->Cfg()->ConfigPathNoext (line));
	if (!ifs.good()) return 1;

	// parse the touchdown point specs
	int i, res, ntdp;
	ifs.getline (line, 512);
	c = trim_string (line);
	res = sscanf (c, "%d", &ntdp);
	if (res != 1 || ntdp < 3)
		return 2;
	
	TOUCHDOWNVTX *tdp = new TOUCHDOWNVTX[ntdp];
	for (i = 0; i < ntdp; i++) {
		ifs.getline (line, 512);
		c = trim_string (line);
		res = sscanf (c, "%lf%lf%lf%lf%lf%lf%lf",
			&tdp[i].pos.x, &tdp[i].pos.y, &tdp[i].pos.z,
			&tdp[i].stiffness, &tdp[i].damping, &tdp[i].mu, &tdp[i].mu_lng);
		if (res < 7) {
			tdp[i].mu_lng = mu_lng;
			if (res < 6) {
				tdp[i].mu = mu;
				if (res < 5) {
					tdp[i].damping = 1e5;
					if (res < 4) {
						tdp[i].stiffness = 1e6;
						if (res < 3) {
							delete []tdp;
							tdp = NULL;
							return 2;
						}
					}
				}
			}
		}
	}

	SetTouchdownPoints (tdp, ntdp);
	delete []tdp;
	tdp = NULL;
	return 0;
}

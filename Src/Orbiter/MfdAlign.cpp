// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "MfdAlign.h"
#include "Pane.h"
#include "Planet.h"
#include "Psys.h"
#include "Log.h"
#include "Select.h"
#include <iomanip>

using namespace std;

extern PlanetarySystem *g_psys;
extern TimeData td;
extern InputBox *g_input;
extern Select *g_select;
extern char DBG_MSG[256];

static const Body *last_target = 0;

// =======================================================================
// class Instrument_OPlaneAlign

struct Instrument_OPlaneAlign::SavePrm Instrument_OPlaneAlign::saveprm = {0,0,0,false,0.0,0.0};

Instrument_OPlaneAlign::Instrument_OPlaneAlign (Pane *_pane, INT_PTR _id, const Spec &spec, Vessel *_vessel, bool restore)
: Instrument (_pane, _id, spec, _vessel)
{
	shpel = new Elements(); TRACENEW
	tgtel = new Elements(); TRACENEW
	mode = ORBIT;
	automode = true;
	if (restore && vessel == saveprm.usr) {
		SetRef (saveprm.elref ? saveprm.elref : _vessel->ElRef());
		SetTarget (saveprm.tgt);
		customel = saveprm.customel;
		if (customel)
			SetCustomEls (saveprm.i, saveprm.theta);
	} else {
		SetRef(_vessel->ElRef());
		tgt = 0;
		customel = false;
	}
	if (elref) {
		shpel->SetMasses(_vessel->Mass(), elref->Mass());
		shpel->Calculate(vessel->GPos() - elref->GPos(), vessel->GVel() - elref->GVel(), td.SimT1);
		shpel->Setup (_vessel->Mass(), elref->Mass(), _vessel->Els()->MJDepoch());
		if (tgt) {
			tgtel->SetMasses(tgt->Mass(), elref->Mass());
			tgtel->Calculate(tgt->GPos() - elref->GPos(), tgt->GVel() - elref->GVel(), td.SimT1);
			tgtel->Setup(tgt->Mass(), elref->Mass(), _vessel->Els()->MJDepoch());
		}
	}

	pphi = -1.0;
	preli = 0.0;
	engage = false;
}

Instrument_OPlaneAlign::~Instrument_OPlaneAlign ()
{
	// save status
	saveprm.usr      = vessel;
	saveprm.elref    = elref;
	saveprm.tgt      = tgt;
	saveprm.customel = customel;
	if (customel) {
		saveprm.i     = tgtel->i;
		saveprm.theta = tgtel->theta;
	}
	delete shpel;
	delete tgtel;
}

HELPCONTEXT *Instrument_OPlaneAlign::HelpTopic () const
{
	extern HELPCONTEXT DefHelpContext;
	DefHelpContext.topic = (char*)"/mfd_align.htm";
	return &DefHelpContext;
}

bool Instrument_OPlaneAlign::SelectRef (char *name)
{
	CelestialBody *obj = g_psys->GetGravObj (name, true);
	if (!obj) return false;
	SetRef (obj);
	return true;
}

void Instrument_OPlaneAlign::SelectAutoRef ()
{
	SetRef (vessel->ElRef());
}

void Instrument_OPlaneAlign::SetRef (const CelestialBody *cbody)
{
	if (cbody != elref) {
		elref = cbody;
		shpel->SetMasses (0.0, elref->Mass());
		tgtel->SetMasses (0.0, elref->Mass());
		refrad = elref->Size();
		Refresh();
	}
}

void Instrument_OPlaneAlign::SetTarget (const Body *target)
{
	if (target == tgt) return;
	tgt = target;
	if (target) {
		last_target = target;
		if (elref) {
			tgtel->SetMasses(tgt->Mass(), elref->Mass());
			tgtel->Calculate(tgt->GPos() - elref->GPos(), tgt->GVel() - elref->GVel(), td.SimT1);
			tgtel->Setup(tgt->Mass(), elref->Mass(), shpel->MJDepoch());
		}
		customel = false;
		pphi = -1.0;
		Refresh();
	}
}

void Instrument_OPlaneAlign::SetCustomEls (double i, double theta)
{
	tgtel->SetMasses (0.0, elref->Mass());
	tgtel->Reset (1.0, 0.0, i, theta, 0.0, 0.0, td.MJD_ref);
	tgt = 0;
	customel = true;
}

void Instrument_OPlaneAlign::UpdateDraw (oapi::Sketchpad *skp)
{
	char cbuf[128];
	int x = cw/2, x2 = x + cw*11, x3 = x2 + cw*11, y = 1+(ch*5)/2, y1;

	// update my elements
	double refrad = elref->Size();
	Vector sp = vessel->GPos() - elref->GPos();
	Vector sv = vessel->GVel() - elref->GVel();
	shpel->Calculate(sp, sv, td.SimT1);
	bool subsurf_node = (shpel->PeDist() < refrad);

	if (automode) {
		if (vessel->GroundContact())
			mode = SURFACE;
		else if (subsurf_node)
			mode = BALLISTIC;
		else
			mode = ORBIT;
	}

	DisplayTitle (skp, "Align plane");
	const char *modestr[3] = { "Orbit  ", "Ballist", "Surface" };
	if (elref) {
		skp->Text (cw*17, 1, elref->Name(), min ((size_t)16, strlen(elref->Name())));
		strcpy (cbuf, tgt ? tgt->Name() : customel ? "[Custom]" : "[None]");
		skp->Text (cw*17, 1+ch, cbuf, strlen (cbuf));
		skp->Text(cw * 5, 1 + ch, modestr[mode], 7);
		skp->SetTextColor (draw[2][1].col);
		skp->Text ((cw*27)/2, 1, "Ref", 3);
		skp->Text ((cw*27)/2, 1+ch, "Tgt", 3);
		skp->Text(x, 1 + ch, automode ? "Auto" : "Mode", 4);
	} else {
		skp->SetTextColor (draw[0][0].col);
		skp->Text (x, y, "No reference body selected.", 27);
		return;
	}
	skp->SetTextColor (draw[0][0].col);

	if (!(tgt || customel)) {
		skp->Text (x, y, "No target orbit selected.", 25); y += ch;
		skp->Text (x, y, "Use Shift-T to select a target", 30); y += ch;
		skp->Text (x, y, "or Shift-E to define a custom", 29); y+= ch;
		skp->Text (x, y, "orbit.", 6);
		return;
	}

	// update target elements
	Vector tp, tv;
	if (tgt) {
		tp = tgt->GPos() - elref->GPos();
		tv = tgt->GVel() - elref->GVel();
		tgtel->Calculate(tp, tv, td.SimT1);
	}

	bool have_intersection = true;

	if (mode == BALLISTIC) {
		if (shpel->PeDist() < refrad) {
			double a = (shpel->ApDist() + refrad) * 0.5; // target semimajor axis
			double pd = refrad; // target periapsis distance
			double le = a - pd; // linear eccentricity
			double e = le / a; // target eccentricity
			shpel->Reset(a, e, shpel->i, shpel->theta, shpel->omegab, shpel->L, shpel->MJDepoch());
		}
	}

	// ship in planet coords
	double svmag = sv.length();
	sp.unify(), sv.unify();
	// normals of the two orbital planes
	Vector nm1 = shpel->HVec().unit();
	Vector nm2 = tgtel->HVec().unit();

	double reli = xangle(nm1, nm2);    // relative inclination
	double didt = (reli - preli) / dT; // inclination rate
	preli = reli;                      // remember for next step

	// angle between ship and ascending node
	Vector nd = crossp(nm1, nm2);
	nd.unify();
	double cosp = dotp(nd, sp);
	double Aan = acos(cosp);
	if (dotp(nm1, crossp(nd, sp)) < 0) Aan = Pi2 - Aan; // ascending node is behind us
	double Adn = posangle(Aan + Pi);
	double Aan_signed = (Aan < Pi ? Aan : Pi2 - Aan);

	// true anomalies of ascending and descending node
	double trs = shpel->TrueAnm();
	double tra = normangle(trs + Aan);
	double trd = normangle(trs + Adn);
	// mean anomalies
	double mas = shpel->MeanAnm();
	double maa = shpel->MeanAnomaly_from_TrueAnomaly(tra);
	double mad = shpel->MeanAnomaly_from_TrueAnomaly(trd);
	// mean anomaly differences
	double dmaa = posangle(maa - mas);
	double dmad = posangle(mad - mas);

	double Tan = dmaa / Pi2 * shpel->OrbitT(); // time to ascending node
	double Tdn = dmad / Pi2 * shpel->OrbitT(); // time to descending node

	double vsurf;
	if (mode == SURFACE) { // modify timings assuming we are sitting on the surface
		have_intersection = GetTimingsFromSurface(Tan, Aan, Tdn, Adn, vsurf);
		Aan_signed = (Aan < Pi ? Aan : Pi2 - Aan);
	}

	bool an_is_next = (have_intersection ? Tan < Tdn : true);
	bool an_is_closest = (fabs(Aan_signed) < PI05);
	double Tnode = (an_is_next ? Tan : Tdn); // time to next node

	double T0 = (an_is_closest ? Tan : Tdn);
	if (an_is_closest != an_is_next)
		T0 -= (mode == SURFACE ? elref->RotT() : shpel->OrbitT());

	// orbital speeds
	double van = (mode == SURFACE ? vsurf : shpel->Spd_TA(tra));
	double vdn = (mode == SURFACE ? vsurf : shpel->Spd_TA(trd));
	// dV requirements (this assumes that the thrust is applied normal to the orbital plane throughout
	// the burn - this is less efficient than pointing directly in the dV direction, where dV = 2*sin(reli/2)*V)
	double dVan = van*reli;
	double dVdn = vdn*reli;
	// burn times
	double Th_main = vessel->GetThrusterGroupMaxth(THGROUP_MAIN);
	double acc = Th_main / vessel->Mass();
	double burnTan = dVan / acc;
	double burnTdn = dVdn / acc;
	double burnT = (an_is_closest ? burnTan : burnTdn);
	if (!engage) {
		engage = (mode == SURFACE ? T0 < burnT*0.5 && T0 > 0 : (T0 < burnT*0.5 && T0 > -burnT*0.5));
		isasc = (mode == ORBIT ? an_is_next : an_is_closest);
	} 
	else {
		if (mode == ORBIT || mode == BALLISTIC) {
			if (isasc != an_is_closest)
				engage = false;
		}
		else if (have_intersection) {
			if (isasc != an_is_next)
				engage = false;
		}
		else {
			if (Tan > elref->RotT()*0.5)
				engage = false;
		}
	}

	// output relevant elements of current and target orbits, and relative inclination + rate
	sprintf(cbuf, "Current     Target      Relative");
	skp->SetPen(draw[0][1].solidpen);
	skp->Text(x, y, cbuf, 32); y += ch+1;
	skp->Line(x, y, x + cw * 11, y);
	skp->Line(x + cw * 12, y, x + cw * 23, y);
	skp->Line(x + cw * 24, y, x + cw * 36, y); y += 1;
	sprintf (cbuf, "Inc% 7.2fº Inc% 7.2fº RInc%7.2fº", Deg(shpel->i), Deg(tgtel->i), Deg(reli));
	skp->Text(x, y, cbuf, strlen(cbuf)); y += ch;
	sprintf (cbuf, "LAN% 7.2fº LAN% 7.2fº R %+7.3fº/s", Deg(shpel->theta), Deg(tgtel->theta), Deg(didt));
	skp->Text(x, y, cbuf, strlen(cbuf)); y += 2 * ch;

	// node encounter data
	skp->Text(x, y, "Node encounter", 14); y += ch;
	skp->Text(x + cw * 3, y, "dA[º] TtN[s]", 12); y += ch + 1;
	skp->Line(x + cw * 3, y, x + cw * 8, y);
	skp->Line(x + cw * 9, y, x + cw * 15, y); y += 1;
	// ascending node
	if (have_intersection) {
		sprintf(cbuf, "AN%6.1f%s", Aan*DEG, FloatStr(Tan, 3));
		skp->Text(x, an_is_next ? y : y + ch, cbuf, strlen(cbuf)); y += ch;
		// descending node
		sprintf(cbuf, "DN%6.1f%s", Adn*DEG, FloatStr(Tdn, 3));
		skp->Text(x, an_is_next ? y : y - ch, cbuf, strlen(cbuf)); y += 2 * ch;
	}
	else {
		sprintf(cbuf, "CE%6.1f%s", Aan*DEG, FloatStr(Tan, 3));
		skp->Text(x, y, cbuf, strlen(cbuf)); y += 3 * ch;
	}

	// thrust estimates
	y = IH - (9*ch)/2;
	sprintf(cbuf, "Thrust [%sN]", FloatStr(Th_main, 4)+1);
	skp->Text(x, y, cbuf, strlen(cbuf)); y += ch;
	strcpy(cbuf, "Dir   dV[m/s]  BT[s]   TtB[s]");
	if (mode == SURFACE) cbuf[25] = 'L';
	skp->Text(x + cw * 4, y, cbuf, 30); y += ch + 1;
	skp->Line(x + cw * 4, y, x + cw * 8, y);
	skp->Line(x + cw * 10, y, x + cw * 17, y);
	skp->Line(x + cw * 19, y, x + cw * 25, y);
	skp->Line(x + cw * 27, y, x + cw * 33, y); y += 1;
	sprintf(cbuf, have_intersection ? "AN  NML- %s" : "CE  90º  %s", FloatStr(dVan));
	y1 = (an_is_next ? y : y + ch);
	skp->Text(x, y1, cbuf, strlen(cbuf));
	strcpy(cbuf, FloatStr(burnTan, 3));
	skp->Text(x + cw * 18, y1, cbuf, strlen(cbuf));
	if (!engage || !an_is_closest) {
		strcpy(cbuf, FloatStr(Tan - burnTan*0.5, 3));
		skp->Text(x + cw * 26, y1, cbuf, strlen(cbuf));
	}
	else {
		skp->SetTextColor(draw[1][0].col);
		skp->Text(x + cw * 26, y1, mode == SURFACE ? "[LAUNCH]" : " [BURN] ", 8);
		skp->SetTextColor(draw[0][0].col);
	}
	y += ch;
	if (have_intersection) {
		y1 = (an_is_next ? y : y - ch);
		sprintf(cbuf, "DN  NML+ %s", FloatStr(dVdn));
		skp->Text(x, y1, cbuf, strlen(cbuf));
		strcpy(cbuf, FloatStr(burnTdn, 3));
		skp->Text(x + cw * 18, y1, cbuf, strlen(cbuf));
		if (!engage || an_is_closest) {
			strcpy(cbuf, FloatStr(Tdn - burnTdn*0.5, 3));
			skp->Text(x + cw * 26, y1, cbuf, strlen(cbuf));
		}
		else {
			skp->SetTextColor(draw[1][0].col);
			skp->Text(x + cw * 26, y1, mode == SURFACE ? "[LAUNCH]" : " [BURN] ", 8);
			skp->SetTextColor(draw[0][0].col);
		}
	}

	// subsurface warning
	if (mode == ORBIT && subsurf_node || !have_intersection) {
		skp->SetTextColor(draw[1][1].col);
		y = ch * 12;
		skp->SetFont(GetDefaultFont(1));
		if (!have_intersection) {
			skp->Text(x, y, "No intersection with target", 27); y += ch;
			skp->Text(x, y, "plane at launch latitude.", 25);
		} else {
			skp->Text(x, y, "Trajectory intersects surface", 29); y += ch;
			skp->Text(x, y, "Data may be inaccurate.", 23);
		}
	}

	// graphics
	int RD, CX, CY;
	RD = (IW*25)/100;
	CX = IW-RD-cw*2;
	CY = (55*IH) / 100;
	int xa, ya, xd, yd;
	Matrix irot(IRotMatrix(shpel->cost, shpel->sint, shpel->cosi, shpel->sini));
	Vector P = mul(irot, sp);
	x = (int)(RD*P.x);
	y = (int)(RD*P.z);
	if (mode == ORBIT) {
		P = mul(irot, nd);
		xa = (int)(RD*P.x);
		ya = -(int)(RD*P.z);
		xd = -xa;
		yd = -ya;
	}
	else {
		double tht = atan2(P.z, P.x);
		double thtan = tht + Aan;
		xa = (int)(cos(thtan)*RD);
		ya = -(int)(sin(thtan)*RD);
		double thtdn = tht + Adn;
		xd = (int)(cos(thtdn)*RD);
		yd = -(int)(sin(thtdn)*RD);
	}
	skp->SetPen (draw[1][0].solidpen);
	skp->SetFont(GetDefaultFont(1));
	skp->SetTextColor(draw[1][0].col);
	skp->SetTextAlign(oapi::Sketchpad::CENTER);
	if (have_intersection) {
		skp->Line(CX + xa, CY + ya, CX + xd, CY + yd); // node line
		xa = (xa * 10) / 9;  ya = (ya * 10) / 9;
		xd = (xd * 10) / 9;  yd = (yd * 10) / 9;
		skp->Text(CX + xa, CY + ya - ch / 2, "AN", 2);
		skp->Text(CX + xd, CY + yd - ch / 2, "DN", 2);
	}
	else {
		skp->Line(CX + xa - cw / 2, CY + ya, CX + xa + cw / 2, CY + ya);
		skp->Line(CX + xa, CY + ya - cw / 2, CX + xa, CY + ya + cw / 2);
		xa = (xa * 10) / 9;  ya = (ya * 10) / 9;
		skp->Text(CX + xa, CY + ya - ch / 2, "CE", 2);
	}
	skp->SetPen (draw[0][0].solidpen);
	skp->Ellipse (CX-RD, CY-RD, CX+RD+1, CY+RD+1);
	skp->Line (CX, CY, CX+x, CY-y); // radius vector
	x = (x*10)/9;  y = (y*10)/9;
	skp->SetTextColor(draw[0][0].col);
	skp->Text (CX+x, CY-y-ch/2, "P", 1);
	pphi = Aan;
	pT = td.SimT1;
}

bool Instrument_OPlaneAlign::GetTimingsFromSurface(double &Tan, double &Aan, double &Tdn, double &Adn, double &VSurf)
{
	// target plane normal
	Vector nm2 = tgtel->HVec().unit();
	nm2 = tmul(elref->GRot(), nm2); // rotate into frame of reference planet
	double nx = nm2.x, ny = nm2.y, nz = nm2.z;

	// ship position in reference body frame
	Vector p = elref->GlobalToLocal(vessel->GPos());
	double y0 = p.y;
	double r0 = hypot(p.x, p.z);
	double phi0 = atan2(p.x, p.z);

	// velocity of surface point
	double rotT = elref->RotT();
	VSurf = r0 * Pi2 / rotT;

	// Find intersection of trajectory of surface point with target orbital plane
	// Assume that vessel remains fixed in reference body frame (e.g. parked on surface)
	// Then the trajectory is a circle in the plane of current y0 = p.y with radius r0 = hypot(p.x, p.z)
	// So equations of trajectory in space:
	//    sqrt(x^2 + z^2) = r0
	//    y = y0
	// Equation of target orbital plane with normal N = (nx, ny, nz):
	//    nx*x + ny*y + nz*z = 0
	// Solving this leads to:
	double a = 1.0 + (nx*nx) / (nz*nz);
	double b = 2.0*nx*ny*y0 / (nz*nz);
	double c = ny*ny*y0*y0 / (nz*nz) - r0*r0;

	double arg = b*b - 4.0*a*c;
	if (arg >= 0.0) {
		double x, z, phi1, phi2, s = sqrt(b*b - 4.0*a*c);
		x = (-b + s) / (2.0*a);
		z = (-nx*x - ny*y0) / nz;
		phi1 = atan2(x, z);
		if (phi1 > phi0) phi1 -= PI2;
		Aan = phi0 - phi1;
		Tan = Aan / PI2 * rotT;

		x = (-b - s) / (2.0*a);
		z = (-nx*x - ny*y0) / nz;
		phi2 = atan2(x, z);
		if (phi2 > phi0) phi2 -= PI2;
		Adn = phi0 - phi2;
		Tdn = Adn / PI2 * rotT;

		double dT = (Tan > Tdn ? Tan - Tdn : Tan + rotT - Tdn);
		if (dT > 0.5*rotT) {
			double A = Aan; Aan = Adn; Adn = A;
			double T = Tan; Tan = Tdn; Tdn = T;
		}
		return true;
	}
	else { // no itersections of vessel trajectory with target plane
		// find points of closest approach instead
		double phi = atan2(-nm2.x, -nm2.z);
		if (phi > phi0) phi -= PI2;
		Aan = phi0 - phi;
		Tan = Aan / PI2 * rotT;
		Adn = 0.0;
		Tdn = 0.0;
		return false;
	}
}

bool Instrument_OPlaneAlign::KeyBuffered (DWORD key)
{
	switch (key) {
	case OAPI_KEY_A:  // auto reference
		SelectAutoRef ();
		return true;
	case OAPI_KEY_E:  // custom elements
		g_input->Open ("Ecliptic inclination and longitude of asc. node [deg.]:", 0, 30, Instrument_OPlaneAlign::CallbackElements, (void*)this);
		return true;
	case OAPI_KEY_R:  // select reference
		OpenSelect_CelBody ("Align MFD: Reference", ClbkEnter_Ref);
		return true;
	case OAPI_KEY_T:  // select target
		OpenSelect_Tgt ("Align MFD: Target", ClbkEnter_Tgt, elref, 0);
		return true;
	case OAPI_KEY_M:  // mode selection
		CycleModes();
		return true;
	}
	return false;
}

bool Instrument_OPlaneAlign::ProcessButton (int bt, int event)
{
	static const DWORD btkey[5] = { OAPI_KEY_R, OAPI_KEY_A, OAPI_KEY_T, OAPI_KEY_E, OAPI_KEY_M };
	if (event & PANEL_MOUSE_LBDOWN) {
		if (bt < 5) return KeyBuffered (btkey[bt]);
	}
	return false;
}

const char *Instrument_OPlaneAlign::BtnLabel (int bt) const
{
	static const char *label[5] = { "REF", "AR", "TGT", "ELS", "MOD" };
	return (bt < 5 ? label[bt] : 0);
}

int Instrument_OPlaneAlign::BtnMenu (const MFDBUTTONMENU **menu) const
{
	static const MFDBUTTONMENU mnu[5] = {
		{"Orbit reference", 0, 'R'},
		{"Auto reference", 0, 'A'},
		{"Select target", 0, 'T'},
		{"Select elements", 0, 'E'},
		{"Cycle modes", "Auto/Orbit/Surf", 'M'}
	};
	if (menu) *menu = mnu;
	return 5;
}

bool Instrument_OPlaneAlign::ClbkEnter_Ref (Select *menu, int item, char *str, void *data)
{
	Instrument_OPlaneAlign *mfd = (Instrument_OPlaneAlign*)data;
	return mfd->SelectRef (str);
}

bool Instrument_OPlaneAlign::ClbkEnter_Tgt (Select *menu, int item, char *str, void *data)
{
	Instrument_OPlaneAlign* mfd = (Instrument_OPlaneAlign*)data;
	return mfd->SelectTarget (str);
}

bool Instrument_OPlaneAlign::CallbackElements (InputBox*, char *str, void *data)
{
	Instrument_OPlaneAlign* instr = (Instrument_OPlaneAlign*)data;
	double i, theta;
	if (sscanf (str, "%lf%lf", &i, &theta) == 2) {
		instr->SetCustomEls (i*RAD, theta*RAD);
		return true;
	}
	return false;
}

bool Instrument_OPlaneAlign::SelectTarget (char *name)
{
	const Body *obj = g_psys->GetObj (name, true);
	if (obj && obj != elref) {
		SetTarget (obj);
		return true;
	}
	return false;
}

void Instrument_OPlaneAlign::CycleModes()
{
	if (automode) {
		automode = false;
		mode = ORBIT;
	}
	else if (mode == SURFACE) {
		automode = true;
	}
	else {
		mode = (Mode)(mode + 1);
	}
	Refresh();
}

int Instrument_OPlaneAlign::ProcessMessage (int msg, void *data)
{
	switch (msg) {
	case MSG_KILLVESSEL:
		if ((Body*)data == tgt) {
			tgt = 0; // current target object was destroyed
		}
		return 1;
	}
	return 0;
}

bool Instrument_OPlaneAlign::ReadParams (ifstream &ifs)
{
	if (!FindScnHeader (ifs)) return false;
	char cbuf[256], cref[128] = "", ctgt[128] = "", *pc;
	double i, theta;
	bool customels = false;
	for (;;) {
		if (!ifs.getline (cbuf, 256)) return false;
		pc = trim_string (cbuf);
		if (!_strnicmp (pc, "END_MFD", 7)) break;
		if (!_strnicmp (pc, "TARGET", 6)) {
			strcpy (ctgt, trim_string (pc+6));
		}
		else if (!_strnicmp(pc, "REF", 3)) {
			strcpy(cref, trim_string(pc + 3));
		} else if (!_strnicmp(pc, "MODE", 4)) {
			int m;
			sscanf(pc + 4, "%d", &m);
			mode = (Mode)m;
		} else if (!_strnicmp (pc, "TGTELS", 6)) {
			sscanf (pc+6, "%lf%lf", &i, &theta);
			customels = true;
		}
	}
	if (cref[0]) SelectRef (cref);
	if (ctgt[0]) SelectTarget (ctgt);
	if (customels) SetCustomEls (RAD*i, RAD*theta);
	return true;
}

void Instrument_OPlaneAlign::WriteParams (ostream &ofs) const
{
	ofs << "  TYPE OAlign" << endl;
	if (elref)
		ofs << "  REF " << elref->Name() << endl;
	if (mode != ORBIT)
		ofs << "  MODE " << (int)mode << endl;
	if (tgt)
		ofs << "  TARGET " << tgt->Name() << endl;
	else if (customel)
		ofs << "  TGTELS " << setprecision(4) << DEG*tgtel->i << ' ' << DEG*tgtel->theta << endl;
}

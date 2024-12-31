// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "MenuInfoBar.h"
#include "Pane.h"
#include "Camera.h"
#include "resource.h"
#include "Dialogs.h"
#include "DlgMgr.h"
#include "DlgMenuCfg.h"
#include "Log.h"
#include "GraphicsAPI.h"

using std::min;
using std::max;

// =======================================================================
// Externs

extern char DBG_MSG[256];
extern Orbiter *g_pOrbiter;
extern Camera *g_camera;
extern DWORD g_vtxcount;
extern DWORD g_tilecount;
extern TimeData td;

class MenuInfoBar;

// extra info mode ids
#define EXTRAINFO_NONE       0
#define EXTRAINFO_FRAMERATE  1
#define EXTRAINFO_RENDERSTAT 2
#define EXTRAINFO_VIEWPORT   3

const int tgtTexH = 128;
const int infoLine1 = tgtTexH-106;
const int infoLine2 = tgtTexH- 89;
const int infoLine3 = tgtTexH- 72;

const double blink_t = 0.8;
const double tmax = blink_t * 4.0;

// =======================================================================
// class ExtraInfoBar: base class for "extra info" bar modes
// =======================================================================

class ExtraInfoBar {
public:
	ExtraInfoBar (MenuInfoBar *_mibar, int side);
	virtual ~ExtraInfoBar ();
	virtual void Update (double t, bool sys_tick) {}
	virtual int Mode () const = 0;
	static ExtraInfoBar *Create (MenuInfoBar *_mibar, int side, int _mode);

protected:
	void Init ();
	inline int TexBltString (const char *str, int tgtx, int tgty, int cleantox, char *curstr = 0, int maxn=1024)
	{ return mibar->TexBltString (str, tgtx, tgty, cleantox, curstr, maxn); }

	MenuInfoBar *mibar;         // parent menu/info bar instance
	const Pane *pane;           // pane instance (should be replaced with UILayer instance)
	oapi::GraphicsClient *gc;   // client instance
	SURFHANDLE infoSrc, infoTgt;// texture surface for the info panes
	int texofs;                 // texture x-offset for left/right extra info bar
};

ExtraInfoBar::ExtraInfoBar (MenuInfoBar *_mibar, int side)
{
	mibar = _mibar;
	pane = mibar->pane;
	gc = mibar->gc;
	infoSrc = mibar->menuSrc;
	infoTgt = mibar->menuTgt;
	texofs = (side ? 400 : 205);
	Init ();
}

ExtraInfoBar::~ExtraInfoBar()
{
}

void ExtraInfoBar::Init ()
{
	gc->clbkBlt (infoTgt, texofs, 20, infoSrc, 800, 148, 196, 54); // clear
}

// =======================================================================
// class GraphInfoBar: base class for graph-based info modes
// =======================================================================

class GraphInfoBar: public ExtraInfoBar {
public:
	GraphInfoBar (MenuInfoBar *_mibar, int side);

protected:
	void Init ();
	double ScanSmax ();
	void LabelSmax (double smax);
	void RescaleGraph (double smax);
	void SetCurrentSample (double s);

	int nsample, isample;              // number of samples, current sample
	double smax, sample[FPSMAXSAMPLE]; // graph upper bound, sample array
	int bufofs;                        // x-offset of graph preparation buffer
};

GraphInfoBar::GraphInfoBar (MenuInfoBar *_mibar, int side)
: ExtraInfoBar (_mibar, side)
{
	bufofs = 1024-FPSMAXSAMPLE*(side+1);
	Init ();
}

void GraphInfoBar::Init ()
{
	nsample = isample = 0; // number of samples and current sample index
	smax = 1.0;            // graph range
	gc->clbkBeginBltGroup (infoSrc);
	for (int i = 0; i < FPSMAXSAMPLE; i++)                           // graph background
		gc->clbkBlt (infoSrc, bufofs+i,96,infoTgt,1023,2,1,48);
	gc->clbkEndBltGroup ();
	gc->clbkBeginBltGroup (infoTgt);
	gc->clbkBlt (infoTgt, texofs+99, infoLine1, infoSrc, 1021, 2, 1, 48);  // left graph frame
	gc->clbkBlt (infoTgt, texofs+160, infoLine1, infoSrc, 1021, 2, 1, 48); // right graph frame
	gc->clbkBlt (infoTgt,texofs+100,infoLine1,infoSrc,bufofs,96,FPSMAXSAMPLE,48); // copy into info texture area
	gc->clbkBlt (infoTgt,texofs+102+FPSMAXSAMPLE,tgtTexH-66,infoSrc,857,0,6,8);    // '0' (lower bound)
	gc->clbkEndBltGroup ();
	LabelSmax (smax);                                                       // upper bound
}

double GraphInfoBar::ScanSmax ()
{
	int i;
	double s = 1.0;
	for (i = 0; i < nsample; i++)
		if (sample[i] > s) s = sample[i];
	if (s <= 100.0) {
		return ceil(s*0.1)*10.0;
	} else if (s <= 1e3) {
		return ceil(s*0.01)*100.0;
	} else if (s <= 1e4) {
		return ceil(s*1e-3)*1e3;
	} else {
		s *= 1e-4;
		int e = 4;
		while (s >= 10.0) {
			s *= 0.1;
			e++;
		}
		return ceil(s) * pow(10.0,(double)e);
	}
}

void GraphInfoBar::LabelSmax (double smax)
{
	char cbuf[32];
	if (smax < 1e4) {
		sprintf (cbuf, "%0.0f", smax);
		for (char *c = cbuf; *c; c++)
			*c += 128-'0';
	} else {
		double smax0 = smax; // DEBUG
		smax = (smax-0.5)*1e-4;
		int e = 4;
		while (smax >= 10.0) {
			smax *= 0.1;
			e++;
		}
		int a = (int)(ceil(smax)+0.5);
		if (a == 10) { a = 1; e++; }
		sprintf (cbuf, "%de%d", a, e);
		for (char *c = cbuf; *c; c++) {
			if (*c == 'e') *c = '\036';
			else *c += 128-'0';
		}
	}
	TexBltString (cbuf, texofs+102+FPSMAXSAMPLE,infoLine1,texofs+194);
}

void GraphInfoBar::RescaleGraph (double smax)
{
	gc->clbkBeginBltGroup (infoSrc);
	int i, i0 = 0, i1, n, y0, y1, h;
	for (n = nsample-1, i = isample; n > 0; n--, i--) {
		i1 = i;
		if (i1 < 0) i1 += FPSMAXSAMPLE;
		i0 = i1-1;
		if (i0 < 0) i0 += FPSMAXSAMPLE;
		y0 = 145-(int)(sample[i0]/smax*46);
		y1 = 145-(int)(sample[i1]/smax*46);
		if (y1 < y0) { int tmp = y0; y0 = y1; y1 = tmp; }
		h = min (max (y1-y0,1), 46);
		gc->clbkBlt (infoSrc, bufofs+i1,96,infoTgt,1023,2,1,48);
		gc->clbkBlt (infoSrc, bufofs+i1,y0,infoTgt,1022,2,1,h);
	}
	y0 = 145-(int)(sample[i0]/smax*46);
	gc->clbkBlt (infoSrc, bufofs+i0,96,infoTgt,1023,2,1,48);
	gc->clbkBlt (infoSrc, bufofs+i0,y0,infoTgt,1022,2,1,1);
	gc->clbkEndBltGroup ();
	LabelSmax (smax);
}

void GraphInfoBar::SetCurrentSample (double s)
{
	if (nsample < FPSMAXSAMPLE) nsample++;
	sample[isample] = s;

	if (nsample >= 2) {
		double mf = ScanSmax();
		if (mf != smax)
			RescaleGraph (smax = mf);
		double s0 = sample[isample > 0 ? isample-1:FPSMAXSAMPLE-1];
		int y0 = 145-(int)(s0/smax*46);
		int y1 = 145-(int)(s/smax*46);
		if (y1 < y0) { int tmp = y0; y0 = y1; y1 = tmp; }
		int h = min (max (y1-y0,1), 46);
		gc->clbkBeginBltGroup (infoSrc);
		gc->clbkBlt (infoSrc, bufofs+isample,96,infoTgt,1023,2,1,48);
		gc->clbkBlt (infoSrc, bufofs+isample,y0,infoTgt,1022,2,1,h);
		gc->clbkEndBltGroup ();
		gc->clbkBeginBltGroup (infoTgt);
		if (isample < FPSMAXSAMPLE-1)
			gc->clbkBlt (infoTgt, texofs+100,infoLine1,infoSrc,bufofs+isample+1,96,FPSMAXSAMPLE-isample-1,48);
		if (isample > 0)
			gc->clbkBlt (infoTgt, texofs+100+FPSMAXSAMPLE-isample-1,infoLine1,infoSrc,bufofs,96,isample+1,48);
		gc->clbkEndBltGroup ();
	}
	if (++isample == FPSMAXSAMPLE) isample = 0;
}

// =======================================================================
// class ViewportInfoBar: render device specs (window size, bpp, T&L)
// =======================================================================

class ViewportInfoBar: public ExtraInfoBar {
public:
	ViewportInfoBar (MenuInfoBar *_mibar, int side);
	int Mode () const { return EXTRAINFO_VIEWPORT; }

protected:
	void Init ();
};

ViewportInfoBar::ViewportInfoBar (MenuInfoBar *_mibar, int side)
: ExtraInfoBar (_mibar, side)
{
	Init();
}

void ViewportInfoBar::Init ()
{
	char cbuf[128];
	sprintf (cbuf, "Viewport: %dx%d, %dbpp", pane->Width(), pane->Height(), pane->BitsPerPixel());
	TexBltString (cbuf, texofs+5, infoLine1, texofs+190);
	DWORD is_tl;
	strcpy (cbuf, "T&L support: ");
	if (gc->clbkGetRenderParam (RP_ISTLDEVICE, &is_tl))
		strcat (cbuf, is_tl ? "yes" : "no");
	else strcat (cbuf, "unknown");
	TexBltString (cbuf, texofs+5, infoLine2, texofs+190);
}

// =======================================================================
// class RenderstatInfoBar: render statistics
// (vertices, surface tiles in last frame)
// =======================================================================

class RenderstatInfoBar: public GraphInfoBar {
public:
	RenderstatInfoBar (MenuInfoBar *_mibar, int side);
	int Mode () const { return EXTRAINFO_RENDERSTAT; }
	void Update (double t, bool sys_tick);

protected:
	void Init ();

private:
	int framecount;
	DWORD vtxsum;
};

RenderstatInfoBar::RenderstatInfoBar (MenuInfoBar *_mibar, int side)
: GraphInfoBar (_mibar, side)
{
	Init();
}

void RenderstatInfoBar::Init ()
{
	framecount = 0;
	vtxsum = 0;
	TexBltString ("Vtx", texofs+5, infoLine1, texofs+35);
	TexBltString ("Tile", texofs+5, infoLine2, texofs+35);
}

void RenderstatInfoBar::Update (double t, bool sys_tick)
{
	framecount++;
	vtxsum += g_vtxcount;
	if (sys_tick && framecount) {
		char cbuf[32];
		DWORD nvtx = vtxsum/framecount;
		SetCurrentSample (nvtx);
		sprintf (cbuf, "%d", g_vtxcount);
		TexBltString (cbuf, texofs+35, infoLine1, texofs+98);
		sprintf (cbuf, "%d", g_tilecount);
		TexBltString (cbuf, texofs+35, infoLine2, texofs+98);
		framecount = 0;
		vtxsum = 0;
	}
}

// =======================================================================
// class FramerateInfoBar: performance stats
// (frames/second, sim time interval/frame)
// =======================================================================

class FramerateInfoBar: public GraphInfoBar {
public:
	FramerateInfoBar (MenuInfoBar *_mibar, int side);
	int Mode () const { return EXTRAINFO_FRAMERATE; }
	void Update (double t, bool sys_tick);

protected:
	void Init ();

private:
	int framecount;
	double sysdt, simdt;
};

FramerateInfoBar::FramerateInfoBar (MenuInfoBar *_mibar, int side)
: GraphInfoBar (_mibar, side)
{
	Init ();
}

void FramerateInfoBar::Init ()
{
	framecount = 0;
	sysdt = simdt = 0.0;
	TexBltString ("F/s", texofs+5, infoLine1, texofs+35);
	TexBltString ("\037t/F", texofs+5, infoLine2, texofs+35);
}

void FramerateInfoBar::Update (double t, bool sys_tick)
{
	framecount++;
	sysdt += td.SysDT;
	simdt += td.SimDT;
	if (sys_tick && framecount) {
		char cbuf[32];
		double fps = framecount/sysdt;
		SetCurrentSample (fps);
		sprintf (cbuf, "%0.*f", fps < 100 ? 1:0, fps);
		TexBltString (cbuf, texofs+35, infoLine1, texofs+98);
		double dt = simdt/framecount;
		sprintf (cbuf, "%0.*fs", dt < 1e-1 ? 3 : dt < 1 ? 2 : dt < 10 ? 1 : 0, dt);
		TexBltString (cbuf, texofs+35, infoLine2, texofs+98);
		framecount = 0;
		sysdt = simdt = 0.0;
	}
}

// =======================================================================

ExtraInfoBar *ExtraInfoBar::Create (MenuInfoBar *_mibar, int side, int mode)
{
	switch (mode) {
	case EXTRAINFO_FRAMERATE:
		return new FramerateInfoBar (_mibar, side);
	case EXTRAINFO_RENDERSTAT:
		return new RenderstatInfoBar (_mibar, side);
	case EXTRAINFO_VIEWPORT:
		return new ViewportInfoBar (_mibar, side);
	default:
		return NULL;
	}
}


// =======================================================================
// class MenuInfoBar
// =======================================================================

MenuInfoBar::MenuInfoBar (const Pane *_pane)
{
	pane = _pane;
	gc   = pane->gc;
	time_upd = -1;
	sys_upd = 0;
	paused = recording = playback = false;
	show_action = action_blink = false;
	action_time = 0.0;
	warp = 1.0;
	datestr[0] = mjdstr[0] = timestr[0] = '\0';
	tgtstr[0] = dststr[0] = camstr[0] = recstr[0] = '\0';
	viewW = pane->W;
	itemN = 12;
	itemW = 48;
	flagW = 68;
	flagH = 54;
	itemHighlight = -1;
	menuW = itemN*itemW;
	menuH = 54;
	infoW = 200;

	scrollzone = (gc->clbkFullscreenMode() ? 0:16);
	scrollspeed = g_pOrbiter->Cfg()->CfgUIPrm.MenuScrollspeed;
	scrollrange = (g_pOrbiter->Cfg()->CfgUIPrm.bMenuLabelOnly ? 17 : menuH);
	opacity = g_pOrbiter->Cfg()->CfgUIPrm.MenuOpacity;
	opacity_info = g_pOrbiter->Cfg()->CfgUIPrm.InfoOpacity;
	menumode = g_pOrbiter->Cfg()->CfgUIPrm.MenuMode;
	infomode = g_pOrbiter->Cfg()->CfgUIPrm.InfoMode;
	pausemode = g_pOrbiter->Cfg()->CfgUIPrm.PauseIndMode;
	fixedstep = (td.FixedStep() > 0.0);
	warp_always = g_pOrbiter->Cfg()->CfgUIPrm.bWarpAlways;
	warp_scientific = g_pOrbiter->Cfg()->CfgUIPrm.bWarpScientific;
	menustate = (menumode == 0 ? 1:0);
	infostate = (infomode == 0 ? 1:0);
	scrollpos = (menumode == 0 ? scrollrange : 0.0);
	scrollpos_info = (infomode == 0 ? menuH : 0.0);
	scrolldir = scrolldir_info = 0;
#ifdef INLINEGRAPHICS
	menuSrc = gc->clbkLoadTexture("main_menu.dds", 0x4);
	menuTgt = gc->clbkLoadTexture("main_menu_tgt.dds", 0x4);
#else
	menuSrc = gc->clbkLoadSurface("main_menu.dds", OAPISURFACE_RENDERTARGET | OAPISURFACE_TEXTURE);
	menuTgt = gc->clbkLoadSurface("main_menu_tgt.dds", OAPISURFACE_RENDERTARGET | OAPISURFACE_TEXTURE);
#endif
	dCHECK(menuSrc && menuTgt, "MenuInfoBar: main_menu.dds or main_menu_tgt.dds could not be loaded from Textures directory.")
	gc->clbkBlt (menuTgt, 0, tgtTexH-menuH, menuSrc, 0, 23+menuH, menuW, menuH);
	int yofs = (menumode == 0 ? -menuH+scrollrange:-menuH);
	int yofs_info = (infomode == 0 ? 0:-menuH);
	int miniW = 100, miniH = menuH/3;
	float xofs_flag = viewW-flagW-20.5f, yofs_flag = menuH+20.5f;

	NTVERTEX vtx[24] = {
		// menu bar
		{0,(float)yofs        ,0, 0,0,0, 0,0.0020f},
		{0,(float)(yofs+menuH),0, 0,0,0, 0,0.0020f},
		{0,(float)yofs        ,0, 0,0,0, 0,0.0020f},
		{0,(float)(yofs+menuH),0, 0,0,0, 0,0.0020f},
		{0,(float)yofs        ,0, 0,0,0, 0,(tgtTexH-menuH-0.5f)/(float)tgtTexH},
		{0,(float)(yofs+menuH),0, 0,0,0, 0,(tgtTexH-0.5f)/(float)tgtTexH},
		{0,(float)yofs        ,0, 0,0,0, 0,(tgtTexH-menuH-0.5f)/(float)tgtTexH},
		{0,(float)(yofs+menuH),0, 0,0,0, 0,(tgtTexH-0.5f)/(float)tgtTexH},

		// left info bar
		{0,(float)yofs_info,        0, 0,0,0, 0,0.0020f},
		{0,(float)(yofs_info+menuH),0, 0,0,0, 0,0.0020f},
		{0,(float)yofs_info,        0, 0,0,0, 0,0.0020f},
		{0,(float)(yofs_info+menuH),0, 0,0,0, 0,0.0020f},
		{0,(float)yofs_info,        0, 0,0,0, 0,(tgtTexH-2*menuH-0.5f)/(float)tgtTexH},
		{0,(float)(yofs_info+menuH),0, 0,0,0, 0,(tgtTexH-menuH-0.5f)/(float)tgtTexH},
		{0,(float)yofs_info,        0, 0,0,0, 0,(tgtTexH-2*menuH-0.5f)/(float)tgtTexH},
		{0,(float)(yofs_info+menuH),0, 0,0,0, 0,(tgtTexH-menuH-0.5f)/(float)tgtTexH},

		// right info bar
		{0,(float)yofs_info,        0, 0,0,0, 0,0.0020f},
		{0,(float)(yofs_info+menuH),0, 0,0,0, 0,0.0020f},
		{0,(float)yofs_info,        0, 0,0,0, 0,0.0020f},
		{0,(float)(yofs_info+menuH),0, 0,0,0, 0,0.0020f},
		{0,(float)yofs_info,        0, 0,0,0, 0,(tgtTexH-2*menuH-0.5f)/(float)tgtTexH},
        {0,(float)(yofs_info+menuH),0, 0,0,0, 0,(tgtTexH-menuH-0.5f)/(float)tgtTexH},
        {0,(float)yofs_info,        0, 0,0,0, 0,(tgtTexH-2*menuH-0.5f)/(float)tgtTexH},
		{0,(float)(yofs_info+menuH),0, 0,0,0, 0,(tgtTexH-menuH-0.5f)/(float)tgtTexH}
	};

	NTVERTEX minivtx[8] = {
		{(float)(viewW-miniW),0           ,0, 0,0,0, 0,0.0020f},
		{(float)(viewW-miniW),(float)miniH,0, 0,0,0, 0,0.0020f},
		{(float)viewW,        0           ,0, 0,0,0, 0,0.0020f},
		{(float)viewW,        (float)miniH,0, 0,0,0, 0,0.0020f},
		{(float)(viewW-miniW),0           ,0, 0,0,0, 700.5f/1024.0f,(tgtTexH-menuH-miniH-0.5f)/(float)tgtTexH},
		{(float)(viewW-miniW),(float)miniH,0, 0,0,0, 700.5f/1024.0f,(tgtTexH-menuH-0.5f)/(float)tgtTexH},
		{(float)viewW,        0           ,0, 0,0,0, 800.5f/1024.0f,(tgtTexH-menuH-miniH-0.5f)/(float)tgtTexH},
		{(float)viewW,        (float)miniH,0, 0,0,0, 800.5f/1024.0f,(tgtTexH-menuH-0.5f)/(float)tgtTexH}
	};

	WORD idx[36] = {
		0,2,1, 1,2,3, 4,6,5, 5,6,7,            // menu bar
		8,10,9, 9,10,11, 12,14,13, 13,14,15,   // left info bar
		16,18,17, 17,18,19, 20,22,21, 21,22,23 // right info bar
	};
	barMesh.AddGroup (vtx, 24, idx, 36, 0, 0, 0, 0, true);
	miniMesh.AddGroup (minivtx, 8, idx, 12, 0, 0, 0, 0, true);
	transf = identity();
	eibar[0] = eibar[1] = NULL;
	UpdateMeshVertices();
	for (int i = 0; i < 2; i++)
		SetAuxInfobar (i, g_pOrbiter->Cfg()->CfgUIPrm.InfoAuxIdx[i]);

	// Add a mesh for the pause/run indicator
	int tw = 1024, th = 128;
	NTVERTEX flagvtx[4] = {
		{(float)xofs_flag,         (float)yofs_flag,         0, 0,0,0, (float)(tw-flagW)/(float)tw, (float)(th-flagH)/(float)th},
		{(float)xofs_flag,         (float)(yofs_flag+flagH), 0, 0,0,0, (float)(tw-flagW)/(float)tw, 1.0f},
		{(float)(xofs_flag+flagW), (float)yofs_flag,         0, 0,0,0, (float)tw/(float)tw, (float)(th-flagH)/(float)th},
		{(float)(xofs_flag+flagW), (float)(yofs_flag+flagH), 0, 0,0,0, (float)tw/(float)tw, 1.0f}
	};
	flagMesh.AddGroup (flagvtx, 4, idx, 6, 0, 0, 0, 0, true);

	SetFOV (0);
	SetWarp (td.Warp());
}

MenuInfoBar::~MenuInfoBar ()
{
	for (int i = 0; i < 2; i++)
		if (eibar[i]) delete eibar[i];
	gc->clbkReleaseSurface (menuSrc);
	gc->clbkReleaseTexture (menuTgt);
}

void MenuInfoBar::Update (double t)
{
	if (scrolldir) {
		double dy = (scrollspeed == 20 ? menuH: td.SysDT * (scrollspeed*30+2));
		double y = scrollpos + dy*scrolldir;
		if (y < 0.0) {
			y = 0.0;
			scrolldir = 0;
			menustate = 0;
		} else if (y > scrollrange) {
			y = scrollrange;
			scrolldir = 0;
			menustate = 1;
		}
		scrollpos = y;
		static const float y0[8] = {-(float)menuH,0,-(float)menuH,0,-(float)menuH,0,-(float)menuH,0};
		int i;
		GroupSpec *grp = barMesh.GetGroup (0);
		NTVERTEX *vtx = grp->Vtx;
		for (i = 0; i < 8; i++) vtx[i].y = y0[i] + (float)y;
	}

	if (scrolldir_info) {
		double dy = (scrollspeed == 20 ? menuH: td.SysDT * (scrollspeed*30+2));
		double y = scrollpos_info + dy*scrolldir_info;
		if (y < 0.0) {
			y = 0.0;
			scrolldir_info = 0;
			infostate = 0;
		} else if (y > menuH) {
			y = menuH;
			scrolldir_info = 0;
			infostate = 1;
		}
		scrollpos_info = y;
		static const float y0[8] = {-(float)menuH,0,-(float)menuH,0,-(float)menuH,0,-(float)menuH,0};
		int i, j;
		GroupSpec *grp = barMesh.GetGroup (0);
		NTVERTEX *vtx = grp->Vtx+8;
		for (j = 0; j < 2; j++)
			for (i = 0; i < 8; i++, vtx++) vtx->y = y0[i] + (float)y;
	}

	// Update the info boxes
	int it = (int)t, sit = (int)td.SysT1;
	bool tick_one, sys_one;
	char cbuf[256];
	if (tick_one = (it != time_upd))  time_upd = it;
	if (sys_one = (sit != sys_upd)) sys_upd = sit;

	if (tick_one) {
		strcpy  (cbuf, DateStr (td.MJD1));
		if (strcmp (cbuf, datestr))
			TexBltString (cbuf, 604, infoLine1, 800, datestr);

		sprintf (cbuf, "%0.4f", td.MJD1);
		if (strcmp (cbuf, mjdstr))
			TexBltString (cbuf, 638, infoLine2, 714, mjdstr, 29);

		if (t < 1e7) sprintf (cbuf, "%0.0fs", t);
		else         sprintf (cbuf, "...%07.0fs", fmod (t, 1e7));
		if (strcmp (cbuf, timestr))
			TexBltString (cbuf, 638, infoLine3, 714, timestr, 29);

		Body *tgt = g_camera->Target();
		if (strncmp (tgt->Name(), tgtstr, 63))
			TexBltString (tgt->Name(), 36, infoLine1, 200, tgtstr, 63);
	}
	if (g_camera->IsExternal()) {
		switch (g_camera->GetExtMode()) {
		case CAMERA_TARGETRELATIVE:   strcpy (cbuf, "track (rel-pos)"); break;
		case CAMERA_ABSDIRECTION:     strcpy (cbuf, "track (abs-dir)"); break;
		case CAMERA_GLOBALFRAME:      strcpy (cbuf, "track (global frame)"); break;
		case CAMERA_TARGETTOOBJECT:   strcpy (cbuf, "target to "); strcat (cbuf, g_camera->GetDirRef()->Name()); break;
		case CAMERA_TARGETFROMOBJECT: strcpy (cbuf, "target from "); strcat (cbuf, g_camera->GetDirRef()->Name()); break;
		case CAMERA_GROUNDOBSERVER:   
			strcpy (cbuf, "ground ");
			strcat (cbuf, g_camera->GroundObserver_TargetLock() ? "(tgt-lock)" : "(free)");
			break;
		}
	} else {
		strcpy (cbuf, "Cockpit");
	}
	if (strncmp (cbuf, camstr, 63))
		TexBltString (cbuf, 36, infoLine2, 200, camstr, 93);
	if (g_camera->IsExternal())
		sprintf (cbuf, "Dst %s", DistStr (g_camera->Distance())+1);
	else
		cbuf[0] = '\0';
	if (strncmp (cbuf, dststr, 29))
		TexBltString (cbuf, 69, infoLine3, 199, dststr, 29);
	if (recording) {
		bool blink = (fmod (td.SysT1, 1.0) < 0.5);
		TexBltString (blink ? "\003":"\004", 714, infoLine2, 744, recstr, 1);
	}

	// update auxiliary info bars
	for (int i = 0; i < 2; i++)
		if (eibar[i]) eibar[i]->Update (t, sys_one);

	// update action blinker
	if (show_action) {
		if (pausemode == 1) {
			action_blink = true;
		} else {
			double dt = td.SysT0-action_time;
			if (dt > tmax) {
				action_blink = show_action = false;
			} else {
				double s = fmod (dt, blink_t);
				action_blink = (s < blink_t*0.5);
			}
		}
	}
}

bool MenuInfoBar::ProcessMouse (UINT event, DWORD state, DWORD x, DWORD y)
{
	x = (DWORD)(x / transf.m11); // account for menu squeezing

	if (event == WM_MOUSEMOVE) {
		if (menumode == 2) {
			if (y <= scrollzone && menustate != 1) {
				menustate = 2;
				scrolldir = 1;
			} else if (menustate != 0 && y >= scrollpos) {
				menustate = 2;
				scrolldir = -1;
			}
		}
		if (infomode == 2) {
			if (y <= scrollzone && infostate != 1) {
				infostate = 2;
				scrolldir_info = 1;
			} else if (infostate != 0 && y >= scrollpos) {
				infostate = 2;
				scrolldir_info = -1;
			}
		}
		int item = -1;
		if (y < scrollpos && x >= menuX && x < menuX+menuW)
			item = (x-menuX)/itemW;
		if (item != itemHighlight) {
			if (itemHighlight >= 0)
				pane->gc->clbkBlt (menuTgt, itemHighlight*itemW, tgtTexH-menuH, menuSrc, itemHighlight*itemW, 23+menuH, itemW, menuH);
			if (item >= 0)
				pane->gc->clbkBlt (menuTgt, item*itemW, tgtTexH-menuH, menuSrc, item*itemW, 23, itemW, menuH);
			itemHighlight = item;
		}
	}
	if (event == WM_LBUTTONDOWN && itemHighlight >= 0) {
		switch (itemHighlight) {
		case 0:
			g_pOrbiter->DlgMgr()->EnsureEntry<DlgFocus> ();
			return true;
		case 1:
			g_pOrbiter->DlgMgr()->EnsureEntry<DlgCamera> ();
			return true;
		case 2:
			g_pOrbiter->DlgMgr()->EnsureEntry<DlgTacc> ();
			return true;
		case 3:
			g_pOrbiter->TogglePause();
			return true;
		case 4:
			g_pOrbiter->DlgMgr()->EnsureEntry<DlgInfo> ();
			return true;
		case 5:
			g_pOrbiter->DlgMgr()->EnsureEntry<DlgFunction> ();
			return true;
		case 6:
			g_pOrbiter->DlgMgr()->EnsureEntry<DlgOptions> ();
			return true;
		case 7:
			g_pOrbiter->DlgMgr()->EnsureEntry<DlgMap> ();
			return true;
		case 8:
			g_pOrbiter->DlgMgr()->EnsureEntry<DlgRecorder> ();
			return true;
		case 9:
			extern HELPCONTEXT DefHelpContext;
			DefHelpContext.topic = (char*)"/mainmenu.htm";
			g_pOrbiter->OpenHelp (&DefHelpContext);
			return true;
		case 10:
			g_pOrbiter->Quicksave ();
			return true;
		case 11:
			g_pOrbiter->PreCloseSession();
			DestroyWindow (g_pOrbiter->GetRenderWnd());
			return true;
		}
	}
	if (event == WM_RBUTTONDOWN && y <= scrollpos && x >= menuX && x < menuX+menuW) {
		g_pOrbiter->DlgMgr()->EnsureEntry<DlgMenuCfg> ();
		return true;
	}
	return false;
}

void MenuInfoBar::Render ()
{
	gc->clbkRender2DPanel (&menuTgt, (MESHHANDLE)&barMesh, &transf, false);
	if (warp_always && scrollpos_info < 17 && (warp != 1.0 || paused)) {
		float scalx = (float)transf.m11;
		transf.m11 = 1.0; // undo squeeze
		gc->clbkRender2DPanel (&menuTgt, (MESHHANDLE)&miniMesh, &transf, false);
		transf.m11 = scalx;
	}
	if (action_blink) {
		float scalx = (float)transf.m11;
		transf.m11 = 1.0; // undo squeeze
		gc->clbkRender2DPanel (&menuTgt, (MESHHANDLE)&flagMesh, &transf, false);
		transf.m11 = scalx;
	}
}

void MenuInfoBar::SetFOV (double fov)
{
	char cbuf[32];
	sprintf (cbuf, "%0.0f\272", 2.0*Deg(fov)); // convert to degrees (full angle)
	TexBltString (cbuf, 36, infoLine3, 69);
}

void MenuInfoBar::SetWarp (double _warp)
{
	char cbuf[32];
	warp = _warp;
	if (fixedstep)
		sprintf (cbuf, "(%gs/f)", td.FixedStep()*warp);
	else if (fabs(warp-1.0) > 1e-6) {
		if (!warp_scientific || warp < 100.0) {
			sprintf (cbuf, "\002 %0.*fx", warp < 9.99 ? 1:0, warp);
		} else {
			char tmp[32], *cs = tmp, *ct = cbuf;
			sprintf (tmp, "%0.1e", warp);
			*ct++ = '\002'; *ct++ = ' ';
			if (strncmp (tmp, "1.0", 3)) {
				*ct++ = *cs++;
				if (cs[1] != '0') {
					*ct++ = *cs++;
					*ct++ = *cs++;
				} else {
					cs += 2;
				}
				*ct++ = '\271';
			} else cs += 3;
			*ct++ = '1';
			*ct++ = '0';
			cs++; // skip 'e'
			while (*cs == '+' || *cs == '0') cs++; // skip leading + and 0
			while (*cs != '\0') *ct++ = *cs++ -'0'+128;
			*ct++ = 'x';
			*ct++ = '\0';
		}
	}
	else cbuf[0] = '\0';
	TexBltString (cbuf, 714, infoLine3, 804);
}

void MenuInfoBar::SetWarpAlways (bool on)
{
	warp_always = on;
}

void MenuInfoBar::SetWarpScientific (bool scientific)
{
	if (scientific != warp_scientific) {
		warp_scientific = scientific;
		SetWarp (warp);
	}
}

void MenuInfoBar::SetPaused (bool _paused)
{
	paused = _paused;
	show_action = (pausemode == 0 || pausemode == 1 && (paused || recording || playback));
	action_time = td.SysT0;
	if (paused)
		TexBltString ("\001 Pause", 714, infoLine3, 804);
	else
		SetWarp (warp);

	gc->clbkBlt (menuTgt, 1024-flagW, 128-flagH, menuSrc, 1024-(paused ? 2*flagW : recording ? 3*flagW : playback ? 4*flagW : flagW), 256-flagH, flagW, flagH);
	action_blink = show_action;
}

void MenuInfoBar::SetRecording (bool rec)
{
	recording = rec;
	show_action = pausemode != 2 && rec;
	action_time = td.SysT0;
	if (!rec) TexBltString ("", 714, infoLine2, 744, recstr, 1);

	if (recording) {
		gc->clbkBlt (menuTgt, 1024-flagW, 128-flagH, menuSrc, 1024-3*flagW, 256-flagH, flagW, flagH);
		action_blink = show_action;
	} else
		action_blink = false;
}

void MenuInfoBar::SetPlayback (bool pback)
{
	playback = pback;
	SetRecording (false); // sanity
	show_action = (pausemode == 0 || pausemode == 1 && playback);
	action_time = td.SysT0;
	TexBltString (playback ? "\005":"", 714, infoLine2, 744, recstr, 1);

	gc->clbkBlt (menuTgt, 1024-flagW, 128-flagH, menuSrc, 1024-(playback ? 4*flagW : flagW), 256-flagH, flagW, flagH);
	action_blink = show_action;
}

void MenuInfoBar::ToggleAutohide ()
{
	SetMenuMode (menumode == 0 ? 2:0);
}

void MenuInfoBar::SetMenuMode (DWORD mode)
{
	if (menumode != mode) {
		menumode = mode;
		if (mode != 0) {
			if (menustate != 0) scrolldir = -1;
		} else if (mode != 1) {
			if (menustate != 1) scrolldir = 1;
		}
		g_pOrbiter->Cfg()->CfgUIPrm.MenuMode = mode;
	}
}

void MenuInfoBar::SetInfoMode (DWORD mode)
{
	if (infomode != mode) {
		infomode = mode;
		if (mode != 0) {
			if (infostate != 0) scrolldir_info = -1;
		} else if (mode != 1) {
			if (infostate != 1) scrolldir_info = 1;
		}
		g_pOrbiter->Cfg()->CfgUIPrm.InfoMode = mode;
	}
}

void MenuInfoBar::SetPauseIndicatorMode (DWORD mode)
{
	if (pausemode != mode) {
		pausemode = mode;
		switch (pausemode) {
		case 0:
			show_action = true; //td.SysT0-action_time < tmax;
			action_time = td.SysT0;
			break;
		case 1:
			show_action = action_blink = (paused || recording || playback);
			break;
		case 2:
			show_action = action_blink = false;
			break;
		}
	}
}

void MenuInfoBar::SetOpacity (int opac)
{
	if (opac != opacity) {
		opacity = opac;
		GroupSpec *grp = barMesh.GetGroup (0);
		NTVERTEX *vtx = grp->Vtx;
		for (int i = 0; i < 4; i++)
			vtx[i].tu = (float)((1013.5+opacity)/1024.0);
	}
}

void MenuInfoBar::SetOpacityInfo (int opac)
{
	if (opac != opacity_info) {
		opacity_info = opac;
		GroupSpec *grp = barMesh.GetGroup (0);
		NTVERTEX *vtx = grp->Vtx+8;
		int i;
		for (i = 0; i < 4; i++)
			vtx[i].tu = (float)((1013.5+opacity_info)/1024.0);
		for (i = 8; i < 12; i++)
			vtx[i].tu = (float)((1013.5+opacity_info)/1024.0);
		grp = miniMesh.GetGroup (0);
		vtx = grp->Vtx;
		for (i = 0; i < 4; i++)
			vtx[i].tu = (float)((1013.5+opacity_info)/1024.0);
	}
}

void MenuInfoBar::SetScrollspeed (int speed)
{
	if (speed != scrollspeed) {
		scrollspeed = speed;
	}
}

void MenuInfoBar::SetLabelOnly (bool labelonly)
{
	scrollrange = (labelonly ? 17 : menuH);
	if (labelonly) {
		if (scrollpos > scrollrange)
			scrolldir = -1;
	} else {
		if (menustate != 0)
			scrolldir = 1;
	}
}

void MenuInfoBar::SetAuxInfobar (int side, DWORD idx)
{
	DWORD curidx = (eibar[side] ? eibar[side]->Mode() : 0);
	if (idx != curidx) {
		if (eibar[side]) delete eibar[side];
		eibar[side] = ExtraInfoBar::Create (this, side, idx);
		UpdateMeshVertices();
	}
}

void MenuInfoBar::UpdateMeshVertices ()
{
	int i;
	GroupSpec *gs = barMesh.GetGroup(0);
	NTVERTEX *vtx = gs->Vtx;
	const int nvtx = 24;

	linfoW = (eibar[0] ? infoW*2:infoW);
	rinfoW = (eibar[1] ? infoW*2:infoW);
	menuX = linfoW+(viewW-menuW-linfoW-rinfoW)/2;
	float barW = (float)viewW;
	double scalx = 1.0;
	if (viewW-menuW-linfoW-rinfoW < 10) {
		scalx = (viewW-10.0)/(menuW+linfoW+rinfoW);
		barW /= (float)scalx;
		menuX = linfoW+5;
	}
	transf.m11 = scalx;
	float x[nvtx] = {
		(float)menuX, (float)menuX, (float)(menuX+menuW), (float)(menuX+menuW),
		(float)menuX, (float)menuX, (float)(menuX+menuW), (float)(menuX+menuW),
		0, 0, (float)linfoW, (float)linfoW,
		0, 0, (float)linfoW, (float)linfoW,
		(float)(barW-rinfoW),(float)(barW-rinfoW),(float)barW,(float)barW,
		(float)(barW-rinfoW),(float)(barW-rinfoW),(float)barW,(float)barW
	};
	float menu_opac = (float)((1013.5+opacity)/1024.0);
	float info_opac = (float)((1013.5+opacity_info)/1024.0);
	float tu[nvtx] = {
		menu_opac,menu_opac,menu_opac,menu_opac,
		4.8828e-004f,4.8828e-004f,(menuW+0.5f)/1024.0f,(menuW+0.5f)/1024.0f,
		info_opac,info_opac,info_opac,info_opac,
		0.5f/1024.0f,0.5f/1024.0f,(linfoW+0.5f)/1024.0f,(linfoW+0.5f)/1024.0f,
		info_opac,info_opac,info_opac,info_opac,
		(infoW*4-rinfoW+0.5f)/1024.0f,(infoW*4-rinfoW+0.5f)/1024.0f,(infoW*4+0.5f)/1024.0f,(infoW*4+0.5f)/1024.0f
	};
	for (i = 0; i < nvtx; i++) {
		vtx[i].x = x[i];
		vtx[i].tu = tu[i];
	}

	gs = miniMesh.GetGroup(0);
	vtx = gs->Vtx;
	for (i = 0; i < 4; i++)
		vtx[i].tu = info_opac;
}

int MenuInfoBar::TexBltString (const char *str, int tgtx, int tgty, int cleantox, char *curstr, int maxn)
{
	static const int x0[256] = {
		0,955/*(paused)*/,974/*(ffwd)*/,936/*(rec)*/,917/*(empty)*/,993/*(playback)*/,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,29/*supE*/,36/*Delta*/,
		47/* */,51/*!*/,56/*"*/,63/*#*/,71/*$*/,79/*%*/,91/*&*/,101/*'*/,105/*(*/,110/*)*/,115/***/,121/*+*/,129/*,*/,135/*-*/,143/*.*/,148/* / */,
		153/*0*/,160/*1*/,167/*2*/,175/*3*/,183/*4*/,191/*5*/,199/*6*/,207/*7*/,215/*8*/,223/*9*/,231/*:*/,236/*;*/,241/*<*/,250/*=*/,259/*>*/,0,
		0,292/*A*/,303/*B*/,314/*C*/,325/*D*/,336/*E*/,346/*F*/,355/*G*/,366/*H*/,376/*I*/,381/*J*/,390/*K*/,400/*L*/,409/*M*/,422/*N*/,432/*O*/,
		444/*P*/,454/*Q*/,465/*R*/,476/*S*/,486/*T*/,496/*U*/,507/*V*/,517/*W*/,531/*X*/,541/*Y*/,551/*Z*/,561/*[*/,567/*\*/,573/*]*/,579/*^*/,587/*_*/,
		595/*`*/,601/*a*/,610/*b*/,619/*c*/,627/*d*/,636/*e*/,645/*f*/,651/*g*/,660/*h*/,668/*i*/,673/*j*/,679/*k*/,687/*l*/,692/*m*/,703/*n*/,712/*o*/,
		721/*p*/,730/*q*/,739/*r*/,746/*s*/,755/*t*/,761/*u*/,770/*v*/,779/*w*/,791/*x*/,800/*y*/,809/*z*/,0,0,0,0,0,
		857/*sup0*/,863/*sup1*/,869/*sup2*/,875/*sup3*/,881/*sup4*/,887/*sup5*/,893/*sup6*/,899/*sup7*/,905/*sup8*/,911/*sup9*/,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,852/*cdot*/,844/*deg*/,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	};
	static const int dx[256] = {
		0,20/*(paused)*/,20/*(ffwd)*/,20/*(rec)*/,20/*(empty)*/,20/*(playback)*/,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,7/*supE*/,9/*Delta*/,
		4/* */,4/*!*/,6/*"*/,7/*#*/,7/*$*/,11/*%*/,9/*&*/,3/*'*/,4/*(*/,4/*)*/,5/***/,7/*+*/,5/*,*/,7/*-*/,4/*.*/,4/* / */,
		7/*0*/,7/*1*/,7/*2*/,7/*3*/,7/*4*/,7/*5*/,7/*6*/,7/*7*/,7/*8*/,7/*9*/,4/*:*/,4/*;*/,7/*<*/,7/*=*/,7/*>*/,0,
		0,9/*A*/,9/*B*/,9/*C*/,9/*D*/,8/*E*/,8/*F*/,10/*G*/,9/*H*/,4/*I*/,7/*J*/,9/*K*/,8/*L*/,11/*M*/,9/*N*/,10/*O*/,
		8/*P*/,10/*Q*/,9/*R*/,8/*S*/,8/*T*/,9/*U*/,8/*V*/,12/*W*/,8/*X*/,8/*Y*/,8/*Z*/,4/*[*/,4/*\*/,4/*]*/,7/*^*/,7/*_*/,
		4/*`*/,7/*a*/,8/*b*/,7/*c*/,8/*d*/,7/*e*/,5/*f*/,8/*g*/,8/*h*/,4/*i*/,4/*j*/,7/*k*/,4/*l*/,10/*m*/,8/*n*/,8/*o*/,
		8/*p*/,8/*q*/,5/*r*/,7/*s*/,4/*t*/,8/*u*/,7/*v*/,10/*w*/,7/*x*/,7/*y*/,6/*z*/,0,0,0,0,0,
		6/*sup0*/,6/*sup1*/,6/*sup2*/,6/*sup3*/,6/*sup4*/,6/*sup5*/,6/*sup6*/,6/*sup7*/,6/*sup8*/,6/*sup9*/,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,4/*cdot*/,6/*deg*/,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	};
	static const int y0 = 0;
	static const int dy = 16;
	int i = 0, tgtx0 = tgtx;
	const unsigned char *c = (const unsigned char*)str;

	if (curstr) {
		for (; *c && *curstr && i < maxn; c++, curstr++, i++) {
			if (*c != *curstr) break;
			tgtx += dx[*c];
		}
	}
	gc->clbkBeginBltGroup (menuTgt);
	for (; *c && i < maxn; c++, i++) {
		gc->clbkBlt (menuTgt, tgtx, tgty, menuSrc, x0[*c], y0, dx[*c], dy);
		tgtx += dx[*c];
		if (curstr) *curstr++ = *c;
	}
	if (curstr) *curstr = '\0';
	if (cleantox > tgtx)
		gc->clbkBlt (menuTgt, tgtx, tgty, menuSrc, 673, 21, cleantox-tgtx, dy);
	gc->clbkEndBltGroup ();
	return tgtx;
}

// Copyright (c) Martin Schweiger
// Licensed under the MIT License

//=============================================================================
// VisualTab class
//=============================================================================

#include <windows.h>
#include <commctrl.h>
#include "TabVisual.h"
#include "resource.h"
#include <fstream>

//-----------------------------------------------------------------------------

orbiter::VisualTab::VisualTab (const LaunchpadDialog *lp): LaunchpadTab (lp)
{
}

//-----------------------------------------------------------------------------

orbiter::VisualTab::~VisualTab ()
{
}

//-----------------------------------------------------------------------------

void orbiter::VisualTab::Create ()
{
	hTab = CreateTab (IDD_PAGE_VIS);

	static int item[] = {
		IDC_OPT_STATIC1, IDC_OPT_STATIC2,
		IDC_RADIO1, IDC_RADIO2, IDC_OPT_COMPLEXMODEL, IDC_OPT_DAMAGE,
		IDC_OPT_COMPLEXGRAV, IDC_OPT_DISTMASS, IDC_OPT_WIND, IDC_OPT_RPRESSURE,
		IDC_VIS_STATIC1, IDC_VIS_STATIC2, IDC_VIS_STATIC3, IDC_VIS_STATIC4,
		IDC_VIS_STATIC5, IDC_VIS_CLOUD, IDC_VIS_CSHADOW, IDC_VIS_HAZE, IDC_VIS_FOG,
		IDC_VIS_REFWATER, IDC_VIS_RIPPLE, IDC_VIS_LIGHTS, IDC_VIS_LTLEVEL,
		IDC_VIS_MAXLEVEL, IDC_VIS_VSHADOW, IDC_VIS_SHADOW, IDC_VIS_SPECULAR,
		IDC_VIS_REENTRY, IDC_VIS_PARTICLE, IDC_VIS_LOCALLIGHT, IDC_VIS_AMBIENT,
		IDC_VIS_ELEV, IDC_VIS_ELEVMODE
	};

	RegisterItemPositions (item, ARRAYSIZE(item));
}

//-----------------------------------------------------------------------------

void orbiter::VisualTab::GetConfig (const Config *cfg)
{
	char cbuf[256];

	SendDlgItemMessage(hTab, IDC_OPT_COMPLEXMODEL, BM_SETCHECK,
		pCfg->CfgLogicPrm.FlightModelLevel ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hTab, IDC_OPT_DAMAGE, BM_SETCHECK,
		pCfg->CfgLogicPrm.DamageSetting ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hTab, IDC_OPT_DISTMASS, BM_SETCHECK,
		pCfg->CfgPhysicsPrm.bDistributedMass ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hTab, IDC_OPT_COMPLEXGRAV, BM_SETCHECK,
		pCfg->CfgPhysicsPrm.bNonsphericalGrav ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hTab, IDC_OPT_RPRESSURE, BM_SETCHECK,
		pCfg->CfgPhysicsPrm.bRadiationPressure ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hTab, IDC_OPT_WIND, BM_SETCHECK,
		pCfg->CfgPhysicsPrm.bAtmWind ? BST_CHECKED : BST_UNCHECKED, 0);

	SendDlgItemMessage (hTab, IDC_VIS_SHADOW, BM_SETCHECK,
		pCfg->CfgVisualPrm.bShadows ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hTab, IDC_VIS_VSHADOW, BM_SETCHECK,
		pCfg->CfgVisualPrm.bVesselShadows ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hTab, IDC_VIS_CLOUD, BM_SETCHECK,
		pCfg->CfgVisualPrm.bClouds ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hTab, IDC_VIS_CSHADOW, BM_SETCHECK,
		pCfg->CfgVisualPrm.bCloudShadows ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hTab, IDC_VIS_LIGHTS, BM_SETCHECK,
		pCfg->CfgVisualPrm.bNightlights ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hTab, IDC_VIS_REFWATER, BM_SETCHECK,
		pCfg->CfgVisualPrm.bWaterreflect ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hTab, IDC_VIS_RIPPLE, BM_SETCHECK,
		pCfg->CfgVisualPrm.bSpecularRipple ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hTab, IDC_VIS_HAZE, BM_SETCHECK,
		pCfg->CfgVisualPrm.bHaze ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hTab, IDC_VIS_FOG, BM_SETCHECK,
		pCfg->CfgVisualPrm.bFog ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hTab, IDC_VIS_SPECULAR, BM_SETCHECK,
		pCfg->CfgVisualPrm.bSpecular ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hTab, IDC_VIS_REENTRY, BM_SETCHECK,
		pCfg->CfgVisualPrm.bReentryFlames ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hTab, IDC_VIS_PARTICLE, BM_SETCHECK,
		pCfg->CfgVisualPrm.bParticleStreams ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hTab, IDC_VIS_LOCALLIGHT, BM_SETCHECK,
		pCfg->CfgVisualPrm.bLocalLight ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hTab, IDC_VIS_ELEV, BM_SETCHECK,
		pCfg->CfgVisualPrm.ElevMode ? BST_CHECKED : BST_UNCHECKED, 0);
	SetWindowText (GetDlgItem (hTab, IDC_VIS_AMBIENT), _itoa (pCfg->CfgVisualPrm.AmbientLevel, cbuf, 10));
	sprintf (cbuf, "%0.2f", pCfg->CfgVisualPrm.LightBrightness);
	SetWindowText (GetDlgItem (hTab, IDC_VIS_LTLEVEL), cbuf);
	SetWindowText (GetDlgItem (hTab, IDC_VIS_MAXLEVEL), _itoa (pCfg->CfgVisualPrm.PlanetMaxLevel, cbuf, 10));

	SendDlgItemMessage (hTab, IDC_VIS_ELEVMODE, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage (hTab, IDC_VIS_ELEVMODE, CB_ADDSTRING, 0, (LPARAM)"linear interpolation");
	SendDlgItemMessage (hTab, IDC_VIS_ELEVMODE, CB_ADDSTRING, 0, (LPARAM)"cubic interpolation");
	SendDlgItemMessage (hTab, IDC_VIS_ELEVMODE, CB_SETCURSEL, pCfg->CfgVisualPrm.ElevMode < 2 ? 0:1, 0);

	VisualsChanged ();
}

//-----------------------------------------------------------------------------

void orbiter::VisualTab::SetConfig (Config *cfg)
{
	DWORD i;
	char cbuf[128];
	double d;

	pCfg->CfgLogicPrm.FlightModelLevel = (SendDlgItemMessage(hTab, IDC_OPT_COMPLEXMODEL, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0);
	pCfg->CfgLogicPrm.DamageSetting = (SendDlgItemMessage(hTab, IDC_OPT_DAMAGE, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0);
	pCfg->CfgPhysicsPrm.bDistributedMass = (SendDlgItemMessage(hTab, IDC_OPT_DISTMASS, BM_GETCHECK, 0, 0) == BST_CHECKED);
	pCfg->CfgPhysicsPrm.bNonsphericalGrav = (SendDlgItemMessage(hTab, IDC_OPT_COMPLEXGRAV, BM_GETCHECK, 0, 0) == BST_CHECKED);
	pCfg->CfgPhysicsPrm.bRadiationPressure = (SendDlgItemMessage(hTab, IDC_OPT_RPRESSURE, BM_GETCHECK, 0, 0) == BST_CHECKED);
	pCfg->CfgPhysicsPrm.bAtmWind = (SendDlgItemMessage(hTab, IDC_OPT_WIND, BM_GETCHECK, 0, 0) == BST_CHECKED);

	pCfg->CfgVisualPrm.bShadows = (SendDlgItemMessage (hTab, IDC_VIS_SHADOW, BM_GETCHECK, 0, 0) == BST_CHECKED);
	pCfg->CfgVisualPrm.bVesselShadows = (SendDlgItemMessage (hTab, IDC_VIS_VSHADOW, BM_GETCHECK, 0, 0) == BST_CHECKED);
	pCfg->CfgVisualPrm.bClouds = (SendDlgItemMessage (hTab, IDC_VIS_CLOUD, BM_GETCHECK, 0, 0) == BST_CHECKED);
	pCfg->CfgVisualPrm.bCloudShadows = (SendDlgItemMessage (hTab, IDC_VIS_CSHADOW, BM_GETCHECK, 0, 0) == BST_CHECKED);
	pCfg->CfgVisualPrm.bNightlights = (SendDlgItemMessage (hTab, IDC_VIS_LIGHTS, BM_GETCHECK, 0, 0) == BST_CHECKED);
	pCfg->CfgVisualPrm.bWaterreflect = (SendDlgItemMessage (hTab, IDC_VIS_REFWATER, BM_GETCHECK, 0, 0) == BST_CHECKED);
	pCfg->CfgVisualPrm.bSpecularRipple = (SendDlgItemMessage (hTab, IDC_VIS_RIPPLE, BM_GETCHECK, 0, 0) == BST_CHECKED);
	pCfg->CfgVisualPrm.bHaze = (SendDlgItemMessage (hTab, IDC_VIS_HAZE, BM_GETCHECK, 0, 0) == BST_CHECKED);
	pCfg->CfgVisualPrm.bFog = (SendDlgItemMessage (hTab, IDC_VIS_FOG, BM_GETCHECK, 0, 0) == BST_CHECKED);
	pCfg->CfgVisualPrm.bSpecular = (SendDlgItemMessage (hTab, IDC_VIS_SPECULAR, BM_GETCHECK, 0, 0) == BST_CHECKED);
	pCfg->CfgVisualPrm.bReentryFlames = (SendDlgItemMessage (hTab, IDC_VIS_REENTRY, BM_GETCHECK, 0, 0) == BST_CHECKED);
	pCfg->CfgVisualPrm.bParticleStreams = (SendDlgItemMessage (hTab, IDC_VIS_PARTICLE, BM_GETCHECK, 0, 0) == BST_CHECKED);
	pCfg->CfgVisualPrm.bLocalLight = (SendDlgItemMessage (hTab, IDC_VIS_LOCALLIGHT, BM_GETCHECK, 0, 0) == BST_CHECKED);
	GetWindowText (GetDlgItem (hTab, IDC_VIS_AMBIENT), cbuf, 127);
	if (!sscanf (cbuf, "%lu", &i)) i = 15; else if (i > 255) i = 255;
	pCfg->SetAmbientLevel (i);
	GetWindowText (GetDlgItem (hTab, IDC_VIS_LTLEVEL), cbuf, 127);
	if (!sscanf (cbuf, "%lf", &d)) d = 0.5; else if (d < 0) d = 0.0; else if (d > 1) d = 1.0;
	pCfg->CfgVisualPrm.LightBrightness = d;
	GetWindowText (GetDlgItem (hTab, IDC_VIS_MAXLEVEL), cbuf, 127);
	if (!sscanf (cbuf, "%lu", &i)) i = SURF_MAX_PATCHLEVEL2;
	pCfg->CfgVisualPrm.PlanetMaxLevel = max (1, min (SURF_MAX_PATCHLEVEL2, i));
	pCfg->CfgVisualPrm.ElevMode = (SendDlgItemMessage (hTab, IDC_VIS_ELEV, BM_GETCHECK, 0, 0) != BST_CHECKED ?
		0 : SendDlgItemMessage (hTab, IDC_VIS_ELEVMODE, CB_GETCURSEL, 0, 0) + 1);
}

//-----------------------------------------------------------------------------

bool orbiter::VisualTab::OpenHelp ()
{
	OpenTabHelp ("tab_visual");
	return true;
}

//-----------------------------------------------------------------------------

INT_PTR orbiter::VisualTab::TabProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_VIS_CLOUD:
		case IDC_VIS_REFWATER:
			if (HIWORD (wParam) == BN_CLICKED) {
				VisualsChanged();
				return TRUE;
			}
			break;
		}
		break;
	}
	return FALSE;
}

//-----------------------------------------------------------------------------

void orbiter::VisualTab::VisualsChanged ()
{
	EnableWindow (GetDlgItem (hTab, IDC_VIS_CSHADOW),
		SendDlgItemMessage (hTab, IDC_VIS_CLOUD, BM_GETCHECK, 0, 0) == BST_CHECKED);
	EnableWindow (GetDlgItem (hTab, IDC_VIS_RIPPLE),
		SendDlgItemMessage (hTab, IDC_VIS_REFWATER, BM_GETCHECK, 0, 0) == BST_CHECKED);
}

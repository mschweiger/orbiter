// ==============================================================
// Class VideoTab (implementation)
// Manages the user selections in the "Video" tab of the Orbiter
// Launchpad dialog.
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2006-2016 Martin Schweiger
//				 2010-2016 Jarmo Nikkanen (D3D9Client implementation)
// ==============================================================

#include "D3D9Client.h"
#include "VideoTab.h"
#include "resource.h"
#include "resource_video.h"
#include "VideoTab.h"
#include "AABBUtil.h"
#include "D3D9Config.h"
#include "Commctrl.h"
#include "Junction.h"
#include "OapiExtension.h"
#include <vector>
#include <sstream>
#include <richedit.h>

using namespace oapi;

const UINT IDC_SCENARIO_TREE = (oapiGetOrbiterVersion() >= 111105) ? 1090 : 1088;

BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam)
{
	if (GetDlgItem(hwnd, IDC_SCENARIO_TREE)) {
		*(HWND*)lParam = hwnd; 
		return false;
	}
	return true;
}


// ==============================================================
// Constructor

VideoTab::VideoTab(D3D9Client *gc, HINSTANCE _hInst, HINSTANCE _hOrbiterInst, HWND hVideoTab)
{
	gclient      = gc;
	hInst        = _hInst;
	hOrbiterInst = _hOrbiterInst;
	hTab         = hVideoTab;
	aspect_idx	 = 0;
	SelectedAdapterIdx = 0;
}

VideoTab::~VideoTab()
{
	
}

// ==============================================================
// Dialog message handler

BOOL VideoTab::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	GraphicsClient::VIDEODATA *data = gclient->GetVideoData();

	switch (uMsg) {

	case WM_INITDIALOG:
	{
		return TRUE;
	}

	case WM_COMMAND:

		switch (LOWORD(wParam)) {

		case IDC_VID_DEVICE:
			if (HIWORD(wParam)==CBN_SELCHANGE) {
				DWORD idx = DWORD(SendDlgItemMessage(hWnd, IDC_VID_DEVICE, CB_GETCURSEL, 0, 0));
				SelectAdapter(idx);
				return TRUE;
			}
			break;

		case IDC_VID_MODE:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				DWORD idx = DWORD(SendDlgItemMessage (hWnd, IDC_VID_MODE, CB_GETCURSEL, 0, 0));
				SelectMode(idx);
				return TRUE;
			}
			break;

		case IDC_VID_BPP:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				SelectFullscreen(data->fullscreen);
				return TRUE;
			}


		case IDC_VID_FULL:
			if (HIWORD(wParam) == BN_CLICKED) {
				SelectFullscreen(true);
				data->fullscreen = true;
				return TRUE;
			}
			break;

		case IDC_VID_WINDOW:
			if (HIWORD(wParam) == BN_CLICKED) {
				SelectFullscreen(false);
				data->fullscreen = false;
				return TRUE;
			}
			break;

		case IDC_VID_WIDTH:
			if (HIWORD(wParam) == EN_CHANGE) {
				SelectWidth ();
				return TRUE;
			}
			break;

		case IDC_VID_HEIGHT:
			if (HIWORD(wParam) == EN_CHANGE) {
				SelectHeight ();
				return TRUE;
			}
			break;

		case IDC_VID_STENCIL:
			return TRUE;
			break;
			
		case IDC_VID_ASPECT:
			if (HIWORD(wParam) == BN_CLICKED) {
				SelectWidth();
				return TRUE;
			}
			break;

		case IDC_VID_4X3:
		case IDC_VID_16X10:
		case IDC_VID_16X9:
			if (HIWORD(wParam) == BN_CLICKED) {
				aspect_idx = LOWORD(wParam) - IDC_VID_4X3;
				SelectWidth();
				return TRUE;
			}
			break;

		case IDC_VID_INFO:
			DialogBoxParamA(hInst, MAKEINTRESOURCE(IDD_D3D9SETUP), hTab, SetupDlgProcWrp, (LPARAM)this);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

// ==============================================================
// Initialise the Launchpad "video" tab

bool VideoTab::Initialise()
{
	D3DDISPLAYMODE mode, curMode;
	D3DADAPTER_IDENTIFIER9 info;

	GraphicsClient::VIDEODATA *data = gclient->GetVideoData();

	data->forceenum = false;
	data->trystencil = false;

	SendDlgItemMessage(hTab, IDC_VID_DEVICE, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hTab, IDC_VID_MODE, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hTab, IDC_VID_BPP, CB_RESETCONTENT, 0, 0);

	ScanAtmoCfgs();

	char cbuf[32];
	int nAdapter = g_pD3DObject->GetAdapterCount();

	if (nAdapter == 0) {
		LogErr("VideoTab::Initialize() No DirectX9 Adapters Found");
		FailedDeviceError();
		return false;
	}

	if (data->deviceidx < 0 || (data->deviceidx)>=nAdapter) data->deviceidx = 0;

	for (int i=0;i<nAdapter;i++) {
		HR(g_pD3DObject->GetAdapterIdentifier(i, 0, &info));
		LogAlw("Adapter %d: %s", i, info.Description);
		SendDlgItemMessageA(hTab, IDC_VID_DEVICE, CB_ADDSTRING, 0, (LPARAM)info.Description);
	}

	SendDlgItemMessage(hTab, IDC_VID_DEVICE, CB_SETCURSEL, data->deviceidx, 0);


	HR(g_pD3DObject->GetAdapterDisplayMode(data->deviceidx, &curMode));

	LogAlw("Current Mode W=%u, H=%u", curMode.Width, curMode.Height);

	UINT nModes = g_pD3DObject->GetAdapterModeCount(data->deviceidx, D3DFMT_X8R8G8B8);

	if (nModes == 0) {
		LogErr("VideoTab::Initialize() No Display Modes Available");
		FailedDeviceError();
	}

	for (UINT k=0;k<nModes;k++) {
		HR(g_pD3DObject->EnumAdapterModes(data->deviceidx, D3DFMT_X8R8G8B8, k, &mode));
		sprintf_s(cbuf,32,"%u x %u  %uHz", mode.Width, mode.Height, mode.RefreshRate);
		LogAlw("Index:%u %u x %u  %uHz (%u)", k, mode.Width, mode.Height, mode.RefreshRate, mode.Format);
		SendDlgItemMessageA(hTab, IDC_VID_MODE, CB_ADDSTRING, 0, (LPARAM)cbuf);
		SendDlgItemMessageA(hTab, IDC_VID_MODE, CB_SETITEMDATA, k, (LPARAM)(mode.Height<<16 | mode.Width));
	}

	SendDlgItemMessageA(hTab, IDC_VID_BPP, CB_ADDSTRING, 0, (LPARAM)"True Full Screen (no alt-tab)");
	SendDlgItemMessageA(hTab, IDC_VID_BPP, CB_ADDSTRING, 0, (LPARAM)"Full Screen Window");
	SendDlgItemMessageA(hTab, IDC_VID_BPP, CB_ADDSTRING, 0, (LPARAM)"Window with Taskbar");
	SendDlgItemMessageA(hTab, IDC_VID_BPP, CB_SETCURSEL, data->style, 0);

	//SetWindowText(GetDlgItem(hTab, IDC_VID_STATIC5), "Resolution");
	SetWindowText(GetDlgItem(hTab, IDC_VID_STATIC6), "Full Screen Mode");


	SendDlgItemMessage(hTab, IDC_VID_MODE, CB_SETCURSEL, data->modeidx, 0);
	SendDlgItemMessage(hTab, IDC_VID_VSYNC, BM_SETCHECK, data->novsync ? BST_CHECKED : BST_UNCHECKED, 0);
		
	SetWindowText(GetDlgItem(hTab, IDC_VID_WIDTH), std::to_string(data->winw).c_str());
	SetWindowText(GetDlgItem(hTab, IDC_VID_HEIGHT), std::to_string(data->winh).c_str());

	aspect_idx = 0;
		
	if (data->winw == (4*data->winh)/3 || data->winh == (3*data->winw)/4)	aspect_idx = 1;
	else if (data->winw == (16*data->winh)/10 || data->winh == (10*data->winw)/16) aspect_idx = 2;
	else if (data->winw == (16*data->winh)/9 || data->winh == (9*data->winw)/16) aspect_idx = 3;
		
	SendDlgItemMessage(hTab, IDC_VID_ASPECT, BM_SETCHECK, aspect_idx ? BST_CHECKED : BST_UNCHECKED, 0);
	if (aspect_idx) aspect_idx--;
	SendDlgItemMessage(hTab, IDC_VID_4X3+aspect_idx, BM_SETCHECK, BST_CHECKED, 0);

	SendDlgItemMessage(hTab, IDC_VID_STENCIL,  BM_SETCHECK, data->trystencil, 0); // GDI Compatibility mode
	SendDlgItemMessage(hTab, IDC_VID_ENUM,     BM_SETCHECK, data->forceenum, 0);  
	SendDlgItemMessage(hTab, IDC_VID_PAGEFLIP, BM_SETCHECK, data->pageflip, 0);	  // Full scrren Window	

	bool bRet = SelectAdapter(data->deviceidx);

	SelectFullscreen(data->fullscreen);

	ShowWindow (GetDlgItem (hTab, IDC_VID_INFO), SW_SHOW);

	SetWindowText(GetDlgItem(hTab, IDC_VID_INFO), "Advanced");

	return bRet;
}


// ==============================================================
// 
void VideoTab::SelectMode(DWORD index)
{
	GraphicsClient::VIDEODATA *data = gclient->GetVideoData();
	SendDlgItemMessage(hTab, IDC_VID_MODE, CB_GETITEMDATA, index, 0);
	data->modeidx = index;
}


// ==============================================================
// Respond to user adapter selection
//
bool VideoTab::SelectAdapter(DWORD index)
{

	SelectedAdapterIdx = index; 

	GraphicsClient::VIDEODATA *data = gclient->GetVideoData();

	if (g_pD3DObject == NULL) {
		LogErr("VideoTab::SelectAdapter(%u) Direct3DCreate9 Failed", index);
		return false;
	}
	else {

		char cbuf[32];
		D3DDISPLAYMODE mode, curMode;
	
		if (g_pD3DObject->GetAdapterCount()<=index) {
			LogErr("Adapter Index out of range");
			return false;
		}

		HR(g_pD3DObject->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &curMode));

		SendDlgItemMessage(hTab, IDC_VID_MODE, CB_RESETCONTENT, 0, 0);

		DWORD nModes = g_pD3DObject->GetAdapterModeCount(index, D3DFMT_X8R8G8B8);

		if (nModes == 0) {
			LogErr("VideoTab::SelectAdapter() No Display Modes Available");	
		}

		for (DWORD k=0;k<nModes;k++) {
			HR(g_pD3DObject->EnumAdapterModes(index, D3DFMT_X8R8G8B8, k, &mode));
			sprintf_s(cbuf,32,"%u x %u %uHz", mode.Width, mode.Height, mode.RefreshRate);
			SendDlgItemMessageA(hTab, IDC_VID_MODE, CB_ADDSTRING, 0, (LPARAM)cbuf);
			SendDlgItemMessageA(hTab, IDC_VID_MODE, CB_SETITEMDATA, k, (LPARAM)(mode.Height<<16 | mode.Width));
		}

		SendDlgItemMessage(hTab, IDC_VID_MODE, CB_SETCURSEL, data->modeidx, 0);
	}

	return true;
}



void VideoTab::SelectFullscreen(bool bFull)
{

	SetWindowText(GetDlgItem(hTab, IDC_VID_ENUM), "(unused)");
	SetWindowText(GetDlgItem(hTab, IDC_VID_STENCIL), "Force window size");
	SetWindowText(GetDlgItem(hTab, IDC_VID_PAGEFLIP), "Multiple displays");

	SendDlgItemMessage(hTab, IDC_VID_FULL, BM_SETCHECK, bFull ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hTab, IDC_VID_WINDOW, BM_SETCHECK, bFull ? BST_UNCHECKED : BST_CHECKED, 0);

	EnableWindow(GetDlgItem(hTab, IDC_VID_ENUM), false);
	EnableWindow(GetDlgItem(hTab, IDC_VID_STENCIL), true);

	if (bFull) {
		EnableWindow(GetDlgItem(hTab, IDC_VID_ASPECT), false);
		EnableWindow(GetDlgItem(hTab, IDC_VID_WIDTH), false);
		EnableWindow(GetDlgItem(hTab, IDC_VID_HEIGHT), false);
		EnableWindow(GetDlgItem(hTab, IDC_VID_4X3), false);
		EnableWindow(GetDlgItem(hTab, IDC_VID_16X10), false);
		EnableWindow(GetDlgItem(hTab, IDC_VID_16X9), false);
		EnableWindow(GetDlgItem(hTab, IDC_VID_MODE), true);
		EnableWindow(GetDlgItem(hTab, IDC_VID_VSYNC), true);
		EnableWindow(GetDlgItem(hTab, IDC_VID_PAGEFLIP), true);
		EnableWindow(GetDlgItem(hTab, IDC_VID_BPP), true);
	}
	else {
		EnableWindow(GetDlgItem(hTab, IDC_VID_ASPECT), true);
		EnableWindow(GetDlgItem(hTab, IDC_VID_WIDTH), true);
		EnableWindow(GetDlgItem(hTab, IDC_VID_HEIGHT), true);
		EnableWindow(GetDlgItem(hTab, IDC_VID_4X3), true);
		EnableWindow(GetDlgItem(hTab, IDC_VID_16X10), true);
		EnableWindow(GetDlgItem(hTab, IDC_VID_16X9), true);
		EnableWindow(GetDlgItem(hTab, IDC_VID_MODE), false);
		EnableWindow(GetDlgItem(hTab, IDC_VID_VSYNC), true);
		EnableWindow(GetDlgItem(hTab, IDC_VID_PAGEFLIP), false);
		EnableWindow(GetDlgItem(hTab, IDC_VID_BPP), false);
	}
}


static int aspect_wfac[3] = {4,16,16};
static int aspect_hfac[3] = {3,10,9};


void VideoTab::SelectWidth ()
{
	if (SendDlgItemMessage (hTab, IDC_VID_ASPECT, BM_GETCHECK, 0, 0) == BST_CHECKED) {
		char cbuf[32];
		int w, h, wfac = aspect_wfac[aspect_idx], hfac = aspect_hfac[aspect_idx];
		GetWindowText(GetDlgItem(hTab, IDC_VID_WIDTH),  cbuf, 32); w = atoi(cbuf);
		GetWindowText(GetDlgItem(hTab, IDC_VID_HEIGHT), cbuf, 32); h = atoi(cbuf);
		if (w != (wfac*h)/hfac) {
			h = (hfac*w)/wfac;
			SetWindowText (GetDlgItem (hTab, IDC_VID_HEIGHT), std::to_string(h).c_str());
		}
	}
}

// ==============================================================
// Respond to user selection of render window height

void VideoTab::SelectHeight ()
{
	if (SendDlgItemMessage (hTab, IDC_VID_ASPECT, BM_GETCHECK, 0, 0) == BST_CHECKED) {
		char cbuf[32];
		int w, h, wfac = aspect_wfac[aspect_idx], hfac = aspect_hfac[aspect_idx];
		GetWindowText(GetDlgItem(hTab, IDC_VID_WIDTH),  cbuf, 32); w = atoi(cbuf);
		GetWindowText(GetDlgItem(hTab, IDC_VID_HEIGHT), cbuf, 32); h = atoi(cbuf);
		if (h != (hfac*w)/wfac) {
			w = (wfac*h)/hfac;
			SetWindowText (GetDlgItem (hTab, IDC_VID_WIDTH), std::to_string(w).c_str());
		}
	}
}

// ==============================================================
// copy dialog state back to parameter structure

void VideoTab::UpdateConfigData()
{
	char cbuf[32];
	GraphicsClient::VIDEODATA *data = gclient->GetVideoData();

	// device parameters
	data->deviceidx  = (int)SendDlgItemMessage (hTab, IDC_VID_DEVICE, CB_GETCURSEL, 0, 0);
	data->modeidx	 = (int)SendDlgItemMessage (hTab, IDC_VID_MODE, CB_GETCURSEL, 0, 0);
	data->style		 = SendDlgItemMessage (hTab, IDC_VID_BPP, CB_GETCURSEL, 0, 0);
	data->fullscreen = (SendDlgItemMessage (hTab, IDC_VID_FULL, BM_GETCHECK, 0, 0) == BST_CHECKED);
	data->novsync    = (SendDlgItemMessage (hTab, IDC_VID_VSYNC, BM_GETCHECK, 0, 0) == BST_CHECKED);
	data->pageflip   = (SendDlgItemMessage (hTab, IDC_VID_PAGEFLIP, BM_GETCHECK, 0, 0) == BST_CHECKED);
	data->trystencil = (SendDlgItemMessage (hTab, IDC_VID_STENCIL, BM_GETCHECK, 0, 0) == BST_CHECKED);
	data->forceenum  = (SendDlgItemMessage (hTab, IDC_VID_ENUM, BM_GETCHECK, 0, 0) == BST_CHECKED);

	GetWindowText(GetDlgItem(hTab, IDC_VID_WIDTH),  cbuf, 32); data->winw = atoi(cbuf);
	GetWindowText(GetDlgItem(hTab, IDC_VID_HEIGHT), cbuf, 32); data->winh = atoi(cbuf);	


	HWND hChild = NULL;
	HWND hRoot = GetAncestor(hTab, GA_ROOT);

	EnumChildWindows(hRoot, EnumChildProc, (LPARAM)&hChild);

	if (hChild) {

		HWND hTree = GetDlgItem(hChild, IDC_SCENARIO_TREE);

		if (hTree==NULL) {
			LogErr("FAILED to get a scenario tree control handle");
			return;
		}

		HTREEITEM item = TreeView_GetSelection(hTree);

		if (item == NULL) {
			LogErr("FAILED. Scenario not selected");
			return;
		}

		using std::vector;
		vector<HTREEITEM> hNodes;

		while (item) { // [ego, parent, grandparent, ...]
			hNodes.push_back( item );
			item = TreeView_GetParent(hTree, item);
		}

		using std::string;
		string path = OapiExtension::GetScenarioDir();
		path.erase( path.find_last_not_of( '\\' )+1 ); // trim trailing path-delimiter

		char buf[MAX_PATH];
		TVITEMA tvItem = {0};
		tvItem.mask = TVIF_TEXT | TVIF_HANDLE;
		tvItem.pszText = buf;
		tvItem.cchTextMax = ARRAYSIZE(buf);

		for (auto it = hNodes.crbegin(); it != hNodes.crend(); ++it) {
			tvItem.hItem = *it;
			TreeView_GetItem(hTree, &tvItem);
			// Note: The returned text will not necessarily be stored in the
			//       original buffer passed by the application.
			//       It is possible that pszText will point to text in a
			//       new buffer rather than place it in the old buffer. 
			path += "\\"; path += tvItem.pszText;
		}
		path += ".scn";

		gclient->SetScenarioName(path);

		LogAlw("Scenario = %s", path.c_str());
	}
	else {
		LogErr("FAILED to get a handle of a scenario dialog");
	}
}





// ***************************************************************************************************
// Advanced setup Dialog
// ***************************************************************************************************


INT_PTR CALLBACK VideoTab::SetupDlgProcWrp(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static class VideoTab *VTab = NULL;
	switch (uMsg) {
		case WM_INITDIALOG: 
			VTab = (class VideoTab *)lParam;
			VTab->InitSetupDialog(hWnd);
			return true;

		case WM_COMMAND:
		case WM_HSCROLL:
			if (VTab) VTab->SetupDlgProc(hWnd, uMsg, wParam, lParam);
			break;
	}
	return false;
}



INT_PTR CALLBACK VideoTab::SetupDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

	if (uMsg==WM_HSCROLL) {
		if (LOWORD(wParam)==TB_THUMBTRACK) {
			char lbl[32];
			WORD pos = HIWORD(wParam);
			if (HWND(lParam)==GetDlgItem(hWnd, IDC_CONVERGENCE)) {
				sprintf_s(lbl,32,"%1.2fm",float(pos)*0.01);
				SetWindowTextA(GetDlgItem(hWnd, IDC_CONV_DSP), lbl);
			}
			if (HWND(lParam)==GetDlgItem(hWnd, IDC_SEPARATION)) {
				sprintf_s(lbl,32,"%1.0f%%",float(pos));
				SetWindowTextA(GetDlgItem(hWnd, IDC_SEPA_DSP), lbl);
			}
		}
		return false;
	}

	switch (LOWORD(wParam)) {

		case IDC_MESH_DEBUGGER:
			MessageBoxA(hWnd,"You must restart launchpad for changes to take effect","Notification",MB_OK);
			break;

		case IDC_SYMBOLIC:
			CreateSymbolicLinks();
			break;

		case IDC_CREDITS:
			LoadLibrary("riched20.dll");
			DialogBoxParamA(hInst, MAKEINTRESOURCEA(IDD_D3D9CREDITS), hWnd, CreditsDlgProcWrp, (LPARAM)this);
			break;

		case IDC_SRFPRELOAD:
			SendDlgItemMessageA(hWnd, IDC_DEMAND, BM_SETCHECK, BST_UNCHECKED, 0);
			break;

		case IDC_DEMAND:
			SendDlgItemMessageA(hWnd, IDC_SRFPRELOAD, BM_SETCHECK, BST_UNCHECKED, 0);
			break;

		case IDOK:
		case IDCANCEL:
			SaveSetupState(hWnd);
			EndDialog (hWnd, 0);
			break;
	}
	
	return false;
}





void VideoTab::InitSetupDialog(HWND hWnd)
{

	char cbuf[32];
	DWORD aamax = 0;
	D3DCAPS9 caps;

	if (g_pD3DObject == NULL) {
		LogErr("VideoTab::SelectAdapter(%u) Direct3DCreate9 Failed", SelectedAdapterIdx);
		return;
	}

	g_pD3DObject->GetDeviceCaps(SelectedAdapterIdx, D3DDEVTYPE_HAL, &caps);

	if (g_pD3DObject->CheckDeviceMultiSampleType(SelectedAdapterIdx, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, true, D3DMULTISAMPLE_2_SAMPLES, NULL)==S_OK) aamax=2;
	if (g_pD3DObject->CheckDeviceMultiSampleType(SelectedAdapterIdx, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, true, D3DMULTISAMPLE_4_SAMPLES, NULL)==S_OK) aamax=4;
	if (g_pD3DObject->CheckDeviceMultiSampleType(SelectedAdapterIdx, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, true, D3DMULTISAMPLE_8_SAMPLES, NULL)==S_OK) aamax=8;
	
	LogAlw("InitSetupDialog() Enum Device AA capability = %u",aamax);


	// AA -----------------------------------------

	SendDlgItemMessage(hWnd, IDC_AA, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_AA, CB_ADDSTRING, 0, (LPARAM)"None");
	if (aamax>=2)  SendDlgItemMessageA(hWnd, IDC_AA, CB_ADDSTRING, 0, (LPARAM)"2x");
	if (aamax>=4)  SendDlgItemMessageA(hWnd, IDC_AA, CB_ADDSTRING, 0, (LPARAM)"4x");
	if (aamax>=8)  SendDlgItemMessageA(hWnd, IDC_AA, CB_ADDSTRING, 0, (LPARAM)"8x");
	

	// AF -----------------------------------------

	SendDlgItemMessage(hWnd, IDC_AF, CB_RESETCONTENT, 0, 0);
	if (caps.MaxAnisotropy>=2) SendDlgItemMessageA(hWnd, IDC_AF, CB_ADDSTRING, 0, (LPARAM)"2x");
	if (caps.MaxAnisotropy>=4) SendDlgItemMessageA(hWnd, IDC_AF, CB_ADDSTRING, 0, (LPARAM)"4x");
	if (caps.MaxAnisotropy>=8) SendDlgItemMessageA(hWnd, IDC_AF, CB_ADDSTRING, 0, (LPARAM)"8x");
	if (caps.MaxAnisotropy>=12) SendDlgItemMessageA(hWnd, IDC_AF, CB_ADDSTRING, 0, (LPARAM)"12x");
	if (caps.MaxAnisotropy>=16) SendDlgItemMessageA(hWnd, IDC_AF, CB_ADDSTRING, 0, (LPARAM)"16x");
	

	// DEBUG --------------------------------------

	SendDlgItemMessage(hWnd, IDC_DEBUG, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_DEBUG, CB_ADDSTRING, 0, (LPARAM)"0");
	SendDlgItemMessageA(hWnd, IDC_DEBUG, CB_ADDSTRING, 0, (LPARAM)"1");
	SendDlgItemMessageA(hWnd, IDC_DEBUG, CB_ADDSTRING, 0, (LPARAM)"2");
	SendDlgItemMessageA(hWnd, IDC_DEBUG, CB_ADDSTRING, 0, (LPARAM)"3");
	SendDlgItemMessageA(hWnd, IDC_DEBUG, CB_ADDSTRING, 0, (LPARAM)"4");
	
	// SKETCHPAD --------------------------------------

	SendDlgItemMessage(hWnd, IDC_FONT, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_FONT, CB_ADDSTRING, 0, (LPARAM)"Crisp");
	SendDlgItemMessageA(hWnd, IDC_FONT, CB_ADDSTRING, 0, (LPARAM)"Antialiased");
	SendDlgItemMessageA(hWnd, IDC_FONT, CB_ADDSTRING, 0, (LPARAM)"Cleartype");
	
	// ENVMAP MODE --------------------------------------

	SendDlgItemMessage(hWnd, IDC_ENVMODE, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_ENVMODE, CB_ADDSTRING, 0, (LPARAM)"Disable (Debug)");
	SendDlgItemMessageA(hWnd, IDC_ENVMODE, CB_ADDSTRING, 0, (LPARAM)"Planet Only");
	SendDlgItemMessageA(hWnd, IDC_ENVMODE, CB_ADDSTRING, 0, (LPARAM)"Full Scene");
	SendDlgItemMessage(hWnd, IDC_ENVMODE, CB_SETCURSEL, 0, 0);

	// CUSTOM CAMERA MODE --------------------------------------

	SendDlgItemMessage(hWnd, IDC_CAMMODE, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_CAMMODE, CB_ADDSTRING, 0, (LPARAM)"Disable");
	SendDlgItemMessageA(hWnd, IDC_CAMMODE, CB_ADDSTRING, 0, (LPARAM)"Enabled");
	SendDlgItemMessage(hWnd, IDC_ENVMODE, CB_SETCURSEL, 0, 0);

	// ENVMAP FACES --------------------------------------

	SendDlgItemMessage(hWnd, IDC_ENVFACES, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_ENVFACES, CB_ADDSTRING, 0, (LPARAM)"Light");
	SendDlgItemMessageA(hWnd, IDC_ENVFACES, CB_ADDSTRING, 0, (LPARAM)"Medimum");
	SendDlgItemMessageA(hWnd, IDC_ENVFACES, CB_ADDSTRING, 0, (LPARAM)"Heavy");
	SendDlgItemMessage(hWnd, IDC_ENVFACES, CB_SETCURSEL, 0, 0);

	// TEXTURE MIPMAP POLICY --------------------------------------

	SendDlgItemMessage(hWnd, IDC_TEXMIPS, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_TEXMIPS, CB_ADDSTRING, 0, (LPARAM)"Load as defined");
	SendDlgItemMessageA(hWnd, IDC_TEXMIPS, CB_ADDSTRING, 0, (LPARAM)"Autogen missing");
	SendDlgItemMessageA(hWnd, IDC_TEXMIPS, CB_ADDSTRING, 0, (LPARAM)"Autogen all");
	SendDlgItemMessage(hWnd, IDC_TEXMIPS, CB_SETCURSEL, 0, 0);

	// MICROTEX FILTER --------------------------------------------

	SendDlgItemMessage(hWnd,  IDC_MICROFILTER, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_MICROFILTER, CB_ADDSTRING, 0, (LPARAM)"Point (Fast/Good)");
	SendDlgItemMessageA(hWnd, IDC_MICROFILTER, CB_ADDSTRING, 0, (LPARAM)"Linear (Fast/Bad)");
	SendDlgItemMessageA(hWnd, IDC_MICROFILTER, CB_ADDSTRING, 0, (LPARAM)"Anisotropic 2x");
	SendDlgItemMessageA(hWnd, IDC_MICROFILTER, CB_ADDSTRING, 0, (LPARAM)"Anisotropic 4x (Better)");
	SendDlgItemMessageA(hWnd, IDC_MICROFILTER, CB_ADDSTRING, 0, (LPARAM)"Anisotropic 8x");
	SendDlgItemMessageA(hWnd, IDC_MICROFILTER, CB_ADDSTRING, 0, (LPARAM)"Anisotropic 16x (Slow/Best)");
	SendDlgItemMessage(hWnd,  IDC_MICROFILTER, CB_SETCURSEL, 0, 0);
	
	// MICROTEX FILTER --------------------------------------------

	SendDlgItemMessage(hWnd,  IDC_MICROMODE, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_MICROMODE, CB_ADDSTRING, 0, (LPARAM)"Disabled");
	SendDlgItemMessageA(hWnd, IDC_MICROMODE, CB_ADDSTRING, 0, (LPARAM)"Enabled");
	SendDlgItemMessage(hWnd,  IDC_MICROMODE, CB_SETCURSEL, 0, 0);


	// MICROTEX BLEND MODE -----------------------------------------
	
	SendDlgItemMessage(hWnd,  IDC_BLENDMODE, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_BLENDMODE, CB_ADDSTRING, 0, (LPARAM)"Soft light");
	SendDlgItemMessageA(hWnd, IDC_BLENDMODE, CB_ADDSTRING, 0, (LPARAM)"Normal light");
	SendDlgItemMessageA(hWnd, IDC_BLENDMODE, CB_ADDSTRING, 0, (LPARAM)"Hard light");
	SendDlgItemMessage(hWnd,  IDC_BLENDMODE, CB_SETCURSEL, 0, 0);

	// TILE MIPMAP POLICY -----------------------------------------

	SendDlgItemMessage(hWnd, IDC_MIPMAPS, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_MIPMAPS, CB_ADDSTRING, 0, (LPARAM)"Disabled");
	SendDlgItemMessageA(hWnd, IDC_MIPMAPS, CB_ADDSTRING, 0, (LPARAM)"Enabled (slow2load)");
	SendDlgItemMessage(hWnd, IDC_MIPMAPS, CB_SETCURSEL, 0, 0);

	// ARCHIVE METHOD ------------------------------------------

	SendDlgItemMessage(hWnd, IDC_ARCHIVE, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_ARCHIVE, CB_ADDSTRING, 0, (LPARAM)"Cache only");
	SendDlgItemMessageA(hWnd, IDC_ARCHIVE, CB_ADDSTRING, 0, (LPARAM)"Archive only");
	SendDlgItemMessageA(hWnd, IDC_ARCHIVE, CB_ADDSTRING, 0, (LPARAM)"Cache & Archive");
	SendDlgItemMessage(hWnd, IDC_ARCHIVE, CB_SETCURSEL, 0, 0);

	// POSTPROCESSING METHOD ------------------------------------------

	SendDlgItemMessage(hWnd, IDC_POSTPROCESS, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_POSTPROCESS, CB_ADDSTRING, 0, (LPARAM)"None");
	SendDlgItemMessageA(hWnd, IDC_POSTPROCESS, CB_ADDSTRING, 0, (LPARAM)"Light glow");
	SendDlgItemMessage(hWnd, IDC_POSTPROCESS, CB_SETCURSEL, 0, 0);

	// Local Lights -----------------------------------------

	SendDlgItemMessage(hWnd, IDC_LIGHTCONFIG, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_LIGHTCONFIG, CB_ADDSTRING, 0, (LPARAM)"None");
	SendDlgItemMessageA(hWnd, IDC_LIGHTCONFIG, CB_ADDSTRING, 0, (LPARAM)"4x Partial");
	SendDlgItemMessageA(hWnd, IDC_LIGHTCONFIG, CB_ADDSTRING, 0, (LPARAM)"4x Full");
	SendDlgItemMessageA(hWnd, IDC_LIGHTCONFIG, CB_ADDSTRING, 0, (LPARAM)"8x Partial");
	SendDlgItemMessageA(hWnd, IDC_LIGHTCONFIG, CB_ADDSTRING, 0, (LPARAM)"8x Full");

	// Shadows -----------------------------------------

	SendDlgItemMessage(hWnd, IDC_SELFSHADOWS, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_SELFSHADOWS, CB_ADDSTRING, 0, (LPARAM)"None");
	SendDlgItemMessageA(hWnd, IDC_SELFSHADOWS, CB_ADDSTRING, 0, (LPARAM)"Focus + payload");
	SendDlgItemMessageA(hWnd, IDC_SELFSHADOWS, CB_ADDSTRING, 0, (LPARAM)"Near by objects");
	SendDlgItemMessageA(hWnd, IDC_SELFSHADOWS, CB_ADDSTRING, 0, (LPARAM)"All visible objects");

	SendDlgItemMessage(hWnd, IDC_SHADOWFILTER, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_SHADOWFILTER, CB_ADDSTRING, 0, (LPARAM)"9 samples");
	SendDlgItemMessageA(hWnd, IDC_SHADOWFILTER, CB_ADDSTRING, 0, (LPARAM)"27 samples");
	SendDlgItemMessageA(hWnd, IDC_SHADOWFILTER, CB_ADDSTRING, 0, (LPARAM)"27s dither");
	//SendDlgItemMessageA(hWnd, IDC_SHADOWFILTER, CB_ADDSTRING, 0, (LPARAM)"40 samples");
	//SendDlgItemMessageA(hWnd, IDC_SHADOWFILTER, CB_ADDSTRING, 0, (LPARAM)"40s dither");

	SendDlgItemMessage(hWnd, IDC_CASCOUNT, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_CASCOUNT, CB_ADDSTRING, 0, (LPARAM)"5m  1-cas low quality");
	SendDlgItemMessageA(hWnd, IDC_CASCOUNT, CB_ADDSTRING, 0, (LPARAM)"8m  2-cas hight quality");
	SendDlgItemMessageA(hWnd, IDC_CASCOUNT, CB_ADDSTRING, 0, (LPARAM)"24m 3-cas hight quality");

	SendDlgItemMessage(hWnd, IDC_TERRAIN, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_TERRAIN, CB_ADDSTRING, 0, (LPARAM)"None");
	SendDlgItemMessageA(hWnd, IDC_TERRAIN, CB_ADDSTRING, 0, (LPARAM)"Stencil");
	SendDlgItemMessageA(hWnd, IDC_TERRAIN, CB_ADDSTRING, 0, (LPARAM)"Projected");

	SendDlgItemMessage(hWnd, IDC_MESHRES, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_MESHRES, CB_ADDSTRING, 0, (LPARAM)"16");
	SendDlgItemMessageA(hWnd, IDC_MESHRES, CB_ADDSTRING, 0, (LPARAM)"32");

	SendDlgItemMessage(hWnd, IDC_TILECOUNT, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_TILECOUNT, CB_ADDSTRING, 0, (LPARAM)"600");
	SendDlgItemMessageA(hWnd, IDC_TILECOUNT, CB_ADDSTRING, 0, (LPARAM)"1200");
	SendDlgItemMessageA(hWnd, IDC_TILECOUNT, CB_ADDSTRING, 0, (LPARAM)"2400");

	// gcGUI -----------------------------------------
	if (Config->gcGUIMode == 1) Config->gcGUIMode = 0;
	SendDlgItemMessage(hWnd, IDC_GUIMODE, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hWnd, IDC_GUIMODE, CB_ADDSTRING, 0, (LPARAM)"Disabled");
	SendDlgItemMessageA(hWnd, IDC_GUIMODE, CB_ADDSTRING, 0, (LPARAM)"(unused)");
	SendDlgItemMessageA(hWnd, IDC_GUIMODE, CB_ADDSTRING, 0, (LPARAM)"Windowed");

	// Earth AtmoConfig -------------------------------
	SendDlgItemMessage(hWnd, IDC_EARTHVISCFG, CB_RESETCONTENT, 0, 0);
	for (auto x : AtmoCfgs["Earth"]) SendDlgItemMessageA(hWnd, IDC_EARTHVISCFG, CB_ADDSTRING, 0, (LPARAM)x.cfg.c_str());
	for (int i = 0; i < AtmoCfgs["Earth"].size(); i++) {
		if (Config->AtmoCfg["Earth"] == AtmoCfgs["Earth"][i].file) {
			SendDlgItemMessage(hWnd, IDC_EARTHVISCFG, CB_SETCURSEL, i, 0);
			break;
		}
	}
		


	// Write values in controls ----------------

	bool bFS = (SendDlgItemMessage(hTab, IDC_VID_BPP, CB_GETCURSEL, 0, 0)==0 && SendDlgItemMessage(hTab, IDC_VID_FULL, BM_GETCHECK, 0, 0)==BST_CHECKED);
	bool bGB = (SendDlgItemMessage (hTab, IDC_VID_STENCIL, BM_GETCHECK, 0, 0)==BST_CHECKED);

	if (bFS || bGB) {
		Config->SceneAntialias = 0;
		EnableWindow(GetDlgItem(hWnd, IDC_AA), false);
	}
	else {
		EnableWindow(GetDlgItem(hWnd, IDC_AA), true);
	}


	SendDlgItemMessage(hWnd, IDC_CONVERGENCE, TBM_SETRANGEMAX, 1, 100);
	SendDlgItemMessage(hWnd, IDC_CONVERGENCE, TBM_SETRANGEMIN, 1, 5);
	SendDlgItemMessage(hWnd, IDC_CONVERGENCE, TBM_SETTICFREQ, 5, 0);
	
	SendDlgItemMessage(hWnd, IDC_SEPARATION, TBM_SETRANGEMAX,  1, 100);
	SendDlgItemMessage(hWnd, IDC_SEPARATION, TBM_SETRANGEMIN,  1, 10);
	SendDlgItemMessage(hWnd, IDC_SEPARATION, TBM_SETTICFREQ,  5, 0);
	
	SendDlgItemMessage(hWnd, IDC_LODBIAS, TBM_SETRANGEMAX, 1, 10);
	SendDlgItemMessage(hWnd, IDC_LODBIAS, TBM_SETRANGEMIN, 1, -10);
	SendDlgItemMessage(hWnd, IDC_LODBIAS, TBM_SETTICFREQ, 1, 0);

	SendDlgItemMessage(hWnd, IDC_MICROBIAS, TBM_SETRANGEMAX, 1, 10);
	SendDlgItemMessage(hWnd, IDC_MICROBIAS, TBM_SETRANGEMIN, 1, 0);
	SendDlgItemMessage(hWnd, IDC_MICROBIAS, TBM_SETTICFREQ, 1, 0);
	

	sprintf_s(cbuf,32,"%1.1fm",float(Config->Convergence));
	SetWindowTextA(GetDlgItem(hWnd, IDC_CONV_DSP), cbuf);
			
	sprintf_s(cbuf,32,"%1.0f%%",float(Config->Separation));
	SetWindowTextA(GetDlgItem(hWnd, IDC_SEPA_DSP), cbuf);

	SendDlgItemMessage(hWnd, IDC_CONVERGENCE, TBM_SETPOS, 1, int(Config->Convergence*100.0));
	SendDlgItemMessage(hWnd, IDC_SEPARATION,  TBM_SETPOS, 1, int(Config->Separation));
	SendDlgItemMessage(hWnd, IDC_LODBIAS,     TBM_SETPOS, 1, int(Config->LODBias*5.0));
	SendDlgItemMessage(hWnd, IDC_MICROBIAS,   TBM_SETPOS, 1, int(Config->MicroBias));

	SendDlgItemMessage(hWnd, IDC_TILECOUNT, CB_SETCURSEL, Config->MaxTiles, 0);
	SendDlgItemMessage(hWnd, IDC_MESHRES, CB_SETCURSEL, Config->MeshRes, 0);
	SendDlgItemMessage(hWnd, IDC_ARCHIVE, CB_SETCURSEL, Config->PlanetTileLoadFlags-1, 0);
	SendDlgItemMessage(hWnd, IDC_BLENDMODE, CB_SETCURSEL, Config->BlendMode, 0);
	SendDlgItemMessage(hWnd, IDC_MICROMODE, CB_SETCURSEL, Config->MicroMode, 0);
	SendDlgItemMessage(hWnd, IDC_MICROFILTER, CB_SETCURSEL, Config->MicroFilter, 0);
	SendDlgItemMessage(hWnd, IDC_TEXMIPS, CB_SETCURSEL, Config->TextureMips, 0);
	SendDlgItemMessage(hWnd, IDC_ENVMODE, CB_SETCURSEL, Config->EnvMapMode, 0);
	SendDlgItemMessage(hWnd, IDC_CAMMODE, CB_SETCURSEL, Config->CustomCamMode, 0);
	SendDlgItemMessage(hWnd, IDC_ENVFACES, CB_SETCURSEL, Config->EnvMapFaces-1, 0);
	SendDlgItemMessage(hWnd, IDC_FONT, CB_SETCURSEL, Config->SketchpadFont, 0);
	SendDlgItemMessage(hWnd, IDC_DEBUG, CB_SETCURSEL, Config->DebugLvl, 0);
	SendDlgItemMessage(hWnd, IDC_MIPMAPS, CB_SETCURSEL, Config->TileMipmaps, 0);
	SendDlgItemMessage(hWnd, IDC_POSTPROCESS, CB_SETCURSEL, Config->PostProcess, 0);
	SendDlgItemMessage(hWnd, IDC_LIGHTCONFIG, CB_SETCURSEL, Config->LightConfig, 0);
	SendDlgItemMessage(hWnd, IDC_SELFSHADOWS, CB_SETCURSEL, Config->ShadowMapMode, 0);
	SendDlgItemMessage(hWnd, IDC_SHADOWFILTER, CB_SETCURSEL, Config->ShadowFilter, 0);
	SendDlgItemMessage(hWnd, IDC_TERRAIN, CB_SETCURSEL, Config->TerrainShadowing, 0);
	SendDlgItemMessage(hWnd, IDC_GUIMODE, CB_SETCURSEL, Config->gcGUIMode, 0);
	SendDlgItemMessage(hWnd, IDC_CASCOUNT, CB_SETCURSEL, Config->VCCascadeCount - 1, 0);

	SendDlgItemMessage(hWnd, IDC_DEMAND, BM_SETCHECK, Config->PlanetPreloadMode==0, 0);
	SendDlgItemMessage(hWnd, IDC_SRFPRELOAD, BM_SETCHECK, Config->PlanetPreloadMode==1, 0);
	SendDlgItemMessage(hWnd, IDC_GLASSSHADE, BM_SETCHECK, Config->EnableGlass==1, 0);
	SendDlgItemMessage(hWnd, IDC_MESH_DEBUGGER, BM_SETCHECK, Config->EnableMeshDbg==1, 0);
	SendDlgItemMessage(hWnd, IDC_CLOUDMICRO, BM_SETCHECK, Config->CloudMicro == 1, 0);
	SendDlgItemMessage(hWnd, IDC_GDIOVERLAY, BM_SETCHECK, Config->GDIOverlay == 1, 0);
	SendDlgItemMessage(hWnd, IDC_ABSANIM, BM_SETCHECK, Config->bAbsAnims == 1, 0);
	SendDlgItemMessage(hWnd, IDC_CLOUDNORM, BM_SETCHECK, Config->bCloudNormals == 1, 0);
	SendDlgItemMessage(hWnd, IDC_FLATS, BM_SETCHECK, Config->bFlats == 1, 0);
	SendDlgItemMessage(hWnd, IDC_ESUNGLARE, BM_SETCHECK, Config->bGlares == 1, 0);
	SendDlgItemMessage(hWnd, IDC_ELIGHTSGLARE, BM_SETCHECK, Config->bLocalGlares == 1, 0);
	SendDlgItemMessage(hWnd, IDC_EIRRAD, BM_SETCHECK, Config->bIrradiance == 1, 0);
	SendDlgItemMessage(hWnd, IDC_ESCACHE, BM_SETCHECK, Config->ShaderCacheUse == 1, 0);
	SendDlgItemMessage(hWnd, IDC_EAQUALITY, BM_SETCHECK, Config->bAtmoQuality == 1, 0);


	SendDlgItemMessage(hWnd, IDC_NORMALMAPS, BM_SETCHECK, Config->UseNormalMap==1, 0);
	SendDlgItemMessage(hWnd, IDC_BASEVIS,    BM_SETCHECK, Config->PreLBaseVis==1, 0);
	SendDlgItemMessage(hWnd, IDC_NEARPLANE,  BM_SETCHECK, Config->NearClipPlane==1, 0);
	SendDlgItemMessage(hWnd, IDC_BREAK,		 BM_SETCHECK, Config->DebugBreak == 1, 0);
	
	sprintf_s(cbuf,32,"%d", Config->PlanetLoadFrequency);
	SetWindowText(GetDlgItem(hWnd, IDC_HZ), cbuf);

	sprintf_s(cbuf,32,"%3.3f", Config->PlanetGlow);
	SetWindowText(GetDlgItem(hWnd, IDC_PLANETGLOW), cbuf);

	DWORD af = min(caps.MaxAnisotropy, DWORD(Config->Anisotrophy));

	switch(af) {
		case 2: SendDlgItemMessage(hWnd, IDC_AF, CB_SETCURSEL, 0, 0); break;
		default:
		case 4: SendDlgItemMessage(hWnd, IDC_AF, CB_SETCURSEL, 1, 0); break;
		case 8: SendDlgItemMessage(hWnd, IDC_AF, CB_SETCURSEL, 2, 0); break;
		case 12: SendDlgItemMessage(hWnd, IDC_AF, CB_SETCURSEL, 3, 0); break;
		case 16: SendDlgItemMessage(hWnd, IDC_AF, CB_SETCURSEL, 4, 0); break;
	}

	DWORD aa = min(aamax, DWORD(Config->SceneAntialias));

	switch(aa) {
		case 0: SendDlgItemMessage(hWnd, IDC_AA, CB_SETCURSEL, 0, 0); break;
		case 2: SendDlgItemMessage(hWnd, IDC_AA, CB_SETCURSEL, 1, 0); break;
		default:
		case 4: SendDlgItemMessage(hWnd, IDC_AA, CB_SETCURSEL, 2, 0); break;
		case 8: SendDlgItemMessage(hWnd, IDC_AA, CB_SETCURSEL, 3, 0); break;
	}
}




void VideoTab::SaveSetupState(HWND hWnd)
{
	char cbuf[32];
	// Combo boxes
	Config->SketchpadFont = (int)SendDlgItemMessage (hWnd, IDC_FONT, CB_GETCURSEL, 0, 0);
	Config->EnvMapMode	  = (int)SendDlgItemMessage (hWnd, IDC_ENVMODE, CB_GETCURSEL, 0, 0);
	Config->CustomCamMode = (int)SendDlgItemMessage (hWnd, IDC_CAMMODE, CB_GETCURSEL, 0, 0);
	Config->EnvMapFaces	  = (int)SendDlgItemMessage (hWnd, IDC_ENVFACES, CB_GETCURSEL, 0, 0) + 1;
	Config->TextureMips	  = (int)SendDlgItemMessage (hWnd, IDC_TEXMIPS, CB_GETCURSEL, 0, 0);
	Config->MicroMode	  = (int)SendDlgItemMessage (hWnd, IDC_MICROMODE, CB_GETCURSEL, 0, 0);
	Config->MicroFilter	  = (int)SendDlgItemMessage (hWnd, IDC_MICROFILTER, CB_GETCURSEL, 0, 0);
	Config->BlendMode	  = (int)SendDlgItemMessage (hWnd, IDC_BLENDMODE, CB_GETCURSEL, 0, 0);
	Config->TileMipmaps   = (int)SendDlgItemMessage (hWnd, IDC_MIPMAPS, CB_GETCURSEL, 0, 0);
	Config->PostProcess   = (int)SendDlgItemMessage (hWnd, IDC_POSTPROCESS, CB_GETCURSEL, 0, 0);
	Config->PlanetTileLoadFlags = (int)SendDlgItemMessage (hWnd, IDC_ARCHIVE, CB_GETCURSEL, 0, 0) + 1;
	Config->LightConfig   = (int)SendDlgItemMessage(hWnd, IDC_LIGHTCONFIG, CB_GETCURSEL, 0, 0);
	Config->ShadowMapMode = (int)SendDlgItemMessage(hWnd, IDC_SELFSHADOWS, CB_GETCURSEL, 0, 0);
	Config->ShadowFilter  = (int)SendDlgItemMessage(hWnd, IDC_SHADOWFILTER, CB_GETCURSEL, 0, 0);
	Config->TerrainShadowing = (int)SendDlgItemMessage(hWnd, IDC_TERRAIN, CB_GETCURSEL, 0, 0);
	Config->gcGUIMode = (int)SendDlgItemMessage(hWnd, IDC_GUIMODE, CB_GETCURSEL, 0, 0);
	Config->VCCascadeCount = (int)SendDlgItemMessage(hWnd, IDC_CASCOUNT, CB_GETCURSEL, 0, 0) + 1;
	Config->MeshRes		  = int(SendDlgItemMessage(hWnd, IDC_MESHRES, CB_GETCURSEL, 0, 0));
	Config->MaxTiles	  = int(SendDlgItemMessage(hWnd, IDC_TILECOUNT, CB_GETCURSEL, 0, 0));


	if (Config->gcGUIMode == 1) Config->gcGUIMode = 0;

	// Check boxes
	Config->UseNormalMap  = (int)SendDlgItemMessage (hWnd, IDC_NORMALMAPS, BM_GETCHECK, 0, 0);
	Config->PreLBaseVis   = (int)SendDlgItemMessage (hWnd, IDC_BASEVIS,    BM_GETCHECK, 0, 0);
	Config->NearClipPlane = (int)SendDlgItemMessage (hWnd, IDC_NEARPLANE,  BM_GETCHECK, 0, 0);
	Config->EnableGlass   = (int)SendDlgItemMessage (hWnd, IDC_GLASSSHADE,  BM_GETCHECK, 0, 0);
	Config->EnableMeshDbg = (int)SendDlgItemMessage (hWnd, IDC_MESH_DEBUGGER,  BM_GETCHECK, 0, 0);
	Config->CloudMicro    = (int)SendDlgItemMessage (hWnd, IDC_CLOUDMICRO, BM_GETCHECK, 0, 0);
	Config->GDIOverlay	  = (int)SendDlgItemMessage (hWnd, IDC_GDIOVERLAY, BM_GETCHECK, 0, 0);
	Config->bAbsAnims	  = (int)SendDlgItemMessage (hWnd, IDC_ABSANIM, BM_GETCHECK, 0, 0);
	Config->bCloudNormals = (int)SendDlgItemMessage(hWnd, IDC_CLOUDNORM, BM_GETCHECK, 0, 0);
	Config->bFlats		  = (int)SendDlgItemMessage(hWnd, IDC_FLATS, BM_GETCHECK, 0, 0);
	Config->DebugBreak	  = (int)SendDlgItemMessage(hWnd, IDC_BREAK, BM_GETCHECK, 0, 0);
	Config->bGlares		  = (int)SendDlgItemMessage(hWnd, IDC_ESUNGLARE, BM_GETCHECK, 0, 0);
	Config->bLocalGlares  = (int)SendDlgItemMessage(hWnd, IDC_ELIGHTSGLARE, BM_GETCHECK, 0, 0);
	Config->bIrradiance   = (int)SendDlgItemMessage(hWnd, IDC_EIRRAD, BM_GETCHECK, 0, 0);
	Config->ShaderCacheUse= (int)SendDlgItemMessage(hWnd, IDC_ESCACHE, BM_GETCHECK, 0, 0);
	Config->bAtmoQuality  = (int)SendDlgItemMessage(hWnd, IDC_EAQUALITY, BM_GETCHECK, 0, 0);

	// Sliders
	Config->Convergence   = double(SendDlgItemMessage(hWnd, IDC_CONVERGENCE, TBM_GETPOS, 0, 0)) * 0.01;
	Config->Separation	  = double(SendDlgItemMessage(hWnd, IDC_SEPARATION,  TBM_GETPOS, 0, 0));
	Config->LODBias       = 0.2 * double(SendDlgItemMessage(hWnd, IDC_LODBIAS,  TBM_GETPOS, 0, 0));
	Config->MicroBias     = int(SendDlgItemMessage(hWnd, IDC_MICROBIAS,  TBM_GETPOS, 0, 0));

	// Other things
	GetWindowText(GetDlgItem(hWnd, IDC_HZ),  cbuf, 32);

	Config->PlanetLoadFrequency = atoi(cbuf);
	Config->PlanetPreloadMode = (int)SendDlgItemMessage (hWnd, IDC_SRFPRELOAD, BM_GETCHECK, 0, 0);

	GetWindowText(GetDlgItem(hWnd, IDC_PLANETGLOW),  cbuf, 32);
	Config->PlanetGlow = atof(cbuf);

	Config->DebugLvl = (int)SendDlgItemMessage (hWnd, IDC_DEBUG, CB_GETCURSEL, 0, 0);

	switch(SendDlgItemMessage (hWnd, IDC_AF, CB_GETCURSEL, 0, 0)) {
		default:
		case 0: Config->Anisotrophy = 2; break;
		case 1: Config->Anisotrophy = 4; break;
		case 2: Config->Anisotrophy = 8; break;
		case 3: Config->Anisotrophy = 12; break;
		case 4: Config->Anisotrophy = 16; break;
	}

	switch(SendDlgItemMessage (hWnd, IDC_AA, CB_GETCURSEL, 0, 0)) {
		default:
		case 0: Config->SceneAntialias = 0; break;
		case 1: Config->SceneAntialias = 2; break;
		case 2: Config->SceneAntialias = 4; break;
		case 3: Config->SceneAntialias = 8; break;
	}

	int EASel = (int)SendDlgItemMessage(hWnd, IDC_EARTHVISCFG, CB_GETCURSEL, 0, 0);
	if (!AtmoCfgs["Earth"][EASel].file.empty()) Config->AtmoCfg["Earth"] = AtmoCfgs["Earth"][EASel].file;
	else Config->AtmoCfg["Earth"] = "Earth.atm.cfg";
}






void VideoTab::CreateSymbolicLinks()
{
	// Ask user
	//
	int ret = MessageBox(NULL, "This function will create a symbolic links in /Modules/Server/ folder "
								"as required by some addons like the spacecraft3.dll.\n\n"
								"Do you want to proceed ?", "D3D9Client Configuration", MB_YESNO);
	if (ret != IDYES) {
		return;
	}

	std::string result("");

	// Config -> Modules/Server/Config
	//
	result += "Config: ";
	if (junction::TargetDirectoryExists(OapiExtension::GetConfigDir()))
	{
		if (!junction::IsDirectoryJunction("Modules\\Server\\Config"))
		{
			if (!junction::CreateJunctionPoint(OapiExtension::GetConfigDir(), "Modules\\Server\\Config"))
			{
				result += (GetLastError() == ERROR_DIR_NOT_EMPTY)
						? "OK. A non-empty 'Config' directory already exists."
						: "FAIL. Could not create link.";
			} else {
				result += "OK. Link created.";
			}
		} else {
			result += "OK. Link exists.";
		}
	} else {
		result += "FAIL. Target does not exist!";
	}
	result += "\r\n";

	// Sound -> Modules/Server/Sound
	//
	if (OapiExtension::RunsOrbiter2010())
	{
		result += "Sound: ";
		if (junction::TargetDirectoryExists("Sound"))
		{
			if (OapiExtension::RunsOrbiterSound40()) {
				result += "OK. OrbiterSound (4.0) detected. No link necessary.";
			}
			else if (!junction::IsDirectoryJunction("Modules\\Server\\Sound"))
			{
				if (!junction::CreateJunctionPoint("Sound", "Modules\\Server\\Sound"))
				{
					result += (GetLastError() == ERROR_DIR_NOT_EMPTY)
							? "OK. A non-empty 'Sound' directory already exists."
							: "FAIL. Could not create link.";
				}
				else {
					result += "OK. Link created.";
				}
			}
			else {
				result += "OK. Link exists.";
			}
		}
		else {
			result += "OK. OrbiterSound not installed.";
		}
		result += "\r\n";
	}

	MessageBox(NULL, result.c_str(), "D3D9Client Configuration", MB_OK);
}






// ***************************************************************************************************
// Credist Dialog
// ***************************************************************************************************

INT_PTR CALLBACK VideoTab::CreditsDlgProcWrp(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static class VideoTab *VTab = NULL;
	switch (uMsg) {
		case WM_INITDIALOG: 
			VTab = (class VideoTab *)lParam;
			VTab->InitCreditsDialog(hWnd);
			return true;
		case WM_COMMAND:
			if (VTab) VTab->CreditsDlgProc(hWnd, uMsg, wParam, lParam);
	}
	return false;
}



INT_PTR CALLBACK VideoTab::CreditsDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (LOWORD(wParam)) {
		case IDOK:
		case IDCANCEL:
			EndDialog (hWnd, 0);
			break;
	}
	return false;
}

void VideoTab::InitCreditsDialog(HWND hWnd)
{
	HANDLE hFile = CreateFile("Modules/D3D9Client/Credits.rtf", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL); 

	if (hFile==INVALID_HANDLE_VALUE) {
		LogErr("Failed to open a file /Modules/D3D9Client/Credits.rtf");
		return;
	}

	DWORD size = GetFileSize(hFile, NULL);
	char *credits = new char[size+1];
	memset(credits,0,size+1);
	DWORD bytes;

	if (ReadFile(hFile, credits, size, &bytes, NULL)) {
		SETTEXTEX text;
		text.flags = ST_DEFAULT;
		text.codepage = CP_ACP;
		SendDlgItemMessageA(hWnd, IDC_CREDITSTEXT, EM_SETTEXTEX, (WPARAM)&text, (LPARAM)credits);
	}
	else LogErr("Failed to read a file \\Modules\\D3D9Client\\Credits.rtf Error=%u",GetLastError());

	delete []credits;
	credits = NULL;

	CloseHandle(hFile);
}

bool VideoTab::GetConfigName(const char* file, string& cfg, string& planet)
{
	string filename = "GC\\" + string(file);
	FILEHANDLE hFile = oapiOpenFile(filename.c_str(), FILE_IN_ZEROONFAIL, CONFIG);
	if (hFile) {
		char ConfigName[32] = {}; char PlanetName[32] = {};
		bool bA = oapiReadItem_string(hFile, (char*)"ConfigName", ConfigName);
		bool bB = oapiReadItem_string(hFile, (char*)"Planet", PlanetName);
		oapiCloseFile(hFile, FILE_IN_ZEROONFAIL);
		cfg = string(ConfigName);
		planet = string(PlanetName);
		return bA && bB;
	}
	return false;
}

void VideoTab::ScanAtmoCfgs()
{
	_AtmoCfg cfg = { "Default", "Earth.atm.cfg"};
	AtmoCfgs["Earth"].push_back(cfg);

	WIN32_FIND_DATA FileInformation;
	string name = string(OapiExtension::GetConfigDir()) + "GC\\*_atm.cfg";
	HANDLE hFile = FindFirstFileA(name.c_str(), &FileInformation);

	if (hFile != INVALID_HANDLE_VALUE) {
		do {
			if (FileInformation.cFileName[0] != '.') {
				if (!(FileInformation.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {		
					string cfgname, planet;
					if (GetConfigName(FileInformation.cFileName, cfgname, planet)) {
						_AtmoCfg cfg = { cfgname, FileInformation.cFileName };
						AtmoCfgs[planet].push_back(cfg);
					}
					else oapiWriteLogV("File Not Found [%s]", FileInformation.cFileName);
				}
			}
		}
		while (FindNextFileA(hFile, &FileInformation) == TRUE);
		FindClose(hFile);
	}
}



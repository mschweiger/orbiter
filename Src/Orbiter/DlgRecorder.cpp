// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
// Flight recorder dialog
// ======================================================================

#define STRICT 1

#include "DlgRecorder.h"
#include "Orbiter.h"
#include "resource.h"
#include "resource2.h"

extern Orbiter *g_pOrbiter;
extern Vessel *g_focusobj;
extern HELPCONTEXT DefHelpContext;

static int RecItemEx[4] = {IDC_REC_GRPADVANCED, IDC_REC_WARP, IDC_REC_SYSSAMPLE, IDC_REC_RECFOCUS};
static int RecItemStd[3] = {IDC_REC_ROLLDOWN, IDC_REC_HELP1, IDC_REC_CANCEL1};

// ======================================================================

DlgRecorder::DlgRecorder (HINSTANCE hInstance, HWND hParent, void *context)
: DialogWin (hInstance, hParent, IDD_RECPLAY, 0, 0, context)
{
}

// ======================================================================

void DlgRecorder::SetupDialog (HWND hDlg)
{
	static int RecItem[4] = {IDC_REC_GRPRECORD, IDC_REC_SCNLABEL, IDC_REC_SCENARIO, IDC_REC_ROLLDOWN};
	static int RecItem2[3] = {IDC_REC_SCNLABEL, IDC_REC_SCENARIO, IDC_REC_SYSSAMPLE};
	static int PlayItem[6] = {IDC_REC_GRPREPLAY, IDC_REC_SHOWNOTES, IDC_REC_PLAYRECSPEED, IDC_REC_USECAM, IDC_REC_USEFOCUS, IDC_REC_EDITOR};
	static const char *statestr[3] = {"Status:\nNormal", "Status:\nRecording", "Status:\nPlaying"};

	int i, status = g_pOrbiter->RecorderStatus();

	for (i = 0; i < 3; i++)
		EnableWindow (GetDlgItem (hDlg, RecItem2[i]), status != 1);
	if (status == 2) {
		for (i = 0; i < 4; i++) ShowWindow (GetDlgItem (hDlg, RecItem[i]), FALSE);
		for (i = 0; i < 6; i++) ShowWindow (GetDlgItem (hDlg, PlayItem[i]), TRUE);
	} else {
		for (i = 0; i < 6; i++) ShowWindow (GetDlgItem (hDlg, PlayItem[i]), FALSE);
		for (i = 0; i < 4; i++) ShowWindow (GetDlgItem (hDlg, RecItem[i]), TRUE);
		ShowWindow (GetDlgItem (hDlg, IDC_REC_ROLLDOWN), IsWindowVisible (GetDlgItem (hDlg, IDC_REC_GRPADVANCED)) ? FALSE:TRUE);
	}

	int img = (status == 0 ? IDI_REC_ICON : IDI_STOP_ICON);
	SendDlgItemMessage (hDlg, IDC_REC_CTRL, BM_SETIMAGE, IMAGE_ICON,
		(LPARAM)LoadImage (g_pOrbiter->GetInstance(), MAKEINTRESOURCE(img), IMAGE_ICON, 32, 32, LR_SHARED));
	SetWindowText (GetDlgItem (hDlg, IDC_REC_STATUS), statestr[status]);
	SendDlgItemMessage (hDlg, IDC_REC_WARP, BM_SETCHECK, g_pOrbiter->Cfg()->CfgRecPlayPrm.bRecordWarp ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_REC_PLAYRECSPEED, BM_SETCHECK, g_pOrbiter->Cfg()->CfgRecPlayPrm.bReplayWarp ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_REC_RECFOCUS, BM_SETCHECK, g_pOrbiter->Cfg()->CfgRecPlayPrm.bRecordFocus ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_REC_USEFOCUS, BM_SETCHECK, g_pOrbiter->Cfg()->CfgRecPlayPrm.bReplayFocus ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_REC_USECAM, BM_SETCHECK, g_pOrbiter->Cfg()->CfgRecPlayPrm.bReplayCam ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_REC_SYSSAMPLE, BM_SETCHECK, g_pOrbiter->Cfg()->CfgRecPlayPrm.bSysInterval ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_REC_SHOWNOTES, BM_SETCHECK, g_pOrbiter->Cfg()->CfgRecPlayPrm.bShowNotes ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_REC_ATTECL, BM_SETCHECK, g_pOrbiter->Cfg()->CfgRecPlayPrm.RecordAttFrame == 0 ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_REC_ATTHOR, BM_SETCHECK, g_pOrbiter->Cfg()->CfgRecPlayPrm.RecordAttFrame == 1 ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_REC_POSECL, BM_SETCHECK, g_pOrbiter->Cfg()->CfgRecPlayPrm.RecordPosFrame == 0 ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_REC_POSEQU, BM_SETCHECK, g_pOrbiter->Cfg()->CfgRecPlayPrm.RecordPosFrame == 1 ? BST_CHECKED:BST_UNCHECKED, 0);
}

// ======================================================================

void DlgRecorder::GetRecordName (char *str, int maxlen) const
{
	GetWindowText (GetDlgItem (hWnd, IDC_REC_SCENARIO), str, maxlen);
}

// ======================================================================

void DlgRecorder::ShowAdvancedRec (HWND hDlg)
{
	int i;
	SetWindowPos (hDlg, HWND_TOP, 0, 0, RecorderDlg_w, RecorderDlg_h2, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
	for (i = 0; i < 3; i++)
		ShowWindow (GetDlgItem (hDlg, RecItemStd[i]), SW_HIDE);
	for (i = 0; i < 4; i++)
		ShowWindow (GetDlgItem (hDlg, RecItemEx[i]), SW_SHOW);
}

// ======================================================================

void DlgRecorder::HideAdvancedRec (HWND hDlg)
{
	int i;
	SetWindowPos (hDlg, HWND_TOP, 0, 0, RecorderDlg_w, RecorderDlg_h1, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
	for (i = 0; i < 4; i++)
		ShowWindow (GetDlgItem (hDlg, RecItemEx[i]), SW_HIDE);
	for (i = 0; i < 3; i++)
		ShowWindow (GetDlgItem (hDlg, RecItemStd[i]), SW_SHOW);
}

// ======================================================================

BOOL DlgRecorder::OnInitDialog (HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	RECT r1, r2;
	GetWindowRect (hDlg, &r1);
	GetWindowRect (GetDlgItem (hDlg, IDC_REC_ROLLDOWN), &r2);
	RecorderDlg_w = r1.right-r1.left;
	RecorderDlg_h2 = r1.bottom-r1.top;
	RecorderDlg_h1 = r2.bottom-r1.top + ((r2.bottom-r2.top)*2)/3;
	HideAdvancedRec (hDlg);
	SetupDialog (hDlg);
	SetWindowText (GetDlgItem (hDlg, IDC_REC_SCENARIO), g_pOrbiter->GetDefRecordName());
	SendDlgItemMessage (hDlg, IDC_REC_ROLLDOWN, BM_SETIMAGE, IMAGE_BITMAP,
		(LPARAM)LoadImage (hInst, MAKEINTRESOURCE(IDB_DNARROW), IMAGE_BITMAP, 0, 0, LR_SHARED|LR_LOADTRANSPARENT|LR_LOADMAP3DCOLORS));
	SendDlgItemMessage (hDlg, IDC_REC_ROLLUP, BM_SETIMAGE, IMAGE_BITMAP,
		(LPARAM)LoadImage (hInst, MAKEINTRESOURCE(IDB_UPARROW), IMAGE_BITMAP, 0, 0, LR_SHARED|LR_LOADTRANSPARENT|LR_LOADMAP3DCOLORS));

	return TRUE;
}

// ======================================================================

BOOL DlgRecorder::OnCommand (HWND hDlg, WORD id, WORD code, HWND hControl)
{
	switch (id) {
	case IDC_REC_CTRL:
		switch (g_pOrbiter->RecorderStatus()) {
		case 0:
		case 1:
			g_pOrbiter->ToggleRecorder ();
			break;
		case 2:
			g_pOrbiter->EndPlayback();
			break;
		}
		break;
	case IDC_REC_ROLLDOWN:
		ShowAdvancedRec (hDlg);
		break;
	case IDC_REC_ROLLUP:
		HideAdvancedRec (hDlg);
		break;
	case IDC_REC_EDITOR:
		g_pOrbiter->FRecorder_ToggleEditor();
		break;
	case IDC_REC_WARP:
		g_pOrbiter->Cfg()->CfgRecPlayPrm.bRecordWarp = (SendDlgItemMessage (hDlg, IDC_REC_WARP, BM_GETCHECK, 0, 0) == BST_CHECKED);
		break;
	case IDC_REC_PLAYRECSPEED:
		g_pOrbiter->Cfg()->CfgRecPlayPrm.bReplayWarp = (SendDlgItemMessage (hDlg, IDC_REC_PLAYRECSPEED, BM_GETCHECK, 0, 0) == BST_CHECKED);
		if (g_pOrbiter->Cfg()->CfgRecPlayPrm.bReplayWarp) {
			extern double RecordingSpeed;
			g_pOrbiter->SetWarpFactor (RecordingSpeed, true);
		}
		break;
	case IDC_REC_USEFOCUS:
		g_pOrbiter->Cfg()->CfgRecPlayPrm.bReplayFocus = (SendDlgItemMessage (hDlg, IDC_REC_USEFOCUS, BM_GETCHECK, 0, 0) == BST_CHECKED);
		if (g_pOrbiter->Cfg()->CfgRecPlayPrm.bReplayFocus) {
			extern Vessel *vfocus;
			if (vfocus && vfocus != g_focusobj) g_pOrbiter->SetFocusObject (vfocus);
			// do we need to check if vfocus is valid?
		}
		break;
	case IDC_REC_USECAM:
		g_pOrbiter->Cfg()->CfgRecPlayPrm.bReplayCam = (SendDlgItemMessage (hDlg, IDC_REC_USECAM, BM_GETCHECK, 0, 0) == BST_CHECKED);
		if (g_pOrbiter->Cfg()->CfgRecPlayPrm.bReplayCam) {
			// should set camera according to current playback status here!
		}
		break;
	case IDC_REC_SYSSAMPLE:
		g_pOrbiter->Cfg()->CfgRecPlayPrm.bSysInterval = (SendDlgItemMessage (hDlg, IDC_REC_SYSSAMPLE, BM_GETCHECK, 0, 0) == BST_CHECKED);
		break;
	case IDC_REC_SHOWNOTES:
		g_pOrbiter->Cfg()->CfgRecPlayPrm.bShowNotes = (SendDlgItemMessage (hDlg, IDC_REC_SHOWNOTES, BM_GETCHECK, 0, 0) == BST_CHECKED);
		break;
	case IDC_REC_ATTECL:
		if (SendDlgItemMessage (hDlg, IDC_REC_ATTECL, BM_GETCHECK, 0, 0) == BST_CHECKED)
			g_pOrbiter->Cfg()->CfgRecPlayPrm.RecordAttFrame = 0;
		break;
	case IDC_REC_ATTHOR:
		if (SendDlgItemMessage (hDlg, IDC_REC_ATTHOR, BM_GETCHECK, 0, 0) == BST_CHECKED)
			g_pOrbiter->Cfg()->CfgRecPlayPrm.RecordAttFrame = 1;
		break;
	case IDC_REC_POSECL:
		if (SendDlgItemMessage (hDlg, IDC_REC_POSECL, BM_GETCHECK, 0, 0) == BST_CHECKED)
			g_pOrbiter->Cfg()->CfgRecPlayPrm.RecordPosFrame = 0;
		break;
	case IDC_REC_POSEQU:
		if (SendDlgItemMessage (hDlg, IDC_REC_POSEQU, BM_GETCHECK, 0, 0) == BST_CHECKED)
			g_pOrbiter->Cfg()->CfgRecPlayPrm.RecordPosFrame = 1;
		break;
	case IDC_REC_HELP1:
	case IDC_REC_HELP2:
		DefHelpContext.topic = (char*)"/recorder.htm";
		g_pOrbiter->OpenHelp (&DefHelpContext);
		return TRUE;
	case IDCANCEL:
	case IDC_REC_CANCEL1:
	case IDC_REC_CANCEL2:
		g_pOrbiter->CloseDialog (hDlg);
		break;
	}
	return DialogWin::OnCommand (hDlg, id, code, hControl);
}

// ======================================================================

BOOL DlgRecorder::OnUser1 (HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	SetupDialog (hDlg);
	return TRUE;
}

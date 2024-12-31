// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
// Screen capture dialog
// ======================================================================

#define STRICT 1

#include <windows.h>
#include "DlgCapture.h"
#include "Orbiter.h"
#include "resource.h"
#include "resource2.h"
#include <string>

extern Orbiter *g_pOrbiter;
extern HELPCONTEXT DefHelpContext;

// ======================================================================

DlgCapture::DlgCapture (HINSTANCE hInstance, HWND hParent, void *context)
: DialogWin (hInstance, hParent, IDD_CAPTURE, 0, 0, context)
{
}

// ======================================================================

DlgCapture::~DlgCapture ()
{
}

// ======================================================================

BOOL DlgCapture::OnInitDialog (HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	const char *fmtstr[4] = {".bmp",".png",".jpg",".tif"};
	char cbuf[256];
	int i;
	CFG_CAPTUREPRM &prm = g_pOrbiter->Cfg()->CfgCapturePrm;
	SendDlgItemMessage (hDlg, IDC_CAP_TOCLIP, BM_SETCHECK, prm.ImageTgt == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage (hDlg, IDC_CAP_TOFILE, BM_SETCHECK, prm.ImageTgt == 0 ? BST_UNCHECKED : BST_CHECKED, 0);
	EnableWindow (GetDlgItem (hDlg, IDC_CAP_FNAME), prm.ImageTgt == 0 ? FALSE:TRUE);
	SetWindowText (GetDlgItem (hDlg, IDC_CAP_FNAME), prm.ImageFile);
	SetWindowText (GetDlgItem (hDlg, IDC_CAP_DNAME), prm.SequenceDir);
	sprintf (cbuf, "%05d", prm.SequenceStart);
	SetWindowText (GetDlgItem (hDlg, IDC_CAP_STARTCOUNT), cbuf);
	sprintf (cbuf, "%d", prm.SequenceSkip);
	SetWindowText (GetDlgItem (hDlg, IDC_CAP_SKIPFRAME), cbuf);

	SendDlgItemMessage (hDlg, IDC_COMBO1, CB_RESETCONTENT, 0, 0);
	for (i = 0; i < 4; i++)
		SendDlgItemMessage (hDlg, IDC_COMBO1, CB_INSERTSTRING, -1, (LPARAM)fmtstr[i]);
	SendDlgItemMessage (hDlg, IDC_COMBO1, CB_SETCURSEL, prm.ImageFormat, 0);
	EnableWindow (GetDlgItem (hDlg, IDC_COMBO2), prm.ImageFormat == 2 ? TRUE:FALSE);

	SendDlgItemMessage (hDlg, IDC_COMBO2, CB_RESETCONTENT, 0, 0);
	for (i = 1; i <= 10; i++) {
		SendDlgItemMessage(hDlg, IDC_COMBO2, CB_INSERTSTRING, -1, (LPARAM)std::to_string(i).data());
	}
	SendDlgItemMessage (hDlg, IDC_COMBO2, CB_SETCURSEL, prm.ImageQuality-1, 0);

	return TRUE;
}

// ======================================================================

BOOL DlgCapture::OnCommand (HWND hDlg, WORD id, WORD code, HWND hControl)
{
	switch (id) {
	case IDCANCEL:
		Take (hDlg);
		g_pOrbiter->CloseDialog (hDlg);
		return TRUE;
	case IDC_COMBO1:
		if (code == CBN_SELCHANGE) {
			int idx = SendDlgItemMessage (hDlg, id, CB_GETCURSEL, 0, 0);
			g_pOrbiter->Cfg()->CfgCapturePrm.ImageFormat = idx;
			EnableWindow (GetDlgItem (hDlg, IDC_COMBO2), idx == 2 ? TRUE:FALSE);
			return TRUE;
		}
		break;
	case IDC_COMBO2:
		if (code == CBN_SELCHANGE) {
			int idx = SendDlgItemMessage (hDlg, id, CB_GETCURSEL, 0, 0);
			g_pOrbiter->Cfg()->CfgCapturePrm.ImageQuality = idx+1;
			return TRUE;
		}
		break;
	case IDC_CAP_SNAPSHOT: {
		oapi::GraphicsClient *gclient = g_pOrbiter->GetGraphicsClient();
		if (gclient) {
			Take (hDlg);
			if (g_pOrbiter->Cfg()->CfgCapturePrm.ImageTgt) {
				oapi::ImageFileFormat fmt = (oapi::ImageFileFormat)g_pOrbiter->Cfg()->CfgCapturePrm.ImageFormat;
				float quality = (float)g_pOrbiter->Cfg()->CfgCapturePrm.ImageQuality/10.0f;
				gclient->clbkSaveSurfaceToImage (0, g_pOrbiter->Cfg()->CfgCapturePrm.ImageFile, fmt, quality);
				AutoIncrement (hDlg);
				GetWindowText (GetDlgItem (hDlg, IDC_CAP_FNAME), g_pOrbiter->Cfg()->CfgCapturePrm.ImageFile, 128);
			} else
				gclient->clbkSaveSurfaceToImage (0, NULL, oapi::IMAGE_BMP);
		}
		} return TRUE;
	case IDC_CAP_RECORD:
		if (g_pOrbiter->IsCapturingFrames()) {
			g_pOrbiter->StopCaptureFrames();
			SetWindowText (GetDlgItem (hDlg, IDC_CAP_RECORD), "Start recording");
		} else if (Take (hDlg)) {
			g_pOrbiter->StartCaptureFrames();
			SetWindowText (GetDlgItem (hDlg, IDC_CAP_RECORD), "Stop recording");
		}
		return TRUE;
	case IDC_BUTTON1:
		g_pOrbiter->Cfg()->SetDefaults_Capture();
		OnInitDialog (hDlg, 0, 0);
		return TRUE;
	case IDHELP: {
		char *topic = (char*)"/capture.htm";
		DefHelpContext.topic = topic;
		g_pOrbiter->OpenHelp (&DefHelpContext);
		} return TRUE;
	case IDC_CAP_TOCLIP:
	case IDC_CAP_TOFILE:
		if (code == BN_CLICKED) {
			Take (hDlg);
			return TRUE;
		}
		break;
	}
	return DialogWin::OnCommand (hDlg, id, code, hControl);
}

// ======================================================================

void DlgCapture::Update ()
{
	if (g_pOrbiter->IsCapturingFrames()) {
		char cbuf[256];
		int curframe = g_pOrbiter->Cfg()->CfgCapturePrm.SequenceStart;
		int showframe;
		GetWindowText (GetDlgItem (hWnd, IDC_CAP_STARTCOUNT), cbuf, 256);
		if (!sscanf (cbuf, "%d", &showframe) || showframe != curframe) {
			sprintf (cbuf, "%05d", curframe);
			SetWindowText (GetDlgItem (hWnd, IDC_CAP_STARTCOUNT), cbuf);
		}
	}
}

// ======================================================================

bool DlgCapture::Take (HWND hDlg)
{
	bool ok = true;
	char cbuf[256];
	CFG_CAPTUREPRM &prm = g_pOrbiter->Cfg()->CfgCapturePrm;
	bool isClipbd = (SendDlgItemMessage (hDlg, IDC_CAP_TOCLIP, BM_GETCHECK, 0, 0) == TRUE);
	prm.ImageTgt = (isClipbd ? 0 : 1);
	EnableWindow (GetDlgItem (hDlg, IDC_CAP_FNAME), isClipbd ? FALSE:TRUE);
	GetWindowText (GetDlgItem (hDlg, IDC_CAP_FNAME), prm.ImageFile, 128);
	GetWindowText (GetDlgItem (hDlg, IDC_CAP_DNAME), prm.SequenceDir, 128);
	GetWindowText (GetDlgItem (hDlg, IDC_CAP_STARTCOUNT), cbuf, 256);
	if (!sscanf (cbuf, "%d", &prm.SequenceStart)) {
		prm.SequenceStart = 0;
		ok = false;
	}
	GetWindowText (GetDlgItem (hDlg, IDC_CAP_SKIPFRAME), cbuf, 256);
	if (!sscanf (cbuf, "%d", &prm.SequenceSkip)) {
		prm.SequenceSkip = 0;
		ok = false;
	}
	return ok;
}

// ======================================================================

bool DlgCapture::AutoIncrement (HWND hDlg)
{
	char cbuf[256];
	GetWindowText (GetDlgItem (hDlg, IDC_CAP_FNAME), cbuf, 256);
	int i, count, len = strlen(cbuf);
	for (i = len; i > 0; i--)
		if (cbuf[i-1] == '\\') break;
	if (sscanf (cbuf+i, "%d", &count) == 1) {
		int w = len-i;
		sprintf (cbuf+i, "%0*d", w, count+1);
		SetWindowText (GetDlgItem (hDlg, IDC_CAP_FNAME), cbuf);
		return true;
	}
	return false;
}

// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
// Camera configuration dialog
// ======================================================================

#ifndef __DLGCAMERA_H
#define __DLGCAMERA_H

#include <windows.h>
#include <commctrl.h>
#include "DialogWin.h"
#include "Celbody.h"
#include "Camera.h"

class CameraTab;
class TabControl;
class TabFov;
class TabGround;

// ======================================================================

class DlgCamera: public DialogWin {
public:
	DlgCamera (HINSTANCE hInstance, HWND hParent, void *context);
	~DlgCamera ();
	void Update ();
	void ModeChanged ();
	void FovChanged (double fov);
	void GObserverChanged (const char *str);
	void Refresh (const Camera *cam);
	char *HelpContext () const { return hcontext; }
	BOOL OnInitDialog (HWND hDlg, WPARAM wParam, LPARAM lParam);
	BOOL OnCommand (HWND hDlg, WORD id, WORD code, HWND hControl);
	BOOL OnNotify (HWND hDlg, int idCtrl, LPNMHDR pnmh);
	BOOL OnApp (HWND hDlg, WPARAM wParam, LPARAM lParam);

protected:
	void Clear ();
	int AddTab (HWND hDlg, CameraTab *tab, const char *label);
	void SwitchTab (HWND hDlg);

private:
	int nTab;
	CameraTab **pTab;
	TabControl *pTabControl;
	TabFov *pTabFov;
	TabGround *pTabGround;
	char *hcontext;
};

// ======================================================================
// Base class for camera dialog tabs

class CameraTab {
public:
	CameraTab (HWND hParentTab, int dlgId, DLGPROC dlgProc);
	virtual ~CameraTab ();
	virtual void Show (bool show);
	virtual void Update () {}
	virtual char *HelpContext() const = 0;

protected:
	static INT_PTR CALLBACK DlgProcInit (HWND, UINT, WPARAM, LPARAM);
	HWND hParent;
	HWND hTab;
	bool active;
};

// ======================================================================
// Camera control tab

class TabControl: public CameraTab {
public:
	TabControl (HWND hParentTab);
	char *HelpContext () const;
	void Show (bool show);
	void Update ();
	void ModeChanged ();
	BOOL Init (HWND hWnd);

protected:
	void GetCamMode ();
	void SetLayout ();
	static INT_PTR CALLBACK DlgProc (HWND, UINT, WPARAM, LPARAM);
	ExtCamMode extmode;
	IntCamMode intmode;
	bool extcam;
	bool ground_lock;
	bool rot_is_tilt, rot_is_pan;
};

// ======================================================================
// Camera target selection tab

class TabTarget: public CameraTab {
public:
	TabTarget (HWND hParentTab);
	char *HelpContext () const;
	void Show (bool show);
	BOOL Init (HWND hWnd);

protected:
	void AddCbodyNode (HWND hWnd, const CelestialBody *cbody, HTREEITEM parent);
	void AddVessels (HWND hWnd);
	void AddSurfbases (HWND hWnd);
	static INT_PTR CALLBACK DlgProc (HWND, UINT, WPARAM, LPARAM);
};

// ======================================================================
// Camera track mode tab

class TabView: public CameraTab {
public:
	TabView (HWND hParentTab);
	char *HelpContext () const;
	BOOL Init (HWND hWnd);

protected:
	static INT_PTR CALLBACK DlgProc (HWND, UINT, WPARAM, LPARAM);
};

// ======================================================================
// Camera ground mode tab

class TabGround: public CameraTab {
public:
	TabGround (HWND hParentTab);
	void GObserverChanged (const char *str);
	void LockChanged (bool lock);
	void Show (bool show);
	char *HelpContext () const;
	BOOL Init (HWND hWnd);

protected:
	void SelectPlanet (HWND hWnd, int idx);
	void SelectSite (HWND hWnd, int idx);
	void SelectAddr (HWND hWnd, int idx);
	void ApplyObserver (HWND hWnd);
	void SetCurrentGroundpos (HWND hWnd);
	bool EchoGroundpos (HWND hWnd, double lng, double lat, double alt);
	void EchoCurrentGroundpos (HWND hWnd);
	static INT_PTR CALLBACK DlgProc (HWND, UINT, WPARAM, LPARAM);
};

// ======================================================================
// Camera field of view tab

class TabFov: public CameraTab {
public:
	TabFov (HWND hParentTab);
	void FovChanged (double fov);
	char *HelpContext () const;

protected:
	void RegisterFov (HWND hWnd, double fov);
	void SetFov (double fov_deg);
	static INT_PTR CALLBACK DlgProc (HWND, UINT, WPARAM, LPARAM);
};

// ======================================================================
// Camera preset position management tab

class TabPreset: public CameraTab {
public:
	TabPreset (HWND hParentTab);
	char *HelpContext () const;

protected:
	static INT_PTR CALLBACK DlgProc (HWND, UINT, WPARAM, LPARAM);
};

#endif // !__DLGCAMERA_H

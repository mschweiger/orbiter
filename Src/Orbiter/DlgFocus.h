// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
// Focus vessel selection dialog
// ======================================================================

#ifndef __DLGFOCUS_H
#define __DLGFOCUS_H

#include <windows.h>
#include <commctrl.h>

#include "DialogWin.h"

class DlgFocus: public DialogWin {
public:
	DlgFocus (HINSTANCE hInstance, HWND hParent, void *context);
	~DlgFocus ();
	BOOL OnInitDialog (HWND hWnd, WPARAM wParam, LPARAM lParam);
	BOOL OnSize (HWND hDlg, WPARAM wParam, int w, int h);
	BOOL OnCommand (HWND hWnd, WORD id, WORD code, HWND hControl);
	BOOL OnUserMessage (HWND hWnd, WPARAM wParam, LPARAM lParam);
	BOOL OnNotify (HWND hDlg, int idCtrl, LPNMHDR pnmh);
	BOOL OnHScroll (HWND hDlg, WORD request, WORD curpos, HWND hControl);

protected:
	BOOL OnNotifyTab (HWND hDlg, LPNMHDR pnmh);
	BOOL OnNotifyTree (HWND hDlg, LPNMHDR pnmh);
	void SwitchTab (HWND hDlg);
	void RescanTree_All (HWND hDlg);
	void RescanTree_Nearby (HWND hDlg);
	void RescanTree_Location (HWND hDlg);
	void RescanTree_Class (HWND hDlg);
	HTREEITEM AddVesselToTree (HWND hDlg, HTREEITEM hti, Vessel *vessel, bool force = false);
	BOOL SelectVessel (HWND hDlg);
	void SelectPreviousVessel (HWND hDlg);
	void UpdateFocus (HWND hDlg);
	void WatchEdit (HWND hDlg);
	void WatchAssemblyUnroll (HWND hDlg);
	void SetRangeLabel (HWND hDlg, int irange);
	HTREEITEM FindItem (HWND hDlg, const char *str, HTREEITEM hti_first);

private:
	int ctab;
	int irange;
	bool unroll_assemblies;
	RECT rClient, rTab, rEdit, rTree, rAppl, rPrev, rHelp, rClse, rSldr, rLabl, rChck;
};

#endif // !__DLGFOCUS_H

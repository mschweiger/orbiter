// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
// Focus vessel selection dialog
// ======================================================================

#define STRICT 1

#include "DlgFocus.h"
#include "resource.h"
#include "Orbiter.h"
#include "Psys.h"
#include "SuperVessel.h"
#include "Pane.h"
#include "Camera.h"
#include "Util.h"
#include <uxtheme.h>
#include <iomanip>

extern Orbiter *g_pOrbiter;
extern PlanetarySystem *g_psys;
extern Vessel *g_focusobj, *g_pfocusobj;
extern Camera *g_camera;
extern HELPCONTEXT DefHelpContext;
extern char DBG_MSG[256];

// ======================================================================

DlgFocus::DlgFocus (HINSTANCE hInstance, HWND hParent, void *context)
: DialogWin (hInstance, hParent, IDD_JUMPVESSEL, 0, 0, context)
{
	pos = &g_pOrbiter->Cfg()->CfgWindowPos.DlgFocus;
	irange = g_pOrbiter->Cfg()->CfgUIPrm.SelVesselRange;
	unroll_assemblies = g_pOrbiter->Cfg()->CfgUIPrm.bSelVesselFlat;
}

// ======================================================================

DlgFocus::~DlgFocus ()
{
	g_pOrbiter->Cfg()->CfgUIPrm.SelVesselTab = ctab;
	g_pOrbiter->Cfg()->CfgUIPrm.SelVesselRange = irange;
	g_pOrbiter->Cfg()->CfgUIPrm.bSelVesselFlat = unroll_assemblies;
}

// ======================================================================

BOOL DlgFocus::OnInitDialog (HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	EnableThemeDialogTexture (hWnd, ETDT_ENABLETAB);

	const int ntab = 4;
	const char *label[ntab] = {"All", "Nearby", "Location", "Class"};
	TC_ITEM tie;
	tie.mask = TCIF_TEXT;
	tie.iImage = -1;
	for (int i = 0; i < ntab; i++) {
		tie.pszText = (char*)label[i];
		SendDlgItemMessage (hWnd, IDC_TAB1, TCM_INSERTITEM, i, (LPARAM)&tie);
	}
	ctab = g_pOrbiter->Cfg()->CfgUIPrm.SelVesselTab;

	GetClientRect (hWnd, &rClient);
	rTab  = GetClientPos (hWnd, GetDlgItem (hWnd, IDC_TAB1));
	rEdit = GetClientPos (hWnd, GetDlgItem (hWnd, IDC_EDIT1));
	rTree = GetClientPos (hWnd, GetDlgItem (hWnd, IDC_TREE1));
	rAppl = GetClientPos (hWnd, GetDlgItem (hWnd, IDOK));
	rPrev = GetClientPos (hWnd, GetDlgItem (hWnd, IDC_BUTTON1));
	rHelp = GetClientPos (hWnd, GetDlgItem (hWnd, IDHELP));
	rClse = GetClientPos (hWnd, GetDlgItem (hWnd, IDCANCEL));
	rSldr = GetClientPos (hWnd, GetDlgItem (hWnd, IDC_SLIDER1));
	rLabl = GetClientPos (hWnd, GetDlgItem (hWnd, IDC_STATIC1));
	rChck = GetClientPos (hWnd, GetDlgItem (hWnd, IDC_CHECK1));

	SetWindowText (GetDlgItem (hWnd, IDC_EDIT1), g_focusobj->Name());
	TabCtrl_SetCurSel (GetDlgItem (hWnd, IDC_TAB1), ctab);
	SwitchTab (hWnd);
	EnableWindow (GetDlgItem (hWnd, IDC_BUTTON1), g_pfocusobj ? TRUE:FALSE);	
	EnableWindow (GetDlgItem (hWnd, IDOK), FALSE);

	SendDlgItemMessage (hWnd, IDC_CHECK1, BM_SETCHECK, unroll_assemblies ? BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hWnd, IDC_SLIDER1, TBM_SETRANGE, FALSE, MAKELONG(1,8));
	SendDlgItemMessage (hWnd, IDC_SLIDER1, TBM_SETPAGESIZE, 0, 1L);
	SendDlgItemMessage (hWnd, IDC_SLIDER1, TBM_SETTICFREQ, 1, 0);
	SendDlgItemMessage (hWnd, IDC_SLIDER1, TBM_SETPOS, TRUE, (LONG)irange);
	SetRangeLabel (hWnd, irange);
	
	return TRUE;
}

// ======================================================================

BOOL DlgFocus::OnSize (HWND hDlg, WPARAM wParam, int w, int h)
{
	RECT r;
	int i;
	const int nSizeElm = 9;
	const int SizeElm[nSizeElm] = {
		IDC_EDIT1, IDC_TREE1, IDOK, IDC_BUTTON1, IDC_SLIDER1, IDC_STATIC1, IDC_CHECK1, IDCANCEL, IDHELP
	};

	GetClientRect (hDlg, &r);
	int dw = r.right - rClient.right;
	int dh = r.bottom - rClient.bottom;
	for (i = 0; i < nSizeElm; i++)
		ShowWindow (GetDlgItem (hDlg, SizeElm[i]), SW_HIDE);
	r = rTab; r.right += dw; r.bottom += dh;
	SetClientPos (hDlg, GetDlgItem (hDlg, IDC_TAB1), r);
	r = rEdit; r.right += dw;
	SetClientPos (hDlg, GetDlgItem (hDlg, IDC_EDIT1), r);
	r = rTree; r.right += dw; r.bottom += dh;
	SetClientPos (hDlg, GetDlgItem (hDlg, IDC_TREE1), r);
	r = rAppl; r.left += dw; r.right += dw;
	SetClientPos (hDlg, GetDlgItem (hDlg, IDOK), r);
	r = rPrev; r.left += dw; r.right += dw;
	SetClientPos (hDlg, GetDlgItem (hDlg, IDC_BUTTON1), r);
	r = rClse; r.left += dw; r.right += dw; r.top += dh; r.bottom += dh;
	SetClientPos (hDlg, GetDlgItem (hDlg, IDCANCEL), r);
	r = rHelp; r.left += dw; r.right += dw; r.top += dh; r.bottom += dh;
	SetClientPos (hDlg, GetDlgItem (hDlg, IDHELP), r);
	r = rSldr; r.left += dw; r.right += dw; r.top += dh; r.bottom += dh;
	SetClientPos (hDlg, GetDlgItem (hDlg, IDC_SLIDER1), r);
	r = rLabl; r.left += dw; r.right += dw; r.top += dh; r.bottom += dh;
	SetClientPos (hDlg, GetDlgItem (hDlg, IDC_STATIC1), r);
	r = rChck; r.left += dw; r.right += dw; r.top += dh; r.bottom += dh;
	SetClientPos (hDlg, GetDlgItem (hDlg, IDC_CHECK1), r);
	for (i = 0; i < nSizeElm; i++) {
		if (ctab != 1 && (SizeElm[i] == IDC_SLIDER1 || SizeElm[i] == IDC_STATIC1)) continue;
		if (ctab == 3 && SizeElm[i] == IDC_CHECK1) continue;
		ShowWindow (GetDlgItem (hDlg, SizeElm[i]), SW_SHOW);
	}
	return DialogWin::OnSize (hDlg, wParam, w, h);
}

// ======================================================================

BOOL DlgFocus::OnCommand (HWND hDlg, WORD id, WORD code, HWND hControl)
{
	switch (id) {
	case IDC_EDIT1:
		if (code == EN_CHANGE) WatchEdit (hDlg);
		return TRUE;
	case IDC_CHECK1:
		if (code == BN_CLICKED) WatchAssemblyUnroll (hDlg);
		return TRUE;
	case IDC_BUTTON1:
		SelectPreviousVessel (hDlg);
		return TRUE;
	case IDHELP:
		DefHelpContext.topic = (char*)"/selvessel.htm";
		g_pOrbiter->OpenHelp (&DefHelpContext);
		return TRUE;
	case IDOK:
		return SelectVessel (hDlg);
	}
	return DialogWin::OnCommand (hDlg, id, code, hControl);
}

// ======================================================================

BOOL DlgFocus::OnNotify (HWND hDlg, int idCtrl, LPNMHDR pnmh)
{
	switch (pnmh->idFrom) {
	case IDC_TAB1:
		return OnNotifyTab (hDlg, pnmh);
	case IDC_TREE1:
		return OnNotifyTree (hDlg, pnmh);
	}
	return FALSE;
}

// ======================================================================

BOOL DlgFocus::OnNotifyTab (HWND hDlg, LPNMHDR pnmh)
{
	if (pnmh->code == TCN_SELCHANGE) SwitchTab (hDlg);
	return FALSE;
}

// ======================================================================

BOOL DlgFocus::OnNotifyTree (HWND hDlg, LPNMHDR pnmh)
{
	NM_TREEVIEW *pnmtv = (NM_TREEVIEW FAR *)pnmh;
	if (pnmtv->hdr.code == TVN_SELCHANGED) {
		HTREEITEM hti = pnmtv->itemNew.hItem;
		bool ok = true;
		if (ctab >= 2)
			ok = (SendDlgItemMessage (hDlg, IDC_TREE1, TVM_GETNEXTITEM, TVGN_PARENT, (LPARAM)hti) != 0);
		if (ok) {
			TVITEM tvi;
			char cbuf[256];
			tvi.hItem = hti;
			tvi.pszText = cbuf;
			tvi.cchTextMax = 256;
			tvi.mask = TVIF_TEXT;
			BOOL res = SendDlgItemMessage (hDlg, IDC_TREE1, TVM_GETITEM, 0, (LPARAM)&tvi);
			Vessel *vessel = g_psys->GetVessel (cbuf, true);
			if (!vessel) vessel = g_focusobj;
			strcpy (cbuf, vessel->Name());
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), cbuf);
			EnableWindow (GetDlgItem (hDlg, IDOK), vessel == g_focusobj ? FALSE : TRUE);
		}
	}
	return FALSE;
}

// ======================================================================

BOOL DlgFocus::OnHScroll (HWND hDlg, WORD request, WORD curpos, HWND hControl)
{
	int irange_old = irange;
	irange = SendDlgItemMessage (hDlg, IDC_SLIDER1, TBM_GETPOS, 0, 0);
	if (irange != irange_old) {
		SetRangeLabel (hDlg, irange);
		RescanTree_Nearby (hDlg);
		return TRUE;
	}
	return DialogWin::OnHScroll (hDlg, request, curpos, hControl);
}

// ======================================================================

void DlgFocus::SetRangeLabel (HWND hDlg, int irange)
{
	char cbuf[256] = "Range: ";
	if (irange == 1) strcpy (cbuf+7, "0.1");
	else if (irange <= 4) {
		int i, e=1;
		for (i = 2; i < irange; i++) e *= 10;
		sprintf (cbuf+7, "%d", e);
	} else sprintf (cbuf+7, "1E%d", irange-2);
	strcat (cbuf, " km");
	SetWindowText (GetDlgItem (hDlg, IDC_STATIC1), cbuf);
}

// ======================================================================

void DlgFocus::WatchEdit (HWND hDlg)
{
	char cbuf[256];
	GetWindowText (GetDlgItem (hDlg, IDC_EDIT1), cbuf, 256);
	Vessel *vessel = g_psys->GetVessel (cbuf, true);
	EnableWindow (GetDlgItem (hDlg, IDOK), vessel && vessel != g_focusobj ? TRUE:FALSE);
}

// ======================================================================

void DlgFocus::WatchAssemblyUnroll (HWND hDlg)
{
	bool old_unroll = unroll_assemblies;
	unroll_assemblies = (SendDlgItemMessage (hDlg, IDC_CHECK1, BM_GETCHECK, 0, 0) == BST_CHECKED);
	if (unroll_assemblies != old_unroll) {
		SwitchTab (hDlg);
	}
}

// ======================================================================

BOOL DlgFocus::OnUserMessage (HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	EnableWindow (GetDlgItem (hWnd, IDC_BUTTON1), g_pfocusobj ? TRUE:FALSE);	

	switch (wParam) {
	case MSG_CREATEVESSEL:
	case MSG_KILLVESSEL:
		SwitchTab (hDlg);
		return TRUE;
	case MSG_FOCUSVESSEL:
		UpdateFocus (hDlg);
		return TRUE;
	}
	return FALSE;
}

// ======================================================================

void DlgFocus::SwitchTab (HWND hDlg)
{
	HWND hTab = GetDlgItem (hDlg, IDC_TAB1);
	HWND hTree = GetDlgItem (hDlg, IDC_TREE1);
	ctab = TabCtrl_GetCurSel (hTab);

	LONG ws = GetWindowLongPtr (hTree, GWL_STYLE);
	if (ctab < 2) ws &= ~TVS_LINESATROOT;
	else          ws |=  TVS_LINESATROOT;
	SetWindowLongPtr (hTree, GWL_STYLE, ws);

	switch (ctab) {
	case 0:
		RescanTree_All (hDlg);
		break;
	case 1:
		RescanTree_Nearby (hDlg);
		break;
	case 2:
		RescanTree_Location (hDlg);
		break;
	case 3:
		RescanTree_Class (hDlg);
		break;
	}
	SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), g_focusobj->Name());

	int showrange = (ctab == 1 ? SW_SHOW : SW_HIDE);
	ShowWindow (GetDlgItem (hDlg, IDC_STATIC1), showrange);
	ShowWindow (GetDlgItem (hDlg, IDC_SLIDER1), showrange);
	ShowWindow (GetDlgItem (hDlg, IDC_CHECK1), ctab == 3 ? SW_HIDE : SW_SHOW);
}

// ======================================================================

void DlgFocus::RescanTree_All (HWND hDlg)
{
	char cbuf[256]; cbuf[255] = '\0';
	HTREEITEM hti, hti_focus = NULL;
	SendDlgItemMessage (hDlg, IDC_TREE1, TVM_DELETEITEM, 0, 0);
	for (DWORD i = 0; i < g_psys->nVessel(); i++) {
		Vessel *vessel = g_psys->GetVessel(i);
		if (vessel->GetEnableFocus()) {
			hti = AddVesselToTree (hDlg, NULL, vessel);
			if (hti) hti_focus = hti;
		}
	}
	SendDlgItemMessage (hDlg, IDC_TREE1, TVM_SELECTITEM, TVGN_CARET, (LPARAM)hti_focus);
}

// ======================================================================

void DlgFocus::RescanTree_Nearby (HWND hDlg)
{
	double range = pow(10.0,(double)(irange+1));
	char cbuf[256]; cbuf[255] = '\0';
	HTREEITEM hti, hti_focus = NULL;
	SendDlgItemMessage (hDlg, IDC_TREE1, TVM_DELETEITEM, 0, 0);
	const Vector &campos = g_camera->GPos();
	for (DWORD i = 0; i < g_psys->nVessel(); i++) {
		Vessel *vessel = g_psys->GetVessel(i);
		if (vessel->GetEnableFocus()) {
			double dst = campos.dist (vessel->GPos());
			if (dst <= range) {
				hti = AddVesselToTree (hDlg, NULL, vessel);
				if (hti) hti_focus = hti;
			}
		}
	}
	SendDlgItemMessage (hDlg, IDC_TREE1, TVM_SELECTITEM, TVGN_CARET, (LPARAM)hti_focus);
}

// ======================================================================

void DlgFocus::RescanTree_Location (HWND hDlg)
{
	HTREEITEM hti, hti_ref, hti_focus = NULL;
	TV_INSERTSTRUCT tvis;
	TVITEM tvi;
	char cbuf[256]; cbuf[255] = '\0';

	SendDlgItemMessage (hDlg, IDC_TREE1, TVM_DELETEITEM, 0, 0);

	tvis.hParent = NULL;
	tvis.hInsertAfter = TVI_LAST;
	tvis.item.mask = TVIF_TEXT | TVIF_CHILDREN;
	tvis.item.cChildren = 1;

	tvi.mask = TVIF_TEXT;
	tvi.pszText = cbuf;
	tvi.cchTextMax = 255;

	for (DWORD i = 0; i < g_psys->nVessel(); i++) {
		Vessel *vessel = g_psys->GetVessel(i);
		if (vessel->GetEnableFocus()) {
			const CelestialBody *ref = vessel->ElRef();
			// enter under reference body
			hti_ref = (HTREEITEM)SendDlgItemMessage (hDlg, IDC_TREE1, TVM_GETNEXTITEM, TVGN_ROOT, 0);
			while (hti_ref) {
				tvi.hItem = hti_ref;
				SendDlgItemMessage (hDlg, IDC_TREE1, TVM_GETITEM, 0, (LPARAM)&tvi);
				if (!strcmp (tvi.pszText, ref->Name()))
					break;
				hti_ref = (HTREEITEM)SendDlgItemMessage (hDlg, IDC_TREE1, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)hti_ref);
			}
			if (!hti_ref) {
				tvis.item.pszText = (char*)ref->Name();
				hti_ref = (HTREEITEM)SendDlgItemMessage (hDlg, IDC_TREE1, TVM_INSERTITEM, 0, (LPARAM)&tvis);
			}
			hti = AddVesselToTree (hDlg, hti_ref, vessel);
			if (hti) hti_focus = hti;
		}
	}
	SendDlgItemMessage (hDlg, IDC_TREE1, TVM_SELECTITEM, TVGN_CARET, (LPARAM)hti_focus);
}

// ======================================================================

void DlgFocus::RescanTree_Class (HWND hDlg)
{
	HTREEITEM hti, hti_ref, hti_focus = NULL;
	TV_INSERTSTRUCT tvis;
	TVITEM tvi;
	char cbuf[256]; cbuf[255] = '\0';

	SendDlgItemMessage (hDlg, IDC_TREE1, TVM_DELETEITEM, 0, 0);

	tvis.hParent = NULL;
	tvis.hInsertAfter = TVI_LAST;
	tvis.item.mask = TVIF_TEXT | TVIF_CHILDREN;
	tvis.item.cChildren = 1;

	tvi.mask = TVIF_TEXT;
	tvi.pszText = cbuf;
	tvi.cchTextMax = 255;

	for (DWORD i = 0; i < g_psys->nVessel(); i++) {
		Vessel *vessel = g_psys->GetVessel(i);
		if (vessel->GetEnableFocus()) {
			// enter under reference body
			hti_ref = (HTREEITEM)SendDlgItemMessage (hDlg, IDC_TREE1, TVM_GETNEXTITEM, TVGN_ROOT, 0);
			while (hti_ref) {
				tvi.hItem = hti_ref;
				SendDlgItemMessage (hDlg, IDC_TREE1, TVM_GETITEM, 0, (LPARAM)&tvi);
				if (!_stricmp (tvi.pszText, vessel->ClassName()))
					break;
				hti_ref = (HTREEITEM)SendDlgItemMessage (hDlg, IDC_TREE1, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)hti_ref);
			}
			if (!hti_ref) {
				strncpy (cbuf, vessel->ClassName(), 255);
				tvis.item.pszText = cbuf;
				hti_ref = (HTREEITEM)SendDlgItemMessage (hDlg, IDC_TREE1, TVM_INSERTITEM, 0, (LPARAM)&tvis);
			}
			hti = AddVesselToTree (hDlg, hti_ref, vessel);
			if (hti) hti_focus = hti;
		}
	}
	SendDlgItemMessage (hDlg, IDC_TREE1, TVM_SELECTITEM, TVGN_CARET, (LPARAM)hti_focus);
}

// ======================================================================

HTREEITEM DlgFocus::AddVesselToTree (HWND hDlg, HTREEITEM hti, Vessel *vessel, bool force)
{
	bool unroll = (unroll_assemblies || ctab == 3);
	if (!unroll && vessel->isAttached() && !force) return NULL;
	HTREEITEM hti_focus = NULL;
	DWORD i;

	SuperVessel *assembly = NULL;
	if (!unroll && (assembly = vessel->SuperStruct())) {
		char cbuf[256], assembly_name[256];
		Vessel *vessel0 = assembly->GetVessel(0);
		sprintf (assembly_name, "%s [assembly]", vessel0->Name());
		TVITEM tvi;
		tvi.mask = TVIF_TEXT;
		tvi.pszText = cbuf;
		tvi.cchTextMax = 255;
		HTREEITEM hti_assembly = (HTREEITEM)SendDlgItemMessage (hDlg, IDC_TREE1, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)hti);
		while (hti_assembly) {
			tvi.hItem = hti_assembly;
			SendDlgItemMessage (hDlg, IDC_TREE1, TVM_GETITEM, 0, (LPARAM)&tvi);
			if (!_stricmp (tvi.pszText, assembly_name))
				break;
			hti_assembly = (HTREEITEM)SendDlgItemMessage (hDlg, IDC_TREE1, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)hti_assembly);
		}
		if (!hti_assembly) {
			TV_INSERTSTRUCT tvis;
			tvis.hParent = hti;
			tvis.hInsertAfter = TVI_SORT;
			tvis.item.mask = TVIF_TEXT | TVIF_CHILDREN;
			tvis.item.cChildren = 1;
			tvis.item.pszText = assembly_name;
			hti_assembly = (HTREEITEM)SendDlgItemMessage (hDlg, IDC_TREE1, TVM_INSERTITEM, 0, (LPARAM)&tvis);
		}
		hti = hti_assembly;
	}
	TV_INSERTSTRUCT tvis;
	tvis.hParent = hti;
	tvis.hInsertAfter = TVI_SORT;
	tvis.item.mask = TVIF_TEXT | TVIF_CHILDREN;
	tvis.item.pszText = (char*)vessel->Name();
	tvis.item.cChildren = 0;
	if (!unroll) {
		for (i = 0; ; i++) {
			AttachmentSpec *as = vessel->GetAttachmentFromIndex (false, i);
			if (!as) break;
			if (as->mate) {tvis.item.cChildren = 1; break; }
		}
	}
	HTREEITEM hti_vessel = (HTREEITEM)SendDlgItemMessage (hDlg, IDC_TREE1, TVM_INSERTITEM, 0, (LPARAM)&tvis);
	if (tvis.item.cChildren) {
		for (i = 0; ; i++) {
			AttachmentSpec *as = vessel->GetAttachmentFromIndex (false, i);
			if (!as) break;
			if (as->mate) {
				HTREEITEM hti_child = AddVesselToTree (hDlg, hti_vessel, as->mate, true);
				if (hti_child) hti_focus = hti_child;
			}
		}
	}
	return (vessel == g_focusobj ? hti_vessel : hti_focus);
}

// ======================================================================

BOOL DlgFocus::SelectVessel (HWND hDlg)
{
	char cbuf[256];
	GetWindowText (GetDlgItem (hDlg, IDC_EDIT1), cbuf, 256);
	Vessel *vessel = g_psys->GetVessel (cbuf, true);
	if (vessel) {
		SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), vessel->Name());
		g_pOrbiter->SetFocusObject (vessel);
		HTREEITEM hti = FindItem (hDlg, vessel->Name(), 0);
		if (hti)
			PostMessage (GetDlgItem (hDlg, IDC_TREE1), TVM_SELECTITEM, TVGN_CARET, (LPARAM)hti);
	} else {
		SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), g_focusobj->Name());
		MessageBeep (MB_ICONEXCLAMATION);
	}
	WatchEdit (hDlg);
	return TRUE;
}

// ======================================================================

void DlgFocus::SelectPreviousVessel (HWND hDlg)
{
	if (g_pfocusobj) {
		SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), g_pfocusobj->Name());
		SelectVessel (hDlg);
	}
}

// ======================================================================

void DlgFocus::UpdateFocus (HWND hDlg)
{
	char cbuf[256];
	GetWindowText (GetDlgItem (hDlg, IDC_EDIT1), cbuf, 256);
	if (strcmp (cbuf, g_focusobj->Name())) {
		SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), g_focusobj->Name());
		HTREEITEM hti = FindItem (hDlg, g_focusobj->Name(), 0);
		if (hti)
			PostMessage (GetDlgItem (hDlg, IDC_TREE1), TVM_SELECTITEM, TVGN_CARET, (LPARAM)hti);
	}
}

// ======================================================================

HTREEITEM DlgFocus::FindItem (HWND hDlg, const char *str, HTREEITEM hti_first)
{
	HTREEITEM hti_match = NULL;
	TVITEM tvi;

	char cbuf[256]; cbuf[255] = '\0';
	tvi.mask = TVIF_TEXT | TVIF_CHILDREN;
	tvi.pszText = cbuf;
	tvi.cchTextMax = 255;

	if (!hti_first)
		hti_first = (HTREEITEM)SendDlgItemMessage (hDlg, IDC_TREE1, TVM_GETNEXTITEM, TVGN_ROOT, 0);
	
	do {
		tvi.hItem = hti_first;
		SendDlgItemMessage (hDlg, IDC_TREE1, TVM_GETITEM, 0, (LPARAM)&tvi);
		if (tvi.cChildren) {
			hti_match = FindItem (hDlg, str, (HTREEITEM)SendDlgItemMessage (hDlg, IDC_TREE1, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)hti_first));
			if (hti_match) return hti_match;
		} else {
			if (!strcmp (cbuf, str)) return hti_first;
		}
		hti_first = (HTREEITEM)SendDlgItemMessage (hDlg, IDC_TREE1, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)hti_first);
	} while (hti_first);
	return NULL;
}

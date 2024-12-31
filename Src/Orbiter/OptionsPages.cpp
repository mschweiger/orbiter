// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
// Template for simulation options pages
// ======================================================================

#include <windows.h>
#include <array>
#include "OptionsPages.h"
#include "DlgCtrl.h"
#include "Orbiter.h"
#include "Psys.h"
#include "Camera.h"
#include "resource.h"
#include <uxtheme.h>

using std::min;
using std::max;

extern Orbiter* g_pOrbiter;
extern PlanetarySystem* g_psys;
extern Camera* g_camera;

// ======================================================================

OptionsPageContainer::OptionsPageContainer(Originator orig, Config* cfg)
	: m_orig(orig)
	, m_cfg(cfg)
{
	m_hDlg = 0;
	m_pageIdx = 0;
	m_vScrollPage = 0;
	m_vScrollRange = 0;
	m_vScrollPos = 0;
	m_contextHelp = 0;
}

// ----------------------------------------------------------------------

OptionsPageContainer::~OptionsPageContainer()
{
	Clear();
}

// ----------------------------------------------------------------------

OptionsPage* OptionsPageContainer::CurrentPage()
{
	return (m_pageIdx < m_pPage.size() ? m_pPage[m_pageIdx] : nullptr);
}

// ----------------------------------------------------------------------

void OptionsPageContainer::SetWindowHandles(HWND hDlg, HWND hSplitter, HWND hPane1, HWND hPane2)
{
	m_hDlg = hDlg;
	m_hPageList = hPane1;
	m_hContainer = hPane2;
	m_splitter.SetHwnd(hSplitter, m_hPageList, m_hContainer);
	m_container.SetHwnd(m_hContainer);
	m_splitter.SetStaticPane(SplitterCtrl::PANE1, 120);
}

// ----------------------------------------------------------------------

void OptionsPageContainer::CreatePages()
{
	HTREEITEM parent;
	if (m_orig == LAUNCHPAD) {
		AddPage(new OptionsPage_Visual(this));
		AddPage(new OptionsPage_Physics(this));
	}
	AddPage(new OptionsPage_Instrument(this));
	AddPage(new OptionsPage_Vessel(this));
	parent = AddPage(new OptionsPage_UI(this));
	AddPage(new OptionsPage_Joystick(this), parent);
	AddPage(new OptionsPage_CelSphere(this));
	parent = AddPage(new OptionsPage_VisHelper(this));
	AddPage(new OptionsPage_Planetarium(this), parent);
	AddPage(new OptionsPage_Labels(this), parent);
	AddPage(new OptionsPage_Forces(this), parent);
	AddPage(new OptionsPage_Axes(this), parent);
	TreeView_SelectItem(m_hPageList, TreeView_GetRoot(m_hPageList));
}

// ----------------------------------------------------------------------

void OptionsPageContainer::ExpandAll()
{
	bool expand = true;
	HWND hTree = GetDlgItem(m_hDlg, IDC_OPT_PAGELIST);
	UINT code = (expand ? TVE_EXPAND : TVE_COLLAPSE);
	TVITEM catitem;
	catitem.mask = NULL;
	catitem.hItem = TreeView_GetRoot(hTree);
	while (TreeView_GetItem(hTree, &catitem)) {
		TreeView_Expand(hTree, catitem.hItem, code);
		catitem.hItem = TreeView_GetNextSibling(hTree, catitem.hItem);
	}
}

// ----------------------------------------------------------------------

void OptionsPageContainer::SetPageSize(HWND hDlg)
{
	if (m_pageIdx >= m_pPage.size()) return; // sanity check

	RECT r0, r1;
	GetClientRect(m_container.HWnd(), &r0);
	GetClientRect(m_pPage[m_pageIdx]->HPage(), &r1);
	bool bVscroll = r1.bottom > r0.bottom;
	ShowWindow(GetDlgItem(hDlg, IDC_SCROLLBAR1), bVscroll ? SW_SHOW : SW_HIDE);

	if (bVscroll) {
		m_vScrollRange = (r1.bottom - r0.bottom);
		SCROLLINFO scrollinfo;
		scrollinfo.cbSize = sizeof(SCROLLINFO);
		scrollinfo.nMin = 0;
		scrollinfo.nMax = r1.bottom;
		scrollinfo.nPage = m_vScrollPage = r0.bottom;
		scrollinfo.nPos = min(m_vScrollPos, m_vScrollRange);
		scrollinfo.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
		SetScrollInfo(GetDlgItem(hDlg, IDC_SCROLLBAR1), SB_CTL, &scrollinfo, TRUE);
		int dy = m_vScrollPos - scrollinfo.nPos;
		m_vScrollPos = scrollinfo.nPos;
		if (dy)
			ScrollWindow(m_pPage[m_pageIdx]->HPage(), 0, dy, NULL, NULL);
	}
	else if (m_vScrollPos) {
		ScrollWindow(m_pPage[m_pageIdx]->HPage(), 0, m_vScrollPos, NULL, NULL);
		m_vScrollPos = 0;
	}
}

// ----------------------------------------------------------------------

HTREEITEM OptionsPageContainer::AddPage(OptionsPage* pPage, HTREEITEM parent)
{
	m_pPage.push_back(pPage);
	return pPage->CreatePage(m_hDlg, parent);
}

// ----------------------------------------------------------------------

const OptionsPage* OptionsPageContainer::FindPage(const char* name) const
{
	for (auto page : m_pPage)
		if (!strcmp(name, page->Name()))
			return page;
	return 0;
}

// ----------------------------------------------------------------------

void OptionsPageContainer::SwitchPage(const char* name)
{
	char cbuf[256];
	TVITEM tvi;
	tvi.hItem = TreeView_GetRoot(m_hPageList);
	tvi.pszText = cbuf;
	tvi.cchTextMax = 256;
	tvi.cChildren = 0;
	tvi.mask = TVIF_HANDLE | TVIF_TEXT | TVIF_CHILDREN;
	while (tvi.hItem) {
		TreeView_GetItem(m_hPageList, &tvi);
		if (!stricmp(cbuf, name)) {
			TreeView_SelectItem(m_hPageList, tvi.hItem);
			break;
		}
		TVITEM tvi_child;
		tvi_child.hItem = TreeView_GetChild(m_hPageList, tvi.hItem);
		tvi_child.pszText = cbuf;
		tvi_child.cchTextMax = 256;
		tvi_child.cChildren = 0;
		tvi_child.mask = TVIF_HANDLE | TVIF_TEXT | TVIF_CHILDREN;
		while (tvi_child.hItem) {
			TreeView_GetItem(m_hPageList, &tvi_child);
			if (!stricmp(cbuf, name)) {
				TreeView_SelectItem(m_hPageList, tvi_child.hItem);
				break;
			}
			tvi_child.hItem = TreeView_GetNextSibling(m_hPageList, tvi_child.hItem);
		}
		tvi.hItem = TreeView_GetNextSibling(m_hPageList, tvi.hItem);
	}
}


// ----------------------------------------------------------------------

void OptionsPageContainer::SwitchPage(size_t page)
{
	if (page < 0 || page >= m_pPage.size())
		return;
	m_pageIdx = page;
	for (size_t pg = 0; pg < m_pPage.size(); pg++)
		if (pg != m_pageIdx) m_pPage[pg]->Show(false);
	m_pPage[m_pageIdx]->Show(true);
	m_pPage[m_pageIdx]->UpdateControls(m_pPage[m_pageIdx]->HPage());
	m_vScrollPos = 0;
	m_vScrollRange = 0;
	m_vScrollPage = 0;
	m_contextHelp = m_pPage[m_pageIdx]->HelpContext();

	SetPageSize(m_hDlg);
	InvalidateRect(m_hDlg, NULL, TRUE);
}

// ----------------------------------------------------------------------

void OptionsPageContainer::SwitchPage(const OptionsPage* page)
{
	for (size_t i = 0; i < m_pPage.size(); i++)
		if (m_pPage[i] == page) {
			SwitchPage(i);
			break;
		}
}

// ----------------------------------------------------------------------

void OptionsPageContainer::Clear()
{
	for (auto pPage : m_pPage)
		delete pPage;
	m_pPage.clear();
}

void OptionsPageContainer::OnNotifyPagelist(LPNMHDR pnmh)
{
	NM_TREEVIEW* pnmtv = (NM_TREEVIEW FAR*)pnmh;
	if (pnmtv->hdr.code == TVN_SELCHANGED) {
		OptionsPage* page = (OptionsPage*)pnmtv->itemNew.lParam;
		SwitchPage(page);
		TreeView_Expand(GetDlgItem(m_hDlg, IDC_OPT_PAGELIST), pnmtv->itemNew.hItem, TVE_EXPAND);
	}
}

// ----------------------------------------------------------------------

BOOL OptionsPageContainer::VScroll(HWND hDlg, WORD request, WORD curpos, HWND hControl)
{
	HWND hPage = CurrentPage()->HPage();

	SCROLLINFO scrollinfo;
	scrollinfo.cbSize = sizeof(SCROLLINFO);
	GetScrollInfo(hControl, SB_CTL, &scrollinfo);
	int pos = -1;

	switch (request) {
	case SB_BOTTOM:
		pos = m_vScrollRange;
		break;
	case SB_TOP:
		pos = 0;
		break;
	case SB_LINEDOWN:
		pos = min(m_vScrollPos + 10, m_vScrollRange);
		break;
	case SB_LINEUP:
		pos = max(m_vScrollPos - 10, 0);
		break;
	case SB_PAGEDOWN:
		pos = min(m_vScrollPos + m_vScrollPage, m_vScrollRange);
		break;
	case SB_PAGEUP:
		pos = max(m_vScrollPos - m_vScrollPage, 0);
		break;
	case SB_THUMBPOSITION:
	case SB_THUMBTRACK:
		pos = curpos;
		break;
	}
	if (pos >= 0 && pos != m_vScrollPos) {
		int dy = -(pos - m_vScrollPos);
		scrollinfo.nPos = m_vScrollPos = pos;
		scrollinfo.fMask = SIF_POS;
		SetScrollInfo(hControl, SB_CTL, &scrollinfo, TRUE);
		ScrollWindow(hPage, 0, dy, NULL, NULL);
		UpdateWindow(hPage);
	}
	return FALSE;
}

// ----------------------------------------------------------------------

void OptionsPageContainer::UpdatePages(bool resetView)
{
	for (auto pPage : m_pPage)
		pPage->UpdateControls(pPage->HPage());
	if (resetView)
		TreeView_SelectItem(m_hPageList, TreeView_GetRoot(m_hPageList));
}

// ----------------------------------------------------------------------

void OptionsPageContainer::UpdateConfig()
{
	for (auto pPage : m_pPage)
		pPage->UpdateConfig(pPage->HPage());
}

// ======================================================================

OptionsPage::OptionsPage(OptionsPageContainer* container)
	: m_container(container)
	, m_hPage(0)
	, m_hItem(0)
{
}

// ----------------------------------------------------------------------

OptionsPage::~OptionsPage()
{
	// Remove the object reference from the window.
	// This is so that object methods will no longer be called from the message loop
	// while the window hasn't been destroyed.
	if (m_hPage)
		SetWindowLongPtr(m_hPage, DWLP_USER, 0);
}

// ----------------------------------------------------------------------

void OptionsPage::Show(bool bShow)
{
	ShowWindow(m_hPage, bShow ? SW_SHOW : SW_HIDE);
}

// ----------------------------------------------------------------------

HWND OptionsPage::HParent() const
{
	return m_container->ContainerControl()->HWnd();
}

// ----------------------------------------------------------------------

HTREEITEM OptionsPage::CreatePage(HWND hDlg, HTREEITEM parent)
{
	int winId = ResourceId();
	m_hPage = CreateDialogParam(g_pOrbiter->GetInstance(), MAKEINTRESOURCE(winId), HParent(), s_DlgProc, (LPARAM)this);
	if (!m_hPage) {
		DWORD err = GetLastError();
		int i = 1;
	}

	char cbuf[256];
	strcpy(cbuf, Name());
	TV_INSERTSTRUCT tvis;
	tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
	tvis.item.pszText = cbuf;
	tvis.item.lParam = (LPARAM)this;
	tvis.hInsertAfter = TVI_LAST;
	tvis.hParent = parent;
	HTREEITEM hti = TreeView_InsertItem(GetDlgItem(hDlg, IDC_OPT_PAGELIST), &tvis);
	m_hItem = hti;
	return hti;
}

// ----------------------------------------------------------------------

BOOL OptionsPage::OnInitDialog(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	UpdateControls(hWnd);
	return TRUE;
}

// ----------------------------------------------------------------------

INT_PTR OptionsPage::DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG:
		return OnInitDialog(hWnd, wParam, lParam);
	case WM_COMMAND:
		return OnCommand(hWnd, LOWORD(wParam), HIWORD(wParam), (HWND)lParam);
	case WM_HSCROLL:
		return OnHScroll(hWnd, wParam, lParam);
	case WM_NOTIFY:
		return OnNotify(hWnd, (DWORD)wParam, (NMHDR*)lParam);
	default:
		return OnMessage(hWnd, uMsg, wParam, lParam);
	}
}

// ----------------------------------------------------------------------

INT_PTR CALLBACK OptionsPage::s_DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	OptionsPage* pPage;
	switch (uMsg) {
	case WM_INITDIALOG:
		EnableThemeDialogTexture(hWnd, ETDT_ENABLE);
		SetWindowLongPtr(hWnd, DWLP_USER, lParam);
		pPage = (OptionsPage*)lParam;
		break;
	default:
		pPage = (OptionsPage*)GetWindowLongPtr(hWnd, DWLP_USER);
		break;
	}
	return (pPage ? pPage->DlgProc(hWnd, uMsg, wParam, lParam) : DefWindowProc(hWnd, uMsg, wParam, lParam));
}

// ======================================================================

OptionsPage_Visual::OptionsPage_Visual(OptionsPageContainer* container)
	: OptionsPage(container)
{
}

// ----------------------------------------------------------------------

int OptionsPage_Visual::ResourceId() const
{
	return IDD_OPTIONS_VISUAL;
}

// ----------------------------------------------------------------------

const char* OptionsPage_Visual::Name() const
{
	const char* name = "Visual settings";
	return name;
}

// ----------------------------------------------------------------------

const HELPCONTEXT* OptionsPage_Visual::HelpContext() const
{
	static HELPCONTEXT hcontext = g_pOrbiter->DefaultHelpPage("/tab_visual.htm"); // this needs to be updated
	return &hcontext;
}

// ----------------------------------------------------------------------

void OptionsPage_Visual::UpdateControls(HWND hPage)
{
	char cbuf[256];

	SendDlgItemMessage(hPage, IDC_OPT_VIS_CLOUD, BM_SETCHECK,
		Cfg()->CfgVisualPrm.bClouds ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VIS_CSHADOW, BM_SETCHECK,
		Cfg()->CfgVisualPrm.bCloudShadows ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VIS_HAZE, BM_SETCHECK,
		Cfg()->CfgVisualPrm.bHaze ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VIS_FOG, BM_SETCHECK,
		Cfg()->CfgVisualPrm.bFog ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VIS_REFWATER, BM_SETCHECK,
		Cfg()->CfgVisualPrm.bWaterreflect ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VIS_RIPPLE, BM_SETCHECK,
		Cfg()->CfgVisualPrm.bSpecularRipple ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VIS_LIGHTS, BM_SETCHECK,
		Cfg()->CfgVisualPrm.bNightlights ? BST_CHECKED : BST_UNCHECKED, 0);
	sprintf(cbuf, "%0.2f", Cfg()->CfgVisualPrm.LightBrightness);
	SetWindowText(GetDlgItem(hPage, IDC_OPT_VIS_LTLEVEL), cbuf);
	SendDlgItemMessage(hPage, IDC_OPT_VIS_ELEV, BM_SETCHECK,
		Cfg()->CfgVisualPrm.ElevMode ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VIS_ELEVMODE, CB_SETCURSEL, Cfg()->CfgVisualPrm.ElevMode < 2 ? 0 : 1, 0);
	sprintf(cbuf, "%d", Cfg()->CfgVisualPrm.PlanetMaxLevel);
	SetWindowText(GetDlgItem(hPage, IDC_OPT_VIS_MAXLEVEL), cbuf);
	SendDlgItemMessage(hPage, IDC_OPT_VIS_VSHADOW, BM_SETCHECK,
		Cfg()->CfgVisualPrm.bVesselShadows ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VIS_REENTRY, BM_SETCHECK,
		Cfg()->CfgVisualPrm.bReentryFlames ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VIS_SHADOW, BM_SETCHECK,
		Cfg()->CfgVisualPrm.bShadows ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VIS_PARTICLE, BM_SETCHECK,
		Cfg()->CfgVisualPrm.bParticleStreams ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VIS_SPECULAR, BM_SETCHECK,
		Cfg()->CfgVisualPrm.bSpecular ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VIS_LOCALLIGHT, BM_SETCHECK,
		Cfg()->CfgVisualPrm.bLocalLight ? BST_CHECKED : BST_UNCHECKED, 0);
	sprintf(cbuf, "%d", Cfg()->CfgVisualPrm.AmbientLevel);
	SetWindowText(GetDlgItem(hPage, IDC_OPT_VIS_AMBIENT), cbuf);

	VisualsChanged(hPage);
}

// ----------------------------------------------------------------------

void OptionsPage_Visual::UpdateConfig(HWND hPage)
{
	char cbuf[256];
	DWORD i;
	double d;

	Cfg()->CfgVisualPrm.bClouds = (SendDlgItemMessage(hPage, IDC_OPT_VIS_CLOUD, BM_GETCHECK, 0, 0) == BST_CHECKED);
	Cfg()->CfgVisualPrm.bCloudShadows = (SendDlgItemMessage(hPage, IDC_OPT_VIS_CSHADOW, BM_GETCHECK, 0, 0) == BST_CHECKED);
	Cfg()->CfgVisualPrm.bHaze = (SendDlgItemMessage(hPage, IDC_OPT_VIS_HAZE, BM_GETCHECK, 0, 0) == BST_CHECKED);
	Cfg()->CfgVisualPrm.bFog = (SendDlgItemMessage(hPage, IDC_OPT_VIS_FOG, BM_GETCHECK, 0, 0) == BST_CHECKED);
	Cfg()->CfgVisualPrm.bWaterreflect = (SendDlgItemMessage(hPage, IDC_OPT_VIS_REFWATER, BM_GETCHECK, 0, 0) == BST_CHECKED);
	Cfg()->CfgVisualPrm.bSpecularRipple = (SendDlgItemMessage(hPage, IDC_OPT_VIS_RIPPLE, BM_GETCHECK, 0, 0) == BST_CHECKED);
	Cfg()->CfgVisualPrm.bNightlights = (SendDlgItemMessage(hPage, IDC_OPT_VIS_LIGHTS, BM_GETCHECK, 0, 0) == BST_CHECKED);
	GetWindowText(GetDlgItem(hPage, IDC_OPT_VIS_LTLEVEL), cbuf, 255);
	if (!sscanf(cbuf, "%lf", &d)) d = 0.5; else if (d < 0) d = 0.0; else if (d > 1) d = 1.0;
	Cfg()->CfgVisualPrm.LightBrightness = d;
	Cfg()->CfgVisualPrm.ElevMode = (SendDlgItemMessage(hPage, IDC_OPT_VIS_ELEV, BM_GETCHECK, 0, 0) != BST_CHECKED ?
		0 : SendDlgItemMessage(hPage, IDC_OPT_VIS_ELEVMODE, CB_GETCURSEL, 0, 0) + 1);
	GetWindowText(GetDlgItem(hPage, IDC_OPT_VIS_MAXLEVEL), cbuf, 127);
	if (!sscanf(cbuf, "%lu", &i)) i = SURF_MAX_PATCHLEVEL2;
	Cfg()->CfgVisualPrm.PlanetMaxLevel = max((DWORD)1, min((DWORD)SURF_MAX_PATCHLEVEL2, i));
	Cfg()->CfgVisualPrm.bVesselShadows = (SendDlgItemMessage(hPage, IDC_OPT_VIS_VSHADOW, BM_GETCHECK, 0, 0) == BST_CHECKED);
	Cfg()->CfgVisualPrm.bReentryFlames = (SendDlgItemMessage(hPage, IDC_OPT_VIS_REENTRY, BM_GETCHECK, 0, 0) == BST_CHECKED);
	Cfg()->CfgVisualPrm.bShadows = (SendDlgItemMessage(hPage, IDC_OPT_VIS_SHADOW, BM_GETCHECK, 0, 0) == BST_CHECKED);
	Cfg()->CfgVisualPrm.bParticleStreams = (SendDlgItemMessage(hPage, IDC_OPT_VIS_PARTICLE, BM_GETCHECK, 0, 0) == BST_CHECKED);
	Cfg()->CfgVisualPrm.bSpecular = (SendDlgItemMessage(hPage, IDC_OPT_VIS_SPECULAR, BM_GETCHECK, 0, 0) == BST_CHECKED);
	Cfg()->CfgVisualPrm.bLocalLight = (SendDlgItemMessage(hPage, IDC_OPT_VIS_LOCALLIGHT, BM_GETCHECK, 0, 0) == BST_CHECKED);
	GetWindowText(GetDlgItem(hPage, IDC_OPT_VIS_AMBIENT), cbuf, 255);
	if (!sscanf(cbuf, "%lu", &i)) i = 15; else if (i > 255) i = 255;
	Cfg()->SetAmbientLevel(i);
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Visual::OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam)
{
	OptionsPage::OnInitDialog(hPage, wParam, lParam);
	SendDlgItemMessage(hPage, IDC_OPT_VIS_ELEVMODE, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VIS_ELEVMODE, CB_ADDSTRING, 0, (LPARAM)"linear interpolation");
	SendDlgItemMessage(hPage, IDC_OPT_VIS_ELEVMODE, CB_ADDSTRING, 0, (LPARAM)"cubic interpolation");
	return TRUE;
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Visual::OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl)
{
	switch (ctrlId)
	{
		case IDC_OPT_VIS_CLOUD:
			if (notification == BN_CLICKED)
			{
				bool check = (SendDlgItemMessage( hPage, IDC_OPT_VIS_CLOUD, BM_GETCHECK, 0, 0 ) == BST_CHECKED);
				Cfg()->CfgVisualPrm.bClouds = check;
				VisualsChanged( hPage );
				return FALSE;
			}
			break;
		case IDC_OPT_VIS_CSHADOW:
			if (notification == BN_CLICKED)
			{
				bool check = (SendDlgItemMessage( hPage, IDC_OPT_VIS_CSHADOW, BM_GETCHECK, 0, 0 ) == BST_CHECKED);
				Cfg()->CfgVisualPrm.bCloudShadows = check;
				return FALSE;
			}
			break;
		case IDC_OPT_VIS_HAZE:
			if (notification == BN_CLICKED)
			{
				bool check = (SendDlgItemMessage( hPage, IDC_OPT_VIS_HAZE, BM_GETCHECK, 0, 0 ) == BST_CHECKED);
				Cfg()->CfgVisualPrm.bHaze = check;
				return FALSE;
			}
			break;
		case IDC_OPT_VIS_FOG:
			if (notification == BN_CLICKED)
			{
				bool check = (SendDlgItemMessage( hPage, IDC_OPT_VIS_FOG, BM_GETCHECK, 0, 0 ) == BST_CHECKED);
				Cfg()->CfgVisualPrm.bFog = check;
				return FALSE;
			}
			break;
		case IDC_OPT_VIS_REFWATER:
			if (notification == BN_CLICKED)
			{
				bool check = (SendDlgItemMessage( hPage, IDC_OPT_VIS_REFWATER, BM_GETCHECK, 0, 0 ) == BST_CHECKED);
				Cfg()->CfgVisualPrm.bWaterreflect = check;
				VisualsChanged( hPage );
				return FALSE;
			}
			break;
		case IDC_OPT_VIS_RIPPLE:
			if (notification == BN_CLICKED)
			{
				bool check = (SendDlgItemMessage( hPage, IDC_OPT_VIS_RIPPLE, BM_GETCHECK, 0, 0 ) == BST_CHECKED);
				Cfg()->CfgVisualPrm.bSpecularRipple = check;
				return FALSE;
			}
			break;
		case IDC_OPT_VIS_LIGHTS:
			if (notification == BN_CLICKED)
			{
				bool check = (SendDlgItemMessage( hPage, IDC_OPT_VIS_LIGHTS, BM_GETCHECK, 0, 0 ) == BST_CHECKED);
				Cfg()->CfgVisualPrm.bNightlights = check;
				VisualsChanged( hPage );
				return FALSE;
			}
			break;
		case IDC_OPT_VIS_LTLEVEL:
			if (notification == EN_CHANGE)
			{
				char cbuf[16];
				double d;
				GetWindowText( GetDlgItem( hPage, IDC_OPT_VIS_LTLEVEL ), cbuf, 16 );
				if (!sscanf( cbuf, "%lf", &d )) d = 0.5;
				else if (d < 0) d = 0.0;
				else if (d > 1) d = 1.0;
				Cfg()->CfgVisualPrm.LightBrightness = d;
				return FALSE;
			}
			break;
		case IDC_OPT_VIS_ELEV:
			if (notification == BN_CLICKED)
			{
				int elevmode = SendDlgItemMessage( hPage, IDC_OPT_VIS_ELEV, BM_GETCHECK, 0, 0 ) != BST_CHECKED ? 0 : (SendDlgItemMessage( hPage, IDC_OPT_VIS_ELEVMODE, CB_GETCURSEL, 0, 0 ) + 1);
				Cfg()->CfgVisualPrm.ElevMode = elevmode;
				VisualsChanged( hPage );
				return FALSE;
			}
			break;
		case IDC_OPT_VIS_ELEVMODE:
			if (notification == CBN_SELCHANGE)
			{
				int elevmode = SendDlgItemMessage( hPage, IDC_OPT_VIS_ELEV, BM_GETCHECK, 0, 0 ) != BST_CHECKED ? 0 : (SendDlgItemMessage( hPage, IDC_OPT_VIS_ELEVMODE, CB_GETCURSEL, 0, 0 ) + 1);
				Cfg()->CfgVisualPrm.ElevMode = elevmode;
				return FALSE;
			}
			break;
		case IDC_OPT_VIS_MAXLEVEL:
			if (notification == EN_CHANGE)
			{
				char cbuf[16];
				DWORD i;
				GetWindowText( GetDlgItem( hPage, IDC_OPT_VIS_MAXLEVEL ), cbuf, 16 );
				if (!sscanf( cbuf, "%lu", &i )) i = SURF_MAX_PATCHLEVEL2;
				Cfg()->CfgVisualPrm.PlanetMaxLevel = max((DWORD)1, min((DWORD)SURF_MAX_PATCHLEVEL2, i));
				return FALSE;
			}
			break;
		case IDC_OPT_VIS_VSHADOW:
			if (notification == BN_CLICKED)
			{
				bool check = (SendDlgItemMessage( hPage, IDC_OPT_VIS_VSHADOW, BM_GETCHECK, 0, 0 ) == BST_CHECKED);
				Cfg()->CfgVisualPrm.bVesselShadows = check;
				return FALSE;
			}
			break;
		case IDC_OPT_VIS_REENTRY:
			if (notification == BN_CLICKED)
			{
				bool check = (SendDlgItemMessage( hPage, IDC_OPT_VIS_REENTRY, BM_GETCHECK, 0, 0 ) == BST_CHECKED);
				Cfg()->CfgVisualPrm.bReentryFlames = check;
				return FALSE;
			}
			break;
		case IDC_OPT_VIS_SHADOW:
			if (notification == BN_CLICKED)
			{
				bool check = (SendDlgItemMessage( hPage, IDC_OPT_VIS_SHADOW, BM_GETCHECK, 0, 0 ) == BST_CHECKED);
				Cfg()->CfgVisualPrm.bShadows = check;
				return FALSE;
			}
			break;
		case IDC_OPT_VIS_PARTICLE:
			if (notification == BN_CLICKED)
			{
				bool check = (SendDlgItemMessage( hPage, IDC_OPT_VIS_PARTICLE, BM_GETCHECK, 0, 0 ) == BST_CHECKED);
				Cfg()->CfgVisualPrm.bParticleStreams = check;
				return FALSE;
			}
			break;
		case IDC_OPT_VIS_SPECULAR:
			if (notification == BN_CLICKED)
			{
				bool check = (SendDlgItemMessage( hPage, IDC_OPT_VIS_SPECULAR, BM_GETCHECK, 0, 0 ) == BST_CHECKED);
				Cfg()->CfgVisualPrm.bSpecular = check;
				return FALSE;
			}
			break;
		case IDC_OPT_VIS_LOCALLIGHT:
			if (notification == BN_CLICKED)
			{
				bool check = (SendDlgItemMessage( hPage, IDC_OPT_VIS_LOCALLIGHT, BM_GETCHECK, 0, 0 ) == BST_CHECKED);
				Cfg()->CfgVisualPrm.bLocalLight = check;
				return FALSE;
			}
			break;
		case IDC_OPT_VIS_AMBIENT:
			if (notification == EN_CHANGE)
			{
				char cbuf[16];
				DWORD i;
				GetWindowText( GetDlgItem(hPage, IDC_OPT_VIS_AMBIENT ), cbuf, 16 );
				if (!sscanf( cbuf, "%lu", &i )) i = 15;
				else if (i > 255) i = 255;
				Cfg()->SetAmbientLevel( i );
				return FALSE;
			}
			break;
	}
	return TRUE;
}

//-----------------------------------------------------------------------------

void OptionsPage_Visual::VisualsChanged(HWND hPage)
{
	EnableWindow(GetDlgItem(hPage, IDC_OPT_VIS_CSHADOW),
		SendDlgItemMessage(hPage, IDC_OPT_VIS_CLOUD, BM_GETCHECK, 0, 0) == BST_CHECKED);
	EnableWindow(GetDlgItem(hPage, IDC_OPT_VIS_RIPPLE),
		SendDlgItemMessage(hPage, IDC_OPT_VIS_REFWATER, BM_GETCHECK, 0, 0) == BST_CHECKED);
	EnableWindow( GetDlgItem( hPage, IDC_OPT_VIS_ELEVMODE ), SendDlgItemMessage( hPage, IDC_OPT_VIS_ELEV, BM_GETCHECK, 0, 0 ) == BST_CHECKED );
	EnableWindow( GetDlgItem( hPage, IDC_OPT_VIS_LTLEVEL ), SendDlgItemMessage( hPage, IDC_OPT_VIS_LIGHTS, BM_GETCHECK, 0, 0 ) == BST_CHECKED );
	return;
}

// ======================================================================

OptionsPage_Physics::OptionsPage_Physics(OptionsPageContainer* container)
	: OptionsPage(container)
{
}

// ----------------------------------------------------------------------

int OptionsPage_Physics::ResourceId() const
{
	return IDD_OPTIONS_PHYSICS;
}

// ----------------------------------------------------------------------

const char* OptionsPage_Physics::Name() const
{
	const char* name = "Physics settings";
	return name;
}

// ----------------------------------------------------------------------

const HELPCONTEXT* OptionsPage_Physics::HelpContext() const
{
	static HELPCONTEXT hcontext = g_pOrbiter->DefaultHelpPage("/tab_param.htm"); // this needs to be updated
	return &hcontext;
}

// ----------------------------------------------------------------------

void OptionsPage_Physics::UpdateControls(HWND hPage)
{
	SendDlgItemMessage(hPage, IDC_OPT_PHYS_COMPLEXGRAV, BM_SETCHECK,
		Cfg()->CfgPhysicsPrm.bNonsphericalGrav ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_PHYS_RPRESSURE, BM_SETCHECK,
		Cfg()->CfgPhysicsPrm.bRadiationPressure ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_PHYS_DISTMASS, BM_SETCHECK,
		Cfg()->CfgPhysicsPrm.bDistributedMass ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_PHYS_WIND, BM_SETCHECK,
		Cfg()->CfgPhysicsPrm.bAtmWind ? BST_CHECKED : BST_UNCHECKED, 0);
}

// ----------------------------------------------------------------------

void OptionsPage_Physics::UpdateConfig(HWND hPage)
{
	Cfg()->CfgPhysicsPrm.bDistributedMass = (SendDlgItemMessage(hPage, IDC_OPT_PHYS_DISTMASS, BM_GETCHECK, 0, 0) == BST_CHECKED);
	Cfg()->CfgPhysicsPrm.bNonsphericalGrav = (SendDlgItemMessage(hPage, IDC_OPT_PHYS_COMPLEXGRAV, BM_GETCHECK, 0, 0) == BST_CHECKED);
	Cfg()->CfgPhysicsPrm.bRadiationPressure = (SendDlgItemMessage(hPage, IDC_OPT_PHYS_RPRESSURE, BM_GETCHECK, 0, 0) == BST_CHECKED);
	Cfg()->CfgPhysicsPrm.bAtmWind = (SendDlgItemMessage(hPage, IDC_OPT_PHYS_WIND, BM_GETCHECK, 0, 0) == BST_CHECKED);
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Physics::OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam)
{
	OptionsPage::OnInitDialog(hPage, wParam, lParam);
	return TRUE;
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Physics::OnCommand( HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl )
{
	switch (ctrlId)
	{
		case IDC_OPT_PHYS_DISTMASS:
			if (notification == BN_CLICKED)
			{
				bool check = (SendDlgItemMessage( hPage, IDC_OPT_PHYS_DISTMASS, BM_GETCHECK, 0, 0 ) == BST_CHECKED);
				Cfg()->CfgPhysicsPrm.bDistributedMass = check;
				return FALSE;
			}
			break;
		case IDC_OPT_PHYS_COMPLEXGRAV:
			if (notification == BN_CLICKED)
			{
				bool check = (SendDlgItemMessage( hPage, IDC_OPT_PHYS_COMPLEXGRAV, BM_GETCHECK, 0, 0 ) == BST_CHECKED);
				Cfg()->CfgPhysicsPrm.bNonsphericalGrav = check;
				return FALSE;
			}
			break;
		case IDC_OPT_PHYS_RPRESSURE:
			if (notification == BN_CLICKED)
			{
				bool check = (SendDlgItemMessage( hPage, IDC_OPT_PHYS_RPRESSURE, BM_GETCHECK, 0, 0 ) == BST_CHECKED);
				Cfg()->CfgPhysicsPrm.bRadiationPressure = check;
				return FALSE;
			}
			break;
		case IDC_OPT_PHYS_WIND:
			if (notification == BN_CLICKED)
			{
				bool check = (SendDlgItemMessage( hPage, IDC_OPT_PHYS_WIND, BM_GETCHECK, 0, 0 ) == BST_CHECKED);
				Cfg()->CfgPhysicsPrm.bAtmWind = check;
				return FALSE;
			}
			break;
	}
	return TRUE;
}

// ======================================================================

OptionsPage_Instrument::OptionsPage_Instrument(OptionsPageContainer* container)
	: OptionsPage(container)
{
}

// ----------------------------------------------------------------------

int OptionsPage_Instrument::ResourceId() const
{
	return IDD_OPTIONS_INSTRUMENT;
}

// ----------------------------------------------------------------------

const char* OptionsPage_Instrument::Name() const
{
	const char* name = "Instruments & panels";
	return name;
}

// ----------------------------------------------------------------------

const HELPCONTEXT* OptionsPage_Instrument::HelpContext() const
{
	static HELPCONTEXT hcontext = g_pOrbiter->DefaultHelpPage("/tab_param.htm"); // this needs to be updated
	return &hcontext;
}

// ----------------------------------------------------------------------

void OptionsPage_Instrument::UpdateControls(HWND hPage)
{
	char cbuf[256];
	double mfdUpdDt = Cfg()->CfgLogicPrm.InstrUpdDT;
	sprintf(cbuf, "%0.2f", mfdUpdDt);
	SetWindowText(GetDlgItem(hPage, IDC_OPT_MFD_INTERVAL), cbuf);
	int mfdSize = Cfg()->CfgLogicPrm.MFDSize;
	sprintf(cbuf, "%d", mfdSize);
	SetWindowText(GetDlgItem(hPage, IDC_OPT_MFD_SIZE), cbuf);
	bool enable = Cfg()->CfgLogicPrm.bMfdTransparent;
	SendDlgItemMessage(hPage, IDC_OPT_MFD_TRANSP, BM_SETCHECK, enable ? BST_CHECKED : BST_UNCHECKED, 0);
	int vcmfdsize = Cfg()->CfgInstrumentPrm.VCMFDSize;
	int idx = (vcmfdsize == 1024 ? 2 : vcmfdsize == 512 ? 1 : 0);
	SendDlgItemMessage(hPage, IDC_OPT_MFD_VCTEXSIZE, CB_SETCURSEL, idx, 0);
	double scrollSpeed = Cfg()->CfgLogicPrm.PanelScrollSpeed;
	sprintf(cbuf, "%0.0f", scrollSpeed * 0.1);
	SetWindowText(GetDlgItem(hPage, IDC_OPT_PANEL_SCROLLSPEED), cbuf);
	double panelSize = Cfg()->CfgLogicPrm.PanelScale;
	sprintf(cbuf, "%0.2f", panelSize);
	SetWindowText(GetDlgItem(hPage, IDC_OPT_PANEL_SCALE), cbuf);
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Instrument::OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam)
{
	OptionsPage::OnInitDialog(hPage, wParam, lParam);
	SendDlgItemMessage(hPage, IDC_OPT_MFD_VCTEXSIZE, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hPage, IDC_OPT_MFD_VCTEXSIZE, CB_ADDSTRING, 0, (LPARAM)"256 x 256");
	SendDlgItemMessage(hPage, IDC_OPT_MFD_VCTEXSIZE, CB_ADDSTRING, 0, (LPARAM)"512 x 512");
	SendDlgItemMessage(hPage, IDC_OPT_MFD_VCTEXSIZE, CB_ADDSTRING, 0, (LPARAM)"1024 x 1024");
	return TRUE;
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Instrument::OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl)
{
	switch (ctrlId) {
	case IDC_OPT_MFD_INTERVAL:
		if (notification == EN_CHANGE) {
			char cbuf[256];
			double updDt;
			GetWindowText(GetDlgItem(hPage, IDC_OPT_MFD_INTERVAL), cbuf, 255);
			if (sscanf(cbuf, "%lf", &updDt)) {
				Cfg()->CfgLogicPrm.InstrUpdDT = max(0.01, updDt);
				g_pOrbiter->OnOptionChanged(OPTCAT_INSTRUMENT, OPTITEM_INSTRUMENT_MFDUPDATEINTERVAL);
			}
			return FALSE;
		}
		break;
	case IDC_OPT_MFD_SIZE:
		if (notification == EN_CHANGE) {
			char cbuf[256];
			int size;
			GetWindowText(GetDlgItem(hPage, IDC_OPT_MFD_SIZE), cbuf, 256);
			if (sscanf(cbuf, "%d", &size)) {
				Cfg()->CfgLogicPrm.MFDSize = max(1, min(10, size));
				g_pOrbiter->OnOptionChanged(OPTCAT_INSTRUMENT, OPTITEM_INSTRUMENT_MFDGENERICSIZE);
			}
			return FALSE;
		}
		break;
	case IDC_OPT_MFD_TRANSP:
		if (notification == BN_CLICKED) {
			bool check = (SendDlgItemMessage(hPage, IDC_OPT_MFD_TRANSP, BM_GETCHECK, 0, 0) == TRUE);
			Cfg()->CfgLogicPrm.bMfdTransparent = check;
			g_pOrbiter->OnOptionChanged(OPTCAT_INSTRUMENT, OPTITEM_INSTRUMENT_MFDGENERICTRANSP);
			return FALSE;
		}
		break;
	case IDC_OPT_MFD_VCTEXSIZE:
		if (notification == CBN_SELCHANGE) {
			int vcmfdsize[3] = { 256, 512, 1024 };
			DWORD idx = (DWORD)SendDlgItemMessage(hPage, IDC_OPT_MFD_VCTEXSIZE, CB_GETCURSEL, 0, 0);
			Cfg()->CfgInstrumentPrm.VCMFDSize = vcmfdsize[idx];
			g_pOrbiter->OnOptionChanged(OPTCAT_INSTRUMENT, OPTITEM_INSTRUMENT_MFDVCSIZE);
		}
		break;
	case IDC_OPT_PANEL_SCROLLSPEED:
		if (notification == EN_CHANGE) {
			char cbuf[256];
			double speed;
			GetWindowText(GetDlgItem(hPage, IDC_OPT_PANEL_SCROLLSPEED), cbuf, 256);
			if (sscanf(cbuf, "%lf", &speed)) {
				Cfg()->CfgLogicPrm.PanelScrollSpeed = 10.0 * max(-100.0, min(100.0, speed));
				g_pOrbiter->OnOptionChanged(OPTCAT_INSTRUMENT, OPTITEM_INSTRUMENT_PANELSCROLLSPEED);
			}
			return FALSE;
		}
		break;
	case IDC_OPT_PANEL_SCALE:
		if (notification == EN_CHANGE) {
			char cbuf[256];
			double scale;
			GetWindowText(GetDlgItem(hPage, IDC_OPT_PANEL_SCALE), cbuf, 256);
			if (sscanf(cbuf, "%lf", &scale)) {
				Cfg()->CfgLogicPrm.PanelScale = max(0.25, min(4.0, scale));
				g_pOrbiter->OnOptionChanged(OPTCAT_INSTRUMENT, OPTITEM_INSTRUMENT_PANELSCALE);
			}
			return FALSE;
		}
		break;
	}
	return TRUE;
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Instrument::OnNotify(HWND hPage, DWORD ctrlId, const NMHDR* pNmHdr)
{
	if (pNmHdr->code == UDN_DELTAPOS) {
		NMUPDOWN* nmud = (NMUPDOWN*)pNmHdr;
		int delta = -nmud->iDelta;
		switch (pNmHdr->idFrom) {
		case IDC_OPT_MFD_INTERVALSPIN:
			Cfg()->CfgLogicPrm.InstrUpdDT = max(0.01, Cfg()->CfgLogicPrm.InstrUpdDT + delta * 0.01);
			g_pOrbiter->OnOptionChanged(OPTCAT_INSTRUMENT, OPTITEM_INSTRUMENT_MFDUPDATEINTERVAL);
			break;
		case IDC_OPT_MFD_SIZESPIN:
			Cfg()->CfgLogicPrm.MFDSize = max(1, min(10, Cfg()->CfgLogicPrm.MFDSize + delta));
			g_pOrbiter->OnOptionChanged(OPTCAT_INSTRUMENT, OPTITEM_INSTRUMENT_MFDGENERICSIZE);
			break;
		case IDC_OPT_PANEL_SCROLLSPEEDSPIN:
			Cfg()->CfgLogicPrm.PanelScrollSpeed = 10.0 * max(-100.0, min(100.0, 0.1 * Cfg()->CfgLogicPrm.PanelScrollSpeed + delta));
			g_pOrbiter->OnOptionChanged(OPTCAT_INSTRUMENT, OPTITEM_INSTRUMENT_PANELSCROLLSPEED);
			break;
		case IDC_OPT_PANEL_SCALESPIN:
			Cfg()->CfgLogicPrm.PanelScale = max(0.25, min(4.0, Cfg()->CfgLogicPrm.PanelScale + delta * 0.01));
			g_pOrbiter->OnOptionChanged(OPTCAT_INSTRUMENT, OPTITEM_INSTRUMENT_PANELSCALE);
			break;
		}
		UpdateControls(hPage);
		return TRUE;
	}
	return FALSE;
}

// ======================================================================

OptionsPage_Vessel::OptionsPage_Vessel(OptionsPageContainer* container)
	: OptionsPage(container)
{
}

// ----------------------------------------------------------------------

int OptionsPage_Vessel::ResourceId() const
{
	return IDD_OPTIONS_VESSEL;
}

// ----------------------------------------------------------------------

const char* OptionsPage_Vessel::Name() const
{
	const char* name = "Vessel settings";
	return name;
}

// ----------------------------------------------------------------------

const HELPCONTEXT* OptionsPage_Vessel::HelpContext() const
{
	static HELPCONTEXT hcontext = g_pOrbiter->DefaultHelpPage("/tab_param.htm"); // this needs to be updated
	return &hcontext;
}

// ----------------------------------------------------------------------

void OptionsPage_Vessel::UpdateControls(HWND hPage)
{
	SendDlgItemMessage(hPage, IDC_OPT_VESSEL_FUELLIMIT, BM_SETCHECK,
		Cfg()->CfgLogicPrm.bLimitedFuel ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VESSEL_PADFUEL, BM_SETCHECK,
		Cfg()->CfgLogicPrm.bPadRefuel ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VESSEL_COMPLEXMODEL, BM_SETCHECK,
		Cfg()->CfgLogicPrm.FlightModelLevel ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VESSEL_DAMAGE, BM_SETCHECK,
		Cfg()->CfgLogicPrm.DamageSetting ? BST_CHECKED : BST_UNCHECKED, 0);
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Vessel::OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam)
{
	OptionsPage::OnInitDialog(hPage, wParam, lParam);
	if (Container()->Environment() == OptionsPageContainer::INLINE) {
		UpdateControls(hPage);
		EnableWindow(GetDlgItem(hPage, IDC_OPT_VESSEL_COMPLEXMODEL), FALSE);
		EnableWindow(GetDlgItem(hPage, IDC_OPT_VESSEL_DAMAGE), FALSE);
	}
	return TRUE;
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Vessel::OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl)
{
	switch (ctrlId) {
	case IDC_OPT_VESSEL_FUELLIMIT:
		if (notification == BN_CLICKED) {
			bool check = (SendDlgItemMessage(hPage, IDC_OPT_VESSEL_FUELLIMIT, BM_GETCHECK, 0, 0) == TRUE);
			Cfg()->CfgLogicPrm.bLimitedFuel = check;
			g_pOrbiter->OnOptionChanged(OPTCAT_VESSEL, OPTITEM_VESSEL_LIMITEDFUEL);
			return FALSE;
		}
		break;
	case IDC_OPT_VESSEL_PADFUEL:
		if (notification == BN_CLICKED) {
			bool check = (SendDlgItemMessage(hPage, IDC_OPT_VESSEL_PADFUEL, BM_GETCHECK, 0, 0) == TRUE);
			Cfg()->CfgLogicPrm.bPadRefuel = check;
			g_pOrbiter->OnOptionChanged(OPTCAT_VESSEL, OPTITEM_VESSEL_PADREFUEL);
			return FALSE;
		}
		break;
	case IDC_OPT_VESSEL_COMPLEXMODEL:
		if (notification == BN_CLICKED) {
			bool check = (SendDlgItemMessage(hPage, IDC_OPT_VESSEL_COMPLEXMODEL, BM_GETCHECK, 0, 0) == TRUE);
			Cfg()->CfgLogicPrm.FlightModelLevel = check;
			return FALSE;
		}
		break;
	case IDC_OPT_VESSEL_DAMAGE:
		if (notification == BN_CLICKED) {
			bool check = (SendDlgItemMessage(hPage, IDC_OPT_VESSEL_DAMAGE, BM_GETCHECK, 0, 0) == TRUE);
			Cfg()->CfgLogicPrm.DamageSetting = check;
			return FALSE;
		}
		break;
	}
	return TRUE;
}

// ======================================================================

OptionsPage_UI::OptionsPage_UI(OptionsPageContainer* container)
	: OptionsPage(container)
{
}

// ----------------------------------------------------------------------

int OptionsPage_UI::ResourceId() const
{
	return IDD_OPTIONS_UI;
}

// ----------------------------------------------------------------------

const char* OptionsPage_UI::Name() const
{
	const char* name = "User interface";
	return name;
}

// ----------------------------------------------------------------------

const HELPCONTEXT* OptionsPage_UI::HelpContext() const
{
	static HELPCONTEXT hcontext = g_pOrbiter->DefaultHelpPage("/tab_param.htm"); // this needs to be updated
	return &hcontext;
}

// ----------------------------------------------------------------------

void OptionsPage_UI::UpdateControls(HWND hPage)
{
	DWORD mode = Cfg()->CfgUIPrm.MouseFocusMode;
	SendDlgItemMessage(hPage, IDC_OPT_UI_MOUSEFOCUSMODE, CB_SETCURSEL, mode, 0);
}

// ----------------------------------------------------------------------

BOOL OptionsPage_UI::OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam)
{
	OptionsPage::OnInitDialog(hPage, wParam, lParam);

	SendDlgItemMessage(hPage, IDC_OPT_UI_MOUSEFOCUSMODE, CB_RESETCONTENT, 0, 0);
	const char* strMouseMode[3] = { "Focus requires click", "Hybrid: Click required only for child windows", "Focus follows mouse" };
	for (int i = 0; i < 3; i++)
		SendDlgItemMessage(hPage, IDC_OPT_UI_MOUSEFOCUSMODE, CB_ADDSTRING, 0, (LPARAM)strMouseMode[i]);

	return TRUE;
}

// ----------------------------------------------------------------------

BOOL OptionsPage_UI::OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl)
{
	switch (ctrlId) {
	case IDC_OPT_UI_MOUSEFOCUSMODE:
		if (notification == CBN_SELCHANGE) {
			DWORD mode = (DWORD)SendDlgItemMessage(hPage, IDC_OPT_UI_MOUSEFOCUSMODE, CB_GETCURSEL, 0, 0);
			Cfg()->CfgUIPrm.MouseFocusMode = mode;
			return FALSE;
		}
		break;
	}
	return TRUE;
}

// ======================================================================

OptionsPage_Joystick::OptionsPage_Joystick(OptionsPageContainer* container)
	: OptionsPage(container)
{
}

// ----------------------------------------------------------------------

int OptionsPage_Joystick::ResourceId() const
{
	return IDD_OPTIONS_JOYSTICK;
}

// ----------------------------------------------------------------------

const char* OptionsPage_Joystick::Name() const
{
	const char* name = "Joystick";
	return name;
}

// ----------------------------------------------------------------------

const HELPCONTEXT* OptionsPage_Joystick::HelpContext() const
{
	static HELPCONTEXT hcontext = g_pOrbiter->DefaultHelpPage("/tab_joystick.htm");
	return &hcontext;
}

// ----------------------------------------------------------------------

void OptionsPage_Joystick::UpdateControls(HWND hPage)
{
	char cbuf[256];

	SendDlgItemMessage(hPage, IDC_OPT_JOY_DEVICE, CB_SETCURSEL, (WPARAM)Cfg()->CfgJoystickPrm.Joy_idx, 0);
	SendDlgItemMessage(hPage, IDC_OPT_JOY_THROTTLE, CB_SETCURSEL, (WPARAM)Cfg()->CfgJoystickPrm.ThrottleAxis, 0);
	SendDlgItemMessage(hPage, IDC_OPT_JOY_INIT, BM_SETCHECK, Cfg()->CfgJoystickPrm.bThrottleIgnore ? BST_CHECKED : BST_UNCHECKED, 0);

	int sat = Cfg()->CfgJoystickPrm.ThrottleSaturation / 10;
	oapiSetGaugePos(GetDlgItem(hPage, IDC_OPT_JOY_SAT), sat);
	sprintf(cbuf, "%d", sat);
	SetWindowText(GetDlgItem(hPage, IDC_OPT_JOY_STATIC1), cbuf);

	int dz = Cfg()->CfgJoystickPrm.Deadzone / 10;
	oapiSetGaugePos(GetDlgItem(hPage, IDC_OPT_JOY_DEAD), dz);
	sprintf(cbuf, "%d", dz);
	SetWindowText(GetDlgItem(hPage, IDC_OPT_JOY_STATIC2), cbuf);

	int residJoystick[] = {
		IDC_OPT_JOY_THROTTLE, IDC_OPT_JOY_INIT, IDC_OPT_JOY_SAT, IDC_OPT_JOY_DEAD,
		IDC_OPT_JOY_STATIC1, IDC_OPT_JOY_STATIC2, IDC_OPT_JOY_STATIC3, IDC_OPT_JOY_STATIC4
	};
	bool enable = Cfg()->CfgJoystickPrm.Joy_idx > 0;
	for (int i = 0; i < ARRAYSIZE(residJoystick); i++) {
		EnableWindow(GetDlgItem(hPage, residJoystick[i]), enable);
	}
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Joystick::OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam)
{
	OptionsPage::OnInitDialog(hPage, wParam, lParam);

	DWORD ndev;
	DIDEVICEINSTANCE* joylist;
	g_pOrbiter->GetDInput()->GetJoysticks(&joylist, &ndev);

	SendDlgItemMessage(hPage, IDC_OPT_JOY_DEVICE, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hPage, IDC_OPT_JOY_DEVICE, CB_ADDSTRING, 0, (LPARAM)"<Disabled>");
	for (int i = 0; i < ndev; i++)
		SendDlgItemMessage(hPage, IDC_OPT_JOY_DEVICE, CB_ADDSTRING, 0, (LPARAM)(joylist[i].tszProductName));

	const char* thmode[4] = { "<Keyboard only>", "Z-axis", "Slider 0", "Slider 1" };
	SendDlgItemMessage(hPage, IDC_OPT_JOY_THROTTLE, CB_RESETCONTENT, 0, 0);
	for (int i = 0; i < ARRAYSIZE(thmode); i++)
		SendDlgItemMessage(hPage, IDC_OPT_JOY_THROTTLE, CB_ADDSTRING, 0, (LPARAM)thmode[i]);

	GAUGEPARAM gp = { 0, 1000, GAUGEPARAM::LEFT, GAUGEPARAM::BLACK };
	oapiSetGaugeParams(GetDlgItem(hPage, IDC_OPT_JOY_SAT), &gp);
	oapiSetGaugeParams(GetDlgItem(hPage, IDC_OPT_JOY_DEAD), &gp);

	return TRUE;
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Joystick::OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl)
{
	switch (ctrlId) {
	case IDC_OPT_JOY_DEVICE:
		if (notification == CBN_SELCHANGE) {
			DWORD idx = (DWORD)SendDlgItemMessage(hPage, IDC_OPT_JOY_DEVICE, CB_GETCURSEL, 0, 0);
			Cfg()->CfgJoystickPrm.Joy_idx = idx;
			g_pOrbiter->OnOptionChanged(OPTCAT_JOYSTICK, OPTITEM_JOYSTICK_DEVICE);
			UpdateControls(hPage);
			return FALSE;
		}
		break;
	case IDC_OPT_JOY_THROTTLE:
		if (notification == CBN_SELCHANGE) {
			DWORD axis = (DWORD)SendDlgItemMessage(hPage, IDC_OPT_JOY_THROTTLE, CB_GETCURSEL, 0, 0);
			Cfg()->CfgJoystickPrm.ThrottleAxis = axis;
			g_pOrbiter->OnOptionChanged(OPTCAT_JOYSTICK, OPTITEM_JOYSTICK_PARAM);
			return FALSE;
		}
		break;
	case IDC_OPT_JOY_INIT:
		if (notification == BN_CLICKED) {
			bool check = (SendDlgItemMessage(hPage, IDC_OPT_JOY_INIT, BM_GETCHECK, 0, 0) == BST_CHECKED);
			Cfg()->CfgJoystickPrm.bThrottleIgnore = check;
			break;
		}
		break;
	}
	return TRUE;
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Joystick::OnHScroll(HWND hPage, WPARAM wParam, LPARAM lParam)
{
	int val;
	switch (GetDlgCtrlID((HWND)lParam)) {
	case IDC_OPT_JOY_SAT:
		switch (LOWORD(wParam)) {
		case SB_THUMBTRACK:
		case SB_LINELEFT:
		case SB_LINERIGHT:
			val = HIWORD(wParam);
			Cfg()->CfgJoystickPrm.ThrottleSaturation = val * 10;
			UpdateControls(hPage);
			g_pOrbiter->OnOptionChanged(OPTCAT_JOYSTICK, OPTITEM_JOYSTICK_PARAM);
			return 0;
		}
		break;
	case IDC_OPT_JOY_DEAD:
		switch (LOWORD(wParam)) {
		case SB_THUMBTRACK:
		case SB_LINELEFT:
		case SB_LINERIGHT:
			val = HIWORD(wParam);
			Cfg()->CfgJoystickPrm.Deadzone = val * 10;
			UpdateControls(hPage);
			g_pOrbiter->OnOptionChanged(OPTCAT_JOYSTICK, OPTITEM_JOYSTICK_PARAM);
			return 0;
		}
		break;
	}
	return FALSE;
}

// ======================================================================

OptionsPage_CelSphere::OptionsPage_CelSphere(OptionsPageContainer* container)
	: OptionsPage(container)
{
}

// ----------------------------------------------------------------------

int OptionsPage_CelSphere::ResourceId() const
{
	return IDD_OPTIONS_CELSPHERE;
}

// ----------------------------------------------------------------------

const char* OptionsPage_CelSphere::Name() const
{
	const char* name = "Celestial sphere";
	return name;
}

// ----------------------------------------------------------------------

const HELPCONTEXT* OptionsPage_CelSphere::HelpContext() const
{
	static HELPCONTEXT hcontext = g_pOrbiter->DefaultHelpPage("/opt_celsphere.htm");
	return &hcontext;
}

// ----------------------------------------------------------------------

BOOL OptionsPage_CelSphere::OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam)
{
	OptionsPage::OnInitDialog(hPage, wParam, lParam);

	GAUGEPARAM gp = { 0, 100, GAUGEPARAM::LEFT, GAUGEPARAM::BLACK };
	oapiSetGaugeParams(GetDlgItem(hPage, IDC_OPT_CSP_BGBRIGHTNESS), &gp);

	PopulateStarmapList(hPage);
	PopulateBgImageList(hPage);
	UpdateControls(hPage);

	return TRUE;
}

// ----------------------------------------------------------------------

BOOL OptionsPage_CelSphere::OnCommand(HWND hPage, WORD id, WORD code, HWND hControl)
{
	switch (id) {
	case IDC_OPT_CSP_ENABLESTARPIX:
		if (code == BN_CLICKED) {
			StarPixelActivationChanged(hPage);
			return FALSE;
		}
		break;
	case IDC_OPT_CSP_ENABLESTARMAP:
		if (code == BN_CLICKED) {
			StarmapActivationChanged(hPage);
			return FALSE;
		}
		break;
	case IDC_OPT_CSP_ENABLEBKGMAP:
		if (code == BN_CLICKED) {
			BackgroundActivationChanged(hPage);
			return FALSE;
		}
		break;
	case IDC_OPT_CSP_STARMAPIMAGE:
		if (code == LBN_SELCHANGE) {
			StarmapImageChanged(hPage);
			return false;
		}
		break;
	case IDC_OPT_CSP_BKGIMAGE:
		if (code == LBN_SELCHANGE) {
			BackgroundImageChanged(hPage);
			return FALSE;
		}
		break;
	case IDC_OPT_CSP_STARMAPLIN:
	case IDC_OPT_CSP_STARMAPEXP:
		if (code == BN_CLICKED) {
			Cfg()->CfgVisualPrm.StarPrm.map_log = (id == IDC_OPT_CSP_STARMAPEXP);
			g_pOrbiter->OnOptionChanged(OPTCAT_CELSPHERE, OPTITEM_CELSPHERE_STARDISPLAYPARAM);
			return FALSE;
		}
		break;
	}
	return TRUE;
}

// ----------------------------------------------------------------------

BOOL OptionsPage_CelSphere::OnHScroll(HWND hPage, WPARAM wParam, LPARAM lParam)
{
	switch (GetDlgCtrlID((HWND)lParam)) {
	case IDC_OPT_CSP_BGBRIGHTNESS:
		switch (LOWORD(wParam)) {
		case SB_THUMBTRACK:
		case SB_LINELEFT:
		case SB_LINERIGHT:
			BackgroundBrightnessChanged(hPage, 0.01 * HIWORD(wParam));
			return 0;
		}
		break;
	}
	return FALSE;
}

// ----------------------------------------------------------------------

BOOL OptionsPage_CelSphere::OnNotify(HWND hPage, DWORD ctrlId, const NMHDR* pNmHdr)
{
	if (pNmHdr->code == UDN_DELTAPOS) {
		NMUPDOWN* nmud = (NMUPDOWN*)pNmHdr;
		int delta = -nmud->iDelta;
		StarRenderPrm& prm = Cfg()->CfgVisualPrm.StarPrm;
		switch (pNmHdr->idFrom) {
		case IDC_OPT_CSP_STARMAGHISPIN:
			prm.mag_hi = min(prm.mag_lo, max(-2.0, prm.mag_hi + delta * 0.1));
			break;
		case IDC_OPT_CSP_STARMAGLOSPIN:
			prm.mag_lo = min(15.0, max(prm.mag_hi, prm.mag_lo + delta * 0.1));
			break;
		case IDC_OPT_CSP_STARMINBRTSPIN:
			prm.brt_min = min(1.0, max(0.0, prm.brt_min + delta * 0.01));
			break;
		}
		UpdateControls(hPage);
		g_pOrbiter->OnOptionChanged(OPTCAT_CELSPHERE, OPTITEM_CELSPHERE_STARDISPLAYPARAM);
		return TRUE;
	}
	return FALSE;
}

// ----------------------------------------------------------------------

void OptionsPage_CelSphere::UpdateControls(HWND hPage)
{
	char cbuf[256];

	std::string starpath = std::string(Cfg()->CfgVisualPrm.StarImagePath);
	for (int idx = 0; idx < m_pathStarmap.size(); idx++)
		if (!starpath.compare(m_pathStarmap[idx].second)) {
			SendDlgItemMessage(hPage, IDC_OPT_CSP_STARMAPIMAGE, CB_SETCURSEL, idx, 0);
			break;
		}

	std::string bgpath = std::string(Cfg()->CfgVisualPrm.CSphereBgPath);
	for (int idx = 0; idx < m_pathBgImage.size(); idx++)
		if (!bgpath.compare(m_pathBgImage[idx].second)) {
			SendDlgItemMessage(hPage, IDC_OPT_CSP_BKGIMAGE, CB_SETCURSEL, idx, 0);
			break;
		}

	bool checked = Cfg()->CfgVisualPrm.bUseStarImage;
	SendDlgItemMessage(hPage, IDC_OPT_CSP_ENABLESTARMAP, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
	EnableWindow(GetDlgItem(hPage, IDC_OPT_CSP_STARMAPIMAGE), checked ? TRUE : FALSE);

	checked = Cfg()->CfgVisualPrm.bUseBgImage;
	SendDlgItemMessage(hPage, IDC_OPT_CSP_ENABLEBKGMAP, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
	int brt = (int)(Cfg()->CfgVisualPrm.CSphereBgIntens * 100.0);
	oapiSetGaugePos(GetDlgItem(hPage, IDC_OPT_CSP_BGBRIGHTNESS), brt);
	EnableWindow(GetDlgItem(hPage, IDC_OPT_CSP_BKGIMAGE), checked ? TRUE : FALSE);
	EnableWindow(GetDlgItem(hPage, IDC_STATIC1), checked ? TRUE : FALSE);
	EnableWindow(GetDlgItem(hPage, IDC_OPT_CSP_BGBRIGHTNESS), checked ? TRUE : FALSE);

	checked = Cfg()->CfgVisualPrm.bUseStarDots;
	SendDlgItemMessage(hPage, IDC_OPT_CSP_ENABLESTARPIX, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
	sprintf(cbuf, "%0.1f", Cfg()->CfgVisualPrm.StarPrm.mag_hi);
	SetWindowText(GetDlgItem(hPage, IDC_OPT_CSP_STARMAGHI), cbuf);
	sprintf(cbuf, "%0.1f", Cfg()->CfgVisualPrm.StarPrm.mag_lo);
	SetWindowText(GetDlgItem(hPage, IDC_OPT_CSP_STARMAGLO), cbuf);
	sprintf(cbuf, "%0.2f", Cfg()->CfgVisualPrm.StarPrm.brt_min);
	SetWindowText(GetDlgItem(hPage, IDC_OPT_CSP_STARMINBRT), cbuf);
	SendDlgItemMessage(hPage, IDC_OPT_CSP_STARMAPLIN, BM_SETCHECK,
		Cfg()->CfgVisualPrm.StarPrm.map_log ? BST_UNCHECKED : BST_CHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_CSP_STARMAPEXP, BM_SETCHECK,
		Cfg()->CfgVisualPrm.StarPrm.map_log ? BST_CHECKED : BST_UNCHECKED, 0);
	std::vector<int> ctrlStarPix{
		IDC_STATIC2, IDC_STATIC3, IDC_STATIC4, IDC_STATIC5, IDC_STATIC6,
		IDC_OPT_CSP_STARMAGHISPIN, IDC_OPT_CSP_STARMAGLOSPIN, IDC_OPT_CSP_STARMINBRTSPIN,
		IDC_OPT_CSP_STARMAGHI, IDC_OPT_CSP_STARMAGLO, IDC_OPT_CSP_STARMINBRT, IDC_OPT_CSP_STARMAPLIN, IDC_OPT_CSP_STARMAPEXP
	};
	for (auto ctrl : ctrlStarPix)
		EnableWindow(GetDlgItem(hPage, ctrl), checked ? TRUE : FALSE);
}

// ----------------------------------------------------------------------

void OptionsPage_CelSphere::PopulateStarmapList(HWND hPage)
{
	SendDlgItemMessage(hPage, IDC_OPT_CSP_STARMAPIMAGE, CB_RESETCONTENT, 0, 0);
	m_pathStarmap.clear();

	std::ifstream ifs(Cfg()->ConfigPath("CSphere\\bkgimage"));
	if (ifs) {
		char* c;
		char cbuf[256];
		bool found = false;
		while (ifs.getline(cbuf, 256)) {
			if (!found) {
				if (!strcmp(cbuf, "BEGIN_STARMAPS"))
					found = true;
				continue;
			}
			if (!strcmp(cbuf, "END_STARMAPS"))
				break;
			c = strtok(cbuf, "|");
			if (c) {
				SendDlgItemMessage(hPage, IDC_OPT_CSP_STARMAPIMAGE, CB_ADDSTRING, 0, (LPARAM)c);
				std::string label(c);
				c = strtok(NULL, "\n");
				std::string path(c);
				m_pathStarmap.push_back(std::make_pair(label, path));
			}
		}
	}
}

// ----------------------------------------------------------------------

void OptionsPage_CelSphere::PopulateBgImageList(HWND hPage)
{
	SendDlgItemMessage(hPage, IDC_OPT_CSP_BKGIMAGE, CB_RESETCONTENT, 0, 0);
	m_pathBgImage.clear();

	std::ifstream ifs(Cfg()->ConfigPath("CSphere\\bkgimage"));
	if (ifs) {
		char* c;
		char cbuf[256];
		bool found = false;
		while (ifs.getline(cbuf, 256)) {
			if (!found) {
				if (!strcmp(cbuf, "BEGIN_BACKGROUNDS"))
					found = true;
				continue;
			}
			if (!strcmp(cbuf, "END_BACKGROUNDS"))
				break;
			c = strtok(cbuf, "|");
			if (c) {
				SendDlgItemMessage(hPage, IDC_OPT_CSP_BKGIMAGE, CB_ADDSTRING, 0, (LPARAM)c);
				std::string label(c);
				c = strtok(NULL, "\n");
				std::string path(c);
				m_pathBgImage.push_back(std::make_pair(label, path));
			}
		}
	}
}

// ----------------------------------------------------------------------

void OptionsPage_CelSphere::StarPixelActivationChanged(HWND hPage)
{
	bool activated = SendDlgItemMessage(hPage, IDC_OPT_CSP_ENABLESTARPIX, BM_GETCHECK, 0, 0) == BST_CHECKED;
	bool active = Cfg()->CfgVisualPrm.bUseStarDots;

	if (activated != active) {
		Cfg()->CfgVisualPrm.bUseStarDots = activated;
		g_pOrbiter->OnOptionChanged(OPTCAT_CELSPHERE, OPTITEM_CELSPHERE_ACTIVATESTARDOTS);
	}
	UpdateControls(hPage);
}

// ----------------------------------------------------------------------

void OptionsPage_CelSphere::StarmapActivationChanged(HWND hPage)
{
	bool activated = SendDlgItemMessage(hPage, IDC_OPT_CSP_ENABLESTARMAP, BM_GETCHECK, 0, 0) == BST_CHECKED;
	bool active = Cfg()->CfgVisualPrm.bUseStarImage;

	if (activated != active) {
		Cfg()->CfgVisualPrm.bUseStarImage = activated;
		g_pOrbiter->OnOptionChanged(OPTCAT_CELSPHERE, OPTITEM_CELSPHERE_ACTIVATESTARIMAGE);
	}
	UpdateControls(hPage);
}

// ----------------------------------------------------------------------

void OptionsPage_CelSphere::StarmapImageChanged(HWND hPage)
{
	int idx = SendDlgItemMessage(hPage, IDC_OPT_CSP_STARMAPIMAGE, CB_GETCURSEL, 0, 0);
	std::string& path = m_pathStarmap[idx].second;
	strncpy(Cfg()->CfgVisualPrm.StarImagePath, path.c_str(), 128);
	g_pOrbiter->OnOptionChanged(OPTCAT_CELSPHERE, OPTITEM_CELSPHERE_STARIMAGECHANGED);
}

// ----------------------------------------------------------------------

void OptionsPage_CelSphere::BackgroundActivationChanged(HWND hPage)
{
	bool activated = SendDlgItemMessage(hPage, IDC_OPT_CSP_ENABLEBKGMAP, BM_GETCHECK, 0, 0) == BST_CHECKED;
	bool active = Cfg()->CfgVisualPrm.bUseBgImage;

	if (activated != active) {
		Cfg()->CfgVisualPrm.bUseBgImage = activated;
		g_pOrbiter->OnOptionChanged(OPTCAT_CELSPHERE, OPTITEM_CELSPHERE_ACTIVATEBGIMAGE);
	}
	UpdateControls(hPage);
}

// ----------------------------------------------------------------------

void OptionsPage_CelSphere::BackgroundImageChanged(HWND hPage)
{
	int idx = SendDlgItemMessage(hPage, IDC_OPT_CSP_BKGIMAGE, CB_GETCURSEL, 0, 0);
	std::string& path = m_pathBgImage[idx].second;
	strncpy(Cfg()->CfgVisualPrm.CSphereBgPath, path.c_str(), 128);
	g_pOrbiter->OnOptionChanged(OPTCAT_CELSPHERE, OPTITEM_CELSPHERE_BGIMAGECHANGED);
}

// ----------------------------------------------------------------------

void OptionsPage_CelSphere::BackgroundBrightnessChanged(HWND hPage, double level)
{
	if (level != Cfg()->CfgVisualPrm.CSphereBgIntens) {
		Cfg()->CfgVisualPrm.CSphereBgIntens = level;
		g_pOrbiter->OnOptionChanged(OPTCAT_CELSPHERE, OPTITEM_CELSPHERE_BGIMAGEBRIGHTNESS);
	}
}

// ======================================================================

OptionsPage_VisHelper::OptionsPage_VisHelper(OptionsPageContainer* container)
	: OptionsPage(container)
{
}

// ----------------------------------------------------------------------

int OptionsPage_VisHelper::ResourceId() const
{
	return IDD_OPTIONS_VISHELPER;
}

// ----------------------------------------------------------------------

const char* OptionsPage_VisHelper::Name() const
{
	const char* name = "Visual helpers";
	return name;
}

// ----------------------------------------------------------------------

const HELPCONTEXT* OptionsPage_VisHelper::HelpContext() const
{
	static HELPCONTEXT hcontext = g_pOrbiter->DefaultHelpPage("/vishelper.htm");
	return &hcontext;
}

// ----------------------------------------------------------------------

void OptionsPage_VisHelper::UpdateControls(HWND hPage)
{
	DWORD& plnFlag = Cfg()->CfgVisHelpPrm.flagPlanetarium;
	bool enable = plnFlag & PLN_ENABLE;
	SendDlgItemMessage(hPage, IDC_OPT_PLN, BM_SETCHECK, enable ? BST_CHECKED : BST_UNCHECKED, 0);
	DWORD& mkrFlag = Cfg()->CfgVisHelpPrm.flagMarkers;
	enable = mkrFlag & MKR_ENABLE;
	SendDlgItemMessage(hPage, IDC_OPT_MKR, BM_SETCHECK, enable ? BST_CHECKED : BST_UNCHECKED, 0);
	DWORD vecFlag = Cfg()->CfgVisHelpPrm.flagBodyForce;
	enable = (vecFlag & BFV_ENABLE);
	SendDlgItemMessage(hPage, IDC_OPT_VEC, BM_SETCHECK, enable ? BST_CHECKED : BST_UNCHECKED, 0);
	DWORD crdFlag = Cfg()->CfgVisHelpPrm.flagFrameAxes;
	enable = (crdFlag & FAV_ENABLE);
	SendDlgItemMessage(hPage, IDC_OPT_CRD, BM_SETCHECK, enable ? BST_CHECKED : BST_UNCHECKED, 0);
}

// ----------------------------------------------------------------------

BOOL OptionsPage_VisHelper::OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam)
{
	OptionsPage::OnInitDialog(hPage, wParam, lParam);
	return TRUE;
}

// ----------------------------------------------------------------------

BOOL OptionsPage_VisHelper::OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl)
{
	switch (ctrlId) {
	case IDC_OPT_PLN:
		if (notification == BN_CLICKED) {
			bool check = (SendDlgItemMessage(hPage, ctrlId, BM_GETCHECK, 0, 0) == BST_CHECKED);
			DWORD flag = PLN_ENABLE;
			DWORD& plnFlag = Cfg()->CfgVisHelpPrm.flagPlanetarium;
			if (check) plnFlag |= flag;
			else       plnFlag &= ~flag;
			return TRUE;
		}
		break;
	case IDC_OPT_MKR:
		if (notification == BN_CLICKED) {
			bool check = (SendDlgItemMessage(hPage, ctrlId, BM_GETCHECK, 0, 0) == BST_CHECKED);
			DWORD flag = MKR_ENABLE;
			DWORD& mkrFlag = Cfg()->CfgVisHelpPrm.flagMarkers;
			if (check) mkrFlag |= flag;
			else       mkrFlag &= ~flag;
			return TRUE;
		}
		break;
	case IDC_OPT_VEC:
		if (notification == BN_CLICKED) {
			bool check = (SendDlgItemMessage(hPage, ctrlId, BM_GETCHECK, 0, 0) == BST_CHECKED);
			DWORD flag = BFV_ENABLE;
			DWORD& vecFlag = Cfg()->CfgVisHelpPrm.flagBodyForce;
			if (check) vecFlag |= flag;
			else       vecFlag &= ~flag;
		}
		break;
	case IDC_OPT_CRD:
		if (notification == BN_CLICKED) {
			bool check = (SendDlgItemMessage(hPage, ctrlId, BM_GETCHECK, 0, 0) == BST_CHECKED);
			DWORD flag = FAV_ENABLE;
			DWORD& crdFlag = Cfg()->CfgVisHelpPrm.flagFrameAxes;
			if (check) crdFlag |= flag;
			else       crdFlag &= ~flag;
		}
		break;
	case IDC_OPT_VHELP_PLN:
		if (notification == BN_CLICKED)
			Container()->SwitchPage("Planetarium");
		break;
	case IDC_OPT_VHELP_MKR:
		if (notification == BN_CLICKED)
			Container()->SwitchPage("Labels");
		break;
	case IDC_OPT_VHELP_VEC:
		if (notification == BN_CLICKED)
			Container()->SwitchPage("Body forces");
		break;
	case IDC_OPT_VHELP_CRD:
		if (notification == BN_CLICKED)
			Container()->SwitchPage("Object axes");
		break;
	}
	return FALSE;
}

// ======================================================================

OptionsPage_Planetarium::OptionsPage_Planetarium(OptionsPageContainer* container)
	: OptionsPage(container)
{
}

// ----------------------------------------------------------------------

int OptionsPage_Planetarium::ResourceId() const
{
	return IDD_OPTIONS_PLANETARIUM;
}

// ----------------------------------------------------------------------

const char* OptionsPage_Planetarium::Name() const
{
	const char* name = "Planetarium";
	return name;
}

// ----------------------------------------------------------------------

const HELPCONTEXT* OptionsPage_Planetarium::HelpContext() const
{
	static HELPCONTEXT hcontext = g_pOrbiter->DefaultHelpPage("/vh_planetarium.htm");
	return &hcontext;
}

// ----------------------------------------------------------------------

void OptionsPage_Planetarium::UpdateControls(HWND hPage)
{
	std::array<int, 15> residPlanetarium = {
		IDC_OPT_PLN_CELGRID, IDC_OPT_PLN_ECLGRID, IDC_OPT_PLN_GALGRID, IDC_OPT_PLN_HRZGRID, IDC_OPT_PLN_EQU,
		IDC_OPT_PLN_CNSTLABEL, IDC_OPT_PLN_CNSTLABEL_FULL, IDC_OPT_PLN_CNSTLABEL_SHORT, IDC_OPT_PLN_CNSTBND,
		IDC_OPT_PLN_CNSTPATTERN, IDC_OPT_PLN_MARKER, IDC_OPT_PLN_MKRLIST,
		IDC_STATIC1, IDC_STATIC2, IDC_STATIC3
	};

	DWORD& plnFlag = Cfg()->CfgVisHelpPrm.flagPlanetarium;
	bool enable = plnFlag & PLN_ENABLE;
	SendDlgItemMessage(hPage, IDC_OPT_PLN, BM_SETCHECK, enable ? BST_CHECKED : BST_UNCHECKED, 0);
	for (auto resid : residPlanetarium)
		EnableWindow(GetDlgItem(hPage, resid), enable ? TRUE : FALSE);
	if (enable && !(plnFlag & PLN_CNSTLABEL)) {
		EnableWindow(GetDlgItem(hPage, IDC_OPT_PLN_CNSTLABEL_FULL), FALSE);
		EnableWindow(GetDlgItem(hPage, IDC_OPT_PLN_CNSTLABEL_SHORT), FALSE);
	}
	if (enable && !(plnFlag & PLN_CCMARK))
		EnableWindow(GetDlgItem(hPage, IDC_OPT_PLN_MKRLIST), FALSE);

	SendDlgItemMessage(hPage, IDC_OPT_PLN_CELGRID, BM_SETCHECK, plnFlag & PLN_CGRID ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_PLN_ECLGRID, BM_SETCHECK, plnFlag & PLN_EGRID ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_PLN_GALGRID, BM_SETCHECK, plnFlag & PLN_GGRID ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_PLN_HRZGRID, BM_SETCHECK, plnFlag & PLN_HGRID ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_PLN_EQU, BM_SETCHECK, plnFlag & PLN_EQU ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_PLN_CNSTLABEL, BM_SETCHECK, plnFlag & PLN_CNSTLABEL ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_PLN_CNSTBND, BM_SETCHECK, plnFlag & PLN_CNSTBND ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_PLN_CNSTPATTERN, BM_SETCHECK, plnFlag & PLN_CONST ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_PLN_MARKER, BM_SETCHECK, plnFlag & PLN_CCMARK ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_PLN_CNSTLABEL_FULL, BM_SETCHECK, plnFlag & PLN_CNSTLONG ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_PLN_CNSTLABEL_SHORT, BM_SETCHECK, plnFlag & PLN_CNSTLONG ? BST_UNCHECKED : BST_CHECKED, 0);
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Planetarium::OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam)
{
	OptionsPage::OnInitDialog(hPage, wParam, lParam);
	RescanMarkerList(hPage);

	return TRUE;
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Planetarium::OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl)
{
	switch (ctrlId) {
	case IDC_OPT_PLN:
	case IDC_OPT_PLN_CELGRID:
	case IDC_OPT_PLN_ECLGRID:
	case IDC_OPT_PLN_GALGRID:
	case IDC_OPT_PLN_HRZGRID:
	case IDC_OPT_PLN_EQU:
	case IDC_OPT_PLN_CNSTLABEL:
	case IDC_OPT_PLN_CNSTBND:
	case IDC_OPT_PLN_CNSTPATTERN:
	case IDC_OPT_PLN_MARKER:
	case IDC_OPT_PLN_CNSTLABEL_FULL:
	case IDC_OPT_PLN_CNSTLABEL_SHORT:
		if (notification == BN_CLICKED) {
			OnItemClicked(hPage, ctrlId);
			return TRUE;
		}
		break;
	case IDC_OPT_PLN_MKRLIST:
		if (notification == LBN_SELCHANGE)
			return OnMarkerSelectionChanged(hPage);
		break;
	}
	return FALSE;
}

// ----------------------------------------------------------------------

void OptionsPage_Planetarium::OnItemClicked(HWND hPage, WORD ctrlId)
{
	bool check = (SendDlgItemMessage(hPage, ctrlId, BM_GETCHECK, 0, 0) == TRUE);
	DWORD flag;
	switch (ctrlId) {
	case IDC_OPT_PLN:                 flag = PLN_ENABLE;    break;
	case IDC_OPT_PLN_CELGRID:         flag = PLN_CGRID;     break;
	case IDC_OPT_PLN_ECLGRID:         flag = PLN_EGRID;     break;
	case IDC_OPT_PLN_GALGRID:         flag = PLN_GGRID;     break;
	case IDC_OPT_PLN_HRZGRID:         flag = PLN_HGRID;     break;
	case IDC_OPT_PLN_EQU:             flag = PLN_EQU;       break;
	case IDC_OPT_PLN_CNSTLABEL:       flag = PLN_CNSTLABEL; break;
	case IDC_OPT_PLN_CNSTBND:         flag = PLN_CNSTBND;   break;
	case IDC_OPT_PLN_CNSTPATTERN:     flag = PLN_CONST;     break;
	case IDC_OPT_PLN_MARKER:          flag = PLN_CCMARK;    break;
	case IDC_OPT_PLN_CNSTLABEL_FULL:  flag = PLN_CNSTLONG;  break;
	case IDC_OPT_PLN_CNSTLABEL_SHORT: flag = PLN_CNSTLONG; check = !check; break;
	default:                          flag = 0;             break;
	}
	DWORD& plnFlag = Cfg()->CfgVisHelpPrm.flagPlanetarium;
	if (check) plnFlag |= flag;
	else       plnFlag &= ~flag;

	g_pOrbiter->OnOptionChanged(OPTCAT_PLANETARIUM, OPTITEM_PLANETARIUM_DISPFLAG);
	UpdateControls(hPage);
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Planetarium::OnMarkerSelectionChanged(HWND hPage)
{
	std::vector<oapi::GraphicsClient::LABELLIST>& list = g_psys->LabelList();
	if (list.size()) {
		for (int i = 0; i < list.size(); i++) {
			int sel = SendDlgItemMessage(hPage, IDC_OPT_PLN_MKRLIST, LB_GETSEL, i, 0);
			list[i].active = (sel ? true : false);
		}
		std::ifstream fcfg(Cfg()->ConfigPath(g_psys->Name().c_str()));
		g_psys->ScanLabelLists(fcfg);
	}
	return 0;
}

// ----------------------------------------------------------------------

void OptionsPage_Planetarium::RescanMarkerList(HWND hPage)
{
	SendDlgItemMessage(hPage, IDC_OPT_PLN_MKRLIST, LB_RESETCONTENT, 0, 0);

	if (!g_psys) return;
	const std::vector< oapi::GraphicsClient::LABELLIST>& list = g_psys->LabelList();
	if (!list.size()) return;

	int n = 0;
	g_psys->ForEach(FILETYPE_MARKER, [&](const fs::directory_entry& entry) {
		SendDlgItemMessage(hPage, IDC_OPT_PLN_MKRLIST, LB_ADDSTRING, 0, (LPARAM)entry.path().stem().string().c_str());
		if (n < list.size() && list[n].active)
			SendDlgItemMessage(hPage, IDC_OPT_PLN_MKRLIST, LB_SETSEL, TRUE, n);
		n++;
	});
}

// ======================================================================

OptionsPage_Labels::OptionsPage_Labels(OptionsPageContainer* container)
	: OptionsPage(container)
{
}

// ----------------------------------------------------------------------

int OptionsPage_Labels::ResourceId() const
{
	return IDD_OPTIONS_LABELS;
}

// ----------------------------------------------------------------------

const char* OptionsPage_Labels::Name() const
{
	const char* name = "Labels";
	return name;
}

// ----------------------------------------------------------------------

const HELPCONTEXT* OptionsPage_Labels::HelpContext() const
{
	static HELPCONTEXT hcontext = g_pOrbiter->DefaultHelpPage("/vh_labels.htm");
	return &hcontext;
}

// ----------------------------------------------------------------------

void OptionsPage_Labels::UpdateControls(HWND hPage)
{
	std::array<int, 10> residLabels = {
		IDC_OPT_MKR_VESSEL, IDC_OPT_MKR_CELBODY, IDC_OPT_MKR_FEATUREBODY, IDC_OPT_MKR_BASE,
		IDC_OPT_MKR_BEACON, IDC_OPT_MKR_FEATURES, IDC_OPT_MKR_FEATUREBODY, IDC_OPT_MKR_FEATURELIST,
		IDC_STATIC1, IDC_STATIC2
	};

	DWORD& mkrFlag = Cfg()->CfgVisHelpPrm.flagMarkers;
	bool enable = mkrFlag & MKR_ENABLE;
	SendDlgItemMessage(hPage, IDC_OPT_MKR, BM_SETCHECK, enable ? BST_CHECKED : BST_UNCHECKED, 0);
	for (auto resid : residLabels)
		EnableWindow(GetDlgItem(hPage, resid), enable ? TRUE : FALSE);
	if (enable && !(mkrFlag & MKR_LMARK)) {
		EnableWindow(GetDlgItem(hPage, IDC_OPT_MKR_FEATUREBODY), FALSE);
		EnableWindow(GetDlgItem(hPage, IDC_OPT_MKR_FEATURELIST), FALSE);
	}

	SendDlgItemMessage(hPage, IDC_OPT_MKR_VESSEL, BM_SETCHECK, mkrFlag & MKR_VMARK ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_MKR_CELBODY, BM_SETCHECK, mkrFlag & MKR_CMARK ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_MKR_BASE, BM_SETCHECK, mkrFlag & MKR_BMARK ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_MKR_BEACON, BM_SETCHECK, mkrFlag & MKR_RMARK ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_MKR_FEATURES, BM_SETCHECK, mkrFlag & MKR_LMARK ? BST_CHECKED : BST_UNCHECKED, 0);
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Labels::OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam)
{
	OptionsPage::OnInitDialog(hPage, wParam, lParam);
	ScanPsysBodies(hPage);

	return TRUE;
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Labels::OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl)
{
	switch (ctrlId) {
	case IDC_OPT_MKR:
	case IDC_OPT_MKR_VESSEL:
	case IDC_OPT_MKR_CELBODY:
	case IDC_OPT_MKR_BASE:
	case IDC_OPT_MKR_BEACON:
	case IDC_OPT_MKR_FEATURES:
		if (notification == BN_CLICKED) {
			OnItemClicked(hPage, ctrlId);
			return TRUE;
		}
		break;
	case IDC_OPT_MKR_FEATUREBODY:
		if (notification == CBN_SELCHANGE)
			UpdateFeatureList(hPage);
		return TRUE;
	case IDC_OPT_MKR_FEATURELIST:
		if (notification == LBN_SELCHANGE)
			RescanFeatures(hPage);
		return TRUE;
	}
	return FALSE;
}

// ----------------------------------------------------------------------

void OptionsPage_Labels::OnItemClicked(HWND hPage, WORD ctrlId)
{
	bool check = (SendDlgItemMessage(hPage, ctrlId, BM_GETCHECK, 0, 0) == TRUE);
	DWORD flag;
	switch (ctrlId) {
	case IDC_OPT_MKR:          flag = MKR_ENABLE; break;
	case IDC_OPT_MKR_VESSEL:   flag = MKR_VMARK;  break;
	case IDC_OPT_MKR_CELBODY:  flag = MKR_CMARK;  break;
	case IDC_OPT_MKR_BASE:     flag = MKR_BMARK;  break;
	case IDC_OPT_MKR_BEACON:   flag = MKR_RMARK;  break;
	case IDC_OPT_MKR_FEATURES: flag = MKR_LMARK;  break;
	default:                   flag = 0;          break;
	}
	DWORD& mkrFlag = Cfg()->CfgVisHelpPrm.flagMarkers;
	if (check) mkrFlag |= flag;
	else       mkrFlag &= ~flag;

	if (g_psys && ctrlId == IDC_OPT_MKR_FEATURES)
		g_psys->ActivatePlanetLabels(mkrFlag & MKR_ENABLE && mkrFlag & MKR_LMARK);

	UpdateControls(hPage);
}

// ----------------------------------------------------------------------

void OptionsPage_Labels::ScanPsysBodies(HWND hPage)
{
	if (!g_psys) return;
	const Body* sel = nullptr;
	for (int i = 0; i < g_psys->nPlanet(); i++) {
		Planet* planet = g_psys->GetPlanet(i);
		if (planet == g_camera->Target())
			sel = planet;
		if (planet->isMoon())
			continue;
		SendDlgItemMessage(hPage, IDC_OPT_MKR_FEATUREBODY, CB_ADDSTRING, 0, (LPARAM)planet->Name());
		for (int j = 0; j < planet->nSecondary(); j++) {
			char cbuf[256] = "    ";
			strncpy(cbuf + 4, planet->Secondary(j)->Name(), 252);
			SendDlgItemMessage(hPage, IDC_OPT_MKR_FEATUREBODY, CB_ADDSTRING, 0, (LPARAM)cbuf);
		}
	}
	if (!sel) {
		Body* tgt = g_camera->Target();
		if (tgt->Type() == OBJTP_VESSEL)
			sel = ((Vessel*)tgt)->GetSurfParam()->ref;
	}
	int idx = (sel ? SendDlgItemMessage(hPage, IDC_OPT_MKR_FEATUREBODY, CB_FINDSTRINGEXACT, -1, (LPARAM)sel->Name()) : 0);
	SendDlgItemMessage(hPage, IDC_OPT_MKR_FEATUREBODY, CB_SETCURSEL, idx, 0);
	UpdateFeatureList(hPage);
}

// ----------------------------------------------------------------------

void OptionsPage_Labels::UpdateFeatureList(HWND hPage)
{
	int n, nlist;
	char cbuf[256], cpath[256];
	int idx = SendDlgItemMessage(hPage, IDC_OPT_MKR_FEATUREBODY, CB_GETCURSEL, 0, 0);
	SendDlgItemMessage(hPage, IDC_OPT_MKR_FEATUREBODY, CB_GETLBTEXT, idx, (LPARAM)cbuf);
	SendDlgItemMessage(hPage, IDC_OPT_MKR_FEATURELIST, LB_RESETCONTENT, 0, 0);
	Planet* planet = g_psys->GetPlanet(trim_string(cbuf), true);
	if (!planet) return;

	if (planet->LabelFormat() < 2) {
		oapi::GraphicsClient::LABELLIST* list = planet->LabelList(&nlist);
		if (!nlist) return;

		n = 0;
		planet->ForEach(FILETYPE_MARKER, [&](const fs::directory_entry& entry) {
				SendDlgItemMessage(hPage, IDC_OPT_MKR_FEATURELIST, LB_ADDSTRING, 0, (LPARAM)entry.path().stem().string().c_str());
				if (n < nlist && list[n].active)
					SendDlgItemMessage(hPage, IDC_OPT_MKR_FEATURELIST, LB_SETSEL, TRUE, n);
				n++;
			});
	}
	else {
		int nlabel = planet->NumLabelLegend();
		if (nlabel) {
			const oapi::GraphicsClient::LABELTYPE* lspec = planet->LabelLegend();
			for (int i = 0; i < nlabel; i++) {
				SendDlgItemMessage(hPage, IDC_OPT_MKR_FEATURELIST, LB_ADDSTRING, 0, (LPARAM)lspec[i].name);
				if (lspec[i].active)
					SendDlgItemMessage(hPage, IDC_OPT_MKR_FEATURELIST, LB_SETSEL, TRUE, i);
			}
		}
	}
}

// ----------------------------------------------------------------------

void OptionsPage_Labels::RescanFeatures(HWND hPage)
{
	char cbuf[256];
	int nlist;

	int idx = SendDlgItemMessage(hPage, IDC_OPT_MKR_FEATUREBODY, CB_GETCURSEL, 0, 0);
	SendDlgItemMessage(hPage, IDC_OPT_MKR_FEATUREBODY, CB_GETLBTEXT, idx, (LPARAM)cbuf);
	Planet* planet = g_psys->GetPlanet(trim_string(cbuf), true);
	if (!planet) return;

	if (planet->LabelFormat() < 2) {
		oapi::GraphicsClient::LABELLIST* list = planet->LabelList(&nlist);
		if (!nlist) return;

		for (int i = 0; i < nlist; i++) {
			BOOL sel = SendDlgItemMessage(hPage, IDC_OPT_MKR_FEATURELIST, LB_GETSEL, i, 0);
			list[i].active = (sel ? true : false);
		}

		std::ifstream fcfg(Cfg()->ConfigPath(planet->Name()));
		planet->ScanLabelLists(fcfg);
	}
	else {
		nlist = planet->NumLabelLegend();
		for (int i = 0; i < nlist; i++) {
			BOOL sel = SendDlgItemMessage(hPage, IDC_OPT_MKR_FEATURELIST, LB_GETSEL, i, 0);
			planet->SetLabelActive(i, sel ? true : false);
		}
	}
}

// ======================================================================

OptionsPage_Forces::OptionsPage_Forces(OptionsPageContainer* container)
	: OptionsPage(container)
{
}

// ----------------------------------------------------------------------

int OptionsPage_Forces::ResourceId() const
{
	return IDD_OPTIONS_BODYFORCE;
}

// ----------------------------------------------------------------------

const char* OptionsPage_Forces::Name() const
{
	const char* name = "Body forces";
	return name;
}

// ----------------------------------------------------------------------

const HELPCONTEXT* OptionsPage_Forces::HelpContext() const
{
	static HELPCONTEXT hcontext = g_pOrbiter->DefaultHelpPage("/vh_force.htm");
	return &hcontext;
}

// ----------------------------------------------------------------------

void OptionsPage_Forces::UpdateControls(HWND hPage)
{
	std::array<int, 16> residForces = {
		IDC_OPT_VEC_WEIGHT, IDC_OPT_VEC_THRUST, IDC_OPT_VEC_LIFT, IDC_OPT_VEC_DRAG, IDC_OPT_VEC_SIDEFORCE, IDC_OPT_VEC_TOTAL,
		IDC_OPT_VEC_TORQUE, IDC_OPT_VEC_LINSCL, IDC_OPT_VEC_LOGSCL, IDC_OPT_VEC_SCALE, IDC_OPT_VEC_OPACITY,
		IDC_STATIC1, IDC_STATIC2, IDC_STATIC3, IDC_STATIC4, IDC_STATIC5
	};

	DWORD vecFlag = Cfg()->CfgVisHelpPrm.flagBodyForce;
	bool enable = (vecFlag & BFV_ENABLE);
	SendDlgItemMessage(hPage, IDC_OPT_VEC, BM_SETCHECK, enable ? BST_CHECKED : BST_UNCHECKED, 0);
	for (auto resid : residForces)
		EnableWindow(GetDlgItem(hPage, resid), enable ? TRUE : FALSE);

	SendDlgItemMessage(hPage, IDC_OPT_VEC_WEIGHT, BM_SETCHECK, vecFlag & BFV_WEIGHT ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VEC_THRUST, BM_SETCHECK, vecFlag & BFV_THRUST ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VEC_LIFT, BM_SETCHECK, vecFlag & BFV_LIFT ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VEC_DRAG, BM_SETCHECK, vecFlag & BFV_DRAG ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VEC_SIDEFORCE, BM_SETCHECK, vecFlag & BFV_SIDEFORCE ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VEC_TOTAL, BM_SETCHECK, vecFlag & BFV_TOTAL ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VEC_TORQUE, BM_SETCHECK, vecFlag & BFV_TORQUE ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VEC_LINSCL, BM_SETCHECK, vecFlag & BFV_LOGSCALE ? BST_UNCHECKED : BST_CHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_VEC_LOGSCL, BM_SETCHECK, vecFlag & BFV_LOGSCALE ? BST_CHECKED : BST_UNCHECKED, 0);

	int scalePos = (int)(25.0 * (1.0 + 0.5 * log(Cfg()->CfgVisHelpPrm.scaleBodyForce) / log(2.0)));
	oapiSetGaugePos(GetDlgItem(hPage, IDC_OPT_VEC_SCALE), scalePos);
	int opacPos = (int)(Cfg()->CfgVisHelpPrm.opacBodyForce * 50.0);
	oapiSetGaugePos(GetDlgItem(hPage, IDC_OPT_VEC_OPACITY), opacPos);
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Forces::OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam)
{
	GAUGEPARAM gp = { 0, 50, GAUGEPARAM::LEFT, GAUGEPARAM::BLACK };
	oapiSetGaugeParams(GetDlgItem(hPage, IDC_OPT_VEC_SCALE), &gp);
	oapiSetGaugeParams(GetDlgItem(hPage, IDC_OPT_VEC_OPACITY), &gp);

	UpdateControls(hPage);

	return TRUE;
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Forces::OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl)
{
	switch (ctrlId) {
	case IDC_OPT_VEC:
	case IDC_OPT_VEC_WEIGHT:
	case IDC_OPT_VEC_THRUST:
	case IDC_OPT_VEC_LIFT:
	case IDC_OPT_VEC_DRAG:
	case IDC_OPT_VEC_SIDEFORCE:
	case IDC_OPT_VEC_TOTAL:
	case IDC_OPT_VEC_TORQUE:
	case IDC_OPT_VEC_LINSCL:
	case IDC_OPT_VEC_LOGSCL:
		if (notification == BN_CLICKED) {
			OnItemClicked(hPage, ctrlId);
			return FALSE;
		}
		break;
	}
	return FALSE;
}

// ----------------------------------------------------------------------

void OptionsPage_Forces::OnItemClicked(HWND hPage, WORD ctrlId)
{
	bool check = (SendDlgItemMessage(hPage, ctrlId, BM_GETCHECK, 0, 0) == TRUE);
	DWORD flag;
	switch (ctrlId) {
	case IDC_OPT_VEC:           flag = BFV_ENABLE;    break;
	case IDC_OPT_VEC_WEIGHT:    flag = BFV_WEIGHT;    break;
	case IDC_OPT_VEC_THRUST:    flag = BFV_THRUST;    break;
	case IDC_OPT_VEC_LIFT:      flag = BFV_LIFT;      break;
	case IDC_OPT_VEC_DRAG:      flag = BFV_DRAG;      break;
	case IDC_OPT_VEC_SIDEFORCE: flag = BFV_SIDEFORCE; break;
	case IDC_OPT_VEC_TOTAL:     flag = BFV_TOTAL;     break;
	case IDC_OPT_VEC_TORQUE:    flag = BFV_TORQUE;    break;
	case IDC_OPT_VEC_LINSCL:    flag = BFV_LOGSCALE; check = false; break;
	case IDC_OPT_VEC_LOGSCL: flag = BFV_LOGSCALE; check = true;  break;
	default:                 flag = 0;           break;
	}
	DWORD& vecFlag = Cfg()->CfgVisHelpPrm.flagBodyForce;
	if (check) vecFlag |= flag;
	else       vecFlag &= ~flag;

	UpdateControls(hPage);
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Forces::OnHScroll(HWND hTab, WPARAM wParam, LPARAM lParam)
{
	switch (GetDlgCtrlID((HWND)lParam)) {
	case IDC_OPT_VEC_SCALE:
		switch (LOWORD(wParam)) {
		case SB_THUMBTRACK:
		case SB_LINELEFT:
		case SB_LINERIGHT:
			Cfg()->CfgVisHelpPrm.scaleBodyForce = (float)pow(2.0, (HIWORD(wParam) - 25) * 0.08);
			return 0;
		}
		break;
	case IDC_OPT_VEC_OPACITY:
		switch (LOWORD(wParam)) {
		case SB_THUMBTRACK:
		case SB_LINELEFT:
		case SB_LINERIGHT:
			Cfg()->CfgVisHelpPrm.opacBodyForce = (float)(HIWORD(wParam) * 0.02);
			return 0;
		}
		break;
	}
	return FALSE;
}

// ======================================================================

OptionsPage_Axes::OptionsPage_Axes(OptionsPageContainer* container)
	: OptionsPage(container)
{
}

// ----------------------------------------------------------------------

int OptionsPage_Axes::ResourceId() const
{
	return IDD_OPTIONS_FRAMEAXES;
}

// ----------------------------------------------------------------------

const char* OptionsPage_Axes::Name() const
{
	const char* name = "Object axes";
	return name;
}

// ----------------------------------------------------------------------

const HELPCONTEXT* OptionsPage_Axes::HelpContext() const
{
	static HELPCONTEXT hcontext = g_pOrbiter->DefaultHelpPage("/vh_coord.htm");
	return &hcontext;
}

// ----------------------------------------------------------------------

void OptionsPage_Axes::UpdateControls(HWND hPage)
{
	std::array<int, 10> residAxes = {
		IDC_OPT_CRD_VESSEL, IDC_OPT_CRD_CELBODY, IDC_OPT_CRD_BASE, IDC_OPT_CRD_NEGATIVE,
		IDC_OPT_CRD_SCALE, IDC_OPT_CRD_OPACITY,
		IDC_STATIC1, IDC_STATIC2, IDC_STATIC3, IDC_STATIC4
	};

	DWORD crdFlag = Cfg()->CfgVisHelpPrm.flagFrameAxes;
	bool enable = (crdFlag & FAV_ENABLE);
	SendDlgItemMessage(hPage, IDC_OPT_CRD, BM_SETCHECK, enable ? BST_CHECKED : BST_UNCHECKED, 0);
	for (auto resid : residAxes)
		EnableWindow(GetDlgItem(hPage, resid), enable ? TRUE : FALSE);

	SendDlgItemMessage(hPage, IDC_OPT_CRD_VESSEL, BM_SETCHECK, crdFlag & FAV_VESSEL ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_CRD_CELBODY, BM_SETCHECK, crdFlag & FAV_CELBODY ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_CRD_BASE, BM_SETCHECK, crdFlag & FAV_BASE ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hPage, IDC_OPT_CRD_NEGATIVE, BM_SETCHECK, crdFlag & FAV_NEGATIVE ? BST_CHECKED : BST_UNCHECKED, 0);

	int scalePos = (int)(25.0 * (1.0 + 0.5 * log(Cfg()->CfgVisHelpPrm.scaleFrameAxes) / log(2.0)));
	oapiSetGaugePos(GetDlgItem(hPage, IDC_OPT_CRD_SCALE), scalePos);
	int opacPos = (int)(Cfg()->CfgVisHelpPrm.opacFrameAxes * 50.0);
	oapiSetGaugePos(GetDlgItem(hPage, IDC_OPT_CRD_OPACITY), opacPos);
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Axes::OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam)
{
	GAUGEPARAM gp = { 0, 50, GAUGEPARAM::LEFT, GAUGEPARAM::BLACK };
	oapiSetGaugeParams(GetDlgItem(hPage, IDC_OPT_CRD_SCALE), &gp);
	oapiSetGaugeParams(GetDlgItem(hPage, IDC_OPT_CRD_OPACITY), &gp);

	UpdateControls(hPage);

	return TRUE;
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Axes::OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl)
{
	switch (ctrlId) {
	case IDC_OPT_CRD:
	case IDC_OPT_CRD_VESSEL:
	case IDC_OPT_CRD_CELBODY:
	case IDC_OPT_CRD_BASE:
	case IDC_OPT_CRD_NEGATIVE:
		if (notification == BN_CLICKED) {
			OnItemClicked(hPage, ctrlId);
			return FALSE;
		}
		break;
	}
	return FALSE;
}

// ----------------------------------------------------------------------

void OptionsPage_Axes::OnItemClicked(HWND hPage, WORD ctrlId)
{
	bool check = (SendDlgItemMessage(hPage, ctrlId, BM_GETCHECK, 0, 0) == TRUE);
	DWORD flag;
	switch (ctrlId) {
	case IDC_OPT_CRD:          flag = FAV_ENABLE;   break;
	case IDC_OPT_CRD_VESSEL:   flag = FAV_VESSEL;   break;
	case IDC_OPT_CRD_CELBODY:  flag = FAV_CELBODY;  break;
	case IDC_OPT_CRD_BASE:     flag = FAV_BASE;     break;
	case IDC_OPT_CRD_NEGATIVE: flag = FAV_NEGATIVE; break;
	default:                   flag = 0;            break;
	}
	DWORD& crdFlag = Cfg()->CfgVisHelpPrm.flagFrameAxes;
	if (check) crdFlag |= flag;
	else       crdFlag &= ~flag;

	UpdateControls(hPage);
}

// ----------------------------------------------------------------------

BOOL OptionsPage_Axes::OnHScroll(HWND hTab, WPARAM wParam, LPARAM lParam)
{
	switch (GetDlgCtrlID((HWND)lParam)) {
	case IDC_OPT_CRD_SCALE:
		switch (LOWORD(wParam)) {
		case SB_THUMBTRACK:
		case SB_LINELEFT:
		case SB_LINERIGHT:
			Cfg()->CfgVisHelpPrm.scaleFrameAxes = (float)pow(2.0, (HIWORD(wParam) - 25) * 0.08);
			return 0;
		}
		break;
	case IDC_OPT_CRD_OPACITY:
		switch (LOWORD(wParam)) {
		case SB_THUMBTRACK:
		case SB_LINELEFT:
		case SB_LINERIGHT:
			Cfg()->CfgVisHelpPrm.opacFrameAxes = (float)(HIWORD(wParam) * 0.02);
			return 0;
		}
		break;
	}
	return FALSE;
}

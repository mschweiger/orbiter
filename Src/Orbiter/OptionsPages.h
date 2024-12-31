// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
// Template for simulation options pages
// ======================================================================

/************************************************************************
 * \file OptionsPages.h
 * \brief Template for simulation options pages
 */

#ifndef __OPTIONSPAGES_H
#define __OPTIONSPAGES_H

#include <windows.h>
#include <commctrl.h>
#include "CustomControls.h"
#include "OrbiterAPI.h"

class OptionsPage;
class Config;

/************************************************************************
 * \brief Container class for options pages
 */
class OptionsPageContainer {
public:
	/**
	 * \brief Enumerates where the options pages are shown.
	 */
	enum Originator {
		LAUNCHPAD, ///< Show in Launchpad dialog
		INLINE     ///< Show as inline dialog during a simulation session
	};

	OptionsPageContainer(Originator orig, Config* cfg);
	~OptionsPageContainer();

	OptionsPage* CurrentPage();

	Originator Environment() const { return m_orig; }

	Config* Cfg() { return m_cfg; }

	void SetWindowHandles(HWND hDlg, HWND hSplitter, HWND hPane1, HWND hPane2);

	const GenericCtrl* ContainerControl() const { return &m_container; }

	/**
	 * \brief Update dialog controls from config settings
	 */
	void UpdatePages(bool resetView);

	/**
	 * \brief Update config object from dialog controls
	 */
	void UpdateConfig();

	void SwitchPage(const char* name);

	void OnNotifyPagelist(LPNMHDR pnmh);

protected:
	/**
     * \brief Adds a new options page.
     * \param hDlg dialog handle
     * \param pPage pointer to new page
     */
	HTREEITEM AddPage(OptionsPage* pPage, HTREEITEM parent = 0);

	const OptionsPage* FindPage(const char* name) const;

	void SwitchPage(size_t page);
	void SwitchPage(const OptionsPage* page);

	void CreatePages();

	void ExpandAll();

	void SetPageSize(HWND hDlg);

	void Clear();

	BOOL VScroll(HWND hDlg, WORD request, WORD curpos, HWND hControl);

	const HELPCONTEXT* HelpContext() const { return m_contextHelp; }

private:
	Originator m_orig;
	Config* m_cfg;
	SplitterCtrl m_splitter;
	GenericCtrl m_container;
	std::vector<OptionsPage*> m_pPage;
	size_t m_pageIdx;
	HWND m_hDlg;
	HWND m_hPageList;
	HWND m_hContainer;
	int m_vScrollPos;
	int m_vScrollRange;
	int m_vScrollPage;
	const HELPCONTEXT* m_contextHelp;
};

/************************************************************************
 * \brief Base class for options dialog pages.
 */
class OptionsPage {
public:
	/**
	 * \brief OptionsPage constructor.
	 * \param container Container owning the page
	 */
	OptionsPage(OptionsPageContainer* container);

	/**
	 * \brief OptionsPage destructor.
	 */
	virtual ~OptionsPage();

	/**
	 * \brief Derived classes return the dialog resource id.
	 */
	virtual int ResourceId() const = 0;

	/**
	 * \brief Derived classes return the page title as it appears in the tree list.
	 */
	virtual const char* Name() const = 0;

	/**
	 * \brief Returns the container object owning the page.
	 */
	OptionsPageContainer* Container() { return m_container; }

	Config* Cfg() { return m_container->Cfg(); }

	/**
	 * \brief Returns the parent dialog handle.
	 * \return Parent dialog handle
	 */
	HWND HParent() const;

	/**
	 * \brief Returns the page window handle.
	 * \return Page window handle
	 */
	HWND HPage() const { return m_hPage; }

	/**
	 * \brief Creates the page window and assigns \ref m_hPage.
	 */
	HTREEITEM CreatePage(HWND hDlg, HTREEITEM parent = 0);

	/**
	 * \brief Show/hide the page.
	 * \param bShow Show page if true, hide if false
	 */
	void Show(bool bShow);

	/**
	 * \brief Update the dialog controls from config settings.
	 * \param hPage dialog page handle
	 */
	virtual void UpdateControls(HWND hPage) {}

	/**
	 * \brief Update config object from dialog control states.
	 *    Only required for pages which don't react directly to controls
	 *    being modified.
	 */
	virtual void UpdateConfig(HWND hPage) {}

	/**
	 * \brief Returns the name of the option page's help page, if applicable.
	 */
	virtual const HELPCONTEXT* HelpContext() const { return 0; }

protected:
	/**
	 * \brief Default handler for WM_INITDIALOG messages.
	 * \default Nothing, returns TRUE
	 */
	virtual BOOL OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam);

	/**
	 * \brief Default handler for WM_COMMAND messages.
	 * \param hPage dialog window handle
	 * \param ctrlId resource identifier of the control (LOWORD(wParam))
	 * \param notification code (HIWORD(wParam))
	 * \param hCtrl control window handle (lParam)
	 * \default Nothing, returns FALSE
	 */
	virtual BOOL OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl) { return FALSE; }

	/**
	 * \brief Default handler for WM_HSCROLL messages.
	 * \default Nothing, returns FALSE
	 * \note This message is called by gauge controls on slider position change.
	 */
	virtual BOOL OnHScroll(HWND hPage, WPARAM wParam, LPARAM lParam) { return FALSE; }

	/**
	 * \brief Default handler for WM_NOTIFY messages.
	 * \param hPage dialog window handle
	 * \param ctrlId resource identifier of the control (wParam)
	 * \param pNmHdr pointer to a NMHDR structure with details of the notification
	 * \default Nothing, returns FALSE
	 */
	virtual BOOL OnNotify(HWND hPage, DWORD ctrlId, const NMHDR* pNmHdr) { return FALSE; }

	/**
	 * \brief Default generic message handler.
	 * \default Nothing, returns FALSE
	 * \note This method is called for any messages which don't have an associated
	 *    specific callback function.
	 */
	virtual BOOL OnMessage(HWND hPage, UINT uMsg, WPARAM wParam, LPARAM lParam) { return FALSE; }

	/**
	 * \Brief page message loop.
	 */
	virtual INT_PTR DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	/**
	 * \brief Message loop hook for all options page.
	 * \note This function dereferences the page instance and then calls
	 *    the specific page's DlgProc method.
	 */
	static INT_PTR CALLBACK s_DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	OptionsPageContainer* m_container; ///< container owning the page
	HWND m_hPage;      ///< page window handle (0 before MakePage has been called)
	HTREEITEM m_hItem; ///< page title in the tree view control
};

/************************************************************************
 * \brief Page for visual parameters
 */
class OptionsPage_Visual : public OptionsPage {
public:
	OptionsPage_Visual(OptionsPageContainer* container);
	int ResourceId() const;
	const char* Name() const;
	const HELPCONTEXT* HelpContext() const;
	void UpdateControls(HWND hPage);
	void UpdateConfig(HWND hPage);

protected:
	BOOL OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam);
	BOOL OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl);
	void VisualsChanged(HWND hPage);

};

/************************************************************************
* \brief Page for physics engine options
*/
class OptionsPage_Physics : public OptionsPage {
public:
	OptionsPage_Physics(OptionsPageContainer* container);
	int ResourceId() const;
	const char* Name() const;
	const HELPCONTEXT* HelpContext() const;
	void UpdateControls(HWND hPage);
	void UpdateConfig(HWND hPage);

protected:
	BOOL OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam);
	BOOL OnCommand( HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl );
};

/************************************************************************
 * \brief Page for instrument and panel options
 */
class OptionsPage_Instrument : public OptionsPage {
public:
	OptionsPage_Instrument(OptionsPageContainer* container);
	int ResourceId() const;
	const char* Name() const;
	const HELPCONTEXT* HelpContext() const;
	void UpdateControls(HWND hPage);

protected:
	BOOL OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam);
	BOOL OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl);
	BOOL OnNotify(HWND hPage, DWORD ctrlId, const NMHDR* pNmHdr);
};

/************************************************************************
 * \brief Page for vessel options
 */
class OptionsPage_Vessel : public OptionsPage {
public:
	OptionsPage_Vessel(OptionsPageContainer* container);
	int ResourceId() const;
	const char* Name() const;
	const HELPCONTEXT* HelpContext() const;
	void UpdateControls(HWND hPage);

protected:
	BOOL OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam);
	BOOL OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl);
};

/************************************************************************
* \brief Page for user interface options
*/
class OptionsPage_UI : public OptionsPage {
public:
	OptionsPage_UI(OptionsPageContainer* container);
	int ResourceId() const;
	const char* Name() const;
	const HELPCONTEXT* HelpContext() const;
	void UpdateControls(HWND hPage);

protected:
	BOOL OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam);
	BOOL OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl);
};

/************************************************************************
* \brief Page for joystick options
*/
class OptionsPage_Joystick : public OptionsPage {
public:
	OptionsPage_Joystick(OptionsPageContainer* container);
	int ResourceId() const;
	const char* Name() const;
	const HELPCONTEXT* HelpContext() const;
	void UpdateControls(HWND hPage);

protected:
	BOOL OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam);
	BOOL OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl);
	BOOL OnHScroll(HWND hPage, WPARAM wParam, LPARAM lParam);
};

/************************************************************************
* \brief Page for celestial sphere rendering options.
*/
class OptionsPage_CelSphere : public OptionsPage {
public:
	OptionsPage_CelSphere(OptionsPageContainer* container);
	int ResourceId() const;
	const char* Name() const;
	const HELPCONTEXT* HelpContext() const;
	void UpdateControls(HWND hPage);

protected:
	BOOL OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam);
	BOOL OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl);
	BOOL OnHScroll(HWND hTab, WPARAM wParam, LPARAM lParam);
	BOOL OnNotify(HWND hPage, DWORD ctrlId, const NMHDR* pNmHdr);
	void PopulateStarmapList(HWND hPage);
	void PopulateBgImageList(HWND hPage);
	void StarPixelActivationChanged(HWND hPage);
	void StarmapActivationChanged(HWND hPage);
	void StarmapImageChanged(HWND hPage);
	void BackgroundActivationChanged(HWND hPage);
	void BackgroundImageChanged(HWND hPage);
	void BackgroundBrightnessChanged(HWND hPage, double level);

private:
	std::vector<std::pair<std::string, std::string>> m_pathStarmap;
	std::vector<std::pair<std::string, std::string>> m_pathBgImage;
};

/************************************************************************
 * \brief Main page for visual helpers options.
 */
class OptionsPage_VisHelper : public OptionsPage {
public:
	OptionsPage_VisHelper(OptionsPageContainer* container);
	int ResourceId() const;
	const char* Name() const;
	const HELPCONTEXT* HelpContext() const;
	void UpdateControls(HWND hPage);

protected:
	BOOL OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam);
	BOOL OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl);
};

/************************************************************************
 * \brief Visual helpers "Planetarium" options page.
 */
class OptionsPage_Planetarium : public OptionsPage {
public:
	OptionsPage_Planetarium(OptionsPageContainer* container);
	int ResourceId() const;
	const char* Name() const;
	const HELPCONTEXT* HelpContext() const;
	void UpdateControls(HWND hPage);

protected:
	BOOL OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam);
	BOOL OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl);
	void RescanMarkerList(HWND hPage);
	void OnItemClicked(HWND hPage, WORD ctrlId);
	BOOL OnMarkerSelectionChanged(HWND hPage);
};

/************************************************************************
 * \brief Visual helpers "Labels" options page.
 */
class OptionsPage_Labels : public OptionsPage {
public:
	OptionsPage_Labels(OptionsPageContainer* container);
	int ResourceId() const;
	const char* Name() const;
	const HELPCONTEXT* HelpContext() const;
	void UpdateControls(HWND hPage);

protected:
	BOOL OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam);
	BOOL OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl);
	void OnItemClicked(HWND hPage, WORD ctrlId);
	void ScanPsysBodies(HWND hPage);
	void UpdateFeatureList(HWND hPage);
	void RescanFeatures(HWND hPage);
};

/************************************************************************
 * \brief Visual helpers "Body force vectors" options page.
 */
class OptionsPage_Forces : public OptionsPage {
public:
	OptionsPage_Forces(OptionsPageContainer* container);
	int ResourceId() const;
	const char* Name() const;
	const HELPCONTEXT* HelpContext() const;
	void UpdateControls(HWND hPage);

protected:
	BOOL OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam);
	BOOL OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl);
	void OnItemClicked(HWND hPage, WORD ctrlId);
	BOOL OnHScroll(HWND hTab, WPARAM wParam, LPARAM lParam);
};

/************************************************************************
 * \brief Visual helpers "Object frame axes" options page.
 */
class OptionsPage_Axes : public OptionsPage {
public:
	OptionsPage_Axes(OptionsPageContainer* container);
	int ResourceId() const;
	const char* Name() const;
	const HELPCONTEXT* HelpContext() const;
	void UpdateControls(HWND hPage);

protected:
	BOOL OnInitDialog(HWND hPage, WPARAM wParam, LPARAM lParam);
	BOOL OnCommand(HWND hPage, WORD ctrlId, WORD notification, HWND hCtrl);
	void OnItemClicked(HWND hPage, WORD ctrlId);
	BOOL OnHScroll(HWND hTab, WPARAM wParam, LPARAM lParam);
};

#endif // !__OPTIONSPAGES_H

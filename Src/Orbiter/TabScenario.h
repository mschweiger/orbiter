// Copyright (c) Martin Schweiger
// Licensed under the MIT License

//-----------------------------------------------------------------------------
// Launchpad tab declaration: ScenarioTab
// Scenario selection
//-----------------------------------------------------------------------------

#ifndef __TABSCENARIO_H
#define __TABSCENARIO_H

#include <commctrl.h>
#include "LpadTab.h"
#include "CustomControls.h"
#include <filesystem>
namespace fs = std::filesystem;

namespace orbiter {

	class ScenarioTab : public orbiter::LaunchpadTab {
	public:
		ScenarioTab(const orbiter::LaunchpadDialog* lp);
		~ScenarioTab();

		void Create();

		void GetConfig(const Config* cfg);
		void SetConfig(Config* cfg);

		bool OpenHelp();

		bool DynamicSize() const { return true; }

		BOOL OnSize(int w, int h);

		int GetSelScenario(char* scn, int len);
		// returns name of currently selected scenario file
		// (without path or extension)

		void LaunchpadShowing(bool show);

		BOOL OnNotify(HWND hDlg, int idCtrl, LPNMHDR pnmh);

		BOOL OnMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	protected:
		void RefreshList(bool preserveSelection);
		// refresh the scenario list

		void ScanDirectory(const fs::path &path, HTREEITEM hti);
		// scan scenario files from a subdirectory

		void ScenarioChanged();
		// Respond to a user scenario selection

		void SaveCurScenario();
		// open dialog to allow saving of current scenario, if available

		int SaveCurScenarioAs(const char* name, char* desc, bool replace = false);
		// copy current scenario file into 'name', replacing description with 'desc'.
		// return value: 0=ok, 1=failed, 2=file exists (only checked if replace=false)

		void ClearQSFolder();
		// remove all scenarios in the quicksave folder

		void OpenScenarioHelp();
		// open the help file associated with the current scenario

	private:
		static INT_PTR CALLBACK SaveProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
		// callback function for "scenario save" dialog

		static DWORD WINAPI threadWatchScnList(LPVOID pPrm);
		// thread function for scenario list watcher

		SplitterCtrl splitListDesc;  // splitter control for scenario list(left) and description(right)
		HIMAGELIST imglist;      // image list for scenario icons
		int treeicon_idx[4];     // icon indices for scenario tree
		char scnhelp[128];       // scenario help string, if available
		RECT r_list0;            // initial position of scenario list - REMOVE!
		RECT r_desc0;            // initial position of description block - REMOVE!
		RECT r_pane;             // initial position of list/description splitter pane
		RECT r_save0;            // initial position of "save current" button
		RECT r_clear0;           // initial position of "clear quicksaves" button
		RECT r_info0;            // initial position of "info" button
		RECT r_pause0;           // initial position of "start paused" button
		int infoId;              // IDC_SCN_HTML or IDC_SCN_INFO, depending on which is active
		bool htmldesc;           // Use embedded html viewer for scenario description
		HANDLE hThread;          // scenario directory tree watcher
	};

}

#endif // !__TABSCENARIO_H

// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#define EXPORT_IMGUI_CONTEXT
#define OAPI_IMPLEMENTATION
#include <stdio.h>
#include "OrbiterAPI.h"
#include "DlgMgr.h"
#include "Resource.h"
#include "Orbiter.h"
#include "Log.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "IconsFontAwesome6.h"

using namespace oapi;

extern char DBG_MSG[256];
extern Orbiter *g_pOrbiter;

static int x_sizeframe = GetSystemMetrics (SM_CXSIZEFRAME);
static int y_sizeframe = GetSystemMetrics (SM_CYSIZEFRAME);
static int x_fixedframe = GetSystemMetrics (SM_CXFIXEDFRAME);
static int y_fixedframe = GetSystemMetrics (SM_CYFIXEDFRAME);

static bool doflip = true;

// dialog thread messages
#define TM_OPENDIALOG WM_USER
#define DLG_CAPTIONBUTTON (DLG_CAPTIONCLOSE|DLG_CAPTIONHELP)

struct THREADDATA {
	HINSTANCE hInst;
	HWND hParent;
	int id;
	DWORD flag;
	DLGPROC procDlg;
	void *context;
} g_tdata;

HWND g_hDlg;       // dialog handle passed from dlg to main thread
HANDLE hCreateDlg; // used for synchronising dialog creation
static DIALOGENTRY *de_create = 0;

// ==================================================================
// class DialogManager

DialogManager::DialogManager (Orbiter *orbiter, HWND hAppWnd)
{
	pOrbiter = orbiter;
	gc = orbiter->GetGraphicsClient();
	hWnd = hAppWnd;
	nEntry = 0;
	nList = nListBuf = 0;
	firstEntry = NULL;
	lastEntry = NULL;
	InitImGui();
}

DialogManager::~DialogManager ()
{
	Clear();
	ShutdownImGui();
}


void DialogManager::Init (HWND hAppWnd)
{
	Clear();
	hWnd        = hAppWnd;
}


void DialogManager::Clear ()
{
	DIALOGENTRY *tmp;
	while (firstEntry) {
		tmp = firstEntry;
		firstEntry = firstEntry->next;
		delete tmp->dlg;
		delete tmp;
	}
	lastEntry = NULL;
	if (nListBuf) {
		delete []DlgList;
		DlgList = NULL;
		nListBuf = 0;
	}
	nList = 0;

	nEntry = 0;
}

// =======================================================================

HWND DialogManager::OpenDialogEx (HINSTANCE hInst, int id, HWND hParent, DLGPROC pDlg, DWORD flag, void *context)
{
	if ((flag & DLG_ALLOWMULTI) == 0)
		if (IsEntry (hInst, id)) return NULL; // already open, and multiple instances not allowed
	return AddEntry (hInst, id, hParent, pDlg, flag, context);
}

void DialogManager::OpenDialogAsync (HINSTANCE hInst, int id, HWND hParent, DLGPROC pDlg, DWORD flag, void *context)
{
	if ((flag & DLG_ALLOWMULTI) == 0)
		if (IsEntry (hInst, id)) return; // already open, and multiple instances not allowed
	AddEntryAsync (hInst, id, hParent, pDlg, flag, context);
}

bool DialogManager::CloseDialog (HWND hDlg)
{
	if (DelEntry (hDlg, 0, 0)) {
		//DestroyWindow (hDlg);
		return true;
	}
	return false;
}

void *DialogManager::GetDialogContext (HWND hDlg)
{
	DialogWin *dlg = (DialogWin*)GetWindowLongPtr (hDlg, DWLP_USER);
	return (dlg ? dlg->GetContext() : 0);
}

DIALOGENTRY *DialogManager::AddWindow (HINSTANCE hInst, HWND hWnd, HWND hParent, DWORD flag)
{
	DIALOGENTRY *tmp = new DIALOGENTRY; TRACENEW
	de_create = tmp;
	tmp->dlg = new DialogWin (hInst, hWnd, hParent, flag);

	tmp->prev = lastEntry;
	tmp->next = NULL;

	if (lastEntry)
		lastEntry->next = tmp;
	else
		firstEntry = tmp;

	lastEntry = tmp;
	nEntry++;

	de_create = 0;
	AddList (tmp->dlg->GetHwnd());

	return tmp;
}

HWND DialogManager::AddEntry (HINSTANCE hInst, int id, HWND hParent, DLGPROC pDlg, DWORD flag, void *context)
{
	return AddEntry (new DialogWin (hInst, hParent, id, pDlg, flag, context));
}

HWND DialogManager::AddEntry (DialogWin *dlg)
{
	DIALOGENTRY *tmp = new DIALOGENTRY; TRACENEW
	de_create = tmp;
	tmp->dlg = dlg;
	tmp->prev = lastEntry;
	tmp->next = NULL;
	if (lastEntry)
		lastEntry->next = tmp;
	else
		firstEntry = tmp;
	lastEntry = tmp;
	nEntry++;
	de_create = 0;
	HWND hWnd = tmp->dlg->OpenWindow(); // is this the best place to create the window?
	AddList (hWnd);
	return hWnd;
}

void DialogManager::AddEntryAsync (HINSTANCE hInst, int id, HWND hParent, DLGPROC pDlg, DWORD flag, void *context)
{
#ifdef USEDLGTHREAD
	g_tdata.hInst = hInst;
	g_tdata.hParent = hParent;
	g_tdata.id = id;
	g_tdata.flag = flag;
	g_tdata.procDlg = pDlg;
	g_tdata.context = context;
	PostThreadMessage (thid, TM_OPENDIALOG, (WPARAM)&g_tdata, 0);
#else
	AddEntry (hInst, id, hParent, pDlg, flag, context);
#endif
}

bool DialogManager::DelEntry (HWND hDlg, HINSTANCE hInst, int id)
{
	DIALOGENTRY *tmp;
	for (tmp = firstEntry; tmp; tmp = tmp->next) {
		DialogWin *dlg = tmp->dlg;
		if (hDlg && hDlg != dlg->GetHwnd()) continue;
		if (hInst && hInst != dlg->GetHinst()) continue;
		if (id && id != dlg->GetResId()) continue;
		delete dlg;
		DelList (hDlg);
		if (tmp == firstEntry) firstEntry = tmp->next;
		if (tmp == lastEntry)  lastEntry  = tmp->prev;
		if (tmp->prev) tmp->prev->next = tmp->next;
		if (tmp->next) tmp->next->prev = tmp->prev;
		delete tmp;
		nEntry--;
		return true;
	}
	return false;
}

void DialogManager::AddList (HWND hWnd)
{
	if (nList == nListBuf) { // grow buffer
		HWND *tmp = new HWND[nListBuf += 16];
		if (nList) {
			memcpy (tmp, DlgList, nList*sizeof(HWND));
			delete []DlgList;
		}
		DlgList = tmp;
	}
	DlgList[nList++] = hWnd;
}

void DialogManager::DelList (HWND hWnd)
{
	DWORD i;
	for (i = 0; i < nList; i++) {
		if (DlgList[i] == hWnd) break;
	}
	if (i < nList) {
		for (; i < nList-1; i++)
			DlgList[i] = DlgList[i+1];
		DlgList[i] = NULL;
		nList--;
	}
}

HWND DialogManager::GetNextEntry (HWND hWnd) const
{
	if (!hWnd) {
		if (firstEntry) {
			searchEntry = firstEntry;
			return firstEntry->dlg->GetHwnd();
		} else {
			searchEntry = NULL;
			return NULL;
		}
	} else {
		if (!searchEntry || searchEntry->dlg->GetHwnd() != hWnd) {
			for (searchEntry = firstEntry; searchEntry && searchEntry->dlg->GetHwnd() != hWnd; searchEntry = searchEntry->next);
		}
		if (searchEntry) searchEntry = searchEntry->next;
		return (searchEntry ? searchEntry->dlg->GetHwnd() : NULL);
	}
}

HWND DialogManager::IsEntry (HINSTANCE hInst, int id)
{
	DIALOGENTRY *tmp = firstEntry;
	while (tmp) {
		DialogWin *dlg = tmp->dlg;
		if (dlg->GetHinst() == hInst && dlg->GetResId() == id)
			return tmp->dlg->GetHwnd();
		tmp = tmp->next;
	}
	return 0;
}

bool DialogManager::AddTitleButton (DWORD msg, HBITMAP hBmp, DWORD flag)
{
	return DialogWin::Create_AddTitleButton (msg, hBmp, flag);
}

DWORD DialogManager::GetTitleButtonState (HWND hDlg, DWORD msg)
{
	DIALOGENTRY *tmp;
	for (tmp = firstEntry; tmp; tmp = tmp->next)
		if (hDlg == tmp->dlg->GetHwnd())
			return tmp->dlg->GetTitleButtonState (msg);
	return 0;
}

bool DialogManager::SetTitleButtonState (HWND hDlg, DWORD msg, DWORD state)
{
	state = (state ? 1:0);
	if (DialogWin::Create_SetTitleButtonState (msg, state)) return true;

	DIALOGENTRY *de;
	for (de = firstEntry; de; de = de->next)
		if (hDlg == de->dlg->GetHwnd())
			return de->dlg->SetTitleButtonState (msg, state);
	return false;
}

void DialogManager::UpdateDialogs ()
{
	for (DIALOGENTRY *tmp = firstEntry; tmp; tmp = tmp->next) {
		if (tmp->dlg->UpdateContinuously())
			tmp->dlg->Update();
	}
}

void DialogManager::BroadcastMessage (DWORD msg, void *data)
{
	for (DIALOGENTRY *tmp = firstEntry; tmp; tmp = tmp->next)
		tmp->dlg->Message (msg, data);
}

//-----------------------------------------------------------------------------
// Name: OrbiterDefDialogProc()
// Desc: Default message handler for orbiter dialog boxes
//-----------------------------------------------------------------------------
INT_PTR OrbiterDefDialogProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_SETCURSOR:
		// implements "focus follows mouse" behaviour
		if (!g_pOrbiter->StickyFocus() && g_pOrbiter->Cfg()->CfgUIPrm.MouseFocusMode == 2 && GetFocus() != hDlg &&
			!IsChild (hDlg, GetFocus()) && GetParent (hDlg) == g_pOrbiter->GetRenderWnd()) {
				SetFocus (hDlg);
				return FALSE;
		}
		break;
	case WM_NCLBUTTONDBLCLK: {
		// implements window minimisation on title bar double-click
		DialogWin *dlg = (DialogWin*)GetWindowLongPtr (hDlg, DWLP_USER);
		if (dlg) dlg->ToggleShrink();
		} break;

	// *** Create a timer to force frame updates during dialog box moves ***
	case WM_ENTERSIZEMOVE:
		SetTimer (hDlg, 0xff, 1, NULL);
		return 0;
	case WM_EXITSIZEMOVE:
		KillTimer (hDlg, 0xff);
		return 0;
	case WM_TIMER:
		if (wParam == 0xff) g_pOrbiter->SingleFrame();
		return 0;

	// *** Provide custom buttons in the window title bar ***
//	case WM_SETTEXT:
//	case WM_ACTIVATE:
//	case WM_NCPAINT:
//		PostMessage (hDlg, WM_USER+2, 0, 0);
//		break;
//	case WM_USER+2: {
//		DialogWin *dlg = (DialogWin*)GetWindowLongPtr (hDlg, DWLP_USER);
//		if (dlg) dlg->PaintTitleButtons();
//		} return 0;
//	case WM_NCLBUTTONDOWN:
//		if (wParam == HTCAPTION) {
//			DialogWin *dlg = (DialogWin*)GetWindowLongPtr (hDlg, DWLP_USER);
//			if (dlg) dlg->CheckTitleButtons (MAKEPOINTS (lParam));
//		}
//		return 0;
	//case WM_CTLCOLORDLG: {
	//	HDC hDC = (HDC)wParam;
	//	SetBkColor (hDC, 0x808080);
	//	} return (INT_PTR)GetStockObject (GRAY_BRUSH);
	}
	return FALSE;
}

// ====================================================================
// Tread management for dialog thread
// ====================================================================

void DialogManager::StartDialogThread ()
{
	DWORD WINAPI DlgThreadProc (void *data);
	hThread = CreateThread (NULL, 2048, DlgThreadProc, this, 0, &thid);
	hCreateDlg = CreateEvent (NULL, FALSE, FALSE, NULL);
}

void DialogManager::DestroyDialogThread ()
{
	PostThreadMessage (thid, WM_QUIT, 0, 0);
	CloseHandle (hThread);
	CloseHandle (hCreateDlg);
}

DWORD WINAPI DlgThreadProc (void *data)
{
	DialogManager *dlgmgr = (DialogManager*)data;
	MSG msg;
	void DoDialog (THREADDATA *tdata);

	while (GetMessage (&msg, NULL, 0, 0)) {
		switch (msg.message) {
		case TM_OPENDIALOG: {
			THREADDATA *tdata = (THREADDATA*)msg.wParam;
			dlgmgr->AddEntry (tdata->hInst, tdata->id, tdata->hParent, tdata->procDlg, tdata->flag, tdata->context);
			} break;
		default:
			TranslateMessage (&msg);
			DispatchMessage (&msg);
			break;
		}
	}

	return 0;
}

// ====================================================================
// End tread management
// ====================================================================
// 
// ====================================================================
// ImGui
// ====================================================================

DLLEXPORT ImGuiContext* GImGui = NULL;

const ImWchar* GetGlyphRangesOrbiter()
{
	static const ImWchar ranges[] =
	{
		0x0020, 0x00FF, // Basic Latin + Latin Supplement
		0x00A0, 0x02D9, // Polish characters 
		0x0393, 0x03C2, // Greek characters
		0x221A, 0x221A, // √
		0x222B, 0x222B, // ∫
		0x2260, 0x2264, // ≠ ≤ ≥
		0x02DD, 0x02DD, // ˝
		0,
	};
	return &ranges[0];
}

// Styling adapted from https://gist.github.com/dougbinks/8089b4bbaccaaf6fa204236978d165a9
static void ImGuiSetStyle(bool bStyleDark_,  float alpha_)
{
    // Setup Dear ImGui style
    ImGui::StyleColorsClassic();

    ImGuiStyle& style = ImGui::GetStyle();
        
    style.Alpha = 1.0f;
    style.FrameRounding = 3.0f;
    style.WindowRounding = 3.0f;
    style.ChildRounding = 3.0f;
    style.PopupRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;
    // light style from Pacôme Danhiez (user itamago) https://github.com/ocornut/imgui/pull/511#issuecomment-175719267
    style.Colors[ImGuiCol_Text]                  = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.94f, 0.94f, 0.94f, 0.94f);
    style.Colors[ImGuiCol_PopupBg]               = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
    style.Colors[ImGuiCol_Border]                = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
    style.Colors[ImGuiCol_BorderShadow]          = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    style.Colors[ImGuiCol_FrameBg]               = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
    style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
    style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
    style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_Button]                = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_Header]                = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(1.00f, 1.00f, 1.00f, 0.50f);
    style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);

    if( bStyleDark_ )
    {
        for (int i = 0; i <= ImGuiCol_COUNT; i++)
        {
            ImVec4& col = style.Colors[i];
            float H, S, V;
            ImGui::ColorConvertRGBtoHSV( col.x, col.y, col.z, H, S, V );

            if( S < 0.1f )
            {
                V = 1.0f - V;
            }
            ImGui::ColorConvertHSVtoRGB( H, S, V, col.x, col.y, col.z );
            if( col.w < 1.00f )
            {
                col.w *= alpha_;
            }
        }
    }
    else
    {
        for (int i = 0; i <= ImGuiCol_COUNT; i++)
        {
            ImVec4& col = style.Colors[i];
            if( col.w < 1.00f )
            {
                col.x *= alpha_;
                col.y *= alpha_;
                col.z *= alpha_;
                col.w *= alpha_;
            }
        }
    }
}

void DialogManager::InitImGui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	// Viewports don't play nice when in full screen mode
	if(!pOrbiter->IsFullscreen())
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGuiSetStyle(true, 1.0f); // Dark, alpha

	ImFontConfig config;

	static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	ImFontConfig icons_config;
	icons_config.MergeMode = true;
	icons_config.PixelSnapH = true;
	icons_config.FontDataOwnedByAtlas = false;

	io.Fonts->AddFontFromFileTTF("Roboto-Medium.ttf", 14.0f, &config, GetGlyphRangesOrbiter());
	io.Fonts->AddFontFromFileTTF("fa-solid-900.ttf", 14.0f, &icons_config, icons_ranges);
	io.Fonts->Build();
	

	ImGui_ImplWin32_Init(hWnd);
	gc->clbkImGuiInit();
}

void DialogManager::ShutdownImGui()
{
	gc->clbkImGuiShutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void DialogManager::ImGuiNewFrame()
{
	gc->clbkImGuiNewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	//ImGui::ShowDemoWindow();

	// We can't use a range-based loop here because Show() may unregister the current dialog
	for (auto it = DlgImGuiList.begin(); it != DlgImGuiList.end();)
	{
		auto current = it++;
		if ((*current)->IsActive()) {
			(*current)->Display();
		}
	}
	ImGui::EndFrame();
}

ImGuiDialog::~ImGuiDialog(){
	// Make sure this dialog is no longer referenced in the DialogManager
	oapiCloseDialog(this);
}

void ImGuiDialog::Display() {
	if(ImGui::Begin(name.c_str(), &active)) {
		OnDraw();
	}
	ImGui::End();
	if (!active) OnClose();
}

// ImGui utils
namespace ImGui {
	// Resettable slider from https://github.com/ocornut/imgui/issues/1751
	DLLEXPORT bool SliderFloatReset(const char* label, float* v, float v_min, float v_max, float v_default, const char* display_format)
	{
		bool ret = ImGui::SliderFloat(label, v, v_min, v_max, display_format);
		if (ImGui::BeginPopupContextItem(label))
		{
			char buf[64];
			sprintf(buf, "Reset to %f", v_default);
			if (ImGui::MenuItem(buf))
				*v = v_default;
			ImGui::MenuItem("Close");
			ImGui::EndPopup();
		}
		return ret;
	}

	// From imgui_demo.cpp, with added sameline argument
	DLLEXPORT void HelpMarker(const char* desc, bool sameline)
	{
		if(sameline)
			ImGui::SameLine();
		ImGui::TextDisabled(ICON_FA_CIRCLE_QUESTION);
		if (ImGui::BeginItemTooltip())
		{
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(desc);
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}
}

// ===========================================================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2012 - 2016 Jarmo Nikkanen
// ===========================================================================================


#include "D3D9Client.h"
#include "resource.h"
#include "D3D9Config.h"
#include "D3D9Surface.h"
#include "DebugControls.h"
#include "Commctrl.h"
#include "vObject.h"
#include "vVessel.h"
#include "vPlanet.h"
#include "Mesh.h"
#include "MaterialMgr.h"
#include "VectorHelpers.h"
#include "OapiExtension.h"
#include <stdio.h>
#include <sstream>

enum scale { LIN, SQRT, SQR };

using namespace oapi;
using std::min;
using std::max;

extern HINSTANCE g_hInst;
extern D3D9Client *g_client;

// Little binary helper
#define SETFLAG(bitmap, bit, value) (value ? bitmap |= bit : bitmap &= ~bit)
#define CLAMP(x,a,b) min(max(a,x),b) 

namespace DebugControls {

DbgDisplay dbgdsp = {};
int ambdir = -1;
int bkl_id = 0;
int probe_id = -1;
DWORD dwGFX, dwCmd, nMesh, nGroup, sMesh, sGroup, debugFlags, dspMode, camMode, SelColor, sEmitter;
double camSpeed;
float cpr, cpg, cpb, cpa;
double resbias = 4.0;
char visual[64];
int  origwidth;
HWND hGfxDlg = NULL;
HWND hDlg = NULL;
HWND hDataWnd = NULL;
vObject *vObj = NULL;
D3D9Mesh* hSelMesh = NULL;
std::string buffer("");
std::string buffer2("");
D3DXVECTOR3 PickLocation;

std::map<int, const LightEmitter*> Emitters;

HWND hTipRed, hTipGrn, hTipBlu, hTipAlp;

OPENFILENAMEA OpenTex, SaveTex, SaveMesh;
char OpenFileName[255];
char SaveFileName[255];
char SaveMeshName[255];
char SaveMeshTitle[255];

void UpdateMaterialDisplay(bool bSetup=false);
void SetGFXSliders();
void UpdateLightsSlider();
INT_PTR CALLBACK WndProcGFX(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void OpenGFXDlgClbk(void *context);


struct _Variable {
	float min, max, extmax, def;
	scale Scl;
	bool bUsed;
	bool bGamma;
	char tip[80];
};

struct MatParams {
	MatParams(string n, DWORD i) : name(n), id(i) {}
	string name;
	DWORD id;
};

std::vector<MatParams> PrmList;
std::vector<MatParams> Dropdown;

struct _Params {
	_Variable var[4];
};


_Params Params[10] = { 0 };

struct DbgMeshGrp {
	vector<string> Lines;
	vector<string> Vtx;
	vector<string> Idx;
};


class DbgMesh
{
public:

	DbgMesh(const char* file)
	{
		enum sec { header, group, geom, materials, textures  };
		sec sec = header;
		char buf[256];
		DWORD ng = 0, nm = 0, nt = 0, nv = 0, ni = 0, gi = 0;
		std::ifstream is(file);

		if (is.is_open())
		{
			while (is.getline(buf, 256))
			{
				if (!_strnicmp(buf, "MATERIALS", 9)) sec = materials;
				if (!_strnicmp(buf, "TEXTURES", 8)) sec = textures;
					
				if (sec == header) Header.push_back(buf);
				if (sec == materials) Materials.push_back(buf);
				if (sec == textures) Textures.push_back(buf);
					
				if (!_strnicmp(buf, "GROUPS", 6))
				{				
					if (sscanf(buf + 6, "%d", &ng) != 1) throw std::invalid_argument("End of file");
					Groups.resize(ng);

					for (gi = 0;gi<ng;gi++)
					{
						while (true)
						{
							if (!is.getline(buf, 256)) throw std::invalid_argument("End of file");
							if (buf[0] != ';') Groups[gi].Lines.push_back(buf);

							if (!_strnicmp(buf, "GEOM", 4))
							{
								if (sscanf(buf + 4, "%d %d", &nv, &ni) != 2) throw std::invalid_argument("End of file");
								for (DWORD i = 0; i < nv; i++) {
									if (!is.getline(buf, 256)) throw std::invalid_argument("End of file");
									Groups[gi].Vtx.push_back(buf);
								}
								for (DWORD i = 0; i < ni; i++) {
									if (!is.getline(buf, 256)) throw std::invalid_argument("End of file");
									Groups[gi].Idx.push_back(buf);
								}
								break;
							}
						}
					}
				}
			}
			is.close();
		}
		else {
			LogErr("[MeshDebug] Failed to Open a Mesh [%s]", file);
		}
	}

	~DbgMesh()
	{

	}

	void Save(const char* file)
	{
		std::ofstream os(file);

		if (os.is_open())
		{
			int i = 0;
			for (auto a : Header) os << a << "\n";
			for (auto a : Groups) {
				os << ";--- GroupIndex " << i << " ---\n"; i++;
				for (auto b : a.Lines) os << b << "\n";
				for (auto b : a.Vtx) os << b << "\n";
				for (auto b : a.Idx) os << b << "\n";
			}
			for (auto a : Materials) os << a << "\n";
			for (auto a : Textures) os << a << "\n";

			os.close();
		}
	}

	string GetGroupLabel(DWORD grp)
	{
		string x;
		for (auto a : Groups[grp].Lines) {
			if (a.find("LABEL") != string::npos) {
				std::istringstream iss(a);
				iss >> key >> x;
				return x;
			}
		}
		return string("");
	}

	void SetMeshFlags(DWORD f)
	{
		std::stringstream s;
		s << std::hex << f;
		for (auto &a : Header) {
			if (a.find("MESHFLAGS") != string::npos) {
				a = string("MESHFLAGS ") + s.str();
				return;
			}
		}
		Header.insert(Header.begin() + 1, string("MESHFLAGS ") + s.str());
	}

	void SetGroupFlags(DWORD grp, DWORD f)
	{
		std::stringstream s;
		s << std::hex << f;
		for (auto &a : Groups[grp].Lines) {
			if (a.find("FLAG") != string::npos) {
				a = string("FLAG ") + s.str();
				return;
			}
		}
		auto b = Groups[grp].Lines.begin();
		Groups[grp].Lines.insert(b, string("FLAG ") + s.str());
	}

	void SetGroupMaterial(DWORD grp, DWORD x)
	{
		for (auto& a : Groups[grp].Lines) {
			if (a.find("MATERIAL") != string::npos) {
				a = string("MATERIAL ") + std::to_string(x + 1);
				return;
			}
		}
		auto b = Groups[grp].Lines.begin();
		Groups[grp].Lines.insert(b, string("MATERIAL ") + std::to_string(x));
	}

	void SetGroupTexture(DWORD grp, DWORD x)
	{
		for (auto& a : Groups[grp].Lines) {
			if (a.find("TEXTURE") != string::npos) {
				a = string("TEXTURE ") + std::to_string(x);
				return;
			}
		}
		auto b = Groups[grp].Lines.begin();
		Groups[grp].Lines.insert(b, string("TEXTURE ") + std::to_string(x));
	}

	void SetGroupLabel(DWORD grp, string x)
	{
		for (auto& a : Groups[grp].Lines) {
			if (a.find("LABEL") != string::npos) {
				a = string("LABEL ") + x;
				return;
			}
		}
		auto b = Groups[grp].Lines.begin();
		Groups[grp].Lines.insert(b, string("LABEL ") + x);
	}

	vector<string> Header;
	vector<string> Materials;
	vector<string> Textures;
	vector<DbgMeshGrp> Groups;
	string key;
};


map<D3D9Mesh *, DbgMesh *> dbgMsh;
DbgMesh* hDbgMsh = NULL;


// ===========================================================================
// Same functionality than 'official' GetConfigParam, but for non-provided
// debug-control config parameters
//
const void *GetConfigParam (DWORD paramtype)
{
	switch (paramtype) {
		case CFGPRM_GETSELECTEDMESH  : return (void*)&sMesh;
		case CFGPRM_GETSELECTEDGROUP : return (void*)&sGroup;
		case CFGPRM_GETDEBUGFLAGS    : return (void*)&debugFlags;
		case CFGPRM_GETDISPLAYMODE   : return (void*)&dspMode;
		case CFGPRM_GETCAMERAMODE    : return (void*)&camMode;
		case CFGPRM_GETCAMERASPEED   : return (void*)&camSpeed;
		default                      : return NULL;
	}
}

// =============================================================================================
//
float GetFloatFromBox(HWND hWnd, int item)
{
	char lbl[32];
	GetWindowTextA(GetDlgItem(hWnd, item), lbl, 32);	
	return float(atof(lbl));
}

// =============================================================================================
//
HWND CreateToolTip(int toolID, HWND hDlg, PTSTR pszText)
{
    if (!toolID || !hDlg || !pszText) return NULL;
    
    // Get the window of the tool.
    HWND hwndTool = GetDlgItem(hDlg, toolID);
    // Create the tooltip. g_hInst is the global instance handle.
    HWND hwndTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL, WS_POPUP |TTS_ALWAYSTIP | TTS_BALLOON, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hDlg, NULL, g_hInst, NULL);
    
    if (!hwndTool || !hwndTip) return NULL;
                                                          
    // Associate the tooltip with the tool.
    TOOLINFO toolInfo = { 0 };
    toolInfo.cbSize = sizeof(toolInfo);
    toolInfo.hwnd = hDlg;
    toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    toolInfo.uId = (UINT_PTR)hwndTool;
    toolInfo.lpszText = pszText;
    SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);

    return hwndTip;
}


void SetToolTip(int toolID, HWND hTip, const char *a)
{
	HWND hwndTool = GetDlgItem(hDlg, toolID);
	TOOLINFO toolInfo = { 0 };
	toolInfo.cbSize = sizeof(toolInfo);
	toolInfo.hwnd = hDlg;
	toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	toolInfo.uId = (UINT_PTR)hwndTool;
	toolInfo.lpszText = (PTSTR)a;
	SendMessage(hTip, TTM_UPDATETIPTEXT, 0, (LPARAM)&toolInfo);
}

// =============================================================================================
//
void Create()
{
	vObj = NULL;
	hDlg = NULL;
	hGfxDlg = NULL;
	nMesh = 0;
	nGroup = 0;
	sMesh = 0;
	sGroup = 0;
	debugFlags = 0;
	camSpeed = 0.5;
	camMode = 0;
	dspMode = 0;
	SelColor = 0;
	PickLocation = D3DXVECTOR3(0,0,0);

	cpr = cpg = cpb = cpa = 0.0f;

	if (Config->EnableMeshDbg) {
		dwCmd = oapiRegisterCustomCmd((char*)"D3D9 Debug Controls", (char*)"This dialog allows to control various debug and development features", OpenDlgClbk, NULL);
	}
	else {
		dwCmd = 0;
	}

	dwGFX = oapiRegisterCustomCmd((char*)"D3D9 Graphics Controls", (char*)"This dialog allows to control various graphics options", OpenGFXDlgClbk, NULL);

	resbias = 4.0 + Config->LODBias;
  
	memset(&OpenTex, 0, sizeof(OPENFILENAME));
	memset(OpenFileName, 0, sizeof(OpenFileName));

	OpenTex.lStructSize = sizeof(OPENFILENAME);
	OpenTex.lpstrFile = OpenFileName;
	OpenTex.lpstrInitialDir = "Textures\0";
	OpenTex.nMaxFile = sizeof(OpenFileName);
	OpenTex.lpstrFilter = "*.dds;*.jpg;*.png;*.hdr;*.bmp;*.tga\0";
	OpenTex.nFilterIndex = 0;
	OpenTex.lpstrFileTitle = NULL;
	OpenTex.nMaxFileTitle = 0;
	OpenTex.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	memset(&SaveTex, 0, sizeof(OPENFILENAME));
	memset(SaveFileName, 0, sizeof(SaveFileName));

	SaveTex.lStructSize = sizeof(OPENFILENAME);
	SaveTex.lpstrFile = SaveFileName;
	SaveTex.lpstrInitialDir = "Textures\0";
	SaveTex.nMaxFile = sizeof(SaveFileName);
	SaveTex.lpstrFilter = "*.dds\0";
	SaveTex.nFilterIndex = 0;
	SaveTex.lpstrFileTitle = NULL;
	SaveTex.nMaxFileTitle = 0;
	SaveTex.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

	memset(&SaveMesh, 0, sizeof(OPENFILENAME));
	memset(SaveMeshName, 0, sizeof(SaveMeshName));
	memset(SaveMeshTitle, 0, sizeof(SaveMeshTitle));
	
	SaveMesh.lStructSize = sizeof(OPENFILENAME);
	SaveMesh.lpstrFile = SaveMeshName;
	SaveMesh.lpstrInitialDir = "Meshes\0";
	SaveMesh.nMaxFile = sizeof(SaveMeshName);
	SaveMesh.lpstrFilter = "*.msh\0";
	SaveMesh.nFilterIndex = 0;
	SaveMesh.lpstrFileTitle = SaveMeshTitle;
	SaveMesh.nMaxFileTitle = 0;
	SaveMesh.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

	PrmList.push_back(MatParams("Diffuse", 0));
	PrmList.push_back(MatParams("Ambient", 1));
	PrmList.push_back(MatParams("Specular", 2));
	PrmList.push_back(MatParams("Emission", 3));
	PrmList.push_back(MatParams("Reflect", 4));
	PrmList.push_back(MatParams("Smoothness", 5));
	PrmList.push_back(MatParams("Fresnel", 6));
	PrmList.push_back(MatParams("Emission2", 7));
	PrmList.push_back(MatParams("Metalness", 8));
	PrmList.push_back(MatParams("SpecialFX", 9));
}

// =============================================================================================
//
void Close()
{
	if (hDlg != NULL) {
		oapiCloseDialog(hDlg);
		hDlg = NULL;
	}
	vObj = NULL;
}

// =============================================================================================
//
bool IsActive()
{
	return (hDlg!=NULL);
}

// =============================================================================================
//
int GetSceneDebug()
{
	if (!hDlg) return -1;
	return (int)SendDlgItemMessageA(hDlg, IDC_DBG_SCENEDBG, CB_GETCURSEL, 0, 0);
}

// =============================================================================================
//
DbgDisplay GetSelectedEnvMap()
{
	if (!hDlg) return DbgDisplay(0);
	return (DbgDisplay)SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_GETCURSEL, 0, 0);
}

// =============================================================================================
//
void Release()
{
	vObj = NULL;
	hDlg = NULL;
	hGfxDlg = NULL;
	if (dwCmd) oapiUnregisterCustomCmd(dwCmd);
	if (dwGFX) oapiUnregisterCustomCmd(dwGFX);
	dwCmd = NULL;
	dwGFX = NULL;
	for (auto x : dbgMsh) SAFE_DELETE(x.second);
}

// =============================================================================================
//
void UpdateFlags()
{
	SETFLAG(debugFlags, DBG_FLAGS_SELGRPONLY,	(SendDlgItemMessageA(hDlg, IDC_DBG_GRPO, BM_GETCHECK, 0, 0)==BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_SELMSHONLY,	(SendDlgItemMessageA(hDlg, IDC_DBG_MSHO, BM_GETCHECK, 0, 0)==BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_TILEBOXES,	(SendDlgItemMessageA(hDlg, IDC_DBG_TILEBB, BM_GETCHECK, 0, 0)==BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_BOXES,		(SendDlgItemMessageA(hDlg, IDC_DBG_BOXES, BM_GETCHECK, 0, 0)==BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_SPHERES,		(SendDlgItemMessageA(hDlg, IDC_DBG_SPHERES, BM_GETCHECK, 0, 0)==BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_HLMESH,		(SendDlgItemMessageA(hDlg, IDC_DBG_HSM, BM_GETCHECK, 0, 0)==BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_HLGROUP,		(SendDlgItemMessageA(hDlg, IDC_DBG_HSG, BM_GETCHECK, 0, 0)==BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_SELVISONLY,	(SendDlgItemMessageA(hDlg, IDC_DBG_VISO, BM_GETCHECK, 0, 0)==BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_AMBIENT,	    (SendDlgItemMessageA(hDlg, IDC_DBG_AMBIENT, BM_GETCHECK, 0, 0)==BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_WIREFRAME,	(SendDlgItemMessageA(hDlg, IDC_DBG_WIRE, BM_GETCHECK, 0, 0)==BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_DUALSIDED,	(SendDlgItemMessageA(hDlg, IDC_DBG_DUAL, BM_GETCHECK, 0, 0)==BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_PICK,			(SendDlgItemMessageA(hDlg, IDC_DBG_PICK, BM_GETCHECK, 0, 0)==BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_FPSLIM,		(SendDlgItemMessageA(hDlg, IDC_DBG_FPSLIM, BM_GETCHECK, 0, 0)==BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_NEARCLIP,		(SendDlgItemMessageA(hDlg, IDC_DBG_CLIPDIST, BM_GETCHECK, 0, 0) == BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_RENDEREXT,	(SendDlgItemMessageA(hDlg, IDC_DBG_EXTVC, BM_GETCHECK, 0, 0) == BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_PICKCURRENT,  (SendDlgItemMessageA(hDlg, IDC_DBG_PICKCURRENT, BM_GETCHECK, 0, 0) == BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_NOSUNAMB,		(SendDlgItemMessageA(hDlg, IDC_DBG_NOSUNAMB, BM_GETCHECK, 0, 0) == BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_NOPLNAMB,		(SendDlgItemMessageA(hDlg, IDC_DBG_NOPLNAMB, BM_GETCHECK, 0, 0) == BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_NODYNSUN,		(SendDlgItemMessageA(hDlg, IDC_DBG_NODYNSUN, BM_GETCHECK, 0, 0) == BST_CHECKED));
	SETFLAG(debugFlags, DBG_FLAGS_VCZONES,		(SendDlgItemMessageA(hDlg, IDC_DBG_VCZONES, BM_GETCHECK, 0, 0) == BST_CHECKED));

	Config->EnableLimiter = (int)((debugFlags&DBG_FLAGS_FPSLIM)>0);
}

// =============================================================================================
//
void SetGroupHighlight(bool bStat)
{
	SETFLAG(debugFlags, DBG_FLAGS_HLGROUP, bStat);
}

inline _Variable DefVar(float min, float max, float extmax, scale scl, const char *tip, bool bGamma = false)
{
	_Variable var;
	var.bUsed = true;
	var.Scl = scl;
	var.bGamma = bGamma;
	var.max = max;
	var.extmax = extmax;
	var.min = min;
	strncpy_s(var.tip, 80, tip, 80);
	return var;
}

inline _Variable DefVar(float min, float max, scale scl, const char *tip, bool bGamma=false)
{
	_Variable var;
	var.bUsed = true;
	var.Scl = scl;
	var.bGamma = bGamma;
	var.max = max;
	var.extmax = max;
	var.min = min;
	strncpy_s(var.tip, 80, tip, 80);
	return var;
}

DWORD DropdownList(DWORD x)
{
	if (x >= Dropdown.size()) return 0;
	return Dropdown[x].id;
}

// =============================================================================================
//
void InitMatList(WORD shader)
{
	LRESULT idx = SendDlgItemMessageA(hDlg, IDC_DBG_MATPRP, CB_GETCURSEL, 0, 0);
	SendDlgItemMessageA(hDlg, IDC_DBG_MATPRP, CB_RESETCONTENT, 0, 0);
	
	Dropdown.clear();

	if (shader == SHADER_NULL) {
		std::list<char> list = { 0, 1, 2, 3, 4, 5, 6, 7, 9 };
		for (auto x : list) Dropdown.push_back(PrmList[x]);
		for (auto x : Dropdown) SendDlgItemMessageA(hDlg, IDC_DBG_MATPRP, CB_ADDSTRING, 0, (LPARAM)x.name.c_str());	
	}

	if (shader == SHADER_METALNESS) {
		std::list<char> list = { 0, 3, 5, 7, 8, 9 };
		for (auto x : list) Dropdown.push_back(PrmList[x]);
		for (auto x : Dropdown) SendDlgItemMessageA(hDlg, IDC_DBG_MATPRP, CB_ADDSTRING, 0, (LPARAM)x.name.c_str());
	}

	if (shader == SHADER_BAKED_VC) {
		std::list<char> list = { 0, 3, 5, 7, 8, 9 };
		for (auto x : list) Dropdown.push_back(PrmList[x]);
		for (auto x : Dropdown) SendDlgItemMessageA(hDlg, IDC_DBG_MATPRP, CB_ADDSTRING, 0, (LPARAM)x.name.c_str());
	}

	SendDlgItemMessageA(hDlg, IDC_DBG_MATPRP, CB_SETCURSEL, idx, 0);

	switch (shader) {
	case SHADER_NULL:
		Params[6].var[1] = DefVar(0, 1, LIN, "Maximum intensity");
		Params[6].var[2] = DefVar(10.0f, 4096.0f, SQRT, "Specular lobe size");
		break;
	case SHADER_METALNESS:
	case SHADER_BAKED_VC:
		Params[6].var[1] = DefVar(0, 1, LIN, "Fresnel effect attennuation 1.0 = disabled, 0.0 = max intensity");
		Params[6].var[2].bUsed = false;
		break;
	}
}



// =============================================================================================
//
void OpenDlgClbk(void *context)
{
	char buf[64];
	DWORD idx = 0;
	HWND l_hDlg = oapiOpenDialog(g_hInst, IDD_D3D9MESHDEBUG, WndProc);

	if (l_hDlg) hDlg = l_hDlg; // otherwise open already
	else return;

	RECT rect;
	GetWindowRect(hDlg, &rect);
	SetWindowPos(hDlg, NULL, rect.left, rect.top, 298, rect.bottom - rect.top, SWP_SHOWWINDOW);
	origwidth = rect.right - rect.left;

	SendDlgItemMessage(hDlg, IDC_DBG_FPSLIM, BM_SETCHECK, Config->EnableLimiter==1, 0);

	SendDlgItemMessageA(hDlg, IDC_DBG_DISPLAY, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hDlg, IDC_DBG_DISPLAY, CB_ADDSTRING, 0, (LPARAM)"Everything");
	SendDlgItemMessageA(hDlg, IDC_DBG_DISPLAY, CB_ADDSTRING, 0, (LPARAM)"Selected Visual");
	SendDlgItemMessageA(hDlg, IDC_DBG_DISPLAY, CB_ADDSTRING, 0, (LPARAM)"Selected Mesh");
	SendDlgItemMessageA(hDlg, IDC_DBG_DISPLAY, CB_ADDSTRING, 0, (LPARAM)"Selected Group");
	SendDlgItemMessageA(hDlg, IDC_DBG_DISPLAY, CB_SETCURSEL, 0, 0);

	SendDlgItemMessageA(hDlg, IDC_DBG_CAMERA, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hDlg, IDC_DBG_CAMERA, CB_ADDSTRING, 0, (LPARAM)"Center on visual");
	SendDlgItemMessageA(hDlg, IDC_DBG_CAMERA, CB_ADDSTRING, 0, (LPARAM)"Wheel Fly/Pan Cam");
	SendDlgItemMessageA(hDlg, IDC_DBG_CAMERA, CB_SETCURSEL, 0, 0);

	SendDlgItemMessageA(hDlg, IDC_DBG_DEFSHADER, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hDlg, IDC_DBG_DEFSHADER, CB_ADDSTRING, 0, (LPARAM)"PBR (Old)");
	SendDlgItemMessageA(hDlg, IDC_DBG_DEFSHADER, CB_ADDSTRING, 0, (LPARAM)"Metalness PBR");
	SendDlgItemMessageA(hDlg, IDC_DBG_DEFSHADER, CB_ADDSTRING, 0, (LPARAM)"Baked VC");
	SendDlgItemMessageA(hDlg, IDC_DBG_DEFSHADER, CB_SETCURSEL, 0, 0);

	SendDlgItemMessageA(hDlg, IDC_DBG_SCENEDBG, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hDlg, IDC_DBG_SCENEDBG, CB_ADDSTRING, 0, (LPARAM)"None");
	SendDlgItemMessageA(hDlg, IDC_DBG_SCENEDBG, CB_ADDSTRING, 0, (LPARAM)"Normals Global");
	SendDlgItemMessageA(hDlg, IDC_DBG_SCENEDBG, CB_ADDSTRING, 0, (LPARAM)"Normals Tangent");
	SendDlgItemMessageA(hDlg, IDC_DBG_SCENEDBG, CB_ADDSTRING, 0, (LPARAM)"Height");
	SendDlgItemMessageA(hDlg, IDC_DBG_SCENEDBG, CB_ADDSTRING, 0, (LPARAM)"Height Mk2");
	SendDlgItemMessageA(hDlg, IDC_DBG_SCENEDBG, CB_ADDSTRING, 0, (LPARAM)"Tile Level");
	SendDlgItemMessageA(hDlg, IDC_DBG_SCENEDBG, CB_SETCURSEL, 0, 0);

	SendDlgItemMessageA(hDlg, IDC_DBG_ACTION, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hDlg, IDC_DBG_ACTION, CB_ADDSTRING, 0, (LPARAM)"Convert to DXT5");
	SendDlgItemMessageA(hDlg, IDC_DBG_ACTION, CB_ADDSTRING, 0, (LPARAM)"Convert to RGB8");
	SendDlgItemMessageA(hDlg, IDC_DBG_ACTION, CB_ADDSTRING, 0, (LPARAM)"Convert to RGB4");
	SendDlgItemMessageA(hDlg, IDC_DBG_ACTION, CB_SETCURSEL, 0, 0);

	SendDlgItemMessageA(hDlg, IDC_DBG_TARGET, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hDlg, IDC_DBG_TARGET, CB_ADDSTRING, 0, (LPARAM)"Save");
	SendDlgItemMessageA(hDlg, IDC_DBG_TARGET, CB_ADDSTRING, 0, (LPARAM)"Assign to slot 0");
	SendDlgItemMessageA(hDlg, IDC_DBG_TARGET, CB_ADDSTRING, 0, (LPARAM)"Assign to slot 1");
	SendDlgItemMessageA(hDlg, IDC_DBG_TARGET, CB_ADDSTRING, 0, (LPARAM)"Assign to slot 2");
	SendDlgItemMessageA(hDlg, IDC_DBG_TARGET, CB_SETCURSEL, 0, 0);

	SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_ADDSTRING, 0, (LPARAM)"None");
	SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_ADDSTRING, 0, (LPARAM)"Mirror");
	SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_ADDSTRING, 0, (LPARAM)"Blur 1");
	SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_ADDSTRING, 0, (LPARAM)"Blur 2");
	SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_ADDSTRING, 0, (LPARAM)"Blur 3");
	SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_ADDSTRING, 0, (LPARAM)"Blur 4");
	SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_ADDSTRING, 0, (LPARAM)"SS_ShadowMap");
	SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_ADDSTRING, 0, (LPARAM)"VC_ShadowMap");
	SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_ADDSTRING, 0, (LPARAM)"EX_ShadowMap");
	SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_ADDSTRING, 0, (LPARAM)"Irradiance");
	SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_ADDSTRING, 0, (LPARAM)"GlowMask");
	SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_ADDSTRING, 0, (LPARAM)"ScreenDepth");
	SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_ADDSTRING, 0, (LPARAM)"Normals");
	SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_ADDSTRING, 0, (LPARAM)"LightVisbil.");
	SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_ADDSTRING, 0, (LPARAM)"BakedLightMap");
	SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_ADDSTRING, 0, (LPARAM)"EclipseTbl");
	SendDlgItemMessageA(hDlg, IDC_DBG_ENVMAP, CB_SETCURSEL, 0, 0);
	
	SendDlgItemMessageA(hDlg, IDC_DBG_AMBDIR, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hDlg, IDC_DBG_AMBDIR, CB_ADDSTRING, 0, (LPARAM)"Omnidir");
	SendDlgItemMessageA(hDlg, IDC_DBG_AMBDIR, CB_ADDSTRING, 0, (LPARAM)"From Up (+y)");
	SendDlgItemMessageA(hDlg, IDC_DBG_AMBDIR, CB_ADDSTRING, 0, (LPARAM)"From Down (-y)");
	SendDlgItemMessageA(hDlg, IDC_DBG_AMBDIR, CB_ADDSTRING, 0, (LPARAM)"From Left (-x)");
	SendDlgItemMessageA(hDlg, IDC_DBG_AMBDIR, CB_ADDSTRING, 0, (LPARAM)"From Right (+x)");
	SendDlgItemMessageA(hDlg, IDC_DBG_AMBDIR, CB_ADDSTRING, 0, (LPARAM)"From Fwd (+z)");
	SendDlgItemMessageA(hDlg, IDC_DBG_AMBDIR, CB_ADDSTRING, 0, (LPARAM)"From Aft (-z)");
	SendDlgItemMessageA(hDlg, IDC_DBG_AMBDIR, CB_SETCURSEL, 0, 0);

	SendDlgItemMessageA(hDlg, IDC_DBG_DATASRC, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(hDlg, IDC_DBG_DATASRC, CB_ADDSTRING, 0, (LPARAM)"Exterior");
	SendDlgItemMessageA(hDlg, IDC_DBG_DATASRC, CB_ADDSTRING, 0, (LPARAM)"Interior 0");
	SendDlgItemMessageA(hDlg, IDC_DBG_DATASRC, CB_ADDSTRING, 0, (LPARAM)"Interior 1");
	SendDlgItemMessageA(hDlg, IDC_DBG_DATASRC, CB_ADDSTRING, 0, (LPARAM)"Interior 2");
	SendDlgItemMessageA(hDlg, IDC_DBG_DATASRC, CB_ADDSTRING, 0, (LPARAM)"Interior 3");
	SendDlgItemMessageA(hDlg, IDC_DBG_DATASRC, CB_SETCURSEL, 0, 0);

	SendDlgItemMessageA(hDlg, IDC_DBG_BKLID, CB_RESETCONTENT, 0, 0);
	for (int i = 0; i < 16; i++) {
		sprintf_s(buf, 64, "Baked_%d", i);
		SendDlgItemMessageA(hDlg, IDC_DBG_BKLID, CB_ADDSTRING, 0, (LPARAM)buf);
	}
	SendDlgItemMessageA(hDlg, IDC_DBG_BKLID, CB_ADDSTRING, 0, (LPARAM)"Ambient");
	SendDlgItemMessageA(hDlg, IDC_DBG_BKLID, CB_ADDSTRING, 0, (LPARAM)"DA.Curve");
	SendDlgItemMessageA(hDlg, IDC_DBG_BKLID, CB_ADDSTRING, 0, (LPARAM)"DA.Bounch");
	SendDlgItemMessageA(hDlg, IDC_DBG_BKLID, CB_ADDSTRING, 0, (LPARAM)"DA.Force");
	SendDlgItemMessageA(hDlg, IDC_DBG_BKLID, CB_SETCURSEL, 0, 0);


	SetWindowText(GetDlgItem(hDlg, IDC_DBG_VARA), "1.3");
	SetWindowText(GetDlgItem(hDlg, IDC_DBG_VARB), "0.01");
	SetWindowText(GetDlgItem(hDlg, IDC_DBG_VARC), "0.00");

	// BakedLights slider
	SendDlgItemMessage(hDlg, IDC_DBG_BKLADJ, TBM_SETRANGEMAX, 1, 255);
	SendDlgItemMessage(hDlg, IDC_DBG_BKLADJ, TBM_SETRANGEMIN, 1, 0);
	SendDlgItemMessage(hDlg, IDC_DBG_BKLADJ, TBM_SETTICFREQ, 1, 0);
	SendDlgItemMessage(hDlg, IDC_DBG_BKLADJ, TBM_SETPOS, 1, 0);

	// Resolution bias slider
	SendDlgItemMessage(hDlg, IDC_DBG_RESBIAS, TBM_SETRANGEMAX, 1,  10);
	SendDlgItemMessage(hDlg, IDC_DBG_RESBIAS, TBM_SETRANGEMIN, 1, -10);
	SendDlgItemMessage(hDlg, IDC_DBG_RESBIAS, TBM_SETTICFREQ, 1, 0);
	SendDlgItemMessage(hDlg, IDC_DBG_RESBIAS, TBM_SETPOS, 1, int((resbias-4.0)*5.0));

	// Speed slider
	SendDlgItemMessage(hDlg, IDC_DBG_SPEED, TBM_SETRANGEMAX, 1, 200);
	SendDlgItemMessage(hDlg, IDC_DBG_SPEED, TBM_SETRANGEMIN, 1, 1);
	SendDlgItemMessage(hDlg, IDC_DBG_SPEED, TBM_SETTICFREQ,  1, 0);
	SendDlgItemMessage(hDlg, IDC_DBG_SPEED, TBM_SETPOS,  1, 75);
	SetWindowTextA(GetDlgItem(hDlg, IDC_DBG_SPEEDDSP), "29");

	// Meterial slider
	SendDlgItemMessage(hDlg, IDC_DBG_MATADJ, TBM_SETRANGEMAX, 1, 255);
	SendDlgItemMessage(hDlg, IDC_DBG_MATADJ, TBM_SETRANGEMIN, 1, 0);
	SendDlgItemMessage(hDlg, IDC_DBG_MATADJ, TBM_SETTICFREQ,  1, 0);
	SendDlgItemMessage(hDlg, IDC_DBG_MATADJ, TBM_SETPOS,  1, 0);
	
	// Set the "pick" checked
	SendDlgItemMessage(hDlg, IDC_DBG_PICK, BM_SETCHECK, 1, 0);

	camMode = 0;
	dspMode = 0;

	OBJHANDLE hTgt = oapiCameraTarget();

	SetVisual(g_client->GetScene()->GetVisObject(hTgt));	// This will call SetupMeshGroups()

	UpdateFlags();

	CreateToolTip(IDC_DBG_TARGET, hDlg, (char*)"Select a target where the resulting image is assigned");
	CreateToolTip(IDC_DBG_SEAMS, hDlg, (char*)"Enable seams reduction at each mipmap level");
	CreateToolTip(IDC_DBG_FADE, hDlg, (char*)"Enable mipmap post processing. Contrast and detail is reduced from each mipmap to prevent 'stripes' (See:Fa,Fb)");
	CreateToolTip(IDC_DBG_NORM, hDlg, (char*)"Center color channels at 0.5f to prevent lightening/darkening the results");
	CreateToolTip(IDC_DBG_VARA, hDlg, (char*)"Attennuates high contrast components. Leaves low contrast parts unchanged [1.0 to 1.6]");
	CreateToolTip(IDC_DBG_VARB, hDlg, (char*)"Attennuates everything equally. Typical range [0.00 to 0.03]");
	CreateToolTip(IDC_DBG_VARC, hDlg, (char*)"Apply noise to main level and all mipmaps before attennuation (Fa,Fb)");
	CreateToolTip(IDC_DBG_MORE, hDlg, (char*)"Click to show/hide more options");
	CreateToolTip(IDC_DBG_EXTEND, hDlg, (char*)"Extend Diffuse/Roughess material range beyond 1.0f to allow texture fine tuning.");
	CreateToolTip(IDC_DBG_LINK, hDlg, (char*)"Adjust all color channels at the same time");
	CreateToolTip(IDC_DBG_DEFINED, hDlg, (char*)"Use the material property for rendering and save it");

	hTipRed = CreateToolTip(IDC_DBG_RED, hDlg, (char*)"Red");
	hTipGrn = CreateToolTip(IDC_DBG_GREEN, hDlg, (char*)"Green");
	hTipBlu = CreateToolTip(IDC_DBG_BLUE, hDlg, (char*)"Blue");
	hTipAlp = CreateToolTip(IDC_DBG_ALPHA, hDlg, (char*)"Alpha");

	// Diffuse
	Params[0].var[0] = DefVar(0, 1, 2, SQRT, "Red");
	Params[0].var[1] = DefVar(0, 1, 2, SQRT, "Green");
	Params[0].var[2] = DefVar(0, 1, 2, SQRT, "Blue");
	Params[0].var[3] = DefVar(0, 1, 2, LIN, "Alpha");

	// Ambient
	Params[1].var[0] = DefVar(0, 1, LIN, "Red");
	Params[1].var[1] = DefVar(0, 1, LIN, "Green");
	Params[1].var[2] = DefVar(0, 1, LIN, "Blue");

	// Specular
	Params[2].var[0] = DefVar(0, 1, SQRT, "Red");
	Params[2].var[1] = DefVar(0, 1, SQRT, "Green");
	Params[2].var[2] = DefVar(0, 1, SQRT, "Blue");
	Params[2].var[3] = DefVar(1, 4096.0f, SQRT, "Specular power");

	// Emission
	Params[3].var[0] = DefVar(0, 1, LIN, "Red");
	Params[3].var[1] = DefVar(0, 1, LIN, "Green");
	Params[3].var[2] = DefVar(0, 1, LIN, "Blue");

	// Reflectivity
	Params[4].var[0] = DefVar(0, 1, LIN, "Red");
	Params[4].var[1] = DefVar(0, 1, LIN, "Green");
	Params[4].var[2] = DefVar(0, 1, LIN, "Blue");

	// Smoothness
	Params[5].var[0] = DefVar(0, 1, 2, LIN, "Smoothness");
	Params[5].var[1] = DefVar(0, 3, SQRT, "Texture linearity (default 1.0)");

	// Fresnel
	Params[6].var[0] = DefVar(0.5f, 2, LIN, "Angle dependency");
	Params[6].var[1] = DefVar(0, 1, LIN, "Maximum intensity");
	Params[6].var[2] = DefVar(10.0f, 4096.0f, SQRT, "Specular lobe size");
	
	// Emission2
	Params[7].var[0] = DefVar(0, 2, LIN, "Red");
	Params[7].var[1] = DefVar(0, 2, LIN, "Green");
	Params[7].var[2] = DefVar(0, 2, LIN, "Blue");

	// Metalness
	Params[8].var[0] = DefVar(0, 1, LIN, "Metalness");

	// SpecialFX
	Params[9].var[0] = DefVar(0, 1, LIN, "Part Temperature");
}


// =============================================================================================
//
float _Clamp(float value, DWORD p, DWORD v)
{
	bool bExtend = (SendDlgItemMessageA(hDlg, IDC_DBG_EXTEND, BM_GETCHECK, 0, 0) == BST_CHECKED);
	return CLAMP(value, Params[p].var[v].min, (bExtend ? Params[p].var[v].extmax : Params[p].var[v].max));
}





// =============================================================================================
//
void UpdateShader()
{
	OBJHANDLE hObj = vObj->GetObjHandle();

	if (!oapiIsVessel(hObj)) return;

	if (!hSelMesh) return;

	DWORD Shader = DWORD(SendDlgItemMessageA(hDlg, IDC_DBG_DEFSHADER, CB_GETCURSEL, 0, 0));

	vVessel *vVes = (vVessel *)vObj;
	MatMgr *pMgr = vVes->GetMaterialManager();
	
	switch (Shader) {
	case 0:
		hSelMesh->SetDefaultShader(SHADER_NULL);
		pMgr->RegisterShaderChange(hSelMesh, SHADER_NULL);
		break;
	case 1:
		hSelMesh->SetDefaultShader(SHADER_METALNESS);
		pMgr->RegisterShaderChange(hSelMesh, SHADER_METALNESS);
		break;
	case 2:
		hSelMesh->SetDefaultShader(SHADER_BAKED_VC);
		pMgr->RegisterShaderChange(hSelMesh, SHADER_BAKED_VC);
		break;
	}

	InitMatList(hSelMesh->GetDefaultShader());
}


// =============================================================================================
//
void UpdateGroup(DWORD grp)
{
	if (!hSelMesh) return;
	if (grp >= hSelMesh->GetGroupCount()) return;
	auto g = hSelMesh->GetGroup(grp);

	if (g) {
		SendDlgItemMessage(hDlg, IDC_DBG_NOSHADOW, BM_SETCHECK, (g->UsrFlag & 0x1) != 0, 0);
		SendDlgItemMessage(hDlg, IDC_DBG_NORENDER, BM_SETCHECK, (g->UsrFlag & 0x2) != 0, 0);
		SendDlgItemMessage(hDlg, IDC_DBG_NOLIGHT, BM_SETCHECK, (g->UsrFlag & 0x4) != 0, 0);
		SendDlgItemMessage(hDlg, IDC_DBG_ADDITIVE, BM_SETCHECK, (g->UsrFlag & 0x8) != 0, 0);
		SendDlgItemMessage(hDlg, IDC_DBG_NOCOLOR, BM_SETCHECK, (g->UsrFlag & 0x10) != 0, 0);
		SendDlgItemMessage(hDlg, IDC_DBG_OIT, BM_SETCHECK, (g->UsrFlag & 0x20) != 0, 0);
		SendDlgItemMessage(hDlg, IDC_DBG_DYNAMIC, BM_SETCHECK, 0, 0);

		char buf[64];
		sprintf_s(buf, 64, "%d", g->MtrlIdx);
		SetWindowText(GetDlgItem(hDlg, IDC_DBG_MATIDX), buf);
		sprintf_s(buf, 64, "%d", g->TexIdx);
		SetWindowText(GetDlgItem(hDlg, IDC_DBG_TEXIDX), buf);
		string s = hDbgMsh->GetGroupLabel(grp);
		SetWindowText(GetDlgItem(hDlg, IDC_DBG_GRPLABEL), s.c_str());
	}
}


// =============================================================================================
//
void ValidateGroup(DWORD grp)
{
	if (!hSelMesh) return;
	if (!hDbgMsh) return;

	if (grp >= hSelMesh->GetGroupCount()) return;
	auto g = hSelMesh->GetGroup(grp);
	DWORD f = 0;

	if (g)
	{
		if (SendDlgItemMessage(hDlg, IDC_DBG_NOSHADOW, BM_GETCHECK, 0, 0) == BST_CHECKED) f |= 0x1;
		if (SendDlgItemMessage(hDlg, IDC_DBG_NORENDER, BM_GETCHECK, 0, 0) == BST_CHECKED) f |= 0x2;
		if (SendDlgItemMessage(hDlg, IDC_DBG_NOLIGHT, BM_GETCHECK, 0, 0) == BST_CHECKED) f |= 0x4;
		if (SendDlgItemMessage(hDlg, IDC_DBG_ADDITIVE, BM_GETCHECK, 0, 0) == BST_CHECKED) f |= 0x8;
		if (SendDlgItemMessage(hDlg, IDC_DBG_NOCOLOR, BM_GETCHECK, 0, 0) == BST_CHECKED) f |= 0x10;
		if (SendDlgItemMessage(hDlg, IDC_DBG_OIT, BM_GETCHECK, 0, 0) == BST_CHECKED) f |= 0x20;
		if (SendDlgItemMessage(hDlg, IDC_DBG_DYNAMIC, BM_GETCHECK, 0, 0) == BST_CHECKED) f |= 0x0; // TODO: To be implemented

		g->UsrFlag = f;
		hDbgMsh->SetGroupFlags(grp, f);

		char buf[64];
		GetWindowText(GetDlgItem(hDlg, IDC_DBG_MATIDX), buf, 64);
		g->MtrlIdx = atoi(buf);
		if (g->MtrlIdx >= hSelMesh->GetMaterialCount()) g->MtrlIdx = hSelMesh->GetMaterialCount() - 1;
		hDbgMsh->SetGroupMaterial(grp, g->MtrlIdx);

		GetWindowText(GetDlgItem(hDlg, IDC_DBG_TEXIDX), buf, 64);
		g->TexIdx = atoi(buf);
		if (g->TexIdx >= hSelMesh->GetTextureCount()) g->TexIdx = hSelMesh->GetTextureCount() - 1;
		hDbgMsh->SetGroupTexture(grp, g->TexIdx);
		
		GetWindowText(GetDlgItem(hDlg, IDC_DBG_GRPLABEL), buf, 64);
		hDbgMsh->SetGroupLabel(grp, string(buf));
	}
}


// =============================================================================================
//
void NextDoNotRender()
{
	if (!hSelMesh) return;
	DWORD bak = sGroup;
	sGroup++;
	for (; sGroup < hSelMesh->GetGroupCount(); sGroup++) {
		auto g = hSelMesh->GetGroup(sGroup);
		if (g->UsrFlag & 0x2) {
			SetupMeshGroups();
			return;
		}
	}
	for (sGroup = 0; sGroup < hSelMesh->GetGroupCount(); sGroup++) {
		auto g = hSelMesh->GetGroup(sGroup);
		if (g->UsrFlag & 0x2) {
			SetupMeshGroups();
			return;
		}
	}
	sGroup = bak;
}


// =============================================================================================
//
void UpdateMeshMaterial(float value, DWORD MatPrp, DWORD clr)
{
	OBJHANDLE hObj = vObj->GetObjHandle();
	if (!oapiIsVessel(hObj)) return;
	if (!hSelMesh) return;
	
	DWORD matidx = hSelMesh->GetMeshGroupMaterialIdx(sGroup);
	DWORD texidx = hSelMesh->GetMeshGroupTextureIdx(sGroup);

	D3D9MatExt Mat;
	if (!hSelMesh->GetMaterial(&Mat, matidx)) return;

	switch(MatPrp) {

		case 0:	// Diffuse
		{
			Mat.ModFlags |= D3D9MATEX_DIFFUSE;
			Mat.Diffuse[clr] = _Clamp(value, MatPrp, clr);
			break;
		}

		case 1:	// Ambient
		{
			Mat.ModFlags |= D3D9MATEX_AMBIENT;
			Mat.Ambient[clr] = _Clamp(value, MatPrp, clr);
			break;
		}

		case 2:	// Specular
		{
			Mat.ModFlags |= D3D9MATEX_SPECULAR;
			Mat.Specular[clr] = _Clamp(value, MatPrp, clr);
			break;
		}

		case 3:	// Emission
		{
			Mat.ModFlags |= D3D9MATEX_EMISSIVE;
			Mat.Emissive[clr] = _Clamp(value, MatPrp, clr);
			break;
		}

		case 4:	// Reflectivity
		{
			Mat.ModFlags |= D3D9MATEX_REFLECT;
			Mat.Reflect[clr] = _Clamp(value, MatPrp, clr);
			break;
		}

		case 5:	// Smoothness
		{
			Mat.ModFlags |= D3D9MATEX_ROUGHNESS;
			Mat.Roughness[clr] = _Clamp(value, MatPrp, clr);
			break;
		}

		case 6:	// Fresnel
		{
			Mat.ModFlags |= D3D9MATEX_FRESNEL;
			Mat.Fresnel[clr] = _Clamp(value, MatPrp, clr);
			break;
		}

		case 7:	// Emission2
		{
			Mat.ModFlags |= D3D9MATEX_EMISSION2;
			Mat.Emission2[clr] = _Clamp(value, MatPrp, clr);
			break;
		}

		case 8:	// Metalness
		{
			Mat.ModFlags |= D3D9MATEX_METALNESS;
			Mat.Metalness = _Clamp(value, MatPrp, clr);
			break;
		}

		case 9:	// SpecialFX
		{
			Mat.ModFlags |= D3D9MATEX_SPECIALFX;
			Mat.SpecialFX[clr] = _Clamp(value, MatPrp, clr);
			break;
		}
	}

	hSelMesh->SetMaterial(&Mat, matidx);
	vVessel *vVes = (vVessel *)vObj;
	vVes->GetMaterialManager()->RegisterMaterialChange(hSelMesh, matidx, &Mat);
}


// =============================================================================================
//
DWORD GetModFlags(DWORD MatPrp)
{
	switch (MatPrp) {
		case 0:	return D3D9MATEX_DIFFUSE;
		case 1:	return D3D9MATEX_AMBIENT;
		case 2:	return D3D9MATEX_SPECULAR;
		case 3:	return D3D9MATEX_EMISSIVE;
		case 4:	return D3D9MATEX_REFLECT;
		case 5:	return D3D9MATEX_ROUGHNESS;
		case 6:	return D3D9MATEX_FRESNEL;
		case 7:	return D3D9MATEX_EMISSION2;
		case 8:	return D3D9MATEX_METALNESS;
		case 9:	return D3D9MATEX_SPECIALFX;
	}
	return 0;
}


// =============================================================================================
//
bool IsMaterialModified(DWORD MatPrp)
{
	OBJHANDLE hObj = vObj->GetObjHandle();
	if (!oapiIsVessel(hObj)) return false;
	if (!hSelMesh) return false;

	DWORD matidx = hSelMesh->GetMeshGroupMaterialIdx(sGroup);
	D3D9MatExt Mat;

	if (!hSelMesh->GetMaterial(&Mat, matidx)) return false;

	return (Mat.ModFlags & GetModFlags(MatPrp)) != 0;
}


// =============================================================================================
//
void SetMaterialModified(DWORD MatPrp, bool bState)
{
	OBJHANDLE hObj = vObj->GetObjHandle();

	if (!oapiIsVessel(hObj)) return;
	if (!hSelMesh) return;

	DWORD matidx = hSelMesh->GetMeshGroupMaterialIdx(sGroup);
	D3D9MatExt Mat;

	if (!hSelMesh->GetMaterial(&Mat, matidx)) return;

	if (bState) Mat.ModFlags |= GetModFlags(MatPrp);
	else		Mat.ModFlags &= (~GetModFlags(MatPrp));

	hSelMesh->SetMaterial(&Mat, matidx);
}


// =============================================================================================
//
float GetMaterialValue(DWORD MatPrp, DWORD clr)
{
	OBJHANDLE hObj = vObj->GetObjHandle();

	if (!oapiIsVessel(hObj)) return 0.0f;
	if (!hSelMesh) return 0.0f;

	DWORD matidx = hSelMesh->GetMeshGroupMaterialIdx(sGroup);
	DWORD texidx = hSelMesh->GetMeshGroupTextureIdx(sGroup);

	const D3D9MatExt *pMat = hSelMesh->GetMaterial(matidx);	
	if (!pMat) return 0.0f;

	switch(MatPrp) {

		case 0:	// Diffuse
		{
			switch(clr) {
				case 0: return pMat->Diffuse.x;
				case 1: return pMat->Diffuse.y;
				case 2: return pMat->Diffuse.z;
				case 3: return pMat->Diffuse.w;
			}
			break;
		}

		case 1:	// Ambient
		{
			switch(clr) {
				case 0: return pMat->Ambient.x;
				case 1: return pMat->Ambient.y;
				case 2: return pMat->Ambient.z;
			}
			break;
		}

		case 2:	// Specular
		{
			switch(clr) {
				case 0: return pMat->Specular.x;
				case 1: return pMat->Specular.y;
				case 2: return pMat->Specular.z;
				case 3: return pMat->Specular.w;
			}
			break;
		}

		case 3:	// Emission
		{
			switch(clr) {
				case 0: return pMat->Emissive.x;
				case 1: return pMat->Emissive.y;
				case 2: return pMat->Emissive.z;
			}
			break;
		}

		case 4:	// Reflectivity
		{
			switch(clr) {
				case 0: return pMat->Reflect.x;
				case 1: return pMat->Reflect.y;
				case 2: return pMat->Reflect.z;
			}
			break;
		}

		case 5:	// Roughness
		{
			switch (clr) {
				case 0: return pMat->Roughness.x;
				case 1: return pMat->Roughness.y;
			}
			break;
		}

		case 6:	// Fresnel
		{
			switch(clr) {
				case 0: return pMat->Fresnel.x;	// Angle
				case 1: return pMat->Fresnel.y; // Multiplier
				case 2: return pMat->Fresnel.z; // SpecPower
			}
			break;
		}

		case 7:	// Emission2
		{
			switch (clr) {
				case 0: return pMat->Emission2.x;
				case 1: return pMat->Emission2.y;
				case 2: return pMat->Emission2.z;
			}
			break;
		}

		case 8:	// Metalness
		{
			switch (clr) {
			case 0: return pMat->Metalness;
			}
			break;
		}

		case 9:	// SpecialFX
		{
			switch (clr) {
				case 0: return pMat->SpecialFX.x;
				case 1: return pMat->SpecialFX.y;
				case 2: return pMat->SpecialFX.z;
				case 3: return pMat->SpecialFX.w;
			}
			break;
		}
	}

	return 0.0f;
}

// =============================================================================================
//
void SetColorSlider()
{
	DWORD MatPrp = DropdownList(DWORD(SendDlgItemMessageA(hDlg, IDC_DBG_MATPRP, CB_GETCURSEL, 0, 0)));
	bool bExtend = (SendDlgItemMessageA(hDlg, IDC_DBG_EXTEND, BM_GETCHECK, 0, 0) == BST_CHECKED);

	float mi = Params[MatPrp].var[SelColor].min;
	float mx = (bExtend ? Params[MatPrp].var[SelColor].extmax : Params[MatPrp].var[SelColor].max);

	float val = GetMaterialValue(MatPrp, SelColor);

	val -= mi; val /= (mx - mi);

	if (Params[MatPrp].var[SelColor].Scl == scale::SQRT) val = sqrt(val);
	if (Params[MatPrp].var[SelColor].Scl == scale::SQR) val = val*val;

	SendDlgItemMessage(hDlg, IDC_DBG_MATADJ, TBM_SETPOS,  1, WORD(val*255.0f));
}

// =============================================================================================
//
void DisplayMat(bool bRed, bool bGreen, bool bBlue, bool bAlpha)
{
	char lbl[32];

	DWORD MatPrp = DropdownList(DWORD(SendDlgItemMessageA(hDlg, IDC_DBG_MATPRP, CB_GETCURSEL, 0, 0)));
	
	float r = GetMaterialValue(MatPrp, 0);
	float g = GetMaterialValue(MatPrp, 1);
	float b = GetMaterialValue(MatPrp, 2);
	float a = GetMaterialValue(MatPrp, 3);

	if (bRed) sprintf_s(lbl,32,"%3.3f", r);
	else	  sprintf_s(lbl,32,"");
	SetWindowText(GetDlgItem(hDlg, IDC_DBG_RED),   lbl);
	
	if (bGreen) sprintf_s(lbl,32,"%3.3f", g);
	else		sprintf_s(lbl,32,"");
	SetWindowText(GetDlgItem(hDlg, IDC_DBG_GREEN), lbl);

	if (bBlue) sprintf_s(lbl,32,"%3.3f", b);
	else	   sprintf_s(lbl,32,"");
	SetWindowText(GetDlgItem(hDlg, IDC_DBG_BLUE),  lbl);

	if (bAlpha) sprintf_s(lbl,32,"%3.3f", a);
	else	    sprintf_s(lbl,32,"");
	SetWindowText(GetDlgItem(hDlg, IDC_DBG_ALPHA), lbl);

	if (bRed)   EnableWindow(GetDlgItem(hDlg, IDC_DBG_RED), true);
	else	    EnableWindow(GetDlgItem(hDlg, IDC_DBG_RED), false);	
	if (bGreen) EnableWindow(GetDlgItem(hDlg, IDC_DBG_GREEN), true);
	else		EnableWindow(GetDlgItem(hDlg, IDC_DBG_GREEN), false);	
	if (bBlue)  EnableWindow(GetDlgItem(hDlg, IDC_DBG_BLUE), true);
	else	    EnableWindow(GetDlgItem(hDlg, IDC_DBG_BLUE), false);	
	if (bAlpha) EnableWindow(GetDlgItem(hDlg, IDC_DBG_ALPHA), true);
	else		EnableWindow(GetDlgItem(hDlg, IDC_DBG_ALPHA), false);

	bool bModified = IsMaterialModified(MatPrp);

	SendDlgItemMessageA(hDlg, IDC_DBG_DEFINED, BM_SETCHECK, bModified, 0);
}

// =============================================================================================
//
void UpdateMaterialDisplay(bool bSetup)
{
	char lbl[256];
	char lbl2[64];

	OBJHANDLE hObj = vObj->GetObjHandle();
	if (!oapiIsVessel(hObj)) return;
	if (!hSelMesh) return;

	WORD Shader = hSelMesh->GetDefaultShader();
	if (Shader == SHADER_NULL) SendDlgItemMessageA(hDlg, IDC_DBG_DEFSHADER, CB_SETCURSEL, 0, 0);
	if (Shader == SHADER_METALNESS) SendDlgItemMessageA(hDlg, IDC_DBG_DEFSHADER, CB_SETCURSEL, 1, 0);
	if (Shader == SHADER_BAKED_VC) SendDlgItemMessageA(hDlg, IDC_DBG_DEFSHADER, CB_SETCURSEL, 2, 0);

	DWORD matidx = hSelMesh->GetMeshGroupMaterialIdx(sGroup);

	// Set material info
	const char *skin = NULL;
	if (skin)	sprintf_s(lbl, 256, "Material %u: [Skin %s]", matidx, skin);
	else		sprintf_s(lbl, 256, "Material %u:", matidx);

	GetWindowText(GetDlgItem(hDlg, IDC_DBG_MATGRP), lbl2, 64);
	if (strcmp(lbl, lbl2)) SetWindowText(GetDlgItem(hDlg, IDC_DBG_MATGRP), lbl); // Avoid causing flashing

	if (bSetup) SelColor = 0;

	DWORD MatPrp = DropdownList(DWORD(SendDlgItemMessageA(hDlg, IDC_DBG_MATPRP, CB_GETCURSEL, 0, 0)));
	
	DisplayMat(Params[MatPrp].var[0].bUsed, Params[MatPrp].var[1].bUsed, Params[MatPrp].var[2].bUsed, Params[MatPrp].var[3].bUsed);

	SetToolTip(IDC_DBG_RED, hTipRed, Params[MatPrp].var[0].tip);
	SetToolTip(IDC_DBG_GREEN, hTipGrn, Params[MatPrp].var[1].tip);
	SetToolTip(IDC_DBG_BLUE, hTipBlu, Params[MatPrp].var[2].tip);
	SetToolTip(IDC_DBG_ALPHA, hTipAlp, Params[MatPrp].var[3].tip);

	DWORD texidx = hSelMesh->GetMeshGroupTextureIdx(sGroup);

	if (texidx==0) SetWindowText(GetDlgItem(hDlg, IDC_DBG_TEXTURE), "Texture: None");
	else {
		SURFHANDLE hSrf = hSelMesh->GetTexture(texidx);
		if (hSrf) {
			sprintf_s(lbl, 256, "Texture: %s [%u]", RemovePath(SURFACE(hSrf)->GetName()), texidx);
			SetWindowText(GetDlgItem(hDlg, IDC_DBG_TEXTURE), lbl);
		}
	}

	sprintf_s(lbl, 256, "Mesh: %s", RemovePath(hSelMesh->GetName()));
	SetWindowText(GetDlgItem(hDlg, IDC_DBG_MESHNAME), lbl);

	string str = "";
	DWORD vis = vObj->GetMeshVisMode(sMesh);

	if (vis == 0) str = "NEVER";
	if (vis == MESHVIS_ALWAYS) str = "ALWAYS";

	if (vis & MESHVIS_COCKPIT) str.append("COCKPIT ");
	if (vis & MESHVIS_VC) str.append("VC ");
	if (vis & MESHVIS_EXTERNAL) str.append("EXTERNAL ");
	if (vis & MESHVIS_EXTPASS) str.append("EXTPASS ");

	sprintf_s(lbl, 256, "MeshVisMode: %s", str.c_str());
	SetWindowText(GetDlgItem(hDlg, IDC_DBG_VISMODE), lbl);
}

// =============================================================================================
//
bool IsSelectedGroupRendered()
{
	if (!vObj) return false;
	if (hSelMesh) return hSelMesh->IsGroupRendered(sGroup);
	return false;
}

// =============================================================================================
//
void UpdateColorSlider(WORD pos)
{
	float val = float(pos)/255.0f;
	
	DWORD MatPrp = DropdownList(DWORD(SendDlgItemMessageA(hDlg, IDC_DBG_MATPRP, CB_GETCURSEL, 0, 0)));	

	bool bLink = (SendDlgItemMessageA(hDlg, IDC_DBG_LINK, BM_GETCHECK, 0, 0)==BST_CHECKED);
	bool bExtend = (SendDlgItemMessageA(hDlg, IDC_DBG_EXTEND, BM_GETCHECK, 0, 0) == BST_CHECKED);

	float mi = Params[MatPrp].var[SelColor].min;
	float mx = (bExtend ? Params[MatPrp].var[SelColor].extmax : Params[MatPrp].var[SelColor].max);

	if (MatPrp==5 || MatPrp==6) bLink = false;	// Roughness, Fresnel
	if (SelColor==3) bLink = false;				// Alpha, Specular power

	if (Params[MatPrp].var[SelColor].Scl == scale::SQRT) val = (val*val);
	if (Params[MatPrp].var[SelColor].Scl == scale::SQR) val = sqrt(val);

	val *= (mx - mi); val += mi;

	float old = GetMaterialValue(MatPrp, SelColor);
	float fct = val/old;
	
	if (old<1e-4) fct = val;

	if (bLink) {
		float r = GetMaterialValue(MatPrp, 0);
		float g = GetMaterialValue(MatPrp, 1);
		float b = GetMaterialValue(MatPrp, 2);
		if (r<1e-4) r=1.0f;
		if (g<1e-4) g=1.0f;
		if (b<1e-4) b=1.0f;
		UpdateMeshMaterial(r*fct, MatPrp, 0);
		UpdateMeshMaterial(g*fct, MatPrp, 1);
		UpdateMeshMaterial(b*fct, MatPrp, 2);
	}
	else UpdateMeshMaterial(val, MatPrp, SelColor);
}

// =============================================================================================
//
DWORD GetSelectedMesh()
{
	return sMesh;
}

void SetPickPos(D3DXVECTOR3 pos)
{
	PickLocation = pos;
}

// =============================================================================================
//
void SelectGroup(DWORD idx)
{
	if (idx<nGroup) {
		sGroup = idx;
		SetupMeshGroups();
	}
}

// =============================================================================================
//
void ValidateMesh(D3D9Mesh* pMesh)
{
	if (pMesh != hSelMesh)
	{	
		if (dbgMsh.find(pMesh) == dbgMsh.end()) {
			char MeshFile[MAX_PATH];
			sprintf_s(MeshFile, MAX_PATH, "%s%s.msh", OapiExtension::GetMeshDir(), pMesh->GetName());
			dbgMsh[pMesh] = new DbgMesh(MeshFile);
		}
		hSelMesh = pMesh;
		hDbgMsh = dbgMsh[pMesh];
	}
}

// =============================================================================================
//
void SelectMesh(D3D9Mesh *pMesh)
{
	sMesh = 0;
	for (DWORD i = 0; i < nMesh; i++) {
		if (vObj->GetMesh(i) == pMesh) {
			sMesh = i;
			ValidateMesh(pMesh);
			SetupMeshGroups();
			return;
		}
	}
}

// =============================================================================================
//
void UpdateMeshFlags()
{
	if (!hSelMesh) return;
	DWORD f = 0;

	if (SendDlgItemMessage(hDlg, IDC_DBG_ISVCMESH, BM_GETCHECK, 0, 0) == BST_CHECKED) f |= MESHFLAG_VC;
	if (SendDlgItemMessage(hDlg, IDC_DBG_VCSHADOW, BM_GETCHECK, 0, 0) == BST_CHECKED) f |= MESHFLAG_SHADOW_VC;
	hSelMesh->MeshFlags = f | 0x1;
	hDbgMsh->SetMeshFlags(hSelMesh->MeshFlags);
}

// =============================================================================================
//
void SetupMeshGroups()
{
	char lbl[256];

	if (!vObj) return; 

	SetWindowText(GetDlgItem(hDlg, IDC_DBG_VISUAL), visual);

	if (nMesh!=0) {
		if (sMesh>0xFFFF) sMesh = nMesh-1;
		if (sMesh>=nMesh) sMesh = 0;
	}
	else {
		sMesh=0, sGroup=0, nGroup=0;
		SetWindowText(GetDlgItem(hDlg, IDC_DBG_MESH), "N/A");
		SetWindowText(GetDlgItem(hDlg, IDC_DBG_GROUP), "N/A");
		return;
	}

	sprintf_s(lbl,256,"%u/%u",sMesh,nMesh-1);
	SetWindowText(GetDlgItem(hDlg, IDC_DBG_MESH), lbl);

	ValidateMesh(vObj->GetMesh(sMesh));

	if (hSelMesh) nGroup = hSelMesh->GetGroupCount();
	else nGroup = 0;

	if (nGroup!=0) {
		if (sGroup>0xFFFF)  sGroup = nGroup-1;
		if (sGroup>=nGroup) sGroup = 0;
		UpdateGroup(sGroup);
	}
	else {
		sGroup=0;
		SetWindowText(GetDlgItem(hDlg, IDC_DBG_GROUP), "N/A");
		return;
	}

	sprintf_s(lbl,256,"%u/%u",sGroup,nGroup-1);
	SetWindowText(GetDlgItem(hDlg, IDC_DBG_GROUP), lbl);

	if (hSelMesh) {
		SendDlgItemMessage(hDlg, IDC_DBG_ISVCMESH, BM_SETCHECK, (hSelMesh->MeshFlags & MESHFLAG_VC) != 0, 0);
		SendDlgItemMessage(hDlg, IDC_DBG_VCSHADOW, BM_SETCHECK, (hSelMesh->MeshFlags & MESHFLAG_SHADOW_VC) != 0, 0);
	}

	UpdateMaterialDisplay();
	SetColorSlider();
	UpdateLightsSlider();
	InitMatList(hSelMesh->GetDefaultShader());
}


// =============================================================================================
//
void UpdateBakedLights(float lvl)
{
	vVessel* vV = (vVessel*)vObj;
	if (vObj->Type() == OBJTP_VESSEL)
	{	
		if (bkl_id < 16 && bkl_id >= 0) 
			vV->SetVisualProperty(VisualProp::BAKED_LIGHT, bkl_id, typeid(FVECTOR3), &FVECTOR3(lvl, lvl, lvl));
		if (bkl_id == 16) vV->SetVisualProperty(VisualProp::AMBIENT, 0, typeid(FVECTOR3), &FVECTOR3(lvl, lvl, lvl));
		if (bkl_id == 17) vV->SetVisualProperty(VisualProp::DA_CURVE, 0, typeid(float), &lvl);
		if (bkl_id == 18) vV->SetVisualProperty(VisualProp::DA_BOUNCH, 0, typeid(float), &lvl);
		if (bkl_id == 19) vV->SetVisualProperty(VisualProp::DA_FORCE, 0, typeid(float), &lvl);

		char lbl[128]; sprintf_s(lbl, 128, "Light Controls  (%1.3f)", lvl);
		SetWindowText(GetDlgItem(hDlg, IDC_DBG_BKLGROUP), lbl);
	}
}

// =============================================================================================
//
void UpdateLightsSlider()
{
	FVECTOR3 val = 0.0f;
	float fVal = 0.0f;
	vVessel* vV = (vVessel*)vObj;
	if (vObj->Type() == OBJTP_VESSEL)
	{
		if (bkl_id < 16 && bkl_id >= 0) vV->GetVisualProperty(VisualProp::BAKED_LIGHT, bkl_id, typeid(val), &val);
		if (bkl_id == 16) vV->GetVisualProperty(VisualProp::AMBIENT, 0, typeid(val), &val);
		fVal = val.x;
		if (bkl_id == 17) vV->GetVisualProperty(VisualProp::DA_CURVE, 0, typeid(float), &fVal);
		if (bkl_id == 18) vV->GetVisualProperty(VisualProp::DA_BOUNCH, 0, typeid(float), &fVal);
		if (bkl_id == 19) vV->GetVisualProperty(VisualProp::DA_FORCE, 0, typeid(float), &fVal);
		SendDlgItemMessage(hDlg, IDC_DBG_BKLADJ, TBM_SETPOS, 1, WORD(255.0f * fVal));
		char lbl[128]; sprintf_s(lbl, 128, "Light Controls  (%1.3f)", fVal);
		SetWindowText(GetDlgItem(hDlg, IDC_DBG_BKLGROUP), lbl);
	}
}

// =============================================================================================
//
LPDIRECT3DTEXTURE9 GetCombinedMap()
{
	if (hSelMesh) {
		DWORD texidx = hSelMesh->GetMeshGroupTextureIdx(sGroup);
		return hSelMesh->GetCombinedMap(texidx);
	}
	return NULL;
}

// =============================================================================================
//
double GetVisualSize()
{
	if (hDlg && vObj) {
		OBJHANDLE hObj = vObj->GetObjHandle();
		if (hObj) return oapiGetSize(hObj);
	}
	return 1.0;
}

// =============================================================================================
//
vObject * GetVisual()
{
	return vObj;
}

// =============================================================================================
//
D3D9Mesh* GetMesh()
{
	return hSelMesh;
}

// =============================================================================================
//
void SetVisual(vObject *vo)
{
	if (!hDlg) {
		vObj = NULL;	// Always set the visual to NULL if the dialog isn't open
		return;
	}
	vObj = vo;
	UpdateVisual();
}

// =============================================================================================
//
void UpdateVisual()
{
	if (!vObj || !hDlg) return; 
	nMesh = vObj->GetMeshCount();
	sprintf_s(visual, 64, "Visual: %s", vObj->GetName());
	SetupMeshGroups();

	SendDlgItemMessageA(hDlg, IDC_DBG_CONES, CB_RESETCONTENT, 0, 0);
	Emitters.clear();

	if (vObj->Type() == OBJTP_VESSEL) {

		SendDlgItemMessageA(hDlg, IDC_DBG_CONES, CB_ADDSTRING, 0, (LPARAM)"NONE");
		Emitters[0] = NULL;

		char line[64]; strcpy(line, "");

		vVessel *vV = static_cast<vVessel*>(vObj);
		VESSEL *vessel = vV->GetInterface();
		DWORD nemitter = vessel->LightEmitterCount();

		for (DWORD j = 0; j < nemitter; j++) {

			const LightEmitter *em = vessel->GetLightEmitter(j);
			
			if (em->GetType() == LightEmitter::LT_SPOT) {
				const SpotLight *sl = static_cast<const SpotLight*>(em);
				double P = sl->GetPenumbra()*DEG;
				double U = sl->GetUmbra()*DEG;
				double R = sl->GetRange();
				sprintf_s(line, 64, "%s P%1.0f U%1.0f R%1.0f", _PTR(em), P, U, R);
			}	

			if (em->GetType() == LightEmitter::LT_POINT) {
				const PointLight *pl = static_cast<const PointLight*>(em);
				double R = pl->GetRange();
				sprintf_s(line, 64, "%s R%1.0f", _PTR(em), R);
			}

			switch (em->GetVisibility())
			{
			case LightEmitter::VIS_EXTERNAL: strcat_s(line, 64, " EXT"); break;
			case LightEmitter::VIS_COCKPIT: strcat_s(line, 64, " VC"); break;
			case LightEmitter::VIS_ALWAYS: strcat_s(line, 64, " ALW"); break;
			}

			if ((em->GetType() == LightEmitter::LT_SPOT) || (em->GetType() == LightEmitter::LT_POINT)) {
				SendDlgItemMessageA(hDlg, IDC_DBG_CONES, CB_ADDSTRING, 0, (LPARAM)line);
				Emitters[j + 1] = em;
			}
		}
	}

	SendDlgItemMessageA(hDlg, IDC_DBG_CONES, CB_SETCURSEL, 0, 0);
}

// =============================================================================================
//
void RemoveVisual(vObject *vo)
{
	if (vObj==vo) vObj=NULL;
}

// =============================================================================================
//
void SetColorValue(const char *lbl)
{
	DWORD MatPrp = DropdownList(DWORD(SendDlgItemMessageA(hDlg, IDC_DBG_MATPRP, CB_GETCURSEL, 0, 0)));
	UpdateMeshMaterial(float(atof(lbl)), MatPrp, SelColor);
	SetColorSlider();
}

// =============================================================================================
//
float reduce(float v, float q)
{
	if (v>0) return max(0.0f, v-q);
	else     return min(0.0f, v+q);
}

struct PCParam {
	DWORD Action;
	DWORD Func;
	DWORD Mip;
	float a,b,c;
};

// =============================================================================================
//
D3DXCOLOR ProcessColor(D3DXVECTOR4 C, PCParam *prm, int x, int y)
{
	float fMip = float(prm->Mip);
	float a = prm->a;
	float b = prm->b;
	float c = prm->c;

	// Swap color channels
	C = D3DXVECTOR4(C.z, C.y, C.z, C.x);
	
	if (c>0.001) {
		D3DXVECTOR4 rnd((float)oapiRand(),(float)oapiRand(), (float)oapiRand(), (float)oapiRand());
		C += (rnd*2.0f-1.0f) * (c/(2.0f+fMip));
	}
	
	// Reduce contrast
	if (prm->Func & 0x2) {

		if (prm->Mip==0) return D3DXCOLOR(C.x, C.y, C.z, C.w);		// Do nothing for the main level

		C = C*2.0f - 1.0f;			// Expand to [-1, 1]
		C *= pow(a, -abs(C)*fMip);

		float k = b * fMip;

		C = D3DXVECTOR4(reduce(C.x, k), reduce(C.y, k), reduce(C.z, k), reduce(C.w, k));
		C = C*0.5f + 0.5f;			// Back to [0, 1]
	}

	return D3DXCOLOR(C.x, C.y, C.z, C.w);
}

// =============================================================================================
//
bool Execute(HWND hWnd, LPOPENFILENAME pOF)
{
	LPDIRECT3DDEVICE9 pDevice = g_client->GetDevice();

	SaveTex.hwndOwner = hWnd;

	D3DLOCKED_RECT in, out;
	PCParam prm;

	DWORD Func = 0;
	DWORD Action = DWORD(SendDlgItemMessageA(hDlg, IDC_DBG_ACTION, CB_GETCURSEL, 0, 0));
	DWORD Target = DWORD(SendDlgItemMessageA(hDlg, IDC_DBG_TARGET, CB_GETCURSEL, 0, 0));

	if (SendDlgItemMessageA(hDlg, IDC_DBG_NORM, BM_GETCHECK, 0, 0)==BST_CHECKED) Func |= 0x1;
	if (SendDlgItemMessageA(hDlg, IDC_DBG_FADE, BM_GETCHECK, 0, 0)==BST_CHECKED) Func |= 0x2;
	if (SendDlgItemMessageA(hDlg, IDC_DBG_SEAMS, BM_GETCHECK, 0, 0)==BST_CHECKED) Func |= 0x4;
	
	if (Action>=0 && Action<=2) {

		LPDIRECT3DTEXTURE9 pTex = NULL;
		LPDIRECT3DTEXTURE9 pWork = NULL;
		LPDIRECT3DTEXTURE9 pSave = NULL;
		D3DXIMAGE_INFO info;

		HR(D3DXCreateTextureFromFileExA(pDevice, pOF->lpstrFile, D3DX_DEFAULT_NONPOW2, D3DX_DEFAULT_NONPOW2, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, D3DX_DEFAULT, D3DX_DEFAULT, 0, &info, NULL, &pTex));

		if (!pTex) {
			LogErr("Failed to open a file [%s]", pOF->lpstrFile); 
			return false;
		}

		DWORD mips = pTex->GetLevelCount();

		if (true) {

			HR(D3DXCreateTexture(pDevice, info.Width, info.Height, mips, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pWork));
			if (!pWork) return false;

			if (Action==0) { HR(D3DXCreateTexture(pDevice, info.Width, info.Height, mips, 0, D3DFMT_DXT5, D3DPOOL_SYSTEMMEM, &pSave)); }
			if (Action==1) { HR(D3DXCreateTexture(pDevice, info.Width, info.Height, mips, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pSave)); }
			if (Action==2) { HR(D3DXCreateTexture(pDevice, info.Width, info.Height, mips, 0, D3DFMT_A4R4G4B4, D3DPOOL_SYSTEMMEM, &pSave)); }

			if (!pSave) return false;

			
			// Process texture ----------------------------------------------
			//
			for (DWORD n=0;n<mips;n++) {

				D3DXCOLOR seam;

				DWORD w = info.Width>>n;
				DWORD h = info.Height>>n;
				HR(pTex->LockRect(n, &in, NULL, 0)); 
				HR(pWork->LockRect(n, &out, NULL, 0));
				DWORD *pIn  = (DWORD *)in.pBits;
				DWORD *pOut = (DWORD *)out.pBits;

				prm.Mip = n;
				prm.Action = Action;
				prm.Func = Func;
				prm.a = GetFloatFromBox(hDlg, IDC_DBG_VARA);
				prm.b = GetFloatFromBox(hDlg, IDC_DBG_VARB);
				prm.c = GetFloatFromBox(hDlg, IDC_DBG_VARC);
				
				for (DWORD y=0;y<h;y++) {
					for (DWORD x=0;x<w;x++) {

						D3DXCOLOR c(pIn[x + y*w]);
						DWORD r = w-1;
						DWORD b = h-1;
						
						if (Func&0x4) {
							seam = c;
							if (x==0) seam = D3DXCOLOR(pIn[r + y*w]);
							if (x==r) seam = D3DXCOLOR(pIn[0 + y*w]);
							if (y==0) seam = D3DXCOLOR(pIn[x + b*w]);
							if (y==b) seam = D3DXCOLOR(pIn[x + 0*w]);
							c = (c*2.0f + seam) * 0.33333f;
						}

						pOut[x + y*w] = ProcessColor(D3DXVECTOR4(c.r, c.g, c.b, c.a), &prm, x, y);
					}
				}

				// Balance fine correction
				if (Func&0x1) {
					D3DXCOLOR c = D3DXCOLOR(0,0,0,0);
					DWORD s = w*h;
					for (DWORD x=0;x<s;x++) c += D3DXCOLOR(pOut[x]);
					c *= 1.0f/float(w*h);
					c -= D3DXCOLOR(0.5f, 0.5f, 0.5f, 0.5f);
					for (DWORD x=0;x<s;x++) {
						D3DXCOLOR q = D3DXCOLOR(pOut[x]);
						pOut[x] = D3DXCOLOR(q.r-c.r, q.g, q.b-c.b, q.a);
					}
				}


				HR(pTex->UnlockRect(n));
				HR(pWork->UnlockRect(n));
			}

			SAFE_RELEASE(pTex);

			// Convert texture format --------------------------------------
			//
			for (DWORD n=0;n<mips;n++) {
				LPDIRECT3DSURFACE9 pIn, pOut;
				pWork->GetSurfaceLevel(n, &pIn);
				pSave->GetSurfaceLevel(n, &pOut);
				if (D3DXLoadSurfaceFromSurface(pOut, NULL, NULL, pIn, NULL, NULL, D3DX_FILTER_LINEAR, 0)!=S_OK) {
					LogErr("D3DXLoadSurfaceFromSurface Failed");
					return false;
				}
				pIn->Release();
				pOut->Release();
			}
			SAFE_RELEASE(pWork);
		}
		else {
			// Copy input to output	
			pSave = pTex;
		}

		// Save the texture into a file -------------------------------
		//
		if (Target==0) {
			strcpy_s(SaveTex.lpstrFile, 255, OpenTex.lpstrFile);
			if (GetSaveFileName(&SaveTex)) {
				if (D3DXSaveTextureToFileA(SaveTex.lpstrFile, D3DXIFF_DDS, pSave, NULL)!=S_OK) {
					LogErr("Failed to create a file [%s]",SaveTex.lpstrFile); 
					return false;
				}
				SAFE_RELEASE(pSave);
				return true;
			}
		}

		// Assign the texture for rendering -----------------------------
		//
		if (Target==1 || Target==2 || Target==3) {
			vPlanet *vP = g_client->GetScene()->GetCameraProxyVisual();	
			if (vP) vP->SetMicroTexture(pSave, Target-1);
			SAFE_RELEASE(pSave);
			return true;
		}
	}

	return false;
}
			

// =============================================================================================
//
void SaveEnvMap()
{
	LPDIRECT3DDEVICE9 pDevice = g_client->GetDevice();

	OBJHANDLE hObj = vObj->Object();

	if (oapiIsVessel(hObj)) {
		vVessel *vVes = (vVessel *)vObj;
		auto pTex = vVes->GetExteriorEnvMap() ? vVes->GetExteriorEnvMap()->pCube : nullptr;
		if (pTex->GetType() == D3DRTYPE_CUBETEXTURE) {
			if (D3DXSaveTextureToFileA("EnvMap.dds", D3DXIFF_DDS, (LPDIRECT3DCUBETEXTURE9)pTex, NULL) != S_OK) {
				LogErr("Failed to save envmap");
			}
		}
	}
}

//-------------------------------------------------------------------------------------------
//
void Append(const char *format, ...)
{
	if (hDataWnd == NULL) return;
	char buf[256];
	va_list args;
	va_start(args, format);
	_vsnprintf_s(buf, 256, 256, format, args);
	va_end(args);
	buffer += buf;
}

//-------------------------------------------------------------------------------------------
//
void Append2(const char *format, ...)
{
	if (hDataWnd == NULL) return;
	char buf[256];
	va_list args;
	va_start(args, format);
	_vsnprintf_s(buf, 256, 256, format, args);
	va_end(args);
	buffer2 += buf;
}

//-------------------------------------------------------------------------------------------
//
void Refresh2()
{
	if (hDataWnd == NULL) return;

	Append2("LocalPos = [%f, %f, %f]", PickLocation.x, PickLocation.y, PickLocation.z);

	SetWindowTextA(GetDlgItem(hDataWnd, IDC_DBG_DATAVIEW2), buffer2.c_str());
	buffer2.clear();
}

//-------------------------------------------------------------------------------------------
//
void Refresh()
{
	if (hDataWnd == NULL) return;
	SetWindowTextA(GetDlgItem(hDataWnd, IDC_DBG_DATAVIEW), buffer.c_str());
	buffer.clear();
	Refresh2();
}

//-------------------------------------------------------------------------------------------
//
void CreateSamplingKernel()
{
	int s = 27;
	int k = 0;
	
	VECTOR3 *data = new VECTOR3[s];
	
	double d = 0.0;
	double a = 0.0;
	
	for (int i = 0; i < s; i++) {	
		double z = (oapiRand()*2.0 - 1.0) * (PI2 / 8);
		data[i].x = sqrt(d) * sin(a+z);
		data[i].y = sqrt(d) * cos(a+z);
		//data[i].x = d * sin(a + z);
		//data[i].y = d * cos(a + z);
		data[i].z = sqrt(data[i].x*data[i].x + data[i].y*data[i].y);
		data[i].z = sqrt(data[i].z);
		data[i].z = 1.0;
		a += PI2 / 4;
		d += 1.0 / double(s);
	}

	for (int i = 0; i < s; i++) {
		oapiWriteLogV("{%4.4ff, %4.4ff, %4.4ff},", data[i].x, data[i].y, data[i].z);
	}

	delete[]data;
}

/*
//-------------------------------------------------------------------------------------------
//
void CreateSamplingKernel()
{
VECTOR3 *data = new VECTOR3[64];

while (true) {

double ava = 0, avb = 0;

for (int i = 0; i < 64; i++) {

double a = oapiRand();
double b = oapiRand() * PI2;
double c = cos(a*PI05);

data[i] = _V(sin(b)*a, cos(b)*a, c);

ava += data[i].x;
avb += data[i].y;
}

// Ensure proper balance
if (ava < 0.1 && avb < 0.1) break;
}

double w = 0.0f;

for (int i = 0; i < 64; i++) {
oapiWriteLogV("{%4.4ff, %4.4ff, %4.4ff},", data[i].x, data[i].y, data[i].z);
w += data[i].z;
}

oapiWriteLogV("TotalWeight = %f", w);
}
*/


// ==============================================================
// Dialog message handler

INT_PTR CALLBACK ViewProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool isOpen = false; // IDC_DBG_MORE (full or reduced width)

	DWORD Prp = DropdownList(DWORD(SendDlgItemMessageA(hDlg, IDC_DBG_MATPRP, CB_GETCURSEL, 0, 0)));
	
	switch (uMsg) {

	case WM_INITDIALOG:
	{
		SetWindowTextA(GetDlgItem(hWnd, IDC_DBG_DATAVIEW), "-- Select a mesh group --");
		buffer.clear();
		return TRUE;	// All Init actions are done in OpenDlgClbk();
	}

	case WM_COMMAND:

		switch (LOWORD(wParam)) {

		case IDCANCEL:
			oapiCloseDialog(hWnd);
			if (!buffer.empty()) buffer.clear();
			hDataWnd = NULL;
			return TRUE;
		}
		break;
	}

	return oapiDefDialogProc(hWnd, uMsg, wParam, lParam);
}


// ==============================================================
// Dialog message handler

INT_PTR CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char lbl[32];
	RECT rect;
	bool bPaused;
	static bool isOpen = false; // IDC_DBG_MORE (full or reduced width)

	OpenTex.hwndOwner = hWnd;

	DWORD Prp = DropdownList(DWORD(SendDlgItemMessageA(hDlg, IDC_DBG_MATPRP, CB_GETCURSEL, 0, 0)));
	
	switch (uMsg) {

	case WM_INITDIALOG:
	{
		isOpen = false; // We always start with reduced width
		return TRUE;	// All Init actions are done in OpenDlgClbk();
	}

	case WM_HSCROLL:
	{
		if (LOWORD(wParam)==TB_THUMBTRACK || LOWORD(wParam)==TB_ENDTRACK) {
			WORD pos = HIWORD(wParam);

			if (HWND(lParam)==GetDlgItem(hWnd, IDC_DBG_SPEED)) {
				if (pos==0) pos = WORD(SendDlgItemMessage(hDlg, IDC_DBG_SPEED, TBM_GETPOS,  0, 0));
				double fpos = pow(2.0,double(pos)*13.0/200.0); 
				sprintf_s(lbl,32,"%1.0f",fpos);
				SetWindowTextA(GetDlgItem(hWnd, IDC_DBG_SPEEDDSP), lbl);
				camSpeed = fpos/50.0;
			}

			if (HWND(lParam) == GetDlgItem(hWnd, IDC_DBG_RESBIAS)) {
				resbias = 4.0 + 0.2 * double(SendDlgItemMessage(hDlg, IDC_DBG_RESBIAS, TBM_GETPOS, 0, 0));
			}

			if (HWND(lParam)==GetDlgItem(hWnd, IDC_DBG_MATADJ)) {
				if (pos==0) pos = WORD(SendDlgItemMessage(hDlg, IDC_DBG_MATADJ, TBM_GETPOS,  0, 0));
				UpdateColorSlider(pos);
				UpdateMaterialDisplay();
			}

			if (HWND(lParam) == GetDlgItem(hWnd, IDC_DBG_BKLADJ)) {
				if (pos == 0) pos = WORD(SendDlgItemMessage(hDlg, IDC_DBG_BKLADJ, TBM_GETPOS, 0, 0));
				float val = float(pos) / 255.0f;
				UpdateBakedLights(val);
			}
		}
		return false;
	}

	case WM_COMMAND:

		switch (LOWORD(wParam)) {

			case IDCANCEL:
				Close();
				break;
		
			case IDC_DBG_KERNEL:
			{
				CreateSamplingKernel();
				break;
			}

			case IDC_DBG_NEXT:
			{
				NextDoNotRender();
				break;
			}

			case IDC_DBG_MATSAVE:
			{
				OBJHANDLE hObj = vObj->GetObjHandle();
				if (oapiIsVessel(hObj)) {
					vVessel *vVes = (vVessel *)vObj;
					vVes->GetMaterialManager()->SaveConfiguration();
				}
				break;
			}

			case IDC_DBG_DATAWND:
			{
				HWND hW = oapiOpenDialog(g_hInst, IDD_DEBUGVIEW, ViewProc);
				if (hW) hDataWnd = hW;
			}
				break;

			case IDC_DBG_LINK:
				break;

			case IDC_DBG_DEFINED:
				SetMaterialModified(Prp, (SendDlgItemMessageA(hDlg, IDC_DBG_DEFINED, BM_GETCHECK, 0, 0) == BST_CHECKED));
				break;

			case IDC_DBG_COPY:
			{
				cpr = GetMaterialValue(Prp, 0);
				cpg = GetMaterialValue(Prp, 1);
				cpb = GetMaterialValue(Prp, 2);
				break;
			}

			case IDC_DBG_PASTE:
			{
				UpdateMeshMaterial(cpr, Prp, 0);
				UpdateMeshMaterial(cpg, Prp, 1);
				UpdateMeshMaterial(cpb, Prp, 2);
				UpdateMaterialDisplay();
				SetColorSlider();
				break;
			}

			case IDC_DBG_RED:
				if (HIWORD(wParam)==EN_SETFOCUS) {
					SelColor = 0;
					UpdateMaterialDisplay();
					SetColorSlider();
				}
				if (HIWORD(wParam)==EN_KILLFOCUS) {
					GetWindowTextA(HWND(lParam), lbl, 32);
					SetColorValue(lbl);
				}
				break;

			case IDC_DBG_GREEN:
				if (HIWORD(wParam)==EN_SETFOCUS) {
					SelColor = 1;
					UpdateMaterialDisplay();
					SetColorSlider();
				}
				if (HIWORD(wParam)==EN_KILLFOCUS) {
					GetWindowTextA(HWND(lParam), lbl, 32);
					SetColorValue(lbl);
				}
				break;

			case IDC_DBG_BLUE:
				if (HIWORD(wParam)==EN_SETFOCUS) {
					SelColor = 2;
					UpdateMaterialDisplay();
					SetColorSlider();
				}
				if (HIWORD(wParam)==EN_KILLFOCUS) {
					GetWindowTextA(HWND(lParam), lbl, 32);
					SetColorValue(lbl);
				}
				break;

			case IDC_DBG_ALPHA:
				if (HIWORD(wParam)==EN_SETFOCUS) {
					SelColor = 3;
					UpdateMaterialDisplay();
					SetColorSlider();
				}
				if (HIWORD(wParam)==EN_KILLFOCUS) {
					GetWindowTextA(HWND(lParam), lbl, 32);
					SetColorValue(lbl);
				}
				break;

			case IDC_DBG_MATPRP:
				if (HIWORD(wParam)==CBN_SELCHANGE) {
					UpdateMaterialDisplay(true);
					SetColorSlider();	
				}
				break;

			case IDC_DBG_CONES:
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					sEmitter = DWORD(SendDlgItemMessage(hDlg, IDC_DBG_CONES, CB_GETCURSEL, 0, 0));
				}
				break;

			case IDC_DBG_DEFSHADER:
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					UpdateShader();
				}
				break;

			case IDC_DBG_BKLID:
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					bkl_id = int(SendDlgItemMessage(hDlg, IDC_DBG_BKLID, CB_GETCURSEL, 0, 0));
					UpdateLightsSlider();
				}
				break;

			case IDC_DBG_DATASRC:
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					probe_id = int(SendDlgItemMessage(hDlg, IDC_DBG_DATASRC, CB_GETCURSEL, 0, 0)) - 1;					
				}
				break;

			case IDC_DBG_AMBDIR:
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					ambdir = int(SendDlgItemMessage(hDlg, IDC_DBG_AMBDIR, CB_GETCURSEL, 0, 0)) - 1;
				}
				break;

			case IDC_DBG_ENVMAP:
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					dbgdsp = DbgDisplay(SendDlgItemMessage(hDlg, IDC_DBG_ENVMAP, CB_GETCURSEL, 0, 0));
				}
				break;

			case IDC_DBG_DISPLAY:
				if (HIWORD(wParam)==CBN_SELCHANGE) dspMode = DWORD(SendDlgItemMessage(hWnd, IDC_DBG_DISPLAY, CB_GETCURSEL, 0, 0));
				break;

			case IDC_DBG_CAMERA:
				if (HIWORD(wParam)==CBN_SELCHANGE) camMode = DWORD(SendDlgItemMessage(hWnd, IDC_DBG_CAMERA, CB_GETCURSEL, 0, 0));
				break;

			case IDC_DBG_MSHUP: 
				if (HIWORD(wParam)==BN_CLICKED) sMesh--;
				SetupMeshGroups();
				break;

			case IDC_DBG_MSHDN: 
				if (HIWORD(wParam)==BN_CLICKED) sMesh++;
				SetupMeshGroups();
				break;

			case IDC_DBG_GRPUP: 
				if (HIWORD(wParam)==BN_CLICKED) sGroup--;
				SetupMeshGroups();
				break;

			case IDC_DBG_GRPDN: 
				if (HIWORD(wParam)==BN_CLICKED) sGroup++;	
				SetupMeshGroups();
				break;

			
			case IDC_DBG_MESH: 
				if (HIWORD(wParam)==EN_KILLFOCUS) {
					char cbuf[32];
					GetWindowText(GetDlgItem(hWnd, IDC_DBG_MESH),  cbuf, 32); 
					for (int i=0; i<32;i++) if (cbuf[i]==0 || cbuf[i]=='/') { cbuf[i]=0; break; }
					sMesh = atoi(cbuf);
					SetupMeshGroups();
				}
				break;

			case IDC_DBG_GROUP: 
				if (HIWORD(wParam)==EN_KILLFOCUS) {
					char cbuf[32];
					GetWindowText(GetDlgItem(hWnd, IDC_DBG_GROUP),  cbuf, 32);
					for (int i=0; i<32;i++) if (cbuf[i]==0 || cbuf[i]=='/') { cbuf[i]=0; break; }
					sGroup = atoi(cbuf);
					SetupMeshGroups();
				}
				break;

			case IDC_DBG_OPEN:
				bPaused = oapiGetPause();
				oapiSetPause(true);
				if (GetOpenFileNameA(&OpenTex)) {
					SetWindowText(GetDlgItem(hWnd, IDC_DBG_FILE), OpenTex.lpstrFile);
				}
				oapiSetPause(bPaused);
				break;

			case IDC_DBG_EXECUTE:
				bPaused = oapiGetPause();
				oapiSetPause(true);
				if (Execute(hWnd, &OpenTex)==false) MessageBox(hWnd,"Failed :(","D3D9 Controls", MB_OK);
				oapiSetPause(bPaused);
				break;

			case IDC_DBG_MESHSAVE:
				if (dbgMsh[hSelMesh]) {
					bPaused = oapiGetPause();
					oapiSetPause(true);
					strcpy(SaveMesh.lpstrFileTitle, hSelMesh->GetName());
					SaveMesh.hwndOwner = hWnd;
					if (GetSaveFileName(&SaveMesh)) {
						dbgMsh[hSelMesh]->Save(SaveMesh.lpstrFile);
						return true;
					}
					oapiSetPause(bPaused);
				}
				break;

			case IDC_DBG_ACTION:
				break;

			case IDC_DBG_MORE:
				GetWindowRect(hDlg, &rect);
				SetWindowPos(hDlg, NULL, rect.left, rect.top, isOpen ?   298 : origwidth, rect.bottom - rect.top, SWP_SHOWWINDOW);
				SetWindowText(GetDlgItem(hWnd, IDC_DBG_MORE), isOpen ? ">>>" : "<<<");
				isOpen = !isOpen;
				break;

			case IDC_DBG_RELOADSHD:
				D3D9Effect::D3D9TechInit(g_client, g_client->GetDevice(), "D3D9Client");
				break;

			case IDC_DBG_RELOADTEX:
				if (vObj) {
					if (vObj->Type() == OBJTP_VESSEL) {
						((vVessel *)vObj)->ReloadTextures();
					}
				}
				break;

			case IDC_DBG_ENVSAVE:
				SaveEnvMap();
				break;

			case IDC_DBG_EXTEND:
				SetColorSlider();
				break;

			case IDC_DBG_ISVCMESH:
			case IDC_DBG_VCSHADOW:
				UpdateMeshFlags();
				break;

			case IDC_DBG_GRPO:
			case IDC_DBG_VISO:
			case IDC_DBG_MSHO:
			case IDC_DBG_BOXES:
			case IDC_DBG_SPHERES:
			case IDC_DBG_HSM:
			case IDC_DBG_HSG:
			case IDC_DBG_AMBIENT:
			case IDC_DBG_WIRE:
			case IDC_DBG_DUAL:
			case IDC_DBG_PICK:
			case IDC_DBG_FPSLIM:
			case IDC_DBG_TILEBB:
			case IDC_DBG_CLIPDIST:
			case IDC_DBG_EXTVC:
			case IDC_DBG_PICKCURRENT:
			case IDC_DBG_NOSUNAMB:
			case IDC_DBG_NOPLNAMB:
			case IDC_DBG_NODYNSUN:
			case IDC_DBG_VCZONES:
				UpdateFlags();
				break;

			case IDC_DBG_NOSHADOW:
			case IDC_DBG_NORENDER:
			case IDC_DBG_NOLIGHT:
			case IDC_DBG_ADDITIVE:
			case IDC_DBG_NOCOLOR:
			case IDC_DBG_OIT:
			case IDC_DBG_DYNAMIC:
				if (HIWORD(wParam) == BN_CLICKED) {
					ValidateGroup(sGroup);
				}
				break;

			case IDC_DBG_MATIDX:
			case IDC_DBG_TEXIDX:					
			case IDC_DBG_GRPLABEL:
				if (HIWORD(wParam) == EN_KILLFOCUS) {
					ValidateGroup(sGroup);
				}
				break;

			case IDC_DBG_VARA:
			case IDC_DBG_VARB:
			case IDC_DBG_VARC:
			case IDC_DBG_FILE:
				break;
		
			default: 
				//LogErr("LOWORD(%hu), HIWORD(0x%hX)",LOWORD(wParam),HIWORD(wParam));
				break;
		}
		break;
	}

	return oapiDefDialogProc(hWnd, uMsg, wParam, lParam);
}






// =============================================================================================
//
void CloseGFX()
{
	if (hGfxDlg != NULL) {
		oapiCloseDialog(hGfxDlg);
		hGfxDlg = NULL;
	}
}

// =============================================================================================
//
void OpenGFXDlgClbk(void *context)
{
	HWND l_hDlg = oapiOpenDialog(g_hInst, IDD_GRAPHICS, WndProcGFX);
	if (l_hDlg) hGfxDlg = l_hDlg; // otherwise open already
	else return;

	// slider
	SendDlgItemMessage(hGfxDlg, IDC_GFX_INTENSITY, TBM_SETRANGEMAX, 1, 255);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_INTENSITY, TBM_SETRANGEMIN, 1, 0);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_INTENSITY, TBM_SETTICFREQ, 1, 0);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_INTENSITY, TBM_SETPOS, 1, 0);

	// slider
	SendDlgItemMessage(hGfxDlg, IDC_GFX_DISTANCE, TBM_SETRANGEMAX, 1, 255);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_DISTANCE, TBM_SETRANGEMIN, 1, 0);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_DISTANCE, TBM_SETTICFREQ, 1, 0);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_DISTANCE, TBM_SETPOS, 1, 0);

	// slider
	SendDlgItemMessage(hGfxDlg, IDC_GFX_THRESHOLD, TBM_SETRANGEMAX, 1, 255);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_THRESHOLD, TBM_SETRANGEMIN, 1, 0);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_THRESHOLD, TBM_SETTICFREQ, 1, 0);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_THRESHOLD, TBM_SETPOS, 1, 0);

	// slider
	SendDlgItemMessage(hGfxDlg, IDC_GFX_GAMMA, TBM_SETRANGEMAX, 1, 255);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_GAMMA, TBM_SETRANGEMIN, 1, 0);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_GAMMA, TBM_SETTICFREQ, 1, 0);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_GAMMA, TBM_SETPOS, 1, 0);

	// slider
	SendDlgItemMessage(hGfxDlg, IDC_GFX_SUNLIGHT, TBM_SETRANGEMAX, 1, 255);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_SUNLIGHT, TBM_SETRANGEMIN, 1, 0);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_SUNLIGHT, TBM_SETTICFREQ, 1, 0);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_SUNLIGHT, TBM_SETPOS, 1, 0);

	// slider
	SendDlgItemMessage(hGfxDlg, IDC_GFX_IRRADIANCE, TBM_SETRANGEMAX, 1, 255);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_IRRADIANCE, TBM_SETRANGEMIN, 1, 0);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_IRRADIANCE, TBM_SETTICFREQ, 1, 0);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_IRRADIANCE, TBM_SETPOS, 1, 0);

	// slider
	SendDlgItemMessage(hGfxDlg, IDC_GFX_LOCALMAX, TBM_SETRANGEMAX, 1, 255);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_LOCALMAX, TBM_SETRANGEMIN, 1, 0);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_LOCALMAX, TBM_SETTICFREQ, 1, 0);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_LOCALMAX, TBM_SETPOS, 1, 0);

	// slider
	SendDlgItemMessage(hGfxDlg, IDC_GFX_GLARE, TBM_SETRANGEMAX, 1, 255);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_GLARE, TBM_SETRANGEMIN, 1, 0);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_GLARE, TBM_SETTICFREQ, 1, 0);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_GLARE, TBM_SETPOS, 1, 0);

	

	// reset-button(s)
	HANDLE hImg = LoadImage(g_hInst, MAKEINTRESOURCE(IDB_BITMAP1), IMAGE_BITMAP, 0, 0, LR_LOADTRANSPARENT | LR_DEFAULTSIZE);
	SendMessage(GetDlgItem(hGfxDlg, IDC_GFX_INTENSITY_RESET), BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hImg);
	SendMessage(GetDlgItem(hGfxDlg, IDC_GFX_DISTANCE_RESET), BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hImg);
	SendMessage(GetDlgItem(hGfxDlg, IDC_GFX_THRESHOLD_RESET), BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hImg);
	SendMessage(GetDlgItem(hGfxDlg, IDC_GFX_GAMMA_RESET), BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hImg);

	SendMessage(GetDlgItem(hGfxDlg, IDC_GFX_SUNLIGHT_RESET), BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hImg);
	SendMessage(GetDlgItem(hGfxDlg, IDC_GFX_IRRADIANCE_RESET), BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hImg);
	SendMessage(GetDlgItem(hGfxDlg, IDC_GFX_LOCALMAX_RESET), BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hImg);
	SendMessage(GetDlgItem(hGfxDlg, IDC_GFX_GLARE_RESET), BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hImg);

	CreateToolTip(IDC_GFX_INTENSITY_RESET,   hGfxDlg, (char*)"Reset to default");
	CreateToolTip(IDC_GFX_DISTANCE_RESET,    hGfxDlg, (char*)"Reset to default");
	CreateToolTip(IDC_GFX_THRESHOLD_RESET,	 hGfxDlg, (char*)"Reset to default");
	CreateToolTip(IDC_GFX_GAMMA_RESET,       hGfxDlg, (char*)"Reset to default");
	CreateToolTip(IDC_GFX_SUNLIGHT_RESET,	 hGfxDlg, (char*)"Reset to default");
	CreateToolTip(IDC_GFX_IRRADIANCE_RESET,	 hGfxDlg, (char*)"Reset to default");
	CreateToolTip(IDC_GFX_LOCALMAX_RESET,	 hGfxDlg, (char*)"Reset to default");
	CreateToolTip(IDC_GFX_GLARE_RESET,		 hGfxDlg, (char*)"Reset to default");

	SetGFXSliders();
}

void SetGFXSliders()
{
	char lbl[32];
	double fpos;

	fpos = Config->GFXIntensity;
	sprintf_s(lbl, 32, "%1.2f", fpos);
	SetWindowTextA(GetDlgItem(hGfxDlg, IDC_GFX_VAL1), lbl);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_INTENSITY, TBM_SETPOS, 1, WORD(fpos*255.0));

	fpos = Config->GFXDistance;
	sprintf_s(lbl, 32, "%1.2f", fpos);
	SetWindowTextA(GetDlgItem(hGfxDlg, IDC_GFX_VAL2), lbl);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_DISTANCE, TBM_SETPOS, 1, WORD(fpos*255.0));

	fpos = Config->GFXThreshold;
	sprintf_s(lbl, 32, "%1.2f", fpos);
	SetWindowTextA(GetDlgItem(hGfxDlg, IDC_GFX_VAL3), lbl);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_THRESHOLD, TBM_SETPOS, 1, WORD(fpos*255.0 / 2.0));

	fpos = Config->GFXGamma;
	sprintf_s(lbl, 32, "%1.2f", fpos);
	SetWindowTextA(GetDlgItem(hGfxDlg, IDC_GFX_VAL4), lbl);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_GAMMA, TBM_SETPOS, 1, WORD(fpos*255.0 / 2.5));

	fpos = Config->GFXSunIntensity;
	sprintf_s(lbl, 32, "%1.2f", fpos);
	SetWindowTextA(GetDlgItem(hGfxDlg, IDC_GFX_VAL5), lbl);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_SUNLIGHT, TBM_SETPOS, 1, WORD(fpos*255.0 / 2.0));

	fpos = Config->PlanetGlow;
	sprintf_s(lbl, 32, "%1.2f", fpos);
	SetWindowTextA(GetDlgItem(hGfxDlg, IDC_GFX_VAL6), lbl);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_IRRADIANCE, TBM_SETPOS, 1, WORD(fpos*255.0 / 2.0));

	fpos = Config->GFXLocalMax;
	sprintf_s(lbl, 32, "%1.2f", fpos);
	SetWindowTextA(GetDlgItem(hGfxDlg, IDC_GFX_VAL7), lbl);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_LOCALMAX, TBM_SETPOS, 1, WORD(fpos*255.0));

	fpos = Config->GFXGlare;
	sprintf_s(lbl, 32, "%1.2f", fpos);
	SetWindowTextA(GetDlgItem(hGfxDlg, IDC_GFX_VAL8), lbl);
	SendDlgItemMessage(hGfxDlg, IDC_GFX_GLARE, TBM_SETPOS, 1, WORD(fpos * 255.0));
}

void ReadGFXSliders()
{
	char lbl[32];
	double fpos;

	fpos = (1.0 / 255.0) * double(SendDlgItemMessage(hGfxDlg, IDC_GFX_INTENSITY, TBM_GETPOS, 0, 0));
	sprintf_s(lbl, 32, "%1.2f", fpos);
	SetWindowTextA(GetDlgItem(hGfxDlg, IDC_GFX_VAL1), lbl);
	Config->GFXIntensity = fpos;
	
	fpos = (1.0 / 255.0) * double(SendDlgItemMessage(hGfxDlg, IDC_GFX_DISTANCE, TBM_GETPOS, 0, 0));
	sprintf_s(lbl, 32, "%1.2f", fpos);
	SetWindowTextA(GetDlgItem(hGfxDlg, IDC_GFX_VAL2), lbl);
	Config->GFXDistance = fpos;
	
	fpos = (2.0 / 255.0) * double(SendDlgItemMessage(hGfxDlg, IDC_GFX_THRESHOLD, TBM_GETPOS, 0, 0));
	sprintf_s(lbl, 32, "%1.2f", fpos);
	SetWindowTextA(GetDlgItem(hGfxDlg, IDC_GFX_VAL3), lbl);
	Config->GFXThreshold = fpos;

	fpos = (2.5 / 255.0) * double(SendDlgItemMessage(hGfxDlg, IDC_GFX_GAMMA, TBM_GETPOS, 0, 0));
	sprintf_s(lbl, 32, "%1.2f", fpos);
	SetWindowTextA(GetDlgItem(hGfxDlg, IDC_GFX_VAL4), lbl);
	Config->GFXGamma = fpos;	

	fpos = (2.0 / 255.0) * double(SendDlgItemMessage(hGfxDlg, IDC_GFX_SUNLIGHT, TBM_GETPOS, 0, 0));
	sprintf_s(lbl, 32, "%1.2f", fpos);
	SetWindowTextA(GetDlgItem(hGfxDlg, IDC_GFX_VAL5), lbl);
	Config->GFXSunIntensity = fpos;

	fpos = (2.0 / 255.0) * double(SendDlgItemMessage(hGfxDlg, IDC_GFX_IRRADIANCE, TBM_GETPOS, 0, 0));
	sprintf_s(lbl, 32, "%1.2f", fpos);
	SetWindowTextA(GetDlgItem(hGfxDlg, IDC_GFX_VAL6), lbl);
	Config->PlanetGlow = fpos;

	fpos = (1.0 / 255.0) * double(SendDlgItemMessage(hGfxDlg, IDC_GFX_LOCALMAX, TBM_GETPOS, 0, 0));
	sprintf_s(lbl, 32, "%1.2f", fpos);
	SetWindowTextA(GetDlgItem(hGfxDlg, IDC_GFX_VAL7), lbl);
	Config->GFXLocalMax = fpos;

	fpos = (1.0 / 255.0) * double(SendDlgItemMessage(hGfxDlg, IDC_GFX_GLARE, TBM_GETPOS, 0, 0));
	sprintf_s(lbl, 32, "%1.2f", fpos);
	SetWindowTextA(GetDlgItem(hGfxDlg, IDC_GFX_VAL8), lbl);
	Config->GFXGlare = fpos;
}

INT_PTR CALLBACK WndProcGFX(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {

	case WM_INITDIALOG:
	{
		return TRUE;	// All Init actions are done in OpenDlgClbk();
	}

	case WM_HSCROLL:
	{
		if (LOWORD(wParam) == TB_THUMBTRACK || LOWORD(wParam) == TB_ENDTRACK) {
			if (HWND(lParam) == GetDlgItem(hWnd, IDC_GFX_INTENSITY)) ReadGFXSliders();
			if (HWND(lParam) == GetDlgItem(hWnd, IDC_GFX_DISTANCE)) ReadGFXSliders();
			if (HWND(lParam) == GetDlgItem(hWnd, IDC_GFX_THRESHOLD)) ReadGFXSliders();
			if (HWND(lParam) == GetDlgItem(hWnd, IDC_GFX_GAMMA)) ReadGFXSliders();
			if (HWND(lParam) == GetDlgItem(hWnd, IDC_GFX_SUNLIGHT)) ReadGFXSliders();
			if (HWND(lParam) == GetDlgItem(hWnd, IDC_GFX_IRRADIANCE)) ReadGFXSliders();
			if (HWND(lParam) == GetDlgItem(hWnd, IDC_GFX_LOCALMAX)) ReadGFXSliders();
			if (HWND(lParam) == GetDlgItem(hWnd, IDC_GFX_GLARE)) ReadGFXSliders();
		}
		return false;
	}

	case WM_COMMAND:

		switch (LOWORD(wParam)) {

		case IDCANCEL:
			CloseGFX();
			break;

		case IDC_GFX_RECOMPILE:
			g_client->GetScene()->CreateSunGlare();
			break;


		case IDC_GFX_INTENSITY_RESET:
			Config->GFXIntensity = 0.5;
			SetGFXSliders();
			break;

		case IDC_GFX_DISTANCE_RESET:
			Config->GFXDistance = 0.8;
			SetGFXSliders();
			break;

		case IDC_GFX_THRESHOLD_RESET:
			Config->GFXThreshold = 1.1;
			SetGFXSliders();
			break;

		case IDC_GFX_GAMMA_RESET:
			Config->GFXGamma = 1.0;
			SetGFXSliders();
			break;

		case IDC_GFX_SUNLIGHT_RESET:
			Config->GFXSunIntensity = 1.2;
			SetGFXSliders();
			break;

		case IDC_GFX_IRRADIANCE_RESET:
			Config->PlanetGlow = 1.0;
			SetGFXSliders();
			break;

		case IDC_GFX_LOCALMAX_RESET:
			Config->GFXLocalMax = 0.5;
			SetGFXSliders();
			break;

		case IDC_GFX_GLARE_RESET:
			Config->GFXGlare = 0.3;
			SetGFXSliders();
			break;

		default:
			//LogErr("WndProcGFX() LOWORD(%hu), HIWORD(0x%hX)", LOWORD(wParam), HIWORD(wParam));
			break;
		}
		break;
	}

	return oapiDefDialogProc(hWnd, uMsg, wParam, lParam);
}




} //namespace



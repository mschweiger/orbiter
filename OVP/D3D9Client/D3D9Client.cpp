// ==============================================================
// D3D9Client.cpp
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2006-2016 Martin Schweiger
//				 2012-2016 Jarmo Nikkanen
// ==============================================================


#define STRICT 1
#define ORBITER_MODULE

#include <set> // ...for Brush-, Pen- and Font-accounting
#include "Orbitersdk.h"
#include "D3D9Client.h"
#include "D3D9Config.h"
#include "D3D9Util.h"
#include "D3D9Catalog.h"
#include "D3D9Surface.h"
#include "D3D9TextMgr.h"
#include "D3D9Frame.h"
#include "D3D9Pad.h"
#include "CSphereMgr.h"
#include "Scene.h"
#include "Mesh.h"
#include "VVessel.h"
#include "VStar.h"
#include "MeshMgr.h"
#include "Particle.h"
#include "TileMgr.h"
#include "RingMgr.h"
#include "HazeMgr.h"
#include "Log.h"
#include "VideoTab.h"
#include "GDIPad.h"
#include "OapiExtension.h"
#include "DebugControls.h"
#include "Surfmgr2.h"
#include "gcCore.h"
#include "gcConst.h"
#include <unordered_map>
#include <d3d9on12.h>
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#if defined(_MSC_VER) && (_MSC_VER <= 1700 ) // Microsoft Visual Studio Version 2012 and lower
#define round(v) floor(v+0.5)
#endif

// ==============================================================
// Structure definitions

struct D3D9Client::RenderProcData {
	__gcRenderProc proc;
	void *pParam;
	DWORD id;
};

struct D3D9Client::GenericProcData {
	__gcGenericProc proc;
	void *pParam;
	DWORD id;
};

using namespace oapi;

HINSTANCE g_hInst = 0;
D3D9Client *g_client = 0;
class gcConst* g_pConst = 0;
IDirect3D9* g_pD3DObject = 0;  // Made valid when VideoTab is created
Memgr<float>* g_pMemgr_f = nullptr;
Memgr<INT16>* g_pMemgr_i = nullptr;
Memgr<UINT8>* g_pMemgr_u = nullptr;
Memgr<WORD>* g_pMemgr_w = nullptr;
Memgr<VERTEX_2TEX>* g_pMemgr_vtx = nullptr;
Texmgr<LPDIRECT3DTEXTURE9>* g_pTexmgr_tt = nullptr;
Vtxmgr<LPDIRECT3DVERTEXBUFFER9>* g_pVtxmgr_vb = nullptr;
Idxmgr<LPDIRECT3DINDEXBUFFER9>* g_pIdxmgr_ib = nullptr;

typedef IDirect3D9* (__stdcall* __Direct3DCreate9On12)(UINT SDKVersion, D3D9ON12_ARGS* pOverrideList, UINT NumOverrideEntries);

set<D3D9Mesh*> MeshCatalog;
set<SurfNative*> SurfaceCatalog;
unordered_map<string, SURFHANDLE> SharedTextures;
unordered_map<string, SURFHANDLE> ClonedTextures;
unordered_map<MESHHANDLE, class SketchMesh*> MeshMap;
unordered_map<std::string, LPDIRECT3DTEXTURE9> MicroTextures;

DWORD uCurrentMesh = 0;
vObject *pCurrentVisual = 0;
_D3D9Stats D3D9Stats;

#ifdef _NVAPI_H
 StereoHandle pStereoHandle = 0;
#endif

bool bFreeze = false;
bool bFreezeEnable = false;
bool bFreezeRenderAll = false;

// Debuging Brush-, Pen- and Font-accounting
std::set<Font *> g_fonts;
std::set<Pen *> g_pens;
std::set<Brush *> g_brushes;

extern list<gcGUIApp *> g_gcGUIAppList;

extern "C" {
	_declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}

// ==============================================================
// API interface
// ==============================================================

// ==============================================================
// Initialise module

DLLCLBK void InitModule(HINSTANCE hDLL)
{

#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	// _CrtSetBreakAlloc(8351);

	assert(sizeof(FVECTOR4) == 16);
	assert(sizeof(FVECTOR4::data) == 16);

	assert(sizeof(FVECTOR4::rgb) == 12);
	assert(sizeof(FVECTOR4::xyz) == 12);

	auto dut = FVECTOR4(1.2, 3.4, 5.6, 7.8);
	assert(dut.data[0] == dut.r);
	assert(dut.data[1] == dut.g);
	assert(dut.data[2] == dut.b);
	assert(dut.data[3] == dut.a);

	assert(dut.data[0] == dut.x);
	assert(dut.data[1] == dut.y);
	assert(dut.data[2] == dut.z);
	assert(dut.data[3] == dut.w);

	assert(dut.rgb.x == dut.xyz.x);
	assert(dut.rgb.y == dut.xyz.y);
	assert(dut.rgb.z == dut.xyz.z);

	assert(dut.a == dut.w);

	// Check that 'a' and 'w' are placed behind 'rgb' rsp. 'xyz' and unaffected
	dut.rgb = 0;
	assert(dut.a == dut.w);
#endif

	D3D9InitLog("Modules/D3D9Client/D3D9ClientLog.html");

	g_pMemgr_f = new Memgr<float>("float");
	g_pMemgr_i = new Memgr<INT16>("UINT16");
	g_pMemgr_u = new Memgr<UINT8>("UINT8");
	g_pMemgr_w = new Memgr<WORD>("WORD");
	g_pMemgr_vtx = new Memgr<VERTEX_2TEX>("VERTEX_2TEX");

	if (!D3DXCheckVersion(D3D_SDK_VERSION, D3DX_SDK_VERSION)) {
		MissingRuntimeError();
		return;
	}

#ifdef _NVAPI_H
	if (NvAPI_Initialize()==NVAPI_OK) {
		LogAlw("[nVidia API Initialized]");
		bNVAPI = true;
	}
	else LogAlw("[nVidia API Not Available]");
#else
	LogAlw("[Not Compiled With nVidia API]");
#endif

	Config = new D3D9Config();

	if (Config->ShaderCacheUse) {
		DWORD fa = GetFileAttributesA("Cache");
		if (fa == INVALID_FILE_ATTRIBUTES) CreateDirectoryA("Cache", NULL);
		fa = GetFileAttributesA("Cache/D3D9Client");
		if (fa == INVALID_FILE_ATTRIBUTES) CreateDirectoryA("Cache/D3D9Client", NULL);
		fa = GetFileAttributesA("Cache/D3D9Client/Shaders");
		if (fa == INVALID_FILE_ATTRIBUTES) CreateDirectoryA("Cache/D3D9Client/Shaders", NULL);
	}

	g_pConst = new gcConst();

	DebugControls::Create();
	AtmoControls::Create();
	vPlanet::ParseMicroTexturesFile();

	D3D9Stats.TilesRendered[RENDERPASS_MAINSCENE] = 0;
	D3D9Stats.TilesRendered[RENDERPASS_CUSTOMCAM] = 0;
	D3D9Stats.TilesRendered[RENDERPASS_ENVCAM] = 0;

	g_hInst = hDLL;
	g_client = new D3D9Client(hDLL);

	if (oapiRegisterGraphicsClient(g_client)==false) {
		delete g_client;
		g_client = 0;
	}
}

// ==============================================================
// Clean up module

DLLCLBK void ExitModule(HINSTANCE hDLL)
{
	LogAlw("--------------ExitModule------------");

	delete Config;
	delete g_pConst;

	if (g_client) {
		oapiUnregisterGraphicsClient(g_client);
		delete g_client;
		g_client = 0;
	}

	DebugControls::Release();
	AtmoControls::Release();

#ifdef _NVAPI_H
	if (bNVAPI) if (NvAPI_Unload()==NVAPI_OK) LogAlw("[nVidia API Unloaded]");
#endif

	LogAlw("Log Closed");
	D3D9CloseLog();

	SAFE_DELETE(g_pMemgr_f);
	SAFE_DELETE(g_pMemgr_i);
	SAFE_DELETE(g_pMemgr_u);
	SAFE_DELETE(g_pMemgr_w);
	SAFE_DELETE(g_pMemgr_vtx);
}

DLLCLBK gcGUIBase * gcGetGUICore()
{
	return dynamic_cast<gcGUIBase *>(g_pWM);
}


DLLCLBK gcConst * gcGetCoreAPI()
{
	return dynamic_cast<gcConst *>(g_pConst);
}



// ==============================================================
// D3D9Client class implementation
// ==============================================================

D3D9Client::D3D9Client (HINSTANCE hInstance) :
	GraphicsClient(hInstance),
	vtab(NULL),
	scenarioName("(none selected)"),
	pLoadLabel(""),
	pLoadItem(""),
	pDevice     (NULL),
	pDefaultTex (NULL),
	pScatterTest(NULL),
	pFramework  (NULL),
	pItemsSkp   (NULL),
	hLblFont1   (NULL),
	hLblFont2   (NULL),
	hMainThread (NULL),
	pCaps       (NULL),
	pWM			(NULL),
	pBltSkp		(NULL),
	pBltGrpTgt	(NULL),
	pCustomSplashScreen(NULL),
	pSplashTextColor(0xE0A0A0),
	hRenderWnd(),
	scene     (),
	meshmgr   (),
	bControlPanel (false),
	bScatterUpdate(false),
	bFullscreen   (false),
	bAAEnabled    (false),
	bFailed       (false),
	bRunning      (false),
	bVertexTex    (false),
	bVSync        (false),
	bRendering	  (false),
	bGDIClear	  (true),
	viewW         (0),
	viewH         (0),
	viewBPP       (0),
	frame_timer   (0),
	loadd_x       (0),
	loadd_y       (0),
	loadd_w       (0),
	loadd_h       (0),
	LabelPos      (0)

{
}

// ==============================================================

D3D9Client::~D3D9Client()
{
	LogAlw("D3D9Client destructor called");
	SAFE_DELETE(vtab);
	SAFE_RELEASE(g_pD3DObject);
}


// ==============================================================
// 
bool D3D9Client::ChkDev(const char *fnc) const
{
	if (pDevice) return false;
	LogErr("Call [%s] Failed. D3D9 Graphics services off-line", fnc);
	return true;
}


// ==============================================================
// Overridden
//
const void *D3D9Client::GetConfigParam (DWORD paramtype) const
{
	return (paramtype >= CFGPRM_TILELOADTHREAD)
		 ? (paramtype >= CFGPRM_GETSELECTEDMESH)
		 ? DebugControls::GetConfigParam(paramtype)
		 : OapiExtension::GetConfigParam(paramtype)
		 : GraphicsClient::GetConfigParam(paramtype);
}


// ==============================================================
// This is called only once when the launchpad will appear
// This callback will initialize the Video tab only
//
bool D3D9Client::clbkInitialise()
{
	_TRACE;
	LogAlw("================ clbkInitialise ===============");
	LogAlw("Orbiter Version = %d",oapiGetOrbiterVersion());

	D3D9ON12_ARGS args = {};
	args.Enable9On12 = Config->Enable9On12 != 0;

	HMODULE hDXMod = GetModuleHandle("D3D9.dll");
	__Direct3DCreate9On12 pDirect3DCreate9On12 = nullptr;

	if (hDXMod) pDirect3DCreate9On12 = (__Direct3DCreate9On12)GetProcAddress(hDXMod, "Direct3DCreate9On12");

	if (OapiExtension::RunsUnderWINE() || pDirect3DCreate9On12 == nullptr) {
		g_pD3DObject = Direct3DCreate9(D3D_SDK_VERSION);
		oapiWriteLog("[D3D9] Native Interface");
	}
	else {
		g_pD3DObject = pDirect3DCreate9On12(D3D_SDK_VERSION, &args, 1);
		if (Config->Enable9On12) oapiWriteLog("[D3D9] DX9 emulation via DX12");
		else oapiWriteLog("[D3D9] Native Interface");
	}

	if (g_pD3DObject) oapiWriteLog("[D3D9] DirectX9 Created...");
	else {
		oapiWriteLog("[D3D9][ERROR] Failed to create DirectX9");
		FailedDeviceError();
		return false;
	}

	// Perform default setup
	if (GraphicsClient::clbkInitialise()==false) return false;

	//Create the Launchpad video tab interface
	oapiWriteLog("[D3D9] Initialize VideoTab...");
	vtab = new VideoTab(this, ModuleInstance(), OrbiterInstance(), LaunchpadVideoTab());
	return vtab->Initialise();
}


// ==============================================================
// This is called when a simulation session will begin
//
HWND D3D9Client::clbkCreateRenderWindow()
{
	_TRACE;

	LogAlw("================ clbkCreateRenderWindow ===============");

	if (!g_pD3DObject) return NULL;

	Config->WriteParams();
	
	uEnableLog		 = Config->DebugLvl;
	pSplashScreen    = NULL;
	pBackBuffer      = NULL;
	pTextScreen      = NULL;
	hRenderWnd       = NULL;
	pDefaultTex		 = NULL;
	hLblFont1		 = NULL;
	hLblFont2		 = NULL;
	bControlPanel    = false;
	bFullscreen      = false;
	bFailed			 = false;
	bRunning		 = false;
	bVertexTex		 = false;
	viewW = viewH    = 0;
	viewBPP          = 0;
	frame_timer		 = 0;
	scene            = NULL;
	meshmgr          = NULL;
	pFramework       = NULL;
	pDevice			 = NULL;
	pBltGrpTgt		 = NULL;	// Let's set this NULL here, constructor is called only once. Not when exiting and restarting a simulation.
	pNoiseTex		 = NULL;
	surfBltTgt		 = NULL;	// This variable is not used, set it to NULL anyway
	hMainThread		 = GetCurrentThread();

	D3DXMatrixIdentity(&ident);

	oapiDebugString()[0] = '\0';

	MeshCatalog.clear();
	SurfaceCatalog.clear();

	hRenderWnd = GraphicsClient::clbkCreateRenderWindow();

	LogAlw("Window Handle = %s",_PTR(hRenderWnd));
	SetWindowText(hRenderWnd, "[D3D9Client]");

	LogOk("Starting to initialize device and 3D environment...");

	pFramework = new CD3DFramework9();

	WriteLog("[DirectX 9 Initialized]");

	HRESULT hr = pFramework->Initialize(hRenderWnd, GetVideoData());

	if (hr!=S_OK) {
		LogErr("ERROR: Failed to initialize 3D Framework");
		return NULL;
	}

	RECT rect;
	GetClientRect(hRenderWnd, &rect);
	HDC hWnd = GetDC(hRenderWnd);
	HBRUSH hBr = CreateSolidBrush(RGB(0,0,0));
	FillRect(hWnd, &rect, hBr);
	DeleteObject(hBr);
	ReleaseDC(hRenderWnd, hWnd);
	ValidateRect(hRenderWnd, NULL);	// avoids white flash after splash screen

	pCaps = pFramework->GetCaps();

	WriteLog("[3DDevice Initialized]");

	pDevice		= pFramework->GetD3DDevice();
	viewW		= pFramework->GetWidth();
	viewH		= pFramework->GetHeight();
	bFullscreen = (pFramework->IsFullscreen() == TRUE);
	bAAEnabled  = (pFramework->IsAAEnabled() == TRUE);
	viewBPP		= 32;
	bVertexTex  = (pFramework->HasVertexTextureSup() == TRUE);
	bVSync		= (pFramework->GetVSync() == TRUE);

	char fld[] = "D3D9Client";

	g_pTexmgr_tt = new Texmgr<LPDIRECT3DTEXTURE9>(pDevice, "TileTextures");
	g_pVtxmgr_vb = new Vtxmgr<LPDIRECT3DVERTEXBUFFER9>(pDevice, "TileVertex");
	g_pIdxmgr_ib = new Idxmgr<LPDIRECT3DINDEXBUFFER9>(pDevice, "TileIndices");

	HR(D3DXCreateTextureFromFileA(pDevice, "Textures/D3D9Noise.dds", &pNoiseTex));

	HR(pDevice->GetRenderTarget(0, &pBackBuffer));
	HR(pDevice->GetDepthStencilSurface(&pDepthStencil));

	LogAlw("Render Target = %s", _PTR(pBackBuffer));
	LogAlw("DepthStencil = %s", _PTR(pDepthStencil));

	meshmgr		= new MeshManager(this);

	// Bring Sketchpad Online
	D3D9PadFont::D3D9TechInit(pDevice);
	D3D9PadPen::D3D9TechInit(pDevice);
	D3D9PadBrush::D3D9TechInit(pDevice);
	D3D9Text::D3D9TechInit(this, pDevice);
	D3D9Pad::D3D9TechInit(this, pDevice);

	deffont = (oapi::Font*) new D3D9PadFont(20, true, "fixed");
	defpen  = (oapi::Pen*)  new D3D9PadPen(1, 1, 0x00FF00);

	pDefaultTex = SURFACE(clbkLoadTexture("Null.dds"));
	if (pDefaultTex==NULL) LogErr("Null.dds not found");

	int x=0;
	if (viewW>1282) x=4;

	hLblFont1 = CreateFont(24+x, 0, 0, 0, 700, false, false, 0, 0, 3, 2, 1, 49, "Courier New");
	hLblFont2 = CreateFont(18+x, 0, 0, 0, 700, false, false, 0, 0, 3, 2, 1, 49, "Courier New");

	SplashScreen();  // Warning SurfNative is not yet fully initialized here

	ShowWindow(hRenderWnd, SW_SHOW);

	OutputLoadStatus("Building Shader Programs...",0);

	D3D9Effect::D3D9TechInit(this, pDevice, fld);

	// Device-specific initialisations

	TileManager::GlobalInit(this);
	TileManager2Base::GlobalInit(this);
	RingManager::GlobalInit(this);
	HazeManager::GlobalInit(this);
	HazeManager2::GlobalInit(this);
	D3D9ParticleStream::GlobalInit(this);
	CSphereManager::GlobalInit(this);
	vStar::GlobalInit(this);
	vObject::GlobalInit(this);
	vVessel::GlobalInit(this);
	vPlanet::GlobalInit(this);
	OapiExtension::GlobalInit(*Config);

	OutputLoadStatus("SceneTech.fx",1);
	Scene::D3D9TechInit(pDevice, fld);
	D3D9Mesh::GlobalInit(pDevice);

	// Create scene instance
	scene = new Scene(this, viewW, viewH);

	WriteLog("[D3D9Client Initialized]");
	LogOk("...3D environment initialised");

#ifdef _NVAPI_H
	if (bNVAPI) {
		NvU8 bEnabled = 0;
		NvAPI_Status nvStereo = NvAPI_Stereo_IsEnabled(&bEnabled);

		if (nvStereo!=NVAPI_OK) {
			if (nvStereo==NVAPI_STEREO_NOT_INITIALIZED) LogWrn("Stereo API not initialized");
			if (nvStereo==NVAPI_API_NOT_INITIALIZED) LogErr("nVidia API not initialized");
			if (nvStereo==NVAPI_ERROR) LogErr("nVidia API ERROR");
		}

		if (bEnabled) {
			LogAlw("[nVidia Stereo mode is Enabled]");
			if (NvAPI_Stereo_CreateHandleFromIUnknown(pDevice, &pStereoHandle)!=NVAPI_OK) {
				LogErr("Failed to get StereoHandle");
			}
			else {
				if (pStereoHandle) {
					if (NvAPI_Stereo_SetConvergence(pStereoHandle, float(Config->Convergence))!=NVAPI_OK) {
						LogErr("SetConvergence Failed");
					}
					if (NvAPI_Stereo_SetSeparation(pStereoHandle, float(Config->Separation))!=NVAPI_OK) {
						LogErr("SetSeparation Failed");
					}
				}
			}
		}
		else LogAlw("[nVidia Stereo mode is Disabled]");
	}
#endif

	// Create status queries -----------------------------------------
	//
	if (pDevice->CreateQuery(D3DQUERYTYPE_OCCLUSION, NULL) == S_OK) LogAlw("D3DQUERYTYPE_OCCLUSION is supported by device");
	else LogAlw("D3DQUERYTYPE_OCCLUSION not supported by device");

	if (pDevice->CreateQuery(D3DQUERYTYPE_PIPELINETIMINGS, NULL)==S_OK) LogAlw("D3DQUERYTYPE_PIPELINETIMINGS is supported by device");
	else LogAlw("D3DQUERYTYPE_PIPELINETIMINGS not supported by device");

	if (pDevice->CreateQuery(D3DQUERYTYPE_BANDWIDTHTIMINGS, NULL) == S_OK) LogAlw("D3DQUERYTYPE_BANDWIDTHTIMINGS is supported by device");
	else LogAlw("D3DQUERYTYPE_BANDWIDTHTIMINGS not supported by device");

	if (pDevice->CreateQuery(D3DQUERYTYPE_PIXELTIMINGS, NULL) == S_OK) LogAlw("D3DQUERYTYPE_PIXELTIMINGS is supported by device");
	else LogAlw("D3DQUERYTYPE_PIXELTIMINGS not supported by device");

	return hRenderWnd;
}


// ==============================================================
// This is called when the simulation is ready to go but the clock
// is not yet ticking
//
void D3D9Client::clbkPostCreation()
{
	_TRACE;
	LogAlw("================ clbkPostCreation ===============");

	if (scene) scene->Initialise();

	// Create Window Manager -----------------------------------------
	//
	if (Config->gcGUIMode != 0) {
		pWM = new WindowManager(hRenderWnd, ModuleInstance(), !(GetVideoData()->fullscreen));
		if (pWM->IsOK() == false) SAFE_DELETE(pWM);
	}

	bRunning = true;

	LogAlw("=============== Loading Completed and Visuals Created ================");

#ifdef _DEBUG
	SketchPadTest();
#endif

	WriteLog("[Scene Initialized]");
}

// ==============================================================
// Perform some routine tests with sketchpad
//
void D3D9Client::SketchPadTest()
{
	SURFHANDLE hSrc = clbkLoadSurface("D3D9/SketchpadTest.dds", OAPISURFACE_TEXTURE);
	SURFHANDLE hTgt = clbkLoadSurface("D3D9/SketchpadTest.dds", OAPISURFACE_RENDERTARGET);

	if (!hSrc || !hTgt) return;

	oapiSetSurfaceColourKey(hSrc, 0xFFFFFFFF);

	Sketchpad *pSkp = oapiGetSketchpad(hTgt);

	RECT r = { 17, 1, 31, 15 };
	RECT t = { 1, 33, 31, 63 };
	RECT q = { 0, 16, 16, 32 };
	RECT w = { 49, 1, 63, 15 };

	pSkp->CopyRect(hSrc, &r, 1, 1);
	pSkp->ColorKey(hSrc, &r, 33, 1);
	pSkp->SetBlendState((Sketchpad::BlendState)(Sketchpad::BlendState::ALPHABLEND | Sketchpad::BlendState::FILTER_POINT));
	pSkp->StretchRect(hSrc, &r, &t);
	pSkp->SetBlendState();
	pSkp->StretchRect(hSrc, &r, &w);
	pSkp->RotateRect(hSrc, &q, 24, 24, float(PI05));
	pSkp->RotateRect(hSrc, &q, 40, 24, float(PI));
	pSkp->RotateRect(hSrc, &q, 56, 24, float(-PI05));

	RECT tr = { 33, 49, 47, 63 };
	pSkp->ColorFill(0xFFFF00FF, &tr);

	pSkp->QuickPen(0xFF000000);
	pSkp->QuickBrush(0x80FFFF00);
	pSkp->Rectangle(49, 33, 63, 47); // 1px margin
	pSkp->Ellipse(49, 49, 63, 63);	// 1px margin

									// No Pen, Brush Only
	pSkp->SetPen(NULL);
	pSkp->QuickBrush(0xFF00FF00);
	pSkp->Rectangle(65, 33, 79, 47); // 1px tlm 1px brm
	pSkp->Ellipse(65, 49, 79, 63);	// 1px tlm 1px brm

	pSkp->QuickPen(0xFFFFFFFF);
	pSkp->MoveTo(65, 1);
	pSkp->LineTo(65, 14);
	pSkp->LineTo(78, 14);
	pSkp->LineTo(78, 1);
	pSkp->LineTo(65, 1);

	RECT cr = { 33, 33, 47, 47 };
	pSkp->ClipRect(&cr);
	pSkp->ColorFill(0xFFFFFF00, NULL);
	pSkp->ClipRect();

	oapiReleaseSketchpad(pSkp);

	oapiSaveSurface("SketchpadOutput", hTgt, ImageFileFormat::IMAGE_PNG);

	oapiReleaseTexture(hSrc);
	oapiReleaseTexture(hTgt);

	 
	// Run Different Kind of Tests
	// 

	hTgt = oapiCreateSurfaceEx(768, 512, OAPISURFACE_RENDERTARGET);

	if (!hTgt) return;

	gcCore2* pCore = gcGetCoreInterface();

	if (!pCore) return;

	float a = 0.0f;
	float s = float(PI2 / 6.0);

	gcCore::clrVtx Vtx[8];
	oapi::FVECTOR2 Pol[6];

	for (int i = 0; i < 6; i++) {
		Pol[i].x = cos(a);
		Pol[i].y = sin(a);
		Vtx[i + 1].pos.x = Pol[i].x;
		Vtx[i + 1].pos.y = Pol[i].y;
		a += s;
	}

	//				 AABBGGRR
	Vtx[1].color = 0xFFFF0000;
	Vtx[2].color = 0xFFFFFF00;
	Vtx[3].color = 0xFF00FF00;
	Vtx[4].color = 0xFF00FFFF;
	Vtx[5].color = 0xFF0000FF;
	Vtx[6].color = 0xFFFF00FF;

	// Center vertex
	Vtx[0].pos.x = 0.0f;
	Vtx[0].pos.y = 0.0f;
	Vtx[0].color = 0xFFFFFFFF;    // White
	Vtx[7] = Vtx[1];

	HPOLY hColors = pCore->CreateTriangles(NULL, Vtx, 8, PF_FAN);
	HPOLY hOutline = pCore->CreatePoly(NULL, Pol, 6, PF_CONNECT);
	HPOLY hOutline2 = pCore->CreatePoly(NULL, Pol, 6);

	Vtx[0].color = 0xFFFF0000;    
	Vtx[1].color = 0xFFFFFF00;	  
	Vtx[2].color = 0xFFF0FF00;   
	Vtx[3].color = 0xFF00FFFF;	
	Vtx[4].color = 0xFF0000FF;    
	Vtx[5].color = 0xFFFF00FF;

	Vtx[0].pos = FVECTOR2(-1, 0);
	Vtx[1].pos = FVECTOR2(-1, 1);
	Vtx[2].pos = FVECTOR2(0, 0);
	Vtx[3].pos = FVECTOR2(0, 1);
	Vtx[4].pos = FVECTOR2(1, 0);
	Vtx[5].pos = FVECTOR2(1, 1);

	HPOLY hStrip = pCore->CreateTriangles(NULL, Vtx, 6, PF_STRIP);


	Vtx[0].color = 0xFF00FF00;    // Green
	Vtx[1].color = 0xFF00FF00;	  // Green	
	Vtx[2].color = 0xFFFF00FF;    // Mangenta
	Vtx[3].color = 0xFFFF00FF;	  // Mangenta
	Vtx[4].color = 0xFF0000FF;    // Blue
	Vtx[5].color = 0xFF0000FF;	  // Blue

	Vtx[0].pos = FVECTOR2(-1, 1);
	Vtx[1].pos = FVECTOR2(-1, 0);
	Vtx[2].pos = FVECTOR2(0, 1);
	Vtx[3].pos = FVECTOR2(0, 0);
	Vtx[4].pos = FVECTOR2(1, 1);
	Vtx[5].pos = FVECTOR2(1, 0);

	HPOLY hStrip2 = pCore->CreateTriangles(NULL, Vtx, 6, PF_STRIP);


	IVECTOR2 pos0 = { 128, 128 };
	IVECTOR2 pos1 = { 128, 384 };
	IVECTOR2 pos2 = { 384, 128 };
	IVECTOR2 pos3 = { 640, 128 };
	IVECTOR2 pos4 = { 384, 384 };
	IVECTOR2 pos5 = { 640, 384 };

	pSkp = oapiGetSketchpad(hTgt);

	pSkp->ColorFill(0xFFFFFFFF, NULL);

	pSkp->QuickBrush(0xA0000088);
	pSkp->QuickPen(0xA0000000, 3.0f);
	pSkp->PushWorldTransform();

	pSkp->SetWorldScaleTransform2D(ptr(FVECTOR2(100.0f, 100.0f)), &pos0);
	pSkp->DrawPoly(hColors);
	pSkp->DrawPoly(hOutline);

	pSkp->SetWorldScaleTransform2D(ptr(FVECTOR2(100.0f, 100.0f)), &pos1);
	pSkp->DrawPoly(hOutline2);

	pSkp->SetWorldScaleTransform2D(ptr(FVECTOR2(100.0f, 100.0f)), &pos2);
	pSkp->DrawPoly(hStrip);

	pSkp->SetWorldScaleTransform2D(ptr(FVECTOR2(100.0f, 100.0f)), &pos3);
	pSkp->DrawPoly(hStrip2);

	pSkp->SetWorldScaleTransform2D(ptr(FVECTOR2(100.0f, 100.0f)), &pos4);
	pSkp->QuickPen(0xFF000000, 25.0f);
	pSkp->DrawPoly(hOutline);

	hSrc = clbkLoadSurface("generic/noisep.dds", OAPISURFACE_TEXTURE);

	pSkp->SetWorldScaleTransform2D(ptr(FVECTOR2(1.0f, 1.0f)), &pos5);

	FVECTOR2 pt[4];
	pt[0] = FVECTOR2(-100.0f, -100.0f);
	pt[1] = FVECTOR2(-100.0f, 50.0f);
	pt[2] = FVECTOR2(100.0f, 100.0f);
	pt[3] = FVECTOR2(100.0f, -50.0f);

	pSkp->CopyTetragon(hSrc, NULL, pt);
	pSkp->PopWorldTransform();


	oapiReleaseTexture(hSrc);
	oapiReleaseSketchpad(pSkp);


	pCore->DeletePoly(hColors); // Must release Sketchpad before releasing any sketchpad resources
	pCore->DeletePoly(hOutline);
	pCore->DeletePoly(hOutline2);
	pCore->DeletePoly(hStrip);
	pCore->DeletePoly(hStrip2);

	oapiSaveSurface("SketchpadOutput2", hTgt, ImageFileFormat::IMAGE_DDS);

	oapiReleaseTexture(hTgt);
}




// ==============================================================
// Called when simulation session is about to be closed
//
void D3D9Client::clbkCloseSession(bool fastclose)
{

	LogAlw("================ clbkCloseSession ===============");

	//	Post shutdown signals for gcGUI applications
	//
	for (auto pApp : g_gcGUIAppList) pApp->clbkShutdown();

	//	Post shutdown signals for user applications
	//
	if (IsGenericProcEnabled(GENERICPROC_SHUTDOWN)) MakeGenericProcCall(GENERICPROC_SHUTDOWN, 0, NULL);


	// Check the status of RenderTarget Stack ------------------------------------------------
	//
	if (RenderStack.empty() == false) {
		LogErr("RenderStack contains %d items:", RenderStack.size());
		while (!RenderStack.empty()) {
			LogErr("RenderTarget=%s, DepthStencil=%s", _PTR(RenderStack.front().pColor), _PTR(RenderStack.front().pDepthStencil));
			RenderStack.pop_front();
		}
	}

	// Disable rendering and some other systems
	//
	bRunning = false;


	// At first, shutdown tile loaders -------------------------------------------------------
	//
	if (TileBuffer::ShutDown()==false) LogErr("Failed to Shutdown TileBuffer()");
	if (TileManager2Base::ShutDown()==false) LogErr("Failed to Shutdown TileManager2Base()");

	// Close dialog if Open and disconnect a visual form debug controls
	DebugControls::Close();

	// Disconnect textures from pipeline (Unlikely nesseccary)
	D3D9Effect::ShutDown();

	// DEBUG: List all textures connected to meshes
	/* DWORD cnt = MeshCatalog->CountEntries();
	for (DWORD i=0;i<cnt;i++) {
		D3D9Mesh *x = (D3D9Mesh*)MeshCatalog->Get(i);
		if (x) x->DumpTextures();
	} */
	//GraphicsClient::clbkCloseSession(fastclose);
	pCustomSplashScreen = NULL;
	pSplashTextColor = 0xE0A0A0;

	SAFE_DELETE(pWM);
	LogAlw("================= Deleting Scene ================");
	Scene::GlobalExit();
	SAFE_DELETE(scene);
	LogAlw("============== Deleting Mesh Manager ============");
	SAFE_DELETE(meshmgr);
	WriteLog("[Session Closed. Scene deleted.]");

}

// ==============================================================

void D3D9Client::clbkDestroyRenderWindow (bool fastclose)
{
	_TRACE;
	oapiWriteLog((char*)"D3D9: [Destroy Render Window Called]");
	LogAlw("============= clbkDestroyRenderWindow ===========");

#ifdef _NVAPI_H
	if (bNVAPI) {
		if (pStereoHandle) {
			if (NvAPI_Stereo_DestroyHandle(pStereoHandle)!=NVAPI_OK) {
				LogErr("Failed to destroy stereo handle");
			}
		}
	}
#endif

	LogAlw("===== Calling GlobalExit() for sub-systems ======");
	HazeManager::GlobalExit();
	HazeManager2::GlobalExit();
	TileManager::GlobalExit();
	TileManager2Base::GlobalExit();
	D3D9ParticleStream::GlobalExit();
	CSphereManager::GlobalExit();
	vStar::GlobalExit();
	vVessel::GlobalExit();
	vPlanet::GlobalExit();
	vObject::GlobalExit();
	D3D9Mesh::GlobalExit();

	SAFE_DELETE(defpen);
	SAFE_DELETE(deffont);

	DeleteObject(hLblFont1);
	DeleteObject(hLblFont2);

	D3D9Pad::GlobalExit();
	D3D9Text::GlobalExit();
	D3D9Effect::GlobalExit();

	SAFE_RELEASE(pSplashScreen);	// Splash screen related
	SAFE_RELEASE(pTextScreen);		// Splash screen related
	DELETE_SURFACE(pDefaultTex);
	SAFE_RELEASE(pNoiseTex);

	SURFHANDLE hBackBuffer = GetBackBufferHandle();

	DELETE_SURFACE(hBackBuffer);

	LogAlw("============ Checking Object Catalogs ===========");

	// Clear microtextures --------------------------------------------------------------------------------------
	//
	for (auto& it : MicroTextures) SAFE_RELEASE(it.second);
	MicroTextures.clear();


	// Check surface catalog --------------------------------------------------------------------------------------
	//
	if (SharedTextures.size() > 0)
	{
		LogWrn("Texture Repository has %u entries... Releasing...", (DWORD)SharedTextures.size());
		auto Undeleted(SharedTextures);
		for (auto srf : Undeleted) {
			LogWrn("Texture [%s]", SURFACE(srf.second)->GetName());
			delete lpSurfNative(srf.second);
		}
	}

	// Check surface catalog --------------------------------------------------------------------------------------
	//
	if (SurfaceCatalog.size() > 0)
	{
		LogErr("UnDeleted Surfaces(s) Detected %u... Releasing...", (DWORD)SurfaceCatalog.size());
		auto Undeleted(SurfaceCatalog);
		for (auto srf : Undeleted) {
			LogErr("Surface [%s] (%u, %u)", srf->GetName(), srf->GetWidth(), srf->GetHeight());
			delete srf;
		}
	}

	// Check mesh catalog --------------------------------------------------------------------------------------
	//
	if (MeshCatalog.size() > 0)
	{
		LogErr("UnDeleted Meshe(s) Detected %u", (DWORD)MeshCatalog.size());
		auto Undeleted(MeshCatalog);
		for (auto msh : Undeleted)
		{
			LogErr("Mesh[%s] Handle = %s ", msh->GetName(), _PTR(msh));
			delete msh;
		}
	}

	// Check Fonts catalog --------------------------------------------------------------------------------------
	//
	if (g_fonts.size()) {
		LogWrn("%u un-released fonts!", g_fonts.size());
		for (auto it = g_fonts.begin(); it != g_fonts.end(); ) {
			clbkReleaseFont(*it++);
		}
		g_fonts.clear();
	}

	// --- Brushes
	if (g_brushes.size()) {
		LogWrn("%u un-released brushes!", g_brushes.size());
		for (auto it = g_brushes.begin(); it != g_brushes.end(); ) {
			clbkReleaseBrush(*it++);
		}
		g_brushes.clear();
	}

	// --- Pens
	if (g_pens.size()) {
		LogWrn("%u un-released pens!", g_pens.size());
		for (auto it = g_pens.begin(); it != g_pens.end(); ) {
			clbkReleasePen(*it++);
		}
		g_pens.clear();
	}

	// Check tile catalog --------------------------------------------------------------------------------------
	//

	for (auto it : MeshMap)	SAFE_DELETE(it.second);

	MeshMap.clear();
	SharedTextures.clear();
	SurfaceCatalog.clear();
	MeshCatalog.clear();

	g_pTexmgr_tt->CleanUp();
	g_pVtxmgr_vb->CleanUp();
	g_pIdxmgr_ib->CleanUp();
	SAFE_DELETE(g_pTexmgr_tt);
	SAFE_DELETE(g_pVtxmgr_vb);
	SAFE_DELETE(g_pIdxmgr_ib);

	pFramework->DestroyObjects();

	SAFE_DELETE(pFramework);

	// Close Render Window -----------------------------------------
	GraphicsClient::clbkDestroyRenderWindow(fastclose);

	hRenderWnd		 = NULL;
	pDevice			 = NULL;
	bFailed			 = false;
	viewW = viewH    = 0;
	viewBPP          = 0;

}

// ==============================================================

void D3D9Client::clbkDebugString(const char* str)
{
	D3D9DebugLog("%s", str);
}


// ==============================================================

void D3D9Client::PushSketchpad(SURFHANDLE surf, D3D9Pad *pSkp) const
{
	if (surf) {
		LPDIRECT3DSURFACE9 pTgt = SURFACE(surf)->GetSurface();
		LPDIRECT3DSURFACE9 pDep = SURFACE(surf)->GetDepthStencil();
		PushRenderTarget(pTgt, pDep, RENDERPASS_SKETCHPAD);
		RenderStack.front().pSkp = pSkp;
	}
}


// ==============================================================

void D3D9Client::PushRenderTarget(LPDIRECT3DSURFACE9 pColor, LPDIRECT3DSURFACE9 pDepthStencil, int code) const
{
	static const char *labels[] = { "NULL", "MAIN", "ENV", "CUSTOMCAM", "SHADOWMAP", "PICK", "SKETCHPAD", "OVERLAY" };

	RenderTgtData data;
	data.pColor = pColor;
	data.pDepthStencil = pDepthStencil;
	data.pSkp = NULL;
	data.code = code;

	if (pColor) {
		D3DSURFACE_DESC desc;
		pColor->GetDesc(&desc);
		D3DVIEWPORT9 vp = { 0, 0, desc.Width, desc.Height, 0.0f, 1.0f };
		pDevice->SetViewport(&vp);
	}

	// If pDepthStencil is NULL set NULL
	if (pDevice->SetDepthStencilSurface(pDepthStencil) != S_OK) assert(false);
	if (pColor) if (pDevice->SetRenderTarget(0, pColor) != S_OK) assert(false);

	RenderStack.push_front(data);
	LogDbg("Plum", "PUSH:RenderStack[%lu]={%s, %s} %s", RenderStack.size(), _PTR(data.pColor), _PTR(data.pDepthStencil), labels[data.code]);
}

// ==============================================================

void D3D9Client::AlterRenderTarget(LPDIRECT3DSURFACE9 pColor, LPDIRECT3DSURFACE9 pDepthStencil)
{
	D3DSURFACE_DESC desc;
	pColor->GetDesc(&desc);
	D3DVIEWPORT9 vp = { 0, 0, desc.Width, desc.Height, 0.0f, 1.0f };

	pDevice->SetViewport(&vp);
	pDevice->SetRenderTarget(0, pColor);
	pDevice->SetDepthStencilSurface(pDepthStencil);
}

// ==============================================================

void D3D9Client::PopRenderTargets() const
{
	static const char *labels[] = { "NULL", "MAIN", "ENV", "CUSTOMCAM", "SHADOWMAP", "PICK", "SKETCHPAD", "OVERLAY" };

	assert(RenderStack.empty() == false);

	RenderStack.pop_front();

	if (RenderStack.empty()) {
		LogDbg("Orange", "POP: Last one out ------------------------------------");
		return;
	}

	RenderTgtData data = RenderStack.front();

	if (data.pColor) {
		D3DSURFACE_DESC desc;
		data.pColor->GetDesc(&desc);
		D3DVIEWPORT9 vp = { 0, 0, desc.Width, desc.Height, 0.0f, 1.0f };

		pDevice->SetViewport(&vp);
		pDevice->SetRenderTarget(0, data.pColor);
		pDevice->SetDepthStencilSurface(data.pDepthStencil);
	}

	LogDbg("Plum", "POP:RenderStack[%lu]={%s, %s, %s} %s", RenderStack.size(), _PTR(data.pColor), _PTR(data.pDepthStencil), _PTR(data.pSkp), labels[data.code]);
}

// ==============================================================

void D3D9Client::HackFriendlyHack()
{
	// Try to make the application more hackable by setting the D3D Device in 'more' expected state.

	D3DSURFACE_DESC desc;
	GetBackBuffer()->GetDesc(&desc);
	D3DVIEWPORT9 vp = { 0, 0, desc.Width, desc.Height, 0.0f, 1.0f };
	pDevice->SetViewport(&vp);
	pDevice->SetRenderTarget(0, GetBackBuffer());
	pDevice->SetDepthStencilSurface(GetDepthStencil());
}

// ==============================================================

LPDIRECT3DSURFACE9 D3D9Client::GetTopDepthStencil()
{
	if (RenderStack.empty()) return NULL;
	return RenderStack.front().pDepthStencil;
}

// ==============================================================

LPDIRECT3DSURFACE9 D3D9Client::GetTopRenderTarget()
{
	if (RenderStack.empty()) return NULL;
	return RenderStack.front().pColor;
}

// ==============================================================

D3D9Pad *D3D9Client::GetTopInterface() const
{
	if (RenderStack.empty()) return NULL;
	return RenderStack.front().pSkp;
}


// ==============================================================

void D3D9Client::clbkUpdate(bool running)
{
	_TRACE;
	double tot_update = D3D9GetTime();
	if (bFailed==false && bRunning) scene->Update();
	D3D9SetTime(D3D9Stats.Timer.Update, tot_update);
}

// ==============================================================

double frame_time = 0.0;
double scene_time = 0.0;

void D3D9Client::clbkRenderScene()
{
	_TRACE;

	if (pDevice==NULL || scene==NULL) return;
	if (bFailed) return;
	if (!bRunning) return;

	if (pWM) pWM->Animate();

	if (Config->PresentLocation == 1) PresentScene();

	scene_time = D3D9GetTime();

	if (pDevice->TestCooperativeLevel()!=S_OK) {
		bFailed=true;
		MessageBoxA(pFramework->GetRenderWindow(),"Connection to Direct3DDevice is lost\nExit the simulation with Ctrl+Q and restart.\n\nAlt-Tabing not supported in a true fullscreen mode.\nDialog windows won't work with multi-sampling in a true fullscreen mode.","D3D9Client: Lost Device",0);
		return;
	}

	UINT mem = pDevice->GetAvailableTextureMem()>>20;
	if (mem<32) TileBuffer::HoldThread(true);

	scene->RenderMainScene();		// Render the main scene

	VESSEL *hVes = oapiGetFocusInterface();

	if (hVes && Config->LabelDisplayFlags)
	{
		char Label[7] = "";
		if (Config->LabelDisplayFlags & D3D9Config::LABEL_DISPLAY_RECORD && hVes->Recording()) strcpy_s(Label, 7, "Record");
		if (Config->LabelDisplayFlags & D3D9Config::LABEL_DISPLAY_REPLAY && hVes->Playback()) strcpy_s(Label, 7, "Replay");

		if (Label[0]!=0) {
			pDevice->BeginScene();
			RECT rect2 = _RECT(0, viewH - 60, viewW, viewH - 20);
			pFramework->GetLargeFont()->DrawTextA(0, Label, 6, &rect2, DT_CENTER | DT_TOP, D3DCOLOR_XRGB(0, 0, 0));
			rect2.left-=4; rect2.top-=4;
			pFramework->GetLargeFont()->DrawTextA(0, Label, 6, &rect2, DT_CENTER | DT_TOP, D3DCOLOR_XRGB(255, 255, 255));
			pDevice->EndScene();
		}
	}

	if (bFreeze) {
		RECT rect2 = _RECT(0, viewH - 60, viewW, viewH - 20);
		pFramework->GetLargeFont()->DrawTextA(0, "Frozen", 6, &rect2, DT_CENTER | DT_TOP, D3DCOLOR_XRGB(0, 255, 255));
	}

	D3D9SetTime(D3D9Stats.Timer.Scene, scene_time);


	if (bControlPanel) RenderControlPanel();

	// Compute total frame time
	D3D9SetTime(D3D9Stats.Timer.FrameTotal, frame_time);
	frame_time = D3D9GetTime();
}

// ==============================================================

void D3D9Client::clbkTimeJump(double simt, double simdt, double mjd)
{
	_TRACE;
	GraphicsClient::clbkTimeJump (simt, simdt, mjd);
}

// ==============================================================

void D3D9Client::PresentScene()
{
	double time = D3D9GetTime();

	if (bFullscreen == false) {
		RenderWithPopupWindows();
		pDevice->Present(0, 0, 0, 0);
	}
	else {
		if (!RenderWithPopupWindows()) pDevice->Present(0, 0, 0, 0);
	}

	D3D9SetTime(D3D9Stats.Timer.Display, time);
}

// ==============================================================

double framer_rater_limit = 0.0;

bool D3D9Client::clbkDisplayFrame()
{
	_TRACE;
//	static int iRefrState = 0;
	double time = D3D9GetTime();

	if (!bRunning && pDevice) {
		RECT txt = _RECT( loadd_x, loadd_y, loadd_x+loadd_w, loadd_y+loadd_h );
		pDevice->StretchRect(pSplashScreen, NULL, pBackBuffer, NULL, D3DTEXF_POINT);
		pDevice->StretchRect(pTextScreen, NULL, pBackBuffer, &txt, D3DTEXF_POINT);
	}

	if (Config->PresentLocation == 0) PresentScene();

	double frmt = (1000000.0/Config->FrameRate) - (time - framer_rater_limit);

	framer_rater_limit = time;

	if (Config->EnableLimiter && Config->FrameRate>0 && bVSync==false) {
		if (frmt>0) frame_timer++;
		else        frame_timer--;
		if (frame_timer>40) frame_timer=40;
		Sleep(frame_timer);
	}

	return true;
}

// ==============================================================

void D3D9Client::clbkPreOpenPopup ()
{
	_TRACE;
	GetDevice()->SetDialogBoxMode(true);
}

// =======================================================================

static DWORD g_lastPopupWindowCount = 0;
static void FixOutOfScreenPositions (const HWND *hWnd, DWORD count)
{
	// Only check if a popup window is *added*
	if (count > g_lastPopupWindowCount)
	{
		for (DWORD i=0; i<count; ++i)
		{
			RECT rect;
			GetWindowRect(hWnd[i], &rect);

			int x = -1, y, w, h; // x != -1 indicates "position change needed"
			if (rect.left < 0) {
				x = 0;
				y = rect.top;
			}
			if (rect.top  < 0) {
				x = rect.left;
				y = 0;
			}

			// For the rest we need monitor information...
			HMONITOR monitor = MonitorFromWindow(hWnd[i], MONITOR_DEFAULTTONEAREST);
			MONITORINFO info;
			info.cbSize = sizeof(MONITORINFO);
			GetMonitorInfo(monitor, &info);

			int monitorWidth = info.rcMonitor.right - info.rcMonitor.left; // info.rcWork....
			int monitorHeight = info.rcMonitor.bottom - info.rcMonitor.top;

			if (rect.right > monitorWidth) {
				x = monitorWidth - (rect.right - rect.left);
				y = rect.top;
			}
			if (rect.bottom > monitorHeight) {
				x = rect.left;
				y = monitorHeight - (rect.bottom - rect.top);
			}

			if (x != -1) {
				w = rect.right - rect.left,
				h = rect.bottom - rect.top;
				MoveWindow(hWnd[i], x, y, w, h, FALSE);
			}
		}

	}
	g_lastPopupWindowCount = count;
}

// =======================================================================

bool D3D9Client::RenderWithPopupWindows()
{
	_TRACE;

	const HWND *hPopupWnd;
	DWORD count = GetPopupList(&hPopupWnd);

	if (bFullscreen) {
		if (count) GetDevice()->SetDialogBoxMode(true);
		else       GetDevice()->SetDialogBoxMode(false);
	}

	FixOutOfScreenPositions(hPopupWnd, count);

	if (!bFullscreen) {
		for (DWORD i=0;i<count;i++) {
			DWORD val = GetWindowLongA(hPopupWnd[i], GWL_STYLE);
			if ((val&WS_SYSMENU)==0) {
				SetWindowLongA(hPopupWnd[i], GWL_STYLE, val|WS_SYSMENU);
			}
		}
	}

	return false;
}

#pragma region Particle stream functions

// =======================================================================
// Particle stream functions
// ==============================================================

ParticleStream *D3D9Client::clbkCreateParticleStream(PARTICLESTREAMSPEC *pss)
{
	LogErr("UnImplemented Feature Used clbkCreateParticleStream");
	return NULL;
}

// =======================================================================

ParticleStream *D3D9Client::clbkCreateExhaustStream(PARTICLESTREAMSPEC *pss,
	OBJHANDLE hVessel, const double *lvl, const VECTOR3 *ref, const VECTOR3 *dir)
{
	_TRACE;
	ExhaustStream *es = new ExhaustStream (this, hVessel, lvl, ref, dir, pss);
	scene->AddParticleStream (es);
	return es;
}

// =======================================================================

ParticleStream *D3D9Client::clbkCreateExhaustStream(PARTICLESTREAMSPEC *pss,
	OBJHANDLE hVessel, const double *lvl, const VECTOR3 &ref, const VECTOR3 &dir)
{
	_TRACE;
	ExhaustStream *es = new ExhaustStream (this, hVessel, lvl, ref, dir, pss);
	scene->AddParticleStream (es);
	return es;
}

// ======================================================================

ParticleStream *D3D9Client::clbkCreateReentryStream (PARTICLESTREAMSPEC *pss,
	OBJHANDLE hVessel)
{
	_TRACE;
	ReentryStream *rs = new ReentryStream (this, hVessel, pss);
	scene->AddParticleStream (rs);
	return rs;
}

#pragma endregion

// ==============================================================

ScreenAnnotation* D3D9Client::clbkCreateAnnotation()
{
	_TRACE;
	return GraphicsClient::clbkCreateAnnotation();
}

#pragma region Mesh functions

// ==============================================================

void D3D9Client::clbkStoreMeshPersistent(MESHHANDLE hMesh, const char *fname)
{
	_TRACE;
	if (ChkDev(__FUNCTION__)) return;

	if (fname) {
		LogAlw("Storing a mesh %s (%s)", _PTR(hMesh), fname);
		if (hMesh==NULL) LogErr("D3D9Client::clbkStoreMeshPersistent(%s) hMesh is NULL",fname);
	}
	else {
		LogAlw("Storing a mesh %s", _PTR(hMesh));
		if (hMesh==NULL) LogErr("D3D9Client::clbkStoreMeshPersistent() hMesh is NULL");
	}

	if (hMesh==NULL) return;

	int idx = meshmgr->StoreMesh(hMesh, fname);
}

// ==============================================================

DEVMESHHANDLE D3D9Client::GetDevMesh(MESHHANDLE hMesh)
{
	const D3D9Mesh *pDevMesh = meshmgr->GetMesh(hMesh);
	if (!pDevMesh) {
		meshmgr->StoreMesh(hMesh, "GetDevMesh()");
		pDevMesh = meshmgr->GetMesh(hMesh);
	}

	// Create a new Instance from a template
	return DEVMESHHANDLE(new D3D9Mesh(hMesh, *pDevMesh));
}

// ==============================================================

bool D3D9Client::clbkSetMeshTexture(DEVMESHHANDLE hMesh, DWORD texidx, SURFHANDLE surf)
{
	_TRACE;
	if (hMesh && surf) return ((D3D9Mesh*)hMesh)->SetTexture(texidx, SURFACE(surf));
	return false;
}

// ==============================================================

int D3D9Client::clbkSetMeshMaterial(DEVMESHHANDLE hMesh, DWORD matidx, const MATERIAL *mat)
{
	_TRACE;
	if (!hMesh) return 3;
	D3D9Mesh *mesh = (D3D9Mesh*)hMesh;
	DWORD nmat = mesh->GetMaterialCount();
	if (matidx >= nmat) return 4; // "index out of range"
	D3D9MatExt meshmat;
	//mesh->GetMaterial(&meshmat, matidx);
	CreateMatExt((const D3DMATERIAL9 *)mat, &meshmat);
	mesh->SetMaterial(&meshmat, matidx);
	return 0;
}

// ==============================================================

int D3D9Client::clbkMeshMaterial (DEVMESHHANDLE hMesh, DWORD matidx, MATERIAL *mat)
{
	_TRACE;
	if (!hMesh) return 3;
	D3D9Mesh *mesh = (D3D9Mesh*)hMesh;
	DWORD nmat = mesh->GetMaterialCount();
	if (matidx >= nmat) return 4; // "index out of range"
	const D3D9MatExt *meshmat = mesh->GetMaterial(matidx);
	if (meshmat) GetMatExt(meshmat, (D3DMATERIAL9 *)mat);
	return 0;
}

// ==============================================================

int D3D9Client::clbkSetMeshMaterialEx(DEVMESHHANDLE hMesh, DWORD matidx, MatProp mat, const oapi::FVECTOR4* in)
{
	if (!hMesh) return 3;
	D3D9Mesh* mesh = (D3D9Mesh*)hMesh;
	return mesh->SetMaterialEx(matidx, mat, in);
}

// ==============================================================

int D3D9Client::clbkMeshMaterialEx(DEVMESHHANDLE hMesh, DWORD matidx, MatProp mat, oapi::FVECTOR4* out)
{
	if (!hMesh) return 3;
	D3D9Mesh* mesh = (D3D9Mesh*)hMesh;
	return mesh->GetMaterialEx(matidx, mat, out);
}

// ==============================================================

bool D3D9Client::clbkSetMeshProperty(DEVMESHHANDLE hMesh, DWORD prop, DWORD value)
{
	_TRACE;
	D3D9Mesh *mesh = (D3D9Mesh*)hMesh;
	switch (prop) {
		case MESHPROPERTY_MODULATEMATALPHA:
			mesh->EnableMatAlpha(value!=0);
			return true;
	}
	return false;
}

// ==============================================================
// Returns a dev-mesh for a visual

MESHHANDLE D3D9Client::clbkGetMesh(VISHANDLE vis, UINT idx)
{
	_TRACE;
	if (vis==NULL) {
		LogErr("NULL visual in clbkGetMesh(NULL,%u)",idx);
		return NULL;
	}
	MESHHANDLE hMesh = ((vObject*)vis)->GetMesh(idx);
	if (hMesh==NULL) LogWrn("clbkGetMesh() returns NULL");
	return hMesh;
}

// =======================================================================

int D3D9Client::clbkEditMeshGroup(DEVMESHHANDLE hMesh, DWORD grpidx, GROUPEDITSPEC *ges)
{
	_TRACE;
	return ((D3D9Mesh*)hMesh)->EditGroup(grpidx, ges);
}

// =======================================================================


int D3D9Client::clbkGetMeshGroup (DEVMESHHANDLE hMesh, DWORD grpidx, GROUPREQUESTSPEC *grs)
{
	_TRACE;
	return ((D3D9Mesh*)hMesh)->GetGroup (grpidx, grs);
}

#pragma endregion

// ==============================================================

void D3D9Client::clbkNewVessel(OBJHANDLE hVessel)
{
	_TRACE;
	if (scene) scene->NewVessel(hVessel);
}

// ==============================================================

void D3D9Client::clbkDeleteVessel(OBJHANDLE hVessel)
{
	if (scene) scene->DeleteVessel(hVessel);
}


// ==============================================================
// copy video options from the video tab

void D3D9Client::clbkRefreshVideoData()
{
	_TRACE;
	if (vtab) vtab->UpdateConfigData();
}

// ==============================================================

void D3D9Client::clbkOptionChanged(DWORD cat, DWORD item)
{
	switch (cat) {
	case OPTCAT_CELSPHERE:
		if (scene) scene->OnOptionChanged(cat, item);
		return;
	}
}

// ==============================================================

bool D3D9Client::clbkUseLaunchpadVideoTab() const
{
	_TRACE;
	return true;
}

// ==============================================================
// Fullscreen mode flag

bool D3D9Client::clbkFullscreenMode() const
{
	_TRACE;
	return bFullscreen;
}

// ==============================================================
// return the dimensions of the render viewport

void D3D9Client::clbkGetViewportSize(DWORD *width, DWORD *height) const
{
	_TRACE;
	*width = viewW, *height = viewH;
}

// ==============================================================
// Returns a specific render parameter

bool D3D9Client::clbkGetRenderParam(DWORD prm, DWORD *value) const
{
	_TRACE;
	switch (prm) {
		case RP_COLOURDEPTH:
			*value = viewBPP;
			return true;

		case RP_ZBUFFERDEPTH:
			*value = GetFramework()->GetZBufferBitDepth();
			return true;

		case RP_STENCILDEPTH:
			*value = GetFramework()->GetStencilBitDepth();
			return true;

		case RP_MAXLIGHTS:
			*value = MAX_SCENE_LIGHTS;
			return true;

		case RP_REQUIRETEXPOW2:
			*value = 0;
			return true;
	}
	return false;
}

// ==============================================================
// Responds to visual events

int D3D9Client::clbkVisEvent(OBJHANDLE hObj, VISHANDLE vis, DWORD msg, DWORD_PTR context)
{
	_TRACE;
	VisObject *vo = (VisObject*)vis;
	vo->clbkEvent(msg, context);
	if (DebugControls::IsActive()) {
		if (msg==EVENT_VESSEL_INSMESH || msg==EVENT_VESSEL_DELMESH) {
			if (DebugControls::GetVisual()==vo) DebugControls::UpdateVisual();
		}
	}
	return 1;
}


// ==============================================================
//
void D3D9Client::PickTerrain(DWORD uMsg, int xpos, int ypos)
{
	bool bUD = (uMsg == WM_LBUTTONUP || uMsg == WM_RBUTTONUP || uMsg == WM_LBUTTONDOWN || uMsg == WM_RBUTTONDOWN);
	bool bPrs = IsGenericProcEnabled(GENERICPROC_PICK_TERRAIN) && bUD;
	bool bHov = IsGenericProcEnabled(GENERICPROC_HOVER_TERRAIN) && (uMsg == WM_MOUSEMOVE || uMsg == WM_MOUSEWHEEL);

	if (bPrs || bHov) {
		gcCore::PickGround pg = gcCore2::ScanScreen(xpos, ypos);
		pg.msg = uMsg;
		if (bPrs) MakeGenericProcCall(GENERICPROC_PICK_TERRAIN, sizeof(gcCore::PickGround), &pg);
		if (bHov) MakeGenericProcCall(GENERICPROC_HOVER_TERRAIN, sizeof(gcCore::PickGround), &pg);
	}
}


// ==============================================================
// Message handler for render window

LRESULT D3D9Client::RenderWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool bTrackMouse = false;
	static short xpos=0, ypos=0;

	D3D9Pick pick;

	if (hRenderWnd!=hWnd && uMsg!= WM_NCDESTROY) {
		LogErr("Invalid Window !! RenderWndProc() called after calling clbkDestroyRenderWindow() uMsg=0x%X", uMsg);
		return 0;
	}

	if (bRunning && DebugControls::IsActive()) {
		// Must update camera to correspond MAIN_SCENE due to Pick() function,
		// because env-maps have altered camera settings
		// GetScene()->UpdateCameraFromOrbiter(RENDERPASS_PICKSCENE);
		// Obsolete: since moving env/cam stuff in pre-scene
	}

	if (pWM) if (pWM->MainWindowProc(hWnd, uMsg, wParam, lParam)) return 0;


	switch (uMsg)
	{
		case WM_MOUSELEAVE:
		{
			if (bTrackMouse && bRunning) GraphicsClient::RenderWndProc (hWnd, WM_LBUTTONUP, 0, 0);
			return 0;
		}

		case WM_MBUTTONDOWN:
		{
			break;
		}

		case WM_RBUTTONUP:
		case WM_RBUTTONDOWN:
		{
			int xp = GET_X_LPARAM(lParam);
			int yp = GET_Y_LPARAM(lParam);
			PickTerrain(uMsg, xp, yp);
			break;
		}


		case WM_LBUTTONDOWN:
		{
			bTrackMouse = true;
			xpos = GET_X_LPARAM(lParam);
			ypos = GET_Y_LPARAM(lParam);

			GetScene()->vPickRay = GetScene()->GetPickingRay(xpos, ypos);

			TRACKMOUSEEVENT te; te.cbSize = sizeof(TRACKMOUSEEVENT); te.dwFlags = TME_LEAVE; te.hwndTrack = hRenderWnd;
			TrackMouseEvent(&te);

			bool bShift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
			bool bCtrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
			bool bPckVsl = IsGenericProcEnabled(GENERICPROC_PICK_VESSEL);

			if (DebugControls::IsActive() || bPckVsl || (bShift && bCtrl)) {
				pick = GetScene()->PickScene(xpos, ypos);
				if (bPckVsl) {
					gcCore::PickData out;
					out.hVessel = pick.vObj->GetObjectA();
					out.mesh = MESHHANDLE(pick.pMesh);
					out.group = pick.group;
					out.pos = _FV(pick.pos);
					out.normal = _FV(pick.normal);
					out.dist = pick.dist;
					MakeGenericProcCall(GENERICPROC_PICK_VESSEL, sizeof(gcCore::PickData), &out);
				}
			}

			PickTerrain(uMsg, xpos, ypos);

			// No Debug Controls
			if (bShift && bCtrl && !DebugControls::IsActive() && !oapiCameraInternal()) {

				if (!pick.pMesh) break;

				OBJHANDLE hObj = pick.vObj->Object();
				if (oapiGetObjectType(hObj) == OBJTP_VESSEL) {
					oapiSetFocusObject(hObj);
				}

				break;
			}

			// With Debug Controls
			if (DebugControls::IsActive()) {

				DWORD flags = *(DWORD*)GetConfigParam(CFGPRM_GETDEBUGFLAGS);

				if (flags&DBG_FLAGS_PICK) {

					if (!pick.pMesh) break;

					if (bShift && bCtrl) {
						OBJHANDLE hObj = pick.vObj->Object();
						if (oapiGetObjectType(hObj)==OBJTP_VESSEL) {
							oapiSetFocusObject(hObj);
							break;
						}
					}
					else if (pick.group>=0) {
						DebugControls::SetVisual(pick.vObj);
						DebugControls::SelectMesh(pick.pMesh);
						DebugControls::SelectGroup(pick.group);
						DebugControls::SetGroupHighlight(true);
						DebugControls::SetPickPos(pick.pos);
					}
				}
			}

			break;
		}

		case WM_LBUTTONUP:
		{
			int xp = GET_X_LPARAM(lParam);
			int yp = GET_Y_LPARAM(lParam);

			PickTerrain(uMsg, xp, yp);

			if (DebugControls::IsActive()) {
				DWORD flags = *(DWORD*)GetConfigParam(CFGPRM_GETDEBUGFLAGS);
				if (flags&DBG_FLAGS_PICK) {
					DebugControls::SetGroupHighlight(false);
				}
			}
			bTrackMouse = false;
			break;
		}

		case WM_KEYDOWN:
		{
			bool bShift = (GetAsyncKeyState(VK_SHIFT) & 0x8000)!=0;
			bool bCtrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000)!=0;
			if (wParam == 'C' && bShift && bCtrl) bControlPanel = !bControlPanel;
			if (wParam == 'N' && bShift && bCtrl) Config->bCloudNormals = !Config->bCloudNormals;
			if (wParam == 'F' && bShift && bCtrl) {
				if (bFreeze) bFreezeEnable = bFreeze = false;
				else bFreezeEnable = true;
			}
			if (wParam == 'A' && bFreeze) bFreezeRenderAll = !bFreezeRenderAll;

			break;
		}

		case WM_MOUSEWHEEL:
		{
			if (DebugControls::IsActive()) {
				short d = GET_WHEEL_DELTA_WPARAM(wParam);
				if (d<-1) d=-1;
				if (d>1) d=1;
				double speed = *(double *)GetConfigParam(CFGPRM_GETCAMERASPEED);
				speed *= (DebugControls::GetVisualSize()/100.0);
				if (scene->CameraPan(_V(0,0,double(d))*2.0, speed)) return 0;
			}

			PickTerrain(uMsg, xpos, ypos);
			break;
		}

		case WM_MOUSEMOVE:

			if (DebugControls::IsActive())
			{

				double x = double(GET_X_LPARAM(lParam) - xpos);
				double y = double(GET_Y_LPARAM(lParam) - ypos);
				xpos = GET_X_LPARAM(lParam);
				ypos = GET_Y_LPARAM(lParam);

				if (bTrackMouse) {
					double speed = *(double *)GetConfigParam(CFGPRM_GETCAMERASPEED);
					speed *= (DebugControls::GetVisualSize() / 100.0);
					if (scene->CameraPan(_V(-x, y, 0)*0.05, speed)) return 0;
				}
			}

			xpos = GET_X_LPARAM(lParam);
			ypos = GET_Y_LPARAM(lParam);

			PickTerrain(uMsg, xpos, ypos);

			break;

		case WM_MOVE:
			// If in windowed mode, move the Framework's window
			break;

		case WM_SYSCOMMAND:
			switch (wParam) {
				case SC_KEYMENU:
					// trap Alt system keys
					return 1;
				case SC_MOVE:
				case SC_SIZE:
				case SC_MAXIMIZE:
				case SC_MONITORPOWER:
					// Prevent moving/sizing and power loss in fullscreen mode
					if (bFullscreen) return 1;
					break;
			}
			break;

		case WM_SYSKEYUP:
			if (bFullscreen) return 0;  // trap Alt-key
			break;
	}

	if (!bRunning && uMsg>=0x0200 && uMsg<=0x020E) return 0;
	return GraphicsClient::RenderWndProc (hWnd, uMsg, wParam, lParam);
}


// ==============================================================
// Message handler for Launchpad "video" tab

INT_PTR D3D9Client::LaunchpadVideoWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	_TRACE;
	if (vtab) return vtab->WndProc(hWnd, uMsg, wParam, lParam);
	else return false;
}

// =======================================================================

void D3D9Client::clbkRender2DPanel (SURFHANDLE *hSurf, MESHHANDLE hMesh, MATRIX3 *T, float alpha, bool additive)
{
	_TRACE;

	SURFHANDLE surf = NULL;
	DWORD ngrp = oapiMeshGroupCount(hMesh);

	if (ngrp==0) return;

	float sx = 1.0f/(float)(T->m11),  dx = (float)(T->m13);
	float sy = 1.0f/(float)(T->m22),  dy = (float)(T->m23);
	float vw = (float)viewW;
	float vh = (float)viewH;

	D3DXMATRIX mVP;
	D3DXMatrixOrthoOffCenterRH(&mVP, (0.0f-dx)*sx, (vw-dx)*sx, (vh-dy)*sy, (0.0f-dy)*sy, -100.0f, 100.0f);
	D3D9Effect::SetViewProjMatrix(&mVP);

	for (DWORD i=0;i<ngrp;i++) {

		float scale = 1.0f;

		MESHGROUP *gr = oapiMeshGroup(hMesh, i);

		if (gr->UsrFlag & 2) continue; // skip this group

		DWORD TexIdx = gr->TexIdx;

		if (TexIdx >= TEXIDX_MFD0) {
			int mfdidx = TexIdx - TEXIDX_MFD0;
			surf = GetMFDSurface(mfdidx);
			if (!surf) surf = (SURFHANDLE)pDefaultTex;
		} else if (hSurf) {
			surf = hSurf[TexIdx];
		}
		else surf = oapiGetTextureHandle (hMesh, gr->TexIdx+1);

		for (unsigned int k=0;k<gr->nVtx;k++) gr->Vtx[k].z = 0.0f;

		D3D9Effect::Render2DPanel(gr, SURFACE(surf), &ident, alpha, scale, additive);
	}
}

// =======================================================================

void D3D9Client::clbkRender2DPanel (SURFHANDLE *hSurf, MESHHANDLE hMesh, MATRIX3 *T, bool additive)
{
	_TRACE;
	clbkRender2DPanel (hSurf, hMesh, T, 1.0f, additive);
}

// =======================================================================

DWORD D3D9Client::clbkGetDeviceColour (BYTE r, BYTE g, BYTE b)
{
	_TRACE;
	return ((DWORD)r << 16) + ((DWORD)g << 8) + (DWORD)b;
}



#pragma region Surface, Blitting and Filling Functions



// =======================================================================
// Surface functions
// =======================================================================

bool D3D9Client::clbkSaveSurfaceToImage(SURFHANDLE surf, const char *fname, ImageFileFormat  fmt, float quality)
{
	_TRACE;
	if (ChkDev(__FUNCTION__)) return false;

	if (surf==NULL) surf = pFramework->GetBackBufferHandle();

	LPDIRECT3DSURFACE9 pRTG = NULL;
	LPDIRECT3DSURFACE9 pSystem = NULL;
	LPDIRECT3DSURFACE9 pSurf = SURFACE(surf)->GetSurface();

	if (pSurf==NULL) return false;

	bool bRet = false;
	const D3DSURFACE_DESC *desc = SURFACE(surf)->GetDesc();
	D3DLOCKED_RECT pRect;

	if (fmt == ImageFileFormat::IMAGE_DDS) {
		char path[MAX_PATH];
		sprintf_s(path, "%s.dds", fname);
		return NatSaveSurface(path, pSurf);
	}

	if (desc->Pool != D3DPOOL_SYSTEMMEM)
	{
		HR(pDevice->CreateRenderTarget(desc->Width, desc->Height, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, false, &pRTG, NULL));
		HR(pDevice->CreateOffscreenPlainSurface(desc->Width, desc->Height, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &pSystem, NULL));
		HR(pDevice->StretchRect(pSurf, NULL, pRTG, NULL, D3DTEXF_NONE));
		HR(pDevice->GetRenderTargetData(pRTG, pSystem));

		if (pSystem->LockRect(&pRect, NULL, 0)==S_OK)
		{
			if (fname == NULL) {
				// copy device-dependent bitmap to clipboard
				bRet = SaveSurfaceToClipboard(desc);
			} else {
				// save as file
				bRet = SaveSurfaceToFile(desc, pRect, fname, fmt, quality);
			}
			pSystem->UnlockRect();
		}

		pRTG->Release();
		pSystem->Release();
		return bRet;
	}

	if (pSurf->LockRect(&pRect, NULL, D3DLOCK_READONLY)==S_OK)
	{
		if (fname == NULL) {
			// copy device-dependent bitmap to clipboard
			bRet = SaveSurfaceToClipboard(desc);
		} else {
			// save as file
			bRet = SaveSurfaceToFile(desc, pRect, fname, fmt, quality);
		}
		pSystem->UnlockRect();
	}

	return bRet;
}

// ==============================================================

bool oapi::D3D9Client::SaveSurfaceToFile (const D3DSURFACE_DESC* desc, D3DLOCKED_RECT& pRect,
                                          const char* fname, oapi::ImageFileFormat fmt, float quality)
{
	bool bRet = false;
	ImageData ID;

	ID.bpp = 24;
	ID.height = desc->Height;
	ID.width = desc->Width;
	ID.stride = ((ID.width * ID.bpp + 31) & ~31) >> 3;
	ID.bufsize = ID.stride * ID.height;

	BYTE* tgt = ID.data = new BYTE[ID.bufsize];
	BYTE* src = (BYTE*)pRect.pBits;

	for (DWORD k = 0; k<desc->Height; k++) {
		for (DWORD i = 0; i<desc->Width; i++) {
			tgt[0 + i * 3] = src[0 + i * 4];
			tgt[1 + i * 3] = src[1 + i * 4];
			tgt[2 + i * 3] = src[2 + i * 4];
		}
		tgt += ID.stride;
		src += pRect.Pitch;
	}

	bRet = WriteImageDataToFile(ID, fname, fmt, quality);

	delete[]ID.data;
	ID.data = NULL;

	return bRet;
}

// ==============================================================

bool oapi::D3D9Client::SaveSurfaceToClipboard (const D3DSURFACE_DESC* desc)
{
	if (OpenClipboard(hRenderWnd))
	{
		HDC hDC = GetDC(hRenderWnd);
		HDC hdcmem = CreateCompatibleDC(hDC);
		HBITMAP hBm = CreateCompatibleBitmap(hDC, desc->Width, desc->Height);

		SelectObject(hdcmem, hBm);
		BitBlt(hdcmem, 0, 0, desc->Width, desc->Height, hDC, 0, 0, SRCCOPY);

		EmptyClipboard();
		SetClipboardData(CF_BITMAP, hBm);
		CloseClipboard();
		return true;
	}
	return false;
}

// ==============================================================

SURFHANDLE D3D9Client::clbkLoadTexture(const char *fname, DWORD flags)
{
	_TRACE;
	if (ChkDev(__FUNCTION__)) return NULL;

	DWORD attrib = OAPISURFACE_TEXTURE;
	if (flags & 0x1) attrib |= OAPISURFACE_SYSMEM;
	if (flags & 0x2) attrib |= OAPISURFACE_UNCOMPRESS | OAPISURFACE_RENDERTARGET;
	if (flags & 0x4) attrib |= OAPISURFACE_NOMIPMAPS;
	if (flags & 0x8) attrib |= OAPISURFACE_SHARED;

	return clbkLoadSurface(fname, attrib);
}

// ==============================================================

SURFHANDLE D3D9Client::clbkLoadSurface (const char *fname, DWORD attrib, bool bPath)
{
	_TRACE;
	if (ChkDev(__FUNCTION__)) return NULL;

	static const DWORD val = OAPISURFACE_RENDERTARGET | OAPISURFACE_GDI | OAPISURFACE_SYSMEM;
	static const DWORD exclude = ~(OAPISURFACE_SHARED | OAPISURFACE_ORIGIN);

	if (!(attrib & val))
	{
		// It's a regular texture, let's manage it
		//
		string name(fname);

		if (attrib & OAPISURFACE_SHARED)
		{
			auto ent = SharedTextures.find(name);

			if (ent == SharedTextures.end())
			{
				SURFHANDLE hSrf = NatLoadSurface(fname, attrib, bPath);
				if (hSrf) SharedTextures[name] = hSrf;
				return hSrf;
			}
			else return ent->second;
		}

		/*
		auto ent = ClonedTextures.find(name);

		if (ent == ClonedTextures.end())
		{
			SURFHANDLE hSrf = NatLoadSurface(fname, attrib);
			if (hSrf) SharedTextures[name] = hSrf;
			return hSrf;
		}
		else
		{
			DWORD original = SURFACE(ent->second)->GetOAPIFlags();

			if (original & OAPISURFACE_ORIGIN)
			{
				if ((attrib & exclude) == (original & exclude))
				{
					return SURFHANDLE(new SurfNative(SURFACE(ent->second))); // Clone it
				}
			}
			else
			{
				// Create "origin" for cloning
				SURFHANDLE hSrf = NatLoadSurface(fname, attrib | OAPISURFACE_ORIGIN);
				if (hSrf) {
					ent->second = hSrf;
					return SURFHANDLE(new SurfNative(SURFACE(hSrf))); // Clone it
				}
				else return NULL;
			}
		}
		*/
	}
	
	return NatLoadSurface(fname, attrib, bPath);
}

// ==============================================================

HBITMAP D3D9Client::gcReadImageFromFile(const char *_path)
{
	char path[MAX_PATH];
	sprintf_s(path, sizeof(path), "%s\\%s", OapiExtension::GetTextureDir(), _path);
	return ReadImageFromFile(path);
}

// ==============================================================

void D3D9Client::clbkReleaseTexture(SURFHANDLE hTex)
{
	clbkReleaseSurface(hTex);
}

// ==============================================================

SURFHANDLE D3D9Client::clbkCreateSurfaceEx(DWORD w, DWORD h, DWORD attrib)
{
	_TRACE;
	if (ChkDev(__FUNCTION__)) return NULL;

#ifdef _DEBUG
	LogAttribs(attrib, w, h, "CreateSrfEx");
#endif // _DEBUG

	if (w == 0 || h == 0) return NULL;	// Inline engine returns NULL for a zero surface

	SURFHANDLE hNew = NatCreateSurface(w, h, attrib);
	SURFACE(hNew)->SetName("clbkCreateSurfaceEx");
	return hNew;
}


// =======================================================================

SURFHANDLE D3D9Client::clbkCreateSurface(DWORD w, DWORD h, SURFHANDLE hTemplate)
{
	_TRACE;
	if (ChkDev(__FUNCTION__)) return NULL;
	if (w == 0 || h == 0) return NULL;	// Inline engine returns NULL for a zero surface

	DWORD attrib = OAPISURFACE_PF_XRGB | OAPISURFACE_RENDERTARGET | OAPISURFACE_TEXTURE;

	if (hTemplate) attrib = SURFACE(hTemplate)->GetOAPIFlags();
	
	SURFHANDLE hNew = NatCreateSurface(w, h, attrib);
	SURFACE(hNew)->SetName("clbkCreateSurface");
	return hNew;
}

// =======================================================================

SURFHANDLE D3D9Client::clbkCreateSurface(HBITMAP hBmp)
{
	_TRACE;
	if (ChkDev(__FUNCTION__)) return NULL;

	SURFHANDLE hSurf = GraphicsClient::clbkCreateSurface(hBmp);
	SURFACE(hSurf)->SetName("clbkCreateSurface_HBITMAP");
	return hSurf;
}

// =======================================================================

SURFHANDLE D3D9Client::clbkCreateTexture(DWORD w, DWORD h)
{
	_TRACE;
	if (ChkDev(__FUNCTION__)) return NULL;
	if (w == 0 || h == 0) return NULL;	// Inline engine returns NULL for a zero surface

	SURFHANDLE hNew = NatCreateSurface(w, h, OAPISURFACE_PF_XRGB | OAPISURFACE_RENDERTARGET | OAPISURFACE_TEXTURE);
	SURFACE(hNew)->SetName("clbkCreateTexture");
	return hNew;
}

// =======================================================================

void D3D9Client::clbkIncrSurfaceRef(SURFHANDLE surf)
{
	_TRACE;
	if (surf) SURFACE(surf)->IncRef();
}

// =======================================================================

bool D3D9Client::clbkReleaseSurface(SURFHANDLE surf)
{
	_TRACE;
	if (ChkDev(__FUNCTION__)) return false;

	// Do not release 'origin' (i.e. reference) for cloned surfaces.
	if (SURFACE(surf)->GetOAPIFlags() & OAPISURFACE_ORIGIN) return false;

	// Do not release surfaces stored in repository
	for (auto ent : SharedTextures) if (ent.second == surf) return false;

	// Don't release surfaces used by meshes
	for (auto mesh : MeshCatalog) if (mesh && mesh->HasTexture(surf)) return false;

	// If the surface exists, delete it.
	if (SURFACE(surf)->DecRef())
	{
		if (SurfaceCatalog.count(SURFACE(surf)))
		{
			delete SURFACE(surf);
			return true;
		}
	}
	return false;
}

// =======================================================================

bool D3D9Client::clbkGetSurfaceSize(SURFHANDLE surf, DWORD *w, DWORD *h)
{
	_TRACE;
	if (!w || !h) return false;
	if (surf==NULL) surf = pFramework->GetBackBufferHandle();
	*w = SURFACE(surf)->GetWidth();
	*h = SURFACE(surf)->GetHeight();
	return true;
}

// =======================================================================

bool D3D9Client::clbkSetSurfaceColourKey(SURFHANDLE surf, DWORD ckey)
{
	_TRACE;
	if (surf==NULL) { LogErr("Surface is NULL"); return false; }
	SURFACE(surf)->SetColorKey(ckey);
	return true;
}



// =======================================================================
// Blitting functions
// =======================================================================

int D3D9Client::clbkBeginBltGroup(SURFHANDLE tgt)
{
	_TRACE;
	if (pBltGrpTgt) return -1;

	if (tgt == RENDERTGT_NONE) {
		pBltGrpTgt = NULL;
		return -2;
	}

	if (tgt == RENDERTGT_MAINWINDOW) pBltGrpTgt = pFramework->GetBackBufferHandle();
	else pBltGrpTgt = tgt;

	if (!SURFACE(tgt)->IsRenderTarget()) {
		pBltGrpTgt = NULL;
		return -3;
	}

	//pBltSkp = SURFACE(tgt)->GetPooledSketchPad();
	//pBltSkp->BeginDrawing();
	return 0;
}

// =======================================================================

int D3D9Client::clbkEndBltGroup()
{
	_TRACE;
	if (pBltGrpTgt==NULL) return -2;
	//pBltSkp->EndDrawing();
	pBltSkp = NULL;
	pBltGrpTgt = NULL;
	return 0;
}

// =======================================================================

bool D3D9Client::clbkBlt(SURFHANDLE tgt, DWORD tgtx, DWORD tgty, SURFHANDLE src, DWORD flag) const
{
	_TRACE;
	const D3DSURFACE_DESC* sd = SURFACE(src)->GetDesc();
	return clbkScaleBlt(tgt, tgtx, tgty, sd->Width, sd->Height, src, 0, 0, sd->Width, sd->Height, flag);
}

// =======================================================================

bool D3D9Client::clbkBlt(SURFHANDLE tgt, DWORD tgtx, DWORD tgty, SURFHANDLE src, DWORD srcx, DWORD srcy, DWORD w, DWORD h, DWORD flag) const
{
	_TRACE;
	return clbkScaleBlt(tgt, tgtx, tgty, w, h, src, srcx, srcy, w, h, flag);
}

// =======================================================================

bool D3D9Client::clbkScaleBlt (SURFHANDLE tgt, DWORD tgtx, DWORD tgty, DWORD tgtw, DWORD tgth,
                               SURFHANDLE src, DWORD srcx, DWORD srcy, DWORD srcw, DWORD srch, DWORD flag) const
{
	
	if (src==NULL) { oapiWriteLog((char*)"ERROR: oapiBlt() Source surface is NULL"); return false; }

	if (tgt==NULL) tgt = pFramework->GetBackBufferHandle();


	RECT rs = _RECT(srcx, srcy, srcx + srcw, srcy + srch);
	RECT rt = _RECT(tgtx, tgty, tgtx + tgtw, tgty + tgth);


	// Can't blit in a clone, declone..
	//
	if (SURFACE(tgt)->IsClone()) SURFACE(tgt)->DeClone();

	// Can't blit in a compressed surface, decompress..
	//
	if (!SURFACE(tgt)->Decompress())
	{
		HALT();
	}

	POINT tp = { (long)tgtx, (long)tgty };

	const D3DSURFACE_DESC* td = SURFACE(tgt)->GetDesc();
	const D3DSURFACE_DESC* sd = SURFACE(src)->GetDesc();


	// Check failure and abort conditions, Match with know DX7 behavior ---------------------
	//
	if (rt.right > (long)td->Width || rt.bottom > (long)td->Height) return true;
	if (rt.left < 0 || rt.top < 0)  return true;

	if (rs.right > (long)sd->Width || rs.bottom > (long)sd->Height)  return true;
	if (rs.left < 0 || rs.top < 0) return true;

	if (rs.left > rs.right) return true;
	if (rt.left > rt.right) return true;
	if (rs.top > rs.bottom) return true;
	if (rt.top > rt.bottom) return true;

	if (srcw == 0 || srch == 0 || tgtw == 0 || tgth == 0) return true;


	// Check Blt conditions
	//
	bool bCK = (SURFACE(src)->GetColorKey() != SURF_NO_CK) && (SURFACE(src)->GetColorKey() != 0);	// ColorKey In Use
	bool bCL = (srcw != tgtw) || (srch != tgth);		// Scaling In Use
	bool bSC = SURFACE(src)->IsCompressed();			// Compressed source

	LPDIRECT3DSURFACE9 pss = SURFACE(src)->GetSurface();
	LPDIRECT3DSURFACE9 pts = SURFACE(tgt)->GetSurface();



	if ((sd->Format == td->Format) && !bCK && !bSC)
	{

		// Most common case: Target is a render-target and source is in a video memory
		//
		if ((td->Usage & D3DUSAGE_RENDERTARGET) && (sd->Pool == D3DPOOL_DEFAULT))
		{
			if (src != tgt)
			{
				if (S_OK == pDevice->StretchRect(pss, &rs, pts, &rt, D3DTEXF_POINT)) return true;

				LogErr("oapiBlt() StretchRect() Failed 1");
				BltError(src, tgt, &rs, &rt);
				return false;
			}
			else
			{
				// Source and Target are the same surface, reroute through temp.
				//
				LPDIRECT3DSURFACE9 tmp = SURFACE(src)->GetTempSurface();

				if (S_OK == pDevice->StretchRect(pss, &rs, tmp, &rs, D3DTEXF_POINT))
				{
					if (S_OK == pDevice->StretchRect(tmp, &rs, pts, &rt, D3DTEXF_POINT)) return true;
				}

				LogErr("oapiBlt() StretchRect() Failed 2");
				BltError(src, tgt, &rs, &rt);
				return false;
			}
		}
	}

	if ((sd->Format == td->Format) && !bCK && !bSC && !bCL)
	{

		// Texture Update: Source is in system memory and target is a texture
		// 
		if (sd->Pool == D3DPOOL_SYSTEMMEM)
		{
			if (S_OK == pDevice->UpdateSurface(pss, &rs, pts, &tp))	return true;

			LogErr("oapiBlt() UpdateSurface() Failed");
			BltError(src, tgt, &rs, &rt);
			return false;
		}


		// Screen Capture: Target is in system memory and source is a render taeget
		// 
		if ((td->Pool == D3DPOOL_SYSTEMMEM) && (sd->Usage & D3DUSAGE_RENDERTARGET))
		{
			if (S_OK == pDevice->GetRenderTargetData(pss, pts)) {
				SURFACE(tgt)->Flags |= OAPISURFACE_CAPTURE;
				return true;
			}
		
			LogErr("oapiBlt() GetRenderTargetData() Failed");
			BltError(src, tgt, &rs, &rt);
			return false;
		}
	}


	// Scaling.. Format mismatch.. ColorKey.. Compressed Source..
	// Go for SketchPad
	//
	if (src != tgt)
	{
		if ((td->Usage & D3DUSAGE_RENDERTARGET) && (SURFACE(src)->GetType() == D3DRTYPE_TEXTURE) && (sd->Pool == D3DPOOL_DEFAULT))
		{
			Sketchpad* pSkp = clbkGetSketchpad_const(tgt);

			if (bCK)
			{
				if (bCL) ((D3D9Pad*)pSkp)->ColorKeyStretch(src, &rs, &rt);
				else pSkp->ColorKey(src, &rs, tgtx, tgty);
				clbkReleaseSketchpad_const(pSkp);
				return true;
			}
			else
			{		
				pSkp->StretchRect(src, &rs, &rt);
				clbkReleaseSketchpad_const(pSkp);
				return true;
			}
		}
	}

	LogErr("oapiBlt() Failed (End)");
	BltError(src, tgt, &rs, &rt);
	return false;
}

// =======================================================================

bool D3D9Client::clbkCopyBitmap(SURFHANDLE pdds, HBITMAP hbm, int x, int y, int dx, int dy)
{
	HDC                     hdcImage;
	HDC                     hdc;
	BITMAP                  bm;

	if (hbm == NULL || pdds == NULL) return false;

	// Select bitmap into a memoryDC so we can use it.
	//
	hdcImage = CreateCompatibleDC(NULL);

	if (!hdcImage) OutputDebugString("createcompatible dc failed\n");

	SelectObject(hdcImage, hbm);

	// Get size of the bitmap
	//
	GetObject(hbm, sizeof(bm), &bm);
	dx = dx == 0 ? bm.bmWidth : dx;     // Use the passed size, unless zero
	dy = dy == 0 ? bm.bmHeight : dy;


	// Get size of surface.
	//
	DWORD surfW = SURFACE(pdds)->GetWidth();
	DWORD surfH = SURFACE(pdds)->GetHeight();

	RECT r = { 0, 0, (long)surfW, (long)surfH };
	POINT tp = { 0, 0 };

	if (SURFACE(pdds)->IsGDISurface())
	{
		if (hdc = clbkGetSurfaceDC(pdds)) {
			StretchBlt(hdc, 0, 0, surfW, surfH, hdcImage, x, y,	dx, dy, SRCCOPY);
			clbkReleaseSurfaceDC(pdds, hdc);
		}
		DeleteDC(hdcImage);
		SURFACE(pdds)->SetName("clbkCopyBitmap");
		return true;
	}
	else 
	{
		LPDIRECT3DTEXTURE9 pTemp = NULL;
		LPDIRECT3DSURFACE9 pSrf = NULL;

		if (SURFACE(pdds)->IsRenderTarget()) pTemp = SURFACE(pdds)->GetGDICache(0);
		else								 pTemp = SURFACE(pdds)->GetGDICache(OAPISURFACE_SYSMEM);

		if (S_OK == pTemp->GetSurfaceLevel(0, &pSrf))
		{
			if (S_OK == pSrf->GetDC(&hdc))
			{
				StretchBlt(hdc, 0, 0, surfW, surfH, hdcImage, x, y, dx, dy, SRCCOPY);

				pSrf->ReleaseDC(hdc);

				if (SURFACE(pdds)->IsRenderTarget()) {
					HR(pDevice->StretchRect(pSrf, &r, SURFACE(pdds)->GetSurface(), &r, D3DTEXF_LINEAR));
				}
				else {
					HR(pDevice->UpdateSurface(pSrf, &r, SURFACE(pdds)->GetSurface(), &tp));
				}

				DeleteDC(hdcImage);
				pSrf->Release();
				SURFACE(pdds)->SetName("clbkCopyBitmap");
				return true;
			}
			else {
				pSrf->Release();
				assert(false);
			}		
		}
	}
	DeleteDC(hdcImage);
	return false;
}

// =======================================================================

bool D3D9Client::clbkFillSurface(SURFHANDLE tgt, DWORD col) const
{
	_TRACE;
	if (tgt==NULL) tgt = pFramework->GetBackBufferHandle();
	bool ret = SURFACE(tgt)->Fill(NULL, col);
	return ret;
}

// =======================================================================

bool D3D9Client::clbkFillSurface(SURFHANDLE tgt, DWORD tgtx, DWORD tgty, DWORD w, DWORD h, DWORD col) const
{
	_TRACE;
	if (tgt==NULL) tgt = pFramework->GetBackBufferHandle();
	RECT r = _RECT(tgtx, tgty, tgtx+w, tgty+h);
	bool ret = SURFACE(tgt)->Fill(&r, col);
	return ret;
}

// =======================================================================

void D3D9Client::BltError(SURFHANDLE src, SURFHANDLE tgt, const LPRECT s, const LPRECT t, bool bHalt) const
{
	LogErr("Source Rect (%d,%d,%d,%d) (w=%u,h=%u)", s->left, s->top, s->right, s->bottom, abs(s->left - s->right), abs(s->top - s->bottom));
	LogErr("Target Rect (%d,%d,%d,%d) (w=%u,h=%u)", t->left, t->top, t->right, t->bottom, abs(t->left - t->right), abs(t->top - t->bottom));
	LogErr("Source Data Below: ----------------------------------");
	SURFACE(src)->LogSpecs();
	LogErr("Target Data Below: ----------------------------------");
	SURFACE(tgt)->LogSpecs();
	if (bHalt) HALT();
}



#pragma endregion

// =======================================================================
// GDI functions
// =======================================================================

HDC D3D9Client::clbkGetSurfaceDC(SURFHANDLE surf)
{
	_TRACE;
	if (ChkDev(__FUNCTION__)) return NULL;

	if (surf == NULL) {
		if (Config->GDIOverlay) {
			LPDIRECT3DSURFACE9 pGDI = GetScene()->GetBuffer(GBUF_GDI);
			HDC hDC;
			if (pGDI) if (pGDI->GetDC(&hDC) == S_OK) {
				if (bGDIClear) {
					bGDIClear = false;
					DWORD color = 0xF08040; // BGR "Color Key" value for transparency
					HBRUSH hBrush = CreateSolidBrush((COLORREF)color);
					RECT r = _RECT( 0, 0, viewW, viewH );
					FillRect(hDC, &r, hBrush);
					DeleteObject(hBrush);
				}
				return hDC;
			}
		}
		return NULL;
	}
	HDC hDC = SURFACE(surf)->GetDC();
	return hDC;
}

// =======================================================================

void D3D9Client::clbkReleaseSurfaceDC(SURFHANDLE surf, HDC hDC)
{
	_TRACE;
	if (ChkDev(__FUNCTION__)) return;

	if (hDC == NULL) { LogErr("D3D9Client::clbkReleaseSurfaceDC() Input hDC is NULL"); return; }
	if (surf == NULL) {
		if (Config->GDIOverlay) {
			LPDIRECT3DSURFACE9 pGDI = GetScene()->GetBuffer(GBUF_GDI);
			if (pGDI) pGDI->ReleaseDC(hDC);
		}
		return;
	}
	SURFACE(surf)->ReleaseDC(hDC);
}

// =======================================================================

bool D3D9Client::clbkFilterElevation(OBJHANDLE hPlanet, int ilat, int ilng, int lvl, double elev_res, INT16* elev)
{
	_TRACE;
	return FilterElevationPhysics(hPlanet, lvl, ilat, ilng, elev_res, elev);
}
void D3D9Client::clbkImGuiNewFrame()
{
	_TRACE;
	ImGui_ImplDX9_NewFrame();
}
void D3D9Client::clbkImGuiRenderDrawData()
{
	_TRACE;

	if (pDevice->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		pDevice->EndScene();
	}

	// Update and Render additional Platform Windows
	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}
void D3D9Client::clbkImGuiInit()
{
	_TRACE;
	ImGui_ImplDX9_Init(pDevice);
}
void D3D9Client::clbkImGuiShutdown()
{
	_TRACE;
	ImGui_ImplDX9_Shutdown();
}

// =======================================================================

bool D3D9Client::clbkSplashLoadMsg (const char *msg, int line)
{
	_TRACE;
	return OutputLoadStatus (msg, line);
}

// =======================================================================

lpSurfNative D3D9Client::GetDefaultTexture() const
{
	return pDefaultTex;
}

// =======================================================================

HWND D3D9Client::GetWindow()
{
	return pFramework->GetRenderWindow();
}

// =======================================================================

SURFHANDLE D3D9Client::GetBackBufferHandle() const
{
	_TRACE;
	return pFramework->GetBackBufferHandle();
}

// =======================================================================

void D3D9Client::MakeRenderProcCall(Sketchpad *pSkp, DWORD id, LPD3DXMATRIX pV, LPD3DXMATRIX pP)
{
	for (auto it = RenderProcs.cbegin(); it != RenderProcs.cend(); ++it) {
		if (it->id == id) {
			D3D9Pad *pSkp2 = (D3D9Pad *)pSkp;
			pSkp2->LoadDefaults();
			if (id == RENDERPROC_EXTERIOR || id == RENDERPROC_PLANETARIUM) {
				pSkp2->SetViewMode(Sketchpad::USER);
			}
			pSkp2->SetViewProj(pV, pP);
			it->proc(pSkp, it->pParam);
			pSkp2->FlushAll(); // Flush render queue
		}
	}
}

// =======================================================================

void D3D9Client::MakeGenericProcCall(DWORD id, int iUser, void *pUser) const
{
	for (auto it = GenericProcs.cbegin(); it != GenericProcs.cend(); ++it) {
		if (it->id == id) it->proc(iUser, pUser, it->pParam);
	}
}

// =======================================================================

bool D3D9Client::RegisterRenderProc(__gcRenderProc proc, DWORD id, void *pParam)
{
	if (id)	{ // register (add)
		RenderProcData data = { proc, pParam, id };
		RenderProcs.push_back(data);
		return true;
	}
	else { // unregister, mark as unused (remove later)
		for (auto it = RenderProcs.begin(); it != RenderProcs.end(); ++it) {
			if (it->proc == proc) {
				it->id = 0;
				it->pParam = NULL;
				it->proc = NULL;
				return true;
			}
		}
	}
	return false;
}

// =======================================================================

bool D3D9Client::RegisterGenericProc(__gcGenericProc proc, DWORD id, void *pParam)
{
	if (id) { // register (add)
		GenericProcData data = { proc, pParam, id };
		GenericProcs.push_back(data);
		return true;
	}
	else { // unregister, mark as unused (remove later)
		for (auto it = GenericProcs.begin(); it != GenericProcs.end(); ++it) {
			if (it->proc == proc) {
				it->id = 0;
				it->pParam = NULL;
				it->proc = NULL;
				return true;
			}
		}
	}
	return false;
}

// =======================================================================

bool D3D9Client::IsGenericProcEnabled(DWORD id) const
{
	for (const auto &val : GenericProcs) if (val.id == id) return true;
	return false;
}

// =======================================================================

void D3D9Client::WriteLog(const char *msg) const
{
	_TRACE;
	char cbuf[256];
	sprintf_s(cbuf, 256, "D3D9: %s", msg);
	oapiWriteLog(cbuf);
}

// =======================================================================

bool D3D9Client::OutputLoadStatus(const char *txt, int line)
{

	if (bRunning) return false;

	if (line == 1) strcpy_s(pLoadItem, 127, txt); else
	if (line == 0) strcpy_s(pLoadLabel, 127, txt), pLoadItem[0] = '\0'; // New top line => clear 2nd line

	if (pTextScreen) {

		if (pDevice->TestCooperativeLevel()!=S_OK) {
			LogErr("TestCooperativeLevel() Failed");
			return false;
		}

		RECT txt = _RECT( loadd_x, loadd_y, loadd_x+loadd_w, loadd_y+loadd_h );

		pDevice->StretchRect(pSplashScreen, &txt, pTextScreen, NULL, D3DTEXF_POINT);

		HDC hDC;
		HR(pTextScreen->GetDC(&hDC));

		HFONT hO = (HFONT)SelectObject(hDC, hLblFont1);
		SetTextColor(hDC, pSplashTextColor);
		SetBkMode(hDC,TRANSPARENT);
		SetTextAlign(hDC, TA_LEFT|TA_TOP);

		TextOut(hDC, 2, 2, pLoadLabel, lstrlen(pLoadLabel));

		SelectObject(hDC, hLblFont2);
		TextOut(hDC, 2, 36, pLoadItem, lstrlen(pLoadItem));

		HPEN pen = CreatePen(PS_SOLID,1,pSplashTextColor);
		HPEN po = (HPEN)SelectObject(hDC, pen);

		MoveToEx(hDC, 0, 32, NULL);
		LineTo(hDC, loadd_w, 32);

		SelectObject(hDC, po);
		SelectObject(hDC, hO);
		DeleteObject(pen);

		HR(pTextScreen->ReleaseDC(hDC));
		HR(pDevice->StretchRect(pSplashScreen, NULL, pBackBuffer, NULL, D3DTEXF_POINT));
		HR(pDevice->StretchRect(pTextScreen, NULL, pBackBuffer, &txt, D3DTEXF_POINT));

		IDirect3DSwapChain9 *pSwap;

		if (pDevice->GetSwapChain(0, &pSwap)==S_OK) {
			pSwap->Present(0, 0, 0, 0, D3DPRESENT_DONOTWAIT);
			pSwap->Release();
			return true;
		}

		// Prevent "Not Responding" during loading
		MSG msg;
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessage(&msg);
	}
	return false;
}

// =======================================================================
void D3D9Client::clbkSetSplashScreen(const char *filename, DWORD textCol)
{
	pCustomSplashScreen = filename;
	pSplashTextColor = textCol;
}

void D3D9Client::SplashScreen()
{

	loadd_x = 279*viewW/1280;
	loadd_y = 545*viewH/800;
	loadd_w = viewW/3;
	loadd_h = 80;

	RECT rS;

	GetWindowRect(hRenderWnd, &rS);

	LogAlw("Splash Window Size = [%u, %u]", rS.right - rS.left, rS.bottom - rS.top);
	LogAlw("Splash Window LeftTop = [%d, %d]", rS.left, rS.top);

	HR(pDevice->TestCooperativeLevel());
	HR(pDevice->Clear(0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, 0x0, 1.0f, 0L));
	HR(pDevice->CreateOffscreenPlainSurface(loadd_w, loadd_h, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &pTextScreen, NULL));
	HR(pDevice->CreateOffscreenPlainSurface(viewW, viewH, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &pSplashScreen, NULL));


	if(pCustomSplashScreen != NULL) {
		D3DXIMAGE_INFO info;
		HR(D3DXGetImageInfoFromFile(pCustomSplashScreen, &info));

		double imageW = info.Width;
		double imageH = info.Height;

		double scale = min(viewW / imageW, viewH / imageH);
		double _w = (imageW * scale);
		double _h = (imageH * scale);
		double _l = abs(viewW - _w)/2.0;
		double _t = abs(viewH - _h)/2.0;
		RECT imgRect = {
			static_cast<LONG>( round(_l) ),
			static_cast<LONG>( round(_t) ),
			static_cast<LONG>( round(_w + _l) ),
			static_cast<LONG>( round(_h + _t) )
		};
		HR(pDevice->ColorFill(pSplashScreen, NULL, D3DCOLOR_XRGB(0, 0, 0)));
		HR(D3DXLoadSurfaceFromFile(pSplashScreen, NULL, &imgRect, pCustomSplashScreen, NULL, D3DX_FILTER_LINEAR, 0, NULL));
	} else {
		D3DXIMAGE_INFO Info;
		HMODULE hOrbiter =  GetModuleHandleA("orbiter.exe");
		HRSRC hRes = FindResourceA(hOrbiter, MAKEINTRESOURCEA(292), "IMAGE");
		HGLOBAL hImage = LoadResource(hOrbiter, hRes);
		LPVOID pData = LockResource(hImage);
		DWORD size = SizeofResource(hOrbiter, hRes);

		// Splash screen image is 1920 x 1200 pixel
		double scale = min(viewW / 1920.0, viewH / 1200.0);
		double _w = (1920.0 * scale);
		double _h = (1200.0 * scale);
		double _l = abs(viewW - _w)/2.0;
		double _t = abs(viewH - _h)/2.0;
		RECT imgRect = {
			static_cast<LONG>( round(_l) ),
			static_cast<LONG>( round(_t) ),
			static_cast<LONG>( round(_w + _l) ),
			static_cast<LONG>( round(_h + _t) )
		};
		HR(pDevice->ColorFill(pSplashScreen, NULL, D3DCOLOR_XRGB(0, 0, 0)));
		HR(D3DXLoadSurfaceFromFileInMemory(pSplashScreen, NULL, &imgRect, pData, size, NULL, D3DX_FILTER_LINEAR, 0, &Info));
	}

	HDC hDC;
	HR(pSplashScreen->GetDC(&hDC));

	LOGFONTA fnt; memset((void *)&fnt, 0, sizeof(LOGFONT));

	fnt.lfHeight		 = 18;
	fnt.lfWeight		 = 700;
	fnt.lfCharSet		 = ANSI_CHARSET;
	fnt.lfOutPrecision	 = OUT_DEFAULT_PRECIS;
	fnt.lfClipPrecision	 = CLIP_DEFAULT_PRECIS;
	fnt.lfQuality		 = ANTIALIASED_QUALITY;
	fnt.lfPitchAndFamily = DEFAULT_PITCH;
	strcpy_s(fnt.lfFaceName, "Courier New");

	HFONT hF = CreateFontIndirect(&fnt);

	HFONT hO = (HFONT)SelectObject(hDC, hF);
	SetTextColor(hDC, pSplashTextColor);
	SetBkMode(hDC,TRANSPARENT);

	const char *months[]={"???","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec","???"};

	DWORD d = oapiGetOrbiterVersion();
	DWORD y = d/10000; d-=y*10000;
	DWORD m = d/100; d-=m*100;
	if (m>12) m=0;

	char dataA[256];
	strcpy(dataA, "D3D9Client");
	if (Config->Enable9On12) strcat_s(dataA, 256, " via D3D9on12 emulator");

#ifdef _DEBUG
	strcat_s(dataA, 256, " (Debug Build)");
#else
	strcat_s(dataA, 256, " (Release Build)");
#endif

	char dataB[128]; sprintf_s(dataB,128,"Build %s %lu 20%lu [%u]", months[m], d, y, oapiGetOrbiterVersion());
	char dataD[] = { "Warning: Config folder not present in /Modules/Server/. Please create symbolic link." };
	//char dataE[] = { "Note: Cubic Interpolation is use... Consider using linear for better elevation matching" };
	//char dataF[] = { "Note: Terrain flattening offline due to cubic interpolation" };

	int xc = viewW*750/1280;
	int yc = viewH*545/800;

	TextOut(hDC, xc, yc + 0*20, "ORBITER Space Flight Simulator",30);
	TextOut(hDC, xc, yc + 1*20, dataB, lstrlen(dataB));
	TextOut(hDC, xc, yc + 2*20, dataA, lstrlen(dataA));

	DWORD VPOS = viewH - 50;
	DWORD LSPACE = 20;

	SetTextAlign(hDC, TA_CENTER);
	DWORD cattrib = GetFileAttributes("Modules/Server/Config");

	if ((cattrib&0x10)==0 || cattrib==INVALID_FILE_ATTRIBUTES) {
		TextOut(hDC, viewW/2, VPOS, dataD, lstrlen(dataD));
		VPOS -= LSPACE;
	}

	SelectObject(hDC, hO);
	DeleteObject(hF);

	HR(pSplashScreen->ReleaseDC(hDC));


	RECT src = _RECT( loadd_x, loadd_y, loadd_x+loadd_w, loadd_y+loadd_h );
	pDevice->StretchRect(pSplashScreen, &src, pTextScreen, NULL, D3DTEXF_POINT);
	pDevice->StretchRect(pSplashScreen, NULL, pBackBuffer, NULL, D3DTEXF_POINT);
	pDevice->Present(0, 0, 0, 0);
}

// =======================================================================

HRESULT D3D9Client::BeginScene()
{
	bRendering = false;
	HRESULT hr = pDevice->BeginScene();
	if (hr == S_OK) bRendering = true;
	return hr;
}

// =======================================================================

void D3D9Client::EndScene()
{
	pDevice->EndScene();
	bRendering = false;
}

// =======================================================================

#pragma region Drawing_(Sketchpad)_Interface


double sketching_time;

// =======================================================================
// 2D Drawing Interface
//
oapi::Sketchpad *D3D9Client::clbkGetSketchpad_const(SURFHANDLE surf) const
{
	if (ChkDev(__FUNCTION__)) return NULL;

	if (GetCurrentThread() != hMainThread) {
		LogErr("Sketchpad called from a worker thread !");
		HALT();
	}

	if (surf == RENDERTGT_MAINWINDOW) surf = GetBackBufferHandle();

	if (SURFACE(surf)->IsRenderTarget())
	{
		// Get Pooled Sketchpad
		D3D9Pad *pPad = SURFACE(surf)->GetPooledSketchPad();

		// Get Current interface if any
		D3D9Pad *pCur = GetTopInterface();

		// Do we have an existing SketchPad interface in use
		if (pCur) {
			if (pCur == pPad) {
				LogErr("Sketchpad already exists for this surface");
				HALT();
			}
			pCur->EndDrawing();	// Put the current one in hold
			LogDbg("Red", "Switching to another sketchpad in a middle");
		}

		// Push a new Sketchpad onto a stack
		PushSketchpad(surf, pPad);

		pPad->BeginDrawing();
		pPad->LoadDefaults();

		return pPad;
	}
	else {
		HDC hDC = SURFACE(surf)->GetDC();
		if (hDC) return new GDIPad(surf, hDC);
	}

	return NULL;
}

// =======================================================================
// 2D Drawing Interface
//
oapi::Sketchpad* D3D9Client::clbkGetSketchpad(SURFHANDLE surf)
{
	return clbkGetSketchpad_const(surf);
}

// =======================================================================

void D3D9Client::clbkReleaseSketchpad_const(oapi::Sketchpad* sp) const
{
	_TRACE;
	if (ChkDev(__FUNCTION__)) return;

	if (!sp) return;

	SURFHANDLE hSrf = sp->GetSurface();

	if (SURFACE(hSrf)->IsRenderTarget()) {

		D3D9Pad* pPad = ((D3D9Pad*)sp);

		if (GetTopInterface() != pPad) {
			LogErr("Sketchpad release failed. Not a top one.");
			HALT();
		}

		pPad->EndDrawing();

		PopRenderTargets();

		// Do we have an old interface ?
		D3D9Pad* pOld = GetTopInterface();
		if (pOld) {
			pOld->BeginDrawing();	// Continue with the old one
			LogDbg("Red", "Continue Previous Sketchpad");
		}
	}
	else {
		GDIPad* pGDI = (GDIPad*)sp;
		SURFACE(hSrf)->ReleaseDC(pGDI->GetDC());
		delete pGDI;
	}
}

// =======================================================================

void D3D9Client::clbkReleaseSketchpad(oapi::Sketchpad *sp)
{
	clbkReleaseSketchpad_const(sp);
}

// =======================================================================

Font *D3D9Client::clbkCreateFont(int height, bool prop, const char *face, FontStyle style, int orientation) const
{
	_TRACE;
	if (ChkDev(__FUNCTION__)) return NULL;
	return *g_fonts.insert(new D3D9PadFont(height, prop, face, style, orientation)).first;
}

Font* D3D9Client::clbkCreateFontEx(int height, char* face, int width, int weight, FontStyle style, float spacing) const
{
	_TRACE;
	if (ChkDev(__FUNCTION__)) return NULL;
	return *g_fonts.insert(new D3D9PadFont(height, face, width, weight, style, spacing)).first;
}


// =======================================================================

void D3D9Client::clbkReleaseFont(Font *font) const
{
	_TRACE;
	if (!g_fonts.count(font)) return;
	g_fonts.erase(font);
	delete ((D3D9PadFont*)font);
}

// =======================================================================

Pen *D3D9Client::clbkCreatePen(int style, int width, DWORD col) const
{
	_TRACE;
	return *g_pens.insert(new D3D9PadPen(style, width, col)).first;
}

// =======================================================================

void D3D9Client::clbkReleasePen(Pen *pen) const
{
	_TRACE;
	if (!g_pens.count(pen)) return;
	g_pens.erase(pen);
	delete ((D3D9PadPen*)pen);
}

// =======================================================================

Brush *D3D9Client::clbkCreateBrush(DWORD col) const
{
	_TRACE;
	return *g_brushes.insert(new D3D9PadBrush(col)).first;
}

// =======================================================================

void D3D9Client::clbkReleaseBrush(Brush *brush) const
{
	_TRACE;
	if (!g_brushes.count(brush)) return;
	g_brushes.erase(brush);
	delete ((D3D9PadBrush*)brush);
}

#pragma endregion


// ======================================================================
// class VisObject

VisObject::VisObject(OBJHANDLE hObj) : hObj(hObj)
{
	_TRACE;
}

// =======================================================================

VisObject::~VisObject ()
{
}

// ===================================================
// Copyright (C) 2021 Jarmo Nikkanen
// licensed under LGPL v2
// ===================================================


#include <d3d9.h>
#include <d3dx9.h>
#include "gcCore.h"
#include "D3D9Surface.h"
#include "D3D9Client.h"
#include "Scene.h"
#include "VVessel.h"
#include "VPlanet.h"
#include "Surfmgr2.h"
#include "IProcess.h"

extern D3D9Client *g_client;
extern std::set<Font *> g_fonts;

class gcSwap
{
public:
	gcSwap() : pSwap(NULL), hSurf(NULL), pBack(NULL) { }
	~gcSwap() { Release(); }
	void Release() {
		DELETE_SURFACE(hSurf);
		SAFE_RELEASE(pBack);
		SAFE_RELEASE(pSwap);
	}
	LPDIRECT3DSWAPCHAIN9 pSwap;
	LPDIRECT3DSURFACE9 pBack;
	SURFHANDLE hSurf;
};



DLLCLBK void gcBindCoreMethod(void** ppFnc, const char* name)
{
	*ppFnc = NULL;
#define binder_start
	if (strcmp(name,"RegisterSwap")==0) *ppFnc = &gcCore2::RegisterSwap;
	if (strcmp(name,"FlipSwap")==0) *ppFnc = &gcCore2::FlipSwap;
	if (strcmp(name,"GetRenderTarget")==0) *ppFnc = &gcCore2::GetRenderTarget;
	if (strcmp(name,"ReleaseSwap")==0) *ppFnc = &gcCore2::ReleaseSwap;
	if (strcmp(name,"DeleteCustomCamera")==0) *ppFnc = &gcCore2::DeleteCustomCamera;
	if (strcmp(name,"CustomCameraOnOff")==0) *ppFnc = &gcCore2::CustomCameraOnOff;
	if (strcmp(name,"CustomCameraOverlay")==0) *ppFnc = &gcCore2::CustomCameraOverlay;
	if (strcmp(name,"SetupCustomCamera")==0) *ppFnc = &gcCore2::SetupCustomCamera;
	if (strcmp(name,"SketchpadVersion")==0) *ppFnc = &gcCore2::SketchpadVersion;
	if (strcmp(name,"CreatePoly")==0) *ppFnc = &gcCore2::CreatePoly;
	if (strcmp(name,"CreateTriangles")==0) *ppFnc = &gcCore2::CreateTriangles;
	if (strcmp(name,"DeletePoly")==0) *ppFnc = &gcCore2::DeletePoly;
	if (strcmp(name,"GetTextLength")==0) *ppFnc = &gcCore2::GetTextLength;
	if (strcmp(name,"GetCharIndexByPosition")==0) *ppFnc = &gcCore2::GetCharIndexByPosition;
	if (strcmp(name,"RegisterRenderProc")==0) *ppFnc = &gcCore2::RegisterRenderProc;
	if (strcmp(name,"CreateSketchpadFont")==0) *ppFnc = &gcCore2::CreateSketchpadFont;
	if (strcmp(name,"GetMeshMaterial")==0) *ppFnc = &gcCore2::GetMeshMaterial;
	if (strcmp(name,"SetMeshMaterial")==0) *ppFnc = &gcCore2::SetMeshMaterial;
	if (strcmp(name,"GetMatrix")==0) *ppFnc = &gcCore2::GetMatrix;
	if (strcmp(name,"SetMatrix")==0) *ppFnc = &gcCore2::SetMatrix;
	if (strcmp(name,"GetDevMesh")==0) *ppFnc = &gcCore2::GetDevMesh;
	if (strcmp(name,"LoadDevMeshGlobal")==0) *ppFnc = &gcCore2::LoadDevMeshGlobal;
	if (strcmp(name,"ReleaseDevMesh")==0) *ppFnc = &gcCore2::ReleaseDevMesh;
	if (strcmp(name,"RenderMesh")==0) *ppFnc = &gcCore2::RenderMesh;
	if (strcmp(name,"PickMesh")==0) *ppFnc = &gcCore2::PickMesh;
	if (strcmp(name,"RenderLines")==0) *ppFnc = &gcCore2::RenderLines;
	if (strcmp(name,"GetSystemSpecs")==0) *ppFnc = &gcCore2::GetSystemSpecs;
	if (strcmp(name,"GetSurfaceSpecs")==0) *ppFnc = &gcCore2::GetSurfaceSpecs;
	if (strcmp(name,"LoadSurface")==0) *ppFnc = &gcCore2::LoadSurface;
	if (strcmp(name,"SaveSurface")==0) *ppFnc = &gcCore2::SaveSurface;
	if (strcmp(name,"GetMipSublevel")==0) *ppFnc = &gcCore2::GetMipSublevel;
	if (strcmp(name,"GenerateMipmaps")==0) *ppFnc = &gcCore2::GenerateMipmaps;
	if (strcmp(name,"CompressSurface")==0) *ppFnc = &gcCore2::CompressSurface;
	if (strcmp(name,"LoadBitmapFromFile")==0) *ppFnc = &gcCore2::LoadBitmapFromFile;
	if (strcmp(name,"GetRenderWindow")==0) *ppFnc = &gcCore2::GetRenderWindow;
	if (strcmp(name,"RegisterGenericProc")==0) *ppFnc = &gcCore2::RegisterGenericProc;
	if (strcmp(name,"StretchRectInScene")==0) *ppFnc = &gcCore2::StretchRectInScene;
	if (strcmp(name,"ClearSurfaceInScene")==0) *ppFnc = &gcCore2::ClearSurfaceInScene;
	if (strcmp(name,"ScanScreen")==0) *ppFnc = &gcCore2::ScanScreen;
	if (strcmp(name,"LockSurface")==0) *ppFnc = &gcCore2::LockSurface;
	if (strcmp(name,"ReleaseLock")==0) *ppFnc = &gcCore2::ReleaseLock;
	if (strcmp(name,"GetPlanetManager")==0) *ppFnc = &gcCore2::GetPlanetManager;
	if (strcmp(name,"SetTileOverlay")==0) *ppFnc = &gcCore2::SetTileOverlay;
	if (strcmp(name,"AddGlobalOverlay")==0) *ppFnc = &gcCore2::AddGlobalOverlay;
	if (strcmp(name,"GetTileData")==0) *ppFnc = &gcCore2::GetTileData;
	if (strcmp(name,"GetTile")==0) *ppFnc = &gcCore2::GetTile;
	if (strcmp(name,"HasTileData")==0) *ppFnc = &gcCore2::HasTileData;
	if (strcmp(name,"SeekTileTexture")==0) *ppFnc = &gcCore2::SeekTileTexture;
	if (strcmp(name,"SeekTileElevation")==0) *ppFnc = &gcCore2::SeekTileElevation;
	if (strcmp(name,"GetElevation")==0) *ppFnc = &gcCore2::GetElevation;
	if (strcmp(name,"CreateIPInterface")==0) *ppFnc = &gcCore2::CreateIPInterface;
	if (strcmp(name,"ReleaseIPInterface")==0) *ppFnc = &gcCore2::ReleaseIPInterface;
#define binder_end
	if (*ppFnc == NULL) oapiWriteLogV("ERROR:gcCoreAPI: Function [%s] failed to bind", name);
}


// ===============================================================================================
// Custom SwapChain Interface
// ===============================================================================================
//
HSWAP gcCore::RegisterSwap(HWND hWnd, HSWAP hData, int AA) 
{ 
	gcSwap * pData = (gcSwap*)hData;

	D3DPRESENT_PARAMETERS pp;
	memset(&pp, 0, sizeof(D3DPRESENT_PARAMETERS));

	pp.BackBufferWidth = 0;
	pp.BackBufferHeight = 0;
	pp.BackBufferFormat = D3DFMT_X8R8G8B8;
	pp.BackBufferCount = 1;
	pp.MultiSampleType = D3DMULTISAMPLE_NONE;
	pp.MultiSampleQuality = 0;
	pp.SwapEffect = D3DSWAPEFFECT_FLIP;
	pp.hDeviceWindow = hWnd;
	pp.Windowed = true;
	pp.EnableAutoDepthStencil = false;
	pp.AutoDepthStencilFormat = D3DFMT_D24S8;
	pp.Flags = 0;
	pp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	LPDIRECT3DDEVICE9 pDev = g_client->GetDevice();
	LPDIRECT3DSWAPCHAIN9 pSwap = NULL;

	if (pDev->CreateAdditionalSwapChain(&pp, &pSwap) == S_OK)
	{
		if (!pData) pData = new gcSwap();
		else pData->Release();
		
		LPDIRECT3DSURFACE9 pBack = NULL;
		HR(pSwap->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &pBack));

		SurfNative *pSrf = new SurfNative(pBack, OAPISURFACE_BACKBUFFER | OAPISURFACE_RENDER3D | OAPISURFACE_RENDERTARGET);
		pSrf->SetName("SwapChainBackBuffer");

		pData->hSurf = SURFHANDLE(pSrf);
		pData->pSwap = pSwap;
		pData->pBack = pBack;

		return HSWAP(pData);
	}
	else {
		LogErr("Failed to create a swapchain. (Feature not supported in true-fullscreen mode)");
		return NULL;
	}
}


// ===============================================================================================
//
void gcCore::FlipSwap(HSWAP hSwap) 
{ 
	HR(((gcSwap*)hSwap)->pSwap->Present(0, 0, 0, 0, 0));
}


// ===============================================================================================
//
SURFHANDLE gcCore::GetRenderTarget(HSWAP hSwap) 
{ 
	return ((gcSwap*)hSwap)->hSurf;
}

// ===============================================================================================
//
void gcCore::ReleaseSwap(HSWAP hSwap) 
{ 
	if (hSwap) delete ((gcSwap*)hSwap);
}






// ===============================================================================================
// Custom Camera Interface
// ===============================================================================================
//
CAMERAHANDLE gcCore::SetupCustomCamera(CAMERAHANDLE hCam, OBJHANDLE hVessel, VECTOR3 &pos, VECTOR3 &dir, VECTOR3 &up, double fov, SURFHANDLE hSurf, DWORD flags)
{
	VECTOR3 x = crossp(up, dir);
	MATRIX3 mTake;
	mTake.m11 = x.x;	mTake.m21 = x.y;	mTake.m31 = x.z;
	mTake.m12 = up.x;	mTake.m22 = up.y;	mTake.m32 = up.z;
	mTake.m13 = dir.x;	mTake.m23 = dir.y;	mTake.m33 = dir.z;
	Scene *pScene = g_client->GetScene();
	return pScene ? pScene->SetupCustomCamera(hCam, hVessel, mTake, pos, fov, hSurf, flags) : NULL;
}


// ===============================================================================================
//
void gcCore::CustomCameraOnOff(CAMERAHANDLE hCam, bool bOn)
{
	Scene *pScene = g_client->GetScene();
	if (pScene) {
		pScene->CustomCameraOnOff(hCam, bOn);
	}
}


// ===============================================================================================
//
void gcCore::CustomCameraOverlay(CAMERAHANDLE hCam, __gcRenderProc clbk, void *pUser)
{
	CAMERA(hCam)->pRenderProc = clbk;
	CAMERA(hCam)->pUser = pUser;
}


// ===============================================================================================
//
int gcCore::DeleteCustomCamera(CAMERAHANDLE hCam)
{
	Scene *pScene = g_client->GetScene();
	return pScene ? pScene->DeleteCustomCamera(hCam) : 0;
}







// ===============================================================================================
// SketchPad Interface
// ===============================================================================================


// ===============================================================================================
//
int gcCore::SketchpadVersion(Sketchpad* pSkp)
{
	return ((D3D9Pad*)pSkp)->GetVersion();
}


// ===============================================================================================
//
oapi::Font* gcCore::CreateSketchpadFont(int height, char* face, int width, int weight, FontStyle Style, float spacing)
{
	return g_client->clbkCreateFontEx(height, face, width, weight, Style, spacing);
}


// ===============================================================================================
//
HPOLY gcCore::CreatePoly(HPOLY hPoly, const FVECTOR2 *pt, int npt, DWORD flags)
{
	LPDIRECT3DDEVICE9 pDev = g_client->GetDevice();
	if (!hPoly) return new D3D9PolyLine(pDev, pt, npt, (flags&PF_CONNECT) != 0);
	((D3D9PolyLine *)hPoly)->Update(pt, npt, (flags&PF_CONNECT) != 0);
	return hPoly;
}


// ===============================================================================================
//
HPOLY gcCore::CreateTriangles(HPOLY hPoly, const gcCore::clrVtx *pt, int npt, DWORD flags)
{
	LPDIRECT3DDEVICE9 pDev = g_client->GetDevice();
	if (!hPoly) return new D3D9Triangle(pDev, pt, npt, flags);
	((D3D9Triangle *)hPoly)->Update(pt, npt);
	return hPoly;
}


// ===============================================================================================
//
void gcCore::DeletePoly(HPOLY hPoly)
{
	if (hPoly) {
		((D3D9PolyBase *)hPoly)->Release();
		delete ((D3D9PolyBase *)hPoly);
	}
}


// ===============================================================================================
//
DWORD gcCore::GetTextLength(oapi::Font *hFont, const char *pText, int len)
{
	return DWORD((static_cast<D3D9PadFont *>(hFont))->GetTextLength(pText, len));
}


// ===============================================================================================
//
DWORD gcCore::GetCharIndexByPosition(oapi::Font *hFont, const char *pText, int pos, int len)
{
	return DWORD((static_cast<D3D9PadFont *>(hFont))->GetIndexByPosition(pText, pos, len));
}


// ===============================================================================================
//
bool gcCore::RegisterRenderProc(__gcRenderProc proc, DWORD flags, void *pParam)
{
	return g_client->RegisterRenderProc(proc, flags, pParam);
}




// ===============================================================================================
// Mesh interface functions
// ===============================================================================================
//
int gcCore::GetMatrix(MatrixId matrix_id, OBJHANDLE hVessel, DWORD mesh, DWORD group, FMATRIX4 *pMat)
{
	if (oapiGetObjectType(hVessel) != OBJTP_VESSEL) return -10;
	Scene *pScn = g_client->GetScene();
	vVessel *pVes = (vVessel *)pScn->GetVisObject(hVessel);
	if (pVes) return pVes->GetMatrixTransform(matrix_id, mesh, group, pMat);
	return -11;
}


// ===============================================================================================
//
int gcCore::SetMatrix(MatrixId matrix_id, OBJHANDLE hVessel, DWORD mesh, DWORD group, const FMATRIX4 *pMat)
{
	if (oapiGetObjectType(hVessel) != OBJTP_VESSEL) return -10;
	Scene *pScn = g_client->GetScene();
	vVessel *pVes = (vVessel *)pScn->GetVisObject(hVessel);
	if (pVes) return pVes->SetMatrixTransform(matrix_id, mesh, group, pMat);
	return -11;
}


// ===============================================================================================
//
int gcCore::GetMeshMaterial(DEVMESHHANDLE hMesh, DWORD idx, MatProp prop, FVECTOR4* value)
{
	return g_client->clbkMeshMaterialEx(hMesh, idx, prop, value);
}


// ===============================================================================================
//
int gcCore::SetMeshMaterial(DEVMESHHANDLE hMesh, DWORD idx, MatProp prop, const FVECTOR4* value)
{
	return g_client->clbkSetMeshMaterialEx(hMesh, idx, prop, value);
}

// ===============================================================================================
//
DEVMESHHANDLE gcCore::GetDevMesh(MESHHANDLE hMesh)
{
	return g_client->GetDevMesh(hMesh);
}


// ===============================================================================================
//
DEVMESHHANDLE gcCore::LoadDevMeshGlobal(const char* file_name, bool bUseCache)
{
	MESHHANDLE hMesh = oapiLoadMeshGlobal(file_name);
	return g_client->GetDevMesh(hMesh);
}


// ===============================================================================================
//
void gcCore::ReleaseDevMesh(DEVMESHHANDLE hMesh)
{
	delete (D3D9Mesh*)(hMesh);
}


// ===============================================================================================
//
void gcCore::RenderMesh(DEVMESHHANDLE hMesh, const oapi::FMATRIX4* pWorld)
{
	Scene* pScene = g_client->GetScene();
	pScene->RenderMesh(hMesh, pWorld);
}


// ===============================================================================================
//
bool gcCore::PickMesh(PickMeshStruct* pm, DEVMESHHANDLE hMesh, const FMATRIX4* pWorld, short x, short y)
{
	Scene* pScene = g_client->GetScene();
	D3D9Pick pk = pScene->PickMesh(hMesh, (const LPD3DXMATRIX)pWorld, x, y);
	if (pk.group >= 0) {
		if (pk.dist < pm->dist) {
			pm->pos = _FV(pk.pos);
			pm->normal = _FV(pk.normal);
			pm->grp_inst = pk.group;
			pm->dist = pk.dist;
			return true;
		}
	}
	return false;
}







// ===============================================================================================
// Custom Render Interface
// ===============================================================================================
//
// ===============================================================================================
//
SURFHANDLE gcCore::LoadSurface(const char* fname, DWORD flags)
{
	return g_client->clbkLoadSurface(fname, flags);
}

// ===============================================================================================
//
bool gcCore::SaveSurface(const char* file, SURFHANDLE hSrf)
{
	return NatSaveSurface(file, SURFACE(hSrf)->GetResource());
}

// ===============================================================================================
//
SURFHANDLE gcCore::GetMipSublevel(SURFHANDLE hSrf, int level)
{
	return NatGetMipSublevel(hSrf, level);
}

// ===============================================================================================
//
bool gcCore::GenerateMipmaps(SURFHANDLE hSurface)
{
	return NatGenerateMipmaps(hSurface);
}

// ===============================================================================================
//
SURFHANDLE gcCore::CompressSurface(SURFHANDLE hSurface, DWORD flags)
{
	return NatCompressSurface(hSurface, flags);
}


// ===============================================================================================
//
void gcCore::RenderLines(const FVECTOR3* pVtx, const WORD* pIdx, int nVtx, int nIdx, const FMATRIX4* pWorld, DWORD color)
{
	D3D9Effect::RenderLines((const D3DXVECTOR3*)pVtx, pIdx, nVtx, nIdx, (const D3DXMATRIX*)pWorld, color);
}


// ===============================================================================================
//
bool gcCore::StretchRectInScene(SURFHANDLE tgt, SURFHANDLE src, LPRECT tr, LPRECT sr)
{
	if (S_OK == g_client->BeginScene())
	{
		LPDIRECT3DSURFACE9 pss = SURFACE(src)->GetSurface();
		LPDIRECT3DSURFACE9 pts = SURFACE(tgt)->GetSurface();
		HRESULT hr = g_client->GetDevice()->StretchRect(pss, sr, pts, tr, D3DTEXF_LINEAR);
		g_client->EndScene();
		return hr == S_OK;
	}
	return false;
}

// ===============================================================================================
//
bool gcCore::ClearSurfaceInScene(SURFHANDLE tgt, DWORD color, LPRECT tr)
{
	if (S_OK == g_client->BeginScene())
	{
		LPDIRECT3DSURFACE9 pts = SURFACE(tgt)->GetSurface();
		HRESULT hr = g_client->GetDevice()->ColorFill(pts, tr, (D3DCOLOR)color);
		g_client->EndScene();
		return hr == S_OK;
	}
	return false;
}





// ===============================================================================================
// Some Helper Functions
// ===============================================================================================
//
// ===============================================================================================
//
gcCore::PickGround gcCore::ScanScreen(int scr_x, int scr_y)
{
	PickGround pg; memset(&pg, 0, sizeof(PickGround));

	Scene* pScene = g_client->GetScene();
	TILEPICK tp = pScene->PickSurface(scr_x, scr_y);
	SurfTile* pTile = static_cast<SurfTile*>(tp.pTile);

	if (pTile) {

		pTile->GetIndex(&pg.iLng, &pg.iLat);

		pg.Bounds.left = pTile->bnd.minlng;
		pg.Bounds.right = pTile->bnd.maxlng;
		pg.Bounds.top = pTile->bnd.maxlat;
		pg.Bounds.bottom = pTile->bnd.minlat;

		pg.lat = tp.lat;
		pg.lng = tp.lng;

		pg.emax = float(pTile->GetMaxElev());
		pg.emin = float(pTile->GetMinElev());

		pg.msg = 0;
		pg.dist = tp.d;
		pg.elev = tp.elev;
		pg.level = pTile->Level();
		pg.hTile = HTILE(pTile);
		pg.normal = _FV(tp._n);
		pg.pos = _FV(tp._p);
	}
	return pg;
}


// ===============================================================================================
//
void gcCore::GetSystemSpecs(SystemSpecs* sp, int size)
{
	if (size == sizeof(SystemSpecs)) {
		sp->DisplayMode = g_client->GetFramework()->GetDisplayMode();
		sp->MaxTexSize = g_client->GetHardwareCaps()->MaxTextureWidth;
		sp->MaxTexRep = g_client->GetHardwareCaps()->MaxTextureRepeat;
		sp->gcAPIVer = BuildDate();
	}
}

// ===============================================================================================
//
bool gcCore::GetSurfaceSpecs(SURFHANDLE hSrf, SurfaceSpecs* sp, int size)
{
	return SURFACE(hSrf)->GetSpecs(sp, size);
}


// ===============================================================================================
//
bool gcCore::RegisterGenericProc(__gcGenericProc proc, DWORD id, void* pParam)
{
	return g_client->RegisterGenericProc(proc, id, pParam);
}


// ===============================================================================================
//
HBITMAP	gcCore::LoadBitmapFromFile(const char* fname)
{
	return g_client->gcReadImageFromFile(fname);
}


// ===============================================================================================
//
HWND gcCore::GetRenderWindow()
{
	return g_client->GetRenderWindow();
}


// ===============================================================================================
// gcCore2 Interface --- Tile access interface functions
// ===============================================================================================
//

HPLANETMGR gcCore2::GetPlanetManager(OBJHANDLE hPlanet)
{
	Scene *pScene = g_client->GetScene();
	vPlanet *vPl = (vPlanet *)pScene->GetVisObject(hPlanet);
	return HPLANETMGR(vPl);
}


// ===============================================================================================
//
HTILE gcCore2::GetTile(HPLANETMGR vPl, double lng, double lat, int maxlevel)
{
	vPlanet *vP = static_cast<vPlanet *>(vPl);
	return HTILE(vP->FindTile(lng, lat, maxlevel));
}


// ===============================================================================================
//
gcCore::PickGround gcCore2::GetTileData(HPLANETMGR vPl, double lng, double lat, int maxlevel)
{
	PickGround pg; memset(&pg, 0, sizeof(PickGround));
	if (!vPl) return pg;

	vPlanet *vP = static_cast<vPlanet *>(vPl);
	SurfTile *pTile = static_cast<SurfTile *>(vP->FindTile(lng, lat, maxlevel));

	if (!pTile) {
		oapiWriteLogV("gcCore::FindTile() Failed");
		return pg;
	}

	pTile->GetIndex(&pg.iLng, &pg.iLat);
	pTile->GetElevation(lng, lat, &pg.elev, &pg.normal, NULL, true, false);

	VECTOR3 pos = vP->GetUnitSurfacePos(lng, lat) * (vP->GetSize() + pg.elev);
	MATRIX3 mRot; oapiGetRotationMatrix(vP->GetObjHandle(), &mRot);

	pos = mul(mRot, pos) + vP->PosFromCamera();

	pg.Bounds.left = pTile->bnd.minlng;
	pg.Bounds.right = pTile->bnd.maxlng;
	pg.Bounds.top = pTile->bnd.maxlat;
	pg.Bounds.bottom = pTile->bnd.minlat;

	pg.lat = lat;
	pg.lng = lng;

	pg.emax = float(pTile->GetMaxElev());
	pg.emin = float(pTile->GetMinElev());

	pg.msg = 0;
	pg.dist = length(pos);
	pg.level = pTile->Level();
	pg.hTile = HTILE(pTile);
	pg.pos = FVECTOR3(pos);

	return pg;
}

// ===============================================================================================
//
bool gcCore2::SeekTileElevation(HPLANETMGR hMgr, int iLng, int iLat, int level, int flags, ElevInfo *pInfo)
{
	ELEVFILEHEADER hdr;
	if (!hMgr) return false;
	if (((vPlanet*)(hMgr))->SurfMgr2()) {
		float* pData = ((vPlanet*)(hMgr))->SurfMgr2()->BrowseElevationData(level, iLat, iLng, flags, &hdr);
		if (!pData) return false;
		pInfo->MaxElev = hdr.emax;
		pInfo->MinElev = hdr.emin;
		pInfo->MeanElev = hdr.emean;
		pInfo->Resolution = hdr.scale;
		pInfo->Offset = hdr.offset;
		pInfo->pElevData = pData;
		return true;
	}
	return false;
}


// ===============================================================================================
//
SURFHANDLE gcCore2::SeekTileTexture(HPLANETMGR hMgr, int iLng, int iLat, int level, int flags, void *reserved)
{
	if (!hMgr) return NULL;
	if (((vPlanet *)(hMgr))->SurfMgr2()) {
		return ((vPlanet *)(hMgr))->SurfMgr2()->SeekTileTexture(iLng, iLat, level, flags);
	}
	return NULL;
}


// ===============================================================================================
//
bool gcCore2::HasTileData(HPLANETMGR hMgr, int iLng, int iLat, int level, int flags)
{
	if (!hMgr) return false;
	if (((vPlanet *)(hMgr))->SurfMgr2()) {
		return ((vPlanet *)(hMgr))->SurfMgr2()->HasTileData(iLng, iLat, level, flags);
	}
	return false;
}


// ===============================================================================================
//
int gcCore2::GetElevation(HTILE hTile, double lng, double lat, double *out_elev)
{
	SurfTile *pTile = static_cast<SurfTile *>(hTile);
	return pTile->GetElevation(lng, lat, out_elev, NULL, NULL, true, true);
}


// ===============================================================================================
//
SURFHANDLE gcCore2::SetTileOverlay(HTILE hTile, const SURFHANDLE hOverlay)
{
	//SurfTile *pTile = static_cast<SurfTile *>(hTile);
	//LPDIRECT3DTEXTURE9 pTex = static_cast<LPDIRECT3DTEXTURE9>(hOverlay);
	//return HSURFNATIVE(pTile->SetOverlay(pTex, true));
	return NULL;
}


// ===============================================================================================
//
HOVERLAY gcCore2::AddGlobalOverlay(HPLANETMGR hMgr, VECTOR4 mmll, OlayType type, const SURFHANDLE hOverlay, HOVERLAY hOld, const FVECTOR4* pBlend)
{
	if (!hMgr) return NULL;
	vPlanet *vP = static_cast<vPlanet *>(hMgr);
	vPlanet::sOverlay* oLay = static_cast<vPlanet::sOverlay*>(hOld);
	if (hOverlay) {
		LPDIRECT3DTEXTURE9 pTex = SURFACE(hOverlay)->GetTexture();
		return vP->AddOverlaySurface(mmll, type, pTex, oLay, pBlend);
	}
	return vP->AddOverlaySurface(mmll, type, NULL, oLay, pBlend);
}

// ===============================================================================================
//
bool gcCore::LockSurface(SURFHANDLE hSrf, Lock* pOut, bool bWait)
{
	D3DLOCKED_RECT lock;
	LPDIRECT3DRESOURCE9 pResource = SURFACE(hSrf)->GetResource();
	DWORD flags = 0;
	if (!bWait) flags |= D3DLOCK_DONOTWAIT;

	if (pResource->GetType() == D3DRTYPE_SURFACE) {
		LPDIRECT3DSURFACE9 pSurf = static_cast<LPDIRECT3DSURFACE9>(hSrf);
		if (HROK(pSurf->LockRect(&lock, NULL, flags))) {
			pOut->pData = lock.pBits;
			pOut->Pitch = lock.Pitch;
			return true;
		}
		return false;
	}

	if (pResource->GetType() == D3DRTYPE_TEXTURE) {
		LPDIRECT3DTEXTURE9 pTex = static_cast<LPDIRECT3DTEXTURE9>(hSrf);
		if (HROK(pTex->LockRect(0, &lock, NULL, flags))) {
			pOut->pData = lock.pBits;
			pOut->Pitch = lock.Pitch;
			return true;
		}
		return false;
	}

	return false;
}


// ===============================================================================================
//
void gcCore::ReleaseLock(SURFHANDLE hSrf)
{
	LPDIRECT3DRESOURCE9 pResource = SURFACE(hSrf)->GetResource();
	if (pResource->GetType() == D3DRTYPE_SURFACE) {
		LPDIRECT3DSURFACE9 pSurf = static_cast<LPDIRECT3DSURFACE9>(hSrf);
		HR(pSurf->UnlockRect());
	}
	if (pResource->GetType() == D3DRTYPE_TEXTURE) {
		LPDIRECT3DTEXTURE9 pTex = static_cast<LPDIRECT3DTEXTURE9>(hSrf);
		HR(pTex->UnlockRect(0));
	}
}

// ===============================================================================================
//
gcIPInterface* gcCore2::CreateIPInterface(const char* file, const char* PSEntry, const char* VSEntry, const char* ppf)
{
	ImageProcessing* pIPI = new ImageProcessing(g_client->GetDevice(), file, PSEntry, ppf);

	if (pIPI->IsOK() == false) {
		oapiWriteLogV("gcCore::CreateIPInterface() Failed !  File = [%s]", file);
		return NULL;
	}
	return new gcIPInterface(pIPI);
}

// ===============================================================================================
//
void gcCore2::ReleaseIPInterface(gcIPInterface* pIPI)
{
	if (!pIPI) return;
	if (pIPI->pIPI) delete pIPI->pIPI;
	delete pIPI;
}


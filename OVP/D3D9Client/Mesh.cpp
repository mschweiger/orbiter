// ==============================================================
// Mesh.cpp
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2006 - 2016 Martin Schweiger
//				 2010 - 2022 Jarmo Nikkanen (D3D9Client implementation)
// ==============================================================

#define VISIBILITY_TOL 0.0015f

#include "Mesh.h"
#include "Log.h"
#include "Scene.h"
#include "D3D9Surface.h"
#include "D3D9Catalog.h"
#include "D3D9Config.h"
#include "DebugControls.h"
#include "VectorHelpers.h"

#pragma warning(push)
#pragma warning(disable : 4838)
#include <xnamath.h>
#pragma warning(pop)

using namespace oapi;

LPDIRECT3DTEXTURE9 D3D9Mesh::pShadowMap[SHM_CASCADE_COUNT] = {};
FVECTOR4 D3D9Mesh::ShdSubRect[SHM_CASCADE_COUNT] = {};
MeshShader* D3D9Mesh::s_pShader[16] = {};

MeshShader::VSConst MeshShader::vs_const = {};
MeshShader::PSConst MeshShader::ps_const = {};
MeshShader::PSBools MeshShader::ps_bools = {};


int compare_lights(const void * a, const void * b)
{
	float fa = static_cast<const _LightList*>(a)->illuminace;
	float fb = static_cast<const _LightList*>(b)->illuminace;
	if (fa < fb * 0.9995f) return  1;
	if (fa > fb * 1.0005f) return -1;
	return 0;
}



// ======================================================================================
// Buffer Object Implementation
// ======================================================================================
//

MeshBuffer::MeshBuffer(DWORD _nVtx, DWORD _nFace, const class D3D9Mesh *_pRoot)
{
	nVtx = _nVtx;
	nIdx = _nFace * 3;
	nRef = 1;

	pVB = NULL;
	pIB = NULL;
	pGB = NULL;
	pSB = NULL;

	pVBSys = new NMVERTEX[nVtx];
	pIBSys = new WORD[nIdx];
	pGBSys = new D3DXVECTOR4[nVtx];
	pSBSys = new SMVERTEX[nVtx];

	pRoot = _pRoot;
	mapMode = MAPMODE_STATIC;
	bMustRemap = true;
}


MeshBuffer::MeshBuffer(MeshBuffer *pSrc, const class D3D9Mesh *_pRoot)
{
	nVtx = pSrc->nVtx;
	nIdx = pSrc->nIdx;
	nRef = 1;

	pVB = NULL;
	pIB = NULL;
	pGB = NULL;
	pSB = NULL;

	pVBSys = new NMVERTEX[nVtx];
	pIBSys = new WORD[nIdx];
	pGBSys = new D3DXVECTOR4[nVtx];
	pSBSys = new SMVERTEX[nVtx];

	memcpy(pSBSys, pSrc->pSBSys, sizeof(SMVERTEX) * nVtx);
	memcpy(pVBSys, pSrc->pVBSys, sizeof(NMVERTEX) * nVtx);
	memcpy(pGBSys, pSrc->pGBSys, sizeof(D3DXVECTOR4) * nVtx);
	memcpy(pIBSys, pSrc->pIBSys, sizeof(WORD) * nIdx);

	pRoot = _pRoot;
	mapMode = MAPMODE_STATIC;
	bMustRemap = true;
}


MeshBuffer::~MeshBuffer()
{
	SAFE_DELETEA(pGBSys);
	SAFE_DELETEA(pIBSys);
	SAFE_DELETEA(pVBSys);
	SAFE_DELETEA(pSBSys);
	SAFE_RELEASE(pIB);
	SAFE_RELEASE(pVB);
	SAFE_RELEASE(pGB);
	SAFE_RELEASE(pSB);
	oapiWriteLog("MeshBuffer::Destruct");
}

bool MeshBuffer::Release()
{
	nRef--;
	return nRef <= 0;
}

MeshBuffer* MeshBuffer::Reference()
{
	nRef++;
	return this;
}

void MeshBuffer::MustRemap(DWORD mode)
{
	if (mode == MAPMODE_CURRENT) mode = mapMode;

	if (mode != mapMode) {
		SAFE_RELEASE(pIB);
		SAFE_RELEASE(pVB);
		SAFE_RELEASE(pGB);
		SAFE_RELEASE(pSB);
		mapMode = mode;
	}

	bMustRemap = true;
}

void MeshBuffer::Map(LPDIRECT3DDEVICE9 pDev)
{

	if (!bMustRemap) return;

	bMustRemap = false;

	DWORD Usage = 0;
	DWORD Lock = 0;

	if (mapMode == MAPMODE_DYNAMIC) Usage = D3DUSAGE_DYNAMIC, Lock = D3DLOCK_DISCARD;

	if (!pSB) {
		HR(pDev->CreateVertexBuffer(nVtx * sizeof(SMVERTEX), Usage, 0, D3DPOOL_DEFAULT, &pSB, NULL));
	}
	if (!pVB) {
		HR(pDev->CreateVertexBuffer(nVtx * sizeof(NMVERTEX), Usage, 0, D3DPOOL_DEFAULT, &pVB, NULL));
	}
	if (!pGB) {
		HR(pDev->CreateVertexBuffer(nVtx * sizeof(D3DXVECTOR4), Usage, 0, D3DPOOL_DEFAULT, &pGB, NULL));
	}
	if (!pIB) {
		HR(pDev->CreateIndexBuffer(nIdx * sizeof(WORD), Usage, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &pIB, NULL));
	}

	LPVOID pTgt;

	HR(pSB->Lock(0, 0, (LPVOID*)&pTgt, Lock));
	memcpy(pTgt, pSBSys, nVtx * sizeof(SMVERTEX));
	HR(pSB->Unlock());

	HR(pVB->Lock(0, 0, (LPVOID*)&pTgt, Lock));
	memcpy(pTgt, pVBSys, nVtx * sizeof(NMVERTEX));
	HR(pVB->Unlock());

	HR(pGB->Lock(0, 0, (LPVOID*)&pTgt, Lock));
	memcpy(pTgt, pGBSys, nVtx * sizeof(D3DXVECTOR4));
	HR(pGB->Unlock());

	HR(pIB->Lock(0, 0, (LPVOID*)&pTgt, Lock));
	memcpy(pTgt, pIBSys, nIdx * sizeof(WORD));
	HR(pIB->Unlock());
}








// ======================================================================================
// Mesh Implementation
// ======================================================================================
//
void D3D9Mesh::Null(const char *meshName /* = NULL */)
{
	nGrp = 0;
	Grp = NULL;
	nTex = 0;
	Tex	= NULL;
	nMtrl = 0;
	pBuf = NULL;
	Mtrl = NULL;
	pGrpTF = NULL;
	cAmbient = { 0.0f, 0.0f, 0.0f };
	MaxFace  = 0;
	MaxVert  = 0;
	vClass = 0;
	DefShader = SHADER_LEGACY;
	hOapiMesh = NULL;
	MeshFlags = 0;
	Flags = 0;

	bIsTemplate = false;
	bGlobalTF = false;
	bBSRecompute = true;
	bBSRecomputeAll = true;
	bModulateMatAlpha = false;
	bIsReflective = false;
	bCanRenderFast = false;
	bMtrlModidied = false;

	bli = BakedLights.begin();

	Locals = new LightStruct[Config->MaxLights()];

	for (int i = 0; i < SHM_CASCADE_COUNT; i++) {
		pShadowMap[i] = NULL;
		ShdSubRect[i] = { 0,0,1,1 };
	}

	memset(Locals, 0, sizeof(LightStruct) * Config->MaxLights());
	memset(LightList, 0, sizeof(LightList));
	strcpy_s(this->name, ARRAYSIZE(this->name), meshName ? meshName : "???");
}

// ===========================================================================================
//
D3D9Mesh::D3D9Mesh(const char *fname) : D3D9Effect()
{
	Null();
	MESHHANDLE hMesh = oapiLoadMesh(fname);

	if (hMesh) {
		LoadMeshFromHandle(hMesh);
		oapiDeleteMesh(hMesh);
	}
	else {
		LogErr("Failed to load [%s]", fname);
	}

	MeshCatalog.insert(this);
	if (pBuf) pBuf->Map(pDev);
}

// ===========================================================================================
//
D3D9Mesh::D3D9Mesh(MESHHANDLE hMesh, bool asTemplate, D3DXVECTOR3 *reorig, float *scale) : D3D9Effect()
{
	Null();
	LoadMeshFromHandle(hMesh, reorig, scale);
	bIsTemplate = asTemplate;
	MeshCatalog.insert(this);
	if (pBuf) pBuf->Map(pDev);
}


// ===========================================================================================
//
D3D9Mesh::D3D9Mesh(DWORD groups, const MESHGROUPEX **hGroup, const SURFHANDLE *hSurf) : D3D9Effect()
{
	Null();
	nGrp = groups;
	Grp = new GROUPREC[nGrp]; memset(Grp, 0, sizeof(GROUPREC) * nGrp);

	for (DWORD i=0;i<nGrp;i++) {
		SetGroupRec(i, hGroup[i]);
		Grp[i].TexIdxEx[0] = SPEC_DEFAULT;
		Grp[i].TexMixEx[0] = 0.0f;
		Grp[i].TexIdx  = i;
		Grp[i].MtrlIdx = SPEC_DEFAULT;
	}

	nMtrl = 0;
	nTex = nGrp+1;
	Tex = new lpSurfNative[nTex];
	Tex[0] = 0; // 'no texture'
	for (DWORD i=1;i<nTex;i++) Tex[i] = SURFACE(hSurf[i-1]);

	ProcessInherit();

	pBuf = new MeshBuffer(MaxVert, MaxFace, this);

	for (DWORD i=0;i<nGrp;i++) CopyVertices(&Grp[i], hGroup[i]);

	pGrpTF = new D3DXMATRIX[nGrp];

	D3DXMatrixIdentity(&mTransform);
	D3DXMatrixIdentity(&mTransformInv);
	MeshCatalog.insert(this);

	UpdateBoundingBox();
	CheckMeshStatus();

	pBuf->Map(pDev);
}


// ===========================================================================================
//
D3D9Mesh::D3D9Mesh(const MESHGROUPEX *pGroup, const MATERIAL *pMat, SurfNative *pTex) : D3D9Effect()
{
	Null();

	// template meshes are stored in system memory
	nGrp   = 1;
	Grp    = new GROUPREC[nGrp]; memset(Grp, 0, sizeof(GROUPREC) * nGrp);
	nTex   = 2;
	Tex	   = new lpSurfNative[nTex];
	Tex[0] = 0; // 'no texture'
	Tex[1] = pTex;
	nMtrl  = 1;
	Mtrl   = new D3D9MatExt[nMtrl];
	pGrpTF = new D3DXMATRIX[nGrp];

	SetGroupRec(0, pGroup);

	pBuf = new MeshBuffer(MaxVert, MaxFace, this);

	SetMaterial((const D3DMATERIAL9*)pMat, 0, false);
	CopyVertices(&Grp[0], pGroup);

	D3DXMatrixIdentity(&mTransform);
	D3DXMatrixIdentity(&mTransformInv);
	MeshCatalog.insert(this);

	UpdateBoundingBox();
	CheckMeshStatus();

	pBuf->Map(pDev);
}


// ===========================================================================================
// Create new Instance from a template mesh
//
D3D9Mesh::D3D9Mesh(MESHHANDLE hMesh, const D3D9Mesh &hTemp)
{
	Null(oapiGetMeshFilename(hMesh));
	hOapiMesh = hMesh;

	// Confirm the source is global template
	assert(hTemp.bIsTemplate == true);

	nGrp = oapiMeshGroupCount(hMesh); assert(nGrp == hTemp.nGrp);

	if (nGrp == 0) return;

	strcpy_s(name, ARRAYSIZE(name), hTemp.name);

	// Use Template's Vertex Data directly, no need for a local copy unless locally modified. 
	pBuf = hTemp.pBuf->Reference();

	BBox = hTemp.BBox;
	MaxVert = hTemp.MaxVert;
	MaxFace = hTemp.MaxFace;
	MeshFlags = hTemp.MeshFlags;
	Flags = hTemp.Flags;

	// Clone group records from a tremplate
	Grp = new GROUPREC[nGrp];
	memcpy(Grp, hTemp.Grp, sizeof(GROUPREC)*nGrp);

	if (MaxVert == 0 || MaxFace == 0) return;

	// -----------------------------------------------------------------------
	nTex = oapiMeshTextureCount(hMesh) + 1;	assert(nTex == hTemp.nTex);
	Tex = new lpSurfNative[nTex];
	Tex[0] = 0; // 'no texture'
	for (DWORD i = 1; i<nTex; i++) Tex[i] = SURFACE(oapiGetTextureHandle(hMesh, i));

	// -----------------------------------------------------------------------
	nMtrl = oapiMeshMaterialCount(hMesh); assert(nMtrl == hTemp.nMtrl);
	if (nMtrl) Mtrl = new D3D9MatExt[nMtrl];
	for (DWORD i = 0; i<nMtrl; i++)	SetMaterial((const D3DMATERIAL9*)oapiMeshMaterial(hMesh, i), i, false);

	pGrpTF = new D3DXMATRIX[nGrp];

	D3DXMatrixIdentity(&mTransform);
	D3DXMatrixIdentity(&mTransformInv);

	MeshCatalog.insert(this);

	UpdateBoundingBox();
	CheckMeshStatus();

	// No need to "pBuf->Map(pDev)" here, source template is already mapped 
}


// ===========================================================================================
//
D3D9Mesh::~D3D9Mesh()
{
	_TRACE;

	if (MeshCatalog.erase(this)) LogAlw("Mesh %s Removed from catalog", _PTR(this));
	else 						 LogErr("Mesh %s wasn't in meshcatalog", _PTR(this));

	Release();

	LogOk("Mesh %s Deleted successfully -------------------------------", _PTR(this));
}


// ===========================================================================================
//
void D3D9Mesh::Release()
{
	SAFE_DELETEA(Locals);
	SAFE_DELETEA(Grp);
	SAFE_DELETEA(Tex);
	SAFE_DELETEA(Mtrl);
	SAFE_DELETEA(pGrpTF);

	if (pBuf) if (pBuf->Release()) delete pBuf;
	pBuf = nullptr;

	for (auto x : BakedLights) {
		for (auto y : x.second.pMap) SAFE_RELEASE(y);
		for (auto y : x.second.pSunAO) SAFE_RELEASE(y);
		SAFE_RELEASE(x.second.pCombined);
		SAFE_RELEASE(x.second.pSunAOComb);
	}
}


// ===========================================================================================
// Reload a mesh file in response to VESSEL::MeshModified() call
//

void D3D9Mesh::ReLoadMeshFromHandle(MESHHANDLE hMesh)
{
	hOapiMesh = hMesh;
	const char* meshn = oapiGetMeshFilename(hMesh);
	strcpy_s(name, 128, meshn ? meshn : "???");

	// Relese buffers, Tex, Mtrl and Grp counts may have changed.
	SAFE_DELETEA(Tex);
	SAFE_DELETEA(Mtrl);
	SAFE_DELETEA(pGrpTF);
	SAFE_DELETEA(Grp);

	nGrp = oapiMeshGroupCount(hMesh);

	if (nGrp == 0) return;

	Grp = new GROUPREC[nGrp];
	memset(Grp, 0, sizeof(GROUPREC) * nGrp);

	MaxFace = 0; // Incremented in SetGroupRec()
	MaxVert = 0;

	for (DWORD i = 0; i<nGrp; i++) SetGroupRec(i, oapiMeshGroupEx(hMesh, i));

	if (MaxVert == 0 || MaxFace == 0) {
		if (pBuf) if (pBuf->IsLocalTo(this)) {
			if (pBuf->Release()) SAFE_DELETE(pBuf);
		}
		return;
	}

	// If this is an instance, Create a local vertex buffers... 
	if (pBuf->IsLocalTo(this) == false) pBuf = new MeshBuffer(MaxVert, MaxFace, this);

	// -----------------------------------------------------------------------
	nTex = oapiMeshTextureCount(hMesh) + 1;
	Tex = new lpSurfNative[nTex];
	Tex[0] = 0; // 'no texture'
	for (DWORD i = 1; i<nTex; i++) Tex[i] = SURFACE(oapiGetTextureHandle(hMesh, i));
	// -----------------------------------------------------------------------
	nMtrl = oapiMeshMaterialCount(hMesh);
	if (nMtrl) Mtrl = new D3D9MatExt[nMtrl];
	for (DWORD i = 0; i<nMtrl; i++)	SetMaterial((const D3DMATERIAL9*)oapiMeshMaterial(hMesh, i), i, false);
	// -----------------------------------------------------------------------

	ProcessInherit();

	for (DWORD i = 0; i<nGrp; i++) CopyVertices(&Grp[i], oapiMeshGroupEx(hMesh, i));

	pGrpTF = new D3DXMATRIX[nGrp];
	MeshFlags = oapiGetMeshFlags(hMesh);

	D3DXMatrixIdentity(&mTransform);
	D3DXMatrixIdentity(&mTransformInv);

	UpdateBoundingBox();
	CheckMeshStatus();

	pBuf->Map(pDev);
}

// ===========================================================================================
//
const char* D3D9Mesh::GetDirName(int i, int v)
{
	static const char* names[] = { "up", "down", "left", "right", "fwd", "aft" };
	static const char* named[] = { "+y", "-y", "-x", "+x", "+z", "-z" };
	if (v == 0) return names[i];
	return named[i];
}

// ===========================================================================================
//
FVECTOR3 D3D9Mesh::GetDir(int i)
{
	switch (i) {
	case 0: return FVECTOR3(0,  1, 0); // Up
	case 1: return FVECTOR3(0, -1, 0); // Down
	case 2: return FVECTOR3(-1, 0, 0); // Left
	case 3: return FVECTOR3( 1, 0, 0); // Right
	case 4: return FVECTOR3(0, 0,  1); // Fwd
	case 5: return FVECTOR3(0, 0, -1); // Aft
	}
	return FVECTOR3(0, 0, 1);
}

// ===========================================================================================
//
void D3D9Mesh::ClearBake(int i)
{
	for (int k = 0; k < 16; k++) BakedLights[i].pMap[k] = NULL;
	for (int k = 0; k < 6; k++) BakedLights[i].pSunAO[k] = NULL;
	BakedLights[i].pCombined = NULL;
	BakedLights[i].pSunAOComb = NULL;
}

// ===========================================================================================
//
void D3D9Mesh::LoadBakedLights()
{
	if (BakedLights.size()) return;	// Already Loaded, skip the rest
	char id[32];

	for (int i = 0; i < nTex; i++)
	{
		if (!Tex[i]) continue; // No base texture, pick next

		for (int j = 0; j < 16; j++)
		{
			sprintf_s(id, "_bkl%d", j);		
			LPDIRECT3DTEXTURE9 pTex = NatLoadSpecialTexture(Tex[i]->GetPath(), id, true);
			if (pTex) {
				if (BakedLights.find(i) == BakedLights.end()) ClearBake(i);
				BakedLights[i].pMap[j] = pTex;
			}
		}

		if (Config->ExpVCLight == 0)
		{
			for (int j = 0; j < 6; j++)
			{
				sprintf_s(id, " baked sunlight %s", GetDirName(j, 0));
				LPDIRECT3DTEXTURE9 pTex = NatLoadSpecialTexture(Tex[i]->GetPath(), id, true);
				if (pTex) {
					if (BakedLights.find(i) == BakedLights.end()) ClearBake(i);
					BakedLights[i].pSunAO[j] = pTex;
				}
				else {
					sprintf_s(id, "_bs%s", GetDirName(j, 1));
					LPDIRECT3DTEXTURE9 pTex = NatLoadSpecialTexture(Tex[i]->GetPath(), id, true);
					if (pTex) {
						if (BakedLights.find(i) == BakedLights.end()) ClearBake(i);
						BakedLights[i].pSunAO[j] = pTex;
					}
				}
			}
		}
	}

	// Construct render surface for all combined maps 
	for (auto& a : BakedLights) {
		int i = a.first;
		if (Tex[i]) {
			HR(D3DXCreateTexture(pDev, Tex[i]->GetWidth(), Tex[i]->GetHeight(), 0, D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &a.second.pCombined));
			HR(D3DXCreateTexture(pDev, Tex[i]->GetWidth(), Tex[i]->GetHeight(), 0, D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &a.second.pSunAOComb));
		}
	}

	bli = BakedLights.begin();
}


// ===========================================================================================
//
void D3D9Mesh::BakeLights(ImageProcessing* pBaker, const FVECTOR3* BakedLightsControl)
{
	if (!pBaker->IsOK()) return; // Baker not initialized
	if (DefShader != SHADER_BAKED_VC) return; // Not supported by shader
	if (BakedLights.size() == 0) return; // Nothing to bake

	DWORD flags = IPF_POINT | IPF_CLAMP;
	FVECTOR3 control[16];

	pBaker->Activate("PSMain");

	for (auto x : BakedLights)
	{
		int slot = 0;
		for (int i = 0; i < 16; i++)
		{
			auto y = x.second.pMap[i];
			if (y)
			{
				pBaker->SetTextureNative(slot, y, flags);
				control[slot] = BakedLightsControl[i];
				slot++;
			}
		}

		pBaker->SetFloat("fControl", control, slot * sizeof(FVECTOR3));
		pBaker->SetInt("iCount", slot);

		LPDIRECT3DSURFACE9 pSrf = NULL;

		if (x.second.pCombined)
		{
			if (x.second.pCombined->GetSurfaceLevel(0, &pSrf) == S_OK)
			{
				pBaker->SetOutputNative(0, pSrf);
				pBaker->Execute(true);
				SAFE_RELEASE(pSrf);
			}
		}
	}
}


// ===========================================================================================
//
void D3D9Mesh::BakeAO(ImageProcessing* pBaker, const FVECTOR3 &vSun, const LVLH &lvlh, const LPDIRECT3DTEXTURE9 pIrrad)
{
	if (!pBaker->IsOK()) return; // Baker not initialized
	if (DefShader != SHADER_BAKED_VC) return; // Not supported by shader
	if (BakedLights.size() == 0) return; // Nothing to bake

	DWORD flags = IPF_POINT | IPF_CLAMP;
	FVECTOR3 control[6];
	FVECTOR3 ParTexCoord[6];
	bool bSE[6];
	float fShine = 1.0f;
	bool bShine = true;

	// Compute texcoords for Irradiance lookup for prime directions
	//
	for (int i = 0; i < 6; i++) {
		FVECTOR3 DirL = GetDir(i);
		float z = dot(lvlh.Up, DirL);	// lvlh is in a local vessel frame
		FVECTOR2 p = FVECTOR2(dot(lvlh.East, DirL), dot(lvlh.North, DirL)) / (1.0f + abs(z));
		p *= FVECTOR2(0.2273f, 0.4545f);
		ParTexCoord[i] = FVECTOR3(p.x, p.y, z);
	}

	pBaker->Activate("PSSunAO");

	// Compute sunligh intensity factors for prime directions
	//
	for (int i = 0; i < 6; i++)
	{
		bSE[i] = false;
		control[i] = 0.0f;

		if (DebugControls::IsActive())
			if ((DebugControls::ambdir != i) && (DebugControls::ambdir != -1)) continue;

		auto tex = bli->second.pSunAO[i];
		bSE[i] = (tex != NULL);
		if (tex)
		{
			pBaker->SetTextureNative(i, tex, flags);
			control[i] = saturate(dot(GetDir(i), vSun));
			control[i] *= control[i];
		}	
	}

	if (DebugControls::IsActive()) {
		if (DebugControls::debugFlags & DBG_FLAGS_NOSUNAMB)
			for (int i = 0; i < 6; i++) control[i] = 0;
		if (DebugControls::debugFlags & DBG_FLAGS_NOPLNAMB)
			fShine = 0.0f, bShine = false;
	}

	if (pIrrad == nullptr) fShine = 0.0f, bShine = false;

	pBaker->SetTextureNative("tIrrad", pIrrad, IPF_LINEAR | IPF_CLAMP);
	pBaker->SetFloat("fControl", control, sizeof(control));
	pBaker->SetFloat("vParTexPos", ParTexCoord, sizeof(ParTexCoord));
	pBaker->SetFloat("fShine", &fShine, sizeof(fShine));
	pBaker->SetBool("bEnabled", bSE, sizeof(bSE));
	pBaker->SetBool("bShine", &bShine, sizeof(bShine));

	LPDIRECT3DSURFACE9 pSrf = NULL;

	if (bli->second.pSunAOComb)
	{
		if (bli->second.pSunAOComb->GetSurfaceLevel(0, &pSrf) == S_OK)
		{
			pBaker->SetOutputNative(0, pSrf);
			pBaker->Execute(true);
			SAFE_RELEASE(pSrf);
		}
	}

	bli++;
	if (bli == BakedLights.end()) bli = BakedLights.begin();
}


// ===========================================================================================
//
LPDIRECT3DTEXTURE9 D3D9Mesh::GetCombinedMap(int tex_idx)
{
	if (tex_idx < 0 ) for (auto a : BakedLights) if (a.second.pCombined) return a.second.pCombined;
	auto it = BakedLights.find(tex_idx);
	if (it != BakedLights.end()) return it->second.pCombined;
	return NULL;
}


// ===========================================================================================
//
void D3D9Mesh::LoadMeshFromHandle(MESHHANDLE hMesh, D3DXVECTOR3 *reorig, float *scale)
{
	hOapiMesh = hMesh;
	const char* meshn = oapiGetMeshFilename(hMesh);
	strcpy_s(name, 128, meshn ? meshn : "???");

	nGrp = oapiMeshGroupCount(hMesh);

	if (nGrp == 0) return;

	Grp = new GROUPREC[nGrp]; memset(Grp, 0, sizeof(GROUPREC) * nGrp);
	
	for (DWORD i = 0; i<nGrp; i++) SetGroupRec(i, oapiMeshGroupEx(hMesh, i));

	if (MaxVert == 0 || MaxFace == 0) return;

	pBuf = new MeshBuffer(MaxVert, MaxFace, this);

	// -----------------------------------------------------------------------
	nTex = oapiMeshTextureCount(hMesh) + 1;
	Tex = new lpSurfNative[nTex];
	Tex[0] = 0; // 'no texture'
	for (DWORD i = 1; i<nTex; i++) Tex[i] = SURFACE(oapiGetTextureHandle(hMesh, i));

	// -----------------------------------------------------------------------
	nMtrl = oapiMeshMaterialCount(hMesh);
	if (nMtrl) Mtrl = new D3D9MatExt[nMtrl];
	for (DWORD i = 0; i<nMtrl; i++)	SetMaterial((const D3DMATERIAL9*)oapiMeshMaterial(hMesh, i), i, false);

	ProcessInherit();

	for (DWORD i = 0; i<nGrp; i++) CopyVertices(&Grp[i], oapiMeshGroupEx(hMesh, i), reorig, scale);

	pGrpTF = new D3DXMATRIX[nGrp];
	MeshFlags = oapiGetMeshFlags(hMesh);

	D3DXMatrixIdentity(&mTransform);
	D3DXMatrixIdentity(&mTransformInv);

	UpdateBoundingBox();
	CheckMeshStatus();
}

// ===========================================================================================
//
void D3D9Mesh::ReloadTextures()
{
	for (UINT i = 0; i < nTex; i++) if (Tex[i]) SURFACE(Tex[i])->Reload();
}

// ===========================================================================================
//
void D3D9Mesh::SetName(const char *name_)
{
	if (name_) strcpy_s(this->name, ARRAYSIZE(this->name), name_);
}

// ===========================================================================================
//
void D3D9Mesh::SetName(UINT idx)
{
	if ((strncmp(name, "???", 3) == 0) || (name[0] == 0)) sprintf_s(name, ARRAYSIZE(name), "MeshIdx-%u", idx);
}

// ===========================================================================================
//
bool D3D9Mesh::HasShadow() const
{
	if (!IsOK()) return false;
	for (DWORD g=0; g<nGrp; g++) {
		if ((Grp[g].UsrFlag & 0x3) != 0) continue;
		//if (Grp[g].IntFlag & 3) continue;
		return true;
	}
	return false;
}


// ===========================================================================================
//
void D3D9Mesh::ProcessInherit()
{
	_TRACE;
	if (!IsOK()) return;
	if (Grp[0].MtrlIdx == SPEC_INHERIT) Grp[0].MtrlIdx = SPEC_DEFAULT;
	if (Grp[0].TexIdx == SPEC_INHERIT) Grp[0].TexIdx = SPEC_DEFAULT;
	if (Grp[0].TexIdxEx[0] == SPEC_INHERIT) Grp[0].TexIdxEx[0] = SPEC_DEFAULT;

	bool bPopUp = false;

	for (DWORD i=0;i<nGrp;i++) {

		if (Grp[i].UsrFlag & 0x8) LogErr("MeshGroupFlag 0x8 in use (OPERATION NOT IMPLEMENTED)");

		// Inherit Material
		if (Grp[i].MtrlIdx == SPEC_INHERIT) Grp[i].MtrlIdx = Grp[i-1].MtrlIdx;

		// Inherit Texture
		if (Grp[i].TexIdx == SPEC_DEFAULT) Grp[i].TexIdx = 0;
		else if (Grp[i].TexIdx == SPEC_INHERIT) Grp[i].TexIdx = Grp[i-1].TexIdx;
		else Grp[i].TexIdx++;

		// Inherit Night Texture
		if (Grp[i].TexIdxEx[0] == SPEC_DEFAULT) Grp[i].TexIdxEx[0] = 0;
		else if (Grp[i].TexIdxEx[0] == SPEC_INHERIT) Grp[i].TexIdxEx[0] = Grp[i-1].TexIdxEx[0];
		else Grp[i].TexIdxEx[0]++;

		// Do some safety checks
		if (Grp[i].TexIdx>=nTex) {
			LogErr("Mesh(%s) has a texture index %u in group %u out of range.", _PTR(this), Grp[i].TexIdx, i);
			Grp[i].TexIdx = 0;
			bPopUp = true;
		}
		if (Grp[i].TexIdxEx[0]>=nTex) {
			LogErr("Mesh(%s) has a night texture index %u in group %u out of range.", _PTR(this), Grp[i].TexIdxEx[0], i);
			Grp[i].TexIdxEx[0] = 0;
			bPopUp = true;
		}

		if (Grp[i].MtrlIdx!=SPEC_DEFAULT) {
			if (Grp[i].MtrlIdx>=nMtrl) {
				LogErr("Mesh(%s) has a material index %u in group %u out of range.", _PTR(this), Grp[i].MtrlIdx, i);
				Grp[i].MtrlIdx = SPEC_DEFAULT;
				bPopUp = true;
			}
		}
	}
	if (bPopUp) MessageBoxA(NULL, "Invalid Mesh Detected", "D3D9Client Error:",MB_OK);
}


// ===========================================================================================
//
D3DXVECTOR3 D3D9Mesh::GetGroupSize(DWORD idx) const
{
	if (!IsOK()) return D3DXVECTOR3(0,0,0);
	if (idx>=nGrp) return D3DXVECTOR3(0,0,0);
	if (Grp[idx].nVert<2) return D3DXVECTOR3(0,0,0);
	return D3DXVECTOR3f4(Grp[idx].BBox.max - Grp[idx].BBox.min);
}


// ===========================================================================================
//
void D3D9Mesh::ResetTransformations()
{
	_TRACE;
	if (!IsOK()) return;
	D3DXMatrixIdentity(&mTransform);
	D3DXMatrixIdentity(&mTransformInv);
	bGlobalTF = false;
	bBSRecompute = true;
	bBSRecomputeAll = true;
	for (DWORD i=0;i<nGrp;i++) {
		D3DXMatrixIdentity(&Grp[i].Transform);
		D3DXMatrixIdentity(&pGrpTF[i]);
		Grp[i].bTransform = false;
	}
}


// ===========================================================================================
//
void D3D9Mesh::UpdateTangentSpace(NMVERTEX *pVrt, WORD *pIdx, DWORD nVtx, DWORD nFace, bool bTextured)
{
	if (!IsOK()) return;

	if (bTextured) {

		XMVECTOR *ta = (XMVECTOR*)_aligned_malloc(sizeof(__m128)*(nVtx+1), 16);
		XMVECTOR zero = XMVectorSet(0, 0, 0, 0);
		for (DWORD i = 0; i < nVtx; i++) ta[i] = zero;

		for (DWORD i=0;i<nFace;i++) {

			DWORD i0 = pIdx[i*3];
			DWORD i1 = pIdx[i*3+1];
			DWORD i2 = pIdx[i*3+2];

			XMVECTOR r0 = XMLoadFloat3((XMFLOAT3*)&pVrt[i0].x);
			XMVECTOR r1 = XMLoadFloat3((XMFLOAT3*)&pVrt[i1].x);
			XMVECTOR r2 = XMLoadFloat3((XMFLOAT3*)&pVrt[i2].x);
			D3DXVECTOR2 t0 = D3DXVECTOR2(pVrt[i0].u, pVrt[i0].v);
			D3DXVECTOR2 t1 = D3DXVECTOR2(pVrt[i1].u, pVrt[i1].v);
			D3DXVECTOR2 t2 = D3DXVECTOR2(pVrt[i2].u, pVrt[i2].v);

			float u0 = t1.x - t0.x;
			float v0 = t1.y - t0.y;
			float u1 = t2.x - t0.x;
			float v1 = t2.y - t0.y;

			XMVECTOR k0 = r1 - r0;
			XMVECTOR k1 = r2 - r0;

			float q = (u0*v1-u1*v0);
			if (q==0) q = 1.0f;
			else q = 1.0f / q;

			XMVECTOR t = ((k0*v1 - k1*v0) * q);
			ta[i0]+=t; ta[i1]+=t; ta[i2]+=t;
			pVrt[i0].w = pVrt[i1].w = pVrt[i2].w = (q<0.0f ? 1.0f : -1.0f);
		}

		for (DWORD i=0;i<nVtx; i++) {
			XMVECTOR n = XMVector3Normalize(XMLoadFloat3((XMFLOAT3*)&pVrt[i].nx));
			XMVECTOR t = XMVector3Normalize((ta[i] - n * XMVector3Dot(ta[i], n)));
			XMStoreFloat3((XMFLOAT3*)&pVrt[i].tx, t);
		}

		_aligned_free(ta);
	}
	else {
		for (DWORD i=0;i<nVtx; i++) {
			D3DXVECTOR3 n = D3DXVECTOR3(pVrt[i].nx,  pVrt[i].ny,  pVrt[i].nz);
			D3DXVECTOR3 t = Perpendicular(&n);
			D3DXVec3Normalize(&t, &t);
			pVrt[i].tx = t.x;
			pVrt[i].ty = t.y;
			pVrt[i].tz = t.z;
		}
	}
}



// ===========================================================================================
//
void D3D9Mesh::SetGroupRec(DWORD i, const MESHGROUPEX *mg)
{
	if (i>=nGrp) return;
	memcpy(Grp[i].TexIdxEx, mg->TexIdxEx, MAXTEX*sizeof(DWORD));
	memcpy(Grp[i].TexMixEx, mg->TexMixEx, MAXTEX*sizeof(float));
	Grp[i].TexIdx  = mg->TexIdx;
	Grp[i].MtrlIdx = mg->MtrlIdx;
	Grp[i].IdexOff = MaxFace*3;
	Grp[i].VertOff = MaxVert;
	Grp[i].nFace   = mg->nIdx/3;
	Grp[i].nVert   = mg->nVtx;
	Grp[i].UsrFlag = mg->UsrFlag;
	Grp[i].IntFlag = mg->Flags;
	Grp[i].zBias   = mg->zBias;

	D3DXMatrixIdentity(&Grp[i].Transform);

	MaxFace += Grp[i].nFace;
	MaxVert += Grp[i].nVert;
}


// ===========================================================================================
//
bool D3D9Mesh::CopyVertices(GROUPREC *grp, const MESHGROUPEX *mg, D3DXVECTOR3 *reorig, float *scale)
{
	if (!pBuf) return false;
	NTVERTEX *pNT = mg->Vtx;
	SMVERTEX *pShad = pBuf->pSBSys + grp->VertOff;
	NMVERTEX *pVert = pBuf->pVBSys + grp->VertOff;
	D3DXVECTOR4 *pGeo = pBuf->pGBSys + grp->VertOff;
	WORD *pIndex = pBuf->pIBSys + grp->IdexOff;

	for (DWORD i=0;i<mg->nIdx;i++) pIndex[i] = mg->Idx[i];

	for (DWORD i=0;i<mg->nVtx; i++) {
		float x = pNT[i].nx; float y = pNT[i].ny; float z = pNT[i].nz;
		float b = 1.0f/sqrt(y*y+z*z+x*x);
		pVert[i].nx = (x*b);
		pVert[i].ny = (y*b);
		pVert[i].nz = (z*b);

		if (scale) {
			pVert[i].x = pNT[i].x * (*scale);
			pVert[i].y = pNT[i].y * (*scale);
			pVert[i].z = pNT[i].z * (*scale);
		}
		else {
			pVert[i].x = pNT[i].x;
			pVert[i].y = pNT[i].y;
			pVert[i].z = pNT[i].z;
		}

		pVert[i].u  = pNT[i].tu;
		pVert[i].v  = pNT[i].tv;
		pVert[i].w  = 1.0f;
		pVert[i].tx = 1.0f;
		pVert[i].ty = 0.0f;
		pVert[i].tz = 0.0f;

		if (reorig) {
			pVert[i].x += reorig->x;
			pVert[i].y += reorig->y;
			pVert[i].z += reorig->z;
		}

		pGeo[i] = D3DXVECTOR4(pVert[i].x, pVert[i].y, pVert[i].z, 0);

		pShad[i].x = pVert[i].x; pShad[i].y = pVert[i].y; pShad[i].z = pVert[i].z;
		pShad[i].tu = pVert[i].u; pShad[i].tv = pVert[i].v;
	}

	// Check vertex index errors (This is important)
	//
	for (DWORD i=0;i<(mg->nIdx/3);i++) {
		DWORD v0 = i*3;	DWORD v1 = v0+1; DWORD v2 = v0+2;
		if (pIndex[v0]>=mg->nVtx || pIndex[v1]>=mg->nVtx || pIndex[v2]>=mg->nVtx) {
			pIndex[v0] = pIndex[v1] = pIndex[v2] = 0;
		}
	}

	// For un-instanced mesh the base-offset is zero
	if (Config->UseNormalMap) UpdateTangentSpace(pVert, pIndex, mg->nVtx, mg->nIdx/3, grp->TexIdx!=0);

	if (mg->nVtx>0) BoundingBox(pVert, mg->nVtx, &grp->BBox);
	else D9ZeroAABB(&grp->BBox);

	return true;
}


// ===========================================================================================
// This is required by Client implementation see clbkEditMeshGroup
//
int D3D9Mesh::EditGroup(DWORD grp, GROUPEDITSPEC *ges)
{
	_TRACE;
	if (!IsOK()) return 1;
	if (grp >= nGrp) return 1;
	if (!pBuf) return 1;

	bBSRecompute = true;

	GROUPREC *g = &Grp[grp];
	DWORD flag = ges->flags;
	DWORD old  = g->UsrFlag;

	if (flag & GRPEDIT_SETUSERFLAG)	     g->UsrFlag  = ges->UsrFlag;
	else if (flag & GRPEDIT_ADDUSERFLAG) g->UsrFlag |= ges->UsrFlag;
	else if (flag & GRPEDIT_DELUSERFLAG) g->UsrFlag &= ~ges->UsrFlag;

	if (flag & GRPEDIT_VTX) {

		if (pBuf->IsLocalTo(this) == false) 
		{
			// Can't make modifications to a global template
			// Create a local copy of the Mesh
			// TODO: Create local copy of the group only
			pBuf = new MeshBuffer(pBuf, this);
		}

		pBuf->MustRemap(MAPMODE_CURRENT);

		SMVERTEX *pShad = pBuf->pSBSys + g->VertOff;
		D3DXVECTOR4 *pGeo = pBuf->pGBSys + g->VertOff;
		NMVERTEX *vtx = pBuf->pVBSys + g->VertOff;
		WORD *idx = pBuf->pIBSys + g->IdexOff;

		DWORD i, vi;
		if (vtx) {
			for (i = 0; i < ges->nVtx; i++) {
				vi = (ges->vIdx ? ges->vIdx[i] : i);
				if (vi < g->nVert) {

					if      (flag & GRPEDIT_VTXCRDX)    vtx[vi].x   = ges->Vtx[i].x;
					else if (flag & GRPEDIT_VTXCRDADDX) vtx[vi].x  += ges->Vtx[i].x;
					if      (flag & GRPEDIT_VTXCRDY)    vtx[vi].y   = ges->Vtx[i].y;
					else if (flag & GRPEDIT_VTXCRDADDY) vtx[vi].y  += ges->Vtx[i].y;
					if      (flag & GRPEDIT_VTXCRDZ)    vtx[vi].z   = ges->Vtx[i].z;
					else if (flag & GRPEDIT_VTXCRDADDZ) vtx[vi].z  += ges->Vtx[i].z;
					if      (flag & GRPEDIT_VTXNMLX)    vtx[vi].nx  = ges->Vtx[i].nx;
					else if (flag & GRPEDIT_VTXNMLADDX) vtx[vi].nx += ges->Vtx[i].nx;
					if      (flag & GRPEDIT_VTXNMLY)    vtx[vi].ny  = ges->Vtx[i].ny;
					else if (flag & GRPEDIT_VTXNMLADDY) vtx[vi].ny += ges->Vtx[i].ny;
					if      (flag & GRPEDIT_VTXNMLZ)    vtx[vi].nz  = ges->Vtx[i].nz;
					else if (flag & GRPEDIT_VTXNMLADDZ) vtx[vi].nz += ges->Vtx[i].nz;
					if      (flag & GRPEDIT_VTXTEXU)    vtx[vi].u  = ges->Vtx[i].tu;
					else if (flag & GRPEDIT_VTXTEXADDU) vtx[vi].u += ges->Vtx[i].tu;
					if      (flag & GRPEDIT_VTXTEXV)    vtx[vi].v  = ges->Vtx[i].tv;
					else if (flag & GRPEDIT_VTXTEXADDV) vtx[vi].v += ges->Vtx[i].tv;

					if ((flag & GRPEDIT_VTXCRD)!=0 || (flag & GRPEDIT_VTXCRDADD)!=0) {
						pGeo[vi] = D3DXVECTOR4(vtx[vi].x, vtx[vi].y, vtx[vi].z, 0);

						pShad[vi].x = vtx[vi].x; pShad[vi].y = vtx[vi].y; pShad[vi].z = vtx[vi].z;
						pShad[vi].tu = vtx[vi].u; pShad[vi].tv = vtx[vi].v;
					}
				}
			}

			if (Config->UseNormalMap) UpdateTangentSpace(vtx, idx, g->nVert, g->nFace, g->TexIdx!=0);

			if (g->nVert>0) BoundingBox(vtx, g->nVert, &g->BBox);
			else D9ZeroAABB(&g->BBox);
		}
	}

	UpdateFlags();
	return 0;
}


NTVERTEX Convert(NMVERTEX &v)
{
	NTVERTEX n;
	n.x = v.x; n.y = v.y; n.z = v.z;
	n.nx = v.nx; n.ny = v.ny; n.nz = v.nz;
	n.tu = v.u;	n.tv = v.v;
	return n;
}


int D3D9Mesh::GetGroup (DWORD grp, GROUPREQUESTSPEC *grs)
{
	if (!pBuf) return 1;

	static NTVERTEX zero = {0,0,0, 0,0,0, 0,0};
	if (grp >= nGrp) return 1;
	DWORD nv = Grp[grp].nVert;
	DWORD ni = Grp[grp].nFace*3;
	DWORD i, vi;
	int ret = 0;

	if (grs->nVtx && grs->Vtx) { // vertex data requested
		NMVERTEX *vtx = pBuf->pVBSys + Grp[grp].VertOff;
		if (vtx) {
			if (grs->VtxPerm) { // random access data request
				for (i = 0; i < grs->nVtx; i++) {
					vi = grs->VtxPerm[i];
					if (vi < nv) {
						grs->Vtx[i] = Convert(vtx[vi]);
					} else {
						grs->Vtx[i] = zero;
						ret = 1;
					}
				}
			} else {
				if (grs->nVtx > nv) grs->nVtx = nv;
				for (i=0;i<grs->nVtx;i++) grs->Vtx[i] = Convert(vtx[i]);
			}
		}
		else return 1;
	}

	if (grs->nIdx && grs->Idx) { // index data requested
		WORD *idx = pBuf->pIBSys + Grp[grp].IdexOff;
		if (idx) {
			if (grs->IdxPerm) { // random access data request
				for (i = 0; i < grs->nIdx; i++) {
					vi = grs->IdxPerm[i];
					if (vi < ni) {
						grs->Idx[i] = idx[vi];
					} else {
						grs->Idx[i] = 0;
						ret = 1;
					}
				}
			} else {
				if (grs->nIdx > ni) grs->nIdx = ni;
				for (i=0;i<grs->nIdx;i++) grs->Idx[i] = idx[i];
			}
		}
		else return 1;
	}

	grs->MtrlIdx = Grp[grp].MtrlIdx;
	grs->TexIdx = Grp[grp].TexIdx;
	return ret;
}

// ===========================================================================================
//
void D3D9Mesh::UpdateFlags()
{
	Flags = 0;
	for (DWORD i = 0; i < nGrp; i++)
	{
		if (Grp[i].UsrFlag & 0x20) Flags |= 0x20;
	}
}

// ===========================================================================================
//
void D3D9Mesh::SetMFDScreenId(DWORD idx, WORD id)
{
	if (idx<nGrp) Grp[idx].MFDScreenId = id;
}

// ===========================================================================================
//
bool D3D9Mesh::SetTexture(DWORD texidx, SURFHANDLE tex)
{
	_TRACE;
	if (!IsOK()) return false;
	if (texidx >= nTex) {
		LogErr("D3D9Mesh::SetTexture(%u, %s) index out of range",texidx, _PTR(tex));
		return false;
	}
	Tex[texidx] = (lpSurfNative)tex;
	LogBlu("D3D9Mesh(%s)::SetTexture(%u, %s) (%s)", _PTR(this), texidx, _PTR(tex), SURFACE(tex)->GetName());
	CheckMeshStatus();
	return true;
}

// ===========================================================================================
//
DWORD D3D9Mesh::GetMeshGroupMaterialIdx(DWORD idx) const
{
	if (!IsOK()) return 0;
	if (idx>=nGrp) return 0;
	return Grp[idx].MtrlIdx;
}

// ===========================================================================================
//
DWORD D3D9Mesh::GetMeshGroupTextureIdx(DWORD idx) const
{
	if (!IsOK()) return 0;
	if (idx>=nGrp) return 0;
	return Grp[idx].TexIdx;
}

// ===========================================================================================
//
bool D3D9Mesh::HasTexture(SURFHANDLE hSurf) const
{
	if (!IsOK()) return false;
	for (DWORD i=0;i<nTex;i++) if (Tex[i]==hSurf) return true;
	return false;
}

// ===========================================================================================
//
void D3D9Mesh::SetTexMixture(DWORD ntex, float mix)
{
	_TRACE;
	if (!IsOK()) return;
	ntex--;
	for (DWORD g = 0; g < nGrp; g++) if (Grp[g].TexIdxEx[ntex] != SPEC_DEFAULT) Grp[g].TexMixEx[ntex] = mix;
}

// ===========================================================================================
//
void D3D9Mesh::SetSunLight(const D3D9Sun *light)
{
	memcpy(&sunLight, light, sizeof(D3D9Sun));
}

// ===========================================================================================
//
DWORD D3D9Mesh::GetVertexCount(int grp) const
{
	if (grp<0) return MaxVert;
	else return Grp[grp].nVert;
}

// ===========================================================================================
//
DWORD D3D9Mesh::GetIndexCount(int grp) const
{
	if (grp<0) return MaxFace*3;
	else return Grp[grp].nFace*3;
}

// ===========================================================================================
//
DWORD D3D9Mesh::GetGroupTransformCount() const
{
	DWORD cnt = 0;
	for (DWORD i=0;i<nGrp;i++) if (Grp[i].bTransform) cnt++;
	return cnt;
}

// ===========================================================================================
//
const D3D9Mesh::GROUPREC *D3D9Mesh::GetGroup(DWORD idx) const
{
	if (!IsOK()) return NULL;
	if (idx<nGrp) return &Grp[idx];
	return NULL;
}

// ===========================================================================================
//
D3D9Mesh::GROUPREC* D3D9Mesh::GetGroup(DWORD idx)
{
	if (!IsOK()) return NULL;
	if (idx < nGrp) return &Grp[idx];
	return NULL;
}

// ===========================================================================================
//
const D3D9MatExt * D3D9Mesh::GetMaterial(DWORD idx) const
{
	if (idx >= nMtrl) return NULL;
	return &Mtrl[idx];
}

// ===========================================================================================
//
bool D3D9Mesh::GetMaterial(D3D9MatExt *pMat, DWORD idx) const
{
	if (pMat && idx<nMtrl) {
		memcpy(pMat, &Mtrl[idx], sizeof(D3D9MatExt));
		return true;
	}
	return false;
}

// ===========================================================================================
//
void D3D9Mesh::SetMaterial(const D3DMATERIAL9 *pMat, DWORD idx, bool bStat)
{
	D3D9MatExt Mat;
	// Faulty output coming from oapiMeshMaterial, why ????
	if (pMat <= ((void*)0x100)) { 
		CreateDefaultMat(&Mat);
	} else CreateMatExt(pMat, &Mat);
	SetMaterial(&Mat, idx, bStat);
}

// ===========================================================================================
//
void D3D9Mesh::SetMaterial(const D3D9MatExt *pMat, DWORD idx, bool bStat)
{
	if (idx < nMtrl) {
		memcpy(&Mtrl[idx], pMat, sizeof(D3D9MatExt));

		if (Mtrl[idx].Specular.w < 0.1f) {
			Mtrl[idx].Specular.x = 0.0f;
			Mtrl[idx].Specular.y = 0.0f;
			Mtrl[idx].Specular.z = 0.0f;
		}
	}
	if (bStat) CheckMeshStatus();
}


// ===========================================================================================
//0 = success, 1=no graphics engine attached,
//2 = graphics engine does not support operation, 3 = invalid mesh handle,
//4 = material index out of range, 5 = material property not supported by shader used by the mesh.
//
int D3D9Mesh::SetMaterialEx(DWORD idx, MatProp mid, const FVECTOR4* value)
{

	if (idx >= nMtrl) return 4;

	// SET ---------------------------------------------------------------
	if (value) {
		switch (mid) {
		case MatProp::Diffuse:
			Mtrl[idx].Diffuse = *((D3DXVECTOR4*)value);
			Mtrl[idx].ModFlags |= D3D9MATEX_DIFFUSE;
			bMtrlModidied = true;
			return 0;
		case MatProp::Ambient:
			Mtrl[idx].Ambient = *((D3DXVECTOR3*)value);
			Mtrl[idx].ModFlags |= D3D9MATEX_AMBIENT;
			bMtrlModidied = true;
			return 0;
		case MatProp::Specular:
			Mtrl[idx].Specular = *((D3DXVECTOR4*)value);
			Mtrl[idx].ModFlags |= D3D9MATEX_SPECULAR;
			bMtrlModidied = true;
			return 0;
		case MatProp::Light:
			Mtrl[idx].Emissive = *((D3DXVECTOR3*)value);
			Mtrl[idx].ModFlags |= D3D9MATEX_EMISSIVE;
			bMtrlModidied = true;
			return 0;
		case MatProp::Emission:
			Mtrl[idx].Emission2 = *((D3DXVECTOR3*)value);
			Mtrl[idx].ModFlags |= D3D9MATEX_EMISSION2;
			bMtrlModidied = true;
			return 0;
		case MatProp::Reflect:
			Mtrl[idx].Reflect = *((D3DXVECTOR3*)value);
			Mtrl[idx].ModFlags |= D3D9MATEX_REFLECT;
			bMtrlModidied = true;
			return 0;
		case MatProp::Smooth:
			Mtrl[idx].Roughness = D3DXVECTOR2(value->g, value->r);
			Mtrl[idx].ModFlags |= D3D9MATEX_ROUGHNESS;
			bMtrlModidied = true;
			return 0;
		case MatProp::Fresnel:
			Mtrl[idx].Fresnel = *((D3DXVECTOR3*)value);
			Mtrl[idx].ModFlags |= D3D9MATEX_FRESNEL;
			bMtrlModidied = true;
			return 0;
		case MatProp::Metal:
			Mtrl[idx].Metalness = value->r;
			Mtrl[idx].ModFlags |= D3D9MATEX_METALNESS;
			bMtrlModidied = true;
			return 0;
		case MatProp::SpecialFX:
			Mtrl[idx].SpecialFX = *((D3DXVECTOR4*)value);
			Mtrl[idx].ModFlags |= D3D9MATEX_SPECIALFX;
			bMtrlModidied = true;
			return 0;
		}
		return 5;
	}

	// CLEAR -------------------------------------------------------------

	if (value == NULL)
	{
		switch (mid) {
		case MatProp::Diffuse:
			Mtrl[idx].Diffuse = D3DXVECTOR4(1, 1, 1, 1);
			Mtrl[idx].ModFlags &= (~D3D9MATEX_DIFFUSE);
			bMtrlModidied = true;
			return 0;
		case MatProp::Ambient:
			Mtrl[idx].Ambient = D3DXVECTOR3(1, 1, 1);
			Mtrl[idx].ModFlags &= (~D3D9MATEX_AMBIENT);
			bMtrlModidied = true;
			return 0;
		case MatProp::Specular:
			Mtrl[idx].Specular = D3DXVECTOR4(1, 1, 1, 20.0);
			Mtrl[idx].ModFlags &= (~D3D9MATEX_SPECULAR);
			bMtrlModidied = true;
			return 0;
		case MatProp::Light:
			Mtrl[idx].Emissive = D3DXVECTOR3(0, 0, 0);
			Mtrl[idx].ModFlags &= (~D3D9MATEX_EMISSIVE);
			bMtrlModidied = true;
			return 0;
		case MatProp::Emission:
			Mtrl[idx].Emission2 = D3DXVECTOR3(1, 1, 1);
			Mtrl[idx].ModFlags &= (~D3D9MATEX_EMISSION2);
			bMtrlModidied = true;
			return 0;
		case MatProp::Reflect:
			Mtrl[idx].Reflect = D3DXVECTOR3(0, 0, 0);
			Mtrl[idx].ModFlags &= (~D3D9MATEX_REFLECT);
			bMtrlModidied = true;
			return 0;
		case MatProp::Smooth:
			Mtrl[idx].Roughness = D3DXVECTOR2(0.0f, 1.0f);
			Mtrl[idx].ModFlags &= (~D3D9MATEX_ROUGHNESS);
			bMtrlModidied = true;
			return 0;
		case MatProp::Fresnel:
			Mtrl[idx].Fresnel = D3DXVECTOR3(1, 0, 1024.0f);
			Mtrl[idx].ModFlags &= (~D3D9MATEX_FRESNEL);
			bMtrlModidied = true;
			return 0;
		case MatProp::Metal:
			Mtrl[idx].Metalness = 0.0f;
			Mtrl[idx].ModFlags &= (~D3D9MATEX_METALNESS);
			bMtrlModidied = true;
			return 0;
		case MatProp::SpecialFX:
			Mtrl[idx].SpecialFX = D3DXVECTOR4(0, 0, 0, 0);
			Mtrl[idx].ModFlags &= (~D3D9MATEX_SPECIALFX);
			bMtrlModidied = true;
			return 0;
		}
		return 5;
	}
	return 5;
}


// ===========================================================================================
//0 = success, 1=no graphics engine attached,
//2 = graphics engine does not support operation, 3 = invalid mesh handle,
//4 = material index out of range, 5 = material property not supported by shader used by the mesh.
//
int D3D9Mesh::GetMaterialEx(DWORD idx, MatProp mid, FVECTOR4* value)
{
	if (idx >= nMtrl) return 4;

	if (value)
	{
		switch (mid)
		{
		case MatProp::Diffuse:
			*((D3DXVECTOR4*)value) = Mtrl[idx].Diffuse;
			return 0;
		case MatProp::Ambient:
			*((D3DXVECTOR3*)value) = Mtrl[idx].Ambient;
			return 0;
		case MatProp::Specular:
			*((D3DXVECTOR4*)value) = Mtrl[idx].Specular;
			return 0;
		case MatProp::Light:
			*((D3DXVECTOR3*)value) = Mtrl[idx].Emissive;
			return 0;
		case  MatProp::Emission:
			if ((Mtrl[idx].ModFlags&D3D9MATEX_EMISSION2) == 0) return -2;
			*((D3DXVECTOR3*)value) = Mtrl[idx].Emission2;
			return 0;
		case MatProp::Reflect:
			if ((Mtrl[idx].ModFlags&D3D9MATEX_REFLECT) == 0) return -2;
			*((D3DXVECTOR3*)value) = Mtrl[idx].Reflect;
			return 0;
		case MatProp::Smooth:
			if ((Mtrl[idx].ModFlags&D3D9MATEX_ROUGHNESS) == 0) return -2;
			value->g = Mtrl[idx].Roughness.x;
			value->r = Mtrl[idx].Roughness.y;
			return 0;
		case MatProp::Fresnel:
			if ((Mtrl[idx].ModFlags&D3D9MATEX_FRESNEL) == 0) return -2;
			*((D3DXVECTOR3*)value) = Mtrl[idx].Fresnel;
			return 0;
		case MatProp::Metal:
			if ((Mtrl[idx].ModFlags& D3D9MATEX_METALNESS) == 0) return -2;
			value->r = Mtrl[idx].Metalness;
			return 0;
		case MatProp::SpecialFX:
			if ((Mtrl[idx].ModFlags& D3D9MATEX_SPECIALFX) == 0) return -2;
			*((D3DXVECTOR4*)value) = Mtrl[idx].SpecialFX;
			return 0;
		}
		return 5;
	}
	return 5;
}

// ===========================================================================================
//
void D3D9Mesh::SetAmbientColor(const FVECTOR3& c)
{
	_TRACE;
	if (!IsOK()) return;
	cAmbient = c;
}

// ===========================================================================================
//
const FVECTOR3& D3D9Mesh::GetAmbientColor()
{
	_TRACE;
	return cAmbient;
}

// ===========================================================================================
//
void D3D9Mesh::SetupFog(const LPD3DXMATRIX pW)
{
	_TRACE;
	if (!IsOK()) return;
	FX->SetVector(eAttennuate, ptr(D3DXVECTOR4(1,1,1,1)));
	FX->SetVector(eInScatter,  ptr(D3DXVECTOR4(0,0,0,0)));
}

// ===========================================================================================
//
void D3D9Mesh::RenderGroup(int idx)
{
	RenderGroup(GetGroup(idx));
}

// ===========================================================================================
//
void D3D9Mesh::RenderGroup(const GROUPREC *grp)
{
	_TRACE;
	if (!IsOK()) return;
	if (!grp) return;

	pBuf->Map(pDev);

	pDev->SetVertexDeclaration(pMeshVertexDecl);
	pDev->SetStreamSource(0, pBuf->pVB, 0, sizeof(NMVERTEX));
	pDev->SetIndices(pBuf->pIB);
	pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, grp->VertOff, 0, grp->nVert, grp->IdexOff, grp->nFace);
	D3D9Stats.Mesh.Vertices += grp->nVert;
	D3D9Stats.Mesh.MeshGrps++;
}


// reset stucts template
template <typename T> void reset (T& _p)
#if __cplusplus > 201103L // C++11
	{ _p = { 0 }; }
#else
	{ static const T _Empty = { 0 }; _p = _Empty; }
#endif


// ===========================================================================================
//
void D3D9Mesh::ResetRenderStatus()
{
	for (DWORD g = 0; g < nGrp; g++) Grp[g].bRendered = false;
}


// ===========================================================================================
//
bool D3D9Mesh::IsGroupRendered(DWORD idx) const
{
	return Grp[idx].bRendered;
}


// ================================================================================================
// Analyze mesh. Must be called when ever there is a change in textures or materials
//
void D3D9Mesh::CheckMeshStatus()
{
	UpdateFlags();

	bCanRenderFast = true;
	bIsReflective = false;
	D3D9MatExt *mat = NULL;

	for (DWORD g = 0; g < nGrp; g++) {

		Grp[g].Shader = SHADER_PBR;
		Grp[g].PBRStatus = 0;

		DWORD ti = Grp[g].TexIdx;

		if (Tex[ti] != NULL) {
			if (Tex[ti]->IsAdvanced()) {
				bCanRenderFast = false;
				if (Tex[ti]->GetMap(MAP_METALNESS) && (DefShader == SHADER_NULL)) DefShader = SHADER_METALNESS;
				if (Tex[ti]->GetMap(MAP_SPECULAR)) Grp[g].PBRStatus |= 0x2;
				if (Tex[ti]->GetMap(MAP_ROUGHNESS)) Grp[g].PBRStatus |= 0x4;
				if (Tex[ti]->GetMap(MAP_REFLECTION)) Grp[g].PBRStatus |= 0x8;
				if (Tex[ti]->GetMap(MAP_TRANSLUCENCE)) Grp[g].Shader = SHADER_ADV;
				if (Tex[ti]->GetMap(MAP_TRANSMITTANCE)) Grp[g].Shader = SHADER_ADV;
			}
		}

		if (Grp[g].MtrlIdx == SPEC_DEFAULT) mat = &defmat;
		else mat = &Mtrl[Grp[g].MtrlIdx];

		if (mat->ModFlags&D3D9MATEX_SPECULAR) Grp[g].PBRStatus |= 0x1;
		if (mat->ModFlags&D3D9MATEX_REFLECT) Grp[g].PBRStatus |= 0x8;
		if (mat->ModFlags&D3D9MATEX_ROUGHNESS) Grp[g].PBRStatus |= 0x4;
		if (mat->ModFlags&D3D9MATEX_FRESNEL) Grp[g].PBRStatus |= 0x10;
	}


	for (DWORD g = 0; g < nGrp; g++) {
		if (Grp[g].PBRStatus >= (0x8|0x2)) bIsReflective = true;
		if (Grp[g].PBRStatus >= 0x2) bCanRenderFast = false;
	}

	if (DefShader == SHADER_METALNESS) {
		bIsReflective = true;
		for (DWORD g = 0; g < nGrp; g++) Grp[g].Shader = SHADER_METALNESS;
	}

	if (DefShader == SHADER_BAKED_VC) {
		bIsReflective = true;
		for (DWORD g = 0; g < nGrp; g++) Grp[g].Shader = SHADER_BAKED_VC;
	}
}


// ===========================================================================================
//
void D3D9Mesh::SetDefaultShader(WORD shader)
{
	DefShader = shader;
	bMtrlModidied = true;
	CheckMeshStatus();

	if (shader == SHADER_BAKED_VC) {
		LoadBakedLights();
	}
}


// ===========================================================================================
//
void D3D9Mesh::ConfigureAtmo()
{
	//LogSunLight(sunLight);
	float x = 1.0f - saturate(max(sunLight.Color.r, sunLight.Color.b) * 2.0f);
	FX->SetFloat(eNight, x);
	if (DebugControls::IsActive() && DebugControls::debugFlags & DBG_FLAGS_NODYNSUN)
		sunLight.Color = sunLight.Ambient = 0;
	FX->SetValue(eSun, &sunLight, sizeof(D3D9Sun));
}


// ===========================================================================================
// Setup shadow map samplers and textures
//
void D3D9Mesh::ConfigureShadows()
{
	for (int i = 0; i < SHM_CASCADE_COUNT; i++)
	{
		int idx = 13 + i; // Sampler index
		HR(pDev->SetSamplerState(idx, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
		HR(pDev->SetSamplerState(idx, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
		HR(pDev->SetSamplerState(idx, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP));	
		HR(pDev->SetSamplerState(idx, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR));
		HR(pDev->SetSamplerState(idx, D3DSAMP_MINFILTER, D3DTEXF_LINEAR));
		HR(pDev->SetSamplerState(idx, D3DSAMP_MIPFILTER, D3DTEXF_NONE));
		HR(pDev->SetTexture(idx, pShadowMap[i]));
	}
	HR(FX->SetValue(D3D9Effect::eSHDSubRect, ShdSubRect, sizeof(ShdSubRect)));
}


// ===========================================================================================
// static:
void D3D9Mesh::SetShadows(const SHADOWMAP *sprm)
{
	if (sprm) {
		for (int i = 0; i < SHM_CASCADE_COUNT; i++) {
			pShadowMap[i] = sprm->Map(i);
			ShdSubRect[i] = sprm->SubrectTF[i];
		}
	}
	else {
		for (int i = 0; i < SHM_CASCADE_COUNT; i++) {
			pShadowMap[i] = NULL;
			ShdSubRect[i] = { 0,0,0,0 };
		}
	}
}


// ================================================================================================
// This is a rendering routine for a Exterior Mesh, non-spherical moons/asteroids
//
void D3D9Mesh::Render(const LPD3DXMATRIX pW, const ENVCAMREC* em, int iTech)
{
	_TRACE;
	
	if (!IsOK()) return;

	pBuf->Map(pDev);

	// Check material status
	//
	if (bMtrlModidied) {
		CheckMeshStatus();
		bMtrlModidied = false;
	}

	if (DebugControls::IsActive() == false) {
		if (bCanRenderFast && vClass != VCLASS_XR2) {
			RenderFast(pW, iTech);
			return;
		}
	}

	DWORD flags=0, selmsh=0, selgrp=0, displ=0; // Debug Variables
	bool bActiveVisual = false;

	const VCHUDSPEC *hudspec;

	if (DebugControls::IsActive()) {
		flags  = *(DWORD*)gc->GetConfigParam(CFGPRM_GETDEBUGFLAGS);
		selmsh = *(DWORD*)gc->GetConfigParam(CFGPRM_GETSELECTEDMESH);
		selgrp = *(DWORD*)gc->GetConfigParam(CFGPRM_GETSELECTEDGROUP);
		displ  = *(DWORD*)gc->GetConfigParam(CFGPRM_GETDISPLAYMODE);
		bActiveVisual = (g_pCurrentVisual == DebugControls::GetVisual());
		if (displ>0 && !bActiveVisual) return;
		if ((displ==2 || displ==3) && g_uCurrentMesh!=selmsh) return;
	}

	Scene *scn = gc->GetScene();

	bool bWorldMesh = false;
	bool bMeshCull = true;
	bool bTextured = true;
	bool bGroupCull = true;
	bool bUpdateFlow = true;
	bool bShadowProjection = false;
	bool bVirtualCockpit = false;



	switch (iTech) {
		case RENDER_VC:
			EnablePlanetGlow(false);
			bVirtualCockpit = true;
			break;
		case RENDER_BASE:
			EnablePlanetGlow(false);
			bMeshCull = false;
			bShadowProjection = true;
			break;
		case RENDER_BASEBS:
			EnablePlanetGlow(false);
			bMeshCull = false;
			bShadowProjection = true;
			break;
		case RENDER_ASTEROID:
			EnablePlanetGlow(false);
			bMeshCull = false;
			bGroupCull = false;
			bShadowProjection = true;
			break;
		case RENDER_VESSEL:
			EnablePlanetGlow(true);
			break;
	}

	HR(D3D9Effect::FX->SetBool(D3D9Effect::eBaseBuilding, bShadowProjection));
	HR(D3D9Effect::FX->SetBool(D3D9Effect::eCockpit, bVirtualCockpit));

	D3DXVECTOR4 Field;
	D3DXMATRIX mWorldView,  q;

	D3DXMatrixMultiply(&mWorldView, pW, scn->GetViewMatrix());

	if (bMeshCull || bGroupCull) Field = D9LinearFieldOfView(scn->GetProjectionMatrix());

	if (bMeshCull) if (!D9IsAABBVisible(&BBox, &mWorldView, &Field)) {
		if (flags&(DBG_FLAGS_BOXES|DBG_FLAGS_SPHERES)) RenderBoundingBox(pW);
		return;
	}

	D3DXMATRIX mWorldMesh;

	if (bGlobalTF) D3DXMatrixMultiply(&mWorldMesh, &mTransform, pW);
	else mWorldMesh = *pW;

	D3D9Stats.Mesh.Meshes++;

	D3D9MatExt *mat, *old_mat = NULL;
	SURFHANDLE old_tex = NULL;

	pDev->SetVertexDeclaration(pMeshVertexDecl);
	pDev->SetStreamSource(0, pBuf->pVB, 0, sizeof(NMVERTEX));
	pDev->SetIndices(pBuf->pIB);

	ConfigureShadows();

	if (flags&DBG_FLAGS_DUALSIDED) pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

	FX->SetTechnique(eVesselTech);
	FX->SetBool(eFresnel, false);
	FX->SetBool(eLightsEnabled, false);
	FX->SetBool(eOITEnable, false);
	FX->SetVector(eColor, ptr(D3DXVECTOR4(0, 0, 0, 0)));
	FX->SetValue(eVCAmbient, &cAmbient, sizeof(FVECTOR3));

	ConfigureAtmo();


	TexFlow FC;	reset(FC);


	// Setup Local lights -------------------------------------------
	//
	const D3D9Light *pLights = gc->GetScene()->GetLights();
	int nSceneLights = gc->GetScene()->GetLightCount();

	for (int i = 0; i < Config->MaxLights(); i++) memcpy(&Locals[i], &null_light, sizeof(LightStruct));

	int nMeshLights = 0;

	if (pLights && nSceneLights>0) {

		D3DXVECTOR3 pos;
		D3DXVec3TransformCoord(&pos, ptr(D3DXVECTOR3f4(BBox.bs)), pW);

		// Find all local lights effecting this mesh ------------------------------------------
		//
		for (int i = 0; i < nSceneLights; i++) {
			float il = pLights[i].GetIlluminance(pos, BBox.bs.w);
			if (il > 0.005f) {
				LightList[nMeshLights].illuminace = il;
				LightList[nMeshLights++].idx = i;
			}
		}

		if (nMeshLights > 0) {

			FX->SetBool(eLightsEnabled, true);

			// If any, Sort the list based on illuminance -------------------------------------------
			qsort(LightList, nMeshLights, sizeof(_LightList), compare_lights);

			nMeshLights = min(nMeshLights, Config->MaxLights());

			// Create a list of N most effective lights ---------------------------------------------
			for (int i = 0; i < nMeshLights; i++) {
				memcpy(&Locals[i], &pLights[LightList[i].idx], sizeof(LightStruct));

				// Override application configuration to prevent oversaturation of lights at point plank range. 
				if (scn->GetRenderPass() == RENDERPASS_MAINSCENE)
					Locals[i].Attenuation.x = max(Locals[i].Attenuation.x, float(Config->GFXLocalMax));
			}
		}
	}

	FX->SetValue(eLights, Locals, sizeof(LightStruct) * Config->MaxLights());


	// Setup Env Maps -------------------------------------------
	//
	if (em && em->pCube && em->pIrrad) { 
		FX->SetBool(eEnvMapEnable, true);
		FX->SetTexture(eEnvMapA, em->pCube);
		FX->SetTexture(eIrradMap, em->pIrrad);
	}
	else {
		FX->SetBool(eEnvMapEnable, false);
	}

	bool bNoAmbient = (Config->ExpVCLight == 1);

	/*if (DebugControls::IsActive()) {
		bNoAmbient = (flags & DBG_FLAGS_NOSUNAMB) != 0 & (flags & DBG_FLAGS_NOPLNAMB) != 0;
	}*/



	UINT numPasses = 0;
	HR(FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE));

	WORD CurrentShader = SHADER_NULL;

	bool bRefl = true;
	int iEnvCam = -1;

	for (DWORD g=0; g<nGrp; g++) {


		bool bHUD = (Grp[g].MFDScreenId == 0x100);

		// Inline engine renders HUD/MFDs in a separate rendering pass and flag 0x2 is used to disable rendering during the main rendering pass
		if ((Grp[g].UsrFlag & 0x2) && (!bHUD)) continue;


		// Check skip conditions ==========================================
		//
		DWORD ti = Grp[g].TexIdx;
		DWORD tni = Grp[g].TexIdxEx[0];

		if (ti == 0 && tni != 0) continue;


		// Cull unvisible geometry ----------------------------------------
		//
		if (bGroupCull) if (!D9IsBSVisible(&Grp[g].BBox, &mWorldView, &Field)) continue;



		// Enforce special shader for XR2 HUD to bypass faulty materials
		if ((vClass == VCLASS_XR2) && (Grp[g].MFDScreenId == 0x100)) Grp[g].Shader = SHADER_XR2HUD;



		// Begin rendering of a specified pass ------------------------------
		//
		if (Grp[g].Shader != CurrentShader) {
			if (CurrentShader != SHADER_NULL) { HR(FX->EndPass()); }
			HR(FX->BeginPass(Grp[g].Shader));
			CurrentShader = Grp[g].Shader;
			if (iTech == RENDER_BASEBS) pDev->SetRenderState(D3DRS_ZENABLE, 0);	// Must be here because BeginPass() sets it enabled
		}



		// Mesh Debugger -------------------------------------------------------------------------------------------
		//
		if (DebugControls::IsActive()) {

			if (bActiveVisual) {

				if (displ==3 && g!=selgrp) continue;

				FX->SetVector(eColor, ptr(D3DXVECTOR4(0, 0, 0, 0)));

				if (flags&DBG_FLAGS_HLMESH) {
					if (g_uCurrentMesh==selmsh) {
						FX->SetVector(eColor, ptr(D3DXVECTOR4(0.0f, 0.0f, 0.5f, 0.5f)));
					}
				}
				if (flags&DBG_FLAGS_HLGROUP) {				
					if (g == selgrp && g_uCurrentMesh == selmsh) {
						FX->SetVector(eColor, ptr(D3DXVECTOR4(0.0f, 0.5f, 0.0f, 0.5f)));
					}
				}
			}
		}


		// ---------------------------------------------------------------------------------------------------------
		//
		if (Tex[ti] != old_tex) {
			reset(FC);
			bUpdateFlow = true;
		}

		if (Tex[ti]==NULL) bTextured = false, old_tex = NULL;
		else bTextured = true;


		
		// Setup Textures and Normal Maps ==========================================================================
		//
		if (bTextured) {

			if (Tex[ti]!=old_tex) {

				D3D9Stats.Mesh.TexChanges++;

				old_tex = Tex[ti];

				FX->SetTexture(eTex0, Tex[ti]->GetTexture());

				if (CurrentShader == SHADER_LEGACY) {
					if (tni && Grp[g].TexMixEx[0] < 0.5f) tni = 0;
					if (tni && Tex[tni]) FX->SetTexture(eEmisMap, Tex[tni]->GetTexture());
				}
				else {
					LPDIRECT3DTEXTURE9 pTransl = NULL;
					LPDIRECT3DTEXTURE9 pTransm = NULL;
					LPDIRECT3DTEXTURE9 pMetl = NULL;
					LPDIRECT3DTEXTURE9 pRefl = Tex[ti]->GetMap(MAP_REFLECTION);
					LPDIRECT3DTEXTURE9 pNorm = Tex[ti]->GetMap(MAP_NORMAL);
					LPDIRECT3DTEXTURE9 pRghn = Tex[ti]->GetMap(MAP_ROUGHNESS);
					LPDIRECT3DTEXTURE9 pEmis = Tex[ti]->GetMap(MAP_EMISSION);
					LPDIRECT3DTEXTURE9 pHeat = Tex[ti]->GetMap(MAP_HEAT);
					LPDIRECT3DTEXTURE9 pSpec = Tex[ti]->GetMap(MAP_SPECULAR);

					if (tni && Grp[g].TexMixEx[0] < 0.5f) tni = 0;
					if (!pEmis && tni && Tex[tni]) pEmis = Tex[tni]->GetTexture();

					if (pNorm) FX->SetTexture(eTex3, pNorm);
					if (pRghn) FX->SetTexture(eRghnMap, pRghn);
					if (pMetl) FX->SetTexture(eMetlMap, pMetl);
					if (pEmis) FX->SetTexture(eEmisMap, pEmis);
					if (pHeat) FX->SetTexture(eHeatMap, pHeat);
					if (pSpec) FX->SetTexture(eSpecMap, pSpec);
					if (pRefl) FX->SetTexture(eReflMap, pRefl);

					if (CurrentShader == SHADER_ADV
						|| CurrentShader == SHADER_METALNESS
						|| CurrentShader == SHADER_BAKED_VC)
					{
						pTransl = Tex[ti]->GetMap(MAP_TRANSLUCENCE);
						pTransm = Tex[ti]->GetMap(MAP_TRANSMITTANCE);
						pMetl = Tex[ti]->GetMap(MAP_METALNESS);

						if (pTransl) FX->SetTexture(eTranslMap, pTransl);
						if (pTransm) FX->SetTexture(eTransmMap, pTransm);
						if (pMetl) FX->SetTexture(eMetlMap, pMetl);

						FC.Transl = (pTransl != NULL);
						FC.Transm = (pTransm != NULL);	
						FC.Metl = (pMetl != NULL);
					}
					else {
						FC.Transl = false;
						FC.Transm = false;
						FC.Metl = false;
					}

					if (CurrentShader == SHADER_BAKED_VC)
					{
						auto bm = BakedLights.find(ti);

						LPDIRECT3DTEXTURE9 pAmbi = Tex[ti]->GetMap(MAP_AMBIENT);
						LPDIRECT3DTEXTURE9 pComb = (bm == BakedLights.end() ? NULL : bm->second.pCombined);
						LPDIRECT3DTEXTURE9 pSun = (bm == BakedLights.end() ? NULL : bm->second.pSunAOComb);

						if (pAmbi) FX->SetTexture(eAmbientMap, pAmbi);
						if (pComb) FX->SetTexture(eCombinedMap, pComb);
						if (pSun)  FX->SetTexture(eCombSunMap, pSun);

						FC.Baked = (pComb != NULL);
						FC.BakedAO = (pAmbi != NULL);
						FC.BakedAmb = (pSun != NULL) & !bNoAmbient;
					}

					FC.Emis = (pEmis != NULL);
					FC.Norm = (pNorm != NULL);
					FC.Rghn = (pRghn != NULL);
					FC.Heat = (pHeat != NULL);
					FC.Spec = (pSpec != NULL);
					FC.Refl = (pRefl != NULL);
				}
			}
		}

		// Apply MFD Screen Override ================================================================================
		//
		if (Grp[g].MFDScreenId) {

			bTextured = true;
			old_tex = NULL;
			old_mat = NULL;
			reset(FC);
			bUpdateFlow = true;

			SURFHANDLE hMFD;
			if (bHUD) hMFD = gc->GetVCHUDSurface(&hudspec);
			else hMFD = gc->GetMFDSurface(Grp[g].MFDScreenId - 1);

			if (hMFD) FX->SetTexture(eTex0, SURFACE(hMFD)->GetTexture());
			else	  FX->SetTexture(eTex0, gc->GetDefaultTexture()->GetTexture());

			if (Grp[g].MtrlIdx==SPEC_DEFAULT) mat = &mfdmat;
			else							  mat = &Mtrl[Grp[g].MtrlIdx];

			if (bModulateMatAlpha || bTextured==false)  FX->SetFloat(eMtrlAlpha, mat->Diffuse.w);
			else										FX->SetFloat(eMtrlAlpha, 1.0f);

			FX->SetValue(eMtrl, mat, sizeof(D3D9MatExt)-4);
		}


		// Setup Mesh group material  ==========================================================================
		//
		else {

			if (Grp[g].MtrlIdx==SPEC_DEFAULT) mat = &defmat;
			else							  mat = &Mtrl[Grp[g].MtrlIdx];

			if (mat!=old_mat) {

				D3D9Stats.Mesh.MtrlChanges++;

				old_mat = mat;

				FX->SetValue(eMtrl, mat, sizeof(D3D9MatExt)-4);

				if (bModulateMatAlpha || bTextured==false)  FX->SetFloat(eMtrlAlpha, mat->Diffuse.w);
				else										FX->SetFloat(eMtrlAlpha, 1.0f);
			}
		}


		// Apply Animations =========================================================================================
		//
		if (Grp[g].bTransform) {
			bWorldMesh = false;
			FX->SetMatrix(eW, D3DXMatrixMultiply(&q, &pGrpTF[g], pW));
		}
		else if (!bWorldMesh) {
			FX->SetMatrix(eW, &mWorldMesh);
			bWorldMesh = true;
		}

		if (bUpdateFlow) {
			bUpdateFlow = false;
			HR(FX->SetValue(eFlow, &FC, sizeof(TexFlow)));
		}

		bool bPBR = (Grp[g].PBRStatus & 0xF) == (0x8 + 0x4);
		bool bRGH = (Grp[g].PBRStatus & 0xE) == (0x8 + 0x2);
		bool bNoL = (Grp[g].UsrFlag & 0x04) != 0;
		bool bNoC = (Grp[g].UsrFlag & 0x10) != 0;
		bool bOIT = (Grp[g].UsrFlag & 0x20) != 0;
		bool bENV = false;
		bool bFRS = false;

		// Setup Mesh drawing options =================================================================================
		//
		FX->SetBool(eOITEnable, bOIT);
		FX->SetBool(eTextured, bTextured);
		FX->SetBool(eFullyLit, bNoL);
		FX->SetBool(eSwitch, bPBR);
		FX->SetBool(eRghnSw, bRGH);

		if (bNoC) FX->SetValue(eNoColor, &D3DXVECTOR3(1, 1, 1), sizeof(D3DXVECTOR3));
		else FX->SetValue(eNoColor, &D3DXVECTOR3(0, 0, 0), sizeof(D3DXVECTOR3));

		// Update envmap and fresnel status as required
		if (bRefl) {
			bFRS = (Grp[g].PBRStatus & 0x1E) >= 0x10;
			FX->SetBool(eFresnel, bFRS);
			if (IsReflective()) {			
				bENV = ((Grp[g].PBRStatus & 0x1E) >= 0xA) | (Grp[g].Shader == SHADER_METALNESS) | (Grp[g].Shader == SHADER_BAKED_VC);
				FX->SetBool(eEnvMapEnable, bENV);
			}
		}

		

		FX->CommitChanges();



		// Mesh Debugger -------------------------------------------------------------------------------------------
		//
		if (DebugControls::IsActive()) {

			if ((bActiveVisual) && (g == selgrp) && (g_uCurrentMesh == selmsh)) {

				bool bAdd = (Grp[g].UsrFlag & 0x08) != 0;
				bool bNoS = (Grp[g].UsrFlag & 0x01) != 0;

				static const char *YesNo[2] = { "No", "Yes" };
				static const char *LPW[2] = { "Legacy", "PBR" };
				static const char *RGH[2] = { "Disabled", "Enabled" };
				static const char *Shaders[7] = { "PBR", "PBR-ADV", "FAST", "XR2", "METALNESS", "BAKED_VC", "???"};

				DebugControls::Append("MeshIdx = %d, GrpIdx = %d\n", g_uCurrentMesh, g);
				DebugControls::Append("MtrlIdx = %d, TexIdx = %d\n", Grp[g].MtrlIdx, Grp[g].TexIdx);
				DebugControls::Append("FaceCnt = %d, VtxCnt = %d\n", Grp[g].nFace, Grp[g].nVert);

				DebugControls::Append("GroupFlags.. = 0x%X\n", Grp[g].UsrFlag);
				DebugControls::Append("Shader...... = %s\n", Shaders[CurrentShader]);
				DebugControls::Append("Textured.... = %s\n", YesNo[bTextured]);
				DebugControls::Append("ModMatAlpha. = %s\n", YesNo[bModulateMatAlpha]);
				DebugControls::Append("NoColor..... = %s\n", YesNo[bNoC]);
				DebugControls::Append("NoLighting.. = %s\n", YesNo[bNoL]);
				DebugControls::Append("NoShadow.... = %s\n", YesNo[bNoS]);
				DebugControls::Append("Additive.... = %s\n", YesNo[bAdd]);

				if (CurrentShader == SHADER_PBR || CurrentShader == SHADER_ADV || CurrentShader == SHADER_METALNESS)
				{
					if (CurrentShader != SHADER_METALNESS) {
						DebugControls::Append("\nPBR-Shader State:\n");
						DebugControls::Append("PBR-Switch.. = %s\n", LPW[bPBR]);
						DebugControls::Append("Rghn-Conver. = %s\n", RGH[bRGH]);
						DebugControls::Append("Fresnel Mode = %s\n", RGH[bFRS]);
					}
					
					DebugControls::Append("Env Mapping. = %s\n", RGH[bENV]);

					DebugControls::Append("TextureMaps = [ ");
					if (FC.Emis) DebugControls::Append("emis ");
					if (FC.Metl) DebugControls::Append("metl ");
					if (FC.Norm) DebugControls::Append("norm ");
					if (FC.Rghn) DebugControls::Append("rghn ");
					if (FC.Spec) DebugControls::Append("spec ");
					if (FC.Refl) DebugControls::Append("refl ");
					if (FC.Transl) DebugControls::Append("transl ");
					if (FC.Transm) DebugControls::Append("transm ");
					DebugControls::Append("]\n");
				}

				if (CurrentShader == SHADER_BAKED_VC) {
					DebugControls::Append("BakedLights = [ ");
					for (auto& a : BakedLights) DebugControls::Append(" %d", a.first);
					DebugControls::Append("]\n");
				}

				DebugControls::Append("Local Lights = %d\n", nMeshLights);

				DebugControls::Refresh();
			}
		}


		// Start rendering -------------------------------------------------------------------------------------------
		//
		if (bHUD) {
			pDev->SetRenderState(D3DRS_ZENABLE, 0);
			pDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		}

		if (Grp[g].bDualSided) {
			pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
			pDev->SetRenderState(D3DRS_ZWRITEENABLE, 0);
			pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, Grp[g].VertOff, 0, Grp[g].nVert, Grp[g].IdexOff, Grp[g].nFace);
			pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
		}

		DWORD dwMSAA;

		if (bOIT) {
			pDev->GetRenderState(D3DRS_MULTISAMPLEANTIALIAS, &dwMSAA);
			pDev->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, 0);
		}

		pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, Grp[g].VertOff,  0, Grp[g].nVert,  Grp[g].IdexOff, Grp[g].nFace);

		Grp[g].bRendered = true;

		if (bOIT && dwMSAA) {
			pDev->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, dwMSAA);
		}

		if (Grp[g].bDualSided) {
			pDev->SetRenderState(D3DRS_ZWRITEENABLE, 1);
		}

		if (bHUD) {
			pDev->SetRenderState(D3DRS_ZENABLE, 1);
			pDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		}

		D3D9Stats.Mesh.Vertices += Grp[g].nVert;
		D3D9Stats.Mesh.MeshGrps++;

	}

	if (CurrentShader != 0xFFFF) {
		HR(FX->EndPass());
	}

	HR(FX->End());

	if (flags&(DBG_FLAGS_BOXES|DBG_FLAGS_SPHERES)) RenderBoundingBox(pW);
	FX->SetVector(eColor, ptr(D3DXVECTOR4(0, 0, 0, 0)));
	if (flags&DBG_FLAGS_DUALSIDED) pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
}



// ================================================================================================
// Render without animations 
//
void D3D9Mesh::RenderSimplified(const LPD3DXMATRIX pW, LPDIRECT3DCUBETEXTURE9 *pEnv, int nEnv, bool bSP)
{
	if (!IsOK()) return;

	pBuf->Map(pDev);

	// Check material status
	//
	if (bMtrlModidied) {
		CheckMeshStatus();
		bMtrlModidied = false;
	}

	Scene *scn = gc->GetScene();

	bool bMeshCull = true;
	bool bTextured = true;
	bool bGroupCull = true;
	bool bUpdateFlow = true;
	
	EnablePlanetGlow(true);
	
	HR(D3D9Effect::FX->SetBool(D3D9Effect::eBaseBuilding, bSP));

	D3D9MatExt *mat, *old_mat = NULL;
	SURFHANDLE old_tex = NULL;
	TexFlow FC;	reset(FC);

	pDev->SetVertexDeclaration(pMeshVertexDecl);
	pDev->SetStreamSource(0, pBuf->pVB, 0, sizeof(NMVERTEX));
	pDev->SetIndices(pBuf->pIB);

	FX->SetTechnique(eVesselTech);
	FX->SetBool(eFresnel, false);
	FX->SetBool(eEnvMapEnable, false);
	FX->SetBool(eLightsEnabled, false);
	FX->SetVector(eColor, ptr(D3DXVECTOR4(0, 0, 0, 0)));
	FX->SetMatrix(eW, pW);

	ConfigureAtmo();

	// Process Local Light Sources ------------------------------------------------------------
	//
	const D3D9Light *pLights = gc->GetScene()->GetLights();
	int nSceneLights = gc->GetScene()->GetLightCount();

	for (int i = 0; i < Config->MaxLights(); i++) memcpy(&Locals[i], &null_light, sizeof(LightStruct));

	//D3D9DebugLog("Mesh=[%s], nLights=%d", GetName(), nSceneLights);

	if (pLights && nSceneLights>0) {

		int nMeshLights = 0;
		D3DXVECTOR3 pos;
		D3DXVec3TransformCoord(&pos, ptr(D3DXVECTOR3f4(BBox.bs)), pW);

		// Find all local lights effecting this mesh ------------------------------------------
		//
		for (int i = 0; i < nSceneLights; i++) {
			float il = pLights[i].GetIlluminance(pos, BBox.bs.w);
			if (il > 0.005f) {
				LightList[nMeshLights].illuminace = il;
				LightList[nMeshLights++].idx = i;
			}
		}

		if (nMeshLights > 0) {

			FX->SetBool(eLightsEnabled, true);

			// If any, Sort the list based on illuminance -------------------------------------------
			qsort(LightList, nMeshLights, sizeof(_LightList), compare_lights);

			nMeshLights = min(nMeshLights, Config->MaxLights());

			// Create a list of N most effective lights ---------------------------------------------
			for (int i = 0; i < nMeshLights; i++) memcpy(&Locals[i], &pLights[LightList[i].idx], sizeof(LightStruct));
		}
	}

	FX->SetValue(eLights, Locals, sizeof(LightStruct) * Config->MaxLights());

	if (nEnv >= 1 && pEnv[0]) FX->SetTexture(eEnvMapA, pEnv[0]);

	bool bRefl = true;
	WORD CurrentShader = 0xFFFF;
	UINT numPasses = 0;

	HR(FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE));


	// Render MeshGroups ----------------------------------------------------
	//
	for (DWORD g = 0; g<nGrp; g++) {

		// Check skip conditions --------------------------------------------
		//
		DWORD ti = Grp[g].TexIdx;
		if (Grp[g].UsrFlag & 0x2) continue;


		// Begin rendering of a specified pass ------------------------------
		//
		if (Grp[g].Shader != CurrentShader) {
			if (CurrentShader != 0xFFFF) { HR(FX->EndPass()); }
			HR(FX->BeginPass(Grp[g].Shader));
			CurrentShader = Grp[g].Shader;
		}

		// -------------------------------------------------------------------
		//
		if (Tex[ti] != old_tex) {
			if (Tex[ti] == NULL) {
				reset(FC);
				bUpdateFlow = true;
			}
		}

		if (Tex[ti] == NULL) bTextured = false, old_tex = NULL;
		else bTextured = true;


		// Setup Textures and Normal Maps =====================================
		//
		if (bTextured) {

			if (Tex[ti] != old_tex) {

				old_tex = Tex[ti];
				FX->SetTexture(eTex0, Tex[ti]->GetTexture());
				bUpdateFlow = true;	// Fix this later

				LPDIRECT3DTEXTURE9 pTransl = NULL;
				LPDIRECT3DTEXTURE9 pTransm = NULL;
				LPDIRECT3DTEXTURE9 pSpec = Tex[ti]->GetMap(MAP_SPECULAR);
				LPDIRECT3DTEXTURE9 pNorm = Tex[ti]->GetMap(MAP_NORMAL);
				LPDIRECT3DTEXTURE9 pRefl = Tex[ti]->GetMap(MAP_REFLECTION);
				LPDIRECT3DTEXTURE9 pRghn = Tex[ti]->GetMap(MAP_ROUGHNESS);
				LPDIRECT3DTEXTURE9 pMetl = Tex[ti]->GetMap(MAP_METALNESS);
				LPDIRECT3DTEXTURE9 pEmis = Tex[ti]->GetMap(MAP_EMISSION);

				if (pNorm) FX->SetTexture(eTex3, pNorm);
				if (pRghn) FX->SetTexture(eRghnMap, pRghn);
				if (pRefl) FX->SetTexture(eReflMap, pRefl);
				if (pMetl) FX->SetTexture(eMetlMap, pMetl);
				if (pSpec) FX->SetTexture(eSpecMap, pSpec);
				if (pEmis) FX->SetTexture(eEmisMap, pEmis);

				if (CurrentShader == SHADER_ADV) {

					pTransl = Tex[ti]->GetMap(MAP_TRANSLUCENCE);
					pTransm = Tex[ti]->GetMap(MAP_TRANSMITTANCE);

					if (pTransl) FX->SetTexture(eTranslMap, pTransl);
					if (pTransm) FX->SetTexture(eTransmMap, pTransm);

					FC.Transl = (pTransl != NULL);
					FC.Transm = (pTransm != NULL);
				}
				else {
					FC.Transl = false;
					FC.Transm = false;
				}

				FC.Emis = (pEmis != NULL);
				FC.Metl = (pMetl != NULL);
				FC.Norm = (pNorm != NULL);
				FC.Rghn = (pRghn != NULL);
				FC.Spec = (pSpec != NULL);
				FC.Refl = (pRefl != NULL);
			}	
		}


		// Setup Mesh group material  ==========================================
		//
		else {
			if (Grp[g].MtrlIdx == SPEC_DEFAULT) mat = &defmat;
			else mat = &Mtrl[Grp[g].MtrlIdx];
			if (mat != old_mat) {
				old_mat = mat;
				FX->SetValue(eMtrl, mat, sizeof(D3D9MatExt) - 4);
				if (bModulateMatAlpha || bTextured == false) FX->SetFloat(eMtrlAlpha, mat->Diffuse.w);
				else FX->SetFloat(eMtrlAlpha, 1.0f);
			}
		}

		// Must update FlowControl ?
		//
		if (bUpdateFlow) {
			bUpdateFlow = false;
			HR(FX->SetValue(eFlow, &FC, sizeof(TexFlow)));
		}

		bool bPBR = (Grp[g].PBRStatus & 0xF) == (0x8 + 0x4);
		bool bRGH = (Grp[g].PBRStatus & 0xE) == (0x8 + 0x2);
		bool bNoL = (Grp[g].UsrFlag & 0x04) != 0;
		bool bNoC = (Grp[g].UsrFlag & 0x10) != 0;
		bool bOIT = (Grp[g].UsrFlag & 0x20) != 0;
		bool bENV = false;
		bool bFRS = false;


		// Setup Mesh drawing options =================================================================================
		//
		FX->SetBool(eOITEnable, bOIT);
		FX->SetBool(eTextured, bTextured);
		FX->SetBool(eFullyLit, bNoL);
		FX->SetBool(eSwitch, bPBR);
		FX->SetBool(eRghnSw, bRGH);

		if (bNoC) FX->SetValue(eNoColor, &D3DXVECTOR3(1, 1, 1), sizeof(D3DXVECTOR3));
		else FX->SetValue(eNoColor, &D3DXVECTOR3(0, 0, 0), sizeof(D3DXVECTOR3));


		// Update envmap and fresnel status as required
		//
		if (bRefl) {
			bFRS = (Grp[g].PBRStatus & 0x1E) >= 0x10;
			FX->SetBool(eFresnel, bFRS);
			if (IsReflective()) {
				bENV = (Grp[g].PBRStatus & 0x1E) >= 0xA;
				FX->SetBool(eEnvMapEnable, bENV);
			}
		}

		// Start rendering -------------------------------------------------------------------------------------------
		//
		FX->CommitChanges();

		DWORD dwMSAA = 0;

		if (bOIT) {
			pDev->GetRenderState(D3DRS_MULTISAMPLEANTIALIAS, &dwMSAA);
			pDev->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, 0);
		}

		pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, Grp[g].VertOff, 0, Grp[g].nVert, Grp[g].IdexOff, Grp[g].nFace);
		Grp[g].bRendered = true;

		if (bOIT && dwMSAA) {
			pDev->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, dwMSAA);		
		}
	}

	if (CurrentShader != 0xFFFF) HR(FX->EndPass());
	HR(FX->End());
}


// ================================================================================================
// Render a legacy orbiter mesh without any additional textures
//
void D3D9Mesh::RenderFast(const LPD3DXMATRIX pW, int iTech)
{

	_TRACE;
	DWORD flags = 0, selmsh = 0, selgrp = 0, displ = 0; // Debug Variables
	bool bActiveVisual = false;

	const VCHUDSPEC *hudspec;

	if (!IsOK()) return;

	pBuf->Map(pDev);

	if (DebugControls::IsActive()) {
		flags = *(DWORD*)gc->GetConfigParam(CFGPRM_GETDEBUGFLAGS);
		selmsh = *(DWORD*)gc->GetConfigParam(CFGPRM_GETSELECTEDMESH);
		selgrp = *(DWORD*)gc->GetConfigParam(CFGPRM_GETSELECTEDGROUP);
		displ = *(DWORD*)gc->GetConfigParam(CFGPRM_GETDISPLAYMODE);
		bActiveVisual = (g_pCurrentVisual == DebugControls::GetVisual());
		if (displ>0 && !bActiveVisual) return;
		if ((displ == 2 || displ == 3) && g_uCurrentMesh != selmsh) return;
	}

	Scene *scn = gc->GetScene();

	bool bWorldMesh = false;
	bool bMeshCull = true;
	bool bTextured = true;
	bool bGroupCull = true;
	bool bUpdateFlow = true;
	bool bShadowProjection = false;
	bool bVirtualCockpit = false;

	switch (iTech) {
		case RENDER_VC:
			bVirtualCockpit = true;
			EnablePlanetGlow(false);
			break;
		case RENDER_BASE:
			EnablePlanetGlow(false);
			bMeshCull = false;
			bShadowProjection = true;
			break;
		case RENDER_BASEBS:
			EnablePlanetGlow(false);
			bMeshCull = false;
			bShadowProjection = true;
			break;
		case RENDER_ASTEROID:
			EnablePlanetGlow(false);
			bMeshCull = false;
			bGroupCull = false;
			bShadowProjection = true;
			break;
		case RENDER_VESSEL:
			EnablePlanetGlow(true);
			break;
	}

	HR(D3D9Effect::FX->SetBool(D3D9Effect::eBaseBuilding, bShadowProjection));
	HR(D3D9Effect::FX->SetBool(D3D9Effect::eCockpit, bVirtualCockpit));

	D3DXVECTOR4 Field;
	D3DXMATRIX mWorldView, q;

	D3DXMatrixMultiply(&mWorldView, pW, scn->GetViewMatrix());

	if (bMeshCull || bGroupCull) Field = D9LinearFieldOfView(scn->GetProjectionMatrix());

	if (bMeshCull) if (!D9IsAABBVisible(&BBox, &mWorldView, &Field)) {
		if (flags&(DBG_FLAGS_BOXES | DBG_FLAGS_SPHERES)) RenderBoundingBox(pW);
		return;
	}

	D3DXMATRIX mWorldMesh;

	if (bGlobalTF) D3DXMatrixMultiply(&mWorldMesh, &mTransform, pW);
	else mWorldMesh = *pW;

	D3D9Stats.Mesh.Meshes++;

	D3D9MatExt *mat, *old_mat = NULL;
	SURFHANDLE old_tex = NULL;
	LPDIRECT3DTEXTURE9 pEmis_old = NULL;

	pDev->SetVertexDeclaration(pMeshVertexDecl);
	pDev->SetStreamSource(0, pBuf->pVB, 0, sizeof(NMVERTEX));
	pDev->SetIndices(pBuf->pIB);

	if (flags&DBG_FLAGS_DUALSIDED) pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);


	FX->SetTechnique(eVesselTech);
	FX->SetBool(eLightsEnabled, false);
	FX->SetVector(eColor, ptr(D3DXVECTOR4(0, 0, 0, 0)));

	ConfigureAtmo();
	TexFlow FC;	reset(FC);

	const D3D9Light *pLights = gc->GetScene()->GetLights();
	int nSceneLights = gc->GetScene()->GetLightCount();

	for (int i = 0; i < Config->MaxLights(); i++) memcpy(&Locals[i], &null_light, sizeof(LightStruct));

	//D3D9DebugLog("Mesh=[%s], nLights=%d", GetName(), nSceneLights);

	if (pLights && nSceneLights>0) {

		int nMeshLights = 0;
		D3DXVECTOR3 pos;
		D3DXVec3TransformCoord(&pos, ptr(D3DXVECTOR3f4(BBox.bs)), pW);

		// Find all local lights effecting this mesh ------------------------------------------
		//
		for (int i = 0; i < nSceneLights; i++) {
			float il = pLights[i].GetIlluminance(pos, BBox.bs.w);
			if (il > 0.0) {
				LightList[nMeshLights].illuminace = il;
				LightList[nMeshLights++].idx = i;
			}
		}

		if (nMeshLights > 0) {

			FX->SetBool(eLightsEnabled, true);

			// If any, Sort the list based on illuminance -------------------------------------------
			qsort(LightList, nMeshLights, sizeof(_LightList), compare_lights);

			nMeshLights = min(nMeshLights, Config->MaxLights());

			// Create a list of N most effective lights ---------------------------------------------
			int i;
			for (i = 0; i < nMeshLights; i++) memcpy(&Locals[i], &pLights[LightList[i].idx], sizeof(LightStruct));
		}
	}

	FX->SetValue(eLights, Locals, sizeof(LightStruct) * Config->MaxLights());

	UINT numPasses = 0;
	HR(FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE));

	// Begin rendering of a specified pass ------------------------------
	//
	HR(FX->BeginPass(2));

	if (iTech == RENDER_BASEBS) pDev->SetRenderState(D3DRS_ZENABLE, 0);	// Must be here because BeginPass() sets it enabled

	for (DWORD g = 0; g<nGrp; g++) {

		// This mesh group must be skipped by user orders
		if ((Grp[g].UsrFlag & 0x2) && (Grp[g].MFDScreenId != 0x100)) continue;

		bool bHUD = Grp[g].MFDScreenId == 0x100;

		// Mesh Debugger -------------------------------------------------------------------------------------------
		//
		if (DebugControls::IsActive()) {

			if (bActiveVisual) {

				if (displ == 3 && g != selgrp) continue;

				FX->SetVector(eColor, ptr(D3DXVECTOR4(0, 0, 0, 0)));

				if (flags&DBG_FLAGS_HLMESH) {
					if (g_uCurrentMesh == selmsh) {
						FX->SetVector(eColor, ptr(D3DXVECTOR4(0.0f, 0.0f, 0.5f, 0.5f)));
					}
				}
				if (flags&DBG_FLAGS_HLGROUP) {
					if (g == selgrp && g_uCurrentMesh == selmsh) {
						FX->SetVector(eColor, ptr(D3DXVECTOR4(0.0f, 0.5f, 0.0f, 0.5f)));
					}
				}
			}
		}


		// ---------------------------------------------------------------------------------------------------------
		//
		DWORD ti = Grp[g].TexIdx;
		DWORD tni = Grp[g].TexIdxEx[0];

		if (ti == 0 && tni != 0) continue;

		if (Tex[ti] == NULL || ti == 0) bTextured = false, old_tex = NULL;
		else						    bTextured = true;


		// Cull unvisible geometry =================================================================================
		//
		if (bGroupCull) if (!D9IsBSVisible(&Grp[g].BBox, &mWorldView, &Field)) continue;


		// Setup Textures and Normal Maps ==========================================================================
		//
		if (bTextured) {

			if (Tex[ti] != old_tex) {

				old_tex = Tex[ti];
				FX->SetTexture(eTex0, Tex[ti]->GetTexture());

				LPDIRECT3DTEXTURE9 pEmis = Tex[ti]->GetMap(MAP_EMISSION);

				if (tni && Grp[g].TexMixEx[0]<0.5f) tni = 0;
				if (!pEmis && tni && Tex[tni]) pEmis = Tex[tni]->GetTexture();

				if (pEmis != pEmis_old) {
					FX->SetTexture(eEmisMap, pEmis);
					pEmis_old = pEmis;
					FC.Emis = (pEmis != NULL);
					bUpdateFlow = true;
				}
			}
		}

		// Apply MFD Screen Override ================================================================================
		//
		if (Grp[g].MFDScreenId) {
			bTextured = true;
			old_tex = NULL;
			old_mat = NULL;
			reset(FC);
			bUpdateFlow = true;

			SURFHANDLE hMFD;
			if (bHUD) hMFD = gc->GetVCHUDSurface(&hudspec);
			else hMFD = gc->GetMFDSurface(Grp[g].MFDScreenId - 1);

			if (hMFD) FX->SetTexture(eTex0, SURFACE(hMFD)->GetTexture());
			else	  FX->SetTexture(eTex0, gc->GetDefaultTexture()->GetTexture());

			if (Grp[g].MtrlIdx == SPEC_DEFAULT) mat = &mfdmat;
			else							    mat = &Mtrl[Grp[g].MtrlIdx];

			if (bModulateMatAlpha || bTextured == false)  FX->SetFloat(eMtrlAlpha, mat->Diffuse.w);
			else										  FX->SetFloat(eMtrlAlpha, 1.0f);

			FX->SetValue(eMtrl, mat, sizeof(D3D9MatExt)-4);
			FX->SetBool(eEnvMapEnable, false);
			FX->SetBool(eFresnel, false);
		}

		// Setup Mesh group material  ==========================================================================
		//
		else {

			if (Grp[g].MtrlIdx == SPEC_DEFAULT) mat = &defmat;
			else							    mat = &Mtrl[Grp[g].MtrlIdx];

			if (mat != old_mat) {

				D3D9Stats.Mesh.MtrlChanges++;

				old_mat = mat;

				FX->SetValue(eMtrl, mat, sizeof(D3D9MatExt)-4);

				if (bModulateMatAlpha || bTextured == false)  FX->SetFloat(eMtrlAlpha, mat->Diffuse.w);
				else										  FX->SetFloat(eMtrlAlpha, 1.0f);
			}
		}


		// Apply Animations =========================================================================================
		//
		if (Grp[g].bTransform) {
			bWorldMesh = false;
			FX->SetMatrix(eW, D3DXMatrixMultiply(&q, &pGrpTF[g], pW));
		}
		else if (!bWorldMesh) {
			FX->SetMatrix(eW, &mWorldMesh);
			bWorldMesh = true;
		}

		if (bUpdateFlow) {
			bUpdateFlow = false;
			HR(FX->SetValue(eFlow, &FC, sizeof(TexFlow)));
		}

		// Setup Mesh drawing options =================================================================================
		//
		FX->SetBool(eTextured, bTextured);
		FX->SetBool(eFullyLit, (Grp[g].UsrFlag & 0x4) != 0);
		FX->SetBool(eOITEnable, (Grp[g].UsrFlag & 0x20) != 0);

		if ((Grp[g].UsrFlag & 0x10) != 0) FX->SetValue(eNoColor, &D3DXVECTOR3(1, 1, 1), sizeof(D3DXVECTOR3));
		else FX->SetValue(eNoColor, &D3DXVECTOR3(0, 0, 0), sizeof(D3DXVECTOR3));

		FX->CommitChanges();

		if (bHUD) {
			pDev->SetRenderState(D3DRS_ZENABLE, 0);
			pDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		}

		if (Grp[g].bDualSided) {
			pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
			pDev->SetRenderState(D3DRS_ZWRITEENABLE, 0);
			pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, Grp[g].VertOff, 0, Grp[g].nVert, Grp[g].IdexOff, Grp[g].nFace);
			pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
		}

		pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, Grp[g].VertOff, 0, Grp[g].nVert, Grp[g].IdexOff, Grp[g].nFace);

		Grp[g].bRendered = true;

		if (Grp[g].bDualSided) {
			pDev->SetRenderState(D3DRS_ZWRITEENABLE, 1);
		}

		if (bHUD) {
			pDev->SetRenderState(D3DRS_ZENABLE, 1);
			pDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		}

		D3D9Stats.Mesh.Vertices += Grp[g].nVert;
		D3D9Stats.Mesh.MeshGrps++;
	}

	HR(FX->EndPass());
	HR(FX->End());

	if (flags&(DBG_FLAGS_BOXES | DBG_FLAGS_SPHERES)) RenderBoundingBox(pW);
	FX->SetVector(eColor, ptr(D3DXVECTOR4(0, 0, 0, 0)));
	if (flags&DBG_FLAGS_DUALSIDED) pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
}


// ===========================================================================================
//
D3DXMATRIX D3D9Mesh::GetTransform(int g, bool bCombined)
{
	if (g < 0) return mTransform;

	if (Grp[g].bTransform) {
		if (bCombined) return pGrpTF[g];
		else return Grp[g].Transform;
	}

	D3DXMATRIX Ident; D3DXMatrixIdentity(&Ident);
	return Ident;
}


// ===========================================================================================
//
bool D3D9Mesh::SetTransform(int g, const LPD3DXMATRIX pMat)
{
	if (g >= int(nGrp)) return false;

	// Set Mesh Transform if g < 0
	if (g < 0) {
		mTransform = *pMat;
		bGlobalTF = true;
		bBSRecompute = true;
		bBSRecomputeAll = true;
		D3DXMatrixInverse(&mTransformInv, NULL, &mTransform);
		for (DWORD i = 0; i<nGrp; i++) {
			if (Grp[i].bTransform) D3DXMatrixMultiply(&pGrpTF[i], &mTransform, &Grp[i].Transform);
			else pGrpTF[i] = mTransform;
		}
		return true;
	}

	Grp[g].bUpdate = true;
	Grp[g].bTransform = true;
	Grp[g].Transform = *pMat;
	D3DXMatrixMultiply(&pGrpTF[g], &mTransform, &Grp[g].Transform);

	return true;
}




// ===========================================================================================
//
void D3D9Mesh::RenderBaseTile(const LPD3DXMATRIX pW)
{
	if (!IsOK()) return;

	Scene *scn = gc->GetScene();

	bool bTextured = true;
	bool bGroupCull = true;
	bool bUseNormalMap = (Config->UseNormalMap==1);

	D3DXMATRIX mWorldView,  q;
	D3DXMatrixMultiply(&mWorldView, pW, scn->GetViewMatrix());

	D3DXVECTOR4 Field = D9LinearFieldOfView(scn->GetProjectionMatrix());

	D3D9MatExt *mat, *old_mat = NULL;
	SURFHANDLE old_tex = NULL;
	LPDIRECT3DTEXTURE9  pNorm = NULL;

	pDev->SetVertexDeclaration(pMeshVertexDecl);
	pDev->SetStreamSource(0, pBuf->pVB, 0, sizeof(NMVERTEX));
	pDev->SetIndices(pBuf->pIB);


	FX->SetTechnique(eBaseTile);
	FX->SetVector(eColor, ptr(D3DXVECTOR4(0, 0, 0, 0)));
	FX->SetMatrix(eGT, gc->GetIdentity());
	FX->SetMatrix(eW, pW);

	ConfigureAtmo();

	UINT numPasses = 0;
	HR(FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE));

	for (DWORD pass=0;pass<numPasses;pass++) {

		if (bUseNormalMap==false && pass==0) continue; // Skip normal mapped rendering pass

		HR(FX->BeginPass(pass));

		for (DWORD g=0; g<nGrp; g++) {

			if (Grp[g].UsrFlag & 0x2) continue;

			// Render group ----------------------------------------------
			//

			DWORD ti=Grp[g].TexIdx;
			DWORD tni=Grp[g].TexIdxEx[0];

			if (ti==0 && tni!=0) continue;

			if (Tex[ti]==NULL || ti==0) bTextured = false;
			else						bTextured = true;

			if (bTextured) {
				pNorm = Tex[ti]->GetMap(MAP_NORMAL);
				if (pNorm==NULL && pass==0) continue;
				if (pNorm!=NULL && pass==1) continue;
			}
			else {
				if (pass==0) continue;
				pNorm=NULL;
				old_tex=NULL;
			}

			// Cull unvisible geometry ------------------------------------------------------
			//
			if (bGroupCull) if (!D9IsBSVisible(&Grp[g].BBox, &mWorldView, &Field)) continue;


			// Setup Textures and Normal Maps ==========================================================================
			//
			if (bTextured) {

				if (Tex[ti]!=old_tex) {

					if (tni && Grp[g].TexMixEx[0]<0.5f) tni=0;

					old_tex = Tex[ti];
					FX->SetTexture(eTex0, Tex[ti]->GetTexture());

					if (tni && Tex[tni]) {
						FX->SetTexture(eEmisMap, Tex[tni]->GetTexture());
						//FX->SetBool(eUseEmis, true);
					} //else FX->SetBool(eUseEmis, false);

					if (bUseNormalMap) if (pNorm) FX->SetTexture(eTex3, pNorm);
				}
			}

			// Setup Mesh group material ==============================================================================
			//
			if (Grp[g].MtrlIdx==SPEC_DEFAULT) mat = &defmat;
			else							  mat = &Mtrl[Grp[g].MtrlIdx];

			if (mat!=old_mat) {
				old_mat = mat;
				FX->SetValue(eMtrl, mat, sizeof(D3D9MatExt)-4);
				if (bModulateMatAlpha || bTextured==false) FX->SetFloat(eMtrlAlpha, mat->Diffuse.w);
				else FX->SetFloat(eMtrlAlpha, 1.0f);
			}

			// Setup Mesh drawing options =================================================================================
			//
			FX->SetBool(eTextured, bTextured);
			FX->SetBool(eFullyLit, (Grp[g].UsrFlag&0x4)!=0);

			FX->CommitChanges();

			pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, Grp[g].VertOff, 0, Grp[g].nVert, Grp[g].IdexOff, Grp[g].nFace);

			D3D9Stats.Mesh.Vertices += Grp[g].nVert;
			D3D9Stats.Mesh.MeshGrps++;
		}
		HR(FX->EndPass());
	}
	HR(FX->End());
}


// ================================================================================================
//
void D3D9Mesh::RenderShadowMap(const LPD3DXMATRIX pW, const LPD3DXMATRIX pVP, int opt, bool bNoCull)
{
	if (!IsOK()) return;

	pBuf->Map(pDev);

	D3DXMATRIX GroupMatrix, mWorldMesh;

	MeshShader* pShader = nullptr;
	
	MeshShader::vs_const.mVP = pVP ? *pVP : FMATRIX4();

	D3DXMatrixIdentity(MeshShader::vs_const.mW);
	
	if (bGlobalTF) D3DXMatrixMultiply(&mWorldMesh, &mTransform, pW);
	else mWorldMesh = *pW;

	if (opt == 1) {
		// Screenspace Depth and Normal buffer rendering
		pDev->SetStreamSource(0, pBuf->pVB, 0, sizeof(NMVERTEX));
		pShader = s_pShader[SHADER_NORMAL_DEPTH];
		pShader->Setup(pMeshVertexDecl, true, 0);
	}
	else {
		if (Flags & 0x20) {
			// Regular shadowmap with OIT support
			pDev->SetStreamSource(0, pBuf->pSB, 0, sizeof(SMVERTEX));
			pShader = s_pShader[SHADER_SHADOWMAP_OIT];
			pShader->Setup(pPosTexDecl, true, 0);
		}
		else {
			// Regular shadowmap for self shadowing
			pDev->SetStreamSource(0, pBuf->pGB, 0, sizeof(D3DXVECTOR4));
			pShader = s_pShader[SHADER_SHADOWMAP];
			pShader->Setup(pVector4Decl, true, 0);
		}
	}
	pDev->SetIndices(pBuf->pIB);
	pShader->ClearTextures();

	bool bInit = true;
	bool bCurrentState = false;

	for (DWORD g = 0; g < nGrp; g++)
	{
		if (Grp[g].UsrFlag & 0x2) continue;
		if (Grp[g].UsrFlag & 0x1) continue;
		
		MeshShader::ps_bools.bOIT = (Grp[g].UsrFlag & 0x20) != 0;

		if (MeshShader::ps_bools.bOIT) {
			DWORD ti = Grp[g].TexIdx;
			if (ti) {
				auto hTex = Tex[ti]->GetTexture();
				if (hTex) pShader->SetTexture(pShader->hPST[0], hTex, IPF_WRAP | IPF_POINT);
				else MeshShader::ps_bools.bOIT = false;
			}
			else MeshShader::ps_bools.bOIT = false;
		}

		if (Grp[g].bTransform) {
			D3DXMatrixMultiply(MeshShader::vs_const.mW, &pGrpTF[g], pW);		// Apply Animations to instance matrices
			bInit = true;
		}
		else {
			if (bInit) MeshShader::vs_const.mW = mWorldMesh;
			bInit = false;
		}

		if (pShader->hVSC) pShader->SetVSConstants(pShader->hVSC, &MeshShader::vs_const, sizeof(MeshShader::vs_const));
		if (pShader->hPSC) pShader->SetPSConstants(pShader->hPSC, &MeshShader::ps_const, sizeof(MeshShader::ps_const));
		if (pShader->hPSB) pShader->SetPSConstants(pShader->hPSB, &MeshShader::ps_bools, sizeof(MeshShader::ps_bools));
		pShader->UpdateTextures();

		DWORD oc;
		if (bNoCull) {
			pDev->GetRenderState(D3DRS_CULLMODE, &oc);
			pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		}
		pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, Grp[g].VertOff, 0, Grp[g].nVert, Grp[g].IdexOff, Grp[g].nFace);
		if (bNoCull) pDev->SetRenderState(D3DRS_CULLMODE, oc);
	}
}


// ================================================================================================
//
void D3D9Mesh::RenderStencilShadows(float alpha, const LPD3DXMATRIX pP, const LPD3DXMATRIX pW, bool bShadowMap, const D3DXVECTOR4 *elev)
{
	if (!IsOK()) return;

	DWORD Pass = 0;
	D3DXMATRIX GroupMatrix, mWorldMesh; UINT numPasses = 0;

	if (bGlobalTF) D3DXMatrixMultiply(&mWorldMesh, &mTransform, pW);
	else mWorldMesh = *pW;

	pDev->SetIndices(pBuf->pIB);
	pDev->SetVertexDeclaration(pPosTexDecl);
	pDev->SetStreamSource(0, pBuf->pSB, 0, sizeof(SMVERTEX));
	FX->SetTechnique(eShadowTech);
	
	if (elev) FX->SetVector(eInScatter, elev);
	else FX->SetVector(eInScatter, ptr(D3DXVECTOR4(0,1,0,0)));

	FX->SetFloat(eMix, alpha);
	FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE);

	FX->BeginPass(Pass);
	
	if (pP) FX->SetValue(eGT, pP, sizeof(D3DXMATRIX));	// Shadow Projection

	bool bInit = true;
	bool bCurrentState = false;

	for (DWORD g = 0; g < nGrp; g++) {

		if (Grp[g].UsrFlag & 0x2) continue;
		if ((Grp[g].UsrFlag & 0x1) && (bShadowMap == false)) continue;

		bool bOIT = (Grp[g].UsrFlag & 0x20) != 0;
		
		if (bOIT) {
			DWORD ti = Grp[g].TexIdx;
			if (ti) {
				auto hTex = Tex[ti]->GetTexture();
				if (hTex) {
					HR(FX->SetTexture(eTex0, hTex));
				} else bOIT = false;
			} else bOIT = false;
		}

		FX->SetBool(eOITEnable, bOIT);

		if (Grp[g].bTransform) {
			D3DXMatrixMultiply(&GroupMatrix, &pGrpTF[g], pW);		// Apply Animations to instance matrices
			FX->SetValue(eW, &GroupMatrix, sizeof(D3DXMATRIX));
			bInit = true;
		}
		else {
			if (bInit) {
				FX->SetValue(eW, &mWorldMesh, sizeof(D3DXMATRIX));
			}
			bInit = false;
		}

		FX->CommitChanges();
		pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, Grp[g].VertOff, 0, Grp[g].nVert, Grp[g].IdexOff, Grp[g].nFace);
	}

	FX->EndPass();
	FX->End();
}


// ================================================================================================
//
void D3D9Mesh::RenderShadowsEx(float alpha, const LPD3DXMATRIX pP, const LPD3DXMATRIX pW, const D3DXVECTOR4 *light, const D3DXVECTOR4 *param)
{
	if (!IsOK()) return;

	D3D9Stats.Mesh.Meshes++;

	pDev->SetVertexDeclaration(pPosTexDecl);
	pDev->SetStreamSource(0, pBuf->pSB, 0, sizeof(SMVERTEX));
	pDev->SetIndices(pBuf->pIB);

	FX->SetTechnique(eShadowTech);
	FX->SetMatrix(eW, pW);
	FX->SetMatrix(eGT, pP);
	FX->SetFloat(eMix, alpha);
	if (light) FX->SetVector(eColor, light);
	else FX->SetVector(eColor, ptr(D3DXVECTOR4(0,1,0,0)));
	FX->SetVector(eTexOff, param);


	UINT numPasses = 0;
	FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE);
	FX->BeginPass(1);

	for (DWORD g = 0; g<nGrp; g++) {

		if (Grp[g].UsrFlag & 0x3) continue;

		bool bOIT = (Grp[g].UsrFlag & 0x20) != 0;

		if (bOIT) {
			DWORD ti = Grp[g].TexIdx;
			if (ti) {
				auto hTex = (Tex[ti] ? Tex[ti]->GetTexture() : NULL);
				if (hTex) {
					HR(FX->SetTexture(eTex0, hTex));
				}
				else bOIT = false;
			}
			else bOIT = false;
		}

		FX->SetBool(eOITEnable, bOIT);
		FX->CommitChanges();

		pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, Grp[g].VertOff, 0, Grp[g].nVert, Grp[g].IdexOff, Grp[g].nFace);

		D3D9Stats.Mesh.Vertices += Grp[g].nVert;
		D3D9Stats.Mesh.MeshGrps++;
	}

	FX->EndPass();
	FX->End();
}



// ================================================================================================
// This is a rendering routine for a Exterior Mesh, non-spherical moons/asteroids
//
void D3D9Mesh::RenderBoundingBox(const LPD3DXMATRIX pW)
{
	_TRACE;

	if (!IsOK()) return;
	if (DebugControls::IsActive()==false) return;

	D3DXMATRIX q, qq;

	static D3DVECTOR poly[10] = {
		{0, 0, 0},
		{1, 0, 0},
		{1, 1, 0},
		{0, 1, 0},
		{0, 0, 0},
		{0, 0, 1},
		{1, 0, 1},
		{1, 1, 1},
		{0, 1, 1},
		{0, 0, 1}
	};

	static D3DVECTOR list[6] = {
		{1, 0, 0},
		{1, 0, 1},
		{1, 1, 0},
		{1, 1, 1},
		{0, 1, 0},
		{0, 1, 1}
	};



	DWORD flags  = *(DWORD*)gc->GetConfigParam(CFGPRM_GETDEBUGFLAGS);
	DWORD selmsh = *(DWORD*)gc->GetConfigParam(CFGPRM_GETSELECTEDMESH);
	DWORD selgrp = *(DWORD*)gc->GetConfigParam(CFGPRM_GETSELECTEDGROUP);
	bool  bSel   =  (g_uCurrentMesh==selmsh);


	if (flags&(DBG_FLAGS_SELVISONLY|DBG_FLAGS_SELMSHONLY|DBG_FLAGS_SELGRPONLY) && DebugControls::GetVisual()!=g_pCurrentVisual) return;
	if (flags&DBG_FLAGS_SELMSHONLY && !bSel) return;
	if (flags&DBG_FLAGS_SELGRPONLY && !bSel) return;

	if (flags&DBG_FLAGS_BOXES) {

		pDev->SetVertexDeclaration(pPositionDecl);

		// ----------------------------------------------------------------
		FX->SetMatrix(eW, pW);
		FX->SetVector(eColor, ptr(D3DXVECTOR4(0, 1, 0, 0.5f)));
		FX->SetTechnique(eBBTech);
		// ----------------------------------------------------------------

		UINT numPasses = 0;
		FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE);
		FX->BeginPass(0);

		for (DWORD g=0; g<nGrp; g++) {

			if (flags&DBG_FLAGS_SELGRPONLY && g!=selgrp) continue;
			if (Grp[g].UsrFlag & 0x2) continue;

			FX->SetVector(eAttennuate, &Grp[g].BBox.min);
			FX->SetVector(eInScatter, &Grp[g].BBox.max);

			// Apply Animations =========================================================================================
			//
			if (Grp[g].bTransform) {
				if (bGlobalTF)  FX->SetMatrix(eGT, D3DXMatrixMultiply(&q, &mTransform, &Grp[g].Transform));
				else FX->SetMatrix(eGT, &Grp[g].Transform);
			}
			else FX->SetMatrix(eGT, &mTransform);


			// Setup Mesh drawing options =================================================================================
			//
			FX->CommitChanges();

			pDev->DrawPrimitiveUP(D3DPT_LINESTRIP, 9, &poly, sizeof(D3DVECTOR));
			pDev->DrawPrimitiveUP(D3DPT_LINELIST, 3, &list, sizeof(D3DVECTOR));
		}

		FX->EndPass();
		FX->End();
	}

	if (flags&DBG_FLAGS_SPHERES) {
		for (DWORD g=0; g<nGrp; g++) {
			if (flags&DBG_FLAGS_SELGRPONLY && g!=selgrp) continue;
			if (Grp[g].UsrFlag & 0x2) continue;
			D3D9Effect::RenderBoundingSphere(pW, NULL, &Grp[g].BBox.bs, ptr(D3DXVECTOR4(0,1,0,0.75f)));
		}
	}
	if (flags&DBG_FLAGS_BOXES) D3D9Effect::RenderBoundingBox(pW, &mTransform, &BBox.min, &BBox.max, ptr(D3DXVECTOR4(0,0,1,0.75f)));
	if (flags&DBG_FLAGS_SPHERES) D3D9Effect::RenderBoundingSphere(pW, &mTransform, &BBox.bs, ptr(D3DXVECTOR4(0,0,1,0.75f)));
}


// ===========================================================================================
//

void D3D9Mesh::BoundingBox(const NMVERTEX *vtx, DWORD n, D9BBox *box)
{
	XMVECTOR mi, mx;
	mi = mx = XMLoadFloat3((XMFLOAT3 *)&vtx[0].x);
	for (DWORD i = 1; i < n; i++) {
		XMVECTOR x = XMLoadFloat3((XMFLOAT3 *)&vtx[i].x);
		mi = XMVectorMin(mi, x);
		mx = XMVectorMax(mx, x);
	}
	XMStoreFloat4((XMFLOAT4 *)&box->min.x, XMVectorSetW(mi, 0));
	XMStoreFloat4((XMFLOAT4 *)&box->max.x, XMVectorSetW(mx, 0));
}

// ===========================================================================================
//
void D3D9Mesh::TransformGroup(DWORD n, const D3DXMATRIX *m)
{
	if (!IsOK()) return;

	bBSRecompute = true;

	Grp[n].Transform = Grp[n].Transform * (*m);
	Grp[n].bTransform = true;
	Grp[n].bUpdate = true;

	D3DXMatrixMultiply(&pGrpTF[n], &mTransform, &Grp[n].Transform);
}

// ===========================================================================================
//
void D3D9Mesh::Transform(const D3DXMATRIX *m)
{
	if (!IsOK()) return;

	bBSRecompute = true;
	bBSRecomputeAll = true;
	bGlobalTF = true;
	mTransform = mTransform * (*m);

	D3DXMatrixInverse(&mTransformInv, NULL, &mTransform);

	for (DWORD i=0;i<nGrp;i++) {
		if (Grp[i].bTransform) D3DXMatrixMultiply(&pGrpTF[i], &mTransform, &Grp[i].Transform);
		else pGrpTF[i] = mTransform;
	}
}

// ===========================================================================================
//
void D3D9Mesh::SetPosition(VECTOR3 &pos)
{
	bGlobalTF = true;

	mTransform._41 = float(pos.x);
	mTransform._42 = float(pos.y);
	mTransform._43 = float(pos.z);

	D3DXMatrixInverse(&mTransformInv, NULL, &mTransform);

	for (DWORD i = 0; i<nGrp; i++) {
		if (Grp[i].bTransform) D3DXMatrixMultiply(&pGrpTF[i], &mTransform, &Grp[i].Transform);
		else pGrpTF[i] = mTransform;
	}
}

// ===========================================================================================
//
void D3D9Mesh::SetRotation(D3DXMATRIX &rot)
{
	bGlobalTF = true;

	// TODO: BUG: Position will be acquired from rot matrix
	//
	memcpy(&mTransform, &rot, 48);

	D3DXMatrixInverse(&mTransformInv, NULL, &mTransform);

	for (DWORD i = 0; i<nGrp; i++) {
		if (Grp[i].bTransform) D3DXMatrixMultiply(&pGrpTF[i], &mTransform, &Grp[i].Transform);
		else pGrpTF[i] = mTransform;
	}
}

// ===========================================================================================
//
void D3D9Mesh::UpdateBoundingBox()
{
	if (!IsOK()) return;
	if (bBSRecompute==false) return;

	for (DWORD i=0;i<nGrp;i++) {
		if (Grp[i].bUpdate || bBSRecomputeAll) {
			Grp[i].bUpdate = false;
			if (bGlobalTF) {
				if (Grp[i].bTransform)	D9UpdateAABB(&Grp[i].BBox, &mTransform, &Grp[i].Transform);
				else					D9UpdateAABB(&Grp[i].BBox, &mTransform);
			}
			else {
				if (Grp[i].bTransform)	D9UpdateAABB(&Grp[i].BBox, &Grp[i].Transform);
				else					D9UpdateAABB(&Grp[i].BBox);
			}
		}
	}

	bBSRecomputeAll = false;
	bBSRecompute = false;

	if (nGrp==0) {
		BBox.min = D3DXVECTOR4(0,0,0,0);
		BBox.max = D3DXVECTOR4(0,0,0,0);
	}
	else {
		for (DWORD i=0;i<nGrp;i++) {
			if (Grp[i].bTransform) {
				if (bGlobalTF) {
					D3DXMATRIX q;
					D3DXMatrixMultiply(&q, D3DXMatrixMultiply(&q, &mTransform, &Grp[i].Transform), &mTransformInv);
					D9AddAABB(&Grp[i].BBox, &q, &BBox, i==0);
				}
				else D9AddAABB(&Grp[i].BBox, &Grp[i].Transform, &BBox, i==0);
			}
			else {
				D9AddAABB(&Grp[i].BBox, NULL, &BBox, i==0);
			}
		}
	}

	D9UpdateAABB(&BBox, &mTransform);
}


// ===========================================================================================
//
D9BBox * D3D9Mesh::GetAABB()
{
	if (!IsOK()) return nullptr;
	UpdateBoundingBox();
	return &BBox;
}

// ===========================================================================================
//
D3DXVECTOR3 D3D9Mesh::GetBoundingSpherePos()
{
	if (!IsOK()) return D3DXVECTOR3(0,0,0);
	UpdateBoundingBox();
	return D3DXVECTOR3f4(BBox.bs);
}

// ===========================================================================================
//
float D3D9Mesh::GetBoundingSphereRadius()
{
	if (!IsOK()) return 0.0f;
	UpdateBoundingBox();
	return BBox.bs.w;
}

// ===========================================================================================
//
D3D9Pick D3D9Mesh::Pick(const LPD3DXMATRIX pW, const LPD3DXMATRIX pT, const D3DXVECTOR3 *vDir, const PickProp *p)
{
	D3D9Pick result;
	result.dist  = 1e30f;
	result.pMesh = NULL;
	result.vObj  = NULL;
	result.group = -1;
	result.idx = -1;

	if (p->pMesh) if (p->pMesh != this) return result;

	if (!pBuf->pGBSys || !pBuf->pIBSys) {
		LogErr("D3D9Mesh::Pick() Failed: No Geometry Available");
		return result;
	}

	UpdateBoundingBox();

	D3DXMATRIX mW, mWT, mWorldMesh;

	if (pT) D3DXMatrixMultiply(&mWT, pT, pW);
	else mWT = *pW;

	if (bGlobalTF) D3DXMatrixMultiply(&mWorldMesh, &mTransform, &mWT);
	else mWorldMesh = mWT;

	for (DWORD g=0;g<nGrp;g++) {

		if ((Grp[g].UsrFlag & 0x2) && (Grp[g].MFDScreenId != 0x100)) continue;

		D3DXVECTOR3 bs = D3DXVECTOR3f4(Grp[g].BBox.bs);
		float rad = Grp[g].BBox.bs.w * D3DMAT_BSScaleFactor(&mWT);

		D3DXVec3TransformCoord(&bs, &bs, &mWT);

		float dst = D3DXVec3Dot(&bs, vDir);
		float len2 = D3DXVec3Dot(&bs, &bs);

		if (dst < -rad) continue;
		if (sqrt(len2 - dst*dst) > rad) continue;

		if (Grp[g].bTransform) D3DXMatrixMultiply(&mW, &pGrpTF[g], &mWT);
		else mW = mWorldMesh;

		D3DXVECTOR3 _a, _b, _c, cp;

		WORD *pIdc = pBuf->pIBSys + Grp[g].IdexOff;
		D3DXVECTOR4 *pVrt = pBuf->pGBSys + Grp[g].VertOff;

		D3DXMATRIX mWI; float det;
		D3DXMatrixInverse(&mWI, &det, &mW);

		D3DXVECTOR3 pos, dir;

		D3DXVec3TransformCoord(&pos, ptr(D3DXVECTOR3(0, 0, 0)), &mWI);
		D3DXVec3TransformNormal(&dir, vDir, &mWI);

		for (DWORD i=0;i<Grp[g].nFace;i++) {

			WORD a = pIdc[i*3+0];
			WORD b = pIdc[i*3+1];
			WORD c = pIdc[i*3+2];

			_a = D3DXVECTOR3f4(pVrt[a]);
			_b = D3DXVECTOR3f4(pVrt[b]);
			_c = D3DXVECTOR3f4(pVrt[c]);

			float u, v, dst;

			D3DXVec3Cross(&cp, ptr(_c - _b), ptr(_a - _b));

			if ((D3DXVec3Dot(&cp, &dir)<0) || p->bDualSided) {
				if (D3DXIntersectTri(&_c, &_b, &_a, &pos, &dir, &u, &v, &dst)) {
					if (dst > p->fnear) {
						if (dst < result.dist) {
							result.dist = dst;
							result.group = int(g);
							result.pMesh = this;
							result.idx = int(i);
							result.u = u;
							result.v = v;
						}
					}
				}
			}
		}
	}


	if (result.idx >= 0 && result.group >= 0) {

		int i = result.idx;
		int g = result.group;

		if (Grp[g].bTransform) mW = pGrpTF[g];
		else {
			if (bGlobalTF) mW = mTransform;
			else D3DXMatrixIdentity(&mW);
		}

		if (pT) D3DXMatrixMultiply(&mW, &mW, pT);

		D3DXVECTOR3 cp;

		WORD *pIdc = &pBuf->pIBSys[Grp[g].IdexOff];
		D3DXVECTOR4 *pVrt = &pBuf->pGBSys[Grp[g].VertOff];

		WORD a = pIdc[i * 3 + 0];
		WORD b = pIdc[i * 3 + 1];
		WORD c = pIdc[i * 3 + 2];

		D3DXVECTOR3 _a = D3DXVECTOR3f4(pVrt[a]);
		D3DXVECTOR3 _b = D3DXVECTOR3f4(pVrt[b]);
		D3DXVECTOR3 _c = D3DXVECTOR3f4(pVrt[c]);

		float u = result.u;
		float v = result.v;

		D3DXVec3Cross(&cp, ptr(_c - _b), ptr(_a - _b));

		D3DXVec3TransformNormal(&cp, &cp, &mW);
		D3DXVec3Normalize(&result.normal, &cp);

		D3DXVECTOR3 p = (_b * u) + (_a * v) + (_c * (1.0f - u - v));
		D3DXVec3TransformCoord(&result.pos, &p, &mW);
	}

	return result;
}



// ===========================================================================================
// SPECIAL RENDER FUNCTIONS SECTION
// ===========================================================================================
//

// This is a special rendering routine used to render 3D arrow --------------------------------
//
void D3D9Mesh::RenderAxisVector(LPD3DXMATRIX pW, const D3DXCOLOR *pColor, float len)
{
	UINT numPasses = 0;
	HR(FX->SetTechnique(eAxisTech));
	HR(FX->SetFloat(eMix, len));
	HR(FX->SetValue(eColor, pColor, sizeof(D3DXCOLOR)));
	HR(FX->SetMatrix(eW, pW));
	HR(FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE));
	HR(FX->BeginPass(0));
	pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
	RenderGroup(0);
	pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
	HR(FX->EndPass());
	HR(FX->End());
}

// Used only by ring manager --------------------------------------------------------------------
//
void D3D9Mesh::RenderRings(const LPD3DXMATRIX pW, LPDIRECT3DTEXTURE9 pTex)
{
	_TRACE;
	if (!IsOK()) return;
	if (!pTex) return;

	D3D9Stats.Mesh.Vertices += Grp[0].nVert;
	D3D9Stats.Mesh.MeshGrps++;

	UINT numPasses = 0;
	HR(FX->SetTechnique(eRingTech));
	HR(FX->SetMatrix(eW, pW));
	HR(FX->SetTexture(eTex0, pTex));
	FX->SetValue(eSun, &sunLight, sizeof(D3D9Sun));
	HR(FX->SetValue(eMtrl, &defmat, sizeof(D3D9MatExt)-4));
	HR(FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE));
	HR(FX->BeginPass(0));
	RenderGroup(0);
	HR(FX->EndPass());
	HR(FX->End());
}

// Used only by ring manager --------------------------------------------------------------------
//
void D3D9Mesh::RenderRings2(const LPD3DXMATRIX pW, LPDIRECT3DTEXTURE9 pTex, float irad, float orad)
{
	_TRACE;
	if (!IsOK()) return;
	if (!pTex) return;

	D3D9Stats.Mesh.Vertices += Grp[0].nVert;
	D3D9Stats.Mesh.MeshGrps++;

	UINT numPasses = 0;
	HR(FX->SetTechnique(eRingTech2));
	HR(FX->SetMatrix(eW, pW));
	HR(FX->SetTexture(eTex0, pTex));
	FX->SetValue(eSun, &sunLight, sizeof(D3D9Sun));
	HR(FX->SetValue(eMtrl, &defmat, sizeof(D3D9MatExt)-4));
	HR(FX->SetVector(eTexOff, ptr(D3DXVECTOR4(irad, orad, 0, 0))));
	HR(FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE));
	HR(FX->BeginPass(0));
	RenderGroup(0);
	HR(FX->EndPass());
	HR(FX->End());
}


// ===========================================================================================
//
void D3D9Mesh::GlobalInit(LPDIRECT3DDEVICE9 pDev)
{
	memset(s_pShader, 0, sizeof(s_pShader));

	s_pShader[SHADER_SHADOWMAP] = new MeshShader(pDev, "Modules/D3D9Client/NewMesh.hlsl", "ShdMapVS", "ShdMapPS");
	s_pShader[SHADER_SHADOWMAP_OIT] = new MeshShader(pDev, "Modules/D3D9Client/NewMesh.hlsl", "ShdMapOIT_VS", "ShdMapOIT_PS");
	s_pShader[SHADER_NORMAL_DEPTH] = new MeshShader(pDev, "Modules/D3D9Client/NewMesh.hlsl", "NormalDepth_VS", "NormalDepth_PS");
}


// ===========================================================================================
//
void D3D9Mesh::GlobalExit()
{
	for (auto x : s_pShader) if (x) delete x;
}

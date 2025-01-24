
// ===================================================
// Copyright (C) 2021 Jarmo Nikkanen
// licensed under LGPL v2
// ===================================================

#include <string.h>
#include "IProcess.h"
#include "D3D9Util.h"
#include "D3D9Surface.h"
#include <sstream>


// ================================================================================================
//
ImageProcessing::ImageProcessing(LPDIRECT3DDEVICE9 pDev, const char *_file, const char *_psentry, const char *_ppf, const char *_vsentry)
	: pDevice(pDev)
	, pVSConst(NULL)
	, pPSConst(NULL)
	, pDepth(NULL)
	, pDepthBak(NULL)
	, pMesh(NULL)
	, hPos(NULL)
	, hSiz(NULL)
	, hVP(NULL)
	, mesh_cull(gcIPInterface::ipicull::None)
	, desc()
	, iVP()
	, mesh_tex_idx(-1)
{
	for (int i=0;i<ARRAYSIZE(pTextures);i++) pTextures[i].hTex = NULL;
	for (int i=0;i<4;i++) pRtg[i] = pRtgBak[i] = NULL;

	if (_vsentry) pVertex = CompileVertexShader(pDevice, _file, _vsentry, "IPIVS", NULL, &pVSConst);
	else pVertex = CompileVertexShader(pDevice, "Modules/D3D9Client/IPI.hlsl", "VSMain", "IPIVS", NULL, &pVSConst);

	pPixel   = CompilePixelShader(pDevice, _file, _psentry, "IPIPS", _ppf, &pPSConst);
	pOcta	 = new SMVERTEX[10];

	Shaders[string(_psentry)].pPixel = pPixel;
	Shaders[string(_psentry)].pPSConst = pPSConst;

	if (pVSConst) {
		hVP = pVSConst->GetConstantByName(NULL, "mVP");
		hPos = pVSConst->GetConstantByName(NULL, "vPos");
		hSiz = pVSConst->GetConstantByName(NULL, "vTgtSize");
		SetTemplate();
	}

	if (!hVP || !hPos) LogErr("Failed to get ImageProcessing::hVP handle");

	double w = 22.5 * RAD;
	double q = w;
	double r = 1.0 / cos(w);
	
	pOcta[0].x = 0.0f;
	pOcta[0].y = 0.0f;
	pOcta[0].z = 0.0f;
	pOcta[0].tu = 0.0f;
	pOcta[0].tv = 0.0f;
	
	for (int i = 1; i < 10; i++) {
		pOcta[i].x = float(cos(q) * r);
		pOcta[i].y = float(sin(q) * r);
		pOcta[i].z = 0.0f;
		pOcta[i].tu = pOcta[i].x;
		pOcta[i].tv = pOcta[i].y;
		q += w*2.0;
	}

	strcpy_s(file, 256, _file);
	strcpy_s(entry, 32, _psentry);
	if (_ppf) strcpy_s(ppf, 256, _ppf);
	else strcpy_s(ppf, 32, "");

	// Create a database of defines ----------------------------------------------------------------
	std::string line;
	std::ifstream fs(_file);
	while (std::getline(fs, line)) {
		if (!line.length() || line.find("//") == 0) continue;
		if (line.find("#define") == 0) def.push_front(line.substr(line.find("#define") + 8));
	}
	fs.close();
}


// ================================================================================================
//
ImageProcessing::~ImageProcessing()
{
	SAFE_RELEASE(pVSConst);
	SAFE_RELEASE(pVertex);
	SAFE_DELETEA(pOcta);

	for (auto x : Shaders) {
		SAFE_RELEASE(x.second.pPixel);
		SAFE_RELEASE(x.second.pPSConst);
	}
	Shaders.clear();
}


// ================================================================================================
//
bool ImageProcessing::CompileShader(const char *Entry)
{
	string name(Entry);
	LPD3DXCONSTANTTABLE pPSC = NULL;
	Shaders[name].pPixel = CompilePixelShader(pDevice, file, Entry, "IPIPS2", ppf, &pPSC);
	Shaders[name].pPSConst = pPSC;
	return ((Shaders[name].pPixel != NULL) && (Shaders[name].pPSConst != NULL));
}


// ================================================================================================
//
bool ImageProcessing::Activate(const char *Entry)
{
	SetTemplate();
	if (!Entry) return Activate(entry);
	string name(Entry);
	if (Shaders.count(name) == 0) {
		LogErr("ImageProcessing::Activate() FAILED Entry=%s", Entry);
		return false;
	}
	strcpy_s(entry, 31, Entry);
	pPixel = Shaders[name].pPixel;
	pPSConst = Shaders[name].pPSConst;
	return true;
}


// ================================================================================================
//
int ImageProcessing::FindDefine(const char *_key)
{
	int retval;
	std::string key;
	auto it = def.begin();
	while (it!=def.end()) {
		std::istringstream iss(*it);
		iss >> key >> retval;
		if (key.compare(_key) == 0) return retval;
		it++;
	}
	return 0;
}

// ================================================================================================
//
bool ImageProcessing::SetupViewPort()
{

	// Check that the first render target is valid
	//
	if (pRtg[0]) pRtg[0]->GetDesc(&desc);
	else {
		LogErr("ImageProcessing(%s): No render target is set", _PTR(this));
		return false;
	}

	D3DSURFACE_DESC ds;

	// Check that all additional render targets have the same size
	//
	for (int i=1;i<4;i++) {
		if (pRtg[i]) {
			pRtg[i]->GetDesc(&ds);
			if ((ds.Height!=desc.Height) || (ds.Width!=desc.Width)) {
				LogErr("ImageProcessing(%s): All render targets must be same the size", _PTR(this));
				return false;
			}
		}
		else break;
	}

	// Setup view-projection matrix and viewport
	//
	D3DXMatrixOrthoOffCenterLH(&mVP, 0.0f, (float)desc.Width, (float)desc.Height, 0.0f, 0.0f, 1.0f);

	iVP.X = 0;
	iVP.Y = 0;
	iVP.Width  = desc.Width;
	iVP.Height = desc.Height;
	iVP.MinZ = 0.0f;
	iVP.MaxZ = 1.0f;

	HR(pDevice->SetViewport(&iVP));
	HR(pVSConst->SetMatrix(pDevice, hVP, &mVP));
	HR(pVSConst->SetVector(pDevice, hSiz, ptr(D3DXVECTOR4(float(desc.Width), float(desc.Height), 1.0f/float(desc.Width), 1.0f/float(desc.Height)))));
	HR(pVSConst->SetVector(pDevice, hPos, &vTemplate));
	return true;
}


// ================================================================================================
//
void ImageProcessing::SetTemplate(float w, float h, float x, float y)
{
	vTemplate = D3DXVECTOR4(w, h, x, y);
}


// ================================================================================================
//
void ImageProcessing::SetMesh(const MESHHANDLE hMesh, const char *tex, gcIPInterface::ipicull cull)
{
	pMesh = GetSketchMesh(hMesh);

	mesh_cull = cull;

	if (tex) {
		D3DXHANDLE hVar = pPSConst->GetConstantByName(NULL, tex);
		if (!hVar) {
			LogErr("IPInterface::SetSketchMesh() Invalid variable name [%s]", tex);
			return;
		}
		mesh_tex_idx = pPSConst->GetSamplerIndex(hVar);
	}
	else mesh_tex_idx = -1;
}


// ================================================================================================
//
bool ImageProcessing::Execute(bool bInScene)
{
	return Execute(0, bInScene, gcIPInterface::ipitemplate::Rect);
}


// ================================================================================================
//
bool ImageProcessing::Execute(const char *shader, bool bInScene, DWORD blendop)
{
	Activate(shader);
	return Execute(blendop, bInScene, gcIPInterface::ipitemplate::Rect);
}


// ================================================================================================
//
bool ImageProcessing::Execute(DWORD blendop, bool bInScene, gcIPInterface::ipitemplate mode, int grp)
{
	if (!IsOK()) return false;
	if (!SetupViewPort()) return false;

	// Set device state -------------------------------------------------------
	//
	HR(pDevice->SetVertexShader(pVertex));
	HR(pDevice->SetPixelShader(pPixel));
	HR(pDevice->SetVertexDeclaration(pPosTexDecl));

	DWORD BakFill;
	pDevice->GetRenderState(D3DRS_FILLMODE, &BakFill);

	HR(pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID));
	HR(pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE));
	HR(pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, (blendop!=0)));
	HR(pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, false));
	HR(pDevice->SetRenderState(D3DRS_STENCILENABLE, false));
	HR(pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF));
	HR(pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, false));

	if (blendop == 1) {
		HR(pDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD));
		HR(pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA));
		HR(pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA));
	}

	// Define vertices --------------------------------------------------------
	//
	static const SMVERTEX Vertex[4] = {
		{0, 0, 0, 0, 0},
		{0, 1, 0, 0, 1},
		{1, 1, 0, 1, 1},
		{1, 0, 0, 1, 0}
	};

	static const WORD cIndex[6] = {0, 2, 1, 0, 3, 2};

	// Set render targets -----------------------------------------------------
	//
	for (int i=0;i<4;i++) {
		pDevice->GetRenderTarget(i, &pRtgBak[i]);
		pDevice->SetRenderTarget(i, pRtg[i]);
		if (pRtg[i]) {
			if (i == 1) { HR(pDevice->SetRenderState(D3DRS_COLORWRITEENABLE1, 0xF)); }
			if (i == 2) { HR(pDevice->SetRenderState(D3DRS_COLORWRITEENABLE2, 0xF)); }
			if (i == 3) { HR(pDevice->SetRenderState(D3DRS_COLORWRITEENABLE3, 0xF)); }
		}
	}

	// Set Depth-Stencil surface ----------------------------------------------
	//

	pDevice->GetDepthStencilSurface(&pDepthBak);

	if (pDepth) {	
		HR(pDevice->SetRenderState(D3DRS_ZENABLE, true));
		HR(pDevice->SetRenderState(D3DRS_ZWRITEENABLE, true));
		HR(pDevice->SetDepthStencilSurface(pDepth));
	}
	else {
		HR(pDevice->SetRenderState(D3DRS_ZENABLE, false));
		HR(pDevice->SetRenderState(D3DRS_ZWRITEENABLE, false));
		HR(pDevice->SetDepthStencilSurface(NULL));
	}

	// Set textures and samplers -----------------------------------------------
	//
	for (int idx=0;idx<ARRAYSIZE(pTextures);idx++) {

		if (pTextures[idx].hTex==NULL) continue;

		DWORD flags = pTextures[idx].flags;

		if (flags&IPF_CLAMP_U)			pDevice->SetSamplerState(idx, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		else if (flags&IPF_MIRROR_U)	pDevice->SetSamplerState(idx, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
		else							pDevice->SetSamplerState(idx, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);

		if (flags&IPF_CLAMP_V)			pDevice->SetSamplerState(idx, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		else if (flags&IPF_MIRROR_V)	pDevice->SetSamplerState(idx, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);
		else							pDevice->SetSamplerState(idx, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

		if (flags&IPF_CLAMP_W)			pDevice->SetSamplerState(idx, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
		else if (flags&IPF_MIRROR_W)	pDevice->SetSamplerState(idx, D3DSAMP_ADDRESSW, D3DTADDRESS_MIRROR);
		else							pDevice->SetSamplerState(idx, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);

		DWORD filter = D3DTEXF_POINT;

		if (flags&IPF_LINEAR) filter = D3DTEXF_LINEAR;
		if (flags&IPF_PYRAMIDAL) filter = D3DTEXF_PYRAMIDALQUAD;
		if (flags&IPF_GAUSSIAN) filter = D3DTEXF_GAUSSIANQUAD;

		HR(pDevice->SetSamplerState(idx, D3DSAMP_MAGFILTER, filter));
		HR(pDevice->SetSamplerState(idx, D3DSAMP_MINFILTER, filter));
		HR(pDevice->SetSamplerState(idx, D3DSAMP_MIPFILTER, D3DTEXF_NONE));

		HR(pDevice->SetTexture(idx, pTextures[idx].hTex));
	}

	// Execute ----------------------------------------------------------------
	//
	if (!bInScene) HR(pDevice->BeginScene());


	if (mode == gcIPInterface::ipitemplate::Rect)
	{
		HR(pDevice->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 4, 2, &cIndex, D3DFMT_INDEX16, &Vertex, sizeof(SMVERTEX)));
	}


	if (mode == gcIPInterface::ipitemplate::Octagon)
	{
		HR(pDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 8, pOcta, sizeof(SMVERTEX)));
	}


	if (mode == gcIPInterface::ipitemplate::Mesh)
	{

		HR(pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW));

		pMesh->Init();

		DWORD nGrp = pMesh->GroupCount();
		if (grp < 0) for (DWORD i=0;i<nGrp;i++) {
			if (mesh_tex_idx >= 0) {
				SURFHANDLE hTex = pMesh->GetTexture(i);
				HR(pDevice->SetTexture(mesh_tex_idx, SURFACE(hTex)->GetTexture()));
			}
			pMesh->RenderGroup(i);
		}
		else {
			if (mesh_tex_idx >= 0) {
				SURFHANDLE hTex = pMesh->GetTexture(grp);
				HR(pDevice->SetTexture(mesh_tex_idx, SURFACE(hTex)->GetTexture()));
			}
			pMesh->RenderGroup(grp);
		}
	}

	if (!bInScene) HR(pDevice->EndScene());

	// Disconnect render targets ----------------------------------------------
	//
	pDevice->SetDepthStencilSurface(pDepthBak);
	SAFE_RELEASE(pDepthBak);
	

	// Disconnect render targets ----------------------------------------------
	//
	for (int i=0;i<4;i++) {
		HR(pDevice->SetRenderTarget(i, pRtgBak[i]));
		SAFE_RELEASE(pRtgBak[i]);
	}

	// Disconnect textures -----------------------------------------------------
	//
	for (int idx=0;idx<ARRAYSIZE(pTextures);idx++) {
		if (pTextures[idx].hTex==NULL) continue;
		HR(pDevice->SetTexture(idx, NULL));
	}

	HR(pDevice->SetRenderState(D3DRS_FILLMODE, BakFill));

	return true;
}


// ================================================================================================
//
void ImageProcessing::SetFloat(const char *var, const void *val, int bytes)
{
	D3DXHANDLE hVar = pPSConst->GetConstantByName(NULL, var);

	if (!hVar) {
		LogErr("IPInterface::SetFloat() Invalid variable name [%s]. File[%s], Entrypoint[%s]", var, file, entry);
		return;
	}

	if (pPSConst->SetFloatArray(pDevice, hVar, (const FLOAT *)val, bytes>>2)!=S_OK) {
		LogErr("IPInterface::SetFloat() Failed. Variable[%s], File[%s], Entrypoint[%s]", var, file, entry);
	}
}


// ================================================================================================
//
void ImageProcessing::SetInt(const char *var, const int *val, int bytes)
{
	D3DXHANDLE hVar = pPSConst->GetConstantByName(NULL, var);

	if (!hVar) {
		LogErr("IPInterface::SetInt() Invalid variable name [%s]. File[%s], Entrypoint[%s]", var, file, entry);
		return;
	}

	if (pPSConst->SetIntArray(pDevice, hVar, val, bytes>>2)!=S_OK) {
		LogErr("IPInterface::SetInt() Failed. Variable[%s], File[%s], Entrypoint[%s]", var, file, entry);
	}
}


// ================================================================================================
//
void ImageProcessing::SetBool(const char *var, const bool *val, int bytes)
{
	D3DXHANDLE hVar = pPSConst->GetConstantByName(NULL, var);

	if (!hVar) {
		LogErr("IPInterface::SetBool() Invalid variable name [%s]. File[%s], Entrypoint[%s]", var, file, entry);
		return;
	}

	int *data = new int[bytes];
	for (int i=0;i<bytes;i++) data[i] = val[i];

	if (pPSConst->SetBoolArray(pDevice, hVar, (const BOOL *)data, bytes)!=S_OK) {
		LogErr("IPInterface::SetBool() Failed. Variable[%s], File[%s], Entrypoint[%s]", var, file, entry);
	}

	delete []data;
	data = NULL;
}


// ================================================================================================
//
void ImageProcessing::SetStruct(const char *var, const void *val, int bytes)
{
	D3DXHANDLE hVar = pPSConst->GetConstantByName(NULL, var);

	if (!hVar) {
		LogErr("IPInterface::SetStruct() Invalid variable name [%s]. File[%s], Entrypoint[%s]", var, file, entry);
		return;
	}

	if (pPSConst->SetValue(pDevice, hVar, val, bytes)!=S_OK) {
		LogErr("IPInterface::SetStruct() Failed. Variable[%s], File[%s], Entrypoint[%s]", var, file, entry);
	}
}


// ================================================================================================
//
void ImageProcessing::SetFloat(const char *var, float val)
{
	SetFloat(var, (const float*)&val, sizeof(float));
}


// ================================================================================================
//
void ImageProcessing::SetInt(const char *var, int val)
{
	SetInt(var, (const int*)&val, sizeof(int));
}


// ================================================================================================
//
void ImageProcessing::SetBool(const char *var, bool val)
{
	SetBool(var, (const bool*)&val, sizeof(bool));
}

// ================================================================================================
//
void ImageProcessing::SetVSFloat(const char *var, const void *val, int bytes)
{
	D3DXHANDLE hVar = pVSConst->GetConstantByName(NULL, var);

	if (pPSConst->SetFloatArray(pDevice, hVar, (const FLOAT *)val, bytes >> 2) != S_OK) {
		LogErr("IPInterface::SetFloat() Failed. Variable[%s], File[%s], Entrypoint[%s]", var, file, entry);
	}
}


// ================================================================================================
//
void ImageProcessing::SetVSInt(const char *var, const int *val, int bytes)
{
	D3DXHANDLE hVar = pVSConst->GetConstantByName(NULL, var);

	if (pPSConst->SetIntArray(pDevice, hVar, val, bytes >> 2) != S_OK) {
		LogErr("IPInterface::SetInt() Failed. Variable[%s], File[%s], Entrypoint[%s]", var, file, entry);
	}
}


// ================================================================================================
//
void ImageProcessing::SetVSBool(const char *var, const bool *val, int bytes)
{
	D3DXHANDLE hVar = pVSConst->GetConstantByName(NULL, var);

	if (!hVar) return;
	int *data = new int[bytes];
	for (int i = 0; i<bytes; i++) data[i] = val[i];

	if (pPSConst->SetBoolArray(pDevice, hVar, (const BOOL *)data, bytes) != S_OK) {
		LogErr("IPInterface::SetBool() Failed. Variable[%s], File[%s], Entrypoint[%s]", var, file, entry);
	}

	delete[]data;
}


// ================================================================================================
//
void ImageProcessing::SetVSStruct(const char *var, const void *val, int bytes)
{
	D3DXHANDLE hVar = pVSConst->GetConstantByName(NULL, var);
	if (!hVar) return;
	if (pPSConst->SetValue(pDevice, hVar, val, bytes) != S_OK) {
		LogErr("IPInterface::SetStruct() Failed. Variable[%s], File[%s], Entrypoint[%s]", var, file, entry);
	}
}


// ================================================================================================
//
void ImageProcessing::SetVSFloat(const char *var, float val)
{
	SetVSFloat(var, (const float*)&val, sizeof(float));
}


// ================================================================================================
//
void ImageProcessing::SetVSInt(const char *var, int val)
{
	SetVSInt(var, (const int*)&val, sizeof(int));
}


// ================================================================================================
//
void ImageProcessing::SetVSBool(const char *var, bool val)
{
	SetVSBool(var, (const bool*)&val, sizeof(bool));
}


// ================================================================================================
//
void ImageProcessing::SetTexture(const char *var, SURFHANDLE hTex, DWORD flags)
{
	D3DXHANDLE hVar = pPSConst->GetConstantByName(NULL, var);

	if (!hVar) {
		LogErr("IPInterface::SetTexture() Invalid variable name [%s]. File[%s], Entrypoint[%s]", var, file, entry);
		return;
	}

	DWORD idx = pPSConst->GetSamplerIndex(hVar);

	if (!hTex) {
		pTextures[idx].hTex = NULL;
		pTextures[idx].flags = 0;
		return;
	}

	pTextures[idx].hTex = SURFACE(hTex)->GetTexture();
	pTextures[idx].flags = flags;
}


// ================================================================================================
//
void ImageProcessing::SetTextureNative(const char *var, LPDIRECT3DBASETEXTURE9 hTex, DWORD flags)
{
	D3DXHANDLE hVar = pPSConst->GetConstantByName(NULL, var);

	if (!hVar) {
		LogErr("IPInterface::SetTextureNative() Invalid variable name [%s]. File[%s], Entrypoint[%s]", var, file, entry);
		return;
	}

	DWORD idx = pPSConst->GetSamplerIndex(hVar);

	if (!hTex) {
		pTextures[idx].hTex = NULL;
		pTextures[idx].flags = 0;
		return;
	}

	pTextures[idx].hTex = hTex;
	pTextures[idx].flags = flags;
}


// ================================================================================================
//
void ImageProcessing::SetTextureNative(int idx, LPDIRECT3DBASETEXTURE9 hTex, DWORD flags)
{
	if (idx < 0 || idx>15) return;

	if (!hTex) {
		pTextures[idx].hTex = NULL;
		pTextures[idx].flags = 0;
		return;
	}

	pTextures[idx].hTex = hTex;
	pTextures[idx].flags = flags;
}


// ================================================================================================
//
void ImageProcessing::SetOutput(int id, SURFHANDLE hTex)
{
	if (id<0) id=0;
	if (id>3) id=3;

	if (hTex) pRtg[id] = SURFACE(hTex)->GetSurface();
	else 	  pRtg[id] = NULL;
}


// ================================================================================================
//
void ImageProcessing::SetDepthStencil(LPDIRECT3DSURFACE9 hSrf)
{
	pDepth = hSrf;
}


// ================================================================================================
//
void ImageProcessing::SetOutputNative(int id, LPDIRECT3DSURFACE9 hSrf)
{
	if (id<0) id=0;
	if (id>3) id=3;
	pRtg[id] = hSrf;
}


// ================================================================================================
//
bool ImageProcessing::IsOK()
{
	for (auto x : Shaders) {
		if (x.second.pPixel == NULL) return false;
		if (x.second.pPSConst == NULL) return false;
	}
	return (pVertex && pVSConst && pDevice && hVP && hPos && hSiz);
}









// ================================================================================================
// PUBLIC INTERFACE
// ================================================================================================
//

gcIPInterface::~gcIPInterface()
{

}

bool gcIPInterface::CompileShader(const char *Entry)
{
	return pIPI->CompileShader(Entry);
}

bool gcIPInterface::Activate(const char *Shader)
{
	return pIPI->Activate(Shader);
}
	
void gcIPInterface::SetFloat(const char *var, float val)
{
	pIPI->SetFloat(var, val);
}

void gcIPInterface::SetInt(const char *var, int val)
{
	pIPI->SetInt(var, val);
}

void gcIPInterface::SetBool(const char *var, bool val)
{
	pIPI->SetBool(var, val);
}

void gcIPInterface::SetFloat(const char *var, const void *val, int bytes)
{
	pIPI->SetFloat(var, val, bytes);
}

void gcIPInterface::SetInt(const char *var, const int *val, int bytes)
{
	pIPI->SetInt(var, val, bytes);
}

void gcIPInterface::SetBool(const char *var, const bool *val, int bytes)
{
	pIPI->SetBool(var, val, bytes);
}

void gcIPInterface::SetStruct(const char *var, const void *val, int bytes)
{
	pIPI->SetStruct(var, val, bytes);
}

void gcIPInterface::SetVSFloat(const char *var, float val)
{
	pIPI->SetVSFloat(var, val);
}

void gcIPInterface::SetVSInt(const char *var, int val)
{
	pIPI->SetVSInt(var, val);
}

void gcIPInterface::SetVSBool(const char *var, bool val)
{
	pIPI->SetVSBool(var, val);
}

void gcIPInterface::SetVSFloat(const char *var, const void *val, int bytes)
{
	pIPI->SetVSFloat(var, val, bytes);
}

void gcIPInterface::SetVSInt(const char *var, const int *val, int bytes)
{
	pIPI->SetVSInt(var, val, bytes);
}

void gcIPInterface::SetVSBool(const char *var, const bool *val, int bytes)
{
	pIPI->SetVSBool(var, val, bytes);
}

void gcIPInterface::SetVSStruct(const char *var, const void *val, int bytes)
{
	pIPI->SetVSStruct(var, val, bytes);
}

void gcIPInterface::SetTexture(const char *var, SURFHANDLE hTex, DWORD flags)
{
	pIPI->SetTexture(var, hTex, flags);
}

void gcIPInterface::SetOutput(int id, SURFHANDLE hSrf)
{
	pIPI->SetOutput(id, hSrf);
}
	
bool gcIPInterface::IsOK()
{
	return pIPI->IsOK();
}

void gcIPInterface::SetOutputRegion(float w, float h, float x, float y)
{
	pIPI->SetTemplate(w, h, x, y);
}

void gcIPInterface::SetMesh(MESHHANDLE hMesh, const char *tex, ipicull cull)
{
	pIPI->SetMesh(hMesh, tex, cull);
}

bool gcIPInterface::Execute(bool bInScene)
{
	return pIPI->Execute(bInScene);
}

bool gcIPInterface::Execute(DWORD blendop, bool bInScene, ipitemplate mde)
{
	return pIPI->Execute(blendop, bInScene, mde);
}

int gcIPInterface::FindDefine(const char *key)
{
	return pIPI->FindDefine(key);
}

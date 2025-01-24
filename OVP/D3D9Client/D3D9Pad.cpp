// ===================================================
// Copyright (C) 2012-2021 Jarmo Nikkanen
// licensed under LGPL v2
// ===================================================


#include "D3D9Pad.h"
#include "D3D9Client.h"
#include "D3D9Surface.h"
#include "D3D9Util.h"
#include "D3D9TextMgr.h"
#include "D3D9Config.h"
#include "Log.h"
#include "Mesh.h"

using namespace oapi;


// ===============================================================================================
// Font cache
// ===============================================================================================

struct FontCache {
	int         height;
	int         orient;
	bool        prop;
	char        face[64];
	FontStyle	style;
	D3D9TextPtr pFont;
};

struct QFontCache {
	int         height;
	int         width;
	int			weight;
	char        face[64];
	FontStyle	style;
	float		spacing;
	D3D9TextPtr pFont;
};

std::vector<QFontCache *> qcache;
std::vector<FontCache *> fcache;


oapi::Font * deffont = 0;
oapi::Pen * defpen = 0;


// ===============================================================================================
//
void D3D9Pad::SinCos(int n, int k)
{
	pSinCos[k] = new D3DXVECTOR2[n];
	float s = float(PI2) / float(n);
	float q = -s / 2.0f;
	for (int i = 0; i<n; i++) { 
		pSinCos[k][i].x = sin(q) + 1.0f;	
		pSinCos[k][i].y = cos(q) + 1.0f; 
		q += s; 
	}
}



// ===============================================================================================
//
void D3D9Pad::D3D9TechInit(D3D9Client *_gc, LPDIRECT3DDEVICE9 pDevice)
{
	pDev = pDevice;
	gc = _gc;
	log = NULL;

	InitializeCriticalSectionAndSpinCount(&LogCrit, 256);


#ifdef SKPDBG
	if (fopen_s(&log, "Sketchpad.log", "w+")) { log = NULL; } // Failed
#endif // SKPDBG


	Idx = new WORD[3 * nQueueMax + 3];
	Vtx = new SkpVtx[3 * nQueueMax + 3];

	SinCos(8, 0);
	SinCos(16, 1);
	SinCos(32, 2);
	SinCos(64, 3);

	// Initialize Techniques -------------------------------------------------------------------------
	//
	char name[256];
	sprintf_s(name, 256, "Modules/D3D9Client/Sketchpad.fx");

	// Create the Effect from a .fx file.
	ID3DXBuffer* errors = 0;

	HR(D3DXCreateEffectFromFileA(pDev, name, 0, 0, 0, 0, &FX, &errors));

	if (errors) {
		LogErr("Effect Error: %s",(char*)errors->GetBufferPointer());
		MessageBoxA(0, (char*)errors->GetBufferPointer(), "Sketchpad.fx Error", 0);
		FatalAppExitA(0,"Critical error has occured. See Orbiter.log for details");
	}

	if (FX==0) {
		LogErr("Failed to create an Effect (%s)",name);
		MissingRuntimeError();
		return;
	}

	if (Config->ShaderDebug) {
		LPD3DXBUFFER pBuffer = NULL;
		if (D3DXDisassembleEffect(FX, true, &pBuffer) == S_OK) {
			FILE *fp = NULL;
			if (!fopen_s(&fp, "Sketchpad_asm.html", "w")) {
				fwrite(pBuffer->GetBufferPointer(), 1, pBuffer->GetBufferSize(), fp);
				fclose(fp);
			}
			pBuffer->Release();
		}
	}


	pNoise	  = gc->GetNoiseTex();

	eDrawMesh = FX->GetTechniqueByName("SketchMesh");
	eSketch   = FX->GetTechniqueByName("SketchTech");
	eVP       = FX->GetParameterByName(0, "gVP");
	eTex0     = FX->GetParameterByName(0, "gTex0");
	eFnt0	  = FX->GetParameterByName(0, "gFnt0");
	eDashEn   = FX->GetParameterByName(0, "gDashEn");
	eW		  = FX->GetParameterByName(0, "gW");
	eKey	  = FX->GetParameterByName(0, "gKey");
	ePen      = FX->GetParameterByName(0, "gPen");
	eWVP	  = FX->GetParameterByName(0, "gWVP");
	eFov	  = FX->GetParameterByName(0, "gFov");
	eRandom   = FX->GetParameterByName(0, "gRandom");
	eTarget	  = FX->GetParameterByName(0, "gTarget");
	eTexEn	  = FX->GetParameterByName(0, "gTexEn");
	eFntEn    = FX->GetParameterByName(0, "gFntEn");
	eKeyEn    = FX->GetParameterByName(0, "gKeyEn");
	eWide	  = FX->GetParameterByName(0, "gWide");
	eWidth	  = FX->GetParameterByName(0, "gWidth");
	eSize	  = FX->GetParameterByName(0, "gSize");
	eMtrl	  = FX->GetParameterByName(0, "gMtrl");
	eShade    = FX->GetParameterByName(0, "gShade");
	ePos	  = FX->GetParameterByName(0, "gPos");
	ePos2	  = FX->GetParameterByName(0, "gPos2");
	eCov	  = FX->GetParameterByName(0, "gCov");
	eCovEn	  = FX->GetParameterByName(0, "gClipEn");
	eClearEn  = FX->GetParameterByName(0, "gClearEn");
	eEffectsEn= FX->GetParameterByName(0, "gEffectsEn");
	eNoiseTex = FX->GetParameterByName(0, "gNoiseTex");
	eGamma	  = FX->GetParameterByName(0, "gGamma");
	eNoiseColor = FX->GetParameterByName(0, "gNoiseColor");
	eColorMatrix = FX->GetParameterByName(0, "gColorMatrix");
	
}


// ===============================================================================================
//
void D3D9Pad::GlobalExit()
{
	LogAlw("Clearing Font Cache... %d Fonts are stored in the cache",fcache.size() + qcache.size());
	for (auto it = fcache.begin(); it != fcache.end(); ++it) {
		delete *it;
	}
	for (auto it = qcache.begin(); it != qcache.end(); ++it) {
		delete *it;
	}
	fcache.clear();
	qcache.clear();

	SAFE_RELEASE(FX);
	SAFE_DELETEA(Idx);
	SAFE_DELETEA(Vtx);
	for (int i=0;i<4;i++) SAFE_DELETEA(pSinCos[i]);

	if (log) fclose(log);
	log = NULL;

	DeleteCriticalSection(&LogCrit);
}

// ===============================================================================================
//
void D3D9Pad::Log(const char *format, ...) const
{
	if (log == NULL) return;
	EnterCriticalSection(&LogCrit);
	char ErrBuf[1024];
	DWORD th = GetCurrentThreadId();
	va_list args;
	va_start(args, format);
	_vsnprintf_s(ErrBuf, 1024, 1024, format, args);
	va_end(args);
	fprintf_s(log, "<0x%X> [%s] %s\n", th, _PTR(this), ErrBuf);
	fflush(log);
	LeaveCriticalSection(&LogCrit);
}

// ===============================================================================================
//
void D3D9Pad::Reset()
{
	bBeginDraw = false;
	bMustEndScene = false;
	vI = 0;
	iI = 0;
	D3DXMatrixIdentity(&mO);
	D3DXMatrixIdentity(&mVOrig);
	D3DXMatrixIdentity(&mPOrig);
	vTarget = D3DXVECTOR4(1,1,1,1);
	pTgt = NULL;
	pDep = NULL;
	zfar = 1.0f;
	tgt = { 0,0,0,0 };
}



// ===============================================================================================
// Restore Default Settings: Fonts, Pens, Colors, etc...
//
void D3D9Pad::LoadDefaults()
{
	assert(vI == 0);

	cfont = deffont;
	cpen = NULL;
	cbrush = NULL;
	hTexture = NULL;
	hFontTex = NULL;
	cx = 0;
	cy = 0;
	linescale = 1.0f;
	pattern = 1.0f;
	Enable = 0;
	RenderConfig = 0;

	tah = LEFT;
	tav = TOP;
	vmode = ORTHO;
	tCurrent = NONE;
	Change = SKPCHG_ALL;
	bkmode = TRANSPARENT;
	dwBlendState = Sketchpad::BlendState::ALPHABLEND;

	bColorComp = true;
	bLine = false;
	bEnableScissor = false;
	bDepthEnable = false;
	bColorKey = false;
	QPen.bEnabled = false;
	QBrush.bEnabled = false;

	memset(ClipData, 0, sizeof(ClipData));
	ScissorRect = { 0,0,0,0 };

	cColorKey  = D3DXCOLOR(DWORD(0));
	brushcolor = SkpColor(0xFF00FF00);
	bkcolor    = SkpColor(0xFF000000);
	textcolor  = SkpColor(0xFF00FF00);
	pencolor   = SkpColor(0xFF00FF00);

	D3DXMatrixIdentity(&mVP);
	D3DXMatrixIdentity(&mW);
	D3DXMatrixIdentity(&mP);
	D3DXMatrixIdentity(&mV);
	D3DXMatrixIdentity((D3DXMATRIX*)&ColorMatrix);

	Gamma = FVECTOR4(1, 1, 1, 1);
	Noise = FVECTOR4(0, 0, 0, 0);
}


// ===============================================================================================
// class D3D9Pad
// ===============================================================================================
// Constructor will create D3D9Pad interface but doesn't prepare it for drawing.
// BeginDrawing() must be called
//
D3D9Pad::D3D9Pad(SURFHANDLE s, const char *_name) : Sketchpad(s),
	_isSaveBuffer(false),
	_saveBuffer(NULL),
	_saveBufferSize(0)
{
#ifdef SKPDBG 
	Log("#### Sketchpad Interface Created");
#endif
	pRState = new RenderState(pDev);
	if (_name) strcpy_s(name, 32, _name);
	else strcpy_s(name, 32, "NoName");
	Reset();
	LoadDefaults();
}


// ===============================================================================================
// class D3D9Pad
// ===============================================================================================
// Constructor will create D3D9Pad interface but doesn't prepare it for drawing.
// BeginDrawing() must be called
//
D3D9Pad::D3D9Pad(const char *_name) : Sketchpad(NULL),
	_isSaveBuffer(false),
	_saveBuffer(NULL),
	_saveBufferSize(0)
{
#ifdef SKPDBG 
	Log("#### Sketchpad Interface Created (NoTgt)");
#endif
	if (_name) strcpy_s(name, 32, _name);
	else strcpy_s(name, 32, "NoName");
	pRState = new RenderState(pDev);
	Reset();
	LoadDefaults();
}



// ===============================================================================================
//
D3D9Pad::~D3D9Pad ()
{
#ifdef SKPDBG 
	Log("#### Sketchpad Interface Deleted");
#endif
	if (pRState) delete pRState;
	assert(bBeginDraw == false);
	SAFE_DELETEA(_saveBuffer);
}


// ===============================================================================================
// Private
//
void D3D9Pad::SetViewProj(const D3DXMATRIX* pV, const D3DXMATRIX* pP)
{
	mV = mVOrig = *pV;
	mP = mPOrig = *pP;
}


// ===============================================================================================
// Bind existing Sketchpad interface to TOP render targets and prepare for rendering
//
void D3D9Pad::BeginDrawing() 
{
	// Acquire render targets from Stack
	BeginDrawing(gc->GetTopRenderTarget(), gc->GetTopDepthStencil());
}



// ===============================================================================================
// Bind existing Sketchpad interface to render targets and prepare for rendering
//
void D3D9Pad::BeginDrawing(LPDIRECT3DSURFACE9 pRenderTgt, LPDIRECT3DSURFACE9 pDepthStensil)
{
#ifdef SKPDBG 
	Log("==== BeginDrawing %s, %s ====\n", _PTR(pRenderTgt), _PTR(pDepthStensil));
#endif

	assert(pRenderTgt != NULL);

	if (vI != 0) LogErr("Sketchpad %s has received drawing commands outside Begin() End() pair", _PTR(this));

	if (bBeginDraw == true) {
		LogErr("D3D9Pad::BeginDrawing() called multiple times");
		HALT();
	}

	// Capture current device state
	pRState->Capture();

	Reset();
	
	bBeginDraw = true;

	// If not already in scene, then start a new one
	if (gc->IsInScene() == false) {
		gc->BeginScene();
		bMustEndScene = true;
	}
	else bMustEndScene = false;

	pRenderTgt->GetDesc(&tgt_desc);
	zfar = float(max(tgt_desc.Width, tgt_desc.Height));
	D3DXMatrixOrthoOffCenterLH(&mO, 0.0f, (float)tgt_desc.Width, (float)tgt_desc.Height, 0.0f, 0.0f, zfar);
	vTarget = D3DXVECTOR4(2.0f / (float)tgt_desc.Width, 2.0f / (float)tgt_desc.Height, (float)tgt_desc.Width, (float)tgt_desc.Height);
	
	pTgt = pRenderTgt;
	pDep = pDepthStensil;
	tgt = { 0, 0, (long)tgt_desc.Width, (long)tgt_desc.Height };
	Change = SKPCHG_ALL;
}



// ===============================================================================================
//
void D3D9Pad::EndDrawing()
{
#ifdef SKPDBG 
	Log("==== EndDrawing ====\n");
#endif

	if (bBeginDraw == false) {
		LogErr("D3D9Pad::EndDrawing() called without BeginDrawing()");
		HALT();
	}

	Flush();

	bBeginDraw = false;

	if (pRState) pRState->Restore();

	if (bMustEndScene) {
		gc->EndScene();
		bMustEndScene = false;
	}
}



// ===============================================================================================
//
bool D3D9Pad::Flush(HPOLY hPoly)
{
	if (bBeginDraw == false) {
		LogErr("D3D9Pad::Flush() called without BeginDrawing()");
		HALT();
	}
	
	UINT numPasses;
	static DWORD bkALPHA, bkZEN, bkZW, bkCULL;

	if ((iI == 0) && (hPoly == NULL)) {
#ifdef SKPDBG 
		Log("Flush (Nothing)", hPoly, iI);
#endif
		return false;
	}

	DWORD dwBlend = dwBlendState & 0xF;
	DWORD dwFilter = dwBlendState & 0xF0;
	
#ifdef SKPDBG 
	char buf[128]; strcpy_s(buf, 128, "");
	char buf2[128]; strcpy_s(buf2, 128, "");

	if (dwBlend == SKPBS_ALPHABLEND) strcpy_s(buf, 128, "SKPBS_ALPHABLEND");
	if (dwBlend == SKPBS_COPY) strcpy_s(buf, 128, "SKPBS_COPY");
	if (dwBlend == SKPBS_COPY_ALPHA) strcpy_s(buf, 128, "SKPBS_COPY_ALPHA");
	if (dwBlend == SKPBS_COPY_COLOR) strcpy_s(buf, 128, "SKPBS_COPY_COLOR");
	
	if (bDepthEnable && pDep) strcpy_s(buf2, 128, "DEPTH_ENABLED");
	else strcpy_s(buf2, 128, "DEPTH_DISABLED");

	Log("Flush [%s] [%s] hPloy=%s, iI=%hu", buf, buf2, _PTR(hPoly), iI);
#endif

	HR(pDev->GetRenderState(D3DRS_ALPHABLENDENABLE, &bkALPHA));

	//HR(pDev->GetRenderState(D3DRS_ZENABLE, &bkZEN));
	//HR(pDev->GetRenderState(D3DRS_ZWRITEENABLE, &bkZW));
	//HR(pDev->GetRenderState(D3DRS_CULLMODE, &bkCULL));

	HR(pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE));
	HR(pDev->SetVertexDeclaration(pSketchpadDecl));
		
	HR(FX->SetFloat(eRandom, float(oapiRand())));
	HR(FX->SetVector(eTarget, &vTarget));
	HR(FX->SetTechnique(eSketch));
	HR(FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE));

	if (vmode == ORTHO) {
		HR(FX->BeginPass(0));
	}
	else {
		HR(FX->BeginPass(1));
	}

	if (dwBlend == Sketchpad::BlendState::ALPHABLEND) {
		pDev->SetRenderState(D3DRS_COLORWRITEENABLE, 0x7);
		HR(pDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
	}
	else if (dwBlend == Sketchpad::BlendState::COPY) {
		pDev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
		HR(pDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
	}
	else if (dwBlend == Sketchpad::BlendState::COPY_ALPHA) {
		pDev->SetRenderState(D3DRS_COLORWRITEENABLE, 0x8);
		HR(pDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
	}
	else if (dwBlend == Sketchpad::BlendState::COPY_COLOR) {
		pDev->SetRenderState(D3DRS_COLORWRITEENABLE, 0x7);
		HR(pDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
	}

	if (dwFilter) {
		if (dwFilter == Sketchpad::BlendState::FILTER_POINT) {
			pDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
			pDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		}
		if (dwFilter == Sketchpad::BlendState::FILTER_ANISOTROPIC) {
			pDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
			pDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
			HR(pDev->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 8));
		}
	}

	if (bDepthEnable && pDep) {
		pDev->SetRenderState(D3DRS_ZENABLE, 1);
		pDev->SetRenderState(D3DRS_ZWRITEENABLE, 1);
	}
	else {
		pDev->SetRenderState(D3DRS_ZENABLE, 0);
		pDev->SetRenderState(D3DRS_ZWRITEENABLE, 0);
	}

	if (hPoly) {
		assert(iI == 0);
		D3D9PolyBase *pBase = static_cast<D3D9PolyBase *>(hPoly);
		pBase->Draw(this, pDev);
	}
	else {
		if (tCurrent == TRIANGLE) HR(pDev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, vI, iI / 3, Idx, D3DFMT_INDEX16, Vtx, sizeof(SkpVtx)));
		if (tCurrent == LINE) HR(pDev->DrawIndexedPrimitiveUP(D3DPT_LINELIST, 0, vI, iI / 2, Idx, D3DFMT_INDEX16, Vtx, sizeof(SkpVtx)));
	}

	HR(FX->EndPass());
	HR(FX->End());
	
	HR(pDev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF));
	HR(pDev->SetRenderState(D3DRS_ALPHABLENDENABLE, bkALPHA));

	//HR(pDev->SetRenderState(D3DRS_ZENABLE, bkZEN));
	//HR(pDev->SetRenderState(D3DRS_ZWRITEENABLE, bkZW));
	//HR(pDev->SetRenderState(D3DRS_CULLMODE, bkCULL));
		
	HR(pDev->SetRenderState(D3DRS_SCISSORTESTENABLE, 0));

	if (dwFilter) {
		pDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		pDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	}
		
	iI = vI = 0;

	return true;
	
}


// ===============================================================================================
//
bool D3D9Pad::Topology(Topo tRequest)
{
	if (tRequest == LINE) {
		if (bLine) { // Can we do LINE Topology ?
			if (tRequest != tCurrent) Change |= SKPCHG_TOPOLOGY;
			SetupDevice(tRequest);
			return true;
		}
		return false;
	}

	if (tRequest != tCurrent) Change |= SKPCHG_TOPOLOGY;

	SetupDevice(tRequest);

	return true;
}


// ===============================================================================================
//
void D3D9Pad::SetupDevice(Topo tNew)
{

	// Check that this is the top one. Only do the check for ones created with oapiGetSketchpad()
	//if (GetSurface()) assert(gc->GetTopInterface() == this);

	// If the queue is filling up, Flush it
	//
	if (iI > (nQueueMax >> 1)) Flush();


	// Has something changed ?
	//
	if (!Change) return;


	// Flush pending drawing instructions before changing a device state
	//
	Flush();

	tCurrent = tNew;

#ifdef SKPDBG 
	char buf[512];
	strcpy_s(buf, 512, "");
	if (Change&SKPCHG_TOPOLOGY)	 strcat_s(buf, 512, "SKPCHG_TOPOLOGY ");
	if (Change&SKPCHG_TRANSFORM) strcat_s(buf, 512, "SKPCHG_TRANSFORM ");
	if (Change&SKPCHG_CLIPCONE)	 strcat_s(buf, 512, "SKPCHG_CLIPCONE ");
	if (Change&SKPCHG_PEN) strcat_s(buf, 512, "SKPCHG_PEN ");
	if (Change&SKPCHG_EFFECTS)	 strcat_s(buf, 512, "SKPCHG_EFFECTS ");
	if (Change&SKPCHG_CLIPRECT)	 strcat_s(buf, 512, "SKPCHG_CLIPRECT ");
	if (Change&SKPCHG_TEXTURE)	strcat_s(buf, 512, "SKPCHG_TEXTURE ");
	if (Change&SKPCHG_FONT)	strcat_s(buf, 512, "SKPCHG_FONT ");
	if (Change&SKPCHG_DEPTH)	strcat_s(buf, 512, "SKPCHG_DEPTH ");
	if (Change&SKPCHG_PATTERN)	strcat_s(buf, 512, "SKPCHG_PATTERN ");
	Log("StateChange = [%s]", buf);
#endif
	

	// Apply a new setup -----------------------------------------------------------------
	//
	if (Change == SKPCHG_TOPOLOGY) {
		HR(FX->SetBool(eWide, (tCurrent == TRIANGLE)));
		Change = 0;
		return;
	}


	// Apply a new setup -----------------------------------------------
	//
	if (Change & (SKPCHG_TRANSFORM | SKPCHG_CLIPCONE)) {

		if (vmode == ORTHO) {
			D3DXMATRIX mWVP;
			D3DXMatrixMultiply(&mWVP, &mW, &mO);
			HR(FX->SetMatrix(eWVP, &mWVP));
			HR(FX->SetMatrix(eVP, &mO));
			HR(FX->SetMatrix(eW, &mW));
			HR(FX->SetBool(eCovEn, false));
		}
		else {
			float d = float(tgt_desc.Height) * mP._22;
			float f = atan(1.0f / d) * 1.7f;
			D3DXMatrixMultiply(&mVP, &mV, &mP);
			HR(FX->SetMatrix(eVP, &mVP));
			HR(FX->SetMatrix(eW, &mW));
			HR(FX->SetFloat(eFov, f));
			HR(FX->SetBool(eCovEn, ClipData[0].bEnable || ClipData[1].bEnable));
		}
	}


	// Apply a new setup -----------------------------------------------------------------
	//
	if (Change & SKPCHG_PEN) {
		float offset = 0.0f;
		int w = int(ceil(GetPenWidth()));
		if ((w & 1) == 0) offset = 0.5f;
		HR(FX->SetValue(ePen, &pencolor.fclr, sizeof(D3DXCOLOR)));
		HR(FX->SetBool(eDashEn, IsDashed()));
		HR(FX->SetValue(eWidth, ptr(D3DXVECTOR3(GetPenWidth(), pattern*0.13f, offset)), sizeof(D3DXVECTOR3)));
	}


	// Apply a new setup -----------------------------------------------------------------
	//
	if (Change & SKPCHG_EFFECTS) {
		HR(FX->SetValue(eNoiseColor, &Noise, sizeof(FVECTOR4)));
		HR(FX->SetValue(eGamma, &Gamma, sizeof(FVECTOR4)));
		HR(FX->SetTexture(eNoiseTex, pNoise));
		HR(FX->SetValue(eColorMatrix, &ColorMatrix, sizeof(FMATRIX4)));
		HR(FX->SetBool(eEffectsEn, Enable != 0));
	}


	// Apply a new setup -----------------------------------------------------------------
	//
	if (Change & SKPCHG_CLIPRECT) {
		if (bEnableScissor) {
			pDev->SetScissorRect(&ScissorRect);
			pDev->SetRenderState(D3DRS_SCISSORTESTENABLE, 1);
		}
		else {
			pDev->SetRenderState(D3DRS_SCISSORTESTENABLE, 0);
		}
	}


	// Apply a new setup -----------------------------------------------------------------
	//
	if (Change & SKPCHG_CLIPCONE) {
		if (ClipData[0].bEnable || ClipData[1].bEnable) {
			HR(FX->SetValue(ePos, &ClipData[0].uDir, sizeof(D3DXVECTOR3)));
			HR(FX->SetValue(ePos2, &ClipData[1].uDir, sizeof(D3DXVECTOR3)));
			HR(FX->SetValue(eCov, ptr(D3DXVECTOR4(ClipData[0].ca, ClipData[0].dst, ClipData[1].ca, ClipData[1].dst)), sizeof(D3DXVECTOR4)));
		}
	}


	// Apply a new setup -----------------------------------------------------------------
	//
	if (Change & (SKPCHG_TEXTURE | SKPCHG_FONT)) {

		float tw = 1.0f, th = 1.0f;

		D3DSURFACE_DESC desc;

		if (hTexture) {

			hTexture->GetLevelDesc(0, &desc);

			tw = 1.0f / float(desc.Width);
			th = 1.0f / float(desc.Height);

			if (Change & SKPCHG_TEXTURE) {
				HR(FX->SetTexture(eTex0, hTexture));
				HR(FX->SetBool(eKeyEn, bColorKey));
				HR(FX->SetValue(eKey, &cColorKey, sizeof(D3DXCOLOR)));
			}
		}

		if (hFontTex) {
			if (Change & SKPCHG_FONT) {
				HR(FX->SetTexture(eFnt0, hFontTex));
			}
		}

		HR(FX->SetVector(eSize, ptr(D3DXVECTOR4(tw, th, 1.0f, 1.0f))));
		HR(FX->SetBool(eTexEn, (hTexture != NULL)));
		HR(FX->SetBool(eFntEn, (hFontTex != NULL)));
	}


	// Apply a new setup -----------------------------------------------------------------
	//
	if (Change & SKPCHG_TOPOLOGY) {
		HR(FX->SetBool(eWide, (tCurrent == TRIANGLE)));
	}

	// All Clear
	Change = 0;

	return;
}


// ===============================================================================================
//
DWORD D3D9Pad::ColorComp(DWORD c) const
{
	if (bColorComp) if ((c & 0xFF000000) == 0) return c | 0xFF000000;
	return c;
}

// ===============================================================================================
//
SkpColor D3D9Pad::ColorComp(const SkpColor &c) const
{
	if (bColorComp) {
		if ((c.dclr & 0xFF000000) == 0) {
			DWORD q = c.dclr | 0xFF000000;
			return SkpColor(q);
		}
	}
	return c;
}






// ===============================================================================================
//
HDC D3D9Pad::GetDC()
{
	DWORD *cf = SURFACE(GetSurface())->GetClientFlags();

	if ((*cf & OAPISURF_SKP_GDI_WARN) == 0) {
		*cf |= OAPISURF_SKP_GDI_WARN;
		LogErr("Call to obsolete Sketchpad::GetDC() detected. Returned NULL");
		if (Config->DebugBreak) DebugBreak();
	}

	return NULL;
}


// ===============================================================================================
//
Font *D3D9Pad::SetFont(Font *font)
{
	if (cfont == font) return font;

#ifdef SKPDBG 
	LOGFONTA lf;
	GetObjHandle(font->GetGDIFont(), sizeof(LOGFONT), &lf);
	Log("SetFont(%s) Face=[%s] Height=%d Weight=%d", _PTR(font), lf.lfFaceName, lf.lfHeight, lf.lfWeight);
#endif
	// No "Change" falgs required here, covered in SetFontTextureNative()

	Font *pfont = cfont;
	if (font) cfont = font;
	else      cfont = deffont;
	return pfont;
}


// ===============================================================================================
//
Brush *D3D9Pad::SetBrush (Brush *brush)
{
	if (cbrush == brush && QBrush.bEnabled == false) return brush;

#ifdef SKPDBG 
	Log("SetBrush(%s)", _PTR(brush));
#endif

	// No "Change" falgs required here, color stored in vertex data

	QBrush.bEnabled = false;

	Brush *pbrush = cbrush;
	cbrush = brush;
	if (cbrush) brushcolor = ColorComp((static_cast<D3D9PadBrush *>(cbrush))->clr);
	else	    brushcolor = SkpColor(0);

	IsLineTopologyAllowed();

	return const_cast<Brush*>(pbrush);
}


// ===============================================================================================
//
Pen *D3D9Pad::SetPen (Pen *pen)
{
	if (cpen == pen && QPen.bEnabled == false) return pen;

#ifdef SKPDBG 
	Log("SetPen(%s)", _PTR(pen));
#endif

	// Change required due to pen width and style change
	Change |= SKPCHG_PEN;

	QPen.bEnabled = false;

	Pen *ppen = cpen;
	if (pen) cpen = pen;
	else     cpen = NULL;
	if (cpen) pencolor = ColorComp(static_cast<D3D9PadPen *>(cpen)->clr);

	IsLineTopologyAllowed();

	return ppen;
}


// ===============================================================================================
//
void D3D9Pad::SetTextAlign (TAlign_horizontal _tah, TAlign_vertical _tav)
{
	// No Change flags
	tah = _tah;
	tav = _tav;
}


// ===============================================================================================
//
DWORD D3D9Pad::SetTextColor(DWORD col)
{
	// Color stored in vertex data, no Change required
	DWORD prev = textcolor.dclr;
	textcolor = SkpColor(ColorComp(col));
	return prev;
}


// ===============================================================================================
//
DWORD D3D9Pad::SetBackgroundColor(DWORD col)
{
	// Color stored in vertex data, no Change required
	DWORD prev = bkcolor.dclr;
	bkcolor = SkpColor(ColorComp(col));
	return prev;
}


// ===============================================================================================
//
void D3D9Pad::SetBackgroundMode(BkgMode mode)
{
	// No Change required

	switch (mode) {
		case BK_TRANSPARENT: bkmode = TRANSPARENT; break;
		case BK_OPAQUE:      bkmode = OPAQUE; break;
	}
}


// ===============================================================================================
//
DWORD D3D9Pad::GetCharSize ()
{
	TEXTMETRIC tm;
	if (cfont==NULL) return 0;
	static_cast<const D3D9PadFont *>(cfont)->pFont->GetD3D9TextMetrics(&tm);
	return MAKELONG(tm.tmHeight-tm.tmInternalLeading, tm.tmAveCharWidth);
}


// ===============================================================================================
//
DWORD D3D9Pad::GetLineHeight () // ... *with* "internal leading"
{
	TEXTMETRIC tm;
	if (cfont == NULL) return 0;
	static_cast<const D3D9PadFont *>(cfont)->pFont->GetD3D9TextMetrics(&tm);
	return tm.tmHeight;
}


// ===============================================================================================
//
DWORD D3D9Pad::GetTextWidth (const char *str, int len)
{
	if (str) if (str[0] == '_') if (strcmp(str, "_SkpVerInfo") == 0) return 2;
	if (cfont==NULL) return 0;
	return DWORD(static_cast<D3D9PadFont *>(cfont)->pFont->Length2(str, len));
}


// ===============================================================================================
//
void D3D9Pad::SetOrigin (int x, int y)
{
#ifdef SKPDBG 
	Log("SetOrigin(%d, %d)", x, y);
#endif
	Change |= SKPCHG_TRANSFORM;

	mW._41 = float(x);
	mW._42 = float(y);
}


// ===============================================================================================
//
void D3D9Pad::GetOrigin(int *x, int *y) const
{
	if (x) *x = int(mW._41);
	if (y) *y = int(mW._42);
}


// ===============================================================================================
//
bool D3D9Pad::HasPen() const
{
	if (QPen.bEnabled) return true;
	if (cpen==NULL) return false;
	if (static_cast<D3D9PadPen*>(cpen)->style==PS_NULL) return false;
	return true;
}


// ===============================================================================================
//
void D3D9Pad::IsLineTopologyAllowed()
{
	bLine = false;
	if ((HasPen() == true) && (GetPenWidth() < 1.1f)) bLine = true;
}


// ===============================================================================================
//
bool D3D9Pad::IsDashed() const
{
	if (QPen.bEnabled) return QPen.style == 2;
	if (cpen==NULL) return false;
	if (static_cast<D3D9PadPen*>(cpen)->style==PS_DOT) return true;
	return false;
}


// ===============================================================================================
//
bool D3D9Pad::IsAlphaTarget() const
{
	if (tgt_desc.Format == D3DFMT_A8R8G8B8) return true;
	if (tgt_desc.Format == D3DFMT_A16B16G16R16F) return true;
	if (tgt_desc.Format == D3DFMT_A32B32G32R32F) return true;
	return false;
}


// ===============================================================================================
//
bool D3D9Pad::HasBrush() const
{
	if (QBrush.bEnabled) return true;
	return (cbrush != NULL);
}


// ===============================================================================================
//
float D3D9Pad::GetPenWidth() const
{
	if (QPen.bEnabled) return linescale * QPen.width;
	if (cpen==NULL) return 1.0f;
	return float(static_cast<D3D9PadPen*>(cpen)->width*linescale);
}


// ===============================================================================================
//
void D3D9Pad::WrapOneLine (char* str, int len, int maxWidth)
{
	D3D9TextPtr pText = static_cast<D3D9PadFont *>(cfont)->pFont;
	if (pText->Length2(str) > maxWidth) {
		char *pStr = str, // sub-string start
		     *it = pStr,  // 'iterator' char
		     *pEnd = str + len, // <= point to terminating zero
		     *pLastSpace = NULL;
		float currentWidth = 0;
		while (it < pEnd)
		{
			while (it < pEnd && currentWidth < maxWidth) {
				if (*it == ' ') { pLastSpace = it; }
				currentWidth = pText->Length2( pStr, int(it - pStr + 1) );
				++it;
			}
			// only split if we have space for it AND we have to (avoids cutting the last word)
			if (pLastSpace != NULL && currentWidth >= maxWidth) {
				*pLastSpace = '\n';
				pStr = pLastSpace + 1; // skip the space (now a newline)
				currentWidth = 0;
				pLastSpace = NULL;
			}
		}
	}
}

// ===============================================================================================
//
bool D3D9Pad::TextBox (int x1, int y1, int x2, int y2, const char *str, int len)
{
#ifdef SKPDBG 
	Log("TextBox()");
#endif

	// No "Setup" required, done on PrintSkp

	if (cfont==NULL) return false;

	bool result = true;
	int lineSpace = static_cast<D3D9PadFont *>(cfont)->pFont->GetLineSpace();

	ToSaveBuffer(str, len);

	char *pch, *pEnd =_saveBuffer+len; // <= point to terminating zero
	for (pch = strtok(_saveBuffer, "\n"); pch != NULL; pch = strtok(NULL, "\n"))
	{
		int _len = lstrlen(pch);
		if (_len>1) { WrapOneLine(pch, _len, x2-x1); }
		if (pch+_len < pEnd) { *(pch+_len) = '\n'; } // strtok splits by inserting '\0's => revert'em
	}

	// "forEach(line...)" split multi-lines
	for (pch = strtok(_saveBuffer, "\n"); pch != NULL; pch = strtok(NULL, "\n")) {
		result = Text(x1, y1, pch, -1); // len is irrelevant for pointer into 'save' buffer
		y1 += lineSpace;
	}

	ReleaseSaveBuffer();
	return result;
}

// ===============================================================================================
//
bool D3D9Pad::Text (int x, int y, const char *str, int len)
{
#ifdef SKPDBG 
	Log("Text(%s)", str);
#endif
	// No "Setup" required, done on PrintSkp

	if (cfont==NULL) return false;

	D3D9TextPtr pText = static_cast<D3D9PadFont *>(cfont)->pFont;

	switch(tah) {
		default:
		case LEFT:   pText->SetTextHAlign(0); break;
		case CENTER: pText->SetTextHAlign(1); break;
		case RIGHT:  pText->SetTextHAlign(2); break;
	}

	switch(tav) {
		default:
		case TOP:      pText->SetTextVAlign(0); break;
		case BASELINE: pText->SetTextVAlign(1); break;
		case BOTTOM:   pText->SetTextVAlign(2); break;
	}

	pText->SetRotation(static_cast<D3D9PadFont *>(cfont)->rotation);
	pText->SetScaling(1.0f);
	pText->PrintSkp(this, float(x - 1), float(y - 1), str, len, (bkmode == OPAQUE));

	return true;
}


// ===============================================================================================
//
void SwapRB(DWORD *c)
{
	DWORD r = ((*c) & 0x00FF0000) >> 16;
	DWORD b = ((*c) & 0x000000FF) << 16;
	*c = ((*c) & 0xFF00FF00) | b | r;
}


// ===============================================================================================
//
void D3D9Pad::Pixel (int x, int y, DWORD col)
{
	FillRect(x, y, x + 1, y + 2, ColorComp(SkpColor(col)));
}


// ===============================================================================================
//
void D3D9Pad::MoveTo (int x, int y)
{
	cx = x;
	cy = y;
}


// ===============================================================================================
//
void D3D9Pad::LineTo (int tx, int ty)
{
	if (!HasPen()) return;
#ifdef SKPDBG 
	Log("LineTo()");
#endif
	Line(cx, cy, tx, ty);
	cx=tx; cy=ty;
}


// ===============================================================================================
//
void D3D9Pad::Line (int x0, int y0, int x1, int y1)
{
	if (!HasPen()) return;
#ifdef SKPDBG 
	Log("Line()");
#endif
	IVECTOR2 pt[2];

	pt[0].x = x0; pt[0].y = y0;
	pt[1].x = x1; pt[1].y = y1;

	AppendLineVertexList<IVECTOR2>(pt);

	cx = x1; cy = y1;
}


// ===============================================================================================
//
void D3D9Pad::FillRect(int l, int t, int r, int b, const SkpColor &c)
{
	if (r == l) return;
	if (b == t) return;
	if (r < l) swap(r, l);
	if (b < t) swap(t, b);

#ifdef SKPDBG 
	Log("FillRect()");
#endif
	if (Topology(TRIANGLE)) {
		AddRectIdx(vI);
		SkpVtxIC(Vtx[vI++], l, t, c);
		SkpVtxIC(Vtx[vI++], r, t, c);
		SkpVtxIC(Vtx[vI++], r, b, c);
		SkpVtxIC(Vtx[vI++], l, b, c);
	}
}


// ===============================================================================================
//
void D3D9Pad::Rectangle (int l, int t, int r, int b)
{
	if (r == l) return;
	if (b == t) return;
	if (r < l) swap(r, l);
	if (b < t) swap(t, b);

#ifdef SKPDBG 
	Log("Rectangle()");
#endif
	r--;
	b--;

	// Fill interion ----------------------------------------------
	//
	if (HasBrush()) FillRect(l, t, r, b, brushcolor);

	// Draw outline ------------------------------------------
	//
	if (HasPen()) {

		IVECTOR2 pts[4];
		pts[0].x = pts[3].x = l;
		pts[0].y = pts[1].y = t;
		pts[1].x = pts[2].x = r;
		pts[2].y = pts[3].y = b;

		AppendLineVertexList<IVECTOR2>(pts, 4, true);
	}
}


// ===============================================================================================
//
void D3D9Pad::Ellipse (int x0, int y0, int x1, int y1)
{
	if (x1 == x0) return;
	if (y1 == y0) return;
	if (x1 < x0) swap(x0, x1);
	if (y1 < y0) swap(y0, y1);

#ifdef SKPDBG 
	Log("Ellipse()");
#endif

	float w = float(x1 - x0); float h = float(y1 - y0);	float fx0 = float(x0); float fy0 = float(y0);
	DWORD z = max((x1-x0), (y1-y0));

	w *= 0.5f;
	h *= 0.5f;
	//fl += w;
	//ft += h;

	IVECTOR2 pts[65] = { 0 };

	WORD n = 8;
	WORD q = 0;

	if (z > 16) q = 1, n = 16;
	if (z > 32) q = 2, n = 32;
	if (z > 64) q = 3, n = 64;

	for (WORD i = 0; i<n; i++) {
		pts[i].x = long(fx0 + pSinCos[q][i].x * w);
		pts[i].y = long(fy0 + pSinCos[q][i].y * h);
	}

	
	// Fill interion -------------------------------------------
	//
	if (HasBrush()) {
		if (Topology(TRIANGLE)) {

			WORD aV = vI;

			SkpVtxIC(Vtx[vI++], (x1 + x0) / 2, (y1 + y0) / 2, brushcolor);

			for (WORD i = 0; i < n; i++) SkpVtxIC(Vtx[vI++], pts[i].x, pts[i].y, brushcolor);
			for (WORD i = 0; i < n; i++) {
				Idx[iI++] = aV;
				Idx[iI++] = aV + i + 1;
				Idx[iI++] = aV + i + 2;
			}
			Idx[iI - 1] = aV + 1;
		}
	}

	// Draw outline ------------------------------------------
	//
	if (HasPen()) AppendLineVertexList<IVECTOR2>(pts, n, true);
}


// ===============================================================================================
//
void D3D9Pad::Polygon (const IVECTOR2 *pt, int npt)
{
#ifdef SKPDBG 
	Log("Polygon(%d)", npt);
#endif

	if (npt<3) return;
	if (HasBrush() && npt > 64) return;


	// Create filled polygon interior -----------------------------------------
	//
	if (HasBrush()) {
		if (Topology(TRIANGLE)) {

			int sIdx = vI;

			// File a vertex buffer.
			for (int i = 0; i < npt; i++) SkpVtxIC(Vtx[vI++], pt[i].x, pt[i].y, brushcolor);

			WORD qIdx[256];
			int nIdx = CreatePolyIndexList<IVECTOR2>(pt, npt, qIdx);

			// Add indices to index buffer
			for (int i = 0; i < nIdx; i++) Idx[iI++] = qIdx[i] + sIdx;
		}
	}

	// Draw outline ------------------------------------------
	//
	if (HasPen()) AppendLineVertexList<IVECTOR2>(pt, npt, true);
}


// ===============================================================================================
//
void D3D9Pad::Polyline (const IVECTOR2 *pt, int npt)
{
#ifdef SKPDBG 
	Log("Polyline(%d)", npt);
#endif
	if (npt < 2) return;
	if (HasPen()) AppendLineVertexList<IVECTOR2>(pt, npt, false);
}


// ===============================================================================================
//
void D3D9Pad::DrawPoly (HPOLY hPoly, DWORD flags)
{
#ifdef SKPDBG 
	Log("DrawPoly(%s, 0x%X)", _PTR(hPoly), flags);
#endif

	if (hPoly) {

		D3D9PolyBase *pBase = static_cast<D3D9PolyBase *>(hPoly);
		int PolyType = pBase->type;

		if ((PolyType == 0) && !HasPen()) return;

		// Flush pending graphics before a use of different interface 
		Flush();

		if (Topology(TRIANGLE)) {

			// Flush the poly object
			Flush(hPoly);
		}
	}
}


// ===============================================================================================
//
void D3D9Pad::Lines(const FVECTOR2 *pt, int nlines)
{
#ifdef SKPDBG 
	Log("Lines(%d)", nlines);
#endif
	if (!HasPen()) return;
	for (int i = 0; i < nlines; i++) {
		AppendLineVertexList<FVECTOR2>(pt);
		pt += 2;
	}
}




// -----------------------------------------------------------------------------------------------
// Save buffer helpers (null-terminated string for Text & TextBox)
// -----------------------------------------------------------------------------------------------

// ===============================================================================================
// Copy string to internal 'save' buffer, so it can be changed (adding terminating zeroes, etc.)
void D3D9Pad::ToSaveBuffer (const char *str, int len)
{
	if (_saveBufferSize < len)
	{ // re-allloc bigger space
		if (_saveBuffer) { delete[] _saveBuffer; }
		_saveBuffer = new char[len + 1];
		_saveBufferSize = len;
	}
	strncpy_s(_saveBuffer, len + 1, str, len);
	_isSaveBuffer = true;
}

// ===============================================================================================
//
void D3D9Pad::ReleaseSaveBuffer () {
	_isSaveBuffer = false;
}


// -----------------------------------------------------------------------------------------------
// Subroutines Section
// -----------------------------------------------------------------------------------------------

// ===============================================================================================
//
short mod(short a, short b)
{
	if (a<0) return b-1;
	if (a>=b) return 0;
	return a;
}


// ===============================================================================================
//
template <typename Type>
int CheckTriangle(short x, const Type *pt, const WORD *Idx, float hd, short npt, bool bSharp)
{
	WORD A = Idx[x];
	WORD B = Idx[mod(x-1,npt)];
	WORD C = Idx[mod(x+1,npt)];

	float bx = float(pt[B].x - pt[A].x);
	float by = float(pt[B].y - pt[A].y);
	float ax = float(pt[C].x - pt[A].x);
	float ay = float(pt[C].y - pt[A].y);

	if ((bx*ay-by*ax)*hd > 0.0f) return 0;	// Check handiness

	float aa = ax*ax + ay*ay;			// dot(a,a)
	float ab = ax*bx + ay*by;			// dot(a,b)
	float bb = bx*bx + by*by;			// dot(b,b)

	float qw = fabs(ab) / sqrt(aa*bb);	// abs(cos(a,b))
	if (bSharp && qw>0.9f) return 0;	// Bad Ear

	float id = 1.0f / (aa * bb - ab * ab);

	for (int i=0;i<npt;i++) {

		WORD P = Idx[i];

		if ((P==B) || (P==A) || (P==C)) continue;

		float cx = float(pt[P].x - pt[A].x);
		float cy = float(pt[P].y - pt[A].y);
		float ac = ax*cx + ay*cy;
		float bc = bx*cx + by*cy;
		float u  = (bb*ac - ab*bc) * id;
		float v  = (aa*bc - ab*ac) * id;

		// Check if the point is inside the triangle
		// NOTE: Having u+v slightly above 1.0 is a bad condition, should find a better ear.
		if  ((u>-0.0001) && (v>-0.0001) && ((u+v)<1.0001f)) return 0;
	}

	return 1; // It's an ear
}


// ===============================================================================================
//
template <typename Type>
int CreatePolyIndexList(const Type *pt, short npt, WORD *Out)
{
	if (npt > 255) return 0;
	if (npt==3) { Out[0]=0; Out[1]=1; Out[2]=2;	return 3; }

	short idx = 0;		// Number of indices written in the output
	short x = npt-1;	// First ear to test is the last one in the list
	bool bSharp = false;// Avoid sharp ears

	// Build initial index list
	WORD In[256];
	for (int i=0;i<npt;i++) In[i]=i;
	float sum = 0;
	int k = npt-1;
	int nr = 0;
	for (int i=0;i<k;i++) sum += (float(pt[i].x)*float(pt[(i+1)%k].y) - float(pt[(i+1)%k].x)*float(pt[i].y));

	if (sum>0) sum=1.0; else sum=-1.0;

	while (npt>3) {

		switch (CheckTriangle<Type>(x, pt, In, sum, npt, bSharp)) {

			case 0:
			{
				x--;
				if (x<0) { // Restart
					if (!bSharp && nr>10) return idx;	
					bSharp = false;
					x = npt - 1;
					nr++;
				}
				break;
			}

			case 1:
			{
				Out[idx] = In[mod(x-1,npt)]; idx++;
				Out[idx] = In[mod(x,npt)]; idx++;
				Out[idx] = In[mod(x+1,npt)]; idx++;
				npt--;
				for (int i=x;i<npt;i++) In[i]=In[i+1];
				x = mod(x-1,npt);
				break;
			}
		}
	}

	Out[idx] = In[0]; idx++;
	Out[idx] = In[1]; idx++;
	Out[idx] = In[2]; idx++;

	return idx;
}


// ===============================================================================================
//
inline D3DXVECTOR2 _DXV2(const IVECTOR2 &pt)
{
	return D3DXVECTOR2(float(pt.x), float(pt.y));
}

inline D3DXVECTOR2 _DXV2(const FVECTOR2 &pt)
{
	return D3DXVECTOR2(pt.x, pt.y);
}


// ===============================================================================================
//
template <typename Type>
void D3D9Pad::AppendLineVertexList(const Type *pt, int _npt, bool bLoop)
{
	if (_npt < 2) return;


	// ----------------------------------------------------------------------
	// Draw a thin hairline
	// ----------------------------------------------------------------------

	if (Topology(LINE)) {

		WORD npt = WORD(_npt);
		WORD wL = vI;
		WORD li = WORD(npt - 1);
		WORD aV;
		float length = 0.0f;

		// Create line segments -------------------------------------------------
		//
		for (WORD i = 0; i<npt; i++) {

			Vtx[vI].x = float(pt[i].x);
			Vtx[vI].y = float(pt[i].y);
			Vtx[vI].l = length;
			Vtx[vI].fnc = SKPSW_CENTER | SKPSW_FRAGMENT;
			Vtx[vI].clr = pencolor.dclr;

			if (IsDashed() && i!=li) {
				float x = float(pt[i].x - pt[i+1].x);
				float y = float(pt[i].y - pt[i+1].y);
				length += sqrt(x*x + y*y);
			}
			vI++;
		}
		aV = wL;
		for (WORD i = 0; i < (npt-1); i++) {
			Idx[iI++] = aV;
			aV++;
			Idx[iI++] = aV;
		}

		// Last segment ---------------------------------------------------------
		//
		if (bLoop) {
			Idx[iI++] = vI - 1;
			Idx[iI++] = wL;
		}

		return;
	}



	// ----------------------------------------------------------------------
	// Wide line mode
	// ----------------------------------------------------------------------

	if (Topology(TRIANGLE)) {

		WORD npt = WORD(_npt);
		WORD wL = vI;
		WORD li = WORD(npt - 1);
		WORD aV, bV, cV, dV;
		float length = 0.0f;

		D3DXVECTOR2 pp; // Prev point
		D3DXVECTOR2 np;	// Next point

		// Line Init ------------------------------------------------------------
		//
		if (bLoop) pp = _DXV2(pt[npt - 1]);
		else	   pp = _DXV2(pt[0]) * 2.0 - _DXV2(pt[1]);

		// Create line segments -------------------------------------------------
		//
		for (WORD i = 0; i < npt; i++) {

			if (i != li)	np = _DXV2(pt[i + 1]);
			else {
				if (bLoop)	np = _DXV2(pt[0]);
				else		np = _DXV2(pt[i]) * 2.0 - _DXV2(pt[i - 1]);
			}

			WORD vII = vI + 1;

			// --------------------------------------
			Vtx[vI].x = Vtx[vII].x = float(pt[i].x);
			Vtx[vI].y = Vtx[vII].y = float(pt[i].y);
			Vtx[vI].nx = Vtx[vII].nx = np.x;
			Vtx[vI].ny = Vtx[vII].ny = np.y;
			Vtx[vI].px = Vtx[vII].px = pp.x;
			Vtx[vI].py = Vtx[vII].py = pp.y;
			Vtx[vI].l = Vtx[vII].l = length;
			Vtx[vI].clr = Vtx[vII].clr = pencolor.dclr;
			// --------------------------------------

			Vtx[vI].fnc = SKPSW_WIDEPEN_L | SKPSW_FRAGMENT;
			aV = vI; vI++;
			Vtx[vI].fnc = SKPSW_WIDEPEN_R | SKPSW_FRAGMENT;
			bV = vI; vI++;
			// --------------------------------------

			if (i > 0) {
				Idx[iI++] = cV;	Idx[iI++] = aV;
				Idx[iI++] = dV;	Idx[iI++] = dV;
				Idx[iI++] = aV;	Idx[iI++] = bV;
			}

			cV = aV;
			dV = bV;

			pp = _DXV2(pt[i]);

			if (IsDashed()) length += D3DXVec2Length(ptr(np - pp));
		}

		// Last segment ---------------------------------------------------------
		//
		if (bLoop) {
			Idx[iI++] = wL;		Idx[iI++] = aV;
			Idx[iI++] = wL + 1;	Idx[iI++] = wL + 1;
			Idx[iI++] = aV;		Idx[iI++] = bV;
		}
	}
}


// ===============================================================================================
//
template <typename Type>
void D3D9Pad::AppendLineVertexList(const Type *pt)
{

	// ----------------------------------------------------------------------
	// Draw a thin hairline
	// ----------------------------------------------------------------------

	if (Topology(LINE)) {

		Vtx[vI].x = float(pt[0].x);
		Vtx[vI].y = float(pt[0].y);
		Vtx[vI].fnc = SKPSW_CENTER | SKPSW_FRAGMENT;
		Vtx[vI].l = 0.0f;
		Vtx[vI].clr = pencolor.dclr;
		Idx[iI++] = vI;
		vI++;

		Vtx[vI].x = float(pt[1].x);
		Vtx[vI].y = float(pt[1].y);
		Vtx[vI].px = float(pt[0].x);
		Vtx[vI].py = float(pt[0].y);
		Vtx[vI].fnc = SKPSW_CENTER | SKPSW_LENGTH | SKPSW_FRAGMENT;
		Vtx[vI].clr = pencolor.dclr;
		Idx[iI++] = vI;
		vI++;

		return;
	}


	// ----------------------------------------------------------------------
	// Wide line mode
	// ----------------------------------------------------------------------

	if (Topology(TRIANGLE)) {

		D3DXVECTOR2  pp = _DXV2(pt[0]) * 2.0 - _DXV2(pt[1]);
		D3DXVECTOR2  np;

		WORD vF = vI;

		for (int i = 0; i < 2; i++) {

			if (i == 0) np = _DXV2(pt[1]);
			else np = _DXV2(pt[1]) * 2.0 - _DXV2(pt[0]);

			WORD vII = vI + 1;

			// --------------------------------------
			Vtx[vI].x = Vtx[vII].x = float(pt[i].x);
			Vtx[vI].y = Vtx[vII].y = float(pt[i].y);
			Vtx[vI].nx = Vtx[vII].nx = np.x;
			Vtx[vI].ny = Vtx[vII].ny = np.y;
			Vtx[vI].px = Vtx[vII].px = pp.x;
			Vtx[vI].py = Vtx[vII].py = pp.y;
			Vtx[vI].l = Vtx[vII].l = 0.0f;
			Vtx[vI].clr = Vtx[vII].clr = pencolor.dclr;
			// --------------------------------------
			Vtx[vI].fnc = SKPSW_WIDEPEN_L | SKPSW_FRAGMENT;
			if (i) Vtx[vI].fnc |= SKPSW_LENGTH;
			vI++;
			Vtx[vI].fnc = SKPSW_WIDEPEN_R | SKPSW_FRAGMENT;
			if (i) Vtx[vI].fnc |= SKPSW_LENGTH;
			vI++;
			// --------------------------------------

			pp = _DXV2(pt[i]);
		}

		Idx[iI++] = vF + 0;
		Idx[iI++] = vF + 1;
		Idx[iI++] = vF + 2;
		Idx[iI++] = vF + 1;
		Idx[iI++] = vF + 3;
		Idx[iI++] = vF + 2;
	}
}


// ===============================================================================================
//
D3DXHANDLE   D3D9Pad::eSketch = 0;
D3DXHANDLE   D3D9Pad::eDrawMesh = 0;
D3DXHANDLE   D3D9Pad::eVP = 0;
D3DXHANDLE   D3D9Pad::eW = 0;
D3DXHANDLE   D3D9Pad::eKey = 0;
D3DXHANDLE   D3D9Pad::ePen = 0;
D3DXHANDLE   D3D9Pad::eWVP = 0;
D3DXHANDLE   D3D9Pad::eFov = 0;
D3DXHANDLE   D3D9Pad::eRandom = 0;
D3DXHANDLE   D3D9Pad::eTarget = 0;
D3DXHANDLE   D3D9Pad::eTexEn = 0;
D3DXHANDLE   D3D9Pad::eFntEn = 0;
D3DXHANDLE   D3D9Pad::eKeyEn = 0;
D3DXHANDLE   D3D9Pad::eWidth = 0;
D3DXHANDLE   D3D9Pad::eTex0 = 0;
D3DXHANDLE   D3D9Pad::eFnt0 = 0;
D3DXHANDLE   D3D9Pad::eDashEn = 0;
D3DXHANDLE   D3D9Pad::eSize = 0;
D3DXHANDLE   D3D9Pad::eWide = 0;
D3DXHANDLE   D3D9Pad::eMtrl = 0;
D3DXHANDLE   D3D9Pad::eShade = 0;
D3DXHANDLE   D3D9Pad::ePos = 0;
D3DXHANDLE   D3D9Pad::ePos2 = 0;
D3DXHANDLE   D3D9Pad::eCov = 0;
D3DXHANDLE   D3D9Pad::eCovEn = 0;
D3DXHANDLE   D3D9Pad::eClearEn = 0;
D3DXHANDLE   D3D9Pad::eEffectsEn = 0;

D3DXHANDLE	 D3D9Pad::eNoiseTex = 0;
D3DXHANDLE   D3D9Pad::eNoiseColor = 0;
D3DXHANDLE   D3D9Pad::eColorMatrix = 0;
D3DXHANDLE   D3D9Pad::eGamma = 0;

ID3DXEffect* D3D9Pad::FX = 0;
D3D9Client * D3D9Pad::gc = 0;
WORD * D3D9Pad::Idx = 0;
SkpVtx * D3D9Pad::Vtx = 0;
LPD3DXVECTOR2 D3D9Pad::pSinCos[];
LPDIRECT3DDEVICE9 D3D9PadFont::pDev = 0;
LPDIRECT3DDEVICE9 D3D9Pad::pDev = 0;
LPDIRECT3DTEXTURE9 D3D9Pad::pNoise = 0;

FILE* D3D9Pad::log = 0;
CRITICAL_SECTION D3D9Pad::LogCrit;


// ======================================================================
// class GDIFont
// ======================================================================
using namespace oapi;

D3D9PadFont::D3D9PadFont(int height, bool prop, const char *face, FontStyle style, int orientation, DWORD flags) : Font(height, prop, face, style, orientation)
{
	const char *def_fixedface = "Courier New";
	const char *def_sansface = "Arial";
	const char *def_serifface = "Times New Roman";

	if (face[0]!='*') {
		if (!_stricmp (face, "fixed")) face = def_fixedface;
		else if (!_stricmp (face, "sans")) face = def_sansface;
		else if (!_stricmp (face, "serif")) face = def_serifface;
		else if (_stricmp (face, def_fixedface) && _stricmp (face, def_sansface) && _stricmp (face, def_serifface)) face = (prop ? def_sansface : def_fixedface);
	}
	else face++;

	hFont = NULL;

	if (orientation!=0) rotation = float(orientation) * 0.1f;
	else                rotation = 0.0f;

	// Browse cache ---------------------------------------------------
	//

	for (size_t i = 0; i < fcache.size(); ++i) {
		if (fcache[i]->height!=height) continue;
		if (fcache[i]->style!=style) continue;
		if (fcache[i]->prop!=prop) continue;
		if (_stricmp(fcache[i]->face,face)!=0) continue;
		pFont = fcache[i]->pFont;
		break;
	}

	int weight = (style & FONT_BOLD) ? FW_BOLD : FW_NORMAL;
	DWORD italic = (style & FONT_ITALIC) ? TRUE : FALSE;
	DWORD underline = (style & FONT_UNDERLINE) ? TRUE : FALSE;
	DWORD strikeout = (style & FONT_STRIKEOUT) ? TRUE : FALSE;

	Quality = NONANTIALIASED_QUALITY;

	if ((flags & 0xF) == 0) {
		if (Config->SketchpadFont == 1) Quality = PROOF_QUALITY;
		if (Config->SketchpadFont == 2) Quality = CLEARTYPE_QUALITY;
	}
	else {
		if (flags&SKP_FONT_ANTIALIAS) Quality = PROOF_QUALITY;
		if (flags&SKP_FONT_CLEARTYPE) Quality = CLEARTYPE_QUALITY;
	}

	// Create DirectX accelerated font for a use with D3D9Pad ------------------
	//
	if (pFont==NULL) {

		HFONT hNew = CreateFont(height, 0, 0, 0, weight, italic, underline, strikeout, 0, 0, 2, Quality, 49, face);

		pFont = std::make_shared<D3D9Text>(pDev);
		pFont->Init(hNew);

		DeleteObject(hNew);

		pFont->SetRotation(rotation);

		// Fill the cache --------------------------------
		FontCache *p = new FontCache();
		p->pFont  = pFont;
		p->height = height;
		p->style  = style;
		p->prop   = prop;
		strcpy_s(p->face, 64, face);
		fcache.push_back(p);
	}

	// Create Rotated windows GDI Font for a use with GDIPad ---------------------------
	//
	hFont = CreateFontA(height, 0, orientation, orientation, weight, italic, underline, strikeout, 0, 0, 2, Quality, 49, face);

	if (hFont==NULL) {
		face  = (prop ? def_sansface : def_fixedface);
		hFont = CreateFont(height, 0, orientation, orientation, weight, italic, underline, strikeout, 0, 0, 2, Quality, 49, face);
	}
}



D3D9PadFont::D3D9PadFont(int height, char *face, int width, int weight, FontStyle style, float spacing)
: Font(height, false, face, style, 0)
{

	hFont = NULL;
	rotation = 0.0f;

	// Browse cache ---------------------------------------------------
	//
	for (size_t i = 0; i < qcache.size(); ++i) {
		if (qcache[i]->height != height) continue;
		if (qcache[i]->style != style) continue;
		if (qcache[i]->width != width) continue;
		if (qcache[i]->weight != weight) continue;
		if (qcache[i]->spacing != spacing) continue;
		if (_stricmp(qcache[i]->face, face) != 0) continue;
		pFont = qcache[i]->pFont;
		break;
	}

	DWORD italic = (style & FONT_ITALIC) ? TRUE : FALSE;
	DWORD underline = (style & FONT_UNDERLINE) ? TRUE : FALSE;
	DWORD strikeout = (style & FONT_STRIKEOUT) ? TRUE : FALSE;

	Quality = NONANTIALIASED_QUALITY;
	if (Config->SketchpadFont == 1) Quality = ANTIALIASED_QUALITY;
	if (Config->SketchpadFont == 2) Quality = PROOF_QUALITY;
	
	if (style & FONT_CRISP) Quality = NONANTIALIASED_QUALITY;
	if (style & FONT_ANTIALIAS) Quality = ANTIALIASED_QUALITY;
	
	
	// Create DirectX accelerated font for a use with D3D9Pad ------------------
	//
	if (pFont == NULL) {

		hFont = CreateFont(height, width, 0, 0, weight, italic, underline, strikeout, 0, 0, 2, Quality, 49, face);

		pFont = std::make_shared<D3D9Text>(pDev);
		pFont->Init(hFont);
		pFont->SetRotation(0.0f);
		pFont->SetTextSpace(spacing);

		// Fill the cache --------------------------------
		QFontCache *p = new QFontCache();
		p->pFont = pFont;
		p->height = height;
		p->width = width;
		p->weight = weight;
		p->style = style;
		p->spacing = spacing;
		strcpy_s(p->face, 64, face);
		qcache.push_back(p);
	}
	else {
		// Create windows GDI Font for a use with GDIPad ---------------------------
		//
		hFont = CreateFont(height, width, 0, 0, weight, italic, underline, strikeout, 0, 0, 2, Quality, 49, face);
	}
}

// -----------------------------------------------------------------------------------------------
//
D3D9PadFont::~D3D9PadFont ()
{
	if (pFont) pFont->SetRotation(0.0f), pFont.reset();
	if (hFont) DeleteObject(hFont);
}


// -----------------------------------------------------------------------------------------------
//
HFONT D3D9PadFont::GetGDIFont () const
{
	return hFont;
}


// -----------------------------------------------------------------------------------------------
//
int D3D9PadFont::GetTextLength(const char *pText, int len) const
{
	return int(pFont->Length2(pText, len));
}


// -----------------------------------------------------------------------------------------------
//
int D3D9PadFont::GetIndexByPosition(const char *pText, int pos, int len) const
{
	return int(pFont->GetIndex(pText, float(pos), len));
}

// -----------------------------------------------------------------------------------------------
//
void D3D9PadFont::D3D9TechInit(LPDIRECT3DDEVICE9 pDevice)
{
	pDev = pDevice;
}



// ======================================================================
// class GDIPen
// ======================================================================

D3D9PadPen::D3D9PadPen (int s, int w, DWORD col): oapi::Pen (style, width, col)
{
	switch (s) {
		case 0:  style = PS_NULL;  break;
		case 2:  style = PS_DOT;   break;
		default: style = PS_SOLID; break;
	}
	width = w;
	if (width<1) width = 1;
	hPen = CreatePen(style, width, COLORREF(col&0xFFFFFF));
	clr = SkpColor(col);
}

// -----------------------------------------------------------------------------------------------
//
D3D9PadPen::~D3D9PadPen ()
{
	DeleteObject(hPen);
}

// -----------------------------------------------------------------------------------------------
//
void D3D9PadPen::D3D9TechInit(LPDIRECT3DDEVICE9 pDevice)
{
	//pDev = pDevice;
}



// ======================================================================
// class GDIBrush
// ======================================================================

D3D9PadBrush::D3D9PadBrush (DWORD col): oapi::Brush (col)
{
	hBrush = CreateSolidBrush(COLORREF(col&0xFFFFFF));
	clr = SkpColor(col);
}

// -----------------------------------------------------------------------------------------------
//
D3D9PadBrush::~D3D9PadBrush ()
{
	DeleteObject(hBrush);
}

// -----------------------------------------------------------------------------------------------
//
void D3D9PadBrush::D3D9TechInit(LPDIRECT3DDEVICE9 pDevice)
{
	//pDev = pDevice;
}


// ===================================================
// Copyright (C) 2012-2021 Jarmo Nikkanen
// licensed under LGPL v2
// ===================================================

#include "D3D9Pad.h"
#include "D3D9TextMgr.h"
#include "D3D9Surface.h"
#include "Scene.h"
#include "Mesh.h"
#include <d3dx9.h>
#include <sstream>




// ===============================================================================================
// Sketchpad2 Interface
// ===============================================================================================

// ===============================================================================================
//
void D3D9Pad::GetRenderSurfaceSize(LPSIZE size)
{
	size->cx = tgt_desc.Width;
	size->cy = tgt_desc.Height;
}


// ===============================================================================================
//
void D3D9Pad::QuickPen(DWORD color, float width, DWORD style)
{
#ifdef SKPDBG 
	Log("QuickPen(0x%X, %d, %u)", color, width, style);
#endif

	if (QPen.bEnabled) {
		if ((QPen.style == style) && (QPen.width == width) && (QPen.color == color)) return;
	}

	Change |= SKPCHG_PEN;

	cpen = NULL;
	if (color == 0) QPen.bEnabled = false;
	else QPen.bEnabled = true;
	QPen.style = style;
	QPen.width = width;
	QPen.color = color;
	pencolor = SkpColor(ColorComp(color));

	IsLineTopologyAllowed();
}


// ===============================================================================================
//
void D3D9Pad::QuickBrush(DWORD color)
{
#ifdef SKPDBG 
	Log("QuickBrush(0x%X)", color);
#endif

	// No Change flags here
	cbrush = NULL;
	if (color == 0) QBrush.bEnabled = false;
	else QBrush.bEnabled = true;
	brushcolor = SkpColor(ColorComp(color));

	IsLineTopologyAllowed();
}


// ===============================================================================================
//
void D3D9Pad::AddRectIdx(WORD aV)
{
	Idx[iI++] = aV;
	Idx[iI++] = aV + 1;
	Idx[iI++] = aV + 2;
	Idx[iI++] = aV;
	Idx[iI++] = aV + 2;
	Idx[iI++] = aV + 3;
}


// ===============================================================================================
//
RECT D3D9Pad::GetFullRect(SURFHANDLE hSrc)
{
	return {0, 0, static_cast<long>(SURFACE(hSrc)->GetWidth()), static_cast<long>(SURFACE(hSrc)->GetHeight())};
}


// ===============================================================================================
//
void D3D9Pad::CopyRect(const SURFHANDLE hSrc, const LPRECT _s, int tx, int ty)
{
#ifdef SKPDBG 
	Log("CopyRect(0x%X)", DWORD(hSrc));
#endif

	TexChange(hSrc);

	if (Topology(TRIANGLE)) {

		auto s = _s ? *_s : GetFullRect(hSrc);

		int h = std::abs(s.bottom - s.top);
		int w = std::abs(s.right - s.left);

		AddRectIdx(vI);

		SkpVtxII(Vtx[vI++], tx    , ty    , s.left , s.top   );
		SkpVtxII(Vtx[vI++], tx    , ty + h, s.left , s.bottom);
		SkpVtxII(Vtx[vI++], tx + w, ty + h, s.right, s.bottom);
		SkpVtxII(Vtx[vI++], tx + w, ty    , s.right, s.top   );

		DWORD x = SKPSW_TEXTURE | SKPSW_CENTER;

		Vtx[vI - 1].fnc = x;
		Vtx[vI - 2].fnc = x;
		Vtx[vI - 3].fnc = x;
		Vtx[vI - 4].fnc = x;
	}
}


// ===============================================================================================
//
void D3D9Pad::StretchRect(const SURFHANDLE hSrc, const LPRECT _s, const LPRECT _t)
{
#ifdef SKPDBG 
	Log("StretchRect(0x%X)", DWORD(hSrc));
#endif

	TexChange(hSrc);

	if (Topology(TRIANGLE)) {

		auto s = _s ? *_s : GetFullRect(hSrc);
		auto t = _t ? *_t : tgt;

		AddRectIdx(vI);

		SkpVtxII(Vtx[vI++], t.left , t.top   , s.left , s.top   );
		SkpVtxII(Vtx[vI++], t.left , t.bottom, s.left , s.bottom);
		SkpVtxII(Vtx[vI++], t.right, t.bottom, s.right, s.bottom);
		SkpVtxII(Vtx[vI++], t.right, t.top   , s.right, s.top   );

		DWORD x = SKPSW_TEXTURE | SKPSW_CENTER;

		Vtx[vI - 1].fnc = x;
		Vtx[vI - 2].fnc = x;
		Vtx[vI - 3].fnc = x;
		Vtx[vI - 4].fnc = x;
	}
}


// ===============================================================================================
//
void D3D9Pad::RotateRect(const SURFHANDLE hSrc, const LPRECT _s, int tcx, int tcy, float angle, float sw, float sh)
{
#ifdef SKPDBG 
	Log("RotateRect(0x%X)", DWORD(hSrc));
#endif

	TexChange(hSrc);

	if (Topology(TRIANGLE)) {

		auto s = _s ? *_s : GetFullRect(hSrc);

		float w = float(s.right - s.left) * sw;
		float h = float(s.bottom - s.top) * sh;

		float san = sin(angle) * 0.5f;
		float can = cos(angle) * 0.5f;

		float ax = float(tcx) + (-w * can + h * san);
		float ay = float(tcy) + (-w * san - h * can);

		float bx = float(tcx) + (-w * can - h * san);
		float by = float(tcy) + (-w * san + h * can);

		float cx = float(tcx) + (+w * can - h * san);
		float cy = float(tcy) + (+w * san + h * can);

		float dx = float(tcx) + (+w * can + h * san);
		float dy = float(tcy) + (+w * san - h * can);

		AddRectIdx(vI);

		SkpVtxFI(Vtx[vI++], ax, ay, s.left , s.top   );
		SkpVtxFI(Vtx[vI++], bx, by, s.left , s.bottom);
		SkpVtxFI(Vtx[vI++], cx, cy, s.right, s.bottom);
		SkpVtxFI(Vtx[vI++], dx, dy, s.right, s.top   );

		DWORD x = SKPSW_TEXTURE | SKPSW_CENTER;

		Vtx[vI - 1].fnc = x;
		Vtx[vI - 2].fnc = x;
		Vtx[vI - 3].fnc = x;
		Vtx[vI - 4].fnc = x;
	}
}


// ===============================================================================================
//
void D3D9Pad::ColorKey(const SURFHANDLE hSrc, const LPRECT _s, int tx, int ty)
{
#ifdef SKPDBG 
	Log("ColorKey(0x%X)", DWORD(hSrc));
#endif

	TexChange(hSrc);

	if (Topology(TRIANGLE)) {

		auto s = _s ? *_s : GetFullRect(hSrc);

		int h = std::abs(s.bottom - s.top);
		int w = std::abs(s.right - s.left);

		AddRectIdx(vI);

		SkpVtxII(Vtx[vI++], tx    , ty    , s.left , s.top   );
		SkpVtxII(Vtx[vI++], tx    , ty + h, s.left , s.bottom);
		SkpVtxII(Vtx[vI++], tx + w, ty + h, s.right, s.bottom);
		SkpVtxII(Vtx[vI++], tx + w, ty    , s.right, s.top   );

		DWORD f = SKPSW_TEXTURE | SKPSW_COLORKEY | SKPSW_CENTER;

		Vtx[vI - 1].fnc = f;
		Vtx[vI - 2].fnc = f;
		Vtx[vI - 3].fnc = f;
		Vtx[vI - 4].fnc = f;
	}
}


// ===============================================================================================
//
void D3D9Pad::ColorKeyStretch(const SURFHANDLE hSrc, const LPRECT _s, const LPRECT _t)
{
#ifdef SKPDBG 
	Log("StretchRect(0x%X)", DWORD(hSrc));
#endif

	TexChange(hSrc);

	DWORD dwBak = dwBlendState;

	SetBlendState(BlendState((dwBak & 0xF) | BlendState::FILTER_POINT));

	if (Topology(TRIANGLE)) {

		auto s = _s ? *_s : GetFullRect(hSrc);
		auto t = _t ? *_t : tgt;

		AddRectIdx(vI);

		SkpVtxII(Vtx[vI++], t.left , t.top   , s.left , s.top   );
		SkpVtxII(Vtx[vI++], t.left , t.bottom, s.left , s.bottom);
		SkpVtxII(Vtx[vI++], t.right, t.bottom, s.right, s.bottom);
		SkpVtxII(Vtx[vI++], t.right, t.top   , s.right, s.top   );

		DWORD x = SKPSW_TEXTURE | SKPSW_COLORKEY | SKPSW_CENTER;

		Vtx[vI - 1].fnc = x;
		Vtx[vI - 2].fnc = x;
		Vtx[vI - 3].fnc = x;
		Vtx[vI - 4].fnc = x;
	}

	SetBlendState(BlendState(dwBak));
}


// ===============================================================================================
//
void D3D9Pad::CopyRectNative(const LPDIRECT3DTEXTURE9 pSrc, const LPRECT _s, int tx, int ty)
{
#ifdef SKPDBG 
	Log("CopyRectNative(0x%X)", DWORD(pSrc));
#endif

	TexChangeNative(pSrc);

	if (Topology(TRIANGLE)) {

		auto s = _s ? *_s : GetFullRectNative(pSrc);

		int h = std::abs(s.bottom - s.top);
		int w = std::abs(s.right - s.left);

		AddRectIdx(vI);

		SkpVtxII(Vtx[vI++], tx    , ty    , s.left     , s.top       );
		SkpVtxII(Vtx[vI++], tx    , ty + h, s.left     , s.bottom - 1);
		SkpVtxII(Vtx[vI++], tx + w, ty + h, s.right - 1, s.bottom - 1);
		SkpVtxII(Vtx[vI++], tx + w, ty    , s.right - 1, s.top       );

		DWORD x = SKPSW_TEXTURE | SKPSW_CENTER;

		Vtx[vI - 1].fnc = x;
		Vtx[vI - 2].fnc = x;
		Vtx[vI - 3].fnc = x;
		Vtx[vI - 4].fnc = x;
	}
}


// ===============================================================================================
//
void D3D9Pad::StretchRectNative(const LPDIRECT3DTEXTURE9 pSrc, const RECT *_s, const RECT *_t)
{
#ifdef SKPDBG 
	Log("StretchRectNative(0x%X)", DWORD(pSrc));
#endif

	TexChangeNative(pSrc);

	if (Topology(TRIANGLE)) {

		auto s = _s ? *_s : GetFullRectNative(pSrc);
		auto t = _t ? *_t : tgt;

		AddRectIdx(vI);

		SkpVtxII(Vtx[vI++], t.left , t.top   , s.left     , s.top       );
		SkpVtxII(Vtx[vI++], t.left , t.bottom, s.left     , s.bottom - 1);
		SkpVtxII(Vtx[vI++], t.right, t.bottom, s.right - 1, s.bottom - 1);
		SkpVtxII(Vtx[vI++], t.right, t.top   , s.right - 1, s.top       );

		DWORD x = SKPSW_TEXTURE | SKPSW_CENTER;

		Vtx[vI - 1].fnc = x;
		Vtx[vI - 2].fnc = x;
		Vtx[vI - 3].fnc = x;
		Vtx[vI - 4].fnc = x;
	}
}


// ===============================================================================================
//
void D3D9Pad::CopyTetragon(const SURFHANDLE hSrc, const LPRECT _s, const FVECTOR2 tp[4])
{
#ifdef SKPDBG 
	Log("CopyTetragon(0x%X)", DWORD(pSrc));
#endif
	FVECTOR2 sp[4];
	FVECTOR2 a, b, c, d;
	static const int n = 6;
	static const float step = 1.0 / float(n - 1);

	DWORD fn = SKPSW_TEXTURE | SKPSW_CENTER;

	TexChange(hSrc);

	if (Topology(TRIANGLE))
	{
		auto s = _s ? *_s : GetFullRect(hSrc);

		sp[0] = FVECTOR2{s.left , s.top   };
		sp[1] = FVECTOR2{s.left , s.bottom};
		sp[2] = FVECTOR2{s.right, s.bottom};
		sp[3] = FVECTOR2{s.right, s.top   };

		// Create indices
		for (int j = 0; j < (n-1); j++)
		{
			for (int i = 0; i < (n-1); i++)
			{
				WORD q = vI + i + j * n;
				Idx[iI++] = q + 0; Idx[iI++] = q + 1; Idx[iI++] = q + (n + 1);
				Idx[iI++] = q + 0; Idx[iI++] = q + (n + 1); Idx[iI++] = q + n;
			}
		}

		// Create grid points
		int j = 0;
		for (int i = 0; i < n; i++)
		{
			float x = float(i) * step;

			a = lerp(tp[0], tp[3], x);
			b = lerp(tp[1], tp[2], x);
			c = lerp(sp[0], sp[3], x);
			d = lerp(sp[1], sp[2], x);

			for (int k = 0; k < n; k++) {
				FVECTOR2 tv = lerp(a, b, float(k) * step);
				FVECTOR2 sv = lerp(c, d, float(k) * step);
				SkpVtxFF(Vtx[vI], tv.x, tv.y, sv.x, sv.y);
				Vtx[vI].fnc = fn;
				vI++;
			}
		}
	}
}


// ===============================================================================================
//
void D3D9Pad::FillTetragon(DWORD c, const FVECTOR2 pt[4])
{
#ifdef SKPDBG 
	Log("CopyTetragon(0x%X)", DWORD(pSrc));
#endif

	DWORD fn = SKPSW_TEXTURE | SKPSW_CENTER;

	if (Topology(TRIANGLE)) {
		AddRectIdx(vI);
		SkpVtxFC(Vtx[vI++], pt[0].x, pt[0].y, c);
		SkpVtxFC(Vtx[vI++], pt[1].x, pt[1].y, c);
		SkpVtxFC(Vtx[vI++], pt[2].x, pt[2].y, c);
		SkpVtxFC(Vtx[vI++], pt[3].x, pt[3].y, c);
	}
}


// ===============================================================================================
//
bool D3D9Pad::TextW (int x, int y, const LPWSTR str, int len)
{
#ifdef SKPDBG 
	Log("TextW()");
#endif

	// No "Setup" here, done in PrintSkp()

	if (!cfont) return false;
	if (len == -1) len = int(wcslen(str));
	if (!len) return true;

	D3D9TextPtr pText = static_cast<const D3D9PadFont *>(cfont)->pFont;

	int lineSpace = pText->GetLineSpace();

	std::wstring _str(str, str + size_t(len));

	std::wistringstream f(_str);
	std::wstring s;
	int _y = y;
	while (getline(f, s, L'\n')) {
		pText->PrintSkp(this, x - 1.0f, _y - 1.0f, s.c_str(), -1);
		_y += lineSpace;
	}

	return true;
}

// ===============================================================================================
//
void D3D9Pad::TextEx(float x, float y, const char *str, float scale, float angle)
{
#ifdef SKPDBG 
	Log("TextEx()");
#endif

	// No "Setup" here, done in PrintSkp()

	if (cfont == NULL) return;

	D3D9TextPtr pText = static_cast<const D3D9PadFont *>(cfont)->pFont;

	switch (tah) {
		default:
		case LEFT:   pText->SetTextHAlign(0); break;
		case CENTER: pText->SetTextHAlign(1); break;
		case RIGHT:  pText->SetTextHAlign(2); break;
	}

	switch (tav) {
		default:
		case TOP:      pText->SetTextVAlign(0); break;
		case BASELINE: pText->SetTextVAlign(1); break;
		case BOTTOM:   pText->SetTextVAlign(2); break;
	}

	pText->SetRotation(angle);
	pText->SetScaling(scale);
	pText->PrintSkp(this, x - 1.0f, y - 1.0f, str, -1, (bkmode == OPAQUE));
}


// ===============================================================================================
//
void D3D9Pad::ClipRect(const LPRECT clip)
{
#ifdef SKPDBG 
	Log("ClipRect(0x%X)", DWORD(clip));
#endif

	Change |= SKPCHG_CLIPRECT;

	bEnableScissor = (clip != NULL);
	if (clip) ScissorRect = (*clip);
	else ScissorRect = { 0,0,0,0 };
}


// ===============================================================================================
//
void D3D9Pad::Clipper(int idx, const VECTOR3 *uDir, double cos_angle, double dist)
{
#ifdef SKPDBG 
	Log("Clipper()");
#endif

	Change |= SKPCHG_CLIPCONE;

	if (idx < 0) idx = 0;
	if (idx > 1) idx = 1;

	if (uDir) {
		ClipData[idx].uDir = D3DXVEC(*uDir);
		ClipData[idx].ca = float(cos_angle);
		ClipData[idx].dst = float(dist);
		ClipData[idx].bEnable = true;
	}
	else {
		ClipData[idx].uDir = D3DXVECTOR3(0,0,1);
		ClipData[idx].ca = 2.0f;
		ClipData[idx].dst = 0.0f;
		ClipData[idx].bEnable = false;
	}
}


// ===============================================================================================
//
void D3D9Pad::DepthEnable(bool bEnable)
{
#ifdef SKPDBG 
	Log("DepthEnable(%u)", DWORD(bEnable));
#endif

	Flush(); // Must Flush() here before a mode change

	if (pDep) {
		Change |= SKPCHG_DEPTH;
		bDepthEnable = bEnable;
	}
	else bDepthEnable = false;
}


// ===============================================================================================
//
const FMATRIX4 *D3D9Pad::ViewMatrix() const
{
	return (const FMATRIX4*)&mV;
}


// ===============================================================================================
//
const FMATRIX4 *D3D9Pad::ProjectionMatrix() const
{
	return (const FMATRIX4*)&mP;
}


// ===============================================================================================
//
const FMATRIX4 *D3D9Pad::GetViewProjectionMatrix() const
{
	D3DXMatrixMultiply(&mVP, &mV, &mP);
	return (const FMATRIX4 *)&mVP;
}


// ===============================================================================================
//
void D3D9Pad::SetViewMatrix(const FMATRIX4 *pV)
{
#ifdef SKPDBG 
	Log("SetViewMatrix(0x%X)", DWORD(pV));
#endif
	Change |= SKPCHG_TRANSFORM;
	if (pV) memcpy(&mV, pV, sizeof(FMATRIX4));
	else mV = mVOrig;
}


// ===============================================================================================
//
void D3D9Pad::SetProjectionMatrix(const FMATRIX4 *pP)
{
#ifdef SKPDBG 
	Log("SetProjectionMatrix(0x%X)", DWORD(pP));
#endif
	Change |= SKPCHG_TRANSFORM;
	if (pP) memcpy(&mP, pP, sizeof(FMATRIX4));
	else mP = mPOrig;
}


// ===============================================================================================
//
void D3D9Pad::SetViewMode(SkpView mode)
{
#ifdef SKPDBG 
	Log("SetViewMode(0x%X)", DWORD(mode));
#endif
	Flush();	// Must Flush() here before a mode change
	Change |= SKPCHG_TRANSFORM;
	vmode = mode;
}


// ===============================================================================================
// !! For a private use in D3D9Client !!
//
LPD3DXMATRIX D3D9Pad::WorldMatrix()
{
#ifdef SKPDBG 
	Log("WorldMatrix() ! - ! - !");
#endif
	Change |= SKPCHG_TRANSFORM;
	return (LPD3DXMATRIX)&mW;
}


// ===============================================================================================
//
void D3D9Pad::SetWorldTransform2D(float scale, float rot, const IVECTOR2 *c, const IVECTOR2 *t)
{
#ifdef SKPDBG 
	Log("SetWorldTransform2D(%f, %f, 0x%X, 0x%X)", scale, rot, DWORD(c), DWORD(t));
#endif
	Change |= SKPCHG_TRANSFORM;

	D3DXVECTOR2 ctr = D3DXVECTOR2(0, 0);
	D3DXVECTOR2 trl = D3DXVECTOR2(0, 0);

	if (c) ctr = D3DXVECTOR2(float(c->x), float(c->y));
	if (t) trl = D3DXVECTOR2(float(t->x), float(t->y));

	D3DXMatrixAffineTransformation2D(&mW, scale, &ctr, rot, &trl);
}


// ===============================================================================================
//
void D3D9Pad::SetWorldTransform(const FMATRIX4 *pWT)
{
#ifdef SKPDBG 
	Log("SetWorldTransform(0x%X)", DWORD(pWT));
#endif
	Change |= SKPCHG_TRANSFORM;

	if (pWT) memcpy(&mW, pWT, sizeof(FMATRIX4));
	else D3DXMatrixIdentity(&mW);
}


// ===============================================================================================
//
void D3D9Pad::SetWorldBillboard(const FVECTOR3& wpos, float scale, bool bFixed, const FVECTOR3* index)
{
#ifdef SKPDBG 
	Log("SetWorldBillboard()");
#endif
	scale *= (mP._11 + mP._22) * 0.5f;
	Change |= SKPCHG_TRANSFORM;
	FVECTOR3 up = unit(wpos);
	FVECTOR3 y  = unit(cross((index ? *index : FVECTOR3(mV._11, mV._21, mV._31)), up));
	FVECTOR3 x  = cross(up, y);
	float d = (bFixed ? dot(up, wpos) / float(tgt_desc.Width) : 1.0f) * scale;
	FMATRIX4 mWorld;
	mWorld._x.xyz = x * d;
	mWorld._y.xyz = y * d;
	mWorld._z.xyz = up * d;
	mWorld._p.xyz = wpos;
	mWorld.m44 = 1.0f;
	memcpy(&mW, &mWorld, sizeof(FMATRIX4));
}


// ===============================================================================================
//
void D3D9Pad::SetGlobalLineScale(float width, float pat)
{
#ifdef SKPDBG 
	Log("SetGlobalLineScale(%f, %f)", width, pat);
#endif
	Change |= SKPCHG_PEN;
	linescale = width;
	pattern = pat;
}


// ===============================================================================================
//
void D3D9Pad::SetFontTextureNative(LPDIRECT3DTEXTURE9 hNew)
{
	if (hNew == hFontTex) return;
	Change |= SKPCHG_FONT;
	hFontTex = hNew;
}


// ===============================================================================================
//
bool D3D9Pad::TexChangeNative(LPDIRECT3DTEXTURE9 hNew)
{
	if (hNew == hTexture) return false;
	Change |= SKPCHG_TEXTURE;
	hTexture = hNew;
	return true;
}


// ===============================================================================================
//
void D3D9Pad::TexChange(SURFHANDLE hNew)
{
	if (!SURFACE(hNew)->IsTexture()) {
		LogErr("Sketchpad2: Source is not a texture");
		HALT();
		return;
	}

	TexChangeNative(SURFACE(hNew)->GetTexture());

	if (SURFACE(hNew)->IsColorKeyEnabled()) {
		bColorKey = true;
		cColorKey = D3DXCOLOR(SURFACE(hNew)->ColorKey);
	}
	else {
		bColorKey = false;
		cColorKey = D3DXCOLOR(DWORD(0));
	}
}


// ===============================================================================================
//
int D3D9Pad::DrawMeshGroup(const MESHHANDLE hMesh, DWORD grp, Sketchpad::MeshFlags flags, const SURFHANDLE hTex)
{
#ifdef SKPDBG 
	Log("DrawMeshGroup(0x%X, gpr=%u, flags=0x%X, hTex=0x%X)", DWORD(hMesh), grp, flags, DWORD(hTex));
#endif
	UINT num;
	SketchMesh* pMesh = GetSketchMesh(hMesh);

	if (!pMesh) return -1;

	DWORD nGrp = pMesh->GroupCount();

	if (grp >= nGrp) return -1;

	// Flush Pending graphics ------------------------------------
	//
	SetupDevice(tCurrent);

	// Initialize device for drawing a mesh ----------------------
	//
	pMesh->Init();

	if (flags & Sketchpad::MeshFlags::CULL_NONE) pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	else pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);

	HR(FX->SetTechnique(eDrawMesh));
	HR(FX->Begin(&num, D3DXFX_DONOTSAVESTATE));
	HR(FX->BeginPass(0));

	HR(FX->SetBool(eShade, (flags & Sketchpad::MeshFlags::SMOOTH_SHADE) != 0));
	
	if (flags & Sketchpad::MeshFlags::RENDER_ALL) grp = 0;

	// Draw a mesh group(s) ----------------------------------------
	//
	while (grp < nGrp)
	{
		SURFHANDLE pTex = hTex ? hTex : pMesh->GetTexture(grp);
		D3DXCOLOR   Mat = pMesh->GetMaterial(grp);
	
		if (pTex)
		{
			HR(FX->SetTexture(eTex0, SURFACE(pTex)->GetTexture()));
			HR(FX->SetBool(eTexEn, true));
		}
		else
		{
			HR(FX->SetBool(eTexEn, false));
		}
		
		HR(FX->SetValue(eMtrl, &Mat, sizeof(D3DXCOLOR)));
		HR(FX->CommitChanges());

		pMesh->RenderGroup(grp);

		grp++;

		if (!(flags & Sketchpad::MeshFlags::RENDER_ALL)) break;
	}

	HR(FX->EndPass());
	HR(FX->End());

	return nGrp;
}


// ===============================================================================================
//
RECT D3D9Pad::GetFullRectNative(LPDIRECT3DTEXTURE9 hSrc)
{
	D3DSURFACE_DESC desc;
	hSrc->GetLevelDesc(0, &desc);
	return {0, 0, static_cast<long>(desc.Width), static_cast<long>(desc.Height)};
}



// ======================================================================================
// Polyline Interface
// ======================================================================================



D3D9PolyLine::D3D9PolyLine(LPDIRECT3DDEVICE9 pDev, const FVECTOR2 *pt, int npt, bool bConnect) : D3D9PolyBase(0)
{
	nPt = npt + 2;
	nVtx = 2 * nPt;
	nIdx = 6 * nPt;

	HR(pDev->CreateVertexBuffer(nVtx * sizeof(SkpVtx), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &pVB, NULL));
	HR(pDev->CreateIndexBuffer(nIdx * sizeof(WORD), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &pIB, NULL));

	WORD *Idx = NULL;

	HR(pIB->Lock(0, 0, (LPVOID*)&Idx, D3DLOCK_DISCARD));

	iI = 0;

	for (WORD i=0,p=0;p<nPt;p++) {
		Idx[iI++] = i+0;
		Idx[iI++] = i+1;
		Idx[iI++] = i+2;
		Idx[iI++] = i+1;
		Idx[iI++] = i+3;
		Idx[iI++] = i+2;
		i += 2;
	}

	HR(pIB->Unlock());

	if (pt) Update(pt, npt, bConnect);
}


// ===============================================================================================
//
D3D9PolyLine::~D3D9PolyLine()
{

}


// ===============================================================================================
//
void D3D9PolyLine::Release()
{
	SAFE_RELEASE(pVB);
	SAFE_RELEASE(pIB);
}


// ===============================================================================================
//
void D3D9PolyLine::Draw(D3D9Pad *pSkp, LPDIRECT3DDEVICE9 pDev)
{
	pDev->SetStreamSource(0, pVB, 0, sizeof(SkpVtx));
	pDev->SetIndices(pIB);
	pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, vI, 0, vI-2);
}


// ===============================================================================================
//
void D3D9PolyLine::Update(const FVECTOR2 *_pt, int _npt, bool bConnect)
{
	SkpVtx *Vtx = NULL;
	D3DXVECTOR2 *pt = (D3DXVECTOR2 *)_pt;

	HR(pVB->Lock(0, 0, (LPVOID*)&Vtx, D3DLOCK_DISCARD));

	WORD npt = WORD(_npt);
	WORD li = WORD(npt - 1);

	vI = 0;
	float length = 0.0f;

	D3DXVECTOR2 pp; // Prev point
	D3DXVECTOR2 np;	// Next point

	bLoop = bConnect;

	// Line Init ------------------------------------------------------------
	//
	if (bLoop) pp = pt[li];
	else	   pp = pt[0] * 2.0 - pt[1];

	// Create line segments -------------------------------------------------
	//
	for (WORD i = 0; i<npt; i++) {

		if (i != li)	np = pt[i + 1];
		else {
			if (bLoop)	np = pt[0];
			else		np = pt[i] * 2 - pt[i - 1];
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
		// --------------------------------------
		Vtx[vI].fnc = SKPSW_WIDEPEN_L | SKPSW_PENCOLOR;
		Vtx[vII].fnc = SKPSW_WIDEPEN_R | SKPSW_PENCOLOR;
		vI+=2;
		// --------------------------------------
		pp = pt[i];
		length += D3DXVec2Length(ptr(np - pp));
	}

	if (bLoop) {
		Vtx[vI++] = Vtx[0];
		Vtx[vI++] = Vtx[1];
	}

	HR(pVB->Unlock());
}








// ======================================================================================
// Triangle Interface
// ======================================================================================



D3D9Triangle::D3D9Triangle(LPDIRECT3DDEVICE9 pDev, const gcCore::clrVtx *pt, int npt, int _style) : D3D9PolyBase(1)
{
	nPt = npt;
	style = _style;
	HR(pDev->CreateVertexBuffer(nPt * sizeof(SkpVtx), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &pVB, NULL));
	if (pt) Update(pt, npt);
}


// ===============================================================================================
//
D3D9Triangle::~D3D9Triangle()
{

}


// ===============================================================================================
//
void D3D9Triangle::Release()
{
	SAFE_RELEASE(pVB);
}


// ===============================================================================================
//
void D3D9Triangle::Draw(D3D9Pad* pSkp, LPDIRECT3DDEVICE9 pDev)
{
	pDev->SetStreamSource(0, pVB, 0, sizeof(SkpVtx));
	if (style == PF_TRIANGLES) 	pDev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, nPt / 3);
	if (style == PF_FAN) pDev->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, nPt - 2);
	if (style == PF_STRIP) pDev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, nPt - 2);
}


// ===============================================================================================
//
void D3D9Triangle::Update(const gcCore::clrVtx *pt, int npt)
{
	SkpVtx *Vtx = NULL;
	HR(pVB->Lock(0, 0, (LPVOID*)&Vtx, D3DLOCK_DISCARD));

	memset(Vtx, 0, sizeof(SkpVtx)*npt);

	for (int i = 0; i < npt; i++) {
		Vtx[i].x = pt[i].pos.x;
		Vtx[i].y = pt[i].pos.y;
		Vtx[i].clr = pt[i].color;
		Vtx[i].fnc = SKPSW_CENTER | SKPSW_FRAGMENT;
	}
	HR(pVB->Unlock());
}

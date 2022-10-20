// ==============================================================
// CelSphere.cpp
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2006-2016 Martin Schweiger
//				 2012-2016 Jarmo Nikkanen
// ==============================================================

// ==============================================================
// Class CelestialSphere (implementation)
//
// This class is responsible for rendering the celestial sphere
// background (stars, constellations, grids, labels, etc.)
// ==============================================================

#include "CelSphere.h"
#include "CSphereMgr.h"
#include "Scene.h"
#include "D3D9Config.h"
#include "D3D9Pad.h"
#include "AABBUtil.h"


#define NSEG 64 // number of segments in celestial grid lines

using namespace oapi;

// ==============================================================

D3D9CelestialSphere::D3D9CelestialSphere(D3D9Client *gc, Scene *scene)
	: oapi::CelestialSphere(gc)
	, m_gc(gc)
	, m_scene(scene)
	, m_pDevice(gc->GetDevice())
	, maxNumVertices(gc->GetHardwareCaps()->MaxPrimitiveCount)
{
	InitStars();
	InitConstellationLines();
	InitConstellationBoundaries();
	LoadConstellationLabels();
	AllocGrids();
	m_bkgImgMgr = new CSphereManager(gc, scene);

	m_textBlendAdditive = true;
	m_mjdPrecessionChecked = -1e10;
}

// ==============================================================

D3D9CelestialSphere::~D3D9CelestialSphere()
{
	for (auto it = m_sVtx.begin(); it != m_sVtx.end(); it++)
		(*it)->Release();
	m_clVtx->Release();
	m_cbVtx->Release();
	m_grdLngVtx->Release();
	m_grdLatVtx->Release();
	delete m_bkgImgMgr;
}

// ==============================================================

void D3D9CelestialSphere::InitCelestialTransform()
{
	MATRIX3 R = Celestial2Ecliptic();

	m_rotCelestial._11 = (float)R.m11; m_rotCelestial._12 = (float)R.m12; m_rotCelestial._13 = (float)R.m13; m_rotCelestial._14 = 0.0f;
	m_rotCelestial._21 = (float)R.m21; m_rotCelestial._22 = (float)R.m22; m_rotCelestial._23 = (float)R.m23; m_rotCelestial._24 = 0.0f;
	m_rotCelestial._31 = (float)R.m31; m_rotCelestial._32 = (float)R.m32; m_rotCelestial._33 = (float)R.m33; m_rotCelestial._34 = 0.0f;
	m_rotCelestial._41 = 0.0f;         m_rotCelestial._42 = 0.0f;         m_rotCelestial._43 = 0.0f;         m_rotCelestial._44 = 1.0f;

	m_mjdPrecessionChecked = oapiGetSimMJD();
}

// ==============================================================

void D3D9CelestialSphere::InitStars ()
{
	const std::vector<oapi::CelestialSphere::StarRenderRec> sList = LoadStars();
	m_nsVtx = sList.size();
	if (!m_nsVtx) return;

	DWORD i, j, nv, idx = 0;

	// convert star database to vertex buffers
	DWORD nbuf = (m_nsVtx + maxNumVertices - 1) / maxNumVertices; // number of buffers required
	m_sVtx.resize(nbuf);
	for (auto it = m_sVtx.begin(); it != m_sVtx.end(); it++) {
		nv = min(maxNumVertices, m_nsVtx - idx);
		m_pDevice->CreateVertexBuffer(UINT(nv*sizeof(VERTEX_XYZC)), D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &*it, NULL);
		VERTEX_XYZC *vbuf;
		(*it)->Lock(0, 0, (LPVOID*)&vbuf, 0);
		for (j = 0; j < nv; j++) {
			const oapi::CelestialSphere::StarRenderRec& rec = sList[idx];
			VERTEX_XYZC &v = vbuf[j];
			v.x = (float)rec.pos.x;
			v.y = (float)rec.pos.y;
			v.z = (float)rec.pos.z;
			v.col = D3DXCOLOR(rec.col.x, rec.col.y, rec.col.z, 1);
			idx++;
		}
		(*it)->Unlock();
	}

	m_starCutoffIdx = ComputeStarBrightnessCutoff(sList);
}

// ==============================================================

int D3D9CelestialSphere::MapLineBuffer(const std::vector<VECTOR3>& lineVtx, LPDIRECT3DVERTEXBUFFER9& buf) const
{
	size_t nv = lineVtx.size();
	if (!nv) return 0;

	// create vertex buffer
	m_pDevice->CreateVertexBuffer(sizeof(VERTEX_XYZ) * nv, D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &buf, NULL);
	VERTEX_XYZ* vbuf;
	buf->Lock(0, 0, (LPVOID*)&vbuf, 0);
	for (size_t i = 0; i < nv; i++) {
		vbuf[i].x = (float)lineVtx[i].x;
		vbuf[i].y = (float)lineVtx[i].y;
		vbuf[i].z = (float)lineVtx[i].z;
	}
	buf->Unlock();

	return nv;
}

// ==============================================================

void D3D9CelestialSphere::InitConstellationLines()
{
	m_nclVtx = MapLineBuffer(LoadConstellationLines(), m_clVtx);
}

// ==============================================================

void D3D9CelestialSphere::InitConstellationBoundaries()
{
	m_ncbVtx = MapLineBuffer(LoadConstellationBoundaries(), m_cbVtx);
}

// ==============================================================

void D3D9CelestialSphere::AllocGrids ()
{
	int i, j, idx;
	double lng, lat, xz, y;
	VERTEX_XYZ *vbuf;

	m_pDevice->CreateVertexBuffer(sizeof(VERTEX_XYZ)*(NSEG+1)*11, D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &m_grdLngVtx, NULL);
	m_grdLngVtx->Lock (0, 0, (LPVOID*)&vbuf, 0);
	for (j = idx = 0; j <= 10; j++) {
		lat = (j-5)*15*RAD;
		xz = cos(lat);
		y  = sin(lat);
		for (i = 0; i <= NSEG; i++) {
			lng = 2.0*PI * (double)i/(double)NSEG;
			vbuf[idx].x = (float)(xz * cos(lng));
			vbuf[idx].z = (float)(xz * sin(lng));
			vbuf[idx].y = (float)y;
			idx++;
		}
	}
	m_grdLngVtx->Unlock();

	m_pDevice->CreateVertexBuffer(sizeof(VERTEX_XYZ)*(NSEG+1)*12, D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &m_grdLatVtx, NULL);
	m_grdLatVtx->Lock (0, 0, (LPVOID*)&vbuf, 0);
	for (j = idx = 0; j < 12; j++) {
		lng = j*15*RAD;
		for (i = 0; i <= NSEG; i++) {
			lat = 2.0*PI * (double)i/(double)NSEG;
			xz = cos(lat);
			y  = sin(lat);
			vbuf[idx].x = (float)(xz * cos(lng));
			vbuf[idx].z = (float)(xz * sin(lng));
			vbuf[idx].y = (float)y;
			idx++;
		}
	}
	m_grdLatVtx->Unlock();
}

// ==============================================================

void D3D9CelestialSphere::Render(LPDIRECT3DDEVICE9 pDevice, const VECTOR3& skyCol)
{
	SetSkyColour(skyCol);

	// Get celestial sphere render flags
	DWORD renderFlag = *(DWORD*)m_gc->GetConfigParam(CFGPRM_PLANETARIUMFLAG);

	// celestial sphere background image
	RenderBkgImage(pDevice);

	if (renderFlag & PLN_ENABLE) {

		HR(s_FX->SetTechnique(s_eLine));
		HR(s_FX->SetMatrix(s_eWVP, m_scene->GetProjectionViewMatrix()));

		// render ecliptic grid
		if (renderFlag & PLN_EGRID) {
			FVECTOR4 baseCol(0.0f, 0.0f, 0.4f, 1.0f);
			D3DXVECTOR4 vColor = ColorAdjusted(baseCol);
			HR(s_FX->SetVector(s_eColor, &vColor));
			RenderGrid(s_FX, !(renderFlag & PLN_ECL));
		}

		// render ecliptic equator
		if (renderFlag & PLN_ECL) {
			FVECTOR4 baseCol(0.0f, 0.0f, 0.8f, 1.0f);
			D3DXVECTOR4 vColor = ColorAdjusted(baseCol);
			HR(s_FX->SetVector(s_eColor, &vColor));
			RenderGreatCircle(s_FX);
		}

		// render celestial grid ----------------------------------------------------------------------------
		if (renderFlag & PLN_CGRID) {
			if (fabs(m_mjdPrecessionChecked - oapiGetSimMJD()) > 1e3)
				InitCelestialTransform();
			D3DXMATRIX rot;
			D3DXMatrixMultiply(&rot, &m_rotCelestial, m_scene->GetProjectionViewMatrix());
			HR(s_FX->SetMatrix(s_eWVP, &rot));
			FVECTOR4 baseCol1(0.3f, 0.0f, 0.3f, 1.0f);
			D3DXVECTOR4 vColor1 = ColorAdjusted(baseCol1);
			HR(s_FX->SetVector(s_eColor, &vColor1));
			RenderGrid(s_FX, false);
			FVECTOR4 baseCol2(0.7f, 0.0f, 0.7f, 1.0f);
			D3DXVECTOR4 vColor2 = ColorAdjusted(baseCol2);
			HR(s_FX->SetVector(s_eColor, &vColor2));
			RenderGreatCircle(s_FX);
		}

		// render equator of target celestial body
		if (renderFlag & PLN_EQU) {
			OBJHANDLE hRef = oapiCameraProxyGbody();
			if (hRef) {
				MATRIX3 R;
				oapiGetRotationMatrix(hRef, &R);
				D3DXMATRIX iR = {
					(float)R.m11, (float)R.m21, (float)R.m31, 0.0f,
					(float)R.m12, (float)R.m22, (float)R.m32, 0.0f,
					(float)R.m13, (float)R.m23, (float)R.m33, 0.0f,
					0.0f,         0.0f,         0.0f,         1.0f
				};
				D3DXMATRIX rot;
				D3DXMatrixMultiply(&rot, &iR, m_scene->GetProjectionViewMatrix());
				HR(s_FX->SetMatrix(s_eWVP, &rot));
				FVECTOR4 baseCol(0.0f, 0.6f, 0.0f, 1.0f);
				D3DXVECTOR4 vColor = ColorAdjusted(baseCol);
				HR(s_FX->SetVector(s_eColor, &vColor));
				RenderGreatCircle(s_FX);
			}
		}

		// render constellation boundaries ----------------------------------------
		if (renderFlag & PLN_CONST) { // for now, hijack the constellation line flag
			HR(s_FX->SetMatrix(s_eWVP, m_scene->GetProjectionViewMatrix()));
			RenderConstellationBoundaries(s_FX);
		}

		// render constellation lines ---------------------------------------------
		if (renderFlag & PLN_CONST) {
			HR(s_FX->SetMatrix(s_eWVP, m_scene->GetProjectionViewMatrix()));
			RenderConstellationLines(s_FX);
		}
	}

	// render stars
	HR(s_FX->SetTechnique(s_eStar));
	HR(s_FX->SetMatrix(s_eWVP, m_scene->GetProjectionViewMatrix()));
	RenderStars(s_FX);

	// render markers and labels
	if (renderFlag & PLN_ENABLE) {

		// Sketchpad for planetarium mode labels and markers
		D3D9Pad* pSketch = m_scene->GetPooledSketchpad(0);

		if (pSketch) {

			// constellation labels --------------------------------------------------
			if (renderFlag & PLN_CNSTLABEL) {
				RenderConstellationLabels((oapi::Sketchpad**)&pSketch, renderFlag & PLN_CNSTLONG);
			}
			// celestial marker (stars) names ----------------------------------------
			if (renderFlag & PLN_CCMARK) {
				RenderCelestialMarkers((oapi::Sketchpad**)&pSketch);
			}
			pSketch->EndDrawing(); //SKETCHPAD_LABELS
		}
	}

}

// ==============================================================

void D3D9CelestialSphere::RenderStars(ID3DXEffect *FX)
{
	_TRACE;

	// render in chunks, because some graphics cards have a limit in the
	// vertex list size
	UINT i, j, numPasses = 0;
	int bgidx = min(255, (int)(GetSkyBrightness() * 256.0));
	int ns = m_starCutoffIdx[bgidx];

	HR(m_pDevice->SetVertexDeclaration(pPosColorDecl));
	HR(FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE));
	HR(FX->BeginPass(0));
	for (i = j = 0; i < ns; i += maxNumVertices, j++) {
		HR(m_pDevice->SetStreamSource(0, m_sVtx[j], 0, sizeof(VERTEX_XYZC)));
		HR(m_pDevice->DrawPrimitive(D3DPT_POINTLIST, 0, min (ns-i, maxNumVertices)));
	}
	HR(FX->EndPass());
	HR(FX->End());	
}

// ==============================================================

void D3D9CelestialSphere::RenderConstellationLines(ID3DXEffect *FX)
{
	const FVECTOR4 baseCol(0.5f, 0.3f, 0.2f, 1.0f);
	D3DXVECTOR4 vColor = ColorAdjusted(baseCol);
	HR(s_FX->SetVector(s_eColor, &vColor));

	_TRACE;
	UINT numPasses = 0;
	HR(FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE));
	HR(FX->BeginPass(0));
	HR(m_pDevice->SetStreamSource(0, m_clVtx, 0, sizeof(VERTEX_XYZ)));
	HR(m_pDevice->SetVertexDeclaration(pPositionDecl));
	HR(m_pDevice->DrawPrimitive(D3DPT_LINELIST, 0, m_nclVtx));
	HR(FX->EndPass());
	HR(FX->End());	
}

// ==============================================================

void D3D9CelestialSphere::RenderConstellationBoundaries(ID3DXEffect* FX)
{
	const FVECTOR4 baseCol(0.25f, 0.22f, 0.2f, 1.0f);
	D3DXVECTOR4 vColor = ColorAdjusted(baseCol);
	HR(s_FX->SetVector(s_eColor, &vColor));

	_TRACE;
	UINT numPasses = 0;
	HR(FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE));
	HR(FX->BeginPass(0));
	HR(m_pDevice->SetStreamSource(0, m_cbVtx, 0, sizeof(VERTEX_XYZ)));
	HR(m_pDevice->SetVertexDeclaration(pPositionDecl));
	HR(m_pDevice->DrawPrimitive(D3DPT_LINELIST, 0, m_ncbVtx));
	HR(FX->EndPass());
	HR(FX->End());
}

// ==============================================================

void D3D9CelestialSphere::RenderGreatCircle(ID3DXEffect *FX)
{
	_TRACE;
	UINT numPasses = 0;
	HR(FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE));
	HR(FX->BeginPass(0));
	HR(m_pDevice->SetStreamSource(0, m_grdLngVtx, 0, sizeof(VERTEX_XYZ)));
	HR(m_pDevice->SetVertexDeclaration(pPositionDecl));
	HR(m_pDevice->DrawPrimitive(D3DPT_LINESTRIP, 5*(NSEG+1), NSEG));
	HR(FX->EndPass());
	HR(FX->End());	
	
}

// ==============================================================

void D3D9CelestialSphere::RenderGrid(ID3DXEffect *FX, bool eqline)
{
	_TRACE;
	int i;
	UINT numPasses = 0;
	HR(m_pDevice->SetVertexDeclaration(pPositionDecl));
	HR(m_pDevice->SetStreamSource(0, m_grdLngVtx, 0, sizeof(VERTEX_XYZ)));
	HR(FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE));
	HR(FX->BeginPass(0));
	for (i = 0; i <= 10; i++)
		if (eqline || i != 5)
			HR(m_pDevice->DrawPrimitive(D3DPT_LINESTRIP, i*(NSEG+1), NSEG));
	HR(m_pDevice->SetStreamSource(0, m_grdLatVtx, 0, sizeof(VERTEX_XYZ)));
	for (i = 0; i < 12; i++)
		HR(m_pDevice->DrawPrimitive(D3DPT_LINESTRIP, i * (NSEG+1), NSEG));
	HR(FX->EndPass());
	HR(FX->End());	
}

// ==============================================================

void D3D9CelestialSphere::RenderBkgImage(LPDIRECT3DDEVICE9 dev)
{
	m_bkgImgMgr->Render(dev, 8, GetSkyBrightness());
	dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
}

// ==============================================================

bool D3D9CelestialSphere::EclDir2WindowPos(const VECTOR3& dir, int& x, int& y) const
{
	D3DXVECTOR3 homog;
	D3DXVECTOR3 fdir((float)dir.x, (float)dir.y, (float)dir.z);

	D3DXVec3TransformCoord(&homog, &fdir, m_scene->GetProjectionViewMatrix());

	if (homog.x >= -1.0f && homog.x <= 1.0f &&
		homog.y >= -1.0f && homog.y <= 1.0f &&
		homog.z < 1.0f) {

		if (_hypot(homog.x, homog.y) < 1e-6) {
			x = m_scene->ViewW() / 2;
			y = m_scene->ViewH() / 2;
		}
		else {
			x = (int)(m_scene->ViewW() * 0.5 * (1.0 + homog.x));
			y = (int)(m_scene->ViewH() * 0.5 * (1.0 - homog.y));
		}
		return true;
	}
	else {
		return false;
	}
}

void D3D9CelestialSphere::D3D9TechInit(ID3DXEffect* fx)
{
	s_FX = fx;
	s_eStar = fx->GetTechniqueByName("StarTech");
	s_eLine = fx->GetTechniqueByName("LineTech");
	s_eColor = fx->GetParameterByName(0, "gColor");
	s_eWVP = fx->GetParameterByName(0, "gWVP");
}

ID3DXEffect* D3D9CelestialSphere::s_FX = 0;
D3DXHANDLE D3D9CelestialSphere::s_eStar = 0;
D3DXHANDLE D3D9CelestialSphere::s_eLine = 0;
D3DXHANDLE D3D9CelestialSphere::s_eColor = 0;
D3DXHANDLE D3D9CelestialSphere::s_eWVP = 0;

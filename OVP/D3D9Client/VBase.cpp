// ==============================================================
// VBase.cpp
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2007 - 2016 Martin Schweiger
//				 2011 - 2016 Jarmo Nikkanen (D3D9Client modification)  
// ==============================================================

// ==============================================================
// class vBase (implementation)
//
// A vBase is the visual representation of a surface base
// object (a "spaceport" on the surface of a planet or moon,
// usually with runways or landing pads where vessels can
// land and take off.
// ==============================================================

#include "VBase.h"
#include "TileMgr.h"
#include "D3D9Client.h"
#include "D3D9Surface.h"
#include "BeaconArray.h"
#include "RunwayLights.h"
#include "AABBUtil.h"
#include "OrbiterAPI.h"
#include "DebugControls.h"
#include "D3D9Config.h"
#include "VPlanet.h"

#pragma warning(push)
#pragma warning(disable : 4838)
#include <xnamath.h>
#pragma warning(pop)

typedef struct {
	float rad;
	float width, length, height;
	D3DXVECTOR3 pos, min, max;
} MeshStats;


void CheckMeshStats(MESHHANDLE hMesh, MeshStats *stats)
{
	int nGrp = oapiMeshGroupCount(hMesh);
	if (nGrp == 0) return;

	XMVECTOR mi = XMLoadFloat3(ptr(XMFLOAT3(1e12f, 1e12f, 1e12f)));
	XMVECTOR mx = -mi;

	for (int i = 0; i < nGrp; i++) {

		MESHGROUPEX *grp = oapiMeshGroupEx(hMesh, i);
		
		for (DWORD v = 0; v < grp->nVtx; v++) {
			XMVECTOR x = XMLoadFloat3((XMFLOAT3 *)&grp->Vtx[v].x);
			mi = XMVectorMin(mi, x);
			mx = XMVectorMax(mx, x);
		}
	}

	XMStoreFloat3((XMFLOAT3 *)&stats->min.x, mi);
	XMStoreFloat3((XMFLOAT3 *)&stats->max.x, mx);

	stats->width = stats->max.x - stats->min.x;
	stats->height = stats->max.y - stats->min.y;
	stats->length = stats->max.z - stats->min.z;
	stats->pos = (stats->max + stats->min) * 0.5f;
	stats->rad = D3DXVec3Length(ptr(stats->max + stats->min)) * 0.5f;
}



vBase::vBase (OBJHANDLE _hObj, const Scene *scene, vPlanet *_vP): vObject (_hObj, scene)
{
	_TRACE;
	DWORD i,j;

	vP = _vP;
	hPlanet = oapiGetBasePlanet(hObj);

	if (!vP) vP = static_cast<vPlanet*>( scene->GetVisObject(hPlanet) );
	
	structure_bs	= NULL;
	structure_as	= NULL;
	nstructure_bs	= 0;
	nstructure_as	= 0;
	tspec			= NULL;
	tilemesh		= NULL;
	numRunwayLights = 0;
	numTaxiLights   = 0;
	runwayLights    = NULL;
	taxiLights		= NULL;
	csun_lights     = RAD * Config->SunAngle;

	// ----------------------------------------------------------------------
	// Compute transformations from local base frame to planet frame and back
	//
	MATRIX3 plrot; VECTOR3 relpos;
	oapiGetRotationMatrix(hPlanet, &plrot);
	oapiGetRotationMatrix(hObj, &mGlobalRot);
	oapiGetRelativePos(hObj, hPlanet, &relpos);
	vLocalPos = tmul(plrot, relpos);

	swap(plrot.m12, plrot.m21);
	swap(plrot.m13, plrot.m31);
	swap(plrot.m23, plrot.m32);

	mGlobalRot = mul(plrot, mGlobalRot);

	D3DXMatrixIdentity(&mGlobalRotDX);
	D3DMAT_SetRotation(&mGlobalRotDX, &mGlobalRot);
	//------------------------------------------------------------------------

	// load surface tiles
	DWORD _ntile = gc->GetBaseTileList (_hObj, &tspec);
	ntile = 0;
	for (i=0; i<_ntile; ++i) {
		// Only count (render) tiles where bit0 is set!
		if (tspec[i].texflag & 0x01) {
			++ntile;
		}
	}

	// Do not render tiles for planets having a new tile format
	if (vP->tilever >= 2) ntile = 0;

	if (ntile) {

		MESHGROUPEX **grps = new MESHGROUPEX*[ntile];
		SURFHANDLE *texs = new SURFHANDLE[ntile];

		for (i = 0, j = 0; i < _ntile; ++i) {
			// Only render tiles where bit0 is set!
			if (tspec[i].texflag & 0x01) {
				DWORD ng = oapiMeshGroupCount(tspec[i].mesh);
				if (ng!=1) LogErr("MeshGroup Count = %u",ng);
				else {
					texs[j] = tspec[i].tex;
					grps[j] = oapiMeshGroupEx(tspec[i].mesh, 0);
				}
				++j;
			}
		}
		tilemesh = new D3D9Mesh(ntile, (const MESHGROUPEX**)grps, texs);	
		delete []grps;
		grps = NULL;
        delete []texs;
		texs = NULL;
	}

	// load meshes for generic structures
	MESHHANDLE *sbs, *sas;
	DWORD nsbs, nsas;
	gc->GetBaseStructures (_hObj, &sbs, &nsbs, &sas, &nsas);

	if (nstructure_bs = nsbs) {
		structure_bs = new D3D9Mesh*[nsbs];
		for (i = 0; i < nsbs; i++) structure_bs[i] = new D3D9Mesh(sbs[i]);
	}

	if (nstructure_as = nsas) {
		structure_as = new D3D9Mesh*[nsas];
		for (i = 0; i < nsas; i++) structure_as[i] = new D3D9Mesh(sas[i]);
	}

	lights = false;
	Tchk = Tlghtchk = oapiGetSimTime()-1.0;

	UpdateBoundingBox();
	
	char name[64];
	oapiGetObjectName(_hObj, name, 64);
	LogAlw("New Base Visual(%s) %s hBase=%s, nsbs=%u, nsas=%u", _PTR(this), name, _PTR(_hObj), nsbs, nsas);

	CreateRunwayLights();
	CreateTaxiLights();
}


// ===========================================================================================
//
VECTOR3 vBase::ToLocal(VECTOR3 pos, double *lng, double *lat) const
{
	double rad;
	VECTOR3 vLoc = mul(mGlobalRot, pos) + vLocalPos;
	if (lng && lat) oapiLocalToEqu(hPlanet, vLoc, lng, lat, &rad);
	return vLoc;
}


// ===========================================================================================
//
VECTOR3 vBase::FromLocal(VECTOR3 pos) const
{
	return tmul(mGlobalRot, pos-vLocalPos);
}

// ===========================================================================================
//
void vBase::FromLocal(VECTOR3 pos, D3DXVECTOR3 *pTgt) const
{
	D3DXVECTOR3 pv(float(pos.x-vLocalPos.x), float(pos.y-vLocalPos.y), float(pos.z-vLocalPos.z));
	D3DXVec3TransformNormal(pTgt, &pv, &mGlobalRotDX);
}

// ===========================================================================================
//
double vBase::GetElevation() const
{
	VECTOR3 bp;
	oapiGetRelativePos(hObj, hPlanet, &bp);
	return length(bp) - oapiGetSize(hPlanet);
}


// ===========================================================================================
//
void vBase::CreateRunwayLights()
{
	const char *file = oapiGetObjectFileName(hObj);
	if (file) numRunwayLights = RunwayLights::CreateRunwayLights(this, scn, file, runwayLights);
	else LogErr("Configuration file not found for object %s", _PTR(hObj));
}

// ===========================================================================================
//
void vBase::CreateTaxiLights()
{
	const char *file = oapiGetObjectFileName(hObj);
	if (file) numTaxiLights = TaxiLights::CreateTaxiLights(hObj, scn, file, taxiLights);
	else LogErr("Configuration file not found for object %s", _PTR(hObj));
}

// ===========================================================================================
//
vBase::~vBase ()
{
	DWORD i;

	if (tilemesh) delete tilemesh;

	if (nstructure_bs) {
		for (i = 0; i < nstructure_bs; i++)	delete structure_bs[i];
		delete []structure_bs;
		structure_bs = NULL;
	}
	if (nstructure_as) {
		for (i = 0; i < nstructure_as; i++)	delete structure_as[i];
		delete []structure_as;
		structure_as = NULL;
	}

	if (runwayLights) {
		for(i=0; i<(DWORD)numRunwayLights; i++)
		{
			SAFE_DELETE(runwayLights[i]);
		}
		delete[] runwayLights;
		runwayLights = NULL;
	}

	if (taxiLights) {
		for(i=0; i<(DWORD)numTaxiLights; i++)
		{
			SAFE_DELETE(taxiLights[i]);
		}
		delete[] taxiLights;
	}

	if (DebugControls::IsActive()) {
		DebugControls::RemoveVisual(this);
	}
}

// ===========================================================================================
//
DWORD vBase::GetMeshCount()
{
	if (tilemesh) return nstructure_bs + nstructure_as + 1;
	else          return nstructure_bs + nstructure_as;
}

// ===========================================================================================
//
bool vBase::GetMinMaxDistance(float *zmin, float *zmax, float *dmin)
{
	if (bBSRecompute) UpdateBoundingBox();
	
	D3DXMATRIX mWorldView;

	Scene *scn = gc->GetScene();

	D3DXVECTOR4 Field = D9LinearFieldOfView(scn->GetProjectionMatrix());
	
	D3DXMatrixMultiply(&mWorldView, &mWorld, scn->GetViewMatrix());
	
	if (tilemesh) {
		D9ComputeMinMaxDistance(gc->GetDevice(), tilemesh->GetAABB(), &mWorldView, &Field, zmin, zmax, dmin);
	}

	if (nstructure_bs) {
		for (DWORD i = 0; i < nstructure_bs; i++) {
			D9ComputeMinMaxDistance(gc->GetDevice(), structure_bs[i]->GetAABB(), &mWorldView, &Field, zmin, zmax, dmin);
		}
	}

	if (nstructure_as) {
		for (DWORD i = 0; i < nstructure_as; i++) {
			D9ComputeMinMaxDistance(gc->GetDevice(), structure_as[i]->GetAABB(), &mWorldView, &Field, zmin, zmax, dmin);
		}
	}

	return true;
}


// ===========================================================================================
//
void vBase::UpdateBoundingBox()
{
	bBSRecompute = false;
	
	if (tilemesh || nstructure_bs || nstructure_as) D9InitAABB(&BBox);
	else D9ZeroAABB(&BBox);

	
	if (tilemesh) D9AddAABB(tilemesh->GetAABB(), NULL, &BBox);
		
	if (nstructure_bs) {
		for (DWORD i = 0; i < nstructure_bs; i++) {
			D9AddAABB(structure_bs[i]->GetAABB(), NULL, &BBox);
		}
	}

	if (nstructure_as) {
		for (DWORD i = 0; i < nstructure_as; i++) {
			D9AddAABB(structure_as[i]->GetAABB(), NULL, &BBox);
		}
	}

	D9UpdateAABB(&BBox);
}


// ===========================================================================================
//
bool vBase::Update (bool bMainScene)
{
	_TRACE;
	if (!active) return false;
	if (!vObject::Update(bMainScene)) return false;

	double simt = oapiGetSimTime();

	if (fabs(simt-Tlghtchk)>0.1 || oapiGetPause()) {
		VECTOR3 rpos = gpos - vP->GlobalPos();
		sunLight = vP->GetObjectAtmoParams(rpos);
		Tlghtchk = simt;
	}

	if (fabs(simt-Tchk)>1.0) {
		VECTOR3 pos, sdir;
		MATRIX3 rot;
		oapiGetGlobalPos (hObj, &pos); normalise(pos);
		oapiGetRotationMatrix (hObj, &rot);
		sdir = tmul (rot, -pos);
		double csun = sdir.y;
		bool night = csun < csun_lights;
		if (lights != night) {
			DWORD i;
			for (i = 0; i < nstructure_bs; i++)	structure_bs[i]->SetTexMixture (1, night ? 1.0f:0.0f);
			for (i = 0; i < nstructure_as; i++)	structure_as[i]->SetTexMixture (1, night ? 1.0f:0.0f);
			lights = night;
		}
		Tchk = simt;
	}
	return true;
}


// ===========================================================================================
//
bool vBase::RenderSurface(LPDIRECT3DDEVICE9 dev)
{
	// note: assumes z-buffer disabled
	if (!active) return false;
	if (!IsVisible()) return false;

	g_pCurrentVisual = this;

	// render tiles
	if (tilemesh) {
		g_uCurrentMesh = 0; // Used for debugging
		tilemesh->SetSunLight(&sunLight);
		tilemesh->RenderBaseTile(&mWorld);
		++g_uCurrentMesh;
	}

	// render generic objects under shadows
	if (nstructure_bs) {
		for (DWORD i = 0; i < nstructure_bs; ++i) {
			structure_bs[i]->SetSunLight(&sunLight);
			structure_bs[i]->Render(&mWorld, nullptr, RENDER_BASEBS);
			++g_uCurrentMesh;
		}
	}

	return true;
}


// ===========================================================================================
//
bool vBase::RenderStructures(LPDIRECT3DDEVICE9 dev)
{
	if (!active) return false;
	if (!IsVisible()) return false;

	g_pCurrentVisual = this;
	g_uCurrentMesh = 0; // Used for debugging

	if (tilemesh) g_uCurrentMesh++;
	g_uCurrentMesh += nstructure_bs;

	// render generic objects above shadows
	for (DWORD i=0; i<nstructure_as; i++) {
		FVECTOR3 bs = structure_as[i]->GetBoundingSpherePos();
		FVECTOR3 qw = TransformCoord(bs, mWorld);
		D3D9Sun sp = vP->GetObjectAtmoParams(qw._V() + vP->CameraPos());
		structure_as[i]->SetSunLight(&sp);
		structure_as[i]->Render(&mWorld, nullptr, RENDER_BASE);
		++g_uCurrentMesh;
	}
	return true;
}


// ===========================================================================================
//
void vBase::RenderRunwayLights(LPDIRECT3DDEVICE9 dev)
{
	if (!active) return;
	if (!IsVisible()) return;

	g_pCurrentVisual = this;

	for(int i=0; i<numRunwayLights; i++)
	{
		if (scn->GetRenderPass() == RENDERPASS_MAINSCENE) runwayLights[i]->Update(vP);
		runwayLights[i]->Render(dev, &mWorld, lights);
	}

	for(int i=0; i<numTaxiLights; i++)
	{
		taxiLights[i]->Render(dev, &mWorld, lights);
	}
	
	if (DebugControls::IsActive()) {
		DWORD flags = *(DWORD*)gc->GetConfigParam(CFGPRM_GETDEBUGFLAGS);
		if (flags&DBG_FLAGS_SELVISONLY && this!=DebugControls::GetVisual()) return; // Used for debugging
		if (flags&DBG_FLAGS_BOXES) {
			D3DXMATRIX id;
			D3D9Effect::RenderBoundingBox(&mWorld, D3DXMatrixIdentity(&id), &BBox.min, &BBox.max, ptr(D3DXVECTOR4(1,0,1,0.75f)));
		}
	}
}


// ===========================================================================================
//
void vBase::RenderGroundShadow(LPDIRECT3DDEVICE9 dev, float alpha)
{
	if (!nstructure_as) return; // nothing to do
	if (!active) return;
	if (!IsVisible()) return;
	if (Config->TerrainShadowing == 0) return;

	g_pCurrentVisual = this;

	VECTOR3 sd;
	oapiGetGlobalPos(hObj, &sd); normalise(sd);
	
	MATRIX3 mRot;
	oapiGetRotationMatrix(hObj, &mRot);
	D3DXVECTOR3 lsun = D3DXVEC(tmul(mRot, sd));

	if (lsun.y > -0.07f) return;

	float scale = (-lsun.y - 0.07f) * 25.0f;
	scale = (1.0f - alpha) * saturate(scale);
	

	// build shadow projection matrix
	D3DXMATRIX mProj;
	
	OBJHANDLE hPlanet = oapiGetBasePlanet(hObj); 
	double prad = oapiGetSize(hPlanet);
	D3DXVECTOR4 param = D9OffsetRange(prad, 30e3);

	for (DWORD i=0; i<nstructure_as; i++) {
		
		if (structure_as[i]->HasShadow()) {

			double a, b, el0, el1, el2;
			double d = atan(1.0 / prad);
			VECTOR3 va, vb, vc;
			VECTOR3 q = _V(structure_as[i]->BBox.bs);
			float rad = structure_as[i]->BBox.bs.w;

			if (rad<250.0f) ToLocal(q, &a, &b);	
			else ToLocal(_V(0,0,0), &a, &b);

			if (vP->GetElevation(a, b, &el0) <= 0) el0 = oapiSurfaceElevation(hPlanet, a, b);
			if (vP->GetElevation(a + d, b, &el1) <= 0) el1 = oapiSurfaceElevation(hPlanet, a + d, b);
			if (vP->GetElevation(a, b + d, &el2) <= 0) el2 = oapiSurfaceElevation(hPlanet, a, b + d);

			oapiEquToLocal(hPlanet, a, b, el0 + prad, &va);
			oapiEquToLocal(hPlanet, a + d, b, el1 + prad, &vb);
			oapiEquToLocal(hPlanet, a, b + d, el2 + prad, &vc);

			va = FromLocal(va);
			vb = FromLocal(vb);
			vc = FromLocal(vc);
				
			VECTOR3 n = -unit(crossp(vb - va, vc - va));

			D3DXVECTOR3 hn = D3DXVEC(n); 
				
			float zo = float(-dotp(va, n));
			float nd = D3DXVec3Dot(&hn, &lsun);
			hn /= nd;
			float ofs = zo / nd;

			mProj._11 = 1.0f - (float)(lsun.x*hn.x);
			mProj._12 = -(float)(lsun.y*hn.x);
			mProj._13 = -(float)(lsun.z*hn.x);
			mProj._14 = 0;
			mProj._21 = -(float)(lsun.x*hn.y);
			mProj._22 = 1.0f - (float)(lsun.y*hn.y);
			mProj._23 = -(float)(lsun.z*hn.y);
			mProj._24 = 0;
			mProj._31 = -(float)(lsun.x*hn.z);
			mProj._32 = -(float)(lsun.y*hn.z);
			mProj._33 = 1.0f - (float)(lsun.z*hn.z);
			mProj._34 = 0;
			mProj._41 = -(float)(lsun.x*ofs);
			mProj._42 = -(float)(lsun.y*ofs);
			mProj._43 = -(float)(lsun.z*ofs);
			mProj._44 = 1;
				
			D3DXVECTOR4 nrml = D3DXVECTOR4(float(n.x), float(n.y), float(n.z), zo);

			structure_as[i]->RenderShadowsEx(scale, &mProj, &mWorld, &nrml, &param);
		}
	}
}

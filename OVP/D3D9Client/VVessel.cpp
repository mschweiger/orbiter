// ==============================================================
// VVessel.cpp
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2006-2016 Martin Schweiger
//				 2010-2019 Jarmo Nikkanen (D3D9Client modification)
// ==============================================================

#include <set>
#include <algorithm>
#include "VVessel.h"
#include "VPlanet.h"
#include "MeshMgr.h"
#include "AABBUtil.h"
#include "D3D9Surface.h"
#include "D3D9Catalog.h"
#include "D3D9Config.h"
#include "OapiExtension.h"
#include "DebugControls.h"
#include "D3D9Util.h"
#include "MaterialMgr.h"
#include "IProcess.h"

using namespace oapi;

// ==============================================================
// Local prototypes

void TransformPoint (VECTOR3 &p, const D3DXMATRIX &T);
void TransformDirection (VECTOR3 &a, const D3DXMATRIX &T, bool normalise);
const char *value_string (double val);

// ==============================================================
// class vVessel (implementation)
//
// A vVessel is the visual representation of a vessel object.
// ==============================================================

void LogComp(ANIMATIONCOMP *AC, int ident)
{
	char id[64];
	strcpy_s(id, 64, "");
	for (int i = 0; i < ident; i++) strcat_s(id, 64, " ");
	oapiWriteLogV("%s COMP[%s] has %u children, Parent = %s", id, _PTR(AC), AC->nchildren, _PTR(AC->parent));
	for (UINT i = 0; i < AC->nchildren; i++) LogComp(AC->children[i], ident + 2);
}


vVessel::vVessel(OBJHANDLE _hObj, const Scene *scene): vObject (_hObj, scene)
{
	_TRACE;

	//@todo throw @ vObject or visObject???
	if (_hObj==NULL) throw std::invalid_argument("_hObj");

	vessel = oapiGetVesselInterface(_hObj);
	nmesh = 0;
	sunLight = *scene->GetSun();
	tCheckLight = oapiGetSimTime()-1.0;
	vClass = 0;
	pMatMgr = new MatMgr(this, scene->GetClient());

	if (strncmp(vessel->GetClassNameA(), "XR2Ravenstar", 12) == 0) vClass = VCLASS_XR2;
	if (strncmp(vessel->GetClassNameA(), "SpaceShuttleUltra", 17) == 0) vClass = VCLASS_ULTRA;
	if (strncmp(vessel->GetClassNameA(), "SSU_CentaurGPrime", 17) == 0) vClass = VCLASS_SSU_CENTAUR;

	bBSRecompute = true;
	ExhaustLength = 0.0f;
	LoadMeshes();

	// Initialize static animations
	//
	UINT na = vessel->GetAnimPtr(&anim);
	
	for (UINT i = 0; i < na; i++) {
		currentstate[i] = anim[i].defstate;
		if (Config->bAbsAnims) for (UINT k = 0; k < anim[i].ncomp; ++k) StoreDefaultState(anim[i].comp[k]);	
	}

	// Initialize default eCams;
	//
	ecDefExt.flags = ENVCAM_OMIT_ATTC;
	ecDefExt.type = EnvCamType::Exterior;

	/*
	oapiWriteLogV("%s", vessel->GetClassNameA());
	oapiWriteLogV("nanim = %u", na);
	for (UINT i = 0; i < na; i++) {
		oapiWriteLogV("ANIM[%u] = %u comp(s)", i, anim[i].ncomp);
		for (UINT k = 0; k < anim[i].ncomp; ++k) LogComp(anim[i].comp[k], 2);
	}*/

	UpdateAnimations();

	for (int i = 0; i < 16; i++) BakedLightsControl[i] = FVECTOR3(0.0f, 0.0f, 0.0f);
	VCAmbient = FVECTOR3(0.5f, 0.5f, 0.5f);
	bMustRebake = true;
}


// ============================================================================================
//
vVessel::~vVessel ()
{
	SAFE_DELETE(pMatMgr);
	LogAlw("Deleting Vessel Visual %s ...", _PTR(this));
	DisposeAnimations();
	DisposeMeshes();

	for (auto& x: InteriorCams) SAFE_DELETE(x);
	
	LogAlw("Vessel visual deleted succesfully");
}


// ============================================================================================
//
void vVessel::GlobalInit(D3D9Client *gc)
{
	_TRACE;
	auto pDevice = gc->GetDevice();
	defreentrytex = SURFACE(gc->clbkLoadTexture("Reentry.dds", 0));
	defexhausttex = SURFACE(gc->clbkLoadTexture("Exhaust.dds", 0));
	pRenderZone = new ShaderClass(pDevice, "Modules/D3D9Client/Custom.hlsl", "QuadVS", "QuadPS", "RenderZones", "");
}


// ============================================================================================
//
void vVessel::GlobalExit ()
{
	DELETE_SURFACE(defexhausttex);
	DELETE_SURFACE(defreentrytex);
	SAFE_DELETE(pRenderZone);
}


// ============================================================================================
//
void vVessel::clbkEvent(DWORD evnt, DWORD_PTR _context)
{
	UINT context = (UINT)_context;

	switch (evnt) {

		case EVENT_VESSEL_INSMESH:
			bBSRecompute = true;
			InsertMesh(context);
			break;

		case EVENT_VESSEL_DELMESH:
			bBSRecompute = true;
			DelMesh(context);
			break;

		case EVENT_VESSEL_MESHVISMODE:
		{
			bBSRecompute = true;
			if (context < nmesh) {
				meshlist[context].vismode = vessel->GetMeshVisibilityMode(context);
			}
		} break;

		case EVENT_VESSEL_MESHOFS:
		{
			bBSRecompute = true;
			DWORD idx = (DWORD)context;
			if (idx < nmesh) {
				VECTOR3 ofs;
				vessel->GetMeshOffset (idx, ofs);
				if (length(ofs)) {
					if (meshlist[idx].trans==NULL) meshlist[idx].trans = new D3DXMATRIX;
					D3DMAT_Identity(meshlist[idx].trans);
					D3DMAT_SetTranslation(meshlist[idx].trans, &ofs);
				}
				else {
					SAFE_DELETE(meshlist[idx].trans);
				}
			}
		} break;

		case EVENT_VESSEL_MODMESHGROUP:
			ResetMesh(context);
			break;

		case EVENT_VESSEL_RESETANIM:
			ResetAnimations();
			break;

		case EVENT_VESSEL_CLEARANIM:
			ResetAnimations(context);
			break;

		case EVENT_VESSEL_DELANIM:
			DelAnimation(context);
			break;

		case EVENT_VESSEL_NEWANIM:
			InitNewAnimation(context);
			break;
	}
}


// ============================================================================================
//
DWORD vVessel::GetMeshCount()
{
	return nmesh;
}


// ============================================================================================
//
D3D9Mesh* vVessel::GetMesh (UINT idx)
{
	return (idx < nmesh ? meshlist[idx].mesh : NULL);
}


// ============================================================================================
//
DWORD vVessel::GetMeshVisMode(UINT idx)
{
	return (idx < nmesh ? meshlist[idx].vismode : MESHVIS_ALWAYS);
}


// ============================================================================================
//
bool vVessel::HasExtPass()
{
	for (DWORD i=0;i<nmesh;i++) if (meshlist[i].vismode&MESHVIS_EXTPASS) return true;
	return false;
}


// ============================================================================================
//
bool vVessel::HasShadow()
{
	for (DWORD i = 0; i < nmesh; i++) if (meshlist[i].mesh) if (meshlist[i].mesh->HasShadow()) return true;
	return false;
}


// ============================================================================================
//
void vVessel::PreInitObject()
{
	if (pMatMgr->LoadConfiguration()) {
		for (DWORD i=0;i<nmesh;i++) if (meshlist[i].mesh) pMatMgr->ApplyConfiguration(meshlist[i].mesh);
		pMatMgr->LoadCameraConfig();
	}
	else LogErr("Failed to load a custom configuration for %s",vessel->GetClassNameA());
}


// ============================================================================================
//
bool vVessel::Update(bool bMainScene)
{
	_TRACE;
	if (!active) return false;
	vObject::Update(bMainScene);

	if (fabs(oapiGetSimTime()-tCheckLight)>0.03 || oapiGetPause()) ModLighting();

	bBSRecompute = true;
	return true;
}


// ============================================================================================
//
void vVessel::LoadMeshes()
{
	_TRACE;
	bBSRecompute = true;
	if (nmesh) DisposeMeshes();

	MESHHANDLE hMesh = NULL;
	const D3D9Mesh *mesh = NULL;
	VECTOR3 ofs;
	UINT idx;

	MeshManager *mmgr = gc->GetMeshMgr();

	nmesh = vessel->GetMeshCount();
	meshlist = new MESHREC[nmesh+1];

	memset(meshlist, 0, nmesh*sizeof(MESHREC));

	LogAlw("Vessel(%s) %s has %u meshes", _PTR(vessel), vessel->GetClassNameA(), nmesh);

	for (idx=0;idx<nmesh;idx++) {

		hMesh = vessel->GetMeshTemplate(idx);
		mesh = mmgr->GetMesh(hMesh);

		if (hMesh && mesh) {
			// copy from preloaded template
			meshlist[idx].mesh = new D3D9Mesh(hMesh, *mesh);							// Create new Instance from an existing mesh template
			meshlist[idx].mesh->SetClass(vClass);
			meshlist[idx].mesh->SetName(idx);
		}
		else {
			// It's vital to use "CopyMeshFromTemplate" here for some reason
			// No global template exists for this mesh. Loaded with oapiLoadMesh()
			hMesh = vessel->CopyMeshFromTemplate(idx);
			if (hMesh) {
				// load on the fly and discard after copying
				meshlist[idx].mesh = new D3D9Mesh(hMesh);								// Create new DX9 Mesh
				meshlist[idx].mesh->SetClass(vClass);
				meshlist[idx].mesh->SetName(idx);
				oapiDeleteMesh(hMesh);
			}
		}

		if (meshlist[idx].mesh) {
			meshlist[idx].vismode = vessel->GetMeshVisibilityMode(idx);
			vessel->GetMeshOffset(idx, ofs);
			LogAlw("Mesh(%s) Offset = (%g, %g, %g)", _PTR(hMesh), ofs.x, ofs.y, ofs.z);
			if (length(ofs)) {
				meshlist[idx].trans = new D3DXMATRIX;
				D3DMAT_Identity(meshlist[idx].trans);
				D3DMAT_SetTranslation(meshlist[idx].trans, &ofs);
				// currently only mesh translations are supported
			}
		}
		else {
			LogWrn("Vessel %s has a NULL mesh in index %u",vessel->GetClassNameA(),idx);
		}
	}

	UpdateBoundingBox();

	LogOk("Loaded %u meshed for %s",nmesh,vessel->GetClassNameA());
}


// ============================================================================================
//
void vVessel::InsertMesh(UINT idx)
{
	_TRACE;

	VECTOR3 ofs=_V(0,0,0);

	UINT i;
	LPD3DXMATRIX pT = NULL;

	if (idx >= nmesh) { // append a new entry to the list
		MESHREC *tmp = new MESHREC[idx+1];
		if (nmesh) {
			memcpy (tmp, meshlist, nmesh*sizeof(MESHREC));
			delete []meshlist;
		}
		meshlist = tmp;
		for (i = nmesh; i <= idx; i++) { // zero any intervening entries
			meshlist[i].mesh = 0;
			meshlist[i].trans = 0;
			meshlist[i].vismode = 0;
		}
		nmesh = idx+1;
	}
	else if (meshlist[idx].mesh) { // replace existing entry
		SAFE_DELETE(meshlist[idx].mesh);
		SAFE_DELETE(meshlist[idx].trans);
	}

	// now add the new mesh
	MeshManager *mmgr = gc->GetMeshMgr();
	MESHHANDLE hMesh = vessel->GetMeshTemplate(idx);
	const D3D9Mesh *mesh = mmgr->GetMesh(hMesh);


	if (hMesh && mesh) {
		meshlist[idx].mesh = new D3D9Mesh(hMesh, *mesh);								// Create new Instance from an existing mesh template
		meshlist[idx].mesh->SetClass(vClass);
		meshlist[idx].mesh->SetName(idx);
	} else if (hMesh = vessel->CopyMeshFromTemplate (idx)) {	
		meshlist[idx].mesh = new D3D9Mesh(hMesh);										// Create new DX9 Mesh
		meshlist[idx].mesh->SetClass(vClass);
		meshlist[idx].mesh->SetName(idx);
		oapiDeleteMesh (hMesh);
	} else {
		meshlist[idx].mesh = 0;
	}

	if (meshlist[idx].mesh) {
		pMatMgr->ApplyConfiguration(meshlist[idx].mesh);
		meshlist[idx].vismode = vessel->GetMeshVisibilityMode (idx);
		vessel->GetMeshOffset (idx, ofs);
		if (length(ofs)) {
			meshlist[idx].trans = new D3DXMATRIX;
			D3DMAT_Identity (meshlist[idx].trans);
			D3DMAT_SetTranslation (meshlist[idx].trans, &ofs);
			// currently only mesh translations are supported
		} else {
			meshlist[idx].trans = 0;
		}
	}

	UpdateAnimations(idx);
}


// ============================================================================================
// In response to VESSEL::MeshModified()
//
void vVessel::ResetMesh(UINT idx)
{
	VECTOR3 ofs = _V(0, 0, 0);

	if ((idx < nmesh) && meshlist[idx].mesh) {

		MESHHANDLE hMesh = vessel->GetMeshTemplate(idx);

		if (hMesh) {
			meshlist[idx].mesh->ReLoadMeshFromHandle(hMesh);
			meshlist[idx].mesh->ResetTransformations();
		}
		else {
			hMesh = vessel->CopyMeshFromTemplate(idx);
			if (hMesh) {
				meshlist[idx].mesh->ReLoadMeshFromHandle(hMesh);
				meshlist[idx].mesh->ResetTransformations();
				oapiDeleteMesh(hMesh);
			}
		}

		pMatMgr->ApplyConfiguration(meshlist[idx].mesh);

		meshlist[idx].vismode = vessel->GetMeshVisibilityMode(idx);
		vessel->GetMeshOffset(idx, ofs);

		if (length(ofs)) {
			if (!meshlist[idx].trans) meshlist[idx].trans = new D3DXMATRIX;
			D3DMAT_Identity(meshlist[idx].trans);
			D3DMAT_SetTranslation(meshlist[idx].trans, &ofs);
		}
		else {
			SAFE_DELETE(meshlist[idx].trans);
		}
	}
}


// ============================================================================================
//
void vVessel::DisposeMeshes()
{
	if (nmesh && meshlist) {
		for (UINT i = 0; i < nmesh; i++) {
			SAFE_DELETE(meshlist[i].mesh);
			SAFE_DELETE(meshlist[i].trans);
		}
	}
	if (meshlist) delete[] meshlist;
	meshlist = 0;
	nmesh = 0;
}


// ============================================================================================
//
void vVessel::DelMesh(UINT idx)
{
	if (idx==0xFFFFFFFF) {
		DisposeMeshes();
		return;
	}

	if (idx >= nmesh) return;
	if (!meshlist[idx].mesh) return;

	SAFE_DELETE(meshlist[idx].mesh);
	SAFE_DELETE(meshlist[idx].trans);
}


// ============================================================================================
//
void vVessel::InitNewAnimation (UINT idx)
{
	//vessel->GetAnimPtr(&anim) returns invalid data here. New idx is not yet included in anim[]
}


// ============================================================================================
//
void vVessel::GrowAnimstateBuffer (UINT newSize)
{
	// Obsolete
}


// ============================================================================================
//
void vVessel::DisposeAnimations ()
{
	defstate.clear();
	applyanim.clear();
	currentstate.clear();
}


// ============================================================================================
//
void vVessel::ResetAnimations (UINT reset/*=1*/)
{
	bBSRecompute = true;
	//Nothing to do here
}


// ============================================================================================
//
void vVessel::DelAnimation (UINT idx)
{
	// Orbiter never reduces the animation buffer size. (i.e. anim[])
	// VESSEL::GetAnimPtr() returns highest existing animation ID + 1, not the actual animation count
	vessel->GetAnimPtr(&anim);
	currentstate.erase(idx);
	if (Config->bAbsAnims) for (UINT k = 0; k < anim[idx].ncomp; ++k) DeleteDefaultState(anim[idx].comp[k]);
}


// ============================================================================================
//
void vVessel::UpdateAnimations (int mshidx)
{
	
	UINT na = vessel->GetAnimPtr(&anim);

	
	// Check that all animations exists in local databases, if not then add it.
	// New animations 'should' be in their default states (at)in this point.
	//
	for (UINT i = 0; i < na; ++i) {

		if (currentstate.count(i) == 0) currentstate[i] = anim[i].defstate;

		if (Config->bAbsAnims) {
			for (UINT k = 0; k < anim[i].ncomp; ++k) {
				ANIMATIONCOMP *AC = anim[i].comp[k];
				if (defstate.count(AC->trans) == 0) StoreDefaultState(AC);
			}
		}
	}


	if (Config->bAbsAnims) 
	{

		// --------------------------------------------
		// Apply Absolute Animations
		// --------------------------------------------

		// Restore default transformations
		for (UINT i = 0; i < nmesh; ++i) if (meshlist[i].mesh) meshlist[i].mesh->ResetTransformations();

		// Restore default animation states 
		for (UINT i = 0; i < na; ++i) {
			currentstate[i] = anim[i].defstate;
			for (UINT k = 0; k < anim[i].ncomp; ++k) {
				if (anim[i].state != anim[i].defstate)
					RestoreDefaultState(anim[i].comp[k]);
			}
		}

		for (UINT i = 0; i < na; ++i) {
			if (!anim[i].ncomp) continue;
			if (applyanim.count(i)) continue;
			if (anim[i].state != anim[i].defstate) applyanim.insert(applyanim.end(), i);
		}

		// Update animations ---------------------------------------------
		for (auto i : applyanim) Animate(i, mshidx);
	}
	else 
	{

		// --------------------------------------------
		// Apply Incremental Animations
		// --------------------------------------------

		for (UINT i = 0; i < na; ++i) {
			if (anim[i].state != currentstate[i]) {
				Animate(i, mshidx);
				currentstate[i] = anim[i].state;
			}
		}
	}
}

// ============================================================================================
//
bool vVessel::GetSMapRenderData(SMI type, int idx, SMapInput *sm)
{
	D3DXVECTOR3 cpos; float rad;

	if (type == SMI::Visual) {
		*sm = { GetBoundingSpherePosDX(), FVECTOR3(-sundir), GetBoundingSphereRadius() };
		return true;
	}
	if (type == SMI::VC) {
		bool bRet = GetVCPos(&cpos, NULL, &rad);
		*sm = { cpos, FVECTOR3(-sundir), rad };
		return bRet;
	}
	if (type == SMI::Mesh) {
		bool bRet = GetMeshPosition(idx, &cpos, nullptr, &rad);
		*sm = { cpos, FVECTOR3(-sundir), rad };
		return bRet;
	}
	return false;
}

// ============================================================================================
//
bool vVessel::IsInsideShadows(const SMapInput* shd)
{
	D3DXVECTOR3 bc;
	D3DXVec3TransformCoord(&bc, ptr(D3DXVECTOR3f4(BBox.bs)), &mWorld);
	FVECTOR3 fbc = FVECTOR3(bc) - shd->pos;
	float x = dot(fbc, shd->ld);
	if (sqrt(dot(fbc, fbc) - x*x) < ((shd->rad * 1.01f) - BBox.bs.w)) return true;
	return false;
}


// ============================================================================================
//
bool vVessel::IntersectShadowVolume(const SMapInput* shd)
{
	D3DXVECTOR3 bc;
	D3DXVec3TransformCoord(&bc, ptr(D3DXVECTOR3f4(BBox.bs)), &mWorld);
	FVECTOR3 fbc = FVECTOR3(bc) - shd->pos;
	float x = dot(fbc, shd->ld);
	if (sqrt(dot(fbc, fbc) - x*x) > (shd->rad + BBox.bs.w)) return false;
	return true;
}


// ============================================================================================
//
bool vVessel::IntersectShadowTarget(const SMapInput* shd)
{
	D3DXVECTOR3 bc;
	D3DXVec3TransformCoord(&bc, ptr(D3DXVECTOR3f4(BBox.bs)), &mWorld);
	FVECTOR3 fbc = FVECTOR3(bc) - shd->pos;
	if (length(fbc) < (shd->rad + BBox.bs.w)) return true;
	return false;
}


// ============================================================================================
//
void vVessel::GetMinMaxLightDist(const SMapInput* shd, float *mind, float *maxd)
{
	D3DXVECTOR3 bc;
	D3DXVec3TransformCoord(&bc, ptr(D3DXVECTOR3f4(BBox.bs)), &mWorld);
	FVECTOR3 fbc = FVECTOR3(bc) - shd->pos;
	float x = dot(fbc, shd->ld);
	*mind = min(*mind, x - BBox.bs.w);
	*maxd = max(*maxd, x + BBox.bs.w);
}


// ============================================================================================
//
bool vVessel::GetVCPos(D3DXVECTOR3* cpos, D3DXVECTOR3* lpos, float* rad)
{
	for (int i = 0; i < nmesh; i++)
	{
		if (!meshlist[i].mesh) continue;
		if (meshlist[i].mesh->MeshFlags & MESHFLAG_VC)
		{
			return GetMeshPosition(i, cpos, lpos, rad);
		}	
	}
	return false;
}


// ============================================================================================
//
bool vVessel::GetMeshPosition(int idx, D3DXVECTOR3* cpos, D3DXVECTOR3* lpos, float* rad)
{
	if (!meshlist[idx].mesh) return false;

	D3DXVECTOR3 pos = meshlist[idx].mesh->GetBoundingSpherePos();
	if (rad) *rad = meshlist[idx].mesh->GetBoundingSphereRadius();

	if (meshlist[idx].trans)
	{
		D3DXVec3TransformCoord(&pos, &pos, meshlist[idx].trans);
		if (lpos) *lpos = pos;
		if (cpos) D3DXVec3TransformCoord(&pos, &pos, &mWorld);
	}
	else {
		if (lpos) *lpos = pos;
		if (cpos) D3DXVec3TransformCoord(&pos, &pos, &mWorld);
	}
	if (cpos) *cpos = pos;
	return true;
}


// ============================================================================================
//
void vVessel::BakeLights(ImageProcessing *pBaker)
{
	OBJHANDLE hGRef = vessel->GetGravityRef();

	MATRIX3 grot; VECTOR3 rpos; LVLH lvlh;
	oapiGetRotationMatrix(hGRef, &grot);
	vessel->GetRelativePos(hGRef, rpos);

	FMATRIX4 mW(mWorld);
	FVECTOR3 polaraxis = mul(grot, _V(0, 1, 0));
	lvlh.Up = unit(rpos);
	lvlh.East = unit(cross(polaraxis, lvlh.Up));
	lvlh.North = unit(cross(lvlh.Up, lvlh.East));
	lvlh.Up = tmul(FVECTOR4(lvlh.Up, 0), mW).xyz;
	lvlh.East = tmul(FVECTOR4(lvlh.East, 0), mW).xyz;
	lvlh.North = tmul(FVECTOR4(lvlh.North, 0), mW).xyz;

	auto maps = GetExteriorEnvMap();

	if (maps) {
		for (int i = 0; i < nmesh; i++)
		{
			if (!meshlist[i].mesh) continue;
			if (meshlist[i].vismode & MESHVIS_VC) // TODO:  Should check Shader type
			{
				auto vSun = tmul(FVECTOR4(sundir, 0), mW);
				if (bMustRebake) meshlist[i].mesh->BakeLights(pBaker, BakedLightsControl);
				if (Config->ExpVCLight == 0) meshlist[i].mesh->BakeAO(pBaker, vSun.xyz, lvlh, maps->pIrrad);
			}
		}
	}

	bMustRebake = false;
}


// ============================================================================================
//
void vVessel::ErrorOnce(Errors e)
{
	static bool bDisp[Errors::ITEMS] = { true };
	static DWORD dwDisp[Errors::ITEMS] = { 0 };

	if (bDisp[e]) {
		switch (e) {
		case Errors::NoVC:
			oapiWriteLogV("[ERROR:D3D9] Cannot identify Virtual Cockpit mesh [%s]", vessel->GetClassNameA());
			break;
		default:
			oapiWriteLogV("[ERROR:D3D9] Unknows error code [%s]", vessel->GetName());
		}
		dwDisp[e]++; // Count the errors;
		bDisp[e] = false; // Disable error from printing again
	}
}

// ============================================================================================
//
bool vVessel::Render(LPDIRECT3DDEVICE9 dev)
{
	_TRACE;
	if (!active) return false;
	UpdateBoundingBox();
	bool bRet = Render(dev, false, nullptr);
	if (oapiCameraInternal()==false) RenderReentry(dev);
	return bRet;
}

// ============================================================================================
//
bool vVessel::Render(LPDIRECT3DDEVICE9 dev, bool internalpass, const SHADOWMAP* shd)
{
	bool bCockpit = (oapiCameraInternal() && (hObj == oapiGetFocusObject()));
	bool bVC = (bCockpit && (oapiCockpitMode() == COCKPIT_VIRTUAL));

	DWORD flags = 0;
	flags |= bCockpit ? Render::D2 : 0;
	flags |= bVC ? Render::VC : 0;
	flags |= internalpass ? Render::IP : 0;

	return Render(dev, shd, flags);
}

// ============================================================================================
//
bool vVessel::Render(LPDIRECT3DDEVICE9 dev, const SHADOWMAP *shd, DWORD flg)
{
	_TRACE;
	if (!active) return false;

	UINT i, mfd;

	g_pCurrentVisual = this; // Set current visual for mesh debugger
	DWORD flags = *(DWORD*)gc->GetConfigParam(CFGPRM_GETDEBUGFLAGS);
	DWORD displ = *(DWORD*)gc->GetConfigParam(CFGPRM_GETDISPLAYMODE);

	bool bCockpit = (flg & Render::D2) | (flg & Render::VC);
	bool bVC = (flg & Render::VC);
	bool bInternal = (flg & Render::IP);

	if (scn->GetRenderPass() == RENDERPASS_CUSTOMCAM) bCockpit = bVC = false;
	// Always render exterior view for custom cams


	static VCHUDSPEC hudspec_;
	const VCHUDSPEC* hudspec = &hudspec_;
	static bool gotHUDSpec(false);
	const VCMFDSPEC* mfdspec[MAXMFD] = { NULL };

	HR(D3D9Effect::FX->SetBool(D3D9Effect::eEnvMapEnable, false));

	if (shd && shd->IsValid())
	{
		float s = float(shd->size);
		float sr = 2.0f * shd->rad / s;
		D3DXVECTOR4 sh = D3DXVECTOR4(sr, 1.0f / s, float(oapiRand()), 1.0f / shd->depth);
		D3DXVECTOR4 px = D3DXVECTOR4(shd->SubPx[0], shd->SubPx[1], shd->SubPx[2], 0.0f);
		HR(D3D9Effect::FX->SetMatrix(D3D9Effect::eLVP, shd->mLVP.toCDX())); // Cascade 0 for all
		HR(D3D9Effect::FX->SetVector(D3D9Effect::eSHD, &sh));
		HR(D3D9Effect::FX->SetVector(D3D9Effect::eSHDPx, &px));
		HR(D3D9Effect::FX->SetBool(D3D9Effect::eShadowToggle, true));
		D3D9Mesh::SetShadows(shd);
	}
	else {
		D3D9Mesh::SetShadows(NULL);
		HR(D3D9Effect::FX->SetBool(D3D9Effect::eShadowToggle, false));
	}

	// Check VC MFD screen resolutions ------------------------------------------------
	//
	if (bVC && bInternal) {
		for (mfd = 0; mfd < MAXMFD; mfd++) gc->GetVCMFDSurface(mfd, &mfdspec[mfd]);
		gotHUDSpec = !!gc->GetVCHUDSurface(&hudspec);
	}


	// Initialize MeshShader constants
	//
	MeshShader::ps_const.Cam_X = *scn->GetCameraX();
	MeshShader::ps_const.Cam_Y = *scn->GetCameraY();
	MeshShader::ps_const.Cam_Z = *scn->GetCameraZ();


	// Render Exterior and Interior (VC) meshes --------------------------------------------
	//
	for (i=0;i<nmesh;i++)
	{
		auto pMesh = meshlist[i].mesh;
		if (!pMesh) continue;

		g_uCurrentMesh = i; // Used for debugging

		// check if mesh should be rendered in this pass
		WORD vismode = meshlist[i].vismode;

		if (DebugControls::IsActive()) if (displ>1) vismode = MESHVIS_ALWAYS;

		if (scn->GetRenderPass() != RENDERPASS_VC_SHADOWMAP)
		{
			if (vismode == 0) continue;
			if (bInternal == false) {
				if (vismode == MESHVIS_VC) continue; // Added 3-jan-2011 to prevent VC interior double rendering during exterior and interior passes
				if ((vismode & MESHVIS_EXTPASS) == 0 && bCockpit) continue;
			}
			if (bCockpit) {
				if (bInternal && (vismode & MESHVIS_EXTPASS)) continue;
				if (!(vismode & MESHVIS_COCKPIT)) {
					if ((!bVC) || (!(vismode & MESHVIS_VC))) continue;
				}
			}
			else {
				if (!(vismode & MESHVIS_EXTERNAL)) continue;
			}
		}

		WORD Shader = pMesh->GetDefaultShader();
		DWORD mFlags = pMesh->MeshFlags;

		D3DXMATRIX mWT;
		LPD3DXMATRIX pWT;
		
		if (meshlist[i].trans) pWT = D3DXMatrixMultiply(&mWT, (const D3DXMATRIX*)meshlist[i].trans, &mWorld);
		else pWT = &mWorld;
		
		if (bVC && bInternal && (pMesh->GetDefaultShader() == SHADER_LEGACY)) {
			D3D9Sun local = sunLight;
			local.Color *= 0.5f;
			pMesh->SetSunLight(&local);
		}
		else pMesh->SetSunLight(&sunLight);


		if (bVC && bInternal)
		{
			for (mfd=0;mfd<MAXMFD;mfd++) {
				if (mfdspec[mfd] && mfdspec[mfd]->nmesh == i) {
					pMesh->SetMFDScreenId(mfdspec[mfd]->ngroup, 1 + mfd);
				}
			}
			if (gotHUDSpec) {
				if (hudspec->nmesh == i) {
					pMesh->SetMFDScreenId(hudspec->ngroup, 0x100);
				}
			}
		}

		const LPD3DXMATRIX pVP = scn->GetProjectionViewMatrix();
		const LPD3DXMATRIX pLVP = shd ? (const LPD3DXMATRIX)&shd->mLVP : nullptr;

		// Render vessel meshes --------------------------------------------------------------------------
		//
		if (scn->GetRenderPass() == RENDERPASS_VC_SHADOWMAP)
		{
			if (mFlags & MESHFLAG_SHADOW_VC)
				pMesh->RenderShadowMap(pWT, pLVP, 0, bVC);
		}
		else if (scn->GetRenderPass() == RENDERPASS_SHADOWMAP)
		{
			pMesh->RenderShadowMap(pWT, pLVP, 0, bVC);
		}
		else if (scn->GetRenderPass() == RENDERPASS_NORMAL_DEPTH)
		{
			pMesh->RenderShadowMap(pWT, pVP, 1);
		}
		else
		{
			auto ec = GetEnvCam(EnvCamType::Interior, 0);
			bool bSL = false;
			if (shd) bSL = (shd->tp == SHADOWMAP::sMapType::SingleLod);

			if (Shader != SHADER_LEGACY)
			{
				if (mFlags & MESHFLAG_VC) {
					if (ec) {
						float f = ec->da_force * 60.0f;
						float b = ec->da_bounch * 2.0f;
						float c = ec->da_curve * 3.0f;
						if (scn->GetRenderPass() != RENDERPASS_MAINSCENE)					
							D3D9Effect::FX->SetValue(D3D9Effect::eVCIrrad, &D3DXVECTOR4(b, b, b, 1.0f), sizeof(D3DXVECTOR4));
						else
							D3D9Effect::FX->SetValue(D3D9Effect::eVCIrrad, &D3DXVECTOR4(f, f, f, c), sizeof(D3DXVECTOR4));
					}
					if (bSL) pMesh->Render(pWT, ec, RENDER_VESSEL);
					else pMesh->Render(pWT, ec, RENDER_VC);
				}
				else pMesh->Render(pWT, GetExteriorEnvMap(), RENDER_VESSEL);
			}
			else {
				if (bInternal) pMesh->Render(pWT, ec, RENDER_VC);
				else pMesh->Render(pWT, GetExteriorEnvMap(), RENDER_VESSEL);
			}
		}
	}

	// Shutdown shadows to prevent from causing problems
	HR(D3D9Effect::FX->SetBool(D3D9Effect::eShadowToggle, false));

	if (scn->GetRenderPass() == RENDERPASS_MAINSCENE) {
		if (DebugControls::IsActive()) {
			if (flags&DBG_FLAGS_SELVISONLY && this != DebugControls::GetVisual()) return true;
			if (flags&DBG_FLAGS_BOXES && !bInternal) {
				D3DXMATRIX id;
				D3D9Effect::RenderBoundingBox(&mWorld, D3DXMatrixIdentity(&id), &BBox.min, &BBox.max, ptr(D3DXVECTOR4(1, 0, 0, 0.75f)));
			}

			RenderLightCone(&mWorld);

			dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
			if (flags & DBG_FLAGS_VCZONES) RenderClickZones();
		}
	}

	HR(D3D9Effect::FX->SetBool(D3D9Effect::eEnvMapEnable, false))

	return true;
}


// ============================================================================================
//
void vVessel::RenderClickZones()
{
	std::list<VCClickZone> Zones;
	oapiVCGetAreaClickZones(&Zones);

	auto hPS = pRenderZone->GetPSHandle("cb");
	auto hVS = pRenderZone->GetVSHandle("cb");

	pRenderZone->Setup(nullptr, false, 1);

	struct {
		FVECTOR3 pt[4];
		FVECTOR4 color;
		FMATRIX4 mW;
		FMATRIX4 mVP;
		BOOL	 bSphere;
	} cb;

	cb.mW = mWorld;
	cb.mVP = scn->GetProjectionViewMatrix();

	cb.bSphere = false;
	for (auto& z : Zones)
	{
		if (z.mode == 2) { // Quadrilateral
			for (int i = 0; i < 4; i++) cb.pt[i] = z.pt[i];
			cb.color = FVECTOR4(1.0f, 1.0f, 0.0f, 1.0f);
			pRenderZone->SetPSConstants(hPS, &cb, sizeof(cb));
			pRenderZone->SetVSConstants(hVS, &cb, sizeof(cb));
			hStockMesh[D3D9SM_BOX]->RenderGroup(0);
		}
	}

	cb.bSphere = true;
	for (auto& z : Zones)
	{
		if (z.mode == 1) { // Spherical
			cb.pt[0] = z.cnt;
			cb.pt[1] = z.rad;
			cb.color = FVECTOR4(0.3f, 1.0f, 1.0f, 1.0f);
			pRenderZone->SetPSConstants(hPS, &cb, sizeof(cb));
			pRenderZone->SetVSConstants(hVS, &cb, sizeof(cb));
			hStockMesh[D3D9SM_SPHERE]->RenderGroup(0);
		}
	}
}


// ============================================================================================
//
void vVessel::RenderVectors (LPDIRECT3DDEVICE9 dev, D3D9Pad *pSkp)
{
	const double threshold = 0;//0.25; // threshold for forces to be drawn
	VECTOR3 vector;
	float lscale = 1e-3f;
	float alpha;
	double len = 1e-9f; // avoids division by zero if len is not updated

	DWORD bfvmode = *(DWORD*)gc->GetConfigParam(CFGPRM_FORCEVECTORFLAG);
	float sclset  = *(float*)gc->GetConfigParam(CFGPRM_FORCEVECTORSCALE);
	float scale   = float(size) / 50.0f;

	// -------------------------------------
	// Render Body Force Vectors

	if (bfvmode & BFV_ENABLE)
	{
		char label[64];
		bool bLog;

		if (bfvmode & BFV_LOGSCALE) bLog = true;
		else                         bLog = false;

		if (!bLog) {
			if (bfvmode & BFV_DRAG) { vessel->GetDragVector(vector); if (length(vector)>len) len = length(vector); }
			if (bfvmode & BFV_WEIGHT) {	vessel->GetWeightVector(vector); if (length(vector)>len) len = length(vector); }
			if (bfvmode & BFV_THRUST) {	vessel->GetThrustVector(vector); if (length(vector)>len) len = length(vector); }
			if (bfvmode & BFV_LIFT) { vessel->GetLiftVector(vector); if (length(vector)>len) len = length(vector); }
			if (bfvmode & BFV_TOTAL) { vessel->GetForceVector(vector); if (length(vector)>len) len = length(vector); }
			if (bfvmode & BFV_TORQUE) {	vessel->GetTorqueVector(vector); if (length(vector)>len) len = length(vector); }
			if (bfvmode & BFV_SIDEFORCE) { vessel->GetSideForceVector(vector); if (length(vector) > len) len = length(vector); }

			lscale = float(size * sclset / len);
		}
		else {
			lscale = float(size * sclset / 50.0);
		}

		alpha = *(float*)gc->GetConfigParam(CFGPRM_FORCEVECTOROPACITY);

		if (alpha > 1e-9) // skip all this when opacity is to small (ZEROish)
		{
			if (bfvmode & BFV_DRAG) {
				vessel->GetDragVector(vector);
				if (length(vector) > threshold) {
					RenderAxisVector(pSkp, ptr(D3DXCOLOR(1,0,0,alpha)), vector, lscale, scale, bLog);
					sprintf_s(label, 64, "D = %sN", value_string(length(vector)));
					RenderAxisLabel(pSkp, ptr(D3DXCOLOR(1,0,0,alpha)), vector, lscale, scale, label, bLog);
				}
			}

			if (bfvmode & BFV_WEIGHT) {
				vessel->GetWeightVector(vector);
				if (length(vector) > threshold) {
					RenderAxisVector(pSkp, ptr(D3DXCOLOR(1,1,0,alpha)), vector, lscale, scale, bLog);
					sprintf_s(label, 64, "G = %sN", value_string(length(vector)));
					RenderAxisLabel(pSkp, ptr(D3DXCOLOR(1,1,0,alpha)), vector, lscale, scale, label, bLog);
				}
			}

			if (bfvmode & BFV_THRUST) {
				vessel->GetThrustVector(vector);
				if (length(vector) > threshold) {
					RenderAxisVector(pSkp, ptr(D3DXCOLOR(0,0,1,alpha)), vector, lscale, scale, bLog);
					sprintf_s(label, 64, "T = %sN", value_string(length(vector)));
					RenderAxisLabel(pSkp, ptr(D3DXCOLOR(0,0,1,alpha)), vector, lscale, scale, label, bLog);
				}
			}

			if (bfvmode & BFV_LIFT) {
				vessel->GetLiftVector(vector);
				if (length(vector) > threshold) {
					RenderAxisVector(pSkp, ptr(D3DXCOLOR(0,1,0,alpha)), vector, lscale, scale, bLog);
					sprintf_s(label, 64, "L = %sN", value_string(length(vector)));
					RenderAxisLabel(pSkp, ptr(D3DXCOLOR(0,1,0,alpha)), vector, lscale, scale, label, bLog);
				}
			}

			if (bfvmode & BFV_TOTAL) {
				vessel->GetForceVector(vector);
				if (length(vector) > threshold) {
					RenderAxisVector(pSkp, ptr(D3DXCOLOR(1,1,1,alpha)), vector, lscale, scale, bLog);
					sprintf_s(label, 64, "F = %sN", value_string(length(vector)));
					RenderAxisLabel(pSkp, ptr(D3DXCOLOR(1,1,1,alpha)), vector, lscale, scale, label, bLog);
				}
			}

			if (bfvmode & BFV_TORQUE) {
				vessel->GetTorqueVector(vector);
				if (length(vector) > threshold) {
					RenderAxisVector(pSkp, ptr(D3DXCOLOR(1,0,1,alpha)), vector, lscale, scale, bLog);
					sprintf_s(label, 64, "M = %sNm", value_string(length(vector)));
					RenderAxisLabel(pSkp, ptr(D3DXCOLOR(1,0,1,alpha)), vector, lscale, scale, label, bLog);
				}
			}

			if (bfvmode & BFV_SIDEFORCE) {
				vessel->GetSideForceVector(vector);
				if (length(vector) > threshold) {
					RenderAxisVector(pSkp, ptr(D3DXCOLOR(0.0392, 0.6235, 0.4941, alpha)), vector, lscale, scale, bLog);
					sprintf_s(label, 64, "SF = %sN", value_string(length(vector)));
					RenderAxisLabel(pSkp, ptr(D3DXCOLOR(0.0392, 0.6235, 0.4941, alpha)), vector, lscale, scale, label, bLog);
				}
			}
		}
	}

	// -------------------------------------
	// Render Coordinate Axes
	vObject::RenderVectors(dev, pSkp);
}


// ============================================================================================
//
bool vVessel::RenderExhaust()
{
	ExhaustLength = 0.0f;
	if (!active) return false;

	DWORD nexhaust = vessel->GetExhaustCount();
	if (!nexhaust) return true; // nothing to do

	EXHAUSTSPEC es;
	MATRIX3 R;
	vessel->GetRotationMatrix(R);
	VECTOR3 cdir = tmul(R, cpos);

	for (DWORD i=0;i<nexhaust;i++) {
		if (vessel->GetExhaustLevel(i)==0.0) continue;
		vessel->GetExhaustSpec(i, &es);
		D3D9Effect::RenderExhaust(&mWorld, cdir, &es, defexhausttex);
		if (es.lsize>ExhaustLength) ExhaustLength = float(es.lsize);
	}
	return true;
}


// ============================================================================================
//
void vVessel::RenderBeacons(LPDIRECT3DDEVICE9 dev)
{
	if (nmesh < 1) return;
	DWORD idx = 0;
	const BEACONLIGHTSPEC *bls = vessel->GetBeacon(idx);
	if (!bls) return; // nothing to do
	bool need_setup = true;
	double simt = oapiGetSimTime();

	for (;bls; bls = vessel->GetBeacon(++idx)) {
		if (bls->active) {
			if (bls->period && (fmod(simt+bls->tofs, bls->period) > bls->duration))	continue;
			double size = bls->size;
			if (cdist > 50.0) size *= pow (cdist/50.0, bls->falloff);
			RenderSpot(dev, bls->pos, (float)size, *bls->col, false, bls->shape);
		}
	}
}


// ============================================================================================
//
void vVessel::RenderGrapplePoints (LPDIRECT3DDEVICE9 dev)
{
	if (!oapiGetShowGrapplePoints()) return; // nothing to do

	DWORD i;
	ATTACHMENTHANDLE hAtt;
	VECTOR3 pos, dir, rot;
	const float size = 0.25;
	const float alpha = 0.5;

	// Flash calculations
	static double lastTime = 0;
	static bool isOn = true;
	double simt = oapiGetSysTime();
	if (simt-lastTime > 0.5) // Flashing period (twice per second)
	{
		isOn = !isOn;
		lastTime = simt;
	}
	if (!isOn) return; // nothing to do

	const OBJHANDLE hVessel = vessel->GetHandle();

	// attachment points to parent
	for (i = 0; i < vessel->AttachmentCount(true); ++i)
	{
		hAtt = vessel->GetAttachmentHandle(true, i);
		vessel->GetAttachmentParams(hAtt, pos, dir, rot);
		D3D9Effect::RenderArrow(hVessel, &pos, &dir, &rot, size, ptr(D3DXCOLOR(1,0,0,alpha)));
	}

	// attachment points to children
	for (i = 0; i < vessel->AttachmentCount(false); ++i)
	{
		hAtt = vessel->GetAttachmentHandle(false, i);
		vessel->GetAttachmentParams(hAtt, pos, dir, rot);
		D3D9Effect::RenderArrow(hVessel, &pos, &dir, &rot, size, ptr(D3DXCOLOR(0,0.5,1,alpha)));
	}
}


// ============================================================================================
//
void vVessel::RenderGroundShadow(LPDIRECT3DDEVICE9 dev, OBJHANDLE hPlanet, float alpha)
{
	if (!bStencilShadow && scn->GetRenderPass() == RENDERPASS_MAINSCENE) return;
	if (Config->TerrainShadowing == 0) return;

	static const double eps = 1e-2;
	static const double shadow_elev_limit = 0.07;
	double d, alt, R;
	VECTOR3 pp, sd, pvr;
	oapiGetGlobalPos(hPlanet, &pp); // planet global pos
	vessel->GetGlobalPos(sd);       // vessel global pos
	pvr = sd - pp;                     // planet-relative vessel position
	d = length(pvr);                 // vessel-planet distance
	R = oapiGetSize(hPlanet);       // planet mean radius
	R += vessel->GetSurfaceElevation();
	alt = d - R;                       // altitude above surface
	if (alt*eps > vessel->GetSize()) return; // too high to cast a shadow

	normalise(sd);                  // shadow projection direction

									// calculate the intersection of the vessel's shadow with the planet surface
	double fac1 = dotp(sd, pvr);
	if (fac1 > 0.0) return;          // shadow doesn't intersect planet surface
	double csun = -fac1 / d;           // sun elevation above horizon
	if (csun < shadow_elev_limit) return;   // sun too low to cast shadow
	double arg = fac1*fac1 - (dotp(pvr, pvr) - R*R);
	if (arg <= 0.0) return;                 // shadow doesn't intersect with planet surface
	double a = -fac1 - sqrt(arg);

	MATRIX3 vR;
	vessel->GetRotationMatrix(vR);
	VECTOR3 sdv = tmul(vR, sd);     // projection direction in vessel frame
	VECTOR3 shp = sdv*a;             // projection point
	VECTOR3 hn, hnp = vessel->GetSurfaceNormal();
	vessel->HorizonInvRot(hnp, hn);

	// perform projections
	//double nr0 = dotp(hn, shp);
	float nr0 = float(-alt);
	double nd = dotp(hn, sdv);
	VECTOR3 sdvs = sdv / nd;

	D3DXVECTOR4 nrml = D3DXVECTOR4(float(hn.x), float(hn.y), float(hn.z), float(alt));
	
	// build shadow projection matrix
	D3DXMATRIX mProj, mProjWorld, mProjWorldShift;

	mProj._11 = 1.0f - (float)(sdvs.x*hn.x);
	mProj._12 = -(float)(sdvs.y*hn.x);
	mProj._13 = -(float)(sdvs.z*hn.x);
	mProj._14 = 0;
	mProj._21 = -(float)(sdvs.x*hn.y);
	mProj._22 = 1.0f - (float)(sdvs.y*hn.y);
	mProj._23 = -(float)(sdvs.z*hn.y);
	mProj._24 = 0;
	mProj._31 = -(float)(sdvs.x*hn.z);
	mProj._32 = -(float)(sdvs.y*hn.z);
	mProj._33 = 1.0f - (float)(sdvs.z*hn.z);
	mProj._34 = 0;
	mProj._41 = (float)(sdvs.x*nr0);
	mProj._42 = (float)(sdvs.y*nr0);
	mProj._43 = (float)(sdvs.z*nr0);
	mProj._44 = 1;

	D3DXMatrixMultiply(&mProjWorld, &mProj, &mWorld);

	float scale = (csun - shadow_elev_limit) * 25.0f;
	alpha = (1.0f - alpha) * saturate(scale);

	// project all vessel meshes. This should be replaced by a dedicated shadow mesh

	for (UINT i = 0; i<nmesh; i++) {

		if (meshlist[i].mesh == NULL) continue;
		if (!(meshlist[i].vismode & MESHVIS_EXTERNAL)) continue; // only render shadows for externally visible meshes
		if (meshlist[i].mesh->HasShadow() == false) continue;

		D3D9Mesh *mesh = meshlist[i].mesh;

		if (meshlist[i].trans) {
			VECTOR3 of;	
			vessel->GetMeshOffset(i, of);
			nrml.w += float(dotp(of, hn));	// Sift a local groung level
			D3DXMatrixMultiply(&mProjWorldShift, meshlist[i].trans, &mProjWorld);
			mesh->RenderStencilShadows(alpha, &mWorld, &mProjWorldShift, false, &nrml);
		}
		else mesh->RenderStencilShadows(alpha, &mWorld, &mProjWorld, false, &nrml);
	}
}


// ============================================================================================
// Return true if it's time to move to a next vessel
// false, if more rendereing is required here.
//
bool vVessel::ProcessEnvMaps(LPDIRECT3DDEVICE9 pDev, DWORD cnt, DWORD flags)
{
	bool bReflective = false;

	if (meshlist) {
		for (DWORD i = 0; i < nmesh; i++) {
			if (meshlist[i].mesh) {
				if (meshlist[i].mesh->IsReflective()) {
					bReflective = true;
					break;
				}
			}
		}
	}

	if (!bReflective) return true; // If none of the meshes are reflective then we are done
	
	// Render vessel specific map
	bool bRet = RenderENVMap(pDev, &ecDefExt, cnt, flags);

	if (bRet) ecDefExt.bRendered = false;

	return bRet;
}


// ============================================================================================
// Render Env Map, return 'true' if the map is completed
// 
bool vVessel::RenderENVMap(LPDIRECT3DDEVICE9 pDev, ENVCAMREC* ec, DWORD cnt, DWORD flags)
{
	LPDIRECT3DSURFACE9 pEnvDS = GetScene()->GetEnvDepthStencil();

	if (!pEnvDS) {
		LogErr("EnvDepthStencil doesn't exists");
		return true;
	}

	// Create a EnvMap if doesn't already exists --------------------------------------------------------------------
	//
	if (ec->flags & ENVCAM_PLANE)
	{
		if (!ec->pPlane)
		{
			D3DSURFACE_DESC desc;
			pEnvDS->GetDesc(&desc);
			if (D3DXCreateTexture(pDev, desc.Width, desc.Height, 5, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &ec->pPlane) != S_OK) {
				LogErr("Failed to create env-plane for visual %s", _PTR(this));
				return true;
			}
		}
	}
	else {
		if (!ec->pCube) // If the maps exists check the type and assign
		{
			D3DSURFACE_DESC desc;
			pEnvDS->GetDesc(&desc);
			if (D3DXCreateCubeTexture(pDev, desc.Width, 5, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &ec->pCube) != S_OK) {
				LogErr("Failed to create env-cubemap for visual %s", _PTR(this));
				return true;
			}
		}
	}


	// Create a Irradiance map if doesn't already exists --------------------------------------------------------------------
	//
	if (!ec->pIrrad)
	{	
		if (D3DXCreateTexture(pDev, 128, 64, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &ec->pIrrad) != S_OK) {
			LogErr("Failed to create irradiance map for visual %s", _PTR(this));
			return true;
		}
	}


	// Create blurred maps and irradiance ---------------------------------------------------------------------
	//
	if (ec->bRendered) {
		if (ec->pCube) {
			scn->RenderBlurredMap(pDev, ec->pCube);
			scn->IntegrateIrradiance(this, ec, false);
		}
		// TODO: Blur 2D plane map
		return true;
	}


	// Render EnvMaps ---------------------------------------------------------------------------------------
	//
	std::set<vVessel*> RndList = scn->GetVessels(10e3, true);
	std::set<vVessel*> AddLightSrc;

	AddLightSrc.insert(this);

	if ((ec->flags & ENVCAM_FOCUS) == 0) RndList.erase(this);

	DWORD nAtc = vessel->AttachmentCount(false);
	DWORD nDoc = vessel->DockCount();

	if (ec->flags & ENVCAM_OMIT_ATTC) {
		for (DWORD i = 0; i < nAtc; i++) {
			ATTACHMENTHANDLE hAtc = vessel->GetAttachmentHandle(false, i);
			if (hAtc) {
				OBJHANDLE hAtcObj = vessel->GetAttachmentStatus(hAtc);
				if (hAtcObj) {
					vObject* vObj = gc->GetScene()->GetVisObject(hAtcObj);
					if (vObj) RndList.erase((vVessel*)vObj);
				}
			}
		}
	}
	else {
		for (auto id : ec->omitAttc) {
			ATTACHMENTHANDLE hAtc = vessel->GetAttachmentHandle(false, id);
			if (hAtc) {
				OBJHANDLE hAtcObj = vessel->GetAttachmentStatus(hAtc);
				if (hAtcObj) {
					vObject* vObj = gc->GetScene()->GetVisObject(hAtcObj);
					if (vObj) RndList.erase((vVessel*)vObj);
				}
			}
		}
	}


	// -----------------------------------------------------------------------------------------------
	//
	VECTOR3 gpos;
	vessel->Local2Global(_V(ec->lPos.x, ec->lPos.y, ec->lPos.z), gpos);

	if (ec->pCube)
	{
		// Prepare camera and scene for env map rendering
		scn->PushCamera();
		scn->SetupInternalCamera(NULL, &gpos, 0.7853981634, 1.0);
		scn->BeginPass(RENDERPASS_ENVCAM);
		gc->PushRenderTarget(NULL, pEnvDS, RENDERPASS_ENVCAM);

		D3DXMATRIX mEnv;
		D3DXVECTOR3 dir, up;
		LPDIRECT3DSURFACE9 pSrf = NULL;

		for (DWORD i = 0; i < cnt; i++) {

			HR(ec->pCube->GetCubeMapSurface(D3DCUBEMAP_FACES(ec->iSide), 0, &pSrf));

			gc->AlterRenderTarget(pSrf, pEnvDS);

			EnvMapDirection(ec->iSide, &dir, &up);

			D3DXVECTOR3 cp;
			D3DXVec3Cross(&cp, &up, &dir);
			D3DXVec3Normalize(&cp, &cp);
			D3DXMatrixIdentity(&mEnv);
			D3DMAT_FromAxis(&mEnv, &cp, &up, &dir);

			scn->SetCameraFrustumLimits(0.25, 1e8);
			scn->SetupInternalCamera(&mEnv, NULL, 0.7853981634, 1.0);
			scn->RenderSecondaryScene(RndList, AddLightSrc, flags);

			SAFE_RELEASE(pSrf);

			ec->iSide++;

			if (ec->iSide >= 6) {
				ec->bRendered = true;
				ec->iSide = 0;
				break;
			}
		}

		gc->PopRenderTargets();
		scn->PopPass();
		scn->PopCamera();

		return false;
	}

	if (ec->pPlane)
	{
		// TODO:
		return true;
	}

	return true;
}



// ============================================================================================
// Render interior Env Map, return 'true' if the map is completed
// 
bool vVessel::RenderInteriorENVMap(LPDIRECT3DDEVICE9 pDev, ENVCAMREC* ec, SHADOWMAP *sm)
{
	if (!ec) return true;

	LPDIRECT3DSURFACE9 pEnvDS = GetScene()->GetDepthStencil(128);

	if (!pEnvDS) {
		LogErr("RenderInteriorENVMap() DS doesn't exists");
		return true;
	}

	D3DSURFACE_DESC desc;
	pEnvDS->GetDesc(&desc);

	// Create a EnvMap if doesn't already exists --------------------------------------------------------------------
	//
	if (!ec->pCube) {
		if (D3DXCreateCubeTexture(pDev, desc.Width, 5, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &ec->pCube) != S_OK) {
			LogErr("Failed to create env-cubemap for visual %s", _PTR(this));
			return true;
		}
	}


	// Create a Irradiance map if doesn't already exists --------------------------------------------------------------------
	//
	if (!ec->pIrrad) {
		if (D3DXCreateTexture(pDev, 128, 64, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &ec->pIrrad) != S_OK) {
			LogErr("Failed to create irradiance map for visual %s", _PTR(this));
			return true;
		}
	}


	// Compute blur and irradiance --------------------------------------------------------------------------
	//
	if (ec->bRendered) {
		scn->RenderBlurredMap(pDev, ec->pCube);
		if (Config->ExpVCLight) scn->IntegrateIrradiance(this, ec, true);
		ec->bRendered = false;
		return true;
	}



	// Render EnvMaps ---------------------------------------------------------------------------------------
	//
	std::set<vVessel*> RndList;
	std::set<vVessel*> AddLightSrc; // empty

	RndList.insert(this);

	if (ec->pCube)
	{
		auto eStage = GetExteriorEnvMap();

		VECTOR3 gp;
		vessel->Local2Global(ec->lPos._V(), gp);

		//FVECTOR3 rpos = TransformCoord(ec->lPos, FMATRIX4(mWorld));
		//D3DXMATRIX mW, mWI;
		//D3DXMatrixIdentity(&mW);
		//D3DMAT_SetTranslation(&mW, &rpos._V());
		//D3DXMatrixInverse(&mWI, NULL, &mW);

		// Prepare camera and scene for env map rendering
		scn->PushCamera();
		scn->BeginPass(RENDERPASS_ENVCAM);
		scn->SetCameraFrustumLimits(0.1, 3e4);

		gc->PushRenderTarget(NULL, pEnvDS, RENDERPASS_ENVCAM);

		D3DXMATRIX mEnv;
		D3DXVECTOR3 dir, up;
		LPDIRECT3DSURFACE9 pSrf = NULL;

		for (DWORD i = 0; i < 6; i++)
		{
			HR(ec->pCube->GetCubeMapSurface(D3DCUBEMAP_FACES(i), 0, &pSrf));
			gc->AlterRenderTarget(pSrf, pEnvDS);
			EnvMapDirection(i, &dir, &up);
			D3DXVECTOR3 cp;
			D3DXVec3Cross(&cp, &up, &dir);
			D3DXVec3Normalize(&cp, &cp);
			D3DXMatrixIdentity(&mEnv);
			D3DMAT_FromAxis(&mEnv, &cp, &up, &dir);
			//D3DXMatrixMultiply(&mEnv, &mWI, &mEnv);
		
			//scn->CameraOffOrigin90(&mEnv, rpos);
			scn->SetupInternalCamera(&mEnv, &gp, 0.7853981634, 1.0);
			scn->RenderStageSet(eStage->pCube);

			SAFE_RELEASE(pSrf);
		}

		gc->PopRenderTargets();
		scn->PopPass();
		scn->PopCamera();

		ec->bRendered = true;
	}

	return false;
}


// ============================================================================================
// Return exterior env-map for a vessel, seek from a root if doesn't exists.
// Root must always have a map
//
ENVCAMREC*vVessel::GetExteriorEnvMap()
{
	if (ecDefExt.pCube) return &ecDefExt;
	else if (vRoot != this && vRoot) return vRoot->GetExteriorEnvMap();

	assert(Config->EnvMapMode != 0); // Should always return valid map if feature is enabled
	return nullptr;
}


// ============================================================================================
//
ENVCAMREC* vVessel::CreateEnvCam(EnvCamType ec, int idx)
{
	if (ec == EnvCamType::Interior)
	{
		if (idx >= MAX_INTCAM) return nullptr;
		auto x = new ENVCAMREC;
		x->type = ec;

		if (idx < 0) { 
			for (auto& c : InteriorCams) if (!c) { c = x; return x;	} // Use empty slot
			idx = 0; // No empty slot
		}
		SAFE_DELETE(InteriorCams[idx]);
		InteriorCams[idx] = x;
		return x;
	}
	if (ec == EnvCamType::Exterior) {
		return &ecDefExt;
	}
	return nullptr;
}


// ============================================================================================
//
bool vVessel::HasOwnEnvCam(EnvCamType ec)
{
	if (ec == EnvCamType::Exterior && (ecDefExt.flags & ENVCAM_USER)) return true;
	if (ec == EnvCamType::Interior) for (auto x : InteriorCams) if (x) return true;
	if (ec == EnvCamType::Mesh) {
		// TODO:
	}
	return false;
}

// ============================================================================================
//
ENVCAMREC* vVessel::GetEnvCam(EnvCamType ec, int idx)
{
	if (ec == EnvCamType::Exterior) return &ecDefExt;
	if (ec == EnvCamType::Interior) {
		if (idx >= MAX_INTCAM) return nullptr;
		if (idx < 0) {
			for (auto c : InteriorCams) if (c) return c; // Get any cam
		}
		else return InteriorCams[std::clamp(idx, 0, MAX_INTCAM - 1)];
	}
	if (ec == EnvCamType::Mesh) {
		// TODO:
	}
	return nullptr;
}

// ============================================================================================
//
bool vVessel::GetInteriorCams(std::list<ENVCAMREC*>* pCams)
{
	pCams->clear();
	for (auto x : InteriorCams) if (x) pCams->push_back(x);
	return !pCams->empty();
}


// ============================================================================================
//
bool vVessel::IsRoot() const
{
	return (vRoot == nullptr) | (vRoot == this);
}

// ============================================================================================
//
void vVessel::RenderLightCone(LPD3DXMATRIX pWT)
{
	if (DebugControls::sEmitter == 0) return;
	if (DebugControls::Emitters.count(DebugControls::sEmitter) == 0) return;

	DWORD ec = vessel->LightEmitterCount();
	const LightEmitter *se = DebugControls::Emitters[DebugControls::sEmitter];
	const LightEmitter *em = NULL;

	for (DWORD i = 0; i < ec; i++) if (vessel->GetLightEmitter(i) == se) { em = se; break; }

	if (!em) return;

	float P = 0.0f, U = 0.0f, R = 0.0f;

	if (em->GetType() == LightEmitter::LT_SPOT) {
		P = float(((const SpotLight *)em)->GetPenumbra());
		U = float(((const SpotLight *)em)->GetUmbra());
		R = float(((const SpotLight *)em)->GetRange());
	}
	if (em->GetType() == LightEmitter::LT_POINT) {
		R = float(((const SpotLight *)em)->GetRange());
		return;
	}

	VECTOR3 _P = em->GetPosition();
	VECTOR3 _D = em->GetDirection();

	if (em->GetType() == LightEmitter::LT_SPOT) {
		D3DXVECTOR3 Main[2];
		Main[0] = D3DXVEC(_P);
		Main[1] = D3DXVEC(_P + _D * R);
		WORD Idx[2] = { 0, 1 };
		D3D9Effect::RenderLines(Main, Idx, 2, 2, pWT, 0xFF00FF00);

		D3DXVECTOR3 Circle[65];
		WORD CIdx[130];

		VECTOR3 _X = crossp(_D, _V(0.4, 0.2, -0.6));
		VECTOR3 _Y = crossp(_D, _X);

		D3DXVECTOR3 _x = D3DXVEC(_X);
		D3DXVECTOR3 _y = D3DXVEC(_Y);
		D3DXVECTOR3 _d = D3DXVEC(_D);

		float q = tan(P*0.5f) * R;
		float a = 0.0f;
		for (int i = 0; i < 64; i++) {
			Circle[i] = _x * (cos(a)*q) + _y * (sin(a)*q) + _d * R + Main[0];
			a += float(PI2 / 63.0);
			CIdx[i * 2] = i;
			CIdx[i * 2 + 1] = i + 1;
		}
		D3D9Effect::RenderLines(Circle, CIdx, 64, 126, pWT, 0xFF00FF00);
	}	
}


// ============================================================================================
//
bool vVessel::ModLighting()
{
	tCheckLight = oapiGetSimTime();

	// we only test the closest celestial body for shadowing
	OBJHANDLE hP = vessel->GetSurfaceRef();
	if (hP==NULL) {	LogErr("Vessel's surface reference is NULL"); return false;	}

	vObject *vO = GetScene()->GetVisObject(hP);

	if (vO) {
		if (vO->Type() == OBJTP_PLANET) {
			vPlanet* vP = (vPlanet*)vO;	
			VECTOR3 rpos = gpos - vP->GlobalPos();
			sunLight = vP->GetObjectAtmoParams(rpos);
			return true;		
		}
	}
	
	DWORD ambient = *(DWORD*)gc->GetConfigParam(CFGPRM_AMBIENTLEVEL);
	sunLight.Ambient = float(ambient) * 0.0039f;
	sunLight.Transmission = 1.0f;
	sunLight.Incatter = 0.0f;
	sunLight.Color = 1.0f; // Config->GFXSunIntensity;
	sunLight.Dir = FVECTOR3(-sundir);
	
	return true;
}


// ============================================================================================
// Delete AC and all of it's children from local database
//
void vVessel::DeleteDefaultState(ANIMATIONCOMP *AC)
{
	defstate.erase(AC->trans);
	for (UINT i = 0; i < AC->nchildren; ++i) DeleteDefaultState(AC->children[i]);
}


// ============================================================================================
// Store AC and all of it's children to local database
//
void vVessel::StoreDefaultState(ANIMATIONCOMP *AC)
{
	// If Already exists then skip it
	if (defstate.count(AC->trans)) return;

	auto trans = AC->trans;
	_defstate def;

	switch (trans->Type()) {
	case MGROUP_TRANSFORM::NULLTRANSFORM:
		break;
	case MGROUP_TRANSFORM::ROTATE: {
		MGROUP_ROTATE *rot = (MGROUP_ROTATE*)trans;
		def.ref = rot->ref;
		def.vdata = unit(rot->axis);
		def.fdata = rot->angle;
	} break;
	case MGROUP_TRANSFORM::TRANSLATE: {
		MGROUP_TRANSLATE *lin = (MGROUP_TRANSLATE*)trans;
		def.vdata = lin->shift;
	} break;
	case MGROUP_TRANSFORM::SCALE: {
		MGROUP_SCALE *scl = (MGROUP_SCALE*)trans;
		def.ref = scl->ref;
		def.vdata = scl->scale;
	} break;
	}
	
	if (trans->mesh == LOCALVERTEXLIST) for (UINT j = 0; j < trans->ngrp; ++j) def.vtx.push_back(((VECTOR3 *)trans->grp)[j]);

	defstate[AC->trans] = def;

	for (UINT i = 0; i < AC->nchildren; ++i) StoreDefaultState(AC->children[i]);
}


// ============================================================================================
//
void vVessel::RestoreDefaultState(ANIMATIONCOMP *AC)
{
	auto trans = AC->trans;
	auto it = defstate.find(AC->trans);

	assert(it != defstate.end());

	if (trans->mesh == LOCALVERTEXLIST) { 
		VECTOR3 *vtx = (VECTOR3*)trans->grp;
		for (UINT i = 0; i < trans->ngrp; i++) vtx[i] = it->second.vtx[i];
	}

	switch (trans->Type()) {
	case MGROUP_TRANSFORM::NULLTRANSFORM:
		break;
	case MGROUP_TRANSFORM::ROTATE: {
		MGROUP_ROTATE *rot = (MGROUP_ROTATE*)trans;
		rot->ref = it->second.ref;
		rot->axis = it->second.vdata;
		rot->angle = it->second.fdata;
	} break;
	case MGROUP_TRANSFORM::TRANSLATE: {
		MGROUP_TRANSLATE *lin = (MGROUP_TRANSLATE*)trans;
		lin->shift = it->second.vdata;
	} break;
	case MGROUP_TRANSFORM::SCALE: {
		MGROUP_SCALE *scl = (MGROUP_SCALE*)trans;
		scl->ref = it->second.ref;
		scl->scale = it->second.vdata;
	} break;
	}

	for (UINT i = 0; i < AC->nchildren; ++i) RestoreDefaultState(AC->children[i]);
}


// ============================================================================================
//
void vVessel::Animate(UINT an, UINT mshidx)
{
	double s0, s1, ds;
	UINT i, ii;
	D3DXMATRIX T;
	ANIMATION *A = anim+an;

	for (ii = 0; ii < A->ncomp; ii++) {

		i = (A->state > currentstate[an] ? ii : A->ncomp-ii-1);
		ANIMATIONCOMP *AC = A->comp[i];

 		if ((mshidx != LOCALVERTEXLIST) && (mshidx != AC->trans->mesh)) continue;

		s0 = currentstate[an]; // current animation state in the visual
		if      (s0 < AC->state0) s0 = AC->state0;
		else if (s0 > AC->state1) s0 = AC->state1;
		s1 = A->state;           // required animation state
		if      (s1 < AC->state0) s1 = AC->state0;
		else if (s1 > AC->state1) s1 = AC->state1;
		if ((ds = (s1-s0)) == 0) continue; // nothing to do for this component
		ds /= (AC->state1 - AC->state0);   // stretch to range 0..1

		// Build transformation matrix
		switch (AC->trans->Type())
		{
			case MGROUP_TRANSFORM::NULLTRANSFORM:
			{
				D3DMAT_Identity (&T);
				AnimateComponent (AC, T);
			}	break;

			case MGROUP_TRANSFORM::ROTATE:
			{
				MGROUP_ROTATE *rot = (MGROUP_ROTATE*)AC->trans;
				D3DXVECTOR3 ax(float(rot->axis.x), float(rot->axis.y), float(rot->axis.z));
				D3DMAT_RotationFromAxis (ax, (float)ds*rot->angle, &T);
				float dx = D3DVAL(rot->ref.x), dy = D3DVAL(rot->ref.y), dz = D3DVAL(rot->ref.z);
				T._41 = dx - T._11*dx - T._21*dy - T._31*dz;
				T._42 = dy - T._12*dx - T._22*dy - T._32*dz;
				T._43 = dz - T._13*dx - T._23*dy - T._33*dz;
				AnimateComponent (AC, T);
			} break;

			case MGROUP_TRANSFORM::TRANSLATE:
			{
				MGROUP_TRANSLATE *lin = (MGROUP_TRANSLATE*)AC->trans;
				D3DMAT_Identity (&T);
				T._41 = (float)(ds*lin->shift.x);
				T._42 = (float)(ds*lin->shift.y);
				T._43 = (float)(ds*lin->shift.z);
				AnimateComponent (AC, T);
			} break;

			case MGROUP_TRANSFORM::SCALE:
			{
				MGROUP_SCALE *scl = (MGROUP_SCALE*)AC->trans;
				s0 = (s0-AC->state0)/(AC->state1-AC->state0);
				s1 = (s1-AC->state0)/(AC->state1-AC->state0);
				D3DMAT_Identity (&T);
				T._11 = (float)((s1*(scl->scale.x-1)+1)/(s0*(scl->scale.x-1)+1));
				T._22 = (float)((s1*(scl->scale.y-1)+1)/(s0*(scl->scale.y-1)+1));
				T._33 = (float)((s1*(scl->scale.z-1)+1)/(s0*(scl->scale.z-1)+1));
				T._41 = (float)scl->ref.x * (1.0f-T._11);
				T._42 = (float)scl->ref.y * (1.0f-T._22);
				T._43 = (float)scl->ref.z * (1.0f-T._33);
				AnimateComponent (AC, T);
			} break;
		}
	}
}


// ============================================================================================
//
void vVessel::AnimateComponent (ANIMATIONCOMP *comp, const D3DXMATRIX &T)
{
	UINT i;

	bBSRecompute = true;

	MGROUP_TRANSFORM *trans = comp->trans;

	if (trans->mesh == LOCALVERTEXLIST) { // transform a list of individual vertices
		VECTOR3 *vtx = (VECTOR3*)trans->grp;
		for (i = 0; i < trans->ngrp; i++) TransformPoint(vtx[i], T);
	}
	else { // transform mesh groups

		if (trans->mesh >= nmesh) return; // mesh index out of range
		D3D9Mesh *mesh = meshlist[trans->mesh].mesh;
		if (!mesh) return;

		if (trans->grp) { // animate individual mesh groups
			for (i=0;i<trans->ngrp;i++) mesh->TransformGroup(trans->grp[i], &T);
		}
		else {          // animate complete mesh
			mesh->Transform(&T);
		}
	}

	// recursively transform all child animations
	for (i = 0; i < comp->nchildren; i++) {

		ANIMATIONCOMP *child = comp->children[i];
		AnimateComponent (child, T);

		switch (child->trans->Type()) {

			case MGROUP_TRANSFORM::NULLTRANSFORM:
				break;

			case MGROUP_TRANSFORM::ROTATE: {
				MGROUP_ROTATE *rot = (MGROUP_ROTATE*)child->trans;
				TransformPoint (rot->ref, T);
				TransformDirection (rot->axis, T, true);
			} break;

			case MGROUP_TRANSFORM::TRANSLATE: {
				MGROUP_TRANSLATE *lin = (MGROUP_TRANSLATE*)child->trans;
				TransformDirection (lin->shift, T, false);
			} break;

			case MGROUP_TRANSFORM::SCALE: {
				MGROUP_SCALE *scl = (MGROUP_SCALE*)child->trans;
				TransformPoint (scl->ref, T);
				// we can't transform anisotropic scaling vector
			} break;
		}
	}
}


// ============================================================================================
//
void vVessel::SetVisualProperty(VisualProp prp, int idx, const type_info& t, const void* val)
{
	if (t == typeid(FVECTOR3))
	{
		FVECTOR3* v = (FVECTOR3*)val;

		if (prp == VisualProp::BAKED_LIGHT) {
			if (idx >= 0 && idx <= 15) {
				if (BakedLightsControl[idx] != *v) {
					BakedLightsControl[idx] = *v;
					bMustRebake = true;
				}
			}
			return;
		}
	
		if (prp == VisualProp::AMBIENT) {
			VCAmbient = *v;
			return;
		}

		if (prp == VisualProp::EXT_PROBE_POS) {
			ecDefExt.lPos = *v;
			ecDefExt.flags |= ENVCAM_USER;
			return;
		}
	
		if (prp == VisualProp::CREATE_VC_PROBE) {
			auto ec = GetEnvCam(EnvCamType::Interior, idx);
			if (!ec) ec = CreateEnvCam(EnvCamType::Interior, idx);
			ec->lPos = *v;
			oapiWriteLogV("-- [ CREATE_VC_PROBE ] --");
			return;
		}
	}

	if (t == typeid(float))
	{
		float* v = (float*)val;
		auto ec = GetEnvCam(EnvCamType::Interior, idx);
		if (ec) {
			if (prp == VisualProp::DA_CURVE) ec->da_curve = *v;
			if (prp == VisualProp::DA_BOUNCH) ec->da_bounch = *v;
			if (prp == VisualProp::DA_FORCE) ec->da_force = *v;
		}
	}

	oapiWriteLogV("Failed to set visual property prp=%d, type=[%s]", int(prp), t.name());
}


// ============================================================================================
//
bool vVessel::GetVisualProperty(VisualProp prp, int idx, const type_info& t, void* val)
{
	if (t == typeid(FVECTOR3))
	{
		if (prp == VisualProp::BAKED_LIGHT) if (idx >= 0 && idx <= 15) {
			*((FVECTOR3*)val) = BakedLightsControl[idx];
			return true;
		}

		if (prp == VisualProp::AMBIENT) {
			*((FVECTOR3*)val) = VCAmbient;
			return true;
		}
	}

	if (t == typeid(float))
	{
		auto ec = GetEnvCam(EnvCamType::Interior, idx);
		if (ec) {
			if (prp == VisualProp::DA_CURVE) *((float*)val) = ec->da_curve;
			if (prp == VisualProp::DA_BOUNCH) *((float*)val) = ec->da_bounch;
			if (prp == VisualProp::DA_FORCE) *((float*)val) = ec->da_force;
		}
	}

	oapiWriteLogV("Failed to get visual property prp=%d, type=[%s]", int(prp), t.name());
	return false;
}


// ============================================================================================
//
void vVessel::RenderReentry(LPDIRECT3DDEVICE9 dev)
{

	if (defreentrytex == NULL || nmesh < 1) return;

	double p = vessel->GetAtmDensity();
	double v = vessel->GetAirspeed();

	float lim  = 100000000.0;
	float www  = float(p*v*v*v);
	float ints = max(0.0f,(www-lim)) / (5.0f*lim);

	if (ints>1.0f) ints = 1.0f;
	if (ints<0.01f) return;

	VECTOR3 d;
	vessel->GetShipAirspeedVector(d);
	vessel->GlobalRot(d, d);
	normalise(d);

	float x = float(dotp(d, unit(cpos)));
	if (x<0) x=-x;	x=pow(x,0.3f);

	float alpha_B = (x*0.40f + 0.60f) * ints;
	float alpha_A = (1.0f - x*0.50f) * ints * 1.2f;

	float size = float(vessel->GetSize()) * 1.7f;

	D3DXVECTOR3 vPosA(float(cpos.x), float(cpos.y), float(cpos.z));
	D3DXVECTOR3 vDir(float(d.x), float(d.y), float(d.z));
	D3DXVECTOR3 vPosB = vPosA + vDir * (size*0.25f);

	D3D9Effect::RenderReEntry(defreentrytex, &vPosA, &vPosB, &vDir, alpha_A, alpha_B, size);
}


// ===========================================================================================
//
bool vVessel::GetMinMaxDistance(float *zmin, float *zmax, float *dmin)
{
	if (bBSRecompute) UpdateBoundingBox();

	D3DXMATRIX mWorldView, mWorldViewTrans, mTF;

	WORD vismode = MESHVIS_EXTERNAL | MESHVIS_EXTPASS;

	Scene *scn = gc->GetScene();

	D3DXVECTOR4 Field = D9LinearFieldOfView(scn->GetProjectionMatrix());

	D3DXMatrixMultiply(&mWorldView, &mWorld, scn->GetViewMatrix());

	for (DWORD i=0;i<nmesh;i++) {
		if (meshlist[i].vismode&vismode && meshlist[i].mesh) {
			if (meshlist[i].trans) {
				D3DXMatrixMultiply(&mWorldViewTrans, (const D3DXMATRIX *)meshlist[i].trans, &mWorldView);
				D9ComputeMinMaxDistance(gc->GetDevice(), meshlist[i].mesh->GetAABB(), &mWorldViewTrans, &Field, zmin, zmax, dmin);
			}
			else {
				D9ComputeMinMaxDistance(gc->GetDevice(), meshlist[i].mesh->GetAABB(), &mWorldView, &Field, zmin, zmax, dmin);
			}
		}
	}

	return true;
}

// ===========================================================================================
//
void vVessel::UpdateBoundingBox()
{
	if (nmesh==0) return;
	if (bBSRecompute==false) return;

	bBSRecompute = false;
	bool bFirst = true;

	WORD vismode = MESHVIS_EXTERNAL | MESHVIS_EXTPASS;

	D3DXMATRIX mTF;

	for (DWORD i=0;i<nmesh;i++) {

		D3DXVECTOR3 q,w;
		if (meshlist[i].mesh==NULL) continue;

		if (meshlist[i].vismode&vismode) {

			LPD3DXMATRIX pMeshTF = meshlist[i].mesh->GetTransform();
			LPD3DXMATRIX pTF = &mTF;

			if (meshlist[i].trans) {
				if (pMeshTF) D3DXMatrixMultiply(pTF, meshlist[i].trans, pMeshTF);
				else         pTF = meshlist[i].trans;
			} else {
				if (pMeshTF) pTF = pMeshTF;
				else         pTF = NULL;
			}

			D9AddAABB(meshlist[i].mesh->GetAABB(), pTF, &BBox, bFirst);
			bFirst = false;
		}
		else {
			meshlist[i].mesh->UpdateBoundingBox();
		}
	}

	DWORD nexhaust = vessel->GetExhaustCount();

	if (nexhaust) {
		EXHAUSTSPEC es;
		for (DWORD i=0;i<nexhaust;i++) {
			double lvl = vessel->GetExhaustLevel(i);
			if (lvl==0.0) continue;
			vessel->GetExhaustSpec(i, &es);
			VECTOR3 e = (*es.lpos) - (*es.ldir) * (es.lofs + es.lsize*lvl);
			D3DXVECTOR3 ext(float(e.x), float(e.y), float(e.z));
			D9AddPointAABB(&BBox, &ext);
		}

		for (DWORD i=0;i<nexhaust;i++) {
			vessel->GetExhaustSpec(i, &es);
			VECTOR3 r = (*es.lpos);
			D3DXVECTOR3 ref(float(r.x), float(r.y), float(r.z));
			D9AddPointAABB(&BBox, &ref);
		}
	}

	D9UpdateAABB(&BBox);
}

// ===========================================================================================
//
D3D9Pick vVessel::Pick(const D3DXVECTOR3 *vDir, const PickProp *p)
{
	D3DXMATRIX mWT;
	LPD3DXMATRIX pWT = NULL;

	DWORD flags = *(DWORD*)gc->GetConfigParam(CFGPRM_GETDEBUGFLAGS);
	DWORD displ = *(DWORD*)gc->GetConfigParam(CFGPRM_GETDISPLAYMODE);

	bool bCockpit = (oapiCameraInternal() && (hObj == oapiGetFocusObject()));
	bool bVC = (bCockpit && (oapiCockpitMode() == COCKPIT_VIRTUAL));

	D3D9Pick result;
	result.dist  = 1e30f;
	result.pMesh = NULL;
	result.vObj  = NULL;
	result.group = -1;
	result.idx = -1;

	if (!meshlist || nmesh==0) return result;

	for (DWORD i=0;i<nmesh;i++) {

		D3D9Mesh *hMesh = meshlist[i].mesh;

		if (!hMesh) continue;

		// check if mesh should be rendered in this pass
		WORD vismode = meshlist[i].vismode;

		if (DebugControls::IsActive() && (DebugControls::debugFlags & DBG_FLAGS_RENDEREXT))
		{
			vismode |= MESHVIS_EXTPASS;
		}

		if (vismode == 0) continue;

		bool bRender = false;

		if (bCockpit) {
			// Internal View
			if (vismode & MESHVIS_COCKPIT) bRender = true;
			if (vismode & MESHVIS_EXTPASS) bRender = true;
			if (bVC) {
				if (vismode & MESHVIS_VC) bRender = true;
			}
		}
		else {
			// External view
			if (vismode & MESHVIS_EXTERNAL) bRender = true;
		}

		if (!bRender) continue;

		D3D9Pick pick = hMesh->Pick(&mWorld, meshlist[i].trans, vDir, p);
		if (pick.pMesh) if (pick.dist<result.dist) result = pick;
	}

	if (result.pMesh) result.vObj = this;

	return result;
}

// ============================================================================================
//
int vVessel::GetMatrixTransform(gcCore::MatrixId func, DWORD mi, DWORD gi, FMATRIX4 *pMat)
{
	if (mi >= nmesh) return -1;
	D3D9Mesh *pMesh = meshlist[mi].mesh;
	if (pMesh == NULL) return -2;
	if (gi >= pMesh->GetGroupCount()) return -3;

	if (func == gcCore::MatrixId::MESH)	memcpy_s(pMat, sizeof(FMATRIX4), ptr(pMesh->GetTransform(-1, false)), sizeof(D3DXMATRIX));
	if (func == gcCore::MatrixId::GROUP) memcpy_s(pMat, sizeof(FMATRIX4), ptr(pMesh->GetTransform(gi, false)), sizeof(D3DXMATRIX));

	if (func == gcCore::MatrixId::OFFSET) {
		if (meshlist[mi].trans) memcpy_s(pMat, sizeof(FMATRIX4), meshlist[mi].trans, sizeof(D3DXMATRIX));
		else {
			D3DXMATRIX Ident;
			D3DXMatrixIdentity(&Ident);
			memcpy_s(pMat, sizeof(FMATRIX4), &Ident, sizeof(D3DXMATRIX));
		}
		return 0;
	}

	if (func == gcCore::MatrixId::COMBINED) {
		D3DXMATRIX MeshGrp = pMesh->GetTransform(gi, true);
		if (meshlist[mi].trans) {
			D3DXMATRIX MeshGrpTrans;
			D3DXMatrixMultiply(&MeshGrpTrans, &MeshGrp, meshlist[mi].trans);
			memcpy_s(pMat, sizeof(FMATRIX4), &MeshGrpTrans, sizeof(D3DXMATRIX));
		}
		else memcpy_s(pMat, sizeof(FMATRIX4), &MeshGrp, sizeof(D3DXMATRIX));
	}

	return 0;
}

// ============================================================================================
//
int vVessel::SetMatrixTransform(gcCore::MatrixId func, DWORD mi, DWORD gi, const FMATRIX4 *pMat)
{
	if (mi >= nmesh) return -1;
	D3D9Mesh *pMesh = meshlist[mi].mesh;
	if (pMesh == NULL) return -2;
	if (gi >= pMesh->GetGroupCount()) return -3;

	if (func == gcCore::MatrixId::OFFSET) {
		if (meshlist[mi].trans == NULL) meshlist[mi].trans = new D3DXMATRIX;
		memcpy_s(meshlist[mi].trans, sizeof(D3DXMATRIX), pMat, sizeof(FMATRIX4));
	}

	if (func == gcCore::MatrixId::MESH) if (!pMesh->SetTransform(-1, (LPD3DXMATRIX)pMat)) return -4;
	if (func == gcCore::MatrixId::GROUP) if (!pMesh->SetTransform(gi, (LPD3DXMATRIX)pMat)) return -5;

	return 0;
}

// ============================================================================================
//
void vVessel::ReloadTextures()
{
	for (UINT i = 0; i < nmesh; i++) if (meshlist[i].mesh) meshlist[i].mesh->ReloadTextures();
}





// ===========================================================================================
//

SurfNative * vVessel::defreentrytex = 0;
SurfNative * vVessel::defexhausttex = 0;
ShaderClass* vVessel::pRenderZone = 0;


// ==============================================================
// Nonmember helper functions

void TransformPoint (VECTOR3 &p, const D3DXMATRIX &T)
{
	double x = p.x*T._11 + p.y*T._21 + p.z*T._31 + T._41;
	double y = p.x*T._12 + p.y*T._22 + p.z*T._32 + T._42;
	double z = p.x*T._13 + p.y*T._23 + p.z*T._33 + T._43;
	double w = 1.0/(p.x*T._14 + p.y*T._24 + p.z*T._34 + T._44);
	p.x = x*w;
	p.y = y*w;
	p.z = z*w;
}

// ===============================================================
//
void TransformDirection (VECTOR3 &a, const D3DXMATRIX &T, bool normalise)
{
	double x = a.x*T._11 + a.y*T._21 + a.z*T._31;
	double y = a.x*T._12 + a.y*T._22 + a.z*T._32;
	double z = a.x*T._13 + a.y*T._23 + a.z*T._33;
	a.x = x, a.y = y, a.z = z;
	if (normalise) {
		double len = 1.0/sqrt (x*x + y*y + z*z);
		a.x *= len;
		a.y *= len;
		a.z *= len;
	}
}

// ===========================================================================
// Add ISO-prefix to a value
//
#define ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))

static inline const int clip(int v, int vMin, int vMax)
{
	if      (v < vMin) return vMin;
	else if (v > vMax) return vMax;
	else               return v;
}

inline const char *value_string (char *buf, size_t buf_size, double val)
{
	static const char unit_prefixes[] = { (char)181/*'µ'*/, 'm', '\0', 'k' , 'M' , 'G' , 'T' , 'P'};

	int index = (int) (log10(val)) / 3;
	val /= pow(10.0, index*3);

	// apply array index offset (+2) [-2...5] => [0...7]
	index = clip(index+2, 0, ARRAY_ELEMS(unit_prefixes) -1);
	sprintf_s(buf, buf_size, "%.3f %c", val, unit_prefixes[index]);

	return buf;
}

const char *value_string (double val)
{
	static char buf[16];
	return value_string(buf, sizeof(buf), val);
}

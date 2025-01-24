// ==============================================================
// VObject.cpp
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2006-2016 Martin Schweiger
//				 2010-2016 Jarmo Nikkanen (D3D9Client related parts)
// ==============================================================

// ==============================================================
// class vObject (implementation)
//
// A "vObject" is the visual representation of an Orbiter object
// (vessel, planet/moon/sun, surface base). vObjects usually have
// one or more meshes associated with it that define their visual
// appearance, but they can be arbitrarily complex (e.g. planets
// with clould layers, atmospheric haze, etc.)
// Visual objects don't persist as their "logical" counterparts,
// but are created and deleted as they pass in and out of the
// visual range of a camera. vObjects are therefore associated
// with a particular scene. In multi-scene environments, a single
// logical object may have multiple vObjects associated with it.
// ==============================================================

#include "VObject.h"
#include "VVessel.h"
#include "VPlanet.h"
#include "VStar.h"
#include "VBase.h"
#include "D3D9Util.h"
#include "Mesh.h"
#include "D3D9Surface.h"
#include "D3D9Client.h"

using namespace oapi;

// Initialisation of static members

D3D9Client*		vObject::gc = NULL;
SurfNative*		vObject::blobtex[3] = { NULL };
D3D9Mesh*		vObject::hStockMesh[16] = { NULL };


// ===========================================================================================
//
vObject::vObject(OBJHANDLE _hObj, const Scene *scene)
	: VisObject (_hObj)
	, active        (true)
	, bBSRecompute  (true)
	, bStencilShadow(true)
	, bOmit         (false)
	, scn( (Scene *)scene) // should be const!
	, sunapprad()
	, sundst   ()
	, cdist    ()
	, ctgtdst  ()
	, hPlanet (NULL)
	, lng(0), lat(0)
{
	_TRACE;
	D3DXMatrixIdentity(&mWorld);
	if (_hObj) size = oapiGetSize(_hObj);
	else size = 0;
	dmWorld = identity4();
	albedo = _V(1,1,1);
	oapiGetObjectName(hObj, name, 64);
	sunLight = *scene->GetSun();
	objtp = oapiGetObjectType(hObj);
}


// ===========================================================================================
//
vObject *vObject::Create(OBJHANDLE _hObj, const Scene *scene)
{
	_TRACE;

	int objtp = oapiGetObjectType(_hObj);
	switch (objtp) {
		case OBJTP_VESSEL:	    return new vVessel (_hObj, scene);
		case OBJTP_PLANET:		return new vPlanet (_hObj, scene);
		case OBJTP_STAR:		return new vStar   (_hObj, scene);
		case OBJTP_SURFBASE:	return new vBase   (_hObj, scene);
		default:
		{
			LogErr("Unidentified Object Type %d in %s",oapiGetObjectType(_hObj), _PTR(_hObj));
			return new vObject (_hObj, scene);
		}
	}
}


// ===========================================================================================
//
void vObject::GlobalInit(D3D9Client *gclient)
{
	_TRACE;
	static const char *fname[3] = {"Ball.dds","Ball2.dds","Ball3.dds"};
	gc = gclient;
	for (int i=0;i<3;i++) blobtex[i] = SURFACE(gc->clbkLoadTexture(fname[i]));

	// Create Some Stock Meshes ----------------------------------------
	//
	hStockMesh[D3D9SM_ARROW] = new D3D9Mesh("D3D9Arrow");
	hStockMesh[D3D9SM_SPHERE] = new D3D9Mesh("D3D9Sphere");
	hStockMesh[D3D9SM_BOX] = new D3D9Mesh("D3D9Box");

	hStockMesh[D3D9SM_SPHERE]->SetDualSided(0, true);
	hStockMesh[D3D9SM_BOX]->SetDualSided(0, true);
}


// ===========================================================================================
//
void vObject::GlobalExit()
{
	_TRACE;
	for (int i=0;i<3;i++) DELETE_SURFACE(blobtex[i]);
	for (int i = 0; i < ARRAYSIZE(hStockMesh); i++) SAFE_DELETE(hStockMesh[i]);
}


// ===========================================================================================
//
void vObject::Activate(bool isactive)
{
	active = isactive;
}


// ===========================================================================================
//
DWORD vObject::GetMeshCount()
{
	return 0;
}


// ===========================================================================================
//
void vObject::ReOrigin(VECTOR3 global_pos)
{
	cpos = gpos - global_pos;

	cdist = length(cpos);

	dmWorld.m41 = cpos.x;
	dmWorld.m42 = cpos.y;
	dmWorld.m43 = cpos.z;

	D3DMAT_SetTranslation(&mWorld, &cpos);
}


// ===========================================================================================
//
bool vObject::Update(bool bMainScene)
{
	if (!active) return false;

	assert(bMainScene==true);

	VECTOR3 tpos, cgpo;
	OBJHANDLE hTgt = oapiCameraTarget();

	if (hObj) {
		oapiGetRotationMatrix(hObj, &grot);
		oapiGetGlobalPos(hObj, &gpos);
	}
	else {
		double elev = oapiSurfaceElevation(hPlanet, lng, lat) + oapiGetSize(hPlanet);
		oapiEquToGlobal(hPlanet, lng, lat, elev, &gpos);
		//grot = identity();
	}

	oapiGetGlobalPos(hTgt, &tpos);

	cgpo   = scn->GetCameraGPos();
	axis   = mul(grot, _V(0, 1, 0));
	cpos   = gpos - cgpo;
	cdist  = length(cpos);

	// Create double precision world matrix
	//
	dmWorld = _M(grot.m11, grot.m21, grot.m31, 0,
		         grot.m12, grot.m22, grot.m32, 0,
				 grot.m13, grot.m23, grot.m33, 0,
				 cpos.x,   cpos.y,   cpos.z,   1);

	// Create single precision world matrix
	//
	D3DMAT_SetInvRotation(&mWorld, &grot);
	D3DMAT_SetTranslation(&mWorld, &cpos);


	OBJHANDLE hSun = oapiGetGbodyByIndex(0);
	oapiGetGlobalPos(hSun, &sundir);

	ctgtdst = length(tpos - gpos);
	sundst = length(sundir - gpos);
	sunapprad = oapiGetSize(hSun) / sundst;

	if (hSun != hObj) sundir = unit(sundir - gpos);
	else			  sundir = unit(sundir - cgpo);

	CheckResolution();


	return true;
}


// ===========================================================================================
//
void vObject::UpdateBoundingBox()
{

}

// ===========================================================================================
//
D3DXVECTOR3 vObject::GetBoundingSpherePosDX()
{
	if (bBSRecompute) UpdateBoundingBox();
	D3DXVECTOR3 pos;
	D3DXVec3TransformCoord(&pos, (LPD3DXVECTOR3)&BBox.bs, &mWorld);
	return pos;
}

// ===========================================================================================
//
VECTOR3 vObject::GetBoundingSpherePos()
{
	D3DXVECTOR3 pos = GetBoundingSpherePosDX();
	return _V((double)pos.x, (double)pos.y, (double)pos.z);
}


// ===========================================================================================
//
float vObject::GetBoundingSphereRadius()
{
	if (bBSRecompute) UpdateBoundingBox();
	return BBox.bs.w;
}


// ===========================================================================================
//
const char *vObject::GetName() const
{
	return name;
}


// ===========================================================================================
//
bool vObject::IsVisible()
{
	VECTOR3 pos  = GetBoundingSpherePos();
	float rad = GetBoundingSphereRadius();
	float apr = scn->GetCameraAperture();
	double apprad = rad / cdist;

	if ((objtp == OBJTP_VESSEL) && apprad < 0.005*apr) return false;
	if ((objtp == OBJTP_SURFBASE) && apprad < 0.02*apr) return false;

	return gc->GetScene()->IsVisibleInCamera(ptr(D3DXVEC(pos)), rad);

	/* 
	if (bVis) {
		double brad = oapiGetSize(gc->GetScene()->GetCameraProxyBody());
		double crad = cdist;
		double alfa = acos(brad/crad);
		double trad = length(pos+cpos);
		double beta = acos(dotp(pos+cpos, cpos)/(crad*trad));
		if (beta<alfa) return true;
		double resl = brad - trad * cos(beta-alfa);
		if (resl>rad) return false;
	}*/
}


// ===========================================================================================
// This routine will render beacons
//
void vObject::RenderSpot(LPDIRECT3DDEVICE9 dev, const VECTOR3 *ofs, float size, const VECTOR3 &col, bool lighting, int shape)
{
	VECTOR3 pos(cpos);

	if (ofs) pos += mul (grot, *ofs);
	VECTOR3 camp = scn->GetCameraGPos();

	const double ambient = 0.2;
	double cosa = dotp (unit(gpos), unit(gpos - camp));
	double intens = (lighting ? 0.5 * ((1.0-ambient)*cosa + 1.0+ambient) : 1.0);

	D3DXMATRIX W;
	D3DXVECTOR3 vPos(float(pos.x), float(pos.y), float(pos.z));
	D3DXVECTOR3 vCam;
	D3DXVec3Normalize(&vCam, &vPos);
	D3DMAT_CreateX_Billboard(&vCam, &vPos, size, &W);

	D3DXCOLOR color((float)col.x, (float)col.y, (float)col.z, 1.0f);

	D3D9Effect::RenderSpot((float)intens, &color, (const LPD3DXMATRIX)&W, blobtex[shape]);
}


// ===========================================================================================
// This routine is for rendering celestial body dots
//
void vObject::RenderDot(LPDIRECT3DDEVICE9 dev)
{
	if (hObj==NULL) return;

	VECTOR3 spos;
	oapiGetGlobalPos(oapiGetGbodyByIndex(0), &spos);
	oapiGetGlobalPos(hObj, &gpos);
	cpos = gpos - scn->GetCameraGPos();
	cdist = length(cpos);

	double alt = max(1.0, cdist - size);
	double apr = size * scn->ViewH()*0.5 / (alt * tan(scn->GetCameraAperture()));

	double ds = 10000.0 / cdist;
	double s = 2.0;
	if (apr<0.3) s = 1.0;
	float scale = float(size * ds * s/apr);

	D3DXMATRIX W;
	D3DXVECTOR3 vCam;
	D3DXVECTOR3 vPos(float(cpos.x), float(cpos.y), float(cpos.z));
	vPos*=float(ds);

	D3DXVec3Normalize(&vCam, &vPos);
	D3DMAT_CreateX_Billboard(&vCam, &vPos, scale, &W);

	float ints = float(sqrt(1.0+dotp(unit(gpos-spos), unit(cpos)))) * 1.0f;

	if (ints>1.0f) ints=1.0f;

	D3DXCOLOR color(float(albedo.x)*ints, float(albedo.y)*ints, float(albedo.z)*ints, 1.0f);

	D3D9Effect::RenderSpot(1.0f, &color, (const LPD3DXMATRIX)&W, blobtex[0]);
}


// ===========================================================================================
//
void vObject::RenderVectors (LPDIRECT3DDEVICE9 dev, D3D9Pad* pSkp)
{
	DWORD favmode = *(DWORD*)gc->GetConfigParam(CFGPRM_FRAMEAXISFLAG);

	if (favmode & FAV_ENABLE) // General AXIS rendering ON/OFF
	{
		if ((objtp == OBJTP_VESSEL && favmode & FAV_VESSEL) ||
			(objtp == OBJTP_PLANET && favmode & FAV_CELBODY) ||
			(objtp == OBJTP_SURFBASE && favmode & FAV_BASE))
		{
			float alpha = *(float*)gc->GetConfigParam(CFGPRM_FRAMEAXISOPACITY);

			if (alpha > 1e-9) // skip all this when opacity is to small (ZEROish)
			{
				float scale = float(size) / 50.0f;
				float sclset = *(float*)gc->GetConfigParam(CFGPRM_FRAMEAXISSCALE);
				//scale *= 0.99f; // 1% "slimmer" to avoid z-fighting with force vector(s)
				float ascale = float(size) * sclset * 0.5f;

				RenderAxisVector(pSkp, ptr(D3DXCOLOR(1, 0, 0, alpha)), _V(1, 0, 0), ascale, scale);
				RenderAxisLabel(pSkp, ptr(D3DXCOLOR(1, 0, 0, alpha)), _V(1, 0, 0), ascale, scale, "+X");

				RenderAxisVector(pSkp, ptr(D3DXCOLOR(0, 1, 0, alpha)), _V(0, 1, 0), ascale, scale);
				RenderAxisLabel(pSkp, ptr(D3DXCOLOR(0, 1, 0, alpha)), _V(0, 1, 0), ascale, scale, "+Y");

				RenderAxisVector(pSkp, ptr(D3DXCOLOR(0, 0, 1, alpha)), _V(0, 0, 1), ascale, scale);
				RenderAxisLabel(pSkp, ptr(D3DXCOLOR(0, 0, 1, alpha)), _V(0, 0, 1), ascale, scale, "+Z");

				if (favmode & FAV_NEGATIVE) {
					RenderAxisVector(pSkp, ptr(D3DXCOLOR(1, 0, 0, alpha * 0.5f)), _V(-1, 0, 0), ascale, scale);
					RenderAxisLabel(pSkp, ptr(D3DXCOLOR(1, 0, 0, alpha)), _V(-1, 0, 0), ascale, scale, "-X");

					RenderAxisVector(pSkp, ptr(D3DXCOLOR(0, 1, 0, alpha * 0.5f)), _V(0, -1, 0), ascale, scale);
					RenderAxisLabel(pSkp, ptr(D3DXCOLOR(0, 1, 0, alpha)), _V(0, -1, 0), ascale, scale, "-Y");

					RenderAxisVector(pSkp, ptr(D3DXCOLOR(0, 0, 1, alpha * 0.5f)), _V(0, 0, -1), ascale, scale);
					RenderAxisLabel(pSkp, ptr(D3DXCOLOR(0, 0, 1, alpha)), _V(0, 0, -1), ascale, scale, "-Z");
				}
			}
		}
	}
}


// ===========================================================================================
//
void vObject::RenderAxisVector(D3D9Pad *pSkp, const D3DXCOLOR *pColor, VECTOR3 vector, float lscale, float size, bool bLog)
{
	D3DXMATRIX W;

	VECTOR3 dir = mul(grot, vector);
	VECTOR3 camp = gc->GetScene()->GetCameraGPos();

    VECTOR3 pos = gpos - camp;
	VECTOR3 rot = crossp(pos, vector);

    VECTOR3 y = mul (grot, unit(vector)) * size;
    VECTOR3 x = mul (grot, unit(rot)) * size;
    VECTOR3 z = mul (grot, unit(crossp(vector, rot))) * size;

    D3DXMatrixIdentity(&W);

    W._11 = float(x.x); W._12 = float(x.y); W._13 = float(x.z);
    W._21 = float(y.x); W._22 = float(y.y); W._23 = float(y.z);
    W._31 = float(z.x); W._32 = float(z.y); W._33 = float(z.z);

    W._41 = float(pos.x);
    W._42 = float(pos.y);
    W._43 = float(pos.z);

	float len = float(length(vector));

	if (bLog) len = max(0.0f, 13.0f+log(len)) * lscale / size;
	else      len = len * lscale / size;

	hStockMesh[D3D9SM_ARROW]->RenderAxisVector(&W, pColor, len);
}


// ===========================================================================================
//
void vObject::RenderAxisLabel(D3D9Pad *pSkp, const D3DXCOLOR *clr, VECTOR3 vector, float lscale, float size, const char *label, bool bLog)
{
	D3DXVECTOR3 homog, ws;
	D3DXMATRIX W;

	VECTOR3 dir = mul(grot, vector);
	VECTOR3 camp = gc->GetScene()->GetCameraGPos();

	VECTOR3 pos = gpos - camp;
	VECTOR3 rot = crossp(pos, vector);

	VECTOR3 y = mul(grot, unit(vector)) * size;
	VECTOR3 x = mul(grot, unit(rot)) * size;
	VECTOR3 z = mul(grot, unit(crossp(vector, rot))) * size;

	D3DXMatrixIdentity(&W);

	W._11 = float(x.x);  W._12 = float(x.y);  W._13 = float(x.z);
	W._21 = float(y.x);  W._22 = float(y.y);  W._23 = float(y.z);
	W._31 = float(z.x);  W._32 = float(z.y);  W._33 = float(z.z);

	W._41 = float(pos.x);
	W._42 = float(pos.y);
	W._43 = float(pos.z);

	float len = float(length(vector));

	if (bLog) len = max(0.0f, 13.0f + log(len)) * lscale / size;
	else      len = len * lscale / size;

	D3DXVec3TransformCoord(&ws, ptr(D3DXVECTOR3(0, len, 0)), &W);
	D3DXVec3TransformCoord(&homog, &ws, scn->GetProjectionViewMatrix());

	if (D3DXVec3Dot(&ws, scn->GetCameraZ()) < 0) return;

	if (homog.x >= -1.0f && homog.x <= 1.0f && homog.y >= -1.0f && homog.y <= 1.0f) {
		int xc = (int)(scn->ViewW()*0.5*(1.0f + homog.x));
		int yc = (int)(scn->ViewH()*0.5*(1.0f - homog.y));
		pSkp->SetTextColor(D3DXCOLOR(clr->b, clr->g, clr->r, clr->a));
		pSkp->Text(xc + 10, yc, label, lstrlen(label));
	}
}


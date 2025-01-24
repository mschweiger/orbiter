// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2006-2016 Martin Schweiger
//				 2012-2016 Jarmo Nikkanen
// ==============================================================

#ifndef __D3DUTIL_H
#define __D3DUTIL_H

#include "OrbiterAPI.h"
#include "Log.h"
#include "DrawAPI.h"
#include <d3d9.h>
#include <d3dx9.h>
#include <string>
#include "gcCore.h"

#define float2 FVECTOR2
#define float3 FVECTOR3
#define float4 FVECTOR4
#define float4x4 FMATRIX4 

#ifdef _DEBUG
#ifndef _TRACE
#define _TRACE { LogTrace("[TRACE] %s Line:%d %s",__FILE__,__LINE__,__FUNCTION__); }
#endif
#else
#ifndef _TRACE
#define _TRACE
#endif
#endif



/*
#ifndef HR
#define HR(x) x;
#endif
*/

#ifndef HR
#define HR(x)                                      \
{                                                  \
	HRESULT hr = x;                                \
	if(FAILED(hr))                                 \
	{												\
		LogErr("%s Line:%d Error:%d %s",__FILE__,__LINE__,hr,(#x)); \
	}                                                \
}
#endif


inline bool _HROK(HRESULT hr, const char *file, int line)
{
	if (hr==S_OK) return true;
	else LogErr("%s Line:%d Error:%d", file, line, hr);
	return false;
}

const char *_PTR(const void *p);


#ifndef HROK
#define HROK(x)	_HROK(x, __FILE__, __LINE__)
#endif

#define PI 3.141592653589793238462643383279

#define SURFACE(x) ((class SurfNative *)x)

// helper function to get address of a temporary
// The regular "easy" way no longer works on some compilers so lets use a hack to get a simple thing done.
// NB: use with caution
template<typename T>
T* ptr(T&& x) { return &x; }

// ------------------------------------------------------------------------------------
// Vertex Declaration equal to NTVERTEX
// ------------------------------------------------------------------------------------

const D3DVERTEXELEMENT9 BAVertexDecl[] = {
	{0, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	{0, 12, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
	{0, 24, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	{0, 40, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
	{0, 48, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
	D3DDECL_END()
};

const D3DVERTEXELEMENT9 NTVertexDecl[] = {
	{0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	{0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
	{0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	D3DDECL_END()
};

const D3DVERTEXELEMENT9 MeshVertexDecl[] = {
	{0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	{0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
	{0, 24, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0},
	{0, 36, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	D3DDECL_END()
};

const D3DVERTEXELEMENT9 PatchVertexDecl[] = {
	{0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	{0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
	{0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	{0, 32, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
	D3DDECL_END()
};

const D3DVERTEXELEMENT9 PosTexDecl[] = {
	{0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	{0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	D3DDECL_END()
};

const D3DVERTEXELEMENT9 PosColorDecl[] = {
	{0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	{0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
	D3DDECL_END()
};

const D3DVERTEXELEMENT9 PositionDecl[] = {
	{0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	D3DDECL_END()
};

const D3DVERTEXELEMENT9 SketchpadDecl[] = {
	{ 0, 0,   D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 12,  D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	{ 0, 28,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
	{ 0, 32,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 1 },
	D3DDECL_END()
};

const D3DVERTEXELEMENT9 Vector4Decl[] = {
	{0, 0,  D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	D3DDECL_END()
};

const D3DVERTEXELEMENT9 GPUBlitDecl[] = {
	{0, 0,  D3DDECLTYPE_SHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	D3DDECL_END()
};

const D3DVERTEXELEMENT9 HazeVertexDecl[] = {
	{0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	{0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
	{0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	D3DDECL_END()
};

const D3DVERTEXELEMENT9 LocalLightsDecl[] = {
	{0, 0,  D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},	//Primitive Index
	{0, 4,  D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},	//Position .xyz and cone .w
	D3DDECL_END()
};

typedef struct
{
	float index;
	FVECTOR3 pos;
	float cone;
} LocalLightsCompute;

typedef struct {
	float x;     ///< vertex x position
	float y;     ///< vertex y position
	float z;     ///< vertex z position
	float tu;    ///< vertex u texture coordinate
	float tv;    ///< vertex v texture coordinate
} SMVERTEX;

typedef struct {
	float x, y, z;
	float nx, ny, nz;
	float tx, ty, tz;
	float u, v, w;
} NMVERTEX;

typedef struct {
	short tx,ty;
	short sx,sy;
} GPUBLITVTX;


typedef struct {
	D3DXVECTOR3 pos;				///< beacon position
	D3DXVECTOR3 dir;				///< light direction
	float size, angle, on, off;		///< beacon size and light cone angle
	float bright, falloff;
	DWORD color;					///< beacon color
} BAVERTEX;

typedef struct _LightStruct  {
    int			  Type;             ///< Type of light source
	float		  Dst2;				///< Square distance between camera and the light emitter
    D3DXCOLOR     Diffuse;          ///< Color of light
    D3DXVECTOR3   Position;         ///< position in world space
    D3DXVECTOR3   Direction;        ///< direction in world space
    D3DXVECTOR3   Attenuation;      ///< Attenuation
	D3DXVECTOR4   Param;            ///< range, falloff, theta, phi
public : _LightStruct () :	Type(0),
							Dst2(0.0),
							Diffuse(D3DXCOLOR(0ul)),
							Position(0,0,0), Direction(1.0f, 0.0f, 0.0f), Attenuation(1.0f, 1.0f, 1.0f),
							Param(0,0,0,0)
							{}
} LightStruct;


class D3D9Light : public LightStruct
{
public:
				D3D9Light();
				D3D9Light(const LightEmitter *le, const class vObject *vo);
				~D3D9Light();

		float	GetIlluminance(D3DXVECTOR3 &pos, float r) const;
		void	UpdateLight(const LightEmitter *le, const class vObject *vo);
		void	Reset();
		const   LightEmitter *GetEmitter() const;

		float	cone;
		int		GPUId;
private:
		float	cosp, tanp, cosu;
		float	range, range2;
		float	intensity;
		const   LightEmitter *le;
};



class SketchMesh
{

public:

	struct SKETCHGRP {			// mesh group definition
		DWORD VertOff;			// Main mesh Vertex Offset
		DWORD IdxOff;			// Main mesh Index Offset
		DWORD nIdx;				// Index count
		DWORD nVert;			// Vertex count
		DWORD MtrlIdx;			// material index
		DWORD TexIdx;			// texture index 0=None
	};

	explicit		SketchMesh(LPDIRECT3DDEVICE9 pDev);
	~SketchMesh();

	void			Init();
	bool			LoadMeshFromHandle(MESHHANDLE hMesh);
	void			RenderGroup(DWORD idx);
	SURFHANDLE		GetTexture(DWORD idx);
	D3DXCOLOR		GetMaterial(DWORD idx);
	DWORD			GroupCount() const { return nGrp; }

private:

	LPDIRECT3DVERTEXBUFFER9 pVB; ///< (Local) Vertex buffer pointer
	LPDIRECT3DINDEXBUFFER9 pIB;

	DWORD MaxVert;
	DWORD MaxIdx;

	DWORD nGrp;                 // number of mesh groups
	DWORD nMtrl;                // number of mesh materials
	DWORD nTex;                 // number of mesh textures

	LPDIRECT3DDEVICE9 pDev;
	SURFHANDLE* Tex;			// list of mesh textures
	SKETCHGRP* Grp;            // list of mesh groups
	D3DXCOLOR* Mtrl;
};

#pragma pack(push, 4)

typedef struct {
	FVECTOR3 Dir;
	FVECTOR3 Color;			// Color and Intensity of received sunlight 
	FVECTOR3 Ambient;		// Ambient light level (Base Objects Only, Vessels are using dynamic methods)
	FVECTOR3 Transmission;	// Visibility through atmosphere (1.0 = fully visible, 0.0 = obscured)
	FVECTOR3 Incatter;		// Amount of incattered light from haze
} D3D9Sun;


#define D3D9MATEX_DIFFUSE		0x001
#define D3D9MATEX_AMBIENT		0x002
#define D3D9MATEX_SPECULAR		0x004
#define D3D9MATEX_EMISSIVE		0x008
#define D3D9MATEX_REFLECT		0x010
#define D3D9MATEX_FRESNEL		0x040
#define D3D9MATEX_ROUGHNESS		0x080
#define D3D9MATEX_EMISSION2		0x100
#define D3D9MATEX_METALNESS		0x200
#define D3D9MATEX_SPECIALFX		0x400


/**
 * \brief Material structure used in D3D9Mesh. ModFlags is not loaded to shaders
 */
typedef struct {
	D3DXVECTOR4	  Diffuse;
	D3DXVECTOR4   Specular;			///< Specular color, power in alpha
	D3DXVECTOR3	  Ambient;
	D3DXVECTOR3   Emissive;
	D3DXVECTOR3   Reflect;			///< Color multiplier and intensity (alpha)
	D3DXVECTOR3	  Emission2;		///<
	D3DXVECTOR3	  Fresnel;			///< Fresnel reflection
	D3DXVECTOR2	  Roughness;		///< 
	float		  Metalness;
	D3DXVECTOR4	  SpecialFX;
	// -----------------------
	DWORD		  ModFlags;			///< Modification flags
} D3D9MatExt;

#pragma pack(pop)

typedef struct {
	class D3D9Mesh *pMesh;			///< Mesh handle
	class vObject  *vObj;			///< Visual handle
	float			dist;			///< Distance to a pick point
	int				group;			///< Mesh group that was picked
	D3DXVECTOR3		normal;			///< Normal vector in local vessel coordinates
	D3DXVECTOR3		pos;			///< Position in local vessel coordinates
	int				idx;			///< Index that was picked
	float			u, v;			///< Barycentric coordinates
} D3D9Pick;

typedef struct {
	D3DXVECTOR3 _p;		// Position from camera
	D3DXVECTOR3 _n;		// Normal
	int i;				// Face Index
	float d;			// Distance from camera
	float u, v;			
	double lng, lat, elev;
	class Tile * pTile;
} TILEPICK;

#define D3D9LRange 0
#define D3D9LFalloff 1
#define D3D9LTheta 2
#define D3D9LPhi 3

extern IDirect3DVertexDeclaration9	*pMeshVertexDecl;
extern IDirect3DVertexDeclaration9	*pHazeVertexDecl;
extern IDirect3DVertexDeclaration9	*pNTVertexDecl;
extern IDirect3DVertexDeclaration9	*pBAVertexDecl;
extern IDirect3DVertexDeclaration9	*pPosColorDecl;
extern IDirect3DVertexDeclaration9	*pPositionDecl;
extern IDirect3DVertexDeclaration9	*pVector4Decl;
extern IDirect3DVertexDeclaration9	*pPosTexDecl;
extern IDirect3DVertexDeclaration9	*pPatchVertexDecl;
extern IDirect3DVertexDeclaration9	*pSketchpadDecl;
extern IDirect3DVertexDeclaration9  *pLocalLightsDecl;



class ShaderClass
{
	

public:
			ShaderClass(LPDIRECT3DDEVICE9 pDev, const char* file, const char* vs, const char* ps, const char* name, const char* options);
		    ~ShaderClass();
	void	ClearTextures();
	void    UpdateTextures();
	void	DetachTextures();
	void	Setup(LPDIRECT3DVERTEXDECLARATION9 pDecl, bool bZ, int blend);
	HANDLE	GetPSHandle(const char* name);
	HANDLE	GetVSHandle(const char* name);

	void	SetTexture(const char* name, LPDIRECT3DBASETEXTURE9 pTex, UINT Flags = IPF_CLAMP | IPF_ANISOTROPIC, UINT AnisoLvl = 4);
	void	SetTextureVS(const char* name, LPDIRECT3DBASETEXTURE9 pTex, UINT flags = IPF_CLAMP | IPF_POINT, UINT AnisoLvl = 0);
	void	SetPSConstants(const char* name, void* data, UINT bytes);
	void	SetVSConstants(const char* name, void* data, UINT bytes);

	void	SetTexture(HANDLE hVar, LPDIRECT3DBASETEXTURE9 pTex, UINT flags = IPF_CLAMP | IPF_POINT, UINT AnisoLvl = 0);
	void	SetTextureVS(HANDLE hVar, LPDIRECT3DBASETEXTURE9 pTex, UINT flags, UINT aniso);
	void	SetPSConstants(HANDLE hVar, void* data, UINT bytes);
	void	SetVSConstants(HANDLE hVar, void* data, UINT bytes);
	LPDIRECT3DDEVICE9 GetDevice() { return pDev; }

private:

	struct TexParams
	{
		LPDIRECT3DBASETEXTURE9 pTex;
		LPDIRECT3DBASETEXTURE9 pAssigned;
		UINT Flags;
		UINT AnisoLvl;
		bool bSamplerSet;
	} pTextures[20];

	LPD3DXCONSTANTTABLE pPSCB, pVSCB;
	LPDIRECT3DPIXELSHADER9 pPS;
	LPDIRECT3DVERTEXSHADER9 pVS;
	LPDIRECT3DDEVICE9 pDev;
	std::string fn, psn, vsn, sn;
};



inline void swap(double &a, double &b)
{
	double c = a; a = b; b = c;
}

// Jump between western and eastern hemispheres
inline double wrap(double a)
{
	if (a<-PI) return a+PI2;
	if (a>PI) return a-PI2;
	return a;
}

inline void LogSunLight(D3D9Sun& s)
{
	LogAlw("Sunlight.Dir   = [%f, %f, %f]", s.Dir.x, s.Dir.y, s.Dir.z);
	LogAlw("Sunlight.Color = [%f, %f, %f]", s.Color.x, s.Color.y, s.Color.z);
	LogAlw("Sunlight.Ambie = [%f, %f, %f]", s.Ambient.x, s.Ambient.y, s.Ambient.z);
	LogAlw("Sunlight.Trans = [%f, %f, %f]", s.Transmission.x, s.Transmission.y, s.Transmission.z);
	LogAlw("Sunlight.Incat = [%f, %f, %f]", s.Incatter.x, s.Incatter.y, s.Incatter.z);
}

// -----------------------------------------------------------------------------------
// Conversion functions
// ------------------------------------------------------------------------------------

inline RECT _RECT(DWORD l, DWORD t, DWORD r, DWORD b)
{
	RECT rect = { long(l), long(t), long(r), long(b) };
	return rect;
}

inline VECTOR3 _V(D3DXVECTOR3 &i)
{
	return _V(double(i.x), double(i.y), double(i.z));
}

inline oapi::FVECTOR3 _FV(D3DXVECTOR3 &i)
{
	return oapi::FVECTOR3(i.x, i.y, i.z);
}

inline VECTOR3 _V(D3DXVECTOR4 &i)
{
	return _V(double(i.x), double(i.y), double(i.z));
}

inline void D3DXCOLORSWAP(D3DXCOLOR *x)
{
	float a = x->r; x->r = x->b; x->b = a;
}

inline D3DXVECTOR3 D3DXVECTOR3f4(D3DXVECTOR4 v)
{
	return D3DXVECTOR3(v.x, v.y, v.z);
}

inline D3DCOLORVALUE D3DCOLORMULT(const D3DCOLORVALUE *a, const D3DCOLORVALUE *b)
{
	D3DCOLORVALUE c;
	c.a = a->a * b->a;
	c.r = a->r * b->r;
	c.g = a->g * b->g;
	c.b = a->b * b->b;
	return c;
}

inline void MATRIX4toD3DMATRIX (const MATRIX4 &M, D3DXMATRIX &D)
{
	D._11 = (float)M.m11;  D._12 = (float)M.m12;  D._13 = (float)M.m13;  D._14 = (float)M.m14;
	D._21 = (float)M.m21;  D._22 = (float)M.m22;  D._23 = (float)M.m23;  D._24 = (float)M.m24;
	D._31 = (float)M.m31;  D._32 = (float)M.m32;  D._33 = (float)M.m33;  D._34 = (float)M.m34;
	D._41 = (float)M.m41;  D._42 = (float)M.m42;  D._43 = (float)M.m43;  D._44 = (float)M.m44;
}

inline MATRIX4 _MATRIX4(const LPD3DXMATRIX M)
{
	MATRIX4 D;
	D.m11 = (double)M->_11;  D.m12 = (double)M->_12;  D.m13 = (double)M->_13;  D.m14 = (double)M->_14;
	D.m21 = (double)M->_21;  D.m22 = (double)M->_22;  D.m23 = (double)M->_23;  D.m24 = (double)M->_24;
	D.m31 = (double)M->_31;  D.m32 = (double)M->_32;  D.m33 = (double)M->_33;  D.m34 = (double)M->_34;
	D.m41 = (double)M->_41;  D.m42 = (double)M->_42;  D.m43 = (double)M->_43;  D.m44 = (double)M->_44;
	return D;
}

inline void TransformVertex(NMVERTEX *pVrt, LPD3DXMATRIX pW)
{
	D3DXVECTOR3 p,n,t;
	D3DXVec3TransformCoord(&p, ptr(D3DXVECTOR3(pVrt->x, pVrt->y, pVrt->z)), pW);
	D3DXVec3TransformNormal(&n, ptr(D3DXVECTOR3(pVrt->nx, pVrt->ny, pVrt->nz)), pW);
	D3DXVec3TransformNormal(&t, ptr(D3DXVECTOR3(pVrt->tx, pVrt->ty, pVrt->tz)), pW);
	pVrt->x  = p.x;	pVrt->y  = p.y; pVrt->z  = p.z;
	pVrt->nx = n.x; pVrt->ny = n.y; pVrt->nz = n.z;
	pVrt->tx = t.x; pVrt->ty = t.y; pVrt->tz = t.z;
}

inline void D3DVEC (const VECTOR3 &v, D3DVECTOR &d3dv)
{
	d3dv.x = (float)v.x;
	d3dv.y = (float)v.y;
	d3dv.z = (float)v.z;
}

inline D3DXVECTOR4 D3DXC2V(const D3DXCOLOR &v)
{
	return D3DXVECTOR4(v.r, v.g, v.b, v.a);
}

inline D3DXVECTOR3 D3DXVEC(const VECTOR3 &v)
{
	return D3DXVECTOR3(float(v.x), float(v.y), float(v.z));
}

inline D3DXVECTOR3 D3DXVEC(const VECTOR4 &v)
{
	return D3DXVECTOR3(float(v.x), float(v.y), float(v.z));
}

inline D3DXVECTOR4 D3DXVEC4(const VECTOR3 &v, float w)
{
	return D3DXVECTOR4(float(v.x), float(v.y), float(v.z), w);
}

inline D3DXCOLOR _D3DXCOLOR(const VECTOR3 &v, float a = 1.0f)
{
	return D3DXCOLOR(float(v.x), float(v.y), float(v.z), a);
}

inline VECTOR3 _VD3DX(const D3DXVECTOR3 &v)
{
	return _V(double(v.x), double(v.y), double(v.z));
}

inline VECTOR4 _VD4DX(const D3DXVECTOR4 &v)
{
	return _V(double(v.x), double(v.y), double(v.z), double(v.w));
}

inline float D3DVAL (double x)
{
	return (float)x;
}

//char* _fgets(char* cbuf, int num, FILE* stream, bool keepOneSpace = false);

int fgets2(char *buf, int cmax, FILE *file, DWORD param=0);

float D3DXVec3Angle(D3DXVECTOR3 a, D3DXVECTOR3 b);
D3DXVECTOR3 Perpendicular(D3DXVECTOR3 *a);

const char *RemovePath(const char *in);
SketchMesh * GetSketchMesh(const MESHHANDLE hMesh);

bool CreateVolumeTexture(LPDIRECT3DDEVICE9 pDevice, int count, LPDIRECT3DTEXTURE9 *pIn, LPDIRECT3DVOLUMETEXTURE9 *pOut);

void CreateMatExt(const D3DMATERIAL9 *pIn, D3D9MatExt *pOut);
void UpdateMatExt(const D3DMATERIAL9 *pIn, D3D9MatExt *pOut);
void CreateDefaultMat(D3D9MatExt *pOut);
void GetMatExt(const D3D9MatExt *pIn, D3DMATERIAL9 *pOut);
bool CopyBuffer(LPDIRECT3DRESOURCE9 _pDst, LPDIRECT3DRESOURCE9 _pSrc);
int LoadPlanetTextures(const char* fname, LPDIRECT3DTEXTURE9* ppdds, DWORD flags, int amount);
float SunOcclusionByPlanet(OBJHANDLE hObj, VECTOR3 gpos);
float OcclusionFactor(float x, float sunrad, float plnrad);
float OcclusionFactor(float x, float r1, float r2, bool bReverse);
double Distance(vObject *a, vObject* b);
bool IsCastingShadows(vObject* body, vObject* ref, double* sunsize_out);

LPDIRECT3DPIXELSHADER9 CompilePixelShader(LPDIRECT3DDEVICE9 pDev, const char *file, const char *function, const char* name, const char *options, LPD3DXCONSTANTTABLE *pConst);
LPDIRECT3DVERTEXSHADER9 CompileVertexShader(LPDIRECT3DDEVICE9 pDev, const char *file, const char *function, const char* name, const char *options, LPD3DXCONSTANTTABLE *pConst);

DWORD BuildDate();

// ------------------------------------------------------------------------------------
// D3D vector and matrix operations
// ------------------------------------------------------------------------------------

float D3DMAT_BSScaleFactor(const D3DXMATRIX *mat);
void D3DMAT_Identity (D3DXMATRIX *mat);
void D3DMAT_ZeroMatrix(D3DXMATRIX *mat);
void D3DMAT_Copy (D3DXMATRIX *tgt, const D3DXMATRIX *src);
void D3DMAT_SetRotation (D3DXMATRIX *mat, const MATRIX3 *rot);
void D3DMAT_SetInvRotation (D3DXMATRIX *mat, const MATRIX3 *rot);
void D3DMAT_RotationFromAxis (const D3DXVECTOR3 &axis, float angle, D3DXMATRIX *rot);
void D3DMAT_FromAxis(D3DXMATRIX *out, const D3DVECTOR *x, const D3DVECTOR *y, const D3DVECTOR *z);
void D3DMAT_FromAxis(D3DXMATRIX *out, const VECTOR3 *x, const VECTOR3 *y, const VECTOR3 *z);
void D3DMAT_FromAxisT(D3DXMATRIX *out, const D3DVECTOR *x, const D3DVECTOR *y, const D3DVECTOR *z);
void D3DMAT_CreateX_Billboard(const D3DXVECTOR3 *toCam, const D3DXVECTOR3 *pos, float scale, D3DXMATRIX *pOut);
void D3DMAT_CreateX_Billboard(const D3DXVECTOR3 *toCam, const D3DXVECTOR3 *pos, const D3DXVECTOR3 *dir, float size, float stretch, D3DXMATRIX *pOut);

// Set up a as matrix for ANTICLOCKWISE rotation r around x/y/z-axis
void D3DMAT_RotX (D3DXMATRIX *mat, double r);
void D3DMAT_RotY (D3DXMATRIX *mat, double r);

void D3DMAT_SetTranslation (D3DXMATRIX *mat, const VECTOR3 *trans);
void D3DMAT_SetTranslation(D3DXMATRIX *mat, const D3DXVECTOR3 *trans);
bool D3DMAT_VectorMatrixMultiply (D3DXVECTOR3 *res, const D3DXVECTOR3 *v, const D3DXMATRIX *mat);
HRESULT D3DMAT_MatrixInvert (D3DXMATRIX *res, D3DXMATRIX *a);

// ------------------------------------------------------------------------------------
// Vertex formats
// ------------------------------------------------------------------------------------
struct VERTEX_XYZ { float x, y, z; };                   // transformed vertex
struct VERTEX_XYZC { float x, y, z; D3DCOLOR col; };     // untransformed vertex with single colour component

// untransformed lit vertex with texture coordinates
struct VERTEX_XYZ_TEX {
	float x, y, z;
	float tu, tv;
};

// untransformed unlit vertex with two sets of texture coordinates
struct VERTEX_2TEX {
	float x, y, z, nx, ny, nz;
	float tu0, tv0, e;
	inline VERTEX_2TEX() : x(0.0f), y(0.0f), z(0.0f), nx(0.0f), ny(0.0f), nz(0.0f),
		tu0(0.0f), tv0(0.0f), e(0.0f) {}
	inline VERTEX_2TEX(const D3DVECTOR& p, const D3DVECTOR& n, float u0, float v0, float u1, float v1)
		: x(p.x), y(p.y), z(p.z), nx(n.x), ny(n.y), nz(n.z),
		tu0(u0), tv0(v0), e(0.0f) {}
};

// -----------------------------------------------------------------------------------
// String helper
// ------------------------------------------------------------------------------------

// trim from start
std::string &ltrim (std::string &s);

// trim from end
std::string &rtrim (std::string &s);

// trim from both ends
std::string &trim (std::string &s);

// uppercase complete string
void toUpper (std::string &s);

// lowercase complete string
//void toLower (std::string &s);

// string to double (returns quiet_NaN if conversion failed)
double toDoubleOrNaN (const std::string &str);

// case insensitive compare
bool startsWith (const std::string &haystack, const std::string &needle);

// case insensitive contains
bool contains (const std::string &haystack, const std::string &needle);

// case insensitive find
size_t find_ci (const std::string &haystack, const std::string &needle);

// case insensitive rfind
size_t rfind_ci (const std::string &haystack, const std::string &needle);

// parse assignments like "foo=bar", "foo = bar" or even "foo= bar ; with comment"
std::pair<std::string, std::string> &splitAssignment (const std::string &line, const char delim = '=');

// replace all occurrences of 's' in 'subj' by 't'
std::string::size_type replace_all (std::string &subj, const std::string &s, const std::string &t);

// -----------------------------------------------------------------------------------
// Resource handling
// ------------------------------------------------------------------------------------
/**
 * \brief Kind of auto_handle.
 * This simple wrapper acts like 'auto_ptr' but is for HANDLE. It is used to
 * avoid any not-closed HANDLES leaks when the block scope is left (via thrown
 * exception or return e.g.)
 */
struct AutoHandle
{
	HANDLE Handle; ///< The handle storage

	AutoHandle () {
		Handle = NULL;
	}

	~AutoHandle () {
		ForceClose();
	}

	/**
	 * \brief Check whether the handle is not valid
	 */
	bool IsInvalid () {
		return Handle == INVALID_HANDLE_VALUE || Handle == NULL;
	}

	/**
	 * \brief Close the handle
	 */
	void ForceClose()
	{
		if (!IsInvalid()) {
			CloseHandle(Handle);
		}
		Handle = NULL;
	}
};


/**
 * \brief Kind of auto_file.
 * This simple wrapper acts like 'auto_ptr' but is for FILE pointer. It is used
 * to avoid any not-closed FILES leaks when the block scope is left (via thrown
 * exception or return e.g.)
 */
struct AutoFile
{
	FILE* pFile; ///< The file handle storage

	AutoFile () {
		pFile = NULL;
	}

	~AutoFile () {
		ForceClose();
	}

	/**
	 * \brief Check whether the file is not valid
	 */
	bool IsInvalid () {
		return pFile == NULL;
	}

	/**
	 * \brief Close the file
	 */
	void ForceClose()
	{
		if (!IsInvalid()) {
			fclose(pFile);
		}
		pFile = NULL;
	}
};


// ------------------------------------------------------------------------------------
// Miscellaneous helper functions
// ------------------------------------------------------------------------------------

#define DELETE_SURFACE(p) { if (p) { delete ((SurfNative*)p); p = NULL; } }
#define SAFE_DELETE(p)  { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_DELETEA(p)  { if(p) { delete []p;     (p)=NULL; } }
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }
#define CLEARARRAY(p) { memset(p, 0, sizeof(p)); }

#endif // !__D3DUTIL_H

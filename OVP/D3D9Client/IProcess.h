// ===================================================
// Copyright (C) 2021 Jarmo Nikkanen
// licensed under LGPL v2
// ===================================================

#ifndef __IPROCESS_H
#define __IPROCESS_H

#include <d3d9.h> 
#include <d3dx9.h>
#include <list>
#include <map>
#include "OrbiterAPI.h"
#include "D3D9Util.h"
#include "gcCore.h"

// Address mode WRAP is assumed by default
// Filter POINT is assumed by default
/*
#define IPF_WRAP		0x0000
#define IPF_WRAP_U		0x0000
#define IPF_WRAP_V		0x0000
#define IPF_WRAP_W		0x0000
#define IPF_CLAMP		0x0007
#define IPF_CLAMP_U		0x0001
#define IPF_CLAMP_V		0x0002
#define IPF_CLAMP_W		0x0004
#define IPF_MIRROR		0x0038
#define IPF_MIRROR_U	0x0008
#define IPF_MIRROR_V	0x0010
#define IPF_MIRROR_W	0x0020
#define IPF_POINT		0x0000
#define IPF_LINEAR		0x0040
#define IPF_PYRAMIDAL	0x0080
#define IPF_GAUSSIAN	0x0100*/


class ImageProcessing {

public:


	// ----------------------------------------------------------------------------------
	// Create a IPI (Image processing interface) which allows to process and create data via GPU
	// _file is the filename where user shader code exists
	// _entry is the function entry point "myFunc". e.g. float4 myFunc(float x : TEXCOORD0, float y : TEXCOORD1) : COLOR 
	// which contains the executed code with two input variables x, y
	// ppf is a list of preprocessor directives e.g. "_MYSECTION;_DEBUG" used like #if defined(_MYSECTION) ..code.. #endif
	// ----------------------------------------------------------------------------------
			ImageProcessing(LPDIRECT3DDEVICE9 pDev, const char *_file, const char *_entry, const char *ppf = NULL, const char *_vsentry = NULL);
			~ImageProcessing();

	bool	CompileShader(const char *Entry);
	bool	Activate(const char *Shader = NULL);

	// ----------------------------------------------------------------------------------
	// Use the 'Set' functions to assign a value into a shader constants ( e.g. uniform extern float4 myVector; )
	// If the variable "var" is defined but NOT used by the shader code, the variable "var" doesn't exists in
	// a constant table and an error is printed when trying to assign a value to it.
	// ----------------------------------------------------------------------------------
	void	SetFloat(const char *var, float val);
	void	SetInt(const char *var, int val);
	void	SetBool(const char *var, bool val);
	// ----------------------------------------------------------------------------------
	void	SetFloat(const char *var, const void *val, int bytes);
	void	SetInt(const char *var, const int *val, int bytes);
	void	SetBool(const char *var, const bool *val, int bytes);
	void	SetStruct(const char *var, const void *val, int bytes);

	// ----------------------------------------------------------------------------------
	// Use the 'Set' functions to assign a value into a shader constants ( e.g. uniform extern float4 myVector; )
	// If the variable "var" is defined but NOT used by the shader code, the variable "var" doesn't exists in
	// a constant table and an error is printed when trying to assign a value to it.
	// ----------------------------------------------------------------------------------
	void	SetVSFloat(const char *var, float val);
	void	SetVSInt(const char *var, int val);
	void	SetVSBool(const char *var, bool val);
	// ----------------------------------------------------------------------------------
	void	SetVSFloat(const char *var, const void *val, int bytes);
	void	SetVSInt(const char *var, const int *val, int bytes);
	void	SetVSBool(const char *var, const bool *val, int bytes);
	void	SetVSStruct(const char *var, const void *val, int bytes);

	// ----------------------------------------------------------------------------------
	// SetTexture can be used to assign a texture and a sampler state flags to a sampler
	// In a shader code sampler is defined as (e.g. sampler mySamp; ) where "mySamp" is
	// the variable passed to SetTexture function. It's then used in a shader code like
	// tex2D(mySamp, float2(x,y))
	// ----------------------------------------------------------------------------------
	void	SetTexture(const char *var, SURFHANDLE hTex, DWORD flags);
	
	// ----------------------------------------------------------------------------------
	// SetOutput assigns a render target to the IP interface. "id" is an index of the render
	// target with a maximum value of 3. It is possible to render in four different targets
	// at the same time. Multisample AA is only supported with one render target. Unbound
	// a render target by setting it to NULL. After a NULL render target all later targets
	// are ignored.
	// ----------------------------------------------------------------------------------
	void	SetOutput(int id, SURFHANDLE hTex);

	// ----------------------------------------------------------------------------------
	bool	IsOK();
	void	SetTemplate(float w = 1.0f, float h = 1.0f, float x = 0.0f, float y = 0.0f);
	void	SetMesh(const MESHHANDLE hMesh, const char *tex = NULL, gcIPInterface::ipicull = gcIPInterface::ipicull::None);

	bool	Execute(bool bInScene = false);
	bool	Execute(const char *shader, bool bInScene, DWORD blendop);
	bool    Execute(DWORD blendop, bool bInScene = false, gcIPInterface::ipitemplate tmp = gcIPInterface::ipitemplate::Rect, int gpr = -1);

	// ----------------------------------------------------------------------------------
	int		FindDefine(const char *key);

	// Native DirectX calls -------------------------------------------------------------
	//
	void	SetDepthStencil(LPDIRECT3DSURFACE9 hSrf = NULL);
	void	SetOutputNative(int id, LPDIRECT3DSURFACE9 hSrf);
	void	SetTextureNative(const char *var, LPDIRECT3DBASETEXTURE9 hTex, DWORD flags);
	void	SetTextureNative(int idx, LPDIRECT3DBASETEXTURE9 hTex, DWORD flags);

private:

	bool	SetupViewPort();	

	typedef struct {
		LPDIRECT3DPIXELSHADER9 pPixel;
		LPD3DXCONSTANTTABLE pPSConst;
	} SHADER;

	struct {
		LPDIRECT3DBASETEXTURE9 hTex;
		DWORD flags;
	} pTextures[16];

	gcIPInterface::ipicull mesh_cull;

	SketchMesh *pMesh;
	std::map<std::string, SHADER> Shaders;
	LPDIRECT3DDEVICE9 pDevice;
	LPDIRECT3DSURFACE9 pRtg[4], pRtgBak[4];
	LPDIRECT3DSURFACE9 pDepth, pDepthBak;
	LPD3DXCONSTANTTABLE pVSConst;
	LPD3DXCONSTANTTABLE pPSConst;
	LPDIRECT3DVERTEXSHADER9 pVertex;
	LPDIRECT3DPIXELSHADER9 pPixel;
	D3DSURFACE_DESC desc;
	D3DXMATRIX   mVP;
	D3DXVECTOR4  vTemplate;
	D3DVIEWPORT9 iVP;
	D3DXHANDLE   hVP;
	D3DXHANDLE   hPos;
	D3DXHANDLE   hSiz;
	SMVERTEX	*pOcta;

	int		mesh_tex_idx;
	char	file[256];
	char	ppf[256];
	char	entry[32];

	std::list<std::string> def;
};

#endif

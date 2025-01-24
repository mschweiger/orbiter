// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Licensed under LGPL v2
// Copyright (C) 2012 - 2016 Jarmo Nikkanen
// ==============================================================

#ifndef __D3DSURFACE_H
#define __D3DSURFACE_H

#include "OrbiterAPI.h"
#include "D3D9Client.h"
#include "D3D9Pad.h"
#include "GDIPad.h"
//#include "gcCore.h"
#include <d3d9.h>
#include <d3dx9.h>

#define	MAP_NORMAL			0
#define	MAP_SPECULAR		1
#define	MAP_EMISSION		2
#define	MAP_REFLECTION		3
#define	MAP_TRANSLUCENCE	4
#define	MAP_TRANSMITTANCE	5
#define	MAP_ROUGHNESS		6
#define	MAP_METALNESS		7
#define	MAP_HEAT			8
#define MAP_AMBIENT			9
#define MAP_MAX_COUNT		10

#define OAPISURFACE_MAPS		0x80000000		// Additional Texture Maps
#define OAPISURFACE_BACKBUFFER	0x40000000		// It's a backbuffer
#define OAPISURFACE_ORIGIN		0x20000000		// The origin from where the clones are being made, can't change (immutable)
#define OAPISURFACE_CAPTURE		0x10000000		// The origin from where the clones are being made, can't change (immutable)

#define OAPISURF_SKP_GDI_WARN	0x00000001

LPDIRECT3DTEXTURE9	NatLoadTexture(const char* path, bool bNoMips = false);
LPDIRECT3DTEXTURE9	NatLoadSpecialTexture(const char* fname, const char* ext, bool bNoMips = false);
SURFHANDLE			NatLoadSurface(const char* file, DWORD flags, bool bPath = false);
bool				NatSaveSurface(const char* file, LPDIRECT3DRESOURCE9 pResource);
SURFHANDLE			NatCreateSurface(int width, int height, DWORD flags);
SURFHANDLE			NatGetMipSublevel(SURFHANDLE hSrf, int level);
bool				NatGenerateMipmaps(SURFHANDLE hSrf);
SURFHANDLE			NatCompressSurface(SURFHANDLE hSurface, DWORD flags);
bool				NatCreateName(char* out, int mlen, const char* fname, const char* id);
DWORD				NatConvertFormat_DX_to_OAPI(DWORD Format);
DWORD				NatConvertFormat_OAPI_to_DX(DWORD Format);
const char*			NatUsage(DWORD Usage);
const char*			NatPool(D3DPOOL Pool);
const char*			NatOAPIFlags(DWORD AF);
const char*			NatOAPIFormat(DWORD PF);
void				NatDumpResource(LPDIRECT3DRESOURCE9 pResource);
void				NatLoadMaps(SurfNative* pNat, const char* file);
void				NatLoadMap(SurfNative* pNat, const char* file);
bool				NatIsTypeOf(const char*, const char*);


#define ERR_DC_NOT_AVAILABLE		0x1
#define ERR_USED_NOT_DEFINED		0x2


// Every SURFHANDLE in the client is a pointer into the SurfNative class

class SurfNative
{
	friend class D3D9Client;
	friend class D3D9Pad;
	friend class GDIPad;

	struct _HDC_LOCAL {
		HDC hDC;
		LPDIRECT3DSURFACE9 pSrf;
	};

public:

							SurfNative(LPDIRECT3DRESOURCE9 pSrf, DWORD Flags, LPDIRECT3DSURFACE9 pDep = NULL);
							SurfNative(SurfNative* hOrigin);
							~SurfNative();

	void					AddMap(DWORD id, LPDIRECT3DTEXTURE9 pMap);
	const D3DSURFACE_DESC*	GetDesc() const { return &desc; }
	bool					GenerateMipMaps();
	bool					Decompress();
	LPDIRECT3DTEXTURE9		GetGDICache(DWORD Flags);
	void					IncRef() { RefCount++; }
	bool					DecRef() { RefCount--; return RefCount <= 0; }
	bool					DeClone();
	bool					GetSpecs(gcCore::SurfaceSpecs* sp, int size);

	void					Reload();

	DWORD					GetMipMaps() const { return Mipmaps; }
	DWORD					GetWidth() const { return desc.Width; }
	DWORD					GetHeight() const { return desc.Height; }
	DWORD					GetOAPIFlags() const { return Flags; }
	DWORD					GetType() const { return (DWORD)type; }
	DWORD					GetSizeInBytes();
	DWORD*					GetClientFlags();

	const char*				GetName() const { return name; }
	const char*				GetPath() const { return path; }
	void					SetName(const char*);
	void					SetPath(const char*);
	HDC						GetDC();
	void					ReleaseDC(HDC);

	bool					IsGDISurface() const;
	bool					IsCompressed() const;
	bool					IsBackBuffer() const;
	bool					IsTexture() const { return (type == D3DRTYPE_TEXTURE); }
	bool					IsRenderTarget() const;
	bool					Is3DRenderTarget() const;
	bool					IsPowerOfTwo() const;
	bool					IsSystemMem() const { return (desc.Pool == D3DPOOL_SYSTEMMEM); }
	bool					IsAdvanced() const { return (Flags & OAPISURFACE_MAPS); }
	bool					IsColorKeyEnabled() const { return (ColorKey != SURF_NO_CK); }
	bool					IsClone() const { return hOrigin != this; }

	LPDIRECT3DSURFACE9		GetTempSurface();
	LPDIRECT3DRESOURCE9		GetResource() const { return pResource; }
	LPDIRECT3DSURFACE9		GetDepthStencil() const { return pDepth; }
	LPDIRECT3DSURFACE9		GetSurface();
	LPDIRECT3DTEXTURE9		GetTexture() const;
	LPDIRECT3DTEXTURE9		GetMap(int type) const { return pMap[type]; }
	LPDIRECT3DTEXTURE9		GetMap(int type, int type2) const { return (pMap[type] ? pMap[type] : pMap[type2]); }
	D3D9Pad*				GetPooledSketchPad();
	void					SetColorKey(DWORD ck);			// Enable and set color key
	DWORD					GetColorKey() const { return ColorKey; }

	bool					Fill(LPRECT r, DWORD color);

	DWORD					GetTextureSizeInBytes(LPDIRECT3DTEXTURE9 pT);
	DWORD					GetFormatSizeInBytes(D3DFORMAT Format, DWORD pixels);

	void					LogSpecs() const;
	bool					CreateDX7();
	void					DX7Sync(bool bUp);


	// -------------------------------------------------------------------------------

	char					name[128];				// Surface name
	char					path[MAX_PATH];			// Surface name with path
	SURFHANDLE				hOrigin;
	D3DSURFACE_DESC			desc;					// Surface size and format description
	D3DRESOURCETYPE			type;					// Resource type
	LPDIRECT3DTEXTURE9		pGDICache;				// Low level GDI cache for surface syncing
	LPDIRECT3DSURFACE9		pTemp;					// Cache for in-surface blitting
	LPDIRECT3DSURFACE9		pDepth;					// DepthStencil surface for 3D rendering
	LPDIRECT3DSURFACE9		pTexSurf;				// Texture "surface" level cache
	LPDIRECT3DRESOURCE9		pResource;				// Main resource
	LPDIRECT3DSURFACE9		pDX7;
	LPDIRECT3DTEXTURE9		pMap[MAP_MAX_COUNT];	// Additional texture maps _norm, _rghn, _spec, etc...
	LPDIRECT3DDEVICE9		pDevice;
	DWORD					ColorKey;
	DWORD					Flags;					// Surface Flags/Attribs
	DWORD					Mipmaps;				// Mipmap count. 1 = no mipmaps
	DWORD					ClientFlags;
	int						RefCount;
	D3D9Pad*				pSkp;					// Pooled sketchpad interface cache
	_HDC_LOCAL				DC;
};


#endif

// ==============================================================
// MaterialMgr.h
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2012 - 2016 Jarmo Nikkanen
// ==============================================================

#ifndef __MATERIALMGR_H
#define __MATERIALMGR_H

#include <d3d9.h>
#include <d3dx9.h>

#include "Mesh.h"
#include "D3D9Client.h"
#include "D3D9Util.h"
#include "vObject.h"

/**
 * \brief Management of custom configurations for vessel materials
 */
class MatMgr {

public:
	// Disable copy construct & copy assign
					MatMgr    (MatMgr const&) = delete;
	MatMgr &		operator= (MatMgr const&) = delete;

					MatMgr(class vObject *vObj, class D3D9Client *_gc);
					~MatMgr();

	void			RegisterMaterialChange(D3D9Mesh *pMesh, DWORD midx, const D3D9MatExt *pM);
	void			RegisterShaderChange(D3D9Mesh *pMesh, WORD id);
	void			ApplyConfiguration(D3D9Mesh *pMesh);
	bool			SaveConfiguration();
	bool			LoadConfiguration(bool bAppend=false);
	bool			LoadCameraConfig();
	bool			HasMesh(const char *name);

private:

	vObject			*vObj;
	D3D9Client		*gc;

	struct SHADER {
		SHADER(string x, WORD i) { name = x; id = i; }
		string name;
		WORD id;
	};

	struct MESHREC {
		WORD shader;
		map<int, D3D9MatExt> material;
	};

	std::map<string, MESHREC> MeshConfig;
	std::list<SHADER> Shaders;
};

#endif

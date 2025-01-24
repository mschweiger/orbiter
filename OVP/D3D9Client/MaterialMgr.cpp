// ===========================================================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2013 - 2016 Jarmo Nikkanen
// ===========================================================================================


#include "MaterialMgr.h"
#include "D3D9Surface.h"
#include "OapiExtension.h"
#include "vVessel.h"



// ===========================================================================================
//
MatMgr::MatMgr(class vObject *v, class D3D9Client *_gc)
{
	gc = _gc;
	vObj = v;
	
	Shaders.push_back(SHADER("PBR-Old", SHADER_NULL));
	Shaders.push_back(SHADER("Metalness", SHADER_METALNESS));
	Shaders.push_back(SHADER("BakedVC", SHADER_BAKED_VC));
}

// ===========================================================================================
//
MatMgr::~MatMgr()
{
	MeshConfig.clear();
}

// ===========================================================================================
//
void MatMgr::RegisterMaterialChange(D3D9Mesh *pMesh, DWORD midx, const D3D9MatExt *pM)
{
	if (!pMesh || !pM) return;
	MeshConfig[pMesh->GetName()].material[midx] = *pM;
}

// ===========================================================================================
//
void MatMgr::RegisterShaderChange(D3D9Mesh *pMesh, WORD id)
{
	if (!pMesh) return;
	for (auto y : Shaders) if (y.id == id) {
		MeshConfig[pMesh->GetName()].shader = id;
		break;
	}
}


// ===========================================================================================
//
void MatMgr::ApplyConfiguration(D3D9Mesh *pMesh)
{
	if (pMesh==NULL) return;

	const char *name = pMesh->GetName();

	LogAlw("Applying custom configuration to a mesh (%s)",name);

	if (MeshConfig.count(name)) 
	{
		pMesh->SetDefaultShader(MeshConfig[name].shader);

		for (auto x : MeshConfig[name].material) 
		{
			auto rec = x.second;
			
			if (x.first >= int(pMesh->GetMaterialCount())) {
				LogErr("MatMgr::ApplyConfiguration: Matrial Idx out of range [%s.msh]", name);
				continue;
			}

			D3D9MatExt Mat;
			auto RecMat = x.second;
			DWORD flags = RecMat.ModFlags;

			if (!pMesh->GetMaterial(&Mat, x.first)) continue;

			if (flags&D3D9MATEX_AMBIENT) Mat.Ambient = RecMat.Ambient;
			if (flags&D3D9MATEX_DIFFUSE) Mat.Diffuse = RecMat.Diffuse;
			if (flags&D3D9MATEX_EMISSIVE) Mat.Emissive = RecMat.Emissive;
			if (flags&D3D9MATEX_REFLECT) Mat.Reflect = RecMat.Reflect;
			if (flags&D3D9MATEX_SPECULAR) Mat.Specular = RecMat.Specular;
			if (flags&D3D9MATEX_FRESNEL) Mat.Fresnel = RecMat.Fresnel;
			if (flags&D3D9MATEX_EMISSION2) Mat.Emission2 = RecMat.Emission2;
			if (flags&D3D9MATEX_ROUGHNESS) Mat.Roughness = RecMat.Roughness;
			if (flags&D3D9MATEX_METALNESS) Mat.Metalness = RecMat.Metalness;

			Mat.ModFlags = flags;

			pMesh->SetMaterial(&Mat, x.first);

			LogBlu("Material %u setup applied to mesh (%s) Flags=0x%X", x.first, name, flags);
		}
	}
}

// ===========================================================================================
//
bool MatMgr::HasMesh(const char *name)
{
	if (MeshConfig.count(name)) return true;
	return false;
}

// ===========================================================================================
//
void parse_vessel_classname(char *lbl)
{
	int i = -1;
	while (lbl[++i]!=0) if (lbl[i]=='/' || lbl[i]=='\\') lbl[i]='_';
}

// ===========================================================================================
//
bool MatMgr::LoadConfiguration(bool bAppend)
{
	_TRACE;

	char cbuf[256];
	char path[256];
	char classname[256];
	char meshname[64];
	char shadername[64];

	OBJHANDLE hObj = vObj->GetObjHandle();

	if (oapiGetObjectType(hObj)!=OBJTP_VESSEL) return false; 

	const char *cfgdir = OapiExtension::GetConfigDir();

	VESSEL *vessel = oapiGetVesselInterface(hObj);
	strcpy_s(classname, 256, vessel->GetClassNameA());
	parse_vessel_classname(classname);

	AutoFile file;

	if (file.IsInvalid()) {
		sprintf_s(path, 256, "%sGC\\%s.cfg", cfgdir, classname);
		fopen_s(&file.pFile, path, "r");	
	}

	if (file.IsInvalid()) return true;

	LogAlw("Reading a custom configuration file for a vessel %s (%s)", vessel->GetName(), vessel->GetClassNameA());
	
	DWORD n = 0;
	int mat_idx = -1;

	while (fgets2(cbuf, 256, file.pFile, 0x0A)>=0) 
	{	
		float a, b, c, d;
		
		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "MESH", 4)) {
			mat_idx = -1;
			if (sscanf_s(cbuf, "MESH %s", meshname, 64)!=1) LogErr("Invalid Line in (%s): %s", path, cbuf);
			if (strncmp(meshname, "???", 3) == 0) meshname[0] = 0;
			if (HasMesh(meshname) && bAppend) meshname[0] = 0; // Mesh is loaded already skip all entries related to it.
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (meshname[0] == 0) continue;  // Do not continue without a valid mesh

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "SHADER", 6)) {
			MeshConfig[meshname].shader = SHADER_NULL;
			if (sscanf_s(cbuf, "SHADER %s", shadername, 64) != 1) LogErr("Invalid Line in (%s): %s", path, cbuf);
			for (auto x : Shaders)
				if (string(shadername) == x.name) {
					MeshConfig[meshname].shader = x.id;
					LogOapi("NewShader [%s]=%hX", meshname, x.id);
				}
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "MATERIAL", 8)) {
			if (sscanf_s(cbuf, "MATERIAL %d", &mat_idx)!=1) LogErr("Invalid Line in (%s): %s", path, cbuf);
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (mat_idx == -1) continue;  // Do not continue without a valid material idx

		auto &Mat = MeshConfig[meshname].material[mat_idx];

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "SPECULAR", 8)) {
			if (sscanf_s(cbuf, "SPECULAR %f %f %f %f", &a, &b, &c, &d)!=4) LogErr("Invalid Line in (%s): %s", path, cbuf);
			Mat.Specular = D3DXVECTOR4(a, b, c, d);
			Mat.ModFlags |= D3D9MATEX_SPECULAR;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "DIFFUSE", 7)) {
			if (sscanf_s(cbuf, "DIFFUSE %f %f %f %f", &a, &b, &c, &d)!=4) LogErr("Invalid Line in (%s): %s", path, cbuf);
			Mat.Diffuse = D3DXVECTOR4(a, b, c, d);
			Mat.ModFlags |= D3D9MATEX_DIFFUSE;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "EMISSIVE", 8)) {
			if (sscanf_s(cbuf, "EMISSIVE %f %f %f", &a, &b, &c)!=3) LogErr("Invalid Line in (%s): %s", path, cbuf);
			Mat.Emissive = D3DXVECTOR3(a, b, c);
			Mat.ModFlags |= D3D9MATEX_EMISSIVE;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "EMISSION2", 9)) {
			if (sscanf_s(cbuf, "EMISSION2 %f %f %f", &a, &b, &c) != 3) LogErr("Invalid Line in (%s): %s", path, cbuf);
			Mat.Emission2 = D3DXVECTOR3(a, b, c);
			Mat.ModFlags |= D3D9MATEX_EMISSION2;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "AMBIENT", 7)) {
			if (sscanf_s(cbuf, "AMBIENT %f %f %f", &a, &b, &c)!=3) LogErr("Invalid Line in (%s): %s", path, cbuf);
			Mat.Ambient = D3DXVECTOR3(a, b, c);
			Mat.ModFlags |= D3D9MATEX_AMBIENT;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "REFLECT", 7)) {
			if (sscanf_s(cbuf, "REFLECT %f %f %f", &a, &b, &c) != 3) LogErr("Invalid Line in (%s): %s", path, cbuf);
			Mat.Reflect = D3DXVECTOR3(a, b, c);
			Mat.ModFlags |= D3D9MATEX_REFLECT;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "FRESNEL", 7)) {
			if (sscanf_s(cbuf, "FRESNEL %f %f %f", &a, &b, &c) != 3) LogErr("Invalid Line in (%s): %s", path, cbuf);
			if (b < 10.0f) b = 1024.0f;
			Mat.Fresnel = D3DXVECTOR3(a, c, b);
			Mat.ModFlags |= D3D9MATEX_FRESNEL;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "ROUGHNESS", 9)) {
			int cnt = sscanf_s(cbuf, "ROUGHNESS %f %f", &a, &b);
			if (cnt == 1) Mat.Roughness = D3DXVECTOR2(a, 1.0f);
			else if (cnt == 2)  Mat.Roughness = D3DXVECTOR2(a, b);
			else LogErr("Invalid Line in (%s): %s", path, cbuf);
			Mat.ModFlags |= D3D9MATEX_ROUGHNESS;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "SMOOTHNESS", 10)) {
			int cnt = sscanf_s(cbuf, "SMOOTHNESS %f %f", &a, &b);
			if (cnt == 1) Mat.Roughness = D3DXVECTOR2(a, 1.0f);
			else if (cnt == 2)  Mat.Roughness = D3DXVECTOR2(a, b);
			else LogErr("Invalid Line in (%s): %s", path, cbuf);
			Mat.ModFlags |= D3D9MATEX_ROUGHNESS;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "METALNESS", 9)) {
			if (sscanf_s(cbuf, "METALNESS %f", &a) != 1) LogErr("Invalid Line in (%s): %s", path, cbuf);
			Mat.Metalness = a;
			Mat.ModFlags |= D3D9MATEX_METALNESS;
			continue;
		}
	}

	return true;
}


// ===========================================================================================
//
bool MatMgr::SaveConfiguration()
{
	_TRACE;
	bool bIfStatement = false;

	char path[256];
	char classname[256];
	
	
	OBJHANDLE hObj = vObj->GetObjHandle();

	if (oapiGetObjectType(hObj)!=OBJTP_VESSEL) return false; 

	VESSEL *vessel = oapiGetVesselInterface(hObj);
	const char *cfgdir = OapiExtension::GetConfigDir();

	strcpy_s(classname, 256, vessel->GetClassNameA());
	parse_vessel_classname(classname);

	AutoFile file;
	sprintf_s(path, 256, "%sGC\\%s.cfg", cfgdir, classname);
	
	// If the target file contains configurations those are not loaded into the editor,
	// Load them before overwriting the file
	LoadConfiguration(true);

	fopen_s(&file.pFile, path, "w");

	if (file.IsInvalid()) {
		LogErr("Failed to write a file");
		return false;
	}

	fprintf(file.pFile, "CONFIG_VERSION 3\n");

	for (auto x : MeshConfig) 
	{		
		string current = x.first;

		fprintf(file.pFile,"; =============================================\n");
		fprintf(file.pFile, "MESH %s\n", current.c_str());

		for (auto y : Shaders) if (y.id == x.second.shader) fprintf(file.pFile, "SHADER %s\n", y.name.c_str());
		
		for (auto rec : x.second.material) 
		{		
			DWORD flags = rec.second.ModFlags;
			D3D9MatExt *pM = &rec.second;

			if (flags==0) continue;

			fprintf(file.pFile,"; ---------------------------------------------\n");
			fprintf(file.pFile,"MATERIAL %u\n", rec.first);
					
			if (flags&D3D9MATEX_AMBIENT)  fprintf(file.pFile,"AMBIENT %f %f %f\n", pM->Ambient.x, pM->Ambient.y, pM->Ambient.z);
			if (flags&D3D9MATEX_DIFFUSE)  fprintf(file.pFile,"DIFFUSE %f %f %f %f\n", pM->Diffuse.x, pM->Diffuse.y, pM->Diffuse.z, pM->Diffuse.w);
			if (flags&D3D9MATEX_SPECULAR) fprintf(file.pFile,"SPECULAR %f %f %f %f\n", pM->Specular.x, pM->Specular.y, pM->Specular.z, pM->Specular.w);
			if (flags&D3D9MATEX_EMISSIVE) fprintf(file.pFile,"EMISSIVE %f %f %f\n", pM->Emissive.x, pM->Emissive.y, pM->Emissive.z);
			if (flags&D3D9MATEX_REFLECT)  fprintf(file.pFile,"REFLECT %f %f %f\n", pM->Reflect.x, pM->Reflect.y, pM->Reflect.z);
			if (flags&D3D9MATEX_FRESNEL)  fprintf(file.pFile,"FRESNEL %f %f %f\n", pM->Fresnel.x, pM->Fresnel.z, pM->Fresnel.y);
			if (flags&D3D9MATEX_EMISSION2) fprintf(file.pFile, "EMISSION2 %f %f %f\n", pM->Emission2.x, pM->Emission2.y, pM->Emission2.z);
			if (flags&D3D9MATEX_ROUGHNESS) fprintf(file.pFile, "SMOOTHNESS %f %f\n", pM->Roughness.x, pM->Roughness.y);
			if (flags&D3D9MATEX_METALNESS) fprintf(file.pFile, "METALNESS %f\n", pM->Metalness);		
		}
	}
	return true;
}


// ===========================================================================================
//
bool MatMgr::LoadCameraConfig()
{
	_TRACE;

	char cbuf[256];
	char path[256];
	char classname[256];

	OBJHANDLE hObj = vObj->GetObjHandle();

	if (oapiGetObjectType(hObj)!=OBJTP_VESSEL) return false; 

	const char *cfgdir = OapiExtension::GetConfigDir();
	
	VESSEL *vessel = oapiGetVesselInterface(hObj);
	strcpy_s(classname, 256, vessel->GetClassNameA());
	parse_vessel_classname(classname);

	AutoFile file;

	sprintf_s(path, 256, "%sGC\\%s_ecam.cfg", cfgdir, classname);
	fopen_s(&file.pFile, path, "r");	
	
	if (file.IsInvalid()) return true;

	LogAlw("Reading a camera configuration file for a vessel %s (%s)", vessel->GetName(), vessel->GetClassNameA());

	ENVCAMREC* pCamera = NULL;

	while(fgets2(cbuf, 256, file.pFile, 0x08)>=0) 
	{	
		float a, b, c;

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "END_CAMERA", 10)) {
			pCamera = NULL;
			continue;
		}
		
		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "BEGIN_CAMERA", 12)) {
			int idx = -1;
			if (sscanf_s(cbuf, "BEGIN_CAMERA %d", &idx)!=1) LogErr("Invalid Line in (%s): %s", path, cbuf);
			if (idx == 0) {
				pCamera = ((vVessel*)vObj)->CreateEnvCam(EnvCamType::Exterior);
				pCamera->id = -1;
				pCamera->flags = 0; // Clear default flags
			}
			if (idx == 1) {
				pCamera = ((vVessel*)vObj)->CreateEnvCam(EnvCamType::Interior);
				pCamera->id = -1;
				pCamera->flags = 0; // Clear default flags
			}
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "LPOS", 4)) {
			if (sscanf_s(cbuf, "LPOS %g %g %g", &a, &b, &c)!=3) LogErr("Invalid Line in (%s): %s", path, cbuf);
			pCamera->lPos = FVECTOR3(a,b,c);
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "CLIPDIST", 8)) {
			if (sscanf_s(cbuf, "CLIPDIST %g", &a)!=1) LogErr("Invalid Line in (%s): %s", path, cbuf);
			pCamera->near_clip = a;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "OMIT_ALL_ATTC", 13)) {
			pCamera->flags |= ENVCAM_OMIT_ATTC;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "DO_NOT_OMIT_FOCUS", 17)) {
			pCamera->flags |= ENVCAM_FOCUS;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "OMIT_ALL_DOCKS", 14)) {
			pCamera->flags |= ENVCAM_OMIT_DOCKS;
			continue;
		}

		if (cbuf[0]!=';') LogErr("Invalid Line in (%s): %s", path, cbuf);
	}

	return true;
}




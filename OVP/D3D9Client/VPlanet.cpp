// ==============================================================
// VPlanet.cpp
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2006-2016 Martin Schweiger
//				 2010-2016 Jarmo Nikkanen
// ==============================================================

// ==============================================================
// class vPlanet (implementation)
//
// A vPlanet is the visual representation of a "planetary" object
// (planet, moon, asteroid).
// Currently this only supports spherical objects, without
// variations in elevation.
// ==============================================================

#define D3D_OVERLOADS

#include <map>
#include <sstream>
#include <unordered_map>

#include "D3D9Client.h"
#include "D3D9Config.h"
#include "VPlanet.h"
#include "VBase.h"
#include "SurfMgr.h"
#include "surfmgr2.h"
#include "cloudmgr2.h"
#include "CloudMgr.h"
#include "HazeMgr.h"
#include "RingMgr.h"
#include "DebugControls.h"
#include "AtmoControls.h"
#include "VectorHelpers.h"
#include "OapiExtension.h"
#include "IProcess.h"
#include <filesystem>

using namespace oapi;

// ==============================================================

static double farplane = 1e6;
static double max_surf_dist = 1e4;

// Buffered MicroTex.cfg file read:
static std::map<std::string, vPlanet::_MicroCfg> MicroCfgs;
typedef std::map<std::string, vPlanet::_MicroCfg>::iterator MicroCfgsIterator;

ImageProcessing* vPlanet::pIP;
LPDIRECT3DDEVICE9 vPlanet::pDev;
LPDIRECT3DTEXTURE9 vPlanet::ptEclipse;

int vPlanet::Qc = 0;
int vPlanet::Wc = 0;
int vPlanet::Nc = 0;

extern int SURF_MAX_PATCHLEVEL;
extern D3D9Client* g_client;
extern unordered_map<std::string, LPDIRECT3DTEXTURE9> MicroTextures;

// ==============================================================
// Face's Terrain Flattening section
// ==============================================================

struct FlatShape
{
	double Lat;
	double Lng;
	double Dim1;
	double Dim2;
	double Cos;
	double Sin;
	double Falloff;
	int Height;
	int Type;
};

struct TilePixelPosition
{
	int iLat;
	int iLng;
	int X;
	int Y;
	bool operator==(const TilePixelPosition& other) const
	{
		return iLat == other.iLat && iLng == other.iLng && X == other.X && Y == other.Y;
	}
};

std::unordered_map<OBJHANDLE, std::unordered_map<int, std::unordered_map<int, std::unordered_map<int, std::vector<FlatShape*>>>>> g_ElevationFlatteningShapes;
std::unordered_map<OBJHANDLE, std::vector<FlatShape*>> g_ShapeStore;
std::unordered_map<OBJHANDLE, bool> g_ShapesLoaded;

std::vector<std::string> EnumerateDirectory(std::string directory, std::string filter)
{
	std::vector<std::string> result;
	std::string search_path = directory + "\\" + filter;
	WIN32_FIND_DATA fd;
	HANDLE hFind = ::FindFirstFile(search_path.c_str(), &fd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) result.push_back(fd.cFileName);
		} while (::FindNextFile(hFind, &fd));
		::FindClose(hFind);
	}
	return result;
}

void GetTilePixelPosition(int lvl, double lat, double lng, TilePixelPosition& pos)
{
	//Normalize input
	if (lat < -90) lat = -90;
	if (lat > 90) lat = 90;
	while (lng < -180)
	{
		lng += 360;
	}
	while (lng > 180)
	{
		lng -= 360;
	}

	//Calculate tile position and pixel position
	double bands = pow((double)2, lvl);
	double width = 180 / bands;
	pos.iLat = (int)((-lat + 90) / width);
	pos.iLng = (int)((lng + 180) / width);
	double step = width / 256;
	double latmin = 90 - width * (pos.iLat + 1);
	double lngmin = -180 + width * pos.iLng;
	pos.X = (int)((lng - lngmin) / width * 256);
	pos.Y = (int)(-(lat - latmin) / width * 256 + 256);
}

void ProcessPlanetFlats(OBJHANDLE hPlanet)
{
	char name[MAX_PATH];
	char fname[MAX_PATH];
	oapiGetObjectName(hPlanet, name, ARRAYSIZE(name) - 6);
	sprintf_s(fname, ARRAYSIZE(fname), "%s\\Flat", name);
	g_client->TexturePath(fname, name);
	auto files = EnumerateDirectory(name, "*.flt");
	// Load all planet shapes
	for (auto file : files)
	{
		auto radius = oapiGetSize(hPlanet);
		sprintf_s(fname, ARRAYSIZE(fname), "%s\\%s", name, file.c_str());
		auto f = fopen(fname, "r");
		if (f != 0)
		{
			while (!feof(f))
			{
				int height, dim1, dim2, falloff, read;
				double lat, lng, phi;
				if ((read = fscanf(f, "%s %d %lf %lf %d %d %lf %d", fname, &height, &lng, &lat, &dim1, &dim2, &phi, &falloff)) < 5) continue; // Skip incomplete lines
				if (fname[0] == '/' && fname[1] == '/') continue; // Skip commented lines
				_strlwr(fname);
				if (read < 6)	dim2 = dim1; // Fallback for one dimension only
				if (read < 7)	phi = 0;     // Fallback for no angle given
				if (read < 8)	falloff = 0; // Fallback for no falloff given
				auto decdeg = radius * cos(lat * RAD) * 2 * PI;
				decdeg = 360 / decdeg;
				if (strcmp(fname, "ellipse") == 0)
				{
					g_ShapeStore[hPlanet].push_back(new FlatShape{ lat ,lng , (double)dim1 * decdeg, (double)dim2 * decdeg, cos(-phi * RAD),sin(-phi * RAD), (double)falloff / 100, height, 1 });
				}
				else if (strcmp(fname, "rect") == 0)
				{
					g_ShapeStore[hPlanet].push_back(new FlatShape{ lat ,lng , (double)dim1 * decdeg / 2, (double)dim2 * decdeg / 2, cos(-phi * RAD),sin(-phi * RAD), (double)falloff / 100, height, 4 });
				}
			}
			fclose(f);
		}
	}
	// Build up planet map only if at least one shape was added
	if (g_ShapeStore.find(hPlanet) != g_ShapeStore.end()) for (auto shape : g_ShapeStore[hPlanet])
	{
		double offset = sqrt(shape->Dim1 * shape->Dim1 + shape->Dim2 * shape->Dim2);
		double latmin = shape->Lat - offset;
		double latmax = shape->Lat + offset;
		double lngmin = shape->Lng - offset;
		double lngmax = shape->Lng + offset;
		TilePixelPosition leftUpper, rightUpper, leftLower, rightLower;
		for (auto lvl = 13; lvl >= 0; lvl--)
		{
			int latBands = 1 << lvl;
			int lngBands = latBands * 2;

			// Get tile pixel position of all corner points
			GetTilePixelPosition(lvl, latmax, lngmin, leftUpper);
			GetTilePixelPosition(lvl, latmax, lngmax, rightUpper);
			GetTilePixelPosition(lvl, latmin, lngmin, leftLower);
			// Since we are symmetrically, we can deduce the last corner directly
			rightLower.iLat = leftLower.iLat;
			rightLower.iLng = rightUpper.iLng;
			rightLower.X = rightUpper.X;
			rightLower.Y = leftLower.Y;
			// Check for subpixel visibility
			if (leftUpper == rightUpper || leftLower == rightLower || leftUpper == leftLower || rightUpper == rightLower) break;
			// Shift if longitude edge is crossed
			if (leftUpper.iLng > rightUpper.iLng)
			{
				leftUpper.iLng -= lngBands;
				leftLower.iLng -= lngBands;
			}
			// Check for edge pixel to compensate overlapping in elevation tiles
			if (leftUpper.X == 0)
			{
				leftUpper.iLng--;
				leftLower.iLng--;
			}
			if (rightUpper.X == 255)
			{
				rightUpper.iLng++;
				rightLower.iLng++;
			}
			if (leftUpper.Y == 0 && leftUpper.iLat > 0)
			{
				leftUpper.iLat--;
				rightUpper.iLat--;
			}
			if (leftLower.Y == 255 && leftLower.iLat < latBands)
			{
				leftLower.iLat++;
				rightLower.iLat++;
			}
			// Sweep over the tile set to put in shape paths
			for (auto ilat = leftUpper.iLat; ilat <= leftLower.iLat; ilat++)
				for (auto ilng = leftUpper.iLng; ilng <= rightUpper.iLng; ilng++)
				{
					// Compensate for longitude edge switch
					g_ElevationFlatteningShapes[hPlanet][lvl][ilat][ilng < 0 ? lngBands + ilng : ilng].push_back(shape);
				}
		}
	}
	g_ShapesLoaded[hPlanet] = true;
}

template <typename Type>
bool FilterElevation(OBJHANDLE hPlanet, int lvl, int ilat, int ilng, double elev_res, Type* elev)
{
	if (!elev) return false;
	if (g_ShapesLoaded.find(hPlanet) == g_ShapesLoaded.end()) ProcessPlanetFlats(hPlanet);
	if (g_ElevationFlatteningShapes.find(hPlanet) == g_ElevationFlatteningShapes.end()) return false; // Nothing for planet
	auto planetShapes = g_ElevationFlatteningShapes[hPlanet];
	if (planetShapes.find(lvl) == planetShapes.end()) return false; // Nothing at this level
	auto levelShapes = planetShapes[lvl];
	if (levelShapes.find(ilat) == levelShapes.end()) return false; // Nothing at this latitude
	auto latShapes = levelShapes[ilat];
	if (latShapes.find(ilng) == latShapes.end()) return false; // Nothing at this longitude
	auto shapes = latShapes[ilng];
	double bands = pow((double)2, lvl);
	double width = 180 / bands;
	double step = width / 256;
	double latmin = 90 - width * (ilat + 1) - step * 1.5;
	double lngmin = -180 + width * ilng - step * 1.5;;
	for (auto i = 0; i < TILE_ELEVSTRIDE * TILE_ELEVSTRIDE; i++)
	{
		for (auto shape : shapes)
		{
			double y = latmin + (i / 259) * step - shape->Lat;
			double x = lngmin + (i % 259) * step - shape->Lng;
			double x1 = (x * shape->Cos - y * shape->Sin) / shape->Dim1;
			double y1 = (x * shape->Sin + y * shape->Cos) / shape->Dim2;
			for (auto pot = 0; pot < shape->Type; pot++)
			{
				x1 *= x1;
				y1 *= y1;
			}
			double dist = x1 + y1;
			if (dist <= 1.0)
			{
				// Do the math in INT16 basis even for "float" output
				INT16 elevation = INT16((double)shape->Height / elev_res);
				if (shape->Falloff > 0)
				{
					for (auto pot = 0; pot < shape->Type; pot++)
					{
						dist = sqrt(dist);
					}
					if (dist >= 1 - shape->Falloff)
					{
						double alpha = (dist - 1 + shape->Falloff) / shape->Falloff;
						elevation = INT16(elev[i] * alpha + (double)shape->Height / elev_res * (1 - alpha));
					}
				}
				// Convert to float or keep as an INT16 depending on case
				elev[i] = Type(elevation);
			}
		}
	}
	return true;
}


bool FilterElevationPhysics(OBJHANDLE hPlanet, int lvl, int ilat, int ilng, double elev_res, INT16* elev)
{
	if (!Config->bFlats) return false;
	char name[64];
	oapiGetObjectName(hPlanet, name, 64);
	auto result = FilterElevation<INT16>(hPlanet, lvl, ilat, ilng, elev_res, elev);
	if (result)
		LogClr("Coral", "FilterElevation[Physics][%s]: Level=%d, ilat=%d, ilng=%d", name, lvl, ilat, ilng);
	return result;
}


void FilterElevationGraphics(OBJHANDLE hPlanet, int lvl, int ilat, int ilng, float* elev)
{
	if (!Config->bFlats) return;
	char name[64];
	oapiGetObjectName(hPlanet, name, 64);
	if (FilterElevation<float>(hPlanet, lvl, ilat, ilng, 1.0, elev))
		LogClr("Coral", "FilterElevation[Graphics][%s]: Level=%d, ilat=%d, ilng=%d", name, lvl, ilat, ilng);
}







// ==============================================================

void vPlanet::GlobalExit()
{
	// Face's Cleanup ShapeStore -----------------------
	//
	for (auto store : g_ShapeStore)
	{
		for (auto shape : store.second)
		{
			delete shape;
		}
	}
	g_ShapeStore.clear();
	g_ElevationFlatteningShapes.clear();
	g_ShapesLoaded.clear();

	SAFE_DELETE(pIP);
	SAFE_RELEASE(ptEclipse);

	for (int i=0;i<8;i++) SAFE_DELETE(pRender[i]);
}

// ==============================================================

void vPlanet::GlobalInit(oapi::D3D9Client* gc)
{
	pDev = gc->GetDevice();

	D3DXCreateTexture(pDev, 512, 1, 1, D3DUSAGE_DYNAMIC, D3DFMT_R32F, D3DPOOL_DEFAULT, &ptEclipse);
	LoadMicroTextures(pDev);
	GlobalInitAtmosphere(gc);
}

// ==============================================================

vPlanet::vPlanet (OBJHANDLE _hObj, const Scene *scene) :
	vObject (_hObj, scene),
	pSunColor(), pRaySkyView(), pMieSkyView(), pLandViewRay(), pLandViewMie(), pAmbientSky(), pLandViewAtn(), ShaderName("Auto\0")
{
	memset(&MicroCfg, 0, sizeof(MicroCfg));
	vRefPoint = _V(1,0,0);
	atm_mode = 0;
	iConfig = 0;
	dist_scale = 1.0f;
	threshold = 1e16;
	dwSctFrame = 0;
	max_centre_dist = 0.9*scene->GetCameraFarPlane();
	maxdist = max (max_centre_dist, max_surf_dist + size);
	DWORD elev_mode = *(DWORD*)gc->GetConfigParam(CFGPRM_ELEVATIONMODE);

	minelev = *(double*)oapiGetObjectParam(_hObj, OBJPRM_PLANET_MINELEVATION);
	maxelev = *(double*)oapiGetObjectParam(_hObj, OBJPRM_PLANET_MAXELEVATION);

	physics_patchres = *(DWORD*)oapiGetObjectParam (_hObj, OBJPRM_PLANET_SURFACEMAXLEVEL);
	physics_patchres = min (physics_patchres, *(DWORD*)gc->GetConfigParam (CFGPRM_SURFACEMAXLEVEL));

	// Push the graphics resolution higher than the one used for physics
	// to enable more accurate bilinear interpolation of the terrain.
	max_patchres = physics_patchres + 4;

	tilever = *(int*)oapiGetObjectParam (_hObj, OBJPRM_PLANET_TILEENGINE);
	if (tilever < 2) {
		surfmgr = new SurfaceManager (gc, this);
		surfmgr2 = NULL;
	} else {
		LoadAtmoConfig();
		surfmgr = NULL;
		int patchlvl = 1 << (4 + Config->MeshRes);
		surfmgr2 = new TileManager2<SurfTile> (this, max_patchres, patchlvl);
		prm.horizon_excess = *(double*)oapiGetObjectParam (_hObj, OBJPRM_PLANET_HORIZONEXCESS);
		prm.tilebb_excess = *(double*)oapiGetObjectParam (_hObj, OBJPRM_PLANET_TILEBBEXCESS);
	}

	prm.horizon_minrad = min (1.0 + minelev / size, 1.0 - 1e-4);

	prm.bAtm = oapiPlanetHasAtmosphere (_hObj);
	if (prm.bAtm) {
		const ATMCONST *atmc = oapiGetPlanetAtmConstants(_hObj);
		prm.atm_hzalt = atmc->horizonalt;
		prm.atm_href = log(atmc->rho0)*2e4 + 2e4;
		prm.atm_amb0 = min (0.7, log1p(atmc->rho0)*0.35);
		DWORD amb0 = *(DWORD*)gc->GetConfigParam (CFGPRM_AMBIENTLEVEL);
		prm.amb0col = 0;
		for (int i = 0; i < 4; i++) prm.amb0col |= amb0 << (i<<3);
	}
	tile_cache = NULL;
	hazemgr = NULL;
	hazemgr2 = NULL;
	hashaze = *(bool*)gc->GetConfigParam (CFGPRM_ATMHAZE) && prm.bAtm;
	bRipple = *(bool*)gc->GetConfigParam (CFGPRM_SURFACERIPPLE) &&
		*(bool*)oapiGetObjectParam (_hObj, OBJPRM_PLANET_SURFACERIPPLE);
	if (bRipple) {
		if (surfmgr) surfmgr->SetMicrotexture ("waves.dds");
	}

	shadowalpha = (float)(*(double*)oapiGetObjectParam (_hObj, OBJPRM_PLANET_SHADOWCOLOUR));

	bVesselShadow = (shadowalpha < 0.98);
	bObjectShadow = (shadowalpha < 0.98);

	clouddata = 0;
	cloudmgr2 = 0;
	prm.bCloud = (*(bool*)gc->GetConfigParam (CFGPRM_CLOUDS) &&
		*(bool*)oapiGetObjectParam (_hObj, OBJPRM_PLANET_HASCLOUDS));
	if (prm.bCloud) {
		int cloudtilever = *(int*)oapiGetObjectParam (_hObj, OBJPRM_PLANET_CLOUDTILEENGINE);
		prm.cloudalt = *(double*)oapiGetObjectParam (_hObj, OBJPRM_PLANET_CLOUDALT);
		prm.bCloudBrighten = *(bool*)oapiGetObjectParam (_hObj, OBJPRM_PLANET_CLOUDOVERSATURATE);
		prm.bCloudShadow = *(bool*)gc->GetConfigParam (CFGPRM_CLOUDSHADOWS);
		prm.shadowalpha = 1.0 - *(float*)oapiGetObjectParam (_hObj, OBJPRM_PLANET_CLOUDSHADOWCOL);
		if (prm.shadowalpha < 0.01)
			prm.bCloudShadow = false;
		if (cloudtilever == 1) { // legacy cloud engine
			clouddata = new CloudData;
			clouddata->cloudmgr = new CloudManager (gc, this);
			clouddata->cloudshadow = prm.bCloudShadow;
			if (clouddata->cloudshadow) {
				clouddata->shadowalpha = (float)prm.shadowalpha;
			}
			if (*(bool*)oapiGetObjectParam (_hObj, OBJPRM_PLANET_CLOUDMICROTEX)) {
				clouddata->cloudmgr->SetMicrotexture ("cloud1.dds");
				clouddata->microalt0 = *(double*)oapiGetObjectParam (_hObj, OBJPRM_PLANET_CLOUDMICROALTMIN);
				clouddata->microalt1 = *(double*)oapiGetObjectParam (_hObj, OBJPRM_PLANET_CLOUDMICROALTMAX);
			}
		} else { // v2 cloud engine
			DWORD maxlvl = (DWORD)*(int*)oapiGetObjectParam (_hObj, OBJPRM_PLANET_CLOUDMAXLEVEL);
			maxlvl = min (maxlvl, *(DWORD*)gc->GetConfigParam (CFGPRM_SURFACEMAXLEVEL));
			cloudmgr2 = new TileManager2<CloudTile> (this, maxlvl, 32);
		}
	} else {
		prm.bCloudShadow = false;
	}

	if (*(bool*)oapiGetObjectParam (_hObj, OBJPRM_PLANET_HASRINGS)) {
		double minrad = *(double*)oapiGetObjectParam (_hObj, OBJPRM_PLANET_RINGMINRAD);
		double maxrad = *(double*)oapiGetObjectParam (_hObj, OBJPRM_PLANET_RINGMAXRAD);
		ringmgr = new RingManager (this, minrad, maxrad);
	} else {
		ringmgr = 0;
	}

	memcpy (&fog, oapiGetObjectParam (_hObj, OBJPRM_PLANET_FOGPARAM), sizeof (FogParam));
	prm.bFogEnabled = (fog.dens_0 > 0);

	patchres = 0;

	nbase = oapiGetBaseCount (_hObj);
	if (nbase)	vbase = new vBase*[nbase];
	else		vbase = NULL;
	for (DWORD i = 0; i < nbase; i++)
		vbase[i] = NULL;

	VESSEL *hVes = oapiGetFocusInterface();

	if (hVes) {
		if (Config->PreLBaseVis && _hObj==hVes->GetSurfaceRef()) {
			LogAlw("PreLoading Base Visuals");
			for (DWORD i=0;i<nbase;i++) {
				OBJHANDLE hBase = oapiGetBaseByIndex (_hObj, i);
				vbase[i] = new vBase (hBase, scn, this);
			}
		}
	}

	mesh = NULL;
	if (surfmgr && surfmgr->GetMaxLevel() == 0) {
		char cbuf[256];
		oapiGetObjectName (hObj, cbuf, 256);
		OBJHANDLE hMesh = oapiLoadMesh (cbuf);
		if (hMesh) {
			LogAlw("Loading mesh [%s] for planetary body '%s'", cbuf, name);
			float fSize = float(size);
			mesh = new D3D9Mesh(hMesh, false, NULL, &fSize);
			oapiDeleteMesh (hMesh);
		}
	}

	// Finish creation -------------------------------------------
	//
	MicroCfg.bEnabled = ParseMicroTextures();

	ParseConfig(oapiGetObjectFileName(hObj));

	UpdateScatter();

	char msg[256]; char path[MAX_PATH];
	// Check texture directory
	GetClient()->PlanetTexturePath(GetName(), path);
	auto x = filesystem::status(path);
	bHasTextures = filesystem::is_directory(x);

	// Check *.tex file
	string tf = string(GetName()) + ".tex";
	GetClient()->PlanetTexturePath(tf.c_str(), path);
	auto y = filesystem::status(path);
	bHasTextures |= filesystem::exists(y);
	
	if (!bHasTextures) {
		VESSEL* vss = oapiGetFocusInterface();
		sprintf_s(msg, sizeof(msg), "[WARNING] Surface textures are missing for %s", GetName());
		if (vss && (vss->GetGravityRef() == hObj)) {
			oapiWriteLog(msg);
			MessageBox(GetClient()->GetWindow(), msg, "Warning", MB_OK);
		}
		else oapiWriteLog(msg);
	}
}


// ===========================================================================================
//
vPlanet::~vPlanet ()
{

	if (nbase) {
		for (DWORD i = 0; i < nbase; i++)
			if (vbase[i]) delete vbase[i];
		delete []vbase;
		vbase = NULL;
	}
	if (surfmgr) delete surfmgr;
	else if (surfmgr2) delete surfmgr2;
	if (cloudmgr2) delete cloudmgr2;

	for (auto x : overlays) for (auto y : x->pSurf) if (y) y->Release();

	if (clouddata) {
		delete clouddata->cloudmgr;
		delete clouddata;
	}
	if (hazemgr)  delete hazemgr;
	if (hazemgr2) delete hazemgr2;
	if (ringmgr)  delete ringmgr;
	if (mesh)     delete mesh;

	SAFE_RELEASE(pSunColor);
	SAFE_RELEASE(pRaySkyView);
	SAFE_RELEASE(pMieSkyView);
	SAFE_RELEASE(pLandViewRay);
	SAFE_RELEASE(pLandViewMie);
	SAFE_RELEASE(pAmbientSky);
	SAFE_RELEASE(pLandViewAtn);
}


// ===========================================================================================
//
void vPlanet::Activate(bool isactive)
{
	vObject::Activate(isactive);
	if (!isactive) {
		if (surfmgr2) surfmgr2->Unload(1);
		if (cloudmgr2) cloudmgr2->Unload(1);
	}
}



// ===========================================================================================
//
bool vPlanet::ParseConfig(const char* fname)
{
	std::string dummy;   
	std::ifstream fs(fname);

	if (fs.fail()) {
		LogErr("Could not open a planet configuration file '%s'", fname);
		return false;
	}

	OBJHANDLE hPlanet = NULL;
	std::string line; // One file line

	while (std::getline(fs, line))
	{
		line = trim(line);

		// skip empty lines and comments
		if (!line.length() || line[0] == ';') continue;

		// AlbedoRGB = <float> <float> <float>
		if (startsWith(line, "AlbedoRGB"))
		{
			std::istringstream iss(line);
			iss >> dummy >> dummy
				>> albedo.x
				>> albedo.y
				>> albedo.z;
		}
	}
	return true;
}

// ==============================================================

bool vPlanet::CameraInAtmosphere() const
{
	double calt = CamDist() - size;
	double halt = GetHorizonAlt();
	if (prm.bAtm==false) return false;
	if (calt>halt) return false;
	return true;
}

// ==============================================================

double vPlanet::GetHorizonAlt() const
{
	if (!prm.bAtm) return 0.0;
	if (!surfmgr2) return prm.atm_hzalt;
	return SPrm.visalt;
}

// ==============================================================

vBase* vPlanet::GetBaseByHandle(OBJHANDLE hBase) const
{
	if (vbase) for (DWORD i=0;i<nbase;i++) if (vbase[i]) if (vbase[i]->Object()==hBase) return vbase[i];
	return NULL;
}

// ==============================================================

VECTOR3 vPlanet::GetUnitSurfacePos(double lng, double lat) const
{
	double slat = sin(lat), clat = cos(lat);
	double slng = sin(lng), clng = cos(lng);
	return _V(clat*clng, slat, clat*slng);
}

// ==============================================================

VECTOR3 vPlanet::ToLocal(VECTOR3 &glob) const
{
	MATRIX3 grot;
	oapiGetRotationMatrix(hObj, &grot);
	return tmul(grot, glob);
}

// ==============================================================

VECTOR3	vPlanet::CameraPos() const
{
	return scn->GetCameraGPos() - gpos;
}

// ==============================================================

void vPlanet::GetLngLat(VECTOR3 &loc, double *lng, double *lat) const
{
	*lat = asin(loc.y);
	*lng = atan2(loc.z, loc.x);
}

// ==============================================================

bool vPlanet::GetMinMaxDistance(float *zmin, float *zmax, float *dmin)
{
	if (mesh==NULL) return false;
	if (bBSRecompute) UpdateBoundingBox();

	D3DXVECTOR3 pos = D3DXVECTOR3(mWorld._41, mWorld._42, mWorld._43);

	float dst = D3DXVec3Length(&pos);

	*dmin = dst - float(size);
	*zmin = *dmin;
	*zmax = dst + float(size);

	return true;
}

// ==============================================================

int vPlanet::GetElevation(double lng, double lat, double *elv, FVECTOR3 *nrm) const
{
	int rv = 0;
	if (!surfmgr2) return -4;
	if (tile_cache) rv = tile_cache->GetElevation(lng, lat, elv, nrm, NULL);	// tile_cache is set to NULL when tiles are deleted
	if (rv!=1) rv = surfmgr2->GetElevation(lng, lat, elv, nrm, &tile_cache);
	return rv;
}

// ==============================================================

SurfTile * vPlanet::FindTile(double lng, double lat, int maxlvl)
{
	return static_cast<SurfTile *>(SurfMgr2()->SearchTile(lng, lat, maxlvl, false));
}

// ==============================================================

void vPlanet::PickSurface(D3DXVECTOR3 &vRay, TILEPICK *result)
{
	if (surfmgr2) {
		surfmgr2->Pick(vRay, result);
		if (result->pTile) {
			VECTOR3 loc = _V(result->_p) - cpos;
			loc = ToLocal(loc);
			double rad;
			oapiLocalToEqu(hObj, loc, &(result->lng), &(result->lat), &rad);
			result->elev = rad - size;
		}
	}
}

// ==============================================================

void vPlanet::UpdateBoundingBox()
{
	bBSRecompute = false;
}

// ==============================================================

void vPlanet::ReOrigin(VECTOR3 global_pos)
{
	vObject::ReOrigin(global_pos);
	if (vbase) for (int i = 0; i < nbase; i++) if (vbase[i]) vbase[i]->ReOrigin(global_pos);
}

// ==============================================================

bool vPlanet::Update (bool bMainScene)
{
	_TRACE;
	if (!active) return false;

	vObject::Update(bMainScene);

	if (patchres==0) return true;

	int i, j;
	float rad_scale = float(size);
	bool rescale = false;
	dist_scale = 1.0f;

	if (IsMesh()) rad_scale = 1.0f; // Mesh vertices are already scaled

	if (cdist > maxdist) {
		rescale = true;
		dist_scale = (FLOAT)(max_centre_dist/cdist);
		prm.DistScale = dist_scale;
	}
	if (rescale) {
		rad_scale *= dist_scale;
		mWorld._41 *= dist_scale;
		mWorld._42 *= dist_scale;
		mWorld._43 *= dist_scale;
	}

	// scale up from template sphere radius 1
	mWorld._11 *= rad_scale; mWorld._12 *= rad_scale; mWorld._13 *= rad_scale;
	mWorld._21 *= rad_scale; mWorld._22 *= rad_scale; mWorld._23 *= rad_scale;
	mWorld._31 *= rad_scale; mWorld._32 *= rad_scale; mWorld._33 *= rad_scale;

	// cloud layer world matrix
	if (prm.bCloud) {
		double cloudrad = size + prm.cloudalt;
		prm.cloudrot = *(double*)oapiGetObjectParam (hObj, OBJPRM_PLANET_CLOUDROTATION);
		prm.cloudrot = posangle(prm.cloudrot);
		prm.cloudvis = (cdist < cloudrad ? 1:0);
		if (cdist > cloudrad*(1.0-1.5e-4)) prm.cloudvis |= 2;

		if (clouddata) {
			if (prm.cloudvis & 1) {
				clouddata->viewap = acos (size/cloudrad);
				if (size < cdist) clouddata->viewap += acos (size/cdist);
			} else {
				clouddata->viewap = 0;
			}

			float cloudscale = (float)(cloudrad/size);

			// world matrix for cloud shadows on the surface
			memcpy (&clouddata->mWorldC0, &mWorld, sizeof (D3DMATRIX));
			if (prm.cloudrot) {
				static D3DXMATRIX crot (1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
				crot._11 =   crot._33 = (float)cos(prm.cloudrot);
				crot._13 = -(crot._31 = (float)sin(prm.cloudrot));
				D3DXMatrixMultiply (&clouddata->mWorldC0, &crot, &clouddata->mWorldC0);
			}

			// world matrix for cloud layer
			memcpy (&clouddata->mWorldC, &clouddata->mWorldC0, sizeof (D3DMATRIX));
			for (i = 0; i < 3; i++)
				for (j = 0; j < 3; j++) {
					clouddata->mWorldC.m[i][j] *= cloudscale;
				}

			// set microtexture intensity
			double alt = cdist-size;
			double lvl = (clouddata->microalt1-alt)/(clouddata->microalt1-clouddata->microalt0);
			clouddata->cloudmgr->SetMicrolevel (max (0.0, min (1.0, lvl)));
		}
	}

	// check all base visuals
	if (nbase) {
		VECTOR3 pos, cpos = scn->GetCameraGPos();
		double scale = (double)scn->ViewH()/scn->GetTanAp();
		for (DWORD i = 0; i < nbase; i++) {
			OBJHANDLE hBase = oapiGetBaseByIndex (hObj, i);
			oapiGetGlobalPos (hBase, &pos);
			double rad = oapiGetSize (hBase);
			double dst = dist (pos, cpos);
			double apprad = rad*scale/dst;

			// -----------------------------------------------------------------
			//
			if (bMainScene) {
				if (vbase[i]) { // base visual exists
					if (apprad < 1.0) { // out of visual range
						delete vbase[i];
						vbase[i] = 0;
					}
				} else {        // base visual doesn't exist
					if (apprad > 2.0) { // within visual range
						vbase[i] = new vBase (hBase, scn, this);
					}
				}
			}

			// Toggle surface base on/off based on visual size -----------------
			//
			if (vbase[i]) {
				if (apprad < 1.0) vbase[i]->Activate(false);
				else if (apprad > 2.0) vbase[i]->Activate(true);
				vbase[i]->Update(bMainScene);
			}
		}
	}
	return true;
}

// ==============================================================

void vPlanet::CheckResolution()
{
	double alt = max (1.0, cdist-size);
	double apr = size * scn->ViewH()*0.5 / (alt * scn->GetTanAp());
	// apparent planet radius in units of screen pixels

	DWORD new_patchres;
	double ntx;

	if (apr < 2.5) { // render planet as 2x2 pixels
		renderpix = true;
		new_patchres = 0;
		ntx = 0;
	} else {
		renderpix = false;
		ntx = PI*2.0 * apr;

		static const double scal2 = 1.0/log(2.0);
		const double shift = (surfmgr2 ? 6.0 : 5.0); // reduce level for tile mgr v2, because of increased patch size
		new_patchres = min (max ((DWORD)(scal2*log(ntx)-shift),(DWORD)1), max_patchres);
	}
	if (new_patchres != patchres) {
		if (hashaze) {
			if (new_patchres < 1) {
				if (hazemgr) { delete hazemgr; hazemgr = NULL; }
				if (hazemgr2) { delete hazemgr2; hazemgr2 = NULL; }
			} else {
				if (tilever>1) {
					if (!hazemgr2) hazemgr2 = new HazeManager2 (this);
				}
				else if (!hazemgr) hazemgr = new HazeManager (scn->GetClient(), this);
			}
		}
		if (ringmgr) {
			int ringres = (new_patchres <= 3 ? 0 : new_patchres <= 4 ? 1:2);
			ringmgr->SetMeshRes (ringres);
		}
		patchres = new_patchres;
	}
}

// ==============================================================

void vPlanet::RenderZRange (double *nplane, double *fplane)
{
	double d = dotp (scn->GetCameraGDir(), cpos);
	*fplane = max (1e3, d+size*1.2);
	*nplane = max (1e0, d-size*1.2);
	*fplane = min (*fplane, *nplane*1e5);
}

// ==============================================================

bool vPlanet::Render(LPDIRECT3DDEVICE9 dev)
{
	_TRACE;
	if (!active) return false;

	const SHADOWMAP *shd = scn->GetSMapData(ShdPackage::Main);

	if (DebugControls::IsActive()) {
		// DWORD flags  = *(DWORD*)gc->GetConfigParam(CFGPRM_GETDEBUGFLAGS);
		DWORD displ  = *(DWORD*)gc->GetConfigParam(CFGPRM_GETDISPLAYMODE);
		vObject *vSel =  DebugControls::GetVisual();
		if (vSel && displ>0) {
			if (vSel->GetObjHandle()) {
				if (oapiGetObjectType(vSel->GetObjHandle())==OBJTP_VESSEL) return false;
			}
		}
	}


	FlowControlPS* fc = GetFlowControl();
	FlowControlVS* fcv = GetFlowControlVS();

	memset(fc, 0, sizeof(FlowControlPS));
	memset(fcv, 0, sizeof(FlowControlVS));


	g_pCurrentVisual = this;

	if (renderpix) { // render as 2x2 pixel block
		RenderDot (dev);
	}
	else // render as sphere ------------------------
	{   

		// Must update the latest view projection matrix
		cp.mVP = *scn->GetProjectionViewMatrix();

		// Setup shadow maps for surface base objects and mesh based bodies ---------------
		//
		D3D9Effect::UpdateEffectCamera(hObj);
		D3D9Effect::FX->SetFloat(D3D9Effect::eDistScale, 1.0f / float(dist_scale));

		HR(D3D9Effect::FX->SetBool(D3D9Effect::eEnvMapEnable, false));
		HR(D3D9Effect::FX->SetBool(D3D9Effect::eShadowToggle, false));

		if (shd->IsValid() && (scn->GetRenderPass() == RENDERPASS_MAINSCENE) && (Config->TerrainShadowing == 2)) {
			if (scn->GetCameraAltitude() < 10e3 || IsMesh()) {
				float s = float(shd->size);
				float is = 1.0f / s;
				float qw = 1.0f / float(Config->ShadowMapSize);
				HR(D3D9Effect::FX->SetMatrix(D3D9Effect::eLVP, shd->mLVP.toCDX()));
				HR(D3D9Effect::FX->SetVector(D3D9Effect::eSHD, ptr(D3DXVECTOR4(s, is, qw, 0))));
				HR(D3D9Effect::FX->SetBool(D3D9Effect::eShadowToggle, true));

				D3D9Mesh::SetShadows(shd);
			}
		}

		DWORD amb = prm.amb0col;
		float fogfactor;

		D3DCOLOR bg		= scn->GetBgColour();
		prm.bFog		= prm.bFogEnabled;
		prm.bTint		= prm.bFogEnabled;
		prm.bAddBkg		= ((bg & 0xFFFFFF) && (hObj != scn->GetCameraProxyBody()));
		prm.FogDensity	= 0.0f;
		prm.SkyColor	= D3DXCOLOR(bg);
		prm.AmbColor	= D3DXCOLOR(0,0,0,0);
		prm.FogColor	= D3DXCOLOR(0,0,0,0);
		prm.TintColor	= D3DXCOLOR(0,0,0,0);
		prm.SunDir		= _D3DXVECTOR3(SunDirection());

		SetupEclipse();

		if (scn->GetRenderPass() == RENDERPASS_MAINSCENE) UpdateScatter();

		if (ringmgr) {
			ringmgr->Render(dev, mWorld, false);
			dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
		}

		if (hazemgr2) {
			double apr = 180.0 * scn->GetCameraAperture() / (scn->GetCameraAspect() * PI);
			hazemgr2->Render(mWorld, float(apr));
		}

		if (prm.bCloud && (prm.cloudvis & 1))
			RenderCloudLayer (dev, D3DCULL_NONE);      // render clouds from below

		if (hazemgr) hazemgr->Render(dev, mWorld);       // horizon ring


		if (prm.bAtm) {
			if (ModLighting (amb))
				prm.AmbColor = D3DXCOLOR(amb);
		}

		if (prm.bFog) { // set up distance fog
			double h = max (1.0, cdist-size);

			VECTOR3 fogcol = fog.col;
			double h_ref = fog.alt_ref;   // 3e3;
			double fog_0 = fog.dens_0;    // 5e-5;
			double fog_ref = fog.dens_ref; // 3e-5;
			double h_max = size*1.5; // At this altitude, fog effect drops to zero

			if (h < h_ref) {
				// linear zone
				fogfactor = (float)(h/h_ref * (fog_ref-fog_0) + fog_0);
			} else {
				// hyperbolic zone: fogfactor = a/(h+b) + c
				// a, b and c are designed such that
				// * fogfactor(h) is continuous at h = h_ref
				// * d fogfactor / dh is continuous at h = h_ref
				// * fogfactor(h_max) = 0
				double b = - (fog_ref*h_max + (fog_ref-fog_0)*(h_max-h_ref)) / (fog_ref + (fog_ref-fog_0)/h_ref * (h_max-h_ref));
				double a = fog_ref*(h_ref+b)*(h_max+b)/(h_max-h_ref);
				double c = -a/(h_max+b);
				fogfactor = (float)(a/(h+b)+c);
			}

			if (fogfactor < 0.0) prm.bFog = false;
			else {
				// day/nighttime fog lighting
				VECTOR3 ppos;
				oapiGetGlobalPos (hObj, &ppos);
				double cosa = dotp (unit(ppos), unit(cpos));
				double bright = 1.0 * max (0.0, min (1.0, cosa + 0.3));
				float rfog = (float)(bright*(min(1.0,fogcol.x)+0.0)); // "whiten" the fog colour
				float gfog = (float)(bright*(min(1.0,fogcol.y)+0.0));
				float bfog = (float)(bright*(min(1.0,fogcol.z)+0.0));
				prm.FogDensity = fogfactor;
				prm.FogColor = D3DXCOLOR(rfog, gfog, bfog, 1.0f);
			}
		}


		if (mesh) {
			mesh->SetSunLight(scn->GetSun());
			mesh->RenderFast(&mWorld, RENDER_ASTEROID);
		} else {
			RenderSphere (dev);
		}

		if (nbase) RenderBaseStructures (dev);

		if (prm.bCloud && (prm.cloudvis & 2))
			RenderCloudLayer (dev, D3DCULL_CCW);	  // render clouds from above

		if (hazemgr) hazemgr->Render (dev, mWorld, true); // haze across planet disc
		if (ringmgr) {
			ringmgr->Render (dev, mWorld, true);
			dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
		}

	}

	// Shutdown shadows to prevent from causing problems
	HR(D3D9Effect::FX->SetBool(D3D9Effect::eShadowToggle, false));

	return true;
}

// ==============================================================

void vPlanet::RenderBeacons(LPDIRECT3DDEVICE9 dev)
{
	// Beacons rendered elsewhere before the cloud layer
}

// ==============================================================

void vPlanet::RenderVectors (LPDIRECT3DDEVICE9 dev, D3D9Pad* pSkp)
{
	// Call base class' method for planets axes
	vObject::RenderVectors(dev, pSkp);

	// If this planet is not a proxy body skip the rest
	if (hObj != oapiCameraProxyGbody()) return;

	for (DWORD i = 0; i < nbase; i++) if (vbase[i]) {
		vbase[i]->RenderVectors(dev, pSkp);
	}
}

// ==============================================================

void vPlanet::ActivateLabels(bool activate)
{
	if (surfmgr2 && *(int*)oapiGetObjectParam(hObj, OBJPRM_PLANET_LABELENGINE) == 2)
	{
		if (activate) surfmgr2->CreateLabels();
		else          surfmgr2->DeleteLabels();
	}
}

// ==============================================================

void vPlanet::RenderLabels(LPDIRECT3DDEVICE9 dev, D3D9Pad *skp, oapi::Font **labelfont, int *fontidx)
{
	if (surfmgr2 && *(int*)oapiGetObjectParam(hObj, OBJPRM_PLANET_LABELENGINE) == 2)
	{
		surfmgr2->RenderLabels(skp, labelfont, fontidx);
	}
}

// ==============================================================

void vPlanet::RenderSphere (LPDIRECT3DDEVICE9 dev)
{
	
	bool bUseZBuf = true;

	double calt = abs(cdist - size);

	if (surfmgr2) {

		tile_cache = NULL;	// Clear tile cache	

		if (scn->GetRenderPass() == RENDERPASS_MAINSCENE) {
			if (size < 500e3) {
				// Comet, Asteroid, Small Moon
				if (cdist >= 1000.0*size) {
					surfmgr2->ElevMode = eElevMode::Spherical;
					bUseZBuf = false;
				}
				else surfmgr2->ElevMode = eElevMode::ForcedElevated;
			}
			else {
				// Planet or Major Moon
				if (cdist >= 1.6*size) {
					surfmgr2->ElevMode = eElevMode::Spherical;
					bUseZBuf = false;
				}
				else {
					TileManager2Base::RenderStats rs = surfmgr2->prevstat;
					if (calt > 35e3) {
						if (abs(cdist - threshold) > 1e3) {
							threshold = cdist;
							// If there are more than 3 spherical tiles render the body as spherical
							if (rs.Sphe > 3) {
								surfmgr2->ElevMode = eElevMode::Spherical; // Spherical body
								bUseZBuf = cdist < (1.6*size);
							}
							else surfmgr2->ElevMode = eElevMode::Elevated; // Elevated body
						}
					}
					else surfmgr2->ElevMode = eElevMode::Elevated; // Elevated body
				}
			}
		}

		//if (string(GetName()) == string("Earth")) sprintf_s(oapiDebugString(), 256, "UseZ=%hhu,  ElevMode=%d", bUseZBuf, surfmgr2->ElevMode);
		
		surfmgr2->Render(dmWorld, bUseZBuf, prm);
	}
	else {
		float fogfactor;
		D3D9Effect::FX->GetFloat(D3D9Effect::eFogDensity, &fogfactor);
		dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
		if (prm.bFog) D3D9Effect::FX->SetFloat(D3D9Effect::eFogDensity, fogfactor/dist_scale);
		surfmgr->SetAmbientColor(prm.AmbColor);
		surfmgr->Render (dev, mWorld, dist_scale, patchres, 0.0, prm.bFog); // surface
		if (prm.bFog) D3D9Effect::FX->SetFloat(D3D9Effect::eFogDensity, fogfactor);
	}

	if (nbase) {
		RenderBaseSurfaces (dev);                     // base surfaces
		RenderBaseShadows (dev, shadowalpha);         // base shadows
	}
	if (prm.bCloudShadow)
		RenderCloudShadows (dev);                		// cloud shadows

	if (bVesselShadow && hObj == oapiCameraProxyGbody())
	// cast shadows only on planet closest to camera
		scn->RenderVesselShadows (hObj, shadowalpha); // vessel shadows
}

// ==============================================================

void vPlanet::RenderCloudLayer (LPDIRECT3DDEVICE9 dev, DWORD cullmode)
{
	if (cullmode != D3DCULL_CCW) dev->SetRenderState (D3DRS_CULLMODE, cullmode);
	if (cloudmgr2) {
		cloudmgr2->Render(dmWorld, false, prm);
	}
	else
		clouddata->cloudmgr->Render (dev, clouddata->mWorldC, dist_scale, min(patchres,8), clouddata->viewap); // clouds
	if (cullmode != D3DCULL_CCW) dev->SetRenderState (D3DRS_CULLMODE, D3DCULL_CCW);
}

// ==============================================================

void vPlanet::RenderCloudShadows (LPDIRECT3DDEVICE9 dev)
{
	if (cloudmgr2) {
		// Nothing to do here
	}
	else if (clouddata) { // legacy method
		float fogfactor;
		D3D9Effect::FX->GetFloat(D3D9Effect::eFogDensity, &fogfactor);
		if (prm.bFog) D3D9Effect::FX->SetFloat(D3D9Effect::eFogDensity, fogfactor/dist_scale);
		clouddata->cloudmgr->RenderShadow(dev, clouddata->mWorldC0, dist_scale, min(patchres,8), clouddata->viewap, clouddata->shadowalpha);
		if (prm.bFog) D3D9Effect::FX->SetFloat(D3D9Effect::eFogDensity, fogfactor);
	}
}

// ==============================================================

void vPlanet::RenderBaseSurfaces(LPDIRECT3DDEVICE9 dev)
{
	// If this planet is not a proxy body skip the rest
	if (hObj != oapiCameraProxyGbody()) return;

	if (scn->GetRenderFlags() & 0x20) {
		for (DWORD i = 0; i < nbase; i++) if (vbase[i]) {
			vbase[i]->RenderSurface(dev);
			vbase[i]->RenderRunwayLights(dev);
		}
	}
}

// ==============================================================

void vPlanet::RenderBaseShadows(LPDIRECT3DDEVICE9 dev, float depth)
{
	// If this planet is not a proxy body skip the rest
	if (hObj != oapiCameraProxyGbody()) return;

	if (scn->GetRenderFlags() & 0x20) {
		if (bObjectShadow) {
			for (DWORD i = 0; i < nbase; i++) if (vbase[i]) vbase[i]->RenderGroundShadow(dev, depth);
			// reset device parameters
			dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
		}
	}
}

// ==============================================================

void vPlanet::RenderBaseStructures (LPDIRECT3DDEVICE9 dev)
{
	// If this planet is not a proxy body skip the rest
	if (hObj != oapiCameraProxyGbody()) return;

	if (scn->GetRenderFlags() & 0x20) {
		for (DWORD i = 0; i < nbase; i++) if (vbase[i]) vbase[i]->RenderStructures(dev);
		for (DWORD i = 0; i < nbase; i++) if (vbase[i]) vbase[i]->RenderBeacons(dev);
	}
}

// ==============================================================

void vPlanet::SetupEclipse()
{
	if (scn->GetRenderPass() != RENDERPASS_MAINSCENE) {
		Eclipse.bEnable = false;
		return;
	}

	// Check Eclipse conditions -------------------------------------------
	//
	OBJHANDLE hPar = oapiGetGbodyParent(hObj);
	OBJHANDLE hSun = oapiGetGbodyByIndex(0);
	VECTOR3 refpoint = _V(0, 0, 0);
	double sunsize = 0.0;
	double plnsize = 0.0;
	double apprad = 0.0;

	vObject* vE = nullptr;

	if (hPar != hSun) {
		// Eclipse by Parent
		vE = scn->GetVisObject(hPar);
		if (IsCastingShadows(vE, this, &sunsize))
		{
			refpoint = vE->GlobalPos() - GlobalPos();
			plnsize = vE->GetSize();
		}
	}
	else {
		// Eclipse by child
		int i = 0;
		while (true)
		{
			OBJHANDLE hC = oapiGetGbodyChild(hObj, i); i++;
			if (!hC) break;
			auto vO = scn->GetVisObject(hC);
			double sz;
			// Find biggest shadow caster
			if (vO && IsCastingShadows(vO, this, &sz)) {
				double pz = vO->GetSize() / Distance(vO, this);
				if (pz > apprad) {
					vE = vO;
					apprad = pz;
					sunsize = sz;
					refpoint = vO->GlobalPos() - GlobalPos();
					plnsize = vO->GetSize();
				}
			}
		}
	}

	if (plnsize > 1.0 && ptEclipse && vE)
	{
		D3DLOCKED_RECT rect;
		if (ptEclipse->LockRect(0, &rect, nullptr, D3DLOCK_DISCARD) == S_OK) {
			for (int i = 0; i < 512; i++) {
				float x = float(i) * float(plnsize + sunsize) / 512.0f;
				float v = OcclusionFactor(x, float(sunsize), float(plnsize));
				((float*)rect.pBits)[i] = saturate(v);
			}
			ptEclipse->UnlockRect(0);
		}
		else LogErr("Failed to Lock 'hEclipse'");

		VECTOR3 toSun = SunDirection();
		Eclipse.bEnable = true;
		Eclipse.vPos = FVECTOR3(refpoint - toSun * dotp(refpoint, toSun)); // Flatten
		Eclipse.fScale = float(1.0 / (plnsize + sunsize));

		float s = sunsize / 1e6f;
		float p = plnsize / 1e6f;

		//D3D9DebugLog("Eclipse of %s by %s Sun(%1.1fGm) %s(%1.1fGm)",
		//	GetName(), vE->GetName(), s, vE->GetName(), p);
	}
	else {
		Eclipse.bEnable = false;
	}

	
}

// ==============================================================

void vPlanet::InitEclipse(ShaderClass* pShr)
{
	ShaderParams* sp = GetTerrainParams();
	FlowControlPS* fc = GetFlowControl();
	fc->bEclipse = Eclipse.bEnable;
	sp->vEclipse = Eclipse.vPos;
	sp->fEclipse = Eclipse.fScale;
	pShr->SetTexture("tEclipse", ptEclipse, IPF_CLAMP | IPF_LINEAR, 0);
}

// ==============================================================

bool vPlanet::ModLighting (DWORD &ambient)
{
	// modify ambient light level inside atmospheres as a function of sun elevation
	if (!prm.bAtm) return false;
	if (cdist >= size+prm.atm_href) return false;

	double alpha = acos (dotp (unit(scn->GetCameraGPos()), -unit(cpos)));
	// angular distance between sun and planet as seen from camera

	double sunelev = alpha - PI05; // elevation of sun above horizon (assuming camera on ground)
	if (sunelev < -14.0*RAD) return false;  // total darkness

	double rscale = (size-cdist)/prm.atm_href + 1.0;    // effect altitude scale (1 on ground, 0 at reference alt)
	double amb = prm.atm_amb0 * min (1.0, (sunelev+14.0*RAD)/(20.0*RAD)); // effect magnitude (dependent on sun elevation)
	if (amb < 0.05) return false;
	amb = max (0.0, amb-0.05);

	DWORD addamb = (DWORD)(amb*rscale*256.0);
	DWORD newamb = *(DWORD*)gc->GetConfigParam (CFGPRM_AMBIENTLEVEL) + addamb;
	ambient = 0;
	for (int i = 0; i < 4; i++)
		ambient |= min ((DWORD)255, newamb) << (i<<3);
	return true;
}

// ==============================================================
// Get a "semi" fixed surface reference point. Update if camera
// movement is greater that 2deg
//
VECTOR3 vPlanet::ReferencePoint()
{
	MATRIX3 mRot;
	oapiGetRotationMatrix(hObj, &mRot);
	VECTOR3 vLPos = unit(tmul(mRot, PosFromCamera()));
	if (dotp(vLPos, vRefPoint)<0.9993) vRefPoint = vLPos;
	return vRefPoint;
}

// ===========================================================================================
// static
void vPlanet::ParseMicroTexturesFile()
{
	std::string filename(OapiExtension::GetConfigDir());
	            filename += "MicroTex.cfg";

	std::string line, // One file line
	            kwd,  // Dummy 'keyword' variable
	            name; // Body name
	int         idx, found = 0;
	_MicroCfg   currCfg = { 0 };

	std::ifstream fs(filename.c_str());
	while (std::getline(fs, line))
	{
		// Empty or comment line?
		if (!line.length() || line.find("//") == 0) {
			continue;
		}

		std::istringstream iss(line);

		// BODY <name> (start of block)?
		if (line.find("BODY") == 0)
		{
			iss >> kwd >> name;
			found = 1; // we don't use 'found |= 1' on purpose here; It's a new block!
		}
		// NORMALS <0|1>?
		else if (line.find("NORMALS") == 0) {
			iss >> kwd >> currCfg.bNormals;
			found |= 2;
		}
		// LEVEL <idx> <texture> <resolution>?
		else if (line.find("LEVEL") == 0) {
			iss >> kwd >> idx;
			if (idx >= 0 && idx <= 2) {
				iss >> currCfg.Level[idx].file >> currCfg.Level[idx].reso;
				found |= (4 << idx);
			} else {
				LogErr("Error in MicroTex.cfg (LEVEL index out of bounds idx:=[0..2])!");
			}
		}

		// Block complete?
		if (found == (1|2|4|8|16)) {
			MicroCfgs[name] = currCfg;
			found = 0;
		}
	}
	fs.close();
}

// ===========================================================================================
//
bool vPlanet::ParseMicroTextures()
{
	if (Config->MicroMode==0) return false;	// Micro textures are disabled
	if (surfmgr2==NULL) return false; // Only supported with tile format 2

	// Find 'our' config
	MicroCfgsIterator it = MicroCfgs.find(GetName());
	if (it != MicroCfgs.end()) {
		MicroCfg = it->second;
		int cnt = 0; // Check that all three textures exists
		for (auto& x : MicroCfg.Level) if (x.pTex) cnt++;
		return (cnt == 3);
	}
	return false;
}


// ===========================================================================================
//
vPlanet::sOverlay * vPlanet::IntersectOverlay(VECTOR4 q, FVECTOR4 *texcoord) const
{
	for (auto olay : overlays)
	{
		double ow = fabs(olay->lnglat.x - olay->lnglat.z);
		double oh = fabs(olay->lnglat.y - olay->lnglat.w);
		double ew = ow * 0.001;
		double eh = oh * 0.001;

		if (q.x > (olay->lnglat.z - ew)) continue;
		if (q.z < (olay->lnglat.x + ew)) continue;
		if (q.y < (olay->lnglat.w + eh)) continue;
		if (q.w > (olay->lnglat.y - eh)) continue;


		double tw = fabs(q.x - q.z);
		double th = fabs(q.y - q.w);

		if (ow > PI) ow = PI2 - ow;
		if (tw > PI) tw = PI2 - tw;

		(*texcoord).x = float((q.x - olay->lnglat.x) / ow);
		(*texcoord).y = float((olay->lnglat.y - q.y) / oh);
		(*texcoord).z = float(tw / ow);
		(*texcoord).w = float(th / oh);

		return olay;
	}
	return NULL;
}


// ===========================================================================================
//
vPlanet::sOverlay * vPlanet::AddOverlaySurface(VECTOR4 lnglat, gcCore::OlayType type, LPDIRECT3DTEXTURE9 pSrf, vPlanet::sOverlay *pOld, const FVECTOR4 *pB)
{
	if (type == gcCore::OlayType::RELEASE_ALL) {
		overlays.remove(pOld);
		return NULL;
	}

	if (pOld) {
		pOld->pSurf[int(type)] = pSrf;
		pOld->lnglat = lnglat;
		if (pB) pOld->Blend[int(type)] = D3DXVECTOR4(pB->r, pB->g, pB->b, pB->a);
		else pOld->Blend[int(type)] = D3DXVECTOR4(1, 1, 1, 1);
		return pOld;
	}

	sOverlay *oLay = new sOverlay();
	memset(&oLay->pSurf, 0, sizeof(oLay->pSurf));
	oLay->pSurf[int(type)] = pSrf;
	oLay->lnglat = lnglat;
	if (pB) oLay->Blend[int(type)] = D3DXVECTOR4(pB->r, pB->g, pB->b, pB->a);
	else oLay->Blend[int(type)] = D3DXVECTOR4(1, 1, 1, 1);
	overlays.push_back(oLay);
	return oLay;
}


// ===========================================================================================
//
void vPlanet::SetMicroTexture(LPDIRECT3DTEXTURE9 pSrc, int slot)
{
	LPDIRECT3DTEXTURE9 pTex = NULL;
	D3DSURFACE_DESC desc;
	pSrc->GetLevelDesc(0, &desc);
	DWORD MipLevels = pSrc->GetLevelCount();
	HR(D3DXCreateTexture(GetDevice(), desc.Width, desc.Height, MipLevels, 0, desc.Format, D3DPOOL_DEFAULT, &pTex));
	HR(GetDevice()->UpdateTexture(pSrc, pTex));
	SAFE_RELEASE(MicroCfg.Level[slot].pTex);
	MicroCfg.Level[slot].pTex = pTex;
}

// ===========================================================================================
// static
void vPlanet::LoadMicroTextures(LPDIRECT3DDEVICE9 pDev)
{
	if (Config->MicroMode == 0) return;

	for (auto &body : MicroCfgs)
	{
		for (auto &x : body.second.Level)
		{
			char file_path[MAX_PATH];
			sprintf_s(file_path, MAX_PATH, "Textures/%s", x.file);
			
			// If texture is not loaded, load it
			if (MicroTextures.find(x.file) == MicroTextures.end()) {
				if (D3DXCreateTextureFromFileA(pDev, file_path, &x.pTex) == S_OK) {
					LogAlw("Microtexture [%s] loaded", x.file);
					MicroTextures[x.file] = x.pTex;
				}
				else {
					LogErr("Failed to read microtexture [%s] for [%s]", file_path, body.first.c_str());
					MicroTextures[x.file] = NULL;
				}
			}

			x.pTex = MicroTextures[x.file];
			
			if (x.pTex) {
				D3DSURFACE_DESC desc;
				x.pTex->GetLevelDesc(0, &desc);
				x.px = double(desc.Width);
				x.size = double(desc.Width) / x.reso;
			}
		}
	}
}

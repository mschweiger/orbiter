// ==============================================================
// D3DClient.h
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2006-2016 Martin Schweiger
//				 2012-2016 Jarmo Nikkanen
// ==============================================================

#ifndef __D3D9CLIENT_H
#define __D3D9CLIENT_H

// must be defined before windows includes to fix warnins on VS 2003+
#if defined(_MSC_VER) && (_MSC_VER >= 1300 ) // Microsoft Visual Studio Version 2003 and higher
//#define _CRT_SECURE_NO_DEPRECATE
//#define _CRT_NONSTDC_NO_WARNINGS
#include <fstream>
#else  // older MSVC++ versions
#include <fstream.h>
#endif

#include <d3d9.h>
#include <d3dx9.h>
#include "D3D9Catalog.h"
#include "GraphicsAPI.h"
#include "D3D9Util.h"
#include <stdio.h>
#include <assert.h>
#include "OrbiterAPI.h"
#include "D3D9Frame.h"
//#include "gcCore.h"
#include <vector>
#include <stack>
#include <list>
#include "WindowMgr.h"

#define SHM_CASCADE_COUNT	3
#define SHM_LOD_COUNT		6


#define PP_DEFAULT			0x1
#define PP_LENSFLARE		0x2

#define MAX_SCENE_LIGHTS	(DWORD)24
#define MAX_MESH_LIGHTS		8	// Must match the setting in D3D9Client.fx

#ifdef _NVAPI_H
extern StereoHandle	pStereoHandle;
#endif

class D3D9Config;
class MeshManager;
class TextureManager;
class Scene;
class VideoTab;
class SurfNative;
class CD3DFramework9;
class D3D9Mesh;
class D3D9Annotation;
class D3D9Text;
class CSphereManager;
class FileParser;
class OapiExtension;
class D3D9Pad;

typedef char * LPCHAR;
typedef void * CAMERAHANDLE;
typedef class D3D9Mesh * HMESH;
typedef class SurfNative* lpSurfNative;


enum class EnvCamType { Undefined, Exterior, Interior, Mesh };
enum class ShdPackage { None, Main, VC, Stage };


/**
 * \brief Storage structure to keep reflection camera information.
 */
struct ENVCAMREC
{
	~ENVCAMREC()
	{
		SAFE_RELEASE(pCube);
		SAFE_RELEASE(pIrrad);
		SAFE_RELEASE(pPlane);
		oapiWriteLog("ENVCAMREC::Destruct");
	}

	std::vector<WORD> omitAttc = {};
	std::vector<WORD> omitDock = {};
	DWORD			flags = 0;			///< Camera flags
	EnvCamType		type = EnvCamType::Undefined;
	FVECTOR3		lPos = { 0,0,0 };	///< Camera local position
	FVECTOR3		lDir = { 1,0,0 };	///< Camera local direction (in 'PLANE' mode only)
	float			near_clip = 0.1f;	///< Near clip-plane distance
	float			da_curve = 0.4f;
	float			da_bounch = 0.35f;
	float			da_force = 0.2f;
	int				mesh_idx = -1;		///< Camera is attached to a mesh 
	int				group_idx = -1;		///< Camera is attached to a group
	int				id = -1;			///< User Id, for binding
	BYTE			iSide = 0;			///< [Private] Current side being rendered
	bool			bRendered = false;	///< [Private] Rendering of camera view is completed
	LPDIRECT3DCUBETEXTURE9 pCube = nullptr;	///< Reflection cube map
	LPDIRECT3DTEXTURE9 pIrrad = nullptr;	///< Irradiance map (baraboloidal)
	LPDIRECT3DTEXTURE9 pPlane = nullptr;	///< Reflection 2D map if (flags & ENVCAM_PLANE)
};

/**
 * \brief Statistical data storage
 */
struct _D3D9Stats {

	_D3D9Stats()
	{
		memset(&Mesh, 0, sizeof(Mesh));
		memset(&Timer, 0, sizeof(Timer));
		TilesAllocated = 0;
	}
	
	struct {
		DWORD Vertices;		///< Number of vertices rendered
		DWORD MeshGrps;		///< Number of mesh groups rendered
		DWORD Meshes;		///< Number of meshes rendered
		DWORD TexChanges;	///< Number of texture changes
		DWORD MtrlChanges;	///< Number of material changes
	} Mesh;					///< Mesh related statistics

	struct {
		D3D9Time Update;		///< clbkUpdate
		D3D9Time Scene;			///< clbkRenderScene
		D3D9Time Display;		///< clbkDisplayFrame
		D3D9Time FrameTotal;	///< Total frame time
		//------------------------------------------------------------
		D3D9Time HUDOverlay;	///< Total time spend in HUD, 2D Panel overlay
		D3D9Time CamVis;		///< Object/camera updates
		D3D9Time Surface;		///< Surface
		D3D9Time Clouds;		///< Clouds
		//-------------------------------------------------------------
		D3D9Time LockWait;		///< Time waiting GetDC or vertex buffer lock
		D3D9Time BlitTime;		///<
		D3D9Time GetDC;			///<
	} Timer;					///< Render timing related statistics

	DWORD TilesAllocated;	///< Number of allocated tiles
	std::map<DWORD, DWORD> TilesRendered;	///< Number of rendered tiles
};



struct RenderTgtData {
	LPDIRECT3DSURFACE9 pColor;
	LPDIRECT3DSURFACE9 pDepthStencil;
	class D3D9Pad *pSkp;
	int code;
};

struct PickProp {
	D3D9Mesh* pMesh;	// Mesh to pick, or NULL for full scene
	float fnear;		// Near clip distance, ignore entities closer than this
	bool bDualSided;	// Pick also back-facing triangles
};

struct SMapInput {
	FVECTOR3 pos = { };		///< Shadow map center in camera centric coords (ecl frame)
	FVECTOR3 ld = { };		///< Direction of sunlight (ecl frame)
	float rad = { };			///< Radius of shadow mapped area
};

class SHADOWMAP : public SMapInput
{
public:

	enum class sMapType { MultiLod, Cascaded, SingleLod };

	SHADOWMAP(LPDIRECT3DDEVICE9 pDevice, sMapType tp, DWORD sz = 1024);
	~SHADOWMAP();

	LPDIRECT3DTEXTURE9 Map(int idx) const
	{
		if (tp == sMapType::MultiLod) {
			if (idx == 0) return ptShmRT[lod];
			else return nullptr;
		}
		return ptShmRT[idx];
	}

	void Clear() { bValid = false; }
	bool IsValid() const { return bValid && (Map(0) != nullptr); }

	

	FMATRIX4	mVP[SHM_CASCADE_COUNT] = {};
	FVECTOR4	Subrect[SHM_CASCADE_COUNT] = {};
	FVECTOR4	SubrectTF[SHM_CASCADE_COUNT] = {};
	FVECTOR2	Center[SHM_CASCADE_COUNT] = {};
	float		SubPx[SHM_CASCADE_COUNT] = {};
	FMATRIX4	mLVP = {};
	float		dist = 0.0f;	// Shadow camera distance from shadow origin
	float		depth = 0.0f;	// near to far plane distance. i.r. depth of the field
	int			lod = 0;		// level of detail, 0 = highest
	int			size = 0;		// Map size in pixels
	int			cascades = 0;	// Number of active cascades
	bool		bValid = false;
	sMapType	tp;
	

	// Cascades are located in entries 0, 1, 2
	// Active entry in MultiLod map is located in index pointed by 'lod'
	// Active entry in SingleLod map is in index 0, 'lod' entry is 0
	LPDIRECT3DSURFACE9 psShmRT[max(SHM_LOD_COUNT, SHM_CASCADE_COUNT)] = { nullptr };
	LPDIRECT3DTEXTURE9 ptShmRT[max(SHM_LOD_COUNT, SHM_CASCADE_COUNT)] = { nullptr };
};

struct LVLH {
	FVECTOR3 Up;
	FVECTOR3 North;
	FVECTOR3 East;
};


extern _D3D9Stats D3D9Stats;
extern bool bFreeze;
extern bool bFreezeEnable;
extern bool bFreezeRenderAll;
extern DWORD g_uCurrentMesh;
extern class vObject* g_pCurrentVisual;
extern set<D3D9Mesh*> MeshCatalog;
extern set<SurfNative*>	SurfaceCatalog;
extern IDirect3D9* g_pD3DObject;
extern Memgr<float>* g_pMemgr_f;
extern Memgr<INT16>* g_pMemgr_i;
extern Memgr<UINT8>* g_pMemgr_u;
extern Memgr<WORD>* g_pMemgr_w;
extern Memgr<VERTEX_2TEX>* g_pMemgr_vtx;
extern Texmgr<LPDIRECT3DTEXTURE9>* g_pTexmgr_tt;
extern Vtxmgr<LPDIRECT3DVERTEXBUFFER9>* g_pVtxmgr_vb;
extern Idxmgr<LPDIRECT3DINDEXBUFFER9>* g_pIdxmgr_ib;


namespace oapi {



// ==============================================================
// D3D9Client class interface
// The DX9 render client for Orbiter
// ==============================================================

class D3D9Client : public GraphicsClient 
{

	friend class ::Scene;	// <= likes to call Render2DOverlay()

public:

	/**
	 * \brief Create a graphics object.
	 *
	 * The graphics object is typically created during module initialisation
	 * (see \ref InitModule). Once the client is created, it must be registered
	 * with the Orbiter core via the oapiRegisterGraphicsClient function.
	 * \param hInstance module instance handle (as passed to InitModule)
	 */
	explicit D3D9Client (HINSTANCE hInstance);

	/**
	 * \brief Destroy the graphics object.
	 *
	 * Usually, the graphics object is destroyed when the module is unloaded
	 * (see opcDLLExit), after is has been detached from the Orbiter core
	 * via a call to oapiUnregisterGraphicsClient.
	 */
	~D3D9Client ();


	HBITMAP gcReadImageFromFile(const char *path);

	/**
	 * \brief Perform any one-time setup tasks.
	 *
	 * This includes enumerating drivers, graphics modes, etc.
	 * Derived classes should also call the base class method to allow
	 * default setup.
	 * \default Initialises the VideoData structure from the Orbiter.cfg
	 *   file
	 * \par Calling sequence:
	 *   Called during processing of oapiRegisterGraphicsClient, after the
	 *   Launchpad Video tab has been inserted (if clbkUseLaunchpadVideoTab
	 *   returns true).
	 */
	bool clbkInitialise ();

	/**
	 * \brief Request for video configuration data
	 *
	 * Called by Orbiter before the render window is opened or configuration
	 * parameters are written to file. Applications should here either update
	 * the provided VIDEODATA structure from any user selections made in the
	 * Launchpad Video tab and leave it to Orbiter to write these parameters
	 * to Orbiter.cfg, or write the current video settings to their own
	 * configuration file.
	 * \default None.
	 */
	void clbkRefreshVideoData ();

	/**
     * \brief Called when a config setting is changed by the user during
     *    a simulation setting, to give the client opportunity to respond to the change.
     * \param cat option category, see \ref optcat
     * \param item option item, see \ref optitem
     */
	void clbkOptionChanged(DWORD cat, DWORD item);

	void clbkDebugString(const char* str);

	/**
	 * \brief Texture request
	 *
	 * Load a texture from a file into a device-specific texture object, and
	 * return a generic SURFHANDLE for it. Derived classes should overload this
	 * method to add texture support.
	 * Usually, the client should read Orbiter's default texture files (in
	 * DXT? format). However, the client also has the option to load its own
	 * texture files stored in a different format, and pass them back via the
	 * SUFHANDLE interface.
	 * \param fname texture file name with path relative to orbiter
	 *   texture folders; can be used as input for OpenTextureFile.
	 * \param flags request for texture properties
	 * \return Texture handle, cast into generic SURFHANDLE, or NULL if texture
	 *   could not be loaded.
	 * \default Return NULL.
	 * \note If the client loads its own of texture files, they can either be
	 *   installed in the default locations, replacing Orbiter's set of
	 *   textures, or stored alongside the original textures, using different
	 *   names or directory locations. In the latter case, the fname parameter
	 *   passed to clbkLoadTexture must be adapted accordingly (for example,
	 *   by replacing the dds extension with jpg, or by adding an 'OGL/'
	 *   prefix to the path name, etc). Not overwriting the original texture
	 *   set has the advantage that other graphics clients relying on the
	 *   original textures can still be used.
	 * \note The following flags are supported:
	 *   - bit 0 set: force creation in system memory
	 *   - bit 1 set: decompress, even if format is supported by device
	 *   - bit 2 set: don't load mipmaps, even if supported by device
	 *   - bit 3 set: load as global resource (can be managed by graphics client)
	 * \note If bit 3 of flags is set, orbiter will not try to modify or release
	 *   the texture. The client should manage the texture (i.e. keep it in a
	 *   repository and release it at destruction). Any further call of
	 *   clbkLoadTexture should first scan the repository. If the texture is
	 *   already present, the function should just return a pointer to it.
	 */
	SURFHANDLE clbkLoadTexture (const char *fname, DWORD flags = 0);

	/**
	 * \brief Load a surface from file into a surface object, and return a SURFHANDLE for it.
	 * \param fname texture file name with path relative to orbiter texture folders
	 * \param attrib \ref surfacecaps (see notes)
	 * \return A SURFHANDLE for the loaded surface, for example a pointer to the surface object.
	 * \note If the request refers to a static surface that has already be loaded, or if the
	 *   client buffers the unmodified surfaces after loading, it can simply return a handle to
	 *   the existing surface object, instead of reading it again from file.
	 * \note The attrib bitflag can contain one of the following main attributes:
	 *  - OAPISURFACE_RO: Load the surface to be readable by the GPU pipeline
	 *  - OAPISURFACE_RW: Load the surface to be readable and writable by the GPU pipeline
	 *  - OAPISURFACE_GDI: Load the surface to be readable and writable by the CPU, and can be blitted into an uncompressed RO or RW surface without alpha channel
	 *  - OAPISURFACE_STATIC: Load the surface to be readable by the GPU pipeline
     *  In addition, the flag can contain any of the following auxiliary attributes:
	 *  - OAPISURFACE_MIPMAPS: Load the mipmaps for the surface from file, or create them if necessary
	 *  - OAPISURFACE_NOMIPMAPS: Don't load mipmaps, even if they are available in the file
	 *  - OAPISURFACE_NOALPHA: Load the surface without an alpha channel
	 *  - OAPISURFACE_UNCOMPRESS: Uncompress the surface on loading.
	 * \sa oapiCreateSurface(DWORD,DWORD,DWORD)
	 */
	SURFHANDLE clbkLoadSurface (const char *fname, DWORD attrib, bool bPath = false);
	SURFHANDLE clbkLoadMaps(const char* diff, const char* maps, bool bPath, SURFHANDLE hOld = NULL, bool bAll = true);
	


	/**
	 * \brief Save the contents of a surface to a formatted image file or to the clipboard
	 * \param surf surface handle (0 for primary render surface)
	 * \param fname image file path relative to orbiter root directory (excluding file extension), or NULL to save to clipboard
	 * \param fmt output file format
	 * \param quality quality request if the format supports it (0-1)
	 * \return Should return true on success
	 * \default Nothing, returns false
	 * \note Implementations can make use of the \ref WriteImageDataToFile method to write to
	 *   a file in the desired format once a pointer to the image data in 24-bit uncompressed
	 *   format has been obtained.
	 */
	bool clbkSaveSurfaceToImage (SURFHANDLE surf, const char *fname, ImageFileFormat fmt, float quality=0.7f);

	/**
	 * \brief Write surface to file (sub-function of \ref clbkSaveSurfaceToImage)
	 *
	 * \param surface size and format description
	 * \param fname image file path relative to orbiter root directory (excluding file extension)
	 * \param fmt output file format
	 * \param quality quality request if the format supports it (0-1)
	 * \return Should return true on success
	 */
	bool SaveSurfaceToFile (const D3DSURFACE_DESC* desc, D3DLOCKED_RECT& pRect,
	                        const char* fname, ImageFileFormat fmt, float quality);

	/**
	 * \brief Store device-dependent bitmap to clipboard (sub-function of \ref clbkSaveSurfaceToImage)
	 *
	 * \param surface size and format description
	 * \return Should return true on success
	 */
	bool SaveSurfaceToClipboard (const D3DSURFACE_DESC * desc);

	/**
	 * \brief Texture release request
	 *
	 * Called by Orbiter when a previously loaded texture can be released
	 * from memory. The client can use the appropriate device-specific method
	 * to release the texture.
	 * \param hTex texture handle
	 * \default None.
	 */
	void clbkReleaseTexture (SURFHANDLE hTex);

	/**
	 * \brief Replace a texture in a device-specific mesh.
	 * \param hMesh device mesh handle
	 * \param texidx texture index (>= 0)
	 * \param tex texture handle
	 * \return Should return \e true if operation successful, \e false otherwise.
	 * \default None, returns \e false.
	 */
	bool clbkSetMeshTexture (DEVMESHHANDLE hMesh, DWORD texidx, SURFHANDLE tex);

	/**
	 * \brief Replace properties of an existing mesh material.
	 * \param hMesh device mesh handle
	 * \param matidx material index (>= 0)
	 * \param mat pointer to material structure
	 * \return Overloaded functions should return an integer error flag, with
	 *   the following codes: 0="success", 3="invalid mesh handle", 4="material index out of range"
	 * \default, None, returns 2 ("client does not support operation").
	 */
	int clbkSetMeshMaterial(DEVMESHHANDLE hMesh, DWORD matidx, const MATERIAL* mat);
	int clbkSetMeshMaterialEx(DEVMESHHANDLE hMesh, DWORD matidx, MatProp mat, const oapi::FVECTOR4* in);

	/**
	* \brief Retrieve the properties of one of the mesh materials.
	* \param hMesh device mesh handle
	* \param matidx material index (>= 0)
	* \param mat [out] pointer to MATERIAL structure to be filled by the method.
	* \return true if successful, false on error (index out of range)
	* \default None, returns 2 ("client does not support operation").
	*/
	int clbkMeshMaterial(DEVMESHHANDLE hMesh, DWORD matidx, MATERIAL* mat);
	int clbkMeshMaterialEx(DEVMESHHANDLE hMesh, DWORD matidx, MatProp mat, oapi::FVECTOR4* out);

	/**
     * \brief Set custom properties for a device-specific mesh.
	 * \param hMesh device mesh handle
	 * \param property property tag
	 * \param value new mesh property value
	 * \return The method should return \e true if the property tag was recognised
	 *   and the request could be executed, \e false otherwise.
	 * \note Currently only a single mesh property request type will be sent, but this may be
	 *  extended in future versions:
	 * - \c MESHPROPERTY_MODULATEMATALPHA \n \n
	 * if value==0 (default) disable material alpha information in textured mesh groups (only use texture alpha channel).\n
	 * if value<>0 modulate (mix) material alpha values with texture alpha maps.
	 * \default None, returns \e false.
	 */
	bool clbkSetMeshProperty (DEVMESHHANDLE hMesh, DWORD property, DWORD value);
	void clbkSetVisualProperty(VISHANDLE vis, VisualProp prp, int idx, const type_info& t, const void* val);

	/**
	 * \brief React to vessel docking, attaching events
	 * \param hVesselA object handle of first vessel
	 * \param hVesselB object handle of second vessel
	 * \param type Event type
	 */
	void clbkScenarioChanged(OBJHANDLE hVessel, ScnChgEvent type);

	/**
	 * \brief React to vessel creation
	 * \param hVessel object handle of new vessel
	 * \note Calls Scene::NewVessel() to check for visual
	 */
	void clbkNewVessel (OBJHANDLE hVessel);

	/**
	 * \brief React to vessel destruction
	 * \param hVessel object handle of vessel to be destroyed
	 * \note Calls Scene::DeleteVessel() to remove the visual
	 */
	void clbkDeleteVessel (OBJHANDLE hVessel);


	/**
	 * \brief Message callback for a visual object.
	 * \param hObj handle of the object that created the message
	 * \param vis client-supplied identifier for the visual
	 * \param msg event identifier
	 * \param context message context
	 * \return Function should return 1 if it processes the message, 0 otherwise.
	 * \default None, returns 0.
	 * \note Messages are generated by Orbiter for objects that have been
	 *   registered with \ref RegisterVisObject by the client, until they are
	 *   un-registered with \ref UnregisterVisObject.
	 * \note Currently only vessel objects create visual messages.
	 * \note For currently supported event types, see \ref visevent.
	 * \note The \e vis pointer passed to this function is the same as that provided
	 *   by RegisterVisObject. It can be used by the client to identify the visual
	 *   object for which the message was created.
	 * \sa RegisterVisObject, UnregisterVisObject, visevent
	 */
	int clbkVisEvent (OBJHANDLE hObj, VISHANDLE vis, DWORD msg, DWORD_PTR context);

	/**
	 * \brief Return a DEVMESHHANDLE handle for a visual, defined by its index. (! Return type incorrect !)
	 * \param vis visual identifier
	 * \param idx mesh index (>= 0)
	 * \return Mesh handle (client-specific)
	 * \note Derived clients should return a handle that identifies a
	 *   mesh for the visual (in client-specific format).
	 * \note Orbiter calls this method in response to a \ref VESSEL::GetMesh
	 *   call by an vessel module.
	 */
	virtual MESHHANDLE clbkGetMesh (VISHANDLE vis, UINT idx);

	/**
	 * \brief Mesh group data retrieval interface for device-specific meshes.
	 * \param hMesh device mesh handle
	 * \param grpidx mesh group index (>= 0)
	 * \param grs data buffers and buffer size information. See \ref oapiGetMeshGroup
	 *    for details.
	 * \return Should return 0 on success, or error flags > 0.
	 * \default None, returns -2.
	 */
	int clbkGetMeshGroup (DEVMESHHANDLE hMesh, DWORD grpidx, GROUPREQUESTSPEC *grs);

	/**
	 * \brief Mesh group editing interface for device-specific meshes.
	 * \param hMesh device mesh handle
	 * \param grpidx mesh group index (>= 0)
	 * \param ges mesh group modification specs
	 * \return Should return 0 on success, or error flags > 0.
	 * \default None, returns -2.
	 * \note Clients should implement this method to allow the modification
	 *   of individual groups in a device-specific mesh. Modifications may
	 *   include vertex values, index lists, texture and material indices,
	 *   and user flags.
	 */
	int clbkEditMeshGroup (DEVMESHHANDLE hMesh, DWORD grpidx, GROUPEDITSPEC *ges);


	// ==================================================================
	/// \name Dialog interface
	//@{
	/**
	 * \brief Popup window open notification.
	 * \note This method is called just before a popup window (e.g. dialog
	 *   box) is opened. It allows the client to prepare for subsequent
	 *   rendering of the window, if necessary.
	 */
	void clbkPreOpenPopup ();

	//@}

	// ==================================================================
	/// \name Particle stream methods
	// @{

	/**
	 * \brief Create a generic particle stream.
	 * \param pss particle stream parameters
	 * \return Pointer to new particle stream.
	 * \default None, returns NULL. Derived classes should overload this method
	 *   to return a ParticleStream-derived class instance in order to support
	 *   particle streams.
	 * \sa ParticleStream
	 */
	ParticleStream *clbkCreateParticleStream (PARTICLESTREAMSPEC *pss);

	/**
	 * \brief Create a particle stream associated with a vessel.
	 *
	 * Typically used for exhaust and plasma effects, but can also be used
	 * for other types of particles.
	 * \param pss particle stream parameters
	 * \param hVessel vessel handle
	 * \param lvl pointer to exhaust level control variable
	 * \param ref pointer to stream source position (vessel frame) [<b>m</b>]
	 * \param dir pointer to stream direction (vessel frame)
	 * \return Pointer to new particle stream
	 * \default None, returns NULL. Derived classes should overload this method
	 *   to return a ParticleStream-derived class instance in order to support
	 *   exhaust streams.
	 * \note The lvl, ref and dir parameters may be modified by orbiter after
	 *   the stream has been created, e.g. to reflect changes in engine thrust
	 *   level or gimballing.
	 */
	ParticleStream *clbkCreateExhaustStream (PARTICLESTREAMSPEC *pss, OBJHANDLE hVessel, const double *lvl, const VECTOR3 *ref, const VECTOR3 *dir);

	/**
	 * \brief Create a particle stream associated with a vessel.
	 *
	 * Typically used for exhaust and plasma effects, but can also be used
	 * for other types of particles.
	 * \param pss particle stream parameters
	 * \param hVessel vessel handle
	 * \param lvl pointer to exhaust level control variable
	 * \param ref pointer to stream source position (vessel frame) [<b>m</b>]
	 * \param dir pointer to stream direction (vessel frame)
	 * \return Pointer to new particle stream
	 * \default None, returns NULL. Derived classes should overload this method
	 *   to return a ParticleStream-derived class instance in order to support
	 *   exhaust streams.
	 * \note The lvl parameter may be modified by orbiter after
	 *   the stream has been created, e.g. to reflect changes in engine thrust
	 *   level.
	 * \note The ref and dir parameters are fixed in this version of the method.
	 */
	ParticleStream *clbkCreateExhaustStream (PARTICLESTREAMSPEC *pss, OBJHANDLE hVessel, const double *lvl, const VECTOR3 &ref, const VECTOR3 &dir);

	/**
	 * \brief Create a vessel particle stream for reentry heating effect
	 * \param pss particle stream parameters
	 * \param hVessel vessel handle
	 * \return Pointer to new particle stream
	 * \default None, returns NULL. Derived classes should overload this method
	 *   to return a ParticleStream-derived class instance in order to support
	 *   reentry streams.
	 */
	ParticleStream *clbkCreateReentryStream (PARTICLESTREAMSPEC *pss, OBJHANDLE hVessel);
	// @}

	/**
	 * \brief Create an annotation object for displaying on-screen text.
	 * \return Pointer to new screen annotation object.
	 * \default Dynamically allocates a 'ScreenAnnotation' instance and returns
	 *   a pointer to it.
	 */
	ScreenAnnotation* clbkCreateAnnotation();

	/**
	 * \brief Render window message handler
	 *
	 * Derived classes should also call the base class method to allow
	 * default message processing.
	 * \param hWnd render window handle
	 * \param uMsg Windows message identifier
	 * \param wParam WPARAM message parameter
	 * \param lParam LPARAM message parameter
	 * \return The return value depends on the message being processed.
	 * \note This is the standard Windows message handler for the render
	 *   window.
	 * \note This method currently intercepts only the WM_CLOSE and WM_DESTROY
	 *   messages, and passes everything else to the Orbiter core message
	 *   handler.
	 */
	LRESULT RenderWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	/**
	 * \brief Message handler for 'video' tab in Orbiter Launchpad dialog
	 *
	 * Overload this method to display and retrieve video parameters using
	 * the Launchpad video tab. This method acts like a standard Windows dialog
	 * message handler.
	 * \param hWnd window handle for video tab
	 * \param uMsg Windows message
	 * \param wParam WPARAM message value
	 * \param lParam LPARAM message value
	 * \return The return value depends on the message type and the action taken.
	 * \default Do nothing, return FALSE.
	 */
	INT_PTR LaunchpadVideoWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	/**
	 * \brief Fullscreen mode flag
	 * \return true if the client is set up for running in fullscreen
	 *   mode, false for windowed mode.
	 */
	bool clbkFullscreenMode () const;
	/**
	 * \brief Returns the dimensions of the render viewport
	 * \param width render viewport width [pixel]
	 * \param height render viewport height [pixel]
	 * \note This function is called by orbiter after the render window or
	 *   fullscreen renderer has been created (see \ref clbkCreateRenderWindow).
	 * \note This should normally return the screen resolution in fullscreen
	 *   mode, and the size of the render window client area in windowed mode,
	 *   clients can also return smaller values if they only use part of the
	 *   screen area for scene rendering.
	 */
	void clbkGetViewportSize (DWORD *width, DWORD *height) const;

	/**
	 * \brief Returns a specific render parameter
	 * \param[in] prm parameter identifier (see \sa renderprm)
	 * \param[out] value value of the queried parameter
	 * \return true if the specified parameter is supported by the client,
	 *    false if not.
	 */
	bool clbkGetRenderParam (DWORD prm, DWORD *value) const;

	/**
	 * \brief Render an instrument panel in cockpit view as a 2D billboard.
	 * \param hSurf array of texture handles for the panel surface
	 * \param hMesh billboard mesh handle
	 * \param T transformation matrix for panel mesh vertices (2D)
	 * \param additive If true, panel should be rendered additive (transparent)
	 * \default None.
	 * \note The texture index of each group in the mesh is interpreted as index into the
	 *   hSurf array. Special indices are TEXIDX_MFD0 and above, which specify the
	 *   surfaces representing the MFD displays. These are obtained separately and
	 *   don't need to be present in the hSurf list.
	 * \note The \e additive flag is used when rendering the default "glass
	 *   cockpit" if the user requested. "transparent MFDs". The renderer can
	 *   then use e.g. additive blending for rendering the panel.
	 */
	void clbkRender2DPanel (SURFHANDLE *hSurf, MESHHANDLE hMesh, MATRIX3 *T, bool transparent = false);

	/**
	 * \brief Render an instrument panel in cockpit view as a 2D billboard.
	 * \param hSurf array of texture handles for the panel surface
	 * \param hMesh billboard mesh handle
	 * \param T transformation matrix for panel mesh vertices (2D)
	 * \param alpha opacity value, between 0 (transparent) and 1 (opaque)
	 * \param additive If true, panel should be rendered additive (transparent)
	 * \default None.
	 * \note The texture index of each group in the mesh is interpreted as index into the
	 *   hSurf array. Special indices are TEXIDX_MFD0 and above, which specify the
	 *   surfaces representing the MFD displays. These are obtained separately and
	 *   don't need to be present in the hSurf list.
	 * \note The \e additive flag is used when rendering the default "glass
	 *   cockpit" if the user requested. "transparent MFDs". The renderer can
	 *   then use e.g. additive blending for rendering the panel.
	 */
	void clbkRender2DPanel (SURFHANDLE *hSurf, MESHHANDLE hMesh, MATRIX3 *T, float alpha, bool additive = false);

	// ==================================================================
	/// \name Surface-related methods
	// @{

	/**
	 * \brief Create a surface for texturing, as a blitting source, etc.
	 *
	 * Surfaces are used for offscreen bitmap and texture manipulation,
	 * blitting and rendering.
	 * Derived classes should create a device-specific surface, and
	 * return a cast to a generic Orbiter SURFHANDLE.
	 * \param w surface width [pixels]
	 * \param h surface height [pixels]
	 * \param attrib \ref surfacecaps (bitflags). See notes.
	 * \return Surface handle (in the simplest case, just a pointer to the
	 *   surface, cast to a SURFHANDLE). On failure, this method should
	 *   return NULL.
	 * \default None, returns NULL.
	 * \note The attribute flag can contain one of the following main attributes:
	 *  - OAPISURFACE_RO: create a surface that can be read by the GPU pipeline, and that can be updated from system memory.
	 *  - OAPISURFACE_RW: create a surface that can be read and written by the GPU pipeline, and that can be updated from system memory.
	 *  - OAPISURFACE_GDI: create a surface that can be read and written from the CPU, and can be blitted into an uncompressed RO or RW surface without an alpha channel
	 *  In addition, the flag can contain any combination of the following auxiliary attributes:
	 *  - OAPISURFACE_MIPMAPS: create a full chain of mipmaps for the surface if possible
	 *  - OAPISURFACE_NOALPHA: create a surface without an alpha channel
	 */
	SURFHANDLE clbkCreateSurfaceEx (DWORD w, DWORD h, DWORD attrib);

	/**
	 * \brief Create an offscreen surface
	 *
	 * Surfaces are used for offscreen bitmap and texture manipulation,
	 * blitting and rendering.
	 * Derived classes should create a device-specific surface, and
	 * return a cast to a generic Orbiter SURFHANDLE.
	 * \param w surface width [pixels]
	 * \param h surface height [pixels]
	 * \param hTemplate surface format template
	 * \return pointer to surface, cast into a SURFHANDLE, or NULL to
	 *   indicate failure.
	 * \default None, returns NULL.
	 * \note If \e hTemplate is provided, this method should create the new
	 *   surface with the same pixel format.
	 * \sa clbkCreateTexture, clbkReleaseSurface
	 */
	SURFHANDLE clbkCreateSurface (DWORD w, DWORD h, SURFHANDLE hTemplate = NULL);

	/**
	 * \brief Create a texture for rendering
	 * \param w texture width
	 * \param h texture height
	 * \return pointer to texture, returned as generic SURFHANDLE. NULL
	 *   indicates failure.
	 * \note This method is similar to \ref clbkCreateSurface, but the
	 *   returned surface handle must be usable as a texture when rendering
	 *   the scene. Clients which don't differentiate between offscreen
	 *   surfaces and textures may use identical code for both functions.
	 * \note Some clients may put restrictions on the texture format (e.g.
	 *   require square size (w=h), and/or powers of two (w=2^n). If the
	 *   texture cannot be created with the requested size, this method
	 *   should return NULL.
	 * \sa clbkCreateSurface, clbkReleaseSurface
	 */
	SURFHANDLE clbkCreateTexture (DWORD w, DWORD h);

	/**
	 * \brief Create an offscreen surface from a bitmap
	 * \param hBmp bitmap handle
	 * \return surface handle, or NULL to indicate failure
	 * \default Creates a surface of the same size as the bitmap, and
	 *   uses clbkCopyBitmap to copy the bitmap over.
	 * \note The reference counter for the new surface is set to 1.
	 * \sa clbkIncrSurfaceRef, clbkReleaseSurface
	 */
	SURFHANDLE clbkCreateSurface (HBITMAP hBmp);

	/**
	 * \brief Increment the reference counter of a surface.
	 * \param surf surface handle
	 * \default None.
	 * \note Derived classes should keep track on surface references, and
	 *   overload this function to increment the reference counter.
	 */
	void clbkIncrSurfaceRef (SURFHANDLE surf);

	/**
	 * \brief Decrement surface reference counter, release surface if counter
	 *   reaches 0.
	 * \param surf surface handle
	 * \return true on success
	 * \default None, returns false.
	 * \note Derived classes should overload this function to decrement a
	 *   surface reference counter and release the surface if required.
	 * \sa clbkCreateSurface, clbkIncrSurfaceRef
	 */
	bool clbkReleaseSurface (SURFHANDLE surf);

	/**
	 * \brief Return the width and height of a surface
	 * \param[in] surf surface handle
	 * \param[out] w surface width
	 * \param[out] h surface height
	 * \return true if surface dimensions could be obtained.
	 * \default Sets w and h to 0 and returns false.
	 * \sa clbkCreateSurface
	 */
	bool clbkGetSurfaceSize (SURFHANDLE surf, DWORD *w, DWORD *h);

	/**
	 * \brief Set transparency colour key for a surface.
	 * \param surf surface handle
	 * \param ckey transparency colour key value
	 * \default None, returns false.
	 * \note Derived classes should overload this method if the renderer
	 *   supports colour key transparency for surfaces.
	 */
	bool clbkSetSurfaceColourKey (SURFHANDLE surf, DWORD ckey);

	/**
	 * \brief Convert an RGB colour triplet into a device-specific colour value.
	 * \param r red component
	 * \param g green component
	 * \param b blue component
	 * \return colour value
	 * \note Derived classes should overload this method to convert RGB colour
	 *   definitions into device-compatible colour values, taking into account
	 *   the colour depth of the render device etc.
	 * \default Packs the RGB values into a DWORD of the form 0x00RRGGBB, with
	 *   8 bits per colour component.
	 * \sa clbkFillSurface
	 */
	DWORD clbkGetDeviceColour (BYTE r, BYTE g, BYTE b);
	// @}

	// ==================================================================
	/// \name Surface blitting methods
	// @{

	/**
	 * \brief Copy one surface into an area of another one.
	 * \param tgt target surface handle
	 * \param tgtx left edge of target rectangle
	 * \param tgty top edge of target rectangle
	 * \param src source surface handle
	 * \param flag blitting parameters (see notes)
	 * \return true on success, false if the blit cannot be performed.
	 * \default None, returns false.
	 * \note By convention, tgt==NULL is valid and refers to the primary render
	 *   surface (e.g. for copying 2-D overlay surfaces).
	 * \note The following bit-flags are defined:
	 *   <table col=2>
	 *   <tr><td>BLT_SRCCOLORKEY</td><td>Use the colour key defined by the source surface for transparency</td></tr>
	 *   <tr><td>BLT_TGTCOLORKEY</td><td>Use the colour key defined by the target surface for transparency</td></tr>
	 *   </table>
	 *   If a client doesn't support some of the flags, it should quietly ignore it.
	 * \sa clbkBlt(SURFHANDLE,DWORD,DWORD,SURFHANDLE,DWORD,DWORD,DWORD,DWORD,DWORD)
	 */
	bool clbkBlt (SURFHANDLE tgt, DWORD tgtx, DWORD tgty, SURFHANDLE src, DWORD flag = 0) const;

	/**
	 * \brief Copy a rectangle from one surface to another.
	 * \param tgt target surfac handle
	 * \param tgtx left edge of target rectangle
	 * \param tgty top edge of target rectangle
	 * \param src source surface handle
	 * \param srcx left edge of source rectangle
	 * \param srcy top edge of source rectangle
	 * \param w width of rectangle
	 * \param h height of rectangle
	 * \param flag blitting parameters (see notes)
	 * \return true on success, false if the blit cannot be performed.
	 * \default None, returns false.
	 * \note By convention, tgt==NULL is valid and refers to the primary render
	 *   surface (e.g. for copying 2-D overlay surfaces).
	 * \note The following bit-flags are defined:
	 *   <table col=2>
	 *   <tr><td>BLT_SRCCOLORKEY</td><td>Use the colour key defined by the source surface for transparency</td></tr>
	 *   <tr><td>BLT_TGTCOLORKEY</td><td>Use the colour key defined by the target surface for transparency</td></tr>
	 *   </table>
	 *   If a client doesn't support some of the flags, it should quietly ignore it.
	 * \sa clbkBlt(SURFHANDLE,DWORD,DWORD,SURFHANDLE,DWORD)
	 */
	bool clbkBlt (SURFHANDLE tgt, DWORD tgtx, DWORD tgty, SURFHANDLE src, DWORD srcx, DWORD srcy, DWORD w, DWORD h, DWORD flag = 0) const;

	/**
	 * \brief Copy a rectangle from one surface to another, stretching or shrinking as required.
	 * \param tgt target surface handle
	 * \param tgtx left edge of target rectangle
	 * \param tgty top edge of target rectangle
	 * \param tgtw width of target rectangle
	 * \param tgth height of target rectangle
	 * \param src source surface handle
	 * \param srcx left edge of source rectangle
	 * \param srcy top edge of source rectangle
	 * \param srcw width of source rectangle
	 * \param srch height of source rectangle
	 * \param flag blitting parameters
	 * \return true on success, fals if the blit cannot be performed.
	 * \default None, returns false.
	 * \note By convention, tgt==NULL is valid and refers to the primary render
	 *   surface (e.g. for copying 2-D overlay surfaces).
	 * \sa clbkBlt(SURFHANDLE,DWORD,DWORD,SURFHANDLE,DWORD),
	 *   clbkBlt(SURFHANDLE,DWORD,DWORD,SURFHANDLE,DWORD,DWORD,DWORD,DWORD,DWORD)
	 */
	bool clbkScaleBlt (SURFHANDLE tgt, DWORD tgtx, DWORD tgty, DWORD tgtw, DWORD tgth,
		                       SURFHANDLE src, DWORD srcx, DWORD srcy, DWORD srcw, DWORD srch, DWORD flag = 0) const;


	/**
	 * \brief Begins a block of blitting operations to the same target surface.
	 * \param tgt Target surface for subsequent blitting calls.
	 * \return Should return an error code (0 on success, the return value from
	 *   the base class call, or a client-specific code)
	 * \default If no target is currently set, stores tgt in surfBltTgt and
	 *   returns 0. Returns -2 if a target was already set. Returns -3 if
	 *   tgt==RENDERTGT_NONE
	 * \note All blitting calls following this function use the same target until
	 *   \ref clbkEndBltGroup is called. Clients which have to perform initialisations
	 *   for a new blitting target can do this here, and then assume for the
	 *   following oapiBlt calls that the target is already initialised.
	 * \note The special target RENDERTGT_MAINWINDOW refers to the main render surface.
	 * \note Within the blitting block, multiple source surfaces may be used. No
	 *   particular order or grouping according to source surface can be guaranteed.
	 * \note Blitting calls using a different target within a blitting group are not
	 *   filtered out. It is up to the client how to handle this (honour the call,
	 *   ignore it or throw an error)
	 * \sa clbkEndBltGroup
	 */
	int clbkBeginBltGroup (SURFHANDLE tgt);

	/**
	 * \brief Ends a block of blitting operations to the same target surface.
	 * \return Should return an error code (0 on success, the return value from
	 *   the base class call, or a client-specific code)
	 * \default If surfBltTgt was set, clears it (to RENDERTGT_NONE) and returns 0.
	 *   Otherwise returns -2;
	 * \sa clbkBeginBltGroup
	 */
	int clbkEndBltGroup ();

	/**
	 * \brief Fill a surface with a uniform colour
	 * \param surf surface handle
	 * \param col colour value
	 * \return true on success, false if the fill operation cannot be performed.
	 * \default None, returns false.
	 * \note Parameter col is a device-dependent colour value
	 *   (see \ref clbkGetDeviceColour).
	 * \sa clbkFillSurface(SURFHANDLE,DWORD,DWORD,DWORD,DWORD,DWORD)
	 */
	bool clbkFillSurface (SURFHANDLE surf, DWORD col) const;

	/**
	 * \brief Fill an area in a surface with a uniform colour
	 * \param surf surface handle
	 * \param tgtx left edge of target rectangle
	 * \param tgty top edge of target rectangle
	 * \param w width of rectangle
	 * \param h height of rectangle
	 * \param col colour value
	 * \return true on success, false if the fill operation cannot be performed.
	 * \default None, returns false.
	 * \note Parameter col is a device-dependent colour value
	 *   (see \ref clbkGetDeviceColour).
	 * \sa clbkFillSurface(SURFHANDLE,DWORD)
	 */
	bool clbkFillSurface (SURFHANDLE surf, DWORD tgtx, DWORD tgty, DWORD w, DWORD h, DWORD col) const;


	/**
	 * \brief Copy a bitmap object into a surface
	 * \param pdds surface handle
	 * \param hbm bitmap handle
	 * \param x left edge of source bitmap area to be copied
	 * \param y top edge of source bitmap area to be copied
	 * \param dx width of source bitmap area to be copied
	 * \param dy height of source bitmap area to be copied
	 * \return \e true on success, \e false if surface or bitmap handle are invalid.
	 * \note The source bitmap area is stretched as required to fit the area of
	 *   the target surface.
	 */
	bool clbkCopyBitmap (SURFHANDLE pdds, HBITMAP hbm, int x, int y, int dx, int dy);
	// @}


	// ==================================================================
	/// \name 2-D drawing interface
	//@{
	/**
	 * \brief Create a 2-D drawing object ("sketchpad") associated with a surface.
	 * \param surf surface handle
	 * \return Pointer to drawing object.
	 * \default None, returns NULL.
	 * \note Clients should overload this function to provide 2-D drawing
	 *   support. This requires an implementation of a class derived from
	 *   \ref Sketchpad which provides the drawing context and drawing
	 *   primitives.
	 * \sa Sketchpad, clbkReleaseSketchpad
	 */
	Sketchpad* clbkGetSketchpad_const(SURFHANDLE surf) const;
	Sketchpad * clbkGetSketchpad (SURFHANDLE surf);

	/**
	 * \brief Release a drawing object.
	 * \param sp pointer to drawing object
	 * \default None.
	 * \sa Sketchpad, clbkGetSketchpad
	 */
	void clbkReleaseSketchpad_const(Sketchpad* sp) const;
	void clbkReleaseSketchpad (Sketchpad *sp);

	/**
	 * \brief Create a font resource for 2-D drawing.
	 * \param height cell or character height [pixel]
	 * \param prop proportional/fixed width flag
	 * \param face font face name
	 * \param style font decoration style
	 * \param orientation text orientation [1/10 deg]
	 * \return Pointer to font resource
	 * \default None, returns NULL.
	 * \note For a description of the parameters, see Font constructor
	 *   \ref oapi::Font::Font
	 * \sa clbkReleaseFont, oapi::Font
	 */
	Font* clbkCreateFont(int height, bool prop, const char* face, FontStyle style = FontStyle::FONT_NORMAL, int orientation = 0) const;
	Font* clbkCreateFontEx(int height, char* face, int width, int weight, FontStyle style, float spacing) const;

	/**
	 * \brief De-allocate a font resource.
	 * \param font pointer to font resource
	 * \default None.
	 * \sa clbkCreateFont, oapi::Font
	 */
	void clbkReleaseFont (Font *font) const;

	/**
	 * \brief Create a pen resource for 2-D drawing.
	 * \param style line style (0=invisible, 1=solid, 2=dashed)
	 * \param width line width [pixel]
	 * \param col line colour (format: 0xBBGGRR)
	 * \return Pointer to pen resource
	 * \default None, returns NULL.
	 * \sa clbkReleasePen, oapi::Pen
	 */
	Pen *clbkCreatePen (int style, int width, DWORD col) const;

	/**
	 * \brief De-allocate a pen resource.
	 * \param pen pointer to pen resource
	 * \default None.
	 * \sa clbkCreatePen, oapi::Pen
	 */
	void clbkReleasePen (Pen *pen) const;

	/**
	 * \brief Create a brush resource for 2-D drawing.
	 * \param col line colour (format: 0xBBGGRR)
	 * \return Pointer to brush resource
	 * \default None, returns NULL.
	 * \sa clbkReleaseBrush, oapi::Brush
	 */
	Brush *clbkCreateBrush (DWORD col) const;

	/**
	 * \brief De-allocate a brush resource.
	 * \param brush pointer to brush resource
	 * \default None.
	 * \sa clbkCreateBrush, oapi::Brush
	 */
	void clbkReleaseBrush (Brush *brush) const;
	//@}


	// ==================================================================
	/// \name GDI-related methods
	// @{

	/**
	 * \brief Return a Windows graphics device interface handle for a surface
	 * \param surf surface handle
	 * \return GDI handle, or NULL on failure
	 * \default None, returns NULL.
	 * \note Clients which can obtain a Windows GDI handle for a surface should
	 *   overload this method.
	 * \todo This method should be moved into the GDIClient class
	 */
	HDC clbkGetSurfaceDC (SURFHANDLE surf);

	/**
	 * \brief Release a Windows graphics device interface
	 * \param surf surface handle
	 * \param hDC GDI handle
	 * \default None.
	 * \note Clients which can obtain a Windows GDI handle for a surface should
	 *   overload this method to release an existing GDI.
	 * \todo This method should be moved into the GDIClient class
	 */
	void clbkReleaseSurfaceDC (SURFHANDLE surf, HDC hDC);
	// @}

	/**
	 * \brief Filter elevation grid data
	 * \param hPlanet object handle of the planet the data belongs to
	 * \param ilat patch latitude index
	 * \param ilng patch longitude index
	 * \param lvl patch resolution level
	 * \param elev_res elevation level resolution
	 * \param elev pointer to array with elevation grid data
	 * \default None.
	 * \note Clients that manipulate elevation file data in memory (e.g. for flattening
	 *   features) for visuals should overload this method in order to manipulate the
	 *   terrain collision data in the same way. As soon as the internal collision tile
	 *   is loaded in the core, the callback is invoked.
	 */
	bool clbkFilterElevation(OBJHANDLE hPlanet, int ilat, int ilng, int lvl, double elev_res, INT16* elev);
	// @}


	HWND				GetRenderWindow () const { return hRenderWnd; }
	CD3DFramework9 *    GetFramework() const { return pFramework; }
	Scene *             GetScene() const { return scene; }
	MeshManager *       GetMeshMgr() const { return meshmgr; }
	void 				WriteLog (const char *msg) const;
    LPDIRECT3DDEVICE9   GetDevice() const { return pDevice; }
	lpSurfNative		GetDefaultTexture() const;
	SURFHANDLE			GetBackBufferHandle() const;
	LPDIRECT3DTEXTURE9  GetNoiseTex() const { return pNoiseTex; }
	void 				SplashScreen();
	inline bool			IsControlPanelOpen() const { return bControlPanel; }
	inline bool 		IsRunning() const { return bRunning; }
	inline bool			IsLimited() const { return ((pCaps->TextureCaps&D3DPTEXTURECAPS_POW2) && (pCaps->TextureCaps&D3DPTEXTURECAPS_NONPOW2CONDITIONAL)); }
	const LPD3DXMATRIX 	GetIdentity() const { return (const LPD3DXMATRIX)&ident; }
	HWND 				GetWindow();
	bool 				HasVertexTextureSupport() const { return bVertexTex; }
	const D3DCAPS9 *	GetHardwareCaps() const { return pCaps; }
	//FileParser *		GetFileParser() const { return parser; }
	LPDIRECT3DSURFACE9	GetBackBuffer() const { return pBackBuffer; }
	LPDIRECT3DSURFACE9	GetDepthStencil() const { return pDepthStencil; }
	const void *		GetConfigParam (DWORD paramtype) const;
	bool				RegisterRenderProc(__gcRenderProc proc, DWORD id, void *pParam = NULL);
	bool				RegisterGenericProc(__gcGenericProc proc, DWORD id, void *pParam = NULL);
	void				MakeRenderProcCall(Sketchpad *pSkp, DWORD id, LPD3DXMATRIX pV, LPD3DXMATRIX pP);
	void				MakeGenericProcCall(DWORD id, int iUser, void *pUser) const;
	bool				IsGenericProcEnabled(DWORD id) const;
	void				SetScenarioName(const std::string &path) { scenarioName = path; };
	void				HackFriendlyHack();
	void				PickTerrain(DWORD uMsg, int xpos, int ypos);
	DEVMESHHANDLE		GetDevMesh(MESHHANDLE hMesh);
	HANDLE				GetMainThread() const {	return hMainThread;	}


	// ==================================================================
	//
	HRESULT				BeginScene();
	void				EndScene();
	bool				IsInScene() const { return bRendering; }
	void				PushSketchpad(SURFHANDLE surf, D3D9Pad* pSkp) const;
	void				PushRenderTarget(LPDIRECT3DSURFACE9 pColor, LPDIRECT3DSURFACE9 pDepthStencil = NULL, int code = 0) const;
	void				AlterRenderTarget(LPDIRECT3DSURFACE9 pColor, LPDIRECT3DSURFACE9 pDepthStencil = NULL);
	void				PopRenderTargets() const;
	LPDIRECT3DSURFACE9  GetTopDepthStencil();
	LPDIRECT3DSURFACE9  GetTopRenderTarget();
	class D3D9Pad *		GetTopInterface() const;


protected:

	/** \brief Launchpad video tab indicator
	 *
	 * Indicate if the the default video tab in the Orbiter launchpad dialog
	 * is to be used for obtaining user video preferences. If a derived
	 * class returns false here, the video tab is not shown.
	 * \return true if the module wants to use the video tab in the launchpad
	 *   dialog, false otherwise.
	 * \default Return true.
	 */
	bool clbkUseLaunchpadVideoTab () const;

	/**
	 * \brief Simulation session start notification
	 *
	 * Called at the beginning of a simulation session to allow the client
	 * to create the 3-D rendering window (or to switch into fullscreen
	 * mode).
	 * \return Should return window handle of the rendering window.
	 * \default For windowed mode, opens a window of the size specified by the
	 *   VideoData structure (for fullscreen mode, opens a small dummy window)
	 *   and returns the window handle.
	 * \note For windowed modes, the viewW and viewH parameters should return
	 *   the window client area size. For fullscreen mode, they should contain
	 *   the screen resolution.
	 * \note Derived classes should perform any required per-session
	 *   initialisation of the 3D render environment here.
	 */
	HWND clbkCreateRenderWindow ();

	/**
	 * \brief Simulation startup finalisation
	 *
	 * Called at the beginning of a simulation session after the scenarion has
	 * been parsed and the logical object have been created.
	 * \default None
	 */
	void clbkPostCreation ();

	/**
	 * \brief End of simulation session notification
	 *
	 * Called before the end of a simulation session. At the point of call,
	 * logical objects still exist (OBJHANDLEs valid), and external modules
	 * are still loaded.
	 * \param fastclose Indicates a "fast shutdown" request (see notes)
	 * \default None.
	 * \note Derived clients can use this function to perform cleanup operations
	 *   for which the simulation objects are still required.
	 * \note If fastclose == true, the user has selected one of the fast
	 *   shutdown options (terminate Orbiter, or respawn Orbiter process). In
	 *   this case, the current process will terminate, and the graphics client
	 *   can skip object cleanup and deallocation in order to speed up the
	 *   closedown process.
	 * \sa clbkDestroyRenderWindow
	 */
	void clbkCloseSession (bool fastclose);

	/**
	 * \brief Render window closure notification
	 *
	 * Called at the end of a simulation session to allow the client to close
	 * the 3-D rendering window (or to switch out of fullscreen mode) and
	 * clean up the session environment. At the point of call, all logical
	 * simulation objects have been destroyed, and object modules have been
	 * unloaded. This method should not access any OBJHANDLE or VESSEL
	 * objects any more. For closedown operations that require access to the
	 * simulation objects, use clbkCloseSession instead.
	 * \param fastclose Indicates a "fast shutdown" request (see notes)
	 * \default None.
	 * \note Derived classes should perform any required cleanup of the 3D
	 *   render environment here.
	 * \note The user may change the video parameters before starting a new
	 *   simulation session. Therefore, device-specific options should be
	 *   destroyed and re-created at the start of the next session.
	 * \note If fastclose == true, the user has selected one of the fast
	 *   shutdown options (terminate Orbiter, or respawn Orbiter process). In
	 *   this case, the current process will terminate, and the graphics client
	 *   can skip object cleanup and deallocation in order to speed up the
	 *   closedown process.
	 * \sa clbkCloseSession
	 */
	void clbkDestroyRenderWindow (bool fastclose);

	/**
	 * \brief Per-frame update notification
	 *
	 * Called once per frame, after the logical world state has been updated,
	 * but before clbkRenderScene(), to allow the client to perform any
	 * logical state updates.
	 * \param running true if simulation is running, false if paused.
	 * \default None.
	 * \note Unlike clbkPreStep and clbkPostStep, this method is also called
	 *   while the simulation is paused.
	 */
	void clbkUpdate (bool running);

	/**
	 * \brief Per-frame render notification
	 *
	 * Called once per frame, after the logical world state has been updated,
	 * to allow the client to render the current scene.
	 * \note This method is also called continuously while the simulation is
	 *   paused, to allow camera panning (although in that case the logical
	 *   world state won't change between frames).
	 * \note After the 3D scene has been rendered, this function should call
	 *   \ref Render2DOverlay to initiate rendering of 2D elements (2D instrument
	 *   panel, HUD, etc.)
	 */
	void clbkRenderScene ();

	/**
	 * \brief React to a discontinous jump in simulation time.
	 * \param simt new simulation time relative to session start [s]
	 * \param simdt jump interval [s]
	 * \param mjd new absolute simulation time in MJD format [days]
	 * \note Currently, this method does nothing.
	 */
	void clbkTimeJump (double simt, double simdt, double mjd);

	/**
	 * \brief Display a scene on screen after rendering it.
	 *
	 * Called after clbkRenderScene to allow the client to display the rendered
	 * scene (e.g. by page-flipping, or blitting from background to primary
	 * frame buffer. This method can also be used by the client to display any
	 * top-level 2-D overlays (e.g. dialogs) on the primary frame buffer.
	 * \return Should return true on successful operation, false on failure or
	 *   if no operation was performed.
	 * \default None, returns false.
	 */
	bool clbkDisplayFrame ();

	/**
	 * \brief Display a load status message on the splash screen
	 *
	 * Called repeatedly while a simulation session is loading, to allow the
	 * client to echo load status messages on its splash screen if desired.
	 * \param msg Pointer to load status message string
	 * \param line message line to be displayed (0 or 1), where 0 indicates
	 *   a group or category heading, and 1 indicates an individual action
	 *   relating to the most recent group.
	 * \return Should return true if it displays the message, false if not.
	 * \default None, returns false.
	 */
	bool clbkSplashLoadMsg (const char *msg, int line);

	void clbkSetSplashScreen(const char *filename, DWORD textCol) override;

	/**
	 * \brief Store a persistent mesh template
	 *
	 * Called when a plugin loads a mesh with oapiLoadMeshGlobal, to allow the
	 * client to store a copy of the mesh in client-specific format. Whenever
	 * the mesh is required later, the client can create an instance as a copy
	 * of the template, rather than creating it by converting from Orbiter's
	 * mesh format.
	 * \param hMesh mesh handle
	 * \param fname mesh file name
	 * \default None.
	 * \note Use \ref oapiMeshGroup to to obtain mesh data and convert them to
	 *   a suitable format.
	 * \note the mesh templates loaded with \ref oapiLoadMeshGlobal are shared between
	 *   all vessel instances and should never be edited. Vessels should make
	 *   individual copies of the mesh before modifying them (e.g. for animations)
	 * \note The file name is provide to allow the client to parse the mesh directly
	 *   from file, rather than copying it from the hMesh object, or to use an
	 *   alternative mesh file.
	 * \note The file name contains a path relative to Orbiter's main mesh
	 *   directory.
	 */
	void clbkStoreMeshPersistent (MESHHANDLE hMesh, const char *fname);

	/**
	 * \brief Renders the fullscreen viewport in the presence of popup windows.
	 * \return \e true if render window has been updated and no more page flipping
	 *   is required, \e false if page still needs to be flipped.
	 */
	bool RenderWithPopupWindows ();

	/// \brief Output 2D graphics on top of the render window
	///
	/// Obtains the GDI of the render surface to output 2D data after
	/// rendering the 3D scene (glass cockpit, date info, etc.)
	//void Output2DOverlay ();

public:

	/**
	 * \brief Displays a message on the splash screen.
	 * \param msg Message to be displayed.
	 * \param line line number (0 or 1)
	 * \return true if load status was initialised, false if not.
	 * \sa clbkSplashLoadMsg
	 */
	bool OutputLoadStatus (const char *msg, int line);

private:
	void BltError(SURFHANDLE src, SURFHANDLE tgt, const LPRECT s, const LPRECT t, bool bHalt = true) const;
	void SketchPadTest();
	void PresentScene();
	void Label(const char *format, ...);
	void DrawTimeBar(double t, double scale, double frames, DWORD color, const char *label=NULL);
	bool ChkDev(const char *fnc) const;

	SURFHANDLE				pBltGrpTgt;
	D3D9Pad*				pBltSkp;


    LPDIRECT3DDEVICE9		pDevice;
	lpSurfNative			pDefaultTex;
	lpSurfNative			pScatterTest;
	LPDIRECT3DTEXTURE9		pNoiseTex;
	LPDIRECT3DSURFACE9		pSplashScreen;
	LPDIRECT3DSURFACE9		pTextScreen;
	LPDIRECT3DSURFACE9		pBackBuffer;
	LPDIRECT3DSURFACE9		pDepthStencil;
	CD3DFramework9 *		pFramework;
	const D3DCAPS9 *		pCaps;
	std::string				scenarioName;
	HANDLE					hMainThread;
	WindowManager *			pWM;
	const char *            pCustomSplashScreen;
	DWORD                   pSplashTextColor;

	HWND hRenderWnd;        // render window handle

	bool bControlPanel;
	bool bScatterUpdate;
	bool bFullscreen;       // fullscreen render mode flag
	bool bAAEnabled;
	bool bFailed;
	bool bRunning;
	bool bVertexTex;
	bool bVSync;
	bool bRendering;
	bool bGDIClear;

	DWORD viewW, viewH;     // dimensions of the render viewport
	DWORD viewBPP;          // bit depth of render viewport
	DWORD frame_timer;

	// device enumeration callback function

	VideoTab *vtab;			// video selection user interface
	Scene *scene;           // Scene description

	MeshManager *meshmgr;   // mesh manager
	D3DXMATRIX ident;

	struct RenderProcData;
	struct GenericProcData;

	std::vector<RenderProcData> RenderProcs;
	std::vector<GenericProcData> GenericProcs;
	mutable std::list<RenderTgtData> RenderStack;

	HFONT hLblFont1;
	HFONT hLblFont2;

	char pLoadLabel[128];
	char pLoadItem[128];

	// Control Panel
	void RenderControlPanel();
	bool ControlPanelMsg(WPARAM wParam);

	Sketchpad *pItemsSkp;

	DWORD loadd_x, loadd_y, loadd_w, loadd_h;
	int LabelPos;

}; // class D3D9Client


// ======================================================================
// class VisObject
// ======================================================================
/**
 * \brief Visual object representation.
 *
 * A VisObject is the visual representation of an Orbiter object (vessel,
 * planet, etc.). The 'logical' object representation resides in the Orbiter
 * core, while its 'visual' representation is located in the graphics client.
 *
 * Visual representations should be non-permanent: they should be created
 * when the object enters the visual range of the camera, and deleted when
 * they leave it.
 *
 * Only a single VisObject instance should be created per object, even if
 * the visual is present in multiple views. If the graphics client supports
 * multiple views, the view-specific parameters (e.g. visibility flags) should
 * be implemented by the client, e.g. by deriving a class from VisObject that
 * holds an array of the view-specific data. In that case, the VisObject should
 * be created when the object becomes visible in any one view, and destroyed
 * when the object disappears from the last view.
 */
class VisObject {
public:
	/**
	 * \brief Creates a visual for object hObj.
	 * \param hObj object handle
	 * \sa oapi::GraphicsClient::RegisterVisObject
	 */
	explicit VisObject (OBJHANDLE hObj);

	/**
	 * \brief Destroys the visual.
	 * \sa oapi::GraphicsClient::UnregisterVisObject
	 */
	virtual ~VisObject ();

	/**
	 * \brief Returns the object handle associated with the visual.
	 * \return Object handle
	 */
	OBJHANDLE GetObjHandle () const { return hObj; }

	/**
	 * \brief Message callback.
	 * \param event message identifier
	 * \param context message content (message-specific)
	 * \default None.
	 * \note This method is called by the Orbiter core to notify the visual
	 *   of certain events (e.g. adding and deleting meshes)
	 * \note For currently supported event types, see \ref visevent.
	 */
	virtual void clbkEvent (DWORD event, DWORD_PTR context) {}

protected:
	OBJHANDLE hObj;	///< Object handle associated with the visual
};

}; // namespace oapi



class RenderState
{
public:

	RenderState(LPDIRECT3DDEVICE9 pD) : pDev(pD)
	{
		bkABE = bkZEN = bkZW = bkCULL = 0;
		bkCW = bkSE = bkFM = bkSTE = bkATE = 0;
		bkBO = bkSB = bkDB = 0;
		bCaptured = false;
	}

	void Capture()
	{
		bCaptured = true;
		HR(pDev->GetScissorRect(&bkSR));
		HR(pDev->GetRenderState(D3DRS_ALPHABLENDENABLE, &bkABE));
		HR(pDev->GetRenderState(D3DRS_ZENABLE, &bkZEN));
		HR(pDev->GetRenderState(D3DRS_ZWRITEENABLE, &bkZW));
		HR(pDev->GetRenderState(D3DRS_CULLMODE, &bkCULL));
		HR(pDev->GetRenderState(D3DRS_COLORWRITEENABLE, &bkCW));
		HR(pDev->GetRenderState(D3DRS_SCISSORTESTENABLE, &bkSE));
		HR(pDev->GetRenderState(D3DRS_FILLMODE, &bkFM));
		HR(pDev->GetRenderState(D3DRS_STENCILENABLE, &bkSTE));
		HR(pDev->GetRenderState(D3DRS_ALPHATESTENABLE, &bkATE));
		HR(pDev->GetRenderState(D3DRS_BLENDOP, &bkBO));
		HR(pDev->GetRenderState(D3DRS_SRCBLEND, &bkSB));
		HR(pDev->GetRenderState(D3DRS_DESTBLEND, &bkDB));
	}

	void Restore()
	{
		assert(bCaptured);
		HR(pDev->SetScissorRect(&bkSR));
		HR(pDev->SetRenderState(D3DRS_ALPHABLENDENABLE, bkABE));
		HR(pDev->SetRenderState(D3DRS_ZENABLE, bkZEN));
		HR(pDev->SetRenderState(D3DRS_ZWRITEENABLE, bkZW));
		HR(pDev->SetRenderState(D3DRS_CULLMODE, bkCULL));
		HR(pDev->SetRenderState(D3DRS_COLORWRITEENABLE, bkCW));
		HR(pDev->SetRenderState(D3DRS_SCISSORTESTENABLE, bkSE));
		HR(pDev->SetRenderState(D3DRS_FILLMODE, bkFM));
		HR(pDev->SetRenderState(D3DRS_STENCILENABLE, bkSTE));
		HR(pDev->SetRenderState(D3DRS_ALPHATESTENABLE, bkATE));
		HR(pDev->SetRenderState(D3DRS_BLENDOP, bkBO));
		HR(pDev->SetRenderState(D3DRS_SRCBLEND, bkSB));
		HR(pDev->SetRenderState(D3DRS_DESTBLEND, bkDB));
	}

	RECT bkSR = {};
	DWORD bkABE, bkZEN, bkZW, bkCULL, bkCW, bkSE, bkFM, bkSTE, bkATE, bkBO, bkSB, bkDB;
	LPDIRECT3DDEVICE9 pDev;
	bool bCaptured;
};

#endif // !__D3D9CLIENT_H

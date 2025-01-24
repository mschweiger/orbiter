// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
//                     ORBITER SOFTWARE DEVELOPMENT KIT
// GraphicsAPI.h
// ORBITER Application Programming Interface (OAPI)
// Interface for external graphics clients
// ======================================================================

#ifndef __GRAPHICSAPI_H
#define __GRAPHICSAPI_H

#include "Orbitersdk.h"
#include <stdio.h>
#include <windows.h>

#ifndef _WIN32
typedef void *HDC;
#endif

/// \defgroup cfgprm Configuration parameter identifiers
/// Used by GraphicsClient::GetConfigParam()
/// @{

/// Max. level of detail for rendering planetary surfaces
/// \par Parameter type:
///   DWORD
#define CFGPRM_SURFACEMAXLEVEL 0x0001

/// Flag for rendering planet surfaces with specular reflections
/// (e.g. for oceans)
/// \par Parameter type:
///   bool
#define CFGPRM_SURFACEREFLECT  0x0002

/// Flag for rendering specular reflections with microtexture
/// ("ripples")
/// \par Parameter type:
///   bool
#define CFGPRM_SURFACERIPPLE   0x0003

/// Flag for rendering emissive textures ("city lights") on the
/// unlit side of planetary surfaces.
/// \par Parameter type:
///   bool
#define CFGPRM_SURFACELIGHTS   0x0004

/// Brightness factor for emissive city light textures (0-1)
/// \par Parameter type:
///   double
#define CFGPRM_SURFACELIGHTBRT 0x0006

/// Flag for rendering "atmospheric haze" over planets with
/// atmospheres
/// \par Parameter type:
///   bool
#define CFGPRM_ATMHAZE         0x0007

/// Flag for rendering distance fog within planetary atmospheres
/// \par Parameter type:
///   bool
#define CFGPRM_ATMFOG          0x0008

/// Flag for rendering planetary cloud layers
/// \par Parameter type:
///   bool
#define CFGPRM_CLOUDS          0x0009

/// Flag for rendering cloud shadows on the planet surface
/// \par Parameter type:
///   bool
#define CFGPRM_CLOUDSHADOWS    0x000A

/// Bit flag for "planetarium mode" elements.
/// For a description of the available bit flags, see \ref plnflag
/// \par Parameter type:
///   DWORD
#define CFGPRM_PLANETARIUMFLAG 0x000B

/**
 * Parameters for rendering stars on the celestial sphere.
 * \par Parameter type:
 *   struct StarRenderPrm
 */
#define CFGPRM_STARRENDERPRM 0x000C

/**
 * Ambient light level (brightness of unlit nonemissive surfaces)
 * in the range 0-255.
 * \par Parameter type:
 *   DWORD
 */
#define CFGPRM_AMBIENTLEVEL    0x000E

/**
 * Flag for rendering vessel shadows on the planet surface
 * \par Parameter type:
 *   bool
 */
#define CFGPRM_VESSELSHADOWS   0x000F

/**
 * Flag for rendering object shadows on the planet surface
 * \par Parameter type:
 *   bool
 */
#define CFGPRM_OBJECTSHADOWS   0x0010

/**
 * Flag for enabling specular reflections from objects
 * \par Parameter type:
 *   bool
 */
#define CFGPRM_OBJECTSPECULAR  0x0011

/**
 * \Flag for rendering background images from a texture set
 * \par Parameter type:
 *    bool
 */
#define CFGPRM_CSPHEREUSEBGIMAGE 0x0012

/**
 * File path for celestial sphere background textures
 * \par Parameter type:
 *   *char
 */
#define CFGPRM_CSPHERETEXTURE 0x0013

/**
 * Flag for rendering intensity of celestial sphere background textures
 * \par Parameter type:
 *   double
 */
#define CFGPRM_CSPHEREINTENS 0x0014

/**
 * Flag for enabling local light sources
 * \par Parameter type:
 *   bool
 */
#define CFGPRM_LOCALLIGHT 0x0015

/**
 * Max number of light sources
 * \par Parameter type:
 *   int
 */
#define CFGPRM_MAXLIGHT 0x0016

/**
 * Planet tile resolution bias
 * \par Parameter type:
 *   double
 */
#define CFGPRM_RESOLUTIONBIAS 0x0017

/**
 * Render meshes as wireframe models?
 * \par Parameter type:
 *   bool
 */
#define CFGPRM_WIREFRAME 0x0018

/**
 * Mesh interpolation method for planetary surfaces
 * (0=none, 1=linear, 2=cubic)
 * \par Parameter type:
 *   int
 */
#define CFGPRM_ELEVATIONMODE 0x0019

/**
 * Size of texture surface used for rendering panel MFDs and VC HUD
 * (multiple of 2, usually 256 or 512)
 * \par Parameter type:
 *   int
 */
#define CFGPRM_PANELMFDHUDSIZE 0x001A

/**
 * Scaling power n of planet surface patch mesh resolution 2 << n
 * Valid range: [4,5,6]
 * \par Parameter type:
 *   int
 */
#define CFGPRM_TILEPATCHRES 0x001B

/**
 * Bitflags for planetary tile file load behaviour
 * - bit 1: load from individual tiles in directory tree
 * - bit 2: load from compressed archive file
 * \par Parameter type:
 *   DWORD
 */
#define CFGPRM_TILELOADFLAGS 0x001C

/**
 * Path to background star texture
 * \par Parameter type:
 *   *char
 */
#define CFGPRM_CSPHERESTARTEXTURE 0x001D

/**
 * \Flag for rendering background stars from a texture set
 * \par Parameter type:
 *    bool
 */
#define CFGPRM_CSPHEREUSESTARIMAGE 0x001E

/**
 * \Flag for rendering background stars as pixels
 * \par Parameter type:
 *    bool
 */
#define CFGPRM_CSPHEREUSESTARDOTS 0x001F

/**
 * Bit flags for force vector display options.
 * For a description of the available bit flags, see \ref bfvflag
 * \par Parameter type:
 *   DWORD
 */
#define CFGPRM_FORCEVECTORFLAG 0x0020

/**
 * Force vector display scaling factor
 * \par Parameter type:
 *   float
 */
#define CFGPRM_FORCEVECTORSCALE 0x0021

/**
 * Force vector display opacity value
 * \par Parameter type:
 *   float
 */
#define CFGPRM_FORCEVECTOROPACITY 0x0022

/**
 * Bit flags for frame axis display options.
 * For a description of the available bit flags, see \ref favflag
 * \par Parameter type:
 *   DWORD
 */
#define CFGPRM_FRAMEAXISFLAG 0x0023

/**
 * Frame axis display scaling factor
 * \par Parameter type:
 *   float
 */
#define CFGPRM_FRAMEAXISSCALE 0x0024

/**
 * Frame axis display opacity value
 * \par Parameter type:
 *   float
 */
#define CFGPRM_FRAMEAXISOPACITY 0x0025

 /**
  * Bit flags for surface and object marker display options.
  * For a description of the available bit flags, see \ref mkrflag
  * \par Parameter type:
  *   DWORD
  */
#define CFGPRM_SURFMARKERFLAG 0x0026
/// @}

/**
 * \defgroup renderprm Render parameter identifiers
 * \sa GraphicsClient::clbkGetRenderParam()
 */
/// @{
#define RP_COLOURDEPTH        1 ///< colour bit depth of the render target
#define RP_ZBUFFERDEPTH       2 ///< z-buffer depth of the render target
#define RP_STENCILDEPTH       3 ///< stencil buffer depth of the render target
#define RP_MAXLIGHTS          4 ///< maximum number of simultaneous light sources supported
#define RP_ISTLDEVICE         5 ///< render device supports Transform&Lighting
#define RP_REQUIRETEXPOW2     6 ///< texture size=pow2 required? (0=no, 1=yes, 2=conditional)
/// @}

/// \defgroup plnflag Bit flags for planetarium mode elements
/// @{
#define PLN_ENABLE    0x0001 ///< Enable planetarium mode (master flag)
#define PLN_CGRID     0x0002 ///< Enable celestial grid
#define PLN_EGRID     0x0004 ///< Enable ecliptic grid
#define PLN_GGRID     0x0008 ///< Enable galactic grid
#define PLN_HGRID     0x0010 ///< Enable local horizon grid
#define PLN_EQU       0x0020 ///< Enable target body equator
#define PLN_CONST     0x0040 ///< Enable constellation patterns
#define PLN_CNSTLABEL 0x0080 ///< Enable constellation labels
#define PLN_CNSTLONG  0x0100 ///< Enable long constellation names
#define PLN_CNSTBND   0x0200 ///< Enable constellation boundaries
#define PLN_CCMARK    0x0400 ///< Enable celestial sphere labels
/// @}

/// \defgroup mkrflag Bit flags for surface and object markers
/// @{
#define MKR_ENABLE    0x0001 ///< Enable surface and object markers
#define MKR_CMARK     0x0002 ///< Enable solar system body markers
#define MKR_VMARK     0x0004 ///< Enable vessel markers
#define MKR_BMARK     0x0008 ///< Enable surface base markers
#define MKR_RMARK     0x0010 ///< Enable VOR transmitter markers
#define MKR_LMARK     0x0020 ///< Enable planetary surface labels
#define MKR_SURFMARK (MKR_BMARK | MKR_RMARK | MKR_LMARK)
/// @}

/// \defgroup bfvflag Bit flags for vessel force vector render options
/// @{
#define BFV_ENABLE    0x0001 ///< Enable body force vectors
#define BFV_LOGSCALE  0x0002 ///< Use logarithmic scale (instead of linear scale)
#define BFV_WEIGHT    0x0004 ///< Show weight vector
#define BFV_THRUST    0x0008 ///< Show thrust vector
#define BFV_LIFT      0x0010 ///< Show lift vector
#define BFV_DRAG      0x0020 ///< Show drag vector
#define BFV_TOTAL     0x0040 ///< Show total force vector
#define BFV_TORQUE    0x0080 ///< Show torque vector
#define BFV_SIDEFORCE 0x0100 ///< Show side-force vector
/// @}

/// \defgroup favflag Bit flags for frame axis vector render options
/// @{
#define FAV_ENABLE    0x0001 ///< Enable frame axis vector display
#define FAV_NEGATIVE  0x0002 ///< Also show negative axes
#define FAV_VESSEL    0x0004 ///< Show vessel frame axes
#define FAV_CELBODY   0x0008 ///< Show celestial body frame axes
#define FAV_BASE      0x0010 ///< Show surface base frame axes
/// @}

/// \defgroup bltflag Bit flags for blitting operations
/// @{
#define BLT_SRCCOLORKEY 0x1 ///< Use source surface colour key for transparency
#define BLT_TGTCOLORKEY 0x2 ///< Use target surface colour key for transparency
/// @}

const UINT TEXIDX_MFD0 = (UINT)(-1) - MAXMFD;

struct StarRenderPrm {
	double mag_hi;
	double mag_lo;
	double brt_min;
	bool   map_log;
};

struct SurftileSpec {
	int res;
	int texflag;
	int ilng, ilat;
	MESHHANDLE mesh;
	SURFHANDLE tex;
};

/**
 * \brief Distance fog render parameters
 */
struct FogParam {
	double dens_0;   ///< fog density at ground level
	double dens_ref; ///< fog density at reference altitude
	double alt_ref;  ///< reference altitude [m]
	VECTOR3 col;     ///< fog colour
};

class Orbiter;
struct IWICImagingFactory;

namespace oapi {

/**
 * \brief Structure for defining a raw image.
 */
struct ImageData {
	BYTE *data;    ///< pointer to image data
	UINT bufsize;  ///< allocated size of \a data
	UINT width;    ///< image width [pixel]
	UINT height;   ///< image height [pixel]
	UINT bpp;      ///< bits per pixel
	UINT stride;   ///< number of bytes per scan line
};

class Sketchpad;
class ParticleStream;
class ScreenAnnotation;

// ======================================================================
// class GraphicsClient
// ======================================================================
/**
 * \brief Base class for external graphics client modules.
 *
 * This class defines the interface between the graphics-less version of
 * the Orbiter core and any external plugins providing a rendering
 * environment for the orbiter-generated scene.
 * The GraphicsClient base class is defined in terms of generic graphics
 * objects (meshes, textures, etc.) Derived classes can then adapt these
 * into specific rendering objects for a given 3-D rendering engine
 * (DX, OGL, etc.)
 */

class OAPIFUNC GraphicsClient: public Module {
	friend class ::Orbiter; ///< Orbiter private class

public:
	/**
	 * \brief Create a graphics object.
	 *
	 * The graphics object is typically created during module initialisation
	 * (see \ref InitModule). Once the client is created, it must be registered
	 * with the Orbiter core via the oapiRegisterGraphicsClient function.
	 * \param hInstance module instance handle (as passed to InitModule)
	 */
	GraphicsClient (HINSTANCE hInstance);

	/**
	 * \brief Destroy the graphics object.
	 *
	 * Usually, the graphics object is destroyed when the module is unloaded
	 * (see opcDLLExit), after is has been detached from the Orbiter core
	 * via a call to oapiUnregisterGraphicsClient.
	 */
	virtual ~GraphicsClient ();

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
	virtual bool clbkInitialise ();

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
	virtual void clbkRefreshVideoData () {}

	/**
	 * \brief Called when the user changes an option during a simulation session.
	 * \param cat option category identifier, see \ref optcat
	 * \param item option item identifier, see \ref optitem
	 * \default None.
	 */
	virtual void clbkOptionChanged(DWORD cat, DWORD item) {}

	/**
	 * \brief React to vessel creation, deletion, docking, attaching events
	 * \param hVesselA object handle of first vessel
	 * \param hVesselB object handle of second vessel
	 * \param type Event type
	 */
	virtual void clbkScenarioChanged(OBJHANDLE hVessel, ScnChgEvent type) {}

	/**
	 * \brief Print multiple debug strings onto a screen, will be cleared when printed on screen.
	 * \param str Text string to print
	 */
	virtual void clbkDebugString(const char *str) {}

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
	virtual SURFHANDLE clbkLoadTexture (const char *fname, DWORD flags = 0) { return NULL; }

	/**
	 * \brief Load a surface from file into a surface object, and return a SURFHANDLE for it.
	 * \param fname texture file name with path relative to orbiter texture folders
	 * \param attrib \ref surfacecaps (see notes)
	 * \return A SURFHANDLE for the loaded surface, for example a pointer to the surface object.
	 * \note If the request refers to a static surface that has already be loaded, or if the
	 *   client buffers the unmodified surfaces after loading, it can simply return a handle to
	 *   the existing surface object, instead of reading it again from file.
	 * \sa oapiCreateSurface(DWORD,DWORD,DWORD)
	 */
	virtual SURFHANDLE clbkLoadSurface (const char *fname, DWORD attrib, bool bPath = false)
	{ return NULL; }

	/**
	 */
	virtual SURFHANDLE clbkLoadMaps(const char* diff, const char* maps, bool bPath, SURFHANDLE hOld = NULL, bool bAll = true)
	{
		return clbkLoadTexture(diff);
	}

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
	virtual bool clbkSaveSurfaceToImage (SURFHANDLE surf, const char *fname,
		ImageFileFormat fmt, float quality=0.7f)
	{ return false; }

	/**
	 * \brief Texture release request
	 *
	 * Called by Orbiter when a previously loaded texture can be released
	 * from memory. The client can use the appropriate device-specific method
	 * to release the texture.
	 * \param hTex texture handle
	 * \default None.
	 */
	virtual void clbkReleaseTexture (SURFHANDLE hTex) {}

	/**
	 * \brief Replace a texture in a device-specific mesh.
	 * \param hMesh device mesh handle
	 * \param texidx texture index (>= 0)
	 * \param tex texture handle
	 * \return Should return \e true if operation successful, \e false otherwise.
	 * \default None, returns \e false.
	 */
	virtual bool clbkSetMeshTexture (DEVMESHHANDLE hMesh, DWORD texidx, SURFHANDLE tex) { return false; }

	/**
	 * \brief Replace properties of an existing mesh material.
	 * \param hMesh device mesh handle
	 * \param matidx material index (>= 0)
	 * \param mat pointer to material structure
	 * \return Overloaded functions should return an integer error flag, with
	 *   the following codes: 0="success", 3="invalid mesh handle", 4="material index out of range"
	 * \default None, returns 2 ("client does not support operation").
	 */
	virtual int clbkSetMeshMaterial (DEVMESHHANDLE hMesh, DWORD matidx, const MATERIAL *mat) { return 2; }
	virtual int clbkSetMeshMaterialEx(DEVMESHHANDLE hMesh, DWORD matidx, MatProp prp, const oapi::FVECTOR4* in) { return 2; }

	/**
	 * \brief Retrieve the properties of one of the mesh materials.
	 * \param hMesh device mesh handle
	 * \param matidx material index (>= 0)
	 * \param mat [out] pointer to MATERIAL structure to be filled by the method.
	 * \return true if successful, false on error (index out of range)
	 * \default None, returns 2 ("client does not support operation").
	 */
	virtual int clbkMeshMaterial (DEVMESHHANDLE hMesh, DWORD matidx, MATERIAL *mat) { return 2; }
	virtual int clbkMeshMaterialEx(DEVMESHHANDLE hMesh, DWORD matidx, MatProp prp, oapi::FVECTOR4* out) { return 2; }

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
	virtual bool clbkSetMeshProperty(DEVMESHHANDLE hMesh, DWORD property, DWORD value) { return false; }

	// ==================================================================
	/// \name Visual object interface
	//@{
	/**
	 * \brief Register a new visual object with Orbiter.
	 * \param hObj handle of the object to register the visual with
	 * \param vis identifier for the visual (passed to the message callback function)
	 * \note When the client creates a visual for an orbiter object (such
	 *   as vessels and planets), it must register them with the core by
	 *   calling RegisterVisObject. This will allow the visual to receive
	 *   event notifications via clbkVisEvent.
	 * \note Visuals should not be persistent, but should be created when an
	 *   object comes into visual range of an observer camera, and deleted when
	 *   the object moves out of visual range.
	 * \note If a client supports multiple views, it should not register visuals
	 *   for an object in each view, but only once when the object is rendered in
	 *   any of the views, and unregister when the object is no longer rendered in
	 *   any of the views.
	 * \note \e vis should be a nonzero handle that allows the client to
	 *   uniquely identify the visual (e.g. a pointer to a client-specific visual
	 *   object instance). The handle is passed to the clbkVisEvent
	 *   method, and also to any VESSEL methods that use VISHANDLEs.
	 * \note For vessel visuals, RegisterVisObject will trigger a
	 *   \ref VESSEL2::clbkVisualCreated notification to the vessel module,
	 *   if it exists.
	 * \sa UnregisterVisObject, clbkVisEvent, clbkVisualCreated
	 */
	void RegisterVisObject (OBJHANDLE hObj, VISHANDLE vis);

	/**
	 * \brief Unregister a visual before deleting it.
	 * \param hObj handle of the object for which the visual is un-registered.
	 * \note Before the client deletes a visual (e.g. when it runs out of
	 *   the camera visual range) it must unregister it from the core.
	 * \note Once the visual is un-registered, Orbiter will no longer generate
	 *   visual events via clbkVisEvent for it.
	 * \note For vessel visuals, UnregisterVisObject will trigger a
	 *   \ref VESSEL2::clbkVisualDestroyed notification to the vessel module,
	 *   if it exists.
	 * \sa RegisterVisObject, clbkVisEvent
	 */
	void UnregisterVisObject (OBJHANDLE hObj);

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
	virtual int clbkVisEvent (OBJHANDLE hObj, VISHANDLE vis, DWORD msg, DWORD_PTR context);
	virtual void clbkSetVisualProperty(VISHANDLE vis, VisualProp prp, int idx, const type_info& t, const void* val) { }

	/**
	 * \brief Return a mesh handle for a visual, defined by its index
	 * \param vis visual identifier
	 * \param idx mesh index (>= 0)
	 * \return Mesh handle (client-specific)
	 * \note Derived clients should return a handle that identifies a
	 *   mesh for the visual (in client-specific format).
	 * \note Orbiter calls this method in response to a \ref VESSEL::GetMesh
	 *   call by an vessel module.
	 */
	virtual MESHHANDLE clbkGetMesh (VISHANDLE vis, UINT idx) { return NULL; }

	/**
	 * \brief Mesh group data retrieval interface for device-specific meshes.
	 * \param hMesh device mesh handle
	 * \param grpidx mesh group index (>= 0)
	 * \param grs data buffers and buffer size information. See \ref oapiGetMeshGroup
	 *    for details.
	 * \return Should return 0 on success, or error flags > 0.
	 * \default None, returns -2.
	 */
	virtual int clbkGetMeshGroup (DEVMESHHANDLE hMesh, DWORD grpidx, GROUPREQUESTSPEC *grs) { return -2; }

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
	virtual int clbkEditMeshGroup (DEVMESHHANDLE hMesh, DWORD grpidx, GROUPEDITSPEC *ges) { return -2; }
	//@}

	// ==================================================================
	/// \name Dialog interface
	//@{
	/**
	 * \brief Popup window open notification.
	 * \note This method is called just before a popup window (e.g. dialog
	 *   box) is opened. It allows the client to prepare for subsequent
	 *   rendering of the window, if necessary.
	 */
	virtual void clbkPreOpenPopup () {}

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
	virtual ParticleStream *clbkCreateParticleStream (PARTICLESTREAMSPEC *pss);

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
	virtual ParticleStream *clbkCreateExhaustStream (PARTICLESTREAMSPEC *pss,
		OBJHANDLE hVessel, const double *lvl, const VECTOR3 *ref, const VECTOR3 *dir);

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
	virtual ParticleStream *clbkCreateExhaustStream (PARTICLESTREAMSPEC *pss,
		OBJHANDLE hVessel, const double *lvl, const VECTOR3 &ref, const VECTOR3 &dir);

	/**
	 * \brief Create a vessel particle stream for reentry heating effect
	 * \param pss particle stream parameters
	 * \param hVessel vessel handle
	 * \return Pointer to new particle stream
	 * \default None, returns NULL. Derived classes should overload this method
	 *   to return a ParticleStream-derived class instance in order to support
	 *   reentry streams.
	 */
	virtual ParticleStream *clbkCreateReentryStream (PARTICLESTREAMSPEC *pss,
		OBJHANDLE hVessel);
	// @}

	/**
	 * \brief Create an annotation object for displaying on-screen text.
	 * \return Pointer to new screen annotation object.
	 * \default Dynamically allocates a 'ScreenAnnotation' instance and returns
	 *   a pointer to it.
	 */
	virtual ScreenAnnotation *clbkCreateAnnotation ();

	/**
	 * \brief Returns the handle of the main render window.
	 */
	HWND GetRenderWindow () const { return hRenderWnd; }

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
	virtual LRESULT RenderWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

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
	virtual INT_PTR LaunchpadVideoWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	/**
	 * \brief Structure containing default video options, as stored in
	 *   Orbiter.cfg.
	 */
	struct VIDEODATA {
		bool fullscreen; ///< fullscreen mode flag
		bool forceenum;  ///< enforce device enumeration flag
		bool trystencil; ///< stencil buffer flag
		bool novsync;    ///< no vsync flag
		bool pageflip;   ///< allow page flipping in fullscreen
		int deviceidx;   ///< video device (adapter) index
		int modeidx;     ///< video mode index (fullscreen resolution)
		int winw;        ///< window/screen width
		int winh;        ///< window/screen height
		int outputidx;	 ///< video output
		int style;		 ///< true fullscreen, fulscreen window, window with taskbar
	};

	/**
	 * \brief Returns a pointer to the VideoData structure.
	 *
	 * This structure contains the user selection for video parameters as
	 * stored in the Orbiter.cfg file. You can use this structure to retrieve
	 * and present video options to the user, or ignore it and define your own
	 * method (e.g. reading/writing to a separate config file)
	 * \return pointer to VIDEODATA structure containing default video settings
	 */
	inline VIDEODATA *GetVideoData() { return &VideoData; }

	/**
	 * \brief returns a list of popup windows owned by the render window.
	 * \param [out] hPopupWnd on exit, points to a list of window handles
	 * \return Number of entries in the list.
	 * \note The list returned by this method contains the handles of 
	 *   popup windows that are to be rendered on top of the render viewport
	 *   (e.g. dialog boxes).
	 * \note A client can use this list if it requires a special method of
	 *   displaying the popup windows. Typically, this is the case in fullscreen
	 *   render modes, where the dialog contents may need to be blitted manually
	 *   into the render surface.
	 */
	DWORD GetPopupList (const HWND **hPopupWnd) const;

	/**
	 * \brief Fullscreen mode flag
	 * \return true if the client is set up for running in fullscreen
	 *   mode, false for windowed mode.
	 */
	virtual bool clbkFullscreenMode () const = 0;

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
	virtual void clbkGetViewportSize (DWORD *width, DWORD *height) const = 0;

	/**
	 * \brief Returns a specific render parameter
	 * \param[in] prm parameter identifier (see \sa renderprm)
	 * \param[out] value value of the queried parameter
	 * \return true if the specified parameter is supported by the client,
	 *    false if not.
	 */
	virtual bool clbkGetRenderParam (DWORD prm, DWORD *value) const = 0;

	/**
	 * \brief Returns a pointer to an Orbiter configuration parameter.
	 *
	 * This function can be used to access various configuration parameters
	 * defined in the Orbiter core (e.g. user selections in the Launchpad
	 * dialog box).
	 * \param paramtype Parameter identifier (see \ref cfgprm)
	 * \return Pointer to parameter
	 * \note The pointer must be cast into the appropriate variable type.
	 *   The variable types can be found in the parameter type list (\ref
	 *   cfgprm).
	 * \par Example:
	 * \code
	 * double lightscale = *(double*)GetConfigParam (CFGPRM_SURFACELIGHTBRT);
	 * \endcode
	 */
	const void *GetConfigParam (DWORD paramtype) const;

	/**
	 * \brief Return the full path for a texture file.
	 *
	 * Returns the fully qualified path for texture file 'fname' in
	 * 'path', relative to the orbiter root directory.
	 * The search method conforms to the standard orbiter convention (first
	 * search under Textures2, then under Textures directory)
	 * Example: for fname="mypath\\tex1.dds", this may return
	 * ".\Textures2\mypath\tex1.dds" or ".\Textures\mypath\tex1.dds"
	 * Return value is false if no file is found in either directory
	 * \param fname texture file name (with path relative to an Orbiter
	 *   texture directory)
	 * \param path string into which the full path is copied
	 * \return true if file was found, false otherwise.
	 */
	bool TexturePath (const char *fname, char *path) const;

	/**
	* \brief Return full path for a v2016-style planetary texture directory
	* \param planetname Planet name
	* \param path path string into which the full path is copied
	* \return true if path was found, false otherwise
	*/
	bool PlanetTexturePath(const char* planetname, char* path) const;

	/**
	 * \brief Returns the surface containing the virtual cockpit HUD
	 * \param[out] hudspec pointer to structure containing mesh and group index,
	 *   and size parameters of VC HUD object
	 * \return HUD surface handle, or NULL if not available
	 */
	SURFHANDLE GetVCHUDSurface (const VCHUDSPEC **hudspec) const;

	/**
	 * \brief Returns the surface containing an MFD display
	 * \param mfd MFD identifier (0 <= mfd < MAXMFD)
	 * \return MFD display surface handle, or NULL if not available
	 */
	SURFHANDLE GetMFDSurface (int mfd) const;

	/**
	 * \brief Returns the surface containing a virtual cockpit MFD display
	 * \param[in] mfd MFD identifier (0 <= mfd < MAXMFD)
	 * \param[out] mfdspec pointer to structure containing mesh and group index
	 *   of the VC MFD display object
	 * \return MFD display surface handle, or NULL if not available
	 */
	SURFHANDLE GetVCMFDSurface (int mfd, const VCMFDSPEC **mfdspec) const;

	/**
	 * \brief Returns a list of high-res surface tile specifications for a base.
	 * \param hBase surface base handle
	 * \param tile pointer to a list of tile specifications, or NULL if none defined
	 * \return number of surface tiles defined for the base
	 */
	DWORD GetBaseTileList (OBJHANDLE hBase, const SurftileSpec **tile) const;

	/**
	 * \brief Returns meshes for generic base objects
	 * \param hBase surface base handle
	 * \param mesh_bs mesh list for objects rendered before shadows (NULL if none)
	 * \param nmesh_bs list length of mesh_bs list
	 * \param mesh_as mesh list for objects rendered after shadows (NULL if none)
	 * \param nmesh_as list length of mesh_as list
	 * \note The lists contain mesh objects as well as generic object primitives
	 *   (blocks, tanks, hangars, etc.)
	 * \note All generic objects are separated into objects rendered before and
	 *   after shadows, and compressed into one mesh each, such that all objects with
	 *   the same textures are merged into a single group.
	 */
	void GetBaseStructures (OBJHANDLE hBase, MESHHANDLE **mesh_bs, DWORD *nmesh_bs, MESHHANDLE **mesh_as, DWORD *nmesh_as) const;

	/**
	 * \brief Returns base meshes in a format that can be used for shadow projections.
	 * \param hBase surface base handle
	 * \param mesh_sh list of base object meshes
	 * \param elev list of object elevation references [m]
	 * \param nmesh_sh length of mesh_sh list
	 * \note This method returns the mesh geometry (without textures and materials) for
	 *   all mesh objects rendered \e after shadows. Unlike GetBaseStructures(), this
	 *   does not merge mesh groups from different objects, so shadow projections can
	 *   be calculated on a per-object basis (onto the local horizon plane).
	 * \note the \e elev list is filled with elevation offsets of each object from the
	 *   reference plane of the base.
	 */
	void GetBaseShadowGeometry (OBJHANDLE hBase, MESHHANDLE **mesh_sh, double **elev, DWORD *nmesh_sh) const;

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
	virtual void clbkRender2DPanel (SURFHANDLE *hSurf, MESHHANDLE hMesh, MATRIX3 *T, bool additive = false);

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
	virtual void clbkRender2DPanel (SURFHANDLE *hSurf, MESHHANDLE hMesh, MATRIX3 *T, float alpha, bool additive = false);

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
	virtual SURFHANDLE clbkCreateSurfaceEx (DWORD w, DWORD h, DWORD attrib)
	{ return NULL; }

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
	virtual SURFHANDLE clbkCreateSurface (DWORD w, DWORD h, SURFHANDLE hTemplate = NULL)
	{ return NULL; }

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
	virtual SURFHANDLE clbkCreateTexture (DWORD w, DWORD h) { return NULL; }

	/**
	 * \brief Create an offscreen surface from a bitmap
	 * \param hBmp bitmap handle
	 * \return surface handle, or NULL to indicate failure
	 * \default Creates a surface of the same size as the bitmap, and
	 *   uses clbkCopyBitmap to copy the bitmap over.
	 * \note The reference counter for the new surface is set to 1.
	 * \sa clbkIncrSurfaceRef, clbkReleaseSurface
	 */
	virtual SURFHANDLE clbkCreateSurface (HBITMAP hBmp);

	/**
	 * \brief Increment the reference counter of a surface.
	 * \param surf surface handle
	 * \default None.
	 * \note Derived classes should keep track on surface references, and
	 *   overload this function to increment the reference counter.
	 */
	virtual void clbkIncrSurfaceRef (SURFHANDLE surf) {}

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
	virtual bool clbkReleaseSurface (SURFHANDLE surf) { return false; }

	/**
	 * \brief Return the width and height of a surface
	 * \param[in] surf surface handle
	 * \param[out] w surface width
	 * \param[out] h surface height
	 * \return true if surface dimensions could be obtained.
	 * \default Sets w and h to 0 and returns false.
	 * \sa clbkCreateSurface
	 */
	virtual bool clbkGetSurfaceSize (SURFHANDLE surf, DWORD *w, DWORD *h)
	{ *w = *h = 0; return false; }

	/**
	 * \brief Set transparency colour key for a surface.
	 * \param surf surface handle
	 * \param ckey transparency colour key value
	 * \default None, returns false.
	 * \note Derived classes should overload this method if the renderer
	 *   supports colour key transparency for surfaces.
	 */
	virtual bool clbkSetSurfaceColourKey (SURFHANDLE surf, DWORD ckey) { return false; }

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
	virtual DWORD clbkGetDeviceColour (BYTE r, BYTE g, BYTE b)
	{ return ((DWORD)r << 16) + ((DWORD)g << 8) + (DWORD)b; }
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
	virtual bool clbkBlt (SURFHANDLE tgt, DWORD tgtx, DWORD tgty, SURFHANDLE src, DWORD flag = 0) const
	{ return false; }

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
	virtual bool clbkBlt (SURFHANDLE tgt, DWORD tgtx, DWORD tgty, SURFHANDLE src, DWORD srcx, DWORD srcy, DWORD w, DWORD h, DWORD flag = 0) const
	{ return false; }

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
	virtual bool clbkScaleBlt (SURFHANDLE tgt, DWORD tgtx, DWORD tgty, DWORD tgtw, DWORD tgth,
		                       SURFHANDLE src, DWORD srcx, DWORD srcy, DWORD srcw, DWORD srch, DWORD flag = 0) const
	{ return false; }

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
	virtual int clbkBeginBltGroup (SURFHANDLE tgt);

	/**
	 * \brief Ends a block of blitting operations to the same target surface.
	 * \return Should return an error code (0 on success, the return value from
	 *   the base class call, or a client-specific code)
	 * \default If surfBltTgt was set, clears it (to RENDERTGT_NONE) and returns 0.
	 *   Otherwise returns -2;
	 * \sa clbkBeginBltGroup
	 */
	virtual int clbkEndBltGroup ();

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
	virtual bool clbkFillSurface (SURFHANDLE surf, DWORD col) const
	{ return false; }

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
	virtual bool clbkFillSurface (SURFHANDLE surf, DWORD tgtx, DWORD tgty, DWORD w, DWORD h, DWORD col) const
	{ return false; }

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
	virtual bool clbkCopyBitmap (SURFHANDLE pdds, HBITMAP hbm, int x, int y, int dx, int dy);
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
	virtual Sketchpad *clbkGetSketchpad (SURFHANDLE surf) { return NULL; }

	/**
	 * \brief Release a drawing object.
	 * \param sp pointer to drawing object
	 * \default None.
	 * \sa Sketchpad, clbkGetSketchpad
	 */
	virtual void clbkReleaseSketchpad (Sketchpad *sp) {}

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
	virtual Font *clbkCreateFont (int height, bool prop, const char *face, FontStyle style = FontStyle::FONT_NORMAL, int orientation = 0) const { return NULL; }
	virtual Font* clbkCreateFontEx (int height, char* face, int width = 0, int weight = 400, FontStyle style = FontStyle::FONT_NORMAL, float spacing = 0.0f) const { return NULL; }
		
	/**
	 * \brief De-allocate a font resource.
	 * \param font pointer to font resource
	 * \default None.
	 * \sa clbkCreateFont, oapi::Font
	 */
	virtual void clbkReleaseFont (Font *font) const {}

	/**
	 * \brief Create a pen resource for 2-D drawing.
	 * \param style line style (0=invisible, 1=solid, 2=dashed)
	 * \param width line width [pixel]
	 * \param col line colour (format: 0xBBGGRR)
	 * \return Pointer to pen resource
	 * \default None, returns NULL.
	 * \sa clbkReleasePen, oapi::Pen
	 */
	virtual Pen *clbkCreatePen (int style, int width, DWORD col) const { return NULL; }

	/**
	 * \brief De-allocate a pen resource.
	 * \param pen pointer to pen resource
	 * \default None.
	 * \sa clbkCreatePen, oapi::Pen
	 */
	virtual void clbkReleasePen (Pen *pen) const {}

	/**
	 * \brief Create a brush resource for 2-D drawing.
	 * \param col line colour (format: 0xBBGGRR)
	 * \return Pointer to brush resource
	 * \default None, returns NULL.
	 * \sa clbkReleaseBrush, oapi::Brush
	 */
	virtual Brush *clbkCreateBrush (DWORD col) const { return NULL; }

	/**
	 * \brief De-allocate a brush resource.
	 * \param brush pointer to brush resource
	 * \default None.
	 * \sa clbkCreateBrush, oapi::Brush
	 */
	virtual void clbkReleaseBrush (Brush *brush) const {}
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
	virtual HDC clbkGetSurfaceDC (SURFHANDLE surf) { return NULL; }

	/**
	 * \brief Release a Windows graphics device interface
	 * \param surf surface handle
	 * \param hDC GDI handle
	 * \default None.
	 * \note Clients which can obtain a Windows GDI handle for a surface should
	 *   overload this method to release an existing GDI.
	 * \todo This method should be moved into the GDIClient class
	 */
	virtual void clbkReleaseSurfaceDC (SURFHANDLE surf, HDC hDC) {}
	// @}

	/**
	 * \brief Constructs a synthetic elevation grid for a tile by interpolating 
	 *   ancestor elevation data
	 * \param emgr elevation manager handle (retrieve with oapiElevationManager)
	 * \param [in] ilat patch latitude index
	 * \param [in] ilng patch longitude index
	 * \param [in] lvl patch resolution level
	 * \param [in] pilat ancestor latitude index
	 * \param [in] pilng ancestor longitude index
	 * \param [in] plvl ancestor resolution level
	 * \param [in] pelev pointer to ancestor elevation grid data
	 * \param [out] elev pointer to array receiving interplated elevation grid data
	 * \param [out] emean pointer to variable receiving mean elevation data
	 * \note This function is used by the surface managers to construct an elevation
	 *   grid for a tile for which no native elevation data are available.
	 * \note The specified ancestor must be an actual ancestor of the destination tile,
	 *   i.e. the area of the destination tile must be contained in the ancestor area.
	 */
	bool ElevationGrid (ELEVHANDLE emgr, int ilat, int ilng, int lvl,
		int pilat, int pilng, int plvl, INT16 *pelev, float *elev, double *emean=0) const;

	bool ElevationGrid(ELEVHANDLE emgr, int ilat, int ilng, int lvl,
		int pilat, int pilng, int plvl, INT16* pelev, INT16* elev, double* emean = 0) const;

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
	virtual bool clbkFilterElevation(OBJHANDLE hPlanet, int ilat, int ilng, int lvl, double elev_res, INT16* elev) { return false; }
	// @}

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
	virtual bool clbkUseLaunchpadVideoTab () const { return true; }

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
	virtual HWND clbkCreateRenderWindow ();

	/**
	 * \brief Simulation startup finalisation
	 *
	 * Called at the beginning of a simulation session after the scenarion has
	 * been parsed and the logical object have been created.
	 * \default None
	 */
	virtual void clbkPostCreation () {}

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
	virtual void clbkCloseSession (bool fastclose) {}

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
	virtual void clbkDestroyRenderWindow (bool fastclose);

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
	virtual void clbkUpdate (bool running) {}

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
	virtual void clbkRenderScene () = 0;

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
	virtual bool clbkDisplayFrame () { return false; }

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
	virtual bool clbkSplashLoadMsg (const char *msg, int line) { return false; }

	/**
	 * \brief Change the default splash screen
	 * 
	 * Called before clbkCreateRenderWindow to override the default splash screen
	 * image and text color.
	 *
	 * \param fname File containing the splashscreen (jpg/bmp)
	 * \param textCol text color
	 */
	virtual void clbkSetSplashScreen(const char *fname, DWORD textCol) {}

	/**
	 * \brief Notifies Orbiter to to initiate rendering of the 2D scene overlay
	 *
	 * The 2D overlay is used to render 2D instrument panels, HUD, the info
	 * boxes at the top left and right of the screen, etc. This function should
	 * typically be called at the end of \ref clbkRenderScene, after the 3D
	 * scene has been rendered, but before the rendering environment is
	 * released. During the execution of this function, Orbiter will call the
	 * \ref clbkRender2DPanel function several times to allow the client to
	 * build up the 2D layer.
	 * \note Orbiter will \e not acquire a Sketchpad environment while
	 *  executing this function, because the graphics driver may not allow to
	 *  lock surfaces for drawing while in render mode. If a Sketchpad environment
	 *  is required to draw on top of the render window (for example for displaying
	 *  specific HUD elements), it is acquired after clbkRenderScene returns.
	 * \sa clbkRenderScene, clbkRender2DPanel
	 */
	void Render2DOverlay ();

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
	virtual void clbkStoreMeshPersistent (MESHHANDLE hMesh, const char *fname) {}

	/**
	 * \brief Displays the default Orbiter splash screen on top of
	 *   the render window.
	 */
	void ShowDefaultSplash ();

	/**
	 * \brief Write a block of raw image data to a formatted image file.
	 * \param data image specification structure
	 * \param fname output file name (relative to orbiter root directory)
	 * \param fmt output format
	 * \param quality requested image quality, if supported by the format
	 * \return \e true on success, \e false if inconsistencies in the image
	 *   specifications were detected (see notes)
	 * \note The following limitations to the provided image data currently
	 *  apply:
	 *  - data.bpp must be 24
	 *  - data.stride must be aligned to 4 bytes, i.e. (data.width * data.bpp + 31) & ~31) >> 3
	 *  - data.bufsize must be >= data.stride * data.height
	 * \sa ImageData, ImageFileFormat
	 */
	bool WriteImageDataToFile (const ImageData &data,
		const char *fname, ImageFileFormat fmt=IMAGE_JPG, float quality=0.7f);

	/**
	 * \brief Read an image from a memory buffer
	 * \param pBuf pointer to memory buffer
	 * \param nBuf size of memory buffer
	 * \param w width of image after scaling (0 to keep original width)
	 * \param h height of image after scaling (0 to keep original height)
	 * \note This function automatically recognises different image formats
	 *   in the memory buffer (bmp, jpg, png, tif)
	 * \note This function can be used to read in an image from a resource
	 *   stored in the executable file (see Windows API functions
	 *   LoadResource, LockResource, SizeofResource)
	 * \sa ReadImageFromFile, WriteImageDataToFile
	 */
	HBITMAP ReadImageFromMemory (BYTE *pBuf, DWORD nBuf, UINT w, UINT h);

	/**
	 * \brief Read an image from a file into a bitmap
	 * \param fname file name
	 * \param fmt image format
	 * \param w width of image after scaling (0 to keep original width)
	 * \param h height of image after scaling (0 to keep original height)
	 * \note This function can read different image formats (bmp, jpg, png, tif)
	 * \sa ReadImageFromMemory, WriteImageDataToFile
	 */
	HBITMAP ReadImageFromFile (const char *fname, UINT w=0, UINT h=0);

	/**
	 * \brief Returns the graphics module instance handle
	 */
	inline HINSTANCE ModuleInstance () const { return hModule; }

	/**
	 * \brief Returns the orbiter core instance handle
	 */
	inline HINSTANCE OrbiterInstance () const { return hOrbiterInst; }

	/**
	 * \brief Returns the window handle of the 'video' tab of the Orbiter
	 *   Launchpad dialog.
	 *
	 * If clbkUseLanuchpadVideoTab() is overloaded to return false, this
	 * function will return NULL.
	 */
	HWND LaunchpadVideoTab() const { return hVid; }

	// ==================================================================
	// Functions for the celestial sphere
public:
	struct ConstLabelRec {
		std::string fullLabel;
		std::string abbrLabel;
		float lngCnt;
		float latCnt;
	};

	//struct ConstLabelRenderRec {
	//	std::string fullLabel;
	//	std::string abbrLabel;
	//	VECTOR3 pos;
	//};

	// ==================================================================
	/// \name Marker and label-related methods
	// @{
	struct LABELSPEC {
		VECTOR3 pos;
		std::string label[2];
	};
	/**
	 * \brief Label list description for celestial and surface markers
	 */
	struct LABELLIST {
		std::string name; ///< list name
		std::vector< LABELSPEC> marker; ///< list of markers
		int colour;       ///< marker colour index (0-5)
		int shape;        ///< marker shape index (0-4)
		float size;       ///< marker size factor
		float distfac;    ///< marker distance cutout factor
		DWORD flag;       ///< reserved
		bool active;      ///< active list flag
	};

	/**
	 * \brief Returns an array of celestial marker lists
	 * \param cm_list array of marker lists
	 * \return number of lists in the array
	 * \sa LABELLIST
	 */
	const std::vector<LABELLIST>& GetCelestialMarkers() const;

	/**
	 * \brief Returns an array of surface marker lists for a planet
	 * \param hObj planet handle
	 * \param sm_list array of marker lists
	 * \return number of lists in the array
	 * \note hObj must refer to a planet or moon. Other objects are not supported.
	 * \sa LABELLIST
	 */
	DWORD GetSurfaceMarkers (OBJHANDLE hObj, const LABELLIST **sm_list) const;

	struct LABELTYPE {
		char labelId;    // label type id
		char markerId;   // marker shape id
		COLORREF col;    // label colour
		char *name;      // label type name
		bool active;     // label type active?
	};

	/**
	 * \brief Returns a surface marker legend (label version 2) for a planet
	 * \param hObj planet handle
	 * \param lspec array of label type specs
	 * \return length of lspec list
	 */
	DWORD GetSurfaceMarkerLegend (OBJHANDLE hObj, const LABELTYPE **lspec) const;
	// @}

	HWND hVid;              ///< Window handle of Launchpad video tab, if available

protected:
	SURFHANDLE surfBltTgt;  ///< target surface for a blitting group (-1=none, NULL=main window render surface)
	oapi::Font *splashFont; // font for splash screen displays

private:
	/**
	 * \brief Render window initialisation
	 *
	 * - Sets the viewW and viewH values
	 * - Sets the GWLP_USERDATA data of the render window to *this
	 * \param hWnd Render window handle. If this is NULL, InitRenderWnd
	 *   will create a dummy window and return its handle.
	 * \return Render window handle
	 * \note This is called after clbkCreateRenderWindow returns.
	 */
	HWND InitRenderWnd (HWND hWnd);

	HWND hRenderWnd;        // render window handle
	HINSTANCE hOrbiterInst; // orbiter core instance handle
	VIDEODATA VideoData;    // the standard video options from config

	IWICImagingFactory *m_pIWICFactory; // Windows Image Component factory instance
};


// ======================================================================
// class ParticleStream
// ======================================================================
/**
 * \brief Defines an array of "particles" (e.g. for exhaust and reentry
 *   effects, gas venting, condensation, etc.)
 *
 * Each particle is represented by a "billboard" object facing the camera
 * and rendered with a semi-transparent texture.
 *
 * Particle streams experience drag in atmosphere. They also can cast
 * shadows on the ground.
 */
class OAPIFUNC ParticleStream {
	friend class GraphicsClient;

public:
	/**
	 * \brief Constructs a new particle stream
	 * \param _gc pointer to graphics client
	 * \param pss particle parameter set
	 * \note The particle stream will only start to generate particles once it
	 *   has been attached to an object with Attach().
	 */
	ParticleStream (GraphicsClient *_gc, PARTICLESTREAMSPEC *pss);

	/**
	 * \brief Destructor
	 */
	~ParticleStream ();

	/**
	 * \brief Attach the stream to an object
	 * \param hObj object handle
	 * \param ppos pointer to particle source point (object frame)
	 * \param pdir pointer to particle direction (object frame)
	 * \param srclvl pointer to particle generator level
	 * \note This method uses pointers to externally defined position and
	 *   direction variables which may be modified by orbiter during the
	 *   lifetime of the particle stream.
	 */
	void Attach (OBJHANDLE hObj, const VECTOR3 *ppos, const VECTOR3 *pdir, const double *srclvl);

	/**
	 * \brief Attach the stream to an object
	 * \param hObj object handle
	 * \param _pos particle source point (object frame)
	 * \param _dir particle direction (object frame)
	 * \param srclvl pointer to particle generator level
	 * \note This method uses fixed position and direction variables.
	 */
	void Attach (OBJHANDLE hObj, const VECTOR3 &_pos, const VECTOR3 &_dir, const double *srclvl);

	/**
	 * \brief Detach the stream from its object
	 * \note After detaching the stream, no new particles will be generated,
	 *   but the existing particle will persist to the end of their lifetime.
	 */
	void Detach ();

	/**
	 * \brief Reset the particle source point to a constant value.
	 * \param _pos particle source point (reference object frame)
	 * \note This method overrides any previous fixed or variable source position.
	 */
	void SetFixedPos (const VECTOR3 &_pos);

	/**
	 * \brief Reset the particle source direction to a constant value.
	 * \param _dir particle direction (reference object frame)
	 * \note This method overrides any previous fixed or variable source direction.
	 */
	void SetFixedDir (const VECTOR3 &_dir);

	/**
	 * \brief Reset the particle source point reference
	 * \param ppos pointer to particle source point
	 * \note This method overrides the previous position reference and any
	 *   constant position value.
	 */
	void SetVariablePos (const VECTOR3 *ppos);

	/**
	 * \brief Reset the particle source direction reference
	 * \param pdir pointer to particle source direction
	 * \note This method overrides the previous direction reference and any
	 *   constant direction value.
	 */
	void SetVariableDir (const VECTOR3 *pdir);

	/**
	 * \brief Reset the particle generator strength reference
	 * \param srclvl pointer to particle generator strength (0...1)
	 * \note The generator strength affects the initial opacity of generated
	 *   particles.
	 * \note This method overrides the previous strength reference.
	 */
	void SetLevelPtr (const double *srclvl);

	/**
	 * \brief Returns the particle generator level
	 * \return pointer to particle generator level (0...1)
	 */
	inline const double *Level() const { return level; }

protected:
	GraphicsClient *gc;   // client
	const double *level;  // pointer to source level variable
	OBJHANDLE hRef;       // reference object
	const VECTOR3 *pos;   // source position pointer
	const VECTOR3 *dir;   // source direction pointer
	VECTOR3 lpos;         // local source position
	VECTOR3 ldir;         // local source direction
};


// ======================================================================
// class ScreenAnnotation
// ======================================================================
/**
 * \brief Defines a block of text displayed on top of the simulation render
 *   window.
 */
class ScreenAnnotation {
public:
	/**
	 * \brief Constructs a new annotation object
	 * \param _gc pointer to graphics client
	 */
	ScreenAnnotation (GraphicsClient *_gc);

	/**
	 * \brief Destroys the annotation object
	 */
	virtual ~ScreenAnnotation();

	/**
	 * \brief Resets annotation parameters to their default values
	 */
	virtual void Reset ();

	/**
	 * \brief Set the text to be displayed by the annotation object
	 * \param str character string
	 */
	virtual void SetText (char *str);

	/**
	 * \brief Clear the text display
	 */
	virtual void ClearText ();

	/**
	 * \brief Set the bounding box of the annotation block
	 * \param x1 left edge
	 * \param y1 top edge
	 * \param x2 right edge
	 * \param y2 bottom edge
	 * \note The positions are relative to the boundaries of the render window,
	 *   in the range 0 to 1, where (0,0) is the top left edge, and (1,1) is the
	 *   bottom right edge of the render window.
	 */
	virtual void SetPosition (double x1, double y1, double x2, double y2);

	/**
	 * \brief Set the font size
	 * \param scale font size parameter (>0)
	 * \note scale=1 defines the default font size, scale<1 is a smaller font,
	 *   and scale>1 a larger font.
	 * \note The default font size is automatically scaled with the size of the
	 *   render window.
	 */
	virtual void SetSize (double scale);

	/**
	 * \brief Set the font colour
	 * \param col RGB values of the font colour (0..1 in each component)
	 */
	virtual void SetColour (const VECTOR3 &col);

	/**
	 * \brief Render the annotation text into the simulation window.
	 */
	virtual void Render ();

private:
	GraphicsClient *gc;
	DWORD viewW, viewH;
	int nw, nh, nx1, ny1, nx2, ny2, hf;
	int txtlen, buflen;
	char *txt;
	double txtscale;
	oapi::Font *font;
	COLORREF txtcol, txtcol2;
};

}; // namespace oapi


// ======================================================================
// API interface: register/unregister the graphics client

/**
 * \ingroup oapi
 * \brief Register graphics client class instance
 *
 * Graphics clients plugins should use this function to register the
 * class instance instead of oapiRegisterModule.
 * \param gc pointer to graphics client instance
 * \return true if client was registered successfully
 */
OAPIFUNC bool oapiRegisterGraphicsClient (oapi::GraphicsClient *gc);

OAPIFUNC bool oapiUnregisterGraphicsClient (oapi::GraphicsClient *gc);

#endif // !__GRAPHICSAPI_H
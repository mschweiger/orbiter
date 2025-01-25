// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
//                     ORBITER SOFTWARE DEVELOPMENT KIT
// OrbiterAPI.h
// ORBITER Application Programming Interface (OAPI)
// ======================================================================

/**
 * \file OrbiterAPI.h
 * \brief General API interface functions
 * \todo Check functions in VESSELSTATUS2::arot and oapiGetPlanetObliquityMatrix(), 
 *	minus sign has changed a place in a matrix. Is this correct??
 * \todo class CameraMode documentation 
*/

#ifndef __ORBITERAPI_H
#define __ORBITERAPI_H

#if defined(_MSC_VER) && (_MSC_VER >= 1300 ) // Microsoft Visual Studio Version 2003 and higher
#if !defined(_CRT_SECURE_NO_DEPRECATE)
#define _CRT_SECURE_NO_DEPRECATE 
#endif
#endif
#include <fstream>
#include <windows.h>
#include <float.h>
#include <math.h>
#include <vector>

#if defined(_MSC_VER) && (_MSC_VER < 1920 ) // Microsoft Visual Studio Version 2017 and lower
#include <algorithm>
#endif

extern "C" {
#include <lua/lua.h>
}

// Assumes MS VC++ compiler. Modify these statements for other compilers
#define DLLEXPORT __declspec(dllexport)
#define DLLIMPORT __declspec(dllimport)
#define DLLCLBK extern "C" __declspec(dllexport)

#ifdef OAPI_IMPLEMENTATION
#define OAPIFUNC DLLEXPORT
#else
#define OAPIFUNC DLLIMPORT
#endif

#pragma warning(disable: 4201)

// Message loop return type - maintain backward compatibility for 32-bit
#ifdef _WIN64
#define OAPI_MSGTYPE LRESULT
#else
#define OAPI_MSGTYPE int
#endif

// ======================================================================
/// \defgroup constants Some useful general constants
// ======================================================================
//@{
const double PI    = 3.14159265358979323846;	///< pi
const double PI05  = 1.57079632679489661923;	///< pi/2
const double PI2   = 6.28318530717958647693;	///< pi*2
const double RAD   = PI/180.0;      ///< factor to map degrees to radians
const double DEG   = 180.0/PI;      ///< factor to map radians to degrees
const double C0    = 299792458.0;   ///< speed of light in vacuum [m/s]
const double TAUA  = 499.004783806; ///< light time for 1 AU [s]
const double AU    = C0*TAUA;       ///< astronomical unit (mean geocentric distance of the sun) [m]
const double GGRAV = 6.67259e-11;   ///< gravitational constant [m^3 kg^-1 s^-2]
const double G     = 9.81;          ///< gravitational acceleration [m/s^2] at Earth mean radius
const double ATMP  = 101.4e3;       ///< atmospheric pressure [Pa] at Earth sea level
const double ATMD  = 1.293;         ///< atmospheric density [kg/m^3] at Earth sea level
//@}

// ======================================================================
// Some general utility functions
// ======================================================================
/**
 * \brief Returns the input argument normalised to range -pi ... pi
 * \param angle input angle [rad]
 * \return normalised angle [rad]
 */
inline double normangle (double angle)
{
	double a = fmod (angle, PI2);
	return (a >= PI ? a-PI2 : a < -PI ? a+PI2 : a);
}

/**
 * \brief Returns the input argument normalised to range 0 ... 2 pi
 * \param angle input angle [rad]
 * \return normalised angle [rad]
 */
inline double posangle (double angle)
{
	double a = fmod (angle, PI2);
	return (a >= 0.0 ? a : a+PI2);
}

/**
 * \brief Write a floating point value to a string
 * \param cbuf character buffer
 * \param n size of cbuf array
 * \param f floating point value
 * \param precision output precision
 * \note Formats the string in the standard Orbiter convention,
 *   with 'k', 'M', 'G' postfixes as required
 * \note cbuf must be allocated to sufficient size to hold the string
 */
OAPIFUNC void FormatValue (char *cbuf, int n, double f, int precision=4);

// ======================================================================
// API data types
// ======================================================================

class VESSEL;
class CELBODY;
class ExternMFD;
class Interpreter;

namespace oapi {
	class Module;
	class Sketchpad;
	class Font;
	class Pen;
	class Brush;
	union FVECTOR4;
}

// ======================================================================
/// \defgroup defines Defines and Enumerations
/// \defgroup structures Structure definitions
// ======================================================================


// ======================================================================
/// \ingroup defines
/// \defgroup handle Handles
// ======================================================================
//@{
/// \brief Handle for objects (vessels, stations, planets)
typedef void *OBJHANDLE;

/// \brief Handle for vessel superstructures
typedef void *SUPERVESSELHANDLE;

/// \brief Handle for visuals
typedef void *VISHANDLE;

/// \brief Handle for meshes
typedef void *MESHHANDLE;

/// \brief Handle for graphics-client-specific meshes
typedef int *DEVMESHHANDLE;
//struct DEVMESHHANDLE {
//	DEVMESHHANDLE() { hMesh = NULL; }
//	DEVMESHHANDLE(MESHHANDLE h) { hMesh = h; }
//	DWORD id;
//	MESHHANDLE hMesh;
//	operator int() { return (int)hMesh; }
//};

/// \brief Handle for bitmap surfaces and textures (panels and panel items)
typedef void *SURFHANDLE;

/// \brief Handle for 2D instrument panels
typedef void *PANELHANDLE;

/// \brief Handle for file streams
typedef void *FILEHANDLE;

/// \brief Handle for script interpreters
typedef void *INTERPRETERHANDLE;

/// \brief Handle for thrusters
typedef void *THRUSTER_HANDLE;

/// \brief Handle for logical thruster groups
typedef void *THGROUP_HANDLE;

/// \brief Propellant resource handle
typedef void *PROPELLANT_HANDLE;

/// \brief Handle for particle streams
typedef void *PSTREAM_HANDLE;

/// \brief Handle for vessel docking ports
typedef void *DOCKHANDLE;

/// \brief Handle vor vessel passive attachment points
typedef void *ATTACHMENTHANDLE;

/// \brief Handle for vessel airfoils
typedef void *AIRFOILHANDLE;

/// \brief Handle for vessel aerodynamic control surfaces
typedef void *CTRLSURFHANDLE;

/// \brief Handle for a navigation radio transmitter (VOR, ILS, IDS, XPDR)
typedef void *NAVHANDLE;

/// \brief Handle for animation components
typedef void *ANIMATIONCOMPONENT_HANDLE;

/// \brief Handle for custom items added to Launchpad "Extra" list
typedef void *LAUNCHPADITEM_HANDLE;

/// \brief Handle for onscreen annotation objects
typedef void *NOTEHANDLE;

/// \brief Handle for elevation query managers
typedef void *ELEVHANDLE;
//@}

typedef enum { FILE_IN, FILE_OUT, FILE_APP, FILE_IN_ZEROONFAIL } FileAccessMode;
typedef enum { ROOT, CONFIG, SCENARIOS, TEXTURES, TEXTURES2, MESHES, MODULES } PathRoot;

/// \defgroup surfid Identifiers for special render surfaces
/// @{
#define RENDERTGT_NONE ((SURFHANDLE)-1) ///< no surface
#define RENDERTGT_MAINWINDOW 0          ///< main render target
/// @}

// ===========================================================================
/**
 * \defgroup vec Vectors and matrices
 * Vectors and matrices are used to represent positions, velocities, translations,
 *   rotations, etc. in the 3-dimensional object space. Orbiter provides the
 *   %VECTOR3 and %MATRIX3 structures for 3-D vectors and matrices. A number
 *   of utility functions allow common operations such as matrix-vector
 *   products, dot and vector products, etc.
 */
// ===========================================================================
//@{
/**
 * \brief 3-element vector
 */
typedef union {
	double data[3];               ///< array data interface
	struct { double x, y, z; };   ///< named data interface
} VECTOR3;

/**
 * \brief 4-element vector
 */
typedef union {
	double data[4];                ///< array data interface
	struct { double x, y, z, w; }; ///< named data interface
} VECTOR4;

/**
 * \brief 3x3-element matrix
 */
typedef union {
	double data[9];               ///< array data interface (row-sorted)
	struct { double m11, m12, m13, m21, m22, m23, m31, m32, m33; }; ///< named data interface
} MATRIX3;

typedef union {      // 4x4 matrix
	double data[16];
	struct { double m11, m12, m13, m14, m21, m22, m23, m24, m31, m32, m33, m34, m41, m42, m43, m44; };
} MATRIX4;
//@}

/** 
 * \ingroup structures
 * \brief colour definition
 */
typedef struct {
	float r;    ///< read colour component [0..1]
	float g;    ///< green colour component [0..1]
	float b;    ///< blue colour component [0..1]
	float a;    ///< alpha (opacity) component (0..1)
} COLOUR4;

/**
 * \ingroup structures
 * \brief vertex definition including normals and texture coordinates 
 */
typedef struct {
	float x;     ///< vertex x position
	float y;     ///< vertex y position
	float z;     ///< vertex z position
	float nx;    ///< vertex x normal
	float ny;    ///< vertex y normal
	float nz;    ///< vertex z normal
	float tu;    ///< vertex u texture coordinate
	float tv;    ///< vertex v texture coordinate
} NTVERTEX;

/**
 * \ingroup structures
 * \brief Defines a mesh group (subset of a mesh).
 *
 * A mesh group contains a vertex list, an index list,
 * a material and texture index, and a set of flags.
 */
typedef struct {
	NTVERTEX *Vtx;     ///< vertex list
	WORD *Idx;         ///< index list
	DWORD nVtx;        ///< vertex count
	DWORD nIdx;        ///< index count
	DWORD MtrlIdx;     ///< material index (>= 1, 0=none)
	DWORD TexIdx;      ///< texture index (>= 1, 0=none)
	DWORD UsrFlag;     ///< user-defined flag
	WORD zBias;        ///< z bias
	WORD Flags;        ///< internal flags
} MESHGROUP;

const DWORD MAXTEX = 1;  // max. extra textures per mesh group

/**
 * \ingroup structures
 * \brief extended mesh group definition 
 */
typedef struct {
	NTVERTEX *Vtx;     ///< vertex list
	WORD *Idx;         ///< index list
	DWORD nVtx;        ///< vertex count
	DWORD nIdx;        ///< index count
	DWORD MtrlIdx;     ///< material index (>= 1, 0=none)
	DWORD TexIdx;      ///< texture index (>= 1, 0=none)
	DWORD UsrFlag;     ///< user-defined flag
	WORD zBias;        ///< z bias
	WORD Flags;        ///< internal flags
	DWORD TexIdxEx[MAXTEX]; ///< additional texture indices
	float TexMixEx[MAXTEX]; ///< texture mix values
} MESHGROUPEX;

/**
 * \ingroup defines
 * \defgroup surfacecaps Surface and texture attributes
 * These bitflags are used during texture creation or loading to
 * specify what they will be used for. This information is required
 * by the graphics clients to initialise and optimise the surfaces
 * accordingly.
 * \sa oapiCreateSurfaceEx, oapiLoadSurfaceEx
 */
//@{
#define OAPISURFACE_TEXTURE      0x0001 ///< Surface can be used as a texture (e.g. by associating it with a mesh)
#define OAPISURFACE_RENDERTARGET 0x0002 ///< Surface can be rendered to by the graphics device
#define OAPISURFACE_GDI          0x0004 ///< A HDC context can be requested from the surface for GDI drawing
#define OAPISURFACE_SKETCHPAD    0x0008 ///< A Sketchpad context can be requested from the surface for Sketchpad drawing
#define OAPISURFACE_MIPMAPS      0x0010 ///< Create a full chain of mipmaps for the surface. If loaded from file, add any missing mipmap levels
#define OAPISURFACE_NOMIPMAPS    0x0020 ///< Don't create mipmaps. If loaded from file, ignore any mipmap levels present
#define OAPISURFACE_ALPHA        0x0040 ///< Create an alpha channel for the surface. If loaded from file, add an alpha channel if required
#define OAPISURFACE_NOALPHA      0x0080 ///< Don't create an alpha channel. If loaded from file, strip any existing alpha channel
#define OAPISURFACE_UNCOMPRESS   0x0100 ///< Create an uncompressed surface. If loaded from file, uncompress if required.
#define OAPISURFACE_SYSMEM       0x0200 ///< Create the surface in system (host) memory
#define OAPISURFACE_RENDER3D     0x0400 ///< Create a surface that can act as a target for rendering a 3D scene
#define OAPISURFACE_ANTIALIAS    0x0800 ///< Create a surface with anti-aliasing the level will depend on launchpad settings.
#define OAPISURFACE_SHARED		 0x1000 ///< Create a shared resource
//@}

/**
 * \ingroup defines
 * \defgroup grpedit Mesh group editing flags
 * These constants can be applied to the \e flags field of the
 *   \ref GROUPEDITSPEC structure to define which parts of a
 *   mesh group are to be modified.
 * \note The GRPEDIT_SETUSERFLAG, GRPEDIT_ADDUSERFLAG and
 *   GRPEDIT_DELUSERFLAG flags are mutually exclusive. Only one
 *   can be used at a time.
 * \sa GROUPEDITSPEC, oapiEditMeshGroup
 */
//@{
#define GRPEDIT_SETUSERFLAG 0x00001 ///< replace the group's UsrFlag entry with the value in the GROUPEDITSPEC structure.
#define GRPEDIT_ADDUSERFLAG 0x00002 ///< Add the UsrFlag value to the group's UsrFlag entry
#define GRPEDIT_DELUSERFLAG 0x00004 ///< Remove the UsrFlag value from the group's UsrFlag entry
#define GRPEDIT_VTXCRDX     0x00008 ///< Replace vertex x-coordinates
#define GRPEDIT_VTXCRDY     0x00010 ///< Replace vertex y-coordinates
#define GRPEDIT_VTXCRDZ     0x00020 ///< Replace vertex z-coordinates
#define GRPEDIT_VTXCRD      (GRPEDIT_VTXCRDX | GRPEDIT_VTXCRDY | GRPEDIT_VTXCRDZ) ///< Replace vertex coordinates
#define GRPEDIT_VTXNMLX     0x00040 ///< Replace vertex x-normals
#define GRPEDIT_VTXNMLY     0x00080 ///< Replace vertex y-normals
#define GRPEDIT_VTXNMLZ     0x00100 ///< Replace vertex z-normals
#define GRPEDIT_VTXNML      (GRPEDIT_VTXNMLX | GRPEDIT_VTXNMLY | GRPEDIT_VTXNMLZ) ///< Replace vertex normals
#define GRPEDIT_VTXTEXU     0x00200 ///< Replace vertex texture u-coordinates
#define GRPEDIT_VTXTEXV     0x00400 ///< Replace vertex texture v-coordinates
#define GRPEDIT_VTXTEX      (GRPEDIT_VTXTEXU | GRPEDIT_VTXTEXV) ///< Replace vertex texture coordinates
#define GRPEDIT_VTX         (GRPEDIT_VTXCRD | GRPEDIT_VTXNML | GRPEDIT_VTXTEX) ///< Replace vertices

#define GRPEDIT_VTXCRDADDX  0x00800 ///< Add to vertex x-coordinates
#define GRPEDIT_VTXCRDADDY  0x01000 ///< Add to vertex y-coordinates
#define GRPEDIT_VTXCRDADDZ  0x02000 ///< Add to vertex z-coordinates
#define GRPEDIT_VTXCRDADD   (GRPEDIT_VTXCRDADDX | GRPEDIT_VTXCRDADDY | GRPEDIT_VTXCRDADDZ) ///< Add to vertex coordinates
#define GRPEDIT_VTXNMLADDX  0x04000 ///< Add to vertex x-normals
#define GRPEDIT_VTXNMLADDY  0x08000 ///< Add to vertex y-normals
#define GRPEDIT_VTXNMLADDZ  0x10000 ///< Add to vertex z-normals
#define GRPEDIT_VTXNMLADD   (GRPEDIT_VTXNMLADDX | GRPEDIT_VTXNMLADDY | GRPEDIT_VTXNMLADDZ) ///< Add to vertex normals
#define GRPEDIT_VTXTEXADDU  0x20000 ///< Add to vertex texture u-coordinates
#define GRPEDIT_VTXTEXADDV  0x40000 ///< Add to vertex texture v-coordinates
#define GRPEDIT_VTXTEXADD   (GRPEDIT_VTXTEXADDU | GRPEDIT_VTXTEXADDV) ///< Add to vertex texture coordinates
#define GRPEDIT_VTXADD      (GRPEDIT_VTXCRDADD | GRPEDIT_VTXNMLADD | GRPEDIT_VTXTEXADD)
#define GRPEDIT_VTXMOD      (GRPEDIT_VTX | GRPEDIT_VTXADD)
//@}

/**
 * \ingroup structures
 * \brief Structure used by \ref oapiEditMeshGroup to define the
 *   group elements to be replaced or modified.
 * \note Only the group elements specified in the \e flags entry will
 *   be replaced or modified. The elements that are to remain unchanged
 *   can be left undefined in the GROUPEDITSPEC structure. For example,
 *   if only GRPEDIT_VTXCRDX is specified, only the 'x' fields in the
 *   Vtx array need to be assigned.
 * \note to replace individual vertices in the group, the nVtx entry
 *   should contain the number of vertices to be replaced, the vIdx
 *   array should contain the indices (>= 0) of the vertices to be 
 *   replaced, and Vtx should contain the new vertex values of those
 *   vertices. If vIdx==NULL, vertices are replaced in sequence from
 *   the beginning of the group's vertex list.
 *   nVtx must be less or equal the number of vertices in the group.
 * \sa oapiEditMeshGroup, grpedit
 */
typedef struct {
	DWORD flags;   ///< flags (see \ref grpedit)
	DWORD UsrFlag; ///< Replacement for group UsrFlag entry
	NTVERTEX *Vtx; ///< Replacement for group vertices
	DWORD nVtx;    ///< Number of vertices to be replaced
	WORD *vIdx;    ///< Index list for vertices to be replaced
} GROUPEDITSPEC;

/**
 * \ingroup structures
 * \brief Structure used by \ref oapiGetMeshGroup containing data
 *    buffers to be filled with vertex and index data.
 */
typedef struct {
	NTVERTEX *Vtx;  ///< Vertex buffer
	DWORD nVtx;     ///< Number of vertices to return
	WORD *VtxPerm;  ///< Vertex permutation index list
	WORD *Idx;      ///< Triangle index buffer
	DWORD nIdx;     ///< Number of indices to return
	WORD *IdxPerm;  ///< Triangle permutation index list
	DWORD MtrlIdx;  ///< Material index
	DWORD TexIdx;   ///< Texture index
} GROUPREQUESTSPEC;

/**
 * \ingroup structures
 * \brief material definition 
 */
typedef struct {
	COLOUR4 diffuse;   ///< diffuse component
	COLOUR4 ambient;   ///< ambient component
	COLOUR4 specular;  ///< specular component
	COLOUR4 emissive;  ///< emissive component
	float power;       ///< specular power
} MATERIAL;


enum MatProp {
	Diffuse,			///< Material Diffuse color or Albedo depending on shader used. [.rgba]
	Ambient,			///< Ambient color or Ambien occlusion [.rgb]
	Specular,			///< Specular color [.rgb] power in [.a]
	Light,				///< DX7 Style emission (color is added in light that lits the diffuse texture) (i.e. light map) [.rgb]
	Emission,			///< Still in a development currently: output = max(diffuse*light, emission) [.rgb]
	Reflect,			///< Reflectivity [.rgb]
	Smooth,				///< Smoothness in [.r] (1 = smooth, 0 = rough)
	Metal,				///< Meralness in [.r]  (1 = metal, 0 = non-metal)
	Fresnel,			///< Fresnel terms for fresnel effect. Used in older 2nd generation shader. 
	SpecialFX			///< Heat map effect control variable in [.r] (i.e. average part temperature)
};

/**
 * \brief Format specifiers for image files
 */
namespace oapi {
	enum ImageFileFormat {
		IMAGE_BMP = 0,
		IMAGE_PNG = 1,
		IMAGE_JPG = 2,
		IMAGE_TIF = 3,
		IMAGE_DDS = 4	///< D3D9+ only
	};
}


/**
 * \brief Kepler orbital elements
 *
 * A set of 6 scalar parameters defining the state of an object in a 2-body
 * (Keplerian) orbit. The orbital trajectory is a conic section, either
 * closed (circular, elliptic), or open (parabolic, hyperbolic).
 * \note semi-major axis a is positive for closed orbits, and negative for
 *   open orbits (in that case, a is referred to as real semi-axis).
 * \note eccentricity e:
 *   - circular orbit: e = 0
 *   - elliptic orbit: 0 < e < 1
 *   - parabolic orbit: e = 1
 *   - hyperbolic orbit: e > 1
 * \note The a and e parameters define the shape of the orbit, the i, theta
 *   and omegab parameters define the orientation of the orbital plane in
 *   space, and the L parameter defines the object position along the
 *   trajectory at a given time.
 * \note This is a generic data format. Additional data are required to
 *   fully define an object's state in space (position and velocity
 *   vectors). These include the position of the orbited body, the
 *   orientation of the reference coordinate system, and the date to which
 *   the mean longitude parameter refers. 
 * \sa ORBITPARAM, \subpage orbit
 */
typedef struct {
	double a;          ///< semi-major axis [m]
	double e;          ///< eccentricity
	double i;          ///< inclination [rad]
	double theta;      ///< longitude of ascending node [rad]
	double omegab;     ///< longitude of periapsis [rad]
	double L;          ///< mean longitude at epoch
} ELEMENTS;

/**
 * \brief Secondary orbital parameters derived from the primary \ref ELEMENTS.
 *
 * This members of this structure provide additional parameters to the
 * primary elements of contained in the ELEMENTS structure.
 * \note SMi: for open orbits, this represents the imaginary semi-axis
 * \note PeD: distance to lowest point of the orbit from focal point
 * \note ApD: distance of highest point of the orbit from focal point. Only
 *   defined for closed orbits.
 * \note T: orbit period only defined for closed orbits.
 * \note PeT: For open orbits, this is negative after periapis passage
 * \note ApT: Only defined for closed orbits.
 * \sa ELEMENTS, \subpage orbit
 */
typedef struct {
	double SMi;        ///< semi-minor axis [m]
	double PeD;        ///< periapsis distance [m]
	double ApD;        ///< apoapsis distance [m]
	double MnA;        ///< mean anomaly [rad]
	double TrA;        ///< true anomaly [rad]
	double MnL;        ///< mean longitude [rad]
	double TrL;        ///< true longitude [rad]
	double EcA;        ///< eccentric anomaly [rad]
	double Lec;        ///< linear eccentricity [m]
	double T;          ///< orbit period [s]
	double PeT;        ///< time to next periapsis passage [s]
	double ApT;        ///< time to next apoapsis passage [s]
} ORBITPARAM;

/**
 * \ingroup structures
 * \brief Planetary atmospheric constants structure
 */
typedef struct {
	double p0;         ///<     pressure at mean radius ('sea level') [Pa]
	double rho0;       ///<     density at mean radius
	double R;          ///<     specific gas constant [J/(K kg)]
	double gamma;      ///<     ratio of specific heats, c_p/c_v
	double C;          ///<     exponent for pressure equation (temporary)
	double O2pp;       ///<     partial pressure of oxygen
	double altlimit;   ///<     atmosphere altitude limit [m]
	double radlimit;   ///<     radius limit (altlimit + mean radius)
	double horizonalt; ///<     horizon rendering altitude
	VECTOR3 color0;    ///<     sky colour at sea level during daytime
} ATMCONST;

/** \brief Atmospheric parameters structure */
typedef struct {
	double T;          ///<     temperature [K]
	double p;          ///<     pressure [Pa]
	double rho;        ///<     density [kg/m^3]
} ATMPARAM;

/** \brief Engine status */
typedef struct {
	double main;       ///<     -1 (full retro) .. +1 (full main)
	double hover;      ///<     0 .. +1 (full hover)
	int attmode;       ///<     0=rotation, 1=translation
} ENGINESTATUS;

/**
 * \ingroup defines
 * \defgroup exhaustflag Bitflags for EXHAUSTSPEC flags field.
 * \sa EXHAUSTSPEC
 */
//@{
#define EXHAUST_CONSTANTLEVEL 0x0001 ///< exhaust level is constant
#define EXHAUST_CONSTANTPOS   0x0002 ///< exhaust position is constant
#define EXHAUST_CONSTANTDIR   0x0004 ///< exhaust direction is constant
//@}

/**
 * \brief Engine exhaust render parameters
 * \sa VESSEL::AddExhaust(EXHAUSTSPEC*)
 */
typedef struct {
	THRUSTER_HANDLE th;///<  handle of associated thruster (or NULL if none)
	double *level;     ///<  pointer to variable containing exhaust level (0..1)
	VECTOR3 *lpos;     ///<  pointer to exhaust position vector [m]
	VECTOR3 *ldir;     ///<  pointer to engine thrust direction (=negative exhaust direction)
	double lsize;      ///<  exhaust length [m]
	double wsize;      ///<  exhaust width [m]
	double lofs;       ///<  longitudinal offset from engine [m]
	double modulate;   ///<  magnitude of random intensity variations (0..1)
	SURFHANDLE tex;    ///<  custom texture handle
	DWORD flags;       ///<  Bit flags (see \ref exhaustflag)
	UINT id;           ///<  reserved
} EXHAUSTSPEC;

/**
 * \brief Particle stream parameters
 * \note The following mapping methods (LEVELMAP) between stream
 *   level L and opacity a are supported:
 *   - LVL_FLAT: \f$ \alpha = \mathrm{const} \f$
 *   - LVL_LIN:  \f$ \alpha = L \f$
 *   - LVL_SQRT: \f$ \alpha = \sqrt(L) \f$
 *   - LVL_PLIN: \f$ \alpha = \left\lbrace \begin{array}{ll}
                     0 & \mathrm{if} L < L_\mathrm{min} \\
					 \frac{L-L_\mathrm{min}}{L_\mathrm{max}-L_\mathrm{min}} & \mathrm{if} L_\mathrm{min} \leq L \leq L_\mathrm{max} \\
					 1 & \mathrm{if} L > L_\mathrm{max}
					 \end{array} \right. \f$
 *   - LVL_PSQRT: \f$ \alpha = \left\lbrace \begin{array}{ll}
                     0 & \mathrm{if} L < L_\mathrm{min} \\
					 \sqrt{\frac{L-L_\mathrm{min}}{L_\mathrm{max}-L_\mathrm{min}}} & \mathrm{if} L_\mathrm{min} \leq L \leq L_\mathrm{max} \\
					 1 & \mathrm{if} L > L_\mathrm{max}
					 \end{array} \right. \f$
 */
typedef struct {
	DWORD flags;       ///<     streamspec bitflags
	double srcsize;    ///<     particle size at creation [m]
	double srcrate;    ///<     average particle creation rate [Hz]
	double v0;         ///<     emission velocity [m/s]
	double srcspread;  ///<     velocity spread during creation
	double lifetime;   ///<     average particle lifetime [s]
	double growthrate; ///<     particle growth rate [m/s]
	double atmslowdown;///<     slowdown rate in atmosphere
	/** \brief Particle lighting method */
	enum LTYPE {
		EMISSIVE,      ///<     emissive lighting (example: plasma stream)
		DIFFUSE        ///<     diffuse lighting (example: vapour stream)
	} ltype;           ///<     render lighting method
	/** \brief Mapping from level to alpha value (particle opacity) */
	enum LEVELMAP {
		LVL_FLAT,      ///<     constant (alpha independent of level)
		LVL_LIN,       ///<     linear mapping (alpha = level)
		LVL_SQRT,      ///<     square root mapping (alpha = sqrt(level)
		LVL_PLIN,      ///<     linear mapping in sub-range
		LVL_PSQRT      ///<     square-root mapping in sub-range
	} levelmap;        ///< mapping from level to alpha
	double lmin, lmax; ///<     min and max levels for level PLIN and PSQRT mapping types
	enum ATMSMAP { ATM_FLAT, ATM_PLIN, ATM_PLOG } atmsmap;    ///< mapping from atmospheric params to alpha
	double amin, amax; ///<     min and max densities for atms PLIN mapping
	SURFHANDLE tex;    ///<     particle texture handle (NULL for default)
} PARTICLESTREAMSPEC;

/**
 * \defgroup imguidialog ImGui dialog
 *
 * This group defines ImGuiDialog definition.
 */
//@{
/**
 * \brief Base class for defining an ImGui dialog.
 */
class OAPIFUNC ImGuiDialog {
	const std::string name;
public:
	/**
	 * \brief Create an ImGui dialog object.
	 * \param name Name of the dialog window
	 * \note This class must by derived from in order to define a custom ImGui dialog
	 */
	ImGuiDialog(const char *n):name(n) {}
	virtual ~ImGuiDialog();
	bool IsActive() { return active; }
	void Activate() { active = true; }
	virtual void Display();
protected:
	/**
	 * \brief Callback that is executed when the dialog is closed.
	 * \note The default behavior is to do nothing
	 */
	virtual void OnClose() {};
	/**
	 * \brief Callback that is executed when the dialog should be drawn.
	 * \note Orbiter takes care of doing the ImGui::Begin() / ImGui::End() pair
	 * required to create the window and handle its visibility.
	 * \note ImGui documentation is available at https://github.com/ocornut/imgui
	 */
	virtual void OnDraw() = 0;
	bool active = false;
};

/**
 * \defgroup locallight Local lighting interface
 *
 * The classes in this group define local light sources.
 * \sa VESSEL3::AddPointLight, VESSEL3::AddSpotLight
 */
//@{
/**
 * \brief Base class for defining a light source that can illuminate other objects.
 */
class OAPIFUNC LightEmitter {
public:
	enum TYPE {
		LT_NONE, LT_POINT, LT_SPOT, LT_DIRECTIONAL
	};

	enum VISIBILITY {
		VIS_EXTERNAL=1, VIS_COCKPIT=2, VIS_ALWAYS=3
	};

	/**
	 * \brief Create a light source with default parameters.
	 * \note Creates a light source with white spectrum for diffuse, specular
	 *   and emissive colour components.
	 * \note Intensity is set to 1, position (for point source objects) is set to (0,0,0)
	 *   and direction (for spot and directional lights) is set to (0,0,1). To change
	 *   these, use \ref SetPosition, \ref SetPositionRef, \ref SetDirection,
	 *   \ref SetDirectionRef, \ref SetIntensity, \ref SetIntensityRef
	 */
	LightEmitter ();

	/**
	 * \brief Create a light source with specific colour parameters.
	 * \param diffuse light source's contribution to lit objects' diffuse colour component
	 * \param specular light source's contribution to lit objects' specular colour component
	 * \param ambient light source's contribution to lit objects' ambient colour component
	 * \note Intensity is set to 1, position (for point source objects) is set to (0,0,0)
	 *   and direction (for spot and directional lights) is set to (0,0,1). To change
	 *   these, use \ref SetPosition, \ref SetPositionRef, \ref SetIntensity, \ref SetIntensityRef
	 */
	LightEmitter (COLOUR4 diffuse, COLOUR4 specular, COLOUR4 ambient);

	/**
	 * \brief Returns the light source type.
	 */
	TYPE GetType() const { return ltype; }

	/**
	 * \brief Returns the light visibility mode
	 * \return visibility mode
	 */
	VISIBILITY GetVisibility() const { return visibility; }

	/**
	 * \brief Set the light visibility mode
	 * \param vis visibility mode
	 */
	void SetVisibility (VISIBILITY vis) { visibility = vis; }

	const COLOUR4 &GetDiffuseColour() const { return col_diff; }
	const COLOUR4 &GetSpecularColour() const { return col_spec; }
	const COLOUR4 &GetAmbientColour() const { return col_ambi; }

	/**
	 * \brief Activate or deactivate the light source
	 * \param act if \e true, activates the light source. Otherwise, deactivates
	 *   the light source
	 * \sa IsActive
	 */
	void Activate (bool act);

	/**
	 * \brief Returns activation status of light source
	 * \return \e true if source is active, \e false otherwise.
	 * \sa Activate
	 */
	bool IsActive () const;

	/**
	 * \brief Set light source position.
	 * \param p new position [<b>m</b>] (in object or global coordinates)
	 * \note The source position is only relevant for point and spot lights. It is
	 *   ignored for directional lights
	 * \note If the source is attached to an object (see \ref Attach) the position is
	 *   interpreted in the local object coordinates. Otherwise, the position is
	 *   taken to be in global coordinates.
	 * \note After a displacement of the vessel's centre of mass (see \ref VESSEL::ShiftCG),
	 *   all light sources that define their position explicitly (via SetPosition) are
	 *   updated automatically. Light sources with implicit position definition (via
	 *   \ref SetPositionRef) must update their positions themselves.
	 * \sa GetPosition, SetPositionRef, GetPositionRef
	 */
	void SetPosition (const VECTOR3 &p);

	/**
	 * \brief Returns the current source position.
	 * \return Current source position [<b>m</b>]
	 * \note The source position is only relevant for point and spot lights. It is
	 *   ignored for directional lights
	 * \note If the source is attached to an object (see \ref Attach) the returned
	 *   vector is the source position in local object coordinates. Otherwise, the
	 *   returned vector is the global source position.
	 * \sa SetPosition, SetPositionRef, GetPositionRef
	 */
	inline VECTOR3 GetPosition () const { return *pos; }

	/**
	 * \brief Set the reference pointer to the light source position.
	 * \param p pointer to vector defining the source position
	 * \note This method links the position of the light source to an externally
	 *   defined vector. By modifying the vector elements, the light source can
	 *   be re-positioned instantly.
	 * \note The vector variable pointed to by \a p must remain valid for the
	 *   lifetime of the light source.
	 * \note The source position is only relevant for point and spot lights. It is
	 *   ignored for directional lights
	 * \sa SetPosition, GetPosition, GetPositionRef
	 */
	void SetPositionRef (const VECTOR3 *p);

	/**
	 * \brief Returns a pointer to the position reference variable.
	 * \return Pointer to the variable defining the light source position
	 * \note If the position is defined explicitly (see \ref SetPosition),
	 *   this method simply returns a pointer to the lpos member variable.
	 *   Otherwise, is returns the pointer specified in \ref SetPositionRef.
	 * \sa SetPosition, SetPositionRef, GetPosition
	 */
	const VECTOR3 *GetPositionRef () const;

	/**
	 * \brief Adds an offset to the explicit position definition of the source.
	 * \param ofs offset vector in local vessel coordinates
	 * \note This method has only an effect for light sources whose positions are
	 *   defined explicitly (via \ref SetPosition). If the source position is defined
	 *   implicitly (via \ref SetPositionRef), this method has no effect. Modules that
	 *   define their light source positions via implicit references must keep the
	 *   positions up to date themselves (e.g. reacting to shifts in the centre of gravity).
	 * \sa SetPosition, SetPositionRef, VESSEL::ShiftCG
	 */
	void ShiftExplicitPosition (const VECTOR3 &ofs);

	/**
	 * \brief Set light source direction.
	 * \param p new direction (in object or global coordinates)
	 * \note The vector argument should be normalised to length 1.
	 * \note The source direction is only relevant for spot and directional lights.
	 *   It is ignored for point lights.
	 * \note If the source is attached to an object (see \ref Attach) the direction is
	 *   interpreted in the local object coordinates. Otherwise, the direction is
	 *   taken to be in global coordinates.
	 * \sa GetDirection, SetDirectionRef, GetDirectionRef
	 */
	void SetDirection (const VECTOR3 &d);

	/**
	 * \brief Returns the current source direction.
	 * \return Current source direction.
	 * \note The source direction is only relevant for spot and directional lights.
	 *   It is ignored for point lights.
	 * \note If the source is attached to an object (see \ref Attach) the returned
	 *   vector is the source direction in local object coordinates. Otherwise, the
	 *   returned vector is the global source direction.
	 * \sa SetDirection, SetDirectionRef, GetDirectionRef
	 */
	VECTOR3 GetDirection () const;

	/**
	 * \brief Set the reference pointer to the light source direction.
	 * \param d pointer to vector defining the source direction
	 * \note This method links the direction of the light source to an externally
	 *   defined vector. By modifying the vector elements, the light source can
	 *   be re-directed instantly.
	 * \note The vector variable pointed to by \a d must remain valid for the
	 *   lifetime of the light source.
	 * \note The source direction is only relevant for spot and directional lights.
	 *   It is ignored for point lights
	 * \sa SetDirection, GetDirection, GetDirectionRef
	 */
	void SetDirectionRef (const VECTOR3 *d);

	/**
	 * \brief Returns a pointer to the direction reference variable.
	 * \return Pointer to the variable defining the light source direction
	 * \note If the direction is defined explicitly (see \ref SetDirection),
	 *   this method simply returns a pointer to the ldir member variable.
	 *   Otherwise, is returns the pointer specified in \ref SetDirectionRef.
	 * \sa SetDirection, SetDirectionRef, GetDirection
	 */
	const VECTOR3 *GetDirectionRef () const;

	/**
	 * \brief Returns the handle of the object the light source is attached to.
	 * \return Object handle, or NULL if not attached
	 */
	const OBJHANDLE GetObjectHandle () const { return hRef; }

	void SetIntensity (double in);
	double GetIntensity () const;
	void SetIntensityRef (double *pin);
	const double *GetIntensityRef () const;

protected:
	OBJHANDLE Attach (OBJHANDLE hObj);
	OBJHANDLE Detach ();

	TYPE ltype;
	VISIBILITY visibility;
	OBJHANDLE hRef;
	bool active;
	COLOUR4 col_diff;
	COLOUR4 col_spec;
	COLOUR4 col_ambi;
	const double *intens;
	double lintens;
	const VECTOR3 *pos;
	const VECTOR3 *dir;
	VECTOR3 lpos;
	VECTOR3 ldir;
};

/**
 * \brief Class for isotropic point light source
 */
class OAPIFUNC PointLight: public LightEmitter {
public:
	/**
	 * \brief Creates a white isotropic point light.
	 * \param hObj handle of object the point light is attached to
	 * \param _pos light position in local object coordinates [<b>m</b>]
	 * \param _range point light range [m]
	 * \param att0 light attenuation parameters
	 * \param att1 light attenuation parameters
	 * \param att2 light attenuation parameters
	 */
	PointLight (OBJHANDLE hObj, const VECTOR3 &_pos, double _range, double att0, double att1, double att2);

	/**
	 * \brief Creates a coloured isotropic point light.
	 * \param hObj handle of object the point light is attached to
	 * \param _pos point light position in local object coordinates [<b>m</b>]
	 * \param _range spotlight range [m]
	 * \param att0 light attenuation parameters
	 * \param att1 light attenuation parameters
	 * \param att2 light attenuation parameters
	 * \param diffuse light source's contribution to lit objects' diffuse colour component
	 * \param specular light source's contribution to lit objects' specular colour component
	 * \param ambient light source's contribution to lit objects' ambient colour component
	 */
	PointLight (OBJHANDLE hObj, const VECTOR3 &_pos, double _range, double att0, double att1, double att2, COLOUR4 diffuse, COLOUR4 specular, COLOUR4 ambient);

	/**
	 * \brief Returns the light source range.
	 * \return Light source range [m]
	 */
	double GetRange() const { return range; }

	/**
	 * \brief Set the light source range.
	 * \param _range new light source range [m]
	 * \note When changing the range, the attenuation factors usually should be adjusted
	 *   accordingly, to avoid sharp cutoff edges or large areas of negligible intensity.
	 */
	void SetRange (double _range);

	/**
	 * \brief Returns a pointer to attenuation coefficients.
	 * \return Pointer to array of 3 attenuation coefficients.
	 * \note The attenuation coefficients define the fractional light intensity I/I0 as
	 *   a function of distance d:
	 *   \f[ \frac{I}{I_0} = \frac{1}{\mathrm{att}_0 + d \mathrm{att}_1 + d^2 \mathrm{att}_2} \f]
	 */
	const double *GetAttenuation() const { return att; }

	/**
	 * \brief Set the attenuation coefficients.
	 * \param att0 attenuation coefficient
	 * \param att1 attenuation coefficient
	 * \param att2 attenuation coefficient
	 * \note The attenuation coefficients define the fractional light intensity I/I0 as
	 *   a function of distance d:
	 *   \f[ \frac{I}{I_0} = \frac{1}{\mathrm{att}_0 + d \mathrm{att}_1 + d^2 \mathrm{att}_2} \f]
	 */
	void SetAttenuation (double att0, double att1, double att2);

protected:
	double range;
	double att[3];
};

/**
 * \brief Class for directed spot light sources
 */
class OAPIFUNC SpotLight: public PointLight {
public:
	/**
	 * \brief Creates a white spotlight.
	 * \param hObj handle of object the spotlight is attached to
	 * \param _pos spotlight position in local object coordinates [<b>m</b>]
	 * \param _dir spotlight direction in local object coordinates
	 * \param _range spotlight range [m]
	 * \param att0 light attenuation parameters
	 * \param att1 light attenuation parameters
	 * \param att2 light attenuation parameters
	 * \param _umbra angular aperture of inner (maximum intensity) cone [rad]
	 * \param _penumbra angular aperture of outer (zero intensity) cone [rad]
	 * \note Direction vector \a _dir must be normalised to length 1.
	 * \note 0 < _umbra <= penumbra <= pi is reqired.
	 * \note The intensity falloff between _umbra and _penumbra is linear from
	 *   maximum intensity to zero.
	 */
	SpotLight (OBJHANDLE hObj, const VECTOR3 &_pos, const VECTOR3 &_dir, double _range, double att0, double att1, double att2, double _umbra, double _penumbra);

	/**
	 * \brief Creates a coloured spotlight.
	 * \param hObj handle of object the spotlight is attached to
	 * \param _pos spotlight position in local object coordinates [<b>m</b>]
	 * \param _dir spotlight direction in local object coordinates
	 * \param _range spotlight range [m]
	 * \param att0 light attenuation parameters
	 * \param att1 light attenuation parameters
	 * \param att2 light attenuation parameters
	 * \param _umbra angular aperture of inner (maximum intensity) cone [rad]
	 * \param _penumbra angular aperture of outer (zero intensity) cone [rad]
	 * \param diffuse light source's contribution to lit objects' diffuse colour component
	 * \param specular light source's contribution to lit objects' specular colour component
	 * \param ambient light source's contribution to lit objects' ambient colour component
	 * \note Direction vector \a _dir must be normalised to length 1.
	 * \note 0 < _umbra <= penumbra <= pi is reqired.
	 * \note The intensity falloff between _umbra and _penumbra is linear from
	 *   maximum intensity to zero.
	 */
	SpotLight (OBJHANDLE hObj, const VECTOR3 &_pos, const VECTOR3 &_dir, double _range, double att0, double att1, double att2, double _umbra, double _penumbra, COLOUR4 diffuse, COLOUR4 specular, COLOUR4 ambient);

	/**
	 * \brief Returns the angular aperture of inner (maximum intensity) cone.
	 * \return Aperture of inner spotlight cone [rad]
	 * \sa GetPenumbra
	 */
	double GetUmbra() const { return umbra; }

	/**
	 * \brief Returns the angular aperture of outer (zero intensity) cone.
	 * \return Aperture of outer spotlight cone [rad]
	 * \sa GetUmbra
	 */
	double GetPenumbra() const { return penumbra; }

	/**
	 * \brief Set the spotlight cone geometry.
	 * \param _umbra angular aperture of inner (maximum intensity) cone [rad]
	 * \param _penumbra angular aperture of outer (zero intensity) cone [rad]
	 */
	void SetAperture (double _umbra, double _penumbra);

protected:
	double umbra;
	double penumbra;
};
//@}


/** \brief Navigation transmitter data
 *
 * This structure contains both general data (transmitter type, channel,
 * output power and description string) and type-specific data.
 * To query type-specific data, first check the transmitter type, for example
 * \code
 * NAVDATA ndata;
 * oapiGetNavData (hNav, &ndata);
 * if (ndata.type == TRANSMITTER_ILS)
 *    approach_dir = ndata.ils.appdir;
 * \endcode
 * \note The power S<sub>0</sub> of a transmitter is defined in arbitrary units
 *   such that the signal S(r) = S<sub>0</sub>/r<sup>2</sup> drops to 1 at the
 *   maximum range r<sub>max</sub>, given a default receiver, i.e.
 *   S<sub>0</sub> = r<sup>2</sup><sub>max</sub>.
 * \sa oapiGetNavData
 */
typedef struct {
	// general data
	DWORD type;                ///< transmitter type id
	DWORD ch;                  ///< transmitter channel (0..639)
	double power;              ///< transmitter power [arbitrary units]
	const char *descr;         ///< pointer to transmitter description string
	// type-specific data
    union {
		struct {
			OBJHANDLE hPlanet; ///< associated planet
			double lng, lat;   ///< transmitter location [rad]
		} vor;
		struct {
			OBJHANDLE hBase;   ///< associated base
			int npad;          ///< pad number (>= 0)
		} vtol;
	    struct {
			OBJHANDLE hBase;   ///< associated base
		    double appdir;     ///< ILS approach direction [rad]
	    } ils;
		struct {
			OBJHANDLE hVessel; ///< associated vessel
			DOCKHANDLE hDock;  ///< associated docking port
		} ids;
		struct {
			OBJHANDLE hVessel; ///< associated vessel
		} xpdr;
	};
} NAVDATA;

/** \brief vessel beacon light parameters */
typedef struct {
	DWORD shape;       ///<   beacon shape identifier (see \ref beaconshape)
	VECTOR3 *pos;      ///<   pointer to position in vessel coordinates
	VECTOR3 *col;      ///<   pointer to beacon RGB colour
	double size;       ///<   beacon radius
	double falloff;    ///<   distance falloff parameter
	double period;     ///<   strobe period (0 for continuous)
	double duration;   ///<   strobe duration
	double tofs;       ///<   strobe time offset
	bool active;       ///<   beacon lit?
} BEACONLIGHTSPEC;

// ===========================================================================
/// \ingroup defines
/// \defgroup beaconshape Light beacon shape parameters
/// \sa BEACONLIGHTSPEC
// ===========================================================================
//@{
#define BEACONSHAPE_COMPACT 0 ///< compact beacon shape
#define BEACONSHAPE_DIFFUSE 1 ///< diffuse beacon shape
#define BEACONSHAPE_STAR    2 ///< star-shaped beacon
//@}

/**
 * \brief Vessel status parameters (version 1)
 *
 * Defines vessel status parameters at a given time. This is version 1 of
 * the vessel status interface. It is retained for backward compatibility,
 * but new modules should use VESSELSTATUS2 instead to exploit the latest
 * vessel capabilities such as individual thruster and propellant resource
 * settings.
 */
typedef struct {
	/// position relative to rbody in ecliptic frame [<b>m</b>]
	VECTOR3 rpos;

	/// velocity relative to rbody in ecliptic frame [<b>m/s</b>]
	VECTOR3 rvel;

	/**
	 * \brief rotation velocity about principal axes in ecliptic frame [<b>rad/s</b>]
     * \note For a vessel with LANDED status, vrot has a different interpretation:
	 *   vrot.x contains the altitude of the CoG above the (elevated) surface. vrot.y
	 *   and vrot.z are ignored.
	 */
	VECTOR3 vrot;

	/**
	 * \brief vessel orientation against ecliptic frame (freeflight) or against
	 *   planet frame (landed)
     */
	VECTOR3 arot;

	/// fuel level [0..1]
	double fuel;

	/// main/retro engine setting [-1..1]
	double eng_main;

	/// hover engine setting [0..1]
	double eng_hovr;

	/// handle of reference body
	OBJHANDLE rbody;

    /// handle of docking or landing target
	OBJHANDLE base;

    /// index of designated docking or landing port
	int port;

    /// \brief flight status indicator
	/// \note
	/// - 0=active (freeflight)
	/// - 1=inactive (landed)
	int status;

	/// \brief additional vector parameters
	/// \note
	/// - vdata[0]: contains landing paramters if status == 1:
	///   vdata[0].x = longitude, vdata[0].y = latitude, vdata[0].z = heading of landed vessel
	/// - vdata[1] - vdata[9]: not used
	VECTOR3 vdata[10];

	/// additional floating point parameters (not used)
	double  fdata[10];

	/// \brief additional integer and bitflag parameters
	///
	/// \par flag[0]&1:
	///   - 0: ingore eng_main and eng_hovr entries, do not change thruster settings
	///   - 1: set THGROUP_MAIN and THGROUP_RETRO thruster groups from eng_main, and THGROUP_HOVER from eng_hovr.
	/// \par flag[0]&2:
	///   - 0: ignore fuel level, do not change fuel levels
	///   - 1: set fuel level of first propellant resource from fuel
	/// \note flag[1] - flag[9]: not used
	DWORD   flag[10];
} VESSELSTATUS;

/**
 * \brief Vessel status parameters (version 2)
 * \details Defines vessel status parameters at a given time. This is version 2 of
 * the vessel status interface and replaces the earlier VESSELSTATUS
 * structure. Functions using VESSELSTATUS are still supported for backward
 * compatibility. \n
 * \note The version specification is an input parameter for all function calls
 *  (including GetStatus) and must be set by the user to tell Orbiter which interface to use.
 * \sa VESSEL::GetStatusEx 
 */
typedef struct {
	/// interface version identifier (2)
	DWORD version;

	/**
	* \brief bit flags
	* \details The meaning of the bitflags in flag depends on whether the VESSELSTATUS2
	*  structure is used to get (GetStatus) or set (SetStatus) a vessel status. The
	*  following flags are currently defined:
	* \par flags:
	* - \c VS_FUELRESET
	*		- Get - not used
	*		- Set - reset all fuel levels to zero, independent of the fuel list.
	*		.
	* - \c VS_FUELLIST
	*		- Get - request a list of current fuel levels in fuel. The module is responsible
	*		  for deleting the list after use.
	*		- Set - set fuel levels for all resources listed in fuel.
	*		.
	* - \c VS_THRUSTRESET
	*		- Get - not used
	*		- Set - reset all thruster levels to zero, independent of the thruster list
	*		.
	* - \c VS_THRUSTLIST
	*		- Get - request a list of current thrust levels in thruster. The module is
	*		   responsible for deleting the list after use.
	*		- Set - set thrust levels for all thrusters listed in thruster.
	*		.
	* - \c VS_DOCKINFOLIST
	*		- Get - request a docking port status list in dockinfo. The module is
	*		  responsible for deleting the list after use.
	*		- Set - initialise docking status for all docking ports in dockinfo.
	* \sa VESSEL::GetStatusEx 
	*/
	DWORD flag;

	/// handle of reference body
	OBJHANDLE rbody;

	/// handle of docking or landing target
	OBJHANDLE base;

	/// index of designated docking or landing port
	int port;

	/// \brief flight status indicator
	/// \note
	/// - 0=active (freeflight)
	/// - 1=inactive (landed)
	int status;

	/// position relative to reference body (rbody) in ecliptic frame [<b>m</b>]
	VECTOR3 rpos;

	/// velocity relative to reference body in ecliptic frame [<b>m/s</b>]
	VECTOR3 rvel;

	/**
	 * \brief angular velocity around principal axes in ecliptic frame [<b>rad/s</b>]
     * \note For a vessel with LANDED status, vrot has a different interpretation:
	 *   vrot.x contains the altitude of the CoG above the (elevated) surface. vrot.y
	 *   and vrot.z are ignored.
	 */
	VECTOR3 vrot;

	/**
	* \brief vessel orientation against ecliptic frame
	* \details \b arot (\f$ \alpha, \beta, \gamma \f$) contains angles of rotation [rad] 
	*  around \e x, \e y, \e z axes in ecliptic
	*  frame to produce this rotation matrix \b R for mapping from the vessel's local
	*  frame of reference to the global frame of reference:
	* \f[ R = 
	*	\left[ \begin{array}{ccc} 1 & 0 & 0 \\	0 & \cos\alpha & \sin\alpha \\ 0 & -\sin\alpha & \cos\alpha \end{array} \right]
	*	\left[ \begin{array}{ccc} \cos\beta & 0 & -\sin\beta \\ 0 & 1 & 0 \\ \sin\beta & 0 & \cos\beta \end{array} \right]
	*	\left[ \begin{array}{ccc} \cos\gamma & \sin\gamma & 0 \\ -\sin\gamma & \cos\gamma & 0 \\ 0 & 0 & 1 \end{array} \right]	 
	* \f]
	* such that \b r<sub>global</sub> = \b R \b r<sub>local</sub> + \b p \n
	* where \b p is the vessel's global position.
	* \note For a vessel with LANDED status, arot has a different interpretation:
	*   It represents the rotation against the planet frame rather than the ecliptic frame. While the
	*   vessel is landed (idle), these values will not change over time.
	*/
	VECTOR3 arot;

	/**
	 * \brief longitude of vessel position in equatorial coordinates of rbody [rad]
	 * \note currently only defined if the vessel is landed (status=1)
	 */
	double surf_lng;

    /**
	 * \brief latitude of vessel position in equatorial coordinates of rbody [rad]
	 * \note currently only defined if the vessel is landed (status=1)
	 */
	double surf_lat;

	/**
	 * \brief vessel heading on the ground [rad]
	 * \note currently only defined if the vessel is landed (status=1)
	 */
	double surf_hdg;

	/// number of entries in the fuel list
	DWORD nfuel;

	/// propellant list
	struct FUELSPEC {
		DWORD idx;      ///< propellant index
		double level;   ///< propellant level
	} *fuel;

	/// number of entries in the thruster list
	DWORD nthruster;

	/// thruster definition list
	struct THRUSTSPEC {
		DWORD idx;      ///< thruster index 
		double level;   ///< thruster level
	} *thruster;

	/// number of entries in the dockinfo list
	DWORD ndockinfo;

	/// dock info list
	struct DOCKINFOSPEC {
		DWORD idx;      ///< docking port index
		DWORD ridx;     ///< docking port index of docked vessel
		OBJHANDLE rvessel; ///< docked vessel
	} *dockinfo;

	/// transponder channel [0...640]
	DWORD xpdr;
} VESSELSTATUS2;


// VESSELSTATUSx bitflags
#define VS_FUELRESET    0x00000001 ///< set all propellant levels to zero
#define VS_FUELLIST     0x00000002 ///< list of propellant levels is provided
#define VS_THRUSTRESET  0x00000004 ///< set all thruster levels to zero
#define VS_THRUSTLIST   0x00000008 ///< list of thruster levels is provided
#define VS_DOCKINFOLIST 0x00000010 ///< list of docked objects is provided

/**
 * \brief Entry specification for selection list entry.
 */
typedef struct {
	char name[64];   ///< entry string
	DWORD flag;      ///< entry flags
} LISTENTRY;

/**
 * \brief Callback function for list entry selections.
 */
typedef bool (*Listentry_clbk)(char *name, DWORD idx, DWORD flag, void *usrdata);

/**
 * \ingroup defines
 * \defgroup listentryflag
 * \sa LISTENTRY
 */
//@
#define LISTENTRY_SUBITEM   0x01  ///< list entry has subitems
#define LISTENTRY_INACTIVE  0x02  ///< list entry can not be selected
#define LISTENTRY_SEPARATOR 0x04  ///< entry is followed by a separator
//@

#define LIST_UPENTRY 0x01         ///< list has parent list

/**
 * \ingroup defines
 * \defgroup listclbkflag
 * \sa LISTENTRY
 */
//@
#define LISTCLBK_CANCEL     0x00  ///< user cancelled the selection list
#define LISTCLBK_SELECT     0x01  ///< user selected an item
#define LISTCLBK_SUBITEM    0x02  ///< user steps down to subitem
#define LISTCLBK_UPLIST     0x03  ///< user steps up to parent list
//@

/**
 * \brief Context information for an Orbiter ingame help page.
 * \sa oapiOpenHelp
 */
typedef struct {
	char *helpfile; 
	char *topic;
	char *toc;
	char *index;
} HELPCONTEXT;

typedef struct {
	char *name;
	void *parent;
	char *desc;
	void (*clbkFunc)(HINSTANCE,HWND);
} LP_EXTRAPRM;

#pragma pack(push,1)
/**
 * \brief This structure defines an affine mesh group transform
 *   (translation, rotation or scaling).
 * \sa VESSEL::MeshgroupTransform
 */
typedef struct {
	union {
		struct {
			VECTOR3 ref;   ///< rotation reference point
			VECTOR3 axis;  ///< rotation axis direction
			float angle;   ///< rotation angle [rad]
		} rotparam;
		struct {
			VECTOR3 shift; ///< translation vector
		} transparam;
		struct {
			VECTOR3 scale; ///< scaling factor
		} scaleparam;
	} P;
	int nmesh;             ///< mesh index (>= 0)
	int ngrp;              ///< group index (>= 0, or < 0 to indicate entire mesh)
	enum { TRANSLATE, ROTATE, SCALE } transform; ///< transformation flag
} MESHGROUP_TRANSFORM;
#pragma pack(pop)

// Animation component (obsolete)
typedef struct {
	UINT *grp;
	UINT ngrp;
	double state0;
	double state1;
	MESHGROUP_TRANSFORM trans;
} ANIMCOMP;

// Transformation types for animations

// Transformation base class
class MGROUP_TRANSFORM {
public:
	MGROUP_TRANSFORM () : mesh(0), grp(0), ngrp(0) {}
	MGROUP_TRANSFORM (UINT _mesh, UINT *_grp, UINT _ngrp)
		: mesh(_mesh), grp(_grp), ngrp(_ngrp) {}
	enum TYPE { NULLTRANSFORM, ROTATE, TRANSLATE, SCALE };
	virtual TYPE Type() const { return NULLTRANSFORM; }

	UINT mesh;
	UINT *grp;
	UINT ngrp;
};

// Rotation
class MGROUP_ROTATE: public MGROUP_TRANSFORM {
public:
	MGROUP_ROTATE (UINT _mesh, UINT *_grp, UINT _ngrp, const VECTOR3 &_ref, const VECTOR3 &_axis, float _angle)
		: MGROUP_TRANSFORM (_mesh, _grp, _ngrp), ref(_ref), axis(_axis), angle(_angle) {}
	TYPE Type() const { return ROTATE; }

	VECTOR3 ref;
	VECTOR3 axis;
	float angle;
};

// Translation
class MGROUP_TRANSLATE: public MGROUP_TRANSFORM {
public:
	MGROUP_TRANSLATE (UINT _mesh, UINT *_grp, UINT _ngrp, const VECTOR3 &_shift)
		: MGROUP_TRANSFORM (_mesh, _grp, _ngrp), shift(_shift) {}
	TYPE Type() const { return TRANSLATE; }

	VECTOR3 shift;
};

// Scaling
class MGROUP_SCALE: public MGROUP_TRANSFORM {
public:
	MGROUP_SCALE (UINT _mesh, UINT *_grp, UINT _ngrp, const VECTOR3 &_ref, const VECTOR3 &_scale)
		: MGROUP_TRANSFORM (_mesh, _grp, _ngrp), ref(_ref), scale(_scale) {}
	TYPE Type() const { return SCALE; }

	VECTOR3 ref;
	VECTOR3 scale;
};

/**
 * \brief Animation component definition
 *
 * Defines one component of an animation, including the mesh transformation,
 * the relative start and end points within the entire animation, and any
 * parent and child relationships with other animations.
 * \sa VESSEL::AddAnimationComponent
 */
struct ANIMATIONCOMP {
	double state0;			  ///< first end state
	double state1;			  ///< second end state
	MGROUP_TRANSFORM *trans;  ///< transformation
	ANIMATIONCOMP *parent;    ///< parent transformation
	ANIMATIONCOMP **children; ///< list of children
	UINT nchildren;           ///< number of children
};

/**
 * \brief Animation definition
 *
 * Defines a complete animation, including a list of components, the current
 * animation state, and the default state (as represented by the original mesh).
 */
struct ANIMATION {
	double defstate;          ///< default animation state in the mesh
	double state;             ///< current state
	UINT ncomp;               ///< number of components
	ANIMATIONCOMP **comp;     ///< list of components
};


/**
 * \ingroup defines
 * \defgroup animationflags Animation flags
 * \sa VESSEL::AddAnimationComponent
 */
//@{
#define LOCALVERTEXLIST ((UINT)(-1)) ///< flags animation component as explicit vertex list
#define MAKEGROUPARRAY(x) ((UINT*)x) ///< casts a vertex array into a group
//@}


typedef struct {
	RECT pos;
	int nbt_left, nbt_right;
	int bt_yofs, bt_ydist;
} MFDSPEC;

typedef struct {
	DWORD nmesh, ngroup;
} VCMFDSPEC;

#define MFD_SHOWMODELABELS       0x0001
#define MFD_TRANSPARENT_WHEN_OFF 0x0002

typedef struct {
	RECT pos;
	DWORD nmesh, ngroup;
	DWORD flag;
	int nbt1, nbt2;
	int bt_yofs, bt_ydist;
} EXTMFDSPEC;

typedef struct {
	DWORD nmesh, ngroup;
	VECTOR3 hudcnt;
	double size;
} VCHUDSPEC;

typedef struct {
	int W, H;
	int CX, CY;
	double Scale;
	int Markersize;
} HUDPAINTSPEC;

typedef struct {
	char *name;
	DWORD key;
	OAPI_MSGTYPE (*msgproc)(UINT,UINT,WPARAM,LPARAM);
} MFDMODESPEC;

typedef struct {
	char *name;
	DWORD key;
	void *context;
	OAPI_MSGTYPE (*msgproc)(UINT,UINT,WPARAM,LPARAM);
} MFDMODESPECEX;

typedef struct {
	int w, h;
	MFDMODESPECEX *spec;
} MFDMODEOPENSPEC;

#pragma pack(push,1)
typedef struct {
	const char *line1, *line2;
	char selchar;
} MFDBUTTONMENU;
#pragma pack(pop)


// ===========================================================================
/// \ingroup defines
/// \defgroup refframe Identifiers for frames of reference
// ===========================================================================
//@{

/**
 * \brief Identifiers for frames of reference
 */
typedef enum {
	FRAME_GLOBAL,    ///< global (ecliptic) frame
	FRAME_LOCAL,     ///< local object frame
	FRAME_REFLOCAL,  ///< local reference object frame
	FRAME_HORIZON    ///< local horizon frame
} REFFRAME;

//@}

// ===========================================================================
/// \ingroup defines
/// \defgroup thrusterparam Thruster and thruster-group parameters
// ===========================================================================
//@{

/**
 * \brief Thruster group identifiers (obsolete)
 */
typedef enum {
	ENGINE_MAIN,     ///< main thrusters
	ENGINE_RETRO,    ///< retro thrusters
	ENGINE_HOVER,    ///< hover thrusters
	ENGINE_ATTITUDE  ///< attitude (RCS) thrusters
} ENGINETYPE;

typedef enum {
	EXHAUST_MAIN,
	EXHAUST_RETRO,
	EXHAUST_HOVER,
	EXHAUST_CUSTOM
} EXHAUSTTYPE;

/**
 * \brief Thruster group types
 * \sa VESSEL::CreateThrusterGroup
 */
enum THGROUP_TYPE {
	THGROUP_MAIN,            ///< main thrusters
	THGROUP_RETRO,           ///< retro thrusters
	THGROUP_HOVER,           ///< hover thrusters
	THGROUP_ATT_PITCHUP,     ///< rotation: pitch up
	THGROUP_ATT_PITCHDOWN,   ///< rotation: pitch down
	THGROUP_ATT_YAWLEFT,     ///< rotation: yaw left
	THGROUP_ATT_YAWRIGHT,    ///< rotation: yaw right
	THGROUP_ATT_BANKLEFT,    ///< rotation: bank left
	THGROUP_ATT_BANKRIGHT,   ///< rotation: bank right
	THGROUP_ATT_RIGHT,       ///< translation: move right
	THGROUP_ATT_LEFT,        ///< translation: move left
	THGROUP_ATT_UP,          ///< translation: move up
	THGROUP_ATT_DOWN,        ///< translation: move down
	THGROUP_ATT_FORWARD,     ///< translation: move forward
	THGROUP_ATT_BACK,        ///< translation: move back
	THGROUP_USER = 0x40,     ///< user-defined group
	THGROUP_USER_LAST = 0xFF ///< range limit for user-defined groups
};
//@}

typedef enum {
	ATTMODE_DISABLED,
	ATTMODE_ROT,
	ATTMODE_LIN,
} ATTITUDEMODE;

typedef double (*LiftCoeffFunc)(double aoa); // obsolete

typedef void (*AirfoilCoeffFunc)(
	double aoa, double M, double Re,
	double *cl, double *cm, double *cd);
// Template for aerodynamic coefficients callback function
// Used to calculate lift coefficient cl, moment coefficient
// cm, and drag coefficient cd as a function of angle of attack
// aoa, Mach number M, and Reynolds number Re

typedef void (*AirfoilCoeffFuncEx)(
	VESSEL *v, double aoa, double M, double Re, void *context,
	double *cl, double *cm, double *cd);
// Extended version of aerodynamic coefficients callback function
// Contains additional parameters (calling vessel and pointer to
// user-defined data)

typedef void (*AirfoilCoeffFuncEx2)(
	VESSEL* v,
	VECTOR3 WindDir,
	double alpha, double beta, double gamma,
	double M, double Re, void* context,
	double* CA, double* CN, double* CY,
	double* Cl, double* Cm, double* Cn);
// Further extended version of aerodynamic coefficients callback function
// Contains additional parameters (calling vessel and pointer to
// user-defined data for all force and moment coefficients)


// ===========================================================================
/// \ingroup defines
/// \defgroup airfoilliftdir Airfoil orientation
// ===========================================================================
//@{
/**
 * \brief Lift vector orientation for airfoils.
 *
 * Defines the orientation of an airfoil by the direction of the lift vector
 * generated (vertical or horizontal).
 * \sa VESSEL::CreateAirfoil, VESSEL::CreateAirfoil2, VESSEL::CreateAirfoil3, VESSEL::CreateAirfoil4
 */
typedef enum {
	LIFT_VERTICAL,     ///< lift direction is vertical (e.g. elevator)
	LIFT_HORIZONTAL,    ///< lift direction is horizontal (e.g. rudder)
	FORCE_AND_MOMENT	///< complex model of forces and moments along and about all axes
} AIRFOIL_ORIENTATION;
//@}


// ===========================================================================
/// \ingroup defines
/// \defgroup airctrltype Aerodynamic control surface types
// ===========================================================================
//@{
/**
 * \brief Control surfaces provide attitude and drag control during
 *   atmospheric flight.
 * \sa VESSEL::CreateControlSurface, VESSEL::CreateControlSurface2, VESSEL::CreateControlSurface3
 */
typedef enum {
	AIRCTRL_ELEVATOR,     ///< elevator control (pitch control)
	AIRCTRL_RUDDER,       ///< rudder control (yaw control)
	AIRCTRL_AILERON,      ///< aileron control (bank control)
	AIRCTRL_FLAP,         ///< flaps (lift, drag control)
	AIRCTRL_ELEVATORTRIM, ///< elevator trim
	AIRCTRL_RUDDERTRIM    ///< rudder trim
} AIRCTRL_TYPE;
//@}


// ===========================================================================
/// \ingroup defines
/// \defgroup airctrlaxis Control surface axis orientation
// ===========================================================================
//@{
/**
 * \brief Constants to define the rotation axis and direction of
 *  aerodynamic control surfaces.
 * \sa VESSEL::CreateControlSurface, VESSEL::CreateControlSurface2, VESSEL::CreateControlSurface3
 */
#define AIRCTRL_AXIS_AUTO        0 ///< automatic orientation
#define AIRCTRL_AXIS_YPOS        1 ///< y-axis (vertical), positive rotation
#define AIRCTRL_AXIS_YNEG        2 ///< y-axis (vertical), negative rotation
#define AIRCTRL_AXIS_XPOS        3 ///< x-axis (transversal), positive rotation
#define AIRCTRL_AXIS_XNEG        4 ///< x-axis (transversal), negative rotation
//@}

// Object types
#define OBJTP_INVALID            0
#define OBJTP_GENERIC            1
#define OBJTP_CBODY              2
#define OBJTP_STAR               3
#define OBJTP_PLANET             4
#define OBJTP_VESSEL            10
#define OBJTP_SURFBASE          20

/** *********************************************************************
 * \ingroup defines
 * \defgroup visevent Identifiers for visual events
 *
 * These constants define events that are sent from the Orbiter core to
 * visual instances in a graphics client. The client receives these notifications
 * via the oapi::GraphicsClient::clbkVisEvent callback function, where the first parameter
 * is the event identifier, and the second parameter is a message-specific context
 * value.
 */
//@{
#define EVENT_VESSEL_INSMESH      0 ///< Insert a mesh (context: mesh index)
#define EVENT_VESSEL_DELMESH      1 ///< Delete a mesh (context: mesh index, or -1 for all)
#define EVENT_VESSEL_MESHVISMODE  2 ///< Set mesh visibility mode (context: mesh index)
#define EVENT_VESSEL_RESETANIM    3 ///< Reset animations
#define EVENT_VESSEL_CLEARANIM    4 ///< Clear all animations (context: UINT (1=reset animations, 0=leave animations at current state)
#define EVENT_VESSEL_DELANIM      5 ///< Delete an animation (context: animation index)
#define EVENT_VESSEL_NEWANIM      6 ///< Create a new animation (context: animation index)
#define EVENT_VESSEL_MESHOFS      7 ///< Shift a mesh (context: mesh index)
#define EVENT_VESSEL_MODMESHGROUP 8 ///< A mesh group has been modified
//@}


/************************************************************************/
/**
 * \ingroup defines
 * \defgroup navmode Navigation mode identifiers
 *
 *  These constants are used to refer to the built-in "auto-navigation"
 *  modes, mostly for mainaining specific vessel attitudes via use of
 *  RCS thrusters.
 * \sa VESSEL::ActivateNavmode, VESSEL::DeactivateNavmode, VESSEL::ToggleNavmode,
 *   VESSEL::GetNavmodeState
 */
//@{
#define NAVMODE_KILLROT          1 ///< "Kill rotation" mode
#define NAVMODE_HLEVEL           2 ///< "Hold level with horizon" mode
#define NAVMODE_PROGRADE         3 ///< "Prograde" mode
#define NAVMODE_RETROGRADE       4 ///< "Retrograde" mode
#define NAVMODE_NORMAL           5 ///< "Normal to orbital plane" mode
#define NAVMODE_ANTINORMAL       6 ///< "Anti-normal to orbital plane" mode
#define NAVMODE_HOLDALT          7 ///< "Hold altitude" mode
//@}

/**
 * \ingroup defines
 * \defgroup nav_bitflag Navigation mode bitflags
 */
//@{
#define NAVBIT_KILLROT    0x01
#define NAVBIT_HLEVEL     0x02
#define NAVBIT_PROGRADE   0x04
#define NAVBIT_RETROGRADE 0x08
#define NAVBIT_NORMAL     0x10
#define NAVBIT_ANTINORMAL 0x20
#define NAVBIT_HOLDALT    0x40
//@}

/**
 * \ingroup defines
 * \defgroup manctrl_mode Manual control mode identifiers
 *
 * Constants used to identify attitude control modes for manual input.
 * \sa VESSEL::GetManualControlLevel
 */
//@{
#define MANCTRL_ATTMODE          0 ///< current attitude mode
#define MANCTRL_REVMODE          1 ///< reverse of current attitude mode
#define MANCTRL_ROTMODE          2 ///< rotational attitude modes only
#define MANCTRL_LINMODE          3 ///< linear attitude modes only
#define MANCTRL_ANYMODE          4 ///< rotational and linear modes
//@}

/**
 * \ingroup defines
 * \defgroup manctrl_dev Manual control device identifiers
 *
 * Constants used to identify manual input devices.
 * \sa VESSEL::GetManualControlLevel
 */
 //@{
#define MANCTRL_KEYBOARD         0 ///< keyboard input
#define MANCTRL_JOYSTICK         1 ///< joystick input
#define MANCTRL_ANYDEVICE        2 ///< input from any device
//@}

#define COCKPIT_GENERIC          1
#define COCKPIT_PANELS           2
#define COCKPIT_VIRTUAL          3

#define CAM_COCKPIT              0
#define CAM_TARGETRELATIVE       1
#define CAM_ABSDIRECTION         2
#define CAM_GLOBALFRAME          3
#define CAM_TARGETTOOBJECT       4
#define CAM_TARGETFROMOBJECT     5
#define CAM_GROUNDOBSERVER       6

// time propagation modes
#define PROP_ORBITAL             0x0F
#define PROP_ORBITAL_ELEMENTS    0x00
#define PROP_ORBITAL_FIXEDSTATE  0x01
#define PROP_ORBITAL_FIXEDSURF   0x02
#define PROP_SORBITAL            0xF0
#define PROP_SORBITAL_ELEMENTS   (0x0 << 4)
#define PROP_SORBITAL_FIXEDSTATE (0x1 << 4)
#define PROP_SORBITAL_FIXEDSURF  (0x2 << 4)
#define PROP_SORBITAL_DESTROY    (0x3 << 4)

#define USRINPUT_NEEDANSWER     1


// ===========================================================================
/**
 * \ingroup defines
 * \defgroup rcsmode RCS mode identifiers
 *
 * These constants are used to define the operation mode of the reaction
 * control system (RCS) of a vessel.
 * \sa VESSEL::GetAttitudeMode, VESSEL::SetAttitudeMode
 */
 // ===========================================================================
//@{
#define RCS_NONE                 0 ///< None (RCS off)
#define RCS_ROT                  1 ///< Rotational mode
#define RCS_LIN                  2 ///< Linear (translational) mode
//@}


// ===========================================================================
/// \ingroup defines
/// \defgroup hudmode HUD mode identifiers
///  These constants are used to refer to the built-in HUD (head-up display) modes.
// ===========================================================================
//@{
#define HUD_NONE                 0 ///< No mode (turn HUD off)
#define HUD_ORBIT                1 ///< Orbit HUD mode
#define HUD_SURFACE              2 ///< Surface HUD mode
#define HUD_DOCKING              3 ///< Docking HUD mode
//@}

/**
 * \brief Mode-specific parameters for HUD mode settings
 * \sa oapiSetHUDMode(int,const HUDPARAM*)
 */
typedef union {
	struct {
		OBJHANDLE hRef;   ///< orbit HUD reference object (NULL for auto)
	} HUDorbit;
	struct {
		DWORD NavIdx;       ///< docking HUD nav receiver index (>= 0)
	} HUDdocking;
} HUDPARAM;

// ===========================================================================
/// \ingroup defines
/// \defgroup mfdmode MFD mode identifiers
/// These constants are used to refer to the built-in MFD (multifunctional display) modes.
// ===========================================================================
//@{
#define MFD_REFRESHBUTTONS      -1 ///< Refresh MFD buttons
#define MFD_NONE                 0 ///< No mode (turn MFD off)
#define MFD_ORBIT                1 ///< Orbit MFD mode
#define MFD_SURFACE              2 ///< Surface MFD mode
#define MFD_MAP                  3 ///< Map MFD mode
#define MFD_HSI                  4 ///< HSI (horizontal situation indicator) MFD mode
#define MFD_LANDING              5 ///< VTOL support MFD mode
#define MFD_DOCKING              6 ///< Docking support MFD mode
#define MFD_OPLANEALIGN          7 ///< Orbital plane alignment MFD mode
#define MFD_OSYNC                8 ///< Orbit synchronisation MFD mode
#define MFD_TRANSFER             9 ///< Transfer orbit MFD mode
#define MFD_COMMS               10 ///< Communications MFD mode
#define MFD_USERTYPE            64 ///< User-defined MFD mode
#define BUILTIN_MFD_MODES       10 ///< Number of built-in MFD modes
//@}

// ===========================================================================
/// \ingroup defines
/// \defgroup mfdidentifier MFD identifiers
// ===========================================================================
//@{
#define MAXMFD                  12  ///< Max. number of MFD displays per panel
#define MFD_LEFT                 0  ///< Left default MFD display
#define MFD_RIGHT                1  ///< Right default MFD display
#define MFD_USER1                2  ///< User-defined MFD display 1
#define MFD_USER2                3  ///< User-defined MFD display 2
#define MFD_USER3                4  ///< User-defined MFD display 3
#define MFD_USER4                5  ///< User-defined MFD display 4
#define MFD_USER5                6  ///< User-defined MFD display 5
#define MFD_USER6                7  ///< User-defined MFD display 6
#define MFD_USER7                8  ///< User-defined MFD display 7
#define MFD_USER8                9  ///< User-defined MFD display 8
#define MFD_USER9               10  ///< User-defined MFD display 9
#define MFD_USER10              11  ///< User-defined MFD display 10
//@}

// ===========================================================================
/// \ingroup defines
/// \defgroup panelneighbour Panel neighbour identifiers
/// \sa oapiSwitchPanel
// ===========================================================================
//@{
#define PANEL_LEFT               0  ///< left neighbour
#define PANEL_RIGHT              1  ///< right neighbour
#define PANEL_UP                 2  ///< above neighbour
#define PANEL_DOWN               3  ///< below neighbour
//@}

// ===========================================================================
/**
 * \ingroup defines
 * \defgroup panel_redraw Panel redraw event identifiers
 *
 * These constants are used to refer to cockpit area redraw event types
 * during panel area registration and by the event handlers.
 */
// ===========================================================================
//@{
#define PANEL_REDRAW_NEVER      0x0000 ///< Don't generate redraw events
#define PANEL_REDRAW_ALWAYS     0x0001 ///< Generate event at each frame
#define PANEL_REDRAW_MOUSE      0x0002 ///< Generate event on mouse event
#define PANEL_REDRAW_INIT       0x0003 ///< Initialisation event
#define PANEL_REDRAW_USER       0x0004 ///< User-generated event
#define PANEL_REDRAW_GDI        0x1000 ///< Allow GDI access during redraw events
#define PANEL_REDRAW_SKETCHPAD  0x2000 ///< Allow Sketchpad access during redraw events
//@}


// ===========================================================================
/**
 * \ingroup defines 
 * \defgroup panel_mouse Mouse event identifiers
 *
 * These constants are used to refer to cockpit mouse event types during
 * panel area registration and by the event handlers.
 * \note PANEL_MOUSE_IGNORE and PANEL_MOUSE_ONREPLAY are used only during
 *  area registration. Areas with the PANEL_MOUSE_IGNORE attribute never
 *  generate mouse events. Areas with the PANEL_MOUSE_ONREPLAY attribute
 *  generate mouse events also during replay sessions (off by default).
 */
// ===========================================================================
//@{
#define PANEL_MOUSE_IGNORE      0x00 ///< Don't generate mouse events
#define PANEL_MOUSE_LBDOWN      0x01 ///< Left button down event
#define PANEL_MOUSE_RBDOWN      0x02 ///< Right button down event
#define PANEL_MOUSE_LBUP        0x04 ///< Left button release event
#define PANEL_MOUSE_RBUP        0x08 ///< Right button release event
#define PANEL_MOUSE_LBPRESSED   0x10 ///< Left button down (continuous)
#define PANEL_MOUSE_RBPRESSED   0x20 ///< Right button down (continuous)
#define PANEL_MOUSE_DOWN        0x03 ///< Composite down event
#define PANEL_MOUSE_UP          0x0C ///< Composite release event
#define PANEL_MOUSE_PRESSED     0x30 ///< Composite down (continous)
#define PANEL_MOUSE_ONREPLAY    0x40 ///< Create mouse events during replay
//@}

/**
 * \ingroup defines
 * \defgroup panel_map Panel area texture mapping identifiers
 *
 * The constants are used during panel area instantiations for defining
 * how the panel area texture is presented to the redraw callback function.
 * \note PANEL_MAP_NONE is the most efficient option if the area texture is completely
 *   redrawn at each redraw event.
 * \note PANEL_MAP_BGONREQUEST is more efficient than PANEL_MAP_BACKGROUND if the
 *   area texture may not need to be updated at each redraw event.
 */
//@{
#define PANEL_MAP_NONE          0x00 ///< area texture is undefined (i.e. should be completely redrawn)
#define PANEL_MAP_BACKGROUND    0x01 ///< area texture contains a copy of the panel background
#define PANEL_MAP_CURRENT       0x02 ///< area texture contains a copy of the current panel state
#define PANEL_MAP_BGONREQUEST   0x03 ///< area texture is undefined, but panel background can be requested
#define PANEL_MAP_DIRECT        0x04 ///< provide the entire input surface to redraw functions without clipping
//@}

#define PANEL_ATTACH_BOTTOM    0x0001
#define PANEL_ATTACH_TOP       0x0002
#define PANEL_ATTACH_LEFT      0x0004
#define PANEL_ATTACH_RIGHT     0x0008
#define PANEL_MOVEOUT_BOTTOM   0x0010
#define PANEL_MOVEOUT_TOP      0x0020
#define PANEL_MOVEOUT_LEFT     0x0040
#define PANEL_MOVEOUT_RIGHT    0x0080

#define SURF_NO_CK         0xFFFFFFFF
#define SURF_PREDEF_CK     0xFFFFFFFE

#define SURF_NO_ROTATION ((DWORD)-1)
#define SURF_HMIRROR     ((DWORD)-2)
#define SURF_VMIRROR     ((DWORD)-3)
#define SURF_ROTATE_90   ((DWORD)-4)
#define SURF_ROTATE_180  ((DWORD)-5)
#define SURF_ROTATE_270  ((DWORD)-6)

#define DLG_ALLOWMULTI     0x1
#define DLG_CAPTIONCLOSE   0x2
#define DLG_CAPTIONHELP    0x4

#define DLG_CB_TWOSTATE    0x1

// Custom MFD message identifiers
#define OAPI_MSG_MFD_OPENED    1
#define OAPI_MSG_MFD_CLOSED    2
#define OAPI_MSG_MFD_UPDATE    3
#define OAPI_MSG_MFD_OPENEDEX  4

// ===========================================================================
/// \ingroup defines
/// \defgroup vmsg Generic vessel message identifiers
// ===========================================================================
//@{
#define VMSG_LUAINTERPRETER    0x0001 ///< initialise Lua interpreter
#define VMSG_LUAINSTANCE       0x0002 ///< create Lua vessel instance
#define VMSG_USER              0x1000 ///< base index for user-defined messages
//@}


// ===========================================================================
/// \ingroup defines
/// \defgroup meshvis Vessel mesh visibility flags
/// These constants determine the visibility of vessel meshes in specific
/// camera modes.
/// \sa VESSEL::SetMeshVisibilityMode, VESSEL::GetMeshVisibilityMode
//@{ =========================================================================
#define MESHVIS_NEVER          0x00  ///< Mesh is never visible
#define MESHVIS_EXTERNAL       0x01  ///< Mesh is visible in external views
#define MESHVIS_COCKPIT        0x02  ///< Mesh is visible in all internal (cockpit) views
#define MESHVIS_ALWAYS         (MESHVIS_EXTERNAL|MESHVIS_COCKPIT) ///< Mesh is always visible
#define MESHVIS_VC             0x04  ///< Mesh is only visible in virtual cockpit internal views
#define MESHVIS_EXTPASS        0x10  ///< Visibility modifier: render mesh during external pass, even for internal views
//@}

#define MESHPROPERTY_MODULATEMATALPHA 1

// ===========================================================================
/// \ingroup defines
/// \defgroup navtype Navigation radio transmitter types
/// \sa oapiGetNavType
//@{ =========================================================================
#define TRANSMITTER_NONE 0
#define TRANSMITTER_VOR  1
#define TRANSMITTER_VTOL 2
#define TRANSMITTER_ILS  3
#define TRANSMITTER_IDS  4
#define TRANSMITTER_XPDR 5
//@}

const UINT ALLDOCKS = (UINT)-1;

// ===========================================================================
/// \ingroup defines
/// \defgroup objprm Object parameter flags
/// Used by oapiGetObjectParam()
// ===========================================================================
//@{
/**
 * \brief Max. resolution level for planet surface rendering.
 *   (Parameter type: DWORD)
 */
#define OBJPRM_PLANET_SURFACEMAXLEVEL    0x0001

/**
 * \brief Flag for ripple effect on reflective surfaces
 *   (Parameter type: bool)
 */
#define OBJPRM_PLANET_SURFACERIPPLE      0x0002

/**
 * \brief Bleed-in factor of atmospheric haze into planet disc.
 *   (Parameter type: double; range: 0-0.9)
 */
#define OBJPRM_PLANET_HAZEEXTENT         0x0003

/**
 * \brief Density at which the horizon haze is rendered (basic
 *   density is calculated from atmospheric density) Default: 1.0.
 *   (Parameter type: double)
 */
#define OBJPRM_PLANET_HAZEDENSITY        0x0004

#define OBJPRM_PLANET_HAZESHIFT          0x0005
#define OBJPRM_PLANET_HAZECOLOUR         0x0006
#define OBJPRM_PLANET_FOGPARAM           0x0007
#define OBJPRM_PLANET_SHADOWCOLOUR       0x0008
#define OBJPRM_PLANET_HASCLOUDS          0x0009
#define OBJPRM_PLANET_CLOUDALT           0x000A
#define OBJPRM_PLANET_CLOUDROTATION      0x000B

/**
 * \brief Depth of cloud shadows
 *   (parameter type: float)
 */
#define OBJPRM_PLANET_CLOUDSHADOWCOL     0x000C

#define OBJPRM_PLANET_CLOUDMICROTEX      0x000D
#define OBJPRM_PLANET_CLOUDMICROALTMIN   0x000E
#define OBJPRM_PLANET_CLOUDMICROALTMAX   0x000F
#define OBJPRM_PLANET_HASRINGS           0x0010
#define OBJPRM_PLANET_RINGMINRAD         0x0011
#define OBJPRM_PLANET_RINGMAXRAD         0x0012

/**
 * \brief Altitude [m] up to which an atmosphere attenuates
 *   light cast from the sun on a spacecraft.
 *   (Parameter type: double)
 */
#define OBJPRM_PLANET_ATTENUATIONALT     0x0013

/**
 * \brief Planet tile engine version (1 or 2)
 *   (Parameter type: int)
 */
#define OBJPRM_PLANET_TILEENGINE         0x0014

/**
 * \brief Planet cloud tile engine version (1 or 2)
 *   (Parameter type: int)
 */
#define OBJPRM_PLANET_CLOUDTILEENGINE    0x0015

/**
 * \brief Atmospheric tint colour. This colour is mixed into
 *   the surface textures when seen through an atmospheric layer.
 *   (Parameter type: VECTOR3)
 */
#define OBJPRM_PLANET_ATMTINTCOLOUR      0x0016

/**
 * \brief Max. resolution level for cloud layer rendering
 *   (Parameter type: int)
 */
#define OBJPRM_PLANET_CLOUDMAXLEVEL      0x0017

/**
 * \brief Enhance cloud brightness?
 *   (Parameter type: bool)
 */
#define OBJPRM_PLANET_CLOUDOVERSATURATE  0x0018

/**
 * \brief Extend horizon visibility radius (avoid disappearing
 *    mountaintop artefacts)
 *   (Parameter type: double)
 */
#define OBJPRM_PLANET_HORIZONEXCESS      0x0019

/**
 * \brief Extend tile bounding box (avoid disappearing tiles
 *   for irregular shaped bodies)
 *   (Parameter type: double)
 */
#define OBJPRM_PLANET_TILEBBEXCESS       0x001A

/**
 * \brief Minimum planet elevation [m] relative to mean radius.
 *   Used to adjust lower horizon edge for rendering
 *   (Parameter type: double)
 */
#define OBJPRM_PLANET_MINELEVATION		 0x001B

/**
 * \brief Target resolution for elevation data [m]
 *   Elevation data loaded from file are rescaled to this resolution
 *   (Parameter type: double)
 */
#define OBJPRM_PLANET_ELEVRESOLUTION     0x001D

/**
 * \brief Planet surface label engine version (1 or 2)
 *   (Parameter type: int)
 */
#define OBJPRM_PLANET_LABELENGINE        0x001E

 /**
  * \brief Maximum planet elevation [m] relative to mean radius.
  *   (Parameter type: double)
  */
#define OBJPRM_PLANET_MAXELEVATION		 0x001F

//@}

typedef int (*KeyFunc)(const char *keybuf);


// ======================================================================
/** \class LaunchpadItem
  * \brief Base class to define launchpad items.
  * \details LaunchpadItem is the base class for objects that can be inserted into the parameter list of the
  * Extra tab of the Orbiter Launchpad dialog. The Extra tab provides a mechanism for plugin
  * modules to allow users to set global parameters specific to an addon. LaunchpadItem is
  * notified whenever the user selects the item from the list, and when parameters need to be
  * read from or written to disk.
  * \sa oapiRegisterLaunchpadItem, oapiUnregisterLaunchpadItem
*/
// ======================================================================

class OAPIFUNC LaunchpadItem {
public:

	/** \brief Constructor. Creates a new launchpad item. */
	LaunchpadItem ();

	/** \brief Destructor. Destroys the launchpad item. */
	virtual ~LaunchpadItem ();

	/**
	* \brief Derived classes should return a pointer to the string to appear in the Launchpad "Extra" list.
	* \return Pointer to the item label in the list.
	* \n\n <b>Default action:</b> Returns NULL (no entry in the list).
	*/
	virtual char *Name ();

	/**
	* \brief Derived classes should return a pointer to the the string containing a description of the
	*  item. The description is shown next to the Launchpad list whenever the item is selected.
	* \return Pointer to the descriptive string, or NULL if there is none.
	* \n\n <b>Default action:</b> Returns NULL (no description).
	* \note Line breaks can be inserted into the description with a carriage
	*  return/newline sequence (\\r\\n).
	*/
	virtual char *Description ();
	
	/**
	* \brief Opens a dialog box associated with the launchpad item.
	* \param hInst module instance handle
	* \param hLaunchpad launchpad window handle
	* \param resId integer resource ID of the dialog box
	* \param pDlg dialog box message handler
	* \return Currently this function always returns \e true.
	* \note This function is usually called in the body of LaunchpadItem::clbkOpen().
	* \note It is an alternative to the standard Windows DialogBox function. It has the
	*  advantage that a pointer to the LaunchpadItem instance is passed as lParam
	*  to the message handler with the \c WM_INITDIALOG message. In all
	*  subsequent calls to the handler, the LaunchpadItem instance pointer can be
	*  obtained with a call to <i>GetWindowLongPtr (hWnd, DWLP_USER)</i>, where hWnd
	*  is the dialog box handle passed to the message handler.
	*/
	virtual bool OpenDialog (HINSTANCE hInst, HWND hLaunchpad, int resId, DLGPROC pDlg);

	/**
	* \brief This method is called whenever the user opens the item by double-clicking on the list or
	*  clicking the "Edit" button below the list.
	* \param hLaunchpad The window handle of the Launchpad dialog
	* \return Currently ignored. Should be \e true if the derived class processes this callback function.
	* \n\n <b>Default action:</b> Nothing; returns false.
	* \note The derived class can use this function to open a dialog box or some other
	*  means of allowing the user to set addon-specific parameters.
	*/
	virtual bool clbkOpen (HWND hLaunchpad);

	/**
	* \brief This method is called whenever the item should write its current state to a file.
	* \return Currently ignored. Should be 0.
	* \n\n<b>Default action:</b> Nothing; returns 0.
	* \note This function is called before a simulation session is launched, before Orbiter
	*  shuts down, and before the module is deactivated. It allows the module to
	*  write its current state to a file, so it can re-load its settings the next time
	*  Orbiter is launched.
	* \note You can either use default C or C++ methods to open a file for output, or you
	*  can use the oapiOpenFile() method.
	* \note Modules should never write to the global Orbiter.cfg configuration file. Any
	*  addons that are not active when Orbiter overwrites Orbiter.cfg will lose their
	*  settings, since their clbkWriteConfig() method cannot be called.
	* \note The best place to read the settings stored during a previous session is in the
	*  overloaded LaunchpadItem constructor. Use oapiOpenFile or another file
	*  access method compatible with the way the file was written. The parameter
	*  settings should then be stored in class member variables, and modified by
	*  user interaction.
	*/
	virtual int clbkWriteConfig ();

	LAUNCHPADITEM_HANDLE hItem;
};


// ======================================================================
// class CameraMode and subclasses
// ======================================================================

class OAPIFUNC CameraMode {
friend class Camera;
public:
	CameraMode ();
	static CameraMode *Create (char *str);
	virtual void Init (char *str) = 0;
	virtual void Store (char *str) = 0;
	enum Mode { CM_COCKPIT, CM_TRACK, CM_GROUND };
	OBJHANDLE GetTarget() const { return target; }
	virtual Mode GetMode() const = 0;
	virtual void GetDescr (char *str, int len) = 0;
	void SetTarget (OBJHANDLE hTgt);
	void SetFOV (double FOV);
	double GetFOV () const { return fov; }

protected:
	OBJHANDLE target;  // camera target
	double fov;        // field of view [deg]
};

class OAPIFUNC CameraMode_Cockpit: public CameraMode {
public:
	CameraMode_Cockpit ();
	void Init (char* str = 0);
	void Store (char *str);
	Mode GetMode() const { return CM_COCKPIT; }
	void GetDescr (char *str, int len);
	enum CockpitMode { CM_CURRENT, CM_GENERIC, CM_PANEL2D, CM_VC };
	CockpitMode GetCockpitMode () const { return cmode; }
	int GetPosition () const { return pos; }
	int GetLeaning() const { return lean; }
	bool GetLeaningSmooth() const { return lean_smooth; }

protected:
	CockpitMode cmode;
	int pos;
	int lean;
	bool lean_smooth;
};

class OAPIFUNC CameraMode_Track: public CameraMode {
public:
	CameraMode_Track ();
	void Init (char *str);
	void Store (char *str);
	Mode GetMode() const { return CM_TRACK; }
	void GetDescr (char *str, int len);
	enum TrackMode { TM_CURRENT, TM_RELATIVE, TM_ABSDIR, TM_GLOBAL, TM_TARGETTOREF, TM_TARGETFROMREF };
	TrackMode GetTrackMode () const { return tmode; }
	void SetTrackMode (TrackMode trackmode, OBJHANDLE refobj = 0);
	OBJHANDLE GetRef() const { return ref; }
	void SetPosition (double rd, double ph, double th);
	void GetPosition (double *rd, double *ph, double *th) const;

protected:
	TrackMode tmode;   // camera track mode
	double reldist;    // distance camera-targets (in units of target size)
	double phi, theta; // camera angle [rad]
	OBJHANDLE ref;     // reference object (for TM_TARGETTOREF and TM_TARGETFROMREF only)
};

class OAPIFUNC CameraMode_Ground: public CameraMode {
public:
	CameraMode_Ground ();
	void Init (char *str);
	void Store (char *str);
	Mode GetMode () const { return CM_GROUND; }
	void GetDescr (char *str, int len);
	void SetPosition (double longitude, double latitude, double altitude, OBJHANDLE hRef = 0);
	void GetPosition (double *longitude, double *latitude, double *altitude, OBJHANDLE *hRef) const;
	void SetOrientation (double ph, double th);
	void GetOrientation (double *ph, double *th) const;
	bool GetTgtLock () const { return tgtlock; }
	bool GetAltMode () const { return alt_above_ground; }
	OBJHANDLE GetRef() const { return ref; }

protected:
	OBJHANDLE ref;
	double lng, lat;       // camera position on the ground [rad]
	double alt;            // camera height over ground [m]
	double phi, theta;     // camera direction (free mode only)
	bool alt_above_ground; // true: alt refers to altitude above local terrain elevation; false: alt refers to mean planet radius
	bool tgtlock;          // flag for target lock/free mode
};


// ======================================================================
/**
 * \defgroup oapi Orbiter API interface methods
 *
 * The functions in this section provide a general framework to retrieve and set
 * Orbiter simulation parameters from an addon module.
 * For a linear list of oapi functions, constants and enumerations, see
 * \ref OrbiterAPI.h.
 * For vessel-specific parameters see also the \ref VESSEL class.
 */
// ======================================================================
//@{
/**
 * \brief Returns the version number of the Orbiter core system.
 * \return version number
 * \note Orbiter version numbers are derived from the build date.
 *   The version number is constructed as
 *   (year%100)*10000 + month*100 + day, resulting in a decimal
 *   version number of the form YYMMDD
 * \sa oapiGetModuleVersion
 */
OAPIFUNC int oapiGetOrbiterVersion ();

/**
 * \brief Returns the API version number against which the module
 *   was linked.
 * \return module version number
 * \note Orbiter version numbers are derived from the build date.
 *   The version number is constructed as
 *   (year%100)*10000 + month*100 + day, resulting in a decimal
 *   version number of the form YYMMDD
 * \sa oapiGetOrbiterVersion
 */
int oapiGetModuleVersion ();

/**
 * \brief Returns the instance handle for the running Orbiter application.
 * \return Orbiter instance handle
 */
OAPIFUNC HINSTANCE oapiGetOrbiterInstance ();

/**
 * \brief Returns a pointer to the command line with which Orbiter was invoked.
 * \return Pointer to orbiter command line string.
 * \note This method can be used to pass custom parameters to a module directly
 *  from the orbiter command line.
 */
OAPIFUNC const char *oapiGetCmdLine ();

/**
 * \brief Returns the dimensions of the render viewport.
 * \param w pointer to viewport width [pixel]
 * \param h pointer to viewport height [pixel]
 * \param bpp pointer to colour depth [bits per pixel]
 * \note This function writes the viewport width, height and (optionally)
 *   colour depth values into the variables pointed to by the function
 *   parameters.
 * \note For fullscreen modes, the viewport size corresponds to the
 *   fullscreen resolution. For windowed modes, the viewport size corresponds
 *   to the client area of the render window.
 */
OAPIFUNC void oapiGetViewportSize (DWORD *w, DWORD *h, DWORD *bpp = 0);

/**
 * \brief Register a module interface class instance
 *
 * Plugin modules that use an interface class instance derived
 * from oapi::Module must register it with this function during module
 * initialisation, in the body of InitModule.
 * \param module pointer to the interface class instance
 * \note The DLL must delete the oapi::Module instance in the body of
 *   ExitModule.
 */
OAPIFUNC void oapiRegisterModule (oapi::Module *module);

/**
 * \brief Returns a pointer to a string which will be displayed in the lower left corner of the viewport.
 * \return Pointer to debugging string.
 * \note This function should only be used for debugging purposes. 
 *  Do not use it in published modules!
 * \note The returned pointer refers to a global char[256] in the Orbiter core. It is the
 *  responsibility of the module to ensure that no overflow occurs.
 * \note If the string is written to more than once per time step (either within a single
 *  module or by multiple modules) the last state before rendering will be displayed.
 * \note A typical use would be:
 * \code
 *  sprintf (oapiDebugString(), "my value is %f", myvalue);
 * \endcode
 */
OAPIFUNC char *oapiDebugString ();


/**
* \brief Print multiple debug strings onto a screen, will be cleared when printed on screen.
* \param str Text string to print
*/
OAPIFUNC void oapiDebugString(const char*);


// ======================================================================
/**
 * \defgroup ObjectAccess Object access functions
 */
// ======================================================================
//@{

/**
 * \brief Returns a handle for a named simulation object.
 * \param name object name
 * \return object handle
 * \note Objects can be vessels, planets, moons or suns.
 * \note A return value of NULL indicates that the object was not found.
 * \note The name is not case-sensitive ("Jupiter" will also match "jupiter" or
 *   "JUPITER").
 * \note Surface base handles cannot be retrieved with this method, because a planet
 *   handle is required in addition to the base name to uniquely identify the base.
 *   Use oapiGetBaseByName() or oapiGetBaseByIndex() instead.
 * \sa oapiGetObjectByIndex, oapiGetVesselByName, oapiGetGbodyByName,
 *   oapiGetBaseByName, oapiGetObjectType
 */
OAPIFUNC OBJHANDLE oapiGetObjectByName (char *name);

/**
 * \brief Returns a handle for an indexed simulation object.
 * \param index object index (0 <= index < oapiGetObjectCount())
 * \return object handle
 * \note Objects can be created and deleted during a simulation session. Therefore
 *   the list index of a given object and the range of valid list indices can
 *   change.
 * \note A typical use for accessing objects by index is in a loop running over all
 *   present objects:
 *   \code
 * for (int i = 0; i < oapiGetObjectCount(); i++) {
 *    OBJHANDLE hObj = oapiGetObjectByIndex (i);
 *    // do something with hObj
 * }
 *   \endcode
 * \sa oapiGetObjectByName, oapiGetObjectType
 */
OAPIFUNC OBJHANDLE oapiGetObjectByIndex (int index);

/**
 * \brief Returns the number of objects currently present in the simulation.
 * \return object count
 * \sa oapiGetObjectByIndex, oapiGetObjectType
 */
OAPIFUNC DWORD oapiGetObjectCount ();

/**
 * \brief Returns the type of an object identified by its handle.
 * \param hObj object handle
 * \return Integer code identifying the vessel type.
 * \note The following type identifiers are currently supported:
 *   <table col=2>
 *   <tr><td>OBJTP_INVALID</td><td>invalid object handle</td></tr>
 *   <tr><td>OBJTP_GENERIC</td><td>generic object (not currently used)</td></tr>
 *   <tr><td>OBJTP_CBODY</td><td>generic celestial body (not currently used</td></tr>
 *   <tr><td>OBJTP_STAR</td><td>star</td></tr>
 *   <tr><td>OBJTP_PLANET</td><td>planet (used for all celestial bodies that are not stars,
 *       including moons, comets, etc.)</td></tr>
 *   <tr><td>OBJTP_VESSEL</td><td>vessel (spacecraft, space stations, etc.)</td></tr>
 *   <tr><td>OBJTP_SURFBASE</td><td>surface base (spaceport)</td></tr>
 *   </table>
 * \note This function searches through Orbiter's object list to determine if the
 *   provided object handle is valid. It scales with O(n) in the presence of n objects.
 * \sa oapiGetObjectParam, oapiGetObjectCount
 */
OAPIFUNC int oapiGetObjectType (OBJHANDLE hObj);

/**
 * \brief Returns an object-specific configuration parameter.
 * \param hObj object handle
 * \param paramtype parameter identifier (see \ref objprm)
 * \return pointer to parameter value
 * \note This function returns the current value of a configuration
 *   parameter for a given object (e.g. planet).
 * \note The type of the return value depends on the parameter. The generic
 *   void pointer must be cast into the appropriate parameter type.
 *   Example:
 *   \code
 *   bool *bClouds = (bool*)oapiGetObjectParam (hObj, OBJPRM_PLANET_HASCLOUDS);
 *   \endcode
 * \sa objprm
 */
OAPIFUNC const void *oapiGetObjectParam (OBJHANDLE hObj, DWORD paramtype);

/**
 * \brief Returns the handle of a vessel identified by its name.
 * \param name vessel name (not case-sensitive)
 * \return Vessel object handle, or NULL if the vessel could not be found.
 * \sa oapiGetVesselByIndex
 */
OAPIFUNC OBJHANDLE oapiGetVesselByName (char *name);

/**
 * \brief Returns the handle of a vessel identified by its reference index.
 * \param index object index (0 <= index < oapiGetVesselCount())
 * \return Vessel object handle, or NULL if index out of range.
 * \note The index of a vessel can change during the simulation if vessels
 *   are created or destroyed. A typical use for oapiGetVesselByIndex()
 *   would be to implement a loop over all vessels:
 *   \code
 *   for (i = 0; i < oapiGetVesselCount(); i++) {
 *     OBJHANDLE hVessel = oapiGetVesselByIndex (i);
 *     // do something with hVessel
 *   }
 *   \endcode
 * \sa oapiGetVesselByName, oapiGetVesselCount
 */
OAPIFUNC OBJHANDLE oapiGetVesselByIndex (int index);

/**
 * \brief Returns the number of vessels currently present in the simulation.
 * \return Vessel count.
 * \sa oapiGetVesselByIndex
 */
OAPIFUNC DWORD oapiGetVesselCount ();

/**
 * \brief Checks if the specified handle is a valid vessel handle.
 * \param hVessel handle to be tested
 * \return \e true if \e hVessel is a valid vessel handle, \e false otherwise.
 * \note This function can be used to test if a previously obtained vessel handle
 *   is still valid. A handle becomes invalid if the associated vessel is deleted.
 * \note An alternative to using oapiIsVessel() is monitoring vessel deletions by
 *   implementing the oapi::Module::clbkDeleteVessel() callback function of the
 *   module instance.
 * \sa oapiGetObjectType
 */
OAPIFUNC bool oapiIsVessel (OBJHANDLE hVessel);

/**
 * \brief Returns the handle of a celestial body (sun, planet or moon) identified
 *   by its name.
 * \param name celestial object name (not case-sensitive)
 * \return Object handle, or NULL if the object could not be found.
 * \note Celestial bodies in orbiter are objects that act as sources for
 *   gravitational fields.
 * \sa oapiGetGbodyByIndex
 */
OAPIFUNC OBJHANDLE oapiGetGbodyByName (char *name);

/**
 * \brief Returns the handle of a celestial body (sun, planet or moon) indentified
 *   by its list index.
 * \param index object index (0 <= index < oapiGetGbodyCount())
 * \return Object handle, or NUL if index out of range.
 * \sa oapiGetGbodyCount, oapiGetGbodyByName
 */
OAPIFUNC OBJHANDLE oapiGetGbodyByIndex (int index);

/**
 * \brief Returns the parent object of a celestial body.
 *
 * The parent is the body being orbited by hBody, e.g. the central star if
 *   hBody is a planet, or the planet if hBody is a moon.
 * \param hBody celestial body handle
 * \return parent body handle or NULL if no parent.
 * \note hBody must refer to a celestial body (type = OBJTP_PLANET or OBJTP_STAR),
 *   otherwise the result is undefined.
 */
OAPIFUNC OBJHANDLE oapiGetGbodyParent (OBJHANDLE hBody);

/**
 * \brief Returns a child object of a celestial body.
 *
 * The children are the objects orbiting hBody, e.g. planets orbiting the
 *  central star, or moons orbiting a planet.
 * \param hBody celestial body handle
 * \param index child index (>= 0)
 * \return child body handle or NULL if requested child doesn't exist
 * \note hBody must refer to a celestial body (type = OBJTP_PLANET or OBJTP_STAR),
 *   otherwise the result is undefined.
 */
OAPIFUNC OBJHANDLE oapiGetGbodyChild (OBJHANDLE hBody, DWORD index);

/**
 * \brief Returns the number of celestial bodies (sun, planets and moons) currently
 *   present in the simulation.
 * \return Number of objects.
 */
OAPIFUNC DWORD oapiGetGbodyCount ();

/**
 * \brief Returns the handle of a surface base on a given planet or moon.
 * \param hPlanet handle of planet or moon on which the base is located
 * \param name base name (not case-sensitive)
 * \return Base object handle, or NULL if base was not found.
 * \sa oapiGetBaseByIndex, oapiGetBasePlanet
 */
OAPIFUNC OBJHANDLE oapiGetBaseByName (OBJHANDLE hPlanet, char *name);

/**
 * \brief Returns the handle of a surface base on a planet or moon given
 *   by its list index.
 * \param hPlanet handle of the planet or moon on which the base is located
 * \param index list index (0 <= index < oapiGetBaseCount(hPlanet))
 * \return Base object handle, or NULL if index out of range.
 * \sa oapiGetBaseCount, oapiGetBaseByName, oapiGetBasePlanet
 */
OAPIFUNC OBJHANDLE oapiGetBaseByIndex (OBJHANDLE hPlanet, int index);

/**
 * \brief Returns the number of surface bases defined for a given planet.
 * \param hPlanet handle of a planet or moon
 * \return Number of surface bases (>= 0).
 */
OAPIFUNC DWORD oapiGetBaseCount (OBJHANDLE hPlanet);

/**
 * \brief Returns the name of an object.
 * \param hObj object handle
 * \param name pointer to character array to receive object name
 * \param n length of character array
 * \note \e name must be allocated to at least size \e n by the calling function.
 * \note If the string buffer is not long enough to hold the object name, the name
 *   is truncated.
 */
OAPIFUNC void oapiGetObjectName (OBJHANDLE hObj, char *name, int n);

/**
* \brief Returns the configuration filename for an object or NULL is no configuration file is used.
* \param hObj object handle
*/
OAPIFUNC const char* oapiGetObjectFileName(OBJHANDLE hObj);

/**
 * \brief Returns the handle for the current focus object.
 * \return Focus object handle
 * \note The focus object is the user-controlled vessel which receives keyboard and
 *   joystick input.
 * \note This function returns a valid vessel handle during a simulation session
 *   (between oapi::Module::clbkSimulationStart() and oapi::Module::clbkSimulationEnd())
 * \sa oapiSetFocusObject
 */
OAPIFUNC OBJHANDLE oapiGetFocusObject ();

/**
 * \brief Switches the input focus to a different vessel object.
 * \param hVessel handle of vessel to receive input focus
 * \return Handle of vessel losing focus, or NULL if focus did not change.
 * \note \e hVessel must refer to a vessel object. Trying to set the focus to a
 *   different object type will fail.
 * \sa oapiGetFocusObject
 */
OAPIFUNC OBJHANDLE oapiSetFocusObject (OBJHANDLE hVessel);

/**
 * \brief Returns a VESSEL class instance for a vessel.
 * \param hVessel vessel handle
 * \return Pointer to an instance of the VESSEL class or a derived class, providing
 *   an interface for access to the specified vessel.
 */
OAPIFUNC VESSEL *oapiGetVesselInterface (OBJHANDLE hVessel);

/**
 * \brief Returns the VESSEL class instance for the current focus object.
 * \return Pointer to an instance of the VESSEL class or a derived class, providing
 *   an interface for access to the current focus object.
 */
OAPIFUNC VESSEL *oapiGetFocusInterface ();

/**
 * \brief Returns a CELBODY interface instance for a celestial body, if
 *   available.
 * \param hBody handle of a celestial body
 * \return Pointer to the CELBODY class instance for the body, or NULL if
 *   the body is not controlled by an external module.
 * \note hBody must be a valid handle for a celestial body (star, planet,
 *   moon, etc.), e.g. as obtained from \ref oapiGetGbodyByName. Passing
 *   a handle of any other type will result in undefined behaviour.
 * \note Only celestial bodies controlled by external plugin modules have
 *   access to a CELBODY instance. Celestial bodies that are updated
 *   internally by Orbiter (e.g. using 2-body orbital elements, or dynamic
 *   updates) return NULL here.
 */
OAPIFUNC CELBODY *oapiGetCelbodyInterface (OBJHANDLE hBody);
//@}

// ======================================================================
/// \defgroup VesselCreation Vessel creation and destruction
// ======================================================================
//@{

/**
 * \brief Creates a new vessel.
 * \param name vessel name
 * \param classname vessel class name
 * \param status initial vessel status
 * \return Handle of the new vessel.
 * \note A configuration file for the specified vessel class must exist
 *   in the Config or Config/Vessels subdirectory.
 * \note \ref oapiCreateVesselEx is an extended version of this function
 *   operating on a more versatile status structure.
 * \sa oapiCreateVesselEx, VESSELSTATUS
 */
OAPIFUNC OBJHANDLE oapiCreateVessel (const char *name, const char *classname, const VESSELSTATUS &status);

/**
 * \brief Creates a new vessel via a VESSELSTATUSx (x >= 2) interface.
 * \param name vessel name
 * \param classname vessel class name
 * \param status pointer to a VESSELSTATUSx structure
 * \return Handle of the new vessel.
 * \note A configuration file for the specified vessel class must exist in
 *   the Config or the Config\\Vessels folder, or a subfolder.
 *   If the config file is located in a subfolder, the relative path must
 *   be included in the \e classname parameter.
 * \note \e status must point to a VESSELSTATUSx structure. Currently only
 *   \ref VESSELSTATUS2 is supported, but future Orbiter versions may add
 *   new interfaces.
 * \note During the vessel creation process Orbiter will call the module's
 *   \ref VESSEL2::clbkSetStateEx callback function if it exists.
 * \sa oapiCreateVessel, VESSEL2::clbkSetStateEx, VESSELSTATUS2
 */
OAPIFUNC OBJHANDLE oapiCreateVesselEx (const char *name, const char *classname, const void *status);

/**
 * \brief Deletes an existing vessel.
 * \param hVessel vessel handle
 * \param hAlternativeCameraTarget optional new camera target
 * \return \e true if vessel could be deleted.
 * \note If the current focus vessel is deleted, Orbiter will switch
 *   focus to the closest focus-enabled vessel. If the last focus-enabled
 *   vessel is deleted, Orbiter returns to the launchpad.
 * \note If the current camera target is deleted, a new camera target can
 *   be provided in \e hAlternativeCameraTarget. If not specified, the
 *   focus object is used as default camera target.
 * \note The actual vessel destruction does not occur until the end of
 *   the current frame. Self-destruct calls are therefore permitted.
 * \note A vessel will undock all its docking ports before being destructed.
 * \sa oapiCreateVessel, oapiCreateVesselEx
 */
OAPIFUNC bool oapiDeleteVessel (OBJHANDLE hVessel, OBJHANDLE hAlternativeCameraTarget = 0);
//@}

/**
 * \brief Returns the global position of the barycentre of a complete planetary system or a single
 *  planet-moons system.
 * \param hObj celestial body handle
 * \param bary pointer to vector receiving barycentre data
 * \note The barycentre is the centre of mass of a distribution of objects. In this case,
 *  all involved celestial bodies are considered point masses, and the barycentre
 *  is defined as
 * \f[
 * \vec{r}_B = \left(\sum_i m_i\right)^{-1} \sum_i m_i \vec{r_i} 
 * \f]
 * \note hObj must be the handle of a celestial body.
 * \note The summation involves the body itself and all its secondaries, e.g. a planet and its moons.
 * \note The barycentre of a star (0th level object) is always the origin (0,0,0).
 * \note The barycentre of an object without associated secondaries is identical to its position.
 */
OAPIFUNC void oapiGetBarycentre (OBJHANDLE hObj, VECTOR3 *bary);


// ======================================================================
/// \defgroup oapi_body Body functions
// ======================================================================
//@{

/**
 * \brief Returns the size (mean radius) of an object.
 * \param hObj object handle
 * \return Object size (mean radius) in meter.
 */
OAPIFUNC double oapiGetSize (OBJHANDLE hObj);

/**
 * \brief Returns the mass of an object. For vessels, this is the total mass, including current fuel mass.
 * \param hObj object handle
 * \return object mass [kg]
 * \sa oapiGetMaxFuelMass, oapiGetEmptyMass
 */
OAPIFUNC double oapiGetMass (OBJHANDLE hObj);
	
/**
 * \brief Returns the position of an object in the global reference frame.
 * \param hObj object handle
 * \param pos pointer to vector receiving coordinates
 * \note The global reference frame is the heliocentric ecliptic system at ecliptic and
 *  equinox of J2000.
 * \note Units are meters.
 * \sa oapiGetBarycentre, oapiGetGlobalVel
 */
OAPIFUNC void oapiGetGlobalPos (OBJHANDLE hObj, VECTOR3 *pos);

/**
 * \brief Returns the velocity of an object in the global reference frame.
 * \param hObj object handle
 * \param vel pointer to vector receiving velocity data
 * \note The global reference frame is the heliocentric ecliptic system at ecliptic and
 *  equinox of J2000.
 * \note Units are meters/second.
 * \sa oapiGetBarycentre, oapiGetGlobalPos
 */
OAPIFUNC void oapiGetGlobalVel (OBJHANDLE hObj, VECTOR3 *vel);

/**
 * \brief Returns the distance vector from hRef to hObj in the ecliptic reference frame.
 * \param hObj object handle
 * \param hRef reference object handle
 * \param pos pointer to vector receiving distance data
 * \note Results are w.r.t. ecliptic frame at equinox and ecliptic of J2000.0.
 * \sa oapiGetBarycentre, oapiGetRelativeVel
 */
OAPIFUNC void oapiGetRelativePos (OBJHANDLE hObj, OBJHANDLE hRef, VECTOR3 *pos);

/**
 * \brief Returns the velocity difference vector of hObj relative to hRef in the ecliptic reference frame.
 * \param hObj object handle
 * \param hRef reference object handle
 * \param vel pointer to vector receiving velocity difference data
 * \note Results are w.r.t. ecliptic frame at equinox and ecliptic of J2000.0.
 * \sa oapiGetBarycentre, oapiGetRelativePos
 */
OAPIFUNC void oapiGetRelativeVel (OBJHANDLE hObj, OBJHANDLE hRef, VECTOR3 *vel);

//@}


// ======================================================================
/// \defgroup oapi_vessel Vessel functions
// ======================================================================
//@{

/**
 * \brief Returns empty mass of a vessel, excluding fuel.
 * \param hVessel vessel handle
 * \return empty vessel mass [kg]
 * \note hVessel must be a vessel handle. Other object types are invalid.
 * \note Do not rely on a constant empty mass. Structural changes (e.g. discarding a
 *  rocket stage) will affect the empty mass.
 * \note For multistage configurations, the fuel mass of all currently inactive stages
 *  contributes to the empty mass. Only the fuel mass of active stages is excluded.
 */
OAPIFUNC double oapiGetEmptyMass (OBJHANDLE hVessel);

/**
 * \brief Set the empty mass of a vessel (excluding fuel)
 * \param hVessel vessel handle
 * \param mass empty mass [kg]
 * \note Use this function to register structural mass changes, for example as a result
 *  of jettisoning a fuel tank, etc.
 */
OAPIFUNC void oapiSetEmptyMass (OBJHANDLE hVessel, double mass);

/**
 * \brief Returns current fuel mass of the first propellant resource of a vessel.
 * \param hVessel vessel handle
 * \return Current fuel mass [kg]
 * \note This function is equivalent to
 * \code oapiGetPropellantMass (oapiGetPropellantHandle (hVessel, 0)) \endcode
 * \note hVessel must be a vessel handle. Other object types are invalid.
 * \note For multistage configurations, this returns the current fuel mass of active stages only.
 * \sa oapiGetMaxFuelMass, oapiGetEmptyMass
 */
OAPIFUNC double oapiGetFuelMass (OBJHANDLE hVessel);

/**
 * \brief Returns maximum fuel capacity of the first propellant resource of a vessel.
 * \param hVessel vessel handle
 * \return Maximum fuel mass [kg]
 * \note This function is equivalent to
 * \code oapiGetPropellantMaxMass (oapiGetPropellantHandle (hVessel, 0)) \endcode
 * \note hVessel must be a vessel handle. Other object types are invalid.
 * \note For multistage configurations, this returns the sum of the max fuel mass of active stages only.
 */
OAPIFUNC double oapiGetMaxFuelMass (OBJHANDLE hVessel);
	
/**
 * \brief Returns an identifier of a vessel's propellant resource.
 * \param hVessel vessel handle
 * \param idx propellant resource index (>=0)
 * \return propellant resource id, or NULL if idx >= # propellant resources
 */
OAPIFUNC PROPELLANT_HANDLE oapiGetPropellantHandle (OBJHANDLE hVessel, DWORD idx);

/**
 * \brief Returns the current fuel mass [kg] of a propellant resource.
 * \param ph propellant resource identifier
 * \return current fuel mass [kg] of the resource.
 * \sa oapiGetPropellantMaxMass, oapiGetPropellantHandle
 */
OAPIFUNC double oapiGetPropellantMass (PROPELLANT_HANDLE ph);

/**
 * \brief Returns the maximum capacity [kg] of a propellant resource.
 * \param ph propellant resource identifier
 * \return maximum fuel capacity [kg] of the resource.
 * \sa oapiGetPropellantHandle, VESSEL::GetPropellantMaxMass
 */
OAPIFUNC double oapiGetPropellantMaxMass (PROPELLANT_HANDLE ph);

/**
 * \brief Returns a handle to a vessel docking port.
 * \param hVessel vessel handle
 * \param n docking port index (>= 0)
 * \return docking port handle, or NULL if index is out of range
 * \sa VESSEL::GetDockHandle
 */
OAPIFUNC DOCKHANDLE oapiGetDockHandle (OBJHANDLE hVessel, UINT n);

/**
 * \brief Returns the handle of a vessel docked at a port.
 * \param dock docking port handle
 * \return Handle of docked vessel, or NULL if no vessel is docked at the port.
 * \sa oapiGetDockHandle, VESSEL::GetDockStatus
 */
OAPIFUNC OBJHANDLE oapiGetDockStatus (DOCKHANDLE dock);

/**
* \brief Set a docking port to auto capture when in close proximity with some other docking port.
* \param hDock docking port handle
* \param enable Enable or disable auto capture
* \note Auto capture is enabled by default.
*/
OAPIFUNC void oapiSetAutoCapture(DOCKHANDLE hDock, bool enable);

/**
* \brief Get the vessel a docking port belongs to.
* \param hDock dock handle
* \return Vessel owning the docking port.
*/
OAPIFUNC OBJHANDLE oapiGetDockOwner(DOCKHANDLE hDock);

/**
 * \brief Returns the position of the current focus object in the global reference frame.
 * \param pos pointer to vector receiving coordinates
 * \note The global reference frame is the heliocentric ecliptic system at ecliptic and
 *  equinox of J2000.0.
 * \note Units are meters.
 * \sa oapiGetFocusGlobalVel
 */
OAPIFUNC void oapiGetFocusGlobalPos (VECTOR3 *pos);

/**
 * \brief Returns the velocity of the current focus object in the global reference frame.
 * \param vel pointer to vector receiving velocity data
 * \note The global reference frame is the heliocentric ecliptic system at ecliptic and
 *  equinox of J2000.
 * \note Units are meters/second.
 * \sa oapiGetFocusGlobalPos
 */
OAPIFUNC void oapiGetFocusGlobalVel (VECTOR3 *vel);

/**
 * \brief Returns the distance vector from hRef to the current focus object.
 * \param hRef reference object handle
 * \param pos pointer to vector receiving distance data
 * \note Results are w.r.t. ecliptic frame at equinox and ecliptic of J2000.0.
 * \sa oapiGetFocusRelativeVel
 */
OAPIFUNC void oapiGetFocusRelativePos (OBJHANDLE hRef, VECTOR3 *pos);

/**
 * \brief Returns the velocity difference vector of the current focus object relative to hRef.
 * \param hRef reference object handle
 * \param vel pointer to vector receiving velocity difference data
 * \note Results are w.r.t. ecliptic frame at equinox and ecliptic of J2000.0.
 * \sa oapiGetFocusRelativePos
 */
OAPIFUNC void oapiGetFocusRelativeVel (OBJHANDLE hRef, VECTOR3 *vel);

/**
 * \brief Altitude mode used by altitude get functions.
 */
enum AltitudeMode {
	ALTMODE_MEANRAD,  ///< altitude over mean radius
	ALTMODE_GROUND    ///< altitude over ground
};

/**
 * \brief Returns the altitude of a vessel over a planet mean radius.
 * \param hVessel vessel handle
 * \param alt pointer to variable receiving altitude value
 * \return Error flag (\e false on failure)
 * \note Unit is meter [m]
 * \note Returns altitude above closest planet.
 * \note Altitude is measured above mean planet radius (as defined by SIZE
 *  parameter in planet's cfg file)
 * \note The handle passed to the function must refer to a vessel.
 * \sa oapiGetAltitude(OBJHANDLE,AltitudeMode,double*), VESSEL::GetAltitude
 */
OAPIFUNC BOOL oapiGetAltitude (OBJHANDLE hVessel, double *alt);

/**
 * \brief Returns the altitude of a vessel over a planetary surface.
 * \param hVessel vessel handle
 * \param mode altitude mode
 * \param alt pointer to variable receiving altitude value
 * \return Error flag (\e false on failure)
 * \note Unit is meter [m]
 * \note Returns altitude above closest planet.
 * \note If mode==ALTMODE_MEANRAD, the function returns the altitude above/below
 *   the planet mean radius and is equivalent to \ref oapiGetAltitude(OBJHANDLE,double*).
 *   If mode==ALTMODE_GROUND, the altitude above the local ground elevation is returned.
 * \note The handle passed to the function must refer to a vessel.
 * \sa oapiGetAltitude(OBJHANDLE,double*), VESSEL::GetAltitude
 */
OAPIFUNC BOOL oapiGetAltitude (OBJHANDLE hVessel, AltitudeMode mode, double *alt);

/**
 * \brief Returns a vessel's pitch angle w.r.t. the local horizon.
 * \param hVessel vessel handle
 * \param pitch pointer to variable receiving pitch value
 * \return Error flag (\e false on failure)
 * \note Unit is radian [rad]
 * \note Returns pitch angle w.r.t. closest planet
 * \note The local horizon is the plane whose normal is defined by the distance
 *  vector from the planet centre to the vessel.
 * \note The handle passed to the function must refer to a vessel.
 * \sa oapiGetHeading, oapiGetBank, oapiGetAltitude
 */
OAPIFUNC BOOL oapiGetPitch (OBJHANDLE hVessel, double *pitch);

/**
 * \brief Returns a vessel's bank angle w.r.t. the local horizon.
 * \param hVessel vessel handle
 * \param bank pointer to variable receiving bank value
 * \return Error flag (\e false on failure)
 * \note Unit is radian [rad]
 * \note Returns bank angle w.r.t. closest planet
 * \note The local horizon is the plane whose normal is defined by the distance
 *  vector from the planet centre to the vessel.
 * \note The handle passed to the function must refer to a vessel.
 * \sa oapiGetHeading, oapiGetPitch, oapiGetAltitude
 */
OAPIFUNC BOOL oapiGetBank (OBJHANDLE hVessel, double *bank);

/**
 * \brief Returns a vessel's heading (against geometric north) calculated for the local horizon plane.
 * \param hVessel vessel handle
 * \param heading pointer to variable receiving heading value [rad]
 * \return Error flag (\e false on failure)
 * \note Unit is radian [rad] 0=north, PI/2=east, etc.
 * \note The handle passed to the function must refer to a vessel.
 * \sa oapiGetBank, oapiGetPitch, oapiGetAltitude
 */
OAPIFUNC BOOL oapiGetHeading (OBJHANDLE hVessel, double *heading);

/**
 * \brief Returns the altitude of the current focus vessel over a planetary surface.
 * \param alt pointer to variable receiving altitude value [m]
 * \return Error flag (\e false on failure )
 */
OAPIFUNC BOOL oapiGetFocusAltitude (double *alt);
	
/**
 * \brief Returns the pitch angle of the current focus vessel w.r.t. the local horizon.
 * \param pitch pointer to variable receiving pitch value
 * \return Error flag (\e false on failure)
 * \sa oapiGetFocusBank, oapiGetFocusHeading, oapiGetFocusAltitude
 */
OAPIFUNC BOOL oapiGetFocusPitch (double *pitch);

/**
 * \brief Returns the bank angle of the current focus vessel w.r.t. the local horizon.
 * \param bank pointer to variable receiving bank angle [rad]
 * \return Error flag (\e false on failure)
 * \sa oapiGetFocusHeading, oapiGetFocusPitch, oapiGetFocusAltitude
 */
OAPIFUNC BOOL oapiGetFocusBank (double *bank);

/**
 * \brief Returns the heading (against geometric north) of the current focus vessel calculated for
 *  the local horizon plane.
 * \param heading pointer to variable receiving heading value [rad]
 * \return Error flag (\e false on failure)
 * \sa oapiGetFocusBank, oapiGetFocusPitch, oapiGetFocusAltitude
 */
OAPIFUNC BOOL oapiGetFocusHeading (double *heading);

/**
 * \brief Returns a vessel's ground speed w.r.t. the closest planet or moon.
 * \param hVessel vessel handle, or NULL for focus vessel
 * \param groundspeed pointer to variable receiving ground speed value [m/s]
 * \return Error flag (\e false on error)
 * \sa oapiGetGroundspeedVector, oapiGetAirspeed, oapiGetAirspeedVector
 */
OAPIFUNC BOOL oapiGetGroundspeed (OBJHANDLE hVessel, double *groundspeed);

/**
 * \brief Returns a vessel's groundspeed vector w.r.t. the closest planet or moon in the
 *   requested frame of reference.
 * \param [in] hVessel vessel handle, or NULL for focus vessel
 * \param [in] frame frame of reference flag
 * \param [out] vel pointer to variable receiving ground speed vector [m/s in x,y,z]
 * \return Error flag (\e false indicates error)
 * \note This method returns the ground speed vector in the requested frame
 *   of reference. The ground speed vector is defined as the vessel's
 *   velocity vector with respect to a point at the vessel position fixed
 *   in the planet's rotating frame of reference.
 * \note Valid entries for \a frame are
 *  - FRAME_GLOBAL: Returns velocity vector in the global frame of reference
 *  - FRAME_LOCAL: Returns velocity vector in the vessel's local frame of
 *    reference
 *  - FRAME_REFLOCAL: Returns velocity vector in the celestial reference
 *    body's local frame of reference
 *  - FRAME_HORIZON: Returns velocity vector in the local horizon frame
 *    (x = longitudinal component, y = vertical component, z = latitudinal
 *    component)
 * \sa oapiGetGroundspeed, oapiGetAirspeedVector, oapiGetAirspeed, VESSEL::GetGroundspeedVector
 */
OAPIFUNC bool oapiGetGroundspeedVector (OBJHANDLE hVessel, REFFRAME frame, VECTOR3 *vel);

/**
 * \brief Returns a vessel's true airspeed w.r.t. the closest planet or moon.
 * \param hVessel vessel handle, or NULL for focus vessel
 * \param airspeed pointer to variable receiving airspeed value [m/s]
 * \return Error flag (\e false on error)
 * \note This function works even for planets or moons without atmosphere. In that case
 *   it returns the ground speed.
 * \sa oapiGetAirspeedVector, oapiGetGroundspeed, oapiGetGroundspeedVector, VESSEL::GetAirspeed
 */
OAPIFUNC BOOL oapiGetAirspeed (OBJHANDLE hVessel, double *airspeed);

/**
 * \brief Returns a vessel's true airspeed vector w.r.t. the closest planet or moon in the
 *   requested frame of reference.
 * \param [in] hVessel vessel handle, or NULL for focus vessel
 * \param [in] frame frame of reference flag
 * \param [out] v pointer to variable receiving airspeed vector [m/s in x,y,z]
 * \return Error flag (\e false indicates error)
 * \note This method returns the true airspeed vector in the requested frame
 *   of reference. The airspeed vector is defined as the vessel's
 *   velocity vector with respect to the surrounding freestream air flow.
 * \note If the vessel is not within an a planetary atmosphere, the returned
 *   vector is equal to the groundspeed vector.
 * \note Valid entries for \a frame are
 *  - FRAME_GLOBAL: Returns velocity vector in the global frame of reference
 *  - FRAME_LOCAL: Returns velocity vector in the vessel's local frame of
 *    reference
 *  - FRAME_REFLOCAL: Returns velocity vector in the celestial reference
 *    body's local frame of reference
 *  - FRAME_HORIZON: Returns velocity vector in the local horizon frame
 *    (x = longitudinal component, y = vertical component, z = latitudinal
 *    component)
 * \sa oapiGetAirspeed, oapiGetGroundspeedVector, oapiGetGroundspeed, VESSEL::GetAirspeedVector
 */
OAPIFUNC bool oapiGetAirspeedVector (OBJHANDLE hVessel, REFFRAME frame, VECTOR3 *v);

/**
 * \brief Returns a vessel's spherical equatorial coordinates (longitude, latitude and radius) with
 *  respect to the closest planet or moon.
 * \param hVessel vessel handle
 * \param longitude pointer to variable receiving longitude value [rad]
 * \param latitude pointer to variable receiving latitude value [rad]
 * \param radius pointer to variable receiving radius value [m]
 * \return Error flag (\e false on failure)
 * \note The handle passed to the function must refer to a vessel.
 */
OAPIFUNC BOOL oapiGetEquPos (OBJHANDLE hVessel, double *longitude, double *latitude, double *radius);

/**
 * \brief Returns the current focus vessel's spherical equatorial coordinates (longitude, latitude
 *  and radius) with respect to the closest planet or moon.
 * \param longitude pointer to variable receiving longitude value [rad]
 * \param latitude pointer to variable receiving latitude value [rad]
 * \param radius pointer to variable receiving radius value [m]
 * \return Error flag (\e false on failure)
 */
OAPIFUNC BOOL oapiGetFocusEquPos (double *longitude, double *latitude, double *radius);

/**
 * \brief Returns the atmospheric parameters at the current vessel position.
 * \param [in] hVessel vessel handle
 * \param [out] prm pointer to ATMPARAM structure receiving atmospheric parameters.
 * \param [out] hAtmRef pointer to handle receiving the atmosphere reference body.
 * \note If \a hVessel == NULL, the current focus vessel is used for the calculation.
 * \note If \a hAtmRef != NULL, it receives the handle of the celestial body
 *   contributing the atmospheric parameters.
 * \note If the vessel is not within range of any planet atmosphere model, all
 *   fields of the prm structure are set to 0. If applicable, *hAtmRef is set to NULL.
 * \note Currently, atmospheric values only depend on altitude, and don't take
 *   into account local weather variations.
 */
OAPIFUNC void oapiGetAtm (OBJHANDLE hVessel, ATMPARAM *prm, OBJHANDLE *hAtmRef = 0);

	/**
	* \brief Retrieve the status of main, retro and hover thrusters for a vessel.
	* \param hVessel vessel handle
	* \param es pointer to an \c ENGINESTATUS structure which will receive the engine level parameters
	* \note The main/retro engine level has a range of [-1,+1]. A positive value indicates
	*  engaged main/disengaged retro thrusters, a negative value indicates engaged
	*  retro/disengaged main thrusters. Main and retro thrusters cannot be engaged
	*  simultaneously. For vessels without retro thrusters the valid range is [0,+1]. The
	*  valid range for hover thrusters is [0,+1].
	* \note \c ENGINESTATUS has the following components:
	* \code
	* typedef struct {       
	*   double main;   // -1 (full retro) .. +1 (full main)
	*   double hover;  // 0 .. +1 (full hover)
	*   int attmode;   // 0=rotation, 1=translation
	* } ENGINESTATUS; \endcode
	*/
OAPIFUNC void oapiGetEngineStatus (OBJHANDLE hVessel, ENGINESTATUS *es);

	/**
	* \brief Retrieve the engine status for the focus vessel.
	* \param es pointer to an \c ENGINESTATUS structure which will receive the
	*  engine level parameters.
	* \sa oapiGetEngineStatus
	*/
OAPIFUNC void oapiGetFocusEngineStatus (ENGINESTATUS *es);

	/**
	* \brief Engage the specified engines.
	* \param hVessel vessel handle
	* \param engine identifies the engine to be set
	* \param level engine thrust level [0,1]
	* \note Not all vessels support all types of engines.
	* \note Setting main thrusters > 0 implies setting retro thrusters to 0 and vice versa.
	* \note Setting main thrusters to -level is equivalent to setting retro thrusters to
	*  +level and vice versa.
	*/	
OAPIFUNC void oapiSetEngineLevel (OBJHANDLE hVessel, ENGINETYPE engine, double level);

	/**
	* \brief Returns a vessel's current attitude thruster mode.
	* \param hVessel vessel handle
	* \return Current attitude mode (0=disabled or not available, 1=rotational, 2=linear)
	* \note The handle must refer to a vessel. This function does not support other
	*  object types.
	* \sa oapiToggleAttitudeMode, oapiSetAttitudeMode
	*/
OAPIFUNC int oapiGetAttitudeMode (OBJHANDLE hVessel);

	/**
	* \brief Flip a vessel's attitude thruster mode between rotational and linear.
	* \param hVessel vessel handle
	* \return The new attitude mode (1=rotational, 2=linear, 0=unchanged disabled)
	* \note he handle must refer to a vessel. This function does not support other object types.
	* \note This function flips between linear and rotational, but has no effect if attitude
	*  thrusters were disabled.
	* \sa oapiSetAttitudeMode, oapiGetAttitudeMode
	*/
OAPIFUNC int oapiToggleAttitudeMode (OBJHANDLE hVessel);

	/**
	* \brief Set a vessel's attitude thruster mode.
	* \param hVessel vessel handle
	* \param mode attitude mode (0=disable, 1=rotational, 2=linear)
	* \return Error flag; \e false indicates failure (requested mode not available)
	* \note The handle must refer to a vessel. This function does not support other object types.
	* \sa oapiToggleAttitudeMode, oapiGetAttitudeMode
	*/
OAPIFUNC bool oapiSetAttitudeMode (OBJHANDLE hVessel, int mode);

	/**
	* \brief Returns the current focus vessel's attitude thruster mode (rotational or linear)
	* \return Current attitude mode (0=disabled or not available, 1=rotational, 2=linear)
	*/
OAPIFUNC int oapiGetFocusAttitudeMode ();

	/**
	* \brief Flip the current focus vessel's attitude thruster mode between rotational and linear.
	* \return The new attitude mode (1=rotational, 2=linear, 0=unchanged disabled)
	* \note This function flips between linear and rotational, but has no effect if attitude
	*  thrusters were disabled.
	* \sa oapiSetFocusAttitudeMode, oapiGetFocusAttitudeMode
	*/
OAPIFUNC int oapiToggleFocusAttitudeMode ();

	/**
	* \brief Set the current focus vessel's attitude thruster mode.
	* \param mode attitude mode (0=disable, 1=rotational, 2=linear)
	* \return Error flag; \e false indicates error (requested mode not available)
	* \sa oapiGetFocusAttitudeMode, oapiToggleFocusAttitudeMode
	*/
OAPIFUNC bool oapiSetFocusAttitudeMode (int mode);

//@}


// ======================================================================
/// \defgroup oapi_transformation Coordinate transformations
// ======================================================================
//@{

/**
 * \brief Returns the current rotation matrix of an object.
 * \param [in] hObj object handle
 * \param [out] mat rotation matrix
 * \note The returned rotation matrix can be used to transform orientations from the
 *   local frame of an object to Orbiter's global reference frame (ecliptic and equinox of J2000)
 *   and vice versa.
 * \note The rotation, defined by matrix R, together with a translation vector t, provides the
 *   transformation of a point p between local and global coordinates:
 *   \f[ \vec{p}_\mathrm{global} = \mathsf{R} \vec{p}_\mathrm{local} + \vec{t} \f]
 *   and
 *   \f[ \vec{p}_\mathrm{local} = \mathrm{R}^T (\vec{p}_\mathrm{global} - \vec{t}) \f]
 * \sa VESSEL::GetRotationMatrix, \n
 *   mul(const MATRIX3&,const VECTOR3&), \n
 *   tmul(const MATRIX3&,const VECTOR3&)
 */
OAPIFUNC void oapiGetRotationMatrix (OBJHANDLE hObj, MATRIX3 *mat);

/**
 * \brief Maps a point from the global frame to a local object frame.
 * \param [in] hObj object handle
 * \param [in] glob point in global coordinates
 * \param [out] loc point mapped into local coordinates
 * \note This function maps global point \a glob into the local reference
 *   frame of body \a hObj. The transformation is given by
 *   \f[ \vec{p}_\mathrm{loc} = \mathsf{R}_\mathrm{hObj}^T (\vec{p}_\mathrm{glob} - \vec{p}_\mathrm{hObj}) \f]
 *   where \f$ \mathsf{R}_\mathrm{hObj},\; \vec{p}_\mathrm{hObj} \f$ are the body's
 *   rotation matrix and global position, respectively.
 * \sa oapiLocalToGlobal, oapiGetRotationMatrix
 */
OAPIFUNC void oapiGlobalToLocal (OBJHANDLE hObj, const VECTOR3 *glob, VECTOR3 *loc);

/**
 * \brief Maps a point from a local object frame to the global frame.
 * \param [in] hObj object handle
 * \param [in] loc point in local coordinates of the object frame
 * \param [out] glob point mapped into global coordinates
 * \note This function maps point \a loc given in local coordinates of
 *   \a hObj into the global reference frame (barycentric ecliptic and
 *   equinox of J2000). The transformation is given by
 *   \f[ \vec{p}_\mathrm{glob} = \mathsf{R}_\mathrm{hObj} \vec{p}_\mathrm{loc} + \vec{p}_\mathrm{hObj} \f]
 *   where \f$ \mathsf{R}_\mathrm{hObj},\; \vec{p}_\mathrm{hObj} \f$ are the body's
 *   rotation matrix and global position, respectively.
 * \sa oapiGlobalToLocal, oapiGetRotationMatrix
 */
OAPIFUNC void oapiLocalToGlobal (OBJHANDLE hObj, const VECTOR3 *loc, VECTOR3 *glob);

/**
 * \brief Returns the cartesian position in the local object frame of a point
 *   given in equatorial coordinates.
 * \param [in] hObj object handle
 * \param [in] lng longitude of point [rad]
 * \param [in] lat latitude of point [rad]
 * \param [in] rad distance from local object origin [m]
 * \param [out] loc point in cartesian coordinates of the local object frame [<b>m</b>]
 * \sa oapiLocalToEqu, oapiEquToGlobal, oapiGlobalToEqu
 */
OAPIFUNC void oapiEquToLocal (OBJHANDLE hObj, double lng, double lat, double rad, VECTOR3 *loc);

/**
 * \brief Returns the equatorial coordinates of a point given in the local frame of an object.
 * \param [in] hObj object handle
 * \param [in] loc point in cartesian coordinates of the local object frame [<b>m</b>]
 * \param [out] lng pointer to variable receiving the longitude value [rad]
 * \param [out] lat pointer to variable receiving the latitude value [rad]
 * \param [out] rad pointer to variable receiving the radial distance value [m]
 * \sa oapiEquToLocal, oapiEquToGlobal, oapiGlobalToEqu
 */
OAPIFUNC void oapiLocalToEqu (OBJHANDLE hObj, const VECTOR3 &loc, double *lng, double *lat, double *rad);

/**
 * \brief Returns the global cartesian position of a point given in equatorial coordinates of an object.
 * \param [in] hObj object handle
 * \param [in] lng longitude of point [rad]
 * \param [in] lat latitude of point [rad]
 * \param [in] rad distance from local object origin [m]
 * \param [out] glob point in cartesian coordinates of the global reference frame [<b>m</b>]
 * \sa oapiGlobalToEqu, oapiEquToLocal, oapiLocalToEqu
 */
OAPIFUNC void oapiEquToGlobal (OBJHANDLE hObj, double lng, double lat, double rad, VECTOR3 *glob);

/**
 * \brief Returns the equatorial coordinates with respect to an object
 *   of a point given in the global reference frame.
 * \param [in] hObj object handle
 * \param [in] glob point in global coordinates
 * \param [out] lng pointer to variable receiving the longitude value [rad]
 * \param [out] lat pointer to variable receiving the latitude value [rad]
 * \param [out] rad pointer to variable receiving the radial distance value [m]
 * \sa oapiEquToLocal, oapiLocalToEqu, oapiEquToGlobal
 */
OAPIFUNC void oapiGlobalToEqu (OBJHANDLE hObj, const VECTOR3 &glob, double *lng, double *lat, double *rad);

/**
 * \brief Returns the angular distance of two points on a sphere.
 * \param lng1 longitude value of point 1 [rad]
 * \param lat1 latitude value of point 1 [rad]
 * \param lng2 longitude value of point 2 [rad]
 * \param lat2 latitude value of point 2 [rad]
 * \note Given two points on the surface of a sphere, this function returns
 *   the orthodome (shortest) angular distance between them.
 * \note The shortest surface path between the points is an arc on a great
 *   circle containing the two points, and its length is given by
 *   d = a R, where a is the angular distance returned by oapiOrthodome, and
 *   R is the radius of the sphere.
 */
OAPIFUNC double oapiOrthodome (double lng1, double lat1, double lng2, double lat2);
//@}

	/**
	* \brief Request a custom texture for vessel exhaust rendering.
	* \param name exhaust texture file name (without path and extension)
	* \return texture handle
	* \note The exhaust texture must be stored in DDS format in Orbiter's default texture directory.
	* \note If the texture is not found the function returns NULL.
	* \note The texture can be used to define custom textures in VESSEL::AddExhaust.
	* \sa oapiRegisterReentryTexture, oapiRegisterParticleTexture
	*/
OAPIFUNC SURFHANDLE oapiRegisterExhaustTexture (char *name);

	/**
	* \brief Request a custom texture for vessel reentry flame rendering.
	* \param name reentry texture file name (without path and extension)
	* \return texture handle
	* \note The exhaust texture must be stored in DDS format in Orbiter's default texture directory.
	* \note If the texture is not found the function returns NULL.
	* \note The texture can be used to define custom textures in VESSEL::SetReentryTexture().
	* \sa oapiRegisterExhaustTexture, oapiRegisterParticleTexture
	*/
OAPIFUNC SURFHANDLE oapiRegisterReentryTexture (char *name);

OAPIFUNC SURFHANDLE oapiRegisterParticleTexture (char *name);

OAPIFUNC void oapiSetShowGrapplePoints (bool show);
OAPIFUNC bool oapiGetShowGrapplePoints ();


	// =============================================================================================
	//  Aerodynamics helper functions
    // =============================================================================================
	/**
	* \brief Aerodynamics helper function
	* \details This is a helper function which is useful when implementing the callback function
	* calculating the aerodynamics coefficients for an airfoil (see VESSEL::CreateAirfoil). It
	* computes the lift-induced component c<sub>D,i</sub> of the drag coefficient as a function of lift
	* coefficient <i>c<sub>L</sub></i>, wing aspect ratio \a A, and wing efficiency factor \a e, as
	* \f[
	* c_{_D,i} = \frac{c_L^2}{\pi Ae}
	* \f]
	* \param cl lift coefficient
	* \param A wing aspect ratio
	* \param e wing efficiency factor
	* \return Induced drag coefficient c<sub>D,i</sub>
	* \note The full drag coefficient required by the airfoil callback function consists of
	*  several components: profile drag c<sub>D,e</sub>, induced drag c<sub>D,i</sub> and wave drag c<sub>D,w</sub>
	* \f[
	*  c_{_D} = c_{_D,e} + c_{_D,i} + c_{_D,w} 
	* \f]
	*  where c<sub>D,e</sub> is caused by skin friction and pressure components, and c<sub>D,w</sub> is a
	*  result of the shock wave and flow separation in transonic and supersonic flight.
	* \note The wing aspect ratio is defined as  <i>b<sup>2</sup>/S</i>, where \a b is the wing span,
	*  and \a S is the wing area.
	* \note The efficiency factor depends on the wing shape. The most efficient wings
	*  are elliptical, with \a e = 1. For all other shapes, \a e < 1.
	* \note This function can be interpreted slightly differently by moving the angle of
	*  attack-dependency of the profile drag into the induced drag component:
	* \f[
	*  c_{_D} = c_{_{D,0}} + \acute{c_{_D,}}_i + c_{_D,w} 
	* \f]
	*  where c<sub>D,0</sub> is the zero-lift component of the profile drag, and 
	*  \f$ \acute{c_{_D,}}_i \f$ is a modified induced drag obtained by replacing the shape 
	*  factor e with the Oswald efficiency factor. See Programmer's Guide for more details.
	*/
OAPIFUNC double oapiGetInducedDrag (double cl, double A, double e);

	/**
	* \brief Aerodynamics helper function
	* \details This is a helper function which is useful when implementing the callback function
	*  calculating the aerodynamics coefficients for an airfoil (see VESSEL::CreateAirfoil). It
	*  uses a simple model to compute the wave drag component of the drag coefficient,
	*  <i>c</i><sub><i>D</i>,<i>w</i></sub>. Wave drag significantly affects the vessel drag
	*  around Mach 1, and falls off towards lower and higher airspeeds.
	*  This function uses the following model:
	*  \f[
	*  c_{D,w} = \left\lbrace \begin{array}{ll}
	*    0 & \mathrm{if } M < M_1 \\
	*    c_m \frac{M-M_1}{M_2-M_1} & \mathrm{if } M_1 < M < M_2 \\
	*    c_m & \mathrm{if } M_2 < M < M_3 \\
	*    c_m \frac{(M_3^2-1)^{1/2}}{(M^2-1)^{1/2}} & \mathrm{if } M > M_3
	*  \end{array}\right.
	*  \f]
	*  where 0 < <i>M</i><sub>1</sub> < <i>M</i><sub>2</sub> < 1 < <i>M</i><sub>3</sub> are
	*  characteristic Mach numbers, and <i>c<sub>m</sub></i> is the maximum wave drag coefficient at transonic speeds.
	* \param M current Mach number
	* \param M1 characteristic Mach number
	* \param M2 characteristic Mach number
	* \param M3 characteristic Mach number
	* \param cmax maximum wave drag coefficient <i>c<sub>m</sub></i>
	* \return Wave drag coefficient <i>c</i><sub><i>D</i>,<i>w</i></sub>
	* \note The model underlying this function assumes a piecewise linear wave drag
	* profile for <i>M</i> < <i>M</i><sub>3</sub>, and a decay with (<i>M</i><sup>2</sup>-1)<sup>-1/2</sup>
	* for <i>M</i> > <i>M</i><sub>3</sub>.
	* If this profile is not suitable for a given airfoil, the programmer must implement wave drag manually.
	* \sa oapiGetInducedDrag, VESSEL::CreateAirfoil
	*/
OAPIFUNC double oapiGetWaveDrag (double M, double M1, double M2, double M3, double cmax);


// =============================================================================================
/// \defgroup Camera Camera functions
// =============================================================================================
//@{
	/**
	* \brief Returns flag to indicate internal/external camera mode.
	* \return \e true indicates an internal camera mode, i.e. the camera is located inside a vessel
	*  cockpit. In this case, the camera target is always the current focus object.
	*  false indicates an external camera mode, i.e. the camera points toward an object
	*  from outside. The camera target may be a vessel, planet, spaceport, etc.
	* \sa oapiCameraMode, oapiCockpitMode
	*/
OAPIFUNC bool oapiCameraInternal ();

	/**
	* \brief Returns the current camera view mode.
	* \return Camera mode:
	*  - \c CAM_COCKPIT cockpit (internal) mode
	*  - \c CAM_TARGETRELATIVE tracking mode (relative direction)
	*  - \c CAM_ABSDIRECTION tracking mode (absolute direction)
	*  - \c CAM_GLOBALFRAME tracking mode (global frame)
	*  - \c CAM_TARGETTOOBJECT tracking mode (target to object)
	*  - \c CAM_TARGETFROMOBJECT tracking mode (object to target)
	*  - \c CAM_GROUNDOBSERVER ground observer mode
	* \sa oapiCameraInternal, VESSEL::GetCameraOffset, VESSEL::GetCameraDefaultDirection
	*/
OAPIFUNC int oapiCameraMode ();

	/**
	* \brief Returns the current cockpit display mode.
	* \return Cockpit mode:
	*  - \c COCKPIT_GENERIC (generic cockpit mode: left+right MFD and HUD)
	*  - \c COCKPIT_PANELS (2D panel mode)
	*  - \c COCKPIT_VIRTUAL (virtual cockpit mode)
	*
	* \note This function also works if the camera is not currently in cockpit mode.
	* \sa oapiCameraInternal, VESSEL::GetCameraOffset, VESSEL::GetCameraDefaultDirection
	*/
OAPIFUNC int oapiCockpitMode ();

	/**
	* \brief Returns a handle to the current camera target.
	* \return Handle to the current camera target (i.e. the object the camera is pointing at in
	*  external mode, or the handle of the vessel in cockpit mode)
	* \note The camera target is not necessarily a vessel, and if it is a vessel, it is not
	*  necessarily the focus object (the vessel receiving user input).
	* \sa oapiCameraAttach
	*/
OAPIFUNC OBJHANDLE oapiCameraTarget ();

	/**
	* \brief Returns the current virtual cockpit position
	* \return VC position (>= 0, or -1 if camera is not in VC mode)
    */
OAPIFUNC int oapiVCPosition ();

	/**
	* \brief Returns celestial body whose surface is closest to the camera.
	*/
OAPIFUNC OBJHANDLE oapiCameraProxyGbody();

	/**
	* \brief Returns current camera position in global coordinates.
	* \param gpos pointer to vector to receive global camera coordinates
	* \note The global coordinate system is the heliocentric ecliptic frame at epoch J2000.0.
	* \sa oapiCameraGlobalDir
	*/
OAPIFUNC void oapiCameraGlobalPos (VECTOR3 *gpos);

	/**
	* \brief Returns current camera direction in global coordinates.
	* \param gdir pointer to vector to receive global camera direction
	* \sa oapiCameraGlobalPos
	*/
OAPIFUNC void oapiCameraGlobalDir (VECTOR3 *gdir);

   /**
    * \brief Returns a rotation matrix which performs the transformation
    *   from the camera rotation into global coordinates.
    * \param rmat pointer to a matrix receiving the rotation data
    * \sa oapiCameraGlobalPos, oapiCameraGlobalDir
    */
OAPIFUNC void oapiCameraRotationMatrix (MATRIX3 *rmat);

	/**
	* \brief Returns the distance between the camera and its target [m].
	* \return Distance between camera and camera target [m].
	*/
OAPIFUNC double oapiCameraTargetDist ();

	/**
	* \brief Returns the current camera azimuth angle with respect to the target.
	* \return Camera azimuth angle [rad]. Value 0 indicates that the camera is behind the target.
	* \note This function is useful only in external camera mode. In internal mode, it will
	*  always return 0.
	*/
OAPIFUNC double oapiCameraAzimuth ();

	/**
	* \brief Returns the current camera polar angle with respect to the target.
	* \return Camera polar angle [rad]. Value 0 indicates that the camera is at the same
	*  elevation as the target.
	* \note This function is useful only in external camera mode. In internal mode, it will
	*  always return 0.
	*/
OAPIFUNC double oapiCameraPolar ();

	/**
	* \brief Returns the current camera aperture (the field of view) in rad.
	* \return camera aperture [rad]
	* \note Orbiter defines the the aperture as 1/2 of the vertical field of view, between
	*  the viewport centre and the top edge of the viewport.
	*/
OAPIFUNC double oapiCameraAperture ();

	/**
	* \brief Change the camera aperture (field of view).
	* \param aperture new aperture [rad]
	* \note Orbiter restricts the aperture to the range from RAD*0.1 to RAD*80 (i. e. field
	*  of view between 0.2 and 160 deg. Very wide angles (\> 90 deg) and very narrow angles
	*  (\< 5 deg) should only be used to implement specific optical devices, e.g. telescopes
	*  or wide-angle cameras, not for standard observer views.
	* \note The Orbiter user interface does not accept apertures \> 45 deg or \< 5 deg. As
	*  soon as the user manipulates the aperture manually, it will be clamped back to the
	*  range from 5 to 45 deg.
	*/
OAPIFUNC void oapiCameraSetAperture (double aperture);

	/**
	* \brief Moves the camera closer to the target or further away.
	* \param dscale distance scaling factor
	* \note Setting dscale \< 1 will move the camera closer to its target. dscale \> 1 will
	*  move it further away.
	* \note This function is ignored if the camera is in internal mode.
	*/
OAPIFUNC void oapiCameraScaleDist (double dscale);

	/**
	* \brief Rotate the camera around the target (azimuth angle).
	* \param dazimuth change in azimuth angle [rad]
	* \note This function is ignored if the camera is in internal mode.
	*/
OAPIFUNC void oapiCameraRotAzimuth (double dazimuth);

	/**
	* \brief Rotate the camera around the target (polar angle).
	* \param dpolar change in polar angle [rad]
	* \note This function is ignored if the camera is in internal mode.
	*/
OAPIFUNC void oapiCameraRotPolar (double dpolar);

	/**
	* \brief Set the camera direction in cockpit mode.
	* \param polar polar angle [rad]
	* \param azimuth azimuth angle [rad]
	* \param transition transition flag (see notes)
	* \note This function is ignored if the camera is not currently in cockpit mode.
	* \note The polar and azimuth angles are relative to the default view direction (see
	*  VESSEL::SetCameraDefaultDirection())
	* \note The requested direction should be within the current rotation ranges (see
	*  VESSEL::SetCameraRotationRange()), otherwise the result is undefined.
	* \note If transition==false, the new direction is set instantaneously; otherwise the
	*  camera swings from the current to the new direction (not yet implemented).
	*/
OAPIFUNC void oapiCameraSetCockpitDir (double polar, double azimuth, bool transition = false);

	/**
	* \brief Attach the camera to a new target, or switch between internal and external camera mode.
	* \param hObj handle of the new camera target 
	* \param mode camera mode (0=internal, 1=external, 2=don't change)
	* \note If the new target is not a vessel, the camera mode is always set to external,
	*  regardless of the value of mode.
	* \sa oapiCameraMode, oapiCameraTarget
	*/
OAPIFUNC void oapiCameraAttach (OBJHANDLE hObj, int mode);

	/**
	* \brief Set the camera to the mode specified by the CameraMode object.
	* \param mode CameraMode subclass object to set the parameters for cockpit, track or ground observer mode
	* \return Currently always returns true.
	*/
OAPIFUNC bool oapiSetCameraMode(const CameraMode &mode);

	/**
	* \brief Move the ground observer camera.
	* \param forward distance in camera-forward direction (< 0 for backward) [m]
	* \param right distance in camera-right direction (< 0 for left) [m]
	* \param up distance in planet-up direction (< 0 for down) [m]
	* \return true on success, false if
	*   - camera not in ground observer mode
	*/
OAPIFUNC bool oapiMoveGroundCamera (double forward, double right=0, double up=0);
//@}


// =============================================================================================
/**
 * \defgroup Planet Functions for planetary bodies
 *
 * All OBJHANDLE function parameters used in this section must refer to planetary bodies
 * (planets, moons, astereoids, etc.) unless stated otherwise. Invalid handles may lead to
 * crashes.\n
 * Currently, the orientation of planetary rotation axes is assumed time-invariant. Precession,
 * nutation and similar effects are not currently simulated.
 */
// =============================================================================================
//@{
	/**
	* \brief Returns the rotation period (the length of a siderial day) of a planet.
	* \param hPlanet planet handle
	* \return planet rotation period [seconds]
	* \sa oapiGetPlanetObliquity, oapiGetPlanetTheta
	*/
OAPIFUNC double oapiGetPlanetPeriod (OBJHANDLE hPlanet);
	
	/**
	* \brief Returns the obliquity of the planet's rotation axis (the angle between the rotation axis
	*  and the ecliptic zenith).
	* \param hPlanet planet handle
	* \return obliquity [rad]
	* \note In Orbiter, the ecliptic zenith (at epoch J2000) is the positive y-axis of the
	*  global frame of reference.
	* \sa oapiGetPlanetPeriod, oapiGetPlanetTheta
	*/
OAPIFUNC double oapiGetPlanetObliquity (OBJHANDLE hPlanet);

	/**
	* \brief Returns the longitude of the ascending node.
	* \details Returns the longitude of the ascending node of the equatorial plane (denoted by \a q),
	*  that is, the angle between the vernal equinox and the ascending node of the equator w.r.t. the ecliptic.
	* \param hPlanet planet handle
	* \return longitude of ascending node of the equator [rad]
	* \note For Earth, this function will return 0. (The ascending node of Earth's
	*  equatorial plane is the definition of the vernal equinox).
	* \sa oapiGetPlanetPeriod, oapiGetPlanetObliquity
	*/
OAPIFUNC double oapiGetPlanetTheta (OBJHANDLE hPlanet);

	/**
	* \brief Returns a rotation matrix which performs the transformation from the planet's tilted
	*  coordinates into global coordinates.
	* \param hPlanet planet handle
	* \param mat pointer to a matrix receiving the rotation data
	* \note The returned matrix is given by 
	* \f[
	*  R_a = \left[ \begin{array}{ccc} \cos\theta & 0 & -\sin\theta \\ 0 & 1 & 0 \\
	*  \sin\theta & 0 & \cos\theta \end{array} \right] \left[ \begin{array}{ccc} 1 & 0 & 0 \\
	*  0 & \cos\varphi & -\sin\varphi \\ 0 & \sin\varphi & \cos\varphi \end{array} \right]
	* \f]
	*  where \f$ \theta \f$ is the longitude of the ascending node of the equator, as returned
	*  by oapiGetPlanetTheta(), and \f$ \varphi \f$ is the obliquity as returned by oapiGetPlanetObliquity().
	* \f$ R_a \f$ does not include the current rotation of the planet around its axis.
	* \f$ R_a \f$ is therefore time-independent.
	* \sa oapiGetPlanetPeriod
	*/
OAPIFUNC void oapiGetPlanetObliquityMatrix (OBJHANDLE hPlanet, MATRIX3 *mat);

	/**
	* \brief Returns the current rotation angle of the planet around its axis.
	* \param hPlanet planet handle
	* \return Rotation angle [rad]
	* \note The complete rotation matrix from planet local to global (ecliptic) coordinates
	*  is given by
	* \f[
	*  R = R_a \left[ \begin{array}{ccc} \cos\omega & 0 & -\sin\omega \\
	*  0 & 1 & 0 \\ \sin\omega & 0 & \cos\omega \end{array} \right]
	* \f]
	* where \f$ R_a \f$ is the obliquity matrix as returned by oapiGetPlanetObliquityMatrix(),
	* and \f$ \omega \f$ is the rotation angle returned by oapiGetPlanetCurrentRotation().
	*/
OAPIFUNC double oapiGetPlanetCurrentRotation (OBJHANDLE hPlanet);

	/**
	* \brief Test for existence of planetary atmosphere.
	* \param hPlanet planet handle
	* \return \e true if an atmosphere has been defined for the planet, \e false otherwise.
	* \sa oapiGetPlanetAtmParams
	*/
OAPIFUNC bool oapiPlanetHasAtmosphere (OBJHANDLE hPlanet);

	/**
	* \brief Returns atmospheric parameters as a function of distance from the planet centre.
	* \param hPlanet planet handle
	* \param rad radius from planet centre [m]
	* \param prm pointer to \c ATMPARAM structure receiving parameters
	* \note If the planet has no atmosphere, or if the defined radius is beyond the
	*  defined upper atmosphere limit, all parameters are set to 0.
	* \note If the atmosphere model is position- as well as altitude-dependent, this
	*   function assumes longitude=0 and latitude=0.
	* \note \c ATMPARAM has the following components:
	* \code 
	* typedef struct {      
	*   double T;      // temperature [K]
	*   double p;      // pressure [Pa]
	*   double rho;    // density [kg/m^3]
	* } ATMPARAM; \endcode
	* \sa oapiGetPlanetAtmParams(OBJHANDLE,double,double,double,ATMPARAM*),
	*   oapiPlanetHasAtmosphere, oapiGetPlanetAtmConstants
	*/
OAPIFUNC void oapiGetPlanetAtmParams (OBJHANDLE hPlanet, double rad, ATMPARAM *prm);

	/**
	* \brief Returns atmospheric parameters of a planet as a function of altitude
	*   and geographic position.
	* \param hPlanet planet handle
	* \param alt altitude above planet mean radius [m]
	* \param lng longitude [rad]
	* \param lat latitude [rad]
	* \param prm pointer to \c ATMPARAM structure receiving parameters
	* \sa oapiGetPlanetAtmParams(OBJHANDLE,double,double,double,ATMPARAM*),
	*   oapiPlanetHasAtmosphere, oapiGetPlanetAtmConstants
	*/
OAPIFUNC void oapiGetPlanetAtmParams (OBJHANDLE hPlanet, double alt, double lng, double lat, ATMPARAM *prm);

	/**
	* \brief Returns atmospheric constants for a planet.
	* \param hPlanet planet handle
	* \return pointer to \c ATMCONST structure containing atmospheric coefficients for the planet (see notes)
	* \note \c ATMCONST has the following components:
	* \code
	* typedef struct {
	*   double p0;         // pressure at mean radius ('sea level') [Pa]
	*   double rho0;       // density at mean radius [kg/m3]
	*   double R;          // specific gas constant [J/(K kg)]
	*   double gamma;      // ratio of specific heats, c_p/c_v
	*   double C;          // exponent for pressure equation (temporary)
	*   double O2pp;       // partial pressure of oxygen
	*   double altlimit;   // atmosphere altitude limit [m]
	*   double radlimit;   // radius limit (altlimit + mean radius)
	*   double horizonalt; // horizon rendering altitude
	*   VECTOR3 color0;    // sky colour at sea level during daytime
	* } ATMCONST; \endcode
	* \note If the specified planet does not have an atmosphere, return value is NULL.
	* \sa oapiPlanetHasAtmosphere, oapiGetPlanetAtmParams
	*/
OAPIFUNC const ATMCONST *oapiGetPlanetAtmConstants (OBJHANDLE hPlanet);

	/**
	* \brief Returns the velocity vector of a surface point.
	* \param hPlanet planet handle
	* \param lng longitude [rad]
	* \param lat latitude [rad]
	* \param frame reference frame flag (see notes)
	* \return surface velocity [<b>m</b>]
	* \note The \a frame flag can be used to specify the reference frame to which the
	*   returned vector refers. The following values are supported:
    *   - 0: surface-relative (relative to local horizon)
	*   - 1: planet-local (relative to local planet frame)
	*   - 2: planet-local non-rotating
	*   - 3: global (maps to global frame and adds planet velocity)
	* \note \a frame = 0 and \a frame = 1 are provided for completeness only. They return (0,0,0)
	*   by definition.
	* \note \a frame = 2 returns the following vector for a planet with mean radius \e R and rotation
	*   period \e T:
	*   \f[ \vec{v} = \frac{2\pi R}{T} \cos(\mathrm{lat}) \left[\begin{array}{c}-\sin(\mathrm{lng}) \\ 0 \\ \cos(\mathrm{lng}) \end{array}\right] \f]
	* \note \a frame = 3 maps the vector given above into the global frame and adds the planet
	*   velocity.
	*/
OAPIFUNC VECTOR3 oapiGetGroundVector (OBJHANDLE hPlanet, double lng, double lat, int frame=2);

	/**
	 * \brief Returns the wind velocity at a given position in a planet's atmosphere.
	 * \param hPlanet planet handle
	 * \param lng longitude [rad]
	 * \param lat latitude [rad]
	 * \param altitude above mean planet radius [m]
	 * \param frame reference frame flag (see notes)
	 * \param windspeed If defined, receives the wind speed magnitude in the local horizon frame,
	 *    independent of the frame selected
	 * \return wind velocity vector relative to surface [<b>m</b>]
	 * \note The \a frame flag can be used to specify the reference frame to which the
	 *   returned vector refers. The following values are supported:
	 *   - 0: surface-relative (relative to local horizon)
	 *   - 1: planet-local (relative to local planet frame)
	 *   - 2: planet-local non-rotating (as 1, but adds the surface velocity, see \ref oapiGetGroundVector)
	 *   - 3: global (maps to global frame and adds planet velocity)
	 * \warning Local wind velocities are not currently implemented. The surface-relative
	 *   wind velocity is always (0,0,0). To ensure forward compatibility, plugins
	 *   should not rely on this limitation, but use this function instead.
	 */
OAPIFUNC VECTOR3 oapiGetWindVector (OBJHANDLE hPlanet, double lng, double lat, double alt, int frame = 0, double *windspeed = NULL);

	/**
	* \brief Returns the number of perturbation coefficients defined for a planet.
	* \details Returns the number of perturbation coefficients defined for a planet to describe the
	* latitude-dependent perturbation of its gaviational potential. A return value of 0 indicates
	* that the planet is considered to have a spherically symmetric gravity field.
	* \param hPlanet planet handle
	* \return Number of perturbation coefficients.
	* \note Even if a planet defines perturbation coefficients, its gravity perturbation may
	*  be ignored, if the user disabled nonspherical gravity sources, or if orbit
	*  stabilisation is active at a given time step. Use the
	*  VESSEL::NonsphericalGravityEnabled() function to check if a vessel uses the
	*  perturbation terms in the update of its state vectors.
	* \note Depending on the distance to the planet, Orbiter may use fewer perturbation
	*  terms than defined, if their contribution is negligible:\n \n
	*  If \f$ J_n(\frac{R}{r})^n < \epsilon,  n\geq 2 \f$, ignore all terms \f$ \geq n \f$, \n \n
	*  where R is the planet radius, r is the distance from the planet, and \f$ J_n \f$ is the n-
	*  2nd perturbation term defined for the planet.\n
	*  Orbiter uses \f$ \epsilon = 10^{-10} \f$
	*/
OAPIFUNC DWORD oapiGetPlanetJCoeffCount (OBJHANDLE hPlanet);

	/**
	* \brief Returns a perturbation coefficient for the calculation of a planet's gravitational potential.
	* \param hPlanet planet handle
	* \param n coefficient index
	* \return Perturbation coefficient \f$ J_{n+2} \f$
	* \note Valid indices \a n are 0 to oapiGetPlanetJCoeffCount()-1
	* \note Orbiter calculates the planet's gravitational potential \a U for a given distance \a r
	*  and latitude \f$ \phi \f$ by 
	* \f[
	* U(r,\phi) = \frac{GM}{r}\left[ 1 - \sum^{N}_{n=2} J_n\left(\frac{R}{r}\right)^2 P_n(\sin\phi) \right]
	* \f] 
	* where \a R is the planet's equatorial radius, \a M is its mass, \a G is the gravitational
	* constant, and \f$ P_n \f$ is the Legendre polynomial of order n.
	* \note Orbiter currently considers perturbations to be only a function of latitude
	*  (polar), not of longitude.
	* \note The first coefficient, \a n = 0, returns J2, which accounts for the ellipsoid shape
	*  of a planet (flattening). Higher perturbation terms are usually small compared
	*  to J2 (and not known for most planets).
	* \sa oapiGetPlanetJCoeffCount
	*/
OAPIFUNC double oapiGetPlanetJCoeff (OBJHANDLE hPlanet, DWORD n);
//@}


// =============================================================================================
/// \defgroup ElevationSupport Elevation data-related functions
// =============================================================================================
//@{
/**
 * \brief Returns a handle for elevation queries for a specified planet
 * \param hPlanet planet object handle
 * \return elevation query handle (or 0 if planet doesn't support elevation data)
 */
OAPIFUNC ELEVHANDLE oapiElevationManager (OBJHANDLE hPlanet);

/**
 * \brief Returns the elevation of a point on a planet surface
 * \param hPlanet planet object handle
 * \param lng longitude [rad]
 * \param lat latitude [rad]
 * \return Surface elevation above planet mean radius
 * \note The OBJHANDLE passed to this function must be a handle for a planetary body
 * \note The return value may be negative if the specified point is below the mean planet radius.
 * \note If no elevation data are available for the specified body, the function returns 0.
 */
OAPIFUNC double oapiSurfaceElevation (OBJHANDLE hPlanet, double lng, double lat);

struct ElevationTile;

/**
 * \brief Returns the elevation of a point on a planet surface (extended version)
 * \param hPlanet planet object handle
 * \param lng longitude [rad]
 * \param lat latitude [rad]
 * \param tgtlvl requested elevation resolution level (see notes)
 * \param [in,out] tilecache tile cache (see notes)
 * \param [out] nml if set, the vector pointed to will receive the surface normal (in the local horizon frame)
 * \param [out] lvl if set, the variable pointed to will receive the actual tile resolution from which the results were obtained
 * \return Surface elevation above planet mean radius
 * \note If tgtlvl == 0 (default), the function will calculate the elevation from the highest available elevation resolution. If tgtlvl > 0, the function will
 *   query at most that level (but may use a lower level if the requested resolution level is not available at the position).
 * \note Typically, lower resolutions would be requested from a vessel at high altitude, where exact surface elevation is not critical (and not realistically measurable
 *   anyway). Querying elevations from lower resolution data inmproves the probability of a cache hit and is therefore more efficient.
 * \note The tile cache is a container to store previously loaded elevation tiles and can speed up the function if the tile required for computing the request
 *   has been loaded before. The tile cache can be initialised with \ref InitTileCache and released with \ref ReleaseTileCache.
 * \note If the requested resolution level is not available at the queried location, the function computes the elevation from the highest available resolution
 *   level. The actual resolution used can be obtained by setting lvl.
 */
OAPIFUNC double oapiSurfaceElevationEx(OBJHANDLE hPlanet, double lng, double lat, int tgtlvl = 0, std::vector<ElevationTile> *tilecache = 0, VECTOR3 *nml = 0, int *lvl = 0);

/**
 * \brief Allocates an elevation data cache to speed up calls to \ref oapiSurfaceElevationEx
 * \param size Cache capacity: number of tiles to be held
 * \return pointer to tile cache
 * \note When passing a tile cache to oapiSurfaceElevationEx, any cache hits avoid having to re-load an elevation tile from file.
 * \note Should be called before the first invocation of oapiSurfaceElevationEx by the module (e.g. in the constructor)
 * \note The cache should persist for as long as the module continues querying elevations, and then be released with
 *   \ref ReleaseTileCache (e.g. in the destructor)
 * \note A cache size of 2 is usually sufficient for spacecraft, but surface vessels that stay in a local area may benefit
 *   from a larger cache.
 */
OAPIFUNC std::vector<ElevationTile> *InitTileCache(int size = 2);

/**
 * \brief Releases a tile cache previously allocated with \ref InitTileCache.
 * \param tilecache Pointer to the tile cache.
 * \note The tilecache pointer is no longer valid when the function returns.
 */
OAPIFUNC void ReleaseTileCache(std::vector<ElevationTile> *tilecache);
//@}

// =============================================================================================
/// \defgroup BaseInterface Surface base interface
// =============================================================================================
//@{
  /**
    * \brief Returns a handle for the planet/moon the given base is located on.
	* \param hBase base handle
	* \return Planet handle, or NULL if the base was not recognised.
	* \sa oapiGetBaseByIndex, oapiGetBaseByName
	*/
OAPIFUNC OBJHANDLE oapiGetBasePlanet (OBJHANDLE hBase);

   /**
	* \brief Returns the equatorial coordinates (longitude, latitude and radius) of the location of a surface base.
	* \param hBase surface base handle
	* \param lng pointer to variable to receive longitude value [rad]
	* \param lat pointer to variable to receive latitude value [rad]
	* \param rad pointer to variable to receive radius value [m]
	* \note hBase must be a valid base handle (e.g. from oapiGetBaseByName())
	* \note The radius pointer can be omitted if not required.
	* \note Currently, rad will always return the planet mean radius.
	*/
OAPIFUNC void oapiGetBaseEquPos (OBJHANDLE hBase, double *lng, double *lat, double *rad = 0);

	/**
	* \brief Returns the number of VTOL landing pads owned by the base.
	* \param hBase surface base handle
	* \return Number of landing pads
	* \note hBase must be a valid base handle (e.g. from oapiGetBaseByName())
	* \note This function only counts VTOL pads, not runways.
	*/
OAPIFUNC DWORD oapiGetBasePadCount (OBJHANDLE hBase);

	/**
	* \brief Returns the equatorial coordinates (longitude, latitude and radius)
	*  of the location of a VTOL landing pad.
	* \param hBase surface base handle
	* \param pad pad index
	* \param lng pointer to variable to receive longitude value [rad]
	* \param lat pointer to variable to receive latitude value [rad]
	* \param rad pointer to variable to receive radius value [m]
	* \return \e false indicates failure (pad index out of range). 
	*  In that case, the return values are undefined.
	* \note hBase must be a valid base handle (e.g. from oapiGetBaseByName())
	* \note 0 <= pad < oapiGetBasePadCount() is required.
	* \note The radius pointer can be omitted if not required.
	*/
OAPIFUNC bool oapiGetBasePadEquPos (OBJHANDLE hBase, DWORD pad, double *lng, double *lat, double *rad = 0);

	/**
	* \brief Returns the status of a VTOL landing pad (free, occupied or cleared).
	* \param hBase surface base handle
	* \param pad pad index
	* \param status pointer to variable to receive pad status
	* \return \e false indicates failure (pad index out of range)
	* \note hBase must be a valid base handle (e.g. from oapiGetBaseByName())
	* \note 0 <= pad < oapiGetBasePadCount() is required.
	* \note \e status can be one of the following:\n
	*  0 = pad is free\n
	*  1 = pad is occupied\n
	*  2 = pad is cleared for an incoming vessel
	*/
OAPIFUNC bool oapiGetBasePadStatus (OBJHANDLE hBase, DWORD pad, int *status);

	/**
	* \brief Returns a handle to the ILS transmitter of a VTOL landing pad, if available.
	* \param hBase surface base handle
	* \param pad pad index
	* \return Handle of a ILS transmitter, or NULL if the pad index is out of range or the pad has no ILS.
	* \note hBase must be a valid base handle (e.g. from oapiGetBaseByName())
	* \note 0 <= pad < oapiGetBasePadCount() is required.
	*/
OAPIFUNC NAVHANDLE oapiGetBasePadNav (OBJHANDLE hBase, DWORD pad);
//@}


// =============================================================================================
/// \defgroup oapi_time Time functions
// =============================================================================================
//@{
	/**
	* \brief Retrieve simulation time (in seconds) since simulation start.
	* \return Simulation up time (seconds)
	* \note Since the simulation up time depends on the simulation start time, this parameter
	*  is useful mainly for time differences. To get an absolute time parameter, use oapiGetSimMJD().
	*/
OAPIFUNC double oapiGetSimTime ();

	/**
	* \brief Retrieve length of last simulation time step (from previous to current frame) in seconds.
	* \return Simulation time step (seconds)
	* \note This parameter is useful for numerical (finite difference) calculation of time derivatives.
	*/
OAPIFUNC double oapiGetSimStep ();

	/**
	* \brief Retrieve system (real) time since simulation start.
	* \return Real-time simulation up time (seconds)
	* \note This function measures the real time elapsed since the simulation was
	*  started. Unlike oapiGetSimTime(), it doesn't take into account time acceleration.
	* \sa oapiGetSysMJD
	*/
OAPIFUNC double oapiGetSysTime ();

	/**
	* \brief Retrieve length of last system time step in seconds.
	* \return System time step (seconds)
	* \note Unlike oapiGetSimStep(), this function does not include the time compression
	* factor. It is useful to control actions which do not depend on the simulation time acceleration.
	*/
OAPIFUNC double oapiGetSysStep ();

	/**
	* \brief Retrieve absolute time measure (Modified Julian Date) for current simulation state.
	* \return Current Modified Julian Date (days)
	* \note Orbiter defines the Modified Julian Date (MJD) as JD - 240 0000.5, where JD is
	*  the Julian Date. JD is the interval of time in mean solar days elapsed since 4713
	*  BC January 1 at Greenwich mean noon.
	* \sa oapiSetSimMJD, oapiGetSimTime
	*/
OAPIFUNC double oapiGetSimMJD ();

	/**
	* \brief Retrieve the current computer system time in Modified Julian Date (MJD) format.
	* \return Computer system time in MJD format
	* \note The returned value is the UTC time obtained from the computer system clock,
	*   plus dt=66.184 seconds to map from UTC to TDB (Barycentric Dynamical Time) used
	*   internally by Orbiter. The dt offset was not added in previous Orbiter releases.
	* \sa oapiGetSysTime
	*/
OAPIFUNC double oapiGetSysMJD ();

	/** 
	* \brief Set the current simulation time. The simulation session performs a jump to the new time.
	* \param mjd new simulation time
	* \param pmode vessel propagation modes (see notes)
	* \return Currently this function always returns \e true.
	* \note The new time can be set before or after the current simulation time.
	* \note Deterministic objects (planets controlled by Keplerian elements or
	* perturbation code) are propagated directly. Vessels are propagated
	* according to pmode, which can be a combination of 
	* <table>
	* <tr><td><b>Orbital vessels</b></td><td>.</td></tr>
	* <tr><td>PROP_ORBITAL_ELEMENTS</td><td>Move the vessel along its current
	* orbital trajectory, assuming that no forces other than the central body's
	* gravitational force are acting on the vessel.</td></tr>
	* <tr><td>PROP_ORBITAL_FIXEDSTATE</td><td>Keep the vessel's relative position and
	* velocity with respect to the central body fixed in a non-rotating frame.</td></tr>
	* <tr><td>PROP_ORBITAL_FIXEDSURF</td><td>Keep the vessel's position velocity and
	* attitude fixed relative to the planet surface.</td></tr>
	* <tr><td><b>Suborbital vessels</b></td><td>.</td></tr>
	* <tr><td>PROP_SORBITAL_ELEMENTS</td><td>PROP_ORBITAL_ELEMENTS</td></tr>
	* <tr><td>PROP_SORBITAL_FIXEDSTATE</td><td>PROP_ORBITAL_FIXEDSTATE</td></tr>
	* <tr><td>PROP_SORBITAL_FIXEDSURF</td><td>PROP_ORBITAL_FIXEDSURF</td></tr>
	* <tr><td>PROP_SORBITAL_DESTROY</td><td>Destroy any suborbital vessels (i.e.
	* assume that the vessels impacted on the ground during time propagation).</td></tr>
	* </table>
	* \note \a pmode can be a bitwise combination of one of the orbital and one of the
	*  suborbital modes. Default is propagation along osculating elements for both.
	* \sa oapiGetSimMJD
	*/
OAPIFUNC bool oapiSetSimMJD (double mjd, int pmode = 0);

	/**
	* \brief Convert a simulation up time value into a Modified Julian Date.
	* \param simt simulation time (seconds)
	* \return Modified Julian Date (MJD) corresponding to simt.
	*/
OAPIFUNC double oapiTime2MJD (double simt);

	/**
	* \brief Returns simulation time acceleration factor.
	* \return time acceleration factor
	* \note This function will not return 0 when the simulation is paused. Instead it will
	*  return the acceleration factor at which the simulation will resume when
	*  unpaused. Use oapiGetPause to obtain the pause/resume state.
	* \sa oapiSetTimeAcceleration
	*/
OAPIFUNC double oapiGetTimeAcceleration ();

	/**
	* \brief Set the simulation time acceleration factor
	* \param warp new time acceleration factor
	* \note Warp factors will be clamped to the valid range [1,100000]. If the new warp
	*  factor is different from the previous one, all DLLs (including the one that
	*  called oapiSetTimeAcceleration()) will be sent a opcTimeAccChanged() message.
	* \sa oapiGetTimeAcceleration
	*/
OAPIFUNC void oapiSetTimeAcceleration (double warp);

	/**
	* \brief Returns current simulation frame rate (frames/sec).
	* \return Current frame rate (fps)
	*/
OAPIFUNC double oapiGetFrameRate ();

	/**
	* \brief Returns the current simulation pause state.
	* \return \e true if simulation is currently paused, \e false if it is running.
	* \sa oapiSetPause
	*/
OAPIFUNC bool oapiGetPause ();

	/**
	* \brief Sets the simulation pause state.
	* \param pause \e true to pause the simulation, \e false to resume.
	* \sa oapiGetPause
	*/
OAPIFUNC void oapiSetPause (bool pause);
//@}


// =============================================================================================
/// \defgroup NavRadio Navigation radio transmitter functions
// =============================================================================================
//@{
	/**
	* \brief Returns the current position of a NAV transmitter 
	*  (in global coordinates, i.e. heliocentric ecliptic).
	* \param hNav NAV transmitter handle
	* \param gpos pointer to variable to receive global position
	* \sa oapiGetNavRange, oapiGetNavType, oapiNavInRange
	*/
OAPIFUNC void oapiGetNavPos (NAVHANDLE hNav, VECTOR3 *gpos);

	/**
	* \brief Returns the channel number of a NAV transmitter.
	* \param hNav NAV transmitter handle
	* \return channel number
	* \note Channel numbers range from 0 to 639.
	* \note To convert a channel number ch into a frequency, use
	*  f = (108.0 + 0.05 ch) MHz
	* \sa oapiGetNavData, oapiGetNavFreq, oapiGetNavRange, oapiGetNavPos, oapiGetNavType
	*/
OAPIFUNC DWORD oapiGetNavChannel (NAVHANDLE hNav);

	/**
	* \brief Returns the frequency of a NAV transmitter.
	* \param hNav NAV transmitter handle
	* \return Transmitter frequency [MHz]
	* \note In Orbiter, NAV transmitter frequencies range from 108.0 to 139.95 MHz and
	*  are incremented in 0.05 MHz steps.
	* \sa oapiGetNavData, oapiGetNavChannel, oapiGetNavRange, oapiGetNavPos, oapiGetNavType
	*/
OAPIFUNC float oapiGetNavFreq (NAVHANDLE hNav);

    /**
	 * \brief Returns the signal strength of a transmitter at a given position.
	 * \param hNav transmitter handle
	 * \param gpos global position
	 * \return Signal strength in arbitrary units
	 * \note The transmitter signal strength drops off with the square of
	 *   distance to the transmitter. The units a chosen so that a 'default'
	 *   receiver will be able to detect signals above a strength of 1.
	 * \sa oapiGetNavData, oapiGetNavRange
	 */
OAPIFUNC double oapiGetNavSignal (NAVHANDLE hNav, const VECTOR3 &gpos);

	/**
	* \brief Returns the range of a NAV transmitter.
	* \param hNav NAV transmitter handle
	* \return Transmitter range [m]
	* \note A NAV receiver will only receive a signal when within the range of a transmitter.
	* \note Variable receiver sensitivity is not currently implemented.
	* \note Shadowing of a transmitter by obstacles between transmitter and receiver is
	*  not currently implemented.
	* \note Because the range of the transmitter depends on receiver gain as well
	*   as transmitter power, the range is not strictly a property of the
	*   transmitter. It is preferred to calculate the range for a given
	*   receiver gain by using the oapiGetNavData or oapiGetNavSignal functions.
	* \sa oapiGetNavData, oapiGetNavSignal, oapiGetNavPos, oapiGetNavType, oapiNavInRange
	*/
OAPIFUNC float oapiGetNavRange (NAVHANDLE hNav);

	/**
	* \brief Returns the type id of a NAV transmitter.
	* \param hNav NAV transmitter handle
	* \return transmitter type identifier
	* \note The following transmitter types are currently supported:
	* - \c TRANSMITTER_VOR (omnidirectional beacon)
	* - \c TRANSMITTER_VTOL (launchpad homing beacon)
	* -	\c TRANSMITTER_ILS (instrument landing system)
	* - \c TRANSMITTER_IDS (instrument docking system)
	* - \c TRANSMITTER_XPDR (transponder)
	* \sa oapiGetNavData, oapiGetNavDescr
	*/
OAPIFUNC DWORD oapiGetNavType (NAVHANDLE hNav);

	/**
	* \brief Returns information about a NAV transmitter.
	* \param [in] hNav NAV transmitter handle
	* \param [out] data pointer to NAVDATA structure receiving transmitter data
	* \return Error flag. Currently always returns 0.
	* \note On call, \a data must point to a NAVDATA variable.
	* \sa NAVDATA, oapiGetNavType
	*/
OAPIFUNC int oapiGetNavData (NAVHANDLE hNav, NAVDATA *data);

	/**
	* \brief Returns a descriptive string for a NAV transmitter.
	* \param hNav NAV transmitter handle
	* \param descr pointer to string receiving description
	* \param maxlen string buffer length
	* \return Number of characters returned (excluding terminating NULL character). If
	*  maxlen was not sufficient to store the complete description, the return value is negative.
	* \note This function fills string \a descr with a description of the NAV radio transmitter
	*  of lenght <= \a maxlen. If the buffer length is greater than required for the
	*  description, a NULL character is appended.
	* \note The description format for the different transmitter types is as follows:
	* <table>
	* <tr><td>VOR</td><td>"VOR <id>"</td><td>where \<id\> is a 3-4 letter sequence</td></tr>
	* <tr><td>VTOL</td><td>"VTOL Pad-<#> <base>"</td><td> where \<#\> is the pad number, and \<base\> is the base name</td></tr>
	* <tr><td>ILS</td><td>"ILS Rwy <#> <base>"</td><td>where \<#\> is the runway id, and \<base\> is the base name</td></tr>
	* <tr><td>IDS</td><td>"IDS D-<#> <vessel>"</td><td>where \<#\> is the dock number, and \<vessel\> is the vessel name</td></tr>
	* <tr><td>XPDR</td><td>"XPDR <vessel>"</td><td>where \<vessel\> is the vessel name</td></tr>
	* </table>
	*/
OAPIFUNC int oapiGetNavDescr (NAVHANDLE hNav, char *descr, int maxlen);

	/**
	* \brief Determines whether a given global coordinate is within the range of a NAV transmitter.
	* \param hNav NAV transmitter handle
	* \param gpos Global coordinates [m,m,m] of a point (cartesian heliocentric ecliptic)
	* \return \e true if the point is within range of the transmitter.
	*/
OAPIFUNC bool oapiNavInRange (NAVHANDLE hNav, const VECTOR3 &gpos);
//@}


// =============================================================================================
/// \defgroup Script Script interpreter functions
// =============================================================================================
//@{
	/**
	* \brief Returns a handle to a new interpreter instance.
	* \note The interpreter can subsequently be used to execute commands and scripts.
	* \sa oapiDelInterpreter, oapiExecScriptCmd
	*/
OAPIFUNC INTERPRETERHANDLE oapiCreateInterpreter ();

	/**
	* \brief Delete an interpreter instance.
	* \param hInterp interpreter handle
	* \note After the interpreter instance has been deleted, the handle becomes
	*   invalid and must not be used any more.
	* \note If the interpreter was executing a background script, the execution is
	*   terminated when the interpreter is deleted.
	* \sa oapiCreateInterpreter, oapiExecScriptCmd
	*/
OAPIFUNC int oapiDelInterpreter (INTERPRETERHANDLE hInterp);

	/**
	* \brief Executes a script command in an interpreter instance.
	* \param hInterp interpreter handle
	* \param cmd Lua command to be executed
	* \return \e false on error (interpreter library not found, or command error)
	* \note This function returns as soon as the command has been executed.
	* \sa oapiAsyncScriptCmd, oapiCreateInterpreter, oapiDelInterpreter
	*/
OAPIFUNC bool oapiExecScriptCmd (INTERPRETERHANDLE hInterp, const char *cmd);

	/**
	* \brief Passes a command to an interpreter instance for execution.
	* \param hInterp interpreter handle
	* \param cmd Lua command to be executed
	* \return \e false on error (interpreter library not found, or command error)
	* \note This function returns immediately. The command is executed during the
	*   next postStep cycle. If more asynchronous commands are issued before
	*   execution starts, they are appended to the execution list. If the
	*   interpreter receives a synchronous request (oapiExecScriptCmd) before the
	*   asynchrounous commands are executed, the synchronous command is executed
	*   immediately, while the asynchronous requests continue waiting.
	* \sa oapiExecScriptCmd, oapiCreateInterpreter, oapiDelInterpreter
	*/
OAPIFUNC bool oapiAsyncScriptCmd (INTERPRETERHANDLE hInterp, const char *cmd);

//typedef struct lua_State lua_State;
OAPIFUNC lua_State *oapiGetLua (INTERPRETERHANDLE hInterp);
//@}

// =============================================================================================
/// \defgroup Mesh Visual and mesh functions
// =============================================================================================
//@{
	/**
	* \brief Returns a pointer storing the objects visual handle.
	* \param hObject object handle
	* \return pointer to visual handle
	* \note Returns a pointer that stores the object's visual handle whenever the object
	*  is within visual range of the camera. When the object is out of range, the
	*  pointer is set to NULL.
	* \note This function currently only works for vessel objects. All other object types
	*  return a pointer to NULL.
	*/
OAPIFUNC VISHANDLE *oapiObjectVisualPtr (OBJHANDLE hObject);

	/**
	* \brief Loads a mesh from file and returns a handle to it.
	* \param fname mesh file name
	* \return Handle to the loaded mesh. (NULL indicates load error)
	* \note The file name should not contain a path or file extension. Orbiter appends
	*  extension .msh and searches in the default mesh directory.
	* \note Meshes should be deallocated with oapiDeleteMesh when no longer needed.
	* \sa oapiDeleteMesh, VESSEL::AddMesh
	*/
OAPIFUNC MESHHANDLE oapiLoadMesh (const char *fname);

	/**
	* \brief Retrieves a mesh handle from the global mesh manager. 
	* \details When called for the first time for any given file name, the mesh is loaded from file 
	* and stored as a system resource. Every further request for the same mesh directly returns
	* a handle to the stored mesh without additional file I/O.
	* \param fname mesh file name
	* \return mesh handle
	* \note Once a mesh is globally loaded it remains in memory until the user closes
	*  the simulation window.
	* \note This function can be used to pre-load meshes to avoid load delays during
	*  the simulation. For example, parent objects may pre-load meshes for any
	*  child objects they may create later.
	* \note Do NOT delete any meshes obtained by this function with oapiDeleteMesh()
	*  Orbiter takes care of deleting globally managed meshes.
	* \note If you assign the mesh to a vessel with a subsequent VESSEL::AddMesh()
	*  call, a copy of the global mesh is created every time the vessel creates its
	*  visual, and discarded as soon as the visual is deleted. The global mesh can
	*  therefore be regarded as a template from which individual vessel instances
	*  make copies whenever they need to initialise their visual representation.
	*  Handles for the individual mesh copies can be obtained within the
	*  VESSEL2::clbkVisualCreated() callback function, using the
	*  VESSEL::GetMesh() method. Vessels should only modify their individual
	*  meshes, never the global template, since the latter is shared across all vessel instances.
	* \note For external graphics clients, the Orbiter core forwards the mesh data
	*  to the client for conversion to a device-specific format. The mesh template
	*  referred to by the handle returned by oapiLoadMeshGlobal is then no longer
	*  used, so any changes made to it will be ignored.
	*/
OAPIFUNC const MESHHANDLE oapiLoadMeshGlobal (const char *fname);

	/**
	 * \brief Callback function used by \ref oapiLoadMeshGlobal(const char*,LoadMeshClbkFunc)
	 * \param hMesh mesh handle
	 * \param firstload flag indicating if the mesh has been loaded for the first time
	 * \note If firstload==false, the mesh had already been loaded previously. In this
	 *   case, the mesh is not re-loaded, and the returned handle points to the
	 *   previously loaded mesh.
	 */
typedef void (*LoadMeshClbkFunc)(MESHHANDLE hMesh, bool firstload);

	/**
	 * \brief Retrieves a mesh handle from the global mesh manager.
	 * \param fname mesh file name
	 * \param fClbk Callback function for mesh modification
	 * \return mesh handle
	 * \note This function is identical to \ref oapiLoadMeshGlobal(const char*),
	 *   except that it invokes the callback function immediately after loading
	 *   the mesh. This is important in combination with external graphics clients,
	 *   because Orbiter hands the loaded mesh on to the client for conversion to
	 *   a device-specific format. The callback function is invoked before the
	 *   mesh is passed to the graphics client. This allows to apply modifications
	 *   (e.g. decryption) while the mesh is still in an editable format.
	 *   Applying the modifications to the mesh handle returned by oapiLoadMeshGlobal
	 *   would not work in this case, because the mesh has already been copied to the
	 *   client.
	 */
OAPIFUNC const MESHHANDLE oapiLoadMeshGlobal (const char *fname, LoadMeshClbkFunc fClbk);

	/**
	 * \brief Retrieves a mesh finename form a handle.
	 * \param hMesh mesh handle
	 */
OAPIFUNC const char* oapiGetMeshFilename(MESHHANDLE hMesh);

	/**
	 * \brief Creates a new mesh from a list of mesh group definitions.
	 * \param ngrp number of groups in the list
	 * \param grp list of mesh groups
	 * \return Handle for the newly created mesh.
	 * \note Orbiter performs a deep copy of the group definitions passed to
	 *   the functions. Therefore it is admissable to pass the groups as
	 *   variables with local scope. If the mesh groups were dynamically
	 *   allocated, they should be deallocated by the caller after use.
	 */
OAPIFUNC MESHHANDLE oapiCreateMesh (DWORD ngrp, MESHGROUP *grp);

	/**
	* \brief Removes a mesh from memory.
	* \param hMesh mesh handle
	*/
OAPIFUNC void oapiDeleteMesh (MESHHANDLE hMesh);

	/**
	* \brief Returns the bit flags for the mesh.
	* \return mesh flags
	*   <table>
	*   <tr><td>bit</td><td>effect</td></tr>
	*   <tr><td>0</td><td>set: mesh cast shadow on planetary surfaces</td></tr>
	*   <tr><td>1</td><td>set: use global shadow setting (bit 0) for all groups; unset: use individual group flags</td></tr>
	*   </table>
	*/
OAPIFUNC DWORD oapiGetMeshFlags (MESHHANDLE hMesh);

	/**
	* \brief Returns the number of mesh groups defined in a mesh.
	* \param hMesh mesh handle
	* \return number of mesh groups defined in the mesh
	* \note Each mesh is subdivided into mesh groups, defining a part of the 3-D object
	*  represented by the mesh.
	* \note A group consists of a list of vertex coordinates and vertex indices,
	*  representing its geometry, and optionally a material and a texture reference.
	* \note See 3DModel document for details of the mesh format.
	*/
OAPIFUNC DWORD oapiMeshGroupCount (MESHHANDLE hMesh);

	/**
	* \brief Returns a pointer to the group specification of a mesh group.
	* \param hMesh mesh handle
	* \param idx group index (>=0)
	* \return pointer to mesh group specification (or NULL if idx out of range)
	* \note \b MESHGROUP is a structure that contains the components of the
	*   group, including vertex list, index list, texture and material index.
	* \note This method can be used to edit the a mesh group directly (for geometry
	*  animation, texture animation, etc.)
	* \note This function should only be applied to device-independent meshes,
	*   such as mesh templates.
	* \note For device-dependent mesh instances (such as returned by
	*   \ref VESSEL::GetDevMesh) use \ref oapiEditMeshGroup instead.
	* \sa oapiEditMeshGroup
	*/
OAPIFUNC MESHGROUP *oapiMeshGroup (MESHHANDLE hMesh, DWORD idx);
OAPIFUNC MESHGROUP *oapiMeshGroup (DEVMESHHANDLE hMesh, DWORD idx);
OAPIFUNC MESHGROUPEX *oapiMeshGroupEx (MESHHANDLE hMesh, DWORD idx);

OAPIFUNC DWORD      oapiAddMeshGroup (MESHHANDLE hMesh, MESHGROUP *grp);
OAPIFUNC bool       oapiAddMeshGroupBlock (MESHHANDLE hMesh, DWORD grpidx,
										   const NTVERTEX *vtx, DWORD nvtx, const WORD *idx, DWORD nidx);

/**
 * \brief Retrieve mesh group data.
 * \param [in] hMesh mesh handle
 * \param [in] grpidx mesh group index (>= 0)
 * \param [in,out] grs data buffers and buffer sizes
 * \return Error code:
 *    * 0: success
 *    * -1: no graphics client attached
 *    * -2: graphics client hasn't implemented this function
 *    * 1: grpidx is out of bounds
 *    * 2: some indices in VtxPerm or IdxPerm were out of bounds (but data are still returned for the rest)
 * \note The vertex buffer (grs.Vtx), index buffer (grs.Idx) and vertex permutation buffer
 *    (grs.VtxPerm) must be allocated by the caller to sufficient size, or set to NULL to
 *    indicate that they are not required.
 * \note If vertex data should be returned, nVtx should be set to the maximum number of
 *    vertices to return, and Vtx must be allocated to at least this size. If the group
 *    contains fewer vertices, the Vtx buffer is only partially filled, and nVtx is set to
 *    the actual number of returned vertices. If the group contains more vertices, and
 *    VtxPerm is NULL, only the first nVtx vertics are returned.
 * \note If an arbitrary subset of vertices should be returned, assign the VtxPerm buffer
 *    to at least size nVtx, and fill it with the indices of the vertices you want returned.
 *    The order of vertices returned in Vtx will correspond to VtxPerm. If VtxPerm 
 *    contains any indices outside the valid range, the corresponding entries in Vtx will
 *    be filled with {0} vertices, and the function will return 2.
 * \note If no vertex data are requested, set Vtx to NULL and/or nVtx to 0.
 * \note Similar for triangle index data: If index data should be returned, set nIdx > 0
 *    and allocate Idx to at least size nIdx.
 * \note If you want indices returned from the beginning, set IdxPerm to NULL. Otherwise,
 *    allocate IdxPerm and fill it with the requested triangle indices.
 * \note The MtrlIdx and TexIdx entries are always returned.
 * \note oapiGetMeshGroup can be an expensive operation. It involves data copying, and
 *    Graphics clients may have to retrieve data from video memory. Avoid continuous
 *    oapiGetMeshGroup/oapiEditMesh cycles and instead keep the data stored in your own
 *    buffers once retrieved.
 */
OAPIFUNC int oapiGetMeshGroup (DEVMESHHANDLE hMesh, DWORD grpidx, GROUPREQUESTSPEC *grs);

/**
 * \brief Modify mesh group data.
 * \param hMesh mesh handle
 * \param grpidx mesh group index (>= 0)
 * \param ges replacement/modification data for the group
 * \return 0 on success, or error code
 * \note This function allows to modify a mesh group, by replacing vertex data,
 *   or group flags.
 * \note It should not be used to apply a linear transformation to the entire
 *   group (use \ref VESSEL::MeshgroupTransform instead), because such transformations
 *   are usually implemented by defining a transformation matrix instead of
 *   editing the vertex positions directly.
 * \note This version operates on device-independent meshes, e.g. mesh templates.
 * \note oapiEditMeshGroup should be used in preference to \ref oapiMeshGroup,
 *   because it is more likely to be supported by external graphics engines.
 * \sa oapiEditMeshGroup(DEVMESHHANDLE,DWORD,GROUPEDITSPEC*)
 */
OAPIFUNC int oapiEditMeshGroup (MESHHANDLE hMesh, DWORD grpidx, GROUPEDITSPEC *ges);

OAPIFUNC int oapiEditMeshGroup (DEVMESHHANDLE hMesh, DWORD grpidx, GROUPEDITSPEC *ges);

/**
 * \brief Returns the number of textures associated with a mesh.
 * \param hMesh mesh handle
 * \return Number of textures
 * \sa oapiGetTextureHandle, oapiSetTexture
 */
OAPIFUNC DWORD oapiMeshTextureCount (MESHHANDLE hMesh);

	/**
	* \brief Retrieve a surface handle for a mesh texture.
	* \param hMesh mesh handle
	* \param texidx texture index (>=1)
	* \return surface handle
	* \note This function can be used for dynamically updating textures during the simulation.
	* \note the texture index is given by the order in which the textures appear in the
	*  texture list at the end of the mesh file.
	* \note Important: Any textures which are to be dynamically modified should be
	*  listed with the "D" flag ("dynamic") in the mesh file. This causes Orbiter to
	*  decompress the texture when it is loaded. Blitting operations to compressed
	*  surfaces is very inefficient on most graphics hardware.
	*/
OAPIFUNC SURFHANDLE oapiGetTextureHandle (MESHHANDLE hMesh, DWORD texidx);

	/**
	* \brief Load a texture from a file.
	* \param fname texture file name
	* \param dynamic allow dynamic modification
	* \return Surface handle for the loaded texture, or NULL if not found.
	* \note Textures loaded by this function should be in DDS format and conform to the
	*  DirectX restrictions for texture surfaces, typically square bitmaps with
	*  dimensions of powers of 2 (128x128, 256x256, etc.).
	* \note File names can contain search paths. Orbiter searches for textures in the
	*  standard way, i.e. first searches the HitexDir directory (usually Textures2),
	*  then the TextureDir directory (usually Textures). All search paths are relative
	*  to the texture root directories. For example, oapiLoadTexture()
	*  ("myvessel\mytex.dds") would first search for Textures2\\myvessel\\mytex.dds, 
	*  then for Textures\\myvessel\\mytex.dds.
	*/
OAPIFUNC SURFHANDLE oapiLoadTexture (const char *fname, bool dynamic = false);

	/**
	* \brief Release a texture.
	* \param hTex Texture surface handle.
	* \note After the function returns, the surface handle is invalid and should no
	*   longer be used.
	* \note Do not release textures that a referenced by a mesh. Mesh textures are
	*   released automatically.
	*/
OAPIFUNC void oapiReleaseTexture (SURFHANDLE hTex);

	/**
	* \brief Replace a mesh texture.
	* \param hMesh mesh handle
	* \param texidx texture index (>=1)
	* \param tex texture handle
	* \return \e true if texture was set successfully, \e false if texidx is out of range.
	* \note This function replaces one of the mesh textures. All mesh groups referencing
	*  the corresponding texture index will show the new texture.
	* \note texidx must be in the range [1..n] where n is the length of the texture list in
	*  the mesh, i.e. textures can be replaced, but no new textures added.
	* \note To point an individual mesh group to a different texture, use oapiMeshGroup()
	*  to retrieve a \c MESHGROUP pointer, and modify the TexIdx entry.
	*/
OAPIFUNC bool oapiSetTexture (MESHHANDLE hMesh, DWORD texidx, SURFHANDLE tex);
OAPIFUNC bool oapiSetTexture (DEVMESHHANDLE hMesh, DWORD texidx, SURFHANDLE tex);

	/**
	* \brief Returns the number of materials defined in the mesh.
	* \param hMesh mesh handle
	* \return number of materials defined in the mesh
	* \note A mesh can contain a number of material specifications, and individual mesh
	*  groups can be linked to a material via the MtrlIdx entry in the group specification.
	* \note A material defines the diffuse, ambient, specular and emissive colour
	*  components of a mesh group, and also its level of transparency.
	* \note See 3DModel document for details of the mesh format.
	*/
OAPIFUNC DWORD oapiMeshMaterialCount (MESHHANDLE hMesh);

	/**
	* \brief Returns a pointer to a material specification in the material list of the mesh.
	* \param hMesh mesh handle
	* \param idx material index (>=0)
	* \return pointer to material specification (or NULL if idx out of range)
	* \note \c MATERIAL is a structure defined as follows:
	* \code
	* typedef struct {    // material definition
	*   COLOUR4 diffuse;  // diffuse component
	*   COLOUR4 ambient;  // ambient component
	*   COLOUR4 specular; // specular component
	*   COLOUR4 emissive; // emissive component
	*   float power;      // specular power
	* } MATERIAL; \endcode
	* where \c COLOUR4 defines a 4-valued (RGBA) colour component (red, green, blue, opacity):
	* \code
	* typedef struct { // vertex definition including normals and texture coordinates
	*   float r; // red component
	*   float g; // green component
	*   float b; // blue component
	*   float a; // opacity
	* } COLOUR4; \endcode
	* \note colour component entries are in the range 0..1. Values > 1 may sometimes
	*  be used to obtain special effects.
	* \note This function can be used to edit mesh materials directly.
	* \note This function should only be used for mesh templates, not for
	*   device-specific rendering meshes (except for Orbiter's built-in
	*   graphics engine). For device meshes, use \erf oapiSetMaterial instead.
	* \sa oapiAddMaterial, oapiDeleteMaterial, oapiMeshMaterialCount
	*/
OAPIFUNC MATERIAL *oapiMeshMaterial (MESHHANDLE hMesh, DWORD idx);

	/**
	 * \brief Retrieve properties of a device mesh material.
	 * \param hMesh device mesh handle
	 * \param idx material index (>= 0)
	 * \param mat pointer to MATERIAL structure to be filled by the method
	 * \return Error code (0 = success)
	 */
OAPIFUNC int oapiMeshMaterial (DEVMESHHANDLE hMesh, DWORD idx, MATERIAL *mat);

	/**
	 * \brief Retrieve properties of a device mesh material.
	 * \param hMesh device mesh handle
	 * \param idx material index (>= 0)
	 * \param prp material property to be acquired.
	 * \param out pointer to FVECTOR4 structure to be filled by the method.
	 * \return Error code (0 = success)
	 */
OAPIFUNC int oapiMeshMaterialEx(DEVMESHHANDLE hMesh, DWORD idx, MatProp prp, oapi::FVECTOR4* out);

	/**
	 * \brief Add a material definition to a mesh.
	 * \param hMesh mesh handle
	 * \param mat pointer to material definition
	 * \return Material index in the mesh.
	 * \note The material is appended to the mesh material list.
	 * \sa oapiMeshMaterial, oapiDeleteMaterial, oapiMeshMaterialCount
	 */
OAPIFUNC DWORD oapiAddMaterial (MESHHANDLE hMesh, MATERIAL *mat);

	/**
	 * \brief Delete a material definition from the mesh.
	 * \param hMesh mesh handle
	 * \param idx material index (>= 0)
	 * \return \e false indicates failure (index out of range)
	 * \note This function adjusts all mesh group material indices
	 *   to account for the modified material table. Any groups that
	 *   referenced the deleted material are reset to material 0
	 *   (default material).
	 * \sa oapiMeshMaterial, oapiAddMaterial, oapiMeshMaterialCount
	 */
OAPIFUNC bool oapiDeleteMaterial (MESHHANDLE hMesh, DWORD idx);

	/**
	 * \brief Reset the properties of a mesh material.
	 * \param hMesh device mesh handle
     * \param matidx material index (>= 0)
	 * \param mat pointer to new material properties.
	 * \return Error flag: 0=success, 1=no graphics engine attached,
	 *   2=graphics engine does not support operation, 3=invalid mesh handle,
	 *   4=material index out of range.
	 * \note This function can be used to reset the parameters of an existing
	 *   mesh material.
	 * \note To add a new material, use \ref oapiAddMaterial instead.
	 */
OAPIFUNC int oapiSetMaterial (DEVMESHHANDLE hMesh, DWORD matidx, const MATERIAL *mat);

	/**
	 * \brief Reset the properties of a mesh material.
	 * \param hMesh device mesh handle
	 * \param matidx material index (>= 0)
	 * \param prp Material property to be set.
	 * \param in pointer to new material properties. Set to NULL to reset to shader defaults.
	 * \return Error flag: 0=success, 1=no graphics engine attached,
	 *   2=graphics engine does not support operation, 3=invalid mesh handle,
	 *   4=material index out of range, 5 = material property not supported by shader used by the mesh.
	 * \note This function can be used to reset the parameters of an existing
	 *   mesh material.
	 * \note To add a new material, use \ref oapiAddMaterial instead.
	 */
OAPIFUNC int oapiSetMaterialEx(DEVMESHHANDLE hMesh, DWORD matidx, MatProp prp, const oapi::FVECTOR4* in);

	/**
	 * \brief Set custom properties for a mesh.
	 * \param hMesh mesh handle
	 * \param property property tag
	 * \param value new mesh property value
	 * \return \e true if the property tag was recognised and the request could be executed, \e false otherwise.
	 * \note Currently only a single mesh property is recognised, but this may be
	 *  extended in future versions:
	 * - \c MESHPROPERTY_MODULATEMATALPHA \n \n
	 * if value==0 (default) disable material alpha information in textured mesh groups (only use texture alpha channel).\n
	 * if value<>0 modulate (mix) material alpha values with texture alpha maps.
	 * \sa oapiSetMeshProperty(DEVMESHHANDLE,DWORD,DWORD)
	 */
OAPIFUNC bool oapiSetMeshProperty (MESHHANDLE hMesh, DWORD property, DWORD value);

	/**
     * \brief Set custom properties for a device-specific mesh.
	 * \param hMesh device mesh handle
	 * \param property property tag
	 * \param value new mesh property value
	 * \return \e true if the property tag was recognised and the request could be executed, \e false otherwise.
	 * \note Currently only a single mesh property is recognised, but this may be
	 *  extended in future versions:
	 * - \c MESHPROPERTY_MODULATEMATALPHA \n \n
	 * if value==0 (default) disable material alpha information in textured mesh groups (only use texture alpha channel).\n
	 * if value<>0 modulate (mix) material alpha values with texture alpha maps.		
	 * \sa oapiSetMeshProperty(MESHHANDLE,DWORD,DWORD)
	 */
OAPIFUNC bool oapiSetMeshProperty (DEVMESHHANDLE hMesh, DWORD property, DWORD value);

// Particle functions
	/**
	* \brief Reset the reference pointer used by the particle stream to calculate the intensity
	*  (opacity) of the generated particles.
	* \param ph particle stream handle
	* \param lvl pointer to variable defining particle intensity
	* \note The variable pointed to by lvl should be set to values between 0 (lowest
	*  intensity) and 1 (highest intensity).
	* \note By default, exhaust streams are linked to the thrust level setting of the
	*  thruster they are associated with. Reentry streams are set to a fixed level of 1 by default.
	* \note This function allows to customise the appearance of the particle streams directly by the module.
	* \note Other parameters besides the intensity level, such as atmospheric density
	*  can also have an effect on the particle intensity.
	*/
OAPIFUNC void oapiParticleSetLevelRef (PSTREAM_HANDLE ph, double *lvl);
//@}


// =============================================================================================
/// \defgroup Panel HUD, MFD and panel functions
// =============================================================================================
//@{
	/**
	* \brief Set HUD (head up display) mode.
	* \param mode new HUD mode
	* \return \e true if mode has changed, \e false otherwise.
	* \note Mode \c HUD_NONE will turn off the HUD display.
	* \note See constants HUD_xxx for currently supported HUD modes.
	* \sa \ref hudmode "HUD Modes", oapiGetHUDMode
	*/
OAPIFUNC bool oapiSetHUDMode (int mode);

   /**
	* \brief Set HUD (head up display) mode with mode-specific parameters.
	* \param mode new HUD mode
	* \param prm mode-specific parameters
	* \return \e true if mode has changed, \e false otherwise.
	* \note Mode \c HUD_NONE will turn off the HUD display.
	* \note See constants HUD_xxx for currently supported HUD modes.
	* \sa \ref hudmode "HUD Modes", HUDPARAM, oapiGetHUDMode, oapiGetHUDMode(const HUDPARAM*)
    */
OAPIFUNC bool oapiSetHUDMode (int mode, const HUDPARAM *prm);

	/**
	* \brief Query current HUD (head up display) mode.
	* \return Current HUD mode
	* \sa \ref hudmode "HUD Modes", oapiGetHUDMode(const HUDPARAM*), oapiSetHUDMode
	*/
OAPIFUNC int oapiGetHUDMode ();

	/**
	* \brief Query current HUD mode and mode parameters.
	* \param prm pointer to HUD parameter structure to be filled.
	* \return Current HUD mode
	* \sa \ref hudmode "HUD Modes", HUDPARAM, oapiGetHUDMode()
	*/
OAPIFUNC int oapiGetHUDMode (HUDPARAM *prm);

	/**
	* \brief Switch the HUD display to a different colour.
	* \note Orbiter currently defines 3 HUD colours: green, red, white. Calls to
	*  oapiToggleHUDColour will cycle through these.
	* \sa oapiIncHUDIntensity, oapiDecHUDIntensity
	*/
OAPIFUNC void oapiToggleHUDColour ();

	/**
	 * \brief Return the current HUD brightness setting
	 * \return Brightness value (0..1)
	 */
OAPIFUNC double oapiGetHUDIntensity ();

	/**
	* \brief Set the HUD brightness
	* \param val brightness setting (0..1)
	* \sa oapiGetHUDIntensity, oapiIncHUDIntensity, oapiDecHUDIntensity
	*/
OAPIFUNC void oapiSetHUDIntensity (double val);

	/**
	* \brief Increase the brightness of the HUD display.
	* \note Calling this function will increase the intensity (in virtual cockpit modes) or
	*  brightness (in other modes) of the HUD display up to a maximum value.
	* \note This function should be called repeatedly (e.g. while the user presses a key).
	* \sa oapiSetHUDIntensity, oapiDecHUDIntensity
	*/
OAPIFUNC void oapiIncHUDIntensity ();

	/**
	* \brief Decrease the brightness of the HUD display.
	* \note Calling this function will decrease the intensity (in virtual cockpit modes) or
	*  brightness (in other modes) of the HUD display down to a minimum value.
	* \note This function should be called repeatedly (e.g. while the user presses a key).
	* \sa oapiSetHUDIntensity, oapiIncHUDIntensity
	*/
OAPIFUNC void oapiDecHUDIntensity ();

	/**
	* \brief Render custom HUD elements.
	* \param hMesh HUD mesh handle
	* \param hTex array of texture handles
	* \note This function should only be called from within VESSEL3::clbkRenderHUD.
	* \note It can be used to render custom HUD elements in glass cockpit and 2-D
	*   panel mode.
	* \note The mesh handle must refer to a 2-D mesh (z-components of all vertices
	*   are zero). The x and y components are in units of screen pixels.
	* \note The mesh may have multiple groups, but generally a single group should
	*   be sufficient. The texture indices of each group refer to the textures in
	*   the hTex list (starting with 0). If only a single texture is used, the
	*   texture index in the mesh should be set to 0, and hTex should be a pointer
	*   to the surface handle.
	* \note Mesh animations can be applied by modifying vertex and/or texture
	*   coordinates at each frame.
	*/
OAPIFUNC void oapiRenderHUD (MESHHANDLE hMesh, SURFHANDLE *hTex);

	/**
	* \brief Set an MFD (multifunctional display) to a specific mode.
	* \param mode MFD mode
	* \param mfd MFD identifier (e.g. \c MFD_LEFT, \c MFD_RIGHT)
	* \note mode \c MFD_NONE will turn off the MFD.
	* \note For the on-screen instruments, only \c MFD_LEFT and \c MFD_RIGHT are
	*  supported. Custom panels may support (up to 3) additional MFDs.
	* \sa \ref mfdidentifier "MFD Identifiers", \ref mfdmode "MFD Modes"
	*/
OAPIFUNC void oapiOpenMFD (int mode, int mfd);

	/**
	 * \brief Switches an MFD on or off.
	 * \param mfd MFD identifier (e.g. \c MFD_LEFT, \c MFD_RIGHT)
	 * \note Flips the on/off state of an MFD. Typically used to respond to
	 *   the user pressing the "power" button.
	 * \sa oapiOpenMFD
	 */
OAPIFUNC void oapiToggleMFD_on (int mfd);

	/**
	* \brief Get the current mode of the specified MFD.
	* \param mfd MFD identifier (e.g. \c MFD_LEFT, \c MFD_RIGHT)
	* \return \ref mfdmode "MFD Mode" 
	* \sa \ref mfdidentifier "MFD Identifiers"
	*/
OAPIFUNC int oapiGetMFDMode (int mfd);

	/**
	* \brief Modify the refresh interval of the specified MFD instrument.
	* \param mfd MFD identifier (e.g. \c MFD_LEFT, \c MFD_RIGHT)
	* \param multiplier refresh interval scale factor (1 = default)
	* \return Previous multiplier value
	* \note The actual refresh rate is the product of the multiplier and the
	*   base refresh rate, which is globally user-selectable in the Launchpad
	*   dialog, and my be modified by specific MFD modes.
	* \note The multiplier is reset to 1 whenever the panel mode or virtual
	*   panel position is changed, so oapiSetMFDRefreshIntervalMultiplier
	*   should be called during VESSEL2::clbkLoadVC, VESSEL3::clbkLoadPanel2D
	*   or VESSEL2::clbkLoadGenericCockpit.
	*/
OAPIFUNC double oapiSetMFDRefreshIntervalMultiplier (int mfd, double multiplier=1.0);

OAPIFUNC int oapiBroadcastMFDMessage (int mode, int msg, void *data);
	
	/**
	* \brief Sends a keystroke to an MFD.
	* \param mfd MFD identifier (e.g. \c MFD_LEFT, \c MFD_RIGHT)
	* \param key key code (see \ref keycodes "OAPI_KEY_xxx" Constants)
	* \return nonzero if the MFD understood and processed the key.
	* \note This function can be used to interact with the MFD as if the user had pressed
	*  Shift-key, for example to select a different MFD mode, to select a target body, etc.
	* \sa \ref mfdidentifier "MFD Identifiers"
	*/
OAPIFUNC int oapiSendMFDKey (int mfd, DWORD key);

	/**
	* \brief Sends a clbkMFDMode call to the current focus vessel to allow it to dynamically update
	*  its button labels.
	* \param mfd MFD identifier (e.g. \c MFD_LEFT, \c MFD_RIGHT)
	* \param hVessel recipient vessel handle
	* \note This message will only be sent to the current input focus vessel. If hVessel != 0,
	*  the function will not have any effect unless hVessel points to the focus vessel.
	* \note The recipient vessel will receive a VESSEL2::clbkMFDMode call, with the mode
	*  parameter set to \c MFD_REFRESHBUTTONS.
	* \note This function can be used to force an MFD to refresh its button labels even if
	*  the mode has not changed. This is useful to update the labels for modes that
	*  dynamically update their labels.
	* \note You don't need to call oapiRefreshMFDButtons after an actual mode
	*  change, because a clbkMFDMode call will be sent automatically by Orbiter.
	* \sa \ref mfdidentifier "MFD Identifiers"
	*/
OAPIFUNC void oapiRefreshMFDButtons (int mfd, OBJHANDLE hVessel = 0);

	/**
	* \brief Requests a default action as a result of a MFD button event.
	* \param mfd MFD identifier (e.g. \c MFD_LEFT, \c MFD_RIGHT)
	* \param bt button number (>=0)
	* \param event mouse event (a combination of \ref panel_mouse "PANEL_MOUSE_xxx" flags)
	* \return Returns \e true if the button was processed, \e false if no action was assigned to the button.
	* \note Orbiter assigns default button actions for the various MFD modes. For
	*  example, in Orbit mode the action assigned to button 0 is Select reference.
	*  Calling oapiProcessMFDButton (for example as a reaction to a mouse button
	*  event) will execute this action.
	*/
OAPIFUNC bool oapiProcessMFDButton (int mfd, int bt, int event);
	
	/**
	* \brief Retrieves a default label for an MFD button.
	* \param mfd MFD identifier (e.g. \c MFD_LEFT, \c MFD_RIGHT)
	* \param bt button number (>=0)
	* \return pointer to static string containing the label, or NULL if the button is not assigned.
	* \note Labels contain 1 to 3 characters.
	* \note This function can be used to paint the labels on the MFD buttons of a custom panel.
	* \note The labels correspond to the default button actions executed by VESSEL::ProcessMFDButton().
	* \sa \ref mfdidentifier "MFD Identifiers"
	*/
OAPIFUNC const char *oapiMFDButtonLabel (int mfd, int bt);

	/**
	* \brief Registers an MFD position for a custom panel.
	* \param mfd MFD identifier (e.g. \c MFD_LEFT, \c MFD_RIGHT)
	* \param spec MFD parameters (see below)
	* \note Should be called in the body of VESSEL2::clbkLoadPanel() for panels which define MFDs.
	* \note Defining more than 2 or 3 MFDs per panel can degrade performance.
	* \note MFDSPEC is a structure with the following interface:
	* \code 
	* typedef struct {
	*   RECT pos;       // position of MFD in panel (pixel)
	*   int nbt_left;   // number of buttons on left side of MFD display
	*   int nbt_right;  // number of buttons on right side of MFD display
	*   int bt_yofs;    // y-offset of top button from top display edge (pixel)
	*   int bt_ydist;   // y-distance between buttons (pixel)
	* } MFDSPEC; \endcode
	* \sa \ref mfdidentifier "MFD Identifiers"
	*/
OAPIFUNC void oapiRegisterMFD (int mfd, const MFDSPEC &spec);

	/**
	* \brief Registers an MFD position for a custom panel or virtual cockpit. This version has an
	*  extended parameter list.
	* \param mfd MFD identifier (e.g. \c MFD_LEFT, \c MFD_RIGHT)
	* \param spec extended MFD parameters (see below)
	* \note Should be called in the body of VESSEL2::clbkLoadPanel() or
	*  VESSEL2::clbkLoadVC() to define MFD instruments for 2-D instrument panels
	*  or 3-D virtual cockpits.
	* \note EXTMFDSPEC is a structure with the following interface:
	* \code
	* typedef struct {
	*   RECT pos;     // position of MFD in panel (pixel)
	*   DWORD nmesh;  // mesh index (>=0)
	*   DWORD ngroup; // mesh group index (>=0)
	*   DWORD flag;   // parameter flags (see below)
	*   int nbt1;     // number of buttons in array 1 (e.g. left side of MFD display)
	*   int nbt2;     // number of buttons in array 2 (e.g. right side of MFD display)
	*   int bt_yofs;  // y-offset of top button from top display edge (pixel)
	*   int bt_ydist; // y-distance between buttons (pixel)
	* } MFDSPEC; \endcode
	* \note \a flag is a bitmask which can be set to a combination of the following options:\n
	* - \c MFD_SHOWMODELABELS Show 3-letter abbreviations for MFD modes when displaying the
	* mode selection page (default: only show carets ">"). This is useful
	* if the buttons are not located next to the list display.
	* \note If this function is used during initialisation of a 2-D instrument panel, pos
	*  defines the rectangle of the MFD display in the panel bitmap (in pixels), while
	*  nmesh and ngroup are ignored.\n
	*  If it is used during initialisation of a virtual cockpit, nmesh and ngroup define
	*  the mesh and group index of the mesh element which will receive the MFD
	*  display texture, while pos is ignored.
	* \sa \ref mfdidentifier "MFD Identifiers"
	*/
OAPIFUNC void oapiRegisterMFD (int mfd, const EXTMFDSPEC *spec);

OAPIFUNC void oapiRegisterExternMFD (ExternMFD *emfd, const MFDSPEC &spec);

OAPIFUNC bool oapiUnregisterExternMFD (ExternMFD *emfd);

	/**
	* \brief Register the background bitmap for a custom panel.
	* \param hBmp bitmap handle
	* \param flag property bit flags (see notes)
	* \param ck transparency colour key
	* \note This function will normally be called in the body of ovcLoadPanel.
	* \note Typically the bitmap will be stored as a resource in the DLL and obtained by
	*  a call to the Windows function LoadBitmap(...).
	* \note \a flag defines panel properties and can be a combination of the following bitmasks:
	*   - \c PANEL_ATTACH_{LEFT/RIGHT/TOP/BOTTOM}
	*   - \c PANEL_MOVOUT_{LEFT/RIGHT/TOP/BOTTOM}
	* \note where \c PANEL_ATTACH_BOTTOM means that the bottom edge of the panel
	*   cannot be scrolled above the bottom edge of the screen (other directions
	*   work equivalently) and \c PANEL_MOVEOUT_BOTTOM means that the panel
	*   can be scrolled downwards out of the screen (other directions work equivalently)
	* \note The colour key, if defined, specifies a colour which will appear transparent
	*   when displaying the panel. The key is in (hex) 0xRRGGBB format. If no key
	*   is specified, the panel will be opaque. It is best to use black (0x000000) or
	*   white (0xffffff) as colour keys, since other values may cause problems in
	*   16bit screen modes. Of course, care must be taken that the keyed colour
	*   does not appear anywhere in the opaque part of the panel.
	* \sa oapiRegisterPanelArea
	*/
OAPIFUNC void oapiRegisterPanelBackground (HBITMAP hBmp, DWORD flag = PANEL_ATTACH_BOTTOM|PANEL_MOVEOUT_BOTTOM,
				                           DWORD ck = (DWORD)-1);

	/**
	* \anchor register_p_a
	* \brief Defines a rectangular area within a panel to receive mouse or redraw notifications.
	* \param id area identifier
	* \param pos bounding box of the marked area
	* \param draw_event defines redraw events
	* \param mouse_event defines mouse events
	* \param bkmode redraw background mode
	* \note Each panel area must be defined with an identifier aid which is unique within the panel.
	* \note draw_event can have the following values: \n\n
	* - \c PANEL_REDRAW_NEVER: do not generate redraw events. \n
	* - \c PANEL_REDRAW_ALWAYS: generate a redraw event at every time step. \n
	* - \c PANEL_REDRAW_MOUSE: mouse events trigger redraw events.\n\n
	*
	* \note For possible values of mouse_event see \ref panel_mouse "Mouse event identifiers".
	*  \c PANEL_MOUSE_IGNORE prevents mouse events from being triggered.
	* \note By default, no mouse events are sent during a playback session. You can
	*  force Orbiter to trigger mouse events during a playback (e.g. to allow the
	*  user to operate MFD buttons) by using \c PANEL_MOUSE_ONREPLAY in
	*  combination with any of the other mouse event flags.\n
	* \note bkmode defines the bitmap handed to the redraw callback:\n\n
	* - \c PANEL_MAP_NONE: provides an undefined bitmap. Should be used if the whole area is repainted.\n
	* - \c PANEL_MAP_CURRENT: provides a copy of the current area.\n
	* - \c PANEL_MAP_BACKGROUND: provides a copy of the panel background (as defined by oapiRegisterPanelBackground()).\n
	* - \c PANEL_MAP_BGONREQUEST: like \c PANEL_MAP_BACKGROUND, this
	*   stores the area background, but the user must request it explicitly with a call
	*   to oapiBltPanelAreaBackground. This can improve performance if the area
	*   does not need to be updated at each call of the repaint callback function.
	* \sa \ref panel_mouse "Mouse event identifiers", \ref panel_redraw "Panel redraw events",
	*  oapiRegisterPanelBackground
	*/
OAPIFUNC void oapiRegisterPanelArea (int id, const RECT &pos, int draw_event = PANEL_REDRAW_NEVER,
					    int mouse_event = PANEL_MOUSE_IGNORE, int bkmode = PANEL_MAP_NONE);

	/**
	* \brief Defines the neighbour panels of the current panels. These are the panels the user can
	*  switch to via Ctrl-Arrow keys.
	* \param left panel id of left neighbour (or -1 if none)
	* \param right panel id of right neighbour (or -1 if none)
	* \param top panel id of top neighbour (or -1 if none)
	* \param bottom panel id of bottom neighbour (or -1 if none)
	* \note This function should be called during panel registration (in
	*  VESSEL2::clbkLoadPanel()) to define the neighbours of the registered panel.
	* \note Every panel (except panel 0) must be listed as a neighbour by at least one
	*  other panel, otherwise it is inaccessible.
	*/
OAPIFUNC void oapiSetPanelNeighbours (int left, int right, int top, int bottom);

	/**
	* \brief Copies the stored background of a panel area into the provided surface.
	* \details This function should only be called from within the repaint callback 
	*  function of an area registered with the \c PANEL_MAP_BGONREQUEST flag.
	* \param area_id area identifier
	* \param surf surface handle
	* \note Areas defined with the \c PANEL_MAP_BGONREQUEST receive a surface
	*  with undefined contents when their repaint callback is called. They can use
	*  oapiBltPanelAreaBackground to copy the area background into the surface.
	* \note For areas not registered with the \c PANEL_MAP_BGONREQUEST, this
	*  function will do nothing.
	* \note Using \c PANEL_MAP_BGONREQUEST is more efficient than
	*  \c PANEL_MAP_BACKGROUND if the area doesn't need to be repainted at
	*  each call of the callback function, because it delays blitting the background
	*  until the module requests the background. This is particularly significant for
	*  areas which are updated at each time step.
	* \sa oapiRegisterPanelArea, oapiRegisterPanelBackground
	*/
OAPIFUNC bool oapiBltPanelAreaBackground (int area_id, SURFHANDLE surf);

	/**
	* \brief Defines how the navigation mode buttons will be displayed in a default cockpit view.
	* \param mode display mode (0 .. 2)
	* \note This function should usually be called in the body of the overloaded
	*  VESSEL2::clbkLoadGenericCockpit().
	* \note It defines if the buttons for navigation modes (e.g. "Killrot" or "Prograde") are
	*  displayed in the generic (non-panel) cockpit camera mode, and if the buttons
	*  can be operated with the mouse.
	* \note The following values for mode are defined:\n
	* - 0 buttons are not shown\n
	* - 1 buttons are shown and can be operated with the mouse (default)\n
	* - 2 only buttons representing active modes are shown, and can not be operated with the mouse
	*/
OAPIFUNC void oapiSetDefNavDisplay (int mode);

	/**
	* \brief Enable or disable the display of the reaction control system indicators/controls in
	*  default cockpit view.
	* \param mode display mode (0 .. 1)
	* \note This function should usually be called in the body of the overloaded
	*  VESSEL2::clbkLoadGenericCockpit().
	* \note The RCS display consists of three buttons in the engine status display at the
	*  top left of the generic cockpit view. If displayed (mode=1), the buttons show
	*  the RCS mode (off/rotational/linear), and can be clicked with the mouse to switch modes.
	* \note The following values for mode are defined: \n
	* - 0 RCS buttons are not shown
	* - 1 RCS buttons are shown and can be operated with the mouse (default)
	*/
OAPIFUNC void oapiSetDefRCSDisplay (int mode);
	
	/**
	* \brief Switch to a neighbour instrument panel in 2-D panel cockpit mode.
	* \param direction neighbour direction (see notes)
 	* \return Identifier of the newly selected panel (>=0) or -1 if the requested panel does not exist.
	* \note direction can be one of the constants in \ref panelneighbour.
	* \note The neighbourhood status between panels is established by the
	*  oapiSetPanelNeighbours() function.
	* \note This function has no effect if the current view is not in 2-D panel cockpit mode.
	* \sa oapiSetPanel
	*/
OAPIFUNC int oapiSwitchPanel (int direction);

	/**
	* \brief Switch to a different instrument panel in 2-D panel cockpit mode.
	* \param panel_id panel identifier (>=0)
	* \return panel_id if the panel was set successfully, or -1 if failed (camera not in 2-D panel
	*  cockpit mode, or requested panel does not exist for the current vessel)
	* \note This function has no effect if the current view is not in 2-D panel cockpit mode.
	* \sa oapiSwitchPanel
	*/
OAPIFUNC int oapiSetPanel (int panel_id);

/**
 * \brief Returns the scaling factor for 2-D instrument panels.
 * \return Panel scaling factor (>0)
 * \note This function returns the panel scaling factor defined by the user
 *   in the Launchpad dialog.
 * \note The default scaling factor is 1. Values > 1 cause panels to be
 *   expanded, values < 1 cause panels to be shrunk.
 * \note The value returned by this function is only used by legacy panels
 *   defined in VESSEL2::clbkLoadPanel. Panels created with VESSEL3::clbkLoadPanel2D
 *   should use oapiGetPanel2DScale instead.
 * \sa oapigetPanel2DScale
 */
OAPIFUNC double oapiGetPanelScale();

/**
 * \brief Returns the current scaling factor for the active 2D instrument
 *   panel.
 * \return Current panel scaling factor.
 * \note The value returned by this function describes the scaling between the
 *   coordinates of the panel mesh and the screen pixels. They correspond to the
 *   values passed to VESSEL3::SetPanelScaling.
 * \note A scaling value of 1 means every mesh unit corresponds to the size of a
 *   pixel. Smaller values indicate a scaled-down mesh, larger values a scaled-up mesh.
 * \note If the current camera view is not in Panel2D mode, the return value is 1.
 * \note The value returned by this function is only used by instrument panels which
 *   have been created with VESSEL3::clbkLoadPanel2D. Legacy panels created with
 *   VESSEL2::clbkLoadPanel should use oapiGetPanelScale instead.
 * \sa VESSEL3::SetPanelScaling, VESSEL3::clbkLoadPanel2D
 */
OAPIFUNC double oapiGetPanel2DScale();

OAPIFUNC void oapiSetPanelBlink (VECTOR3 v[4]);
//@}


// =============================================================================================
/// \defgroup DrawSupport Drawing support functions
// =============================================================================================
//@{
/**
 * \brief Obtain a drawing context for a surface.
 * \param surf surface handle
 * \return drawing context instance, or NULL if no graphics support
 * \note This function returns a valid context instance only when Orbiter
 *   is attached to a graphics client which supports 2-D drawing into
 *   surfaces. The caller should check the return value for NULL.
 * \note If a nonzero Sketchpad instance was returned, it should be
 *   released with \ref oapiReleaseSketchpad after drawing.
 * \note Most graphics clients must lock the surface data buffer (and copy it
 *   to main memory, if necessary) before drawing access can be provided. This
 *   means that read/write access to the surface (e.g. for blitting) may be
 *   disabled between oapiGetSketchpad and oapiReleaseSketchpad, and should
 *   be avoided.
 * \sa oapiReleaseSketchpad
 */
OAPIFUNC oapi::Sketchpad *oapiGetSketchpad (SURFHANDLE surf);

/**
 * \brief Release a drawing device context instance.
 * \param skp drawing context instance
 * \note Use this function to release a device instance previously
 *   acquired with oapiGetSketchpad.
 * \sa oapiGetSketchpad
 */
OAPIFUNC void oapiReleaseSketchpad (oapi::Sketchpad *skp);

enum FontStyle {
	FONT_NORMAL = 0x0,
	FONT_BOLD = 0x1,
	FONT_ITALIC = 0x2,
	FONT_UNDERLINE = 0x4,
	FONT_STRIKEOUT = 0x8,		
	FONT_CRISP = 0x10,			///< Override app-default, No Antialiasing
	FONT_ANTIALIAS = 0x20		///< Override app-default, Use Antialiashing
};

/**
 * \brief Creates a font resource for drawing text into surfaces.
 * \param height font height [pixel]
 * \param prop flag for proportional/fixed pitch font
 * \param face typeface name (see notes)
 * \param style font decoration style (see notes)
 * \return pointer to font resource, or NULL if not supported.
 * \note The following generic typeface names should be understood
 *   by all graphics systems:
 *   - Fixed (fixed pitch font)
 *   - Sans (sans-serif proportional font)
 *   - Serif (serif proportional font)
 *   Other font names may not be recognised by all graphics clients.
 *   In that case, the default fixed or sans-serif font will be used,
 *   depending on the value of \e prop.
 * \note The decoration style flags allow bold, italic and underlining.
 * \note After use, the font should be deallocated with oapiReleaseFont.
 * \sa oapiReleaseFont
 */
OAPIFUNC oapi::Font *oapiCreateFont (int height, bool prop, char *face, FontStyle style = FONT_NORMAL);

/**
 * \brief Creates a font resource for drawing text into surfaces.
 * \param height font height [pixel]
 * \param prop flag for proportional/fixed pitch font
 * \param face typeface name (see notes)
 * \param style font decoration style (see notes)
 * \param orientation text orientation [1/10 deg]
 * \return pointer to font resource, or NULL if not supported.
 * \note Identical to oapiCreateFont(int,bool,char*,FontStyle), but
 *   contains the additional orientation parameter.
 */
OAPIFUNC oapi::Font *oapiCreateFont (int height, bool prop, const char *face, FontStyle style, int orientation);

/**
* \brief Creates a font resource for drawing text into surfaces.
* \param height Font height in pixels
* \param face Name of the font
* \param width Width of the font (0 for default aspect ration)
* \param weight Font thikness (400 for default weight)
* \param style Ccombination of \see FontStyle flags (0 for default)
* \param spacing Spacing between charters in a string (0.0f for default)
* \return A pointer to a created or pre-existing font or NULL in a case of an error.
*/
OAPIFUNC oapi::Font* oapiCreateFontEx(int height, char* face, int width = 0, int weight = 400, FontStyle style = FontStyle::FONT_NORMAL, float spacing = 0.0f);

/**
 * \brief Release a font resource.
 * \param font pointer to font resource
 * \sa oapiCreateFont
 */
OAPIFUNC void oapiReleaseFont (oapi::Font *font);

/**
 * \brief Creates a pen resource for drawing lines and shape outlines.
 * \param style line style (0=invisible, 1=solid, 2=dashed)
 * \param width line width [pixel]
 * \param col line colour (format: 0xBBGGRR)
 * \note After use, the pen should be deallocated with oapiReleasePen.
 * \sa oapiReleasePen
 */
OAPIFUNC oapi::Pen *oapiCreatePen (int style, int width, DWORD col);

/**
 * \brief Release a pen resource.
 * \param pen pointer to pen resource
 * \sa oapiCreatePen
 */
OAPIFUNC void oapiReleasePen (oapi::Pen *pen);

/**
 * \brief Creates a brush resource for filling shapes.
 * \param col shape fill colour (format: 0xBBGGRR)
 * \note After use, the brush should be deallocated with oapiReleaseBrush.
 * \sa oapiReleaseBrush
 */
OAPIFUNC oapi::Brush *oapiCreateBrush (DWORD col);

/**
 * \brief Release a brush resource.
 * \param brush pointer to brush resource
 * \sa oapiCreateBrush
 */
OAPIFUNC void oapiReleaseBrush (oapi::Brush *brush);

/**
 * \brief Obtain a Windows device context handle (HDC) for a surface.
 * \param surf surface handle
 * \return device context handle, or NULL if not supported.
 * \warning This function uses a device-dependent drawing context handle
 *   and may not work with all graphics clients. It has been superseded by
 *   oapiGetSketchpad.
 * \note This function returns a valid device handle only when Orbiter is
 *   using its inline graphics client, or if an external client is attached
 *   that supports GDI drawing. In all other cases, the function returns
 *   NULL. Therefore, the caller should always check the returned value before
 *   using it.
 * \note If a nonzero HDC was returned, it should be released with
 *   \ref oapiReleaseDC after drawing.
 * \note Most graphics clients must lock the surface data buffer (and copy it
 *   to main memory, if necessary) before GDI access can be provided. This
 *   means that read/write access to the surface (e.g. for blitting) may be
 *   disabled between oapiGetDC and oapiReleaseDC, and should be avoided.
 * \sa oapiReleaseDC, oapiGetSketchpad
 */
OAPIFUNC HDC oapiGetDC (SURFHANDLE surf);

/**
 * \brief Release a GDI drawing device context handle.
 * \param surf surface handle
 * \param hDC device context handle
 * \warning This function uses a device-dependent drawing context handle
 *   and may not work with all graphics clients. It has been superseded by
 *   oapiReleaseSketchpad.
 * \note Use this function to release a device context previously acquired
 *   with \ref oapiGetDC.
 * \note Standard Windows device context rules apply. For example, any custom
 *   device objects loaded via SelectObject must be unloaded before calling
 *   oapiReleaseDC.
 * \sa oapiGetDC, oapiGetSketchpad, oapiReleaseSketchpad
 */
OAPIFUNC void oapiReleaseDC (SURFHANDLE surf, HDC hDC);
//@}


// =============================================================================================
/// \defgroup Surface Surface functions
// =============================================================================================
//@{
	/**
	* \brief Create a surface of the specified dimensions.
	* \deprecated This function has been superseded by oapiCreateSurfaceEx
	* \param width width of surface bitmap (pixels)
	* \param height height of surface bitmap (pixels)
	* \return Handle to the new surface.
	* \note The bitmap contents are undefined after creation, so the surface must be
	*  repainted fully before mapping it to the screen.
	* \note The surface is created with the OAPISURFACE_RENDERTARGET, OAPISURFACE_GDI and
	*   OAPISURFACE_SKETCHPAD attributes (see \ref surfacecaps). For more control over
	*   the surface attributes, e.g. if you want to use the surface as a texture, use
	*   oapiCreateSurfaceEx instead.
	* \note Surfaces should be destroyed by calling oapiDestroySurface when they are no longer needed.
	* \sa oapiCreateSurfaceEx, oapiDestroySurface
	*/
OAPIFUNC SURFHANDLE oapiCreateSurface (int width, int height);

	/**
	* \brief Create a surface of the specified dimensions and usage/access attributes.
	* \param width width of surface bitmap (pixels)
	* \param height height of surface bitmap (pixels)
	* \param attrib surface creation attributes (see \ref surfacecaps)
	* \return Handle to the new surface.
	* \note The surface contents are undefined after creation, so the surface must be
	*  repainted fully before mapping it to the screen.
	* \note Surfaces should be destroyed by calling oapiDestroySurface when they are no longer needed.
	* \note If you create a texture surface (by including the OAPISURFACE_TEXTURE attribute),
	*   the width and height should be multiples of 2, and not greater than 2048 x 2048 pixels
	*   for best hardware compatibility.
	* \sa oapiDestroySurface
	*/
OAPIFUNC SURFHANDLE oapiCreateSurfaceEx (int width, int height, DWORD attrib);

	/**
	* \brief Load a texture from a file (*.dds, *.png, *.jpg, *.bmp)
	* \param fname texture file name
	* \param attrib OAPISURFACE_* attribute flags defining surface type.
	* \param bPath if 'true' then 'fname' must contain absolute path to a file. If 'false' a normal Orbiter texture search path is used.
	* \return Surface handle for the loaded texture, or NULL if not found.
	*/
OAPIFUNC SURFHANDLE oapiLoadSurfaceEx(const char* fname, DWORD attrib, bool bPath = false);

	/**
	* \brief Save the contents of a surface to a formatted image file or to the clipboard
	* \param hSrf surface handle (0 for primary render surface)
	* \param fname image file path relative to orbiter root directory (excluding file extension), or NULL to save to clipboard
	* \param fmt output file format
	* \param quality quality request if the format supports it (0-1)
	* \return Should return true on success
	* \default Nothing, returns false
	*/
OAPIFUNC bool oapiSaveSurface(const char* fname, SURFHANDLE hSrf, oapi::ImageFileFormat fmt, float quality = 0.7f);


	/**
	* \brief Create a surface from a bitmap. Bitmap surfaces are typically used for blitting
	*  operations during instrument panel redraws.
	* \param hBmp bitmap handle
	* \param release_bmp flag for bitmap release
	* \return Handle to the new surface.
	* \note The easiest way to access bitmaps is by storing them as resources in the
	*  module, and loading them via a call to LoadBitmap.
	* \note Do not use this function with a bitmap generated by CreateBitmap. To create
	*  a surface of specified dimensions, use oapiCreateSurface (width, height) instead.
	* \note If \e release_bmp==true, then oapiCreateSurface() will destroy the bitmap after
	*  creating a surface from it (i.e. the hBmp handle will be invalid after the
	*  function returns), otherwise the module is responsible for destroying the
	*  bitmap by a call to DestroyObject when it is no longer needed.
	* \note Surfaces should be destroyed by calling oapiDestroySurface when they are
	*  no longer needed.
	* \sa oapiDestroySurface
	*/
OAPIFUNC SURFHANDLE oapiCreateSurface (HBITMAP hBmp, bool release_bmp = true);

	/**
	* \brief Create a surface that can be used as a texture for a 3-D object.
	* \deprecated This function has been superseded by oapiCreateSurfaceEx
	* \param width width of surface bitmap (pixels)
	* \param height height of surface bitmap (pixels)
	* \return handle of new texture surface
	* \note Use this function instead of oapiCreateSurface if you want the surface to be
	*  used as a surface texture for a 3-D object, for example via a call to oapiSetTexture.
	* \note This is equivalent to calling oapiCreateSurfaceEx with the
	*  OAPISURFACE_TEXTURE attribute.
	* \note For maximum compatibility, the surface should be square, and dimensions
	*  powers of 2, for example 64x64, 128x128, 256x256, etc. Note that older
	*  video cards may not support textures larger than 256x256.
	* \note Surfaces should be destroyed by calling oapiDestroySurface when they are no longer needed.
	*/
OAPIFUNC SURFHANDLE oapiCreateTextureSurface (int width, int height);

	/**
	* \brief Get surface size information
	* \param hSrf handle to a surface
	* \param width [out] width of surface bitmap (pixels)
	* \param height [out] height of surface bitmap (pixels)
	* \return false if an error occured, true otherwise
	*/
OAPIFUNC bool oapiGetSurfaceSize(SURFHANDLE hSrf, int* width, int* height);


	/**
	* \brief Destroy a surface previously created with oapiCreateSurface.
	* \param surf surface handle
	*/
OAPIFUNC void       oapiDestroySurface (SURFHANDLE surf);

OAPIFUNC void       oapiClearSurface (SURFHANDLE surf, DWORD col = 0);

	/**
	* \brief Define a colour key for a surface to allow transparent blitting.
	* \param surf surface handle
	* \param ck colour key (0xRRGGBB)
	* \note Defining a colour key and subsequently calling oapiBlt with the
	*  \c SURF_PREDEF_CK flag is slightly more efficient than passing the colour
	* key explicitly to oapiBlt each time, if the same colour key is used repeatedly.
	* \sa oapiClearSurfaceColourKey, oapiBlt
	*/
OAPIFUNC void       oapiSetSurfaceColourKey (SURFHANDLE surf, DWORD ck);

	/**
	* \brief Clear a previously defined colour key.
	* \param surf surface handle
	* \sa oapiSetSurfaceColourKey, oapiBlt
	*/
OAPIFUNC void       oapiClearSurfaceColourKey (SURFHANDLE surf);

	/**
	* \brief Copy a rectangular area from one surface to another.
	* \param tgt target surface
	* \param src source surface
	* \param tgtx left edge of target rectangle [pixel]
	* \param tgty top edge of target rectangle [pixel]
	* \param srcx left edge of source rectangle [pixel]
	* \param srcy top edge of source rectangle [pixel]
	* \param w width of copied rectangle [pixel]
	* \param h height of copied rectangle [pixel]
	* \param ck transparency colour key (inline graphics only)
	* \note This function copies rectangular areas between two surfaces, or between two
	*   locations of the same surface.
	* \note A typical use is the dynamic update of instrument panels, e.g. in the
	*   body of \ref VESSEL2::clbkPanelRedrawEvent.
	* \note This function must not be used while a device context is acquired for the
	*   target surface (i.e. between \ref oapiGetDC and \ref oapiReleaseDC calls). If a
	*   blitting operation is necessary between oapiGetDC and oapiReleaseDC, you must use
	*   the standard Windows BitBlt function. However this does not use hardware
	*   acceleration and should therefore be avoided.
	* \note Transparent blitting can be performed by specifying a colour key in ck. The
	*   transparent colour can either be passed explicitly in ck, or ck can be set to
	*   SURF_PREDEF_CK to use the key previously defined with \ref oapiSetSurfaceColourKey.
	* \note Colour keys are only supported with Orbiter's inline graphics client. External
	*   clients ignore the ck parameter. The use of colour keys is therefore discouraged.
	*/
OAPIFUNC void oapiBlt (SURFHANDLE tgt, SURFHANDLE src, int tgtx, int tgty, int srcx, int srcy, int w, int h, DWORD ck = SURF_NO_CK);

	/**
	* \brief Copy a scaled rectangular area from one surface to another.
	* \param tgt target surface
	* \param src source surface
	* \param tgtr pointer to target rectangle [pixel]
	* \param srcr pointer to source rectangle [pixel]
	* \param ck transparency colour key (inline graphics only)
	* \param rotate rotation flag (deprecated)
	* \note This function copies a rectangluar area from a source to a target surface.
	* \note If the sizes of the source and target rectangles differ, the copied area
	*   is stretched or shrunk to fit into the target rectangle.
	* \note This function must not be used while a device context is acquired for the
	*   target surface (i.e. between \ref oapiGetDC and \ref oapiReleaseDC calls).
	* \note Transparent blitting can be performed by specifying a colour key in ck. The
	*   transparent colour can either be passed explicitly in ck, or ck can be set to
	*   SURF_PREDEF_CK to use the key previously defined with \ref oapiSetSurfaceColourKey.
	* \note Colour keys are only supported with Orbiter's inline graphics client. External
	*   clients ignore the ck parameter. The use of colour keys is therefore discouraged.
	* \note The rotation flag is deprecated. It has no effect.
	*/
OAPIFUNC void oapiBlt (SURFHANDLE tgt, SURFHANDLE src, RECT *tgtr, RECT *srcr, DWORD ck = SURF_NO_CK, DWORD rotate = SURF_NO_ROTATION);

	/**
	 * \brief Begin a block of blitting operations to the same target surface.
	 * \param tgt Target surface for subsequent blitting calls.
	 * \return Error code:
	 *   - 0: success
	 *   - -1: no graphics client loaded
	 *   - -1: blitting target was set already
	 *   - -2: tgt was given as RENDERTGT_NONE
	 *   - other error codes are client-specific
	 * \note All blitting calls following this function must address the same
	 *   target until the blitting block is ended with a call to oapiEndBltGroup.
	 * \note This mechanism should always be used when multiple blitting operations
	 *   go to the same target, because some graphics clients may be able to
	 *   optimise the calls.
	 * \note To refer to the main render surface, use tgt=RENDERTGT_MAINWINDOW.
	 * \note Do not call a blitting function with a target other than \a tgt within
	 *   a blitting group.
	 * \note Within the blitting group, multiple source surfaces can be used, but
	 *   if possible the blitting calls should be arranged so that blits from the
	 *   same surface a grouped together, to allow additional optimisation.
	 * \sa oapiEndBltGroup
	 */
OAPIFUNC int oapiBeginBltGroup (SURFHANDLE tgt);

	/**
	 * \brief End a block of blitting operations to the same target surface.
	 * \return Error code:
	 *   - 0; success
	 *   - -1: no graphics client loaded
	 *   - -2: no blitting target was set
	 *   - other error codes are client-specific
	 * \sa oapiBeginBltGroup
	 */
OAPIFUNC int oapiEndBltGroup ();

	/**
	* \brief Fill an area of the target surface with a uniform colour.
	* \param tgt target surface
	* \param fillcolor fill colour
	* \param tgtx coordinate of upper left corner of area to fill.
	* \param tgty coordinate of upper left corner of area to fill.
	* \param w width of area to fill.
	* \param h height of area to fill.
	* \note The fill colour should be acquired with oapiGetColour(), to ensure
	*  compatibility with 16-bit colour modes.
	* \note This function must not be used while a device context is acquired for the
	*  target surface (i.e. between oapiGetDC() and oapiReleaseDC() calls).
	* \note If w and h are zero (the default) the whole surface is filled. The tgtx and tgty
	*  values are ignored in that case and can be omitted.
	*/
OAPIFUNC void oapiColourFill (SURFHANDLE tgt, DWORD fillcolor, int tgtx = 0, int tgty = 0, int w = 0, int h = 0);
//@}


// =============================================================================================
/// \defgroup CustomMFD Custom MFD mode definition
// =============================================================================================
//@{ 
	/**
	* \brief Register a custom MFD mode.
	* \param spec MFD specs (see notes below)
	* \return MFD mode identifier
	* \note This function registers a custom MFD mode with Orbiter. There are two
	*  types of custom MFDs: generic and vessel class-specific. Generic MFD
	*  modes are available to all vessel types, while specific modes are only
	*  available for a single vessel class. Generic modes should be registered in
	*  the \ref InitModule callback function of a plugin module. Vessel class specific
	*  modes should be registered via VESSEL4::RegisterMFDMode.
	* \note MFDMODESPECEX is a struct defining the parameters of the new mode:
	* \code
	* typedef struct {
	*   char *name;    // points to the name of the new mode
	*   DWORD key;     // mode selection key
	*   void *context; // mode-specific context pointer
	*   int (*msgproc)(UINT,UINT,WPARAM,LPARAM);   // address of MFD message parser
	* } MFDMODESPEC; \endcode
	* \note See orbitersdk\\samples\\CustomMFD for a sample MFD mode implementation.
	* \sa oapiUnregisterMFDMode, VESSEL4::RegisterMFDMode
	*/
OAPIFUNC int oapiRegisterMFDMode (MFDMODESPECEX &spec);

	/**
	* \brief Unregister a previously registered custom MFD mode.
	* \param mode mode identifier, as returned by \ref oapiRegisterMFDMode
	* \return \e true on success (mode could be unregistered).
	*/
OAPIFUNC bool oapiUnregisterMFDMode (int mode);

	/**
	* \brief Disable an MFD mode.
	* \param mode MFD mode to be disabled.
	* \note The list of disabled MFDs is cleared whenever the focus switches to a new
	*  vessel. To disable MFD modes permanently for a particular vessel type,
	*  oapiDisableMFDMode() should be called from within the ovcFocusChanged() callback function.
	* \note For builtin MFD modes, mode can be any of the MFD_xxx constants. For
	*  MFD modes defined in plugin modules, the mode id must be obtained by a
	*  call to oapiGetMFDModeSpec().
	* \sa \ref mfdmode "MFD Modes"
	*/
OAPIFUNC void oapiDisableMFDMode (int mode);

	/**
	* \brief Returns the mode identifier and spec for an MFD mode defined by its name.
	* \param name MFD name (as defined in MFDMODESPECEX::name during oapiRegisterMFDMode())
	* \param spec If defined, this will return a pointer to the MFDMODESPECEX structure for the mode.
	* \return MFD mode identifier.
	* \note This function returns the same value as oapiRegisterMFDMode() for the given mode.
	* \note If no matching mode is found, the return value is MFD_NONE. In that case,
	*  the returned spec pointer is undefined.
	* \note The mode identifiers for custom MFD modes can not be assumed to persist
	*  across simulation runs, since they will change if the user loads or unloads MFD plugins.
	* \note This function can also be used for built-in MFD modes, which are defined as follows:
	*	<table>
	*   <tr><td><b>Name string</b></td><td><b>Mode identifier</b></td></tr>
	*   <tr><td>Orbit</td><td>MFD_ORBIT</td></tr>
	*   <tr><td>Surface</td><td>MFD_SURFACE</td></tr>
	*   <tr><td>Map</td><td>MFD_MAP</td></tr>
	*   <tr><td>HSI</td><td>MFD_HSI</td></tr>
	*   <tr><td>VOR/VTOL</td><td>MFD_LANDING</td></tr>
	*   <tr><td>Docking</td><td>MFD_DOCKING</td></tr>
	*   <tr><td>Align Planes</td><td>MFD_OPLANEALIGN</td></tr>
	*   <tr><td>Sync Orbit</td><td>MFD_OSYNC</td></tr>
	*   <tr><td>Transfer</td><td>MFD_TRANSFER</td></tr>
	*   <tr><td>COM/NAV</td><td>MFD_COMMS</td></tr>
	*   </table>
	*/
OAPIFUNC int oapiGetMFDModeSpecEx (char *name, MFDMODESPECEX **spec = 0);
//@}


// =============================================================================================
/// \defgroup VirtualCockpit Virtual cockpit functions
// =============================================================================================
//@{ 
	/**
	* \brief Define a render target for rendering an MFD display in a virtual cockpit.
	* \param mfd MFD identifier (e.g. \c MFD_LEFT, \c MFD_RIGHT)
	* \param spec render target specification (see notes)
	* \note The render target specification is defined as a structure:\n
	* \code struct VCMFDSPEC { DWORD nmesh, ngroup }; \endcode
	*  where nmesh is the mesh index (>=0), and ngroup is the group index (>=0) defining the render target.
	* \note This function should be placed in the body of the ovcLoadVC vessel module callback function.
	* \note The addressed mesh group should define a simple square (4 vertices, 2
	*  triangles). The group materials and textures can be set to 0.
	* \sa \ref mfdidentifier "MFD Identifiers"
	*/
OAPIFUNC void       oapiVCRegisterMFD (int mfd, const VCMFDSPEC *spec);

	/**
	* \brief Define an active area in a virtual cockpit. Active areas can be repainted. This function is
	*  similar to oapiRegisterPanelArea.
	* \param id area identifier
	* \param tgtrect bounding box of the active area in the target texture (pixels)
	* \param draw_event redraw condition (see \ref register_p_a "draw events")
	* \param mouse_event mouse event ( see \ref panel_mouse "mouse events")
	* \param bkmode background mode (see \ref register_p_a "bkmodes")
	* \param tgt target texture to be updated
	* \note The target texture can be retrieved from a mesh by using the
	*  oapiGetTextureHandle() method. Dynamic textures must be marked with flag "D" in the mesh file.
	* \note Redraw events can be used not only to update mesh textures dynamically,
	*  but also to animate mesh groups, or edit mesh vertices or texture coordinates.
	* \note If no dynamic texture repaints are required during redraw events, use the
	*  alternative version of oapiVCRegisterArea() instead.
	* \note To define a mouse-sensitive volume in the virtual cockpit, use one of the
	*  \a oapiVCSetAreaClickmode_XXX functions.
	*/
OAPIFUNC void oapiVCRegisterArea (int id, const RECT &tgtrect, int draw_event, int mouse_event, int bkmode, SURFHANDLE tgt);

	/**
	* \brief Define an active area in a virtual cockpit. This version is used when no dynamic texture
	*  update is required during redraw events.
	* \param id area identifier
	* \param draw_event redraw condition (see \ref register_p_a "draw events")
	* \param mouse_event mouse event (see \ref panel_mouse "mouse events")
	* \note This function is equivalent to:
	* \code oapiVCRegisterArea (aid, _R(0,0,0,0), draw_event,mouse_event, PANEL_MAP_NONE, NULL); \endcode
	*/
OAPIFUNC void oapiVCRegisterArea (int id, int draw_event, int mouse_event);

	/**
	* \brief Associate a spherical region in the virtual cockpit with a registered area to receive mouse events.
	* \param id area identifier (as specified during area registration)
	* \param cnt centre of active area in the local vessel frame
	* \param rad radius of active area [m]
	* \note The area identifier must refer to an area which has previously been
	*  registered with a call to oapiVCRegisterArea(), with the required mouse event modes.
	* \note This function can be called repeatedly, to change the mouse-sensitive area.
	* \sa VESSEL2::clbkVCMouseEvent
	*/
OAPIFUNC void       oapiVCSetAreaClickmode_Spherical (int id, const VECTOR3 &cnt, double rad);

	/**
	* \brief Associate a quadrilateral region in the virtual cockpit with a registered area to receive mouse events.
	* \param id area identifier (as specified during area registration)
	* \param p1 top left corner of region
	* \param p2 top right corner
	* \param p3 bottom left corner
	* \param p4 bottom right corner
	* \note This function will trigger mouse events when the user clicks within the
	*  projection of the quadrilateral region on the render window. The mouse
	*  event handler will receive the relative position within the area at which the
	*  mouse event occurred, where the top left corner has coordinates (0,0), and
	*  the bottom right corner has coordinates (1,1). 
	* \note The area can define any flat quadrilateral in space. It is not limited to
	*  rectangles, but all 4 points should be in the same plane.
	* \sa VESSEL2::clbkVCMouseEvent
	*/
OAPIFUNC void       oapiVCSetAreaClickmode_Quadrilateral (int id, const VECTOR3 &p1, const VECTOR3 &p2, const VECTOR3 &p3, const VECTOR3 &p4);

	/**
	* \brief Defines the neighbouring virtual cockpit camera positions in relation to the current
	*  position. The user can switch to neighbour positions with Ctrl-Arrow keys.
	* \param left panel id of left neighbour position (or -1 if none)
	* \param right panel id of right neighbour position (or -1 if none)
	* \param top panel id of top neighbour position (or -1 if none)
	* \param bottom panel id of bottom neighbour position (or -1 if none)
	* \note This function should be called during virtual cockpit registration (in VESSEL2::clbkLoadVC())
	*  to define the neighbouring cockpit camera positions, if any.
	* \note The left, right, top and bottom values specify the (zero-based) identifiers of
	*  the VC positions to switch to when the user presses Ctrl and an arrow
	*  button, or -1 if no position is available in this direction.
	* \note The neighbour relations should normally be reciprocal, i.e. if position 0
	*  defines position 1 as its right neighbour, then position 1 should define
	*  position 0 as its left neighbour.
	* \note If only a single VC position (id 0) is defined, this function doesn't need to be called.
	* \note Orbiter calls VESSEL2::clbkLoadVC() with the appropriate id whenever the user switches to a new position.
	*/
OAPIFUNC void       oapiVCSetNeighbours (int left, int right, int top, int bottom);

	/**
	* \brief Triggers a redraw notification for a virtual cockpit area.
	* \param vc_id virtual cockpit identifier
	* \param area_id area identifier (as specified during area registration)
	* \note This function triggers a call to the VESSEL2::ovcVCRedrawEvent() callback function in the vessel module.
	* \note The redraw notification is normally only sent if vc_id is equal to the currently
	*  active virtual cockpit position (>=0). To invoke the redraw notification
	*  independent of the currently active position, set vc_id to -1.
	*/
OAPIFUNC void       oapiVCTriggerRedrawArea (int vc_id, int area_id);

	/**
	* \brief Define a render target for the head-up display (HUD) in a virtual cockpit.
	* \param spec hud specification (see notes)
	* \note This function should be placed in the body of the VESSEL2::ovcLoadVC() vessel module callback function.
	* \note VCHUDSPEC is a structure defined as:
	* \code 
	*  struct VCHUDSPEC {
	*    DWORD nmesh;    // mesh index
	*    DWORD ngroup;   // group index
	*    VECTOR3 hudcnt; // HUD centre in vessel frame
	*    double size;    // physical size of the HUD [m]
	*  }; \endcode
	* \note The mesh group specified by nmesh and ngroup should be a square panel in
	*  front of the camera position in the virtual cockpit. This group is rendered
	*  separately from the rest of the mesh and should therefore have FLAG 2 set
	*  in the mesh file. The group material and texture can be set to 0.
	* \note The HUD centre position and size are required to allow Orbiter to correctly scale the display.
	* \note Orbiter renders the HUD with completely transparent background. Rendering
	*  the glass pane, brackets, etc. is up to the vessel designer.
	*/
OAPIFUNC void       oapiVCRegisterHUD (const VCHUDSPEC *spec);
//@}


// =============================================================================================
/// \defgroup Dialog Customisation - custom menu, dialogs
// =============================================================================================
//@{ 
	/**
	* \brief Register a new item in the parameter list of the "Extra" tab of the Orbiter Launchpad dialog.
	* \param item pointer to LaunchpadItem structure (see notes)
	* \param parent parent item, or NULL for root item
	* \return Handle for the new item
	* \note The "Extra" list of the Launchpad dialog is customisable and can be used by
	*  modules to allow user selection of global parameters and settings. Data can
	*  be written to/read from file and therefore persist across Orbiter sessions.
	* \note Item is a pointer to a class instance derived from LaunchpadItem.
	*  It defines what is displayed in the list, and how the user accesses the item.
	* \note Items can be arranged in a hierarchy. Child items can be defined by passing
	*  the handle of a previous item as the parent parameter.
	* \note If an entry with the same name as item->Name() already exists, no new
	*  entry is generated, and the handle of the existing entry is returned.
	* \note Because double-clicking on an item both activates it and expands the child
	*  list of parent items, parent items should be inert (i.e. should not define their
	*  clbkOpen method) to avoid ambiguities.
	* \note oapiRegisterLaunchpadItem() should usually be called during the DLL
	*  initialisation function. A matching oapiUnregisterLaunchpadItem() should be
	*  called during the DLL exit function.
	* \sa oapiUnregisterLaunchpadItem, oapiFindLaunchpadItem
	*/
OAPIFUNC LAUNCHPADITEM_HANDLE oapiRegisterLaunchpadItem (LaunchpadItem *item, LAUNCHPADITEM_HANDLE parent = 0);

	/**
	* \brief Unregister a previously registered entry in the "Extra" tab of the Orbiter Launchpad dialog.
	* \param item handle of the item to be removed
	* \return value \e true if item could be unregistered, \e false if no matching item was found.
	* \note A module must unregister all the launchpad items it has registered before it
	* is unloaded, at the latest during ExitModule. Failing to do so will leave stale
	* items in the parameter list of the Extra tab, leading to undefined behaviour.
	* \sa oapiRegisterLaunchpadItem, oapiFindLaunchpadItem
	*/
OAPIFUNC bool       oapiUnregisterLaunchpadItem (LaunchpadItem *item);

	/**
	* \brief Returns a handle for an existing entry in the Extra parameter list.
	* \param name the name of the item in the list (or 0 for first entry)
	* \param parent the parent item below which to search (or 0 for root)
	* \return value Item handle if found, or 0 otherwise.
	* \note This method allows to retrieve the handle of an already existing entry in the
	*  Extra list. It is useful for placing new items below a parent that wasn't defined by the module itself.
	* \note It can be used iteratively to search for lower-level entries.
	* \note If name is not set, the first child entry of parent is returned (or the first root entry, if parent==0).
	* \note You should only attach children to items that don't themselves define an activation method.
	* \sa oapiRegisterLaunchpadItem, oapiUnregisterLaunchpadItem
	*/
OAPIFUNC LAUNCHPADITEM_HANDLE oapiFindLaunchpadItem (const char *name = 0, LAUNCHPADITEM_HANDLE parent = 0);

typedef void (*CustomFunc)(void *context);

	/**
	* \brief Register a custom function. Custom functions can be accessed in Orbiter by pressing
	*  Ctrl-F4. A common use for custom functions is opening plugin dialog boxes.
	* \param label label to appear in the custom function list.
	* \param desc a short description of the function
	* \param func pointer to the function to be executed
	* \param context pointer to custom data which will be passed to func
	* \return function identifier
	* \note The interface of the custom function is defined as follows:\n
	* \code typedef void (*CustomFunc)(void *context) \endcode
	*  where context is the pointer passed to oapiRegisterCustomCmd().
	* \sa oapiUnregisterCustomCmd
	*/
OAPIFUNC DWORD      oapiRegisterCustomCmd (char *label, char *desc, CustomFunc func, void *context);

	/**
	* \brief Unregister a previously defined custom function.
	* \param cmdId custom function identifier (as returned by oapiRegisterCustomCmd())
	* \return \e false indicates failure (cmdId not recognised)
	* \sa oapiRegisterCustomCmd
	*/
OAPIFUNC bool       oapiUnregisterCustomCmd (int cmdId);

	/**
	* \brief Open a dialog box defined as a Windows resource.
	* \param hDLLInst module instance handle (as obtained from InitModule)
	* \param resourceId dialog resource identifier
	* \param msgProc pointer to Windows message handler
	* \param context optional user-defined pointer
	* \return handle of the new dialog box, or NULL if the dialog was open already.
	* \note Use oapiOpenDialog() instead of standard Windows methods such as
	*  CreateWindow or DialogBox, to make sure the dialog works in fullscreen mode.
	* \note Only one instance of a dialog box can be open at a time. A second call to
	*  oapiOpenDialog() with the same dialog id will fail and return NULL.
	* \note The interface of the message handler is as follows:
	* \code INT_PTR CALLBACK MsgProc ( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) \endcode
	* \note See standard Windows documentation for usage of the dialog message handler.
	* \note The context pointer can be set to user-defined data which can be retrieved
	*  via the oapiGetDialogContext() function. This allows to pass data into the message handler.
	* \note Note that oapiGetDialogContext() can not be used when processing the
	* \c WM_INITDIALOG message. In this case, the context pointer can be acessed via lParam instead.
	* \sa oapiFindDialog, oapiCloseDialog, oapiOpenDialogEx
	*/
OAPIFUNC HWND       oapiOpenDialog (HINSTANCE hDLLInst, int resourceId, DLGPROC msgProc, void *context = 0);

	/**
	* \brief Open a dialog box specified by an ImGuiDialog object.
	* \param dlg pointer to an ImGuiDialog object responsible for drawing the dialog box.
	* \note Only one instance of a dialog box can be open at a time. A second call to
	*  oapiOpenDialog() with the same dialog will do nothing.
	*/
OAPIFUNC void       oapiOpenDialog (ImGuiDialog *dlg);

	/**
	* \brief Open a dialog box defined as a Windows resource. This version provides additional
	*  functionality compared to oapiOpenDialog().
	* \param hDLLInst module instance handle (as obtained from InitModule)
	* \param resourceId dialog resource identifier
	* \param msgProc pointer to Windows message handler
	* \param flag bit-flags to define dialog box options (see notes)
	* \param context optional user-defined pointer
	* \return handle of the new dialog box, or NULL if the box could not be opened.
	* \note The flag parameter can be a combination of the following values:
	* - \c DLG_ALLOWMULTI: Allows multiple instances of the same dialog resource
	*  to be open simultaneously.\n
	* - \c DLG_CAPTIONCLOSE: Shows a Close button in the dialog title bar.
	*  Pressing it produces an \c IDCANCEL notification to the message procedure.\n
	* - \c DLG_CAPTIONHELP: Shows a Help button in the dialog title bar. Pressing it
	*  produces an \c IDHELP notification to the message procedure.
	* \note If customised title bar buttons are requested, the dialog box template should
	* not contain standard title buttons, by omitting the \c WS_SYSMENU window style.
	* \note Additional buttons can be created by using the oapiAddTitleButton function.
	* \sa oapiFindDialog, oapiCloseDialog, oapiGetDialogContext
	*/
OAPIFUNC HWND       oapiOpenDialogEx (HINSTANCE hDLLInst, int resourceId, DLGPROC msgProc, DWORD flag = 0, void *context = 0);

	/**
	* \brief Returns the window handle of an open dialog box, or NULL if the specified dialog box is not open.
	* \param hDLLInst module instance handle (as obtained from InitModule)
	* \param resourceId dialog resource identifier
	* \return Window handle of dialog box, or NULL if the dialog was not found.
	*/
OAPIFUNC HWND       oapiFindDialog (HINSTANCE hDLLInst, int resourceId);

	/**
	* \brief Close a dialog box.
	* \param hDlg dialog window handle (as obtained by oapiOpenDialog)
	* \note This function should be called in response to an \c IDCANCEL message in the
	*  dialog message handler to close a dialog which was opened by oapiOpenDialog().
	*/
OAPIFUNC void       oapiCloseDialog (HWND hDlg);

	/**
	* \brief Close a dialog box.
	* \param dlg object pointer that was used to open the dialog box.
	*/
OAPIFUNC void       oapiCloseDialog (ImGuiDialog *dlg);

	/**
	* \brief Retrieves the context pointer of a dialog box which has been defined during the call to oapiOpenDialog().
	* \param hDlg dialog window handle
	* \note  This function returns NULL if no context pointer was specified in oapiOpenDialog().
	*/
OAPIFUNC void      *oapiGetDialogContext (HWND hDlg);

OAPIFUNC bool       oapiRegisterWindow (HINSTANCE hDLLInst, HWND hWnd, DWORD flag = 0);

	/**
	* \brief Adds a custom button in the title bar of a dialog box.
	* \param msgid The message identifier generated by pressing the button
	* \param hBmp bitmap containing the button images.
	* \param flag additional parameters (see notes)
	* \return \e true if the button could be created, \e false otherwise.
	* \note oapiAddTitleButton can only be called while processing the
	*  \c WM_INITDIALOG message in the dialog message procedure.
	* \note Up to 5 buttons can be created in the title bar, including the standard buttons
	*  defined in the call to oapiOpenDialogEx.
	* \note Whenever the users left-clicks on the button, a \c WM_COMMAND message is
	*  generated in the message procedure, where the low-word of the WPARAM
	*  parameter is set to msgid.
	* \note The button size defined in the bitmap should be 15x15 pixels large. Their
	*  look should conform to Orbiter's standard dialog buttons.
	* \note The following bit-flags in the flag parameter are currently supported:
	*  \c DLG_CB_TWOSTATE: The button has two states, and clicking on it will flip
	*  between the two states.
	* \note If the \c DLG_CB_TWOSTATE flag is set, the bitmap must be 15x30 pixels
	*  large, containing two images, where the upper image represents the initial
	*  state, and the lower image represents the "checked" state.
	* \note If the \c DLG_CB_TWOSTATE flag is set, the button state (0 or 1) is passed in
	*  the high-word of the WPARAM parameter whenever the dialog is notified of a button press.
	*/
OAPIFUNC bool       oapiAddTitleButton (DWORD msgid, HBITMAP hBmp, DWORD flag);

OAPIFUNC DWORD      oapiGetTitleButtonState (HWND hDlg, DWORD msgid);

OAPIFUNC bool       oapiSetTitleButtonState (HWND hDlg, DWORD msgid, DWORD state);

	/**
	* \brief Default Orbiter dialog message handler.
	* \details This function should be called from the message handler of all
	* dialogs created with oapiOpenDialog to perform default actions
	* for any messages not processed in the handler.
	* \n<b> Parameters:</b> \n
	* The parameters passed to the message handler.
	* \return The value returned by oapiDefDialogProc should be returned by the message handler.
	* \n <b> Typical usage:</b>\n
	* \code
	* INT_PTR CALLBACK MsgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
	* {
	*   switch (uMsg) {
	*   case WM_COMMAND:
	*      switch (LOWORD (wParam)) {
	*      case IDCANCEL: // dialog closed by user
	*         CloseDlg (hDlg);
	*         return TRUE;
	*       }
	*       break;
	*       // add more messages to be processed here
	*   }
	*   return oapiDefDialogProc (hDlg, uMsg, wParam, lParam);
	* }
	* \endcode
	* \note oapiDefDialogProc currently only processes the WM_SETCURSOR message,
	*  and always returns \e false.
	* \sa oapiCloseDialog, oapiFindDialog, oapiOpenDialog
	*/
OAPIFUNC INT_PTR oapiDefDialogProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

	/**
	 * \brief Opens the ingame help window on the specified help page.
	 * \param hcontext help context structure.
	 * \return Currently always returns \e true.
	 */
OAPIFUNC bool oapiOpenHelp (HELPCONTEXT *hcontext);

	/**
	 * \brief Opens a help window outside a simulation session, i.e. when the
	 *   Launchpad dialog is displayed.
	 * \param hcontext help context structure.
	 * \return Currently always returns \e true.
	 */
OAPIFUNC bool oapiOpenLaunchpadHelp (HELPCONTEXT *hcontext);

	/**
	 * \brief Returns the display mode of the main menu bar.
	 * \return 0=show, 1=hide, 2=auto-hide
	 * \sa oapiSetMainMenuVisibilityMode, oapiGetMainInfoVisibilityMode, oapiSetMainInfoVisibilityMode
	 */
OAPIFUNC DWORD oapiGetMainMenuVisibilityMode();

	/**
	 * \brief Set the display mode for the main menu bar.
	 * \param mode display mode: 0=show, 1=hide, 2=auto-hide
	 * \sa oapiGetMainMenuVisibilityMode, oapiGetMainInfoVisibilityMode, oapiSetMainInfoVisibilityMode
	 */
OAPIFUNC void oapiSetMainMenuVisibilityMode (DWORD mode);

	/**
	 * \brief Returns the display mode of the two info blocks at the top left and right screen corners.
	 * \return 0=show, 1=hide, 2=auto-hide
	 * \sa oapiSetMainInfoVisibilityMode, oapiGetMainMenuVisibilityMode, oapiSetMainMenuVisibilityMode
	 */
OAPIFUNC DWORD oapiGetMainInfoVisibilityMode();

	/**
	 * \brief Set the display mode for the two info blocks at the top left and right screen corners.
	 * \param mode display mode: 0=show, 1=hide, 2=auto-hide
	 * \sa oapiGetMainInfoVisibilityMode, oapiGetMainMenuVisibilityMode, oapiSetMainMenuVisibilityMode
	 */
OAPIFUNC void oapiSetMainInfoVisibilityMode (DWORD mode);
//@}


// =============================================================================================
/// \defgroup FileIO File IO Functions
// =============================================================================================
//@{ 
	/**
	* \brief Open a file for reading or writing.
	* \param fname file name (with optional path)
	* \param mode read/write mode (see notes)
	* \param root path origin (see notes)
	* \return file handle
	* \note The following access modes are supported:\n
	*  - \c FILE_IN read\n
	*  - \c FILE_IN_ZEROONFAIL read\n
	*  - \c FILE_OUT write (overwrite)\n
	*  - \c FILE_APP write (append)\n
	* \note The file path defined in fname is relative to either the main Orbiter folder or
	*  to one of Orbiter's default subfolders, depending on the root parameter:\n
	*  - \c ROOT Orbiter main directory\n
	*  - \c CONFIG Orbiter config folder\n
	*  - \c SCENARIOS Orbiter scenarios folder\n
	*  - \c TEXTURES Orbiter standard texture folder\n
	*  - \c TEXTURES2 Orbiter high-res texture folder\n
	*  - \c MESHES Orbiter mesh folder\n
	*  - \c MODULES Orbiter module folder\n
	* \note You should always specify a standard Orbiter subfolder by the above
	*  mechanism, rather than manually as a path in fname, because Orbiter
	*  installations can redirect these directories.
	* \note Access mode FILE_IN will always return a valid file handle, even if the file
	*  doesn't exist or can't be opened for reading (in which case all subsequent read
	*  attempts will fail). By contrast, FILE_IN_ZEROONFAIL will return 0 if the requested
	*  file can't be opened for reading.
	* \note Be careful when opening a file for writing in the standard Orbiter subfolders:
	*  except for ROOT and SCENARIOS, all other standard folders may be readonly
	*  (e.g. for CD installations)
	* \sa oapiCloseFile
	*/
OAPIFUNC FILEHANDLE oapiOpenFile (const char *fname, FileAccessMode mode, PathRoot root = ROOT);

	/**
	* \brief Close a file after reading or writing.
	* \param file file handle
	* \param mode access mode with which the file was opened
	* \note Use this function on files opened with oapiOpenFile after finishing with it.
	* \note The file access mode passed to oapiCloseFile must be the same as used to open it.
	*/
OAPIFUNC void       oapiCloseFile (FILEHANDLE file, FileAccessMode mode);

	/**
	* \brief Writes the current simulation state to a scenario file.
	* \param fname scenario file name
	* \param desc scenario description
	* \return \e true if scenario could be written successfully, \e false if an error occurred.
	* \note The file name is always calculated relative from the default orbiter scenario
	*  folder (usually Orbiter\\Scenarios). The file name can contain a relative path
	*  starting from that directory, but the subdirectories must already exist. The
	*  function will not create new directories. The file name should not contain an
	*  absolute path.
	* \note The file name should not contain an extension. Orbiter will automatically add
	*  a .scn extension.
	* \note The description string can be empty ("").
	*/
OAPIFUNC bool oapiSaveScenario (const char *fname, const char *desc);

	/**
	* \brief Writes a line to a file.
	* \param file file handle
	* \param line line to be written (zero-terminated)
	*/
OAPIFUNC void oapiWriteLine (FILEHANDLE file, char *line);

	/**
	* \brief Writes a line to the Orbiter log file (orbiter.log) in the main orbiter directory.
	* \param line line to be written (zero-terminated)
	* \note This function is intended for diagnostic initialisation and error messages by
	*  plugin modules. The messages should make it easier to track problems.
	* \note Avoid unnecessary output. In particular, don't write to the log file continously
	*  from within the simulation loop.
	* \sa oapiWriteLogV
	*/
OAPIFUNC void oapiWriteLog (char *line);

	/**
	* \brief Writes a formatted string with variable number of arguments to orbiter.log.
	* \param format Format string. Can contain any C-style parameter flags.
	* \param ... List of output parameters. Must match the parameter flags in the format string.
	* \note A newline character is appended to the end of the format string.
	* \sa oapiWriteLog
	*/
OAPIFUNC void oapiWriteLogV (const char *format, ...);

	/**
	* \brief Writes a formatted error message with variable number of arguments to orbiter.log.
	* \param format Format string. Can contain any C-style parameter flags.
	* \param ... List of output parameters. Must match the parameter flags in the format string.
	* \sa oapiWriteLog, oapiWriteLogV
	*/
#define oapiWriteLogError(format, ...) __writeLogError(__FUNCTION__,__FILE__,__LINE__, format, __VA_ARGS__)
OAPIFUNC void __writeLogError(const char *func, const char *file, int line, const char *format, ...);

   /**
	* \brief Writes a string-valued item to a scenario file.
	* \param scn file handle
	* \param item item id
	* \param string string to be written (zero-terminated)
	*/
OAPIFUNC void oapiWriteScenario_string (FILEHANDLE scn, char *item, char *string);

	/**
	* \brief Writes an integer-valued item to a scenario file.
	* \param scn file handle
	* \param item item id
	* \param i integer value to be written
	*/
OAPIFUNC void oapiWriteScenario_int (FILEHANDLE scn, char *item, int i);

	/**
	* \brief Writes a floating point-valued item to a scenario file.
	* \param scn file handle
	* \param item item id
	* \param d floating point value to be written
	*/
OAPIFUNC void oapiWriteScenario_float (FILEHANDLE scn, char *item, double d);

	/**
	* \brief Writes a vector-valued item to a scenario file.
	* \param scn file handle
	* \param item item id
	* \param vec vector to be written
	*/
OAPIFUNC void oapiWriteScenario_vec (FILEHANDLE scn, char *item, const VECTOR3 &vec);

	/**
	* \brief Reads an item from a scenario file.
	* \param scn file handle
	* \param line pointer to the scanned line
	* \note The function returns \e true as long as an item for the current block could be
	*   read. It returns false at EOF, or when an "END" token is read.
	* \note Leading and trailing whitespace, and trailing comments (from ";" to EOL) are
	*   automatically removed.
	* \note "line" points to an internal static character buffer. The buffer grows
	*   automatically to hold lines of arbitrary length.
	* \note The buffer is overwritten on the next call to oapiReadScenario_nextline,
	*   so it must be copied or processed before the next call.
	*/
OAPIFUNC bool oapiReadScenario_nextline (FILEHANDLE scn, char *&line);

	/**
	* \brief Read the value of a tag from a configuration file.
	* \param f file handle
	* \param item tag defining the item
	* \param string character-string value
	* \return \e true if tag was found in the file, \e false if not.
	* \note The tag-value entries of a configuration file have the format \<tag\> = \<value\>
	* \note The functions search the complete file independent of the current position of the file pointer.
	* \note Whitespace around tag and value are discarded, as well as comments 
	*  beginning with a semicolon (;) to the end of the line.
	* \note String values can contain internal whitespace.
	*/
OAPIFUNC bool oapiReadItem_string (FILEHANDLE f, char *item, char *string);

	/**
	* \brief Read the value of a tag from a configuration file.
	* \param f file handle
	* \param item tag defining the item
	* \param d double value
	* \return \e true if tag was found in the file, \e false if not.
	* \sa oapiReadItem_string for more details.
	*/
OAPIFUNC bool oapiReadItem_float (FILEHANDLE f, char *item, double &d);

	/**
	* \brief Read the value of a tag from a configuration file.
	* \param f file handle
	* \param item tag defining the item
	* \param i integer value
	* \return \e true if tag was found in the file, \e false if not.
	* \sa oapiReadItem_string for more details.
	*/
OAPIFUNC bool oapiReadItem_int (FILEHANDLE f, char *item, int &i);

	/**
	* \brief Read the value of a tag from a configuration file.
	* \param f file handle
	* \param item tag defining the item
	* \param b boolean value
	* \return \e true if tag was found in the file, \e false if not.
	* \note In a file boolean values are represented by the strings "FALSE" and "TRUE".
	* \sa oapiReadItem_string for more details.
	*/
OAPIFUNC bool oapiReadItem_bool (FILEHANDLE f, char *item, bool &b);

	/**
	* \brief Read the value of a tag from a configuration file.
	* \param f file handle
	* \param item tag defining the item
	* \param vec vector value
	* \return \e true if tag was found in the file, \e false if not.
	* \note Vector values are represented by space-separated triplets of floating point values.
	* \sa oapiReadItem_string for more details.
	*/
OAPIFUNC bool oapiReadItem_vec (FILEHANDLE f, char *item, VECTOR3 &vec);
	
	/**
	* \brief Write a tag and its value to a configuration file.
	* \param f file handle
	* \param item pointer to tag string
	* \param string character-string value
	* \note Use these functions to write items (tags and values) to configuration files.
	* \note The format of the written items is recognised by the corresponding \b oapiReadItem_xxx functions.
	* \note For historic reasons, the format for scenario file entries is different. 
	*  Use the oapiWriteLine function.
	* \sa oapiReadItem_string
	*/
OAPIFUNC void oapiWriteItem_string (FILEHANDLE f, char *item, char *string);

	/**
	* \brief Write a tag and its value to a configuration file.
	* \param f file handle
	* \param item pointer to tag string
	* \param d double value
	* \sa oapiWriteItem_string for more details
	*/
OAPIFUNC void oapiWriteItem_float (FILEHANDLE f, char *item, double d);

	/**
	* \brief Write a tag and its value to a configuration file.
	* \param f file handle
	* \param item pointer to tag string
	* \param i integer value
	* \sa oapiWriteItem_string for more details
	*/
OAPIFUNC void oapiWriteItem_int (FILEHANDLE f, char *item, int i);

	/**
	* \brief Write a tag and its value to a configuration file.
	* \param f file handle
	* \param item pointer to tag string
	* \param b boolean value
	* \note In a file boolean values are represented by the strings "FALSE" and "TRUE".
	* \sa oapiWriteItem_string for more details
	*/
OAPIFUNC void oapiWriteItem_bool (FILEHANDLE f, char *item, bool b);

	/**
	* \brief Write a tag and its value to a configuration file.
	* \param f file handle
	* \param item pointer to tag string
	* \param vec vector value
	* \note Vector values are represented by space-separated triplets of floating point values.
	* \sa oapiWriteItem_string for more details
	*/
OAPIFUNC void oapiWriteItem_vec (FILEHANDLE f, char *item, const VECTOR3 &vec);
//@} 


// =============================================================================================
/// \defgroup Utility Utility functions
// =============================================================================================
//@{
	/**
	* \brief Returns uniformly distributed pseudo-random number in the range [0..1].
	* \return Random value between 0 and 1.
	* \note This function uses the system call rand(), so the quality of the random
	*  sequence depends on the system implementation. If you need high-quality
	*  random sequences you may need to implement your own generator.
	* \note Orbiter seeds the generator with the system time on startup, so the
	*  generated sequences are not reproducible.
	*/
OAPIFUNC double oapiRand ();

	/**
	* \brief Compress a data block.
	* \param ebuf input data buffer
	* \param nebuf size of input buffer
	* \param zbuf output data buffer to receive the compressed data
	* \param nzbuf size of output buffer
	* \return size of compressed data buffer (0=error)
	* \note The output buffer must have been allocated by the caller before the call to
	*   opaiDeflate, and must be large enough to store the complete compressed data buffer.
	*   If the buffer is too small, the output data block is invalid, and the function
	*   returns 0.
	* \sa oapiInflate
	*/
OAPIFUNC DWORD oapiDeflate (const BYTE *ebuf, DWORD nebuf, BYTE *zbuf, DWORD nzbuf);

	/**
	* \brief Uncompress a data block previously compressed with oapiDeflate.
	* \param zbuf compressed input data buffer
	* \param nzbuf size of input buffer
	* \param ebuf output data buffer to receive the uncompressed data
	* \param nebuf size of output buffer
	* \return size of uncompressed data buffer (0=error)
	* \note The output buffer must have been allocated by the caller before the call to
	*   oapiInflate, and must be large enough to store the complete uncompressed data
	*   buffer. If the buffer is too small, the output data block is invalid, and the
	*   function returns 0.
	* \sa oapiDeflate
	*/
OAPIFUNC DWORD oapiInflate (const BYTE *zbuf, DWORD nzbuf, BYTE *ebuf, DWORD nebuf);

	/**
	* \brief Returns a colour value adapted to the current screen colour 
	*  depth for given red, green and blue components.
	* \param red red component (0-255)
	* \param green green component (0-255)
	* \param blue blue component (0-255)
	* \return colour value
	* \note Colour values are required for some surface functions like oapiClearSurface
	*  or oapiSetSurfaceColourKey. The colour key for a given RGB triplet depends
	*  on the screen colour depth. This function returns the colour value for the
	*  closest colour match which can be displayed in the current screen mode.
	* \note In 24 and 32 bit modes the requested colour can always be matched. The
	*  colour value in that case is (red \<\< 16) + (green \<\< 8) + blue.
	* \note For 16 bit displays the colour value is calculated as
	*  ((red*31)/255) \<\< 11 + ((green*63)/255 \<\< 5 + (blue*31)/255
	*  assuming a "565" colour mode (5 bits for red, 6, for green, 5 for blue). This
	*  means that a requested colour may not be perfectly matched.
	* \note These colour values should not be used for Windows (GDI) drawing
	*  functions where a COLORREF value is expected.
	*/
OAPIFUNC DWORD oapiGetColour (DWORD red, DWORD green, DWORD blue);
//@} 


// =============================================================================================
/// \defgroup UserInput User input functions 
// =============================================================================================
//@{
	/**
	* \brief Opens a modal input box requesting a string from the user.
	* \param title input box title
	* \param Clbk callback function receiving the result of the user input (see notes)
	* \param buf initial state of the input string
	* \param vislen number of characters visible in input box
	* \param usrdata user-defined data passed to the callback function
	* \note Format for callback function:
	* \code bool InputCallback (void *id, char *str, void *usrdata ) \endcode
	*  where id identifies the input box, str contains the user-supplied string, and
	*  usrdata contains the data specified in the call to oapiOpenInputBox.
	*  The callback function should return \e true if it accepts the string, false
	*  otherwise (the box will not be closed if the callback function returns false).
	* \note The box can be closed by the user by pressing Enter ("OK") or Esc
	*  ("Cancel"). The callback function is only called in the first case.
	* \note The input box is modal, i.e. all keyboard input is redirected into the dialog
	*  box. Normal key functions resume after the box is closed.
	* \sa oapiOpenInputBoxEx
	*/
OAPIFUNC void oapiOpenInputBox (char *title, bool (*Clbk)(void*,char*,void*), char *buf = 0, int vislen = 20, void *usrdata = 0);

OAPIFUNC void oapiOpenInputBoxEx (const char *title, bool (*Clbk_enter)(void*,char*,void*), bool (*Clbk_cancel)(void*,char*,void*), char *buf = 0, int vislen = 20, void *usrdata = 0, DWORD flags = 0);

/**
 * \brief Send a buffered key event to Orbiter, to be treated like a user keypress.
 * \param key keycode (see \ref keycodes)
 * \param mod list of modifier keys
 * \param nmod length of modifier list
 * \param onRunningOnly only send the key event if the simulation is not paused
 * \return true if key was dispatched (but not necessarily processed)
 */
OAPIFUNC bool oapiSimulateBufferedKey (DWORD key, DWORD *mod = 0, DWORD nmod = 0, bool onRunningOnly = false);

/**
 * \brief Send a key state to Orbiter for one frame, to be treated like user keyboard input.
 * \param kstate state for each key, where bit 0x80 indicates key pressed
 * \param onRunningOnly only send the key event if the simulation is not paused
 * \return true if key was dispatched (but not necessarily processed)
 * \note Orbiter doesn't process the simulated key state request directly, but merges
 *   all requests and the actual key state for the current frame, and submits the result
 *   once per frame.
 * \note To simulate a continuous key press event, this function must be called for multiple
 *   frames for the duration of the simulated input.
 */
OAPIFUNC bool oapiSimulateImmediateKey (char kstate[256], bool onRunningOnly = false);
//@} 


// =============================================================================================
/**
  * \defgroup Annotations Onscreen annotations
  *
  * These functions can be used to display text on top of the render window during a running
  * simulation. These may include flight parameters of the currently observed spacecraft, user
  * instructions for tutorials, or debugging information during development.
  */
// =============================================================================================
//@{
/**
 * \brief Creates an annotation handle for displaying onscreen text during a
 *   simulation.
 * \param exclusive [not currently used]
 * \param size text scaling factor (>0, 1=standard)
 * \param col text colour (RGB triplet, range 0-1 for each component)
 * \return Annotation handle
 * \sa oapiDelAnnotation, oapiAnnotationSetPos, oapiAnnotationSetSize,
 *   oapiAnnotationSetColour, oapiAnnotationSetText
 */
OAPIFUNC NOTEHANDLE oapiCreateAnnotation (bool exclusive, double size, const VECTOR3 &col);

/**
 * \brief Deletes an annotation handle.
 * \param hNote annotation handle
 * \return \e true on success, \e false if an annotation corresponding to hNote was
 *   not found.
 * \sa oapiCreateAnnotation
 */
OAPIFUNC bool oapiDelAnnotation (NOTEHANDLE hNote);

/**
 * \brief Resets the bounding box of the annotation display area.
 * \param hNote annotation handle
 * \param x1 left edge of bounding box (0 <= x1 < x2)
 * \param y1 top edge of bounding box (0 <= y1 < y2)
 * \param x2 right edge of bounding box (x1 < x2 <= 1)
 * \param y2 bottom edge of bounding box (y1 < y2 <= 1)
 * \note boundary values are specified in units of the render window area, with (0,0)
 *   being the top left corner, and (1,1) the bottom right corner.
 * \note If the bounding box is set too small, part of the annotation may not be
 *   visible.
 * \sa oapiCreateAnnotation
 */
OAPIFUNC void oapiAnnotationSetPos (NOTEHANDLE hNote, double x1, double y1, double x2, double y2);

/**
 * \brief Resets the font size of the annotation text.
 * \param hNote annotation handle
 * \param size font size in relative units (> 0)
 * \note Annotations are sized in relation to the simulation window size. Size 1 is
 *   the default annotation size.
 * \sa oapiCreateAnnotation
 */
OAPIFUNC void oapiAnnotationSetSize (NOTEHANDLE hNote, double size);

/**
 * \brief Resets the font colour of the annotation text.
 * \param hNote annotation handle
 * \param col font colour (RGB triplet with ranges 0-1)
 * \sa oapiCreateAnnotation
 */
OAPIFUNC void oapiAnnotationSetColour (NOTEHANDLE hNote, const VECTOR3 &col);

/**
 * \brief Writes a new annotation to screen, or overwrites the previous text.
 * \param hNote annotation handle
 * \param note annotation text
 * \sa oapiCreateAnnotation
 */
OAPIFUNC void oapiAnnotationSetText (NOTEHANDLE hNote, char *note);
//@}


// ======================================================================
/// \defgroup Obsolete Obsolete functions
// ======================================================================
//@{
/**
 * \deprecated Stations are no longer distinguished from vessels.
 *   This function does not perform any action other than writing a
 *   warning to the log file.
 *   Use \ref oapiGetVesselByName instead.
 */
OAPIFUNC OBJHANDLE oapiGetStationByName (char *name);

/**
 * \deprecated Stations are no longer distinguished from vessels.
 *   This function does not perform any action other than writing a
 *   warning to the log file.
 *   Use \ref oapiGetVesselByIndex instead.
 */
OAPIFUNC OBJHANDLE oapiGetStationByIndex (int index);

/**
 * \deprecated Stations are no longer distinguished from vessels.
 *   This function always returns 0.
 *   Use \ref oapiGetVesselCount instead.
 */
OAPIFUNC DWORD oapiGetStationCount ();

/**
 * \brief Returns a vessel's airspeed vector w.r.t. the closest planet or moon in the local
 *  horizon's frame of reference.
 * \deprecated This method has been replaced by \ref oapiGetAirspeedVector(OBJHANDLE,REFFRAME,VECTOR3*)
 */
OAPIFUNC BOOL oapiGetAirspeedVector (OBJHANDLE hVessel, VECTOR3 *speedvec);

/**
 * \brief Returns a vessel's airspeed vector w.r.t. the closest planet or moon in the vessel's local
 *  frame of reference.
 * \deprecated This method has been replaced by \ref oapiGetAirspeedVector(OBJHANDLE,REFFRAME,VECTOR3*)
 */
OAPIFUNC BOOL oapiGetShipAirspeedVector (OBJHANDLE hVessel, VECTOR3 *speedvec);

/**
 * \brief Returns the current focus vessel's airspeed w.r.t. the closest planet or moon.
 * \deprecated This method has been replaced by \ref oapiGetAirspeed()
 */
OAPIFUNC BOOL oapiGetFocusAirspeed (double *airspeed);
	
/**
 * \brief Returns the current focus vessel's airspeed vector w.r.t. the closest planet or moon in
 *  the local horizon's frame of reference.
 * \deprecated This method has been replaced by \ref oapiGetAirspeedVector(OBJHANDLE,REFFRAME,VECTOR3*)
 */
OAPIFUNC BOOL oapiGetFocusAirspeedVector (VECTOR3 *speedvec);

/**
 * \brief Returns the current focus vessel's airspeed vector w.r.t. closest planet or moon in the
 *  vessel's local frame of reference.
 * \deprecated This method has been replaced by \ref oapiGetAirspeedVector(OBJHANDLE,REFFRAME,VECTOR3*)
 */
OAPIFUNC BOOL oapiGetFocusShipAirspeedVector (VECTOR3 *speedvec);

/**
 * \deprecated This function has been replaced by oapiGetAtm.
 * \brief Returns the atmospheric pressure and density caused by a planetary atmosphere at
 *  the current vessel position.
 * \param hVessel vessel handle
 * \param pressure pointer to variable receiving pressure value [Pa]
 * \param density pointer to variable receiving density value [kg/m<sup>3</sup>]
 * \note Pressure and density are calculated using an exponential barometric
 *  equation, without accounting for local variations.
 * \sa oapiGetAtm
 */
OAPIFUNC void oapiGetAtmPressureDensity (OBJHANDLE hVessel, double *pressure, double *density);

/**
 * \deprecated This function has been replaced by oapiGetAtm.
 * \brief Returns the atmospheric pressure and density caused by a planetary atmosphere at
 *  the current focus vessel's position.
 * \param pressure pointer to variable receiving pressure value [Pa]
 * \param density pointer to variable receiving density value [kg/m<sup>3</sup>]
 * \note Pressure and density are calculated using an exponential barometric
 *  equation, without accounting for local variations.
 * \sa oapiGetAtm
 */
OAPIFUNC void oapiGetFocusAtmPressureDensity (double *pressure, double *density);

OAPIFUNC bool       oapiAcceptDelayedKey (char key, double interval);

	/**
	* \brief Register a custom MFD mode.
	* \deprecated This function has been replaced by \ref oapiRegisterMFDMode(MFDMODESPECEX&).
	* \sa oapiRegisterMFDMode(MFDMODESPECEX&)
	*/
OAPIFUNC int oapiRegisterMFDMode (MFDMODESPEC &spec);

	/**
	* \brief Returns the mode identifier and spec for an MFD mode defined by its name.
	* \deprecated This function has been replaced by \ref oapiGetMFDModeSpecEx
	* \sa oapiGetMFDModeSpecEx
	*/
OAPIFUNC int oapiGetMFDModeSpec (char *name, MFDMODESPEC **spec = 0);

	/**
	 * \deprecated This function is unsafe because it can be used by vessels who don't
	 *   own the current cockpit visuals. Use \ref VESSEL::TriggerPanelRedrawArea instead.
	 */
OAPIFUNC void oapiTriggerPanelRedrawArea (int panel_id, int area_id);

	/**
	 * \deprecated This function is unsafe because it can be used by vessels who don't
	 *   own the current cockpit visuals. Use \ref VESSEL::TriggerRedrawArea instead.
  	 */
OAPIFUNC void oapiTriggerRedrawArea (int panel_id, int vc_id, int area_id);
//@}

//@}  -- End of Orbiter API interface methods --


/**
 * \ingroup defines
 * \defgroup optcat Option category identifiers
 */
//@{
#define OPTCAT_CELSPHERE                     0x0001
#define OPTCAT_PLANETARIUM                   0x0002
#define OPTCAT_INSTRUMENT                    0x0003
#define OPTCAT_VESSEL                        0x0004
#define OPTCAT_JOYSTICK                      0x0005
//@}

/**
 * \ingroup defines
 * \defgroup optitem Option item identifiers
 */
//@{
#define OPTITEM_CELSPHERE_ACTIVATESTARDOTS   0x0001
#define OPTITEM_CELSPHERE_STARDISPLAYPARAM   0x0002
#define OPTITEM_CELSPHERE_ACTIVATESTARIMAGE  0x0003
#define OPTITEM_CELSPHERE_STARIMAGECHANGED   0x0004
#define OPTITEM_CELSPHERE_ACTIVATEBGIMAGE    0x0005
#define OPTITEM_CELSPHERE_BGIMAGECHANGED     0x0006
#define OPTITEM_CELSPHERE_BGIMAGEBRIGHTNESS  0x0007
#define OPTITEM_PLANETARIUM_DISPFLAG         0x0001
#define OPTITEM_INSTRUMENT_MFDUPDATEINTERVAL 0x0001
#define OPTITEM_INSTRUMENT_MFDGENERICSIZE    0x0002
#define OPTITEM_INSTRUMENT_MFDGENERICTRANSP  0x0003
#define OPTITEM_INSTRUMENT_MFDVCSIZE         0x0004
#define OPTITEM_INSTRUMENT_PANELSCROLLSPEED  0x0005
#define OPTITEM_INSTRUMENT_PANELSCALE        0x0006
#define OPTITEM_VESSEL_LIMITEDFUEL           0x0001
#define OPTITEM_VESSEL_PADREFUEL             0x0002
#define OPTITEM_JOYSTICK_DEVICE              0x0001
#define OPTITEM_JOYSTICK_PARAM               0x0002
//@}

// ======================================================================
/**
 * \ingroup defines
 * \defgroup keycodes Keyboard key identifiers
 */
// ======================================================================
//@{
#define OAPI_KEY_ESCAPE			0x01  ///< Escape key
#define OAPI_KEY_1				0x02  ///< '1' key on main keyboard
#define OAPI_KEY_2				0x03  ///< '2' key on main keyboard
#define OAPI_KEY_3				0x04  ///< '3' key on main keyboard
#define OAPI_KEY_4				0x05  ///< '4' key on main keyboard
#define OAPI_KEY_5				0x06  ///< '5' key on main keyboard
#define OAPI_KEY_6				0x07  ///< '6' key on main keyboard
#define OAPI_KEY_7				0x08  ///< '7' key on main keyboard
#define OAPI_KEY_8				0x09  ///< '8' key on main keyboard
#define OAPI_KEY_9				0x0A  ///< '9' key on main keyboard
#define OAPI_KEY_0				0x0B  ///< '0' key on main keyboard
#define OAPI_KEY_MINUS			0x0C  ///< '-' key on main keyboard
#define OAPI_KEY_EQUALS			0x0D  ///< '=' key on main keyboard
#define OAPI_KEY_BACK			0x0E  ///< backspace key
#define OAPI_KEY_TAB			0x0F  ///< tab key
#define OAPI_KEY_Q				0x10  ///< 'Q' key
#define OAPI_KEY_W				0x11  ///< 'W' key
#define OAPI_KEY_E				0x12  ///< 'E' key
#define OAPI_KEY_R				0x13  ///< 'R' key
#define OAPI_KEY_T				0x14  ///< 'T' key
#define OAPI_KEY_Y				0x15  ///< 'Y' key
#define OAPI_KEY_U				0x16  ///< 'U' key
#define OAPI_KEY_I				0x17  ///< 'I' key
#define OAPI_KEY_O				0x18  ///< 'O' key
#define OAPI_KEY_P				0x19  ///< 'P' key
#define OAPI_KEY_LBRACKET		0x1A  ///< '[' (left bracket) key
#define OAPI_KEY_RBRACKET		0x1B  ///< ']' (right bracket) key
#define OAPI_KEY_RETURN			0x1C  ///< 'Enter' key on main keyboard
#define OAPI_KEY_LCONTROL		0x1D  ///< Left 'Ctrl' key
#define OAPI_KEY_A				0x1E  ///< 'A' key
#define OAPI_KEY_S				0x1F  ///< 'S' key
#define OAPI_KEY_D				0x20  ///< 'D' key
#define OAPI_KEY_F				0x21  ///< 'F' key
#define OAPI_KEY_G				0x22  ///< 'G' key
#define OAPI_KEY_H				0x23  ///< 'H' key
#define OAPI_KEY_J				0x24  ///< 'J' key
#define OAPI_KEY_K				0x25  ///< 'K' key
#define OAPI_KEY_L				0x26  ///< 'L' key
#define OAPI_KEY_SEMICOLON		0x27  ///< ';' (semicolon) key
#define OAPI_KEY_APOSTROPHE		0x28  ///< ' (apostrophe) key
#define OAPI_KEY_GRAVE			0x29  ///< accent grave
#define OAPI_KEY_LSHIFT			0x2A  ///< Left 'Shift' key
#define OAPI_KEY_BACKSLASH		0x2B  ///< '\' (Backslash) key
#define OAPI_KEY_Z				0x2C  ///< 'Z' key
#define OAPI_KEY_X				0x2D  ///< 'X' key
#define OAPI_KEY_C				0x2E  ///< 'C' key
#define OAPI_KEY_V				0x2F  ///< 'V' key
#define OAPI_KEY_B				0x30  ///< 'B' key
#define OAPI_KEY_N				0x31  ///< 'N' key
#define OAPI_KEY_M				0x32  ///< 'M' key
#define OAPI_KEY_COMMA			0x33  ///< ',' (comma) key
#define OAPI_KEY_PERIOD			0x34  ///< '.' key on main keyboard
#define OAPI_KEY_SLASH			0x35  ///< '/' key on main keyboard
#define OAPI_KEY_RSHIFT			0x36  ///< Right 'Shift' key
#define OAPI_KEY_MULTIPLY		0x37  ///< * on numeric keypad
#define OAPI_KEY_LALT			0x38  ///< left Alt
#define OAPI_KEY_SPACE			0x39  ///< 'Space' key
#define OAPI_KEY_CAPITAL		0x3A  ///< caps lock key
#define OAPI_KEY_F1				0x3B  ///< F1 function key
#define OAPI_KEY_F2				0x3C  ///< F2 function key
#define OAPI_KEY_F3				0x3D  ///< F3 function key
#define OAPI_KEY_F4				0x3E  ///< F4 function key
#define OAPI_KEY_F5				0x3F  ///< F5 function key
#define OAPI_KEY_F6				0x40  ///< F6 function key
#define OAPI_KEY_F7				0x41  ///< F7 function key
#define OAPI_KEY_F8				0x42  ///< F8 function key
#define OAPI_KEY_F9				0x43  ///< F9 function key
#define OAPI_KEY_F10			0x44  ///< F10 function key
#define OAPI_KEY_NUMLOCK		0x45  ///< 'Num Lock' key
#define OAPI_KEY_SCROLL			0x46  ///< Scroll lock
#define OAPI_KEY_NUMPAD7		0x47  ///< '7' key on numeric keypad
#define OAPI_KEY_NUMPAD8		0x48  ///< '8' key on numeric keypad
#define OAPI_KEY_NUMPAD9		0x49  ///< '9' key on numeric keypad
#define OAPI_KEY_SUBTRACT		0x4A  ///< '-' key on numeric keypad
#define OAPI_KEY_NUMPAD4		0x4B  ///< '4' key on numeric keypad
#define OAPI_KEY_NUMPAD5		0x4C  ///< '5' key on numeric keypad
#define OAPI_KEY_NUMPAD6		0x4D  ///< '6' key on numeric keypad
#define OAPI_KEY_ADD			0x4E  ///< '+' key on numeric keypad
#define OAPI_KEY_NUMPAD1		0x4F  ///< '1' key on numeric keypad
#define OAPI_KEY_NUMPAD2		0x50  ///< '2' key on numeric keypad
#define OAPI_KEY_NUMPAD3		0x51  ///< '3' key on numeric keypad
#define OAPI_KEY_NUMPAD0		0x52  ///< '0' key on numeric keypad
#define OAPI_KEY_DECIMAL		0x53  ///< '.' key on numeric keypad
#define OAPI_KEY_OEM_102		0x56  ///< | \< \> on UK/German keyboards
#define OAPI_KEY_F11			0x57  ///< F11 function key
#define OAPI_KEY_F12			0x58  ///< F12 function key
#define OAPI_KEY_NUMPADENTER	0x9C  ///< Enter on numeric keypad
#define OAPI_KEY_RCONTROL		0x9D  ///< right Control key
#define OAPI_KEY_DIVIDE			0xB5  ///< '/' key on numeric keypad
#define OAPI_KEY_SYSRQ          0xB7  ///< SysRq/PrtScn key
#define OAPI_KEY_RALT           0xB8  ///< right Alt
#define OAPI_KEY_PAUSE          0xC5  ///< Break/Pause key
#define OAPI_KEY_HOME           0xC7  ///< Home on cursor keypad
#define OAPI_KEY_UP             0xC8  ///< up-arrow on cursor keypad
#define OAPI_KEY_PRIOR          0xC9  ///< PgUp on cursor keypad
#define OAPI_KEY_LEFT           0xCB  ///< left-arrow on cursor keypad
#define OAPI_KEY_RIGHT          0xCD  ///< right-arrow on cursor keypad
#define OAPI_KEY_END            0xCF  ///< End on cursor keypad
#define OAPI_KEY_DOWN           0xD0  ///< down-arrow on cursor keypad
#define OAPI_KEY_NEXT           0xD1  ///< PgDn on cursor keypad
#define OAPI_KEY_INSERT         0xD2  ///< Insert on cursor keypad
#define OAPI_KEY_DELETE         0xD3  ///< Delete on cursor keypad
//@}

#define KEYDOWN(buf,key) (buf[key] & 0x80)
#define RESETKEY(buf,key) (buf[key] = 0)

#define KEYMOD_LSHIFT(buf)   (KEYDOWN(buf,OAPI_KEY_LSHIFT))
#define KEYMOD_RSHIFT(buf)   (KEYDOWN(buf,OAPI_KEY_RSHIFT))
#define KEYMOD_SHIFT(buf)    (KEYMOD_LSHIFT(buf) || KEYMOD_RSHIFT(buf))
#define KEYMOD_LCONTROL(buf) (KEYDOWN(buf,OAPI_KEY_LCONTROL))
#define KEYMOD_RCONTROL(buf) (KEYDOWN(buf,OAPI_KEY_RCONTROL))
#define KEYMOD_CONTROL(buf)  (KEYMOD_LCONTROL(buf) || KEYMOD_RCONTROL(buf))
#define KEYMOD_LALT(buf)     (KEYDOWN(buf,OAPI_KEY_LALT))
#define KEYMOD_RALT(buf)     (KEYDOWN(buf,OAPI_KEY_RALT))
#define KEYMOD_ALT(buf)      (KEYMOD_LALT(buf) || KEYMOD_RALT(buf))

// ======================================================================
/// \ingroup defines
/// \defgroup logical_keys Logical key ids
// ======================================================================
//@{
#define OAPI_LKEY_CockpitRotateLeft  0 ///< rotate camera left in cockpit view
#define OAPI_LKEY_CockpitRotateRight 1 ///< rotate camera right in cockpit view
#define OAPI_LKEY_CockpitRotateUp    2 ///< rotate camera up in cockpit view
#define OAPI_LKEY_CockpitRotateDown  3 ///< rotate camera down in cockpit view
#define OAPI_LKEY_CockpitDontLean    4 ///< return to default cockpit camera position
#define OAPI_LKEY_CockpitLeanForward 5 ///< move cockpit camera forward
#define OAPI_LKEY_CockpitLeanLeft    6 ///< move cockpit camera left
#define OAPI_LKEY_CockpitLeanRight   7 ///< move cockpit camera right
#define OAPI_LKEY_CockpitResetCam    8 ///< rotate and shift cockpit camera back to default
#define OAPI_LKEY_PanelShiftLeft     9 ///< shift 2D instrument panel left
#define OAPI_LKEY_PanelShiftRight   10 ///< shift 2D instrument panel right
#define OAPI_LKEY_PanelShiftUp      11 ///< shift 2D instrument panel up
#define OAPI_LKEY_PanelShiftDown    12 ///< shift 2D instrument panel down
#define OAPI_LKEY_PanelSwitchLeft   13 ///< switch to left neighbour panel
#define OAPI_LKEY_PanelSwitchRight  14 ///< switch to right neighbour panel
#define OAPI_LKEY_PanelSwitchUp     15 ///< switch to upper neighbour panel
#define OAPI_LKEY_PanelSwitchDown   16 ///< switch to lower neighbour panel
#define OAPI_LKEY_TrackRotateLeft   17 ///< turn track view camera left
#define OAPI_LKEY_TrackRotateRight  18 ///< turn track view camera right
#define OAPI_LKEY_TrackRotateUp     19 ///< turn track view camera up
#define OAPI_LKEY_TrackRotateDown   20 ///< turn track view camera down
#define OAPI_LKEY_TrackAdvance      21 ///< advance track view camera towards target
#define OAPI_LKEY_TrackRetreat      22 ///< retreat track view camera from target
#define OAPI_LKEY_GroundTiltLeft    23 ///< tilt camera left in ground view
#define OAPI_LKEY_GroundTiltRight   24 ///< tilt camera right in ground view
#define OAPI_LKEY_GroundTiltUp      25 ///< tilt camera up in ground view
#define OAPI_LKEY_GroundTiltDown    26 ///< tilt camera down in ground view
#define OAPI_LKEY_IncMainThrust     27 ///< increment thrust of main thrusters
#define OAPI_LKEY_DecMainThrust     28 ///< decrement thrust of main thrusters
#define OAPI_LKEY_KillMainRetro     29 ///< kill main and retro thrusters
#define OAPI_LKEY_FullMainThrust    30 ///< temporary full main thrust
#define OAPI_LKEY_FullRetroThrust   31 ///< temporary full retro thrust
#define OAPI_LKEY_IncHoverThrust    32 ///< increment thrust of hover thrusters
#define OAPI_LKEY_DecHoverThrust    33 ///< decrement thrust of hover thrusters
#define OAPI_LKEY_RCSEnable         34 ///< enable/disable RCS (reaction control system)
#define OAPI_LKEY_RCSMode           35 ///< toggle linear/rotational RCS mode
#define OAPI_LKEY_RCSPitchUp        36 ///< rotational RCS: pitch up
#define OAPI_LKEY_RCSPitchDown      37 ///< rotational RCS: pitch down
#define OAPI_LKEY_RCSYawLeft        38 ///< rotational RCS: yaw left
#define OAPI_LKEY_RCSYawRight       39 ///< rotational RCS: yaw right
#define OAPI_LKEY_RCSBankLeft       40 ///< rotational RCS: bank left
#define OAPI_LKEY_RCSBankRight      41 ///< rotational RCS: bank right
#define OAPI_LKEY_RCSUp             42 ///< linear RCS: accelerate up (+y)
#define OAPI_LKEY_RCSDown           43 ///< linear RCS: accelerate down (-y)
#define OAPI_LKEY_RCSLeft           44 ///< linear RCS: accelerate left (-x)
#define OAPI_LKEY_RCSRight          45 ///< linear RCS: accelerate right (+x)
#define OAPI_LKEY_RCSForward        46 ///< linear RCS: accelerate forward (+z)
#define OAPI_LKEY_RCSBack           47 ///< linear RCS: accelerate backward (-z)
#define OAPI_LKEY_LPRCSPitchUp      48 ///< rotational RCS: pitch up 10%
#define OAPI_LKEY_LPRCSPitchDown    49 ///< rotational RCS: pitch down 10%
#define OAPI_LKEY_LPRCSYawLeft      50 ///< rotational RCS: yaw left 10%
#define OAPI_LKEY_LPRCSYawRight     51 ///< rotational RCS: yaw right 10%
#define OAPI_LKEY_LPRCSBankLeft     52 ///< rotational RCS: bank left 10%
#define OAPI_LKEY_LPRCSBankRight    53 ///< rotational RCS: bank right 10%
#define OAPI_LKEY_LPRCSUp           54 ///< linear RCS: accelerate up 10% (+y)
#define OAPI_LKEY_LPRCSDown         55 ///< linear RCS: accelerate down 10% (-y)
#define OAPI_LKEY_LPRCSLeft         56 ///< linear RCS: accelerate left 10% (-x)
#define OAPI_LKEY_LPRCSRight        57 ///< linear RCS: accelerate right 10% (+x)
#define OAPI_LKEY_LPRCSForward      58 ///< linear RCS: accelerate forward 10% (+z)
#define OAPI_LKEY_LPRCSBack         59 ///< linear RCS: accelerate backward 10% (-z)
#define OAPI_LKEY_NMHoldAltitude    60 ///< toggle navmode: hold altitude
#define OAPI_LKEY_NMHLevel          61 ///< toggle navmode: level with horizon
#define OAPI_LKEY_NMPrograde        62 ///< toggle navmode: prograde
#define OAPI_LKEY_NMRetrograde      63 ///< toggle navmode: retrograde
#define OAPI_LKEY_NMNormal          64 ///< toggle navmode: normal to orbital plane
#define OAPI_LKEY_NMAntinormal      65 ///< toggle navmode: antinormal to orbital plane
#define OAPI_LKEY_NMKillrot         66 ///< toggle navmode: kill rotation
#define OAPI_LKEY_Undock            67 ///< undock from docked vessel
#define OAPI_LKEY_IncElevatorTrim   68 ///< increment elevator trim setting
#define OAPI_LKEY_DecElevatorTrim   69 ///< decrement elevator trim setting
#define OAPI_LKEY_WheelbrakeLeft    70 ///< apply wheelbrake left
#define OAPI_LKEY_WheelbrakeRight   71 ///< apply wheelbrake right
#define OAPI_LKEY_HUD               72 ///< toggle HUD on/off
#define OAPI_LKEY_HUDMode           73 ///< switch through HUD modes
#define OAPI_LKEY_HUDReference      74 ///< query reference object for HUD display
#define OAPI_LKEY_HUDTarget         75 ///< query target object for HUD display
#define OAPI_LKEY_HUDColour         76 ///< switch through HUD colours
#define OAPI_LKEY_IncSimSpeed       77 ///< increase simulation speed x10
#define OAPI_LKEY_DecSimSpeed       78 ///< decrease simulation speed x0.1
#define OAPI_LKEY_IncFOV            79 ///< increment field of view
#define OAPI_LKEY_DecFOV            80 ///< decrement field of view
#define OAPI_LKEY_StepIncFOV        81 ///< increment field of view by 10 deg
#define OAPI_LKEY_StepDecFOV        82 ///< decrement field of view by 10 deg
#define OAPI_LKEY_MainMenu          83 ///< open main menu
#define OAPI_LKEY_DlgHelp           84 ///< open help dialog
#define OAPI_LKEY_DlgCamera         85 ///< open camera dialog
#define OAPI_LKEY_DlgSimspeed       86 ///< open simulation speed dialog
#define OAPI_LKEY_DlgCustomCmd      87 ///< open custom command dialog
#define OAPI_LKEY_DlgVisHelper      88 ///< open visual helper dialog
#define OAPI_LKEY_DlgRecorder       89 ///< open flight recorder dialog
#define OAPI_LKEY_DlgInfo           90 ///< open object info dialog
#define OAPI_LKEY_DlgMap            91 ///< open map dialog
#define OAPI_LKEY_ToggleCamInternal 92 ///< switch between cockpit and external camera
#define OAPI_LKEY_ToggleTrackMode   93 ///< switch between track camera modes
#define OAPI_LKEY_TogglePanelMode   94 ///< switch between cockpit modes
#define OAPI_LKEY_TogglePlanetarium 95 ///< toggle celestial marker display on/off
#define OAPI_LKEY_ToggleLabels      96 ///< toggle object and surface markers on/off
#define OAPI_LKEY_ToggleRecPlay     97 ///< toggle flight recorder/playback on/off
#define OAPI_LKEY_Pause             98 ///< toggle simulation pause on/off
#define OAPI_LKEY_Quicksave         99 ///< quick-save current simulation state
#define OAPI_LKEY_Quit             100 ///< quit simulation session
#define OAPI_LKEY_DlgSelectVessel  101 ///< open vessel selection dialog
#define OAPI_LKEY_SelectPrevVessel 102 ///< switch focus to previous vessel
#define OAPI_LKEY_DlgCapture       103 ///< open screen capture dialog
#define OAPI_LKEY_DlgOptions       104 ///< open options dialog
#define LKEY_COUNT 105                 ///< number of logical key definitions
//@}

// ======================================================================
// Some helper functions
// ======================================================================

/**
 * \ingroup vec
 * \brief Vector composition
 *
 * Returns a vector composed of the three provided arguments
 * \param x x-component
 * \param y y-component
 * \param z z-component
 * \return vector defined as (x,y,z)
 */
inline VECTOR3 _V(double x, double y, double z)
{
	VECTOR3 vec = {x,y,z}; return vec;
}

/**
 * \ingroup vec
 * \brief Vector copy
 *
 * Copies the element values from the source to the target vector.
 * \param[out] a target vector
 * \param[in] b source vector
 */
inline void veccpy (VECTOR3 &a, const VECTOR3 &b)
{
	a.x = b.x;
	a.y = b.y;
	a.z = b.z;
}

/**
 * \ingroup vec
 * \brief Vector addition
 * \param a first vector operand
 * \param b second vector operand
 * \return Result of a+b.
 */
inline VECTOR3 operator+ (const VECTOR3 &a, const VECTOR3 &b)
{
	VECTOR3 c;
	c.x = a.x+b.x;
	c.y = a.y+b.y;
	c.z = a.z+b.z;
	return c;
}

/**
 * \ingroup vec
 * \brief Vector subtraction
 * \param a first vector operand
 * \param b second vector operand
 * \return Result of a-b.
 */
inline VECTOR3 operator- (const VECTOR3 &a, const VECTOR3 &b)
{
	VECTOR3 c;
	c.x = a.x-b.x;
	c.y = a.y-b.y;
	c.z = a.z-b.z;
	return c;
}

/**
 * \ingroup vec
 * \brief Multiplication of vector with scalar
 * \param a vector operand
 * \param f scalar operand
 * \return Result of element-wise a*f.
 */
inline VECTOR3 operator* (const VECTOR3 &a, const double f)
{
	VECTOR3 c;
	c.x = a.x*f;
	c.y = a.y*f;
	c.z = a.z*f;
	return c;
}

/**
 * \ingroup vec
 * \brief Division of vector by a scalar
 * \param a vector operand
 * \param f scalar operand
 * \return Result of element-wise a/f.
 */
inline VECTOR3 operator/ (const VECTOR3 &a, const double f)
{
	VECTOR3 c;
	c.x = a.x/f;
	c.y = a.y/f;
	c.z = a.z/f;
	return c;
}

/**
 * \ingroup vec
 * \brief Vector addition-assignment a += b
 * \param[in,out] a Left-hand vector operand
 * \param[in] b Right-hand vector operand
 * \return Replaces a with a+b and returns the result.
 */
inline VECTOR3 &operator+= (VECTOR3 &a, const VECTOR3 &b)
{
	a.x += b.x;
	a.y += b.y;
	a.z += b.z;
	return a;
}

/**
 * \ingroup vec
 * \brief Vector subtraction-assignment a -= b
 * \param[in,out] a Left-hand vector operand
 * \param[in] b Right-hand vector operand
 * \return Replaces a with a-b and returns the result.
 */
inline VECTOR3 &operator-= (VECTOR3 &a, const VECTOR3 &b)
{
	a.x -= b.x;
	a.y -= b.y;
	a.z -= b.z;
	return a;
}

/**
 * \ingroup vec
 * \brief Vector-scalar multiplication-assignment a *= f
 * \param[in,out] a Left-hand vector operand
 * \param[in] f Right hand scalar operand
 * \return Replaces a with element-wise a*f and returns the result.
 */
inline VECTOR3 &operator*= (VECTOR3 &a, const double f)
{
	a.x *= f;
	a.y *= f;
	a.z *= f;
	return a;
}

/**
 * \ingroup vec
 * \brief Vector-scalar division-assignment a /= f
 * \param[in,out] a Left-hand vector operand
 * \param[in] f Right-hand scalar operand
 * \return Replaces a with element-wise a/f and returns the result.
 */
inline VECTOR3 &operator/= (VECTOR3 &a, const double f)
{
	a.x /= f;
	a.y /= f;
	a.z /= f;
	return a;
}

/**
 * \ingroup vec
 * \brief Vector unary minus -a
 * \param[in] a Vector operand
 * \return Negative vector (-a.x, -a.y, -a.z)
 */
inline VECTOR3 operator- (const VECTOR3 &a)
{
	VECTOR3 c;
	c.x = -a.x;
	c.y = -a.y;
	c.z = -a.z;
	return c;
}

/**
 * \ingroup vec
 * \brief Scalar (inner, dot) product of two vectors
 * \param[in] a First vector operand
 * \param[in] b Second vector operand
 * \return Scalar product <b>ab</b>
 */
inline double dotp (const VECTOR3 &a, const VECTOR3 &b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

/**
 * \ingroup vec
 * \brief Vector (cross) product of two vectors
 * \param[in] a First vector operand
 * \param[in] b Second vector operand
 * \return Vector product <b>a</b>x<b>b</b>
 */
inline VECTOR3 crossp (const VECTOR3 &a, const VECTOR3 &b)
{
	return _V(a.y*b.z - b.y*a.z, a.z*b.x - b.z*a.x, a.x*b.y - b.x*a.y);
}

/**
 * \ingroup vec
 * \brief Length (L2-norm) of a vector
 * \param a Vector operand
 * \return Vector norm |<b>a</b>|<sub>2</sub>
 */
inline double length (const VECTOR3 &a)
{
	return sqrt (a.x*a.x + a.y*a.y + a.z*a.z);
}

/**
 * \ingroup vec
 * \brief Length squared of a vector
 * \param a Vector operand
 * \return Vector norm |<b>a</b>|<sub>2</sub><sup>2</sup>
 */
inline double length2 (const VECTOR3 &a)
{
	return (a.x*a.x + a.y*a.y + a.z*a.z);
}

/**
 * \ingroup vec
 * \brief Distance between two points
 * \param[in] a First point
 * \param[in] b Second point
 * \return Distance between a and b
 */
inline double dist (const VECTOR3 &a, const VECTOR3 &b)
{
	return length (a-b);
}

/**
 * \ingroup vec
 * \brief Normalise a vector
 *
 * Resizes the argument vector to length 1.
 * \param[in,out] a Vector argument
 * \note The length of a must be greater than 0.
 */
inline void normalise (VECTOR3 &a)
{
	a /= length(a);
}

/**
 * \ingroup vec
 * \brief Returns normalised vector
 *
 * Returns a vector of length 1 with the same direction
 * as the argument vector.
 * \param[in] a Vector argument
 * \return Normalised vector.
 * \note The length of a must be greater than 0.
 */
inline VECTOR3 unit (const VECTOR3 &a)
{
	return a / length(a);
}

/**
 * \ingroup vec
 * \brief Matrix composition
 *
 * Returns a matrix composed of the provided elements.
 * \return
 * \f$
 *  \left(\begin{array}{ccc}
 *  m_{11} & m_{12} & m_{13} \\
 *  m_{21} & m_{22} & m_{23} \\
 *  m_{31} & m_{32} & m_{33}
 *  \end{array}\right)
 * \f$
 */
inline MATRIX3 _M(double m11, double m12, double m13,
				  double m21, double m22, double m23,
				  double m31, double m32, double m33)
{
	MATRIX3 mat = {m11,m12,m13,  m21,m22,m23,  m31,m32,m33};
	return mat;
}

/**
 * \ingroup vec
 * \brief Returns the identity matrix
 */
inline MATRIX3 identity ()
{
	static MATRIX3 mat = {1,0,0, 0,1,0, 0,0,1};
	return mat;
}

/**
 * \ingroup vec
 * \brief Return transpose of a matrix
 */
inline MATRIX3 transp(const MATRIX3& M)
{
	return _M(M.m11, M.m21, M.m31,
		      M.m12, M.m22, M.m32,
		      M.m13, M.m23, M.m33);
}

/**
 * \ingroup vec
 * \brief Outer product of two vectors
 * \param[in] a First vector operand
 * \param[in] b Second vector operand
 * \return Outer product <b>a</b><b>b</b><sup>T</sup>, where
 * <b>a</b> and <b>b</b> represent column vectors.
 */
inline MATRIX3 outerp (const VECTOR3 &a, const VECTOR3 &b)
{
	return _M(a.x*b.x, a.x*b.y, a.x*b.z,
		      a.y*b.x, a.y*b.y, a.y*b.z,
			  a.z*b.x, a.z*b.y, a.z*b.z);
}

/**
 * \ingroup vec
 * \brief Sum of matrix and scalar.
 * \param[in] A Matrix operand (left)
 * \param[in] s scalar operand (right)
 * \return A+s (element-wise sum of A and s)
 */
inline MATRIX3 operator+ (const MATRIX3 &A, double s)
{
	MATRIX3 mat = {A.m11+s, A.m12+s, A.m13+s,
		           A.m21+s, A.m22+s, A.m23+s,
				   A.m31+s, A.m32+s, A.m33+s};
	return mat;
}

/**
 * \ingroup vec
 * \brief Difference of matrix and scalar.
 * \param[in] A Matrix operand (left)
 * \param[in] s scalar operand (right)
 * \return A-s (element-wise difference of A and s)
 */
inline MATRIX3 operator- (const MATRIX3 &A, double s)
{
	MATRIX3 mat = {A.m11-s, A.m12-s, A.m13-s,
		           A.m21-s, A.m22-s, A.m23-s,
				   A.m31-s, A.m32-s, A.m33-s};
	return mat;
}

/**
 * \ingroup vec
 * \brief Product of matrix and scalar.
 * \param[in] A Matrix operand (left)
 * \param[in] s scalar operand (right)
 * \return A*s (element-wise product of A and s)
 */
inline MATRIX3 operator* (const MATRIX3 &A, double s)
{
	MATRIX3 mat = {A.m11*s, A.m12*s, A.m13*s,
		           A.m21*s, A.m22*s, A.m23*s,
				   A.m31*s, A.m32*s, A.m33*s};
	return mat;
}

/**
 * \ingroup vec
 * \brief Quotient of matrix and scalar.
 * \param[in] A Matrix operand (left)
 * \param[in] s scalar operand (right)
 * \return A/s (element-wise quotient of A and s)
 * \note s != 0 is required.
 */
inline MATRIX3 operator/ (const MATRIX3 &A, double s)
{
	MATRIX3 mat = {A.m11/s, A.m12/s, A.m13/s,
		           A.m21/s, A.m22/s, A.m23/s,
				   A.m31/s, A.m32/s, A.m33/s};
	return mat;
}

/**
 * \ingroup vec
 * \brief Matrix-scalar product-assignment A *= s
 * \param[in] A Matrix operand (left)
 * \param[in] s scalar operand (right)
 * \return Replaces A with element-wise product A*s and returns the result.
 */
inline MATRIX3 &operator*= (MATRIX3 &A, double s)
{
	for (int i = 0; i < 9; i++) A.data[i] *= s;
	return A;
}

/**
 * \ingroup vec
 * \brief Matrix-scalar division-assignment A /= s
 * \param[in] A Matrix operand (left)
 * \param[in] s scalar operand (right)
 * \return Replaces A with element-wise quotient A/s and returns the result.
 * \note s != 0 is required.
 */
inline MATRIX3 &operator/= (MATRIX3 &A, double s)
{
	for (int i = 0; i < 9; i++) A.data[i] /= s;
	return A;
}

/**
 * \ingroup vec
 * \brief Matrix-vector multiplication
 * \param[in] A matrix operand
 * \param[in] b vector operand
 * \return Result of <b>Ab</b>
 */
inline VECTOR3 mul (const MATRIX3 &A, const VECTOR3 &b)
{
	return _V (
		A.m11*b.x + A.m12*b.y + A.m13*b.z,
		A.m21*b.x + A.m22*b.y + A.m23*b.z,
		A.m31*b.x + A.m32*b.y + A.m33*b.z);
}

/**
 * \ingroup vec
 * \brief Matrix transpose-vector multiplication
 * \param[in] A matrix operand
 * \param[in] b vector operand
 * \return Result of <b>A</b><sup>T</sup><b>b</b>
 */
inline VECTOR3 tmul (const MATRIX3 &A, const VECTOR3 &b)
{
	return _V (
		A.m11*b.x + A.m21*b.y + A.m31*b.z,
		A.m12*b.x + A.m22*b.y + A.m32*b.z,
		A.m13*b.x + A.m23*b.y + A.m33*b.z);
}

/**
 * \ingroup vec
 * \brief Matrix-matrix multiplication
 * \param[in] A First matrix operand
 * \param[in] B Second matrix operand
 * \return Result of <b>AB</b>
 */
inline MATRIX3 mul (const MATRIX3 &A, const MATRIX3 &B)
{
	MATRIX3 mat = {
		A.m11*B.m11 + A.m12*B.m21 + A.m13*B.m31, A.m11*B.m12 + A.m12*B.m22 + A.m13*B.m32, A.m11*B.m13 + A.m12*B.m23 + A.m13*B.m33,
		A.m21*B.m11 + A.m22*B.m21 + A.m23*B.m31, A.m21*B.m12 + A.m22*B.m22 + A.m23*B.m32, A.m21*B.m13 + A.m22*B.m23 + A.m23*B.m33,
		A.m31*B.m11 + A.m32*B.m21 + A.m33*B.m31, A.m31*B.m12 + A.m32*B.m22 + A.m33*B.m32, A.m31*B.m13 + A.m32*B.m23 + A.m33*B.m33
	};
	return mat;
}

/**
 * \ingroup vec
 * \brief Construct a rotation matrix from an axis and an angle
 * \param axis rotation axis direction (must be normalised)
 * \param angle rotation angle [rad]
 * \return rotation matrix
 */
inline MATRIX3 rotm (const VECTOR3 &axis, double angle)
{
	double c = cos(angle), s = sin(angle);
	double t = 1-c, x = axis.x, y = axis.y, z = axis.z;

	return _M(t*x*x+c, t*x*y-z*s, t*x*z+y*s,
		      t*x*y+z*s, t*y*y+c, t*y*z-x*s,
			  t*x*z-y*s, t*y*z+x*s, t*z*z+c);
}

inline VECTOR4 _V(double x, double y, double z, double w)
{
	VECTOR4 vec = {x,y,z,w}; return vec;
}

inline VECTOR4 _V(const VECTOR3 &v)
{
	VECTOR4 vec = {v.x, v.y, v.z, 1.0};
	return vec;
}

inline VECTOR4 mul (const MATRIX4 &A, const VECTOR4 &b)
{
	return _V (
		A.m11*b.x + A.m12*b.y + A.m13*b.z + A.m14*b.w,
		A.m21*b.x + A.m22*b.y + A.m23*b.z + A.m24*b.w,
		A.m31*b.x + A.m32*b.y + A.m33*b.z + A.m34*b.w,
		A.m41*b.x + A.m42*b.y + A.m43*b.z + A.m44*b.w);
}

inline VECTOR4 mul (const VECTOR4 &b, const MATRIX4 &A)
{
	return _V (
		b.x*A.m11 + b.y*A.m21 + b.z*A.m31 + b.w*A.m41,
		b.x*A.m12 + b.y*A.m22 + b.z*A.m32 + b.w*A.m42,
		b.x*A.m13 + b.y*A.m23 + b.z*A.m33 + b.w*A.m43,
		b.x*A.m14 + b.y*A.m24 + b.z*A.m34 + b.w*A.m44);
}

inline MATRIX4 _M(double m11, double m12, double m13, double m14,
				  double m21, double m22, double m23, double m24,
				  double m31, double m32, double m33, double m34,
				  double m41, double m42, double m43, double m44)
{
	MATRIX4 mat = {m11,m12,m13,m14,  m21,m22,m23,m24,  m31,m32,m33,m34, m41,m42,m43,m44};
	return mat;
}

/**
 * \ingroup vec
 * \brief Returns the identity matrix
 */
inline MATRIX4 identity4 ()
{
	static MATRIX4 mat = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
	return mat;
}

/**
 * \ingroup vec
 * \brief Matrix-matrix multiplication for 4-matrices
 * \param[in] A First matrix operand
 * \param[in] B Second matrix operand
 * \return Result of <b>AB</b>
 */
inline MATRIX4 mul (const MATRIX4 &A, const MATRIX4 &B)
{
	MATRIX4 mat = {
		A.m11*B.m11 + A.m12*B.m21 + A.m13*B.m31 + A.m14*B.m41, A.m11*B.m12 + A.m12*B.m22 + A.m13*B.m32 + A.m14*B.m42, A.m11*B.m13 + A.m12*B.m23 + A.m13*B.m33 + A.m14*B.m43, A.m11*B.m14 + A.m12*B.m24 + A.m13*B.m34 + A.m14*B.m44,
		A.m21*B.m11 + A.m22*B.m21 + A.m23*B.m31 + A.m24*B.m41, A.m21*B.m12 + A.m22*B.m22 + A.m23*B.m32 + A.m24*B.m42, A.m21*B.m13 + A.m22*B.m23 + A.m23*B.m33 + A.m24*B.m43, A.m21*B.m14 + A.m22*B.m24 + A.m23*B.m34 + A.m24*B.m44,
		A.m31*B.m11 + A.m32*B.m21 + A.m33*B.m31 + A.m34*B.m41, A.m31*B.m12 + A.m32*B.m22 + A.m33*B.m32 + A.m34*B.m42, A.m31*B.m13 + A.m32*B.m23 + A.m33*B.m33 + A.m34*B.m43, A.m31*B.m14 + A.m32*B.m24 + A.m33*B.m34 + A.m34*B.m44,
		A.m41*B.m11 + A.m42*B.m21 + A.m43*B.m31 + A.m44*B.m41, A.m41*B.m12 + A.m42*B.m22 + A.m43*B.m32 + A.m44*B.m42, A.m41*B.m13 + A.m42*B.m23 + A.m43*B.m33 + A.m44*B.m43, A.m41*B.m14 + A.m42*B.m24 + A.m43*B.m34 + A.m44*B.m44
	};
	return mat;
}

inline RECT _R (int left, int top, int right, int bottom)
{
	RECT r = { left, top, right, bottom }; return r;
}

inline VECTOR3 POINTERTOREF (VECTOR3 *p)
{
	VECTOR3 v;
	v.x = DBL_MAX;            // flag
	*((VECTOR3**)&v.z) = p;   // address
	v.z = 0.0;
	return v;
}

// ======================================================================
// Internal data structures
// ======================================================================

#ifdef ORBITER_MODULE
void dummy();
void calldummy () { dummy(); }
DLLCLBK char *ModuleDate () { return (char*)__DATE__; }
#endif

#endif // !__ORBITERAPI_H
// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// =============================================================
// Config.h
// Interface for class Config
// Load configuration settings for Orbiter
// =============================================================

#ifndef __CONFIG_H
#define __CONFIG_H

//#include <d3d.h>
#include <windows.h>
#include "Vecmat.h"
#include <iostream>
#include <fstream>
#include <list>
#include "GraphicsAPI.h"

// dynamic state propagation methods
#define MAX_PROP_LEVEL  5
#define MAX_APROP_LEVEL 5
#define NPROP_METHOD   10
#define NAPROP_METHOD   6
#define PROP_RK2        0
#define PROP_RK4        1
#define PROP_RK5        2
#define PROP_RK6        3
#define PROP_RK7        4
#define PROP_RK8        5
#define PROP_SY2        6
#define PROP_SY4        7
#define PROP_SY6        8
#define PROP_SY8        9

#define SURF_MAX_PATCHLEVEL 14
#define SURF_MAX_PATCHLEVEL2 21

class Mesh;
class PlanetarySystem;
class Camera;
class Pane;

typedef struct {
	enum {NONE, SERVER, CLIENT} role;
	unsigned short port;
	char server_name[256];
} NetParam;

struct CFG_DIRPRM {
	char   ConfigDir[256];		// location of config files
	char   MeshDir[256];		// location of mesh files
	char   TextureDir[256];		// location of texture files
	char   HightexDir[256];		// location of highres planet textures
	char   PlanetTexDir[256];   // location for planetary textures (version-2 textures)
	char   ScnDir[256];			// location of scenario files
};

struct CFG_PHYSICSPRM {
	bool   bDistributedMass;	// vessels can define 2-point mass distribution
	bool   bNonsphericalGrav;	// take into account nonspherical planet shapes for gravity calculations
	bool   bRadiationPressure;	// take into account radiation pressure effects
	bool   bAtmWind;            // nonzero wind speeds
	bool   bOrbitStabilise;		// use Encke orbit stabilisation at high time accelerations
	double Stabilise_PLimit;	// perturbation limit for stabilisation
	double Stabilise_SLimit;	// step size limit for stabilisation
	double PPropSubLimit;		// orbit step target for perturbation subsampling
	int    PPropSubMax;			// max number of subsampling steps (perturbation integration)
	double PPropStepLimit;		// orbit step limit for nonspherical gravity suppression
	int    nLPropLevel;			// number of linear state propagation levels defined
	int    PropMode[MAX_PROP_LEVEL];	// propagation mode indices
	double PropTTgt[MAX_PROP_LEVEL];    // time step targets for the propagation levels
	double PropATgt[MAX_PROP_LEVEL];    // angle step targets for the propagation levels
	double PropTLim[MAX_PROP_LEVEL];	// time step limits for the propagation levels
	double PropALim[MAX_PROP_LEVEL];  // angle step limits for the propagation levels
	double APropSubLimit;		// angle step limit for subsampling
	int    PropSubMax;			// max number of subsampling steps
	double APropCouplingLimit;	// angle step limit for cross term suppresion
	double APropTorqueLimit;	// angle step limit for torque suppression
};

struct CFG_LOGICPRM {
	bool   bStartPaused;		// pause at simulation start
	int    FlightModelLevel;	// 0=simple, 1=realistic
	int    DamageSetting;		// 0=no damage model, 1=allow damage
	bool   bLimitedFuel;		// limited fuel for spacecraft?
	bool   bPadRefuel;			// auto-refuel when landed on pad?
	bool   bMfdTransparent;		// make MFD display transparent
	bool   bGlasspitCompact;    // compact glass cockpit layout for widescreen formats
	int    MFDSize;				// MFD size parameter in generic view
	int    MFDMapVersion;		// 0=legacy style, 1=new style
	double InstrUpdDT;			// instrument update interval [s]
	double PanelScale;			// old-style 2D instrument panel scale
	double PanelScrollSpeed;	// speed for panel panning [pixel/sec]
};

struct CFG_VISUALPRM {
	bool   bShadows;			// render shadows for surface base objects?
	bool   bVesselShadows;		// render ground shadows for vessels?
	bool   bClouds;				// render cloud layers?
	bool   bCloudShadows;		// render cloud shadows on the ground?
	bool   bNightlights;		// render planetary night light textures?
	bool   bWaterreflect;		// reflecting planetary water surfaces?
	bool   bSpecularRipple;		// ripples on specular water reflections?
	bool   bHaze;				// render glowing haze layer at horizion?
	bool   bFog;				// enable distance fog?
	bool   bSpecular;			// enable specular reflection effects?
	bool   bReentryFlames;		// render reentry flames?
	bool   bParticleStreams;	// render particle streams? (exhaust, contrails, etc.)
	bool   bLocalLight;			// enable local light sources?
	DWORD  MaxLight;			// max number of light sources
	DWORD  AmbientLevel;		// ambient light level (0-255)
	DWORD  PlanetMaxLevel;		// max. planet patch resolution level
	double PlanetPatchRes;		// resolution scaling for planet patches
	double LightBrightness;		// brightness of planetary night lights
	bool   bUseStarDots;        // render stars as pixels?
	StarRenderPrm StarPrm;		// render parameters for background stars
	bool   bUseStarImage;       // render stars as a background image?
	char   StarImagePath[128];  // starlist image path
	bool   bUseBgImage;         // render celestial sphere background image?
	char   CSphereBgPath[128];	// background image path
	double CSphereBgIntens;		// intensity of background image
	int    ElevMode;            // elevation mode: 0=none, 1=linear, 2=cubic spline
};

struct CFG_CAPTUREPRM {
	int    ImageTgt;            // 0=clipboard, 1=file
	char   ImageFile[128];      // image output file name
	char   SequenceDir[128];    // directory for frame sequence
	int    ImageFormat;         // image format id (0=BMP, 1=PNG, 2=JPG, 3=TIF)
	int    ImageQuality;        // image quality if supported (1-10)
	int    SequenceStart;       // starting frame of sequence
	int    SequenceSkip;        // number of frames to skip
};

struct CFG_INSTRUMENTPRM {
	int  bMfdPow2;              // force MFD display textures to size power of 2? (0=no, 1=yes, 2=auto)
	int  MfdHiresThreshold;     // if bMfdPow2==true, this is the size above which glass cockpit MFD textures switch from 256 to 512
	int  PanelMFDHUDSize;       // 256 or 512
	int  VCMFDSize;             // MFD texture size for virtual cockpits (256/512/1024)
};

struct CFG_VISHELPPRM {
	DWORD  flagPlanetarium;		// bitflags for items to be displayed in planetarium mode
		// bit 0: enable planetarium mode         bit 5: constellation patterns
		// bit 1: celestial grid                  bit 6: constellation labels
		// bit 2: ecliptic grid                   bit 7: long constellation names
		// bit 3: galactic grid                   bit 8: constellation boundaries
		// bit 4: equator of current target       bit 9: celestial sphere feature markers
	DWORD  flagMarkers;         // bitflags for surface and object marker display
	    // bit 0: enable markers                  bit 3: surface bases
	    // bit 1: solar system bodies             bit 4: VOR transmitters
	    // bit 2: vessels                         bit 5: surface features
	DWORD  flagBodyForce;		// body force vector display
	float  scaleBodyForce;		// force vector scaling factor
	float  opacBodyForce;		// force vector opacity factor
	DWORD  flagFrameAxes;		// frame axes vector display
	float  scaleFrameAxes;		// frame axes scaling factor
	float  opacFrameAxes;		// frame axes opacity factor
};

struct CFG_DEBUGPRM {
	int    ShutdownMode;		// 0=standard (redisplay launchpad), 1=respawn, 2=terminate
	double FixedStep;			// fixed time step length [s] (0=variable)
	int    TimerMode;			// timer mode (0=auto, 1=hires hardware, 2=lores software
	bool   bDisableSmoothFont;  // disable font smoothing for better performance
	bool   bForceReenableSmoothFont; // enable font smoothing on exit, even if it wasn't on originally (recover from losing settings after a previous crash)
	int    bHtmlScnDesc;        // 0=use simple text box, 1=use inline html viewer, 2=auto-detect (disable inline html under linux/wine)
	bool   bSaveExitScreen;     // save screenshot on scenario exit
	bool   bWireframeMode;      // set renderer to wireframe mode?
	bool   bNormaliseNormals;   // force auto-normalisation of all normals?
	bool   bVerboseLog;         // verbose log output?
};

struct CFG_PLANETRENDERPRM {
	int    PreloadMode;         // preload tiles?
	int    CacheSize;           // number of tiles to cache
	bool   bLoadOnThread;       // load tiles in separate thread
	int    MipmapMode;
	double MipmapBias;
	double ResolutionBias;      // -2..+2: higher values switch to higher res patches earlier
	int    PatchRes;            // power n of surface patch mesh resolution 2<<n [n=4,5]
	int    LoadFrequency;       // tile load frequency
	int    AnisoMode;
	DWORD  TileLoadFlags;       // flags for planetary tile load mechanism
};

struct CFG_MAPPRM {
	DWORD DispFlag;
};

struct CFG_RECPLAYPRM {
	int    RecordPosFrame;		// recorder position/velocity data frame (0=ecl, 1=equ)
	int    RecordAttFrame;		// recorder attitude data frame (0=ecl., 1=local hor.)
	bool   bRecordWarp;			// write time acceleration info to recording?
	bool   bRecordFocus;		// write focus vessel events to recording?
	bool   bReplayWarp;			// use recorded acceleration data during playback?
	bool   bReplayFocus;		// use recorded focus vessel events during playback?
	bool   bReplayCam;			// use recorded camera events during playback?
	bool   bSysInterval;		// sample in system time intervals?
	bool   bShowNotes;			// show inflight notes during playback?
};

struct CFG_DEVPRM {
	int    Device_idx;			// index of default device
	DWORD  Device_mode;			// index of default fullscreen mode
	DWORD  Device_out;			// device output
	DWORD  Device_style;		// Rendering layout
	bool   bForceEnum;			// force enumeration, bypass device.dat
	bool   bFullscreen;			// use window mode
	bool   bStereo;				// use stereo mode
	bool   bNoVsync;			// no vertical sync (fullscreen only)
	bool   bTryStencil;			// try stencil buffers when available
	bool   bPageflip;			// allow page flipping in fullscreen mode (disabling can fix flicker problem)
	DWORD  WinW;				// window width (pixel) for windowed mode
	DWORD  WinH;				// window height (pixel) for windowed mode
};

struct CFG_JOYSTICKPRM {
	DWORD  Joy_idx;				// joystick device index (0=disabled)
	DWORD  Deadzone;			// central deadzone range for all axes (0-10000)
	DWORD  ThrottleAxis;		// joystick throttle axis (0=none, 1=z-axis, 2=slider 0, 3=slider 1)
	DWORD  ThrottleSaturation;	// saturation level for joystick throttle control (0-10000)
	bool   bThrottleIgnore;		// ignore joystick throttle setting on start
};

struct CFG_UIPRM {              // user interface options
	DWORD  MouseFocusMode;	    // 0: focus requires click; 1: focus requires click for child windows only; 2: focus follow mouse
	DWORD  MenuMode;            // 0=show, 1=hide, 2=auto-hide
	bool   bMenuLabelOnly;      // display only menu labels?
	bool   bWarpAlways;         // always display time acceleration != 1
	bool   bWarpScientific;     // display time acceleration in scientific notation?
	DWORD  InfoMode;            // 0=show, 1=hide, 2=auto-hide
	DWORD  InfoAuxIdx[2];       // index for auxiliary info bars left/right (0=none)
	DWORD  MenuOpacity;         // menubar opacity (0-10)
	DWORD  InfoOpacity;         // infobar opacity (0-20)
	DWORD  MenuScrollspeed;     // menubar scroll speed (1-20)
	DWORD  PauseIndMode;        // 0=flash on pause/resume, 1=show on pause, 2=don't show
	int    SelVesselTab;        // tab to open in vessel selection dialog
 	int    SelVesselRange;      // "nearby" range for vessel selection dialog
	bool   bSelVesselFlat;      // flat assemblies for vessel selection dialog
};

struct CFG_DEMOPRM {
	bool   bDemo;				// run in demo mode?
	bool   bBkImage;			// show background image?
	bool   bBlockExit;			// prevent from terminating orbiter?
	double MaxDemoTime;			// maximum demo run time [s]
	double LPIdleTime;			// maximum launchpad idle time [s]
};

struct CFG_FONTPRM {
	float  dlgFont_Scale;		// font scaling factor
	char   dlgFont1_Face[64];	// dialog font face name
	float  ImGui_FontSize;		// Font size for ImGui dialogs
	char   ImGui_FontFile[256];	// Font file for ImGui default font
};

struct CFG_CAMERAPRM {
	double Panspeed;			// camera panning speed (ground mode)
	double TerrainLimit;        // altitude limit for terrain-following mode [m]
	int    HUDCol;				// HUD colour index
};

struct CFG_WINDOWPOS {
	RECT DlgMap;                // map window position
	RECT DlgInfo;               // info window position
	RECT DlgCamera;             // camera dialog position
	RECT DlgFocus;				// focus dialog position
	RECT DlgTacc;               // time acceleration dialog position
	RECT DlgOptions;            // options dialog position
	RECT DlgVishelper;          // visual helper dialog position
	int LaunchpadScnListWidth;  // width of Launchpad scenario list
	int LaunchpadModListWidth;  // width of Launchpad modules list
	int LaunchpadExtListWidth;  // width of Launchpad extras list
};

struct CFG_CMDLINEPRM {
	bool   bFastExit;           // Terminate Orbiter at session end?
	bool   bOpenVideoTab;       // Open Launchpad on video tab?
	bool   bAppendLog;          // Orbiter log: append instead of overwrite?
	size_t FrameLimit;          // max number of timeframes before session termination (0 = unlimited)
	double FixedStep;           // fixed time step length (0 = disabled). If != 0, overrides CFG_DEBUGPRM::FixedStep
	double MaxSysTime;          // Max session runtime (sys time). 0 = unlimited
	double MaxSimTime;          // Max session runtime (sim time). 0 = unlimited
	std::string LaunchScenario; // if not empty, start scenario instantly without opening Launchpad
	std::list<std::string> LoadPlugins; // list of plugins to load
};

// =============================================================

char *trim_string (char *cbuf);
// Cut off comments (starting with ';') and trailing white space
// (' ', '\t') from cbuf. Return first character in cbuf after
// leading white space.
// cbuf is modified by this function

char *readline (std::istream &is);
// Reads a line from a stream and returns a pointer to a static
// buffer containing the line. The buffer is grown dynamically to
// hold a string of arbitrary length.

bool GetItemString (std::istream &is, const char *label, char *val);
bool GetItemReal   (std::istream &is, const char *label, double &val);
bool GetItemInt    (std::istream &is, const char *label, int &val);
bool GetItemSize   (std::istream& is, const char* label, size_t& val);
bool GetItemHex    (std::istream &is, const char *label, int &val);
bool GetItemBool   (std::istream &is, const char *label, bool &val);
bool GetItemVector (std::istream &is, const char *label, Vector &val);
bool GetItemVECTOR (std::istream &is, const char *label, VECTOR3 &val);

bool FindLine      (std::istream &is, const char *line);
// scans stream 'is' from beginning for a line beginning with 'line' 
// and leaves file pointer on the beginning of the next line
// return value is false if line is not found

int ListIndex      (int listlen, char **list, char *label);
// returns index of entry 'label' in 'list' of length 'listlen',
// or -1 if entry does not exist. Comparison is case-insensitive

inline int StrComp (const char *str1, const char *str2, bool ignorecase)
{
	if (ignorecase) return _stricmp (str1, str2);
	else            return strcmp (str1, str2);
}

inline int StrNComp (const char *str1, const char *str2, int n, bool ignorecase)
{
	if (ignorecase) return _strnicmp (str1, str2, n);
	else            return strncmp (str1, str2, n);
}

// =============================================================

class Config {
	friend class GDIResources;

public:
	Config ();

	Config (char *fname);
	// construct from defaults in file "fname"

	~Config ();

	void SetDefaults ();
	void SetDefaults_Capture ();

	bool FoundFile() const { return found_config_file; }

	bool Load(const char* fname);

	BOOL Write (const char *fname = 0) const;
	// write config parameters to file "fname"
	// returns FALSE if write fails

	char *ConfigPath (const char *name) const;
	// Return full path for config file name (adds ".cfg" to name)
	char *ConfigPathNoext (const char *name);
	// Return full path for config file name (no file extension added)
	char *MeshPath   (const char *name);
	// Return full path for mesh file name
	char *TexPath    (const char *name, const char *ext = 0);
	// Return full path for texture file name. default extension is ".dds" - NOT THREADSAFE
	char *HTexPath   (const char *name, const char *ext = 0);
	// Return full path for texture file name in hightex dir.
	// Default extension is ".dds"
	// If hightex dir is not defined, function returns NULL
	char* PTexPath(const char* name, const char* ext = 0);
	// Return full path for planetary texture file name
	const char *ScnPath    (const char *name);
	// Return full path for scenario file name

	void TexPath (char *cbuf, const char *name, const char *ext=0);
	// fill cbuf with the complete path for file name.ext in the texture directory
	void PTexPath(char* cbuf, const char* name, const char* ext = 0);
	// fill cbuf with the complete path for file name.ext in the planetary texture directory

	bool bEchoAll;          // echo all configuration parameters (or only non-default ones)?
	RECT rLaunchpad;        // launchpad dialog position

	// directory information
	char *Root;             // master config file

	// visual parameters
	DWORD AmbientColour;         // RGBA value of ambient component

	CFG_DIRPRM CfgDirPrm;				// subdirectory list
	CFG_PHYSICSPRM CfgPhysicsPrm;		// physics engine parameters
	CFG_LOGICPRM CfgLogicPrm;			// logical parameters
	CFG_VISUALPRM CfgVisualPrm;			// visualisation parameters
	CFG_CAPTUREPRM CfgCapturePrm;       // screen capture parameters
	CFG_INSTRUMENTPRM CfgInstrumentPrm; // instrument parameters
	CFG_VISHELPPRM CfgVisHelpPrm;		// visual helper parameters
	CFG_PLANETRENDERPRM CfgPRenderPrm;	// planet render parameters
	CFG_MAPPRM CfgMapPrm;               // map dialog parameters
	CFG_DEVPRM CfgDevPrm;               // video device parameters
	CFG_JOYSTICKPRM CfgJoystickPrm;		// joystick parameters
	CFG_UIPRM CfgUIPrm;                 // user interface parameters
	CFG_DEMOPRM CfgDemoPrm;				// demo mode parameters
	CFG_RECPLAYPRM CfgRecPlayPrm;		// record/playback parameters
	CFG_DEBUGPRM CfgDebugPrm;			// debugging parameters (shutdown method, fixed time steps, timer interface)
	CFG_FONTPRM CfgFontPrm;				// ingame font characteristics
	CFG_CAMERAPRM CfgCameraPrm;			// camera parameters
	CFG_WINDOWPOS CfgWindowPos;         // subwindow positions
	CFG_CMDLINEPRM CfgCmdlinePrm;       // Populated by command line parameters. Overrides interactive settings

	/**
	 * \brief Returns a list of active plugin module names
	 */
	const std::list<std::string>& GetActiveModules() const { return m_activeModules; }

	/**
	 * \brief Checks if a module is in the active list.
	 * \param name Module name
	 * \return true if name is in the list, false otherwise
	 */
	bool IsActiveModule(const std::string& name);

	/**
	 * \brief Add a module to the active list.
	 */
	void AddActiveModule (const std::string& name);

	/**
	 * \brief Delete a module from the active list.
	 */
	void DelActiveModule (const std::string& name);

	inline void SetAmbientLevel (DWORD lvl)
	{ AmbientColour = (CfgVisualPrm.AmbientLevel = std::min (lvl, (DWORD)0xff)) * 0x01010101; }

	const void *GetParam (DWORD paramtype) const;
	// return a specific parameter setting (paramtype defined in GraphicsAPI.h)

	// Read items from master config
	bool GetString (const char *category, char *val);
	bool GetReal (const char *category, double &val);
	bool GetInt (const char *category, int &val);
	bool GetSize (const char* category, size_t& val);
	bool GetBool (const char *category, bool &val);
	bool GetVector (const char *category, Vector &val);

private:
	bool GetString (std::istream &is, const char *category, char *val);
	bool GetReal (std::istream &is, const char *category, double &val);
	bool GetInt (std::istream &is, const char *category, int &val);
	bool GetSize (std::istream& is, const char* category, size_t& val);
	bool GetBool (std::istream &is, const char *category, bool &val);
	bool GetVector (std::istream &is, const char *category, Vector &val);

	mutable char cfgpath[256];  // buffer for creating full path names
	char mshpath[256];
	char texpath[256];
	char htxpath[256];
	char ptxpath[256];
	char scnpath[256];
	int cfglen, mshlen, texlen, htxlen, ptxlen, scnlen; // string length
	bool found_config_file;

	std::list<std::string> m_activeModules; ///< list of active modules
};

// =============================================================

class GDIResources {
public:
	GDIResources (HWND hWnd, DWORD winW, DWORD winH, const Config &config);
	~GDIResources ();

	// ingame dialog fonts
	HFONT dlgF1r, dlgF1i; // standard dialog font (roman and italic) - can be variable pitch
	int dlgF1H, dlgF1W;   // line height, average character width

	HFONT dlgF2;          // fixed pitch dialog font
	int dlgF2H, dlgF2W;   // line height, character width
};

// =============================================================

// OBSOLETE!
#ifdef __CONFIG_CPP
GDIResources *g_gdires = 0;
#else
extern GDIResources *g_gdires;
#endif

#endif // !__CONFIG_H
// Copyright (c) 2024 TheGondos
// Licensed under the MIT License

#define INTERPRETER_IMPLEMENTATION

#include "Interpreter.h"
#include "XRSound.h"

#include "lua.h"

void Interpreter::LoadXRSoundAPI ()
{
	static const struct luaL_Reg xrsoundLib[] = {
		{"create_instance", xrsound_create_instance},
		{"is_present", xrsound_is_present},
		{"get_version", xrsound_get_version},
		{"load_wav", xrsound_load_wav},
		{"play_wav", xrsound_play_wav},
		{"stop_wav", xrsound_stop_wav},
		{"is_wavplaying", xrsound_is_wavplaying},
		{"set_paused", xrsound_set_paused},
		{"is_paused", xrsound_is_paused},
		{"set_defaultsoundenabled", xrsound_set_defaultsoundenabled},
		{"get_defaultsoundenabled", xrsound_get_defaultsoundenabled},
		{"set_defaultsoundgroupfolder", xrsound_set_defaultsoundgroupfolder},
		{"get_defaultsoundgroupfolder", xrsound_get_defaultsoundgroupfolder},
		{"set_pan", xrsound_set_pan},
		{"get_pan", xrsound_get_pan},
		{"set_playbackspeed", xrsound_set_playbackspeed},
		{"get_playbackspeed", xrsound_get_playbackspeed},
		{"set_playposition", xrsound_set_playposition},
		{"get_playposition", xrsound_get_playposition},
		{"__gc", xrsound_collect},
		{NULL, NULL}
	};

	luaL_newmetatable (L, "XRSound.vtable");
	lua_pushstring (L, "__index");
	lua_pushvalue (L, -2); // push metatable
	lua_settable (L, -3);  // metatable.__index = metatable
	luaL_setfuncs(L, xrsoundLib, 0);

	lua_createtable (L, 0, 7);
	lua_pushnumber (L, (int)XRSound::PlaybackType::InternalOnly); lua_setfield (L, -2, "InternalOnly");
	lua_pushnumber (L, (int)XRSound::PlaybackType::BothViewFar); lua_setfield (L, -2, "BothViewFar");
	lua_pushnumber (L, (int)XRSound::PlaybackType::BothViewMedium); lua_setfield (L, -2, "BothViewMedium");
	lua_pushnumber (L, (int)XRSound::PlaybackType::BothViewClose); lua_setfield (L, -2, "BothViewClose");
	lua_pushnumber (L, (int)XRSound::PlaybackType::Radio); lua_setfield (L, -2, "Radio");
	lua_pushnumber (L, (int)XRSound::PlaybackType::Wind); lua_setfield (L, -2, "Wind");
	lua_pushnumber (L, (int)XRSound::PlaybackType::Global); lua_setfield (L, -2, "Global");
	lua_setglobal (L, "PlaybackType");

	lua_createtable (L, 0, 51);
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::AirConditioning); lua_setfield (L, -2, "AirConditioning");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::LandedWind); lua_setfield (L, -2, "LandedWind");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::AudioGreeting); lua_setfield (L, -2, "AudioGreeting");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::MainEngines); lua_setfield (L, -2, "MainEngines");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::RetroEngines); lua_setfield (L, -2, "RetroEngines");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::HoverEngines); lua_setfield (L, -2, "HoverEngines");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::RCSSustain); lua_setfield (L, -2, "RCSSustain");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::SwitchOn); lua_setfield (L, -2, "SwitchOn");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::SwitchOff); lua_setfield (L, -2, "SwitchOff");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::Rotation); lua_setfield (L, -2, "Rotation");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::Translation); lua_setfield (L, -2, "Translation");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::Off); lua_setfield (L, -2, "Off");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::CustomEngines); lua_setfield (L, -2, "CustomEngines");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::AFPitch); lua_setfield (L, -2, "AFPitch");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::AFOn); lua_setfield (L, -2, "AFOn");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::AFOff); lua_setfield (L, -2, "AFOff");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::Crash); lua_setfield (L, -2, "Crash");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::MetalCrunch); lua_setfield (L, -2, "MetalCrunch");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::WheelChirp); lua_setfield (L, -2, "WheelChirp");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::Touchdown); lua_setfield (L, -2, "Touchdown");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::WheelStop); lua_setfield (L, -2, "WheelStop");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::TiresRolling); lua_setfield (L, -2, "TiresRolling");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::OneHundredKnots); lua_setfield (L, -2, "OneHundredKnots");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::Liftoff); lua_setfield (L, -2, "Liftoff");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::WarningGearIsUp); lua_setfield (L, -2, "WarningGearIsUp");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::YouAreClearedToLand); lua_setfield (L, -2, "YouAreClearedToLand");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::Docking); lua_setfield (L, -2, "Docking");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::DockingCallout); lua_setfield (L, -2, "DockingCallout");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::Undocking); lua_setfield (L, -2, "Undocking");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::UndockingCallout); lua_setfield (L, -2, "UndockingCallout");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::Wheekbrakes); lua_setfield (L, -2, "Wheekbrakes");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::DockingRadarBeep); lua_setfield (L, -2, "DockingRadarBeep");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::FlightWind); lua_setfield (L, -2, "FlightWind");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::ReentryPlasma); lua_setfield (L, -2, "ReentryPlasma");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::SonicBoom); lua_setfield (L, -2, "SonicBoom");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::AutopilotOn); lua_setfield (L, -2, "AutopilotOn");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::AutopilotOff); lua_setfield (L, -2, "AutopilotOff");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::SubsonicCallout); lua_setfield (L, -2, "SubsonicCallout");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::RCSAttackPlusX); lua_setfield (L, -2, "RCSAttackPlusX");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::RCSAttackPlusY); lua_setfield (L, -2, "RCSAttackPlusY");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::RCSAttackPlusZ); lua_setfield (L, -2, "RCSAttackPlusZ");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::RCSAttackMinusX); lua_setfield (L, -2, "RCSAttackMinusX");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::RCSAttackMinusY); lua_setfield (L, -2, "RCSAttackMinusY");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::RCSAttackMinusZ); lua_setfield (L, -2, "RCSAttackMinusZ");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::LastDefaultSound); lua_setfield (L, -2, "LastDefaultSound");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::RadioATCGroup); lua_setfield (L, -2, "RadioATCGroup");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::CabinAmbienceGroup); lua_setfield (L, -2, "CabinAmbienceGroup");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::MachCalloutsGroup); lua_setfield (L, -2, "MachCalloutsGroup");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::AltitudeCalloutsGroup); lua_setfield (L, -2, "AltitudeCalloutsGroup");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::DockingDistanceCalloutsGroup); lua_setfield (L, -2, "DockingDistanceCalloutsGroup");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::MusicFolder); lua_setfield (L, -2, "MusicFolder");
	lua_pushnumber (L, (int)XRSound::DefaultSoundID::LastDefaultGroup); lua_setfield (L, -2, "LastDefaultGroup");
	lua_setglobal (L, "DefaultSoundID");
}

int Interpreter::lua_isxrsound(lua_State *L, int idx)
{
	return luaL_tryudata(L, idx, "XRSound.vtable") != NULL;
}

XRSound *Interpreter::lua_toxrsound (lua_State *L, int idx)
{
	ASSERT_SYNTAX(lua_isxrsound(L, idx), "Argument is not an xrsound object");
	XRSound **pxrsound = (XRSound**)lua_touserdata(L,idx);
	return *pxrsound;
}

/***
XRSound interface functions
@module xrsound
*/
/// @lookup types

/***
Create an instance of an XRSound proxy object for the supplied vessel; invoke this from your vessel's clbk_postcreation method.
Normally you will only invoke this method from your own vessel's code; however, if you call this method from a *module*, 
you will be able to use the returned proxy object to control the sounds for the supplied vessel.

@function create_instance
@tparam handle vessel handle
@treturn xrsound XRSound proxy to use with the specified vessel
@usage
function clbk_postcreation()
	snd = xrsound.create_instance(vi)
end
*/
#ifndef XRSOUND
int Interpreter::xrsound_create_instance(lua_State *L)
{
	// XRSound module not build. Can't call CreateInstance()
	return luaL_error(L, "Application not build with XRSound support");
}

#else
int Interpreter::xrsound_create_instance (lua_State *L)
{
	XRSound *snd = NULL;
	if(lua_isstring(L, 1)) {
		const char *moduleName = lua_tostring(L, 1);
		snd = XRSound::CreateInstance(moduleName);
	} else if (VESSEL *v = lua_tovessel(L, 1)) {
		snd = XRSound::CreateInstance(v);
	} else {
		return luaL_error(L, "Invalid parameter to xrsound.create_instance, string or vessel handle needed");
	}

	XRSound **proxy = (XRSound **)lua_newuserdata(L, sizeof(XRSound *));
	*proxy = snd;
	luaL_getmetatable(L, "XRSound.vtable");
	lua_setmetatable(L, -2);

	return 1;
}
#endif // !XRSOUND

/***
xrsound class: Lua access to XRSound objects
@classmod xrsound
*/

/***
XRSound status.

Returns true if XRSound.dll is present, false if not.

@function is_present
@treturn boolean XRSound status
*/
int Interpreter::xrsound_is_present(lua_State *L)
{
	XRSound *snd = lua_toxrsound(L, 1);
	lua_pushboolean(L, snd->IsPresent());
	return 1;
}

/***
XRSound version.

Returns the version of XRSound.dll, or 0 if DLL not present.

@function get_version
@treturn number XRSound version
*/
int Interpreter::xrsound_get_version(lua_State *L)
{
	XRSound *snd = lua_toxrsound(L, 1);
	lua_pushnumber(L, snd->GetVersion());
	return 1;
}

/***
Load wav file.

Loads the specified sound file and assigns the supplied sound ID to it.  Note: it does not need to be a .wav file: it may be any supported sound file format
supported by XRSound.  Note that this call is lightweight and does not actually load the sound file data into memory: 
the sound data is not loaded until it is needed by PlayWav later.
If you load a different sound file with the same ID as an existing loaded sound file, the previous sound file is
automatically stopped if it is playing and replaced by this sound.

Note: for module sounds, you should pass PlaybackType.Global for this since other values have no effect when playing sounds from an Orbiter module (as opposed to an Orbiter vessel).

@function load_wav
@tparam number soundID sound ID to be assigned to this sound file (vessel-instance unique)
@tparam string filename path relatve to $ORBITER_ROOT of sound file to load
@tparam number playbackType denotes how sound will be faded. See @{types.PlaybackType|PlaybackType}
@treturn boolean true on success, false if file not found or XRSound.dll not present.
@usage
function clbk_postcreation()
	snd = xrsound.create_instance(vi)
	snd:load_wav(1, "XRSound/Default/On.wav", PlaybackType.InternalOnly)
	snd:load_wav(2, "XRSound/Default/Off.wav", PlaybackType.InternalOnly)
end
*/
int Interpreter::xrsound_load_wav(lua_State *L)
{
	XRSound *snd = lua_toxrsound(L, 1);
	int ID = luaL_checkinteger(L, 2);
	const char *filename = luaL_checkstring(L, 3);
	int type = luaL_checkinteger(L, 4);
	lua_pushboolean(L, snd->LoadWav(ID, filename, (XRSound::PlaybackType)type));
	return 1;
}

/***
Play wav file.

Play the sound file with the specified ID. If the sound is already playing, this call will only alter its loop or volume settings.

@function play_wav
@tparam number soundID vessel-instance-unique sound ID originally passed to LoadWav
@tparam[opt=false] boolean loop true to loop sound continuously until stop_wav called, false to play only once
@tparam[opt=1.0] number volume 0 (muted) - 1.0 (loudest)
@treturn boolean true on success, false if invalid sound ID or if XRSound.dll not present.
@usage
function clbk_consumebufferedkey(key, down, kstate)
    if oapi.keydown(kstate, OAPI_KEY.A) then
		snd:play_wav(1)
    end
    if oapi.keydown(kstate, OAPI_KEY.B) then
		snd:play_wav(2)
    end
end
*/
int Interpreter::xrsound_play_wav(lua_State *L)
{
	bool loop = false;
	float volume = 1.0f;

	XRSound *snd = lua_toxrsound(L, 1);
	int ID = luaL_checkinteger(L, 2);
	if(lua_gettop(L)>=3)
		loop = lua_toboolean(L, 3);
	if(lua_gettop(L)>=4)
		volume = luaL_checknumber(L, 4);

	lua_pushboolean(L, snd->PlayWav(ID, loop, volume));
	return 1;
}

/***
Stop wav file.

Stop playing the sound file with the specified ID.

@function stop_wav
@tparam number soundID unique sound ID originally passed to LoadWav and PlayWav
@treturn boolean true on success, false if invalid sound ID or if XRSound.dll not present.
*/
int Interpreter::xrsound_stop_wav(lua_State *L)
{
	XRSound *snd = lua_toxrsound(L, 1);
	int ID = luaL_checkinteger(L, 2);
	lua_pushboolean(L, snd->StopWav(ID));
	return 1;
}

/***
Sound status.

Returns the status of a sound.

@function is_wavplaying
@tparam number soundID unique sound ID originally passed to LoadWav and PlayWav
@treturn boolean false if the specified sound is not playing or XRSound.dll is not present.
*/
int Interpreter::xrsound_is_wavplaying(lua_State *L)
{
	XRSound *snd = lua_toxrsound(L, 1);
	int ID = luaL_checkinteger(L, 2);
	lua_pushboolean(L, snd->IsWavPlaying(ID));
	return 1;
}

/***
Pause/unpause.

Pause or unpause a sound.

@function set_paused
@tparam number soundID unique sound ID originally passed to LoadWav and PlayWav
@tparam boolean pause true to pause sound, false to unpause it
@treturn boolean true on success, false if invalid sound ID or XRSound.dll not present.
*/
int Interpreter::xrsound_set_paused(lua_State *L)
{
	XRSound *snd = lua_toxrsound(L, 1);
	int ID = luaL_checkinteger(L, 2);
	bool paused = lua_toboolean(L, 3);
	lua_pushboolean(L, snd->SetPaused(ID, paused));
	return 1;
}

/***
Pause status.

Detect if a sound is paused.

@function is_paused
@tparam number soundID unique sound ID originally passed to LoadWav and PlayWav
@treturn boolean true if sound is paused, or false if not paused, invalid sound ID, or XRSound.dll not present.
*/
int Interpreter::xrsound_is_paused(lua_State *L)
{
	XRSound *snd = lua_toxrsound(L, 1);
	int ID = luaL_checkinteger(L, 2);
	lua_pushboolean(L, snd->IsPaused(ID));
	return 1;
}

/***
Default sounds.

Enable or disable a default sound or group.  This can also be done globally via the configuration file; however, this option 
is useful if you want to disable some sound effects at runtime for a specific vessel class.

Not supported for module sounds.

@function set_defaultsoundenabled
@tparam number which default sound to enable or disable. See @{types.DefaultSoundID|DefaultSoundID}
@tparam boolean enabled true to enable default sound, false to disable it
@treturn boolean true on success, false if XRSound.dll not present or this default sound is disabled via config file (i.e., no sound file specified for this).
*/
int Interpreter::xrsound_set_defaultsoundenabled(lua_State *L)
{
	XRSound *snd = lua_toxrsound(L, 1);
	int ID = luaL_checkinteger(L, 2);
	bool enabled = lua_toboolean(L, 3);
	lua_pushboolean(L, snd->SetDefaultSoundEnabled((XRSound::DefaultSoundID)ID, enabled));
	return 1;
}

/***
Default sound status.

Get the status of a default sound or group.

Not supported for module sounds.

@function get_defaultsoundenabled
@tparam number ID which default sound ID to check. See @{types.DefaultSoundID|DefaultSoundID}
@treturn boolean true if the specified default sound or group is enabled, false if sound or group is disabled or XRSound.dll not present.
*/
int Interpreter::xrsound_get_defaultsoundenabled(lua_State *L)
{
	XRSound *snd = lua_toxrsound(L, 1);
	int ID = luaL_checkinteger(L, 2);
	lua_pushboolean(L, snd->GetDefaultSoundEnabled((XRSound::DefaultSoundID)ID));
	return 1;
}

/***
Set sound folder.

Set the default subfolder path for a default sound group, relative to $ORBITER_ROOT.

Not supported for module sounds.

@function set_defaultsoundgroupfolder
@tparam number ID which default XRSound group to update (only @{types.DefaultSoundID|DefaultSoundID}s that end in "Group" are valid for this call).
@tparam string subfolderpath subfolder path relative to $ORBITER_ROOT
@treturn boolean true on success, false if defaultSoundID is not a valid default group sound ID, no default sounds loaded for the supplied defaultSoundID, or XRSound.dll not present.
*/
int Interpreter::xrsound_set_defaultsoundgroupfolder(lua_State *L)
{
	XRSound *snd = lua_toxrsound(L, 1);
	int ID = luaL_checkinteger(L, 2);
	const char *path = luaL_checkstring(L, 3);
	lua_pushboolean(L, snd->SetDefaultSoundGroupFolder((XRSound::DefaultSoundID)ID, path));
	return 1;
}

/***
Get sound folder.

Returns the default subfolder path for a default sound group, relative to $ORBITER_ROOT.

Not supported for module sounds.

@function get_defaultsoundgroupfolder
@tparam number ID which default XRSound group to query (only @{types.DefaultSoundID|DefaultSoundID}s that end in "Group" are valid for this call).
@treturn string|nil default subfolder path for a default sound group, or nil if no default sounds loaded for the supplied defaultSoundID or XRSoundDLL not present.
*/
int Interpreter::xrsound_get_defaultsoundgroupfolder(lua_State *L)
{
	XRSound *snd = lua_toxrsound(L, 1);
	int ID = luaL_checkinteger(L, 2);

	const char *path = snd->GetDefaultSoundGroupFolder((XRSound::DefaultSoundID)ID);
	if(path)
		lua_pushstring(L, path);
	else
		lua_pushnil(L);
	return 1;
}

/***
Pan configuration.

Sets the pan (left/right balance) of the sound with the specified  ID.

Note: the sound must have already started playing via PlayWav for this to have any effect.

@function set_pan
@tparam number soundID unique sound ID originally passed to LoadWav and PlayWav
@tparam number pan range is from -1.0 (full left) to 1.0 (full right). Zero is the center.
@treturn boolean true on success, false if sound ID is invalid or is not currently playing.
*/
int Interpreter::xrsound_set_pan(lua_State *L)
{
	XRSound *snd = lua_toxrsound(L, 1);
	int ID = luaL_checkinteger(L, 2);
	float pan = luaL_checknumber(L, 3);
	lua_pushboolean(L, snd->SetPan(ID, pan));
	return 1;
}

/***
Pan status.

Returns the pan of the sound with the specified ID, from -1.0 (full left) to 1.0 (full right). Zero is the center.

Note: the sound must have already started playing via PlayWav.

@function get_pan
@tparam number soundID unique sound ID originally passed to LoadWav and PlayWav
@treturn number range is from -1.0 (full left) to 1.0 (full right). Zero is the center. Returns -100 if sound ID is invalid or is not currently playing.
*/
int Interpreter::xrsound_get_pan(lua_State *L)
{
	XRSound *snd = lua_toxrsound(L, 1);
	int ID = luaL_checkinteger(L, 2);
	lua_pushnumber(L, snd->GetPan(ID));
	return 1;
}

/***
Speed configuration.

Sets the playback speed of the sound with the specified ID.

Note: the sound must have already started playing via PlayWav for this to have any effect.

Plays the sound at a higher or lower speed, increasing or decreasing its frequency, which makes it sound lower or higher.

@function set_playbackspeed
@tparam number soundID unique sound ID originally passed to LoadWav and PlayWav
@tparam[opt=1.0] number speed factor of the speed increase or decrease; 2 is twice as fast, 0.5 is only half as fast. The default is 1.0.
@treturn boolean true on success, false if sound ID is invalid or is not currently playing.
*/
int Interpreter::xrsound_set_playbackspeed(lua_State *L)
{
	XRSound *snd = lua_toxrsound(L, 1);
	int ID = luaL_checkinteger(L, 2);
	float speed = 1.0f;
	if(lua_gettop(L)>=3)
		speed = luaL_checknumber(L, 3);

	lua_pushboolean(L, snd->SetPlaybackSpeed(ID, speed));
	return 1;
}

/***
Speed status.

Returns the playback speed of the sound with the specified ID.

Note: the sound must have already started playing via PlayWav.

@function get_playbackspeed
@tparam number soundID unique sound ID originally passed to LoadWav and PlayWav
@treturn number factor of the speed increase or decrease; 2 is twice as fast, 0.5 is only half as fast. The default is 1.0. Returns 0 if sound ID is invalid or is not currently playing.
*/
int Interpreter::xrsound_get_playbackspeed(lua_State *L)
{
	XRSound *snd = lua_toxrsound(L, 1);
	int ID = luaL_checkinteger(L, 2);
	lua_pushnumber(L, snd->GetPlaybackSpeed(ID));
	return 1;
}

/***
Play position configuration.

Sets the playback position of the sound with the specified ID.

Note: the sound must have already started playing via PlayWav for this to have any effect.

@function set_playposition
@tparam number soundID unique sound ID originally passed to LoadWav and PlayWav
@tparam number positionMillis  must be between zero (the start of the file) and the length of the file, in milliseconds
@treturn boolean true on success, false if positionMillis is invalid or if sound ID is invalid or is not currently playing.
*/
int Interpreter::xrsound_set_playposition(lua_State *L)
{
	XRSound *snd = lua_toxrsound(L, 1);
	int ID = luaL_checkinteger(L, 2);
	int position = luaL_checkinteger(L, 3);

	lua_pushboolean(L, snd->SetPlayPosition(ID, position));
	return 1;
}

/***
Play position status.

Returns the playback position of the sound in milliseconds with the specified ID.

Note: the sound must have already started playing via PlayWav.

@function get_playposition
@tparam number soundID unique sound ID originally passed to LoadWav and PlayWav
@treturn number between zero (the start of the file) and the length of the file, in milliseconds. Returns < 0 if sound ID is invalid or is not currently playing.
*/
int Interpreter::xrsound_get_playposition(lua_State *L)
{
	XRSound *snd = lua_toxrsound(L, 1);
	int ID = luaL_checkinteger(L, 2);
	lua_pushnumber(L, snd->GetPlayPosition(ID));
	return 1;
}

// Clean up if handle is garbage collected
int Interpreter::xrsound_collect(lua_State *L)
{
	XRSound *snd = lua_toxrsound(L, 1);
	delete snd;
	return 0;
}


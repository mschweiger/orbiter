-- Orbiter Lua script initialisations

-- Some useful general constants
PI=3.14159265358979    -- pi
PI2=PI*2               -- pi*2
PI05=1.57079632679490  -- pi/2
RAD=PI/180.0           -- deg->rad conversion factor
DEG=180.0/PI           -- rad->deg conversion factor
C0=299792458.0         -- speed of light in vacuum [m/s]
TAUA=499.004783806     -- light time for 1 AU [s]
AU=C0*TAUA             -- astronomical unit (mean geocentric distance of the sun) [m]
GGRAV=6.67259e-11      -- gravitational constant [m^3 kg^-1 s^-2]
G0=9.81                -- gravitational acceleration [m/s^2] at Earth mean radius
ATMP=101.4e3           -- atmospheric pressure [Pa] at Earth sea level
ATMD=1.293             -- atmospheric density [kg/m^3] at Earth sea level


-- Build a key table for orbiter keys
-- NEEDS TO BE COMPLETED!
ktable = {Q=0x10, W=0x11, E=0x12, R=0x13, T=0x14, Y=0x15, U=0x16, I=0x17,
          O=0x18, P=0x19, A=0x1E, S=0x1F, D=0x20, F=0x21, G=0x22, H=0x23,
          J=0x24, K=0x25, L=0x26, Z=0x2C, X=0x2D, C=0x2E, V=0x2F, B=0x30,
          N=0x31, M=0x32}


-- execute a script in the 'Script' folder (.lua extension is assumed)
function run (script)
  dofile('./Script/'..script..'.lua')
end

-- execute a script in the Orbiter root folder
function run_global (script)
  dofile(script)
end

-- -------------------------------------------------
-- Branch management
-- Branch threads are stored in table 'branch' with
-- counter 'branch.count'
-- -------------------------------------------------

branch = {}
branch.count = 0
branch.nslot = 0

-- Create a new branch coroutine, store it in the branch
-- table, and execute its first cycle

function proc.bg (func,...)
	-- first, collect all dead branches
	for i=1,branch.nslot do
		if branch[i] ~= nil then
			if coroutine.status(branch[i]) == 'dead' then
				branch[i] = nil
				branch.count = branch.count-1
			end
		end
	end

	-- check if there is a free slot
	local slot = 0
	for i=1,branch.nslot do
		if branch[i] == nil then
			slot = i
			break
		end
	end

	-- no free slot: increase branch counter
	if slot == 0 then
		branch.nslot = branch.nslot+1
		slot = branch.nslot
	end

	-- create the new branch
	local th = coroutine.create (func)
	branch[slot] = th
	branch.count = branch.count+1
	coroutine.resume (th,...)
	term.out ('job id='..slot..' ('..branch.count..' jobs)')
	return slot
end

-- Remove a branch from the branch table (and keep fingers
-- crossed that the coroutine will be garbage-collected)

function proc.kill (n)
	if branch[n] ~= nil then
		branch[n] = nil
		branch.count = branch.count-1
		while (branch.nslot > 0) and (branch[branch.nslot] == nil) do
			branch.nslot = branch.nslot-1
		end
		term.out ('job '..n..' killed ('..branch.count..' jobs left)')
	end
end

-- Time skip: branches yield, the main trunk resumes all
-- coroutines for a single cycle, then calls proc.Frameskip
-- to pass control back to orbiter for a new simulation cycle

function proc.skip ()
	if coroutine.running() then  -- we are in the main trunk
		for i=1,branch.nslot do
			if branch[i] ~= nil then
				coroutine.resume (branch[i])
			end
		end
		proc.Frameskip() -- hand control to orbiter for one cycle
            if wait_exit ~= nil then
                error()   -- return to caller immediately
            end
	else
		coroutine.yield()
	end
end

-- -------------------------------------------------
-- A few waiting functions
-- -------------------------------------------------

-- wait for simulation time t.
-- Optionally execute a function at each frame while waiting
function proc.wait_simtime (t, f, ...)
    while oapi.get_simtime() < t do
		if f then
			f(table.unpack({...}))
		end
		proc.skip()
	end
end

-- wait for simulation interval dt
-- Optionally execute a function at each frame while waiting
function proc.wait_simdt (dt, f, ...)
    local t1 = oapi.get_simtime()+dt
    while oapi.get_simtime() < t1 do
		if f then
			f(table.unpack({...}))
		end
		proc.skip()
	end
end

-- wait for system time t
-- Optionally execute a function at each frame while waiting
function proc.wait_systime (t, f, ...)
    while oapi.get_systime() < t do
		if f then
			f(table.unpack({...}))
		end
		proc.skip()
	end
end

-- wait for system interval dt
-- Optionally execute a function at each frame while waiting
function proc.wait_sysdt (dt, f, ...)
    local t1 = oapi.get_systime()+dt
    while oapi.get_systime() < t1 do
		if f then
			f(table.unpack({...}))
		end
		proc.skip()
	end
end

-- wait for a given number of frames
-- Optionally execute a function at each frame while waiting
function proc.wait_frames(n, f, ...)
	local frame = 0
	while frame < n do
		if f then
			f(table.unpack({...}))
		end
		frame = frame+1
		proc.skip()
	end
end

-- wait for f() >= tgt
function proc.wait_ge (f, tgt, ...)
    while f(table.unpack({...})) < tgt do proc.skip() end
end

-- wait for f() <= tgt
function proc.wait_le (f, tgt, ...)
    while f(table.unpack({...})) > tgt do proc.skip() end
end

-- wait for input
function proc.wait_input (title)
    oapi.open_inputbox (title)
    local ans = oapi.receive_input ()
    while ans == nil do
        proc.skip()
        ans = oapi.receive_input ()
    end
    return ans
end

-- -------------------------------------------------
-- Private functions (should not be directly called)
-- -------------------------------------------------

-- Idle loop (called after command returns, to allow
-- background jobs to continue executing)

function _idle ()
	for i=1,branch.nslot do
		if branch[i] ~= nil then
			coroutine.resume (branch[i])
		end
	end
	return branch.count
end

-- Returns the number of background jobs

function _nbranch ()
    return branch.count
end


-- helpers for basic types
function _V(x,y,z)
	return {x=x, y=y, z=z}
end
function _M(m11, m12, m13, m21, m22, m23, m31, m32, m33)
	return {m11=m11,m12=m12,m13=m13,m21=m21,m22=m22,m23=m23,m31=m31,m32=m32,m33=m33}
end
function _R(left, top, right, bottom)
	return {left=left, top=top, right=right, bottom=bottom}
end
function _COLOUR4(r,g,b,a)
	return {r=r, g=g, b=b, a=a}
end
function _RGB(r,g,b)
	return r + g*256 + b*65536
end

-- helper to create a classlike object
-- Usage:
--      myclass = Class()
--      function myclass:new(a,b)
--      	self.a = a
--      	self.b = b
--      end
--      function myclass:print()
--      	print(a+b)
--      end
--      myobj = myclass(1,2)
--      myobj:print()

function Class(parent)
    local class = {}
    class.__index = class
    local mt = parent or {}
    setmetatable(class, mt)
    local function __new(self, ...)
        local obj = setmetatable({}, self)
        if obj.new then
            obj.new(obj, ...)
        end
        return obj
    end
    mt.__call = __new
    return class
end

-- helpers for Lua MFDs

-- ==============================================================
-- __orbiter_extend_userdata:
--     associate the userdata metatable with the one from a Lua object
--     so that we can call both OAPI and Lua methods from it
--     For example for an MFD you can do :
--          self:set_title(skp, title)  -- native MFD2 method
--          self:update_pg_prm(skp)     -- Lua method
-- ==============================================================
local function __orbiter_extend_userdata(module, userdata)
    local mt = {}
	function mt.__index(self, key)
		-- first search in the module
		local ret = rawget(module, key)
		if ret then
			return ret
		else
			-- try in the userdata
			ret = userdata[key]
			if type(ret) == "function" then
				return function(self, ...)
					return ret(userdata, ...)
				end
			else
				return ret
			end
		end
	end
    return setmetatable({}, mt)
end

local function __orbiter_extend_vessel(module)
    local mt = {}

	local obj = {}
	for k, v in pairs(module) do
		if type(v) == "function" then
			obj[k] = v
		end
	end

	function mt.__index(self, key)
		-- first search in the module
--		local ret = rawget(module, key)
		local ret = module[key]
		if ret then
			return ret
		else
			-- try in the userdata
			ret = vi[key]
			if type(ret) == "function" then
				return function(self, ...)
					-- native functions expect a vessel handle so we pass vi instead of self
					return ret(vi, ...)
				end
			else
				return ret
			end
		end
	end
--    return setmetatable({}, mt)
    return setmetatable(obj, mt)
end

-- ==============================================================
-- MFDModule:
-- Create a module object from a Lua MFD implementation
--   It only provides a __call meta function and you can
--   use it like this with require:
--      mymfdmodule = require("mymfdmodule")
--      mymfd = mymfdmodule()
--
-- The method links the C++ MFD (userdata containing a VesselMFD *)
-- with the Lua object using __orbiter_extend_userdata then
-- calls the "new" method
-- This hides the userdata part from the MFD implementation
-- ==============================================================
function MFDModule(impl)
	local module = {}
	module.__index = module
	setmetatable(module, module)
	function module.__call(_, mfd, w, h, vessel, userdata)
		local mfd = __orbiter_extend_userdata(impl, userdata)
		mfd:new(mfd, w, h, vessel)
		return mfd
	end
	return module
end

-- Syntactic sugar for animation components to look like their C++ equivalents
function MGROUP_ROTATE(_mesh, _grp, _ref, _axis, _angle)
	return oapi.create_animationcomponent {
		type = 'rotation',
		mesh = _mesh,
		grp = _grp,
		ref = _ref,
		axis = _axis,
		angle = _angle
	}
end

function MGROUP_TRANSLATE(_mesh, _grp, _shift)
	return oapi.create_animationcomponent {
		type = 'translation',
		mesh = _mesh,
		grp = _grp,
		shift = _shift
	}
end

function MGROUP_SCALE(_mesh, _grp, _ref, _scale)
	return oapi.create_animationcomponent {
		type = 'scaling',
		mesh = _mesh,
		grp = _grp,
		ref = _ref,
		scale = _scale
	}
end

-- scenario parsing helpers
--
-- ==============================================================
-- scenario_lines(scn)
--     returns an iterator constructed from a filehandle to use in for loops, typically used in clbk_loadstateex
-- Typical usage:
--     function clbk_loadstateex(scn, vs)
--         ...
--         for line in scenario_lines(scn) do
--             ... do stuff with the line
--         end
--         ...
--     end
-- ==============================================================
function scenario_lines(scn)
    return function()
              return oapi.readscenario_nextline(scn)
           end
end

-- ==============================================================
-- __orbiter__tointeger(str)
--     return a number from a string if it represents an integer, nil otherwise
-- ==============================================================
local function __orbiter__tointeger(str)
    if str:find("[^0-9%-]") then
        return nil
    end
    return tonumber(str)
end

-- ==============================================================
-- __orbiter__toboolean(str)
--     return a boolean from a string if it is "0" or "1", nil otherwise
-- ==============================================================
local function __orbiter__toboolean(str)
	if str == "0" or str == "false" then
		return false
	elseif str == "1" or str == "true" then
		return true
	else
		return nil
	end
end

-- ==============================================================
-- scenario_line_match(line, format, result)
--     match a line with a pattern and extract the associated values, typically used in clbk_loadstateex
--     return true if the line matches the pattern and initialise the result table
--     return false otherwise
-- Parameters:
--     line: string containing the pattern to match
--     format: format of the expected pattern in a scanf like format
--           supported format types:
--                - %d: integer
--                - %f: number (float or integer)
--                - %b: boolean (0 or 1)
--                - %s: string
--     result: table whose "res" member will be populated with the matched values in their expected format
-- Typical usage:
--      function clbk_loadstateex(scn, vs)
--          for line in scenario_lines(scn) do
--              match = {}
--              if scenario_line_match(line, "ASCENTAP %b %b %b %f %f %f %f", match) then
--              		active         = match.res[1]  -- boolean
--              		met_active     = match.res[2]  -- boolean
--              		do_oms2        = match.res[3]  -- boolean
--              		tgt_alt        = match.res[4]  -- number
--              		launch_azimuth = match.res[5]  -- number
--              		launch_lng     = match.res[6]  -- number
--              		launch_lat     = match.res[7]  -- number
--              		return true
--              end
--          end
--      end
-- ==============================================================
function scenario_line_match(line, format, result)
    local patterns={}
    local types = {}
    for str in string.gmatch(format, "([^%s]+)") do
        if str == "%d" then
            table.insert(patterns,"(%S+)")
            table.insert(types, __orbiter__tointeger)
        elseif str == "%f" then
            table.insert(patterns,"(%S+)")
            table.insert(types, tonumber)
        elseif str == "%b" then
            table.insert(patterns,"(%S+)")
            table.insert(types, __orbiter__toboolean)
        elseif str == "%s" then
            table.insert(patterns,"(%S+)")
            table.insert(types, tostring)
        else
            table.insert(patterns,str)
        end    
    end

    local match_pattern = table.concat(patterns, "%s+")
    local matches = {line:match(match_pattern)}
    if #matches ~= 0 then
        result.res = {}
        for i=1,#matches do
            local conversion = types[i](matches[i])
            if conversion == nil then
                return false
            end
            result.res[i] = conversion
        end
        return true
    end
    return false
end

-- Object style vessels
function VesselClass(parent)
	parent = parent or {}
	local vessel = __orbiter_extend_vessel(parent)
	
	return vessel
end

-- create global functions from object methods if they don't start with '_'
function register_vesselclass(vessel, cpp_names)
	local cpp_mapping = {
		clbkSetClassCaps	     = "clbk_setclasscaps",
		clbkPostCreation	     = "clbk_postcreation",
		clbkPreStep	             = "clbk_prestep",
		clbkPostStep	         = "clbk_poststep",
		clbkSaveState	         = "clbk_savestate",
		clbkLoadStateEx	         = "clbk_loadstateex",
		clbkConsumeDirectKey	 = "clbk_consumedirectkey",
		clbkConsumeBufferedKey   = "clbk_consumebufferedkey",
		clbkFocusChanged	     = "clbk_focuschanged",
		clbkPlaybackEvent	     = "clbk_playbackevent",
		clbkRCSMode	             = "clbk_RCSmode",
		clbkADCtrlMode	         = "clbk_ADctrlmode",
		clbkHUDMode	             = "clbk_HUDmode",
		clbkMFDMode	             = "clbk_MFDmode",
		clbkNavMode	             = "clbk_NAVmode",
		clbkDockEvent	         = "clbk_dockevent",
		clbkAnimate	             = "clbk_animate",
		clbkLoadGenericCockpit   = "clbk_loadgenericcockpit",
		clbkPanelMouseEvent	     = "clbk_panelmouseevent",
		clbkPanelRedrawEvent	 = "clbk_panelredrawevent",
		clbkLoadVC	             = "clbk_loadVC",
		clbkVisualCreated	     = "clbk_visualcreated",
		clbkVisualDestroyed	     = "clbk_visualdestroyed",
		clbkVCMouseEvent	     = "clbk_VCmouseevent",
		clbkVCRedrawEvent	     = "clbk_VCredrawevent",
		clbkDrawHUD	             = "clbk_drawHUD",
		clbkNavProcess	         = "clbk_navprocess",
		clbkLoadPanel2D	         = "clbk_loadpanel2d",
		clbkRenderHUD	         = "clbk_renderHUD",
		clbkGetRadiationForce    = "clbk_getradiationforce",
		new = "clbk_new",
		destroy = "clbk_destroy"
	}
	if cpp_names == nil then
		cpp_names = false
	end
	for k, v in pairs(vessel) do
		if type(v) == "function" and k:sub(1, 1) ~= '_' then
			if cpp_names then
				k = cpp_mapping[k] or k
			end
			--oapi.write_log("Exporting "..k)
			-- declare global if in strict mode
			if strictmode_add_global then
				strictmode_add_global(k)
			end
			_G[k] = function(...) return v(vessel, ...) end
		end
	end
end

function KEYMOD_CONTROL(kstate)
	return oapi.keydown(kstate, OAPI_KEY.LCONTROL) or oapi.keydown(kstate, OAPI_KEY.RCONTROL)
end
function KEYMOD_ALT(kstate)
	return oapi.keydown(kstate, OAPI_KEY.LALT) or oapi.keydown(kstate, OAPI_KEY.RALT)
end
function KEYMOD_SHIFT(kstate)
	return oapi.keydown(kstate, OAPI_KEY.LSHIFT) or oapi.keydown(kstate, OAPI_KEY.RSHIFT)
end

-- perform a simple copy of an object :
--   only deepcopies the values, not the keys
--   no recursive table support
--   no meta types support
function simplecopy(obj)
    if type(obj) == 'table' then
        local copy = {}
        for k, v in pairs(obj) do
            copy[k] = simplecopy(v)
        end
        return copy
    else
        return obj
    end
end

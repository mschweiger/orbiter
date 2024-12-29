-- ---------------------------------------------------
-- General TEST Utils
-- ---------------------------------------------------

function add_line(line)
	oapi.dbg_out(line)
	oapi.write_log(line)
end

function assert(cond)
	if cond == false then
		add_line(" - FAILED!")
		error("Assertion failed\n"..debug.traceback())
        oapi.exit(1)
	end
end

function pass()
	add_line(" - passed")
end


-- float comparison
function almost_equal(a,b)
	if a==0 and b==0 then
		return true
	end
	if b==0 then
		return math.abs(a) < 0.000000001
	end
	return math.abs(a/b) < 1.0000000001 
end

-- ---------------------------------------------------
-- "Constants"
-- ---------------------------------------------------

data = "Hello world! Hello world! Hello world! Hello world!\n"
    .. "Hello world! Hello world! Hello world! Hello world!\n"
    .. "Hello world! Hello world! Hello world! Hello world!"
vec = { x = 1.2, y = -3.4, z = 5.6 }

fname_root      = "__delete_me__.txt"
fname_config    = "Sun.cfg"
fname_scenarios = "Tests\\Description.txt"
fname_textures  = "transp.dds"
fname_textures2 = "DG\\dgmk4_1.dds"
fname_meshes    = "dummy.msh"
fname_modules   = "ScriptVessel.dll"


-- ---------------------------------------------------
-- Helper
-- ---------------------------------------------------
local function equ (fa, fb)
	return (math.abs(fa - fb) < 0.00001)
end

-- ---------------------------------------------------
-- TEST(S)
-- ---------------------------------------------------

add_line("=== Lua script unit tests ===")
add_line("")

add_line("--- oapi module ---")

-- ---------------------------------------------------
add_line("Test: oapi.get_orbiter_version()")
-- ---------------------------------------------------
value = oapi.get_orbiter_version()
assert( value ~= nil )
assert( type(value) == "number" )
add_line("   Version " .. tostring(value))
pass()
-- ---------------------------------------------------


-- ---------------------------------------------------
add_line("Test: oapi.rand()")
-- ---------------------------------------------------
value = oapi.rand()
assert( value ~= nil )
assert( type(value) == "number" )
assert( (value <= 1.0) and (value >= 0.0) )
assert( value ~= oapi.rand() ) -- very unlikely ;)
pass()
-- ---------------------------------------------------


-- ---------------------------------------------------
add_line("Test: oapi.deflate()")
-- ---------------------------------------------------
zdata = oapi.deflate(data) -- zipped data
assert( zdata ~= nil   )
assert( #zdata < #data ) -- 30 < 155
assert( #zdata == 30   )
pass()
-- ---------------------------------------------------


-- ---------------------------------------------------
add_line("Test: oapi.inflate()")
-- ---------------------------------------------------
udata = oapi.inflate(zdata) -- unzipped zdata
assert( udata ~= nil    )
assert( #udata == #data ) -- both 155 in size
assert( udata == data   ) -- should be equal
pass()
-- ---------------------------------------------------


-- ---------------------------------------------------
add_line("Test: oapi.formatvalue(value,prec)")
-- ---------------------------------------------------
-- rediculous precisions...
assert( oapi.formatvalue(PI,0)  == " 3.141593")
assert( oapi.formatvalue(PI,1)  == " 3.141593")
assert( oapi.formatvalue(PI,2)  == " 3")
assert( oapi.formatvalue(PI,3)  == " 3.1")
assert( oapi.formatvalue(PI,4)  == " 3.14")
assert( oapi.formatvalue(PI,5)  == " 3.142")
assert( oapi.formatvalue(PI,6)  == " 3.1416")
assert( oapi.formatvalue(PI,7)  == " 3.14159")
assert( oapi.formatvalue(PI,8)  == " 3.141593")
assert( oapi.formatvalue(PI,9)  == " 3.1415927")
assert( oapi.formatvalue(PI,10) == " 3.14159265")
assert( oapi.formatvalue(PI,11) == " 3.141592654")
assert( oapi.formatvalue(PI,12) == " 3.1415926536")
assert( oapi.formatvalue(PI,13) == " 3.14159265359")
assert( oapi.formatvalue(PI,14) == " 3.141592653590")
assert( oapi.formatvalue(PI,15) == " 3.1415926535898")
assert( oapi.formatvalue(PI,16) == " 3.14159265358979") -- <= From here on it's just "base-2 noise"
assert( oapi.formatvalue(PI,17) == " 3.141592653589790") --   see definition of PI in oapi_init.lua
assert( oapi.formatvalue(PI,18) == " 3.1415926535897900")
assert( oapi.formatvalue(PI,19) == " 3.14159265358979001")
assert( oapi.formatvalue(PI,20) == " 3.141592653589790007")
assert( oapi.formatvalue(PI,21) == " 3.1415926535897900074")
assert( oapi.formatvalue(PI,22) == " 3.14159265358979000737")
assert( oapi.formatvalue(PI,23) == " 3.141592653589790007373")
assert( oapi.formatvalue(PI,24) == " 3.1415926535897900073735")
assert( oapi.formatvalue(PI,25) == " 3.14159265358979000737349")
assert( oapi.formatvalue(PI,26) == " 3.141592653589790007373495")
assert( oapi.formatvalue(PI,27) == " 3.1415926535897900073734945")
assert( oapi.formatvalue(PI,28) == " 3.14159265358979000737349452")
assert( oapi.formatvalue(PI,29) == " 3.141592653589790007373494518")
assert( oapi.formatvalue(PI,30) == " 3.1415926535897900073734945181")
assert( oapi.formatvalue(PI,31) == " 3.14159265358979000737349451811")
assert( oapi.formatvalue(PI,32) == " 3.141592653589790007373494518106")
assert( oapi.formatvalue(PI,33) == " 3.1415926535897900073734945181059")
assert( oapi.formatvalue(PI,34) == " 3.14159265358979000737349451810587")
assert( oapi.formatvalue(PI,35) == " 3.141592653589790007373494518105872")
assert( oapi.formatvalue(PI,36) == " 3.1415926535897900073734945181058720")
assert( oapi.formatvalue(PI,37) == " 3.14159265358979000737349451810587198")
assert( oapi.formatvalue(PI,38) == " 3.141592653589790007373494518105871975")
assert( oapi.formatvalue(PI,39) == " 3.1415926535897900073734945181058719754")
assert( oapi.formatvalue(PI,40) == " 3.14159265358979000737349451810587197542")
assert( oapi.formatvalue(PI,41) == " 3.141592653589790007373494518105871975422")
assert( oapi.formatvalue(PI,42) == " 3.1415926535897900073734945181058719754219")
-- regular postfixes
assert( oapi.formatvalue(PI*1e1 ) == " 31.42"  )
assert( oapi.formatvalue(PI*1e2 ) == " 314.2"  )
assert( oapi.formatvalue(PI*1e3 ) == " 3.142k" )
assert( oapi.formatvalue(PI*1e4 ) == " 31.42k" )
assert( oapi.formatvalue(PI*1e5 ) == " 314.2k" )
assert( oapi.formatvalue(PI*1e6 ) == " 3.142M" )
assert( oapi.formatvalue(PI*1e7 ) == " 31.42M" )
assert( oapi.formatvalue(PI*1e8 ) == " 314.2M" )
assert( oapi.formatvalue(PI*1e9 ) == " 3.142G" )
assert( oapi.formatvalue(PI*1e10) == " 31.42G" )
assert( oapi.formatvalue(PI*1e11) == " 314.2G" )
assert( oapi.formatvalue(PI*1e12) == " 3.142T" )
assert( oapi.formatvalue(PI*1e13) == " 31.42T" )
assert( oapi.formatvalue(PI*1e14) == " 314.2T" )
assert( oapi.formatvalue(PI*1e15) == " 3e+15"  )
assert( oapi.formatvalue(PI*1e16) == " 3e+16"  )
assert( oapi.formatvalue(PI*1e17) == " 3e+17"  )
assert( oapi.formatvalue(PI*1e18) == " 3e+18"  )
assert( oapi.formatvalue(PI*1e19) == " 3e+19"  )
assert( oapi.formatvalue(PI*1e20) == " 3e+20"  )
-- negative values
assert( oapi.formatvalue(-PI*1e1 ) == "-31.42"  )
assert( oapi.formatvalue(-PI*1e2 ) == "-314.2"  )
assert( oapi.formatvalue(-PI*1e3 ) == "-3.142k" )
assert( oapi.formatvalue(-PI*1e4 ) == "-31.42k" )
assert( oapi.formatvalue(-PI*1e5 ) == "-314.2k" )
assert( oapi.formatvalue(-PI*1e6 ) == "-3.142M" )
assert( oapi.formatvalue(-PI*1e7 ) == "-31.42M" )
assert( oapi.formatvalue(-PI*1e8 ) == "-314.2M" )
assert( oapi.formatvalue(-PI*1e9 ) == "-3.142G" )
assert( oapi.formatvalue(-PI*1e10) == "-31.42G" )
assert( oapi.formatvalue(-PI*1e11) == "-314.2G" )
assert( oapi.formatvalue(-PI*1e12) == "-3.142T" )
assert( oapi.formatvalue(-PI*1e13) == "-31.42T" )
assert( oapi.formatvalue(-PI*1e14) == "-314.2T" )
assert( oapi.formatvalue(-PI*1e15) == "-3e+15"  )
assert( oapi.formatvalue(-PI*1e16) == "-3e+16"  )
assert( oapi.formatvalue(-PI*1e17) == "-3e+17"  )
assert( oapi.formatvalue(-PI*1e18) == "-3e+18"  )
assert( oapi.formatvalue(-PI*1e19) == "-3e+19"  )
assert( oapi.formatvalue(-PI*1e20) == "-3e+20"  )
pass()
-- ---------------------------------------------------


--[[ get_color doesn't seem to work in "headless" tests :(
-- ---------------------------------------------------
add_line("Test: oapi.get_color(r,g,b)")
-- ---------------------------------------------------
assert( oapi.get_color(  0,  0,  0)  == 0        )
assert( oapi.get_color(  0,  0,255)  == 255      )
assert( oapi.get_color(  0,255,  0)  == 65280    )
assert( oapi.get_color(255,  0,  0)  == 16711680 )
assert( oapi.get_color(  0,255,255)  == 65535    )
assert( oapi.get_color(255,255,255)  == 16777215 )
assert( oapi.get_color(255,255,  0)  == 16776960 )
assert( oapi.get_color(  1,  2,  3)  == 66051    )
pass()
-- ---------------------------------------------------
--]]


-- ---------------------------------------------------
add_line("Test: oapi.openfile(fname,FILE_OUT,root)")
-- ---------------------------------------------------
-- We need to create this file in ROOT first,
--   so we can be sure it is present when we do the
--   read-tests later on
mode = FILE_ACCESS_MODE.FILE_OUT
add_line("   ...FILE_OUT write (overwrite)")
f = oapi.openfile(fname_root, mode)
assert(f ~= nil)
oapi.closefile(f, mode)

mode = FILE_ACCESS_MODE.FILE_APP
add_line("   ...FILE_APP write (append)")
f = oapi.openfile(fname_root, mode)
assert(f ~= nil)
-- oapi.closefile(f, mode) -- NOT yet! oapi_writeitem_xxx test use it!
pass()
-- ---------------------------------------------------


-- ---------------------------------------------------
add_line("Test: oapi.writeline(f,line)")
-- ---------------------------------------------------
oapi.writeline(f, "# >>> This is a test-artifact and can be deleted! <<<");
oapi.writeline(f, "");
pass() -- not much to test here
-- ---------------------------------------------------


-- ---------------------------------------------------
add_line("Test: oapi.writeitem_xxx(f,item,value)")
-- ---------------------------------------------------
add_line("   ...oapi.writeitem_string()")
oapi.writeitem_string(f, "VAL_STR", "foo")

add_line("   ...oapi.writeitem_float()")
oapi.writeitem_float(f, "VAL_FLOAT", PI)

add_line("   ...oapi.writeitem_int()")
oapi.writeitem_int(f, "VAL_INT", 4711)

add_line("   ...oapi.writeitem_bool()")
oapi.writeitem_bool(f, "VAL_BOOL[0]", false)
oapi.writeitem_bool(f, "VAL_BOOL[1]", true)

add_line("   ...oapi.writeitem_vec()")
oapi.writeitem_vec(f, "VAL_VEC", vec)

oapi.closefile(f, mode)
pass()
-- ---------------------------------------------------


-- ---------------------------------------------------
add_line("Test: oapi.readitem_xxx(f,item)")
-- ---------------------------------------------------
mode = FILE_ACCESS_MODE.FILE_IN
f = oapi.openfile(fname_root, mode)
assert(f ~= nil)

add_line("   ...oapi.readitem_vec()")
vec2 = oapi.readitem_vec(f, "VAL_VEC")
assert( vec2.x == vec.x and vec2.y == vec.y and vec2.z == vec.z )

add_line("   ...oapi.readitem_bool()")
assert( oapi.readitem_bool(f, "VAL_BOOL[1]") == true )
assert( oapi.readitem_bool(f, "VAL_BOOL[0]") == false )

add_line("   ...oapi.readitem_int()")
assert( oapi.readitem_int(f, "VAL_INT") == 4711 )

add_line("   ...oapi.readitem_float()")
assert( equ( oapi.readitem_float(f, "VAL_FLOAT"), 3.14159) ) -- close enough?

add_line("   ...oapi.readitem_string()")
assert( oapi.readitem_string(f, "VAL_STR") == "foo" )

oapi.closefile(f, mode)
pass()
-- ---------------------------------------------------


-- ---------------------------------------------------
add_line("Test: oapi.openfile(fname,FILE_IN,...)")
-- ---------------------------------------------------
mode = FILE_ACCESS_MODE.FILE_IN

add_line("   ...ROOT      Orbiter main directory")
f = oapi.openfile(fname_root, mode, PATH_ROOT.ROOT)
assert(f ~= nil)
oapi.closefile(f, mode)

add_line("   ...CONFIG    Orbiter config folder")
f = oapi.openfile(fname_config, mode, PATH_ROOT.CONFIG)
assert(f ~= nil)
oapi.closefile(f, mode)

add_line("   ...SCENARIOS Orbiter scenarios folder")
f = oapi.openfile(fname_scenarios, mode, PATH_ROOT.SCENARIOS)
assert(f ~= nil)
oapi.closefile(f, mode)

add_line("   ...TEXTURES  Orbiter standard texture folder")
f = oapi.openfile(fname_textures, mode, PATH_ROOT.TEXTURES)
assert(f ~= nil)
oapi.closefile(f, mode)

add_line("   ...TEXTURES2 Orbiter high-res texture folder")
f = oapi.openfile(fname_textures2, mode, PATH_ROOT.TEXTURES2)
assert(f ~= nil)
oapi.closefile(f, mode)

add_line("   ...MESHES    Orbiter mesh folder")
f = oapi.openfile(fname_meshes, mode, PATH_ROOT.MESHES)
assert(f ~= nil)
oapi.closefile(f, mode)

add_line("   ...MODULES   Orbiter module folder")
f = oapi.openfile(fname_modules, mode, PATH_ROOT.MODULES)
assert(f ~= nil)
oapi.closefile(f, mode)

add_line("   ...FILE_IN vs. FILE_IN_ZEROONFAIL on non-existing file")
f1 = oapi.openfile("nofile.txt", FILE_ACCESS_MODE.FILE_IN)
f2 = oapi.openfile("nofile.txt", FILE_ACCESS_MODE.FILE_IN_ZEROONFAIL)
assert(f1 ~= f2)
assert(f1 ~= nil)
assert(f2 == nil)
oapi.closefile(f1, FILE_ACCESS_MODE.FILE_IN)
oapi.closefile(f2, FILE_ACCESS_MODE.FILE_IN_ZEROONFAIL)

pass()
-- ---------------------------------------------------


add_line("Test: planet status")

earth = oapi.get_gbody("Earth")
assert(almost_equal(oapi.get_planetperiod(earth), 86164.10132))
assert(almost_equal(oapi.get_planetobliquity(earth)/math.pi*180,23.43929100097))
assert(oapi.planet_hasatmosphere(earth))

atm1 = oapi.get_planetatmparams(earth,oapi.get_size(earth))
atm2 = oapi.get_planetatmparams(earth,0,0,0)

assert(atm1.rho == atm2.rho)
assert(atm1.T == atm2.T)
assert(atm1.p == atm2.p)

assert(almost_equal(atm1.rho,1.2247449178673))
assert(almost_equal(atm1.T,288.15))
assert(almost_equal(atm1.p,101329.1294913))
-- by definition surface relative and planet local is (0,0,0)
gv = oapi.get_groundvector(earth,0,0,0)
assert(gv.x == 0)
assert(gv.y == 0)
assert(gv.z == 0)

gv = oapi.get_groundvector(earth,0,0,1)
assert(gv.x == 0)
assert(gv.y == 0)
assert(gv.z == 0)

-- tangential speed at the equator
gv = oapi.get_groundvector(earth,0,0,2)
assert(gv.x == 0)
assert(gv.y == 0)
assert(almost_equal(gv.z, 464.58137217991))

-- tangential speed at the pole
gv = oapi.get_groundvector(earth,0,math.pi/2,2)
assert(gv.x == 0)
assert(gv.y == 0)
assert(math.abs(gv.z) < 1e-13 )

pass()

-- ---------------------------------------------------
-- FINAL RESULT
-- ---------------------------------------------------
os.remove(fname_root) -- cleanup the mess we left...

add_line("=== All tests passed ===")
oapi.exit(0)

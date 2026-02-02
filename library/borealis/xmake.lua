add_rules("mode.debug", "mode.release")

option("platfrom")
    set_default("desktop")
    set_showmenu(true)
option_end()

option("window")
    set_default("glfw")
    set_showmenu(true)
option_end()

option("driver")
    set_default("opengl")
    set_showmenu(true)
option_end()

option("winrt")
    set_default(false)
    set_showmenu(true)
option_end()

option("unity_build")
    set_default(false)
    set_showmenu(true)
option_end()

if is_plat("windows") then
    set_languages("c++20")
    add_cxflags("/utf-8")
    add_includedirs("library/include/compat")
    if is_mode("release") then
        set_optimize("faster")
    end
else
    set_languages("c++17")
end
if is_plat("mingw") then
    add_defines("WINVER=0x0605")
end


add_requires("tinyxml2")
add_requires("yoga")
add_requires("stb")
add_requires("nanovg")
add_requires("nlohmann_json")
add_requires("fmt")
add_requires("tweeny")

add_defines(
    'BRLS_RESOURCES="./resources/"',
    "YG_ENABLE_EVENTS"
)

if get_config("driver") == "opengl" then
    add_requires("glad")
end

if get_config("window") == "sdl" then
    if get_config("winrt") then
        add_requires("sdl2", {configs={shared=true,winrt=true}})
        add_requires("cppwinrt")
    else
        add_requires("sdl2")
    end
elseif get_config("window") == "glfw" then
    add_requires("xfangfang_glfw")
end

target("borealis")
    set_kind("static")
    -- set_kind("shared")
    add_includedirs("library/include")
    add_includedirs("library/include/borealis/extern")
    add_files("library/lib/core/**.cpp")
    add_files("library/lib/views/**.cpp")
    add_files("library/lib/extern/libretro-common/**.c")

    if get_config("window") == "glfw" then
        add_files("library/lib/platforms/glfw/*.cpp")
        add_files("library/lib/platforms/desktop/*.cpp")
        add_packages("xfangfang_glfw")
        add_defines("__GLFW__")
    elseif get_config("window") == "sdl" then
        add_files("library/lib/platforms/sdl/*.cpp")
        add_files("library/lib/platforms/desktop/*.cpp")
        add_packages("sdl2")
        if get_config("winrt") then
            add_defines("SDL_VIDEO_DRIVER_WINRT")
            add_packages("cppwinrt")
        end
        add_defines("__SDL2__")
    end
    local driver = get_config("driver")
    if driver == "metal" then
        add_defines("BOREALIS_USE_METAL")
        add_frameworks("Metal", "MetalKit", "QuartzCore")
        add_files("library/lib/platforms/glfw/driver/metal.mm")
        add_links("nanovg_metal")
    elseif driver == "opengl" then
        add_defines("BOREALIS_USE_OPENGL")
        add_packages("glad")
    elseif driver == "d3d11" then
        add_files("library/lib/platforms/driver/d3d11.cpp", {unity_group = "d3d11"})
        if get_config("winrt") then
            add_defines("__WINRT__=1")
        end
        add_defines("BOREALIS_USE_D3D11")
        add_syslinks("d3d11")
        -- d3d11 可以开启可变帧率
        -- add_defines("__ALLOW_TEARING__=1")
    end
    add_packages("tinyxml2", "nlohmann_json", "nanovg", "fmt", "tweeny", "yoga", "stb")
    add_defines("BOREALIS_USE_STD_THREAD")
    if is_plat("macosx") then
        add_frameworks("SystemConfiguration", "CoreWlan")
        add_files("library/lib/platforms/desktop/desktop_darwin.mm")
    end
    if get_config("unity_build") then
        add_rules("c++.unity_build", {batchsize = 16})
        add_rules("c.unity_build", {batchsize = 16})
    end


target("demo")
    set_default(false)
    add_includedirs("library/include")
    add_includedirs("library/include/borealis/extern")
    add_includedirs("demo/include")
    add_files("demo/src/*.cpp")
    add_files("demo/src/*/*.cpp")
    add_packages("tinyxml2", "nanovg", "fmt", "tweeny", "yoga")
    if get_config("window") == "sdl" then
        add_packages("sdl2")
    end
    add_deps("borealis")
    on_config(function (target)
        local cmakefile = io.readfile("CMakeLists.txt")
        target:set("configvar", "VERSION_MAJOR", string.match(cmakefile, "set%(VERSION_MAJOR \"(%d)\"%)"))
        target:set("configvar", "VERSION_MINOR", string.match(cmakefile, "set%(VERSION_MINOR \"(%d)\"%)"))
        target:set("configvar", "VERSION_ALTER", string.match(cmakefile, "set%(VERSION_ALTER \"(%d)\"%)"))
        target:set("configvar", "VERSION_BUILD", "$(shell git rev-list --count --all)")
    end)
    if get_config("winrt") then
        add_defines("__WINRT__=1")
        add_syslinks("WindowsApp")
        add_configfiles("winrt/AppxManifest.xml.in")
        after_build(function (target)
            import("uwp")(target)
        end)
    end
    if is_plat("windows", "mingw") then
        add_syslinks("Wlanapi", "iphlpapi", "Ws2_32")
    end
    if is_plat("windows") then
        add_ldflags("/manifest:EMBED", "/MANIFESTINPUT:demo/resource.manifest", {force = true})
    elseif is_plat("mingw") then
        add_ldflags("-static")
    end
    if is_mode("release") then
        if is_plat("mingw") then
            add_cxflags("-Wl,--subsystem,windows", {force = true})
            add_ldflags("-Wl,--subsystem,windows", {force = true})
        elseif is_plat("windows") then
            add_ldflags("/SUBSYSTEM:WINDOWS", "/ENTRY:mainCRTStartup", {force = true})
        end
    end
    set_rundir("$(projectdir)")
    if get_config("unity_build") then
        add_rules("c++.unity_build", {batchsize = 16})
        add_rules("c.unity_build", {batchsize = 16})
    end

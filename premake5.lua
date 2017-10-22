-- windows genrration:
--     premake5 vs2017 --os=windows

workspace "libepub"
    configurations { "debug", "release" }
    language "C++"
    architecture "x86_64"
    platforms {"linux", "windows"}
    includedirs {"./miniz/", "./tinyxml2/", "./pugixml/src/" }

    filter "configurations:debug"
        symbols "On"
        defines {"DEBUG"}
        targetdir ("./debug/")

    filter "configurations:release"
        defines { "NDEBUG" }
        optimize "On"
        targetdir ("./release/")

    filter "platforms:windows"
        system "windows"
        defines {"_CRT_SECURE_NO_WARNINGS"}
        buildoptions {"/std:c++17"}
        
        filter "platforms:linux"
        system "linux"
        objdir ("/tmp/")

    local project_name = "miniz"
    project (project_name)
        location (project_name) 
        language "C"
        kind "StaticLib"
        targetprefix "lib"
        files {(project_name .. "/miniz*.c")}

    project_name = "pugixml"
    project (project_name)
        location (project_name) 
        kind "StaticLib"
        targetprefix "lib"
        files { (project_name .. "/src/pugixml.hpp"), (project_name .. "/src/pugiconfig.hpp"), (project_name .. "/src/pugixml.cpp") }
        
    project_name = "libepub"
    project (project_name)
        location (project_name) 
        kind "StaticLib"
        targetprefix ""
        dependson {"miniz, tinyxml2"}
        files {(project_name .. "/**.cpp"), (project_name .. "/**.h")}        
        links {"miniz", "pugixml"}
        
    project_name = "tests"
    project (project_name)
        location (project_name) 
        kind "ConsoleApp"
        dependson {"libepub"}
        includedirs {"./libepub/"}
        files {(project_name .. "/**.cpp"), (project_name .. "/**.h")}
        
        links {"libepub", "miniz"}
            
workspace "Candle"
	architecture "x64"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include directories relative to root folder (solution directory)
IncludeDir = {}
IncludeDir["GLFW"] = "Candle/vendor/GLFW/include"

-- Include glfw premake file in this premake file
include "Candle/vendor/GLFW"

project "Candle"
	location "Candle"
	kind "SharedLib"
	language "C++"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "candlepch.h"
	pchsource "Candle/src/candlepch.cpp"

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
	}

	includedirs
	{
		"%{prj.name}/src",
		"%{prj.name}/vendor/spdlog/include",
		"%{IncludeDir.GLFW}"
	}

	links
	{
		"GLFW",
		"opengl32.lib"
	}

	filter "system:windows"
		cppdialect "C++17"
		staticruntime "On"
		systemversion "latest"

		defines
		{
			"CANDLE_PLATFORM_WINDOWS",
			"CANDLE_BUILD_DLL"
		}

		postbuildcommands
		{
			("{COPY} %{cfg.buildtarget.relpath} ../bin/" ..outputdir.. "/Sandbox")
		}
	
	filter "configurations:Debug"
		defines 
		{
			"CANDLE_DEBUG",
			"CANDLE_ENABLE_ASSERTS"
		}
		symbols "On"
	
	filter "configurations:Release"
		defines "CANDLE_RELEASE"
		optimize "On"

	filter "configurations:Dist"
		defines "CANDLE_DIST"
		optimize "On"

project "Sandbox"
	location "Sandbox"
	kind "ConsoleApp"
	language "C++"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
	}

	includedirs
	{
		"Candle/vendor/spdlog/include",
		"Candle/src"
	}

	links
	{
		"Candle"
	}

	filter "system:windows"
		cppdialect "C++17"
		staticruntime "On"
		systemversion "latest"

		defines
		{
			"CANDLE_PLATFORM_WINDOWS"
		}
	
	filter "configurations:Debug"
		defines
		{
			"CANDLE_DEBUG",
			"CANDLE_ENABLE_ASSERTS"
		}
		symbols "On"
	
	filter "configurations:Release"
		defines "CANDLE_RELEASE"
		optimize "On"

	filter "configurations:Dist"
		defines "CANDLE_DIST"
		optimize "On"
project "Candle"
   kind "StaticLib"
   language "C++"
   cppdialect "C++23"

   targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
   objdir ("%{wks.location}/bin/int/" .. outputdir .. "/%{prj.name}")

   pchheader "cdlpch.h"
   pchsource "src/cdlpch.cpp"

   multiprocessorcompile "On"

   includedirs 
   {
      "src",
      "vendor/SDL3/SDL3/include",
      "vendor/glm",
      "vendor/imgui/imgui",
      "vendor/imgui/imgui/backends",
   }

   files 
   { 
      "src/**.h",
      "src/**.inl",
      "src/**.cpp"
   }

   links
   {
      "SDL3",
      "imgui",
   }

   defines
   {
      "CDL_ENGINE",
   }

   filter "system:windows"
      systemversion "latest"

   filter "configurations:Debug"
      defines { "CDL_DEBUG", "CDL_ENABLE_ASSERTS" }
      symbols "On"

   filter "configurations:Release"
      defines { "CDL_RELEASE", "CDL_ENABLE_ASSERTS" }
      optimize "On"

   filter "configurations:Profile"
      defines { "CDL_PROFILE", "CDL_RELEASE" }
      optimize "On"
      symbols "On"

   filter "configurations:Dist"
      defines { "CDL_DIST" }
      runtime "Release"
      staticruntime "On"
      optimize "On"
      symbols "Off"

   filter {}
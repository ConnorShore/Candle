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
      "vendor/tracy/tracy/public",
      "vendor/VMA/VMA/include",
      "vendor/stb/stb",
      "vendor/ktx/ktx/include",
      "$(VULKAN_SDK)/Include",
      "$(VULKAN_SDK)/Source/SPIRV-Reflect",
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
      "tracy",
      "SPIRV-Reflect",
      "VMA",
      "stb",
      "ktx",
   }

   defines
   {
      "CDL_ENGINE",
      -- Per-project, not workspace-wide: SDL's headers read KHRONOS_STATIC, and SDL defines STBI_NO_STDIO itself.
      "KHRONOS_STATIC",    -- ktx is a static lib; without it every KTX_API function is declared dllimport
      "STBI_NO_STDIO",     -- must match vendor/stb's build, which compiles out the path-taking loaders
   }

   filter "system:windows"
      systemversion "latest"

   filter "configurations:Debug"
      defines { "CDL_DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "CDL_RELEASE" }
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
-- stb single-file libraries built as a static lib from the Candle-owned StbImplementation.cpp.
-- Relative paths resolve against THIS script's directory, so the pristine
-- submodule at Candle/vendor/stb is addressed as "stb/...".
-- stb is header-only; this project exists so each *_IMPLEMENTATION is compiled exactly once, outside the pch.
-- Include it as <stb_image.h>, with STBI_NO_STDIO defined like here, or the header declares loaders this lib lacks.
-- Not workspace-wide: SDL compiles its own STB_IMAGE_STATIC copy and defines it itself.
project "stb"
   kind "StaticLib"
   language "C++"
   cppdialect "C++17"

   targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
   objdir    ("%{wks.location}/bin/int/" .. outputdir .. "/%{prj.name}")

   multiprocessorcompile "On"

   includedirs
   {
      "stb",
   }

   files
   {
      "stb/stb_image.h",
      "StbImplementation.cpp",
   }

   -- Drops the path-taking loaders, which bypass Platform::ReadFile and take ANSI paths; load from memory instead.
   defines { "STBI_NO_STDIO" }

   filter "system:windows"
      systemversion "latest"

   -- Runtime library must match Candle's per-config settings exactly, or the
   -- final link fails with LNK2038 RuntimeLibrary mismatch.
   filter "configurations:Debug"
      runtime "Debug"
      symbols "On"

   filter "configurations:Release"
      runtime "Release"
      optimize "On"

   filter "configurations:Profile"
      runtime "Release"
      optimize "On"
      symbols "On"

   -- STBI_ASSERT is assert(), so NDEBUG turns it off where CDL_ENABLE_ASSERTS is off too.
   filter "configurations:Dist"
      defines { "NDEBUG" }
      runtime "Release"
      staticruntime "On"
      optimize "On"
      symbols "Off"

   filter {}

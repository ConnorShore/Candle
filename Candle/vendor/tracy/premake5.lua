-- Tracy profiler client built as a static lib from its single amalgamated TU.
-- Relative paths resolve against THIS script's directory, so the pristine
-- submodule at Candle/vendor/tracy is addressed as "tracy/...".
-- TRACY_ENABLE is set workspace-wide for Profile only; in every other config
-- TracyClient.cpp compiles to nothing and this lib is empty.
-- System libs (ws2_32, dbghelp, ...) arrive via Tracy's own #pragma comment(lib).
project "tracy"
   kind "StaticLib"
   language "C++"
   cppdialect "C++17"

   targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
   objdir    ("%{wks.location}/bin/int/" .. outputdir .. "/%{prj.name}")

   multiprocessorcompile "On"

   includedirs
   {
      "tracy/public",
   }

   files
   {
      "tracy/public/TracyClient.cpp",
   }

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

   filter "configurations:Dist"
      runtime "Release"
      staticruntime "On"
      optimize "On"
      symbols "Off"

   filter {}

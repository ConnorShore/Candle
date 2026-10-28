-- KTX-Software's read-only libktx (upstream's ktx_read target) built as a static lib.
-- Relative paths resolve against THIS script's directory, so the pristine
-- submodule at Candle/vendor/ktx is addressed as "ktx/...".
-- Include it as <ktx.h>, with KHRONOS_STATIC defined, or every KTX_API function is declared dllimport.

-- Upstream's KTX_MAIN_SRC is a plain path list, so parsing it lets a submodule bump pick up new files unedited.
local function ktxReadSources()
   local cmakeLists = _SCRIPT_DIR .. "/ktx/CMakeLists.txt"

   local file = io.open(cmakeLists, "r")
   if not file then
      error("ktx: cannot open " .. cmakeLists ..
            "\n  Is the submodule initialized? git submodule update --init")
   end
   local cmake = file:read("*a")
   file:close()

   local list = cmake:match("set%(%s*KTX_MAIN_SRC(.-)%)")
   if not list then
      error("ktx: no set(KTX_MAIN_SRC ...) in " .. cmakeLists ..
            "\n  Upstream layout changed -- find where libktx's sources are listed now.")
   end

   -- ASTC decode needs astc-encoder and spawns its own threads; ETC unpack serves only GL upload, under a GL-only licence.
   local excluded = { ["lib/astc_codec.cpp"] = true, ["lib/etcunpack.cxx"] = true }

   -- texture1.c is appended by upstream's KTX_FEATURE_KTX1 block rather than listed in KTX_MAIN_SRC.
   local sources = { "ktx/lib/texture1.c", "ktx/lib/texture1.h" }
   for path in list:gmatch("%S+") do
      if not excluded[path] then
         sources[#sources + 1] = "ktx/" .. path
      end
   end

   table.sort(sources)
   return sources
end

project "ktx"
   kind "StaticLib"
   language "C++"
   cdialect "C11"
   cppdialect "C++17"

   targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
   objdir    ("%{wks.location}/bin/int/" .. outputdir .. "/%{prj.name}")

   multiprocessorcompile "On"

   -- other_include also carries upstream's own glm, VMA and stb copies; it must never reach a Candle project.
   includedirs
   {
      "ktx/include",
      "ktx/external",
      "ktx/external/basisu/transcoder",
      "ktx/external/basisu/zstd",
      "ktx/utils",
      "ktx/other_include",
   }

   files(ktxReadSources())

   -- Upstream's ktx_read settings: libktx parses the KTX2 container itself, so basisu's own KTX2 reader is compiled out.
   defines
   {
      "KHRONOS_STATIC",
      "LIBKTX",
      "BASISD_SUPPORT_FXT1=0",
      "BASISD_SUPPORT_KTX2=0",
      "BASISD_SUPPORT_KTX2_ZSTD=0",
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

   -- libktx and basisu use assert(), so NDEBUG turns them off where CDL_ENABLE_ASSERTS is off too.
   filter "configurations:Dist"
      defines { "NDEBUG" }
      runtime "Release"
      staticruntime "On"
      optimize "On"
      symbols "Off"

   filter {}

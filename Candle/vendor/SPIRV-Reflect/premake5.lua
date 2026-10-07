-- SPIRV-Reflect built as a static lib from the copy the Vulkan SDK ships, not a submodule.
-- That keeps it the same version as the slangc that produced the SPIR-V it parses and the
-- Vulkan headers Candle builds against, so an SDK bump moves all three together and a pinned
-- snapshot can never silently lag the compiler.
-- Paths are emitted as $(VULKAN_SDK), like Candle's Vulkan includedirs, so MSBuild resolves them.
local sdk = os.getenv("VULKAN_SDK")
if not sdk or not os.isfile(sdk .. "/Source/SPIRV-Reflect/spirv_reflect.c") then
   error("SPIRV-Reflect: %VULKAN_SDK%/Source/SPIRV-Reflect/spirv_reflect.c not found. "
      .. "Install the Vulkan SDK from https://vulkan.lunarg.com/sdk/home and open a new terminal.")
end

project "SPIRV-Reflect"
   kind "StaticLib"
   language "C"

   targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
   objdir    ("%{wks.location}/bin/int/" .. outputdir .. "/%{prj.name}")

   multiprocessorcompile "On"

   files
   {
      "$(VULKAN_SDK)/Source/SPIRV-Reflect/spirv_reflect.h",
      "$(VULKAN_SDK)/Source/SPIRV-Reflect/spirv_reflect.c",
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

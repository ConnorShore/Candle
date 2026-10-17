-- Vulkan Memory Allocator built as a static lib from the Candle-owned VmaImplementation.cpp.
-- Relative paths resolve against THIS script's directory, so the pristine
-- submodule at Candle/vendor/VMA is addressed as "VMA/...".
-- VMA is header-only; this project exists so VMA_IMPLEMENTATION is compiled exactly once, outside the pch.
-- Include it as <vk_mem_alloc.h>, never <vma/vk_mem_alloc.h>: that is the SDK's copy, which can differ from this one.
-- The public structs are laid out by macros derived from the Vulkan headers, so every TU must use the same ones.
project "VMA"
   kind "StaticLib"
   language "C++"
   cppdialect "C++17"

   targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
   objdir    ("%{wks.location}/bin/int/" .. outputdir .. "/%{prj.name}")

   multiprocessorcompile "On"

   includedirs
   {
      "VMA/include",
      "$(VULKAN_SDK)/Include",
   }

   files
   {
      "VMA/include/vk_mem_alloc.h",
      "VmaImplementation.cpp",
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

   -- VMA_ASSERT is assert(), so NDEBUG turns it off where CDL_ENABLE_ASSERTS is off too.
   filter "configurations:Dist"
      defines { "NDEBUG" }
      runtime "Release"
      staticruntime "On"
      optimize "On"
      symbols "Off"

   filter {}

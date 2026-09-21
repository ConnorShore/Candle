-- Dear ImGui (docking branch) built as a static lib.
-- Relative paths resolve against THIS script's directory, so the pristine
-- submodule at Candle/vendor/imgui is addressed as "imgui/...". Keeping the
-- script out of the submodule means the generated .vcxproj lands here rather
-- than dirtying imgui's working tree.
project "imgui"
   kind "StaticLib"
   language "C++"
   cppdialect "C++17"

   targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
   objdir    ("%{wks.location}/bin/int/" .. outputdir .. "/%{prj.name}")

   multiprocessorcompile "On"

   includedirs
   {
      "imgui",
      "imgui/backends",
      "SDL3/include",
      "$(VULKAN_SDK)/Include",
   }

   files
   {
      "imgui/*.h",
      "imgui/*.cpp",
      "imgui/backends/imgui_impl_sdl3.*",
      "imgui/backends/imgui_impl_vulkan.*",
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

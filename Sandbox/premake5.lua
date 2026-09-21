project "Sandbox"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++23"

   targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
   objdir ("%{wks.location}/bin/int/" .. outputdir .. "/%{prj.name}")
   debugdir "%{wks.location}"

   multiprocessorcompile "On"

   files
   { 
      "src/**.h",
      "src/**.cpp" 
   }

   includedirs 
   {
      "src",
      "vendor/ImGuizmo",
      "vendor/imgui-node-editor",
      "%{wks.location}/Candle/src",
      "%{wks.location}/Candle/vendor/glm",
      "%{wks.location}/Candle/vendor/SDL3/SDL3/include",
      "%{wks.location}/Candle/vendor/imgui/imgui",
      "%{wks.location}/Candle/vendor/imgui/imgui/backends",
   }

   links 
   {
      "Candle",
   }

   filter "system:windows"
      -- Required by the static SDL3 build; they belong on the final executable
      -- rather than inside SDL3.lib. Vulkan comes from the installed SDK.
      libdirs { "$(VULKAN_SDK)/Lib" }
      links
      {
         "user32", "gdi32", "winmm", "imm32", "ole32", "oleaut32",
         "version", "uuid", "advapi32", "setupapi", "shell32",
         "vulkan-1",
      }

   filter {}

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
      kind "WindowedApp"
      entrypoint "mainCRTStartup"

   filter {}
workspace "Candle"
   architecture "x86_64"
   configurations { "Debug", "Release", "Profile", "Dist" }
   cppdialect "C++23"
   startproject "Sandbox"

   -- Workspace-wide: both change type definitions, so every TU including glm or vulkan.hpp must agree.
   defines { "GLM_FORCE_CTOR_INIT", "VULKAN_HPP_NO_STRUCT_CONSTRUCTORS" }

   filter "configurations:not Dist"
      defines { "CDL_ENABLE_ASSERTS" }

   -- Workspace-wide so every TU including Tracy.hpp agrees; a mismatch is silent ODR breakage.
   -- No TRACY_ON_DEMAND: it drops everything before the viewer connects, which hides startup.
   filter "configurations:Profile"
      defines { "TRACY_ENABLE" }
   filter {}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Vendors"
   include "Candle/vendor/SDL3"
   include "Candle/vendor/imgui"
   include "Candle/vendor/tracy"
   include "Candle/vendor/SPIRV-Reflect"
group ""

include "Candle"
include "Sandbox"
include "Candle-Test"
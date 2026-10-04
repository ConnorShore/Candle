workspace "Candle"
   architecture "x86_64"
   configurations { "Debug", "Release", "Profile", "Dist" }
   cppdialect "C++23"
   startproject "Sandbox"

   defines { "GLM_FORCE_CTOR_INIT" }

   filter "configurations:not Dist"
      defines { "CDL_ENABLE_ASSERTS" }

   -- Workspace-wide so every TU including Tracy.hpp agrees; a mismatch is silent ODR breakage.
   -- ON_DEMAND stops Tracy buffering events without limit while no viewer is connected.
   filter "configurations:Profile"
      defines { "TRACY_ENABLE", "TRACY_ON_DEMAND" }
   filter {}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Dependencies"
   include "Candle/vendor/SDL3"
   include "Candle/vendor/imgui"
   include "Candle/vendor/tracy"
group ""

include "Candle"
include "Sandbox"
include "Candle-Test"
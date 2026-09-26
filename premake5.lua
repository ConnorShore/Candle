workspace "Candle"
   architecture "x86_64"
   configurations { "Debug", "Release", "Profile", "Dist" }
   cppdialect "C++23"
   startproject "Sandbox"

   defines { "GLM_FORCE_CTOR_INIT" }

   filter "configurations:not Dist"
      defines { "CDL_ENABLE_ASSERTS" }
   filter {}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Dependencies"
   include "Candle/vendor/SDL3"
   include "Candle/vendor/imgui"
group ""

include "Candle"
include "Sandbox"
include "Candle-Test"
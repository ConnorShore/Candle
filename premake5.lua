workspace "Candle"
   architecture "x86_64"
   configurations { "Debug", "Release", "Profile", "Dist" }
   cppdialect "C++23"
   startproject "Sandbox"

   defines { "GLM_FORCE_CTOR_INIT" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Dependencies"
   include "Candle/vendor/SDL3"
   include "Candle/vendor/imgui"
group ""

include "Candle"
include "Sandbox"
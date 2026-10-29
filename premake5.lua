workspace "Candle"
   architecture "x86_64"
   configurations { "Debug", "Release", "Profile", "Dist" }
   cppdialect "C++23"
   startproject "Sandbox"

   -- Workspace-wide: each changes declarations or inline definitions, so every TU including glm or vulkan.hpp must agree.
   -- OUT_OF_DATE_AS_SUCCESS makes acquire/present return eErrorOutOfDateKHR instead of throwing it.
   -- DEPTH_ZERO_TO_ONE makes glm's projections output Vulkan's [0, 1] depth range instead of OpenGL's [-1, 1].
   defines { "GLM_FORCE_CTOR_INIT", "GLM_FORCE_DEPTH_ZERO_TO_ONE", "VULKAN_HPP_NO_STRUCT_CONSTRUCTORS", "VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS" }

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
   include "Candle/vendor/VMA"
   include "Candle/vendor/stb"
   include "Candle/vendor/ktx"
group ""

include "Candle"
include "Sandbox"
include "Candle-Test"
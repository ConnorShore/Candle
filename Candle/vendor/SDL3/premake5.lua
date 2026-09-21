
-- Helper function to extract SDL source files from the VisualC project
-- This ensures that only the files actually compiled by SDL on Windows are included,
-- avoiding platform-specific issues and unnecessary files.
local function sdlWindowsSources()
   local vcxproj = _SCRIPT_DIR .. "/SDL3/VisualC/SDL/SDL.vcxproj"

   local file = io.open(vcxproj, "r")
   if not file then
      error("SDL3: cannot open " .. vcxproj ..
            "\n  Is the submodule initialized? git submodule update --init --recursive")
   end
   local xml = file:read("*a")
   file:close()

   local sources, seen = {}, {}
   for include in xml:gmatch('Include="([^"]+)"') do
      local rel = include:gsub("\\", "/"):match("^%.%./%.%./src/(.+)$")
      if rel and (rel:match("%.c$") or rel:match("%.cpp$")) and not seen[rel] then
         seen[rel] = true
         sources[#sources + 1] = "SDL3/src/" .. rel
      end
   end

   if #sources == 0 then
      error("SDL3: no sources found in " .. vcxproj ..
            "\n  Upstream layout changed -- check whether VisualC/ still exists.")
   end

   table.sort(sources)
   return sources
end

project "SDL3"
   kind "StaticLib"
   language "C++"

   targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
   objdir    ("%{wks.location}/bin/int/" .. outputdir .. "/%{prj.name}")

   multiprocessorcompile "On"

   includedirs
   {
      "SDL3/include",
      "SDL3/include/build_config",
      "SDL3/src",
   }

   files(sdlWindowsSources())
   files
   {
      "SDL3/include/**.h",
      "SDL3/src/**.h",
   }

   defines
   {
      "SDL_AUDIO_DISABLED",    -- Candle will own audio directly
      "SDL_CAMERA_DISABLED",   -- webcam capture, unused
      "SDL_GPU_DISABLED",      -- SDL's GPU abstraction; Candle targets Vulkan directly
      "SDL_POWER_DISABLED",    -- system battery info, unrelated to window/input
      "SDL_RENDER_DISABLED",   -- SDL's 2D renderer; Candle targets Vulkan directly
   }

   filter "system:windows"
      systemversion "latest"

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

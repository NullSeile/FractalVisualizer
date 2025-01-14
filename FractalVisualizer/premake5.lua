project "FractalVisualizer"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"

	targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
	objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

	files {
		"src/**.h",
		"src/**.cpp",
		"vendor/**.h",
		"vendor/**.cpp"
	}

	includedirs {
		"../OpenGL-Core/vendor/spdlog/include",
		"../OpenGL-Core/src",
		"../OpenGL-Core/vendor",
		"../OpenGL-Core/vendor/glm",
		"../OpenGL-Core/vendor/Glad/include",
		"../OpenGL-Core/vendor/imgui",
		"../OpenGL-Core/vendor/implot",
		"vendor"
	}

	links {
		"OpenGL-Core"
	}

	postbuildcommands {
		"{RMDIR} %{cfg.targetdir}/assets",
		"{COPYDIR} ./assets %{cfg.targetdir}/assets",
		"{COPYFILE} ./imgui.ini %{cfg.targetdir}/imgui.ini"
	}

	filter "system:windows"
		systemversion "latest"

		defines {
			"GLCORE_PLATFORM_WINDOWS"
		}

	filter "configurations:Debug"
		kind "ConsoleApp"
		defines "GLCORE_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		kind "WindowedApp"
		defines "GLCORE_RELEASE"
		runtime "Release"
        optimize "on"

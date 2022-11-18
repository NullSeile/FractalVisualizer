#pragma once

#include <GLCore.h>
#include <GLCoreUtils.h>

#include "FractalVisualizer.h"
#include "ColorFunction.h"

struct SmoothZoomData
{
	double t = 1.0;
	double start_radius;
	double target_radius;
	ImVec2 target_pos;
};

struct VideoRenderData
{
	std::string fileName = "output.mp4";
	FractalVisualizer* fractal = nullptr;
	glm::ivec2 resolution = { 1920, 1080 };
	float duration = 10.f;
	int fps = 30;
	int steps_per_frame = 10;

	glm::dvec2 initial_center = { 0, 0 };
	double initial_radius = 1.0;

	glm::dvec2 final_center = { 0, 0 };
	double final_radius = 1e-5;

	FILE* ffmpeg;
	BYTE* pixels;

	size_t steps;
	double delta;
	ImVec2 initial_coords;
	ImVec2 final_coords;

	int current_iter = 0;

	glm::dvec2 stored_center;
	double stored_radius;
};

class MainLayer : public GLCore::Layer
{
public:
	MainLayer();
	~MainLayer();

	virtual void OnAttach() override;
	//virtual void OnDetach() override;
	virtual void OnUpdate(GLCore::Timestep ts) override;
	virtual void OnImGuiRender() override;

private:

	struct ColorPreview
	{
		GLuint shaderID;
		GLuint textureID;

		ColorPreview(GLuint texture, GLuint shader) : textureID(texture), shaderID(shader) {}
	};

	void RefreshColorFunctions();
	bool m_ShouldRefreshColors = false;

	void ShowHelpWindow();
	void ShowMandelbrotWindow();
	void ShowJuliaWindow();
	void ShowControlsWindow();
	void ShowRenderWindow();

	bool m_VSync = true;

	float m_FrameRate = 0;
	int m_ResolutionPercentage = 100;
	int m_ItersPerSteps = 100;
	int m_StepsPerFrame = 1;
	int m_MaxEpochs = 100;
	int m_FadeThreshold = 0;
	bool m_SmoothColor = true;
	bool m_SmoothZoom = true;

	bool m_ShouldRenderVideo = false;
	VideoRenderData m_VideoRenderData;

	std::vector<ColorPreview> m_ColorsPreview;
	std::vector<ColorFunction> m_Colors;
	size_t m_SelectedColor = 0;
	ImColor m_IterationsColor = { 1.f, 1.f, 1.f, 0.9f };
	bool m_ShowHelp = true;
	
	SmoothZoomData m_MandelbrotZoomData;
	bool m_MandelbrotMinimized = true;
	//glm::dvec2 m_MandelbrotZ = { 0, 0 };
	std::string m_MandelbrotSrcPath;
	FractalVisualizer m_Mandelbrot;

	SmoothZoomData m_JuliaZoomData;
	bool m_JuliaMinimized = true;
	glm::dvec2 m_JuliaC = { 0, 0 };
	std::string m_JuliaSrcPath;
	FractalVisualizer m_Julia;
};


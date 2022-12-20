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

enum class State
{
	Exploring = 0,
	Rendering,
	Previewing
};


typedef std::pair<FloatUniform*, float> UniformRenderData;

struct RenderKeyFrame
{
	glm::dvec2 center = { 0, 0 };
	double radius = 1.0;
	std::vector<UniformRenderData> uniforms;
};


struct VideoRenderData
{
	void Prepare(const std::string& path, const FractalVisualizer& other);
	void UpdateIter(float t);
	void SetColorFunction(const std::shared_ptr<ColorFunction>& new_color);
	void FillKeyFrameUniforms(RenderKeyFrame& key);

	std::string fileName = "output.mp4";
	std::unique_ptr<FractalVisualizer> fract;
	std::shared_ptr<ColorFunction> color;

	glm::ivec2 resolution = { 1920, 1080 };
	float duration = 10.f;
	int fps = 30;
	int steps_per_frame = 10;

	FILE* ffmpeg;
	BYTE* pixels;

	size_t steps;

	int current_iter = 0;

	std::vector<RenderKeyFrame> keyFrames = { RenderKeyFrame(), RenderKeyFrame() };
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

	State m_State = State::Exploring;

	bool m_ShouldUpdatePreview = false;
	float m_PreviewT = 0.0;

	VideoRenderData m_VideoRenderData;

	std::vector<ColorPreview> m_ColorsPreview;
	std::vector<std::shared_ptr<ColorFunction>> m_Colors;
	std::vector<const char*> m_ColorsName;
	size_t m_SelectedColor = 0;
	ImColor m_IterationsColor = { 1.f, 1.f, 1.f, 0.9f };
	bool m_ShowHelp = true;
	bool m_ShowDemo = false;

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


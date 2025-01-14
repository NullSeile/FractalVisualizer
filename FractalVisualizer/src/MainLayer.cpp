#include "MainLayer.h"

#include <iomanip>
#include <filesystem>
#include <fstream>
#include <format>
#include <algorithm>
#include <functional>
#include <ranges>

#include "LayerUtils.h"
#include <imgui_internal.h>
#include <IconsMaterialDesign.h>


static glm::uvec2 previewSize = { 100, 1 };

template<typename T, glm::qualifier Q>
std::ostream& operator<<(std::ostream& os, const glm::vec<2, T, Q>& vec)
{
	os << "(" << vec.x << ", " << vec.y << ")";
	return os;
}

MainLayer::MainLayer()
	: m_MandelbrotSrcPath("assets/mandelbrot.glsl")
	, m_Mandelbrot(m_MandelbrotSrcPath)
	, m_JuliaSrcPath("assets/julia.glsl")
	, m_Julia(m_JuliaSrcPath)
	, m_SelectedFractal(&m_Mandelbrot)
{
	RefreshColorFunctions();

	GLCore::Application::Get().GetWindow().SetVSync(m_VSync);

	m_Mandelbrot.SetSetColor(m_SetColor);
	m_Julia.SetSetColor(m_SetColor);

	m_Mandelbrot.SetIterationsPerFrame(m_ItersPerSteps);
	m_Julia.SetIterationsPerFrame(m_ItersPerSteps);

	m_Mandelbrot.SetMaxEpochs(m_MaxEpochs);
	m_Julia.SetMaxEpochs(m_MaxEpochs);

	m_Mandelbrot.SetSmoothColor(m_SmoothColor);
	m_Julia.SetSmoothColor(m_SmoothColor);

	m_Mandelbrot.SetEqExponent(m_EqExponent);
	m_Julia.SetEqExponent(m_EqExponent);

	m_Mandelbrot.SetCenter({ -0.5, 0 });
	m_Julia.SetRadius(1.3);

	m_VideoRenderer.SetColorFunction(GetColorFunction(m_SelectedColor));
	m_VideoRenderer.Prepare(m_MandelbrotSrcPath, m_Mandelbrot);

	m_MandelbrotZoomData.start_radius = m_Mandelbrot.GetRadius();
	m_MandelbrotZoomData.target_radius = m_Mandelbrot.GetRadius();

	m_JuliaZoomData.start_radius = m_Julia.GetRadius();
	m_JuliaZoomData.target_radius = m_Julia.GetRadius();

	UpdatePlots();
}

MainLayer::~MainLayer()
{
	for (auto prev : m_ColorsPreview)
	{
		glDeleteTextures(1, &prev.textureID);
		glDeleteProgram(prev.shaderID);
	}
}

std::shared_ptr<ColorFunction> MainLayer::GetColorFunction(size_t index) const
{
	if (m_ColorsError[index])
		return ColorFunction::Default;
	else
		return m_Colors[index];
}

std::shared_ptr<ColorFunction> MainLayer::SetColorFunction(size_t index)
{
	m_SelectedColor = index;
	auto color = GetColorFunction(index);
	m_Mandelbrot.SetColorFunction(color);
	m_Julia.SetColorFunction(color);
	return color;
}

void MainLayer::OnAttach()
{
	ImGuiStyle& style = ImGui::GetStyle();
	style.FrameRounding = 3.f;
	style.GrabRounding = 3.f;
	style.WindowRounding = 3.f;

	ImGui::StyleColorsClassic();

	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_WindowBg] = ImVec4(0.04f, 0.04f, 0.04f, 0.85f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 0.83f);

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	io.Fonts->AddFontDefault();

	ImFontConfig config;
	config.MergeMode = true;
	config.GlyphOffset = { 0.f, 3.f };
	config.GlyphMinAdvanceX = 13.0f; // Use if you want to make the icon monospaced
	static const ImWchar icon_ranges[] = { (ImWchar)ICON_MIN_MD, (ImWchar)ICON_MAX_MD, 0 };
	io.Fonts->AddFontFromFileTTF("assets/fonts/MaterialIcons-Regular.ttf", 13.0f, &config, icon_ranges);
}

void MainLayer::OnUpdate(GLCore::Timestep ts)
{
	m_FrameRate = 1 / ts.GetSeconds();

	if (m_ShouldRefreshColors)
	{
		RefreshColorFunctions();
		m_ShouldRefreshColors = false;
	}

	switch (m_State)
	{
	case State::Exploring:
	{
		glUseProgram(m_Julia.GetShader());
		GLint loc = glGetUniformLocation(m_Julia.GetShader(), "i_JuliaC");
		glUniform2d(loc, m_JuliaC.x, m_JuliaC.y);

		for (int i = 0; i < m_StepsPerFrame; i++)
		{
			if (!m_MandelbrotMinimized)
				m_Mandelbrot.Update();

			if (!m_JuliaMinimized)
				m_Julia.Update();
		}

		if (m_ShouldUpdatePreview && !m_PreviewMinimized)
		{
			m_VideoRenderer.UpdateToFractal();
			m_VideoRenderer.UpdateIter(m_PreviewT);
			m_ShouldUpdatePreview = false;
		}
		break;
	}
	case State::Rendering:
	{
		auto& data = m_VideoRenderer;
		auto& i = data.current_iter;

		if (i < data.steps)
		{
			data.UpdateIter(i / (float)(data.steps - 1));

			glBindTexture(GL_TEXTURE_2D, data.fract->GetTexture());
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.pixels);

			fwrite(data.pixels, data.resolution.x * data.resolution.y * 4 * sizeof(BYTE), 1, data.ffmpeg);

			i++;
		}
		else // finished
		{
			m_State = State::Exploring;

			_pclose(data.ffmpeg);
			delete[] data.pixels;
		}

		break;
	}
	default:
		break;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

template<typename T>
static T map(const T& x, const T& x0, const T& x1, const T& y0, const T& y1)
{
	return y0 + ((y1 - y0) / (x1 - x0)) * (x - x0);
}

template<typename T>
double InterpolateArray(const std::vector<T>& array, double t)
{
	double x = map(t, 0.0, 1.0, 0.0, (double)(array.size() - 1));

	if (x == std::floor(x))
		return array[(int)x];

	int n = (int)x;
	double lt = x - n;
	T a = array[n];
	T b = array[n+1];
	return a * (1.0 - lt) + b * lt;
}

#define N_SAMPLES 1000
static ImPlotPoint centerPoints[N_SAMPLES];
#ifdef GLCORE_DEBUG
static ImPlotPoint centerXPoints[N_SAMPLES];
static ImPlotPoint centerYPoints[N_SAMPLES];
static ImPlotPoint radiusPoints[N_SAMPLES];
#endif

void MainLayer::UpdatePlots()
{
	m_VideoRenderer.InvalidateRadius();
	m_VideoRenderer.InvalidateCenter();
	for (size_t n = 0; n < N_SAMPLES; n++)
	{
		double t = n / (double)(N_SAMPLES - 1);

		glm::dvec2 center = m_VideoRenderer.GetCenter(t, 1e-1);
		centerPoints[n] = ImPlotPoint(center.x, center.y);
#ifdef GLCORE_DEBUG
		centerXPoints[n] = ImPlotPoint(t, center.x);
		centerYPoints[n] = ImPlotPoint(t, center.y);

		radiusPoints[n] = ImPlotPoint(t, m_VideoRenderer.GetRadius(t));
#endif
	}
}

template<typename T>
void SortKeyFrames(KeyFrameList<T>& keyFrames)
{
	std::ranges::sort(keyFrames, [](auto a, auto b) { return a->t < b->t; });
}

void MainLayer::OnImGuiRender()
{
	ImGui::DockSpaceOverViewport();

#ifdef GLCORE_DEBUG
	if (ImGui::Begin("Plots"))
	{
		if (ImPlot::BeginPlot("##Center", ImVec2(-1, 700), ImPlotFlags_Equal))
		{
			ImPlot::PlotLine("Spline", &centerPoints[0].x, &centerPoints[0].y, N_SAMPLES, 0, 0, sizeof(ImPlotPoint));
			ImPlot::PlotLine("x", &centerXPoints[0].x, &centerXPoints[0].y, N_SAMPLES, 0, 0, sizeof(ImPlotPoint));
			ImPlot::PlotLine("y", &centerYPoints[0].x, &centerYPoints[0].y, N_SAMPLES, 0, 0, sizeof(ImPlotPoint));

			for (auto p : m_VideoRenderer.centerKeyFrames)
			{
				ImGui::PushID(p.get());

				if (ImPlot::DragPoint(0, &p->val.pos.x, &p->val.pos.y, ImVec4(0.8f, 0.8f, 0.8f, 1.f)))
					UpdatePlots();

				if (ImPlot::DragPoint(1, &p->t, &p->val.pos.x, ImVec4(0.8f, 0.8f, 0.8f, 1.f), 4.f, ImPlotDragToolFlags_Delayed))
				{
					if (p->t < 0.0) p->t = 0.0;
					if (p->t > 1.0) p->t = 1.0;
					UpdatePlots();
					SortKeyFrames(m_VideoRenderer.centerKeyFrames);
				}

				if (ImPlot::DragPoint(2, &p->t, &p->val.pos.y, ImVec4(0.8f, 0.8f, 0.8f, 1.f), 4.f, ImPlotDragToolFlags_Delayed))
				{
					if (p->t < 0.0) p->t = 0.0;
					if (p->t > 1.0) p->t = 1.0;
					UpdatePlots();
					SortKeyFrames(m_VideoRenderer.centerKeyFrames);
				}

				ImGui::PopID();
			}

			ImPlot::EndPlot();
		}
		if (ImPlot::BeginPlot("##Radius", ImVec2(-1, 700)))
		{
			ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
			ImPlot::PlotLine("Radius", &radiusPoints[0].x, &radiusPoints[0].y, N_SAMPLES, 0, 0, sizeof(ImPlotPoint));

			for (auto [i, p] : std::views::enumerate(m_VideoRenderer.radiusKeyFrames))
			{
				ImGui::PushID(p.get());
				if (ImPlot::DragPoint(0, &p->t, &p->val, ImVec4(0.8f, 0.8f, 0.8f, 1.f), 4.f, ImPlotDragToolFlags_Delayed))
				{
					if (p->t < 0.0) p->t = 0.0;
					if (p->t > 1.0) p->t = 1.0;
					UpdatePlots();
					SortKeyFrames(m_VideoRenderer.radiusKeyFrames);
				}
				ImGui::PopID();
			}

			ImPlot::EndPlot();
		}
	}
	ImGui::End();
#endif

#if 1
//#ifdef GLCORE_DEBUG
	if (ImGui::IsKeyPressed(ImGuiKey_D))
		m_ShowDemo = !m_ShowDemo;

	if (m_ShowDemo)
		ImGui::ShowDemoWindow(&m_ShowDemo);

	if (ImGui::IsKeyPressed(ImGuiKey_S))
		m_ShowStyle = !m_ShowStyle;

	if (m_ShowStyle)
	{
		if (ImGui::Begin("Style Editor", &m_ShowStyle))
			ImGui::ShowStyleEditor();
		
		ImGui::End();
	}

	static bool showPlotDemo = false;
	if (ImGui::IsKeyPressed(ImGuiKey_P))
		showPlotDemo = !showPlotDemo;

	if (showPlotDemo)
		ImPlot::ShowDemoWindow(&showPlotDemo);

#endif

	if (ImGui::IsKeyPressed(ImGuiKey_H))
		m_ShowHelp = !m_ShowHelp;

	if (m_ShowHelp)
		ShowHelpWindow();

	ShowControlsWindow();
	ShowPreviewWindow();
	ShowRenderWindow();

	ShowJuliaWindow();
	ShowMandelbrotWindow();
}

void FractalHandleZoom(FractalVisualizer& fract, int resolutionPercentage, float fps, bool smoothZoom, SmoothZoomData& data)
{
	auto& io = ImGui::GetIO();

	if (ImGui::IsWindowHovered() && io.MouseWheel != 0)
	{
		data.start_radius = fract.GetRadius();
		data.target_radius = data.target_radius / std::pow(1.1f, io.MouseWheel);
		data.target_pos = WindowPosToImagePos(ImGui::GetMousePos(), resolutionPercentage);
		data.t = 0.0;

		if (!smoothZoom)
			data.t = 1.0;
		ZoomToScreenPos(fract, data.target_pos, data.target_radius);
	}
	if (smoothZoom)
	{
		if (data.t < 1.0)
		{
			data.t += 8.0 / fps;

			if (data.t >= 1.0)
				data.t = 1.0;

			// (1 - a^x) / (1 - a)
			double a = 0.025;
			double t = (1.0 - std::pow(a, data.t)) / (1.0 - a);

			double radius = data.start_radius * (1 - t) + data.target_radius * t;
			ZoomToScreenPos(fract, data.target_pos, radius);
		}
	}
}

static const float DRAG_GRAB_HALF_SIZE = 4.0f;

#include <implot_internal.h>

ImVec2 FractToWindow(const glm::dvec2 pos, const FractalVisualizer& fract, int resolutionPercentage)
{
	return ImagePosToWindowPos(fract.MapPosToCoords(pos), resolutionPercentage);
}

glm::dvec2 WindowToFract(const ImVec2 pos, const FractalVisualizer& fract, int resolutionPercentage)
{
	return fract.MapCoordsToPos(WindowPosToImagePos(pos, resolutionPercentage));
}

bool DragPoint(int n_id, glm::dvec2* point, const FractalVisualizer& fract, int resolutionPercentage, const ImVec4& col, float radius = 4, ImPlotDragToolFlags flags = 0, bool* out_clicked = nullptr, bool* out_hovered = nullptr, bool* out_held = nullptr) {
    ImGui::PushID("#IMPLOT_DRAG_POINT");

    const bool input = !ImHasFlag(flags, ImPlotDragToolFlags_NoInputs);
    const bool show_curs = !ImHasFlag(flags, ImPlotDragToolFlags_NoCursors);
    const bool no_delay = !ImHasFlag(flags, ImPlotDragToolFlags_Delayed);
    const float grab_half_size = ImMax(DRAG_GRAB_HALF_SIZE, radius);
    const ImVec4 color = ImPlot::IsColorAuto(col) ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : col;
    const ImU32 col32 = ImGui::ColorConvertFloat4ToU32(color);

    // ImVec2 pos = PlotToPixels(*x,*y,IMPLOT_AUTO,IMPLOT_AUTO);
    ImVec2 pos = FractToWindow(*point, fract, resolutionPercentage);
    const ImGuiID id = ImGui::GetCurrentWindow()->GetID(n_id);
    ImRect rect(pos.x-grab_half_size,pos.y-grab_half_size,pos.x+grab_half_size,pos.y+grab_half_size);
    bool hovered = false, held = false;

    ImGui::KeepAliveID(id);
    if (input) {
        bool clicked = ImGui::ButtonBehavior(rect,id,&hovered,&held);
		// LOG_INFO("Clicked: {}", clicked);
        if (out_clicked) *out_clicked = clicked;
        if (out_hovered) *out_hovered = hovered;
        if (out_held)    *out_held    = held;
    }

    bool modified = false;
    if (held && ImGui::IsMouseDragging(0)) {
		*point = WindowToFract(ImGui::GetMousePos(), fract, resolutionPercentage);
        // *x = ImPlot::GetPlotMousePos(IMPLOT_AUTO,IMPLOT_AUTO).x;
        // *y = ImPlot::GetPlotMousePos(IMPLOT_AUTO,IMPLOT_AUTO).y;
        modified = true;
    }

    // PushPlotClipRect();
    ImDrawList& DrawList = *ImGui::GetWindowDrawList();
    if ((hovered || held) && show_curs)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (modified && no_delay)
        pos = FractToWindow(*point, fract, resolutionPercentage);
        // pos = PlotToPixels(*x,*y,IMPLOT_AUTO,IMPLOT_AUTO);
    DrawList.AddCircleFilled(pos, radius, col32);
    // PopPlotClipRect();

    ImGui::PopID();
    return modified;
}

bool MainLayer::ShowCenterKeyFrames(const FractalVisualizer& fract)
{
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	ImVector<ImVec2> points;
	points.reserve(N_SAMPLES);
	for (int i = 0; i < N_SAMPLES; i++)
	{
		auto c = centerPoints[i];
		points.push_back(FractToWindow({c.x, c.y}, fract, m_ResolutionPercentage));
	}
	draw_list->AddPolyline(points.begin(), points.size(), 0xFFFFFFFF, 0, 1.5);

	bool val_changed = false;

	for (auto p : m_VideoRenderer.centerKeyFrames)
	{
		ImGui::PushID(p.get());

		bool hover = false;
		if (DragPoint(0, &(p->val.pos), fract, m_ResolutionPercentage, ImVec4(1, 1, 1, 1), 5, 0, nullptr, &hover))
			val_changed = true;
		
		if (hover && ImGui::IsMouseDoubleClicked(0))
		{
			if (p->val.vel == glm::dvec2(0))
				p->val.vel = glm::dvec2(0.0, fract.GetRadius());
			else
				p->val.vel = glm::dvec2(0);

			val_changed = true;
		}

		auto handle = p->val.pos + 0.1*p->val.vel;
		if (DragPoint(1, &handle, fract, m_ResolutionPercentage, ImVec4(0.8f, 0.8f, 0.8f, 0.9f), 4))
		{
			p->val.vel = (handle - p->val.pos) / 0.1;
			val_changed = true;
		}

		ImGui::PopID();
	}

	return val_changed;
}

void MainLayer::ShowMandelbrotWindow()
{
	ImGuiIO& io = ImGui::GetIO();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
	m_MandelbrotMinimized = !ImGui::Begin("Mandelbrot");

	if (!m_MandelbrotMinimized)
	{
		// Resize
		FractalHandleResize(m_Mandelbrot, m_ResolutionPercentage);

		// Draw
		ImGui::GetCurrentWindow()->DrawList->AddCallback(DisableBlendCallback, nullptr);
		ImGui::Image((ImTextureID)(intptr_t)m_Mandelbrot.GetTexture(), ImGui::GetContentRegionAvail(), ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
		ImGui::GetCurrentWindow()->DrawList->AddCallback(EnableBlendCallback, nullptr);

		// Events
		FractalHandleInteract(m_Mandelbrot, m_ResolutionPercentage);
		FractalHandleZoom(m_Mandelbrot, m_ResolutionPercentage, m_FrameRate, m_SmoothZoom, m_MandelbrotZoomData);

		if (ImGui::IsWindowHovered())
		{
			// Right click to set `julia c`
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
				(ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0) && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)))
			{
				m_JuliaC = WindowToFract(ImGui::GetMousePos(), m_Mandelbrot, m_ResolutionPercentage);
				m_Julia.ResetRender();
			}
		}

		static bool showIters = false;
		static glm::dvec2 c;
		PersistentMiddleClick(showIters, c, m_Mandelbrot, m_ResolutionPercentage);

		if (showIters)
			DrawIterations(c, c, m_EqExponent, m_IterationsColor, m_Mandelbrot, m_ResolutionPercentage);

		if (m_ShowAnimationCenter && m_SelectedFractal == &m_Mandelbrot)
		{
			if (ShowCenterKeyFrames(m_Mandelbrot))
			{
				UpdatePlots();
				m_ShouldUpdatePreview = true;
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

void MainLayer::ShowJuliaWindow()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
	m_JuliaMinimized = !ImGui::Begin("Julia");

	if (!m_JuliaMinimized)
	{
		// Resize
		FractalHandleResize(m_Julia, m_ResolutionPercentage);

		// Draw
		ImGui::GetCurrentWindow()->DrawList->AddCallback(DisableBlendCallback, nullptr);
		ImGui::Image((ImTextureID)(intptr_t)m_Julia.GetTexture(), ImGui::GetContentRegionAvail(), ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
		ImGui::GetCurrentWindow()->DrawList->AddCallback(EnableBlendCallback, nullptr);

		// Events
		FractalHandleInteract(m_Julia, m_ResolutionPercentage);
		FractalHandleZoom(m_Julia, m_ResolutionPercentage, m_FrameRate, m_SmoothZoom, m_JuliaZoomData);

		static bool showIters = false;
		static glm::dvec2 z;
		PersistentMiddleClick(showIters, z, m_Julia, m_ResolutionPercentage);

		if (showIters)
			DrawIterations(z, m_JuliaC, m_EqExponent, m_IterationsColor, m_Julia, m_ResolutionPercentage);

		if (m_ShowAnimationCenter && m_SelectedFractal == &m_Julia)
		{
			if (ShowCenterKeyFrames(m_Julia))
			{
				UpdatePlots();
				m_ShouldUpdatePreview = true;
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

void MainLayer::ShowControlsWindow()
{
	ImGuiStyle& style = ImGui::GetStyle();
	auto windowBgCol = style.Colors[ImGuiCol_WindowBg];
	windowBgCol.w = 0.5f;
	ImGui::PushStyleColor(ImGuiCol_WindowBg, windowBgCol);
	if (ImGui::Begin("Controls"))
	{
		ImGui::Text("%.1ffps", m_FrameRate);

		ImGui::Spacing();

		if (ImGui::CollapsingHeader("General"))
		{
			if (ImGui::DragInt("Iterations per step", &m_ItersPerSteps, 10, 1, 10000, "%d", ImGuiSliderFlags_AlwaysClamp))
			{
				m_Mandelbrot.SetIterationsPerFrame(m_ItersPerSteps);
				m_Julia.SetIterationsPerFrame(m_ItersPerSteps);
			}

			if (ImGui::DragInt("Steps per frame", &m_StepsPerFrame, 1, 1, 100, "%d", ImGuiSliderFlags_AlwaysClamp))
			{
				m_Mandelbrot.ResetRender();
				m_Julia.ResetRender();
			}

			if (ImGui::DragInt("Resolution percentage", &m_ResolutionPercentage, 1, 30, 500, "%d%%", ImGuiSliderFlags_AlwaysClamp))
			{
				glm::uvec2 mandelbrotSize = (glm::vec2)m_Mandelbrot.GetSize() * (m_ResolutionPercentage / 100.f);
				m_Mandelbrot.SetSize(mandelbrotSize);

				glm::uvec2 juliaSize = (glm::vec2)m_Mandelbrot.GetSize() * (m_ResolutionPercentage / 100.f);
				m_Julia.SetSize(juliaSize);
			}

			if (ImGui::DragInt("Equation exponent", &m_EqExponent, 0.1f, 2, INT_MAX, "%d", ImGuiSliderFlags_AlwaysClamp))
			{
				m_Mandelbrot.SetEqExponent(m_EqExponent);
				m_Julia.SetEqExponent(m_EqExponent);
			}

			if (ImGui::ColorEdit3("Set Color", glm::value_ptr(m_SetColor))) 
			{
				m_Mandelbrot.SetSetColor(m_SetColor);
				m_Julia.SetSetColor(m_SetColor);
			}

			// If unlimited epochs is checked, set m_MaxEpochs to 0 but keep the slider value to the previous value
			static bool limitEpochs = m_MaxEpochs != 0;
			static int max_epochs = limitEpochs ? m_MaxEpochs : 100;
			if (ImGui::Checkbox("Limit epochs", &limitEpochs))
			{
				if (limitEpochs)
					m_MaxEpochs = max_epochs;
				else
					m_MaxEpochs = 0;

				m_Mandelbrot.SetMaxEpochs(m_MaxEpochs);
				m_Julia.SetMaxEpochs(m_MaxEpochs);
			}

			ImGui::SameLine(); HelpMarker("You may want to limit the maximum number of epochs to avoid getting a blurry image.");

			ImGui::BeginDisabled(!limitEpochs);
			{
				ImGui::PushItemWidth(ImGui::CalcItemWidth() - ImGui::GetContentRegionAvail().x);
				ImGui::Indent();

				if (ImGui::DragInt("Max epochs", &max_epochs, 1, 1, 2000, "%d", ImGuiSliderFlags_AlwaysClamp))
				{
					m_MaxEpochs = max_epochs;

					m_Mandelbrot.SetMaxEpochs(m_MaxEpochs);
					m_Julia.SetMaxEpochs(m_MaxEpochs);
				}

				ImGui::Unindent();
				ImGui::PopItemWidth();
			}
			ImGui::EndDisabled();

			static bool fadeEnabled = m_FadeThreshold != 0;
			static int fadeThreshold = fadeEnabled ? fadeEnabled : 10000;
			if (ImGui::Checkbox("Enable fade", &fadeEnabled))
			{
				if (fadeEnabled)
					m_FadeThreshold = fadeThreshold;
				else
					m_FadeThreshold = 0;

				m_Mandelbrot.SetFadeThreshold(m_FadeThreshold);
				m_Julia.SetFadeThreshold(m_FadeThreshold);
			}

			ImGui::SameLine(); HelpMarker("Similar to Anti-Aliasing, this feature should smoothen the edge of the set. The threshold sets the number of iterations needed to start loosing detail.");

			ImGui::BeginDisabled(!fadeEnabled);
			{
				ImGui::PushItemWidth(ImGui::CalcItemWidth() - ImGui::GetContentRegionAvail().x);
				ImGui::Indent();

				if (ImGui::DragInt("Fade Threshold", &fadeThreshold, 1, 1, 100000, "%d", ImGuiSliderFlags_AlwaysClamp))
				{
					m_FadeThreshold = fadeThreshold;

					m_Mandelbrot.SetFadeThreshold(m_FadeThreshold);
					m_Julia.SetFadeThreshold(m_FadeThreshold);
				}

				ImGui::Unindent();
				ImGui::PopItemWidth();
			}
			ImGui::EndDisabled();

			ImGui::Spacing();

			ImGui::AlignTextToFramePadding();
			ImGui::ColorEdit4("Iterations color", &m_IterationsColor.Value.x);

			ImGui::SameLine(); HelpMarker("Press the middle mouse button to show the first iterations at that point");

			ImGui::Checkbox("Smooth Zoom", &m_SmoothZoom);

			if (ImGui::Checkbox("VSync", &m_VSync))
				GLCore::Application::Get().GetWindow().SetVSync(m_VSync);

			ImGui::Spacing();
		}

		if (ImGui::CollapsingHeader("Color function", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Checkbox("Smooth Color", &m_SmoothColor))
			{
				m_Mandelbrot.SetSmoothColor(m_SmoothColor);
				m_Julia.SetSmoothColor(m_SmoothColor);
			}

			if (ImGui::Button("Refresh"))
				m_ShouldRefreshColors = true;

			ImGui::SameLine(); HelpMarker("Edit (or add) the files in the 'assets/colors' folder and they will appear here after a refresh.");

			ImVec2 button_size = { 100, 50 };
			float window_visible_x = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
			for (size_t i = 0; i < m_Colors.size(); i++)
			{
				ImGui::PushID((int)i);

				if (m_ColorsError[i])
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 0.2f, 0.2f, 1.f));
				}
				else if (m_SelectedColor == i)
					ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
				else
					ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_Button]);

				if (ImGui::ImageButton((ImTextureID)(intptr_t)m_ColorsPreview[i].textureID, button_size))
					SetColorFunction(i);
				
				if (ImGui::IsItemHovered() && GImGui->HoveredIdTimer > 0.5f)
					ImGui::SetTooltip(m_Colors[i]->GetName().c_str());

				ImGui::PopStyleColor();
				if (m_ColorsError[i])
					ImGui::PopStyleColor();

				float last_button_x = ImGui::GetItemRectMax().x;
				float next_button_x = last_button_x + style.ItemSpacing.x + button_size.x; // Expected position if next button was on same line

				if (i + 1 < m_Colors.size() && next_button_x < window_visible_x)
					ImGui::SameLine();

				ImGui::PopID();
			}

			bool updated = false;

			ImGui::Spacing();
			if (m_ColorsError[m_SelectedColor])
			{
				ImGui::Text("Errors:");
				ImGui::TextColored(ImColor(255, 50, 50), m_ColorsError[m_SelectedColor].value().c_str());
			}
			else
			{
				ImGui::Text("Color function parameters");
				for (Uniform* uniform : m_Colors[m_SelectedColor]->GetUniforms())
				{
					bool modified = false;

					switch (uniform->type)
					{
					case UniformType::FLOAT: {
						auto u = dynamic_cast<FloatUniform*>(uniform);
						modified = DragFloatR(u->displayName.c_str(), &u->val, u->default_val, u->speed, u->range.x, u->range.y);
						break;
					}
					case UniformType::COLOR: {
						auto u = dynamic_cast<ColorUniform*>(uniform);
						modified = ColorEdit3R(u->displayName.c_str(), glm::value_ptr(u->color), u->default_color);
						break;
					}
					case UniformType::BOOL: {
						auto u = dynamic_cast<BoolUniform*>(uniform);
						modified = ImGui::Checkbox(u->displayName.c_str(), &u->val);
						break;
					}
					default: {
						LOG_ERROR("ERROR: uniform type with code `{0}` is not implemented", (int)uniform->type);
						exit(EXIT_FAILURE);
						break;
					}
					}
					if (modified)
					{
						m_Mandelbrot.ResetRender();
						m_Julia.ResetRender();

						if (uniform->update)
						{
							updated = true;
							uniform->UpdateToShader(m_ColorsPreview[m_SelectedColor].shaderID);
						}
					}
				}

				if (updated)
				{
					GLuint fb;
					glGenFramebuffers(1, &fb);
					glBindFramebuffer(GL_FRAMEBUFFER, fb);

					glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorsPreview[m_SelectedColor].textureID, 0);
					GLenum buffers[] = { GL_COLOR_ATTACHMENT0 };
					glDrawBuffers(1, buffers);

					glUseProgram(m_ColorsPreview[m_SelectedColor].shaderID);

					glViewport(0, 0, previewSize.x, previewSize.y);
					glDisable(GL_BLEND);

					glBindVertexArray(GLCore::Application::GetDefaultQuadVA());
					glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

					glDeleteFramebuffers(1, &fb);

					glBindFramebuffer(GL_FRAMEBUFFER, 0);
				}
			}
			ImGui::Spacing();
		}

		if (ImGui::CollapsingHeader("Mandelbrot"))
		{
			ImGui::PushID(0);

			if (ImGui::Button("Reload Mandelbrot Shader"))
				m_Mandelbrot.SetShader(m_MandelbrotSrcPath);

			double cmin = -2, cmax = 2;
			glm::dvec2 center = m_Mandelbrot.GetCenter();
			if (ImGui::DragScalarN("Center", ImGuiDataType_Double, glm::value_ptr(center), 2, (float)m_Mandelbrot.GetRadius() / 70.f, &cmin, &cmax, "%.15f"))
				m_Mandelbrot.SetCenter(center);

			//double rmin = 1e-15, rmax = 50;
			double radius = m_Mandelbrot.GetRadius();
			if (DragDouble("Radius", &radius, 0.01f, 1e-15, 50, "%e", ImGuiSliderFlags_Logarithmic))
			//if (ImGui::DragScalar("Radius", ImGuiDataType_Double, &radius, 0.01f, &rmin, &rmax, "%e", ImGuiSliderFlags_Logarithmic))
			{
				m_Mandelbrot.SetRadius(radius);
				m_MandelbrotZoomData.start_radius = radius;
				m_MandelbrotZoomData.target_radius = radius;
			}

			if (ImGui::Button("Screenshot"))
			{
				std::string fileName = std::format("mandelbrot_{:.15f},{:.15f}", center.x, center.y);
				if (SaveImageDialog(fileName))
					if (!GLCore::Utils::ExportTexture(m_Mandelbrot.GetTexture(), fileName, true))
						LOG_ERROR("Failed to export the image! :(");
			}

			ImGui::Spacing();
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("Julia"))
		{
			ImGui::PushID(1);

			if (ImGui::Button("Reload Julia Shader"))
				m_Julia.SetShader(m_JuliaSrcPath);

			double cmin = -2, cmax = 2;
			glm::dvec2 center = m_Julia.GetCenter();
			if (ImGui::DragScalarN("Center", ImGuiDataType_Double, glm::value_ptr(center), 2, (float)m_Julia.GetRadius() / 200.f, &cmin, &cmax, "%.15f"))
				m_Julia.SetCenter(center);

			//double rmin = 1e-15, rmax = 50;
			double radius = m_Julia.GetRadius();
			if (DragDouble("Radius", &radius, 0.01f, 1e-15, 50, "%e", ImGuiSliderFlags_Logarithmic))
			//if (ImGui::DragScalar("Radius", ImGuiDataType_Double, &radius, 0.01f, &rmin, &rmax, "%e", ImGuiSliderFlags_Logarithmic))
			{
				m_Julia.SetRadius(radius);
				m_JuliaZoomData.start_radius = radius;
				m_JuliaZoomData.target_radius = radius;
			}

			if (ImGui::DragScalarN("C value", ImGuiDataType_Double, glm::value_ptr(m_JuliaC), 2, (float)m_Julia.GetRadius() * 1e-5f, &cmin, &cmax, "%.15f"))
				m_Julia.ResetRender();


			if (ImGui::Button("Screenshot"))
			{
				std::string fileName = std::format("julia_{:.15f},{:.15f}", m_JuliaC.x, m_JuliaC.y);
				if (SaveImageDialog(fileName))
					if (!GLCore::Utils::ExportTexture(m_Julia.GetTexture(), fileName, true))
						LOG_ERROR("Failed to export the image! :(");
			}

			ImGui::Spacing();
			ImGui::PopID();
		}

	}
	ImGui::End();
	ImGui::PopStyleColor();
}

template<typename T>
bool EditKeyFrames(KeyFrameList<T>& keyFrames, T new_val, double new_t, std::function<bool(T&)> value_edit)
{
	bool val_changed = false;
	bool edited_time = false;

	ImGui::SameLine();
	if (ImGui::SmallButton("+"))
	{
		keyFrames.emplace_back(std::make_shared<KeyFrame<T>>(new_t, new_val));
		edited_time = true;
		val_changed = true;
	}

	int deleted_index = -1;
	for (int i = 0; i < keyFrames.size(); i++)
	{
		auto& [t, v] = *(keyFrames[i]);

		ImGui::PushID(keyFrames[i].get());

		if (CloseButton("##close"))
			deleted_index = i;

		ImGui::SameLine();

		ImGui::PushItemWidth(ImGui::CalcItemWidth() * 0.25f);

		if (DragDouble("##time", &t, 0.01f, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
		{
			edited_time = true;
			val_changed = true;
		}

		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::PushItemWidth(ImGui::CalcItemWidth() * 0.75f);

		if (value_edit(v))
			val_changed = true;

		ImGui::PopItemWidth();

		ImGui::PopID();
	}
	if (edited_time)
		SortKeyFrames(keyFrames);

	if (deleted_index >= 0)
		if (keyFrames.size() > 1)
		{
			keyFrames.erase(keyFrames.begin() + deleted_index);
			val_changed = true;
		}

	return val_changed;
}

void MainLayer::ShowRenderWindow()
{
	if (ImGui::Begin("Rendering"))
	{
		if (ImGui::CollapsingHeader("Image"))
		{
			ImGui::PushID("Image");

			const char* fractal_names[] = { "Mandelbrot", "Julia" };
			static int fractal_index = 0;
			ImGui::Combo("Fractal", &fractal_index, fractal_names, IM_ARRAYSIZE(fractal_names));

			auto& fract = fractal_index == 0 ? m_Mandelbrot : m_Julia;

			static glm::ivec2 resolution(1920, 1080);
			ImGui::InputInt2("Resolution", glm::value_ptr(resolution));

			static int steps = 200;
			ImGui::DragInt("Steps", &steps, 1, 10000);

			static int iters_per_step = m_ItersPerSteps;
			ImGui::DragInt("Iters per step", &iters_per_step, 1, 10000);

			static glm::dvec2 center;
			static double radius = 1.0;
			PositionPicker("Position", glm::value_ptr(center), &radius, fract);

			if (ImGui::Button("Render Image"))
			{
				glm::dvec2 display_pos = fractal_index == 0 ? center : m_JuliaC;
				std::string fileName = std::format("{}_{:.15f},{:.15f}", fractal_names[fractal_index], display_pos.x, display_pos.y);
				if (SaveImageDialog(fileName))
				{
					fract.SetCenter(center);
					fract.SetRadius(radius);
					fract.SetSize(resolution);
					fract.SetIterationsPerFrame(iters_per_step);

					fract.ResetRender();

					for (int i = 0; i < steps; i++)
						fract.Update();

					GLCore::Utils::ExportTexture(fract.GetTexture(), fileName, true);

					glBindFramebuffer(GL_FRAMEBUFFER, 0);
				}
			}

			ImGui::Spacing();

			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("Video"))
		{
			ImGui::PushID("Video");

			auto& data = m_VideoRenderer;

			//bool rendering_video = m_State == State::Rendering;
			if (ImGui::BeginPopupModal("Rendering Video", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::ProgressBar(data.current_iter / (float)data.steps, { 200, 0 });

				if (m_State != State::Rendering)
					ImGui::CloseCurrentPopup();

				ImGui::EndPopup();
			}

			const char* fractal_names[] = { "Mandelbrot", "Julia" };
			static int fractal_index = 0;
			if (ImGui::Combo("Fractal", &fractal_index, fractal_names, IM_ARRAYSIZE(fractal_names)))
			{
				m_SelectedFractal = fractal_index == 0 ? &m_Mandelbrot : &m_Julia;
				const auto& path = fractal_index == 0 ? m_MandelbrotSrcPath : m_JuliaSrcPath;
				data.Prepare(path, *m_SelectedFractal);
				m_ShouldUpdatePreview = true;
			}

			auto fract = m_SelectedFractal;

			if (ImGui::InputInt2("Resolution", glm::value_ptr(data.resolution)))
				m_ShouldUpdatePreview = true;

			ImGui::DragFloat("Duration", &data.duration, 0.1f, 0.1f, 200.f);

			ImGui::DragInt("fps", &data.fps, 1, 1, 1000);

			if (ImGui::DragInt("Steps per frame", &data.steps_per_frame, 1, 100))
				m_ShouldUpdatePreview = true;

			if (ComboR("Color Function", &m_RenderColorIndex, (int)m_SelectedColor, m_ColorsName.data(), (int)m_ColorsName.size()))
			{
				data.SetColorFunction(GetColorFunction(m_RenderColorIndex));
				m_ShouldUpdatePreview = true;
			}

			if (ImGui::TreeNode("Animation"))
			{
				if (ImGui::TreeNodeEx("Radius", ImGuiTreeNodeFlags_AllowItemOverlap))
				{
					if (EditKeyFrames<double>(data.radiusKeyFrames, fract->GetRadius(), m_PreviewT, [&fract](double& r)
						{
							return DragDoubleR("##radius", &r, fract->GetRadius(), 0.01f, 1e-15, 50, "%e", ImGuiSliderFlags_Logarithmic);
						}))
					{
						m_ShouldUpdatePreview = true;
						UpdatePlots();
					}

					ImGui::TreePop();
				}
				bool open = ImGui::TreeNodeEx("Center", ImGuiTreeNodeFlags_AllowItemOverlap);

				
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0.5f, 0 });
				ImGui::SameLine();
				ImGui::PopStyleVar();

				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

				auto icon = m_ShowAnimationCenter ? ICON_MD_VISIBILITY : ICON_MD_VISIBILITY_OFF;
				if (ImGui::SmallButton(icon))
					m_ShowAnimationCenter = !m_ShowAnimationCenter;
				
				ImGui::PopStyleColor();

				if (open)
				{
					if (EditKeyFrames<CenterKey>(data.centerKeyFrames, {fract->GetCenter(), {0.0, 0.0}}, m_PreviewT, [&fract](CenterKey& c)
						{
							bool val_changed = false;
							ImGui::BeginGroup();
							val_changed |= DragDouble2R("Center##center", glm::value_ptr(c.pos), fract->GetCenter(), (float)fract->GetRadius() / 70.f, -2.0, 2.0, "%.15f");
							if (ImGui::TreeNode("Velocity")) {
								val_changed |= DragDouble2("##vel", glm::value_ptr(c.vel), (float)fract->GetRadius() / 70.f, -2.0, 2.0, "%.15f");
								ImGui::TreePop();
							}
							ImGui::EndGroup();

							return val_changed;
						}))
					{
						m_ShouldUpdatePreview = true;
						UpdatePlots();
					}

					ImGui::TreePop();
				}
				if (ImGui::TreeNode("Uniforms"))
				{
					for (auto& [u, keys] : data.uniformsKeyFrames)
					{
						if (ImGui::TreeNodeEx(u->displayName.c_str(), ImGuiTreeNodeFlags_AllowItemOverlap))
						{
							float new_val = 1.f;
							if (m_SelectedColor == m_RenderColorIndex)
							{
								auto color = GetColorFunction(m_SelectedColor);
								auto it = std::ranges::find_if(color->GetUniforms(), [u](Uniform* p) { return p->name == u->name; });
								if (it != color->GetUniforms().end())
									new_val = ((FloatUniform*)*it)->val;
							}
							if (EditKeyFrames<float>(keys, new_val, m_PreviewT, [u, new_val](float& v)
								{
									return DragFloatR("##val", &v, new_val, u->speed);
								}))
								m_ShouldUpdatePreview = true;

							ImGui::TreePop();
						}
					}
					ImGui::TreePop();
				}
				if (fractal_index == 1)
				{
					if (ImGui::TreeNode("Jula C"))
					{
						ImGui::SameLine();
						HelpMarker("This is a placeholder tool to edit the Julia C value.\nIt will be replaced with something similar to the center interpolation (hopefuly).");

						double min = -2, max = 2;
						if (DragDoubleR("Amplitude", &data.cAmplitude, 1e-3, 0.01f, 1e-15, 50, "%e", ImGuiSliderFlags_Logarithmic))
							m_ShouldUpdatePreview = true;
						
						if (DragDouble2R("Jula C Center", glm::value_ptr(data.cCenter), m_JuliaC, (float)fract->GetRadius() / 70.f, -2.0, 2.0, "%.15f"))
							m_ShouldUpdatePreview = true;

						ImGui::TreePop();
					}
				}

				ImGui::TreePop();
			}

			if (ImGui::Button("Render Video"))
			{
				data.fileName = std::format("{}_{:.15f},{:.15f}", fractal_names[fractal_index], data.centerKeyFrames.back()->val.pos.x, data.centerKeyFrames.back()->val.pos.y);
				if (GLCore::Application::Get().GetWindow().SaveFileDialog("mp4 (*.mp4)\0*.mp4\0", data.fileName))
				{
					m_State = State::Rendering;

					const auto& path = fractal_index == 0 ? m_MandelbrotSrcPath : m_JuliaSrcPath;

					data.Prepare(path, *fract);
					double dt = data.duration / (double)data.steps;

					data.InvalidateRadius(dt * 1e-3);
					data.InvalidateCenter();

					auto width = data.resolution.x;
					auto height = data.resolution.y;

					std::stringstream cmd;
					cmd << "ffmpeg ";
					cmd << "-y ";
					cmd << "-loglevel error ";

					cmd << "-r " << data.fps << " ";
					cmd << "-f rawvideo ";
					cmd << "-pix_fmt rgba ";
					cmd << "-s " << width << "x" << height << " ";
					cmd << "-i - ";

					cmd << "-vcodec libx264 ";
					cmd << "-pix_fmt yuv420p ";
					cmd << "-crf 15 ";
					cmd << "-vf \"vflip, pad = ceil(iw / 2) * 2:ceil(ih / 2) * 2\" ";
					cmd << "\"" << data.fileName << "\"";

					data.ffmpeg = _popen(cmd.str().c_str(), "wb");
					data.pixels = new BYTE[width * height * 4];

					//std::cout << "Please, do not close this window :)";

					ImGui::OpenPopup("Rendering Video");
				}
			}

			ImGui::PopID();
		}
	}
	ImGui::End();
}

void MainLayer::ShowPreviewWindow()
{
	m_PreviewMinimized = !ImGui::Begin("Render Preview");
	if (!m_PreviewMinimized)
	{
		auto& data = m_VideoRenderer;

		ImGui::BeginChild("img", { 0, -ImGui::GetFrameHeightWithSpacing() });

		ImVec2 avail = ImGui::GetContentRegionAvail();
		ImVec2 img_size = FitToScreen({ (float)data.resolution.x, (float)data.resolution.y }, avail);
		ImVec2 pos = { (avail.x - img_size.x) / 2.f, (avail.y - img_size.y) / 2.f };

		ImGui::SetCursorPos(ImGui::GetCursorPos() + pos);

		ImGui::GetCurrentWindow()->DrawList->AddCallback(DisableBlendCallback, nullptr);
		ImGui::Image((ImTextureID)(intptr_t)data.fract->GetTexture(), img_size, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
		ImGui::GetCurrentWindow()->DrawList->AddCallback(EnableBlendCallback, nullptr);

		ImGui::EndChild();

		if (ImGui::SliderFloat("##preview_percent", &m_PreviewT, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
			m_ShouldUpdatePreview = true;

		ImGui::SameLine();

		if (ImGui::Button("Apply To Fract"))
		{
			auto& fract = *m_SelectedFractal;

			fract.SetCenter(data.fract->GetCenter());
			fract.SetRadius(data.fract->GetRadius());

			auto color = SetColorFunction(m_RenderColorIndex);
			for (int i = 0; i < color->GetUniforms().size(); i++)
			{
				auto dest = (FloatUniform*)color->GetUniforms()[i];
				auto source = (FloatUniform*)data.color->GetUniforms()[i];

				dest->val = source->val;
			}
		}
	}
	ImGui::End();
}

void MainLayer::ShowHelpWindow()
{
	if (ImGui::Begin("Help", &m_ShowHelp, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("CONTROLS:");
		ImGui::BulletText("H to toggle this help window.");
		ImGui::BulletText("Mouse drag to pan.");
		ImGui::BulletText("Mouse wheel to zoom.");
		ImGui::BulletText("Left click the mandelbrot set to set the julia c to the mouse location.");
		ImGui::BulletText("CTRL + left click to set the center to the mouse location.");
		ImGui::BulletText("Middle mouse button to show the first iterations of the equation.");
		ImGui::BulletText("Hold CTRL while releasing the middle mouse button to continuously show the\n"
						  "iterations.");

		ImGui::Spacing();

		ImGui::Text("FEATURES:");
		ImGui::BulletText("All panels (including this one) can be moved and docked wherever you want.");
		ImGui::BulletText("You can edit (or add) the color functions by editing (or adding) the .glsl\n"
						  "shader flies in the `./assets/colors` folder. In this files use the\n"
						  "preprocessor command `#uniform` to set a custom uniform which will be exposed\n"
						  "through the UI, under color function parameters. There are the following types\n"
						  "of uniforms:");
		ImGui::Indent();
		{
			ImGui::BulletText("`#uniform float <name> <display_name> <default_value> <slider_increment>\n"
							  "<min> <max>;`. Either min or max can be set to `NULL` to indicate it is\n"
							  "unbounded.");
			ImGui::BulletText("`#uniform bool <name> <display_name> <default_value>;`. The default value\n"
							  "must be either `true` or `false`.");
			ImGui::BulletText("`#uniform color <name> <display_name> <default_red> <default_green>\n"
							  "<default_blue>;`. The RGB values must be between 0 and 1.");

			ImGui::Text("All uniform types accept an optional boolean parameter at the end (defaults to\n"
						"`true`) which indicates whether this parameter should update the preview image.");
		}
		ImGui::Unindent();

		ImGui::Spacing();

		ImGui::Text("TIPS:");
		ImGui::BulletText("If the images are too noisy, try increasing the colorMult parameter in the\n"
						  "color function section of the controls panel.");
		ImGui::BulletText("If the first iteration contains too much black parts and you can not\n"
						  "confortably pan the image, try increasing the number of iterations per\n"
						  "frame. It may reduce the framerate but it would increase the quality of the\n"
						  "first frame.");
		ImGui::BulletText("If the image ends up being too blurry after a while of rendering, try\n"
						  "limiting the maximum number of epochs. This option is located in the general\n"
						  "section of the controls panel.");
	}
	ImGui::End();
}

void MainLayer::RefreshColorFunctions()
{
	// Crear previews colors
	m_Colors.clear();

	for (const auto& [prev, error] : std::views::zip(m_ColorsPreview, m_ColorsError))
	{
		if (!error)
		{
			glDeleteTextures(1, &prev.textureID);
			glDeleteProgram(prev.shaderID);
		}
	}
	m_ColorsPreview.clear();
	m_ColorsError.clear();

	// Allocate new colors
	m_Colors.reserve(10);
	m_ColorsError.reserve(10);
	for (const auto& path : std::filesystem::directory_iterator("assets/colors"))
	{
		std::ifstream colorSrc(path.path());

		const auto& name = path.path().filename().replace_extension().string();
		auto colorFn = std::make_shared<ColorFunction>(name);
		
		std::optional<std::string> error;
		try
		{
			colorFn->Initialize(std::string(std::istreambuf_iterator<char>(colorSrc), std::istreambuf_iterator<char>()));
			error = GLCore::Utils::ValidateShader(colorFn->GetSource());
		}
		catch (const custom_error& e)
		{
			error = e.what();
		}
		catch (const std::exception& e)
		{
			error = std::format("Uncatched error: '{}'\nMake sure that uniforms follow the format specified in the documentation.", e.what());
		}

		m_ColorsError.push_back(error);
		if (error)
			LOG_ERROR("{}", error.value());
		
		m_Colors.push_back(colorFn);
	}

	m_ColorsName.clear();
	m_ColorsName.reserve(m_Colors.size());
	for (const auto& c : m_Colors)
		m_ColorsName.push_back(c->GetName().c_str());

	// Framebuffer
	GLuint fb;
	glGenFramebuffers(1, &fb);
	glBindFramebuffer(GL_FRAMEBUFFER, fb);

	// Allocate the prevews
	m_ColorsPreview.reserve(m_Colors.size());
	for (const auto& [c, error] : std::views::zip(m_Colors, m_ColorsError))
	{
		if (error)
		{
			m_ColorsPreview.emplace_back(0, 0);
			continue;
		}

		// Make the preview
		GLuint tex;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, previewSize.x, previewSize.y, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
		GLenum buffers[] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, buffers);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			LOG_ERROR("Failed to create the color function preview framebuffer");
			exit(EXIT_FAILURE);
		}

		// Shader
		std::stringstream ss;
		ss << "#version 400\n\n";
		ss << c->GetSource() << '\n';
		ss << R"(
layout (location = 0) out vec3 outColor;

uniform uint i_Range;
uniform uvec2 i_Size;

void main()
{
	int i = int((gl_FragCoord.x / i_Size.x) * i_Range);
	outColor = get_color(i);
}
	)";

		GLuint shader = GLCore::Utils::CreateShader(ss.str());
		glUseProgram(shader);
		GLint loc;

		loc = glGetUniformLocation(shader, "i_Range");
		glUniform1ui(loc, 100);

		loc = glGetUniformLocation(shader, "i_Size");
		glUniform2ui(loc, previewSize.x, previewSize.y);

		c->UpdateUniformsToShader(shader);

		// Drawing
		glViewport(0, 0, previewSize.x, previewSize.y);
		glDisable(GL_BLEND);

		glBindVertexArray(GLCore::Application::GetDefaultQuadVA());
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

		m_ColorsPreview.emplace_back(tex, shader);
	}
	glDeleteFramebuffers(1, &fb);

	if (m_SelectedColor >= m_Colors.size())
		m_SelectedColor = 0;


	SetColorFunction(m_SelectedColor);
}

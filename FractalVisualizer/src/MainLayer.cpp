#include "MainLayer.h"

#include <iomanip>
#include <filesystem>
#include <fstream>
#include <format>
#include <algorithm>
#include <functional>

#define _USE_MATH_DEFINES
#include <math.h>

#include <imgui_internal.h>
#include "LayerUtils.h"

#include <IconsMaterialDesign.h>

static glm::uvec2 previewSize = { 100, 1 };

MainLayer::MainLayer()
	: m_MandelbrotSrcPath("assets/mandelbrot.glsl")
	, m_Mandelbrot(m_MandelbrotSrcPath)
	, m_JuliaSrcPath("assets/julia.glsl")
	, m_Julia(m_JuliaSrcPath)
{
	RefreshColorFunctions();

	GLCore::Application::Get().GetWindow().SetVSync(m_VSync);

	m_Mandelbrot.SetColorFunction(m_Colors[m_SelectedColor]);
	m_Julia.SetColorFunction(m_Colors[m_SelectedColor]);

	m_VideoRenderData.SetColorFunction(m_Colors[m_SelectedColor]);

	m_Mandelbrot.SetIterationsPerFrame(m_ItersPerSteps);
	m_Julia.SetIterationsPerFrame(m_ItersPerSteps);

	m_Mandelbrot.SetMaxEpochs(m_MaxEpochs);
	m_Julia.SetMaxEpochs(m_MaxEpochs);

	m_Mandelbrot.SetSmoothColor(m_SmoothColor);
	m_Julia.SetSmoothColor(m_SmoothColor);

	m_Mandelbrot.SetCenter({ -0.5, 0 });
	m_Julia.SetRadius(1.3);

	m_MandelbrotZoomData.start_radius = m_Mandelbrot.GetRadius();
	m_MandelbrotZoomData.target_radius = m_Mandelbrot.GetRadius();

	m_JuliaZoomData.start_radius = m_Julia.GetRadius();
	m_JuliaZoomData.target_radius = m_Julia.GetRadius();
}

MainLayer::~MainLayer()
{
	for (auto prev : m_ColorsPreview)
	{
		glDeleteTextures(1, &prev.textureID);
		glDeleteProgram(prev.shaderID);
	}
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
		break;
	}
	case State::Rendering:
	{
		auto& data = m_VideoRenderData;
		auto& i = data.current_iter;

		if (i < data.steps)
		{
			data.UpdateIter(i / (float)data.steps);

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
	case State::Previewing:
	{
		if (m_ShouldUpdatePreview)
		{
			m_VideoRenderData.UpdateIter(m_PreviewT);
			m_ShouldUpdatePreview = false;
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

void MainLayer::OnImGuiRender()
{
	ImGui::DockSpaceOverViewport();

	if (ImGui::IsKeyPressed(ImGuiKey_D))
		m_ShowDemo = !m_ShowDemo;

	if (m_ShowDemo)
		ImGui::ShowDemoWindow(&m_ShowDemo);

	if (ImGui::IsKeyPressed(ImGuiKey_H))
		m_ShowHelp = !m_ShowHelp;

	if (m_ShowHelp)
		ShowHelpWindow();

	ShowControlsWindow();
	ShowRenderWindow();

	ShowMandelbrotWindow();
	ShowJuliaWindow();
}

void MainLayer::ShowMandelbrotWindow()
{
	ImGuiIO& io = ImGui::GetIO();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
	m_MandelbrotMinimized = !ImGui::Begin("Mandelbrot");

	// Resize
	FractalHandleResize(m_Mandelbrot, m_ResolutionPercentage);

	// Draw
	ImGui::GetCurrentWindow()->DrawList->AddCallback(DisableBlendCallback, nullptr);
	ImGui::Image((ImTextureID)(intptr_t)m_Mandelbrot.GetTexture(), ImGui::GetContentRegionAvail(), ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
	ImGui::GetCurrentWindow()->DrawList->AddCallback(EnableBlendCallback, nullptr);

	// Events
	FractalHandleInteract(m_Mandelbrot, m_ResolutionPercentage);
	FractalHandleZoom(m_Mandelbrot, m_ResolutionPercentage, m_FrameRate, m_SmoothZoom, m_MandelbrotZoomData);

	static bool showIters = false;
	static glm::dvec2 c;
	PersistentMiddleClick(showIters, c, m_Mandelbrot, m_ResolutionPercentage);

	if (showIters)
		DrawIterations(c, c, m_IterationsColor, m_Mandelbrot, m_ResolutionPercentage);

	if (ImGui::IsWindowHovered())
	{
		ImVec2 mousePos = WindowPosToImagePos(ImGui::GetMousePos(), m_ResolutionPercentage);

		// Right click to set `julia c`
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
			(ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0) && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)))
		{
			m_JuliaC = m_Mandelbrot.MapCoordsToPos(mousePos);
			m_Julia.ResetRender();
		}
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

void MainLayer::ShowJuliaWindow()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
	m_JuliaMinimized = !ImGui::Begin("Julia");

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
		DrawIterations(z, m_JuliaC, m_IterationsColor, m_Julia, m_ResolutionPercentage);

	ImGui::End();
	ImGui::PopStyleVar();
}

void MainLayer::ShowControlsWindow()
{
	ImGuiStyle& style = ImGui::GetStyle();
	auto windowBgCol = style.Colors[ImGuiCol_WindowBg];
	windowBgCol.w = 0.5f;
	ImGui::PushStyleColor(ImGuiCol_WindowBg, windowBgCol);
	ImGui::Begin("Controls");

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

			if (m_SelectedColor == i)
				ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
			else
				ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_Button]);

			if (ImGui::ImageButton((ImTextureID)(intptr_t)m_ColorsPreview[i].textureID, button_size))
			{
				m_SelectedColor = i;
				m_Mandelbrot.SetColorFunction(m_Colors[m_SelectedColor]);
				m_Julia.SetColorFunction(m_Colors[m_SelectedColor]);
			}
			if (ImGui::IsItemHovered() && GImGui->HoveredIdTimer > 0.5f)
				ImGui::SetTooltip(m_Colors[i]->GetName().c_str());

			ImGui::PopStyleColor();

			float last_button_x = ImGui::GetItemRectMax().x;
			float next_button_x = last_button_x + style.ItemSpacing.x + button_size.x; // Expected position if next button was on same line

			if (i + 1 < m_Colors.size() && next_button_x < window_visible_x)
				ImGui::SameLine();

			ImGui::PopID();
		}

		bool updated = false;

		ImGui::Spacing();
		ImGui::Text("Color function parameters");
		for (Uniform* uniform : m_Colors[m_SelectedColor]->GetUniforms())
		{
			bool modified = false;

			switch (uniform->type)
			{
			case UniformType::FLOAT: {
				auto u = dynamic_cast<FloatUniform*>(uniform);
				modified = DragFloatR(u->name.c_str(), &u->val, u->default_val, u->speed, u->range.x, u->range.y);
				break;
			}
			case UniformType::COLOR: {
				auto u = dynamic_cast<ColorUniform*>(uniform);
				modified = ColorEdit3R(u->name.c_str(), glm::value_ptr(u->color), u->default_color);
				break;
			}
			case UniformType::BOOL: {
				auto u = dynamic_cast<BoolUniform*>(uniform);
				modified = ImGui::Checkbox(u->name.c_str(), &u->val);
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

		double rmin = 1e-15, rmax = 50;
		double radius = m_Mandelbrot.GetRadius();
		if (ImGui::DragScalar("Radius", ImGuiDataType_Double, &radius, 0.01f, &rmin, &rmax, "%e", ImGuiSliderFlags_Logarithmic))
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

		double rmin = 1e-15, rmax = 50;
		double radius = m_Julia.GetRadius();
		if (ImGui::DragScalar("Radius", ImGuiDataType_Double, &radius, 0.01f, &rmin, &rmax, "%e", ImGuiSliderFlags_Logarithmic))
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

	ImGui::End();
	ImGui::PopStyleColor();
}

void MainLayer::ShowRenderWindow()
{
	ImGui::Begin("Rendering");

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

		auto& data = m_VideoRenderData;

		//bool rendering_video = m_State == State::Rendering;
		if (ImGui::BeginPopupModal("Rendering Video"))
		{
			ImGui::ProgressBar(data.current_iter / (float)data.steps, { 200, 0 });

			if (m_State != State::Rendering)
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}

		const char* fractal_names[] = { "Mandelbrot", "Julia" };
		static int fractal_index = 0;
		ImGui::Combo("Fractal", &fractal_index, fractal_names, IM_ARRAYSIZE(fractal_names));
		auto& fract = fractal_index == 0 ? m_Mandelbrot : m_Julia;

		ImGui::InputInt2("Resolution", glm::value_ptr(data.resolution));

		ImGui::DragFloat("Duration", &data.duration, 0.1f, 0.1f, 200.f);

		ImGui::DragInt("fps", &data.fps, 1, 1, 1000);

		ImGui::DragInt("Steps per frame", &data.steps_per_frame, 1, 100);

		static int color_index = (int)m_SelectedColor;
		if (ComboR("Color Function", &color_index, (int)m_SelectedColor, m_ColorsName.data(), (int)m_ColorsName.size()))
			data.SetColorFunction(m_Colors[color_index]);
		
		if (ImGui::TreeNode("Animation"))
		{
			if (ImGui::TreeNode("Radius"))
			{
				const float button_size = ImGui::GetFrameHeight();
				if (ImGui::Button("-", ImVec2(button_size, button_size)))
					if (data.radiusKeyFrames.size() > 1)
						data.radiusKeyFrames.pop_back();

				ImGui::SameLine();

				if (ImGui::Button("+", ImVec2(button_size, button_size)))
					data.radiusKeyFrames.emplace_back(std::make_shared<KeyFrame<double>>(1.f, 1.0));

				bool edited = false;
				for (auto& key : data.radiusKeyFrames)
				{
					auto& [t, r] = *key;

					ImGui::PushID(key.get());
					ImGui::PushItemWidth(ImGui::CalcItemWidth() * 0.25f);
					
					if (ImGui::DragFloat("##time", &t, 0.01f, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
						edited = true;

					ImGui::PopItemWidth(); 
					ImGui::SameLine();
					double rmin = 1e-15, rmax = 50;
					ImGui::PushItemWidth(ImGui::CalcItemWidth() * 0.75f);
					ImGui::DragScalar("##radius", ImGuiDataType_Double, &r, 0.01f, &rmin, &rmax, "%e", ImGuiSliderFlags_Logarithmic);
					ImGui::PopItemWidth();

					ImGui::PopID();
				}
				if (edited)
					data.SortKeyFrames();

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Center"))
			{
				const float button_size = ImGui::GetFrameHeight();
				if (ImGui::Button("-", ImVec2(button_size, button_size)))
					if (data.centerKeyFrames.size() > 1)
						data.centerKeyFrames.pop_back();

				ImGui::SameLine();

				if (ImGui::Button("+", ImVec2(button_size, button_size)))
					data.centerKeyFrames.emplace_back(std::make_shared<KeyFrame<glm::dvec2>>(1.f, glm::dvec2{ 0, 0 }));

				bool edited = false;
				for (auto& key : data.centerKeyFrames)
				{
					auto& [t, p] = *key;

					ImGui::PushID(key.get());
					ImGui::PushItemWidth(ImGui::CalcItemWidth() * 0.25f);

					if (ImGui::DragFloat("##time", &t, 0.01f, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
						edited = true;

					ImGui::PopItemWidth();
					ImGui::SameLine();
					ImGui::PushItemWidth(ImGui::CalcItemWidth() * 0.75f);
					double cmin = -2, cmax = 2;
					ImGui::DragScalarN("##center", ImGuiDataType_Double, glm::value_ptr(p), 2, (float)m_Julia.GetRadius() / 200.f, &cmin, &cmax, "%.15f");

					ImGui::PopItemWidth();

					ImGui::PopID();
				}
				if (edited)
					data.SortKeyFrames();

				ImGui::TreePop();
			}

			ImGui::TreePop();
		}

#if 0
		if (ImGui::TreeNode("Animation"))
		{
			const float button_size = ImGui::GetFrameHeight();
			if (ImGui::Button("-", ImVec2(button_size, button_size)))
				if (data.keyFrames.size() > 2)
					data.keyFrames.pop_back();

			ImGui::SameLine();

			if (ImGui::Button("+", ImVec2(button_size, button_size)))
			{
				auto& key = data.keyFrames.emplace_back();
				data.FillKeyFrameUniforms(key);
			}

			for (int i = 0; i < data.keyFrames.size(); i++)
			{
				auto& keyFrame = data.keyFrames[i];

				auto label = std::to_string(i);
				ImGui::PushID(label.c_str());

				bool open = ImGui::TreeNode(label.c_str());
				ImGui::SameLine();
				if (ImGui::SmallButton("Current"))
				{
					keyFrame.center = fract.GetCenter();
					keyFrame.radius = fract.GetRadius();

					
				}

				if (open)
				{
					ImGui::DragScalarN("Center", ImGuiDataType_Double, glm::value_ptr(keyFrame.center), 2, 0.01f, nullptr, nullptr, "%.15f");

					double rmin = 1e-15, rmax = 50;
					ImGui::DragScalar("Radius", ImGuiDataType_Double, &keyFrame.radius, 0.01f, &rmin, &rmax, "%e", ImGuiSliderFlags_Logarithmic);
					
					if (ImGui::TreeNode("Uniforms"))
					{
						for (auto& [u, val] : keyFrame.uniforms)
							ImGui::DragFloat(u->name.c_str(), &val, u->speed);

						ImGui::TreePop();
					}

					ImGui::TreePop();
				}
				ImGui::PopID();

			}

			ImGui::TreePop();
		}
#endif

		bool render_preview_open = (m_State == State::Previewing);
		if (ImGui::BeginPopupModal("Render Preview", &render_preview_open))
		{
			ImGui::GetCurrentWindow()->DrawList->AddCallback(DisableBlendCallback, nullptr);
			ImGui::Image((ImTextureID)(intptr_t)data.fract->GetTexture(), ImGui::GetContentRegionAvail(), ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
			ImGui::GetCurrentWindow()->DrawList->AddCallback(EnableBlendCallback, nullptr);

			if (ImGui::SliderFloat("##preview_percent", &m_PreviewT, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
				m_ShouldUpdatePreview = true;

			ImGui::EndPopup();
		}
		if (!render_preview_open && m_State == State::Previewing) // If the close button is pressed
			m_State = State::Exploring;

		if (ImGui::Button("Preview"))
		{
			m_State = State::Previewing;

			const auto& path = fractal_index == 0 ? m_MandelbrotSrcPath : m_JuliaSrcPath;
			data.Prepare(path, fract);

			m_ShouldUpdatePreview = true;

			ImGui::OpenPopup("Render Preview");
		}

		ImGui::SameLine();
		if (ImGui::Button("Render Video"))
		{
			//data.fileName = std::format("{}_{:.15f},{:.15f}", fractal_names[fractal_index], data.keyFrames.back().center.x, data.keyFrames.back().center.y);
			data.fileName = "nye";
			if (GLCore::Application::Get().GetWindow().SaveFileDialog("mp4 (*.mp4)\0*.mp4\0", data.fileName))
			{
				m_State = State::Rendering;

				const auto& path = fractal_index == 0 ? m_MandelbrotSrcPath : m_JuliaSrcPath;

				data.Prepare(path, fract);

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

	ImGui::End();
}

void MainLayer::ShowHelpWindow()
{
	ImGui::Begin("Help", &m_ShowHelp, ImGuiWindowFlags_AlwaysAutoResize);

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
		ImGui::BulletText("`#uniform float <name> <default_value> <slider_increment> <min> <max>;`.\n"
			"Either min or max can be set to `NULL` to indicate it is unbounded.");
		ImGui::BulletText("`#uniform bool <name> <default_value>;`. The default value must be either `true`\n"
			"or `false`.");
		ImGui::BulletText("`#uniform color <default_red> <default_green> <default_blue>;`. The RGB values\n"
			"must be between 0 and 1.");

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

	ImGui::End();
}

void MainLayer::RefreshColorFunctions()
{
	// Crear previews colors
	m_Colors.clear();

	for (auto prev : m_ColorsPreview)
	{
		glDeleteTextures(1, &prev.textureID);
		glDeleteProgram(prev.shaderID);
	}
	m_ColorsPreview.clear();

	// Allocate new colors
	m_Colors.reserve(10);
	for (const auto& path : std::filesystem::directory_iterator("assets/colors"))
	{
		std::ifstream colorSrc(path.path());
		m_Colors.emplace_back(std::make_shared<ColorFunction>(std::string((std::istreambuf_iterator<char>(colorSrc)), std::istreambuf_iterator<char>()), path.path().filename().replace_extension().string()));
	}

	m_ColorsName.reserve(m_Colors.size());
	for (const auto& c : m_Colors)
		m_ColorsName.push_back(c->GetName().c_str());

	// Framebuffer
	GLuint fb;
	glGenFramebuffers(1, &fb);
	glBindFramebuffer(GL_FRAMEBUFFER, fb);

	// Allocate the prevews
	m_ColorsPreview.reserve(m_Colors.size());
	for (const auto& c : m_Colors)
	{
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

	m_Mandelbrot.SetColorFunction(m_Colors[m_SelectedColor]);
	m_Julia.SetColorFunction(m_Colors[m_SelectedColor]);
}

void VideoRenderData::Prepare(const std::string& path, const FractalVisualizer& other)
{
	fract = std::make_unique<FractalVisualizer>(path);
	fract->SetColorFunction(color);
	fract->SetSmoothColor(other.GetSmoothColor());
	fract->SetFadeThreshold(other.GetFadeThreshold());
	fract->SetIterationsPerFrame(other.GetIterationsPerFrame());
	fract->SetSize(resolution);

	steps = (size_t)std::ceil(fps * duration);

	current_iter = 0;
}

double VideoRenderData::GetRadius(float t)
{
	// Assuming ordered keyframes
	if (t <= radiusKeyFrames.front()->t)
		return radiusKeyFrames.front()->val;

	if (t >= radiusKeyFrames.back()->t)
		return radiusKeyFrames.back()->val;

	for (int i = 0; i < radiusKeyFrames.size() - 1; i++)
	{
		if (radiusKeyFrames[i]->t <= t && t <= radiusKeyFrames[i + 1]->t)
		{
			float lt = map(t, radiusKeyFrames[i]->t, radiusKeyFrames[i + 1]->t, 0.f, 1.f);
			return mult_interp(radiusKeyFrames[i]->val, radiusKeyFrames[i + 1]->val, lt);
		}
	}
}

template<typename T, typename F>
T GetInterpolation(const std::vector<std::shared_ptr<KeyFrame<T>>>& keyFrames, float t, F interp)
{
	// Assuming ordered keyframes
	if (t <= keyFrames.front()->t)
		return keyFrames.front()->val;

	if (t >= keyFrames.back()->t)
		return keyFrames.back()->val;

	for (int i = 0; i < keyFrames.size() - 1; i++)
	{
		auto& a = keyFrames[i];
		auto& b = keyFrames[i+1];
		if (a->t <= t && t <= b->t)
		{
			float lt = map(t, a->t, b->t, 0.f, 1.f);
			return interp(*keyFrames[i], *keyFrames[i + 1], lt);
		}
	}
}

glm::dvec2 center_interp(const glm::ivec2& resolution, double initial_radius, double current_radius, const KeyFrame<glm::dvec2>& a, const KeyFrame<glm::dvec2>& b, float t)
{
	// Coords of end.center should lerp to the center of the screen
	const auto initial_coords = MapPosToCoords(resolution, initial_radius, a.val, b.val); // Screen coordinates of end.center at t=0
	const auto final_coords = ImVec2{ (float)resolution.x / 2.f, (float)resolution.y / 2.f };
	const auto target_coords = lerp(initial_coords, final_coords, (float)t); // Coordinates end.center should be at the current t

	// Move the center to set the coords of end.center to be target_coords
	const auto target_pos = MapCoordsToPos(resolution, current_radius, a.val, target_coords);
	const auto delta = target_pos - b.val;
	glm::dvec2 new_center = a.val - delta;
	return new_center;
}



void VideoRenderData::UpdateIter(float t)
{
	static const auto GetRadius = [this](float t) { 
		return GetInterpolation(radiusKeyFrames, t, [](const KeyFrame<double>& a, const KeyFrame<double>& b, float t) { return mult_interp(a.val, b.val, t); });
	};
	auto new_radius = GetRadius(t);

	auto f = [this, new_radius](const KeyFrame<glm::dvec2>& a, const KeyFrame<glm::dvec2>& b, float t) { 
		return center_interp(resolution, GetRadius(a.t), new_radius, a, b, t); 
	};
	auto new_center = GetInterpolation(centerKeyFrames, t, f);
	fract->SetRadius(new_radius);
	fract->SetCenter(new_center);

#if 0
	const float x = t * (keyFrames.size() - 1);
	int i = (int)std::floor(x);
	float lt = std::fmod(x, 1.f);

	if (i == keyFrames.size() - 1)
	{
		i -= 1;
		lt = 1.f;
	}

	const auto& start = keyFrames[i];
	const auto& end = keyFrames[i + 1];

	double new_radius = mult_interp(start.radius, end.radius, lt);

	// Coords of end.center should lerp to the center of the screen
	const auto initial_coords = MapPosToCoords(resolution, start.radius, start.center, end.center); // Screen coordinates of end.center at t=0
	const auto final_coords = ImVec2{ (float)resolution.x / 2.f, (float)resolution.y / 2.f };
	const auto target_coords = lerp(initial_coords, final_coords, (float)lt); // Coordinates end.center should be at the current t

	// Move the center to set the coords of end.center to be target_coords
	const auto target_pos = MapCoordsToPos(resolution, new_radius, start.center, target_coords); 
	const auto delta = target_pos - end.center;
	glm::dvec2 new_center = start.center - delta;

	fract->SetRadius(new_radius);
	fract->SetCenter(new_center);

	// Uniforms
	for (int i = 0; i < start.uniforms.size(); i++)
	{
		auto u = start.uniforms[i].first;
		auto start_v = start.uniforms[i].second;
		auto end_v = end.uniforms[i].second;
		const float new_val = lerp(start_v, end_v, lt);

		u->val = new_val;
	}
#endif

	for (int f = 0; f < steps_per_frame; f++)
		fract->Update();
}

void VideoRenderData::SetColorFunction(const std::shared_ptr<ColorFunction>& new_color)
{
	color = std::make_shared<ColorFunction>(*new_color);
}

void VideoRenderData::SortKeyFrames()
{
	std::sort(radiusKeyFrames.begin(), radiusKeyFrames.end(), [](auto a, auto b) { return a->t < b->t; });
	std::sort(centerKeyFrames.begin(), centerKeyFrames.end(), [](auto a, auto b) { return a->t < b->t; });
}

//void VideoRenderData::FillKeyFrameUniforms(RenderKeyFrame& key)
//{
//	key.uniforms.clear();
//	for (auto u : color->GetUniforms())
//	{
//		if (u->type == UniformType::FLOAT)
//		{
//			auto ptr = dynamic_cast<FloatUniform*>(u);
//			key.uniforms.emplace_back(ptr, ptr->val);
//		}
//	}
//}

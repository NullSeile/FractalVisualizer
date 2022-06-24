#include "MainLayer.h"

#include <iomanip>
#include <filesystem>
#include <fstream>
#include <commdlg.h>

#include <imgui_internal.h>
#include "ImGuiUtils.h"

static glm::uvec2 previewSize = { 100, 1 };

template<typename T>
std::ostream& operator<<(std::ostream& os, const glm::vec<2, T>& vec)
{
	os << '(' << vec.x << ", " << vec.y << ')';
	return os;
}

template<typename T>
ImVec2 GlmToImVec(const glm::vec<2, T>& vec)
{
	return { (float)vec.x, (float)vec.y };
}

template<size_t file_size>
static bool SaveImageDialog(char (&fileName)[file_size])
{
	const char* filter = "PNG (*.png)\0*.png\0JPEG (*jpg; *jpeg)\0*.jpg;*.jpeg\0BMP (*.bmp)\0*.bmp\0TGA (*.tga)\0*.tga\0";

	OPENFILENAMEA ofn;
	ZeroMemory(&ofn, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	//ofn.hwndOwner = Application::Get().GetWindow().GetNativeWindow(); TODO ;-;
	ofn.lpstrFile = fileName;
	ofn.nMaxFile = sizeof(fileName);
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

	// Sets the default extension by extracting it from the filter
	ofn.lpstrDefExt = strchr(filter, '\0') + 1;

	return GetSaveFileNameA(&ofn) == TRUE;
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
		m_Colors.emplace_back(std::string((std::istreambuf_iterator<char>(colorSrc)), std::istreambuf_iterator<char>()));
	}

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
		ss << c.GetSource() << '\n';
		ss << R"(
layout (location = 0) out vec3 outColor;

uniform uint range;
uniform uvec2 size;

void main()
{
int i = int((gl_FragCoord.x / size.x) * range);
outColor = get_color(i);
}
	)";

		GLuint shader = GLCore::Utils::CreateShader(ss.str());
		glUseProgram(shader);
		GLint loc;

		loc = glGetUniformLocation(shader, "range");
		glUniform1ui(loc, 100);

		loc = glGetUniformLocation(shader, "size");
		glUniform2ui(loc, previewSize.x, previewSize.y);

		c.UpdateUniformsToShader(shader);

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

	m_Mandelbrot.SetColorFunction(&m_Colors[m_SelectedColor]);
	m_Julia.SetColorFunction(&m_Colors[m_SelectedColor]);
}

MainLayer::MainLayer()
	: m_MandelbrotSrcPath("assets/mandelbrot.glsl")
	, m_Mandelbrot(m_MandelbrotSrcPath)
	, m_JuliaSrcPath("assets/julia.glsl")
	, m_Julia(m_JuliaSrcPath)
{
	RefreshColorFunctions();

	m_Mandelbrot.SetColorFunction(&m_Colors[m_SelectedColor]);
	m_Julia.SetColorFunction(&m_Colors[m_SelectedColor]);

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
}

void MainLayer::OnUpdate(GLCore::Timestep ts)
{
	m_FrameRate = 1 / ts.GetSeconds();

	if (m_ShouldRefreshColors)
	{
		RefreshColorFunctions();
		m_ShouldRefreshColors = false;
	}

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

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void DisableBlendCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
	glDisable(GL_BLEND);
}

void EnableBlendCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
	glEnable(GL_BLEND);
}

ImVec2 ImagePosToWindowPos(const ImVec2& imagePos, int resolutionPercentage)
{
	return imagePos / (resolutionPercentage / 100.f) + ImGui::GetWindowPos() + ImGui::GetWindowContentRegionMin();
}

ImVec2 WindowPosToImagePos(const ImVec2& windowPos, int resolutionPercentage)
{
	return (windowPos - ImGui::GetWindowPos() - ImGui::GetWindowContentRegionMin()) * (resolutionPercentage / 100.f);
}

void ZoomToScreenPos(FractalVisualizer& fract, ImVec2 pos, double radius)
{
	glm::dvec2 iMousePos = fract.MapCoordsToPos(pos);

	fract.SetRadius(radius);

	glm::dvec2 fMousePos = fract.MapCoordsToPos(pos);

	glm::dvec2 delta = fMousePos - iMousePos;
	fract.SetCenter(fract.GetCenter() - delta);
}

void FractalHandleInteract(FractalVisualizer& fract, int resolutionPercentage)
{
	if (ImGui::IsWindowHovered())
	{
		ImGuiIO& io = ImGui::GetIO();
		auto mouseDeltaScaled = io.MouseDelta * (resolutionPercentage / 100.f);

		auto mousePos = WindowPosToImagePos(ImGui::GetMousePos(), resolutionPercentage);

		if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0) && mousePos.y >= 0)
		{
			if (mouseDeltaScaled.x != 0 || mouseDeltaScaled.y != 0)
			{
				glm::dvec2 center = fract.MapCoordsToPos(fract.MapPosToCoords(fract.GetCenter()) - mouseDeltaScaled);
				fract.SetCenter(center);
			}
		}

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.KeyCtrl)
			fract.SetCenter(fract.MapCoordsToPos(mousePos));
	}
}

void FractalHandleZoom(FractalVisualizer& fract, int resolutionPercentage, float fps, bool smoothZoom, SmoothZoomData& data)
{
	auto io = ImGui::GetIO();

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

void FractalHandleResize(FractalVisualizer& fract, int resolutionPercentage)
{
	ImVec2 viewportPanelSizeScaled = ImGui::GetContentRegionAvail() * (resolutionPercentage / 100.f);

	auto size = fract.GetSize();
	if (glm::uvec2{ viewportPanelSizeScaled.x, viewportPanelSizeScaled.y } != size)
		fract.SetSize(glm::uvec2{ viewportPanelSizeScaled.x, viewportPanelSizeScaled.y });
}

void DrawIterations(const glm::dvec2& z0, const glm::dvec2& c, const ImColor& baseColor, FractalVisualizer& fract, int resolutionPercentage)
{
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	glm::dvec2 z = z0;
	for (int i = 0; i < 100; i++)
	{
		auto p0 = ImagePosToWindowPos(fract.MapPosToCoords(z), resolutionPercentage);
		z = glm::dvec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
		auto p1 = ImagePosToWindowPos(fract.MapPosToCoords(z), resolutionPercentage);

		float scale = 10.f;
		ImColor color(baseColor);
		color.Value.w *= scale / (i + scale);
		draw_list->AddLine(p0, p1, color, 2.f);
	}
}

void PersistentMiddleClick(bool& clicked, glm::dvec2& pos, FractalVisualizer& fract, int resolutionPercentage)
{
	if (ImGui::IsWindowHovered())
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
		{
			clicked = true;
			auto mousePos = WindowPosToImagePos(ImGui::GetMousePos(), resolutionPercentage);
			pos = fract.MapCoordsToPos(mousePos);
		}

		if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle) && !ImGui::GetIO().KeyCtrl)
			clicked = false;
	}
}

void MainLayer::OnImGuiRender()
{
	ImGuiStyle& style = ImGui::GetStyle();
	ImGuiIO& io = ImGui::GetIO();

	ImGui::DockSpaceOverViewport();

	//ImGui::ShowDemoWindow();

	// Help
	if (ImGui::IsKeyPressed(ImGuiKey_H))
		m_ShowHelp = !m_ShowHelp;

	if (m_ShowHelp)
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

	// Mandelbrot
	{
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

	// Julia
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		m_JuliaMinimized = !ImGui::Begin("Julia");

		// Resize
		FractalHandleResize(m_Julia, m_ResolutionPercentage);

		// Draw
		ImGui::GetCurrentWindow()->DrawList->AddCallback(DisableBlendCallback, nullptr);
		ImGui::Image((ImTextureID)(intptr_t)m_Julia.GetTexture(), ImGui::GetContentRegionAvail(), ImVec2{0, 1}, ImVec2{1, 0});
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

	// Controls
	{
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

			ImGui::Spacing();

			ImGui::AlignTextToFramePadding();
			ImGui::ColorEdit4("Iterations color", &m_IterationsColor.Value.x);

			ImGui::SameLine(); HelpMarker("Press the middle mouse button to show the first iterations at that point");

			ImGui::Checkbox("Smooth Zoom", &m_SmoothZoom);

			ImGui::Spacing();
		}

		if (ImGui::CollapsingHeader("Color function"))
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
					m_Mandelbrot.SetColorFunction(&m_Colors[m_SelectedColor]);
					m_Julia.SetColorFunction(&m_Colors[m_SelectedColor]);
				}

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
			for (Uniform* uniform : m_Colors[m_SelectedColor].GetUniforms())
			{
				bool modified = false;

				switch (uniform->type)
				{
				case UniformType::FLOAT: {
					auto u = dynamic_cast<FloatUniform*>(uniform);
					modified = DragFloatR(u->name.c_str(), &u->val, u->speed, u->range.x, u->range.y, u->default_val);
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
				CHAR fileName[260];
				auto center = m_Mandelbrot.GetCenter();
				sprintf_s(fileName, "mandelbrot_%.15f,%.15f", center.x, center.y);

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
				CHAR fileName[260];
				sprintf_s(fileName, "julia_%.15f,%.15f", m_JuliaC.x, m_JuliaC.y);

				if (SaveImageDialog(fileName))
					if (!GLCore::Utils::ExportTexture(m_Julia.GetTexture(), fileName, true))
						LOG_ERROR("Failed to export the image! :(");
			}

			ImGui::Spacing();
			ImGui::PopID();
		}

		ImGui::End(); // Controls
		ImGui::PopStyleColor();
	}
}
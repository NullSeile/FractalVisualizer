#pragma once

#include "FractalVisualizer.h"
#include <imgui_internal.h>
#include <IconsMaterialDesign.h>

ImVec2 operator+(const ImVec2& l, const ImVec2& r)
{
	return { l.x + r.x, l.y + r.y };
}

ImVec2 operator-(const ImVec2& l, const ImVec2& r)
{
	return { l.x - r.x, l.y - r.y };
}

ImVec2 operator*(const ImVec2& vec, float scalar)
{
	return { vec.x * scalar, vec.y * scalar };
}

ImVec2 operator/(const ImVec2& vec, float scalar)
{
	return { vec.x / scalar, vec.y / scalar };
}

std::ostream& operator<<(std::ostream& os, const ImVec2& vec)
{
	os << '(' << vec.x << ", " << vec.y << ')';
	return os;
}

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

template<typename T>
T sine_interp(const T& x)
{
	return -cos(M_PI * x) * 0.5 + 0.5;
}

template<typename T>
T lerp(T a, T b, T t)
{
	return b * t + a * ((T)1 - t);
}

static bool SaveImageDialog(std::string& fileName)
{
	return GLCore::Application::Get().GetWindow().SaveFileDialog(
		"PNG (*.png)\0*.png\0JPEG (*jpg; *jpeg)\0*.jpg;*.jpeg\0BMP (*.bmp)\0*.bmp\0TGA (*.tga)\0*.tga\0",
		fileName
	);
}

void HelpMarker(const char* desc)
{
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

#define RESET_CHAR ICON_MD_UNDO

bool DragFloatR(const char* label, float* v, float v_default, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	ImGui::PushID(label);

	ImGuiStyle style = ImGui::GetStyle();

	bool value_changed = false;
	const float button_size = ImGui::GetFrameHeight();

	ImGui::SetNextItemWidth(std::max(1.0f, ImGui::CalcItemWidth() - (button_size + style.ItemInnerSpacing.x)));
	value_changed |= ImGui::DragFloat("", v, v_speed, v_min, v_max, format, flags);

	ImGui::SameLine(0, style.ItemInnerSpacing.x);

	if (ImGui::Button(RESET_CHAR, ImVec2(button_size, button_size)))
	{
		value_changed = true;
		*v = v_default;
	}
	ImGui::SameLine(0, style.ItemInnerSpacing.x);
	ImGui::Text(label);

	ImGui::PopID();

	return value_changed;
}

bool DragFloat2R(const char* label, float v[2], glm::vec2 v_default, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	ImGui::PushID(label);

	ImGuiStyle style = ImGui::GetStyle();

	bool value_changed = false;
	const float button_size = ImGui::GetFrameHeight();

	ImGui::SetNextItemWidth(std::max(1.0f, ImGui::CalcItemWidth() - (button_size + style.ItemInnerSpacing.x)));
	value_changed |= ImGui::DragFloat2("", v, v_speed, v_min, v_max, format, flags);

	ImGui::SameLine(0, style.ItemInnerSpacing.x);

	if (ImGui::Button(RESET_CHAR, ImVec2(button_size, button_size)))
	{
		value_changed = true;
		v[0] = v_default.x;
		v[1] = v_default.y;
	}
	ImGui::SameLine(0, style.ItemInnerSpacing.x);
	ImGui::Text(label);

	ImGui::PopID();

	return value_changed;
}

bool ColorEdit3R(const char* label, float col[3], glm::vec3 default_col, ImGuiColorEditFlags flags = 0)
{
	ImGui::PushID(label);

	ImGuiStyle style = ImGui::GetStyle();

	bool value_changed = false;
	const float button_size = ImGui::GetFrameHeight();

	ImGui::SetNextItemWidth(std::max(1.0f, ImGui::CalcItemWidth() - (button_size + style.ItemInnerSpacing.x)));
	value_changed |= ImGui::ColorEdit3("", col, flags);

	ImGui::SameLine(0, style.ItemInnerSpacing.x);

	if (ImGui::Button(RESET_CHAR, ImVec2(button_size, button_size)))
	{
		value_changed = true;
		col[0] = default_col.r;
		col[1] = default_col.g;
		col[2] = default_col.b;
	}
	ImGui::SameLine(0, style.ItemInnerSpacing.x);
	ImGui::Text(label);

	ImGui::PopID();

	return value_changed;
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
		//if (ImGui::IsKeyDown(ImGuiKey_Space))
		if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
		{
			clicked = true;
			auto mousePos = WindowPosToImagePos(ImGui::GetMousePos(), resolutionPercentage);
			pos = fract.MapCoordsToPos(mousePos);
		}

		//if (ImGui::IsKeyReleased(ImGuiKey_Space) && !ImGui::GetIO().KeyCtrl)
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle) && !ImGui::GetIO().KeyCtrl)
			clicked = false;
	}
}

void PositionPicker(const char* label, double center[2], double* radius, const FractalVisualizer& fract)
{
	ImGui::PushID(label);

	bool open = ImGui::TreeNode(label);
	ImGui::SameLine();
	if (ImGui::SmallButton("Current"))
	{
		center[0] = fract.GetCenter().x;
		center[1] = fract.GetCenter().y;
		*radius = fract.GetRadius();
	}

	if (open)
	{
		ImGui::PushItemWidth(ImGui::CalcItemWidth() - ImGui::GetContentRegionAvail().x);
		ImGui::DragScalarN("Center", ImGuiDataType_Double, center, 2, 0.01f, nullptr, nullptr, "%.15f");

		double rmin = 1e-15, rmax = 50;
		ImGui::DragScalar("Radius", ImGuiDataType_Double, radius, 0.01f, &rmin, &rmax, "%e", ImGuiSliderFlags_Logarithmic);

		ImGui::PopItemWidth();
		ImGui::TreePop();
	}

	ImGui::PopID();
}

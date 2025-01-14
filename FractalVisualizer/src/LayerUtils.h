#pragma once

#include "FractalVisualizer.h"

#include <imgui_internal.h>
#include <IconsMaterialDesign.h>

// A mess of button
bool CloseButton(const char* label)
{
	ImGuiButtonFlags flags = ImGuiButtonFlags_None;
	auto size_arg = ImVec2(0, 0);

	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	const ImVec2 label_size = ImGui::CalcTextSize("", NULL, true);

	ImVec2 pos = window->DC.CursorPos;
	if ((flags & ImGuiButtonFlags_AlignTextBaseLine) && style.FramePadding.y < window->DC.CurrLineTextBaseOffset) // Try to vertically align buttons that are smaller/have no padding so that text baseline matches (bit hacky, since it shouldn't be a flag)
		pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;
	ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

	const ImRect bb(pos, pos + size);
	ImGui::ItemSize(size, style.FramePadding.y);
	if (!ImGui::ItemAdd(bb, id))
		return false;

	if (g.LastItemData.InFlags & ImGuiItemFlags_ButtonRepeat)
		flags |= ImGuiButtonFlags_Repeat;

	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);

	// Render (taken from ImGui::CloseButton())
	ImU32 col = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered);
	ImVec2 center = bb.GetCenter();
	if (hovered)
		window->DrawList->AddCircleFilled(center, ImMax(2.0f, g.FontSize * 0.5f + 1.0f), col, 12);

	float cross_extent = g.FontSize * 0.5f * 0.7071f - 1.0f;
	ImU32 cross_col = ImGui::GetColorU32(ImGuiCol_Text);
	center -= ImVec2(0.5f, 0.5f);
	window->DrawList->AddLine(center + ImVec2(+cross_extent, +cross_extent), center + ImVec2(-cross_extent, -cross_extent), cross_col, 1.0f);
	window->DrawList->AddLine(center + ImVec2(+cross_extent, -cross_extent), center + ImVec2(-cross_extent, +cross_extent), cross_col, 1.0f);

	return pressed;
}

ImVec2 FitToScreen(const ImVec2& source, const ImVec2& screen)
{
	float wi = source.x;
	float hi = source.y;
	float ws = screen.x;
	float hs = screen.y;

	float ri = source.x / source.y;
	float rs = screen.x / screen.y;

	return rs > ri ? ImVec2{ wi * hs / hi, hs } : ImVec2{ ws, hi * ws / wi };
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

	const char* label_end = ImGui::FindRenderedTextEnd(label);
	if (label != label_end)
	{
		ImGui::SameLine(0, style.ItemInnerSpacing.x);
		ImGui::TextEx(label, label_end);
	}

	ImGui::PopID();

	return value_changed;
}

bool DragDouble(const char* label, double* v, float v_speed, double v_min = 0.0, double v_max = 0.0, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return ImGui::DragScalar(label, ImGuiDataType_Double, v, v_speed, &v_min, &v_max, format, flags);
}

bool DragDoubleR(const char* label, double* v, double v_default, float v_speed = 1.0f, double v_min = 0.0f, double v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	ImGui::PushID(label);

	ImGuiStyle style = ImGui::GetStyle();

	bool value_changed = false;
	const float button_size = ImGui::GetFrameHeight();

	ImGui::SetNextItemWidth(std::max(1.0f, ImGui::CalcItemWidth() - (button_size + style.ItemInnerSpacing.x)));
	value_changed |= ImGui::DragScalar("", ImGuiDataType_Double, v, v_speed, &v_min, &v_max, format, flags);

	ImGui::SameLine(0, style.ItemInnerSpacing.x);

	if (ImGui::Button(RESET_CHAR, ImVec2(button_size, button_size)))
	{
		value_changed = true;
		*v = v_default;
	}

	const char* label_end = ImGui::FindRenderedTextEnd(label);
	if (label != label_end)
	{
		ImGui::SameLine(0, style.ItemInnerSpacing.x);
		ImGui::TextEx(label, label_end);
	}

	ImGui::PopID();

	return value_changed;
}

bool DragDouble2(const char* label, double v[2], float v_speed = 0.0f, double v_min = 0.0, double v_max = 0.0, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
    return ImGui::DragScalarN(label, ImGuiDataType_Double, v, 2, v_speed, &v_min, &v_max, format, flags);
}

bool DragDouble2R(const char* label, double v[2], glm::dvec2 v_default, float v_speed = 1.0f, double v_min = 0.0, double v_max = 0.0, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	ImGui::PushID(label);

	ImGuiStyle style = ImGui::GetStyle();

	bool value_changed = false;
	const float button_size = ImGui::GetFrameHeight();

	ImGui::SetNextItemWidth(std::max(1.0f, ImGui::CalcItemWidth() - (button_size + style.ItemInnerSpacing.x)));
	value_changed |= ImGui::DragScalarN("", ImGuiDataType_Double, v, 2, v_speed, &v_min, &v_max, format, flags);

	ImGui::SameLine(0, style.ItemInnerSpacing.x);

	if (ImGui::Button(RESET_CHAR, ImVec2(button_size, button_size)))
	{
		value_changed = true;
		v[0] = v_default.x;
		v[1] = v_default.y;
	}

	const char* label_end = ImGui::FindRenderedTextEnd(label);
	if (label != label_end)
	{
		ImGui::SameLine(0, style.ItemInnerSpacing.x);
		ImGui::TextEx(label, label_end);
	}

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

	const char* label_end = ImGui::FindRenderedTextEnd(label);
	if (label != label_end)
	{
		ImGui::SameLine(0, style.ItemInnerSpacing.x);
		ImGui::TextEx(label, label_end);
	}

	ImGui::PopID();

	return value_changed;
}

bool ComboR(const char* label, int* current_item, int default_item, const char* const items[], int items_count, int popup_max_height_in_items = -1)
{
	ImGui::PushID(label);

	ImGuiStyle style = ImGui::GetStyle();

	bool value_changed = false;
	const float button_size = ImGui::GetFrameHeight();

	ImGui::SetNextItemWidth(std::max(1.0f, ImGui::CalcItemWidth() - (button_size + style.ItemInnerSpacing.x)));
	value_changed |= ImGui::Combo("", current_item, items, items_count, popup_max_height_in_items);

	ImGui::SameLine(0, style.ItemInnerSpacing.x);

	if (ImGui::Button(RESET_CHAR, ImVec2(button_size, button_size)))
	{
		value_changed = true;
		*current_item = default_item;
	}

	const char* label_end = ImGui::FindRenderedTextEnd(label);
	if (label != label_end)
	{
		ImGui::SameLine(0, style.ItemInnerSpacing.x);
		ImGui::TextEx(label, label_end);
	}

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

glm::dvec2 ZoomToScreenPos(const glm::uvec2& resolution, double initial_radius, const glm::dvec2& initial_center, ImVec2 zoom_coords, double new_radius)
{
	glm::dvec2 initial_pos = MapCoordsToPos(resolution, initial_radius, initial_center, zoom_coords);
	glm::dvec2 final_pos = MapCoordsToPos(resolution, new_radius, initial_center, zoom_coords);
	glm::dvec2 delta = final_pos - initial_pos;
	return initial_center - delta;
}

void ZoomToScreenPos(FractalVisualizer& fract, ImVec2 pos, double radius)
{
	auto new_center = ZoomToScreenPos(fract.GetSize(), fract.GetRadius(), fract.GetCenter(), pos, radius);
	fract.SetCenter(new_center);
	fract.SetRadius(radius);
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

void FractalHandleResize(FractalVisualizer& fract, int resolutionPercentage)
{
	ImVec2 viewportPanelSizeScaled = ImGui::GetContentRegionAvail() * (resolutionPercentage / 100.f);

	auto size = fract.GetSize();
	if (glm::uvec2{ viewportPanelSizeScaled.x, viewportPanelSizeScaled.y } != size)
		fract.SetSize(glm::uvec2{ viewportPanelSizeScaled.x, viewportPanelSizeScaled.y });
}

glm::dvec2 mul(const glm::dvec2& a, const glm::dvec2& b)
{
	return { a.x*b.x-a.y*b.y, a.x*b.y+a.y*b.x };
}

void DrawIterations(const glm::dvec2& z0, const glm::dvec2& c, int eqExp, const ImColor& baseColor, FractalVisualizer& fract, int resolutionPercentage)
{
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	glm::dvec2 z = z0;
	for (int i = 0; i < 100; i++)
	{
		auto p0 = ImagePosToWindowPos(fract.MapPosToCoords(z), resolutionPercentage);
		auto newZ = z;
		for (int j = 0; j < eqExp - 1; j++)
			newZ = mul(newZ, z);
		z = newZ + c;
		
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

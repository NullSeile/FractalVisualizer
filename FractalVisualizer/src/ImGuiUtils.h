#pragma once

#include <GLCore.h>

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

bool DragFloatR(const char* label, float* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, float v_default = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	ImGui::PushID(label);

	ImGuiStyle style = ImGui::GetStyle();

	bool value_changed = false;
	const float button_size = ImGui::GetFrameHeight();

	ImGui::SetNextItemWidth(std::max(1.0f, ImGui::CalcItemWidth() - (button_size + style.ItemInnerSpacing.x)));
	value_changed |= ImGui::DragFloat("", v, v_speed, v_min, v_max, format, flags);

	ImGui::SameLine(0, style.ItemInnerSpacing.x);

	if (ImGui::Button("R", ImVec2(button_size, button_size)))
	{
		value_changed = true;
		*v = v_default;
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

	if (ImGui::Button("R", ImVec2(button_size, button_size)))
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

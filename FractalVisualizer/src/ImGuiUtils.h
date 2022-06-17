#pragma once

#include <GLCore.h>

static void HelpMarker(const char* desc)
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

static bool DragFloatR(const std::string& label, float* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, float v_default = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	ImGuiStyle style = ImGui::GetStyle();

	bool value_changed = false;

	std::string name = "##" + label;
	value_changed |= ImGui::DragFloat(name.c_str(), v, v_speed, v_min, v_max);

	ImGui::SameLine(0, style.ItemInnerSpacing.x);
	const float button_size = ImGui::GetFrameHeight();
	if (ImGui::Button("R", ImVec2(button_size, button_size)))
	{
		*v = v_default;
		value_changed = true;
	}
	ImGui::SameLine(0, style.ItemInnerSpacing.x);
	ImGui::Text(label.c_str());

	return value_changed;
}

#pragma once

#include "encounter_log.h"
#include "settings.h"

#include "../imgui/imgui.h"

namespace ImGui
{
	class ID
	{
	public:
		ID(const char* id) { ImGui::PushID(id); }
		ID(std::string id) { ImGui::PushID(id.c_str()); }
		~ID() { ImGui::PopID(); }
	};

	void ClipWindowToScreen();
	bool SmallCheckbox(const char* label, bool* v);
	bool ButtonDisabled(const char* label, bool disabled);
	bool ButtonParser(ParseStatus status);
	void DelayedTooltipText(const std::string& text, double delay = .85);
	bool KeySelector(const char* label, Hotkey* v);
	void Indicator(ImVec4 color);
	bool ButtonDpsReport(DpsReportUploadStatus status);
	bool ButtonWingman(WingmanUploadStatus status, ParseStatus parse_status);
	void CenterNextItemHorizontally(const char* text);
	inline void SmallSpacing() { ImGui::Dummy(ImVec2(0.f, 5.f)); }
	bool EncounterSelector(const char* label, EncounterSelection* value);
}

inline bool operator!=(const ImVec2& lhs, const ImVec2& rhs)
{
	return lhs.x != rhs.x || lhs.y != rhs.y;
}

inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs)
{
	return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}

namespace Color
{
	static const auto Green = ImVec4(.38f, 1.f, .38f, 1.f);
	static const auto Red = ImVec4(1.f, .38f, .38f, 1.f);
	static const auto Yellow = ImVec4(1.f, 1.f, .38f, 1.f);
	static const auto Blue = ImVec4(.38f, .38f, 1.f, 1.f);
	static const auto Orange = ImVec4(1.f, .5f, .38f, 1.f);
	static const auto Purple = ImVec4(.5f, .38f, 1.f, 1.f);
	static const auto Cyan = ImVec4(.38f, 1.f, 1.f, 1.f);
	static const auto Gray = ImVec4(.5f, .5f, .5f, 1.f);
	static const auto LightGray = ImVec4(.7f, .7f, .7f, 1.f);
	static const auto DarkGray = ImVec4(.3f, .3f, .3f, 1.f);
	static const auto White = ImVec4(1.f, 1.f, 1.f, 1.f);
	static const auto Black = ImVec4(0.f, 0.f, 0.f, 1.f);
	static const auto Transparent = ImVec4(0.f, 0.f, 0.f, 0.f);
	static const auto Disabled = ImGui::ColorConvertU32ToFloat4(0xFFA3A3A3);
}

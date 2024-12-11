#include "arcdps.h"
#include "directory_monitor.h"
#include "dps_report_uploader.h"
#include "elite_insights.h"
#include "global.h"
#include "log_manager.h"
#include "logger.h"
#include "mumble_link.h"
#include "settings.h"
#include "ui.h"
#include "wingman_uploader.h"

#include "../imgui/imgui.h"

#include <mini/ini.h>

#include <d3d11.h>
#include <ShlObj.h>
#include <Windows.h>

#define LOG(message, log_level) global::logger->write(message, log_level, LogSource::Core)

std::atomic<bool> initialized = false;

arcdps_exports arc_exports;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

uintptr_t mod_wnd(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!initialized.load())
		return uMsg;

	switch (uMsg)
	{
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	{
		if (!wParam)
			break;

		auto hotkey = GET_SETTING(display.hotkey);

		if (hotkey.key == wParam)
		{
			auto ctrl = hotkey.ctrl && (GetKeyState(VK_CONTROL) & 0x8000);
			auto shift = hotkey.shift && (GetKeyState(VK_SHIFT) & 0x8000);
			auto alt = hotkey.alt && (GetKeyState(VK_MENU) & 0x8000);

			if (hotkey.ctrl == ctrl && hotkey.shift == shift && hotkey.alt == alt)
				global::ui->on_open_hotkey();

			return 0;
		}

		break;
	}
	}

	return uMsg;
}

uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id, uint64_t revision)
{
	if (!ev && !src->elite && src->prof && dst->self && src->name != nullptr && dst->name != nullptr)
	{
		if (!initialized.load())
			return 0;

		std::string account_name(dst->name);

		if (account_name.empty())
			return 0;

		if (account_name.at(0) == ':')
			account_name.erase(0, 1);

		LOG("Account name set to: " + account_name, LogLevel::Debug);

		static auto mumble_link_disabled = false;

		if (!mumble_link_disabled && !global::mumble_link->is_initialized())
		{
			if (!global::mumble_link->initialize())
				if (!global::mumble_link->initialize("MumbleLink_" + account_name)) // fallback for multiboxing support
				{
					mumble_link_disabled = true;
					LOG("Mumble link disabled: Unable to initialize under 'MumbleLink' or 'MumbleLink_" + account_name + "'", LogLevel::Error);
				}
		}
	}

	return 0;
}

uintptr_t mod_imgui(uint32_t not_charsel_or_loading)
{
	if (!not_charsel_or_loading)
		return 0;

	if (!initialized.load())
		return 0;

	global::ui->draw();

	return 0;
}

uintptr_t mod_options_windows(const char* window_name)
{
	if (window_name)
		return 0;

	if (!initialized.load())
		return 0;

	global::ui->draw_arc_window_options();

	return 0;
}

uintptr_t mod_options_end()
{
	if (!initialized.load())
		return 0;

	global::ui->draw_arc_mod_options();

	return 0;
}

arcdps_exports* mod_init()
{
	memset(&arc_exports, 0, sizeof(arcdps_exports));
	arc_exports.sig = 0xE41A0F2;
	arc_exports.imguivers = IMGUI_VERSION_NUM;
	arc_exports.size = sizeof(arcdps_exports);
	arc_exports.out_name = "Log Uploader";
	arc_exports.out_build = global::version;
	arc_exports.wnd_nofilter = mod_wnd;
	arc_exports.combat = mod_combat;
	arc_exports.imgui = mod_imgui;
	arc_exports.options_windows = mod_options_windows;
	arc_exports.options_end = mod_options_end;

	return &arc_exports;
}

void mod_release() {}

std::mutex initialization_mutex = {};
std::thread initialization_thread = {};

extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, ImGuiContext* imguictx, void* id3dptr, HANDLE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion)
{
	auto initialize = [&]() -> void
		{
			std::lock_guard lock(initialization_mutex);

			if (initialized.load())
				throw std::runtime_error("Already initialized");

			std::unique_ptr<WCHAR[]> module_path = std::make_unique<WCHAR[]>(MAX_PATH);

			std::filesystem::path data_path;

			if (GetModuleFileName(NULL, module_path.get(), MAX_PATH) > 0)
			{
				data_path = std::filesystem::path(module_path.get()).parent_path() / "addons" / "log-uploader";

				if (!std::filesystem::exists(data_path))
					if (!std::filesystem::create_directories(data_path))
						throw std::runtime_error("Failed to create data directory");

				global::logger->initialize(data_path / "log.txt", arcdll);
			}
			else
				return;

			auto boss_encounter_path = std::filesystem::path();

			if (auto get_ini_path = reinterpret_cast<wchar_t* (*)()>(reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(arcdll), "e0"))); get_ini_path)
			{
				auto ini_path = std::filesystem::path(get_ini_path());

				if (!ini_path.empty() && std::filesystem::exists(ini_path))
				{
					try
					{
						mINI::INIFile ini_file(ini_path.string());

						mINI::INIStructure ini;

						if (ini_file.read(ini))
						{
							if (ini.has("session") && ini.get("session").has("boss_encounter_path") && !ini.get("session").get("boss_encounter_path").empty())
								boss_encounter_path = std::filesystem::path(ini.get("session").get("boss_encounter_path")) / "arcdps.cbtlogs";
							else
								LOG("boss_encounter_path not found in arcdps.ini", LogLevel::Warning);
						}
						else
							LOG("Failed to read arcdps.ini", LogLevel::Warning);
					}
					catch (std::exception& e)
					{
						LOG(e.what(), LogLevel::Error);
					}
				}
				else
					LOG("arcdps.ini not found", LogLevel::Warning);
			}
			else
				return;

			if (boss_encounter_path.empty())
			{
				PWSTR path = nullptr;

				if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &path)) && path != nullptr)
				{
					std::unique_ptr<wchar_t, decltype(&::CoTaskMemFree)> guard(path, ::CoTaskMemFree);

					const auto size = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);

					if (size > 0)
					{
						std::string string(size, 0);
						WideCharToMultiByte(CP_UTF8, 0, path, -1, &string[0], size, NULL, NULL);
						string.resize(static_cast<std::basic_string<char, std::char_traits<char>, std::allocator<char>>::size_type>(size) - 1);

						if (!string.empty())
						{
							const auto documents_path = std::filesystem::path(string);
							boss_encounter_path = documents_path / "Guild Wars 2" / "addons" / "arcdps" / "arcdps.cbtlogs";
						}
					}
				}
			}

#ifdef _DEBUG
			boss_encounter_path = std::filesystem::path(module_path.get()).parent_path() / "addons" / "arcdps" / "arcdps.cbtlogs";

			if (!std::filesystem::exists(boss_encounter_path))
				if (!std::filesystem::create_directories(boss_encounter_path))
					return;
#endif // _DEBUG

			if (boss_encounter_path.empty())
			{
				LOG("Unable to determine boss encounter path.", LogLevel::Error);
				return;
			}

			if (!std::filesystem::exists(boss_encounter_path))
			{
				LOG("Boss encounter path does not exist", LogLevel::Error);
				return;
			}

			ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(imguictx));
			ImGui::SetAllocatorFunctions(reinterpret_cast<void* (*)(size_t, void*)>(mallocfn), reinterpret_cast<void (*)(void*, void*)>(freefn)); // on imgui 1.80+

			/*ID3D11Device* d3d_device = nullptr;
			if (auto dx_swap_chain = static_cast<IDXGISwapChain*>(id3dptr))
				dx_swap_chain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&d3d_device));*/

			global::settings->initialize(data_path / "settings.json");
			global::mumble_link->initialize();
			global::ui->initialize();

			initialization_thread = std::thread([data_path, boss_encounter_path]() -> void
				{
					global::elite_insights->initialize(data_path / "elite-insights", data_path / "data");
					global::directory_monitor->initialize(boss_encounter_path);
					global::dps_report_uploader->initialize();
					global::wingman_uploader->initialize();

					LOG("Initialized", LogLevel::Info);
				});

			initialized.store(true);
		};

	initialize();

	return mod_init;
}

extern "C" __declspec(dllexport) void* get_release_addr()
{
	auto release = [&]() -> void
		{
			std::lock_guard lock(initialization_mutex);

			if (!initialized.exchange(false))
				return;

			if (initialization_thread.joinable())
				initialization_thread.join();

			global::directory_monitor->release();
			global::elite_insights->release();
			global::dps_report_uploader->release();
			global::wingman_uploader->release();
			global::ui->release();
			global::mumble_link->release();
			global::settings->release();
			global::logger->release();
		};

	release();

	return mod_release;
}

#undef LOG
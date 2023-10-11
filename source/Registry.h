#pragma once

#include <cstdint>
#include <optional>
#include <string>

// Portability stuff
namespace Registry
{
	inline const wchar_t* ACCLAIM_SECTION_NAME = L"Acclaim";

	inline const wchar_t* WIDESCREEN_KEY_NAME = L"Widescreen";
	inline const wchar_t* UNLOCK_KEY_NAME = L"UnlockAllContent";
	inline const wchar_t* ALL_UNLOCK_KEY_NAME = L"UnlockAllMenus";
	inline const wchar_t* DRIVER_NAME_KEY_NAME = L"DriverName";

	bool Init();
	void ApplyPatches(void* module);

	std::optional<uint32_t> GetDword(const wchar_t* section, const wchar_t* key);
	std::optional<std::string> GetAnsiString(const wchar_t* section, const wchar_t* key);
}

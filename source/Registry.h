#pragma once

#include <cstdint>
#include <optional>

#include <guiddef.h>

// Portability stuff
namespace Registry
{
	inline const wchar_t* REGISTRY_SECTION_NAME = L"Registry";
	inline const wchar_t* ACCLAIM_SECTION_NAME = L"Acclaim";

	bool Init();
	void ApplyPatches(void* module);

	std::optional<uint32_t> GetRegistryDword(const wchar_t* section, const wchar_t* key);
	std::optional<CLSID> GetRegistryCLSID(const wchar_t* section, const wchar_t* key);

	void SetRegistryDword(const wchar_t* section, const wchar_t* key, uint32_t value);
	void SetRegistryCLSID(const wchar_t* section, const wchar_t* key, const CLSID& value);
}

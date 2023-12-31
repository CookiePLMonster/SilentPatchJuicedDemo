#include "Registry.h"

#include <filesystem>
#include <string>

#include <guiddef.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <wil/win32_helpers.h>

static std::wstring pathToPatchIni = L".\\" rsc_Name ".ini";
static std::wstring pathToGameIni = L".\\settings.ini";

namespace Registry
{
	inline const wchar_t* REGISTRY_SECTION_NAME = L"Registry";

	std::optional<int32_t> GetRegistryInt(const wchar_t* section, const wchar_t* key, const std::wstring& path);
	std::optional<uint32_t> GetRegistryDword(const wchar_t* section, const wchar_t* key, const std::wstring& path);
	std::optional<CLSID> GetRegistryCLSID(const wchar_t* section, const wchar_t* key, const std::wstring& path);
	std::optional<std::string> GetRegistryAnsiString(const wchar_t* section, const wchar_t* key, const std::wstring& path);
	std::optional<std::wstring> GetRegistryString(const wchar_t* section, const wchar_t* key, const std::wstring& path);

	void SetRegistryDword(const wchar_t* section, const wchar_t* key, uint32_t value, const std::wstring& path);
	void SetRegistryCLSID(const wchar_t* section, const wchar_t* key, const CLSID& value, const std::wstring& path);
}

static std::wstring AnsiToWchar(std::string_view text)
{
	std::wstring result;

	const int count = MultiByteToWideChar(CP_ACP, 0, text.data(), text.size(), nullptr, 0);
	if (count != 0)
	{
		result.resize(count);
		MultiByteToWideChar(CP_ACP, 0, text.data(), text.size(), result.data(), count);
	}

	return result;
}

static std::string WcharToAnsi(std::wstring_view text)
{
	std::string result;

	const int count = WideCharToMultiByte(CP_ACP, 0, text.data(), text.size(), nullptr, 0, nullptr, nullptr);
	if (count != 0)
	{
		result.resize(count);
		WideCharToMultiByte(CP_ACP, 0, text.data(), text.size(), result.data(), count, nullptr, nullptr);
	}

	return result;
}

bool Registry::Init()
{
	bool gotPathToPatchIni = false, gotPathToGameIni = false;

	wil::unique_cotaskmem_string pathToAsi, pathToGame;
	if (SUCCEEDED(wil::GetModuleFileNameW(wil::GetModuleInstanceHandle(), pathToAsi)))
	{
		try
		{
			pathToPatchIni = std::filesystem::path(pathToAsi.get()).replace_extension(L"ini").wstring();
			gotPathToPatchIni = true;
		}
		catch (const std::filesystem::filesystem_error&)
		{
		}
	}
	if (SUCCEEDED(wil::GetModuleFileNameW(nullptr, pathToGame)))
	{
		try
		{
			pathToGameIni = std::filesystem::path(pathToGame.get()).replace_filename(L"settings.ini").wstring();
			gotPathToGameIni = true;
		}
		catch (const std::filesystem::filesystem_error&)
		{
		}
	}
	return gotPathToPatchIni && gotPathToGameIni;
}

std::optional<int32_t> Registry::GetInt(const wchar_t* section, const wchar_t* key)
{
	return GetRegistryInt(section, key, pathToPatchIni);
}

std::optional<uint32_t> Registry::GetDword(const wchar_t* section, const wchar_t* key)
{
	return GetRegistryDword(section, key, pathToPatchIni);
}

std::optional<std::string> Registry::GetAnsiString(const wchar_t* section, const wchar_t* key)
{
	return GetRegistryAnsiString(section, key, pathToPatchIni);
}

std::optional<std::wstring> Registry::GetString(const wchar_t* section, const wchar_t* key)
{
	return GetRegistryString(section, key, pathToPatchIni);
}

std::optional<int32_t> Registry::GetRegistryInt(const wchar_t* section, const wchar_t* key, const std::wstring& path)
{
	std::optional<int32_t> result;
	const INT val = GetPrivateProfileIntW(section, key, INT_MIN, path.c_str());
	if (val != INT_MIN)
	{
		result.emplace(val);
	}
	return result;
}

std::optional<uint32_t> Registry::GetRegistryDword(const wchar_t* section, const wchar_t* key, const std::wstring& path)
{
	std::optional<uint32_t> result;
	const INT val = GetPrivateProfileIntW(section, key, -1, path.c_str());
	if (val >= 0)
	{
		result.emplace(static_cast<uint32_t>(val));
	}
	return result;
}

std::optional<CLSID> Registry::GetRegistryCLSID(const wchar_t* section, const wchar_t* key, const std::wstring& path)
{
	std::optional<CLSID> result;

	wchar_t buf[wil::guid_string_buffer_length];
	GetPrivateProfileStringW(section, key, L"", buf, static_cast<DWORD>(std::size(buf)), path.c_str());
	if (buf[0] != '\0')
	{
		CLSID clsid;
		if (SUCCEEDED(CLSIDFromString(buf, &clsid)))
		{
			result.emplace(clsid);
		}
	}
	return result;
}

std::optional<std::string> Registry::GetRegistryAnsiString(const wchar_t* section, const wchar_t* key, const std::wstring& path)
{
	std::optional<std::string> result;

	wchar_t buf[128];
	GetPrivateProfileStringW(section, key, L"", buf, static_cast<DWORD>(std::size(buf)), path.c_str());
	if (buf[0] != '\0')
	{
		result.emplace(WcharToAnsi(buf));
	}
	return result;
}

std::optional<std::wstring> Registry::GetRegistryString(const wchar_t* section, const wchar_t* key, const std::wstring& path)
{
	std::optional<std::wstring> result;

	wchar_t buf[128];
	GetPrivateProfileStringW(section, key, L"", buf, static_cast<DWORD>(std::size(buf)), path.c_str());
	if (buf[0] != '\0')
	{
		result.emplace(buf);
	}
	return result;
}

void Registry::SetRegistryDword(const wchar_t* section, const wchar_t* key, uint32_t value, const std::wstring& path)
{
	WritePrivateProfileStringW(section, key, std::to_wstring(value).c_str(), path.c_str());
}

void Registry::SetRegistryCLSID(const wchar_t* section, const wchar_t* key, const CLSID& value, const std::wstring& path)
{
	wchar_t buf[wil::guid_string_buffer_length];
	if (StringFromGUID2(value, buf, std::size(buf)) != 0)
	{
		WritePrivateProfileStringW(section, key, buf, path.c_str());
	}
}

static decltype(::RegCreateKeyExA)* orgRegCreateKeyExA;
static LSTATUS WINAPI RegCreateKeyExA_Redirect(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass, DWORD dwOptions, REGSAM samDesired,
	const LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition)
{
	if (hKey == HKEY_CURRENT_USER && strstr(lpSubKey, "\\Juiced") != nullptr)
	{
		// Return a "pseudo-handle"
		*phkResult = reinterpret_cast<HKEY>(&pathToGameIni);
		return ERROR_SUCCESS;
	}
	return orgRegCreateKeyExA(hKey, lpSubKey, Reserved, lpClass, dwOptions, samDesired, lpSecurityAttributes, phkResult, lpdwDisposition);
}

static decltype(::RegCloseKey)* orgRegCloseKey;
static LSTATUS WINAPI RegCloseKey_Redirect(HKEY hKey)
{
	if (hKey == reinterpret_cast<HKEY>(&pathToGameIni))
	{
		return ERROR_SUCCESS;
	}
	return orgRegCloseKey(hKey);
}

static decltype(::RegQueryValueExA)* orgRegQueryValueExA;
static LSTATUS WINAPI RegQueryValueExA_Redirect(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
	if (hKey == reinterpret_cast<HKEY>(&pathToGameIni))
	{
		if (lpValueName == nullptr)
		{
			return ERROR_SUCCESS;
		}
		if (_stricmp(lpValueName, "Adapter") == 0)
		{
			auto guid = Registry::GetRegistryCLSID(Registry::REGISTRY_SECTION_NAME, AnsiToWchar(lpValueName).c_str(), pathToGameIni);
			if (guid)
			{
				if (lpData != nullptr && lpcbData != nullptr)
				{
					const DWORD bytesToWrite = std::min<DWORD>(*lpcbData, sizeof(*guid));
					memcpy(lpData, &guid.value(), bytesToWrite);
					*lpcbData = bytesToWrite;
				}
				return ERROR_SUCCESS;
			}
			return ERROR_FILE_NOT_FOUND;
		}

		// Everything else is integers
		auto value = Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, AnsiToWchar(lpValueName).c_str(), pathToGameIni);
		if (value)
		{
			if (lpData != nullptr && lpcbData != nullptr)
			{
				const DWORD bytesToWrite = std::min<DWORD>(*lpcbData, sizeof(*value));
				memcpy(lpData, &value.value(), bytesToWrite);
				*lpcbData = bytesToWrite;
			}
			return ERROR_SUCCESS;
		}
		return ERROR_FILE_NOT_FOUND;
	}
	return orgRegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}

static decltype(::RegSetValueExA)* orgRegSetValueExA;
static LSTATUS WINAPI RegSetValueExA_Redirect(HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE *lpData, DWORD cbData)
{
	if (hKey == reinterpret_cast<HKEY>(&pathToGameIni))
	{
		if (lpValueName == nullptr)
		{
			return ERROR_SUCCESS;
		}

		if (_stricmp(lpValueName, "Adapter") == 0)
		{
			if (cbData >= sizeof(CLSID))
			{
				Registry::SetRegistryCLSID(Registry::REGISTRY_SECTION_NAME, AnsiToWchar(lpValueName).c_str(), *reinterpret_cast<const CLSID*>(lpData), pathToGameIni);
			}
			return ERROR_SUCCESS;
		}

		// Everything else is integers
		if (cbData >= sizeof(DWORD))
		{
			Registry::SetRegistryDword(Registry::REGISTRY_SECTION_NAME, AnsiToWchar(lpValueName).c_str(), *reinterpret_cast<const DWORD*>(lpData), pathToGameIni);
		}
		return ERROR_SUCCESS;
	}
	return orgRegSetValueExA(hKey, lpValueName, Reserved, dwType, lpData, cbData);
}

static void ReplaceFunction_RegCreateKeyExA(void** funcPtr)
{
	DWORD dwProtect;

	auto func = reinterpret_cast<decltype(::RegCreateKeyExA)**>(funcPtr);

	VirtualProtect(func, sizeof(*func), PAGE_READWRITE, &dwProtect);
	orgRegCreateKeyExA = std::exchange(*func, &RegCreateKeyExA_Redirect);
	VirtualProtect(func, sizeof(*func), dwProtect, &dwProtect);
}

static void ReplaceFunction_RegCloseKey(void** funcPtr)
{
	DWORD dwProtect;

	auto func = reinterpret_cast<decltype(::RegCloseKey)**>(funcPtr);

	VirtualProtect(func, sizeof(*func), PAGE_READWRITE, &dwProtect);
	orgRegCloseKey = std::exchange(*func, &RegCloseKey_Redirect);
	VirtualProtect(func, sizeof(*func), dwProtect, &dwProtect);
}

static void ReplaceFunction_RegQueryValueExA(void** funcPtr)
{
	DWORD dwProtect;

	auto func = reinterpret_cast<decltype(::RegQueryValueExA)**>(funcPtr);

	VirtualProtect(func, sizeof(*func), PAGE_READWRITE, &dwProtect);
	orgRegQueryValueExA = std::exchange(*func, &RegQueryValueExA_Redirect);
	VirtualProtect(func, sizeof(*func), dwProtect, &dwProtect);
}

static void ReplaceFunction_RegSetValueExA(void** funcPtr)
{
	DWORD dwProtect;

	auto func = reinterpret_cast<decltype(::RegSetValueExA)**>(funcPtr);

	VirtualProtect(func, sizeof(*func), PAGE_READWRITE, &dwProtect);
	orgRegSetValueExA = std::exchange(*func, &RegSetValueExA_Redirect);
	VirtualProtect(func, sizeof(*func), dwProtect, &dwProtect);
}

void Registry::ApplyPatches(void* module)
{
	const DWORD_PTR instance = reinterpret_cast<DWORD_PTR>(module);
	const PIMAGE_NT_HEADERS ntHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(instance + reinterpret_cast<PIMAGE_DOS_HEADER>(instance)->e_lfanew);

	// Find IAT
	PIMAGE_IMPORT_DESCRIPTOR pImports = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(instance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	for ( ; pImports->Name != 0; pImports++ )
	{
		if ( _stricmp(reinterpret_cast<const char*>(instance + pImports->Name), "advapi32.dll") == 0 )
		{
			if ( pImports->OriginalFirstThunk != 0 )
			{
				const PIMAGE_THUNK_DATA pThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(instance + pImports->OriginalFirstThunk);

				for ( ptrdiff_t j = 0; pThunk[j].u1.AddressOfData != 0; j++ )
				{
					if ( strcmp(reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(instance + pThunk[j].u1.AddressOfData)->Name, "RegCreateKeyExA") == 0 )
					{
						void** pAddress = reinterpret_cast<void**>(instance + pImports->FirstThunk) + j;
						ReplaceFunction_RegCreateKeyExA(pAddress);
					}
					else if ( strcmp(reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(instance + pThunk[j].u1.AddressOfData)->Name, "RegCloseKey") == 0 )
					{
						void** pAddress = reinterpret_cast<void**>(instance + pImports->FirstThunk) + j;
						ReplaceFunction_RegCloseKey(pAddress);
					}
					else if ( strcmp(reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(instance + pThunk[j].u1.AddressOfData)->Name, "RegQueryValueExA") == 0 )
					{
						void** pAddress = reinterpret_cast<void**>(instance + pImports->FirstThunk) + j;
						ReplaceFunction_RegQueryValueExA(pAddress);
					}
					else if ( strcmp(reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(instance + pThunk[j].u1.AddressOfData)->Name, "RegSetValueExA") == 0 )
					{
						void** pAddress = reinterpret_cast<void**>(instance + pImports->FirstThunk) + j;
						ReplaceFunction_RegSetValueExA(pAddress);
					}
				}
			}
			else
			{
				void** pFunctions = reinterpret_cast<void**>(instance + pImports->FirstThunk);

				for ( ptrdiff_t j = 0; pFunctions[j] != nullptr; j++ )
				{
					if ( pFunctions[j] == &RegCreateKeyExA )
					{
						ReplaceFunction_RegCreateKeyExA(&pFunctions[j]);
					}
					else if ( pFunctions[j] == &RegCloseKey )
					{
						ReplaceFunction_RegCloseKey(&pFunctions[j]);
					}
					else if ( pFunctions[j] == &RegQueryValueExA )
					{
						ReplaceFunction_RegQueryValueExA(&pFunctions[j]);
					}
					else if ( pFunctions[j] == &RegSetValueExA )
					{
						ReplaceFunction_RegSetValueExA(&pFunctions[j]);
					}
				}
			}
		}
	}
}
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <mmreg.h>
#include <dsound.h>

#include <cmath>
#include <cstdio>
#include <filesystem>

#include <wil/resource.h>
#include <wil/win32_helpers.h>

#include "Registry.h"

#include "Utils/MemoryMgr.h"
#include "Utils/Patterns.h"
#include "Utils/ScopedUnprotect.hpp"


// Stub always returning DX 9.0c, because this function cannot be reached anyway if DX9 is not installed
HRESULT GetDirectXVersion_Stub(int* major, int* minor, char* letter)
{
	if (major != nullptr)
	{
		*major = 9;
	}
	if (minor != nullptr)
	{
		*minor = 0;
	}
	if (letter != nullptr)
	{
		*letter = 'c';
	}

	return S_OK;
}

// Acclaim Juiced June/July: Fix a FPU stack corruption caused by a LockVertexBuffer function
// Callers seem to assume that this function does not affect the x87 FPU stack, but it calls into
// a D3D9 function without preserving it at all, so it cannot be guaranteed
namespace FPUCorruptionFix
{
	static void* LockVertexBuffer_CallBack;
	__declspec(naked) void LockVertexBuffer_SaveFPU()
	{
		_asm
		{
			push	ebp
			mov		ebp, esp
			and		esp, -16
			sub		esp, 512
			fxsave	[esp]

			// Original function
			mov		eax, [esi+8]
			cmp		edi, eax
			call	[LockVertexBuffer_CallBack]

			fxrstor	[esp]
			mov		esp, ebp
			pop		ebp
			ret
		}
	}
}


namespace AudioCrackleFix
{
	HRESULT WINAPI SetNotificationPositions_FixPositions(IDirectSoundNotify* pDSNotify, DWORD cPositionNotifies, LPCDSBPOSITIONNOTIFY lpcPositionNotifies)
	{
		// Failsafe
		if (cPositionNotifies != 3)
		{
			return pDSNotify->SetNotificationPositions(cPositionNotifies, lpcPositionNotifies);
		}
		// Take the value from the first element in the array
		const DWORD singleBufferSize = lpcPositionNotifies[0].dwOffset + 1;

		DSBPOSITIONNOTIFY positionNotifies[3];
		positionNotifies[0].dwOffset = singleBufferSize / 2;
		positionNotifies[0].hEventNotify = lpcPositionNotifies[0].hEventNotify;
		positionNotifies[1].dwOffset = positionNotifies[0].dwOffset + singleBufferSize;
		positionNotifies[1].hEventNotify = lpcPositionNotifies[1].hEventNotify;
		positionNotifies[2].dwOffset = positionNotifies[1].dwOffset + singleBufferSize;
		positionNotifies[2].hEventNotify = lpcPositionNotifies[2].hEventNotify;

		return pDSNotify->SetNotificationPositions(std::size(positionNotifies), positionNotifies);
	}

	__declspec(naked) void SetNotificationPositions_Hook()
	{
		_asm
		{
			push	dword ptr [esp+12]
			push	dword ptr [esp+4+8]
			push	dword ptr [esp+8+4]
			call	SetNotificationPositions_FixPositions
			test	eax, eax
			retn	12
		}
	}
};


namespace AcclaimWidescreen
{
	float aspectRatioMult = 1.0f;
	float aspectRatioMultInv = 1.0f;
	static void CalculateAR(uint32_t width, uint32_t height)
	{
		const double originalMult = std::atan(320.0 / 480.0);
		const double currentMult = std::atan((static_cast<double>(width) / 2.0) / height);

		aspectRatioMult = static_cast<float>(currentMult / originalMult);
		aspectRatioMultInv = static_cast<float>(originalMult / currentMult);
	}

	static void* (*orgCreateWindow)();
	__declspec(naked) static void* CreateWindow_CalculateAR()
	{
		uint32_t width, height;
		void* a2;
		_asm
		{
			push	ebp
			mov		ebp, esp
			sub		esp, __LOCAL_SIZE

			mov		[width], eax
			mov		[height], ecx
			mov		[a2], edx
		}

		CalculateAR(width, height);

		_asm
		{
			mov		eax, [width]
			mov		ecx, [height]
			mov		edx, [a2]
			mov		esp, ebp
			pop		ebp
			jmp		[orgCreateWindow]
		}
	}
}


static bool ToyotaMR2FilesPresent()
{
	try
	{
		return std::filesystem::exists(L"cars/mr2.dat") && std::filesystem::exists(L"cars/mr2_ui.dat") &&
			std::filesystem::exists(L"scripts/Demo2Unlock.txt");
	}
	catch (const std::filesystem::filesystem_error&)
	{
	}
	return false;
}

static bool VideoFilesPresent()
{
	try
	{
		return std::filesystem::exists(L"movies/video1.bik") && std::filesystem::exists(L"movies/video2.bik") &&
			std::filesystem::exists(L"movies/video3.bik");
	}
	catch (const std::filesystem::filesystem_error&)
	{
	}
	return false;
}

static bool bVideoFilesPresent = false;
static bool bAllEntriesUnlocked = false;
__declspec(naked) void ShouldUnlockMenuEntry()
{
	_asm
	{
		cmp		[bAllEntriesUnlocked], 1
		je		ShouldUnlockMenuEntry_Return
		test	eax, eax
		je		ShouldUnlockMenuEntry_Return
		cmp		eax, 2
		je		ShouldUnlockMenuEntry_Return
		cmp		eax, 9
		je		ShouldUnlockMenuEntry_Return

		cmp		[bVideoFilesPresent], 1
		jne		ShouldUnlockMenuEntry_Return
		cmp		eax, 5

		ShouldUnlockMenuEntry_Return:
		retn
	}
}

__declspec(naked) void ShouldUnlockMenuEntry_May()
{
	_asm
	{
		cmp		[bAllEntriesUnlocked], 1
		je		ShouldUnlockMenuEntry_Return
		test	eax, eax
		je		ShouldUnlockMenuEntry_Return
		cmp		eax, 1
		je		ShouldUnlockMenuEntry_Return
		cmp		eax, 9

		ShouldUnlockMenuEntry_Return:
		retn
	}
}

void OnInitializeHook()
{
	using namespace Memory;
	using namespace hook::txn;

	const HMODULE hModule = GetModuleHandle(nullptr);
	auto Protect = ScopedUnprotect::UnprotectSectionOrFullModule(hModule, ".text");

#ifndef NDEBUG
	wil::unique_file hFile;
	_wfopen_s(hFile.put(), L"patches.log", L"w");

	auto Log = [&hFile](const char* format, auto... args)
	{
		fprintf_s(hFile.get(), format, args...);
		fprintf_s(hFile.get(), "\n");
	};
#else
	// Empty log
#define Log(...)
#endif

	// Redirect registry to the INI file
	const bool HasRegistry = Registry::Init();
	if (HasRegistry)
	{
		Registry::ApplyPatches(hModule);
	}

	// JuicedConfig: Enable all resolutions in windowed mode (Acclaim)
	try
	{
		auto is_windowed = get_pattern("56 0F 85 ? ? ? ? FF D7 50 FF D3", 1);
		Patch(is_windowed, { 0x90, 0xE9 });

		Log("Done: Enable all windowed mode resolutions (Acclaim)");
	}
	TXN_CATCH();


	// JuicedConfig: Enable all resolutions in windowed mode (Acclaim Debug)
	try
	{
		auto is_windowed = get_pattern("83 F8 01 0F 85 ? ? ? ? 8B F4", 3);
		Patch(is_windowed, { 0x90, 0xE9 });

		Log("Done: Enable all windowed mode resolutions (Acclaim Debug)");
	}
	TXN_CATCH();


	// JuicedConfig: Enable all resolutions in windowed mode (THQ)
	try
	{
		auto is_windowed = get_pattern("53 0F 85 ? ? ? ? FF D6 50 FF D7", 1);
		Patch(is_windowed, { 0x90, 0xE9 });

		Log("Done: Enable all windowed mode resolutions (THQ)");
	}
	TXN_CATCH();


	// JuicedConfig: Shim GetDirectXVersion
	try
	{
		auto get_version = get_pattern("53 57 32 DB 33 FF 57 89 44 24 48", -8);
		InjectHook(get_version, GetDirectXVersion_Stub, HookType::Jump);

		Log("Done: GetDirectXVersion_Stub");
	}
	TXN_CATCH();


	// JuicedConfig (Debug build): Shim GetDirectXVersion
	try
	{
		auto get_version = get_pattern("53 56 57 8D BD ? ? ? ? B9 ? ? ? ? B8 ? ? ? ? F3 AB C6 45 EF 00", -9);
		InjectHook(get_version, GetDirectXVersion_Stub, HookType::Jump);

		Log("Done: GetDirectXVersion_Stub (Debug)");
	}
	TXN_CATCH();


	// Acclaim Juiced June/July: Fix a FPU stack corruption caused by a LockVertexBuffer function
	// Callers seem to assume that this function does not affect the x87 FPU stack, but it calls into
	// a D3D9 function without preserving it at all, so it cannot be guaranteed
	try
	{
		using namespace FPUCorruptionFix;

		auto lock_vb = pattern("53 8D 5E 1C C7 03 ? ? ? ? 76 04 33 C0 5B C3").get_one();

		LockVertexBuffer_CallBack = lock_vb.get<void>();
		InjectHook(lock_vb.get<void>(-5), LockVertexBuffer_SaveFPU, HookType::Jump);
	}
	TXN_CATCH();


	// Juiced Acclaim: Notify the music thread it's time to stream new music earlier
	// Fixes music crackling due to the new data arriving too late
	try
	{
		using namespace AudioCrackleFix;

		auto set_notifications = get_pattern("FF 51 0C 85 C0 74 0F");
		InjectHook(set_notifications, SetNotificationPositions_Hook, HookType::Call);
	}
	TXN_CATCH();


	// Acclaim Juiced (May): Make Alt+F4 forcibly kill the process
	try
	{
		auto exit_process = get_pattern("75 11 6A 00 FF 15 ? ? ? ? 5F", 4);
		InjectHook(exit_process, &ExitProcess, HookType::Jump);
	}
	TXN_CATCH();


	// Acclaim Juiced: Proper widescreen
	if (Registry::GetDword(Registry::ACCLAIM_SECTION_NAME, Registry::WIDESCREEN_KEY_NAME).value_or(0) != 0) try
	{
		using namespace AcclaimWidescreen;

		auto set_ar_func = [] {
			try
			{
				// June/July
				return get_pattern("E8 ? ? ? ? 8B F8 85 FF 74 55");
			}
			catch (hook::txn_exception&)
			{
				// May
				return get_pattern("0F 94 C2 56 E8 ? ? ? ? 8B F8 85 FF", 4);
			}
		}();
		auto widescreen_flag_and_mult = pattern("A1 ? ? ? ? 85 C0 74 10 D9 44 24 04 D8 0D ? ? ? ? D9 99 A8 00 00 00").get_one();
		auto widescreen_div = get_pattern<float*>("D8 0D ? ? ? ? C3 D9 81 A8 00 00 00 C3", 2);

		InterceptCall(set_ar_func, orgCreateWindow, CreateWindow_CalculateAR);

		Patch(widescreen_flag_and_mult.get<void>(13 + 2), &aspectRatioMult);
		Patch(widescreen_div, &aspectRatioMultInv);

		**widescreen_flag_and_mult.get<BOOL*>(1) = 1;
	}
	TXN_CATCH();


	// Acclaim Juiced: Unlock a Toyota MR2 from the May demo (if present)
	if (ToyotaMR2FilesPresent()) try
	{
		auto demo_unlock = get_pattern("8B 49 2C 8B 11 8D 44 24 10 50 55 68 ? ? ? ? FF 12", 11 + 1);
		Patch<const char*>(demo_unlock, "Demo2Unlock.txt");
	}
	TXN_CATCH();


	// Acclaim Juiced: Unlock all content available in the demo
	if (Registry::GetDword(Registry::ACCLAIM_SECTION_NAME, Registry::UNLOCK_KEY_NAME).value_or(0) != 0)
	{
		// Each unlock is technically independent, so separate them in blocks
		try
		{
			try
			{
				// June/July
				auto courses_lock1 = get_pattern("83 7F 18 01 6A 01 68 ? ? ? ? 0F 84", 11);
				Patch(courses_lock1, {0x90, 0xE9});
			}
			catch (hook::txn_exception&)
			{
				// May
				auto courses_lock1 = get_pattern("74 0F 8B 94 24 ? ? ? ? 52");
				Patch<uint8_t>(courses_lock1, 0xEB);
			}

			auto courses_lock2 = pattern("8B 0C 81 3B CD 74 02 89 29").count_hint(4);

			courses_lock2.for_each_result([](pattern_match match)
				{
					Nop(match.get<void>(7), 2);
				});

			// Only disable forced Route 2 if all routes unlocked fine
			try
			{
				auto forced_course_begin = get_pattern_uintptr("72 B4 8B 47 10 8B 90 1C 01 00 00 2B 90 18 01 00 00", 2);
				auto forced_course_end = get_pattern_uintptr("E8 ? ? ? ? 39 2D ? ? ? ? 0F 84 ? ? ? ? 8B 4F 10 8B 91 1C 01 00 00", 5);

				Patch(forced_course_begin, {0xEB, static_cast<uint8_t>(forced_course_end - forced_course_begin - 2)});
			}
			TXN_CATCH();
		}
		TXN_CATCH();

		try
		{
			auto race_modes = pattern("8B CD 8B 0C 81 3B CB 74 02 89 19").count_hint(2);
			race_modes.for_each_result([](pattern_match match)
				{
					Nop(match.get<void>(9), 2);
				});
		}
		TXN_CATCH();
	
		try
		{
			auto up_to_6_laps = get_pattern("46 83 FE ? 89 74 24 08 0F 8C", 1 + 2);
			Patch<int8_t>(up_to_6_laps, 7);
		}
		TXN_CATCH();

		try
		{
			auto max_opponents_at_night = get_pattern("BB 04 00 00 00 8B 51 70 8B 42 54");
			Nop(max_opponents_at_night, 5);
		}
		TXN_CATCH();

		try
		{
			// June/July only
			auto arcade_menu_unlock = pattern("83 F8 02 74 2E 83 F8 09 74 29 83 F8 FF 7E 24").get_one();
			Nop(arcade_menu_unlock.get<void>(), 3);
			InjectHook(arcade_menu_unlock.get<void>(3), ShouldUnlockMenuEntry, HookType::Call);

			bVideoFilesPresent = VideoFilesPresent();
		}
		catch (hook::txn_exception&)
		{
			try
			{
				// May
				auto arcade_menu_unlock = pattern("85 C0 74 33 83 F8 09 74 2E 83 F8 05 74 29 83 F8 FF").get_one();
				Nop(arcade_menu_unlock.get<void>(), 2);
				InjectHook(arcade_menu_unlock.get<void>(2), ShouldUnlockMenuEntry_May, HookType::Call);

				bVideoFilesPresent = true;
			}
			TXN_CATCH();
		}

		// Also unlock all menu options if requested
		if (Registry::GetDword(Registry::ACCLAIM_SECTION_NAME, Registry::ALL_UNLOCK_KEY_NAME).value_or(0) != 0) try
		{
			bAllEntriesUnlocked = true;

			auto cheats_multiplay_hide = pattern("E8 ? ? ? ? BB ? ? ? ? E8 ? ? ? ? 8B 15 ? ? ? ?").get_one();

			Nop(cheats_multiplay_hide.get<void>(), 5);
			Nop(cheats_multiplay_hide.get<void>(10), 5);
		}
		TXN_CATCH();
	}


	// Acclaim Juiced: Custom driver names
	const auto customDriverName = Registry::GetAnsiString(Registry::ACCLAIM_SECTION_NAME, Registry::DRIVER_NAME_KEY_NAME);
	if (customDriverName) try
	{
		auto driver_name = get_pattern(" B8 ? ? ? ? 8D 4C 24 18 E8 ? ? ? ? 8B 7D 68", 1);
		Patch(driver_name, _strdup(customDriverName->c_str()));
	}
	TXN_CATCH();


	// THQ Juiced (January 2005): Fix a startup crash with more than 4 cores
	try
	{
		auto get_core_count = get_pattern("03 C8 83 F9 20 7C EE 5F", 2 + 2);
		Patch<uint8_t>(get_core_count, 4);
	}
	TXN_CATCH();
}

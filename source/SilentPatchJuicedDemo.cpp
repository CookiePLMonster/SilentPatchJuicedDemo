#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <mmreg.h>
#include <dsound.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include <wil/resource.h>
#include <wil/win32_helpers.h>

#include "Registry.h"

#include "Utils/MemoryMgr.h"
#include "Utils/Patterns.h"
#include "Utils/ScopedUnprotect.hpp"

char* strndup(const char *str, size_t size)
{
	const size_t len = std::min(std::strlen(str), size);
	char* mem = static_cast<char*>(std::malloc(len + 1));
	if (mem != nullptr)
	{
		std::memcpy(mem, str, len);
		mem[len] = '\0';
	}
	return mem;
}

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
	#pragma warning(suppress:4740)
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


static bool ToyotaMR2FilesPresent() try
{
	return std::filesystem::exists(L"cars/mr2.dat") && std::filesystem::exists(L"cars/mr2_ui.dat") &&
		std::filesystem::exists(L"scripts/Demo2Unlock.txt");
}
catch (const std::filesystem::filesystem_error&)
{
	return false;
}

static bool VideoFilesPresent() try
{
	return std::filesystem::exists(L"movies/video1.bik") && std::filesystem::exists(L"movies/video2.bik") &&
		std::filesystem::exists(L"movies/video3.bik");
}
catch (const std::filesystem::filesystem_error&)
{
	return false;
}

static bool CareerFilePresent() try
{
	return std::filesystem::exists(L"scripts/CMSPlayersCrewCollection2.txt");
}
catch (const std::filesystem::filesystem_error&)
{
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


namespace THQCustomizableRace
{
	struct RaceInfoInner
	{
		char field_0;
		char field_1;
		char field_2;
		int m_gameMode;
		int field_8;
		char field_C;
		char m_opponentPicked[9];
		char m_numOpponents;
		char field_17;
		char field_18;
		int m_trackNum;
		int field_20;
		uint8_t m_numLaps;
		int field_28;
		int field_2C;
		int field_30;
		int field_34;
	};

	struct RaceInfo
	{
		char field_0;
		char field_1;
		char field_2;
		char field_3;
		std::byte gap4[2];
		char field_6;
		char field_7;
		int field_8;
		char field_C;
		char field_D;
		int m_gameMode;
		int field_14;
		int field_18;
		RaceInfoInner m_trackInfo[1];
		char m_indexAlwaysZero;
		char field_55;
		bool m_opponentPresent[9];
		std::byte gap5f;
		char field_60[12];
		int field_6C;
		std::byte gap70[12];
		uint32_t m_timeOfDay;
		uint32_t m_weather;
	};
	static_assert(sizeof(RaceInfo) == 0x84, "Wrong size: RaceInfo");

	static void (__fastcall* orgSetupRace)(void*, void*, RaceInfo* raceInfo);
	static void __fastcall SetupRace_Customizable(void* a1, void* a2,  RaceInfo* raceInfo)
	{
		if (raceInfo->m_gameMode == 2) // Showoff
		{
			raceInfo->m_trackInfo[0].m_trackNum = 33;
			raceInfo->m_trackInfo[0].m_numLaps = 1;
		}
		else if (raceInfo->m_gameMode == 5) // Sprint
		{
			raceInfo->m_trackInfo[0].m_trackNum = 34;
			raceInfo->m_trackInfo[0].m_numLaps = 1;
		}
		else
		{
			raceInfo->m_trackInfo[0].m_numLaps = static_cast<int8_t>(std::clamp(Registry::GetInt(Registry::THQ_SECTION_NAME, L"Race.NumLaps").value_or(3), 1, 99));

			// Cruise - 33
			// Sprint - 34
			const auto routeStr = Registry::GetString(Registry::THQ_SECTION_NAME, L"Race.Route").value_or(L"Downtown_r1");
			const std::pair<const wchar_t*, uint32_t> routeChoices[] = { 
				{ L"Downtown_r1", 25 },
				{ L"Downtown_r2", 26 },
				{ L"Downtown_r3", 27 },
				{ L"Downtown_r4", 28 },
				{ L"Downtown_r1_rev", 29 },
				{ L"Downtown_r2_rev", 30 },
				{ L"Downtown_r3_rev", 31 },
				{ L"Downtown_r4_rev", 32 },
				{ L"Downtown_P2P", 35 },
				{ L"Downtown_P2P_rev", 36 },
			};
			for (const auto& choice : routeChoices)
			{
				if (_wcsicmp(routeStr.c_str(), choice.first) == 0)
				{
					raceInfo->m_trackInfo[0].m_trackNum = choice.second;
					break;
				}
			}
		}
		{
			const auto timeOfDayStr = Registry::GetString(Registry::THQ_SECTION_NAME, L"Race.TimeOfDay").value_or(L"morning");
			const std::pair<const wchar_t*, uint32_t> timeOfDayChoices[] = { 
				{ L"morning", 1 },
				{ L"afternoon", 2 },
				{ L"evening", 3 },
				{ L"night", 4 },
			};
			for (const auto& choice : timeOfDayChoices)
			{
				if (_wcsicmp(timeOfDayStr.c_str(), choice.first) == 0)
				{
					raceInfo->m_timeOfDay = choice.second;
					break;
				}
			}
		}
		{
			const auto weatherStr = Registry::GetString(Registry::THQ_SECTION_NAME, L"Race.Weather").value_or(L"clear");
			const std::pair<const wchar_t*, uint32_t> weatherChoices[] = { 
				{ L"clear", 1 },
				{ L"wet", 2 },
			};
			for (const auto& choice : weatherChoices)
			{
				if (_wcsicmp(weatherStr.c_str(), choice.first) == 0)
				{
					raceInfo->m_weather = choice.second;
					break;
				}
			}
		}

		orgSetupRace(a1, a2, raceInfo);
	}

	void SetupInfoForGameMode_Customizable(RaceInfo* raceInfo)
	{
		{
			const auto gameModeStr = Registry::GetString(Registry::THQ_SECTION_NAME, L"Race.GameMode").value_or(L"race");
			const std::pair<const wchar_t*, uint32_t> gameModeChoices[] = { 
				{ L"solo", 0 },
				{ L"showoff", 2 },
				{ L"race", 4 },
				{ L"sprint", 5 },
			};
			for (const auto& choice : gameModeChoices)
			{
				if (_wcsicmp(gameModeStr.c_str(), choice.first) == 0)
				{
					raceInfo->m_gameMode = choice.second;
					break;
				}
			}
		}

		uint32_t numCars = Registry::GetDword(Registry::THQ_SECTION_NAME, L"Race.NumCars").value_or(4);
		if (raceInfo->m_gameMode == 0 || raceInfo->m_gameMode == 2)
		{
			numCars = 1;
		}
		else if (raceInfo->m_gameMode == 5)
		{
			numCars = std::clamp(numCars, 2u, 4u);
		}
		else
		{
			numCars = std::clamp(numCars, 2u, 6u);
		}

		std::fill(std::begin(raceInfo->m_opponentPresent), std::end(raceInfo->m_opponentPresent), false);
		if (numCars >= 1)
		{
			raceInfo->m_opponentPresent[0] = true;
		}
		if (numCars >= 2)
		{
			raceInfo->m_opponentPresent[1] = true;
		}
		if (numCars >= 3)
		{
			raceInfo->m_opponentPresent[2] = true;
		}
		if (numCars >= 4)
		{
			raceInfo->m_opponentPresent[4] = true;
		}
		if (numCars >= 5)
		{
			raceInfo->m_opponentPresent[5] = true;
		}
		if (numCars >= 6)
		{
			raceInfo->m_opponentPresent[6] = true;
		}
	}

	static void* orgSetupInfoForGameMode;
	#pragma warning(suppress:4740)
	__declspec(naked) void SetupInfoForGameMode_Hook()
	{
		void* a1;
		RaceInfo* raceInfo;
		_asm
		{
			push	ebp
			mov		ebp, esp
			sub		esp, __LOCAL_SIZE
			mov		[a1], edx
			mov		[raceInfo], edi
		}
		SetupInfoForGameMode_Customizable(raceInfo);
		_asm
		{
			mov		edx, [a1]
			mov		edi, [raceInfo]
			mov		esp, ebp
			pop		ebp
			jmp		[orgSetupInfoForGameMode]
		}
	}
}


namespace ZeroInitializeAllocations
{
	template<std::size_t Index>
	void* (*orgAllocMemory)(size_t size);

	template<std::size_t Index>
	static void* AllocMemory(size_t size)
	{
		void* mem = orgAllocMemory<Index>(size);
		if (mem != nullptr)
		{
			memset(mem, 0, size);
		}
		return mem;
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgAllocMemory<Ctr << 16 | I>, AllocMemory<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
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

		auto set_notifications = get_pattern("FF 51 0C 85 C0 74 0F 8B 4E 28 E8");
		InjectHook(set_notifications, SetNotificationPositions_Hook, HookType::Call);

		Log("Done: AudioCrackleFix");
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
	{
		const auto customDriverName = Registry::GetAnsiString(Registry::ACCLAIM_SECTION_NAME, Registry::DRIVER_NAME_KEY_NAME);
		if (customDriverName) try
		{
			auto driver_name = get_pattern("B8 ? ? ? ? 8D 4C 24 18 E8 ? ? ? ? 8B 7D 68", 1);
			Patch(driver_name, strndup(customDriverName->c_str(), 19));
		}
		TXN_CATCH();
	}


	// THQ Juiced (January 2005): Fix a startup crash with more than 4 cores
	try
	{
		auto get_core_count = get_pattern("03 C8 83 F9 20 7C EE 5F", 2 + 2);
		Patch<uint8_t>(get_core_count, 4);
	}
	TXN_CATCH();


	// THQ Juiced (April/May 2005): Fix "Juiced requires virtual memory to be enabled"
	try
	{
		auto global_memory_status = pattern("3B 4C 24 08 76 07 83 7C 24 ? 00 77 20").get_one();

		Nop(global_memory_status.get<void>(4), 2);
		Patch<uint8_t>(global_memory_status.get<void>(11), 0xEB);
	}
	TXN_CATCH();


	// THQ Juiced (April/May 2005): Zero initialize string? allocations as they break with page heap enabled
	try
	{
		using namespace ZeroInitializeAllocations;

		auto allocs = pattern("8D 14 9D ? ? ? ? 52 E8 ? ? ? ? 83 C4 04 8B E8").count(2);
		std::array<void*, 2> allocations = 
		{
			allocs.get(0).get<void>(8),
			allocs.get(1).get<void>(8),
		};

		HookEach(allocations, InterceptCall);
	}
	TXN_CATCH();


	// THQ Juiced (May 2005): Disable Polish, Russian and Czech as they're not shipped
	// Facepalm...
	try
	{
		auto languages_switch = pattern("B8 05 00 00 00 C3 B8 06 00 00 00 C3 B8 07 00 00 00 C3").get_one();

		Patch<int32_t>(languages_switch.get<void>(1), 0);
		Patch<int32_t>(languages_switch.get<void>(6 + 1), 0);
		Patch<int32_t>(languages_switch.get<void>(12 + 1), 0);
	}
	TXN_CATCH();


	// THQ Juiced: Custom starter car
	if (CareerFilePresent()) try
	{
		auto cms_player_crew_collection = [] {
			try {
				// January 2005
				return get_pattern("68 ? ? ? ? E8 ? ? ? ? 8B 85 84 00 00 00 8B 08 8B 11", 1);
			}
			catch (hook::txn_exception&)
			{
				// April/May
				return get_pattern("68 ? ? ? ? E8 ? ? ? ? 8B 9D 84 00 00 00 8B 33 33 C0", 1);
			}
		}();
		Patch<const char*>(cms_player_crew_collection, "CMSPlayersCrewCollection2.txt");
	}
	TXN_CATCH();


	// THQ Juiced: Customizable second race
	if (HasRegistry) try
	{
		using namespace THQCustomizableRace;

		auto setup_race = get_pattern("E8 ? ? ? ? 8B 8C 24 ? ? ? ? E8 ? ? ? ? 5F 5E 5B 8B E5 5D C2 0C 00");
		auto setup_info_for_gamemode = get_pattern("C7 44 24 ? ? ? ? ? E8 ? ? ? ? B8 01 00 00 00", 8);

		InterceptCall(setup_race, orgSetupRace, SetupRace_Customizable);
		InterceptCall(setup_info_for_gamemode, orgSetupInfoForGameMode, SetupInfoForGameMode_Hook);
	}
	TXN_CATCH();


	// THQ Juiced: Endless demo
	if (Registry::GetDword(Registry::THQ_SECTION_NAME, Registry::ENDLESS_DEMO_KEY_NAME).value_or(0) != 0) try
	{
		auto endless_demo = get_pattern("80 7C D0 32 02 75 ? 8B 5E 70 89 3B", 5);
		Nop(endless_demo, 2);
	}
	TXN_CATCH();
	
	
	// THQ Juiced: Custom driver names
	{
		const auto customDriverName = Registry::GetAnsiString(Registry::THQ_SECTION_NAME, Registry::DRIVER_NAME_KEY_NAME);
		if (customDriverName) try
		{
			auto driver_name_switch = get_pattern("FF 52 08 83 C0 FF 83 F8 ? 77 ? FF 24 85 ? ? ? ? B8", 3 + 2);
			auto driver_name = get_pattern("B8 ? ? ? ? 8D 4C 24 18 E8 ? ? ? ? 8B ? 64", 1);

			Patch<int8_t>(driver_name_switch, 127);
			Patch(driver_name, strndup(customDriverName->c_str(), 19));
		}
		TXN_CATCH();
	}
}

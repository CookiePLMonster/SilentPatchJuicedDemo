#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdio>

#include <wil/resource.h>

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
}

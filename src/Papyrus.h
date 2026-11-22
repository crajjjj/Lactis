#pragma once

namespace Lactis::Papyrus
{
	// LactisNative.psc's API version: bump when a native is added. Never change the
	// arguments of a shipped native; add a new one instead.
	inline constexpr std::int32_t kAPIVersion = 2;

	bool Register(RE::BSScript::IVirtualMachine* a_vm);
}

#ifndef JA2_UTILS_KEY_BINDING_MODEL_H
#define JA2_UTILS_KEY_BINDING_MODEL_H

#include <cstddef>
#include <cstdint>
#include <string_view>

// JA2 configuration files store up to four Win32-compatible virtual-key
// values in one 32-bit integer.  This model owns that persisted spelling and
// packing contract without depending on Windows, SDL, or the legacy SGP types.
namespace ja2::runtime_control
{
using PackedKeyBinding = std::uint32_t;
inline constexpr std::size_t maximumKeyBindingTextLength = 511;

class LegacyKeyStateSource
{
public:
	virtual ~LegacyKeyStateSource() = default;
	virtual bool isPressed(std::uint8_t virtualKey) const noexcept = 0;
};

std::uint8_t legacyVirtualKeyFromName(std::string_view name) noexcept;
PackedKeyBinding parsePackedKeyBinding(std::string_view text) noexcept;
std::size_t unpackPackedKeyBinding(
	PackedKeyBinding binding, std::uint8_t* keys,
	std::size_t capacity) noexcept;
bool isPackedKeyBindingPressed(
	PackedKeyBinding binding,
	const LegacyKeyStateSource& source) noexcept;
}

#endif

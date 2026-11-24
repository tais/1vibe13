#ifndef JA2_COOP_RUNTIME_FAILURE_DIAGNOSTIC_H
#define JA2_COOP_RUNTIME_FAILURE_DIAGNOSTIC_H

#include <array>
#include <cstddef>

// Allocation-free, first-failure diagnostic storage for the two full-engine
// co-op composition roots. Callers may record only bounded classification text
// such as an operation name and enum name. Peer identities, reconnect tokens,
// campaign identifiers, wire payloads, and filesystem paths deliberately have
// no field in this record.
class CoopRuntimeFailureDiagnostic final
{
public:
	void capture(const char* stage, const char* reason,
		const char* detail = "none") noexcept
	{
		if (captured_) return;
		Copy(stage_, stage, "unknown stage");
		Copy(reason_, reason, "unknown reason");
		Copy(detail_, detail, "none");
		captured_ = true;
	}

	bool available() const noexcept { return captured_; }
	const char* stage() const noexcept
	{
		return captured_ ? stage_.data() : "<unavailable>";
	}
	const char* reason() const noexcept
	{
		return captured_ ? reason_.data() : "<unavailable>";
	}
	const char* detail() const noexcept
	{
		return captured_ ? detail_.data() : "<unavailable>";
	}

private:
	template<std::size_t Size>
	static void Copy(std::array<char, Size>& destination,
		const char* source, const char* fallback) noexcept
	{
		static_assert(Size > 1, "diagnostic buffers need a terminator");
		if (source == nullptr || source[0] == '\0') source = fallback;
		std::size_t index = 0;
		for (; index + 1 < Size && source[index] != '\0'; ++index)
		{
			const unsigned char value =
				static_cast<unsigned char>(source[index]);
			// Keep diagnostics parseable and terminal-safe even if a future enum
			// adapter accidentally supplies arbitrary control text.
			destination[index] = value < 0x20u || value > 0x7eu
				? ' ' : static_cast<char>(value);
		}
		destination[index] = '\0';
	}

	std::array<char, 64> stage_{};
	std::array<char, 96> reason_{};
	std::array<char, 96> detail_{};
	bool captured_ = false;
};

#endif

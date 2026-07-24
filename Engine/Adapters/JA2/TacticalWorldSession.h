#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_SESSION_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_SESSION_H

#include <cstdint>

// Engine-owned identity and location of the currently selected tactical world.
// The legacy application keeps exact read-only coordinate projections while
// production writers pass through this object. It deliberately does not own
// map or campaign data yet; committing a load only publishes a world after
// legacy loading has completed successfully.
class TacticalWorldSession
{
public:
	struct Sector
	{
		std::int16_t x = 0;
		std::int16_t y = 0;
		std::int8_t z = -1;

		friend bool operator==(const Sector& lhs, const Sector& rhs) noexcept
		{
			return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
		}
	};

	struct Snapshot
	{
		struct Turn
		{
			bool turnBased = false;
			bool inCombat = false;
			std::uint8_t currentTeam = 0;

			friend bool operator==(const Turn& lhs, const Turn& rhs) noexcept
			{
				return lhs.turnBased == rhs.turnBased &&
					lhs.inCombat == rhs.inCombat &&
					lhs.currentTeam == rhs.currentTeam;
			}

			friend bool operator!=(const Turn& lhs, const Turn& rhs) noexcept
			{
				return !(lhs == rhs);
			}
		};

		Sector sector;
		bool loaded = false;
		std::uint64_t worldGeneration = 0;
		std::uint64_t turnSerial = 0;
		Turn turn;
	};

	const Snapshot& snapshot() const noexcept { return state_; }

	void setSector(Sector sector) noexcept { state_.sector = sector; }
	void setDepth(std::int8_t depth) noexcept { state_.sector.z = depth; }
	void clearSector() noexcept { state_.sector = Sector{}; }
	void setTurnState(Snapshot::Turn turn) noexcept { state_.turn = turn; }
	void setTurnBased(bool turnBased) noexcept
	{
		state_.turn.turnBased = turnBased;
	}
	void setCombatActive(bool inCombat) noexcept
	{
		state_.turn.inCombat = inCombat;
	}
	void setCurrentTeam(std::uint8_t currentTeam) noexcept
	{
		state_.turn.currentTeam = currentTeam;
	}

	// Preserve the legacy generation sequence: zero is reserved, and wrapping
	// the unsigned counter starts again at one. A committed world starts with
	// turn serial one.
	std::uint64_t commitLoad() noexcept;
	void unload() noexcept;
	void beginTeamTurn() noexcept;

	// Compatibility/import path for existing callers and deterministic tests.
	// New production load paths should call commitLoad instead.
	void restore(Snapshot state) noexcept;

private:
	Snapshot state_;
};

#endif

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
			// Number of asynchronous combat effects that must finish before the
			// active action can be finalized. This replaces JA2's writable
			// attack-busy scalar. Save compatibility clamps large counts only
			// while emitting the established byte; runtime retains the workload.
			std::uint32_t pendingCombatActions = 0;

			friend bool operator==(const Turn& lhs, const Turn& rhs) noexcept
			{
				return lhs.turnBased == rhs.turnBased &&
					lhs.inCombat == rhs.inCombat &&
					lhs.currentTeam == rhs.currentTeam &&
					lhs.pendingCombatActions == rhs.pendingCombatActions;
			}

			friend bool operator!=(const Turn& lhs, const Turn& rhs) noexcept
			{
				return !(lhs == rhs);
			}
		};

		// Tactical-world-local narrative timing. The application still selects
		// the quote and supplies randomized delays, while the runtime owns the
		// state and wrap-safe deadline decision. These values retain their
		// established save positions through the JA2 persistence adapter.
		struct CreatureQuote
		{
			bool saidFlavourQuote = false;
			bool hasSeenCreature = false;
			bool saidSmellQuote = false;
			std::uint16_t tenseDelaySeconds = 0;
			std::uint32_t lastTenseQuoteMilliseconds = 0;

			friend bool operator==(
				const CreatureQuote& lhs,
				const CreatureQuote& rhs) noexcept
			{
				return lhs.saidFlavourQuote == rhs.saidFlavourQuote &&
					lhs.hasSeenCreature == rhs.hasSeenCreature &&
					lhs.saidSmellQuote == rhs.saidSmellQuote &&
					lhs.tenseDelaySeconds == rhs.tenseDelaySeconds &&
					lhs.lastTenseQuoteMilliseconds ==
						rhs.lastTenseQuoteMilliseconds;
			}
		};

		Sector sector;
		bool loaded = false;
		std::uint64_t worldGeneration = 0;
		std::uint64_t turnSerial = 0;
		Turn turn;
		CreatureQuote creatureQuote;
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
	void setPendingCombatActions(std::uint32_t pending) noexcept
	{
		state_.turn.pendingCombatActions = pending;
	}

	// Invalid duplicate completion and capacity exhaustion fail closed instead
	// of wrapping and prematurely ending an action.
	bool beginCombatAction() noexcept;
	bool completeCombatAction() noexcept;
	void resetCombatActions() noexcept
	{
		state_.turn.pendingCombatActions = 0;
	}

	void resetCreatureQuoteState() noexcept
	{
		state_.creatureQuote = Snapshot::CreatureQuote{};
	}
	void resetCreatureEncounterFlags() noexcept
	{
		state_.creatureQuote.saidFlavourQuote = false;
		state_.creatureQuote.hasSeenCreature = false;
		state_.creatureQuote.saidSmellQuote = false;
	}
	void setCreatureTenseQuoteDelay(std::uint16_t delaySeconds) noexcept
	{
		state_.creatureQuote.tenseDelaySeconds = delaySeconds;
	}
	bool creatureTenseQuoteDue(std::uint32_t nowMilliseconds) const noexcept
	{
		return nowMilliseconds -
			state_.creatureQuote.lastTenseQuoteMilliseconds >
			static_cast<std::uint32_t>(
				state_.creatureQuote.tenseDelaySeconds) * 1000U;
	}
	void recordCreatureTenseQuoteTime(
		std::uint32_t nowMilliseconds) noexcept
	{
		state_.creatureQuote.lastTenseQuoteMilliseconds = nowMilliseconds;
	}
	void restoreCreatureQuoteState(
		Snapshot::CreatureQuote state) noexcept
	{
		state_.creatureQuote = state;
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

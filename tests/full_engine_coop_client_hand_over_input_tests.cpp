#include <Ja2/FullEngineCoopClientHandOverInput.h>
#include <cstdio>

using namespace CoopSession;
namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
using Key = FullEngineCoopClientHandOverInput::Key;
struct Fixture
{
	FullEngineCoopClientHandOverInput input;
	CoopCampaignStatus status;
	TacticalEntityId selected{3, 4};
	bool controlled = true, enabled = true, pending = false;
	Fixture() { status.sessionEpoch = 7; status.revision = 1; status.phase = CoopCampaignPhase::Tactical; }
	auto key(Key key, bool down = true, bool up = false)
	{ return input.handle(key, down, up, &status, selected, controlled, enabled, pending); }
	auto press(Key key) { (void)this->key(key, false, true); return this->key(key); }
};
}

int main()
{
	{
		Fixture f;
		CHECK(!f.press(Key::Confirm), "Enter alone gives nothing away");
		CHECK(!f.press(Key::Arm) && f.input.armed() && f.input.armedActor() == (TacticalEntityId{3, 4}),
			"J arms the selected merc");
		const auto request = f.press(Key::Confirm);
		CHECK(request && request->action == CoopCampaignAction::HandOverMerc &&
			request->decision == (3u | (std::uint64_t(4) << 16)) && !request->destinationX && !request->group.valid() &&
			!f.input.armed(), "Enter sends the exact merc and disarms");
		CHECK(!f.key(Key::Confirm) && !f.press(Key::Confirm), "a held or fresh Enter cannot repeat it");
	}
	{
		Fixture f; f.controlled = false;
		CHECK(!f.press(Key::Arm) && !f.input.armed() && !f.input.enabled(), "a merc this player does not control cannot be armed");
	}
	{
		Fixture f; (void)f.press(Key::Arm);
		f.selected = {5, 6};
		CHECK(!f.press(Key::Confirm) && !f.input.armed(), "selecting another merc disarms");
	}
	{
		Fixture f; (void)f.press(Key::Arm); (void)f.press(Key::Cancel);
		CHECK(!f.input.armed() && !f.press(Key::Confirm), "Esc disarms");
	}
	{
		Fixture f; (void)f.press(Key::Arm); f.pending = true;
		CHECK(!f.press(Key::Confirm) && !f.input.armed(), "another pending campaign request disarms");
	}
	{
		Fixture f; f.status.battleNotice.id = 9; f.status.battleNotice.kind = CoopCampaignBattleNoticeKind::Defeated;
		CHECK(!f.press(Key::Arm) && !f.input.armed(), "an end-of-battle notice blocks it");
		Fixture g; g.status.phase = CoopCampaignPhase::Strategic;
		CHECK(!g.press(Key::Arm) && !g.input.armed(), "only in a loaded sector");
	}
	{
		Fixture f; (void)f.press(Key::Arm); f.status.sessionEpoch = 8;
		CHECK(!f.press(Key::Confirm) && !f.input.armed(), "a new session disarms");
	}
	if (failures) return 1;
	std::printf("co-op client hand-over input tests passed\n");
	return 0;
}

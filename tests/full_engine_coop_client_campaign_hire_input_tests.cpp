#include <Ja2/FullEngineCoopClientCampaignHireInput.h>
#include <cstdio>

using namespace CoopSession;
namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
using Key = FullEngineCoopClientCampaignHireKey;
struct Fixture
{
	FullEngineCoopClientCampaignHireInput input;
	CoopCampaignStatus status; CoopCampaignEconomy economy; CoopCampaignAimQuotes quotes;
	bool enabled = true, pending = false;
	Fixture()
	{
		status.sessionEpoch = 1; status.revision = 1; status.phase = CoopCampaignPhase::Strategic;
		economy.sessionEpoch = 1; economy.revision = 2; economy.available = true; economy.balance = 5000; economy.mercenaryLimit = 32;
		quotes.sessionEpoch = 1; quotes.revision = 3; quotes.economyRevision = 2; quotes.available = true;
		quotes.arrivalMinutes = 1900; quotes.landingX = 9; quotes.landingY = 1; quotes.quoteCount = 3;
		for (unsigned i = 0; i < 3; ++i)
		{
			auto& q = quotes.quotes[i]; q.profile = i * 7; q.status = CoopCampaignAimQuoteStatus::Available;
			q.salary = {100,600,1100}; q.medicalDeposit = 300; q.total = {400,0,900,0,1400,0};
		}
	}
	auto key(Key key, bool down = true, bool up = false)
	{ return input.handle(key,down,up,&status,&economy,&quotes,enabled,pending); }
	auto press(Key key) { (void)this->key(key,false,true); return this->key(key); }
	void open() { CHECK(!press(Key::Toggle) && input.open(), "H opens local AIM panel without request"); }
};
void SelectionAndConfirmation()
{
	Fixture f;
	CHECK(!f.press(Key::Confirm) && !f.input.open() && !f.input.armed(), "Enter outside AIM cannot hire");
	f.open(); CHECK(f.input.profile() == 0 && f.input.days() == 7, "first copied AIM offer and seven-day default");
	CHECK(!f.press(Key::Previous) && f.input.profile() == 14 && !f.press(Key::Next) && f.input.profile() == 0,
		"offer selection wraps within bounded server rows");
	CHECK(!f.press(Key::FourteenDays) && f.input.days() == 14 && !f.press(Key::OneDay) && f.input.days() == 1,
		"contract keys select exact native supported lengths without requests");
	CHECK(!f.press(Key::Confirm) && f.input.armed() && !f.key(Key::Confirm) && f.input.armed(), "first Enter arms and held repeats cannot confirm");
	CHECK(!f.key(Key::Confirm,false,true) && f.input.armed(), "release only unlocks confirmation key");
	const auto hire = f.key(Key::Confirm);
	CHECK(hire && hire->profile == 0 && hire->days == 1 && !hire->buyGear && !f.input.armed() && f.input.open(), "second released Enter returns selected terms only");
	CHECK(!f.key(Key::Confirm) && !f.input.armed(), "held confirmation cannot start another hire");
	CHECK(!f.press(Key::Confirm) && f.input.armed() && !f.press(Key::Cancel) && !f.input.armed() && f.input.open(), "Esc cancels confirmation before closing panel");
	CHECK(!f.press(Key::Cancel) && !f.input.open(), "second Esc closes panel");
	f.open(); CHECK(!f.press(Key::Toggle) && !f.input.open(), "H closes panel");
}
void EquipmentSelection()
{
	Fixture f;
	auto& offer = f.quotes.quotes[0];
	offer.gearAvailable = true; offer.gearCost = 250; offer.total = {400,650,900,1150,1400,1650};
	f.open(); CHECK(!f.input.buyGear(), "equipment starts unselected");
	CHECK(!f.press(Key::Confirm) && f.input.armed(), "original no-gear terms can be reviewed");
	CHECK(!f.press(Key::Equipment) && f.input.buyGear() && !f.input.armed(), "changing equipment cancels the old confirmation");
	CHECK(!f.key(Key::Equipment) && f.input.buyGear(), "held gear key cannot oscillate the selection");
	f.economy.balance = 1149;
	CHECK(!f.input.canHire(f.status,f.economy,f.quotes) && !f.press(Key::Confirm) && !f.input.armed(),
		"equipment price participates in the shared-balance guard");
	f.economy.balance = 1150;
	CHECK(!f.press(Key::Confirm) && f.input.armed(), "full equipment total requires fresh review");
	const auto hire = f.press(Key::Confirm);
	CHECK(hire && hire->profile == 0 && hire->days == 7 && hire->buyGear,
		"released confirmation returns the explicitly reviewed equipment choice");
	CHECK(!f.press(Key::Next) && !f.input.buyGear(), "another merc does not inherit a purchase choice");
	CHECK(!f.press(Key::Equipment) && !f.input.buyGear(), "unavailable gear cannot be selected");
	(void)f.press(Key::Previous); (void)f.press(Key::Equipment); (void)f.press(Key::Confirm);
	CHECK(f.input.armed() && f.input.buyGear(), "equipment terms armed before offer withdrawal");
	offer.gearAvailable = false; offer.gearCost = 0; offer.total = {400,0,900,0,1400,0}; ++f.quotes.revision;
	f.input.synchronize(&f.status,&f.economy,&f.quotes,true,false);
	CHECK(!f.input.armed() && !f.input.buyGear() && !f.key(Key::Confirm), "withdrawn equipment invalidates selection and held confirmation");
	CHECK(!f.press(Key::Confirm) && f.input.armed(), "remaining no-gear offer must be reviewed separately");
}

void Invalidation()
{
	for (unsigned fault = 0; fault < 10; ++fault)
	{
		Fixture f; f.open(); CHECK(!f.press(Key::Confirm) && f.input.armed(), "exact terms armed before context change");
		if (fault == 0) ++f.status.timeControlRevision;
		if (fault == 1) { ++f.economy.revision; ++f.quotes.economyRevision; ++f.quotes.revision; }
		if (fault == 2) ++f.quotes.revision;
		if (fault == 3) f.pending = true;
		if (fault == 4) f.enabled = false;
		if (fault == 5) { ++f.status.sessionEpoch; ++f.economy.sessionEpoch; ++f.quotes.sessionEpoch; }
		if (fault == 6) f.status.phase = CoopCampaignPhase::Tactical;
		if (fault == 7) { f.quotes.available = false; f.quotes.quoteCount = 0; f.quotes.arrivalMinutes = 0; f.quotes.landingX = f.quotes.landingY = 0; ++f.quotes.revision; }
		if (fault == 8) ++f.economy.revision;
		if (fault == 9) f.economy.available = false;
		f.input.synchronize(&f.status,&f.economy,&f.quotes,f.enabled,f.pending);
		CHECK(!f.input.armed() && !f.key(Key::Confirm), "session/revision/pending/context changes invalidate old confirmation and held Enter");
		if (fault < 3) CHECK(!f.press(Key::Confirm) && f.input.armed(), "fresh terms require re-arming instead of auto-confirming");
	}
	Fixture f; f.open(); (void)f.press(Key::Confirm);
	CHECK(!f.press(Key::Next) && !f.input.armed() && f.input.profile() == 7, "changing selected mercenary cancels confirmation");
	(void)f.press(Key::Confirm);
	CHECK(!f.press(Key::SevenDays) && !f.input.armed(), "contract choice clears confirmation even if same length is selected");
	f.pending = true; CHECK(!f.press(Key::Toggle) && !f.input.open(), "panel may close while server mutation remains pending");
	CHECK(!f.press(Key::Toggle) && !f.input.open(), "pending campaign mutation cannot open new hiring controls");
}
void ServerEligibility()
{
	for (unsigned fault = 0; fault < 6; ++fault)
	{
		Fixture f;
		if (fault == 0) f.economy.balance = 899;
		if (fault == 1) { f.quotes.quotes[0] = {}; f.quotes.quotes[0].status = CoopCampaignAimQuoteStatus::AlreadyHired; }
		if (fault == 2) { f.quotes.quotes[0] = {}; f.quotes.quotes[0].status = CoopCampaignAimQuoteStatus::Unwilling; f.quotes.quotes[0].willingnessReason = 4; }
		if (fault == 3) { f.quotes.quotes[0] = {}; f.quotes.quotes[0].status = CoopCampaignAimQuoteStatus::Unsupported; }
		if (fault == 4) { f.economy.mercenaryCount = f.economy.rosterCount = f.economy.mercenaryLimit = 1;
			f.economy.roster[0].actor = {2,1}; f.economy.roster[0].x = 9; f.economy.roster[0].y = 1; f.economy.roster[0].profile = 23; }
		if (fault == 5) { f.economy.mercenaryCount = f.economy.rosterCount = 1; f.economy.roster[0].actor = {2,1};
			f.economy.roster[0].x = 9; f.economy.roster[0].y = 1; f.economy.roster[0].pendingHire = true; f.economy.roster[0].arrivalMinutes = 1900; }
		f.open(); CHECK(!f.input.canHire(f.status,f.economy,f.quotes) && !f.press(Key::Confirm) && !f.input.armed(),
			"copied funds, server offer status, capacity and pending roster prohibit confirmation");
	}
	Fixture f; f.open(); f.input.synchronize(&f.status,&f.economy,nullptr,true,false);
	CHECK(!f.input.enabled() && !f.input.armed() && f.input.open(), "transient missing quote replacement disables panel without exposing old terms");
	f.input.reset(); CHECK(!f.input.open() && f.input.profile() == FullEngineCoopClientCampaignHireInput::NoProfile, "new screen session resets local choices and latches");
}
}
int main() { SelectionAndConfirmation(); EquipmentSelection(); Invalidation(); ServerEligibility(); return failures ? 1 : 0; }

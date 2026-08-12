#include "CampaignGunCommentPolicy.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
void Check(bool condition, const char* message)
{
	if (condition) return;
	std::cerr << "FAIL: " << message << '\n';
	std::exit(1);
}

template <typename ItemIndexProbe, typename CommentEffect>
bool RouteGroundPickupComment(
	const CampaignGunCommentPolicy& policy,
	ItemIndexProbe&& itemIndexProbe,
	CommentEffect&& commentEffect)
{
	if (!policy.usesUnfinishedBusinessGunComments()) return false;

	const std::int32_t itemIndex = itemIndexProbe();
	if (itemIndex == 0) return false;

	commentEffect(itemIndex, true);
	return true;
}

template <typename ItemIdProbe, typename CommentEffect>
bool RouteInventoryPlacementComment(
	const CampaignGunCommentPolicy& policy,
	ItemIdProbe&& itemIdProbe,
	CommentEffect&& commentEffect)
{
	if (!policy.usesUnfinishedBusinessGunComments()) return false;

	commentEffect(itemIdProbe(), false);
	return true;
}
}

int main()
{
	GameCapabilities arulcoCapabilities;
	const CampaignGunCommentPolicy arulco(arulcoCapabilities);
	GameCapabilities arulcoEditorCapabilities = arulcoCapabilities;
	arulcoEditorCapabilities.editor = true;
	const CampaignGunCommentPolicy arulcoEditor(arulcoEditorCapabilities);

	GameCapabilities unfinishedBusinessCapabilities;
	unfinishedBusinessCapabilities.campaign =
		GameCampaign::UnfinishedBusiness;
	const CampaignGunCommentPolicy unfinishedBusiness(
		unfinishedBusinessCapabilities);

	GameCapabilities unfinishedBusinessEditorCapabilities =
		unfinishedBusinessCapabilities;
	unfinishedBusinessEditorCapabilities.editor = true;
	const CampaignGunCommentPolicy unfinishedBusinessEditor(
		unfinishedBusinessEditorCapabilities);

	Check(!arulco.usesUnfinishedBusinessGunComments() &&
		!arulcoEditor.usesUnfinishedBusinessGunComments() &&
		unfinishedBusiness.usesUnfinishedBusinessGunComments() &&
		unfinishedBusinessEditor.usesUnfinishedBusinessGunComments(),
		"immutable campaign capability, not editor host identity, selects gun comments");

	int groundItemReads = 0;
	int groundEffects = 0;
	std::int32_t forwardedGroundIndex = 0;
	bool forwardedFromGround = false;
	auto groundProbe = [&](std::int32_t value) {
		return [&, value]() {
			++groundItemReads;
			return value;
		};
	};
	auto groundEffect = [&](std::int32_t itemIndex, bool fromGround) {
		++groundEffects;
		forwardedGroundIndex = itemIndex;
		forwardedFromGround = fromGround;
	};

	Check(!RouteGroundPickupComment(arulco, groundProbe(4498), groundEffect) &&
		groundItemReads == 0 && groundEffects == 0,
		"Arulco short-circuits the ground item-index read and gun-comment effect");
	Check(!RouteGroundPickupComment(
			unfinishedBusiness, groundProbe(0), groundEffect) &&
		groundItemReads == 1 && groundEffects == 0,
		"UB preserves the ground path's raw zero-index rejection");

	constexpr std::array<std::int32_t, 7> GroundIndices = {
		std::numeric_limits<std::int32_t>::min(),
		-1, 1, 4498, 31000, 32000,
		std::numeric_limits<std::int32_t>::max()};
	for (const std::int32_t itemIndex : GroundIndices)
	{
		const int readsBefore = groundItemReads;
		const int effectsBefore = groundEffects;
		Check(RouteGroundPickupComment(
				unfinishedBusiness, groundProbe(itemIndex), groundEffect) &&
			groundItemReads == readsBefore + 1 &&
			groundEffects == effectsBefore + 1 &&
			forwardedGroundIndex == itemIndex && forwardedFromGround,
			"UB forwards every nonzero raw ground index exactly once with TRUE");
	}

	int inventoryItemReads = 0;
	int inventoryEffects = 0;
	std::uint16_t forwardedInventoryItem = 0;
	bool inventoryFromGround = true;
	auto inventoryProbe = [&](std::uint16_t value) {
		return [&, value]() {
			++inventoryItemReads;
			return value;
		};
	};
	auto inventoryEffect = [&](std::uint16_t itemId, bool fromGround) {
		++inventoryEffects;
		forwardedInventoryItem = itemId;
		inventoryFromGround = fromGround;
	};

	Check(!RouteInventoryPlacementComment(
			arulco, inventoryProbe(4498), inventoryEffect) &&
		inventoryItemReads == 0 && inventoryEffects == 0,
		"Arulco short-circuits the inventory item read and gun-comment effect");
	constexpr std::array<std::uint16_t, 4> InventoryItems = {
		0, 1, 4498, UINT16_MAX};
	for (const std::uint16_t itemId : InventoryItems)
	{
		const int readsBefore = inventoryItemReads;
		const int effectsBefore = inventoryEffects;
		Check(RouteInventoryPlacementComment(
				unfinishedBusinessEditor,
				inventoryProbe(itemId),
				inventoryEffect) &&
			inventoryItemReads == readsBefore + 1 &&
			inventoryEffects == effectsBefore + 1 &&
			forwardedInventoryItem == itemId && !inventoryFromGround,
			"UB inventory placement forwards every raw item ID once with FALSE");
	}

	std::cout << "campaign gun-comment policy tests passed\n";
	return 0;
}

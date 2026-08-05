#include "BobbyRayLayout.h"

#include <cstdlib>
#include <iostream>

namespace
{
using namespace BobbyRayLayoutModel;

void Require(bool condition, const char* message)
{
	if (condition) return;
	std::cerr << "FAILED: " << message << '\n';
	std::exit(EXIT_FAILURE);
}

bool Is(const Point& point, int x, int y)
{
	return point.x == x && point.y == y;
}

bool Is(const Rect& rect, int x, int y, int width, int height)
{
	return rect.x == x && rect.y == y &&
		rect.width == width && rect.height == height;
}

bool Is(const TextArea& area, int x, int y, int width)
{
	return Is(area.origin, x, y) && area.width == width;
}
}

int main()
{
	using namespace BobbyRayLayoutModel;
	constexpr Anchors anchors{111, 46, 613, 446, 0, 0, 19};

	constexpr HomeLayout home = MakeHomeLayout(anchors);
	Require(Is(home.pageBounds, 111, 46, 502, 400) &&
		home.wood.capacity() == 16 &&
		Is(home.wood.tile(0), 111, 46, 125, 100) &&
		Is(home.wood.tile(15), 486, 346, 125, 100),
		"Bobby Ray home and fulfilment pages share one tiled wood canvas");
	Require(Is(home.name, 188, 46) && Is(home.plaques, 150, 220) &&
		Is(home.topHinge, 111, 88) && Is(home.bottomHinge, 111, 384) &&
		Is(home.storePlaque, 259, 112) && Is(home.handle, 568, 193),
		"Bobby Ray homepage artwork derives from the page anchors");
	Require(Is(home.signs[0].bounds, 243, 252, 92, 50) &&
		Is(home.signs[2].bounds, 153, 322, 116, 75) &&
		Is(home.signs[4].bounds, 440, 328, 114, 70),
		"Bobby Ray sign drawing and hitboxes use one authored sign table");
	for (const HomeSign& sign : home.signs)
		Require(LaptopLayoutModel::Contains(home.pageBounds, sign.bounds),
			"every Bobby Ray sign remains inside the web canvas");
	Require(Is(home.underConstruction.at(0), 155, 175, 414, 64) &&
		Is(home.underConstruction.at(1), 155, 378, 414, 64) &&
		Is(home.underConstructionText, 111, 297, 502),
		"under-construction drawing and invalidation share two rectangles");

	constexpr CatalogueLayout catalogue = MakeCatalogueLayout(anchors);
	Require(catalogue.background.capacity() == 16 &&
		Is(catalogue.catalogueGrid, 115, 51) &&
		Is(catalogue.itemImage(0), 118, 54, 118, 69) &&
		Is(catalogue.itemImage(3), 118, 270, 118, 69),
		"catalogue artwork and item hitboxes share the four-row layout");
	Require(Is(catalogue.itemDescription(0), 292, 57, 224) &&
		Is(catalogue.itemName(3), 121, 327) &&
		Is(catalogue.itemCost(2), 240, 201, 42) &&
		Is(catalogue.itemWeightValue(1), 542, 129, 60) &&
		Is(catalogue.itemQuantity(0), 120, 85, 95) &&
		Is(catalogue.itemPurchasedCount(3), 205, 301) &&
		Is(catalogue.itemQuality(2), 119, 200, 15),
		"catalogue item text columns follow the same row stride as artwork");
	Require(Is(catalogue.previousButton, 116, 346) &&
		Is(catalogue.nextButton, 523, 346) &&
		Is(catalogue.menuButtons.at(0), 208, 346) &&
		Is(catalogue.menuButtons.at(4), 460, 346) &&
		Is(catalogue.filterButton(0), 116, 368) &&
		Is(catalogue.filterButton(8), 116, 390) &&
		Is(catalogue.filterButton(8, 20), 116, 388),
		"catalogue navigation and all filter variants use tested sequences");
	for (std::size_t slot = 0; slot < CatalogueLayout::ItemCount; ++slot)
		Require(LaptopLayoutModel::Contains(
			catalogue.pageBounds, catalogue.itemImage(slot)),
			"every catalogue item hitbox stays inside the page bounds");

	constexpr MailOrderLayout order = MakeMailOrderLayout(anchors);
	Require(Is(order.orderGrid.origin, 113, 108) &&
		Is(order.title, 282, 49, 160, 35) &&
		Is(order.orderFormTitle, 282, 86, 159) &&
		Is(order.locationGraphic, 387, 108) &&
		Is(order.deliverySpeedGraphic, 387, 195),
		"mail-order artwork and title retain the authored page geometry");
	Require(Is(order.clearOrderButton, 420, 318) &&
		Is(order.acceptOrderButton, 410, 353) &&
		Is(order.backButton, 130, 423) && Is(order.homeButton, 515, 423) &&
		Is(order.shipmentsButton, 344, 423),
		"mail-order controls derive from one centered-screen layout");
	Require(Is(order.shippingSpeedLights.at(0), 585, 237, 9, 9) &&
		Is(order.shippingSpeedLights.at(2), 585, 277, 9, 9) &&
		Is(order.shippingMethodLabels.at(0), 395, 237) &&
		Is(order.shippingMethodLabels.at(2), 395, 277),
		"shipping labels, hitboxes, drawing, and invalidation share one stride");
	Require(Is(order.selectedDestination, 396, 147, 197, 18) &&
		Is(order.destinationList, 393, 169, 182, 139) &&
		Is(order.destinationScroll, 575, 169, 22, 139) &&
		Is(order.destinationScrollUp, 575, 174, 18, 20) &&
		Is(order.destinationScrollDown, 575, 284, 18, 20),
		"destination drawing and mouse regions use one drop-down layout");
	Require(Is(order.destinationRow(0, 12), 397, 173, 174, 15) &&
		Is(order.destinationRow(1, 12), 397, 187, 174, 15),
		"destination rows preserve the localized-font stride and overlap");

	constexpr OrderGridLayout grid =
		MakeOrderGridLayout(order.orderGrid.origin);
	Require(Is(grid.columns[0], 136, 145, 23) &&
		Is(grid.columns[2], 203, 145, 91) &&
		Is(grid.columns[4], 337, 145, 42),
		"order-grid headers and values share five typed text columns");
	Require(Is(grid.scrollColumn, 115, 143, 23, 200) &&
		Is(grid.scrollUp, 117, 147, 18, 20) &&
		Is(grid.scrollDown, 117, 319, 18, 20),
		"order-grid scroll drawing and mouse regions share one geometry");
	Require(Is(grid.subtotal, 116, 348, 212) &&
		Is(grid.shippingAndHandling, 116, 365, 212) &&
		Is(grid.grandTotal, 116, 385, 212),
		"order totals derive from the ten-row purchase grid");

	constexpr ShipmentLayout shipments = MakeShipmentLayout(anchors);
	Require(Is(shipments.title, 282, 86, 159) &&
		Is(shipments.deliveryGrid, 113, 108) &&
		Is(shipments.orderGrid, 334, 108),
		"shipment title and both grids share the fulfilment page anchors");
	Require(Is(shipments.rows.at(0), 116, 144, 183, 12) &&
		Is(shipments.rows.at(12), 116, 384, 183, 12) &&
		Is(shipments.orderedOn(12), 116, 384, 64) &&
		Is(shipments.itemCount(12), 183, 384, 116),
		"shipment drawing and all thirteen row hitboxes share one sequence");
	for (std::size_t slot = 0; slot < ShipmentLayout::VisibleRowCount; ++slot)
		Require(LaptopLayoutModel::Contains(
			shipments.pageBounds, shipments.rows.at(slot)),
			"every shipment row stays inside the page bounds");

	constexpr Anchors shifted{271, 136, 773, 536, 160, 90, 19};
	constexpr HomeLayout shiftedHome = MakeHomeLayout(shifted);
	constexpr CatalogueLayout shiftedCatalogue =
		MakeCatalogueLayout(shifted);
	constexpr MailOrderLayout shiftedOrder = MakeMailOrderLayout(shifted);
	constexpr ShipmentLayout shiftedShipments = MakeShipmentLayout(shifted);
	Require(Is(shiftedHome.signs[0].bounds, 403, 342, 92, 50) &&
		Is(shiftedCatalogue.itemImage(3), 278, 360, 118, 69) &&
		Is(shiftedOrder.orderGrid.origin, 273, 198) &&
		Is(shiftedOrder.shippingSpeedLights.at(2), 745, 367, 9, 9) &&
		Is(shiftedShipments.rows.at(12), 276, 474, 183, 12),
		"Bobby Ray geometry follows centered-screen and web-page offsets");

	return EXIT_SUCCESS;
}

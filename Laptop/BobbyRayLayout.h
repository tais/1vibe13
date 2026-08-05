#ifndef LAPTOP_BOBBY_RAY_LAYOUT_H
#define LAPTOP_BOBBY_RAY_LAYOUT_H

#include "LaptopLayout.h"

#include <array>
#include <cstddef>

namespace BobbyRayLayoutModel
{
	using LaptopLayoutModel::Point;
	using LaptopLayoutModel::Rect;
	using LaptopLayoutModel::TextArea;

	struct Anchors
	{
		int pageX = 0;
		int pageY = 0;
		int pageRight = 0;
		int pageBottom = 0;
		int screenX = 0;
		int screenY = 0;
		int webDeltaY = 0;
	};

	struct PointSequence
	{
		Point first;
		Point step;

		constexpr Point at(std::size_t index) const noexcept
		{
			return {
				first.x + static_cast<int>(index) * step.x,
				first.y + static_cast<int>(index) * step.y};
		}
	};

	struct RectSequence
	{
		Rect first;
		Point step;

		constexpr Rect at(std::size_t index) const noexcept
		{
			return {
				first.x + static_cast<int>(index) * step.x,
				first.y + static_cast<int>(index) * step.y,
				first.width,
				first.height};
		}
	};

	struct TileGrid
	{
		Rect first;
		int columns = 0;
		int rows = 0;

		constexpr std::size_t capacity() const noexcept
		{
			return static_cast<std::size_t>(columns * rows);
		}

		constexpr Rect tile(std::size_t index) const noexcept
		{
			return {
				first.x + static_cast<int>(index % columns) * first.width,
				first.y + static_cast<int>(index / columns) * first.height,
				first.width,
				first.height};
		}
	};

	struct HomeSign
	{
		Rect bounds;
		TextArea label;
	};

	struct HomeLayout
	{
		static constexpr std::size_t SignCount = 5;

		Rect pageBounds;
		TileGrid wood;
		Point name;
		Point plaques;
		Point topHinge;
		Point bottomHinge;
		Point storePlaque;
		Point handle;
		std::array<TextArea, 3> advertisements;
		std::array<HomeSign, SignCount> signs;
		RectSequence underConstruction;
		TextArea underConstructionText;
	};

	constexpr HomeLayout MakeHomeLayout(Anchors anchors) noexcept
	{
		HomeLayout result;
		result.pageBounds = {
			anchors.pageX, anchors.pageY,
			anchors.pageRight - anchors.pageX,
			anchors.pageBottom - anchors.pageY};
		result.wood = {{anchors.pageX, anchors.pageY, 125, 100}, 4, 4};
		result.name = {anchors.pageX + 77, anchors.pageY};
		result.plaques = {anchors.pageX + 39, anchors.pageY + 174};
		result.topHinge = {anchors.pageX, anchors.pageY + 42};
		result.bottomHinge = {anchors.pageX, anchors.pageY + 338};
		result.storePlaque = {anchors.pageX + 148, anchors.pageY + 66};
		result.handle = {anchors.pageX + 457, anchors.pageY + 147};
		result.advertisements = {{
			{{anchors.pageX, anchors.pageY + 156}, 500},
			{{anchors.pageX, anchors.pageY + 169}, 500},
			{{anchors.pageX, anchors.pageY + 378}, 500}}};
		result.signs = {{
			{{anchors.pageX + 132, anchors.pageY + 206, 92, 50},
			 {{anchors.pageX + 132, anchors.pageY + 216}, 87}},
			{{anchors.pageX + 277, anchors.pageY + 201, 103, 57},
			 {{anchors.pageX + 277, anchors.pageY + 224}, 103}},
			{{anchors.pageX + 42, anchors.pageY + 276, 116, 75},
			 {{anchors.pageX + 42, anchors.pageY + 299}, 116}},
			{{anchors.pageX + 189, anchors.pageY + 279, 112, 71},
			 {{anchors.pageX + 189, anchors.pageY + 302}, 112}},
			{{anchors.pageX + 329, anchors.pageY + 282, 114, 70},
			 {{anchors.pageX + 329, anchors.pageY + 305}, 114}}}};
		result.underConstruction = {
			{anchors.screenX + 155, anchors.screenY + 175, 414, 64},
			{0, 203}};
		result.underConstructionText = {
			{anchors.pageX, anchors.screenY + 297}, 502};
		return result;
	}

	struct CatalogueLayout
	{
		static constexpr std::size_t ItemCount = 4;
		static constexpr std::size_t MenuButtonCount = 5;
		static constexpr std::size_t FilterColumns = 8;

		Rect pageBounds;
		TileGrid background;
		Point catalogueGrid;
		Rect title;
		Point toOrderTitle;
		TextArea toOrderText;
		Point previousButton;
		Point nextButton;
		PointSequence menuButtons;
		Point homeButton;
		Point orderFormButton;
		Point orderSubtotal;
		Point orderPage;
		Point usedQualityNote;

		constexpr Rect itemImage(std::size_t slot) const noexcept
		{
			return {
				catalogueGrid.x + 3,
				catalogueGrid.y + 3 + static_cast<int>(slot) * 72,
				118,
				69};
		}

		constexpr TextArea itemDescription(std::size_t slot) const noexcept
		{
			return {{
				catalogueGrid.x + 177,
				catalogueGrid.y + 6 + static_cast<int>(slot) * 72}, 224};
		}

		constexpr Point itemName(std::size_t slot) const noexcept
		{
			return {
				catalogueGrid.x + 6,
				catalogueGrid.y + 60 + static_cast<int>(slot) * 72};
		}

		constexpr TextArea itemCost(std::size_t slot) const noexcept
		{
			return {{
				catalogueGrid.x + 125,
				catalogueGrid.y + 6 + static_cast<int>(slot) * 72}, 42};
		}

		constexpr Point itemWeightLabel(std::size_t slot) const noexcept
		{
			return {
				catalogueGrid.x + 412,
				catalogueGrid.y + 6 + static_cast<int>(slot) * 72};
		}

		constexpr TextArea itemWeightValue(std::size_t slot) const noexcept
		{
			return {{
				catalogueGrid.x + 427,
				catalogueGrid.y + 6 + static_cast<int>(slot) * 72}, 60};
		}

		constexpr TextArea itemQuantity(std::size_t slot) const noexcept
		{
			return {{
				catalogueGrid.x + 5,
				catalogueGrid.y + 34 + static_cast<int>(slot) * 72}, 95};
		}

		constexpr Point itemPurchasedCount(std::size_t slot) const noexcept
		{
			return {
				catalogueGrid.x + 90,
				catalogueGrid.y + 34 + static_cast<int>(slot) * 72};
		}

		constexpr TextArea itemQuality(std::size_t slot) const noexcept
		{
			return {{
				catalogueGrid.x + 4,
				catalogueGrid.y + 5 + static_cast<int>(slot) * 72}, 15};
		}

		constexpr Point filterButton(
			std::size_t slot, int rowStep = 22) const noexcept
		{
			return {
				previousButton.x + static_cast<int>(slot % FilterColumns) * 62,
				previousButton.y + 22 +
					static_cast<int>(slot / FilterColumns) * rowStep};
		}
	};

	constexpr CatalogueLayout MakeCatalogueLayout(Anchors anchors) noexcept
	{
		CatalogueLayout result;
		result.pageBounds = {
			anchors.pageX, anchors.pageY,
			anchors.pageRight - anchors.pageX,
			anchors.pageBottom - anchors.pageY};
		result.background = {{anchors.pageX, anchors.pageY, 125, 100}, 4, 4};
		result.catalogueGrid = {anchors.pageX + 4, anchors.pageY + 5};
		result.title = {anchors.pageX + 4, anchors.pageY + 3, 46, 42};
		result.toOrderTitle = {
			anchors.screenX + 195,
			anchors.screenY + 42 + anchors.webDeltaY};
		result.toOrderText = {{
			anchors.screenX + 270,
			anchors.screenY + 33 + anchors.webDeltaY}, 330};
		result.previousButton = {anchors.pageX + 5, anchors.pageY + 300};
		result.nextButton = {anchors.pageX + 412, anchors.pageY + 300};
		result.menuButtons = {{anchors.pageX + 97, anchors.pageY + 300}, {63, 0}};
		result.homeButton = {
			anchors.screenX + 120,
			anchors.screenY + 400 + anchors.webDeltaY};
		result.orderFormButton = {anchors.pageX + 200, anchors.pageY + 367};
		result.orderSubtotal = {
			anchors.screenX + 470, anchors.pageY + 369};
		result.orderPage = {
			anchors.screenX + 285, anchors.pageY + 369};
		result.usedQualityNote = {
			result.orderSubtotal.x, result.orderSubtotal.y + 15};
		return result;
	}

	struct OrderGridLayout
	{
		static constexpr std::size_t VisibleRowCount = 10;

		Point origin;
		std::array<TextArea, 5> columns;
		Rect scrollColumn;
		Rect scrollUp;
		Rect scrollDown;
		TextArea subtotal;
		TextArea shippingAndHandling;
		TextArea grandTotal;

		constexpr Point row(std::size_t slot) const noexcept
		{
			return {
				origin.x,
				origin.y + 37 + static_cast<int>(slot) * 20};
		}
	};

	constexpr OrderGridLayout MakeOrderGridLayout(Point origin) noexcept
	{
		OrderGridLayout result;
		result.origin = origin;
		result.columns = {{
			{{origin.x + 23, origin.y + 37}, 23},
			{{origin.x + 48, origin.y + 37}, 40},
			{{origin.x + 90, origin.y + 37}, 91},
			{{origin.x + 184, origin.y + 37}, 40},
			{{origin.x + 224, origin.y + 37}, 42}}};
		result.scrollColumn = {origin.x + 2, origin.y + 35, 23, 200};
		result.scrollUp = {origin.x + 4, origin.y + 39, 18, 20};
		result.scrollDown = {origin.x + 4, origin.y + 211, 18, 20};
		result.subtotal = {{origin.x + 3, origin.y + 240}, 212};
		result.shippingAndHandling = {
			{result.subtotal.origin.x, result.subtotal.origin.y + 17}, 212};
		result.grandTotal = {
			{result.subtotal.origin.x,
			 result.shippingAndHandling.origin.y + 20}, 212};
		return result;
	}

	struct MailOrderLayout
	{
		static constexpr std::size_t ShippingSpeedCount = 3;

		Rect pageBounds;
		OrderGridLayout orderGrid;
		Rect title;
		TextArea orderFormTitle;
		Point locationGraphic;
		Point deliverySpeedGraphic;
		Point clearOrderButton;
		Point acceptOrderButton;
		Point backButton;
		Point homeButton;
		Point shipmentsButton;
		Point confirmOrder;
		Point packageWeight;
		TextArea packageWeightText;
		Point shippingLocationLabel;
		Point shippingSpeedLabel;
		Point shippingCostLabel;
		PointSequence shippingMethodLabels;
		RectSequence shippingSpeedLights;
		Rect selectedDestination;
		Rect destinationList;
		Rect destinationScroll;
		Rect destinationScrollUp;
		Rect destinationScrollDown;
		Point destinationText;
		Point totalSaveArea;
		Point usedWarning;

		constexpr Rect destinationRow(
			std::size_t slot, int fontHeight) const noexcept
		{
			return {
				destinationList.x + 4,
					destinationList.y + 4 +
					static_cast<int>(slot) * (fontHeight + 2),
				destinationList.width - 8,
				fontHeight + 3};
		}
	};

	constexpr MailOrderLayout MakeMailOrderLayout(Anchors anchors) noexcept
	{
		MailOrderLayout result;
		result.pageBounds = {
			anchors.pageX, anchors.pageY,
			anchors.pageRight - anchors.pageX,
			anchors.pageBottom - anchors.pageY};
		result.orderGrid = MakeOrderGridLayout({
			anchors.pageX + 2, anchors.pageY + 62});
		result.title = {anchors.pageX + 171, anchors.pageY + 3, 160, 35};
		result.orderFormTitle = {{
			result.title.x, result.title.y + 37}, 159};
		result.locationGraphic = {anchors.pageX + 276, anchors.pageY + 62};
		result.deliverySpeedGraphic = {anchors.pageX + 276, anchors.pageY + 149};
		result.clearOrderButton = {anchors.pageX + 309, anchors.pageY + 272};
		result.acceptOrderButton = {anchors.pageX + 299, anchors.pageY + 307};
		result.backButton = {
			anchors.screenX + 130,
			anchors.screenY + 404 + anchors.webDeltaY};
		result.homeButton = {
			anchors.screenX + 515, result.backButton.y};
		result.shipmentsButton = {
			anchors.screenX +
				((anchors.pageX - anchors.screenX) +
				 ((anchors.pageRight - anchors.screenX) -
				  (anchors.pageX - anchors.screenX - 75))) / 2,
			result.backButton.y};
		result.confirmOrder = {
			anchors.screenX + 220, anchors.screenY + 170};
		result.packageWeight = {anchors.pageX + 276, anchors.pageY + 249};
		result.packageWeightText = {{
			result.packageWeight.x + 8, result.packageWeight.y + 4}, 188};
		result.shippingLocationLabel = {
			result.locationGraphic.x + 8, result.locationGraphic.y + 8};
		result.shippingSpeedLabel = {
			result.shippingLocationLabel.x, result.deliverySpeedGraphic.y + 11};
		result.shippingCostLabel = {
			result.shippingSpeedLabel.x + 130,
			result.shippingSpeedLabel.y};
		result.shippingMethodLabels = {{
			result.shippingSpeedLabel.x, result.deliverySpeedGraphic.y + 42},
			{0, 20}};
		result.shippingSpeedLights = {{
			anchors.screenX + 585,
			anchors.screenY + 218 + anchors.webDeltaY,
			9,
			9}, {0, 20}};
		result.selectedDestination = {
			result.locationGraphic.x + 9,
			result.locationGraphic.y + 39,
			197,
			18};
		result.destinationList = {
			result.locationGraphic.x + 6,
			result.locationGraphic.y + 61,
			182,
			139};
		result.destinationScroll = {
			result.destinationList.right(), result.destinationList.y, 22, 139};
		result.destinationScrollUp = {
			result.destinationScroll.x,
			result.destinationScroll.y + 5,
			18,
			20};
		result.destinationScrollDown = {
			result.destinationScroll.x,
			result.destinationScroll.bottom() - 24,
			18,
			20};
		result.destinationText = {
			result.destinationList.x + 6,
			result.selectedDestination.y + 3};
		result.totalSaveArea = {
			result.orderGrid.origin.x + 221,
			result.orderGrid.origin.y + 237};
		result.usedWarning = {
			anchors.screenX + 122,
			anchors.screenY + 382 + anchors.webDeltaY};
		return result;
	}

	struct ShipmentLayout
	{
		static constexpr std::size_t VisibleRowCount = 13;

		Rect pageBounds;
		TextArea title;
		Point deliveryGrid;
		Point orderGrid;
		Point backButton;
		Point homeButton;
		TextArea orderedOnHeader;
		TextArea itemCountHeader;
		RectSequence rows;
		constexpr TextArea orderedOn(std::size_t slot) const noexcept
		{
			const Rect rowBounds = rows.at(slot);
			return {{rowBounds.x, rowBounds.y}, 64};
		}

		constexpr TextArea itemCount(std::size_t slot) const noexcept
		{
			const Rect rowBounds = rows.at(slot);
			return {{rowBounds.x + 67, rowBounds.y}, 116};
		}
	};

	constexpr ShipmentLayout MakeShipmentLayout(Anchors anchors) noexcept
	{
		ShipmentLayout result;
		result.pageBounds = {
			anchors.pageX, anchors.pageY,
			anchors.pageRight - anchors.pageX,
			anchors.pageBottom - anchors.pageY};
		result.title = {{anchors.pageX + 171, anchors.pageY + 40}, 159};
		result.deliveryGrid = {anchors.pageX + 2, anchors.pageY + 62};
		result.orderGrid = {anchors.pageX + 223, anchors.pageY + 62};
		result.backButton = {
			anchors.screenX + 130,
			anchors.screenY + 404 + anchors.webDeltaY};
		result.homeButton = {
			anchors.screenX + 515, result.backButton.y};
		result.orderedOnHeader = {
			{anchors.screenX + 116, anchors.screenY + 117}, 64};
		result.itemCountHeader = {
			{anchors.screenX + 183, anchors.screenY + 117}, 116};
		result.rows = {{
			anchors.screenX + 116, anchors.screenY + 144, 183, 12},
			{0, 20}};
		return result;
	}
}

#endif

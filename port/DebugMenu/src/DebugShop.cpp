#include "DebugMenu.h"
#include "DebugShop.h"

#include <imgui.h>
#include "Types.h"

#include "InventoryInfo.h"
#include "DebugHelpers.h"
#include "DebugSetting.h"
#include "ActorHero.h"
#include "LevelScheduler.h"

namespace Debug
{
	namespace Shop
	{
		constexpr int gNumBraceletColors = 8;

		Debug::Setting<bool> gAutoBuyBoomy("Auto Buy Boomy", false);
		// Loading a level directly skips the story step that teaches exorcism and leaves the magic gauge empty.
		Debug::Setting<bool> gUnlockExorcism("Unlock Exorcism", false);
		Debug::Setting<bool> gFillMagic("Fill Magic", false);
		// Every non-bracelet shop item and ability, bought in order (Boomy base -> silver -> gold).
		Debug::Setting<bool> gAutoBuyAllGadgets("Auto Buy All Gadgets", false);
		constexpr int gAllGadgets[] = { INVENTORY_ITEM_BOUNCE, INVENTORY_ITEM_CLIMBING_GLOVES, INVENTORY_ITEM_BASE_BOOMY, INVENTORY_ITEM_SILVER_BOOMY,
			INVENTORY_ITEM_GOLD_BOOMY, INVENTORY_ITEM_UNUSED_REGENERATE, INVENTORY_ITEM_JAMGUT_WHISTLE, INVENTORY_ITEM_MAGIC_BOARD, INVENTORY_ITEM_TELESCOPE };

		struct BraceletInfo
		{
			BraceletInfo(const char* color) : color(color), name(color + std::string(" Bracelet")), autoBuySetting(std::string("Auto Buy ") + name, false) {}
			std::string color;
			std::string name;
			Debug::Setting<bool> autoBuySetting;
		};

		std::array<BraceletInfo, gNumBraceletColors> gBracelets = {
			BraceletInfo("White"), BraceletInfo("Yellow"),
			BraceletInfo("Green"), BraceletInfo("Blue"),
			BraceletInfo("Brown"), BraceletInfo("Black"),
			BraceletInfo("Silver"), BraceletInfo("Gold")
		};
	}
}

void Debug::Shop::ShowMenu(bool* bOpen)
{
	// Show a window with a grid of items
	if (ImGui::Begin("Debug Shop", bOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		ImGui::PushStyleColor(ImGuiCol_Button, DebugHelpers::GetValidatedColor(CInventoryInfo::IsObjectPurchased(INVENTORY_ITEM_BASE_BOOMY)));
		if (ImGui::Button("Buy Boomy")) {
			CInventoryInfo info;
			info.purchaseId = INVENTORY_ITEM_BASE_BOOMY;

			info.ObjectPurchased();
		}

		ImGui::PopStyleColor();
		ImGui::SameLine(0.0f, 10.0f);

		gAutoBuyBoomy.DrawImguiControl();
		gUnlockExorcism.DrawImguiControl();
		gFillMagic.DrawImguiControl();
		gAutoBuyAllGadgets.DrawImguiControl();

		for (int i = 0; i < gBracelets.size(); ++i) {
			ImGui::PushStyleColor(ImGuiCol_Button, DebugHelpers::GetValidatedColor(CInventoryInfo::IsObjectPurchased(INVENTORY_ITEM_WHITE_BRACELET + i)));
			if (ImGui::Button(gBracelets[i].name.c_str())) {
				CInventoryInfo info;
				info.purchaseId = INVENTORY_ITEM_WHITE_BRACELET + i;
				info.ObjectPurchased();
			}
			ImGui::PopStyleColor();
			ImGui::SameLine(0.0f, 10.0f);
			gBracelets[i].autoBuySetting.DrawImguiControl();
		}
	}

	ImGui::End();
}

void Debug::Shop::Update()
{
	if (CActorHero::_gThis != nullptr) {
		if (gUnlockExorcism && CLevelScheduler::ScenVar_Get(SCN_ABILITY_MAGIC_EXORCISM) == 0) {
			CLevelScheduler::ScenVar_Set(SCN_ABILITY_MAGIC_EXORCISM, 1);
		}

		if (gFillMagic) {
			CActorHero::_gThis->magicInterface.SetValue(CActorHero::_gThis->magicInterface.GetValueMax());
		}

		if (gAutoBuyBoomy) {
			if (!CInventoryInfo::IsObjectPurchased(INVENTORY_ITEM_BASE_BOOMY)) {
				CInventoryInfo info;
				info.purchaseId = INVENTORY_ITEM_BASE_BOOMY;
				info.ObjectPurchased();
			}
		}

		if (gAutoBuyAllGadgets) {
			for (int item : gAllGadgets) {
				const bool bOwned = (item == INVENTORY_ITEM_UNUSED_REGENERATE) ? CLevelScheduler::ScenVar_Get(SCN_ABILITY_MAGIC_REGENERATE) != 0 : CInventoryInfo::IsObjectPurchased(item) != 0;
				if (!bOwned) {
					CInventoryInfo info;
					info.purchaseId = item;
					info.ObjectPurchased();
				}
			}
		}

		for (int i = 0; i < gBracelets.size(); ++i) {
			if (gBracelets[i].autoBuySetting) {
				if (!CInventoryInfo::IsObjectPurchased(INVENTORY_ITEM_WHITE_BRACELET + i)) {
					CInventoryInfo info;
					info.purchaseId = INVENTORY_ITEM_WHITE_BRACELET + i;
					info.ObjectPurchased();
				}
			}
		}
	}
}

namespace Debug {
    MenuRegisterer sDebugShopMenuReg("Shop", Debug::Shop::ShowMenu, true);
    UpdateRegisterer sDebugShopUpdateReg(Debug::Shop::Update);
}


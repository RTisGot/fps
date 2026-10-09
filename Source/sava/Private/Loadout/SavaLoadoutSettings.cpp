// Fill out your copyright notice in the Description page of Project Settings.

#include "Loadout/SavaLoadoutSettings.h"
#include "AbilitySystem/SavaLoadoutAbilityData.h"
#include "Weapon/SavaWeaponData.h"

namespace SavaLoadout
{
	//リストのアセットを読み込んで返す(読み込めないものは飛ばす)
	template <typename T>
	TArray<T*> LoadAll(const TArray<TSoftObjectPtr<T>>& List)
	{
		TArray<T*> Result;
		for (const TSoftObjectPtr<T>& Entry : List)
		{
			if (T* Asset = Entry.LoadSynchronous())
			{
				Result.Add(Asset);
			}
		}
		return Result;
	}

	//Selected が Selectable にあればそのまま、なければ Selectable の最初のもの(空なら空)
	template <typename T>
	TSoftObjectPtr<T> PickSelectable(const TSoftObjectPtr<T>& Selected, const TArray<T*>& Selectable)
	{
		for (T* Asset : Selectable)
		{
			if (Selected.ToSoftObjectPath() == FSoftObjectPath(Asset))
			{
				return Selected;
			}
		}
		return Selectable.Num() > 0 ? TSoftObjectPtr<T>(Selectable[0]) : TSoftObjectPtr<T>();
	}
}

TArray<USavaWeaponData*> USavaLoadoutSettings::GetSelectableWeapons(ESavaWeaponSlot Slot)
{
	TArray<USavaWeaponData*> Weapons = SavaLoadout::LoadAll(GetDefault<USavaLoadoutSettings>()->Weapons);
	Weapons.RemoveAll([Slot](const USavaWeaponData* Weapon) { return Weapon->Slot != Slot; });
	return Weapons;
}

TArray<USavaSkillData*> USavaLoadoutSettings::GetSelectableSkills()
{
	return SavaLoadout::LoadAll(GetDefault<USavaLoadoutSettings>()->Skills);
}

TArray<USavaGadgetData*> USavaLoadoutSettings::GetSelectableGadgets()
{
	return SavaLoadout::LoadAll(GetDefault<USavaLoadoutSettings>()->Gadgets);
}

FSavaLoadout USavaLoadoutSettings::Sanitize(const FSavaLoadout& Loadout)
{
	FSavaLoadout Result;
	Result.PrimaryWeapon = SavaLoadout::PickSelectable(Loadout.PrimaryWeapon, GetSelectableWeapons(ESavaWeaponSlot::Primary));
	Result.SecondaryWeapon = SavaLoadout::PickSelectable(Loadout.SecondaryWeapon, GetSelectableWeapons(ESavaWeaponSlot::Secondary));
	Result.Skill = SavaLoadout::PickSelectable(Loadout.Skill, GetSelectableSkills());
	Result.Gadget = SavaLoadout::PickSelectable(Loadout.Gadget, GetSelectableGadgets());
	return Result;
}

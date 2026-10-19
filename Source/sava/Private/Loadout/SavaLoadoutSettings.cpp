// Fill out your copyright notice in the Description page of Project Settings.

#include "Loadout/SavaLoadoutSettings.h"
#include "Loadout/SavaCharacterTypes.h"
#include "AbilitySystem/SavaLoadoutAbilityData.h"
#include "Engine/DataTable.h"
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

TArray<FName> USavaLoadoutSettings::GetSelectableCharacters()
{
	const UDataTable* Table = GetDefault<USavaLoadoutSettings>()->CharacterTable.LoadSynchronous();
	if (!Table || Table->GetRowStruct() != FSavaCharacterRow::StaticStruct())
	{
		return {};
	}
	return Table->GetRowNames();
}

const FSavaCharacterRow* USavaLoadoutSettings::FindCharacterRow(FName CharacterId)
{
	const UDataTable* Table = GetDefault<USavaLoadoutSettings>()->CharacterTable.LoadSynchronous();
	if (!Table || CharacterId.IsNone() || Table->GetRowStruct() != FSavaCharacterRow::StaticStruct())
	{
		return nullptr;
	}
	return Table->FindRow<FSavaCharacterRow>(CharacterId, TEXT("SavaLoadoutSettings"), false);
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
	const TArray<FName> Characters = GetSelectableCharacters();
	Result.Character = Characters.Contains(Loadout.Character) ? Loadout.Character : (Characters.Num() > 0 ? Characters[0] : NAME_None);
	Result.PrimaryWeapon = SavaLoadout::PickSelectable(Loadout.PrimaryWeapon, GetSelectableWeapons(ESavaWeaponSlot::Primary));
	Result.SecondaryWeapon = SavaLoadout::PickSelectable(Loadout.SecondaryWeapon, GetSelectableWeapons(ESavaWeaponSlot::Secondary));
	//武器が置き換わったら、前の武器向けのカスタマイズは使わない
	Result.PrimaryTuning = Result.PrimaryWeapon == Loadout.PrimaryWeapon ? Loadout.PrimaryTuning.GetSanitized() : FSavaWeaponTuning();
	Result.SecondaryTuning = Result.SecondaryWeapon == Loadout.SecondaryWeapon ? Loadout.SecondaryTuning.GetSanitized() : FSavaWeaponTuning();
	Result.Skill = SavaLoadout::PickSelectable(Loadout.Skill, GetSelectableSkills());
	Result.Gadget = SavaLoadout::PickSelectable(Loadout.Gadget, GetSelectableGadgets());
	return Result;
}

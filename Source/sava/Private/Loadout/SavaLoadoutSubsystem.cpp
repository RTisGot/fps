// Fill out your copyright notice in the Description page of Project Settings.

#include "Loadout/SavaLoadoutSubsystem.h"
#include "Loadout/SavaLoadoutSaveGame.h"
#include "Loadout/SavaLoadoutSettings.h"
#include "AbilitySystem/SavaLoadoutAbilityData.h"
#include "Weapon/SavaWeaponData.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogSavaLoadout, Log, All);

namespace SavaLoadout
{
	const FString SaveSlotName(TEXT("Loadout"));
}

void USavaLoadoutSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadFromDisk();
}

//--------------------------------変更

void USavaLoadoutSubsystem::SetWeapon(ESavaWeaponSlot Slot, USavaWeaponData* Weapon)
{
	FSavaLoadout NewLoadout = Loadout;
	(Slot == ESavaWeaponSlot::Primary ? NewLoadout.PrimaryWeapon : NewLoadout.SecondaryWeapon) = Weapon;
	ApplyChange(NewLoadout);
}

void USavaLoadoutSubsystem::SetSkill(USavaSkillData* Skill)
{
	FSavaLoadout NewLoadout = Loadout;
	NewLoadout.Skill = Skill;
	ApplyChange(NewLoadout);
}

void USavaLoadoutSubsystem::SetGadget(USavaGadgetData* Gadget)
{
	FSavaLoadout NewLoadout = Loadout;
	NewLoadout.Gadget = Gadget;
	ApplyChange(NewLoadout);
}

void USavaLoadoutSubsystem::SelectItem(UObject* Item)
{
	if (USavaWeaponData* Weapon = Cast<USavaWeaponData>(Item))
	{
		SetWeapon(Weapon->Slot, Weapon);
	}
	else if (USavaSkillData* Skill = Cast<USavaSkillData>(Item))
	{
		SetSkill(Skill);
	}
	else if (USavaGadgetData* Gadget = Cast<USavaGadgetData>(Item))
	{
		SetGadget(Gadget);
	}
}

bool USavaLoadoutSubsystem::IsSelected(const UObject* Item) const
{
	if (!Item)
	{
		return false;
	}

	const FSoftObjectPath ItemPath(Item);
	return Loadout.PrimaryWeapon.ToSoftObjectPath() == ItemPath
		|| Loadout.SecondaryWeapon.ToSoftObjectPath() == ItemPath
		|| Loadout.Skill.ToSoftObjectPath() == ItemPath
		|| Loadout.Gadget.ToSoftObjectPath() == ItemPath;
}

void USavaLoadoutSubsystem::GetItemDisplayInfo(const UObject* Item, FText& DisplayName, UTexture2D*& Icon)
{
	DisplayName = FText::GetEmpty();
	Icon = nullptr;

	if (const USavaWeaponData* Weapon = Cast<USavaWeaponData>(Item))
	{
		DisplayName = Weapon->DisplayName;
		Icon = Weapon->Icon;
	}
	else if (const USavaLoadoutAbilityData* AbilityData = Cast<USavaLoadoutAbilityData>(Item))
	{
		DisplayName = AbilityData->DisplayName;
		Icon = AbilityData->Icon;
	}

	//表示名が未入力なら、アセット名を出す
	if (DisplayName.IsEmpty() && Item)
	{
		DisplayName = FText::FromString(Item->GetName());
	}
}

void USavaLoadoutSubsystem::ApplyChange(const FSavaLoadout& NewLoadout)
{
	//選べないもの(違う枠の武器など)を渡されたら、各リストの最初のものになる
	const FSavaLoadout Sanitized = USavaLoadoutSettings::Sanitize(NewLoadout);
	if (Sanitized == Loadout)
	{
		return;
	}

	Loadout = Sanitized;
	SaveToDisk();
	OnLoadoutChanged.Broadcast(Loadout);
}

//--------------------------------今選んでいるもの

USavaWeaponData* USavaLoadoutSubsystem::GetWeapon(ESavaWeaponSlot Slot) const
{
	return (Slot == ESavaWeaponSlot::Primary ? Loadout.PrimaryWeapon : Loadout.SecondaryWeapon).LoadSynchronous();
}

USavaSkillData* USavaLoadoutSubsystem::GetSkill() const
{
	return Loadout.Skill.LoadSynchronous();
}

USavaGadgetData* USavaLoadoutSubsystem::GetGadget() const
{
	return Loadout.Gadget.LoadSynchronous();
}

//--------------------------------選べるもの

TArray<USavaWeaponData*> USavaLoadoutSubsystem::GetSelectableWeapons(ESavaWeaponSlot Slot) const
{
	return USavaLoadoutSettings::GetSelectableWeapons(Slot);
}

TArray<USavaSkillData*> USavaLoadoutSubsystem::GetSelectableSkills() const
{
	return USavaLoadoutSettings::GetSelectableSkills();
}

TArray<USavaGadgetData*> USavaLoadoutSubsystem::GetSelectableGadgets() const
{
	return USavaLoadoutSettings::GetSelectableGadgets();
}

//--------------------------------保存

void USavaLoadoutSubsystem::LoadFromDisk()
{
	FSavaLoadout Saved;
	if (const USavaLoadoutSaveGame* SaveGame = Cast<USavaLoadoutSaveGame>(UGameplayStatics::LoadGameFromSlot(SavaLoadout::SaveSlotName, 0)))
	{
		Saved = SaveGame->Loadout;
	}

	//初めての起動(保存がない)や、保存した武器がもう選べない場合は、各リストの最初のものになる
	Loadout = USavaLoadoutSettings::Sanitize(Saved);
	UE_LOG(LogSavaLoadout, Log, TEXT("ロードアウトを読み込みました: %s / %s / %s / %s"),
		*Loadout.PrimaryWeapon.GetAssetName(), *Loadout.SecondaryWeapon.GetAssetName(),
		*Loadout.Skill.GetAssetName(), *Loadout.Gadget.GetAssetName());
}

void USavaLoadoutSubsystem::SaveToDisk() const
{
	USavaLoadoutSaveGame* SaveGame = Cast<USavaLoadoutSaveGame>(UGameplayStatics::CreateSaveGameObject(USavaLoadoutSaveGame::StaticClass()));
	SaveGame->Loadout = Loadout;
	if (!UGameplayStatics::SaveGameToSlot(SaveGame, SavaLoadout::SaveSlotName, 0))
	{
		UE_LOG(LogSavaLoadout, Warning, TEXT("ロードアウトを保存できませんでした"));
	}
}

//--------------------------------テスト用コンソールコマンド(ロードアウト画面ができるまでの確認用)

namespace SavaLoadout
{
	template <typename T>
	T* FindByName(const TArray<T*>& List, const FString& Name)
	{
		T* const* Found = List.FindByPredicate([&Name](const T* Asset) { return Asset->GetName() == Name; });
		if (!Found)
		{
			UE_LOG(LogSavaLoadout, Warning, TEXT("%s は選べるものの中にありません"), *Name);
			return nullptr;
		}
		return *Found;
	}

	static FAutoConsoleCommandWithWorldAndArgs SetLoadoutCommand(
		TEXT("Sava.SetLoadout"), TEXT("ロードアウトを変える。例: Sava.SetLoadout Skill DA_Blink(枠は Primary / Secondary / Skill / Gadget)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			USavaLoadoutSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<USavaLoadoutSubsystem>() : nullptr;
			if (!Subsystem || Args.Num() < 2)
			{
				return;
			}

			const FString& SlotName = Args[0];
			const FString& AssetName = Args[1];
			if (SlotName == TEXT("Primary") || SlotName == TEXT("Secondary"))
			{
				const ESavaWeaponSlot Slot = SlotName == TEXT("Primary") ? ESavaWeaponSlot::Primary : ESavaWeaponSlot::Secondary;
				if (USavaWeaponData* Weapon = FindByName(Subsystem->GetSelectableWeapons(Slot), AssetName))
				{
					Subsystem->SetWeapon(Slot, Weapon);
				}
			}
			else if (SlotName == TEXT("Skill"))
			{
				if (USavaSkillData* Skill = FindByName(Subsystem->GetSelectableSkills(), AssetName))
				{
					Subsystem->SetSkill(Skill);
				}
			}
			else if (SlotName == TEXT("Gadget"))
			{
				if (USavaGadgetData* Gadget = FindByName(Subsystem->GetSelectableGadgets(), AssetName))
				{
					Subsystem->SetGadget(Gadget);
				}
			}

			const FSavaLoadout& Loadout = Subsystem->GetLoadout();
			UE_LOG(LogSavaLoadout, Log, TEXT("ロードアウト: %s / %s / %s / %s"),
				*Loadout.PrimaryWeapon.GetAssetName(), *Loadout.SecondaryWeapon.GetAssetName(),
				*Loadout.Skill.GetAssetName(), *Loadout.Gadget.GetAssetName());
		}));
}

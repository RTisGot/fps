// Fill out your copyright notice in the Description page of Project Settings.

#include "Loadout/SavaLoadoutSubsystem.h"
#include "Loadout/SavaLoadoutSaveGame.h"
#include "Loadout/SavaLoadoutSettings.h"
#include "Loadout/SavaCharacterTypes.h"
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

void USavaLoadoutSubsystem::SetCharacter(FName CharacterId)
{
	FSavaLoadout NewLoadout = Loadout;
	NewLoadout.Character = CharacterId;
	ApplyChange(NewLoadout);
}

void USavaLoadoutSubsystem::SetWeapon(ESavaWeaponSlot Slot, USavaWeaponData* Weapon)
{
	FSavaLoadout NewLoadout = Loadout;
	(Slot == ESavaWeaponSlot::Primary ? NewLoadout.PrimaryWeapon : NewLoadout.SecondaryWeapon) = Weapon;
	//その武器で前に決めたカスタマイズに戻す(初めての武器なら変化なし)
	(Slot == ESavaWeaponSlot::Primary ? NewLoadout.PrimaryTuning : NewLoadout.SecondaryTuning) = WeaponTunings.FindRef(FSoftObjectPath(Weapon));
	ApplyChange(NewLoadout);
}

void USavaLoadoutSubsystem::SetWeaponTuning(ESavaWeaponSlot Slot, const FSavaWeaponTuning& Tuning)
{
	FSavaLoadout NewLoadout = Loadout;
	(Slot == ESavaWeaponSlot::Primary ? NewLoadout.PrimaryTuning : NewLoadout.SecondaryTuning) = Tuning;
	ApplyChange(NewLoadout);
}

void USavaLoadoutSubsystem::SetWeaponTuningSliderValue(ESavaWeaponSlot Slot, FName Slider, float Value)
{
	FSavaWeaponTuning Tuning = GetWeaponTuning(Slot);
	Tuning.SetSliderValue(Slider, Value);
	SetWeaponTuning(Slot, Tuning);
}

void USavaLoadoutSubsystem::SetWeaponTuningOption(ESavaWeaponSlot Slot, FName Toggle, FName Option)
{
	FSavaWeaponTuning Tuning = GetWeaponTuning(Slot);
	Tuning.SetToggleOption(Toggle, Option);
	SetWeaponTuning(Slot, Tuning);
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
	if (const USavaCharacterOption* Character = Cast<USavaCharacterOption>(Item))
	{
		SetCharacter(Character->CharacterId);
	}
	else if (USavaWeaponData* Weapon = Cast<USavaWeaponData>(Item))
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

	if (const USavaCharacterOption* Character = Cast<USavaCharacterOption>(Item))
	{
		return Loadout.Character == Character->CharacterId;
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

	if (const USavaCharacterOption* Character = Cast<USavaCharacterOption>(Item))
	{
		if (const FSavaCharacterRow* Row = USavaLoadoutSettings::FindCharacterRow(Character->CharacterId))
		{
			DisplayName = Row->DisplayName;
			Icon = Row->Icon;
		}
		//表示名が未入力なら、行名を出す
		if (DisplayName.IsEmpty())
		{
			DisplayName = FText::FromName(Character->CharacterId);
		}
		return;
	}

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
	RememberWeaponTunings();
	SaveToDisk();
	OnLoadoutChanged.Broadcast(Loadout);
}

//--------------------------------今選んでいるもの

USavaWeaponData* USavaLoadoutSubsystem::GetWeapon(ESavaWeaponSlot Slot) const
{
	return (Slot == ESavaWeaponSlot::Primary ? Loadout.PrimaryWeapon : Loadout.SecondaryWeapon).LoadSynchronous();
}

FSavaWeaponTuning USavaLoadoutSubsystem::GetWeaponTuning(ESavaWeaponSlot Slot) const
{
	return Slot == ESavaWeaponSlot::Primary ? Loadout.PrimaryTuning : Loadout.SecondaryTuning;
}

FSavaWeaponStats USavaLoadoutSubsystem::GetTunedWeaponStats(ESavaWeaponSlot Slot) const
{
	const USavaWeaponData* Weapon = GetWeapon(Slot);
	return Weapon ? Weapon->GetTunedStats(GetWeaponTuning(Slot)) : FSavaWeaponStats();
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

TArray<USavaCharacterOption*> USavaLoadoutSubsystem::GetSelectableCharacters()
{
	const TArray<FName> CharacterIds = USavaLoadoutSettings::GetSelectableCharacters();
	if (CharacterOptions.Num() != CharacterIds.Num())
	{
		CharacterOptions.Reset();
		for (const FName CharacterId : CharacterIds)
		{
			USavaCharacterOption* Option = NewObject<USavaCharacterOption>(this);
			Option->CharacterId = CharacterId;
			CharacterOptions.Add(Option);
		}
	}
	return CharacterOptions;
}

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

//--------------------------------枠の種類で扱う

TArray<UObject*> USavaLoadoutSubsystem::GetSelectableItems(ESavaLoadoutCategory Category)
{
	TArray<UObject*> Items;
	switch (Category)
	{
	case ESavaLoadoutCategory::Character:
		Items.Append(GetSelectableCharacters());
		break;
	case ESavaLoadoutCategory::PrimaryWeapon:
		Items.Append(GetSelectableWeapons(ESavaWeaponSlot::Primary));
		break;
	case ESavaLoadoutCategory::SecondaryWeapon:
		Items.Append(GetSelectableWeapons(ESavaWeaponSlot::Secondary));
		break;
	case ESavaLoadoutCategory::Skill:
		Items.Append(GetSelectableSkills());
		break;
	case ESavaLoadoutCategory::Gadget:
		Items.Append(GetSelectableGadgets());
		break;
	}
	return Items;
}

UObject* USavaLoadoutSubsystem::GetSelectedItem(ESavaLoadoutCategory Category)
{
	switch (Category)
	{
	case ESavaLoadoutCategory::Character:
		//キャラクターは一覧の項目(USavaCharacterOption)を返す
		for (USavaCharacterOption* Option : GetSelectableCharacters())
		{
			if (Option->CharacterId == Loadout.Character)
			{
				return Option;
			}
		}
		return nullptr;
	case ESavaLoadoutCategory::PrimaryWeapon:
		return GetWeapon(ESavaWeaponSlot::Primary);
	case ESavaLoadoutCategory::SecondaryWeapon:
		return GetWeapon(ESavaWeaponSlot::Secondary);
	case ESavaLoadoutCategory::Skill:
		return GetSkill();
	case ESavaLoadoutCategory::Gadget:
		return GetGadget();
	}
	return nullptr;
}

FText USavaLoadoutSubsystem::GetCategoryDisplayName(ESavaLoadoutCategory Category)
{
	switch (Category)
	{
	case ESavaLoadoutCategory::Character:
		return NSLOCTEXT("SavaLoadout", "CategoryCharacter", "キャラクター");
	case ESavaLoadoutCategory::PrimaryWeapon:
		return NSLOCTEXT("SavaLoadout", "CategoryPrimaryWeapon", "メイン武器");
	case ESavaLoadoutCategory::SecondaryWeapon:
		return NSLOCTEXT("SavaLoadout", "CategorySecondaryWeapon", "サブ武器");
	case ESavaLoadoutCategory::Skill:
		return NSLOCTEXT("SavaLoadout", "CategorySkill", "スキル");
	case ESavaLoadoutCategory::Gadget:
		return NSLOCTEXT("SavaLoadout", "CategoryGadget", "ガジェット");
	}
	return FText::GetEmpty();
}

bool USavaLoadoutSubsystem::GetCategoryWeaponSlot(ESavaLoadoutCategory Category, ESavaWeaponSlot& Slot)
{
	Slot = ESavaWeaponSlot::Primary;
	if (Category == ESavaLoadoutCategory::PrimaryWeapon)
	{
		return true;
	}
	if (Category == ESavaLoadoutCategory::SecondaryWeapon)
	{
		Slot = ESavaWeaponSlot::Secondary;
		return true;
	}
	return false;
}

FSavaWeaponStats USavaLoadoutSubsystem::GetWeaponStatsWithSavedTuning(const USavaWeaponData* Weapon) const
{
	if (!Weapon)
	{
		return FSavaWeaponStats();
	}

	const FSoftObjectPath WeaponPath(Weapon);
	if (Loadout.PrimaryWeapon.ToSoftObjectPath() == WeaponPath)
	{
		return Weapon->GetTunedStats(Loadout.PrimaryTuning);
	}
	if (Loadout.SecondaryWeapon.ToSoftObjectPath() == WeaponPath)
	{
		return Weapon->GetTunedStats(Loadout.SecondaryTuning);
	}
	//保存した値は武器データが変わっているかもしれないので直してから使う
	return Weapon->GetTunedStats(Weapon->SanitizeTuning(WeaponTunings.FindRef(WeaponPath)));
}

//--------------------------------保存

void USavaLoadoutSubsystem::RememberWeaponTunings()
{
	if (!Loadout.PrimaryWeapon.IsNull())
	{
		WeaponTunings.Add(Loadout.PrimaryWeapon.ToSoftObjectPath(), Loadout.PrimaryTuning);
	}
	if (!Loadout.SecondaryWeapon.IsNull())
	{
		WeaponTunings.Add(Loadout.SecondaryWeapon.ToSoftObjectPath(), Loadout.SecondaryTuning);
	}
}

void USavaLoadoutSubsystem::LoadFromDisk()
{
	FSavaLoadout Saved;
	if (const USavaLoadoutSaveGame* SaveGame = Cast<USavaLoadoutSaveGame>(UGameplayStatics::LoadGameFromSlot(SavaLoadout::SaveSlotName, 0)))
	{
		Saved = SaveGame->Loadout;
		//武器ごとのカスタマイズは、その武器を選んだときに Sanitize で直る
		WeaponTunings = SaveGame->WeaponTunings;
	}

	//初めての起動(保存がない)や、保存した武器がもう選べない場合は、各リストの最初のものになる
	Loadout = USavaLoadoutSettings::Sanitize(Saved);
	RememberWeaponTunings();
	UE_LOG(LogSavaLoadout, Log, TEXT("ロードアウトを読み込みました: %s / %s / %s / %s / %s"),
		*Loadout.Character.ToString(), *Loadout.PrimaryWeapon.GetAssetName(), *Loadout.SecondaryWeapon.GetAssetName(),
		*Loadout.Skill.GetAssetName(), *Loadout.Gadget.GetAssetName());
}

void USavaLoadoutSubsystem::SaveToDisk() const
{
	USavaLoadoutSaveGame* SaveGame = Cast<USavaLoadoutSaveGame>(UGameplayStatics::CreateSaveGameObject(USavaLoadoutSaveGame::StaticClass()));
	SaveGame->Loadout = Loadout;
	SaveGame->WeaponTunings = WeaponTunings;
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
		TEXT("Sava.SetLoadout"), TEXT("ロードアウトを変える。例: Sava.SetLoadout Skill DA_Blink(枠は Character / Primary / Secondary / Skill / Gadget。Character は DT_Character の行名)"),
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
			if (SlotName == TEXT("Character"))
			{
				if (USavaLoadoutSettings::GetSelectableCharacters().Contains(FName(*AssetName)))
				{
					Subsystem->SetCharacter(FName(*AssetName));
				}
				else
				{
					UE_LOG(LogSavaLoadout, Warning, TEXT("%s は選べるキャラクターの中にありません"), *AssetName);
				}
			}
			else if (SlotName == TEXT("Primary") || SlotName == TEXT("Secondary"))
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
			UE_LOG(LogSavaLoadout, Log, TEXT("ロードアウト: %s / %s / %s / %s / %s"),
				*Loadout.Character.ToString(), *Loadout.PrimaryWeapon.GetAssetName(), *Loadout.SecondaryWeapon.GetAssetName(),
				*Loadout.Skill.GetAssetName(), *Loadout.Gadget.GetAssetName());
		}));

	void LogTuning(const USavaLoadoutSubsystem* Subsystem, ESavaWeaponSlot Slot)
	{
		const FSavaWeaponTuning Tuning = Subsystem->GetWeaponTuning(Slot);
		TArray<FString> Parts;
		for (const FSavaWeaponTuningSliderValue& Value : Tuning.Sliders)
		{
			Parts.Add(FString::Printf(TEXT("%s %.2f"), *Value.Slider.ToString(), Value.Value));
		}
		for (const FSavaWeaponTuningToggleValue& Value : Tuning.Toggles)
		{
			Parts.Add(FString::Printf(TEXT("%s %s"), *Value.Toggle.ToString(), *Value.Option.ToString()));
		}
		UE_LOG(LogSavaLoadout, Log, TEXT("%s のカスタマイズ: %s"), Slot == ESavaWeaponSlot::Primary ? TEXT("Primary") : TEXT("Secondary"), *FString::Join(Parts, TEXT(" / ")));
	}

	static FAutoConsoleCommandWithWorldAndArgs SetTuningCommand(
		TEXT("Sava.SetTuning"), TEXT("武器のスライダーを変える。例: Sava.SetTuning Primary Barrel 0.5(スライダーは武器データの Tuning Sliders の Id、値は -1 〜 1)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			USavaLoadoutSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<USavaLoadoutSubsystem>() : nullptr;
			if (!Subsystem || Args.Num() < 3)
			{
				return;
			}

			const ESavaWeaponSlot Slot = Args[0] == TEXT("Secondary") ? ESavaWeaponSlot::Secondary : ESavaWeaponSlot::Primary;
			const USavaWeaponData* Weapon = Subsystem->GetWeapon(Slot);
			if (!Weapon || !Weapon->FindTuningSlider(FName(*Args[1])))
			{
				UE_LOG(LogSavaLoadout, Warning, TEXT("%s に %s というスライダーはありません"), Weapon ? *Weapon->GetName() : TEXT("武器"), *Args[1]);
				return;
			}

			Subsystem->SetWeaponTuningSliderValue(Slot, FName(*Args[1]), FCString::Atof(*Args[2]));
			LogTuning(Subsystem, Slot);
		}));

	static FAutoConsoleCommandWithWorldAndArgs SetTuningOptionCommand(
		TEXT("Sava.SetTuningOption"), TEXT("武器の切り替え式のカスタマイズを変える。例: Sava.SetTuningOption Primary Sight Holo(武器データの Tuning Toggles の Id と、その Options の Id)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			USavaLoadoutSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<USavaLoadoutSubsystem>() : nullptr;
			if (!Subsystem || Args.Num() < 3)
			{
				return;
			}

			const ESavaWeaponSlot Slot = Args[0] == TEXT("Secondary") ? ESavaWeaponSlot::Secondary : ESavaWeaponSlot::Primary;
			const USavaWeaponData* Weapon = Subsystem->GetWeapon(Slot);
			const FSavaWeaponTuningToggle* Toggle = Weapon ? Weapon->FindTuningToggle(FName(*Args[1])) : nullptr;
			const FName OptionId(*Args[2]);
			if (!Toggle || !Toggle->Options.ContainsByPredicate([OptionId](const FSavaWeaponTuningOption& Option) { return Option.Id == OptionId; }))
			{
				UE_LOG(LogSavaLoadout, Warning, TEXT("%s に %s / %s という切り替えはありません"), Weapon ? *Weapon->GetName() : TEXT("武器"), *Args[1], *Args[2]);
				return;
			}

			Subsystem->SetWeaponTuningOption(Slot, Toggle->Id, OptionId);
			LogTuning(Subsystem, Slot);
		}));
}

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Loadout/SavaLoadoutTypes.h"
#include "Weapon/SavaWeaponTypes.h"
#include "SavaLoadoutSettings.generated.h"

class USavaGadgetData;
class USavaSkillData;
class USavaWeaponData;

//ロードアウトで選べるもの(Project Settings → Game → Sava Loadout)
//・ロードアウト画面にはここに並べた順で出る
//・サーバーは、届いたロードアウトがここにあるものだけか確かめる(ないものは各リストの最初のものに置き換える)
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Sava Loadout"))
class SAVA_API USavaLoadoutSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	//武器(メイン・サブはそれぞれの武器データの Slot で分ける)
	UPROPERTY(Config, EditAnywhere, Category = "Loadout")
	TArray<TSoftObjectPtr<USavaWeaponData>> Weapons;

	UPROPERTY(Config, EditAnywhere, Category = "Loadout")
	TArray<TSoftObjectPtr<USavaSkillData>> Skills;

	UPROPERTY(Config, EditAnywhere, Category = "Loadout")
	TArray<TSoftObjectPtr<USavaGadgetData>> Gadgets;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	//選べるもの(読み込み済みのアセット)
	static TArray<USavaWeaponData*> GetSelectableWeapons(ESavaWeaponSlot Slot);
	static TArray<USavaSkillData*> GetSelectableSkills();
	static TArray<USavaGadgetData*> GetSelectableGadgets();

	//選べないもの・空の枠を、各リストの最初のものに置き換えたロードアウトを返す
	static FSavaLoadout Sanitize(const FSavaLoadout& Loadout);
};

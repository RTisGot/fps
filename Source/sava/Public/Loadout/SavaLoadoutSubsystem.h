// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Loadout/SavaLoadoutTypes.h"
#include "Weapon/SavaWeaponTypes.h"
#include "SavaLoadoutSubsystem.generated.h"

class USavaGadgetData;
class USavaSkillData;
class USavaWeaponData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSavaOnLocalLoadoutChanged, const FSavaLoadout&, Loadout);

//自分が選んだロードアウトを持つ(自分の PC だけの情報。マップを移っても残る)
//・起動時にディスクから読み、変えるたびに保存する
//・変わると OnLoadoutChanged が呼ばれ、ASavaPlayerController がサーバーへ送る
//ロードアウト画面はここを読み書きするだけでよい
UCLASS()
class SAVA_API USavaLoadoutSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	const FSavaLoadout& GetLoadout() const { return Loadout; }

	//--------------------------------変更(画面から呼ぶ)

	UFUNCTION(BlueprintCallable, Category = "Sava|Loadout")
	void SetWeapon(ESavaWeaponSlot Slot, USavaWeaponData* Weapon);

	UFUNCTION(BlueprintCallable, Category = "Sava|Loadout")
	void SetSkill(USavaSkillData* Skill);

	UFUNCTION(BlueprintCallable, Category = "Sava|Loadout")
	void SetGadget(USavaGadgetData* Gadget);

	//--------------------------------今選んでいるもの(読み込み済みのアセット)

	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	USavaWeaponData* GetWeapon(ESavaWeaponSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	USavaSkillData* GetSkill() const;

	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	USavaGadgetData* GetGadget() const;

	//--------------------------------選べるもの(Project Settings → Game → Sava Loadout)

	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	TArray<USavaWeaponData*> GetSelectableWeapons(ESavaWeaponSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	TArray<USavaSkillData*> GetSelectableSkills() const;

	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	TArray<USavaGadgetData*> GetSelectableGadgets() const;

	//ロードアウトが変わった
	UPROPERTY(BlueprintAssignable, Category = "Sava|Loadout")
	FSavaOnLocalLoadoutChanged OnLoadoutChanged;

private:
	//選べないものを直して持ち、保存して知らせる
	void ApplyChange(const FSavaLoadout& NewLoadout);

	void LoadFromDisk();
	void SaveToDisk() const;

	FSavaLoadout Loadout;
};

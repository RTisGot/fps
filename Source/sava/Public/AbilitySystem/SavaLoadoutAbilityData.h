// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SavaLoadoutAbilityData.generated.h"

class UTexture2D;
class USavaGameplayAbility;

//スキル・ガジェット 1 種類分のデータの共通部分(データアセット)
//武器の USavaWeaponData に当たるもの。USavaAbilityLoadoutComponent が「何を持っているか」としてこれを保持する
UCLASS(Abstract, BlueprintType, Const)
class SAVA_API USavaLoadoutAbilityData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//画面に出す名前
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	FText DisplayName;

	//選択画面・HUD に出すアイコン
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	TObjectPtr<UTexture2D> Icon;

	//持たせる能力(クールダウンや個数は能力側の Cooldown Duration / Max Charges で決める)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	TSubclassOf<USavaGameplayAbility> Ability;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout", meta = (ClampMin = "1"))
	int32 AbilityLevel = 1;

	//どのボタンで発動するか(DA_InputConfig に登録したタグ。パッシブなら空でよい)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

//スキル(クールダウン制の能力。例: グラップル)
//新しいスキルは、これを 1 つ作って能力とボタンを入れるだけで追加できる
UCLASS(BlueprintType, Const)
class SAVA_API USavaSkillData : public USavaLoadoutAbilityData
{
	GENERATED_BODY()
};

//ガジェット(個数制の能力)。個数は能力側の Max Charges で決める。1 人 1 つ
UCLASS(BlueprintType, Const)
class SAVA_API USavaGadgetData : public USavaLoadoutAbilityData
{
	GENERATED_BODY()
};

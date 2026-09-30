// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AbilitySystem/SavaGameplayEffects.h"
#include "SavaAbilitySettings.generated.h"

//全員が共通で使う GameplayEffect を選ぶ設定(Project Settings → Game → Sava Abilities)
//選べるのは C++ の共通 GE の子 Blueprint だけ(数値を受け取る SetByCaller の設定を親から引き継ぐため)
//空のときは C++ の共通 GE をそのまま使う
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Sava Abilities"))
class SAVA_API USavaAbilitySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	//ダメージ(Apply Damage)で使う GE
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay Effects")
	TSoftClassPtr<USavaGE_Damage> DamageEffect;

	//回復(Apply Healing)で使う GE
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay Effects")
	TSoftClassPtr<USavaGE_Healing> HealingEffect;

	//能力の Cooldown Duration / Cooldown Tags で使う GE
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay Effects")
	TSoftClassPtr<USavaGE_Cooldown> CooldownEffect;

	//個数制の能力(Max Charges が 1 以上)のコストで使う GE
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay Effects")
	TSoftClassPtr<USavaGE_GadgetChargeCost> GadgetChargeCostEffect;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	//設定された GE(なければ C++ の共通 GE)を返す
	static TSubclassOf<UGameplayEffect> GetDamageEffectClass();
	static TSubclassOf<UGameplayEffect> GetHealingEffectClass();
	static TSubclassOf<UGameplayEffect> GetCooldownEffectClass();
	static TSubclassOf<UGameplayEffect> GetGadgetChargeCostEffectClass();
};

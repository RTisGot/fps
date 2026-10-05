// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/SavaGameplayAbility.h"
#include "SavaWeaponAbility.generated.h"

class USavaEquipmentComponent;
class USavaWeaponData;

//武器の能力(射撃・ADS・リロード・持ち替え)の親クラス
//・Ability.Type.Weapon タグが付く
//・武器を持っていなければ発動できない
UCLASS(Abstract)
class SAVA_API USavaWeaponAbility : public USavaGameplayAbility
{
	GENERATED_BODY()

public:
	USavaWeaponAbility();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	//この能力を使っているキャラクターの装備
	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	USavaEquipmentComponent* GetEquipmentComponent() const;

	//今持っている武器
	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	USavaWeaponData* GetCurrentWeaponData() const;

protected:
	//アセットタグを 1 つ追加する(コンストラクタでのみ呼ぶ)
	void AddAssetTag(const FGameplayTag& Tag);

	static USavaEquipmentComponent* GetEquipmentFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo);
};

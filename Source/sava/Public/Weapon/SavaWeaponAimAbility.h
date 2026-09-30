// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/SavaWeaponAbility.h"
#include "SavaWeaponAimAbility.generated.h"

//ADS(覗き込み)。押している間だけ State.Weapon.Aiming タグが付く
//視野角のズームと拡散の切り替えは、装備コンポーネントがこのタグを見て行う(ADS Time をかけて滑らかに変わる)
UCLASS()
class SAVA_API USavaWeaponAimAbility : public USavaWeaponAbility
{
	GENERATED_BODY()

public:
	USavaWeaponAimAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

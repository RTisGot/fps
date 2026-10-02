// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/SavaGameplayAbility.h"
#include "SavaGrappleAbility.generated.h"

/**
 * グラップル用の能力。移動系の能力は、キャラクターの移動コンポーネントを使って移動する
 */
UCLASS()
class SAVA_API USavaGrappleAbility : public USavaGameplayAbility
{
	GENERATED_BODY()

public:
	USavaGrappleAbility();

protected:

	//GAS
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	)override;

	
};

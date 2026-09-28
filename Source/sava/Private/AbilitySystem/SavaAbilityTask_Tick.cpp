// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/SavaAbilityTask_Tick.h"

USavaAbilityTask_Tick::USavaAbilityTask_Tick(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

USavaAbilityTask_Tick* USavaAbilityTask_Tick::CreateTickTask(UGameplayAbility* OwningAbility)
{
	return NewAbilityTask<USavaAbilityTask_Tick>(OwningAbility);
}

void USavaAbilityTask_Tick::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);
	OnTick.Broadcast(DeltaTime);
}

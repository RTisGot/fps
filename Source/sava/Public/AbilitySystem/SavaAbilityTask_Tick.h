// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "SavaAbilityTask_Tick.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FSavaAbilityTaskTickDelegate, float /*DeltaTime*/);

//能力が終わるまで毎フレーム OnTick を呼ぶ(プレビュー表示など、見た目の更新用)
UCLASS()
class SAVA_API USavaAbilityTask_Tick : public UAbilityTask
{
	GENERATED_BODY()

public:
	USavaAbilityTask_Tick(const FObjectInitializer& ObjectInitializer);

	static USavaAbilityTask_Tick* CreateTickTask(UGameplayAbility* OwningAbility);

	virtual void TickTask(float DeltaTime) override;

	FSavaAbilityTaskTickDelegate OnTick;
};

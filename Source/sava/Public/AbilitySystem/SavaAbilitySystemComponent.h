// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "SavaAbilitySystemComponent.generated.h"

//能力の管理役。PlayerState に 1 つ置く(リスポーンしてもクールダウン等が残る)
UCLASS()
class SAVA_API USavaAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	USavaAbilitySystemComponent();

	//ボタンが押された/離された。InputTag が一致する能力を発動・通知する(キャラクターの入力から呼ぶ)
	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

private:
	//狙っている最中の能力のうち、このボタンがキャンセルボタンのものをキャンセルする。1 つでもあれば true
	bool CancelAimingAbilities(const FGameplayTag& InputTag);
};

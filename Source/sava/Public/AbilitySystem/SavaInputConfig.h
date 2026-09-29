// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SavaInputConfig.generated.h"

class UInputAction;

//入力アクション → InputTag の対応 1 つ分
USTRUCT(BlueprintType)
struct FSavaInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

//能力用の入力の対応表(データアセット)。ここに書いたボタンは、同じ InputTag を持つ能力を発動する
UCLASS(BlueprintType, Const)
class SAVA_API USavaInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (TitleProperty = "InputTag"))
	TArray<FSavaInputAction> AbilityInputActions;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "SavaAbilitySet.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class USavaGameplayAbility;

//付与する能力 1 つ分
USTRUCT(BlueprintType)
struct FSavaAbilitySet_GameplayAbility
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<USavaGameplayAbility> Ability;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "1"))
	int32 AbilityLevel = 1;

	//どのボタンで発動するか(パッシブなら空でよい)
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

//付与する GameplayEffect 1 つ分(初期ステータス・常時効果など)
USTRUCT(BlueprintType)
struct FSavaAbilitySet_GameplayEffect
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> GameplayEffect;

	UPROPERTY(EditDefaultsOnly)
	float EffectLevel = 1.0f;
};

//付与したものを後で取り除くための控え
USTRUCT()
struct FSavaAbilitySet_GrantedHandles
{
	GENERATED_BODY()

	void TakeFromAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent);

	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;
};

//キャラクターに持たせる能力のセット(データアセット)
//各担当は自分の能力をここに追加するだけで、コードを書かずにキャラクターへ持たせられる
UCLASS(BlueprintType, Const)
class SAVA_API USavaAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//サーバーでのみ呼ぶ
	void GiveToAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent, FSavaAbilitySet_GrantedHandles* OutGrantedHandles) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Abilities", meta = (TitleProperty = "Ability"))
	TArray<FSavaAbilitySet_GameplayAbility> GrantedGameplayAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Effects", meta = (TitleProperty = "GameplayEffect"))
	TArray<FSavaAbilitySet_GameplayEffect> GrantedGameplayEffects;
};

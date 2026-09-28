// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "SavaAttributeSet.generated.h"

//Get/Set/Init 関数をまとめて作るマクロ(例: GetHealth(), SetHealth(), GetHealthAttribute())
#define SAVA_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

//HP が 0 になったとき(サーバーのみ)
DECLARE_MULTICAST_DELEGATE_ThreeParams(FSavaOutOfHealthDelegate, AActor* /*DamageInstigator*/, AActor* /*DamageCauser*/, float /*DamageAmount*/);

//プレイヤーの数値。ダメージ・回復は必ず GameplayEffect 経由で Damage / Healing に入れる(直接 Health を書き換えない)
UCLASS()
class SAVA_API USavaAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	USavaAttributeSet();

	SAVA_ATTRIBUTE_ACCESSORS(USavaAttributeSet, Health);
	SAVA_ATTRIBUTE_ACCESSORS(USavaAttributeSet, MaxHealth);
	SAVA_ATTRIBUTE_ACCESSORS(USavaAttributeSet, MoveSpeedMultiplier);
	SAVA_ATTRIBUTE_ACCESSORS(USavaAttributeSet, Damage);
	SAVA_ATTRIBUTE_ACCESSORS(USavaAttributeSet, Healing);

	mutable FSavaOutOfHealthDelegate OnOutOfHealth;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue);

private:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Sava|Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Sava|Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxHealth;

	//移動速度の倍率(1.0 = 通常)。CharacterMovement が歩き・ダッシュ速度に掛ける
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeedMultiplier, Category = "Sava|Movement", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MoveSpeedMultiplier;

	//受けるダメージ量の受け渡し用(サーバーで Health に反映して 0 に戻す。複製しない)
	UPROPERTY(BlueprintReadOnly, Category = "Sava|Meta", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Damage;

	//受ける回復量の受け渡し用(サーバーで Health に反映して 0 に戻す。複製しない)
	UPROPERTY(BlueprintReadOnly, Category = "Sava|Meta", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Healing;

	bool bOutOfHealth = false;
};

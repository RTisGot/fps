// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "SavaTestDummyCharacter.generated.h"

class USavaAbilitySystemComponent;
class USavaAttributeSet;

//キル速チェック用のダミーキャラクター。PlayerState がないので簡易版
UCLASS()
class SAVA_API ASavaTestDummyCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ASavaTestDummyCharacter();

    virtual void BeginPlay() override;
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    //現在のHP を取得
    UFUNCTION(BlueprintPure, Category = "Test Dummy")
    float GetCurrentHealth() const;

    //最大HP を取得
    UFUNCTION(BlueprintPure, Category = "Test Dummy")
    float GetMaxHealth() const;

protected:
    //Ability System Component(PlayerState がないので自分が持つ)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability System")
    TObjectPtr<USavaAbilitySystemComponent> AbilitySystemComponent;

    //属性セット
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability System")
    TObjectPtr<USavaAttributeSet> AttributeSet;

    //最大HP
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Dummy", meta = (ClampMin = "1"))
    float MaxHealthValue = 100.0f;
};
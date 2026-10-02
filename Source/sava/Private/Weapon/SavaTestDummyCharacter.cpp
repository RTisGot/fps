// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/SavaTestDummyCharacter.h"
#include "AbilitySystem/SavaAbilitySystemComponent.h"
#include "AbilitySystem/SavaAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ASavaTestDummyCharacter::ASavaTestDummyCharacter()
{
    PrimaryActorTick.TickInterval = 0.0f;

    //重力を有効化
    GetCharacterMovement()->GravityScale = 1.0f;
    GetCharacterMovement()->MaxWalkSpeed = 0.0f;

    //カプセルサイズを設定
    GetCapsuleComponent()->InitCapsuleSize(55.0f, 96.0f);

    //Ability System Component を作成
    AbilitySystemComponent = CreateDefaultSubobject<USavaAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

    //属性セットを作成
    AttributeSet = CreateDefaultSubobject<USavaAttributeSet>(TEXT("AttributeSet"));

    //レプリケーションなし(ダミーはシングルプレイヤーテスト用)
    bReplicates = false;
}

void ASavaTestDummyCharacter::BeginPlay()
{
    Super::BeginPlay();

    //ASC を初期化
    AbilitySystemComponent->InitAbilityActorInfo(this, this);

    //最大HP を設定
    if (AttributeSet)
    {
        AttributeSet->SetMaxHealth(MaxHealthValue);
        AttributeSet->SetHealth(MaxHealthValue);
    }
}

UAbilitySystemComponent* ASavaTestDummyCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

float ASavaTestDummyCharacter::GetCurrentHealth() const
{
    return AttributeSet ? AttributeSet->GetHealth() : 0.0f;
}

float ASavaTestDummyCharacter::GetMaxHealth() const
{
    return AttributeSet ? AttributeSet->GetMaxHealth() : 0.0f;
}
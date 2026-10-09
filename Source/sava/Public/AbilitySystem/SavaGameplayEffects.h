// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SavaGameplayEffects.generated.h"

//全員が共通で使う GameplayEffect。数値は SetByCaller で渡す(USavaAbilitySystemLibrary 経由で使う)

//ダメージ(即時)。SetByCaller.Damage の値を Damage に入れる → Health が減る
UCLASS()
class SAVA_API USavaGE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USavaGE_Damage();
};

//回復(即時)。SetByCaller.Healing の値を Healing に入れる
UCLASS()
class SAVA_API USavaGE_Healing : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USavaGE_Healing();
};

//クールダウン(期間 = SetByCaller.Cooldown 秒)。付与するタグは USavaGameplayAbility が能力ごとに追加する
UCLASS()
class SAVA_API USavaGE_Cooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USavaGE_Cooldown();
};

//ガジェットのコスト(即時)。GadgetCharges を 1 減らす。USavaGameplayAbility の Max Charges が 1 以上のときに使われる
//残りが 0 なら発動できない(GAS のコストの確認で止まる)
UCLASS()
class SAVA_API USavaGE_GadgetChargeCost : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USavaGE_GadgetChargeCost();
};

//旗を運んでいる間(無期限)。State.CarryingFlag を付け、移動速度に SetByCaller.MoveSpeedMultiplier を掛ける
//旗(ASavaFlag)が拾われたときに付け、落とした・持ち帰ったときに外す
UCLASS()
class SAVA_API USavaGE_CarryingFlag : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USavaGE_CarryingFlag(const FObjectInitializer& ObjectInitializer);
};

//ラウンド開始前のカウントダウン中(無期限)。State.RoundFrozen を付け、移動速度を 0 にする
//CTF の GameMode がカウントダウンの開始で付け、終了で外す
UCLASS()
class SAVA_API USavaGE_RoundFrozen : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USavaGE_RoundFrozen(const FObjectInitializer& ObjectInitializer);
};

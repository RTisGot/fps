// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GadgetData.generated.h"

/**
 * ガジェット共通の設定値。
 *
 * ガジェット固有の処理は持たず、
 * 「そのガジェットがどのような数値を持つか」だけを定義する。
 *
 * 所持数・クールダウンは GameplayAbility 側で管理する。
 */
USTRUCT(BlueprintType)
struct SAVA_API FGadgetData
{
	GENERATED_BODY()

	//========================================
	// 基本
	//========================================

	// ガジェットを識別するID
	UPROPERTY(EditDefaultsOnly, Category = "Gadget")
	FName GadgetId = NAME_None;

	//========================================
	// 投擲
	//========================================

	// 投擲速度
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|Throw",
		meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float ThrowSpeed = 1500.0f;

	// 投擲時の重力倍率
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|Throw",
		meta = (ClampMin = "0"))
	float Gravity = 1.0f;

	// 最大投擲距離
	//
	// 0 の場合は現時点では距離制限なし。
	// 実際の距離制限処理は今後実装する。
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|Throw",
		meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxThrowDistance = 0.0f;

	//========================================
	// 効果
	//========================================

	// 起爆までの時間
	//
	// 0 の場合は、現時点では待機時間なし。
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|Effect",
		meta = (ClampMin = "0", ForceUnits = "s"))
	float FuseTime = 0.0f;

	// 効果範囲
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|Effect",
		meta = (ClampMin = "0", ForceUnits = "cm"))
	float EffectRadius = 0.0f;

	// 効果時間
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|Effect",
		meta = (ClampMin = "0", ForceUnits = "s"))
	float EffectDuration = 0.0f;

	// ダメージ
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|Effect",
		meta = (ClampMin = "0"))
	float Damage = 0.0f;

	// 減速率
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|Effect",
		meta = (ClampMin = "0"))
	float SlowRate = 0.0f;
};
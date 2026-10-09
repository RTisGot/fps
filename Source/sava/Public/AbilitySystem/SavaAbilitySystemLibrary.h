// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ActiveGameplayEffectHandle.h"
#include "SavaAbilitySystemLibrary.generated.h"

class APlayerState;
class UGameplayEffect;

//全員が共通で使う関数(Blueprint からも呼べる)
//ダメージ・回復は必ずここを通す(計算方法を 1 か所にまとめるため)
UCLASS()
class SAVA_API USavaAbilitySystemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//ダメージを与える(サーバーのみ)。相手が ASC を持たない場合は何もせず false を返す
	//DamageInstigator: 攻撃したプレイヤーのキャラクター / DamageCauser: 弾など、実際に当たった物
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sava|Damage")
	static bool ApplyDamage(AActor* DamageInstigator, AActor* Target, float Damage, AActor* DamageCauser);

	//回復する(サーバーのみ)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sava|Damage")
	static bool ApplyHealing(AActor* HealInstigator, AActor* Target, float Amount);

	//任意の GameplayEffect を相手に付ける(サーバーのみ。状態異常など)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sava|Effect")
	static FActiveGameplayEffectHandle ApplyEffectToTarget(AActor* EffectInstigator, AActor* Target, TSubclassOf<UGameplayEffect> EffectClass, float Level = 1.0f);

	//持ち主のプレイヤーの PlayerState(キャラクター・死んだ体・その人が出した弾や設置物に対応)。見つからなければ null
	UFUNCTION(BlueprintPure, Category = "Sava|Team")
	static APlayerState* GetOwningPlayerState(const AActor* Actor);

	//所属チーム(キャラクター・PlayerState・その人が出した弾や設置物に対応)。未所属なら 255
	UFUNCTION(BlueprintPure, Category = "Sava|Team")
	static uint8 GetTeamId(const AActor* Actor);

	//敵同士か。同じプレイヤーの物同士や同じチームなら false。チーム未所属同士は敵とみなす
	UFUNCTION(BlueprintPure, Category = "Sava|Team")
	static bool AreEnemies(const AActor* A, const AActor* B);
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "SavaPlayerState.generated.h"

class USavaAbilitySystemComponent;
class USavaAttributeSet;

//プレイヤーごとの情報。能力(ASC)と数値(AttributeSet)、チームをここに持つ
UCLASS()
class SAVA_API ASavaPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	//チーム未所属
	static constexpr uint8 NoTeam = 255;

	ASavaPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	USavaAbilitySystemComponent* GetSavaAbilitySystemComponent() const { return AbilitySystemComponent; }
	const USavaAttributeSet* GetAttributeSet() const { return AttributeSet; }

	UFUNCTION(BlueprintPure, Category = "Sava|Team")
	uint8 GetTeamId() const { return TeamId; }

	//サーバーでのみ呼ぶ(GameMode がチーム分けに使う)
	void SetTeamId(uint8 NewTeamId);

	void Respawn();

	// 現在のキル数を取得
	UFUNCTION(BlueprintPure, Category = "Sava|Kill")
	int32 GetKillCount() const { return KillCount; }

	// 現在のデス数を取得
	UFUNCTION(BlueprintPure, Category = "Sava|Death")
	int32 GetDeathCount() const { return DeathCount; }

	// キル数を1増加させる
	void AddKill();

	// デス数を1増加させる
	void AddDeath();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated)
	int32 KillCount = 0;
	int32 DeathCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Sava|Abilities")
	TObjectPtr<USavaAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<USavaAttributeSet> AttributeSet;

	UPROPERTY(Replicated)
	uint8 TeamId = NoTeam;
};

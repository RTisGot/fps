// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Loadout/SavaLoadoutTypes.h"
#include "SavaPlayerState.generated.h"

class USavaAbilitySystemComponent;
class USavaAttributeSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSavaOnPlayerLoadoutChanged, const FSavaLoadout&, Loadout);

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

	//サーバーでのみ呼ぶ。ラウンドの切り替えで、生きていた人も含めて GE(クールダウン・旗の効果など)を外し、HP を戻す
	//体を破棄した後、新しい体を出す前に呼ぶ
	void ResetForNewRound();

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

	//--------------------------------ロードアウト

	//このプレイヤーが選んだロードアウト(全員に同期する。待合室で他の人の装備を見るのにも使う)
	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	const FSavaLoadout& GetLoadout() const { return Loadout; }

	//サーバーでのみ呼ぶ。選べないものは直して持ち、今の体にもすぐ反映する
	void SetLoadout(const FSavaLoadout& NewLoadout);

	//サーバーでのみ呼ぶ。持っているロードアウトを体の武器・スキル・ガジェットに反映する(変わった枠だけ)
	void ApplyLoadoutTo(APawn* TargetPawn) const;

	//本人からロードアウトが一度でも届いたか(試合中は最初の 1 回だけ受け付けるのに使う)
	bool HasReceivedLoadout() const { return bHasReceivedLoadout; }

	//ロードアウトが変わった(サーバーでも、同期を受け取ったクライアントでも呼ばれる)
	UPROPERTY(BlueprintAssignable, Category = "Sava|Loadout")
	FSavaOnPlayerLoadoutChanged OnLoadoutChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	//Seamless Travel(待合室 → 試合)で新しいマップの PlayerState へ引き継ぐ
	virtual void CopyProperties(APlayerState* PlayerState) override;

private:
	UFUNCTION()
	void OnRep_Loadout();

	UPROPERTY(ReplicatedUsing = OnRep_Loadout)
	FSavaLoadout Loadout;

	//サーバーだけで使う
	bool bHasReceivedLoadout = false;

	UPROPERTY(Replicated)
	int32 KillCount = 0;

	UPROPERTY(Replicated)
	int32 DeathCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Sava|Abilities")
	TObjectPtr<USavaAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<USavaAttributeSet> AttributeSet;

	UPROPERTY(Replicated)
	uint8 TeamId = NoTeam;
};

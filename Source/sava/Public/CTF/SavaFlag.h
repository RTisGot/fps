// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ActiveGameplayEffectHandle.h"
#include "SavaFlag.generated.h"

class ASavaFlagBase;
class UAbilitySystemComponent;
class USphereComponent;
class UStaticMeshComponent;

//旗の状態
UENUM(BlueprintType)
enum class ESavaFlagState : uint8
{
	//旗台にある
	AtBase,
	//敵が運んでいる
	Carried,
	//運び手が死んで地面に落ちている
	Dropped,
	//持ち帰られた・デスマッチで消えた(次のラウンドまで出てこない)
	Removed,
};

//チームの旗。旗台(ASavaFlagBase)がサーバーでスポーンする
//拾う・落とす・戻す・持ち帰りの判定はすべてサーバーで行い、状態と運び手を全員に複製する
UCLASS()
class SAVA_API ASavaFlag : public AActor
{
	GENERATED_BODY()

public:
	ASavaFlag();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Sava|Flag")
	uint8 GetTeamId() const { return TeamId; }

	UFUNCTION(BlueprintPure, Category = "Sava|Flag")
	ESavaFlagState GetFlagState() const { return FlagState; }

	//運んでいるキャラクター(運ばれていなければ null)
	UFUNCTION(BlueprintPure, Category = "Sava|Flag")
	APawn* GetCarrier() const { return Carrier; }

	//以下はサーバーだけが呼ぶ
	void InitFlag(ASavaFlagBase* InHomeBase, uint8 InTeamId);
	//旗台へ戻す(運ばれていれば運び手の効果も外す)
	void ReturnToBase();
	//消す(持ち帰られた・デスマッチ)。次のラウンドで ReturnToBase されるまで出てこない
	void Remove();

protected:
	virtual void BeginPlay() override;

	//拾える範囲
	UPROPERTY(VisibleAnywhere, Category = "Sava|Flag")
	TObjectPtr<USphereComponent> PickupSphere;

	//旗竿(見た目だけ。当たり判定なし)
	UPROPERTY(VisibleAnywhere, Category = "Sava|Flag")
	TObjectPtr<UStaticMeshComponent> PoleMesh;

	//旗の布(見た目だけ。チームの色になる)
	UPROPERTY(VisibleAnywhere, Category = "Sava|Flag")
	TObjectPtr<UStaticMeshComponent> ClothMesh;

	//運び手の移動速度に掛ける倍率
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Flag", meta = (ClampMin = "0", ClampMax = "1"))
	float CarrierMoveSpeedMultiplier = 0.85f;

	//落ちてから自動で旗台へ戻るまでの秒数
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Flag", meta = (ClampMin = "0", ForceUnits = "s"))
	float DroppedReturnTime = 15.0f;

	//運ばれている間の位置(運び手のカプセルの中心から。背中の上あたり)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Flag")
	FVector CarryOffset = FVector(-40.0f, 0.0f, 20.0f);

	//チームの色(添字 = チーム番号)。布のマテリアルの "Color" パラメーターに入れる
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Flag")
	TArray<FLinearColor> TeamColors;

private:
	UPROPERTY(ReplicatedUsing = OnRep_TeamId)
	uint8 TeamId = 255;

	UPROPERTY(ReplicatedUsing = OnRep_FlagState)
	ESavaFlagState FlagState = ESavaFlagState::AtBase;

	UPROPERTY(ReplicatedUsing = OnRep_Carrier)
	TObjectPtr<APawn> Carrier;

	UFUNCTION()
	void OnRep_TeamId();
	UFUNCTION()
	void OnRep_FlagState();
	UFUNCTION()
	void OnRep_Carrier();

	//見た目を状態に合わせる(サーバー・クライアント共通)
	void UpdateVisuals();

	//以下はサーバーだけ
	void PickUp(APawn* NewCarrier);
	//運び手の位置に落とす(地面が見つからなければ旗台へ戻す)
	void Drop();
	//運び手の効果を外して、くっつきを外す
	void ReleaseCarrier();
	//触れているキャラクターを調べて、拾う・回収する
	void HandleOverlaps();
	//運び手が自陣の旗台にいれば持ち帰りを GameMode に伝える
	void CheckCapture();
	void OnDroppedTimeUp();

	UPROPERTY()
	TObjectPtr<ASavaFlagBase> HomeBase;

	//運び手に付けた効果(旗を手放すときに外す)
	TWeakObjectPtr<UAbilitySystemComponent> CarrierAbilitySystem;
	FActiveGameplayEffectHandle CarryEffectHandle;

	FTimerHandle DroppedReturnTimer;
};

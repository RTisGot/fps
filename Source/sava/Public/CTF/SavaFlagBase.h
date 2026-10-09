// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SavaFlagBase.generated.h"

class ASavaFlag;
class USphereComponent;
class UStaticMeshComponent;

//チームの旗台。レベルに各チーム 1 個置く
//サーバーで自チームの旗をスポーンし、敵の旗を持ってここに触れると持ち帰りになる
UCLASS()
class SAVA_API ASavaFlagBase : public AActor
{
	GENERATED_BODY()

public:
	ASavaFlagBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Sava|Flag")
	uint8 GetTeamId() const { return TeamId; }

	//このチームの旗(サーバーでスポーンされ、全員に複製される)
	UFUNCTION(BlueprintPure, Category = "Sava|Flag")
	ASavaFlag* GetFlag() const { return Flag; }

	//旗が台にあるときの位置
	FTransform GetFlagHomeTransform() const;

	//持ち帰りの判定範囲に入っているか
	bool IsInCaptureZone(const AActor* Actor) const;

protected:
	virtual void BeginPlay() override;

	//このチームの旗台(0 または 1)
	UPROPERTY(EditAnywhere, Category = "Sava|Flag")
	uint8 TeamId = 0;

	//スポーンする旗(見た目を変えたいときは ASavaFlag の BP を作ってここに入れる)
	UPROPERTY(EditAnywhere, Category = "Sava|Flag")
	TSubclassOf<ASavaFlag> FlagClass;

	//持ち帰りの判定範囲
	UPROPERTY(VisibleAnywhere, Category = "Sava|Flag")
	TObjectPtr<USphereComponent> CaptureZone;

	//台の見た目(当たり判定なし)
	UPROPERTY(VisibleAnywhere, Category = "Sava|Flag")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	//旗を置く高さ(台の原点から)
	UPROPERTY(EditAnywhere, Category = "Sava|Flag")
	float FlagHeight = 10.0f;

private:
	UPROPERTY(Replicated)
	TObjectPtr<ASavaFlag> Flag;
};

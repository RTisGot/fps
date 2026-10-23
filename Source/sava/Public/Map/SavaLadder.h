// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SavaLadder.generated.h"

class UArrowComponent;
class UBoxComponent;
class UStaticMeshComponent;

//マップに置く梯子。登り降りの動きは USavaCharacterMovementComponent の CMOVE_Ladder が行う
//・アクターの原点 = 梯子の下端の中央(梯子の面の上)。正面(矢印・X)= 登る人が立つ側
//・ClimbVolume の中で梯子へ向かって入力すると掴まる。上端より少し上・裏側まで伸ばしてあり、上から下を見て乗り込める
//・大きさは LadderHeight / LadderWidth で変える(アクターの拡大縮小は使わない)
UCLASS()
class SAVA_API ASavaLadder : public AActor
{
	GENERATED_BODY()

public:
	ASavaLadder();

	virtual void OnConstruction(const FTransform& Transform) override;

	//梯子の正面の向き(水平、梯子から登る人の方へ)
	UFUNCTION(BlueprintPure, Category = "Sava|Ladder")
	FVector GetLadderNormal() const;

	//梯子の下端 / 上端の高さ(ワールド)
	UFUNCTION(BlueprintPure, Category = "Sava|Ladder")
	float GetBottomZ() const { return GetActorLocation().Z; }

	UFUNCTION(BlueprintPure, Category = "Sava|Ladder")
	float GetTopZ() const { return GetActorLocation().Z + LadderHeight; }

	UBoxComponent* GetClimbVolume() const { return ClimbVolume; }

protected:
	//梯子の高さ(下端から上の足場の床まで)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Ladder", meta = (ClampMin = "50", ForceUnits = "cm"))
	float LadderHeight = 400.0f;

	//梯子の幅(掴まれる範囲の左右の幅)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Ladder", meta = (ClampMin = "10", ForceUnits = "cm"))
	float LadderWidth = 80.0f;

	//正面へどこまで近づけば掴めるか(梯子の面から)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Ladder", meta = (ClampMin = "10", ForceUnits = "cm"))
	float ClimbVolumeDepth = 100.0f;

	//裏側(壁側)へ伸ばす分。上の足場の端に立ったときに範囲へ入るように
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Ladder", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ClimbVolumeBackDepth = 40.0f;

	//上端より上に伸ばす分(上から乗り込む・頂上で上に乗るための範囲)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Ladder", meta = (ClampMin = "0", ForceUnits = "cm"))
	float TopGrabHeight = 100.0f;

	//メッシュを梯子の大きさに合わせて伸ばす(標準の板メッシュ用。自分で作ったメッシュを使うときはオフにして位置・大きさを手で合わせる)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Ladder")
	bool bFitMeshToSize = true;

	//見た目・当たり判定(標準は薄い板。Blueprint や配置後に差し替えてよい)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sava|Ladder")
	TObjectPtr<UStaticMeshComponent> Mesh;

	//この箱に入っている間、梯子へ向かって入力すると掴まる
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sava|Ladder")
	TObjectPtr<UBoxComponent> ClimbVolume;

#if WITH_EDITORONLY_DATA
	//正面の向き(エディタでの表示用)
	UPROPERTY(VisibleAnywhere, Category = "Sava|Ladder")
	TObjectPtr<UArrowComponent> Arrow;
#endif

private:
	void UpdateLayout();
};

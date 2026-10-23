// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SavaCameraEffectComponent.generated.h"

class AsavaCharacter;
class APlayerController;
class UCameraComponent;
class USavaCharacterMovementComponent;
class USavaEquipmentComponent;
struct FSavaWeaponStats;

//行きすぎない一番速いバネ(臨界減衰)。Value が 0 へ戻る(フレームの長さによらず同じ動きになる)
struct SAVA_API FSavaCameraSpring
{
	FVector Value = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	//強さ(1/秒)。大きいほど速く戻る
	float Stiffness = 0.0f;
	bool bActive = false;

	//Value が PeakTime 後に Peak まで動いてから戻るように、速さを足す(戻りきるのは PeakTime の 5 倍くらい)
	void AddKick(const FVector& Peak, float PeakTime);
	void Step(float DeltaTime);
	void Reset();
};

//1 人称カメラの演出をまとめて動かす(新しいカメラの演出もここに足す)
//・各演出の状態はここで持ち、Tick で合成してカメラ(位置・視野角・見た目のずれ)・腕・視点の向きに反映する
//  - カメラの位置: しゃがみ(撃つ位置にも使うので、全員の画面で動かす)
//  - 視野角: ADS × 動きによる倍率
//  - 見た目のずれ(回転・位置): 反動の揺れ・衝撃など。画面と腕を一緒に動かし、狙う向き(Control Rotation)は変えない
//  - 視点の向き(Control Rotation): 反動の跳ね上がり・壁走りの傾き
//・しゃがみ以外は、操作している本人の画面だけで動かす
//・武器の ADS の進み具合は装備コンポーネントから読む(装備コンポーネントの Tick の後に動く)
UCLASS(ClassGroup = (Sava), meta = (BlueprintSpawnableComponent))
class SAVA_API USavaCameraEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USavaCameraEffectComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	//--------------------------------しゃがみ(キャラクターから呼ぶ)

	//しゃがんだ・立った(OnStartCrouch / OnEndCrouch の ScaledHalfHeightAdjust)
	void NotifyStartCrouch(float ScaledHalfHeightAdjust);
	void NotifyEndCrouch(float ScaledHalfHeightAdjust);

	//目の高さ(カプセルの中心から)を変える。しゃがみの補間は止める
	void SetEyeHeight(float Height);

	//--------------------------------反動(武器から呼ぶ。自分の画面だけ)

	//1 発分の反動(X = 横、Y = 縦)で視点を動かし、見た目だけの揺れを足す。動き方は武器の数値で決まる
	void AddRecoil(const FVector2D& Kick, const FSavaWeaponStats& Stats);

	//--------------------------------状態

	//ズームなどを含まない、元の視野角
	float GetBaseFieldOfView() const { return BaseFieldOfView; }

	//設定の「画面の揺れの強さ」(0 〜 1)
	static float GetShakeScale();

protected:
	//しゃがみ切り替え時にカメラが目標の高さへ追いつく速さ
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Crouch", meta = (ClampMin = "0"))
	float CrouchCameraInterpSpeed = 12.0f;

	//壁走り中にカメラを壁と反対側へ傾ける角度(マイナスにすると壁側へ傾く)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|WallRun", meta = (ClampMin = "-45", ClampMax = "45", ForceUnits = "Deg"))
	float WallRunCameraTiltAngle = 12.0f;

	//カメラの傾きが目標の角度へ追いつく速さ
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|WallRun", meta = (ClampMin = "0"))
	float WallRunCameraTiltSpeed = 8.0f;

	//動きによる視野角の倍率が、目標へ追いつく速さ
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|MoveFOV", meta = (ClampMin = "0"))
	float MoveFOVInterpSpeed = 6.0f;

private:
	AsavaCharacter* GetCharacter() const;
	UCameraComponent* GetCamera() const;
	USavaEquipmentComponent* GetEquipment() const;
	USavaCharacterMovementComponent* GetMovement() const;
	APlayerController* GetLocalPlayerController() const;

	//カメラ本来の相対位置を、まだ覚えていなければ今の位置で覚える
	void CaptureCameraBaseLocation();

	//--------------------------------演出ごとの更新

	//しゃがみ切り替え直後のずれを 0 へ戻す
	void UpdateCrouch(float DeltaTime);
	//溜まった反動を視点に足し、足し終えて撃つのをやめていれば戻す
	void UpdateRecoil(APlayerController* PlayerController, float DeltaTime);
	//壁走り中に視点を傾ける
	void UpdateWallRunTilt(APlayerController* PlayerController, float DeltaTime);

	//今の動き方での、視野角の倍率の目標(1 = そのまま)
	float CalculateTargetMoveFOVScale() const;

	//--------------------------------合成してカメラ・腕に反映

	//カメラの位置 = 本来の位置 - しゃがみで下げる量 + 切り替え直後のずれ
	void ApplyCameraLocation();
	//視野角 = 元の視野角 × ADS のズーム × 動きによる倍率(ADS 中は弱める)
	void ApplyFieldOfView(float DeltaTime);
	//見た目のずれ(回転・位置)を合成する
	FTransform CalculateViewOffset() const;
	//見た目のずれを画面と腕に、ADS の分を腕に反映する
	void ApplyViewOffsetAndArms();

	//反動の分だけ視点を動かす(X = 横、Y = 縦)
	void AddRecoilToView(APlayerController* PlayerController, const FVector2D& Kick);
	//プレイヤーが自分で視点を下げた分は、反動で戻す量から引く(戻しすぎないように)
	void AbsorbPlayerPitchInput(float CurrentPitch);

	//--------------------------------しゃがみ

	FVector CameraBaseLocation = FVector::ZeroVector; //カメラ本来の相対位置
	bool bHasCameraBaseLocation = false;
	FVector CrouchCameraOffset = FVector::ZeroVector; //しゃがみ切り替え直後のずれ(0 へ補間する)
	float CrouchCameraDrop = 0.0f; //しゃがみ中にカメラを下げる量(カプセルが縮んだ分。目がカプセルの上に出ないように)

	//--------------------------------視野角

	float BaseFieldOfView = 0.0f;
	//動きによる視野角の倍率(目標へ滑らかに追いつく)
	float MoveFOVScale = 1.0f;

	//--------------------------------反動

	//まだ視点に足していない量(X = 横、Y = 縦)と、足しきるまでの残り時間
	FVector2D PendingRecoil = FVector2D::ZeroVector;
	float RecoilKickTimeRemaining = 0.0f;
	//まだ戻していない跳ね上がりの量
	float RecoilPitchToRecover = 0.0f;
	float LastRecoilPitch = 0.0f;
	double LastRecoilTime = -1.0e9;
	float RecoilRecoveryDelay = 0.0f;
	float RecoilRecoverySpeed = 0.0f;

	//--------------------------------見た目のずれ

	//反動の見た目だけの揺れ(X = 縦、Y = 横、Z = 傾き。度)。設定の揺れの強さは掛けない
	FSavaCameraSpring RecoilKickSpring;
	//前のフレームでカメラに見た目のずれを足したか(足していたら外す)
	bool bViewOffsetApplied = false;

	//--------------------------------腕

	//ずらしていないときの腕の位置(ずれている間は、カメラと一緒に動かす)
	FTransform ArmsBaseTransform;
	//腕を、見た目のずれ・ADS で元の位置から動かしているか(動かしていない間に、元の位置を覚え直す)
	bool bArmsModified = false;
};

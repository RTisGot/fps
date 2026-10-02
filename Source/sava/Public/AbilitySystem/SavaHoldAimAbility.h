// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/SavaGameplayAbility.h"
#include "SavaHoldAimAbility.generated.h"

struct FGameplayAbilityTargetDataHandle;
class USavaAbilityTask_Tick;

//狙い方
UENUM(BlueprintType)
enum class ESavaAimMode : uint8
{
	//視線の先の地面・壁(物を置く位置、移動先など)
	PointOnSurface,
	//放物線(投げる物)。確定時の Transform は「投げる位置と向き」になる
	ProjectileArc,
};

//「長押しで狙い(予測線・プレビューを表示)、離して確定」する能力の親クラス
//
//流れ:
//  押す   → On Aim Started(自分の画面だけ。プレビューを出す)
//  押し中 → On Aim Updated(自分の画面だけ・毎フレーム。プレビューを動かす)
//  離す   → 位置をサーバーへ送る → サーバーが確認 → On Confirmed(サーバーだけ。ここで Spawn する)
//  終了   → On Aim Ended(自分の画面だけ。プレビューを消す)
//
//キャンセル: Cancel Input Tag のボタン / Cancel On Tags Added のタグ(死亡など) / 狙った場所が無効なまま離した
//クールダウンは確定したときに始まる(キャンセルしたら消費しない)
//
//※ Blueprint では Event ActivateAbility を使わないこと(上のイベントだけを実装する)
//※ C++ で継承する場合は、各イベントの <名前>_Implementation を override する(ActivateAbility は override しない)
UCLASS(Abstract)
class SAVA_API USavaHoldAimAbility : public USavaGameplayAbility
{
	GENERATED_BODY()

public:
	USavaHoldAimAbility();

	const FGameplayTag& GetCancelInputTag() const { return CancelInputTag; }

	//狙っている最中ならキャンセルする(ASC がキャンセルボタンの入力で呼ぶ)
	void CancelAiming();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	//--------------------------------Blueprint / C++ で実装するイベント(C++ では <名前>_Implementation を override する)

	//狙い始めた(自分の画面だけ)
	UFUNCTION(BlueprintNativeEvent, Category = "Sava|Aim")
	void OnAimStarted();

	//毎フレーム(自分の画面だけ)。PathPoints は ProjectileArc のときの予測線の点
	UFUNCTION(BlueprintNativeEvent, Category = "Sava|Aim")
	void OnAimUpdated(const FTransform& AimTransform, bool bIsValid, const TArray<FVector>& PathPoints);

	//狙い終わった(自分の画面だけ)。bConfirmed = 確定したか / false ならキャンセル
	UFUNCTION(BlueprintNativeEvent, Category = "Sava|Aim")
	void OnAimEnded(bool bConfirmed);

	//確定した(サーバーだけ)。Spawn Actor はここで行う(Instigator = Get Avatar Actor From Actor Info)
	UFUNCTION(BlueprintNativeEvent, Category = "Sava|Aim")
	void OnConfirmed(const FTransform& TargetTransform);

	//独自の条件(例: 味方の近くには置けない)。自分の画面とサーバーの両方で呼ばれる
	UFUNCTION(BlueprintNativeEvent, Category = "Sava|Aim")
	bool IsTargetAllowed(const FTransform& TargetTransform) const;

	//--------------------------------設定

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Aim")
	ESavaAimMode AimMode = ESavaAimMode::PointOnSurface;

	//このボタンを押すとキャンセル(例: InputTag.Weapon.Aim)。空ならボタンでのキャンセルなし
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Aim", meta = (Categories = "InputTag"))
	FGameplayTag CancelInputTag;

	//このタグが付いたらキャンセル(既定は死亡)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Aim")
	FGameplayTagContainer CancelOnTagsAdded;

	//PointOnSurface: 狙える最大距離
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Aim|Surface", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxRange = 1500.0f;

	//PointOnSurface: 置ける面の最大の傾き(0 = 平らな床だけ、90 = 壁にも置ける)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Aim|Surface", meta = (ClampMin = "0", ClampMax = "90", ForceUnits = "deg"))
	float MaxSurfaceAngle = 40.0f;

	//ProjectileArc: 投げる速さ(投げ物の Projectile Movement の Initial Speed と合わせる)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Aim|Projectile", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float LaunchSpeed = 1500.0f;

	//ProjectileArc: 視点から見た投げる位置のずれ(前・右・上)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Aim|Projectile")
	FVector LaunchOffset = FVector(50.0f, 20.0f, -10.0f);

	//ProjectileArc: 予測線の当たり判定の太さ(投げ物の当たり判定と合わせる)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Aim|Projectile", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ProjectileRadius = 8.0f;

	//ProjectileArc: 予測線を計算する時間
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Aim|Projectile", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float MaxPredictionTime = 3.0f;

	//サーバーの確認で許す位置のずれ(通信の遅れで、サーバーから見た位置が少し違うため)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Aim", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ServerDistanceTolerance = 200.0f;

	//今の視点から狙いを計算する(OutPathPoints は ProjectileArc のときだけ)
	UFUNCTION(BlueprintCallable, Category = "Sava|Aim")
	bool ComputeAim(FTransform& OutTransform, TArray<FVector>& OutPathPoints) const;

private:
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	UFUNCTION()
	void OnCancelTagAdded();

	void OnAimTick(float DeltaTime);
	void OnServerTargetDataReceived(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ApplicationTag);

	//確定・キャンセル時の共通処理
	void FinishAiming(bool bConfirmed);
	void CancelThisAbility();

	bool GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
	bool IsTargetValid(const FTransform& TargetTransform) const;

	bool bIsAiming = false;
	bool bListeningForServerTargetData = false;
};

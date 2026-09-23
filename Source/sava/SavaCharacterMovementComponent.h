#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SavaCharacterMovementComponent.generated.h"

// MOVE_Custom の中で使う独自の移動モード（CustomMovementMode に入る値）
UENUM(BlueprintType)
enum class ESavaMovementMode : uint8
{
	None  UMETA(Hidden),
	Slide UMETA(DisplayName = "Slide"),
};

/**
 * ダッシュ・しゃがみ・スライディングを持つ移動コンポーネント。
 *
 * 入力の状態（ダッシュ中か）は SavedMove に含めてサーバーへ送るため、
 * クライアント予測とサーバーの結果が一致し、ラバーバンドが起きない。
 * しゃがみはエンジン標準の bWantsToCrouch をそのまま使う。
 */
UCLASS()
class USavaCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	USavaCharacterMovementComponent();

	// ---- ダッシュ ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Sprint", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxSprintSpeed = 700.f;

	// 入力方向と前方向の内積がこれ以上ならダッシュできる（0.5 = 前方±60°）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Sprint", meta = (ClampMin = "0", ClampMax = "1"))
	float SprintForwardThreshold = 0.5f;

	// ---- スライディング ----

	// この速度以上でしゃがむとスライディングになる（歩き速度より大きくすること）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SlideMinStartSpeed = 600.f;

	// 開始時に加算する速度
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SlideEnterBoost = 250.f;

	// 開始時の加算で到達できる上限（連続スライディングで無限に加速しないため）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SlideMaxStartSpeed = 1000.f;

	// この速度を下回るとスライディング終了（しゃがみ歩きへ）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SlideExitSpeed = 300.f;

	// 平地での減速（cm/s^2）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0"))
	float SlideDeceleration = 600.f;

	// 坂道で重力がどれだけ効くか（下り坂で加速、上り坂で減速）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0"))
	float SlideGravityScale = 1.f;

	// 左右入力で曲がる強さ（cm/s^2）。速度は増えない
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0"))
	float SlideSteerAcceleration = 500.f;

	UFUNCTION(BlueprintCallable, Category = "Sava|Sprint")
	void SetWantsToSprint(bool bNewWantsToSprint) { bWantsToSprint = bNewWantsToSprint; }

	bool WantsToSprint() const { return bWantsToSprint; }

	UFUNCTION(BlueprintPure, Category = "Sava|Sprint")
	bool IsSprinting() const;

	UFUNCTION(BlueprintPure, Category = "Sava|Slide")
	bool IsSliding() const;

	virtual float GetMaxSpeed() const override;
	virtual bool IsMovingOnGround() const override;
	virtual bool CanAttemptJump() const override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

private:
	bool CanStartSlide() const;
	void StartSlide();
	void PhysSlide(float DeltaTime, int32 Iterations);

	// ダッシュキーを押しているか。FLAG_Custom_0 でサーバーへ送る
	bool bWantsToSprint = false;
};

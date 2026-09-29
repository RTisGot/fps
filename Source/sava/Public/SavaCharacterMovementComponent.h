// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SavaCharacterMovementComponent.generated.h"

//MOVE_Custom のサブモード(今後の独自移動はここに追加する)
enum ESavaCustomMovementMode : uint8
{
	CMOVE_None = 0,
	CMOVE_Slide,
	CMOVE_WallRun,
};

//サーバーが位置を補正するときに、独自の移動状態も一緒に送るためのデータ
struct FSavaCharacterMoveResponseDataContainer : FCharacterMoveResponseDataContainer
{
	using Super = FCharacterMoveResponseDataContainer;

	float SlideBoostCooldownRemaining = 0.0f;
	FVector WallRunNormal = FVector::ZeroVector;
	float WallRunElapsed = 0.0f;
	FVector LastWallRunNormal = FVector::ZeroVector;
	int32 WallJumpCountSinceLanded = 0;

	virtual void ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap) override;
};

UCLASS()
class SAVA_API USavaCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	friend struct FSavaCharacterMoveResponseDataContainer;

	class FSavedMove_Sava : public FSavedMove_Character
	{
	public:
		typedef FSavedMove_Character Super;

		uint8 bSavedWantsToSprint : 1;

		virtual void Clear() override;
		virtual uint8 GetCompressedFlags() const override;
		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;
		virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
		virtual void PrepMoveFor(class ACharacter* Character) override;
	};


	class FNetworkPredictionData_Client_Sava : public FNetworkPredictionData_Client_Character
	{
	public:
		typedef FNetworkPredictionData_Client_Character Super;
		FNetworkPredictionData_Client_Sava(const UCharacterMovementComponent& ClientMovement);
		virtual FSavedMovePtr AllocateNewMove() override;
	};

public:
	//ダッシュ時の速度倍率(歩き速度 MaxWalkSpeed に掛ける)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Sprint", meta = (ClampMin = "1"))
		float SprintSpeedMultiplier = 1.35f;

	//前からこれだけ反れても走れる角度
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Sprint", meta = (ClampMin = "0", ClampMax = "90", ForceUnits = "Deg"))
		float SprintMaxAngle = 50.0f;

	UFUNCTION(BlueprintCallable,Category = "Sava|Sprint")
	void StartSprint() { bWantsToSprint = true; }
	UFUNCTION(BlueprintCallable, Category = "Sava|Sprint")
	void StopSprint() { bWantsToSprint = false; }

	UFUNCTION(BlueprintPure, Category = "Sava|Sprint")
	bool IsSprinting() const;

	//GAS の移動速度倍率(スキル・ガジェットによる加速/減速)。ASC がなければ 1.0
	UFUNCTION(BlueprintPure, Category = "Sava|Abilities")
	float GetAbilityMoveSpeedMultiplier() const;

	USavaCharacterMovementComponent();

	//スライディング開始に必要な水平速度(ダッシュが１)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SlideMinStartSpeedRate = 0.9f;

	//これより遅くなったらスライディング終了
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SlideMinSpeed = 250.0f;

	//開始時に進行方向へ加える速度
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SlideEnterImpulse = 300.0f;

	//スライディング中の最高速度(下り坂での加速の上限)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SlideMaxSpeed = 1500.0f;

	//速度に比例する減速(大きいほど早く止まる)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0"))
	float SlideFriction = 0.6f;

	//速度に関係なく一定の減速
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float SlideBrakingDeceleration = 200.0f;

	//坂道で重力の影響を受ける強さ(1=そのまま, 0=坂の影響なし)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0"))
	float SlideGravityScale = 1.0f;

	//左右入力で進行方向を曲げる強さ
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float SlideSteerAcceleration = 400.0f;

	//開始時のブースト(SlideEnterImpulse)が再び出るまでの時間。クールダウン中もスライディング自体はできる
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "s"))
	float SlideBoostCooldown = 2.0f;

	//しゃがみボタンを押したまま十分な速度で着地したら、そのままスライディングに移る
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide")
	bool bSlideOnLanding = true;

	UFUNCTION(BlueprintPure, Category = "Sava|Slide")
	bool IsSliding() const;

	//ブーストのクールダウン残り時間(UI表示用)
	UFUNCTION(BlueprintPure, Category = "Sava|Slide")
	float GetSlideBoostCooldownRemaining() const { return SlideBoostCooldownRemaining; }

	//空中で減速し始める水平速度 = 歩き速度 MaxWalkSpeed × この倍率
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Air", meta = (ClampMin = "1"))
	float AirSpeedSoftCapMultiplier = 1.35f;

	//空中で基準速度を超えた分が減る速さ(1秒あたり。0.5 なら約1.4秒で超過分が半分になる)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Air", meta = (ClampMin = "0"))
	float AirOverspeedDecayRate = 0.5f;

	//壁走りを始められる最低の水平速度
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float WallRunMinSpeed = 600.0f;

	//壁走り中に近づいていく水平速度 = 歩き速度 MaxWalkSpeed × この倍率(超えている分は空中と同じ割合で減る)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0"))
	float WallRunSpeedMultiplier = 1.35f;

	//壁走り中、目標速度より遅いときの加速
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float WallRunAcceleration = 1000.0f;

	//壁走り中、下降しているときの重力の強さ(1=通常, 0=落ちない)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0"))
	float WallRunGravityScale = 0.25f;

	//壁走り中、上昇しているときの重力の強さ(1=通常のジャンプと同じ弧。下げると壁を登れるようになる)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0"))
	float WallRunRisingGravityScale = 1.0f;

	//壁走りを続けられる最大時間
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0", ForceUnits = "s"))
	float WallRunMaxDuration = 1.75f;

	//張り付いた瞬間に、それまでの上下の速度をどれだけ残すか(1=そのまま / 0=上下の動きを止めて壁に吸い付く)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0", ClampMax = "1"))
	float WallRunEntryVerticalScale = 1.0f;

	//開始時の上向き速度の上限(上昇中に張り付いても飛び上がりすぎないように)。ジャンプの勢いを活かすなら JumpZVelocity 以上にする
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float WallRunMaxEntryUpSpeed = 420.0f;

	//これより速く落下中は壁に張り付けない
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float WallRunMaxEntryFallSpeed = 800.0f;

	//足元からこの高さ以内に床があると壁走りを始めない(地上すれすれでの誤発動を防ぐ)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0", ForceUnits = "cm"))
	float WallRunMinHeight = 80.0f;

	//カプセルの表面から壁までの、壁として検出する距離
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0", ForceUnits = "cm"))
	float WallRunCheckDistance = 30.0f;

	//壁とみなす面の傾き(法線の上下成分がこれ以下なら壁。0=完全な垂直のみ)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0", ClampMax = "1"))
	float WallRunMaxWallNormalZ = 0.3f;

	//ウォールジャンプで壁から離れる方向の速度
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float WallJumpOffSpeed = 600.0f;

	//壁走り中、壁から離れないように壁へ押し付ける速さ(大きいほど壁に吸い付く)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float WallRunStickSpeed = 200.0f;

	//壁から離れる入力の判定。入力が壁の外向きにこの値以上向いていたら壁から離れる
	//(0=少しでも外に入れたら離れる / 0.7=ほぼ真横に入れないと離れない / 1=離れない)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0", ClampMax = "1"))
	float WallRunDetachInputDot = 0.7f;

	//ウォールジャンプの上向き速度(着地後の1回目)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float WallJumpUpSpeed = 500.0f;

	//着地する前の2回目以降のウォールジャンプの上向き速度(0=真横に飛ぶ)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float WallJumpRepeatUpSpeed = 0.0f;

	UFUNCTION(BlueprintPure, Category = "Sava|WallRun")
	bool IsWallRunning() const;

	//壁走り中の壁の向き(壁から外向き)。カメラの傾きやアニメーション用。自分とサーバーでのみ有効
	UFUNCTION(BlueprintPure, Category = "Sava|WallRun")
	FVector GetWallRunNormal() const { return WallRunNormal; }

	virtual bool IsMovingOnGround() const override;
	virtual bool CanAttemptJump() const override;
	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;
	virtual float GetMaxSpeed() const override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse) override;

protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void PhysFalling(float DeltaTime, int32 Iterations) override;
	virtual void SetPostLandedPhysics(const FHitResult& Hit) override;

private:
	void EnterSlide();
	void PhysSlide(float DeltaTime, int32 Iterations);
	void ApplyAirOverspeedDecay(float DeltaTime);
	float ApplyOverspeedDecay(float Speed, float SoftCap, float DeltaTime) const;

	bool TryStartWallRun();
	bool TraceMovementLine(const FVector& Start, const FVector& End, FHitResult& OutHit) const;
	bool FindWallRunWall(const FVector& Direction, FHitResult& OutHit) const;
	bool IsValidWallRunWall(const FHitResult& Hit) const;
	bool HasWallRunInput(const FVector& RunDirection, const FVector& Normal) const;
	void ExitWallRun();
	void PhysWallRun(float DeltaTime, int32 Iterations);

	bool bWantsToSprint = false;

	//移動の計算の中で減らす(Timer は使わない)。補正時はサーバーの値に合わせる
	float SlideBoostCooldownRemaining = 0.0f;

	//壁走り中の壁の向き(水平、壁から外向き)
	FVector WallRunNormal = FVector::ZeroVector;

	//壁走りを始めてからの時間
	float WallRunElapsed = 0.0f;

	//最後に走った壁の向き。着地するまで同じ壁には張り付けない(ゼロなら制限なし)
	FVector LastWallRunNormal = FVector::ZeroVector;

	//着地してからのウォールジャンプの回数
	int32 WallJumpCountSinceLanded = 0;

	FSavaCharacterMoveResponseDataContainer SavaMoveResponseDataContainer;
};

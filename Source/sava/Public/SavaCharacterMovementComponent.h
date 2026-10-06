// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SavaCharacterMovementComponent.generated.h"

//MOVE_Custom のサブモード(今後の独自移動はここに追加する)。Blueprint でも名前で判定できる
UENUM(BlueprintType)
enum ESavaCustomMovementMode : uint8
{
	CMOVE_None = 0	UMETA(DisplayName = "None"),
	CMOVE_Slide		UMETA(DisplayName = "Slide"),
	CMOVE_WallRun	UMETA(DisplayName = "Wall Run"),
	CMOVE_Mantle	UMETA(DisplayName = "Mantle"),
	CMOVE_WallPerch	UMETA(DisplayName = "Wall Perch"),
	CMOVE_MAX		UMETA(Hidden),
};

//独自移動の切り替わりを Blueprint に知らせるイベント(音・エフェクト・アニメーション用)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSavaCustomMovementModeChangedSignature, TEnumAsByte<ESavaCustomMovementMode>, PreviousMode, TEnumAsByte<ESavaCustomMovementMode>, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSavaMovementEventSignature);

//サーバーが位置を補正するときに、独自の移動状態も一緒に送るためのデータ
struct FSavaCharacterMoveResponseDataContainer : FCharacterMoveResponseDataContainer
{
	using Super = FCharacterMoveResponseDataContainer;

	float SlideBoostCooldownRemaining = 0.0f;
	FVector WallRunNormal = FVector::ZeroVector;
	float WallRunElapsed = 0.0f;
	FVector LastWallRunNormal = FVector::ZeroVector;
	float WallTapElapsed = 0.0f;
	bool bWallRunOnSameWall = false;
	float WallJumpDecay = 0.0f;
	float WallRunSameWallCooldownRemaining = 0.0f;
	float WallRunStartDelayRemaining = 0.0f;
	float WallJumpLateReleaseRemaining = 0.0f;
	bool bWallRunLockedUntilLanded = false;
	float JustLandingInputRemaining = 0.0f;
	float JustLandingCooldownRemaining = 0.0f;
	bool bPrevWantsToCrouch = false;
	float WallPerchElapsed = 0.0f;
	float LurchTimeRemaining = 0.0f;
	float LurchAngleRemaining = 0.0f;
	FVector LurchInputDir = FVector::ZeroVector;
	float CrestUpSpeed = 0.0f;
	float CrestTimeRemaining = 0.0f;
	FVector MantleStartLocation = FVector::ZeroVector;
	FVector MantleTargetLocation = FVector::ZeroVector;
	FVector MantleExitDirection = FVector::ZeroVector;
	float MantleElapsed = 0.0f;
	float MantleDuration = 0.0f;

	virtual void ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap) override;
};

UCLASS()
class SAVA_API USavaCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	friend struct FSavaCharacterMoveResponseDataContainer;

	//独自の移動状態(タイマー・壁走り・よじ登りなど)の一括コピー。
	//移動の結合(CombineWith)で位置と速度を巻き戻すときに、これも一緒に巻き戻さないとクライアントだけタイマーが二重に進む
	struct FSavaCustomMoveState
	{
		float SlideBoostCooldownRemaining = 0.0f;
		FVector WallRunNormal = FVector::ZeroVector;
		float WallRunElapsed = 0.0f;
		FVector LastWallRunNormal = FVector::ZeroVector;
		float WallTapElapsed = 0.0f;
		bool bWallRunOnSameWall = false;
		float WallJumpDecay = 0.0f;
		float WallRunSameWallCooldownRemaining = 0.0f;
		float WallRunStartDelayRemaining = 0.0f;
		float WallJumpLateReleaseRemaining = 0.0f;
		bool bWallRunLockedUntilLanded = false;
		float JustLandingInputRemaining = 0.0f;
		float JustLandingCooldownRemaining = 0.0f;
		bool bPrevWantsToCrouch = false;
		float WallPerchElapsed = 0.0f;
		float LurchTimeRemaining = 0.0f;
		float LurchAngleRemaining = 0.0f;
		FVector LurchInputDir = FVector::ZeroVector;
		float CrestUpSpeed = 0.0f;
		float CrestTimeRemaining = 0.0f;
		FVector MantleStartLocation = FVector::ZeroVector;
		FVector MantleTargetLocation = FVector::ZeroVector;
		FVector MantleExitDirection = FVector::ZeroVector;
		float MantleElapsed = 0.0f;
		float MantleDuration = 0.0f;
	};

	class FSavedMove_Sava : public FSavedMove_Character
	{
	public:
		typedef FSavedMove_Character Super;

		uint8 bSavedWantsToSprint : 1;
		uint8 bSavedJumpHeld : 1;

		//この移動を始める時点の独自状態(結合で巻き戻すときに使う)
		FSavaCustomMoveState StartCustomState;

		virtual void Clear() override;
		virtual void SetInitialPosition(ACharacter* Character) override;
		virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter, APlayerController* PC, const FVector& OldStartLocation) override;
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

	//ジャンプボタンを押している間は壁走りでき、離すとウォールジャンプする(キャラクターの入力から呼ぶ)
	UFUNCTION(BlueprintCallable, Category = "Sava|WallRun")
	void SetJumpHeld(bool bHeld) { bJumpHeld = bHeld; }

	// Already saved and replicated through FLAG_Custom_1 (also used by grapple lift).
	bool IsJumpHeld() const { return bJumpHeld; }

	//水平の速度の絶対上限(どの移動・テクニックでもこれを超えない。スキルの倍率も掛からない)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|SpeedCap", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float AbsoluteMaxHorizontalSpeed = 2000.0f;

	//GAS の移動速度倍率(スキル・ガジェットによる加速/減速)。ASC がなければ 1.0
	UFUNCTION(BlueprintPure, Category = "Sava|Abilities")
	float GetAbilityMoveSpeedMultiplier() const;

	USavaCharacterMovementComponent();

	//独自の移動(スライディング・壁走り・よじ登り・張り付き)を計算するときの最大の刻み幅。
	//これより長いフレームは分割して計算する(フレームレートが低くても同じ動きになる)。1/60 より短いフレームは変わらない
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Network", meta = (ClampMin = "0.001", ForceUnits = "s"))
	float CustomPhysicsMaxStep = 1.0f / 60.0f;

	//スライディング開始に必要な水平速度 = 歩きの速度 MaxWalkSpeed × この倍率(スキルの速度倍率も掛かる)
	//1 より少し大きくして、歩いている(ちょうど歩きの最高速度)ときはしゃがみになるようにする
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0"))
	float SlideMinStartWalkSpeedRate = 1.05f;

	//これより遅くなったらスライディング終了
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SlideMinSpeed = 350.0f;

	//開始時に進行方向へ加える速度
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SlideEnterImpulse = 700.0f;

	//スライディング中の最高速度(下り坂での加速の上限)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SlideMaxSpeed = 2000.0f;

	//速度に比例する減速(大きいほど早く止まる)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0"))
	float SlideFriction = 0.8f;

	//速度に関係なく一定の減速
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float SlideBrakingDeceleration = 200.0f;

	//坂道で重力の影響を受ける強さ(1=そのまま, 0=坂の影響なし)。上り坂で使う
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0"))
	float SlideGravityScale = 1.0f;

	//下り坂で重力の影響を受ける強さ(大きいほど下り坂で加速する)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0"))
	float SlideDownhillGravityScale = 1.5f;

	//下り坂がこの角度に近づくほど摩擦が弱まり、この角度以上で SlideDownhillFrictionScale 倍になる
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ClampMax = "89", ForceUnits = "Deg"))
	float SlideDownhillFullAngle = 10.0f;

	//急な下り坂での摩擦(SlideFriction と SlideBrakingDeceleration)の倍率(0 = 摩擦なしで加速し続ける)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ClampMax = "1"))
	float SlideDownhillFrictionScale = 0.0f;

	//左右入力で進行方向を曲げる強さ
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float SlideSteerAcceleration = 400.0f;

	//開始時のブースト(SlideEnterImpulse)が再び出るまでの時間。クールダウン中もスライディング自体はできる
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "s"))
	float SlideBoostCooldown = 2.0f;

	//しゃがみボタンを押したまま十分な速度で着地したら、そのままスライディングに移る
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide")
	bool bSlideOnLanding = true;

	//----- ジャストランディング: 空中でしゃがみを押してすぐ着地すると、スライディングとは別のブーストが出る -----
	//(使えるならスライディングのブーストより優先し、スライディングのブーストは温存する。1回の着地で2つ同時には出ない)

	//しゃがみを押してから着地までがこの時間以内なら成功
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|JustLanding", meta = (ClampMin = "0", ForceUnits = "s"))
	float JustLandingWindow = 0.15f;

	//ジャストランディングのブースト(進行方向へ加える速度)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|JustLanding", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float JustLandingBoostSpeed = 700.0f;

	//ジャストランディングのブーストが再び出るまでの時間(スライディングのブーストとは別に数える)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|JustLanding", meta = (ClampMin = "0", ForceUnits = "s"))
	float JustLandingCooldown = 5.0f;

	//ジャストランディングのクールダウン残り時間(UI表示用)
	UFUNCTION(BlueprintPure, Category = "Sava|JustLanding")
	float GetJustLandingCooldownRemaining() const { return JustLandingCooldownRemaining; }

	UFUNCTION(BlueprintPure, Category = "Sava|Slide")
	bool IsSliding() const;

	//ブーストのクールダウン残り時間(UI表示用)
	UFUNCTION(BlueprintPure, Category = "Sava|Slide")
	float GetSlideBoostCooldownRemaining() const { return SlideBoostCooldownRemaining; }

	//空中で減速し始める水平速度 = 歩き速度 MaxWalkSpeed × この倍率(スキルの速度倍率も掛かる)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Air", meta = (ClampMin = "1"))
	float AirSpeedSoftCapMultiplier = 1.35f;

	//空中で基準速度を超えた分が減る速さ(1秒あたり。0.5 なら約1.4秒で超過分が半分になる)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Air", meta = (ClampMin = "0"))
	float AirOverspeedDecayRate = 0.7f;

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

	//壁走りを始めるとき、進行方向と視線の角度がこれ以内であること(90=真横まで。後ろ向きでは張り付かない)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "Deg"))
	float WallRunMaxEntryLookAngle = 90.0f;

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

	//地上からジャンプした直後、この時間は壁走りを始めない(壁際で普通に跳びたいときに、長押しで壁走りにならないように)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0", ForceUnits = "s"))
	float WallRunAfterGroundJumpDelay = 0.1f;

	//壁走りが途切れて(壁の終わり・時間切れ・離れる入力など)から、この時間内にジャンプを離せばウォールジャンプになる
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0", ForceUnits = "s"))
	float WallJumpLateReleaseTime = 0.1f;

	//壁から離れてから、同じ壁にまた張り付けるようになるまでの時間
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "0", ForceUnits = "s"))
	float WallRunSameWallCooldown = 0.15f;

	//ウォールジャンプの上向き速度(着地後の1回目)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ForceUnits = "cm/s"))
	float WallJumpUpSpeed = 500.0f;

	//繰り返したウォールジャンプの上向き速度の下限(マイナス = 水平より下向き。-100 で約10度下)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ForceUnits = "cm/s"))
	float WallJumpMinUpSpeed = -100.0f;

	//別の壁でのウォールジャンプ何回で、上向き速度が下限まで下がるか
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "1"))
	float WallJumpAngleDecaySteps = 4.0f;

	//同じ壁で繰り返したときは、別の壁の何回分として数えるか(2 なら2倍の速さで角度が下がる)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallRun", meta = (ClampMin = "1"))
	float WallJumpSameWallDecayScale = 2.0f;

	//次のウォールジャンプの上向き速度(UI・デバッグ表示用)
	UFUNCTION(BlueprintPure, Category = "Sava|WallRun")
	float GetNextWallJumpUpSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Sava|WallRun")
	bool IsWallRunning() const;

	//壁走り中の壁の向き(壁から外向き)。カメラの傾きやアニメーション用。自分とサーバーでのみ有効
	UFUNCTION(BlueprintPure, Category = "Sava|WallRun")
	FVector GetWallRunNormal() const { return WallRunNormal; }

	//壁走り中の進行方向(水平)。壁走りしていなければゼロ。自分とサーバーでのみ有効
	UFUNCTION(BlueprintPure, Category = "Sava|WallRun")
	FVector GetWallRunDirection() const;

	//----- ウォールタップ: 壁に張り付いてからすぐにウォールジャンプすると、壁沿いの速度が上乗せされる -----
	//(同じ壁に張り付き直したときは上乗せなし。減速しない効果だけ残る)

	//ボーナスが出る時間(張り付いた直後が最大で、この時間で 0 になる)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallTap", meta = (ClampMin = "0", ForceUnits = "s"))
	float WallTapWindow = 0.25f;

	//張り付いた直後にジャンプしたときの、壁沿いの速度の上乗せ(最大値)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallTap", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float WallTapBonusSpeed = 250.0f;

	//普通のウォールジャンプで失う壁沿いの速度の割合(0.15 なら 15%)。ウォールタップが早いほど減り、張り付いた直後なら 0
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallTap", meta = (ClampMin = "0", ClampMax = "1"))
	float WallJumpSpeedLoss = 0.15f;

	//普通の地上ジャンプで失う水平の速度の割合。エッジジャンプ・クレストジャンプでは失わない
	//(スライディングを始められる速度より下には減らさない。歩きのジャンプは変わらず、着地スライディングの条件も満たせる)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|EdgeJump", meta = (ClampMin = "0", ClampMax = "1"))
	float JumpSpeedLoss = 0.12f;

	//----- ウォールパーチ: 壁走り中にしゃがみを押すと、壁に張り付いて止まる -----
	//(ジャンプを離すとウォールジャンプ、しゃがみを離すと落ちる)

	//張り付いていられる最大時間(過ぎると落ちる)。張り付いた後は、着地するまで壁走り・張り付きができない
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|WallPerch", meta = (ClampMin = "0", ForceUnits = "s"))
	float WallPerchMaxDuration = 2.0f;

	UFUNCTION(BlueprintPure, Category = "Sava|WallPerch")
	bool IsWallPerching() const;

	//張り付いていられる残り時間(UI表示用)
	UFUNCTION(BlueprintPure, Category = "Sava|WallPerch")
	float GetWallPerchTimeRemaining() const;

	//----- ラーチ: ジャンプ直後の短い時間だけ、新しく入れた方向へ大きく曲がれる -----

	//ジャンプしてから曲がれる時間
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Lurch", meta = (ClampMin = "0", ForceUnits = "s"))
	float LurchWindow = 0.2f;

	//1回のジャンプで曲がれる角度の合計
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Lurch", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "Deg"))
	float LurchMaxAngle = 90.0f;

	//曲がる速さ(1秒あたりの角度。大きいほど一瞬で曲がる)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Lurch", meta = (ClampMin = "0"))
	float LurchTurnRate = 720.0f;

	//LurchMaxAngle まで曲がったときに失う速度の割合(0.15 なら 15% 遅くなる。曲がった角度に比例)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Lurch", meta = (ClampMin = "0", ClampMax = "1"))
	float LurchSpeedLossAtMaxAngle = 0.15f;

	//ジャンプした瞬間の入力からこれ以上向きを変えた入力だけが「新しい入力」として曲がる
	//(ジャンプ前から入れっぱなしの方向へは勝手に曲がらない)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Lurch", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "Deg"))
	float LurchInputChangeAngle = 20.0f;

	//----- エッジジャンプ: 足場から落ちる直前にジャンプすると、水平の速度が少し上乗せされる -----

	//体全体が足場から出るまでの残り時間がこれ以内なら成功(速度から計算する。小さいほど厳しい)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|EdgeJump", meta = (ClampMin = "0", ForceUnits = "s"))
	float EdgeJumpWindow = 0.1f;

	//水平の速度の上乗せ
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|EdgeJump", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float EdgeJumpBonusSpeed = 150.0f;

	//これより遅いと発動しない(端に立ち止まってのジャンプは対象外)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|EdgeJump", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float EdgeJumpMinSpeed = 500.0f;

	//「端」とみなす段差の深さ(これより浅い下り坂や段差は端ではない)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|EdgeJump", meta = (ClampMin = "0", ForceUnits = "cm"))
	float EdgeJumpMinDropHeight = 100.0f;

	//----- クレストジャンプ: スライディングで坂を上り、頂上でジャンプすると上向きの勢いがジャンプに足される -----

	//これより緩い坂では発動しない
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|CrestJump", meta = (ClampMin = "0", ClampMax = "89", ForceUnits = "Deg"))
	float CrestJumpMinSlopeAngle = 10.0f;

	//頂上を越えた直後もこの時間内ならクレストジャンプになる(頂上の手前だけでは受付が狭すぎるため)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|CrestJump", meta = (ClampMin = "0", ForceUnits = "s"))
	float CrestJumpGraceTime = 0.15f;

	//頂上かどうかを調べる前方の距離(この先で坂が終わっていれば頂上)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|CrestJump", meta = (ClampMin = "0", ForceUnits = "cm"))
	float CrestJumpCheckDistance = 100.0f;

	//坂がそのまま続いているとみなす高さの誤差
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|CrestJump", meta = (ClampMin = "0", ForceUnits = "cm"))
	float CrestJumpHeightTolerance = 10.0f;

	//坂を上る上向きの速度を、ジャンプにどれだけ足すか(1 = そのまま足す)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|CrestJump", meta = (ClampMin = "0"))
	float CrestJumpUpSpeedScale = 1.0f;

	//足す上向きの速度の上限
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|CrestJump", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float CrestJumpMaxBonusUpSpeed = 600.0f;

	//よじ登れる縁の高さの上限(足元から縁の上面まで)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Mantle", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MantleMaxHeight = 160.0f;

	//よじ登る縁の高さの下限(これより低い段差は普通に乗り越えられるので対象外)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Mantle", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MantleMinHeight = 45.0f;

	//カプセルの表面から前方の壁までの、よじ登りを始める距離
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Mantle", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MantleCheckDistance = 50.0f;

	//入力と視線が、壁の方向からこれ以内であること
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Mantle", meta = (ClampMin = "0", ClampMax = "90", ForceUnits = "Deg"))
	float MantleMaxAngle = 50.0f;

	//低い縁 / 高い縁をよじ登るのにかかる時間(高さに応じて間の値になる)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Mantle", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float MantleMinDuration = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Mantle", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float MantleMaxDuration = 0.6f;

	//よじ登り終えたときの前向きの速度 = 歩き速度 MaxWalkSpeed × この倍率
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Mantle", meta = (ClampMin = "0"))
	float MantleExitSpeedMultiplier = 0.8f;

	UFUNCTION(BlueprintPure, Category = "Sava|Mantle")
	bool IsMantling() const;

	//今の独自移動の種類(AnimBP 用)。他のプレイヤーでも有効
	UFUNCTION(BlueprintPure, Category = "Sava")
	TEnumAsByte<ESavaCustomMovementMode> GetCustomMovementModeType() const;

	//----- Blueprint 用のイベント -----
	//自分・サーバー・他のプレイヤーのすべての画面で呼ばれる(ウォールジャンプだけは例外)。
	//音・エフェクト・アニメーション専用。ダメージなどゲームの判定には使わないこと

	//独自移動(スライディング・壁走り・よじ登り・壁への張り付き)が切り替わったとき
	UPROPERTY(BlueprintAssignable, Category = "Sava|Events")
	FSavaCustomMovementModeChangedSignature OnCustomMovementModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Sava|Events")
	FSavaMovementEventSignature OnSlideStarted;

	UPROPERTY(BlueprintAssignable, Category = "Sava|Events")
	FSavaMovementEventSignature OnSlideEnded;

	//壁の左右はキャラクターの Get Wall Run Side で取れる(他のプレイヤーでは開始直後だけ 0 の場合がある)
	UPROPERTY(BlueprintAssignable, Category = "Sava|Events")
	FSavaMovementEventSignature OnWallRunStarted;

	UPROPERTY(BlueprintAssignable, Category = "Sava|Events")
	FSavaMovementEventSignature OnWallRunEnded;

	//ウォールジャンプしたとき(OnWallRunEnded / OnWallPerchEnded の直後。壁走りが途切れた直後の猶予中なら単独で)。自分とサーバーでだけ呼ばれる
	UPROPERTY(BlueprintAssignable, Category = "Sava|Events")
	FSavaMovementEventSignature OnWallJumped;

	//壁に張り付いたとき(目立つ音やエフェクトを出して、相手に気づけるようにする)
	UPROPERTY(BlueprintAssignable, Category = "Sava|Events")
	FSavaMovementEventSignature OnWallPerchStarted;

	UPROPERTY(BlueprintAssignable, Category = "Sava|Events")
	FSavaMovementEventSignature OnWallPerchEnded;

	UPROPERTY(BlueprintAssignable, Category = "Sava|Events")
	FSavaMovementEventSignature OnMantleStarted;

	UPROPERTY(BlueprintAssignable, Category = "Sava|Events")
	FSavaMovementEventSignature OnMantleEnded;

	//----- デバッグ表示(一時的): テクニックの成立と移動の状態を画面左上に文字で出す -----

	//オフにすると表示しない(開発用ビルドだけで有効。製品版では常に表示されない)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sava|Debug")
	bool bShowTechniqueDebug = true;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

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
	virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

private:
	void EnterSlide(bool bJustLanding = false);
	bool HasSlideStartSpeed() const;
	void PhysSlide(float DeltaTime, int32 Iterations);
	void ApplyAirOverspeedDecay(float DeltaTime);
	float ApplyOverspeedDecay(float Speed, float SoftCap, float DeltaTime) const;

	bool TryStartWallRun();
	bool IsSameWallAsLast(const FVector& Normal) const;
	bool TraceMovementLine(const FVector& Start, const FVector& End, FHitResult& OutHit) const;
	bool FindWallRunWall(const FVector& Direction, FHitResult& OutHit) const;
	bool IsValidWallRunWall(const FHitResult& Hit) const;
	bool HasWallRunInput(const FVector& RunDirection, const FVector& Normal) const;
	void ExitWallRun();
	void PhysWallRun(float DeltaTime, int32 Iterations);
	void DoWallJump();

	void EnterWallPerch();
	bool IsWallRunLandingImminent(float TimeAhead) const;
	void PhysWallPerch(float DeltaTime, int32 Iterations);

	void StartLurch();
	void ApplyLurch(float DeltaTime);
	void TurnTowardLurchInput(float DeltaTime);
	void EndLurch();

	void ShowTechniqueDebug(const FString& Text, const FColor& Color) const;

	FSavaCustomMoveState CaptureCustomMoveState() const;
	void RestoreCustomMoveState(const FSavaCustomMoveState& State);

	bool IsEdgeJumpAvailable(float LookAheadTime) const;
	bool IsCrestJumpAvailable(FString* OutFailReason = nullptr) const;
	float GetSlopeTanAlongVelocity() const;

	bool TryStartMantle();
	bool IsCapsuleSweepClear(const FVector& Start, const FVector& End) const;
	void PhysMantle(float DeltaTime, int32 Iterations);

	bool bWantsToSprint = false;

	//ジャンプボタンを押しているか(押している間は壁走り、離すとウォールジャンプ)。ダッシュと同じく入力としてサーバーへ送る
	bool bJumpHeld = false;

	//このフレームでウォールジャンプした(壁走り終了のイベントと一緒に知らせる)
	bool bWallJumpedThisMove = false;

	//移動の計算の中で減らす(Timer は使わない)。補正時はサーバーの値に合わせる
	float SlideBoostCooldownRemaining = 0.0f;

	//壁走り中の壁の向き(水平、壁から外向き)
	FVector WallRunNormal = FVector::ZeroVector;

	//今の壁で壁走りした時間(同じ壁に張り付き直したときは引き継ぐ。WallRunMaxDuration と比べる)
	float WallRunElapsed = 0.0f;

	//今回張り付いてからの時間(ウォールタップの判定用。張り付くたびに 0 から)
	float WallTapElapsed = 0.0f;

	//最後に走った壁の向き。同じ壁かどうかの判定に使う(ゼロなら着地後まだどの壁も走っていない)
	FVector LastWallRunNormal = FVector::ZeroVector;

	//今の壁走りが、直前と同じ壁への張り付き直しか
	bool bWallRunOnSameWall = false;

	//ウォールジャンプの角度の下がり具合(着地で 0。別の壁で 1、同じ壁で WallJumpSameWallDecayScale ずつ増える)
	float WallJumpDecay = 0.0f;

	//壁から離れた直後、同じ壁に張り付けない残り時間
	float WallRunSameWallCooldownRemaining = 0.0f;

	//地上ジャンプ直後で、壁走りを始めない残り時間
	float WallRunStartDelayRemaining = 0.0f;

	//壁走りが途切れた後、ジャンプを離せばウォールジャンプになる残り時間
	float WallJumpLateReleaseRemaining = 0.0f;

	//壁に張り付いた(パーチした)後は、着地するまで壁走りできない
	bool bWallRunLockedUntilLanded = false;

	//ジャストランディング: 空中でしゃがみを押してからの受付の残り時間・ブーストのクールダウン残り時間
	float JustLandingInputRemaining = 0.0f;
	float JustLandingCooldownRemaining = 0.0f;

	//前のフレームのしゃがみ入力(押した瞬間の判定用)
	bool bPrevWantsToCrouch = false;

	//壁に張り付いてからの時間
	float WallPerchElapsed = 0.0f;

	//ラーチ: 曲がれる残り時間・残りの角度・ジャンプした瞬間の入力の向き(ゼロなら入力なし)
	float LurchTimeRemaining = 0.0f;
	float LurchAngleRemaining = 0.0f;
	FVector LurchInputDir = FVector::ZeroVector;

	//クレストジャンプ: 最後に坂を上っていたときの上向きの速度と、頂上を越えてからの受付の残り時間
	float CrestUpSpeed = 0.0f;
	float CrestTimeRemaining = 0.0f;

	//よじ登り:開始位置・到着位置・終了後に進む向き・経過時間・かかる時間
	FVector MantleStartLocation = FVector::ZeroVector;
	FVector MantleTargetLocation = FVector::ZeroVector;
	FVector MantleExitDirection = FVector::ZeroVector;
	float MantleElapsed = 0.0f;
	float MantleDuration = 0.0f;

	FSavaCharacterMoveResponseDataContainer SavaMoveResponseDataContainer;
};

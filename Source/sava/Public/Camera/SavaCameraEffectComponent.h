// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SavaCameraEffectComponent.generated.h"

class AsavaCharacter;
class APlayerController;
class UCameraComponent;
class USavaCharacterMovementComponent;
class USavaEquipmentComponent;
struct FSavaWeaponStats;
struct FSavaDamageTakenInfo;

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

	//--------------------------------移動(キャラクターから呼ぶ)

	//着地した(FallSpeed = 着地直前の落ちる速さ。上向きなら 0 以下)
	void NotifyLanded(float FallSpeed);

	//着地の揺れの大きさ(0 〜 1)。MinSpeed より遅ければ 0、MaxSpeed 以上で 1
	static float CalculateLandingAlpha(float FallSpeed, float MinSpeed, float MaxSpeed);

	//--------------------------------被弾

	//ダメージを受けた(キャラクターの ClientDamageTaken から呼ぶ)。来た向きから押されたように揺らす
	void NotifyDamageTaken(const FSavaDamageTakenInfo& Info);

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

	//ダッシュ中の視野角の倍率(1.08 なら 8% 広がる)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|MoveFOV", meta = (ClampMin = "0.5", ClampMax = "2"))
	float SprintFOVScale = 1.08f;

	//スライディング中の視野角の倍率
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|MoveFOV", meta = (ClampMin = "0.5", ClampMax = "2"))
	float SlideFOVScale = 1.12f;

	//グラップルで引っ張られている間の視野角の倍率
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|MoveFOV", meta = (ClampMin = "0.5", ClampMax = "2"))
	float GrappleFOVScale = 1.12f;

	//スキルで速くなっている間(移動速度の倍率が 1 より大きい)の視野角の倍率。ダッシュなどの倍率にさらに掛かる
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|MoveFOV", meta = (ClampMin = "0.5", ClampMax = "2"))
	float SpeedBoostFOVScale = 1.1f;

	//ブリンクで動いている間(一瞬)の視野角の倍率
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|MoveFOV", meta = (ClampMin = "0.5", ClampMax = "2"))
	float BlinkFOVScale = 1.25f;

	//ブリンク中に視野角が広がる速さ(一瞬で広がるように、Move FOV Interp Speed より速くする)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|MoveFOV", meta = (ClampMin = "0"))
	float BlinkFOVInterpSpeed = 25.0f;

	//--------------------------------着地

	//これより遅く着地したら揺らさない(落ちる速さ)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Landing", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float LandingMinFallSpeed = 500.0f;

	//これ以上の速さで着地したら、一番大きく揺らす
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Landing", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float LandingMaxFallSpeed = 1500.0f;

	//一番大きいときに、カメラが沈む量
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Landing", meta = (ClampMin = "0", ForceUnits = "cm"))
	float LandingMaxDrop = 6.0f;

	//一番大きいときに、画面が下を向く角度
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Landing", meta = (ClampMin = "0", ForceUnits = "Deg"))
	float LandingMaxPitch = 2.5f;

	//一番沈むまでの時間(戻りきるのはこの 5 倍くらい)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Landing", meta = (ClampMin = "0.005", ForceUnits = "s"))
	float LandingKickTime = 0.07f;

	//--------------------------------ジャンプ

	//二段ジャンプで、画面が下を向く角度(踏み込んだ感じ)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Jump", meta = (ClampMin = "0", ForceUnits = "Deg"))
	float DoubleJumpKickPitch = 1.5f;

	//二段ジャンプで、カメラが沈む量
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Jump", meta = (ClampMin = "0", ForceUnits = "cm"))
	float DoubleJumpKickDrop = 2.0f;

	//ウォールジャンプで、壁と反対側へ傾く角度
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Jump", meta = (ClampMin = "0", ForceUnits = "Deg"))
	float WallJumpKickRoll = 3.0f;

	//ジャンプの揺れが一番大きくなるまでの時間
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Jump", meta = (ClampMin = "0.005", ForceUnits = "s"))
	float JumpKickTime = 0.08f;

	//--------------------------------歩行の揺れ

	//歩き・ダッシュで 1 秒に何歩分揺れるか(1 歩ごとに 1 回上下する)
	//キャラクターごとの歩く速さの違いで揺れ方が変わらないように、速さではなく歩き・ダッシュで決める
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|HeadBob", meta = (ClampMin = "0"))
	float HeadBobWalkStepsPerSecond = 1.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|HeadBob", meta = (ClampMin = "0"))
	float HeadBobSprintStepsPerSecond = 2.6f;

	//上下に揺れる大きさ(歩き)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|HeadBob", meta = (ClampMin = "0", ForceUnits = "cm"))
	float HeadBobVerticalAmplitude = 2.0f;

	//左右に揺れる大きさ(歩き。2 歩で 1 往復)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|HeadBob", meta = (ClampMin = "0", ForceUnits = "cm"))
	float HeadBobHorizontalAmplitude = 1.0f;

	//ダッシュ中の揺れの大きさの倍率
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|HeadBob", meta = (ClampMin = "0"))
	float HeadBobSprintScale = 1.3f;

	//これより遅いときは、止まっているとみなして揺らさない
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|HeadBob", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float HeadBobMinSpeed = 50.0f;

	//歩き始め・止まったときに、揺れの大きさが追いつく速さ
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|HeadBob", meta = (ClampMin = "0"))
	float HeadBobBlendSpeed = 8.0f;

	//ADS に入りきったときの揺れの倍率
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|HeadBob", meta = (ClampMin = "0", ClampMax = "1"))
	float HeadBobADSScale = 0.2f;

	//--------------------------------被弾

	//このダメージ以上で、一番大きく揺らす
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Damage", meta = (ClampMin = "1"))
	float DamageForMaxKick = 50.0f;

	//小さいダメージでも、この割合では揺らす
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Damage", meta = (ClampMin = "0", ClampMax = "1"))
	float DamageMinScale = 0.3f;

	//一番大きいときの、縦に向く角度(前から撃たれると上を、後ろからだと下を向く)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Damage", meta = (ClampMin = "0", ForceUnits = "Deg"))
	float DamageMaxPitch = 3.0f;

	//一番大きいときの、傾く角度(横から撃たれると、反対側へ傾く)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Damage", meta = (ClampMin = "0", ForceUnits = "Deg"))
	float DamageMaxRoll = 4.0f;

	//一番大きいときに、来た向きと反対へ押される量
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Damage", meta = (ClampMin = "0", ForceUnits = "cm"))
	float DamageMaxPush = 3.0f;

	//被弾の揺れが一番大きくなるまでの時間
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Damage", meta = (ClampMin = "0.005", ForceUnits = "s"))
	float DamageKickTime = 0.05f;

	//--------------------------------スタン・スロー

	//スタン中にふらつく角度(縦・横・傾き)。状態を伝える演出なので、設定の「画面の揺れの強さ」は掛けない
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Stun", meta = (ClampMin = "0", ForceUnits = "Deg"))
	FVector StunWobbleRotation = FVector(2.0f, 3.0f, 4.0f);

	//スタン中に、上下左右にふらつく量
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Stun", meta = (ClampMin = "0", ForceUnits = "cm"))
	float StunWobbleLocation = 1.5f;

	//ふらつく速さ(小さいほどゆっくり)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Stun", meta = (ClampMin = "0"))
	float StunWobbleFrequency = 0.6f;

	//スタンの始まり・終わりに、ふらつきの大きさが追いつく速さ
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Stun", meta = (ClampMin = "0"))
	float StunBlendSpeed = 3.0f;

	//スロー中の視野角の倍率(0.95 なら 5% 狭くなる)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Camera|Stun", meta = (ClampMin = "0.5", ClampMax = "2"))
	float SlowFOVScale = 0.95f;

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
	//地上を歩く速さに合わせて、歩行の揺れを進める
	void UpdateHeadBob(float DeltaTime);
	//スタン中のふらつきを進める
	void UpdateStun(float DeltaTime);
	//状態(スタン・スローなど)のタグが付いているか
	bool HasStateTag(const FGameplayTag& Tag) const;

	//二段ジャンプ・ウォールジャンプした(移動コンポーネントのイベント。サーバーでも呼ばれるので、本人の画面か確かめる)
	UFUNCTION()
	void HandleDoubleJumped();
	UFUNCTION()
	void HandleWallJumped();

	//今の動き方での、視野角の倍率の目標(1 = そのまま)
	float CalculateTargetMoveFOVScale() const;
	//その名前の Root Motion Source で動かされているか(グラップル・ブリンク)
	bool HasRootMotionSource(FName InstanceName) const;

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
	//衝撃(着地・ジャンプなど)の回転(X = 縦、Y = 横、Z = 傾き。度)と位置(X = 前、Y = 右、Z = 上。cm)
	FSavaCameraSpring ImpulseRotationSpring;
	FSavaCameraSpring ImpulseLocationSpring;

	//歩行の揺れ: 時間で進む位相と、今の大きさ(歩き始め・止まったときに滑らかに変わる)
	float HeadBobPhase = 0.0f;
	float HeadBobAlpha = 0.0f;

	//最後に壁走りしていた壁の左右(ウォールジャンプの瞬間には、もう壁走りが終わっているため)
	float LastWallRunSide = 0.0f;

	//スタンのふらつきの大きさ(0 〜 1。滑らかに変わる)と、ふらつきの時刻
	float StunAlpha = 0.0f;
	float StunTime = 0.0f;
	//前のフレームでカメラに見た目のずれを足したか(足していたら外す)
	bool bViewOffsetApplied = false;

	//--------------------------------腕

	//ずらしていないときの腕の位置(ずれている間は、カメラと一緒に動かす)
	FTransform ArmsBaseTransform;
	//腕を、見た目のずれ・ADS で元の位置から動かしているか(動かしていない間に、元の位置を覚え直す)
	bool bArmsModified = false;
};

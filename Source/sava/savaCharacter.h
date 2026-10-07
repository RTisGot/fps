// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/SavaAbilitySet.h"
#include "Logging/LogMacros.h"
#include "savaCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UUserWidget;
class USavaSettingsMenuController;
class USavaScoreboardWidget;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);
class USavaCharacterMovementComponent;
class USavaAbilitySystemComponent;
class USavaInputConfig;
class USavaEquipmentComponent;
class USavaAbilityLoadoutComponent;
UCLASS(config=Game)
class AsavaCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

	//このキャラクターに持たせる能力のセット(スキル・ガジェット・武器の各担当が作ったもの)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Abilities", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<USavaAbilitySet>> AbilitySets;

	//能力用のボタン(入力アクション → InputTag の対応表)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USavaInputConfig> AbilityInputConfig;

	//PlayerState にある ASC への参照(InitAbilitySystem で設定)
	UPROPERTY(Transient)
	TObjectPtr<USavaAbilitySystemComponent> AbilitySystemComponent;

	//付与した能力の控え(キャラクターを離れるときに取り除く)
	FSavaAbilitySet_GrantedHandles GrantedAbilityHandles;

	//持っている武器(メイン・サブ)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sava|Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USavaEquipmentComponent> EquipmentComponent;

	//持っているスキル(1 つ)とガジェット(1 つ)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sava|Loadout", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USavaAbilityLoadoutComponent> AbilityLoadoutComponent;

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Mesh, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* Mesh1P;

	//1人称カメラ
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* SprintAction;

	/** Crouch / Slide Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* CrouchAction;

	//しゃがみ切り替え時にカメラが目標の高さへ追いつく速さ
	UPROPERTY(EditDefaultsOnly, Category = Camera, meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	float CrouchCameraInterpSpeed = 12.0f;

	FVector CameraBaseLocation = FVector::ZeroVector; //カメラ本来の相対位置
	FVector CrouchCameraOffset = FVector::ZeroVector; //しゃがみ切り替え直後のずれ(0 へ補間する)

	//壁走り中にカメラを壁と反対側へ傾ける角度(マイナスにすると壁側へ傾く)
	UPROPERTY(EditDefaultsOnly, Category = Camera, meta = (AllowPrivateAccess = "true", ClampMin = "-45", ClampMax = "45", ForceUnits = "Deg"))
	float WallRunCameraTiltAngle = 12.0f;

	//カメラの傾きが目標の角度へ追いつく速さ
	UPROPERTY(EditDefaultsOnly, Category = Camera, meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	float WallRunCameraTiltSpeed = 8.0f;

	//壁走り中に見回せる範囲(進行方向から、壁と反対側へ何度まで)
	UPROPERTY(EditDefaultsOnly, Category = Camera, meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "180", ForceUnits = "Deg"))
	float WallRunCameraYawLimitAway = 110.0f;

	//壁走り中に見回せる範囲(進行方向から、壁側へ何度まで)
	UPROPERTY(EditDefaultsOnly, Category = Camera, meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "180", ForceUnits = "Deg"))
	float WallRunCameraYawLimitTowardWall = 30.0f;

	//範囲の外を向いて張り付いたとき、範囲内へ寄せる速さ
	UPROPERTY(EditDefaultsOnly, Category = Camera, meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	float WallRunCameraYawLimitSpeed = 10.0f;

	//カメラの向きが範囲内に入ったか(入った後はしっかり固定する)
	bool bWallRunCameraYawSettled = false;

	//壁走り中の壁の向き。他のプレイヤー(SimulatedProxy)へ同期する(アニメーション・演出用)
	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal ReplicatedWallRunNormal;
	///
	UPROPERTY(EditDefaultsOnly, Category = UI, meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> SettingsWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> SettingsWidget;//生成した設定画面Widgetへの参照を保持する変数

	UPROPERTY(Transient)
	TObjectPtr<USavaSettingsMenuController> SettingsMenuController;

	/** スコアボードWidgetのクラス */
	UPROPERTY(EditDefaultsOnly, Category = "UI|Scoreboard")
	TSubclassOf<USavaScoreboardWidget> ScoreboardWidgetClass;

	/** 現在表示しているスコアボード */
	UPROPERTY(Transient)
	TObjectPtr<USavaScoreboardWidget> ScoreboardWidget;
	
public:
	AsavaCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//壁走り中の壁の向き(壁から外向き)。壁走りしていなければゼロ。自分・サーバー・他のプレイヤーのどれでも有効
	UFUNCTION(BlueprintPure, Category = "Movement|WallRun")
	FVector GetWallRunNormal() const;

	//壁走り中の壁の位置: 右なら 1、左なら -1、壁走りしていなければ 0(アニメーション用)
	UFUNCTION(BlueprintPure, Category = "Movement|WallRun")
	float GetWallRunSide() const;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	USavaAbilitySystemComponent* GetSavaAbilitySystemComponent() const { return AbilitySystemComponent; }

	//設定メニューを閉じる関数/
	UFUNCTION(BlueprintCallable, Category = "UI|Settings")
	void CloseSettingsMenu();

protected:
	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	void StartSprint();
	void StopSprint();

	void StartJump();
	void StopJump();

	void StartCrouch();
	void StopCrouch();

	virtual void BeginPlay() override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual bool CanJumpInternal_Implementation() const override;

	//壁走り中のカメラの傾き(自分の画面だけの演出)
	void UpdateWallRunCameraTilt(float DeltaSeconds);

	//壁走り中に見回せる範囲を制限する(自分の画面だけ。サーバーには制限後の向きが送られる)
	void UpdateWallRunCameraYawLimit(float DeltaSeconds);

	//能力のボタン
	void Input_AbilityInputTagPressed(FGameplayTag InputTag);
	void Input_AbilityInputTagReleased(FGameplayTag InputTag);

	//GAS の初期化(サーバー: PossessedBy / クライアント: OnRep_PlayerState から呼ぶ)
	void InitAbilitySystem();

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;


	void ToggleSettingsMenu();

	/** スコアボードを表示する */
	void ShowScoreboard();

	/** スコアボードを非表示にする */
	void HideScoreboard();

	//HP が 0 になった(サーバーだけで呼ばれる。AttributeSet の OnOutOfHealth から)
	void HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, float DamageAmount);

	//死亡処理(サーバーだけ)
	void HandleDeath();

	//bIsDead が届いたとき(クライアント)/ サーバーは HandleDeath から直接呼ぶ。死体の見た目と動きを止める
	UFUNCTION()
	void OnRep_IsDead();

	//死んでいるか。全員に複製して、死体の見た目を揃える
	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	//死んでから死体を消すまでの秒数(GameMode の RespawnDelay より長くすること)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Death", meta = (ClampMin = "0", ForceUnits = "s"))
	float DeadBodyLifeSpan = 10.0f;

protected:
	// APawn interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface

public:
	/** Returns Mesh1P subobject **/
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	/** Returns FirstPersonCameraComponent subobject **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
	USavaCharacterMovementComponent* GetSavaCharacterMovementComponent() const;
	USavaEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }
	USavaAbilityLoadoutComponent* GetAbilityLoadoutComponent() const { return AbilityLoadoutComponent; }
};
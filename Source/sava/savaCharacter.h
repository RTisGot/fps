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
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);
class USavaCharacterMovementComponent;
class USavaAbilitySystemComponent;
class USavaInputConfig;
class USavaEquipmentComponent;
class USavaAbilityLoadoutComponent;
class USavaCameraEffectComponent;
class ASavaInteractable;
struct FSavaCharacterRow;
class USoundBase;

//ダメージを受けたときに、受けた本人へ届く情報(ダメージ方向の表示・被弾音に使う)
USTRUCT(BlueprintType)
struct FSavaDamageTakenInfo
{
	GENERATED_BODY()

	//受けたダメージ
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float Damage = 0.0f;

	//ダメージが来た位置(銃なら撃った人、グレネードなら爆発した位置。ダメージを受けた瞬間の位置)
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FVector_NetQuantize SourceLocation = FVector::ZeroVector;

	//Source Location が分かっているか(分からないダメージは方向を出さない)
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bHasSourceLocation = false;

	//自分の攻撃(自分のグレネードなど)
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bSelfDamage = false;

	//攻撃してきたキャラクター(遠くて自分の画面に居ない場合は空)
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	TObjectPtr<AActor> Attacker;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSavaOnDamageTaken, const FSavaDamageTakenInfo&, Info);

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

	//1人称カメラの演出(しゃがみの高さ・壁走りの傾き・ADS の視野角・反動など)をまとめて動かす
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USavaCameraEffectComponent> CameraEffectComponent;

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

	//壁走り中に見回せる範囲(進行方向から、壁と反対側へ何度まで)。90 以上にすると壁に背を向けられてしまう
	UPROPERTY(EditDefaultsOnly, Category = Camera, meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "180", ForceUnits = "Deg"))
	float WallRunCameraYawLimitAway = 60.0f;

	//壁走り中に見回せる範囲(進行方向から、壁側へ何度まで)
	UPROPERTY(EditDefaultsOnly, Category = Camera, meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "180", ForceUnits = "Deg"))
	float WallRunCameraYawLimitTowardWall = 30.0f;

	//範囲の外を向いて張り付いたとき、範囲内へ寄せる速さ
	UPROPERTY(EditDefaultsOnly, Category = Camera, meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	float WallRunCameraYawLimitSpeed = 10.0f;

	//カメラの向きが範囲内に入ったか(入った後はしっかり固定する)
	bool bWallRunCameraYawSettled = false;

	//操作する人が決まったが、その PlayerController がまだこの PC とつながっていない(Seamless Travel 直後のクライアント)
	//つながったら Tick で NotifyControllerChanged をやり直す
	bool bControllerChangePending = false;

	//壁走り中の壁の向き。他のプレイヤー(SimulatedProxy)へ同期する(アニメーション・演出用)
	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal ReplicatedWallRunNormal;

	//範囲に入っている端末・ボタン(しゃがみキーで使う)
	TWeakObjectPtr<ASavaInteractable> NearbyInteractable;

	//ロードアウトで選んだキャラクター(DT_Character の行名)。全員に複製して、カプセル・速度・カメラを揃える
	UPROPERTY(ReplicatedUsing = OnRep_CharacterId)
	FName CharacterId;

	UFUNCTION()
	void OnRep_CharacterId();

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

	//死んでいるか(死体として残っている間も true)
	UFUNCTION(BlueprintPure, Category = "Sava|Death")
	bool IsDead() const { return bIsDead; }

	//奈落(KillZ)に落ちたら死亡扱いにする(既定では体が消えるだけで、リスポーンされないため)
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

	//キャラの操作(移動・視点・ジャンプ・能力など)を受け付けるか。設定画面を開いている間は受け付けない
	//(設定画面は ASavaPlayerController が持つ)
	void SetGameplayInputEnabled(APlayerController* PlayerController, bool bEnabled);

	//近くの端末・ボタン(範囲に入ると相手が呼ぶ。自分の画面だけ)。使えるものの近くでは、しゃがみキーでしゃがまずにそれを使う
	void SetNearbyInteractable(ASavaInteractable* Interactable);
	void ClearNearbyInteractable(ASavaInteractable* Interactable);

	//--------------------------------キャラクター(ロードアウトで選ぶ)

	//DT_Character の行名(まだ決まっていなければ None)
	UFUNCTION(BlueprintPure, Category = "Sava|Character")
	FName GetCharacterId() const { return CharacterId; }

	//サーバーでのみ呼ぶ。キャラクターを変え、能力値を体に反映する(クライアントには複製で届く)
	void SetCharacterId(FName NewCharacterId);

	//能力値(カプセル・しゃがみの高さ・歩きの速さ・カメラの高さ)を体に反映する。HP と与えるダメージは PlayerState 側
	void ApplyCharacterStats(const FSavaCharacterRow& Row);

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

	//壁走り中に見回せる範囲を制限する(自分の画面だけ。サーバーには制限後の向きが送られる)
	void UpdateWallRunCameraYawLimit(float DeltaSeconds);

	//能力のボタン
	void Input_AbilityInputTagPressed(FGameplayTag InputTag);
	void Input_AbilityInputTagReleased(FGameplayTag InputTag);

	//GAS の初期化(サーバー: PossessedBy / クライアント: OnRep_PlayerState から呼ぶ)
	void InitAbilitySystem();

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//この体が変数に持っている画面(BP_FirstPersonCharacter の HUD など)を外す
	//体が操作されなくなった・消えたときに呼ぶ(外さないと、リスポーンやラウンドの出し直しのたびに HUD が重なる)
	void RemoveOwnedWidgets();
	virtual void OnRep_PlayerState() override;


	//HP が 0 になった(サーバーだけで呼ばれる。AttributeSet の OnOutOfHealth から)
	void HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, float DamageAmount);

	//ダメージを受けた(サーバーだけで呼ばれる。AttributeSet の OnDamageTaken から)。受けた本人へ伝える
	void HandleDamageTaken(AActor* DamageInstigator, AActor* DamageCauser, float DamageAmount);

	//受けた本人の画面: 被弾音を鳴らし、On Damage Taken を呼ぶ(毒ガスなど続くダメージで多く届くので、届かなくてもよい送り方にする)
	UFUNCTION(Client, Unreliable)
	void ClientDamageTaken(const FSavaDamageTakenInfo& Info);

	//ダメージを受けたときに、受けた本人だけに聞こえる音(空なら鳴らさない)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Damage")
	TObjectPtr<USoundBase> DamageTakenSound;

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
	USavaCameraEffectComponent* GetCameraEffectComponent() const { return CameraEffectComponent; }

	//ダメージを受けた(受けた本人の画面で呼ばれる)。ダメージ方向の表示は、これで Source Location を覚え、
	//毎フレーム Get Damage Direction Angle で矢印を回す(振り向くと矢印も回る)
	UPROPERTY(BlueprintAssignable, Category = "Sava|Damage")
	FSavaOnDamageTaken OnDamageTaken;

	//SourceLocation が、今向いている方向から見て何度の向きにあるか(0 = 正面、90 = 右、-90 = 左、±180 = 真後ろ。上下は見ない)
	UFUNCTION(BlueprintPure, Category = "Sava|Damage")
	float GetDamageDirectionAngle(const FVector& SourceLocation) const;

	//ViewLocation から Yaw の向きを見たとき、SourceLocation が何度の向きにあるか(GetDamageDirectionAngle の中身)
	static float CalculateDirectionAngle(const FVector& ViewLocation, float ViewYaw, const FVector& SourceLocation);
};
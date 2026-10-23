// Copyright Epic Games, Inc. All Rights Reserved.

#include "savaCharacter.h"
#include "savaProjectile.h"
#include "SavaCharacterMovementComponent.h"
#include "AbilitySystem/SavaAbilityLoadoutComponent.h"
#include "AbilitySystem/SavaAbilitySystemComponent.h"
#include "AbilitySystem/SavaInputConfig.h"
#include "Player/SavaPlayerState.h"
#include "Weapon/SavaEquipmentComponent.h"
#include "Lobby/SavaInteractable.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "SavaGameUserSettings.h"
#include "Player/SavaPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "savaGameMode.h"
#include "SavaGameplayTags.h"
#include "AbilitySystem/SavaAttributeSet.h"
#include "AbilitySystem/SavaAbilitySystemLibrary.h"
#include "AbilitySystem/SavaHoldAimAbility.h"
#include "Loadout/SavaCharacterTypes.h"
#include "Loadout/SavaLoadoutSettings.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);


// AsavaCharacter

AsavaCharacter::AsavaCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<USavaCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	//地上のジャンプ + 空中で1回(二段ジャンプ)。動きは移動コンポーネントの Sava|DoubleJump で調整する
	JumpMaxCount = 2;

	//カメラコンポネントを作成
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));//一人称視点のコンポネントを作成
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());//カメラをキャラにつける
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-10.f, 0.f, 60.f)); // カメラの位置は親コンポネントを基準に
	FirstPersonCameraComponent->bUsePawnControlRotation = true;


	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));

	EquipmentComponent = CreateDefaultSubobject<USavaEquipmentComponent>(TEXT("Equipment"));
	AbilityLoadoutComponent = CreateDefaultSubobject<USavaAbilityLoadoutComponent>(TEXT("AbilityLoadout"));
}

//--------------------------------Input

void AsavaCharacter::NotifyControllerChanged()
{
	//自分が操作しなくなった体(死んで抜けた・ラウンドの出し直し)の HUD は外す
	if (!IsLocallyControlled())
	{
		RemoveOwnedWidgets();
	}

	//Seamless Travel(待合室 → 試合)の直後、クライアントでは作り直された PlayerController が、この PC とつながる前に届くことがある
	//そのまま進めると HUD の作成(BP_FirstPersonCharacter)や入力の設定が失敗するので、つながるまで待つ(Tick でやり直す)
	const APlayerController* LocalController = Cast<APlayerController>(Controller);
	bControllerChangePending = LocalController && LocalController->IsLocalController() && !LocalController->GetLocalPlayer();
	if (bControllerChangePending)
	{
		UE_LOG(LogTemplateCharacter, Log, TEXT("%s: PlayerController がまだこの PC とつながっていないので、つながるまで待ちます"), *GetName());
		return;
	}

	Super::NotifyControllerChanged();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		//設定画面を開いたままリスポーンした場合は、閉じるまで操作を受け付けない
		const ASavaPlayerController* SavaPlayerController = Cast<ASavaPlayerController>(PlayerController);
		const bool bSettingsMenuOpen = SavaPlayerController && SavaPlayerController->IsSettingsMenuOpen();
		SetGameplayInputEnabled(PlayerController, !bSettingsMenuOpen);

		//入力モード(UI 用かゲーム用か)はマップをまたいで残る。タイトル(UI 用)を通って来た場合などに備えて、
		//自分が操作するキャラクターを持ったらゲーム用に戻す
		if (PlayerController->IsLocalController() && !bSettingsMenuOpen)
		{
			PlayerController->SetInputMode(FInputModeGameOnly());
			PlayerController->bShowMouseCursor = false;
		}
	}
}

void AsavaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping(押している間は壁走り、離すとウォールジャンプ)
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AsavaCharacter::StartJump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AsavaCharacter::StopJump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Canceled, this, &AsavaCharacter::StopJump);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AsavaCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AsavaCharacter::Look);

		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AsavaCharacter::StartSprint);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AsavaCharacter::StopSprint);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Canceled, this, &AsavaCharacter::StopSprint);

		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &AsavaCharacter::StartCrouch);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AsavaCharacter::StopCrouch);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Canceled, this, &AsavaCharacter::StopCrouch);

		// Abilities(対応表に書かれたボタンを、同じ InputTag を持つ能力へ流す)
		if (AbilityInputConfig)
		{
			for (const FSavaInputAction& Entry : AbilityInputConfig->AbilityInputActions)
			{
				if (!Entry.InputAction || !Entry.InputTag.IsValid())
				{
					continue;
				}
				EnhancedInputComponent->BindAction(Entry.InputAction, ETriggerEvent::Started, this, &AsavaCharacter::Input_AbilityInputTagPressed, Entry.InputTag);
				EnhancedInputComponent->BindAction(Entry.InputAction, ETriggerEvent::Completed, this, &AsavaCharacter::Input_AbilityInputTagReleased, Entry.InputTag);
				EnhancedInputComponent->BindAction(Entry.InputAction, ETriggerEvent::Canceled, this, &AsavaCharacter::Input_AbilityInputTagReleased, Entry.InputTag);
			}
		}
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}

	//Esc(設定画面)と Tab(スコアボード)は、死亡中も効くように ASavaPlayerController で受け取る
}


void AsavaCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		if (GetCharacterMovement()->GetRootMotionSource(FName(TEXT("GrapplePull"))).IsValid())
		{
			const FRotator ViewYaw(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
			AddMovementInput(FRotationMatrix(ViewYaw).GetUnitAxis(EAxis::X), MovementVector.Y);
			AddMovementInput(FRotationMatrix(ViewYaw).GetUnitAxis(EAxis::Y), MovementVector.X);
			return;
		}
		// add movement 
		AddMovementInput(GetActorForwardVector(), MovementVector.Y);
		AddMovementInput(GetActorRightVector(), MovementVector.X);
	}
}

void AsavaCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	const USavaGameUserSettings* Settings = USavaGameUserSettings::GetSavaGameUserSettings();
	//ADS 中は、ズームに合わせて感度を下げる
	const float MouseSensitivity = (Settings ? Settings->GetMouseSensitivity() : 1.0f)
		* (EquipmentComponent ? EquipmentComponent->GetLookSensitivityScale() : 1.0f);

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X * MouseSensitivity);
		const float PitchDirection = Settings && Settings->IsYInverted() ? -1.0f : 1.0f;
		AddControllerPitchInput(LookAxisVector.Y * MouseSensitivity * PitchDirection);
	}
}

void AsavaCharacter::StartSprint() {
	GetSavaCharacterMovementComponent()->StartSprint();
}

void AsavaCharacter::StopSprint() {
	GetSavaCharacterMovementComponent()->StopSprint();
}

void AsavaCharacter::StartJump() {
	Jump();
	GetSavaCharacterMovementComponent()->SetJumpHeld(true);
}

void AsavaCharacter::StopJump() {
	StopJumping();
	GetSavaCharacterMovementComponent()->SetJumpHeld(false);
}

void AsavaCharacter::StartCrouch() {
	//端末・ボタンの近くでは、しゃがまずにそれを使う(ホスト限定で使えないときは普通にしゃがむ)
	if (ASavaInteractable* Interactable = NearbyInteractable.Get(); Interactable && IsLocallyControlled()
		&& Interactable->TryInteract(GetController<APlayerController>()))
	{
		return;
	}

	Crouch();
}

void AsavaCharacter::SetNearbyInteractable(ASavaInteractable* Interactable)
{
	NearbyInteractable = Interactable;
}

void AsavaCharacter::ClearNearbyInteractable(ASavaInteractable* Interactable)
{
	if (NearbyInteractable.Get() == Interactable)
	{
		NearbyInteractable.Reset();
	}
}

void AsavaCharacter::StopCrouch() {
	UnCrouch();
}

void AsavaCharacter::BeginPlay()
{
	Super::BeginPlay();
	CameraBaseLocation = FirstPersonCameraComponent->GetRelativeLocation();
}

void AsavaCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	//PlayerController がこの PC とつながったので、待っていた「操作する人が決まった」処理を行う
	if (bControllerChangePending)
	{
		const APlayerController* LocalController = Cast<APlayerController>(Controller);
		if (!LocalController)
		{
			bControllerChangePending = false;
		}
		else if (LocalController->GetLocalPlayer())
		{
			UE_LOG(LogTemplateCharacter, Log, TEXT("%s: PlayerController がつながったので、操作する人が決まった処理をやり直します"), *GetName());
			NotifyControllerChanged();
		}
	}

	//しゃがみ切り替え時のカメラのずれを滑らかに戻す(見た目だけの処理)
	if (!CrouchCameraOffset.IsZero())
	{
		CrouchCameraOffset = FMath::VInterpTo(CrouchCameraOffset, FVector::ZeroVector, DeltaSeconds, CrouchCameraInterpSpeed);
		UpdateFirstPersonCameraLocation();
	}

	//壁の向きを他のプレイヤーへ同期する(サーバーだけが書き込む。値が変わったときだけ送られる)
	if (HasAuthority())
	{
		const USavaCharacterMovementComponent* SavaMovement = GetSavaCharacterMovementComponent();
		ReplicatedWallRunNormal = SavaMovement->IsWallRunning() ? SavaMovement->GetWallRunNormal() : FVector::ZeroVector;
	}

	UpdateWallRunCameraYawLimit(DeltaSeconds);
	UpdateWallRunCameraTilt(DeltaSeconds);
}

void AsavaCharacter::UpdateWallRunCameraYawLimit(float DeltaSeconds)
{
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	const USavaCharacterMovementComponent* SavaMovement = GetSavaCharacterMovementComponent();
	const FVector RunDir = SavaMovement->GetWallRunDirection();
	if (!PlayerController || !IsLocallyControlled() || RunDir.IsNearlyZero())
	{
		bWallRunCameraYawSettled = false;
		return;
	}

	//壁と反対側へ向く回転の向き(+1 なら右回り)
	const FVector RunRight = FVector::CrossProduct(FVector::UpVector, RunDir);
	const float AwaySign = FVector::DotProduct(SavaMovement->GetWallRunNormal(), RunRight) >= 0.0f ? 1.0f : -1.0f;

	//進行方向から見た視線の角度(+ が壁と反対側)
	FRotator ControlRotation = PlayerController->GetControlRotation();
	const float RunYaw = RunDir.Rotation().Yaw;
	const float YawOffset = FRotator::NormalizeAxis(ControlRotation.Yaw - RunYaw) * AwaySign;
	const float ClampedOffset = FMath::Clamp(YawOffset, -WallRunCameraYawLimitTowardWall, WallRunCameraYawLimitAway);
	if (YawOffset == ClampedOffset)
	{
		bWallRunCameraYawSettled = true;
		return;
	}

	//範囲に入った後はしっかり止める。入る前(範囲外を向いて張り付いた直後)は滑らかに寄せる
	float NewOffset = ClampedOffset;
	if (!bWallRunCameraYawSettled)
	{
		NewOffset = FMath::FInterpTo(YawOffset, ClampedOffset, DeltaSeconds, WallRunCameraYawLimitSpeed);
		bWallRunCameraYawSettled = FMath::IsNearlyEqual(NewOffset, ClampedOffset, 0.5f);
	}

	ControlRotation.Yaw = RunYaw + NewOffset * AwaySign;
	PlayerController->SetControlRotation(ControlRotation);
}

void AsavaCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//自分とサーバーは移動コンポーネントが持っているので、他のプレイヤーにだけ送る
	DOREPLIFETIME_CONDITION(AsavaCharacter, ReplicatedWallRunNormal, COND_SimulatedOnly);
	DOREPLIFETIME(AsavaCharacter, bIsDead);
	DOREPLIFETIME(AsavaCharacter, CharacterId);
}

FVector AsavaCharacter::GetWallRunNormal() const
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return ReplicatedWallRunNormal;
	}

	const USavaCharacterMovementComponent* SavaMovement = GetSavaCharacterMovementComponent();
	return SavaMovement->IsWallRunning() ? SavaMovement->GetWallRunNormal() : FVector::ZeroVector;
}

float AsavaCharacter::GetWallRunSide() const
{
	//壁の向きは壁から外向きなので、右の壁なら右方向と逆向きになる
	const FVector WallNormal = GetWallRunNormal();
	if (WallNormal.IsNearlyZero())
	{
		return 0.0f;
	}
	return FVector::DotProduct(WallNormal, GetActorRightVector()) < 0.0f ? 1.0f : -1.0f;
}

void AsavaCharacter::UpdateWallRunCameraTilt(float DeltaSeconds)
{
	//視点の回転に傾き(ロール)を足す。カメラに付いた腕も一緒に傾き、狙う方向には影響しない
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (!PlayerController || !IsLocallyControlled())
	{
		return;
	}

	//右の壁なら左へ、左の壁なら右へ傾ける
	const float TargetRoll = -GetWallRunSide() * WallRunCameraTiltAngle;
	FRotator ControlRotation = PlayerController->GetControlRotation();
	const float CurrentRoll = FRotator::NormalizeAxis(ControlRotation.Roll);
	if (CurrentRoll == TargetRoll)
	{
		return;
	}

	ControlRotation.Roll = FMath::FInterpTo(CurrentRoll, TargetRoll, DeltaSeconds, WallRunCameraTiltSpeed);
	PlayerController->SetControlRotation(ControlRotation);
}

void AsavaCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	//カプセルの上端は中心より ScaledHalfHeightAdjust だけ下がるので、カメラも同じだけ下げる
	//(下げないと、しゃがんだときに目がカプセルの上に出て、自分は撃てるのに撃たれない高さができる)
	CrouchCameraDrop = ScaledHalfHeightAdjust;
	float CameraJump = ScaledHalfHeightAdjust;

	//地上ではカプセルが足元基準で縮み、中心も一瞬で下がる
	if (GetCharacterMovement()->bCrouchMaintainsBaseLocation)
	{
		CameraJump += ScaledHalfHeightAdjust;
	}

	//一瞬で下がった分を打ち消して、元の高さから補間させる
	CrouchCameraOffset.Z += CameraJump;
	UpdateFirstPersonCameraLocation();
}

void AsavaCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	CrouchCameraDrop = 0.0f;
	float CameraJump = ScaledHalfHeightAdjust;

	if (GetCharacterMovement()->bCrouchMaintainsBaseLocation)
	{
		CameraJump += ScaledHalfHeightAdjust;
	}

	CrouchCameraOffset.Z -= CameraJump;
	UpdateFirstPersonCameraLocation();
}

void AsavaCharacter::UpdateFirstPersonCameraLocation()
{
	FirstPersonCameraComponent->SetRelativeLocation(CameraBaseLocation - FVector(0.0f, 0.0f, CrouchCameraDrop) + CrouchCameraOffset);
}

//--------------------------------キャラクター

void AsavaCharacter::SetCharacterId(FName NewCharacterId)
{
	if (!HasAuthority() || CharacterId == NewCharacterId)
	{
		return;
	}

	CharacterId = NewCharacterId;
	OnRep_CharacterId(); //サーバーでは OnRep が呼ばれないので自分で呼ぶ
}

void AsavaCharacter::OnRep_CharacterId()
{
	if (const FSavaCharacterRow* Row = USavaLoadoutSettings::FindCharacterRow(CharacterId))
	{
		ApplyCharacterStats(*Row);
	}
}

void AsavaCharacter::ApplyCharacterStats(const FSavaCharacterRow& Row)
{
	USavaCharacterMovementComponent* Movement = GetSavaCharacterMovementComponent();

	//しゃがんだまま大きさを変えると、立ち上がるときの戻し先がずれるので先に立たせる
	if (bIsCrouched)
	{
		Movement->bWantsToCrouch = false;
		Movement->UnCrouch(GetLocalRole() == ROLE_SimulatedProxy);
	}

	//カプセル(0 は未入力として体の既定値を使う。高さの半分は半径より小さくできない)
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	const UCapsuleComponent* DefaultCapsule = GetClass()->GetDefaultObject<ACharacter>()->GetCapsuleComponent();
	const float Radius = Row.CapsuleRadius > 0.0f ? Row.CapsuleRadius : DefaultCapsule->GetUnscaledCapsuleRadius();
	const float HalfHeight = FMath::Max(Row.CapsuleHalfHeight > 0.0f ? Row.CapsuleHalfHeight : DefaultCapsule->GetUnscaledCapsuleHalfHeight(), Radius);
	const float OldHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	Capsule->SetCapsuleSize(Radius, HalfHeight);

	//足元の高さを保つ。位置はサーバーが決めて届くので、サーバーだけ動かす
	if (HasAuthority())
	{
		AddActorWorldOffset(FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight() - OldHalfHeight), false, nullptr, ETeleportType::TeleportPhysics);
	}

	Movement->SetCrouchedHalfHeight(FMath::Max(Row.CrouchedHalfHeight, Radius));
	Movement->MaxWalkSpeed = Row.MoveSpeed;

	//目はカプセルの上端から決まった距離に置く(しゃがみ中は OnStartCrouch で縮んだ分だけ下げる)
	//BeginPlay 前(複製が先に届いた)ならまだ控えがないので、今の位置を元にする
	FVector BaseLocation = HasActorBegunPlay() ? CameraBaseLocation : FirstPersonCameraComponent->GetRelativeLocation();
	BaseLocation.Z = HalfHeight - Row.EyeHeightBelowTop;
	CameraBaseLocation = BaseLocation;
	CrouchCameraOffset = FVector::ZeroVector;
	UpdateFirstPersonCameraLocation();
}

bool AsavaCharacter::CanJumpInternal_Implementation() const
{
	//ラウンド開始前のカウントダウン中は動けない(移動速度は GE で 0 になっている)
	if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(SavaGameplayTags::State_RoundFrozen))
	{
		return false;
	}


	//通常はしゃがみ中ジャンプ不可だが、スライディング中(スライディングジャンプ)と空中(二段ジャンプ)は許可する
	const USavaCharacterMovementComponent* SavaMovement = GetSavaCharacterMovementComponent();
	if (SavaMovement->IsSliding() || SavaMovement->IsFalling())
	{
		return JumpIsAllowedInternal();
	}
	return Super::CanJumpInternal_Implementation();
}

USavaCharacterMovementComponent* AsavaCharacter::GetSavaCharacterMovementComponent() const
{
	return CastChecked<USavaCharacterMovementComponent>(GetCharacterMovement());
}

//--------------------------------Abilities

UAbilitySystemComponent* AsavaCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AsavaCharacter::InitAbilitySystem()
{
	ASavaPlayerState* SavaPlayerState = GetPlayerState<ASavaPlayerState>();
	if (!SavaPlayerState)
	{
		if (GetPlayerState())
		{
			UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' PlayerState is not ASavaPlayerState. Set the GameMode's Player State Class to SavaPlayerState."), *GetNameSafe(this));
		}
		return;
	}

	AbilitySystemComponent = SavaPlayerState->GetSavaAbilitySystemComponent();
	//持ち主 = PlayerState、体 = このキャラクター
	AbilitySystemComponent->InitAbilityActorInfo(SavaPlayerState, this);

	//能力の付与はサーバーだけが行う(クライアントへは自動で伝わる)
	if (HasAuthority())
	{
		GrantedAbilityHandles.TakeFromAbilitySystem(AbilitySystemComponent);
		for (const USavaAbilitySet* AbilitySet : AbilitySets)
		{
			if (AbilitySet)
			{
				AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &GrantedAbilityHandles);
			}
		}
		//プレイヤーが選んだロードアウト(届いていれば)をこの体に入れてから、スキル・ガジェットを付ける
		if (SavaPlayerState->HasReceivedLoadout())
		{
			SavaPlayerState->ApplyLoadoutTo(this);
		}
		AbilityLoadoutComponent->GrantAbilities(AbilitySystemComponent);

		//HP 0 の通知を受け取る。PlayerState の AttributeSet は体が変わっても残るので、二重登録しない
		if (const USavaAttributeSet* Attributes = SavaPlayerState->GetAttributeSet())
		{
			Attributes->OnOutOfHealth.RemoveAll(this);
			Attributes->OnOutOfHealth.AddUObject(this, &AsavaCharacter::HandleOutOfHealth);
			Attributes->OnDamageTaken.RemoveAll(this);
			Attributes->OnDamageTaken.AddUObject(this, &AsavaCharacter::HandleDamageTaken);
		}
	}
}

void AsavaCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitAbilitySystem();
}

void AsavaCharacter::UnPossessed()
{
	//このキャラクターで付与した能力を取り除く(次のキャラクターで付与し直す)
	if (AbilitySystemComponent && HasAuthority())
	{
		AbilitySystemComponent->CancelAllAbilities();
		GrantedAbilityHandles.TakeFromAbilitySystem(AbilitySystemComponent);
		AbilityLoadoutComponent->RevokeAbilities();
	}

	if (const ASavaPlayerState* SavaPlayerState = GetPlayerState<ASavaPlayerState>())
	{
		if (const USavaAttributeSet* Attributes = SavaPlayerState->GetAttributeSet())
		{
			Attributes->OnOutOfHealth.RemoveAll(this);
			Attributes->OnDamageTaken.RemoveAll(this);
		}
	}

	Super::UnPossessed();
}

void AsavaCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveOwnedWidgets();
	Super::EndPlay(EndPlayReason);
}

void AsavaCharacter::RemoveOwnedWidgets()
{
	//画面は Blueprint の変数に入っているので、Widget 型の変数を探して外す
	for (TFieldIterator<FObjectProperty> It(GetClass()); It; ++It)
	{
		if (!It->PropertyClass || !It->PropertyClass->IsChildOf(UUserWidget::StaticClass()))
		{
			continue;
		}
		if (UUserWidget* Widget = Cast<UUserWidget>(It->GetObjectPropertyValue_InContainer(this)))
		{
			Widget->RemoveFromParent();
			//空にしておく(同じ体をまた操作したときに作り直されるように)
			It->SetObjectPropertyValue_InContainer(this, nullptr);
		}
	}
}

void AsavaCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitAbilitySystem();
}

void AsavaCharacter::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	UE_LOG(LogTemp, Warning, TEXT("Ability Input Tag Pressed: %s"), *InputTag.ToString());
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityInputTagPressed(InputTag);
	}
}

void AsavaCharacter::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityInputTagReleased(InputTag);
	}
}

void AsavaCharacter::SetGameplayInputEnabled(APlayerController* PlayerController, bool bEnabled)
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PlayerController
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer())
		: nullptr;
	if (!Subsystem || !DefaultMappingContext)
	{
		return;
	}

	if (bEnabled)
	{
		//押したままのキーは、一度離すまで反応しない(AddMappingContext の既定の動き)
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
		return;
	}

	//押していたボタンを離した扱いにしてから外す(走り続け・撃ち続けを防ぐ)
	StopSprint();
	StopCrouch();
	StopJump();
	if (AbilitySystemComponent)
	{
		//狙っている途中の能力(ブリンク・投げ物など)は、離した扱いにすると確定してしまうので先にキャンセルする
		TArray<USavaHoldAimAbility*> AimingAbilities;
		for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
		{
			for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
			{
				if (USavaHoldAimAbility* AimAbility = Cast<USavaHoldAimAbility>(Instance))
				{
					AimingAbilities.Add(AimAbility);
				}
			}
		}
		for (USavaHoldAimAbility* AimAbility : AimingAbilities)
		{
			AimAbility->CancelAiming();
		}
	}
	if (AbilityInputConfig)
	{
		for (const FSavaInputAction& Entry : AbilityInputConfig->AbilityInputActions)
		{
			if (Entry.InputTag.IsValid())
			{
				Input_AbilityInputTagReleased(Entry.InputTag);
			}
		}
	}
	//マウスでの視点操作もこの中に入っている
	Subsystem->RemoveMappingContext(DefaultMappingContext);
}

void AsavaCharacter::HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, float DamageAmount)
{

	if (DamageInstigator && DamageInstigator != this)
	{
		//相手が死んだ後に爆発したグレネードでも、投げた人のキルになる
		if (ASavaPlayerState* AttackerPlayerState =
			Cast<ASavaPlayerState>(USavaAbilitySystemLibrary::GetOwningPlayerState(DamageInstigator)))
		{
			AttackerPlayerState->AddKill();
		}

		if (APawn* diedPawn = Cast<APawn>(this))
		{
			if (ASavaPlayerState* diedPlayerState =
				diedPawn->GetPlayerState<ASavaPlayerState>())
			{
				diedPlayerState->AddDeath();
			}
		}
	}

	//誰に倒されたか(DamageInstigator)は、後でキル数の加算に使う
	HandleDeath();
}

void AsavaCharacter::HandleDamageTaken(AActor* DamageInstigator, AActor* DamageCauser, float DamageAmount)
{
	if (!HasAuthority() || bIsDead || !IsPlayerControlled())
	{
		return;
	}

	FSavaDamageTakenInfo Info;
	Info.Damage = DamageAmount;
	Info.Attacker = DamageInstigator;
	Info.bSelfDamage = DamageInstigator == this;

	//来た位置は、ダメージを起こした物(銃なら撃った人、グレネードなら爆発した物)の位置。無ければ攻撃した人の位置
	if (const AActor* Source = DamageCauser ? DamageCauser : DamageInstigator)
	{
		Info.SourceLocation = Source->GetActorLocation();
		Info.bHasSourceLocation = true;
	}

	//ホスト自身なら、その場で呼ばれる
	ClientDamageTaken(Info);
}

void AsavaCharacter::ClientDamageTaken_Implementation(const FSavaDamageTakenInfo& Info)
{
	if (DamageTakenSound)
	{
		UGameplayStatics::PlaySound2D(this, DamageTakenSound);
	}
	OnDamageTaken.Broadcast(Info);
}

float AsavaCharacter::GetDamageDirectionAngle(const FVector& SourceLocation) const
{
	FVector ViewLocation = GetActorLocation();
	FRotator ViewRotation = GetActorRotation();
	if (Controller)
	{
		ViewRotation = Controller->GetControlRotation();
	}
	if (FirstPersonCameraComponent)
	{
		ViewLocation = FirstPersonCameraComponent->GetComponentLocation();
	}
	return CalculateDirectionAngle(ViewLocation, ViewRotation.Yaw, SourceLocation);
}

float AsavaCharacter::CalculateDirectionAngle(const FVector& ViewLocation, float ViewYaw, const FVector& SourceLocation)
{
	//上下は見ず、水平面での向きの差
	const FVector ToSource = SourceLocation - ViewLocation;
	if (ToSource.SizeSquared2D() < KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}
	const float SourceYaw = FMath::RadiansToDegrees(FMath::Atan2(ToSource.Y, ToSource.X));
	return FRotator::NormalizeAxis(SourceYaw - ViewYaw);
}

void AsavaCharacter::HandleDeath()
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	bIsDead = true;
	OnRep_IsDead(); //サーバー自身には OnRep が自動では呼ばれない

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(SavaGameplayTags::State_Dead); //先にタグ。能力が再発動しないように
		AbilitySystemComponent->CancelAllAbilities();
		AbilitySystemComponent->RemoveActiveEffects(FGameplayEffectQuery()); //期間付き・永続の GE をすべて外す(バフ・クールダウンなど)
	}

	AController* DeadController = GetController(); //Unpossess すると null になるので先に取っておく
	if (AsavaGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AsavaGameMode>() : nullptr)
	{
		GameMode->NotifyPlayerDied(DeadController); //手順 1: N 秒後に新しい体を出す
	}

	if (DeadController)
	{
		DeadController->UnPossess();
		if (APlayerController* PlayerController = Cast<APlayerController>(DeadController))
		{
			PlayerController->SetViewTarget(this); //死んだ場所の視点のまま見せる
		}
	}

	SetLifeSpan(DeadBodyLifeSpan); //死体は自動で消える(クライアントにも消滅が伝わる)
}

void AsavaCharacter::FellOutOfWorld(const UDamageType& DamageType)
{
	//クライアントでは何もしない(サーバーの死亡が複製されてくる)
	if (!HasAuthority())
	{
		return;
	}
	if (bIsDead)
	{
		//死体が落ちただけなら、既定どおり消す
		Super::FellOutOfWorld(DamageType);
		return;
	}

	if (ASavaPlayerState* SavaPlayerState = GetPlayerState<ASavaPlayerState>())
	{
		SavaPlayerState->AddDeath();
	}
	HandleDeath(); //リスポーン・旗の処理は通常の死亡と同じ(運んでいた旗は台へ戻る)
}

void AsavaCharacter::OnRep_IsDead()
{
	if (!bIsDead)
	{
		return;
	}

	//先に移動を止める(コリジョンだけ消すと、床をすり抜けて落ちる)
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	//撃たれても当たらないようにする
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	//3P メッシュが無いので、見た目は隠すだけ(後で死亡アニメやラグドールに差し替える)
	Mesh1P->SetVisibility(false, true);
	GetMesh()->SetVisibility(false, true);
}

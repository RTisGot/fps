// Copyright Epic Games, Inc. All Rights Reserved.

#include "savaCharacter.h"
#include "savaProjectile.h"
#include "SavaCharacterMovementComponent.h"
#include "AbilitySystem/SavaAbilityLoadoutComponent.h"
#include "AbilitySystem/SavaAbilitySystemComponent.h"
#include "AbilitySystem/SavaInputConfig.h"
#include "Player/SavaPlayerState.h"
#include "Weapon/SavaEquipmentComponent.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "SavaGameUserSettings.h"
#include "SavaSettingsWidget.h"
#include "SavaScoreboardWidget.h"
#include "SavaSettingsMenuController.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/PackageName.h"
#include "Net/UnrealNetwork.h"
#include "savaGameMode.h"
#include "SavaGameplayTags.h"
#include "AbilitySystem/SavaAttributeSet.h"
#include "AbilitySystem/SavaAbilitySystemLibrary.h"

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

	// Layout, style and animation are authored in the Widget Blueprint.
	if (FPackageName::DoesPackageExist(TEXT("/Game/WBP/WBP_SettingsMenu")))
	{
		static ConstructorHelpers::FClassFinder<USavaSettingsWidget> SettingsView(TEXT("/Game/WBP/WBP_SettingsMenu"));
		SettingsWidgetClass = SettingsView.Class;
	}

}

//--------------------------------Input

void AsavaCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
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

	// UEnhancedInputComponent intentionally disallows legacy BindKey calls.
	// Bind the Escape key through the base input component instead.
	FInputKeyBinding& SettingsBinding = PlayerInputComponent->BindKey(
		EKeys::Escape, IE_Pressed, this, &AsavaCharacter::ToggleSettingsMenu);
	SettingsBinding.bExecuteWhenPaused = true;

	// Tabキーでスコアボードを表示
	PlayerInputComponent->BindKey(
		EKeys::Tab,IE_Pressed,this,&AsavaCharacter::ShowScoreboard);

	// Tabキーを離したら非表示
	PlayerInputComponent->BindKey(
		EKeys::Tab,IE_Released,this,&AsavaCharacter::HideScoreboard);
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
	const float MouseSensitivity = Settings ? Settings->GetMouseSensitivity() : 1.0f;

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
	Crouch();
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

	//しゃがみ切り替え時のカメラのずれを滑らかに戻す(見た目だけの処理)
	if (!CrouchCameraOffset.IsZero())
	{
		CrouchCameraOffset = FMath::VInterpTo(CrouchCameraOffset, FVector::ZeroVector, DeltaSeconds, CrouchCameraInterpSpeed);
		FirstPersonCameraComponent->SetRelativeLocation(CameraBaseLocation + CrouchCameraOffset);
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

	//地上ではカプセルが足元基準で縮み、カメラが一瞬で下がるので、元の高さから補間させる
	if (GetCharacterMovement()->bCrouchMaintainsBaseLocation)
	{
		CrouchCameraOffset.Z += ScaledHalfHeightAdjust;
		FirstPersonCameraComponent->SetRelativeLocation(CameraBaseLocation + CrouchCameraOffset);
	}
}

void AsavaCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	if (GetCharacterMovement()->bCrouchMaintainsBaseLocation)
	{
		CrouchCameraOffset.Z -= ScaledHalfHeightAdjust;
		FirstPersonCameraComponent->SetRelativeLocation(CameraBaseLocation + CrouchCameraOffset);
	}
}

bool AsavaCharacter::CanJumpInternal_Implementation() const
{
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
		//選んだスキル・ガジェットも付ける
		AbilityLoadoutComponent->GrantAbilities(AbilitySystemComponent);

		//HP 0 の通知を受け取る。PlayerState の AttributeSet は体が変わっても残るので、二重登録しない
		if (const USavaAttributeSet* Attributes = SavaPlayerState->GetAttributeSet())
		{
			Attributes->OnOutOfHealth.RemoveAll(this);
			Attributes->OnOutOfHealth.AddUObject(this, &AsavaCharacter::HandleOutOfHealth);
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
		}
	}

	Super::UnPossessed();
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

//設定画面の開閉
void AsavaCharacter::ToggleSettingsMenu()
{
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (!PlayerController)
	{
		return;
	}

	if (SettingsWidget && SettingsWidget->IsInViewport())
	{
		CloseSettingsMenu();
		return;
	}

	USavaGameUserSettings* Settings = USavaGameUserSettings::GetSavaGameUserSettings();
	if (!Settings)
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("SavaGameUserSettings is not configured."));
		return;
	}

	// Migrate an inherited reference to the old WBP_Setting; allow subclasses of the new view.
	if (!SettingsWidgetClass || SettingsWidgetClass == USavaSettingsWidget::StaticClass()
		|| !SettingsWidgetClass->IsChildOf(USavaSettingsWidget::StaticClass()))
	{
		SettingsWidgetClass = LoadClass<USavaSettingsWidget>(nullptr, TEXT("/Game/WBP/WBP_SettingsMenu.WBP_SettingsMenu_C"));
	}
	if (!SettingsWidgetClass)
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("WBP_SettingsMenu is missing. The settings view must be a Widget Blueprint."));
		return;
	}
	if (SettingsWidget && !SettingsWidget->IsA<USavaSettingsWidget>()) SettingsWidget = nullptr;

	if (!SettingsWidget)
	{
		SettingsWidget = CreateWidget<UUserWidget>(PlayerController, SettingsWidgetClass);
	}

	if (USavaSettingsWidget* View = Cast<USavaSettingsWidget>(SettingsWidget))
	{
		SettingsMenuController = NewObject<USavaSettingsMenuController>(this);
		SettingsMenuController->Initialize(Settings, GetWorld()->WorldType != EWorldType::PIE);
		SettingsMenuController->AttachView(View);
		View->OnCloseRequested.BindUObject(this, &AsavaCharacter::CloseSettingsMenu);

		SettingsWidget->AddToViewport(100);
		PlayerController->bShowMouseCursor = true;

		FInputModeGameAndUI InputMode;                           //ゲーム操作とUI操作の両方を受け付ける入力モード
		InputMode.SetWidgetToFocus(SettingsWidget->TakeWidget());//入力のフォーカスを設定画面に向ける
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);//マウスカーソルをゲーム画面内に閉じ込めない
		InputMode.SetHideCursorDuringCapture(false);//マウス入力をゲームがキャプチャしたときでも、カーソルを隠さない
		PlayerController->SetInputMode(InputMode);//PlayerControllerに反映
		PlayerController->SetPause(true);
	}
}

void AsavaCharacter::ShowScoreboard()
{
	APlayerController* PlayerController =
		Cast<APlayerController>(Controller);

	if (!PlayerController || !ScoreboardWidgetClass)
	{
		return;
	}

	if (!ScoreboardWidget)
	{
		ScoreboardWidget =
			CreateWidget<USavaScoreboardWidget>(
				PlayerController,
				ScoreboardWidgetClass);
	}

	if (!ScoreboardWidget)
	{
		return;
	}

	if (!ScoreboardWidget->IsInViewport())
	{
		ScoreboardWidget->AddToViewport(50);
	}

	ScoreboardWidget->RefreshScoreboard();
}

void AsavaCharacter::HideScoreboard()
{
	if (!ScoreboardWidget)
	{
		return;
	}

	ScoreboardWidget->RemoveFromParent();
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

void AsavaCharacter::CloseSettingsMenu()
{
	if (SettingsMenuController) SettingsMenuController->Discard();

	StopSprint();
	StopCrouch();
	StopJump();
	if (SettingsWidget)
	{
		SettingsWidget->RemoveFromParent();//現在表示されている親から外す。
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		PlayerController->SetPause(false);
		PlayerController->bShowMouseCursor = false; //マウスカーソルを非表示に戻す処理
		PlayerController->SetInputMode(FInputModeGameOnly()); //ゲーム操作だけ受け付ける
		PlayerController->FlushPressedKeys(); //メニューを閉じた直後の誤入力を防ぐ
	}
}

void AsavaCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Restore an unconfirmed video preview even if the pawn/level is destroyed.
	if (SettingsMenuController) SettingsMenuController->Discard();
	if (SettingsWidget) SettingsWidget->RemoveFromParent();
	Super::EndPlay(EndPlayReason);
}

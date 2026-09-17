// Copyright Epic Games, Inc. All Rights Reserved.

#include "savaCharacter.h"
#include "savaProjectile.h"
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
#include "SavaSettingsMenuController.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);


// AsavaCharacter

AsavaCharacter::AsavaCharacter()
{
	
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
		
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
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AsavaCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AsavaCharacter::Look);
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
}


void AsavaCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
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

void AsavaCharacter::CloseSettingsMenu()
{
	if (SettingsMenuController) SettingsMenuController->Discard();
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

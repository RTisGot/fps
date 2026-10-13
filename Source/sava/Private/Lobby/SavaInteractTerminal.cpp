// Fill out your copyright notice in the Description page of Project Settings.

#include "Lobby/SavaInteractTerminal.h"
#include "Lobby/SavaTerminalWidget.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "savaCharacter.h"

ASavaInteractTerminal::ASavaInteractTerminal()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	InteractArea = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractArea"));
	InteractArea->SetupAttachment(Mesh);
	InteractArea->SetBoxExtent(FVector(150.0f, 150.0f, 100.0f));
	InteractArea->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	InteractArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void ASavaInteractTerminal::BeginPlay()
{
	Super::BeginPlay();

	InteractArea->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleBeginOverlap);
	InteractArea->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleEndOverlap);
}

void ASavaInteractTerminal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseMenu();
	if (PromptWidget)
	{
		PromptWidget->RemoveFromParent();
	}

	Super::EndPlay(EndPlayReason);
}

void ASavaInteractTerminal::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	//自分が操作しているキャラクターだけ(他の人やサーバー上のキャラクターは無視)
	AsavaCharacter* Character = Cast<AsavaCharacter>(OtherActor);
	if (!Character || !Character->IsLocallyControlled())
	{
		return;
	}

	Character->SetNearbyTerminal(this);
	SetPromptVisible(Character->GetController<APlayerController>(), true);
}

void ASavaInteractTerminal::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	AsavaCharacter* Character = Cast<AsavaCharacter>(OtherActor);
	if (!Character || !Character->IsLocallyControlled())
	{
		return;
	}

	Character->ClearNearbyTerminal(this);
	SetPromptVisible(Character->GetController<APlayerController>(), false);
}

void ASavaInteractTerminal::SetPromptVisible(APlayerController* PlayerController, bool bVisible)
{
	if (!bVisible)
	{
		if (PromptWidget)
		{
			PromptWidget->RemoveFromParent();
		}
		return;
	}

	if (!PromptWidgetClass || !PlayerController || IsMenuOpen())
	{
		return;
	}

	if (!PromptWidget)
	{
		PromptWidget = CreateWidget<UUserWidget>(PlayerController, PromptWidgetClass);
	}
	if (PromptWidget && !PromptWidget->IsInViewport())
	{
		PromptWidget->AddToViewport();
	}
}

void ASavaInteractTerminal::OpenMenu(APlayerController* PlayerController)
{
	if (!PlayerController || !MenuWidgetClass || IsMenuOpen())
	{
		return;
	}

	if (!MenuWidget)
	{
		MenuWidget = CreateWidget<USavaTerminalWidget>(PlayerController, MenuWidgetClass);
		if (!MenuWidget)
		{
			return;
		}
		MenuWidget->SetTerminal(this);
	}

	MenuOwner = PlayerController;
	SetPromptVisible(PlayerController, false);
	MenuWidget->AddToViewport(50);

	//設定画面(ASavaPlayerController)と同じ: カーソルを出し、入力を画面へ向け、キャラの操作だけ止める
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	PlayerController->SetInputMode(InputMode);
	PlayerController->bShowMouseCursor = true;

	if (AsavaCharacter* Character = PlayerController->GetPawn<AsavaCharacter>())
	{
		Character->SetGameplayInputEnabled(PlayerController, false);
	}
}

void ASavaInteractTerminal::CloseMenu()
{
	if (!IsMenuOpen())
	{
		return;
	}

	MenuWidget->RemoveFromParent();

	APlayerController* PlayerController = MenuOwner.Get();
	MenuOwner.Reset();
	if (!PlayerController)
	{
		return;
	}

	AsavaCharacter* Character = PlayerController->GetPawn<AsavaCharacter>();
	if (Character)
	{
		Character->SetGameplayInputEnabled(PlayerController, true);
	}
	PlayerController->bShowMouseCursor = false;
	PlayerController->SetInputMode(FInputModeGameOnly());
	PlayerController->FlushPressedKeys(); //閉じた直後の誤入力を防ぐ

	//まだ範囲内にいるなら案内を出し直す
	if (Character && InteractArea->IsOverlappingActor(Character))
	{
		SetPromptVisible(PlayerController, true);
	}
}

bool ASavaInteractTerminal::IsMenuOpen() const
{
	return MenuWidget && MenuWidget->IsInViewport();
}

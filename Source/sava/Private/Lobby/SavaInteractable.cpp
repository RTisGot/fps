// Fill out your copyright notice in the Description page of Project Settings.

#include "Lobby/SavaInteractable.h"
#include "Blueprint/UserWidget.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/SavaPlayerController.h"
#include "savaCharacter.h"

ASavaInteractable::ASavaInteractable()
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

void ASavaInteractable::BeginPlay()
{
	Super::BeginPlay();

	InteractArea->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleBeginOverlap);
	InteractArea->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleEndOverlap);
}

void ASavaInteractable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HidePrompt();

	Super::EndPlay(EndPlayReason);
}

bool ASavaInteractable::CanInteract(const APlayerController* PlayerController) const
{
	if (!PlayerController)
	{
		return false;
	}

	return !IsHostOnly() || ASavaPlayerController::IsHostController(PlayerController);
}

bool ASavaInteractable::TryInteract(APlayerController* PlayerController)
{
	if (!CanInteract(PlayerController))
	{
		return false;
	}

	OnInteract(PlayerController);
	return true;
}

void ASavaInteractable::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	//自分が操作しているキャラクターだけ(他の人やサーバー上のキャラクターは無視)
	AsavaCharacter* Character = Cast<AsavaCharacter>(OtherActor);
	if (!Character || !Character->IsLocallyControlled())
	{
		return;
	}

	Character->SetNearbyInteractable(this);
	RefreshPrompt(Character->GetController<APlayerController>());
}

void ASavaInteractable::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	AsavaCharacter* Character = Cast<AsavaCharacter>(OtherActor);
	if (!Character || !Character->IsLocallyControlled())
	{
		return;
	}

	Character->ClearNearbyInteractable(this);
	HidePrompt();
}

void ASavaInteractable::RefreshPrompt(APlayerController* PlayerController)
{
	HidePrompt();

	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!Pawn || !InteractArea->IsOverlappingActor(Pawn) || !ShouldShowPrompt())
	{
		return;
	}

	//使えるなら通常の案内、ホスト限定で使えないなら「ホストのみ」の案内
	const bool bUsable = CanInteract(PlayerController);
	const TSubclassOf<UUserWidget> WidgetClass = bUsable ? PromptWidgetClass : HostOnlyPromptWidgetClass;
	TObjectPtr<UUserWidget>& Widget = bUsable ? PromptWidget : HostOnlyPromptWidget;
	if (!WidgetClass)
	{
		return;
	}

	if (!Widget)
	{
		Widget = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
	}
	if (Widget && !Widget->IsInViewport())
	{
		Widget->AddToViewport();
	}
}

void ASavaInteractable::HidePrompt()
{
	if (PromptWidget)
	{
		PromptWidget->RemoveFromParent();
	}
	if (HostOnlyPromptWidget)
	{
		HostOnlyPromptWidget->RemoveFromParent();
	}
}

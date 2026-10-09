// Fill out your copyright notice in the Description page of Project Settings.

#include "CTF/SavaFlagBase.h"
#include "CTF/SavaFlag.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ASavaFlagBase::ASavaFlagBase()
{
	bReplicates = true;
	bAlwaysRelevant = true; //旗の位置を UI に出せるように、どこにいても届ける

	CaptureZone = CreateDefaultSubobject<USphereComponent>(TEXT("CaptureZone"));
	CaptureZone->InitSphereRadius(150.0f);
	CaptureZone->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	CaptureZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CaptureZone->SetCollisionObjectType(ECC_WorldDynamic);
	CaptureZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	CaptureZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CaptureZone->SetGenerateOverlapEvents(true);
	RootComponent = CaptureZone;

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(CaptureZone);
	BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BaseMesh->SetRelativeScale3D(FVector(1.5f, 1.5f, 0.1f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		BaseMesh->SetStaticMesh(CylinderMesh.Object);
	}

	FlagClass = ASavaFlag::StaticClass();
}

void ASavaFlagBase::BeginPlay()
{
	Super::BeginPlay();

	//旗はサーバーだけがスポーンする(クライアントへは複製で届く)
	if (HasAuthority() && FlagClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Flag = GetWorld()->SpawnActor<ASavaFlag>(FlagClass, GetFlagHomeTransform(), SpawnParams);
		if (Flag)
		{
			Flag->InitFlag(this, TeamId);
		}
	}
}

FTransform ASavaFlagBase::GetFlagHomeTransform() const
{
	return FTransform(GetActorRotation(), GetActorLocation() + FVector(0.0f, 0.0f, FlagHeight));
}

bool ASavaFlagBase::IsInCaptureZone(const AActor* Actor) const
{
	return Actor && CaptureZone->IsOverlappingActor(Actor);
}

void ASavaFlagBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASavaFlagBase, Flag);
}

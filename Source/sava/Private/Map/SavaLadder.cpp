// Fill out your copyright notice in the Description page of Project Settings.

#include "Map/SavaLadder.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	//標準の板メッシュの厚さ(梯子の面より裏側に置く)
	constexpr float LadderMeshThickness = 10.0f;
}

ASavaLadder::ASavaLadder()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}

	//キャラクターだけと重なる(移動の邪魔はしない)
	ClimbVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ClimbVolume"));
	ClimbVolume->SetupAttachment(RootComponent);
	ClimbVolume->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	ClimbVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ClimbVolume->SetCollisionObjectType(ECC_WorldDynamic);
	ClimbVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	ClimbVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ClimbVolume->SetGenerateOverlapEvents(false);

#if WITH_EDITORONLY_DATA
	Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (Arrow)
	{
		Arrow->SetupAttachment(RootComponent);
		Arrow->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	}
#endif

	//梯子の板で壁走り・よじ登りが始まらないように(USavaCharacterMovementComponent::NoWallRunTag と同じ名前)
	Tags.Add(TEXT("NoWallRun"));

	UpdateLayout();
}

void ASavaLadder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateLayout();
}

void ASavaLadder::UpdateLayout()
{
	//掴まれる範囲: 梯子の面の前(ClimbVolumeDepth)と裏(ClimbVolumeBackDepth)、下端から上端 + TopGrabHeight まで
	const float Height = LadderHeight + TopGrabHeight;
	ClimbVolume->SetBoxExtent(FVector((ClimbVolumeDepth + ClimbVolumeBackDepth) * 0.5f, LadderWidth * 0.5f, Height * 0.5f));
	ClimbVolume->SetRelativeLocation(FVector((ClimbVolumeDepth - ClimbVolumeBackDepth) * 0.5f, 0.0f, Height * 0.5f));

	if (bFitMeshToSize)
	{
		//標準の立方体(100cm)を、梯子の面の裏側に置いた薄い板にする
		Mesh->SetRelativeLocation(FVector(-LadderMeshThickness * 0.5f, 0.0f, LadderHeight * 0.5f));
		Mesh->SetRelativeScale3D(FVector(LadderMeshThickness, LadderWidth, LadderHeight) / 100.0f);
	}
}

FVector ASavaLadder::GetLadderNormal() const
{
	return GetActorForwardVector().GetSafeNormal2D();
}

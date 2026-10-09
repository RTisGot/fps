// Fill out your copyright notice in the Description page of Project Settings.

#include "CTF/SavaFlag.h"
#include "CTF/SavaCTFGameMode.h"
#include "CTF/SavaFlagBase.h"
#include "AbilitySystem/SavaAbilitySystemLibrary.h"
#include "AbilitySystem/SavaGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "SavaGameplayTags.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "savaCharacter.h"

ASavaFlag::ASavaFlag()
{
	//拾う・持ち帰りの判定をサーバーで毎フレーム行う(旗は 2 本だけなので負荷は小さい)
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(true); //落ちた位置・運び手へのくっつきも複製される
	bAlwaysRelevant = true; //旗の位置を UI に出せるように、どこにいても届ける

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->InitSphereRadius(100.0f);
	PickupSphere->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionObjectType(ECC_WorldDynamic);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupSphere->SetGenerateOverlapEvents(true);
	RootComponent = PickupSphere;

	//見た目は仮(エンジン付属の形)。差し替えるときは BP で Static Mesh を変える
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	PoleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PoleMesh"));
	PoleMesh->SetupAttachment(PickupSphere);
	PoleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PoleMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f));
	PoleMesh->SetRelativeScale3D(FVector(0.06f, 0.06f, 1.5f));
	if (CylinderMesh.Succeeded())
	{
		PoleMesh->SetStaticMesh(CylinderMesh.Object);
	}

	ClothMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClothMesh"));
	ClothMesh->SetupAttachment(PickupSphere);
	ClothMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ClothMesh->SetRelativeLocation(FVector(0.0f, 30.0f, 125.0f));
	ClothMesh->SetRelativeScale3D(FVector(0.03f, 0.6f, 0.4f));
	if (CubeMesh.Succeeded())
	{
		ClothMesh->SetStaticMesh(CubeMesh.Object);
	}

	TeamColors = { FLinearColor(0.05f, 0.25f, 1.0f), FLinearColor(1.0f, 0.1f, 0.05f) };
}

void ASavaFlag::BeginPlay()
{
	Super::BeginPlay();
	UpdateVisuals();
}

void ASavaFlag::InitFlag(ASavaFlagBase* InHomeBase, uint8 InTeamId)
{
	HomeBase = InHomeBase;
	TeamId = InTeamId;
	ReturnToBase();
}

void ASavaFlag::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	//カウントダウン中・結果表示中は何も起きない
	const ASavaCTFGameMode* GameMode = GetWorld()->GetAuthGameMode<ASavaCTFGameMode>();
	if (!GameMode || !GameMode->IsRoundInProgress())
	{
		return;
	}

	switch (FlagState)
	{
	case ESavaFlagState::Carried:
	{
		//切断・奈落などで体が消えたら、すぐ旗台へ戻す
		if (!IsValid(Carrier))
		{
			ReturnToBase();
			return;
		}
		const AsavaCharacter* CarrierCharacter = Cast<AsavaCharacter>(Carrier);
		if (CarrierCharacter && CarrierCharacter->IsDead())
		{
			Drop();
			return;
		}
		CheckCapture();
		break;
	}
	case ESavaFlagState::AtBase:
	case ESavaFlagState::Dropped:
		HandleOverlaps();
		break;
	default:
		break;
	}
}

void ASavaFlag::HandleOverlaps()
{
	TArray<AActor*> OverlappingPawns;
	PickupSphere->GetOverlappingActors(OverlappingPawns, AsavaCharacter::StaticClass());

	for (AActor* Actor : OverlappingPawns)
	{
		AsavaCharacter* Character = Cast<AsavaCharacter>(Actor);
		const uint8 CharacterTeam = USavaAbilitySystemLibrary::GetTeamId(Character);
		if (!Character || Character->IsDead() || CharacterTeam == 255)
		{
			continue;
		}

		if (CharacterTeam != TeamId)
		{
			PickUp(Character);
			return;
		}
		if (FlagState == ESavaFlagState::Dropped)
		{
			ReturnToBase(); //味方が触れたら即座に戻る
			return;
		}
	}
}

void ASavaFlag::CheckCapture()
{
	ASavaCTFGameMode* GameMode = GetWorld()->GetAuthGameMode<ASavaCTFGameMode>();
	const uint8 CarrierTeam = USavaAbilitySystemLibrary::GetTeamId(Carrier);
	const ASavaFlagBase* CarrierHomeBase = GameMode ? GameMode->FindFlagBase(CarrierTeam) : nullptr;
	if (CarrierHomeBase && CarrierHomeBase->IsInCaptureZone(Carrier) && GameMode->CanTeamCapture(CarrierTeam))
	{
		GameMode->NotifyFlagCaptured(this, Carrier);
	}
}

void ASavaFlag::PickUp(APawn* NewCarrier)
{
	GetWorldTimerManager().ClearTimer(DroppedReturnTimer);

	Carrier = NewCarrier;
	FlagState = ESavaFlagState::Carried;
	AttachToComponent(NewCarrier->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeLocation(CarryOffset);

	if (UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(NewCarrier))
	{
		//使用中のスキル(グラップル中など)は止める。以後は State.CarryingFlag で発動できない
		const FGameplayTagContainer SkillTags(SavaGameplayTags::Ability_Type_Skill);
		AbilitySystem->CancelAbilities(&SkillTags);

		const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(USavaGE_CarryingFlag::StaticClass(), 1.0f, AbilitySystem->MakeEffectContext());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(SavaGameplayTags::SetByCaller_MoveSpeedMultiplier, CarrierMoveSpeedMultiplier);
			CarryEffectHandle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			CarrierAbilitySystem = AbilitySystem;
		}
	}

	UpdateVisuals();
}

void ASavaFlag::ReleaseCarrier()
{
	if (UAbilitySystemComponent* AbilitySystem = CarrierAbilitySystem.Get())
	{
		//死亡時は全 GE が外れているので、見つからなくても問題ない
		AbilitySystem->RemoveActiveGameplayEffect(CarryEffectHandle);
	}
	CarrierAbilitySystem.Reset();
	CarryEffectHandle.Invalidate();

	if (Carrier)
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Carrier = nullptr;
	}
}

void ASavaFlag::Drop()
{
	const FVector DropFrom = Carrier ? Carrier->GetActorLocation() : GetActorLocation();
	ReleaseCarrier();

	//真下の地面に置く。地面がない・奈落より下なら旗台へ戻す
	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SavaFlagDrop), false, this);
	const bool bHit = GetWorld()->LineTraceSingleByObjectType(
		Hit, DropFrom, DropFrom - FVector(0.0f, 0.0f, 10000.0f), FCollisionObjectQueryParams(ECC_WorldStatic), QueryParams);
	const AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings();
	if (!bHit || (WorldSettings && WorldSettings->bEnableWorldBoundsChecks && Hit.Location.Z < WorldSettings->KillZ))
	{
		ReturnToBase();
		return;
	}

	SetActorLocationAndRotation(Hit.Location, FRotator(0.0f, GetActorRotation().Yaw, 0.0f));
	FlagState = ESavaFlagState::Dropped;
	if (DroppedReturnTime > 0.0f)
	{
		GetWorldTimerManager().SetTimer(DroppedReturnTimer, this, &ASavaFlag::OnDroppedTimeUp, DroppedReturnTime, false);
	}
	UpdateVisuals();
}

void ASavaFlag::OnDroppedTimeUp()
{
	if (FlagState == ESavaFlagState::Dropped)
	{
		ReturnToBase();
	}
}

void ASavaFlag::ReturnToBase()
{
	GetWorldTimerManager().ClearTimer(DroppedReturnTimer);
	ReleaseCarrier();

	FlagState = ESavaFlagState::AtBase;
	if (HomeBase)
	{
		SetActorTransform(HomeBase->GetFlagHomeTransform());
	}
	UpdateVisuals();
}

void ASavaFlag::Remove()
{
	GetWorldTimerManager().ClearTimer(DroppedReturnTimer);
	ReleaseCarrier();

	FlagState = ESavaFlagState::Removed;
	if (HomeBase)
	{
		SetActorTransform(HomeBase->GetFlagHomeTransform());
	}
	UpdateVisuals();
}

void ASavaFlag::UpdateVisuals()
{
	//消えている間・自分が運んでいる間(1 人称カメラの邪魔になる)は見せない
	const bool bCarriedByMe = FlagState == ESavaFlagState::Carried && Carrier && Carrier->IsLocallyControlled();
	const bool bVisible = FlagState != ESavaFlagState::Removed && !bCarriedByMe;
	PoleMesh->SetVisibility(bVisible);
	ClothMesh->SetVisibility(bVisible);

	//拾えるのは台にある・落ちているときだけ
	const bool bCanPickUp = FlagState == ESavaFlagState::AtBase || FlagState == ESavaFlagState::Dropped;
	PickupSphere->SetCollisionEnabled(bCanPickUp ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

	if (TeamColors.IsValidIndex(TeamId))
	{
		if (UMaterialInstanceDynamic* Material = ClothMesh->CreateDynamicMaterialInstance(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), TeamColors[TeamId]);
		}
	}
}

void ASavaFlag::OnRep_TeamId()
{
	UpdateVisuals();
}

void ASavaFlag::OnRep_FlagState()
{
	UpdateVisuals();
}

void ASavaFlag::OnRep_Carrier()
{
	UpdateVisuals();
}

void ASavaFlag::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASavaFlag, TeamId);
	DOREPLIFETIME(ASavaFlag, FlagState);
	DOREPLIFETIME(ASavaFlag, Carrier);
}

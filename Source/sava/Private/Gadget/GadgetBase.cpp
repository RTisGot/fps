// Fill out your copyright notice in the Description page of Project Settings.

#include "Gadget/GadgetBase.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"


AGadgetBase::AGadgetBase()
{
	PrimaryActorTick.bCanEverTick = false;

	//========================================
	// Network
	//========================================

	bReplicates = true;

	SetReplicateMovement(true);

	//========================================
	// Collision
	//========================================

	m_CollisionComponent =
		CreateDefaultSubobject<USphereComponent>(
			TEXT("GadgetCollision"));

	RootComponent = m_CollisionComponent;

	m_CollisionComponent->SetCollisionEnabled(
		ECollisionEnabled::QueryAndPhysics);

	m_CollisionComponent->SetCollisionObjectType(
		ECC_WorldDynamic);

	m_CollisionComponent->SetCollisionResponseToAllChannels(
		ECR_Block);

	//========================================
	// Projectile Movement
	//========================================

	m_ProjectileMovementComponent =
		CreateDefaultSubobject<UProjectileMovementComponent>(
			TEXT("ProjectileMovement"));

	m_ProjectileMovementComponent->UpdatedComponent =
		m_CollisionComponent;

	m_ProjectileMovementComponent->InitialSpeed =
		m_GadgetData.ThrowSpeed;

	m_ProjectileMovementComponent->MaxSpeed =
		m_GadgetData.ThrowSpeed;

	m_ProjectileMovementComponent->ProjectileGravityScale =
		m_GadgetData.Gravity;

	m_ProjectileMovementComponent->bShouldBounce = false;

	// Spawn直後はまだ投擲しない。
	m_ProjectileMovementComponent->Deactivate();

	m_ProjectileMovementComponent->OnProjectileStop.AddDynamic(
		this,
		&AGadgetBase::OnProjectileStopped);
}


void AGadgetBase::BeginPlay()
{
	Super::BeginPlay();

	//========================================
	// Owner
	//========================================

	/**
	 * GadgetをSpawnするときにAbility側でSetOwner()する。
	 *
	 * ここではそのOwnerを取得して保持する。
	 */
	if (HasAuthority())
	{
		m_GadgetOwner = GetOwner();
	}

	//========================================
	// Projectile Movement Settings
	//========================================

	if (m_ProjectileMovementComponent)
	{
		m_ProjectileMovementComponent->InitialSpeed =
			m_GadgetData.ThrowSpeed;

		m_ProjectileMovementComponent->MaxSpeed =
			m_GadgetData.ThrowSpeed;

		m_ProjectileMovementComponent->ProjectileGravityScale =
			m_GadgetData.Gravity;
	}
}


void AGadgetBase::ThrowGadget(const FVector& Direction)
{
	//========================================
	// Authority
	//========================================

	if (!HasAuthority())
	{
		return;
	}

	//========================================
	// State Check
	//========================================

	if (m_State != EGadgetState::Unused)
	{
		return;
	}

	//========================================
	// Direction Check
	//========================================

	if (Direction.IsNearlyZero())
	{
		return;
	}

	const FVector NormalizedDirection =
		Direction.GetSafeNormal();

	//========================================
	// Projectile Settings
	//========================================

	if (m_ProjectileMovementComponent)
	{
		m_ProjectileMovementComponent->InitialSpeed =
			m_GadgetData.ThrowSpeed;

		m_ProjectileMovementComponent->MaxSpeed =
			m_GadgetData.ThrowSpeed;

		m_ProjectileMovementComponent->ProjectileGravityScale =
			m_GadgetData.Gravity;

		m_ProjectileMovementComponent->Velocity =
			NormalizedDirection * m_GadgetData.ThrowSpeed;

		m_ProjectileMovementComponent->Activate(true);
	}

	//========================================
	// State
	//========================================

	SetGadgetState(EGadgetState::Thrown);

	SetGadgetState(EGadgetState::Flying);
}


void AGadgetBase::OnProjectileStopped(
	const FHitResult& ImpactResult)
{
	//========================================
	// Authority
	//========================================

	if (!HasAuthority())
	{
		return;
	}

	//========================================
	// State Check
	//========================================

	if (m_State != EGadgetState::Flying &&
		m_State != EGadgetState::Thrown)
	{
		return;
	}

	//========================================
	// Stop Movement
	//========================================

	if (m_ProjectileMovementComponent)
	{
		m_ProjectileMovementComponent->StopMovementImmediately();

		m_ProjectileMovementComponent->Deactivate();
	}

	//========================================
	// State
	//========================================

	SetGadgetState(EGadgetState::Landed);

	//========================================
	// Derived Gadget
	//========================================

	OnGadgetLanded(ImpactResult);
}


void AGadgetBase::Detonate()
{
	//========================================
	// Authority
	//========================================

	if (!HasAuthority())
	{
		return;
	}

	//========================================
	// State Check
	//========================================

	if (m_State == EGadgetState::Detonating ||
		m_State == EGadgetState::Destroyed)
	{
		return;
	}

	//========================================
	// Detonating
	//========================================

	SetGadgetState(EGadgetState::Detonating);

	//========================================
	// Effect
	//========================================

	ApplyGadgetEffect();

	//========================================
	// Cleanup
	//========================================

	OnGadgetDestroyed();

	SetGadgetState(EGadgetState::Destroyed);

	Destroy();
}


void AGadgetBase::OnGadgetLanded(
	const FHitResult& Hit)
{
	// Baseでは何もしない。
	//
	// 派生クラスで実装する。
}


void AGadgetBase::ApplyGadgetEffect()
{
	// Baseでは何もしない。
	//
	// 派生クラスで実装する。
}


void AGadgetBase::OnGadgetDestroyed()
{
	// Baseでは何もしない。
	//
	// 必要になったら派生クラスで実装する。
}


void AGadgetBase::SetGadgetState(
	EGadgetState NewState)
{
	if (m_State == NewState)
	{
		return;
	}

	m_State = NewState;
}


void AGadgetBase::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(
		AGadgetBase,
		m_GadgetOwner);

	DOREPLIFETIME(
		AGadgetBase,
		m_State);
}
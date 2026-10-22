// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/SavaEquipmentComponent.h"
#include "Weapon/SavaWeaponData.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "SavaCharacterMovementComponent.h"
#include "SavaGameplayTags.h"
#include "SavaGrappleRootMotionSource.h"
#include "savaCharacter.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"

namespace
{
	int32 ToIndex(ESavaWeaponSlot Slot)
	{
		return static_cast<int32>(Slot);
	}

	//武器の当たり判定の確認用の表示(SavaWeaponFireAbility.cpp で定義している sava.Weapon.Debug を使う)
	bool IsWeaponDebugEnabled()
	{
		static const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("sava.Weapon.Debug"));
		return CVar && CVar->GetBool();
	}

	ESavaWeaponSlot GetOtherSlot(ESavaWeaponSlot Slot)
	{
		return Slot == ESavaWeaponSlot::Primary ? ESavaWeaponSlot::Secondary : ESavaWeaponSlot::Primary;
	}

	bool AreModifiersEqual(const TArray<FSavaWeaponModifier>& A, const TArray<FSavaWeaponModifier>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (!FSavaWeaponModifier::StaticStruct()->CompareScriptStruct(&A[Index], &B[Index], PPF_None))
			{
				return false;
			}
		}
		return true;
	}
}

USavaEquipmentComponent::USavaEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	Loadout.SetNum(2);
}

USavaEquipmentComponent* USavaEquipmentComponent::FindEquipmentComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<USavaEquipmentComponent>() : nullptr;
}

void USavaEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USavaEquipmentComponent, Loadout);
	DOREPLIFETIME_CONDITION(USavaEquipmentComponent, CurrentSlot, COND_SkipOwner);
}

void USavaEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	//1 人称の腕に持たせる武器のメッシュを用意する(本人にだけ見える)
	if (AsavaCharacter* Character = Cast<AsavaCharacter>(GetOwner()))
	{
		BaseFieldOfView = Character->GetFirstPersonCameraComponent()->FieldOfView;

		//WeaponMeshComponent = NewObject<USkeletalMeshComponent>(Character, TEXT("WeaponMesh1P"));
		WeaponMeshComponent = NewObject<UStaticMeshComponent>(Character, TEXT("WeaponMesh1P"));
		WeaponMeshComponent->SetOnlyOwnerSee(true);
		WeaponMeshComponent->bCastDynamicShadow = false;
		WeaponMeshComponent->CastShadow = false;
		WeaponMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WeaponMeshComponent->SetupAttachment(Character->GetMesh1P());
		WeaponMeshComponent->RegisterComponent();
	}

	if (GetOwner()->HasAuthority())
	{
		if (DefaultPrimaryWeapon && !GetWeaponData(ESavaWeaponSlot::Primary))
		{
			SetLoadout(ESavaWeaponSlot::Primary, DefaultPrimaryWeapon, {});
		}
		if (DefaultSecondaryWeapon && !GetWeaponData(ESavaWeaponSlot::Secondary))
		{
			SetLoadout(ESavaWeaponSlot::Secondary, DefaultSecondaryWeapon, {});
		}
	}
	else
	{
		//BeginPlay より先に装備が届いていた場合
		ApplyLoadout();
	}

	UpdateWeaponMesh();
}

void USavaEquipmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//ダッシュ後の射撃の待ち時間用に、最後にダッシュしていた時刻を覚える(自分の画面とサーバーの両方)
	if (IsOwnerSprinting())
	{
		LastSprintTime = GetWorld()->GetTimeSeconds();
	}

	//ADS と反動は、操作している本人の画面だけで動かす
	if (GetLocalPlayerController())
	{
		UpdateADS(DeltaTime);
		UpdateRecoil(DeltaTime);
		UpdateVisualKick(DeltaTime);
	}

	//デバッグ描画(エディタ・ゲーム画面のすべてで可視)
	DrawMuzzleDebug();
}

//--------------------------------Loadout

void USavaEquipmentComponent::SetLoadout(ESavaWeaponSlot Slot, USavaWeaponData* Weapon, const TArray<FSavaWeaponModifier>& Modifiers)
{
	if (!GetOwner()->HasAuthority() || !Loadout.IsValidIndex(ToIndex(Slot)))
	{
		return;
	}

	//同じものを入れ直しても何もしない(弾が満タンに戻らないように)
	FSavaWeaponLoadoutEntry& Entry = Loadout[ToIndex(Slot)];
	if (Entry.Weapon == Weapon && AreModifiersEqual(Entry.Modifiers, Modifiers))
	{
		return;
	}

	Entry.Weapon = Weapon;
	Entry.Modifiers = Modifiers;
	ApplyLoadout();
}

void USavaEquipmentComponent::OnRep_Loadout()
{
	ApplyLoadout();
}

void USavaEquipmentComponent::ApplyLoadout()
{
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SlotStates); ++Index)
	{
		const ESavaWeaponSlot Slot = static_cast<ESavaWeaponSlot>(Index);
		const FSavaWeaponLoadoutEntry* Entry = FindLoadoutEntry(Slot);
		const USavaWeaponData* Weapon = Entry ? Entry->Weapon.Get() : nullptr;
		static const TArray<FSavaWeaponModifier> NoModifiers;
		const TArray<FSavaWeaponModifier>& Modifiers = Entry ? Entry->Modifiers : NoModifiers;

		FSlotState& State = SlotStates[Index];
		if (State.AppliedWeapon == Weapon && AreModifiersEqual(State.AppliedModifiers, Modifiers))
		{
			continue;
		}

		//装備が変わった枠は、数値を計算し直して弾を満タンにする
		State.AppliedWeapon = Weapon;
		State.AppliedModifiers = Modifiers;
		State.Stats = Weapon ? USavaWeaponLibrary::ApplyWeaponModifiers(Weapon->Stats, Modifiers) : FSavaWeaponStats();
		State.AmmoInMagazine = Weapon ? State.Stats.MagazineSize : 0;
		State.ReserveAmmo = Weapon ? State.Stats.MaxReserveAmmo : 0;

		OnWeaponChanged.Broadcast(Slot, const_cast<USavaWeaponData*>(Weapon));
		BroadcastAmmoChanged(Slot);
	}

	UpdateWeaponMesh();
}

void USavaEquipmentComponent::RefillAllAmmo()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	RefillAmmoLocal();
	ClientRefillAllAmmo();
}

void USavaEquipmentComponent::ClientRefillAllAmmo_Implementation()
{
	RefillAmmoLocal();
}

void USavaEquipmentComponent::RefillAmmoLocal()
{
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SlotStates); ++Index)
	{
		FSlotState& State = SlotStates[Index];
		if (!State.AppliedWeapon)
		{
			continue;
		}
		State.AmmoInMagazine = State.Stats.MagazineSize;
		State.ReserveAmmo = State.Stats.MaxReserveAmmo;
		BroadcastAmmoChanged(static_cast<ESavaWeaponSlot>(Index));
	}
}

//--------------------------------State

USavaWeaponData* USavaEquipmentComponent::GetWeaponData(ESavaWeaponSlot Slot) const
{
	const FSavaWeaponLoadoutEntry* Entry = FindLoadoutEntry(Slot);
	return Entry ? Entry->Weapon.Get() : nullptr;
}

FSavaWeaponStats USavaEquipmentComponent::GetStats(ESavaWeaponSlot Slot) const
{
	const FSlotState* State = FindSlotState(Slot);
	return State ? State->Stats : FSavaWeaponStats();
}

int32 USavaEquipmentComponent::GetAmmoInMagazine(ESavaWeaponSlot Slot) const
{
	const FSlotState* State = FindSlotState(Slot);
	return State ? State->AmmoInMagazine : 0;
}

int32 USavaEquipmentComponent::GetReserveAmmo(ESavaWeaponSlot Slot) const
{
	const FSlotState* State = FindSlotState(Slot);
	return State ? State->ReserveAmmo : 0;
}

bool USavaEquipmentComponent::HasInfiniteReserveAmmo(ESavaWeaponSlot Slot) const
{
	const USavaWeaponData* Weapon = GetWeaponData(Slot);
	return Weapon && Weapon->HasInfiniteReserveAmmo();
}

bool USavaEquipmentComponent::CanReload(ESavaWeaponSlot Slot) const
{
	const FSlotState* State = FindSlotState(Slot);
	if (!State || !State->AppliedWeapon || State->AmmoInMagazine >= State->Stats.MagazineSize)
	{
		return false;
	}
	return HasInfiniteReserveAmmo(Slot) || State->ReserveAmmo > 0;
}

bool USavaEquipmentComponent::CanSwapWeapon() const
{
	return GetWeaponData(GetOtherSlot(CurrentSlot)) != nullptr;
}

float USavaEquipmentComponent::GetAimMoveSpeedMultiplier() const
{
	//見た目の ADS の進み具合(ADSAlpha)は自分の画面にしかないので、サーバーでも分かるタグで決める
	const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	const FSlotState* State = FindSlotState(CurrentSlot);
	if (!State || !State->AppliedWeapon || !AbilitySystem || !AbilitySystem->HasMatchingGameplayTag(SavaGameplayTags::State_Weapon_Aiming))
	{
		return 1.0f;
	}
	return State->Stats.ADSMoveSpeed;
}

float USavaEquipmentComponent::GetRemainingEquipTime() const
{
	const FSlotState* State = FindSlotState(CurrentSlot);
	if (!State)
	{
		return 0.0f;
	}
	const double TimeSinceEquip = GetWorld()->GetTimeSeconds() - EquipStartTime;
	return FMath::Max(State->Stats.EquipTime - static_cast<float>(TimeSinceEquip), 0.0f);
}

float USavaEquipmentComponent::GetCurrentSpreadAngle() const
{
	const FSlotState* State = FindSlotState(CurrentSlot);
	if (!State || !State->AppliedWeapon)
	{
		return 0.0f;
	}

	float MoveAlpha = 0.0f;
	bool bInAir = false;
	ESavaSpreadMovement SpreadMovement = ESavaSpreadMovement::Walk;
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr)
	{
		//スキル・ガジェットによる加速/減速も含めた歩く速さで、歩き(加速中はダッシュ)の拡散を全部足す
		const USavaCharacterMovementComponent* SavaMovement = Cast<USavaCharacterMovementComponent>(Movement);
		const float AbilitySpeedMultiplier = SavaMovement ? SavaMovement->GetAbilityMoveSpeedMultiplier() : 1.0f;
		const float WalkSpeed = Movement->MaxWalkSpeed * AbilitySpeedMultiplier;
		if (WalkSpeed > 0.0f)
		{
			MoveAlpha = FMath::Clamp(static_cast<float>(Movement->Velocity.Size2D()) / WalkSpeed, 0.0f, 1.0f);
		}
		bInAir = Movement->IsFalling();

		const bool bGrappling = Movement->CurrentRootMotion.RootMotionSources.ContainsByPredicate([](const TSharedPtr<FRootMotionSource>& Source)
		{
			return Source.IsValid() && Source->InstanceName == FSavaGrappleRootMotionSource::GetPullInstanceName();
		});
		if (bGrappling)
		{
			SpreadMovement = ESavaSpreadMovement::Grapple;
		}
		else if (SavaMovement && SavaMovement->IsSliding())
		{
			SpreadMovement = ESavaSpreadMovement::Slide;
		}
		else if (SavaMovement && (SavaMovement->IsSprinting() || AbilitySpeedMultiplier > 1.0f + KINDA_SMALL_NUMBER))
		{
			SpreadMovement = ESavaSpreadMovement::Sprint;
		}
	}
	return State->Stats.CalculateSpread(ADSAlpha, MoveAlpha, bInAir, GetShotSpread(*State), SpreadMovement);
}

float USavaEquipmentComponent::GetShotSpread(const FSlotState& State) const
{
	return State.Stats.CalculateShotSpread(State.ShotSpread, static_cast<float>(GetWorld()->GetTimeSeconds() - State.ShotSpreadTime));
}

void USavaEquipmentComponent::AddShotSpread(ESavaWeaponSlot Slot)
{
	FSlotState* State = FindSlotState(Slot);
	if (!State)
	{
		return;
	}
	State->ShotSpread = FMath::Min(GetShotSpread(*State) + State->Stats.SpreadPerShot, State->Stats.MaxShotSpread);
	State->ShotSpreadTime = GetWorld()->GetTimeSeconds();
}

//--------------------------------Swap

void USavaEquipmentComponent::SwapWeapon()
{
	if (!CanSwapWeapon())
	{
		return;
	}
	CurrentSlot = GetOtherSlot(CurrentSlot);
	//しまう時間は取らず、出す武器の Equip Time だけ待つ(出している途中に持ち替えても、新しい武器の時間で数え直す)
	EquipStartTime = GetWorld()->GetTimeSeconds();
	OnRep_CurrentSlot();
}

void USavaEquipmentComponent::OnRep_CurrentSlot()
{
	UpdateWeaponMesh();
	OnWeaponChanged.Broadcast(CurrentSlot, GetCurrentWeaponData());
	BroadcastAmmoChanged(CurrentSlot);
}

void USavaEquipmentComponent::UpdateWeaponMesh()
{
	const AsavaCharacter* Character = Cast<AsavaCharacter>(GetOwner());

	if (!WeaponMeshComponent || !Character)
	{
		return;
	}

	const USavaWeaponData* Weapon = GetCurrentWeaponData();
	UStaticMesh* Mesh = Weapon ? Weapon->Mesh.Get() : nullptr;

	WeaponMeshComponent->SetStaticMesh(Mesh);
	WeaponMeshComponent->SetVisibility(Mesh != nullptr);

	if (Weapon && Mesh)
	{
		WeaponMeshComponent->AttachToComponent(
			Character->GetMesh1P(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			Weapon->AttachSocketName);
	}
}

//--------------------------------Ammo

float USavaEquipmentComponent::GetRemainingFireInterval(ESavaWeaponSlot Slot) const
{
	const FSlotState* State = FindSlotState(Slot);
	if (!State)
	{
		return 0.0f;
	}
	const double TimeSinceLastShot = GetWorld()->GetTimeSeconds() - State->LocalLastFireTime;
	return State->Stats.GetFireInterval() - static_cast<float>(TimeSinceLastShot);
}

float USavaEquipmentComponent::GetRemainingSprintToFireTime() const
{
	const FSlotState* State = FindSlotState(CurrentSlot);
	if (!State)
	{
		return 0.0f;
	}
	if (IsOwnerSprinting())
	{
		return State->Stats.SprintToFireTime;
	}
	const double TimeSinceSprint = GetWorld()->GetTimeSeconds() - LastSprintTime;
	return FMath::Max(State->Stats.SprintToFireTime - static_cast<float>(TimeSinceSprint), 0.0f);
}

float USavaEquipmentComponent::GetRemainingReadyTime() const
{
	return FMath::Max(GetRemainingEquipTime(), GetRemainingSprintToFireTime());
}

bool USavaEquipmentComponent::IsOwnerSprinting() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const USavaCharacterMovementComponent* Movement = Character ? Cast<USavaCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	return Movement && Movement->IsSprinting();
}

bool USavaEquipmentComponent::TryConsumeLocalShot(ESavaWeaponSlot Slot)
{
	FSlotState* State = FindSlotState(Slot);
	if (!State || !State->AppliedWeapon || State->AmmoInMagazine <= 0)
	{
		return false;
	}

	--State->AmmoInMagazine;
	State->LocalLastFireTime = GetWorld()->GetTimeSeconds();
	BroadcastAmmoChanged(Slot);
	return true;
}

bool USavaEquipmentComponent::TryConsumeServerShot(ESavaWeaponSlot Slot, float IntervalTolerance)
{
	FSlotState* State = FindSlotState(Slot);
	if (!State || !State->AppliedWeapon || State->AmmoInMagazine <= 0)
	{
		return false;
	}

	--State->AmmoInMagazine;
	BroadcastAmmoChanged(Slot);

	const double Now = GetWorld()->GetTimeSeconds();
	const bool bTooFast = Now - State->ServerLastFireTime < State->Stats.GetFireInterval() * IntervalTolerance;
	//持ち替えた直後(通信で届く時刻がずれるので、間隔と同じだけ余裕を持たせる)
	const bool bEquipping = Now - EquipStartTime < State->Stats.EquipTime * IntervalTolerance;
	//ダッシュをやめた直後(同じく余裕を持たせる)
	const bool bJustSprinted = Now - LastSprintTime < State->Stats.SprintToFireTime * IntervalTolerance;
	State->ServerLastFireTime = Now;
	return !bTooFast && !bEquipping && !bJustSprinted;
}

void USavaEquipmentComponent::FinishReload(ESavaWeaponSlot Slot)
{
	FSlotState* State = FindSlotState(Slot);
	if (!State || !CanReload(Slot))
	{
		return;
	}

	const int32 Needed = State->Stats.MagazineSize - State->AmmoInMagazine;
	const bool bInfinite = HasInfiniteReserveAmmo(Slot);
	const int32 Loaded = bInfinite ? Needed : FMath::Min(Needed, State->ReserveAmmo);
	State->AmmoInMagazine += Loaded;
	if (!bInfinite)
	{
		State->ReserveAmmo -= Loaded;
	}
	BroadcastAmmoChanged(Slot);

	//サーバーの予備弾を本人へ伝える(サーバーが弾かれた弾の分などで、ずれていることがあるため)
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!bInfinite && Pawn && Pawn->HasAuthority() && !Pawn->IsLocallyControlled())
	{
		ClientSyncReserveAmmo(Slot, State->ReserveAmmo);
	}
}

void USavaEquipmentComponent::ClientSyncReserveAmmo_Implementation(ESavaWeaponSlot Slot, int32 ReserveAmmo)
{
	if (FSlotState* State = FindSlotState(Slot))
	{
		State->ReserveAmmo = ReserveAmmo;
		BroadcastAmmoChanged(Slot);
	}
}

void USavaEquipmentComponent::BroadcastAmmoChanged(ESavaWeaponSlot Slot)
{
	OnAmmoChanged.Broadcast(Slot, GetAmmoInMagazine(Slot), GetReserveAmmo(Slot));
}

//--------------------------------ADS / Recoil(自分の画面だけ)

void USavaEquipmentComponent::UpdateADS(float DeltaTime)
{
	const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	//武器を出している途中は構えない(ボタンを押したままなら、出し終わってから構え始める)
	const bool bAiming = AbilitySystem && AbilitySystem->HasMatchingGameplayTag(SavaGameplayTags::State_Weapon_Aiming) && !IsEquipping();
	const FSavaWeaponStats Stats = GetStats(CurrentSlot);

	const float TargetAlpha = bAiming ? 1.0f : 0.0f;
	if (Stats.ADSTime <= 0.0f)
	{
		ADSAlpha = TargetAlpha;
	}
	else
	{
		ADSAlpha = FMath::FInterpConstantTo(ADSAlpha, TargetAlpha, DeltaTime, 1.0f / Stats.ADSTime);
	}

	const AsavaCharacter* Character = Cast<AsavaCharacter>(GetOwner());
	if (Character && BaseFieldOfView > 0.0f)
	{
		Character->GetFirstPersonCameraComponent()->SetFieldOfView(BaseFieldOfView * FMath::Lerp(1.0f, Stats.ADSZoom, ADSAlpha));
	}
}

void USavaEquipmentComponent::ApplyRecoil(ESavaWeaponSlot Slot)
{
	APlayerController* PlayerController = GetLocalPlayerController();
	const FSlotState* State = FindSlotState(Slot);
	if (!PlayerController || !State)
	{
		return;
	}

	//撃つのをしばらくやめた・持ち替えた: パターンの最初から
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastRecoilTime > State->Stats.RecoilResetTime || RecoilSlot != Slot)
	{
		RecoilShotIndex = 0;
	}

	//パターンに沿った量を溜める(少しだけランダムにずらす)。視点へは Recoil Kick Time かけて Tick で足す
	const FVector2D Noise(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f));
	const FVector2D Kick = State->Stats.CalculateRecoilKick(RecoilShotIndex, Noise);
	PendingRecoil += Kick;

	//見た目だけの揺れ: 速さを与えると、Visual Kick Time 後に (速さ / (強さ × e)) まで跳ねてから戻るので、狙った大きさになる速さを足す
	const FVector VisualKick(Kick.Y * State->Stats.VisualKickScale, Kick.X * State->Stats.VisualKickScale,
		FMath::FRandRange(-State->Stats.VisualKickRoll, State->Stats.VisualKickRoll));
	if (!VisualKick.IsNearlyZero())
	{
		VisualKickStiffness = 1.0f / FMath::Max(State->Stats.VisualKickTime, 0.005f);
		VisualKickVelocity += VisualKick * VisualKickStiffness * UE_EULERS_NUMBER;
		bVisualKickActive = true;
	}

	RecoilKickTimeRemaining = State->Stats.RecoilKickTime;
	if (RecoilKickTimeRemaining <= 0.0f)
	{
		AddRecoilToView(PlayerController, PendingRecoil);
		PendingRecoil = FVector2D::ZeroVector;
	}

	LastRecoilTime = Now;
	++RecoilShotIndex;
	RecoilSlot = Slot;
	RecoilRecoveryDelay = State->Stats.GetFireInterval();
	RecoilRecoverySpeed = State->Stats.RecoilRecovery;
}

void USavaEquipmentComponent::UpdateRecoil(float DeltaTime)
{
	APlayerController* PlayerController = GetLocalPlayerController();
	if (!PlayerController)
	{
		return;
	}

	//溜まった反動を足す。残りの割合を (残り時間 / Kick Time)^2 で減らすので、最初は速く、だんだんゆっくりになり、ちょうど Kick Time で足し終わる
	//(撃つたびに残り時間は Kick Time に戻り、前の 1 発の残りもまとめて足す)
	if (RecoilKickTimeRemaining > 0.0f)
	{
		const float NewRemaining = FMath::Max(RecoilKickTimeRemaining - DeltaTime, 0.0f);
		const float RemainingRatio = FMath::Square(NewRemaining / RecoilKickTimeRemaining);
		const FVector2D Step = PendingRecoil * (1.0f - RemainingRatio);
		AddRecoilToView(PlayerController, Step);
		PendingRecoil -= Step;
		RecoilKickTimeRemaining = NewRemaining;
		if (RecoilKickTimeRemaining <= 0.0f)
		{
			PendingRecoil = FVector2D::ZeroVector;
		}
		return;
	}

	if (RecoilPitchToRecover <= 0.0f)
	{
		return;
	}

	//撃ち続けている間は戻さない(反動を足し終え、次の 1 発までの間隔が過ぎたら戻し始める)
	if (GetWorld()->GetTimeSeconds() - LastRecoilTime < RecoilRecoveryDelay)
	{
		return;
	}

	FRotator ControlRotation = PlayerController->GetControlRotation();
	const float CurrentPitch = FRotator::NormalizeAxis(ControlRotation.Pitch);
	AbsorbPlayerPitchInput(CurrentPitch);

	const float Step = FMath::Min(RecoilPitchToRecover, RecoilRecoverySpeed * DeltaTime);
	if (Step <= 0.0f)
	{
		return;
	}
	ControlRotation.Pitch = CurrentPitch - Step;
	PlayerController->SetControlRotation(ControlRotation);

	RecoilPitchToRecover -= Step;
	LastRecoilPitch = ControlRotation.Pitch;
}

void USavaEquipmentComponent::UpdateVisualKick(float DeltaTime)
{
	const AsavaCharacter* Character = Cast<AsavaCharacter>(GetOwner());
	UCameraComponent* Camera = Character ? Character->GetFirstPersonCameraComponent() : nullptr;
	USkeletalMeshComponent* Arms = Character ? Character->GetMesh1P() : nullptr;
	if (!bVisualKickActive || !Camera || !Arms)
	{
		return;
	}

	//揺れ始め: 揺れていないときの腕の位置を覚える
	if (VisualKickOffset.IsZero())
	{
		ArmsBaseTransform = Arms->GetRelativeTransform();
	}

	//行きすぎない一番速いバネ(臨界減衰)の式で進める(フレームの長さによらず同じ動きになる)
	const float W = VisualKickStiffness;
	const float Decay = FMath::Exp(-W * DeltaTime);
	const FVector Temp = VisualKickVelocity + VisualKickOffset * W;
	VisualKickOffset = (VisualKickOffset + Temp * DeltaTime) * Decay;
	VisualKickVelocity = (VisualKickVelocity - Temp * (W * DeltaTime)) * Decay;

	//十分小さくなったら止める
	const bool bSettled = VisualKickOffset.IsNearlyZero(0.001f) && VisualKickVelocity.IsNearlyZero(0.01f);
	if (bSettled)
	{
		VisualKickOffset = FVector::ZeroVector;
		VisualKickVelocity = FVector::ZeroVector;
		bVisualKickActive = false;
	}

	//画面(カメラの見た目だけの回転)と腕を同じだけ回す。視点の向き(Control Rotation)は変えないので、弾の向きは変わらない
	const FTransform Kick(FRotator(VisualKickOffset.X, VisualKickOffset.Y, VisualKickOffset.Z));
	Camera->ClearAdditiveOffset();
	Arms->SetRelativeTransform(bSettled ? ArmsBaseTransform : ArmsBaseTransform * Kick);
	if (!bSettled)
	{
		Camera->AddAdditiveOffset(Kick, 0.0f);
	}
}

void USavaEquipmentComponent::AddRecoilToView(APlayerController* PlayerController, const FVector2D& Kick)
{
	//角度を上書きせず差分を足すので、反動を足している間もプレイヤーの視点操作はそのまま効く
	FRotator ControlRotation = PlayerController->GetControlRotation();
	const float CurrentPitch = FRotator::NormalizeAxis(ControlRotation.Pitch);
	AbsorbPlayerPitchInput(CurrentPitch);

	ControlRotation.Pitch = CurrentPitch + Kick.Y;
	ControlRotation.Yaw += Kick.X;
	PlayerController->SetControlRotation(ControlRotation);

	//下向きの反動の分は、戻す量から引く
	RecoilPitchToRecover = FMath::Max(0.0f, RecoilPitchToRecover + Kick.Y);
	LastRecoilPitch = ControlRotation.Pitch;
}

void USavaEquipmentComponent::AbsorbPlayerPitchInput(float CurrentPitch)
{
	if (RecoilPitchToRecover <= 0.0f)
	{
		return;
	}
	const float PlayerDelta = FRotator::NormalizeAxis(CurrentPitch - LastRecoilPitch);
	if (PlayerDelta < 0.0f)
	{
		RecoilPitchToRecover = FMath::Max(0.0f, RecoilPitchToRecover + PlayerDelta);
	}
}

//--------------------------------Weapon Visual

bool USavaEquipmentComponent::GetCurrentMuzzleLocation(FVector& OutLocation) const
{
	const USavaWeaponData* Weapon = GetCurrentWeaponData();
	if (!Weapon)
	{
		return false;
	}

	USkeletalMeshComponent* FirstPersonMesh = GetFirstPersonMesh();
	if (!FirstPersonMesh)
	{
		return false;
	}

	// 武器メッシュにMuzzleSocketが存在する場合はその位置を優先する
	if (WeaponMeshComponent &&
		WeaponMeshComponent->GetStaticMesh() &&
		!Weapon->MuzzleSocketName.IsNone() &&
		WeaponMeshComponent->DoesSocketExist(Weapon->MuzzleSocketName))
	{
		OutLocation = WeaponMeshComponent->GetSocketLocation(
			Weapon->MuzzleSocketName);

		UE_LOG(
			LogTemp,
			Verbose,
			TEXT("[GetCurrentMuzzleLocation] Using MuzzleSocket: %s"),
			*OutLocation.ToString()
		);

		return true;
	}

	// MuzzleSocketが存在しない場合はMuzzleOffsetを使用する
	if (!FirstPersonMesh->DoesSocketExist(Weapon->AttachSocketName))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[GetCurrentMuzzleLocation] MuzzleSocket and AttachSocket are unavailable.")
		);

		return false;
	}

	const FTransform AttachSocketTransform =
		FirstPersonMesh->GetSocketTransform(Weapon->AttachSocketName);

	OutLocation =
		AttachSocketTransform.GetLocation() +
		AttachSocketTransform.GetRotation().RotateVector(Weapon->MuzzleOffset);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[GetCurrentMuzzleLocation] Using MuzzleOffset fallback: %s"),
		*OutLocation.ToString()
	);

	return true;
}

void USavaEquipmentComponent::SpawnMuzzleFlash(const FVector& MuzzleLocation)
{
	const AActor* OwnerActor = GetOwner();

	const USavaWeaponData* Weapon = GetCurrentWeaponData();

	UNiagaraSystem* MuzzleFlash = Weapon && Weapon->MuzzleFlashEffect ? Weapon->MuzzleFlashEffect.Get() : MuzzleFlashSystem.Get();

	if (!OwnerActor || OwnerActor->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!MuzzleFlash)
	{
		return;
	}

	// 武器メッシュのMuzzleSocketにアタッチする
	if (WeaponMeshComponent && WeaponMeshComponent->GetStaticMesh() && WeaponMeshComponent->DoesSocketExist(TEXT("MuzzleSocket")))
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			MuzzleFlash,
			WeaponMeshComponent,
			TEXT("MuzzleSocket"),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			true
		);

		return;
	}

	// MuzzleSocketがない場合は座標計算による位置を使用する
	UNiagaraComponent* NiagaraComponent =
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			MuzzleFlash,
			MuzzleLocation,
			FRotator::ZeroRotator,
			FVector(1.0f),
			true,
			true,
			ENCPoolMethod::None
		);

	if (!NiagaraComponent)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[MuzzleFlash] NiagaraComponent creation failed.")
		);
	}
}

void USavaEquipmentComponent::SpawnTracerEffect(const FVector& MuzzleLocation, const TArray<FVector>& TraceEnds)
{
	const AActor* OwnerActor = GetOwner();
	const USavaWeaponData* Weapon = GetCurrentWeaponData();

	// サーバー専用環境では視覚エフェクトを生成しない
	if (!OwnerActor || OwnerActor->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 武器ごとに設定された弾道エフェクトを取得する
	if (!Weapon || !Weapon->TracerEffect)
	{
		return;
	}

	// 終点が存在しない場合は生成しない
	if (TraceEnds.IsEmpty())
	{
		return;
	}

	// Niagaraのユーザーパラメータを設定するため自動再生を無効にして生成する
	UNiagaraComponent* NiagaraComponent =
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			Weapon->TracerEffect.Get(),
			MuzzleLocation,
			FRotator::ZeroRotator,
			FVector(1.0f),
			true,
			false,
			ENCPoolMethod::None
		);

	if (!NiagaraComponent)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Tracer] Failed to create NiagaraComponent.")
		);

		return;
	}

	// NiagaraのUserパラメータに銃口位置を渡す
	NiagaraComponent->SetVariableVec3(FName(TEXT("User.MuzzlePosition")), MuzzleLocation);

	// NiagaraのImpactPositions配列に弾道の終点を渡す
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(NiagaraComponent, FName(TEXT("User.ImpactPositions")), TraceEnds);

	// 発砲トリガーを有効にする
	NiagaraComponent->SetVariableBool(FName(TEXT("User.Trigger")), true);

	// パラメータを設定してから再生する
	NiagaraComponent->Activate(true);
}

void USavaEquipmentComponent::SpawnImpactDecals(const TArray<FVector>& TraceEnds, const TArray<FVector>& ImpactNormals)
{
	const AActor* OwnerActor = GetOwner();
	const USavaWeaponData* Weapon = GetCurrentWeaponData();

	// サーバー専用環境では生成しない
	if (!OwnerActor || OwnerActor->GetNetMode() == NM_DedicatedServer || !Weapon)
	{
		return;
	}

	const bool bDebug = IsWeaponDebugEnabled();

	// 弾痕が未設定の武器では生成しない
	if (!Weapon->ImpactDecal)
	{
		if (bDebug && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
				FString::Printf(TEXT("[ImpactDecal] %s: Impact Decal is not set"), *GetNameSafe(Weapon)));
		}
		return;
	}

	const float HalfSize = Weapon->ImpactDecalSize * 0.5f;
	const FVector DecalExtent(2.0f, HalfSize, HalfSize);
	int32 NumSpawned = 0;
	int32 NumSkipped = 0;

	const int32 Num = FMath::Min(TraceEnds.Num(), ImpactNormals.Num());
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const FVector& Normal = ImpactNormals[Index];
		if (Normal.IsNearlyZero())
		{
			++NumSkipped;
			continue;
		}

		// デカールは X 軸の向きに投影されるので、壁の中へ向ける。回転(Roll)をランダムにして同じ見た目が並ばないようにする
		FRotator Rotation = (-Normal).Rotation();
		Rotation.Roll = FMath::FRandRange(0.0f, 360.0f);

		// X = 投影の深さ(薄くして裏側の物に貼られないようにする)、Y・Z = 大きさ(半径)
		UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
			GetWorld(),
			Weapon->ImpactDecal,
			DecalExtent,
			TraceEnds[Index],
			Rotation,
			Weapon->ImpactDecalLifeSpan
		);

		if (Decal)
		{
			// 遠くて小さく見えるときは描かない(数が増えたときの負荷対策)
			Decal->SetFadeScreenSize(0.001f);
			++NumSpawned;
		}

		// デカールの範囲(橙の箱)と壁の向き(水色の矢印)
		if (bDebug)
		{
			DrawDebugBox(GetWorld(), TraceEnds[Index], DecalExtent, Rotation.Quaternion(), FColor::Orange, false, 3.0f);
			DrawDebugDirectionalArrow(GetWorld(), TraceEnds[Index], TraceEnds[Index] + Normal * 20.0f, 5.0f, FColor::Cyan, false, 3.0f);
		}
	}

	if (bDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
			FString::Printf(TEXT("[ImpactDecal] %s: spawned %d, skipped %d (miss / character / shield), material %s"),
				*GetNameSafe(Weapon), NumSpawned, NumSkipped, *GetNameSafe(Weapon->ImpactDecal)));
	}
}

void USavaEquipmentComponent::DrawMuzzleDebug() const
{
	if (!bDrawMuzzleDebug)
	{
		return;
	}

	FVector MuzzleLocation;
	if (GetCurrentMuzzleLocation(MuzzleLocation))
	{
		// 現在の武器を描画色で表示
		const USavaWeaponData* WeaponData = GetCurrentWeaponData();
		FColor DrawColor = WeaponData ? WeaponData->MuzzleFlashColor : FColor::Red;

		// マズル位置を球で描画
		DrawDebugSphere(GetWorld(), MuzzleLocation, 10.0f, 12, DrawColor, false, -1.0f, 0, 2.0f);

		// マズル位置から前方へ短い線を描画(方向確認用)
		if (WeaponData)
		{
			const FRotator ViewRotation = GetFirstPersonMesh()->GetSocketTransform(WeaponData->AttachSocketName).GetRotation().Rotator();
			const FVector Forward = ViewRotation.Vector();
		}
	}
}

void USavaEquipmentComponent::NotifyWeaponFireVisual(
	const FVector& MuzzleLocation,
	const TArray<FVector>& TraceEnds,
	const TArray<FVector>& ImpactNormals)
{
	//発砲者自身にはネットワークを待たず、即座に演出を表示する。
	OnWeaponMuzzleFlashEvent.Broadcast(MuzzleLocation);
	OnWeaponTracerEvent.Broadcast(MuzzleLocation, TraceEnds);

	SpawnMuzzleFlash(MuzzleLocation);
	SpawnTracerEffect(MuzzleLocation, TraceEnds);
	SpawnImpactDecals(TraceEnds, ImpactNormals);

	TArray<FVector_NetQuantize> QuantizedTraceEnds;
	QuantizedTraceEnds.Reserve(TraceEnds.Num());

	for (const FVector& TraceEnd : TraceEnds)
	{
		QuantizedTraceEnds.Add(TraceEnd);
	}

	TArray<FVector_NetQuantizeNormal> QuantizedImpactNormals;
	QuantizedImpactNormals.Reserve(ImpactNormals.Num());

	for (const FVector& ImpactNormal : ImpactNormals)
	{
		QuantizedImpactNormals.Add(ImpactNormal);
	}

	if (GetOwner()->HasAuthority())
	{
		//Listen Serverの場合は、そのまま全クライアントへ通知する。
		MulticastWeaponFireVisual(
			FVector_NetQuantize(MuzzleLocation),
			QuantizedTraceEnds,
			QuantizedImpactNormals);
	}
	else
	{
		//クライアントの場合は、サーバーへ発砲演出を通知する。
		ServerNotifyWeaponFireVisual(
			FVector_NetQuantize(MuzzleLocation),
			QuantizedTraceEnds,
			QuantizedImpactNormals);
	}
}

void USavaEquipmentComponent::ServerNotifyWeaponFireVisual_Implementation(
	FVector_NetQuantize MuzzleLocation,
	const TArray<FVector_NetQuantize>& TraceEnds,
	const TArray<FVector_NetQuantizeNormal>& ImpactNormals)
{
	//視覚演出だけを全クライアントへ通知する。
	MulticastWeaponFireVisual(MuzzleLocation, TraceEnds, ImpactNormals);
}

void USavaEquipmentComponent::MulticastWeaponFireVisual_Implementation(
	FVector_NetQuantize MuzzleLocation,
	const TArray<FVector_NetQuantize>& TraceEnds,
	const TArray<FVector_NetQuantizeNormal>& ImpactNormals)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());

	//発砲者自身は既にローカルで即時再生している。
	//ここで再生すると二重表示になるためスキップする。
	if (Pawn && Pawn->IsLocallyControlled())
	{
		return;
	}

	TArray<FVector> ConvertedTraceEnds;
	ConvertedTraceEnds.Reserve(TraceEnds.Num());

	for (const FVector_NetQuantize& TraceEnd : TraceEnds)
	{
		ConvertedTraceEnds.Add(FVector(TraceEnd));
	}

	TArray<FVector> ConvertedImpactNormals;
	ConvertedImpactNormals.Reserve(ImpactNormals.Num());

	for (const FVector_NetQuantizeNormal& ImpactNormal : ImpactNormals)
	{
		ConvertedImpactNormals.Add(FVector(ImpactNormal));
	}

	OnWeaponMuzzleFlashEvent.Broadcast(FVector(MuzzleLocation));
	OnWeaponTracerEvent.Broadcast(FVector(MuzzleLocation), ConvertedTraceEnds);

	SpawnMuzzleFlash(MuzzleLocation);
	SpawnTracerEffect(FVector(MuzzleLocation), ConvertedTraceEnds);
	SpawnImpactDecals(ConvertedTraceEnds, ConvertedImpactNormals);
}

//--------------------------------Helpers

USkeletalMeshComponent* USavaEquipmentComponent::GetFirstPersonMesh() const
{
	const AsavaCharacter* Character = Cast<AsavaCharacter>(GetOwner());
	return Character ? Character->GetMesh1P() : nullptr;
}

APlayerController* USavaEquipmentComponent::GetLocalPlayerController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled() ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
}

USavaEquipmentComponent::FSlotState* USavaEquipmentComponent::FindSlotState(ESavaWeaponSlot Slot)
{
	const int32 Index = ToIndex(Slot);
	return Index >= 0 && Index < UE_ARRAY_COUNT(SlotStates) ? &SlotStates[Index] : nullptr;
}

const USavaEquipmentComponent::FSlotState* USavaEquipmentComponent::FindSlotState(ESavaWeaponSlot Slot) const
{
	return const_cast<USavaEquipmentComponent*>(this)->FindSlotState(Slot);
}

const FSavaWeaponLoadoutEntry* USavaEquipmentComponent::FindLoadoutEntry(ESavaWeaponSlot Slot) const
{
	const int32 Index = ToIndex(Slot);
	return Loadout.IsValidIndex(Index) ? &Loadout[Index] : nullptr;
}

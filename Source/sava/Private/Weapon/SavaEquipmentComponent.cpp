// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/SavaEquipmentComponent.h"
#include "Weapon/SavaWeaponData.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "SavaGameplayTags.h"
#include "savaCharacter.h"

namespace
{
	int32 ToIndex(ESavaWeaponSlot Slot)
	{
		return static_cast<int32>(Slot);
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

	//ADS と反動は、操作している本人の画面だけで動かす
	if (GetLocalPlayerController())
	{
		UpdateADS(DeltaTime);
		UpdateRecoilRecovery(DeltaTime);
	}
}

//--------------------------------Loadout

void USavaEquipmentComponent::SetLoadout(ESavaWeaponSlot Slot, USavaWeaponData* Weapon, const TArray<FSavaWeaponModifier>& Modifiers)
{
	if (!GetOwner()->HasAuthority() || !Loadout.IsValidIndex(ToIndex(Slot)))
	{
		return;
	}

	FSavaWeaponLoadoutEntry& Entry = Loadout[ToIndex(Slot)];
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

float USavaEquipmentComponent::GetCurrentSpreadAngle() const
{
	const FSlotState* State = FindSlotState(CurrentSlot);
	if (!State || !State->AppliedWeapon)
	{
		return 0.0f;
	}

	float MoveAlpha = 0.0f;
	bool bInAir = false;
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr)
	{
		//歩く速さで Movement Spread を全部足す
		if (Movement->MaxWalkSpeed > 0.0f)
		{
			MoveAlpha = FMath::Clamp(static_cast<float>(Movement->Velocity.Size2D()) / Movement->MaxWalkSpeed, 0.0f, 1.0f);
		}
		bInAir = Movement->IsFalling();
	}
	return State->Stats.CalculateSpread(ADSAlpha, MoveAlpha, bInAir);
}

//--------------------------------Swap

void USavaEquipmentComponent::SwapWeapon()
{
	if (!CanSwapWeapon())
	{
		return;
	}
	CurrentSlot = GetOtherSlot(CurrentSlot);
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
	State->ServerLastFireTime = Now;
	return !bTooFast;
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
	const bool bAiming = AbilitySystem && AbilitySystem->HasMatchingGameplayTag(SavaGameplayTags::State_Weapon_Aiming);
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

	FRotator ControlRotation = PlayerController->GetControlRotation();
	const float CurrentPitch = FRotator::NormalizeAxis(ControlRotation.Pitch);
	AbsorbPlayerPitchInput(CurrentPitch);

	//上へは 0 〜 Vertical Recoil、左右へは ±Horizontal Recoil だけランダムに動かす
	const float KickUp = FMath::FRandRange(0.0f, State->Stats.VerticalRecoil);
	const float KickSide = FMath::FRandRange(-State->Stats.HorizontalRecoil, State->Stats.HorizontalRecoil);
	ControlRotation.Pitch = CurrentPitch + KickUp;
	ControlRotation.Yaw += KickSide;
	PlayerController->SetControlRotation(ControlRotation);

	RecoilPitchToRecover += KickUp;
	LastRecoilPitch = ControlRotation.Pitch;
	LastRecoilTime = GetWorld()->GetTimeSeconds();
	RecoilRecoveryDelay = State->Stats.GetFireInterval();
	RecoilRecoverySpeed = State->Stats.RecoilRecovery;
}

void USavaEquipmentComponent::UpdateRecoilRecovery(float DeltaTime)
{
	APlayerController* PlayerController = GetLocalPlayerController();
	if (!PlayerController || RecoilPitchToRecover <= 0.0f)
	{
		return;
	}

	//撃ち続けている間は戻さない(次の 1 発までの間隔が過ぎたら戻し始める)
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
	if (!WeaponMeshComponent || !WeaponMeshComponent->GetStaticMesh())
	{
		return false;
	}

	static const FName MuzzleSocketName(TEXT("Muzzle"));

	// 銃口ソケットが存在しない場合は取得できない。
	if (!WeaponMeshComponent->DoesSocketExist(MuzzleSocketName))
	{
		return false;
	}

	OutLocation = WeaponMeshComponent->GetSocketLocation(MuzzleSocketName);

	return true;
}

void USavaEquipmentComponent::NotifyWeaponFireVisual(
	const FVector& MuzzleLocation,
	const TArray<FVector>& TraceEnds)
{
	//発砲者自身にはネットワークを待たず、即座に演出を表示する。
	OnWeaponMuzzleFlashEvent.Broadcast(MuzzleLocation);
	OnWeaponTracerEvent.Broadcast(MuzzleLocation, TraceEnds);

	TArray<FVector_NetQuantize> QuantizedTraceEnds;
	QuantizedTraceEnds.Reserve(TraceEnds.Num());

	for (const FVector& TraceEnd : TraceEnds)
	{
		QuantizedTraceEnds.Add(TraceEnd);
	}

	if (GetOwner()->HasAuthority())
	{
		//Listen Serverの場合は、そのまま全クライアントへ通知する。
		MulticastWeaponFireVisual(
			FVector_NetQuantize(MuzzleLocation),
			QuantizedTraceEnds);
	}
	else
	{
		//クライアントの場合は、サーバーへ発砲演出を通知する。
		ServerNotifyWeaponFireVisual(
			FVector_NetQuantize(MuzzleLocation),
			QuantizedTraceEnds);
	}
}

void USavaEquipmentComponent::ServerNotifyWeaponFireVisual_Implementation(
	FVector_NetQuantize MuzzleLocation,
	const TArray<FVector_NetQuantize>& TraceEnds)
{
	//視覚演出だけを全クライアントへ通知する。
	MulticastWeaponFireVisual(MuzzleLocation, TraceEnds);
}

void USavaEquipmentComponent::MulticastWeaponFireVisual_Implementation(
	FVector_NetQuantize MuzzleLocation,
	const TArray<FVector_NetQuantize>& TraceEnds)
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

	OnWeaponMuzzleFlashEvent.Broadcast(FVector(MuzzleLocation));
	OnWeaponTracerEvent.Broadcast(FVector(MuzzleLocation), ConvertedTraceEnds);
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

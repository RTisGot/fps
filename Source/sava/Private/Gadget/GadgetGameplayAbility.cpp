#include "Gadget/GadgetGameplayAbility.h"

#include "Gadget/GadgetBase.h"
#include "savaCharacter.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UGadgetGameplayAbility::UGadgetGameplayAbility()
{
	ActivationPolicy = ESavaAbilityActivationPolicy::OnInputTriggered;
}

void UGadgetGameplayAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo)
	{
		return;
	}

	AsavaCharacter* Character = GetSavaCharacterFromActorInfo();
	if (!Character || !GadgetClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	/*
	 * コストとクールダウンを確定する。
	 *
	 * USavaGameplayAbility側で、
	 * MaxCharges > 0 の場合は GadgetChargeCost が設定され、
	 * CooldownDuration / CooldownTags もここで処理される。
	 */
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	/*
	 * ガジェットActorの生成はサーバーだけが行う。
	 *
	 * LocalPredictedのAbilityなので、クライアント側でも
	 * Ability自体は予測されるが、ゲームプレイActorの生成は
	 * サーバーを正とする。
	 */
	if (HasAuthority(&ActivationInfo))
	{
		UCameraComponent* Camera = Character->GetFirstPersonCameraComponent();
		if (Camera)
		{
			const FVector CameraLocation = Camera->GetComponentLocation();
			const FVector CameraForward = Camera->GetForwardVector();

			const FVector SpawnLocation =
				CameraLocation + CameraForward * SpawnForwardOffset;

			const FRotator SpawnRotation = Camera->GetComponentRotation();

			SpawnGadget(Character, SpawnLocation, SpawnRotation);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

AGadgetBase* UGadgetGameplayAbility::SpawnGadget(
	AsavaCharacter* Character,
	const FVector& SpawnLocation,
	const FRotator& SpawnRotation) const
{
	if (!Character || !GadgetClass)
	{
		return nullptr;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Character;
	SpawnParameters.Instigator = Character;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AGadgetBase* Gadget = World->SpawnActor<AGadgetBase>(
		GadgetClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParameters);

	if (!Gadget)
	{
		return nullptr;
	}

	/*
	 * OwnerはSpawn時にも設定しているが、
	 * GadgetBase側のGameplay用Owner情報を確実に
	 * Characterへ紐付ける。
	 */
	Gadget->SetOwner(Character);

	Gadget->ThrowGadget(SpawnRotation.Vector());

	return Gadget;
}
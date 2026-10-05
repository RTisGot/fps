#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/SavaGameplayAbility.h"
#include "GadgetGameplayAbility.generated.h"

class AGadgetBase;

UCLASS()
class SAVA_API UGadgetGameplayAbility : public USavaGameplayAbility
{
	GENERATED_BODY()

public:
	UGadgetGameplayAbility();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, Category = "Gadget")
	TSubclassOf<AGadgetBase> GadgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Gadget|Throw", meta = (ClampMin = "0"))
	float SpawnForwardOffset = 30.0f;

private:
	AGadgetBase* SpawnGadget(
		AsavaCharacter* Character,
		const FVector& SpawnLocation,
		const FRotator& SpawnRotation) const;
};
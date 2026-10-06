#pragma once

#include "CoreMinimal.h"
#include "Gadget/GadgetBase.h"
#include "FragGrenade.generated.h"

UCLASS()
class SAVA_API AFragGrenade : public AGadgetBase
{
	GENERATED_BODY()

public:
	AFragGrenade();

protected:
	virtual void OnGadgetLanded(const FHitResult& Hit) override;
	virtual void ApplyGadgetEffect() override;
};
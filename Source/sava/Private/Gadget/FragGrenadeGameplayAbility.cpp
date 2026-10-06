#include "Gadget/FragGrenadeGameplayAbility.h"

#include "Gadget/FragGrenade.h"

UFragGrenadeGameplayAbility::UFragGrenadeGameplayAbility()
{
	GadgetClass = AFragGrenade::StaticClass();
	MaxCharges = 3;
}
// Fill out your copyright notice in the Description page of Project Settings.


#include "SavaGrappleAbility.h"


USavaGrappleAbility::USavaGrappleAbility()

{

    InstancingPolicy =

        EGameplayAbilityInstancingPolicy::InstancedPerActor;

    NetExecutionPolicy =

        EGameplayAbilityNetExecutionPolicy::LocalPredicted;

}

void USavaGrappleAbility::ActivateAbility(

    const FGameplayAbilitySpecHandle Handle,

    const FGameplayAbilityActorInfo* ActorInfo,

    const FGameplayAbilityActivationInfo ActivationInfo,

    const FGameplayEventData* TriggerEventData)

{

    Super::ActivateAbility(

        Handle,

        ActorInfo,

        ActivationInfo,

        TriggerEventData

    );

    UE_LOG(LogTemp, Warning,

        TEXT("GRAPPLE ABILITY ACTIVATED"));

    EndAbility(

        Handle,

        ActorInfo,

        ActivationInfo,

        true,

        false

    );

}

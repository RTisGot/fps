#include "Skill/SavaShieldAbility.h"

#include "Skill/SavaDomeShield.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "NativeGameplayTags.h"
#include "SavaGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Shield, "Cooldown.Skill.Shield");

USavaShieldAbility::USavaShieldAbility()
{
	ActivationPolicy = ESavaAbilityActivationPolicy::OnInputTriggered;
	CooldownDuration = FScalableFloat(20.0f);
	CooldownTags.AddTag(TAG_Cooldown_Shield);
	ShieldClass = ASavaDomeShield::StaticClass();

	// 能力の種類
	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(SavaGameplayTags::Ability_Type_Skill);
	SetAssetTags(Tags);
}

void USavaShieldAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !ShieldClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// クールダウン開始(クールダウン中ならここで失敗して発動しない)
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// シールドの Spawn はサーバーだけ(クライアントには同期されて現れる)
	if (HasAuthority(&ActivationInfo))
	{
		SpawnShield(Avatar);
	}

	// 展開したら能力はすぐ終わる(シールドは自分の時間で消える)
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void USavaShieldAbility::SpawnShield(AActor* Avatar) const
{
	UWorld* World = Avatar->GetWorld();
	if (!World)
	{
		return;
	}

	if (bReplaceExistingShield)
	{
		for (TActorIterator<ASavaDomeShield> It(World); It; ++It)
		{
			if (It->GetOwner() == Avatar)
			{
				It->Destroy();
			}
		}
	}

	// ドームの中心 = 足元(カプセルの一番下)
	FVector Location = Avatar->GetActorLocation();
	if (const ACharacter* Character = Cast<ACharacter>(Avatar))
	{
		Location.Z -= Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}

	FActorSpawnParameters SpawnParameters;
	// Owner: 展開した人(チーム判定や「自分のシールド」の判定に使う)
	SpawnParameters.Owner = Avatar;
	SpawnParameters.Instigator = Cast<APawn>(Avatar);
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	World->SpawnActor<ASavaDomeShield>(ShieldClass, Location, FRotator::ZeroRotator, SpawnParameters);
}

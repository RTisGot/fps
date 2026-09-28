#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystem/SavaAbilitySystemComponent.h"
#include "AbilitySystem/SavaAbilitySystemLibrary.h"
#include "AbilitySystem/SavaAttributeSet.h"
#include "AbilitySystem/SavaGameplayEffects.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Player/SavaPlayerState.h"
#include "SavaGameplayTags.h"

namespace
{
	//テスト用の一時的なワールド(スコープを抜けると破棄する)
	struct FSavaTestWorld
	{
		UWorld* World = nullptr;

		FSavaTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FSavaTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		ASavaPlayerState* SpawnPlayer(uint8 TeamId) const
		{
			ASavaPlayerState* PlayerState = World->SpawnActor<ASavaPlayerState>();
			PlayerState->SetTeamId(TeamId);
			PlayerState->GetSavaAbilitySystemComponent()->InitAbilityActorInfo(PlayerState, PlayerState);
			return PlayerState;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaDamagePipelineTest, "Sava.AbilitySystem.DamagePipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaDamagePipelineTest::RunTest(const FString&)
{
	FSavaTestWorld TestWorld;
	ASavaPlayerState* Attacker = TestWorld.SpawnPlayer(0);
	ASavaPlayerState* Victim = TestWorld.SpawnPlayer(1);
	USavaAbilitySystemComponent* VictimASC = Victim->GetSavaAbilitySystemComponent();
	const USavaAttributeSet* Attributes = Victim->GetAttributeSet();

	TestTrue(TEXT("AttributeSet is registered"), VictimASC->GetSet<USavaAttributeSet>() == Attributes);
	TestEqual(TEXT("Initial health"), Attributes->GetHealth(), 100.0f);
	TestEqual(TEXT("Initial move speed multiplier"), Attributes->GetMoveSpeedMultiplier(), 1.0f);

	int32 OutOfHealthCount = 0;
	Attributes->OnOutOfHealth.AddLambda([&OutOfHealthCount](AActor*, AActor*, float) { ++OutOfHealthCount; });

	//HP 100 に 10 ダメージ → HP 90
	TestTrue(TEXT("Damage applied"), USavaAbilitySystemLibrary::ApplyDamage(Attacker, Victim, 10.0f, Attacker));
	TestEqual(TEXT("Damage reduces health"), Attributes->GetHealth(), 90.0f);
	TestEqual(TEXT("Damage meta attribute is reset"), Attributes->GetDamage(), 0.0f);

	//回復は最大 HP を超えない
	TestTrue(TEXT("Healing applied"), USavaAbilitySystemLibrary::ApplyHealing(Attacker, Victim, 50.0f));
	TestEqual(TEXT("Healing is clamped to max health"), Attributes->GetHealth(), 100.0f);

	//HP 0 の通知は 1 回だけ
	USavaAbilitySystemLibrary::ApplyDamage(Attacker, Victim, 500.0f, Attacker);
	USavaAbilitySystemLibrary::ApplyDamage(Attacker, Victim, 10.0f, Attacker);
	TestEqual(TEXT("Health does not go below zero"), Attributes->GetHealth(), 0.0f);
	TestEqual(TEXT("Out of health fires once"), OutOfHealthCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaCooldownEffectTest, "Sava.AbilitySystem.CooldownEffect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaCooldownEffectTest::RunTest(const FString&)
{
	FSavaTestWorld TestWorld;
	ASavaPlayerState* Player = TestWorld.SpawnPlayer(0);
	USavaAbilitySystemComponent* ASC = Player->GetSavaAbilitySystemComponent();

	//USavaGameplayAbility::ApplyCooldown と同じ組み立て方
	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(USavaGE_Cooldown::StaticClass(), 1.0f, ASC->MakeEffectContext());
	SpecHandle.Data->DynamicGrantedTags.AddTag(SavaGameplayTags::Cooldown);
	SpecHandle.Data->SetSetByCallerMagnitude(SavaGameplayTags::SetByCaller_Cooldown, 5.0f);
	const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	TestTrue(TEXT("Cooldown effect is active"), Handle.IsValid());
	TestTrue(TEXT("Cooldown tag is granted"), ASC->HasMatchingGameplayTag(SavaGameplayTags::Cooldown));
	const FActiveGameplayEffect* ActiveEffect = ASC->GetActiveGameplayEffect(Handle);
	TestTrue(TEXT("Cooldown duration comes from SetByCaller"), ActiveEffect && FMath::IsNearlyEqual(ActiveEffect->GetDuration(), 5.0f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaTeamTest, "Sava.AbilitySystem.Teams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaTeamTest::RunTest(const FString&)
{
	FSavaTestWorld TestWorld;
	ASavaPlayerState* Red1 = TestWorld.SpawnPlayer(0);
	ASavaPlayerState* Red2 = TestWorld.SpawnPlayer(0);
	ASavaPlayerState* Blue = TestWorld.SpawnPlayer(1);
	ASavaPlayerState* NoTeamA = TestWorld.SpawnPlayer(ASavaPlayerState::NoTeam);
	ASavaPlayerState* NoTeamB = TestWorld.SpawnPlayer(ASavaPlayerState::NoTeam);

	TestFalse(TEXT("Same team are allies"), USavaAbilitySystemLibrary::AreEnemies(Red1, Red2));
	TestTrue(TEXT("Different teams are enemies"), USavaAbilitySystemLibrary::AreEnemies(Red1, Blue));
	TestFalse(TEXT("Self is not an enemy"), USavaAbilitySystemLibrary::AreEnemies(NoTeamA, NoTeamA));
	TestTrue(TEXT("Unassigned players are enemies"), USavaAbilitySystemLibrary::AreEnemies(NoTeamA, NoTeamB));

	return true;
}

#endif

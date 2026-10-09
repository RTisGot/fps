#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystem/SavaAbilitySystemComponent.h"
#include "AbilitySystem/SavaAttributeSet.h"
#include "AbilitySystem/SavaGameplayEffects.h"
#include "CTF/SavaGameState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Player/SavaPlayerState.h"
#include "SavaGameplayTags.h"
#include "SavaGrappleAbility.h"
#include "Skill/SavaShieldAbility.h"

namespace
{
	//テスト用の一時的なワールド(スコープを抜けると破棄する)
	struct FSavaCTFTestWorld
	{
		UWorld* World = nullptr;

		FSavaCTFTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FSavaCTFTestWorld()
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

	//ASavaFlag::PickUp と同じ組み立て方
	FActiveGameplayEffectHandle ApplyCarryingFlag(UAbilitySystemComponent* ASC, float SpeedMultiplier)
	{
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(USavaGE_CarryingFlag::StaticClass(), 1.0f, ASC->MakeEffectContext());
		Spec.Data->SetSetByCallerMagnitude(SavaGameplayTags::SetByCaller_MoveSpeedMultiplier, SpeedMultiplier);
		return ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaCarryingFlagEffectTest, "Sava.CTF.CarryingFlagEffect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaCarryingFlagEffectTest::RunTest(const FString&)
{
	FSavaCTFTestWorld TestWorld;
	ASavaPlayerState* Player = TestWorld.SpawnPlayer(0);
	USavaAbilitySystemComponent* ASC = Player->GetSavaAbilitySystemComponent();
	const USavaAttributeSet* Attributes = Player->GetAttributeSet();

	const FActiveGameplayEffectHandle Handle = ApplyCarryingFlag(ASC, 0.85f);
	TestTrue(TEXT("Carrying effect is active"), Handle.IsValid());
	TestTrue(TEXT("Carrying tag is granted"), ASC->HasMatchingGameplayTag(SavaGameplayTags::State_CarryingFlag));
	TestEqual(TEXT("Move speed is reduced"), Attributes->GetMoveSpeedMultiplier(), 0.85f);

	ASC->RemoveActiveGameplayEffect(Handle);
	TestFalse(TEXT("Carrying tag is removed"), ASC->HasMatchingGameplayTag(SavaGameplayTags::State_CarryingFlag));
	TestEqual(TEXT("Move speed is restored"), Attributes->GetMoveSpeedMultiplier(), 1.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaRoundFrozenEffectTest, "Sava.CTF.RoundFrozenEffect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaRoundFrozenEffectTest::RunTest(const FString&)
{
	FSavaCTFTestWorld TestWorld;
	ASavaPlayerState* Player = TestWorld.SpawnPlayer(0);
	USavaAbilitySystemComponent* ASC = Player->GetSavaAbilitySystemComponent();
	const USavaAttributeSet* Attributes = Player->GetAttributeSet();

	//旗を運んでいても、カウントダウン中は速度 0(上書き)
	ApplyCarryingFlag(ASC, 0.85f);
	ASC->ApplyGameplayEffectToSelf(GetDefault<USavaGE_RoundFrozen>(), 1.0f, ASC->MakeEffectContext());
	TestTrue(TEXT("Frozen tag is granted"), ASC->HasMatchingGameplayTag(SavaGameplayTags::State_RoundFrozen));
	TestEqual(TEXT("Move speed is zero"), Attributes->GetMoveSpeedMultiplier(), 0.0f);

	ASC->RemoveActiveGameplayEffectBySourceEffect(USavaGE_RoundFrozen::StaticClass(), nullptr);
	TestFalse(TEXT("Frozen tag is removed"), ASC->HasMatchingGameplayTag(SavaGameplayTags::State_RoundFrozen));
	TestEqual(TEXT("Carrying slowdown remains"), Attributes->GetMoveSpeedMultiplier(), 0.85f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaSkillBlockedWhileCarryingTest, "Sava.CTF.SkillBlockedWhileCarrying",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaSkillBlockedWhileCarryingTest::RunTest(const FString&)
{
	FSavaCTFTestWorld TestWorld;
	ASavaPlayerState* Player = TestWorld.SpawnPlayer(0);
	USavaAbilitySystemComponent* ASC = Player->GetSavaAbilitySystemComponent();

	//シールドは Ability.Type.Skill を持つ
	const FGameplayAbilitySpecHandle SkillHandle = ASC->GiveAbility(FGameplayAbilitySpec(USavaShieldAbility::StaticClass()));
	const UGameplayAbility* Skill = GetDefault<USavaShieldAbility>();
	const FGameplayAbilityActorInfo* ActorInfo = ASC->AbilityActorInfo.Get();
	TestTrue(TEXT("Shield is a skill"), Skill->GetAssetTags().HasTag(SavaGameplayTags::Ability_Type_Skill));
	TestTrue(TEXT("Grapple is a skill"), GetDefault<USavaGrappleAbility>()->GetAssetTags().HasTag(SavaGameplayTags::Ability_Type_Skill));
	TestTrue(TEXT("Skill can be used normally"), Skill->CanActivateAbility(SkillHandle, ActorInfo));

	const FActiveGameplayEffectHandle CarryHandle = ApplyCarryingFlag(ASC, 0.85f);
	FGameplayTagContainer FailureTags;
	TestFalse(TEXT("Skill is blocked while carrying the flag"), Skill->CanActivateAbility(SkillHandle, ActorInfo, nullptr, nullptr, &FailureTags));
	TestTrue(TEXT("Failure reason is the carrying tag"), FailureTags.HasTag(SavaGameplayTags::State_CarryingFlag));

	ASC->RemoveActiveGameplayEffect(CarryHandle);
	TestTrue(TEXT("Skill can be used after dropping the flag"), Skill->CanActivateAbility(SkillHandle, ActorInfo));

	ASC->ApplyGameplayEffectToSelf(GetDefault<USavaGE_RoundFrozen>(), 1.0f, ASC->MakeEffectContext());
	TestFalse(TEXT("Skill is blocked during the countdown"), Skill->CanActivateAbility(SkillHandle, ActorInfo));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaGameStateRoundTest, "Sava.CTF.GameStateRound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaGameStateRoundTest::RunTest(const FString&)
{
	FSavaCTFTestWorld TestWorld;
	ASavaGameState* GameState = TestWorld.World->SpawnActor<ASavaGameState>();
	GameState->InitTeams(2);

	TestEqual(TEXT("Two teams"), GameState->GetNumTeams(), 2);
	TestTrue(TEXT("Teams can respawn at first"), GameState->CanTeamRespawn(0) && GameState->CanTeamRespawn(1));
	TestFalse(TEXT("Waiting is not in progress"), GameState->IsRoundInProgress());

	GameState->BeginNewRound();
	GameState->SetRoundPhase(ESavaRoundPhase::InRound, 180.0f);
	TestEqual(TEXT("Round number"), GameState->GetRoundNumber(), 1);
	TestTrue(TEXT("Round is in progress"), GameState->IsRoundInProgress());
	TestTrue(TEXT("Time remaining is set"), GameState->GetPhaseTimeRemaining() > 0.0f);

	GameState->SetTeamRespawnDisabled(1, true);
	TestFalse(TEXT("Captured team cannot respawn"), GameState->CanTeamRespawn(1));
	TestTrue(TEXT("Other team can respawn"), GameState->CanTeamRespawn(0));

	GameState->AddRoundWin(0);
	TestEqual(TEXT("Round win is counted"), GameState->GetTeamRoundWins(0), 1);
	TestEqual(TEXT("Last round winner"), GameState->GetLastRoundWinner(), static_cast<uint8>(0));

	//次のラウンドではリスポーン不可が解ける(取ったラウンド数は残る)
	GameState->BeginNewRound();
	TestTrue(TEXT("Respawn is restored next round"), GameState->CanTeamRespawn(1));
	TestEqual(TEXT("Round wins are kept"), GameState->GetTeamRoundWins(0), 1);
	TestEqual(TEXT("Round number advances"), GameState->GetRoundNumber(), 2);

	return true;
}

#endif

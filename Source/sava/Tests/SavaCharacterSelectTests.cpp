#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystem/SavaAbilitySystemComponent.h"
#include "AbilitySystem/SavaAbilitySystemLibrary.h"
#include "AbilitySystem/SavaAttributeSet.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Loadout/SavaCharacterTypes.h"
#include "Loadout/SavaLoadoutSettings.h"
#include "Player/SavaPlayerState.h"
#include "SavaCharacterMovementComponent.h"
#include "savaCharacter.h"

namespace
{
	//テスト用の一時的なワールド(スコープを抜けると破棄する)
	struct FCharacterSelectTestWorld
	{
		UWorld* World = nullptr;

		FCharacterSelectTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FCharacterSelectTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		ASavaPlayerState* SpawnPlayer(FName Character, uint8 TeamId) const
		{
			ASavaPlayerState* PlayerState = World->SpawnActor<ASavaPlayerState>();
			PlayerState->SetTeamId(TeamId);
			PlayerState->GetSavaAbilitySystemComponent()->InitAbilityActorInfo(PlayerState, PlayerState);
			FSavaLoadout Loadout;
			Loadout.Character = Character;
			PlayerState->SetLoadout(Loadout);
			return PlayerState;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaCharacterSanitizeTest, "Sava.Character.Sanitize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaCharacterSanitizeTest::RunTest(const FString&)
{
	const TArray<FName> Characters = USavaLoadoutSettings::GetSelectableCharacters();
	if (!TestTrue(TEXT("DT_Character has rows of FSavaCharacterRow"), Characters.Num() > 0))
	{
		return false;
	}

	FSavaLoadout Loadout;
	Loadout.Character = Characters.Last();
	TestEqual(TEXT("Selectable character is kept"), USavaLoadoutSettings::Sanitize(Loadout).Character, Characters.Last());

	Loadout.Character = TEXT("NoSuchCharacter");
	TestEqual(TEXT("Unknown character falls back to the first row"), USavaLoadoutSettings::Sanitize(Loadout).Character, Characters[0]);

	Loadout.Character = NAME_None;
	TestEqual(TEXT("Empty character falls back to the first row"), USavaLoadoutSettings::Sanitize(Loadout).Character, Characters[0]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaCharacterStatsTest, "Sava.Character.ApplyStats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaCharacterStatsTest::RunTest(const FString&)
{
	FCharacterSelectTestWorld TestWorld;
	AsavaCharacter* Character = TestWorld.World->SpawnActor<AsavaCharacter>(FVector(0.0f, 0.0f, 1000.0f), FRotator::ZeroRotator);
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const double FeetZ = Character->GetActorLocation().Z - Capsule->GetScaledCapsuleHalfHeight();

	FSavaCharacterRow Row;
	Row.CapsuleRadius = 40.0f;
	Row.CapsuleHalfHeight = 90.0f;
	Row.CrouchedHalfHeight = 50.0f;
	Row.EyeHeightBelowTop = 30.0f;
	Row.MoveSpeed = 700.0f;
	Character->ApplyCharacterStats(Row);

	TestEqual(TEXT("Capsule radius"), Capsule->GetUnscaledCapsuleRadius(), 40.0f);
	TestEqual(TEXT("Capsule half height"), Capsule->GetUnscaledCapsuleHalfHeight(), 90.0f);
	TestEqual(TEXT("Feet stay on the same height"), Character->GetActorLocation().Z - Capsule->GetScaledCapsuleHalfHeight(), FeetZ, 0.01);
	TestEqual(TEXT("Crouched half height"), Character->GetCharacterMovement()->GetCrouchedHalfHeight(), 50.0f);
	TestEqual(TEXT("Walk speed"), Character->GetCharacterMovement()->MaxWalkSpeed, 700.0f);
	TestEqual(TEXT("Eye is EyeHeightBelowTop under the capsule top"), Character->GetFirstPersonCameraComponent()->GetRelativeLocation().Z, 60.0);

	//未入力(0)のカプセルは体の既定値。しゃがみの高さの半分は半径より小さくならない
	FSavaCharacterRow EmptyCapsuleRow;
	EmptyCapsuleRow.CapsuleRadius = 0.0f;
	EmptyCapsuleRow.CapsuleHalfHeight = 0.0f;
	EmptyCapsuleRow.CrouchedHalfHeight = 20.0f;
	Character->ApplyCharacterStats(EmptyCapsuleRow);

	const UCapsuleComponent* DefaultCapsule = GetDefault<AsavaCharacter>()->GetCapsuleComponent();
	TestEqual(TEXT("Zero radius uses the default"), Capsule->GetUnscaledCapsuleRadius(), DefaultCapsule->GetUnscaledCapsuleRadius());
	TestEqual(TEXT("Zero half height uses the default"), Capsule->GetUnscaledCapsuleHalfHeight(), DefaultCapsule->GetUnscaledCapsuleHalfHeight());
	TestEqual(TEXT("Crouched half height is at least the radius"), Character->GetCharacterMovement()->GetCrouchedHalfHeight(), DefaultCapsule->GetUnscaledCapsuleRadius());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaCharacterHealthAndDamageTest, "Sava.Character.HealthAndDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaCharacterHealthAndDamageTest::RunTest(const FString&)
{
	const TArray<FName> Characters = USavaLoadoutSettings::GetSelectableCharacters();
	if (!TestTrue(TEXT("DT_Character has rows of FSavaCharacterRow"), Characters.Num() > 0))
	{
		return false;
	}
	const FName CharacterId = Characters.Last();
	const FSavaCharacterRow* Row = USavaLoadoutSettings::FindCharacterRow(CharacterId);

	FCharacterSelectTestWorld TestWorld;
	ASavaPlayerState* Attacker = TestWorld.SpawnPlayer(CharacterId, 0);
	ASavaPlayerState* Victim = TestWorld.SpawnPlayer(CharacterId, 1);

	//体に入れると、キャラクターが決まり、最大 HP が行の値になって満タンになる
	AsavaCharacter* VictimBody = TestWorld.World->SpawnActor<AsavaCharacter>(FVector(0.0f, 0.0f, 1000.0f), FRotator::ZeroRotator);
	Victim->ApplyLoadoutTo(VictimBody);
	TestEqual(TEXT("Body knows the character"), VictimBody->GetCharacterId(), CharacterId);
	TestEqual(TEXT("Max health from the row"), Victim->GetAttributeSet()->GetMaxHealth(), Row->MaxHP);
	TestEqual(TEXT("Health is full"), Victim->GetAttributeSet()->GetHealth(), Row->MaxHP);

	//与えるダメージは攻撃側の倍率が掛かる
	TestEqual(TEXT("Damage multiplier from the row"), USavaAbilitySystemLibrary::GetOutgoingDamageMultiplier(Attacker), Row->DamageMultiplier);
	TestEqual(TEXT("Non-player deals normal damage"), USavaAbilitySystemLibrary::GetOutgoingDamageMultiplier(nullptr), 1.0f);
	USavaAbilitySystemLibrary::ApplyDamage(Attacker, Victim, 20.0f, Attacker);
	TestEqual(TEXT("Damage is multiplied"), Victim->GetAttributeSet()->GetHealth(), Row->MaxHP - 20.0f * Row->DamageMultiplier, 0.01f);
	return true;
}

#endif

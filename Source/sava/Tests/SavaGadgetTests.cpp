#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystem/SavaAbilitySystemComponent.h"
#include "AbilitySystem/SavaAttributeSet.h"
#include "AbilitySystem/SavaGameplayEffects.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Gadget/FragGrenade.h"
#include "Gadget/GadgetPlaceAbility.h"
#include "Player/SavaPlayerState.h"
#include "SavaGameplayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaFragGrenadeFalloffTest, "Sava.Gadget.FragGrenadeFalloff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaFragGrenadeFalloffTest::RunTest(const FString&)
{
	//最大 100、半径 400、100cm までは減らない、端で 20%
	auto Damage = [](float Distance)
	{
		return AFragGrenade::CalculateFalloffDamage(Distance, 100.0f, 400.0f, 100.0f, 0.2f);
	};

	TestEqual(TEXT("At the center"), Damage(0.0f), 100.0f);
	TestEqual(TEXT("Inside full damage radius"), Damage(100.0f), 100.0f);
	TestEqual(TEXT("Halfway through falloff"), Damage(250.0f), 60.0f);
	TestEqual(TEXT("At the edge"), Damage(400.0f), 20.0f);
	TestEqual(TEXT("Outside the radius"), Damage(401.0f), 0.0f);

	//全範囲で減る(FullDamageRadius = 0)
	TestEqual(TEXT("No full damage radius, halfway"),
		AFragGrenade::CalculateFalloffDamage(200.0f, 100.0f, 400.0f, 0.0f, 0.0f), 50.0f);

	//FullDamageRadius が半径以上なら範囲内はすべて最大
	TestEqual(TEXT("Full damage radius larger than radius"),
		AFragGrenade::CalculateFalloffDamage(400.0f, 100.0f, 400.0f, 500.0f, 0.2f), 100.0f);

	TestEqual(TEXT("Zero radius"),
		AFragGrenade::CalculateFalloffDamage(0.0f, 100.0f, 0.0f, 0.0f, 0.2f), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaGadgetPlaceLocationTest, "Sava.Gadget.PlaceLocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaGadgetPlaceLocationTest::RunTest(const FString&)
{
	//視点 (0,0,100) から前(+X)へ、リーチ 600、壁から 30 離す
	const FVector View(0.0f, 0.0f, 100.0f);
	auto Place = [&View](float HitDistance)
	{
		return UGadgetPlaceAbility::CalculatePlaceLocation(View, FVector::ForwardVector, 600.0f, HitDistance, 30.0f);
	};

	TestEqual(TEXT("No wall: placed at reach (in the air)"), Place(-1.0f), FVector(600.0f, 0.0f, 100.0f));
	TestEqual(TEXT("Wall: placed in front of it"), Place(200.0f), FVector(170.0f, 0.0f, 100.0f));
	TestEqual(TEXT("Wall right in front: never behind the view"), Place(10.0f), View);

	//向きの長さが 1 でなくても距離は変わらない
	TestEqual(TEXT("Direction is normalized"),
		UGadgetPlaceAbility::CalculatePlaceLocation(View, FVector(2.0f, 0.0f, 0.0f), 600.0f, -1.0f, 30.0f),
		FVector(600.0f, 0.0f, 100.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaSlowedEffectTest, "Sava.Gadget.SlowedEffect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaSlowedEffectTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());

	ASavaPlayerState* Player = World->SpawnActor<ASavaPlayerState>();
	USavaAbilitySystemComponent* ASC = Player->GetSavaAbilitySystemComponent();
	ASC->InitAbilityActorInfo(Player, Player);
	const USavaAttributeSet* Attributes = Player->GetAttributeSet();

	//ASlowField::ApplySlow と同じ組み立て方
	auto ApplySpeedEffect = [ASC](TSubclassOf<UGameplayEffect> EffectClass, float Multiplier)
	{
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.0f, ASC->MakeEffectContext());
		Spec.Data->SetSetByCallerMagnitude(SavaGameplayTags::SetByCaller_MoveSpeedMultiplier, Multiplier);
		return ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	};

	const FActiveGameplayEffectHandle Handle = ApplySpeedEffect(USavaGE_Slowed::StaticClass(), 0.5f);
	TestTrue(TEXT("Slowed effect is active"), Handle.IsValid());
	TestTrue(TEXT("Slowed tag is granted"), ASC->HasMatchingGameplayTag(SavaGameplayTags::State_Slowed));
	TestEqual(TEXT("Move speed is halved"), Attributes->GetMoveSpeedMultiplier(), 0.5f);

	//旗を運んでいる減速とは掛け算で重なる
	const FActiveGameplayEffectHandle FlagHandle = ApplySpeedEffect(USavaGE_CarryingFlag::StaticClass(), 0.8f);
	TestEqual(TEXT("Stacks with carrying flag"), Attributes->GetMoveSpeedMultiplier(), 0.4f);
	ASC->RemoveActiveGameplayEffect(FlagHandle);

	ASC->RemoveActiveGameplayEffect(Handle);
	TestFalse(TEXT("Slowed tag is removed"), ASC->HasMatchingGameplayTag(SavaGameplayTags::State_Slowed));
	TestEqual(TEXT("Move speed is restored"), Attributes->GetMoveSpeedMultiplier(), 1.0f);

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif

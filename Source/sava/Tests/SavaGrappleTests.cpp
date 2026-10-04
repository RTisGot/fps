#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystem/SavaAbilitySet.h"
#include "AbilitySystem/SavaAbilitySystemComponent.h"
#include "AbilitySystem/SavaInputConfig.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "InputMappingContext.h"
#include "Player/SavaPlayerState.h"
#include "SavaCharacterMovementComponent.h"
#include "SavaGameplayTags.h"
#include "SavaGrappleAbility.h"
#include "SavaGrappleRootMotionSource.h"
#include "SavaGrappleRope.h"
#include "savaCharacter.h"

namespace
{
	struct FGrappleTestWorld
	{
		UWorld* World;
		AsavaCharacter* Character;
		USavaAbilitySystemComponent* ASC;
		FGrappleTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Character = World->SpawnActor<AsavaCharacter>(FVector(0, 0, 300), FRotator::ZeroRotator);
			ASavaPlayerState* State = World->SpawnActor<ASavaPlayerState>();
			APlayerController* Controller = World->SpawnActor<APlayerController>();
			Controller->SetPlayerState(State);
			Controller->Possess(Character);
			Controller->SetControlRotation(FRotator::ZeroRotator);
			ASC = State->GetSavaAbilitySystemComponent();
			ASC->InitAbilityActorInfo(State, Character);
		}
		~FGrappleTestWorld()
		{
			ASC->CancelAllAbilities();
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		void Wall(float X, EComponentMobility::Type Mobility = EComponentMobility::Static)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			auto* Box = NewObject<UBoxComponent>(Actor);
			Actor->SetRootComponent(Box);
			Box->SetBoxExtent(FVector(20, 1000, 1000));
			Box->SetCollisionProfileName(TEXT("BlockAll"));
			Box->SetWorldLocation(FVector(X, 0, 300));
			Box->SetMobility(Mobility);
			Box->RegisterComponent();
		}
		FGameplayAbilitySpecHandle Grant(TSubclassOf<UGameplayAbility> Class = USavaGrappleAbility::StaticClass())
		{
			FGameplayAbilitySpec Spec(Class, 1);
			Spec.InputPressed = true;
			return ASC->GiveAbility(Spec);
		}
		bool Active(FGameplayAbilitySpecHandle Handle) const
		{
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
			return Spec && Spec->IsActive();
		}
	};

	// Tick on separate automation frames: CharacterMovement/root-motion gate work by GFrameCounter.
	class FGrappleMovementCommand : public IAutomationLatentCommand
	{
	public:
		explicit FGrappleMovementCommand(FAutomationTestBase* InTest, float InSteering = 0.0f, float InYaw = 0.0f, bool bInLift = false)
			: Test(InTest), Steering(InSteering), Yaw(InYaw), bLift(bInLift) {}
		virtual bool Update() override
		{
			if (!Fixture)
			{
				Fixture = MakeUnique<FGrappleTestWorld>();
				Fixture->Wall(1000);
				Fixture->World->BeginPlay();
				Fixture->World->GetWorldSettings()->NotifyBeginPlay();
				Test->TestTrue(TEXT("Movement test world has begun play"), Fixture->World->HasBegunPlay());
				Handle = Fixture->Grant();
				Fixture->ASC->TryActivateAbility(Handle);
				Fixture->Character->GetController()->SetControlRotation(FRotator(0, Yaw, 0));
				return false;
			}
			const FVector Right = FRotationMatrix(FRotator(0, Yaw, 0)).GetUnitAxis(EAxis::Y);
			Fixture->Character->AddMovementInput(Right, Steering);
			Fixture->Character->GetSavaCharacterMovementComponent()->SetJumpHeld(bLift);
			Fixture->World->Tick(LEVELTICK_All, 1.0f / 60.0f);
			const float Side = Fixture->Character->GetActorLocation().Y;
			MaxSide = FMath::Max(MaxSide, FMath::Abs(Side));
			PeakHeight = FMath::Max(PeakHeight, Fixture->Character->GetActorLocation().Z);
			if (Steering != 0.0f && FMath::Abs(Side) > 5.0f)
				Test->TestTrue(TEXT("Steering follows camera-relative input"), Side * Right.Y * Steering > 0.0f);
			if (++Frames < 90 && Fixture->Active(Handle)) return false;
			Test->AddInfo(FString::Printf(TEXT("Pull finished at %s; active=%d; frames=%d"),
				*Fixture->Character->GetActorLocation().ToString(), Fixture->Active(Handle), Frames));
			Test->TestTrue(TEXT("Root motion actually pulls the character toward the wall"), Fixture->Character->GetActorLocation().X > 800.0f);
			Test->TestTrue(TEXT("Capsule does not pass through the wall"), Fixture->Character->GetActorLocation().X < 980.0f);
			Test->TestFalse(TEXT("Arrival automatically ends ability"), Fixture->Active(Handle));
			Test->TestTrue(TEXT("Arrival restores normal falling physics"), Fixture->Character->GetCharacterMovement()->IsFalling());
			if (Steering != 0.0f) Test->TestTrue(TEXT("Input changes the actual flight path"), MaxSide > 25.0f);
			Test->TestTrue(TEXT("Sideways deviation stays bounded"), MaxSide <= 701.0f);
			if (bLift) Test->TestTrue(TEXT("Jump raises the flight path above the target height"), PeakHeight > 390.0f);
			Fixture.Reset();
			return true;
		}
	private:
		FAutomationTestBase* Test;
		TUniquePtr<FGrappleTestWorld> Fixture;
		FGameplayAbilitySpecHandle Handle;
		int32 Frames = 0;
		float Steering = 0.0f;
		float Yaw = 0.0f;
		float MaxSide = 0.0f;
		float PeakHeight = 0.0f;
		bool bLift = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaGrappleMovementTest, "Sava.Grapple.Movement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSavaGrappleMovementTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FGrappleMovementCommand(this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaGrappleSteeringTest, "Sava.Grapple.SteeringMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSavaGrappleSteeringTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FGrappleMovementCommand(this, 1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FGrappleMovementCommand(this, -1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FGrappleMovementCommand(this, 1.0f, 180.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FGrappleMovementCommand(this, 0.0f, 0.0f, true));
	ADD_LATENT_AUTOMATION_COMMAND(FGrappleMovementCommand(this, 1.0f, 0.0f, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaGrappleSteeringMathTest, "Sava.Grapple.SteeringLimits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSavaGrappleSteeringMathTest::RunTest(const FString&)
{
	FSavaGrappleRootMotionSource Source;
	Source.StartLocation = FVector::ZeroVector;
	Source.TargetLocation = FVector(3000, 0, 0);
	Source.MaxLateralOffset = 35;
	Source.Duration = 3;
	FVector Position = Source.StartLocation;
	FVector Velocity = FVector::ZeroVector;
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		Velocity = Source.CalculateVelocity(Position, Velocity, FVector(0, 10, 0), 1.0f / 60.0f);
		Position += Velocity / 60.0f;
		TestTrue(TEXT("Even oversized input respects the offset cap"), FMath::Abs(Position.Y) <= 35.01f);
		TestTrue(TEXT("Speed is bounded"), Velocity.Size() <= FMath::Sqrt(FMath::Square(Source.PullSpeed) + FMath::Square(Source.SteeringSpeed) + FMath::Square(Source.JumpLiftSpeed)) + 0.1f);
	}
	TestTrue(TEXT("Holding steer still reaches fixed destination"), FVector::Dist(Position, Source.TargetLocation) < 35.0f);
	TestTrue(TEXT("Zero delta time is safe"), Source.CalculateVelocity(Position, Velocity, FVector::RightVector, 0).IsZero());
	Source.SteeringSpeed = 0;
	Source.JumpLiftSpeed = 0;
	TestTrue(TEXT("Disabling steering preserves straight pull"), FMath::IsNearlyZero(Source.CalculateVelocity(FVector::ZeroVector,
		FVector(0, 700, 0), FVector::RightVector, 0.016f).Y));
	TUniquePtr<FRootMotionSource> Copy(Source.Clone());
	TestTrue(TEXT("Saved-move clone retains custom source type"), Copy->GetScriptStruct() == FSavaGrappleRootMotionSource::StaticStruct());
	TestTrue(TEXT("Saved-move clone matches settings"), Source.Matches(Copy.Get()));
	static_cast<FSavaGrappleRootMotionSource*>(Copy.Get())->SteeringSpeed = 900;
	TestFalse(TEXT("Different steering settings cannot combine"), Source.Matches(Copy.Get()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaGrappleLiftTest, "Sava.Grapple.JumpLift",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSavaGrappleLiftTest::RunTest(const FString&)
{
	FSavaGrappleRootMotionSource Source;
	Source.StartLocation = FVector::ZeroVector;
	Source.TargetLocation = FVector(3000, 0, 0);
	Source.Duration = 3.0f;
	const FVector Lift = Source.CalculateVelocity(FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, 0.1f, true);
	TestTrue(TEXT("Jump produces upward velocity while still pulling"), Lift.Z > 100.0f && Lift.X > 1000.0f);
	const FVector Released = Source.CalculateVelocity(FVector::ZeroVector, Lift, FVector::ZeroVector, 0.1f, false);
	TestTrue(TEXT("Releasing jump smoothly reduces upward velocity"), Released.Z >= 0.0f && Released.Z < Lift.Z);
	FSavaGrappleRootMotionSource OldTuning = Source;
	OldTuning.SteeringSpeed = 700.0f;
	OldTuning.SteeringResponse = 8.0f;
	const FVector Input = FVector::RightVector;
	TestTrue(TEXT("New movement tuning is more responsive"),
		Source.CalculateVelocity(FVector::ZeroVector, FVector::ZeroVector, Input, 0.1f).Y >
		OldTuning.CalculateVelocity(FVector::ZeroVector, FVector::ZeroVector, Input, 0.1f).Y);
	FGrappleTestWorld Fixture;
	Fixture.Character->GetSavaCharacterMovementComponent()->SetJumpHeld(true);
	auto* Prediction = Fixture.Character->GetSavaCharacterMovementComponent()->GetPredictionData_Client_Character();
	FSavedMovePtr Saved = Prediction->AllocateNewMove();
	Saved->Clear();
	Saved->SetMoveFor(Fixture.Character, 0.016f, FVector::ZeroVector, *Prediction);
	TestTrue(TEXT("Jump lift uses the existing network flag"), (Saved->GetCompressedFlags() & FSavedMove_Character::FLAG_Custom_1) != 0);
	Fixture.Character->GetSavaCharacterMovementComponent()->SetJumpHeld(false);
	Saved->PrepMoveFor(Fixture.Character);
	TestTrue(TEXT("Movement replay restores held jump"), Fixture.Character->GetSavaCharacterMovementComponent()->IsJumpHeld());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaGrappleAssetsTest, "Sava.Grapple.ExistingAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSavaGrappleAssetsTest::RunTest(const FString&)
{
	UClass* GrappleClass = LoadClass<USavaGrappleAbility>(nullptr, TEXT("/Game/Skill/GA_Grapple.GA_Grapple_C"));
	if (!TestNotNull(TEXT("Existing GA_Grapple inherits native implementation"), GrappleClass)) return false;
	const auto* Config = LoadObject<USavaInputConfig>(nullptr, TEXT("/Game/FirstPerson/Input/DA_InputConfig.DA_InputConfig"));
	const auto* Mapping = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/FirstPerson/Input/IMC_Default.IMC_Default"));
	const auto* Set = LoadObject<USavaAbilitySet>(nullptr, TEXT("/Game/FirstPerson/Input/DA_AbilitySet.DA_AbilitySet"));
	if (!TestNotNull(TEXT("Input config"), Config) || !TestNotNull(TEXT("Mapping"), Mapping)
		|| !TestNotNull(TEXT("Ability set"), Set)) return false;
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ablity.Grapple"));
	const FSavaInputAction* Input = Config->AbilityInputActions.FindByPredicate([&](const FSavaInputAction& Entry) { return Entry.InputTag == Tag; });
	if (!TestNotNull(TEXT("Existing grapple tag maps to an input action"), Input)) return false;
	bool bMapped = false;
	for (const FEnhancedActionKeyMapping& Entry : Mapping->GetMappings())
	{
		if (Entry.Action == Input->InputAction)
		{
			bMapped = true;
			AddInfo(FString::Printf(TEXT("Existing grapple key: %s"), *Entry.Key.ToString()));
		}
	}
	TestTrue(TEXT("Grapple action has a key in the existing mapping"), bMapped);
	FGrappleTestWorld Test;
	FSavaAbilitySet_GrantedHandles Handles;
	Set->GiveToAbilitySystem(Test.ASC, &Handles);
	bool bGranted = false;
	for (const FGameplayAbilitySpec& Spec : Test.ASC->GetActivatableAbilities())
		bGranted |= Spec.Ability && Spec.Ability->GetClass() == GrappleClass && Spec.GetDynamicSpecSourceTags().HasTagExact(Tag);
	TestTrue(TEXT("Existing ability set grants GA_Grapple with its existing tag"), bGranted);
	// Test the Blueprint subclass too: stored defaults must not break the native lifecycle.
	Test.Wall(1000);
	Test.ASC->AbilityInputTagPressed(Tag);
	TestTrue(TEXT("Existing tagged input actually starts the pull"), Test.Character->GetCharacterMovement()->MovementMode == MOVE_Flying);
	Test.ASC->AbilityInputTagReleased(Tag);
	TestTrue(TEXT("Existing tagged input release returns to falling"), Test.Character->GetCharacterMovement()->IsFalling());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaGrappleLifecycleTest, "Sava.Grapple.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSavaGrappleLifecycleTest::RunTest(const FString&)
{
	const FGameplayTag Cooldown = FGameplayTag::RequestGameplayTag(TEXT("Cooldown.Skill.Grapple"));
	{
		FGrappleTestWorld Test;
		const auto Handle = Test.Grant();
		Test.ASC->TryActivateAbility(Handle);
		TestFalse(TEXT("A miss ends immediately"), Test.Active(Handle));
		TestFalse(TEXT("A miss does not consume cooldown"), Test.ASC->HasMatchingGameplayTag(Cooldown));
	}
	{
		FGrappleTestWorld Test;
		Test.Wall(5000);
		const auto Handle = Test.Grant();
		Test.ASC->TryActivateAbility(Handle);
		TestFalse(TEXT("Out-of-range wall is rejected"), Test.Active(Handle));
	}
	{
		FGrappleTestWorld Test;
		Test.Wall(1000, EComponentMobility::Movable);
		const auto Handle = Test.Grant();
		Test.ASC->TryActivateAbility(Handle);
		TestFalse(TEXT("Moving objects cannot become static anchors"), Test.Active(Handle));
	}
	{
		FGrappleTestWorld Test;
		Test.Wall(1000);
		const auto Handle = Test.Grant();
		Test.ASC->TryActivateAbility(Handle);
		TestTrue(TEXT("Wall hit keeps the ability active"), Test.Active(Handle));
		TestTrue(TEXT("Pull uses flying physics"), Test.Character->GetCharacterMovement()->MovementMode == MOVE_Flying);
		TestTrue(TEXT("Successful grapple consumes cooldown"), Test.ASC->HasMatchingGameplayTag(Cooldown));
		bool bHasRope = false;
		for (TActorIterator<ASavaGrappleRope> It(Test.World); It; ++It) bHasRope |= IsValid(*It);
		TestTrue(TEXT("Pull creates a real rope actor"), bHasRope);
		Test.ASC->CancelAbilityHandle(Handle);
		TestFalse(TEXT("Cancel ends ability"), Test.Active(Handle));
		TestTrue(TEXT("Cancel restores falling, not stale slide/wallrun mode"), Test.Character->GetCharacterMovement()->IsFalling());
		bHasRope = false;
		for (TActorIterator<ASavaGrappleRope> It(Test.World); It; ++It) bHasRope |= IsValid(*It);
		TestFalse(TEXT("Cancel removes rope"), bHasRope);
	}
	{
		FGrappleTestWorld Test;
		Test.Wall(1000);
		const auto Handle = Test.Grant();
		Test.ASC->TryActivateAbility(Handle);
		Test.ASC->AddLooseGameplayTag(SavaGameplayTags::State_Dead);
		TestFalse(TEXT("Death ends grapple"), Test.Active(Handle));
		TestTrue(TEXT("Death does not leave flying enabled"), Test.Character->GetCharacterMovement()->IsFalling());
	}
	return true;
}
#endif

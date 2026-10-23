#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Map/SavaLadder.h"
#include "SavaCharacterMovementComponent.h"
#include "savaCharacter.h"

namespace
{
	constexpr float LadderTestDeltaTime = 1.0f / 60.0f;

	//梯子は原点に、正面を -X 向きに置く(高さ 400)。奥(+X)に上面が梯子の上端と同じ高さの足場、下に床
	struct FLadderTestWorld
	{
		UWorld* World;
		AsavaCharacter* Character;
		USavaCharacterMovementComponent* Movement;
		APlayerController* Controller;
		ASavaLadder* Ladder;
		FLadderTestWorld(const FVector& SpawnLocation, const FRotator& SpawnRotation)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Box(FVector(0, 0, -20), FVector(5000, 5000, 20));
			Box(FVector(200, 0, 200), FVector(200, 200, 200));
			Ladder = World->SpawnActor<ASavaLadder>(FVector::ZeroVector, FRotator(0, 180, 0));
			Character = World->SpawnActor<AsavaCharacter>(SpawnLocation, SpawnRotation);
			Controller = World->SpawnActor<APlayerController>();
			Controller->Possess(Character);
			Controller->SetControlRotation(SpawnRotation);
			Movement = Character->GetSavaCharacterMovementComponent();
		}
		~FLadderTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		void Box(const FVector& Center, const FVector& Extent)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			auto* Box = NewObject<UBoxComponent>(Actor);
			Actor->SetRootComponent(Box);
			Box->SetBoxExtent(Extent);
			Box->SetCollisionProfileName(TEXT("BlockAll"));
			Box->SetWorldLocation(Center);
			Box->SetMobility(EComponentMobility::Static);
			Box->RegisterComponent();
		}
		void Look(float Pitch)
		{
			FRotator Rotation = Controller->GetControlRotation();
			Rotation.Pitch = Pitch;
			Controller->SetControlRotation(Rotation);
		}
		void Forward()
		{
			Character->AddMovementInput(Character->GetActorForwardVector(), 1.0f);
		}
		float FeetZ() const
		{
			return Character->GetActorLocation().Z - Character->GetSimpleCollisionHalfHeight();
		}
	};

	//Step は毎フレームの Tick の前に呼ばれ、true を返すと終わる
	class FLadderCommand : public IAutomationLatentCommand
	{
	public:
		using FSetup = TFunction<void(FLadderTestWorld&)>;
		using FStep = TFunction<bool(FLadderTestWorld&, int32 Frame)>;

		FLadderCommand(const FVector& InSpawn, const FRotator& InRotation, FSetup InSetup, FStep InStep, FSimpleDelegate InOnTimeout)
			: Spawn(InSpawn), Rotation(InRotation), Setup(MoveTemp(InSetup)), Step(MoveTemp(InStep)), OnTimeout(MoveTemp(InOnTimeout)) {}

		virtual bool Update() override
		{
			if (!Fixture)
			{
				Fixture = MakeUnique<FLadderTestWorld>(Spawn, Rotation);
				if (Setup)
				{
					Setup(*Fixture);
				}
				Fixture->World->BeginPlay();
				Fixture->World->GetWorldSettings()->NotifyBeginPlay();
				return false;
			}
			if (Step(*Fixture, Frame++))
			{
				Fixture.Reset();
				return true;
			}
			if (Frame > 600)
			{
				OnTimeout.ExecuteIfBound();
				Fixture.Reset();
				return true;
			}
			Fixture->World->Tick(LEVELTICK_All, LadderTestDeltaTime);
			return false;
		}

	private:
		FVector Spawn;
		FRotator Rotation;
		FSetup Setup;
		FStep Step;
		FSimpleDelegate OnTimeout;
		TUniquePtr<FLadderTestWorld> Fixture;
		int32 Frame = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaLadderClimbUpTest, "Sava.Ladder.ClimbUpAndOver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSavaLadderClimbUpTest::RunTest(const FString&)
{
	//梯子へ歩いて掴まり、前を見たまま前入力で登り、頂上でよじ登って奥の足場に乗る
	TSharedRef<bool> bSawLadder = MakeShared<bool>(false);
	TSharedRef<bool> bSawMantle = MakeShared<bool>(false);
	ADD_LATENT_AUTOMATION_COMMAND(FLadderCommand(FVector(-300, 0, 100), FRotator::ZeroRotator, nullptr,
		[this, bSawLadder, bSawMantle](FLadderTestWorld& W, int32 Frame)
		{
			*bSawLadder |= W.Movement->IsOnLadder();
			*bSawMantle |= *bSawLadder && W.Movement->IsMantling();
			if (W.Movement->IsOnLadder())
			{
				TestTrue(TEXT("Stays in front of the ladder while climbing"), W.Character->GetActorLocation().X < 0.0f);
			}
			if (*bSawMantle && W.Movement->MovementMode == MOVE_Walking)
			{
				TestTrue(TEXT("Ends on top of the platform"), W.FeetZ() > 390.0f && W.Character->GetActorLocation().X > 0.0f);
				return true;
			}
			W.Forward();
			return false;
		},
		FSimpleDelegate::CreateLambda([this, bSawLadder, bSawMantle]()
		{
			AddError(FString::Printf(TEXT("Did not climb over the ladder (ladder %d, mantle %d)"), *bSawLadder, *bSawMantle));
		})));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaLadderClimbDownTest, "Sava.Ladder.LookDownClimbsDown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSavaLadderClimbDownTest::RunTest(const FString&)
{
	//空中で梯子に掴まり、下を見て前入力で降りる → 床で歩きに戻る
	TSharedRef<float> LadderZ = MakeShared<float>(0.0f);
	ADD_LATENT_AUTOMATION_COMMAND(FLadderCommand(FVector(-80, 0, 300), FRotator::ZeroRotator,
		[](FLadderTestWorld& W) { W.Movement->SetMovementMode(MOVE_Falling); },
		[this, LadderZ](FLadderTestWorld& W, int32 Frame)
		{
			W.Forward();
			if (*LadderZ == 0.0f)
			{
				if (W.Movement->IsOnLadder())
				{
					*LadderZ = W.Character->GetActorLocation().Z;
					W.Look(-60.0f);
				}
				return false;
			}
			if (W.Movement->IsOnLadder())
			{
				TestTrue(TEXT("Does not climb up while looking down"), W.Character->GetActorLocation().Z <= *LadderZ + 1.0f);
				return false;
			}
			TestEqual(TEXT("Reaches the ground and walks"), static_cast<int32>(W.Movement->MovementMode.GetValue()), static_cast<int32>(MOVE_Walking));
			TestTrue(TEXT("Stands on the floor"), W.FeetZ() < 10.0f);
			return true;
		},
		FSimpleDelegate::CreateLambda([this]() { AddError(TEXT("Did not climb down to the ground")); })));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaLadderJumpOffTest, "Sava.Ladder.JumpOff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSavaLadderJumpOffTest::RunTest(const FString&)
{
	//梯子でジャンプ → 梯子から離れる方向(-X)と上へ飛び、すぐには掴み直さない
	TSharedRef<int32> JumpFrame = MakeShared<int32>(-1);
	ADD_LATENT_AUTOMATION_COMMAND(FLadderCommand(FVector(-80, 0, 300), FRotator::ZeroRotator,
		[](FLadderTestWorld& W) { W.Movement->SetMovementMode(MOVE_Falling); },
		[this, JumpFrame](FLadderTestWorld& W, int32 Frame)
		{
			W.Forward();
			if (*JumpFrame < 0)
			{
				if (W.Movement->IsOnLadder())
				{
					W.Character->Jump();
					*JumpFrame = Frame;
				}
				return false;
			}
			if (Frame == *JumpFrame + 1)
			{
				W.Character->StopJumping();
				TestTrue(TEXT("Jumping leaves the ladder"), W.Movement->IsFalling());
				TestTrue(TEXT("Jumps away from the ladder"), W.Movement->Velocity.X < -W.Movement->LadderJumpOffSpeed * 0.7f);
				TestTrue(TEXT("Jumps upward"), W.Movement->Velocity.Z > 0.0f);
				return false;
			}
			TestFalse(TEXT("Does not grab the ladder again during the cooldown"), W.Movement->IsOnLadder());
			return Frame >= *JumpFrame + 10;
		},
		FSimpleDelegate::CreateLambda([this]() { AddError(TEXT("Did not grab the ladder")); })));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaLadderNoGrabTest, "Sava.Ladder.NoGrab",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSavaLadderNoGrabTest::RunTest(const FString&)
{
	//地上で下を見て梯子へ歩いても掴まらない(降りる向き)
	ADD_LATENT_AUTOMATION_COMMAND(FLadderCommand(FVector(-200, 0, 100), FRotator::ZeroRotator,
		[](FLadderTestWorld& W) { W.Look(-60.0f); },
		[this](FLadderTestWorld& W, int32 Frame)
		{
			TestFalse(TEXT("Looking down on the ground does not grab"), W.Movement->IsOnLadder());
			W.Forward();
			return Frame >= 90;
		}, FSimpleDelegate()));
	//梯子の範囲の外(横)を通っても掴まらない
	ADD_LATENT_AUTOMATION_COMMAND(FLadderCommand(FVector(-300, 400, 100), FRotator::ZeroRotator, nullptr,
		[this](FLadderTestWorld& W, int32 Frame)
		{
			TestFalse(TEXT("Outside the climb volume does not grab"), W.Movement->IsOnLadder());
			W.Forward();
			return Frame >= 90;
		}, FSimpleDelegate()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaLadderEnterFromTopTest, "Sava.Ladder.EnterFromTop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSavaLadderEnterFromTopTest::RunTest(const FString&)
{
	//上の足場から梯子の方(-X)へ、下を見て歩く → 上から掴まり、そのまま床まで降りる
	TSharedRef<bool> bSawLadder = MakeShared<bool>(false);
	ADD_LATENT_AUTOMATION_COMMAND(FLadderCommand(FVector(200, 0, 500), FRotator(0, 180, 0),
		[](FLadderTestWorld& W) { W.Look(-60.0f); },
		[this, bSawLadder](FLadderTestWorld& W, int32 Frame)
		{
			*bSawLadder |= W.Movement->IsOnLadder();
			if (*bSawLadder && !W.Movement->IsOnLadder())
			{
				TestEqual(TEXT("Climbs down to the ground and walks"), static_cast<int32>(W.Movement->MovementMode.GetValue()), static_cast<int32>(MOVE_Walking));
				TestTrue(TEXT("Stands on the floor"), W.FeetZ() < 10.0f);
				return true;
			}
			TestFalse(TEXT("Does not fall off the platform edge"), !*bSawLadder && W.Movement->IsFalling() && W.Character->GetActorLocation().X < 0.0f);
			W.Forward();
			return false;
		},
		FSimpleDelegate::CreateLambda([this, bSawLadder]()
		{
			AddError(FString::Printf(TEXT("Did not climb down from the top (ladder %d)"), *bSawLadder));
		})));
	return true;
}

#endif

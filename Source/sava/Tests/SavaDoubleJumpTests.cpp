#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "SavaCharacterMovementComponent.h"
#include "savaCharacter.h"

namespace
{
	constexpr float DoubleJumpTestDeltaTime = 1.0f / 60.0f;

	struct FDoubleJumpTestWorld
	{
		UWorld* World;
		AsavaCharacter* Character;
		USavaCharacterMovementComponent* Movement;
		explicit FDoubleJumpTestWorld(const FVector& SpawnLocation)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Character = World->SpawnActor<AsavaCharacter>(SpawnLocation, FRotator::ZeroRotator);
			APlayerController* Controller = World->SpawnActor<APlayerController>();
			Controller->Possess(Character);
			Controller->SetControlRotation(FRotator::ZeroRotator);
			Movement = Character->GetSavaCharacterMovementComponent();
		}
		~FDoubleJumpTestWorld()
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
		void SetFalling(const FVector& Velocity)
		{
			Movement->SetMovementMode(MOVE_Falling);
			Movement->Velocity = Velocity;
		}
		void Press()
		{
			Character->Jump();
		}
		void Release()
		{
			Character->StopJumping();
		}
	};

	//Step は毎フレームの Tick の前に呼ばれ、true を返すと終わる。Tick はフレームごとに別の Update で行う(移動の処理がフレーム番号で判定するため)
	class FDoubleJumpCommand : public IAutomationLatentCommand
	{
	public:
		using FSetup = TFunction<void(FDoubleJumpTestWorld&)>;
		using FStep = TFunction<bool(FDoubleJumpTestWorld&, int32 Frame)>;

		FDoubleJumpCommand(const FVector& InSpawn, FSetup InSetup, FStep InStep)
			: Spawn(InSpawn), Setup(MoveTemp(InSetup)), Step(MoveTemp(InStep)) {}

		virtual bool Update() override
		{
			if (!Fixture)
			{
				Fixture = MakeUnique<FDoubleJumpTestWorld>(Spawn);
				if (Setup)
				{
					Setup(*Fixture);
				}
				Fixture->World->BeginPlay();
				Fixture->World->GetWorldSettings()->NotifyBeginPlay();
				return false;
			}
			if (Step(*Fixture, Frame++) || Frame > 600)
			{
				Fixture.Reset();
				return true;
			}
			Fixture->World->Tick(LEVELTICK_All, DoubleJumpTestDeltaTime);
			return false;
		}

	private:
		FVector Spawn;
		FSetup Setup;
		FStep Step;
		TUniquePtr<FDoubleJumpTestWorld> Fixture;
		int32 Frame = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaDoubleJumpOnceTest, "Sava.DoubleJump.OnceInAir",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSavaDoubleJumpOnceTest::RunTest(const FString&)
{
	//足場から落ちた(ジャンプしていない)ときも、空中で跳べるのは1回だけ
	ADD_LATENT_AUTOMATION_COMMAND(FDoubleJumpCommand(FVector(0, 0, 3000), nullptr,
		[this](FDoubleJumpTestWorld& W, int32 Frame)
		{
			switch (Frame)
			{
			case 0:
				W.SetFalling(FVector(0, 0, -300));
				W.Press();
				return false;
			case 1:
				TestTrue(TEXT("Air jump while falling sets the double jump up speed"),
					FMath::IsNearlyEqual(W.Movement->Velocity.Z, W.Movement->DoubleJumpZVelocity, 30.0f));
				TestFalse(TEXT("No air jump left after using it"), W.Character->CanJump());
				W.Release();
				W.SetFalling(FVector(0, 0, -300));
				return false;
			case 2:
				W.Press();
				return false;
			case 3:
				TestTrue(TEXT("Second air press does nothing"), W.Movement->Velocity.Z < -250.0f);
				//壁走りなど、落下以外の移動に入ると回数が戻る
				W.Release();
				W.Movement->SetMovementMode(MOVE_Custom, CMOVE_WallRun);
				W.Movement->SetMovementMode(MOVE_Falling);
				TestTrue(TEXT("Leaving a custom mode (wall run) restores the air jump"), W.Character->CanJump());
				return true;
			default:
				return true;
			}
		}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaDoubleJumpSidewaysTest, "Sava.DoubleJump.RisingGoesSideways",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSavaDoubleJumpSidewaysTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FDoubleJumpCommand(FVector(0, 0, 3000), nullptr,
		[this](FDoubleJumpTestWorld& W, int32 Frame)
		{
			const float Gravity = FMath::Abs(W.Movement->GetGravityZ()) * DoubleJumpTestDeltaTime;
			if (Frame == 0)
			{
				//上昇速度が横へ飛ぶ基準以上: 上には足さず、横へ DoubleJumpSidewaysSpeed
				W.SetFalling(FVector(500, 0, 600));
				W.Press();
				return false;
			}
			TestTrue(TEXT("Fast rise keeps its up speed (nothing added upward)"),
				FMath::IsNearlyEqual(W.Movement->Velocity.Z, 600.0f - Gravity, 5.0f));
			TestTrue(TEXT("Fast rise turns the double jump sideways"),
				FMath::IsNearlyEqual(W.Movement->Velocity.Size2D(), 500.0f + W.Movement->DoubleJumpSidewaysSpeed, 15.0f));
			return true;
		}));
	ADD_LATENT_AUTOMATION_COMMAND(FDoubleJumpCommand(FVector(0, 0, 3000), nullptr,
		[this](FDoubleJumpTestWorld& W, int32 Frame)
		{
			if (Frame == 0)
			{
				//落下中: 横へのボーナスはなく、上へ跳ぶ
				W.SetFalling(FVector(500, 0, -200));
				W.Press();
				return false;
			}
			TestTrue(TEXT("Falling double jump goes up"), W.Movement->Velocity.Z > W.Movement->DoubleJumpZVelocity - 30.0f);
			TestTrue(TEXT("Falling double jump adds no sideways speed"), FMath::IsNearlyEqual(W.Movement->Velocity.Size2D(), 500.0f, 15.0f));
			return true;
		}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaDoubleJumpRedirectTest, "Sava.DoubleJump.Redirect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSavaDoubleJumpRedirectTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FDoubleJumpCommand(FVector(0, 0, 3000), nullptr,
		[this](FDoubleJumpTestWorld& W, int32 Frame)
		{
			W.Character->AddMovementInput(FVector::RightVector, 1.0f);
			if (Frame == 0)
			{
				W.SetFalling(FVector(800, 0, -100));
				return false;
			}
			if (Frame == 1)
			{
				//前へ進みながら右を入れて跳ぶ → 右へ向きが変わる(90度なので速度は DoubleJumpRedirectSpeedLoss の半分を失う)
				W.SetFalling(FVector(800, 0, -100));
				W.Press();
				return false;
			}
			const float Expected = 800.0f * (1.0f - W.Movement->DoubleJumpRedirectSpeedLoss * 0.5f);
			TestTrue(TEXT("Double jump turns toward the input"), FMath::Abs(W.Movement->Velocity.X) < 30.0f && W.Movement->Velocity.Y > 0.0f);
			//この後の1フレーム分の空中制御で少し速くなる分を許容する
			TestTrue(TEXT("Turning 90 degrees loses half of the redirect loss"), FMath::IsNearlyEqual(W.Movement->Velocity.Size2D(), Expected, 60.0f));
			return true;
		}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaDoubleJumpTimingBoostTest, "Sava.DoubleJump.TimingBoost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSavaDoubleJumpTimingBoostTest::RunTest(const FString&)
{
	//その場から地上ジャンプし、DelayFrames 後に前を入れて二段ジャンプしたときの水平の速さ
	//(ジャンプ直後ほど上昇が速いので、早く押すほど横へ飛ぶ)
	struct FResult { float Speed = -1.0f; };
	TSharedRef<FResult> Early = MakeShared<FResult>();
	TSharedRef<FResult> Late = MakeShared<FResult>();
	auto Run = [this](int32 DelayFrames, TSharedRef<FResult> Result)
	{
		TSharedRef<int32> JumpFrame = MakeShared<int32>(-1);
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDoubleJumpCommand>(FVector(0, 0, 120),
			[](FDoubleJumpTestWorld& W) { W.Box(FVector(0, 0, -20), FVector(5000, 5000, 20)); },
			[this, DelayFrames, Result, JumpFrame](FDoubleJumpTestWorld& W, int32 Frame)
			{
				//着地して止まるまで待つ
				if (*JumpFrame < 0)
				{
					if (Frame > 10 && W.Movement->IsMovingOnGround())
					{
						W.Press();
						*JumpFrame = Frame;
					}
					return false;
				}
				const int32 Since = Frame - *JumpFrame;
				if (Since == 1)
				{
					W.Release();
				}
				if (Since >= DelayFrames)
				{
					W.Character->AddMovementInput(FVector::ForwardVector, 1.0f);
				}
				if (Since == DelayFrames + 1)
				{
					W.Press();
				}
				if (Since == DelayFrames + 2)
				{
					Result->Speed = W.Movement->Velocity.Size2D();
					TestFalse(TEXT("Double jump was used"), W.Character->CanJump());
					return true;
				}
				return false;
			}));
	};
	Run(2, Early);
	Run(24, Late);
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Early, Late]()
	{
		AddInfo(FString::Printf(TEXT("Early double jump %.0f / late double jump %.0f"), Early->Speed, Late->Speed));
		TestTrue(TEXT("Both runs finished"), Early->Speed >= 0.0f && Late->Speed >= 0.0f);
		TestTrue(TEXT("Pressing the double jump sooner after the jump gives more horizontal speed"), Early->Speed > Late->Speed + 150.0f);
		return true;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaDoubleJumpWallRunPriorityTest, "Sava.DoubleJump.WallRunPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSavaDoubleJumpWallRunPriorityTest::RunTest(const FString&)
{
	//右側に壁(面は Y=60、カプセルの半径は 55)
	ADD_LATENT_AUTOMATION_COMMAND(FDoubleJumpCommand(FVector(0, 0, 3000),
		[](FDoubleJumpTestWorld& W) { W.Box(FVector(0, 80, 3000), FVector(5000, 20, 1000)); },
		[this](FDoubleJumpTestWorld& W, int32 Frame)
		{
			W.Character->AddMovementInput(FVector::ForwardVector, 1.0f);
			if (Frame == 0)
			{
				W.SetFalling(FVector(900, 0, -100));
				return false;
			}
			if (Frame == 1)
			{
				//壁の横でジャンプを押す → 二段ジャンプではなく壁走り(上に打ち上げられない)
				W.SetFalling(FVector(900, 0, -100));
				W.Press();
				W.Movement->SetJumpHeld(true);
				return false;
			}
			TestTrue(TEXT("Jump next to a wall starts a wall run"), W.Movement->IsWallRunning());
			TestTrue(TEXT("Wall run is not launched upward by a double jump"), W.Movement->Velocity.Z < 0.0f);
			return true;
		}));
	return true;
}
#endif

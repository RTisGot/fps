#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Skill/SavaBlinkAbility.h"

namespace
{
	struct FBlinkTestWorld
	{
		UWorld* World;
		ACharacter* Character;

		FBlinkTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Character = World->SpawnActor<ACharacter>(FVector(0, 0, 300), FRotator::ZeroRotator);
		}
		~FBlinkTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		void Box(const FVector& Center, const FVector& Extent)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
			Actor->SetRootComponent(Box);
			Box->SetBoxExtent(Extent);
			Box->SetCollisionProfileName(TEXT("BlockAll"));
			Box->SetWorldLocation(Center);
			Box->RegisterComponent();
		}

		// 目の高さ(カプセルの中心 + 60)から Direction の向きに狙う
		bool Find(const FVector& Direction, FVector& OutTarget) const
		{
			return USavaBlinkAbility::FindBlinkTarget(Character, Character->GetActorLocation() + FVector(0, 0, 60), Direction,
				1500.0f, 10.0f, 100.0f, OutTarget);
		}

		float Radius() const { return Character->GetCapsuleComponent()->GetScaledCapsuleRadius(); }
		float HalfHeight() const { return Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaBlinkTargetTest, "Sava.Skill.BlinkTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaBlinkTargetTest::RunTest(const FString&)
{
	//何もない空中: まっすぐ 15 m 先
	{
		FBlinkTestWorld Test;
		FVector Target;
		TestTrue(TEXT("Open air is valid"), Test.Find(FVector::ForwardVector, Target));
		TestTrue(TEXT("Open air moves the full range"), Target.Equals(FVector(1500, 0, 300), 1.0f));
	}

	//壁を狙う: 壁の手前で止まり、目の高さが狙った点に来る(カプセルの中心は今と同じ高さ)
	{
		FBlinkTestWorld Test;
		Test.Box(FVector(800, 0, 300), FVector(20, 1000, 1000));
		FVector Target;
		TestTrue(TEXT("Wall is valid"), Test.Find(FVector::ForwardVector, Target));
		TestEqual(TEXT("Stops in front of the wall"), Target.X, 780.0 - Test.Radius() - 10.0, 1.0);
		TestEqual(TEXT("Keeps the same height at a wall"), Target.Z, 300.0, 1.0);
	}

	//床を狙う: 床の上に立つ高さ
	{
		FBlinkTestWorld Test;
		Test.Box(FVector(0, 0, 0), FVector(3000, 3000, 20));
		FVector Target;
		const FVector Direction = (FVector(600, 0, 20) - FVector(0, 0, 360)).GetSafeNormal();
		TestTrue(TEXT("Floor is valid"), Test.Find(Direction, Target));
		TestEqual(TEXT("Lands on the floor"), Target.Z, 20.0 + Test.HalfHeight() + 10.0, 1.0);
		TestEqual(TEXT("Lands where aimed"), Target.X, 600.0, 1.0);
	}

	//目線より低い壁: 視線は越えるが体は通れないので、壁の手前で止まる(壁抜けしない)
	{
		FBlinkTestWorld Test;
		Test.Box(FVector(500, 0, 140), FVector(20, 1000, 140));
		FVector Target;
		TestTrue(TEXT("Low wall is valid"), Test.Find(FVector::ForwardVector, Target));
		TestTrue(TEXT("Does not pass the low wall"), Target.X <= 480.0 - Test.Radius() + 1.0);
	}

	//目の前が壁: 近すぎるので無効
	{
		FBlinkTestWorld Test;
		Test.Box(FVector(100, 0, 300), FVector(20, 1000, 1000));
		FVector Target;
		TestFalse(TEXT("Too close is invalid"), Test.Find(FVector::ForwardVector, Target));
	}
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Camera/SavaCameraEffectComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaCameraSpringTest, "Sava.Camera.Spring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaCameraSpringTest::RunTest(const FString&)
{
	FSavaCameraSpring Spring;
	TestFalse(TEXT("Starts inactive"), Spring.bActive);

	Spring.AddKick(FVector::ZeroVector, 0.1f);
	TestFalse(TEXT("Zero kick does nothing"), Spring.bActive);

	//Peak Time 後にちょうど Peak まで動く(1 回で進めても、細かく進めても同じ)
	const FVector Peak(2.0f, -1.0f, 0.5f);
	Spring.AddKick(Peak, 0.1f);
	TestTrue(TEXT("Kick activates"), Spring.bActive);
	Spring.Step(0.1f);
	TestTrue(TEXT("Reaches the peak at Peak Time"), Spring.Value.Equals(Peak, 0.001f));

	FSavaCameraSpring Fine;
	Fine.AddKick(Peak, 0.1f);
	for (int32 Index = 0; Index < 100; ++Index)
	{
		Fine.Step(0.001f);
	}
	TestTrue(TEXT("Same motion with small steps"), Fine.Value.Equals(Peak, 0.001f));

	//時間が経てば 0 に戻って止まる
	for (int32 Index = 0; Index < 200; ++Index)
	{
		Spring.Step(0.016f);
	}
	TestFalse(TEXT("Settles and stops"), Spring.bActive);
	TestTrue(TEXT("Back to zero"), Spring.Value.IsZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaCameraLandingTest, "Sava.Camera.Landing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaCameraLandingTest::RunTest(const FString&)
{
	TestEqual(TEXT("Slow landing: no shake"), USavaCameraEffectComponent::CalculateLandingAlpha(400.0f, 500.0f, 1500.0f), 0.0f);
	TestEqual(TEXT("Moving up: no shake"), USavaCameraEffectComponent::CalculateLandingAlpha(-300.0f, 500.0f, 1500.0f), 0.0f);
	TestEqual(TEXT("Halfway"), USavaCameraEffectComponent::CalculateLandingAlpha(1000.0f, 500.0f, 1500.0f), 0.5f);
	TestEqual(TEXT("Capped at max"), USavaCameraEffectComponent::CalculateLandingAlpha(5000.0f, 500.0f, 1500.0f), 1.0f);
	TestEqual(TEXT("Max not above min: full shake"), USavaCameraEffectComponent::CalculateLandingAlpha(600.0f, 500.0f, 500.0f), 1.0f);
	return true;
}

#endif

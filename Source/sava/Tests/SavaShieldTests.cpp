#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Skill/SavaDomeShield.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaDomeShieldIntersectTest, "Sava.Skill.DomeShieldIntersect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaDomeShieldIntersectTest::RunTest(const FString&)
{
	//中心 (0,0,0)、半径 400 のドーム
	const FVector Center = FVector::ZeroVector;
	const float Radius = 400.0f;
	float Time = -1.0f;

	//外から中へ水平に撃つ: 手前の表面(x = -400)で止まる
	TestTrue(TEXT("Outside to inside is blocked"),
		ASavaDomeShield::IntersectDome(Center, Radius, FVector(-1000, 0, 100), FVector(0, 0, 100), Time));
	TestEqual(TEXT("Stops at the near surface"),
		-1000.0f + 1000.0f * Time, -FMath::Sqrt(400.0f * 400.0f - 100.0f * 100.0f), 0.1f);

	//中から外へ撃つ: 表面で止まる
	TestTrue(TEXT("Inside to outside is blocked"),
		ASavaDomeShield::IntersectDome(Center, Radius, FVector(0, 0, 100), FVector(1000, 0, 100), Time));

	//中から中へ撃つ: 止まらない
	TestFalse(TEXT("Inside to inside is not blocked"),
		ASavaDomeShield::IntersectDome(Center, Radius, FVector(-100, 0, 100), FVector(100, 0, 100), Time));

	//外から外へ、ドームの上を通り過ぎる: 止まらない
	TestFalse(TEXT("Passing above is not blocked"),
		ASavaDomeShield::IntersectDome(Center, Radius, FVector(-1000, 0, 500), FVector(1000, 0, 500), Time));

	//外から外へ、ドームを貫通する: 止まる
	TestTrue(TEXT("Passing through is blocked"),
		ASavaDomeShield::IntersectDome(Center, Radius, FVector(-1000, 0, 100), FVector(1000, 0, 100), Time));

	//地面より下(ドームの下半分)は止めない
	TestFalse(TEXT("Lower half is not a shield"),
		ASavaDomeShield::IntersectDome(Center, Radius, FVector(-1000, 0, -100), FVector(0, 0, -100), Time));

	//届かない(線分が表面の手前で終わる)
	TestFalse(TEXT("Segment ends before the surface"),
		ASavaDomeShield::IntersectDome(Center, Radius, FVector(-1000, 0, 100), FVector(-500, 0, 100), Time));
	return true;
}

#endif

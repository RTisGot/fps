#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Gadget/FragGrenade.h"

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

#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Weapon/SavaWeaponTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaWeaponDamageFalloffTest, "Sava.Weapon.DamageFalloff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaWeaponDamageFalloffTest::RunTest(const FString&)
{
	FSavaWeaponStats Stats;
	Stats.BaseDamage = 30.0f;
	Stats.HeadDamage = 60.0f;
	Stats.MinimumDamage = 15.0f;
	Stats.FalloffStartDistance = 1000.0f;
	Stats.FalloffEndDistance = 3000.0f;

	TestEqual(TEXT("Before falloff start"), Stats.CalculateDamage(500.0f, false), 30.0f);
	TestEqual(TEXT("Halfway through falloff"), Stats.CalculateDamage(2000.0f, false), 22.5f);
	TestEqual(TEXT("After falloff end"), Stats.CalculateDamage(5000.0f, false), 15.0f);
	TestEqual(TEXT("Headshot before falloff"), Stats.CalculateDamage(500.0f, true), 60.0f);
	TestEqual(TEXT("Headshot falls off by the same ratio"), Stats.CalculateDamage(5000.0f, true), 30.0f);

	Stats.RateOfFire = 600.0f;
	TestEqual(TEXT("600 RPM = 0.1s"), Stats.GetFireInterval(), 0.1f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaWeaponSpreadTest, "Sava.Weapon.Spread",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaWeaponSpreadTest::RunTest(const FString&)
{
	FSavaWeaponStats Stats;
	Stats.HipSpread = 2.0f;
	Stats.ADSSpread = 0.4f;
	Stats.MovementSpread = 1.5f;
	Stats.JumpSpread = 4.0f;
	Stats.ADSMovementSpreadScale = 0.5f;

	TestEqual(TEXT("Hip, standing still"), Stats.CalculateSpread(0.0f, 0.0f, false), 2.0f);
	TestEqual(TEXT("Hip, moving and in the air: full movement spread"), Stats.CalculateSpread(0.0f, 1.0f, true), 2.0f + 1.5f + 4.0f);
	TestEqual(TEXT("ADS, moving and in the air: movement spread halved"), Stats.CalculateSpread(1.0f, 1.0f, true), 0.4f + (1.5f + 4.0f) * 0.5f);
	TestEqual(TEXT("Halfway into ADS: scale is halfway too"), Stats.CalculateSpread(0.5f, 1.0f, false), 1.2f + 1.5f * 0.75f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaWeaponModifierTest, "Sava.Weapon.Modifiers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaWeaponModifierTest::RunTest(const FString&)
{
	FSavaWeaponStats Base;
	Base.BaseDamage = 20.0f;
	Base.MagazineSize = 30;
	Base.HipSpread = 2.0f;

	//例: 拡張マガジン(弾数 +10、ADS が遅くなる)と、軽量バレル(ダメージ 0.9 倍、拡散 0.5 倍)
	FSavaWeaponModifier ExtendedMag;
	ExtendedMag.MagazineSizeAdd = 10;
	ExtendedMag.ADSTimeMultiplier = 1.2f;

	FSavaWeaponModifier LightBarrel;
	LightBarrel.DamageMultiplier = 0.9f;
	LightBarrel.HipSpreadMultiplier = 0.5f;

	const FSavaWeaponStats Result = USavaWeaponLibrary::ApplyWeaponModifiers(Base, { ExtendedMag, LightBarrel });
	TestEqual(TEXT("Magazine adds"), Result.MagazineSize, 40);
	TestEqual(TEXT("Damage multiplies"), Result.BaseDamage, 18.0f);
	TestEqual(TEXT("Spread multiplies"), Result.HipSpread, 1.0f);
	TestEqual(TEXT("ADS time multiplies"), Result.ADSTime, Base.ADSTime * 1.2f);

	FSavaWeaponModifier TinyMag;
	TinyMag.MagazineSizeAdd = -100;
	TestEqual(TEXT("Magazine never drops below 1"), USavaWeaponLibrary::ApplyWeaponModifiers(Base, { TinyMag }).MagazineSize, 1);
	return true;
}

#endif

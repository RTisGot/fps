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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaWeaponTuningTest, "Sava.Weapon.Tuning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaWeaponTuningTest::RunTest(const FString&)
{
	const FSavaWeaponStats Base;
	const FSavaWeaponTuningStrength Strength;
	auto Tune = [&Base](const FSavaWeaponTuning& Tuning, const FSavaWeaponTuningStrength& InStrength)
	{
		return USavaWeaponLibrary::ApplyWeaponModifiers(Base, { USavaWeaponLibrary::MakeTuningModifier(Base, Tuning, InStrength) });
	};

	//スライダーが全部 0 なら何も変わらない
	const FSavaWeaponStats Neutral = Tune(FSavaWeaponTuning(), Strength);
	TestTrue(TEXT("Neutral tuning changes nothing"), FSavaWeaponStats::StaticStruct()->CompareScriptStruct(&Neutral, &Base, PPF_None));

	//弾薬: 威力を上げた分だけ連射が遅くなり、DPS は変わらない
	FSavaWeaponTuning HeavyAmmo;
	HeavyAmmo.Ammo = 1.0f;
	const FSavaWeaponStats Heavy = Tune(HeavyAmmo, Strength);
	TestEqual(TEXT("Heavy ammo raises damage"), Heavy.BaseDamage, Base.BaseDamage * 1.15f, 0.01f);
	TestEqual(TEXT("Heavy ammo raises head damage too"), Heavy.HeadDamage, Base.HeadDamage * 1.15f, 0.01f);
	TestEqual(TEXT("Heavy ammo keeps DPS"), Heavy.BaseDamage / Heavy.GetFireInterval(), Base.BaseDamage / Base.GetFireInterval(), 0.01f);

	//- 側は + 側の逆数
	FSavaWeaponTuning LightAmmo;
	LightAmmo.Ammo = -1.0f;
	TestEqual(TEXT("Light ammo is the inverse"), Tune(LightAmmo, Strength).RateOfFire, Base.RateOfFire * 1.15f, 0.01f);

	//マガジン: 弾数は元の弾数の 25% を丸めて増減
	FSavaWeaponTuning SmallMag;
	SmallMag.Magazine = -1.0f;
	const FSavaWeaponStats Small = Tune(SmallMag, Strength);
	TestEqual(TEXT("Small magazine: 30 -> 22"), Small.MagazineSize, 22);
	TestEqual(TEXT("Small magazine reloads faster"), Small.ReloadTime, Base.ReloadTime / 1.25f, 0.001f);

	//バレル: 途中の値は倍率の途中になる(+0.5 で 1.25 ^ 0.5)
	FSavaWeaponTuning HalfLongBarrel;
	HalfLongBarrel.Barrel = 0.5f;
	const FSavaWeaponStats HalfLong = Tune(HalfLongBarrel, Strength);
	TestEqual(TEXT("Half long barrel: falloff"), HalfLong.FalloffStartDistance, Base.FalloffStartDistance * FMath::Sqrt(1.25f), 0.1f);
	TestEqual(TEXT("Half long barrel: hip spread"), HalfLong.HipSpread, Base.HipSpread * FMath::Sqrt(1.3f), 0.001f);

	//武器ごとの強さ: 0 なら効かない、2 なら倍率が 2 乗
	FSavaWeaponTuningStrength NoBarrel;
	NoBarrel.Barrel = 0.0f;
	TestEqual(TEXT("Strength 0 disables the slider"), Tune(HalfLongBarrel, NoBarrel).HipSpread, Base.HipSpread);
	FSavaWeaponTuningStrength DoubleMuzzle;
	DoubleMuzzle.Muzzle = 2.0f;
	FSavaWeaponTuning Compensator;
	Compensator.Muzzle = 1.0f;
	TestEqual(TEXT("Strength 2 squares the multiplier"), Tune(Compensator, DoubleMuzzle).VerticalRecoil, Base.VerticalRecoil * 0.64f, 0.001f);

	//範囲外・細かすぎる値は直す
	FSavaWeaponTuning Broken;
	Broken.Ammo = 5.0f;
	Broken.Frame = -0.123456f;
	Broken.Stock = std::numeric_limits<float>::quiet_NaN();
	const FSavaWeaponTuning Fixed = Broken.GetSanitized();
	TestEqual(TEXT("Clamped to 1"), Fixed.Ammo, 1.0f);
	TestEqual(TEXT("Rounded to 0.01"), Fixed.Frame, -0.12f);
	TestEqual(TEXT("NaN becomes 0"), Fixed.Stock, 0.0f);
	TestEqual(TEXT("Out-of-range input is clamped before use"), Tune(Broken, Strength).BaseDamage, Base.BaseDamage * 1.15f, 0.01f);
	return true;
}

#endif

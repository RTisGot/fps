#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Weapon/SavaWeaponData.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaWeaponSliderTest, "Sava.Weapon.TuningSlider",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaWeaponSliderTest::RunTest(const FString&)
{
	FSavaWeaponTuningSlider Slider;
	Slider.MaxModifier.DamageMultiplier = 1.21f;
	Slider.MaxModifier.MagazineSizeAdd = 8;
	Slider.MinModifier.DamageMultiplier = 0.5f;
	Slider.MinModifier.MagazineSizeAdd = -5;

	TestEqual(TEXT("0 changes nothing"), USavaWeaponLibrary::MakeSliderModifier(Slider, 0.0f).DamageMultiplier, 1.0f);
	TestEqual(TEXT("0 adds nothing"), USavaWeaponLibrary::MakeSliderModifier(Slider, 0.0f).MagazineSizeAdd, 0);
	TestEqual(TEXT("+1 uses the max end"), USavaWeaponLibrary::MakeSliderModifier(Slider, 1.0f).DamageMultiplier, 1.21f, 0.0001f);
	TestEqual(TEXT("-1 uses the min end"), USavaWeaponLibrary::MakeSliderModifier(Slider, -1.0f).DamageMultiplier, 0.5f, 0.0001f);
	TestEqual(TEXT("+0.5 is the square root of the max end"), USavaWeaponLibrary::MakeSliderModifier(Slider, 0.5f).DamageMultiplier, 1.1f, 0.0001f);
	TestEqual(TEXT("+0.5 adds half"), USavaWeaponLibrary::MakeSliderModifier(Slider, 0.5f).MagazineSizeAdd, 4);
	TestEqual(TEXT("-0.5 rounds away from zero"), USavaWeaponLibrary::MakeSliderModifier(Slider, -0.5f).MagazineSizeAdd, -3);
	TestEqual(TEXT("Out of range is clamped"), USavaWeaponLibrary::MakeSliderModifier(Slider, 3.0f).DamageMultiplier, 1.21f, 0.0001f);

	TestEqual(TEXT("Rounded to 0.01"), USavaWeaponLibrary::SanitizeSliderValue(-0.123456f), -0.12f);
	TestEqual(TEXT("NaN becomes 0"), USavaWeaponLibrary::SanitizeSliderValue(std::numeric_limits<float>::quiet_NaN()), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaWeaponTuningTest, "Sava.Weapon.Tuning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaWeaponTuningTest::RunTest(const FString&)
{
	USavaWeaponData* Weapon = NewObject<USavaWeaponData>();
	const FSavaWeaponStats& Base = Weapon->Stats;
	TestEqual(TEXT("New weapons start with the recommended 6 sliders"), Weapon->TuningSliders.Num(), 6);

	//スライダーが全部 0 なら何も変わらない
	const FSavaWeaponStats Neutral = Weapon->GetTunedStats(FSavaWeaponTuning());
	TestTrue(TEXT("Neutral tuning changes nothing"), FSavaWeaponStats::StaticStruct()->CompareScriptStruct(&Neutral, &Base, PPF_None));

	//弾薬: 威力を上げた分だけ連射が遅くなり、DPS は変わらない
	FSavaWeaponTuning HeavyAmmo;
	HeavyAmmo.SetSliderValue(TEXT("Ammo"), 1.0f);
	const FSavaWeaponStats Heavy = Weapon->GetTunedStats(HeavyAmmo);
	TestEqual(TEXT("Heavy ammo raises damage"), Heavy.BaseDamage, Base.BaseDamage * 1.15f, 0.01f);
	TestEqual(TEXT("Heavy ammo keeps DPS"), Heavy.BaseDamage / Heavy.GetFireInterval(), Base.BaseDamage / Base.GetFireInterval(), 0.01f);

	//マガジン: - 側は + 側の逆
	FSavaWeaponTuning SmallMag;
	SmallMag.SetSliderValue(TEXT("Magazine"), -1.0f);
	const FSavaWeaponStats Small = Weapon->GetTunedStats(SmallMag);
	TestEqual(TEXT("Small magazine: 30 -> 22"), Small.MagazineSize, 22);
	TestEqual(TEXT("Small magazine reloads faster"), Small.ReloadTime, Base.ReloadTime / 1.25f, 0.001f);

	//切り替え: 選んだ選択肢の効果が付く。選べないものは最初の選択肢になる
	FSavaWeaponTuningToggle& Sight = Weapon->TuningToggles.AddDefaulted_GetRef();
	Sight.Id = TEXT("Sight");
	Sight.Options.AddDefaulted_GetRef().Id = TEXT("Iron");
	FSavaWeaponTuningOption& Scope = Sight.Options.AddDefaulted_GetRef();
	Scope.Id = TEXT("Scope");
	Scope.Modifier.ADSZoomMultiplier = 0.5f;

	FSavaWeaponTuning WithScope;
	WithScope.SetToggleOption(TEXT("Sight"), TEXT("Scope"));
	TestEqual(TEXT("Scope zooms in"), Weapon->GetTunedStats(WithScope).ADSZoom, Base.ADSZoom * 0.5f, 0.0001f);
	TestEqual(TEXT("No choice means the first option"), Weapon->SanitizeTuning(FSavaWeaponTuning()).GetToggleOption(TEXT("Sight")), FName(TEXT("Iron")));

	//この武器に無いもの・範囲外の値は直す
	FSavaWeaponTuning Broken;
	Broken.SetSliderValue(TEXT("Unknown"), 1.0f);
	Broken.SetSliderValue(TEXT("Barrel"), 5.0f);
	Broken.SetToggleOption(TEXT("Sight"), TEXT("Unknown"));
	const FSavaWeaponTuning Fixed = Weapon->SanitizeTuning(Broken);
	TestFalse(TEXT("Unknown slider is removed"), Fixed.Sliders.ContainsByPredicate([](const FSavaWeaponTuningSliderValue& Value) { return Value.Slider == TEXT("Unknown"); }));
	TestEqual(TEXT("Slider is clamped to 1"), Fixed.GetSliderValue(TEXT("Barrel")), 1.0f);
	TestEqual(TEXT("Unknown option falls back to the first"), Fixed.GetToggleOption(TEXT("Sight")), FName(TEXT("Iron")));
	TestEqual(TEXT("Order follows the weapon data"), Fixed.Sliders[0].Slider, FName(TEXT("Ammo")));
	TestTrue(TEXT("Sanitized tuning is stable"), Weapon->SanitizeTuning(Fixed) == Fixed);
	return true;
}

#endif

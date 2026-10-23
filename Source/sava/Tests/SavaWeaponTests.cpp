#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Weapon/SavaWeaponData.h"
#include "Loadout/SavaLoadoutStatLibrary.h"
#include "savaCharacter.h"

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
	Stats.ADSShotSpreadScale = 0.5f;

	//移動・空中の拡散 → ADS で減らして足す
	auto Spread = [&Stats](float ADSAlpha, float MoveAlpha, bool bInAir, float ShotSpread = 0.0f, ESavaSpreadMovement Movement = ESavaSpreadMovement::Walk)
	{
		return Stats.CalculateSpreadBreakdown(ADSAlpha, Stats.CalculateMovementSpread(MoveAlpha, bInAir, Movement), ShotSpread).Total;
	};
	TestEqual(TEXT("Hip, standing still"), Spread(0.0f, 0.0f, false), 2.0f);
	TestEqual(TEXT("Hip, moving and in the air: full movement spread"), Spread(0.0f, 1.0f, true), 2.0f + 1.5f + 4.0f);
	TestEqual(TEXT("ADS, moving and in the air: movement spread halved"), Spread(1.0f, 1.0f, true), 0.4f + (1.5f + 4.0f) * 0.5f);
	TestEqual(TEXT("Halfway into ADS: scale is halfway too"), Spread(0.5f, 1.0f, false), 1.2f + 1.5f * 0.75f);
	TestEqual(TEXT("Hip, shot spread is added"), Spread(0.0f, 0.0f, false, 1.0f), 3.0f);
	TestEqual(TEXT("ADS, shot spread is scaled by its own scale"), Spread(1.0f, 0.0f, false, 1.0f), 0.4f + 0.5f);

	//動き方ごとの拡散(歩きの拡散の代わりに使う)
	Stats.SprintSpread = 3.0f;
	Stats.SlideSpread = 2.0f;
	Stats.GrappleSpread = 5.0f;
	TestEqual(TEXT("Sprint replaces walk spread"), Spread(0.0f, 1.0f, false, 0.0f, ESavaSpreadMovement::Sprint), 2.0f + 3.0f);
	TestEqual(TEXT("Sprint (boosted walk) scales with speed"), Spread(0.0f, 0.5f, false, 0.0f, ESavaSpreadMovement::Sprint), 2.0f + 1.5f);
	TestEqual(TEXT("Slide ignores speed"), Spread(0.0f, 0.2f, false, 0.0f, ESavaSpreadMovement::Slide), 2.0f + 2.0f);
	TestEqual(TEXT("Grapple ignores air"), Spread(0.0f, 1.0f, true, 0.0f, ESavaSpreadMovement::Grapple), 2.0f + 5.0f);
	TestEqual(TEXT("ADS scales sprint spread"), Spread(1.0f, 1.0f, false, 0.0f, ESavaSpreadMovement::Sprint), 0.4f + 3.0f * 0.5f);

	//移動と連射は、ADS で別々の割合に減る
	Stats.ADSShotSpreadScale = 0.25f;
	const FSavaSpreadBreakdown Separate = Stats.CalculateSpreadBreakdown(1.0f, 2.0f, 2.0f);
	TestEqual(TEXT("ADS movement scale"), Separate.Movement, 1.0f);
	TestEqual(TEXT("ADS shot scale"), Separate.Shot, 0.5f);
	TestEqual(TEXT("Total is the sum"), Separate.Total, 0.4f + 1.0f + 0.5f);

	//1 発目は基本の拡散だけ減る
	Stats.FirstShotSpreadScale = 0.0f;
	const FSavaSpreadBreakdown First = Stats.CalculateSpreadBreakdown(0.0f, 1.0f, 0.0f, true);
	TestEqual(TEXT("First shot removes base spread"), First.Base, 0.0f);
	TestEqual(TEXT("First shot keeps movement spread"), First.Movement, 1.0f);
	TestEqual(TEXT("Not the first shot: base spread stays"), Stats.CalculateSpreadBreakdown(0.0f, 0.0f, 0.0f, false).Base, 2.0f);

	FSavaWeaponModifier HeavyFrame;
	HeavyFrame.MovementSpreadMultiplier = 2.0f;
	const FSavaWeaponStats Heavy = USavaWeaponLibrary::ApplyWeaponModifiers(Stats, { HeavyFrame });
	TestEqual(TEXT("Movement multiplier scales sprint"), Heavy.SprintSpread, 6.0f);
	TestEqual(TEXT("Movement multiplier scales slide"), Heavy.SlideSpread, 4.0f);
	TestEqual(TEXT("Movement multiplier scales grapple"), Heavy.GrappleSpread, 10.0f);

	//連射の拡散は、連射間隔が過ぎてから戻り始める
	Stats.RateOfFire = 600.0f;
	Stats.ShotSpreadRecovery = 10.0f;
	TestEqual(TEXT("Shot spread holds within the fire interval"), Stats.CalculateShotSpread(1.0f, 0.1f), 1.0f);
	TestEqual(TEXT("Shot spread recovers after the fire interval"), Stats.CalculateShotSpread(1.0f, 0.15f), 0.5f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Shot spread never goes below 0"), Stats.CalculateShotSpread(1.0f, 5.0f), 0.0f);
	Stats.ShotSpreadRecoveryDelay = 0.3f;
	TestEqual(TEXT("Recovery delay holds longer than the interval"), Stats.CalculateShotSpread(1.0f, 0.25f), 1.0f);
	TestEqual(TEXT("Recovery starts after the delay"), Stats.CalculateShotSpread(1.0f, 0.35f), 0.5f, KINDA_SMALL_NUMBER);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaWeaponSpreadBlendTest, "Sava.Weapon.SpreadBlend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaWeaponSpreadBlendTest::RunTest(const FString&)
{
	FSavaWeaponStats Stats;
	Stats.MovementSpreadInTime = 0.1f;
	Stats.MovementSpreadOutTime = 0.4f;

	//Blend Time で差の 95% が縮む
	TestEqual(TEXT("Grows to 95% in the in time"), Stats.BlendMovementSpread(0.0f, 4.0f, 0.1f), 4.0f * 0.95f, 0.01f);
	TestEqual(TEXT("Shrinks to 5% in the out time"), Stats.BlendMovementSpread(4.0f, 0.0f, 0.4f), 4.0f * 0.05f, 0.01f);
	TestTrue(TEXT("Shrinking is slower than growing"), Stats.BlendMovementSpread(4.0f, 0.0f, 0.1f) > 4.0f - Stats.BlendMovementSpread(0.0f, 4.0f, 0.1f));

	//時間を刻んでも、まとめて進めても同じ
	float Stepped = 0.0f;
	for (int32 Step = 0; Step < 10; ++Step)
	{
		Stepped = Stats.BlendMovementSpread(Stepped, 4.0f, 0.01f);
	}
	TestEqual(TEXT("Frame rate independent"), Stepped, Stats.BlendMovementSpread(0.0f, 4.0f, 0.1f), 0.001f);

	Stats.MovementSpreadInTime = 0.0f;
	TestEqual(TEXT("0 = instant"), Stats.BlendMovementSpread(0.0f, 4.0f, 0.001f), 4.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaWeaponSpreadDistributionTest, "Sava.Weapon.SpreadDistribution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaWeaponSpreadDistributionTest::RunTest(const FString&)
{
	//乱数 0.25 で半径の半分 = 均等(Center Ratio 0.25)。乱数 0.5 で半径の半分 = Center Ratio 0.5
	TestEqual(TEXT("Uniform: half radius at rand 0.25"), static_cast<float>(FSavaWeaponStats::SampleConeOffset(4.0f, 0.25f, 0.25f, 0.0f).Size()), 2.0f, 0.001f);
	TestEqual(TEXT("Ratio 0.5: half radius at rand 0.5"), static_cast<float>(FSavaWeaponStats::SampleConeOffset(4.0f, 0.5f, 0.5f, 0.0f).Size()), 2.0f, 0.001f);
	TestEqual(TEXT("Never beyond the cone"), static_cast<float>(FSavaWeaponStats::SampleConeOffset(4.0f, 0.9f, 1.0f, 0.3f).Size()), 4.0f, 0.001f);
	TestTrue(TEXT("No spread"), FSavaWeaponStats::SampleConeOffset(0.0f, 0.5f, 0.7f, 0.3f).IsZero());

	//向き: 0 で右、0.25 で上
	TestTrue(TEXT("Angle 0 points right"), FSavaWeaponStats::SampleConeOffset(2.0f, 0.5f, 1.0f, 0.0f).Equals(FVector2D(2.0f, 0.0f), 0.001f));
	TestTrue(TEXT("Angle 0.25 points up"), FSavaWeaponStats::SampleConeOffset(2.0f, 0.5f, 1.0f, 0.25f).Equals(FVector2D(0.0f, 2.0f), 0.001f));

	//たくさん撃つと、半径の半分より内側に入る割合が Center Ratio になる
	FSavaWeaponStats Stats;
	Stats.SpreadCenterRatio = 0.7f;
	FRandomStream Random(1234);
	int32 Inside = 0;
	constexpr int32 NumShots = 20000;
	for (int32 Shot = 0; Shot < NumShots; ++Shot)
	{
		if (Stats.SampleSpreadOffset(2.0f, Random.FRand(), Random.FRand()).Size() < 1.0f)
		{
			++Inside;
		}
	}
	TestEqual(TEXT("Center ratio matches the share inside half radius"), static_cast<float>(Inside) / NumShots, 0.7f, 0.02f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaWeaponReloadStatsTest, "Sava.Weapon.ReloadStats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaWeaponReloadStatsTest::RunTest(const FString&)
{
	FSavaWeaponStats Stats;
	Stats.MagazineSize = 30;
	Stats.ReloadTime = 2.0f;
	Stats.EmptyReloadTime = 3.0f;
	Stats.ReloadInsertRatio = 0.5f;

	TestEqual(TEXT("Tactical reload time"), Stats.GetReloadDuration(false), 2.0f);
	TestEqual(TEXT("Empty reload time"), Stats.GetReloadDuration(true), 3.0f);
	TestEqual(TEXT("Tactical insert time"), Stats.GetReloadInsertTime(false), 1.0f);
	TestEqual(TEXT("Empty insert time"), Stats.GetReloadInsertTime(true), 1.5f);

	TestEqual(TEXT("No chamber: same capacity"), Stats.GetMagazineCapacity(true), 30);
	Stats.bChamberRound = true;
	TestEqual(TEXT("Chamber: +1 when a round was left"), Stats.GetMagazineCapacity(true), 31);
	TestEqual(TEXT("Chamber: no +1 from empty"), Stats.GetMagazineCapacity(false), 30);

	//Reload Time Multiplier はリロードの時間すべてに掛かる
	FSavaWeaponModifier Fast;
	Fast.ReloadTimeMultiplier = 0.5f;
	Stats.ReloadStartTime = 0.4f;
	Stats.ReloadRoundTime = 0.6f;
	Stats.ReloadEndTime = 0.2f;
	const FSavaWeaponStats Tuned = USavaWeaponLibrary::ApplyWeaponModifiers(Stats, { Fast });
	TestEqual(TEXT("Tactical time scaled"), Tuned.ReloadTime, 1.0f);
	TestEqual(TEXT("Empty time scaled"), Tuned.EmptyReloadTime, 1.5f);
	TestEqual(TEXT("Start time scaled"), Tuned.ReloadStartTime, 0.2f);
	TestEqual(TEXT("Round time scaled"), Tuned.ReloadRoundTime, 0.3f);
	TestEqual(TEXT("End time scaled"), Tuned.ReloadEndTime, 0.1f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaWeaponRecoilPatternTest, "Sava.Weapon.RecoilPattern",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaWeaponRecoilPatternTest::RunTest(const FString&)
{
	FSavaWeaponStats Stats;
	Stats.VerticalRecoil = 0.5f;
	Stats.HorizontalRecoil = 0.2f;
	Stats.RecoilRandomness = 0.1f;

	const FVector2D NoNoise = FVector2D::ZeroVector;
	auto TestKick = [this](const TCHAR* What, const FVector2D& Actual, const FVector2D& Expected)
	{
		TestTrue(FString::Printf(TEXT("%s: %s (expected %s)"), What, *Actual.ToString(), *Expected.ToString()), Actual.Equals(Expected, KINDA_SMALL_NUMBER));
	};
	TestKick(TEXT("Empty pattern: straight up"), Stats.CalculateRecoilKick(5, NoNoise), FVector2D(0.0f, 0.5f));

	//4 発のパターン。最後まで撃ったら 2 発目(番号 1)から繰り返す
	Stats.RecoilPattern = { FVector2D(0.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(-1.0f, 2.0f), FVector2D(0.5f, 0.0f) };
	Stats.RecoilPatternLoopStart = 1;
	TestKick(TEXT("1st shot"), Stats.CalculateRecoilKick(0, NoNoise), FVector2D(0.0f, 0.5f));
	TestKick(TEXT("3rd shot is scaled"), Stats.CalculateRecoilKick(2, NoNoise), FVector2D(-0.2f, 1.0f));
	TestKick(TEXT("5th shot loops to index 1"), Stats.CalculateRecoilKick(4, NoNoise), FVector2D(0.2f, 0.5f));
	TestKick(TEXT("7th shot loops to index 3"), Stats.CalculateRecoilKick(6, NoNoise), FVector2D(0.1f, 0.0f));

	//ずれは Recoil Randomness の分だけ(パターンの値に足してから掛ける)
	TestKick(TEXT("Max noise"), Stats.CalculateRecoilKick(0, FVector2D(1.0f, -1.0f)), FVector2D(0.1f * 0.2f, 0.9f * 0.5f));

	Stats.RecoilPatternLoopStart = 100;
	TestKick(TEXT("Loop start past the end repeats the last shot"), Stats.CalculateRecoilKick(10, NoNoise), FVector2D(0.1f, 0.0f));
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

	FSavaWeaponModifier LightStock;
	LightStock.ADSMoveSpeedMultiplier = 1.2f;
	TestEqual(TEXT("ADS move speed multiplies"), USavaWeaponLibrary::ApplyWeaponModifiers(Base, { LightStock }).ADSMoveSpeed, Base.ADSMoveSpeed * 1.2f);
	LightStock.ADSMoveSpeedMultiplier = 10.0f;
	TestEqual(TEXT("ADS move speed never exceeds 1"), USavaWeaponLibrary::ApplyWeaponModifiers(Base, { LightStock }).ADSMoveSpeed, 1.0f);

	FSavaWeaponModifier QuickHands;
	QuickHands.EquipTimeMultiplier = 0.5f;
	QuickHands.SprintToFireTimeMultiplier = 0.5f;
	const FSavaWeaponStats Quick = USavaWeaponLibrary::ApplyWeaponModifiers(Base, { QuickHands });
	TestEqual(TEXT("Equip time multiplies"), Quick.EquipTime, Base.EquipTime * 0.5f);
	TestEqual(TEXT("Sprint to fire time multiplies"), Quick.SprintToFireTime, Base.SprintToFireTime * 0.5f);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaWeaponStatRowsTest, "Sava.Weapon.StatRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaWeaponStatRowsTest::RunTest(const FString&)
{
	auto FindRow = [](const TArray<FSavaStatRow>& Rows, const TCHAR* Label) -> const FSavaStatRow*
	{
		return Rows.FindByPredicate([Label](const FSavaStatRow& Row) { return Row.Label.ToString() == Label; });
	};

	FSavaWeaponStats Base;

	//マズル(跳ね上がり抑制): 縦は良くなり、横は悪くなる(まとめた 1 行では差が見えなかった)
	FSavaWeaponModifier Muzzle;
	Muzzle.VerticalRecoilMultiplier = 0.8f;
	Muzzle.HorizontalRecoilMultiplier = 1.25f;
	const TArray<FSavaStatRow> Rows = USavaLoadoutStatLibrary::MakeWeaponStatRows(USavaWeaponLibrary::ApplyWeaponModifiers(Base, { Muzzle }), Base);

	const FSavaStatRow* Vertical = FindRow(Rows, TEXT("縦反動"));
	const FSavaStatRow* Horizontal = FindRow(Rows, TEXT("横反動"));
	if (TestNotNull(TEXT("Vertical recoil row"), Vertical) && TestNotNull(TEXT("Horizontal recoil row"), Horizontal))
	{
		TestEqual(TEXT("Less vertical recoil is better"), Vertical->Change, ESavaStatChange::Better);
		TestEqual(TEXT("More horizontal recoil is worse"), Horizontal->Change, ESavaStatChange::Worse);
	}
	const FSavaStatRow* Damage = FindRow(Rows, TEXT("ダメージ"));
	if (TestNotNull(TEXT("Damage row"), Damage))
	{
		TestEqual(TEXT("Unchanged stat has no change"), Damage->Change, ESavaStatChange::None);
	}

	//決めた 15 行だけを、決めた順に出す
	const TArray<FString> ExpectedLabels = { TEXT("ダメージ"), TEXT("連射速度"), TEXT("有効射程"), TEXT("最大射程"), TEXT("マガジン"), TEXT("リロード"),
		TEXT("持ち替え"), TEXT("射撃遅延"), TEXT("ADS速度"), TEXT("ズーム倍率"), TEXT("縦反動"), TEXT("横反動"), TEXT("反動制御"), TEXT("基本拡散"), TEXT("ADS拡散") };
	TArray<FString> Labels;
	for (const FSavaStatRow& Row : Rows)
	{
		Labels.Add(Row.Label.ToString());
	}
	TestEqual(TEXT("Stat labels and order"), FString::Join(Labels, TEXT(",")), FString::Join(ExpectedLabels, TEXT(",")));

	//ズームは視野角の倍率の逆数で見せる
	const FSavaStatRow* Zoom = FindRow(Rows, TEXT("ズーム倍率"));
	if (TestNotNull(TEXT("Zoom row"), Zoom))
	{
		TestEqual(TEXT("ADS Zoom 0.8 shows as x1.25"), Zoom->ValueText.ToString(), FString(TEXT("×1.25")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaWeaponADSViewTest, "Sava.Weapon.ADSView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaWeaponADSViewTest::RunTest(const FString&)
{
	FSavaWeaponStats Stats;
	Stats.ADSTime = 0.2f;
	Stats.ADSEase = 2.0f;

	//半分の時間で 1 - 0.5^2 = 75% まで入る(最初は速い)。ADS Time で入りきる
	TestEqual(TEXT("Half time: 75% in"), Stats.StepADSViewAlpha(0.0f, true, 0.1f), 0.75f, 0.001f);
	TestEqual(TEXT("Full time: fully in"), Stats.StepADSViewAlpha(0.0f, true, 0.2f), 1.0f);
	//抜けるときも最初は速い
	TestEqual(TEXT("Half time out: 25% left"), Stats.StepADSViewAlpha(1.0f, false, 0.1f), 0.25f, 0.001f);
	TestEqual(TEXT("Full time out"), Stats.StepADSViewAlpha(1.0f, false, 0.2f), 0.0f);

	//刻んで進めても、まとめて進めても同じ
	float Stepped = 0.0f;
	for (int32 Step = 0; Step < 10; ++Step)
	{
		Stepped = Stats.StepADSViewAlpha(Stepped, true, 0.01f);
	}
	TestEqual(TEXT("Frame rate independent"), Stepped, 0.75f, 0.001f);

	//途中で向きを変えても値は飛ばない(ほんの少し進んだだけ)
	const float Reversed = Stats.StepADSViewAlpha(0.75f, false, 0.001f);
	TestTrue(TEXT("Reversing does not jump"), Reversed < 0.75f && Reversed > 0.7f);

	//Ease 1 は一定の速さ
	Stats.ADSEase = 1.0f;
	TestEqual(TEXT("Ease 1 is linear"), Stats.StepADSViewAlpha(0.0f, true, 0.1f), 0.5f, 0.001f);

	//ADS Time 0 は一瞬
	Stats.ADSTime = 0.0f;
	TestEqual(TEXT("Instant in"), Stats.StepADSViewAlpha(0.0f, true, 0.001f), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaDamageDirectionTest, "Sava.Weapon.DamageDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaDamageDirectionTest::RunTest(const FString&)
{
	const FVector View(100.0f, 200.0f, 50.0f);

	//+X を向いているとき(Yaw 0)。UE は +Y が右
	TestEqual(TEXT("In front"), AsavaCharacter::CalculateDirectionAngle(View, 0.0f, View + FVector(500.0f, 0.0f, 0.0f)), 0.0f, 0.01f);
	TestEqual(TEXT("To the right"), AsavaCharacter::CalculateDirectionAngle(View, 0.0f, View + FVector(0.0f, 500.0f, 0.0f)), 90.0f, 0.01f);
	TestEqual(TEXT("To the left"), AsavaCharacter::CalculateDirectionAngle(View, 0.0f, View + FVector(0.0f, -500.0f, 0.0f)), -90.0f, 0.01f);
	TestEqual(TEXT("Behind"), FMath::Abs(AsavaCharacter::CalculateDirectionAngle(View, 0.0f, View + FVector(-500.0f, 0.0f, 0.0f))), 180.0f, 0.01f);

	//振り向くと向きも変わる: 右を向く(Yaw 90)と、+X にいた相手は左
	TestEqual(TEXT("Turning right puts the source on the left"), AsavaCharacter::CalculateDirectionAngle(View, 90.0f, View + FVector(500.0f, 0.0f, 0.0f)), -90.0f, 0.01f);

	//上下は見ない
	TestEqual(TEXT("Height is ignored"), AsavaCharacter::CalculateDirectionAngle(View, 0.0f, View + FVector(500.0f, 0.0f, 3000.0f)), 0.0f, 0.01f);
	TestEqual(TEXT("Directly above: front"), AsavaCharacter::CalculateDirectionAngle(View, 0.0f, View + FVector(0.0f, 0.0f, 300.0f)), 0.0f);
	return true;
}

#endif

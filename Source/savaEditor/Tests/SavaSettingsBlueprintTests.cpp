#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaSettingsBlueprintTest, "Sava.Settings.BlueprintPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaSettingsBlueprintTest::RunTest(const FString&)
{
	auto Blueprint = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/WBP/WBP_SettingsMenu.WBP_SettingsMenu"));
	if (!TestNotNull(TEXT("Editable Widget Blueprint exists"), Blueprint)) return false;
	if (!TestNotNull(TEXT("Designer hierarchy exists"), Blueprint->WidgetTree.Get())) return false;
	if (!TestNotNull(TEXT("Generated Blueprint class exists"), Blueprint->GeneratedClass.Get())) return false;
	for (const FName Function : {FName(TEXT("UpdateTabVisuals")), FName(TEXT("UpdateStateVisuals"))})
	{
		auto Implementation = Blueprint->GeneratedClass->FindFunctionByName(Function);
		if (TestNotNull(*Function.ToString(), Implementation))
			TestTrue(TEXT("UI event has real Blueprint bytecode"), Implementation->Script.Num() > 0);
	}
	auto Tint = Cast<UBorder>(Blueprint->WidgetTree->FindWidget(TEXT("WorldTint")));
	if (TestNotNull(TEXT("World background is independently editable"), Tint))
		TestTrue(TEXT("World is visible through backdrop"), Tint->GetBrushColor().A >= 0.4f && Tint->GetBrushColor().A <= 0.75f);
	Blueprint->WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		if (auto Text = Cast<UTextBlock>(Widget))
		{
			TestNotNull(TEXT("Text uses a serializable font asset"), Text->GetFont().FontObject.Get());
			TestTrue(TEXT("Text remains opaque"), FMath::IsNearlyEqual(Text->GetRenderOpacity(), 1.0f));
		}
	});
	TestTrue(TEXT("Intro animation is preserved"), Blueprint->Animations.Num() > 0);
	return true;
}
#endif

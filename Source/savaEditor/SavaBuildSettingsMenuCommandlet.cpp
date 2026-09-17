#include "SavaBuildSettingsMenuCommandlet.h"
#include "SavaUpgradeSettingsMenuCommandlet.h"
#include "SavaSettingsWidget.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Styling/CoreStyle.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Engine/Font.h"
#include "Misc/Parse.h"

namespace SettingsDesign
{
	const FLinearColor Ink(0.86f, 0.94f, 0.93f);
	const FLinearColor Muted(0.35f, 0.51f, 0.54f);
	const FLinearColor Mint(0.32f, 0.92f, 0.76f);
	const FLinearColor Coral(0.9f, 0.075f, 0.12f);
	const FLinearColor Base(0.007f, 0.017f, 0.027f, 0.96f);
	const FLinearColor Panel(0.022f, 0.044f, 0.058f, 0.96f);
	const FLinearColor Row(0.032f, 0.063f, 0.080f, 0.92f);
	const FLinearColor Line(0.085f, 0.17f, 0.19f);

	FSlateFontInfo Font(float Size, FName Typeface = TEXT("Regular"))
	{
		// CoreStyle's transient composite font cannot be serialized into a Widget Blueprint.
		return FSlateFontInfo(LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")), Size, Typeface);
	}

	template<class T> T* Make(UWidgetTree* Tree, FName Name = NAME_None)
	{
		T* Widget = Tree->ConstructWidget<T>(T::StaticClass(), Name);
		Widget->bIsVariable = !Name.IsNone();
		return Widget;
	}

	UTextBlock* Text(UWidgetTree* Tree, const TCHAR* Value, int32 Size = 16, FLinearColor Color = Ink, FName Name = NAME_None, bool Bold = false)
	{
		UTextBlock* Result = Make<UTextBlock>(Tree, Name);
		Result->SetText(FText::FromString(Value));
		Result->SetFont(Font(Size, Bold ? "Bold" : "Regular"));
		Result->SetColorAndOpacity(Color);
		return Result;
	}

	UBorder* Border(UWidgetTree* Tree, FLinearColor Color, FMargin Padding = FMargin(0), FName Name = NAME_None)
	{
		UBorder* Result = Make<UBorder>(Tree, Name);
		Result->SetBrush(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		Result->SetBrushColor(Color);
		Result->SetPadding(Padding);
		return Result;
	}

	USizeBox* Size(UWidgetTree* Tree, UWidget* Child, float Width = 0, float Height = 0, FName Name = NAME_None)
	{
		USizeBox* Result = Make<USizeBox>(Tree, Name);
		if (Width > 0) Result->SetWidthOverride(Width);
		if (Height > 0) Result->SetHeightOverride(Height);
		if (Child) Result->AddChild(Child);
		return Result;
	}

	void V(UVerticalBox* Parent, UWidget* Child, FMargin Padding = FMargin(0), bool Fill = false)
	{
		auto Slot = Parent->AddChildToVerticalBox(Child);
		Slot->SetPadding(Padding);
		Slot->SetHorizontalAlignment(HAlign_Fill);
		if (Fill) Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	void H(UHorizontalBox* Parent, UWidget* Child, FMargin Padding = FMargin(0), bool Fill = false)
	{
		auto Slot = Parent->AddChildToHorizontalBox(Child);
		Slot->SetPadding(Padding);
		Slot->SetVerticalAlignment(VAlign_Center);
		if (Fill) Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	void O(UOverlay* Parent, UWidget* Child)
	{
		auto Slot = Parent->AddChildToOverlay(Child);
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetVerticalAlignment(VAlign_Fill);
	}

	FSlateBrush Surface(FLinearColor Color, FLinearColor Outline = Line)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = Color;
		Brush.OutlineSettings.Color = Outline;
		Brush.OutlineSettings.Width = 1;
		Brush.OutlineSettings.CornerRadii = FVector4(2);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return Brush;
	}

	FButtonStyle ButtonStyle(bool Accent = false)
	{
		FButtonStyle Style;
		Style.SetNormal(Surface(Accent ? Coral : Panel, Accent ? Coral : Line));
		Style.SetHovered(Surface(Accent ? FLinearColor(1.0f, 0.16f, 0.20f) : Row, Accent ? Ink : Mint));
		Style.SetPressed(Surface(Accent ? FLinearColor(0.65f, 0.03f, 0.065f) : Line, Mint));
		Style.SetDisabled(Surface(Panel, Panel));
		Style.SetNormalPadding(FMargin(20, 10)).SetPressedPadding(FMargin(20, 10));
		return Style;
	}

	UButton* Button(UWidgetTree* Tree, FName Name, const TCHAR* Label, bool Accent = false)
	{
		auto Result = Make<UButton>(Tree, Name);
		Result->SetStyle(ButtonStyle(Accent));
		Result->AddChild(Text(Tree, Label, 16, Ink, NAME_None, true));
		return Result;
	}

	UWidget* Rule(UWidgetTree* Tree, FLinearColor Color = Line, FName Name = NAME_None, float Height = 1)
	{
		return Size(Tree, Border(Tree, Color, FMargin(0), Name), 0, Height);
	}

	UVerticalBox* Section(UWidgetTree* Tree, const TCHAR* Title, const TCHAR* Subtitle)
	{
		auto Result = Make<UVerticalBox>(Tree);
		V(Result, Text(Tree, Subtitle, 11, Mint, NAME_None, true), FMargin(0, 0, 0, 8));
		V(Result, Text(Tree, Title, 26, Ink, NAME_None, true), FMargin(0, 0, 0, 22));
		return Result;
	}

	UWidget* SettingRow(UWidgetTree* Tree, const TCHAR* Label, const TCHAR* Description, UWidget* Control)
	{
		auto Back = Border(Tree, Row, FMargin(24, 12));
		auto Columns = Make<UHorizontalBox>(Tree);
		Back->AddChild(Columns);
		auto Labels = Make<UVerticalBox>(Tree);
		V(Labels, Text(Tree, Label, 17, Ink, NAME_None, true));
		V(Labels, Text(Tree, Description, 11, Muted), FMargin(0, 5, 0, 0));
		H(Columns, Labels, FMargin(0, 0, 20, 0), true);
		H(Columns, Control, FMargin(0), true);
		return Back;
	}

	USpinBox* Number(UWidgetTree* Tree, FName Name, float Min, float Max, float Value, int32 Digits)
	{
		auto Result = Make<USpinBox>(Tree, Name);
		Result->SetMinValue(Min); Result->SetMaxValue(Max);
		Result->SetMinSliderValue(Min); Result->SetMaxSliderValue(Max);
		Result->SetDelta(Digits > 0 ? 0.01f : 1.0f);
		Result->SetMinFractionalDigits(Digits); Result->SetMaxFractionalDigits(Digits);
		Result->SetValue(Value);
		Result->SetFont(Font(18, "Bold"));
		Result->SetForegroundColor(Ink);
		FSpinBoxStyle Style = Result->GetWidgetStyle();
		Style.SetBackgroundBrush(Surface(Base));
		Style.SetHoveredBackgroundBrush(Surface(Panel, Mint));
		Style.SetActiveFillBrush(Surface(Panel, Mint));
		Style.SetInactiveFillBrush(Surface(Base));
		Result->SetWidgetStyle(Style);
		return Result;
	}

	UComboBoxString* Combo(UWidgetTree* Tree, FName Name)
	{
		auto Result = Make<UComboBoxString>(Tree, Name);
		Result->SetContentPadding(FMargin(14, 8));
		Result->SetMaxListHeight(300);
		// These are construction-only properties; author them on the Blueprint template.
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Result->Font = Font(16);
		Result->ForegroundColor = Ink;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		FComboBoxStyle Style = Result->GetWidgetStyle();
		Style.ComboButtonStyle.ButtonStyle = ButtonStyle();
		Style.ComboButtonStyle.MenuBorderBrush = Surface(Base, Mint);
		Result->SetWidgetStyle(Style);
		FTableRowStyle Item = Result->GetItemStyle();
		Item.SetEvenRowBackgroundBrush(Surface(Base, Base)).SetOddRowBackgroundBrush(Surface(Base, Base));
		Item.SetEvenRowBackgroundHoveredBrush(Surface(Row, Mint)).SetOddRowBackgroundHoveredBrush(Surface(Row, Mint));
		Item.SetActiveBrush(Surface(Row, Mint)).SetInactiveBrush(Surface(Panel));
		Item.SetTextColor(Ink).SetSelectedTextColor(Mint);
		Result->SetItemStyle(Item);
		return Result;
	}

	UWidget* Toggle(UWidgetTree* Tree, const TCHAR* OffName, const TCHAR* OnName)
	{
		auto Result = Make<UHorizontalBox>(Tree);
		H(Result, Button(Tree, FName(OffName), TEXT("オフ")), FMargin(0, 0, 6, 0), true);
		H(Result, Button(Tree, FName(OnName), TEXT("オン")), FMargin(0), true);
		return Result;
	}

	void AddIntro(UWidgetBlueprint* Blueprint)
	{
		auto Animation = NewObject<UWidgetAnimation>(Blueprint, TEXT("Intro"), RF_Transactional);
		Animation->SetDisplayLabel(TEXT("Intro"));
		Animation->MovieScene = NewObject<UMovieScene>(Animation, TEXT("MovieScene"), RF_Transactional);
		Animation->MovieScene->SetTickResolutionDirectly(FFrameRate(24000, 1));
		Animation->MovieScene->SetDisplayRate(FFrameRate(30, 1));
		Animation->MovieScene->SetPlaybackRange(0, 6001);
		const FGuid Guid = Animation->MovieScene->AddPossessable(TEXT("AnimatedMenu"), USizeBox::StaticClass());
		FWidgetAnimationBinding Binding;
		Binding.AnimationGuid = Guid; Binding.WidgetName = TEXT("AnimatedMenu"); Binding.bIsRootWidget = false;
		Animation->AnimationBindings.Add(Binding);
		auto Track = Animation->MovieScene->AddTrack<UMovieSceneFloatTrack>(Guid);
		Track->SetPropertyNameAndPath(TEXT("RenderOpacity"), TEXT("RenderOpacity"));
		auto Section = CastChecked<UMovieSceneFloatSection>(Track->CreateNewSection());
		Section->SetRange(TRange<FFrameNumber>(0, 6001));
		Section->GetChannel().AddCubicKey(0, 0.0f);
		Section->GetChannel().AddCubicKey(6000, 1.0f);
		Track->AddSection(*Section);
		Blueprint->Animations.Add(Animation);
	}
}

USavaBuildSettingsMenuCommandlet::USavaBuildSettingsMenuCommandlet()
{
	IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 USavaBuildSettingsMenuCommandlet::Main(const FString& Params)
{
	using namespace SettingsDesign;
	const FString PackageName = TEXT("/Game/WBP/WBP_SettingsMenu");
	if (FPackageName::DoesPackageExist(PackageName))
	{
		if (FParse::Param(*Params, TEXT("RepairFonts")))
		{
			// Scoped migration: preserve the Designer hierarchy, layout, styles and animations.
			auto Existing = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/WBP/WBP_SettingsMenu.WBP_SettingsMenu"));
			if (!Existing || !Existing->WidgetTree) return 4;
			Existing->WidgetTree->ForEachWidget([](UWidget* Widget)
			{
				if (auto Label = Cast<UTextBlock>(Widget))
				{
					const auto Previous = Label->GetFont();
					Label->SetFont(Font(Previous.Size, Previous.TypefaceFontName));
				}
				else if (auto Spin = Cast<USpinBox>(Widget))
				{
					const auto Previous = Spin->GetFont();
					Spin->SetFont(Font(Previous.Size, Previous.TypefaceFontName));
				}
				else if (auto Dropdown = Cast<UComboBoxString>(Widget))
				{
					PRAGMA_DISABLE_DEPRECATION_WARNINGS
					Dropdown->Font = Font(Dropdown->Font.Size, Dropdown->Font.TypefaceFontName);
					PRAGMA_ENABLE_DEPRECATION_WARNINGS
				}
			});
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Existing);
			FCompilerResultsLog Result;
			FKismetEditorUtilities::CompileBlueprint(Existing, EBlueprintCompileOptions::None, &Result);
			if (Result.NumErrors || Result.NumWarnings || Existing->Status == BS_Error) return 5;
			FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
			const FString File = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
			if (!UPackage::SavePackage(Existing->GetOutermost(), Existing, *File, Args)) return 6;
			UE_LOG(LogTemp, Display, TEXT("Repaired serialized fonts; Widget Blueprint compiled with zero errors or warnings."));
			return 0;
		}
		UE_LOG(LogTemp, Error, TEXT("WBP_SettingsMenu already exists. Edit it in the UMG Designer; this authoring tool will not overwrite it."));
		return 1;
	}
	UPackage* Package = CreatePackage(*PackageName);
	auto Blueprint = CastChecked<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(USavaSettingsWidget::StaticClass(),
		Package, TEXT("WBP_SettingsMenu"), BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
	UWidgetTree* Tree = Blueprint->WidgetTree;
	if (!Tree) Tree = Blueprint->WidgetTree = NewObject<UWidgetTree>(Blueprint, TEXT("WidgetTree"), RF_Transactional);
	auto Root = Make<UCanvasPanel>(Tree, TEXT("MenuCanvas"));
	Tree->RootWidget = Root;
	auto Screen = Make<UOverlay>(Tree, TEXT("ScreenLayers"));
	auto ScreenSlot = Root->AddChildToCanvas(Screen);
	ScreenSlot->SetAnchors(FAnchors(0, 0, 1, 1)); ScreenSlot->SetOffsets(FMargin(0));
	auto Blur = Make<UBackgroundBlur>(Tree, TEXT("WorldBlur"));
	Blur->SetBlurStrength(12.0f);
	O(Screen, Blur); O(Screen, Border(Tree, Base));
	auto Scale = Make<UScaleBox>(Tree, TEXT("ResponsiveScale"));
	Scale->SetStretch(EStretch::ScaleToFit); O(Screen, Scale);
	auto Animated = Size(Tree, nullptr, 1440, 810, TEXT("AnimatedMenu"));
	Scale->AddChild(Animated);
	auto Layers = Make<UOverlay>(Tree); Animated->AddChild(Layers);
	auto Frame = Border(Tree, FLinearColor::Transparent, FMargin(54, 32)); O(Layers, Frame);
	auto Body = Make<UVerticalBox>(Tree, TEXT("SettingsBody")); Frame->AddChild(Body);

	// Header and category tabs.
	auto Header = Make<UHorizontalBox>(Tree);
	auto Heading = Make<UVerticalBox>(Tree);
	V(Heading, Text(Tree, TEXT("S A V A   /   PLAYER PREFERENCES"), 11, Mint, NAME_None, true));
	V(Heading, Text(Tree, TEXT("設定"), 42, Ink, NAME_None, true), FMargin(0, 6, 0, 0));
	H(Header, Heading, FMargin(0), true);
	H(Header, Text(Tree, TEXT("FOCUS.  ADJUST.  PLAY."), 12, Muted));
	V(Body, Header, FMargin(0, 0, 0, 22));
	auto Tabs = Make<UHorizontalBox>(Tree);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		auto Tab = Make<UVerticalBox>(Tree);
		V(Tab, Button(Tree, Index == 0 ? TEXT("GeneralTab") : TEXT("VideoTab"), Index == 0 ? TEXT("一般    GENERAL") : TEXT("映像    VIDEO")));
		V(Tab, Rule(Tree, Mint, Index == 0 ? TEXT("GeneralUnderline") : TEXT("VideoUnderline"), 3), FMargin(0, 5, 0, 0));
		H(Tabs, Size(Tree, Tab, 220), FMargin(0, 0, 6, 0));
	}
	V(Body, Tabs, FMargin(0, 0, 0, 26));

	// Two editable UMG pages, plus a contextual help card.
	auto Columns = Make<UHorizontalBox>(Tree);
	auto Pages = Make<UWidgetSwitcher>(Tree, TEXT("Pages")); H(Columns, Pages, FMargin(0), true);
	auto General = Section(Tree, TEXT("マウス"), TEXT("01   /   AIM & CONTROL")); Pages->AddChild(General);
	auto SensitivityControl = Make<UHorizontalBox>(Tree);
	auto Slider = Make<USlider>(Tree, TEXT("SensitivitySlider"));
	Slider->SetMinValue(0.1f); Slider->SetMaxValue(3.0f); Slider->SetValue(1.0f); Slider->SetStepSize(0.01f);
	Slider->SetSliderBarColor(Muted); Slider->SetSliderHandleColor(Mint);
	FSliderStyle SliderStyle;
	FSlateBrush Bar = *FCoreStyle::Get().GetBrush("WhiteBrush");
	SliderStyle.SetNormalBarImage(Bar).SetHoveredBarImage(Bar).SetDisabledBarImage(Bar).SetBarThickness(3);
	Bar.ImageSize = FVector2D(5, 20);
	SliderStyle.SetNormalThumbImage(Bar).SetHoveredThumbImage(Bar).SetDisabledThumbImage(Bar);
	Slider->SetWidgetStyle(SliderStyle);
	H(SensitivityControl, Slider, FMargin(0, 0, 20, 0), true);
	H(SensitivityControl, Size(Tree, Number(Tree, TEXT("SensitivityInput"), 0.1f, 3, 1, 3), 115, 38));
	V(General, SettingRow(Tree, TEXT("照準感度"), TEXT("マウス移動に対する視点の速さ"), SensitivityControl), FMargin(0, 0, 0, 4));
	V(General, SettingRow(Tree, TEXT("垂直視点を反転"), TEXT("上下のマウス操作を反転"), Toggle(Tree, TEXT("InvertOff"), TEXT("InvertOn"))));
	V(General, Rule(Tree), FMargin(0, 30, 0, 22));
	V(General, Text(Tree, TEXT("PRECISION STARTS HERE"), 11, Mint, NAME_None, true));
	V(General, Text(Tree, TEXT("スライダーで調整、数値入力で微調整。\n初期値は 1.000。数値が小さいほど視点がゆっくり動きます。"), 15, Muted), FMargin(0, 12, 0, 0));

	auto Video = Section(Tree, TEXT("ディスプレイ"), TEXT("02   /   DISPLAY & PERFORMANCE")); Pages->AddChild(Video);
	auto Mode = Combo(Tree, TEXT("DisplayModeInput"));
	Mode->AddOption(TEXT("ウィンドウ")); Mode->AddOption(TEXT("ボーダーレス")); Mode->AddOption(TEXT("フルスクリーン")); Mode->SetSelectedIndex(0);
	V(Video, SettingRow(Tree, TEXT("画面モード"), TEXT("ゲームの表示方法"), Mode), FMargin(0, 0, 0, 4));
	auto Resolution = Combo(Tree, TEXT("ResolutionInput")); Resolution->AddOption(TEXT("1920 × 1080")); Resolution->SetSelectedIndex(0);
	V(Video, SettingRow(Tree, TEXT("解像度 / ウィンドウサイズ"), TEXT("ボーダーレスはデスクトップと同じサイズ"), Resolution), FMargin(0, 0, 0, 4));
	V(Video, SettingRow(Tree, TEXT("最大フレームレート"), TEXT("0 = 無制限  /  30–500 FPS"), Size(Tree, Number(Tree, TEXT("FrameLimitInput"), 0, 500, 144, 0), 0, 38)), FMargin(0, 0, 0, 4));
	V(Video, SettingRow(Tree, TEXT("垂直同期 / VSync"), TEXT("画面のティアリングを抑える"), Toggle(Tree, TEXT("VSyncOff"), TEXT("VSyncOn"))));
	V(Video, Text(Tree, TEXT("画面変更後、15秒以内に「維持する」を選択してください。"), 11, Muted, TEXT("DisplayHint")), FMargin(0, 14, 0, 0));

	auto Help = Border(Tree, Panel, FMargin(24));
	auto HelpBody = Make<UVerticalBox>(Tree); Help->AddChild(HelpBody);
	V(HelpBody, Text(Tree, TEXT("FIELD NOTES"), 11, Mint, NAME_None, true));
	V(HelpBody, Text(Tree, TEXT("自分のエイムに。"), 20, Ink, TEXT("HelpTitle"), true), FMargin(0, 22, 0, 16));
	V(HelpBody, Rule(Tree, Mint));
	auto HelpText = Text(Tree, TEXT("小さい値で精密な調整。\n大きい値で素早い振り向き。\n\nこのゲーム独自の感度倍率です。他のゲームの数値とは一致しません。\n\n「適用」を押すまでは、元の設定でプレイできます。"), 13, Muted, TEXT("HelpText"));
	HelpText->SetAutoWrapText(true); V(HelpBody, HelpText, FMargin(0, 22, 0, 12));
	V(HelpBody, Size(Tree, nullptr), FMargin(0), true);
	V(HelpBody, Text(Tree, TEXT("調整 → 適用 → プレイ"), 11, Mint));
	auto HelpSlot = Columns->AddChildToHorizontalBox(Size(Tree, Help, 260)); HelpSlot->SetPadding(FMargin(30, 0, 0, 0)); HelpSlot->SetVerticalAlignment(VAlign_Fill);
	V(Body, Columns, FMargin(0), true);

	// Fixed footer remains visible on both pages.
	V(Body, Rule(Tree), FMargin(0, 22, 0, 16));
	auto Footer = Make<UHorizontalBox>(Tree);
	H(Footer, Button(Tree, TEXT("ResetButton"), TEXT("初期値に戻す")));
	H(Footer, Text(Tree, TEXT("すべての変更は適用済みです"), 12, Muted, TEXT("StatusText")), FMargin(22, 0), true);
	H(Footer, Size(Tree, Button(Tree, TEXT("CloseButton"), TEXT("閉じる  [ESC]")), 165, 48), FMargin(0, 0, 10, 0));
	H(Footer, Size(Tree, Button(Tree, TEXT("ApplyButton"), TEXT("設定を適用"), true), 180, 48));
	V(Body, Footer);

	// Real UMG overlay for video preview confirmation.
	auto Modal = Make<UOverlay>(Tree, TEXT("ConfirmOverlay")); O(Layers, Modal);
	O(Modal, Border(Tree, FLinearColor(0, 0, 0, 0.85f)));
	auto Card = Border(Tree, Panel, FMargin(38));
	auto ModalBody = Make<UVerticalBox>(Tree); Card->AddChild(ModalBody);
	V(ModalBody, Text(Tree, TEXT("DISPLAY CONFIRMATION"), 11, Mint, NAME_None, true));
	V(ModalBody, Text(Tree, TEXT("この画面設定を維持しますか？"), 23, Ink, NAME_None, true), FMargin(0, 18));
	V(ModalBody, Text(Tree, TEXT("15 秒後に変更前の設定へ戻ります。"), 15, Muted, TEXT("CountdownText")), FMargin(0, 0, 0, 28));
	auto ModalActions = Make<UHorizontalBox>(Tree);
	H(ModalActions, Button(Tree, TEXT("RevertButton"), TEXT("元に戻す")), FMargin(0, 0, 12, 0), true);
	H(ModalActions, Button(Tree, TEXT("ConfirmButton"), TEXT("維持する"), true), FMargin(0), true);
	V(ModalBody, ModalActions);
	auto CardSlot = Modal->AddChildToOverlay(Size(Tree, Card, 650)); CardSlot->SetHorizontalAlignment(HAlign_Center); CardSlot->SetVerticalAlignment(VAlign_Center);
	Modal->SetVisibility(ESlateVisibility::Collapsed);
	Pages->SetActiveWidgetIndex(0);
	AddIntro(Blueprint);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FCompilerResultsLog CompileLog;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &CompileLog);
	if (CompileLog.NumErrors > 0 || Blueprint->Status == BS_Error) return 2;
	FAssetRegistryModule::AssetCreated(Blueprint);
	Package->MarkPackageDirty();
	FSavePackageArgs SaveArgs; SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	if (!UPackage::SavePackage(Package, Blueprint, *Filename, SaveArgs)) return 3;
	UE_LOG(LogTemp, Display, TEXT("Created editable Widget Blueprint: %s (including Intro animation)"), *Filename);
	// A fresh asset needs the same Blueprint visual events as migrated assets.
	return NewObject<USavaUpgradeSettingsMenuCommandlet>()->Main(TEXT(""));
}

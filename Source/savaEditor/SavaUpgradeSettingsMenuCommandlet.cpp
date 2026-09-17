#include "SavaUpgradeSettingsMenuCommandlet.h"
#include "SavaSettingsWidget.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Kismet/KismetMathLibrary.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "EdGraphSchema_K2.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"

namespace SettingsUpgrade
{
	// These helpers author ordinary, editable K2 nodes. They never execute in a game.
	struct FGraphWriter
	{
		UEdGraph* Graph;
		UEdGraphPin* Exec;
		int32 X = 400;

		FGraphWriter(UWidgetBlueprint* Blueprint, FName GraphName, FName EventName)
		{
			Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, GraphName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
			FBlueprintEditorUtils::AddUbergraphPage(Blueprint, Graph);
			auto Event = Add<UK2Node_Event>(0, 0);
			Event->EventReference.SetExternalMember(EventName, USavaSettingsWidget::StaticClass());
			Event->bOverrideFunction = true;
			Event->AllocateDefaultPins();
			Exec = Pin(Event, UEdGraphSchema_K2::PN_Then);
		}

		template<class T> T* Add(int32 NodeX, int32 NodeY)
		{
			auto Node = NewObject<T>(Graph);
			Graph->AddNode(Node, false, false);
			Node->CreateNewGuid(); Node->NodePosX = NodeX; Node->NodePosY = NodeY;
			return Node;
		}

		static UEdGraphPin* Pin(UEdGraphNode* Node, FName Name)
		{
			auto Result = Node->FindPin(Name);
			checkf(Result, TEXT("Missing pin %s on %s"), *Name.ToString(), *Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
			return Result;
		}

		void Link(UEdGraphPin* A, UEdGraphPin* B)
		{
			checkf(A && B && Graph->GetSchema()->TryCreateConnection(A, B), TEXT("Cannot connect Blueprint pins"));
		}

		UEdGraphPin* Parameter(FName Name) { return Pin(Graph->Nodes[0], Name); }

		UEdGraphPin* Get(FName Member, int32 NodeX, int32 NodeY)
		{
			auto Node = Add<UK2Node_VariableGet>(NodeX, NodeY);
			// This is a member of the current widget (including inherited BindWidget fields).
			Node->VariableReference.SetSelfMember(Member);
			Node->AllocateDefaultPins();
			return Node->GetValuePin();
		}

		UK2Node_CallFunction* Call(UClass* Owner, FName Function, int32 NodeX, int32 NodeY)
		{
			auto Node = Add<UK2Node_CallFunction>(NodeX, NodeY);
			Node->SetFromFunction(Owner->FindFunctionByName(Function)); Node->AllocateDefaultPins();
			return Node;
		}

		UK2Node_CallFunction* Set(FName Widget, UClass* Owner, FName Function)
		{
			auto Node = Call(Owner, Function, X, 0);
			Link(Get(Widget, X, -100), Pin(Node, UEdGraphSchema_K2::PN_Self));
			Link(Exec, Pin(Node, UEdGraphSchema_K2::PN_Execute));
			Exec = Pin(Node, UEdGraphSchema_K2::PN_Then); X += 440;
			return Node;
		}

		UEdGraphPin* Select(FName Function, UEdGraphPin* Condition, FName A, FName B, int32 NodeX)
		{
			auto Node = Call(UKismetMathLibrary::StaticClass(), Function, NodeX, 240);
			Link(Get(A, NodeX - 200, 260), Pin(Node, TEXT("A")));
			Link(Get(B, NodeX - 200, 330), Pin(Node, TEXT("B")));
			Link(Condition, Pin(Node, TEXT("bPickA")));
			return Node->GetReturnValuePin();
		}

		void Tint(FName Widget, UEdGraphPin* Condition, bool bReverse = false)
		{
			auto Node = Set(Widget, UButton::StaticClass(), TEXT("SetColorAndOpacity"));
			Link(Select(TEXT("SelectColor"), Condition, bReverse ? TEXT("IdleTint") : TEXT("SelectedTint"),
				bReverse ? TEXT("SelectedTint") : TEXT("IdleTint"), Node->NodePosX), Pin(Node, TEXT("InColorAndOpacity")));
		}

		void Opacity(FName Widget, UEdGraphPin* Condition, bool bReverse, double Low = 0)
		{
			auto Node = Set(Widget, UWidget::StaticClass(), TEXT("SetRenderOpacity"));
			auto Select = Call(UKismetMathLibrary::StaticClass(), TEXT("SelectFloat"), Node->NodePosX, 220);
			Pin(Select, TEXT("A"))->DefaultValue = FString::SanitizeFloat(bReverse ? Low : 1.0);
			Pin(Select, TEXT("B"))->DefaultValue = FString::SanitizeFloat(bReverse ? 1.0 : Low);
			Link(Condition, Pin(Select, TEXT("bPickA")));
			Link(Select->GetReturnValuePin(), Pin(Node, TEXT("InOpacity")));
		}
	};

	void BuildGraphs(UWidgetBlueprint* Blueprint)
	{
		FGraphWriter Tabs(Blueprint, TEXT("Settings_TabVisuals"), TEXT("UpdateTabVisuals"));
		auto Page = Tabs.Set(TEXT("Pages"), UWidgetSwitcher::StaticClass(), TEXT("SetActiveWidgetIndex"));
		Tabs.Link(Tabs.Parameter(TEXT("TabIndex")), Tabs.Pin(Page, TEXT("Index")));
		auto Compare = Tabs.Call(UKismetMathLibrary::StaticClass(), TEXT("EqualEqual_IntInt"), 0, 240);
		Tabs.Link(Tabs.Parameter(TEXT("TabIndex")), Tabs.Pin(Compare, TEXT("A")));
		Tabs.Pin(Compare, TEXT("B"))->DefaultValue = TEXT("0");
		auto General = Compare->GetReturnValuePin();
		Tabs.Opacity(TEXT("GeneralUnderline"), General, false);
		Tabs.Opacity(TEXT("VideoUnderline"), General, true);
		Tabs.Tint(TEXT("GeneralTab"), General);
		Tabs.Tint(TEXT("VideoTab"), General, true);
		for (bool bTitle : {true, false})
		{
			auto Node = Tabs.Set(bTitle ? TEXT("HelpTitle") : TEXT("HelpText"), UTextBlock::StaticClass(), TEXT("SetText"));
			Tabs.Link(Tabs.Select(TEXT("SelectText"), General, bTitle ? TEXT("GeneralHelpTitle") : TEXT("GeneralHelpBody"),
				bTitle ? TEXT("VideoHelpTitle") : TEXT("VideoHelpBody"), Node->NodePosX), Tabs.Pin(Node, TEXT("InText")));
		}
		Tabs.Graph->Nodes[0]->NodeComment = TEXT("UI only: switch page, selected tab and help copy. Text / colors are editable in Class Defaults.");
		Tabs.Graph->Nodes[0]->bCommentBubbleVisible = true;

		FGraphWriter State(Blueprint, TEXT("Settings_StateVisuals"), TEXT("UpdateStateVisuals"));
		State.Tint(TEXT("InvertOff"), State.Parameter(TEXT("bInvertY")), true);
		State.Tint(TEXT("InvertOn"), State.Parameter(TEXT("bInvertY")));
		State.Tint(TEXT("VSyncOff"), State.Parameter(TEXT("bVSync")), true);
		State.Tint(TEXT("VSyncOn"), State.Parameter(TEXT("bVSync")));
		State.Opacity(TEXT("StatusText"), State.Parameter(TEXT("bDirty")), false, 0.8);
		State.Graph->Nodes[0]->NodeComment = TEXT("UI only: shared selection tints. Pending, validation and saving stay in the Controller.");
		State.Graph->Nodes[0]->bCommentBubbleVisible = true;
	}

	void ImproveDesigner(UWidgetTree* Tree)
	{
		// Leave the hierarchy, existing bindings, animations and user-authored event graphs intact.
		Tree->ForEachWidget([](UWidget* Widget)
		{
			if (auto Text = Cast<UTextBlock>(Widget))
			{
				auto Font = Text->GetFont();
				if (Font.Size <= 13) Font.Size = 14;
				else if (Font.Size == 17) Font.Size = 18;
				Text->SetFont(Font);
				const auto Color = Text->GetColorAndOpacity().GetSpecifiedColor();
				if (Color.R < 0.5f && Color.G < 0.65f) Text->SetColorAndOpacity(FLinearColor(0.65f, 0.77f, 0.81f));
				Text->SetShadowOffset(FVector2D(0, 1));
				Text->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.4f));
			}
			else if (auto Border = Cast<UBorder>(Widget))
			{
				FLinearColor Color = Border->GetBrushColor();
				if (Color.A > 0.5f && Color.R > 0.025f && Color.R < 0.04f && Color.G < 0.09f)
				{
					Border->SetBrushColor(FLinearColor(0.015f, 0.029f, 0.044f, 0.94f));
					Border->SetPadding(FMargin(24, 14));
				}
			}
		});
		auto Screen = CastChecked<UOverlay>(Tree->FindWidget(TEXT("ScreenLayers")));
		for (UWidget* Child : Screen->GetAllChildren())
		{
			if (auto Tint = Cast<UBorder>(Child))
			{
				Tint->Rename(TEXT("WorldTint"), Tree); Tint->bIsVariable = true;
				Tint->SetBrushColor(FLinearColor(0.008f, 0.015f, 0.024f, 0.65f));
			}
		}
		CastChecked<UBackgroundBlur>(Tree->FindWidget(TEXT("WorldBlur")))->SetBlurStrength(2.5f);
		auto Pages = CastChecked<UWidgetSwitcher>(Tree->FindWidget(TEXT("Pages")));
		CastChecked<UHorizontalBoxSlot>(Pages->Slot)->SetVerticalAlignment(VAlign_Fill);
		auto HelpText = CastChecked<UTextBlock>(Tree->FindWidget(TEXT("HelpText")));
		auto Font = HelpText->GetFont(); Font.Size = 16; HelpText->SetFont(Font);
		HelpText->SetText(NSLOCTEXT("SavaSettings", "AimHelp", "小さい値：精密な照準\n大きい値：素早い振り向き\n\n数値をクリックすると、直接入力できます。\n\n変更は「設定を適用」で保存されます。"));
		CastChecked<UTextBlock>(Tree->FindWidget(TEXT("HelpTitle")))->SetText(NSLOCTEXT("SavaSettings", "AimTitle", "感度の調整"));
		auto HelpPanel = CastChecked<UBorder>(HelpText->GetParent()->GetParent());
		HelpPanel->SetBrushColor(FLinearColor(0.015f, 0.028f, 0.042f, 0.95f));
		CastChecked<USizeBox>(HelpPanel->GetParent())->SetWidthOverride(320);
		// Editor-only visibility: keep the regular page visible when opening the Designer.
		Tree->FindWidget(TEXT("ConfirmOverlay"))->bHiddenInDesigner = true;
	}
}

USavaUpgradeSettingsMenuCommandlet::USavaUpgradeSettingsMenuCommandlet()
{
	IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 USavaUpgradeSettingsMenuCommandlet::Main(const FString& Params)
{
	auto Blueprint = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/WBP/WBP_SettingsMenu.WBP_SettingsMenu"));
	if (!Blueprint || !Blueprint->WidgetTree) return 1;
	if (!FParse::Param(*Params, TEXT("ValidateOnly")))
	{
		for (auto Graph : Blueprint->UbergraphPages)
		{
			if (Graph->GetFName() == TEXT("Settings_TabVisuals") || Graph->GetFName() == TEXT("Settings_StateVisuals"))
			{
				UE_LOG(LogTemp, Error, TEXT("Migration already applied. Edit the existing Blueprint graphs instead of overwriting them."));
				return 2;
			}
		}
		SettingsUpgrade::BuildGraphs(Blueprint);
		SettingsUpgrade::ImproveDesigner(Blueprint->WidgetTree);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
	if (Results.NumErrors || Results.NumWarnings || Blueprint->Status == BS_Error) return 3;
	if (!FParse::Param(*Params, TEXT("ValidateOnly")))
	{
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
		const FString Filename = FPackageName::LongPackageNameToFilename(TEXT("/Game/WBP/WBP_SettingsMenu"), FPackageName::GetAssetPackageExtension());
		if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Filename, Args)) return 4;
	}
	UE_LOG(LogTemp, Display, TEXT("Settings Widget Blueprint: zero errors/warnings. Editable tab/state graphs and translucent world background are ready."));
	return 0;
}

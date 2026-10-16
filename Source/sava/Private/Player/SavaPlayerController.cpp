// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/SavaPlayerController.h"
#include "Player/SavaPlayerState.h"
#include "Loadout/SavaLoadoutSubsystem.h"
#include "savaCharacter.h"
#include "SavaGameUserSettings.h"
#include "SavaScoreboardWidget.h"
#include "SavaSettingsMenuController.h"
#include "SavaSettingsWidget.h"
#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Misc/PackageName.h"
#include "UObject/ConstructorHelpers.h"
#include "savaGameMode.h"
#include "Lobby/SavaLobbyGameMode.h"
#include "Online/SavaSessionSubsystem.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogSavaPlayerController, Log, All);

ASavaPlayerController::ASavaPlayerController()
{
	// Layout, style and animation are authored in the Widget Blueprint.
	if (FPackageName::DoesPackageExist(TEXT("/Game/WBP/WBP_SettingsMenu")))
	{
		static ConstructorHelpers::FClassFinder<USavaSettingsWidget> SettingsView(TEXT("/Game/WBP/WBP_SettingsMenu"));
		SettingsWidgetClass = SettingsView.Class;
	}
	if (FPackageName::DoesPackageExist(TEXT("/Game/UI/WBP_Scoreboard")))
	{
		static ConstructorHelpers::FClassFinder<USavaScoreboardWidget> ScoreboardView(TEXT("/Game/UI/WBP_Scoreboard"));
		ScoreboardWidgetClass = ScoreboardView.Class;
	}
}

void ASavaPlayerController::BeginPlay()
{
	Super::BeginPlay();

	//自分の PC でだけ: 選んであるロードアウトをサーバーへ送り、以後変わるたびに送る
	//(サーバーにある他の人の PlayerController では何もしない)
	if (IsLocalController())
	{
		if (USavaLoadoutSubsystem* LoadoutSubsystem = GetGameInstance()->GetSubsystem<USavaLoadoutSubsystem>())
		{
			LoadoutSubsystem->OnLoadoutChanged.AddDynamic(this, &ThisClass::HandleLocalLoadoutChanged);
			ServerSetLoadout(LoadoutSubsystem->GetLoadout());
		}
	}
}

void ASavaPlayerController::HandleLocalLoadoutChanged(const FSavaLoadout& Loadout)
{
	ServerSetLoadout(Loadout);
}

void ASavaPlayerController::ServerSetLoadout_Implementation(const FSavaLoadout& Loadout)
{
	//ここはサーバー。受け付けるかはルールを持つ GameMode が決める(試合中は変えられない、など)
	ASavaPlayerState* SavaPlayerState = GetPlayerState<ASavaPlayerState>();
	const AsavaGameMode* GameMode = GetWorld()->GetAuthGameMode<AsavaGameMode>();
	if (!SavaPlayerState || (GameMode && !GameMode->CanChangeLoadout(SavaPlayerState)))
	{
		return;
	}

	SavaPlayerState->SetLoadout(Loadout);
}

//--------------------------------Lobby

bool ASavaPlayerController::IsHostController(const APlayerController* PlayerController)
{
	//listen サーバー(ソロも同じ)では、サーバーにあって、その PC で操作している PlayerController がホスト
	//クライアントの PC では自分の PlayerController も Authority ではないので false になる
	return PlayerController && PlayerController->IsLocalController() && PlayerController->HasAuthority();
}

void ASavaPlayerController::RunLobbyRequest(TFunctionRef<bool(ASavaLobbyGameMode&, FText&)> Request)
{
	//ロビー以外のマップでは何もしない
	ASavaLobbyGameMode* LobbyGameMode = GetWorld()->GetAuthGameMode<ASavaLobbyGameMode>();
	if (!LobbyGameMode)
	{
		return;
	}

	//断られたら、頼んだ人の画面へ理由を返す
	FText Error;
	if (!Request(*LobbyGameMode, Error) && !Error.IsEmpty())
	{
		ClientShowLobbyMessage(Error);
	}
}

void ASavaPlayerController::ServerJoinTeam_Implementation(uint8 TeamId)
{
	RunLobbyRequest([this, TeamId](ASavaLobbyGameMode& GameMode, FText& Error)
	{
		return GameMode.RequestJoinTeam(this, TeamId, Error);
	});
}

void ASavaPlayerController::ServerSetReady_Implementation(bool bReady)
{
	RunLobbyRequest([this, bReady](ASavaLobbyGameMode& GameMode, FText& Error)
	{
		return GameMode.RequestSetReady(this, bReady, Error);
	});
}

void ASavaPlayerController::ServerUpdateRoomSettings_Implementation(const FSavaRoomSettings& NewSettings)
{
	RunLobbyRequest([this, &NewSettings](ASavaLobbyGameMode& GameMode, FText& Error)
	{
		return GameMode.RequestUpdateRoomSettings(this, NewSettings, Error);
	});
}

void ASavaPlayerController::ServerMovePlayerToTeam_Implementation(ASavaPlayerState* Target, uint8 TeamId)
{
	RunLobbyRequest([this, Target, TeamId](ASavaLobbyGameMode& GameMode, FText& Error)
	{
		return GameMode.RequestMovePlayerToTeam(this, Target, TeamId, Error);
	});
}

void ASavaPlayerController::ServerKickPlayer_Implementation(ASavaPlayerState* Target)
{
	RunLobbyRequest([this, Target](ASavaLobbyGameMode& GameMode, FText& Error)
	{
		return GameMode.RequestKickPlayer(this, Target, Error);
	});
}

void ASavaPlayerController::ServerStartMatch_Implementation()
{
	RunLobbyRequest([this](ASavaLobbyGameMode& GameMode, FText& Error)
	{
		return GameMode.RequestStartMatch(this, Error);
	});
}

void ASavaPlayerController::ClientShowLobbyMessage_Implementation(const FText& Message)
{
	UE_LOG(LogSavaPlayerController, Log, TEXT("ロビー: %s"), *Message.ToString());
	OnLobbyMessage.Broadcast(Message);

	//受け取る画面がまだないときでも分かるように、画面の左上にも出す(開発中だけ)
#if !UE_BUILD_SHIPPING
	if (!OnLobbyMessage.IsBound() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Yellow, Message.ToString());
	}
#endif
}

void ASavaPlayerController::ClientWasKicked_Implementation(const FText& KickReason)
{
	Super::ClientWasKicked_Implementation(KickReason);

	if (USavaSessionSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<USavaSessionSubsystem>() : nullptr)
	{
		Sessions->NotifyKicked(KickReason.ToString());
	}
}

void ASavaPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	//キャラクターがいない間(死亡中)でも効くように、PlayerController で受け取る
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ASavaPlayerController::ToggleSettingsMenu);

	// Tabキーでスコアボードを表示 / 離したら非表示
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ASavaPlayerController::ShowScoreboard);
	InputComponent->BindKey(EKeys::Tab, IE_Released, this, &ASavaPlayerController::HideScoreboard);
}

void ASavaPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Restore an unconfirmed video preview even if the level is destroyed.
	if (SettingsMenuController) SettingsMenuController->Discard();
	if (SettingsWidget) SettingsWidget->RemoveFromParent();
	if (ScoreboardWidget) ScoreboardWidget->RemoveFromParent();
	if (USavaLoadoutSubsystem* LoadoutSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<USavaLoadoutSubsystem>() : nullptr)
	{
		LoadoutSubsystem->OnLoadoutChanged.RemoveDynamic(this, &ThisClass::HandleLocalLoadoutChanged);
	}
	Super::EndPlay(EndPlayReason);
}

//--------------------------------Settings

bool ASavaPlayerController::IsSettingsMenuOpen() const
{
	return SettingsWidget && SettingsWidget->IsInViewport();
}

//設定画面の開閉
void ASavaPlayerController::ToggleSettingsMenu()
{
	if (IsSettingsMenuOpen())
	{
		CloseSettingsMenu();
		return;
	}

	USavaGameUserSettings* Settings = USavaGameUserSettings::GetSavaGameUserSettings();
	if (!Settings)
	{
		UE_LOG(LogSavaPlayerController, Error, TEXT("SavaGameUserSettings is not configured."));
		return;
	}

	// Migrate an inherited reference to the old WBP_Setting; allow subclasses of the new view.
	if (!SettingsWidgetClass || SettingsWidgetClass == USavaSettingsWidget::StaticClass()
		|| !SettingsWidgetClass->IsChildOf(USavaSettingsWidget::StaticClass()))
	{
		SettingsWidgetClass = LoadClass<USavaSettingsWidget>(nullptr, TEXT("/Game/WBP/WBP_SettingsMenu.WBP_SettingsMenu_C"));
	}
	if (!SettingsWidgetClass)
	{
		UE_LOG(LogSavaPlayerController, Error, TEXT("WBP_SettingsMenu is missing. The settings view must be a Widget Blueprint."));
		return;
	}
	if (SettingsWidget && !SettingsWidget->IsA<USavaSettingsWidget>()) SettingsWidget = nullptr;

	if (!SettingsWidget)
	{
		SettingsWidget = CreateWidget<UUserWidget>(this, SettingsWidgetClass);
	}

	if (USavaSettingsWidget* View = Cast<USavaSettingsWidget>(SettingsWidget))
	{
		SettingsMenuController = NewObject<USavaSettingsMenuController>(this);
		SettingsMenuController->Initialize(Settings, GetWorld()->WorldType != EWorldType::PIE);
		SettingsMenuController->AttachView(View);
		View->OnCloseRequested.BindUObject(this, &ASavaPlayerController::CloseSettingsMenu);

		//スコアボードを出したまま開いた場合は隠す
		HideScoreboard();

		SettingsWidget->AddToViewport(100);
		bShowMouseCursor = true;

		FInputModeGameAndUI InputMode;                           //ゲーム操作とUI操作の両方を受け付ける入力モード
		InputMode.SetWidgetToFocus(SettingsWidget->TakeWidget());//入力のフォーカスを設定画面に向ける
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);//マウスカーソルをゲーム画面内に閉じ込めない
		InputMode.SetHideCursorDuringCapture(false);//マウス入力をゲームがキャプチャしたときでも、カーソルを隠さない
		SetInputMode(InputMode);//PlayerControllerに反映

		//対戦中なのでゲームは止めない(ホストが止めると全員の試合が止まるため)。キャラの操作だけ止める
		//(開いたまま死んでリスポーンした場合は、新しいキャラクターが自分で止める)
		SetPawnGameplayInputEnabled(false);
	}
}

void ASavaPlayerController::CloseSettingsMenu()
{
	if (SettingsMenuController) SettingsMenuController->Discard();

	if (SettingsWidget)
	{
		SettingsWidget->RemoveFromParent();//現在表示されている親から外す。
	}

	//死亡中(キャラクターがいない)なら、次のキャラクターが操作を受け付ける状態で始まる
	SetPawnGameplayInputEnabled(true);
	bShowMouseCursor = false; //マウスカーソルを非表示に戻す処理
	SetInputMode(FInputModeGameOnly()); //ゲーム操作だけ受け付ける
	FlushPressedKeys(); //メニューを閉じた直後の誤入力を防ぐ
}

void ASavaPlayerController::SetPawnGameplayInputEnabled(bool bEnabled)
{
	if (AsavaCharacter* SavaCharacter = GetPawn<AsavaCharacter>())
	{
		SavaCharacter->SetGameplayInputEnabled(this, bEnabled);
	}
}

//--------------------------------Scoreboard

void ASavaPlayerController::ShowScoreboard()
{
	//設定画面を開いている間は出さない
	if (!ScoreboardWidgetClass || IsSettingsMenuOpen())
	{
		return;
	}

	if (!ScoreboardWidget)
	{
		ScoreboardWidget = CreateWidget<USavaScoreboardWidget>(this, ScoreboardWidgetClass);
	}

	if (!ScoreboardWidget)
	{
		return;
	}

	if (!ScoreboardWidget->IsInViewport())
	{
		ScoreboardWidget->AddToViewport(50);
	}

	ScoreboardWidget->RefreshScoreboard();
}

void ASavaPlayerController::HideScoreboard()
{
	if (!ScoreboardWidget)
	{
		return;
	}

	ScoreboardWidget->RemoveFromParent();
}

void ASavaPlayerController::ClientShowDeathmatchResult_Implementation()
{
	if (!IsLocalController())
	{
		return;
	}

	if (DeathmatchResultWidget &&
		DeathmatchResultWidget->IsInViewport())
	{
		return;
	}

	if (!DeathmatchResultWidgetClass)
	{
		UE_LOG(LogSavaPlayerController, Error,
			TEXT("DeathmatchResultWidgetClass is not set."));
		return;
	}

	DeathmatchResultWidget = CreateWidget<UUserWidget>(
		this,
		DeathmatchResultWidgetClass
	);

	if (!DeathmatchResultWidget)
	{
		return;
	}

	DeathmatchResultWidget->AddToViewport(100);

	bShowMouseCursor = true;
	SetInputMode(FInputModeGameAndUI());
}

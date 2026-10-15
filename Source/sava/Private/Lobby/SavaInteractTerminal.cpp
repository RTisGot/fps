// Fill out your copyright notice in the Description page of Project Settings.

#include "Lobby/SavaInteractTerminal.h"
#include "Lobby/SavaTerminalWidget.h"
#include "GameFramework/PlayerController.h"
#include "savaCharacter.h"

void ASavaInteractTerminal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseMenu();

	Super::EndPlay(EndPlayReason);
}

void ASavaInteractTerminal::OnInteract(APlayerController* PlayerController)
{
	OpenMenu(PlayerController);
}

void ASavaInteractTerminal::OpenMenu(APlayerController* PlayerController)
{
	if (!PlayerController || !MenuWidgetClass || IsMenuOpen())
	{
		return;
	}

	if (!MenuWidget)
	{
		MenuWidget = CreateWidget<USavaTerminalWidget>(PlayerController, MenuWidgetClass);
		if (!MenuWidget)
		{
			return;
		}
		MenuWidget->SetTerminal(this);
	}

	MenuOwner = PlayerController;
	bMenuOpen = true;
	HidePrompt();
	MenuWidget->AddToViewport(50);

	//設定画面(ASavaPlayerController)と同じ: カーソルを出し、入力を画面へ向け、キャラの操作だけ止める
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	PlayerController->SetInputMode(InputMode);
	PlayerController->bShowMouseCursor = true;

	if (AsavaCharacter* Character = PlayerController->GetPawn<AsavaCharacter>())
	{
		Character->SetGameplayInputEnabled(PlayerController, false);
	}
}

void ASavaInteractTerminal::CloseMenu()
{
	if (!IsMenuOpen())
	{
		return;
	}
	bMenuOpen = false;

	//マップ移動のときは、エンジンが先に画面からウィジェットを外しているが、入力モードの戻しは必ず行う
	//(入力モードはマップをまたいで残るので、戻さないと次のマップでマウスが効かなくなる)
	if (MenuWidget)
	{
		MenuWidget->RemoveFromParent();
	}

	APlayerController* PlayerController = MenuOwner.Get();
	MenuOwner.Reset();
	if (!PlayerController)
	{
		return;
	}

	if (AsavaCharacter* Character = PlayerController->GetPawn<AsavaCharacter>())
	{
		Character->SetGameplayInputEnabled(PlayerController, true);
	}
	PlayerController->bShowMouseCursor = false;
	PlayerController->SetInputMode(FInputModeGameOnly());
	PlayerController->FlushPressedKeys(); //閉じた直後の誤入力を防ぐ

	//まだ範囲内にいるなら案内を出し直す
	RefreshPrompt(PlayerController);
}

bool ASavaInteractTerminal::IsMenuOpen() const
{
	//ウィジェットが表示中かでは判断しない(マップ移動の途中では、開いたままでも外されているため)
	return bMenuOpen;
}

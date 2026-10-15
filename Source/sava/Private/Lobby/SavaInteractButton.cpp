// Fill out your copyright notice in the Description page of Project Settings.

#include "Lobby/SavaInteractButton.h"
#include "Online/SavaSessionSubsystem.h"
#include "Player/SavaPlayerController.h"
#include "Player/SavaPlayerState.h"
#include "Engine/GameInstance.h"

bool ASavaInteractButton::IsHostOnly() const
{
	//ゲーム開始はいつもホストだけ(サーバーでも確かめる)
	return Super::IsHostOnly() || Action == ESavaInteractButtonAction::StartMatch;
}

void ASavaInteractButton::OnInteract(APlayerController* PlayerController)
{
	ASavaPlayerController* SavaPlayerController = Cast<ASavaPlayerController>(PlayerController);

	switch (Action)
	{
	case ESavaInteractButtonAction::JoinTeam:
		if (SavaPlayerController)
		{
			SavaPlayerController->ServerJoinTeam(TeamId);
		}
		break;

	case ESavaInteractButtonAction::ToggleReady:
		if (const ASavaPlayerState* SavaPlayerState = PlayerController->GetPlayerState<ASavaPlayerState>(); SavaPlayerController && SavaPlayerState)
		{
			SavaPlayerController->ServerSetReady(!SavaPlayerState->IsReady());
		}
		break;

	case ESavaInteractButtonAction::LeaveRoom:
		//部屋を抜けるのは自分の PC だけの処理(ホストが抜けると部屋ごと無くなる)
		if (USavaSessionSubsystem* Sessions = PlayerController->GetGameInstance()->GetSubsystem<USavaSessionSubsystem>())
		{
			Sessions->LeaveRoom();
		}
		break;

	case ESavaInteractButtonAction::StartMatch:
		if (SavaPlayerController)
		{
			SavaPlayerController->ServerStartMatch();
		}
		break;

	case ESavaInteractButtonAction::Custom:
		break;
	}

	K2_OnPressed(PlayerController);
}

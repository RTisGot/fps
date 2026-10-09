// Copyright Epic Games, Inc. All Rights Reserved.

#include "savaGameMode.h"
#include "savaCharacter.h"
#include "Player/SavaPlayerController.h"
#include "Player/SavaPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "UObject/ConstructorHelpers.h"

AsavaGameMode::AsavaGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	//能力とチームを持つ PlayerState を使う
	PlayerStateClass = ASavaPlayerState::StaticClass();

	//設定画面・スコアボードを持つ PlayerController を使う(キャラクターが死んでも残るため)
	PlayerControllerClass = ASavaPlayerController::StaticClass();
}

void AsavaGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return; //標準の審査で拒否済み
	}

	//ErrorMessage に文字を入れると、その接続は拒否され、相手に理由として表示される
	if (GetNumPlayers() >= NumTeams * MaxPlayersPerTeam)
	{
		ErrorMessage = TEXT("Match is full.");
	}
}

void AsavaGameMode::PostLogin(APlayerController* NewPlayer)
{
	//チームは体をスポーンする前に決める(PlayerState は Login の時点で作られている)
	AssignTeam(NewPlayer);
	Super::PostLogin(NewPlayer); //この中でスポーンまで行われる
}

void AsavaGameMode::AssignTeam(APlayerController* NewPlayer)
{
	ASavaPlayerState* NewPlayerState = NewPlayer ? NewPlayer->GetPlayerState<ASavaPlayerState>() : nullptr;
	if (!NewPlayerState || !GameState)
	{
		return;
	}

	//人数の少ないチームに入れる
	TArray<int32> TeamCounts;
	TeamCounts.SetNumZeroed(NumTeams);
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		const ASavaPlayerState* SavaPlayerState = Cast<ASavaPlayerState>(PlayerState);
		if (SavaPlayerState && SavaPlayerState != NewPlayerState && TeamCounts.IsValidIndex(SavaPlayerState->GetTeamId()))
		{
			TeamCounts[SavaPlayerState->GetTeamId()]++;
		}
	}

	int32 SmallestTeam = 0;
	for (int32 TeamIndex = 1; TeamIndex < TeamCounts.Num(); ++TeamIndex)
	{
		if (TeamCounts[TeamIndex] < TeamCounts[SmallestTeam])
		{
			SmallestTeam = TeamIndex;
		}
	}
	NewPlayerState->SetTeamId(static_cast<uint8>(SmallestTeam));
}

void AsavaGameMode::NotifyPlayerDied(AController* DeadController)
{
	if (!DeadController)
	{
		return;
	}

	//Controller を弱参照で渡し、待っている間に切断されても安全にする
	FTimerHandle Handle;
	const FTimerDelegate Delegate = FTimerDelegate::CreateUObject(
		this, &AsavaGameMode::RespawnPlayer, TWeakObjectPtr<AController>(DeadController));
	GetWorldTimerManager().SetTimer(Handle, Delegate, RespawnDelay, false);
}

void AsavaGameMode::RespawnPlayer(TWeakObjectPtr<AController> Controller)
{
	AController* PlayerController = Controller.Get();
	if (!PlayerController)
	{
		return; //待っている間に切断した
	}

	//手順 2 で死亡時に Unpossess するが、念のため体が残っていれば捨てる
	if (APawn* OldPawn = PlayerController->GetPawn())
	{
		PlayerController->UnPossess();
		OldPawn->Destroy();
	}

	//新しい体に能力を付ける前に、HP とタグを戻す(ASC は PlayerState にあり、体が変わっても残るため)
	if (ASavaPlayerState* SavaPlayerState = PlayerController->GetPlayerState<ASavaPlayerState>())
	{
		SavaPlayerState->Respawn();
	}

	RestartPlayer(PlayerController);
}

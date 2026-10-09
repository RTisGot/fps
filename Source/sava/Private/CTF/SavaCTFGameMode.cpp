// Fill out your copyright notice in the Description page of Project Settings.

#include "CTF/SavaCTFGameMode.h"
#include "CTF/SavaFlag.h"
#include "CTF/SavaFlagBase.h"
#include "CTF/SavaGameState.h"
#include "CTF/SavaTeamPlayerStart.h"
#include "AbilitySystem/SavaAbilitySystemComponent.h"
#include "AbilitySystem/SavaGameplayEffects.h"
#include "Gadget/GadgetBase.h"
#include "Gadget/GasSmokeArea.h"
#include "Player/SavaPlayerState.h"
#include "Skill/SavaDomeShield.h"
#include "EngineUtils.h"
#include "SavaGrappleRope.h"
#include "TimerManager.h"
#include "savaCharacter.h"
#include "savaProjectile.h"

DEFINE_LOG_CATEGORY_STATIC(LogSavaCTF, Log, All);

ASavaCTFGameMode::ASavaCTFGameMode()
{
	GameStateClass = ASavaGameState::StaticClass();
}

ASavaGameState* ASavaCTFGameMode::GetSavaGameState() const
{
	return GetGameState<ASavaGameState>();
}

void ASavaCTFGameMode::InitGameState()
{
	Super::InitGameState();

	if (ASavaGameState* SavaGameState = GetSavaGameState())
	{
		SavaGameState->InitTeams(NumTeams);
	}
	else
	{
		UE_LOG(LogSavaCTF, Error, TEXT("GameState is not ASavaGameState. Set the GameMode's Game State Class to SavaGameState."));
	}
}

void ASavaCTFGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	//人数が揃ったら最初のラウンドを始める(体のスポーンが終わった次のフレームで)
	const ASavaGameState* SavaGameState = GetSavaGameState();
	if (SavaGameState && SavaGameState->GetRoundPhase() == ESavaRoundPhase::WaitingToStart
		&& GetNumPlayers() >= MinPlayersToStart && !GetWorldTimerManager().IsTimerActive(PhaseTimer))
	{
		PhaseTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ASavaCTFGameMode::StartRound);
	}
}

void ASavaCTFGameMode::Logout(AController* Exiting)
{
	FTimerHandle Handle;
	if (RespawnTimers.RemoveAndCopyValue(Exiting, Handle))
	{
		GetWorldTimerManager().ClearTimer(Handle);
	}
	Super::Logout(Exiting);

	//抜けた人の体が消えてから、全滅したかを調べる
	GetWorldTimerManager().SetTimerForNextTick(this, &ASavaCTFGameMode::CheckRoundEnd);
}

bool ASavaCTFGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	//途中参加でも、自分のチームがリスポーン不可ならラウンドが終わるまで出られない
	const ASavaGameState* SavaGameState = GetSavaGameState();
	const ASavaPlayerState* SavaPlayerState = Player ? Player->GetPlayerState<ASavaPlayerState>() : nullptr;
	if (SavaGameState && SavaPlayerState && SavaGameState->IsRoundInProgress() && !SavaGameState->CanTeamRespawn(SavaPlayerState->GetTeamId()))
	{
		return false;
	}
	return Super::PlayerCanRestart_Implementation(Player);
}

void ASavaCTFGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	//カウントダウン中に出た人は、開始まで動けない
	const ASavaGameState* SavaGameState = GetSavaGameState();
	if (SavaGameState && SavaGameState->GetRoundPhase() == ESavaRoundPhase::PreRound)
	{
		SetRoundFrozen(NewPlayer, true);
	}
}

bool ASavaCTFGameMode::ShouldSpawnAtStartSpot(AController* Player)
{
	//毎回 ChoosePlayerStart で選び直す(既定では最初に出た地点を使い続ける)
	return false;
}

AActor* ASavaCTFGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const ASavaPlayerState* SavaPlayerState = Player ? Player->GetPlayerState<ASavaPlayerState>() : nullptr;
	if (!SavaPlayerState)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	//自チームの地点のうち、近くに生きているキャラクターがいないもの
	TArray<ASavaTeamPlayerStart*> TeamStarts;
	TArray<ASavaTeamPlayerStart*> FreeStarts;
	for (TActorIterator<ASavaTeamPlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->GetTeamId() != SavaPlayerState->GetTeamId())
		{
			continue;
		}
		TeamStarts.Add(*It);

		bool bOccupied = false;
		for (TActorIterator<AsavaCharacter> CharacterIt(GetWorld()); CharacterIt; ++CharacterIt)
		{
			if (IsValid(*CharacterIt) && !CharacterIt->IsDead()
				&& FVector::DistSquared(CharacterIt->GetActorLocation(), It->GetActorLocation()) < FMath::Square(150.0f))
			{
				bOccupied = true;
				break;
			}
		}
		if (!bOccupied)
		{
			FreeStarts.Add(*It);
		}
	}

	if (FreeStarts.Num() > 0)
	{
		return FreeStarts[FMath::RandRange(0, FreeStarts.Num() - 1)];
	}
	if (TeamStarts.Num() > 0)
	{
		return TeamStarts[FMath::RandRange(0, TeamStarts.Num() - 1)];
	}

	UE_LOG(LogSavaCTF, Warning, TEXT("No SavaTeamPlayerStart for team %d. Using a normal PlayerStart."), SavaPlayerState->GetTeamId());
	return Super::ChoosePlayerStart_Implementation(Player);
}

void ASavaCTFGameMode::NotifyPlayerDied(AController* DeadController)
{
	//親クラスのタイマーは取り消せないので使わず、自分で管理する
	const ASavaGameState* SavaGameState = GetSavaGameState();
	const ASavaPlayerState* SavaPlayerState = DeadController ? DeadController->GetPlayerState<ASavaPlayerState>() : nullptr;
	if (SavaGameState && SavaPlayerState && SavaGameState->CanTeamRespawn(SavaPlayerState->GetTeamId()))
	{
		FTimerHandle& Handle = RespawnTimers.FindOrAdd(DeadController);
		const FTimerDelegate Delegate = FTimerDelegate::CreateUObject(
			this, &ASavaCTFGameMode::RespawnPlayer, TWeakObjectPtr<AController>(DeadController));
		GetWorldTimerManager().SetTimer(Handle, Delegate, RespawnDelay, false);
	}

	CheckRoundEnd();
}

void ASavaCTFGameMode::RespawnPlayer(TWeakObjectPtr<AController> Controller)
{
	RespawnTimers.Remove(Controller);

	//カウントダウン中・結果表示中の人は、次のラウンドの開始で出し直される
	const ASavaGameState* SavaGameState = GetSavaGameState();
	if (!SavaGameState || !Controller.IsValid())
	{
		return;
	}
	const ESavaRoundPhase Phase = SavaGameState->GetRoundPhase();
	if (Phase != ESavaRoundPhase::WaitingToStart && !SavaGameState->IsRoundInProgress())
	{
		return;
	}

	//待っている間に自分の旗を持ち帰られた
	const ASavaPlayerState* SavaPlayerState = Controller->GetPlayerState<ASavaPlayerState>();
	if (SavaPlayerState && !SavaGameState->CanTeamRespawn(SavaPlayerState->GetTeamId()))
	{
		return;
	}

	Super::RespawnPlayer(Controller);
}

ASavaFlagBase* ASavaCTFGameMode::FindFlagBase(uint8 TeamId) const
{
	for (TActorIterator<ASavaFlagBase> It(GetWorld()); It; ++It)
	{
		if (It->GetTeamId() == TeamId)
		{
			return *It;
		}
	}
	return nullptr;
}

bool ASavaCTFGameMode::CanTeamCapture(uint8 TeamId) const
{
	const ASavaFlagBase* FlagBase = FindFlagBase(TeamId);
	const ASavaFlag* OwnFlag = FlagBase ? FlagBase->GetFlag() : nullptr;
	if (!OwnFlag)
	{
		return true; //旗台を置いていないチームには条件をかけない
	}
	return OwnFlag->GetFlagState() == ESavaFlagState::AtBase || OwnFlag->GetFlagState() == ESavaFlagState::Removed;
}

bool ASavaCTFGameMode::IsRoundInProgress() const
{
	const ASavaGameState* SavaGameState = GetSavaGameState();
	return SavaGameState && SavaGameState->IsRoundInProgress();
}

void ASavaCTFGameMode::NotifyFlagCaptured(ASavaFlag* CapturedFlag, APawn* Carrier)
{
	ASavaGameState* SavaGameState = GetSavaGameState();
	if (!CapturedFlag || !SavaGameState || !SavaGameState->IsRoundInProgress())
	{
		return;
	}

	const uint8 VictimTeam = CapturedFlag->GetTeamId();
	UE_LOG(LogSavaCTF, Log, TEXT("Team %d flag captured by %s. Team %d can no longer respawn."), VictimTeam, *GetNameSafe(Carrier), VictimTeam);

	CapturedFlag->Remove();
	SavaGameState->SetTeamRespawnDisabled(VictimTeam, true);

	//リスポーン待ちの人も出られなくなるので、この時点で全滅していることがある
	CheckRoundEnd();
}

void ASavaCTFGameMode::StartRound()
{
	ASavaGameState* SavaGameState = GetSavaGameState();
	if (!SavaGameState)
	{
		return;
	}

	ClearRespawnTimers();
	SavaGameState->BeginNewRound();
	//先に段階を変える(この後の RestartPlayer で全員が動けなくなる)
	SavaGameState->SetRoundPhase(ESavaRoundPhase::PreRound, PreRoundDuration);
	UE_LOG(LogSavaCTF, Log, TEXT("Round %d: countdown."), SavaGameState->GetRoundNumber());

	CleanUpWorldForNewRound();
	for (TActorIterator<ASavaFlagBase> It(GetWorld()); It; ++It)
	{
		if (ASavaFlag* Flag = It->GetFlag())
		{
			Flag->ReturnToBase();
		}
	}
	RespawnAllPlayersForNewRound();

	if (PreRoundDuration > 0.0f)
	{
		GetWorldTimerManager().SetTimer(PhaseTimer, this, &ASavaCTFGameMode::BeginRoundPlay, PreRoundDuration, false);
	}
	else
	{
		BeginRoundPlay();
	}
}

void ASavaCTFGameMode::BeginRoundPlay()
{
	ASavaGameState* SavaGameState = GetSavaGameState();
	if (!SavaGameState)
	{
		return;
	}

	SavaGameState->SetRoundPhase(ESavaRoundPhase::InRound, RoundTimeLimit);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		SetRoundFrozen(It->Get(), false);
	}
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &ASavaCTFGameMode::OnRoundTimeUp, RoundTimeLimit, false);
	UE_LOG(LogSavaCTF, Log, TEXT("Round %d: start."), SavaGameState->GetRoundNumber());
}

void ASavaCTFGameMode::OnRoundTimeUp()
{
	const ASavaGameState* SavaGameState = GetSavaGameState();
	if (!SavaGameState || SavaGameState->GetRoundPhase() != ESavaRoundPhase::InRound)
	{
		return;
	}

	//旗が残っている(リスポーンできる)チームを数える
	TArray<uint8> TeamsWithFlag;
	for (int32 TeamIndex = 0; TeamIndex < NumTeams; ++TeamIndex)
	{
		if (SavaGameState->CanTeamRespawn(static_cast<uint8>(TeamIndex)))
		{
			TeamsWithFlag.Add(static_cast<uint8>(TeamIndex));
		}
	}

	//片方だけ持ち帰られていたら、旗が残っているチームの勝ち
	if (TeamsWithFlag.Num() == 1)
	{
		UE_LOG(LogSavaCTF, Log, TEXT("Time up. Team %d still has its flag."), TeamsWithFlag[0]);
		EndRound(TeamsWithFlag[0]);
		return;
	}

	//どちらも持ち帰られていない(または両方とも持ち帰られている)ならデスマッチ
	StartDeathmatch();
}

void ASavaCTFGameMode::StartDeathmatch()
{
	ASavaGameState* SavaGameState = GetSavaGameState();
	if (!SavaGameState)
	{
		return;
	}

	UE_LOG(LogSavaCTF, Log, TEXT("Time up. Deathmatch: no respawn."));
	SavaGameState->SetRoundPhase(ESavaRoundPhase::Deathmatch, 0.0f);
	for (int32 TeamIndex = 0; TeamIndex < NumTeams; ++TeamIndex)
	{
		SavaGameState->SetTeamRespawnDisabled(static_cast<uint8>(TeamIndex), true);
	}
	for (TActorIterator<ASavaFlagBase> It(GetWorld()); It; ++It)
	{
		if (ASavaFlag* Flag = It->GetFlag())
		{
			Flag->Remove();
		}
	}
	ClearRespawnTimers();

	//リスポーン待ちの人がいたチームは、この時点で全滅していることがある
	CheckRoundEnd();
}

void ASavaCTFGameMode::CheckRoundEnd()
{
	const ASavaGameState* SavaGameState = GetSavaGameState();
	if (!SavaGameState || !SavaGameState->IsRoundInProgress())
	{
		return;
	}

	TArray<uint8> SurvivingTeams;
	for (int32 TeamIndex = 0; TeamIndex < NumTeams; ++TeamIndex)
	{
		if (!IsTeamEliminated(static_cast<uint8>(TeamIndex)))
		{
			SurvivingTeams.Add(static_cast<uint8>(TeamIndex));
		}
	}

	if (SurvivingTeams.Num() == 1)
	{
		EndRound(SurvivingTeams[0]);
	}
	else if (SurvivingTeams.Num() == 0)
	{
		EndRound(ASavaGameState::NoTeam); //相打ちで両チーム全滅
	}
}

bool ASavaCTFGameMode::IsTeamEliminated(uint8 TeamId) const
{
	const ASavaGameState* SavaGameState = GetSavaGameState();
	if (!SavaGameState || SavaGameState->CanTeamRespawn(TeamId))
	{
		return false;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		const ASavaPlayerState* SavaPlayerState = PlayerController ? PlayerController->GetPlayerState<ASavaPlayerState>() : nullptr;
		if (!SavaPlayerState || SavaPlayerState->GetTeamId() != TeamId)
		{
			continue;
		}
		const AsavaCharacter* Character = Cast<AsavaCharacter>(PlayerController->GetPawn());
		if (Character && !Character->IsDead())
		{
			return false; //まだ生きている人がいる
		}
	}
	return true;
}

void ASavaCTFGameMode::EndRound(uint8 WinnerTeamId)
{
	ASavaGameState* SavaGameState = GetSavaGameState();
	if (!SavaGameState)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(PhaseTimer);
	ClearRespawnTimers();

	SavaGameState->AddRoundWin(WinnerTeamId); //引き分け(NoTeam)なら勝者の記録だけ更新される
	UE_LOG(LogSavaCTF, Log, TEXT("Round %d: winner = %d."), SavaGameState->GetRoundNumber(), WinnerTeamId);

	//試合の勝者が決まった
	if (WinnerTeamId != ASavaGameState::NoTeam && SavaGameState->GetTeamRoundWins(WinnerTeamId) >= RoundsToWin)
	{
		SavaGameState->SetMatchWinner(WinnerTeamId);
		SavaGameState->SetRoundPhase(ESavaRoundPhase::MatchEnd, 0.0f);
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			SetRoundFrozen(It->Get(), true);
		}
		UE_LOG(LogSavaCTF, Log, TEXT("Match end: team %d wins."), WinnerTeamId);
		return;
	}

	SavaGameState->SetRoundPhase(ESavaRoundPhase::RoundEnd, RoundEndDuration);
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &ASavaCTFGameMode::StartRound, FMath::Max(RoundEndDuration, KINDA_SMALL_NUMBER), false);
}

void ASavaCTFGameMode::CleanUpWorldForNewRound()
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		const AsavaCharacter* Character = Cast<AsavaCharacter>(Actor);
		const bool bDeadBody = Character && Character->IsDead();
		const bool bLeftover = Actor->IsA<AGadgetBase>() || Actor->IsA<AGasSmokeArea>() || Actor->IsA<ASavaDomeShield>()
			|| Actor->IsA<AsavaProjectile>() || Actor->IsA<ASavaGrappleRope>();
		if (bDeadBody || bLeftover)
		{
			Actor->Destroy();
		}
	}
}

void ASavaCTFGameMode::RespawnAllPlayersForNewRound()
{
	//先に全員の体を消す(スポーン地点の空きを正しく判定するため)
	TArray<AController*> Controllers;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (!PlayerController)
		{
			continue;
		}
		Controllers.Add(PlayerController);

		if (APawn* OldPawn = PlayerController->GetPawn())
		{
			PlayerController->UnPossess();
			OldPawn->Destroy();
		}
		if (ASavaPlayerState* SavaPlayerState = PlayerController->GetPlayerState<ASavaPlayerState>())
		{
			SavaPlayerState->ResetForNewRound();
		}
	}

	for (AController* Controller : Controllers)
	{
		RestartPlayer(Controller);
	}
}

void ASavaCTFGameMode::SetRoundFrozen(AController* Controller, bool bFrozen)
{
	ASavaPlayerState* SavaPlayerState = Controller ? Controller->GetPlayerState<ASavaPlayerState>() : nullptr;
	UAbilitySystemComponent* AbilitySystem = SavaPlayerState ? SavaPlayerState->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystem)
	{
		return;
	}

	//二重に付かないように、いったん外してから付ける
	AbilitySystem->RemoveActiveGameplayEffectBySourceEffect(USavaGE_RoundFrozen::StaticClass(), nullptr);
	if (bFrozen)
	{
		//発動中の能力は止めない(パッシブ能力まで止まって、再発動されなくなるため)
		AbilitySystem->ApplyGameplayEffectToSelf(GetDefault<USavaGE_RoundFrozen>(), 1.0f, AbilitySystem->MakeEffectContext());
	}
}

void ASavaCTFGameMode::ClearRespawnTimers()
{
	for (TPair<TWeakObjectPtr<AController>, FTimerHandle>& Pair : RespawnTimers)
	{
		GetWorldTimerManager().ClearTimer(Pair.Value);
	}
	RespawnTimers.Reset();
}

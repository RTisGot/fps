#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CTF/SavaGameState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Player/SavaPlayerState.h"
#include "SavaDeathmatchGameMode.h"

namespace
{
	//テスト用の一時的なワールド(スコープを抜けると破棄する)
	struct FSavaDeathmatchTestWorld
	{
		UWorld* World = nullptr;
		ASavaGameState* GameState = nullptr;

		FSavaDeathmatchTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			GameState = World->SpawnActor<ASavaGameState>();
		}

		~FSavaDeathmatchTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		//キル数・デス数を決めて GameState に入れる
		ASavaPlayerState* SpawnPlayer(const FString& Name, uint8 TeamId, int32 Kills, int32 Deaths = 0) const
		{
			ASavaPlayerState* PlayerState = World->SpawnActor<ASavaPlayerState>();
			PlayerState->SetPlayerName(Name);
			PlayerState->SetTeamId(TeamId);
			for (int32 Index = 0; Index < Kills; ++Index)
			{
				PlayerState->AddKill();
			}
			for (int32 Index = 0; Index < Deaths; ++Index)
			{
				PlayerState->AddDeath();
			}
			GameState->AddPlayerState(PlayerState);
			return PlayerState;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaDeathmatchFreeForAllWinnerTest, "Sava.Deathmatch.FreeForAllWinner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaDeathmatchFreeForAllWinnerTest::RunTest(const FString&)
{
	FSavaDeathmatchTestWorld TestWorld;
	TestWorld.SpawnPlayer(TEXT("A"), ASavaPlayerState::NoTeam, 3);
	ASavaPlayerState* Best = TestWorld.SpawnPlayer(TEXT("B"), ASavaPlayerState::NoTeam, 5);
	TestWorld.SpawnPlayer(TEXT("C"), ASavaPlayerState::NoTeam, 1);

	TArray<ASavaPlayerState*> Winners;
	int32 WinningKills = 0;
	uint8 WinnerTeam = 0;
	ASavaDeathmatchGameMode::DecideWinners(TestWorld.GameState->PlayerArray, false, 2, Winners, WinningKills, WinnerTeam);
	TestEqual(TEXT("One winner"), Winners.Num(), 1);
	TestTrue(TEXT("Most kills wins"), Winners.Num() == 1 && Winners[0] == Best);
	TestEqual(TEXT("Winning kills"), WinningKills, 5);
	TestEqual(TEXT("No winning team in free-for-all"), WinnerTeam, ASavaGameState::NoTeam);

	//同数なら全員が勝者(引き分け)
	TestWorld.SpawnPlayer(TEXT("D"), ASavaPlayerState::NoTeam, 5);
	ASavaDeathmatchGameMode::DecideWinners(TestWorld.GameState->PlayerArray, false, 2, Winners, WinningKills, WinnerTeam);
	TestEqual(TEXT("Tied players all win"), Winners.Num(), 2);

	TestWorld.GameState->SetDeathmatchRules(false, 10);
	TestWorld.GameState->SetDeathmatchResults(Winners, WinningKills, WinnerTeam);
	TestTrue(TEXT("Tie is a draw"), TestWorld.GameState->IsDeathmatchDraw());
	TestEqual(TEXT("Winner names are kept"), TestWorld.GameState->GetDeathmatchWinnerNames().Num(), 2);
	TestEqual(TEXT("Winners are kept"), TestWorld.GameState->GetDeathmatchWinners().Num(), 2);

	TestWorld.GameState->SetDeathmatchResults({ Best }, 5, ASavaGameState::NoTeam);
	TestFalse(TEXT("One winner is not a draw"), TestWorld.GameState->IsDeathmatchDraw());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaDeathmatchTeamWinnerTest, "Sava.Deathmatch.TeamWinner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaDeathmatchTeamWinnerTest::RunTest(const FString&)
{
	FSavaDeathmatchTestWorld TestWorld;
	//1 人で一番多くても、チームの合計で決まる
	TestWorld.SpawnPlayer(TEXT("Ace"), 0, 6);
	TestWorld.SpawnPlayer(TEXT("A2"), 0, 0);
	TestWorld.SpawnPlayer(TEXT("B1"), 1, 4);
	TestWorld.SpawnPlayer(TEXT("B2"), 1, 4);

	TestEqual(TEXT("Team 0 kills"), TestWorld.GameState->GetTeamKills(0), 6);
	TestEqual(TEXT("Team 1 kills"), TestWorld.GameState->GetTeamKills(1), 8);

	TArray<ASavaPlayerState*> Winners;
	int32 WinningKills = 0;
	uint8 WinnerTeam = ASavaGameState::NoTeam;
	ASavaDeathmatchGameMode::DecideWinners(TestWorld.GameState->PlayerArray, true, 2, Winners, WinningKills, WinnerTeam);
	TestEqual(TEXT("Team with more total kills wins"), WinnerTeam, static_cast<uint8>(1));
	TestEqual(TEXT("Team total"), WinningKills, 8);
	TestEqual(TEXT("Whole team is listed"), Winners.Num(), 2);

	TestWorld.GameState->SetDeathmatchRules(true, 20);
	TestWorld.GameState->SetDeathmatchResults(Winners, WinningKills, WinnerTeam);
	TestEqual(TEXT("Match winner is the team"), TestWorld.GameState->GetMatchWinner(), static_cast<uint8>(1));
	TestFalse(TEXT("Not a draw"), TestWorld.GameState->IsDeathmatchDraw());

	//同数なら引き分け(勝ったチームなし、両チームの全員)
	TestWorld.SpawnPlayer(TEXT("A3"), 0, 2);
	ASavaDeathmatchGameMode::DecideWinners(TestWorld.GameState->PlayerArray, true, 2, Winners, WinningKills, WinnerTeam);
	TestEqual(TEXT("Tied teams have no winner"), WinnerTeam, ASavaGameState::NoTeam);
	TestEqual(TEXT("Everyone of the tied teams is listed"), Winners.Num(), 5);
	TestWorld.GameState->SetDeathmatchResults(Winners, WinningKills, WinnerTeam);
	TestTrue(TEXT("Tied teams are a draw"), TestWorld.GameState->IsDeathmatchDraw());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaDeathmatchRulesTest, "Sava.Deathmatch.RulesAndRanking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaDeathmatchRulesTest::RunTest(const FString&)
{
	FSavaDeathmatchTestWorld TestWorld;
	TestFalse(TEXT("Not a deathmatch until rules are set"), TestWorld.GameState->IsDeathmatch());

	TestWorld.GameState->SetDeathmatchRules(true, 25);
	TestTrue(TEXT("Deathmatch"), TestWorld.GameState->IsDeathmatch());
	TestTrue(TEXT("Team match"), TestWorld.GameState->IsDeathmatchTeamMatch());
	TestEqual(TEXT("Kill limit"), TestWorld.GameState->GetDeathmatchKillLimit(), 25);

	ASavaPlayerState* Third = TestWorld.SpawnPlayer(TEXT("Third"), 0, 2, 5);
	ASavaPlayerState* First = TestWorld.SpawnPlayer(TEXT("First"), 1, 4, 3);
	ASavaPlayerState* Second = TestWorld.SpawnPlayer(TEXT("Second"), 0, 2, 1);

	//キル数の多い順、同じならデスの少ない順
	const TArray<ASavaPlayerState*> Ranking = TestWorld.GameState->GetPlayersByKills();
	TestTrue(TEXT("Ranking order"), Ranking.Num() == 3 && Ranking[0] == First && Ranking[1] == Second && Ranking[2] == Third);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaDeathmatchSpawnTest, "Sava.Deathmatch.SafestStarts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaDeathmatchSpawnTest::RunTest(const FString&)
{
	const TArray<FVector> Starts = { FVector(0, 0, 0), FVector(1000, 0, 0), FVector(5000, 0, 0) };

	//敵がいなければ空いている地点すべて
	TestEqual(TEXT("All starts without enemies"), ASavaDeathmatchGameMode::FindSafestStarts(Starts, {}, {}).Num(), 3);

	//敵から一番遠い地点を選ぶ
	TArray<int32> Safest = ASavaDeathmatchGameMode::FindSafestStarts(Starts, { FVector(-100, 0, 0) }, {});
	TestTrue(TEXT("Farthest from the enemy"), Safest.Num() == 1 && Safest[0] == 2);

	//人が立っている地点は使わない
	Safest = ASavaDeathmatchGameMode::FindSafestStarts(Starts, { FVector(-100, 0, 0) }, { FVector(5050, 0, 0) });
	TestTrue(TEXT("Occupied start is skipped"), Safest.Num() == 1 && Safest[0] == 1);

	//全部使用中なら全部が候補(出られないよりはよい)
	Safest = ASavaDeathmatchGameMode::FindSafestStarts(Starts, {}, { FVector(0, 0, 0), FVector(1000, 0, 0), FVector(5000, 0, 0) });
	TestEqual(TEXT("All occupied falls back to all"), Safest.Num(), 3);

	//同じくらい安全な地点は両方とも候補(毎回同じ地点にならない)
	const TArray<FVector> TwoFar = { FVector(0, 0, 0), FVector(4000, 0, 0), FVector(-3800, 0, 0) };
	Safest = ASavaDeathmatchGameMode::FindSafestStarts(TwoFar, { FVector(0, 0, 0) }, {});
	TestTrue(TEXT("Nearly as safe starts are candidates"), Safest.Num() == 2 && Safest.Contains(1) && Safest.Contains(2));

	return true;
}

#endif

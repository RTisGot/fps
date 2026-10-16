// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Loadout/SavaLoadoutTypes.h"
#include "Lobby/SavaLobbyTypes.h"
#include "SavaPlayerController.generated.h"

class ASavaPlayerState;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSavaOnLobbyMessage, const FText&, Message);
class USavaScoreboardWidget;
class USavaSettingsMenuController;

//プレイヤーの操作の窓口。設定画面(Esc)とスコアボード(Tab)をここで持つ
//キャラクターは死ぬと消えるが、PlayerController は残るので、リスポーンしても画面が開いたまま続く
UCLASS()
class SAVA_API ASavaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASavaPlayerController();

	//設定画面を開いているか(開いている間はキャラの操作を受け付けない)
	UFUNCTION(BlueprintPure, Category = "UI|Settings")
	bool IsSettingsMenuOpen() const;

	//設定画面を閉じる
	UFUNCTION(BlueprintCallable, Category = "UI|Settings")
	void CloseSettingsMenu();

	//自分のロードアウトをサーバーへ送る(呼ぶのは自分の PC、実行されるのはサーバー)
	UFUNCTION(Server, Reliable)
	void ServerSetLoadout(const FSavaLoadout& Loadout);

	//--------------------------------ロビー(待合室)
	//ボタン・端末はサーバーの物なので、クライアントからは直接頼めない。自分の PlayerController を通してサーバーへ頼む
	//受け付けるかはサーバーの ASavaLobbyGameMode が決め、だめなら理由が OnLobbyMessage で届く

	//部屋のホストが操作している PlayerController か(listen サーバーで、その PC で操作している人がホスト)
	//自分の画面でもサーバーでも同じ答えになる
	static bool IsHostController(const APlayerController* PlayerController);

	//自分が部屋のホストか(ソロでも true)
	UFUNCTION(BlueprintPure, Category = "Sava|Lobby")
	bool IsRoomHost() const { return IsHostController(this); }

	//自分のチームを変える
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Sava|Lobby")
	void ServerJoinTeam(uint8 TeamId);

	//準備 OK を切り替える
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Sava|Lobby")
	void ServerSetReady(bool bReady);

	//ホストだけ: ルーム設定を変える
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Sava|Lobby")
	void ServerUpdateRoomSettings(const FSavaRoomSettings& NewSettings);

	//ホストだけ: 他の人のチームを変える
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Sava|Lobby")
	void ServerMovePlayerToTeam(ASavaPlayerState* Target, uint8 TeamId);

	//ホストだけ: 他の人を部屋から追い出す
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Sava|Lobby")
	void ServerKickPlayer(ASavaPlayerState* Target);

	//ホストだけ: 試合を始める
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Sava|Lobby")
	void ServerStartMatch();

	//サーバーからのお知らせ(頼みを断られた理由など)。画面に出すなら UI でこれを受け取る
	UPROPERTY(BlueprintAssignable, Category = "Sava|Lobby")
	FSavaOnLobbyMessage OnLobbyMessage;

	// デスマッチの結果を表示する
	UFUNCTION(Client, Reliable)
	void ClientShowDeathmatchResult();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Settings")
	TSubclassOf<UUserWidget> SettingsWidgetClass;

	/** スコアボードWidgetのクラス */
	UPROPERTY(EditDefaultsOnly, Category = "UI|Scoreboard")
	TSubclassOf<USavaScoreboardWidget> ScoreboardWidgetClass;

	//ホストに追い出された。この後切断されるので、ソロロビーで理由を出せるように伝えておく
	virtual void ClientWasKicked_Implementation(const FText& KickReason) override;

private:
	//サーバーで、ロビーの GameMode に頼む。断られたら理由を ClientShowLobbyMessage で返す(ロビー以外のマップでは何もしない)
	void RunLobbyRequest(TFunctionRef<bool(class ASavaLobbyGameMode&, FText&)> Request);

	//サーバーから自分の画面へお知らせを送る
	UFUNCTION(Client, Reliable)
	void ClientShowLobbyMessage(const FText& Message);

	//自分の PC でロードアウトが変わった(USavaLoadoutSubsystem から)
	UFUNCTION()
	void HandleLocalLoadoutChanged(const FSavaLoadout& Loadout);

	void ToggleSettingsMenu();

	/** スコアボードを表示する */
	void ShowScoreboard();

	/** スコアボードを非表示にする */
	void HideScoreboard();

	//今操作しているキャラクターの操作を受け付けるか(キャラクターがいなければ何もしない)
	void SetPawnGameplayInputEnabled(bool bEnabled);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> SettingsWidget;//生成した設定画面Widgetへの参照を保持する変数

	UPROPERTY(Transient)
	TObjectPtr<USavaSettingsMenuController> SettingsMenuController;

	/** 現在表示しているスコアボード */
	UPROPERTY(Transient)
	TObjectPtr<USavaScoreboardWidget> ScoreboardWidget;

	// デスマッチの結果画面のWidgetクラス
	UPROPERTY(EditDefaultsOnly, Category = "UI|Deathmatch")
	TSubclassOf<UUserWidget> DeathmatchResultWidgetClass;

	// 作成した結果画面Widget
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> DeathmatchResultWidget;
};

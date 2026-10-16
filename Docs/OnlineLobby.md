# オンラインロビー(待合室)

タイトル → ソロロビー → **オンラインロビー** → 試合 の流れのうち、オンラインロビーの仕組みと、エディタでの組み立て方。

## 流れ

1. ソロロビーのマッチ端末で部屋を作る → ホストは `OnlineLobby` を listen サーバーとして開く(GameMode は `SavaLobbyGameMode`)
2. 参加者は部屋を探して入る → 人数の少ないチームへ自動で入る
3. ボタンでチーム切替・準備 OK、ホストはルーム設定の端末でモード・マップなどを決める
4. ホストが「ゲーム開始」 → 部屋を閉じる(途中参加・検索を止める)→ 1 秒後に試合マップへ全員で移る(Seamless Travel)
5. 試合マップの GameMode が、ロビーから全員そろうのを待ってから始める(最大 30 秒)
6. 試合が終わる(CTF なら先取ラウンド数に達する)→ 結果を見る間 `ReturnToLobbyDelay`(既定 10 秒)待つ → ホストが全員を連れてオンラインロビーへ戻る(Seamless Travel)
7. ロビーに着いたら部屋を開き直す(途中参加・検索を再開)。チームとルーム設定は前の試合のまま、準備 OK は押し直し

試合マップを直接開いて試した(部屋がない)ときは、試合が終わるとソロロビーへ戻る。

## 仕組み(Unity との対応)

| UE | 役割 | Unity でいうと |
|---|---|---|
| `ASavaLobbyGameMode` | ロビーのルール。サーバーにだけある | サーバー専用の Manager |
| `ASavaLobbyGameState` | ルーム設定・開始中か。全員に同期 | NetworkVariable を持つ共有オブジェクト |
| `ASavaPlayerState` | 各プレイヤーのチーム・準備 OK・ホスト印。全員に同期 | プレイヤーごとの NetworkVariable |
| `ASavaPlayerController::Server〜` | クライアントからサーバーへの頼み | `[ServerRpc]` |

マップに置いたボタンはサーバーの物なので、クライアントから直接 RPC を送れない(Unity でも所有していない NetworkObject からは ServerRpc を送れないのと同じ)。
そのため、押したことは自分の画面で受け取り、自分の PlayerController の `Server〜` で頼む。
断られたとき(満員・ホスト以外など)は、理由が `OnLobbyMessage` で届く(画面がまだなければ左上にデバッグ表示)。

**ホスト** = listen サーバーで、その PC で操作している PlayerController(`ASavaPlayerController::IsRoomHost`)。
ホスト限定の操作は、手元(案内・押せるか)とサーバー(GameMode)の両方で確かめる。

## 置ける物

どれも、近づいてしゃがみキーで使う。共通の設定(親 `SavaInteractable`):

| 項目 | 内容 |
|---|---|
| Mesh | 見た目 |
| Interact Area | この箱の中で使える |
| Host Only | ホストだけが使える |
| Prompt Widget Class | 範囲内で出す案内 |
| Host Only Prompt Widget Class | ホスト限定のとき、ホスト以外に出す案内(「ホストのみ」) |

ホスト以外がホスト限定の物の前でしゃがみキーを押すと、普通にしゃがむ。

### ボタン `SavaInteractButton`

| Action | すること |
|---|---|
| Join Team | `Team Id` のチームに入る(0 = チーム 1、1 = チーム 2。満員なら断られる) |
| Toggle Ready | 準備 OK を切り替える(ホストは押せない。案内も出ない) |
| Leave Room | 部屋を抜けてソロロビーへ(ホストが抜けると部屋ごと無くなる) |
| Start Match | 試合を始める(Host Only に関係なく、いつもホスト限定) |
| Custom | 何もしない。Blueprint の `On Pressed` で処理を書く |

音・光などの演出は Blueprint の `On Pressed`(押した本人の画面だけで呼ばれる)で付ける。

### 端末 `SavaInteractTerminal`

画面(`Menu Widget Class`、親は `SavaTerminalWidget`)を開く。ルーム設定の端末は **Host Only** にする。

## ルーム設定

`FSavaRoomSettings`(モード番号・マップ番号・勝利ラウンド数・制限時間)。候補は **Project Settings → Game → Sava Lobby**:

- **Modes**: モードごとに表示名・GameMode・開始に必要な人数・ラウンド制か・遊べるマップ
  - CTF: `SavaCTFGameMode`、4 人以上
  - Deathmatch: GameMode が空(制作中)。できたら GameModeClass に入れるだけで選べる
- 勝利ラウンド数・制限時間の範囲と最初の値

開始の条件: ホスト以外の全員が準備 OK、かつ人数がモードの MinPlayers 以上(ホストは準備 OK 不要)。
`SavaLobbyGameState` の `Can Start Match` で、開始ボタンの横に理由を出せる。

### 試合の GameMode への渡し方

試合マップへは URL のオプションで渡す(`SavaMatchOptions`):

```
/Game/Map/FPSMap?listen?game=/Script/sava.SavaCTFGameMode?Rounds=2?TimeLimit=180?Players=6
```

| オプション | 内容 |
|---|---|
| `Rounds` | 勝利ラウンド数(ラウンド制のモードだけ付く) |
| `TimeLimit` | 制限時間(秒) |
| `Players` | ロビーから移ってくる人数 |

デスマッチの GameMode を作る人へ: `InitGame` の `Options` から `UGameplayStatics::GetIntOption(Options, TEXT("TimeLimit"), 既定値)` で読む。
チームは `ASavaPlayerState` ごと引き継がれる(`AsavaGameMode` を親にすれば、決まっていない人だけ自動で振り分ける)。
Seamless Travel で来た人には `PostLogin` が呼ばれないので、人数を数えるなら `HandleSeamlessTravelPlayer` も見る(`ASavaCTFGameMode` を参照)。

## エディタでの組み立て

1. **ボタンの BP**: `SavaInteractButton` を親に Blueprint を作り、Mesh を設定する(例: `BP_TeamButton`、`BP_ReadyButton`、`BP_LeaveButton`、`BP_StartButton`)。Action・Team Id は置いた後のインスタンスで変えてもよい
2. **案内 Widget**: `WBP_ButtonPrompt`(「しゃがみで押す」)、`WBP_HostOnlyPrompt`(「ホストのみ」)を作り、各 BP の Prompt Widget Class / Host Only Prompt Widget Class に入れる
3. **ルーム設定の画面** `WBP_RoomSettings`(親 `SavaTerminalWidget`):
   - 読む: `Get Game State` → `Cast to SavaLobbyGameState` → `Get Room Settings` / `Get Lobby Players`。`On Lobby Changed` に描き直しを登録する
   - 候補: `Get Mode Options`(モードとマップの一覧)、`Get Sava Lobby Settings`(範囲)
   - 変える: `Get Owning Player` → `Cast to SavaPlayerController` → `Server Update Room Settings`
   - メンバー管理: 一覧の各行から `Server Kick Player` / `Server Move Player To Team`
   - 閉じる: `Close Terminal`
4. **ルーム設定の端末**: 既存の `BP_RoomSettings`(親は `SavaInteractTerminal`)の Menu Widget Class に `WBP_RoomSettings` を入れ、**Host Only** をオンにする
5. **`OnlineLobby` マップ**に配置する: チーム切替 × 2(Team Id 0 / 1)、準備 OK、退出、ゲーム開始、ルーム設定の端末、ロードアウトのクレート
   - GameMode は部屋を作るときに `SavaLobbyGameMode` が自動で使われる(World Settings は変えなくてよい)
6. (任意)ロビーの HUD: `On Lobby Changed` でチームごとの一覧・準備 OK・ホスト印(`Is Room Host`)を出す。`On Lobby Message` で断られた理由を出す

## 確認

### テスト用コンソールコマンド(UI ができるまで。` キーで打つ)

| コマンド | 内容 |
|---|---|
| `Sava.CreateRoom 名前` / `Sava.FindRooms` / `Sava.JoinRoom 番号` / `Sava.LeaveRoom` | 部屋を作る・探す・入る・抜ける |
| `Sava.JoinTeam 0` | チームに入る |
| `Sava.Ready` / `Sava.Ready 0` | 準備 OK / 解除 |
| `Sava.RoomSettings モード マップ ラウンド 秒` | ホストだけ: ルーム設定(書かなかった項目はそのまま) |
| `Sava.MovePlayer 名前 チーム` / `Sava.Kick 名前` | ホストだけ: メンバー管理 |
| `Sava.StartMatch` | ホストだけ: 試合を始める |
| `Sava.LobbyStatus` | ルーム設定・全員のチーム/準備 OK・開始できるかをログに出す |
| `Sava.LocalStatus` | 自分の PlayerController・体・表示中の画面をログに出す |
| `Sava.Lobby.MinPlayersOverride 2` | 開発用: 開始に必要な人数を上書き(ホストで打つ。0 で戻す) |
| `Sava.After 秒 コマンド` | 秒数の後にコマンドを打つ(起動オプションの `-ExecCmds` で流れを自動で試す用) |

### PIE

- **Net Mode = Play Standalone**、**Number of Players = 2** にして、タイトルから: 1 人目が部屋を作り、2 人目が部屋を探して入る
  - Play As Listen Server だと部屋を作らずに直接マップを開くので、ロビーの GameMode にならない
- 2 人で CTF を試すときは、ホストで `Sava.Lobby.MinPlayersOverride 2`
- PIE でも Seamless Travel するように `net.AllowPIESeamlessTravel=1` を DefaultEngine.ini に入れてある
- 自動テスト: Session Frontend → Automation で `Sava.Lobby`

### 2 つ起動して流れを自動で試す(エディタを閉じて、PowerShell で)

```
$exe = "D:\unreal\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe"
$proj = "E:\school\works\unreal\fps\sava.uproject"
Start-Process $exe "$proj -game -windowed -ResX=960 -ResY=540 -log -ExecCmds=`"Sava.SetPlayerName Host,Sava.Lobby.MinPlayersOverride 2,Sava.After 5 Sava.CreateRoom Test,Sava.After 45 Sava.StartMatch`""
Start-Process $exe "$proj -game -windowed -ResX=960 -ResY=540 -log -ExecCmds=`"Sava.After 3 Sava.SetPlayerName Guest,Sava.After 20 Sava.FindRooms,Sava.After 27 Sava.JoinRoom 0,Sava.After 38 Sava.Ready`""
```

部屋の作成 → 参加 → 準備 OK → 開始 → FPSMap へ Seamless Travel → CTF のラウンド開始 まで通ることを確認済み(チームは引き継がれ、HUD は両方に 1 つずつ)。
(プレイヤー名は `Saved/SaveGames/Profile.sav` に保存されるので、最後に打った名前が残る)

## マップ移動まわりで直したこと

- **参加者の HUD**: Seamless Travel の直後、参加者の PC では PlayerController が作り直され、PC とつながる前に体が届くことがある。
  そのまま BP_FirstPersonCharacter の HUD 作成が走ると失敗するので、`AsavaCharacter::NotifyControllerChanged` はつながるまで待ち、Tick でやり直す
- **HUD の重なり**: BP_FirstPersonCharacter は体ごとに HUD を作るが外していなかったため、リスポーン・ラウンドの出し直しのたびに重なっていた。
  体が操作されなくなったとき・消えたときに、体が変数に持っている Widget を外す(`AsavaCharacter::RemoveOwnedWidgets`)。死んでからリスポーンまでの間は HUD が消える

## 試合マップの準備

FPSMap には今 `SavaFlagBase` も `SavaTeamPlayerStart` もない。CTF は動く(普通の PlayerStart から出て、時間切れでデスマッチになる)が、
旗を取り合うには、各チームの旗の台 1 個と出現地点 3 個以上を置く。

## まだないもの

- 試合の結果画面(今は `MatchEnd` の間、デバッグ表示だけ。残り時間 = ロビーへ戻るまでの秒数なので、カウントダウンも出せる)
- デスマッチから戻す(`SavaDeathmatchGameMode` の試合終了で `ScheduleReturnToLobby()` を呼べばよい)
- 試合中に抜けた人の扱い・途中参加
- EOS(今は LAN のみ)

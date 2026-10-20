# デスマッチ仕様

倒されてもリスポーンして戦い続け、**キル数で勝敗を決める**モード。**個人戦**と**チーム戦**を部屋のルーム設定で選ぶ。

## ルール

| 項目 | 個人戦 | チーム戦 |
|---|---|---|
| 敵 | 自分以外の全員 | 相手チームの人(味方には当たらない) |
| チーム | 全員チームなし(待合室のチームは外れる) | 待合室で決めたチームのまま |
| 勝ち(キル数) | 勝利キル数に最初に届いた人 | チームの**合計キル**が勝利キル数に最初に届いたチーム |
| 勝ち(時間切れ) | キル数が一番多い人 | 合計キルが一番多いチーム |
| 引き分け | 1 位が複数いる(全員を勝者として記録) | 合計キルが同じ(並んだチームの全員を記録、勝ったチームはなし) |

- 倒されたら `RespawnDelay`(既定 4 秒)後にリスポーンする。試合が終わったら出ない
- 待っている間(カウントダウン前)のキル・デスは、試合の開始で 0 に戻す
- 試合中はロードアウトを変えられない(待合室を通らずに入ったときの最初の 1 回だけ受け付ける)

### スポーン地点

毎回選び直す(同じ地点から出続けない)。

1. 候補: チーム戦なら自チームの `SavaTeamPlayerStart`(なければ全部)、個人戦ならチーム用も含めた全部の `PlayerStart`
2. 近く(150 以内)に生きている人がいる地点は除く(全部埋まっていれば全部を候補に戻す)
3. 「一番近い敵までの距離」が一番遠い地点と、その 8 割以上離れている地点の中からランダム

### 試合の流れ

```
WaitingToStart(人がそろうまで)→ PreRound(カウントダウン・動けない)→ InRound → MatchEnd(結果画面・動けない)→ 部屋のロビーへ
```

| 項目 | 初期値 | ルーム設定 |
|---|---|---|
| 制限時間 | 300 秒 | `TimeLimit` で上書き |
| 勝利キル数 | 10 | `Kills` で上書き(5〜100) |
| 個人戦 / チーム戦 | 個人戦 | `Teams` で上書き |
| カウントダウン | 3 秒 | — |
| 結果を見る時間 | 10 秒(`ReturnToLobbyDelay`) | — |
| 待合室から来る人を待つ上限 | 30 秒 | — |

待合室を通らずにマップを直接開いたときは、ルーム設定がないので初期値で始まる(`MinPlayersToStart` 人そろったら)。

## クラス構成

| クラス | 役割 |
|---|---|
| `ASavaDeathmatchGameMode` | `AsavaGameMode` を継承。試合の進行・勝敗判定・スポーン地点選び |
| `ASavaGameState` | CTF と共通。デスマッチのルール・結果を全員に配る(下の表) |
| `ASavaPlayerState` | キル数・デス数。キルが増えたら GameMode の `CheckKillLimit` を呼ぶ |
| `ASavaPlayerController` | `ClientShowDeathmatchResult` で結果画面(`WBP_DeathmatchResult`)を出す |

- 個人戦は `AsavaGameMode::bUseTeams = false` で全員チームなしにしている(`USavaAbilitySystemLibrary::AreEnemies` がチームなし同士を敵と判定する)
- 旗台(`SavaFlagBase`)は CTF 以外では自分を消すので、CTF 用のマップをそのまま使える

## UI(BP)から使えるもの

すべて `SavaGameState`(Get Game State → Cast To SavaGameState)にある。

| ノード | 内容 |
|---|---|
| `Is Deathmatch` | デスマッチの試合か |
| `Is Deathmatch Team Match` | チーム戦か |
| `Get Deathmatch Kill Limit` | 勝利キル数(「あと何キル」の表示に) |
| `Get Team Kills` | チームの合計キル(チーム戦のスコア表示に) |
| `Get Players By Kills` | 全員をキル数の多い順に(同じならデスの少ない順)。順位表に |
| `Get Phase Time Remaining` | 残り時間 |
| **結果(試合が終わってから)** | |
| `Get Deathmatch Winners` | 勝者の PlayerState(個人戦: 1 位の人 / チーム戦: 勝ったチームの全員) |
| `Get Deathmatch Winner Names` | 勝者の名前(上と同じ人) |
| `Get Deathmatch Winning Kills` | 勝者のキル数(チーム戦ならチームの合計) |
| `Get Match Winner` | チーム戦で勝ったチームの番号(個人戦・引き分けなら 255) |
| `Is Deathmatch Draw` | 引き分けか |

結果画面の出し分けの例:

```
Is Deathmatch Draw → True: 「引き分け」
                   → False: Is Deathmatch Team Match → True: 「チーム {Get Match Winner + 1} の勝ち」
                                                    → False: 「{Get Deathmatch Winner Names[0]} の勝ち」
順位表: Get Players By Kills → For Each → 名前・Get Kill Count・Get Death Count
```

## デバッグ表示

画面左上に黄色の文字で出る(`ASavaGameState` の `bShowDebugText`。Shipping では出ない)。

```
[DM] FFA  InRound  245s  goal 10 kills
  Taro: 3 / 1
  Hanako: 2 / 2
```

チーム戦ではチームごとの合計キルと、各自のチーム番号も出る。試合が終わると勝者(または Draw)が出る。

## マップのセットアップ

1. `PlayerStart` または `SavaTeamPlayerStart` を置く。チーム戦もするなら `SavaTeamPlayerStart` を各チーム 3 個以上(CTF 用のマップならそのまま使える)
2. 個人戦では散らばっているほうがよいので、各チームの地点がマップの両端に固まっているなら、普通の `PlayerStart` を中央付近に足す
3. **Project Settings → Game → Sava Lobby → Modes** の Deathmatch の `Maps` にマップを追加する

## テスト

`Sava.Deathmatch.*`(Session Frontend → Automation、またはコマンドラインの `Automation RunTests Sava.Deathmatch`)

- 個人戦の勝者・同率の引き分け
- チーム戦の勝者(合計キルで決まる)・同数の引き分け
- ルールの値・キル数の順位
- スポーン地点の選び方(敵から遠い・使用中を避ける・幅を持たせる)

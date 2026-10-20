# オンラインロビーの BP の作り方

オンラインロビーのボタン・案内・ルーム設定の画面・ロビーの HUD を、Blueprint(BP)で組む手順。
C++ 側(ボタンの動き・サーバーへの頼み・ルールの確認)はできているので、BP では **見た目を作り、C++ の関数を呼ぶだけ** でよい。
仕組みの全体は [OnlineLobby.md](OnlineLobby.md)。

---

## 0. 先に知っておくこと(Unity との対応)

| UE | Unity でいうと |
|---|---|
| Blueprint Class(BP) | Prefab + そのスクリプト |
| 親クラス(Parent Class) | 継承元の C# クラス。C++ で作った `SavaInteractButton` などを親にすると、その機能がそのまま使える |
| Components パネル | Prefab の子オブジェクト・コンポーネント |
| Details パネル | Inspector |
| Construction Script | エディタ上で値を変えるたびに走る処理(`OnValidate` に近い) |
| Event Graph | スクリプト本体(`Start` や `Update`、イベント処理を書く場所) |
| Widget Blueprint(WBP) | UI の Prefab(Canvas の中身)+ そのスクリプト |
| Designer タブ / Graph タブ | Widget の見た目を置く画面 / スクリプトを書く画面 |
| Event Construct | Widget の `Start` |
| Create Widget → Add to Viewport | `Instantiate` して画面に出す |
| Cast To 〇〇 | `as 〇〇` / `GetComponent<〇〇>()`(型を確かめて変換する。失敗したら Cast Failed 側へ進む) |
| Event Dispatcher(青い「On 〜」) | C# の `event` / `UnityEvent`。`Bind Event to 〜` が `+=` |
| Custom Event(赤いイベント) | 自分で作るメソッド。Bind Event の赤い線をつなぐ相手にもなる |

### よく使う操作

- **ノードを置く**: Graph の何もない所を右クリック → 名前で検索
  - 日本語表示のエディタでも、英語名で検索して出ることが多い。出なければ日本語名(例: Create Widget = ウィジェットを作成)で探す
- **C++ の関数を探す**: 右クリックの検索欄に `Server Join Team` などとそのまま打つ。この手順書に出てくる `Sava` 系の関数は、どれも英語名のまま出る
- **ピンから伸ばす**: 青いピン(オブジェクト)をドラッグして離すと、その型で使えるノードだけが出る。迷ったらこれが一番早い
- **変数にする**: Designer で置いた部品(テキストやボタン)は、Details の一番上の **Is Variable** にチェックすると Graph から使える
- **Widget の親クラスを変える**: Graph タブ → 上の **Class Settings** → Details の **Parent Class**

---

## 1. ボタン `BP_LobbyButton`(1 つ作って、置いた先ごとに役割を変える)

チーム切替・準備 OK・退出・ゲーム開始は、すべて **同じ BP** で作れる。置いたものごとに Details の **Action** を変えればよい。

### 1-1. 作る

1. Content Browser で `Content/Lobby` を開く
2. 何もない所を右クリック → **Blueprint Class**
3. 下の **All Classes** を開き、`SavaInteractButton` を検索して選ぶ → **Select**
4. 名前を `BP_LobbyButton` にする → ダブルクリックで開く

### 1-2. 見た目

1. 左上の **Components** パネルで **Mesh** を選ぶ(C++ で作ってあるので、最初からある)
2. 右の Details → **Static Mesh** に、ボタンらしいメッシュを入れる(仮なら `Cube` や `Cylinder`)。Scale で台の形にする
3. **InteractArea**(四角い枠)を選び、**Box Extent** で押せる範囲を決める(初期値は 150 × 150 × 100)
4. 文字を出す: Components の **+ Add** → **Text Render** を追加し、名前を `Label` にする。Mesh の上に来るように Location を調整する

### 1-3. 役割に合わせて文字を変える(Construction Script)

置いて Action を変えたとき、文字が自動で変わるようにする。

1. 上のタブの **Construction Script** を開く
2. 右クリック → `Get Action` を置く
3. `Get Action` の出力ピンをドラッグ → **Switch on ESavaInteractButtonAction** を置き、Construction Script の白い実行線をつなぐ
4. Switch の出口ごとに、Components から `Label` をドラッグして置き → そこから **Set Text** を置き、文字を入れる
   - Toggle Ready → `準備 OK`
   - Leave Room → `部屋を出る`
   - Start Match → `ゲーム開始`
   - Join Team → 下の 5 へ
5. Join Team は番号を入れる:
   - 右クリック → `Get Team Id` → ピンから **+**(Add)で `1` を足す(Team Id は 0 から始まるため)
   - 右クリック → **Format Text** を置き、Format に `チーム {0} に入る` と書く → 出てきた `{0}` のピンに、さっきの数をつなぐ
   - Format Text の結果を Set Text の Value へ
6. **Compile** → **Save**

### 1-4. 案内の Widget を入れる

先に「2. 案内の Widget」を作ってから戻ってくる。

1. 上のツールバーの **Class Defaults** を押す
2. Details の **Sava | Interact**:
   - **Prompt Widget Class** → `WBP_ButtonPrompt`
   - **Host Only Prompt Widget Class** → `WBP_HostOnlyPrompt`
   - **Host Only** はオフのまま(ゲーム開始は、これがオフでもいつもホスト限定になる)

### 1-5. 押したときの音など(任意)

1. Event Graph で右クリック → **On Pressed**(Sava | Button)
2. そこから **Play Sound 2D** などをつなぐ(押した本人の画面だけで鳴る)

※ チーム切替などの処理そのものは C++ がやるので、ここには何も書かなくてよい。

### 1-6. マップに置く

`OnlineLobby` を開き、Content Browser から `BP_LobbyButton` を 5 つドラッグして置く。それぞれを選んで Details の **Sava | Button**:

| 置いたもの | Action | Team Id |
|---|---|---|
| チーム 1 のボタン | Join Team | 0 |
| チーム 2 のボタン | Join Team | 1 |
| 準備 OK | Toggle Ready | - |
| 部屋を出る | Leave Room | - |
| ゲーム開始 | Start Match | - |

Team Id は Action を Join Team にしたときだけ表示される。

---

## 2. 案内の Widget(範囲に入ったときに出る文字)

### 2-1. `WBP_ButtonPrompt`(「しゃがみで押す」)

1. `Content/Lobby` で右クリック → **User Interface** → **Widget Blueprint** → **User Widget** を選ぶ → 名前 `WBP_ButtonPrompt`
2. 開いたら Designer タブ。左の **Palette** から **Canvas Panel** をドラッグして置く(既にあれば不要)
3. **Text** を Canvas の中へドラッグ
4. Text の Details:
   - **Anchors** を画面下中央にする(Anchors のプルダウンで下中央の絵)
   - **Alignment** を X 0.5、Y 1.0、**Position** を X 0、Y -150 にする(画面の下の方の真ん中)
   - **Size To Content** にチェック
   - **Text** に `[しゃがみ] で押す`、Font の Size を 24 くらい
5. **Compile** → **Save**

既存の `WBP_LoadoutPrompt` / `WBP_MatchingPrompt` と同じ作りなので、それを複製(右クリック → Duplicate)して文字を変えてもよい。

### 2-2. `WBP_HostOnlyPrompt`(「ホストのみ」)

`WBP_ButtonPrompt` を複製して、文字を `ホストのみ操作できます` に変える(色を灰色にすると分かりやすい)。

---

## 3. ルーム設定の端末

端末の BP `BP_RoomSettings`(親は `SavaInteractTerminal`)は既にある。設定するだけ。

1. `Content/Lobby/BP_RoomSettings` を開く → **Class Defaults**
2. Details:
   - **Sava | Interact**
     - **Host Only** にチェック
     - **Prompt Widget Class** → `WBP_ButtonPrompt`(端末用に文字を変えたものを作ってもよい)
     - **Host Only Prompt Widget Class** → `WBP_HostOnlyPrompt`
   - **Sava | Terminal** → **Menu Widget Class** → `WBP_RoomSettings`(次の 4 で作る)
3. Mesh に見た目を入れ、`OnlineLobby` に置く

---

## 4. ルーム設定の画面 `WBP_RoomSettings`

### 4-1. 作る(親を `SavaTerminalWidget` にする)

1. `Content/Lobby` で右クリック → **User Interface** → **Widget Blueprint**
2. 親を選ぶ画面で **All Classes** を開き、`SavaTerminalWidget` を検索して選ぶ → 名前 `WBP_RoomSettings`
   - 間違えて User Widget で作った場合は、Graph → **Class Settings** → **Parent Class** を `SavaTerminalWidget` に変える
   - 親を `SavaTerminalWidget` にすると、Esc キーで閉じる・`Close Terminal` で閉じる、が最初から使える

### 4-2. 見た目(Designer)

次の部品を置き、**名前を変えて Is Variable にチェック** する(名前は Details の一番上で変える)。

```
Canvas Panel
└ Border(背景。画面中央に大きめに)
  └ Vertical Box
    ├ Text「ルーム設定」
    ├ Horizontal Box: Text「モード」 + Combo Box (String)  … 名前 ModeCombo
    ├ Horizontal Box: Text「マップ」 + Combo Box (String)  … 名前 MapCombo
    ├ Horizontal Box(名前 RoundsRow): Text「勝利ラウンド数」 + Spin Box … 名前 RoundsSpin
    ├ Horizontal Box(名前 KillsRow): Text「勝利キル数」 + Spin Box … 名前 KillsSpin
    ├ Horizontal Box(名前 TeamRow): Text「チーム戦」 + Check Box … 名前 TeamCheck
    ├ Horizontal Box: Text「制限時間(秒)」 + Spin Box  … 名前 TimeSpin
    ├ Text「メンバー」
    ├ Scroll Box  … 名前 MemberList
    └ Button(中に Text「閉じる」) … 名前 CloseButton
```

Spin Box の Details:

- RoundsSpin: **Delta** 1、**Min Fractional Digits** 0、**Max Fractional Digits** 0、**Always Uses Delta Snap** にチェック
- KillsSpin: **Delta** 1、Fractional Digits は両方 0、**Always Uses Delta Snap** にチェック
- TimeSpin: **Delta** 10、Fractional Digits は両方 0、**Always Uses Delta Snap** にチェック
- 最小・最大は Graph で Project Settings の値を入れる(4-3 の手順 2)

### 4-3. 処理(Graph)

まず **変数を 1 つ作る**: 左の My Blueprint → Variables の **+** → 名前 `LobbyState`、型は **Sava Lobby Game State**(Object Reference)。

**① 開いたとき(Event Construct)**

1. **Event Construct** から:
2. 範囲を入れる: `Get Sava Lobby Settings` → ピンから `Min Rounds To Win` / `Max Rounds To Win` を取り出し、RoundsSpin の **Set Min Value** / **Set Max Value**(と **Set Min Slider Value** / **Set Max Slider Value**)へ。TimeSpin も `Min Round Time Limit` / `Max Round Time Limit`、KillsSpin も `Min Kill Limit` / `Max Kill Limit` で同じようにする
3. モードの一覧を入れる: ModeCombo → **Clear Options**。`Get Mode Options` → **For Each Loop** → Array Element から **Break SavaMatchModeOption** → `Display Name` を **To String (Text)** で文字列にして → ModeCombo の **Add Option**
4. GameState を覚える: **Get Game State** → **Cast To SavaLobbyGameState** → 成功したら **Set LobbyState**
5. 変わったら描き直すように登録する: LobbyState から **Bind Event to On Lobby Changed** を置く。赤い Event ピンをドラッグ → **Add Custom Event** → 名前 `Refresh`
6. 最後に `Refresh` を 1 回呼ぶ(開いた時点の内容を出すため)

**② 描き直し(Custom Event `Refresh`)**

ここで画面を「今の状態」に合わせる。

1. `LobbyState` → **Get Room Settings** → **Break SavaRoomSettings**(Mode Index・Map Index・Rounds To Win・Round Time Limit・Kill Limit・Team Match が出る)
2. ModeCombo → **Set Selected Index** ← Mode Index
3. マップの一覧を、選んでいるモードのものに入れ直す:
   - `Get Mode Option`(Mode Index を入れる)→ 出力の Out Mode を **Break SavaMatchModeOption**
   - MapCombo → **Clear Options** → `Maps` を **For Each Loop** → **Break SavaMatchMapOption** → Display Name → To String → MapCombo **Add Option**
   - ループの **Completed** から MapCombo **Set Selected Index** ← Map Index
4. ラウンド制のモードだけ、ラウンド数の行を出す: Break したモードの `Uses Rounds` → **Select**(Visible / Collapsed)→ RoundsRow の **Set Visibility**
   - 同じように `Uses Kill Limit` → KillsRow、`Can Choose Team Match` → TeamRow の **Set Visibility**
5. RoundsSpin **Set Value** ← Rounds To Win、TimeSpin **Set Value** ← Round Time Limit、KillsSpin **Set Value** ← Kill Limit、TeamCheck **Set Is Checked** ← Team Match
6. メンバー一覧: MemberList → **Clear Children** → `LobbyState` → **Get Lobby Players** → **For Each Loop** → **Create Widget**(Class = `WBP_MemberRow`、Owning Player = **Get Owning Player**、Player ピン = Array Element)→ MemberList の **Add Child**
   - `WBP_MemberRow` は 4-4 で作る。作ると Create Widget に Player ピンが出る

**③ 選んだら送る**

ホストが値を変えたら、すぐサーバーへ送る。送った結果は On Lobby Changed で全員の画面に戻ってくる(自分の画面も Refresh で描き直される)。

1. 関数を 1 つ作る: My Blueprint → Functions の **+** → 名前 `SendSettings`
   - 中身: **Make SavaRoomSettings** に Mode Index ← ModeCombo **Get Selected Index**、Map Index ← MapCombo **Get Selected Index**、Rounds To Win ← RoundsSpin **Get Value** を **Round**、Round Time Limit ← TimeSpin **Get Value** を **Round**、Kill Limit ← KillsSpin **Get Value** を **Round**、Team Match ← TeamCheck **Is Checked**
   - Make に入れなかった項目は 0 / false で送られてしまうので、**全部の項目をつなぐ**こと
   - **Get Owning Player** → **Cast To SavaPlayerController** → **Server Update Room Settings**(New Settings ← Make の結果)
2. ModeCombo を選び、Details の一番下の **Events** → **On Selection Changed** の **+** を押す
   - 出てきたイベントの **Selection Type** を **==**(Equal)で **Direct** と比べ、**Branch** の **False** の時だけ `SendSettings` を呼ぶ
   - 理由: Refresh の中で Set Selected Index したときも、このイベントが Direct で来る。それで送り返すと、送信が止まらなくなる
3. MapCombo も同じようにする
4. RoundsSpin・TimeSpin・KillsSpin は **On Value Committed**(値を確定したとき)の **+** → `SendSettings` を呼ぶ
5. TeamCheck は **On Check State Changed** の **+** → `SendSettings` を呼ぶ
   - Refresh の Set Is Checked ではこのイベントは来ない(ユーザーがクリックしたときだけ)ので、ModeCombo のような判定は要らない

**④ 閉じる**

CloseButton の **On Clicked** の **+** → **Close Terminal**

### 4-4. メンバーの 1 行 `WBP_MemberRow`

1. Widget Blueprint(User Widget)を作る → 名前 `WBP_MemberRow`
2. 変数 `Player` を作る。型は **Sava Player State**(Object Reference)で、Details の **Instance Editable** と **Expose on Spawn** にチェック(これで Create Widget に Player ピンが出る)
3. Designer:

```
Horizontal Box
├ Text … 名前 NameText(Size を Fill にすると横に広がる)
├ Text … 名前 TeamText
├ Button(中に Text「チーム移動」) … 名前 MoveButton
└ Button(中に Text「キック」) … 名前 KickButton
```

4. Graph:
   - **Event Construct**:
     - NameText **Set Text** ← `Player` → **Get Player Name**(String を Text につなぐと自動で変換される)。`Player` → **Is Room Host** が true なら、名前の後ろに「(ホスト)」を付けてもよい(**Append** で文字をつなぐ)
     - TeamText **Set Text** ← `Player` → **Get Team Id** に 1 を足した数(Format Text `チーム {0}`)
     - ホストの行では操作ボタンを隠す: `Player` → **Is Room Host** → Select(Collapsed / Visible)→ MoveButton と KickButton の **Set Visibility**
   - MoveButton **On Clicked**:
     - 移る先 = 1 − 今のチーム(チームが 2 つなので、0 なら 1、1 なら 0)。`Player` → Get Team Id → **1 −**(Subtract)
     - **Get Owning Player** → Cast To SavaPlayerController → **Server Move Player To Team**(Target ← `Player`、Team Id ← 計算した値)
   - KickButton **On Clicked**: Get Owning Player → Cast To SavaPlayerController → **Server Kick Player**(Target ← `Player`)

   - **個人戦ではチームを隠す**: Event Construct で **Get Game State** → Cast To SavaLobbyGameState → **Is Team Match** → Select(Visible / Collapsed)→ TeamText と MoveButton の **Set Visibility**
     - MoveButton はホストの行でも隠すので、「ホストではない」**AND**「Is Team Match」のときだけ Visible にする
     - 行は Refresh のたびに作り直されるので、ルーム設定を切り替えればすぐ反映される
     - 個人戦でチームを移そうとしても、サーバーが「個人戦ではチームを選べません」で断る

この画面はホストしか開けない(端末が Host Only)ので、ボタンが参加者に見えることはない。
仮に参加者が押せたとしても、サーバー側で断られる。

---

## 5. ロビーの HUD `WBP_LobbyHUD`(部屋にいる間ずっと出す)

チームごとの名前・準備 OK・ルーム設定・開始できるか・断られた理由を出す。

### 5-1. 見た目(Designer)

```
Canvas Panel
├ Vertical Box(画面左上)
│ ├ Text … 名前 SettingsText(「CTF / FPSMap / 2 ラウンド先取 / 180 秒」)
│ ├ Text「チーム 1」 … 名前 Team1Header
│ ├ Vertical Box … 名前 Team1List
│ ├ Text「チーム 2」 … 名前 Team2Header
│ ├ Vertical Box … 名前 Team2List
│ └ Text … 名前 StartText(「開始できます」/ 開始できない理由)
└ Text … 名前 MessageText(画面中央上。最初は Visibility を Hidden)
```

### 5-2. 1 人分の行 `WBP_LobbyPlayerRow`

`WBP_MemberRow` と同じ作り方で、変数 `Player`(Sava Player State、Expose on Spawn)と Text を 1 つ(名前 `RowText`)置く。
Event Construct で、名前 + 印を出す:

- `Player` → Get Player Name
- Is Room Host なら「 ★」、Is Ready なら「 ✔ 準備 OK」を **Append** で後ろに付ける
- RowText **Set Text**

### 5-3. 処理(Graph)

変数 `LobbyState`(Sava Lobby Game State)を作る(4-3 と同じ)。

1. **Event Construct**:
   - Get Game State → Cast To SavaLobbyGameState → Set LobbyState
   - LobbyState → **Bind Event to On Lobby Changed** → Custom Event `Refresh`
   - **Get Owning Player** → Cast To SavaPlayerController → **Bind Event to On Lobby Message** → Custom Event `ShowMessage`(Message ピンが付いてくる)
   - **Set Timer by Event**(Time 1.0、Looping にチェック)→ Event に `Refresh` をつなぐ
     - 理由: 名前は少し遅れて届くことがあり、On Lobby Changed だけだと最初の名前が空のまま残ることがあるため。1 秒ごとの描き直しは保険
   - 最後に `Refresh` を呼ぶ
2. **Custom Event `Refresh`**:
   - ルーム設定: LobbyState → Get Room Settings → **Get Room Settings Display Names**(Mode Name・Map Name が出る)。Break SavaRoomSettings で Rounds To Win・Round Time Limit も取り、Format Text `{mode} / {map} / {rounds} ラウンド先取 / {time} 秒` → SettingsText **Set Text**
   - チーム 1: Team1List **Clear Children** → LobbyState **Get Team Players**(Team Id 0)→ For Each Loop → Create Widget `WBP_LobbyPlayerRow`(Player ← Array Element)→ Team1List **Add Child**
   - チーム 2: 同じく Team Id 1 で Team2List へ
   - **個人戦のとき**: LobbyState → **Is Team Match** で **Branch**
     - True: 上の 2 つ(チームごとの一覧)をそのまま行う。見出し「チーム 1」「チーム 2」と Team2List は Visible
     - False: Team1List に **Get Lobby Players**(全員)を入れ、Team2List は Clear Children だけ。見出し「チーム 1」を「メンバー」に Set Text し、「チーム 2」の見出しと Team2List は Collapsed
     - 見出しの Text も Is Variable にして名前を付けておく(例: Team1Header・Team2Header)
   - 開始できるか: LobbyState → **Can Start Match** → 戻り値で **Branch**
     - True: StartText に「開始できます」
     - False: StartText に Out Reason
3. **Custom Event `ShowMessage`**:
   - MessageText **Set Text** ← Message、**Set Visibility** Visible
   - **Set Timer by Event**(Time 3.0、Looping なし)→ Custom Event `HideMessage` → MessageText Set Visibility Hidden

### 5-4. 画面に出す(OnlineLobby のレベル BP)

1. `OnlineLobby` を開く → 上のツールバーの Blueprint のアイコン → **Open Level Blueprint**
2. **Event BeginPlay** → **Create Widget**(Class = `WBP_LobbyHUD`、Owning Player = **Get Player Controller**(Player Index 0))→ **Add to Viewport**

レベル BP はホストでも参加者でも、それぞれの PC で動く。なので全員に自分用の HUD が出る。

---

## 6. 確認

1. Play ボタン横の **︙** → **Net Mode** を **Play Standalone**、**Number of Players** を 2 にする
2. 再生 → 1 つ目のウィンドウでタイトル → ソロロビー → マッチ端末で部屋を作る
3. 2 つ目のウィンドウで部屋を探して入る
4. 確かめること:
   - ロビー HUD に 2 人が出ている。ホストに ★ が付いている
   - チームのボタンでチームが変わり、満員なら断られた理由が出る
   - 参加者が準備 OK を押すと ✔ が付く
   - 参加者がゲーム開始・ルーム設定に近づくと「ホストのみ」になる
   - ホストがルーム設定を変えると、参加者の HUD の表示も変わる
   - 2 人で CTF を試すときは、ホストのウィンドウで ` キー → `Sava.Lobby.MinPlayersOverride 2`
   - ホストがゲーム開始 → 2 人とも FPSMap へ移り、カウントダウンが始まる
5. うまく動かないとき:
   - ` キー → `Sava.LobbyStatus` で、サーバーから見た状態がログ(Window → Output Log)に出る
   - `Sava.LocalStatus` で、自分の PlayerController と、表示中の画面が出る
   - BP の Cast が失敗していないかは、Cast Failed のピンに **Print String** をつなぐと分かる

---

## つまずきやすいところ

- **Cast To の青い入力ピンが空**: Get Game State / Get Owning Player などをつなぎ忘れている
- **Bind Event がつながらない**: 赤い四角(Event)ピンには、**Custom Event** か、同じ引数を持つイベントしかつなげない。ピンからドラッグして **Add Custom Event** を選ぶと、引数が合ったものができる
- **Create Widget に Player ピンが出ない**: 行の Widget の `Player` 変数で **Expose on Spawn** と **Instance Editable** にチェックし、Compile する
- **ルーム設定を変えると止まらなくなる**: On Selection Changed で Selection Type が Direct のものまで送っている(4-3 ③)
- **案内が出ない**: BP の Class Defaults で Prompt Widget Class が空のまま。または InteractArea が小さすぎる
- **しゃがみキーで反応しない**: ホスト限定のものに参加者が近づいている(仕様。普通にしゃがむ)

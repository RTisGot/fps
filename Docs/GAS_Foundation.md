# 能力(スキル・ガジェット・武器)の土台 ガイド

スキル・ガジェット・武器の「ボタンを押したら何かが起きる」処理は、すべて UE の **Gameplay Ability System(GAS)** の上で作ります。
通信・クールダウン・ダメージ計算などの難しい部分は C++ の土台として用意してあるので、各担当は **Blueprint とデータアセットだけで** 能力を作れます。

> このガイドは「どう作るか」の説明です。どんなスキル・ガジェットを作るかは、まだ決まっていません。
> 例として出てくる能力は、すべて説明用の架空のものです。
>
> 決まっていること: **スキルはクールダウン、ガジェットは個数制(リスポーンで補充)。ガジェットは 1 人 1 つ**(5-6 を参照)
> 武器の作り方は [Weapon_Guide.md](Weapon_Guide.md) を見てください。

---

## 0. 最初に覚える 3 つの言葉

| 言葉 | 一言でいうと | 例 | アセット名の頭 |
|---|---|---|---|
| **Ability(アビリティ)** | ボタンを押したら何が起きるか(処理) | 「押すと 5 秒間速くなる」「長押しで狙って、離すと物を出す」 | `GA_` |
| **Effect(エフェクト)** | 数値や状態がどう変わるか(データ) | 「5 秒間、移動速度 +20%」「HP を 30 回復」 | `GE_` |
| **Tag(タグ)** | 状態や種類を表すラベル | `State.Dead`、`Cooldown.Skill.<能力名>` | ー |

- Ability は **処理**(Blueprint のイベントグラフを書く)
- Effect は **データ**(数値を入力するだけ。グラフは書かない)
- Tag は **名札**。`State.Dead` のように `.` で階層になっていて、「この名札を持っている間は○○できない」といった判定に使います

### Unity 経験者向けの対応表

| UE | Unity でいうと |
|---|---|
| Ability(`GA_`) | ボタンで起動するコルーチン付きのスクリプト |
| Effect(`GE_`) | バフ・デバフを表す ScriptableObject |
| Tag | enum の代わりに使う、階層つきの文字列ラベル |
| データアセット(`DA_`) | ScriptableObject |
| Ability Task(`Wait Delay` など) | コルーチンの `yield return` |

---

## 1. 全体像

```
PlayerState(プレイヤーごとの情報。キャラが死んで作り直されても残る)
 ├─ AbilitySystemComponent(ASC) … 能力・効果・タグをまとめて管理する本体
 ├─ AttributeSet …………………… Health / MaxHealth / MoveSpeedMultiplier / GadgetCharges / MaxGadgetCharges(数値)
 └─ TeamId ……………………………… 0 か 1(参加時に人数の少ないチームへ自動で入る)

Character(プレイヤーの体)
 ├─ Ability Sets ……………… 持たせる能力の一覧(データアセット)
 ├─ Ability Input Config … ボタン → InputTag の対応表(データアセット)
 └─ CharacterMovement ……… MoveSpeedMultiplier を歩き・ダッシュの速度に掛ける
```

### ボタンを押してから能力が動くまで

```
キーを押す
  → 入力アクション(IA_)
  → Ability Input Config で InputTag に変換(例: InputTag.Skill)
  → ASC が「同じ InputTag を持っている能力」を探して発動
  → 能力(GA_)の Event ActivateAbility が動く
```

ボタンと能力を直接つながず、間に **InputTag** を挟んでいます。
こうしておくと、能力を入れ替えてもボタンの設定を触らなくて済みます。

### なぜ ASC を PlayerState に置いているのか

Character は死ぬと消えて作り直されます。Character に ASC を置くと、死ぬたびにクールダウンが消えてしまいます。
PlayerState はゲームの間ずっと残るので、**リスポーンしてもクールダウンが続きます**。

---

## 2. 通信対戦で必ず守ること(全員必読)

このゲームは 3v3 のオンライン対戦です。UE では **サーバーの結果が正しい** というルールで動いています。

| やりたいこと | どこで行うか | 理由 |
|---|---|---|
| ダメージ・回復・効果を与える | **サーバーだけ** | クライアントが勝手に HP を減らせるとチートになる |
| 物(Actor)を出す(Spawn) | **サーバーだけ** | クライアントでも出すと 2 個になる |
| 見た目・音を出す | 全員の画面 | ー |

- 土台の関数(`Apply Damage` など)は、クライアントで呼んでも **何も起きません**(安全のため)
- 能力の中で物を出すときは、**`Has Authority` が true のときだけ** `Spawn Actor from Class` を呼びます
- 能力そのものは「押した瞬間に自分の画面で先に動き、あとでサーバーが確認する」(**予測**)ようになっています。操作が遅れて感じないための仕組みで、設定は不要です

---

## 3. C++ の土台クラス一覧(`Source/sava`)

| クラス | 役割 | 各担当は |
|---|---|---|
| `SavaGameplayAbility` | すべての能力の親。予測・クールダウン・死亡中ブロックが入っている | **親にする** |
| `SavaHoldAimAbility` | 「長押しで狙って(予測線を出して)、離して確定」する能力の親 | **親にする** |
| `SavaAbilitySet` | キャラに持たせる能力のリスト(データアセット) | **データを作る** |
| `SavaInputConfig` | 入力アクション → InputTag の対応表(データアセット) | データを作る |
| `SavaAbilitySystemLibrary` | ダメージ・回復・効果・チーム判定の共通関数 | **呼ぶ** |
| `SavaAttributeSet` | HP・移動速度倍率などの数値 | 触らない(読むだけ) |
| `SavaGameplayEffects` | ダメージ・回復・クールダウンの共通 Effect | 触らない(ライブラリ経由で自動) |
| `SavaAbilitySystemComponent` | ASC。InputTag で能力を探して発動する | 触らない |
| `SavaPlayerState` | ASC・AttributeSet・チームを持つ | 触らない |
| `SavaGameplayTags` | C++ から使うタグの一覧 | 追加は相談 |

---

## 4. 最初に 1 回だけ必要な準備

まだ作っていないので、能力を初めて動かす人が(1 人だけ)行ってください。

1. **入力アクションを作る**
   コンテンツブラウザで右クリック → Input → Input Action。能力用のボタンの数だけ作ります(例: `IA_Skill`)。
2. **既存の Input Mapping Context(IMC)に追加する**
   キャラクターが使っている IMC を開き、1 で作った IA にキーを割り当てます。
3. **入力の対応表を作る**
   右クリック → Miscellaneous → Data Asset → **SavaInputConfig** を選び、`DA_InputConfig` という名前にします。
   **Ability Input Actions** に「Input Action(IA_Skill)」と「Input Tag(InputTag.Skill)」の組を追加します。
4. **共通の能力セットを作る**
   右クリック → Miscellaneous → Data Asset → **SavaAbilitySet** を選び、`DA_AbilitySet_Default` という名前にします。
5. **キャラクターに設定する**
   `BP_FirstPersonCharacter` を開き、Class Defaults の **Sava | Abilities** にある
   - **Ability Sets** に `DA_AbilitySet_Default`
   - **Ability Input Config** に `DA_InputConfig`

   を設定します。

> `BP_FirstPersonCharacter` の親クラスが `savaCharacter` になっていることを確認してください(Class Settings → Parent Class)。
> 違うと、能力もスライディングも動きません。

---

## 5. 能力の作り方(基本)

### 5-1. Blueprint を作る

1. コンテンツブラウザで右クリック → Blueprint Class → All Classes から **SavaGameplayAbility** を選ぶ
2. 名前は `GA_<種類>_<能力名>`(例: `GA_Skill_<能力名>`)

### 5-2. Class Defaults で設定する

| 項目 | 何を入れるか |
|---|---|
| **Activation Policy** | いつ発動するか(下の表) |
| **Cooldown Duration** | クールダウンの秒数。0 ならクールダウンなし |
| **Cooldown Tags** | `Cooldown.<種類>.<能力名>`。**能力ごとに別のタグにする**(同じタグだと、片方を使うともう片方もクールダウンになる) |
| **Max Charges** | ガジェットの個数。0 なら個数制なし(スキル・武器は 0 のまま)。5-6 を参照 |
| **Asset Tags**(古い資料では Ability Tags) | 能力の種類。`Ability.Type.Skill` / `Ability.Type.Gadget` / `Ability.Type.Weapon` のどれか |
| **Block Abilities with Tag** | この能力の使用中に、使えなくする能力の種類(例: 使用中は `Ability.Type.Weapon` を撃てない) |
| **Activation Blocked Tags** | このタグを持っている間は発動できない。`State.Dead` は最初から入っている |

**Activation Policy の選び方**

| 値 | 動き | 向いているもの |
|---|---|---|
| `On Input Triggered` | 押したら発動。終わりは自分で `End Ability` を呼ぶ | ほとんどの能力 |
| `While Input Active` | 押している間だけ。離すと自動で終わる | 押しっぱなしで効き続けるもの |
| `On Spawn` | キャラに持たせた瞬間に自動で発動 | 常にかかっている効果(パッシブ) |

### 5-3. Event Graph を書く

```
Event ActivateAbility
  → Commit Ability     … クールダウン開始・個数を 1 減らす。false が返ったら End Ability して終わる
  → やりたいこと        … Effect を自分に付ける、物を出す など
  → End Ability
```

**よくある失敗**
- **`End Ability` を呼び忘れる** → その能力は二度と発動できなくなります。途中で失敗したときも必ず呼んでください
- **`Delay` ノードや Timer を使う** → 能力が終わったあとも動き続けてしまいます。代わりに **Ability Task** のノードを使います
  - `Wait Delay`(○秒待つ)
  - `Play Montage and Wait`(アニメーションが終わるまで待つ)
  - `Wait Input Release`(ボタンを離すまで待つ)
- **`Commit Ability` の前に効果を出す** → クールダウン中でも効果が出てしまいます。先に Commit します

### 5-4. キャラクターに持たせる

`DA_AbilitySet_Default` を開き、**Granted Gameplay Abilities** に追加します。

| 項目 | 入れるもの |
|---|---|
| Ability | 作った `GA_` |
| Ability Level | 基本は 1 |
| Input Tag | 発動するボタンのタグ(例: `InputTag.Skill`)。パッシブなら空でよい |

> 同じ InputTag を持つ能力が複数あると、ボタン 1 つで全部が発動します。1 つのボタンに 1 つの能力にしてください。

### 5-5. 例: 「押すと一定時間速くなる」能力

1. **Effect を作る**: 右クリック → Blueprint Class → **GameplayEffect**、名前は `GE_Skill_<能力名>`
   - Duration Policy: `Has Duration`、Duration Magnitude: `5`(秒)
   - Modifiers に 1 つ追加: Attribute = `SavaAttributeSet.MoveSpeedMultiplier`、Modifier Op = `Add`、Magnitude = `0.2`
2. **Ability を作る**: `GA_Skill_<能力名>`(親 SavaGameplayAbility)
   ```
   Event ActivateAbility → Commit Ability → Apply Gameplay Effect to Owner(GE_Skill_<能力名>) → End Ability
   ```
3. Class Defaults で Cooldown Duration と Cooldown Tags を設定
4. `DA_AbilitySet_Default` に追加して、Input Tag を選ぶ

`MoveSpeedMultiplier` は移動コンポーネントが歩き・ダッシュの速度に掛けているので、C++ を触らずに速度が変わります。
減速させたい場合は `Add` に `-0.3` のようなマイナスの値を入れます(0 より下にはなりません)。

### 5-6. スキルはクールダウン、ガジェットは個数

| 種類 | 使える回数の決まり方 | Class Defaults の設定 |
|---|---|---|
| スキル | 使うと一定時間使えなくなる(クールダウン) | **Cooldown Duration** と **Cooldown Tags** を入れる。Max Charges は 0 のまま |
| ガジェット | 決まった個数だけ使える。**リスポーンすると満タンに戻る** | **Max Charges** に個数を入れる(例: 2)。クールダウンは入れなくてよい |

ガジェットの個数の仕組み:
- 残り個数は `SavaAttributeSet.GadgetCharges`、最大個数は `SavaAttributeSet.MaxGadgetCharges` に入っています
- **`Commit Ability` を呼んだときに 1 減ります**。残りが 0 のときは、ボタンを押しても発動しません
- 自分の画面ではすぐ減り、サーバーの値で確定します(予測)
- 長押しの能力(6 章)なら **投げた・置いたときだけ** 減ります。キャンセルしたら減りません
- 能力がキャラクターに付与されたとき(= リスポーン時)に、Max Charges の値で満タンになります
- 残り個数は **本人にだけ** 同期されます(相手には見えない)

**注意**
- **個数制の能力(Max Charges が 1 以上)は、1 人 1 つだけ** にしてください。残り個数の数値は 1 つしかないので、2 つあると個数を共有してしまいます
- 個数を UI に出すときは、`SavaAttributeSet.GadgetCharges` の値を読みます(値が変わったときに更新するなら、`Wait for Attribute Changed` ノードが使えます)
- 個数を途中で増やしたい場合(拾うと 1 個増える など)は、`GadgetCharges` に `Add 1` する Effect を作って付けます。最大個数を超えることはありません

---

## 6. 長押しで狙う能力(予測線・プレビュー付き)

「長押ししている間は狙っている場所(や飛んでいく線)を表示して、離したら確定する」能力は、親を **SavaHoldAimAbility** にします。
狙った位置をサーバーへ送る・サーバーで確認する処理は、この親クラスがやってくれます。

### 6-1. 流れ

```
押す     → On Aim Started      (自分の画面だけ)プレビューを出す
押し中   → On Aim Updated      (自分の画面だけ・毎フレーム)プレビューを動かす
離す     → 位置をサーバーへ送る → サーバーが距離などを確認
         → On Confirmed        (サーバーだけ)ここで物を出す・効果を与える
終わり   → On Aim Ended        (自分の画面だけ)プレビューを消す
```

- **`Event ActivateAbility` は使わないでください。** 上の 4 つのイベントだけを実装します
- クールダウン・個数は **確定したときに** 消費します。キャンセルしたら消費しません
- キャンセルになるのは次のとき
  - **Cancel Input Tag** に設定したボタンを押した(例: `InputTag.Weapon.Aim` にすると右クリックでやめられる)
  - **Cancel On Tags Added** のタグが付いた(最初は `State.Dead`)
  - 置けない場所を狙ったまま離した

### 6-2. イベントの中身

| イベント | 動く場所 | 書くこと |
|---|---|---|
| On Aim Started | 自分の画面 | プレビュー用の Actor や線を出す |
| On Aim Updated | 自分の画面 | `Aim Transform` の位置へプレビューを動かす。`Is Valid` が false なら赤くするなど。放物線モードでは `Path Points` が予測線の点 |
| On Aim Ended | 自分の画面 | プレビューを消す(`Confirmed` = 確定したか) |
| On Confirmed | **サーバー** | `Spawn Actor from Class` など。Instigator には `Get Avatar Actor from Actor Info` を入れる。**サーバーでしか呼ばれないので Has Authority は不要** |
| Is Target Allowed(上書きは任意) | 両方 | 独自の条件。false を返すと置けない扱いになる |

### 6-3. 設定項目

| 項目 | 意味 |
|---|---|
| **Aim Mode** | `Point On Surface` = 視線の先の床・壁 / `Projectile Arc` = 投げたときの放物線 |
| Max Range | Point On Surface: 狙える最大距離 |
| Max Surface Angle | Point On Surface: 置ける面の傾き(0 = 平らな床だけ、90 = 壁にも置ける) |
| Launch Speed | Projectile Arc: 投げる速さ。**出す物の Projectile Movement の Initial Speed と同じ値にする**(違うと線と実際の飛び方がずれる) |
| Launch Offset | Projectile Arc: 視点から見た投げる位置のずれ(前・右・上) |
| Projectile Radius | Projectile Arc: 予測線の当たり判定の太さ。出す物の当たり判定と合わせる |
| Server Distance Tolerance | サーバーの確認で許す位置のずれ(通信の遅れの分) |

---

## 7. ダメージ・回復・効果(全員必読)

| やりたいこと | 使うノード(カテゴリ `Sava`) |
|---|---|
| ダメージを与える | **Apply Damage**(Damage Instigator = 攻撃した人のキャラ、Damage Causer = 弾など実際に当たった物) |
| 回復する | **Apply Healing** |
| Effect を相手に付ける | **Apply Effect To Target** |
| 敵かどうか | **Are Enemies**(同じプレイヤーの物同士・同じチームなら false) |
| チーム番号 | **Get Team Id**(キャラ・PlayerState・その人が出した物のどれでも使える。未所属は 255) |

**ルール**
- ダメージ・回復は **必ずこの関数を通す**。計算を 1 か所にまとめるためです
- **Health を直接 Set しない**。範囲チェックや、HP が 0 になったときの通知が動かなくなります
- どれも **サーバーでだけ** 動きます
- `Apply Damage` は、ASC を持たない相手(ドアや箱など)には何もせず false を返します

### 状態(〇〇中)を作りたいとき

「一定時間、能力が使えなくなる状態」などは、C++ を変えずに作れます。

1. Project Settings → GameplayTags でタグを追加する(例: `State.<状態名>`)。**State のタグは追加前に相談**
2. Effect を作り、Components に **Grant Tags to Target Actor** を追加して、そのタグを入れる
3. 封じたい能力の **Activation Blocked Tags** にそのタグを入れる(長押しの能力なら **Cancel On Tags Added** にも)
4. 相手には `Apply Effect To Target` で付ける

---

## 8. 移動に関わる能力

- キャラの位置を直接書き換える(`Set Actor Location` / `Teleport`)と、通信対戦ではサーバーに引き戻されます
- まずは **Root Motion 系の Ability Task**(`Apply Root Motion Move To Force` など)を使ってください。移動コンポーネントと連携して、予測もされます
- 速さを変えるだけなら、5-5 のように `MoveSpeedMultiplier` を使います
- それでも表現できない独自の動きは、キャラクター操作担当が `SavaCharacterMovementComponent` に移動モードを追加します(スライディングと同じ方法)。**先に相談してください**

---

## 9. タグの命名ルール

| 分類 | 形式 | 例 | 誰が追加するか |
|---|---|---|---|
| 入力 | `InputTag.<種類>` | `InputTag.Skill`、`InputTag.Weapon.Fire` | 土台(C++) |
| 能力の種類 | `Ability.Type.<種類>` | `Ability.Type.Gadget` | 土台(C++) |
| 状態 | `State.<状態>` | `State.Dead` | 相談して追加 |
| クールダウン | `Cooldown.<種類>.<能力名>` | `Cooldown.Skill.<能力名>` | 各担当 |
| 見た目・音 | `GameplayCue.<種類>.<能力名>` | `GameplayCue.Gadget.<能力名>.<場面>` | 各担当 |

- 今 C++ にあるタグ: `InputTag.Skill / Gadget / Weapon.Fire / Weapon.Aim / Weapon.Reload`、`Ability.Type.Skill / Gadget / Weapon`、`State.Dead`、`Cooldown`、`SetByCaller.Damage / Healing / Cooldown`
- 各担当のタグは Project Settings → GameplayTags から追加します(`Config/DefaultGameplayTags.ini` に保存される)
- 「Skill」「Gadget」という分類は仮のものです。能力の分け方が決まったら見直します

## 10. アセットの命名ルール

| 種類 | 頭 | 例 |
|---|---|---|
| Ability | `GA_` | `GA_Skill_<能力名>` |
| GameplayEffect | `GE_` | `GE_Skill_<能力名>` |
| 能力セット | `DA_AbilitySet_` | `DA_AbilitySet_Default` |
| 入力の対応表 | `DA_InputConfig` | ー |
| 入力アクション | `IA_` | `IA_Skill` |
| 能力が出す物 | `BP_` | `BP_<種類>_<能力名>` |

---

## 11. 作業ルールと困ったとき

- **プルして `Source/` に変更があったら、エディタを開く前にビルドする**(古いまま Blueprint を保存すると壊れることがある)
- **能力が発動しない**: プレイ中にコンソールを開き(日本語キーボードでは `@` キー、英語キーボードでは `` ` `` キー)、`showdebug abilitysystem` と入力する。持っている能力・タグ・効果が画面に出る
  - 能力が一覧にない → 能力セットに入っていない / キャラの Ability Sets が未設定
  - 一覧にあるが動かない → Input Tag の違い、クールダウン中、Activation Blocked Tags のタグを持っている
- **一度しか使えない**: `End Ability` を呼んでいない
- **通信のテスト**: Play の設定で Net Mode = **Play As Listen Server**、Number of Players = **2 以上** にする。1 人だけでは通信の問題は見つかりません

---

## 12. まだないもの(今後)

| 内容 | 状態 |
|---|---|
| 死亡処理(HP 0 → `State.Dead` を付ける → リスポーン) | HP 0 の通知(`OnOutOfHealth`)だけある |
| キャラごとの初期ステータス(最大 HP など) | 今は全員 HP 100。能力セットの Granted Gameplay Effects で上書きできる |
| 能力用の入力アクション・`DA_InputConfig`・`DA_AbilitySet_Default` | 4 章の手順で作る |
| HP・クールダウン・ガジェットの個数の UI | 未着手 |
| 武器(射撃・リロード・ADS)の能力化 | 今はテンプレートの処理のまま。段階的に移行する |
| スキル・ガジェットの種類と、1 人が持てる数 | **未定**。決まったら能力セットの分け方を相談する |

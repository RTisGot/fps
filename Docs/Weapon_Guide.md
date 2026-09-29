# 武器の作り方 ガイド(Blueprint)

武器は **Blueprint だけで** 作ります。能力(Ability)の仕組みは [GAS_Foundation.md](GAS_Foundation.md) の土台を使うので、先にそちらの 0〜2 章と 5 章を読んでください。

> 決まっていること
> - 武器は **ヒットスキャン**(弾を飛ばさず、撃った瞬間にレイで当たりを決める)
> - 武器枠は **メイン・サブの 2 つ**。持ち替えは **専用ボタン**
> - 当たり判定は **撃った人の画面で決めて、サーバーが確認する**(キャラの移動が速いため、撃った人の画面を優先する)
> - 武器は将来ロードアウト画面で選ぶ(ガジェットと一緒に選べる画面にする予定。今はまだない)
>
> 例として出てくる武器の名前は、すべて `<名前>` にしています。

---

## 0. 全体の形

```
DA_Weapon_<名前>(武器 1 つ分のデータ)
  └ 性能(ST_WeaponStats)、枠(E_WeaponSlot)、撃ち方(E_FireMode)、種類(E_WeaponType)、見た目

キャラクター(BP_FirstPersonCharacter)
  └ BPC_WeaponManager(武器を管理するコンポーネント)
       ├ メイン・サブの武器と、今どちらを持っているか
       ├ 武器ごとの弾数
       └ 命中をサーバーへ送る窓口(Run on Server のイベント)

能力(全武器で共通。DA_AbilitySet_Default に入れる)
  ├ GA_Weapon_Fire    … 射撃
  ├ GA_Weapon_Aim     … ADS
  ├ GA_Weapon_Reload  … リロード
  └ GA_Weapon_Swap    … 持ち替え
```

### 射撃の能力は武器ごとに作らない
`GA_Weapon_Fire` は全武器で 1 つだけ作り、「今持っている武器のデータ」を読んで動きを変えます。
**武器を増やすときは、データアセット(`DA_Weapon_<名前>`)を 1 つ作るだけ** で済みます。

### 撃ってからダメージが入るまで

```
[撃った人の画面]  ボタンを押す → GA_Weapon_Fire
                   → レイを飛ばして当たりを調べる → 発射の演出
                   → 当たった情報を BPC_WeaponManager からサーバーへ送る
[サーバー]         送られてきた命中を確認する(連射間隔・弾数・距離・壁など)
                   → 問題なければ Apply Damage
[全員]             HP が同期される
```

---

## 1. 使う BP アセット(`Content/FPWeapon/Blueprints`)

| アセット | 中身 |
|---|---|
| `ST_WeaponStats` | 武器の性能(ダメージ・弾数・連射速度・拡散・反動・ADS・距離減衰・ヒットスキャン) |
| `ST_WeaponModifier` | 性能の補正(Multiplier は掛ける値、Add は足す値) |
| `E_WeaponSlot` | Primary(メイン) / Secondary(サブ) |
| `E_FireMode` | SemiAuto / FullAuto / Burst |
| `E_WeaponType` | Rifle / SMG / Shotgun / Sniper / Pistol / HeavyPistol |

### 最初に決めておくこと(単位)
計算に直接使うので、作り始める前にチームで決めて、ここに書き足してください。

| 項目 | 決めること |
|---|---|
| `RateOfFire` | 1 分あたりの発数か、1 秒あたりか |
| `FalloffStartDistance` などの距離 | cm か m か(UE の距離は cm) |
| `HipSpread` などの拡散 | 角度(度)か、それ以外か |
| `ADSTime` | 秒か |
| Burst の発数 | `ST_WeaponStats` に項目がないので、追加するか決める |

---

## 2. 武器データの型を作る(最初に 1 回)

1. コンテンツブラウザで右クリック → Blueprint Class → All Classes で **PrimaryDataAsset** を選ぶ
2. 名前を `PDA_WeaponDefinition` にする
3. 変数を追加する。**すべて Instance Editable にチェック**(データアセットで入力できるようにするため)

| 変数 | 型 |
|---|---|
| `Stats` | ST_WeaponStats |
| `Slot` | E_WeaponSlot |
| `FireMode` | E_FireMode |
| `WeaponType` | E_WeaponType |
| `Mesh` | Skeletal Mesh |
| `DisplayName` | Text(ロードアウト画面用) |
| `Icon` | Texture2D(ロードアウト画面用) |

## 3. 武器を 1 つ作る(武器ごと)

1. 右クリック → Miscellaneous → Data Asset → **PDA_WeaponDefinition** を選ぶ
2. 名前を `DA_Weapon_<名前>` にする
3. 開いて、性能・枠・撃ち方・見た目を入力する

---

## 4. 武器の管理役 `BPC_WeaponManager`(最初に 1 回)

1. 右クリック → Blueprint Class → **Actor Component**、名前を `BPC_WeaponManager` にする
2. `BP_FirstPersonCharacter` を開き、Components に追加する
3. 追加したコンポーネントを選び、Details の **Component Replicates にチェック**(オンラインで同期するため。忘れると何も同期されない)

### 変数

| 変数 | 型 | Replication | 用途 |
|---|---|---|---|
| `PrimaryWeapon` | PDA_WeaponDefinition | Replicated | メインの武器 |
| `SecondaryWeapon` | PDA_WeaponDefinition | Replicated | サブの武器 |
| `CurrentSlot` | E_WeaponSlot | **RepNotify** | 今持っている方。変わったら見た目を切り替える |
| `PrimaryAmmo` / `SecondaryAmmo` | Integer | Replicated / 条件 **Owner Only** | マガジンの中の弾数 |
| `PrimaryReserve` / `SecondaryReserve` | Integer | Replicated / 条件 **Owner Only** | 予備弾 |
| `LastServerFireTime` | Float | なし | サーバーでの連射間隔の確認用 |

- 弾数を **Owner Only** にするのは、相手に弾数を知られないため
- ロードアウト画面ができるまでは、Class Defaults で `PrimaryWeapon` と `SecondaryWeapon` を直接設定しておく
- 試合開始時(BeginPlay で **Has Authority** が true のとき)に、各武器の `MagazineSize` と `MaxReserveAmmo` で弾数を満タンにする

### 関数・イベント

| 名前 | 種類 | 中身 |
|---|---|---|
| `GetCurrentWeapon` | 関数(Pure) | `CurrentSlot` に合わせて `PrimaryWeapon` か `SecondaryWeapon` を返す |
| `GetCurrentAmmo` | 関数(Pure) | 今持っている武器の弾数を返す |
| `OnRep_CurrentSlot` | RepNotify | 手に持つメッシュを `GetCurrentWeapon` の `Mesh` に切り替える |
| `ServerFire` | カスタムイベント / **Run on Server** / Reliable | 命中の確認とダメージ(7 章) |
| `ServerReload` | カスタムイベント / **Run on Server** / Reliable | 予備弾からマガジンへ補充 |
| `ServerSwapWeapon` | カスタムイベント / **Run on Server** / Reliable | `CurrentSlot` を切り替える |

> **なぜ送信の窓口を能力ではなくコンポーネントに置くのか**
> 能力(GA_)の中には、サーバーへ送るイベント(Run on Server)を作れません。
> プレイヤーのキャラについているコンポーネントなら作れるので、ここを窓口にします。

---

## 5. 射撃の能力 `GA_Weapon_Fire`

### Class Defaults
- 親: **SavaGameplayAbility**
- Activation Policy: **While Input Active**(フルオートのため。離すと自動で終わる)
- Asset Tags(古い資料では Ability Tags): `Ability.Type.Weapon`
- `DA_AbilitySet_Default` の Granted Gameplay Abilities に追加し、Input Tag を **`InputTag.Weapon.Fire`** にする

### Event Graph(この能力は撃った人の画面で先に動く)

```
Event ActivateAbility
 → Get Avatar Actor from Actor Info → Get Component by Class(BPC_WeaponManager)
 → Get Current Weapon → Stats / FireMode を取り出す
 → [1 発撃つ処理]
     ・前回撃ってから連射間隔が経っていなければ、何もしない
     ・弾が 0 なら End Ability(空撃ちの音などはここ)
     ・PelletCount 回くり返す:
         視点の位置と向き(Get Player Camera Manager など)から、
         Random Unit Vector in Cone in Degrees で拡散分ずらした方向を作り、
         HitScanRadius が 0 なら Line Trace By Channel、0 より大きければ Sphere Trace By Channel
         当たったら「当たった Actor・当たった位置・ボーン名」を配列に追加
     ・BPC_WeaponManager の ServerFire を呼ぶ(撃った位置と命中の配列を渡す)
     ・発射の演出(音・マズルフラッシュ・反動)
 → SemiAuto: End Ability
   Burst:    決めた発数だけ Wait Delay(連射間隔)を挟んでくり返し、End Ability
   FullAuto: Wait Delay(連射間隔)して [1 発撃つ処理] に戻る(ボタンを離すと能力ごと終わる)
```

- 連射間隔は `RateOfFire` から計算する(1 分あたりなら `60 ÷ RateOfFire` 秒)
- 拡散は状況で切り替える: 通常 `HipSpread`、ADS 中 `ADSSpread`、移動中 `MovementSpread`、空中 `JumpSpread`
- 待つのは必ず **Wait Delay**(Ability Task)を使う。`Delay` ノードは使わない
- 何もなかった場合(外れ)も `ServerFire` を呼ぶ(弾数と連射間隔をサーバーで数えるため)

---

## 6. サーバーでの確認とダメージ(`ServerFire` の中)

クライアントから届いた内容は **信用しない** のが基本です。次を順番に確認し、駄目ならそこで止めます(または、その命中だけ無視します)。

| 順 | 確認 | 駄目なら | 目的 |
|---|---|---|---|
| 1 | 前回の射撃(`LastServerFireTime`)から連射間隔が経っているか(通信の揺れの分、少し余裕を持たせる) | 全部無視 | 連射速度の改造を防ぐ |
| 2 | 弾が残っているか → 残っていれば 1 減らし、`LastServerFireTime` を更新 | 全部無視 | 弾数の改造を防ぐ |
| 3 | 命中の数が `PelletCount` 以下か | 全部無視 | 命中の水増しを防ぐ |
| 4 | 撃った位置が、サーバー上のキャラの位置に近いか | 全部無視 | 位置の偽装を防ぐ。**移動が速いので許容範囲は広めに**(まずは 300cm 程度から調整) |
| 5 | 当たった位置までの距離が、射程(`FalloffEndDistance` など)以内か | その命中を無視 | 超遠距離からの命中を防ぐ |
| 6 | 撃った位置から当たった位置へ Line Trace し、途中に壁がないか(自分と相手は無視する) | その命中を無視 | 壁越しの命中を防ぐ |
| 7 | **Are Enemies**(自分, 当たった Actor)が true か | その命中を無視 | 味方撃ちを防ぐ |

### ダメージの計算
確認を通った命中ごとに計算して、**Apply Damage**(カテゴリ `Sava|Damage`)を呼びます。

1. **頭か**: ボーン名が頭のボーン(スケルトンによって名前が違う。例: `head`)なら `HeadDamage`、それ以外は `BaseDamage`
2. **距離減衰**: 撃った位置から当たった位置までの距離を `d` として
   - `d ≤ FalloffStartDistance` → そのまま
   - `d ≥ FalloffEndDistance` → `MinimumDamage`
   - その間 → 元のダメージから `MinimumDamage` へ、距離に合わせて線形に下げる(Map Range Clamped ノードが便利)
3. **Apply Damage**: Damage Instigator = 自分のキャラ(`Get Owner`)、Target = 当たった Actor、Damage = 計算した値、Damage Causer = 自分のキャラ

- `Apply Damage` はサーバーでしか動きません(クライアントで呼んでも何も起きない安全装置付き)
- **HP を直接 Set しない**でください。HP 0 の通知などが動かなくなります

---

## 7. その他の能力

### リロード `GA_Weapon_Reload`
- 親: SavaGameplayAbility / Activation Policy: On Input Triggered / Input Tag: **`InputTag.Weapon.Reload`**
- Asset Tags: `Ability.Type.Weapon`
- **Block Abilities with Tag** で、リロード中に使えなくする能力を指定する
  - `Ability.Type.Weapon` を入れる → 射撃も ADS も止まる
  - 射撃だけ止めたい → `GA_Weapon_Fire` の Asset Tags に専用のタグ(例: `Ability.Weapon.Fire`。追加は相談)も付けて、そのタグを入れる
- 流れ: マガジンが満タン・予備弾 0 なら End Ability → `Play Montage and Wait` → 終わったら `ServerReload` → End Ability
- 途中で持ち替えたらキャンセルされる(下の Swap の設定)ので、補充はアニメーションが**終わったあと**に行う

### ADS `GA_Weapon_Aim`
- 親: SavaGameplayAbility / Activation Policy: **While Input Active** / Input Tag: **`InputTag.Weapon.Aim`**
- 流れ: 画角を `ADSZoom` に合わせて `ADSTime` かけて変える → 押している間は ADS 中の状態にしておく → 離したら戻す
- 「ADS 中か」は、射撃の能力が拡散を選ぶときに使う。タグ(例: `State.ADS`。追加は相談)を Effect で付けておくと、他の能力からも判定しやすい
- 構え中に遅くしたい場合は、`MoveSpeedMultiplier` を下げる Effect を付ける(GAS_Foundation.md 5-5)

### 持ち替え `GA_Weapon_Swap`
1. Project Settings → GameplayTags で **`InputTag.Weapon.Swap`** を追加
2. 入力アクション `IA_SwapWeapon` を作って IMC にキーを割り当て、`DA_InputConfig` に「IA_SwapWeapon → InputTag.Weapon.Swap」を追加
3. `GA_Weapon_Swap` を作る
   - 親: SavaGameplayAbility / Activation Policy: On Input Triggered / Input Tag: `InputTag.Weapon.Swap`
   - **Cancel Abilities with Tag** に `Ability.Type.Weapon`(持ち替えると射撃・リロード・ADS が中断される)
   - 流れ: 持ち替えのアニメーション(任意)→ `ServerSwapWeapon` → End Ability

---

## 8. 気をつけること

- **ダメージは必ずサーバーの確認を通してから**。確認を飛ばすと、改造したクライアントに好きなだけダメージを出されます
- **テンプレートの銃は使わない**: `BP_Pickup_Rifle` や C++ の `savaWeaponComponent` は通信に対応していません。この仕組みで置き換えます
- **Blueprint は git でマージできない**: `BPC_WeaponManager`・`GA_Weapon_Fire`・`PDA_WeaponDefinition` を 2 人で同時に編集しないでください。触る前に声をかける
- **テストは Net Mode = Play As Listen Server、プレイヤー 2 人以上**。サーバーでの確認は **ゲスト(クライアント)側で撃ったとき** に初めて意味を持つので、ゲストの画面から撃って確かめる
- 動かないとき
  - 何も同期されない → `BPC_WeaponManager` の Component Replicates を確認
  - ゲストが撃ってもダメージが出ない → `ServerFire` が Run on Server になっているか、確認のどこで止まっているか(Print String で確認)
  - 能力が発動しない → コンソールで `showdebug abilitysystem`(GAS_Foundation.md 11 章)

## 9. まだないもの(今後)

| 内容 | 状態 |
|---|---|
| 1 章の単位の決定 | **最初に決める** |
| ロードアウト画面(武器とガジェットを選ぶ) | 武器ができてから作る。`PDA_WeaponDefinition` の `DisplayName` と `Icon` を使う予定 |
| `ST_WeaponModifier`(アタッチメントなど)の使い方 | 未定 |
| 弾数・リロードの UI | 未着手 |
| 死亡・リスポーン時の弾数の扱い | 未定 |

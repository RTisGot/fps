# HUD に武器・スキル・ガジェットを出す BP の作り方

`WBP_HUD` に、今持っている武器・サブ武器・弾数・スキル・ガジェット(個数)を出す手順。
C++ 側(装備の管理・サーバーとの同期・変わったときの通知)はできているので、BP では **キャラクターを HUD に渡し、通知を受けて表示を書き換えるだけ** でよい。

BP の基本操作(ノードの置き方・Is Variable など)は [OnlineLobby_BP_Guide.md](OnlineLobby_BP_Guide.md) の 0 章を参照。

---

## 0. 全体の形

```
BP_FirstPersonCharacter(自分が操作する体)
  ├ Event On Controller Changed → Create Widget(WBP_HUD)… ここで自分(Self)を HUD に渡す
  ├ Equipment コンポーネント       … メイン・サブの武器、今どちらを持っているか、弾数
  ├ AbilityLoadout コンポーネント  … スキル・ガジェット(名前・アイコン)
  └ (PlayerState の) ASC          … ガジェットの残り個数(Attribute: GadgetCharges / MaxGadgetCharges)

WBP_HUD
  ├ 変数 OwnerCharacter(作るときに受け取る)
  ├ Event Construct で、コンポーネントの通知(Event Dispatcher)に Bind する
  └ 通知が来たら / 最初に 1 回、表示を書き換える(Refresh)
```

### HUD はキャラクターと一緒に作り直される

- HUD は `BP_FirstPersonCharacter` の **Event On Controller Changed** で作っている(今の作りのまま)
- 死んで体が操作されなくなったときや体が消えたときは、C++(`AsavaCharacter::RemoveOwnedWidgets`)が **体の変数に入っている Widget を自動で外す**
- そのため **HUD 1 つ = 体 1 つ** になる。リスポーンしたら新しい体が新しい HUD を作るので、HUD 側で「前の体の Bind を外す」処理はいらない

### 通知の一覧(Unity でいう C# の event)

| 通知 | 持ち主 | いつ来るか | 引数 |
|---|---|---|---|
| `On Weapon Changed` | Equipment | 持ち替えたとき・装備が変わったとき(サーバーから装備が届いたときも) | Slot, Weapon |
| `On Ammo Changed` | Equipment | 撃った瞬間・リロード・持ち替え | Slot, Ammo In Magazine, Reserve Ammo |
| `On Skill Changed` | AbilityLoadout | スキルが変わったとき(届いたときも) | Skill |
| `On Gadget Changed` | AbilityLoadout | ガジェットが変わったとき(届いたときも) | Gadget |

> **最初の 1 回は自分で Refresh する**
> オンラインでは、装備がサーバーから **HUD を作った後に** 届くことがある(そのときは通知が来る)。逆に **HUD を作る前に** 届いていれば通知は来ない。
> だから「Bind する」と「今の値で 1 回描く」を **両方** やる。こうすると、どちらの順番でも正しく出る。

---

## 1. HUD にキャラクターを渡す

### 1-1. WBP_HUD に変数を作る

1. `Content/UI/WBP_HUD` を開き、**Graph** タブへ
2. 左の **My Blueprint** → **Variables** の **+** で変数を作る
   - 名前: `OwnerCharacter`
   - 型: `BP_First Person Character`(**Object Reference**)
3. 右の Details で次の 2 つにチェック
   - **Instance Editable**
   - **Expose on Spawn**(Create Widget のノードにピンとして出る。Unity でいう `Instantiate` 直後に代入する値)
4. **Compile** → **Save**

### 1-2. BP_FirstPersonCharacter から渡す

1. `Content/FirstPerson/Blueprints/BP_FirstPersonCharacter` を開く
2. Event Graph で、HUD を作っている **Create Widget(WBP_HUD)** を探す(Event On Controller Changed から伸びている)
3. 1-1 の後なら、ノードに **Owner Character** のピンが増えている。何もない所を右クリック → **Get a reference to self**(`Self`)を置き、このピンにつなぐ
   - 増えていなければ、ノードを右クリック → **Refresh Node**
4. **Compile** → **Save**

> Create Widget の前に「自分が操作している体か(Is Locally Controlled)」の確認が入っているかも見ておく。入っていないと、listen サーバーのホストの PC で、他のプレイヤーの体の分まで HUD が作られることがある。
> 確認がなければ、Create Widget の前に **Is Locally Controlled** → **Branch** を入れ、True のときだけ作るようにする。

---

## 2. 見た目を置く(Designer)

`WBP_HUD` の **Designer** タブで、次の部品を置く。どれも Details の一番上の **Is Variable** にチェックする(Graph から使うため)。

| 部品 | 名前 | 中身 |
|---|---|---|
| Image | `Img_PrimaryIcon` | メイン武器のアイコン |
| Text | `Txt_PrimaryName` | メイン武器の名前 |
| Text | `Txt_PrimaryAmmo` | メイン武器の弾数(例: `24 / 120`) |
| Image | `Img_SecondaryIcon` | サブ武器のアイコン |
| Text | `Txt_SecondaryName` | サブ武器の名前 |
| Text | `Txt_SecondaryAmmo` | サブ武器の弾数(サブは予備弾が無限なので `12 / ∞`) |
| Image | `Img_SkillIcon` | スキルのアイコン |
| Text | `Txt_SkillName` | スキルの名前 |
| Image | `Img_GadgetIcon` | ガジェットのアイコン |
| Text | `Txt_GadgetCount` | ガジェットの残り個数(例: `2 / 2`) |

- 今持っている方の武器を目立たせたいので、メイン・サブをそれぞれ **Border**(または Horizontal Box)で囲み、`Box_Primary` / `Box_Secondary` として Is Variable にしておく。持っている方を不透明、持っていない方を半透明にする(4 章)
- 配置は自由。よくある形は、右下に武器 2 つを縦に並べ、左下にスキル・ガジェットを置く

---

## 3. 通知に Bind する(Event Construct)

`WBP_HUD` の **Graph** タブで作業する。

### 3-1. コンポーネントを変数に入れる

1. 変数を 2 つ作る(Expose on Spawn はいらない)
   - `Equipment`: 型 `Sava Equipment Component`(Object Reference)
   - `AbilityLoadout`: 型 `Sava Ability Loadout Component`(Object Reference)
2. **Event Construct** から、次の順につなぐ
   1. `OwnerCharacter` を Get → **Is Valid**(マクロの方。実行ピンが Is Valid / Is Not Valid に分かれる)
   2. Is Valid → **Find Equipment Component**(Actor に `OwnerCharacter`)→ 戻り値を **Set Equipment**
   3. → **Find Ability Loadout Component**(Actor に `OwnerCharacter`)→ 戻り値を **Set AbilityLoadout**

> `OwnerCharacter` から直接 `Equipment Component` を Get しても同じ物が取れる。どちらでもよい。

### 3-2. Bind する

続けて、次の 4 つをつなぐ。`Equipment` / `AbilityLoadout` の青いピンをドラッグ → `Bind Event to On 〜` で検索すると出る。

| Bind するノード | Target | Event につなぐ Custom Event |
|---|---|---|
| Bind Event to On Weapon Changed | `Equipment` | `HandleWeaponChanged` |
| Bind Event to On Ammo Changed | `Equipment` | `HandleAmmoChanged` |
| Bind Event to On Skill Changed | `AbilityLoadout` | `HandleSkillChanged` |
| Bind Event to On Gadget Changed | `AbilityLoadout` | `HandleGadgetChanged` |

Custom Event の作り方: Bind ノードの赤い **Event** ピンをドラッグ → **Add Custom Event**。通知と同じ引数を持ったイベントが自動でできるので、名前だけ変える。

### 3-3. 最初の 1 回を描く

Bind の後に、4 章で作る関数を順に呼ぶ。

`Refresh Weapons` → `Refresh Skill` → `Refresh Gadget`

### 3-4. 通知が来たら描き直す

| Custom Event | 中で呼ぶもの |
|---|---|
| `HandleWeaponChanged` | `Refresh Weapons` |
| `HandleAmmoChanged` | `Refresh Ammo`(Slot を渡す) |
| `HandleSkillChanged` | `Refresh Skill` |
| `HandleGadgetChanged` | `Refresh Gadget` |

> `On Weapon Changed` は「持ち替え」でも「装備の変更」でも来て、装備の変更のときは枠ごとに 1 回ずつ来る。引数で場合分けせず、**毎回メイン・サブの両方を描き直す** のが簡単で間違いにくい。

---

## 4. 表示を書き換える関数

My Blueprint → **Functions** の **+** で関数を作る。

### 4-1. `Refresh Slot`(1 つの枠を描く。下の関数から使う)

- 入力: `Slot`(`ESava Weapon Slot`)、`Icon`(Image)、`Name`(Text Block)、`Box`(Border など)
  - 入力の型は、Designer で置いた部品と同じ型にする

1. `Equipment` → **Get Weapon Data**(Slot)
2. 戻り値を **Is Valid** で確かめる
   - Is Not Valid(その枠に何も持っていない)→ `Box` の **Set Visibility** を `Collapsed` にして終わり
   - Is Valid → `Box` を `Visible` にして、次へ
3. 戻り値から **Display Name** を Get → `Name` の **Set Text**
4. 戻り値から **Icon** を Get → `Icon` の **Set Brush from Texture**
5. 今持っている枠か: `Equipment` → **Get Current Slot** と `Slot` を **==** で比べる → **Select Float**(True: `1.0`、False: `0.4`)→ `Box` の **Set Render Opacity**
6. 最後に `Refresh Ammo`(Slot)を呼ぶ

### 4-2. `Refresh Weapons`

1. `Equipment` を **Is Valid** で確かめる
2. `Refresh Slot`(Slot: `Primary`、Icon: `Img_PrimaryIcon`、Name: `Txt_PrimaryName`、Box: `Box_Primary`)
3. `Refresh Slot`(Slot: `Secondary`、Icon: `Img_SecondaryIcon`、Name: `Txt_SecondaryName`、Box: `Box_Secondary`)

### 4-3. `Refresh Ammo`

- 入力: `Slot`(`ESava Weapon Slot`)

1. `Equipment` → **Get Ammo In Magazine**(Slot)と **Get Reserve Ammo**(Slot)
2. 予備弾の文字: `Equipment` → **Has Infinite Reserve Ammo**(Slot)→ **Select**(True: `∞`、False: Reserve Ammo を **To Text**)
3. **Format Text** で `{Mag} / {Reserve}` を作る
4. Slot で **Switch on ESavaWeaponSlot** → Primary なら `Txt_PrimaryAmmo`、Secondary なら `Txt_SecondaryAmmo` に **Set Text**

> 弾数は、撃った人の画面では撃った瞬間に減る(サーバーの返事を待たない)。だから HUD もすぐ反応する。

### 4-4. `Refresh Skill`

1. `AbilityLoadout` → **Get Skill**
2. **Is Valid**
   - Is Not Valid → `Img_SkillIcon` と `Txt_SkillName` を `Collapsed`
   - Is Valid → `Visible` にして、**Display Name** → `Txt_SkillName` の Set Text、**Icon** → `Img_SkillIcon` の Set Brush from Texture

### 4-5. `Refresh Gadget`

4-4 と同じ形で、**Get Gadget** を使い、`Img_GadgetIcon` を書き換える。
ガジェットを持っていなければ `Txt_GadgetCount` も `Collapsed` にする。

---

## 5. ガジェットの残り個数

個数は、コンポーネントではなく **ASC の Attribute**(`GadgetCharges` / `MaxGadgetCharges`)に入っている。
ASC は PlayerState が持っていて、クライアントでは **HUD を作った時点ではまだ届いていない** ことがある。そのため、体力バー(`Health_Bar`)と同じく **Binding**(毎フレーム値を読む)で出すのが一番確実。数字 2 つを読むだけなので重くない。

1. Designer で `Txt_GadgetCount` を選ぶ → Details の **Text** の横の **Bind** → **Create Binding**
2. できた関数の中で、次のようにつなぐ
   1. **Get Float Attribute**(Actor: `OwnerCharacter`、Attribute: `SavaAttributeSet.GadgetCharges`)
   2. もう 1 つ **Get Float Attribute**(Attribute: `SavaAttributeSet.MaxGadgetCharges`)
   3. それぞれ **Truncate** で整数にする → **Format Text** の `{Count} / {Max}` → Return Value へ
3. ASC がまだ届いていないときは 0 が返るだけなので、エラーにはならない(届けば次のフレームから正しく出る)

> 使い切ったときに色を変えたいなら、同じように **Color and Opacity** にも Binding を作り、Count が 0 なら灰色を返す。

---

## 6. 確認

1. **Play** の設定(▶ の横の ︙)で、**Number of Players** を `2`、**Net Mode** を `Play As Listen Server` にして起動
2. ホストとクライアントの **両方の画面で** 確かめる
   - [ ] 起動直後に、メイン・サブの名前・アイコン・弾数が出ている
   - [ ] 撃つと、持っている方の弾数がすぐ減る
   - [ ] リロードで弾数が戻る。サブの予備弾は `∞`
   - [ ] 持ち替えると、目立つ枠が入れ替わる
   - [ ] スキル・ガジェットのアイコンと名前が出ている
   - [ ] ガジェットを投げると個数が減る
   - [ ] 死んでリスポーンしても、HUD が重ならず、表示が正しい
   - [ ] ホストの画面に、クライアントの体の分の HUD が重なっていない
3. 表示が出ないとき
   - 変数 `OwnerCharacter` が空になっていないか: Event Construct の Is Not Valid 側に **Print String** をつないで確かめる
   - コンソール(`` ` `` キー)で `Sava.LocalStatus` を打つと、表示中の Widget が Output Log に出る(HUD が作られているかの確認)

---

## 7. まだないもの(必要になったら C++ に足す)

- **スキルのクールダウンの残り秒数**: BP から簡単に読める関数がまだない。`USavaAbilitySystemLibrary` に「Cooldown タグを持つ効果の残り時間」を返す BlueprintPure の関数を 1 つ足せば、5 章と同じ Binding で出せる
  - 「クールダウン中かどうか」だけなら、BP の **Wait Gameplay Tag Count Changed On Actor**(Tag: その能力の Cooldown タグ)でも作れる
- **スキル・ガジェットのボタン表示**(`Q` など): データの `InputTag` から、今のキー設定を引く仕組みはまだない

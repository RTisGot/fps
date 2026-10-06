# グレネードの作り方 ガイド(C++)

ガジェット担当向けに、**C++ でグレネードを作る手順と、その仕組み** をまとめたものです。
能力(Ability)の土台は [GAS_Foundation.md](GAS_Foundation.md) を使うので、先にそちらの 0〜2 章・5 章・6 章(長押しで狙う能力)を読んでください。

> 作るもの
> - **長押しで狙う**(予測線と爆発範囲を表示)→ **離して投げる**
> - **着弾した地点で爆発** する
> - ダメージは **爆発の中心から遠いほど減る**(端では最大の 20% など)
> - **ダメージ・半径・所持数** はエディタで調整できる
> - 長押し中に右クリックでキャンセル(個数は減らない)

---

## 0. 全体の形

C++ のクラスは 3 つです。**「投げる能力」と「投げられる物」を分けている** のがポイントです。

```
UGadgetThrowAbility(投げる能力)          … Gadget/GadgetThrowAbility.h/.cpp  ← 今回新しく作った
  親: USavaHoldAimAbility(長押しで狙う能力の土台。狙う・確定・サーバーへの送信・個数の消費をやってくれる)
  やること: 予測線を出す / 離したらグレネードを Spawn して投げる

AGadgetBase(投げられる物の共通部分)     … Gadget/GadgetBase.h/.cpp      ← 既存(少しだけ修正)
  やること: 飛ぶ(ProjectileMovement)/ 着弾を検知 / 状態(飛行中・着弾・起爆…)の管理
  └ AFragGrenade(グレネード)           … Gadget/FragGrenade.h/.cpp     ← 中身を実装
       やること: 着弾したら起爆 / 範囲内の敵にダメージ(距離で減衰)/ 爆発の見た目を全員に出す
```

エディタ側で作るもの:

```
BP_Gadget_FragGrenade  (親: FragGrenade)          … 見た目のメッシュ、ダメージ・半径の値
GA_Gadget_FragGrenade  (親: GadgetThrowAbility)   … Gadget Class = BP_Gadget_FragGrenade、所持数(Max Charges)
DA_Gadget_FragGrenade  (SavaGadgetData)           … Ability = GA_Gadget_FragGrenade、Input Tag = InputTag.Gadget
キャラクターの AbilityLoadout > Default Gadget = DA_Gadget_FragGrenade
```

### Unity との対応

| Unreal | Unity でいうと |
|---|---|
| `AFragGrenade`(Actor) | グレネードの Prefab に付いた MonoBehaviour |
| `BP_Gadget_FragGrenade`(C++ クラスの Blueprint の子) | その Prefab(値やメッシュをインスペクタで設定したもの) |
| `UProjectileMovementComponent` | Rigidbody + 初速を与えるスクリプト(物理ではなく、自前で放物線を動かす) |
| `USphereComponent` | SphereCollider |
| `OverlapMultiByObjectType` | `Physics.OverlapSphere` |
| `LineTraceTestByObjectType` | `Physics.Linecast` |
| CDO(`GetDefaultObject`) | Prefab のアセットそのものの値を読む(Instantiate していない状態) |
| `UPROPERTY(EditDefaultsOnly)` | `[SerializeField]`(ただし Prefab=Blueprint 側でだけ編集できる) |
| GameplayAbility | 「ボタンを押すと動く処理」をまとめたクラス。通信・クールダウン・コストを面倒みてくれる |

---

## 1. 処理の流れ(どこで動くか)

マルチプレイなので、**どの処理がどのマシンで動くか** が一番大事です。

```
[自分の画面]                                [サーバー]                       [全員の画面]
 ボタンを押す
  └ GadgetThrowAbility 発動
     OnAimStarted   … 予測線の設定を合わせる
     OnAimUpdated   … 毎フレーム予測線を描く
 ボタンを離す
  └ Commit Ability(個数を 1 減らす。予測)
  └ 投げる位置と向きをサーバーへ送る  ──→  届いた位置が手元から遠すぎないか確認
                                           Commit Ability(個数を 1 減らす。確定)
                                           OnConfirmed
                                            └ BP_Gadget_FragGrenade を Spawn
                                            └ ThrowGadget(向き) で飛ばす   ──→  Actor が複製されて見える
                                           (飛行中…)
                                           着弾 → OnProjectileStopped
                                            └ OnGadgetLanded → Detonate
                                               └ ApplyGadgetEffect
                                                  ├ Multicast_PlayExplosion ──→  OnExploded(爆発の見た目・音)
                                                  ├ 範囲内の敵を探す
                                                  └ 距離でダメージを計算 → ApplyDamage(HP が減る → 全員へ同期)
                                               └ Destroy
```

- **Spawn・ダメージはサーバーだけ** が行います。クライアントが「当たった」と言っても信用しない、というのがチート対策の基本です
- **予測線は自分の画面だけ** で描きます(他の人には見えなくてよいので)
- 爆発の **見た目** は全員に必要なので、サーバーから `NetMulticast` の RPC で全員に伝えます

---

## 2. 手順

### 手順 1: AGadgetBase の修正(既存クラス)

`AGadgetBase` はすでに「飛ぶ・着弾を検知して `OnGadgetLanded` を呼ぶ・`Detonate` で `ApplyGadgetEffect` を呼んで消える」まで出来ていました。
実際に投げると問題が出る箇所が 2 つあったので直しています。

**(1) 投げた本人に当たって手元で爆発する**

グレネードはカメラの少し前(手元)で Spawn するので、自分のカプセルの中から飛び始めます。何もしないと、動いた瞬間に自分に当たって止まり → 着弾扱い → 手元で爆発します。

```cpp
// GadgetBase.cpp ThrowGadget()
if (AActor* GadgetOwner = GetOwner())
{
    m_CollisionComponent->IgnoreActorWhenMoving(GadgetOwner, true);   // 本人を無視
    // 本人に付いている物(武器など)も無視
    ...
}
```

`IgnoreActorWhenMoving` は「このコンポーネントが動くとき、この Actor には当たらない」という設定です(Unity の `Physics.IgnoreCollision` に近い)。

**(2) 落下中に速さが頭打ちになって、予測線とずれる**

`MaxSpeed = ThrowSpeed` になっていたため、落下で加速しても投げた速さで止められ、実際の軌道が予測線より手前に落ちていました。`MaxSpeed = 0`(上限なし)にしています。

あわせて、予測線の太さを合わせるために当たり判定の半径を返す `GetCollisionRadius()` を足しました。

> ガジェットのソースファイルは Shift-JIS で保存されていたので、他のファイルと同じ UTF-8 に変換しました。Visual Studio で開いて文字化けする場合は「ファイル → 保存オプションの詳細設定」で「Unicode (UTF-8 シグネチャなし)」を選んでください。

### 手順 2: AFragGrenade(グレネード本体)

`AGadgetBase` の **仮想関数を 2 つ override するだけ** で、グレネード固有の動きになります。これがクラスを分けている理由です(スタン・スモークも同じように `OnGadgetLanded` と `ApplyGadgetEffect` を変えるだけで作れます)。

```cpp
// 着弾した瞬間に爆発
void AFragGrenade::OnGadgetLanded(const FHitResult& Hit)
{
    Detonate();   // → 親が ApplyGadgetEffect() を呼んで、その後 Destroy する
}
```

`ApplyGadgetEffect()` の中身は 4 段階です。

**(a) 爆発の見た目を全員に出す**

```cpp
Multicast_PlayExplosion(Origin);
```

```cpp
UFUNCTION(NetMulticast, Reliable)
void Multicast_PlayExplosion(const FVector_NetQuantize& ExplosionLocation);
```

- `NetMulticast` … サーバーで呼ぶと、サーバーと全クライアントで `_Implementation` が実行される RPC
- `Reliable` … 必ず届く。**この直後に Destroy するので Unreliable だと落ちることがある**
- `FVector_NetQuantize` … 小数点以下を丸めて送る FVector(通信量が減る。爆発位置ならこれで十分)
- 中で `OnExploded`(`BlueprintImplementableEvent`)を呼ぶので、**エフェクト・音は Blueprint 側で置けます**

**(b) 範囲内のキャラクターを集める**

```cpp
World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity,
    FCollisionObjectQueryParams(ECC_Pawn),        // Pawn(キャラクター)だけ
    FCollisionShape::MakeSphere(Radius), QueryParams);
```

ここで 2 つ工夫しています。

- **同じ人に 2 回ダメージを入れない**: 1 人のキャラが複数の当たり判定を持つことがあるので、`TMap<AActor*, float>` で「人 → 一番近い距離」にまとめます
- **距離は相手の中心ではなく、当たり判定の表面まで**: `GetClosestPointOnCollision` を使います。中心までだと、足元で爆発しても「90cm 離れている」扱いになり、ダメージが減ってしまうためです

**(c) ダメージを与えてよい相手か**

```cpp
bool AFragGrenade::CanDamageTarget(const AActor* Target) const
{
    if (!GadgetOwner)          return true;              // 投げた人が抜けた
    if (Target == GadgetOwner) return m_bDamageOwner;    // 自爆するか(設定)
    return USavaAbilitySystemLibrary::AreEnemies(GadgetOwner, Target);   // 味方はダメージなし
}
```

壁越しの判定(`m_bBlockedByWalls`)は、爆発の中心から相手の中心へ線を引き、**WorldStatic(壁・床)** に当たったらダメージなしにしています。人や他の投げ物は遮りません。

**(d) 距離でダメージを計算して与える**

```cpp
const float Damage = CalculateFalloffDamage(Distance, MaxDamage, Radius, m_FullDamageRadius, m_MinDamageRatio);
USavaAbilitySystemLibrary::ApplyDamage(DamageInstigator, Target, Damage, this);
```

ダメージは必ず `USavaAbilitySystemLibrary::ApplyDamage` を通します。中で GAS の GameplayEffect(`USavaGE_Damage`)を使って HP を減らすので、死亡処理・HP の同期・(今後の)防御力の計算などが全員共通になります。
Unreal 標準の `UGameplayStatics::ApplyRadialDamageWithFalloff` もありますが、これは GAS ではない古いダメージの仕組み(`TakeDamage`)なので、このプロジェクトでは使いません。

### 手順 3: 減衰の計算

```
ダメージ
 100 |■■■■■■■■\
     |         \
     |          \
  20 |           \■ ← 端(Effect Radius)では Min Damage Ratio(20%)
   0 +---------+----+------→ 距離
     0       100   400
         Full Damage  Effect
         Radius       Radius
```

```cpp
if (Radius <= 0 || Distance > Radius) return 0;          // 範囲外
if (Distance <= FullDamageRadius)     return MaxDamage;  // 近くは減らない
const float Alpha = (Distance - FullDamageRadius) / (Radius - FullDamageRadius);  // 0〜1
return FMath::Lerp(MaxDamage, MaxDamage * MinDamageRatio, Alpha);
```

- `Alpha` は「減り始めの位置で 0、端で 1」になる割合です
- `FMath::Lerp(A, B, Alpha)` は Unity の `Mathf.Lerp` と同じです
- 例(最大 100、半径 400、Full 100、端 20%): 距離 250 → Alpha 0.5 → **60 ダメージ**

この関数は **`static`(メンバー変数を使わない)** にしてあります。Actor を Spawn しなくても呼べるので、自動テスト(`Source/sava/Tests/SavaGadgetTests.cpp`)で数字を確かめられます。減り方を変えたいとき(例: 2 乗で減らす)は、ここを変えてテストの期待値も直してください。

### 手順 4: UGadgetThrowAbility(投げる能力)

「長押しで狙って離して確定」は `USavaHoldAimAbility` が全部やってくれるので、**子クラスは 3 つのイベントを実装するだけ** です。

```cpp
UGadgetThrowAbility::UGadgetThrowAbility()
{
    AimMode = ESavaAimMode::ProjectileArc;                     // 放物線で狙う
    MaxCharges = 2;                                            // 所持数
    CancelInputTag = SavaGameplayTags::InputTag_Weapon_Aim;    // 右クリックでキャンセル
    // Asset Tags に Ability.Type.Gadget を付ける
}
```

| イベント | どこで動く | 中身 |
|---|---|---|
| `OnAimStarted` | 自分の画面 | 予測線の速さ・太さを、投げる物の `ThrowSpeed`・当たり判定の半径に合わせる |
| `OnAimUpdated` | 自分の画面・毎フレーム | 予測線(`PathPoints` を線で結ぶ)・着弾点・爆発範囲の円を描く |
| `OnConfirmed` | サーバー | `GadgetClass` を Spawn して `ThrowGadget` |

ポイント:

- **C++ では `<イベント名>_Implementation` を override します**。`BlueprintNativeEvent` は「C++ に既定の処理があって、Blueprint でも上書きできる関数」で、C++ 側の本体は `_Implementation` という名前になる決まりです
- **`ActivateAbility` は override しません**。親が「押す → 狙う → 離す → サーバーへ送る → 確認 → 個数を減らす」をやっているので、壊さないようにイベントだけを使います
- **個数(所持数)は親が `CommitAbility` を呼んだときに 1 減ります**。投げた瞬間だけ減り、キャンセルでは減りません。0 個なら押しても発動しません。リスポーンで満タンに戻ります(`USavaGameplayAbility::OnGiveAbility`)
- **CDO(`GetDefaultObject`)** で、まだ Spawn していない `BP_Gadget_FragGrenade` の設定値(投げる速さ・半径)を読んでいます。値を 2 か所に書かなくて済むので、ずれが起きません
- `OnConfirmed` の `TargetTransform` はクライアントから届いた値ですが、親が「手元から遠すぎないか」を確認済みです

**予測線と実際の軌道を一致させる条件**: 速さ(自動で合わせる)・太さ(自動で合わせる)・重力(`Gravity` を 1 のままにする)。`Gravity` を変えると線がずれます(ログに警告が出ます)。

### 手順 5: ビルド

エディタを閉じて Visual Studio / Rider でビルドするか、エディタの Live Coding(Ctrl+Alt+F11)を使います。
**新しいクラスを追加したときや UPROPERTY・UFUNCTION を増やしたときは、Live Coding ではなくエディタを閉じてビルド** するのが安全です。

### 手順 6: エディタでの設定

1. **BP_Gadget_FragGrenade を作る**
   - コンテンツブラウザ → 右クリック → Blueprint Class → All Classes で `FragGrenade` を選ぶ
   - Components に **Static Mesh** を追加(球など。スケール 0.15 くらい)。**Collision Presets を `NoCollision`** にする(当たり判定は親の球だけにする)
   - Class Defaults:
     - **Gadget Data > Damage** … 中心での最大ダメージ(既定 100)
     - **Gadget Data > Effect Radius** … 爆発の半径 cm(既定 400)
     - **Gadget Data > Throw Speed** … 投げる速さ(既定 1500)
     - **Gadget|Frag** の Full Damage Radius / Min Damage Ratio / Damage Owner / Blocked By Walls
     - 調整中は **Draw Debug** を ON にすると、爆発範囲(赤 = 半径、黄 = 減らない範囲)と各相手へのダメージが 3 秒表示されます
   - Event Graph で **Event On Exploded** を置き、`Spawn System at Location`(Niagara)や `Play Sound at Location` をつなぐと爆発の見た目・音になります

2. **GA_Gadget_FragGrenade を作る**
   - Blueprint Class → All Classes で `GadgetThrowAbility` を選ぶ
   - Class Defaults:
     - **Gadget Class** = `BP_Gadget_FragGrenade`
     - **Max Charges** = 所持数(既定 2)
     - Launch Offset(視点から見た投げる位置)、Max Prediction Time(予測線の長さ)は必要なら

3. **DA_Gadget_FragGrenade を作る**
   - Miscellaneous → Data Asset → **SavaGadgetData**
   - Ability = `GA_Gadget_FragGrenade`、Input Tag = `InputTag.Gadget`、Display Name、Icon

4. **キャラクターに持たせる**
   - キャラクター Blueprint の **AbilityLoadout** コンポーネント → **Default Gadget** = `DA_Gadget_FragGrenade`
   - 今入っている Blueprint 版(`DA_Impact` など)と入れ替えます。ガジェットは 1 人 1 つです

### 手順 7: 動作確認

- Play の設定で **Number of Players = 2、Net Mode = Play As Listen Server** にして確認します(一人プレイだけだと通信の問題に気づけません)
- 確認すること:
  - [ ] ガジェットボタン長押しで緑の予測線と爆発範囲の円が出る(クライアント側の画面でも)
  - [ ] 離すとその線の通りに飛び、着弾点で爆発する
  - [ ] 右クリックでキャンセルでき、個数が減らない
  - [ ] 2 個投げたら投げられなくなり、リスポーンで戻る
  - [ ] 近いほどダメージが大きい(Draw Debug の数字で確認。ホスト側の画面に出ます)
  - [ ] 味方にはダメージが入らない / 壁の裏には入らない
- 自動テスト: Tools → Session Frontend → Automation で `Sava.Gadget` を実行

---

## 3. 調整できる値の一覧

| 何を | どこで | 既定 |
|---|---|---|
| 最大ダメージ | BP_Gadget_FragGrenade > Gadget Data > Damage | 100 |
| 爆発の半径 | BP_Gadget_FragGrenade > Gadget Data > Effect Radius | 400 cm |
| 減らない範囲 | BP_Gadget_FragGrenade > Gadget Frag > Full Damage Radius | 100 cm |
| 端でのダメージ割合 | BP_Gadget_FragGrenade > Gadget Frag > Min Damage Ratio | 0.2 |
| 自爆するか | BP_Gadget_FragGrenade > Gadget Frag > Damage Owner | ON |
| 壁で防げるか | BP_Gadget_FragGrenade > Gadget Frag > Blocked By Walls | ON |
| 投げる速さ(= 飛距離) | BP_Gadget_FragGrenade > Gadget Data > Throw Speed | 1500 cm/s |
| 所持数 | GA_Gadget_FragGrenade > Max Charges | 2 |
| キャンセルボタン | GA_Gadget_FragGrenade > Cancel Input Tag | InputTag.Weapon.Aim |

---

## 4. よくある問題

| 症状 | 原因と対処 |
|---|---|
| 投げた瞬間に手元で爆発する | 本人以外の何か(手に持った物など)に当たっている。Draw Debug で位置を確認し、その物の Collision を見直す |
| 予測線と違う所に落ちる | `Gravity` が 1 以外になっている / 投げ物の Projectile Movement の値を Blueprint で直接変えている |
| ダメージが入らない | 相手に AbilitySystemComponent がない / 味方になっている / 壁(WorldStatic)に遮られている / Effect Radius が 0 |
| 同じ人に 2 回ダメージが入る | `ApplyGadgetEffect` で人ごとにまとめる処理(`ClosestDistances`)を消していないか |
| クライアントで爆発の見た目が出ない | `OnExploded` に何もつないでいない。または Multicast を Unreliable にした |
| Draw Debug の表示がクライアントに出ない | 仕様。爆発の計算はサーバーだけなので、ホストの画面にだけ出る |
| ボタンを押しても何も起きない | 個数が 0 / Default Gadget に入っていない / Input Tag が `InputTag.Gadget` ではない |

---

## 5. 次にやること(今回はやっていない)

- **予測線の見た目**: 今はデバッグ線(配布用ビルドでは消える)。GA_Gadget_FragGrenade で `Draw Debug Preview` を OFF にし、Blueprint で `On Aim Updated` を実装して Spline Mesh や Niagara で描く
- **飛んでいる物の見た目のカクつき**: クライアントでは位置が通信の間隔でしか届かないので、速いとカクつく。クライアント側でも ProjectileMovement を動かすと滑らかになる
- **時限式(投げてから数秒で爆発)**: `OnGadgetLanded` で `Detonate` を呼ぶ代わりに、`FuseTime` 秒後のタイマーで `Detonate` を呼ぶ。跳ね返らせるなら `bShouldBounce = true`
- **GameplayCue**: 爆発の見た目を GAS の GameplayCue(`GameplayCue.Gadget.FragGrenade.Explode`)にすると、見た目担当が C++ を触らずに差し替えられる
- **スタン・スモーク**: `AGadgetBase` の子クラスを作り、`OnGadgetLanded` と `ApplyGadgetEffect` を変える。投げる能力は `UGadgetThrowAbility` をそのまま使える

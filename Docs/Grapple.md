# 引き寄せ型グラップル

既存の `GA_Grapple` → `SavaGrappleAbility` を使用します。既存の `IA_Grapple`、
`DA_InputConfig`、`DA_AbilitySet`、`IMC_Default` と入力タグ `InputTag.Ablity.Grapple` は変更しません。
入力タグの綴りも互換性のためそのままです。

## 使い方

照準を静止した壁や地形に合わせ、`IMC_Default` で `IA_Grapple` に割り当て済みのキーを押し続けます。
引き寄せ中はA/Dなどの移動入力でカメラ基準の方向へ曲がれます。接続先は固定され、入力のうち引き寄せ方向に直交する成分だけが操舵に使われます。W/Sも視線とロープの角度によって操舵に寄与します。
グラップル中にSpace（既存ジャンプ入力）を押している間、軌道を上へ膨らませる力が加わります。離すと上向きの操作力をやめて滑らかに引き寄せへ戻ります。ロープは解除されません。真上・真下へ引かれている場合は上向きの接線成分がないため、追加浮力はありません。
キーを離すか、到着・タイムアウト・障害物で停止・死亡・能力キャンセルすると解除されます。
空振り・射程外・移動する物体では発動せず、クールダウンも消費しません。
引き寄せ中は重力を無効にし、解除後は落下に戻します。滑走や壁走りの古い状態へは戻しません。

## Blueprintで調整

`GA_Grapple` の Class Defaults → **Sava | Grapple**:

- Max Range: 3000 cm (30 m)
- Pull Speed: 2000 cm/s
- Steering Speed: 1200 cm/s (移動キーの横方向の操作速度)
- Max Lateral Offset: 700 cm (発射位置と到着位置を結ぶ直線から離れられる幅。上下も含む)
- Steering Response: 12 (大きいほど切り返しが速い)
- Jump Lift Speed: 1000 cm/s (ジャンプ長押しの上向き操作。0で無効)
- Max Pull Duration: 3秒 (上限時間。長距離設定では到着前に解除する場合があります)
- Surface Clearance: 20 cm (キャラのカプセルサイズに加えて壁から離す距離)
- Trace Channel: Visibility。対象にはこのチャンネルをBlockするコリジョンが必要です。
- Rope Class: SavaGrappleRope。Blueprint子クラスを設定すればメッシュ・マテリアルを変更できます。
- Sava | Cooldown: 標準2秒、Cooldown.Skill.Grapple。既存BPで上書き済みならその値が優先されます。

`On Grapple Started` / `On Grapple Finished` は自分とサーバー側で発生する演出用BPイベントです。
他プレイヤー向け演出は複製されるRope Class側で実装してください。
発動と終了の管理はC++で行うため、`Event ActivateAbility` に別の移動処理を書く必要はありません。

## 実装

独自の `FSavaGrappleRootMotionSource` を既存CharacterMovementへ適用します。位置の直接変更やテレポートはしません。
操舵は移動シミュレーション内で、標準のServerMove/保存移動に含まれるAccelerationから計算します。
ジャンプは既存の保存移動・圧縮フラグ `FLAG_Custom_1` を読み、追加RPCや未同期の入力状態を使いません。
接続先・速度・横幅などのソース設定もNetSerializeで同期され、補正後の再シミュレーションに対応します。
目標まで335cm以内では操舵を徐々に弱め、横入力を押し続けても到着できるようにします。
壁面法線と立ち状態のカプセルサイズから停止点を計算し、停止点の重なりも検査します。
移動中のコリジョンはCharacterMovementに任せ、障害物で止まった場合は0.25秒で解除します。

操作側は発動を予測し、サーバーは届いた目標の射程・視線方向・遮蔽物・静止物であることを再検証します。
キー解除にはGASのWaitInputReleaseを使用します。ロープはサーバーから他プレイヤーへ複製し、
本人には予測したロープを表示します。クライアント2台の実対戦・遅延ありの見た目は別途PIEで確認してください。

## 検証

自動テスト: `Sava.Grapple`。
既存BPの親・能力セット・入力対応、空振り、射程外、移動物体の拒否、発動、クールダウン、
ロープ生成/破棄、キャンセル、死亡、既存入力での解除、実際の引き寄せ/到着を検査します。

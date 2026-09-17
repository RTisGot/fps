# ゲーム内設定

Escで開き、下部の「閉じる」またはEscでゲームへ戻ります。

## 操作

- 一般：照準感度（0.100～3.000）、垂直視点の反転。
- 映像：ウィンドウ／ボーダーレス／フルスクリーン、解像度、FPS上限、VSync。
- 数値変更は一時データ `Pending` に保持し、「設定を適用」で反映・保存。
- 「閉じる」は未適用分を破棄。「初期値に戻す」も適用するまでは保存しません。
- 画面モード／解像度変更は15秒間のプレビュー。「維持する」で保存、時間切れ／「元に戻す」／Escで復元。
- ボーダーレスはデスクトップサイズに固定します。
- ビューポート内のPIEでは画面モード・解像度は変更できません。スタンドアロンで確認してください。
- 感度の数値はこのプロジェクト独自の倍率で、VALORANTの感度換算ではありません。

## 責務

`WBP_SettingsMenu` がUMGのレイアウト、配色、ホバー表示、背景ぼかし、`Intro`フェードアニメーションを保持します。デザイナーから直接編集できます。

`USavaSettingsWidget` はこのWidget Blueprintの親クラスです。`BindWidget`で部品を受け取り、数値の同期・誤通知防止・操作可否・確認状態を反映して入力イベントを通知します。タブと選択状態の見た目はBlueprintの `UpdateTabVisuals` / `UpdateStateVisuals` に通知します。設定の取得・適用・保存、Pawnへのアクセスは持ちません。

`USavaSettingsMenuController` は `Pending`、適用済み値、入力の検証、適用／キャンセル、15秒タイマーを管理します。タイマーはゲームの一時停止中にも進むCore Tickerです。

`USavaGameUserSettings` は適用済みの設定とGameUserSettings.iniへの永続保存を担当します。

`AsavaCharacter` はEsc入力、WidgetとControllerの接続、ポーズと入力モードの切り替え、および適用済み感度を使った視点操作を担当します。

既存の `/Game/WBP/WBP_Setting` アセットは保存してあります。ゲーム中は新しい `/Game/WBP/WBP_SettingsMenu` を使います。

## デザイナーでの編集

コンテンツブラウザの `WBP/WBP_SettingsMenu` を開き、デザイナーを選択してください。

- `MenuCanvas` → `ScreenLayers`：全面の背景と `WorldBlur`。
- `ResponsiveScale` → `AnimatedMenu`：1440×810の基準レイアウトを画面サイズに合わせて拡縮。
- `SettingsBody`：ヘッダー、タブ、設定ページ、フッター。
- `Pages`：`WidgetSwitcher`の0が一般、1が映像。詳細のActive Widget Indexで切り替えられます。
- `SensitivitySlider` / `SensitivityInput`：感度入力。各部品のStyleで配色・サイズを変更できます。
- `DisplayModeInput` / `ResolutionInput` / `FrameLimitInput`：映像設定。
- `HelpTitle` / `HelpText`：右側の説明欄。
- `ConfirmOverlay`：画面設定の確認。通常はCollapsedです。デザイナーでは階層の目アイコンで表示／非表示を切り替えて編集できます（実行時の表示とは別）。Visibilityを変更して確認した場合は必ずCollapsedへ戻してください。
- アニメーションパネルの `Intro`：0.25秒のフェードイン。

### Blueprintで調整する場所（改善版）

- `Settings_TabVisuals` グラフ：タブ切替、下線、タブの色、右側の説明文。通常のUMGノードなので演出を追加できます。
- `Settings_StateVisuals` グラフ：反転／VSyncの選択状態と未適用表示。毎フレームではなく、状態が変わったときに呼ばれます。
- **クラスのデフォルト → Settings → Appearance**：`SelectedTint` / `IdleTint` を共有するため、選択色をまとめて変更できます。
- **クラスのデフォルト → Settings → Text**：一般／映像の説明文、未適用・適用済みメッセージ、確認の説明。`CountdownFormat` 内の `{Seconds}` は残してください。
- `WorldTint` の **Brush Color → A**：背景の不透明度。改善時の基準は `0.65`（低くすると後ろが見える）。
- `WorldBlur` の **Blur Strength**：背景ぼかし。基準は `2.5`。

背景だけを半透明にしています。ルートや `SettingsBody` の Render Opacity を下げると文字まで薄くなるため、透明度は `WorldTint` で調整してください。見出し・設定行・右側の説明欄は読みやすさを優先しています。

名前はC++の `BindWidget` と対応しています。接続済み部品の名前・種類を変える場合は親クラス側も更新してください。配置・スタイル・フォント・余白はDesignerで、状態に応じた色と説明文は上記のグラフ／クラスのデフォルトで調整します。数値同期・安全性に関する処理とControllerの適用結果メッセージはC++側に残っています。

`savaEditor` モジュールの `SavaBuildSettingsMenuCommandlet` は初回アセット作成用です。作成後はデザイナーで編集してください。通常実行では既存アセットの上書きは行いません。`-RepairFonts` はこのアセットのフォント参照だけをEngineのRobotoアセットへ移行する修復オプションです（レイアウトは維持）。このモジュールはパッケージ版に含まれません。

`SavaUpgradeSettingsMenuCommandlet` は既存アセットへ上記のBlueprintグラフと読みやすい半透明スタイルを追加する一度限りの移行です。既存グラフを上書きしません。`-ValidateOnly` ではコンパイル確認だけを行い、保存しません。通常のデザイン調整では再実行せず、Designerを使用してください。

## 検証

自動テスト `Sava.Settings.PendingIsolation` は、未適用値の分離、キャンセル、初期値へのリセット、範囲制限、NaN、PIE中の画面変更抑止を検証します。

`Sava.Settings.BlueprintPresentation` は、表示イベントが実際のBlueprintコードを持つこと、背景だけが半透明で文字のRender Opacityが維持されていること、保存可能なフォントとIntroアニメーションを検証します。

UMG移行後の再実行も成功しています（`Saved/Logs/SettingsDesigner.log`）。

表示・操作の確認：Esc → 感度変更 → 閉じる → 再度Esc（元の値）、感度変更 → 適用 → 閉じる（ゲーム操作復帰）、映像タブで解像度変更 → 適用 → 確認／時間切れを確認します。

2026-09-08の実機確認（UE 5.5、スタンドアロン）：日本語表示、一般／映像タブ、感度の未適用変更の破棄、閉じる／Esc、VSyncの適用・ini保存、1280×720→1600×900の変更と15秒後の自動復元を確認済み。テスト後のVSyncは元の値に復元しました。アセット再コンパイルはエラー・警告ともに0件（`Saved/Logs/RepairSettingsFonts.log`）。パッケージ版と他のディスプレイ環境は未検証です。

2026-09-17の改善版確認（UE 5.5、スタンドアロン）：半透明背景・弱いぼかし・文字の読みやすさ、Blueprintによるタブ切替と選択色、感度スライダーと数値の同期、未適用の感度／VSync変更の破棄、Esc／閉じるでの復帰を確認しました。1280×720→1600×900の確認ダイアログと秒数更新、15秒後の自動復元も確認済みです。既存の保存済み設定値は維持しています。

C++のEditorビルド成功、アセットのコンパイルはエラー・警告0件（`Saved/Logs/UpgradeSettingsUI.log`）。上記の自動テスト2件は成功（`Saved/Logs/SettingsImprovedTests.log`）、実機操作のログは `Saved/Logs/SettingsImprovedUI.log` です。パッケージ版と他のディスプレイ環境は引き続き未検証です。

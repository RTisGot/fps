# fps

Unreal Engine 5.5 のファーストパーソン・プロジェクトです。UE内のプロジェクト名とC++モジュール名は `sava` です。

## 開発環境

- Unreal Engine 5.5
- Visual Studio 2022（C++によるゲーム開発、Windows SDK）
- Git / Git LFS

## セットアップ

```powershell
git lfs install
git clone https://github.com/RTisGot/fps.git
cd fps
git lfs pull
```

`sava.uproject` を右クリックしてVisual Studioのプロジェクトファイルを生成し、`Development Editor / Win64` でビルドしてください。その後 `sava.uproject` をUE 5.5で開きます。

`Binaries`、`Intermediate`、`Saved`、`DerivedDataCache`、IDEのローカル設定は生成物のため管理対象外です。`.uasset` と `.umap` はGit LFSで管理しています。

Android File Serverは無効化してあり、認証トークンはリポジトリに含めていません。Android向けの転送が必要になった場合は、ローカル環境で別途設定してください。

## ゲーム内設定

Escキーで設定画面を開閉します。感度、視点反転、画面モード、解像度、FPS上限、VSyncを変更できます。未適用値はControllerの `Pending` に保持し、UIから適用・保存処理を分離しています。

- UI：`Content/WBP/WBP_SettingsMenu.uasset`（Widget Blueprint）
- 表示と入力：`Source/sava/SavaSettingsWidget.cpp`
- 設定の制御：`Source/sava/SavaSettingsMenuController.cpp`
- 永続化：`Source/sava/SavaGameUserSettings.cpp`

デザインの編集箇所や検証内容は [設定画面のドキュメント](Docs/SettingsMenu.md) を参照してください。

## 自動テスト

UEエディタのAutomationから `Sava.Settings` を実行できます。

- `Sava.Settings.PendingIsolation`
- `Sava.Settings.BlueprintPresentation`

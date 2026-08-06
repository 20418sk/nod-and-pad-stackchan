# Nodding StackChan 設計資料

## 目的

話の意味や感情を推測せず、「相手が話している」という確度の高い信号だけに身体反応を返します。内容を理解したふりをせず、録音もしない、そばで聞いてくれる小さな伴侶を目指します。

対象は製品版M5Stack StackChan K151です。公式ファームの顔となでなで表現を土台に、音声の開始・終了検出、非ブロッキングなうなずき、睡眠表現を追加しています。

## 構成

```text
内蔵デュアルマイク
  └─ AudioDetector
       ├─ ListenerStateMachine ─ EndNodPlanner ─ MotionController ─ 上下サーボ
       └─ AudioDirectionEstimator ─ デバッグ表示のみ

頭部3ゾーンタッチ
  └─ HeadPetGestureDetector ─ HeadPetController ─ 表情・上下サーボ

LCD ─ FaceRenderer
RGB LED ─ 発話中は緑 / 頭部反応中は桃色
```

- `AudioDetector`: ステレオPCM取得、モノラルRMS、左右RMS、平滑化、ノイズフロア、動的しきい値、較正。
- `AudioDirectionEstimator`: 最大±3サンプルの到来時間差から左右を推定する純粋ロジック。サーボには接続しません。
- `ListenerStateMachine`: 発話開始・終了、最小発話時間、反応間隔、睡眠を扱う純粋ロジック。
- `EndNodPlanner`: 回数、最下点、速度、保持時間を安全範囲内で決める純粋ロジック。
- `MotionController`: 角度を二重に制限し、固定長の動作列を非ブロッキング実行。サーボ中と直後はマイク判定を抑制。
- `HeadPetGestureDetector`: 隣接2ゾーンの移動と40～400 msの単発接触を判別。
- `HeadPetController`: 装飾時間と、リリースから2秒後の復帰を管理。
- `FaceRenderer`: 公式デフォルトスキン由来の表情と、瞬き・睡眠・余韻を画面外バッファへ描画。

## 状態遷移

```text
STARTUP → IDLE → SPEECH_CANDIDATE → LISTENING → END_CANDIDATE
            ↑          │                 ↑             │
            └──────────┘                 └─────────────┘
                                                    ↓
                           COOLDOWN ← REACTING ← 発話確定

IDLE → SLEEPING → 声を検出 → IDLE
```

開始しきい値を120 ms連続で超えると発話を開始します。終了しきい値を600 ms連続で下回ると終了します。200 ms未満は反応しません。反応後は1200 msのクールダウン、反応開始同士は最低3000 ms空けます。

## うなずき計画

すべて0.1°単位です。通常姿勢は200（20°）、発話中は168（16.8°）まで浅く動きます。終了時は50～130（5～13°）を最下点に選び、必ず200へ戻ります。

`EndNodPlanner`はハードウェア乱数そのものを受け取らず、乱数値の集合から計画を作ります。この分離により、PC上で大量の入力に対して次を検証できます。

- 回数は1～2回
- 深い5～7°は必ず1回
- 最下点、速度、保持時間が規定範囲内
- 復帰速度は下降速度より遅い
- 完全に同じ計画を連続させない

`MotionController`でも角度を作品範囲5～72°と公式範囲5～85°の両方へクランプします。

## 自己音と接触音の抑制

サーボ動作時間と動作後650 msは発話判定を止めます。頭部はジェスチャー成立前の最初の接触から、最後の接触またはリリース後900 msまで、発話判定・方向推定・ノイズ学習を止めます。録音処理自体は継続し、抑制中の古いバッファを後から判定しません。

## カメラと音方向追尾を採用しない理由

試作ではK151カメラの画素形式変換、公式ESP-DL顔検出、ステレオマイク方向の多数決と横サーボ追尾を評価しました。しかし実機では顔検出の再現性が不足し、音方向も室内反射や声の周波数成分で揺れました。

誤って相手と逆を向く動作は「寄り添って聞く」という体験を大きく損ないます。そのため製品版では実験コードとモデルを削除し、カメラを一切初期化しません。音方向は技術診断用の`DIR:L/C/R/?`表示だけを残し、横サーボは正面0°を維持します。将来再検討する場合も、十分な実機データと独立した安全検証を前提に別ブランチで行います。

## プライバシー

- 音声は短いRAMバッファでレベル計算後に破棄
- 音声・音量履歴・画像を永続化しない
- 音声や画像をシリアルへ出さない
- Wi-Fi、BLE、HTTP、クラウド、音声認識を使用しない
- 通常版はカメラを初期化しない

## 起動時の安全確認

起動時に`SERVO TESTING`と表示して現在角を読みます。起動時のサーボテストは読み取り専用で、電源投入直後に補正移動を行いません。サーボ音抑制後は`SERVO TEST COMPLETE!`で停止し、利用者のタップ後に`MIC CALIBRATION / PLEASE BE QUIET (3 SEC)`を表示しながら3秒の環境音較正を行います。20ms単位で最大128個のRMSを保持し、直近最大約2.56秒分の中央値をノイズ基準にします。

較正後は`ALL TESTS COMPLETE!`で停止し、画面タップごとに`YOUR PRIVACY`（NO RECORDING / NO SPEECH ANALYSIS / LOCAL PROCESSING ONLY）、`READY TO LISTEN`を表示します。最後の`TAP TO START`までは発話判定・方向推定・ノイズ学習を止め、説明中の声やタップ音を誤って反応へ使いません。画面上の文章とエラー名は英語へ統一しています。失敗時は追加動作を止め、トルク解放を要求し、エラー顔と赤LEDを表示します。BSPの自動トルク解放も明示的に有効化します。

## 自動テスト

PlatformIOの`native`環境で、状態遷移、左右方向推定、うなずき計画の安全範囲、頭部ジェスチャー、接触音ガードを検証します。ハードウェア固有のマイクゲイン、筐体音響、サーボ実角度、個体較正は実機試験が必要です。

時間差はすべて符号なし減算で判定し、約49.7日ごとの`millis()`周回をまたげる設計です。

## 公式資料

- [StackChan K151](https://docs.m5stack.com/en/StackChan)
- [StackChan Arduino Quick Start](https://docs.m5stack.com/en/arduino/stackchan/program)
- [StackChan Servo](https://docs.m5stack.com/en/arduino/stackchan/servo)
- [StackChan MIC](https://docs.m5stack.com/en/arduino/stackchan/mic)
- [m5stack/StackChan-BSP](https://github.com/m5stack/StackChan-BSP)
- [m5stack/StackChan](https://github.com/m5stack/StackChan)

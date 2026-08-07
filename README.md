# Nod & Pat Stack-chan

[English](#english) | [日本語](#japanese)

## English

### A Standalone Companion That Responds Without Speech Recognition

**No audio files. No speech recognition. No cloud service. Just nods, faces, and touch.**

This project began with a simple wish:

> I wanted a cute companion I could talk to freely, even about secrets and everyday frustrations.

Nod & Pat Stack-chan is a small companion for the M5Stack StackChan K151. The companion detects when a person starts and stops speaking. The companion responds with nods, facial expressions, light, and reactions to touch.

The firmware does not recognize words or generate replies. This design is intentional. The project asks a simple question:

> Does a companion need to understand our words to make us feel heard?

Because Stack-chan does not understand or store the content of a conversation, a person can choose to talk about private concerns or worries without turning the conversation into data for another service. This is a design goal, not a guarantee of safety in every situation.

The project is not against AI. AI can be useful when a product needs language understanding. This project is a counterpoint: language understanding is not necessary for this quiet, physical interaction.

### Design Principles

- **Human-centered:** Small nonverbal reactions are more important than a large feature list.
- **Privacy by design:** The firmware does not create audio files or conversation history.
- **Data minimization:** The firmware processes only the signal values required for the current reaction.
- **Standalone operation:** After firmware installation, everyday use needs no phone, account, Wi-Fi setup, cloud setup, or external computer.
- **Predictable behavior:** Unreliable camera tracking and sound-direction servo tracking are not part of the current firmware.

### Privacy by Design

The internal microphones provide stereo PCM samples to three 20 ms RAM buffers. The firmware overwrites these buffers continuously. The firmware uses the samples only to calculate sound level, speech timing, and a diagnostic left/right estimate.

- No audio files or conversation history are created.
- No speech recognition or transcription is performed.
- No word meaning, topic, or emotion is analyzed.
- No camera is initialized.
- No Wi-Fi, Bluetooth, BLE, HTTP, or cloud API is used by the application.
- No conversation data is sent to an external service.
- No audio, level history, or image is written to microSD, Flash, or NVS.
- Serial diagnostics contain state names and numeric signal values, not raw audio or words.

The M5Stack hardware and its libraries can support other functions. This application does not initialize or use those communication and camera functions.

### Standalone and Everyday Setup

The current prototype requires one firmware installation with PlatformIO and a USB cable. After installation, daily operation is local to Stack-chan. The startup guide asks the user to tap the screen and complete a three-second room-noise calibration. No network, account, mobile application, or cloud configuration is required.

### Current Features

| State or input | Current response |
|---|---|
| Idle | Blinking, subtle breathing motion on the display, and small gaze changes |
| Speech starts | Listening face; the servos do not move at speech start |
| Speech continues | Green LED and listening face; no mid-speech nod |
| Speech ends after at least 200 ms | One or two varied nods, then a return to the 20° pitch home position |
| Sound shorter than 200 ms | No reaction |
| Head swipe across two adjacent zones | Happy face, one heart, blush, pink LED, and a small upward pitch motion |
| Short head touch | Happy face, one heart, blush, and pink LED; no servo motion |
| Short left or right screen tap | Happy response and a 7° yaw step toward the touched side, limited to ±45° |
| Short center screen tap | Happy response without servo motion |
| 60 seconds of silence | Sleep face, 8° pitch, and a repeating one-to-three `Z` indicator |

The speech detector requires 120 ms above the start threshold. Speech ends after 600 ms below the end threshold. A completed speech reaction uses a randomized pitch target from 5° to 11°. A target from 5° to 7° always uses one nod. Other targets can use one or two nods. Every nod returns to 20°.

The stereo microphone direction estimate appears only as `DIR:L/C/R/?` in the optional diagnostic display. Room reflections can make the estimate unstable. The estimate does not control the yaw servo. Screen-touch yaw control is independent of microphone direction.

### How It Works

```text
Internal stereo microphones
  -> AudioDetector
     -> ListenerStateMachine -> EndNodPlanner -> MotionController -> pitch servo
     -> AudioDirectionEstimator -> diagnostic display only

Three-zone head touch
  -> HeadPetGestureDetector -> HeadPetController -> face, LED, and pitch servo

LCD touch -> Happy response and optional yaw step
LCD -> FaceRenderer
```

The detector calculates RMS sound level. It uses a calibrated noise floor and dynamic thresholds. The firmware does not classify words. Servo motion and head contact suppress microphone decisions for a short period so that mechanical and touch sounds do not create false speech reactions.

### Hardware

- M5Stack StackChan K151 production model
- StackChan Core with ESP32-S3
- Internal stereo microphones
- LCD touchscreen
- Pitch and yaw servos
- Three-zone head touch sensor
- RGB LED

Other DIY Stack-chan builds, Core2 models, and CoreS3 Lite models are outside the current target.

### Software and Libraries

- Arduino framework on `pioarduino/platform-espressif32 55.03.37`
- M5Stack StackChan-BSP
- M5Unified
- M5GFX
- Unity for native logic tests

Versions and source revisions are pinned in [`platformio.ini`](platformio.ini). License details and upstream source references are in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

### Build and Install

Requirements:

- Visual Studio Code with the PlatformIO IDE extension, or PlatformIO Core
- A data-capable USB-C cable
- Internet access for the first dependency download
- M5Stack StackChan K151

Build the K151 firmware:

```powershell
pio run -e stackchan_k151
```

Run the pure logic tests:

```powershell
pio test -e native
```

Upload only after checking the build and keeping the moving parts clear:

```powershell
pio run -e stackchan_k151 -t upload --upload-port <PORT>
```

The build and test steps do not upload firmware automatically. If `pio` is not available in the terminal, use the PlatformIO buttons in Visual Studio Code or run the installed PlatformIO Core executable.

### How to Use

1. Power on Stack-chan on a stable surface.
2. Wait for `SERVO TEST COMPLETE!`, then tap the screen.
3. Keep the room quiet during the three-second microphone calibration.
4. Read each startup page and tap `TAP TO START`.
5. Speak normally, pat the head, swipe across two adjacent head zones, or tap the screen.

Controls after startup:

- Short left or right screen tap: Happy response and one 7° yaw step, up to ±45°
- Short center screen tap: Happy response only
- Center long press for about 1.2 seconds: microphone recalibration
- Upper-right long press for about 2 seconds: diagnostic display on or off
- Head contact for 40–400 ms: short-touch Happy response
- Head swipe across two adjacent zones: patting response with a small pitch motion

The yaw servo returns slowly to 0° after the interaction. The working pitch range is 5°–72°. The working yaw range is ±45°. The firmware also clamps commands to the wider official BSP limits.

### Project Structure

```text
include/                 configuration and module interfaces
src/                     firmware implementation
test/test_logic/         hardware-independent logic tests
docs/design.md           architecture and design decisions
docs/tuning-guide.md     K151 hardware verification guide
platformio.ini           pinned build environments and dependencies
THIRD_PARTY_NOTICES.md   licenses and upstream sources
```

### Current Limitations

- The firmware detects sound level and timing. It cannot identify a speaker or understand speech.
- The sound-direction estimate is diagnostic only and can change with room reflections.
- The camera is not used. There is no face tracking.
- Microphone gain, room acoustics, touch response, and physical servo angles require verification on each K151 unit.
- Firmware installation is required before everyday standalone use.

### Future Work

Camera face tracking and automatic sound-direction servo tracking were evaluated in prototypes. The results were not reliable enough on the K151. These functions are not included in the current firmware. Any future evaluation should use a separate development branch, real-device test data, and the existing servo safety limits.

Future work may simplify firmware installation and startup. Commercial product distribution is outside the current project scope.

### M5Stack Global Innovation Contest 2026

This project is prepared for the [M5Stack Global Innovation Contest 2026](https://m5stack.com/global-innovation-contest-2026). The project uses the M5Stack controller, microphones, touch sensors, display, LED, and servos to explore minimal and privacy-conscious human-robot interaction.

### License

- Project code: [MIT License](LICENSE)
- Third-party libraries and adapted upstream work: [Third-Party Notices](THIRD_PARTY_NOTICES.md)

The face proportions, expressions, and patting behavior are adapted from the official StackChan firmware for Arduino and M5GFX. The upstream license notice is preserved.

---

<a id="japanese"></a>

## 日本語

### 音声認識を使わずに応える、スタンドアロンの小さな伴侶

**音声ファイルなし。音声認識なし。クラウドなし。あるのは、うなずきと表情と触れ合いです。**

本作品は、こんな素朴な思いから始まりました。

> 秘密や日々の愚痴まで、気兼ねなく話せるかわいい存在が欲しかった。

Nod & Pat Stack-chan（日本語愛称：きいてるﾁｬﾝ）は、製品版M5Stack StackChan K151向けの小さな伴侶です。人が話し始めたことと話し終えたことを検出し、うなずき、表情、光、タッチ反応で応えます。

このファームウェアは言葉を認識せず、返答も生成しません。これは意図した設計です。本作品は、次の問いから始まりました。

> 人が「話を聞いてもらった」と感じるために、伴侶は本当に言葉を理解する必要があるのでしょうか。

会話の内容を理解・保存しないからこそ、秘密や悩みを外部サービスのデータに変えず、自分の意思で話せる余地を作ります。これは設計目標であり、あらゆる状況での安全を保証する表現ではありません。

本作品はAIを否定するものではありません。言語理解が必要な製品ではAIが役立ちます。一方、この静かな身体的コミュニケーションには、言語理解を必須としない選択肢もあると考えました。

### 設計原則

- **人間中心：** 機能数より、小さな非言語反応を重視します。
- **Privacy by Design：** 音声ファイルや会話履歴を作りません。
- **データ最小化：** 現在の反応に必要な信号値だけを処理します。
- **スタンドアロン動作：** ファームウェア導入後の日常利用には、スマートフォン、アカウント、Wi‑Fi設定、クラウド設定、外部コンピュータが不要です。
- **予測しやすい動作：** 信頼性が不足したカメラ追尾と音方向サーボ追尾は、現在のファームウェアに含めません。

### Privacy by Design

内蔵マイクのステレオPCMは、RAM上の20 msバッファ3個へ入り、常に上書きされます。PCMから計算するのは、音量、発話タイミング、診断用の左右推定だけです。

- 音声ファイルや会話履歴を作らない
- 音声認識や文字起こしを行わない
- 言葉の意味、話題、感情を解析しない
- カメラを初期化しない
- アプリからWi‑Fi、Bluetooth、BLE、HTTP、クラウドAPIを使用しない
- 会話データを外部サービスへ送信しない
- 音声、音量履歴、画像をmicroSD、Flash、NVSへ書き込まない
- シリアル診断には状態名と数値信号だけを出し、生音声や言葉を出さない

M5Stack本体とライブラリには別の機能もありますが、本アプリは通信機能とカメラ機能を初期化・使用しません。

### スタンドアロンと日常利用の準備

現在の試作機には、PlatformIOとUSBケーブルを使った1回のファームウェア導入が必要です。導入後の日常利用はStack-chan単体で完結します。起動案内に従って画面をタップし、3秒の環境音較正を行います。ネットワーク、アカウント、スマートフォンアプリ、クラウドの設定は不要です。

### 現在の機能

| 状態・入力 | 現在の反応 |
|---|---|
| 待機 | 画面内の瞬き、わずかな呼吸表現、小さな視線移動 |
| 発話開始 | 傾聴顔。発話開始時はサーボを動かさない |
| 発話中 | 緑LEDと傾聴顔。発話中うなずきは行わない |
| 200 ms以上の発話終了 | 変化を付けた1～2回のうなずき後、上下20°へ戻る |
| 200 ms未満の音 | 反応しない |
| 頭部の隣接2ゾーンをスワイプ | Happy顔、ハート1個、照れ頬、桃色LED、小さな上向き動作 |
| 頭部へ短く接触 | Happy顔、ハート1個、照れ頬、桃色LED。サーボは動かさない |
| 画面左・右を短押し | Happy反応と、押した側への7°ずつの横向き。上限±45° |
| 画面中央を短押し | サーボ動作なしのHappy反応 |
| 60秒の無音 | 上下8°の寝姿勢と、1～3個を繰り返す`Z`表示 |

発話開始には開始しきい値を120 ms連続で超える必要があります。終了には終了しきい値を600 ms連続で下回る必要があります。発話終了後の最下点は5～11°から選びます。5～7°の深いうなずきは必ず1回です。それ以外は1回または2回です。最後は必ず20°へ戻ります。

デュアルマイクによる左右推定は、任意で表示できる診断画面の`DIR:L/C/R/?`だけに使います。室内反射で推定が揺れるため、横サーボには接続していません。画面タッチによる横向きは、マイク方向推定から独立しています。

### 仕組み

```text
内蔵デュアルマイク
  -> AudioDetector
     -> ListenerStateMachine -> EndNodPlanner -> MotionController -> 上下サーボ
     -> AudioDirectionEstimator -> 診断表示のみ

頭部3ゾーンタッチ
  -> HeadPetGestureDetector -> HeadPetController -> 表情、LED、上下サーボ

LCDタッチ -> Happy反応と任意の横向き
LCD -> FaceRenderer
```

発話検出には、RMS音量、較正したノイズ基準、動的しきい値を使います。言葉は分類しません。サーボ動作中と頭部接触中はマイク判定を短時間抑制し、機械音や接触音による誤反応を防ぎます。

### ハードウェア

- 製品版 M5Stack StackChan K151
- ESP32-S3搭載StackChan Core
- 内蔵デュアルマイク
- LCDタッチスクリーン
- 上下・横サーボ
- 頭部3ゾーンタッチセンサー
- RGB LED

一般的な自作版、Core2版、CoreS3 Lite版は現在の対象外です。

### ソフトウェアとライブラリ

- `pioarduino/platform-espressif32 55.03.37`上のArduinoフレームワーク
- M5Stack StackChan-BSP
- M5Unified
- M5GFX
- 純粋ロジックテスト用Unity

バージョンと取得元リビジョンは[`platformio.ini`](platformio.ini)へ固定しています。ライセンスと移植元は[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)に記載しています。

### ビルドと導入

必要なもの：

- Visual Studio CodeとPlatformIO IDE拡張機能、またはPlatformIO Core
- データ通信対応USB-Cケーブル
- 初回の依存取得に使うインターネット接続
- M5Stack StackChan K151

K151ファームウェアをビルドします。

```powershell
pio run -e stackchan_k151
```

純粋ロジックテストを実行します。

```powershell
pio test -e native
```

ビルド確認後、可動部の周囲を空けてから、明示的に書き込みます。

```powershell
pio run -e stackchan_k151 -t upload --upload-port <PORT>
```

ビルドとテストだけでは実機へ自動書き込みしません。端末で`pio`が見つからない場合は、VS CodeのPlatformIOボタンを使うか、インストール済みPlatformIO Coreの実体を指定してください。

### 使い方

1. Stack-chanを安定した場所で起動します。
2. `SERVO TEST COMPLETE!`まで待ち、画面をタップします。
3. 3秒のマイク較正中は周囲を静かにします。
4. 起動案内を1画面ずつ読み、`TAP TO START`を押します。
5. 普通に話す、頭へ短く触れる、隣接2ゾーンをなでる、または画面をタップします。

起動後の操作：

- 画面左・右を短押し：Happy反応と1回7°の横向き。最大±45°
- 画面中央を短押し：Happy反応のみ
- 画面中央を約1.2秒長押し：マイク再較正
- 画面右上を約2秒長押し：診断表示のON／OFF
- 頭部へ40～400 ms接触：短接触Happy反応
- 頭部の隣接2ゾーンをスワイプ：小さな上下動作を伴うなでなで反応

横サーボは余韻後にゆっくり0°へ戻ります。作品内の上下範囲は5～72°、横範囲は±45°です。命令は、より広い公式BSP範囲に対しても二重に制限します。

### プロジェクト構成

```text
include/                 設定値と各モジュールのインターフェース
src/                     ファームウェア実装
test/test_logic/         ハードウェア非依存の純粋ロジックテスト
docs/design.md           構成と設計判断
docs/tuning-guide.md     K151実機確認ガイド
platformio.ini           ビルド環境と固定依存
THIRD_PARTY_NOTICES.md   ライセンスと移植元
```

### 現在の制限

- 音量とタイミングは検出できますが、話者の識別や発話内容の理解はできません。
- 音方向推定は診断専用で、室内反射により揺れる場合があります。
- カメラは使用せず、顔追尾はありません。
- マイク感度、室内音響、タッチ反応、サーボ実角度はK151個体ごとの実機確認が必要です。
- 日常的なスタンドアロン利用の前に、ファームウェア導入が必要です。

### Future Work

カメラ顔追尾と音方向サーボ追尾は試作で評価しましたが、K151で十分な信頼性を得られませんでした。現在のファームウェアには含めていません。将来再評価する場合は、別の開発ブランチ、実機データ、現在のサーボ安全制限を使います。

将来はファームウェア導入と起動手順の簡略化を検討します。製品化や商用提供は現在のプロジェクト範囲に含みません。

### M5Stack Global Innovation Contest 2026

本作品は[M5Stack Global Innovation Contest 2026](https://m5stack.com/global-innovation-contest-2026)への応募用に整備しています。M5Stackのコントローラ、マイク、タッチセンサー、画面、LED、サーボを使い、最小限でプライバシーに配慮した人とロボットの関わり方を探ります。

### ライセンス

- 本プロジェクト：[MIT License](LICENSE)
- 第三者ライブラリと移植元：[Third-Party Notices](THIRD_PARTY_NOTICES.md)

顔の比率、表情、なでなで反応は公式StackChanファームウェアを参照し、Arduino／M5GFX向けに移植しています。移植元のライセンス表示を維持しています。

# Third-Party Notices

本プロジェクトは次の第三者ソフトウェアを利用または移植元として参照しています。ライブラリ依存は`platformio.ini`、移植元は下表のGitコミットで固定しています。各著作権は各権利者に帰属します。

| ソフトウェア | 固定リビジョン | ライセンス | 用途 |
|---|---|---|---|
| [m5stack/StackChan firmware](https://github.com/m5stack/StackChan/tree/main/firmware) | `b72b3ede38b32d54f0b6ba51c62cfcef2ec3ae1e` | MIT | 製品版公式default skinの顔比率・表情とHeadPetModifierのなでなで反応をM5GFX/Arduinoへ移植 |
| [m5stack/StackChan-BSP](https://github.com/m5stack/StackChan-BSP) | `f7ed40e6f5d9a1d08440cb926f3a0865b81882f8` (1.1.0) | MIT | K151本体、上下サーボ、RGB LED、表示のBSP |
| [m5stack/M5Unified](https://github.com/m5stack/M5Unified) | `28c6a1f03d05f3ff3216adc89b09c9328448ad53` (0.2.11) | MIT | CoreS3初期化、内蔵デュアルマイク、LCD、タッチ |
| [m5stack/M5GFX](https://github.com/m5stack/M5GFX) | `03565ccc96cb0b73c8b157f5ec3fbde439b034ad` (0.2.18) | FreeBSD | 図形と日本語テキスト描画 |
| [m5stack/M5Unit-NFC](https://github.com/m5stack/M5Unit-NFC) | `93745b547364f310cd64b5155a870103a7800a5d` (0.1.0) | MIT | BSPマニフェスト依存。固定するがコンパイル対象外 |
| [m5stack/M5UnitUnified](https://github.com/m5stack/M5UnitUnified) | `bf711f370047cf16355b00005450ef615fab36e2` (0.5.5) | MIT | M5Unit-NFCの固定依存。コンパイル対象外 |
| [m5stack/M5HAL](https://github.com/m5stack/M5HAL) | `0f06f9d3134706ce030fd5515601cce65a267233` (0.1.2) | MIT | M5UnitUnifiedの固定依存。コンパイル対象外 |
| [m5stack/M5Utility](https://github.com/m5stack/M5Utility) | `301a6b5c6413875e1dd80b027e0639921972b433` (0.2.0) | MIT | M5UnitUnified/M5HALの固定依存。コンパイル対象外 |
| [crankyoldgit/IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) | `8833210f93073b8f732130cf935e18693f93641b` (2.9.0) | LGPL-2.1 | BSPマニフェスト依存。固定するがコンパイル対象外 |
| [ThrowTheSwitch/Unity](https://github.com/ThrowTheSwitch/Unity) | 2.6.1 | MIT | native純粋ロジックテスト |

## M5Stack公式ファームウェアからの移植部分

`src/FaceRenderer.cpp`、`src/HeadPetController.cpp`、`src/main.cpp`のなでなで処理は、上表の固定コミットにある次の実装を参照しています。

- `firmware/main/stackchan/avatar/skins/default/`
- `firmware/main/stackchan/avatar/decorators/heart.cpp`
- `firmware/main/stackchan/avatar/decorators/shy.cpp`
- `firmware/main/stackchan/modifiers/head_pet.h`
- `firmware/main/hal/hal_head_touch.cpp`

MIT License

Copyright (c) 2026 M5Stack Technology CO LTD

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

ビルド基盤には、Arduino-ESP32 3.3.7を含む[pioarduino/platform-espressif32 55.03.37](https://github.com/pioarduino/platform-espressif32/releases/tag/55.03.37)を使用します。これは開発・ビルド環境であり、本リポジトリへ再配布していません。

各ライセンス全文は、PlatformIOが取得した`.pio/libdeps/stackchan_k151/`以下および各リンク先の`LICENSE`ファイルで確認できます。本プロジェクト自体のライセンスは[LICENSE](LICENSE)です。

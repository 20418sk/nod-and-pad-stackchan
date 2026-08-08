# Third-Party Notices

This document records upstream software used by, or adapted in, Nod & Pat Stack-chan. It does not change any upstream license.

## M5Stack Official StackChan Firmware

- Project: M5Stack official StackChan firmware
- Source: <https://github.com/m5stack/StackChan>
- Compared revision: [`b72b3ede38b32d54f0b6ba51c62cfcef2ec3ae1e`](https://github.com/m5stack/StackChan/tree/b72b3ede38b32d54f0b6ba51c62cfcef2ec3ae1e)
- Relevant license file: `firmware/LICENSE`
- License: MIT
- Copyright: Copyright (c) 2026 M5Stack Technology CO LTD

### Adapted Portions

| Local code or behavior | Official source used as the reference | Relationship |
|---|---|---|
| `src/FaceRenderer.cpp`: eye positions and base size | [`firmware/main/stackchan/avatar/skins/default/eyes.cpp`](https://github.com/m5stack/StackChan/blob/b72b3ede38b32d54f0b6ba51c62cfcef2ec3ae1e/firmware/main/stackchan/avatar/skins/default/eyes.cpp) | Ports the default 320x240 geometry and selected expression values from LVGL to M5GFX. |
| `src/FaceRenderer.cpp`: mouth position and weight mapping | [`firmware/main/stackchan/avatar/skins/default/mouth.cpp`](https://github.com/m5stack/StackChan/blob/b72b3ede38b32d54f0b6ba51c62cfcef2ec3ae1e/firmware/main/stackchan/avatar/skins/default/mouth.cpp) | Ports the official mouth position, minimum/maximum size, and radius interpolation. |
| Happy, Sleepy, and Error/Doubt-style faces | `eyes.cpp` and the official `Emotion` mapping | Uses official expression values as visual references; M5GFX drawing primitives are project-specific. |
| Heart and blush decoration | `avatar/decorators/heart.cpp`, `avatar/decorators/shy.cpp` | Adapts official colors and positions. The local heart and blush shapes are redrawn with M5GFX primitives. |
| Patting reaction | [`firmware/main/stackchan/modifiers/head_pet.h`](https://github.com/m5stack/StackChan/blob/b72b3ede38b32d54f0b6ba51c62cfcef2ec3ae1e/firmware/main/stackchan/modifiers/head_pet.h) | Adapts the Happy/decorator/motion/delayed-restore interaction. Local gesture detection, timing controller, and fixed safe pitch motion are project-specific. |
| Idle blink, breathing-like movement, and gaze | `modifiers/blink.h`, `breath.h`, and `idle_expression.h` | Adapts the behavior ideas. Local schedules, amplitudes, and rendering state are different. |

The local implementation is not a line-for-line copy of the official LVGL source. Attribution is retained because the official geometry, constants, decorator placement, and interaction design are recognizable in the result.

No official camera, network, AI-agent, cloud, speech-recognition, remote-control, or OTA implementation is copied into this repository.

## M5Stack StackChan-BSP

- Project: M5Stack StackChan Board Support Package for Arduino
- Source: <https://github.com/m5stack/StackChan-BSP>
- Pinned revision: [`f7ed40e6f5d9a1d08440cb926f3a0865b81882f8`](https://github.com/m5stack/StackChan-BSP/tree/f7ed40e6f5d9a1d08440cb926f3a0865b81882f8) (1.1.0)
- License: MIT
- Copyright: Copyright (c) 2026 M5Stack Technology CO LTD

`StackChan-BSP` is downloaded by PlatformIO. It supplies K151 initialization and APIs for the display, three-zone head-touch sensor, RGB LED, and pitch/yaw servos. The application uses these APIs from `src/main.cpp` and `src/MotionController.cpp`. The BSP implementation is not copied into this repository.

The project-specific motion sequencer, working limits, randomized nod plans, screen-touch yaw behavior, home/sleep poses, and microphone suppression are not supplied by the BSP.

## M5Unified and M5GFX

| Project | Pinned revision | License and copyright | Use in this project |
|---|---|---|---|
| [M5Unified](https://github.com/m5stack/M5Unified) | `28c6a1f03d05f3ff3216adc89b09c9328448ad53` | MIT; Copyright (c) 2021 M5Stack | Microphone, display, touch, and base-device APIs used through M5Unified and the BSP. |
| [M5GFX](https://github.com/m5stack/M5GFX) | `03565ccc96cb0b73c8b157f5ec3fbde439b034ad` | MIT; Copyright (c) 2021 M5Stack | Display and off-screen sprite rendering. |

Their source is downloaded as a dependency and is not copied into this repository. Their own repositories include their complete license notices.

## Other Declared Dependencies and Build Tools

| Project | Pinned revision or version | License | Relationship |
|---|---|---|---|
| [M5Utility](https://github.com/m5stack/M5Utility) | `301a6b5c6413875e1dd80b027e0639921972b433` (0.2.0) | MIT | Present in the BSP dependency set; excluded by `lib_ignore`. |
| [M5HAL](https://github.com/m5stack/M5HAL) | `0f06f9d3134706ce030fd5515601cce65a267233` (0.1.2) | MIT | Present in the BSP dependency set; excluded by `lib_ignore`. |
| [M5UnitUnified](https://github.com/m5stack/M5UnitUnified) | `bf711f370047cf16355b00005450ef615fab36e2` (0.5.5) | MIT | Present in the BSP dependency set; excluded by `lib_ignore`. |
| [M5Unit-NFC](https://github.com/m5stack/M5Unit-NFC) | `93745b547364f310cd64b5155a870103a7800a5d` (0.1.0) | MIT | Present in the BSP dependency set; excluded by `lib_ignore`. |
| [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) | `8833210f93073b8f732130cf935e18693f93641b` (2.9.0) | LGPL-2.1 | Present in the BSP dependency set; excluded by `lib_ignore`. |
| [Unity](https://github.com/ThrowTheSwitch/Unity) | 2.6.1 | MIT | Used only by the native pure-logic test environment. |

The excluded libraries are pinned so PlatformIO can resolve the BSP dependency set reproducibly. The `stackchan_k151` application does not link their code. Their upstream licenses remain with their respective copyright holders.

The build uses [pioarduino/platform-espressif32 55.03.37](https://github.com/pioarduino/platform-espressif32/releases/tag/55.03.37), including Arduino-ESP32 3.3.7. This is a downloaded development and build environment, not source vendored by this repository. Its components retain their own upstream licenses.

## MIT License Notice for the Adapted M5Stack Work

The following notice is reproduced from the official StackChan firmware and StackChan-BSP license files:

> MIT License
>
> Copyright (c) 2026 M5Stack Technology CO LTD
>
> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

## Project License Boundary

The root [`LICENSE`](LICENSE) covers the original project work under the MIT License and identifies its project contributors. That file does not remove or replace the M5Stack copyright and license notice above. Source files that substantially adapt official visual work also carry a file-level upstream attribution.

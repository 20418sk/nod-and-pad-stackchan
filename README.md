# Nod & Pat Stack-chan

[Japanese](README_ja.md)

## A Standalone Companion That Responds Without Speech Recognition

**No audio files. No speech recognition. No cloud service. Just nods, faces, and touch.**

This project began with a simple wish:

> I wanted a cute companion I could talk to freely, even about secrets and everyday frustrations.

Nod & Pat Stack-chan is a small companion for the M5Stack StackChan K151. The companion detects when a person starts and stops speaking. The companion responds with nods, facial expressions, light, and reactions to touch.

The firmware does not recognize words or generate replies. This design is intentional. The project asks a simple question:

> Does a companion need to understand our words to make us feel heard?

Because Stack-chan does not understand or store the content of a conversation, a person can choose to talk about private concerns or worries without turning the conversation into data for another service. This is a design goal, not a guarantee of safety in every situation.

The project is not against AI. AI can be useful when a product needs language understanding. This project is a counterpoint: language understanding is not necessary for this quiet, physical interaction.

## Design Principles

- **Human-centered:** Small nonverbal reactions are more important than a large feature list.
- **Privacy by design:** The firmware does not create audio files or conversation history.
- **Data minimization:** The firmware processes only the signal values required for the current reaction.
- **Standalone operation:** After firmware installation, everyday use needs no phone, account, Wi-Fi setup, cloud setup, or external computer.
- **Predictable behavior:** Unreliable camera tracking and sound-direction servo tracking are not part of the current firmware.

## Privacy by Design

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

## Standalone and Everyday Setup

The current prototype requires one firmware installation with PlatformIO and a USB cable. After installation, daily operation is local to Stack-chan. The startup guide asks the user to tap the screen and complete a three-second room-noise calibration. No network, account, mobile application, or cloud configuration is required.

## Current Features

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

## How It Works

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

## Hardware

- M5Stack StackChan K151 production model
- StackChan Core with ESP32-S3
- Internal stereo microphones
- LCD touchscreen
- Pitch and yaw servos
- Three-zone head touch sensor
- RGB LED

Other DIY Stack-chan builds, Core2 models, and CoreS3 Lite models are outside the current target.

## Software and Libraries

- Arduino framework on `pioarduino/platform-espressif32 55.03.37`
- M5Stack StackChan-BSP
- M5Unified
- M5GFX
- Unity for native logic tests

Versions and source revisions are pinned in [`platformio.ini`](platformio.ini). License details and upstream source references are in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

## Build and Install

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

## How to Use

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

## Project Structure

```text
include/                 configuration and module interfaces
src/                     firmware implementation
test/test_logic/         hardware-independent logic tests
docs/design.md           architecture and design decisions
docs/tuning-guide.md     K151 hardware verification guide
README_ja.md             Japanese documentation
platformio.ini           pinned build environments and dependencies
THIRD_PARTY_NOTICES.md   licenses and upstream sources
```

## Current Limitations

- The firmware detects sound level and timing. It cannot identify a speaker or understand speech.
- The sound-direction estimate is diagnostic only and can change with room reflections.
- The camera is not used. There is no face tracking.
- Microphone gain, room acoustics, touch response, and physical servo angles require verification on each K151 unit.
- Firmware installation is required before everyday standalone use.

## Future Work

Camera face tracking and automatic sound-direction servo tracking were evaluated in prototypes. The results were not reliable enough on the K151. These functions are not included in the current firmware. Any future evaluation should use a separate development branch, real-device test data, and the existing servo safety limits.

Future work may simplify firmware installation and startup. Commercial product distribution is outside the current project scope.

## M5Stack Global Innovation Contest 2026

This project is prepared for the [M5Stack Global Innovation Contest 2026](https://m5stack.com/global-innovation-contest-2026). The project uses the M5Stack controller, microphones, touch sensors, display, LED, and servos to explore minimal and privacy-conscious human-robot interaction.

## Official-Derived and Original Work

This is an independent contest project, not official M5Stack firmware. It is based on the standard K151 hardware and the official M5Stack software ecosystem. The following boundary is intentional:

**Adapted from the official M5Stack StackChan firmware:**

- `FaceRenderer.cpp` ports the default face proportions and geometry to M5GFX: eye positions, neutral eye size, mouth position, and the mouth size/radius interpolation.
- Happy, Sleepy, and Doubt-style expressions use official expression values as visual references.
- The heart and blush colors and positions follow the official Heart and Shy decorators.
- The patting experience follows the official `HeadPetModifier` concept: Happy expression, heart and blush, a small head motion, and delayed restoration after release.
- Idle blinking, breathing-like face movement, and small gaze changes are behavioral adaptations of the official modifier ideas. Their timing and rendering state machine are project-specific.

These parts are rewritten for Arduino/M5GFX and are not a line-for-line copy of the LVGL implementation. They are still identified as adapted work because the official geometry, values, placement, and interaction design remain recognizable.

**Used as upstream libraries:**

- `StackChan-BSP` provides K151 initialization and the display, head-touch, RGB LED, and servo APIs. This repository calls the BSP APIs but does not vendor or copy the BSP implementation.
- M5Unified and M5GFX provide microphone and graphics APIs.

**Implemented specifically for this project:**

- RMS sound-level measurement, calibrated dynamic thresholds, voice activity start/end timing, and the diagnostic stereo direction estimate
- The rule that speech content is not recognized, transcribed, semantically analyzed, stored, or sent
- Randomized one/two-nod planning, the non-blocking motion sequence, project safety clamps, and microphone suppression during mechanical or touch noise
- Two-zone head-swipe detection, short head-touch detection, screen-touch yaw steps, and the related interaction state machines
- The listening face, sleep transition and `Z` animation, wake transition, nod afterglow, startup guide, privacy screen, diagnostics, and off-screen rendering used to prevent flicker

The official firmware and `StackChan-BSP` source revisions, file-level mapping, copyright notice, and MIT license text are recorded in [Third-Party Notices](THIRD_PARTY_NOTICES.md).

## License

- Project code: [MIT License](LICENSE)
- Third-party libraries and adapted upstream work: [Third-Party Notices](THIRD_PARTY_NOTICES.md)

The project MIT license applies to original project work. It does not replace third-party copyright notices or licenses.

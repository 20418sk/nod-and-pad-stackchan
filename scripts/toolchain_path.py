"""PIOArduinoのWindowsツールチェーン配置差を安全に吸収する。"""

import os

from SCons.Script import SetOption

Import("env")  # type: ignore[name-defined]  # PlatformIO/SConsが提供する。

# C:\dev\hearing-chan内の中間ディレクトリ作成を安定させるため、
# VS CodeのBuildボタンから実行した場合も1ジョブへ固定する。
SetOption("num_jobs", 1)

platform = env.PioPlatform()  # type: ignore[name-defined]
package_dir = platform.get_package_dir("toolchain-xtensa-esp-elf")

if package_dir:
    candidates = (
        os.path.join(package_dir, "bin"),
        os.path.join(package_dir, "xtensa-esp-elf", "bin"),
    )
    executable_names = (
        "xtensa-esp32s3-elf-g++.exe",
        "xtensa-esp32s3-elf-g++",
    )
    for candidate in candidates:
        if any(os.path.isfile(os.path.join(candidate, name)) for name in executable_names):
            env.PrependENVPath("PATH", candidate)  # type: ignore[name-defined]
            break

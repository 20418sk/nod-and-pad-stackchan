"""Handle PIOArduino toolchain path differences on Windows."""

import os

from SCons.Script import SetOption

Import("env")  # type: ignore[name-defined]  # Provided by PlatformIO/SCons.

# Use one job so intermediate directory creation stays stable on Windows.
# Apply the same setting when the build starts from the VS Code button.
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

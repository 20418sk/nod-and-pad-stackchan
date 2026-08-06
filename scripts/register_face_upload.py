"""Register the separately packed ESP-DL model with esptool uploads."""

from pathlib import Path

Import("env")  # type: ignore[name-defined]  # Provided by PlatformIO/SCons.


build_dir = Path(env.subst("$BUILD_DIR"))  # type: ignore[name-defined]
packed_model = build_dir / "espdl_models" / "human_face_detect.espdl"

# This script runs before the platform's upload command is assembled, so the
# model partition is included only when the user explicitly selects upload.
env.Append(  # type: ignore[name-defined]
    FLASH_EXTRA_IMAGES=[("0xd10000", str(packed_model))]
)

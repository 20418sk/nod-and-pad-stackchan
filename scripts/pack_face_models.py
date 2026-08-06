"""Make PlatformIO build the official ESP-DL face-model partition image.

ESP-IDF's component registers this as a custom ALL target. PlatformIO imports
the normal firmware targets into SCons but does not execute that custom target,
so express the same dependency explicitly here.
"""

from pathlib import Path
import subprocess
import sys

Import("env")  # type: ignore[name-defined]  # Provided by PlatformIO/SCons.


project_dir = Path(env.subst("$PROJECT_DIR"))  # type: ignore[name-defined]
build_dir = Path(env.subst("$BUILD_DIR"))  # type: ignore[name-defined]
component_dir = project_dir / "managed_components"
model_dir = component_dir / "espressif__human_face_detect" / "models" / "s3"
packer = component_dir / "espressif__esp-dl" / "fbs_loader" / "pack_espdl_models.py"
espdet_model = model_dir / "espdet_pico_224_224_face.espdl"
packed_model = build_dir / "espdl_models" / "human_face_detect.espdl"


def pack_face_models(target, source, env):
    del source, env
    output = Path(str(target[0]))
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [
            sys.executable,
            str(packer),
            "--model_path",
            str(espdet_model),
            "--out_file",
            str(output),
        ],
        check=True,
    )


packed_target = env.Command(  # type: ignore[name-defined]
    str(packed_model),
    [str(packer), str(espdet_model)],
    pack_face_models,
)
env.Depends("$BUILD_DIR/${PROGNAME}.bin", packed_target)  # type: ignore[name-defined]

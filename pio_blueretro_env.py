Import("env")
import os
import shutil
from pathlib import Path

env["ENV"]["BR_HW"] = "_hw2"
env["ENV"]["BR_SYS"] = "_universal"

wokwi = env["PIOENV"] == "ps2_hw2_universal_wokwi"
env["ENV"]["BR_WOKWI"] = "1" if wokwi else "0"

idf_dir = Path(env.PioPlatform().get_package_dir("framework-espidf"))
heap_file = idf_dir / "components" / "heap" / "heap_caps_init.c"
backup = Path(env.subst("$PROJECT_DIR")) / ".pio" / "espidf-original" / "heap_caps_init.c"

backup.parent.mkdir(parents=True, exist_ok=True)

if not backup.exists():
    shutil.copy2(heap_file, backup)

original = backup.read_text(encoding="utf-8")
patched = original

if wokwi:
    anchor = "        if (region->type == -1) {"
    workaround = '        if (region->type == 4) {\n            region->type = -1;\n            num_heaps--;\n        }\n\n'
    if anchor not in patched:
        raise RuntimeError("Could not find heap_caps_init.c patch location")
    patched = patched.replace(anchor, workaround + anchor, 1)

if heap_file.read_text(encoding="utf-8") != patched:
    heap_file.write_text(patched, encoding="utf-8")
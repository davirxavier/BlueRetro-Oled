
Import("env")

import shutil
from pathlib import Path

# BlueRetro hardware configuration
env["ENV"]["BR_HW"] = "_hw2"
env["ENV"]["BR_SYS"] = "_universal"

# Actual PlatformIO environment name
wokwi = env["PIOENV"] == "wokwi"
env["ENV"]["BR_WOKWI"] = "1" if wokwi else "0"

print(f"[BlueRetro] PlatformIO environment: {env['PIOENV']}")
print(f"[BlueRetro] Wokwi workaround: {'ENABLED' if wokwi else 'DISABLED'}")

# Locate ESP-IDF heap initialization source
idf_dir = Path(env.PioPlatform().get_package_dir("framework-espidf"))
heap_file = idf_dir / "components" / "heap" / "heap_caps_init.c"

# Backup original ESP-IDF source
project_dir = Path(env.subst("$PROJECT_DIR"))
backup = project_dir / ".pio" / "espidf-original" / "heap_caps_init.c"

backup.parent.mkdir(parents=True, exist_ok=True)

PATCH_MARKER = "WOKWI_HEAP_REGION_TYPE4_WORKAROUND"
OLD_PATCH = "region->type = -1;\n            num_heaps--;"

# Create pristine backup only if one does not already exist
if not backup.exists():
    current = heap_file.read_text(encoding="utf-8")

    if PATCH_MARKER in current or OLD_PATCH in current:
        raise RuntimeError(
            "ESP-IDF source appears patched, but no pristine backup exists. "
            "Restore/reinstall ESP-IDF before continuing."
        )

    shutil.copy2(heap_file, backup)
    print("[BlueRetro] Created original ESP-IDF backup")

original = backup.read_text(encoding="utf-8")

# Validate backup
if PATCH_MARKER in original or OLD_PATCH in original:
    raise RuntimeError(
        "ESP-IDF backup is not pristine. Restore the original ESP-IDF "
        "package and recreate .pio/espidf-original/heap_caps_init.c"
    )

patched = original

if wokwi:

    # Exclude type-4 regions after ESP-IDF region coalescing.
    anchor1 = "/* Start by allocating the registered heap data on the stack."

    workaround1 = """/* WOKWI_HEAP_REGION_TYPE4_WORKAROUND */
for (size_t i = 0; i < num_regions; i++) {
    if (regions[i].type == 4) {
        regions[i].type = -1;
        num_heaps--;
    }
}

"""

    if patched.count(anchor1) != 1:
        raise RuntimeError(
            "Could not find unique heap allocation anchor in heap_caps_init.c"
        )

    patched = patched.replace(anchor1, workaround1 + anchor1, 1)

    # Skip invalid regions before accessing soc_memory_types
    # or allocating/accessing their heap descriptors.
    old_block = """        soc_memory_region_t *region = &regions[i];
        const soc_memory_type_desc_t *type = &soc_memory_types[region->type];
        heap_t *heap = &temp_heaps[heap_idx];
        if (region->type == -1) {
            memset(heap, 0, sizeof(*heap));
            continue;
        }
"""

    new_block = """        soc_memory_region_t *region = &regions[i];

        if (region->type == -1) {
            continue;
        }

        const soc_memory_type_desc_t *type = &soc_memory_types[region->type];
        heap_t *heap = &temp_heaps[heap_idx];

"""

    if patched.count(old_block) != 1:
        raise RuntimeError(
            "Could not find unique heap region processing block in heap_caps_init.c"
        )

    patched = patched.replace(old_block, new_block, 1)

    if PATCH_MARKER not in patched:
        raise RuntimeError("Wokwi heap workaround was not applied")

    print("[BlueRetro] Applying Wokwi heap region workaround")

else:
    print("[BlueRetro] Restoring original ESP-IDF heap initialization")

# Always generate from the pristine backup.
# This prevents duplicate patches and restores the original
# source when switching between Wokwi and normal environments.

current = heap_file.read_text(encoding="utf-8")

if current != patched:
    heap_file.write_text(patched, encoding="utf-8")

print(
    "[BlueRetro] ESP-IDF heap initialization: "
    + ("Wokwi workaround applied" if wokwi else "original restored")
)

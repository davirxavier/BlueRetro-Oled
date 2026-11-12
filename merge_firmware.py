# PlatformIO extra script: merge bootloader + partition table + app into one image
# that Wokwi can load (.pio/build/<env>/firmware.merged.bin).
Import("env")
from os.path import join, isfile

# Optional: also include a filesystem image built with `pio run -t buildfs`.
# Set this to the offset of the spiffs/littlefs partition in your partition table.
FS_OFFSET = None  # e.g. "0x290000" for the default 4MB partition table

def merge_firmware(source, target, env):
    board = env.BoardConfig()
    build_dir = env.subst("$BUILD_DIR")
    merged = join(build_dir, "firmware.merged.bin")
    esptool = join(env.PioPlatform().get_package_dir("tool-esptoolpy"), "esptool.py")
    images = [(offset, env.subst(path)) for offset, path in env.get("FLASH_EXTRA_IMAGES", [])]
    images.append((env.subst("$ESP32_APP_OFFSET"), str(target[0])))
    fs_image = join(build_dir, env.subst("$ESP32_FS_IMAGE_NAME") + ".bin")
    if FS_OFFSET and isfile(fs_image):
        images.append((FS_OFFSET, fs_image))
    cmd = [
        '"$PYTHONEXE"', '"%s"' % esptool,
        "--chip", board.get("build.mcu", "esp32"),
        "merge_bin", "-o", '"%s"' % merged,
        "--flash_mode", board.get("build.flash_mode", "dio"),
        "--flash_size", board.get("upload.flash_size", "4MB"),
                        ]
    for offset, path in images:
        # offsets may be strings ("0x1000") or ints, depending on the framework
        cmd += [offset if isinstance(offset, str) else hex(offset), '"%s"' % path]
    env.Execute(env.VerboseAction(" ".join(cmd), "Merging firmware images into %s" % merged))

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_firmware)
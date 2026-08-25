Import("env")

import os
import shutil

# LVGL and the M5GFX utility sources are C, while the firmware renderer and
# preview harness are C++.  GCC 16 defaults C sources to C23, where the old
# M5GFX QR helper's `bool` typedef is rejected, so keep the two languages on
# their normal standards independently.
env.Append(CFLAGS=["-std=gnu17"])
env.Append(CXXFLAGS=["-std=gnu++17"])

# Make the native preview portable when launched by double-clicking
# program.exe.  PlatformIO normally relies on the MSYS2 bin directory being
# present in PATH, which is true in a terminal but not when Windows launches
# the executable from Explorer or a chat attachment.
def _bundle_runtime(target, source, env):
    output_dir = os.path.dirname(str(target[0]))
    runtime_dir = r"C:\msys64\ucrt64\bin"
    for name in ("SDL2.dll", "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"):
        source_path = os.path.join(runtime_dir, name)
        target_path = os.path.join(output_dir, name)
        if os.path.isfile(source_path):
            shutil.copy2(source_path, target_path)

env.AddPostAction(env.subst("$BUILD_DIR/program.exe"), _bundle_runtime)

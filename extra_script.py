import json
import os
import subprocess

Import("env")

# SCons exec's this script without defining __file__, so the library directory has
# to come from PlatformIO. It exports `pio_lib_builder` for library extra scripts
# (see piolib.py process_extra_options); .path is the absolute library root. The
# cwd fallback covers the project-extra_script case, where cwd is the library dir
# anyway thanks to fs.cd(self.path) + SConscriptChdir(True).
try:
    Import("pio_lib_builder")
    lib_dir = pio_lib_builder.path
except Exception:
    lib_dir = os.getcwd()

lib_version = "unknown"
manifest_path = os.path.join(lib_dir, "library.json")
try:
    with open(manifest_path) as f:
        lib_version = json.load(f).get("version", "unknown")
except (IOError, OSError, ValueError) as e:
    # Don't fail the firmware build over a version string; SocketClientDefs.h
    # falls back to "unknown" and the device just reports no libVersion.
    print("SocketClient: could not read version from %s (%s)" % (manifest_path, e))


def _git(args):
    """Run a git command in lib_dir; return stripped stdout, or "" on any failure.

    Failure is normal, not exceptional: when the library is installed from the
    PlatformIO registry it is an unpacked tarball with no .git, and git may not
    be on PATH at all. Either way the build must still succeed.
    """
    try:
        out = subprocess.check_output(
            ["git"] + args,
            cwd=lib_dir,
            stderr=subprocess.DEVNULL,
        )
        return out.decode("utf-8", "replace").strip()
    except Exception:
        return ""


lib_commit = _git(["rev-parse", "--short", "HEAD"]) or "unknown"
if lib_commit != "unknown":
    # -uno = tracked files only. Untracked files are excluded deliberately: stray
    # scratch files would otherwise mark every build dirty, and .gitignore already
    # hides real build output. A tracked edit is the case that makes the hash lie.
    if _git(["status", "--porcelain", "-uno"]):
        lib_commit += "-dirty"

print("SocketClient: libVersion = %s (%s)" % (lib_version, lib_commit))

env.Append(CPPDEFINES=[
    ("SOCKETCLIENT_LIB_VERSION", env.StringifyMacro(lib_version)),
    ("SOCKETCLIENT_LIB_COMMIT", env.StringifyMacro(lib_commit)),
])

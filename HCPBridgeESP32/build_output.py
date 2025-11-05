Import('env')
import os
import re
import shutil

def get_version_from_header(header_path):
    """Liest die Version aus configuration.h (z.B. const char *HA_VERSION = "0.9";)."""
    if not os.path.exists(header_path):
        print(f"[WARN] {header_path} nicht gefunden!")
        return "unknown"

    with open(header_path, "r", encoding="utf-8") as f:
        content = f.read()

    match = re.search(r'HA_VERSION\s*=\s*"([^"]+)"', content)
    if match:
        return match.group(1)
    else:
        print("[WARN] Keine HA_VERSION gefunden!")
        return "unknown"

def post_program_action(source, target, env):
    project_dir = env["PROJECT_DIR"]
    build_dir = os.path.join(project_dir, ".pio", "build", env["PIOENV"])
    src_bin = os.path.join(build_dir, f"{env['PROGNAME']}.bin")

    # Version aus configuration.h auslesen
    config_path = os.path.join(project_dir, "include", "configuration.h")
    if not os.path.exists(config_path):
        config_path = os.path.join(project_dir, "src", "configuration.h")
    version = get_version_from_header(config_path)

    # Zielverzeichnis "fw" vorbereiten
    dest_dir = os.path.join(project_dir, "fw")
    os.makedirs(dest_dir, exist_ok=True)

    # Neuer Name: <env>-v<version>.bin
    new_name = f"{env['PIOENV']}-v{version}.bin"
    dest_path = os.path.join(dest_dir, new_name)

    shutil.copy(src_bin, dest_path)
    print(f"[INFO] Firmware exportiert: {dest_path}")

# Standardname anpassen, falls nötig
env.Replace(PROGNAME="%s" % env.subst("$PIOENV"))

# Post-Aktion nach dem Build hinzufügen
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", post_program_action)

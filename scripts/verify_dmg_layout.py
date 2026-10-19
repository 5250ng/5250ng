#!/usr/bin/env python3
"""Check the Finder layout of a mounted 5250ng disk image.

Finder cannot be screenshotted on a headless CI runner, so this reads the
.DS_Store written by dmgbuild and compares it with the dmgbuild settings
file: icon positions, icon size, icon view and the background picture.

    verify_dmg_layout.py <settings.py> <mount-point> <app-name>

Requires the ds_store module, which is installed alongside dmgbuild.
"""

import os
import sys

from ds_store import DSStore


def load_settings(settings_path, app_name):
    namespace = {"defines": {"app": app_name}}
    with open(settings_path, encoding="utf-8") as fp:
        exec(compile(fp.read(), settings_path, "exec"), namespace)
    return namespace


def main():
    if len(sys.argv) != 4:
        sys.exit(f"usage: {sys.argv[0]} <settings.py> <mount-point> <app-name>")
    settings_path, mount_point, app_name = sys.argv[1:]
    settings = load_settings(settings_path, app_name)
    errors = []

    def expect(condition, message):
        if not condition:
            errors.append(message)

    store_path = os.path.join(mount_point, ".DS_Store")
    if not os.path.isfile(store_path):
        sys.exit(f"error: {store_path} is missing; the disk image has no layout")

    expect(
        os.path.isfile(os.path.join(mount_point, ".background.tiff")),
        "multi-resolution .background.tiff is missing",
    )
    if settings.get("icon"):
        expect(
            os.path.isfile(os.path.join(mount_point, ".VolumeIcon.icns")),
            ".VolumeIcon.icns is missing",
        )

    with DSStore.open(store_path, "r") as store:
        for name, expected in settings["icon_locations"].items():
            try:
                location = tuple(store[name]["Iloc"])
            except KeyError:
                errors.append(f"no icon position recorded for {name!r}")
                continue
            expect(
                location == tuple(expected),
                f"{name!r} is at {location}, expected {tuple(expected)}",
            )

        view = store["."]["icvp"]
        expect(
            float(view["iconSize"]) == float(settings["icon_size"]),
            f"icon size is {view['iconSize']}, expected {settings['icon_size']}",
        )
        # backgroundType 2 is a picture; 0 and 1 are default and colour.
        expect(view["backgroundType"] == 2, "background is not a picture")
        expect(store["."]["icvl"][1] == b"icnv", "default view is not icon view")

    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        sys.exit(1)
    print(f"Disk image layout matches {os.path.basename(settings_path)}")


if __name__ == "__main__":
    main()

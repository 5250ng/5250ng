# dmgbuild settings for the 5250ng drag-to-Applications disk image.
#
# Used by scripts/package_macos.sh as:
#
#   dmgbuild -s dmg_settings.py -D app=<5250ng.app> \
#       -D background=<background.png> [-D icon=<volume.icns>] \
#       5250ng <output.dmg>
#
# dmgbuild writes the Finder layout (.DS_Store) itself, so this works on
# headless CI runners where Finder cannot be scripted.
#
# The icon positions and window size must match the artwork drawn by
# scripts/generate_dmg_background.py.

import os.path

# `defines` is injected by dmgbuild from the -D command line options.
application = defines.get("app", "5250ng.app")  # noqa: F821
app_name = os.path.basename(application)

format = "UDZO"
filesystem = "HFS+"

files = [application]
symlinks = {"Applications": "/Applications"}
hide_extensions = [app_name]

# Volume icon shown on the desktop and in the Finder sidebar.
icon = defines.get("icon") or None  # noqa: F821

# dmgbuild combines background.png and background@2x.png into a
# multi-resolution TIFF so the artwork stays sharp on Retina displays.
background = defines.get("background", "background.png")  # noqa: F821

show_status_bar = False
show_tab_view = False
show_toolbar = False
show_pathbar = False
show_sidebar = False

# The window bounds include the title bar, so the height is the 400 pt
# artwork plus the 28 pt title bar of current macOS releases.
window_rect = ((200, 120), (660, 428))
default_view = "icon-view"
show_icon_preview = False
include_icon_view_settings = True
include_list_view_settings = False

arrange_by = None
icon_size = 160
text_size = 14
label_pos = "bottom"
icon_locations = {
    app_name: (165, 180),
    "Applications": (495, 180),
}

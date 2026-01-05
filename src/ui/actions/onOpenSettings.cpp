#include "../dialogs/settings_dialog.h"
#include "../main_window.h"

using namespace core;

/**
 * Open the application Settings dialog.
 *
 * Presents categories in a sidebar and content on the right (e.g., Theme).
 */
void MainWindow::onOpenSettings() {
    SettingsDialog dlg(this);
    dlg.exec();
}
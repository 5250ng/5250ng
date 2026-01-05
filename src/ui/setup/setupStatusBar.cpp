#include "../main_window.h"
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

/**
 * Initialize the main window status bar widgets and default state.
 *
 * Creates and adds two permanent widgets:
 * - A small round status indicator (left) whose color reflects the connection
 *   state (e.g., red when disconnected). The indicator has a tooltip for
 *   accessibility and quick context.
 * - A textual status label (right) that displays the current connection status
 *   or connection target (e.g., "Connected to host:port").
 *
 * Finally, sets the initial indicator/text to the Disconnected state to ensure
 * a consistent baseline before any network activity occurs.
 */
void MainWindow::setupStatusBar() {
    // Moved connection status into each tab footer so it is owned by the session
    // Leave status bar available for future global indicators if needed
}

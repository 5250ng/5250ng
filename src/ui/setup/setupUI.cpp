#include "ui/main_window.h"
#include "ui/themes/manager.h"
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QMenuBar>
#include <QPushButton>
#include <QSizePolicy>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

/**
 * Initialize the main window central UI layout.
 *
 * Responsibilities:
 * - Apply the default UI theme using ThemeManager (startup look-and-feel).
 * - Create the central widget and a top-level vertical layout with zero
 *   margins to maximize usable space.
 * - Build the tabbed session area:
 *   - A movable, closable QTabWidget that hosts one container per session.
 *   - Wire tab events: currentChanged (active session swap), tabMoved
 *     (reorder sessions), tabCloseRequested (close a session), and install
 *     an event filter on the tab bar for context-menu operations (rename/close).
 * - Maintain “active session” convenience pointers (display/client/parser) and
 *   initialize them to nullptr until a session is created.
 * - Create the empty-state placeholder visible when no tabs are open:
 *   - A centered box constrained to at most one third of the window width.
 *   - Contains:
 *     1) A heading label (“No sessions currently open.”)
 *     2) A “Create New Session” button (full width of the box)
 *     3) A horizontal rule (full width of the box)
 *     4) A label (“Open saved session”)
 *     5) A saved-sessions dropdown; changing selection triggers quick open
 *        via onQuickOpenChanged
 * - Add the placeholder to the central layout and keep it ready to toggle with
 *   updateEmptyState().
 * - Connect screen size changes from the display widget to adjust the
 *   coordinates label font, and initialize both the font and coordinates.
 * - Finalize by assigning the layout to the central widget and invoking
 *   updateEmptyState() to set the proper initial visibility (tabs vs. empty).
 */
void MainWindow::setupUI() {
    // Load built-in themes before applying one (must come first)
    ui::themes::ThemeManager::instance().loadBuiltinThemes();
    // Set the UI theme to dark via ThemeManager
    ui::themes::ThemeManager::instance().setCurrentTheme("dark");

    // Use BaseFramelessWindow content area
    QVBoxLayout *content = contentLayout();

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setMovable(true);
    // We'll provide our own close button in custom tab headers
    m_tabWidget->setTabsClosable(false);
    content->addWidget(m_tabWidget, 1); // Stretch factor 1 to fill space
    m_activeIndex = -1;
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);
    connect(m_tabWidget->tabBar(), &QTabBar::tabMoved, this, &MainWindow::onTabMoved);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::onCloseTabRequested);
    m_tabWidget->tabBar()->installEventFilter(this);

    // Active session pointers are null until a tab is created
    m_displayWidget = nullptr;
    m_parser = nullptr;

    // Empty placeholder (visible when no tabs)
    m_emptyPlaceholder = new QWidget(this);
    // Outer layout to center the box
    QHBoxLayout *outerLayout = new QHBoxLayout(m_emptyPlaceholder);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addStretch();
    QWidget *emptyBox = new QWidget(m_emptyPlaceholder);
    emptyBox->setMaximumWidth(width() * 2 / 5); // limit to 2/5 of the tab width
    QVBoxLayout *emptyLayout = new QVBoxLayout(emptyBox);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    emptyLayout->addStretch();
    QLabel *title = new QLabel("5250ng", this);
    {
        QFont f = title->font();
        f.setBold(true);
        f.setPointSize(f.pointSize() + 8);
        title->setFont(f);
    }
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("color: #cccccc;");
    emptyLayout->addWidget(title);
    emptyLayout->addSpacing(6);
    QLabel *emptyText = new QLabel("No sessions currently open.", this);
    emptyText->setAlignment(Qt::AlignCenter);
    emptyText->setStyleSheet("color: #cccccc; font-size: 16px;");
    emptyLayout->addWidget(emptyText);
    emptyLayout->addSpacing(8);
    QPushButton *newSessionBtn = new QPushButton("Create New Session", this);
    newSessionBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(newSessionBtn, &QPushButton::clicked, this, &MainWindow::onNewSession);
    emptyLayout->addWidget(newSessionBtn);
    // hrule
    emptyLayout->addSpacing(12);
    QFrame *hr = new QFrame(this);
    hr->setFrameShape(QFrame::HLine);
    hr->setFrameShadow(QFrame::Sunken);
    hr->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    emptyLayout->addWidget(hr);
    emptyLayout->addSpacing(8);
    // Open saved session label and dropdown
    QLabel *openLabel = new QLabel("Open saved session", this);
    openLabel->setAlignment(Qt::AlignCenter);
    openLabel->setStyleSheet("color: #cccccc; font-size: 16px;");
    emptyLayout->addWidget(openLabel);
    m_emptyOpenCombo = new QComboBox(this);
    m_emptyOpenCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_emptyOpenCombo->addItem("(Open saved session)");
    connect(m_emptyOpenCombo, &QComboBox::currentTextChanged, this, &MainWindow::onQuickOpenChanged);
    emptyLayout->addWidget(m_emptyOpenCombo);
    emptyLayout->addStretch();
    outerLayout->addWidget(emptyBox, 0, Qt::AlignCenter);
    outerLayout->addStretch();
    content->addWidget(m_emptyPlaceholder, 1);

    // Screen size changes are connected when a session display widget is created

    // Initialize coordinates and font
    updateCursorCoordinatesFont();
    updateCursorCoordinates();

    updateEmptyState();
}
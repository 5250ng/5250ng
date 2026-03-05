#include "Q5250TerminalView.h"
#include "core/ebcdic.h"

namespace ui::widgets {

Q5250TerminalView::Q5250TerminalView(QWidget *parent)
    : QWidget(parent),
      m_layout(new QVBoxLayout(this)),
      m_screen(new Q5250ScreenWidget(this)),
      m_rule(new Q5250HRule(this)),
      m_footer(new Q5250ScreenWidget(this)) {
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    // Screen fills all available space; its paintEvent centers content via screenOffset()
    m_screen->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_footer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_rule->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_footer->setCursorEnabled(false);

    m_layout->addWidget(m_screen, 1); // screen takes all available vertical space
    m_layout->addWidget(m_rule);
    m_layout->addWidget(m_footer);
    setLayout(m_layout);

    // Footer is single row; column count follows main screen
    m_footer->setScreenSize(1, m_screen->screenCols());
    m_footer->setSelectionEnabled(false);
    connect(m_screen, &Q5250ScreenWidget::screenSizeChanged, this, &Q5250TerminalView::onScreenSizeChanged);
    connect(m_screen, &Q5250ScreenWidget::terminalStateChanged, this, &Q5250TerminalView::updateStatusIndicators);
}

void Q5250TerminalView::setScreenSize(int rows, int cols) {
    m_screen->setScreenSize(rows, cols);
    m_footer->setScreenSize(1, cols);
}

void Q5250TerminalView::setFont(const QFont &font) {
    m_screen->setFont(font);
    m_footer->setFont(font);
}

void Q5250TerminalView::onScreenSizeChanged(int rows, int cols) {
    Q_UNUSED(rows);
    m_footer->setScreenSize(1, cols);
}

void Q5250TerminalView::updateStatusIndicators() {
    if (!m_footer || !m_footer->screenBuffer() || !m_screen) {
        return;
    }
    auto *buf = m_footer->screenBuffer();
    int cols = buf->cols();

    // Clear footer row
    for (int c = 0; c < cols; ++c) {
        buf->writeChar(0, c, 0x40); // EBCDIC space
    }

    // Build status text based on screen's terminal state
    // Position indicators at the right side of the footer
    // Format: "X II" (locked), "IM" (insert mode), "MW" (message waiting)
    KeyboardState kbState = m_screen->keyboardState();

    // Keyboard lock indicator at column 70+
    int pos = cols - 10;
    if (pos < 0) pos = 0;

    CellAttributes indicatorAttr;

    if (kbState == KeyboardState::Locked) {
        indicatorAttr.color = 12; // Red
        // Write "X II" (system lock indicator)
        const char *text = "X II";
        for (int j = 0; text[j] && pos + j < cols; ++j) {
            buf->writeChar(0, pos + j, core::EBCDIC::charToEBCDIC(QChar(text[j])), indicatorAttr);
        }
    } else if (kbState == KeyboardState::ErrorLocked) {
        indicatorAttr.color = 12; // Red
        const char *text = "X ER";
        for (int j = 0; text[j] && pos + j < cols; ++j) {
            buf->writeChar(0, pos + j, core::EBCDIC::charToEBCDIC(QChar(text[j])), indicatorAttr);
        }
    } else if (kbState == KeyboardState::SystemRequest) {
        indicatorAttr.color = 14; // Yellow
        const char *text = "X SR";
        for (int j = 0; text[j] && pos + j < cols; ++j) {
            buf->writeChar(0, pos + j, core::EBCDIC::charToEBCDIC(QChar(text[j])), indicatorAttr);
        }
    }

    // Insert mode indicator
    pos = cols - 5;
    if (pos < 0) pos = 0;
    if (m_screen->insertMode()) {
        indicatorAttr.color = 14; // Yellow
        const char *text = "IM";
        for (int j = 0; text[j] && pos + j < cols; ++j) {
            buf->writeChar(0, pos + j, core::EBCDIC::charToEBCDIC(QChar(text[j])), indicatorAttr);
        }
    }

    // Message waiting indicator
    pos = cols - 2;
    if (pos < 0) pos = 0;
    if (m_screen->messageWaiting()) {
        indicatorAttr.color = 15; // White
        const char *text = "MW";
        for (int j = 0; text[j] && pos + j < cols; ++j) {
            buf->writeChar(0, pos + j, core::EBCDIC::charToEBCDIC(QChar(text[j])), indicatorAttr);
        }
    }

    m_footer->update();
}

} // namespace ui::widgets



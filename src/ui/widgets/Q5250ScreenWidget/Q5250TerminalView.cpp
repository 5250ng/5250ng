#include "Q5250TerminalView.h"

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

} // namespace ui::widgets



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

    // Prefer content-sized widgets; center them
    m_screen->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_footer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_rule->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_footer->setCursorEnabled(false);

    m_layout->addStretch(1);
    m_layout->addWidget(m_screen, 0, Qt::AlignHCenter);
    m_layout->addWidget(m_rule, 0, Qt::AlignHCenter);
    m_layout->addWidget(m_footer, 0, Qt::AlignHCenter);
    m_layout->addStretch(1);
    setLayout(m_layout);

    // Footer is single row; width follows main screen columns
    m_footer->setScreenSize(1, m_screen->screenCols());
    m_footer->setSelectionEnabled(false);
    connect(m_screen, &Q5250ScreenWidget::screenSizeChanged, this, &Q5250TerminalView::onScreenSizeChanged);

    // Initialize widths to the screen content width
    int w = static_cast<const QWidget *>(m_screen)->sizeHint().width();
    m_rule->setFixedWidth(w);
    m_footer->setMinimumWidth(w);
    m_footer->setMaximumWidth(w);
}

void Q5250TerminalView::setScreenSize(int rows, int cols) {
    m_screen->setScreenSize(rows, cols);
    m_footer->setScreenSize(1, cols);
    int w = static_cast<const QWidget *>(m_screen)->sizeHint().width();
    m_rule->setFixedWidth(w);
    m_footer->setMinimumWidth(w);
    m_footer->setMaximumWidth(w);
}

void Q5250TerminalView::setFont(const QFont &font) {
    m_screen->setFont(font);
    m_footer->setFont(font);
    int w = static_cast<const QWidget *>(m_screen)->sizeHint().width();
    m_rule->setFixedWidth(w);
    m_footer->setMinimumWidth(w);
    m_footer->setMaximumWidth(w);
}

void Q5250TerminalView::onScreenSizeChanged(int rows, int cols) {
    Q_UNUSED(rows);
    m_footer->setScreenSize(1, cols);
    int w = static_cast<const QWidget *>(m_screen)->sizeHint().width();
    m_rule->setFixedWidth(w);
    m_footer->setMinimumWidth(w);
    m_footer->setMaximumWidth(w);
}

} // namespace ui::widgets



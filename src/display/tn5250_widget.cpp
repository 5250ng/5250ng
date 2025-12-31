#include "tn5250_widget.h"
#include <QDebug>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>

namespace display {

TN5250Widget::TN5250Widget(QWidget *parent)
    : QWidget(parent), m_screenBuffer(new ScreenBuffer(24, 80, this)),
      m_font("Courier", 12), m_cellSize(8, 16), m_bgColor(Qt::black),
      m_fgColor(Qt::green), m_cursorBlinkRate(500), m_cursorBlinkState(true),
      m_extendedMode(false) {
  setFocusPolicy(Qt::StrongFocus);
  setAttribute(Qt::WA_OpaquePaintEvent);

  // Initialize color scheme (standard 5250 colors)
  m_colorScheme.resize(16);
  m_colorScheme[0] = QColor(0, 0, 0);        // Black
  m_colorScheme[1] = QColor(0, 0, 128);      // Blue
  m_colorScheme[2] = QColor(0, 128, 0);      // Green
  m_colorScheme[3] = QColor(0, 128, 128);    // Cyan
  m_colorScheme[4] = QColor(128, 0, 0);      // Red
  m_colorScheme[5] = QColor(128, 0, 128);    // Magenta
  m_colorScheme[6] = QColor(128, 64, 0);     // Brown
  m_colorScheme[7] = QColor(192, 192, 192);  // Light gray
  m_colorScheme[8] = QColor(128, 128, 128);  // Dark gray
  m_colorScheme[9] = QColor(0, 0, 255);      // Light blue
  m_colorScheme[10] = QColor(0, 255, 0);     // Light green
  m_colorScheme[11] = QColor(0, 255, 255);   // Light cyan
  m_colorScheme[12] = QColor(255, 0, 0);     // Light red
  m_colorScheme[13] = QColor(255, 0, 255);   // Light magenta
  m_colorScheme[14] = QColor(255, 255, 0);   // Yellow
  m_colorScheme[15] = QColor(255, 255, 255); // White

  // Setup blink timer
  m_blinkTimer = new QTimer(this);
  connect(m_blinkTimer, &QTimer::timeout, this, &TN5250Widget::onBlinkTimer);
  m_blinkTimer->start(m_cursorBlinkRate);

  // Connect screen buffer signals
  connect(m_screenBuffer, &ScreenBuffer::screenChanged, this,
          &TN5250Widget::onScreenChanged);
  connect(m_screenBuffer, &ScreenBuffer::cursorMoved, this,
          &TN5250Widget::onCursorMoved);

  // Create input handler
  m_inputHandler = new core::InputHandler(m_screenBuffer, this);
  connect(m_inputHandler, &core::InputHandler::inputReady, this,
          &TN5250Widget::inputReady);
  connect(m_inputHandler, &core::InputHandler::screenUpdateNeeded, this,
          QOverload<>::of(&QWidget::update));

  calculateCellSize();
}

TN5250Widget::~TN5250Widget() {}

void TN5250Widget::setScreenSize(int rows, int cols) {
  m_screenBuffer->resize(rows, cols);
  calculateCellSize();
  updateGeometry();
  update();
  emit screenSizeChanged(rows, cols);
}

void TN5250Widget::setFont(const QFont &font) {
  m_font = font;
  calculateCellSize();
  updateGeometry();
  update();
}

void TN5250Widget::setColorScheme(const QVector<QColor> &colors) {
  if (colors.size() >= 16) {
    m_colorScheme = colors;
    update();
  }
}

void TN5250Widget::setBackgroundColor(const QColor &color) {
  m_bgColor = color;
  update();
}

void TN5250Widget::setForegroundColor(const QColor &color) {
  m_fgColor = color;
  update();
}

void TN5250Widget::setCursorBlinkRate(int msec) {
  m_cursorBlinkRate = msec;
  if (m_blinkTimer) {
    m_blinkTimer->setInterval(msec);
  }
}

void TN5250Widget::updateScreen() { update(); }

void TN5250Widget::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  // Fill background
  painter.fillRect(rect(), m_bgColor);

  // Render screen
  renderScreen(painter);
}

void TN5250Widget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  // Could adjust font size to fit, but for now just recalculate
  calculateCellSize();
}

QSize TN5250Widget::sizeHint() const {
  return QSize(m_cellSize.width() * m_screenBuffer->cols(),
               m_cellSize.height() * m_screenBuffer->rows());
}

QSize TN5250Widget::minimumSizeHint() const { return sizeHint(); }

void TN5250Widget::keyPressEvent(QKeyEvent *event) {
  if (m_inputHandler) {
    m_inputHandler->processKeyEvent(event);
  }
  QWidget::keyPressEvent(event);
}

void TN5250Widget::focusInEvent(QFocusEvent *event) {
  QWidget::focusInEvent(event);
  // Widget now has focus and can receive keyboard input
}

void TN5250Widget::focusOutEvent(QFocusEvent *event) {
  QWidget::focusOutEvent(event);
  // Widget lost focus
}

void TN5250Widget::onScreenChanged() { update(); }

void TN5250Widget::onCursorMoved(const QPoint &pos) { update(); }

void TN5250Widget::onBlinkTimer() {
  m_cursorBlinkState = !m_cursorBlinkState;
  if (m_screenBuffer->isCursorVisible()) {
    update();
  }
}

void TN5250Widget::renderScreen(QPainter &painter) {
  painter.setFont(m_font);

  int rows = m_screenBuffer->rows();
  int cols = m_screenBuffer->cols();

  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      const ScreenCell &cell = m_screenBuffer->cell(row, col);
      renderCell(painter, row, col, cell);
    }
  }

  // Render cursor
  if (m_screenBuffer->isCursorVisible() && m_cursorBlinkState) {
    QPoint cursorPos = m_screenBuffer->cursorPosition();
    renderCursor(painter, cursorPos.y(), cursorPos.x());
  }
}

void TN5250Widget::renderCell(QPainter &painter, int row, int col,
                              const ScreenCell &cell) {
  QRect cellRect = this->cellRect(row, col);

  // Get colors based on attributes
  QColor bgColor = m_bgColor;
  QColor fgColor = m_fgColor;

  if (cell.attributes.color < m_colorScheme.size()) {
    fgColor = m_colorScheme[cell.attributes.color];
  }

  // Reverse video
  if (cell.attributes.reverse) {
    qSwap(bgColor, fgColor);
  }

  // Fill background
  painter.fillRect(cellRect, bgColor);

  // Draw character
  QChar ch = core::EBCDIC::ebcdicToChar(cell.character);

  painter.setPen(fgColor);

  // Handle underline
  QFont font = m_font;
  if (cell.attributes.underline) {
    font.setUnderline(true);
    painter.setFont(font);
  }

  // Draw text
  painter.drawText(cellRect, Qt::AlignCenter, ch);

  // Reset font
  painter.setFont(m_font);

  // Handle blink (could be implemented with timer, but for now just render)
  // Blinking is typically handled by the timer updating the screen
}

void TN5250Widget::renderCursor(QPainter &painter, int row, int col) {
  QRect cellRect = this->cellRect(row, col);

  // Draw cursor as a block or underline
  painter.fillRect(cellRect.adjusted(0, cellRect.height() - 2, 0, 0),
                   m_fgColor);
}

QColor TN5250Widget::getColorForCode(uint8_t colorCode) const {
  if (colorCode < m_colorScheme.size()) {
    return m_colorScheme[colorCode];
  }
  return m_fgColor;
}

void TN5250Widget::calculateCellSize() {
  QFontMetrics fm(m_font);
  m_cellSize = QSize(fm.horizontalAdvance('M'), fm.height());
}

QPoint TN5250Widget::cellPosition(int row, int col) const {
  return QPoint(col * m_cellSize.width(), row * m_cellSize.height());
}

QRect TN5250Widget::cellRect(int row, int col) const {
  QPoint pos = cellPosition(row, col);
  return QRect(pos, m_cellSize);
}

} // namespace display

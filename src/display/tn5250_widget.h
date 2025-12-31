#pragma once

#include "../core/ebcdic.h"
#include "../core/input_handler.h"
#include "screen_buffer.h"
#include <QKeyEvent>
#include <QTimer>
#include <QWidget>

namespace display {

// Custom QWidget for TN5250 display rendering
class TN5250Widget : public QWidget {
  Q_OBJECT

public:
  explicit TN5250Widget(QWidget *parent = nullptr);
  ~TN5250Widget();

  // Screen buffer access
  ScreenBuffer *screenBuffer() { return m_screenBuffer; }
  const ScreenBuffer *screenBuffer() const { return m_screenBuffer; }

  // Display configuration
  void setScreenSize(int rows, int cols);
  int screenRows() const { return m_screenBuffer->rows(); }
  int screenCols() const { return m_screenBuffer->cols(); }

  // Font configuration
  void setFont(const QFont &font);
  QFont font() const { return m_font; }

  // Color scheme
  void setColorScheme(const QVector<QColor> &colors);
  QColor backgroundColor() const { return m_bgColor; }
  QColor foregroundColor() const { return m_fgColor; }
  void setBackgroundColor(const QColor &color);
  void setForegroundColor(const QColor &color);

  // Cursor
  void setCursorBlinkRate(int msec);
  int cursorBlinkRate() const { return m_cursorBlinkRate; }

public slots:
  void updateScreen();

signals:
  void screenSizeChanged(int rows, int cols);
  void inputReady(const QByteArray &data);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

private slots:
  void onScreenChanged();
  void onCursorMoved(const QPoint &pos);
  void onBlinkTimer();

private:
  // Rendering
  void renderScreen(QPainter &painter);
  void renderCell(QPainter &painter, int row, int col, const ScreenCell &cell);
  void renderCursor(QPainter &painter, int row, int col);
  QColor getColorForCode(uint8_t colorCode) const;

  // Layout
  void calculateCellSize();
  QPoint cellPosition(int row, int col) const;
  QRect cellRect(int row, int col) const;

  ScreenBuffer *m_screenBuffer;
  core::InputHandler *m_inputHandler;
  QFont m_font;
  QSize m_cellSize;
  QColor m_bgColor;
  QColor m_fgColor;
  QVector<QColor> m_colorScheme;

  // Cursor blinking
  QTimer *m_blinkTimer;
  int m_cursorBlinkRate;
  bool m_cursorBlinkState;

  // 27×132 mode support
  bool m_extendedMode;
};

} // namespace display

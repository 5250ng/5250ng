#pragma once

#include "ui/themes/terminal_theme.h"
#include <QWidget>

class ThemePreviewWidget : public QWidget {
    Q_OBJECT

  public:
    explicit ThemePreviewWidget(QWidget *parent = nullptr);

    void setTheme(const ui::themes::TerminalTheme &theme);

    QSize sizeHint() const override { return QSize(480, 220); }
    QSize minimumSizeHint() const override { return QSize(320, 150); }

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    ui::themes::TerminalTheme m_theme;
    QVector<QColor> m_colorScheme;

    struct PreviewCell {
        QChar ch;
        int colorIndex;  // index into 16-color scheme
        bool reverse = false;
        bool underline = false;
    };

    QVector<QVector<PreviewCell>> m_previewScreen;
    void buildPreviewScreen();
    void addText(int row, int col, const QString &text, int colorIndex,
                 bool reverse = false, bool underline = false);

    // CRT post-processing
    void renderCRTEffect(QPainter &painter, const QRect &area);
    // Cursor preview
    void renderCursor(QPainter &painter, const QRect &cellRect);

    // Cursor position in the preview (on the User input field)
    static constexpr int kCursorRow = 6;
    static constexpr int kCursorCol = 23;
};

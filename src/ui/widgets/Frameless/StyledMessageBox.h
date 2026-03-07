#pragma once

#include "BaseFramelessDialog.h"
#include <QLabel>
#include <QPushButton>
#include <QString>

namespace ui::widgets {

/// Drop-in replacement for QMessageBox that uses the frameless dialog style.
class StyledMessageBox : public BaseFramelessDialog {
    Q_OBJECT
  public:
    enum Result { Yes, No, Ok };

    static void information(QWidget *parent, const QString &title, const QString &text);
    static void warning(QWidget *parent, const QString &title, const QString &text);
    static Result question(QWidget *parent, const QString &title, const QString &text);

  private:
    explicit StyledMessageBox(QWidget *parent, const QString &title,
                              const QString &text, bool hasNo = false);
    Result m_result = Ok;
};

} // namespace ui::widgets

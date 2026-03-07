#include "../main_window.h"
#include "ui/widgets/Frameless/BaseFramelessDialog.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

void MainWindow::onAbout() {
    ui::widgets::BaseFramelessDialog dlg(this);
    dlg.setWindowTitle("About 5250ng");
    dlg.setModal(true);

    QVBoxLayout *content = dlg.contentLayout();
    content->setContentsMargins(24, 16, 24, 16);
    content->setSpacing(8);

    QLabel *title = new QLabel("5250ng", &dlg);
    QFont f = title->font();
    f.setPointSize(18);
    f.setBold(true);
    title->setFont(f);
    title->setAlignment(Qt::AlignCenter);
    content->addWidget(title);

    QLabel *version = new QLabel("Version 0.5.0", &dlg);
    version->setAlignment(Qt::AlignCenter);
    content->addWidget(version);

    content->addSpacing(8);

    QLabel *desc = new QLabel(
        "A modern IBM TN5250 terminal emulator.\n\n"
        "Multi-session tabbed interface with full TN5250 protocol support,\n"
        "9 EBCDIC code pages, CRT effects, and custom themes.\n\n"
        "Built with Qt " QT_VERSION_STR " / C++17",
        &dlg);
    desc->setWordWrap(true);
    desc->setAlignment(Qt::AlignCenter);
    content->addWidget(desc);

    content->addSpacing(12);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *ok = new QPushButton("OK", &dlg);
    ok->setDefault(true);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnLayout->addWidget(ok);
    btnLayout->addStretch();
    content->addLayout(btnLayout);

    dlg.exec();
}

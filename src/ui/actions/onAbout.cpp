// 5250ng - A modern IBM TN5250 terminal emulator                                                                                                                                                            
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)                                                                                                                                                          
//                                                                                                                                                                                                           
// This program is free software: you can redistribute it and/or modify                                                                                                                                      
// it under the terms of the GNU General Public License as published by                                                                                                                                      
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.                                                                                                                                                                       
//                                                                                                                                                                                                           
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "../main_window.h"
#include <QtUiStyle/BaseFramelessDialog.h>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

void MainWindow::onAbout() {
    qt_ui_style::BaseFramelessDialog dlg(this);
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

    QLabel *version = new QLabel("Version " + qApp->applicationVersion(), &dlg);
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

    content->addSpacing(8);

    QLabel *copyright = new QLabel(
        "Copyright (C) 2025-2026 Remi GASCOU (Podalirius)\n\n"
        "This program is free software: you can redistribute it and/or modify\n"
        "it under the terms of the GNU General Public License as published by\n"
        "the Free Software Foundation, either version 3 of the License, or\n"
        "(at your option) any later version.\n\n"
        "This program comes with ABSOLUTELY NO WARRANTY.\n"
        "See the GNU General Public License for more details.",
        &dlg);
    copyright->setWordWrap(true);
    copyright->setAlignment(Qt::AlignCenter);
    QFont cf = copyright->font();
    cf.setPointSize(cf.pointSize() - 1);
    copyright->setFont(cf);
    content->addWidget(copyright);

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

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

#include "core/keyboard_mapping.h"
#include "ui/dialogs/keyboard_remap_dialog.h"
#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QPushButton>
#include <QShortcut>
#include <QTimer>
#include <QtTest/QtTest>

using core::KeyboardMapping;
using core::KeyChord;
using ui::dialogs::ChordListEditor;
using ui::dialogs::KeyboardRemapDialog;
using ui::dialogs::KeyChordCaptureEdit;

// Regression tests for issue #371: Remap Keyboard bound a chord that is also
// a menu or window shortcut without saying which action it shadows.
class TestRemapMenuShortcutWarning : public QObject {
    Q_OBJECT

    static KeyChord chord(int key, Qt::KeyboardModifiers mods) {
        KeyChord c;
        c.key = key;
        c.modifiers = mods;
        return c;
    }

    // A window with one menu item bound to Ctrl+Alt+K
    static QAction *addMenuItem(QMainWindow &window) {
        QAction *item = window.menuBar()->addMenu("&View")->addAction("&Zoom Panel...");
        item->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_K));
        return item;
    }

    // Answer every modal prompt for a bounded time, recording its text. A
    // chain started by an earlier test stops once init() moves m_generation.
    void answerDialogs(const QString &button) { answerFrom(button, m_generation, 0); }
    void answerFrom(const QString &button, int generation, int attempt) {
        if (generation != m_generation)
            return;
        if (QWidget *modal = QApplication::activeModalWidget()) {
            for (QLabel *l : modal->findChildren<QLabel *>())
                m_dialogText += l->text() + '\n';
            bool clicked = false;
            for (QPushButton *b : modal->findChildren<QPushButton *>()) {
                if (b->text() == button) {
                    ++m_prompts;
                    b->click();
                    clicked = true;
                    break;
                }
            }
            if (!clicked)
                modal->close();
        }
        if (attempt < 100)
            QTimer::singleShot(20, this, [this, button, generation, attempt] {
                answerFrom(button, generation, attempt + 1);
            });
    }

    // Answer successive prompts with `answers`, one per prompt
    void answerSequence(QStringList answers, int attempt = 0) {
        const int generation = m_generation;
        if (QWidget *modal = QApplication::activeModalWidget()) {
            for (QLabel *l : modal->findChildren<QLabel *>())
                m_dialogText += l->text() + '\n';
            for (QPushButton *b : modal->findChildren<QPushButton *>()) {
                if (!answers.isEmpty() && b->text() == answers.first()) {
                    ++m_prompts;
                    answers.removeFirst();
                    b->click();
                    break;
                }
            }
        }
        if (!answers.isEmpty() && attempt < 150)
            QTimer::singleShot(20, this, [this, answers, attempt, generation] {
                if (generation == m_generation)
                    answerSequence(answers, attempt + 1);
            });
    }

    // Capture `c` as a new chord on the first row of the dialog
    static ChordListEditor *captureOnFirstRow(KeyboardRemapDialog &dialog, const KeyChord &c) {
        ChordListEditor *editor = dialog.findChild<ChordListEditor *>();
        if (!editor)
            return nullptr;
        QPushButton *add = nullptr;
        for (QPushButton *b : editor->findChildren<QPushButton *>())
            if (b->text().contains("Add chord"))
                add = b;
        if (!add)
            return nullptr;
        add->click();
        const QList<KeyChordCaptureEdit *> edits = editor->findChildren<KeyChordCaptureEdit *>();
        if (edits.isEmpty())
            return nullptr;
        QTest::keyClick(edits.last(), Qt::Key(c.key), c.modifiers);
        return editor;
    }

    QString m_dialogText;
    int m_prompts = 0;
    int m_generation = 0;

  private slots:
    void init() {
        KeyboardMapping::instance().resetToDefaults();
        m_dialogText.clear();
        m_prompts = 0;
        ++m_generation;
    }

    void namesTheMenuItem() {
        QMainWindow window;
        addMenuItem(window);
        QCOMPARE(KeyboardRemapDialog::shadowedShortcutName(
                     &window, chord(Qt::Key_K, Qt::ControlModifier | Qt::AltModifier)),
                 QStringLiteral("Zoom Panel"));
        QVERIFY(KeyboardRemapDialog::shadowedShortcutName(
                    &window, chord(Qt::Key_K, Qt::ControlModifier)).isEmpty());
        QVERIFY(KeyboardRemapDialog::shadowedShortcutName(
                    nullptr, chord(Qt::Key_K, Qt::ControlModifier | Qt::AltModifier)).isEmpty());
    }

    // A shortcut that only fires inside one widget is not shadowed
    void ignoresWidgetShortcuts() {
        QMainWindow window;
        addMenuItem(window)->setShortcutContext(Qt::WidgetShortcut);
        QVERIFY(KeyboardRemapDialog::shadowedShortcutName(
                    &window, chord(Qt::Key_K, Qt::ControlModifier | Qt::AltModifier)).isEmpty());
    }

    void namesAWindowQShortcut() {
        QMainWindow window;
        auto *shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Tab), &window);
        shortcut->setWhatsThis("Next Tab");
        QCOMPARE(KeyboardRemapDialog::shadowedShortcutName(&window, chord(Qt::Key_Tab, Qt::ControlModifier)),
                 QStringLiteral("Next Tab"));
    }

    void declineKeepsTheOldBindings() {
        QMainWindow window;
        addMenuItem(window);
        KeyboardRemapDialog dialog(&window);
        const KeyChord c = chord(Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
        answerDialogs(QStringLiteral("No"));
        ChordListEditor *editor = captureOnFirstRow(dialog, c);
        QVERIFY(editor);
        QTRY_VERIFY(m_prompts >= 1);
        QVERIFY2(m_dialogText.contains("Zoom Panel"), qPrintable(m_dialogText));
        QVERIFY(!editor->chords().contains(c));
    }

    void acceptBindsTheChord() {
        QMainWindow window;
        addMenuItem(window);
        KeyboardRemapDialog dialog(&window);
        const KeyChord c = chord(Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
        answerDialogs(QStringLiteral("Yes"));
        ChordListEditor *editor = captureOnFirstRow(dialog, c);
        QVERIFY(editor);
        QTRY_VERIFY(m_prompts >= 1);
        QVERIFY(editor->chords().contains(c));
    }

    // A child top-level window (the Script Editor) has shortcuts of its own
    // that never fire while the main window has focus
    void ignoresChildWindowShortcuts() {
        QMainWindow window;
        auto *editor = new QWidget(&window, Qt::Window);
        auto *run = new QAction("", editor);
        run->setShortcut(QKeySequence(Qt::Key_F5));
        editor->addAction(run);
        QVERIFY(KeyboardRemapDialog::shadowedShortcutName(&window, chord(Qt::Key_F5, Qt::NoModifier))
                    .isEmpty());
        // The same action on the main window itself is reported
        auto *own = new QAction("Run", &window);
        own->setShortcut(QKeySequence(Qt::Key_F5));
        QCOMPARE(KeyboardRemapDialog::shadowedShortcutName(&window, chord(Qt::Key_F5, Qt::NoModifier)),
                 QStringLiteral("Run"));
    }

    // The screen pastes and copies on the platform keys before it checks
    // the mapping, so the prompt says the binding will not take effect
    void pasteAndCopyKeysExplainScreenHandling() {
        const QList<QKeySequence> paste = QKeySequence::keyBindings(QKeySequence::Paste);
        const QList<QKeySequence> copy = QKeySequence::keyBindings(QKeySequence::Copy);
        if (paste.isEmpty() || copy.isEmpty())
            QSKIP("No platform Copy/Paste bindings");
        auto toChord = [](const QKeySequence &seq) {
            KeyChord c;
            c.key = seq[0].key();
            c.modifiers = seq[0].keyboardModifiers();
            return c;
        };
        const QString pastePrompt = KeyboardRemapDialog::shadowingPrompt(
            nullptr, toChord(paste.first()), core::MappedAction::Enter);
        QVERIFY2(pastePrompt.contains("Paste shortcut") && pastePrompt.contains("will not do"),
                 qPrintable(pastePrompt));
        const QString copyPrompt = KeyboardRemapDialog::shadowingPrompt(
            nullptr, toChord(copy.first()), core::MappedAction::Enter);
        QVERIFY2(copyPrompt.contains("Copy shortcut"), qPrintable(copyPrompt));
        QVERIFY(KeyboardRemapDialog::shadowingPrompt(
                    nullptr, chord(Qt::Key_J, Qt::ControlModifier | Qt::AltModifier),
                    core::MappedAction::Enter).isEmpty());
    }

    // A chord that shadows a menu item and is bound to another 5250 action
    // asks twice; No to the second keeps both rows unchanged
    void secondPromptDeclinedKeepsBindings() {
        QMainWindow window;
        QAction *item = addMenuItem(window);
        // Give the menu item the chord of an existing 5250 binding
        const auto bindings = KeyboardMapping::instance().allBindings();
        QVERIFY(!bindings.isEmpty());
        KeyChord taken;
        core::MappedAction owner = core::MappedAction::None;
        // A modified F-key (Shift+F1 is PF13 by default), chosen in a fixed
        // order since QHash iteration order varies between runs
        for (auto it = bindings.constBegin(); it != bindings.constEnd(); ++it) {
            const KeyChord &c = it.key();
            if (c.modifiers == Qt::NoModifier || c.key < Qt::Key_F1 || c.key > Qt::Key_F24)
                continue;
            if (!taken.isValid() || c.toString() < taken.toString()) {
                taken = c;
                owner = it.value();
            }
        }
        QVERIFY(taken.isValid());
        item->setShortcut(QKeySequence(QKeyCombination(taken.modifiers, Qt::Key(taken.key))));
        KeyboardRemapDialog dialog(&window);
        // Pick a row other than the chord's owner
        ChordListEditor *editor = nullptr;
        for (ChordListEditor *e : dialog.findChildren<ChordListEditor *>())
            if (e->action() != owner && !editor)
                editor = e;
        QVERIFY(editor);
        const QList<KeyChord> before = editor->chords();
        QStringList answers{QStringLiteral("Yes"), QStringLiteral("No")};
        answerSequence(answers);
        QPushButton *add = nullptr;
        for (QPushButton *b : editor->findChildren<QPushButton *>())
            if (b->text().contains("Add chord"))
                add = b;
        QVERIFY(add);
        add->click();
        QTest::keyClick(editor->findChildren<KeyChordCaptureEdit *>().last(), Qt::Key(taken.key),
                        taken.modifiers);
        QTRY_COMPARE(m_prompts, 2);
        QCOMPARE(editor->chords(), before);
        ChordListEditor *ownerEditor = nullptr;
        for (ChordListEditor *e : dialog.findChildren<ChordListEditor *>())
            if (e->action() == owner)
                ownerEditor = e;
        QVERIFY(ownerEditor);
        QVERIFY(ownerEditor->chords().contains(taken));
        QVERIFY2(m_dialogText.contains("Zoom Panel"), qPrintable(m_dialogText));
    }

    // A chord no menu uses binds without a prompt
    void unusedChordDoesNotPrompt() {
        QMainWindow window;
        addMenuItem(window);
        KeyboardRemapDialog dialog(&window);
        const KeyChord c = chord(Qt::Key_J, Qt::ControlModifier | Qt::AltModifier);
        answerDialogs(QStringLiteral("No"));
        ChordListEditor *editor = captureOnFirstRow(dialog, c);
        QVERIFY(editor);
        QTest::qWait(200);
        QCOMPARE(m_prompts, 0);
        QVERIFY(editor->chords().contains(c));
    }
};

QTEST_MAIN(TestRemapMenuShortcutWarning)
#include "test_remap_menu_shortcut_warning.moc"

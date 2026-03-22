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

#include "screen_buffer_adapter.h"
#include "Q5250ScreenWidget.h"
#include "core/ebcdic.h"
#include "screen_buffer.h"
#include <QPoint>

namespace core::scripting {

ScreenBufferAdapter::ScreenBufferAdapter(ui::widgets::Q5250ScreenWidget *widget)
    : m_widget(widget) {}

int ScreenBufferAdapter::rows() const {
    auto *buf = m_widget ? m_widget->screenBuffer() : nullptr;
    return buf ? buf->rows() : 0;
}

int ScreenBufferAdapter::cols() const {
    auto *buf = m_widget ? m_widget->screenBuffer() : nullptr;
    return buf ? buf->cols() : 0;
}

int ScreenBufferAdapter::cursorRow() const {
    auto *buf = m_widget ? m_widget->screenBuffer() : nullptr;
    return buf ? buf->cursorPosition().y() : 0;
}

int ScreenBufferAdapter::cursorCol() const {
    auto *buf = m_widget ? m_widget->screenBuffer() : nullptr;
    return buf ? buf->cursorPosition().x() : 0;
}

QString ScreenBufferAdapter::readText(int row, int col, int length) const {
    auto *buf = m_widget ? m_widget->screenBuffer() : nullptr;
    if (!buf || row < 0 || row >= buf->rows() || col < 0) return {};

    QString result;
    for (int i = 0; i < length && col + i < buf->cols(); ++i) {
        uint8_t ch = buf->character(row, col + i);
        result += core::EBCDIC::ebcdicToChar(ch);
    }
    return result;
}

QString ScreenBufferAdapter::readFieldText(int row, int col) const {
    auto *buf = m_widget ? m_widget->screenBuffer() : nullptr;
    if (!buf) return {};

    auto field = buf->getField(row, col);
    if (field.length == 0) return {};

    QByteArray data = buf->getFieldData(field);
    return core::EBCDIC::ebcdicToString(data);
}

ScreenInterface::KeyboardState ScreenBufferAdapter::keyboardState() const {
    if (!m_widget) return KeyboardState::Unlocked;

    switch (m_widget->keyboardState()) {
    case ui::widgets::KeyboardState::Unlocked:
        return KeyboardState::Unlocked;
    case ui::widgets::KeyboardState::Locked:
        return KeyboardState::Locked;
    case ui::widgets::KeyboardState::ErrorLocked:
        return KeyboardState::ErrorLocked;
    case ui::widgets::KeyboardState::SystemRequest:
        return KeyboardState::SystemRequest;
    }
    return KeyboardState::Unlocked;
}

bool ScreenBufferAdapter::messageWaiting() const {
    return m_widget ? m_widget->messageWaiting() : false;
}

} // namespace core::scripting

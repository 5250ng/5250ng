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

#include "script_parser.h"
#include "script_compiler.h"
#include <functional>

namespace core::scripting {

ScriptParser::ScriptParser() {}

ParseResult ScriptParser::parse(const QString &source) {
    auto result = parse(m_lexer.tokenize(source));
    result.metadata = ScriptCompiler::extractMetadata(source).values;
    return result;
}

ParseResult ScriptParser::parse(const QVector<TokenLine> &tokenLines) {
    m_errors.clear();

    auto root = std::make_shared<ASTNode>();
    root->type = NodeType::Script;

    // Stack for nested blocks: each entry is a pair of (block node, collecting-else flag)
    struct BlockFrame {
        std::shared_ptr<ASTNode> node;
        bool inElse = false;
    };
    QVector<BlockFrame> blockStack;

    for (int i = 0; i < tokenLines.size(); ++i) {
        const TokenLine &tl = tokenLines[i];
        if (tl.isEmpty()) continue;

        TokenType first = tl[0].type;
        int line = tl[0].line;

        // Block terminators
        if (first == TokenType::ENDDEF) {
            if (blockStack.isEmpty() || blockStack.last().node->type != NodeType::FunctionDef) {
                error(line, "ENDDEF without matching DEF");
            } else {
                blockStack.removeLast();
            }
            continue;
        }
        if (first == TokenType::ENDIF) {
            if (blockStack.isEmpty() || blockStack.last().node->type != NodeType::If) {
                error(line, "ENDIF without matching IF");
            } else {
                blockStack.removeLast();
            }
            continue;
        }
        if (first == TokenType::ENDWHILE) {
            if (blockStack.isEmpty() || blockStack.last().node->type != NodeType::While) {
                error(line, "ENDWHILE without matching WHILE");
            } else {
                blockStack.removeLast();
            }
            continue;
        }
        if (first == TokenType::ENDREPEAT) {
            if (blockStack.isEmpty() || blockStack.last().node->type != NodeType::Repeat) {
                error(line, "ENDREPEAT without matching REPEAT");
            } else {
                blockStack.removeLast();
            }
            continue;
        }

        // ELSE
        if (first == TokenType::ELSE) {
            if (blockStack.isEmpty() || blockStack.last().node->type != NodeType::If) {
                error(line, "ELSE without matching IF");
            } else if (blockStack.last().inElse) {
                error(line, "Duplicate ELSE in IF block");
            } else {
                blockStack.last().inElse = true;
            }
            continue;
        }

        // DEF must be at top level
        if (first == TokenType::DEF && !blockStack.isEmpty()) {
            error(line, "DEF must be at the top level (not inside IF/WHILE/REPEAT/DEF)");
            continue;
        }

        // Parse the line into an AST node
        auto node = parseLine(tl);
        if (!node) continue;

        // Determine where to append the node
        auto &target = [&]() -> QVector<std::shared_ptr<ASTNode>> & {
            if (blockStack.isEmpty()) return root->children;
            auto &frame = blockStack.last();
            if (frame.inElse) return frame.node->elseChildren;
            return frame.node->children;
        }();
        target.append(node);

        // If the node starts a new block, push it
        if (node->type == NodeType::If || node->type == NodeType::While ||
            node->type == NodeType::Repeat || node->type == NodeType::FunctionDef) {
            blockStack.append({node, false});
        }
    }

    // Check unclosed blocks
    for (const auto &frame : blockStack) {
        QString keyword;
        switch (frame.node->type) {
        case NodeType::If: keyword = "IF"; break;
        case NodeType::While: keyword = "WHILE"; break;
        case NodeType::Repeat: keyword = "REPEAT"; break;
        case NodeType::FunctionDef: keyword = "DEF"; break;
        default: keyword = "block"; break;
        }
        error(frame.node->line, QString("Unclosed %1 block").arg(keyword));
    }

    // Extract function definitions from root children into result.functions
    ParseResult result;
    result.root = root;

    QVector<std::shared_ptr<ASTNode>> mainChildren;
    for (const auto &child : root->children) {
        if (child->type == NodeType::FunctionDef) {
            const QString &name = child->stringValue;
            if (result.functions.contains(name)) {
                error(child->line, QString("Duplicate function '%1'").arg(name));
            }
            result.functions[name] = child;
        } else {
            mainChildren.append(child);
        }
    }
    root->children = mainChildren;

    // Resolve labels (after function extraction so indices are correct)
    for (int i = 0; i < root->children.size(); ++i) {
        if (root->children[i]->type == NodeType::Label) {
            const QString &name = root->children[i]->stringValue;
            if (result.labels.contains(name)) {
                error(root->children[i]->line,
                      QString("Duplicate label '%1'").arg(name));
            }
            result.labels[name] = i;
        }
    }

    // Validate GOTO targets and CALL targets (recursive walk)
    std::function<void(const QVector<std::shared_ptr<ASTNode>> &)> validateNodes;
    validateNodes = [&](const QVector<std::shared_ptr<ASTNode>> &nodes) {
        for (const auto &child : nodes) {
            if (child->type == NodeType::Goto) {
                if (!result.labels.contains(child->stringValue)) {
                    error(child->line,
                          QString("GOTO target '%1' not found").arg(child->stringValue));
                }
            }
            if (child->type == NodeType::OnTimeout || child->type == NodeType::OnError) {
                if (!result.labels.contains(child->stringValue)) {
                    error(child->line,
                          QString("ON handler target '%1' not found").arg(child->stringValue));
                }
            }
            if (child->type == NodeType::FunctionCall) {
                if (!result.functions.contains(child->stringValue)) {
                    error(child->line,
                          QString("CALL target '%1' not found").arg(child->stringValue));
                } else {
                    auto &func = result.functions[child->stringValue];
                    if (child->argValues.size() != func->paramNames.size()) {
                        error(child->line,
                              QString("CALL '%1' expects %2 arguments, got %3")
                                  .arg(child->stringValue)
                                  .arg(func->paramNames.size())
                                  .arg(child->argValues.size()));
                    }
                }
            }
            if (!child->children.isEmpty())
                validateNodes(child->children);
            if (!child->elseChildren.isEmpty())
                validateNodes(child->elseChildren);
        }
    };
    validateNodes(root->children);
    for (const auto &func : result.functions) {
        validateNodes(func->children);
    }

    result.errors = m_errors;
    return result;
}

std::shared_ptr<ASTNode> ScriptParser::parseLine(const TokenLine &tokens) {
    if (tokens.isEmpty()) return nullptr;

    auto node = std::make_shared<ASTNode>();
    node->line = tokens[0].line;
    TokenType first = tokens[0].type;

    switch (first) {
    // Input
    case TokenType::TYPE:
        node->type = NodeType::TypeInput;
        if (tokens.size() > 1 && tokens[1].type == TokenType::STRING_LITERAL)
            node->stringValue = tokens[1].value;
        else
            error(node->line, "TYPE requires a double-quoted string");
        return node;

    case TokenType::PRESS:
        return parsePressKey(tokens);

    // AID keys
    case TokenType::ENTER:
    case TokenType::F1: case TokenType::F2: case TokenType::F3:
    case TokenType::F4: case TokenType::F5: case TokenType::F6:
    case TokenType::F7: case TokenType::F8: case TokenType::F9:
    case TokenType::F10: case TokenType::F11: case TokenType::F12:
    case TokenType::F13: case TokenType::F14: case TokenType::F15:
    case TokenType::F16: case TokenType::F17: case TokenType::F18:
    case TokenType::F19: case TokenType::F20: case TokenType::F21:
    case TokenType::F22: case TokenType::F23: case TokenType::F24:
    case TokenType::PAGEUP: case TokenType::PAGEDOWN:
    case TokenType::ATTN: case TokenType::SYSREQ:
    case TokenType::HELP: case TokenType::CLEAR: case TokenType::PRINT:
        node->type = NodeType::AIDKey;
        node->stringValue = tokens[0].value;
        node->aidByte = aidByteForToken(first);
        return node;

    // Local keys
    case TokenType::TAB: case TokenType::BACKTAB:
    case TokenType::BACKSPACE: case TokenType::DELETE_KEY:
    case TokenType::INSERT: case TokenType::HOME: case TokenType::END:
    case TokenType::ESC:
    case TokenType::FIELDPLUS: case TokenType::FIELDMINUS:
    case TokenType::FIELDEXIT: case TokenType::DUP:
    case TokenType::ERASEINPUT: case TokenType::ERASEFIELD:
    case TokenType::ERASEEOF:
        node->type = NodeType::LocalKey;
        node->stringValue = tokens[0].value;
        return node;

    // Cursor movement
    case TokenType::MOVE:
        return parseMoveCursor(tokens);

    // Screen inspection
    case TokenType::EXPECT: return parseExpect(tokens);
    case TokenType::EXTRACT: return parseExtract(tokens);

    // Timing
    case TokenType::WAIT:
        node->type = NodeType::Wait;
        node->intValue = (tokens.size() > 1) ? tokens[1].value.toInt() : 1000;
        return node;
    case TokenType::GLOBAL:
        if (tokens.size() >= 2 && tokens[1].type == TokenType::DELAY) {
            node->type = NodeType::GlobalDelay;
            node->intValue = (tokens.size() > 2) ? tokens[2].value.toInt() : 0;
            if (node->intValue < 0) node->intValue = 0;
        } else if (tokens.size() >= 2 && tokens[1].type == TokenType::JITTER) {
            node->type = NodeType::GlobalJitter;
            node->intValue = (tokens.size() > 2) ? tokens[2].value.toInt() : 0;
            node->intValue2 = (tokens.size() > 3) ? tokens[3].value.toInt() : 0;
            if (node->intValue < 0) node->intValue = 0;
            if (node->intValue2 < node->intValue) node->intValue2 = node->intValue;
        } else if (tokens.size() >= 2 && tokens[1].type == TokenType::EXPECT_TIMEOUT) {
            node->type = NodeType::GlobalExpectTimeout;
            node->intValue = (tokens.size() > 2) ? tokens[2].value.toInt() : 30000;
            if (node->intValue < 0) node->intValue = 30000;
        } else {
            error(node->line, "GLOBAL requires DELAY, JITTER, or EXPECT_TIMEOUT");
        }
        return node;

    // Variables
    case TokenType::SET: return parseSet(tokens);
    case TokenType::INC:
        node->type = NodeType::Inc;
        node->varName = (tokens.size() > 1) ? tokens[1].value : "";
        return node;
    case TokenType::DEC:
        node->type = NodeType::Dec;
        node->varName = (tokens.size() > 1) ? tokens[1].value : "";
        return node;
    case TokenType::ADD: return parseAdd(tokens);

    // Control flow
    case TokenType::IF: return parseIf(tokens);
    case TokenType::WHILE: return parseWhile(tokens);
    case TokenType::REPEAT:
        node->type = NodeType::Repeat;
        node->intValue = (tokens.size() > 1) ? tokens[1].value.toInt() : 1;
        return node;
    case TokenType::LABEL:
        node->type = NodeType::Label;
        if (tokens.size() > 1)
            node->stringValue = tokens[1].value;
        else
            error(node->line, "LABEL requires a name");
        return node;
    case TokenType::GOTO:
        node->type = NodeType::Goto;
        if (tokens.size() > 1)
            node->stringValue = tokens[1].value;
        else
            error(node->line, "GOTO requires a label name");
        return node;

    // Functions
    case TokenType::DEF: return parseDef(tokens);
    case TokenType::CALL: return parseCall(tokens);
    case TokenType::RETURN:
        node->type = NodeType::Return;
        return node;

    // Error handling
    case TokenType::ON: return parseOn(tokens);
    case TokenType::ABORT:
        node->type = NodeType::Abort;
        if (tokens.size() > 1 && tokens[1].type == TokenType::STRING_LITERAL)
            node->stringValue = tokens[1].value;
        return node;

    // Utility
    case TokenType::LOG:
        node->type = NodeType::Log;
        if (tokens.size() > 1)
            node->stringValue = tokens[1].value;
        return node;
    case TokenType::PAUSE:
        node->type = NodeType::Pause;
        return node;

    default:
        // Bare function call: identifier(args...)
        if (first == TokenType::STRING_LITERAL && tokens.size() >= 2 &&
            tokens[1].type == TokenType::LPAREN) {
            return parseCall(tokens);
        }
        error(tokens[0].line, QString("Unexpected token: %1").arg(tokens[0].value));
        return nullptr;
    }
}

std::shared_ptr<ASTNode> ScriptParser::parseExpect(const TokenLine &tokens) {
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::Expect;
    node->line = tokens[0].line;

    if (tokens.size() < 2) {
        error(node->line, "EXPECT requires arguments");
        return node;
    }

    int idx = 1;

    // Check for NOT
    if (tokens[idx].type == TokenType::NOT) {
        node->negated = true;
        idx++;
        if (idx >= tokens.size()) {
            error(node->line, "EXPECT NOT requires a condition");
            return node;
        }
    }

    TokenType subType = tokens[idx].type;

    if (subType == TokenType::TEXT) {
        // EXPECT TEXT "str" [AT row col | AT ROW row]
        idx++;
        if (idx < tokens.size() && tokens[idx].type == TokenType::STRING_LITERAL) {
            node->stringValue = tokens[idx++].value;
        } else {
            error(node->line, "EXPECT TEXT requires a quoted string");
            return node;
        }
        if (idx < tokens.size() && tokens[idx].type == TokenType::AT) {
            idx++;
            if (idx < tokens.size() && tokens[idx].type == TokenType::ROW) {
                idx++;
                node->expectType = ExpectType::TextAtRow;
                node->intValue = (idx < tokens.size()) ? tokens[idx].value.toInt() : 1;
            } else {
                node->expectType = ExpectType::TextAtPos;
                node->intValue = (idx < tokens.size()) ? tokens[idx++].value.toInt() : 1;
                node->intValue2 = (idx < tokens.size()) ? tokens[idx].value.toInt() : 1;
            }
        } else {
            node->expectType = ExpectType::TextAnywhere;
        }
    } else if (subType == TokenType::CURSOR) {
        // EXPECT CURSOR AT row col | AT ROW row
        idx++;
        if (idx < tokens.size() && tokens[idx].type == TokenType::AT) idx++;
        if (idx < tokens.size() && tokens[idx].type == TokenType::ROW) {
            idx++;
            node->expectType = ExpectType::CursorAtRow;
            node->intValue = (idx < tokens.size()) ? tokens[idx].value.toInt() : 1;
        } else {
            node->expectType = ExpectType::CursorAtPos;
            node->intValue = (idx < tokens.size()) ? tokens[idx++].value.toInt() : 1;
            node->intValue2 = (idx < tokens.size()) ? tokens[idx].value.toInt() : 1;
        }
    } else if (subType == TokenType::KEYBOARD) {
        idx++;
        if (idx < tokens.size() && tokens[idx].type == TokenType::UNLOCKED) {
            node->expectType = ExpectType::KeyboardUnlocked;
        } else if (idx < tokens.size() && tokens[idx].type == TokenType::ERRORLOCKED) {
            node->expectType = ExpectType::KeyboardErrorLocked;
        } else {
            error(node->line, "EXPECT KEYBOARD requires UNLOCKED or ERRORLOCKED");
        }
    } else if (subType == TokenType::FIELD) {
        // EXPECT FIELD AT row col CONTAINS "str"
        idx++;
        node->expectType = ExpectType::FieldContains;
        if (idx < tokens.size() && tokens[idx].type == TokenType::AT) idx++;
        node->intValue = (idx < tokens.size()) ? tokens[idx++].value.toInt() : 1;
        node->intValue2 = (idx < tokens.size()) ? tokens[idx++].value.toInt() : 1;
        if (idx >= tokens.size() || tokens[idx].type != TokenType::CONTAINS) {
            error(node->line, "EXPECT FIELD requires CONTAINS keyword");
            return node;
        }
        idx++; // skip CONTAINS
        if (idx >= tokens.size() || tokens[idx].type != TokenType::STRING_LITERAL) {
            error(node->line, "EXPECT FIELD CONTAINS requires a quoted string");
            return node;
        }
        node->stringValue = tokens[idx].value;
    } else if (subType == TokenType::MESSAGEWAITING) {
        node->expectType = ExpectType::MessageWaiting;
    } else {
        error(node->line, QString("Unknown EXPECT type: %1").arg(tokens[idx].value));
    }

    return node;
}

std::shared_ptr<ASTNode> ScriptParser::parseExtract(const TokenLine &tokens) {
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::Extract;
    node->line = tokens[0].line;

    if (tokens.size() < 3) {
        error(node->line, "EXTRACT requires variable and source");
        return node;
    }

    // EXTRACT $var ...
    node->varName = tokens[1].value;
    int idx = 2;

    if (idx >= tokens.size()) {
        error(node->line, "EXTRACT requires FROM, FIELD, or CURSOR after variable");
        return node;
    }

    if (tokens[idx].type == TokenType::FROM) {
        // EXTRACT $var FROM row col LENGTH n
        idx++;
        node->extractType = ExtractType::FromPosition;
        node->intValue = (idx < tokens.size()) ? tokens[idx++].value.toInt() : 1;
        node->intValue2 = (idx < tokens.size()) ? tokens[idx++].value.toInt() : 1;
        if (idx < tokens.size() && tokens[idx].type == TokenType::LENGTH) idx++;
        node->stringValue = (idx < tokens.size()) ? tokens[idx].value : "1";
    } else if (tokens[idx].type == TokenType::FIELD) {
        // EXTRACT $var FIELD AT row col
        node->extractType = ExtractType::FieldAt;
        idx++;
        if (idx < tokens.size() && tokens[idx].type == TokenType::AT) idx++;
        node->intValue = (idx < tokens.size()) ? tokens[idx++].value.toInt() : 1;
        node->intValue2 = (idx < tokens.size()) ? tokens[idx].value.toInt() : 1;
    } else if (tokens[idx].type == TokenType::CURSOR) {
        // EXTRACT $var CURSOR ROW / COL
        idx++;
        if (idx < tokens.size() && tokens[idx].type == TokenType::ROW) {
            node->extractType = ExtractType::CursorRow;
        } else if (idx < tokens.size() && tokens[idx].type == TokenType::COL) {
            node->extractType = ExtractType::CursorCol;
        } else {
            error(node->line, "EXTRACT CURSOR requires ROW or COL");
        }
    } else if (tokens[idx].type == TokenType::LINE) {
        // EXTRACT $var LINE row
        node->extractType = ExtractType::LineAt;
        idx++;
        node->intValue = (idx < tokens.size()) ? tokens[idx].value.toInt() : 1;
    } else {
        error(node->line, "EXTRACT requires FROM, FIELD, CURSOR, or LINE");
    }

    return node;
}

std::shared_ptr<ASTNode> ScriptParser::parseSet(const TokenLine &tokens) {
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::Set;
    node->line = tokens[0].line;

    if (tokens.size() < 3) {
        error(node->line, "SET requires variable and value");
        return node;
    }

    node->varName = tokens[1].value;
    node->stringValue = tokens[2].value;
    return node;
}

std::shared_ptr<ASTNode> ScriptParser::parseAdd(const TokenLine &tokens) {
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::Add;
    node->line = tokens[0].line;

    if (tokens.size() < 3) {
        error(node->line, "ADD requires variable and value");
        return node;
    }

    node->varName = tokens[1].value;
    node->intValue = tokens[2].value.toInt();
    return node;
}

std::shared_ptr<ASTNode> ScriptParser::parseIf(const TokenLine &tokens) {
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::If;
    node->line = tokens[0].line;

    // IF ISSET $var
    if (tokens.size() >= 3 && tokens[1].type == TokenType::ISSET) {
        node->condLeft = tokens[2].value;
        node->condOp = CompareOp::IsSet;
        return node;
    }

    // IF $var op value
    if (tokens.size() < 4) {
        error(node->line, "IF requires condition (e.g., IF $VAR == \"value\" or IF ISSET $VAR)");
        return node;
    }

    parseCondition(tokens, 1, node->condLeft, node->condOp, node->condRight);
    return node;
}

std::shared_ptr<ASTNode> ScriptParser::parseWhile(const TokenLine &tokens) {
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::While;
    node->line = tokens[0].line;

    // WHILE ISSET $var
    if (tokens.size() >= 3 && tokens[1].type == TokenType::ISSET) {
        node->condLeft = tokens[2].value;
        node->condOp = CompareOp::IsSet;
        return node;
    }

    if (tokens.size() < 4) {
        error(node->line, "WHILE requires condition");
        return node;
    }

    parseCondition(tokens, 1, node->condLeft, node->condOp, node->condRight);
    return node;
}

std::shared_ptr<ASTNode> ScriptParser::parseOn(const TokenLine &tokens) {
    auto node = std::make_shared<ASTNode>();
    node->line = tokens[0].line;

    // ON TIMEOUT GOTO label  or  ON ERROR GOTO label
    if (tokens.size() < 4) {
        error(node->line, "ON requires TIMEOUT/ERROR GOTO label");
        return node;
    }

    if (tokens[1].type == TokenType::TIMEOUT) {
        node->type = NodeType::OnTimeout;
    } else if (tokens[1].type == TokenType::ERROR) {
        node->type = NodeType::OnError;
    } else {
        error(node->line, "ON requires TIMEOUT or ERROR");
        return node;
    }

    // tokens[2] should be GOTO
    if (tokens[2].type != TokenType::GOTO) {
        error(node->line, "ON TIMEOUT/ERROR requires GOTO keyword");
        return node;
    }
    if (tokens[3].type == TokenType::EOF_TOKEN) {
        error(node->line, "ON TIMEOUT/ERROR GOTO requires a label name");
        return node;
    }
    node->stringValue = tokens[3].value; // label name
    return node;
}

std::shared_ptr<ASTNode> ScriptParser::parseMoveCursor(const TokenLine &tokens) {
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::MoveCursorAt;
    node->line = tokens[0].line;

    if (tokens.size() < 3 || tokens[1].type != TokenType::CURSOR) {
        error(node->line, "Expected MOVE CURSOR ...");
        return node;
    }

    TokenType third = tokens[2].type;

    // MOVE CURSOR UP/DOWN/LEFT/RIGHT [n]
    if (third == TokenType::UP || third == TokenType::DOWN ||
        third == TokenType::LEFT || third == TokenType::RIGHT) {
        if (third == TokenType::UP)    node->moveCursorMode = MoveCursorMode::StepUp;
        else if (third == TokenType::DOWN)  node->moveCursorMode = MoveCursorMode::StepDown;
        else if (third == TokenType::LEFT)  node->moveCursorMode = MoveCursorMode::StepLeft;
        else                                node->moveCursorMode = MoveCursorMode::StepRight;
        node->intValue = (tokens.size() > 3) ? tokens[3].value.toInt() : 1;
        if (node->intValue < 1) node->intValue = 1;
        return node;
    }

    // MOVE CURSOR AT ...
    if (third != TokenType::AT || tokens.size() < 4) {
        error(node->line, "Expected MOVE CURSOR AT ... or MOVE CURSOR UP/DOWN/LEFT/RIGHT [n]");
        return node;
    }

    int idx = 3;

    if (tokens[idx].type == TokenType::LPAREN) {
        // MOVE CURSOR AT (row,col)
        node->moveCursorMode = MoveCursorMode::Position;
        idx++; // skip (
        if (idx < tokens.size()) node->intValue = tokens[idx++].value.toInt();  // row
        if (idx < tokens.size() && tokens[idx].type == TokenType::COMMA) idx++; // skip ,
        if (idx < tokens.size()) node->intValue2 = tokens[idx++].value.toInt(); // col
        // skip )
    } else if (tokens[idx].type == TokenType::INPUTFIELD) {
        // MOVE CURSOR AT INPUTFIELD n
        node->moveCursorMode = MoveCursorMode::FieldIndex;
        idx++;
        if (idx < tokens.size())
            node->intValue = tokens[idx].value.toInt();
        else
            error(node->line, "MOVE CURSOR AT INPUTFIELD requires a field number");
    } else if (tokens[idx].type == TokenType::NEXT) {
        // MOVE CURSOR AT NEXT INPUTFIELD
        node->moveCursorMode = MoveCursorMode::NextField;
    } else if (tokens[idx].type == TokenType::PREVIOUS) {
        // MOVE CURSOR AT PREVIOUS INPUTFIELD
        node->moveCursorMode = MoveCursorMode::PreviousField;
    } else {
        error(node->line, "Expected (row,col), INPUTFIELD, NEXT, or PREVIOUS after MOVE CURSOR AT");
    }

    return node;
}

std::shared_ptr<ASTNode> ScriptParser::parsePressKey(const TokenLine &tokens) {
    auto node = std::make_shared<ASTNode>();
    node->line = tokens[0].line;

    if (tokens.size() < 2) {
        error(node->line, "PRESS requires a key name (e.g., PRESS ENTER, PRESS KEY_A)");
        node->type = NodeType::CharInput;
        return node;
    }

    TokenType keyType = tokens[1].type;

    if (isAIDKeyToken(keyType)) {
        node->type = NodeType::AIDKey;
        node->stringValue = tokens[1].value;
        node->aidByte = aidByteForToken(keyType);
    } else if (isLocalKeyToken(keyType)) {
        node->type = NodeType::LocalKey;
        node->stringValue = tokens[1].value;
    } else if (keyType == TokenType::KEY_CHAR) {
        node->type = NodeType::CharInput;
        node->stringValue = tokens[1].value; // resolved character
    } else {
        error(node->line, QString("Unknown key: %1 (use KEY_A for characters)").arg(tokens[1].value));
        node->type = NodeType::CharInput;
    }

    return node;
}

bool ScriptParser::isAIDKeyToken(TokenType type) const {
    switch (type) {
    case TokenType::ENTER:
    case TokenType::F1: case TokenType::F2: case TokenType::F3:
    case TokenType::F4: case TokenType::F5: case TokenType::F6:
    case TokenType::F7: case TokenType::F8: case TokenType::F9:
    case TokenType::F10: case TokenType::F11: case TokenType::F12:
    case TokenType::F13: case TokenType::F14: case TokenType::F15:
    case TokenType::F16: case TokenType::F17: case TokenType::F18:
    case TokenType::F19: case TokenType::F20: case TokenType::F21:
    case TokenType::F22: case TokenType::F23: case TokenType::F24:
    case TokenType::PAGEUP: case TokenType::PAGEDOWN:
    case TokenType::ATTN: case TokenType::SYSREQ:
    case TokenType::HELP: case TokenType::CLEAR: case TokenType::PRINT:
        return true;
    default:
        return false;
    }
}

bool ScriptParser::isLocalKeyToken(TokenType type) const {
    switch (type) {
    case TokenType::TAB: case TokenType::BACKTAB:
    case TokenType::BACKSPACE: case TokenType::DELETE_KEY:
    case TokenType::INSERT: case TokenType::HOME: case TokenType::END:
    case TokenType::ESC:
    case TokenType::FIELDPLUS: case TokenType::FIELDMINUS:
    case TokenType::FIELDEXIT: case TokenType::DUP:
    case TokenType::ERASEINPUT: case TokenType::ERASEFIELD:
    case TokenType::ERASEEOF:
        return true;
    default:
        return false;
    }
}

bool ScriptParser::parseCondition(const TokenLine &tokens, int startIndex,
                                  QString &left, CompareOp &op, QString &right) {
    if (startIndex + 2 >= tokens.size()) return false;

    left = tokens[startIndex].value;

    switch (tokens[startIndex + 1].type) {
    case TokenType::OP_EQ: op = CompareOp::Eq; break;
    case TokenType::OP_NE: op = CompareOp::Ne; break;
    case TokenType::OP_LT: op = CompareOp::Lt; break;
    case TokenType::OP_GT: op = CompareOp::Gt; break;
    case TokenType::OP_LE: op = CompareOp::Le; break;
    case TokenType::OP_GE: op = CompareOp::Ge; break;
    case TokenType::CONTAINS: op = CompareOp::Contains; break;
    default:
        error(tokens[startIndex].line, "Expected comparison operator");
        return false;
    }

    right = tokens[startIndex + 2].value;
    return true;
}

std::shared_ptr<ASTNode> ScriptParser::parseDef(const TokenLine &tokens) {
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::FunctionDef;
    node->line = tokens[0].line;

    if (tokens.size() < 2) {
        error(node->line, "DEF requires a function name");
        return node;
    }

    // DEF name([$p1, $p2, ...])
    node->stringValue = tokens[1].value; // function name

    int idx = 2;
    if (idx < tokens.size() && tokens[idx].type == TokenType::LPAREN) {
        idx++; // skip (
        while (idx < tokens.size() && tokens[idx].type != TokenType::RPAREN) {
            if (tokens[idx].type == TokenType::VARIABLE) {
                node->paramNames.append(tokens[idx].value);
            } else if (tokens[idx].type == TokenType::COMMA) {
                // skip comma
            } else {
                error(node->line, QString("Expected parameter variable, got '%1'").arg(tokens[idx].value));
            }
            idx++;
        }
        // skip )
    }

    return node;
}

std::shared_ptr<ASTNode> ScriptParser::parseCall(const TokenLine &tokens) {
    auto node = std::make_shared<ASTNode>();
    node->type = NodeType::FunctionCall;
    node->line = tokens[0].line;

    // Bare call: name(args...)  vs  CALL name(args...)
    bool bareCall = (tokens[0].type != TokenType::CALL);
    int nameIdx = bareCall ? 0 : 1;

    if (nameIdx >= tokens.size()) {
        error(node->line, "CALL requires a function name");
        return node;
    }

    node->stringValue = tokens[nameIdx].value; // function name

    int idx = nameIdx + 1;
    if (idx < tokens.size() && tokens[idx].type == TokenType::LPAREN) {
        idx++; // skip (
        while (idx < tokens.size() && tokens[idx].type != TokenType::RPAREN) {
            if (tokens[idx].type == TokenType::STRING_LITERAL ||
                tokens[idx].type == TokenType::NUMBER_LITERAL ||
                tokens[idx].type == TokenType::VARIABLE) {
                node->argValues.append(tokens[idx].value);
            } else if (tokens[idx].type == TokenType::COMMA) {
                // skip comma
            } else {
                error(node->line, QString("Unexpected argument token '%1'").arg(tokens[idx].value));
            }
            idx++;
        }
        // skip )
    }

    return node;
}

void ScriptParser::error(int line, const QString &msg) {
    m_errors.append({line, msg});
}

uint8_t ScriptParser::aidByteForToken(TokenType type) const {
    switch (type) {
    case TokenType::ENTER:    return 0xF1;
    case TokenType::F1:       return 0x31;
    case TokenType::F2:       return 0x32;
    case TokenType::F3:       return 0x33;
    case TokenType::F4:       return 0x34;
    case TokenType::F5:       return 0x35;
    case TokenType::F6:       return 0x36;
    case TokenType::F7:       return 0x37;
    case TokenType::F8:       return 0x38;
    case TokenType::F9:       return 0x39;
    case TokenType::F10:      return 0x3A;
    case TokenType::F11:      return 0x3B;
    case TokenType::F12:      return 0x3C;
    case TokenType::F13:      return 0xB1;
    case TokenType::F14:      return 0xB2;
    case TokenType::F15:      return 0xB3;
    case TokenType::F16:      return 0xB4;
    case TokenType::F17:      return 0xB5;
    case TokenType::F18:      return 0xB6;
    case TokenType::F19:      return 0xB7;
    case TokenType::F20:      return 0xB8;
    case TokenType::F21:      return 0xB9;
    case TokenType::F22:      return 0xBA;
    case TokenType::F23:      return 0xBB;
    case TokenType::F24:      return 0xBC;
    case TokenType::PAGEUP:   return 0xF5; // RollUp
    case TokenType::PAGEDOWN: return 0xF4; // RollDown
    case TokenType::ATTN:     return 0x70;
    case TokenType::SYSREQ:   return 0x71;
    case TokenType::HELP:     return 0xF3;
    case TokenType::CLEAR:    return 0xBD;
    case TokenType::PRINT:    return 0xF6;
    default: return 0;
    }
}

} // namespace core::scripting

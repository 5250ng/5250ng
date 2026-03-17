# 5250Script Reference

5250Script is a line-based scripting language for automating TN5250 terminal sessions in 5250ng. It is inspired by Unix `expect` and Hak5 DuckyScript — one command per line, ALL-CAPS keywords, with the ability to react to screen content, branch on conditions, and recover from errors.

**File extension:** `.5250script`
**Encoding:** UTF-8

## Quick Start

```
# Login to AS/400
EXPECT TEXT "Sign On" AT 1 23
MOVE CURSOR AT INPUTFIELD 1
TYPE "QSECOFR"
MOVE CURSOR AT INPUTFIELD 2
TYPE "mypassword"
PRESS ENTER

EXPECT TEXT "MAIN MENU"
LOG "Login successful"
```

Save as `login.5250script`, then run it from **Scripts > Scripts > Run** in 5250ng.

---

## Language Reference

### Comments

```
# This is a comment          # Lines starting with # are ignored
PRESS ENTER # inline comment  # Text after # on any line is ignored
```

### Typing Text

```
TYPE "Hello World"           # Types each character as a keystroke
TYPE "$SESSION_USERNAME"     # Variables are expanded in the string
TYPE "$$100"                 # Use $$ for a literal dollar sign
```

The argument to `TYPE` must be a double-quoted string. Variables are expanded inside the string.

### Pressing Keys

`PRESS` sends a single key. It accepts special key names or character key constants (`KEY_*`):

```
PRESS ENTER                  # Send the Enter AID key
PRESS F12                    # Send function key F12
PRESS TAB                    # Move to next input field
PRESS ESC                    # Send Escape
PRESS KEY_A                  # Type the character 'A'
PRESS KEY_5                  # Type the character '5'
PRESS KEY_SPACE              # Type a space
PRESS KEY_PERIOD             # Type a period '.'
```

AID and local key names also work as standalone commands (e.g., `ENTER` on its own is equivalent to `PRESS ENTER`).

#### Complete Key Reference

##### AID Keys

AID (Attention Identifier) keys send data to the host and lock the keyboard until the host responds. After any AID key, script execution pauses automatically until the keyboard is unlocked.

| Key name    | Description                |
|-------------|----------------------------|
| `ENTER`     | Enter / Record Advance     |
| `F1`        | Function key 1             |
| `F2`        | Function key 2             |
| `F3`        | Function key 3             |
| `F4`        | Function key 4             |
| `F5`        | Function key 5             |
| `F6`        | Function key 6             |
| `F7`        | Function key 7             |
| `F8`        | Function key 8             |
| `F9`        | Function key 9             |
| `F10`       | Function key 10            |
| `F11`       | Function key 11            |
| `F12`       | Function key 12            |
| `F13`       | Function key 13            |
| `F14`       | Function key 14            |
| `F15`       | Function key 15            |
| `F16`       | Function key 16            |
| `F17`       | Function key 17            |
| `F18`       | Function key 18            |
| `F19`       | Function key 19            |
| `F20`       | Function key 20            |
| `F21`       | Function key 21            |
| `F22`       | Function key 22            |
| `F23`       | Function key 23            |
| `F24`       | Function key 24            |
| `PAGEUP`    | Page Up (Roll Up)          |
| `PAGEDOWN`  | Page Down (Roll Down)      |
| `ATTN`      | Attention                  |
| `SYSREQ`    | System Request             |
| `HELP`      | Help                       |
| `CLEAR`     | Clear screen               |
| `PRINT`     | Print                      |

##### Local Keys

Local keys are processed by the terminal without a host round-trip.

| Key name     | Description                       |
|--------------|-----------------------------------|
| `TAB`        | Move to next input field          |
| `BACKTAB`    | Move to previous input field      |
| `BACKSPACE`  | Delete character before cursor    |
| `DELETE`     | Delete character at cursor        |
| `INSERT`     | Toggle insert mode                |
| `HOME`       | Move to first input field         |
| `END`        | Move to end of current field      |
| `ESC`        | Escape                            |
| `FIELDPLUS`  | Field Plus (numeric fields)       |
| `FIELDMINUS` | Field Minus (numeric fields)      |
| `FIELDEXIT`  | Exit current field                |
| `DUP`        | Duplicate key                     |
| `ERASEINPUT` | Erase all input fields            |
| `ERASEFIELD` | Erase current field               |
| `ERASEEOF`   | Erase from cursor to end of field |

##### Character Keys (`KEY_*`)

Character key constants type a single character. Use `TYPE` for multi-character strings.

| Key name         | Character | Description        |
|------------------|-----------|--------------------|
| `KEY_A`–`KEY_Z`  | A–Z       | Letter keys        |
| `KEY_0`–`KEY_9`  | 0–9       | Digit keys         |
| `KEY_SPACE`      | ` `       | Space              |
| `KEY_PERIOD`     | `.`       | Period             |
| `KEY_COMMA`      | `,`       | Comma              |
| `KEY_SEMICOLON`  | `;`       | Semicolon          |
| `KEY_COLON`      | `:`       | Colon              |
| `KEY_SLASH`      | `/`       | Forward slash      |
| `KEY_BACKSLASH`  | `\`       | Backslash          |
| `KEY_MINUS`      | `-`       | Minus / Hyphen     |
| `KEY_PLUS`       | `+`       | Plus               |
| `KEY_EQUAL`      | `=`       | Equals             |
| `KEY_UNDERSCORE` | `_`       | Underscore         |
| `KEY_QUOTE`      | `'`       | Single quote       |
| `KEY_DOUBLEQUOTE`| `"`       | Double quote       |
| `KEY_EXCLAIM`    | `!`       | Exclamation mark   |
| `KEY_QUESTION`   | `?`       | Question mark      |
| `KEY_AT`         | `@`       | At sign            |
| `KEY_HASH`       | `#`       | Hash / Number sign |
| `KEY_DOLLAR`     | `$`       | Dollar sign        |
| `KEY_PERCENT`    | `%`       | Percent            |
| `KEY_CARET`      | `^`       | Caret              |
| `KEY_AMPERSAND`  | `&`       | Ampersand          |
| `KEY_ASTERISK`   | `*`       | Asterisk           |
| `KEY_PIPE`       | `\|`      | Pipe               |
| `KEY_TILDE`      | `~`       | Tilde              |
| `KEY_BACKQUOTE`  | `` ` ``   | Backtick           |
| `KEY_PAREN_LEFT` | `(`       | Left parenthesis   |
| `KEY_PAREN_RIGHT`| `)`       | Right parenthesis  |
| `KEY_BRACKET_LEFT` | `[`     | Left bracket       |
| `KEY_BRACKET_RIGHT`| `]`     | Right bracket      |
| `KEY_BRACE_LEFT` | `{`       | Left brace         |
| `KEY_BRACE_RIGHT`| `}`       | Right brace        |
| `KEY_LESS`       | `<`       | Less than          |
| `KEY_GREATER`    | `>`       | Greater than       |

### Cursor Movement

All cursor movement uses the `MOVE CURSOR` prefix:

```
MOVE CURSOR AT (5,20)              # Move cursor to row 5, column 20 (1-based)
MOVE CURSOR AT INPUTFIELD 1        # Move cursor to the 1st input field
MOVE CURSOR AT INPUTFIELD 3        # Move cursor to the 3rd input field
MOVE CURSOR AT NEXT INPUTFIELD     # Move cursor to the next input field
MOVE CURSOR AT PREVIOUS INPUTFIELD # Move cursor to the previous input field
MOVE CURSOR UP 1                   # Move cursor up one row
MOVE CURSOR DOWN 3                 # Move cursor down three rows
MOVE CURSOR LEFT 2                 # Move cursor left two columns
MOVE CURSOR RIGHT                  # Move cursor right one column (count defaults to 1)
```

`MOVE CURSOR AT INPUTFIELD n` moves the cursor to the start of the Nth unprotected (input) field on screen, counting from 1. Protected (display-only) fields are skipped. `NEXT INPUTFIELD` and `PREVIOUS INPUTFIELD` move relative to the current cursor position.

`MOVE CURSOR UP/DOWN/LEFT/RIGHT [n]` moves the cursor in the given direction `n` times. The count is optional and defaults to 1.

All positions in 5250Script are **1-based** (row 1, col 1 is the top-left corner).

### Screen Inspection — EXPECT

`EXPECT` pauses execution until a condition is met or the timeout expires. On success, `$EXPECT_RESULT` is set to `"OK"`. On timeout, it is set to `"TIMEOUT"`.

#### Text Matching

```
EXPECT TEXT "Sign On"               # Text appears anywhere on screen
EXPECT TEXT "Sign On" AT 1 23       # Text at exact row 1, column 23
EXPECT TEXT "Sign On" AT ROW 1      # Text anywhere on row 1
```

#### Cursor Position

```
EXPECT CURSOR AT 6 20               # Cursor is at row 6, column 20
EXPECT CURSOR AT ROW 6              # Cursor is anywhere on row 6
```

#### Keyboard State

```
EXPECT KEYBOARD UNLOCKED            # Keyboard is unlocked (ready for input)
EXPECT KEYBOARD ERRORLOCKED         # Keyboard is in error-lock state
```

#### Field Content

```
EXPECT FIELD AT 6 20 CONTAINS "X"   # Field at position contains text
```

#### Message Waiting

```
EXPECT MESSAGEWAITING               # Message-waiting indicator is on
```

#### Negation

Any EXPECT condition can be negated with `NOT`:

```
EXPECT NOT TEXT "Error"              # Screen does NOT contain "Error"
```

### Screen Reading — EXTRACT

Read values from the screen into variables.

```
EXTRACT $TITLE FROM 1 1 LENGTH 40   # Read 40 characters starting at row 1, col 1
EXTRACT $VAL FIELD AT 6 20          # Read entire field content at position
EXTRACT $ROW CURSOR ROW             # Read current cursor row number
EXTRACT $COL CURSOR COL             # Read current cursor column number
```

### Timing

```
WAIT 2000          # Pause execution for 2000 milliseconds
```

### Global Settings

`GLOBAL` commands configure persistent execution behavior for the rest of the script.

```
GLOBAL EXPECT_TIMEOUT 10000   # Set EXPECT timeout to 10000 ms (default: 30000 ms)
GLOBAL DELAY 100              # Set fixed inter-action delay to 100 ms
GLOBAL DELAY 0                # Reset to no delay (default)
GLOBAL JITTER 50 200          # Set random inter-action delay between 50–200 ms
GLOBAL JITTER 0 0             # Disable jitter (default)
```

**`GLOBAL EXPECT_TIMEOUT ms`** sets the maximum time (in milliseconds) that `EXPECT` commands will wait for their condition to be met before timing out. Default is 30000 ms (30 seconds).

**`GLOBAL DELAY ms`** sets a fixed minimum delay (in milliseconds) between every action. Useful for slowing down execution to observe the script running or to pace interactions with a slow host.

**`GLOBAL JITTER min max`** adds a random delay between `min` and `max` milliseconds (inclusive) after each action. The actual delay per action is the greater of the fixed delay and the random jitter value. This is useful for simulating realistic human-like timing.

All settings persist until changed by another `GLOBAL` command.

### Variables

#### Setting Variables

```
SET $USERNAME "QSECOFR"    # Set variable to a string value
SET $COUNT 0               # Set variable to a numeric value
INC $COUNT                 # Increment by 1
DEC $COUNT                 # Decrement by 1
ADD $COUNT 5               # Add a value
```

#### Variable Interpolation

Variables are expanded in `TYPE`, `LOG`, and `SET` values:

```
SET $USER "QSECOFR"
TYPE "$USER"              # Types "QSECOFR"
LOG "User is $USER"       # Logs "User is QSECOFR"
TYPE "$$100"              # Types literal "$100"
```

#### Built-in Variables (Read-Only)

| Variable           | Description                            |
|--------------------|----------------------------------------|
| `$SCREEN_ROWS`     | Number of screen rows (e.g., 24)       |
| `$SCREEN_COLS`     | Number of screen columns (e.g., 80)    |
| `$CURSOR_ROW`      | Current cursor row (1-based)           |
| `$CURSOR_COL`      | Current cursor column (1-based)        |
| `$KEYBOARD_STATE`  | `UNLOCKED`, `LOCKED`, `ERRORLOCKED`, or `SYSTEMREQUEST` |
| `$EXPECT_RESULT`   | `OK` or `TIMEOUT` (set by last EXPECT) |
| `$MESSAGE_WAITING` | `1` if message waiting, `0` otherwise  |
| `$REPEAT_INDEX`    | Current iteration index in REPEAT (0-based) |

### Control Flow

#### IF / ELSE / ENDIF

```
IF $EXPECT_RESULT == "TIMEOUT"
    ABORT "Timed out waiting for screen"
ELSE
    LOG "Screen appeared"
ENDIF
```

The `ELSE` block is optional. Blocks can be nested.

**Comparison operators:** `==`, `!=`, `<`, `>`, `<=`, `>=`

Numeric values are compared numerically; strings are compared lexicographically.

#### ISSET

`ISSET` checks whether a variable has been defined:

```
IF ISSET $SESSION_USERNAME
    TYPE "$SESSION_USERNAME"
ELSE
    TYPE "QSECOFR"
ENDIF
```

This is useful for checking whether session variables (like `$SESSION_USERNAME` and `$SESSION_PASSWORD`) were provided before using them.

#### WHILE / ENDWHILE

```
SET $I 0
WHILE $I < 5
    PRESS ENTER
    EXPECT KEYBOARD UNLOCKED
    INC $I
ENDWHILE
```

#### REPEAT / ENDREPEAT

```
REPEAT 3
    TYPE "test"
    PRESS ENTER
ENDREPEAT
```

Inside a REPEAT block, `$REPEAT_INDEX` holds the current iteration (0, 1, 2, ...).

#### LABEL / GOTO

```
LABEL retry
EXPECT TEXT "Sign On"
IF $EXPECT_RESULT == "TIMEOUT"
    GOTO retry
ENDIF
```

Labels are only valid at the top level of the script (not inside IF/WHILE/REPEAT blocks). GOTO jumps to the statement after the label.

### Error Handling

#### Global Handlers

```
ON TIMEOUT GOTO handler    # Any EXPECT timeout jumps to this label
ON ERROR GOTO handler      # Keyboard error-lock jumps to this label
```

Global handlers remain active until overwritten or the script ends.

#### ABORT

```
ABORT                      # Stop script execution immediately
ABORT "Login failed"       # Stop with an error message (shown in dialog)
```

### User Input

`INPUT` pauses execution and shows a popup dialog asking the user to fill in a value. The result is stored in the given variable.

```
INPUT "Username:" $USER
INPUT "Password:" $PASS
```

When multiple `INPUT` commands appear on consecutive lines, they are batched into a **single dialog** with all fields:

```
INPUT "Username:" $USER
INPUT "Password:" $PASS
INPUT "Library:" $LIB
```

This shows one popup with three rows:

| Label     | Field |
|-----------|-------|
| Username: | _____ |
| Password: | _____ |
| Library:  | _____ |

Clicking **OK** sets all variables and continues. Clicking **Cancel** stops the script.

### Utility

```
LOG "Currently on: $TITLE" # Write message to session log
PAUSE                      # Show dialog and wait for user to click OK
```

---

## Examples

### Login with Retry

```
# AS/400 Login
GLOBAL EXPECT_TIMEOUT 15000
SET $RETRIES 0

LABEL login
EXPECT TEXT "Sign On" AT 1 23
MOVE CURSOR AT (6,53)
TYPE "QSECOFR"
MOVE CURSOR AT (7,53)
TYPE "mypassword"
PRESS ENTER

EXPECT TEXT "MAIN MENU" AT 1 30
IF $EXPECT_RESULT == "TIMEOUT"
    INC $RETRIES
    IF $RETRIES < 3
        LOG "Retry $RETRIES"
        GOTO login
    ENDIF
    ABORT "Login failed after 3 retries"
ENDIF
LOG "Login successful"
```

### Navigate a Menu

```
# Open Work with Jobs
EXPECT TEXT "MAIN MENU"
TYPE "WRKACTJOB"
PRESS ENTER

EXPECT TEXT "Work with Active Jobs"
LOG "Navigated to WRKACTJOB"
```

### Data Entry Loop

```
# Enter 10 Records
SET $COUNT 0

EXPECT TEXT "Data Entry"

REPEAT 10
    MOVE CURSOR AT (5,20)
    TYPE "Record $REPEAT_INDEX"
    PRESS TAB
    TYPE "Description for item $REPEAT_INDEX"
    PRESS ENTER
    EXPECT KEYBOARD UNLOCKED
ENDREPEAT

LOG "Entered 10 records"
```

### Screen Reading

```
# Read System Name
EXPECT TEXT "Sign On"
EXTRACT $SYSNAME FROM 1 1 LENGTH 10
LOG "Connected to: $SYSNAME"

EXTRACT $ROW CURSOR ROW
LOG "Cursor is on row $ROW"
```

### Error Recovery with ON TIMEOUT

```
# Robust Navigation
ON TIMEOUT GOTO timeout_handler

LABEL start
EXPECT TEXT "MAIN MENU"
TYPE "5"
PRESS ENTER
EXPECT TEXT "SUBMENU"
LOG "Navigation complete"
ABORT

LABEL timeout_handler
LOG "Timed out — trying again"
PRESS CLEAR
EXPECT KEYBOARD UNLOCKED
GOTO start
```

---

## Recording Scripts

Scripts can be recorded directly from the **Scripts** menu:

1. Click **Scripts > Record** to start recording your terminal session
2. Interact with the terminal as normal
3. Click **Scripts > Record** again to stop recording
4. Enter a name for the script and click **Save**

The recorder coalesces consecutive keystrokes into `TYPE` lines and maps keys to `PRESS` commands. The generated script is a faithful, linear replay — you can then edit it to add EXPECT waits, loops, and error handling.

---

## Running Scripts

1. **Scripts > Scripts > [script name] > Run** — run a saved script
2. **Scripts > New Script...** — create a new script from a template and open it in your editor
3. While a script is running, the status bar shows **SCRIPT** in cyan
4. To stop a running script, click **Scripts > Stop**

Scripts are stored in the application data directory under `scripts/`. On Linux this is typically `~/.local/share/5250ng/scripts/`.

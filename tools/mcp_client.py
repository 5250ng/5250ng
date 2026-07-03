#!/usr/bin/env python3
"""Python client for the 5250ng MCP server.

Provides a high-level class that wraps every MCP tool exposed by 5250ng
into a simple Python method call. Communication uses HTTP + JSON-RPC 2.0.

Usage:
    from mcp_client import Client5250

    client = Client5250("localhost", 5250)
    client.initialize()

    session_id = client.create_session("myhost.example.com")
    client.login(session_id, "MYUSER", "MYPASS")

    print(client.read_screen(session_id))
    client.send_keys(session_id, '"WRKACTJOB" ENTER')

    client.close_session(session_id)
"""

from __future__ import annotations

import http.client
import json
from typing import Any


class McpError(Exception):
    """Raised when the MCP server returns a JSON-RPC error or a tool error."""

    def __init__(self, message: str, code: int | None = None):
        super().__init__(message)
        self.code = code


class Client5250:
    """High-level Python wrapper around the 5250ng MCP server.

    All tool methods return the text content of the MCP response on success
    and raise :class:`McpError` on failure.
    """

    def __init__(self, host: str = "localhost", port: int = 5250):
        self._host = host
        self._port = port
        self._request_id = 0
        self._session_id: str | None = None  # MCP protocol session
        self._endpoint = "/mcp"

    # ------------------------------------------------------------------
    # Low-level JSON-RPC helpers
    # ------------------------------------------------------------------

    def _next_id(self) -> int:
        self._request_id += 1
        return self._request_id

    def _rpc(self, method: str, params: dict[str, Any] | None = None) -> Any:
        """Send a JSON-RPC 2.0 request and return the ``result`` field."""
        payload = {
            "jsonrpc": "2.0",
            "id": self._next_id(),
            "method": method,
        }
        if params is not None:
            payload["params"] = params

        body = json.dumps(payload).encode()
        headers = {"Content-Type": "application/json"}
        if self._session_id:
            headers["Mcp-Session-Id"] = self._session_id

        conn = http.client.HTTPConnection(self._host, self._port)
        conn.request("POST", self._endpoint, body=body, headers=headers)
        resp = conn.getresponse()
        data = resp.read()
        conn.close()

        if not data:
            return None

        result = json.loads(data)
        if "error" in result:
            err = result["error"]
            raise McpError(err.get("message", str(err)), err.get("code"))

        return result.get("result")

    def _call_tool(self, name: str, arguments: dict[str, Any] | None = None) -> str:
        """Call an MCP tool and return its text content.

        Raises :class:`McpError` if the tool signals an error.
        """
        result = self._rpc("tools/call", {
            "name": name,
            "arguments": arguments or {},
        })
        # Extract text from the MCP content array
        is_error = result.get("isError", False)
        content_list = result.get("content", [])
        text = "\n".join(c.get("text", "") for c in content_list)

        if is_error:
            raise McpError(text)
        return text

    # ------------------------------------------------------------------
    # MCP protocol methods
    # ------------------------------------------------------------------

    def initialize(self) -> dict[str, Any]:
        """Initialize the MCP protocol session. Must be called first.

        Returns the server capability object.
        """
        result = self._rpc("initialize", {
            "protocolVersion": "2025-03-26",
            "capabilities": {},
            "clientInfo": {"name": "python-5250ng-client", "version": "1.0.0"},
        })
        self._session_id = None  # will be set from response headers if needed
        return result

    def ping(self) -> None:
        """Send a ping to verify the server is responsive."""
        self._rpc("ping")

    def list_tools(self) -> list[dict[str, Any]]:
        """Return the list of tools the server exposes."""
        result = self._rpc("tools/list")
        return result.get("tools", [])

    # ------------------------------------------------------------------
    # Session lifecycle tools
    # ------------------------------------------------------------------

    def create_session(
        self,
        hostname: str,
        port: int = 23,
        use_tls: bool = False,
    ) -> str:
        """Create a new TN5250 session connected to *hostname*.

        Returns the ``session_id`` needed by all session-aware methods.
        """
        text = self._call_tool("create_session", {
            "hostname": hostname,
            "port": port,
            "useTLS": use_tls,
        })
        # Response format: "session_id: <uuid>\nConnected to host:port"
        for line in text.splitlines():
            if line.startswith("session_id:"):
                return line.split(":", 1)[1].strip()
        raise McpError("Could not parse session_id from response: " + text)

    def close_session(self, session_id: str) -> str:
        """Close a TN5250 session."""
        return self._call_tool("close_session", {"session_id": session_id})

    def list_sessions(self) -> str:
        """List all active MCP-managed sessions.

        Returns a human-readable multi-line string with session details.
        """
        return self._call_tool("list_sessions")

    # ------------------------------------------------------------------
    # Screen tools (require session_id)
    # ------------------------------------------------------------------

    def read_screen(self, session_id: str) -> str:
        """Read the full text content of the 5250 terminal screen."""
        return self._call_tool("read_screen", {"session_id": session_id})

    def get_cursor_position(self, session_id: str) -> tuple[int, int]:
        """Return the current cursor position as ``(row, col)`` (0-based)."""
        text = self._call_tool("get_cursor_position", {"session_id": session_id})
        # Response format: "row: N, col: M"
        parts = {}
        for token in text.split(","):
            key, _, value = token.partition(":")
            parts[key.strip()] = int(value.strip())
        return parts["row"], parts["col"]

    def get_field_at(self, session_id: str, row: int, col: int) -> dict[str, Any]:
        """Get field information at a screen position.

        Returns a dict with keys: ``startRow``, ``startCol``, ``length``,
        ``protected``, ``modified``, ``text``.
        """
        text = self._call_tool("get_field_at", {
            "session_id": session_id,
            "row": row,
            "col": col,
        })
        # Response format: "startRow: N, startCol: N, length: N, protected: bool, modified: bool, text: "...""
        result: dict[str, Any] = {}
        for token in text.split(", "):
            key, _, value = token.partition(": ")
            if key in ("startRow", "startCol", "length"):
                result[key] = int(value)
            elif key in ("protected", "modified"):
                result[key] = value == "true"
            elif key == "text":
                result[key] = value.strip('"')
        return result

    def get_screen_size(self, session_id: str) -> tuple[int, int]:
        """Return the screen dimensions as ``(rows, cols)``."""
        text = self._call_tool("get_screen_size", {"session_id": session_id})
        parts = {}
        for token in text.split(","):
            key, _, value = token.partition(":")
            parts[key.strip()] = int(value.strip())
        return parts["rows"], parts["cols"]

    def set_cursor_position(self, session_id: str, row: int, col: int) -> str:
        """Move the cursor to an absolute screen position (0-based)."""
        return self._call_tool("set_cursor_position", {
            "session_id": session_id,
            "row": row,
            "col": col,
        })

    def move_cursor(
        self, session_id: str, rows: int = 0, cols: int = 0
    ) -> str:
        """Move the cursor relative to its current position.

        Positive *rows* moves down, negative moves up.
        Positive *cols* moves right, negative moves left.
        """
        return self._call_tool("move_cursor", {
            "session_id": session_id,
            "rows": rows,
            "cols": cols,
        })

    def find_text(self, session_id: str, text: str) -> list[tuple[int, int]]:
        """Find all occurrences of *text* on the screen.

        Returns a list of ``(row, col)`` tuples (0-based).
        Raises :class:`McpError` if the text is not found.
        """
        result = self._call_tool("find_text", {
            "session_id": session_id,
            "text": text,
        })
        matches = []
        for line in result.splitlines():
            parts = {}
            for token in line.split(","):
                key, _, value = token.partition(":")
                parts[key.strip()] = int(value.strip())
            matches.append((parts["row"], parts["col"]))
        return matches

    def wait_for_text(
        self, session_id: str, text: str, timeout: int = 30000
    ) -> str:
        """Wait until *text* appears on the screen.

        Blocks until the text is found or *timeout* (milliseconds) expires.
        """
        return self._call_tool("wait_for_text", {
            "session_id": session_id,
            "text": text,
            "timeout": timeout,
        })

    def read_line(self, session_id: str, row: int) -> str:
        """Read a single line of text from the screen at *row* (0-based)."""
        return self._call_tool("read_line", {
            "session_id": session_id,
            "row": row,
        })

    def read_region(
        self,
        session_id: str,
        row: int,
        col: int,
        num_rows: int,
        num_cols: int,
    ) -> str:
        """Read a rectangular region of text from the screen.

        All coordinates are 0-based.
        """
        return self._call_tool("read_region", {
            "session_id": session_id,
            "row": row,
            "col": col,
            "numRows": num_rows,
            "numCols": num_cols,
        })

    def screenshot(self, session_id: str, path: str) -> str:
        """Capture the terminal screen as a PNG image saved to *path*."""
        return self._call_tool("screenshot", {
            "session_id": session_id,
            "path": path,
        })

    # ------------------------------------------------------------------
    # Input / script tools (require session_id)
    # ------------------------------------------------------------------

    def press_key(self, session_id: str, key: str) -> str:
        """Press a single key on the terminal.

        *key* is one key name, e.g. ``"ENTER"``, ``"F5"``, ``"TAB"``.

        Supported keys: ENTER, F1-F24, TAB, BACKTAB, BACKSPACE, DELETE,
        INSERT, HOME, END, ESCAPE, PAGEUP, PAGEDOWN, FIELDPLUS, FIELDMINUS,
        FIELDEXIT, DUP, ERASEINPUT, ERASEFIELD, ERASEEOF, ATTN, SYSREQ,
        HELP, CLEAR, PRINT, UP, DOWN, LEFT, RIGHT.

        PAGEDOWN shows the next page (5250 Roll Up), PAGEUP the previous
        page (Roll Down).
        """
        return self._call_tool("press_key", {
            "session_id": session_id,
            "key": key,
        })

    def press_keys(self, session_id: str, keys: list[str]) -> str:
        """Press a sequence of keys on the terminal.

        *keys* is an ordered list of key names, e.g. ``["F5", "ENTER"]``.
        Same supported keys as :meth:`press_key`.
        """
        return self._call_tool("press_keys", {
            "session_id": session_id,
            "keys": keys,
        })

    def type_text(self, session_id: str, text: str) -> str:
        """Type a text string at the current cursor position.

        *text* is typed character by character into the current field.
        Does not press Enter or any other key after typing.
        """
        return self._call_tool("type_text", {
            "session_id": session_id,
            "text": text,
        })

    def send_keys(self, session_id: str, keys: str) -> str:
        """Send keystrokes to the terminal session.

        *keys* is a space-separated string of key names. Text is quoted:
        ``'"hello" ENTER'``.

        Supported keys: ENTER, F1-F24, TAB, BACKSPACE, PAGEUP, PAGEDOWN,
        HOME, END, INSERT, DELETE, ESCAPE, UP, DOWN, LEFT, RIGHT, FIELDEXIT.
        PAGEDOWN shows the next page (5250 Roll Up), PAGEUP the previous
        page (Roll Down).
        """
        return self._call_tool("send_keys", {
            "session_id": session_id,
            "keys": keys,
        })

    def run_script(self, session_id: str, script: str) -> str:
        """Execute a 5250script on the terminal session.

        *script* is the full 5250script source code.
        """
        return self._call_tool("run_5250script", {
            "session_id": session_id,
            "script": script,
        })

    def login(self, session_id: str, username: str, password: str) -> str:
        """Log in to the AS/400 on the given session.

        Waits for the sign-on screen, types credentials, and presses Enter.
        Handles auto-signoff recovery and "Display Program Messages" screens.
        """
        return self._call_tool("login", {
            "session_id": session_id,
            "username": username,
            "password": password,
        })

    def clear_inputs(self, session_id: str) -> str:
        """Clear all input fields on the current screen (Erase Input)."""
        return self._call_tool("clear_inputs", {"session_id": session_id})

    # ------------------------------------------------------------------
    # Filesystem tools (no session_id needed)
    # ------------------------------------------------------------------

    def list_files(self, path: str = "") -> str:
        """List files and directories at *path* on the server host."""
        args = {}
        if path:
            args["path"] = path
        return self._call_tool("list_files", args)

    def read_file(self, path: str) -> str:
        """Read a file from the server host filesystem."""
        return self._call_tool("read_file", {"path": path})

    def write_file(self, path: str, content: str) -> str:
        """Write *content* to a file on the server host filesystem."""
        return self._call_tool("write_file", {"path": path, "content": content})


# ----------------------------------------------------------------------
# CLI demo
# ----------------------------------------------------------------------

if __name__ == "__main__":
    import argparse
    import sys

    parser = argparse.ArgumentParser(description="5250ng MCP client demo")
    parser.add_argument("--mcp-host", default="localhost", help="MCP server host")
    parser.add_argument("--mcp-port", type=int, default=5250, help="MCP server port")
    parser.add_argument("--hostname", required=True, help="AS/400 hostname to connect to")
    parser.add_argument("--port", type=int, default=23, help="AS/400 TN5250 port")
    parser.add_argument("--tls", action="store_true", help="Use TLS")
    parser.add_argument("--username", help="AS/400 username (optional, for login)")
    parser.add_argument("--password", help="AS/400 password (optional, for login)")
    args = parser.parse_args()

    client = Client5250(args.mcp_host, args.mcp_port)

    print("[*] Initializing MCP session ...")
    info = client.initialize()
    print(f"    Server: {info['serverInfo']['name']} v{info['serverInfo']['version']}")

    print(f"[*] Creating session to {args.hostname}:{args.port} (TLS={args.tls}) ...")
    try:
        session_id = client.create_session(args.hostname, args.port, args.tls)
    except McpError as e:
        print(f"    ERROR: {e}", file=sys.stderr)
        sys.exit(1)
    print(f"    Session ID: {session_id}")

    if args.username:
        print(f"[*] Logging in as {args.username} ...")
        try:
            client.login(session_id, args.username, args.password or "")
        except McpError as e:
            print(f"    Login error: {e}", file=sys.stderr)

    print("[*] Reading screen ...")
    screen = client.read_screen(session_id)
    print(screen)

    row, col = client.get_cursor_position(session_id)
    print(f"[*] Cursor at row={row}, col={col}")

    print("[*] Closing session ...")
    client.close_session(session_id)
    print("[*] Done.")

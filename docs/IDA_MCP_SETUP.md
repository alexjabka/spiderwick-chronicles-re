# IDA Pro MCP Setup

Connects Claude Code to IDA Pro for direct interaction — rename functions, decompile, disassemble, search strings, etc.

## Requirements

- IDA Pro with Python support
- Claude Code (VS Code extension or CLI)
- GitHub: https://github.com/mrexodia/ida-pro-mcp
- IDA Plugin Manager: https://plugins.hex-rays.com/mrexodia/ida-pro-mcp

## Installation

### 1. Install the IDA plugin

```
pip install https://github.com/mrexodia/ida-pro-mcp/archive/refs/heads/main.zip
ida-pro-mcp --install
```

This automatically installs the plugin into IDA and configures the MCP server.
Restart IDA after installation.

### 2. Start MCP server in IDA

1. Open your .idb in IDA
2. Edit → Plugins → MCP (or it starts automatically)
3. Server runs at `http://127.0.0.1:13337/mcp`

### 3. Configure Claude Code

In VS Code: `Ctrl+Shift+P` → "MCP: Add Server" → "HTTP" → enter URL:

```
http://127.0.0.1:13337/mcp
```

This creates an entry in `.vscode/mcp.json` (workspace) or user-level config:

```json
{
  "servers": {
    "ida-pro-mcp": {
      "url": "http://127.0.0.1:13337/mcp"
    }
  }
}
```

### 4. Verify

In Claude Code, tools like `mcp__ida-pro-mcp__disasm`, `mcp__ida-pro-mcp__decompile`, etc. should appear.

## Known Issues

- **IDA 7.7**: `decompile` tool fails — `hexrays_failure_t` SWIG binding broken. Fixed in IDA 9.
- **`rename` tool fails on some functions**: tries to decompile after renaming, hits the same bug. Workaround: use `py_eval` with `idc.set_name(addr, name, 0x1)`.

## Useful Tools

| Tool | Description |
|------|-------------|
| `disasm` | Disassemble function at address |
| `decompile` | Decompile to pseudocode (needs working Hex-Rays) |
| `rename` | Batch rename functions/globals |
| `py_eval` | Execute Python in IDA context |
| `find` / `find_regex` | Search strings/immediates |
| `xrefs_to` / `xref_query` | Cross-references |
| `entity_query` | Query functions/globals/names/strings |
| `idb_save` | Save IDA database |

## Reinstalling After IDA Upgrade

When upgrading IDA (e.g. 7.7 → 9.x):
1. Check for updated plugin version on GitHub
2. Copy `ida_mcp.py` + `ida_mcp/` to new IDA plugins directory
3. VS Code MCP config stays the same (same URL)

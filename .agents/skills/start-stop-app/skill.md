---
name: Start/Stop Demo App
description: Start or stop the demo server (FastAPI) on port 2026. Use when you need to run or kill the development server.
model: ""
---

# Start/Stop Demo App

Manage the demo FastAPI development server running on port 2026.

## Tools

- `tools/start.py` - Start the demo server (`apps/server/main.py`)
- `tools/stop.py` - Stop the server running on port 2026

## Usage

```bash
# Start the app
python3 .agents/skills/start-stop-app/tools/start.py

# Stop the app
python3 .agents/skills/start-stop-app/tools/stop.py
```

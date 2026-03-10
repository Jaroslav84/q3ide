---
description: Start the application
model: ""
---

# Start Apps

Start the demo server (FastAPI) on port 2026.

## Workflow

1. EXECUTE the start skill
   ```bash
   python3 .agents/skills/start-stop-app/tools/start.py
   ```

2. VERIFY the app is running
   ```bash
   python3 apps/client/main.py
   ```

## Report

Confirm the server started and the client successfully connected.

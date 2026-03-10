---
description: Closed loop backend testing - runs and fixes pytest for server
model: ""
---

# Test Backend (Closed Loop)

Run backend tests and fix failures automatically.

## Closed Loop Pattern

### 1. Request
Run the backend test suite.

### 2. Validate
```bash
cd apps/server
python3 -m pytest
```

### 3. Resolve
If tests fail:
- Analyze pytest output
- Fix the failing code or test
- Return to step 2

Loop exits when all tests pass.

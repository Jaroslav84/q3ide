---
description: Closed loop client testing - runs and fixes pytest for client
model: ""
---

# Test Client (Closed Loop)

Run client tests and fix failures automatically.

## Closed Loop Pattern

### 1. Request
Run the client test suite.

### 2. Validate
```bash
cd apps/client
python3 -m pytest
```

### 3. Resolve
If tests fail:
- Read the failing test output
- Fix the component or test
- Return to step 2

Loop exits when all tests pass.

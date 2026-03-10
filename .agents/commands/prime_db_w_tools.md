---
description: Prime agent with Database context via psql
allowed-tools: Read, Glob, Grep, Bash
model: ""
---

# Prime: Database

Connect to the project postgres database and understand the schema.

## Database Connection

```bash
# Assumes PG env vars are set or .env is present
export PGPASSWORD=$(grep DB_PASS .env | cut -d'=' -f2)
psql -h localhost -p 5432 -U postgres -d postgres
```

## Workflow

1. READ .env for DB credentials (if applicable)
2. Verify connection with `psql -c "SELECT 1;"`
3. Query `information_schema` to understand tables.
4. Execute queries as needed for the task.

## Useful Commands

```bash
# List all tables
psql -c "\dt"

# Describe a table
psql -c "\d table_name"
```

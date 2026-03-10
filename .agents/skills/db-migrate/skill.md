---
name: Database Migration
description: Executes database migrations using the postgres MCP server. Use when applying schema changes, running migrations, or updating database structure.
model: ""
---

# Database Migration

Execute and manage database migrations safely using MCP tools.

## Instructions

### Prerequisites

- Postgres MCP server configured in .mcp.json
- Migration files in migrations/ directory
- Database connection verified

### Workflow

1. List pending migrations:
   ```sql
   SELECT * FROM schema_migrations ORDER BY version;
   ```

2. Read migration file from `migrations/{version}.sql`

3. Execute in transaction:
   ```sql
   BEGIN;
   -- migration SQL here
   INSERT INTO schema_migrations (version) VALUES ('{version}');
   COMMIT;
   ```

4. Verify schema changes:
   ```sql
   SELECT column_name, data_type
   FROM information_schema.columns
   WHERE table_name = '{table}';
   ```

5. Update Pydantic models in `modules/schemas.py` if needed

## Examples

### Example 1: Run a migration

User: "Apply the add-users-table migration"

1. Read `migrations/001_add_users_table.sql`
2. Execute via `mcp__postgres__query`
3. Verify users table exists
4. Update `modules/schemas.py` with new model
5. Confirm migration recorded

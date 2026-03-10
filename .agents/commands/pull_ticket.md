---
description: Pull a ticket from the issue tracker (Jira)
model: ""
---

# Pull Ticket

Retrieve, read, and summarize a ticket from Jira.

## Variables

TICKET_ID: $ARGUMENTS

## Workflow

1. READ the ticket using the Atlassian MCP server
   - Use `mcp__atlassian__get_issue` (or equivalent available tool) with `TICKET_ID`
   
2. ANALYZE the ticket content
   - Summarize the description
   - List acceptance criteria
   - Identify formatting requirements

## Report

Create a summary in markdown format:

```markdown
# Ticket: {TICKET_ID}

## Summary
{summary}

## Requirements
- {req_1}
- {req_2}
```

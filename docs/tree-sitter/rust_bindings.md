# Tree-sitter Rust Bindings: UML Cache Daemon Reference

## Overview

Tree-sitter is a parser generator and incremental parsing library with Rust bindings. For the Q3IDE UML cache daemon, it provides fast syntax tree parsing and traversal for C and Rust source files, enabling extraction of function signatures, struct definitions, and implementation blocks.

## Cargo Dependencies

Add to `Cargo.toml`:

```toml
[dependencies]
tree-sitter = "0.21"
tree-sitter-c = "0.21"
tree-sitter-rust = "0.21"
```

**Note:** Ensure all three crates use matching major versions to avoid `Language` type conflicts.

## Basic Setup: Parser & Language

```rust
use tree_sitter::{Parser, Language};

extern "C" {
    fn tree_sitter_c() -> Language;
    fn tree_sitter_rust() -> Language;
}

fn parse_c_code(source: &str) -> Result<tree_sitter::Tree, String> {
    let mut parser = Parser::new();
    let language = unsafe { tree_sitter_c() };

    parser.set_language(language)
        .map_err(|_| "Failed to set C language".to_string())?;

    parser.parse(source, None)
        .ok_or_else(|| "Parse failed".to_string())
}
```

## Tree & Node Traversal

Every parse returns a `Tree` containing a root `Node`. Traverse with:

```rust
let tree = parse_c_code(source)?;
let root = tree.root_node();

// Basic properties
println!("Kind: {}", root.kind());           // e.g., "translation_unit"
println!("Children: {}", root.child_count()); // Number of direct children

// Iterate children
for i in 0..root.child_count() {
    if let Some(child) = root.child(i) {
        println!("  {}: {}", i, child.kind());
    }
}

// Extract text from source (requires source bytes)
if let Ok(text) = root.utf8_text(source.as_bytes()) {
    println!("Node text: {}", text);
}
```

## Efficient Traversal with TreeCursor

For large trees, `TreeCursor` is faster than repeated `child()` calls:

```rust
let mut cursor = root.walk();

loop {
    let node = cursor.node();
    println!("{}: {}", node.kind(), node.start_byte());

    if cursor.goto_first_child() {
        // Process children depth-first
        continue;
    }

    // Move to next sibling or backtrack
    while !cursor.goto_next_sibling() {
        if !cursor.goto_parent() {
            break; // Done
        }
    }
}
```

## Query System: Pattern Matching

Queries use S-expression syntax to extract specific nodes. Create with `Query::new()`:

```rust
use tree_sitter::{Query, QueryCursor};

// Query to find all function definitions in C
let query_src = r#"
    (function_definition
        declarator: (function_declarator
            declarator: (identifier) @func.name
        ) @func.decl
        type: (_) @func.type
    ) @function
"#;

let query = Query::new(unsafe { tree_sitter_c() }, query_src)?;
let mut cursor = QueryCursor::new();

for m in cursor.matches(&query, root, source.as_bytes()) {
    for cap in m.captures {
        let name = query.capture_names()[cap.index as usize];
        let text = cap.node.utf8_text(source.as_bytes())?;
        println!("{}: {}", name, text);
    }
}
```

## Extract All C Function Signatures

```rust
fn extract_c_functions(source: &str) -> Result<Vec<String>, String> {
    let mut parser = Parser::new();
    parser.set_language(unsafe { tree_sitter_c() })
        .map_err(|_| "Language error".to_string())?;

    let tree = parser.parse(source, None)
        .ok_or("Parse failed".to_string())?;

    let root = tree.root_node();
    let mut functions = Vec::new();

    // Simple query: find function_declaration nodes
    let query = Query::new(
        unsafe { tree_sitter_c() },
        "(function_definition) @func"
    )?;

    let mut cursor = QueryCursor::new();
    for m in cursor.matches(&query, root, source.as_bytes()) {
        for cap in m.captures {
            let func_node = cap.node;

            // Find function name (identifier child)
            for i in 0..func_node.child_count() {
                if let Some(child) = func_node.child(i) {
                    if child.kind() == "function_declarator" {
                        // Recurse to find identifier
                        if let Ok(sig) = child.utf8_text(source.as_bytes()) {
                            functions.push(sig.to_string());
                        }
                    }
                }
            }
        }
    }

    Ok(functions)
}
```

## Extract All Rust Structs & Impl Blocks

```rust
fn extract_rust_definitions(source: &str) -> Result<(Vec<String>, Vec<String>), String> {
    let mut parser = Parser::new();
    parser.set_language(unsafe { tree_sitter_rust() })
        .map_err(|_| "Language error".to_string())?;

    let tree = parser.parse(source, None)
        .ok_or("Parse failed".to_string())?;

    let root = tree.root_node();
    let mut structs = Vec::new();
    let mut impls = Vec::new();

    // Extract struct definitions
    let struct_query = Query::new(
        unsafe { tree_sitter_rust() },
        "(struct_item name: (type_identifier) @name) @struct"
    )?;

    let mut cursor = QueryCursor::new();
    for m in cursor.matches(&struct_query, root, source.as_bytes()) {
        for cap in m.captures {
            if let Ok(name) = cap.node.utf8_text(source.as_bytes()) {
                structs.push(name.to_string());
            }
        }
    }

    // Extract impl blocks
    let impl_query = Query::new(
        unsafe { tree_sitter_rust() },
        "(impl_item type: (type_identifier) @impl_type) @impl"
    )?;

    cursor = QueryCursor::new();
    for m in cursor.matches(&impl_query, root, source.as_bytes()) {
        for cap in m.captures {
            if let Ok(name) = cap.node.utf8_text(source.as_bytes()) {
                impls.push(name.to_string());
            }
        }
    }

    Ok((structs, impls))
}
```

## Error Handling

Check for parse errors in the tree:

```rust
fn check_errors(node: tree_sitter::Node) -> bool {
    if node.is_error() {
        eprintln!("Parse error at {}", node.start_byte());
        return true;
    }

    if node.is_missing() {
        eprintln!("Missing node at {}", node.start_byte());
        return true;
    }

    // Recursively check children
    for i in 0..node.child_count() {
        if let Some(child) = node.child(i) {
            if check_errors(child) {
                return true;
            }
        }
    }

    false
}

// After parsing:
if check_errors(tree.root_node()) {
    // Handle parse errors
}
```

## Best Practices for UML Cache Daemon

1. **Reuse parsers** – Create once, call `parser.parse()` multiple times (incremental)
2. **Combine queries + traversal** – Use queries for targeting, then traverse child nodes for complex extraction
3. **Cache trees** – Store `Tree` objects alongside source hashes for change detection
4. **Error nodes** – Always check `is_error()` before processing; skip malformed subtrees
5. **Memory** – `Query` objects are thread-safe; share across worker threads

## References

- [Tree-sitter Query API](https://docs.rs/tree-sitter/latest/tree_sitter/struct.Query.html)
- [Tree-sitter Node API](https://docs.rs/tree-sitter/latest/tree_sitter/struct.Node.html)
- [Query Pattern Guide](https://parsiya.net/blog/knee-deep-tree-sitter-queries/)
- [Tree-sitter Rust Bindings](https://github.com/tree-sitter/rust-tree-sitter)

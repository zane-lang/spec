# The Zane Language Specification

This repository is a **prior art publication** for the Zane programming language. The techniques described in these documents were conceived and developed by the author and are dedicated to the public domain under [CC0 1.0 Universal](LICENSE). Anyone may use these ideas freely, for any purpose, without restriction or attribution. No patent claims are made by the author.

---

| File | Description |
|---|---|
| `memory_model.md` | Ownership model, refs, anchors, scope rules, lifetime, and memory layout |
| `concurrency_model.md` | Implicit parallelism, `spawn`, water-tower lifetimes, and concurrency safety rules |
| `oop.md` | Classes, structs, constructors, methods, packages, and call resolution |
| `purity.md` | Effect model, `mut`, inferred effect levels, and compiler guarantees |
| `operators.md` | Operator set, derived operators, precedence, and boolean keywords |
| `type_parameters.md` | Const-parameterized types, identifier rules, and the `Array[size]<T>` primitive |
| `error_handling.md` | Bifurcated return paths, `?` handlers, and abort/resolve semantics |
| `dependency_management.md` | Package identity, manifests, version pinning, and fetch rules |
| `syntax.md` | Canonical surface syntax reference for all constructs |

# Zane Packages

This document specifies Zane's package model: package declarations, member naming, and the instanceful package pattern. Manifests, fetching, and version pinning live in [`dependencies.md`](dependencies.md).

> **See also:** [`types.md`](types.md) for classes and structs. [`functions.md`](functions.md) for methods and free functions. [`dependencies.md`](dependencies.md) for package identity, manifests, and the import system. [`syntax.md`](syntax.md) §8 for `package` and `$` syntax.

---

## 1. Overview

Zane packages are namespaces that host every type, function, and constant in the language. A package may also export a same-named class, letting one identifier serve as both a namespace and an instantiable type.

- **`PascalCase namespaces`.** Package names use PascalCase, like type names, so a same-named package class uses one spelling for both.
- **`Explicit member access`.** Cross-package members are addressed with `PackageName$member`.
- **`Instanceful package pattern`.** A package may declare a class with the same name; importing the package reserves that type spelling locally.
- **`No hidden ambient state`.** Packages expose constants and functions; time-varying state must live in objects and be passed explicitly.

---

## 2. Package Declarations and Member Access

### 2.1 Packages are PascalCase namespaces
`package PackageName` introduces a namespace. Package names use PascalCase, like type names. Members are referenced as `PackageName$member`.

### 2.2 Imports
A package becomes available with `import PackageName`. After import, both qualified (`PackageName$member`) and unqualified references (subject to method-lookup rules in [`functions.md`](functions.md) §6) are usable.

> **See also:** [`dependencies.md`](dependencies.md) §8 for the import syntax and resolution rules across the dependency graph.

---

## 3. The Instanceful Package Pattern

### 3.1 A package may define a same-named class
A package may define a class with the same name as the package, allowing both stateless namespace members and stateful instances:

```zane
package Math

pi Float(3.141592)

Float radsToDeg(x Float) {
    return x / pi * Float(180)
}

class Math {
    _deterministicRandomCounter Int
}

Math() => init{_deterministicRandomCounter: 0}

Int deterministicRandom(this Math) mut {
    ...
}
```

### 3.2 Same-named package classes are instantiated through the package name
If package `Math` defines class `Math`, a caller instantiates it exactly like any other class constructor:

```zane
package Main

import Math

Void main() {
    math Math()
    print(math!deterministicRandom())
}
```

The package name and the exported class name are the same identifier. The package still names namespace members such as `Math$radsToDeg`, and that same identifier also names the class constructor `Math()`.

### 3.3 Imported package names reserve that type name in the current package
If the current package imports `Math`, it **MUST NOT** also declare a top-level type named `Math`. The imported package name already occupies that type/namespace spelling:

```zane
package Main

import Math

class Math {   // ILLEGAL: duplicate type "Math"
    value Int
}
```

### 3.4 Package scope contains no hidden mutable ambient state
Packages may expose immutable constants and package-scope functions. Time-varying state must live in objects and be passed or stored explicitly.

---

## 4. Design Rationale

| Decision | Rationale |
|---|---|
| PascalCase package names | Aligns package names with type names so a same-named package class uses one spelling for the namespace and the instantiated object type. |
| `$` as the member separator | A non-letter separator removes any ambiguity between package-qualified access and ordinary identifier characters such as `.` (used for field access). |
| Instanceful package pattern | Lets a library expose both pure namespace members and a stateful instance type without splitting the API across two names. |
| Imported package names reserve the type spelling | Avoids the confusion of two unrelated types both named `Math` in the same file. |
| No hidden mutable package state | Prevents ambient state from undermining ownership and effect reasoning. |

# Zane Math

This document specifies Zane's math functions: the `@math$` intrinsics on the scalar primitives, which of them are exact and which approximate, and the `math` package's functions over them.

> **See also:** [`operators.md`](operators.md) §2.6 for the machine operations of `@operators$`. [`types.md`](types.md) §2.9 for conversions between scalar primitives. [`syntax.md`](syntax.md) §2.7 for the intrinsic namespaces.

---

## 1. Overview

Zane's arithmetic operators cover addition, multiplication, division, negation, and comparison. Everything else a program computes on numbers — square roots, bit operations, trigonometry — is a function of the `@math$` intrinsic namespace, and the `math` package declares a function over each one for `core`'s `Int` and `Float`.

- **`@math$ holds the math functions`.** Each takes operands of one scalar primitive and has one overload per scalar primitive it takes, like the machine operations of `@operators$`.
- **`Exact and approximate`.** An exact function's result is fully specified and the same on every platform. An approximate function's result is close to the true value and may differ between platforms.
- **`Every exact function is total`.** Each has a result for every operand, as every integer machine operation does.
- **`The math package`.** `core`'s project ships a second package, `math`, that declares the math functions over `Int` and `Float`. A program imports it beside `core`.
- **`Math functions are functions`.** `sqrt(x)`, not `x:sqrt()`: they compute a result from their inputs ([`functions.md`](functions.md) §3.6).

> **Story:** [`stories/math.md`](../stories/math.md#math-as-intrinsics-not-a-c-foreign-function-interface) — "Math as intrinsics, not a C foreign-function interface".

---

## 2. The `@math$` Intrinsics

### 2.1 Operands of one type, one overload per type

Every function of `@math$` takes operands of one scalar primitive, and its result is that primitive unless its table says otherwise. Each has one overload per scalar primitive it takes. In the tables below, `I` is `@primitives$I32` or `@primitives$I64`, and `F` is `@primitives$F32` or `@primitives$F64`. No function takes operands of two types; an operand is converted first ([`types.md`](types.md) §2.9).

A function of `@math$` is called like any function. It is not an operator, so it has no infix form ([`syntax.md`](syntax.md) §2.7).

### 2.2 Exact and approximate functions

Each function is **exact** or **approximate**.

An exact function's result is the one this document specifies for its operands. It is the same on every platform, and the same whether the function runs at compile time or at run time.

An approximate function's result is close to the exact mathematical value of the function at its operands, and need not be the representable value nearest to it. It may differ between platforms. Within one program it is a function of its operands: the same operands give the same result each time.

> **Story:** [`stories/math.md`](../stories/math.md#exact-and-approximate-functions-in-one-namespace-math) — "Exact and approximate functions in one namespace, `@math$`".

### 2.3 Bit operations

The bit operations take `@primitives$I32` and `@primitives$I64`, and treat a value as its bits in two's complement. All of them are exact.

| Function | Signature | Result |
|---|---|---|
| `bitAnd` | `I @math$bitAnd(left I, right I)` | Each bit set where it is set in both operands |
| `bitOr` | `I @math$bitOr(left I, right I)` | Each bit set where it is set in either operand |
| `bitXor` | `I @math$bitXor(left I, right I)` | Each bit set where it is set in exactly one operand |
| `bitNot` | `I @math$bitNot(value I)` | Every bit of the operand flipped |
| `shiftLeft` | `I @math$shiftLeft(value I, count I)` | The bits moved `count` places toward the most significant end, zeros filling in |
| `shiftRight` | `I @math$shiftRight(value I, count I)` | The bits moved `count` places toward the least significant end, copies of the sign bit filling in |
| `shiftRightLogical` | `I @math$shiftRightLogical(value I, count I)` | The bits moved `count` places toward the least significant end, zeros filling in |
| `rotateLeft` | `I @math$rotateLeft(value I, count I)` | The bits moved `count` places toward the most significant end, those moved out filling in at the other end |
| `rotateRight` | `I @math$rotateRight(value I, count I)` | The bits moved `count` places toward the least significant end, those moved out filling in at the other end |
| `popCount` | `I @math$popCount(value I)` | The number of set bits |
| `leadingZeros` | `I @math$leadingZeros(value I)` | The number of zero bits above the most significant set bit |
| `trailingZeros` | `I @math$trailingZeros(value I)` | The number of zero bits below the least significant set bit |

A shift or rotation count is taken modulo the type's width, 32 or 64, as a value from zero to one less than the width. So a count of 64 on `@primitives$I64` moves nothing, and a count of `-1` moves 63 places. `leadingZeros` and `trailingZeros` of zero give the width.

`shiftRight` keeps the sign of a negative value, so it divides by a power of two rounding toward negative infinity. `shiftRightLogical` treats the value as unsigned.

```zane
mask @primitives$I64 = @math$bitAnd(flags, @primitives$I64(255));
high @primitives$I64 = @math$shiftRightLogical(word, @primitives$I64(32));
```

> **Story:** [`stories/math.md`](../stories/math.md#exact-and-approximate-functions-in-one-namespace-math) — "Exact and approximate functions in one namespace, `@math$`".

### 2.4 Integer arithmetic

| Function | Signature | Result |
|---|---|---|
| `abs` | `I @math$abs(value I)` | The operand's magnitude |
| `min` | `I @math$min(left I, right I)` | The lesser operand |
| `max` | `I @math$max(left I, right I)` | The greater operand |

All three are exact. `abs` of the most negative value of the type gives that value back, wrapping as `@operators$negate` does ([`operators.md`](operators.md) §2.6).

### 2.5 Exact float functions

| Function | Signature | Result |
|---|---|---|
| `sqrt` | `F @math$sqrt(value F)` | The square root |
| `abs` | `F @math$abs(value F)` | The operand with its sign cleared |
| `floor` | `F @math$floor(value F)` | The greatest integer not above the operand |
| `ceil` | `F @math$ceil(value F)` | The least integer not below the operand |
| `trunc` | `F @math$trunc(value F)` | The operand rounded toward zero to an integer |
| `roundEven` | `F @math$roundEven(value F)` | The nearest integer, a tie going to the even one |
| `fma` | `F @math$fma(left F, right F, addend F)` | `left * right + addend`, rounded once |
| `copySign` | `F @math$copySign(magnitude F, sign F)` | `magnitude` with the sign of `sign` |
| `min` | `F @math$min(left F, right F)` | The lesser operand |
| `max` | `F @math$max(left F, right F)` | The greater operand |

Each is IEEE 754's operation of that name: `sqrt` is *squareRoot*, `floor`, `ceil`, `trunc`, and `roundEven` are *roundToIntegral* toward negative infinity, toward positive infinity, toward zero, and to nearest with ties to even, `fma` is *fusedMultiplyAdd*, `abs` and `copySign` are the sign-bit operations of the same names, and `min` and `max` are *minimum* and *maximum*. So:

- `sqrt` and `fma` are correctly rounded, to nearest with ties to even, like the arithmetic of [`operators.md`](operators.md) §2.6.
- `sqrt` of a value below zero is a NaN, and `sqrt` of a negative zero is that zero.
- The rounding functions keep the operand's sign, so `ceil` of `-0.5` is a negative zero, and give back an infinity or a NaN unchanged.
- `abs` and `copySign` act on the sign bit alone. A NaN's sign is not observable, so `copySign` takes a NaN `sign` operand as positive.
- `min` and `max` give a NaN when either operand is a NaN, and order a negative zero below a positive zero.

```zane
length @primitives$F64 = @math$sqrt(@math$fma(dx, dx, @operators$multiply(dy, dy)));
```

### 2.6 Approximate float functions

| Function | Signature | Result |
|---|---|---|
| `cbrt` | `F @math$cbrt(value F)` | The cube root |
| `sin`, `cos`, `tan` | `F @math$sin(value F)` | The sine, cosine, and tangent of an angle in radians |
| `asin`, `acos`, `atan` | `F @math$asin(value F)` | The arcsine, arccosine, and arctangent, in radians |
| `atan2` | `F @math$atan2(y F, x F)` | The angle in radians, from `-π` to `π`, of the point `(x, y)` |
| `exp` | `F @math$exp(value F)` | `e` raised to the operand |
| `log` | `F @math$log(value F)` | The natural logarithm |
| `pow` | `F @math$pow(base F, exponent F)` | `base` raised to `exponent` |
| `hypot` | `F @math$hypot(x F, y F)` | `sqrt(x * x + y * y)`, without overflow or underflow in between |

All are approximate (§2.2). For a NaN, an infinity, a zero, or an operand outside the function's domain, the result is the one IEEE 754 §9.2 gives for the function of that name, with `cbrt` taken as its `rootn` of `value` and `3`. So `cbrt` of a negative zero is that zero, `log` of a negative value and `asin` of `2` give a NaN, `log` of zero is negative infinity, and `pow` of any base to a zero exponent is `1`.

Compile-time evaluation ([`effects.md`](effects.md) §5.3) changes nothing a program computes, so an approximate function evaluated at compile time gives the result it gives where the program runs.

> **Story:** [`stories/math.md`](../stories/math.md#exact-and-approximate-functions-in-one-namespace-math) — "Exact and approximate functions in one namespace, `@math$`".

---

## 3. The `math` Package

### 3.1 A package beside `core`

The project that holds `core` holds a second public library package, `math` ([`packages.md`](packages.md) §2.1). It imports `core` and declares a function over each `@math$` function `core`'s fundamental types can use: the integer ones over `Int`, which holds an `@primitives$I64`, and the float ones over `Float`, which holds an `@primitives$F64`. Each has the name and the parameters of the intrinsic it calls, and passes its operands to the intrinsic through `core`'s implicit conversions to the storage primitives ([`types.md`](types.md) §2.6).

```zane
package math;

import core$;

Float sqrt(value Float) => Float(@math$sqrt(value))
Int shiftLeft(value Int, count Int) => Int(@math$shiftLeft(value, count))
```

A program that depends on `core`'s project reaches `math` as it reaches `core`, and imports it by name ([`dependencies.md`](dependencies.md) §8). Examples in this document are written as though `import math$` were present beside `import core$`.

> **Story:** [`stories/math.md`](../stories/math.md#the-math-functions-move-out-of-core-into-a-math-package) — "The math functions move out of `core` into a `math` package".

### 3.2 Functions, not methods

The math functions are functions rather than methods, because each computes its result from its inputs ([`functions.md`](functions.md) §3.6). A call is an ordinary function call:

```zane
import core$;
import math$;

distance Float = sqrt(dx * dx + dy * dy);
angle Float = atan2(y, x);
smaller Int = min(a, b);
```

A package may declare the same names over its own types. Imported together with `math`'s, they form one overload set ([`packages.md`](packages.md) §3.8), so `sqrt(v)` on a package's `Vec2` and `sqrt(x)` on a `Float` are written alike in one file.

> **Story:** [`stories/functions.md`](../stories/functions.md#computations-are-functions-and-math-functions-with-them) — "Computations are functions, and math functions with them".

### 3.3 The declarations

| Over | Functions |
|---|---|
| `Int` | `bitAnd`, `bitOr`, `bitXor`, `bitNot`, `shiftLeft`, `shiftRight`, `shiftRightLogical`, `rotateLeft`, `rotateRight`, `popCount`, `leadingZeros`, `trailingZeros`, `abs`, `min`, `max` |
| `Float` | `sqrt`, `abs`, `floor`, `ceil`, `trunc`, `roundEven`, `fma`, `copySign`, `min`, `max`, `cbrt`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `exp`, `log`, `pow`, `hypot` |

Each gives what its intrinsic gives, wrapped in the fundamental type, and is exact or approximate as its intrinsic is.

---

## 4. Summary

| Concept | Rule |
|---|---|
| `@math$` | The intrinsic namespace of the math functions; each takes operands of one scalar primitive, with one overload per primitive it takes |
| Exact function | Fully specified result, the same on every platform and at compile time as at run time |
| Approximate function | Result close to the exact value, possibly different between platforms, the same for the same operands within one program |
| Bit operations | On `I32` and `I64`, exact; shift and rotation counts taken modulo the width; `leadingZeros` and `trailingZeros` of zero give the width |
| Integer `abs` | Wraps on the most negative value, as `negate` does |
| Exact float functions | IEEE 754's `squareRoot`, `roundToIntegral`, `fusedMultiplyAdd`, sign-bit operations, `minimum` and `maximum` |
| Approximate float functions | `cbrt`, the trigonometric functions, `exp`, `log`, `pow`, `hypot`; special operands give what IEEE 754 §9.2 gives |
| The `math` package | A public library package beside `core` in its project, declaring functions over `Int` and `Float` named and shaped like their intrinsics |

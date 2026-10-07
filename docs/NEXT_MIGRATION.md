# Next migration

The logarithm, exponential, and power family is now complete.  The next major
block is trigonometric and hyperbolic functions.

## Required shared core

1. certified reduction by pi/2 using the independent pi cache;
2. exact quadrant and signed-zero handling;
3. a paired `sin_cos` kernel with one Ziv-style evaluator;
4. tangent, secant, cosecant, and cotangent from the paired kernel;
5. inverse trigonometric functions and two-argument atan2;
6. a paired `sinh_cosh` kernel built on the completed exponential family;
7. scaled-unit and pi-scaled interfaces.

The existing transcendental evaluator should remain the public correctness
shell.  Faster series, binary splitting, and table methods can be introduced
underneath it without changing the numerical contract.

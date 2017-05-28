# micro-lisp
Basically assembler coding practice.  First write the simplest possible LISP interpreter in C then translate that to assembly.

The language
------------

* ``(set! var expr)``
* ``(and expr expr) (or expr) (not expr)``
* ``(quote form)``
* ``(fn (var var ...) expr ...)``
* ``(begin expr ...)``
* ``(+ expr expr) (- expr expr) (* expr expr) (/ expr expr)``
* ``(if expr expr expr)``
* ``(let ((var expr) (var expr) ...) expr ...)``
* ``(cons expr expr)``
* ``(car expr expr)``
* ``(cdr expr expr)``
* ``(symbol? expr) (number? expr) (function? expr) (cons? expr) (null? expr)``





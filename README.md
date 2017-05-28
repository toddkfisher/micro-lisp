# micro-lisp
Basically assembler coding practice.  First write the simplest possible LISP interpreter in C then translate that to assembly.

The Language
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


The Implementation (C version)
------------------------------

TODO List
---------

* ☐ Cons cell management
	* ☐ Initialize free list
	* ☐ Get cons cell from free list
	* ☐ Garbage collection
	* ☐ Diagnostic/info
* ☐ Input/output
	* ☐ Reading
		* ☐ Symbol
		* ☐ Number
		* ☐ List
	* ☐ Writing
		* ☐ Symbol
		* ☐ Number
		* ☐ List
	* ☐ Interpreter
		* ☐ Interpreter switch/eval
			* ☐ Keywords
				* ☐ set!
				* ☐ and
				* ☐ or
				* ☐ not
				* ☐ quote
				* ☐ fn
				* ☐ begin
				* ☐ if
				* ☐ let
			* ☐ Call built-in function
				* ☐ Install built-in functions
			* ☐ Call user-defined function
		* ☐ Environment/frame management
			* ☐ New frame/extend env
			* ☐ Set var
			* ☐ Get var
			* ☐ Initialize globlal environment
			* ☐ Debugging/diagnostics.



ml: micro-lisp.o
	gcc -o ml micro-lisp.o

micro-lisp.o : micro-lisp.asm
	yasm -f elf64 -g dwarf2 micro-lisp.asm

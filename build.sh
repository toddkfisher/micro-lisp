#!/bin/sh
etags *.c *.h
awk -f gather-protos.awk micro-lisp.c > proto.h
clang -o ml micro-lisp.c

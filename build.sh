#!/bin/sh
etags *.c *.h
clang -o ml micro-lisp.c

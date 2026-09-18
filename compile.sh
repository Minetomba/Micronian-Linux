#!/bin/bash
musl-gcc -static src/main.c -o micronium-linux
strip micronium-linux
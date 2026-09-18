#!/bin/bash
musl-gcc -static src/main.c -o micronian-linux
strip micronian-linux
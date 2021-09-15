#!/bin/sh
g++ tetris.cpp -c -o tetris.o
g++ tetris.cpp -pthread -lSDL2 -o tetris
./tetris

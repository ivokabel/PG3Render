#include <stdlib.h>
#include <stdio.h>
#include "debugging.hxx"

bool gTrueValue = false;

void pg3_exit()
{
    //getchar(); // Wait for pressing the enter key on the command line

    if (gTrueValue)
        // Avoid "unreachable code" warning by making sure that compiler doesn't know 
        // at compile time whether exit() gets execuded
        exit(-1);
}

void init_debugging()
{
    gTrueValue = true;
}

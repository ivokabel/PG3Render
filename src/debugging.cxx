#include <stdlib.h>
#include <stdio.h>
#include "debugging.hxx"

bool Debugging::trueValue = false;

void Debugging::Exit()
{
    //getchar(); // Wait for pressing the enter key on the command line

    if (trueValue)  // Avoid "unreachable code" warning by making sure that compiler doesn't know 
                    // at compile time whether exit() gets execuded
        exit(-1);
}

void Debugging::Init()
{
    trueValue = true;
}

///////////////////////////////////////////////////////////////////////////////
// IOPT-GPIO - Dummy driver implementation: files inputs.txt / outputs.txt   //
// Author: Fernando J. G. Pereira (2015)                                     //
///////////////////////////////////////////////////////////////////////////////


#if !defined(ARDUINO)

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#ifndef _WIN32
#  include <unistd.h>
#endif


#define ANALOG_MAX 1024
#define MAX_OUT    256

static int outputs[MAX_OUT] = { 0 };
static int n_outputs = 1;


void pinMode( int pin, int mode )
{
}


int digitalRead( int pin )
{
    int i, res = 0;
    FILE *fptr = fopen( "inputs.txt", "r" );
    if( fptr == NULL ) return 0;
    for( i = 1; i <= pin; ++i ) 
       if( fscanf( fptr, "%d", &res ) < 1 ) return 0;
    fclose( fptr );
    return res;
}


void digitalWrite( int pin, int v )
{
    if( pin < 0 || pin > MAX_OUT ) return;
    outputs[pin] = v;
    if( pin > n_outputs ) n_outputs = pin;

    int i;
    FILE *fptr = fopen( "outputs.txt", "w" );
    if( fptr == NULL ) return;

    for( i = 1; i <= n_outputs; ++i ) fprintf( fptr, "O%d = %d\n", i, outputs[i] );
    fclose( fptr );
}


int analogRead( int pin )
{
    return digitalRead( pin );
}


void analogWrite( int pin, int v )
{
    digitalWrite( pin, v );
}


#endif

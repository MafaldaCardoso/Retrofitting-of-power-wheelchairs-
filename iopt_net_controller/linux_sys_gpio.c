///////////////////////////////////////////////////////////////////////////////
// IOPT-GPIO - Linux /sys/class/gpio Implementation.                         //
// Author: Fernando J. G. Pereira (2015)                                     //
///////////////////////////////////////////////////////////////////////////////


#if defined(linux) && !defined(ARDUINO)

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>


#define ANALOG_MAX 1023
#define N_GPIO     256
#define BASE_IO    0
//#define BASE_IO    54 


static char saved_values[N_GPIO] = { 0 };


void pinMode( int pin, int mode )
{
    char str[256];

    FILE* fptr = fopen( "/sys/class/gpio/export", "w" );
    if( fptr == NULL ) {
        perror( "export" );
	return;
    }
    fprintf( fptr, "%d\n", pin+BASE_IO );
    fclose( fptr );

    sprintf( str, "/sys/class/gpio/gpio%d/direction", pin+BASE_IO );
    fptr = fopen( str, "w" );
    if( fptr == NULL ) {
	perror( "direction" );
	return;
    }
    fprintf( fptr, "%s\n", mode != 0 ? "out" : "in" );
    fclose( fptr );

    if( pin >= 0 && pin < N_GPIO ) saved_values[pin] = -1;
}


int digitalRead( int pin )
{
    char res, str[256];
    sprintf( str, "/sys/class/gpio/gpio%d/value", pin+BASE_IO );
    int fd = open( str, O_RDONLY );
    if( fd < 0 ) return 0;
    if( read( fd, &res, 1 ) > 0 ) res -= '0';
    else res = 0;
    close( fd );
    if( pin >= 0 && pin < N_GPIO ) saved_values[pin] = res;
    return res;
}


void digitalWrite( int pin, int v )
{
    if( pin >= 0 && pin < N_GPIO ) {
        if( v == saved_values[pin] ) return;
	saved_values[pin] = v;
    }

    char str[256];
    char c = (v != 0) ? '1' : '0';

    sprintf( str, "/sys/class/gpio/gpio%d/value", pin+BASE_IO );
    int fd = open( str, O_WRONLY );
    if( fd < 0 ) return;
    write( fd, &c, 1 );
    close( fd );
}


int analogRead( int pin )
{
    return digitalRead( pin ) ? ANALOG_MAX : 0;
}


void analogWrite( int pin, int v )
{
    digitalWrite( pin, v >= ANALOG_MAX / 2 );
}


#endif

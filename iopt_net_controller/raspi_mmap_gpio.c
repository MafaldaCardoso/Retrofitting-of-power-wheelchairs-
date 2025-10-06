///////////////////////////////////////////////////////////////////////////////
// IOPT-GPIO - Raspberry PI 1/2 memory mapped I/O Implementation.            //
///////////////////////////////////////////////////////////////////////////////

#if defined(linux) && !defined(ARDUINO)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>


#define ANALOG_MAX 1023

#define BCM2708_PERI_BASE        0x20000000
#define BCM2709_PERI_BASE        0x3F000000
#define BCM2835_PERI_BASE        0xFE000000
#define GPIO_OFFSET                0x200000
 
 
// GPIO setup macros.
// Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))
 
#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

#define GPIO_GET(g) (*(gpio+13)&(1<<g))
   
#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock



volatile unsigned int *gpio = 0;
 
 
void setup_io()
{
    int fd, mem_fd;
    void *gpio_map;
    char buffer[256];
    unsigned int gpio_base = 0;

    /* detect raspi2 or raspi1 */
    fd = open( "/proc/cpuinfo", O_RDONLY );
    if( fd < 0 ) {
      printf("can't open /proc/cpuinfo\n");
      exit(-1);
    }
    gpio_base = BCM2708_PERI_BASE + GPIO_OFFSET;
    while( read( fd, buffer, 256 ) > 0 ) {
	buffer[255] = '\0';
	if( strstr( buffer, "BCM2709" ) ||
	    strstr( buffer, "Raspberry Pi 3" ) ) {
	    gpio_base = BCM2709_PERI_BASE + GPIO_OFFSET;
	    break;
	}
	if( strstr( buffer, "Raspberry Pi 4" ) ) {
	    gpio_base = BCM2835_PERI_BASE + GPIO_OFFSET;
	    break;
	}
    }
    close( fd );

    /* open /dev/mem */
    if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem \n");
      exit(-1);
    }

    /* mmap GPIO */
    gpio_map = mmap(
      NULL,             //Any adddress in our space will do
      4096,       //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      gpio_base         //Offset to GPIO peripheral
    );

    close(mem_fd); //No need to keep mem_fd open after mmap

    if (gpio_map == MAP_FAILED) {
      printf("mmap error %x\n", gpio_map);//errno also set!
      exit(-1);
    }

    // Always use volatile pointer!
    gpio = (volatile unsigned *)gpio_map;
}


void pinMode( int pin, int mode )
{
    if( gpio == 0 ) setup_io();

    if( mode ) {
	INP_GPIO(pin);
	OUT_GPIO(pin);
    }
}


int digitalRead( int pin )
{
    return (GPIO_GET( pin ) != 0);
}


void digitalWrite( int pin, int v )
{
    if( v != 0 ) GPIO_SET = (1 << pin);
    else         GPIO_CLR = (1 << pin);
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


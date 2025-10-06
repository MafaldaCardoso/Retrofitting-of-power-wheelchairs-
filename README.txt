For Arduino boards:

    - Copy all .c/.h/.cpp files whose name starts with net_* to your Arduino project folder
    - Rename the file "net_main.c" to "<PROJ>.ino" where <PROJ> is the Arduino project name
    - To enable the remote debugger, edit file net_types.h and uncomment the HTTP_SERVER definition
    - The default IP ADDR is 192.168.1.177 and the default password is "1234".
    - To define the Arduino Ethernet IP address, edit "net_server.h" and change the ARDUINO_IP_ADDR macro. Please note that the numbers must be seperated by commas (,) instead of dots (.).
    - If you wish the use the internal pull-up resistors of the input GPIO pins,
    change the net_io.c and replace "INPUT" with "INPUT_PULLUP" in the pin initialization code.

For embedded boards based on the Linux operating system:
    - A Makefile is supplied to simplify the project compilation
compilation (Just "cd" to the project directoty and run the "make" command)
    - Use the files http_server.c and http_server.h instead of net_server.h/net_server.cpp
    - To enable remote debugging, just uncomment the -DHTTP_SERVER line on the Makefile, or if the Makefile is not used, add this definition to net_types.h
    - There are several inplementations of the GPIO functions:
functionsa) For Linux based boards (including Raspberry Pi boards) you may use the linux_sys_gpio.c file, that uses the kernel /sys/class/gpio interface to access GPIO pins.
pinsb) In the case of Raspberry PI cards, you may use the GPIO funcions defined on raspi_mmap_gpio.c, but the memory-mapped base address may need adjustments depending on the Raspberry Pi model version.


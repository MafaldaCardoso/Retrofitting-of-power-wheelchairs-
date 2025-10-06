# 
# NOTES:
#
# - To enable remote debugging, uncomment -DHTTP_SERVER.
#
# - For improved I/O performance on Raspberry PI targets, 
#   replace linux_sys_gpio.o by raspi_mmap_io.o on the OBJS list.
#
# - Use dummy_gpio.o for simulated IO using two files: inputs.txt/outputs.txt
#

CC = gcc -O3 -Wall \
# -DHTTP_SERVER

OBJS = net_exec_step.o net_functions.o net_io.o net_main.o \
       net_dbginfo.o http_server.o \
       linux_sys_gpio.o 
#      raspi_mmap_gpio.o
#      dummy_gpio.o
#      net_server.o for Arduino

net_exec: $(OBJS) net_types.h
	$(CC)  $(OBJS) -o net_exec

net_exec_step.o: net_exec_step.c
	$(CC) -c net_exec_step.c
net_functions.o: net_functions.c
	$(CC) -c net_functions.c
net_io.o: net_io.c
	$(CC) -c net_io.c
net_main.o: net_main.c
	$(CC) -c net_main.c
net_dbginfo.o: net_dbginfo.c
	$(CC) -c net_dbginfo.c
http_server.o: http_server.c
	$(CC) -c http_server.c
net_server.o: net_server.cpp
	$(CC) -c net_server.c
linux_sys_gpio.o: linux_sys_gpio.c
	$(CC) -c linux_sys_gpio.c
raspi_mmap_io.o: raspi_mmap_io.c
	$(CC) -c raspi_mmap_io.c
dummy_gpio.o: dummy_gpio.c
	$(CC) -c dummy_gpio.c

clean:
	rm -f net_exec *.o

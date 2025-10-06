CC = gcc
CFLAGS = -O3 -Wall $(shell pkg-config --cflags gtk+-3.0)
LDFLAGS = $(shell pkg-config --libs gtk+-3.0) -lpigpio -lpthread -lm -lrt
# -DHTTP_SERVER

OBJS = net_exec_step.o net_functions.o net_io.o net_main.o \
       net_dbginfo.o http_server.o \
       raspi_mmap_gpio.o interface.o sensors.o
#      linux_sys_gpio.o 
#      dummy_gpio.o
#      net_server.o for Arduino

TARGET = wheelchair_app

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *.o

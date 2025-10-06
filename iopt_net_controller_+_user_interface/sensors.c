#include <pigpio.h>
#include <stdio.h>
#include <unistd.h>
#include "sensors.h"

#define CMSP14_ADDRESS 0x60
#define ULTRASONIC_TIMEOUT_US 30000 

// ========== IMU ========== //

static int handle = -1;

static int read_byte_signed(int reg) {
    int data = i2cReadByteData(handle, reg);
    return (data > 127) ? data - 256 : data;
}

int imu_init(void) {
    handle = i2cOpen(1, CMSP14_ADDRESS, 0);
    return (handle < 0) ? -1 : 0;
}

void imu_close(void) {
    if (handle >= 0) {
        i2cClose(handle);
        handle = -1;
    }
}

int imu_read_pitch_roll(int *pitch, int *roll) {
    if (handle < 0) return -1;
    *pitch = read_byte_signed(4);
    *roll  = read_byte_signed(5);
    return 0;
}


// ========== Ultrasonic ========== //

struct UltrasonicPins PINS = {
    27, 17,  // Front: TRIG, ECHO
    10, 22,  // Back:  TRIG, ECHO
    24, 23,  // Left:  TRIG, ECHO
    8,  25   // Right: TRIG, ECHO
};

void ultrasonic_init(void) {
    int* const trigs[] = {&PINS.TRIG_FRONT, &PINS.TRIG_BACK, &PINS.TRIG_LEFT, &PINS.TRIG_RIGHT};
    int* const echos[] = {&PINS.ECHO_FRONT, &PINS.ECHO_BACK, &PINS.ECHO_LEFT, &PINS.ECHO_RIGHT};

    for (int i = 0; i < 4; i++) {
        gpioSetMode(*trigs[i], PI_OUTPUT);
        gpioSetMode(*echos[i], PI_INPUT);
        gpioWrite(*trigs[i], 0);
    }

    gpioDelay(50000);
}

int get_distance(int TRIG, int ECHO) {
    gpioTrigger(TRIG, 10, 1);

    uint32_t timeoutTick = gpioTick();
    while (gpioRead(ECHO) == 0) {
        if ((gpioTick() - timeoutTick) > ULTRASONIC_TIMEOUT_US)
            return -1;
    }

    uint32_t startTick = gpioTick();
    while (gpioRead(ECHO) == 1) {
        if ((gpioTick() - startTick) > ULTRASONIC_TIMEOUT_US)
            return -1;
    }

    uint32_t duration = gpioTick() - startTick;
    return (int)((duration / 58.0f) + 0.5f);
}

struct UltrasonicData {
    int front;
    int back;
    int left;
    int right;
};

struct UltrasonicData ultrasonic_read_all(void) {
    struct UltrasonicData d;
    d.front = get_distance(PINS.TRIG_FRONT, PINS.ECHO_FRONT);
    gpioDelay(60000);
    d.back  = get_distance(PINS.TRIG_BACK,  PINS.ECHO_BACK);
    gpioDelay(60000);
    d.left  = get_distance(PINS.TRIG_LEFT,  PINS.ECHO_LEFT);
    gpioDelay(60000);
    d.right = get_distance(PINS.TRIG_RIGHT, PINS.ECHO_RIGHT);
    return d;
}

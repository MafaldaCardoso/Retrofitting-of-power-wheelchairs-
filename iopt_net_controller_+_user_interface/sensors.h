#ifndef SENSORS_H
#define SENSORS_H

#include <stdbool.h>

int imu_init(void);
int imu_read_pitch_roll(int* pitch, int* roll);
void imu_close(void);

struct UltrasonicPins {
    int TRIG_FRONT, ECHO_FRONT;
    int TRIG_BACK,  ECHO_BACK;
    int TRIG_LEFT,  ECHO_LEFT;
    int TRIG_RIGHT, ECHO_RIGHT;
};

extern struct UltrasonicPins PINS;
int get_distance(int TRIG, int ECHO);
void ultrasonic_init(void);
struct UltrasonicData ultrasonic_read_all(void);

#endif

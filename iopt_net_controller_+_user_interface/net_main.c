/* net_main.c - GTK in main thread, net loop in worker thread */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>

#include <pigpio.h>
#include "net_types.h"

#include "interface.h"
#include "sensors.h"


extern int roll_value;
int trace_control = TRACE_CONT_RUN;

extern void httpServer_init();
extern void httpServer_getRequest();
extern void httpServer_sendResponse();
extern void httpServer_disconnectClient();
extern void httpServer_checkBreakPoints();
extern void httpServer_finish();

static ACM_signals_NetMarking marking;
static ACM_signals_InputSignals inputs, prev_inputs;
static ACM_signals_PlaceOutputSignals place_out;
static ACM_signals_EventOutputSignals ev_out;


static volatile sig_atomic_t net_running = 1;


gboolean start_refresh_timer(gpointer data);


static void *net_thread_func(void *arg)
{
    (void)arg;

#ifdef HTTP_SERVER
    httpServer_init();
#endif

    do {
        if (!net_running) break;

#ifdef HTTP_SERVER
        httpServer_getRequest();
#endif

        if (trace_control != TRACE_PAUSE) {
            ACM_signals_ExecutionStep(&marking, &inputs, &prev_inputs, &place_out, &ev_out);
        } else {
            ACM_signals_GetInputSignals(&inputs, NULL);
        }

        if (trace_control > TRACE_PAUSE) --trace_control;

        int pitch, roll;
        if (imu_read_pitch_roll(&pitch, &roll) == 0) {
            roll_value = roll;
        }

#ifdef HTTP_SERVER
        httpServer_sendResponse();
#endif

        ACM_signals_LoopDelay();

#ifdef HTTP_SERVER
        httpServer_disconnectClient();
        httpServer_checkBreakPoints();
#endif

    } while (net_running && ACM_signals_FinishExecution(&marking) == 0);

#ifdef HTTP_SERVER
    httpServer_finish();
#endif

    return NULL;
}


gboolean start_refresh_timer(gpointer data) {
    (void)data;
    g_timeout_add(50, refresh_ui, NULL);

    return G_SOURCE_REMOVE;
}

#ifndef ARDUINO
int main()
{
    if (gpioInitialise() < 0) {
        fprintf(stderr, "Failed to initialize pigpio\n");
        return 1;
    }
    imu_init();
    ultrasonic_init();

    createInitial_ACM_signals_NetMarking(&marking);
    init_ACM_signals_OutputSignals(&place_out, &ev_out);
    ACM_signals_InitializeIO();

    ACM_signals_GetInputSignals(&prev_inputs, NULL);

    int argc = 0;
    char **argv = NULL;
    interface_init(&argc, &argv);
    interface_create();

    g_idle_add((GSourceFunc)start_refresh_timer, NULL);

    pthread_t net_thread;
    if (pthread_create(&net_thread, NULL, net_thread_func, NULL) != 0) {
        fprintf(stderr, "Failed to create net worker thread\n");
        gpioTerminate();
        return 1;
    }

    gtk_main();

    net_running = 0;
    pthread_join(net_thread, NULL);

    gpioTerminate();
    imu_close();

    return 0;
}
#endif


ACM_signals_NetMarking* get_ACM_signals_NetMarking()
{
    return &marking;
}

ACM_signals_InputSignals* get_ACM_signals_InputSignals()
{
    return &inputs;
}

ACM_signals_PlaceOutputSignals* get_ACM_signals_PlaceOutputSignals()
{
    return &place_out;
}

ACM_signals_EventOutputSignals* get_ACM_signals_EventOutputSignals()
{
    return &ev_out;
}

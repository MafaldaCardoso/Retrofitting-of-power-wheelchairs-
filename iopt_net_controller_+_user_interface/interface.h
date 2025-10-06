#ifndef INTERFACE_H
#define INTERFACE_H

#include <gtk/gtk.h>
#include "net_types.h"


void interface_init(int *argc, char ***argv);
void interface_create(void);
void *interface_run(void *arg);

gboolean refresh_ui(gpointer data);


extern GtkWidget *speed_canvas;
extern GtkWidget *tpi_inChg_indicator;
extern GtkWidget *assist_mode_indicator;

extern GtkWidget *us_front_indicator;
extern GtkWidget *us_left_bar;
extern GtkWidget *us_back_bar;
extern GtkWidget *us_right_bar;

extern GtkWidget *pitch_canvas;
extern GtkWidget *roll_canvas;


typedef enum {
    GAUGE_TOP,
    GAUGE_RIGHT
} GaugeOrientation;

struct gauge_data {
    int value;
    GaugeOrientation orientation;
};
extern struct gauge_data *pitch_gauge;
extern struct gauge_data *roll_gauge;

extern int roll_value;


void update_pwm_speed(void);

#endif // INTERFACE_H

#include <gtk/gtk.h>
#include <cairo.h>
#include <math.h>
#include <pigpio.h>

#include "interface.h"
#include "net_types.h"


static ACM_signals_InputSignals *inputs;
static ACM_signals_PlaceOutputSignals *outputs;

#define NUM_SPEED_LEVELS 4
const int speed_levels[NUM_SPEED_LEVELS] = {25, 50, 75, 100};
#define PWM_GPIO 18
static int last_speedDial = -1;

GtkWidget *speed_canvas = NULL;
gboolean tpi_inChg_state = FALSE;
GtkWidget *tpi_inChg_indicator = NULL;
gboolean assist_mode_state = FALSE;
GtkWidget *assist_mode_indicator = NULL;

GtkWidget *us_front_indicator = NULL;
gboolean us_front_state = 0;
GtkWidget *us_left_bar = NULL;
int us_left_state = 3;
GtkWidget *us_back_bar = NULL;
int us_back_state = 3;
GtkWidget *us_right_bar = NULL;
int us_right_state = 3;

GtkWidget *pitch_canvas = NULL;
GtkWidget *roll_canvas = NULL;
int roll_value  = 0;

struct gauge_data *pitch_gauge;
struct gauge_data *roll_gauge;


void update_pwm_speed() {
    if (outputs->tpi_inChg == 0) {
        gpioPWM(PWM_GPIO, 0);
        last_speedDial = -1;
        return;
    }

    if (outputs->speedDial != last_speedDial) {
        last_speedDial = outputs->speedDial;

        int speed_index = outputs->speedDial;
        if (speed_index >= 0 && speed_index < NUM_SPEED_LEVELS) {
            int duty = speed_levels[speed_index];
            gpioPWM(PWM_GPIO, (duty * 255) / 100);
        } else {
            gpioPWM(PWM_GPIO, 0);
        }
    }

    gtk_widget_queue_draw(speed_canvas);
}


// ================= Drawing Functions ================= //

gboolean draw_speed_bar(GtkWidget *widget, cairo_t *cr, gpointer data) {
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    int height = allocation.height;
    int width = allocation.width;
    int segment_height = height / NUM_SPEED_LEVELS;
    int current_speed_index = outputs->speedDial;

    for (int i = 0; i < NUM_SPEED_LEVELS; ++i) {
        int y0 = height - (i + 1) * segment_height;
        if (i <= current_speed_index) {
            cairo_set_source_rgb(cr, 0.12, 0.56, 1.0);
        } else {
            cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }

        cairo_rectangle(cr, 0, y0, width, segment_height);
        cairo_fill(cr);

        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_select_font_face(cr, "Arial", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, segment_height / 2);
        gchar *label = g_strdup_printf("%d%%", speed_levels[i]);
        cairo_move_to(cr, 5, y0 + segment_height * 0.7);
        cairo_show_text(cr, label);
        g_free(label);
    }

    return FALSE;
}


gboolean draw_circle_indicator(GtkWidget *widget, cairo_t *cr, gpointer state_ptr) {
    gboolean *state = (gboolean *)state_ptr;

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    int size = MIN(allocation.width, allocation.height);
    int radius = size / 2 - 4;

    cairo_translate(cr, allocation.width / 2, allocation.height / 2);

    if (*state) {
        cairo_set_source_rgb(cr, 0.0, 0.8, 0.0);
    } else {
        cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
    }

    cairo_arc(cr, 0, 0, radius, 0, 2 * M_PI);
    cairo_fill(cr);

    return FALSE;
}


gboolean draw_obstacle_bars(GtkWidget *widget, cairo_t *cr, gpointer state_ptr) {
    int *state = (int *)state_ptr;
    int level = *state;

    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);

    int total_bars = 4;
    int bar_height = alloc.height / total_bars - 3;
    int bar_width = alloc.width - 6;
    int spacing = 3;

    int active_bars = 0;
    double r = 0, g = 0, b = 0;

    switch (level) {
        case 0: active_bars = 4; r = 0.0; g = 1.0; b = 0.0; break; // green
        case 1: active_bars = 3; r = 1.0; g = 1.0; b = 0.0; break; // yellow
        case 2: active_bars = 2; r = 1.0; g = 0.5; b = 0.0; break; // orange
        case 3: active_bars = 1; r = 1.0; g = 0.0; b = 0.0; break; // red
        default: active_bars = 0; r = 0.5; g = 0.5; b = 0.5; break; // gray
    }

    for (int i = 0; i < total_bars; i++) {
        int x = 3; 
        int y = i * (bar_height + spacing);

        if (i >= total_bars - active_bars) {
            cairo_set_source_rgb(cr, r, g, b);
        } else {
            cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }

        cairo_rectangle(cr, x, y, bar_width, bar_height);
        cairo_fill(cr);
    }

    return FALSE;
}


gboolean draw_semi_circle(GtkWidget *widget, cairo_t *cr, gpointer data) {
    struct gauge_data *g = (struct gauge_data *)data;

    int angle_deg = g->value;
    if (angle_deg < -20) angle_deg = -20;
    if (angle_deg > 20) angle_deg = 20;

    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);
    int cx = alloc.width / 2;
    int cy = alloc.height / 2;
    int radius = MIN(cx, cy) - 5;

    int start_angle = 0, end_angle = 0;
    int display_angle_rad = 0;
    int pivot_x = cx, pivot_y = cy;

    if (g->orientation == GAUGE_TOP) {
        pivot_y = alloc.height - 5;
        start_angle = M_PI;
        end_angle = 2 * M_PI;
        display_angle_rad = M_PI - (angle_deg + 20) / 40 * M_PI;
    } else if (g->orientation == GAUGE_RIGHT) {
        pivot_x = 5;
        pivot_y = alloc.height / 2;
        start_angle = -M_PI/2;
        end_angle = M_PI/2;
        display_angle_rad = -M_PI/2 + (angle_deg + 20) / 40 * M_PI;
    }

    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_set_line_width(cr, 4);
    cairo_arc(cr, pivot_x, pivot_y, radius, start_angle, end_angle);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
    cairo_set_line_width(cr, 3);
    cairo_move_to(cr, pivot_x, pivot_y);
    cairo_line_to(cr, pivot_x + radius * cos(display_angle_rad), pivot_y - radius * sin(display_angle_rad));
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_arc(cr, pivot_x, pivot_y, 4, 0, 2 * G_PI);
    cairo_fill(cr);

    char buf[16];
    snprintf(buf, sizeof(buf), "%d°", angle_deg);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, buf, &extents);
    cairo_set_source_rgb(cr, 0, 0, 0);

    if (g->orientation == GAUGE_TOP) {
        cairo_move_to(cr, pivot_x - extents.width / 2, pivot_y - radius - 5);
    } else if (g->orientation == GAUGE_RIGHT) {
        cairo_move_to(cr, pivot_x + radius + 5, pivot_y + extents.height / 2);
    } else {
        cairo_move_to(cr, pivot_x - extents.width / 2, pivot_y - radius / 2 + extents.height / 2);
    }

    cairo_show_text(cr, buf);

    return FALSE;
}


// ================= Button Callbacks ================= //

void on_button_pressed(GtkWidget *widget, gpointer user_data) {
    const gchar *label = gtk_button_get_label(GTK_BUTTON(widget));

    if (g_strcmp0(label, "↑") == 0)         inputs->btnF = 1;
    else if (g_strcmp0(label, "↓") == 0)    inputs->btnB = 1;
    else if (g_strcmp0(label, "←") == 0)    inputs->btnL = 1;
    else if (g_strcmp0(label, "→") == 0)    inputs->btnR = 1;
    else if (g_strcmp0(label, "↖") == 0)    inputs->btnF_L = 1;
    else if (g_strcmp0(label, "↗") == 0)    inputs->btnF_R = 1;
    else if (g_strcmp0(label, "↙") == 0)    inputs->btnB_L = 1;
    else if (g_strcmp0(label, "↘") == 0)    inputs->btnB_R = 1;
    else if (g_strcmp0(label, "BEEP") == 0) inputs->btnHorn = 1;
    else if (g_strcmp0(label, "Assist") == 0) inputs->btnAssist_mode = 1;
    else if (g_strcmp0(label, "Speed +") == 0) inputs->btnInc = 1;
    else if (g_strcmp0(label, "Speed -") == 0) inputs->btnDec = 1;
}

void on_button_released(GtkWidget *widget, gpointer user_data) {
    const gchar *label = gtk_button_get_label(GTK_BUTTON(widget));

    if (g_strcmp0(label, "↑") == 0)         inputs->btnF = 0;
    else if (g_strcmp0(label, "↓") == 0)    inputs->btnB = 0;
    else if (g_strcmp0(label, "←") == 0)    inputs->btnL = 0;
    else if (g_strcmp0(label, "→") == 0)    inputs->btnR = 0;
    else if (g_strcmp0(label, "↖") == 0)    inputs->btnF_L = 0;
    else if (g_strcmp0(label, "↗") == 0)    inputs->btnF_R = 0;
    else if (g_strcmp0(label, "↙") == 0)    inputs->btnB_L = 0;
    else if (g_strcmp0(label, "↘") == 0)    inputs->btnB_R = 0;
    else if (g_strcmp0(label, "BEEP") == 0) inputs->btnHorn = 0;
    else if (g_strcmp0(label, "Assist") == 0) inputs->btnAssist_mode = 0;
    else if (g_strcmp0(label, "Speed +") == 0) inputs->btnInc = 0;
    else if (g_strcmp0(label, "Speed -") == 0) inputs->btnDec = 0;
}


// ================= Widget Helper Functions ================= //

GtkWidget* create_button(const gchar *label, GCallback callback) {
    GtkWidget *btn = gtk_button_new_with_label(label);
    gtk_widget_set_size_request(btn, 110, 110);
    gtk_widget_set_margin_top(btn, 3);
    gtk_widget_set_margin_bottom(btn, 3);
    gtk_widget_set_margin_start(btn, 3);
    gtk_widget_set_margin_end(btn, 3);

    if (callback) {
        g_signal_connect(btn, "pressed", callback, NULL);
        g_signal_connect(btn, "released", G_CALLBACK(on_button_released), NULL);
    }
    return btn;
}


GtkWidget* create_obstacle_sensor(const char *label, int *state) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *area = gtk_drawing_area_new();
    gtk_widget_set_size_request(area, 100, 120);
    g_signal_connect(area, "draw", G_CALLBACK(draw_obstacle_bars), state);
    gtk_box_pack_start(GTK_BOX(box), area, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), gtk_label_new(label), FALSE, FALSE, 0);
    return box;
}


// ================= Interface Creation Functions ================= //

GtkWidget* create_main_interface() {
    GtkWidget *main_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(main_grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(main_grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(main_grid), 10);

    GtkWidget *dir_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(dir_grid), 7);
    gtk_grid_set_column_spacing(GTK_GRID(dir_grid), 7);

    const char *labels[3][3] = {
    {"↖", "↑", "↗"},
    {"←", "BEEP", "→"},
    {"↙", "↓", "↘"}
    };

    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            gtk_grid_attach(GTK_GRID(dir_grid),
                create_button(labels[r][c], G_CALLBACK(on_button_pressed)), c, r, 1, 1);
        }
    }

    gtk_grid_attach(GTK_GRID(main_grid), dir_grid, 0, 0, 1, 1);

    GtkWidget *right_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(right_grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(right_grid), 10);

    tpi_inChg_indicator = gtk_drawing_area_new();
    gtk_widget_set_halign(tpi_inChg_indicator, GTK_ALIGN_END);
    gtk_widget_set_valign(tpi_inChg_indicator, GTK_ALIGN_START);
    gtk_widget_set_size_request(tpi_inChg_indicator, 40, 40);
    tpi_inChg_state = (gboolean) (outputs->tpi_inChg);
    g_signal_connect(tpi_inChg_indicator, "draw", G_CALLBACK(draw_circle_indicator), &tpi_inChg_state);
    gtk_grid_attach(GTK_GRID(right_grid), tpi_inChg_indicator, 2, 0, 1, 1);

    GtkWidget *btn_inc = create_button("Speed +", G_CALLBACK(on_button_pressed));
    GtkWidget *btn_dec = create_button("Speed -", G_CALLBACK(on_button_pressed));
    GtkWidget *btn_assist = create_button("Assist", G_CALLBACK(on_button_pressed));

    assist_mode_indicator = gtk_drawing_area_new();
    gtk_widget_set_halign(assist_mode_indicator, GTK_ALIGN_END);
    gtk_widget_set_valign(assist_mode_indicator, GTK_ALIGN_START);
    gtk_widget_set_size_request(assist_mode_indicator, 40, 40);
    assist_mode_state = (gboolean) (outputs->Assist_mode);
    g_signal_connect(assist_mode_indicator, "draw", G_CALLBACK(draw_circle_indicator), &assist_mode_state);
    gtk_grid_attach(GTK_GRID(right_grid), assist_mode_indicator, 2, 2, 1, 1);

    speed_canvas = gtk_drawing_area_new();
    gtk_widget_set_size_request(speed_canvas, 80, 200);
    g_signal_connect(speed_canvas, "draw", G_CALLBACK(draw_speed_bar), NULL);

    gtk_grid_attach(GTK_GRID(right_grid), btn_inc,      0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(right_grid), speed_canvas, 1, 0, 1, 2);
    gtk_grid_attach(GTK_GRID(right_grid), btn_dec,      0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(right_grid), btn_assist,   1, 2, 1, 1);

    gtk_grid_attach(GTK_GRID(main_grid), right_grid, 1, 0, 1, 1);

    gtk_grid_set_column_spacing(GTK_GRID(main_grid), 100);

    return main_grid;}

GtkWidget* create_data_view_tab() {
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 30);
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 20);

    // Step Sensor
    GtkWidget *step_frame = gtk_frame_new("Step Sensor");
    gtk_widget_set_halign(step_frame, GTK_ALIGN_START);
    gtk_widget_set_valign(step_frame, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), step_frame, FALSE, FALSE, 0);

    GtkWidget *step_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_add(GTK_CONTAINER(step_frame), step_box);

    us_front_indicator = gtk_drawing_area_new();
    gtk_widget_set_size_request(us_front_indicator, 100, 100);
    us_front_state = (gboolean) (outputs->front_alert);
    g_signal_connect(us_front_indicator, "draw", G_CALLBACK(draw_circle_indicator), &us_front_state);
    gtk_box_pack_start(GTK_BOX(step_box), us_front_indicator, FALSE, FALSE, 0);

    // Obstacle Sensors
    GtkWidget *obstacle_frame = gtk_frame_new("Obstacle Sensors");
    gtk_widget_set_halign(obstacle_frame, GTK_ALIGN_START);
    gtk_widget_set_valign(obstacle_frame, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), obstacle_frame, FALSE, FALSE, 0);

    GtkWidget *obstacle_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(obstacle_grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(obstacle_grid), 20);
    gtk_container_add(GTK_CONTAINER(obstacle_frame), obstacle_grid);

    us_left_state = (int) (outputs->left_alert);
    us_back_state = (int) (outputs->back_alert);
    us_right_state = (int) (outputs->right_alert);

    us_left_bar = create_obstacle_sensor("Left", &us_left_state);
    us_back_bar = create_obstacle_sensor("Back", &us_back_state);
    us_right_bar = create_obstacle_sensor("Right", &us_right_state);

    gtk_grid_attach(GTK_GRID(obstacle_grid), us_left_bar,  0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(obstacle_grid), us_back_bar,  1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(obstacle_grid), us_right_bar, 2, 0, 1, 1);

    // Tilt Indicators
    GtkWidget *tilt_frame = gtk_frame_new("Tilt");
    gtk_widget_set_halign(tilt_frame, GTK_ALIGN_START);
    gtk_widget_set_valign(tilt_frame, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), tilt_frame, FALSE, FALSE, 0);

    GtkWidget *tilt_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_container_add(GTK_CONTAINER(tilt_frame), tilt_box);

    pitch_gauge = g_new0(struct gauge_data, 1);
    pitch_gauge->value = inputs->pitch;
    pitch_gauge->orientation = GAUGE_RIGHT;

    GtkWidget *pitch_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    pitch_canvas = gtk_drawing_area_new();
    gtk_widget_set_size_request(pitch_canvas, 120, 120);
    g_signal_connect(pitch_canvas, "draw", G_CALLBACK(draw_semi_circle), pitch_gauge);
    gtk_widget_set_margin_start(pitch_canvas, 15);
    gtk_widget_set_halign(pitch_canvas, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(pitch_box), pitch_canvas, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pitch_box), gtk_label_new("Pitch"), FALSE, FALSE, 0);

    roll_gauge = g_new0(struct gauge_data, 1);
    roll_gauge->value = roll_value;
    roll_gauge->orientation = GAUGE_TOP;

    GtkWidget *roll_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    roll_canvas = gtk_drawing_area_new();
    gtk_widget_set_size_request(roll_canvas, 120, 120);
    g_signal_connect(roll_canvas, "draw", G_CALLBACK(draw_semi_circle), roll_gauge);
    gtk_box_pack_start(GTK_BOX(roll_box), roll_canvas, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(roll_box), gtk_label_new("Roll"), FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(tilt_box), pitch_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tilt_box), roll_box, FALSE, FALSE, 0);

    return main_box;
}



gboolean refresh_ui(gpointer data) {
    update_pwm_speed();

    if (tpi_inChg_indicator != NULL && GTK_IS_WIDGET(tpi_inChg_indicator)) {
        tpi_inChg_state = (gboolean)(outputs->tpi_inChg);
        gtk_widget_queue_draw(tpi_inChg_indicator);
    }

    if (assist_mode_indicator != NULL && GTK_IS_WIDGET(assist_mode_indicator)) {
        assist_mode_state = (gboolean)(outputs->Assist_mode);
        gtk_widget_queue_draw(assist_mode_indicator);
    }

    if (us_front_indicator != NULL && GTK_IS_WIDGET(us_front_indicator)) {
        us_front_state = (gboolean)(outputs->front_alert);
        gtk_widget_queue_draw(us_front_indicator);
    }

    if (us_left_bar != NULL && GTK_IS_WIDGET(us_left_bar)) {
        us_left_state = (int)(outputs->left_alert);
        gtk_widget_queue_draw(us_left_bar);
    }

    if (us_back_bar != NULL && GTK_IS_WIDGET(us_back_bar)) {
        us_back_state = (int)(outputs->back_alert);
        gtk_widget_queue_draw(us_back_bar);
    }

    if (us_right_bar != NULL && GTK_IS_WIDGET(us_right_bar)) {
        us_right_state = (int)(outputs->right_alert);
        gtk_widget_queue_draw(us_right_bar);
    }

    if (pitch_gauge) {
        pitch_gauge->value = inputs->pitch;
    }
    if (roll_gauge) {
        roll_gauge->value = roll_value;
    }

    if (pitch_canvas != NULL && GTK_IS_WIDGET(pitch_canvas)) {
        gtk_widget_queue_draw(pitch_canvas);
    }

    if (roll_canvas != NULL && GTK_IS_WIDGET(roll_canvas)) {
        gtk_widget_queue_draw(roll_canvas);
    }

    return G_SOURCE_CONTINUE;
}


void interface_init(int *argc, char ***argv) {
    gtk_init(argc, argv);

    inputs = get_ACM_signals_InputSignals();
    outputs = get_ACM_signals_PlaceOutputSignals();
}

void interface_create() {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Touch Controller");
    gtk_window_maximize(GTK_WINDOW(window));
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider,
        "button {"
        "  font-size: 24px;"
        "  font-weight: bold;"
        "  background-color: #C0D0FF;"
        "  border: 1px solid #888;"
        "  color: black;"
        "}"
        "button:active { background-color: #7090FF; }",
        -1, NULL);

    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);
    gtk_style_context_add_provider_for_screen(
        screen, GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(css_provider);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
        create_main_interface(), gtk_label_new("Control"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
        create_data_view_tab(), gtk_label_new("Sensors"));

    gtk_container_add(GTK_CONTAINER(window), notebook);

    gtk_widget_show_all(window);
}

void *interface_run(void *arg) {
    gtk_main();
    return NULL;
}
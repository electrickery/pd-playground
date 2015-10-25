/* short addendum to f_all_guis.h for the toggle derived led object */

typedef struct _led
{
    t_iemgui x_gui;
    t_float    x_on;
    t_float    x_nonzero;
} t_led;

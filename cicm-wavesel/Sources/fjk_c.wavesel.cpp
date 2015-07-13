
#include "../c.library.hpp"

#define BACKGROUNDLAYER "wavesel_background_layer"
#define WAVEFORMLAYER   "wavesel_waveform_layer"
#define SELECTEDLAYER   "wavesel_selected_layer"


typedef struct  _wavesel
{
    t_ebox      j_box;
    t_rect      f_box;
    t_rect      bg_rect;
    t_rect      fg_rect;
    t_rect      sel_rect;
    t_elayer*   bg_layer;
    t_elayer*   fg_layer;
    t_elayer*   sel_layer;
    t_symbol*   bg_layer_name;
    t_symbol*   fg_layer_name;
    t_symbol*   sel_layer_name;
    int         sel_start_cursor;
    int         sel_end_cursor;

    t_garray*   x_array;
    t_symbol*   x_arrayname;
    int         x_arraysize;
    t_float     x_samplesPerColumn;
    t_float     x_ksr;
    t_float     x_outputFactor;
    t_float     x_currentElementTop;
    t_float     x_currentElementBottom;
    int         x_clipmode;
    int         x_selection;
    
    long        f_width;
    t_symbol*   f_color;
    char        f_fill;
    
    t_outlet*   f_out_1;
    t_outlet*   f_out_2;
    t_outlet*   f_out_3;
    t_outlet*   f_out_4;
    
    t_rgba		f_color_background;
    t_rgba		f_color_border;
    t_rgba      f_color_waveform;
    t_rgba      f_color_selection;
    char**      f_instructions;
    int         f_ninstructions;
    static const int maxcmd = 1000;
} t_wavesel;

t_eclass *wavesel_class;

static void wavesel_setarrayprops(t_wavesel *x)
{
    x->x_samplesPerColumn = (t_float)x->x_arraysize / x->bg_rect.width;
    x->x_outputFactor = x->x_samplesPerColumn / x->x_ksr;
}

// find the extreme values in the array for each displayed waveform column
static void wavesel_get_column_size(t_wavesel* x, int column, t_word *vec)
{
    int sampleOffset   = (int)(column * x->x_samplesPerColumn);
    int sampleEndPoint = sampleOffset + (int)x->x_samplesPerColumn;
    int i;
    t_float maxSize = 0.5f;
    t_float upper = 0.f, lower = 0.f;
    for (i = sampleOffset; i < sampleEndPoint; i++) 
    {
        upper = (upper < (vec[i].w_float)) ? vec[i].w_float : upper;
        lower = (lower > (vec[i].w_float)) ? vec[i].w_float : lower;
    }
    if (x->x_clipmode) 
    {
         x->x_currentElementTop = (upper >  maxSize) ?  maxSize : upper;
         x->x_currentElementBottom = 
             (lower < -maxSize) ? -maxSize : lower;
    }
    else
    {
        x->x_currentElementTop    = upper;
        x->x_currentElementBottom = lower;
    }
}

static void wavesel_draw_background(t_wavesel *x, t_rect *rect)
{
    int err;
    t_elayer *g = ebox_start_layer((t_ebox *)x, x->bg_layer_name, 
        rect->width, rect->height);
    if (g)
    {
        err = ebox_end_layer((t_ebox*)x, x->fg_layer_name);
        if (err)
            post("wavesel_draw_background: problem ending %s", 
                x->bg_layer_name->s_name);
        err = ebox_paint_layer((t_ebox *)x, x->bg_layer_name, 0.f, 0.f);
/*        if (err) // most exits return -1
            post("c.wavesel_draw_background: problem painting %s", 
                x->bg_layer_name->s_name); */
        x->bg_layer = g;
    }
}

// draws the waveform. Initially all zeroes, later derived from an array
static void wavesel_draw_waveform(t_wavesel *x, t_rect *rect)
{
    t_garray *ax = x->x_array;
    t_word *vec;
    int size;
    int err;
    if (ax) // check if an array is present, because initially it isn't
    {
        if (!garray_getfloatwords(ax, &size, &vec))
        {
            post("c.wavesel_draw_waveform: couldn't read from array!");
            return;
        }
    }
    int i;
    int middle = (int)rect->height / 2;
    t_float top = 1.f;
    t_float bottom = 0.f;
    int columns = (int)rect->width;
    
    t_elayer *g = ebox_start_layer((t_ebox *)x, x->fg_layer_name, 
        rect->width, rect->height);
    if (g) 
    {
        egraphics_set_color_rgba(g, &x->f_color_waveform);
        for(i = 0; i < columns; i++)
        {
            if (ax)
            {
                wavesel_get_column_size(x, i, vec);
                top    = x->x_currentElementTop;
                bottom = x->x_currentElementBottom;

                egraphics_line_fast(g, 
                    i, middle + floor(0.5f - top * rect->height), 
                    i, middle + floor(0.5f - bottom * rect->height));
            }
            else
                egraphics_line_fast(g, i, middle - (int)top, i, 
                    middle + (int)bottom);
        }
        err = ebox_end_layer((t_ebox*)x, x->fg_layer_name);
        if (err)
            post("wavesel_draw_waveform: problem ending %s", 
                x->fg_layer_name->s_name);
    
        err = ebox_paint_layer((t_ebox *)x, x->fg_layer_name, 0.f, 0.f);
        if (err)
            post("wavesel_draw_waveform: problem painting %s", 
                x->fg_layer_name->s_name);

        x->fg_layer = g;
    }
}

/* 
static void wavesel_draw_selection(t_wavesel *x, t_rect *rect)
{
    int err;
    t_elayer *g = ebox_start_layer((t_ebox *)x, x->sel_layer_name, 
        rect->width, rect->height);
    if (g)
    {
        egraphics_set_color_rgba(g, &x->f_color_selection);
        egraphics_rectangle(g, x->sel_start_cursor, 0, 
            x->sel_end_cursor, rect->height);
        x->sel_layer = g;
    }
    else
    {
        post("c.wavesel_draw_selection: problem starting %s", 
            x->sel_layer_name->s_name);
    }
    err = ebox_end_layer((t_ebox*)x, x->sel_layer_name);
    if (err)
        post("wavesel_draw_selection: problem ending %s", 
            x->sel_layer_name->s_name);
    
    err = ebox_paint_layer((t_ebox *)x, x->sel_layer_name, 0.f, 0.f);
    if (err)
        post("wavesel_draw_selection: problem painting %s", 
            x->sel_layer_name->s_name);
    x->x_selection = 1;
} */

// (re)run when an array is set /  changed
static void wavesel_redraw_waveform(t_wavesel *x)
{
    int err;
    if (!x->x_array)
    {
        post("redraw_waveform: no array defined!");
        return;
    }
    wavesel_setarrayprops(x);
    
    err = ebox_invalidate_layer((t_ebox *)x, x->fg_layer_name);
    if (err)
        post("wavesel_redraw: problem invalidating %s", 
            x->fg_layer_name->s_name);

    t_rect fg_rect;
    ebox_get_rect_for_view((t_ebox *)x, &fg_rect);
    wavesel_draw_waveform(x, &fg_rect);
    x->fg_rect = fg_rect;
    ebox_redraw((t_ebox *)x);
}

// called by the framework to restore the ebox
static void wavesel_paint(t_wavesel *x, t_object *view)
{
    post("waveform layer state: %d", (x->fg_layer) ? x->fg_layer->e_state : 9999);
    t_rect bg_rect;
    ebox_get_rect_for_view((t_ebox *)x, &bg_rect);
    wavesel_draw_background(x, &bg_rect);
    x->bg_rect = bg_rect;
    
    t_rect fg_rect;
    ebox_get_rect_for_view((t_ebox *)x, &fg_rect);
    wavesel_draw_waveform(x, &fg_rect);
    x->fg_rect = fg_rect;

/*    if (x->x_selection)
    {
        t_rect sel_rect;
        ebox_get_rect_for_view((t_ebox *)x, &sel_rect);
        wavesel_draw_selection(x, &sel_rect);
        x->sel_rect = sel_rect; 
    }  */
    
    post("wavesel_paint");
    if (view) {} // suppress compiler warning
}

// mouse position x values to be integrated in selection logic
static void wavesel_mousedown(t_wavesel *x, t_object *patcherview, 
    t_pt pt, long modifiers)
{
    outlet_float(x->f_out_1, pt.x);
    if (patcherview && modifiers) {} // suppress compiler warnings
}
static void wavesel_mouseup(t_wavesel *x, t_object *patcherview, 
    t_pt pt, long modifiers)
{
    outlet_float(x->f_out_2, pt.x);
    if (patcherview && modifiers) {} // suppress compiler warning
}
static void wavesel_mousedrag(t_wavesel *x, t_object *patcherview, 
    t_pt pt, long modifiers)
{
    outlet_float(x->f_out_3, pt.x);
    if (patcherview && modifiers) {} // suppress compiler warning
}
static void wavesel_mousemove(t_wavesel *x, t_object *patcherview, 
    t_pt pt, long modifiers)
{
    outlet_float(x->f_out_4, pt.x);
    if (patcherview && modifiers) {} // suppress compiler warning
}

// set the array
static void wavesel_setarray(t_wavesel *x, t_symbol *s)
{
    t_garray *array;
    x->x_arrayname = s;
    
    if ((array = (t_garray *)pd_findbyclass(x->x_arrayname, 
        garray_class)))
    {
        x->x_array = array;
        x->x_arraysize = garray_npoints(x->x_array);
    } else {
        post("wavesel: no array \"%s\" (error %d)", 
            x->x_arrayname->s_name, array);
        x->x_array = 0;
        x->x_arraysize = 0;
        return;
    }
}

// object state info for debug
static void wavesel_state(t_wavesel *x)
{
    post(" --==## c.wavesel_state ##==--");
    post("x_array: %s",          (x->x_array) ? "defined" : "null");
    post("x_arrayname: %s",       x->x_arrayname->s_name);
    post("x_arraysize: %d",       x->x_arraysize);
    post("x_ksr: %f",             x->x_ksr);
    post("x_samplesPerColumn: %f", x->x_samplesPerColumn);
    post("x_outputFactor: %f",    x->x_outputFactor);
    post("-");
    post("bg_rect- x: %f, y: %f, width: %f, height: %f", x->bg_rect.x, 
        x->bg_rect.y, x->bg_rect.width, x->bg_rect.height);
    post("fg_rect- x: %f, y: %f, width: %f, height: %f", x->bg_rect.x, 
        x->bg_rect.y, x->fg_rect.width, x->fg_rect.height);
    post("-");
    post("background layer name: %s", BACKGROUNDLAYER);
    post("background layer state: %d", (x->bg_layer) ? x->bg_layer->e_state : 9999); // 9999 here stands for undefined
    post("background layer objects: %d", x->bg_layer->e_number_objects);
    post("waveform layer name: %s",   WAVEFORMLAYER);
    post("waveform layer state: %d", (x->fg_layer) ? x->fg_layer->e_state : 9999);
    post("waveform layer objects: %d", x->fg_layer->e_number_objects);
//    post("selection layer name: %s",  SELECTEDLAYER);
//    post("selection layer state: %d", (x->sel_layer) ? x->sel_layer->e_state : 9999);
}

static void *wavesel_new(t_symbol *s, int argc, t_atom *argv)
{
    t_wavesel *x = (t_wavesel *)eobj_new(wavesel_class);
    t_binbuf* d     = binbuf_via_atoms(argc, argv);
    
    if(x && d)
    {
        ebox_new((t_ebox *)x, 0 | EBOX_GROWINDI);
        
        // currently just reports the four mouse data x values
        x->f_out_1   = outlet_new((t_object *)x, &s_float);
        x->f_out_2   = outlet_new((t_object *)x, &s_float);
        x->f_out_3   = outlet_new((t_object *)x, &s_float);
        x->f_out_4   = outlet_new((t_object *)x, &s_float);
        
        x->f_width      = 1;
        x->f_color      = gensym("#000000");
        x->f_fill       = 0;

        ebox_attrprocess_viabinbuf(x, d);
        ebox_ready((t_ebox *)x);
    }
    
    #define MILLIS 0.001
    x->x_ksr   = sys_getsr() * MILLIS; // kilo sample rate
    x->x_arrayname = gensym("");
    x->x_array     = 0;
    x->x_samplesPerColumn = 0.f; // how many samples are represented in each wavefor column
    x->x_outputFactor = 0.f; // converts cursor position to ms
    x->x_clipmode  = 0;
    x->x_selection = 0;
    
    x->bg_layer  = 0;
    x->fg_layer  = 0;
    x->sel_layer = 0;
    x->bg_layer_name  = gensym(BACKGROUNDLAYER);
    x->fg_layer_name  = gensym(WAVEFORMLAYER); 
    x->sel_layer_name = gensym(SELECTEDLAYER);
    x->sel_start_cursor = 100; // arbitrary test value
    x->sel_end_cursor   = 200; // arbitrary test value
    
    post("%s instantiated", s->s_name);
    
    return (x);
}

static void wavesel_free(t_wavesel *x)
{
    ebox_free((t_ebox *)x);

}

extern "C" void setup_c0x2ewavesel(void)
{
    t_eclass *c;
    
    c = eclass_new("c.wavesel", (method)wavesel_new, 
        (method)wavesel_free, (short)sizeof(t_wavesel), 0L, A_GIMME, 0);
    eclass_guiinit(c, 0);

    eclass_addmethod(c, (method) wavesel_mousemove, "mousemove", A_NULL, 0);
    eclass_addmethod(c, (method) wavesel_mousedrag, "mousedrag", A_NULL, 0);
    eclass_addmethod(c, (method) wavesel_mousedown, "mousedown", A_NULL, 0);
    eclass_addmethod(c, (method) wavesel_mouseup,   "mouseup",   A_NULL, 0);
    eclass_addmethod(c, (method) wavesel_paint,     "paint",     A_NULL, 0);

    eclass_addmethod(c, (method) wavesel_setarray,  "setarray",  A_SYMBOL, 0);
    eclass_addmethod(c, (method) wavesel_state,     "state",     A_NULL, 0);
    eclass_addmethod(c, (method) wavesel_redraw_waveform,    "redraw",    A_NULL, 0);
//    eclass_addmethod(c, (method) wavesel_draw_selection,  "drawselection",    A_NULL, 0);
    
    CLASS_ATTR_RGBA      (c, "sicolor", 0, t_wavesel, f_color_waveform);
    CLASS_ATTR_LABEL                (c, "sicolor", 0, "Waveform Color");
    CLASS_ATTR_ORDER                (c, "sicolor", 0, "3");
    CLASS_ATTR_DEFAULT_SAVE_PAINT   (c, "sicolor", 0, "0. 0.6 0. 0.8");
    CLASS_ATTR_STYLE                (c, "sicolor", 0, "color");

    CLASS_ATTR_RGBA      (c, "secolor", 0, t_wavesel, f_color_selection);
    CLASS_ATTR_LABEL                (c, "secolor", 0, "Selection Color");
    CLASS_ATTR_ORDER                (c, "secolor", 0, "3");
    CLASS_ATTR_DEFAULT_SAVE_PAINT   (c, "secolor", 0, "0.5 0.5 0.5 0.8");
    CLASS_ATTR_STYLE                (c, "secolor", 0, "color");

    eclass_register(CLASS_BOX, c);
    wavesel_class = c;
    
    post("c.wavesel~ 0.1 , a waveform~ wannabe, that isn't there yet...");
    post("fjkraan@xs4all.nl, 2015-07-09");

}







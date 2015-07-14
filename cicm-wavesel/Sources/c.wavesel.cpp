
#include "../c.library.hpp"

typedef struct  _wavesel
{
    t_ebox      j_box;
    t_garray*   x_array;
    t_symbol*   x_arrayname;
    int         x_arraysize;
    t_float     x_samplesPerColumn;
    t_float     x_ksr;
    t_float     x_outputFactor;
    t_float     x_currentElementTop;
    t_float     x_currentElementBottom;
    int         x_clipmode;
    
    int         sel_start_cursor;
    int         sel_end_cursor;    
    
    t_outlet*   f_out_1;
    t_outlet*   f_out_2;
    t_outlet*   f_out_3;
    t_outlet*   f_out_4;
    
    t_rgba		f_color_background;
    t_rgba		f_color_border;
    t_rgba      f_color_waveform;
    t_rgba      f_color_selection;
} t_wavesel;

static t_symbol*   wavesel_sym_background_layer;
static t_symbol*   wavesel_sym_waveform_layer;
static t_symbol*   wavesel_sym_selection_layer;
static t_eclass*   wavesel_class;

static void wavesel_draw_background(t_wavesel *x, t_rect *rect)
{
    t_elayer *g = ebox_start_layer((t_ebox *)x, wavesel_sym_background_layer, rect->width, rect->height);
    if(g)
    {
        egraphics_set_color_rgba(g, &x->f_color_background);
        egraphics_rectangle(g, 0., 0., rect->width, rect->height);
        egraphics_fill(g);
        
/*        egraphics_set_color_rgba(g, &x->f_color_border);
        for(int i = 1; i < 5; i++)
        {
            egraphics_line(g, rect->width / 5.f * float(i), 0., rect->width / 5.f * float(i), rect->height);
            egraphics_stroke(g);
        } */

        ebox_end_layer((t_ebox*)x, wavesel_sym_background_layer);
    }
    ebox_paint_layer((t_ebox *)x, wavesel_sym_background_layer, 0.f, 0.f);
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

static void wavesel_draw_waveform(t_wavesel *x, t_rect *rect)
{
    t_elayer *g = ebox_start_layer((t_ebox *)x, wavesel_sym_waveform_layer,rect->width, rect->height);
    if(g)
    {
        t_word *vec  = NULL;
        int size     = 0;
        int middle = (int)rect->height / 2;
        t_float top = 1.f;
        t_float bottom = 0.f;
        int columns = (int)rect->width;
        if(x->x_array)
        {
            garray_getfloatwords(x->x_array, &size, &vec);
            x->x_arraysize = size;
            x->x_samplesPerColumn = (t_float)x->x_arraysize / rect->width;
            x->x_outputFactor = x->x_samplesPerColumn / x->x_ksr;
        }
        egraphics_set_color_rgba(g, &x->f_color_waveform);
        if(vec && size)
        {
            if (x->x_samplesPerColumn > 1)
            {
                for(int i = 0; i < columns; i++)
                {
                    wavesel_get_column_size(x, i, vec);
                    top    = x->x_currentElementTop;
                    bottom = x->x_currentElementBottom;

                    egraphics_line(g, 
                        i, middle + floor(0.5f - top * rect->height), 
                        i, middle + floor(0.5f - bottom * rect->height)); 
                }
            }
            else
            {
                egraphics_move_to(g, 0., rect->height * 0.5f + vec[0].w_float * rect->height);
                for(int i = 0; i < (int)rect->width && i < size; i++)
                {
                    egraphics_line_to(g, float(i), rect->height * 0.5f + vec[i].w_float * rect->height);
                } 
            }
            egraphics_stroke(g);
        }
        else
        {
            egraphics_line(g, 0., rect->height * 0.5f, rect->width, rect->height * 0.5f);
            egraphics_stroke(g);
        }
        
        ebox_end_layer((t_ebox *)x, wavesel_sym_waveform_layer);
    }
    ebox_paint_layer((t_ebox *)x, wavesel_sym_waveform_layer, 0.f, 0.f);
}

static void wavesel_paint(t_wavesel *x, t_object *view)
{
    t_rect rect;
    ebox_get_rect_for_view((t_ebox *)x, &rect);
    wavesel_draw_background(x, &rect);
    wavesel_draw_waveform(x, &rect);
}

// mouse position x values to be integrated in selection logic
static void wavesel_mousedown(t_wavesel *x, t_object *patcherview, t_pt pt, long modifiers)
{
    outlet_float(x->f_out_1, pt.x);
    if (patcherview && modifiers) {} // suppress compiler warnings
}

static void wavesel_mouseup(t_wavesel *x, t_object *patcherview, t_pt pt, long modifiers)
{
    outlet_float(x->f_out_2, pt.x);
    if (patcherview && modifiers) {} // suppress compiler warning
}

static void wavesel_mousedrag(t_wavesel *x, t_object *patcherview, t_pt pt, long modifiers)
{
    outlet_float(x->f_out_3, pt.x);
    if (patcherview && modifiers) {} // suppress compiler warning
}

static void wavesel_mousemove(t_wavesel *x, t_object *patcherview, t_pt pt, long modifiers)
{
    outlet_float(x->f_out_4, pt.x);
    if (patcherview && modifiers) {} // suppress compiler warning
}

static t_pd_err wavesel_sa(t_wavesel *x, t_object *attr, int ac, t_atom *av)
{
    if(ac && av && atom_gettype(av) == A_SYMBOL)
    {
        x->x_arrayname  = atom_getsymbol(av);
        if(x->x_arrayname)
        {
            x->x_array = (t_garray *)pd_findbyclass(x->x_arrayname, garray_class);
            ebox_invalidate_layer((t_ebox *)x, wavesel_sym_waveform_layer);
            ebox_redraw((t_ebox *)x);
        }
    }
    return -1;
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
//        x->x_arraysize = garray_npoints(x->x_array);
    } else {
        post("wavesel: no array \"%s\" (error %d)", 
            x->x_arrayname->s_name, array);
        x->x_array = 0;
//        x->x_arraysize = 0;
        return;
    }
    ebox_invalidate_layer((t_ebox *)x, wavesel_sym_waveform_layer);
    ebox_redraw((t_ebox *)x);

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
/*    post("-");
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
    post("waveform layer objects: %d", x->fg_layer->e_number_objects); */
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
        
        x->f_out_1   = outlet_new((t_object *)x, &s_float);
        x->f_out_2   = outlet_new((t_object *)x, &s_float);
        x->f_out_3   = outlet_new((t_object *)x, &s_float);
        x->f_out_4   = outlet_new((t_object *)x, &s_float);

        #define MILLIS 0.001
        x->x_ksr   = sys_getsr() * MILLIS; // kilo sample rate
        x->x_arrayname = gensym("");
        x->x_array     = 0;
        x->x_samplesPerColumn = 0.f; // how many samples are represented in each wavefor column
        x->x_outputFactor = 0.f; // converts cursor position to ms
        x->x_clipmode  = 0;
        
        ebox_attrprocess_viabinbuf(x, d);
        ebox_ready((t_ebox *)x);
        
        return x;
    }
    
    return NULL;
}

static void wavesel_free(t_wavesel *x)
{
    ebox_free((t_ebox *)x);
}

extern "C" void setup_c0x2ewavesel(void)
{
    t_eclass *c;
    
    c = eclass_new("c.wavesel", (method)wavesel_new, (method)wavesel_free, (short)sizeof(t_wavesel), 0L, A_GIMME, 0);
    eclass_guiinit(c, 0);

    eclass_addmethod(c, (method) wavesel_mousemove, "mousemove", A_NULL, 0);
    eclass_addmethod(c, (method) wavesel_mousedrag, "mousedrag", A_NULL, 0);
    eclass_addmethod(c, (method) wavesel_mousedown, "mousedown", A_NULL, 0);
    eclass_addmethod(c, (method) wavesel_mouseup,   "mouseup",   A_NULL, 0);
    eclass_addmethod(c, (method) wavesel_paint,     "paint",     A_NULL, 0);
    eclass_addmethod(c, (method) wavesel_setarray,  "setarray",     A_SYMBOL, 0);
    eclass_addmethod(c, (method) wavesel_state,     "state",     A_NULL, 0);

    
    CLASS_ATTR_SYMBOL               (c, "array", 0, t_wavesel, x_arrayname);
    CLASS_ATTR_LABEL                (c, "array", 0, "Array Name");
    CLASS_ATTR_ACCESSORS			(c, "array", NULL, wavesel_sa);
    CLASS_ATTR_ORDER                (c, "array", 0, "1");
    CLASS_ATTR_DEFAULT              (c, "array", 0, "");
    CLASS_ATTR_STYLE                (c, "array", 0, "entry");
    
    CLASS_ATTR_RGBA                 (c, "bgcolor", 0, t_wavesel, f_color_background);
    CLASS_ATTR_LABEL                (c, "bgcolor", 0, "Background Color");
    CLASS_ATTR_ORDER                (c, "bgcolor", 0, "2");
    CLASS_ATTR_DEFAULT_SAVE_PAINT   (c, "bgcolor", 0, "0.75 0.75 0.75 1.");
    CLASS_ATTR_STYLE                (c, "bgcolor", 0, "color");
    
    CLASS_ATTR_RGBA                 (c, "bdcolor", 0, t_wavesel, f_color_border);
    CLASS_ATTR_LABEL                (c, "bdcolor", 0, "Border Color");
    CLASS_ATTR_ORDER                (c, "bdcolor", 0, "2");
    CLASS_ATTR_DEFAULT_SAVE_PAINT   (c, "bdcolor", 0, "0.5 0.5 0.5 1.");
    CLASS_ATTR_STYLE                (c, "bdcolor", 0, "color");
    
    CLASS_ATTR_RGBA                 (c, "sicolor", 0, t_wavesel, f_color_waveform);
    CLASS_ATTR_LABEL                (c, "sicolor", 0, "Waveform Color");
    CLASS_ATTR_ORDER                (c, "sicolor", 0, "4");
    CLASS_ATTR_DEFAULT_SAVE_PAINT   (c, "sicolor", 0, "0. 0.6 0. 0.8");
    CLASS_ATTR_STYLE                (c, "sicolor", 0, "color");

    CLASS_ATTR_RGBA                 (c, "secolor", 0, t_wavesel, f_color_selection);
    CLASS_ATTR_LABEL                (c, "secolor", 0, "Selection Color");
    CLASS_ATTR_ORDER                (c, "secolor", 0, "5");
    CLASS_ATTR_DEFAULT_SAVE_PAINT   (c, "secolor", 0, "0.5 0.5 0.5 0.8");
    CLASS_ATTR_STYLE                (c, "secolor", 0, "color");

    eclass_register(CLASS_BOX, c);
    wavesel_class = c;
    
    wavesel_sym_background_layer = gensym("background_layer");
    wavesel_sym_waveform_layer = gensym("foreground_layer");
    wavesel_sym_selection_layer  = gensym("selection_layer");
    
    post("c.wavesel~ 0.1 , a waveform~ wannabe, that isn't there yet...");
    post("fjkraan@xs4all.nl, 2015-07-09");

}







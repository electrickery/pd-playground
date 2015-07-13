
#include "../c.library.hpp"

typedef struct  _wavesel
{
    t_ebox      j_box;
    t_garray*   x_array;
    t_symbol*   x_arrayname;
    
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
        
        egraphics_set_color_rgba(g, &x->f_color_border);
        for(int i = 1; i < 5; i++)
        {
            egraphics_line(g, rect->width / 5.f * float(i), 0., rect->width / 5.f * float(i), rect->height);
            egraphics_stroke(g);
        }

        ebox_end_layer((t_ebox*)x, wavesel_sym_background_layer);
    }
    ebox_paint_layer((t_ebox *)x, wavesel_sym_background_layer, 0.f, 0.f);
}

static void wavesel_draw_waveform(t_wavesel *x, t_rect *rect)
{
    t_elayer *g = ebox_start_layer((t_ebox *)x, wavesel_sym_waveform_layer,rect->width, rect->height);
    if(g)
    {
        t_word *vec  = NULL;
        int size     = 0;
        if(x->x_array)
        {
            garray_getfloatwords(x->x_array, &size, &vec);
        }
        
        egraphics_set_color_rgba(g, &x->f_color_waveform);
        if(vec && size)
        {
            egraphics_move_to(g, 0., rect->height * 0.5f + vec[0].w_float * rect->height);
            for(int i = 0; i < (int)rect->width && i < size; i++)
            {
                egraphics_line_to(g, float(i), rect->height * 0.5f + vec[i].w_float * rect->height);
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

static t_pd_err wavesel_setarray(t_wavesel *x, t_object *attr, int ac, t_atom *av)
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
        x->x_arrayname = gensym("");
        x->x_array     = 0;
        
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
    
    CLASS_ATTR_SYMBOL               (c, "array", 0, t_wavesel, x_arrayname);
    CLASS_ATTR_LABEL                (c, "array", 0, "Array Name");
    CLASS_ATTR_ACCESSORS			(c, "array", NULL, wavesel_setarray);
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







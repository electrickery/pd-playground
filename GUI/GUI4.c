/* GUI2, skeleton GUI object, based on gcanvas from Guenter Geiger,
 * iem_gui objects from Thomas Musil and the zoom function from 
 * Miller Puckette
 * Fred Jan Kraan, fjkraan@xs4all.nl, 2016-11-27 */

#include <math.h>
#include "m_pd.h"
#include "g_canvas.h"
#include "s_stuff.h"

/* ------------------------ GUI ----------------------------- */

#define MAJORVERSION 0
#define MINORVERSION 1
#define BUGFIXVERSION 0

#define GUI_ZOOM(x) ((x)->x_glist->gl_zoom)
/* IOHEIGHT 1 is Pd-vanilla mode, IOHEIGHT 2 is Pd-extendend mode */
#define GUI_IOHEIGHT 2

static t_class *GUI_class;

typedef struct _GUI
{
    t_object   x_obj;
    t_glist   *x_glist;
    int        canvas_width;
    int        canvas_height;
    int        inletCount;  /* GUI xlet bookkeeping only! */
    int        outletCount;
    int        prevX;  /* only to limit */
    int        prevY;  /*  movement logging */
} t_GUI;

/* ------------------- widget helper functions ------------------- */

void GUI_drawme(t_GUI *x, t_glist *glist, int firsttime)
{
    int xpos      = x->x_obj.te_xpix;
    int ypos      = x->x_obj.te_ypix;
    int width     = x->canvas_width  * GUI_ZOOM(x);
    int height    = x->canvas_height * GUI_ZOOM(x);
    int linewidth = 1 * GUI_ZOOM(x);
    int iowidth   = IOWIDTH * GUI_ZOOM(x);
    int ioheight  = GUI_IOHEIGHT * GUI_ZOOM(x);
    t_canvas *canvas = glist_getcanvas(glist);
        
    if (firsttime) {
        /* main rectangle */
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -width %d -tags %lxBASE\n",
             canvas, 
             xpos,         ypos, 
             xpos + width, ypos + height, 
             linewidth, x);
printf(".x%lx.c create rectangle %d %d %d %d -width %d -tags %lxBASE\n", 
    (long int)canvas, xpos, ypos, xpos + width, ypos + height, linewidth, (long int)x);    
    } else {
        sys_vgui(".x%lx.c coords %lxBASE %d %d %d %d\n",
             canvas, x, 
             xpos,         ypos, 
             xpos + width, ypos + height);
printf(".x%lx.c coords %lxBASE %d %d %d %d\n", 
    (long int)canvas, (long int)x, xpos, ypos, xpos + width, ypos + height);
    }
    int n, nplus, i, onset;

    /* inlets */
    n = x->inletCount;
    nplus = (n == 1 ? 1 : n-1);
    for (i = 0; i < n; i++)
    {
        onset = xpos + (width - iowidth) * i / nplus;
        if (firsttime)
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -width %d -tags %lxIN%d\n",
                canvas,
                onset,           ypos,
                onset + iowidth, ypos + ioheight,
                linewidth, x, i);
        else
            sys_vgui(".x%lx.c coords %lxIN%d %d %d %d %d\n",
                canvas, x, i,
                onset,           ypos, 
                onset + iowidth, ypos + ioheight);
    }
    printf(" %s %d inlet(s) ", (firsttime) ? "created" : "moved", n);
      
    /* outlets */
    n = x->outletCount;
    nplus = (n == 1 ? 1 : n-1);
    for (i = 0; i < n; i++)
    {
        onset = xpos + (width - iowidth) * i / nplus;
        if (firsttime)
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -width %d -tags %lxOUT%d\n",
                canvas,
                onset,           ypos + height - ioheight,
                onset + iowidth, ypos + height,
                linewidth, x, i);
        else
            sys_vgui(".x%lx.c coords %lxOUT%d %d %d %d %d\n",
                canvas, x, i,
                onset,           ypos + height - ioheight,
                onset + iowidth, ypos + height);

    }
    canvas_fixlinesfor(glist,(t_text*) x);
    printf(" and %d outlet(s)\n", n);
}

/* ---------------- widget behaviour functions -------------------- */

static void GUI_getrect(t_gobj *z, t_glist *owner,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_GUI* x = (t_GUI*)z;

    int xpos      = x->x_obj.te_xpix;
    int ypos      = x->x_obj.te_ypix;
    int width     = x->canvas_width  * GUI_ZOOM(x);
    int height    = x->canvas_height * GUI_ZOOM(x);
    
    /* debug section */
    if (xpos != x->prevX || ypos != x->prevY) {
printf("getrect: xpos: %d, ypos: %d, width: %d, height: %d\n", 
    xpos, ypos, width, height);
        x->prevX = xpos;
        x->prevY = ypos;
    }
    *xp1 = xpos;
    *yp1 = ypos;
    *xp2 = xpos + width;
    *yp2 = ypos + height;
} 

static void GUI_displace(t_gobj *z, t_glist *glist,
    int dx, int dy)
{
printf("displace: dx: %d, dy: %d\n", dx, dy);
    t_GUI *x = (t_GUI *)z;
    
    x->x_obj.te_xpix += dx;
    x->x_obj.te_ypix += dy;
    GUI_drawme(x, glist, 0);
}

// canvas can be selected
static void GUI_select(t_gobj *z, t_glist *glist, int state)
{
printf("select: %d\n", state);
    t_GUI *x = (t_GUI *)z;
    char* color = "black";
    if (state) {
        color = "blue";
    }
    sys_vgui(".x%lx.c itemconfigure %lxBASE -outline %s\n", 
        glist_getcanvas(glist), x, color);
printf(".x%lx.c itemconfigure %lxBASE -outline %s\n", 
    (long int)glist_getcanvas(glist), (long int)x, color);

}

static void GUI_activate(t_gobj *z, t_glist *glist, int state)
{
printf("activate: %d\n", state);
/*    t_GUI* x = (t_GUI *)z; */
    if (z || glist || state) { } // eliminate compiler warnings
}

static void GUI_delete(t_gobj *z, t_glist *glist)
{
    t_GUI *x = (t_GUI *)z;
    sys_vgui(".x%lx.c delete %lxBASE\n", glist_getcanvas(glist), x, 0);
printf(".x%lx.c delete %lxBASE\n", 
    (long int)glist_getcanvas(glist), (long int)x);

    /* inlets */
    int i;
    int n = x->inletCount;
    for (i = 0; i < n; i++) {
        sys_vgui(".x%lx.c delete %lxIN%d\n", glist_getcanvas(glist), x, 0);
    }
printf(" deleted %d inlets ", n);
    /* outlets */
    n = x->outletCount;
    for (i = 0; i < n; i++) {
        sys_vgui(".x%lx.c delete %lxOUT%d\n", glist_getcanvas(glist), x, 0);
    }
printf(" and %d outlets\n", n);
    canvas_deletelinesfor(glist, (t_text *)z);
}

#define FRAME 0       
static void GUI_vis(t_gobj *z, t_glist *glist, int vis)
{
printf("vis: %d\n", vis);
    t_GUI* x = (t_GUI *)z;


    if (vis) {
        GUI_drawme(x, glist, 1);
    } else {
        GUI_delete(z, glist);
     }
}

static int GUI_newclick(t_gobj *z, struct _glist *glist,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
printf("newclick: %d\n", doit);  
    if (z && glist && xpix && ypix && shift && alt && dbl && doit) { } // eliminate compiler warnings
    return (1);
}

static void GUI_bang(t_GUI *x)
{
    if (x) {} /* prevents compiler warnings */
    post("GUI4 banged");
}

static void GUI_status(t_GUI *x)
{
    post("  --==## GUI4 status %d.%d.%d##==--", MAJORVERSION, MINORVERSION, BUGFIXVERSION);
    post("canvas_width: %d", x->canvas_width);
    post("canvas_height %d", x->canvas_height);
    post("inletCount:   %d", x->inletCount);
    post("outletCount:  %d", x->outletCount);
    post("prevX:        %d", x->prevX);
    post("prevY:        %d", x->prevY);
    post("zoom:         %d", x->x_glist->gl_zoom);
}

static void GUI_zoom(t_GUI *x, t_float zoomf)
{
    post("GUI4 zoom: %f", zoomf);
    x->x_glist->gl_zoom = (int)zoomf;
}

t_widgetbehavior   GUI_widgetbehavior;

// when is what called (also see g_canvas.h):
// GUI_getrect  - mouse cursor inside patch window
// GUI_displace - move canvas in edit mode
// GUI_select   - GUI object is selected in edit mode
// GUI_activate - GUI object is moved in edit mode
// GUI_delete   - GUI object is removed
// GUI_vis      - patch window is visable (again)
// GUI_newclick - mouse cursor inside GUI canvas
static void GUI_setwidget(void)
{
    GUI_widgetbehavior.w_getrectfn  = GUI_getrect;
    GUI_widgetbehavior.w_displacefn = GUI_displace;
    GUI_widgetbehavior.w_selectfn   = GUI_select;
    GUI_widgetbehavior.w_activatefn = GUI_activate;
    GUI_widgetbehavior.w_deletefn   = GUI_delete;
    GUI_widgetbehavior.w_visfn      = GUI_vis;
    GUI_widgetbehavior.w_clickfn    = GUI_newclick;
}

static void *GUI_new(t_symbol *s, int ac, t_atom *av)
{
printf("----\n%s new \n", s->s_name);
    if (ac && av) {} /* prevents compiler warnings */

    t_GUI *x = (t_GUI *)pd_new(GUI_class);
    
    x->canvas_width  = 20;
    x->canvas_height = 20;
    x->prevX = 0;
    x->prevY = 0;
    x->inletCount = 1;
    x->outletCount = 1;
    x->x_glist = (t_glist *)canvas_getcurrent();
    outlet_new(&x->x_obj, &s_float);
    
    return (x);
}

void GUI4_setup(void)
{
printf("----\nGUI4 setup \n");
    GUI_class = class_new(gensym("GUI4"), 
        (t_newmethod)GUI_new, 0, sizeof(t_GUI), 0, A_GIMME, 0);

// set graphic event behaviour
    GUI_setwidget();
// override the standard object graphic behaviour    
    class_setwidget(GUI_class, &GUI_widgetbehavior);
    class_addmethod(GUI_class, (t_method)GUI_bang, gensym("bang"), 0);
    class_addmethod(GUI_class, (t_method)GUI_status, gensym("status"), 0);
    class_addmethod(GUI_class, (t_method)GUI_zoom, gensym("zoom"),
        A_CANT, 0);
    
    post("GUI4 %d.%d.%d", MAJORVERSION, MINORVERSION, BUGFIXVERSION);
    post("fjkraan@xs4all.nl, 2016-08-07");
}

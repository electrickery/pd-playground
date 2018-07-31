/* GUI5, skeleton GUI object, based on gcanvas from Guenter Geiger,
 * iem_gui objects from Thomas Musil and the zoom function from 
 * Miller Puckette
 * 
 * This object uses the mouse button tracking mechanism from the
 * iem_guts/receive_canvas object.
 * 
 * 
 * Fred Jan Kraan, fjkraan@xs4all.nl, 2018-07-31 */

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

#define GUI_MINSIZE 10

static t_class *GUI_class;
static t_class *GUI_proxy_class;

typedef struct _GUI_proxy
{
  t_object     p_obj;
  t_symbol    *p_sym;
  t_clock     *p_clock;
  struct _GUI *p_parent;
} t_GUI_proxy;

typedef struct _GUI
{
    t_object     x_obj;
    t_glist     *x_glist;
    int          canvas_width;
    int          canvas_height;
    int          inletCount;  /* GUI xlet bookkeeping only! */
    int          outletCount;
    int          X;
    int          Y;
    int          prevX;  /* only to limit */
    int          prevY;  /*  movement logging */
    int          oldX;   /* for differential X */
    int          oldY;   /* for differential Y */
    int          shift;
    int          alt;
    int          mouseState;
    t_GUI_proxy *x_proxy;
} t_GUI;

/* ------------------------- GUI proxy ---------------------------- */

static void GUI_anything(t_GUI *x, t_symbol*s, int argc, t_atom*argv);

static void GUI_proxy_anything(t_GUI_proxy *p, t_symbol*s, int argc, t_atom*argv) {
    if(p->p_parent)
        GUI_anything(p->p_parent, s, argc, argv);
}
static void GUI_proxy_free(t_GUI_proxy *p)
{
    if(p->p_sym)
        pd_unbind(&p->p_obj.ob_pd, p->p_sym);
    p->p_sym = NULL;

    clock_free(p->p_clock);
    p->p_clock = NULL;

    p->p_parent = NULL;
    pd_free(&p->p_obj.ob_pd);

    p = NULL;
}
static t_GUI_proxy *GUI_proxy_new(t_GUI *x, t_symbol*s) {
    t_GUI_proxy *p = NULL;

    if(!x) return p;

    p = (t_GUI_proxy*)pd_new(GUI_proxy_class);

    p->p_sym=s;
    if(p->p_sym) {
        pd_bind(&p->p_obj.ob_pd, p->p_sym);
    }
    p->p_parent = x;
    p->p_clock = clock_new(p, (t_method)GUI_proxy_free);

    return p;
}


/* ------------------- widget helper functions ------------------- */

static void GUI_free(t_GUI *x)
{
  if(x->x_proxy) {
    x->x_proxy->p_parent = NULL;
    clock_delay(x->x_proxy->p_clock, 0);
  }
}

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

void GUI_eraseme(t_GUI *x, t_glist *glist)
{
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
    
    if (owner) {} // prevent compiler warning
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
    canvas_deletelinesfor(glist, (t_text *)z);
    // removal of non-gui stuff like freebytes(), pd_unbind(), pd_free(), ...
}

#define FRAME 0       
static void GUI_vis(t_gobj *z, t_glist *glist, int vis)
{
printf("vis: %d\n", vis);
    t_GUI* x = (t_GUI *)z;

    if (vis) {
        GUI_drawme(x, glist, 1);
    } else {
        GUI_eraseme(z, glist);
     }
}

static int GUI_newclick(t_gobj *z, struct _glist *glist,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    if (z || glist || xpix || ypix || shift || alt || dbl || doit) {}
    return (1);
}

/* ---------------- message functions -------------------- */

static void GUI_anything(t_GUI *x, t_symbol*s, int argc, t_atom*argv) {
    int X = atom_getintarg(0, argc, argv);
    int Y = atom_getintarg(1, argc, argv);
    int dX = X - x->oldX;
    int dY = Y - x->oldY;
    x->oldX = X;
    x->oldY = Y;
            
   if (X > x->x_obj.te_xpix && 
       Y > x->x_obj.te_ypix &&
       X < x->x_obj.te_xpix + x->canvas_width  * GUI_ZOOM(x) &&
       Y < x->x_obj.te_ypix + x->canvas_height * GUI_ZOOM(x)) {
    
        if (s == gensym("mouse"))
            post("mousedown");
        if (s == gensym("mouseup"))
            post("mouseup");

        if (s == gensym("motion")) {
            outlet_anything(x->x_obj.ob_outlet, s, argc, argv);
            post("symbol: %s, argc: %d, %d, %d, %d", s->s_name, X, Y, dX, dY);
        }
    }
    
}

static void GUI_bang(t_GUI *x)
{
    if (x) {} /* prevents compiler warnings */
    post("GUI4 banged");
}

static void GUI_size(t_GUI *x, t_float xSize, t_float ySize)
{
    post("GUI size %f %f", xSize, ySize);
    if (xSize < GUI_MINSIZE || ySize < GUI_MINSIZE) return;
    x->canvas_width  = (int)xSize;
    x->canvas_height = (int)ySize;
    GUI_drawme(x, x->x_glist, 0);
}

static void GUI_status(t_GUI *x)
{
    post("  --==## %s status %d.%d.%d##==--", 
        "GUI6", MAJORVERSION, MINORVERSION, BUGFIXVERSION);
    post("canvas_x:      %d", x->x_obj.te_xpix);
    post("canvas_y:      %d", x->x_obj.te_ypix);
    post("canvas_width:  %d", x->canvas_width);
    post("canvas_height: %d", x->canvas_height);
    post("inletCount:    %d", x->inletCount);
    post("outletCount:   %d", x->outletCount);
    post("prevX:         %d", x->prevX);
    post("prevY:         %d", x->prevY);
    post("zoom:          %d", x->x_glist->gl_zoom);
    post("font size:     %d", x->x_glist->gl_font);
    post("patch name:    %s", x->x_glist->gl_name->s_name);
    post("compiled against Pd version:  %d.%d.%d",  
        PD_MAJOR_VERSION, PD_MINOR_VERSION, PD_BUGFIX_VERSION);
    int major, minor, bugfix;
    sys_getversion(&major, &minor, &bugfix);
    post("this Pd version: %d.%d.%d", major, minor, bugfix);
}

static void GUI_zoom(t_GUI *x, t_float zoomf)
{
    post("zoom: %f", zoomf);
    x->x_glist->gl_zoom = (int)zoomf;
}

t_widgetbehavior   GUI_widgetbehavior;

// when is what called (also see g_canvas.h):
// GUI_getrect  - provides current location and size
// GUI_displace - moves gui parts in edit mode
// GUI_select   - selects or deselects object gui in edit mode
// GUI_activate - GUI object is moved in edit mode
// GUI_delete   - deletes non-gui parts of the object
// GUI_vis      - creates or deletes gui parts
// GUI_newclick - mouse and keyboard info inside object rectangle
static void GUI_setwidget(void)
{
    GUI_widgetbehavior.w_getrectfn  = GUI_getrect;  // (t_gobj *z, t_glist *owner, int *xp1, int *yp1, int *xp2, int *yp2)
    GUI_widgetbehavior.w_displacefn = GUI_displace; // (t_gobj *z, t_glist *glist, int dx, int dy)
    GUI_widgetbehavior.w_selectfn   = GUI_select;   // (t_gobj *z, t_glist *glist, int state)
    GUI_widgetbehavior.w_activatefn = GUI_activate; // (t_gobj *z, t_glist *glist, int state)
    GUI_widgetbehavior.w_deletefn   = GUI_delete;   // (t_gobj *z, t_glist *glist)
    GUI_widgetbehavior.w_visfn      = GUI_vis;      // (t_gobj *z, t_glist *glist, int vis)
    GUI_widgetbehavior.w_clickfn    = GUI_newclick; // (t_gobj *z, struct _glist *glist, int xpix, int ypix, int shift, int alt, int dbl, int doit)
}

static void *GUI_new(t_symbol *s, int ac, t_atom *av)
{
printf("----\n%s new \n", s->s_name);
    if (ac && av) {} /* prevents compiler warnings */

    t_GUI *x = (t_GUI *)pd_new(GUI_class);
    t_glist *glist   = (t_glist *)canvas_getcurrent();
    t_canvas *canvas = (t_canvas *)glist_getcanvas(glist);
    
    x->canvas_width  = 50;
    x->canvas_height = 50;
    x->prevX         = 0;
    x->prevY         = 0;
    x->oldX         = 0;
    x->oldY         = 0;
    x->inletCount    = 1;
    x->outletCount   = 1;
    x->x_glist       = (t_glist *)canvas_getcurrent();
    x->shift         = 0;
    x->mouseState    = 0;
    outlet_new(&x->x_obj, &s_float);
    
    x->x_proxy = NULL;

    if(canvas) {
        char buf[MAXPDSTRING];
        snprintf(buf, MAXPDSTRING-1, ".x%lx", (t_int)canvas);
        buf[MAXPDSTRING-1]=0;

        x->x_proxy = GUI_proxy_new(x, gensym(buf));
    }
    
    return (x);
}

void GUI6_setup(void)
{
printf("----\nGUI6 setup \n");
    GUI_class = class_new(gensym("GUI6"), 
        (t_newmethod)GUI_new, (t_method)GUI_free, sizeof(t_GUI), 0, A_GIMME, 0);

// set graphic event behaviour
    GUI_setwidget();
// override the standard object graphic behaviour    
    class_setwidget(GUI_class, &GUI_widgetbehavior);
    class_addmethod(GUI_class, (t_method)GUI_bang,   gensym("bang"), 0);
    class_addmethod(GUI_class, (t_method)GUI_size,   gensym("size"), A_DEFFLOAT, A_DEFFLOAT,0);
    class_addmethod(GUI_class, (t_method)GUI_status, gensym("status"), 0);
    class_addmethod(GUI_class, (t_method)GUI_zoom,   gensym("zoom"),
        A_CANT, 0);
        
    GUI_proxy_class = class_new(0, 0, 0, sizeof(t_GUI_proxy), CLASS_NOINLET | CLASS_PD, 0);
    class_addanything(GUI_proxy_class, GUI_proxy_anything);
    
    post("GUI6 %d.%d.%d", MAJORVERSION, MINORVERSION, BUGFIXVERSION);
    post("fjkraan@xs4all.nl, 2018-07-29");
}

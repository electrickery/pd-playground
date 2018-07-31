/* GUI5, skeleton GUI object, based on gcanvas from Guenter Geiger,
 * iem_gui objects from Thomas Musil and the zoom function from 
 * Miller Puckette
 * 
 * This object uses the mouse button tracking mechanism from the
 * iem_gui/iem_event object.
 * 
 * Problem: the *_widgetbehavior.w_clickfn function pointer defines the 
 * *_click() function to be called when the mouse pointer is on the object's 
 * canvas. One of the arguments is the mouse clicked event. This is a one-time 
 * event, it does not report the button state.  
 * 
 * Once the button is clicked, the event is stored in the x->mouseDown flag and 
 * the *_motion() and *_key() methods are registered via glist_grab. This 
 * prevents the *_click() from being called until the button is released.
 * 
 * Fred Jan Kraan, fjkraan@xs4all.nl, 2018-07-29 */

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
    int        X;
    int        Y;
    int        prevX;  /* only to limit */
    int        prevY;  /*  movement logging */
    int        shift;
    int        alt;
    int        mouseClick;
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

void GUI_eraseme(t_GUI *x, t_glist *glist)
{
//    t_GUI *x = (t_GUI *)z;
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

// defined as function pointer via g_graph.c:glist_grab call in GUI_newclick
//  called when a key is pressed
static void GUI_key(t_GUI *x, t_floatarg f)
{
    printf("key: %d - %c\n", (int)f, (int)f );
}

// defined as function pointer via g_graph.c:glist_grab call in GUInewclick
//  called when the mouse is dragged
static void GUI_motion(t_GUI *x, t_floatarg dx, t_floatarg dy)
{
    x->X += (t_int)dx;
    x->Y -= (t_int)dy;
    
    printf("motion(%d); dragg_x: %d, dragg_y: %d\n", x->mouseClick, x->X, x->Y);
}

//  called via GUI_widgetbehavior.w_clickfn 
static int GUI_newclick(t_gobj *z, struct _glist *glist,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    if (dbl) { } // eliminate compiler warnings
    t_GUI *x = (t_GUI*)z;
    t_int xpos = text_xpix(&x->x_obj, glist);
    t_int ypos = text_ypix(&x->x_obj, glist);
    
    x->shift = shift;
    x->alt = alt ? 1 : 0;
    if(doit != x->mouseClick)
    {
//        printf("newclick; doit: %d, shift: %d, alt:\n", doit, shift, alt ? 1 : 0);
        x->mouseClick = doit;
    }
    x->X = xpix - xpos;
    x->Y = x->canvas_height - (ypix - ypos);
    if(doit)
    {
        // install mouse drag and key press tracking.
        glist_grab(x->x_glist, &x->x_obj.te_g, 
            (t_glistmotionfn) GUI_motion,
            (t_glistkeyfn) GUI_key, xpix, ypix);
            
        printf("newclick(%d); dragg_x: %d, dragg_y: %d\n", doit, x->X, x->Y);
    }
    else
    {
        printf("newclick(%d); move_x: %d, move_y: %d\n", doit, x->X, x->Y);
    }
    return (1);
}

static void GUI_bang(t_GUI *x)
{
    if (x) {} /* prevents compiler warnings */
    post("GUI4 banged");
}

static void GUI_status(t_GUI *x)
{
    post("  --==## %s status %d.%d.%d##==--", 
        "GUI5", MAJORVERSION, MINORVERSION, BUGFIXVERSION);
    post("canvas_width: %d", x->canvas_width);
    post("canvas_height %d", x->canvas_height);
    post("inletCount:   %d", x->inletCount);
    post("outletCount:  %d", x->outletCount);
    post("prevX:        %d", x->prevX);
    post("prevY:        %d", x->prevY);
    post("zoom:         %d", x->x_glist->gl_zoom);
    post("font size:    %d", x->x_glist->gl_font);
    post("patch name:   %s", x->x_glist->gl_name->s_name);
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
    
    x->canvas_width  = 50;
    x->canvas_height = 50;
    x->prevX         = 0;
    x->prevY         = 0;
    x->inletCount    = 1;
    x->outletCount   = 1;
    x->x_glist       = (t_glist *)canvas_getcurrent();
    x->shift         = 0;
    x->mouseClick    = 0;
    outlet_new(&x->x_obj, &s_float);
    
    return (x);
}

void GUI5_setup(void)
{
printf("----\nGUI5 setup \n");
    GUI_class = class_new(gensym("GUI5"), 
        (t_newmethod)GUI_new, 0, sizeof(t_GUI), 0, A_GIMME, 0);

// set graphic event behaviour
    GUI_setwidget();
// override the standard object graphic behaviour    
    class_setwidget(GUI_class, &GUI_widgetbehavior);
    class_addmethod(GUI_class, (t_method)GUI_bang, gensym("bang"), 0);
    class_addmethod(GUI_class, (t_method)GUI_status, gensym("status"), 0);
    class_addmethod(GUI_class, (t_method)GUI_zoom, gensym("zoom"),
        A_CANT, 0);
    
    post("GUI5 %d.%d.%d", MAJORVERSION, MINORVERSION, BUGFIXVERSION);
    post("fjkraan@xs4all.nl, 2016-12-04");
}

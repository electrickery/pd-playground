/* GUI2, skeleton GUI object, based on gcanvas from Guenter Geiger
 * Fred Jan Kraan, fjkraan@xs4all.nl, 2016-08-07 */

#include <math.h>
#include "m_pd.h"
#include "g_canvas.h"
#include "s_stuff.h"

/* ------------------------ GUI ----------------------------- */

#define MAJORVERSION 0
#define MINORVERSION 1
#define BUGFIXVERSION 0


static t_class *GUI_class;

typedef struct _GUI
{
    t_object   x_obj;
    t_glist *  x_glist;
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
    int zoom = 1;
    int width  = x->canvas_width;
    int height = x->canvas_height;
    int xp1 = x->x_obj.te_xpix;
    int yp1 = x->x_obj.te_ypix;
    int xp2 = x->x_obj.te_xpix + width * zoom;
    int yp2 = x->x_obj.te_ypix + height * zoom;
        
    if (firsttime) {
        /* main rectangle */
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lx%d\n",
             glist_getcanvas(glist), 
             xp1, yp1, 
             xp2, yp2, 
             x, 0);
printf(".x%lx.c create rectangle %d %d %d %d -tags %lx%d\n", 
    (long int)glist_getcanvas(glist), xp1, yp1, xp2, yp2, (long int)x, 0);    
    } else {
        sys_vgui(".x%lx.c coords %lx%d %d %d %d %d\n",
             glist_getcanvas(glist), x, 0, 
             xp1, yp1, 
             xp2, yp2);
printf(".x%lx.c coords %lx%d %d %d %d %d\n", 
    (long int)glist_getcanvas(glist), (long int)x, 0, xp1, yp1, xp2, yp2);
        canvas_fixlinesfor(glist,(t_text*) x);
    }
	int n;
	int nplus, i, onset;
    /* IOHEIGHT 1 is Pd-vanilla mode, IOHEIGHT 2 is Pd-extendend mode */
    /* No provision for message/signal difference yet */
    #define IOHEIGHT 2

    /* inlets */
	n = x->inletCount;
	nplus = (n == 1 ? 1 : n-1);
	for (i = 0; i < n; i++)
	{
	    onset = xp1 + (width - IOWIDTH) * i / nplus;
        if (firsttime)
		    sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxi%d\n",
			    glist_getcanvas(glist),
			    onset,           yp1,
			    onset + IOWIDTH, yp1 + IOHEIGHT,
			    x, i);
        else
		    sys_vgui(".x%lx.c coords %lxi%d %d %d %d %d\n",
			    glist_getcanvas(glist), x, i,
                onset,           yp1, 
                onset + IOWIDTH, yp1 + IOHEIGHT);
    }
    printf(" created/moved %d inlets ", n);
      
    /* outlets */
	n = x->outletCount;
	nplus = (n == 1 ? 1 : n-1);
	for (i = 0; i < n; i++)
	{
	    onset = xp1 + (width - IOWIDTH) * i / nplus;
	    if (firsttime)
		    sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxo%d\n",
			    glist_getcanvas(glist),
			    onset,           yp2 - IOHEIGHT,
			    onset + IOWIDTH, yp2,
			    x, i);
        else
		    sys_vgui(".x%lx.c coords %lxo%d %d %d %d %d\n",
			    glist_getcanvas(glist), x, i,
			    onset,           yp2 - IOHEIGHT,
			    onset + IOWIDTH, yp2);
	  }
      printf(" and %d outlets\n", n);
}

/* ---------------- widget behaviour functions -------------------- */

static void GUI_getrect(t_gobj *z, t_glist *owner,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_GUI* x = (t_GUI*)z;
    int zoom = 1;

    int width  = x->canvas_width;
    int height = x->canvas_height;
    *xp1 = x->x_obj.te_xpix;
    *yp1 = x->x_obj.te_ypix;
    *xp2 = x->x_obj.te_xpix + width * zoom;
    *yp2 = x->x_obj.te_ypix + height * zoom;
    
    /* debug section */
    if (x->x_obj.te_xpix != x->prevX || x->x_obj.te_ypix != x->prevY) {
printf("getrect: xp1: %d, yp1: %d, xp2: %d, yp2: %d\n", *xp1, *yp1, *xp2, *yp2);
        x->prevX = x->x_obj.te_xpix;
        x->prevY = x->x_obj.te_ypix;
    }
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
    sys_vgui(".x%lx.c itemconfigure %lx%d -outline %s\n", 
        glist_getcanvas(glist), x, 0, color);
printf(".x%lx.c itemconfigure %lx%d -outline %s\n", 
    (long int)glist_getcanvas(glist), (long int)x, 0, color);

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
    sys_vgui(".x%lx.c delete %lx%d\n", glist_getcanvas(glist), x, 0);
printf(".x%lx.c delete %lx%d\n", 
    (long int)glist_getcanvas(glist), (long int)x, 0);

    /* inlets */
    int i;
    int n = x->inletCount;
    for (i = 0; i < n; i++) {
        sys_vgui(".x%lx.c delete %lxi%d\n", glist_getcanvas(glist), x, 0);
    }
printf(" deleted %d inlets ", n);
    /* outlets */
    n = x->outletCount;
    for (i = 0; i < n; i++) {
        sys_vgui(".x%lx.c delete %lxo%d\n", glist_getcanvas(glist), x, 0);
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
    if (z) { } // eliminate compiler warnings
    return (1);
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
printf("----\nGUI2 new \n");

    t_GUI *x = (t_GUI *)pd_new(GUI_class);
    
    x->canvas_width  = 20;
    x->canvas_height = 20;
    x->prevX = 0;
    x->prevY = 0;
    x->inletCount = 1;
    x->outletCount = 1;
    
    outlet_new(&x->x_obj, &s_float);
    
    return (x);
}

void GUI2_setup(void)
{
printf("----\nGUI2 setup \n");
    GUI_class = class_new(gensym("GUI2"), 
        (t_newmethod)GUI_new, 0, sizeof(t_GUI), 0, A_GIMME, 0);

// GUI behaviour
    GUI_setwidget();
    class_setwidget(GUI_class, &GUI_widgetbehavior);
    
    post("GUI2 %d.%d.%d", MAJORVERSION, MINORVERSION, BUGFIXVERSION);
    post("fjkraan@xs4all.nl, 2016-08-07");
}

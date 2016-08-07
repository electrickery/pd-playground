/* GUI1, skeleton GUI object, based on gcanvas from Guenter Geiger
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
    int        prevX;
    int        prevY;
} t_GUI;


static void GUI_getrect(t_gobj *z, t_glist *owner,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    int width, height;
    t_GUI* x = (t_GUI*)z;

    width  = x->canvas_width;
    height = x->canvas_height;
    *xp1 = x->x_obj.te_xpix;
    *yp1 = x->x_obj.te_ypix;
    *xp2 = x->x_obj.te_xpix + width;
    *yp2 = x->x_obj.te_ypix + height;
    
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
//    GUI_drawme(x, glist, 0);
    canvas_fixlinesfor(glist,(t_text*) x);
}

// canvas is selected in edit mode
static void GUI_select(t_gobj *z, t_glist *glist, int state)
{
printf("select: %d\n", state);
    t_GUI *x = (t_GUI *)z;
}

static void GUI_activate(t_gobj *z, t_glist *glist, int state)
{
printf("activate: %d", state);

    if (z || glist || state) {} // eliminate compiler warnings
}

static void GUI_delete(t_gobj *z, t_glist *glist)
{
    t_text *x = (t_text *)z;
//    canvas_deletelinesfor(glist, x);
}

#define FRAME 0       
static void GUI_vis(t_gobj *z, t_glist *glist, int vis)
{
printf("vis: %d\n", vis);
    t_GUI* x = (t_GUI*)z;

    if (vis)
    {
//        GUI_drawme(x, glist, 1);
    }
    else
    {
//        GUI_drawme(x, glist, 0);
//        GUI_draw_foreground(x);
    }
}

static int GUI_newclick(t_gobj *z, struct _glist *glist,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
printf("newclick: %d\n", doit);  
    return (1);
}

t_widgetbehavior   GUI_widgetbehavior;

// when is what called (also see g_canvas.h):
// GUI_getrect  - mouse cursor inside patch window
// GUI_displace - move canvas in edit mode ?
// GUI_select   - GUI object is selected in edit mode
// GUI_activate - GUI object is moved in edit mode
// GUI_delete   - GUI object is removed ?
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
printf("----\nGUI1 new \n");

    t_GUI *x = (t_GUI *)pd_new(GUI_class);
    
    x->canvas_width  = 10;
    x->canvas_height = 10;
    x->prevX = 0;
    x->prevY = 0;
    
    return (x);
}

void GUI1_setup(void)
{
printf("----\nGUI1 setup \n");
    GUI_class = class_new(gensym("GUI1"), 
        (t_newmethod)GUI_new, 0, sizeof(t_GUI), 0, A_GIMME, 0);

// GUI behaviour
    GUI_setwidget();
    class_setwidget(GUI_class, &GUI_widgetbehavior);
    
    post("GUI %d.%d.%d , ", MAJORVERSION, MINORVERSION, BUGFIXVERSION);
    post("fjkraan@xs4all.nl, 2016-08-07");
}

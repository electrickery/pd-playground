/* GUI3, skeleton GUI object, based on gcanvas from Guenter Geiger
 * Fred Jan Kraan, fjkraan@xs4all.nl, 2016-08-07 */

#include <math.h>
#include "m_pd.h"
#include "g_canvas.h"
#include "s_stuff.h"

/* ------------------------ GUI ----------------------------- */

#define MAJOR_VERSION 0
#define MINOR_VERSION 1
#define BUGFIX_VERSION 0


static t_class *GUI_class;

typedef struct _GUI
{
    t_object   x_obj;
    t_glist *  x_glist;
    int        canvas_width;
    int        canvas_height;
    int        prevX;  /* only to limit */
    int        prevY;  /*  movement logging */
    int        fontSize;
    t_symbol*  fontName;
    t_symbol*  fontColorName;
    t_symbol*  commentText;
    
} t_GUI;

/* -------------------- setting struct values -------------------- */

void GUI_fontcolorname(t_GUI *x, t_symbol *fontColorName)
{
    x->fontColorName = fontColorName;
//        GUI_drawme(x, x->x_glist, 0);
}

void GUI_fontcolor(t_GUI *x, t_float red, t_float green, t_float blue)
{
    
}

void GUI_fontsize(t_GUI *x, t_float size)
{
    if (size > 0) 
    {
        x->fontSize = (int)size;
//        GUI_drawme(x, x->x_glist, 0);
    }
}

void GUI_status(t_GUI *x)
{
    post("--==## GUI3 %d.%d.%d status ##==--", MAJOR_VERSION, MINOR_VERSION, BUGFIX_VERSION);
    post("fontSize:      %d", x->fontSize);
    post("fontName:      %s", x->fontName->s_name);
    post("fontColorName: %s", x->fontColorName->s_name);
    post("commentText:   %s", x->commentText->s_name);
    post("compiled against Pd version:  %d.%d.%d",  
        PD_MAJOR_VERSION, PD_MINOR_VERSION, PD_BUGFIX_VERSION);
    int major, minor, bugfix;
    sys_getversion(&major, &minor, &bugfix);
    post("this Pd version: %d.%d.%d", major, minor, bugfix);    
}

void GUI_commenttext(t_GUI *x, t_symbol *comment)
{
    x->commentText = comment;
//    GUI_drawme(x, x->x_glist, 0);
}

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
        sys_vgui("pdtk_text_new .x%lx.c {.x%lx.%lx obj text} %f %f {%s} %d %s\n", 
                          glist_getcanvas(glist),
                                   glist_getcanvas(glist),
                                       x, (t_float)xp1, (t_float)yp1,
                                                          x->commentText->s_name,
                                                              x->fontSize, x->fontColorName->s_name);
                                       
printf("pdtk_text_new .x%lx.c {.x%lx.%lx obj text} %f %f {%s} %d %s\n", 
                          glist_getcanvas(glist),
                                 glist_getcanvas(glist),
                                     x, (t_float)xp1, (t_float)yp1,
                                                          x->commentText->s_name,
                                                              x->fontSize, x->fontColorName->s_name);

    } else {
        sys_vgui(".x%lx.c coords .x%lx.%lx %d %d\n",
             glist_getcanvas(glist), glist_getcanvas(glist), x, 
             xp1, yp1);
printf(".x%lx.c coords .x%lx.%lx %d %d\n",
             glist_getcanvas(glist), glist_getcanvas(glist), x, 
             xp1, yp1);
        canvas_fixlinesfor(glist,(t_text*) x);
    }
//printf("text width: %d\n", rtext_width(x));
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
    
    sys_vgui(".x%lx.c delete .x%lx.%lx\n",
             glist_getcanvas(glist), glist_getcanvas(glist), x);
printf(".x%lx.c delete .x%lx.%lx\n",
             (long int)glist_getcanvas(glist), (long int)glist_getcanvas(glist), (long int)x);
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
printf("----\nGUI3 new \n");

    t_GUI *x = (t_GUI *)pd_new(GUI_class);
    
    x->canvas_width  = 30;
    x->canvas_height = 20;
    x->fontSize      = 10;
    x->fontName      = gensym("");
    x->fontColorName = gensym("black");
    x->commentText   = gensym("comment");
    
//    outlet_new(&x->x_obj, &s_float);
    
    return (x);
}

void GUI3_setup(void)
{
printf("----\nGUI3 setup \n");
    GUI_class = class_new(gensym("GUI3"), 
        (t_newmethod)GUI_new, 0, sizeof(t_GUI), 0, A_GIMME, 0);

// set graphic event behaviour
    GUI_setwidget();
// override the standard object graphic behaviour    
    class_setwidget(GUI_class, &GUI_widgetbehavior);
    class_addmethod(GUI_class, (t_method)GUI_status,
		    gensym("status"), 0);
    class_addmethod(GUI_class, (t_method)GUI_fontsize,
		    gensym("fontsize"), A_FLOAT, 0);
    class_addmethod(GUI_class, (t_method)GUI_fontcolor,
		    gensym("fontcolor"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(GUI_class, (t_method)GUI_fontcolorname,
		    gensym("fontcolorname"), A_SYMBOL, 0);
    class_addmethod(GUI_class, (t_method)GUI_commenttext,
		    gensym("text"), A_SYMBOL, 0);    
    post("GUI3 %d.%d.%d", MAJOR_VERSION, MINOR_VERSION, BUGFIX_VERSION);
    post("fjkraan@xs4all.nl, 2016-08-07");
}

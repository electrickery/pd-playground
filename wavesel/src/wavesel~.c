/* wavesel, cheap waveform knock-off, based on gcanvas from Guenter Geiger
 * Fred Jan Kraan, fjkraan@xs4all.nl, 2015-06 - 2015-07 */

#include <math.h>
#include "m_pd.h"
#include "g_canvas.h"
#include "s_stuff.h"
#include "shared.h"
//#include "common/loud.h"
#include "hammer/gui.h"  // just for the mouse-up, silly!
#include "unstable/forky.h"
#include "wavesel~.h"

/* ------------------------ wavesel ----------------------------- */

static void wavesel_setmode(t_wavesel *x, t_float mode)
{
    if (mode < 0 || mode > 4) return;
    x->mode = (int)mode;
    if (x->mode == 4)
        post("wavesel: mode %d not implemented yet", x->mode);
}

static t_canvas *wavesel_getcanvas(t_wavesel *x, t_glist *glist)
{
    if (glist != x->x_glist)
    {
	post("wavesel_getcanvas: glist error");
	x->x_glist = glist;
    }
    return (x->canvas = glist_getcanvas(glist));
}

static t_canvas *wavesel_isvisible(t_wavesel *x)
{
    return (glist_isvisible(x->x_glist) ? x->canvas : 0);
}

static void wavesel_revis(t_wavesel *x)
{
post("revis: drawme 0");
    sys_vgui(".x%lx.c delete %s\n", x->canvas, x->canvas_allTag);

        wavesel_drawme(x, x->x_glist, 0);
        if (x->array_name)
            wavesel_setarray(x, x->array_name);
}

static void wavesel_create_rectangle(void* cv, void* o, int c, int x, 
    int y, int w, int h, char* color, char* outlinecolor) 
{
    sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lx%d -fill %s -outline %s\n",
                cv,                    x, y, x+w,y+h,    o, c,   color, outlinecolor);
}

static void wavesel_move_object(void* cv, void* x, int c, int xx, int y,
    int w,int h) {
    sys_vgui(".x%lx.c coords %lx%d %d %d %d %d\n",
                cv,          x, c, xx, y, xx+w,y+h);
    sys_pollgui();
}

static void wavesel_color_object(void* cv, void* o, int c, char* color) 
{
     sys_vgui(".x%lx.c itemconfigure %lx%d -fill %s\n", cv, 
	     o, c, color);
}

static void wavesel_color_rect_object(void* cv, void* o, int c, char* color,
    char* outlinecolor) 
{
     sys_vgui(".x%lx.c itemconfigure %lx%d -fill %s -outline %s\n", cv, 
	     o, c, color, outlinecolor);
}

static void wavesel_delete_object(void* cv,void* o, int c) {
     sys_vgui(".x%lx.c delete %lx%d\n",
	      cv, o,c);
}

static void wavesel_create_column(void* cv, void* o, int c, int x, int y, 
    int w, int h, char* color) {
     sys_vgui(".x%lx.c create line %d %d %d %d -tags %lx%d -fill %s\n",
     cv, x, y, x+w, y+h, o, c, color);
}

static void wavesel_draw_element(t_wavesel *x, int num)
{
    t_element* e = x->canvas_element[num];
    if (!e) post("wavesel_draw_element assertion failed");
    switch (e->type) {
    case RECT:
        wavesel_create_rectangle(glist_getcanvas(x->x_glist), x, num,
            x->x_obj.te_xpix + e->x, x->x_obj.te_ypix + e->y,
            e->w, e->h, e->color, e->outlinecolor);
        break;
    case LINE:
        wavesel_create_column(glist_getcanvas(x->x_glist), x, num,
             x->x_obj.te_xpix + e->x, x->x_obj.te_ypix + e->y,
             e->w, e->h, e->color);                            
            break;
    default:
        post("wavesel: unknown element");
    }
}

static void wavesel_move_element(t_wavesel *x, int num)
{
    t_element* e = x->canvas_element[num];
    wavesel_move_object(
        glist_getcanvas(x->x_glist), x, num,
        x->x_obj.te_xpix + e->x, x->x_obj.te_ypix + e->y,
        e->w, e->h);
}

static void wavesel_set_canvas_list(t_wavesel *x)
{
    SETFLOAT(&x->outlet_canvas[CANVAS_LIST_VIEW_START],   (t_float)x->canvas_startForeground);
    SETFLOAT(&x->outlet_canvas[CANVAS_LIST_VIEW_END],     (t_float)x->canvas_endForeground);
    SETFLOAT(&x->outlet_canvas[CANVAS_LIST_SELECT_START], (t_float)x->canvas_startcursor);
    SETFLOAT(&x->outlet_canvas[CANVAS_LIST_SELECT_END],   (t_float)x->canvas_endcursor);
}

static void wavesel_set_mouse_list(t_wavesel *x)
{
    SETFLOAT(&x->outlet_mouse[MOUSE_LIST_X],   (t_float)x->canvas_x);
    SETFLOAT(&x->outlet_mouse[MOUSE_LIST_Y],   (t_float)x->canvas_y);
//    SETFLOAT(&x->outlet_mouse[MOUSE_STATE],   0.f);
}
static void wavesel_move_background_selection(t_wavesel *x, int xx, 
    int w)
{
    t_element* e = x->canvas_element[x->canvas_selectBackground];
    e->x = xx;
    e->w = w;
    wavesel_move_object(glist_getcanvas(x->x_glist), x, x->canvas_selectBackground,
        x->x_obj.te_xpix + e->x, x->x_obj.te_ypix + e->y,
        e->w, e->h);
}


static void wavesel_delete_element(t_wavesel *x, int num)
{
    wavesel_delete_object(glist_getcanvas(x->x_glist),x,num); 
}


static void wavesel_color_element(t_wavesel* x, int num, char* color)
{
    t_element* e = x->canvas_element[num];
    e->color = color;
    wavesel_color_object(glist_getcanvas(x->x_glist), x, num, color); 
}


static void wavesel_color_rect_element(t_wavesel* x, int num, char* color,
    char* outlinecolor)
{
    t_element* e = x->canvas_element[num];
    e->color = color;
    e->outlinecolor = outlinecolor;
    wavesel_color_rect_object(glist_getcanvas(x->x_glist), x, num, color, outlinecolor); 
}

/* widget helper functions */

void wavesel_drawme(t_wavesel *x, t_glist *glist, int firsttime)
{
post("drawme: %d (%d)", firsttime, x->canvas_numelem);
    int i;
    
    // rectangle and contents
    for (i = 0; i < x->canvas_numelem; i++)
    {
        if (firsttime)
            wavesel_draw_element(x, i);
        else
            wavesel_move_element(x, i);
    }
        
    /* outlets  "-tags %lxo%d" */
    int n = 6;
    int nplus;
    nplus = (n == 1 ? 1 : n-1);
    for (i = 0; i < n; i++)
    {
        int onset = x->x_obj.te_xpix + 
            (x->canvas_width - IOWIDTH) * i / nplus;
        if (firsttime) 
        {
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxo%d\n",
                glist_getcanvas(glist),
                onset, x->x_obj.te_ypix + x->canvas_height - WAVESEL_XLETHEIGHT,
                onset + IOWIDTH, x->x_obj.te_ypix + x->canvas_height,
                x, i);
        }
        else
        {
            sys_vgui(".x%lx.c coords %lxo%d %d %d %d %d\n",
                 glist_getcanvas(glist), x, i,
                 onset, x->x_obj.te_ypix + x->canvas_height - WAVESEL_XLETHEIGHT,
                 onset + IOWIDTH, x->x_obj.te_ypix + x->canvas_height);
        }
    }
    /* inlets "-tags %lxi%d" */
    n = 1; 
    nplus = (n == 1 ? 1 : n-1);
    for (i = 0; i < n; i++)
    {
        int onset = x->x_obj.te_xpix + 
            (x->canvas_width - IOWIDTH) * i / nplus;
        if (firsttime)
        {
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxi%d\n",
                glist_getcanvas(glist),
                onset, x->x_obj.te_ypix,
                onset + IOWIDTH, x->x_obj.te_ypix + WAVESEL_XLETHEIGHT,
                x, i);
        }
        else
        {
                sys_vgui(".x%lx.c coords %lxi%d %d %d %d %d\n",
                    glist_getcanvas(glist), x, i,
                    onset, x->x_obj.te_ypix,
                    onset + IOWIDTH, x->x_obj.te_ypix + WAVESEL_XLETHEIGHT);
        }
    }
}

// called when an object is destroyed
void wavesel_erase(t_wavesel* x, t_glist* glist)
{
post("erase");
    int n, i;

    if (x->mouseBound)
    {
        hammergui_unbindmouse((t_pd *)x);
        x->mouseBound = 0;
    }
    // rectangle and contents
    for (i = 0; i < x->canvas_numelem; i++)
         wavesel_delete_element(x, i);

    // inlets %lxi%d
    n = 1;
    while (n--) {
        sys_vgui(".x%lx.c delete %lxi%d\n", 
            glist_getcanvas(glist), x, n);
    }
     
    // outlets %lxo%d
    n = 6;
    while (n--) {
        sys_vgui(".x%lx.c delete %lxo%d\n", 
            glist_getcanvas(glist), x, n);
    }
    x->canvas_numelem = 0;
    if (x->system_clockRunning)
         clock_free(x->system_clock);
}


/* ------------------ wavesel widgetbehaviour----------------------- */


static void wavesel_getrect(t_gobj *z, t_glist *owner,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    int width, height;
    t_wavesel* s = (t_wavesel*)z;

    width = s->canvas_width;
    height = s->canvas_height;
    *xp1 = s->x_obj.te_xpix;
    *yp1 = s->x_obj.te_ypix;
    *xp2 = s->x_obj.te_xpix + width;
    *yp2 = s->x_obj.te_ypix + height;
} 

static void wavesel_displace(t_gobj *z, t_glist *glist,
    int dx, int dy)
{
post("displace: drawme 0");
    t_wavesel *x = (t_wavesel *)z;
    x->x_obj.te_xpix += dx;
    x->x_obj.te_ypix += dy;
    wavesel_drawme(x, glist, 0);
    canvas_fixlinesfor(glist,(t_text*) x);
}

static void wavesel_redraw_all(t_wavesel* x)
{
post("redraw: initbase, wavesel_drawme, wavesel_draw_foreground, wavesel_draw_columns");
//        wavesel_erase(x, x->x_glist);

    wavesel_initbase(x);
    wavesel_drawme(x, x->x_glist, 1);
    wavesel_draw_foreground(x);
    wavesel_draw_columns(x);
}

static void wavesel_deselect(t_wavesel* x, t_canvas *cv, 
    t_waveselhandle *wsh)
{
post("deselect");
    sys_vgui(".x%lx.c itemconfigure %s -outline black -width %f\
        -fill #%2.2x000000\n", cv, x->canvas_backgroundTag, WAVESEL_GRIDWIDTH);
    sys_vgui("destroy %s\n", wsh->h_pathname);
    
//    wavesel_initbase(x);
//    wavesel_drawme(x, x->x_glist, 1);
//    wavesel_draw_columns(x);
//    wavesel_draw_foreground(x);
    wavesel_vis(x, x->x_glist, 1);
    
    wsh->h_selectedmode = 0;  
}

// canvas is selected in edit mode
static void wavesel_select(t_gobj *z, t_glist *glist, int state)
{
    t_wavesel *x = (t_wavesel *)z;
    wavesel_reset_selection(x);
     
// handle start
    t_canvas *cv = wavesel_getcanvas(x, glist);
    t_waveselhandle *wsh = (t_waveselhandle *)x->editHandle;
    if (state)
    {
post("select: selected");
        if (wsh->h_selectedmode) // prevent re-selection
            return;

        int x1, y1, x2, y2;
        wavesel_getrect(z, glist, &x1, &y1, &x2, &y2);

        sys_vgui(".x%lx.c itemconfigure %s -outline blue -width %f -fill %s\n",
            cv, x->canvas_backgroundTag, WAVESEL_SELBDWIDTH, WAVESEL_SELCOLOR);
        sys_vgui("canvas %s -width %d -height %d -bg #fedc00 -bd 0\n",
            wsh->h_pathname, WAVESELHANDLE_WIDTH, WAVESELHANDLE_HEIGHT);
            sys_vgui(".x%lx.c create window %f %f -anchor nw\
            -width %d -height %d -window %s -tags %s\n",
             cv, x2 - (WAVESELHANDLE_WIDTH - WAVESEL_SELBDWIDTH),
             y2 - (WAVESELHANDLE_HEIGHT - WAVESEL_SELBDWIDTH),
            WAVESELHANDLE_WIDTH, WAVESELHANDLE_HEIGHT,
            wsh->h_pathname, x->canvas_allTag);
        sys_vgui("bind %s <Button> {pdsend [concat %s _click 1 \\;]}\n",
            wsh->h_pathname, wsh->h_bindsym->s_name);
        sys_vgui("bind %s <ButtonRelease> {pdsend [concat %s _click 0 \\;]}\n",
            wsh->h_pathname, wsh->h_bindsym->s_name);
        sys_vgui("bind %s <Motion> {pdsend [concat %s _motion %%x %%y \\;]}\n",
            wsh->h_pathname, wsh->h_bindsym->s_name);
        wsh->h_selectedmode = 1;
    }
    else
    {
post("select: deselected");        
        wavesel_deselect(x, cv, wsh);

        wavesel_erase(x, glist);
        wavesel_initbase(x);
        wavesel_draw_foreground(x);
        wavesel_drawme(x, glist, 1);
        wavesel_vis(x, x->x_glist, 0);
         
    }
// handle end
}


static void wavesel_delete(t_gobj *z, t_glist *glist)
{
    t_text *x = (t_text *)z;
    canvas_deletelinesfor(glist, x);
}

#define FRAME 0       
static void wavesel_vis(t_gobj *z, t_glist *glist, int vis)
{
post("vis: %d", vis);
    t_wavesel* x = (t_wavesel*)z;
    t_canvas *cv = wavesel_getcanvas(x, glist);
    t_waveselhandle *wsh = (t_waveselhandle *)x->editHandle;
    
    if (wsh->h_selectedmode)
        return;
        
    if (!x->canvas_numelem)
        return;
    
    sprintf(wsh->h_pathname, ".x%lx.h%lx", (unsigned long)cv, 
        (unsigned long)wsh);
    if (vis)
    {
        x->canvas_element[FRAME]->h = x->canvas_height;
        x->canvas_element[FRAME]->w = x->canvas_width;
        wavesel_drawme(x, glist, 1);
        if (x->array_name)
printf("about to call wavesel_setarray(x, x->array_name)\n");
            wavesel_setarray(x, x->array_name);
    }
    else
    {
        wavesel_drawme(x, glist, 0);
        wavesel_draw_foreground(x);
    }
}

t_widgetbehavior   wavesel_widgetbehavior;

static void wavesel_color_columns(t_wavesel *x, int _s, int _e, 
    int select)
{
    int start;
    int end;
    int i;
    if (_s < _e)
    {
        start = _s;
        end = _e;
    }
    else
    {
        start = _e;
        end = _s;
    }
    for (i = start; i <= end; i++)
    {
        if (select)
             wavesel_color_element(x, i + x->canvas_startForeground, 
                 x->canvas_foregroundSelectedColor->s_name);
        else
             wavesel_color_element(x, i + x->canvas_startForeground, 
                 x->canvas_foregroundColor->s_name);
    }
    // background selected rectangle
    wavesel_color_rect_element(x, x->canvas_selectBackground, x->canvas_backgroundSelectedColor->s_name,
        x->canvas_backgroundSelectedColor->s_name);
    wavesel_move_background_selection(x, x->canvas_startcursor, 
        x->canvas_endcursor - x->canvas_startcursor);
}

static void wavesel_color_all_columns(t_wavesel *x)
{
    int width = x->canvas_endForeground - x->canvas_startForeground;
    if (x->canvas_startcursor < 0 || x->canvas_endcursor > width)
    {
        post("!!!x->canvas_startcursor: %d, x->canvas_endcursor: %d",
            x->canvas_startcursor, x->canvas_endcursor);
            return;
    }
    if (x->canvas_endcursor == 0)
        wavesel_reset_selection(x);
    else {
        wavesel_color_columns(x, 0, x->canvas_startcursor, 0);
        wavesel_color_columns(x, x->canvas_startcursor, x->canvas_endcursor, 1);
        wavesel_color_columns(x, x->canvas_endcursor, width, 0);
    }
}

static void wavesel_updateSelectRange(t_wavesel *x)
{
    int width = x->canvas_endForeground - x->canvas_startForeground;
    if (x->canvas_arrayViewStart > x->array_endSelectSample || x->canvas_arrayViewEnd < x->array_startSelectSample)
    { // selected range out of visible range
        wavesel_reset_selection(x);
    }
    else
    { // selection whole or partially visible
        x->canvas_startcursor = (x->canvas_arrayViewStart < x->array_startSelectSample) ?
            (int)(x->array_startSelectSample - x->canvas_arrayViewStart) / x->canvas_samplesPerColumn : 0;
            
        x->canvas_endcursor = (x->canvas_arrayViewEnd > x->array_endSelectSample) ?
            (int)((t_float)(x->array_endSelectSample - x->canvas_arrayViewStart) / x->canvas_samplesPerColumn) : width;
        x->canvas_endcursor = (x->canvas_endcursor > width) ? width : x->canvas_endcursor;
//post("canvas_startcursor: %d, canvas_endcursor: %d", x->canvas_startcursor, x->canvas_endcursor);
        wavesel_color_all_columns(x);
    }
}

static void wavesel_motion_selectMode(t_wavesel *x, t_floatarg dx)
{   
    int width = x->canvas_endForeground - x->canvas_startForeground;
    t_float sqrt_dx = (dx > 0) ? sqrt(dx) : -sqrt(-dx); 
    x->canvas_x += sqrt_dx;
    x->canvas_x = ((int)(x->canvas_x)  < 0) ? x->canvas_startcursor : x->canvas_x;
    x->canvas_x = ((int)(x->canvas_x + 0.5f) > width) ? width : x->canvas_x;

    if (x->canvas_x > x->canvas_clickOrigin) // right side
    {
        wavesel_color_columns(x, x->canvas_startcursor, x->canvas_clickOrigin, 0);
        wavesel_color_columns(x, x->canvas_clickOrigin, (int)(x->canvas_x), 1);
        wavesel_color_columns(x, (int)(x->canvas_x), x->canvas_endcursor, 0);
        x->canvas_startcursor = x->canvas_clickOrigin;
        x->canvas_endcursor   = (int)(x->canvas_x);
        // update cursor outlet
//        outlet_float(x->out4, x->canvas_x * x->outlet_outputFactor);
    }
    else //left side
    { 
        wavesel_color_columns(x, x->canvas_clickOrigin, x->canvas_endcursor, 0);
        wavesel_color_columns(x, x->canvas_x, x->canvas_clickOrigin, 1);
        wavesel_color_columns(x, x->canvas_startcursor, x->canvas_x, 0);
        x->canvas_startcursor = (int)(x->canvas_x);
        x->canvas_endcursor   = x->canvas_clickOrigin;
        // update cursor outlet
//        outlet_float(x->out3, x->canvas_x * x->outlet_outputFactor);
    }  
    x->array_startSelectSample = 
        (int)x->canvas_startcursor * x->canvas_samplesPerColumn + 
        x->canvas_arrayViewStart;
    x->array_endSelectSample   = 
        (int)x->canvas_endcursor * x->canvas_samplesPerColumn + 
        x->canvas_arrayViewStart;
        
    // update cursor outlets
    outlet_float(x->out3, (x->array_startSelectSample / x->system_ksr));
    outlet_float(x->out4, (x->array_endSelectSample   / x->system_ksr));
}

static void wavesel_motion_loopMode(t_wavesel *x, t_floatarg dx, 
    t_floatarg dy)
{
    int i;
    int width = x->canvas_endForeground - x->canvas_startForeground;
    t_float sqrt_dx = (dx > 0) ? sqrt(dx) : -sqrt(-dx); 
    x->canvas_x += sqrt_dx;
    int dix = (int)sqrt_dx;
    int diy = (int)dy;
    int dstart = dix + diy;
    int dend   = dix - diy;
    int oldStartCursor = x->canvas_startcursor;
    int oldEndCursor = x->canvas_endcursor;
    x->canvas_startcursor += dstart;
    x->canvas_endcursor   += dend;

    // check boundaries and internal consistency    
    if (x->canvas_startcursor > x->canvas_endcursor) {
        int tmp = (int)((x->canvas_startcursor + x->canvas_endcursor) / 2.f);
        x->canvas_startcursor = tmp;
        x->canvas_endcursor   = tmp;
    }
    
    // boundary checks & corrections
    x->canvas_startcursor = ((int)(x->canvas_startcursor)  < 0) ? 0: x->canvas_startcursor;
    x->canvas_startcursor = ((int)(x->canvas_startcursor + 0.f) > width) ? width : x->canvas_startcursor;
    x->canvas_endcursor = ((int)(x->canvas_endcursor)  < 0) ? 0 : x->canvas_endcursor;
    x->canvas_endcursor = ((int)(x->canvas_endcursor + 0.5f) > width) ? width : x->canvas_endcursor;

    // minimize the range to update
    int startUpdateRange = (x->canvas_startcursor > oldStartCursor) ? 
        oldStartCursor : x->canvas_startcursor;
    int endUpdateRange   = (x->canvas_endcursor > oldEndCursor) ? 
        x->canvas_endcursor : oldEndCursor;
 
    for (i = startUpdateRange; i <= endUpdateRange; i++)
    {
        if (i < x->canvas_startcursor || i > x->canvas_endcursor)
            wavesel_color_element(x, i + x->canvas_startForeground, 
                x->canvas_foregroundColor->s_name);
        else
            wavesel_color_element(x, i + x->canvas_startForeground, 
                x->canvas_foregroundSelectedColor->s_name);
    }
    wavesel_move_background_selection(x, x->canvas_startcursor, 
        x->canvas_endcursor - x->canvas_startcursor);
        
    x->array_startSelectSample = 
        (int)x->canvas_startcursor * x->canvas_samplesPerColumn + 
        x->canvas_arrayViewStart;
    x->array_endSelectSample   = 
        (int)x->canvas_endcursor * x->canvas_samplesPerColumn + 
        x->canvas_arrayViewStart;

    // update cursor outlets
    outlet_float(x->out3, (x->array_startSelectSample / x->system_ksr));
    outlet_float(x->out4, (x->array_endSelectSample   / x->system_ksr));
}

static void wavesel_motion_zoom_moveMode(t_wavesel *x, t_float dx, t_float dy)
{
    int canvasWidth = x->canvas_endForeground - x->canvas_startForeground;
    t_float leftSideFactor  = ((t_float)x->canvas_clickOrigin / (t_float)canvasWidth);
    t_float rightSideFactor = ((t_float)(canvasWidth - x->canvas_clickOrigin) / (t_float)canvasWidth);

    int range = x->canvas_arrayViewEnd - x->canvas_arrayViewStart;
    int tmp;
    
    if (x->canvas_samplesPerColumn < MAXZOOM && dy < 0)
        return;
    
    // zoom
    x->canvas_arrayViewStart -= (t_float)range * dy * ZOOMFACTOR * leftSideFactor;
    x->canvas_arrayViewEnd   += (t_float)range * dy * ZOOMFACTOR * rightSideFactor;
    if (x->canvas_arrayViewStart > x->canvas_arrayViewEnd)
    {
        tmp = x->canvas_arrayViewStart;
        x->canvas_arrayViewStart = x->canvas_arrayViewEnd;
        x->canvas_arrayViewEnd = tmp;
    }
    // move
    // prevent asymetric move at a border, which results in a zoom.
    if (x->canvas_arrayViewStart <= 0 && dx > 0.f)
        dx = 0;
    if (x->canvas_arrayViewEnd >= x->array_size && dx < 0.f)
        dx = 0;
    if (x->canvas_arrayViewStart >= 0 && x->canvas_arrayViewEnd <= x->array_size)
    {
        x->canvas_arrayViewStart -= (int)((t_float)range * dx * ZOOMFACTOR + 0.5f);
        x->canvas_arrayViewEnd   -= (int)((t_float)range * dx * ZOOMFACTOR + 0.5f);
    }
    // boundary checks & corrections
    x->canvas_arrayViewStart = (x->canvas_arrayViewStart < 0) 
        ? 0 : x->canvas_arrayViewStart;
    x->canvas_arrayViewStart = (x->canvas_arrayViewStart > x->array_size) 
        ? x->array_size : x->canvas_arrayViewStart;
    x->canvas_arrayViewEnd   = (x->canvas_arrayViewEnd > x->array_size) 
        ? x->array_size : x->canvas_arrayViewEnd;
    x->canvas_arrayViewEnd   = (x->canvas_arrayViewEnd < 0) 
        ? 0 : x->canvas_arrayViewEnd;
    
    wavesel_arrayZoom(x);
       
    wavesel_updateSelectRange(x);
}

static void wavesel_motion(t_wavesel *x, t_floatarg dx, t_floatarg dy)
{
    if (x->canvas_x >= 0.f && x->canvas_x <= (t_float)(x->canvas_width - 2.f))
    {
        switch (x->mode) {
        case MODE_NONE:
            break; 
        case MODE_SELECT:
            wavesel_motion_selectMode(x, dx);
            break;
        case MODE_LOOP:
            wavesel_motion_loopMode(x, dx, dy);
            break;
        case MODE_MOVE:
            wavesel_motion_zoom_moveMode(x, dx, dy);
            break;
        case MODE_DRAW:
             break;
        }
        outlet_list(x->out6, 0, 4, x->outlet_canvas);
        wavesel_set_mouse_list(x);
        SETFLOAT(&x->outlet_mouse[MOUSE_LIST_STATE], 2.f);
        outlet_list(x->out5, 0, 3, x->outlet_mouse);
    }
    wavesel_drawme(x, x->x_glist, 0);
    wavesel_draw_columns(x);
    
    // update list outlets
    wavesel_set_mouse_list(x);
    SETFLOAT(&x->outlet_mouse[MOUSE_LIST_STATE], 1.f);
    outlet_list(x->out5, 0, 3, x->outlet_mouse);
    wavesel_set_canvas_list(x);
    outlet_list(x->out6, 0, 4, x->outlet_canvas);
}

void wavesel_key(t_wavesel *x, t_floatarg f)
{
//  post("key");
  if (x || f) {}
}

static void wavesel_click_selectMode(t_wavesel *x)
{
    wavesel_reset_selection(x);
    
//    outlet_float(x->out3, x->canvas_arrayViewStart * x->system_ksr + x->canvas_x * x->outlet_outputFactor);
//    outlet_float(x->out4, x->canvas_arrayViewEnd * x->system_ksr + x->canvas_x * x->outlet_outputFactor);
    
    wavesel_color_element(x, (int)x->canvas_x + x->canvas_startForeground, 
        x->canvas_foregroundSelectedColor->s_name);
        
    wavesel_color_rect_element(x, x->canvas_selectBackground, 
        x->canvas_backgroundSelectedColor->s_name, x->canvas_backgroundSelectedColor->s_name);
    wavesel_move_background_selection(x, (int)x->canvas_x, 0);

    x->canvas_startcursor = (int)(x->canvas_x + 0.5f);
    x->canvas_endcursor   = (int)(x->canvas_x + 0.5f);
    x->canvas_clickOrigin = (int)(x->canvas_x + 0.5f);
    
    x->array_startSelectSample = 
        (int)(x->canvas_x * (t_float)x->canvas_samplesPerColumn + 0.5f) + 
        x->canvas_arrayViewStart;
    x->array_endSelectSample   = x->array_startSelectSample;
    // update cursor outlets
    outlet_float(x->out3, (x->array_startSelectSample / x->system_ksr));
    outlet_float(x->out4, (x->array_endSelectSample   / x->system_ksr));
}

static void wavesel_click_loopMode(t_wavesel *x)
{
    int selectedRange = (x->canvas_endcursor - x->canvas_startcursor) / 2;
    int columnCount = x->canvas_endForeground - x->canvas_startForeground;
    if (!x->canvas_endcursor) // if no selection
        wavesel_click_selectMode(x);
    else
    {
        wavesel_reset_selection(x);
        x->canvas_startcursor = x->canvas_x - selectedRange;
        x->canvas_endcursor   = x->canvas_x + selectedRange;
        x->canvas_startcursor = (x->canvas_startcursor < 0) ? 
            0 : x->canvas_startcursor;
        x->canvas_endcursor   = (x->canvas_endcursor > columnCount) ? 
            columnCount : x->canvas_endcursor;
        x->array_startSelectSample = 
            (int)x->canvas_startcursor * x->canvas_samplesPerColumn + 
            x->canvas_arrayViewStart;
        x->array_endSelectSample   = 
            (int)x->canvas_endcursor * x->canvas_samplesPerColumn + 
            x->canvas_arrayViewStart;

        wavesel_color_all_columns(x);
    }
    // update cursor outlets
    outlet_float(x->out3, (x->array_startSelectSample / x->system_ksr));
    outlet_float(x->out4, (x->array_endSelectSample   / x->system_ksr));
}

static void wavesel_click_zoom_moveMode(t_wavesel *x)
{
    x->canvas_clickOrigin = (int)(x->canvas_x + 0.5f);
}

static void wavesel_click(t_wavesel *x,
    t_floatarg xpos, t_floatarg ypos, t_floatarg shift, t_floatarg ctrl,
    t_floatarg doit, int up)
{
    glist_grab(x->x_glist, &x->x_obj.te_g,
        (t_glistmotionfn)wavesel_motion, (t_glistkeyfn)NULL, xpos, ypos);

    x->canvas_x = xpos - x->x_obj.te_xpix;
    x->canvas_y = ypos - x->x_obj.te_ypix;
 
    if (x->canvas_x > 0 && x->canvas_x < x->canvas_width -2 && x->canvas_y > 0 && 
        x->canvas_y < x->canvas_height)
    {
        switch (x->mode) {
        case MODE_NONE:
            break; 
        case MODE_SELECT:
            wavesel_click_selectMode(x);
            break;
        case MODE_LOOP:
            wavesel_click_loopMode(x);
            break;
        case MODE_MOVE:
            wavesel_click_zoom_moveMode(x);
            break;
        case MODE_DRAW:
            break;
        }
        // update list outlets
        wavesel_set_mouse_list(x);
        SETFLOAT(&x->outlet_mouse[MOUSE_LIST_STATE], 1.f);
        outlet_list(x->out5, 0, 3, x->outlet_mouse);
        wavesel_set_canvas_list(x);
        outlet_list(x->out6, 0, 4, x->outlet_canvas);
    }
    wavesel_drawme(x, x->x_glist, 0);
}

static int wavesel_newclick(t_gobj *z, struct _glist *glist,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    if (doit)
        wavesel_click((t_wavesel *)z, (t_floatarg)xpix, 
            (t_floatarg)ypix, (t_floatarg)shift, 0, (t_floatarg)alt,dbl);

//    if (dbl) outlet_float(((t_wavesel*)z)->out5, 1);
   
    return (1);
}

static void wavesel_activate(t_gobj *z, t_glist *glist, int state)
{
    if (z || glist || state) {} // eliminate compiler warnings
}

// when is what called (also see g_canvas.h):
// wavesel_getrect  - mouse cursor inside patch window
// wavesel_displace - move canvas in edit mode ?
// wavesel_select   - wavesel object is selected in edit mode
// wavesel_activate - wavesel object is moved in edit mode
// wavesel_delete   - wavesel object is removed ?
// wavesel_vis      - patch window is visable (again)
// wavesel_newclick - mouse cursor inside wavesel canvas
static void wavesel_setwidget(void)
{
    wavesel_widgetbehavior.w_getrectfn  = wavesel_getrect;
    wavesel_widgetbehavior.w_displacefn = wavesel_displace;
    wavesel_widgetbehavior.w_selectfn   = wavesel_select;
    wavesel_widgetbehavior.w_activatefn = wavesel_activate;
    wavesel_widgetbehavior.w_deletefn   = wavesel_delete;
    wavesel_widgetbehavior.w_visfn      = wavesel_vis;
    wavesel_widgetbehavior.w_clickfn    = wavesel_newclick;
}

static void wavesel_rect(t_wavesel* x, t_symbol* c, float xp, float y, 
    float w, float h)
{
    t_element* e = getbytes(sizeof(t_element));
    x->canvas_element[x->canvas_numelem] = e;
    
    e->type = RECT;
    e->x = xp;
    e->y = y;
    e->w = w;
    e->h = h;
    e->color = c->s_name;
    wavesel_draw_element(x, x->canvas_numelem);
    x->canvas_numelem++;
}

static void wavesel_column(t_wavesel* x, t_symbol* c, float xp, float y, 
    float w, float h)
{
    if (x->canvas_numelem < MAXELEM-1) {
        t_element* e = getbytes(sizeof(t_element));
        x->canvas_element[x->canvas_numelem] = e;
        
        e->type = LINE;
        e->x = xp;
        e->y = y;
        e->w = w;
        e->h = h;
        e->g = 1;
        e->color = c->s_name;
        wavesel_draw_element(x, x->canvas_numelem);
        x->canvas_numelem++;
    }
}

static void wavesel_color(t_wavesel* x, t_symbol* c, float num)
{
    wavesel_color_element(x,(int)num,c->s_name);
}


void wavesel_deletenum(t_wavesel* x,float num)
{
    int i = (int) num;
    if (x->canvas_element[i]) {
        wavesel_delete_element(x,i);
        freebytes(x->canvas_element[i],sizeof(t_element));
        x->canvas_element[i] = NULL;
    }
}

// find extremes in the sample range represented by this column
static void wavesel_get_column_size(t_wavesel* x, int column, 
    t_word *vec)
{
    int sampleOffset = x->canvas_arrayViewStart + 
        (int)(column * x->canvas_samplesPerColumn);
    t_float upper = 0.f, lower = 0.f;
    if (x->canvas_samplesPerColumn > 1.f) {
        int sampleEndPoint = sampleOffset + (int)x->canvas_samplesPerColumn;
        int i;
        for (i = sampleOffset; i < sampleEndPoint; i++) 
        {
            upper = (upper < (vec[i].w_float)) ? vec[i].w_float : upper;
            lower = (lower > (vec[i].w_float)) ? vec[i].w_float : lower;
        }
    }
    else
    {
        if (vec[sampleOffset].w_float > 0)
            upper = vec[sampleOffset].w_float;
        else
            lower = vec[sampleOffset].w_float;
    }
    x->canvas_columnTop = upper;
    x->canvas_columnBottom = lower;
}

static void wavesel_draw_columns(t_wavesel* x)
{
    if (!x->array) 
        return;
    t_garray *ax = x->array;
    t_word *vec;
    int size;
    if (!garray_getfloatwords(ax, &size, &vec))
    {
        post("wavesel: couldn't read from array!");
        return;
    }
    int column;
    int middle = (int)(x->canvas_height / 2);
    int columnCount = x->canvas_endForeground - x->canvas_startForeground;
    for (column = 0; column <= columnCount; column++)
    {
        wavesel_get_column_size(x, column, vec);
        t_float upper = (int)(x->canvas_columnTop * x->canvas_height * x->canvas_columnGain);
        t_float lower = (int)(x->canvas_columnBottom * x->canvas_height * x->canvas_columnGain); // + 1 ??
        t_float maxSize = 0.5f * x->canvas_height;
        
        if (x->canvas_clipmode) 
        {
             upper = (upper >  maxSize) ?  maxSize : upper;
             lower = (lower < -maxSize) ? -maxSize : lower;
        }
        x->canvas_element[column + x->canvas_startForeground]->y = 
            (int)(middle - upper);
        x->canvas_element[column + x->canvas_startForeground]->h = 
            (int)(upper - lower);
    }
}

static void wavesel_draw_foreground(t_wavesel* x)
{
    int column;
    int e = x->canvas_numelem;
    int middle = (int)(x->canvas_height / 2);
    x->canvas_startForeground = x->canvas_numelem;
    for (column = 1; column < x->canvas_width; column++)
    {
        x->canvas_element[e] = getbytes(sizeof(t_element));
        x->canvas_element[e]->type = LINE;
        x->canvas_element[e]->x = column;
        x->canvas_element[e]->y = middle;
        x->canvas_element[e]->w = 0;
        x->canvas_element[e]->h = 1;
        x->canvas_element[e]->color = x->canvas_foregroundColor->s_name;
        x->canvas_numelem++;
        e++;
    }
#define BORDERPIXELS 2
    x->canvas_endForeground = 
        x->canvas_width - BORDERPIXELS + x->canvas_startForeground; 
}

static void wavesel_reset_selection(t_wavesel* x) 
{
    if (x->canvas_startForeground == 0) return;
    int i;
    for (i = x->canvas_startForeground; i <= x->canvas_endForeground; i++)
        wavesel_color_element(x, i, x->canvas_foregroundColor->s_name);
        
    wavesel_color_rect_element(x, x->canvas_selectBackground, x->canvas_backgroundColor->s_name, 
        x->canvas_backgroundColor->s_name);
    wavesel_move_background_selection(x, x->canvas_selectBackground, 0);
    x->canvas_startcursor = 0;
    x->canvas_endcursor = 0;
    x->array_startSelectSample = 0;
    x->array_endSelectSample = 0;
}

static void wavesel_initbase(t_wavesel* x) 
{
    if (x->canvas_numelem)
        wavesel_erase(x, x->x_glist);
    t_symbol* selectColor = x->canvas_backgroundColor;
    int start = 1;
    int width = 0;
    if (x->canvas_endcursor)
    {
        selectColor = x->canvas_backgroundSelectedColor;
        start = x->canvas_startcursor;
        width = x->canvas_endcursor - x->canvas_startcursor;
    }

    // outline box
    int e = x->canvas_numelem;
    x->canvas_element[e] = getbytes(sizeof(t_element));
    x->canvas_element[e]->type = RECT;
    x->canvas_element[e]->x = 0;
    x->canvas_element[e]->y = 0;
    x->canvas_element[e]->w = x->canvas_width;
    x->canvas_element[e]->h = x->canvas_height;
    x->canvas_element[e]->color = x->canvas_backgroundColor->s_name;
    x->canvas_element[e]->outlinecolor = BORDER;
    x->canvas_numelem++;
    
    // background selection rectangle
    e = x->canvas_numelem;
    x->canvas_selectBackground = e;
    x->canvas_element[e] = getbytes(sizeof(t_element));
    x->canvas_element[e]->type = RECT;
    x->canvas_element[e]->x = start;
    x->canvas_element[e]->y = 1;
    x->canvas_element[e]->w = width;
    x->canvas_element[e]->h = x->canvas_height - 3;
    x->canvas_element[e]->color = selectColor->s_name;
    x->canvas_element[e]->outlinecolor = selectColor->s_name;
    x->canvas_numelem++;
}
  
static void wavesel_status(t_wavesel* x) 
{
    post(" --==## wavesel~ %d.%d.%d_state ##==--", MAJORVERSION, MINORVERSION, BUGFIXVERSION);
    post("arrayname: %s",               (x->array_name) ? x->array_name->s_name : "");
    post("canvas_width: %d",            x->canvas_width);
    post("canvas_height: %d",           x->canvas_height);
    post("canvas_numelem: %d",          x->canvas_numelem);
    post("canvas_arrayViewStart (absolute in array): %d",   x->canvas_arrayViewStart);
    post("canvas_arrayViewEnd (absolute in array): %d",     x->canvas_arrayViewEnd);
    post("array_startSelectSample (absolute in array): %d", x->array_startSelectSample);
    post("array_endSelectSample (absolute in array): %d",   x->array_endSelectSample);
    post("canvas_startcursor (relative to canvas): %d",     x->canvas_startcursor);
    post("canvas_endcursor (relative to canvas): %d",       x->canvas_endcursor);
    post("canvas_clickOrigin (relative to canvas): %d",     x->canvas_clickOrigin);
    post("canvas_startbg (absolute in element array): %d",  x->canvas_selectBackground);
    post("selected bg width: %d", x->canvas_endcursor - x->canvas_startcursor);
    post("canvas_startfg (absolute in element array): %d",  x->canvas_startForeground);
    post("canvas_endfg (absolute in element array): %d",    x->canvas_endForeground);
    post("canvas_samplesPerColumn: %f", x->canvas_samplesPerColumn);
    post("canvas_vzoom: %f",            x->canvas_columnGain);
    post("canvas_linePosition: %f",     x->canvas_linePosition);
    post("canvas_clipmode: %d",         x->canvas_clipmode);
    post("canvas_mode: %d",             x->mode);
    post("array: %s",                  (x->array) ? "defined" : "null");
//    post("arrayrawname: '%s'",            x->array_raw_name->s_name);
    post("arraysize: %d",               x->array_size);
    post("array_bufRefresh: %lf",       x->array_viewRefresh);
    post("outlet_outputFactor: %f",     x->outlet_outputFactor);
    post("system_ksr: %f",              x->system_ksr);
    post("system_clockRunning: %d",     x->system_clockRunning);
    post("-- base rectangle:");
    post(" x: %d", x->canvas_element[0]->x);
    post(" y: %d", x->canvas_element[0]->y);
    post(" w: %d", x->canvas_element[0]->w);
    post(" h: %d", x->canvas_element[0]->h);
    post(" color: %s", x->canvas_element[0]->color);
    post(" outlinecolor: %s", x->canvas_element[0]->outlinecolor);
    post("-- background select rectangle:");
    post(" x: %d", x->canvas_element[x->canvas_selectBackground]->x);
    post(" y: %d", x->canvas_element[x->canvas_selectBackground]->y);
    post(" w: %d", x->canvas_element[x->canvas_selectBackground]->w);
    post(" h: %d", x->canvas_element[x->canvas_selectBackground]->h);
    post(" color: %s", x->canvas_element[x->canvas_selectBackground]->color);
    post(" outlinecolor: %s", x->canvas_element[x->canvas_selectBackground]->outlinecolor);
    int major, minor, bugfix;
    sys_getversion(&major, &minor, &bugfix);
    if (major > 0 || minor > 42) 
        logpost(NULL, 4, "this is wavesel~ %s, %dth %s build",
            0, 7, 0);
}

static void wavesel_setarray(t_wavesel *x, t_symbol *s)
{
    t_garray *array;
    x->array_name = s;
    
    if ((array = (t_garray *)pd_findbyclass(x->array_name, 
        garray_class)))
    {
        wavesel_reset_selection(x);
        x->array = array;
        x->array_size = garray_npoints(x->array);
        x->canvas_samplesPerColumn = (t_float)x->array_size / 
            (t_float)x->canvas_width;
        x->outlet_outputFactor = x->canvas_samplesPerColumn / x->system_ksr;
        x->canvas_arrayViewStart = 0;
        x->canvas_arrayViewEnd   = x->array_size;
    } else {
        post("wavesel: no array \"%s\" (error %d)",
            x->array_name->s_name, array);
        x->array = 0;
        x->array_size = 0;
        return;
    }
    // update the sample view range outlets
    outlet_float(x->out1, 0);
    outlet_float(x->out2, x->array_size / x->system_ksr);

    // update cursor outlets
    outlet_float(x->out3, (x->array_startSelectSample / x->system_ksr));
    outlet_float(x->out4, (x->array_endSelectSample   / x->system_ksr));

    wavesel_draw_columns(x);
    wavesel_drawme(x, x->x_glist, 0);
}

static void wavesel_arrayZoom(t_wavesel *x)
{
    int arrayViewSize = x->canvas_arrayViewEnd - x->canvas_arrayViewStart;
    int minArrayViewSize = (x->canvas_endForeground - x->canvas_startForeground) * MAXZOOM;
    if (arrayViewSize < minArrayViewSize)
    {
        x->canvas_arrayViewEnd = x->canvas_arrayViewStart + minArrayViewSize;
    }
    
    x->canvas_samplesPerColumn = arrayViewSize / (t_float)x->canvas_width;
    x->outlet_outputFactor = x->canvas_samplesPerColumn / x->system_ksr;
    
    // update the sample view range outlets
    outlet_float(x->out1, x->canvas_arrayViewStart / x->system_ksr);
    outlet_float(x->out2, x->canvas_arrayViewEnd / x->system_ksr);

    wavesel_draw_columns(x);
    wavesel_drawme(x, x->x_glist, 0);
}

static void wavesel_setForegroundColor(t_wavesel *x, t_symbol *s)
{
    x->canvas_foregroundColor = s;
    wavesel_drawme(x, x->x_glist, 0);
}

static void wavesel_setBackgroundColor(t_wavesel *x, t_symbol *s)
{
    x->canvas_backgroundColor = s;
    x->canvas_element[0]->color = s->s_name;
    wavesel_drawme(x, x->x_glist, 0);
}

static void wavesel_setfForegroundSelectColor(t_wavesel *x, t_symbol *s)
{
    x->canvas_foregroundSelectedColor = s;
    wavesel_drawme(x, x->x_glist, 0);
}

static void wavesel_setBackgroundSelectColor(t_wavesel *x, t_symbol *s)
{
    x->canvas_backgroundSelectedColor = s;
    wavesel_drawme(x, x->x_glist, 0);
}

static void wavesel_buftime(t_wavesel* x, float fRefresh)
{
    x->array_viewRefresh = (double)(fRefresh < 0) ? WAVESEL_BUFREFRESH : 
        fRefresh;
    clock_delay(x->system_clock, x->array_viewRefresh);
}

static void wavesel_tick(t_wavesel *x)
{
    x->system_clockRunning = 0;
    if (x->array_viewRefresh == 0)
        return;
//    if (!x->x_mouseDown)
//    {
        wavesel_draw_columns(x);
//    }
    clock_delay(x->system_clock, x->array_viewRefresh);
    x->system_clockRunning = 1;
}

static void waveselhandle__clickhook(t_waveselhandle *wsh, t_floatarg f)
{
post ("handle__clickhook");
    int newstate = (int)f;
    if (wsh->h_dragon && newstate == 0)
    {
	t_wavesel *x = wsh->h_master;
	t_canvas *cv;
	x->canvas_width += wsh->h_dragx;
	x->canvas_height += wsh->h_dragy;
	if ((cv = wavesel_isvisible(x)))
	{
	    sys_vgui(".x%lx.c delete %s\n", cv, wsh->h_outlinetag);
	    wavesel_revis(x);
	    sys_vgui("destroy %s\n", wsh->h_pathname);
	    wavesel_select((t_gobj *)x, x->x_glist, 1);
	    canvas_fixlinesfor(x->x_glist, (t_text *)x);  /* 2nd inlet */
	}
    }
    else if (!wsh->h_dragon && newstate)
    {
	t_wavesel *x = wsh->h_master;
	t_canvas *cv;
	if ((cv = wavesel_isvisible(x)))
	{
	    int x1, y1, x2, y2;
	    wavesel_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
	    sys_vgui("lower %s\n", wsh->h_pathname);
	    sys_vgui(".x%lx.c create rectangle %d %d %d %d\
 -outline blue -width %f -tags %s\n",
		     cv, x1, y1, x2, y2, WAVESEL_SELBDWIDTH, wsh->h_outlinetag);
	}
	wsh->h_dragx = 0;
	wsh->h_dragy = 0;
    }
    wsh->h_dragon = newstate;
}

static void waveselhandle__motionhook(t_waveselhandle *wsh,
				    t_floatarg f1, t_floatarg f2)
{
post ("handle__motionhook");
    if (wsh->h_dragon)
    {
        t_wavesel *x = wsh->h_master;
        int dx = (int)f1, dy = (int)f2;
        int x1, y1, x2, y2, newx, newy;
        wavesel_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        newx = x2 + dx;
        newy = y2 + dy;
        if (newx > x1 + WAVESEL_MINWIDTH && 
            newy > y1 + WAVESEL_MINHEIGHT)
        {
            t_canvas *cv;
            if ((cv = wavesel_isvisible(x)))
                sys_vgui(".x%lx.c coords %s %d %d %d %d\n",
                    cv, wsh->h_outlinetag, x1, y1, newx, newy);
            wsh->h_dragx = dx;
            wsh->h_dragy = dy;
        }
        x->canvas_width  = x2 - x1;
        x->canvas_height = y2 - y1;
    }
}

static void *wavesel_line(t_wavesel *x, t_float f)
{
    x->canvas_linePosition = f;
    wavesel_reset_selection(x);
    int size = x->canvas_endForeground - x->canvas_startForeground;
    int column = (int)((x->canvas_linePosition / x->canvas_samplesPerColumn) * 
        x->system_ksr + 0.5);
        
    if (column >= 0 && column <= size) {
        wavesel_color_element(x, column + x->canvas_startForeground, 
            x->canvas_foregroundSelectedColor->s_name);
            
        wavesel_move_background_selection(x, column, 1);
        wavesel_color_rect_element(x, x->canvas_selectBackground, 
            x->canvas_backgroundSelectedColor->s_name, x->canvas_backgroundSelectedColor->s_name);
    }
    return 0;
}

static void *wavesel_clipdraw(t_wavesel *x, t_float f)
{
    x->canvas_clipmode = (int)(f < 0) ? 0 : f;
    return 0;
}

static void *wavesel_doup(t_wavesel *x, t_float f)
{
    x->canvas_mouseDown = (int)(f > 0) ? 0 : 1;
//    wavesel_set_mouse_list(x);
//    SETFLOAT(&x->outlet_mouse[MOUSE_LIST_STATE], 0.f);
//    outlet_list(x->out5, 0, 3, x->outlet_mouse);

    return 0;
}

static void *wavesel_vzoom(t_wavesel *x, t_float f)
{
    x->canvas_columnGain = (f > 0) ? f : 1;
    
    wavesel_drawme(x, x->x_glist, 0);
    wavesel_draw_columns(x);
    return 0;
}

static void *wavesel_loadbang(t_wavesel *x)
{
    if (!sys_noloadbang)
    {
//        post("wavesel_loadbang: !sys_noloadbang");
        if (!x->array && x->array_name->s_name[0] != 0)
            wavesel_setarray(x, x->array_name);
    }
    return 0;
}

static void wavesel_save(t_gobj *z, t_binbuf *b)
{
    t_wavesel *x = (t_wavesel *)z;
    t_text *t = (t_text *)x;
    binbuf_addv(b, "ssiisiiii;", gensym("#X"), gensym("obj"), 
        (int)t->te_xpix, (int)t->te_ypix,
        x->array_name->s_name, x->canvas_width, x->canvas_height, 
        x->mode, x->array_viewRefresh, 0);
/*    post("saving wavesel: '#X obj %d %d %s %d %d %d %d'", 
        (int)t->te_xpix, (int)t->te_ypix,
        x->array_name->s_name, x->canvas_width, x->canvas_height, 
        x->mode, x->array_viewRefresh);  */
}

 
/* static void *wavesel_new(
t_symbol* s,              0
* t_float w,              1
* t_float h,              2
* t_floatarg _mode,       3
* t_floatarg _buftime,    4
* t_symbol* _fgcolor,     5
* t_symbol* _bgcolor,     6
* t_symbol* _fgselcolor,  7
* t_symbol* _bgselcolor   8
* ) */
static void *wavesel_new(t_symbol *s, int ac, t_atom *av)
{
printf("----\nwavesel creation \n");
//  ac, atom_getsymbol(&av[0])->s_name, atom_getfloat(&av[1]), atom_getfloat(&av[2]), atom_getfloat(&av[3]), atom_getfloat(&av[4]));
    int i;
    t_wavesel *x = (t_wavesel *)pd_new(wavesel_class);

    x->x_glist = (t_glist*) canvas_getcurrent();
    x->system_ksr   = sys_getsr() * MILLIS;
    x->outlet_outputFactor = 1.; // used for outlet unit calculation
    
    x->canvas_mouseDown = 0;
    
    for (i=0;i<MAXELEM;i++)
        x->canvas_element[i] = NULL;
    x->canvas_numelem   = 0;
    // 0: s
    x->array = 0;
    x->array_name = 0;
    if (ac > 0) {
        x->array_name = atom_getsymbol(&av[0]);
printf("wavesel array: '%s'\n", x->array_name->s_name);
    } else {
        x->array_name = atom_getsymbol("");
    }
    // 1: w
    t_float width;
    if (ac > 1) 
    {
        width = atom_getfloat(&av[1]);
        if (width > 0)
            x->canvas_width = (int)width;
printf("wavesel width: %d(%f:%f)\n", x->canvas_width, atom_getfloat(&av[1]), width);
    }
    else
        x->canvas_width = WAVESEL_DEFWIDTH;
    // 2: h
    t_float height;
    if (ac > 2) 
    {
        height = atom_getfloat(&av[2]);
        if (height > 0)
            x->canvas_height = (int)height;
printf("wavesel height: %d(%f:%f)\n", x->canvas_height, atom_getfloat(&av[2]), height);
    }
    else
        x->canvas_height = WAVESEL_DEFHEIGHT;
    // 3: _mode
    t_float fmode;
    if (ac > 3) 
    {
        fmode = atom_getfloat(&av[3]);
        if (fmode > 0)
            x->mode = (int)fmode;
printf("wavesel mode: %d(%f)\n", x->mode, fmode);
    }
    else
        x->mode = MODE_NONE;
    // 4: _buftime
    t_float buftime;
    if (ac > 4) 
    {
        buftime = atom_getfloat(&av[4]);
        if (buftime > 0)
            x->array_viewRefresh = (double)buftime;
printf("wavesel buftime: %lf(%f)\n", x->array_viewRefresh, buftime);
    }
    else
        x->array_viewRefresh = 0.;
    
    x->system_clock = clock_new(x, (t_method)wavesel_tick); // get a clock
    if (x->array_viewRefresh == 0) 
    {
        clock_delay(x->system_clock, x->array_viewRefresh); // start it
        x->system_clockRunning = 1;
    } else 
        x->system_clockRunning = 0;

    x->canvas_arrayViewStart = 0;
    x->canvas_arrayViewEnd   = 0;
    x->canvas_startcursor = 0;
    x->canvas_endcursor   = 0;
    x->canvas_foregroundColor = gensym(FGCOLOR);
    x->canvas_backgroundColor = gensym(BGCOLOR);
    x->canvas_foregroundSelectedColor = gensym(FGSELCOLOR);
    x->canvas_backgroundSelectedColor = gensym(BGSELCOLOR);

    x->canvas_columnGain = 1;
    wavesel_initbase(x);
    wavesel_draw_foreground(x);
//    wavesel_draw_columns(x);

    x->out1 = outlet_new(&x->x_obj, &s_float);
    x->out2 = outlet_new(&x->x_obj, &s_float);
    x->out3 = outlet_new(&x->x_obj, &s_float);
    x->out4 = outlet_new(&x->x_obj, &s_float);
    x->out5 = outlet_new(&x->x_obj, &s_list);
    x->out6 = outlet_new(&x->x_obj, &s_list);
    
    SETFLOAT(&x->outlet_mouse[MOUSE_LIST_X],     0.f);
    SETFLOAT(&x->outlet_mouse[MOUSE_LIST_Y],     0.f);
    SETFLOAT(&x->outlet_mouse[MOUSE_LIST_STATE], 0.f);
    SETFLOAT(&x->outlet_canvas[CANVAS_LIST_VIEW_START],   0.f);
    SETFLOAT(&x->outlet_canvas[CANVAS_LIST_VIEW_END],     0.f);
    SETFLOAT(&x->outlet_canvas[CANVAS_LIST_SELECT_START], 0.f);
    SETFLOAT(&x->outlet_canvas[CANVAS_LIST_SELECT_END],   0.f); 
    
    sprintf(x->canvas_allTag,        "all%lx", (unsigned long)x);
    sprintf(x->canvas_backgroundTag, "bg%lx",  (unsigned long)x);
    sprintf(x->canvas_gridTag,       "gr%lx",  (unsigned long)x);
    sprintf(x->canvas_foregroundTag, "fg%lx",  (unsigned long)x);
//    sprintf(x->canvas_inletTag,      "%lxi",   (unsigned long)x);
//    sprintf(x->canvas_outletTag,     "%lxo",   (unsigned long)x);

// handle start  
    t_waveselhandle *wsh;
    char handlebuf[TAGBUFFER];    
    x->editHandle = pd_new(waveselhandle_class);
    wsh = (t_waveselhandle *)x->editHandle;
    wsh->h_master = x;
    sprintf(handlebuf, "_h%lx", (unsigned long)wsh);
    pd_bind(x->editHandle, wsh->h_bindsym = gensym(handlebuf));
    sprintf(wsh->h_outlinetag, "h%lx", (unsigned long)wsh);
    wsh->h_dragon = 0;
    wsh->h_selectedmode = 0;
// handle end 

// init hammer for mouseUp (wavesel_doup)
    hammergui_bindmouse((t_pd *)x); 
    x->mouseBound = 1;
    
    forky_setsavefn(wavesel_class, wavesel_save);
    
printf("wavesel creation arguments: '%s' %d %d %d %lf \n", 
    (x->array_name) ? x->array_name->s_name : "", x->canvas_width, x->canvas_height,
    x->mode, x->array_viewRefresh);

    return (x);
}

void wavesel_tilde_setup(void)
{
    wavesel_class = class_new(gensym("wavesel~"), 
        (t_newmethod)wavesel_new, 0, sizeof(t_wavesel), 0, A_GIMME, 0);
//        A_DEFSYM, A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 
//        A_DEFSYM, A_DEFSYM, A_DEFSYM, A_DEFSYM, 0);

// Max/MSP messages
    class_addmethod(wavesel_class, (t_method)wavesel_buftime, 
        gensym("buftime"), A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_clipdraw, 
        gensym("clipdraw"), A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_line, 
        gensym("line"), A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_setarray, 
        gensym("set"), A_SYMBOL, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_vzoom, 
        gensym("vzoom"), A_FLOAT, 0);

// wavesel specific
    class_addmethod(wavesel_class, (t_method)wavesel_setarray, 
        gensym("setarray"), A_SYMBOL, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_setForegroundColor, 
        gensym("setfgcolor"), A_SYMBOL, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_setBackgroundColor, 
        gensym("setbgcolor"), A_SYMBOL, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_setfForegroundSelectColor, 
        gensym("setfgselcolor"), A_SYMBOL, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_setBackgroundSelectColor, 
        gensym("setbgselcolor"), A_SYMBOL, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_reset_selection, 
        gensym("reset_selection"), 0);
    class_addmethod(wavesel_class, (t_method)wavesel_draw_columns, 
        gensym("drawwf"), A_FLOAT, 0);

// internal
    class_addmethod(wavesel_class, (t_method)wavesel_column, 
        gensym("column"), A_SYMBOL, A_FLOAT, A_FLOAT, A_FLOAT,A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_rect, 
        gensym("rect"), A_SYMBOL, A_FLOAT, A_FLOAT, A_FLOAT,A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_color, 
        gensym("color"), A_SYMBOL, A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_deletenum, 
        gensym("delete"), A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_setmode, 
        gensym("setmode"), A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_status, 
        gensym("status"), 0);
    class_addmethod(wavesel_class, (t_method)wavesel_redraw_all, 
        gensym("redraw_all"), 0);
    class_addmethod(wavesel_class, (t_method)wavesel_loadbang, 
        gensym("loadbang"), 0);
    class_addmethod(wavesel_class, (t_method)wavesel_move_element, 
        gensym("move_element"), A_FLOAT, 0);
        
// hammergui
    class_addmethod(wavesel_class, (t_method)wavesel_doup, 
        gensym("_up"), A_FLOAT, 0);
//    class_addmethod(wavesel_class, (t_method)wavesel_doelse, 
//        gensym("_bang"), A_FLOAT, A_FLOAT, 0);
//    class_addmethod(wavesel_class, (t_method)wavesel_doelse,
//        gensym("_zero"), A_FLOAT, A_FLOAT, 0);
//    class_addmethod(wavesel_class, (t_method)wavesel_doelse,
//        gensym("_poll"), A_FLOAT, A_FLOAT, 0);

// GUI behaviour
    wavesel_setwidget();
    class_setwidget(wavesel_class, &wavesel_widgetbehavior);

// resize handle in edit mode    
    waveselhandle_class = class_new(gensym("_waveselhandle"), 0, 0,
        sizeof(t_waveselhandle), CLASS_PD, 0);
    class_addmethod(waveselhandle_class, 
        (t_method)waveselhandle__clickhook, gensym("_click"), A_FLOAT, 0);
    class_addmethod(waveselhandle_class, 
    (t_method)waveselhandle__motionhook, gensym("_motion"), 
        A_FLOAT, A_FLOAT, 0);
//    class_setsavefn(wavesel_class, wavesel_save);
    
    post("wavesel~ %d.%d.%d , a waveform~ wannabe, that isn't there yet...", MAJORVERSION, MINORVERSION, BUGFIXVERSION);
    post("fjkraan@xs4all.nl, 2016-07-29");
}



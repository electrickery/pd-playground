
/*****************************************************
 *
 * receivecanvas - implementation file
 *
 * copyleft (c) 2009, IOhannes m zmölnig
 *
 *   forum::für::umläute
 *
 *   institute of electronic music and acoustics (iem)
 *
 ******************************************************
 *
 * license: GNU General Public License v.2 (or later)
 *
 ******************************************************/


/*
 * this object provides a way to receive messages to upstream canvases
 * by default it receives messages to the containing canvas, but you can give the "depth" as argument;
 * e.g. [receivecanvas 1] will receive messages to the parent of the containing canvas
 */

/* NOTES:
 *  it would be _really_ nice to get all the messages that are somehow "sent" to a (parent) object,
 *  no matter whether using typedmess() or using sendcanvas()
 *  this would require (however) to overwrite and proxy the classmethods for canvas which is a chore
 *
 *  currently this objects only gets the messages from typedmess()...
 */

#include "iemguts.h"
#include "g_canvas.h"

#include <stdio.h>

#define MAJORVERSION 0
#define MINORVERSION 1
#define BUGFIXVERSION 1

#define DEFAULTSIZE 100;

static t_class *receivecanvas_class, *receivecanvas_proxy_class;

typedef struct _receivecanvas_proxy
{
  t_object  p_obj;
  t_symbol *p_sym;
  t_clock  *p_clock;
  struct _receivecanvas *p_parent;
} t_receivecanvas_proxy;

typedef struct _receivecanvas
{
  t_object               x_obj;
  t_receivecanvas_proxy *x_proxy;
  t_int                  mouseState;
  t_int                  previousMouseState;
  t_atom                 rOut[3];
  t_atom                 lOut[3];
  t_int                  width;
  t_int                  height;
  t_int                  prevX;
  t_int                  prevY;
  t_outlet              *rOutlet;
  t_outlet              *lOutlet;
  t_symbol              *mouseUp;
  t_symbol              *mouseDown;
  t_symbol              *motion;
  t_glist     *x_glist;
  t_int                  offsetX;
  t_int                  offsetY;
  t_canvas              *x_pCanvas;
  t_canvas              *x_cCanvas;
} t_receivecanvas;

/* ------------------------- receivecanvas proxy ---------------------------- */

static void receivecanvas_anything(t_receivecanvas *x, t_symbol*s, int argc, t_atom*argv);

static void receivecanvas_proxy_anything(t_receivecanvas_proxy *prox, t_symbol*s, int argc, t_atom*argv) {
  if(prox->p_parent)
    receivecanvas_anything(prox->p_parent, s, argc, argv);
}

// the free method is called via the clock set by the main free method, destroying
// the proxy instance just after the main instance
static void receivecanvas_proxy_free(t_receivecanvas_proxy *prox)
{
  if (prox->p_sym)
    pd_unbind(&prox->p_obj.ob_pd, prox->p_sym);
  prox->p_sym = NULL;

  clock_free(prox->p_clock);
  prox->p_clock = NULL;

  prox->p_parent = NULL;
  pd_free(&prox->p_obj.ob_pd);

  prox = NULL;
}

// instantiate the proxy, bind it (so it receives via receivecanvas_proxy_anything?)
// register a clock (but do not set it?) to have some automatic free on destroy?
static t_receivecanvas_proxy*receivecanvas_proxy_new(t_receivecanvas *x, t_symbol*s) {
  t_receivecanvas_proxy *prox = NULL;

  if(!x) return prox;

  prox = (t_receivecanvas_proxy*)pd_new(receivecanvas_proxy_class);

  prox->p_sym = s;
  if (prox->p_sym) {
    pd_bind(&prox->p_obj.ob_pd, prox->p_sym);
  }
  prox->p_parent = x;
  prox->p_clock = clock_new(prox, (t_method)receivecanvas_proxy_free);

  return prox;
}


/* ------------------------- receivecanvas ---------------------------- */
static void receivecanvas_anything(t_receivecanvas *x, t_symbol*s, int argc, t_atom*argv)
{
    if (argc < 3) return;
    if (s != x->motion && s != x->mouseDown && s != x->mouseUp) return;
    
    t_canvas*c  = x->x_cCanvas;
    if(!c) return;
    t_float pGOPx = c->gl_obj.te_xpix;
    t_float pGOPy = c->gl_obj.te_ypix;
    
    int xpos = x->x_obj.te_xpix + pGOPx;
    int ypos = x->x_obj.te_ypix + pGOPy;
    int X = atom_getintarg(0, argc, argv);
    int Y = atom_getintarg(1, argc, argv);
    int rX = X - xpos;
    int rY = Y - ypos;
    int dX = rX - x->prevX;
    int dY = rY - x->prevY;
    x->prevX = rX;
    x->prevY = rY;

    x->previousMouseState = x->mouseState;
    if (s == x->mouseDown) x->mouseState = 1;
    if (s == x->mouseUp)   x->mouseState = 0;
    
    //current + (current - previous) = 2 * current + previous
    /* current  previous | dyn.state
     *    0         0         0
     *    0         1        -1 
     *    1         0         2
     *    1         1         1
     */
    
    int dynMouseState = (x->mouseState) ? 2 * x->mouseState - x->previousMouseState : -x->previousMouseState;
    
    printf("screen te_xpix: %f, te_ypix: %f\n", pGOPx, pGOPy);
     
    SETFLOAT(x->rOut,   (t_float)dynMouseState);
    SETFLOAT(x->rOut+1, (t_float)rX);
    SETFLOAT(x->rOut+2, (t_float)rY);
    SETFLOAT(x->lOut,   (t_float)dynMouseState);
    SETFLOAT(x->lOut+1, (t_float)dX);
    SETFLOAT(x->lOut+2, (t_float)dY);

    if (X > xpos && Y > ypos && X < xpos + x->width && Y < ypos + x->height) {
//        post("inside %d %d %d", mouseState, rX, rY);
        outlet_anything(x->lOutlet, gensym("inside"), 3, x->rOut);
        outlet_anything(x->rOutlet, gensym("inside"), 3, x->lOut);
    } else {
//        post("outside %d %d %d", x->mouseState, rX, rY);
//        outlet_anything(x->x_obj.ob_outlet, s, argc, argv);
        outlet_anything(x->lOutlet, gensym("outside"), 3, x->rOut);
        outlet_anything(x->rOutlet, gensym("outside"), 3, x->lOut);
    }
}

static void receivecanvas_offset(t_receivecanvas *x, t_float offsetX, t_float offsetY) 
{
    x->offsetX = offsetX;
    x->offsetY = offsetY;
}

static void receivecanvas_size(t_receivecanvas *x, t_float newX, t_float newY) 
{
    if (newX > 0) x->width  = newX;
    if (newY > 0) x->height = newY;
}

static void receivecanvas_status(t_receivecanvas *x) 
{
    post("  --==## %s status %d.%d.%d##==--", 
        "mouseCanvas", MAJORVERSION, MINORVERSION, BUGFIXVERSION);

    post("child offset x: %d", x->x_obj.te_xpix);
    post("child offset y: %d", x->x_obj.te_ypix);
    post("width:          %d", x->width);
    post("height:         %d", x->height);
    post("prevX:          %d", x->prevX);
    post("prevY:          %d", x->prevY);
    
    t_canvas*c  = x->x_cCanvas;
    if(!c) return;
    post("parent offset x: %d", c->gl_obj.te_xpix);
    post("parent offset y: %d", c->gl_obj.te_ypix);

}

// set the clock to destroy the proxy instance post-mortem
static void receivecanvas_free(t_receivecanvas *x)
{
  if(x->x_proxy) {
    x->x_proxy->p_parent = NULL;
    clock_delay(x->x_proxy->p_clock, 0);
  }
}

static void *receivecanvas_new(t_floatarg f)
{
  t_receivecanvas *x = (t_receivecanvas *)pd_new(receivecanvas_class);
  t_glist *glist = (t_glist *)canvas_getcurrent();
  t_canvas *cCanvas = (t_canvas*)glist_getcanvas(glist);
  t_canvas *pCanvas = (t_canvas*)glist_getcanvas(glist);
  int depth = (int)f;
  if(depth < 0)depth = 0;

  // find the canvas depth levels up
  while(depth && pCanvas) {
    cCanvas = pCanvas;
    pCanvas = pCanvas->gl_owner;
    depth--;
  }

  x->x_pCanvas  = pCanvas;
  x->x_cCanvas  = cCanvas;
  x->x_proxy =   NULL;
  x->mouseState = 0;
  x->previousMouseState = 0;
  x->width     = DEFAULTSIZE;
  x->height    = DEFAULTSIZE;
  x->prevX     = 0;
  x->prevY     = 0;
  x->mouseUp   = gensym("mouseup");
  x->mouseDown = gensym("mouse");
  x->motion    = gensym("motion");
  x->offsetX   = 0;
  x->offsetY   = 0;

  // call the proxy_new method using the selected (upper) canvas pointer as name
  if(pCanvas) {
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING-1, ".x%lx", (t_int)pCanvas);
    buf[MAXPDSTRING-1] = 0;

    x->x_proxy = receivecanvas_proxy_new(x, gensym(buf));
  }

  x->lOutlet = outlet_new(&x->x_obj, 0);
  x->rOutlet = outlet_new(&x->x_obj, 0);

  return (x);
}

void mouseCanvas_setup(void)
{
  iemguts_boilerplate("[mouseCanvas]", 0);
  receivecanvas_class = class_new(gensym("mouseCanvas"), (t_newmethod)receivecanvas_new,
                               (t_method)receivecanvas_free, sizeof(t_receivecanvas), 0, A_DEFFLOAT, 0);
  class_addmethod(receivecanvas_class, (t_method)receivecanvas_offset, 
        gensym("offset"), A_DEFFLOAT, A_DEFFLOAT,0);
  class_addmethod(receivecanvas_class, (t_method)receivecanvas_size, 
        gensym("size"), A_DEFFLOAT, A_DEFFLOAT,0);
   class_addmethod(receivecanvas_class, (t_method)receivecanvas_status, 
        gensym("status"), 0);
 
  // register the proxy class with an anything message method
  receivecanvas_proxy_class = class_new(0, 0, 0, sizeof(t_receivecanvas_proxy), CLASS_NOINLET | CLASS_PD, 0);
  class_addanything(receivecanvas_proxy_class, receivecanvas_proxy_anything);
}

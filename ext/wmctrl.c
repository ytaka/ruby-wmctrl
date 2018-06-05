#include <ruby.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xmu/WinUtil.h>
#include <glib.h>

#include <ruby/encoding.h>
#define RB_UTF8_STRING_NEW(ptr, size) rb_enc_str_new(ptr, size, rb_utf8_encoding())
#define RB_UTF8_STRING_NEW2(ptr) rb_enc_str_new(ptr, strlen(ptr), rb_utf8_encoding())

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

#define MAX_PROPERTY_VALUE_LEN 4096

#define p_verbose(...) if (RTEST(ruby_debug)) { \
    fprintf(stderr, __VA_ARGS__); \
}

#define envir_utf8 TRUE

static int client_msg(Display *disp, Window win, const char *msg,
		      unsigned long data0, unsigned long data1,
		      unsigned long data2, unsigned long data3,
		      unsigned long data4);
static gchar *get_property (Display *disp, Window win,
			    Atom xa_prop_type, const gchar *prop_name, unsigned long *size);
static Window *get_client_list (Display *disp, unsigned long *size);
static gchar *get_window_class (Display *disp, Window win);
static gchar *get_window_title (Display *disp, Window win);
static int activate_window (Display *disp, Window win, gboolean switch_desktop);
static int close_window (Display *disp, Window win);
static gboolean wm_supports (Display *disp, const gchar *prop);
static int window_move_resize (Display *disp, Window win, signed long grav,
			       signed long x, signed long y, signed long w, signed long h);
static int window_state (Display *disp, Window win,
			 const char *action_str, const char *prop1_str, const char *prop2_str);
static int window_to_desktop (Display *disp, Window win, int desktop);
static void window_set_title (Display *disp, Window win, char *title, char mode);
static int action_window (Display *disp, Window win, char mode, int argc, VALUE *argv);
static Window Select_Window(Display *dpy);
static Window get_active_window(Display *disp);
static Window get_target_window (Display *disp, VALUE obj);


static VALUE rb_wmctrl_class, key_id, key_title, key_pid, key_geometry,
  key_active, key_class, key_client_machine, key_desktop,
  key_viewport, key_workarea, key_current, key_showing_desktop, key_name,
  key_state, key_window_type, key_frame_extents, key_strut, key_exterior_frame;

static ID id_select, id_active, id_activate, id_close, id_move_resize,
  id_change_state, id_move_to_desktop, id_move_to_current,
  id_set_title_long, id_set_title_short, id_set_title_both;

static void rb_wmctrl_free (void **ptr)
{
  Display **disp;
  disp = (Display **) ptr;
  XCloseDisplay(*disp);
  free(ptr);
}

static VALUE rb_wmctrl_alloc(VALUE self)
{
  Display **ptr;
  return Data_Make_Struct(rb_wmctrl_class, Display*, 0, rb_wmctrl_free, ptr);
}

static VALUE rb_wmctrl_initialize(int argc, VALUE *argv, VALUE self)
{
  Display **ptr;
  Data_Get_Struct(self, Display*, ptr);
  if (! (*ptr = XOpenDisplay(NULL))) {
    rb_raise(rb_eStandardError, "Cannot open display.\n");
  }
  return self;
}

static int client_msg(Display *disp, Window win, const char *msg,
		      unsigned long data0, unsigned long data1,
		      unsigned long data2, unsigned long data3,
		      unsigned long data4)
{
  XEvent event;
  long mask = SubstructureRedirectMask | SubstructureNotifyMask;

  event.xclient.type = ClientMessage;
  event.xclient.serial = 0;
  event.xclient.send_event = True;
  event.xclient.message_type = XInternAtom(disp, msg, False);
  event.xclient.window = win;
  event.xclient.format = 32;
  event.xclient.data.l[0] = data0;
  event.xclient.data.l[1] = data1;
  event.xclient.data.l[2] = data2;
  event.xclient.data.l[3] = data3;
  event.xclient.data.l[4] = data4;

  if (!XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event)) {
    rb_raise(rb_eStandardError, "Cannot send %s event.\n", msg);
  }
  XFlush(disp);			/* Maybe, we need XFlush. */
  return True;
}

/* Copy from debian package for 64bit support. */
static gchar *get_property (Display *disp, Window win,
			    Atom xa_prop_type, const gchar *prop_name, unsigned long *size)
{
  Atom xa_prop_name;
  Atom xa_ret_type;
  int ret_format;
  unsigned long ret_nitems;
  unsigned long ret_bytes_after;
  unsigned long tmp_size;
  unsigned char *ret_prop;
  gchar *ret;

  xa_prop_name = XInternAtom(disp, prop_name, False);

  /* MAX_PROPERTY_VALUE_LEN / 4 explanation (XGetWindowProperty manpage):
   *
   * long_length = Specifies the length in 32-bit multiples of the
   *               data to be retrieved.
   *
   * NOTE:  see
   * http://mail.gnome.org/archives/wm-spec-list/2003-March/msg00067.html
   * In particular:
   *
   * 	When the X window system was ported to 64-bit architectures, a
   * rather peculiar design decision was made. 32-bit quantities such
   * as Window IDs, atoms, etc, were kept as longs in the client side
   * APIs, even when long was changed to 64 bits.
   *
   */
  if (XGetWindowProperty(disp, win, xa_prop_name, 0, MAX_PROPERTY_VALUE_LEN / 4, False,
			 xa_prop_type, &xa_ret_type, &ret_format,
			 &ret_nitems, &ret_bytes_after, &ret_prop) != Success) {
    p_verbose("Cannot get %s property.\n", prop_name);
    return NULL;
  }

  if (xa_ret_type != xa_prop_type) {
    p_verbose("Invalid type of %s property.\n", prop_name);
    XFree(ret_prop);
    return NULL;
  }

  /* null terminate the result to make string handling easier */
  tmp_size = (ret_format / 8) * ret_nitems;
  /* Correct 64 Architecture implementation of 32 bit data */
  if(ret_format==32) tmp_size *= sizeof(long)/4;
  ret = g_malloc(tmp_size + 1);
  memcpy(ret, ret_prop, tmp_size);
  ret[tmp_size] = '\0';

  if (size) {
    *size = tmp_size;
  }

  XFree(ret_prop);
  return ret;
}

static Window *get_client_list (Display *disp, unsigned long *size)
{
  Window *client_list;

  if ((client_list = (Window *)get_property(disp, DefaultRootWindow(disp),
  					    XA_WINDOW, "_NET_CLIENT_LIST", size)) == NULL) {
    if ((client_list = (Window *)get_property(disp, DefaultRootWindow(disp),
  					      XA_CARDINAL, "_WIN_CLIENT_LIST", size)) == NULL) {
      fputs("Cannot get client list properties. \n"
  	    "(_NET_CLIENT_LIST or _WIN_CLIENT_LIST)"
  	    "\n", stderr);
      return NULL;
    }
  }

  return client_list;
}

static gchar *get_window_class (Display *disp, Window win)
{
  gchar *class_utf8;
  gchar *wm_class;
  unsigned long size;

  wm_class = get_property(disp, win, XA_STRING, "WM_CLASS", &size);
  if (wm_class) {
    gchar *p_0 = strchr(wm_class, '\0');
    if (wm_class + size - 1 > p_0) {
      *(p_0) = '.';
    }
    class_utf8 = g_locale_to_utf8(wm_class, -1, NULL, NULL, NULL);
  }
  else {
    class_utf8 = NULL;
  }

  g_free(wm_class);

  return class_utf8;
}

static gchar *get_window_title (Display *disp, Window win)
{
  gchar *title_utf8;
  gchar *wm_name;
  gchar *net_wm_name;

  wm_name = get_property(disp, win, XA_STRING, "WM_NAME", NULL);
  net_wm_name = get_property(disp, win,
			     XInternAtom(disp, "UTF8_STRING", False), "_NET_WM_NAME", NULL);

  if (net_wm_name) {
    title_utf8 = g_strdup(net_wm_name);
  }
  else {
    if (wm_name) {
      title_utf8 = g_locale_to_utf8(wm_name, -1, NULL, NULL, NULL);
    }
    else {
      title_utf8 = NULL;
    }
  }

  g_free(wm_name);
  g_free(net_wm_name);

  return title_utf8;
}

static VALUE get_window_hash_data (Window win, Display *disp, Window window_active, int get_state)
{
  VALUE window_obj = rb_hash_new();
  gchar *title_utf8 = get_window_title(disp, win); /* UTF8 */
  gchar *client_machine;
  gchar *class_out = get_window_class(disp, win); /* UTF8 */
  unsigned long *desktop;

  if ((int) window_active < 0) {
    window_active = get_active_window(disp);
  }

  rb_hash_aset(window_obj, key_id, INT2NUM(win));
  rb_hash_aset(window_obj, key_title, (title_utf8 ? RB_UTF8_STRING_NEW2(title_utf8) : Qnil));
  rb_hash_aset(window_obj, key_class, (class_out ? RB_UTF8_STRING_NEW2(class_out) : Qnil));

  if (window_active == win) {
    rb_hash_aset(window_obj, key_active, Qtrue);
  } else {
    rb_hash_aset(window_obj, key_active, Qnil);
  }

  /* desktop ID */
  if ((desktop = (unsigned long *)get_property(disp, win,
                                               XA_CARDINAL, "_NET_WM_DESKTOP", NULL)) == NULL) {
    desktop = (unsigned long *)get_property(disp, win, XA_CARDINAL, "_WIN_WORKSPACE", NULL);
  }
  /* special desktop ID -1 means "all desktops", so we
     have to convert the desktop value to signed long */
  rb_hash_aset(window_obj, key_desktop, INT2NUM(desktop ? (signed long)*desktop : 0));

  /* client machine */
  client_machine = get_property(disp, win, XA_STRING, "WM_CLIENT_MACHINE", NULL);
  rb_hash_aset(window_obj, key_client_machine, (client_machine ? RB_UTF8_STRING_NEW2(client_machine) : Qnil));

  if (get_state) {
    unsigned long *pid;
    int x, y, junkx, junky;
    unsigned long j;
    unsigned int wwidth, wheight, bw, depth;
    Window junkroot;
    unsigned long state_size;
    Atom *window_state;
    gchar *state_name;
    VALUE state_ary;
    unsigned long window_type_size;
    Atom *window_type;
    gchar *window_type_name;
    VALUE window_type_ary;
    Atom *extents;
    unsigned long extents_size;
    VALUE extents_ary;
    Atom *strut;
    unsigned long strut_size;
    VALUE strut_ary;

    /* pid */
    pid = (unsigned long *)get_property(disp, win, XA_CARDINAL, "_NET_WM_PID", NULL);
    rb_hash_aset(window_obj, key_pid, (pid ? ULONG2NUM(*pid) : Qnil));
    g_free(pid);

    /* geometry */
    XGetGeometry (disp, win, &junkroot, &junkx, &junky, &wwidth, &wheight, &bw, &depth);
    XTranslateCoordinates (disp, win, junkroot, -bw, -bw, &x, &y, &junkroot);

    rb_hash_aset(window_obj, key_geometry,
                 rb_ary_new3(4, INT2NUM(x), INT2NUM(y), INT2NUM(wwidth), INT2NUM(wheight)));

    /* state */
    if ((window_state = (Atom *)get_property(disp, win,
                                             XA_ATOM, "_NET_WM_STATE", &state_size)) != NULL) {
      state_ary = rb_ary_new();
      for (j = 0; j < state_size / sizeof(Atom); j++) {
        state_name = XGetAtomName(disp, window_state[j]);
        rb_ary_push(state_ary, rb_str_new2(state_name));
        g_free(state_name);
      }
      g_free(window_state);
    } else {
      state_ary = Qnil;
    }
    rb_hash_aset(window_obj, key_state, state_ary);

    /* window type */
    if ((window_type = (Atom *)get_property(disp, win,
                                            XA_ATOM, "_NET_WM_WINDOW_TYPE", &window_type_size)) != NULL) {
      window_type_ary = rb_ary_new();
      for (j = 0; j < window_type_size / sizeof(Atom); j++) {
        window_type_name = XGetAtomName(disp, window_type[j]);
        rb_ary_push(window_type_ary, rb_str_new2(window_type_name));
        g_free(window_type_name);
      }
      g_free(window_type);
    } else {
      window_type_ary = Qnil;
    }
    rb_hash_aset(window_obj, key_window_type, window_type_ary);

    /* frame extents */
    if ((extents = (unsigned long *)get_property(disp, win,
                                                 XA_CARDINAL, "_NET_FRAME_EXTENTS", &extents_size)) != NULL) {
      extents_ary = rb_ary_new();
      for (j = 0; j < extents_size / sizeof(unsigned long); j++) {
        rb_ary_push(extents_ary, ULONG2NUM(extents[j]));
      }
      /* exterior frame */
      if (extents) {
        rb_hash_aset(window_obj, key_exterior_frame,
                     rb_ary_new3(4, INT2NUM(x - (int)extents[0]), INT2NUM(y - (int)extents[2]),
                                 INT2NUM(wwidth + (int)extents[0] + (int)extents[1]),
                                 INT2NUM(wheight + (int)extents[2] + (int)extents[3])));
      }
      g_free(extents);
    } else {
      extents_ary = Qnil;
    }
    rb_hash_aset(window_obj, key_frame_extents, extents_ary);

    /* strut partial or strut */
    if ((strut = (unsigned long *)get_property(disp, win,
                                               XA_CARDINAL, "_NET_WM_STRUT_PARTIAL", &strut_size)) == NULL) {
      strut = (unsigned long *)get_property(disp, win, XA_CARDINAL, "_NET_WM_STRUT", &strut_size);
    }
    if (strut) {
      strut_ary = rb_ary_new();
      for (j = 0; j < strut_size / sizeof(unsigned long); j++) {
        rb_ary_push(strut_ary, ULONG2NUM(strut[j]));
      }
      g_free(strut);
    } else {
      strut_ary = Qnil;
    }
    rb_hash_aset(window_obj, key_strut, strut_ary);
  }

  g_free(title_utf8);
  g_free(desktop);
  g_free(client_machine);
  g_free(class_out);
  return window_obj;
}

/*
  @overload list_windows(get_state = nil)

  Get list of information of windows.
  @param get_state [Boolean] If the value is true then we get some properties at the same time

  @return [Hash] An array of hashes of window information.
*/
static VALUE rb_wmctrl_list_windows (int argc, VALUE *argv, VALUE self) {
  Display **ptr, *disp;
  Window *client_list;
  Window window_active;
  unsigned long client_list_size;
  unsigned int i;
  int get_state;
  VALUE get_state_obj, window_ary;
  Data_Get_Struct(self, Display*, ptr);
  disp = *ptr;
  rb_scan_args(argc, argv, "01", &get_state_obj);
  get_state = RTEST(get_state_obj);

  if ((client_list = get_client_list(disp, &client_list_size)) == NULL) {
    /* return EXIT_FAILURE; */
    return Qfalse;
  }

  window_active = get_active_window(disp);
  window_ary = rb_ary_new2(client_list_size);

  for (i = 0; i < client_list_size / sizeof(Window); i++) {
    rb_ary_push(window_ary, get_window_hash_data(client_list[i], disp, window_active, get_state));
  }
  g_free(client_list);

  return window_ary;
}

/*
  @return [Hash] Hash of specified window data
*/
static VALUE rb_wmctrl_get_window_data (VALUE self, VALUE win_id_obj) {
  Display **ptr, *disp;
  Window win_id;
  win_id = (Window) NUM2LONG(win_id_obj);
  Data_Get_Struct(self, Display*, ptr);
  disp = *ptr;
  return get_window_hash_data(win_id, disp, -1, TRUE);
}

/*
  Get list of information of desktops.
  
  @return [Hash] An array of hashes of desktop information.
*/
static VALUE rb_wmctrl_list_desktops (VALUE self) {
  Display **ptr, *disp;
  unsigned long *num_desktops = NULL;
  unsigned long *cur_desktop = NULL;
  unsigned long desktop_list_size = 0;
  unsigned long *desktop_geometry = NULL;
  unsigned long desktop_geometry_size = 0;
  unsigned long *desktop_viewport = NULL;
  unsigned long desktop_viewport_size = 0;
  unsigned long *desktop_workarea = NULL;
  unsigned long desktop_workarea_size = 0;
  gchar *list = NULL;
  unsigned int i;
  unsigned int id;
  Window root;
  const gchar *error_message = NULL;
  VALUE ret = Qnil;
  VALUE *ret_arys;

  Data_Get_Struct(self, Display*, ptr);
  disp = *ptr;

  root = DefaultRootWindow(disp);

  if (! (num_desktops = (unsigned long *)get_property(disp, root,
						      XA_CARDINAL, "_NET_NUMBER_OF_DESKTOPS", NULL))) {
    if (! (num_desktops = (unsigned long *)get_property(disp, root,
							XA_CARDINAL, "_WIN_WORKSPACE_COUNT", NULL))) {
      error_message = "Cannot get number of desktops properties. (_NET_NUMBER_OF_DESKTOPS or _WIN_WORKSPACE_COUNT)";
      goto cleanup;
    }
  }

  if (! (cur_desktop = (unsigned long *)get_property(disp, root,
						     XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL))) {
    if (! (cur_desktop = (unsigned long *)get_property(disp, root,
						       XA_CARDINAL, "_WIN_WORKSPACE", NULL))) {
      error_message = "Cannot get current desktop properties. (_NET_CURRENT_DESKTOP or _WIN_WORKSPACE property)";
      goto cleanup;
    }
  }

  if ((list = get_property(disp, root, XInternAtom(disp, "UTF8_STRING", False),
			   "_NET_DESKTOP_NAMES", &desktop_list_size)) == NULL) {
    /* If charaset is not utf8 then there may be bugs here. */
    if ((list = get_property(disp, root,
			     XA_STRING,
			     "_WIN_WORKSPACE_NAMES", &desktop_list_size)) == NULL) {
      p_verbose("Cannot get desktop names properties. "
		"(_NET_DESKTOP_NAMES or _WIN_WORKSPACE_NAMES)"
		"\n");
      /* ignore the error - list the desktops without names */
    }
  }

  /* common size of all desktops */
  if (! (desktop_geometry = (unsigned long *)get_property(disp, DefaultRootWindow(disp),
							  XA_CARDINAL, "_NET_DESKTOP_GEOMETRY", &desktop_geometry_size))) {
    p_verbose("Cannot get common size of all desktops (_NET_DESKTOP_GEOMETRY).\n");
  }

  /* desktop viewport */
  if (! (desktop_viewport = (unsigned long *)get_property(disp, DefaultRootWindow(disp),
							  XA_CARDINAL, "_NET_DESKTOP_VIEWPORT", &desktop_viewport_size))) {
    p_verbose("Cannot get common size of all desktops (_NET_DESKTOP_VIEWPORT).\n");
  }

  /* desktop workarea */
  if (! (desktop_workarea = (unsigned long *)get_property(disp, DefaultRootWindow(disp),
							  XA_CARDINAL, "_NET_WORKAREA", &desktop_workarea_size))) {
    if (! (desktop_workarea = (unsigned long *)get_property(disp, DefaultRootWindow(disp),
							    XA_CARDINAL, "_WIN_WORKAREA", &desktop_workarea_size))) {
      p_verbose("Cannot get _NET_WORKAREA property.\n");
    }
  }

  ret_arys = (VALUE *)g_malloc0(*num_desktops * sizeof(VALUE));
  for (i = 0; i < *num_desktops; i++) {
    ret_arys[i] = rb_hash_new();
    rb_hash_aset(ret_arys[i], key_id, UINT2NUM(i));
    if (i == *cur_desktop) {
      rb_hash_aset(ret_arys[i], key_current, Qtrue);
    } else {
      rb_hash_aset(ret_arys[i], key_current, Qnil);
    }
  }

  if (list) {
    id = 0;
    rb_hash_aset(ret_arys[id], key_title, RB_UTF8_STRING_NEW2(list));
    id++;
    for (i = 0; i < desktop_list_size; i++) {
      if (list[i] == '\0') {
	if (id >= *num_desktops) {
	  break;
	}
	rb_hash_aset(ret_arys[id], key_title, RB_UTF8_STRING_NEW2(list + i + 1));
	id++;
      }
    }
  }

  /* prepare desktop geometry strings */
  if (desktop_geometry && desktop_geometry_size > 0) {
    if (desktop_geometry_size == 2 * sizeof(*desktop_geometry)) {
      /* only one value - use it for all desktops */
      p_verbose("WM provides _NET_DESKTOP_GEOMETRY value common for all desktops.\n");
      for (i = 0; i < *num_desktops; i++) {
	rb_hash_aset(ret_arys[i], key_geometry,
		     rb_ary_new3(2, INT2NUM(desktop_geometry[0]), INT2NUM(desktop_geometry[1])));
      }
    }
    else {
      /* seperate values for desktops of different size */
      p_verbose("WM provides separate _NET_DESKTOP_GEOMETRY value for each desktop.\n");
      for (i = 0; i < *num_desktops; i++) {
  	if (i < desktop_geometry_size / sizeof(*desktop_geometry) / 2) {
	  rb_hash_aset(ret_arys[i], key_geometry,
		       rb_ary_new3(2, INT2NUM(desktop_geometry[i*2]), INT2NUM(desktop_geometry[i*2+1])));
  	}
  	else {
	  rb_hash_aset(ret_arys[i], key_geometry, Qnil);
  	}
      }
    }
  }
  else {
    for (i = 0; i < *num_desktops; i++) {
      rb_hash_aset(ret_arys[i], key_geometry, Qnil);
    }
  }

  /* prepare desktop viewport strings */
  if (desktop_viewport && desktop_viewport_size > 0) {
    if (desktop_viewport_size == 2 * sizeof(*desktop_viewport)) {
      /* only one value - use it for current desktop */
      p_verbose("WM provides _NET_DESKTOP_VIEWPORT value only for the current desktop.\n");
      for (i = 0; i < *num_desktops; i++) {
  	if (i == *cur_desktop) {
	  rb_hash_aset(ret_arys[i], key_viewport,
		       rb_ary_new3(2, INT2NUM(desktop_viewport[0]), INT2NUM(desktop_viewport[1])));
  	}
  	else {
	  rb_hash_aset(ret_arys[i], key_viewport, Qnil);
  	}
      }
    }
    else {
      /* seperate values for each of desktops */
      for (i = 0; i < *num_desktops; i++) {
  	if (i < desktop_viewport_size / sizeof(*desktop_viewport) / 2) {
	  rb_hash_aset(ret_arys[i], key_viewport,
		       rb_ary_new3(2, INT2NUM(desktop_viewport[i*2]), INT2NUM(desktop_viewport[i*2+1])));
  	}
  	else {
	  rb_hash_aset(ret_arys[i], key_viewport, Qnil);
  	}
      }
    }
  }
  else {
    for (i = 0; i < *num_desktops; i++) {
      rb_hash_aset(ret_arys[i], key_viewport, Qnil);
    }
  }

  /* prepare desktop workarea strings */
  if (desktop_workarea && desktop_workarea_size > 0) {
    if (desktop_workarea_size == 4 * sizeof(*desktop_workarea)) {
      /* only one value - use it for current desktop */
      p_verbose("WM provides _NET_WORKAREA value only for the current desktop.\n");
      for (i = 0; i < *num_desktops; i++) {
  	if (i == *cur_desktop) {
	  rb_hash_aset(ret_arys[i], key_workarea,
		       rb_ary_new3(4, INT2NUM(desktop_workarea[0]), INT2NUM(desktop_workarea[1]),
				   INT2NUM(desktop_workarea[2]), INT2NUM(desktop_workarea[3])));
  	}
  	else {
	  rb_hash_aset(ret_arys[i], key_workarea, Qnil);
  	}
      }
    }
    else {
      /* seperate values for each of desktops */
      for (i = 0; i < *num_desktops; i++) {
  	if (i < desktop_workarea_size / sizeof(*desktop_workarea) / 4) {
	  rb_hash_aset(ret_arys[i], key_workarea,
		       rb_ary_new3(4, INT2NUM(desktop_workarea[i*4]), INT2NUM(desktop_workarea[i*4+1]),
				   INT2NUM(desktop_workarea[i*4+2]), INT2NUM(desktop_workarea[i*4+3])));
  	}
  	else {
	  rb_hash_aset(ret_arys[i], key_workarea, Qnil);
  	}
      }
    }
  }
  else {
    for (i = 0; i < *num_desktops; i++) {
      rb_hash_aset(ret_arys[i], key_workarea, Qnil);
    }
  }

  ret = rb_ary_new4(*num_desktops, ret_arys);

  p_verbose("Total number of desktops: %lu\n", *num_desktops);
  p_verbose("Current desktop ID (counted from zero): %lu\n", *cur_desktop);
  goto cleanup;

 cleanup:
  g_free(num_desktops);
  g_free(cur_desktop);
  g_free(desktop_geometry);
  g_free(desktop_viewport);
  g_free(desktop_workarea);
  g_free(list);
  if (error_message) {
    rb_raise(rb_eStandardError, "%s", error_message);
  }
  return ret;
}

/*
  @overload switch_desktop(desktop_id)
  
  Switch desktop.

  @param desktop_id [Integer] ID number of desktop.
  
  @return [Qtrue]
*/
static VALUE rb_wmctrl_switch_desktop (VALUE self, VALUE desktop_id) {
  int target;
  Display **ptr, *disp;
  Data_Get_Struct(self, Display*, ptr);
  disp = *ptr;

  target = FIX2INT(desktop_id);
  if (target < 0) {
    rb_raise(rb_eStandardError, "Invalid desktop ID: %d", target);
  }
  client_msg(disp, DefaultRootWindow(disp), "_NET_CURRENT_DESKTOP", (unsigned long)target, 0, 0, 0, 0);
  return Qtrue;
}

/*
  Get hash of information of window manager.
  
  @return [Hash]
*/
static VALUE rb_wmctrl_info (VALUE self) {
  Display **ptr, *disp;
  Window *sup_window = NULL;
  gchar *wm_name = NULL;
  gchar *wm_class = NULL;
  unsigned long *wm_pid = NULL;
  unsigned long *showing_desktop = NULL;
  gboolean name_is_utf8;
  VALUE ret = rb_hash_new();

  Data_Get_Struct(self, Display*, ptr);
  disp = *ptr;

  if (! (sup_window = (Window *)get_property(disp, DefaultRootWindow(disp),
					     XA_WINDOW, "_NET_SUPPORTING_WM_CHECK", NULL))) {
    if (! (sup_window = (Window *)get_property(disp, DefaultRootWindow(disp),
					       XA_CARDINAL, "_WIN_SUPPORTING_WM_CHECK", NULL))) {
      fputs("Cannot get window manager info properties.\n"
	    "(_NET_SUPPORTING_WM_CHECK or _WIN_SUPPORTING_WM_CHECK)\n", stderr);
      /* return EXIT_FAILURE; */
      return Qfalse;
    }
  }

  /* WM_NAME */
  name_is_utf8 = TRUE;
  if (! (wm_name = get_property(disp, *sup_window,
				XInternAtom(disp, "UTF8_STRING", False), "_NET_WM_NAME", NULL))) {
    name_is_utf8 = FALSE;
    if (! (wm_name = get_property(disp, *sup_window,
				  XA_STRING, "_NET_WM_NAME", NULL))) {
      p_verbose("Cannot get name of the window manager (_NET_WM_NAME).\n");
    }
  }
  if (wm_name) {
    rb_hash_aset(ret, key_name, (name_is_utf8 ? RB_UTF8_STRING_NEW2(wm_name) : rb_str_new(wm_name, strlen(wm_name))));
  }

  /* WM_CLASS */
  name_is_utf8 = TRUE;
  if (! (wm_class = get_property(disp, *sup_window,
				 XInternAtom(disp, "UTF8_STRING", False), "WM_CLASS", NULL))) {
    name_is_utf8 = FALSE;
    if (! (wm_class = get_property(disp, *sup_window,
				   XA_STRING, "WM_CLASS", NULL))) {
      p_verbose("Cannot get class of the window manager (WM_CLASS).\n");
    }
  }
  if (wm_class) {
    rb_hash_aset(ret, key_class, (name_is_utf8 ? RB_UTF8_STRING_NEW2(wm_class) : rb_str_new(wm_class, strlen(wm_class))));
  }

  /* WM_PID */
  if (! (wm_pid = (unsigned long *)get_property(disp, *sup_window,
						XA_CARDINAL, "_NET_WM_PID", NULL))) {
    p_verbose("Cannot get pid of the window manager (_NET_WM_PID).\n");
  }

  /* _NET_SHOWING_DESKTOP */
  if (! (showing_desktop = (unsigned long *)get_property(disp, DefaultRootWindow(disp),
							 XA_CARDINAL, "_NET_SHOWING_DESKTOP", NULL))) {
    p_verbose("Cannot get the _NET_SHOWING_DESKTOP property.\n");
  }

  rb_hash_aset(ret, key_pid, (wm_pid ? UINT2NUM(*wm_pid) : Qnil));
  rb_hash_aset(ret, key_showing_desktop,
	       (showing_desktop ? RB_UTF8_STRING_NEW2(*showing_desktop == 1 ? "ON" : "OFF") : Qnil));

  g_free(sup_window);
  g_free(wm_name);
  g_free(wm_class);
  g_free(wm_pid);
  g_free(showing_desktop);

  return ret;
}

/*
  @overload showing_desktop(state)
  
  Minimize windows to show desktop if the window manager implements "show the desktop" mode.
  
  @param state [boolean] We set true if we want to turn on showing desktop mode.
  
  @return [true]
*/
static VALUE rb_wmctrl_showing_desktop (VALUE self, VALUE state) {
  Display **ptr, *disp;
  Data_Get_Struct(self, Display*, ptr);
  disp = *ptr;
  client_msg(disp, DefaultRootWindow(disp), "_NET_SHOWING_DESKTOP", RTEST(state), 0, 0, 0, 0);
  return Qtrue;
}

/*
  @overload change_viewport(x, y)
  
  Change the viewport. A window manager may ignore this request.
  
  @param x [Integer] Offset of top position
  @param y [Integer] Offset of left position

  @return [true]
 */
static VALUE rb_wmctrl_change_viewport (VALUE self, VALUE xnum, VALUE ynum) {
  long x, y;
  Display **ptr, *disp;
  Data_Get_Struct(self, Display*, ptr);
  disp = *ptr;
  x = NUM2LONG(xnum);
  y = NUM2LONG(ynum);
  if (x < 0 || y < 0) {
    rb_raise(rb_eArgError, "Arguments must be nonnegative integers.");
  }
  client_msg(disp, DefaultRootWindow(disp), "_NET_DESKTOP_VIEWPORT", x, y, 0, 0, 0);
  return Qtrue;
}

/*
  @overload change_geometry(w, h)
  
  Change geometry of all desktops. A window manager may ignore this request.

  @param w [Ineteger] Width
  @param h [Ineteger] Height

  @return [true]
*/
static VALUE rb_wmctrl_change_geometry (VALUE self, VALUE xnum, VALUE ynum) {
  long x, y;
  Display **ptr, *disp;
  Data_Get_Struct(self, Display*, ptr);
  disp = *ptr;
  x = NUM2LONG(xnum);
  y = NUM2LONG(ynum);
  if (x < 0 || y < 0) {
    rb_raise(rb_eArgError, "Arguments must be nonnegative integers.");
  }
  client_msg(disp, DefaultRootWindow(disp), "_NET_DESKTOP_GEOMETRY", x, y, 0, 0, 0);
  return Qtrue;
}

/*
  @overload change_number_of_desktops(num)
  
  Change number of desktops.
  
  @param num [Integer] Number of desktops.

  @return [true]
*/
static VALUE rb_wmctrl_change_number_of_desktops (VALUE self, VALUE num) {
  long n;
  Display **ptr, *disp;
  Data_Get_Struct(self, Display*, ptr);
  disp = *ptr;
  n = NUM2LONG(num);
  if (n < 0) {
    rb_raise(rb_eArgError, "An argument must be a nonnegative integer.");
  }
  client_msg(disp, DefaultRootWindow(disp), "_NET_NUMBER_OF_DESKTOPS", n, 0, 0, 0, 0);
  return Qtrue;
}

static int activate_window (Display *disp, Window win, gboolean switch_desktop)
{
  unsigned long *desktop;

  /* desktop ID */
  if ((desktop = (unsigned long *)get_property(disp, win,
					       XA_CARDINAL, "_NET_WM_DESKTOP", NULL)) == NULL) {
    if ((desktop = (unsigned long *)get_property(disp, win,
						 XA_CARDINAL, "_WIN_WORKSPACE", NULL)) == NULL) {
      p_verbose("Cannot find desktop ID of the window.\n");
    }
  }

  if (switch_desktop && desktop) {
    if (client_msg(disp, DefaultRootWindow(disp),
		   "_NET_CURRENT_DESKTOP",
		   *desktop, 0, 0, 0, 0) != EXIT_SUCCESS) {
      p_verbose("Cannot switch desktop.\n");
    }
    g_free(desktop);
  }

  client_msg(disp, win, "_NET_ACTIVE_WINDOW", 0, 0, 0, 0, 0);
  XMapRaised(disp, win);

  return True;
}

static int close_window (Display *disp, Window win)
{
  return client_msg(disp, win, "_NET_CLOSE_WINDOW", 0, 0, 0, 0, 0);
}

static gboolean wm_supports (Display *disp, const gchar *prop)
{
  Atom xa_prop = XInternAtom(disp, prop, False);
  Atom *list;
  unsigned long size;
  unsigned int i;

  if (! (list = (Atom *)get_property(disp, DefaultRootWindow(disp),
				     XA_ATOM, "_NET_SUPPORTED", &size))) {
    p_verbose("Cannot get _NET_SUPPORTED property.\n");
    return FALSE;
  }

  for (i = 0; i < size / sizeof(Atom); i++) {
    if (list[i] == xa_prop) {
      g_free(list);
      return TRUE;
    }
  }

  g_free(list);
  return FALSE;
}

static int window_move_resize (Display *disp, Window win, signed long grav,
			       signed long x, signed long y, signed long w, signed long h)
{
  unsigned long grflags;
  grflags = grav;
  if (x != -1) grflags |= (1 << 8);
  if (y != -1) grflags |= (1 << 9);
  if (w != -1) grflags |= (1 << 10);
  if (h != -1) grflags |= (1 << 11);

  p_verbose("move_resize: %lu %ld %ld %ld %ld\n", grflags, x, y, w, h);

  if (wm_supports(disp, "_NET_MOVERESIZE_WINDOW")){
    return client_msg(disp, win, "_NET_MOVERESIZE_WINDOW",
		      grflags, (unsigned long)x, (unsigned long)y, (unsigned long)w, (unsigned long)h);
  }
  else {
    p_verbose("WM doesn't support _NET_MOVERESIZE_WINDOW. Gravity will be ignored.\n");
    if ((w < 1 || h < 1) && (x >= 0 && y >= 0)) {
      XMoveWindow(disp, win, x, y);
    }
    else if ((x < 0 || y < 0) && (w >= 1 && h >= -1)) {
      XResizeWindow(disp, win, w, h);
    }
    else if (x >= 0 && y >= 0 && w >= 1 && h >= 1) {
      XMoveResizeWindow(disp, win, x, y, w, h);
    }
    return EXIT_SUCCESS;
  }
}

static int window_state (Display *disp, Window win,
			 const char *action_str, const char *prop1_str, const char *prop2_str)
{
  unsigned long action;
  Atom prop1 = 0;
  Atom prop2 = 0;
  gchar *tmp_prop1, *tmp1;

  /* action */
  if (strcmp(action_str, "remove") == 0) {
    action = _NET_WM_STATE_REMOVE;
  }
  else if (strcmp(action_str, "add") == 0) {
    action = _NET_WM_STATE_ADD;
  }
  else if (strcmp(action_str, "toggle") == 0) {
    action = _NET_WM_STATE_TOGGLE;
  }
  else {
    rb_raise(rb_eArgError, "Invalid action. Use either remove, add or toggle.");
  }

  tmp_prop1 = g_strdup_printf("_NET_WM_STATE_%s",  tmp1 = g_ascii_strup(prop1_str, -1));
  p_verbose("State 1: %s\n", tmp_prop1);
  prop1 = XInternAtom(disp, tmp_prop1, False);
  g_free(tmp1);
  g_free(tmp_prop1);

  /* the second property */
  if (prop2_str) {
    gchar *tmp_prop2, *tmp2;
    tmp_prop2 = g_strdup_printf("_NET_WM_STATE_%s", tmp2 = g_ascii_strup(prop2_str, -1));
    p_verbose("State 2: %s\n", tmp_prop2);
    prop2 = XInternAtom(disp, tmp_prop2, False);
    g_free(tmp2);
    g_free(tmp_prop2);
  }
  return client_msg(disp, win, "_NET_WM_STATE", action, (unsigned long)prop1, (unsigned long)prop2, 0, 0);
}

static int window_to_desktop (Display *disp, Window win, int desktop)
{
  unsigned long *cur_desktop = NULL;
  Window root = DefaultRootWindow(disp);

  if (desktop == -1) {
    if (! (cur_desktop = (unsigned long *)get_property(disp, root,
						       XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL))) {
      if (! (cur_desktop = (unsigned long *)get_property(disp, root,
							 XA_CARDINAL, "_WIN_WORKSPACE", NULL))) {
	fputs("Cannot get current desktop properties. "
	      "(_NET_CURRENT_DESKTOP or _WIN_WORKSPACE property)"
	      "\n", stderr);
	return EXIT_FAILURE;
      }
    }
    desktop = *cur_desktop;
  }
  g_free(cur_desktop);

  return client_msg(disp, win, "_NET_WM_DESKTOP", (unsigned long)desktop, 0, 0, 0, 0);
}

static void window_set_title (Display *disp, Window win, char *title, char mode)
{
  gchar *title_utf8;
  gchar *title_local;

  if (envir_utf8) {
    title_utf8 = g_strdup(title);
    title_local = NULL;
  }
  else {
    if (! (title_utf8 = g_locale_to_utf8(title, -1, NULL, NULL, NULL))) {
      title_utf8 = g_strdup(title);
    }
    title_local = g_strdup(title);
  }

  if (mode == 'T' || mode == 'N') {
    /* set name */
    if (title_local) {
      XChangeProperty(disp, win, XA_WM_NAME, XA_STRING, 8, PropModeReplace,
		      (const unsigned char *)title_local, strlen(title_local));
    }
    else {
      XDeleteProperty(disp, win, XA_WM_NAME);
    }
    XChangeProperty(disp, win, XInternAtom(disp, "_NET_WM_NAME", False),
		    XInternAtom(disp, "UTF8_STRING", False), 8, PropModeReplace,
		    (const unsigned char *)title_utf8, strlen(title_utf8));
  }

  if (mode == 'T' || mode == 'I') {
    /* set icon name */
    if (title_local) {
      XChangeProperty(disp, win, XA_WM_ICON_NAME, XA_STRING, 8, PropModeReplace,
		      (const unsigned char *)title_local, strlen(title_local));
    }
    else {
      XDeleteProperty(disp, win, XA_WM_ICON_NAME);
    }
    XChangeProperty(disp, win, XInternAtom(disp, "_NET_WM_ICON_NAME", False),
		    XInternAtom(disp, "UTF8_STRING", False), 8, PropModeReplace,
		    (const unsigned char *)title_utf8, strlen(title_utf8));
  }

  g_free(title_utf8);
  g_free(title_local);
}

static int action_window (Display *disp, Window win, char mode, int argc, VALUE *argv)
{
  p_verbose("Using window: 0x%.8lx\n", win);
  switch (mode) {
  case 'a':
    if (argc == 0) {
      return activate_window(disp, win, TRUE);
    }
    break;
  case 'c':
    if (argc == 0) {
      return close_window(disp, win);
    }
    break;
  case 'e':
    /* resize/move the window around the desktop => -r -e */
    if (argc == 5) {
      int grav, x, y, w, h;
      grav = FIX2INT(argv[0]);
      x = FIX2INT(argv[1]);
      y = FIX2INT(argv[2]);
      w = FIX2INT(argv[3]);
      h = FIX2INT(argv[4]);
      if (grav >= 0) {
	return window_move_resize(disp, win, grav, x, y, w, h);
      }
    }
    break;
  case 'b':
    /* change state of a window => -r -b */
    if (argc == 2 || argc == 3) {
      const char *action, *prop1, *prop2;
      action = StringValuePtr(argv[0]);
      prop1 = StringValuePtr(argv[1]);
      if (argc == 3) {
	prop2 = StringValuePtr(argv[2]);	
      } else {
	prop2 = NULL;
      }
      return window_state(disp, win, action, prop1, prop2);
    }
    break;
  case 't':
    /* move the window to the specified desktop => -r -t */
    if (argc == 1) {
      return window_to_desktop(disp, win, FIX2INT(argv[0]));
    }
    break;
  case 'R':
    /* move the window to the current desktop and activate it => -r */
    if (argc == 0) {
      if (window_to_desktop(disp, win, -1)) {
	usleep(100000); /* 100 ms - make sure the WM has enough
			   time to move the window, before we activate it */
	return activate_window(disp, win, FALSE);
      }
      else {
	return False;
      }
    }
    break;
  case 'N': case 'I': case 'T':
    if (argc == 1) {
      window_set_title(disp, win, StringValuePtr(argv[0]), mode);
      return True;
    }
    break;
  }
  rb_raise(rb_eArgError, "Invalid argument of action_window.");
}

static Window Select_Window(Display *dpy)
{
  /*
   * Routine to let user select a window using the mouse
   * Taken from xfree86.
   */

  int status;
  Cursor cursor;
  XEvent event;
  Window target_win = None, root = DefaultRootWindow(dpy);
  int buttons = 0;
  int dummyi;
  unsigned int dummy;

  /* Make the target cursor */
  cursor = XCreateFontCursor(dpy, XC_crosshair);

  /* Grab the pointer using target cursor, letting it room all over */
  status = XGrabPointer(dpy, root, False,
			ButtonPressMask|ButtonReleaseMask, GrabModeSync,
			GrabModeAsync, root, cursor, CurrentTime);
  if (status != GrabSuccess) {
    fputs("ERROR: Cannot grab mouse.\n", stderr);
    return 0;
  }

  /* Let the user select a window... */
  while ((target_win == None) || (buttons != 0)) {
    /* allow one more event */
    XAllowEvents(dpy, SyncPointer, CurrentTime);
    XWindowEvent(dpy, root, ButtonPressMask|ButtonReleaseMask, &event);
    switch (event.type) {
    case ButtonPress:
      if (target_win == None) {
	target_win = event.xbutton.subwindow; /* window selected */
	if (target_win == None) target_win = root;
      }
      buttons++;
      break;
    case ButtonRelease:
      if (buttons > 0) /* there may have been some down before we started */
	buttons--;
      break;
    }
  }

  XUngrabPointer(dpy, CurrentTime);      /* Done with pointer */

  if (XGetGeometry (dpy, target_win, &root, &dummyi, &dummyi,
		    &dummy, &dummy, &dummy, &dummy) && target_win != root) {
    target_win = XmuClientWindow (dpy, target_win);
  }

  return(target_win);
}

static Window get_active_window(Display *disp)
{
  char *prop;
  unsigned long size;
  Window ret = (Window)0;

  prop = get_property(disp, DefaultRootWindow(disp), XA_WINDOW, "_NET_ACTIVE_WINDOW", &size);
  if (prop) {
    ret = *((Window*)prop);
    g_free(prop);
  }

  return(ret);
}

static Window get_target_window (Display *disp, VALUE obj)
{
  Window wid = 0;
  switch (TYPE(obj)) {
  case T_FIXNUM:
    wid = (Window) NUM2LONG(obj);
    break;
  /* case T_STRING: */
  /*   break; */
  case T_SYMBOL:
    {
      ID sym_id = SYM2ID(obj);
      if (sym_id == id_select) {
	wid = Select_Window(disp);
      } else if (sym_id == id_active) {
	wid = get_active_window(disp);
      }
    }
    break;
  }
  if (wid == 0) {
    rb_raise(rb_eArgError, "Invalid target window object. It must be an integer or :active, :select");
  }
  return wid;
}

/*
  Manage a window.
  @overload action_window(wid, :activate)
    Activate a window.
    @param [Integer] wid Window ID
    @return [boolean] true if succeeded. Otherwise, false.
  @overload action_window(wid, :close)
    Close a window
    @param [Integer] wid Window ID
    @return [boolean] true if succeeded. Otherwise, false.
  @overload action_window(wid, :move_resize, grav, x, y, w, h)
    @param [Integer] wid Window ID
    @param [Integer] grav Gravity
    @param [Integer] x    X coordinate
    @param [Integer] y    Y coordinate
    @param [Integer] w    Width
    @param [Integer] h    Height
    @return [boolean] true if succeeded. Otherwise, false.
  @overload action_window(wid, :change_state, "add", prop1, prop2 = nil)
    @param [Integer] wid Window ID
    @param [String] prop1 String of _NET_WM_STATE type.
    @param [String] prop2 String of _NET_WM_STATE type.
    @return [boolean] true if succeeded. Otherwise, false.
  @overload action_window(wid, :change_state, "remove", prop1, prop2 = nil)
    @param [Integer] wid Window ID
    @param [String] prop1 String of _NET_WM_STATE type.
    @param [String] prop2 String of _NET_WM_STATE type.
    @return [boolean] true if succeeded. Otherwise, false.
  @overload action_window(wid, :change_state, "toggle", prop1, prop2 = nil)
    @param [Integer] wid Window ID
    @param [String] prop1 String of _NET_WM_STATE type.
    @param [String] prop2 String of _NET_WM_STATE type.
    @return [boolean] true if succeeded. Otherwise, false.
  @overload action_window(wid, :move_to_desktop, desktop_id)
    @param [Integer] wid Window ID
    @param [Integer] desktop_id Desktop ID
    @return [boolean] true if succeeded. Otherwise, false.
  @overload action_window(wid, :move_to_current)
    @param [Integer] wid Window ID
    @return [boolean] true if succeeded. Otherwise, false.
  @overload action_window(wid, :set_title_long, str)
    @param [Integer] wid Window ID
    @return [boolean] true if succeeded. Otherwise, false.
  @overload action_window(wid, :set_title_short, str)
    @param [Integer] wid Window ID
    @return [boolean] true if succeeded. Otherwise, false.
  @overload action_window(wid, :set_title_both, str)
    @param [Integer] wid Window ID
    @return [boolean] true if succeeded. Otherwise, false.

  @note
    The arguments of prop1 and prop2 for :change_state command are strings
    that mean type of _NET_WM_STATE.
    For example, For example, we use "modal" or "MODAL" for _NET_WM_STATE_MODAL.
    See also http://standards.freedesktop.org/wm-spec/wm-spec-latest.html
*/
static VALUE rb_wmctrl_action_window(int argc, VALUE *argv, VALUE self) {
  Window wid;
  int mode;
  ID sym_id;
  Display **ptr, *disp;
  Data_Get_Struct(self, Display*, ptr);
  disp = *ptr;
  if (argc < 2) {
    rb_raise(rb_eArgError, "Need more than one argument.");
  }
  wid = get_target_window(disp, argv[0]);
  sym_id = SYM2ID(argv[1]);
  if (sym_id == id_activate) {
    mode = 'a';
  } else if (sym_id == id_close) {
    mode = 'c';
  } else if (sym_id == id_move_resize) {
    mode = 'e';
  } else if (sym_id == id_change_state) {
    mode = 'b';
  } else if (sym_id == id_move_to_desktop) {
    mode = 't';
  } else if (sym_id == id_move_to_current) {
    mode = 'R';
  } else if (sym_id == id_set_title_long) {
    mode = 'N';
  } else if (sym_id == id_set_title_short) {
    mode = 'I';
  } else if (sym_id == id_set_title_both) {
    mode = 'T';
  } else {
    rb_raise(rb_eStandardError, "Invalid argument of action_window.");
  }
  if (action_window(disp, wid, mode, argc - 2, (argv + 2))) {
    return Qtrue;
  } else {
    return Qfalse;
  }
}

/*
  Get an array of _NET_SUPPORTED property.

  @return [Array] An array of strings.
 */
static VALUE rb_wmctrl_supported (VALUE self)
{
  Atom *list;
  unsigned long size;
  unsigned int i;
  gchar *prop_name;
  VALUE ret;
  Display **ptr, *disp;
  Data_Get_Struct(self, Display*, ptr);
  disp = *ptr;

  
  if (!(list = (Atom *)get_property(disp, DefaultRootWindow(disp), XA_ATOM, "_NET_SUPPORTED", &size))) {
    rb_raise(rb_eStandardError, "Cannot get _NET_SUPPORTED property.");
  }

  ret = rb_ary_new();
  for (i = 0; i < size / sizeof(Atom); i++) {
    prop_name = XGetAtomName(disp, list[i]);
    rb_ary_push(ret, rb_str_new2(prop_name));
    g_free(prop_name);
  }
  g_free(list);
  return ret;
}

void Init_wmctrl()
{
  rb_wmctrl_class = rb_define_class("WMCtrl", rb_cObject);

  rb_define_alloc_func(rb_wmctrl_class, rb_wmctrl_alloc);
  rb_define_private_method(rb_wmctrl_class, "initialize", rb_wmctrl_initialize, -1);

  rb_define_method(rb_wmctrl_class, "list_windows", rb_wmctrl_list_windows, -1);
  rb_define_method(rb_wmctrl_class, "get_window_data", rb_wmctrl_get_window_data, 1);
  rb_define_method(rb_wmctrl_class, "list_desktops", rb_wmctrl_list_desktops, 0);
  rb_define_method(rb_wmctrl_class, "switch_desktop", rb_wmctrl_switch_desktop, 1);
  rb_define_method(rb_wmctrl_class, "info", rb_wmctrl_info, 0);
  rb_define_method(rb_wmctrl_class, "showing_desktop", rb_wmctrl_showing_desktop, 1);
  rb_define_method(rb_wmctrl_class, "change_viewport", rb_wmctrl_change_viewport, 2);
  rb_define_method(rb_wmctrl_class, "change_geometry", rb_wmctrl_change_geometry, 2);
  rb_define_method(rb_wmctrl_class, "change_number_of_desktops", rb_wmctrl_change_number_of_desktops, 1);
  rb_define_method(rb_wmctrl_class, "action_window", rb_wmctrl_action_window, -1);
  rb_define_method(rb_wmctrl_class, "supported", rb_wmctrl_supported, 0);

  key_id = ID2SYM(rb_intern("id"));
  key_title = ID2SYM(rb_intern("title"));
  key_pid = ID2SYM(rb_intern("pid"));
  key_geometry = ID2SYM(rb_intern("geometry"));
  key_active = ID2SYM(rb_intern("active"));
  key_class = ID2SYM(rb_intern("class"));
  key_client_machine = ID2SYM(rb_intern("client_machine"));
  key_desktop = ID2SYM(rb_intern("desktop"));
  key_viewport = ID2SYM(rb_intern("viewport"));
  key_workarea = ID2SYM(rb_intern("workarea"));
  key_current = ID2SYM(rb_intern("current"));
  key_showing_desktop = ID2SYM(rb_intern("showing_desktop"));
  key_name = ID2SYM(rb_intern("name"));
  key_state = ID2SYM(rb_intern("state"));
  key_window_type = ID2SYM(rb_intern("window_type"));
  key_frame_extents = ID2SYM(rb_intern("frame_extents"));
  key_strut = ID2SYM(rb_intern("strut"));
  key_exterior_frame = ID2SYM(rb_intern("exterior_frame"));

  id_active = rb_intern("active");
  id_select = rb_intern("select");
  id_activate = rb_intern("activate");
  id_close = rb_intern("close");
  id_move_resize = rb_intern("move_resize");
  id_change_state = rb_intern("change_state");
  id_move_to_desktop = rb_intern("move_to_desktop");
  id_move_to_current = rb_intern("move_to_current");
  id_set_title_long = rb_intern("set_title_long");
  id_set_title_short = rb_intern("set_title_short");
  id_set_title_both = rb_intern("set_title_both");
}

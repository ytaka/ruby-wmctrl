require 'mkmf'
require 'pkg-config'

$CFLAGS << " -Wall"

glib = PKGConfig.package_config("glib-2.0")
$CFLAGS << " " << glib.cflags
$libs << " " << glib.libs

dir_config("X11")
if have_header('X11/Xlib.h') && have_library('X11') && 
   have_header('X11/Xmu/WinUtil.h') && have_library('Xmu')
  create_makefile("wmctrl")
end

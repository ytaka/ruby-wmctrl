# ruby-wmctrl

Ruby bindings to control windows in EWMH and NetWM compatible X Window manager.

- <http://rubygems.org/gems/ruby-wmctrl>
- <https://github.com/ytaka/ruby-wmctrl>

## Installation

ruby-wmctrl is a C extended library, which uses glib and x11,
and is not a program to parse the output of command "wmctrl".

On ubuntu 11.04, we should install the following packages.

    apt-get install libx11-dev libglib2.0-dev libxmu-dev

Then we can install ruby-wmctrl from rubygems.

    gem install ruby-wmctrl

## Remark

Note that values of positions of windows aredifferent from
those of original "wmctrl" and include frame sizes.

## Usage

We load 'wmctrl' as below;

    require 'wmctrl'

We get desktops and windows as follows;

    pp WMCtrl.instance.desktops
    pp WMCtrl.instance.windows

We can specify search conditions of windows; for example,

    pp WMCtrl.instance.windows(:wm_class => /^emacs/)

## Usage (Basic method similar to the command "wmctrl")

ruby-wmctrl has some basic methods derived from wmctrl,
but the author think that these methods may be changed in future release.

### List windows

    require 'wmctrl'
    require 'pp'
    wm = WMCtrl.instance
    pp wm.list_windows

If you want to get properties of windows, you pass true as an argument.

    pp wm.list_windows(true)

### Activate a window

    wm.action_window(window_id, :activate)

The method 'action\_window' takes window ID as first argument.
We can get window ID by the method 'list\_windows'.
The second argument of 'action\_window' is an action that
manages the window of window ID.
The rest arguments are arguments of the action.

### Close a window

    wm.action_window(window_id, :close)

### Move and resize a window

    wm.action_window(window_id, :move_resize, 0, 100, 200, 500, 400)

The integers of the arguments means gravity, x coordinate, y coordinate,
width, and height, respectively. That is,

    wm.action_window(window_id, :move_resize, gravity, x, y, width, height)

### Move window to a desktop

    wm.action_window(window_id, :move_to_desktop, desktop_id)

### Move wndow to current desktop

    wm.action_window(window_id, :move_to_current)

### List desktops

    pp wm.list_desktops

## rwmctrl

ruby-wmctrl includes the command "rwmctrl", which imitates the command "wmctrl"
but does not implement all functionalities of "wmctrl".

## License

GPLv2

## Copyright

### wmctrl

ruby-wmctrl is created from source code of wmctrl <http://sweb.cz/tripie/utils/wmctrl/>.

The copyright of original wmctrl:

Author, current maintainer: Tomas Styblo tripie@cpan.org
Copyright (C) 2003

### ruby-wmctrl

Takayuki YAMAGUCHI d@ytak.info Copyright (C) 2011

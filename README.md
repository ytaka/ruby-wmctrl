# ruby-wmctrl

Ruby bindings to control windows in EWMH and NetWM compatible X Window manager.

- <http://rubygems.org/gems/ruby-wmctrl>
- <https://github.com/ytaka/ruby-wmctrl>

## Installation

ruby-wmctrl is a C extended library, which uses glib and x11.
For ubuntu 11.04, we should install packages.

    apt-get install libx11-dev libglib2.0-dev libxmu-dev

Then we can install ruby-wmctrl from rubygems.

    gem install ruby-wmctrl

## Usage

We load 'wmctrl' as below

    require 'wmctrl'

and call methods of an instance of WMCtrl.

### List windows

    require 'wmctrl'
    require 'pp'
    wm = WMCtrl.new
    pp wm.list_windows

### Activate a window

    wm.action_window(window_id, :activate)

The method 'action_window' takes window ID as first argument.
We can get window ID by the method 'list_windows'.
The second argument of 'action_window' is an action that
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

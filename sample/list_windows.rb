require 'wmctrl'
require 'pp'

wm = WMCtrl.new
pp wm.list_windows

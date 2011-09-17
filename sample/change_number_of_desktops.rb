require 'wmctrl'
require 'pp'

wm = WMCtrl.new
pp wm.change_number_of_desktops(2)

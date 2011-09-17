$:.unshift(File.expand_path(File.join(File.dirname(__FILE__), "../ext")))
require 'wmctrl'
require 'pp'

wm = WMCtrl.new
pp wm.change_number_of_desktops(2)

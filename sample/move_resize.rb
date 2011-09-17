$:.unshift(File.expand_path(File.join(File.dirname(__FILE__), "../ext")))
require 'wmctrl'
require 'pp'

wm = WMCtrl.new
wm.action_window(ARGV[0].to_i, :move_resize, 0, 100, 200, 500, 400)

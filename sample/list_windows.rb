$:.unshift(File.expand_path(File.join(File.dirname(__FILE__), "../ext")))
require 'wmctrl'
require 'pp'

wm = WMCtrl.new
pp wm.list_windows

puts "---"

pp wm.list_windows(true)

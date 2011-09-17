$:.unshift(File.expand_path(File.join(File.dirname(__FILE__), "../ext")))
require 'wmctrl'
require 'pp'

wm = WMCtrl.new
pp wm.supported.sort

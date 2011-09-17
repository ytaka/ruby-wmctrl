$:.unshift(File.expand_path(File.join(File.dirname(__FILE__), "../ext")))
require 'wmctrl'
require 'pp'

fork do
  wm = WMCtrl.new
  pp wm.switch_desktop(1)
end

sleep(3)

wm = WMCtrl.new
pp wm.switch_desktop(0)

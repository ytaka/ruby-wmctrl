$:.unshift(File.expand_path(File.join(File.dirname(__FILE__), "../ext")))
require 'wmctrl'
require 'pp'

fork do
  wm = WMCtrl.new
  wm.showing_desktop(true)
end

sleep(3)

wm = WMCtrl.new
pp wm.showing_desktop(false)

require 'wmctrl'
require 'pp'

fork do
  wm = WMCtrl.new
  pp wm.switch_desktop(1)
end

sleep(3)

wm = WMCtrl.new
pp wm.switch_desktop(0)

require 'wmctrl'
require 'pp'

fork do
  wm = WMCtrl.new
  wm.showing_desktop(true)
end

sleep(3)

wm = WMCtrl.new
pp wm.showing_desktop(false)

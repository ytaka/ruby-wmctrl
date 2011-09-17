require 'wmctrl'
require 'pp'

pid = Process.spawn('zenity --calendar')

sleep(0.1)

wm = WMCtrl.new

win = wm.list_windows.find do |h|
  h[:pid] == pid
end

sleep(1)

wm.action_window(win[:id], :move_to_desktop, 1)

sleep(1)

wm.action_window(win[:id], :move_to_current)

puts "Close calendars after 1 second"
sleep(1)
wm.action_window(win[:id], :close)

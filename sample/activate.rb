require 'wmctrl'
require 'pp'

pid1 = Process.spawn('zenity --calendar')
pid2 = Process.spawn('zenity --calendar')

sleep(0.1)

wm = WMCtrl.new

pp wm.list_windows
win1 = wm.list_windows.find do |h|
  h[:pid] == pid1
end
win2 = wm.list_windows.find do |h|
  h[:pid] == pid2
end

wm.action_window(win1[:id], :move_resize, 0, 0, 0, -1, -1)
wm.action_window(win2[:id], :move_resize, 0, 300, 0, -1, -1)

wm.action_window(win1[:id], :activate)
puts "Activate another calendar after 1 second"
sleep(1)
wm.action_window(win2[:id], :activate)

puts "Close calendars after 1 second"
sleep(1)
wm.action_window(win1[:id], :close)
wm.action_window(win2[:id], :close)

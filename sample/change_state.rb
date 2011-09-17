$:.unshift(File.expand_path(File.join(File.dirname(__FILE__), "../ext")))
require 'wmctrl'
require 'pp'

pid = Process.spawn('zenity --calendar')

sleep(0.1)

wm = WMCtrl.new

win = wm.list_windows.find do |h|
  h[:pid] == pid
end

p win

sleep(1)

wm.action_window(win[:id], :change_state, "add", "maximized_vert")

sleep(1)

wm.action_window(win[:id], :change_state, "remove", "maximized_vert")

sleep(1)

wm.action_window(win[:id], :change_state, "toggle", "maximized_horz", "maximized_vert")

sleep(1)

wm.action_window(win[:id], :change_state, "toggle", "maximized_vert")

puts "Close calendars after 1 second"
sleep(1)
wm.action_window(win[:id], :close)

require 'wmctrl'
require 'pp'

pid = Process.spawn('zenity --calendar')

sleep(0.1)

wm = WMCtrl.new

win = wm.list_windows.find do |h|
  h[:pid] == pid
end

wm.action_window(win[:id], :set_title_long, "title long")
wm.action_window(win[:id], :set_title_short, "title short")

sleep(3)

wm.action_window(win[:id], :set_title_both, "title both") # This method is ignored?

puts "Close calendars after 3 second"
sleep(3)
wm.action_window(win[:id], :close)

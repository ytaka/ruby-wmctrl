require 'wmctrl'
require 'pp'

pid = Process.spawn('zenity --calendar')

sleep(0.1)

wm = WMCtrl.new
win = wm.list_windows.find do |h|
  h[:pid] == pid
end

pp win
puts "Close after 1 second"
sleep(1)
wm.action_window(win[:id], :close)

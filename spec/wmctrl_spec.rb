require "spec_helper"

describe WMCtrl do
  before(:all) do
    @wm = WMCtrl.new
  end

  let :wm do
    @wm
  end

  it "should get an array of hashes." do
    wins = wm.list_windows
    wins.should be_an_instance_of Array
    wins.each do |w|
      w.should be_an_instance_of Hash
    end
  end

  it "should get an array of hashes." do
    desktops = wm.list_desktops
    desktops.should be_an_instance_of Array
    desktops.each do |w|
      w.should be_an_instance_of Hash
    end
  end

  it "should get information of window manager." do
    wm.info.should be_an_instance_of Hash
  end

  it "should get an array." do
    sp = wm.supported
    sp.should be_an_instance_of Array
    sp.each do |prop|
      prop.should be_an_instance_of String
    end
  end

  # action_window(wid, cmd, *args)
  # change_geometry(w, h)
  # change_number_of_desktops(num)
  # change_viewport(x, y)
  # showing_desktop(state)
  # switch_desktop(desktop_id)
end

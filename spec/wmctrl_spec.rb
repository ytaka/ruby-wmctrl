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
    expect(wins).to be_an_instance_of Array
    wins.each do |w|
      expect(w).to be_an_instance_of Hash
    end
  end

  it "should get an array of hashes." do
    desktops = wm.list_desktops
    expect(desktops).to be_an_instance_of Array
    desktops.each do |w|
      expect(w).to be_an_instance_of Hash
    end
  end

  it "should get information of window manager." do
    expect(wm.info).to be_an_instance_of Hash
  end

  it "should get an array." do
    sp = wm.supported
    expect(sp).to be_an_instance_of Array
    sp.each do |prop|
      expect(prop).to be_an_instance_of String
    end
  end

  # action_window(wid, cmd, *args)
  # change_geometry(w, h)
  # change_number_of_desktops(num)
  # change_viewport(x, y)
  # showing_desktop(state)
  # switch_desktop(desktop_id)
end

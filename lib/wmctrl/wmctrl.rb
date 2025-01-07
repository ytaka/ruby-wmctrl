require 'wmctrl.so'

class WMCtrl
  NOT_USE_NET_MOVERESIZE_WINDOW = -1
  GRAVITY_DEFAULT = 0
  GRAVITY_NORTH_WEST = 1
  GRAVITY_NORTH = 2
  GRAVITY_NORTH_EAST = 3
  GRAVITY_WEST = 4
  GRAVITY_CENTER = 5
  GRAVITY_EAST = 6
  GRAVITY_SOUTH_WEST = 7
  GRAVITY_SOUTH = 8
  GRAVITY_SOUTH_EAST = 9
  GRAVITY_STATIC = 10

  @wmctrl = {}

  def self.instance
    STDERR.puts "WMCtrl.instance is deprecated. Please use WMCtrl.display alternatively."
    self.display
  end

  def self.display(dpy = nil)
    dpy ||= ENV["DISPLAY"]
    unless display = @wmctrl[dpy]
      display = self.new(dpy)
      @wmctrl[dpy] = display
    end
    display
  end

  class DataHash
    def initialize(data_hash)
      @data = data_hash
    end

    def method_missing(*args, &block)
      @data.__send__(*args, &block)
    end
  end

  class Desktop < DataHash
    # Valid keys of the method [] are :id, :current, :title, :geometry, :viewport, and :workarea.

    [:id, :current, :title, :geometry, :viewport, :workarea].each do |key|
      define_method(key) do
        self[key]
      end
    end

    def geometry_width
      self[:geometry][0]
    end

    def geometry_height
      self[:geometry][1]
    end

    # @return [Integer] X coordinate of Top left corner of workarea
    def workarea_x
      self[:workarea][0]
    end

    # @return [Integer] Y coordinate of Top left corner of workarea
    def workarea_y
      self[:workarea][1]
    end

    # @return [Integer] Width of workarea
    def workarea_width
      self[:workarea][2]
    end

    # @return [Integer] Height of workarea
    def workarea_height
      self[:workarea][3]
    end
  end

  class Window < DataHash
    [:id, :title, :active, :desktop, :client_machine,
     :pid, :geometry, :state, :exterior_frame, :frame_extents, :strut, :display].each do |key|
      define_method(key) do
        self[key]
      end
    end

    # Because method name "class" is a basic ruby method, we use "wm_class" to get window class.
    def wm_class
      self[:class]
    end

    # @param [Symbol] key Valid keys are :class, :id, :title, :active, :desktop, :client_machine,
    #   :pid, :geometry, :state, :exterior_frame, :frame_extents, and :strut.
    #   :wm_class is an alias of :class.
    def [](key)
      if key == :wm_class
        self.wm_class
      else
        super(key)
      end
    end

    def sticky?
      self[:desktop] == -1
    end

    def active?
      self[:active]
    end

    def fullscreen?
      self[:state] && self[:state].include?("_NET_WM_STATE_FULLSCREEN")
    end

    def modal?
      self[:state] && self[:state].include?("_NET_WM_STATE_MODAL")
    end

    def maximized_vertically?
      self[:state] && self[:state].include?("_NET_WM_STATE_MAXIMIZED_VERT")
    end

    def maximized_horizontally?
      self[:state] && self[:state].include?("_NET_WM_STATE_MAXIMIZED_HORZ")
    end

    def shaded?
      self[:state] && self[:state].include?("_NET_WM_STATE_SHADED")
    end

    def hidden?
      self[:state] && self[:state].include?("_NET_WM_STATE_HIDDEN")
    end

    def skipped_by_taskbar?
      self[:state] && self[:state].include?("_NET_WM_STATE_SKIP_TASKBAR")
    end

    def skipped_by_pager?
      self[:state] && self[:state].include?("_NET_WM_STATE_SKIP_PAGER")
    end

    def action(*args)
      wmctrl = WMCtrl.display(self[:display])
      wmctrl.action_window(self[:id], *args)
      @data = wmctrl.get_window_data(self[:id])
      self
    end
    private :action

    def close
      action(:close)
    end

    def change_state(*args)
      action(:change_state, *args)
    end

    def activate
      action(:activate)
    end

    # @param [Hash] opts Options
    # @option opts [Integer] :x X coordinate
    # @option opts [Integer] :y Y coordinate
    # @option opts [Integer] :width Width of window
    # @option opts [Integer] :height Height of window
    # @option opts [Integer,:current] :desktop Desktop number
    # @option opts [Integer] :gravity Number meaning gravity: NOT_USE_NET_MOVERESIZE_WINDOW,
    #   GRAVITY_DEFAULT, GRAVITY_NORTH_WEST, GRAVITY_NORTH, GRAVITY_NORTH_EAST, GRAVITY_WEST, GRAVITY_CENTER,
    #   GRAVITY_EAST, GRAVITY_SOUTH_WEST, GRAVITY_SOUTH, GRAVITY_SOUTH_EAST, GRAVITY_STATIC
    def place(opts = {})
      if opts[:desktop]
        if opts[:desktop] == :current
          action(:move_to_current)
        elsif (opts[:desktop] != self[:desktop])
          action(:move_to_desktop, opts[:desktop])
        end
      end
      gravity = opts[:gravity] || GRAVITY_DEFAULT
      x = self[:exterior_frame][0]
      y = self[:exterior_frame][1]
      width = self[:exterior_frame][2]
      height = self[:exterior_frame][3]
      extent_horizontal = self[:frame_extents][0] + self[:frame_extents][1]
      extent_vertical = self[:frame_extents][2] + self[:frame_extents][3]
      width = opts[:width] if opts[:width] && (opts[:width] != width)
      height = opts[:height] if opts[:height] && (opts[:height] != height)
      x = opts[:x] if opts[:x] && (opts[:x] != x)
      y = opts[:y] if opts[:y] && (opts[:y] != y)
      action(:move_resize, gravity, x + self[:frame_extents][0], y + self[:frame_extents][1], width - extent_horizontal, height - extent_vertical)
    end

    def position
      a = self[:exterior_frame]
      { :x => a[0], :y => a[1], :width => a[2], :height => a[3] }
    end
  end

  # @return [Array] An array of WMCtrl::Desktop
  def desktops
    list_desktops.map { |hash| WMCtrl::Desktop.new(hash) }
  end

  # @overload windows(*conditions, opts = {}, &block)
  # @param [Array] conditions An array of condition hash.
  #   The keys of the hash are keys of WMCtrl::Window#[].
  #   The values of the hash are tested by the method ===.
  #   Keys of each condition are combined by logical product and
  #   all conditions are combined by logical sum.
  # @param [Hash] opts Options
  # @option opts [Integer] :order Order of windows: :mapping or :stacking
  # @yield [WMCtrl::Window] Test window by block
  # @return [Array] An array of WMCtrl::Window including windows
  #   matched by the conditions and the block.
  def windows(*conditions, &block)
    cond_last = conditions.last || {}
    if cond_last[:order] && (cond_last[:order] == :stacking)
      conditions.pop
      wins = list_windows(true, true)
    else
      wins = list_windows(true, nil)
    end
    wins.map! { |h| WMCtrl::Window.new(h) }
    unless conditions.empty?
      wins_selected = []
      conditions.each do |condition|
        wins_rest = []
        wins.each do |win|
          if condition.all? { |k, v| v === win[k] }
            wins_selected << win
          else
            wins_rest << win
          end
        end
        wins = wins_rest
      end
      wins = wins_selected
    end
    if block_given?
      wins.delete_if { |win| !yield(win) }
    end
    wins
  end

  WAIT_CHANGE_STACKING_ORDER = 0.01

  # @param [Hash] opts Options
  # @option opts [Float] :waiting_time Time to wait restack request
  # @option opts [String] :display Display name
  def restack(wins_bottom_to_top, opts = {})
    win_upper = nil
    waiting_time = opts[:sleep] || WAIT_CHANGE_STACKING_ORDER
    wins_bottom_to_top.reverse.each do |win|
      next if win.sticky?
      if win_upper
        # 1 means below
        WMCtrl.display(opts[:display]).client_msg(win.id, "_NET_RESTACK_WINDOW", 2, win_upper.id, 1, 0, 0)
        # If there is no sleep, change the stacking order fails.
        # Can we use _NET_WM_SYNC_REQUEST?
        sleep(waiting_time)
      end
      win_upper = win
    end
  end
end

require 'wmctrl.so'

class WMCtrl
  def self.instance
    @wmctrl ||= self.new
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
     :pid, :geometry, :state, :exterior_frame, :frame_extents, :strut].each do |key|
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

    STATE_FULLSCREEN = "_NET_WM_STATE_FULLSCREEN"

    def fullscreen?
      self[:state].include?(STATE_FULLSCREEN)
    end

    def action(*args)
      WMCtrl.instance.action_window(self[:id], *args)
      @data = WMCtrl.instance.get_window_data(self[:id])
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
    def place(opts = {})
      if opts[:desktop]
        if opts[:desktop] == :current
          action(:move_to_current)
        elsif (opts[:desktop] != self[:desktop])
          action(:move_to_desktop, opts[:desktop])
        end
      end
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
      action(:move_resize, 0, x + self[:frame_extents][0], y + self[:frame_extents][1], width - extent_horizontal, height - extent_vertical)
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

  # @param [Array] conditions An array of condition hash. The values of hash are tested by the method ===.
  # @return [Array] An array of WMCtrl::Window
  def windows(*conditions)
    wins = list_windows(true).map { |hash| WMCtrl::Window.new(hash) }
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
    wins
  end
end

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
    [:id, :current, :title, :geometry, :viewport, :workarea].each do |key|
      define_method(key) do
        self[key]
      end
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

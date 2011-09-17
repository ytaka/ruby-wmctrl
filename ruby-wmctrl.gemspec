# -*- encoding: utf-8 -*-
$:.push File.expand_path("../lib", __FILE__)
require "wmctrl/version"

Gem::Specification.new do |s|
  s.name        = "ruby-wmctrl"
  s.version     = Ruby::WMCtrl::VERSION
  s.authors     = ["Takayuki YAMAGUCHI"]
  s.email       = ["d@ytak.info"]
  s.homepage    = ""
  s.summary     = "Ruby binding to control windows"
  s.description = "Ruby binding to control windows in EWMH and NetWM compatible X Window manager, which is created from source code of wmctrl command."

  s.rubyforge_project = "ruby-wmctrl"

  s.files         = `git ls-files`.split("\n")
  s.test_files    = `git ls-files -- {test,spec,features}/*`.split("\n")
  s.executables   = `git ls-files -- bin/*`.split("\n").map{ |f| File.basename(f) }
  s.require_paths = ["lib", "ext"]
  s.extensions    = Dir.glob("ext/**/extconf.rb")

  # specify any dependencies here; for example:
  s.add_development_dependency "rspec"
  s.add_development_dependency "yard"
  s.add_runtime_dependency "pkg-config"
end

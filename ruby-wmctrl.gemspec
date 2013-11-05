# -*- encoding: utf-8 -*-
$:.push File.expand_path("../lib", __FILE__)
require "wmctrl/version"

Gem::Specification.new do |spec|
  spec.name        = "ruby-wmctrl"
  spec.version     = WMCtrl::VERSION
  spec.authors     = ["Takayuki YAMAGUCHI"]
  spec.email       = ["d@ytak.info"]
  spec.homepage    = ""
  spec.summary     = "Ruby bindings to control windows"
  spec.description = "Ruby bindings to control windows in EWMH and NetWM compatible X Window manager, which is created from source code of wmctrl command."
  spec.license = "GPLv2"

  spec.files         = `git ls-files`.split("\n")
  spec.test_files    = `git ls-files -- {test,spec,features}/*`.split("\n")
  spec.executables   = `git ls-files -- bin/*`.split("\n").map{ |f| File.basename(f) }
  spec.require_paths = ["lib", "ext"]
  spec.extensions    = Dir.glob("ext/**/extconf.rb")

  # specify any dependencies here; for example:
  spec.add_development_dependency "rspec"
  spec.add_development_dependency "yard"
  spec.add_runtime_dependency "pkg-config"
end

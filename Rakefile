require "bundler/gem_tasks"

require 'yard'
YARD::Rake::YardocTask.new

def each_extconf_directory(&block)
  Dir.glob("ext/**/extconf.rb").each do |extconf|
    cd File.dirname(extconf) do
      yield
    end
  end
end

desc 'Compile extended library'
task 'ext:compile' do |t|
  each_extconf_directory do
    sh 'ruby extconf.rb' unless File.exist?('Makefile')
    sh 'make'
  end
end

desc 'Compile extended library'
task 'ext:recompile' do |t|
  each_extconf_directory do
    sh 'make distclean' if File.exist?('Makefile')
    sh 'ruby extconf.rb'
    sh 'make'
  end
end

desc 'Clean'
task 'ext:clean' do |t|
  each_extconf_directory do
    sh 'make clean' if File.exist?('Makefile')
  end
end

desc 'Clean completely'
task 'ext:distclean' do |t|
  each_extconf_directory do
    sh 'make distclean' if File.exist?('Makefile')
  end
end

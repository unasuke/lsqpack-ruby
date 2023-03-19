# frozen_string_literal: true

require "bundler/gem_tasks"
require "rake/extensiontask"
require "rake/testtask"

task build: :compile

Rake::ExtensionTask.new("lsqpack") do |ext|
  ext.lib_dir = "lib/lsqpack"
end

Rake::TestTask.new(:test) do |t|
  t.libs << "test"
  t.libs << "lib"
  t.test_files = FileList["test/**/test_*.rb"]
end

task default: %i[clobber compile test]

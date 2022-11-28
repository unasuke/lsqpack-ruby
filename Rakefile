# frozen_string_literal: true

require "bundler/gem_tasks"
require "rake/extensiontask"

task build: :compile

Rake::ExtensionTask.new("lsqpack") do |ext|
  ext.lib_dir = "lib/lsqpack"
end

task default: %i[clobber compile]

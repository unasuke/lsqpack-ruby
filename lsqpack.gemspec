# frozen_string_literal: true

require_relative "lib/lsqpack/version"

Gem::Specification.new do |spec|
  spec.name = "lsqpack"
  spec.version = Lsqpack::VERSION
  spec.authors = ["Yusuke Nakamura"]
  spec.email = ["yusuke1994525@gmail.com"]

  spec.summary = "A wrapper gem of the ls-qpack library. It strongly influenced by pylsqpack."
  spec.description = "A wrapper gem of the ls-qpack library. It strongly influenced by pylsqpack. It has same api of pylsqpack."
  spec.homepage = "https://github.com/unasuke/lsqpack-ruby"
  spec.required_ruby_version = ">= 2.6.0"
  spec.license = ["MIT"]

  spec.metadata["homepage_uri"] = spec.homepage
  spec.metadata["source_code_uri"] = spec.homepage
  # spec.metadata["changelog_uri"] = "Put your gem's CHANGELOG.md URL here."
  spec.metadata["rubygems_mfa_required"] = "true"

  # Specify which files should be added to the gem when it is released.
  # The `git ls-files -z` loads the files in the RubyGem that have been added into git.
  spec.files = Dir.chdir(__dir__) do
    `git ls-files -z`.split("\x0").reject do |f|
      (f == __FILE__) || f.match(%r{\A(?:(?:bin|test|spec|features)/|\.(?:git|travis|circleci)|appveyor)})
    end
  end
  spec.bindir = "exe"
  spec.executables = spec.files.grep(%r{\Aexe/}) { |f| File.basename(f) }
  spec.require_paths = ["lib"]
  spec.extensions = ["ext/lsqpack/extconf.rb"]

  # Uncomment to register a new dependency of your gem
  # spec.add_dependency "example-gem", "~> 1.0"

  # For more information and examples about making a new gem, check out our
  # guide at: https://bundler.io/guides/creating_gem.html
end

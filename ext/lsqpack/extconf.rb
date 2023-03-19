# frozen_string_literal: true

require "mkmf"

repo_root = File.join(File.expand_path(__dir__), "../", "../")

$CPPFLAGS = "-I #{repo_root}"
$LDFLAGS = "-lls-qpack -L#{File.join(repo_root, "ls-qpack")}"

Dir.chdir(repo_root) do
  Dir.chdir("ls-qpack") do
    puts(system("git status"))
    system("cmake .", exception: true)
    syetem("make", exception: true)
  end
end

create_makefile("lsqpack/lsqpack")

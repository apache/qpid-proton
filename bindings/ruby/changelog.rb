#!/bin/env ruby
#
# Generate a changelog from git history.
# - Assume each version has a 0.0[.0] tag
# - Write the subject line from each commit with a JIRA number that touches files under this dir
#

if ARGV.size != 2
  STDERR.puts "Usage: #{$0} <rubydir> <outfile>"
end
rubydir = ARGV[0]
outfile = ARGV[1]

version_re = /^[0-9]+(\.[0-9]+)+$/
releases = `git tag --sort=-creatordate`.split.select { |t| version_re.match(t) }
exit(1) unless $?.success?

def tag(x) x ? "tags/#{x}" : ""; end

changelog=""
while r = releases.shift do
  changelog << "\nversion #{r}:\n"
  p = releases[0]
  changelog << `git log --grep='^PROTON-' #{tag(p)}..#{tag(r)} --format='	* %s' -- #{rubydir}`
  exit(1) unless $?.success?
end

File.open(outfile, "w") { |f| f.puts(changelog) }

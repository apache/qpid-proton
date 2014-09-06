# -*- encoding: utf-8 -*-
lib = File.expand_path('lib/', __FILE__)
$:.unshift lib unless $:.include?(lib)

# Generate the Swig wrapper
system "swig -ruby -I../../include -o ext/cproton/cproton.c ruby.i"

Gem::Specification.new do |s|
  s.name        = "qpid_proton"
  s.version     = "0.3"
  s.licenses    = ['Apache-2.0']
  s.platform    = Gem::Platform::RUBY
  s.authors     = ["Darryl L. Pierce"]
  s.email       = ["proton@qpid.apache.org"]
  s.homepage    = "http://qpid.apache.org/proton"
  s.summary     = "Ruby language bindings for the Qpid Proton messaging framework"
  s.description = <<-EOF
Proton is a high performance, lightweight messaging library. It can be used in
the widest range of messaging applications including brokers, client libraries,
routers, bridges, proxies, and more. Proton is based on the AMQP 1.0 messaging
standard.
EOF

  s.extensions   = "ext/cproton/extconf.rb"
  s.files        = Dir[
                "LICENSE",
                "TODO",
                "ChangeLog",
                "ext/cproton/*.rb",
                "ext/cproton/*.c",
                "lib/**/*.rb",
                ]
  s.require_path = 'lib'
end


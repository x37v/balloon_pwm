#!/usr/bin/env ruby

puts "\n\n#######"
puts "press enter to start"
puts "#######"
gets

100.times.each do |i|
  puts `make clean && make SEED=#{i} && make program`

  puts "\n\n#######"
  puts "press enter to do another"
  puts "#######"
  gets
end

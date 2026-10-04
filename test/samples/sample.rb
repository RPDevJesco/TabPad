# a queue
require "set"

class Jobs
  LIMIT = 10

  def initialize(name)
    @name = name
    @seen = Set.new
  end

  def push(job)
    return false if @seen.size >= LIMIT

    @seen << job
    puts "queued #{job} on #{@name}"
    :ok
  end
end

Jobs.new("main").push(42) unless ARGV.empty?

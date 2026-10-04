-- a counter
local Counter = {}
Counter.__index = Counter

function Counter.new(start)
    return setmetatable({ n = start or 0 }, Counter)
end

function Counter:bump(by)
    self.n = self.n + (by or 1)
    if self.n > 10 then
        print("big: " .. tostring(self.n))
    end
    return self.n
end

for i = 1, 3 do
    Counter.new(i):bump()
end

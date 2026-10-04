// a class template
#include <vector>

namespace app {

template <typename T>
class Stack {
public:
    void push(const T &v) { items.push_back(v); }
    bool empty() const { return items.empty(); }

private:
    std::vector<T> items;
};

}

int main() {
    app::Stack<int> s;
    s.push(42);
    auto text = "done";
    return s.empty() ? 1 : 0;
}

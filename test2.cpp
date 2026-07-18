
#include <iostream>
#include <vector>

int main() {
    std::vector<int> v = {1, 2, 3, 4, 5, 6};
    for(auto it = v.rbegin(); it != v.rend(); ++it) {
        if(*it == 4) {
            v.erase(std::next(it).base());
            v.push_back(4);
            break;
        }
    }
    for(int x : v) std::cout << x << " ";
    std::cout << "\n";
}

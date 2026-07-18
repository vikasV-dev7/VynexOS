import subprocess
import os

code = """
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
    std::cout << "\\n";
}
"""

with open('test2.cpp', 'w') as f:
    f.write(code)

# Since cl might not be available, let's just write this to test2.cpp and we will see if we can use g++ or something else.
try:
    subprocess.run(['cl', 'test2.cpp', '/EHsc'], check=True)
    res = subprocess.run(['test2.exe'], capture_output=True, text=True)
    print(res.stdout)
except Exception as e:
    print(e)

### vector::data()

STL vector is easier to use when programming by C++. However, sometimes we need to use C style functions or system call.

`std::vector::data()` is STL in C++，which returns a pointer to a memory array. The memory array is used internally by vectors to store the elements it owns.

```cpp
#include <bits/stdc++.h>
using namespace std;

int main()
{
	vector<int> vec = { 10, 20, 30, 40, 50 };

	// memory pointer pointing to the
	// first element
	int* pos = vec.data();

	// prints the vector
	cout << "The vector elements are: ";
	for (int i = 0; i < vec.size(); ++i) {
		cout << *pos++ << " ";
    }
	return 0;
}
```
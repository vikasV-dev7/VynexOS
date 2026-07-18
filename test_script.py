def simulate():
    v = [1, 2, 3, 4, 5, 6]
    # reverse iteration
    # indices: 5, 4, 3, 2, 1, 0
    # rbegin() points to index 5
    for i in range(len(v) - 1, -1, -1):
        if v[i] == 4:
            # We want to erase element at i.
            # In C++: std::next(it) points to i - 1.
            # base() of an iterator pointing to i - 1 returns the iterator AFTER i - 1, which is i!
            # So it erases i.
            v.pop(i)
            v.append(4)
            break
    print(v)

simulate()

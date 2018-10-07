# Variant

This is an attempt to implement an interface similar to `std::variant`.
https://en.cppreference.com/w/cpp/utility/variant

Some differences are:
  - Only single instance of a type `T` is allowed.
  - No `std::monostate` default construction support


## How it works

  Every instance of a `variant` construct a buffer on the heap which is the size of the largest element in the `parameter pack`. Upon modification, the same buffer is used to `placement new` and `delete` in order to destruct the old element and construct the new one.

### To Do:
Next step is to add a `std::visit` implementation as well.



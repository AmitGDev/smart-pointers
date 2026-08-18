// main.cpp
//
// Exercises RefCountedPtr's copy/move paths so the ref count changes are
// visible rather than just asserted in a test.

#include <iostream>
#include <utility>

#include "ref_counted_ptr.hpp"

struct Widget {
  explicit Widget(int v) : value(v) {}
  ~Widget() { std::cout << "Widget(" << value << ") destroyed\n"; }
  int value;
};

int main() {
  auto a = mem::MakeRefCounted<Widget>(42);
  std::cout << "after construct: use_count = " << a.UseCount() << '\n';

  auto b = a;  // copy: shares ownership
  std::cout << "after copy:      use_count = " << a.UseCount() << '\n';

  auto c = std::move(a);  // move: transfers a's reference to c
  std::cout << "after move:      a is " << (a ? "valid" : "empty")
            << ", c use_count = " << c.UseCount() << '\n';

  {
    auto d = c;  // another copy, scoped to this block
    std::cout << "inner scope:     use_count = " << c.UseCount() << '\n';
  }  // d destroyed here, count drops back down
  std::cout << "after inner scope: use_count = " << c.UseCount() << '\n';

  std::cout << "value via c->value: " << c->value << '\n';

  return 0;
}  // b and c go out of scope here; Widget is destroyed when the last one does

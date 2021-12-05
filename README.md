# Berry-Bind

Simple C++ bindings for berry-lang built using C++ templates;

## Why

In one of my embedded projects I decided to move from lua to [berry-lang](https://github.com/berry-lang/berry). It's a great language - has a tiny resource footprint, and a really simple yet powerful syntax. Previously in the project I've utilized sol2 for bindings with C++, so I decided to develop something with simillar syntax for berry-lang. It is not nearly as powerful as sol2, but does the job for simple function bindings.


## Usage

Copy `BerryBind.cpp` and `BerryBind.h` into your project.

```cpp
int add(int a, int b) {
    return a + b;
}

...
// Create a new vm instance
auto vm = std::make_shared<berry::VmState>()

// Bind function to berry vm
vm.export_function("add", &add);

// `add` is now available from berry
vm.execute_string("print(add)");
```

Check `example.cpp` for more examples.

## Details

Binding into berry's state happens using procedures that pass the registered function's address as an upvalue. This means that the memory usage is going to be higher than binding pure-c functions into the berry-vm.

## Thanks

A lot of the code is based on [alex-ac's gist](https://gist.github.com/alex-ac/4514013). 

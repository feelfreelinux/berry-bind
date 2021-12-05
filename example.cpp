#include "BerryBind.h"
#include <iostream>
#include <memory>

// Simple function with no arguments
void simpleFunction() {
    std::cout << "Simple function called!" << std::endl;
}

// Simple function with arguments and return value
int add(int a, int b) {
    return a + b;
}

// Both list and map types are supported, both as arg and return value
berry::map listAndMap(berry::list list) {
    berry::map result;

    // Automatically recognizes type
    list.push_back("test");
    list.push_back(2);

    result["exampleList"] = list;
    
    return result;
}

// Showcase example of binding a class member function to berry vm
class TestClass {
public:
    TestClass(std::shared_ptr<berry::VmState> beVm) {
        beVm->export_this("classMethod", this, &helloWorld);
    }

    void helloWorld() {
        std::cout << "Class method called!" << std::endl;
    }
};


int main(int argc, char *argv[]) {
    auto beVm = std::make_shared<berry::VmState>();

    // Bind functions to berry state
    beVm->export_function("simpleFunction", &simpleFunction);
    beVm->export_function("add", &add);
    beVm->export_function("listAndMap", &listAndMap);

    // Execute methods in berry vm
    beVm->execute_string("simpleFunction()");
    beVm->execute_string("print(add(1, 2))");
    beVm->execute_string("print(listAndMap(['asd']))");

    auto testClass = new TestClass(beVm);
    beVm->execute_string("classMethod()");

    delete testClass;
}

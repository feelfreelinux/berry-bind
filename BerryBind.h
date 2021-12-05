#ifndef BERRY_BIND_H
#define BERRY_BIND_H
#include "be_vm.h"
#include <any>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace berry {
typedef std::map<std::string, std::any> map;
typedef std::vector<std::any> list;

template <typename T> berry::map to_map(std::map<std::string, T> inputMap) {
    berry::map berryMap;
    for (auto &pair : inputMap) {
        berryMap[pair.first] = pair.second;
    }
    return berryMap;
}

class VmState {
  private:
    bool del;
    bvm *vm;
    std::vector<std::function<int(VmState &)> *> lambdas;
    static int call(bvm *vm);

    template <class T> T arg(const int i) { return *(T *)comptr(i); }

    template <typename T> int ret(const T r) { be_return(vm); }

    template <class T> T *argp(const int i) { return (T *)comptr(i); }

    template <typename T, typename T1, typename... Args>
    std::tuple<T, T1, Args...> args(const int i = 1) {
        return std::tuple_cat(std::make_tuple(arg<T>(i)),
                              args<T1, Args...>(i + 1));
    }

    template <typename T> std::tuple<T> args(const int i = 1) {
        return std::tuple<T>(arg<T>(i));
    }

    template <typename... Args> struct sizer {
        static const int size = sizeof...(Args);
    };

    template <int N> struct apply_method {
        template <class T, typename R, typename... MethodArgs,
                  typename... TupleArgs, typename... Args>
        static R apply(T *o, R (T::*method)(MethodArgs...),
                       std::tuple<TupleArgs...> &t, Args... args) {
            return apply_method<N - 1>::apply(o, method, t, std::get<N - 1>(t),
                                              args...);
        }
    };

    template <int N> struct apply_function {
        template <typename R, typename... FunctionArgs, typename... TupleArgs,
                  typename... Args>
        static R apply(R (*function)(FunctionArgs...),
                       std::tuple<TupleArgs...> &t, Args... args) {
            return apply_function<N - 1>::apply(function, t, std::get<N - 1>(t),
                                                args...);
        }
    };

    template <int N> struct apply_function_stdfunc {
        template <typename R, typename... FunctionArgs, typename... TupleArgs,
                  typename... Args>
        static R apply(std::function<R(FunctionArgs...)> function,
                       std::tuple<TupleArgs...> &t, Args... args) {
            return apply_function_stdfunc<N - 1>::apply(
                function, t, std::get<N - 1>(t), args...);
        }
    };

  public:
    VmState();
    VmState(bvm *vm);
    ~VmState();

    bvm *raw_ptr() { return vm; }

    void execute_string(const std::string &string);

    void lambda(std::function<int(VmState &)> *function,
                const std::string &name);

    void push_data(int n = -2) {
        be_data_push(vm, n);
        be_pop(vm, -n - 1);
    }

    void pcall(const int args) {
        if (be_pcall(vm, args) != BE_OK) {
            be_dumpexcept(vm);
        }
        be_pop(vm, args + 1);
    }

    void comptr(const void *d);
    void *comptr(const int i = 1);

    void closure(int (*)(bvm *), const int i = -1);
    void set_global(const std::string &name);
    void get_global(const std::string &name) { be_getglobal(vm, name.c_str()); }

    void boolean(const bool);
    bool boolean(const int i = -1);

    void number(const bint);
    bint tonumber(const int i = -1);

    void string(const std::string &);

    void map(const berry::map &berryMap);
    berry::map map(const int i = -1);

    void list(const berry::list &berryList);
    berry::list list(const int i = -1);

    std::string string(const int i = -1);

    void real(const breal);
    breal toreal(const int i = -1);

    int doreturn() { be_return(vm); }

    template <typename R, class T, typename... Args>
    void export_method(const std::string &name, R (T::*method)(Args...)) {
        auto function = new std::function<int(
            berry::VmState &)>([method](berry::VmState &vm) -> int {
            auto tuple = vm.args<Args...>(2);
            if constexpr (std::is_same_v<R, void>) {
                apply_method<std::tuple_size<decltype(tuple)>::value>::apply(
                    vm.argp<T>(1), method, tuple);
                return vm.doreturn();
            } else {
                return vm.ret(
                    apply_method<std::tuple_size<decltype(tuple)>::value>::
                        apply(vm.argp<T>(1), method, tuple));
            }
        });
        lambda(function, name);
    }

    void map_push(const std::string &key, const std::string &value) {
        string(key);
        string(value);
        set_index(2);
    }

    void map_push(const std::string &key, int value) {
        string(key);
        number(value);
        set_index(2);
    }

    void map_push(const std::string &key, bool value) {
        string(key);
        boolean(value);
        set_index(2);
    }

    void set_index(int n = 1) {
        be_setindex(vm, -1 - n);
        be_pop(vm, n);
    }

    void end_element(const char *element) {
        be_getbuiltin(vm, element);
        be_pushvalue(vm, -2);
        be_call(vm, 1);
        be_moveto(vm, -2, -3);
        be_pop(vm, 2);
    }

    template <typename R, typename... Args>
    void export_function(const std::string &name,
                         std::function<R(Args...)> callback) {
        auto function =
            new std::function<int(VmState &)>([callback](VmState &vm) -> int {
                auto tuple = vm.args<Args...>();

                if constexpr (std::is_same_v<R, void>) {
                    apply_function_stdfunc<std::tuple_size<
                        decltype(tuple)>::value>::apply(callback, tuple);
                    return vm.doreturn();
                } else {
                    return vm.ret(
                        apply_function_stdfunc<std::tuple_size<
                            decltype(tuple)>::value>::apply(callback, tuple));
                }
            });
        lambda(function, name);
    }

    template <typename R, typename... Args>
    void export_function(const std::string &name, R (*callback)(Args...)) {
        auto function = new std::function<int(VmState &)>([callback](
                                                              VmState &vm)
                                                              -> int {
            auto tuple = vm.args<Args...>();

            if constexpr (std::is_same_v<R, void>) {
                apply_function<std::tuple_size<decltype(tuple)>::value>::apply(
                    callback, tuple);
                return vm.doreturn();
            } else {
                return vm.ret(
                    apply_function<std::tuple_size<decltype(tuple)>::value>::
                        apply(callback, tuple));
            }
        });
        lambda(function, name);
    }

    template <typename R>
    void export_function(const std::string &name, R (*callback)()) {
        auto function =
            new std::function<int(VmState &)>([callback](VmState &vm) -> int {
                if constexpr (std::is_same_v<R, void>) {
                    (*callback)();
                    return vm.doreturn();
                } else {
                    return vm.ret((*callback)());
                }
            });
        lambda(function, name);
    }

    template <typename R, class T>
    void export_method(const std::string &name, R (T::*method)()) {
        auto function =
            new std::function<int(VmState &)>([method](VmState &vm) -> int {
                if constexpr (std::is_same_v<R, void>) {
                    vm.argp<T>(1)->*method();
                    return vm.doreturn();
                } else {
                    return vm.ret((vm.argp<T>(1)->*method)());
                }
            });
        lambda(function, name);
    }

    template <typename R>
    void export_function(const std::string &name,
                         std::function<R(void)> callback) {
        auto function =
            new std::function<int(VmState &)>([callback](VmState &vm) -> int {
                if constexpr (std::is_same_v<R, void>) {
                    callback();
                    return vm.doreturn();
                } else {
                    return vm.ret(callback());
                }
            });
        lambda(function, name);
    }

    template <class C, typename Ret, typename... Ts>
    void export_this(const std::string &name, C *c, Ret (C::*m)(Ts...)) {
        std::function<Ret(Ts...)> res = [=](auto &&...args) {
            return (c->*m)(std::forward<decltype(args)>(args)...);
        };
        export_function(name, res);
    }
};

template <> int VmState::ret<bool>(const bool r);

template <> int VmState::ret<bint>(const bint r);

template <> int VmState::ret<int>(const int r);

template <> int VmState::ret<breal>(const breal r);

template <> int VmState::ret<berry::map>(const berry::map r);

template <> int VmState::ret<berry::list>(const berry::list r);

template <> int VmState::ret<std::string>(const std::string r);

template <> std::string VmState::arg<std::string>(const int i);

template <> bint VmState::arg<bint>(const int i);

template <> float VmState::arg<float>(const int i);

template <> int VmState::arg<int>(const int i);

template <> bool VmState::arg<bool>(const int i);

template <> berry::map VmState::arg<berry::map>(const int i);

template <> berry::list VmState::arg<berry::list>(const int i);

template <> struct VmState::apply_method<0> {
    template <class T, typename R, typename... MethodArgs,
              typename... TupleArgs, typename... Args>
    static R apply(T *o, R (T::*method)(MethodArgs...),
                   std::tuple<TupleArgs...> &t, Args... args) {
        return (o->*method)(args...);
    }
};

template <> struct VmState::apply_function<0> {
    template <typename R, typename... FunctionArgs, typename... TupleArgs,
              typename... Args>
    static R apply(R (*function)(FunctionArgs...), std::tuple<TupleArgs...> &t,
                   Args... args) {
        return (*function)(args...);
    }
};

template <> struct VmState::apply_function_stdfunc<0> {
    template <typename R, typename... FunctionArgs, typename... TupleArgs,
              typename... Args>
    static R apply(std::function<R(FunctionArgs...)> function,
                   std::tuple<TupleArgs...> &t, Args... args) {
        return function(args...);
    }
};
} // namespace berry

#endif

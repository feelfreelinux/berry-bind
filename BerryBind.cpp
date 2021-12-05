#include "BerryBind.h"

using namespace berry;

VmState::VmState(bvm *vm) : vm(vm) {}

VmState::VmState() { vm = be_vm_new(); }

VmState::~VmState() {}

int VmState::call(bvm *vm) {
    be_getupval(vm, 0, 0);

    auto function = (std::function<int(VmState &)> *)be_tocomptr(vm, -1);

    auto l = VmState(vm);
    return (*function)(l);
}

void VmState::execute_string(const std::string &data) {
    // create an array
    if (be_loadstring(vm, data.c_str()) == 0) {
        if (be_pcall(vm, 0) != 0) {
            be_dumpexcept(vm);
        }
    } else {
        be_dumpexcept(vm);
    }
    be_pop(vm, 1);
}

void VmState::lambda(std::function<int(VmState &)> *function,
                     const std::string &name) {
    lambdas.push_back(function);
    closure(VmState::call, 1);
    comptr(function);
    be_setupval(vm, -2, 0);
    be_pop(vm, 1);
    set_global(name);
    be_pop(vm, 1);
}

void VmState::closure(int (*callback)(bvm *), const int i) {
    be_pushntvclosure(vm, callback, i);
}

void VmState::boolean(const bool b) { be_pushbool(vm, b); }

bool VmState::boolean(const int i) {
    if (!be_isbool(vm, i))
        be_raise(vm, "internal_error", "is not boolean");
    return be_tobool(vm, i);
}

void VmState::string(const std::string &string) {
    be_pushstring(vm, string.c_str());
}

std::string VmState::string(const int i) {
    if (!be_isstring(vm, i))
        be_raise(vm, "internal_error", "Is not string");
    return be_tostring(vm, i);
}

void VmState::real(const breal b) { be_pushreal(vm, b); }

breal VmState::toreal(const int i) {
    if (!be_isreal(vm, i))
        be_raise(vm, "internal_error", "is not breal");
    return be_toreal(vm, i);
}

void VmState::number(const bint b) { be_pushint(vm, b); }

bint VmState::tonumber(const int i) {
    if (!be_isint(vm, i))
        be_raise(vm, "internal_error", "is not breal");
    return be_toint(vm, i);
}

void VmState::set_global(const std::string &name) {
    be_setglobal(vm, name.c_str());
}

berry::list VmState::list(const int i) {
    be_getmember(vm, i, ".p");

    berry::list result;
    if (!be_islist(vm, -1)) {
        throw std::runtime_error("Is not a list");
    }

    // push the iterator
    be_pushiter(vm, -1);

    while (be_iter_hasnext(vm, -2)) {
        be_iter_next(vm, -2);

        if (be_isstring(vm, -1)) {
            result.push_back(string(-1));
        } else if (be_islist(vm, -1)) {
            result.push_back(list(-1));
        } else if (be_isnumber(vm, -1)) {
            result.push_back((int)tonumber(-1));
        } else if (be_islist(vm, -1)) {
            result.push_back(list(-1));
        } else if (be_isbool(vm, -1)) {
            result.push_back(boolean(-1));
        } else if (be_isinstance(vm, -1)) {
            be_getmember(vm, -1, ".p");

            if (be_ismap(vm, -1)) {
                be_pop(vm, 1);
                result.push_back(map(-1));
            } else if (be_islist(vm, -1)) {
                be_pop(vm, 1);
                result.push_back(list(-1));
            } else {
                be_pop(vm, 1);
            }
        } else {
            // Unknown type
        }

        be_pop(vm, 1);
    }

    be_pop(vm, 1);

    return result;
}

berry::map VmState::map(const int i) {
    be_getmember(vm, i, ".p");

    berry::map result;
    if (!be_ismap(vm, -1)) {
        throw std::runtime_error("Is not a map");
    }

    // push the iterator
    be_pushiter(vm, -1);

    while (be_iter_hasnext(vm, -2)) {
        be_iter_next(vm, -2);
        auto key = string(-2);

        if (be_isstring(vm, -1)) {
            result[key] = string(-1);
        } else if (be_isnumber(vm, -1)) {
            result[key] = (int)tonumber(-1);
        } else if (be_islist(vm, -1)) {
            result[key] = list(-1);
        } else if (be_isbool(vm, -1)) {
            result[key] = boolean(-1);
        } else if (be_isinstance(vm, -1)) {
            be_getmember(vm, -1, ".p");
            if (be_ismap(vm, -1)) {
                be_pop(vm, 1);
                result[key] = map(-1);
            } else if (be_islist(vm, -1)) {
                be_pop(vm, 1);
                result[key] = list(-1);
            } else {
                be_pop(vm, 1);
            }
        } else {
            // Unknown type
        }

        be_pop(vm, 2);
    }

    be_pop(vm, 1);

    return result;
}

void VmState::list(const berry::list &berryList) {

    be_newlist(vm);
    // iterate over all items in berryList
    for (auto &value : berryList) {
        if (std::any_cast<std::string>(&value)) {
            string(std::any_cast<std::string>(value));
            push_data();
        } else if (std::any_cast<const char *>(&value)) {
            be_pushstring(vm, std::any_cast<const char *>(value));
            push_data();
        } else if (std::any_cast<bool>(&value)) {
            boolean(std::any_cast<bool>(value));
            push_data();
        } else if (std::any_cast<int>(&value)) {
            number(std::any_cast<int>(value));
            push_data();
        } else if (std::any_cast<berry::list>(&value)) {
            list(std::any_cast<berry::list>(value));
            push_data();
        } else if (std::any_cast<berry::map>(&value)) {
            map(std::any_cast<berry::map>(value));
            push_data();
        }
    }

    end_element("list");
}

void VmState::map(const berry::map &berryMap) {
    be_newmap(vm);

    // iterate over each berryMap field
    auto it = berryMap.begin();

    while (it != berryMap.end()) {
        std::string key = it->first;
        auto value = it->second;

        if (std::any_cast<std::string>(&value)) {
            string(key);
            string(std::any_cast<std::string>(value));
            set_index(2);
        } else if (std::any_cast<const char *>(&value)) {
            string(key);
            be_pushstring(vm, std::any_cast<const char *>(value));
            set_index(2);
        } else if (std::any_cast<bool>(&value)) {
            string(key);
            boolean(std::any_cast<bool>(value));
            set_index(2);
        } else if (std::any_cast<berry::list>(&value)) {
            string(key);
            list(std::any_cast<berry::list>(value));
        } else if (std::any_cast<int>(&value)) {
            string(key);
            number(std::any_cast<int>(value));
            set_index(2);
        } else if (std::any_cast<berry::map>(&value)) {
            string(key);
            map(std::any_cast<berry::map>(value));
            set_index(2);
        }
        // Increment the Iterator to point to next entry
        it++;
    }

    end_element("map");
}

void VmState::comptr(const void *d) {
    be_pushcomptr(vm, const_cast<void *>(d));
}

void *VmState::comptr(const int i) {
    if (!be_iscomptr(vm, i))
        be_raise(vm, "internal_error", "is not comptr");
    return be_tocomptr(vm, i);
}

template <> int VmState::ret<bool>(const bool r) {
    boolean(r);
    be_return(vm);
}

template <> int VmState::ret<berry::map>(const berry::map r) {
    map(r);
    be_return(vm);
}

template <> int VmState::ret<berry::list>(const berry::list r) {
    list(r);
    be_return(vm);
}

template <> int VmState::ret<std::string>(const std::string r) {
    string(r);
    be_return(vm);
}

template <> int VmState::ret<breal>(const breal r) {
    real(r);
    be_return(vm);
}

template <> int VmState::ret<bint>(const bint r) {
    number(r);
    be_return(vm);
}

template <> int VmState::ret<int>(const int r) {
    number(r);
    be_return(vm);
}

template <> std::string VmState::arg<std::string>(const int i) {
    return string(i);
}

template <> float VmState::arg<float>(const int i) { return toreal(i); }

template <> bool VmState::arg<bool>(const int i) { return boolean(i); }

template <> bint VmState::arg<bint>(const int i) { return tonumber(i); }

template <> berry::map VmState::arg<berry::map>(const int i) { return map(i); }

template <> berry::list VmState::arg<berry::list>(const int i) {
    return list(i);
}

template <> int VmState::arg<int>(const int i) { return tonumber(i); }

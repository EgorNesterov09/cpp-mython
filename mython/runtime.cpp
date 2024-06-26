#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>
#include <algorithm>

using namespace std;

namespace runtime {

ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
    : data_(std::move(data)) {
}

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
}

ObjectHolder ObjectHolder::None() {
    return ObjectHolder();
}

Object& ObjectHolder::operator*() const {
    AssertIsValid();
    return *Get();
}

Object* ObjectHolder::operator->() const {
    AssertIsValid();
    return Get();
}

Object* ObjectHolder::Get() const {
    return data_.get();
}

ObjectHolder::operator bool() const {
    return Get() != nullptr;
}

bool IsTrue(const ObjectHolder& object) {
    bool result = false;
    if (object.TryAs<Bool>()) {
        result = object.TryAs<Bool>()->GetValue() == true;
    }
    if (object.TryAs<Number>()) {
        result = object.TryAs<Number>()->GetValue() != 0;
    }
    if (object.TryAs<String>()) {
        result = object.TryAs<String>()->GetValue() != ""s;
    }
    return result;
}

void ClassInstance::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    if (HasMethod("__str__", 0) == true) {
        Call("__str__", {}, context).Get()->Print(os, context);
    }
    else {
        os << this;
    }
}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    const Method* method_ptr = cls_.GetMethod(method);
    if (cls_.GetMethod(method) != nullptr && method_ptr->formal_params.size() == argument_count) {
        return true;
    }
    return false;
}

Closure& ClassInstance::Fields() {
    return closure_;
}

const Closure& ClassInstance::Fields() const {
    return closure_;
}

ClassInstance::ClassInstance(const Class& cls) :cls_(cls) {
    closure_["self"] = ObjectHolder::Share(*this);
}

ObjectHolder ClassInstance::Call(const std::string& method,
                                 const std::vector<ObjectHolder>& actual_args,
                                 Context& context) {
    // Заглушка. Реализуйте метод самостоятельно.
    if (HasMethod(method, actual_args.size())) {
        const Method* method_ptr = cls_.GetMethod(method);
        Closure local_closure;
        local_closure["self"] = ObjectHolder::Share(*this);
        for (size_t i = 0; i < actual_args.size(); ++i) {
            local_closure[method_ptr->formal_params[i]] = actual_args[i];
        }
        return method_ptr->body->Execute(local_closure, context);
    }
    else {
        throw std::runtime_error("No such method"s);
    }
}

Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
    : name_(name), methods_(std::move(methods)), parent_(parent) {}


const Method* Class::GetMethod(const std::string& name) const {
    const auto it = find_if(methods_.begin(), methods_.end(), [&name](const Method& method) {
        return method.name == name;
        });
    if (it != methods_.end()) {
        return &(*it);
    }
    if (parent_) {
        const Method* parent_method_ptr = parent_->GetMethod(name);
        if (parent_method_ptr != nullptr) {
            return parent_method_ptr;
        }
    }
    return nullptr;
}


[[nodiscard]] /*inline*/ const std::string& Class::GetName() const {
    return name_;
}

void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
    os << "Class "s << name_;
}

void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    os << (GetValue() ? "True"sv : "False"sv);
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    ClassInstance* class_instance_ptr = lhs.TryAs<ClassInstance>();
    if (class_instance_ptr) {
        return IsTrue(class_instance_ptr->Call("__eq__", { rhs }, context));
    }
    auto result = CompareValuableObjectIfPossible(lhs, rhs, equal_to{});
    if (result) {
        return result.value();
    }
    if (!bool(lhs) && !bool(rhs)) {
        return true;
    }
    throw std::runtime_error("Cannot compare objects for equality"s);
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    ClassInstance* class_instance_ptr = lhs.TryAs<ClassInstance>();
    if (class_instance_ptr) {
        return IsTrue(class_instance_ptr->Call("__lt__", { rhs }, context));
    }
    auto result = CompareValuableObjectIfPossible(lhs, rhs, less{});
    if (result) {
        return result.value();
    }
    throw std::runtime_error("Cannot compare objects for less"s);
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Equal(lhs, rhs, context);
}

bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !LessOrEqual(lhs, rhs, context);
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return Less(lhs, rhs, context) || Equal(lhs, rhs, context);
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context);
}

}  // namespace runtime
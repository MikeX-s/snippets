import std;

struct Person {
  std::string name;
  unsigned int age;
  std::string email;
};

template <typename T> void debugPrint(const T &obj) {
  std::println("{}:", std::meta::identifier_of(^^T));

  template for (constexpr auto member :
                std::define_static_array(std::meta::nonstatic_data_members_of(
                    ^^T, std::meta::access_context::current()))) {

    std::println(" {}: {}", std::meta::identifier_of(member), obj.[:member:]);
  }
}

auto main() -> int {
  Person bob{.name = "Bob", //
             .age = 42,     //
             .email = "a@b.com"};

  debugPrint(bob);

  return 0;
}

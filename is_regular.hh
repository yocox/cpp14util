#include <type_traits>
#include "./equality_comparable.hh"

namespace yoco {
    template <typename T>
    struct is_regular
      : std::integral_constant<bool,
        std::is_default_constructible<T>::value &&
        std::is_copy_constructible<T>::value &&
        std::is_move_assignable<T>::value &&
        std::is_copy_assignable<T>::value &&
        std::is_move_assignable<T>::value &&
        is_equality_comparable<T>::value>
    {};
}

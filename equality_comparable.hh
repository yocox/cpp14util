#include <type_traits>

namespace yoco {
    namespace detail {
        template <typename T>
        std::false_type check_equality_comparable(const T& t, long);

        template <typename T>
        auto check_equality_comparable(const T& t, int)
            -> typename std::is_convertible<decltype(t == t), bool>::type;
    }

    template <typename T>
    struct is_equality_comparable
      : decltype(detail::check_equality_comparable(std::declval<const T&>(), 1))
    {};
}

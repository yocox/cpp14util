#ifndef YOCO_GENERAL_VARIANT_HH
#define YOCO_GENERAL_VARIANT_HH

#include <cassert>
#include <cstddef>
#include <functional>

namespace generic {

namespace detail {

template <typename Head, typename ... Rest>
struct largest_size_of {
    static constexpr size_t max_size = sizeof(Head) > largest_size_of<Rest...>::max_size ?
                                       sizeof(Head) : largest_size_of<Rest...>::max_size ;
} ;

template <typename T>
struct largest_size_of<T> {
    static constexpr size_t max_size = sizeof(T);
} ;

template <int N, typename T, typename ... TL>
struct find_impl;

template <int N, typename T, typename ... Rest>
struct find_impl<N, T, T, Rest...> {
    static constexpr int value = N;
} ;

template <int N, typename T, typename Head, typename ... Rest>
struct find_impl<N, T, Head, Rest...> {
    static constexpr int value = find_impl<N + 1, T, Rest...>::value;
} ;

template <int N, typename T>
struct find_impl<N, T> {
    static constexpr int value = -1;
} ;

template <typename T, typename ... TL>
struct find {
    static constexpr int value = find_impl<0, T, TL ...>::value;
} ;

template <int N, int UNTIL, typename ... TL>
struct at_impl;

template <int N, typename Head, typename ... Rest>
struct at_impl<N, N, Head, Rest...> {
    using type = Head;
} ;

template <int N, int UNTIL, typename Head, typename ... Rest>
struct at_impl<N, UNTIL, Head, Rest...> {
    using type = typename at_impl<N + 1, UNTIL, Rest...>::type;
} ;

template <int N, typename ... TL>
struct at {
    using type = typename at_impl<0, N, TL...>::type;
} ;

template <int N, typename ... TL>
struct clear_impl {
    static bool dtor(int n, void* p) {
        if(N == n) {
            using T = typename at<N, TL...>::type;
            static_cast<T*>(p)->~T();
            return true;
        } else {
            return clear_impl<N - 1, TL...>::dtor(n, p);
        }
    }
};

template <typename ... TL>
struct clear_impl<-1, TL...> {
    static bool dtor(int, void*) {
        return false;
    }
};

template <int N, typename ... TL>
struct equal_compare_impl {
    static bool equal_compare(int n, const void* const lhs, const void* const rhs) {
        if(N == n) {
            using T = typename at<N, TL...>::type;
            return *static_cast<const T*const>(lhs) == *static_cast<const T*const>(rhs);
        } else {
            return equal_compare_impl<N - 1, TL...>::equal_compare(n, lhs, rhs);
        }
    }
};

template <typename ... TL>
struct equal_compare_impl<-1, TL...> {
    static bool equal_compare(int, const void* const, const void* const) {
        return false;
    }
};

template <int N, typename ... TL>
struct assign_impl {
    static void assign(int n, void* const lhs, const void* const rhs) {
        if(N == n) {
            using T = typename at<N, TL...>::type;
            new(static_cast<void*>(lhs)) T(*static_cast<const T*const>(rhs));
        } else {
            assign_impl<N - 1, TL...>::assign(n, lhs, rhs);
        }
    }
};

template <typename ... TL>
struct assign_impl<-1, TL...> {
    static void assign(int, void* const, const void* const) {
    }
};

template <int N, typename ... TL>
struct apply_impl {
    template<typename Visitor>
    static decltype(auto) apply(Visitor&& v, int n, void* p) {
        if(N == n) {
            using T = typename at<N, TL...>::type;
            return v(*static_cast<T*>(static_cast<void*>(p)));
        } else {
            return apply_impl<N - 1, TL...>::apply(std::forward<Visitor>(v), n, p);
        }
    }
    template<typename Visitor>
    static decltype(auto) apply(Visitor&& v, int n, const void* p) {
        if(N == n) {
            using T = typename at<N, TL...>::type;
            return v(*static_cast<const T*>(static_cast<const void*>(p)));
        } else {
            return apply_impl<N - 1, TL...>::apply(std::forward<Visitor>(v), n, p);
        }
    }
};

template <typename Head, typename ... TL>
struct apply_impl<0, Head, TL...> {
    template<typename Visitor>
    static decltype(auto) apply(Visitor&& v, int, void* p) {
        return v(*static_cast<Head*>(static_cast<void*>(p)));
    }
    template<typename Visitor>
    static decltype(auto) apply(Visitor&& v, int, const void* p) {
        return v(*static_cast<const Head*>(static_cast<const void*>(p)));
    }
};

template <typename ... TL>
struct any_dup;

template <typename Head, typename ... Rest>
struct any_dup<Head, Rest...> {
    static constexpr bool value = find<Head, Rest...>::value != -1 || any_dup<Rest...>::value == true;
} ;

template <typename Head>
struct any_dup<Head> {
    static constexpr bool value = false;
} ;

} // namespace detail

template <typename T0, typename ... Rest>
class variant
{
    static_assert(!detail::any_dup<T0, Rest...>::value, "Variant has duplicated type");
    static constexpr size_t max_size = detail::largest_size_of<T0, Rest...>::max_size;

  public:
    //////////////////////////////////////////////////////////////////////
    // constructor
    //////////////////////////////////////////////////////////////////////
    explicit variant() : index_(-1) {
        //std::cout << "variant::variant<T>(const T&)" << std::endl;
    }

    explicit variant(const variant& rhs) {
        //std::cout << "variant::variant(const variant&), rhs.index_ = " << rhs.index_ << ", this = " << this << ", rhs = " << &rhs << std::endl;
        index_ = rhs.index_;
        detail::assign_impl<sizeof...(Rest), T0, Rest...>::assign(index_, storage_, rhs.storage_);
    }

    explicit variant(variant&& rhs) noexcept {
        //std::cout << "variant::variant(variant&&)" << std::endl;
        index_ = rhs.index_;
        std::copy(rhs.storage_, rhs.storage_ + max_size, storage_);
    }

    template<typename T>
    explicit variant(const T& rhs) {
        //std::cout << "variant::variant<T>(const T&), rhs = " << rhs << ", this = " << this << std::endl;
        constexpr static int type_idx = detail::find<T, T0, Rest...>::value;
        static_assert(type_idx >= 0 && type_idx <= int(sizeof...(Rest)),
                      "Assigned type is not in variant.");
        new(static_cast<void*>(storage_)) T(rhs);
        index_ = type_idx;
    }

    //////////////////////////////////////////////////////////////////////
    // assign operator
    //////////////////////////////////////////////////////////////////////
    variant& operator=(const variant& rhs) {
        //std::cout << "variant::operator=(const variant&)" << std::endl;
        if(this == &rhs)
            return *this;
        clear();
        index_ = rhs.index_;
        detail::assign_impl<sizeof...(Rest), T0, Rest...>::assign(index_, storage_, rhs.storage_);
        return *this;
    }

    variant& operator=(variant&& rhs) noexcept {
        //std::cout << "variant::operator=(variant&&)" << std::endl;
        if(this == &rhs)
            return *this;
        clear();
        index_ = rhs.index_;
        std::copy(rhs.storage_, rhs.storage_ + max_size, storage_);
        rhs.index_ = -1;
        return *this;
    }

    template<typename T>
    variant& operator=(const T& v) {
        //std::cout << "variant::operator=<T>(const T&)" << std::endl;
        constexpr static int type_idx = detail::find<T, T0, Rest...>::value;
        static_assert(type_idx >= 0 && type_idx <= int(sizeof...(Rest)),
                      "Assigned type is not in variant.");
        clear();
        new(static_cast<void*>(storage_)) T(v);
        index_ = type_idx;
        return *this;
    }

    //////////////////////////////////////////////////////////////////////
    // cast operator
    //////////////////////////////////////////////////////////////////////
    template<typename T>
    operator T() {
        return get<T>();
    }

    //////////////////////////////////////////////////////////////////////
    // equality operator
    //////////////////////////////////////////////////////////////////////
    bool operator==(const variant& rhs) const noexcept {
        if (index_ == -1 && rhs.index_ == -1)
            return true;
        else
            return index_ == rhs.index_ &&
                   detail::equal_compare_impl<sizeof...(Rest), T0, Rest...>::equal_compare(index_, storage_, rhs.storage_);
    }

    //////////////////////////////////////////////////////////////////////
    // getter
    //////////////////////////////////////////////////////////////////////
    template<typename T>
    T& get() noexcept {
        constexpr static int type_idx = detail::find<T, T0, Rest...>::value;
        static_assert(type_idx >= 0 && type_idx <= int(sizeof...(Rest)),
                      "Get type is not in variant.");
        assert(index_ != -1 && "the variant object is empty");
        assert(index_ == type_idx && "stored type is not access type");
        return *static_cast<T*>(static_cast<void*>(storage_));
    }

    template<typename T>
    const T& get() const noexcept {
        constexpr static int type_idx = detail::find<T, T0, Rest...>::value;
        static_assert(type_idx >= 0 && type_idx <= int(sizeof...(Rest)),
                      "Get type is not in variant.");
        assert(index_ != -1 && "the variant object is empty");
        assert(index_ == type_idx && "stored type is not access type");
        return *static_cast<T*>(static_cast<void*>(storage_));
    }

    template<int type_idx>
    decltype(auto) get() noexcept {
        static_assert(type_idx >= 0 && type_idx <= int(sizeof...(Rest)),
                      "Get type is not in variant.");
        assert(index_ != -1 && "the variant object is empty");
        assert(index_ == type_idx && "stored type is not access type");
        using T = typename detail::at<type_idx, T0, Rest...>::type;
        return *static_cast<T*>(static_cast<void*>(storage_));
    }

    template<int type_idx>
    decltype(auto) get() const noexcept {
        static_assert(type_idx >= 0 && type_idx <= int(sizeof...(Rest)),
                      "Get type is not in variant.");
        assert(index_ != -1 && "the variant object is empty");
        assert(index_ == type_idx && "stored type is not access type");
        using T = typename detail::at<type_idx, T0, Rest...>::type;
        return *static_cast<const T*>(static_cast<const void*>(storage_));
    }

    int get_index() const noexcept {
        return index_;
    }

    //////////////////////////////////////////////////////////////////////
    // query
    //////////////////////////////////////////////////////////////////////
    template<typename T>
    bool is() const noexcept {
        constexpr static int type_idx = detail::find<T, T0, Rest...>::value;
        static_assert(type_idx >= 0 && type_idx <= int(sizeof...(Rest)),
                      "Get type is not in variant.");
        return index_ == type_idx;
    }

    bool empty() const noexcept {
        return index_ == -1;
    }

    void clear() {
        if(index_ == -1)
            return;
        detail::clear_impl<sizeof...(Rest), T0, Rest...>::dtor(index_, storage_);
        index_ = -1;
    }

    ~variant() {
        clear();
    }

    //////////////////////////////////////////////////////////////////////////////
    // visitor apply
    //////////////////////////////////////////////////////////////////////////////
    template <typename Visitor>
    decltype(auto) apply(Visitor&& v) {
        assert(index_ != -1);
        return detail::apply_impl<sizeof...(Rest), T0, Rest...>::apply(std::forward<Visitor>(v), index_, storage_);
    }

    template <typename Visitor>
    decltype(auto) apply(Visitor&& v) const {
        assert(index_ != -1);
        return detail::apply_impl<sizeof...(Rest), T0, Rest...>::apply(std::forward<Visitor>(v), index_, storage_);
    }

  private:
    unsigned char storage_[max_size];
    int index_ = -1;
} ;

} // namespace generic

#endif // YOCO_GENERAL_VARIANT_HH

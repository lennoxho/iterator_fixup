#include <boost/range.hpp>
#include <iterator>
#include <type_traits>

namespace detail {

    template <typename IterTy, typename Reference, typename = void>
    struct inferred_value_type {
        using type = std::remove_reference_t<Reference>;
    };

    template <typename IterTy, typename Reference>
    struct inferred_value_type<IterTy, Reference, std::void_t<typename IterTy::value_type>> {
        using type = typename IterTy::value_type;
    };

    template <typename IterTy, typename = void>
    struct inferred_difference_type {
        using type = std::ptrdiff_t;
    };

    template <typename IterTy>
    struct inferred_difference_type<IterTy, std::void_t<typename IterTy::difference_type>> {
        using type = typename IterTy::difference_type;
    };

    template <typename IterTy, typename = void>
    struct inferred_iterator_category {
        using type = std::input_iterator_tag;
    };

    template <typename IterTy>
    struct inferred_iterator_category<IterTy, std::void_t<typename IterTy::iterator_category>> {
        using type = typename IterTy::iterator_category;
    };

}

/// @brief Performs fixup of the iterator requirements for given iterator type.
///
///        Writing a conformant iterator is difficult. An InputIterator is required to:
///        1)  satisfy CopyConstructible
///        2)  satisfy CopyAssignable
///        3)  satisfy Destructible
///        4)  lvalues must satisfy Swappable
///        5)  provide value_type
///        6)  provide difference_type
///        7)  provide reference
///        8)  provide pointer
///        9)  provide iterator_category
///        10) be dereferencible via * AND ->
///        11) be pre AND post incrementible
///        12) be comparible with == and !=
///        * the operations have subtle semantics that have to be fulfilled.
///
///        Additionally, there are a few implied requirements:
///        1) operator* should return reference
///        2) operator-> should return pointer
///
///        In my experience, almost none of the InputIterator I've seen fulfills all those requirements.
///        That is unfortunate, since libraries such as boost::range and boost::algorithm relies heavily on those.
///
///        Right now, we will perform fixup of requirements 5-9 and additional requirements 1-2.
///        This list may expand in the future.
template <typename IterTy>
struct iterator_fixup : public IterTy {
    using decayed_type = std::decay_t<IterTy>;

    // Always override reference and pointer, since we know the real types for sure.
    using reference = decltype(std::declval<decayed_type>().operator*());
    using pointer = decltype(std::declval<decayed_type>().operator->());

    // If value_type is not provided, infer from reference.
    using value_type = typename detail::inferred_value_type<decayed_type, reference>::type;

    // If difference_type is not provided, default to std::ptrdiff_t.
    using difference_type = typename detail::inferred_difference_type<decayed_type>::type;

    // If iterator_category is not provided, default to std::input_iterator_tag.
    // I don't think you really fulfilled the requirements of iterator_category but we'll roll with it.
    using iterator_category = typename detail::inferred_iterator_category<decayed_type>::type;

    //
    // Defining those will disable the default constructor, which we want.
    //

    // Writing out both constructors is still cleaner than dealing with forwarding references + enable_if + is_same.
    iterator_fixup(const decayed_type &iter)
    :IterTy{ iter }
    {}

    // If decayed_type does not define a move constructor, will bind to its copy constructor.
    iterator_fixup(decayed_type &&iter)
    :IterTy{ std::move(iter) }
    {}

    //
    // Let the compiler auto-generate destructor, copy/move constructors and assignment operator.
    //

    //
    // Inherit dereference operators, increment operators etc from decayed_type.
    //

    //
    // There are 2 approaches to the data members - store a reference or a copy of the iterator.
    // - The reference approach is ugly, since we have to deal with const vs non-const.
    //   It's also dangerous, since in the non-const case, the reference iterator may go out of scope.
    // - The copy approach is more in line with the philosophy of iterators, and how range adaptors are implemented.
    //   The only downside is the iterator has to be copyable, which... is actually required. See
    //   https://en.cppreference.com/w/cpp/named_req/Iterator.
    //
};

template <typename IterTy>
inline auto fixup_iterator(IterTy &&iter) {
    return iterator_fixup<IterTy>(std::forward<IterTy>(iter));
}

template <typename RangeTy>
inline auto fixup_range(RangeTy &&range) {
    return boost::make_iterator_range(fixup_iterator(std::forward<RangeTy>(range).begin()),
                                      fixup_iterator(std::forward<RangeTy>(range).end()));
}

struct range_fixup_adaptor {};

template <typename RangeTy>
inline auto operator|(RangeTy &&range, range_fixup_adaptor) {
    return fixup_range(std::forward<RangeTy>(range));
}

//
// Examples
//

#include <boost/range/adaptors.hpp>
#include <vector>

struct rvalue_iterator {
    mutable int x;

    using value_type = int;
    using reference = int&;
    using pointer = int*;
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;

    value_type operator*() const { return 1; }
    pointer operator->() const { return &x; }
    bool operator==(rvalue_iterator) const { return true; }
    bool operator!=(rvalue_iterator) const { return false; }
    rvalue_iterator &operator++() { return *this; }
    rvalue_iterator operator++(int) { return { *this }; }
};

struct pointer_rvalue_iterator {
    mutable int* x;

    using value_type = int*;
    using reference = int*&;
    using pointer = int**;
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;

    value_type operator*() const { return x; }
    pointer operator->() const { return &x; }
    bool operator==(pointer_rvalue_iterator) const { return true; }
    bool operator!=(pointer_rvalue_iterator) const { return false; }
    pointer_rvalue_iterator &operator++() { return *this; }
    pointer_rvalue_iterator operator++(int) { return { *this }; }
};

static_assert(std::is_same<decltype(*std::declval<std::vector<int>>().begin()), int&>::value);
static_assert(std::is_same<decltype(*std::declval<const std::vector<int>>().begin()), const int&>::value);
static_assert(std::is_same<decltype(*std::declval<std::vector<int>>().cbegin()), const int&>::value);


int main() {
    std::vector<int> vec;
    auto const_lvalue_range = boost::make_iterator_range(vec.cbegin(), vec.cend());
    auto non_const_lvalue_range = boost::make_iterator_range(vec.begin(), vec.end());
    auto rvalue_range = boost::make_iterator_range(rvalue_iterator{}, rvalue_iterator{});
    auto pointer_rvalue_range = boost::make_iterator_range(pointer_rvalue_iterator{}, pointer_rvalue_iterator{});

    auto always_true = [](const auto &) {
        return true;
    };

    auto const_lvalue_rng = const_lvalue_range | boost::adaptors::filtered(always_true);
    auto non_const_lvalue_rng = non_const_lvalue_range | boost::adaptors::filtered(always_true);
    auto rvalue_rng = rvalue_range | range_fixup_adaptor{} | boost::adaptors::filtered(always_true);
    auto pointer_rvalue_rng = pointer_rvalue_range | range_fixup_adaptor{} | boost::adaptors::filtered(always_true);

    return 0;
}
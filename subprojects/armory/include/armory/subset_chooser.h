// Subset Chooser by Toby Speith
// https://codereview.stackexchange.com/a/164766

#include <functional>
#include <vector>
// C++ Concepts: template<ForwardIterator It>

template<typename It>
class SubsetChooser
{
    using Subset = std::vector<It>;
    using Predicate = std::function<bool(const Subset&)>;

    const It m_first;
    const It m_last;
    const size_t m_subset_size;
    const Predicate m_is_valid;

    Subset m_state;

public:
    SubsetChooser(It first, It last, size_t subset_size, SubsetChooser<It>::Predicate is_valid);
    const Subset& subset() const;
    bool advance();
};

//---
// factory methods

template<typename It, typename Predicate>
auto make_chooser(It first, It last, size_t subset_size, Predicate is_valid)
{
    return SubsetChooser<It>{first, last, subset_size, is_valid};
}
template<typename Container, typename Predicate>
auto make_chooser(const Container& c, size_t subset_size, Predicate is_valid)
{
    using std::begin;
    using std::end;
    return make_chooser(begin(c), end(c), subset_size, is_valid);
}

//---
// private helpers

// Calculate it+n==end, without requiring a BidirectionalIterator
template<typename Iter, typename Distance>
bool is_n_from(Iter it, Distance n, const Iter& end)
{
    std::advance(it, n);
    return it == end;
}

//---
// implementation

template<typename It>
SubsetChooser<It>::SubsetChooser(It first, It last, size_t subset_size, SubsetChooser<It>::Predicate is_valid)
    : m_first{first}, m_last{last},
      m_subset_size{subset_size},
      m_is_valid{is_valid},
      m_state{}
{
    m_state.reserve(m_subset_size);
}

template<typename It>
const typename SubsetChooser<It>::Subset& SubsetChooser<It>::subset() const
{
    return m_state;
}

template<typename It>
bool SubsetChooser<It>::advance()
{
    do {
        if (m_state.empty()) {
            m_state.push_back(m_first);
        } else {
            if (m_state.size() < m_subset_size && m_is_valid(m_state)) {
                m_state.push_back(m_state.back());
            }

            // Roll over when the remaining elements wouldn't fill the subset.
            while (is_n_from(++m_state.back(), m_subset_size - m_state.size(), m_last)) {
                m_state.pop_back();
                if (m_state.empty())
                    // we have run out of possibilities
                    return false;
            }
        }
    } while (m_state.size() < m_subset_size || !m_is_valid(m_state));
    return true;
}


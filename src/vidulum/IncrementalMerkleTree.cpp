#include <stdexcept>

#include <boost/foreach.hpp>

#include "vidulum/IncrementalMerkleTree.hpp"
#include "crypto/sha256.h"
#include "vidulum/util.h"
#include "librustzcash.h"

namespace libzcash {

PedersenHash PedersenHash::combine(
    const PedersenHash& a,
    const PedersenHash& b,
    size_t depth
)
{
    PedersenHash res = PedersenHash();

    librustzcash_merkle_hash(
        depth,
        a.begin(),
        b.begin(),
        res.begin()
    );

    return res;
}

PedersenHash PedersenHash::uncommitted() {
    PedersenHash res = PedersenHash();

    librustzcash_tree_uncommitted(res.begin());

    return res;
}

SHA256Compress SHA256Compress::combine(
    const SHA256Compress& a,
    const SHA256Compress& b,
    size_t depth
)
{
    SHA256Compress res = SHA256Compress();

    CSHA256 hasher;
    hasher.Write(a.begin(), 32);
    hasher.Write(b.begin(), 32);
    hasher.FinalizeNoPadding(res.begin());

    return res;
}

template <size_t Depth, typename Hash>
class PathFiller {
private:
    std::deque<Hash> queue;
    static EmptyMerkleRoots<Depth, Hash> emptyroots;
public:
    PathFiller() : queue() { }
    PathFiller(std::deque<Hash> queue) : queue(queue) { }

    Hash next(size_t depth) {
        if (queue.size() > 0) {
            Hash h = queue.front();
            queue.pop_front();

            return h;
        } else {
            return emptyroots.empty_root(depth);
        }
    }

};

template<size_t Depth, typename Hash>
EmptyMerkleRoots<Depth, Hash> PathFiller<Depth, Hash>::emptyroots;

template<size_t Depth, typename Hash>
void IncrementalMerkleTree<Depth, Hash>::wfcheck() const {
    if (parents.size() >= Depth) {
        throw std::ios_base::failure("tree has too many parents");
    }

    // The last parent cannot be null.
    if (!(parents.empty()) && !(parents.back())) {
        throw std::ios_base::failure("tree has non-canonical representation of parent");
    }

    // Left cannot be empty when right exists.
    if (!left && right) {
        throw std::ios_base::failure("tree has non-canonical representation; right should not exist");
    }

    // Left cannot be empty when parents is nonempty.
    if (!left && parents.size() > 0) {
        throw std::ios_base::failure("tree has non-canonical representation; parents should not be unempty");
    }
}

template<size_t Depth, typename Hash>
Hash IncrementalMerkleTree<Depth, Hash>::last() const {
    if (right) {
        return *right;
    } else if (left) {
        return *left;
    } else {
        throw std::runtime_error("tree has no cursor");
    }
}

template<size_t Depth, typename Hash>
size_t IncrementalMerkleTree<Depth, Hash>::size() const {
    size_t ret = 0;
    if (left) {
        ret++;
    }
    if (right) {
        ret++;
    }
    // Treat occupation of parents array as a binary number
    // (right-shifted by 1)
    for (size_t i = 0; i < parents.size(); i++) {
        if (parents[i]) {
            ret += (1 << (i+1));
        }
    }
    return ret;
}

template<size_t Depth, typename Hash>
void IncrementalMerkleTree<Depth, Hash>::append(Hash obj) {
    if (is_complete(Depth)) {
        throw std::runtime_error("tree is full");
    }

    if (!left) {
        // Set the left leaf
        left = obj;
    } else if (!right) {
        // Set the right leaf
        right = obj;
    } else {
        // Combine the leaves and propagate it up the tree
        boost::optional<Hash> combined = Hash::combine(*left, *right, 0);

        // Set the "left" leaf to the object and make the "right" leaf none
        left = obj;
        right = boost::none;

        for (size_t i = 0; i < Depth; i++) {
            if (i < parents.size()) {
                if (parents[i]) {
                    combined = Hash::combine(*parents[i], *combined, i+1);
                    parents[i] = boost::none;
                } else {
                    parents[i] = *combined;
                    break;
                }
            } else {
                parents.push_back(combined);
                break;
            }
        }
    }
}

// This is for allowing the witness to determine if a subtree has filled
// to a particular depth, or for append() to ensure we're not appending
// to a full tree.
template<size_t Depth, typename Hash>
bool IncrementalMerkleTree<Depth, Hash>::is_complete(size_t depth) const {
    if (!left || !right) {
        return false;
    }

    if (parents.size() != (depth - 1)) {
        return false;
    }

    BOOST_FOREACH(const boost::optional<Hash>& parent, parents) {
        if (!parent) {
            return false;
        }
    }

    return true;
}

// This finds the next "depth" of an unfilled subtree, given that we've filled
// `skip` uncles/subtrees.
template<size_t Depth, typename Hash>
size_t IncrementalMerkleTree<Depth, Hash>::next_depth(size_t skip) const {
    if (!left) {
        if (skip) {
            skip--;
        } else {
            return 0;
        }
    }

    if (!right) {
        if (skip) {
            skip--;
        } else {
            return 0;
        }
    }

    size_t d = 1;

    BOOST_FOREACH(const boost::optional<Hash>& parent, parents) {
        if (!parent) {
            if (skip) {
                skip--;
            } else {
                return d;
            }
        }

        d++;
    }

    return d + skip;
}

// This calculates the root of the tree.
template<size_t Depth, typename Hash>
Hash IncrementalMerkleTree<Depth, Hash>::root(size_t depth,
                                              std::deque<Hash> filler_hashes) const {
    PathFiller<Depth, Hash> filler(filler_hashes);

    Hash combine_left =  left  ? *left  : filler.next(0);
    Hash combine_right = right ? *right : filler.next(0);

    Hash root = Hash::combine(combine_left, combine_right, 0);

   
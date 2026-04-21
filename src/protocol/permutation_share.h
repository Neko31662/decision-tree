#pragma once

#include "protocol/addictive_share.h"
#include "protocol/masked_RSS.h"

class MSSshare_permutation {
  public:
    MSSshare_v shares;

    MSSshare_permutation(int len, int ell);

    inline int size() const { return shares.size(); }
    inline MSSshare &operator[](int i) { return shares[i]; }
    inline const MSSshare &operator[](int i) const { return shares[i]; }
    inline MSSshare_v &vec() { return shares; }
    inline const MSSshare_v &vec() const { return shares; }
};

template <typename ShareType = ShareValue> class ADDshare_permutation {
  public:
    ADDshare_v<ShareType> shares;

    ADDshare_permutation(int len, int ell);

    inline int size() const { return shares.size(); }
    inline ADDshare<ShareType> &operator[](int i) { return shares[i]; }
    inline const ADDshare<ShareType> &operator[](int i) const { return shares[i]; }
    inline ADDshare_v<ShareType> &vec() { return shares; }
    inline const ADDshare_v<ShareType> &vec() const { return shares; }
};

#include "protocol/permutation_share.tpp"


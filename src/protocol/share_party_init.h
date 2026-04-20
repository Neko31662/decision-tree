#pragma once

#include "protocol/addictive_share.h"
#include "protocol/addictive_share_p.h"
#include "protocol/masked_RSS.h"
#include "protocol/masked_RSS_p.h"

inline void init_share_party_refs(int party_id) {
    MSSshare::init_party(party_id);
    MSSshare_p::init_party(party_id);
    ADDshare<ShareValue>::init_party(party_id);
    ADDshare_p::init_party(party_id);
}

#pragma once
#include "config/config.h"
#include "protocol/masked_RSS.h"
#include "protocol/sign_mult.h"
#include "utils/misc.h"

struct PI_cotp_intermediate {
    int ell;
    int vec_size;

    // Random placeholder inputs used by sign-mult preprocess.
    MSSshare random_cmp_input;
    std::vector<MSSshare> random_mul_input_vec;

    // Internal outputs of sign-mult.
    MSSshare sign_out;
    std::vector<MSSshare> mult_out_vec;

    PI_sign_mult_intermediate sign_mult_intermediate;
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif

    PI_cotp_intermediate(int ell, int vec_size)
        : ell(ell), vec_size(vec_size), random_cmp_input(ell),
          random_mul_input_vec(vec_size, MSSshare(ell)), sign_out(ell),
          mult_out_vec(vec_size, MSSshare(ell)), sign_mult_intermediate(ell, vec_size) {}
};

/* Preprocess for COTP.
 * It does NOT depend on online x/y/L/R values.
 */
void PI_cotp_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                        PI_cotp_intermediate &intermediate);

/* COTP online:
 * return opened plaintext vector: (x >= y) ? L : R
 */
std::vector<ShareValue> PI_cotp(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                PI_cotp_intermediate &intermediate, MSSshare *input_x,
                                MSSshare *input_y, std::vector<MSSshare *> &input_L_vec,
                                std::vector<MSSshare *> &input_R_vec);

#include "protocol/cotp.tpp"

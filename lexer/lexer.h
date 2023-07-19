#include <stdbool.h>
#include <string>

enum Token {
    token_eof,
    token_id,
    token_number,
    token_reg,
    token_pred,
    token_label,
    token_direct,
    token_modifier,
    token_type,
    token_dim,

    token_semicolon,
    token_comma,
    token_leftbracket,
    token_rightbracket,
    token_leftcurlybracket,
    token_rightcurlybracket,
    token_leftparenth,
    token_rightparenth,
    token_vbar,
    token_newline,

    token_plus,

    // instructions
    token_abs, token_activemask, token_add, token_addc, token_alloca, token_and, token_applypriority, token_atom,
    token_bar, token_barrier, token_bfe, token_bfi, token_bfind, token_bmsk, token_bra, token_brev,
    token_brkpt, token_brx, token_call, token_cIz, token_cnot, token_copysign, token_cos, token_cp,
    token_createpolicy, token_cvt, token_cvta, token_discard, token_div, token_dp2a, token_dp4a, token_elect,
    token_ex2, token_exit, token_fence, token_fma, token_fns, token_getctarank, token_griddepcontrol, token_Idu,
    token_isspacep, token_istypep, token_ld, token_ldmatrix, token_lg2, token_lop3, token_mad, token_mad24,
    token_madc, token_mapa, token_match, token_max, token_mbarrier, token_membar, token_min, token_mma,
    token_mov, token_movmatrix, token_mu124, token_mul, token_multimem, token_nanosleep, token_neg, token_not,
    token_or, token_pmevent, token_popc, token_prefetch, token_prefetchu, token_prmt, token_rcp, token_red,
    token_redux, token_rem, token_ret, token_rsqrt, token_sad, token_selp, token_set, token_setmaxnreg,
    token_setp, token_shf, token_shfl, token_shl, token_shr, token_sin, token_slct, token_sqrt,
    token_st, token_stackrestore, token_stacksave, token_stmatrix, token_sub, token_subc, token_suld, token_suq,
    token_sured, token_sust, token_szext, token_tanh, token_testp, token_tex, token_tld4, token_trap,
    token_txq, token_vabsdiff, token_vabsdiff2, token_vabsdiffh, token_vadd, token_vadd2, token_vadd4, token_vavrg2,
    token_vavrgh, token_vmad, token_vmax, token_vmax2, token_vmax4, token_vmin, token_vmin2, token_vmin4,
    token_vote, token_vset, token_vset2, token_vset4, token_vshl, token_vshr, token_vsub, token_vsub2,
    token_vsub4, token_wgmma, token_wmma, token_xor
};

extern int currentToken;
extern int prevToken;
extern std::string currStrVal;
extern double currNumVal;

int getToken();

bool isIdChar(char ch);
bool isIdStartingChar(char ch);
bool isType(std::string str);
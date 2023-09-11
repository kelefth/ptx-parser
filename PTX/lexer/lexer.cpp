#include <stdlib.h>
#include <ctype.h>
#include <iostream>
#include <string>
#include <regex>

#include "lexer.h"

using namespace std;

int prevToken = -1;
string currStrVal;
double currNumVal;

bool isIdChar(char ch) {
    return isalnum(ch) || ch == '_' || ch == '$';
}

bool isIdStartingChar(char ch) {
    return isalpha(ch) || ch == '_' || ch == '$' || ch == '%';
}

bool isType(std::string str) {

    const std::regex reg("\\.[a-z]\\d{1,2}[a-z]*\\d{1}|\\.[a-z]\\d{1,2}");

    return (std::regex_match(str, reg) || str == ".pred");
}

int getToken() {
    int currentToken = -1;
    char currChar;
    currStrVal = "";

    while (isspace(currChar = getchar()));

    if (isIdStartingChar(currChar) || currChar == '.') {
        currStrVal = string(1, currChar);

        // include the memory space conversions, e.g. .to.global
        while (
            isIdChar(currChar = getchar()) ||
            (currChar == '.' && currStrVal == ".to")
        ) {
            currStrVal += currChar;
        }

        // unread the last (invalid) character
        ungetc(currChar, stdin);


        // check for directives
        if (currStrVal == ".version")
            currentToken = token_version_dir;
        else if (currStrVal == ".target")
            currentToken = token_target_dir;
        else if (currStrVal == ".address_size")
            currentToken = token_address_size_dir;
        else if (currStrVal == ".entry")
            currentToken = token_entry_dir;
        else if (currStrVal == ".param" && prevToken != token_ld)
            currentToken = token_param_dir;
        else if (currStrVal == ".align")
            currentToken = token_align_dir;
        else {
            // check for special cases
            if (currStrVal[0] == '%') {
                currentToken = token_reg;
            }
            else if (isType(currStrVal))
                currentToken = token_type;
            else if (currStrVal[0] == '.' && currChar == ' ')
                currentToken = token_direct;
            else if (currStrVal[0] == '.') {
                if (prevToken == token_reg)
                    currentToken = token_dim;
                else
                    currentToken = token_modifier;
            }
            else if (currChar == ':') {
                currentToken = token_label;
            }
        }


        // check for instructions
        if (currStrVal == "abs")
            currentToken = token_abs;
        if (currStrVal == "activemask")
            currentToken = token_activemask;
        if (currStrVal == "add")
            currentToken = token_add;
        if (currStrVal == "activemask")
            currentToken = token_activemask;
        if (currStrVal == "addc")
            currentToken = token_addc;
        if (currStrVal == "alloca")
            currentToken = token_alloca;
        if (currStrVal == "and")
            currentToken = token_and;
        if (currStrVal == "applypriority")
            currentToken = token_applypriority;
        if (currStrVal == "atom")
            currentToken = token_atom;
        if (currStrVal == "bar")
            currentToken = token_bar;
        if (currStrVal == "barrier")
            currentToken = token_barrier;
        if (currStrVal == "bfe")
            currentToken = token_bfe;
        if (currStrVal == "bfi")
            currentToken = token_bfi;
        if (currStrVal == "bfind")
            currentToken = token_bfind;
        if (currStrVal == "bmsk")
            currentToken = token_bmsk;
        if (currStrVal == "bra")
            currentToken = token_bra;
        if (currStrVal == "brev")
            currentToken = token_brev;
        if (currStrVal == "brkpt")
            currentToken = token_brkpt;
        if (currStrVal == "brx")
            currentToken = token_brx;
        if (currStrVal == "call")
            currentToken = token_call;
        if (currStrVal == "cIz")
            currentToken = token_cIz;
        if (currStrVal == "cnot")
            currentToken = token_cnot;
        if (currStrVal == "copysign")
            currentToken = token_copysign;
        if (currStrVal == "cos")
            currentToken = token_cos;
        if (currStrVal == "cp")
            currentToken = token_cp;
        if (currStrVal == "createpolicy")
            currentToken = token_createpolicy;
        if (currStrVal == "cvt")
            currentToken = token_cvt;
        if (currStrVal == "bfind")
            currentToken = token_bfind;
        if (currStrVal == "cvta")
            currentToken = token_cvta;
        if (currStrVal == "discard")
            currentToken = token_discard;
        if (currStrVal == "div")
            currentToken = token_div;
        if (currStrVal == "dp2a")
            currentToken = token_dp2a;
        if (currStrVal == "dp4a")
            currentToken = token_dp4a;
        if (currStrVal == "elect")
            currentToken = token_elect;
        if (currStrVal == "ex2")
            currentToken = token_ex2;
        if (currStrVal == "exit")
            currentToken = token_exit;
        if (currStrVal == "fence")
            currentToken = token_fence;
        if (currStrVal == "fma")
            currentToken = token_fma;
        if (currStrVal == "fns")
            currentToken = token_fns;
        if (currStrVal == "getctarank")
            currentToken = token_getctarank;
        if (currStrVal == "griddepcontrol")
            currentToken = token_griddepcontrol;
        if (currStrVal == "Idu")
            currentToken = token_Idu;
        if (currStrVal == "isspacep")
            currentToken = token_isspacep;
        if (currStrVal == "istypep")
            currentToken = token_istypep;
        if (currStrVal == "ld")
            currentToken = token_ld;
        if (currStrVal == "ldmatrix")
            currentToken = token_ldmatrix;
        if (currStrVal == "lg2")
            currentToken = token_lg2;
        if (currStrVal == "lop3")
            currentToken = token_lop3;
        if (currStrVal == "mad")
            currentToken = token_mad;
        if (currStrVal == "mad24")
            currentToken = token_mad24;
        if (currStrVal == "madc")
            currentToken = token_madc;
        if (currStrVal == "mapa")
            currentToken = token_mapa;
        if (currStrVal == "match")
            currentToken = token_match;
        if (currStrVal == "max")
            currentToken = token_max;
        if (currStrVal == "mbarrier")
            currentToken = token_mbarrier;
        if (currStrVal == "membar")
            currentToken = token_membar;
        if (currStrVal == "min")
            currentToken = token_min;
        if (currStrVal == "mma")
            currentToken = token_mma;
        if (currStrVal == "mov")
            currentToken = token_mov;
        if (currStrVal == "movmatrix")
            currentToken = token_movmatrix;
        if (currStrVal == "mu124")
            currentToken = token_mu124;
        if (currStrVal == "mul")
            currentToken = token_mul;
        if (currStrVal == "multimem")
            currentToken = token_multimem;
        if (currStrVal == "nanosleep")
            currentToken = token_nanosleep;
        if (currStrVal == "neg")
            currentToken = token_neg;
        if (currStrVal == "not")
            currentToken = token_not;
        if (currStrVal == "or")
            currentToken = token_or;
        if (currStrVal == "pmevent")
            currentToken = token_pmevent;
        if (currStrVal == "popc")
            currentToken = token_popc;
        if (currStrVal == "prefetch")
            currentToken = token_prefetch;
        if (currStrVal == "prefetchu")
            currentToken = token_prefetchu;
        if (currStrVal == "prmt")
            currentToken = token_prmt;
        if (currStrVal == "rcp")
            currentToken = token_rcp;
        if (currStrVal == "red")
            currentToken = token_red;
        if (currStrVal == "redux")
            currentToken = token_redux;
        if (currStrVal == "rem")
            currentToken = token_rem;
        if (currStrVal == "ret")
            currentToken = token_ret;
        if (currStrVal == "rsqrt")
            currentToken = token_rsqrt;
        if (currStrVal == "sad")
            currentToken = token_sad;
        if (currStrVal == "selp")
            currentToken = token_selp;
        if (currStrVal == "set")
            currentToken = token_set;
        if (currStrVal == "setmaxnreg")
            currentToken = token_setmaxnreg;
        if (currStrVal == "setp")
            currentToken = token_setp;
        if (currStrVal == "shf")
            currentToken = token_shf;
        if (currStrVal == "shfl")
            currentToken = token_shfl;
        if (currStrVal == "shl")
            currentToken = token_shl;
        if (currStrVal == "shr")
            currentToken = token_shr;
        if (currStrVal == "sin")
            currentToken = token_sin;
        if (currStrVal == "slct")
            currentToken = token_slct;
        if (currStrVal == "sqrt")
            currentToken = token_sqrt;
        if (currStrVal == "st")
            currentToken = token_st;
        if (currStrVal == "stackrestore")
            currentToken = token_stackrestore;
        if (currStrVal == "stacksave")
            currentToken = token_stacksave;
        if (currStrVal == "stmatrix")
            currentToken = token_stmatrix;
        if (currStrVal == "sub")
            currentToken = token_sub;
        if (currStrVal == "subc")
            currentToken = token_subc;
        if (currStrVal == "suld")
            currentToken = token_suld;
        if (currStrVal == "suq")
            currentToken = token_suq;
        if (currStrVal == "sured")
            currentToken = token_sured;
        if (currStrVal == "sust")
            currentToken = token_sust;
        if (currStrVal == "szext")
            currentToken = token_szext;
        if (currStrVal == "tanh")
            currentToken = token_tanh;
        if (currStrVal == "sured")
            currentToken = token_sured;
        if (currStrVal == "testp")
            currentToken = token_testp;
        if (currStrVal == "tex")
            currentToken = token_tex;
        if (currStrVal == "tld4")
            currentToken = token_tld4;
        if (currStrVal == "trap")
            currentToken = token_trap;
        if (currStrVal == "txq")
            currentToken = token_txq;
        if (currStrVal == "vabsdiff")
            currentToken = token_vabsdiff;
        if (currStrVal == "vabsdiff2")
            currentToken = token_vabsdiff2;
        if (currStrVal == "vabsdiffh")
            currentToken = token_vabsdiffh;
        if (currStrVal == "vadd")
            currentToken = token_vadd;
        if (currStrVal == "vadd2")
            currentToken = token_vadd2;
        if (currStrVal == "vadd4")
            currentToken = token_vadd4;
        if (currStrVal == "vavrg2")
            currentToken = token_vavrg2;
        if (currStrVal == "vavrgh")
            currentToken = token_vavrgh;
        if (currStrVal == "vmad")
            currentToken = token_vmad;
        if (currStrVal == "vmax")
            currentToken = token_vmax;
        if (currStrVal == "vmax2")
            currentToken = token_vmax2;
        if (currStrVal == "vmax4")
            currentToken = token_vmax4;
        if (currStrVal == "vmin")
            currentToken = token_vmin;
        if (currStrVal == "vmin2")
            currentToken = token_vmin2;
        if (currStrVal == "vmin4")
            currentToken = token_vmin4;
        if (currStrVal == "vote")
            currentToken = token_vote;
        if (currStrVal == "vset")
            currentToken = token_vset;
        if (currStrVal == "vset2")
            currentToken = token_vset2;
        if (currStrVal == "vset4")
            currentToken = token_vset4;
        if (currStrVal == "vshl")
            currentToken = token_vshl;
        if (currStrVal == "vshr")
            currentToken = token_vshr;
        if (currStrVal == "vsub")
            currentToken = token_vsub;
        if (currStrVal == "vsub2")
            currentToken = token_vsub2;
        if (currStrVal == "vsub4")
            currentToken = token_vsub4;
        if (currStrVal == "wgmma")
            currentToken = token_wgmma;
        if (currStrVal == "wmma")
            currentToken = token_wmma;
        if (currStrVal == "xor")
            currentToken = token_xor;
        
        if (currentToken == -1)
            currentToken = token_id;
    }
    else if (isdigit(currChar) || currChar == '.' || currChar == '-') {
        string numStr = "";

        if (currChar == '-') {
            numStr += currChar;
            currChar = getchar();
        }

        bool dotParsed = false;
        while (
            isdigit(currChar)   ||
            isxdigit(currChar)  ||
            currChar == '.'     ||
            currChar == 'x'     ||
            currChar == 'X'     ||
            currChar == 'f'     ||
            currChar == 'F'     ||
            currChar == 'd'     ||
            currChar == 'D'
        ) {
            if (currChar == '.') {
                if(dotParsed)
                    cerr << "Error in number value" << endl;
                else dotParsed == true;
            }
            numStr += currChar;
            currChar = getchar();
        }

        ungetc(currChar, stdin);

        // Convert all possible types and formats to double
        std::string::size_type pos;
        if (pos = numStr.find('d') != std::string::npos)
            numStr.erase(0, pos+1);
        else if (pos = numStr.find('D') != std::string::npos)
            numStr.erase(0, pos+1);
        else if (pos = numStr.find('f') != std::string::npos)
            numStr.erase(0, pos+1);
        else if (pos = numStr.find('F') != std::string::npos)
            numStr.erase(0, pos+1);
        else if (pos = numStr.find('b') != std::string::npos) {
            numStr.erase(0, pos+1);
            numStr = to_string(stoi(numStr, 0, 2));
        }
        else if (pos = numStr.find('B') != std::string::npos) {
            numStr.erase(0, pos+1);
            numStr = to_string(stoi(numStr, 0, 2));
        }
        else if (numStr[0] == '0')
            numStr = to_string(stoi(numStr, 0, 8));

        currNumVal = strtod(numStr.c_str(), nullptr);
        currentToken = token_number;
    }
    else if (currChar == '@') {
        currStrVal = string(1, currChar);
        if (isIdStartingChar(currChar = getchar())) {
            currStrVal += currChar;
            while (isIdChar(currChar = getchar())) {
                currStrVal += currChar;
            }

            currentToken = token_pred;
        }
    }
    // comments
    else if (currChar == '/') {
        char nextChar = getchar();
        bool commentEnd = false;
        if (nextChar == '/') {
            while (getchar() != '\n');
        }
        else if (nextChar == '*') {
            while (!commentEnd) {
                while(getchar() != '*');
                if (getchar() == '/') commentEnd = true;
            }
        }
    }
    else if (currChar == ';')
        currentToken = token_semicolon;
    else if (currChar == ',')
        currentToken = token_comma;
    else if (currChar == '[')
        currentToken = token_leftbracket;
    else if (currChar == ']')
        currentToken = token_rightbracket;
    else if (currChar == '{')
        currentToken = token_leftcurlybracket;
    else if (currChar == '}')
        currentToken = token_rightcurlybracket;
    else if (currChar == '(')
        currentToken = token_leftparenth;
    else if (currChar == ')')
        currentToken = token_rightparenth;
    else if (currChar == '|')
        currentToken = token_vbar;
    else if (currChar == '\n')
        currentToken = token_newline;
    else if(currChar == '+') {
        currentToken = token_plus;
    }

    if (currChar == EOF)
        currentToken = token_eof;

    prevToken = currentToken;

    return currentToken;
}

// int main() {

    // int tokenId = -1;
    // while (tokenId != token_eof) {
    //     tokenId = getToken();
    //     if (tokenId == token_pred)
    //         cout << "pred: " << currStrVal << endl;
    //     else if (tokenId == token_label)
    //         cout << "label: " << currStrVal << endl;
    //     else if (tokenId == token_leftbracket)
    //         cout << "token: " << "[" << endl;
    //     else if (tokenId == token_rightbracket)
    //         cout << "token: " << "]" << endl;
    //     else if (tokenId == token_type)
    //         cout << "type: " << currStrVal << endl;
    //     else if (tokenId == token_direct)
    //         cout << "directive: " << currStrVal << endl;
    //     else if (tokenId == token_mod_dir)
    //         cout << "modifier: " << currStrVal << endl;
    //     else if (tokenId == token_reg)
    //         cout << "register: " << currStrVal << endl;
    //     else if(currStrVal != "")
    //         cout << "token: "+currStrVal << endl;
    //     else if (tokenId == token_number)
    //         cout << "number: " << currNumVal << endl;
    // }
// }
#pragma once
#include <iomanip>
#include <iostream>
#include "./utils.hpp"
#include "../src/types/syntax_t.hpp"

inline void logToken(const Token& tok) {
  std::cout 
    << std::left << std::setw(20) << "Token"
    << "| type: " << std::setw(25) << tokenTypeToString(tok.tok_type)
    << "| val: "  << std::setw(15) << ("\"" + tok.tok_val + "\"")
    << "| row: "  << std::setw(4)  << tok.tok_row
    << "| col: "  << std::setw(4)  << tok.tok_col
    << std::endl;
}


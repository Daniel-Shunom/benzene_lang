#pragma once

// Umbrella header: existing callers can include all parser declarations here.
#include <ether/parser/calls.hpp>
#include <ether/parser/collections.hpp>
#include <ether/parser/control_flow.hpp>
#include <ether/parser/declarations.hpp>
#include <ether/parser/expressions.hpp>
#include <ether/parser/functions.hpp>
#include <ether/parser/operators.hpp>
#include <ether/parser/tokens.hpp>
#include <ether/parser/types.hpp>

auto run_parser(ParserState& state) -> PResult<Parent>;

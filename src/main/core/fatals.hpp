#pragma once

#include "definesStandard.hpp"
#include "logger.hpp"

#include <cmath>

/************************************************************************************************************************ 
 *  @brief Checks a boolean expression. If false, a fatal-error is passed to the prompt. Cannot be disabled.
 * 
 *  @param expr    a boolean expression to check (e.g. 1 == 0)
 *  @param message a message to throw before the code crashs. Message uses format specifiers.
 * 
 *  @return None
 ************************************************************************************************************************/ 
#define FATAL_ASSERT(expr, message)                                                                                    \
{                                                                                                                      \
    if(expr){}                                                                                                         \
    else {                                                                                                             \
        FATAL_MSG("Assertion Failure: %s, message: %s, in file: %s, line: %d\n", #expr, message, __FILE__, __LINE__);  \
        exit(EXIT_FAILURE_ASSERTION);                                                                                  \
    }                                                                                                                  \
}                                                                                                                      \

/************************************************************************************************************************ 
 *  @brief Checks whether the iteration error is nan or not. If it is, a fatal-error is passed to the prompt. Cannot be 
 *         disabled.
 * 
 *  @param kappa     iteration number
 *  @param iterError residual error estimate
 * 
 *  @return None
 ************************************************************************************************************************/ 
#define FATAL_ITERATION(kappa, iterError)                                                               \
{                                                                                                       \
    if(iterError==iterError){}                                                                          \
    else {                                                                                              \
        FATAL_MSG("Iteration Failure: kappa = %d, in file: %s, line: %d\n", kappa, __FILE__, __LINE__); \
        exit(EXIT_FAILURE_ITERATION);                                                                   \
    }                                                                                                   \
}                                                                                                       \


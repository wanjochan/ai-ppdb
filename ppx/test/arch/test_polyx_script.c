/**
 * Test Suite for PolyxScript
 * 
 * This test suite verifies the functionality of the PolyxScript language implementation.
 * It focuses on testing:
 * 1. Lexical Analysis
 *    - Basic token recognition (numbers, strings, identifiers)
 *    - Operator handling
 *    - Complex expressions and statements
 * 
 * 2. Future Tests (TODO):
 *    - Parser functionality
 *    - AST construction
 *    - Variable management
 *    - Expression evaluation
 *    - Control flow execution
 *    - Error handling and recovery
 */

#include "internal/polyx/PolyxScript.h"
#include "internal/infrax/InfraxCore.h"

static InfraxCore* core = NULL;

static void test_basic_lexer(void) {
    core->printf(core, "Testing basic lexer...\n");
    
    const char* source = "x = 42.5 + y * \"hello world\"";
    
    PolyxScript* script = PolyxScriptClass.new();
    INFRAX_ASSERT(core, script != NULL);
    
    InfraxError err = script->klass->load_source(script, source);
    INFRAX_ASSERT(core, INFRAX_ERROR_IS_OK(err));
    
    script->klass->print_tokens(script);
    
    script->klass->free(script);
    core->printf(core, "Basic lexer test passed\n");
}

static void test_complex_lexer(void) {
    core->printf(core, "Testing complex lexer...\n");
    
    const char* source = 
        "if (x <= 10) {\n"
        "    y = x * 2\n"
        "    print(\"Result: \" + y)\n"
        "} else {\n"
        "    print(\"x is too large\")\n"
        "}";
    
    PolyxScript* script = PolyxScriptClass.new();
    INFRAX_ASSERT(core, script != NULL);
    
    InfraxError err = script->klass->load_source(script, source);
    INFRAX_ASSERT(core, INFRAX_ERROR_IS_OK(err));
    
    script->klass->print_tokens(script);
    
    script->klass->free(script);
    core->printf(core, "Complex lexer test passed\n");
}

int main(void) {
    core = &gInfraxCore;
    INFRAX_ASSERT(core, core != NULL);
    
    core->printf(core, "Starting PolyxScript tests...\n");
    
    test_basic_lexer();
    test_complex_lexer();
    
    core->printf(core, "All PolyxScript tests passed!\n");
    return 0;
} 
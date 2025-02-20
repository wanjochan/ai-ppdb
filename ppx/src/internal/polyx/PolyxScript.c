#include "PolyxScript.h"
#include "internal/infrax/InfraxMemory.h"

// Global instances
static InfraxCore* g_core = NULL;
static InfraxMemory* g_memory = NULL;

// Initialize global instances
static InfraxBool init_globals(void) {
    if (g_core && g_memory) return INFRAX_TRUE;
    
    g_core = &gInfraxCore;
    if (!g_core) return INFRAX_FALSE;
    
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = INFRAX_FALSE,
        .use_pool = INFRAX_TRUE,
        .gc_threshold = 0
    };
    
    g_memory = InfraxMemoryClass.new(&config);
    return (g_memory != NULL);
}

// Helper functions for lexer
static InfraxBool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static InfraxBool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static InfraxBool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static InfraxBool is_operator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '=' || 
           c == '<' || c == '>' || c == '!';
}

// Token management
static PolyxToken create_token(PolyxTokenType type, const char* start, InfraxSize length,
                             InfraxSize line, InfraxSize column) {
    PolyxToken token = {
        .type = type,
        .value = NULL,
        .line = line,
        .column = column
    };
    
    if (length > 0) {
        token.value = g_memory->alloc(g_memory, length + 1);
        if (token.value) {
            g_core->memcpy(g_core, token.value, start, length);
            token.value[length] = '\0';
        }
    }
    
    return token;
}

static void free_token(PolyxToken* token) {
    if (token && token->value) {
        g_memory->dealloc(g_memory, token->value);
        token->value = NULL;
    }
}

// Lexer implementation
static PolyxToken get_next_token(PolyxScript* self) {
    while (self->source[self->position] != '\0') {
        char c = self->source[self->position];
        
        // Skip whitespace
        if (is_whitespace(c)) {
            if (c == '\n') {
                self->line++;
                self->column = 0;
            } else {
                self->column++;
            }
            self->position++;
            continue;
        }
        
        // Numbers
        if (is_digit(c)) {
            const char* start = &self->source[self->position];
            InfraxSize length = 0;
            InfraxSize start_column = self->column;
            
            while (is_digit(self->source[self->position]) || 
                   self->source[self->position] == '.') {
                self->position++;
                self->column++;
                length++;
            }
            
            return create_token(POLYX_TOKEN_NUMBER, start, length, 
                              self->line, start_column);
        }
        
        // Identifiers and keywords
        if (is_alpha(c)) {
            const char* start = &self->source[self->position];
            InfraxSize length = 0;
            InfraxSize start_column = self->column;
            
            while (is_alpha(self->source[self->position]) || 
                   is_digit(self->source[self->position])) {
                self->position++;
                self->column++;
                length++;
            }
            
            // TODO: Check if it's a keyword
            return create_token(POLYX_TOKEN_IDENTIFIER, start, length,
                              self->line, start_column);
        }
        
        // Operators
        if (is_operator(c)) {
            const char* start = &self->source[self->position];
            InfraxSize length = 1;
            InfraxSize start_column = self->column;
            
            self->position++;
            self->column++;
            
            // Check for two-character operators
            if (is_operator(self->source[self->position])) {
                self->position++;
                self->column++;
                length++;
            }
            
            return create_token(POLYX_TOKEN_OPERATOR, start, length,
                              self->line, start_column);
        }
        
        // Strings
        if (c == '"' || c == '\'') {
            char quote = c;
            self->position++;
            self->column++;
            
            const char* start = &self->source[self->position];
            InfraxSize length = 0;
            InfraxSize start_column = self->column;
            
            while (self->source[self->position] != quote && 
                   self->source[self->position] != '\0') {
                if (self->source[self->position] == '\\') {
                    self->position++;
                    self->column++;
                }
                self->position++;
                self->column++;
                length++;
            }
            
            if (self->source[self->position] == quote) {
                self->position++;
                self->column++;
            }
            
            return create_token(POLYX_TOKEN_STRING, start, length,
                              self->line, start_column);
        }
        
        // Punctuation
        {
            const char* start = &self->source[self->position];
            InfraxSize start_column = self->column;
            
            self->position++;
            self->column++;
            
            return create_token(POLYX_TOKEN_PUNCTUATION, start, 1,
                              self->line, start_column);
        }
    }
    
    // End of file
    return create_token(POLYX_TOKEN_EOF, NULL, 0, self->line, self->column);
}

// Constructor
static PolyxScript* script_new(void) {
    if (!init_globals()) return NULL;
    
    PolyxScript* script = g_memory->alloc(g_memory, sizeof(PolyxScript));
    if (!script) return NULL;
    
    g_core->memset(g_core, script, 0, sizeof(PolyxScript));
    script->self = script;
    script->klass = &PolyxScriptClass;
    
    return script;
}

// Destructor
static void script_free(PolyxScript* self) {
    if (!self) return;
    
    // Free current token
    free_token(&self->current_token);
    
    // Free variables
    if (self->variables.names) {
        for (InfraxSize i = 0; i < self->variables.count; i++) {
            if (self->variables.names[i]) {
                g_memory->dealloc(g_memory, self->variables.names[i]);
            }
        }
        g_memory->dealloc(g_memory, self->variables.names);
    }
    
    if (self->variables.values) {
        g_memory->dealloc(g_memory, self->variables.values);
    }
    
    g_memory->dealloc(g_memory, self);
}

// Load source code
static InfraxError script_load_source(PolyxScript* self, const char* source) {
    if (!self || !source) {
        return make_error(-1, "Invalid arguments");
    }
    
    // Reset state
    self->source = source;
    self->position = 0;
    self->line = 1;
    self->column = 0;
    
    // Get first token
    free_token(&self->current_token);
    self->current_token = get_next_token(self);
    
    return INFRAX_ERROR_OK_STRUCT;
}

// Run script
static InfraxError script_run(PolyxScript* self) {
    if (!self) {
        return make_error(-1, "Invalid script instance");
    }
    
    // TODO: Implement parser and interpreter
    return INFRAX_ERROR_OK_STRUCT;
}

// Debug: Print tokens
static void script_print_tokens(PolyxScript* self) {
    if (!self) return;
    
    g_core->printf(g_core, "Tokens:\n");
    
    while (self->current_token.type != POLYX_TOKEN_EOF) {
        g_core->printf(g_core, "  Type: %d, Value: %s, Line: %zu, Column: %zu\n",
                      self->current_token.type,
                      self->current_token.value ? self->current_token.value : "(null)",
                      self->current_token.line,
                      self->current_token.column);
        
        free_token(&self->current_token);
        self->current_token = get_next_token(self);
    }
}

// Debug: Print AST
static void script_print_ast(PolyxScript* self, PolyxAstNode* node) {
    if (!self || !node) return;
    
    // TODO: Implement AST printing
}

// Global class instance
PolyxScriptClassType PolyxScriptClass = {
    .new = script_new,
    .free = script_free,
    .load_source = script_load_source,
    .run = script_run,
    .print_tokens = script_print_tokens,
    .print_ast = script_print_ast
}; 
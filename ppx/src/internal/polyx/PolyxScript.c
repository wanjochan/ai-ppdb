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

// Helper function to copy string
static char* copy_string(const char* str) {
    if (!str) return NULL;
    
    InfraxSize len = g_core->strlen(g_core, str);
    char* copy = g_memory->alloc(g_memory, len + 1);
    if (copy) {
        g_core->memcpy(g_core, copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

// AST node management
static PolyxAstNode* create_node(PolyxAstType type) {
    PolyxAstNode* node = g_memory->alloc(g_memory, sizeof(PolyxAstNode));
    if (node) {
        g_core->memset(g_core, node, 0, sizeof(PolyxAstNode));
        node->type = type;
    }
    return node;
}

static PolyxAstNode* script_create_number_node(double value) {
    PolyxAstNode* node = create_node(POLYX_AST_NUMBER);
    if (node) {
        node->as.number_value = value;
    }
    return node;
}

static PolyxAstNode* script_create_string_node(const char* value) {
    PolyxAstNode* node = create_node(POLYX_AST_STRING);
    if (node) {
        node->as.string_value = copy_string(value);
    }
    return node;
}

static PolyxAstNode* script_create_identifier_node(const char* name) {
    PolyxAstNode* node = create_node(POLYX_AST_IDENTIFIER);
    if (node) {
        node->as.identifier = copy_string(name);
    }
    return node;
}

static PolyxAstNode* script_create_binary_op_node(char operator, PolyxAstNode* left, PolyxAstNode* right) {
    PolyxAstNode* node = create_node(POLYX_AST_BINARY_OP);
    if (node) {
        node->as.binary_op.operator = operator;
        node->as.binary_op.left = left;
        node->as.binary_op.right = right;
    }
    return node;
}

static PolyxAstNode* script_create_unary_op_node(char operator, PolyxAstNode* operand) {
    PolyxAstNode* node = create_node(POLYX_AST_UNARY_OP);
    if (node) {
        node->as.unary_op.operator = operator;
        node->as.unary_op.operand = operand;
    }
    return node;
}

static PolyxAstNode* script_create_assignment_node(const char* name, PolyxAstNode* value) {
    PolyxAstNode* node = create_node(POLYX_AST_ASSIGNMENT);
    if (node) {
        node->as.assignment.name = copy_string(name);
        node->as.assignment.value = value;
    }
    return node;
}

static PolyxAstNode* script_create_if_node(PolyxAstNode* condition, PolyxAstNode* then_branch, PolyxAstNode* else_branch) {
    PolyxAstNode* node = create_node(POLYX_AST_IF);
    if (node) {
        node->as.if_stmt.condition = condition;
        node->as.if_stmt.then_branch = then_branch;
        node->as.if_stmt.else_branch = else_branch;
    }
    return node;
}

static PolyxAstNode* script_create_while_node(PolyxAstNode* condition, PolyxAstNode* body) {
    PolyxAstNode* node = create_node(POLYX_AST_WHILE);
    if (node) {
        node->as.while_stmt.condition = condition;
        node->as.while_stmt.body = body;
    }
    return node;
}

static PolyxAstNode* script_create_block_node(void) {
    PolyxAstNode* node = create_node(POLYX_AST_BLOCK);
    if (node) {
        node->as.block.statements = NULL;
        node->as.block.count = 0;
    }
    return node;
}

static InfraxError script_add_statement_to_block(PolyxAstNode* block, PolyxAstNode* statement) {
    if (!block || !statement || block->type != POLYX_AST_BLOCK) {
        return make_error(-1, "Invalid arguments to add_statement_to_block");
    }
    
    // Allocate or reallocate statements array
    InfraxSize new_size = block->as.block.count + 1;
    PolyxAstNode** new_statements = g_memory->alloc(g_memory, 
        new_size * sizeof(PolyxAstNode*));
    
    if (!new_statements) {
        return make_error(-1, "Memory allocation failed");
    }
    
    // Copy existing statements
    if (block->as.block.statements) {
        g_core->memcpy(g_core, new_statements, block->as.block.statements,
            block->as.block.count * sizeof(PolyxAstNode*));
        g_memory->dealloc(g_memory, block->as.block.statements);
    }
    
    // Add new statement
    new_statements[block->as.block.count] = statement;
    block->as.block.statements = new_statements;
    block->as.block.count = new_size;
    
    return INFRAX_ERROR_OK_STRUCT;
}

static void script_free_ast_node(PolyxAstNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case POLYX_AST_STRING:
            if (node->as.string_value) {
                g_memory->dealloc(g_memory, node->as.string_value);
            }
            break;
            
        case POLYX_AST_IDENTIFIER:
            if (node->as.identifier) {
                g_memory->dealloc(g_memory, node->as.identifier);
            }
            break;
            
        case POLYX_AST_BINARY_OP:
            script_free_ast_node(node->as.binary_op.left);
            script_free_ast_node(node->as.binary_op.right);
            break;
            
        case POLYX_AST_UNARY_OP:
            script_free_ast_node(node->as.unary_op.operand);
            break;
            
        case POLYX_AST_ASSIGNMENT:
            if (node->as.assignment.name) {
                g_memory->dealloc(g_memory, node->as.assignment.name);
            }
            script_free_ast_node(node->as.assignment.value);
            break;
            
        case POLYX_AST_IF:
            script_free_ast_node(node->as.if_stmt.condition);
            script_free_ast_node(node->as.if_stmt.then_branch);
            script_free_ast_node(node->as.if_stmt.else_branch);
            break;
            
        case POLYX_AST_WHILE:
            script_free_ast_node(node->as.while_stmt.condition);
            script_free_ast_node(node->as.while_stmt.body);
            break;
            
        case POLYX_AST_BLOCK:
            if (node->as.block.statements) {
                for (InfraxSize i = 0; i < node->as.block.count; i++) {
                    script_free_ast_node(node->as.block.statements[i]);
                }
                g_memory->dealloc(g_memory, node->as.block.statements);
            }
            break;
            
        default:
            break;
    }
    
    g_memory->dealloc(g_memory, node);
}

// Parser helper functions
static void set_error(PolyxScript* self, const char* message) {
    self->had_error = INFRAX_TRUE;
    if (self->error_message) {
        g_memory->dealloc(g_memory, self->error_message);
    }
    self->error_message = copy_string(message);
}

static InfraxBool match_token(PolyxScript* self, PolyxTokenType type) {
    if (self->current_token.type == type) {
        free_token(&self->current_token);
        self->current_token = get_next_token(self);
        return INFRAX_TRUE;
    }
    return INFRAX_FALSE;
}

static InfraxBool expect_token(PolyxScript* self, PolyxTokenType type, const char* error_message) {
    if (match_token(self, type)) {
        return INFRAX_TRUE;
    }
    set_error(self, error_message);
    return INFRAX_FALSE;
}

// Forward declarations for parser functions
static PolyxAstNode* parse_expression(PolyxScript* self);
static PolyxAstNode* parse_statement(PolyxScript* self);

// Parser implementation
static PolyxAstNode* parse_primary_expression(PolyxScript* self) {
    switch (self->current_token.type) {
        case POLYX_TOKEN_NUMBER: {
            double value = g_core->atof(g_core, self->current_token.value);
            match_token(self, POLYX_TOKEN_NUMBER);
            return script_create_number_node(value);
        }
        
        case POLYX_TOKEN_STRING: {
            char* value = copy_string(self->current_token.value);
            match_token(self, POLYX_TOKEN_STRING);
            return script_create_string_node(value);
        }
        
        case POLYX_TOKEN_IDENTIFIER: {
            char* name = copy_string(self->current_token.value);
            match_token(self, POLYX_TOKEN_IDENTIFIER);
            return script_create_identifier_node(name);
        }
        
        case POLYX_TOKEN_PUNCTUATION:
            if (self->current_token.value[0] == '(') {
                match_token(self, POLYX_TOKEN_PUNCTUATION);
                PolyxAstNode* expr = parse_expression(self);
                if (!expr) return NULL;
                
                if (!expect_token(self, POLYX_TOKEN_PUNCTUATION, "Expected ')' after expression")) {
                    script_free_ast_node(expr);
                    return NULL;
                }
                return expr;
            }
            break;
            
        default:
            set_error(self, "Expected expression");
            return NULL;
    }
    
    set_error(self, "Expected expression");
    return NULL;
}

static int get_operator_precedence(char op) {
    switch (op) {
        case '=': return 1;
        case '|': return 2;
        case '&': return 3;
        case '<': case '>': case '!': return 4;
        case '+': case '-': return 5;
        case '*': case '/': case '%': return 6;
        default: return 0;
    }
}

static PolyxAstNode* parse_unary_expression(PolyxScript* self) {
    if (self->current_token.type == POLYX_TOKEN_OPERATOR) {
        char op = self->current_token.value[0];
        if (op == '-' || op == '!' || op == '~') {
            match_token(self, POLYX_TOKEN_OPERATOR);
            PolyxAstNode* operand = parse_unary_expression(self);
            if (!operand) return NULL;
            return script_create_unary_op_node(op, operand);
        }
    }
    
    return parse_primary_expression(self);
}

static PolyxAstNode* parse_binary_expression(PolyxScript* self, int min_precedence) {
    PolyxAstNode* left = parse_unary_expression(self);
    if (!left) return NULL;
    
    while (self->current_token.type == POLYX_TOKEN_OPERATOR) {
        char op = self->current_token.value[0];
        int precedence = get_operator_precedence(op);
        
        if (precedence <= min_precedence) break;
        
        match_token(self, POLYX_TOKEN_OPERATOR);
        
        PolyxAstNode* right = parse_binary_expression(self, precedence);
        if (!right) {
            script_free_ast_node(left);
            return NULL;
        }
        
        left = script_create_binary_op_node(op, left, right);
        if (!left) return NULL;
    }
    
    return left;
}

static PolyxAstNode* parse_expression(PolyxScript* self) {
    return parse_binary_expression(self, 0);
}

static PolyxAstNode* parse_block_statement(PolyxScript* self) {
    if (!match_token(self, POLYX_TOKEN_PUNCTUATION)) return NULL;
    
    PolyxAstNode* block = script_create_block_node();
    if (!block) return NULL;
    
    while (self->current_token.type != POLYX_TOKEN_PUNCTUATION ||
           self->current_token.value[0] != '}') {
        PolyxAstNode* statement = parse_statement(self);
        if (statement) {
            script_add_statement_to_block(block, statement);
        }
        
        if (self->had_error) {
            script_free_ast_node(block);
            return NULL;
        }
    }
    
    if (!expect_token(self, POLYX_TOKEN_PUNCTUATION, "Expected '}' after block")) {
        script_free_ast_node(block);
        return NULL;
    }
    
    return block;
}

static PolyxAstNode* parse_if_statement(PolyxScript* self) {
    if (!match_token(self, POLYX_TOKEN_KEYWORD)) return NULL;
    
    if (!expect_token(self, POLYX_TOKEN_PUNCTUATION, "Expected '(' after 'if'")) {
        return NULL;
    }
    
    PolyxAstNode* condition = parse_expression(self);
    if (!condition) return NULL;
    
    if (!expect_token(self, POLYX_TOKEN_PUNCTUATION, "Expected ')' after condition")) {
        script_free_ast_node(condition);
        return NULL;
    }
    
    PolyxAstNode* then_branch = parse_statement(self);
    if (!then_branch) {
        script_free_ast_node(condition);
        return NULL;
    }
    
    PolyxAstNode* else_branch = NULL;
    if (self->current_token.type == POLYX_TOKEN_KEYWORD &&
        g_core->strcmp(g_core, self->current_token.value, "else") == 0) {
        match_token(self, POLYX_TOKEN_KEYWORD);
        else_branch = parse_statement(self);
        if (!else_branch) {
            script_free_ast_node(condition);
            script_free_ast_node(then_branch);
            return NULL;
        }
    }
    
    return script_create_if_node(condition, then_branch, else_branch);
}

static PolyxAstNode* parse_while_statement(PolyxScript* self) {
    if (!match_token(self, POLYX_TOKEN_KEYWORD)) return NULL;
    
    if (!expect_token(self, POLYX_TOKEN_PUNCTUATION, "Expected '(' after 'while'")) {
        return NULL;
    }
    
    PolyxAstNode* condition = parse_expression(self);
    if (!condition) return NULL;
    
    if (!expect_token(self, POLYX_TOKEN_PUNCTUATION, "Expected ')' after condition")) {
        script_free_ast_node(condition);
        return NULL;
    }
    
    PolyxAstNode* body = parse_statement(self);
    if (!body) {
        script_free_ast_node(condition);
        return NULL;
    }
    
    return script_create_while_node(condition, body);
}

static PolyxAstNode* parse_let_statement(PolyxScript* self) {
    if (!match_token(self, POLYX_TOKEN_KEYWORD)) return NULL;
    
    if (!expect_token(self, POLYX_TOKEN_IDENTIFIER, "Expected identifier after 'let'")) {
        return NULL;
    }
    
    char* name = copy_string(self->current_token.value);
    if (!name) return NULL;
    
    if (!expect_token(self, POLYX_TOKEN_OPERATOR, "Expected '=' after identifier")) {
        g_memory->dealloc(g_memory, name);
        return NULL;
    }
    
    if (self->current_token.value[0] != '=') {
        set_error(self, "Expected '=' after identifier");
        g_memory->dealloc(g_memory, name);
        return NULL;
    }
    
    match_token(self, POLYX_TOKEN_OPERATOR);
    
    PolyxAstNode* value = parse_expression(self);
    if (!value) {
        g_memory->dealloc(g_memory, name);
        return NULL;
    }
    
    if (!expect_token(self, POLYX_TOKEN_PUNCTUATION, "Expected ';' after expression")) {
        g_memory->dealloc(g_memory, name);
        script_free_ast_node(value);
        return NULL;
    }
    
    return script_create_assignment_node(name, value);
}

static PolyxAstNode* parse_statement(PolyxScript* self) {
    if (!self) return NULL;
    
    switch (self->current_token.type) {
        case POLYX_TOKEN_KEYWORD:
            if (g_core->strcmp(g_core, self->current_token.value, "let") == 0) {
                return parse_let_statement(self);
            } else if (g_core->strcmp(g_core, self->current_token.value, "if") == 0) {
                return parse_if_statement(self);
            } else if (g_core->strcmp(g_core, self->current_token.value, "while") == 0) {
                return parse_while_statement(self);
            }
            break;
            
        case POLYX_TOKEN_PUNCTUATION:
            if (self->current_token.value[0] == '{') {
                return parse_block_statement(self);
            }
            break;
    }
    
    // Expression statement
    PolyxAstNode* expression = parse_expression(self);
    if (!expression) return NULL;
    
    if (!expect_token(self, POLYX_TOKEN_PUNCTUATION, "Expected ';' after expression")) {
        script_free_ast_node(expression);
        return NULL;
    }
    
    return expression;
}

static PolyxAstNode* script_parse_program(PolyxScript* self) {
    if (!self) return NULL;
    
    PolyxAstNode* program = script_create_block_node();
    if (!program) return NULL;
    
    while (self->current_token.type != POLYX_TOKEN_EOF) {
        PolyxAstNode* statement = parse_statement(self);
        if (statement) {
            script_add_statement_to_block(program, statement);
        }
        
        if (self->had_error) {
            script_free_ast_node(program);
            return NULL;
        }
    }
    
    return program;
}

// Debug: Print AST
static void print_ast_indent(int level) {
    for (int i = 0; i < level; i++) {
        g_core->printf(g_core, "  ");
    }
}

static void script_print_ast_node(PolyxScript* self, PolyxAstNode* node, int indent_level) {
    if (!self || !node) return;
    
    print_ast_indent(indent_level);
    
    switch (node->type) {
        case POLYX_AST_NUMBER:
            g_core->printf(g_core, "Number: %f\n", node->as.number_value);
            break;
            
        case POLYX_AST_STRING:
            g_core->printf(g_core, "String: \"%s\"\n", 
                node->as.string_value ? node->as.string_value : "");
            break;
            
        case POLYX_AST_IDENTIFIER:
            g_core->printf(g_core, "Identifier: %s\n", 
                node->as.identifier ? node->as.identifier : "");
            break;
            
        case POLYX_AST_BINARY_OP:
            g_core->printf(g_core, "Binary Op: %c\n", node->as.binary_op.operator);
            script_print_ast_node(self, node->as.binary_op.left, indent_level + 1);
            script_print_ast_node(self, node->as.binary_op.right, indent_level + 1);
            break;
            
        case POLYX_AST_UNARY_OP:
            g_core->printf(g_core, "Unary Op: %c\n", node->as.unary_op.operator);
            script_print_ast_node(self, node->as.unary_op.operand, indent_level + 1);
            break;
            
        case POLYX_AST_ASSIGNMENT:
            g_core->printf(g_core, "Assignment: %s\n", 
                node->as.assignment.name ? node->as.assignment.name : "");
            script_print_ast_node(self, node->as.assignment.value, indent_level + 1);
            break;
            
        case POLYX_AST_IF:
            g_core->printf(g_core, "If\n");
            print_ast_indent(indent_level + 1);
            g_core->printf(g_core, "Condition:\n");
            script_print_ast_node(self, node->as.if_stmt.condition, indent_level + 2);
            print_ast_indent(indent_level + 1);
            g_core->printf(g_core, "Then:\n");
            script_print_ast_node(self, node->as.if_stmt.then_branch, indent_level + 2);
            if (node->as.if_stmt.else_branch) {
                print_ast_indent(indent_level + 1);
                g_core->printf(g_core, "Else:\n");
                script_print_ast_node(self, node->as.if_stmt.else_branch, indent_level + 2);
            }
            break;
            
        case POLYX_AST_WHILE:
            g_core->printf(g_core, "While\n");
            print_ast_indent(indent_level + 1);
            g_core->printf(g_core, "Condition:\n");
            script_print_ast_node(self, node->as.while_stmt.condition, indent_level + 2);
            print_ast_indent(indent_level + 1);
            g_core->printf(g_core, "Body:\n");
            script_print_ast_node(self, node->as.while_stmt.body, indent_level + 2);
            break;
            
        case POLYX_AST_BLOCK:
            g_core->printf(g_core, "Block (%zu statements)\n", node->as.block.count);
            for (InfraxSize i = 0; i < node->as.block.count; i++) {
                script_print_ast_node(self, node->as.block.statements[i], indent_level + 1);
            }
            break;
            
        default:
            g_core->printf(g_core, "Unknown node type: %d\n", node->type);
            break;
    }
}

static void script_print_ast(PolyxScript* self, PolyxAstNode* node) {
    script_print_ast_node(self, node, 0);
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

// Global class instance
PolyxScriptClassType PolyxScriptClass = {
    .new = script_new,
    .free = script_free,
    .load_source = script_load_source,
    .run = script_run,
    .get_next_token = get_next_token,
    .parse_program = script_parse_program,
    .parse_statement = parse_statement,
    .parse_expression = parse_expression,
    .create_number_node = script_create_number_node,
    .create_string_node = script_create_string_node,
    .create_identifier_node = script_create_identifier_node,
    .create_binary_op_node = script_create_binary_op_node,
    .create_unary_op_node = script_create_unary_op_node,
    .create_assignment_node = script_create_assignment_node,
    .create_if_node = script_create_if_node,
    .create_while_node = script_create_while_node,
    .create_block_node = script_create_block_node,
    .add_statement_to_block = script_add_statement_to_block,
    .free_ast_node = script_free_ast_node,
    .print_tokens = script_print_tokens,
    .print_ast = script_print_ast
}; 
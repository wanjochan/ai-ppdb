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

// Value management
static PolyxValue* script_create_null_value(void) {
    PolyxValue* value = g_memory->alloc(g_memory, sizeof(PolyxValue));
    if (value) {
        g_core->memset(g_core, value, 0, sizeof(PolyxValue));
        value->type = POLYX_VALUE_NULL;
    }
    return value;
}

static PolyxValue* script_create_number_value(double number) {
    PolyxValue* value = g_memory->alloc(g_memory, sizeof(PolyxValue));
    if (value) {
        g_core->memset(g_core, value, 0, sizeof(PolyxValue));
        value->type = POLYX_VALUE_NUMBER;
        value->as.number = number;
    }
    return value;
}

static PolyxValue* script_create_string_value(const char* string) {
    PolyxValue* value = g_memory->alloc(g_memory, sizeof(PolyxValue));
    if (value) {
        g_core->memset(g_core, value, 0, sizeof(PolyxValue));
        value->type = POLYX_VALUE_STRING;
        value->as.string = copy_string(string);
    }
    return value;
}

static PolyxValue* script_create_boolean_value(InfraxBool boolean) {
    PolyxValue* value = g_memory->alloc(g_memory, sizeof(PolyxValue));
    if (value) {
        g_core->memset(g_core, value, 0, sizeof(PolyxValue));
        value->type = POLYX_VALUE_BOOLEAN;
        value->as.boolean = boolean;
    }
    return value;
}

static PolyxValue* script_create_function_value(PolyxAstNode* body, char** parameters, InfraxSize param_count, PolyxScope* closure) {
    PolyxValue* value = g_memory->alloc(g_memory, sizeof(PolyxValue));
    if (!value) return NULL;
    
    g_core->memset(g_core, value, 0, sizeof(PolyxValue));
    value->type = POLYX_VALUE_FUNCTION;
    value->as.function.body = body;
    value->as.function.closure = closure;
    
    if (param_count > 0) {
        value->as.function.parameters = g_memory->alloc(g_memory, param_count * sizeof(char*));
        if (!value->as.function.parameters) {
            g_memory->dealloc(g_memory, value);
            return NULL;
        }
        
        for (InfraxSize i = 0; i < param_count; i++) {
            value->as.function.parameters[i] = copy_string(parameters[i]);
            if (!value->as.function.parameters[i]) {
                for (InfraxSize j = 0; j < i; j++) {
                    g_memory->dealloc(g_memory, value->as.function.parameters[j]);
                }
                g_memory->dealloc(g_memory, value->as.function.parameters);
                g_memory->dealloc(g_memory, value);
                return NULL;
            }
        }
        value->as.function.param_count = param_count;
    }
    
    return value;
}

static PolyxValue* script_create_array_value(void) {
    PolyxValue* value = g_memory->alloc(g_memory, sizeof(PolyxValue));
    if (value) {
        g_core->memset(g_core, value, 0, sizeof(PolyxValue));
        value->type = POLYX_VALUE_ARRAY;
    }
    return value;
}

static PolyxValue* script_create_object_value(void) {
    PolyxValue* value = g_memory->alloc(g_memory, sizeof(PolyxValue));
    if (value) {
        g_core->memset(g_core, value, 0, sizeof(PolyxValue));
        value->type = POLYX_VALUE_OBJECT;
    }
    return value;
}

static void script_free_value(PolyxValue* value) {
    if (!value) return;
    
    switch (value->type) {
        case POLYX_VALUE_STRING:
            if (value->as.string) {
                g_memory->dealloc(g_memory, value->as.string);
            }
            break;
            
        case POLYX_VALUE_FUNCTION:
            if (value->as.function.parameters) {
                for (InfraxSize i = 0; i < value->as.function.param_count; i++) {
                    if (value->as.function.parameters[i]) {
                        g_memory->dealloc(g_memory, value->as.function.parameters[i]);
                    }
                }
                g_memory->dealloc(g_memory, value->as.function.parameters);
            }
            break;
            
        case POLYX_VALUE_ARRAY:
            if (value->as.array.elements) {
                for (InfraxSize i = 0; i < value->as.array.count; i++) {
                    script_free_value(value->as.array.elements[i]);
                }
                g_memory->dealloc(g_memory, value->as.array.elements);
            }
            break;
            
        case POLYX_VALUE_OBJECT:
            if (value->as.object.keys) {
                for (InfraxSize i = 0; i < value->as.object.count; i++) {
                    if (value->as.object.keys[i]) {
                        g_memory->dealloc(g_memory, value->as.object.keys[i]);
                    }
                    script_free_value(value->as.object.values[i]);
                }
                g_memory->dealloc(g_memory, value->as.object.keys);
                g_memory->dealloc(g_memory, value->as.object.values);
            }
            break;
            
        default:
            break;
    }
    
    g_memory->dealloc(g_memory, value);
}

// Scope management
static PolyxScope* script_create_scope(PolyxScope* parent) {
    PolyxScope* scope = g_memory->alloc(g_memory, sizeof(PolyxScope));
    if (scope) {
        g_core->memset(g_core, scope, 0, sizeof(PolyxScope));
        scope->parent = parent;
        scope->capacity = 8;  // Initial capacity
        
        scope->names = g_memory->alloc(g_memory, scope->capacity * sizeof(char*));
        scope->values = g_memory->alloc(g_memory, scope->capacity * sizeof(PolyxValue*));
        
        if (!scope->names || !scope->values) {
            if (scope->names) g_memory->dealloc(g_memory, scope->names);
            if (scope->values) g_memory->dealloc(g_memory, scope->values);
            g_memory->dealloc(g_memory, scope);
            return NULL;
        }
    }
    return scope;
}

static void script_free_scope(PolyxScope* scope) {
    if (!scope) return;
    
    if (scope->names) {
        for (InfraxSize i = 0; i < scope->count; i++) {
            if (scope->names[i]) {
                g_memory->dealloc(g_memory, scope->names[i]);
            }
            if (scope->values[i]) {
                script_free_value(scope->values[i]);
            }
        }
        g_memory->dealloc(g_memory, scope->names);
        g_memory->dealloc(g_memory, scope->values);
    }
    
    g_memory->dealloc(g_memory, scope);
}

static InfraxError script_define_variable(PolyxScope* scope, const char* name, PolyxValue* value) {
    if (!scope || !name || !value) {
        return make_error(-1, "Invalid arguments to define_variable");
    }
    
    // Check if we need to resize
    if (scope->count >= scope->capacity) {
        InfraxSize new_capacity = scope->capacity * 2;
        char** new_names = g_memory->alloc(g_memory, new_capacity * sizeof(char*));
        PolyxValue** new_values = g_memory->alloc(g_memory, new_capacity * sizeof(PolyxValue*));
        
        if (!new_names || !new_values) {
            if (new_names) g_memory->dealloc(g_memory, new_names);
            if (new_values) g_memory->dealloc(g_memory, new_values);
            return make_error(-1, "Memory allocation failed");
        }
        
        g_core->memcpy(g_core, new_names, scope->names, scope->count * sizeof(char*));
        g_core->memcpy(g_core, new_values, scope->values, scope->count * sizeof(PolyxValue*));
        
        g_memory->dealloc(g_memory, scope->names);
        g_memory->dealloc(g_memory, scope->values);
        
        scope->names = new_names;
        scope->values = new_values;
        scope->capacity = new_capacity;
    }
    
    // Add the new variable
    scope->names[scope->count] = copy_string(name);
    scope->values[scope->count] = value;
    scope->count++;
    
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError script_set_variable(PolyxScope* scope, const char* name, PolyxValue* value) {
    if (!scope || !name || !value) {
        return make_error(-1, "Invalid arguments to set_variable");
    }
    
    // Search in current scope
    for (InfraxSize i = 0; i < scope->count; i++) {
        if (g_core->strcmp(g_core, scope->names[i], name) == 0) {
            script_free_value(scope->values[i]);
            scope->values[i] = value;
            return INFRAX_ERROR_OK_STRUCT;
        }
    }
    
    // If not found and we have a parent scope, try there
    if (scope->parent) {
        return script_set_variable(scope->parent, name, value);
    }
    
    return make_error(-1, "Variable not found");
}

static PolyxValue* script_get_variable(PolyxScope* scope, const char* name) {
    if (!scope || !name) return NULL;
    
    // Search in current scope
    for (InfraxSize i = 0; i < scope->count; i++) {
        if (g_core->strcmp(g_core, scope->names[i], name) == 0) {
            return scope->values[i];
        }
    }
    
    // If not found and we have a parent scope, try there
    if (scope->parent) {
        return script_get_variable(scope->parent, name);
    }
    
    return NULL;
}

// Debug: Print value
static void script_print_value(PolyxScript* self, PolyxValue* value) {
    if (!self || !value) return;
    
    switch (value->type) {
        case POLYX_VALUE_NULL:
            g_core->printf(g_core, "null");
            break;
            
        case POLYX_VALUE_NUMBER:
            g_core->printf(g_core, "%f", value->as.number);
            break;
            
        case POLYX_VALUE_STRING:
            g_core->printf(g_core, "\"%s\"", value->as.string ? value->as.string : "");
            break;
            
        case POLYX_VALUE_BOOLEAN:
            g_core->printf(g_core, "%s", value->as.boolean ? "true" : "false");
            break;
            
        case POLYX_VALUE_FUNCTION:
            g_core->printf(g_core, "<function>");
            break;
            
        case POLYX_VALUE_ARRAY:
            g_core->printf(g_core, "[");
            for (InfraxSize i = 0; i < value->as.array.count; i++) {
                if (i > 0) g_core->printf(g_core, ", ");
                script_print_value(self, value->as.array.elements[i]);
            }
            g_core->printf(g_core, "]");
            break;
            
        case POLYX_VALUE_OBJECT:
            g_core->printf(g_core, "{");
            for (InfraxSize i = 0; i < value->as.object.count; i++) {
                if (i > 0) g_core->printf(g_core, ", ");
                g_core->printf(g_core, "\"%s\": ", value->as.object.keys[i]);
                script_print_value(self, value->as.object.values[i]);
            }
            g_core->printf(g_core, "}");
            break;
    }
}

// Expression evaluation
static PolyxValue* script_eval_expression(PolyxScript* self, PolyxAstNode* node) {
    if (!self || !node) return NULL;
    
    switch (node->type) {
        case POLYX_AST_NUMBER: {
            return script_create_number_value(node->as.number_value);
        }
        
        case POLYX_AST_STRING: {
            return script_create_string_value(node->as.string_value);
        }
        
        case POLYX_AST_IDENTIFIER: {
            PolyxValue* value = script_get_variable(self->current_scope, node->as.identifier);
            if (!value) {
                self->had_error = INFRAX_TRUE;
                if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
                self->error_message = copy_string("Undefined variable");
                return NULL;
            }
            return value;
        }
        
        case POLYX_AST_BINARY_OP: {
            PolyxValue* left = script_eval_expression(self, node->as.binary_op.left);
            if (!left) return NULL;
            
            PolyxValue* right = script_eval_expression(self, node->as.binary_op.right);
            if (!right) {
                script_free_value(left);
                return NULL;
            }
            
            PolyxValue* result = NULL;
            
            // Both operands must be numbers for arithmetic operations
            if (left->type == POLYX_VALUE_NUMBER && right->type == POLYX_VALUE_NUMBER) {
                double left_num = left->as.number;
                double right_num = right->as.number;
                
                switch (node->as.binary_op.operator) {
                    case '+':
                        result = script_create_number_value(left_num + right_num);
                        break;
                    case '-':
                        result = script_create_number_value(left_num - right_num);
                        break;
                    case '*':
                        result = script_create_number_value(left_num * right_num);
                        break;
                    case '/':
                        if (right_num == 0) {
                            self->had_error = INFRAX_TRUE;
                            if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
                            self->error_message = copy_string("Division by zero");
                        } else {
                            result = script_create_number_value(left_num / right_num);
                        }
                        break;
                    case '<':
                        result = script_create_boolean_value(left_num < right_num);
                        break;
                    case '>':
                        result = script_create_boolean_value(left_num > right_num);
                        break;
                    case TOKEN_EQ:
                        result = script_create_boolean_value(left_num == right_num);
                        break;
                    case TOKEN_NEQ:
                        result = script_create_boolean_value(left_num != right_num);
                        break;
                    case TOKEN_LEQ:
                        result = script_create_boolean_value(left_num <= right_num);
                        break;
                    case TOKEN_GEQ:
                        result = script_create_boolean_value(left_num >= right_num);
                        break;
                }
            }
            // String concatenation
            else if (left->type == POLYX_VALUE_STRING && right->type == POLYX_VALUE_STRING && node->as.binary_op.operator == '+') {
                InfraxSize left_len = g_core->strlen(g_core, left->as.string);
                InfraxSize right_len = g_core->strlen(g_core, right->as.string);
                InfraxSize total_len = left_len + right_len;
                
                char* concat = g_memory->alloc(g_memory, total_len + 1);
                if (concat) {
                    g_core->strcpy(g_core, concat, left->as.string);
                    g_core->strcat(g_core, concat, right->as.string);
                    result = script_create_string_value(concat);
                    g_memory->dealloc(g_memory, concat);
                }
            }
            // Boolean operations
            else if (left->type == POLYX_VALUE_BOOLEAN && right->type == POLYX_VALUE_BOOLEAN) {
                switch (node->as.binary_op.operator) {
                    case TOKEN_AND:
                        result = script_create_boolean_value(left->as.boolean && right->as.boolean);
                        break;
                    case TOKEN_OR:
                        result = script_create_boolean_value(left->as.boolean || right->as.boolean);
                        break;
                    case TOKEN_EQ:
                        result = script_create_boolean_value(left->as.boolean == right->as.boolean);
                        break;
                    case TOKEN_NEQ:
                        result = script_create_boolean_value(left->as.boolean != right->as.boolean);
                        break;
                }
            }
            
            script_free_value(left);
            script_free_value(right);
            
            if (!result) {
                self->had_error = INFRAX_TRUE;
                if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
                self->error_message = copy_string("Invalid operands for binary operation");
            }
            
            return result;
        }
        
        case POLYX_AST_UNARY_OP: {
            PolyxValue* operand = script_eval_expression(self, node->as.unary_op.operand);
            if (!operand) return NULL;
            
            PolyxValue* result = NULL;
            
            switch (node->as.unary_op.operator) {
                case '-':
                    if (operand->type == POLYX_VALUE_NUMBER) {
                        result = script_create_number_value(-operand->as.number);
                    }
                    break;
                    
                case '!':
                    if (operand->type == POLYX_VALUE_BOOLEAN) {
                        result = script_create_boolean_value(!operand->as.boolean);
                    }
                    break;
            }
            
            script_free_value(operand);
            
            if (!result) {
                self->had_error = INFRAX_TRUE;
                if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
                self->error_message = copy_string("Invalid operand for unary operation");
            }
            
            return result;
        }
        
        case POLYX_AST_FUNCTION_CALL: {
            // Get the function value
            PolyxValue* func = script_eval_expression(self, node->as.function_call.callee);
            if (!func) return NULL;
            
            if (func->type != POLYX_VALUE_FUNCTION) {
                script_free_value(func);
                self->had_error = INFRAX_TRUE;
                if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
                self->error_message = copy_string("Attempting to call a non-function value");
                return NULL;
            }
            
            // Evaluate arguments
            PolyxValue** args = NULL;
            InfraxSize arg_count = node->as.function_call.arg_count;
            
            if (arg_count > 0) {
                args = g_memory->alloc(g_memory, arg_count * sizeof(PolyxValue*));
                if (!args) {
                    script_free_value(func);
                    self->had_error = INFRAX_TRUE;
                    if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
                    self->error_message = copy_string("Memory allocation failed");
                    return NULL;
                }
                
                for (InfraxSize i = 0; i < arg_count; i++) {
                    args[i] = script_eval_expression(self, node->as.function_call.arguments[i]);
                    if (!args[i]) {
                        for (InfraxSize j = 0; j < i; j++) {
                            script_free_value(args[j]);
                        }
                        g_memory->dealloc(g_memory, args);
                        script_free_value(func);
                        return NULL;
                    }
                }
            }
            
            // Create a new scope for the function
            PolyxScope* func_scope = script_create_scope(func->as.function.closure);
            if (!func_scope) {
                if (args) {
                    for (InfraxSize i = 0; i < arg_count; i++) {
                        script_free_value(args[i]);
                    }
                    g_memory->dealloc(g_memory, args);
                }
                script_free_value(func);
                self->had_error = INFRAX_TRUE;
                if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
                self->error_message = copy_string("Memory allocation failed");
                return NULL;
            }
            
            // Bind arguments to parameters
            for (InfraxSize i = 0; i < arg_count && i < func->as.function.param_count; i++) {
                script_define_variable(func_scope, func->as.function.parameters[i], args[i]);
            }
            
            // Switch to function scope
            PolyxScope* prev_scope = self->current_scope;
            self->current_scope = func_scope;
            
            // Execute function body
            PolyxValue* result = script_eval_expression(self, func->as.function.body);
            
            // Restore previous scope
            self->current_scope = prev_scope;
            
            // Clean up
            script_free_scope(func_scope);
            if (args) {
                g_memory->dealloc(g_memory, args);
            }
            script_free_value(func);
            
            return result;
        }
        
        default:
            self->had_error = INFRAX_TRUE;
            if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
            self->error_message = copy_string("Unknown expression type");
            return NULL;
    }
}

// Statement execution
static InfraxError script_execute_statement(PolyxScript* self, PolyxAstNode* node) {
    if (!self || !node) return make_error(-1, "Invalid arguments to execute_statement");
    
    switch (node->type) {
        case POLYX_AST_LET: {
            // Evaluate the initializer if present
            PolyxValue* value = NULL;
            if (node->as.let.initializer) {
                value = script_eval_expression(self, node->as.let.initializer);
                if (!value) {
                    return make_error(-1, "Failed to evaluate initializer");
                }
            } else {
                value = script_create_null_value();
                if (!value) {
                    return make_error(-1, "Memory allocation failed");
                }
            }
            
            // Define the variable in the current scope
            InfraxError error = script_define_variable(self->current_scope, node->as.let.name, value);
            if (error.code != 0) {
                script_free_value(value);
                return error;
            }
            
            return INFRAX_ERROR_OK_STRUCT;
        }
        
        case POLYX_AST_ASSIGNMENT: {
            // Evaluate the value
            PolyxValue* value = script_eval_expression(self, node->as.assignment.value);
            if (!value) {
                return make_error(-1, "Failed to evaluate assignment value");
            }
            
            // Set the variable value
            InfraxError error = script_set_variable(self->current_scope, node->as.assignment.name, value);
            if (error.code != 0) {
                script_free_value(value);
                return error;
            }
            
            return INFRAX_ERROR_OK_STRUCT;
        }
        
        case POLYX_AST_IF: {
            // Evaluate the condition
            PolyxValue* condition = script_eval_expression(self, node->as.if_stmt.condition);
            if (!condition) {
                return make_error(-1, "Failed to evaluate if condition");
            }
            
            InfraxError error = INFRAX_ERROR_OK_STRUCT;
            
            if (condition->type != POLYX_VALUE_BOOLEAN) {
                script_free_value(condition);
                return make_error(-1, "If condition must be a boolean");
            }
            
            if (condition->as.boolean) {
                error = script_execute_statement(self, node->as.if_stmt.then_branch);
            } else if (node->as.if_stmt.else_branch) {
                error = script_execute_statement(self, node->as.if_stmt.else_branch);
            }
            
            script_free_value(condition);
            return error;
        }
        
        case POLYX_AST_WHILE: {
            InfraxError error = INFRAX_ERROR_OK_STRUCT;
            
            while (1) {
                // Evaluate the condition
                PolyxValue* condition = script_eval_expression(self, node->as.while_stmt.condition);
                if (!condition) {
                    return make_error(-1, "Failed to evaluate while condition");
                }
                
                if (condition->type != POLYX_VALUE_BOOLEAN) {
                    script_free_value(condition);
                    return make_error(-1, "While condition must be a boolean");
                }
                
                if (!condition->as.boolean) {
                    script_free_value(condition);
                    break;
                }
                
                script_free_value(condition);
                
                // Execute the body
                error = script_execute_statement(self, node->as.while_stmt.body);
                if (error.code != 0) {
                    return error;
                }
            }
            
            return error;
        }
        
        case POLYX_AST_BLOCK: {
            // Create a new scope for the block
            PolyxScope* block_scope = script_create_scope(self->current_scope);
            if (!block_scope) {
                return make_error(-1, "Memory allocation failed");
            }
            
            // Switch to block scope
            PolyxScope* prev_scope = self->current_scope;
            self->current_scope = block_scope;
            
            // Execute all statements in the block
            InfraxError error = INFRAX_ERROR_OK_STRUCT;
            for (InfraxSize i = 0; i < node->as.block.count; i++) {
                error = script_execute_statement(self, node->as.block.statements[i]);
                if (error.code != 0) {
                    break;
                }
            }
            
            // Restore previous scope
            self->current_scope = prev_scope;
            
            // Clean up block scope
            script_free_scope(block_scope);
            
            return error;
        }
        
        case POLYX_AST_EXPRESSION: {
            // Evaluate the expression and store the result
            if (self->last_result) {
                script_free_value(self->last_result);
            }
            self->last_result = script_eval_expression(self, node->as.expression);
            if (!self->last_result) {
                return make_error(-1, "Failed to evaluate expression");
            }
            
            return INFRAX_ERROR_OK_STRUCT;
        }
        
        default:
            return make_error(-1, "Unknown statement type");
    }
}

// Run function implementation
static InfraxError script_run(PolyxScript* self) {
    if (!self) return make_error(-1, "Invalid script instance");
    
    // Reset error state
    self->had_error = INFRAX_FALSE;
    if (self->error_message) {
        g_memory->dealloc(g_memory, self->error_message);
    }
    
    // Parse the program
    PolyxAstNode* ast = script_parse_program(self);
    if (!ast) {
        return make_error(-1, "Failed to parse program");
    }
    
    // Execute the program
    InfraxError error = script_execute_statement(self, ast);
    
    // Clean up
    script_free_ast_node(ast);
    
    return error;
}

// Built-in function implementations
static PolyxValue* builtin_print(PolyxScript* self, PolyxValue** args, InfraxSize arg_count) {
    if (arg_count < 1) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("print() requires at least one argument");
        return NULL;
    }
    
    for (InfraxSize i = 0; i < arg_count; i++) {
        if (i > 0) g_core->printf(g_core, " ");
        script_print_value(self, args[i]);
    }
    g_core->printf(g_core, "\n");
    
    return script_create_null_value();
}

static PolyxValue* builtin_to_string(PolyxScript* self, PolyxValue** args, InfraxSize arg_count) {
    if (arg_count != 1) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("toString() requires exactly one argument");
        return NULL;
    }
    
    PolyxValue* arg = args[0];
    char buffer[64];  // For number conversion
    
    switch (arg->type) {
        case POLYX_VALUE_NULL:
            return script_create_string_value("null");
            
        case POLYX_VALUE_NUMBER:
            g_core->snprintf(g_core, buffer, sizeof(buffer), "%g", arg->as.number);
            return script_create_string_value(buffer);
            
        case POLYX_VALUE_STRING:
            return script_create_string_value(arg->as.string);
            
        case POLYX_VALUE_BOOLEAN:
            return script_create_string_value(arg->as.boolean ? "true" : "false");
            
        case POLYX_VALUE_FUNCTION:
            return script_create_string_value("<function>");
            
        case POLYX_VALUE_ARRAY:
            return script_create_string_value("<array>");
            
        case POLYX_VALUE_OBJECT:
            return script_create_string_value("<object>");
            
        default:
            return script_create_string_value("<unknown>");
    }
}

static PolyxValue* builtin_to_number(PolyxScript* self, PolyxValue** args, InfraxSize arg_count) {
    if (arg_count != 1) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("toNumber() requires exactly one argument");
        return NULL;
    }
    
    PolyxValue* arg = args[0];
    
    switch (arg->type) {
        case POLYX_VALUE_NUMBER:
            return script_create_number_value(arg->as.number);
            
        case POLYX_VALUE_STRING: {
            char* end;
            double number = g_core->strtod(g_core, arg->as.string, &end);
            if (*end != '\0') {
                self->had_error = INFRAX_TRUE;
                if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
                self->error_message = copy_string("Invalid number format");
                return NULL;
            }
            return script_create_number_value(number);
        }
            
        case POLYX_VALUE_BOOLEAN:
            return script_create_number_value(arg->as.boolean ? 1.0 : 0.0);
            
        default:
            self->had_error = INFRAX_TRUE;
            if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
            self->error_message = copy_string("Cannot convert value to number");
            return NULL;
    }
}

static PolyxValue* builtin_array_push(PolyxScript* self, PolyxValue** args, InfraxSize arg_count) {
    if (arg_count < 2) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("push() requires at least two arguments");
        return NULL;
    }
    
    PolyxValue* array = args[0];
    if (array->type != POLYX_VALUE_ARRAY) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("First argument must be an array");
        return NULL;
    }
    
    // Resize array if needed
    if (array->as.array.count + arg_count - 1 > array->as.array.capacity) {
        InfraxSize new_capacity = array->as.array.capacity * 2;
        if (new_capacity < array->as.array.count + arg_count - 1) {
            new_capacity = array->as.array.count + arg_count - 1;
        }
        
        PolyxValue** new_elements = g_memory->alloc(g_memory, new_capacity * sizeof(PolyxValue*));
        if (!new_elements) {
            self->had_error = INFRAX_TRUE;
            if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
            self->error_message = copy_string("Memory allocation failed");
            return NULL;
        }
        
        g_core->memcpy(g_core, new_elements, array->as.array.elements, array->as.array.count * sizeof(PolyxValue*));
        g_memory->dealloc(g_memory, array->as.array.elements);
        array->as.array.elements = new_elements;
        array->as.array.capacity = new_capacity;
    }
    
    // Add new elements
    for (InfraxSize i = 1; i < arg_count; i++) {
        array->as.array.elements[array->as.array.count++] = args[i];
    }
    
    return script_create_number_value(array->as.array.count);
}

static PolyxValue* builtin_array_pop(PolyxScript* self, PolyxValue** args, InfraxSize arg_count) {
    if (arg_count != 1) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("pop() requires exactly one argument");
        return NULL;
    }
    
    PolyxValue* array = args[0];
    if (array->type != POLYX_VALUE_ARRAY) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("Argument must be an array");
        return NULL;
    }
    
    if (array->as.array.count == 0) {
        return script_create_null_value();
    }
    
    return array->as.array.elements[--array->as.array.count];
}

static PolyxValue* builtin_array_length(PolyxScript* self, PolyxValue** args, InfraxSize arg_count) {
    if (arg_count != 1) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("length() requires exactly one argument");
        return NULL;
    }
    
    PolyxValue* array = args[0];
    if (array->type != POLYX_VALUE_ARRAY) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("Argument must be an array");
        return NULL;
    }
    
    return script_create_number_value(array->as.array.count);
}

static PolyxValue* builtin_object_set(PolyxScript* self, PolyxValue** args, InfraxSize arg_count) {
    if (arg_count != 3) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("set() requires exactly three arguments");
        return NULL;
    }
    
    PolyxValue* object = args[0];
    if (object->type != POLYX_VALUE_OBJECT) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("First argument must be an object");
        return NULL;
    }
    
    PolyxValue* key = args[1];
    if (key->type != POLYX_VALUE_STRING) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("Second argument must be a string");
        return NULL;
    }
    
    // Check if key already exists
    for (InfraxSize i = 0; i < object->as.object.count; i++) {
        if (g_core->strcmp(g_core, object->as.object.keys[i], key->as.string) == 0) {
            script_free_value(object->as.object.values[i]);
            object->as.object.values[i] = args[2];
            return script_create_null_value();
        }
    }
    
    // Resize object if needed
    if (object->as.object.count >= object->as.object.capacity) {
        InfraxSize new_capacity = object->as.object.capacity * 2;
        if (new_capacity == 0) new_capacity = 8;
        
        char** new_keys = g_memory->alloc(g_memory, new_capacity * sizeof(char*));
        PolyxValue** new_values = g_memory->alloc(g_memory, new_capacity * sizeof(PolyxValue*));
        
        if (!new_keys || !new_values) {
            if (new_keys) g_memory->dealloc(g_memory, new_keys);
            if (new_values) g_memory->dealloc(g_memory, new_values);
            self->had_error = INFRAX_TRUE;
            if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
            self->error_message = copy_string("Memory allocation failed");
            return NULL;
        }
        
        g_core->memcpy(g_core, new_keys, object->as.object.keys, object->as.object.count * sizeof(char*));
        g_core->memcpy(g_core, new_values, object->as.object.values, object->as.object.count * sizeof(PolyxValue*));
        
        g_memory->dealloc(g_memory, object->as.object.keys);
        g_memory->dealloc(g_memory, object->as.object.values);
        
        object->as.object.keys = new_keys;
        object->as.object.values = new_values;
        object->as.object.capacity = new_capacity;
    }
    
    // Add new key-value pair
    object->as.object.keys[object->as.object.count] = copy_string(key->as.string);
    object->as.object.values[object->as.object.count] = args[2];
    object->as.object.count++;
    
    return script_create_null_value();
}

static PolyxValue* builtin_object_get(PolyxScript* self, PolyxValue** args, InfraxSize arg_count) {
    if (arg_count != 2) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("get() requires exactly two arguments");
        return NULL;
    }
    
    PolyxValue* object = args[0];
    if (object->type != POLYX_VALUE_OBJECT) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("First argument must be an object");
        return NULL;
    }
    
    PolyxValue* key = args[1];
    if (key->type != POLYX_VALUE_STRING) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("Second argument must be a string");
        return NULL;
    }
    
    for (InfraxSize i = 0; i < object->as.object.count; i++) {
        if (g_core->strcmp(g_core, object->as.object.keys[i], key->as.string) == 0) {
            return object->as.object.values[i];
        }
    }
    
    return script_create_null_value();
}

// Initialize built-in functions
static void init_builtins(PolyxScript* self) {
    // Create built-in function values
    PolyxValue* print_func = script_create_function_value(NULL, NULL, 0, NULL);
    print_func->as.function.native_fn = builtin_print;
    
    PolyxValue* to_string_func = script_create_function_value(NULL, NULL, 0, NULL);
    to_string_func->as.function.native_fn = builtin_to_string;
    
    PolyxValue* to_number_func = script_create_function_value(NULL, NULL, 0, NULL);
    to_number_func->as.function.native_fn = builtin_to_number;
    
    PolyxValue* array_push_func = script_create_function_value(NULL, NULL, 0, NULL);
    array_push_func->as.function.native_fn = builtin_array_push;
    
    PolyxValue* array_pop_func = script_create_function_value(NULL, NULL, 0, NULL);
    array_pop_func->as.function.native_fn = builtin_array_pop;
    
    PolyxValue* array_length_func = script_create_function_value(NULL, NULL, 0, NULL);
    array_length_func->as.function.native_fn = builtin_array_length;
    
    PolyxValue* object_set_func = script_create_function_value(NULL, NULL, 0, NULL);
    object_set_func->as.function.native_fn = builtin_object_set;
    
    PolyxValue* object_get_func = script_create_function_value(NULL, NULL, 0, NULL);
    object_get_func->as.function.native_fn = builtin_object_get;
    
    // Define built-in functions in global scope
    script_define_variable(self->global_scope, "print", print_func);
    script_define_variable(self->global_scope, "toString", to_string_func);
    script_define_variable(self->global_scope, "toNumber", to_number_func);
    script_define_variable(self->global_scope, "arrayPush", array_push_func);
    script_define_variable(self->global_scope, "arrayPop", array_pop_func);
    script_define_variable(self->global_scope, "arrayLength", array_length_func);
    script_define_variable(self->global_scope, "objectSet", object_set_func);
    script_define_variable(self->global_scope, "objectGet", object_get_func);
}

// Async operation management
static PolyxValue* script_create_promise(PolyxScript* self) {
    PolyxValue* promise = g_memory->alloc(g_memory, sizeof(PolyxValue));
    if (!promise) return NULL;
    
    g_core->memset(g_core, promise, 0, sizeof(PolyxValue));
    promise->type = POLYX_VALUE_PROMISE;
    promise->as.promise.state = POLYX_ASYNC_PENDING;
    
    return promise;
}

static void script_resolve_promise(PolyxScript* self, PolyxValue* promise, PolyxValue* value) {
    if (!self || !promise || promise->type != POLYX_VALUE_PROMISE) return;
    
    promise->as.promise.state = POLYX_ASYNC_COMPLETED;
    promise->as.promise.result = value;
    
    // Execute then handler if exists
    if (promise->as.promise.then_handler) {
        PolyxValue* args[] = { value };
        script_eval_expression(self, promise->as.promise.then_handler);
    }
}

static void script_reject_promise(PolyxScript* self, PolyxValue* promise, const char* error) {
    if (!self || !promise || promise->type != POLYX_VALUE_PROMISE) return;
    
    promise->as.promise.state = POLYX_ASYNC_ERROR;
    if (error) {
        PolyxValue* error_value = script_create_string_value(error);
        promise->as.promise.result = error_value;
    }
    
    // Execute catch handler if exists
    if (promise->as.promise.catch_handler) {
        PolyxValue* args[] = { promise->as.promise.result };
        script_eval_expression(self, promise->as.promise.catch_handler);
    }
}

static void script_update_async(PolyxScript* self) {
    if (!self || !self->async_operations) return;
    
    for (InfraxSize i = 0; i < self->async_count; i++) {
        PolyxAsyncContext* ctx = self->async_operations[i];
        if (!ctx) continue;
        
        if (ctx->state == POLYX_ASYNC_COMPLETED) {
            // Execute callback
            if (ctx->callback) {
                PolyxAsyncResult result = {
                    .state = POLYX_ASYNC_COMPLETED,
                    .result = ctx->promise->as.promise.result,
                    .error_message = NULL
                };
                ctx->callback(self, &result);
            }
            
            // Clean up
            g_memory->dealloc(g_memory, ctx);
            self->async_operations[i] = NULL;
        }
        else if (ctx->state == POLYX_ASYNC_ERROR) {
            // Execute callback
            if (ctx->callback) {
                PolyxAsyncResult result = {
                    .state = POLYX_ASYNC_ERROR,
                    .result = NULL,
                    .error_message = ctx->error_message
                };
                ctx->callback(self, &result);
            }
            
            // Clean up
            if (ctx->error_message) {
                g_memory->dealloc(g_memory, ctx->error_message);
            }
            g_memory->dealloc(g_memory, ctx);
            self->async_operations[i] = NULL;
        }
    }
}

// Built-in async functions
static void sleep_callback(PolyxScript* script, void* user_data) {
    PolyxAsyncContext* ctx = (PolyxAsyncContext*)user_data;
    if (!ctx) return;
    
    ctx->state = POLYX_ASYNC_COMPLETED;
    script_resolve_promise(script, ctx->promise, script_create_null_value());
}

static PolyxValue* script_async_sleep(PolyxScript* self, PolyxValue** args, InfraxSize arg_count) {
    if (arg_count != 1) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("sleep() requires exactly one argument");
        return NULL;
    }
    
    if (args[0]->type != POLYX_VALUE_NUMBER) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("sleep() argument must be a number");
        return NULL;
    }
    
    // Create promise
    PolyxValue* promise = script_create_promise(self);
    if (!promise) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("Memory allocation failed");
        return NULL;
    }
    
    // Create async context
    PolyxAsyncContext* ctx = g_memory->alloc(g_memory, sizeof(PolyxAsyncContext));
    if (!ctx) {
        script_free_value(promise);
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("Memory allocation failed");
        return NULL;
    }
    
    g_core->memset(g_core, ctx, 0, sizeof(PolyxAsyncContext));
    ctx->state = POLYX_ASYNC_PENDING;
    ctx->promise = promise;
    ctx->callback = sleep_callback;
    
    // Add to async operations
    if (self->async_count >= self->async_capacity) {
        InfraxSize new_capacity = self->async_capacity * 2;
        if (new_capacity == 0) new_capacity = 8;
        
        PolyxAsyncContext** new_operations = g_memory->alloc(g_memory, new_capacity * sizeof(PolyxAsyncContext*));
        if (!new_operations) {
            g_memory->dealloc(g_memory, ctx);
            script_free_value(promise);
            self->had_error = INFRAX_TRUE;
            if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
            self->error_message = copy_string("Memory allocation failed");
            return NULL;
        }
        
        if (self->async_operations) {
            g_core->memcpy(g_core, new_operations, self->async_operations, self->async_count * sizeof(PolyxAsyncContext*));
            g_memory->dealloc(g_memory, self->async_operations);
        }
        
        self->async_operations = new_operations;
        self->async_capacity = new_capacity;
    }
    
    self->async_operations[self->async_count++] = ctx;
    promise->as.promise.context = ctx;
    
    return promise;
}

static void file_read_callback(PolyxScript* script, void* user_data) {
    PolyxAsyncContext* ctx = (PolyxAsyncContext*)user_data;
    if (!ctx) return;
    
    // TODO: Implement actual file reading
    ctx->state = POLYX_ASYNC_COMPLETED;
    script_resolve_promise(script, ctx->promise, script_create_string_value("File content"));
}

static PolyxValue* script_async_read_file(PolyxScript* self, PolyxValue** args, InfraxSize arg_count) {
    if (arg_count != 1) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("readFile() requires exactly one argument");
        return NULL;
    }
    
    if (args[0]->type != POLYX_VALUE_STRING) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("readFile() argument must be a string");
        return NULL;
    }
    
    // Create promise
    PolyxValue* promise = script_create_promise(self);
    if (!promise) {
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("Memory allocation failed");
        return NULL;
    }
    
    // Create async context
    PolyxAsyncContext* ctx = g_memory->alloc(g_memory, sizeof(PolyxAsyncContext));
    if (!ctx) {
        script_free_value(promise);
        self->had_error = INFRAX_TRUE;
        if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
        self->error_message = copy_string("Memory allocation failed");
        return NULL;
    }
    
    g_core->memset(g_core, ctx, 0, sizeof(PolyxAsyncContext));
    ctx->state = POLYX_ASYNC_PENDING;
    ctx->promise = promise;
    ctx->callback = file_read_callback;
    
    // Add to async operations
    if (self->async_count >= self->async_capacity) {
        InfraxSize new_capacity = self->async_capacity * 2;
        if (new_capacity == 0) new_capacity = 8;
        
        PolyxAsyncContext** new_operations = g_memory->alloc(g_memory, new_capacity * sizeof(PolyxAsyncContext*));
        if (!new_operations) {
            g_memory->dealloc(g_memory, ctx);
            script_free_value(promise);
            self->had_error = INFRAX_TRUE;
            if (self->error_message) g_memory->dealloc(g_memory, self->error_message);
            self->error_message = copy_string("Memory allocation failed");
            return NULL;
        }
        
        if (self->async_operations) {
            g_core->memcpy(g_core, new_operations, self->async_operations, self->async_count * sizeof(PolyxAsyncContext*));
            g_memory->dealloc(g_memory, self->async_operations);
        }
        
        self->async_operations = new_operations;
        self->async_capacity = new_capacity;
    }
    
    self->async_operations[self->async_count++] = ctx;
    promise->as.promise.context = ctx;
    
    return promise;
}

// Constructor
static PolyxScript* script_new(void) {
    if (!init_globals()) return NULL;
    
    PolyxScript* script = g_memory->alloc(g_memory, sizeof(PolyxScript));
    if (!script) return NULL;
    
    g_core->memset(g_core, script, 0, sizeof(PolyxScript));
    script->self = script;
    script->klass = &PolyxScriptClass;
    
    // Initialize interpreter state
    script->global_scope = script_create_scope(NULL);
    if (!script->global_scope) {
        g_memory->dealloc(g_memory, script);
        return NULL;
    }
    script->current_scope = script->global_scope;
    
    // Initialize async state
    script->async_count = 0;
    script->async_capacity = 8;
    script->async_operations = g_memory->alloc(g_memory, script->async_capacity * sizeof(PolyxAsyncContext*));
    if (!script->async_operations) {
        script_free_scope(script->global_scope);
        g_memory->dealloc(g_memory, script);
        return NULL;
    }
    
    // Initialize built-in functions
    init_builtins(script);
    
    return script;
}

// Destructor
static void script_free(PolyxScript* self) {
    if (!self) return;
    
    // Free current token
    free_token(&self->current_token);
    
    // Free error message
    if (self->error_message) {
        g_memory->dealloc(g_memory, self->error_message);
    }
    
    // Free scopes
    if (self->global_scope) {
        script_free_scope(self->global_scope);
    }
    
    // Free last result
    if (self->last_result) {
        script_free_value(self->last_result);
    }
    
    // Free async operations
    if (self->async_operations) {
        for (InfraxSize i = 0; i < self->async_count; i++) {
            if (self->async_operations[i]) {
                if (self->async_operations[i]->error_message) {
                    g_memory->dealloc(g_memory, self->async_operations[i]->error_message);
                }
                g_memory->dealloc(g_memory, self->async_operations[i]);
            }
        }
        g_memory->dealloc(g_memory, self->async_operations);
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
    .create_null_value = script_create_null_value,
    .create_number_value = script_create_number_value,
    .create_string_value = script_create_string_value,
    .create_boolean_value = script_create_boolean_value,
    .create_function_value = script_create_function_value,
    .create_array_value = script_create_array_value,
    .create_object_value = script_create_object_value,
    .free_value = script_free_value,
    .create_scope = script_create_scope,
    .free_scope = script_free_scope,
    .define_variable = script_define_variable,
    .set_variable = script_set_variable,
    .get_variable = script_get_variable,
    .print_tokens = script_print_tokens,
    .print_ast = script_print_ast,
    .print_value = script_print_value,
    .eval_expression = script_eval_expression,
    .execute_statement = script_execute_statement,
    .create_promise = script_create_promise,
    .resolve_promise = script_resolve_promise,
    .reject_promise = script_reject_promise,
    .update_async = script_update_async,
    .async_sleep = script_async_sleep,
    .async_read_file = script_async_read_file
}; 
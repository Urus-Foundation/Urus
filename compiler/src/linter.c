#include "linter.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Helper: Cek apakah nama variabel mengikuti snake_case (huruf kecil & underscore)
static void check_snake_case(const char* name, int line) {
    for (int i = 0; name[i] != '\0'; i++) {
        if (isupper(name[i])) {
            printf("[Linter] Warning (Line %d): Nama '%s' harusnya snake_case\n", line, name);
            break;
        }
    }
}

// Fungsi inti Linter: Memindai AST secara rekursif
void run_urus_linter(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case AST_VARIABLE_DECL:
            // 1. Cek gaya penamaan
            check_snake_case(node->data.var_decl.name, node->line);

            // 2. Deteksi Unused Variable (memerlukan flag 'is_used' pada struct ASTNode)
            if (!node->data.var_decl.is_used) {
                printf("[Linter] Warning (Line %d): Variabel '%s' dideklarasikan tapi tidak dipakai\n",
                        node->line, node->data.var_decl.name);
            }
            break;

        case AST_FUNCTION_DECL:
            check_snake_case(node->data.func_decl.name, node->line);
            // Rekursi ke isi fungsi
            run_urus_linter(node->data.func_decl.body);
            break;

        // Tambahkan case lain sesuai kebutuhan (IF, WHILE, BINARY_OP, dll)
        default:
            // Untuk node yang memiliki anak (branch), teruskan pemindaian
            // Contoh: run_urus_linter(node->left); run_urus_linter(node->right);
            break;
    }
}

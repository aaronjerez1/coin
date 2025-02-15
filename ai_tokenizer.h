#ifndef AI_TOKENIZER_H
#define AI_TOKENIZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
    uint32_t vocab_size;
    char **token_table;
    int init_ok;
    int eot_token;
} Tokenizer;

void safe_printf(const char *piece);
void tokenizer_init(Tokenizer *tokenizer, const char *filename);
const char *tokenizer_decode(Tokenizer *tokenizer, uint32_t token_id);
void tokenizer_free(Tokenizer *tokenizer);

#ifdef __cplusplus
}
#endif
#endif // AI_TOKENIZER_H

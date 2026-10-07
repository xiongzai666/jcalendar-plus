/* Exercise decoder error returns before they can become table indices. */
#include <assert.h>
#include <stdio.h>
#include "../lib/BoundedUZlib/src/bounded_tinflate.c"
int main(void) {
    const unsigned char input[8] = {0};
    unsigned char output[8] = {0};
    TINF_DATA state = {0};
    TINF_TREE literal = {0}, distance = {0};
    bounded_uzlib_uncompress_init(&state,NULL,0);
    state.source=input;state.source_limit=input+sizeof(input);
    state.dest_start=state.dest=output;state.dest_limit=output+sizeof(output);
    /* One length symbol, followed by an invalid empty distance tree. */
    literal.table[1]=1;literal.trans[0]=257;
    assert(tinf_inflate_block_data(&state,&literal,&distance)==TINF_DATA_ERROR);
    puts("Decoder rejects symbol errors before table access");
    return 0;
}

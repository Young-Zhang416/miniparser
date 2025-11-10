#include <stdio.h>
#include "parser.h"


int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file.dyd>\n", argv[0]);
        return 1;
    }

    Parser* parser = create_parser(argv[1]);
    if (!parser) {
        return 1;
    }

    print_tokens(parser);

    bool result = program(parser);
    
    printf("Parsing %s\n", result ? "successful" : "failed");
    
    destroy_parser(parser);
    return result ? 0 : 1;
}
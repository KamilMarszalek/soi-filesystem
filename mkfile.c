#include <stdio.h>
#include <stdlib.h>


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uzycie: %s <nazwa_pliku> <rozmiar_w_bajtach>\n", argv[0]);
        return 1;
    }

    char *filename = argv[1];
    long size = atol(argv[2]);
    if (size < 0) {
        fprintf(stderr, "Rozmiar musi byc nieujemny.\n");
        return 1;
    }

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    if (size > 0) {
        if (fseek(fp, size - 1, SEEK_SET) != 0) {
            perror("fseek");
            fclose(fp);
            return 1;
        }
        fputc('\0', fp);
    }

    fclose(fp);

    printf("Utworzono plik '%s' o rozmiarze %ld bajtow.\n", filename, size);
    return 0;
}

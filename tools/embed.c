#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    FILE *in;
    FILE *out;
    int bytes = argc > 1 && !strcmp(argv[1], "-b");
    int first = bytes ? 2 : 1;
    int c;
    int i;
    unsigned n = 0u;

    if (argc < first + 3)
    {
        fputs("usage: embed [-b] <symbol> <out> <in> [<in> ...]\n", stderr);

        return 2;
    }

    out = fopen(argv[first + 1], "wb");

    if (!out)
    {
        fprintf(stderr, "embed: cannot open %s\n", argv[first + 1]);

        return 1;
    }

    fprintf(out, "extern const %schar %s[];\nextern const unsigned %s_size;\n\nconst %schar %s[] = {", bytes ? "unsigned " : "", argv[first], argv[first], bytes ? "unsigned " : "", argv[first]);

    for (i = first + 2; i < argc; i++)
    {
        in = fopen(argv[i], "rb");

        if (!in)
        {
            fprintf(stderr, "embed: cannot open %s\n", argv[i]);

            return 1;
        }

        while ((c = fgetc(in)) != EOF)
        {
            fprintf(out, "%s%d,", n++ % 20u ? " " : "\n    ", c);
        }

        if (!bytes)
        {
            fprintf(out, "%s10,", n++ % 20u ? " " : "\n    ");
        }

        fclose(in);
    }

    fprintf(out, "\n    0\n};\n\nconst unsigned %s_size = %uu;\n", argv[first], n);

    return fclose(out) ? 1 : 0;
}

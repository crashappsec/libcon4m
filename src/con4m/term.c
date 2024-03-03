#include <con4m.h>

void
terminal_dimensions(size_t *cols, size_t *rows)
{
    struct winsize dims = {0, };

    ioctl(0, TIOCGWINSZ, &dims);
    if (cols != NULL) {
	*cols = dims.ws_col;
    }

    if (rows != NULL) {
	*rows = dims.ws_row;
    }
}

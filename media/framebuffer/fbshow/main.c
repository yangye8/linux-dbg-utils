#include "config.h"
#include "fbv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <termios.h>
#include <signal.h>

static int opt_clear = 1;
static int opt_hide_cursor = 1;
static int opt_image_info = 1;
static char *imagename = NULL;

void setup_console(int t)
{
	struct termios our_termios;
	static struct termios old_termios;

	if (isatty(fileno(stdout)))
	{
		if(t)
		{
			printf("setup console\n");
			tcgetattr(0, &old_termios);
			memcpy(&our_termios, &old_termios, sizeof(struct termios));
			our_termios.c_lflag &= !(ECHO | ICANON);
			tcsetattr(0, TCSANOW, &our_termios);
		}
		else
		{
			printf("restore console\n");
			tcsetattr(0, TCSANOW, &old_termios);
		}
	}
	else
	{
		printf("stdout is not connected to a terminal\n");
	}
 }

int show_image(char *filename)
{
	int (*load)(char *, unsigned char *, unsigned char **, int, int);

	unsigned char * image = NULL;
	unsigned char * alpha = NULL;

	int c, ret;
	int x_size, y_size, screen_width, screen_height;
	int x_pan, y_pan, x_offs, y_offs, refresh = 1;
	int retransform = 1, noshow = 0;

	struct image i;

#ifdef FBV_SUPPORT_PNG
	if(fh_png_id(filename))
	if(fh_png_getsize(filename, &x_size, &y_size) == FH_ERROR_OK)
	{
		load = fh_png_load;
		goto identified;
	}
#endif

#ifdef FBV_SUPPORT_JPEG
	if(fh_jpeg_id(filename))
	if(fh_jpeg_getsize(filename, &x_size, &y_size) == FH_ERROR_OK)
	{
		load = fh_jpeg_load;
		goto identified;
	}
#endif

#ifdef FBV_SUPPORT_BMP
	if(fh_bmp_id(filename))
	if(fh_bmp_getsize(filename, &x_size, &y_size) == FH_ERROR_OK)
	{
		load = fh_bmp_load;
		goto identified;
	}
#endif
	fprintf(stderr, "%s: Unable to access file or file format unknown.\n", filename);
	return(1);

identified:

	if(!(image = (unsigned char*)malloc(x_size * y_size * 3)))
	{
		fprintf(stderr, "%s: Out of memory.\n", filename);
		goto error;
	}

	if(load(filename, image, &alpha, x_size, y_size) != FH_ERROR_OK)
	{
		fprintf(stderr, "%s: Image data is corrupt?\n", filename);
		goto error;
	}

	if(getCurrentRes(&screen_width, &screen_height))
		goto error;
	i.do_free = 0;

	while(1)
	{
		if(retransform)
		{
			if(i.do_free)
			{
				free(i.rgb);
			}
			i.width = x_size;
			i.height = y_size;
			i.rgb = image;
			i.alpha = alpha;
			i.do_free = 0;

			x_pan = y_pan = 0;

			refresh = 1; retransform = 0;
			if(opt_clear)
			{
				printf("\033[H\033[J");
				fflush(stdout);
			}
			if(opt_image_info) {
				printf("fbshow - The Framebuffer Viewer\n");
				printf("%s\n", imagename ? imagename : filename);
				printf("%d x %d\n", x_size, y_size);
			}
		}
		if(refresh && !noshow)
		{
			if(i.width < screen_width)
				x_offs = (screen_width - i.width) / 2;
			else
				x_offs = 0;

			if(i.height < screen_height)
				y_offs = (screen_height - i.height) / 2;
			else
				y_offs = 0;

			if(fb_display(i.rgb, i.alpha, i.width, i.height, x_pan, y_pan, x_offs, y_offs))
				goto error;
			refresh = 0;
		}

		if (isatty(fileno(stdin)))
		{
			c = getchar();
			if (c == -1)
				c = 'r';
			switch(c)
			{
				case EOF:
				case 'q':
					ret = 0;
					goto done;
				case ' ': case 10: case 13:
					goto done;
			}
		}
		else
		{
			// Non-interactive, exit immediately
			ret = 1;
			break;
		}
	}// while(1)

done:
	if(opt_clear)
	{
		printf("\033[H\033[J");
		fflush(stdout);
	}

error:
	free(image);
	if(i.do_free)
	{
		free(i.rgb);
	}
	return ret;
}

void help(char *name)
{
	printf("Usage: fbshow image1 \n\n");
}

void sighandler(int s)
{
	if(opt_hide_cursor)
	{
		printf("\033[?25h");
		fflush(stdout);
	}
	setup_console(0);
	_exit(128 + s);
}

int main(int argc, char **argv)
{
	int i;
	char *nameopts = NULL;
	char **namestarts = NULL;

	if(argc < 2)
	{
		help(argv[0]);
		fprintf(stderr, "Error: Required argument missing.\n");
		return 1;
	}

	signal(SIGHUP, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGQUIT, sighandler);
	signal(SIGSEGV, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGABRT, sighandler);

	if(opt_hide_cursor)
	{
		printf("\033[?25l");
		fflush(stdout);
	}

	setup_console(1);

	i = optind;
	while(argv[i])
	{
		imagename = (argc - optind == 1) ?
			 nameopts : namestarts[i - optind];
		int r = show_image(argv[i]);
		if(r == 0)
			break;
		i += r;
		if(i < optind)
			i = optind;
	}

	setup_console(0);

	if(opt_hide_cursor)
	{
		printf("\033[?25h");
		fflush(stdout);
	}
	return 0;
}

#include <stdio.h>
#include <gmsf.h>

typedef struct {
	gdouble red;
	gdouble green;
	gdouble blue;
	gdouble alpha;
} FakeRGBA;


int main(int argc, char **argv) { 
	GMSFStyles *styles;
	GMSFColor color;
	FakeRGBA fake;
	const gchar **cschemes;
	const gchar **sschemes;
	int i;
	
	color = GMSF_COLOR_PACK(255, 0, 255, 255);
	GMSF_COLOR_FILL_GDK_RGBA(fake, color);
	printf("Color Test: %f %f %f %f\n", fake.red, fake.green, fake.blue, fake.alpha);
		
	styles = gmsf_styles_new_from_file(argv[1]);
	
	printf("Version: %d\n", gmsf_styles_get_version(styles));
	
	puts("\nColor Schemes:");
	cschemes = gmsf_styles_get_color_schemes(styles);
	for (i = 0; cschemes[i]; i++) {
		puts(cschemes[i]);
	}
	
	puts("\nSize Schemes:");
	sschemes = gmsf_styles_get_size_schemes(styles);
	for (i = 0; sschemes[i]; i++) {
		puts(sschemes[i]);
	}

	puts("\nSelecting first color scheme and first size scheme!");
	gmsf_styles_select_scheme(styles, cschemes[0], sschemes[0]);
	
	g_object_unref(styles);
	
	return 0;
}

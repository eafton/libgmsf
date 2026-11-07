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
	GMSFStylesDocumentation doc;
	FakeRGBA fake;
	const gchar **cschemes;
	const gchar **sschemes;
	int i;
	
	color = GMSF_COLOR_PACK(255, 0, 255, 255);
	GMSF_COLOR_FILL_GDK_RGBA(fake, color);
	printf("Color Test: %f %f %f %f\n", fake.red, fake.green, fake.blue, fake.alpha);
		
	styles = gmsf_styles_new_from_file(argv[1]);
	
	printf("Version: %d\n", gmsf_styles_get_version(styles));
	gmsf_styles_get_documentation(styles, &doc);
	puts("\nDocumentation:");
	printf("-DISPLAY NAME: %s\n", doc.display_name);
	printf("-LAST UPDATED: %s\n", doc.last_updated);
	printf("-TOOLTIP: %s\n", doc.tooltip);
	printf("-AUTHOR: %s\n", doc.author);
	printf("-COMPANY: %s\n", doc.company);
	printf("-COPYRIGHT: %s\n", doc.copyright);
	printf("-URL: %s\n", doc.url);
	printf("-VERSION: %s\n", doc.version);
	printf("-DESCRIPTION: %s\n", doc.description);
	printf("-THEME NAME: %s\n", doc.theme_name);
	printf("-WMP SKIN: %s\n", doc.wmp_skin);
	gmsf_styles_documentation_free_fields(&doc);


	puts("\nColor Schemes:");
	cschemes = gmsf_styles_get_color_schemes(styles);
	for (i = 0; cschemes[i]; i++) {
		gchar *display;
		gchar *tooltip;
		
		gmsf_styles_get_color_scheme_info(styles, cschemes[i], &display, &tooltip);
		printf("Name: %s, DisplayName: %s, ToolTip: %s\n", cschemes[i], display, tooltip);
		g_free(display);
		g_free(tooltip);
	}
	
	puts("\nSize Schemes:");
	sschemes = gmsf_styles_get_size_schemes(styles);
	for (i = 0; sschemes[i]; i++) {
		gchar *display;
		gchar *tooltip;
		
		gmsf_styles_get_size_scheme_info(styles, sschemes[i], &display, &tooltip);
		printf("Name: %s, DisplayName: %s, ToolTip: %s\n", sschemes[i], display, tooltip);
		g_free(display);
		g_free(tooltip);
	}

	puts("\nSelecting first color scheme and first size scheme!");
	gmsf_styles_select_scheme(styles, cschemes[0], sschemes[0]);
	
	g_object_unref(styles);
	
	return 0;
}

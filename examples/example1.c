#include <gmsf.h>

int main(int argc, char **argv) { 
	GMSFStyles *styles;
	
	styles = gmsf_styles_new_from_file(argv[1]);
	g_object_unref(styles);
	
	return 0;
}

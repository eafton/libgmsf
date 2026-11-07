#include <stdio.h>
#include <gmsf.h>

int main(int argc, char **argv) { 
	GMSFRichText *rtf;
	
	rtf = gmsf_rich_text_new_from_file(argv[1]);
	g_object_unref(rtf);
	
	return 0;
}

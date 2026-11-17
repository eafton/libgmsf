#include <stdio.h>
#include <gmsf.h>

int main(int argc, char **argv) { 
	GMSFAgent *agent;

	agent = gmsf_agent_new_from_file("Merlin.acs");
	gmsf_agent_get_image(agent, 234);
	gmsf_agent_get_image_region(agent, 234);
	
	return 0;
}

#include <stdio.h>
#include <gmsf.h>

int main(int argc, char **argv) { 
	GMSFAgent *agent;
	
	agent = gmsf_agent_new_from_file("Merlin.acs");
	
	return 0;
}

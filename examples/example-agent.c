#include <stdio.h>
#include <gmsf.h>
#include <ao/ao.h>

int main(int argc, char **argv) { 
	GMSFAgent *agent;
   	guint i;
    int driver;
    
    ao_initialize();
    driver = ao_default_driver_id();
	agent = gmsf_agent_new_from_file("Merlin.acs");
	for (i = 0; i < gmsf_agent_get_audio_count(agent); i++) {
		short buffer[512];
		ao_device *device;
		SNDFILE *snd;
		SF_INFO info;
		ao_sample_format format;
		sf_count_t read;
		
		snd = gmsf_agent_open_audio(agent, i, &info);
		
		format.bits = 16;
		format.channels = info.channels;
		format.rate = info.samplerate;
		format.byte_format = AO_FMT_NATIVE;
		format.matrix = 0;
		device = ao_open_live(driver, &format, NULL);
			
		
		while ((read = sf_read_short(snd, buffer, 512)) > 0) {
			ao_play(device, (char *)buffer, 512 * sizeof(short));
		}
    
		sf_close(snd);
		ao_close(device);
	}
	
    ao_shutdown();
	return 0;
}

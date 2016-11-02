#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
//#include "asoundlib.h"

#define BUFF_SIZE 4096

int main (int argc, char *argv[])
{
	int err;
	short buf[BUFF_SIZE];
	int rate = 44100; /* Sample rate */
	int exact_rate; /* Sample rate returned by */
	snd_pcm_t *capture_handle;

	/* This structure contains information about the hardware and can be used to specify the configuration to be used for */
	/* the PCM stream. */
	snd_pcm_hw_params_t *hw_params;

	/* Name of the PCM device, like hw:0,0 */
	/* The first number is the number of the soundcard, the second number is the number of the device. */
	static char *device = "hw:0,0"; /* capture device */

	/* Open PCM. The last parameter of this function is the mode. */
	if ((err = snd_pcm_open (&capture_handle, device, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		fprintf (stderr, "cannot open audio device (%s)\n", snd_strerror (err));
		exit (1);
	}

	memset(buf,0,BUFF_SIZE);

	/* Allocate the snd_pcm_hw_params_t structure on the stack. */
	if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
		fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n", snd_strerror (err));
		exit (1);
	}

	/* Init hwparams with full configuration space */
	if ((err = snd_pcm_hw_params_any (capture_handle, hw_params)) < 0) {
		fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n", snd_strerror (err));
		exit (1);
	}

	/* Set access type. */
	if ((err = snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf (stderr, "cannot set access type (%s)\n", snd_strerror (err));
		exit (1);
	}

	/* Set sample format */
	if ((err = snd_pcm_hw_params_set_format (capture_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
		fprintf (stderr, "cannot set sample format (%s)\n", snd_strerror (err)); 
		exit (1);
	}

	/* Set sample rate. If the exact rate is not supported by the hardware, use nearest possible rate. */
	exact_rate = rate;
	if ((err = snd_pcm_hw_params_set_rate_near (capture_handle, hw_params, &exact_rate, 0)) < 0) {
		fprintf (stderr, "cannot set sample rate (%s)\n", snd_strerror (err));
		exit (1);
	}

	if (rate != exact_rate) {
		fprintf(stderr, "The rate %d Hz is not supported by your hardware.\n ==> Using %d Hz instead.\n", rate, exact_rate);
	}

	/* Set number of channels */
	if ((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params, 2)) < 0) {
		fprintf (stderr, "cannot set channel count (%s)\n", snd_strerror (err));
		exit (1);
	}

	/* Apply HW parameter settings to PCM device and prepare device. */
	if ((err = snd_pcm_hw_params (capture_handle, hw_params)) < 0) {
		fprintf (stderr, "cannot set parameters (%s)\n", snd_strerror (err));
		exit (1);
	}

	snd_pcm_hw_params_free (hw_params);

	if ((err = snd_pcm_prepare (capture_handle)) < 0) {
		fprintf (stderr, "cannot prepare audio interface for use (%s)\n", snd_strerror (err));
		exit (1);
	}

	/* Read data into the buffer. */
	if ((err = snd_pcm_readi (capture_handle, buf, 128)) != 128) {
		fprintf (stderr, "read from audio interface failed (%s)\n", snd_strerror (err));
		exit (1);
	} else {
		fprintf (stdout, "snd_pcm_readi successful\n");
	}

	snd_pcm_close (capture_handle);

	exit (0);
}

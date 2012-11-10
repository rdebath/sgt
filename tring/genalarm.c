#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static signed short *alarm_sound;
static int alarm_len, alarm_freq;

void generate_alarm_sound(void)
{
    double basefreqs[2];
    int i;
    double len, *tmp;
    double twopi = 2 * 3.141592653589793238462643383279502884197169399;

    alarm_freq = 44100;

    /*
     * Two-tone bell in a minor sixth (== inverted _major_ third,
     * so it sounds a bit cheery).
     */
    basefreqs[0] = 640.0;
    basefreqs[1] = basefreqs[0] * (24.0 / 40.0);

    /*
     * Bell tone lasts nearly a second.
     */
    len = 0.9;
    alarm_len = len * alarm_freq;

    /*
     * Allocate memory for the temporary sound during generation,
     * and for the eventual final scaled output.
     */
    tmp = malloc(alarm_len * sizeof(double));
    alarm_sound = malloc(alarm_len * sizeof(signed short));

    /*
     * Generate the theoretical sample values.
     */
    for (i = 0; i < alarm_len; i++) {
	double sample;
	double vol;
	double t = (double)i / alarm_freq;
	int note, harm;

	/*
	 * Cap at full volume.
	 */
	vol = 1.0;

	/*
	 * Attack: sound goes from silence to full volume in 1/100
	 * of a second.
	 */
	vol = fminf(vol, 100.0 * t);

	/*
	 * Release: when we cut out at the end of the bell tone,
	 * same drop-off rate.
	 */
	vol = fminf(vol, 100.0 * (len - t));

	/*
	 * Decay: in between, the tone exponentially decays with a
	 * time constant of 5 (meaning volume reduces by a factor
	 * of e in 0.2 second).
	 */
	vol = fminf(vol, expf(-5 * t));

	/*
	 * Now we know our volume, calculate the waveform. To get
	 * a nice sound which is less piercingly pure than a sine
	 * wave but less gratingly harsh than a square wave, I
	 * arbitrarily decided to let the harmonic frequencies
	 * drop off as n^{3/2}.
	 */
	sample = 0;
	
	for (note = 0; note < 2; note++)
	    for (harm = 1; harm < 15; harm += 2) {
		sample += vol * sinf(harm * basefreqs[note] * twopi * t) *
		    powf(harm, -1.5);
	    }

	/*
	 * And we're done. Store our sample.
	 */
	tmp[i] = sample;
    }

    /*
     * Now find the maximum absolute sample value, and use that to
     * scale the output into real sample values.
     */
    {
	double scale;

	scale = 0.0;
	for (i = 0; i < alarm_len; i++)
	    scale = fmaxf(scale, fabsf(tmp[i]));
	scale = 1.0 / scale;

	for (i = 0; i < alarm_len; i++)
	    alarm_sound[i] = 0x7FFF * (scale * tmp[i]);

	free(tmp);
    }
}

int main(void)
{
    int i;
    FILE *fp;

    generate_alarm_sound();

    for (i = 0; i < alarm_len; i++) {
	fputc(alarm_sound[i] & 0xFF, stdout);
	fputc((alarm_sound[i] >> 8) & 0xFF, stdout);
    }

    /*
     * Diagnostic output as a .wav.
     */
    if ((fp = fopen("build/almsnd.wav", "wb")) != NULL) {
	char header[256], *p = header;
	int j;

#define PUT(n, value) do { \
    int i; \
    for (i=0; i<(n); i++) { \
	*p++ = (((unsigned)value) >> (i*8)) & 0xFF; \
    } \
} while (0)
#define PUTS(s) do { \
    int i; \
    for (i=0; i<sizeof(s)-1; i++) { \
	*p++ = s[i]; \
    } \
} while (0)

	PUTS("RIFF");
	PUT(4, alarm_len * 2 + 36);
	PUTS("WAVE");
	PUTS("fmt ");
	PUT(4, 0x10);
	PUT(2, 1);		       /* PCM */
	PUT(2, 1);		       /* channels */
	PUT(4, alarm_freq);	       /* sample rate */
	PUT(4, alarm_freq * 2);	       /* data rate */
	PUT(2, 2);		       /* bytes per frame */
	PUT(2, 16);		       /* bits per sample */
	PUTS("data");
	PUT(4, alarm_len * 2);
	fwrite(header, 1, p - header, fp);

	for (j = 0; j < alarm_len; j++) {
	    p = header;
	    PUT(2, alarm_sound[j]);
	    fwrite(header, 1, p - header, fp);
	}

	fclose(fp);
    }

    return 0;
}

